#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/frame_interpolation.h"
#include "soh/ResourceManagerHelpers.h"
#include "FollowerModel.h"
#include "soh/resource/type/Skeleton.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "src/overlays/actors/ovl_En_Dog/z_en_dog.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/gameplay_dangeon_keep/gameplay_dangeon_keep.h"
#include "objects/gameplay_field_keep/gameplay_field_keep.h"
#include "objects/object_wf/object_wf.h"
#include "objects/object_dodongo/object_dodongo.h"
#include "objects/object_ma1/object_ma1.h"
#include "objects/object_ta/object_ta.h"
#include "objects/object_du/object_du.h"
#include "objects/object_mm/object_mm.h"
#include "objects/object_rd/object_rd.h"
#include "objects/object_skb/object_skb.h"
#include "objects/object_dekunuts/object_dekunuts.h"
#include "objects/object_niw/object_niw.h"
#include "objects/object_skj/object_skj.h"
#include "objects/object_sa/object_sa.h"
#include "objects/object_md/object_md.h"
#include "objects/object_poh/object_poh.h"
#include "objects/object_firefly/object_firefly.h"
#include "objects/object_am/object_am.h"
#include "objects/object_kanban/object_kanban.h"
#include "objects/object_gs/object_gs.h"
#include "objects/object_wallmaster/object_wallmaster.h"
#include "objects/object_okuta/object_okuta.h"
#include "objects/object_owl/object_owl.h"
#include "objects/object_tk/object_tk.h"
#include "objects/object_kz/object_kz.h"
#include "objects/object_ani/object_ani.h"
#include "objects/object_ru1/object_ru1.h"
#include "objects/object_sd/object_sd.h"
#include "objects/object_dns/object_dns.h"
#include "scenes/indoors/hylia_labo/hylia_labo_scene.h"
#include "objects/object_ec/object_ec.h"
#include "objects/object_km1/object_km1.h"
#include "objects/object_kw1/object_kw1.h"
#include "objects/object_zo/object_zo.h"
#include "objects/object_fu/object_fu.h"
#include "objects/object_in/object_in.h"
#include "objects/object_ane/object_ane.h"
#include "objects/object_ds2/object_ds2.h"
#include "objects/object_os/object_os.h"
#include "objects/object_fish/object_fish.h"
#include "objects/object_rs/object_rs.h"
#include "objects/object_oF1d_map/object_oF1d_map.h"
#include "objects/object_ma2/object_ma2.h"
#include "objects/object_ge1/object_ge1.h"
#include "objects/object_goma/object_goma.h"
#include "objects/object_gol/object_gol.h"
#include "objects/object_sk2/object_sk2.h"
#include "objects/object_mb/object_mb.h"
#include "objects/object_fw/object_fw.h"
#include "objects/object_peehat/object_peehat.h"
#include "objects/object_bird/object_bird.h"
#include "objects/object_ei/object_ei.h"
#include "objects/object_po_composer/object_po_composer.h"
#include "objects/object_po_sisters/object_po_sisters.h"
#include "objects/object_ganon2/object_ganon2.h"
#include "objects/object_gndd/object_gndd.h"
#include "objects/object_xc/object_xc.h"
#include "objects/object_tr/object_tr.h"
#include "objects/object_geldb/object_geldb.h"
#include "objects/object_gla/object_gla.h"
#include "objects/object_cs/object_cs.h"
#include "objects/object_gr/object_gr.h"
#include "objects/object_dnk/object_dnk.h"
#include "objects/object_js/object_js.h"
#include "objects/object_ps/object_ps.h"
#include "objects/object_dy_obj/object_dy_obj.h"
#include "objects/object_horse/object_horse.h"
#include "objects/object_horse_link_child/object_horse_link_child.h"
#include "objects/object_horse_ganon/object_horse_ganon.h"
#include "objects/object_horse_normal/object_horse_normal.h"
#include "objects/object_hni/object_hni.h"
#include "objects/object_fhg/object_fhg.h"
#include "objects/object_horse_zelda/object_horse_zelda.h"
#include "objects/object_spot15_obj/object_spot15_obj.h"
#include "objects/object_spot18_obj/object_spot18_obj.h"
#include "objects/object_haka/object_haka.h"
#include "objects/object_syokudai/object_syokudai.h"
#include "objects/object_sst/object_sst.h"
#include "objects/object_spot01_matoya/object_spot01_matoya.h"
extern PlayState* gPlayState;
}

// Swaps the model drawn for the dog following Link. The dog's own actor still handles all of the movement, this only
// draws something else on top of its behavior: an animated skeleton, a prop that hops along, or a floating item.
//
// This is purely cosmetic, so it stays out of the way: every asset is checked before the swap, and if anything is
// missing or doesn't match (e.g. a mod replacing a skeleton), the dog is left as a normal dog.

static constexpr int32_t CVAR_FOLLOWER_MODEL_DEFAULT = 0; // The dog itself
#define CVAR_FOLLOWER_MODEL_VALUE CVarGetInteger(CVAR_FOLLOWER_MODEL_NAME, CVAR_FOLLOWER_MODEL_DEFAULT)

// Mirrors DogBehavior in z_en_dog.c
#define DOG_WALK 0
#define DOG_RUN 1

// The dog's own size and shadow (z_en_dog.c), which every buddy's shadow keeps
static constexpr f32 DOG_SCALE = 0.0075f;
static constexpr f32 DOG_SHADOW_SCALE = 24.0f;

#define ANIM(anim) ((AnimationHeader*)(anim))
#define LIMB(index) (1u << (index))

typedef void (*FollowerPropDraw)(PlayState* play, Actor* actor);

struct FollowerModelInfo {
    const char* name;
    const char* group;

    // Skeleton models
    void* skeleton = nullptr;
    bool isFlex = false;
    // Skin skeletons, like the horses, deform their vertices and draw through the skin system instead
    bool isSkin = false;
    AnimationHeader* idleAnim = nullptr;
    AnimationHeader* walkAnim = nullptr;
    AnimationHeader* runAnim = nullptr;
    // Played while celebrating an item or jamming to the ocarina; models without one hop instead
    AnimationHeader* danceAnim = nullptr;
    f32 idleSpeed = 1.0f;
    f32 walkSpeed = 1.0f;
    f32 runSpeed = 1.0f;
    // Textures (or display lists) the model expects on segments 0x08, 0x09 and 0x0A, such as eyes and mouths
    void* segments[3] = {};
    Color_RGBA8 envColor = { 0, 0, 0, 255 };
    Color_RGBA8 primColor = { 255, 255, 255, 255 };
    // Limbs the original actor hides when drawing
    u32 hiddenLimbs = 0;
    // Draw like most NPCs do (func_80034BA0), which also sets up segment 0x0C. Flex skeletons only.
    bool npcDraw = false;
    // Clothing tinted by an env color display list on segments 0x08 and 0x09, like the Kokiri in the credits
    bool clothes = false;
    Color_RGB8 clothesColor = {};
    // Segment 0x08 turns off backface culling, for models like Gohma that are open on one side
    bool noCull = false;
    // Segments that hold an env color display list, such as the Composer Poes' clothing colors
    struct {
        u8 segment;
        Color_RGBA8 color;
    } envSegments[3] = {};

    // Props without a skeleton hop along instead of animating. Assets lists what the draw needs, to check first.
    FollowerPropDraw drawProp = nullptr;
    const void* assets[3] = {};
    // Get Item models, drawn with GetItem_Draw, float and spin like a collectible
    s16 getItemDrawId = -1;
    // Cutouts draw assets[0], an RGBA16 texture of this size, on a quad that faces the camera
    u16 cutoutWidth = 0;
    u16 cutoutHeight = 0;

    f32 scale;
    // Lifts the model off the ground (in world units) while its shadow stays put
    f32 hover = 0.0f;
};

// Segments 0x08 to 0x0C are set to this before every draw, so a segment a model uses but doesn't set can't send the
// renderer into stale memory. As a display list it ends immediately; as a texture it reads as blank.
static Gfx sSafeSegment[0x4000 / sizeof(Gfx)] = { gsSPEndDisplayList() };

static void SetSafeSegments(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    for (u32 segment = 0x08; segment <= 0x0C; segment++) {
        gSPSegment(POLY_OPA_DISP++, segment, (uintptr_t)sSafeSegment);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void LoadModelMatrix(PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawSign(PlayState* play, Actor* actor) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)object_kanban_DL_000C30);
    Matrix_Translate(0.0f, 0.0f, -100.0f, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gSignRectangularDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawPot(PlayState* play, Actor* actor) {
    Gfx_DrawDListOpa(play, (Gfx*)gPotDL);
}

static void DrawCrate(PlayState* play, Actor* actor) {
    Gfx_DrawDListOpa(play, (Gfx*)gSmallWoodenBoxDL);
}

static void DrawBeehive(PlayState* play, Actor* actor) {
    Gfx_DrawDListOpa(play, (Gfx*)gFieldBeehiveDL);
}

static void DrawSilverRock(PlayState* play, Actor* actor) {
    Gfx_DrawDListOpa(play, (Gfx*)gSilverRockDL);
}

static void DrawBombchu(PlayState* play, Actor* actor) {
    OPEN_DISPS(play->state.gfxCtx);
    gDPSetEnvColor(POLY_OPA_DISP++, 9, 9, 35, 255);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gBombchuDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Same as an unlit bomb in z_en_bom.c, always facing the camera
static void DrawBomb(PlayState* play, Actor* actor) {
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    func_8002EBCC(actor, play, 0);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gBombCapDL);
    Matrix_RotateZYX(0x4000, 0, 0, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 40, 255);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 0, 0, 40, 255);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gBombBodyDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawGossipStone(PlayState* play, Actor* actor) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGossipStoneMaterialDL);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gGossipStoneDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void SetVtx(Vtx* vtx, s16 x, s16 y, s16 s, s16 t) {
    *vtx = {};
    vtx->v.ob[0] = x;
    vtx->v.ob[1] = y;
    vtx->v.tc[0] = s;
    vtx->v.tc[1] = t;
    vtx->v.cn[0] = vtx->v.cn[1] = vtx->v.cn[2] = vtx->v.cn[3] = 255;
}

static const FollowerModelInfo* CurrentModel();

// A flat picture standing on the ground and always facing the camera, like the posters in the Lakeside Laboratory
static void DrawCutout(PlayState* play, Actor* actor) {
    const FollowerModelInfo* model = CurrentModel();
    const s16 w = model->cutoutWidth;
    const s16 h = model->cutoutHeight;
    // One texel is 100 model units
    const s16 halfWidth = w * 50;
    const s16 height = h * 100;

    Vtx* vtx = (Vtx*)Graph_Alloc(play->state.gfxCtx, 4 * sizeof(Vtx));
    SetVtx(&vtx[0], -halfWidth, height, 0, 0);
    SetVtx(&vtx[1], halfWidth, height, w << 5, 0);
    SetVtx(&vtx[2], halfWidth, 0, w << 5, h << 5);
    SetVtx(&vtx[3], -halfWidth, 0, 0, h << 5);

    OPEN_DISPS(play->state.gfxCtx);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCycleType(POLY_OPA_DISP++, G_CYC_1CYCLE);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gSPClearGeometryMode(POLY_OPA_DISP++, G_LIGHTING | G_CULL_BOTH);
    gSPTexture(POLY_OPA_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPLoadTextureBlock(POLY_OPA_DISP++, model->assets[0], G_IM_FMT_RGBA, G_IM_SIZ_16b, w, h, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    gSPVertex(POLY_OPA_DISP++, (uintptr_t)vtx, 4, 0);
    gSP2Triangles(POLY_OPA_DISP++, 0, 1, 2, 0, 0, 2, 3, 0);
    CLOSE_DISPS(play->state.gfxCtx);
}

static FollowerModelInfo Cutout(const char* name, const void* texture, u16 width, u16 height, f32 scale = 0.004f) {
    return { .name = name,
             .group = "Props",
             .drawProp = DrawCutout,
             .assets = { texture },
             .cutoutWidth = width,
             .cutoutHeight = height,
             .scale = scale };
}

// Any standalone display list, tinted by the model's prim and env colors
static void DrawOddity(PlayState* play, Actor* actor) {
    const FollowerModelInfo* model = CurrentModel();

    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, model->primColor.r, model->primColor.g, model->primColor.b,
                    model->primColor.a);
    gDPSetEnvColor(POLY_OPA_DISP++, model->envColor.r, model->envColor.g, model->envColor.b, model->envColor.a);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)model->assets[0]);
    CLOSE_DISPS(play->state.gfxCtx);
}

