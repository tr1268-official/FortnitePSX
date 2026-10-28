#include "Skydive.h"
#include "SkyGeo.h"
#include "Renderer.h"

using namespace skygeo;

SkyHud gSkyHud;

static Model Mk(MeshBuilder& mb) { return gR.MakeModel(mb); }
static void Part(const Model& m, const Matrix& mat, Color tint) {
    Model t = m; t.transform = mat;
    DrawModel(t, Vector3{0, 0, 0}, 1.0f, tint);
}
static Matrix At(Matrix local, const Matrix& parent) { return MatrixMultiply(local, parent); }

// ---------------------------------------------------------------------------------------- route + flight
void BattleBus::Start(float half, float altitude) {
    const float th = RandF(0.0f, 2.0f * kPi);
    dir = {sinf(th), 0.0f, cosf(th)};
    const Vector3 perp = {cosf(th), 0.0f, -sinf(th)};
    const float off = RandF(-0.28f, 0.28f) * half;     // route does not always cross the exact centre
    const float ext = half * 1.55f;                    // starts / ends well outside the map square
    const Vector3 mid = Vector3Scale(perp, off);
    start = Vector3Subtract(mid, Vector3Scale(dir, ext));
    end   = Vector3Add(mid, Vector3Scale(dir, ext));
    length = 2.0f * ext;

    // Where does the route enter / leave the square [-half, half]^2 ?  (slab clipping)
    float t0 = 0.0f, t1 = length;
    auto slab = [&](float p0, float d) {
        if (fabsf(d) < 1e-5f) { if (fabsf(p0) > half) { t0 = 1.0f; t1 = 0.0f; } return; }
        float a = (-half - p0) / d, b = (half - p0) / d;
        if (a > b) std::swap(a, b);
        t0 = fmaxf(t0, a); t1 = fminf(t1, b);
    };
    slab(start.x, dir.x); slab(start.z, dir.z);
    if (t1 <= t0) { t0 = length * 0.3f; t1 = length * 0.7f; }          // degenerate safety net
    mapEnter = t0; mapLeave = t1;

    speed = fmaxf(Sky::kMinBusSpeed, (mapLeave - mapEnter) / Sky::kCrossTime);
    alt = altitude;
    travel = 0; wheelSpin = 0; propSpin = 0; sway = 0; bobT = 0;
    yaw = atan2f(dir.x, dir.z);
    pos = {start.x, alt, start.z};
    active = true;
}

void BattleBus::Update(float dt) {
    if (!active) return;
    travel += speed * dt;
    bobT += dt; sway += dt * 1.1f; propSpin += dt * 26.0f; wheelSpin += speed * dt / 0.55f;
    pos = {start.x + dir.x * travel, alt + sinf(bobT * 1.3f) * 0.25f, start.z + dir.z * travel};
}

float BattleBus::MapProgress() const {
    return Clamp((travel - mapEnter) / fmaxf(1.0f, mapLeave - mapEnter), 0.0f, 1.0f);
}

