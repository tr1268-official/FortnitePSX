#include "Game.h"
#include "Renderer.h"
#include "IconCache.h"
#include "Audio.h"

static const float kWait[4] = {80, 50, 40, 30}, kShrink[4] = {50, 40, 30, 25}, kRad[5] = {85, 52, 32, 16, 4};

static void Present(RenderTexture2D& rt, Rectangle dest) {
    Rectangle src = {0, 0, (float)rt.texture.width, -(float)rt.texture.height};
    DrawTexturePro(rt.texture, src, dest, {0, 0}, 0, WHITE);
}

Game::Game() {
    gR.Init();
    InitAudioDevice();
    gAudio.Init();
    gIcons.Init();
    SetExitKey(KEY_NULL);
    for (int i = 0; i < kLookCount; i++) CharacterBuild(chars[i], i);
    HeldBuild(held);
    world.gunModel = &held.guns[I_AR];
    { MeshBuilder a, b;
        a.Loft({{0, 1.0f, 1.0f, C(150, 150, 155)}, {0.14f, 1.0f, 1.0f, C(190, 190, 195)}, {0.14f, 0.82f, 0.82f, C(90, 235, 255)}}, 8);
        b.Loft({{0, 1.0f, 1.0f, C(150, 150, 155)}, {0.14f, 1.0f, 1.0f, C(190, 190, 195)}, {0.14f, 0.82f, 0.82f, C(55, 62, 68)}}, 8);
        padOn = gR.MakeModel(a); padOff = gR.MakeModel(b);
    }
    rtFull = LoadRenderTexture(320, 240);
    rtHalf[0] = LoadRenderTexture(214, 240);
    rtHalf[1] = LoadRenderTexture(214, 240);
    SetTextureFilter(rtFull.texture, TEXTURE_FILTER_POINT);
    SetTextureFilter(rtHalf[0].texture, TEXTURE_FILTER_POINT);
    SetTextureFilter(rtHalf[1].texture, TEXTURE_FILTER_POINT);
}

Game::~Game() {
    world.Unload();
    for (int i = 0; i < kLookCount; i++) CharacterUnload(chars[i]);
    HeldUnload(held);
    gR.FreeModel(padOn); gR.FreeModel(padOff);
    UnloadRenderTexture(rtFull); UnloadRenderTexture(rtHalf[0]); UnloadRenderTexture(rtHalf[1]);
    gIcons.Shutdown();
    gAudio.Shutdown();
    CloseAudioDevice();
    gR.Shutdown();
}

void Game::Run() {
    while (!WindowShouldClose() && !quit) {
        float dt = fminf(GetFrameTime(), 0.05f);
        if (state == State::Lobby) { UpdateLobby(dt); DrawLobby(); }
        else { UpdateMatch(dt); DrawMatch(); }
    }
}

void Game::UpdateLobby(float) {
    if (IsKeyPressed(KEY_ESCAPE)) quit = true;
    if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT)) p1Look = (p1Look + kLookCount - 1) % kLookCount;
    if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) p1Look = (p1Look + 1) % kLookCount;
    if (IsKeyPressed(KEY_TAB)) map = (MapId)((map + 1) % MAP_COUNT);
    if (IsKeyPressed(KEY_J) || (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT))) p2Joined = !p2Joined;
    if (p2Joined) {
        if (IsKeyPressed(KEY_Q) || (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT))) p2Look = (p2Look + kLookCount - 1) % kLookCount;
        if (IsKeyPressed(KEY_E) || (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT))) p2Look = (p2Look + 1) % kLookCount;
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) StartMatch();
}

void Game::DrawLobby() {
    float t = (float)GetTime();
    Camera3D cam = {};
    cam.position = {0, 1.15f, 6.4f}; cam.target = {0, 0.95f, 0}; cam.up = {0, 1, 0}; cam.fovy = 34; cam.projection = CAMERA_PERSPECTIVE;

    BeginDrawing();
    ClearBackground(BLACK);
    BeginTextureMode(rtFull);
    ClearBackground(C(10, 30, 80));
    DrawRectangleGradientV(0, 0, 320, 148, C(8, 28, 84), C(32, 84, 150));
    for (int gy = 0; gy < 8; gy++)
        for (int gx = 0; gx < 12; gx++) {
            auto P = [&](int x, int y) { float j1 = sinf(x * 12.9f + y * 78.2f) * 9, j2 = cosf(x * 39.3f + y * 11.1f) * 9; return Vector2{x * 28.0f + j1 - 8, y * 22.0f + j2}; };
            Vector2 a = P(gx, gy), b = P(gx + 1, gy), c = P(gx, gy + 1), d = P(gx + 1, gy + 1);
            Color lc = Color{110, 160, 230, 70};
            DrawLineV(a, b, lc); DrawLineV(a, c, lc); DrawLineV(a, d, lc);
        }
    DrawRectangleGradientV(0, 148, 320, 68, C(160, 140, 108), C(120, 102, 80));
    gR.SetResolution(320, 240);
    BeginMode3D(cam);
    float x1 = -1.35f, x2 = 1.35f;
    DrawModelEx(padOn, {x1, 0, 0}, {0, 1, 0}, 0, {1, 1, 1}, WHITE);
    DrawModelEx(p2Joined ? padOn : padOff, {x2, 0, 0}, {0, 1, 0}, 0, {1, 1, 1}, WHITE);
    Pose p1; p1.idle = t; p1.pick = false;
    CharacterDraw(chars[p1Look], held, {x1, 0.14f, 0}, sinf(t * 0.8f) * 0.25f, p1, WHITE);
    Pose p2; p2.idle = t + 1;
    CharacterDraw(chars[p2Look], held, {x2, 0.14f, 0}, sinf(t * 0.8f + 1) * 0.25f, p2, p2Joined ? WHITE : Color{8, 8, 12, 255});
    EndMode3D();
    Vector2 l1 = GetWorldToScreenEx({x1, 2.2f, 0}, cam, 320, 240), l2 = GetWorldToScreenEx({x2, 2.2f, 0}, cam, 320, 240);
    const char* n1 = TextFormat("Player 1 - %s", kLooks[p1Look].name);
    DrawText(n1, (int)l1.x - MeasureText(n1, 10) / 2, (int)l1.y, 10, WHITE);
    const char* n2 = p2Joined ? TextFormat("Player 2 - %s", kLooks[p2Look].name) : "Player 2";
    DrawText(n2, (int)l2.x - MeasureText(n2, 10) / 2, (int)l2.y, 10, p2Joined ? WHITE : Color{200, 200, 220, 255});
    DrawText("FORTNITE", 7, 7, 20, Color{0, 0, 0, 160}); DrawText("FORTNITE", 5, 5, 20, WHITE);
    const char* mn = MapName(map); DrawText(mn, 316 - MeasureText(mn, 20), 7, 20, Color{0, 0, 0, 160}); DrawText(mn, 314 - MeasureText(mn, 20), 5, 20, WHITE);
    DrawRectangle(0, 216, 320, 24, C(10, 20, 50));
    DrawText("A/D select character", 6, 221, 10, WHITE);
    DrawText("J: P2 (pad)   TAB: map", 6, 231, 8, Color{170, 190, 230, 255});
    DrawText("ESC back", 130, 221, 10, WHITE);
    DrawRectangle(214, 214, 102, 24, C(250, 200, 30)); DrawRectangleLines(214, 214, 102, 24, C(255, 240, 150));
    DrawCircle(230, 226, 7, C(40, 70, 180)); DrawText("ENTER", 244, 220, 12, BLACK); DrawText("PLAY", 244, 228, 10, BLACK);
    EndTextureMode();
    int sw = GetScreenWidth(), sh = GetScreenHeight(); float w = sh * 4.0f / 3.0f, h = (float)sh; if (w > sw) { w = (float)sw; h = sw * 0.75f; }
    Present(rtFull, {(sw - w) / 2, (sh - h) / 2, w, h});
    EndDrawing();
}