static FollowerModelInfo Oddity(const char* name, const void* dList, f32 scale, f32 hover = 0.0f,
                                Color_RGBA8 primColor = { 255, 255, 255, 255 },
                                Color_RGBA8 envColor = { 0, 0, 0, 255 }) {
    return { .name = name,
             .group = "Oddities",
             .envColor = envColor,
             .primColor = primColor,
             .drawProp = DrawOddity,
             .assets = { dList },
             .scale = scale,
             .hover = hover };
}

static FollowerModelInfo Prop(const char* name, FollowerPropDraw draw, const void* asset, f32 scale) {
    return { .name = name, .group = "Props", .drawProp = draw, .assets = { asset }, .scale = scale };
}

static FollowerModelInfo Item(const char* name, s16 getItemDrawId, f32 scale = 0.35f) {
    return { .name = name, .group = "Items", .getItemDrawId = getItemDrawId, .scale = scale, .hover = 15.0f };
}

// Index 0 is the dog itself; the CVar stores an index into this list, so only ever add to the end.
static const FollowerModelInfo sModels[] = {
    { .name = "Dog", .group = "Creatures" },
    { .name = "Wolfos",
      .group = "Creatures",
      .skeleton = (void*)gWolfosNormalSkel,
      .isFlex = true,
      .idleAnim = ANIM(gWolfosWaitingAnim),
      .walkAnim = ANIM(gWolfosRunningAnim),
      .runAnim = ANIM(gWolfosRunningAnim),
      .walkSpeed = 0.5f,
      .segments = { (void*)gWolfosNormalEyeOpenTex },
      .scale = 0.003f },
    { .name = "White Wolfos",
      .group = "Creatures",
      .skeleton = (void*)gWolfosWhiteSkel,
      .isFlex = true,
      .idleAnim = ANIM(gWolfosWaitingAnim),
      .walkAnim = ANIM(gWolfosRunningAnim),
      .runAnim = ANIM(gWolfosRunningAnim),
      .walkSpeed = 0.5f,
      .segments = { (void*)gWolfosWhiteEyeOpenTex },
      .scale = 0.003f },
    { .name = "Dodongo",
      .group = "Creatures",
      .skeleton = (void*)gDodongoSkel,
      .idleAnim = ANIM(gDodongoWaitAnim),
      .walkAnim = ANIM(gDodongoWalkAnim),
      .runAnim = ANIM(gDodongoWalkAnim),
      .runSpeed = 2.0f,
      .scale = 0.005f },
    // Sings while she walks and throws her hands up when she has to run to catch up
    { .name = "Malon",
      .group = "NPCs",
      .skeleton = (void*)gMalonChildSkel,
      .isFlex = true,
      .idleAnim = ANIM(gMalonChildIdleAnim),
      .walkAnim = ANIM(gMalonChildSingAnim),
      .runAnim = ANIM(gMalonChildRaiseHandsAnim),
      .danceAnim = ANIM(gMalonChildRaiseHandsAnim),
      .segments = { (void*)gMalonChildEyeOpenTex, (void*)gMalonChildSmilingMouthTex },
      .hiddenLimbs = LIMB(2) | LIMB(5),
      .scale = 0.006f },
    { .name = "Talon",
      .group = "NPCs",
      .skeleton = (void*)gTalonSkel,
      .isFlex = true,
      .idleAnim = ANIM(gTalonStandAnim),
      .walkAnim = ANIM(gTalonRunAnim),
      .runAnim = ANIM(gTalonRunAnim),
      .danceAnim = ANIM(gTalonSitHandsUpAnim),
      .walkSpeed = 0.6f,
      .segments = { (void*)gTalonEyeOpenTex, (void*)gTalonHeadSkinTex },
      .scale = 0.005f },
    // Never stops dancing while he's on the move
    { .name = "Darunia",
      .group = "NPCs",
      .skeleton = (void*)gDaruniaSkel,
      .isFlex = true,
      .idleAnim = ANIM(gDaruniaIdleAnim),
      .walkAnim = ANIM(gDaruniaDancingLoop1Anim),
      .runAnim = ANIM(gDaruniaDancingLoop2Anim),
      .danceAnim = ANIM(gDaruniaDancingLoop3Anim),
      .segments = { (void*)gDaruniaEyeOpenTex, (void*)gDaruniaMouthGrinningTex, (void*)gDaruniaNoseHappyTex },
      .npcDraw = true,
      .scale = 0.004f },
    { .name = "Running Man",
      .group = "NPCs",
      .skeleton = (void*)gRunningManSkel,
      .isFlex = true,
      .idleAnim = ANIM(gRunningManHappyAnim),
      .walkAnim = ANIM(gRunningManRunAnim),
      .runAnim = ANIM(gRunningManSprintAnim),
      .danceAnim = ANIM(gRunningManExcitedAnim),
      .walkSpeed = 0.6f,
      .segments = { (void*)gRunningManMouthClosedTex },
      .scale = 0.005f },
    { .name = "ReDead",
      .group = "Creatures",
      .skeleton = (void*)gRedeadSkel,
      .isFlex = true,
      .idleAnim = ANIM(gGibdoRedeadIdleAnim),
      .walkAnim = ANIM(gGibdoRedeadWalkAnim),
      .runAnim = ANIM(gGibdoRedeadWalkAnim),
      .runSpeed = 2.0f,
      .segments = { &D_80116280[2] },
      .scale = 0.004f },
    { .name = "Gibdo",
      .group = "Creatures",
      .skeleton = (void*)gGibdoSkel,
      .isFlex = true,
      .idleAnim = ANIM(gGibdoRedeadIdleAnim),
      .walkAnim = ANIM(gGibdoRedeadWalkAnim),
      .runAnim = ANIM(gGibdoRedeadWalkAnim),
      .runSpeed = 2.0f,
      .segments = { &D_80116280[2] },
      .scale = 0.004f },
    // No idle animation, so it freezes mid-step
    { .name = "Stalchild",
      .group = "Creatures",
      .skeleton = (void*)gStalchildSkel,
      .idleAnim = ANIM(gStalchildWalkingAnim),
      .walkAnim = ANIM(gStalchildWalkingAnim),
      .runAnim = ANIM(gStalchildWalkingAnim),
      .idleSpeed = 0.0f,
      .runSpeed = 2.0f,
      .envColor = { 255, 255, 255, 255 },
      .scale = 0.006f },
    { .name = "Deku Scrub",
      .group = "Creatures",
      .skeleton = (void*)gDekuNutsSkel,
      .idleAnim = ANIM(gDekuNutsStandAnim),
      .walkAnim = ANIM(gDekuNutsRunAnim),
      .runAnim = ANIM(gDekuNutsRunAnim),
      .walkSpeed = 0.6f,
      .scale = 0.01f },
    // Only has the one animation, so it glides along stiffly
    { .name = "Cucco",
      .group = "Creatures",
      .skeleton = (void*)gCuccoSkel,
      .isFlex = true,
      .idleAnim = ANIM(gCuccoAnim),
      .walkAnim = ANIM(gCuccoAnim),
      .runAnim = ANIM(gCuccoAnim),
      .scale = 0.01f },
    { .name = "Skull Kid",
      .group = "NPCs",
      .skeleton = (void*)gSkullKidSkel,
      .isFlex = true,
      .idleAnim = ANIM(gSkullKidWaitAnim),
      .walkAnim = ANIM(gSkullKidWalkToPlayerAnim),
      .runAnim = ANIM(gSkullKidWalkToPlayerAnim),
      .danceAnim = ANIM(gSkullKidPlayFluteAnim),
      .runSpeed = 2.0f,
      .npcDraw = true,
      .scale = 0.006f },
    { .name = "Saria",
      .group = "NPCs",
      .skeleton = (void*)gSariaSkel,
      .isFlex = true,
      .idleAnim = ANIM(gSariaHandsBehindBackWaitAnim),
      .walkAnim = ANIM(gSariaRunAnim),
      .runAnim = ANIM(gSariaRunAnim),
      .danceAnim = ANIM(gSariaPlayingOcarinaAnim),
      .walkSpeed = 0.6f,
      .segments = { (void*)gSariaEyeOpenTex, (void*)gSariaEyeOpenTex, (void*)gSariaMouthSmilingOpenTex },
      .npcDraw = true,
      .scale = 0.006f },
    { .name = "Mido",
      .group = "NPCs",
      .skeleton = (void*)gMidoSkel,
      .isFlex = true,
      .idleAnim = ANIM(gMidoHandsOnHipsIdleAnim),
      .walkAnim = ANIM(gMidoWalkingAnim),
      .runAnim = ANIM(gMidoWalkingAnim),
      .danceAnim = ANIM(gMidoRaiseHand1Anim),
      .runSpeed = 2.0f,
      .segments = { (void*)gMidoEyeOpenTex },
      .npcDraw = true,
      .scale = 0.006f },
    { .name = "Poe",
      .group = "Creatures",
      .skeleton = (void*)gPoeSkel,
      .idleAnim = ANIM(gPoeFloatAnim),
      .walkAnim = ANIM(gPoeFloatAnim),
      .runAnim = ANIM(gPoeFloatAnim),
      .segments = { &D_80116280[2] },
      .envColor = { 255, 255, 210, 255 },
      .scale = 0.006f,
      .hover = 20.0f },
    { .name = "Keese",
      .group = "Creatures",
      .skeleton = (void*)gKeeseSkeleton,
      .idleAnim = ANIM(gKeeseFlyAnim),
      .walkAnim = ANIM(gKeeseFlyAnim),
      .runAnim = ANIM(gKeeseFlyAnim),
      .runSpeed = 1.5f,
      .scale = 0.005f,
      .hover = 25.0f },
    // Armos only hop, so it stands still between hops
    { .name = "Armos",
      .group = "Creatures",
      .skeleton = (void*)gArmosSkel,
      .idleAnim = ANIM(gArmosHopAnim),
      .walkAnim = ANIM(gArmosHopAnim),
      .runAnim = ANIM(gArmosHopAnim),
      .idleSpeed = 0.0f,
      .runSpeed = 2.0f,
      .scale = 0.005f },
    Prop("Sign", DrawSign, gSignRectangularDL, 0.005f),
    Prop("Pot", DrawPot, gPotDL, 0.06f),
    Prop("Crate", DrawCrate, gSmallWoodenBoxDL, 0.05f),
    Prop("Bomb", DrawBomb, gBombBodyDL, 0.01f),
    Prop("Bombchu", DrawBombchu, gBombchuDL, 0.015f),
    Prop("Gossip Stone", DrawGossipStone, gGossipStoneDL, 0.03f),
    Prop("Beehive", DrawBeehive, gFieldBeehiveDL, 0.05f),
    Prop("Silver Rock", DrawSilverRock, gSilverRockDL, 0.04f),
    Item("Ocarina of Time", GID_OCARINA_TIME),
    Item("Biggoron's Sword", GID_SWORD_BGS),
    Item("Megaton Hammer", GID_HAMMER),
    Item("Heart Container", GID_HEART_CONTAINER),
    Item("Gold Skulltula Token", GID_SKULL_TOKEN),
    Item("Giant Gold Rupee", GID_RUPEE_GOLD),
    Item("Bunny Hood", GID_MASK_BUNNY),
    Item("Mask of Truth", GID_MASK_TRUTH),
    Item("Cojiro", GID_COJIRO),
    Item("Big Poe", GID_BIG_POE),
    Item("Bottled Fish", GID_FISH),
    Item("Hover Boots", GID_BOOTS_HOVER),
    Item("Weird Egg", GID_EGG),
    { .name = "Wallmaster",
      .group = "Creatures",
      .skeleton = (void*)gWallmasterSkel,
      .isFlex = true,
      .idleAnim = ANIM(gWallmasterWaitAnim),
      .walkAnim = ANIM(gWallmasterWalkAnim),
      .runAnim = ANIM(gWallmasterWalkAnim),
      .runSpeed = 2.0f,
      .scale = 0.004f },
    // Octoroks only bob, so it drifts along on land
    { .name = "Octorok",
      .group = "Creatures",
      .skeleton = (void*)gOctorokSkel,
      .idleAnim = ANIM(gOctorokFloatAnim),
      .walkAnim = ANIM(gOctorokFloatAnim),
      .runAnim = ANIM(gOctorokFloatAnim),
      .runSpeed = 2.0f,
      .scale = 0.006f },
    { .name = "Kaepora Gaebora",
      .group = "NPCs",
      .skeleton = (void*)gOwlFlyingSkel,
      .isFlex = true,
      .idleAnim = ANIM(gOwlGlideAnim),
      .walkAnim = ANIM(gOwlFlyAnim),
      .runAnim = ANIM(gOwlFlyAnim),
      .runSpeed = 1.5f,
      .segments = { (void*)gObjOwlEyeOpenTex },
      .scale = 0.008f,
      .hover = 30.0f },
    { .name = "Dampé",
      .group = "NPCs",
      .skeleton = (void*)gDampeSkel,
      .isFlex = true,
      .idleAnim = ANIM(gDampeRestAnim),
      .walkAnim = ANIM(gDampeWalkAnim),
      .runAnim = ANIM(gDampeWalkAnim),
      .danceAnim = ANIM(gDampeDigAnim),
      .runSpeed = 2.0f,
      .segments = { (void*)gDampeEyeOpenTex },
      .scale = 0.006f },
    // Has no walk, so His Majesty slides along mid-idle
    { .name = "King Zora",
      .group = "NPCs",
      .skeleton = (void*)gKzSkel,
      .isFlex = true,
      .idleAnim = ANIM(gKzIdleAnim),
      .walkAnim = ANIM(gKzIdleAnim),
      .runAnim = ANIM(gKzIdleAnim),
      .danceAnim = ANIM(gKzMweepAnim),
      .segments = { (void*)gKzEyeOpenTex },
      .scale = 0.003f },
    { .name = "Man on the Roof",
      .group = "NPCs",
      .skeleton = (void*)gRoofManSkel,
      .isFlex = true,
      .idleAnim = ANIM(gRoofManIdleAnim),
      .walkAnim = ANIM(gRoofManIdleAnim),
      .runAnim = ANIM(gRoofManIdleAnim),
      .segments = { (void*)gRoofManEyeOpenTex },
      .scale = 0.006f },
    // Backstrokes across dry land to keep up
    { .name = "Ruto",
      .group = "NPCs",
      .skeleton = (void*)gRutoChildSkel,
      .isFlex = true,
      .idleAnim = ANIM(gRutoChildWaitHandsOnHipsAnim),
      .walkAnim = ANIM(gRutoChildSwimOnBackAnim),
      .runAnim = ANIM(gRutoChildSwimOnBackAnim),
      .danceAnim = ANIM(gRutoChildSquirmAnim),
      .runSpeed = 2.0f,
      .segments = { (void*)gRutoChildEyeOpenTex, (void*)gRutoChildMouthClosedTex },
      .npcDraw = true,
      .scale = 0.006f },
    { .name = "Hyrule Guard",
      .group = "NPCs",
      .skeleton = (void*)gEnHeishiSkel,
      .idleAnim = ANIM(gEnHeishiIdleAnim),
      .walkAnim = ANIM(gEnHeishiWalkAnim),
      .runAnim = ANIM(gEnHeishiWalkAnim),
      .runSpeed = 2.0f,
      .scale = 0.005f },
    // Dances when it has to hurry
    { .name = "Scrub Elder",
      .group = "NPCs",
      .skeleton = (void*)gDntJijiSkel,
      .idleAnim = ANIM(gDntJijiWaitAnim),
      .walkAnim = ANIM(gDntJijiWalkAnim),
      .runAnim = ANIM(gDntJijiDanceAnim),
      .danceAnim = ANIM(gDntJijiDanceAnim),
      .segments = { (void*)gDntJijiEyeOpenTex },
      .scale = 0.01f },
    Item("Kokiri Sword", GID_SWORD_KOKIRI),
    Item("Broken Giant's Knife", GID_SWORD_BROKEN),
    Item("Deku Shield", GID_SHIELD_DEKU),
    Item("Hylian Shield", GID_SHIELD_HYLIAN),
    Item("Mirror Shield", GID_SHIELD_MIRROR),
    Item("Fairy Slingshot", GID_SLINGSHOT),
    Item("Boomerang", GID_BOOMERANG),
    Item("Hookshot", GID_HOOKSHOT),
    Item("Lens of Truth", GID_LENS),
    Item("Din's Fire", GID_DINS_FIRE),
    Item("Keaton Mask", GID_MASK_KEATON),
    Item("Skull Mask", GID_MASK_SKULL),
    Item("Spooky Mask", GID_MASK_SPOOKY),
    Item("Goron Mask", GID_MASK_GORON),
    Item("Zora Mask", GID_MASK_ZORA),
    Item("Gerudo Mask", GID_MASK_GERUDO),
    Item("Pocket Cucco", GID_CHICKEN),
    Item("Odd Mushroom", GID_ODD_MUSHROOM),
    Item("Poacher's Saw", GID_SAW),
    Item("Eyeball Frog", GID_FROG),
    Item("Bottled Fairy", GID_FAIRY),
    Item("Blue Fire", GID_BLUE_FIRE),
    Item("Bottled Bug", GID_BUG),
    Item("Ruto's Letter", GID_LETTER_RUTO),
    Item("Magic Bean", GID_BEAN),
    Item("Goron's Ruby", GID_GORON_RUBY),
    Item("Zora's Sapphire", GID_ZORA_SAPPHIRE),
    Item("Light Medallion", GID_MEDALLION_LIGHT),
    Item("Boss Key", GID_KEY_BOSS),
    Item("Gerudo Membership Card", GID_GERUDO_CARD),
    Cutout("Lab Octopus", hylia_labo_sceneTex_003C90, 32, 64),
    Cutout("Lab Crab", hylia_labo_sceneTex_003490, 32, 32),
    Cutout("Lab Shark", hylia_labo_sceneTex_001490, 64, 32),
    Cutout("Lab Seashell", hylia_labo_sceneTex_00C890, 32, 32),
    // Borrows the credits party walk, and hops when it hurries
    { .name = "Strutting Cucco",
      .group = "Creatures",
      .skeleton = (void*)gCuccoSkel,
      .isFlex = true,
      .idleAnim = ANIM(gCuccoAnim),
      .walkAnim = ANIM(gDemoEcWalkingCuccoAnim),
      .runAnim = ANIM(gDemoEcJumpingCuccoAnim),
      .danceAnim = ANIM(gDemoEcJumpingCucco2Anim),
      .scale = 0.01f },
    { .name = "Gohma",
      .group = "Creatures",
      .skeleton = (void*)gGohmaSkel,
      .idleAnim = ANIM(gGohmaStandAnim),
      .walkAnim = ANIM(gGohmaWalkAnim),
      .runAnim = ANIM(gGohmaWalkAnim),
      .danceAnim = ANIM(gGohmaEyeRollAnim),
      .runSpeed = 2.0f,
      .noCull = true,
      .scale = 0.003f },
    { .name = "Gohma Larva",
      .group = "Creatures",
      .skeleton = (void*)gObjectGolSkel,
      .idleAnim = ANIM(gObjectGolStandAnim),
      .walkAnim = ANIM(gObjectGolWalkAnim),
      .runAnim = ANIM(gObjectGolRunningAnim),
      .noCull = true,
      .scale = 0.006f },
    // Laughs at you whenever you stop
    { .name = "Stalfos",
      .group = "Creatures",
      .skeleton = (void*)gStalfosSkel,
      .idleAnim = ANIM(gStalfosLaughAnim),
      .walkAnim = ANIM(gStalfosSlowAdvanceAnim),
      .runAnim = ANIM(gStalfosFastAdvanceAnim),
      .danceAnim = ANIM(gStalfosJumpAnim),
      .scale = 0.004f },
    { .name = "Moblin",
      .group = "Creatures",
      .skeleton = (void*)gEnMbSpearSkel,
      .isFlex = true,
      .idleAnim = ANIM(gEnMbSpearStandStillAnim),
      .walkAnim = ANIM(gEnMbSpearWalkAnim),
      .runAnim = ANIM(gEnMbSpearWalkAnim),
      .runSpeed = 2.0f,
      .scale = 0.003f },
    // Just the little flame core, always running in place
    { .name = "Flare Dancer",
      .group = "Creatures",
      .skeleton = (void*)gFlareDancerCoreSkel,
      .isFlex = true,
      .idleAnim = ANIM(gFlareDancerCoreRunCycleAnim),
      .walkAnim = ANIM(gFlareDancerCoreRunCycleAnim),
      .runAnim = ANIM(gFlareDancerCoreRunCycleAnim),
      .idleSpeed = 0.3f,
      .scale = 0.006f },
    { .name = "Peahat",
      .group = "Creatures",
      .skeleton = (void*)gPeehatSkel,
      .idleAnim = ANIM(gPeehatFlyingAnim),
      .walkAnim = ANIM(gPeehatFlyingAnim),
      .runAnim = ANIM(gPeehatFlyingAnim),
      .scale = 0.004f,
      .hover = 25.0f },
    { .name = "Bird",
      .group = "Creatures",
      .skeleton = (void*)gBirdSkel,
      .idleAnim = ANIM(gBirdFlyAnim),
      .walkAnim = ANIM(gBirdFlyAnim),
      .runAnim = ANIM(gBirdFlyAnim),
      .runSpeed = 1.5f,
      .scale = 0.01f,
      .hover = 30.0f },
    { .name = "Stinger",
      .group = "Creatures",
      .skeleton = (void*)gStingerSkel,
      .idleAnim = ANIM(gStingerIdleAnim),
      .walkAnim = ANIM(gStingerIdleAnim),
      .runAnim = ANIM(gStingerIdleAnim),
      .segments = { &D_80116280[2] },
      .envColor = { 255, 255, 255, 255 },
      .scale = 0.006f,
      .hover = 15.0f },
    { .name = "Composer Poe",
      .group = "Creatures",
      .skeleton = (void*)gPoeComposerSkel,
      .isFlex = true,
      .idleAnim = ANIM(gPoeComposerFloatAnim),
      .walkAnim = ANIM(gPoeComposerFloatAnim),
      .runAnim = ANIM(gPoeComposerFloatAnim),
      .npcDraw = true,
      .envSegments = { { 0x08, { 255, 255, 255, 255 } },
                       { 0x0A, { 75, 20, 25, 255 } },
                       { 0x0B, { 90, 85, 50, 255 } } },
      .scale = 0.006f,
      .hover = 20.0f },
    { .name = "Poe Sister",
      .group = "Creatures",
      .skeleton = (void*)gPoeSistersSkel,
      .idleAnim = ANIM(gPoeSistersFloatAnim),
      .walkAnim = ANIM(gPoeSistersFloatAnim),
      .runAnim = ANIM(gPoeSistersFloatAnim),
      .segments = { nullptr, &D_80116280[2] },
      .envColor = { 255, 255, 255, 255 },
      .scale = 0.006f,
      .hover = 20.0f },
    // Lounges on her side and floats after you
    { .name = "Great Fairy",
      .group = "NPCs",
      .skeleton = (void*)gGreatFairySkel,
      .isFlex = true,
      .idleAnim = ANIM(gGreatFairyLayingSidewaysAnim),
      .walkAnim = ANIM(gGreatFairyLayingSidewaysAnim),
      .runAnim = ANIM(gGreatFairyLayingSidewaysAnim),
      .danceAnim = ANIM(gGreatFairyGivingUpgradeAnim),
      .segments = { (void*)gGreatFairyEyeOpenTex, (void*)gGreatFairyEyeOpenTex, (void*)gGreatFairyMouthClosedTex },
      .scale = 0.004f,
      .hover = 20.0f },
    // A tiny Ganon, without his swords
    { .name = "Ganon",
      .group = "NPCs",
      .skeleton = (void*)gGanonSkel,
      .isFlex = true,
      .idleAnim = ANIM(gGanonGuardIdleAnim),
      .walkAnim = ANIM(gGanonWalkAnim),
      .runAnim = ANIM(gGanonWalkAnim),
      .runSpeed = 2.0f,
      .segments = { (void*)gGanonEyeOpenTex },
      .scale = 0.002f },
    { .name = "Ganondorf",
      .group = "NPCs",
      .skeleton = (void*)gYoungGanondorfSkel,
      .isFlex = true,
      .idleAnim = ANIM(gYoungGanondorfWalkAnim),
      .walkAnim = ANIM(gYoungGanondorfWalkAnim),
      .runAnim = ANIM(gYoungGanondorfWalkAnim),
      .idleSpeed = 0.0f,
      .runSpeed = 2.0f,
      .segments = { (void*)gYoungGanondorfEyeOpenTex, (void*)gYoungGanondorfEyeOpenTex },
      .npcDraw = true,
      .scale = 0.005f },
    { .name = "Sheik",
      .group = "NPCs",
      .skeleton = (void*)gSheikSkel,
      .isFlex = true,
      .idleAnim = ANIM(gSheikIdleAnim),
      .walkAnim = ANIM(gSheikWalkingAnim),
      .runAnim = ANIM(gSheikWalkingAnim),
      .runSpeed = 2.0f,
      .segments = { (void*)gSheikEyeOpenTex, (void*)gSheikEyeOpenTex },
      .scale = 0.006f },
    { .name = "Kotake",
      .group = "NPCs",
      .skeleton = (void*)gKotakeSkel,
      .isFlex = true,
      .idleAnim = ANIM(gKotakeKoumeFlyAnim),
      .walkAnim = ANIM(gKotakeKoumeFlyAnim),
      .runAnim = ANIM(gKotakeKoumeFlyAnim),
      .segments = { (void*)gKotakeKoumeEyeOpenTex },
      .scale = 0.006f,
      .hover = 25.0f },
    { .name = "Koume",
      .group = "NPCs",
      .skeleton = (void*)gKoumeSkel,
      .isFlex = true,
      .idleAnim = ANIM(gKotakeKoumeFlyAnim),
      .walkAnim = ANIM(gKotakeKoumeFlyAnim),
      .runAnim = ANIM(gKotakeKoumeFlyAnim),
      .segments = { (void*)gKotakeKoumeEyeOpenTex },
      .scale = 0.006f,
      .hover = 25.0f },
    { .name = "Gerudo Guard",
      .group = "NPCs",
      .skeleton = (void*)gGerudoRedSkel,
      .isFlex = true,
      .idleAnim = ANIM(gGerudoRedStandAnim),
      .walkAnim = ANIM(gGerudoRedWalkAnim),
      .runAnim = ANIM(gGerudoRedWalkAnim),
      .danceAnim = ANIM(gGerudoRedJumpAnim),
      .runSpeed = 2.0f,
      .segments = { (void*)gGerudoRedEyeOpenTex },
      .scale = 0.006f },
    { .name = "Gerudo Patrol",
      .group = "NPCs",
      .skeleton = (void*)gGerudoPurpleSkel,
      .isFlex = true,
      .idleAnim = ANIM(gGerudoPurpleWalkingAnim),
      .walkAnim = ANIM(gGerudoPurpleWalkingAnim),
      .runAnim = ANIM(gGerudoPurpleWalkingAnim),
      .idleSpeed = 0.0f,
      .runSpeed = 2.0f,
      .segments = { (void*)gGerudoPurpleEyeOpenTex },
      .scale = 0.006f },
    { .name = "Graveyard Kid",
      .group = "NPCs",
      .skeleton = (void*)gGraveyardKidSkel,
      .isFlex = true,
      .idleAnim = ANIM(gGraveyardKidIdleAnim),
      .walkAnim = ANIM(gGraveyardKidWalkAnim),
      .runAnim = ANIM(gGraveyardKidWalkAnim),
      .runSpeed = 2.0f,
      .segments = { (void*)gGraveyardKidEyesOpenTex },
      .scale = 0.006f },
    { .name = "Cucco Chaser",
      .group = "NPCs",
      .skeleton = (void*)gNiwGirlSkel,
      .isFlex = true,
      .idleAnim = ANIM(gNiwGirlRunAnim),
      .walkAnim = ANIM(gNiwGirlRunAnim),
      .runAnim = ANIM(gNiwGirlRunAnim),
      .danceAnim = ANIM(gNiwGirlJumpAnim),
      .idleSpeed = 0.0f,
      .segments = { (void*)gNiwGirlEyeOpenTex },
      .scale = 0.006f },
    // Breaks into a dance to catch up
    { .name = "Stage Scrub",
      .group = "NPCs",
      .skeleton = (void*)gDntStageSkel,
      .idleAnim = ANIM(gDntStageWaitAnim),
      .walkAnim = ANIM(gDntStageWalkAnim),
      .runAnim = ANIM(gDntStageDanceAnim),
      .danceAnim = ANIM(gDntStageDanceAnim),
      .segments = { (void*)gDntStageEyeOpenTex },
      .envColor = { 210, 255, 0, 255 },
      .scale = 0.01f },
    // Slides along mid-pitch
    { .name = "Carpet Merchant",
      .group = "NPCs",
      .skeleton = (void*)gCarpetMerchantSkel,
      .isFlex = true,
      .idleAnim = ANIM(gCarpetMerchantIdleAnim),
      .walkAnim = ANIM(gCarpetMerchantIdleAnim),
      .runAnim = ANIM(gCarpetMerchantIdleAnim),
      .scale = 0.006f },
    { .name = "Poe Collector",
      .group = "NPCs",
      .skeleton = (void*)gPoeSellerSkel,
      .isFlex = true,
      .idleAnim = ANIM(gPoeSellerIdleAnim),
      .walkAnim = ANIM(gPoeSellerIdleAnim),
      .runAnim = ANIM(gPoeSellerIdleAnim),
      .envColor = { 255, 255, 255, 255 },
      .scale = 0.006f },
    // The credits party crowd, dancing their way after you
    { .name = "Dancing Kokiri Boy",
      .group = "Party",
      .skeleton = (void*)gKm1Skel,
      .isFlex = true,
      .idleAnim = ANIM(gDemoEcKokiriAnim),
      .walkAnim = ANIM(gDemoEcDancingKokiriAnim),
      .runAnim = ANIM(gDemoEcDancingKokiriAnim),
      .danceAnim = ANIM(gDemoEcDancingKokiriAnim),
      .npcDraw = true,
      .clothes = true,
      .clothesColor = { 0, 130, 70 },
      .scale = 0.006f },
    { .name = "Dancing Kokiri Girl",
      .group = "Party",
      .skeleton = (void*)gKw1Skel,
      .isFlex = true,
      .idleAnim = ANIM(gDemoEcKokiriAnim),
      .walkAnim = ANIM(gDemoEcDancingKokiriAnim),
      .runAnim = ANIM(gDemoEcDancingKokiriAnim),
      .danceAnim = ANIM(gDemoEcDancingKokiriAnim),
      .segments = { nullptr, nullptr, (void*)gKw1EyeOpenTex },
      .npcDraw = true,
      .clothes = true,
      .clothesColor = { 70, 190, 60 },
      .scale = 0.006f },
    { .name = "Dancing Zora",
      .group = "Party",
      .skeleton = (void*)gZoraSkel,
      .isFlex = true,
      .idleAnim = ANIM(gZoraIdleAnim),
      .walkAnim = ANIM(gDemoEcDancingZoraAnim),
      .runAnim = ANIM(gDemoEcDancingZoraAnim),
      .danceAnim = ANIM(gDemoEcDancingZoraAnim),
      .segments = { (void*)gZoraEyeOpenTex, (void*)gZoraEyeOpenTex },
      .npcDraw = true,
      .scale = 0.006f },
    { .name = "Windmill Man (Guru-Guru)",
      .group = "Party",
      .skeleton = (void*)gWindmillManSkel,
      .isFlex = true,
      .idleAnim = ANIM(gDemoEcWindmillManAnim),
      .walkAnim = ANIM(gDemoEcWindmillManAnim),
      .runAnim = ANIM(gDemoEcWindmillManAnim),
      .danceAnim = ANIM(gDemoEcWindmillManAnim),
      .segments = { (void*)gWindmillManEyeClosedTex, (void*)gWindmillManMouthAngryTex },
      .npcDraw = true,
      .scale = 0.006f },
    { .name = "Partying Ingo",
      .group = "Party",
      .skeleton = (void*)gIngoSkel,
      .isFlex = true,
      .idleAnim = ANIM(gDemoEcIngoAnim),
      .walkAnim = ANIM(gDemoEcIngoAnim),
      .runAnim = ANIM(gDemoEcIngoAnim),
      .danceAnim = ANIM(gDemoEcIngoAnim),
      .segments = { (void*)gIngoEyeOpenTex, (void*)gIngoRedTex },
      .npcDraw = true,
      .scale = 0.006f },
    { .name = "Anju",
      .group = "Party",
      .skeleton = (void*)gCuccoLadySkel,
      .isFlex = true,
      .idleAnim = ANIM(gDemoEcCuccoLadyAnim),
      .walkAnim = ANIM(gDemoEcCuccoLadyAnim),
      .runAnim = ANIM(gDemoEcCuccoLadyAnim),
      .danceAnim = ANIM(gDemoEcCuccoLadyAnim),
      .segments = { (void*)gCuccoLadyEyeOpenTex, (void*)gCuccoLadyEyeOpenTex },
      .npcDraw = true,
      .scale = 0.006f },
    { .name = "Potion Shop Owner",
      .group = "Party",
      .skeleton = (void*)object_ds2_Skel_004258,
      .isFlex = true,
      .idleAnim = ANIM(gDemoEcPotionShopOwnerAnim),
      .walkAnim = ANIM(gDemoEcPotionShopOwnerAnim),
      .runAnim = ANIM(gDemoEcPotionShopOwnerAnim),
      .danceAnim = ANIM(gDemoEcPotionShopOwnerAnim),
      .segments = { (void*)gPotionShopkeeperEyeOpenTex, (void*)gPotionShopkeeperEyeOpenTex },
      .npcDraw = true,
      .scale = 0.006f },
    { .name = "Happy Mask Salesman",
      .group = "Party",
      .skeleton = (void*)gHappyMaskSalesmanSkel,
      .isFlex = true,
      .idleAnim = ANIM(gHappyMaskSalesmanIdleAnim),
      .walkAnim = ANIM(gDemoEcPotionShopOwnerAnim),
      .runAnim = ANIM(gDemoEcPotionShopOwnerAnim),
      .danceAnim = ANIM(gDemoEcPotionShopOwnerAnim),
      .segments = { (void*)gHappyMaskSalesmanEyeOpenTex, (void*)gHappyMaskSalesmanEyeOpenTex },
      .npcDraw = true,
      .scale = 0.006f },
    { .name = "Fishing Pond Owner",
      .group = "Party",
      .skeleton = (void*)gFishingOwnerSkel,
      .isFlex = true,
      .idleAnim = ANIM(gDemoEcPotionShopOwnerAnim),
      .walkAnim = ANIM(gDemoEcPotionShopOwnerAnim),
      .runAnim = ANIM(gDemoEcPotionShopOwnerAnim),
      .danceAnim = ANIM(gDemoEcPotionShopOwnerAnim),
      .segments = { (void*)gFishingOwnerEyeOpenTex, (void*)gFishingOwnerEyeOpenTex },
      .npcDraw = true,
      .scale = 0.006f },
    { .name = "Bombchu Shopkeeper",
      .group = "Party",
      .skeleton = (void*)gBombchuShopkeeperSkel,
      .isFlex = true,
      .idleAnim = ANIM(gBombchuShopkeeperIdleAnim),
      .walkAnim = ANIM(gDemoEcPotionShopOwnerAnim),
      .runAnim = ANIM(gDemoEcPotionShopOwnerAnim),
      .danceAnim = ANIM(gDemoEcPotionShopOwnerAnim),
      .segments = { (void*)gBombchuShopkeeperEyeOpenTex, (void*)gBombchuShopkeeperEyeOpenTex },
      .npcDraw = true,
      .scale = 0.006f },
    { .name = "Goron",
      .group = "Party",
      .skeleton = (void*)gGoronSkel,
      .isFlex = true,
      .idleAnim = ANIM(gDemoEcGoronAnim),
      .walkAnim = ANIM(gDemoEcGoron2Anim),
      .runAnim = ANIM(gDemoEcGoron2Anim),
      .danceAnim = ANIM(gDemoEcGoron2Anim),
      .segments = { (void*)gGoronCsEyeOpenTex, (void*)gGoronCsMouthNeutralTex },
      .npcDraw = true,
      .scale = 0.005f },
    { .name = "Adult Malon",
      .group = "Party",
      .skeleton = (void*)gMalonAdultSkel,
      .isFlex = true,
      .idleAnim = ANIM(gMalonAdultIdleAnim),
      .walkAnim = ANIM(gMalonAdultSingAnim),
      .runAnim = ANIM(gMalonAdultSingAnim),
      .danceAnim = ANIM(gMalonAdultSingAnim),
      .segments = { (void*)gMalonAdultEyeOpenTex, (void*)gMalonAdultMouthHappyTex },
      .npcDraw = true,
      .scale = 0.006f },
    { .name = "Partying Gerudo",
      .group = "Party",
      .skeleton = (void*)gGerudoWhiteSkel,
      .isFlex = true,
      .idleAnim = ANIM(gDemoEcGerudoAnim),
      .walkAnim = ANIM(gDemoEcGerudoAnim),
      .runAnim = ANIM(gDemoEcGerudoAnim),
      .danceAnim = ANIM(gDemoEcGerudoAnim),
      .segments = { (void*)gGerudoWhiteEyeOpenTex, (void*)gGerudoWhiteEyeOpenTex },
      .npcDraw = true,
      .scale = 0.006f },
    // The moon from the night sky, tinted the way the sky draws it
    Oddity("The Moon", gMoonDL, 0.3f, 30.0f, { 240, 255, 180, 255 }, { 80, 70, 20, 255 }),
    Oddity("Milk Crate", gLonLonMilkCrateDL, 0.04f),
    Oddity("Goron Vase", gGoronCityVaseDL, 0.04f),
    Oddity("Gravestone", gGravestoneStoneDL, 0.03f),
    Oddity("Bush", gFieldBushDL, 0.15f),
    Oddity("Wooden Torch", gWoodenTorchDL, 0.4f),
    Oddity("Golden Torch", gGoldenTorchDL, 0.4f),
    Oddity("Bongo Drum", gBongoDrumDL, 0.003f),
    Oddity("Amy's Painting", gPoSistersAmyPaintingDL, 0.3f),
    Oddity("Bazaar Sign", gKakarikoBazaarSignDL, 0.04f),
    Oddity("Gohma Egg", gObjectGolEggDL, 0.01f),
    Oddity("Dampé's Lantern", gDampeLanternDL, 0.01f, 10.0f, { 255, 255, 255, 255 }, { 240, 200, 150, 128 }),
    Oddity("Fishing Hat", gFishingOwnerHatDL, 0.01f),
    { .name = "Epona",
      .group = "Horses",
      .skeleton = (void*)gEponaSkel,
      .isSkin = true,
      .idleAnim = ANIM(gEponaIdleAnim),
      .walkAnim = ANIM(gEponaWalkingAnim),
      .runAnim = ANIM(gEponaGallopingAnim),
      .danceAnim = ANIM(gEponaRearingAnim),
      .segments = { (void*)gEponaEyeOpenTex },
      .scale = 0.004f },
    { .name = "Young Epona",
      .group = "Horses",
      .skeleton = (void*)gChildEponaSkel,
      .isSkin = true,
      .idleAnim = ANIM(gChildEponaIdleAnim),
      .walkAnim = ANIM(gChildEponaWalkingAnim),
      .runAnim = ANIM(gChildEponaGallopingAnim),
      .danceAnim = ANIM(gChildEponaWhinnyAnim),
      .segments = { (void*)gChildEponaEyeOpenTex },
      .scale = 0.004f },
    { .name = "Ganondorf's Horse",
      .group = "Horses",
      .skeleton = (void*)gHorseGanonSkel,
      .isSkin = true,
      .idleAnim = ANIM(gHorseGanonIdleAnim),
      .walkAnim = ANIM(gHorseGanonWalkingAnim),
      .runAnim = ANIM(gHorseGanonGallopingAnim),
      .danceAnim = ANIM(gHorseGanonRearingAnim),
      .segments = { (void*)gHorseGanonEyeTex },
      .scale = 0.004f },
    { .name = "Lon Lon Horse",
      .group = "Horses",
      .skeleton = (void*)gHorseNormalSkel,
      .isSkin = true,
      .idleAnim = ANIM(gHorseNormalIdleAnim),
      .walkAnim = ANIM(gHorseNormalWalkingAnim),
      .runAnim = ANIM(gHorseNormalGallopingAnim),
      .danceAnim = ANIM(gHorseNormalRearingAnim),
      .segments = { (void*)gHorseNormalEyeTex },
      .scale = 0.004f },
    { .name = "Ingo's Horse",
      .group = "Horses",
      .skeleton = (void*)gHorseIngoSkel,
      .isSkin = true,
      .idleAnim = ANIM(gHorseIngoIdleAnim),
      .walkAnim = ANIM(gHorseIngoWalkingAnim),
      .runAnim = ANIM(gHorseIngoGallopingAnim),
      .danceAnim = ANIM(gHorseIngoRearingAnim),
      .segments = { (void*)gHorseIngoEyeTex },
      .scale = 0.004f },
    // Phantom Ganon's horse, minus Phantom Ganon
    { .name = "Phantom Horse",
      .group = "Horses",
      .skeleton = (void*)gPhantomHorseSkel,
      .isSkin = true,
      .idleAnim = ANIM(gPhantomHorseIdleAnim),
      .walkAnim = ANIM(gPhantomHorseRunningAnim),
      .runAnim = ANIM(gPhantomHorseRunningAnim),
      .danceAnim = ANIM(gPhantomHorseRearingAnim),
      .scale = 0.004f },
    // Only gallops, so it freezes mid-stride when you stop
    { .name = "Zelda's Horse",
      .group = "Horses",
      .skeleton = (void*)gHorseZeldaSkel,
      .isSkin = true,
      .idleAnim = ANIM(gHorseZeldaGallopingAnim),
      .walkAnim = ANIM(gHorseZeldaGallopingAnim),
      .runAnim = ANIM(gHorseZeldaGallopingAnim),
      .idleSpeed = 0.0f,
      .segments = { (void*)gHorseZeldaEyeTex },
      .scale = 0.004f },
};

