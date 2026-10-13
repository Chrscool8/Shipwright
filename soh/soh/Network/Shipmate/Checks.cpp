#include "Shipmate.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/randomizer/randomizer_check_objects.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/randomizer_check_tracker.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/OTRGlobals.h"

extern "C" {
#include "variables.h"
}

namespace {
const char* StatusName(RandomizerCheckStatus status) {
    switch (status) {
        case RCSHOW_SEEN_OR_HINTED:
            return "seen";
        case RCSHOW_IDENTIFIED:
            return "identified";
        case RCSHOW_SCUMMED:
            return "scummed";
        case RCSHOW_COLLECTED:
            return "collected";
        case RCSHOW_SAVED:
            return "saved";
        default:
            return "unchecked";
    }
}

// The tracker names a vanilla save's item only for heart pieces, since a check's name usually implies
// it. The page names every item once it is obtained, as a rando save does.
std::string ItemText(RandomizerCheck rc, Rando::ItemLocation* itemLoc) {
    auto txt = CheckTracker::GetCheckItemText(rc);
    const auto status = itemLoc->GetCheckStatus();
    if (txt.empty() && !IS_RANDO &&
        (status == RCSHOW_COLLECTED || status == RCSHOW_SAVED || status == RCSHOW_SCUMMED)) {
        const auto vanillaItem = Rando::StaticData::GetLocation(rc)->GetVanillaItem();
        txt = Rando::StaticData::RetrieveItem(vanillaItem).GetName().GetForLanguage(gSaveContext.language);
    }
    return txt;
}
} // namespace

// Everything here comes from the check tracker, so both views agree on what is listed and revealed.
nlohmann::json Shipmate::Checks() {
    bool loaded = GameInteractor::IsSaveLoaded() && CheckTracker::IsInitialized();
    nlohmann::json state = { { "loaded", loaded } };
    // The tracker never starts for Boss Rush, so such a save loads without any checks.
    if (!loaded && GameInteractor::IsSaveLoaded() && IS_BOSS_RUSH) {
        state["bossRush"] = true;
    }
    if (loaded) {
        state["total"] = CheckTracker::GetTotalChecks();
        state["gotten"] = CheckTracker::GetTotalChecksGotten();
        state["areas"] = nlohmann::json::array();
        for (const auto& [area, checks] : CheckTracker::GetChecksByArea()) {
            const auto total = CheckTracker::GetAreaCheckTotal(area);
            if (total == 0) {
                continue;
            }
            nlohmann::json entry = { { "name", RandomizerCheckObjects::GetRCAreaName(area) },
                                     { "revealed", CheckTracker::IsAreaRevealed(area) } };
            // Unrevealed areas show only their name, as the tracker window shows "???".
            if (entry["revealed"]) {
                entry["gotten"] = CheckTracker::GetAreaChecksGotten(area);
                entry["total"] = total;
                entry["checks"] = nlohmann::json::array();
                // The tracker keeps each area sorted: unchecked first, then saved, skipped and end of dungeon.
                for (auto rc : checks) {
                    if (!CheckTracker::IsVisibleInCheckTracker(rc)) {
                        continue;
                    }
                    auto* itemLoc = OTRGlobals::Instance->gRandoContext->GetItemLocation(rc);
                    entry["checks"].push_back({ { "name", CheckTracker::GetCheckName(rc) },
                                                { "item", ItemText(rc, itemLoc) },
                                                { "status", StatusName(itemLoc->GetCheckStatus()) },
                                                { "skipped", itemLoc->GetIsSkipped() } });
                }
            }
            state["areas"].push_back(std::move(entry));
        }
    }
    return { { "status", "success" }, { "state", state } };
}
