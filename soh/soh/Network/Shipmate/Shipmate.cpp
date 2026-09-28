// Keep HTTP and its platform socket headers out of the game-facing header.
#include <httplib.h>
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
constexpr int Port = 43385;
std::unique_ptr<httplib::Server> server;
std::thread listener;
std::string error;
struct Job {
    std::function<void()> run;
    enum class State { Pending, Running, Cancelled };
    std::atomic<State> state{ State::Pending };
};
std::mutex jobsMutex;
std::deque<std::shared_ptr<Job>> jobs;
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
    {
        std::lock_guard lock(jobsMutex);
        if (!accepting || jobs.size() >= 64)
            throw std::runtime_error("Shipmate is busy or disabled");
        jobs.push_back(job);
    }
    if (result.wait_for(std::chrono::seconds(2)) != std::future_status::ready) {
        auto pending = Job::State::Pending;
        if (job->state.compare_exchange_strong(pending, Job::State::Cancelled) || pending == Job::State::Cancelled)
            throw std::runtime_error("Ship is not responding; try again when the game is running");
        // A short game action already claimed this request. Return its actual result;
        // never report a timeout and then apply an equipment change later.
    }
    return result.get();
}
void RegisterHooks() {
    COND_HOOK(OnGameFrameUpdate, IsEnabled(), [] {
        std::shared_ptr<Job> job;
        {
            std::unique_lock lock(jobsMutex, std::try_to_lock);
            if (!lock.owns_lock() || jobs.empty())
                return;
            job = jobs.front();
            jobs.pop_front();
        }
        // One small snapshot/action/context capture per frame, outside the queue lock.
        // If a worker owns the queue, gameplay skips it and Shipmate waits.
        auto pending = Job::State::Pending;
        if (job->state.compare_exchange_strong(pending, Job::State::Running))
            job->run();
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
void Disable() {
    {
        std::lock_guard lock(jobsMutex);
        accepting = false;
        for (auto& job : jobs)
            job->state = Job::State::Cancelled;
        jobs.clear();
    }
    if (server) {
        server->stop();
        if (listener.joinable())
            listener.join();
        server.reset();
    }
    RegisterHooks();
    std::lock_guard lock(imagesMutex);
    images.clear();
}
bool Enable(bool lan) {
    Disable();
    error.clear();
    InvalidateAssets();
    auto next = std::make_unique<httplib::Server>();
    next->new_task_queue = [] { return new httplib::ThreadPool(4); };
    // A few idle browser sockets must not occupy the whole small worker pool.
    next->set_keep_alive_max_count(1);
    next->set_payload_max_length(4096);
    next->set_read_timeout(2, 0);
    next->set_write_timeout(2, 0);
    next->set_default_headers({ { "Cache-Control", "no-store" },
                                { "Content-Security-Policy", "frame-ancestors 'none'" },
                                { "X-Content-Type-Options", "nosniff" },
                                { "X-Frame-Options", "DENY" } });
    next->set_pre_routing_handler([](const auto& request, auto& response) {
        // Browser origin checks, without pairing or credentials.
        const auto host = request.get_header_value("Host");
        const auto origin = request.get_header_value("Origin");
        const auto suffix = std::string(":") + std::to_string(Port);
        auto address = host.ends_with(suffix) ? host.substr(0, host.size() - suffix.size()) : std::string();
        in_addr numericAddress{};
        bool knownHost = address == "localhost" || inet_pton(AF_INET, address.c_str(), &numericAddress) == 1;
        if (!knownHost || (!origin.empty() && origin != "http://" + host) ||
            request.get_header_value("Sec-Fetch-Site") == "cross-site") {
            response.status = 403;
            Json(response, { { "status", "failure" }, { "message", "Unrecognized browser origin" } });
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });
    next->set_exception_handler([](const auto&, auto& response, std::exception_ptr exception) {
        std::string message = "Shipmate request failed";
        try {
            if (exception)
                std::rethrow_exception(exception);
        } catch (const std::exception& e) { message = e.what(); } catch (...) {
        }
        response.status = 503;
        Json(response, { { "status", "failure" }, { "message", message } });
    });
    next->Get("/", [](const auto&, auto& r) { r.set_content(Web::Html, "text/html; charset=utf-8"); });
    next->Get("/app.js", [](const auto&, auto& r) { r.set_content(Web::Js, "text/javascript; charset=utf-8"); });
    next->Get("/style.css", [](const auto&, auto& r) { r.set_content(Web::Css, "text/css; charset=utf-8"); });
    next->Get("/state", [](const auto&, auto& r) {
        Json(r, OnGameThread([] { return HandleRequest({ { "action", "snapshot" } }); }));
    });
    next->Post("/action", [](const auto& request, auto& r) {
        auto payload = nlohmann::json::parse(request.body, nullptr, false);
        if (!request.get_header_value("Content-Type").starts_with("application/json") || !payload.is_object() ||
            !payload.contains("action") || !payload["action"].is_string()) {
            r.status = 400;
            Json(r, { { "status", "failure" }, { "message", "Expected an inventory/equipment action" } });
            return;
        }
        auto action = payload["action"].template get<std::string>();
        if (action != "assign" && action != "unassign" && action != "equip") {
            r.status = 400;
            Json(r, { { "status", "failure" }, { "message", "Unknown action" } });
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
            if ((!request.has_param("v") || MatchesRevision(request, revision)) && imageGeneration == revision) {
                if (auto it = images.find(name); it != images.end()) {
                    Png(r, request, it->second, revision);
                    return;
                }
            }
        }
        auto context = OnGameThread([] { return CaptureAssets(); });
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
            if (imageGeneration == revision)
                images[name] = png;
        }
        Png(r, request, png, revision);
    });
    if (!next->bind_to_port(lan ? "0.0.0.0" : "127.0.0.1", Port)) {
        error = "Cannot open port 43385. Close the old helper or another Ship instance.";
        return false;
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
    if (CVarGetInteger(CVAR_REMOTE("Shipmate.Enabled"), 0) && !Enable(CVarGetInteger(CVAR_REMOTE("Shipmate.LAN"), 0))) {
        CVarSetInteger(CVAR_REMOTE("Shipmate.Enabled"), 0);
    }
});
} // namespace Shipmate