const std::map<int32_t, const char*> followerModelNames = [] {
    std::map<int32_t, const char*> names;
    for (int32_t i = 0; i < (int32_t)ARRAY_COUNT(sModels); i++) {
        names[i] = sModels[i].name;
    }
    return names;
}();

const char* FollowerModelGroup(int32_t id) {
    return (id >= 0 && id < (int32_t)ARRAY_COUNT(sModels)) ? sModels[id].group : "";
}

// The leader is the dog the game itself spawns and keeps track of (the one Richard's quest knows about). Party members
// are extra dogs spawned alongside it, steered here instead of by their own follow logic. Each dog gets a slot that
// holds its swapped model, if it wears one: the leader in slot 0 and party member i in slot i + 1.
struct FollowerSlot {
    Actor* owner;
    const FollowerModelInfo* model; // nullptr for a plain dog
    bool hasSkeleton;
    SkelAnime skelAnime;
    bool hasSkin;
    Skin skin;
    AnimationHeader* curAnim;
    s16 hopPhase;
    s32 partyIndex; // -1 for the leader
    s16 yaw;        // Party members' heading, kept here so the dog's own steering can't fight it
    bool dancing;
    s16 dancePhase;
};

static FollowerSlot sSlots[1 + FOLLOWER_PARTY_MAX];
static Actor* sLeader;
static s32 sPartyCount;
// While a party member is spawning, its place in line, so its init knows which slot it takes
static s32 sSpawningPartyIndex = -1;
// The slot being drawn, for the draw callbacks that only get the actor
static const FollowerSlot* sDrawingSlot;
// Frames left in the party's celebration after Link gets an item
static s32 sCelebrateTimer;
static constexpr s32 CELEBRATE_FRAMES = 50;

