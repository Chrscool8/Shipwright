#pragma once
#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace Ship {
class ResourceManager;
}

namespace Shipmate {
inline constexpr int DefaultPort = 43385;
bool Enable(bool lan, int port);
void Disable();
// Start, restart or stop from the Shipmate CVars; a failed start turns Enabled off.
void ApplySettings();
bool IsEnabled();
const std::string& Error();
const std::string& LanUrl();
// Game-thread request handling and cosmetic reads.
nlohmann::json Snapshot();
nlohmann::json HandleRequest(const nlohmann::json& request);
nlohmann::json HudColors();
std::string ItemIconPath(int item);

// Atomic revision reads are also safe on HTTP workers; invalidation runs on the game thread.
uint64_t AssetRevision();
void InvalidateAssets();

// Capture the manager lifetime, asset selection, and requested item path on the game thread.
struct AssetContext {
    std::shared_ptr<Ship::ResourceManager> manager;
    bool alternate;
    uint64_t revision;
    std::string itemIconPath;
};
AssetContext CaptureAssets(const std::string& name);

// Resource waits, pixel conversion/compositing and encoding belong to HTTP workers.
struct Image {
    int width = 0, height = 0;
    std::vector<unsigned char> rgba;
};
Image ReadAsset(const std::string& name, const AssetContext& context);
std::string EncodePng(const Image& image);
} // namespace Shipmate