// ---------------------------------------------------------------------------------------- models
// Bus space: +Z = front, +Y = up, origin on the ground line under the body centre. ~8.6 long, 3.1 wide, roof at y = 3.0.
static void BuildBusBody(MeshBuilder& mb) {
    const Color blue = C(52, 118, 226), blueD = C(34, 82, 176), light = C(96, 170, 246), white = C(238, 240, 246);
    const Color glass = C(30, 48, 74), steel = C(128, 134, 146), dark = C(38, 40, 48), yellow = C(250, 206, 52);
    const Color red = C(212, 52, 52), lamp = C(255, 244, 178);

    mb.Box({0, 0.45f, 0}, {2.9f, 0.7f, 8.0f}, dark);                                    // chassis
    mb.Box({0, 1.25f, -0.1f}, {3.1f, 1.1f, 8.2f}, blue);                                // lower body
    for (int s = -1; s <= 1; s += 2) mb.Box({s * 1.57f, 1.30f, -0.1f}, {0.05f, 0.22f, 8.0f}, yellow);   // belt stripe
    mb.Box({0, 2.35f, -0.55f}, {2.95f, 1.2f, 6.9f}, light);                             // cabin
    mb.Box({0, 3.0f, -0.55f}, {2.7f, 0.14f, 6.6f}, white);                              // roof
    mb.Box({0, 3.09f, -0.55f}, {0.5f, 0.06f, 5.6f}, blueD);                             // roof spine

    for (int s = -1; s <= 1; s += 2)                                                    // side windows
        for (int k = 0; k < 7; k++) mb.Box({s * 1.49f, 2.4f, -3.3f + k * 0.95f}, {0.05f, 0.72f, 0.72f}, glass);
    mb.Box({1.58f, 1.35f, 2.2f}, {0.03f, 1.0f, 0.95f}, blueD);                          // doors
    mb.Box({-1.58f, 1.35f, 2.2f}, {0.03f, 1.0f, 0.95f}, blueD);

    // front: hood, fascia, windshield, lights
    mb.Box({0, 2.0f, 3.5f}, {2.8f, 0.55f, 1.0f}, blue);
    mb.Box({0, 1.25f, 4.05f}, {3.0f, 1.1f, 0.12f}, blueD);
    mb.Box({0, 1.1f, 4.13f}, {1.4f, 0.5f, 0.05f}, dark);
    for (int s = -1; s <= 1; s += 2) mb.Box({s * 1.05f, 1.5f, 4.13f}, {0.45f, 0.28f, 0.06f}, lamp);
    mb.Box({0, 0.72f, 4.2f}, {3.2f, 0.22f, 0.3f}, steel);                               // bumper
    { Matrix ws = MatrixMultiply(MatrixRotateX(-0.30f), MatrixTranslate(0, 2.62f, 2.97f));
      mb.Box({0, 0, 0}, {2.6f, 0.95f, 0.06f}, glass, &ws); }
    for (int s = -1; s <= 1; s += 2) {                                                  // mirrors
        mb.Box({s * 1.62f, 2.15f, 2.95f}, {0.28f, 0.05f, 0.05f}, dark);
        mb.Box({s * 1.76f, 2.2f, 2.95f}, {0.07f, 0.4f, 0.16f}, dark);
    }

    // rear
    mb.Box({0, 1.25f, -4.22f}, {3.0f, 1.1f, 0.1f}, blueD);
    mb.Box({0, 2.4f, -4.03f}, {2.4f, 0.7f, 0.05f}, glass);
    for (int s = -1; s <= 1; s += 2) mb.Box({s * 1.2f, 1.5f, -4.28f}, {0.3f, 0.2f, 0.05f}, red);
    mb.Box({0, 0.72f, -4.35f}, {3.2f, 0.22f, 0.3f}, steel);
    mb.Box({0, 1.95f, -4.65f}, {0.16f, 0.16f, 0.6f}, dark);                             // propeller mast

    // axles + roof rope plates
    mb.Box({0, 0.55f, 2.5f}, {3.4f, 0.14f, 0.14f}, dark);
    mb.Box({0, 0.55f, -2.4f}, {3.4f, 0.14f, 0.14f}, dark);
    for (int sx = -1; sx <= 1; sx += 2) for (int sz = -1; sz <= 1; sz += 2)
        mb.Box({sx * 1.1f, 3.12f, sz * 2.7f}, {0.26f, 0.1f, 0.26f}, steel);
}

static void BuildBusWheel(MeshBuilder& mb) {          // axis = X, origin at the hub
    const Color tyre = C(30, 30, 34), hub = C(170, 176, 188), spoke = C(214, 218, 226);
    Matrix xf = MatrixRotateZ(kPi / 2);
    mb.Loft({{-0.20f, 0.40f, 0.40f, tyre}, {-0.20f, 0.55f, 0.55f, tyre}, {0.20f, 0.55f, 0.55f, tyre}, {0.20f, 0.40f, 0.40f, tyre}}, 10, &xf, true, true);
    mb.Loft({{0.20f, 0.40f, 0.40f, hub}, {0.215f, 0.40f, 0.40f, hub}, {0.215f, 0.12f, 0.12f, hub}, {0.24f, 0.10f, 0.10f, hub}}, 10, &xf, false, true);
    mb.Loft({{-0.24f, 0.10f, 0.10f, hub}, {-0.215f, 0.12f, 0.12f, hub}, {-0.215f, 0.40f, 0.40f, hub}, {-0.20f, 0.40f, 0.40f, hub}}, 10, &xf, true, false);
    for (int k = 0; k < 4; k++)                                                          // spokes make the spin readable
        for (int s = -1; s <= 1; s += 2) {
            Matrix m = MatrixRotateX(k * kPi / 4);
            mb.Box({s * 0.235f, 0, 0}, {0.03f, 0.74f, 0.07f}, spoke, &m);
        }
}