static const FollowerModelInfo* CurrentModel() {
    return sDrawingSlot->model;
}

static bool IsFollowerDog(Actor* actor) {
    return actor->params & 0x8000;
}

static FollowerSlot* FindSlot(Actor* actor) {
    for (FollowerSlot& slot : sSlots) {
        if (slot.owner == actor) {
            return &slot;
        }
    }
    return nullptr;
}

std::vector<int32_t> FollowerModel_GetParty() {
    std::vector<int32_t> party;
    std::stringstream stream(CVarGetString(CVAR_FOLLOWER_PARTY_NAME, ""));
    std::string item;
    while (party.size() < FOLLOWER_PARTY_MAX && std::getline(stream, item, ',')) {
        char* end;
        const long id = strtol(item.c_str(), &end, 10);
        if (end != item.c_str() && id >= 0 && id < (long)ARRAY_COUNT(sModels)) {
            party.push_back((int32_t)id);
        }
    }
    return party;
}

void FollowerModel_SetParty(const std::vector<int32_t>& party) {
    std::string value;
    for (size_t i = 0; i < party.size() && i < FOLLOWER_PARTY_MAX; i++) {
        if (party[i] >= 0 && party[i] < (int32_t)ARRAY_COUNT(sModels)) {
            value += (value.empty() ? "" : ",") + std::to_string(party[i]);
        }
    }
    CVarSetString(CVAR_FOLLOWER_PARTY_NAME, value.c_str());
}

