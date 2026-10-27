#include "World.h"
#include "Renderer.h"
#include "IconCache.h"

const Color kMatColor[3] = {{222, 170, 100, 255}, {196, 98, 84, 255}, {165, 176, 192, 255}};
const char* MapName(MapId m) {
    switch (m) { case MAP_DUSTY: return "Dusty Depot"; case MAP_CONTAINER: return "Container Yard"; case MAP_SALTY: return "Salty Springs"; default: return "Retail Row"; }
}

static Vector3 DirVec(int d) { static const Vector3 v[4] = {{0, 0, 1}, {1, 0, 0}, {0, 0, -1}, {-1, 0, 0}}; return v[d & 3]; }
static int DirFromYaw(float yaw) { float fx = sinf(yaw), fz = cosf(yaw); if (fabsf(fx) > fabsf(fz)) return fx > 0 ? 1 : 3; return fz > 0 ? 0 : 2; }
static Matrix About(Matrix rot, Vector3 c) { return MatrixMultiply(MatrixMultiply(MatrixTranslate(-c.x, -c.y, -c.z), rot), MatrixTranslate(c.x, c.y, c.z)); }

// ----------------------------------------------------------------------------------------- helpers
void World::Block(MeshBuilder& mb, Vector3 c, Vector3 s, Color col, bool collide) {
    mb.Box(c, s, col);
    if (collide) { Solid so; so.box = BoxAt(c, s); so.kind = SK_STATIC; solids.push_back(so); }
}

void World::Building(MeshBuilder& mb, float cx, float cz, float w, float d, float h, Color wc, float door, int roof, Color rc) {
    const float t = 0.4f, dh = 3.4f;
    Block(mb, {cx - w / 2, h / 2, cz}, {t, h, d}, wc);
    Block(mb, {cx + w / 2, h / 2, cz}, {t, h, d}, wc);
    for (int s = -1; s <= 1; s += 2) {
        float z = cz + s * d / 2;
        if (door <= 0) Block(mb, {cx, h / 2, z}, {w, h, t}, wc);
        else {
            float seg = (w - door) / 2;
            Block(mb, {cx - door / 2 - seg / 2, h / 2, z}, {seg, h, t}, wc);
            Block(mb, {cx + door / 2 + seg / 2, h / 2, z}, {seg, h, t}, wc);
            Block(mb, {cx, (h + dh) / 2, z}, {door, h - dh, t}, wc);
        }
    }
    if (roof == 1) Block(mb, {cx, h + 0.15f, cz}, {w + 0.6f, 0.3f, d + 0.6f}, rc);
}

void World::ArchRoof(MeshBuilder& mb, float cx, float cz, float len, float hw, float baseY, float rise, Color a, Color b) {
    const int seg = 8; int n = (int)(len / 2.0f);
    for (int k = 0; k < n; k++) {
        float z0 = cz - len / 2 + k * len / n, z1 = z0 + len / n; Color c = (k & 1) ? a : b;
        for (int i = 0; i < seg; i++) {
            float t0 = kPi * i / seg, t1 = kPi * (i + 1) / seg;
            Vector3 p0 = {cx + hw * cosf(t0), baseY + rise * sinf(t0), z0}, p1 = {cx + hw * cosf(t1), baseY + rise * sinf(t1), z0};
            Vector3 p2 = {p1.x, p1.y, z1}, p3 = {p0.x, p0.y, z1};
            mb.Quad(p0, p1, p2, p3, Mul(c, 0.9f + 0.1f * sinf(t0 + 0.5f)), 3.0f);
        }
    }
    for (int e = -1; e <= 1; e += 2) {          // gable end caps
        float z = cz + e * len / 2;
        for (int i = 0; i < seg; i++) {
            float t0 = kPi * i / seg, t1 = kPi * (i + 1) / seg;
            mb.Tri({cx, baseY, z}, {cx + hw * cosf(t0), baseY + rise * sinf(t0), z}, {cx + hw * cosf(t1), baseY + rise * sinf(t1), z}, Mul(a, 0.85f));
        }
    }
}

void World::Scaffold(MeshBuilder& mb, float cx, float cz, float w, float d, float h, Color col) {
    for (int sx = -1; sx <= 1; sx += 2) for (int sz = -1; sz <= 1; sz += 2) Block(mb, {cx + sx * w / 2, h / 2, cz + sz * d / 2}, {0.18f, h, 0.18f}, col);
    for (float y : {h * 0.5f, h}) {
        for (int sz = -1; sz <= 1; sz += 2) mb.Box({cx, y, cz + sz * d / 2}, {w, 0.14f, 0.14f}, col);
        for (int sx = -1; sx <= 1; sx += 2) mb.Box({cx + sx * w / 2, y, cz}, {0.14f, 0.14f, d}, col);
    }
    float ang = atanf(h * 0.5f / w);                                      // diagonal braces
    for (int sz = -1; sz <= 1; sz += 2) {
        Vector3 c = {cx, h * 0.25f, cz + sz * d / 2}; Matrix r = About(MatrixRotateZ(ang), c);
        mb.Box(c, {sqrtf(w * w + h * h * 0.25f), 0.1f, 0.1f}, col, &r);
    }
}

void World::AddCrate(float x, float y, float z, float s) {
    Solid so; so.kind = SK_CRATE; so.pos = {x, y, z}; so.scale = {s, s, s}; so.yaw = RandF(0, 360); so.hp = 60 + s * 40;
    so.box = BoxAt({x, y + s / 2, z}, {s * 1.15f, s, s * 1.15f});
    solids.push_back(so);
}
void World::AddTree(float x, float z, float s) {
    Solid so; so.kind = SK_TREE; so.pos = {x, 0, z}; so.scale = {s, s, s}; so.yaw = RandF(0, 360); so.hp = 150;
    so.box = BoxAt({x, 4.0f * s, z}, {0.8f * s, 8.0f * s, 0.8f * s});
    solids.push_back(so);
}
void World::AddContainer(float x, float y, float z, float yaw, Color tint) {
    Solid so; so.kind = SK_CONTAINER; so.pos = {x, y, z}; so.yaw = yaw; so.tint = tint; so.hp = 1e9f;
    bool rot = ((int)(yaw / 90.0f + 0.5f)) & 1;
    so.box = BoxAt({x, y + 1.3f, z}, rot ? Vector3{2.4f, 2.6f, 12.0f} : Vector3{12.0f, 2.6f, 2.4f});
    solids.push_back(so);
}

bool World::Free(float x, float z, float m) const {
    for (const auto& s : solids) {
        if (!s.alive || s.box.max.y < 0.2f) continue;
        if (x > s.box.min.x - m && x < s.box.max.x + m && z > s.box.min.z - m && z < s.box.max.z + m) return false;
    }
    return true;
}
bool World::FindFree(float x0, float x1, float z0, float z1, float m, Vector3& out) const {
    for (int i = 0; i < 60; i++) { float x = RandF(x0, x1), z = RandF(z0, z1); if (Free(x, z, m)) { out = {x, 0, z}; return true; } }
    return false;
}
void World::ScatterLoot(float x0, float x1, float z0, float z1, int floorLoot, int nChests) {
    Vector3 p;
    for (int i = 0; i < nChests; i++) if (FindFree(x0, x1, z0, z1, 1.2f, p)) { Chest c; c.pos = p; c.yaw = RandF(0, 360); chests.push_back(c); }
    for (int i = 0; i < floorLoot; i++) if (FindFree(x0, x1, z0, z1, 0.8f, p)) AddDrop(RollLoot(false), p);
}
void World::AddDrop(LootRoll r, Vector3 p) { Drop d; d.item = r; d.pos = {p.x, 0.0f, p.z}; d.spin = RandF(0, 360); drops.push_back(d); }

