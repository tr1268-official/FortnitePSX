#include "Hud.h"
#include "IconCache.h"

// ---------------------------------------------------------------------------
// UI scaling
// The HUD was originally laid out in PSX-sized pixels. Every size/offset below is
// still written in those "design units"; U() converts them to real pixels for the
// current render target, so the HUD stays correct at 1920x1080 (16:9), split-screen
// halves (960x1080), or any other size.
//
// kRefH is the height (in pixels) that counts as scale 1.0. Lower it to make the
// whole HUD bigger, raise it to make it smaller.
// ---------------------------------------------------------------------------
static const float kRefH = 360.0f;
static const float kRefW = 640.0f;
static float gU = 1.0f;

static inline int U(float v) { return (int)lroundf(v * gU); }
static inline int U1(float v) { int r = U(v); return r < 1 ? 1 : r; }   // never collapses to 0 px

static void Txt(const char* s, int x, int y, int sz, Color c) {
    float fs = (float)sz * gU;
    float sh = fmaxf(1.0f, gU * 0.5f);
    DrawTextEx(gFont, s, {(float)x + sh, (float)y + sh}, fs, 1.0f, Color{0, 0, 0, 180});
    DrawTextEx(gFont, s, {(float)x, (float)y}, fs, 1.0f, c);
}
static float TxtW(const char* s, int sz) { return MeasureTextEx(gFont, s, (float)sz * gU, 1.0f).x; }
static void CTxt(const char* s, int cx, int y, int sz, Color c) {
    Txt(s, cx - (int)(TxtW(s, sz) / 2), y, sz, c);
}
static void Box(int x, int y, int w, int h, Color c) {
    DrawRectangleLinesEx(Rectangle{(float)x, (float)y, (float)w, (float)h}, (float)U1(1), c);
}

// ---------------------------------------------------------------------------
// Reticles
// ---------------------------------------------------------------------------
static void ReticleLine(float x1, float y1, float x2, float y2, float th, Color c) {
    float o = fmaxf(1.0f, gU * 0.5f);
    DrawLineEx({x1, y1}, {x2, y2}, th + o * 2.0f, Color{0, 0, 0, (unsigned char)(c.a * 0.7f)});
    DrawLineEx({x1, y1}, {x2, y2}, th, c);
}
static void ReticleDot(float cx, float cy, float r, Color c) {
    DrawCircleV({cx, cy}, r + fmaxf(1.0f, gU * 0.5f), Color{0, 0, 0, (unsigned char)(c.a * 0.7f)});
    DrawCircleV({cx, cy}, r, c);
}
static void ReticleRing(float cx, float cy, float r, float th, Color c) {
    float o = fmaxf(1.0f, gU * 0.5f);
    DrawRing({cx, cy}, r - th / 2 - o, r + th / 2 + o, 0, 360, 48, Color{0, 0, 0, (unsigned char)(c.a * 0.7f)});
    DrawRing({cx, cy}, r - th / 2, r + th / 2, 0, 360, 48, c);
}
// Four ticks around the centre with an open gap.
static void ReticleCross(float cx, float cy, float gap, float len, float th, Color c) {
    ReticleLine(cx - gap - len, cy, cx - gap, cy, th, c);
    ReticleLine(cx + gap, cy, cx + gap + len, cy, th, c);
    ReticleLine(cx, cy - gap - len, cx, cy - gap, th, c);
    ReticleLine(cx, cy + gap, cx, cy + gap + len, th, c);
}
static void ReticleDiagonals(float cx, float cy, float r0, float r1, float th, Color c) {
    const float k = 0.70710678f;
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2)
            ReticleLine(cx + sx * r0 * k, cy + sy * r0 * k, cx + sx * r1 * k, cy + sy * r1 * k, th, c);
}