// Pointers that aren't OTR paths (e.g. D_80116280) are in-memory data and always available
static bool AssetExists(const void* asset) {
    if (asset == nullptr) {
        return false;
    }
    if (!ResourceMgr_OTRSigCheck((char*)asset)) {
        return true;
    }
    return ResourceMgr_FileExists((const char*)asset);
}

// Skin and regular skeletons read their limbs differently, so the resource's limb type has to match the model
static bool SkeletonKindMatches(const FollowerModelInfo* model) {
    std::string path = (const char*)model->skeleton;
    if (path.starts_with("__OTR__")) {
        path = path.substr(7);
    }
    auto skeleton = std::dynamic_pointer_cast<SOH::Skeleton>(ResourceMgr_GetResourceByNameHandlingMQ(path.c_str()));
    if (skeleton == nullptr) {
        return false;
    }
    const bool isSkin = skeleton->limbType == SOH::LimbType::Skin || skeleton->limbTableType == SOH::LimbType::Skin;
    return isSkin == model->isSkin;
}

static bool IsModelUsable(const FollowerModelInfo* model) {
    if (model->getItemDrawId >= 0) {
        return model->getItemDrawId < GID_MAXIMUM;
    }

    if (model->drawProp != nullptr) {
        for (const void* asset : model->assets) {
            if (asset != nullptr && !AssetExists(asset)) {
                return false;
            }
        }
        return true;
    }

    if (!AssetExists(model->skeleton)) {
        return false;
    }
    if (!SkeletonKindMatches(model)) {
        return false;
    }
    SkeletonHeader* header = ResourceMgr_LoadSkeletonByName((const char*)model->skeleton, nullptr);
    if (header == nullptr ||
        (!model->isSkin && header->skeletonType != (model->isFlex ? SKELANIME_TYPE_FLEX : SKELANIME_TYPE_NORMAL))) {
        return false;
    }

    for (AnimationHeader* anim : { model->idleAnim, model->walkAnim, model->runAnim, model->danceAnim }) {
        if (anim == model->danceAnim && anim == nullptr) {
            continue; // Optional
        }
        if (!AssetExists(anim) || ResourceMgr_LoadAnimByName((const char*)anim) == nullptr) {
            return false;
        }
    }

    for (void* segment : model->segments) {
        if (segment != nullptr && !AssetExists(segment)) {
            return false;
        }
    }

    return true;
}

