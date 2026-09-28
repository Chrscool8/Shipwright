#include "Shipmate.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/cvar_prefixes.h"
#include "soh/util.h"
#include <algorithm>
#include <string_view>
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
    if (slot < 0 || slot >= ARRAY_COUNT(gSaveContext.inventory.items)) {
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

namespace {
constexpr int CButtonCount = 3;
constexpr int EquipmentItems[][3] = {
    { ITEM_SWORD_KOKIRI, ITEM_SWORD_MASTER, ITEM_SWORD_BGS },
    { ITEM_SHIELD_DEKU, ITEM_SHIELD_HYLIAN, ITEM_SHIELD_MIRROR },
    { ITEM_TUNIC_KOKIRI, ITEM_TUNIC_GORON, ITEM_TUNIC_ZORA },
    { ITEM_BOOTS_KOKIRI, ITEM_BOOTS_IRON, ITEM_BOOTS_HOVER },
};
static_assert(ARRAY_COUNT(EquipmentItems) == EQUIP_TYPE_MAX);

bool ButtonAvailable(int button, bool dpad) {
    return button >= 0 && button < ARRAY_COUNT(gSaveContext.equips.cButtonSlots) && (button < CButtonCount || dpad);
}

bool OwnsEquipment(int category, int value) {
    return CHECK_OWNED_EQUIP(category, value - 1) ||
           (category == EQUIP_TYPE_SWORD && value == EQUIP_VALUE_SWORD_BIGGORON &&
            CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE));
}

nlohmann::json DescribeItem(int item) {
    std::string name = "Empty";
    if (item != ITEM_NONE) {
        name = item >= 0 && item < ARRAY_COUNT(gItemIcons) ? SohUtils::GetItemName(item)
                                                           : "Unknown item " + std::to_string(item);
        if (item == ITEM_SWORD_BGS && !gSaveContext.bgsFlag) {
            name = "Giant's Knife";
        }
    }
    nlohmann::json entry = {
        { "item", item }, { "name", name }, { "empty", item == ITEM_NONE }, { "asset", nullptr }, { "ammo", nullptr }
    };
    if (!Shipmate::ItemIconPath(item).empty()) {
        entry["asset"] = "item-" + std::to_string(item);
    }
    // Use the pause menu's ammo-slot mapping.
    int ammoItem = item >= ITEM_BOW_ARROW_FIRE && item <= ITEM_BOW_ARROW_LIGHT ? ITEM_BOW : item;
    for (int slot = 0; slot < ARRAY_COUNT(gSaveContext.inventory.ammo); ++slot) {
        if (ammoItem != ITEM_NONE && gAmmoItems[slot] == ammoItem) {
            entry["ammo"] = std::max(0, int(gSaveContext.inventory.ammo[slot]));
            break;
        }
    }
    return entry;
}

nlohmann::json DescribeEquipment(int category, int value) {
    int item = EquipmentItems[category][value - 1];
    if (item == ITEM_SWORD_BGS && !gSaveContext.bgsFlag &&
        CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE)) {
        item = ITEM_SWORD_KNIFE;
    }
    auto entry = DescribeItem(item);
    entry["category"] = category;
    entry["value"] = value;
    entry["allowed"] = CHECK_AGE_REQ_EQUIP(category, value);
    entry["equipped"] = CUR_EQUIP_VALUE(category) == value;
    return entry;
}
} // namespace

std::string Shipmate::ItemIconPath(int item) {
    if (item < 0 || item >= ARRAY_COUNT(gItemIcons) || !gItemIcons[item]) {
        return {};
    }
    const auto* path = static_cast<const char*>(gItemIcons[item]);
    // Only registered resource paths are exposed; never interpret arbitrary item data as a path.
    return std::string_view(path).starts_with("__OTR__") ? std::string(path + 7) : std::string();
}

