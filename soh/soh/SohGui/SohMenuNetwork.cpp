#include <ship/Context.h>

#include "SohMenu.h"
#include <soh/Notification/Notification.h>
#include "SohGui.hpp"
#include "soh/OTRGlobals.h"
#include "soh/util.h"
#include <soh/Network/Sail/Sail.h>
#ifdef ENABLE_SHIPMATE
#include <soh/Network/Shipmate/Shipmate.h>
#include <qrcodegen.h>
#include <algorithm>
#endif
#include <soh/Network/CrowdControl/CrowdControl.h>
#include "soh/SohGui/UIWidgets.hpp"

namespace SohGui {

extern std::shared_ptr<SohMenu> mSohMenu;
using namespace UIWidgets;

#ifdef ENABLE_SHIPMATE
// Background choices; index 0 leaves it to the browser's last pick. Values match app.js's backgrounds.
static const std::map<int32_t, const char*> shipmateBackgrounds = {
    { 0, "Last used" }, { 1, "Plain" }, { 2, "Sunrise sky" }, { 3, "Day sky" }, { 4, "Sunset sky" }, { 5, "Night sky" },
};
static const char* shipmateBackgroundValues[] = { nullptr, "plain", "sunrise", "day", "sunset", "night" };
// Starting pause page; index 0 leaves it to the browser's last pick. Values match app.js's views.
static const std::map<int32_t, const char*> shipmateViews = {
    { 0, "Last used" },
    { 1, "Select Item" },
    { 2, "Equipment" },
    { 3, "Quest Status" },
};
static const char* shipmateViewValues[] = { nullptr, "items", "equipment", "quest" };

// Page options from the menu, as read by the web page.
static std::string ShipmateUrl(const std::string& base) {
    std::string query;
    if (CVarGetInteger(CVAR_REMOTE("Shipmate.HideToolbar"), 0)) {
        query += "&toolbar=false";
    }
    if (CVarGetInteger(CVAR_REMOTE("Shipmate.HideHud"), 0)) {
        query += "&hud=false";
    }
    const int background = CVarGetInteger(CVAR_REMOTE("Shipmate.Background"), 0);
    if (background > 0 && background < std::size(shipmateBackgroundValues)) {
        query += std::string("&background=") + shipmateBackgroundValues[background];
    }
    const int view = CVarGetInteger(CVAR_REMOTE("Shipmate.View"), 0);
    if (view > 0 && view < std::size(shipmateViewValues)) {
        query += std::string("&view=") + shipmateViewValues[view];
    }
    return query.empty() ? base : base + "?" + query.substr(1);
}

static std::string ShipmateLocalUrl() {
    return ShipmateUrl("http://127.0.0.1:" +
                       std::to_string(CVarGetInteger(CVAR_REMOTE("Shipmate.Port"), Shipmate::DefaultPort)) + "/");
}

static void ShipmateCopyButton(const std::string& url, const std::string& id) {
    if (Button(("Copy URL" + id).c_str(), ButtonOptions().Size(Sizes::Inline).Color(THEME_COLOR))) {
        ImGui::SetClipboardText(url.c_str());
        Notification::Emit({ .message = "Copied to clipboard" });
    }
}
#endif

void SohMenu::AddMenuNetwork() {
    // Add Network Menu
    AddMenuEntry("Network", CVAR_SETTING("Menu.NetworkSidebarSection"));
    WidgetPath path;

#ifdef ENABLE_SHIPMATE
    path = { "Network", "Shipmate", SECTION_COLUMN_1 };
    AddSidebarEntry("Network", path.sidebarName, 1);
    AddWidget(path, "About Shipmate", WIDGET_SEPARATOR_TEXT);
    AddWidget(path,
              "Shipmate is a companion web page served by Ship. Open it in a browser on this PC, or on a "
              "phone or tablet on your network, to see your pause menu while you play: items, equipment, "
              "quest status, hearts and magic.\n"
              "\n"
              "You can assign items to the C-buttons and D-pad and change equipment from the page, and a "
              "checklist page tracks what you have collected.",
              WIDGET_TEXT);
    AddWidget(path, "Server##Shipmate", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Enable##Shipmate", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_REMOTE("Shipmate.Enabled"))
        .Callback([](WidgetInfo&) { Shipmate::ApplySettings(); });
    AddWidget(path, "Port", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        ImGui::BeginDisabled(Shipmate::IsEnabled() || CVarGetInteger(CVAR_SETTING("DisableChanges"), 0));
        ImGui::Text("%s", info.name.c_str());
        ImGui::SameLine();
        CVarInputInt("##PortShipmate", CVAR_REMOTE("Shipmate.Port"),
                     InputOptions()
                         .Color(THEME_COLOR)
                         .PlaceholderText(std::to_string(Shipmate::DefaultPort))
                         .DefaultValue(std::to_string(Shipmate::DefaultPort))
                         .Size(ImVec2(ImGui::GetFontSize() * 5, 0))
                         .LabelPosition(LabelPositions::None));
        ImGui::EndDisabled();
    });
    AddWidget(path, "Allow LAN access", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_REMOTE("Shipmate.LAN"))
        .Options(
            CheckboxOptions().Tooltip("Let devices on your network open Shipmate at this PC's IP and configured port."))
        .Callback([](WidgetInfo&) {
            if (Shipmate::IsEnabled()) {
                Shipmate::ApplySettings();
            }
        });
    AddWidget(path, "##ShipmateError", WIDGET_TEXT).PreFunc([](WidgetInfo& info) {
        info.isHidden = Shipmate::Error().empty();
        info.name = Shipmate::Error() + "##ShipmateError";
    });
    // Page options and links only matter while the server runs; Port and LAN stay visible to set up beforehand.
    const auto hideWhenOff = [](WidgetInfo& info) { info.isHidden = !Shipmate::IsEnabled(); };
    AddWidget(path, "Page Options##Shipmate", WIDGET_SEPARATOR_TEXT).PreFunc(hideWhenOff);
    AddWidget(path, "Hide toolbar##Shipmate", WIDGET_CVAR_CHECKBOX)
        .PreFunc(hideWhenOff)
        .CVar(CVAR_REMOTE("Shipmate.HideToolbar"))
        .Options(CheckboxOptions().Tooltip(
            "Hide the page's top-right buttons (fullscreen, background, checklist). Applies to the links and QR "
            "code below; adds toolbar=false to the URL."));
    AddWidget(path, "Hide HUD##Shipmate", WIDGET_CVAR_CHECKBOX)
        .PreFunc(hideWhenOff)
        .CVar(CVAR_REMOTE("Shipmate.HideHud"))
        .Options(CheckboxOptions().Tooltip(
            "Hide the hearts, magic and button bar, leaving only the pause panel. Items cannot be assigned to "
            "buttons without it. Applies to the links and QR code below; adds hud=false to the URL."));
    AddWidget(path, "Background##Shipmate", WIDGET_CVAR_COMBOBOX)
        .PreFunc(hideWhenOff)
        .CVar(CVAR_REMOTE("Shipmate.Background"))
        .Options(ComboboxOptions()
                     .ComboMap(shipmateBackgrounds)
                     .Tooltip("The page's starting background. Last used keeps whatever that browser picked "
                              "before. Clicking the page's background button switches it and drops this from "
                              "the URL."));
    AddWidget(path, "Starting page##Shipmate", WIDGET_CVAR_COMBOBOX)
        .PreFunc(hideWhenOff)
        .CVar(CVAR_REMOTE("Shipmate.View"))
        .Options(ComboboxOptions()
                     .ComboMap(shipmateViews)
                     .Tooltip("The pause page the web page opens on. Last used keeps whichever page that "
                              "browser was on. Switching pages on the page drops this from the URL."));
    AddWidget(path, "On This PC##Shipmate", WIDGET_SEPARATOR_TEXT).PreFunc(hideWhenOff);
    AddWidget(path, "Local link##Shipmate", WIDGET_CUSTOM).PreFunc(hideWhenOff).CustomFunction([](WidgetInfo&) {
        const auto url = ShipmateLocalUrl();
        ImGui::TextUnformatted(url.c_str());
        if (Button("Open in Browser##Shipmate", ButtonOptions().Size(Sizes::Inline).Color(THEME_COLOR))) {
            SDL_OpenURL(url.c_str());
        }
        ImGui::SameLine();
        ShipmateCopyButton(url, "##ShipmateLocal");
    });
    AddWidget(path, "On a Phone or Tablet##Shipmate", WIDGET_SEPARATOR_TEXT).PreFunc(hideWhenOff);
    AddWidget(path, "Phone link##Shipmate", WIDGET_CUSTOM).PreFunc(hideWhenOff).CustomFunction([](WidgetInfo&) {
        if (!CVarGetInteger(CVAR_REMOTE("Shipmate.LAN"), 0)) {
            ImGui::TextWrapped("Turn on Allow LAN access above to open Shipmate from another device.");
            return;
        }
        if (Shipmate::LanUrl().empty()) {
            ImGui::TextWrapped("No LAN address found. On your phone, open http://<PC-LAN-IP>:%d/.",
                               CVarGetInteger(CVAR_REMOTE("Shipmate.Port"), Shipmate::DefaultPort));
            return;
        }
        const auto url = ShipmateUrl(Shipmate::LanUrl());
        static std::string previousUrl;
        // Version 6 fits a full LAN address with every option (up to 106 bytes at medium ECC).
        static uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(6)]{};
        static bool valid = false;
        if (previousUrl != url) {
            uint8_t scratch[sizeof(qr)];
            valid =
                qrcodegen_encodeText(url.c_str(), scratch, qr, qrcodegen_Ecc_MEDIUM, 1, 6, qrcodegen_Mask_AUTO, true);
            previousUrl = url;
        }
        ImGui::TextUnformatted("Scan with your phone on the same Wi-Fi.");
        if (valid) {
            const int modules = qrcodegen_getSize(qr);
            // Integer-sized squares and a four-module white border keep the code sharp.
            const int scale = std::max(1, int(ImGui::GetFontSize() * 12 / (modules + 8)));
            const float size = float((modules + 8) * scale);
            auto origin = ImGui::GetCursorScreenPos();
            origin.x = float(int(origin.x));
            origin.y = float(int(origin.y));
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(origin, ImVec2(origin.x + size, origin.y + size), IM_COL32_WHITE);
            for (int y = 0; y < modules; ++y) {
                for (int x = 0; x < modules; ++x) {
                    if (qrcodegen_getModule(qr, x, y)) {
                        const ImVec2 corner(origin.x + (x + 4) * scale, origin.y + (y + 4) * scale);
                        draw->AddRectFilled(corner, ImVec2(corner.x + scale, corner.y + scale), IM_COL32_BLACK);
                    }
                }
            }
            ImGui::Dummy(ImVec2(size, size));
        }
        ImGui::TextUnformatted(url.c_str());
        ShipmateCopyButton(url, "##ShipmateLAN");
    });
#endif