// Frees the slot's skeleton, or when the play state is already going away (and the arena with it), only drops the
// skeleton registration
static void ReleaseSlot(FollowerSlot* slot, bool freeMemory) {
    if (slot->hasSkeleton) {
        if (freeMemory) {
            SkelAnime_Free(&slot->skelAnime, gPlayState);
        } else {
            ResourceMgr_UnregisterSkeleton(&slot->skelAnime);
        }
    }
    if (slot->hasSkin && freeMemory) {
        Skin_Free(gPlayState, &slot->skin);
    }
    if (slot->owner == sLeader) {
        sLeader = nullptr;
    }
    *slot = {};
    slot->partyIndex = -1;
}

static s32 FollowerModel_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                          void* thisx) {
    if (limbIndex >= 0 && limbIndex < 32 && (sDrawingSlot->model->hiddenLimbs & LIMB(limbIndex))) {
        *dList = NULL;
    }
    return false;
}

static void DrawSkeleton(PlayState* play, Actor* actor, FollowerSlot* slot) {
    const FollowerModelInfo* model = slot->model;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    if (model->noCull) {
        Gfx* dList = (Gfx*)Graph_Alloc(play->state.gfxCtx, 2 * sizeof(Gfx));
        Gfx* gfx = dList;
        gSPClearGeometryMode(gfx++, G_CULL_BACK);
        gSPEndDisplayList(gfx++);
        gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)dList);
    }
    if (model->clothes) {
        Gfx* dList = (Gfx*)Graph_Alloc(play->state.gfxCtx, 2 * sizeof(Gfx));
        Gfx* gfx = dList;
        gDPSetEnvColor(gfx++, model->clothesColor.r, model->clothesColor.g, model->clothesColor.b, 255);
        gSPEndDisplayList(gfx++);
        gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)dList);
        gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)dList);
    }
    for (const auto& envSegment : model->envSegments) {
        if (envSegment.segment != 0) {
            gSPSegment(POLY_OPA_DISP++, envSegment.segment,
                       (uintptr_t)Gfx_EnvColor(play->state.gfxCtx, envSegment.color.r, envSegment.color.g,
                                               envSegment.color.b, envSegment.color.a));
        }
    }
    for (int i = 0; i < ARRAY_COUNT(model->segments); i++) {
        if (model->segments[i] != nullptr) {
            gSPSegment(POLY_OPA_DISP++, 0x08 + i, (uintptr_t)SEGMENTED_TO_VIRTUAL(model->segments[i]));
        }
    }

    if (model->npcDraw) {
        func_80034BA0(play, &slot->skelAnime, NULL, NULL, actor, 255);
    } else {
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetEnvColor(POLY_OPA_DISP++, model->envColor.r, model->envColor.g, model->envColor.b, model->envColor.a);
        SkelAnime_DrawSkeletonOpa(play, &slot->skelAnime, FollowerModel_OverrideLimbDraw, NULL, actor);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

static void DrawSkin(PlayState* play, Actor* actor, FollowerSlot* slot) {
    const FollowerModelInfo* model = slot->model;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    for (int i = 0; i < ARRAY_COUNT(model->segments); i++) {
        if (model->segments[i] != nullptr) {
            gSPSegment(POLY_OPA_DISP++, 0x08 + i, (uintptr_t)SEGMENTED_TO_VIRTUAL(model->segments[i]));
        }
    }
    CLOSE_DISPS(play->state.gfxCtx);

    func_800A6330(actor, play, &slot->skin, NULL, true);
}

static void FollowerModel_Draw(Actor* actor, PlayState* play) {
    FollowerSlot* slot = FindSlot(actor);
    if (slot == nullptr || slot->model == nullptr) {
        return;
    }

    const FollowerModelInfo* model = slot->model;
    sDrawingSlot = slot;

    SetSafeSegments(play);

    // Celebrating models without a dance animation bounce instead. Offsets are in model space, hence the scale.
    if (slot->dancing && model->danceAnim == nullptr && !slot->hasSkin) {
        Matrix_Translate(0.0f, fabsf(Math_SinS(slot->dancePhase)) * 12.0f / actor->scale.y, 0.0f, MTXMODE_APPLY);
    }

    if (slot->hasSkin) {
        DrawSkin(play, actor, slot);
    } else if (slot->hasSkeleton) {
        DrawSkeleton(play, actor, slot);
    } else {
        // Props and items have no legs, so they hop while moving
        f32 hop = (actor->speedXZ > 0.2f) ? fabsf(Math_SinS(slot->hopPhase)) * 8.0f : 0.0f;
        Matrix_Translate(0.0f, hop / actor->scale.y, 0.0f, MTXMODE_APPLY);

        if (model->getItemDrawId >= 0) {
            // Spins faster while celebrating
            Matrix_RotateY(play->gameplayFrames * (slot->dancing ? 0.25f : 0.05f), MTXMODE_APPLY);
            func_8002EBCC(actor, play, 0);
            func_8002ED80(actor, play, 0);
            GetItem_Draw(play, model->getItemDrawId);
        } else {
            Gfx_SetupDL_25Opa(play->state.gfxCtx);
            model->drawProp(play, actor);
        }
    }

    sDrawingSlot = nullptr;
}

// Dresses a dog in a model, after checking everything the model needs. Anything missing leaves it a plain dog.
static void ApplyModel(FollowerSlot* slot, Actor* actor, int32_t modelId) {
    if (modelId <= 0 || modelId >= ARRAY_COUNT(sModels)) {
        return;
    }

    const FollowerModelInfo* model = &sModels[modelId];
    if (!IsModelUsable(model)) {
        SPDLOG_WARN("Following dog model {} is unavailable, keeping the dog", modelId);
        return;
    }

    if (model->isSkin) {
        slot->skin = {};
        Skin_Init(gPlayState, &slot->skin, (SkeletonHeader*)model->skeleton, model->idleAnim);
        if (slot->skin.vtxTable == nullptr || slot->skin.skelAnime.jointTable == nullptr) {
            SPDLOG_WARN("Following dog model {} couldn't set up its skin, keeping the dog", modelId);
            Skin_Free(gPlayState, &slot->skin);
            return;
        }
        slot->hasSkin = true;
    } else if (model->skeleton != nullptr) {
        slot->skelAnime = {};
        if (model->isFlex) {
            SkelAnime_InitFlex(gPlayState, &slot->skelAnime, (FlexSkeletonHeader*)model->skeleton, model->idleAnim,
                               NULL, NULL, 0);
        } else {
            SkelAnime_Init(gPlayState, &slot->skelAnime, (SkeletonHeader*)model->skeleton, model->idleAnim, NULL,
                           NULL, 0);
        }
        if (slot->skelAnime.jointTable == nullptr || slot->skelAnime.morphTable == nullptr) {
            SPDLOG_WARN("Following dog model {} couldn't allocate its skeleton, keeping the dog", modelId);
            SkelAnime_Free(&slot->skelAnime, gPlayState);
            return;
        }
        slot->hasSkeleton = true;
    }

    slot->model = model;
    slot->curAnim = model->idleAnim;
    slot->hopPhase = 0;
    Actor_SetScale(actor, model->scale);
    actor->shape.yOffset = model->hover / model->scale;
    // The shadow grows with the actor's scale, so shrink it back to the dog's own size
    actor->shape.shadowScale = DOG_SHADOW_SCALE * DOG_SCALE / model->scale;
    actor->draw = FollowerModel_Draw;
}

static void SpawnParty(Actor* leader) {
    const std::vector<int32_t> party = FollowerModel_GetParty();
    sPartyCount = 0;

    for (size_t i = 0; i < party.size(); i++) {
        // Start in a short line behind the leader; steering spreads them into formation
        const s16 behind = leader->shape.rot.y + 0x8000;
        const f32 distance = 20.0f * (i + 1);
        sSpawningPartyIndex = (s32)i;
        Actor* member = Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_EN_DOG,
                                    leader->world.pos.x + Math_SinS(behind) * distance, leader->world.pos.y,
                                    leader->world.pos.z + Math_CosS(behind) * distance, 0, leader->shape.rot.y, 0,
                                    leader->params | 0x8000);
        sSpawningPartyIndex = -1;
        if (member != nullptr) {
            member->room = leader->room;
            sPartyCount = (s32)i + 1;
        }
    }
}

