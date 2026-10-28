#pragma once
#include "Common.h"

// One ring of a lofted tube (axis = Y). Used for organic low-poly shapes: limbs, torso, head, trees...
struct Ring { float y, rx, rz; Color c; float ox = 0, oz = 0; };

class MeshBuilder {
public:
    void Quad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color col, float tile = 2.0f);
    void Tri(Vector3 a, Vector3 b, Vector3 c, Color col);
    // Same winding rules as Quad/Tri (normal = cross(b-a, d-a)), but with explicit texture coordinates per corner.
    // Use these for atlas-mapped parts; pass WHITE so the texture shows unmodified.
    void QuadUV(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Vector2 ta, Vector2 tb, Vector2 tc, Vector2 td, Color col = WHITE);
    void TriUV(Vector3 a, Vector3 b, Vector3 c, Vector2 ta, Vector2 tb, Vector2 tc, Color col = WHITE);
    void Box(Vector3 center, Vector3 size, Color col, const Matrix* xf = nullptr, float tile = 2.0f);
    void Loft(const std::vector<Ring>& rings, int sides, const Matrix* xf = nullptr, bool capBottom = true, bool capTop = true);
    Mesh Build();
    size_t Count() const { return verts.size(); }
private:
    struct V { Vector3 p, n; Vector2 uv; Color c; };
    std::vector<V> verts;
    std::vector<unsigned short> idx;
    int Add(Vector3 p, Vector3 n, Vector2 uv, Color c);
    void T(int a, int b, int c) { idx.push_back((unsigned short)a); idx.push_back((unsigned short)b); idx.push_back((unsigned short)c); }
};