// ----------------------------------------------------------------------------------------- shared models
void World::BuildShared() {
    const Color W = C(255, 255, 255);
    { MeshBuilder mb; mb.Box({0, 0, 0}, {4, 4, 0.25f}, W); mPiece[P_WALL] = gR.MakeModel(mb, &gR.texWood); }
    { MeshBuilder mb; mb.Box({0, 0, 0}, {4, 0.25f, 4}, W); mPiece[P_FLOOR] = gR.MakeModel(mb, &gR.texWood); }
    { MeshBuilder mb; for (int k = 0; k < 8; k++) mb.Box({0, (k + 1) * 0.25f, -2.0f + (k + 0.5f) * 0.5f}, {4, (k + 1) * 0.5f, 0.5f}, Mul(W, 0.9f + 0.1f * (k & 1))); mPiece[P_STAIR] = gR.MakeModel(mb, &gR.texWood); }
    { MeshBuilder mb; Matrix ry = MatrixRotateY(kPi / 4); mb.Loft({{0, 2.83f, 2.83f, W}, {2.0f, 0.0f, 0.0f, Mul(W, 0.85f)}}, 4, &ry, false, false); mb.Box({0, -0.125f, 0}, {4, 0.25f, 4}, W); mPiece[P_ROOF] = gR.MakeModel(mb, &gR.texWood); }
    { // crate: unit cube (y 0..1) with frame
        MeshBuilder mb; Color body = C(210, 128, 58), fr = C(150, 88, 40);
        mb.Box({0, 0.5f, 0}, {1, 1, 1}, body);
        for (float y : {0.04f, 0.96f}) mb.Box({0, y, 0}, {1.04f, 0.09f, 1.04f}, fr);
        for (int sx = -1; sx <= 1; sx += 2) for (int sz = -1; sz <= 1; sz += 2) mb.Box({sx * 0.48f, 0.5f, sz * 0.48f}, {0.09f, 1.0f, 0.09f}, fr);
        Matrix r = About(MatrixRotateZ(0.78f), {0, 0.5f, 0.51f}); mb.Box({0, 0.5f, 0.51f}, {1.3f, 0.07f, 0.03f}, fr, &r);
        mCrate = gR.MakeModel(mb, &gR.texWood);
    }
    { // tree: trunk + chunky canopy
        MeshBuilder mb; Color tk = C(112, 80, 52), l1 = C(60, 110, 40), l2 = C(86, 140, 52);
        mb.Loft({{0, 0.38f, 0.38f, Mul(tk, 0.8f)}, {2.5f, 0.28f, 0.28f, tk}, {5.0f, 0.22f, 0.22f, tk}}, 7, nullptr, false, false);
        mb.Loft({{3.2f, 0.1f, 0.1f, l1}, {4.2f, 2.2f, 2.2f, l1}, {6.0f, 2.5f, 2.4f, l2}, {7.8f, 1.5f, 1.5f, l1}, {9.0f, 0.0f, 0.0f, l2}}, 8, nullptr, false, false);
        mb.Loft({{5.0f, 0.1f, 0.1f, l2, 1.0f, 0.5f}, {6.0f, 1.4f, 1.4f, l1, 1.0f, 0.5f}, {7.2f, 0.0f, 0.0f, l2, 1.0f, 0.5f}}, 6, nullptr, false, false);
        mTree = gR.MakeModel(mb, &gR.texNoise);
    }
    { // shipping container 12 x 2.6 x 2.4 (origin bottom-centre), tinted per instance
        MeshBuilder mb; Color b = C(235, 235, 235), rib = C(205, 205, 205), dk = C(120, 120, 120);
        mb.Box({0, 1.3f, 0}, {12, 2.6f, 2.4f}, b, nullptr, 3.0f);
        for (int k = 0; k < 15; k++) for (int s = -1; s <= 1; s += 2) mb.Box({-5.6f + k * 0.8f, 1.3f, s * 1.2f}, {0.14f, 2.5f, 0.1f}, rib);
        for (int s = -1; s <= 1; s += 2) { mb.Box({6.02f, 1.3f, s * 0.6f}, {0.08f, 2.4f, 0.1f}, dk); mb.Box({6.05f, 1.3f, s * 0.2f}, {0.08f, 0.5f, 0.06f}, dk); }
        mb.Box({6.02f, 1.3f, 0}, {0.06f, 0.1f, 2.3f}, dk);
        mContainer = gR.MakeModel(mb, &gR.texMetal);
    }
    { // chest (body + hinged lid)
        MeshBuilder mb; Color o = C(236, 150, 36), dk = C(60, 40, 18);
        mb.Box({0, 0.22f, 0}, {0.9f, 0.44f, 0.6f}, o);
        mb.Box({-0.3f, 0.22f, 0}, {0.08f, 0.46f, 0.62f}, dk); mb.Box({0.3f, 0.22f, 0}, {0.08f, 0.46f, 0.62f}, dk);
        mb.Box({0, 0.44f, 0}, {0.92f, 0.04f, 0.04f}, dk);
        mChestBody = gR.MakeModel(mb);
        MeshBuilder lb;
        lb.Box({0, 0.1f, 0.3f}, {0.92f, 0.2f, 0.62f}, Mul(o, 1.08f));
        lb.Box({-0.3f, 0.1f, 0.3f}, {0.08f, 0.22f, 0.64f}, dk); lb.Box({0.3f, 0.1f, 0.3f}, {0.08f, 0.22f, 0.64f}, dk);
        lb.Box({0, 0.1f, 0.62f}, {0.12f, 0.1f, 0.04f}, C(250, 220, 90));
        mChestLid = gR.MakeModel(lb);
    }
    { // storm wall: unit-radius open cylinder, 220 m tall; vertex colour red channel tells the shader "wall" (1) vs "haze" (0)
        MeshBuilder mb; mb.Loft({{0, 1, 1, C(255, 255, 255)}, {220, 1, 1, C(255, 255, 255)}}, 48, nullptr, false, false); mStorm = gR.MakeStormModel(mb);
        MeshBuilder hz; const int N = 48;                                   // dark haze on the ground outside the wall (scaled with the wall)
        for (int i = 0; i < N; i++) {
            float a0 = 2 * kPi * i / N, a1 = 2 * kPi * (i + 1) / N, R0 = 1.0f, R1 = 150.0f;
            hz.Quad({cosf(a0) * R0, 0, sinf(a0) * R0}, {cosf(a1) * R0, 0, sinf(a1) * R0}, {cosf(a1) * R1, 0, sinf(a1) * R1}, {cosf(a0) * R1, 0, sinf(a0) * R1}, C(0, 0, 0), 1.0f);
        }
        mStormHaze = gR.MakeStormModel(hz);
    }
}

