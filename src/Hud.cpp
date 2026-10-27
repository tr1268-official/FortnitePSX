#include "Hud.h"
#include "IconCache.h"

static void Txt(const char* s, int x, int y, int sz, Color c) { 
    DrawTextEx(gFont, s, {(float)x + 1, (float)y + 1}, (float)sz, 1.0f, Color{0, 0, 0, 180}); 
    DrawTextEx(gFont, s, {(float)x, (float)y}, (float)sz, 1.0f, c); 
}
static void CTxt(const char* s, int cx, int y, int sz, Color c) { 
    Txt(s, cx - (int)(MeasureTextEx(gFont, s, (float)sz, 1.0f).x / 2), y, sz, c); 
}

static void Compass(const Actor& a, int W) {
    float hd = atan2f(sinf(a.yaw), -cosf(a.yaw)) * RAD2DEG;
    static const char* names[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    float step = 15.0f, base = roundf(hd / step) * step;
    for (int k = -4; k <= 4; k++) {
        float deg = base + k * step, off = deg - hd, x = W / 2 + off * 1.35f;
        float d = fmodf(deg + 720.0f, 360.0f);
        int ix = (int)roundf(d / 45.0f) % 8;
        float alpha = 1.0f - fabsf(off) / 62.0f; if (alpha <= 0) continue;
        Color c = Color{255, 255, 255, (unsigned char)(220 * alpha)};
        if (fabsf(fmodf(d, 45.0f)) < 0.5f || fabsf(fmodf(d, 45.0f) - 45.0f) < 0.5f) CTxt(names[ix], (int)x, 4, 12, c);
        else DrawRectangle((int)x, 6, 2, 7, c);
    }
}

static void Minimap(const Actor& a, const World& w, int W, const HudInfo& info) {
    const int S = 56, x0 = W - S - 4, y0 = 4; const float sc = 0.9f;
    DrawRectangle(x0 - 1, y0 - 1, S + 2, S + 2, Color{240, 240, 240, 220});
    DrawRectangle(x0, y0, S, S, Color{92, 96, 40, 255});
    auto clip = [&](float rx, float ry, float rw, float rh, Color c) {
        float x1 = fmaxf(rx, x0), y1 = fmaxf(ry, y0), x2 = fminf(rx + rw, x0 + S), y2 = fminf(ry + rh, y0 + S);
        if (x2 > x1 && y2 > y1) DrawRectangle((int)x1, (int)y1, (int)(x2 - x1) + 1, (int)(y2 - y1) + 1, c);
    };
    float cx = x0 + S / 2.0f, cy = y0 + S / 2.0f;
    for (const auto& s : w.solids) {
        if (!s.alive || s.box.max.y < 0.5f) continue;
        Color c = s.kind == SK_TREE ? Color{50, 90, 40, 255} : s.kind == SK_CONTAINER ? Color{120, 120, 130, 255} : s.kind == SK_CRATE ? Color{190, 120, 60, 255} : Color{200, 200, 205, 255};
        clip(cx + (s.box.min.x - a.pos.x) * sc, cy + (s.box.min.z - a.pos.z) * sc, (s.box.max.x - s.box.min.x) * sc, (s.box.max.z - s.box.min.z) * sc, c);
    }
    for (const auto& p : w.pieces) if (p.alive) clip(cx + (p.box.min.x - a.pos.x) * sc, cy + (p.box.min.z - a.pos.z) * sc, (p.box.max.x - p.box.min.x) * sc, (p.box.max.z - p.box.min.z) * sc, Color{230, 190, 120, 255});
    for (const auto& c : w.chests) if (!c.opened) clip(cx + (c.pos.x - a.pos.x) * sc - 1, cy + (c.pos.z - a.pos.z) * sc - 1, 3, 3, Color{255, 220, 40, 255});
    Vector2 f = {sinf(a.yaw), cosf(a.yaw)}, r = {cosf(a.yaw), -sinf(a.yaw)};
    Vector2 tip = {cx + f.x * 5, cy + f.y * 5}, l = {cx - f.x * 3 + r.x * 3, cy - f.y * 3 + r.y * 3}, rr = {cx - f.x * 3 - r.x * 3, cy - f.y * 3 - r.y * 3};
    DrawTriangle(tip, rr, l, WHITE); DrawTriangle(tip, l, rr, WHITE);
    float dist = sqrtf(a.pos.x * a.pos.x + a.pos.z * a.pos.z);
    if (info.stormR - dist < 25) Txt("STORM", x0 + 2, y0 + S - 10, 10, Color{200, 120, 255, 255});
}

static void Icon(PieceType t, int x, int y, int s, Color c) {
    switch (t) {
        case P_WALL:  DrawRectangle(x + s / 4, y + 3, s / 2, s - 6, c); break;
        case P_FLOOR: DrawRectangle(x + 3, y + s / 2 - 1, s - 6, 4, c); break;
        case P_STAIR: for (int k = 0; k < 4; k++) DrawRectangle(x + 3 + k * 3, y + s - 6 - k * 3, 3 * (4 - k) + 1, 3, c); break;
        default:      DrawTriangle({(float)x + s / 2, (float)y + 3}, {(float)x + 3, (float)y + s - 4}, {(float)x + s - 3, (float)y + s - 4}, c); break;
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
    if (d.kind == K_WEAPON || d.kind == K_PICKAXE) { DrawRectangle(x + 3, y + sz / 2 - 2, sz - 6, 4, d.icon); DrawRectangle(x + 5, y + sz / 2 + 1, 3, 5, d.icon); DrawRectangle(x + sz - 9, y + sz / 2 - 4, 2, 3, d.icon); }
    else if (d.kind == K_HEAL) { DrawCircle(x + sz / 2, y + sz / 2 + 1, 5, d.icon); DrawRectangle(x + sz / 2 - 1, y + 3, 3, 4, WHITE); }
    else if (d.kind == K_THROWABLE) DrawCircle(x + sz / 2, y + sz / 2, 5, d.icon);
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
    if (fallback && fallback[0]) CTxt(fallback, W / 2, cy - fbSz / 2, fbSz, fbCol);
    return false;
}

void DrawHud(const Actor& a, const World& w, int W, int H, const HudInfo& info) {
    if (a.hitFlash > 0) DrawRectangle(0, 0, W, H, Color{255, 0, 0, (unsigned char)(90 * fminf(1.0f, a.hitFlash * 4))});
    if (a.alive) {
        if (!a.buildMode) { DrawCircleLines(W / 2, H / 2, 5, Color{255, 255, 255, 200}); DrawRectangle(W / 2, H / 2, 1, 1, WHITE); }
        else DrawRectangle(W / 2 - 1, H / 2 - 1, 3, 3, WHITE);
        Compass(a, W);
        Minimap(a, w, W, info);
        
        int sx = W - 66, sy = 64;
        int t = (int)fmaxf(0, info.stormT); char buf[32]; snprintf(buf, sizeof buf, "%d:%02d", t / 60, t % 60);
        DrawRectangle(sx, sy + 1, 7, 7, info.shrinking ? Color{200, 100, 255, 255} : Color{140, 200, 255, 255});
        Txt(buf, sx + 10, sy, 12, WHITE);
        DrawCircle(sx + 36, sy + 4, 4, WHITE); snprintf(buf, sizeof buf, "%d", info.alive); Txt(buf, sx + 43, sy, 12, WHITE);
        DrawCircleLines(sx + 56, sy + 4, 4, WHITE); snprintf(buf, sizeof buf, "%d", a.kills); Txt(buf, sx + 60, sy + 14, 12, WHITE);
        
        // Health / Shield (Bottom Left)
        int bx = 22, by = H - 26, bw = 108;
        
        DrawRectangle(bx, by, bw, 8, Color{20, 20, 20, 190}); 
        DrawRectangle(bx, by, (int)(bw * a.shield / 100.0f), 8, Color{80, 160, 255, 255});
        snprintf(buf, sizeof buf, "%d", (int)a.shield); 
        Txt(buf, bx + 4, by - 2, 12, WHITE);
        
        const Texture2D* shieldIcon = gIcons.UIItem("Shield");
        if (shieldIcon && shieldIcon->id != 0) {
            Rectangle src = {0, 0, (float)shieldIcon->width, (float)shieldIcon->height};
            Rectangle dst = {(float)(bx - 16), (float)(by - 2), 14, 14};
            DrawTexturePro(*shieldIcon, src, dst, {0, 0}, 0, WHITE);
        } else {
            DrawRectangle(bx - 14, by, 10, 8, Color{200, 220, 255, 255}); DrawRectangle(bx - 12, by + 1, 7, 7, WHITE); DrawRectangle(bx - 11, by - 0, 1, 1, WHITE);
        }
        
        int hy = by + 12;
        DrawRectangle(bx, hy, bw, 9, Color{20, 20, 20, 190}); 
        DrawRectangle(bx, hy, (int)(bw * a.health / 100.0f), 9, Color{80, 200, 60, 255});
        snprintf(buf, sizeof buf, "%d", (int)ceilf(a.health)); 
        Txt(buf, bx + 4, hy - 2, 12, WHITE);
        
        const Texture2D* healthIcon = gIcons.UIItem("Health");
        if (healthIcon && healthIcon->id != 0) {
            Rectangle src = {0, 0, (float)healthIcon->width, (float)healthIcon->height};
            Rectangle dst = {(float)(bx - 16), (float)(hy - 2), 14, 14};
            DrawTexturePro(*healthIcon, src, dst, {0, 0}, 0, WHITE);
        } else {
            DrawRectangle(bx - 14, hy, 10, 8, Color{200, 220, 255, 255}); DrawRectangle(bx - 12, hy + 1, 7, 7, WHITE); DrawRectangle(bx - 11, hy - 0, 1, 1, WHITE);
        }
        
        // Hotbar (Bottom Right)
        const int S = 20, gap = 2; int hx = W - 6 - 6 * (S + gap), hy2 = H - S - 5;
        for (int i = 0; i < 6; i++) {
            int x = hx + i * (S + gap);
            
            // --- FIXED: Rarity gradients and item icons now apply to slot 0 (Pickaxe) too ---
            if (!a.slots[i].Empty()) {
                Color rc = RarityColor(a.slots[i].rar);
                Color mid = { (unsigned char)(rc.r * 0.6f), (unsigned char)(rc.g * 0.6f), (unsigned char)(rc.b * 0.6f), 220 };
                Color dark = { (unsigned char)(rc.r * 0.15f), (unsigned char)(rc.g * 0.15f), (unsigned char)(rc.b * 0.15f), 220 };
                DrawRectangleGradientV(x, hy2, S, S/2, rc, mid);
                DrawRectangleGradientV(x, hy2 + S/2, S, S/2, mid, dark);
            } else {
                DrawRectangle(x, hy2, S, S, Color{40, 60, 80, 170});
            }
            
            DrawRectangleLines(x, hy2, S, S, (i == a.sel && !a.buildMode) ? Color{255, 255, 255, 255} : Color{200, 215, 235, 100});
            
            if (!a.slots[i].Empty()) {
                ItemIcon(a.slots[i], x, hy2, S);
                const ItemDef& d = GetItemDef(a.slots[i].id);
                if (d.kind == K_WEAPON) { 
                    snprintf(buf, sizeof buf, "%d", a.slots[i].mag); 
                    Txt(buf, x + 2, hy2 + S - 9, 10, Color{255, 255, 255, 230}); 
                }
                else if (d.kind != K_PICKAXE) { 
                    snprintf(buf, sizeof buf, "%d", a.slots[i].count); 
                    Txt(buf, x + S - (int)MeasureTextEx(gFont, buf, 10, 1.0f).x - 2, hy2 + S - 9, 10, WHITE); 
                }
            }
            snprintf(buf, sizeof buf, "%d", i + 1); Txt(buf, x + 2, hy2 + 1, 9, Color{255, 255, 255, 140});
        }
        
        int by2 = hy2 - S - 4;
        for (int i = 0; i < 5; i++) {
            int x = hx + i * (S + gap) + (S + gap) / 2 - 2;
            DrawRectangle(x, by2, S, S, Color{40, 90, 130, 170});
            bool selp = a.buildMode && i == (int)a.piece;
            DrawRectangleLines(x, by2, S, S, selp ? Color{255, 255, 255, 255} : Color{200, 215, 235, 100});
            if (i < 4) Icon((PieceType)i, x, by2, S, Color{180, 225, 255, 230});
        }
        
        int my = by2 - 28;
        const char* matNames[3] = {"Wood", "Brick", "Metal"};
        for (int i = 0; i < 3; i++) {
            int x = hx + 8 + i * 26;
            DrawRectangle(x, my, 14, 14, Color{0, 0, 0, 110});
            
            const Texture2D* matTex = gIcons.UIItem(matNames[i]);
            if (matTex && matTex->id != 0) {
                Rectangle src = {0, 0, (float)matTex->width, (float)matTex->height};
                Rectangle dst = {(float)(x + 1), (float)(my + 1), 12, 12};
                DrawTexturePro(*matTex, src, dst, {0, 0}, 0, WHITE);
            } else {
                DrawRectangle(x + 2, my + 2, 10, 10, kMatColor[i]);
            }
            
            if (a.buildMode && a.mat == i) DrawRectangleLines(x - 1, my - 1, 16, 16, WHITE);
            snprintf(buf, sizeof buf, "%d", a.mats[i]); Txt(buf, x + 7 - (int)(MeasureTextEx(gFont, buf, 12, 1.0f).x / 2), my + 15, 12, WHITE);
        }
        
        if (!a.prompt.empty()) CTxt(("[E] " + a.prompt).c_str(), W / 2, H / 2 + 24, 12, Color{255, 240, 120, 255});
        if (a.useT > 0 && a.sel > 0 && !a.slots[a.sel].Empty()) {
            float need = GetItemDef(a.slots[a.sel].id).useTime; if (need > 0) { DrawRectangle(W / 2 - 25, H / 2 + 38, 50, 5, Color{0, 0, 0, 160}); DrawRectangle(W / 2 - 25, H / 2 + 38, (int)(50 * a.useT / need), 5, Color{120, 255, 120, 255}); }
        }
        if (a.reloadT > 0) CTxt("Reloading...", W / 2, H / 2 + 38, 12, WHITE);
        if (a.toastT > 0) CTxt(a.toast.c_str(), W / 2, H / 2 - 40, 12, Color{255, 255, 255, 230});
    }
    
    int fy = 20;
    for (const auto& m : *info.feed) { if (m.t > 0) { Txt(m.s.c_str(), 6, fy, 11, WHITE); fy += 13; } }
    
    if (info.over) {
        DrawRectangle(0, H / 2 - 30, W, 60, Color{0, 0, 0, 170});
        if (info.won) {
            UICentred("Victory Royale", W, H / 2, (float)W, 100.0f, "VICTORY ROYALE", 40, Color{255, 220, 60, 255});
        } else {
            char b2[32]; snprintf(b2, sizeof b2, "YOU PLACED #%d", info.placement);
            CTxt(b2, W / 2, H / 2 - 20, 24, Color{255, 120, 120, 255});
        }
        CTxt("ENTER - back to lobby", W / 2, H / 2 + 8, 11, WHITE);
    } else if (!a.alive) CTxt("ELIMINATED - spectating", W / 2, H / 2, 12, Color{255, 120, 120, 255});
}