static void BuildBusFan(MeshBuilder& mb) {            // blades in the XY plane, axis = Z, origin at the hub
    const Color blade = C(236, 238, 244), tip = C(250, 206, 52), hub = C(60, 64, 74);
    for (int k = 0; k < 4; k++) {
        Matrix m = MatrixRotateZ(k * kPi / 2);
        mb.Box({0, 0.72f, 0}, {0.2f, 1.3f, 0.05f}, blade, &m);
        mb.Box({0, 1.32f, 0}, {0.2f, 0.16f, 0.055f}, tip, &m);
    }
    Matrix xf = MatrixRotateX(kPi / 2);
    mb.Loft({{-0.10f, 0.13f, 0.13f, hub}, {0.14f, 0.13f, 0.13f, hub}, {0.20f, 0.0f, 0.0f, hub}}, 8, &xf, true, false);
}

static void BuildBusBalloon(MeshBuilder& mb) {        // origin = roof centre; ropes then the balloon above
    const Color rope = C(72, 54, 40);
    for (int sx = -1; sx <= 1; sx += 2) for (int sz = -1; sz <= 1; sz += 2)
        Strut(mb, {sx * 1.1f, 0.0f, sz * 2.7f}, {sx * 0.6f, 4.2f, sz * 0.6f}, 0.05f, rope, 5);

    const std::vector<Prof> prof = {{0.0f, 0.8f}, {0.7f, 1.9f}, {2.2f, 3.9f}, {4.6f, 5.3f}, {7.2f, 5.6f}, {9.6f, 4.9f}, {11.6f, 3.4f}, {13.1f, 1.8f}, {13.9f, 0.0f}};
    const Color blue = C(58, 124, 232), white = C(238, 242, 250), skirt = C(40, 44, 62), cap = C(236, 92, 52);
    Gores(mb, prof, 14, Vector3{0, 4.2f, 0}, false, [&](int g, int j, int segs) {
        if (j == 0) return skirt;
        if (j == segs - 1) return cap;
        Color c = (g % 2 == 0) ? blue : white;
        return Mul(c, 0.88f + 0.12f * (float)j / (float)segs);      // slightly lighter towards the top
    });
    mb.Loft({{4.2f, 0.82f, 0.82f, skirt}, {4.5f, 0.82f, 0.82f, skirt}}, 10, nullptr, false, false);   // neck collar
}

// ---------------------------------------------------------------------------------------- public: build / draw
void SkydiveBuild(SkyModels& m) {
    { MeshBuilder mb; BuildBusBody(mb);   m.busBody = Mk(mb); }
    { MeshBuilder mb; BuildBusWheel(mb);  m.busWheel = Mk(mb); }
    { MeshBuilder mb; BuildBusBalloon(mb); m.busBalloon = Mk(mb); }
    { MeshBuilder mb; BuildBusFan(mb);    m.busFan = Mk(mb); }
    GliderBuild(m);
}

void SkydiveUnload(SkyModels& m) {
    gR.FreeModel(m.busBody); gR.FreeModel(m.busWheel); gR.FreeModel(m.busBalloon); gR.FreeModel(m.busFan);
    GliderUnload(m);
}

void DrawBattleBus(const SkyModels& m, const BattleBus& b, Color tint) {
    if (!b.active) return;
    Matrix root = MatrixMultiply(MatrixRotateY(b.yaw), MatrixTranslate(b.pos.x, b.pos.y, b.pos.z));
    Part(m.busBody, root, tint);

    static const float wz[2] = {2.5f, -2.4f};
    for (int s = -1; s <= 1; s += 2)
        for (int k = 0; k < 2; k++)
            Part(m.busWheel, At(MatrixMultiply(MatrixRotateX(b.wheelSpin), MatrixTranslate(s * 1.72f, 0.55f, wz[k])), root), tint);

    const float swA = sinf(b.sway) * 0.025f, swB = sinf(b.sway * 0.7f + 1.0f) * 0.02f;   // balloon rocks gently
    Part(m.busBalloon, At(MatrixMultiply(MatrixMultiply(MatrixRotateZ(swA), MatrixRotateX(swB)), MatrixTranslate(0, 3.16f, 0)), root), tint);
    Part(m.busFan, At(MatrixMultiply(MatrixRotateZ(b.propSpin), MatrixTranslate(0, 1.95f, -4.95f)), root), tint);
}