// ----------------------------------------------------------------------------------------- DUSTY DEPOT
void World::BuildDusty() {
    half = 60.0f;
    { MeshBuilder g; g.Quad({-300, 0, 300}, {300, 0, 300}, {300, 0, -300}, {-300, 0, -300}, C(255, 255, 255), 8.0f); mGround = gR.MakeModel(g, &gR.texGrass); }
    // Sand yard: flush decal at y = 0 (was a raised 0.2 m plane that swallowed feet, shadows and the base of every prop)
    { MeshBuilder g; g.Quad({-40, 0, 40}, {40, 0, 40}, {40, 0, -40}, {-40, 0, -40}, C(255, 255, 255), 5.0f); mDecal = gR.MakeModel(g, &gR.texSand); }

    MeshBuilder mb;
    // --- main arched hangar (blue roof like the far-left building in the reference) ---
    Building(mb, 0, -8, 20, 34, 5.0f, C(150, 160, 178), 10.0f, 0, WHITE);
    ArchRoof(mb, 0, -8, 34, 10.6f, 5.0f, 6.5f, C(52, 84, 150), C(62, 96, 164));
    // steel scaffolding + girders inside
    Scaffold(mb, -6, -20, 5, 5, 4.6f, C(150, 150, 156));
    Scaffold(mb, 6, -12, 5, 5, 4.6f, C(150, 150, 156));
    Scaffold(mb, -6, -2, 5, 4, 4.6f, C(150, 150, 156));
    Scaffold(mb, 7, 3, 4, 4, 4.0f, C(150, 150, 156));
    // --- outbuildings ---
    Building(mb, -28, 14, 12, 9, 3.8f, C(150, 112, 74), 4.0f, 1, C(110, 84, 58));
    Building(mb, 26, 16, 10, 8, 3.8f, C(138, 134, 124), 4.0f, 1, C(96, 94, 88));
    Building(mb, 28, -30, 14, 10, 4.5f, C(176, 164, 142), 4.0f, 1, C(120, 112, 98));
    // --- silo ---
    { Matrix id = MatrixTranslate(-30, 0, -30); mb.Loft({{0, 3.2f, 3.2f, C(150, 70, 46)}, {9, 3.2f, 3.2f, C(170, 82, 52)}, {10.5f, 1.4f, 1.4f, C(120, 60, 44)}}, 10, &id, true, true);
      Solid s; s.box = BoxAt({-30, 5, -30}, {6.4f, 10, 6.4f}); solids.push_back(s); }
    // concrete barriers
    for (int i = 0; i < 6; i++) Block(mb, {-14.0f + i * 5.0f, 0.5f, 22}, {2.4f, 1.0f, 0.6f}, C(150, 150, 148));
    mStatic = gR.MakeModel(mb, &gR.texMetal);

    // --- crates (harvestable) ---
    for (int i = 0; i < 26; i++) {   // inside hangar
        float x = RandF(-8.5f, 8.5f), z = RandF(-24, 8);
        if (fabsf(x) < 4.5f && (z > 4 || z < -20)) continue;      // keep door lanes open
        if (!Free(x, z, 1.6f)) continue;
        float s = RandF(0, 1) < 0.25f ? 2.4f : 1.6f; AddCrate(x, 0, z, s);
        if (RandF(0, 1) < 0.35f) AddCrate(x, s, z, 1.2f);
    }
    for (int i = 0; i < 16; i++) { float x = RandF(-38, 38), z = RandF(-38, 38); if (Free(x, z, 3.0f)) AddCrate(x, 0, z, 1.6f); }
    // --- trees ring ---
    for (int i = 0; i < 70; i++) {
        float a = RandF(0, 2 * kPi), r = RandF(38, 70);
        float x = cosf(a) * r, z = sinf(a) * r;
        if (fabsf(x) > half - 2 || fabsf(z) > half - 2) continue;
        if (Free(x, z, 2.0f)) AddTree(x, z, RandF(0.8f, 1.3f));
    }
    // --- spawn points on the perimeter ---
    for (int i = 0; i < 12; i++) { float a = i * kPi / 6 + 0.2f; spawns.push_back({cosf(a) * 54, 0, sinf(a) * 54}); }
    // --- loot ---
    ScatterLoot(-9, 9, -24, 8, 8, 4);        // hangar
    ScatterLoot(-33, -23, 10, 18, 3, 1);
    ScatterLoot(21, 31, 12, 20, 3, 1);
    ScatterLoot(22, 34, -34, -26, 3, 1);
    ScatterLoot(-40, 40, -40, 40, 8, 3);
}

// ----------------------------------------------------------------------------------------- CONTAINER YARD
void World::BuildYard() {
    half = 60.0f;
    { MeshBuilder g; g.Quad({-300, 0, 300}, {300, 0, 300}, {300, 0, -300}, {-300, 0, -300}, C(255, 255, 255), 8.0f); mGround = gR.MakeModel(g, &gR.texDirt); }
    // Concrete apron: flush decal at y = 0
    { MeshBuilder g; g.Quad({-56, 0, 56}, {56, 0, 56}, {56, 0, -56}, {-56, 0, -56}, C(255, 255, 255), 6.0f); mDecal = gR.MakeModel(g, &gR.texConcrete); }
    MeshBuilder mb;
    Building(mb, 0, 0, 22, 14, 5.0f, C(170, 168, 160), 5.0f, 1, C(110, 108, 104));      // dock office
    Scaffold(mb, 0, 0, 6, 4, 4.5f, C(150, 150, 156));
    // gantry crane
    Color yel = C(230, 190, 40);
    for (int sx = -1; sx <= 1; sx += 2) for (int sz = -1; sz <= 1; sz += 2) Block(mb, {sx * 12.0f, 9, -44.0f + sz * 4.0f}, {1.0f, 18, 1.0f}, yel);
    mb.Box({0, 18.5f, -44}, {26, 1.6f, 1.6f}, yel); mb.Box({0, 18.5f, -40}, {26, 1.6f, 1.6f}, yel); mb.Box({0, 18.5f, -48}, {26, 1.6f, 1.6f}, yel);
    // perimeter fence
    Color fc = C(120, 124, 130);
    Block(mb, {0, 1.5f, -57}, {116, 3, 0.4f}, fc); Block(mb, {0, 1.5f, 57}, {116, 3, 0.4f}, fc);
    Block(mb, {-57, 1.5f, 0}, {0.4f, 3, 116}, fc); Block(mb, {57, 1.5f, 0}, {0.4f, 3, 116}, fc);
    mStatic = gR.MakeModel(mb, &gR.texMetal);

    const Color pal[6] = {C(190, 52, 44), C(46, 90, 170), C(50, 140, 80), C(225, 130, 40), C(215, 190, 60), C(150, 150, 155)};
    for (int iz = -2; iz <= 2; iz++)
        for (int ix = -2; ix <= 2; ix++) {
            if (ix == 0 && iz == 0) continue;
            if (RandF(0, 1) < 0.15f) continue;
            float cx = ix * 23.0f + RandF(-2, 2), cz = iz * 20.0f + RandF(-2, 2);
            if (fabsf(iz) == 2 && fabsf(cz) > 47) continue;
            float yaw = RandI(0, 1) * 90.0f; bool rot = yaw > 45;
            for (int k = 0; k < 2; k++) {
                float ox = rot ? k * 3.0f : 0, oz = rot ? 0 : k * 3.0f;
                Color col = pal[RandI(0, 5)];
                AddContainer(cx + ox, 0, cz + oz, yaw, col);
                if (RandF(0, 1) < 0.4f) AddContainer(cx + ox, 2.6f, cz + oz, yaw, pal[RandI(0, 5)]);
            }
        }
    for (int i = 0; i < 18; i++) { float x = RandF(-50, 50), z = RandF(-50, 50); if (Free(x, z, 3.0f)) AddCrate(x, 0, z, 1.6f); }
    for (int i = 0; i < 24; i++) {
        float a = RandF(0, 2 * kPi), r = RandF(62, 80);
        // trees outside fence are only decoration (collider kept so they cannot be walked through)
        AddTree(cosf(a) * r, sinf(a) * r, RandF(0.9f, 1.3f));
    }
    for (int i = 0; i < 12; i++) { float a = i * kPi / 6 + 0.1f; spawns.push_back({cosf(a) * 53, 0, sinf(a) * 53}); }
    ScatterLoot(-52, 52, -52, 52, 22, 10);
    ScatterLoot(-9, 9, -5, 5, 4, 2);
}

