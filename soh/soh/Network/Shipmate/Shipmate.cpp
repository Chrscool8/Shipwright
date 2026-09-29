// Keep HTTP and its platform socket headers out of the game-facing header.
#include <httplib.h>
#ifdef _WIN32
#include <iphlpapi.h>
#else
#include <ifaddrs.h>
#include <net/if.h>
#endif
#include "Shipmate.h"
#include "ShipmateWeb.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>

namespace Shipmate {
namespace {
std::unique_ptr<httplib::Server> server;
std::thread listener;
std::string error;
std::string lanUrl;

// Best effort only: use the first active private IPv4 address.
std::string FindLanAddress() {
    auto privateAddress = [](const sockaddr* address) -> std::string {
        if (!address || address->sa_family != AF_INET) {
            return {};
        }
        const auto& ipv4 = reinterpret_cast<const sockaddr_in*>(address)->sin_addr;
        const uint32_t ip = ntohl(ipv4.s_addr);
        if ((ip >> 24) != 10 && (ip >> 20) != 0xAC1 && (ip >> 16) != 0xC0A8) {
            return {};
        }
        char text[INET_ADDRSTRLEN]{};
        return inet_ntop(AF_INET, &ipv4, text, sizeof(text)) ? text : "";
    };
#ifdef _WIN32
    ULONG size = 0;
    constexpr ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, nullptr, &size) != ERROR_BUFFER_OVERFLOW) {
        return {};
    }
    std::vector<unsigned char> buffer(size);
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &size) != NO_ERROR) {
        return {};
    }
    for (auto* adapter = adapters; adapter; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp) {
            continue;
        }
        for (auto* entry = adapter->FirstUnicastAddress; entry; entry = entry->Next) {
            auto address = privateAddress(entry->Address.lpSockaddr);
            if (!address.empty()) {
                return address;
            }
        }
    }
#else
    ifaddrs* interfaces = nullptr;
    if (getifaddrs(&interfaces) != 0) {
        return {};
    }
    std::string address;
    for (auto* entry = interfaces; entry; entry = entry->ifa_next) {
        if ((entry->ifa_flags & IFF_UP) && !(entry->ifa_flags & IFF_LOOPBACK)) {
            address = privateAddress(entry->ifa_addr);
            if (!address.empty()) {
                break;
            }
        }
    }
    freeifaddrs(interfaces);
    return address;
#endif
    return {};
}
struct Job {
    std::function<void()> run;
    std::function<void()> cancel;
    enum class State { Pending, Running, Cancelled };
    std::atomic<State> state{ State::Pending };
};
std::mutex jobsMutex;
std::deque<std::shared_ptr<Job>> jobs;
std::atomic<bool> jobsPending = false;
bool accepting = false;
std::mutex imagesMutex;
std::unordered_map<std::string, std::string> images;
uint64_t imageGeneration = 0;

// Timeout cancels a job before it can make a delayed equipment change.
template <typename F> auto OnGameThread(F fn) -> decltype(fn()) {
    using T = decltype(fn());
    auto promise = std::make_shared<std::promise<T>>();
    auto result = promise->get_future();
    auto job = std::make_shared<Job>();
    job->run = [promise, fn] {
        try {
            promise->set_value(fn());
        } catch (...) { promise->set_exception(std::current_exception()); }
    };
    job->cancel = [promise] {
        promise->set_exception(std::make_exception_ptr(std::runtime_error("Shipmate was disabled")));
    };
    {
        std::lock_guard lock(jobsMutex);
        if (!accepting || jobs.size() >= 64) {
            throw std::runtime_error("Shipmate is busy or disabled");
        }
        jobs.push_back(job);
        jobsPending.store(true, std::memory_order_release);
    }
    if (result.wait_for(std::chrono::seconds(2)) != std::future_status::ready) {
        auto pending = Job::State::Pending;
        if (job->state.compare_exchange_strong(pending, Job::State::Cancelled) || pending == Job::State::Cancelled) {
            throw std::runtime_error("Ship is not responding; try again when the game is running");
        }
        // Wait for running jobs so a timeout cannot precede an equipment change.
    }
    return result.get();
}

