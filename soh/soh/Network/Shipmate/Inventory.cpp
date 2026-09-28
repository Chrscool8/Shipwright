#include "Shipmate.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "src/overlays/misc/ovl_kaleido_scope/z_kaleido_scope.h"
extern PlayState* gPlayState;
}

static bool CanChangeEquipment() {
    return GameInteractor::IsSaveLoaded() && GameInteractor::IsPlayerInControl() && gSaveContext.health > 0 &&
           gPlayState->transitionTrigger == 0 && gPlayState->transitionMode == 0 && gSaveContext.minigameState == 0 &&
           !gPlayState->shootingGalleryStatus && !(GET_PLAYER(gPlayState)->stateFlags1 & PLAYER_STATE1_ON_HORSE);
}

static bool CanAssignSlot(int slot) {
    if (slot < 0 || slot >= 24) {
        return false;
    }
    int item = gSaveContext.inventory.items[slot];
    if (item == ITEM_NONE || item == ITEM_SOLD_OUT || !CHECK_AGE_REQ_SLOT(slot)) {
        return false;
    }
    if (item == ITEM_ARROW_FIRE || item == ITEM_ARROW_ICE || item == ITEM_ARROW_LIGHT) {
        return INV_CONTENT(ITEM_BOW) == ITEM_BOW;
    }
    return true;
}

nlohmann::json Shipmate::HandleRequest(const nlohmann::json& request) {
    auto fail = [](const char* message) { return nlohmann::json{ { "status", "failure" }, { "message", message } }; };
    if (request.value("schemaVersion", 1) != 1) {
        return fail("Unsupported Shipmate version");
    }
    const auto action = request.at("action").get<std::string>();
    bool loaded = GameInteractor::IsSaveLoaded();
    bool canChange = CanChangeEquipment();
    bool dpad = CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0);
    if (action == "snapshot") {
        nlohmann::json state = { { "schemaVersion", 1 },
                                 { "loaded", loaded },
                                 { "canChange", canChange },
                                 { "dpadEnabled", dpad },
                                 { "assetRevision", AssetRevision() },
                                 { "colors", HudColors() } };
        if (loaded) {
            state["fileNum"] = gSaveContext.fileNum;
            state["age"] = gSaveContext.linkAge;
            state["items"] = gSaveContext.inventory.items;
            state["ammo"] = gSaveContext.inventory.ammo;
            state["ownedEquipment"] = gSaveContext.inventory.equipment;
            state["upgrades"] = gSaveContext.inventory.upgrades;
            state["biggoronSword"] = gSaveContext.bgsFlag != 0;
            state["equipmentAllowed"] = nlohmann::json::array();
            for (int category = 0; category < 4; ++category) {
                for (int value = 1; value <= 3; ++value) {
                    state["equipmentAllowed"].push_back(CHECK_AGE_REQ_EQUIP(category, value));
                }
            }
            state["equipment"] = gSaveContext.equips.equipment;
            state["buttonItems"] = gSaveContext.equips.buttonItems;
            state["buttonSlots"] = gSaveContext.equips.cButtonSlots;
            state["assignable"] = nlohmann::json::array();
            for (int slot = 0; slot < 24; ++slot) {
                state["assignable"].push_back(CanAssignSlot(slot));
            }
        }
        return { { "status", "success" }, { "state", state } };
    }
    if (!canChange) {
        return fail("Wait until Link is in control, outside menus and transitions");
    }
    if (request.at("fileNum").get<int>() != gSaveContext.fileNum ||
        request.at("age").get<int>() != gSaveContext.linkAge) {
        return fail("Save or age changed; select the item again");
    }
    if (action == "assign") {
        int slot = request.at("slot").get<int>();
        int button = request.at("button").get<int>();
        if (button < 0 || button >= 7 || (button >= 3 && !dpad)) {
            return fail("Button unavailable; D-pad requires Equip Items on Dpad");
        }
        if (!CanAssignSlot(slot)) {
            return fail("Item is not owned or cannot be assigned at this age");
        }
        int item = gSaveContext.inventory.items[slot];
        if (request.at("item").get<int>() != item) {
            return fail("Inventory changed; select the item again");
        }
        KaleidoScope_AssignItemToButton(gPlayState, item, slot, button);
    } else if (action == "unassign") {
        int button = request.at("button").get<int>();
        if (button < 0 || button >= 7 || (button >= 3 && !dpad)) {
            return fail("Button unavailable; D-pad requires Equip Items on Dpad");
        }
        if (request.at("item").get<int>() != gSaveContext.equips.buttonItems[button + 1] ||
            request.at("slot").get<int>() != gSaveContext.equips.cButtonSlots[button]) {
            return fail("Assignment changed; select the button again");
        }
        // Empty buttons are not drawn; no item texture needs loading for ITEM_NONE.
        gSaveContext.equips.buttonItems[button + 1] = ITEM_NONE;
        gSaveContext.equips.cButtonSlots[button] = SLOT_NONE;
    } else if (action == "equip") {
        int category = request.at("category").get<int>();
        int value = request.at("value").get<int>();
        if (category < EQUIP_TYPE_SWORD || category > EQUIP_TYPE_BOOTS || value < 1 || value > 3) {
            return fail("Invalid equipment selection");
        }
        bool owned = CHECK_OWNED_EQUIP(category, value - 1) != 0;
        if (category == EQUIP_TYPE_SWORD && value == EQUIP_VALUE_SWORD_BIGGORON) {
            owned = owned || CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE);
        }
        if (!owned || !CHECK_AGE_REQ_EQUIP(category, value)) {
            return fail("Equipment is not owned or cannot be worn at this age");
        }
        // Explicit equip, rather than the pause menu's optional toggle-to-remove behavior.
        Inventory_ChangeEquipment(category, value);
        if (category == EQUIP_TYPE_SWORD) {
            KaleidoScope_UpdateSwordItem(value, ITEM_SWORD_KOKIRI + value - 1);
            Interface_LoadItemIcon1(gPlayState, 0);
        }
        // The pause menu does this on closing; apply it immediately for Shipmate.
        Player_SetEquipmentData(gPlayState, GET_PLAYER(gPlayState));
    } else {
        return fail("Unknown Shipmate action");
    }
    return { { "status", "success" }, { "message", "Equipment updated" } };
}