// ----------------------------------------------------------------------------------------- POI helpers
// Reference-traced maps (Salty Springs / Retail Row). Positions come from overhead screenshots in pixels;
// pixel -> world: x = (px - imageCentreX) * metresPerPixel, z = (py - imageCentreY) * metresPerPixel.
// +x = east, +z = south (matches the HUD compass). metresPerPixel is the one number to tweak for true 1:1 scale.
struct RoadSeg { float ax, az, bx, bz, hw; };
static std::vector<RoadSeg> gRoadSegs;
static std::vector<AABB> gLots;

static bool OnRoadOrLot(float x, float z, float margin) {
    for (const auto& s : gRoadSegs) {
        float dx = s.bx - s.ax, dz = s.bz - s.az, L2 = dx * dx + dz * dz;
        float t = L2 > 0 ? Clamp(((x - s.ax) * dx + (z - s.az) * dz) / L2, 0.0f, 1.0f) : 0.0f;
        float px = s.ax + dx * t - x, pz = s.az + dz * t - z;
        if (px * px + pz * pz < (s.hw + margin) * (s.hw + margin)) return true;
    }
    for (const auto& l : gLots) if (x > l.min.x - margin && x < l.max.x + margin && z > l.min.z - margin && z < l.max.z + margin) return true;
    return false;
}

static void FlatQuad(MeshBuilder& mb, float x0, float z0, float x1, float z1, Color col, float tile) {
    mb.Quad({x0, 0, z1}, {x1, 0, z1}, {x1, 0, z0}, {x0, 0, z0}, col, tile);
}
static void RoadStrip(MeshBuilder& mb, const Vector2* p, int n, float width, Color col, bool record) {
    float hw = width / 2;
    for (int i = 0; i < n - 1; i++) {
        Vector2 a = p[i], b = p[i + 1]; float dx = b.x - a.x, dz = b.y - a.y, L = sqrtf(dx * dx + dz * dz);
        if (L < 0.01f) continue;
        float nx = -dz / L * hw, nz = dx / L * hw;
        mb.Quad({a.x + nx, 0, a.y + nz}, {b.x + nx, 0, b.y + nz}, {b.x - nx, 0, b.y - nz}, {a.x - nx, 0, a.y - nz}, col, 6.0f);
        if (record) gRoadSegs.push_back({a.x, a.y, b.x, b.y, hw});
    }
    for (int i = 0; i < n; i++)                                           // round joints
        for (int k = 0; k < 8; k++) {
            float t0 = 2 * kPi * k / 8, t1 = 2 * kPi * (k + 1) / 8;
            mb.Tri({p[i].x, 0, p[i].y}, {p[i].x + cosf(t0) * hw, 0, p[i].y + sinf(t0) * hw}, {p[i].x + cosf(t1) * hw, 0, p[i].y + sinf(t1) * hw}, col);
        }
}
static void RectOutline(MeshBuilder& mb, float x0, float z0, float x1, float z1, float t, Color c) {
    FlatQuad(mb, x0, z0, x1, z0 + t, c, 4); FlatQuad(mb, x0, z1 - t, x1, z1, c, 4);
    FlatQuad(mb, x0, z0, x0 + t, z1, c, 4); FlatQuad(mb, x1 - t, z0, x1, z1, c, 4);
}

void World::Gable(MeshBuilder& mb, float cx, float cz, float w, float d, float baseY, float rise, Color roof, Color wall) {
    const float o = 0.5f; float hw = w / 2 + o, hd = d / 2 + o, ty = baseY + rise;
    if (w >= d) {   // ridge along x
        mb.Quad({cx - hw, baseY, cz - hd}, {cx + hw, baseY, cz - hd}, {cx + hw, ty, cz}, {cx - hw, ty, cz}, Mul(roof, 1.0f), 3.0f);
        mb.Quad({cx - hw, baseY, cz + hd}, {cx + hw, baseY, cz + hd}, {cx + hw, ty, cz}, {cx - hw, ty, cz}, Mul(roof, 0.85f), 3.0f);
        for (int s = -1; s <= 1; s += 2) mb.Tri({cx + s * w / 2, baseY, cz - d / 2}, {cx + s * w / 2, baseY, cz + d / 2}, {cx + s * w / 2, ty, cz}, wall);
    } else {        // ridge along z
        mb.Quad({cx - hw, baseY, cz - hd}, {cx - hw, baseY, cz + hd}, {cx, ty, cz + hd}, {cx, ty, cz - hd}, Mul(roof, 1.0f), 3.0f);
        mb.Quad({cx + hw, baseY, cz - hd}, {cx + hw, baseY, cz + hd}, {cx, ty, cz + hd}, {cx, ty, cz - hd}, Mul(roof, 0.85f), 3.0f);
        for (int s = -1; s <= 1; s += 2) mb.Tri({cx - w / 2, baseY, cz + s * d / 2}, {cx + w / 2, baseY, cz + s * d / 2}, {cx, ty, cz + s * d / 2}, wall);
    }
}

void World::Windows(MeshBuilder& mb, float cx, float cz, float w, float d, float h, float door, Color glass) {
    const float ws = 1.3f, wh = 1.2f;
    int rows = h >= 8.0f ? 2 : 1;
    for (int r = 0; r < rows; r++) {
        float y = (r == 0) ? 1.9f : 5.6f;
        if (y + wh / 2 > h - 0.3f) continue;
        int n = (int)(w / 3.2f);
        for (int i = 0; i < n; i++) {
            float x = cx - w / 2 + (i + 0.5f) * w / n;
            if (door > 0 && r == 0 && fabsf(x - cx) < door / 2 + 0.9f) continue;
            for (int s = -1; s <= 1; s += 2) mb.Box({x, y, cz + s * (d / 2 + 0.22f)}, {ws, wh, 0.06f}, glass);
        }
        int m = (int)(d / 3.2f);
        for (int i = 0; i < m; i++) {
            float z = cz - d / 2 + (i + 0.5f) * d / m;
            for (int s = -1; s <= 1; s += 2) mb.Box({cx + s * (w / 2 + 0.22f), y, z}, {0.06f, wh, ws}, glass);
        }
    }
}

void World::Car(MeshBuilder& mb, float x, float z, bool alongZ, Color c) {
    Matrix r = alongZ ? About(MatrixRotateY(kPi / 2), {x, 0, z}) : MatrixIdentity();
    mb.Box({x, 0.55f, z}, {4.2f, 0.8f, 1.8f}, c, &r);
    mb.Box({x - 0.2f, 1.15f, z}, {2.2f, 0.6f, 1.6f}, C(70, 90, 110), &r);
    for (int sx = -1; sx <= 1; sx += 2) for (int sz = -1; sz <= 1; sz += 2) mb.Box({x + sx * 1.3f, 0.32f, z + sz * 0.85f}, {0.7f, 0.64f, 0.25f}, C(26, 26, 28), &r);
    Solid so; so.box = BoxAt({x, 0.8f, z}, alongZ ? Vector3{1.8f, 1.6f, 4.2f} : Vector3{4.2f, 1.6f, 1.8f}); so.kind = SK_STATIC; solids.push_back(so);
}

void World::Fence(MeshBuilder& mb, float cx, float cz, float w, float d, Color c) {
    const float t = 0.12f, h = 1.0f, gap = 3.0f;
    Block(mb, {cx, h / 2, cz - d / 2}, {w, h, t}, c);
    Block(mb, {cx - w / 2, h / 2, cz}, {t, h, d}, c);
    Block(mb, {cx + w / 2, h / 2, cz}, {t, h, d}, c);
    float seg = (w - gap) / 2;
    Block(mb, {cx - gap / 2 - seg / 2, h / 2, cz + d / 2}, {seg, h, t}, c);
    Block(mb, {cx + gap / 2 + seg / 2, h / 2, cz + d / 2}, {seg, h, t}, c);
}

