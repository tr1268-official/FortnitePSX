#pragma once
#include "Common.h"

enum VfxType { VFX_HIT_WHITE, VFX_HIT_BLUE, VFX_HIT_RED, VFX_DAMAGE_NUMBER, VFX_IMPACT };
struct Particle { Vector3 pos, vel; float life, maxLife; Color color; VfxType type; std::string text; };

class VFX {
public:
    void SpawnHitMarker(int type); // 0=white, 1=blue, 2=red
    void SpawnDamage(Vector3 pos, float dmg, bool headshot);
    void SpawnImpact(Vector3 pos, Vector3 normal, Color c);
    void Update(float dt, const Camera3D& cam);
    void Draw2D(const Camera3D& cam, int W, int H);
    void Draw3D();
    void Clear() { particles.clear(); }
private:
    std::vector<Particle> particles;
};

extern VFX gVFX;