    // Sail
    path = { "Network", "Sail", SECTION_COLUMN_1 };
    AddSidebarEntry("Network", path.sidebarName, 3);

    AddWidget(path,
              "Sail is a networking protocol designed to facilitate remote "
              "control of the Ship of Harkinian client. It is intended to "
              "be utilized alongside a Sail server, for which we provide a "
              "few straightforward implementations on our GitHub. The current "
              "implementations available allow integration with Twitch chat "
              "and SAMMI Bot, feel free to contribute your own!\n"
              "\n"
              "Click this button to copy the link to the Sail Github "
              "page to your clipboard.",
              WIDGET_TEXT);
    AddWidget(path, ICON_FA_CLIPBOARD "##Sail", WIDGET_BUTTON)
        .Callback([](WidgetInfo& info) {
            ImGui::SetClipboardText("https://github.com/HarbourMasters/sail");
            Notification::Emit({
                .message = "Copied to clipboard",
            });
        })
        .Options(ButtonOptions().Tooltip("https://github.com/HarbourMasters/sail"));
    AddWidget(path, "Host & Port", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        ImGui::BeginDisabled(Sail::Instance->isEnabled || CVarGetInteger(CVAR_SETTING("DisableChanges"), 0));
        ImGui::Text("%s", info.name.c_str());
        CVarInputString("##HostSail", CVAR_REMOTE_SAIL("Host"),
                        InputOptions()
                            .Color(THEME_COLOR)
                            .PlaceholderText("127.0.0.1")
                            .DefaultValue("127.0.0.1")
                            .Size(ImVec2(ImGui::GetFontSize() * 15, 0))
                            .LabelPosition(LabelPositions::None));
        ImGui::SameLine();
        ImGui::Text(":");
        ImGui::SameLine();
        CVarInputInt("##PortSail", CVAR_REMOTE_SAIL("Port"),
                     InputOptions()
                         .Color(THEME_COLOR)
                         .PlaceholderText("43384")
                         .DefaultValue("43384")
                         .Size(ImVec2(ImGui::GetFontSize() * 5, 0))
                         .LabelPosition(LabelPositions::None));
        ImGui::EndDisabled();
    });
    AddWidget(path, "Enable##Sail", WIDGET_BUTTON)
        .PreFunc([](WidgetInfo& info) {
            std::string host = CVarGetString(CVAR_REMOTE_SAIL("Host"), "127.0.0.1");
            uint16_t port = CVarGetInteger(CVAR_REMOTE_SAIL("Port"), 43384);
            info.options->disabled = !(!SohUtils::IsStringEmpty(host) && port > 1024 && port < 65535);
            if (Sail::Instance->isEnabled) {
                info.name = "Disable##Sail";
            } else {
                info.name = "Enable##Sail";
            }
        })
        .Callback([](WidgetInfo& info) {
            if (Sail::Instance->isEnabled) {
                CVarClear(CVAR_REMOTE_SAIL("Enabled"));
                Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
                Sail::Instance->Disable();
            } else {
                CVarSetInteger(CVAR_REMOTE_SAIL("Enabled"), 1);
                Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
                Sail::Instance->Enable();
            }
        });
    AddWidget(path, "Connecting...##Sail", WIDGET_TEXT).PreFunc([](WidgetInfo& info) {
        info.isHidden = !Sail::Instance->isEnabled;
        if (Sail::Instance->isConnected) {
            info.name = "Connected##Sail";
        } else {
            info.name = "Connecting...##Sail";
        }
    });

    path.sidebarName = "Crowd Control";
    AddSidebarEntry("Network", path.sidebarName, 3);
    path.column = SECTION_COLUMN_1;

    AddWidget(path, "About Crowd Control", WIDGET_SEPARATOR_TEXT);
    AddWidget(path,
              "Crowd Control is a platform that allows viewers to interact "
              "with a streamer's game in real time.\n"
              "\n"
              "Please head over to www.crowdcontrol.live for more information!",
              WIDGET_TEXT);

    AddWidget(path, "Connect to Crowd Control", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Host & Port", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        ImGui::BeginDisabled(CrowdControl::Instance->isEnabled || CVarGetInteger(CVAR_SETTING("DisableChanges"), 0));
        ImGui::Text("%s", info.name.c_str());
        CVarInputString("##HostCrowdControl", CVAR_REMOTE_CROWD_CONTROL("Host"),
                        InputOptions()
                            .Color(THEME_COLOR)
                            .PlaceholderText("127.0.0.1")
                            .DefaultValue("127.0.0.1")
                            .Size(ImVec2(ImGui::GetFontSize() * 15, 0))
                            .LabelPosition(LabelPositions::None));
        ImGui::SameLine();
        ImGui::Text(":");
        ImGui::SameLine();
        CVarInputInt("##PortCrowdControl", CVAR_REMOTE_CROWD_CONTROL("Port"),
                     InputOptions()
                         .Color(THEME_COLOR)
                         .PlaceholderText("43384")
                         .DefaultValue("43384")
                         .Size(ImVec2(ImGui::GetFontSize() * 5, 0))
                         .LabelPosition(LabelPositions::None));
        ImGui::EndDisabled();
    });
    AddWidget(path, "Enable##CrowdControl", WIDGET_BUTTON)
        .PreFunc([](WidgetInfo& info) {
            std::string host = CVarGetString(CVAR_REMOTE_CROWD_CONTROL("Host"), "127.0.0.1");
            uint16_t port = CVarGetInteger(CVAR_REMOTE_CROWD_CONTROL("Port"), 43384);
            info.options->disabled = !(!SohUtils::IsStringEmpty(host) && port > 1024 && port < 65535);
            if (CrowdControl::Instance->isEnabled) {
                info.name = "Disable##CrowdControl";
            } else {
                info.name = "Enable##CrowdControl";
            }
        })
        .Callback([](WidgetInfo& info) {
            if (CrowdControl::Instance->isEnabled) {
                CVarClear(CVAR_REMOTE_CROWD_CONTROL("Enabled"));
                Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
                CrowdControl::Instance->Disable();
            } else {
                CVarSetInteger(CVAR_REMOTE_CROWD_CONTROL("Enabled"), 1);
                Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
                CrowdControl::Instance->Enable();
            }
        });
    AddWidget(path, "Connecting...", WIDGET_TEXT).PreFunc([](WidgetInfo& info) {
        info.isHidden = !CrowdControl::Instance->isEnabled;
        if (CrowdControl::Instance->isConnected) {
            info.name = "Connected";
        } else {
            info.name = "Connecting...";
        }
    });
    AddWidget(path, "Additional Settings", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Enemy Name Tags", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_REMOTE_CROWD_CONTROL("EnemyNameTags"))
        .RaceDisable(true)
        .Options(CheckboxOptions().Tooltip(
            "When viewers spawn enemies, the enemy will have a name tag above them with the viewer's name."));
    AddWidget(path, "Spawned Enemies Ignored Ingame", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_REMOTE_CROWD_CONTROL("SpawnedEnemiesIgnoredIngame"))
        .RaceDisable(true)
        .Options(CheckboxOptions().Tooltip("Enemies spawned by CrowdControl won't be considered for \"clear enemy "
                                           "rooms\", so they don't need to be killed to complete these rooms."));
    path.sidebarName = "Anchor";
    AddSidebarEntry("Network", path.sidebarName, 2);
}

} // namespace SohGui
