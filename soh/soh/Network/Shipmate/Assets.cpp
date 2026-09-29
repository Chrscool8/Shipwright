#include "Shipmate.h"
#include "AssetPaths.h"
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <fast/resource/type/Texture.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/cvar_prefixes.h"
// Must precede the extern "C" kaleido include.
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include <stb_image_write.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string_view>

extern "C" {
#include "src/overlays/misc/ovl_kaleido_scope/z_kaleido_scope.h"
}

namespace Shipmate {
// The alternate-assets flag changes only on the game thread, just before OnAssetAltChange invalidates.
// Recording it with the revision lets HTTP workers capture a consistent pair without a game-thread hop.
static std::mutex revisionMutex;
static uint64_t revision = static_cast<uint64_t>(
    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
static bool alternate = false;

uint64_t AssetRevision() {
    std::lock_guard lock(revisionMutex);
    return revision;
}

void InvalidateAssets() {
    const bool enabled = Ship::Context::GetRawInstance()->GetResourceManager()->IsAltAssetsEnabled();
    std::lock_guard lock(revisionMutex);
    alternate = enabled;
    ++revision;
}

static std::array<int, 3> Color(const char* key, std::array<int, 3> fallback) {
    std::string path = CVAR_COSMETIC("") + std::string(key);
    if (!CVarGetInteger((path + ".Changed").c_str(), 0)) {
        return fallback;
    }
    auto color =
        CVarGetColor24((path + ".Value").c_str(), { (uint8_t)fallback[0], (uint8_t)fallback[1], (uint8_t)fallback[2] });
    return { color.r, color.g, color.b };
}

nlohmann::json HudColors() {
    auto b = CVarGetInteger(CVAR_COSMETIC("DefaultColorScheme"), 0) == 1 ? std::array<int, 3>{ 255, 30, 30 }
                                                                         : std::array<int, 3>{ 0, 150, 0 };
    auto c = Color("HUD.CButtons", { 255, 160, 0 });
    return { { "b", Color("HUD.BButton", b) },
             { "left", Color("HUD.CLeftButton", c) },
             { "down", Color("HUD.CDownButton", c) },
             { "right", Color("HUD.CRightButton", c) },
             { "dpad", Color("HUD.Dpad", { 255, 255, 255 }) } };
}

// Exact "<prefix><n>": no sign or leading zeros.
static bool ParseIndexed(const std::string& name, std::string_view prefix, int& value) {
    if (!name.starts_with(prefix)) {
        return false;
    }
    auto [end, error] = std::from_chars(name.data() + prefix.size(), name.data() + name.size(), value);
    return error == std::errc() && end == name.data() + name.size() && value >= 0 &&
           name.size() == prefix.size() + std::to_string(value).size();
}

AssetContext CaptureAssets(const std::string& name) {
    AssetContext context{ Ship::Context::GetRawInstance()->GetResourceManager(), false, 0, {} };
    {
        std::lock_guard lock(revisionMutex);
        context.alternate = alternate;
        context.revision = revision;
    }
    if (int item; ParseIndexed(name, "item-", item)) {
        context.itemIconPath = ItemIconPath(item);
    }
    return context;
}

static std::string StripOtrSignature(const char* path) {
    // Resource paths omit the OTR signature, whose loader shortcut otherwise resets loadExact.
    std::string_view view(path ? path : "");
    return std::string(view.starts_with("__OTR__") ? view.substr(7) : view);
}

static Image LoadTexture(const std::string& path, const AssetContext& context) {
    // Exact paths avoid consulting the live alternate-assets flag on a worker.
    std::shared_ptr<Ship::IResource> loaded;
    if (context.alternate) {
        loaded = context.manager->LoadResourceAsync("alt/" + path, true, BS::pr::low).get();
    }
    if (!loaded) {
        loaded = context.manager->LoadResourceAsync(path, true, BS::pr::low).get();
    }
    // Shared ownership pins the texture buffer through conversion, even across invalidation.
    auto resource = std::dynamic_pointer_cast<Fast::Texture>(loaded);
    if (!resource || !resource->ImageData) {
        throw std::runtime_error("Texture unavailable: " + path);
    }
    Image result;
    result.width = resource->Width;
    result.height = resource->Height;
    size_t count = size_t(result.width) * result.height;
    if (!count || count > 4096 * 4096) {
        throw std::runtime_error("Unsupported texture dimensions: " + path);
    }
    auto type = resource->Type;
    using T = Fast::TextureType;
    // HD raw textures contain RGBA pixels but retain their original N64 type.
    // Indexed textures still require a palette and are rejected below.
    if ((resource->Flags & TEX_FLAG_LOAD_AS_IMG) ||
        ((resource->Flags & TEX_FLAG_LOAD_AS_RAW) && type != T::Palette4bpp && type != T::Palette8bpp)) {
        type = T::RGBA32bpp;
    }
    int bits = 0;
    switch (type) {
        case T::RGBA32bpp:
            bits = 32;
            break;
        case T::RGBA16bpp:
        case T::GrayscaleAlpha16bpp:
            bits = 16;
            break;
        case T::Grayscale8bpp:
        case T::GrayscaleAlpha8bpp:
            bits = 8;
            break;
        case T::Grayscale4bpp:
        case T::GrayscaleAlpha4bpp:
            bits = 4;
            break;
        default:
            throw std::runtime_error("Unsupported texture format: " + path);
    }
    if ((count * bits + 7) / 8 > resource->ImageDataSize) {
        throw std::runtime_error("Incomplete texture: " + path);
    }
    result.rgba.resize(count * 4);
    const auto* data = resource->ImageData;
    for (size_t i = 0; i < count; ++i) {
        auto* p = &result.rgba[i * 4];
        unsigned v = bits == 4 ? (data[i / 2] >> (i % 2 ? 0 : 4)) & 15 : data[i * bits / 8];
        switch (type) {
            case T::RGBA32bpp:
                std::copy_n(data + i * 4, 4, p);
                break;
            case T::RGBA16bpp: {
                v = (data[i * 2] << 8) | data[i * 2 + 1];
                p[0] = ((v >> 11) & 31) * 255 / 31;
                p[1] = ((v >> 6) & 31) * 255 / 31;
                p[2] = ((v >> 1) & 31) * 255 / 31;
                p[3] = (v & 1) * 255;
                break;
            }
            case T::Grayscale4bpp:
                p[0] = p[1] = p[2] = 255;
                p[3] = v * 17;
                break;
            case T::Grayscale8bpp:
                p[0] = p[1] = p[2] = 255;
                p[3] = v;
                break;
            case T::GrayscaleAlpha4bpp:
                p[0] = p[1] = p[2] = ((v >> 1) * 255 + 3) / 7;
                p[3] = (v & 1) * 255;
                break;
            case T::GrayscaleAlpha8bpp:
                p[0] = p[1] = p[2] = (v >> 4) * 17;
                p[3] = (v & 15) * 17;
                break;
            case T::GrayscaleAlpha16bpp:
                p[0] = p[1] = p[2] = v;
                p[3] = data[i * 2 + 1];
                break;
            default:
                break;
        }
    }
    return result;
}

static Image Texture(const std::string& name, const AssetContext& context) {
    std::string path;
    if (name.starts_with("item-")) {
        path = context.itemIconPath;
    } else if (auto entry = AssetPaths.find(name); entry != AssetPaths.end()) {
        path = entry->second;
    }
    return path.empty() ? Image{} : LoadTexture(path, context);
}

Image ReadAsset(const std::string& name, const AssetContext& context) {
    // Pause pages are "items-<language>" and "equipment-<language>".
    int language;
    const bool items = ParseIndexed(name, "items-", language);
    if (!items && !ParseIndexed(name, "equipment-", language)) {
        return Texture(name, context);
    }
    if (language >= LANGUAGE_MAX) {
        return {};
    }
    // The pause menu's own tables, 3 columns of 5 rows.
    void** pageTextures = KaleidoScope_GetPageTextures(items ? PAUSE_ITEM : PAUSE_EQUIP, language);
    std::array<Image, 15> tiles;
    int scale = 1;
    for (size_t i = 0; i < tiles.size(); ++i) {
        auto& tile = tiles[i];
        tile = LoadTexture(StripOtrSignature(static_cast<const char*>(pageTextures[i])), context);
        scale = std::max({ scale, (tile.width + 79) / 80, (tile.height + 31) / 32 });
    }
    // Preserve HD tile detail, including packs that only replace some tiles.
    if (scale > 16) {
        throw std::runtime_error("Pause texture scale exceeds 16x");
    }
    Image result{ 240 * scale, 160 * scale, {} };
    result.rgba.resize(size_t(result.width) * result.height * 4);
    const std::array<int, 3> edge = items ? std::array<int, 3>{ 10, 50, 80 } : std::array<int, 3>{ 10, 50, 40 };
    const std::array<int, 3> center = items ? std::array<int, 3>{ 70, 100, 130 } : std::array<int, 3>{ 90, 100, 60 };
    for (int y = 0; y < result.height; ++y) {
        for (int x = 0; x < result.width; ++x) {
            int col = x / (80 * scale), row = y / (32 * scale);
            const auto& tile = tiles[col * 5 + row];
            int tx = (x % (80 * scale)) * tile.width / (80 * scale);
            int ty = (y % (32 * scale)) * tile.height / (32 * scale);
            auto* dst = &result.rgba[(size_t(y) * result.width + x) * 4];
            const auto* src = &tile.rgba[(size_t(ty) * tile.width + tx) * 4];
            double px = double(x) / scale;
            double mix = std::clamp(std::min(px / 80, (239 - px) / 80), 0.0, 1.0);
            for (int c = 0; c < 3; ++c) {
                dst[c] = static_cast<uint8_t>(std::lround(src[c] * (edge[c] + (center[c] - edge[c]) * mix) / 255));
            }
            dst[3] = src[3];
        }
    }
    return result;
}

std::string EncodePng(const Image& image) {
    std::string result;
    auto append = [](void* context, void* data, int size) {
        static_cast<std::string*>(context)->append(static_cast<const char*>(data), size);
    };
    if (!stbi_write_png_to_func(append, &result, image.width, image.height, 4, image.rgba.data(), image.width * 4)) {
        throw std::runtime_error("PNG encoding failed");
    }
    return result;
}
} // namespace Shipmate