void Game::StartMatch() {
    world.Build(map);
    const float ms = world.half / 60.0f;      // map scale: storm radii and bot ranges scale with map size
    world.gunModel = &held.guns[I_AR];
    actors.clear(); grenades.clear(); fx.clear(); feed.clear();
    gVFX.Clear();
    humans = p2Joined ? 2 : 1;
    std::vector<Vector3> sp = world.spawns;
    for (int i = (int)sp.size() - 1; i > 0; i--) std::swap(sp[i], sp[RandI(0, i)]);
    for (int i = 0; i < 8; i++) {
        Actor a; a.isBot = i >= humans;
        a.device = (i == 0) ? 0 : (i == 1 && humans == 2 ? 1 : -1);
        a.look = (i == 0) ? p1Look : (i == 1 && humans == 2 ? p2Look : RandI(0, kLookCount - 1));
        a.name = a.isBot ? TextFormat("Bot%d", i) : TextFormat("Player %d", i + 1);
        a.pos = sp[i % sp.size()]; a.yaw = atan2f(-a.pos.x, -a.pos.z);
        a.slots[0].id = I_PICKAXE; a.slots[0].count = 1;
        a.goal = {RandF(-30, 30) * ms, 0, RandF(-30, 30) * ms}; a.thinkT = RandF(0, 0.4f);
        actors.push_back(a);
    }
    stormR = kRad[0] * ms; stormT = kWait[0]; stormPhase = 0; shrinking = false; matchT = 0; victory = false; humanPlacement = 0;
    state = State::Match;
    DisableCursor();
    gAudio.StartAmbience("Wind_Falling/sfx_parachute_skydive_loop_3p");
}

void Game::EndMatch() {
    EnableCursor();
    gAudio.StopAmbience();
    world.Unload();
    actors.clear();
    state = State::Lobby;
}

