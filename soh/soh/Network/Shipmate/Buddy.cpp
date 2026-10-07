#include "Shipmate.h"
#include "soh/Enhancements/ExtraModes/FollowerModel.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <algorithm>

static void SaveSettings() {
    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

// The following dog's model and speed, the same settings as in Enhancements > Extra Modes.
nlohmann::json Shipmate::Buddy() {
    nlohmann::json models = nlohmann::json::array();
    for (const auto& [id, name] : followerModelNames) {
        models.push_back({ { "id", id }, { "name", name }, { "group", FollowerModelGroup(id) } });
    }
    nlohmann::json speed = { { "value", CVarGetFloat(CVAR_FOLLOWER_DOG_SPEED_NAME, FOLLOWER_DOG_SPEED_DEFAULT) },
                             { "min", FOLLOWER_DOG_SPEED_MIN },
                             { "max", FOLLOWER_DOG_SPEED_MAX } };
    nlohmann::json state = { { "loaded", true },
                             { "selected", CVarGetInteger(CVAR_FOLLOWER_MODEL_NAME, 0) },
                             { "models", models },
                             { "speed", speed },
                             { "following", FollowerModel_IsFollowing() },
                             { "party", FollowerModel_GetParty() },
                             { "partyMax", FOLLOWER_PARTY_MAX },
                             { "formation", CVarGetInteger(CVAR_FOLLOWER_FORMATION_NAME, FOLLOWER_FORMATION_CONGA) },
                             { "gap",
                               { { "value", CVarGetFloat(CVAR_FOLLOWER_CONGA_GAP_NAME, FOLLOWER_CONGA_GAP_DEFAULT) },
                                 { "min", FOLLOWER_CONGA_GAP_MIN },
                                 { "max", FOLLOWER_CONGA_GAP_MAX } } } };
    return { { "status", "success" }, { "state", state } };
}

// Either or both of the party list and its formation; both apply the next time the party spawns.
nlohmann::json Shipmate::SetBuddyParty(const nlohmann::json& request) {
    if (request.contains("party")) {
        std::vector<int32_t> party;
        for (const auto& id : request.at("party")) {
            if (!id.is_number_integer() || !followerModelNames.contains(id.get<int32_t>())) {
                return { { "status", "failure" }, { "message", "Unknown buddy" } };
            }
            party.push_back(id.get<int32_t>());
        }
        if (party.size() > FOLLOWER_PARTY_MAX) {
            return { { "status", "failure" }, { "message", "The party is full" } };
        }
        FollowerModel_SetParty(party);
    }
    if (request.contains("formation")) {
        const int32_t formation = request.at("formation").get<int32_t>();
        if (formation < FOLLOWER_FORMATION_CONGA || formation > FOLLOWER_FORMATION_TOTEM) {
            return { { "status", "failure" }, { "message", "Unknown formation" } };
        }
        CVarSetInteger(CVAR_FOLLOWER_FORMATION_NAME, formation);
    }
    if (request.contains("gap")) {
        CVarSetFloat(CVAR_FOLLOWER_CONGA_GAP_NAME,
                     std::clamp(request.at("gap").get<float>(), FOLLOWER_CONGA_GAP_MIN, FOLLOWER_CONGA_GAP_MAX));
    }
    SaveSettings();
    return { { "status", "success" } };
}

nlohmann::json Shipmate::BuddyAction(const nlohmann::json& request) {
    const auto action = request.at("action").get<std::string>();
    const char* error;
    if (action == "summon") {
        error = FollowerModel_Summon();
    } else if (action == "dismiss") {
        error = FollowerModel_Dismiss();
    } else {
        error = "Unknown buddy action";
    }
    if (error != nullptr) {
        return { { "status", "failure" }, { "message", error } };
    }
    return { { "status", "success" } };
}

nlohmann::json Shipmate::SetBuddy(const nlohmann::json& request) {
    const int32_t id = request.at("model").get<int32_t>();
    if (!followerModelNames.contains(id)) {
        return { { "status", "failure" }, { "message", "Unknown buddy" } };
    }
    CVarSetInteger(CVAR_FOLLOWER_MODEL_NAME, id);
    SaveSettings();
    return { { "status", "success" } };
}

nlohmann::json Shipmate::SetBuddySpeed(const nlohmann::json& request) {
    const float speed = request.at("speed").get<float>();
    CVarSetFloat(CVAR_FOLLOWER_DOG_SPEED_NAME, std::clamp(speed, FOLLOWER_DOG_SPEED_MIN, FOLLOWER_DOG_SPEED_MAX));
    SaveSettings();
    return { { "status", "success" } };
}
