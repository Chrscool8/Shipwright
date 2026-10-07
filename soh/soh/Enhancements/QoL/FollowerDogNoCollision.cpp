#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "src/overlays/actors/ovl_En_Dog/z_en_dog.h"
}

static constexpr int32_t CVAR_FOLLOWER_DOG_NO_COLLISION_DEFAULT = 0;
#define CVAR_FOLLOWER_DOG_NO_COLLISION_NAME CVAR_ENHANCEMENT("FollowerDogNoCollision")
#define CVAR_FOLLOWER_DOG_NO_COLLISION_VALUE \
    CVarGetInteger(CVAR_FOLLOWER_DOG_NO_COLLISION_NAME, CVAR_FOLLOWER_DOG_NO_COLLISION_DEFAULT)

static void OnInitFollowerDog(void* refActor) {
    EnDog* dog = reinterpret_cast<EnDog*>(refActor);

    // 0x8000 marks the dog that Player respawns each scene while it's following Link. OC hits still register,
    // but the dog and Link no longer push each other around.
    if (dog->actor.params & 0x8000) {
        dog->collider.base.ocFlags1 |= OC1_NO_PUSH;
    }
}

static void RegisterFollowerDogNoCollision() {
    COND_ID_HOOK(OnActorInit, ACTOR_EN_DOG, CVAR_FOLLOWER_DOG_NO_COLLISION_VALUE, OnInitFollowerDog);
}

static RegisterShipInitFunc initFunc(RegisterFollowerDogNoCollision, { CVAR_FOLLOWER_DOG_NO_COLLISION_NAME });
