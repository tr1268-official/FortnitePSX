#pragma once
// Battle bus, gliders and the per-player drop state (bus -> freefall -> glide -> landed).
#include "Common.h"
#include "Character.h"

// ---------------------------------------------------------------------------------------- tuning
namespace Sky {
    constexpr float kCrossTime   = 14.0f;   // seconds the bus spends over the map
    constexpr float kMinBusSpeed = 8.0f;    // world units / second
    constexpr float kAutoDeployAGL = 52.0f; // humans: glider opens by itself below this height above ground
}

constexpr int kGliderCount = 3;             // 0 Default, 1 Mako, 2 Founder's  (same order as Game's equippedGlider)

enum class DropPhase { Landed = 0, Bus, Freefall, Glide };

// Per-actor drop state. Game keeps one per actor, same index as Game::actors.
struct AirState {
    DropPhase phase = DropPhase::Landed;
    int   glider = 0;
    float t = 0;                 // seconds in the current phase
    float open = 0;              // glider deploy animation 0..1
    float fold = 0;              // >0 while the glider folds away after landing (1 -> 0)
    float tilt = 1.4f;           // visual body pitch (radians, ~1.4 = belly-down skydive)
    float roll = 0;              // visual bank
    float prevYaw = 0;
    bool  jumpPrev = true;       // edge detection for the jump / deploy button
    float jumpAt = 0.5f;         // bots: bus progress (0..1) at which they jump
    float deployAGL = 52.0f;     // height above ground at which the glider opens by itself
    Vector3 hvel{0, 0, 0};       // horizontal velocity while airborne
};

// ---------------------------------------------------------------------------------------- bus flight
struct BattleBus {
    bool    active = false;
    Vector3 pos{0, 0, 0};
    Vector3 dir{0, 0, 1};        // horizontal unit travel direction
    Vector3 start{0, 0, 0}, end{0, 0, 0};
    float   yaw = 0, speed = 0, alt = 0;
    float   travel = 0, length = 0;          // distance flown / total route length
    float   mapEnter = 0, mapLeave = 0;      // travel distances at which the bus enters / leaves the map square
    float   wheelSpin = 0, propSpin = 0, sway = 0, bobT = 0;

    void  Start(float mapHalf, float altitude);   // picks a random straight route across the map
    void  Update(float dt);
    bool  OverMap() const { return travel >= mapEnter; }
    float MapProgress() const;                    // 0 when it reaches the map, 1 when it leaves
    float SecondsToLeave() const { return speed > 0 ? fmaxf(0.0f, (mapLeave - travel) / speed) : 0.0f; }
    bool  Finished() const { return travel >= length; }
    Vector3 Seat(int) const { return Vector3{pos.x, pos.y + 1.6f, pos.z}; }
};

// Read by the HUD (minimap marker, reticle hiding) so Hud.h / HudInfo do not need to change.
struct SkyHud { bool busActive = false, airborne = false, onBus = false; float x = 0, z = 0, dirx = 0, dirz = 1, busSecs = 0; };
extern SkyHud gSkyHud;

// ---------------------------------------------------------------------------------------- models
struct SkyModels {
    Model busBody{}, busWheel{}, busBalloon{}, busFan{};
    Model glider[kGliderCount]{};
};
void SkydiveBuild(SkyModels& m);      // builds the bus and all gliders (call once, after gR.Init())
void SkydiveUnload(SkyModels& m);
void GliderBuild(SkyModels& m);       // (Glider.cpp) called by SkydiveBuild
void GliderUnload(SkyModels& m);

void DrawBattleBus(const SkyModels& m, const BattleBus& b, Color tint);

const char* GliderName(int id);
float GliderGripSplay(int id);        // +outward (bar gliders) / -inward (umbrella): how the hands sit on the grip
// frame = matrix at the grip point (the value CharacterDrawAir returns). open: 0..1.
// unfurl=true plays the deploy "pop" (overshoot); false is a plain linear scale (landing fold).
void DrawGlider(const SkyModels& m, int id, const Matrix& frame, float open, bool unfurl, Color tint);

// ---------------------------------------------------------------------------------------- skydiver pose
struct AirPose {
    float tilt = 1.4f;     // body pitch around the pelvis / grip
    float roll = 0.0f;     // bank
    float blend = 0.0f;    // 0 = freefall (belly-down, limbs spread), 1 = hanging from a glider
    float time = 0.0f;
    float splay = 0.30f;   // see GliderGripSplay
};
// Draws a character skydiving or hanging from a glider. Returns the grip-frame matrix for DrawGlider.
Matrix CharacterDrawAir(const CharacterModel& cm, Vector3 pos, float yaw, const AirPose& p, Color tint);