nlohmann::json Shipmate::Snapshot() {
    bool loaded = GameInteractor::IsSaveLoaded();
    bool canChange = CanChangeEquipment();
    bool dpad = CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0);
    nlohmann::json state = { { "schemaVersion", 2 },
                             { "loaded", loaded },
                             { "canChange", canChange },
                             { "assetRevision", AssetRevision() },
                             { "colors", HudColors() } };
    if (loaded) {
        state["fileNum"] = gSaveContext.fileNum;
        state["age"] = gSaveContext.linkAge;
        state["items"] = nlohmann::json::array();
        for (int slot = 0; slot < ARRAY_COUNT(gSaveContext.inventory.items); ++slot) {
            auto entry = DescribeItem(gSaveContext.inventory.items[slot]);
            entry["slot"] = slot;
            entry["allowed"] = CanAssignSlot(slot);
            entry["equipped"] =
                std::find(std::begin(gSaveContext.equips.cButtonSlots), std::end(gSaveContext.equips.cButtonSlots),
                          slot) != std::end(gSaveContext.equips.cButtonSlots);
            if (slot >= SLOT_BOTTLE_1 && slot <= SLOT_BOTTLE_4) {
                entry["name"] =
                    entry["name"].get<std::string>() + " (bottle " + std::to_string(slot - SLOT_BOTTLE_1 + 1) + ")";
            }
            state["items"].push_back(std::move(entry));
        }
        state["buttons"] = nlohmann::json::array();
        for (int index = 0; index < ARRAY_COUNT(gSaveContext.equips.buttonItems); ++index) {
            auto entry = DescribeItem(gSaveContext.equips.buttonItems[index]);
            entry["button"] = index - 1;
            entry["slot"] = index == 0 ? SLOT_NONE : gSaveContext.equips.cButtonSlots[index - 1];
            entry["allowed"] = ButtonAvailable(index - 1, dpad);
            state["buttons"].push_back(std::move(entry));
        }
        state["equipment"] = nlohmann::json::array();
        state["worn"] = nlohmann::json::array();
        for (int category = 0; category < EQUIP_TYPE_MAX; ++category) {
            for (int value = 1; value <= ARRAY_COUNT(EquipmentItems[category]); ++value) {
                if (OwnsEquipment(category, value)) {
                    state["equipment"].push_back(DescribeEquipment(category, value));
                }
            }
            int value = CUR_EQUIP_VALUE(category);
            auto entry = DescribeItem(ITEM_NONE);
            entry["name"] = "None";
            if (value > 0 && value <= ARRAY_COUNT(EquipmentItems[category])) {
                entry = DescribeEquipment(category, value);
            }
            state["worn"].push_back(std::move(entry));
        }
        const bool bulletBag = LINK_IS_CHILD || CUR_UPG_VALUE(UPG_QUIVER) == 0;
        const int upgrades[] = { bulletBag ? UPG_BULLET_BAG : UPG_QUIVER, UPG_BOMB_BAG, UPG_STRENGTH, UPG_SCALE };
        const int bases[] = { bulletBag ? ITEM_BULLET_BAG_30 : ITEM_QUIVER_30, ITEM_BOMB_BAG_20, ITEM_BRACELET,
                              ITEM_SCALE_SILVER };
        state["upgrades"] = nlohmann::json::array();
        for (int row = 0; row < ARRAY_COUNT(upgrades); ++row) {
            int level = CUR_UPG_VALUE(upgrades[row]);
            if (level <= 0 || level >= ARRAY_COUNT(gUpgradeCapacities[0])) {
                continue;
            }
            auto entry = DescribeItem(bases[row] + level - 1);
            entry["row"] = row;
            int capacity = CAPACITY(upgrades[row], level);
            if (capacity > 0) {
                entry["name"] = entry["name"].get<std::string>() + " (Holds " + std::to_string(capacity) + ")";
            }
            state["upgrades"].push_back(std::move(entry));
        }
    }
    return { { "status", "success" }, { "state", state } };
}

nlohmann::json Shipmate::HandleRequest(const nlohmann::json& request) {
    auto fail = [](const char* message) { return nlohmann::json{ { "status", "failure" }, { "message", message } }; };
    if (request.value("schemaVersion", 2) != 2) {
        return fail("Unsupported Shipmate version");
    }
    const auto action = request.at("action").get<std::string>();
    bool canChange = CanChangeEquipment();
    bool dpad = CVarGetInteger(CVAR_ENHANCEMENT("DpadEquips"), 0);
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
        if (!ButtonAvailable(button, dpad)) {
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
        if (!ButtonAvailable(button, dpad)) {
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
        if (category < EQUIP_TYPE_SWORD || category >= EQUIP_TYPE_MAX || value < 1 ||
            value > ARRAY_COUNT(EquipmentItems[category])) {
            return fail("Invalid equipment selection");
        }
        if (!OwnsEquipment(category, value) || !CHECK_AGE_REQ_EQUIP(category, value)) {
            return fail("Equipment is not owned or cannot be worn at this age");
        }
        // Explicit equip, rather than the pause menu's optional toggle-to-remove behavior.
        Inventory_ChangeEquipment(category, value);
        if (category == EQUIP_TYPE_SWORD) {
            KaleidoScope_UpdateSwordItem(value, EquipmentItems[category][value - 1]);
            Interface_LoadItemIcon1(gPlayState, 0);
        }
        // The pause menu does this on closing; apply it immediately for Shipmate.
        Player_SetEquipmentData(gPlayState, GET_PLAYER(gPlayState));
    } else {
        return fail("Unknown Shipmate action");
    }
    return { { "status", "success" }, { "message", "Equipment updated" } };
}