void World::PoiBuild(MeshBuilder& mb, const PoiB* b, int n, float cxp, float cyp, float mpp) {
    for (int i = 0; i < n; i++) {
        float w = b[i].w * mpp, d = b[i].h * mpp, cx = (b[i].x + b[i].w / 2 - cxp) * mpp, cz = (b[i].y + b[i].h / 2 - cyp) * mpp;
        Building(mb, cx, cz, w, d, b[i].ht, b[i].wall, b[i].door, b[i].roofType == 0 ? 1 : 0, b[i].roof);
        if (b[i].roofType == 1) Gable(mb, cx, cz, w, d, b[i].ht, fminf(w, d) * 0.32f, b[i].roof, b[i].wall);
        Windows(mb, cx, cz, w, d, b[i].ht, b[i].door, C(52, 72, 104));
    }
    for (int i = 0; i < n; i++) {
        float w = b[i].w * mpp, d = b[i].h * mpp, cx = (b[i].x + b[i].w / 2 - cxp) * mpp, cz = (b[i].y + b[i].h / 2 - cyp) * mpp;
        ScatterLoot(cx - w / 2 + 1.2f, cx + w / 2 - 1.2f, cz - d / 2 + 1.2f, cz + d / 2 - 1.2f, 2, b[i].chests);
    }
}

void World::ScatterTrees(int n, float roadMargin) {
    int placed = 0;
    for (int tries = 0; tries < n * 12 && placed < n; tries++) {
        float x = RandF(-half + 3, half - 3), z = RandF(-half + 3, half - 3);
        if (!Free(x, z, 2.4f) || OnRoadOrLot(x, z, roadMargin)) continue;
        AddTree(x, z, RandF(0.8f, 1.35f)); placed++;
    }
}

