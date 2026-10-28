#include "Skydive.h"
#include "SkyGeo.h"
#include "Renderer.h"

using namespace skygeo;

// Glider space: origin = the point the pilot's hands hold (about 2.05 above the character's feet),
// +Z = forward, +Y = up. Wingspan about 3.9. Each glider is one static mesh.

static Model Mk(MeshBuilder& mb) { return gR.MakeModel(mb); }

static const char* kGliderNames[kGliderCount] = {"Default Glider", "Mako", "Founder's Glider"};
const char* GliderName(int id) { return kGliderNames[(id < 0 || id >= kGliderCount) ? 0 : id]; }
float GliderGripSplay(int id) { return id == 2 ? -0.30f : 0.30f; }      // umbrella: hands together on one handle

// Grip bar at the origin with two uprights going up to the glider.
static void ControlBar(MeshBuilder& mb, float halfBar, Vector3 attachL, Vector3 attachR) {
    const Color tube = C(176, 182, 192), grip = C(40, 40, 46);
    Strut(mb, Vector3{-halfBar, 0.0f, 0.0f}, Vector3{halfBar, 0.0f, 0.0f}, 0.024f, grip, 6);
    Strut(mb, Vector3{-halfBar, 0.0f, 0.0f}, attachL, 0.02f, tube, 5);
    Strut(mb, Vector3{halfBar, 0.0f, 0.0f}, attachR, 0.02f, tube, 5);
}

// ---------------------------------------------------------------------------------------- Default glider
// Classic delta hang-glider: cream sail, orange wing tips, steel frame.
static void BuildDefaultGlider(MeshBuilder& mb) {
    const Vector3 N = {0.0f, 1.78f, 1.55f}, T = {0.0f, 1.52f, -1.25f};
    const Color cream = C(238, 234, 222), creamB = C(204, 199, 188), accent = C(214, 84, 48), accentB = C(168, 66, 40), steel = C(176, 182, 192);
    Vector3 tips[2];
    for (int i = 0; i < 2; i++) {
        const float s = (i == 0) ? -1.0f : 1.0f;
        const Vector3 tip = {s * 1.95f, 1.30f, -1.05f};
        tips[i] = tip;
        const Vector3 P = Vector3Lerp(N, tip, 0.62f), Q = Vector3Lerp(T, tip, 0.62f);
        SailQuad(mb, N, P, Q, T, cream, creamB);                         // inner sail
        SailTri(mb, P, tip, Q, accent, accentB);                         // coloured wing tip
        Strut(mb, N, tip, 0.03f, steel, 5);                              // leading edge
        Strut(mb, Vector3Lerp(N, tip, 0.30f), Vector3Lerp(T, tip, 0.30f), 0.014f, steel, 4);   // batten
        Strut(mb, Vector3Lerp(N, tip, 0.80f), Vector3Lerp(T, tip, 0.80f), 0.014f, steel, 4);
    }
    Strut(mb, N, T, 0.035f, steel, 5);                                   // keel
    mb.Box({0.0f, 1.80f, 1.55f}, {0.06f, 0.06f, 0.14f}, accent);         // nose cap
    ControlBar(mb, 0.40f, Vector3Lerp(N, tips[0], 0.40f), Vector3Lerp(N, tips[1], 0.40f));
}

// ---------------------------------------------------------------------------------------- Mako
// Shark: slate back, white belly, dorsal + tail fins, pectoral fins as the wing, gills, eyes and teeth.
static void BuildMakoGlider(MeshBuilder& mb) {
    const Color slate = C(72, 98, 130), slateD = C(46, 64, 92), belly = C(224, 230, 236), dark = C(24, 28, 38), white = C(246, 246, 248);
    const float Y = 1.60f;

    struct R { float z, w, h; };
    const R prof[] = {{1.80f, 0.0f, 0.0f}, {1.70f, 0.09f, 0.08f}, {1.35f, 0.21f, 0.18f}, {0.70f, 0.30f, 0.25f},
                      {0.00f, 0.34f, 0.28f}, {-0.80f, 0.28f, 0.23f}, {-1.40f, 0.16f, 0.14f}, {-1.90f, 0.06f, 0.07f}};
    {   // body: ring axis (local Y) becomes the shark's length (Z)
        std::vector<Ring> rings;
        for (const R& r : prof) rings.push_back({r.z, r.w, r.h, r.z < -1.5f ? slateD : slate});
        Matrix xf = MatrixMultiply(MatrixRotateX(kPi / 2), MatrixTranslate(0.0f, Y, 0.0f));
        mb.Loft(rings, 10, &xf, true, true);
    }
    {   // pale belly: a narrower, flatter copy pushed down so only a white ridge shows underneath
        std::vector<Ring> rings;
        for (const R& r : prof) rings.push_back({r.z, r.w * 0.6f, r.h * 0.8f, belly});
        Matrix xf = MatrixMultiply(MatrixRotateX(kPi / 2), MatrixTranslate(0.0f, Y - 0.07f, 0.0f));
        mb.Loft(rings, 10, &xf, false, false);
    }

    // fins
    TriBoth(mb, {0.0f, Y + 0.22f, 0.50f}, {0.0f, Y + 0.22f, -0.60f}, {0.0f, Y + 0.85f, -0.70f}, slateD, slateD);   // dorsal
    TriBoth(mb, {0.0f, Y, -1.75f}, {0.0f, Y + 0.55f, -2.40f}, {0.0f, Y + 0.05f, -2.15f}, slateD, slateD);       // tail, upper lobe
    TriBoth(mb, {0.0f, Y, -1.75f}, {0.0f, Y - 0.50f, -2.30f}, {0.0f, Y - 0.05f, -2.10f}, slateD, slateD);       // tail, lower lobe

    Vector3 attach[2];
    for (int i = 0; i < 2; i++) {
        const float s = (i == 0) ? -1.0f : 1.0f;
        const Vector3 F = {s * 0.26f, Y - 0.02f, 0.85f}, B = {s * 0.26f, Y - 0.02f, -0.45f}, tip = {s * 1.95f, Y - 0.12f, -1.20f};
        const Vector3 P = Vector3Lerp(F, tip, 0.60f), Q = Vector3Lerp(B, tip, 0.60f);
        SailQuad(mb, F, P, Q, B, slate, belly);                          // pectoral fin = the wing
        SailTri(mb, P, tip, Q, slateD, belly);
        SailTri(mb, {s * 0.18f, Y - 0.08f, -1.00f}, {s * 0.75f, Y - 0.14f, -1.65f}, {s * 0.12f, Y - 0.06f, -1.50f}, slateD, belly);   // pelvic fin
        mb.Box({s * 0.215f, Y + 0.07f, 1.28f}, {0.04f, 0.05f, 0.05f}, dark);                                                          // eye
        for (int g = 0; g < 3; g++) mb.Box({s * 0.325f, Y + 0.02f, 0.55f - g * 0.13f}, {0.015f, 0.14f, 0.02f}, dark);              // gills
        attach[i] = {s * 0.27f, Y - 0.16f, 0.45f};
    }
    for (int k = 0; k < 4; k++) mb.Box({-0.07f + k * 0.047f, Y - 0.13f, 1.52f}, {0.02f, 0.05f, 0.03f}, white);                      // teeth
    ControlBar(mb, 0.40f, attach[0], attach[1]);
}

