#include "Shipmate.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/randomizer/randomizer_check_objects.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/randomizer_check_tracker.h"
#include "soh/OTRGlobals.h"

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
} // namespace

// Everything here comes from the check tracker, so both views agree on what is listed and revealed.
nlohmann::json Shipmate::Checks() {
    bool loaded = GameInteractor::IsSaveLoaded() && CheckTracker::IsInitialized();
    nlohmann::json state = { { "loaded", loaded } };
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
                                                { "item", CheckTracker::GetCheckItemText(rc) },
                                                { "status", StatusName(itemLoc->GetCheckStatus()) },
                                                { "skipped", itemLoc->GetIsSkipped() } });
                }
            }
            state["areas"].push_back(std::move(entry));
        }
    }
    return { { "status", "success" }, { "state", state } };
}
