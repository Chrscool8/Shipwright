#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "FollowerModel.h"

extern "C" {
#include "z64.h"
}

#define CVAR_FOLLOWER_DOG_SPEED_VALUE CVarGetFloat(CVAR_FOLLOWER_DOG_SPEED_NAME, FOLLOWER_DOG_SPEED_DEFAULT)

// Runs after the dog's update, which has already moved it by its velocity this frame. Adding the extra share here
// leaves the dog's own speed logic untouched, and the next frame's floor and wall checks settle the new position.
static void OnFollowerDogUpdate(void* refActor) {
    Actor* actor = static_cast<Actor*>(refActor);

    // 0x8000 marks the dog that follows Link
    if (!(actor->params & 0x8000)) {
        return;
    }

    const float extra = CVAR_FOLLOWER_DOG_SPEED_VALUE - 1.0f;
    actor->world.pos.x += actor->velocity.x * extra;
    actor->world.pos.z += actor->velocity.z * extra;
}

static void RegisterFollowerDogSpeed() {
    COND_ID_HOOK(OnActorUpdate, ACTOR_EN_DOG, CVAR_FOLLOWER_DOG_SPEED_VALUE != FOLLOWER_DOG_SPEED_DEFAULT,
                 OnFollowerDogUpdate);
}

static RegisterShipInitFunc initFunc(RegisterFollowerDogSpeed, { CVAR_FOLLOWER_DOG_SPEED_NAME });