// ---------------------------------------------------------------------------------------- Founder's
// Umbrella: navy / ivory gores with a gold hem, gold pole + finial, leather handle.
static void BuildFounderGlider(MeshBuilder& mb) {
    const Color navy = C(34, 58, 124), ivory = C(240, 228, 192), gold = C(226, 178, 62), leather = C(70, 48, 34), rib = C(70, 74, 84);
    const std::vector<Prof> prof = {{2.00f, 1.86f}, {2.14f, 1.72f}, {2.36f, 1.32f}, {2.52f, 0.82f}, {2.60f, 0.32f}, {2.64f, 0.0f}};   // rim -> apex
    Gores(mb, prof, 10, Vector3{0.0f, 0.0f, 0.0f}, true, [&](int g, int j, int segs) {
        if (j == 0) return gold;
        if (j == segs - 1) return gold;
        return (g % 2 == 0) ? navy : ivory;
    });
    Strut(mb, Vector3{0.0f, 0.0f, 0.0f}, Vector3{0.0f, 2.64f, 0.0f}, 0.022f, gold, 6);          // pole
    Strut(mb, Vector3{-0.16f, 0.0f, 0.0f}, Vector3{0.16f, 0.0f, 0.0f}, 0.03f, leather, 6);      // handle
    mb.Loft({{2.62f, 0.04f, 0.04f, gold}, {2.74f, 0.025f, 0.025f, gold}, {2.84f, 0.0f, 0.0f, gold}}, 6, nullptr, false, false);   // finial
    for (int g = 0; g < 10; g++) {                                                              // ribs under the canopy
        const float a = 2.0f * kPi * g / 10;
        Strut(mb, Vector3{0.0f, 2.60f, 0.0f}, Vector3{1.84f * cosf(a), 2.00f, 1.84f * sinf(a)}, 0.012f, rib, 4);
    }
}

// ---------------------------------------------------------------------------------------- build / draw
void GliderBuild(SkyModels& m) {
    { MeshBuilder mb; BuildDefaultGlider(mb); m.glider[0] = Mk(mb); }
    { MeshBuilder mb; BuildMakoGlider(mb);    m.glider[1] = Mk(mb); }
    { MeshBuilder mb; BuildFounderGlider(mb); m.glider[2] = Mk(mb); }
}

void GliderUnload(SkyModels& m) {
    for (int i = 0; i < kGliderCount; i++) gR.FreeModel(m.glider[i]);
}

void DrawGlider(const SkyModels& m, int id, const Matrix& frame, float open, bool unfurl, Color tint) {
    if (open <= 0.001f) return;
    id = (id < 0 || id >= kGliderCount) ? 0 : id;
    const Model& mdl = m.glider[id];
    if (mdl.meshCount <= 0) return;
    const float o = fminf(open, 1.0f);
    float s = o, sy = o;
    if (unfurl) {                                              // pops open with a little overshoot
        const float c1 = 1.70158f, c3 = c1 + 1.0f, x = o - 1.0f;
        s = 1.0f + c3 * x * x * x + c1 * x * x;
        sy = s * Lerp(0.15f, 1.0f, Clamp(o * 1.25f, 0.0f, 1.0f));
    }
    Model t = mdl;
    t.transform = MatrixMultiply(MatrixScale(s, sy, s), frame);
    DrawModel(t, Vector3{0.0f, 0.0f, 0.0f}, 1.0f, tint);
}