void RegisterHooks() {
    COND_HOOK(OnGameFrameUpdate, IsEnabled(), [] {
        if (!jobsPending.load(std::memory_order_acquire)) {
            return;
        }
        std::shared_ptr<Job> job;
        {
            std::unique_lock lock(jobsMutex, std::try_to_lock);
            if (!lock.owns_lock()) {
                return;
            }
            if (jobs.empty()) {
                jobsPending.store(false, std::memory_order_release);
                return;
            }
            job = jobs.front();
            jobs.pop_front();
            jobsPending.store(!jobs.empty(), std::memory_order_release);
        }
        // Run one job per frame without holding the queue lock.
        auto pending = Job::State::Pending;
        if (job->state.compare_exchange_strong(pending, Job::State::Running)) {
            job->run();
        }
    });
    COND_HOOK(OnAssetAltChange, IsEnabled(), [] { InvalidateAssets(); });
}

void Json(httplib::Response& response, const nlohmann::json& value) {
    response.set_content(value.dump(), "application/json");
}

bool MatchesRevision(const httplib::Request& request, uint64_t revision) {
    return request.has_param("v") && request.get_param_value("v") == std::to_string(revision);
}

void Png(httplib::Response& response, const httplib::Request& request, const std::string& png, uint64_t revision) {
    response.set_content(png, "image/png");
    if (MatchesRevision(request, revision)) {
        response.headers.erase("Cache-Control");
        response.set_header("Cache-Control", "public, max-age=31536000, immutable");
    }
}
} // namespace
bool IsEnabled() {
    return server != nullptr;
}

const std::string& Error() {
    return error;
}

const std::string& LanUrl() {
    return lanUrl;
}

void Disable() {
    lanUrl.clear();
    std::deque<std::shared_ptr<Job>> cancelled;
    {
        std::lock_guard lock(jobsMutex);
        accepting = false;
        cancelled.swap(jobs);
        jobsPending.store(false, std::memory_order_release);
    }
    for (auto& job : cancelled) {
        auto pending = Job::State::Pending;
        if (job->state.compare_exchange_strong(pending, Job::State::Cancelled)) {
            job->cancel();
        }
    }
    if (server) {
        server->stop();
        if (listener.joinable()) {
            listener.join();
        }
        server.reset();
    }
    RegisterHooks();
    std::lock_guard lock(imagesMutex);
    images.clear();
}

