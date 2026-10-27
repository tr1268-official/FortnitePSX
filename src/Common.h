#pragma once
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <vector>
#include <string>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <algorithm>

constexpr float kPi = 3.14159265f;
struct AABB { Vector3 min, max; };

inline Color C(int r, int g, int b, int a = 255) { return Color{(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a}; }
inline Color Mul(Color c, float f) { return C((int)Clamp(c.r * f, 0, 255), (int)Clamp(c.g * f, 0, 255), (int)Clamp(c.b * f, 0, 255), c.a); }
inline Color Mix(Color a, Color b, float t) { return C((int)(a.r + (b.r - a.r) * t), (int)(a.g + (b.g - a.g) * t), (int)(a.b + (b.b - a.b) * t), (int)(a.a + (b.a - a.a) * t)); }
inline float Rand01() { return (float)GetRandomValue(0, 10000) / 10000.0f; }
inline float RandF(float a, float b) { return a + (b - a) * Rand01(); }
inline int RandI(int a, int b) { return GetRandomValue(a, b); }
inline AABB BoxAt(Vector3 c, Vector3 s) { return AABB{{c.x - s.x / 2, c.y - s.y / 2, c.z - s.z / 2}, {c.x + s.x / 2, c.y + s.y / 2, c.z + s.z / 2}}; }
inline bool Overlap(const AABB& a, const AABB& b) {
    return a.min.x < b.max.x && a.max.x > b.min.x && a.min.y < b.max.y && a.max.y > b.min.y && a.min.z < b.max.z && a.max.z > b.min.z;
}
inline Vector3 Fwd(float yaw, float pitch) { return Vector3{cosf(pitch) * sinf(yaw), sinf(pitch), cosf(pitch) * cosf(yaw)}; }
inline Vector3 RightOf(float yaw) { return Vector3{-cosf(yaw), 0, sinf(yaw)}; }
inline float WrapPi(float a) { while (a > kPi) a -= 2 * kPi; while (a < -kPi) a += 2 * kPi; return a; }

// Global UI Font
extern Font gFont;