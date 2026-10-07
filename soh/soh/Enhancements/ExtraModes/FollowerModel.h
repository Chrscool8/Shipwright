#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "soh/cvar_prefixes.h"

#define CVAR_FOLLOWER_MODEL_NAME CVAR_ENHANCEMENT("FollowerModel")

// Multiplies how fast the following dog moves; 1 leaves it unchanged
#define CVAR_FOLLOWER_DOG_SPEED_NAME CVAR_ENHANCEMENT("FollowerDogSpeed")
inline constexpr float FOLLOWER_DOG_SPEED_DEFAULT = 1.0f;
inline constexpr float FOLLOWER_DOG_SPEED_MIN = 1.0f;
inline constexpr float FOLLOWER_DOG_SPEED_MAX = 3.0f;

// Extra buddies that follow along with the main one: a comma separated list of model ids, at most FOLLOWER_PARTY_MAX
#define CVAR_FOLLOWER_PARTY_NAME CVAR_ENHANCEMENT("FollowerParty")
inline constexpr size_t FOLLOWER_PARTY_MAX = 19;

// How the party arranges itself: a line trailing the main buddy, a ring around Link, or a tower on the main buddy
#define CVAR_FOLLOWER_FORMATION_NAME CVAR_ENHANCEMENT("FollowerFormation")
enum FollowerFormation {
    FOLLOWER_FORMATION_CONGA,
    FOLLOWER_FORMATION_CIRCLE,
    FOLLOWER_FORMATION_TOTEM,
};

// How far apart the conga line spaces itself, in units
#define CVAR_FOLLOWER_CONGA_GAP_NAME CVAR_ENHANCEMENT("FollowerCongaGap")
inline constexpr float FOLLOWER_CONGA_GAP_DEFAULT = 30.0f;
inline constexpr float FOLLOWER_CONGA_GAP_MIN = 10.0f;
inline constexpr float FOLLOWER_CONGA_GAP_MAX = 80.0f;

std::vector<int32_t> FollowerModel_GetParty();
void FollowerModel_SetParty(const std::vector<int32_t>& party);

// Display names for the following dog's models, keyed by the FollowerModel CVar value
extern const std::map<int32_t, const char*> followerModelNames;

// The kind of buddy a model is, such as "Creatures" or "Items"
const char* FollowerModelGroup(int32_t id);

// Controls for the following dog, for the game thread only. Summon replaces the current follower (or brings back the
// last one) with the selected model; dismiss sends it home. Each returns an error message, or nullptr on success.
bool FollowerModel_IsFollowing();
const char* FollowerModel_Summon();
const char* FollowerModel_Dismiss();