// ----------------------------------------------------------------------------------------- RETAIL ROW
// Traced from the overhead shot (1920x1080). metresPerPixel from parking-bay width (~2.5 m).
void World::BuildRetail() {
    half = 100.0f;
    const float cxp = 960, cyp = 540, mpp = 0.115f;
    gRoadSegs.clear(); gLots.clear();
    auto X = [&](float px) { return (px - cxp) * mpp; };
    auto Z = [&](float py) { return (py - cyp) * mpp; };
    { MeshBuilder g; g.Quad({-300, 0, 300}, {300, 0, 300}, {300, 0, -300}, {-300, 0, -300}, C(255, 255, 255), 8.0f); mGround = gR.MakeModel(g, &gR.texGrass); }

    const Color brick = C(152, 100, 80), dkRoof = C(70, 72, 80), grey = C(110, 112, 120);
    const PoiB bs[] = {
        // retail strip, north-east
        {1060, 160,  70, 102,  7.0f, brick,           dkRoof, 0, 4.0f, 1},
        {1130, 145, 110, 133, 11.0f, C(164, 110, 88), dkRoof, 0, 6.0f, 2},   // NOMS grocery (2 storeys tall)
        {1240, 190, 118,  65,  6.5f, C(140, 92, 74),  dkRoof, 0, 4.0f, 1},   // Big Shots
        {1440, 245,  80,  50,  6.0f, C(130, 126, 120), dkRoof, 0, 3.5f, 1},  // pylon base
        // east row
        {1510, 305, 140, 105,  7.0f, C(166, 160, 150), dkRoof, 0, 4.0f, 1},
        {1510, 410,  90,  62,  7.0f, C(120, 120, 126), dkRoof, 0, 4.0f, 1},
        {1540, 478,  90,  62,  7.0f, C(150, 150, 146), dkRoof, 0, 4.0f, 1},
        {1525, 540, 140, 108,  8.0f, brick,           dkRoof, 0, 6.0f, 2},
        {1545, 648,  65,  62,  6.0f, C(176, 168, 150), dkRoof, 0, 4.0f, 1},
        {1485, 740,  60,  40,  4.0f, C(200, 200, 205), dkRoof, 0, 3.0f, 0},
        // diner
        { 905, 485,  70,  67,  5.0f, C(206, 64, 64),  grey,   0, 4.0f, 1},
        // houses, west side
        { 375, 150, 125, 150,  8.5f, C(150, 72, 60),  C(100, 104, 112), 1, 4.0f, 2},
        { 778, 165,  72, 125,  8.5f, C(190, 190, 192), C(96, 98, 104),  1, 4.0f, 2},
        { 510, 325, 120,  85,  7.5f, C(150, 72, 60),  C(110, 112, 120), 1, 4.0f, 1},
        { 500, 610, 155, 125,  8.5f, C(112, 100, 96), C(70, 66, 64),    1, 4.0f, 2},
        { 868, 718, 147, 120,  8.5f, C(176, 170, 160), C(100, 104, 112), 1, 4.0f, 2},
    };
    MeshBuilder mb;
    PoiBuild(mb, bs, (int)(sizeof(bs) / sizeof(PoiB)), cxp, cyp, mpp);

    // NOMS: sign, striped awning, shelving and checkouts
    { float cx = X(1130 + 55), cz = Z(145 + 66.5f), d = 133 * mpp, w = 110 * mpp;
      for (int i = 0; i < 4; i++) mb.Box({cx - 3.0f + i * 2.0f, 8.8f, cz + d / 2 + 0.3f}, {1.4f, 1.6f, 0.3f}, C(250, 200, 50));
      for (int i = 0; i < 6; i++) mb.Box({cx - w / 2 + 1.0f + i * 2.0f, 3.5f, cz + d / 2 + 0.9f}, {1.0f, 0.12f, 1.8f}, (i & 1) ? C(236, 240, 240) : C(50, 160, 170));
      for (int k = 0; k < 3; k++) Block(mb, {cx - 1.0f, 0.9f, cz - 3.0f + k * 3.2f}, {6.0f, 1.8f, 0.7f}, C(122, 122, 128));
      Block(mb, {cx + 3.5f, 0.55f, cz + d / 2 - 3.0f}, {4.0f, 1.1f, 1.0f}, C(60, 112, 72));
      Block(mb, {cx - 3.5f, 0.55f, cz + d / 2 - 3.0f}, {4.0f, 1.1f, 1.0f}, C(60, 112, 72)); }
    // Big Shots green awning
    { float cx = X(1240 + 59), cz = Z(190 + 32.5f), d = 65 * mpp;
      for (int i = 0; i < 5; i++) mb.Box({cx - 4.0f + i * 2.0f, 3.3f, cz + d / 2 + 0.8f}, {1.0f, 0.12f, 1.6f}, (i & 1) ? C(236, 240, 240) : C(60, 150, 80)); }
    // diner pink domes
    { Matrix m1 = MatrixTranslate(X(962), 0, Z(465)), m2 = MatrixTranslate(X(993), 0, Z(525));
      mb.Loft({{0, 0.25f, 0.25f, C(220, 220, 224)}, {3.0f, 0.25f, 0.25f, C(220, 220, 224)}, {3.0f, 1.6f, 1.6f, C(236, 120, 160)}, {4.2f, 1.2f, 1.2f, C(236, 120, 160)}, {4.8f, 0.0f, 0.0f, C(236, 120, 160)}}, 8, &m1, false, false);
      mb.Loft({{0, 0.25f, 0.25f, C(220, 220, 224)}, {3.0f, 0.25f, 0.25f, C(220, 220, 224)}, {3.0f, 1.6f, 1.6f, C(236, 120, 160)}, {4.2f, 1.2f, 1.2f, C(236, 120, 160)}, {4.8f, 0.0f, 0.0f, C(236, 120, 160)}}, 8, &m2, false, false); }
    // blue pylon
    Scaffold(mb, X(1500), Z(212), 4.5f, 4.5f, 18.0f, C(50, 100, 200));
    // water tower
    { float wx = X(1575), wz = Z(808); Matrix m = MatrixTranslate(wx, 0, wz);
      Color rust = C(150, 92, 60), rust2 = C(176, 112, 72);
      mb.Loft({{0, 0.9f, 0.9f, rust}, {16.0f, 0.9f, 0.9f, rust}}, 8, &m, true, false);
      mb.Loft({{16.0f, 1.2f, 1.2f, rust}, {17.2f, 4.6f, 4.6f, rust2}, {19.0f, 5.0f, 5.0f, rust2}, {21.0f, 4.2f, 4.2f, rust}, {22.6f, 1.2f, 1.2f, rust2}, {23.2f, 0.0f, 0.0f, rust}}, 10, &m, false, false);
      Solid s; s.box = BoxAt({wx, 8.0f, wz}, {2.0f, 16.0f, 2.0f}); solids.push_back(s); }
    // north cliff with a lookout hut on top
    { Color rock = C(166, 166, 172);
      Block(mb, {X(1140), 5.5f, Z(65)}, {600 * mpp, 11.0f, 130 * mpp}, rock);
      Block(mb, {X(1020), 3.0f, Z(30)}, {260 * mpp, 6.0f, 60 * mpp}, Mul(rock, 0.9f));
      mb.Box({X(1180), 12.6f, Z(70)}, {4.0f, 2.4f, 4.0f}, C(120, 84, 56));
      mb.Box({X(1180), 14.2f, Z(70)}, {5.0f, 0.5f, 5.0f}, C(90, 60, 44)); }
    // basketball hoops
    { float cx = X(1042 + 46), z0 = Z(715), z1 = Z(838);
      for (float z : {z0 + 1.0f, z1 - 1.0f}) { mb.Box({cx, 1.6f, z}, {0.15f, 3.2f, 0.15f}, C(60, 60, 66)); mb.Box({cx, 3.1f, z + (z < Z(780) ? 0.4f : -0.4f)}, {1.2f, 0.9f, 0.08f}, C(236, 236, 240)); } }
    // parked cars
    { struct CP { float x, y; bool az; Color c; };
      const CP cars[] = {{1158, 330, true, C(46, 90, 170)}, {1243, 377, true, C(230, 230, 232)}, {1044, 438, true, C(190, 52, 44)}, {1070, 513, true, C(46, 90, 170)},
                         {1262, 510, true, C(150, 80, 170)}, {1420, 632, true, C(200, 60, 56)}, {1155, 578, false, C(226, 190, 50)}, {960, 602, false, C(230, 230, 232)},
                         {1118, 458, false, C(210, 210, 214)}, {1380, 327, true, C(120, 122, 128)}};
      for (const auto& c : cars) Car(mb, X(c.x), Z(c.y), c.az, c.c); }
    mStatic = gR.MakeModel(mb, &gR.texNoise);

    // ---- decals: roads + lots (asphalt), sidewalks, court; painted lines on top
    { MeshBuilder d, l;
      const Color asphalt = C(88, 88, 96), walk = C(196, 194, 186);
      auto road = [&](std::initializer_list<Vector2> px, float wpx) {
          std::vector<Vector2> p; for (auto v : px) p.push_back({X(v.x), Z(v.y)});
          RoadStrip(d, p.data(), (int)p.size(), wpx * mpp, asphalt, true); };
      road({{330, 500}, {700, 500}, {790, 570}, {1000, 600}, {1180, 580}, {1250, 650}}, 46);
      road({{1390, 0}, {1400, 150}, {1390, 300}}, 40);
      road({{1230, 740}, {1230, 1000}, {1000, 1080}}, 46);
      road({{0, 930}, {400, 950}, {420, 760}}, 40);
      road({{700, 500}, {700, 350}, {620, 300}, {540, 240}}, 36);
      FlatQuad(d, X(1040), Z(310), X(1440), Z(700), asphalt, 6.0f);                // main parking lot
      gLots.push_back({{X(1040), 0, Z(310)}, {X(1440), 1, Z(700)}});
      FlatQuad(d, X(1042), Z(715), X(1135), Z(838), C(78, 82, 92), 6.0f);          // basketball court
      gLots.push_back({{X(1042), 0, Z(715)}, {X(1135), 1, Z(838)}});
      FlatQuad(d, X(1050), Z(268), X(1370), Z(300), walk, 4.0f);                   // sidewalk in front of the strip
      FlatQuad(d, X(1440), Z(300), X(1510), Z(700), walk, 4.0f);                   // sidewalk in front of the east row
      RectOutline(l, X(1042), Z(715), X(1135), Z(838), 0.12f, C(232, 232, 236));   // court lines
      FlatQuad(l, X(1086), Z(715), X(1090), Z(838), C(232, 232, 236), 4.0f);
      auto bays = [&](float xa, float xb, float ya, float yb) { for (float x = xa; x <= xb; x += 24) FlatQuad(l, X(x), Z(ya), X(x) + 0.12f, Z(yb), C(220, 220, 214), 4.0f); };
      bays(1060, 1420, 322, 372); bays(1180, 1340, 430, 470); bays(1180, 1420, 500, 545);
      mDecal = gR.MakeModel(d, &gR.texNoise); mDecalTop = gR.MakeModel(l, &gR.texNoise); }

    ScatterTrees(62, 3.0f);
    for (int i = 0; i < 14; i++) { float x = RandF(-half + 8, half - 8), z = RandF(-half + 8, half - 8); if (Free(x, z, 3.0f) && !OnRoadOrLot(x, z, 2.0f)) AddCrate(x, 0, z, 1.6f); }
    for (int i = 0; i < 12; i++) { float a = i * kPi / 6 + 0.2f; spawns.push_back({cosf(a) * half * 0.9f, 0, sinf(a) * half * 0.9f}); }
    ScatterLoot(-70, 70, -60, 60, 10, 0);
}

