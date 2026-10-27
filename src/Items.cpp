#include "Items.h"

static ItemDef kDefs[I_COUNT];
static bool kInit = false;

static void InitDefs() {
    kInit = true;
    auto W = [](ItemId i, const char* n, float wt, Rarity minR, Rarity maxR, float dmg, float rate, float rel, float spr, float rng, int mag, int pel, AmmoType am, bool au, bool ex, float rad, Color ic) {
        kDefs[i] = ItemDef{n, K_WEAPON, minR, wt, 1, dmg, rate, rel, spr, rng, mag, pel, am, au, ex, rad, 0, 0, 0, 0, 0, 0, ic};
    };
    
    // Ensure this name matches your PNG exactly: "Harvesting Tool.png"
    kDefs[I_PICKAXE] = ItemDef{"Harvesting Tool", K_PICKAXE, R_COMMON, 0, 1, 20, 1.8f, 0, 0, 3.5f, 0, 1, AM_NONE, false, false, 0, 0, 0, 0, 0, 0, 0, C(170, 170, 180)};
    
    W(I_PISTOL,   "Pistol",             8, R_COMMON, R_COMMON, 24,   6.75f, 1.5f, 0.020f, 150, 16, 1,  AM_LIGHT,   false, false, 0,    C(150, 150, 160));
    W(I_PUMP,     "Pump Shotgun",       9, R_UNCOMMON, R_RARE, 9,    0.70f, 5.0f, 0.100f, 35,  5,  10, AM_SHELLS,  false, false, 0,    C(190, 120, 60));
    W(I_TACTICAL, "Tactical Shotgun",   6, R_RARE, R_RARE, 7.7f, 2.00f, 6.3f, 0.090f, 35,  8,  10, AM_SHELLS,  true,  false, 0,    C(200, 140, 70));
    W(I_AR,       "Assault Rifle",      10, R_COMMON, R_RARE, 30,  5.50f, 2.3f, 0.012f, 250, 30, 1,  AM_MEDIUM,  true,  false, 0,    C(120, 130, 90));
    W(I_SMG,      "Submachine Gun",     8, R_COMMON, R_RARE, 19,   12.0f, 2.2f, 0.030f, 120, 30, 1,  AM_LIGHT,   true,  false, 0,    C(110, 110, 125));
    W(I_HUNTING,  "Hunting Rifle",      4, R_UNCOMMON, R_RARE, 75,   1.00f, 2.4f, 0.003f, 400, 1,  1,  AM_HEAVY,   false, false, 0,    C(140, 100, 60));
    W(I_SNIPER,   "Bolt-Action Sniper Rifle", 3, R_RARE, R_EPIC, 105,  0.33f, 3.0f, 0.001f, 500, 1,  1,  AM_HEAVY,   false, false, 0,    C(90, 100, 110));
    W(I_ROCKET,   "Rocket Launcher",    2, R_RARE, R_LEGENDARY, 100,  0.50f, 2.5f, 0.005f, 300, 1,  1,  AM_ROCKETS, false, true,  6.0f, C(170, 60, 50));
    
    auto H = [](ItemId i, const char* n, Rarity r, float wt, int st, float heal, float hm, float sh, float sm, float use, Color ic) {
        kDefs[i] = ItemDef{n, K_HEAL, r, wt, st, 0, 0, 0, 0, 0, 0, 0, AM_NONE, false, false, 0, heal, hm, sh, sm, use, 0, ic};
    };
    H(I_BANDAGE,      "Bandage",             R_COMMON,   12, 15, 15, 75,  0,  0,   4.0f,  C(235, 235, 235));
    H(I_MEDKIT,       "Med Kit",             R_RARE,     4,  3,  100, 100, 0, 0,   10.0f, C(235, 60, 60));
    H(I_SMALL_SHIELD, "Mini Shield",         R_UNCOMMON, 10, 10, 0,  0,   25, 50,  2.0f,  C(90, 170, 255));
    H(I_SHIELD,       "Shield Potion",       R_RARE,     5,  3,  0,  0,   50, 100, 5.0f,  C(50, 110, 255));
    
    kDefs[I_GRENADE] = ItemDef{"Grenade", K_THROWABLE, R_UNCOMMON, 4, 6, 100, 0, 0, 0, 0, 0, 0, AM_NONE, false, true, 4.5f, 0, 0, 0, 0, 0, 0, C(70, 110, 60)};
    
    auto A = [](ItemId i, const char* n, AmmoType am, float wt, int pack, Color ic) {
        kDefs[i] = ItemDef{n, K_AMMO, R_COMMON, wt, 999, 0, 0, 0, 0, 0, 0, 0, am, false, false, 0, 0, 0, 0, 0, 0, pack, ic};
    };
    A(I_AMMO_LIGHT,   "Light Bullets",  AM_LIGHT,   8, 30, C(220, 200, 90));
    A(I_AMMO_MEDIUM,  "Medium Bullets", AM_MEDIUM,  8, 30, C(220, 200, 90));
    A(I_AMMO_HEAVY,   "Heavy Bullets",  AM_HEAVY,   4, 6,  C(200, 80, 70));
    A(I_AMMO_SHELLS,  "Shells",         AM_SHELLS,  8, 6,  C(220, 60, 50));
    A(I_AMMO_ROCKETS, "Rockets",        AM_ROCKETS, 2, 2,  C(200, 200, 200));
}