bool Enable(bool lan, int port) {
    Disable();
    error.clear();
    if (port < 1 || port > 65535) {
        error = "Shipmate port must be between 1 and 65535.";
        return false;
    }
    InvalidateAssets();
    auto next = std::make_unique<httplib::Server>();
    next->new_task_queue = [] { return new httplib::ThreadPool(4, 16); };
    // Prevent idle connections from occupying all workers.
    next->set_keep_alive_max_count(1);
    next->set_payload_max_length(4096);
    next->set_read_timeout(2, 0);
    next->set_write_timeout(2, 0);
    next->set_default_headers({ { "Cache-Control", "no-store" },
                                { "Content-Security-Policy", "frame-ancestors 'none'" },
                                { "X-Content-Type-Options", "nosniff" },
                                { "X-Frame-Options", "DENY" } });
    next->set_pre_routing_handler([port](const auto& request, auto& response) {
        // Reject unrecognized hosts and cross-origin requests.
        const auto host = request.get_header_value("Host");
        const auto origin = request.get_header_value("Origin");
        const auto suffix = std::string(":") + std::to_string(port);
        // HTTP's default port may be omitted from Host and Origin independently.
        auto address =
            host.ends_with(suffix) ? host.substr(0, host.size() - suffix.size()) : (port == 80 ? host : std::string());
        const auto expectedOrigin = "http://" + address + (port == 80 ? std::string() : suffix);
        const bool sameOrigin =
            origin.empty() || origin == expectedOrigin || (port == 80 && origin == expectedOrigin + suffix);
        in_addr numericAddress{};
        bool knownHost = address == "localhost" || inet_pton(AF_INET, address.c_str(), &numericAddress) == 1;
        if (!knownHost || !sameOrigin || request.get_header_value("Sec-Fetch-Site") == "cross-site") {
            response.status = 403;
            Json(response, { { "status", "failure" }, { "message", "Unrecognized browser origin" } });
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });
    next->set_exception_handler([](const auto&, auto& response, std::exception_ptr exception) {
        std::string message = "Shipmate request failed";
        try {
            if (exception) {
                std::rethrow_exception(exception);
            }
        } catch (const std::exception& e) { message = e.what(); } catch (...) {
        }
        response.status = 503;
        Json(response, { { "status", "failure" }, { "message", message } });
    });
    next->Get("/", [](const auto&, auto& r) { r.set_content(Web::Html, "text/html; charset=utf-8"); });
    next->Get("/app.js", [](const auto&, auto& r) { r.set_content(Web::Js, "text/javascript; charset=utf-8"); });
    next->Get("/style.css", [](const auto&, auto& r) { r.set_content(Web::Css, "text/css; charset=utf-8"); });
    next->Get("/favicon.svg", [](const auto&, auto& r) { r.set_content(Web::Favicon, "image/svg+xml"); });
    next->Get("/state", [](const auto&, auto& r) { Json(r, OnGameThread(Snapshot)); });
    next->Post("/action", [](const auto& request, auto& r) {
        auto payload = nlohmann::json::parse(request.body, nullptr, false);
        if (!request.get_header_value("Content-Type").starts_with("application/json") || !payload.is_object() ||
            !payload.contains("action") || !payload["action"].is_string()) {
            r.status = 400;
            Json(r, { { "status", "failure" }, { "message", "Expected an inventory/equipment action" } });
            return;
        }
        Json(r, OnGameThread([payload] {
                 try {
                     return HandleRequest(payload);
                 } catch (const std::exception&) {
                     return nlohmann::json{ { "status", "failure" }, { "message", "Invalid equipment request" } };
                 }
             }));
    });
    next->Get(R"(/assets/([A-Za-z0-9_-]+)\.png)", [](const auto& request, auto& r) {
        const std::string name = request.matches[1];
        auto revision = AssetRevision();
        {
            std::lock_guard lock(imagesMutex);
            if (imageGeneration == revision) {
                if (auto it = images.find(name); it != images.end()) {
                    Png(r, request, it->second, revision);
                    return;
                }
            }
        }
        auto context = OnGameThread([name] { return CaptureAssets(name); });
        revision = context.revision;
        {
            std::lock_guard lock(imagesMutex);
            if (imageGeneration != revision) {
                images.clear();
                imageGeneration = revision;
            }
            if (auto it = images.find(name); it != images.end()) {
                Png(r, request, it->second, revision);
                return;
            }
        }
        // Waiting for resources and all image processing stay on this HTTP worker.
        auto image = ReadAsset(name, context);
        if (image.rgba.empty()) {
            r.status = 404;
            return;
        }
        auto png = EncodePng(image);
        {
            std::lock_guard lock(imagesMutex);
            if (imageGeneration == revision) {
                images[name] = png;
            }
        }
        Png(r, request, png, revision);
    });
    if (!next->bind_to_port(lan ? "0.0.0.0" : "127.0.0.1", port)) {
        error = "Cannot open port " + std::to_string(port) + ". Try a different port.";
        return false;
    }
    if (lan) {
        auto address = FindLanAddress();
        if (!address.empty()) {
            lanUrl = "http://" + address + ":" + std::to_string(port) + "/";
        }
    }
    server = std::move(next);
    {
        std::lock_guard lock(jobsMutex);
        accepting = true;
    }
    RegisterHooks();
    listener = std::thread([] { server->listen_after_bind(); });
    server->wait_until_ready();
    return true;
}

static RegisterShipInitFunc init([] {
    if (CVarGetInteger(CVAR_REMOTE("Shipmate.Enabled"), 0) &&
        !Enable(CVarGetInteger(CVAR_REMOTE("Shipmate.LAN"), 0),
                CVarGetInteger(CVAR_REMOTE("Shipmate.Port"), DefaultPort))) {
        CVarSetInteger(CVAR_REMOTE("Shipmate.Enabled"), 0);
    }
});
} // namespace Shipmate