static void OnFollowerInit(void* refActor) {
    Actor* actor = static_cast<Actor*>(refActor);
    if (!IsFollowerDog(actor) || gPlayState == nullptr) {
        return;
    }

    if (sSpawningPartyIndex >= 0) {
        FollowerSlot* slot = &sSlots[1 + sSpawningPartyIndex];
        if (slot->owner != nullptr) {
            ReleaseSlot(slot, true);
        }
        slot->owner = actor;
        slot->partyIndex = sSpawningPartyIndex;
        slot->yaw = actor->shape.rot.y;
        // A whole party shoving each other around would never settle into formation
        reinterpret_cast<EnDog*>(actor)->collider.base.ocFlags1 |= OC1_NO_PUSH;
        // Keep moving while Link plays the ocarina, which freezes most actors, so the party can gather and dance
        actor->flags |= ACTOR_FLAG_UPDATE_DURING_OCARINA;
        const std::vector<int32_t> party = FollowerModel_GetParty();
        if (sSpawningPartyIndex < (s32)party.size()) {
            ApplyModel(slot, actor, party[sSpawningPartyIndex]);
        }
        return;
    }

    // Only one leader at a time. Any other following dog (say, a party member whose init was deferred) stays a plain
    // dog, so a party can never spawn parties of its own.
    if (sLeader != nullptr && sLeader != actor) {
        return;
    }

    FollowerSlot* slot = &sSlots[0];
    if (slot->owner != nullptr) {
        ReleaseSlot(slot, true);
    }
    slot->owner = actor;
    slot->partyIndex = -1;
    sLeader = actor;
    ApplyModel(slot, actor, CVAR_FOLLOWER_MODEL_VALUE);
    SpawnParty(actor);
    // Buddies dance through the ocarina; a plain dog with no party keeps the vanilla freeze
    if (slot->model != nullptr || sPartyCount > 0) {
        actor->flags |= ACTOR_FLAG_UPDATE_DURING_OCARINA;
    }
}

// Mirrors the rest of DogBehavior in z_en_dog.c
// Height of each buddy in the totem pole
static constexpr f32 TOTEM_SPACING = 20.0f;

#define DOG_BARK 2
#define DOG_SIT 3
#define DOG_SIT_2 4

static bool IsJamming() {
    Player* player = GET_PLAYER(gPlayState);
    return player != nullptr && (player->stateFlags2 & PLAYER_STATE2_OCARINA_PLAYING);
}

