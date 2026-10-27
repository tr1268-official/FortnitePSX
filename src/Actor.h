#pragma once
#include "Common.h"
#include "Items.h"
#include "World.h"
#include <string>

struct Input {
    Vector2 move{0, 0};
    Vector2 look{0, 0};
    bool jump = false, fire = false, firePressed = false, reload = false, interact = false, toggleBuild = false, next = false, prev = false, cycleMat = false;
    bool drop = false;
    bool aim = false;
    int slot = -1, piece = -1;
};

struct Actor {
    bool isBot = false; int device = -1; int look = 0; std::string name;
    Vector3 pos{0, 0, 0}, vel{0, 0, 0}; float yaw = 0, pitch = 0;
    float health = 100, shield = 0; bool alive = true; int kills = 0, placement = 0;
    int mats[3] = {0, 0, 0};
    Slot slots[6]; int sel = 0; int ammo[AM_COUNT] = {0};
    bool buildMode = false; PieceType piece = P_WALL; Mat mat = MAT_WOOD; bool grounded = true;
    float phase = 0, moveAmt = 0, swing = -1, fireCd = 0, reloadT = 0, useT = 0, hitFlash = 0, idle = 0, placeCd = 0, stormTick = 0;
    Vector3 camPos{0, 0, 0}, camFwd{0, 0, 1}; Vector3 tA{}, tB{}; float tracerT = 0;
    std::string prompt, toast; float toastT = 0;
    Vector3 goal{0, 0, 0}, lastPos{0, 0, 0}; float thinkT = 0, stuckT = 0, interactT = 0; int enemy = -1;
    bool isAiming = false;
};