Input Game::ReadInput(int device, float dt) {
    Input in;
    if (device == 0) {
        in.move.y = (IsKeyDown(KEY_W) ? 1.0f : 0.0f) - (IsKeyDown(KEY_S) ? 1.0f : 0.0f);
        in.move.x = (IsKeyDown(KEY_D) ? 1.0f : 0.0f) - (IsKeyDown(KEY_A) ? 1.0f : 0.0f);
        Vector2 md = GetMouseDelta(); in.look = {md.x * 0.0025f, md.y * 0.0025f};
        in.fire = IsMouseButtonDown(MOUSE_BUTTON_LEFT); in.firePressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        in.aim = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
        in.jump = IsKeyDown(KEY_SPACE); in.reload = IsKeyPressed(KEY_R); in.interact = IsKeyPressed(KEY_E);
        in.toggleBuild = IsKeyPressed(KEY_Q); in.cycleMat = IsKeyPressed(KEY_G);
        in.drop = IsKeyPressed(KEY_X);
        
        float wh = GetMouseWheelMove();
        if (actors[device].buildMode) {
            if (wh > 0) in.piece = ((int)actors[device].piece == 0) ? 3 : (int)actors[device].piece - 1;
            if (wh < 0) in.piece = ((int)actors[device].piece + 1) % 4;
        } else {
            in.next = wh < 0; in.prev = wh > 0;
        }
        for (int i = 0; i < 6; i++) if (IsKeyPressed(KEY_ONE + i)) in.slot = i;
        if (IsKeyPressed(KEY_Z)) in.piece = 0; if (IsKeyPressed(KEY_X)) in.piece = 1; if (IsKeyPressed(KEY_C)) in.piece = 2; if (IsKeyPressed(KEY_V)) in.piece = 3;
    } else {
        int g = device - 1;
        if (!IsGamepadAvailable(g)) return in;
        auto dz = [](float v) { return fabsf(v) < 0.18f ? 0.0f : v; };
        static bool prevFire[4] = {false, false, false, false};
        in.move.x = dz(GetGamepadAxisMovement(g, GAMEPAD_AXIS_LEFT_X)); in.move.y = -dz(GetGamepadAxisMovement(g, GAMEPAD_AXIS_LEFT_Y));
        in.look = {dz(GetGamepadAxisMovement(g, GAMEPAD_AXIS_RIGHT_X)) * 2.6f * dt, dz(GetGamepadAxisMovement(g, GAMEPAD_AXIS_RIGHT_Y)) * 2.0f * dt};
        in.fire = GetGamepadAxisMovement(g, GAMEPAD_AXIS_RIGHT_TRIGGER) > 0.3f || IsGamepadButtonDown(g, GAMEPAD_BUTTON_RIGHT_TRIGGER_2);
        in.aim = GetGamepadAxisMovement(g, GAMEPAD_AXIS_LEFT_TRIGGER) > 0.3f;
        if (g < 4) { in.firePressed = in.fire && !prevFire[g]; prevFire[g] = in.fire; }
        in.jump = IsGamepadButtonDown(g, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
        in.reload = IsGamepadButtonPressed(g, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
        in.interact = IsGamepadButtonPressed(g, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
        in.toggleBuild = IsGamepadButtonPressed(g, GAMEPAD_BUTTON_RIGHT_FACE_UP);
        in.drop = IsGamepadButtonPressed(g, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
        in.next = IsGamepadButtonPressed(g, GAMEPAD_BUTTON_RIGHT_TRIGGER_1); in.prev = IsGamepadButtonPressed(g, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
        if (IsGamepadButtonPressed(g, GAMEPAD_BUTTON_LEFT_FACE_UP)) in.piece = 0; if (IsGamepadButtonPressed(g, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) in.piece = 1;
        if (IsGamepadButtonPressed(g, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) in.piece = 2; if (IsGamepadButtonPressed(g, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) in.piece = 3;
        in.cycleMat = IsGamepadButtonPressed(g, GAMEPAD_BUTTON_RIGHT_THUMB);
    }
    return in;
}

int Game::AliveCount() const { int n = 0; for (const auto& a : actors) if (a.alive) n++; return n; }

void Game::UpdateStorm(float dt) {
    const float ms = world.half / 60.0f;
    if (stormPhase >= 4) { stormR = kRad[4] * ms; return; }
    stormT -= dt;
    if (stormT <= 0) {
        if (!shrinking) { shrinking = true; stormT = kShrink[stormPhase]; gAudio.Play("Shield/SFX_SafeZone_BeginShrinking"); }
        else { shrinking = false; stormPhase++; stormT = stormPhase < 4 ? kWait[stormPhase] : 0; }
    }
    if (stormPhase >= 4) { stormR = kRad[4] * ms; return; }
    stormR = shrinking ? Lerp(kRad[stormPhase + 1] * ms, kRad[stormPhase] * ms, fmaxf(0.0f, stormT) / kShrink[stormPhase]) : kRad[stormPhase] * ms;
}

void Game::UpdateMatch(float dt) {
    if (IsKeyPressed(KEY_ESCAPE)) { EndMatch(); return; }
    if (state == State::Over && IsKeyPressed(KEY_ENTER)) { EndMatch(); return; }
    matchT += dt;
    if (state == State::Match) UpdateStorm(dt);
    for (int i = 0; i < (int)actors.size(); i++) {
        Input in;
        if (state == State::Over && !actors[i].isBot) { UpdateActor(i, Input(), dt); continue; }
        if (!actors[i].isBot) in = ReadInput(actors[i].device, dt); else BotThink(i, dt, in);
        UpdateActor(i, in, dt);
    }
    for (int i = (int)grenades.size() - 1; i >= 0; i--) {
        Grenade& g = grenades[i]; g.t -= dt; g.v.y -= 22 * dt;
        g.p = Vector3Add(g.p, Vector3Scale(g.v, dt));
        float gy = world.GroundY(g.p, g.p.y, 0.1f);
        if (g.p.y < gy + 0.12f) { g.p.y = gy + 0.12f; g.v.y = fabsf(g.v.y) * 0.4f; g.v.x *= 0.6f; g.v.z *= 0.6f; }
        if (g.t <= 0) { Explode(g.p, 4.5f, 100, g.owner); grenades.erase(grenades.begin() + i); }
    }
    for (int i = (int)fx.size() - 1; i >= 0; i--) { fx[i].t += dt; if (fx[i].t > fx[i].life) fx.erase(fx.begin() + i); }
    for (auto& m : feed) m.t -= dt;
    
    Camera3D mainCam = {};
    for (const auto& a : actors) if (!a.isBot && a.alive) { mainCam.position = a.camPos; mainCam.target = Vector3Add(a.camPos, a.camFwd); }
    gVFX.Update(dt, mainCam);
    
    world.Update(dt);
    if (state == State::Match) {
        bool humanAlive = false; for (const auto& a : actors) if (!a.isBot && a.alive) humanAlive = true;
        int alive = AliveCount();
        if (!humanAlive || alive <= 1) {
            victory = humanAlive && alive <= 1;
            for (auto& a : actors) if (!a.isBot) humanPlacement = (humanPlacement == 0) ? a.placement : std::min(humanPlacement, a.placement == 0 ? 99 : a.placement);
            state = State::Over;
            if (victory) gAudio.Play("Character/SFX_LastPlayerKilled");
            else gAudio.Play("Character/SFX_PlayerKilled_UILayer");
        }
    }
}

void Game::ComputeCamera(Actor& a) {
    Vector3 head = {a.pos.x, a.pos.y + 1.55f, a.pos.z};
    Vector3 fwd = Fwd(a.yaw, a.pitch), right = RightOf(a.yaw);
    if (a.isAiming) { a.camPos = Vector3Add(head, Vector3Scale(fwd, 0.25f)); a.camFwd = fwd; return; }   // looking down a scope: eye view
    Vector3 want = Vector3Add(Vector3Subtract(head, Vector3Scale(fwd, 3.0f)), Vector3Add(Vector3Scale(right, 0.9f), Vector3{0, 0.3f, 0}));
    Vector3 d = Vector3Subtract(want, head); float L = Vector3Length(d);
    if (L > 0.01f) {
        RayHit h = world.Raycast(Ray{head, Vector3Scale(d, 1.0f / L)}, L);
        if (h.hit) want = Vector3Add(head, Vector3Scale(d, fmaxf(0.4f, h.dist - 0.3f) / L));
    }
    want.y = fmaxf(want.y, 0.3f);
    a.camPos = want; a.camFwd = fwd;
}

Hit3 Game::TraceRay(Ray r, float range, int ignore) {
    Hit3 best; best.d = range; best.p = Vector3Add(r.position, Vector3Scale(r.direction, range));
    RayHit h = world.Raycast(r, range);
    if (h.hit) { best.kind = h.piece >= 0 ? 2 : (h.solid >= 0 ? 1 : 4); best.idx = h.piece >= 0 ? h.piece : h.solid; best.p = h.point; best.d = h.dist; }
    for (int j = 0; j < (int)actors.size(); j++) {
        if (j == ignore || !actors[j].alive) continue;
        const Actor& o = actors[j];
        BoundingBox bb = {{o.pos.x - 0.35f, o.pos.y, o.pos.z - 0.35f}, {o.pos.x + 0.35f, o.pos.y + 1.8f, o.pos.z + 0.35f}};
        RayCollision c = GetRayCollisionBox(r, bb);
        if (c.hit && c.distance >= 0 && c.distance < best.d) { best.kind = 3; best.idx = j; best.p = c.point; best.d = c.distance; best.head = c.point.y > o.pos.y + 1.5f; }
    }
    return best;
}

void Game::FireWeapon(int i, Slot& s, const ItemDef& d) {
    Actor& a = actors[i];
    Hit3 ah = TraceRay(Ray{a.camPos, a.camFwd}, 300, i);
    Vector3 aim = ah.kind ? ah.p : Vector3Add(a.camPos, Vector3Scale(a.camFwd, 300));
    Vector3 muzzle = Vector3Add(Vector3{a.pos.x, a.pos.y + 1.35f, a.pos.z}, Vector3Scale(Fwd(a.yaw, 0), 0.5f));
    Vector3 base = Vector3Normalize(Vector3Subtract(aim, muzzle));
    
    std::string sndFolder = "Weapons/SMG/sfx_smg_singlefire_close";
    if (s.id == I_PISTOL) sndFolder = "Weapons/Pistol_SixShooter/Sixshooter_Fire_Medium_01";
    else if (s.id == I_PUMP) sndFolder = "Weapons/Scoped_AR/sfx_scopedAR_close";
    else if (s.id == I_TACTICAL) sndFolder = "Weapons/Scoped_AR/sfx_scopedAR_close";
    else if (s.id == I_AR) sndFolder = "Weapons/Striker_Auto/Single_Shot/sfx_striker_singlefire_close";
    else if (s.id == I_SMG) sndFolder = "Weapons/Tactical_SMG/sfx_tacsmg_singlefire_close";
    else if (s.id == I_SNIPER || s.id == I_HUNTING) sndFolder = "Weapons/AMR/sfx_amr_fire_thunder_02";
    
    gAudio.PlayRandomAt(sndFolder, muzzle, a.camPos, 1.0f, RandF(0.95f, 1.05f));
    
    for (int p = 0; p < d.pellets; p++) {
        Vector3 j = {RandF(-1, 1), RandF(-1, 1), RandF(-1, 1)};
        Vector3 dir = Vector3Normalize(Vector3Add(base, Vector3Scale(j, d.spread)));
        Hit3 h = TraceRay(Ray{muzzle, dir}, d.range, i);
        float dmg = d.dmg * RarityMult(s.rar);
        
        if (h.kind == 3) { 
            if (h.head && d.pellets == 1) dmg *= 1.5f; 
            Actor& v = actors[h.idx];
            bool hittingShield = v.shield > 0;
            Damage(h.idx, dmg, i); 
            if (!a.isBot) { 
                if (h.head) { gVFX.SpawnHitMarker(2); gAudio.Play("HitNotifications/sfx_hitmarker_notification_critical_layer", 0.6f); }
                else if (hittingShield) { gVFX.SpawnHitMarker(1); gAudio.Play("HitNotifications/sfx_hitmarker_bighit", 0.4f); }
                else { gVFX.SpawnHitMarker(0); gAudio.Play("HitNotifications/sfx_hitmarker_bighit", 0.4f); }
            }
            gVFX.SpawnDamage(h.p, dmg, h.head);
            gAudio.PlayRandomAt("Impacts/sniper_body_impact_01", h.p, a.camPos, 0.8f);
        }
        else if (h.kind == 2) { 
            Piece& pc = world.pieces[h.idx]; pc.hp -= dmg; if (pc.hp <= 0) pc.alive = false; 
            gVFX.SpawnImpact(h.p, Vector3Negate(dir), kMatColor[pc.mat]);
            gAudio.PlayRandomAt("Weapons/BulletWhips/bullet_whip", h.p, a.camPos, 0.6f);
        }
        else if (h.kind == 1) {
            gVFX.SpawnImpact(h.p, Vector3Negate(dir), WHITE);
            gAudio.PlayRandomAt("Weapons/BulletWhips/bullet_whip", h.p, a.camPos, 0.6f);
        }
        
        if (d.explosive) Explode(h.p, d.radius, dmg, i);
        if (p == 0) { a.tA = muzzle; a.tB = h.p; a.tracerT = 0.06f; }
    }
}

void Game::Harvest(int i) {
    Actor& a = actors[i];
    Hit3 h = TraceRay(Ray{a.camPos, a.camFwd}, 50, i);
    Vector3 head = {a.pos.x, a.pos.y + 1.5f, a.pos.z};
    if (h.kind == 0 || h.kind == 4 || Vector3Distance(head, h.p) > 3.8f) return;
    
    gAudio.PlayRandomAt("Weapons/Fire_Axe_Swing", h.p, a.camPos, 0.8f);
    
    if (h.kind == 1) {
        Solid& s = world.solids[h.idx];
        if (s.kind == SK_CRATE || s.kind == SK_TREE) { 
            s.hp -= 34; a.mats[MAT_WOOD] = std::min(999, a.mats[MAT_WOOD] + 10); 
            if (s.hp <= 0) s.alive = false; 
            gAudio.PlayRandomAt("Weapons/BulletWhips/bullet_whip_heavy", h.p, a.camPos, 0.8f);
        } else {
            gAudio.PlayRandomAt("Weapons/BulletWhips/bullet_whip_heavy", h.p, a.camPos, 0.8f);
        }
    } else if (h.kind == 2) {
        Piece& p = world.pieces[h.idx]; p.hp -= 34; a.mats[p.mat] = std::min(999, a.mats[p.mat] + 10); 
        if (p.hp <= 0) p.alive = false;
        gAudio.PlayRandomAt("Weapons/BulletWhips/bullet_whip_heavy", h.p, a.camPos, 0.8f);
    } else if (h.kind == 3) {
        Damage(h.idx, GetItemDef(I_PICKAXE).dmg, i);
        gAudio.PlayRandomAt("Impacts/sniper_body_impact_01", h.p, a.camPos, 0.8f);
    }
}

void Game::Explode(Vector3 p, float radius, float dmg, int owner) {
    fx.push_back(Fx{p, 0, 0.35f, radius});
    gAudio.PlayRandomAt("Weapons/Smoke_Grenade/sfx_SmokeGrenade_Explode", p, (owner >= 0 ? actors[owner].camPos : Vector3{0,0,0}), 1.5f);
    
    for (int j = 0; j < (int)actors.size(); j++) {
        if (!actors[j].alive) continue;
        Vector3 c = {actors[j].pos.x, actors[j].pos.y + 1.0f, actors[j].pos.z};
        float dist = Vector3Distance(c, p);
        if (dist < radius) Damage(j, dmg * (1.0f - dist / radius), owner);
    }
    for (auto& pc : world.pieces) if (pc.alive && Vector3Distance(pc.pos, p) < radius + 2) { pc.hp -= dmg; if (pc.hp <= 0) pc.alive = false; }
    for (auto& s : world.solids) if (s.alive && (s.kind == SK_CRATE || s.kind == SK_TREE) && Vector3Distance(s.pos, p) < radius) { s.hp -= dmg; if (s.hp <= 0) s.alive = false; }
}

void Game::Damage(int victim, float dmg, int attacker) {
    Actor& v = actors[victim];
    if (!v.alive || dmg <= 0) return;
    v.hitFlash = 0.25f;
    float sh = fminf(v.shield, dmg); v.shield -= sh; dmg -= sh; v.health -= dmg;
    if (v.health <= 0) Eliminate(victim, attacker);
}

void Game::Eliminate(int victim, int attacker) {
    Actor& v = actors[victim];
    v.alive = false; v.health = 0;
    v.placement = AliveCount() + 1;
    for (int k = 1; k < 6; k++) if (!v.slots[k].Empty()) { world.AddDrop(LootRoll{v.slots[k].id, v.slots[k].rar, std::max(1, v.slots[k].count)}, Vector3{v.pos.x + RandF(-1, 1), 0, v.pos.z + RandF(-1, 1)}); v.slots[k] = Slot(); }
    std::string msg = (attacker >= 0 && attacker != victim) ? actors[attacker].name + " eliminated " + v.name : v.name + (attacker == victim ? " eliminated themself" : " was lost to the storm");
    if (attacker >= 0 && attacker != victim) actors[attacker].kills++;
    feed.push_back(FeedMsg{msg, 6.0f});
    if (feed.size() > 5) feed.erase(feed.begin());
    
    gAudio.Play("Character/SFX_PlayerKilled_UILayer", 1.0f);
}

bool Game::AddToInventory(Actor& a, const LootRoll& r, bool allowSwap) {
    const ItemDef& d = GetItemDef(r.id);
    if (d.kind == K_AMMO) { a.ammo[d.ammo] += r.count; return true; }
    if (d.kind == K_WEAPON) {
        int slot = -1; for (int k = 1; k < 6; k++) if (a.slots[k].Empty()) { slot = k; break; }
        if (slot < 0) {
            if (!allowSwap) return false;
            slot = a.sel > 0 ? a.sel : 1;
            Slot old = a.slots[slot];
            world.AddDrop(LootRoll{old.id, old.rar, 1}, Vector3Add(a.pos, Vector3Scale(Fwd(a.yaw, 0), 1.0f)));
        }
        a.slots[slot].id = r.id; a.slots[slot].rar = r.rar; a.slots[slot].mag = d.mag; a.slots[slot].count = 1;
        if (!a.isBot) a.sel = slot;
        return true;
    }
    for (int k = 1; k < 6; k++) if (a.slots[k].id == r.id && a.slots[k].count < d.stack) { a.slots[k].count = std::min(d.stack, a.slots[k].count + r.count); return true; }
    for (int k = 1; k < 6; k++) if (a.slots[k].Empty()) { a.slots[k].id = r.id; a.slots[k].rar = r.rar; a.slots[k].mag = 0; a.slots[k].count = r.count; return true; }
    return false;
}

void Game::PickUp(Actor& a, int di) {
    Drop& d = world.drops[di];
    if (AddToInventory(a, d.item, !a.isBot)) {
        d.alive = false;
        gAudio.PlayRandomAt("Items/Fort_Bandage_Use_Athena_01", d.pos, a.camPos, 0.6f);
    }
    else if (!a.isBot) Toast(a, "Inventory full");
}

void Game::OpenChest(int ci) {
    Chest& c = world.chests[ci]; c.opened = true;
    gAudio.PlayRandomAt("Items/Fort_Medkit_Use_Athena", c.pos, actors[0].camPos, 1.0f);
    for (int n = 0; n < 3; n++) {
        LootRoll r = RollLoot(true);
        if (n == 0) for (int t = 0; t < 8 && GetItemDef(r.id).kind != K_WEAPON; t++) r = RollLoot(true);
        world.AddDrop(r, Vector3{c.pos.x + RandF(-1.3f, 1.3f), 0, c.pos.z + RandF(-1.3f, 1.3f)});
    }
}

void Game::UpdateActor(int idx, const Input& in, float dt) {
    Actor& a = actors[idx];
    a.hitFlash = fmaxf(0, a.hitFlash - dt); a.toastT -= dt; a.tracerT -= dt;
    if (!a.alive) return;
    if (!a.isBot) { a.yaw -= in.look.x; a.pitch = Clamp(a.pitch - in.look.y, -1.3f, 1.3f); }
    a.fireCd = fmaxf(0, a.fireCd - dt); a.placeCd -= dt; a.idle += dt;
    if (a.swing >= 0) { a.swing += dt / 0.32f; if (a.swing >= 1) a.swing = -1; }
    
    if (in.drop && a.sel > 0) {
        Slot& s = a.slots[a.sel];
        if (!s.Empty()) {
            Vector3 dropPos = Vector3Add(a.pos, Vector3Scale(Fwd(a.yaw, 0), 1.2f));
            dropPos.y = a.pos.y;
            world.AddDrop(LootRoll{s.id, s.rar, s.count}, dropPos);
            s = Slot();
            a.fireCd = 0.2f;
        }
    }

    auto select = [&](int s) { if (s != a.sel) { a.sel = s; a.reloadT = 0; a.useT = 0; } a.buildMode = false; };
    if (in.slot >= 0) select(in.slot);
    if (in.next) select((a.sel + 1) % 6);
    if (in.prev) select((a.sel + 5) % 6);
    if (in.toggleBuild) a.buildMode = !a.buildMode;
    if (in.piece >= 0) { 
        if (a.buildMode) a.piece = (PieceType)in.piece; 
        else { a.piece = (PieceType)in.piece; a.buildMode = true; } 
    }
    if (in.cycleMat) a.mat = (Mat)((a.mat + 1) % MAT_COUNT);

    a.isAiming = false;
    if (!a.buildMode && a.sel > 0) {
        const Slot& s = a.slots[a.sel];
        if (!s.Empty() && GetItemDef(s.id).kind == K_WEAPON) {
            ItemId id = s.id;
            if (id == I_SNIPER || id == I_HUNTING) {
                a.isAiming = in.aim;
            }
        }
    }

    Vector3 f = {sinf(a.yaw), 0, cosf(a.yaw)}, r = RightOf(a.yaw);
    Vector3 wish = Vector3Add(Vector3Scale(f, in.move.y), Vector3Scale(r, in.move.x));
    float wl = Vector3Length(wish); if (wl > 1) wish = Vector3Scale(wish, 1.0f / wl);
    
    float spd = 6.2f;
    if (a.useT > 0) spd *= 0.45f;
    if (a.isAiming) spd *= 0.4f;
    
    a.pos = world.MoveSlide(a.pos, Vector3{wish.x * spd * dt, 0, wish.z * spd * dt}, 0.35f, 1.8f, 0.55f);
    a.pos.x = Clamp(a.pos.x, -world.half + 1, world.half - 1); a.pos.z = Clamp(a.pos.z, -world.half + 1, world.half - 1);
    float gy = world.GroundY(a.pos, a.pos.y, 0.55f);
    a.vel.y -= 22.0f * dt; a.pos.y += a.vel.y * dt;
    if (a.pos.y <= gy && a.vel.y <= 0) { a.pos.y = gy; a.vel.y = 0; a.grounded = true; } else a.grounded = false;
    if (in.jump && a.grounded) { a.vel.y = 7.2f; a.grounded = false; }
    a.moveAmt = Lerp(a.moveAmt, (wl > 0.1f && a.grounded) ? fminf(wl, 1.0f) : 0.0f, fminf(1.0f, 10 * dt));
    a.phase += dt * 10.0f * a.moveAmt;

    if (!a.isBot) ComputeCamera(a);

    if (sqrtf(a.pos.x * a.pos.x + a.pos.z * a.pos.z) > stormR) { a.stormTick += dt; if (a.stormTick >= 1.0f) { a.stormTick = 0; Damage(idx, (float)(1 + stormPhase), -1); if (!a.alive) return; } }

    a.prompt.clear();
    int nd = -1; float dd = 2.4f;
    for (int k = 0; k < (int)world.drops.size(); k++) if (world.drops[k].alive) { float d = Vector3Distance(a.pos, world.drops[k].pos); if (d < dd) { dd = d; nd = k; } }
    int nc = -1; float cd = 2.8f;
    for (int k = 0; k < (int)world.chests.size(); k++) if (!world.chests[k].opened) { float d = Vector3Distance(a.pos, world.chests[k].pos); if (d < cd) { cd = d; nc = k; } }
    if (nd >= 0) {
        a.prompt = std::string("Pick up ") + GetItemDef(world.drops[nd].item.id).name;
        if (in.interact) PickUp(a, nd);
    } else if (nc >= 0) {
        a.prompt = "Open Chest";
        if (in.interact) OpenChest(nc);
    }

    if (a.buildMode) {
        if (in.fire && a.placeCd <= 0) {
            a.placeCd = 0.12f;
            if (a.mats[a.mat] < 10) { Toast(a, "Not enough materials"); }
            else if (world.Place(a.piece, a.mat, a.pos, a.yaw)) { 
                a.mats[a.mat] -= 10; 
                gAudio.PlayRandomAt("Items/Fort_Bandage_Use_Athena_01", a.pos, a.camPos, 0.8f);
            }
        }
        return;
    }
    if (a.sel == 0) {
        if (in.fire && a.swing < 0 && a.fireCd <= 0) { a.swing = 0; a.fireCd = 0.55f; Harvest(idx); }
        return;
    }
    Slot& s = a.slots[a.sel];
    if (s.Empty()) return;
    const ItemDef& d = GetItemDef(s.id);
    if (d.kind == K_WEAPON) {
        if (a.reloadT > 0) {
            a.reloadT -= dt;
            if (a.reloadT <= 0) { 
                int take = std::min(d.mag - s.mag, a.ammo[d.ammo]); 
                s.mag += take; 
                a.ammo[d.ammo] -= take; 
                gAudio.PlayRandomAt("Weapons/SMG/sfx_smg_singlefire_medium", a.pos, a.camPos, 0.8f);
            }
        } else if ((in.reload || (s.mag <= 0 && in.fire)) && s.mag < d.mag && a.ammo[d.ammo] > 0) {
            a.reloadT = d.reload;
            gAudio.PlayRandomAt("Weapons/SMG/sfx_smg_singlefire_medium", a.pos, a.camPos, 0.8f);
        } else if (in.fire && a.fireCd <= 0 && s.mag > 0 && (d.autoFire || in.firePressed)) {
            a.fireCd = 1.0f / d.rate; 
            s.mag--; 
            ItemDef dMod = d;
            if (a.isAiming) dMod.spread *= 0.1f;
            FireWeapon(idx, s, dMod);
        } else if (in.fire && s.mag <= 0 && a.ammo[d.ammo] <= 0 && in.firePressed) Toast(a, "Out of ammo");
    } else if (d.kind == K_HEAL) {
        bool can = (d.heal > 0 && a.health < d.healMax) || (d.shield > 0 && a.shield < d.shieldMax);
        if (in.fire && can) {
            a.useT += dt;
            if (a.useT >= d.useTime) {
                if (d.heal > 0 && a.health < d.healMax) a.health = fminf(d.healMax, a.health + d.heal);
                if (d.shield > 0 && a.shield < d.shieldMax) a.shield = fminf(d.shieldMax, a.shield + d.shield);
                if (--s.count <= 0) s = Slot();
                a.useT = 0;
                gAudio.PlayRandomAt("Items/sfx_MiniShieldPotion_Complete_01", a.pos, a.camPos, 0.8f);
            }
        } else a.useT = 0;
    } else if (d.kind == K_THROWABLE) {
        if (in.firePressed && a.fireCd <= 0) {
            a.fireCd = 0.6f;
            Vector3 m = Vector3{a.pos.x, a.pos.y + 1.4f, a.pos.z};
            Vector3 v = Vector3Add(Vector3Scale(a.camFwd, 16.0f), Vector3{0, 4.0f, 0});
            grenades.push_back(Grenade{m, v, 1.7f, idx});
            if (--s.count <= 0) s = Slot();
            gAudio.PlayRandomAt("Weapons/Smoke_Grenade/sfx_SmokeGrenade_Bounce_01", a.pos, a.camPos, 0.8f);
        }
    }
}

void Game::BotThink(int i, float dt, Input& in) {
    Actor& b = actors[i];
    Vector3 eye = {b.pos.x, b.pos.y + 1.5f, b.pos.z};
    b.thinkT -= dt;
    if (b.thinkT <= 0) {
        b.thinkT = 0.35f; b.enemy = -1; float best = 50;
        for (int j = 0; j < (int)actors.size(); j++) {
            if (j == i || !actors[j].alive) continue;
            Vector3 tp = {actors[j].pos.x, actors[j].pos.y + 1.2f, actors[j].pos.z};
            Vector3 dv = Vector3Subtract(tp, eye); float d = Vector3Length(dv);
            if (d >= best || d < 0.1f) continue;
            RayHit h = world.Raycast(Ray{eye, Vector3Scale(dv, 1.0f / d)}, d);
            if (!h.hit || h.dist > d - 0.6f) { best = d; b.enemy = j; }
        }
        int w = -1;
        for (int k = 1; k < 6; k++) if (!b.slots[k].Empty() && GetItemDef(b.slots[k].id).kind == K_WEAPON) { w = k; break; }
        b.sel = (w >= 0) ? w : 0;
        if (b.enemy < 0) {
            float bd = 70; bool found = false;
            for (const auto& d : world.drops) if (d.alive) { float dist = Vector3Distance(b.pos, d.pos); if (dist < bd) { bd = dist; b.goal = d.pos; found = true; } }
            if (w < 0) for (const auto& c : world.chests) if (!c.opened) { float dist = Vector3Distance(b.pos, c.pos); if (dist < bd) { bd = dist; b.goal = c.pos; found = true; } }
            if (!found && (Vector3Distance(b.pos, b.goal) < 2.0f || RandF(0, 1) < 0.05f)) {
                float R = fmaxf(8.0f, fminf(stormR - 8, 50.0f * world.half / 60.0f)); b.goal = {RandF(-R, R), 0, RandF(-R, R)};
            }
        }
    }
    Vector3 goal = b.goal;
    if (sqrtf(b.pos.x * b.pos.x + b.pos.z * b.pos.z) > stormR - 4) goal = {0, 0, 0};
    bool aiming = b.enemy >= 0 && actors[b.enemy].alive;
    Vector3 look, target = {0, 0, 0};
    if (aiming) { Actor& e = actors[b.enemy]; target = {e.pos.x, e.pos.y + 1.2f, e.pos.z}; look = target; }
    else look = {goal.x, eye.y, goal.z};
    Vector3 dv = Vector3Subtract(look, eye);
    float want = atan2f(dv.x, dv.z), dy = WrapPi(want - b.yaw);
    b.yaw += Clamp(dy, -6.0f * dt, 6.0f * dt);
    float hd = sqrtf(dv.x * dv.x + dv.z * dv.z);
    b.pitch = Lerp(b.pitch, Clamp(atan2f(dv.y, fmaxf(hd, 0.1f)), -1.0f, 1.0f), fminf(1.0f, 8 * dt));
    float gd = Vector3Distance(Vector3{b.pos.x, 0, b.pos.z}, Vector3{goal.x, 0, goal.z});
    bool gun = b.sel > 0 && !b.slots[b.sel].Empty() && GetItemDef(b.slots[b.sel].id).kind == K_WEAPON;
    float edist = aiming ? Vector3Distance(eye, target) : 0;
    in.move.y = (gd > 1.5f) ? 1.0f : 0.0f;
    if (aiming) {
        in.move.x = sinf((float)GetTime() * 1.3f + i) > 0 ? 0.6f : -0.6f;
        if (gun && edist < 14) in.move.y = 0;
        if (!gun) in.move.y = edist > 2.5f ? 1.0f : 0.0f;
        if (gun && fabsf(dy) < 0.25f && edist < 45) { in.fire = true; in.firePressed = true; }
        else if (!gun && edist < 3.2f && fabsf(dy) < 0.5f) in.fire = true;
    }
    Vector3 aimPt = aiming ? Vector3Add(target, Vector3{RandF(-0.6f, 0.6f), RandF(-0.4f, 0.4f), RandF(-0.6f, 0.6f)}) : Vector3Add(eye, Vector3Scale(Fwd(b.yaw, 0), 40));
    b.camPos = eye; b.camFwd = Vector3Normalize(Vector3Subtract(aimPt, eye));
    if (gun && b.slots[b.sel].mag <= 0) in.reload = true;
    if (Vector3Distance(b.pos, b.lastPos) < dt * 1.5f && in.move.y > 0) b.stuckT += dt; else b.stuckT = 0;
    b.lastPos = b.pos;
    if (b.stuckT > 0.35f) { in.jump = true; in.move.x = 1; if (b.stuckT > 1.2f) { b.goal = {RandF(-35, 35) * world.half / 60.0f, 0, RandF(-35, 35) * world.half / 60.0f}; b.stuckT = 0; } }
    b.interactT -= dt; if (b.interactT <= 0) { in.interact = true; b.interactT = 0.5f; }
}

// Sniper scope: black mask with a round lens, crosshair, mil-dots and thick outer posts.
static void DrawScope(int W, int H, bool bolt) {
    float cx = W / 2.0f, cy = H / 2.0f, R = H * 0.46f;
    DrawRing({cx, cy}, R, sqrtf((float)(W * W + H * H)) / 2.0f * 1.05f + 8.0f, 0, 360, 72, BLACK);
    DrawRing({cx, cy}, R - 2.0f, R, 0, 360, 72, C(30, 30, 34));
    Color rc = C(8, 8, 8, 240);
    DrawLineEx({cx - R, cy}, {cx + R, cy}, 1.0f, rc); DrawLineEx({cx, cy - R}, {cx, cy + R}, 1.0f, rc);
    float post = R * 0.5f, th = bolt ? 3.0f : 2.0f;
    DrawRectangle((int)(cx - R), (int)(cy - th / 2), (int)post, (int)th, BLACK); DrawRectangle((int)(cx + R - post), (int)(cy - th / 2), (int)post, (int)th, BLACK);
    DrawRectangle((int)(cx - th / 2), (int)(cy - R), (int)th, (int)post, BLACK); DrawRectangle((int)(cx - th / 2), (int)(cy + R - post), (int)th, (int)post, BLACK);
    for (int k = 1; k <= 4; k++) {
        float o = k * R * 0.11f;
        DrawRectangle((int)(cx + o), (int)(cy - 1), 2, 3, rc); DrawRectangle((int)(cx - o), (int)(cy - 1), 2, 3, rc);
        DrawRectangle((int)(cx - 1), (int)(cy + o), 3, 2, rc); DrawRectangle((int)(cx - 1), (int)(cy - o), 3, 2, rc);
    }
    DrawRectangle((int)cx - 1, (int)cy - 1, 3, 3, Color{220, 30, 30, 255});
}

void Game::RenderView(RenderTexture2D& rt, int W, int H, int ai) {
    Actor& me = actors[ai];
    Camera3D cam = {};
    cam.position = me.camPos; cam.target = Vector3Add(me.camPos, me.camFwd); cam.up = {0, 1, 0};
    bool scoped = me.isAiming && me.alive && !me.buildMode;
    float fov = 58.0f;
    if (scoped) fov = (me.slots[me.sel].id == I_SNIPER) ? 12.0f : 20.0f;     // bolt-action = high zoom, hunting rifle = medium
    cam.fovy = fov;
    cam.projection = CAMERA_PERSPECTIVE;
    
    BeginTextureMode(rt);
    ClearBackground(gR.fogColor);
    DrawRectangleGradientV(0, 0, W, H / 2, C(230, 140, 92), gR.fogColor);
    gR.SetResolution((float)W, (float)H);
    BeginMode3D(cam);
    world.Draw(cam.position, stormR, cam);
    for (const auto& a : actors) {
        if (!a.alive) continue;
        if (scoped && &a == &me) continue;      // eye is inside the model while scoped
        float gy = world.GroundY(a.pos, a.pos.y, 0.55f);
        DrawCylinder({a.pos.x, gy + 0.06f, a.pos.z}, 0.45f, 0.45f, 0.01f, 8, Color{0, 0, 0, 90});
        Pose p; p.phase = a.phase; p.moveAmt = a.moveAmt; p.swing = a.swing; p.pitch = a.pitch; p.idle = a.idle; p.airborne = !a.grounded;
        const Slot& s = a.slots[a.sel];
        if (!a.buildMode && a.sel == 0) p.pick = true;
        else if (!a.buildMode && !s.Empty() && GetItemDef(s.id).kind == K_WEAPON) {
            p.gun = true; p.gunTint = RarityColor(s.rar);
            p.gunScale = 1.0f; p.weapon = (int)s.id;      // every weapon has its own model, built at true size
        }
        p.ads = a.isAiming;
        CharacterDraw(chars[a.look], held, a.pos, a.yaw, p, a.hitFlash > 0 ? Color{255, 130, 130, 255} : WHITE);
        if (a.tracerT > 0) DrawLine3D(a.tA, a.tB, Color{255, 240, 150, 255});
    }
    for (const auto& g : grenades) DrawSphere(g.p, 0.12f, C(50, 80, 40));
    for (const auto& f : fx) { float k = f.t / f.life; DrawSphereEx(f.p, f.r * (0.3f + 0.7f * k), 6, 6, Color{255, (unsigned char)(190 - 120 * k), 60, (unsigned char)(200 * (1 - k))}); }
    
    gVFX.Draw3D();
    world.DrawStorm(stormR, cam);      // translucent: after every opaque thing
    
    if (!me.isBot && me.alive && me.buildMode) {
        Vector3 pos; int dir; world.GetPlacement(me.piece, me.pos, me.yaw, pos, dir);
        AABB bx = world.PieceBox(me.piece, pos, dir);
        Vector3 c = {(bx.min.x + bx.max.x) / 2, (bx.min.y + bx.max.y) / 2, (bx.min.z + bx.max.z) / 2}, sz = {bx.max.x - bx.min.x, bx.max.y - bx.min.y, bx.max.z - bx.min.z};
        DrawCubeWiresV(c, sz, me.mats[me.mat] >= 10 ? Color{120, 255, 160, 255} : Color{255, 90, 90, 255});
    }
    EndMode3D();
    {   // storm screen effect while standing in the storm + scope mask
        float dist = sqrtf(me.pos.x * me.pos.x + me.pos.z * me.pos.z);
        if (me.alive && dist > stormR) {
            float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 3.0f);
            DrawRectangle(0, 0, W, H, Color{120, 40, 190, (unsigned char)(48 + 28 * pulse)});
            const int e = 36; Color s = Color{70, 10, 120, 150}, z = Color{70, 10, 120, 0};
            DrawRectangleGradientH(0, 0, e, H, s, z); DrawRectangleGradientH(W - e, 0, e, H, z, s);
            DrawRectangleGradientV(0, 0, W, e, s, z); DrawRectangleGradientV(0, H - e, W, e, z, s);
        }
        if (scoped) DrawScope(W, H, me.slots[me.sel].id == I_SNIPER);
    }
    HudInfo info{AliveCount(), stormT, shrinking, stormR, &feed, state == State::Over, victory && me.alive, me.placement ? me.placement : humanPlacement};
    DrawHud(me, world, W, H, info);
    if (!me.isBot && me.alive) gVFX.Draw2D(cam, W, H);
    EndTextureMode();
}

void Game::DrawMatch() {
    BeginDrawing();
    ClearBackground(BLACK);
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (humans == 1) {
        RenderView(rtFull, 320, 240, 0);
        float w = sh * 4.0f / 3.0f, h = (float)sh; if (w > sw) { w = (float)sw; h = sw * 0.75f; }
        Present(rtFull, {(sw - w) / 2, (sh - h) / 2, w, h});
    } else {
        RenderView(rtHalf[0], 214, 240, 0);
        RenderView(rtHalf[1], 214, 240, 1);
        Present(rtHalf[0], {0, 0, sw / 2.0f, (float)sh});
        Present(rtHalf[1], {sw / 2.0f, 0, sw / 2.0f, (float)sh});
        DrawRectangle(sw / 2 - 1, 0, 2, sh, BLACK);
    }
    EndDrawing();
}