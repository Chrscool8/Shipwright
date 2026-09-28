#include "Shipmate.h"
#include "AssetPaths.h"
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <fast/resource/type/Texture.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/cvar_prefixes.h"
#include <zlib.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace Shipmate {
static unsigned revision = 1;
unsigned AssetRevision() { return revision; }
void InvalidateAssets() { ++revision; }

static std::array<int, 3> Color(const char* key, std::array<int, 3> fallback) {
    std::string path = CVAR_COSMETIC("") + std::string(key);
    if (!CVarGetInteger((path + ".Changed").c_str(), 0)) return fallback;
    auto color = CVarGetColor24((path + ".Value").c_str(),
                              { (uint8_t)fallback[0], (uint8_t)fallback[1], (uint8_t)fallback[2] });
    return { color.r, color.g, color.b };
}
nlohmann::json HudColors() {
    auto b = CVarGetInteger(CVAR_COSMETIC("DefaultColorScheme"), 0) == 1
                 ? std::array<int, 3>{255, 30, 30} : std::array<int, 3>{0, 150, 0};
    auto c = Color("HUD.CButtons", {255, 160, 0});
    return {{"b", Color("HUD.BButton", b)}, {"left", Color("HUD.CLeftButton", c)},
            {"down", Color("HUD.CDownButton", c)}, {"right", Color("HUD.CRightButton", c)},
            {"dpad", Color("HUD.Dpad", {255, 255, 255})}};
}
AssetContext CaptureAssets() {
    auto manager = Ship::Context::GetRawInstance()->GetResourceManager();
    return {manager, manager->IsAltAssetsEnabled(), AssetRevision()};
}
static Image Texture(const std::string& name, const AssetContext& context) {
    auto entry = AssetPaths.find(name);
    if (entry == AssetPaths.end()) return {};
    // Exact paths avoid consulting the live alternate-assets flag on a worker.
    // Strip the OTR signature: its loader shortcut otherwise resets loadExact.
    const auto path = entry->second.substr(7);
    std::shared_ptr<Ship::IResource> loaded;
    if (context.alternate)
        loaded = context.manager->LoadResourceAsync("alt/" + path, true, BS::pr::low).get();
    if (!loaded) loaded = context.manager->LoadResourceAsync(path, true, BS::pr::low).get();
    // Shared ownership pins the texture buffer through conversion, even across invalidation.
    auto resource = std::dynamic_pointer_cast<Fast::Texture>(loaded);
    if (!resource || !resource->ImageData) throw std::runtime_error("Texture unavailable: " + name);
    Image result;
    result.width = resource->Width; result.height = resource->Height;
    size_t count = size_t(result.width) * result.height;
    if (!count || count > 4096 * 4096) throw std::runtime_error("Unsupported texture dimensions: " + name);
    auto type = resource->Type;
    if (resource->Flags & TEX_FLAG_LOAD_AS_IMG) type = Fast::TextureType::RGBA32bpp;
    int bits = 0;
    using T = Fast::TextureType;
    switch (type) {
        case T::RGBA32bpp: bits = 32; break;
        case T::RGBA16bpp: case T::GrayscaleAlpha16bpp: bits = 16; break;
        case T::Grayscale8bpp: case T::GrayscaleAlpha8bpp: bits = 8; break;
        case T::Grayscale4bpp: case T::GrayscaleAlpha4bpp: bits = 4; break;
        default: throw std::runtime_error("Unsupported texture format: " + name);
    }
    if ((count * bits + 7) / 8 > resource->ImageDataSize)
        throw std::runtime_error("Incomplete texture: " + name);
    result.rgba.resize(count * 4);
    const auto* data = resource->ImageData;
    for (size_t i = 0; i < count; ++i) {
        auto* p = &result.rgba[i * 4];
        unsigned v = bits == 4 ? (data[i / 2] >> (i % 2 ? 0 : 4)) & 15 : data[i * bits / 8];
        switch (type) {
            case T::RGBA32bpp: std::copy_n(data + i * 4, 4, p); break;
            case T::RGBA16bpp: {
                v = (data[i * 2] << 8) | data[i * 2 + 1];
                p[0] = ((v >> 11) & 31) * 255 / 31; p[1] = ((v >> 6) & 31) * 255 / 31;
                p[2] = ((v >> 1) & 31) * 255 / 31; p[3] = (v & 1) * 255; break;
            }
            case T::Grayscale4bpp: p[0] = p[1] = p[2] = 255; p[3] = v * 17; break;
            case T::Grayscale8bpp: p[0] = p[1] = p[2] = 255; p[3] = v; break;
            case T::GrayscaleAlpha4bpp: p[0] = p[1] = p[2] = ((v >> 1) * 255 + 3) / 7; p[3] = (v & 1) * 255; break;
            case T::GrayscaleAlpha8bpp: p[0] = p[1] = p[2] = (v >> 4) * 17; p[3] = (v & 15) * 17; break;
            case T::GrayscaleAlpha16bpp: p[0] = p[1] = p[2] = v; p[3] = data[i * 2 + 1]; break;
            default: break;
        }
    }
    return result;
}
Image ReadAsset(const std::string& name, const AssetContext& context) {
    if (name != "items" && name != "equipment") {
        return Texture(name, context);
    }
    const bool items = name == "items";
    std::array<Image, 15> tiles;
    int scale = 1;
    for (int col = 0; col < 3; ++col) for (int row = 0; row < 5; ++row) {
        auto symbol = std::string("gPause") + (items ? "SelectItem" : "Equipment") +
                      std::to_string(col) + std::to_string(row) +
                      ((row == 0 && (items || col == 1)) ? "ENGTex" : "Tex");
        auto& tile = tiles[col * 5 + row];
        tile = Texture(symbol, context);
        scale = std::max({scale, (tile.width + 79) / 80, (tile.height + 31) / 32});
    }
    // Preserve HD tile detail, including packs that only replace some tiles.
    if (scale > 16) throw std::runtime_error("Pause texture scale exceeds 16x");
    Image result{240 * scale, 160 * scale, {}};
    result.rgba.resize(size_t(result.width) * result.height * 4);
    const std::array<int, 3> edge = items ? std::array<int, 3>{10, 50, 80} : std::array<int, 3>{10, 50, 40};
    const std::array<int, 3> center = items ? std::array<int, 3>{70, 100, 130} : std::array<int, 3>{90, 100, 60};
    for (int y = 0; y < result.height; ++y) for (int x = 0; x < result.width; ++x) {
        int col = x / (80 * scale), row = y / (32 * scale);
        const auto& tile = tiles[col * 5 + row];
        int tx = (x % (80 * scale)) * tile.width / (80 * scale);
        int ty = (y % (32 * scale)) * tile.height / (32 * scale);
        auto* dst = &result.rgba[(size_t(y) * result.width + x) * 4];
        const auto* src = &tile.rgba[(size_t(ty) * tile.width + tx) * 4];
        double px = double(x) / scale;
        double mix = std::clamp(std::min(px / 80, (239 - px) / 80), 0.0, 1.0);
        for (int c = 0; c < 3; ++c)
            dst[c] = static_cast<uint8_t>(std::lround(src[c] * (edge[c] + (center[c] - edge[c]) * mix) / 255));
        dst[3] = src[3];
    }
    return result;
}

