#pragma once
#include "Common.h"
#include <string>

// Number of outfit looks (must match Game.cpp kOutfitNames[])
#ifndef kLookCount
#define kLookCount 12
#endif

// Display names used by the locker UI (Game.cpp references kLooks[i].name)
struct LookInfo {
    const char* name;
    const char* id;   // internal id / file stem
};
extern const LookInfo kLooks[kLookCount];

// Pose state passed into CharacterDraw each frame
struct Pose {
    float phase   = 0;     // walk cycle
    float moveAmt = 0;     // 0 idle .. 1 running
    float swing   = -1;    // pickaxe swing timer (<0 = idle)
    float pitch   = 0;     // look pitch (radians)
    float idle    = 0;
    bool  airborne = false;
    bool  pick    = false; // holding pickaxe
    bool  gun     = false; // holding a gun
    bool  ads     = false; // aiming down sights
    Color gunTint = WHITE;
    float gunScale = 1.0f;
    int   weapon  = 0;     // ItemId for held gun model
};

// AirPose lives in Skydive.h — forward declare only (avoids C2011 redefinition)
struct AirPose;

// One character look: either a loaded GLB/GLTF Model, or empty (procedural fallback)
struct CharacterModel {
    Model  model{};
    bool   loaded = false;
    float  scale  = 1.0f;   // world scale applied when drawing
    Vector3 offset{0, 0, 0}; // extra translation (e.g. feet to origin)
    // Optional: first animation clip if the GLB has any
    ModelAnimation* anims = nullptr;
    int animCount = 0;
    int animFrame = 0;
};

// Held items (guns / pickaxe) – still procedural or separate models
struct HeldModels {
    Model guns[16]{};   // index by ItemId
    Model pick{};
    bool  ready = false;
};

// ---- API used by Game.cpp ----
void CharacterBuild(CharacterModel& c, int lookIndex);
void CharacterUnload(CharacterModel& c);
void CharacterDraw(const CharacterModel& c, const HeldModels& held,
                   Vector3 pos, float yaw, const Pose& p, Color tint);
Matrix CharacterDrawAir(const CharacterModel& c, Vector3 pos, float yaw,
                        const AirPose& p, Color tint);

void HeldBuild(HeldModels& h);
void HeldUnload(HeldModels& h);

// Load a single GLB/GLTF from disk (searches assets/models/, assets/characters/, cwd)
bool LoadCharacterGLB(CharacterModel& c, const char* filename, float scale = 1.0f);