// ----------------------------------------------------------------------------------------- SALTY SPRINGS
// Traced from the overhead shot (1280x835). metresPerPixel ~0.10.
void World::BuildSalty() {
    half = 70.0f;
    const float cxp = 640, cyp = 417.5f, mpp = 0.10f;
    gRoadSegs.clear(); gLots.clear();
    auto X = [&](float px) { return (px - cxp) * mpp; };
    auto Z = [&](float py) { return (py - cyp) * mpp; };
    { MeshBuilder g; g.Quad({-300, 0, 300}, {300, 0, 300}, {300, 0, -300}, {-300, 0, -300}, C(255, 255, 255), 8.0f); mGround = gR.MakeModel(g, &gR.texGrass); }

    const Color brown = C(130, 70, 58), greyRoof = C(94, 98, 106);
    const PoiB bs[] = {
        { 643, 238,  82,  57,  5.0f, C(212, 208, 198), C(60, 150, 150), 0, 4.0f, 1},   // gas station
        { 368, 160, 124, 102,  9.0f, C(152, 80, 66),   greyRoof,        1, 4.0f, 2},   // brick house, grey roof
        { 174, 345, 110,  95,  9.0f, C(150, 200, 230), brown,           1, 4.0f, 2},   // light-blue clapboard house
        { 283, 380,  40,  30,  6.0f, C(150, 200, 230), brown,           1, 3.5f, 0},   //   its side wing
        { 563, 497,  60,  90,  6.0f, C(150, 140, 120), brown,           1, 3.5f, 1},   // stone single-storey
        { 200, 575, 115,  93,  8.0f, C(176, 170, 160), brown,           1, 4.0f, 2},
        { 800, 412,  97, 120,  9.0f, C(170, 82, 68),   greyRoof,        1, 4.0f, 2},   // east house
        {  17, 545,  38,  35,  3.5f, C(180, 180, 184), C(110, 110, 115), 0, 2.5f, 0},  // shed
        {1150, 425, 125,  63,  4.5f, C(122, 86, 54),   C(90, 62, 44),   0, 3.5f, 1},   // wooden structure
        {1118, 737,  64,  75,  5.5f, C(132, 92, 62),   C(120, 64, 52),  1, 3.5f, 1},   // cabin
    };
    MeshBuilder mb;
    PoiBuild(mb, bs, (int)(sizeof(bs) / sizeof(PoiB)), cxp, cyp, mpp);
    // picket fences round the two yards
    Fence(mb, X(205 + 72.5f), Z(355 + 57.5f), 145 * mpp, 115 * mpp, C(232, 232, 238));
    Fence(mb, X(790 + 55), Z(400 + 65), 110 * mpp, 130 * mpp, C(232, 232, 238));
    // gas station: teal trim band + sign
    { float cx = X(643 + 41), cz = Z(238 + 28.5f);
      mb.Box({cx, 4.2f, cz + 28.5f * mpp + 0.25f}, {82 * mpp, 0.5f, 0.15f}, C(60, 170, 170));
      mb.Box({cx, 6.5f, cz}, {1.6f, 2.0f, 0.3f}, C(230, 60, 50)); }
    // rock formation on the east side
    { Color rock = C(142, 142, 148);
      Block(mb, {X(965), 2.0f, Z(380)}, {110 * mpp, 4.0f, 160 * mpp}, rock);
      Block(mb, {X(975), 1.5f, Z(530)}, {100 * mpp, 3.0f, 150 * mpp}, Mul(rock, 0.92f));
      Block(mb, {X(952), 1.8f, Z(665)}, {95 * mpp, 3.6f, 130 * mpp}, Mul(rock, 1.05f)); }
    // campfire in the cul-de-sac
    Block(mb, {X(477), 0.3f, Z(350)}, {1.0f, 0.6f, 1.0f}, C(210, 100, 40));
    // a few cars
    Car(mb, X(330), Z(492), false, C(150, 150, 156)); Car(mb, X(722), Z(590), true, C(190, 52, 44)); Car(mb, X(862), Z(690), true, C(226, 190, 50));
    mStatic = gR.MakeModel(mb, &gR.texNoise);

    { MeshBuilder d;
      const Color asphalt = C(92, 92, 100);
      auto road = [&](std::initializer_list<Vector2> px, float wpx) {
          std::vector<Vector2> p; for (auto v : px) p.push_back({X(v.x), Z(v.y)});
          RoadStrip(d, p.data(), (int)p.size(), wpx * mpp, asphalt, true); };
      road({{0, 330}, {80, 470}, {190, 520}, {430, 520}, {560, 480}, {700, 490}, {740, 560}, {850, 720}, {940, 835}}, 50);
      road({{700, 490}, {700, 330}, {730, 250}, {860, 150}, {960, 40}, {990, 0}}, 44);
      road({{430, 520}, {440, 430}, {395, 420}, {395, 330}, {460, 300}, {530, 330}, {530, 400}, {440, 430}}, 34);
      FlatQuad(d, X(405), Z(310), X(525), Z(420), C(120, 118, 112), 4.0f);          // cul-de-sac pad
      mDecal = gR.MakeModel(d, &gR.texNoise); }

    ScatterTrees(70, 2.6f);
    for (int i = 0; i < 8; i++) { float x = RandF(-half + 6, half - 6), z = RandF(-half + 6, half - 6); if (Free(x, z, 3.0f) && !OnRoadOrLot(x, z, 2.0f)) AddCrate(x, 0, z, 1.6f); }
    for (int i = 0; i < 12; i++) { float a = i * kPi / 6 + 0.2f; spawns.push_back({cosf(a) * half * 0.9f, 0, sinf(a) * half * 0.9f}); }
    ScatterLoot(-50, 50, -40, 40, 8, 0);
}

// ----------------------------------------------------------------------------------------- build / unload
void World::Build(MapId m) {
    Unload();
    id = m;
    SetRandomSeed(m == MAP_DUSTY ? 2017 : m == MAP_CONTAINER ? 1117 : m == MAP_SALTY ? 3301 : 4412);
    BuildShared();
    if (m == MAP_DUSTY) BuildDusty(); else if (m == MAP_CONTAINER) BuildYard(); else if (m == MAP_SALTY) BuildSalty(); else BuildRetail();
    SetRandomSeed((unsigned)time(nullptr));
    loaded = true;
}

void World::Unload() {
    if (loaded) {
        gR.FreeModel(mGround); gR.FreeModel(mDecal); gR.FreeModel(mDecalTop); gR.FreeModel(mStormHaze); gR.FreeModel(mStatic); gR.FreeModel(mCrate); gR.FreeModel(mTree); gR.FreeModel(mContainer);
        gR.FreeModel(mChestBody); gR.FreeModel(mChestLid); gR.FreeModel(mStorm);
        for (int i = 0; i < P_COUNT; i++) gR.FreeModel(mPiece[i]);
    }
    solids.clear(); pieces.clear(); chests.clear(); drops.clear(); spawns.clear();
    loaded = false;
}

void World::Update(float dt) {
    for (auto& c : chests) if (c.opened && c.t < 1) c.t = fminf(1.0f, c.t + dt * 3.0f);
    for (auto& d : drops) d.spin += dt * 90.0f;
}

// ----------------------------------------------------------------------------------------- drawing
void World::Draw(Vector3 cam, float stormR, const Camera3D& view) {
    const Vector3 up = {0, 1, 0};
    DrawModel(mGround, {0, 0, 0}, 1, WHITE);
    // floor layers: everything is at y = 0; decals are only biased in depth while drawn (no z-fighting, no step)
    if (mDecal.meshCount > 0)    { gR.DecalBegin(-1.0f, -1.0f); DrawModel(mDecal, {0, 0, 0}, 1, WHITE); gR.DecalEnd(); }
    if (mDecalTop.meshCount > 0) { gR.DecalBegin(-2.0f, -2.0f); DrawModel(mDecalTop, {0, 0, 0}, 1, WHITE); gR.DecalEnd(); }
    DrawModel(mStatic, {0, 0, 0}, 1, WHITE);
    for (const auto& s : solids) {
        if (!s.alive || s.kind == SK_STATIC) continue;
        float dx = s.pos.x - cam.x, dz = s.pos.z - cam.z;
        if (dx * dx + dz * dz > 120.0f * 120.0f) continue;
        if (s.kind == SK_CRATE) DrawModelEx(mCrate, s.pos, up, s.yaw, s.scale, WHITE);
        else if (s.kind == SK_TREE) DrawModelEx(mTree, s.pos, up, s.yaw, s.scale, WHITE);
        else DrawModelEx(mContainer, s.pos, up, s.yaw, {1, 1, 1}, s.tint);
    }
    for (const auto& p : pieces) if (p.alive) DrawModelEx(mPiece[p.type], p.pos, up, p.dir * 90.0f, {1, 1, 1}, kMatColor[p.mat]);
    float tt = (float)GetTime();
    for (const auto& c : chests) {
        DrawModelEx(mChestBody, c.pos, up, c.yaw, {1, 1, 1}, WHITE);
        Model lid = mChestLid; lid.transform = MatrixMultiply(MatrixRotateX(-1.6f * c.t), MatrixTranslate(0, 0.44f, -0.3f));
        DrawModelEx(lid, c.pos, up, c.yaw, {1, 1, 1}, WHITE);
        if (!c.opened) DrawCylinderEx({c.pos.x, 0, c.pos.z}, {c.pos.x, 3.0f, c.pos.z}, 0.28f, 0.05f, 4, Color{255, 220, 80, 70});
    }
    for (const auto& d : drops) {
        if (!d.alive) continue;
        const ItemDef& def = GetItemDef(d.item.id);
        float bob = sinf(tt * 2.0f + d.spin) * 0.08f; Color rc = RarityColor(d.item.rar);
        Vector3 p = {d.pos.x, 0.55f + bob, d.pos.z};
        
        // 1. Draw the glow/beam FIRST so it is in the background
        DrawCylinderEx({p.x, 0, p.z}, {p.x, 2.4f, p.z}, 0.30f, 0.05f, 4, Color{rc.r, rc.g, rc.b, 80});
        
        // 2. Draw the icon ON TOP of the glow
        const Texture2D* tex = gIcons.Item(def.name);
        if (tex && tex->id != 0) {
            DrawBillboard(view, *tex, p, 0.75f, WHITE);
        } 
        // Fallback if the PNG is missing
        else if (def.kind == K_WEAPON) { 
            Model g = *gunModel; g.transform = MatrixIdentity(); 
            DrawModelEx(g, {p.x, p.y, p.z}, up, d.spin, {1.3f, 1.3f, 1.3f}, Mix(WHITE, rc, 0.55f)); 
        }
        else if (def.kind == K_AMMO) DrawCubeV(p, {0.35f, 0.22f, 0.22f}, def.icon);
        else DrawSphereEx(p, 0.2f, 5, 5, def.icon);
    }
}

