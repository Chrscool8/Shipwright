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

void SohMenu::AddMenuNetwork() {
    // Add Network Menu
    AddMenuEntry("Network", CVAR_SETTING("Menu.NetworkSidebarSection"));
    WidgetPath path;

#ifdef ENABLE_SHIPMATE
    path = { "Network", "Shipmate", SECTION_COLUMN_1 };
    AddSidebarEntry("Network", path.sidebarName, 1);
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
    AddWidget(path, "Connect phone##Shipmate", WIDGET_CUSTOM).CustomFunction([](WidgetInfo&) {
        if (!Shipmate::IsEnabled() || !CVarGetInteger(CVAR_REMOTE("Shipmate.LAN"), 0)) {
            return;
        }
        const auto& url = Shipmate::LanUrl();
        if (url.empty()) {
            ImGui::TextWrapped("No LAN address found. On your phone, open http://<PC-LAN-IP>:%d/.",
                               CVarGetInteger(CVAR_REMOTE("Shipmate.Port"), Shipmate::DefaultPort));
            return;
        }
        static std::string previousUrl;
        static uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(4)]{};
        static bool valid = false;
        if (previousUrl != url) {
            uint8_t scratch[sizeof(qr)];
            valid =
                qrcodegen_encodeText(url.c_str(), scratch, qr, qrcodegen_Ecc_MEDIUM, 1, 4, qrcodegen_Mask_AUTO, true);
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
        if (ImGui::Button("Copy URL##ShipmateLAN")) {
            ImGui::SetClipboardText(url.c_str());
        }
    });
    AddWidget(path, "Open in Browser", WIDGET_BUTTON)
        .PreFunc([](WidgetInfo& info) { info.options->disabled = !Shipmate::IsEnabled(); })
        .Callback([](WidgetInfo&) {
            auto url = "http://127.0.0.1:" +
                       std::to_string(CVarGetInteger(CVAR_REMOTE("Shipmate.Port"), Shipmate::DefaultPort)) + "/";
            SDL_OpenURL(url.c_str());
        });
    AddWidget(path, "Shipmate URL", WIDGET_TEXT).PreFunc([](WidgetInfo& info) {
        info.name =
            "http://127.0.0.1:" + std::to_string(CVarGetInteger(CVAR_REMOTE("Shipmate.Port"), Shipmate::DefaultPort)) +
            "/";
    });
    AddWidget(path, "##ShipmateError", WIDGET_TEXT).PreFunc([](WidgetInfo& info) {
        info.isHidden = Shipmate::Error().empty();
        info.name = Shipmate::Error() + "##ShipmateError";
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