// Weapon-type reticle. Scoped weapons (a.isAiming) are skipped: the scope overlay has its own.
static void Reticle(const Actor& a, int W, int H) {
    float cx = W / 2.0f, cy = H / 2.0f;
    unsigned char al = a.reloadT > 0 ? 110 : 240;
    Color col = Color{255, 255, 255, al};
    float th = fmaxf(1.0f, gU * 0.7f);          // normal line thickness
    float thin = fmaxf(1.0f, gU * 0.45f);       // thin line (rifles)

    const ItemDef* d = nullptr;
    if (!a.buildMode && a.sel >= 0 && !a.slots[a.sel].Empty()) d = &GetItemDef(a.slots[a.sel].id);

    // Pickaxe / building / healing / empty hands: just a small dot
    if (!d || d->kind == K_PICKAXE || d->kind == K_HEAL || d->kind == K_AMMO) {
        ReticleDot(cx, cy, (float)U1(a.buildMode ? 1.3f : 1.0f), col);
        return;
    }
    if (d->kind == K_THROWABLE) {
        ReticleRing(cx, cy, (float)U(6), th, col);
        ReticleDot(cx, cy, (float)U1(1.0f), col);
        return;
    }

    // Spread bloom: moving widens it, firing kicks it for a moment
    float bloom = a.moveAmt + (a.tracerT > 0 ? 0.6f : 0.0f);

    switch (a.slots[a.sel].id) {
        case I_PISTOL:
            ReticleCross(cx, cy, U(3) + bloom * U(2), (float)U(4), th, col);
            ReticleDot(cx, cy, (float)U1(0.9f), col);
            break;
        case I_AR:
            ReticleCross(cx, cy, U(4) + bloom * U(3), (float)U(6), th, col);
            ReticleDot(cx, cy, (float)U1(0.9f), col);
            break;
        case I_SMG:
            ReticleCross(cx, cy, U(2.5f) + bloom * U(2), (float)U(4), th, col);
            break;
        case I_PUMP:
            ReticleRing(cx, cy, U(14) + bloom * U(4), th, col);
            ReticleDot(cx, cy, (float)U1(0.9f), col);
            break;
        case I_TACTICAL: {
            float r = U(11) + bloom * U(4);
            ReticleRing(cx, cy, r, th, col);
            ReticleLine(cx - r - U(5), cy, cx - r - U(1), cy, th, col);
            ReticleLine(cx + r + U(1), cy, cx + r + U(5), cy, th, col);
            ReticleDot(cx, cy, (float)U1(0.9f), col);
            break;
        }
        case I_HUNTING:
            ReticleCross(cx, cy, U(6) + bloom * U(3), (float)U(10), thin, col);
            ReticleDot(cx, cy, (float)U1(0.9f), col);
            break;
        case I_SNIPER:
            ReticleCross(cx, cy, U(9) + bloom * U(3), (float)U(16), thin, col);
            ReticleDot(cx, cy, (float)U1(1.0f), Color{230, 40, 40, al});
            break;
        case I_ROCKET:
            ReticleRing(cx, cy, (float)U(9), th, col);
            ReticleDiagonals(cx, cy, U(12) + bloom * U(3), U(17) + bloom * U(3), th, col);
            ReticleDot(cx, cy, (float)U1(0.9f), col);
            break;
        default:
            ReticleCross(cx, cy, U(4) + bloom * U(3), (float)U(5), th, col);
            ReticleDot(cx, cy, (float)U1(0.9f), col);
            break;
    }
}

