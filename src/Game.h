#pragma once
#include "Actor.h"
#include "Character.h"
#include "Hud.h"
#include "VFX.h"
#include <deque>

struct Grenade { Vector3 p, v; float t; int owner; };
struct Fx { Vector3 p; float t, life, r; };
struct Hit3 { int kind = 0; int idx = -1; Vector3 p{}; float d = 0; bool head = false; };   // kind: 0 none, 1 solid, 2 piece, 3 actor, 4 ground
enum class State { Lobby, Match, Over };

class Game {
public:
    Game();
    ~Game();
    void Run();
private:
    // lobby
    void UpdateLobby(float dt);
    void DrawLobby();
    void StartMatch();
    void EndMatch();
    // match
    void UpdateMatch(float dt);
    void DrawMatch();
    void RenderView(RenderTexture2D& rt, int W, int H, int actorIdx);
    void UpdateActor(int i, const Input& in, float dt);
    void BotThink(int i, float dt, Input& in);
    Input ReadInput(int device, float dt);
    void ComputeCamera(Actor& a);
    Hit3 TraceRay(Ray r, float range, int ignore);
    void FireWeapon(int i, Slot& s, const ItemDef& d);
    void Harvest(int i);
    void Explode(Vector3 p, float radius, float dmg, int owner);
    void Damage(int victim, float dmg, int attacker);
    void Eliminate(int victim, int attacker);
    void PickUp(Actor& a, int dropIdx);
    bool AddToInventory(Actor& a, const LootRoll& r, bool allowSwap);
    void OpenChest(int ci);
    void UpdateStorm(float dt);
    void Toast(Actor& a, const std::string& s) { a.toast = s; a.toastT = 2.0f; }
    int AliveCount() const;

    State state = State::Lobby;
    bool quit = false;
    MapId map = MAP_DUSTY;
    int p1Look = 0, p2Look = 1; bool p2Joined = false; int humans = 1;
    CharacterModel chars[kLookCount];
    HeldModels held;
    Model padOn{}, padOff{};
    RenderTexture2D rtFull{}, rtHalf[2]{};
    World world;
    std::vector<Actor> actors;
    std::vector<Grenade> grenades;
    std::vector<Fx> fx;
    std::vector<FeedMsg> feed;
    float matchT = 0, stormR = 85, stormT = 80; int stormPhase = 0; bool shrinking = false;
    bool victory = false; int humanPlacement = 0;
};