// Fixed browser asset allowlist. No arbitrary resource paths are accepted.
#pragma once
#include <unordered_map>
#include <string>
namespace Shipmate {
inline const std::unordered_map<std::string, std::string> AssetPaths = {
    { "gEquippedItemOutlineTex", "textures/parameter_static/gEquippedItemOutlineTex" },
    { "gLButtonTex", "textures/icon_item_static/gLButtonTex" },
    { "gRButtonTex", "textures/icon_item_static/gRButtonTex" },
    { "gAmmoDigit0Tex", "textures/parameter_static/gAmmoDigit0Tex" },
    { "gAmmoDigit1Tex", "textures/parameter_static/gAmmoDigit1Tex" },
    { "gAmmoDigit2Tex", "textures/parameter_static/gAmmoDigit2Tex" },
    { "gAmmoDigit3Tex", "textures/parameter_static/gAmmoDigit3Tex" },
    { "gAmmoDigit4Tex", "textures/parameter_static/gAmmoDigit4Tex" },
    { "gAmmoDigit5Tex", "textures/parameter_static/gAmmoDigit5Tex" },
    { "gAmmoDigit6Tex", "textures/parameter_static/gAmmoDigit6Tex" },
    { "gAmmoDigit7Tex", "textures/parameter_static/gAmmoDigit7Tex" },
    { "gAmmoDigit8Tex", "textures/parameter_static/gAmmoDigit8Tex" },
    { "gAmmoDigit9Tex", "textures/parameter_static/gAmmoDigit9Tex" },
    { "gPauseMenuCursorTopLeftTex", "textures/icon_item_static/gPauseMenuCursorTopLeftTex" },
    { "gPauseMenuCursorTopRightTex", "textures/icon_item_static/gPauseMenuCursorTopRightTex" },
    { "gPauseMenuCursorBottomLeftTex", "textures/icon_item_static/gPauseMenuCursorBottomLeftTex" },
    { "gPauseMenuCursorBottomRightTex", "textures/icon_item_static/gPauseMenuCursorBottomRightTex" },
    { "hud-button", "textures/parameter_static/gButtonBackgroundTex" },
    { "hud-dpad", "textures/parameter_static/gDPad" },
    { "hud-c-left", "textures/parameter_static/gEmptyCLeftArrowTex" },
    { "hud-c-down", "textures/parameter_static/gEmptyCDownArrowTex" },
    { "hud-c-right", "textures/parameter_static/gEmptyCRightArrowTex" },
};
}