// RGBA PNGs need only a filter byte per row and zlib, already used by Ship.
std::string EncodePng(const Image& image) {
    auto integer = [](std::string& out, uint32_t n) {
        for (int shift = 24; shift >= 0; shift -= 8) out.push_back(char(n >> shift));
    };
    std::string result("\x89PNG\r\n\x1a\n", 8);
    auto chunk = [&](const char* type, const std::string& bytes) {
        integer(result, uint32_t(bytes.size()));
        size_t start = result.size(); result.append(type, 4); result += bytes;
        integer(result, crc32(0, reinterpret_cast<const Bytef*>(result.data() + start), uInt(4 + bytes.size())));
    };
    std::string header;
    integer(header, image.width); integer(header, image.height);
    header.append("\x08\x06\0\0\0", 5); chunk("IHDR", header);
    std::string rows;
    const size_t stride = size_t(image.width) * 4;
    rows.reserve((stride + 1) * image.height);
    for (int y = 0; y < image.height; ++y) {
        rows.push_back(0);
        rows.append(reinterpret_cast<const char*>(image.rgba.data() + size_t(y) * stride), stride);
    }
    uLongf size = compressBound(uLong(rows.size()));
    std::string compressed(size, '\0');
    if (compress2(reinterpret_cast<Bytef*>(compressed.data()), &size,
                  reinterpret_cast<const Bytef*>(rows.data()), uLong(rows.size()), Z_BEST_SPEED) != Z_OK)
        throw std::runtime_error("PNG encoding failed");
    compressed.resize(size); chunk("IDAT", compressed); chunk("IEND", "");
    return result;
}
}
