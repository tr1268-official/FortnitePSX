#include "VFX.h"

VFX gVFX;

void VFX::SpawnHitMarker(int type) {
    Particle p; 
    if (type == 2) { p.type = VFX_HIT_RED; p.color = Color{255, 50, 50, 255}; }
    else if (type == 1) { p.type = VFX_HIT_BLUE; p.color = Color{50, 150, 255, 255}; }
    else { p.type = VFX_HIT_WHITE; p.color = WHITE; }
    p.life = 0.25f; 
    p.maxLife = 0.25f; 
    particles.push_back(p);
}

void VFX::SpawnDamage(Vector3 pos, float dmg, bool headshot) {
    Particle p; 
    p.type = VFX_DAMAGE_NUMBER; 
    p.pos = pos; 
    p.vel = {RandF(-1,1), RandF(2,3), RandF(-1,1)}; 
    p.life = 0.8f; 
    p.maxLife = 0.8f; 
    p.color = headshot ? Color{255, 80, 80, 255} : WHITE;
    p.text = TextFormat("%d", (int)dmg);
    particles.push_back(p);
}

void VFX::SpawnImpact(Vector3 pos, Vector3 normal, Color c) {
    for (int i = 0; i < 6; i++) {
        Particle p;
        p.type = VFX_IMPACT;
        p.pos = pos;
        p.vel = Vector3Add(Vector3Scale(normal, RandF(2, 6)), Vector3{RandF(-2,2), RandF(-2,2), RandF(-2,2)});
        p.life = 0.4f;
        p.maxLife = 0.4f;
        p.color = c;
        particles.push_back(p);
    }
}

void VFX::Update(float dt, const Camera3D& cam) {
    for (int i = (int)particles.size() - 1; i >= 0; i--) {
        Particle& p = particles[i];
        p.life -= dt;
        if (p.life <= 0) { particles.erase(particles.begin() + i); continue; }
        
        if (p.type == VFX_DAMAGE_NUMBER || p.type == VFX_IMPACT) {
            p.pos = Vector3Add(p.pos, Vector3Scale(p.vel, dt));
            if (p.type == VFX_DAMAGE_NUMBER) p.vel.y -= 4.0f * dt;
            else p.vel = Vector3Scale(p.vel, 0.9f);
        }
    }
}

void VFX::Draw2D(const Camera3D& cam, int W, int H) {
    for (const auto& p : particles) {
        if (p.type >= VFX_HIT_WHITE && p.type <= VFX_HIT_RED) {
            float alpha = p.life / p.maxLife;
            Color c = Fade(p.color, alpha);
            int cx = W / 2, cy = H / 2;
            // Fortnite style Hitmarker: 4 diagonal lines
            DrawLine(cx - 12, cy - 12, cx - 5, cy - 5, c);
            DrawLine(cx - 12, cy + 12, cx - 5, cy + 5, c);
            DrawLine(cx + 12, cy - 12, cx + 5, cy - 5, c);
            DrawLine(cx + 12, cy + 12, cx + 5, cy + 5, c);
        }
        else if (p.type == VFX_DAMAGE_NUMBER) {
            Vector2 screen = GetWorldToScreen(p.pos, cam);
            if (screen.x > 0 && screen.x < W && screen.y > 0 && screen.y < H) {
                float alpha = p.life / p.maxLife;
                Color c = Fade(p.color, alpha);
                int fontSize = 10;
                int tw = MeasureText(p.text.c_str(), fontSize);
                // Use default font for numbers to avoid heavy scaling
                DrawText(p.text.c_str(), (int)screen.x - tw/2, (int)screen.y, fontSize, Color{0,0,0,(unsigned char)(alpha*160)});
                DrawText(p.text.c_str(), (int)screen.x - tw/2 + 1, (int)screen.y + 1, fontSize, c);
            }
        }
    }
}

void VFX::Draw3D() {
    for (const auto& p : particles) {
        if (p.type == VFX_IMPACT) {
            float alpha = p.life / p.maxLife;
            DrawCubeV(p.pos, {0.05f, 0.05f, 0.05f}, Fade(p.color, alpha));
        }
    }
}