// ---------------------------------------------------------------------------
// HUD pieces
// ---------------------------------------------------------------------------
static void Compass(const Actor& a, int W) {
    float hd = atan2f(sinf(a.yaw), -cosf(a.yaw)) * RAD2DEG;
    static const char* names[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    float step = 15.0f, base = roundf(hd / step) * step;
    for (int k = -4; k <= 4; k++) {
        float deg = base + k * step, off = deg - hd, x = W / 2 + off * 1.35f * gU;
        float d = fmodf(deg + 720.0f, 360.0f);
        int ix = (int)roundf(d / 45.0f) % 8;
        float alpha = 1.0f - fabsf(off) / 62.0f; if (alpha <= 0) continue;
        Color c = Color{255, 255, 255, (unsigned char)(220 * alpha)};
        if (fabsf(fmodf(d, 45.0f)) < 0.5f || fabsf(fmodf(d, 45.0f) - 45.0f) < 0.5f) CTxt(names[ix], (int)x, U(4), 12, c);
        else DrawRectangle((int)x, U(6), U1(2), U(7), c);
    }
}

static void Minimap(const Actor& a, const World& w, int W, const HudInfo& info) {
    const int S = U(56), x0 = W - S - U(4), y0 = U(4); const float sc = 0.9f * gU;
    int b = U1(1);
    DrawRectangle(x0 - b, y0 - b, S + 2 * b, S + 2 * b, Color{240, 240, 240, 220});
    DrawRectangle(x0, y0, S, S, Color{92, 96, 40, 255});
    auto clip = [&](float rx, float ry, float rw, float rh, Color c) {
        float x1 = fmaxf(rx, (float)x0), y1 = fmaxf(ry, (float)y0), x2 = fminf(rx + rw, (float)(x0 + S)), y2 = fminf(ry + rh, (float)(y0 + S));
        if (x2 > x1 && y2 > y1) DrawRectangle((int)x1, (int)y1, (int)(x2 - x1) + 1, (int)(y2 - y1) + 1, c);
    };
    float cx = x0 + S / 2.0f, cy = y0 + S / 2.0f;
    for (const auto& s : w.solids) {
        if (!s.alive || s.box.max.y < 0.5f) continue;
        Color c = s.kind == SK_TREE ? Color{50, 90, 40, 255} : s.kind == SK_CONTAINER ? Color{120, 120, 130, 255} : s.kind == SK_CRATE ? Color{190, 120, 60, 255} : Color{200, 200, 205, 255};
        clip(cx + (s.box.min.x - a.pos.x) * sc, cy + (s.box.min.z - a.pos.z) * sc, (s.box.max.x - s.box.min.x) * sc, (s.box.max.z - s.box.min.z) * sc, c);
    }
    for (const auto& p : w.pieces) if (p.alive) clip(cx + (p.box.min.x - a.pos.x) * sc, cy + (p.box.min.z - a.pos.z) * sc, (p.box.max.x - p.box.min.x) * sc, (p.box.max.z - p.box.min.z) * sc, Color{230, 190, 120, 255});
    float cs = 3.0f * gU;
    for (const auto& c : w.chests) if (!c.opened) clip(cx + (c.pos.x - a.pos.x) * sc - cs / 3, cy + (c.pos.z - a.pos.z) * sc - cs / 3, cs, cs, Color{255, 220, 40, 255});
    Vector2 f = {sinf(a.yaw), cosf(a.yaw)}, r = {cosf(a.yaw), -sinf(a.yaw)};
    float tipL = 5.0f * gU, backL = 3.0f * gU, sideL = 3.0f * gU;
    Vector2 tip = {cx + f.x * tipL, cy + f.y * tipL}, l = {cx - f.x * backL + r.x * sideL, cy - f.y * backL + r.y * sideL}, rr = {cx - f.x * backL - r.x * sideL, cy - f.y * backL - r.y * sideL};
    DrawTriangle(tip, rr, l, WHITE); DrawTriangle(tip, l, rr, WHITE);
    float dist = sqrtf(a.pos.x * a.pos.x + a.pos.z * a.pos.z);
    if (info.stormR - dist < 25) Txt("STORM", x0 + U(2), y0 + S - U(10), 10, Color{200, 120, 255, 255});
}

static void Icon(PieceType t, int x, int y, int s, Color c) {
    switch (t) {
        case P_WALL:  DrawRectangle(x + s / 4, y + U(3), s / 2, s - U(6), c); break;
        case P_FLOOR: DrawRectangle(x + U(3), y + s / 2 - U1(1), s - U(6), U1(4), c); break;
        case P_STAIR: for (int k = 0; k < 4; k++) DrawRectangle(x + U(3) + k * U(3), y + s - U(6) - k * U(3), U(3) * (4 - k) + U1(1), U(3), c); break;
        default:      DrawTriangle({(float)x + s / 2, (float)y + U(3)}, {(float)x + U(3), (float)y + s - U(4)}, {(float)x + s - U(3), (float)y + s - U(4)}, c); break;
    }
}

static void ItemIcon(const Slot& s, int x, int y, int sz) {
    const ItemDef& d = GetItemDef(s.id);
    const Texture2D* tex = gIcons.Item(d.name);
    if (tex && tex->id != 0) {
        Rectangle src = {0, 0, (float)tex->width, (float)tex->height};
        Rectangle dst = {(float)x, (float)y, (float)sz, (float)sz};
        DrawTexturePro(*tex, src, dst, {0, 0}, 0, WHITE);
        return;
    }
    if (d.kind == K_WEAPON || d.kind == K_PICKAXE) {
        DrawRectangle(x + U(3), y + sz / 2 - U(2), sz - U(6), U(4), d.icon);
        DrawRectangle(x + U(5), y + sz / 2 + U1(1), U(3), U(5), d.icon);
        DrawRectangle(x + sz - U(9), y + sz / 2 - U(4), U(2), U(3), d.icon);
    }
    else if (d.kind == K_HEAL) { DrawCircle(x + sz / 2, y + sz / 2 + U1(1), (float)U(5), d.icon); DrawRectangle(x + sz / 2 - U1(1), y + U(3), U(3), U(4), WHITE); }
    else if (d.kind == K_THROWABLE) DrawCircle(x + sz / 2, y + sz / 2, (float)U(5), d.icon);
}

static bool UICentred(const char* name, int W, int cy, float maxW, float maxH, const char* fallback, int fbSz, Color fbCol) {
    const Texture2D* tex = gIcons.UI(name);
    if (tex && tex->id != 0) {
        float scale = fminf(maxW / tex->width, maxH / tex->height);
        float w = tex->width * scale, h = tex->height * scale;
        Rectangle src = {0, 0, (float)tex->width, (float)tex->height};
        Rectangle dst = {(W - w) / 2, cy - h / 2, w, h};
        DrawTexturePro(*tex, src, dst, {0, 0}, 0, WHITE);
        return true;
    }
    if (fallback && fallback[0]) CTxt(fallback, W / 2, cy - (int)(fbSz * gU) / 2, fbSz, fbCol);
    return false;
}

static void StatIcon(const char* name, int x, int y, int sz) {
    const Texture2D* tex = gIcons.UIItem(name);
    if (tex && tex->id != 0) {
        Rectangle src = {0, 0, (float)tex->width, (float)tex->height};
        Rectangle dst = {(float)x, (float)y, (float)sz, (float)sz};
        DrawTexturePro(*tex, src, dst, {0, 0}, 0, WHITE);
    } else {
        DrawRectangle(x, y + U(2), U(10), U(8), Color{200, 220, 255, 255});
        DrawRectangle(x + U(2), y + U(3), U(7), U(7), WHITE);
    }
}

void DrawHud(const Actor& a, const World& w, int W, int H, const HudInfo& info) {
    // Scale from the render target: height drives it, width caps it (split-screen halves are narrow).
    gU = fminf((float)H / kRefH, (float)W / kRefW);
    if (gU < 0.5f) gU = 0.5f;

    if (a.hitFlash > 0) DrawRectangle(0, 0, W, H, Color{255, 0, 0, (unsigned char)(90 * fminf(1.0f, a.hitFlash * 4))});
    if (a.alive) {
        // Scoped view draws its own reticle in the scope overlay
        if (!a.isAiming || a.buildMode) Reticle(a, W, H);
        Compass(a, W);
        Minimap(a, w, W, info);

        // Storm timer / players alive / kills (under the minimap)
        int sx = W - U(66), sy = U(64);
        int t = (int)fmaxf(0, info.stormT); char buf[32]; snprintf(buf, sizeof buf, "%d:%02d", t / 60, t % 60);
        DrawRectangle(sx, sy + U1(1), U(7), U(7), info.shrinking ? Color{200, 100, 255, 255} : Color{140, 200, 255, 255});
        Txt(buf, sx + U(10), sy, 12, WHITE);
        DrawCircle(sx + U(36), sy + U(4), (float)U(4), WHITE); snprintf(buf, sizeof buf, "%d", info.alive); Txt(buf, sx + U(43), sy, 12, WHITE);
        DrawCircleLines(sx + U(56), sy + U(4), (float)U(4), WHITE); snprintf(buf, sizeof buf, "%d", a.kills); Txt(buf, sx + U(60), sy + U(14), 12, WHITE);

        // Health / Shield (Bottom Center)
        // Group = icon (16 units wide incl. gap) + bar; centre the whole group on screen.
        int bw = U(108), iconW = U(16);
        int bx = (W - (iconW + bw)) / 2 + iconW, by = H - U(26);

        DrawRectangle(bx, by, bw, U(8), Color{20, 20, 20, 190});
        DrawRectangle(bx, by, (int)(bw * a.shield / 100.0f), U(8), Color{80, 160, 255, 255});
        snprintf(buf, sizeof buf, "%d", (int)a.shield);
        Txt(buf, bx + U(4), by - U(2), 12, WHITE);
        StatIcon("Shield", bx - U(16), by - U(2), U(14));

        int hy = by + U(12);
        DrawRectangle(bx, hy, bw, U(9), Color{20, 20, 20, 190});
        DrawRectangle(bx, hy, (int)(bw * a.health / 100.0f), U(9), Color{80, 200, 60, 255});
        snprintf(buf, sizeof buf, "%d", (int)ceilf(a.health));
        Txt(buf, bx + U(4), hy - U(2), 12, WHITE);
        StatIcon("Health", bx - U(16), hy - U(2), U(14));

        // Hotbar (Bottom Right)
        const int S = U(20), gap = U(2); int hx = W - U(6) - 6 * (S + gap), hy2 = H - S - U(5);
        for (int i = 0; i < 6; i++) {
            int x = hx + i * (S + gap);

            if (!a.slots[i].Empty()) {
                Color rc = RarityColor(a.slots[i].rar);
                Color mid = { (unsigned char)(rc.r * 0.6f), (unsigned char)(rc.g * 0.6f), (unsigned char)(rc.b * 0.6f), 220 };
                Color dark = { (unsigned char)(rc.r * 0.15f), (unsigned char)(rc.g * 0.15f), (unsigned char)(rc.b * 0.15f), 220 };
                DrawRectangleGradientV(x, hy2, S, S / 2, rc, mid);
                DrawRectangleGradientV(x, hy2 + S / 2, S, S / 2, mid, dark);
            } else {
                DrawRectangle(x, hy2, S, S, Color{40, 60, 80, 170});
            }

            Box(x, hy2, S, S, (i == a.sel && !a.buildMode) ? Color{255, 255, 255, 255} : Color{200, 215, 235, 100});

            if (!a.slots[i].Empty()) {
                ItemIcon(a.slots[i], x, hy2, S);
                const ItemDef& d = GetItemDef(a.slots[i].id);
                if (d.kind == K_WEAPON) {
                    snprintf(buf, sizeof buf, "%d", a.slots[i].mag);
                    Txt(buf, x + U(2), hy2 + S - U(9), 10, Color{255, 255, 255, 230});
                }
                else if (d.kind != K_PICKAXE) {
                    snprintf(buf, sizeof buf, "%d", a.slots[i].count);
                    Txt(buf, x + S - (int)TxtW(buf, 10) - U(2), hy2 + S - U(9), 10, WHITE);
                }
            }
            snprintf(buf, sizeof buf, "%d", i + 1); Txt(buf, x + U(2), hy2 + U1(1), 9, Color{255, 255, 255, 140});
        }

        // Build pieces row
        int by2 = hy2 - S - U(4);
        for (int i = 0; i < 5; i++) {
            int x = hx + i * (S + gap) + (S + gap) / 2 - U(2);
            DrawRectangle(x, by2, S, S, Color{40, 90, 130, 170});
            bool selp = a.buildMode && i == (int)a.piece;
            Box(x, by2, S, S, selp ? Color{255, 255, 255, 255} : Color{200, 215, 235, 100});
            if (i < 4) Icon((PieceType)i, x, by2, S, Color{180, 225, 255, 230});
        }

        // Materials
        int my = by2 - U(28);
        const char* matNames[3] = {"Wood", "Brick", "Metal"};
        for (int i = 0; i < 3; i++) {
            int x = hx + U(8) + i * U(26);
            DrawRectangle(x, my, U(14), U(14), Color{0, 0, 0, 110});

            const Texture2D* matTex = gIcons.UIItem(matNames[i]);
            if (matTex && matTex->id != 0) {
                Rectangle src = {0, 0, (float)matTex->width, (float)matTex->height};
                Rectangle dst = {(float)(x + U1(1)), (float)(my + U1(1)), (float)U(12), (float)U(12)};
                DrawTexturePro(*matTex, src, dst, {0, 0}, 0, WHITE);
            } else {
                DrawRectangle(x + U(2), my + U(2), U(10), U(10), kMatColor[i]);
            }

            if (a.buildMode && a.mat == i) Box(x - U1(1), my - U1(1), U(16), U(16), WHITE);
            snprintf(buf, sizeof buf, "%d", a.mats[i]); Txt(buf, x + U(7) - (int)(TxtW(buf, 12) / 2), my + U(15), 12, WHITE);
        }

        // Centre prompts
        if (!a.prompt.empty()) CTxt(("[E] " + a.prompt).c_str(), W / 2, H / 2 + U(24), 12, Color{255, 240, 120, 255});
        if (a.useT > 0 && a.sel > 0 && !a.slots[a.sel].Empty()) {
            float need = GetItemDef(a.slots[a.sel].id).useTime;
            if (need > 0) {
                int bw2 = U(50), bh2 = U(5);
                DrawRectangle(W / 2 - bw2 / 2, H / 2 + U(38), bw2, bh2, Color{0, 0, 0, 160});
                DrawRectangle(W / 2 - bw2 / 2, H / 2 + U(38), (int)(bw2 * a.useT / need), bh2, Color{120, 255, 120, 255});
            }
        }
        if (a.reloadT > 0) CTxt("Reloading...", W / 2, H / 2 + U(38), 12, WHITE);
        if (a.toastT > 0) CTxt(a.toast.c_str(), W / 2, H / 2 - U(40), 12, Color{255, 255, 255, 230});
    }

    // Kill feed
    int fy = U(20);
    for (const auto& m : *info.feed) { if (m.t > 0) { Txt(m.s.c_str(), U(6), fy, 11, WHITE); fy += U(13); } }

    if (info.over) {
        DrawRectangle(0, H / 2 - U(30), W, U(60), Color{0, 0, 0, 170});
        if (info.won) {
            UICentred("Victory Royale", W, H / 2, (float)W, (float)U(100), "VICTORY ROYALE", 40, Color{255, 220, 60, 255});
        } else {
            char b2[32]; snprintf(b2, sizeof b2, "YOU PLACED #%d", info.placement);
            CTxt(b2, W / 2, H / 2 - U(20), 24, Color{255, 120, 120, 255});
        }
        CTxt("ENTER - back to lobby", W / 2, H / 2 + U(8), 11, WHITE);
    } else if (!a.alive) CTxt("ELIMINATED - spectating", W / 2, H / 2, 12, Color{255, 120, 120, 255});
}
