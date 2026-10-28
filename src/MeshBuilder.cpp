#include "MeshBuilder.h"

static Vector3 TP(const Matrix* m, Vector3 v) { return m ? Vector3Transform(v, *m) : v; }
static Vector3 TD(const Matrix* m, Vector3 v) {
    if (!m) return v;
    Vector3 o = Vector3Transform(Vector3{0, 0, 0}, *m);
    return Vector3Normalize(Vector3Subtract(Vector3Transform(v, *m), o));
}

int MeshBuilder::Add(Vector3 p, Vector3 n, Vector2 uv, Color c) {
    verts.push_back(V{p, n, uv, c});
    return (int)verts.size() - 1;
}

void MeshBuilder::Quad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color col, float tile) {
    Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(d, a)));
    float u = Vector3Length(Vector3Subtract(b, a)) / tile, v = Vector3Length(Vector3Subtract(d, a)) / tile;
    int i0 = Add(a, n, {0, 0}, col), i1 = Add(b, n, {u, 0}, col), i2 = Add(c, n, {u, v}, col), i3 = Add(d, n, {0, v}, col);
    T(i0, i1, i2); T(i0, i2, i3);
}

void MeshBuilder::Tri(Vector3 a, Vector3 b, Vector3 c, Color col) {
    Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a)));
    int i0 = Add(a, n, {0, 0}, col), i1 = Add(b, n, {1, 0}, col), i2 = Add(c, n, {0, 1}, col);
    T(i0, i1, i2);
}

void MeshBuilder::QuadUV(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Vector2 ta, Vector2 tb, Vector2 tc, Vector2 td, Color col) {
    Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(d, a)));
    int i0 = Add(a, n, ta, col), i1 = Add(b, n, tb, col), i2 = Add(c, n, tc, col), i3 = Add(d, n, td, col);
    T(i0, i1, i2); T(i0, i2, i3);
}

void MeshBuilder::TriUV(Vector3 a, Vector3 b, Vector3 c, Vector2 ta, Vector2 tb, Vector2 tc, Color col) {
    Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a)));
    int i0 = Add(a, n, ta, col), i1 = Add(b, n, tb, col), i2 = Add(c, n, tc, col);
    T(i0, i1, i2);
}

void MeshBuilder::Box(Vector3 c, Vector3 s, Color col, const Matrix* xf, float tile) {
    float hx = s.x / 2, hy = s.y / 2, hz = s.z / 2;
    Vector3 p[8] = {
        {c.x - hx, c.y - hy, c.z - hz}, {c.x + hx, c.y - hy, c.z - hz}, {c.x + hx, c.y + hy, c.z - hz}, {c.x - hx, c.y + hy, c.z - hz},
        {c.x - hx, c.y - hy, c.z + hz}, {c.x + hx, c.y - hy, c.z + hz}, {c.x + hx, c.y + hy, c.z + hz}, {c.x - hx, c.y + hy, c.z + hz}};
    if (xf) for (auto& v : p) v = Vector3Transform(v, *xf);
    Quad(p[4], p[5], p[6], p[7], Mul(col, 1.00f), tile); // +z
    Quad(p[1], p[0], p[3], p[2], Mul(col, 0.90f), tile); // -z
    Quad(p[0], p[4], p[7], p[3], Mul(col, 0.95f), tile); // -x
    Quad(p[5], p[1], p[2], p[6], Mul(col, 0.95f), tile); // +x
    Quad(p[3], p[7], p[6], p[2], Mul(col, 1.05f), tile); // +y
    Quad(p[0], p[1], p[5], p[4], Mul(col, 0.80f), tile); // -y
}

void MeshBuilder::Loft(const std::vector<Ring>& rg, int sides, const Matrix* xf, bool capB, bool capT) {
    int R = (int)rg.size();
    if (R < 2 || sides < 3) return;
    std::vector<int> base(R);
    for (int r = 0; r < R; r++) {
        base[r] = (int)verts.size();
        for (int i = 0; i <= sides; i++) {
            float a = 2 * kPi * i / sides, cx = cosf(a), sz = sinf(a);
            Vector3 p = {rg[r].ox + rg[r].rx * cx, rg[r].y, rg[r].oz + rg[r].rz * sz};
            Vector3 n = Vector3Normalize(Vector3{cx / fmaxf(rg[r].rx, 0.01f), 0.0f, sz / fmaxf(rg[r].rz, 0.01f)});
            Add(TP(xf, p), TD(xf, n), {(float)i / sides * 2.0f, rg[r].y * 2.0f}, rg[r].c);
        }
    }
    for (int r = 0; r < R - 1; r++)
        for (int i = 0; i < sides; i++) {
            int a = base[r] + i, b = a + 1, c = base[r + 1] + i, d = c + 1;
            T(a, b, d); T(a, d, c);
        }
    if (capB) {
        int c0 = Add(TP(xf, Vector3{rg[0].ox, rg[0].y, rg[0].oz}), TD(xf, Vector3{0, -1, 0}), {0.5f, 0.5f}, rg[0].c);
        for (int i = 0; i < sides; i++) T(c0, base[0] + i + 1, base[0] + i);
    }
    if (capT) {
        const Ring& t = rg[R - 1];
        int c1 = Add(TP(xf, Vector3{t.ox, t.y, t.oz}), TD(xf, Vector3{0, 1, 0}), {0.5f, 0.5f}, t.c);
        for (int i = 0; i < sides; i++) T(c1, base[R - 1] + i, base[R - 1] + i + 1);
    }
}

Mesh MeshBuilder::Build() {
    Mesh m = {};
    if (verts.size() > 65000) TraceLog(LOG_WARNING, "MeshBuilder: too many vertices (%zu)", verts.size());
    m.vertexCount = (int)verts.size();
    m.triangleCount = (int)idx.size() / 3;
    m.vertices = (float*)MemAlloc(m.vertexCount * 3 * sizeof(float));
    m.normals = (float*)MemAlloc(m.vertexCount * 3 * sizeof(float));
    m.texcoords = (float*)MemAlloc(m.vertexCount * 2 * sizeof(float));
    m.colors = (unsigned char*)MemAlloc(m.vertexCount * 4);
    m.indices = (unsigned short*)MemAlloc(idx.size() * sizeof(unsigned short));
    for (int i = 0; i < m.vertexCount; i++) {
        const V& v = verts[i];
        m.vertices[i * 3] = v.p.x; m.vertices[i * 3 + 1] = v.p.y; m.vertices[i * 3 + 2] = v.p.z;
        m.normals[i * 3] = v.n.x; m.normals[i * 3 + 1] = v.n.y; m.normals[i * 3 + 2] = v.n.z;
        m.texcoords[i * 2] = v.uv.x; m.texcoords[i * 2 + 1] = v.uv.y;
        m.colors[i * 4] = v.c.r; m.colors[i * 4 + 1] = v.c.g; m.colors[i * 4 + 2] = v.c.b; m.colors[i * 4 + 3] = v.c.a;
    }
    for (size_t i = 0; i < idx.size(); i++) m.indices[i] = idx[i];
    UploadMesh(&m, false);
    return m;
}