const ItemDef& GetItemDef(ItemId id) { if (!kInit) InitDefs(); return kDefs[id]; }

Color RarityColor(Rarity r) {
    static const Color c[5] = {{170, 170, 175, 255}, {90, 200, 60, 255}, {60, 150, 255, 255}, {190, 80, 255, 255}, {255, 190, 40, 255}};
    return c[r];
}
float RarityMult(Rarity r) { static const float m[5] = {1.0f, 1.05f, 1.10f, 1.15f, 1.20f}; return m[r]; }

LootRoll RollLoot(bool chest) {
    if (!kInit) InitDefs();
    float tot = 0;
    for (int i = 1; i < I_COUNT; i++) tot += kDefs[i].weight;
    float r = Rand01() * tot; int pick = I_PISTOL;
    for (int i = 1; i < I_COUNT; i++) { r -= kDefs[i].weight; if (r <= 0) { pick = i; break; } }
    const ItemDef& d = kDefs[pick];
    LootRoll out; out.id = (ItemId)pick; out.rar = d.baseRarity; out.count = 1;
    
    if (d.kind == K_WEAPON) {
        Rarity minR = R_COMMON, maxR = R_LEGENDARY;
        if (pick == I_PISTOL) { minR = R_COMMON; maxR = R_COMMON; }
        else if (pick == I_PUMP) { minR = R_UNCOMMON; maxR = R_RARE; }
        else if (pick == I_TACTICAL) { minR = R_RARE; maxR = R_RARE; }
        else if (pick == I_AR) { minR = R_COMMON; maxR = R_RARE; }
        else if (pick == I_SMG) { minR = R_COMMON; maxR = R_RARE; }
        else if (pick == I_HUNTING) { minR = R_UNCOMMON; maxR = R_RARE; }
        else if (pick == I_SNIPER) { minR = R_RARE; maxR = R_EPIC; }
        else if (pick == I_ROCKET) { minR = R_RARE; maxR = R_LEGENDARY; }
        
        static const float wf[5] = {50, 28, 14, 6, 2}, wc[5] = {8, 28, 36, 20, 8};
        const float* w = chest ? wc : wf; 
        
        float t = 0; for (int i = minR; i <= maxR; i++) t += w[i];
        float q = Rand01() * t; int k = minR; 
        for (; k < maxR; k++) { q -= w[k]; if (q <= 0) break; }
        out.rar = (Rarity)k;
    } 
    else if (d.kind == K_AMMO) out.count = d.ammoPack;
    else if (pick == I_BANDAGE) out.count = RandI(3, 6);
    else if (pick == I_GRENADE) out.count = RandI(1, 3);
    
    return out;
}