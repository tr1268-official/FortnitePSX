#pragma once
// Small geometry helpers shared by the battle bus and glider models.
// Everything here only calls MeshBuilder's public API (Quad / Tri / Box / Loft).
#include "MeshBuilder.h"
#include <algorithm>
#include <vector>

namespace skygeo {

// Quad visible from both sides. `front` colours the side the quad's normal
// (cross(b-a, d-a)) points to, `back` colours the other side.
inline void QuadBoth(MeshBuilder& mb, Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color front, Color back, float th = 0.014f) {
    Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(d, a)));
    Vector3 o = Vector3Scale(n, th * 0.5f);
    mb.Quad(Vector3Add(a, o), Vector3Add(b, o), Vector3Add(c, o), Vector3Add(d, o), front);
    mb.Quad(Vector3Subtract(a, o), Vector3Subtract(d, o), Vector3Subtract(c, o), Vector3Subtract(b, o), back);
}
inline void TriBoth(MeshBuilder& mb, Vector3 a, Vector3 b, Vector3 c, Color front, Color back, float th = 0.014f) {
    Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a)));
    Vector3 o = Vector3Scale(n, th * 0.5f);
    mb.Tri(Vector3Add(a, o), Vector3Add(b, o), Vector3Add(c, o), front);
    mb.Tri(Vector3Subtract(a, o), Vector3Subtract(c, o), Vector3Subtract(b, o), back);
}

// Mostly-horizontal sail panels: `top` is always the colour seen from above, `bot` from below.
inline void SailQuad(MeshBuilder& mb, Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color top, Color bot) {
    Vector3 n = Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(d, a));
    if (n.y >= 0) QuadBoth(mb, a, b, c, d, top, bot); else QuadBoth(mb, a, b, c, d, bot, top);
}
inline void SailTri(MeshBuilder& mb, Vector3 a, Vector3 b, Vector3 c, Color top, Color bot) {
    Vector3 n = Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a));
    if (n.y >= 0) TriBoth(mb, a, b, c, top, bot); else TriBoth(mb, a, b, c, bot, top);
}

// Thin round strut between two points (cables, poles, frame tubes, ropes).
inline void Strut(MeshBuilder& mb, Vector3 p0, Vector3 p1, float r, Color c, int sides = 6) {
    Vector3 d = Vector3Subtract(p1, p0);
    float L = Vector3Length(d);
    if (L < 1e-4f) return;
    d = Vector3Scale(d, 1.0f / L);
    Vector3 ax = Vector3CrossProduct(Vector3{0, 1, 0}, d);
    float s = Vector3Length(ax);
    Matrix rot = (s < 1e-5f) ? (d.y > 0 ? MatrixIdentity() : MatrixRotateX(kPi))
                             : MatrixRotate(Vector3Scale(ax, 1.0f / s), atan2f(s, d.y));
    Matrix xf = MatrixMultiply(rot, MatrixTranslate(p0.x, p0.y, p0.z));
    mb.Loft({{0.0f, r, r, c}, {L, r, r, c}}, sides, &xf, true, true);
}

// Profile point of a surface of revolution around Y.
struct Prof { float y, r; };

// Gore-panelled dome / balloon. `colourOf(gore, segment, segmentCount)` picks each panel's colour.
// With `inside` the panels are also visible (darker) from the inside, which umbrellas need.
template <class F>
inline void Gores(MeshBuilder& mb, const std::vector<Prof>& pr, int gores, Vector3 off, bool inside, F colourOf) {
    const int segs = (int)pr.size() - 1;
    for (int g = 0; g < gores; g++) {
        const float a0 = 2.0f * kPi * g / gores, a1 = 2.0f * kPi * (g + 1) / gores, am = 0.5f * (a0 + a1);
        const float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
        const Vector3 radial = {cosf(am), 0.0f, sinf(am)};
        for (int j = 0; j < segs; j++) {
            auto P = [&](const Prof& q, float c, float s) { return Vector3{off.x + q.r * c, off.y + q.y, off.z + q.r * s}; };
            Vector3 p00 = P(pr[j], c0, s0), p01 = P(pr[j], c1, s1), p11 = P(pr[j + 1], c1, s1), p10 = P(pr[j + 1], c0, s0);
            Vector3 n = Vector3CrossProduct(Vector3Subtract(p01, p00), Vector3Subtract(p10, p00));
            if (Vector3DotProduct(n, radial) < 0) std::swap(p01, p10);      // make the winding face outwards
            Color col = colourOf(g, j, segs);
            if (inside) QuadBoth(mb, p00, p01, p11, p10, col, Mul(col, 0.62f), 0.02f);
            else        mb.Quad(p00, p01, p11, p10, col);
        }
    }
}

} // namespace skygeo
