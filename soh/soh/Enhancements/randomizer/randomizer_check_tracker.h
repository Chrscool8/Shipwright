#pragma once

#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "randomizerTypes.h"
#include "soh/SohGui/UIWidgets.hpp"

namespace CheckTracker {

class CheckTrackerSettingsWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;
    ~CheckTrackerSettingsWindow(){};

  protected:
    void InitElement() override{};
    void DrawElement() override;
    void UpdateElement() override{};
};

class CheckTrackerWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;
    void Draw() override;
    ~CheckTrackerWindow(){};

  protected:
    void InitElement() override;
    void DrawElement() override;
    void UpdateElement() override;
};

// Converts an index into a Little Endian bitmask, as follows:
// 00: 0000000100000000
// 01: 0000001000000000
//...
// 06: 0100000000000000
// 07: 1000000000000000
// 08: 0000000000000001
// 09: 0000000000000010
//...
// 14: 0000000001000000
// 15: 0000000010000000
// repeat...
#define INDEX_TO_16BIT_LITTLE_ENDIAN_BITMASK(idx) (0x8000 >> (7 - (idx % 8) + ((idx % 16) / 8) * 8))

void Teardown();
void UpdateAllOrdering();
bool IsVisibleInCheckTracker(RandomizerCheck rc);
bool IsCheckShuffled(RandomizerCheck rc);
void InitTrackerData(bool isDebug);
RandomizerCheckArea GetCheckArea();
uint16_t GetTotalChecks();
uint16_t GetTotalChecksGotten();
bool IsAreaSpoiled(RandomizerCheckArea rcArea);
// Whether the area's checks may be listed: spoiled, or MQ spoilers are turned on.
bool IsAreaRevealed(RandomizerCheckArea rcArea);
void SetAreaSpoiled(RandomizerCheckArea rcArea);
// Tracker data for other views, such as Shipmate; valid once a save has loaded.
bool IsInitialized();
const std::map<RandomizerCheckArea, std::vector<RandomizerCheck>>& GetChecksByArea();
uint16_t GetAreaChecksGotten(RandomizerCheckArea rcArea);
uint16_t GetAreaCheckTotal(RandomizerCheckArea rcArea);
// The tracker's row text: the check's name, and the item or price shown beside it (empty if none).
std::string GetCheckName(RandomizerCheck rc);
std::string GetCheckItemText(RandomizerCheck rc);
void UpdateInventoryChecks();
void UpdateAreas(RandomizerCheckArea area);
void UpdateAllOrdering();
void UpdateAllAreas();
void RecalculateAllAreaTotals();
void SpoilAreaFromCheck(RandomizerCheck rc);
void SpoilAreaFromCantObtain(RandomizerGet rg);
void RecalculateAvailableChecks(RandomizerRegion startingRegion = RR_ROOT, RandoAgeTime startingAgeTime = RAT_NONE);
void LoadFromPreset(const nlohmann::json& info);
bool AreAllSilversSpoiled();
} // namespace CheckTracker