static void SteerPartyMember(FollowerSlot* slot, EnDog* dog, bool jamming) {
    Actor* actor = &dog->actor;
    Player* player = GET_PLAYER(gPlayState);

    // The party goes home with the leader, such as when the dog is handed back or wanders off
    if (sLeader == nullptr || (gSaveContext.dogParams & 0x7FFF) == 0) {
        ReleaseSlot(slot, true);
        Actor_Kill(actor);
        return;
    }

    // Only the totem pole sways
    actor->shape.rot.z = 0;

    if (!jamming &&
        CVarGetInteger(CVAR_FOLLOWER_FORMATION_NAME, FOLLOWER_FORMATION_CONGA) == FOLLOWER_FORMATION_TOTEM) {
        // Stack on top of the one below (the first on the leader), held in place every frame so gravity never gets a
        // say. Catching up only partway each frame makes the tower lag and sway, more the higher it goes.
        Actor* below = (slot->partyIndex == 0) ? sLeader : sSlots[slot->partyIndex].owner;
        if (below == nullptr) {
            below = sLeader;
        }
        actor->world.pos.x += (below->world.pos.x - actor->world.pos.x) * 0.6f;
        actor->world.pos.z += (below->world.pos.z - actor->world.pos.z) * 0.6f;
        actor->world.pos.y = below->world.pos.y + TOTEM_SPACING;
        actor->velocity.y = 0.0f;
        actor->speedXZ = 0.0f;
        Math_SmoothStepToS(&slot->yaw, below->shape.rot.y, 3, 0x1000, 0x100);
        actor->world.rot.y = slot->yaw;
        actor->shape.rot.y = slot->yaw;
        actor->shape.rot.z = (s16)(Math_SinS(gPlayState->gameplayFrames * 0x500 + slot->partyIndex * 0x1800) *
                                   (0x180 + slot->partyIndex * 0x30));
        if (dog->behavior != DOG_SIT && dog->behavior != DOG_SIT_2) {
            dog->nextBehavior = DOG_SIT;
        }
        return;
    }

    // Celebrating an item: stop right where they are and turn to Link
    if (sCelebrateTimer > 0 && !jamming) {
        actor->speedXZ = 0.0f;
        Math_SmoothStepToS(&slot->yaw, Math_Vec3f_Yaw(&actor->world.pos, &player->actor.world.pos), 3, 0x1800, 0x100);
        actor->world.rot.y = slot->yaw;
        actor->shape.rot.y = slot->yaw;
        return;
    }

    const bool followsEverywhere = CVarGetInteger(CVAR_ENHANCEMENT("DogFollowsEverywhere"), 0);
    const s32 count = MAX(sPartyCount, 1);
    Vec3f target;
    s16 idleYaw;
    f32 stopDistance;

    if (jamming) {
        // The ocarina gathers everyone into a ring around Link, whatever the formation. The ring holds still and each
        // member runs around it to its spot, rather than cutting across Link.
        Vec3f center = player->actor.world.pos;
        const f32 radius = 45.0f + count * 4.0f;
        const s16 spot = (s16)(0x10000 * slot->partyIndex / count);
        const s16 current = Math_Vec3f_Yaw(&center, &actor->world.pos);
        const s16 remaining = spot - current; // Wraps to the shorter way around

        s16 angle;
        f32 waypointRadius;
        if (ABS(remaining) < 0x600) {
            // Nearly there: settle into the spot itself
            angle = spot;
            waypointRadius = radius;
        } else {
            // A waypoint a little further around the ring, spiralling in from wherever they are
            angle = current + CLAMP(remaining, -0x1400, 0x1400);
            const f32 currentRadius = Math_Vec3f_DistXZ(&center, &actor->world.pos);
            waypointRadius = MAX(radius, currentRadius * 0.6f);
        }
        target = center;
        target.x += Math_SinS(angle) * waypointRadius;
        target.z += Math_CosS(angle) * waypointRadius;
        idleYaw = Math_Vec3f_Yaw(&actor->world.pos, &center);
        stopDistance = 6.0f;
    } else if (CVarGetInteger(CVAR_FOLLOWER_FORMATION_NAME, FOLLOWER_FORMATION_CONGA) == FOLLOWER_FORMATION_CIRCLE) {
        // An evenly spaced ring around Link that slowly turns
        const f32 radius = 45.0f + count * 4.0f;
        const s16 angle = (s16)(0x10000 * slot->partyIndex / count + gPlayState->gameplayFrames * 0x80);
        target = player->actor.world.pos;
        target.x += Math_SinS(angle) * radius;
        target.z += Math_CosS(angle) * radius;
        idleYaw = Math_Vec3f_Yaw(&actor->world.pos, &player->actor.world.pos);
        stopDistance = 6.0f;
    } else {
        // Each member trails the one ahead of it; the first trails the leader
        Actor* ahead = (slot->partyIndex == 0) ? sLeader : sSlots[slot->partyIndex].owner;
        if (ahead == nullptr) {
            ahead = &player->actor;
        }
        target = ahead->world.pos;
        idleYaw = Math_Vec3f_Yaw(&actor->world.pos, &ahead->world.pos);
        // Without Dogs Follow You Everywhere, a dog over 400 units from Link stops following and takes the leader
        // with it, so the whole line has to fit inside that
        const f32 gap = CLAMP(CVarGetFloat(CVAR_FOLLOWER_CONGA_GAP_NAME, FOLLOWER_CONGA_GAP_DEFAULT),
                              FOLLOWER_CONGA_GAP_MIN, FOLLOWER_CONGA_GAP_MAX);
        stopDistance = followsEverywhere ? gap : MIN(gap, 320.0f / (count + 1));
    }

    const f32 dx = target.x - actor->world.pos.x;
    const f32 dz = target.z - actor->world.pos.z;
    f32 distance = sqrtf(SQ(dx) + SQ(dz));

    // Stuck or left far behind (a ledge, a sudden warp): pop back into place rather than drop out of the party
    if (distance > 400.0f || (!followsEverywhere && actor->xzDistToPlayer > 330.0f)) {
        actor->world.pos = target;
        actor->prevPos = target;
        distance = 0.0f;
    }

    s16 yaw;
    if (distance <= stopDistance) {
        actor->speedXZ = 0.0f;
        yaw = idleYaw;
        if (dog->behavior != DOG_SIT && dog->behavior != DOG_SIT_2) {
            dog->nextBehavior = DOG_SIT;
        }
    } else {
        actor->speedXZ = CLAMP((distance - stopDistance) * 0.2f, 1.0f, 12.0f);
        yaw = Math_Vec3f_Yaw(&actor->world.pos, &target);
        dog->nextBehavior = (actor->speedXZ > 3.0f) ? DOG_RUN : DOG_WALK;
    }

    Math_SmoothStepToS(&slot->yaw, yaw, 3, 0x1800, 0x100);
    actor->world.rot.y = slot->yaw;
    actor->shape.rot.y = slot->yaw;
}

static void AnimateModel(FollowerSlot* slot, EnDog* dog) {
    const FollowerModelInfo* model = slot->model;
    if (model == nullptr) {
        return;
    }

    if (!slot->hasSkeleton && !slot->hasSkin) {
        slot->hopPhase += (s16)(dog->actor.speedXZ * 0x700);
        return;
    }
    SkelAnime* skelAnime = slot->hasSkin ? &slot->skin.skelAnime : &slot->skelAnime;

    AnimationHeader* anim = model->idleAnim;
    f32 playSpeed = model->idleSpeed;

    if (slot->dancing && model->danceAnim != nullptr) {
        anim = model->danceAnim;
        playSpeed = 1.0f;
    } else if (dog->behavior == DOG_WALK) {
        anim = model->walkAnim;
        playSpeed = model->walkSpeed;
    } else if (dog->behavior == DOG_RUN) {
        anim = model->runAnim;
        playSpeed = model->runSpeed;
    }

    if (anim != slot->curAnim) {
        Animation_Change(skelAnime, anim, playSpeed, 0.0f, Animation_GetLastFrame(anim), ANIMMODE_LOOP, -6.0f);
        slot->curAnim = anim;
    }
    skelAnime->playSpeed = playSpeed;
    SkelAnime_Update(skelAnime);
}

static void OnFollowerUpdate(void* refActor) {
    FollowerSlot* slot = FindSlot(static_cast<Actor*>(refActor));
    if (slot == nullptr) {
        return;
    }

    EnDog* dog = static_cast<EnDog*>(refActor);
    Actor* actor = &dog->actor;
    const bool jamming = IsJamming();
    const bool celebrating = sCelebrateTimer > 0 || jamming;

    if (slot->partyIndex >= 0) {
        SteerPartyMember(slot, dog, jamming);
        if (slot->owner == nullptr) {
            return; // Sent home
        }
    } else if (celebrating) {
        // The leader stays put and turns to Link
        actor->speedXZ = 0.0f;
        actor->world.rot.y = actor->yawTowardsPlayer;
        actor->shape.rot.y = actor->yawTowardsPlayer;
    }

    // Dance once in place; party members still finding their spot in the ring keep going
    slot->dancing = celebrating && actor->speedXZ == 0.0f;
    if (slot->dancing) {
        slot->dancePhase += 0x1400;
        // A plain dog barks along; a dog wearing a model stays quiet under it
        if (slot->model == nullptr) {
            dog->nextBehavior = DOG_BARK;
        } else if (dog->behavior != DOG_SIT && dog->behavior != DOG_SIT_2) {
            dog->nextBehavior = DOG_SIT;
        }
    }
    AnimateModel(slot, dog);
}

static void OnFollowerDestroy(void* refActor) {
    if (FollowerSlot* slot = FindSlot(static_cast<Actor*>(refActor))) {
        ReleaseSlot(slot, true);
    }
}

// Safety net in case a follower wasn't destroyed normally: the arena is torn down with the play state, so only drop
// skeleton registrations and forget the stale actors rather than freeing anything.
static void OnFollowerPlayDestroy() {
    for (FollowerSlot& slot : sSlots) {
        if (slot.owner != nullptr) {
            ReleaseSlot(&slot, false);
        }
    }
    sLeader = nullptr;
    sPartyCount = 0;
}

// #region Summon and dismiss

// A dog with no path that isn't Richard (type 1), for summoning before any dog has followed Link this session
static constexpr s16 GENERIC_DOG_PARAMS = 0x0FF0;
static s16 sLastDogParams = 0;

static Actor* FindFollowerDog() {
    for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head; actor != nullptr; actor = actor->next) {
        if (actor->id == ACTOR_EN_DOG && IsFollowerDog(actor) && actor->update != nullptr) {
            return actor;
        }
    }
    return nullptr;
}

static const char* CheckCanControl() {
    if (!GameInteractor::IsSaveLoaded() || gPlayState == nullptr || GET_PLAYER(gPlayState) == nullptr) {
        return "Load a save first";
    }
    return nullptr;
}

// Sends every following dog away, letting go of their models right away so replacements spawned before the old dogs
// are destroyed can take the slots
static void KillFollowerDogs() {
    for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head; actor != nullptr; actor = actor->next) {
        if (actor->id == ACTOR_EN_DOG && IsFollowerDog(actor) && actor->update != nullptr) {
            if (FollowerSlot* slot = FindSlot(actor)) {
                ReleaseSlot(slot, true);
            }
            Actor_Kill(actor);
        }
    }
    sLeader = nullptr;
    sPartyCount = 0;
}

bool FollowerModel_IsFollowing() {
    return CheckCanControl() == nullptr && FindFollowerDog() != nullptr;
}

const char* FollowerModel_Summon() {
    if (const char* error = CheckCanControl()) {
        return error;
    }
    // Same rule Player uses when respawning the dog: outside the Market it needs Dogs Follow You Everywhere
    if (!CVarGetInteger(CVAR_ENHANCEMENT("DogFollowsEverywhere"), 0) &&
        Object_GetIndex(&gPlayState->objectCtx, OBJECT_DOG) < 0) {
        return "Turn on Dogs Follow You Everywhere to summon here";
    }

    KillFollowerDogs();

    s16 params = gSaveContext.dogParams & 0x7FFF;
    if (params == 0) {
        params = (sLastDogParams != 0) ? sLastDogParams : GENERIC_DOG_PARAMS;
    }
    // Player spawns the leader on its next update, the same as after a scene change, and the party comes with it
    gSaveContext.dogParams = (s16)(params | 0x8000);
    return nullptr;
}

const char* FollowerModel_Dismiss() {
    if (const char* error = CheckCanControl()) {
        return error;
    }

    if ((gSaveContext.dogParams & 0x7FFF) != 0) {
        sLastDogParams = gSaveContext.dogParams & 0x7FFF;
    }
    // Same as the dog wandering off: nothing follows Link until it's summoned again
    gSaveContext.dogParams = 0;
    KillFollowerDogs();
    return nullptr;
}

// #endregion

static void OnFollowerItemReceive(GetItemEntry itemEntry) {
    sCelebrateTimer = CELEBRATE_FRAMES;
}

static void OnFollowerPlayerUpdate() {
    if (sCelebrateTimer > 0) {
        sCelebrateTimer--;
    }
}

static void RegisterFollowerModel() {
    // Always registered: they only look at following dogs, and slots are only taken when there's a model or a party
    COND_ID_HOOK(OnActorInit, ACTOR_EN_DOG, true, OnFollowerInit);
    COND_ID_HOOK(OnActorUpdate, ACTOR_EN_DOG, true, OnFollowerUpdate);
    COND_ID_HOOK(OnActorDestroy, ACTOR_EN_DOG, true, OnFollowerDestroy);
    COND_HOOK(OnPlayDestroy, true, OnFollowerPlayDestroy);
    COND_HOOK(OnItemReceive, true, OnFollowerItemReceive);
    COND_HOOK(OnPlayerUpdate, true, OnFollowerPlayerUpdate);
}

static RegisterShipInitFunc initFunc(RegisterFollowerModel);
