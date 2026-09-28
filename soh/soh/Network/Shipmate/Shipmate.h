#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>

namespace Ship { class ResourceManager; }

namespace Shipmate {
bool Enable(bool lan);
void Disable();
bool IsEnabled();
const std::string& Error();
// The following functions are called only on the game thread.
nlohmann::json HandleRequest(const nlohmann::json& request);
nlohmann::json HudColors();
unsigned AssetRevision();
void InvalidateAssets();
// Capture only the manager lifetime and asset selection on the game thread.
struct AssetContext {
    std::shared_ptr<Ship::ResourceManager> manager;
    bool alternate;
    unsigned revision;
};
AssetContext CaptureAssets();
// Resource waits, pixel conversion/compositing and encoding belong to HTTP workers.
struct Image { int width = 0, height = 0; std::vector<unsigned char> rgba; };
Image ReadAsset(const std::string& name, const AssetContext& context);
std::string EncodePng(const Image& image);
}