// Storm: translucent, so it is drawn last with depth writes off. Haze on the ground outside + the animated wall.
void World::DrawStorm(float stormR, const Camera3D&) {
    if (stormR >= 300) return;
    const Vector3 up = {0, 1, 0};
    float t = (float)GetTime();
    SetShaderValue(gR.stormShader, gR.locStormTime, &t, SHADER_UNIFORM_FLOAT);
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    gR.DecalBegin(-3.0f, -3.0f);
    DrawModelEx(mStormHaze, {0, 0, 0}, up, 0, {stormR, 1, stormR}, WHITE);
    gR.DecalEnd();
    DrawModelEx(mStorm, {0, 0, 0}, up, 0, {stormR, 1, stormR}, WHITE);
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
}

// ----------------------------------------------------------------------------------------- physics
bool World::Blocked(Vector3 p, float r, float h, float step) const {
    AABB pb = {{p.x - r, p.y, p.z - r}, {p.x + r, p.y + h, p.z + r}};
    auto test = [&](const AABB& b) { return Overlap(pb, b) && b.max.y > p.y + step && b.min.y < p.y + h; };
    for (const auto& s : solids) if (s.alive && test(s.box)) return true;
    for (const auto& pc : pieces) if (pc.alive && pc.type != P_STAIR && test(pc.box)) return true;
    return false;
}

Vector3 World::MoveSlide(Vector3 pos, Vector3 d, float r, float h, float step) const {
    Vector3 np = pos;
    np.x += d.x; if (Blocked(np, r, h, step)) np.x = pos.x;
    np.z += d.z; if (Blocked(np, r, h, step)) np.z = pos.z;
    return np;
}

float World::GroundY(Vector3 p, float footY, float step) const {
    float y = 0.0f, lim = footY + step;
    auto consider = [&](const AABB& b) {
        if (p.x > b.min.x - 0.2f && p.x < b.max.x + 0.2f && p.z > b.min.z - 0.2f && p.z < b.max.z + 0.2f && b.max.y <= lim) y = fmaxf(y, b.max.y);
    };
    for (const auto& s : solids) if (s.alive) consider(s.box);
    for (const auto& pc : pieces) {
        if (!pc.alive) continue;
        if (pc.type != P_STAIR) { consider(pc.box); continue; }
        Vector3 dv = DirVec(pc.dir);
        float along = (p.x - pc.pos.x) * dv.x + (p.z - pc.pos.z) * dv.z + 2.0f;
        float lat = fabsf((p.x - pc.pos.x) * dv.z - (p.z - pc.pos.z) * dv.x);
        if (lat < 2.1f && along >= 0 && along < 4.0f) { float h = pc.pos.y + ((int)(along / 0.5f) + 1) * 0.5f; if (h <= lim) y = fmaxf(y, h); }
    }
    return y;
}

RayHit World::Raycast(Ray r, float maxD) const {
    RayHit best; best.dist = maxD;
    auto test = [&](const AABB& b, int si, int pi) {
        RayCollision c = GetRayCollisionBox(r, BoundingBox{b.min, b.max});
        if (c.hit && c.distance >= 0 && c.distance < best.dist) { best.hit = true; best.dist = c.distance; best.point = c.point; best.solid = si; best.piece = pi; }
    };
    for (size_t i = 0; i < solids.size(); i++) if (solids[i].alive) test(solids[i].box, (int)i, -1);
    for (size_t i = 0; i < pieces.size(); i++) if (pieces[i].alive) test(pieces[i].box, -1, (int)i);
    if (r.direction.y < -1e-4f) {
        float t = -r.position.y / r.direction.y;
        if (t > 0 && t < best.dist) { best.hit = true; best.dist = t; best.point = Vector3Add(r.position, Vector3Scale(r.direction, t)); best.solid = -1; best.piece = -1; }
    }
    return best;
}

// ----------------------------------------------------------------------------------------- building
AABB World::PieceBox(PieceType t, Vector3 pos, int dir) const {
    switch (t) {
        case P_WALL:  return (dir & 1) ? BoxAt(pos, {0.25f, 4, 4}) : BoxAt(pos, {4, 4, 0.25f});
        case P_FLOOR: return BoxAt(pos, {4, 0.25f, 4});
        case P_STAIR: return BoxAt({pos.x, pos.y + 2, pos.z}, {4, 4, 4});
        default:      return BoxAt({pos.x, pos.y - 0.125f, pos.z}, {4, 0.25f, 4});
    }
}

void World::GetPlacement(PieceType t, Vector3 feet, float yaw, Vector3& pos, int& dir) const {
    dir = DirFromYaw(yaw);
    Vector3 dv = DirVec(dir);
    float base = roundf(feet.y / 4.0f) * 4.0f;
    float cx = floorf(feet.x / 4.0f) * 4.0f + 2.0f, cz = floorf(feet.z / 4.0f) * 4.0f + 2.0f;
    switch (t) {
        case P_WALL:  pos = {cx + dv.x * 2, base + 2, cz + dv.z * 2}; break;
        case P_FLOOR: pos = {cx + dv.x * 4, base - 0.125f, cz + dv.z * 4}; break;
        case P_STAIR: pos = {cx + dv.x * 4, base, cz + dv.z * 4}; break;
        default:      pos = {cx + dv.x * 4, base + 4, cz + dv.z * 4}; break;
    }
}

bool World::Place(PieceType t, Mat m, Vector3 feet, float yaw) {
    Vector3 pos; int dir; GetPlacement(t, feet, yaw, pos, dir);
    if (fabsf(pos.x) > half || fabsf(pos.z) > half) return false;
    for (const auto& p : pieces)
        if (p.alive && p.type == t && (t != P_WALL || (p.dir & 1) == (dir & 1)) && (t != P_STAIR || p.dir == dir) && Vector3Distance(p.pos, pos) < 0.5f) return false;
    Piece pc{t, m, pos, dir, 150.0f, PieceBox(t, pos, dir), true};
    pieces.push_back(pc);
    return true;
}