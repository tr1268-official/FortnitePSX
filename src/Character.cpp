#include "Character.h"
#include "MeshBuilder.h"
#include "Renderer.h"
#include "Skydive.h"

const Look kLooks[kLookCount] = {
    {"Jonesy",    C(214,160,120), C(224,192,92),  C(205,200,170), C(78,92,58),   C(138,108,64),  C(72,56,40),  C(38,38,40),  C(222,216,190), false, false, true},
    {"Ramirez",   C(206,150,114), C(104,58,38),   C(58,54,50),    C(58,54,50),   C(150,116,70),  C(82,62,42),  C(60,50,42),  C(90,80,70),    true,  true,  false},
    {"Headhunter",C(150,104,74),  C(26,20,20),    C(96,42,42),    C(62,62,72),   C(62,72,92),    C(40,40,46),  C(30,30,30),  C(204,62,50),   false, false, true},
    {"Wildcat",   C(236,190,160), C(190,62,40),   C(230,230,235), C(230,230,235),C(92,92,102),   C(50,50,56),  C(200,200,200),C(220,40,40),   true,  true,  false},
    // ---- 4..11: same order as outfitCid[] / kOutfitNames[] in Game.cpp (CID_017, 028, 029, 030, 038, 044, 048, 049)
    //  name                     skin            hair            top             vest            pants           boots           gloves          accent          F      pony   vest
    {"Aerial Assault Trooper", C(206,160,124), C(70,52,36),    C(88,98,62),    C(60,68,48),    C(84,92,60),    C(46,42,36),    C(36,36,34),    C(120,128,92),  false, false, true},
    {"Renegade Raider",        C(226,178,142), C(120,70,40),   C(150,108,66),  C(112,78,46),   C(96,86,64),    C(52,38,30),    C(60,44,34),    C(210,170,70),  true,  true,  true},
    {"Ghoul Trooper",          C(150,170,140), C(40,36,34),    C(92,100,74),   C(70,78,58),    C(74,82,62),    C(40,38,34),    C(60,64,52),    C(110,120,90),  true,  false, false},
    {"Skull Trooper",          C(232,230,218), C(20,20,22),    C(24,24,28),    C(24,24,28),    C(26,26,30),    C(20,20,22),    C(224,222,210), C(224,222,210), false, false, false},
    {"Funk Ops",               C(176,120,86),  C(30,22,20),    C(236,206,96),  C(236,206,96),  C(70,56,140),   C(80,220,240),  C(176,120,86),  C(70,110,230),  false, false, false},
    {"Sparkle Specialist",     C(236,196,166), C(240,90,170),  C(225,228,240), C(225,228,240), C(200,150,230), C(235,235,245), C(240,100,180), C(120,220,255), true,  true,  false},
    {"Ginger Gunner",          C(186,120,66),  C(120,40,40),   C(168,104,56),  C(168,104,56),  C(150,92,50),   C(236,236,236), C(240,240,240), C(240,240,240), true,  false, false},
    {"Merry Marauder",         C(186,120,66),  C(240,240,240), C(168,104,56),  C(168,104,56),  C(60,110,60),   C(180,40,40),   C(240,240,240), C(240,240,240), false, false, false},
};

static Model Mk(MeshBuilder& mb) { return gR.MakeModel(mb); }

// ---------------------------------------------------------------------------------------- outfit extras
// Headgear / face / chest details that make each outfit read like its in-game counterpart.
// Hair-model coordinates are head-local (y 0..0.29, face looks down +Z); torso coordinates are torso-local (front = +Z).
static const Color kWhite = C(240, 240, 240), kBone = C(226, 224, 210), kBlack = C(18, 18, 22);

static void Dome(MeshBuilder& mb, float s, float y0, Color c) {            // helmet / cap shell over the top of the head
    mb.Loft({{y0, 0.108f * s, 0.116f * s, c}, {0.20f, 0.116f * s, 0.122f * s, c}, {0.245f, 0.108f * s, 0.114f * s, c},
             {0.278f, 0.072f * s, 0.082f * s, c}, {0.298f, 0.0f, 0.0f, c}}, 10, nullptr, false, true);
}
static void Band(MeshBuilder& mb, float y0, float y1, float s, Color c) {  // open ring (head band, fur trim, strap)
    mb.Loft({{y0, 0.109f * s, 0.116f * s, c}, {y1, 0.109f * s, 0.116f * s, c}}, 10, nullptr, false, false);
}

static void ArmColors(int li, const Look& L, Color& up, Color& fore) {
    switch (li) {
        case 4: case 5: case 7: case 9: up = L.top; fore = L.top; break;   // long sleeves
        case 6: up = L.top; fore = L.skin; break;                          // ragged sleeve, bare forearm
        default: break;                                                    // original four + Funk Ops + gingerbread: bare / skin-coloured
    }
}

static void ExtraHead(int li, MeshBuilder& mb) {
    switch (li) {
        case 6:   // Ghoul Trooper: glowing eyes
            mb.Box({-0.034f, 0.135f, 0.103f}, {0.030f, 0.018f, 0.008f}, C(190, 255, 110));
            mb.Box({ 0.034f, 0.135f, 0.103f}, {0.030f, 0.018f, 0.008f}, C(190, 255, 110));
            break;
        case 7:   // Skull Trooper: skull face
            mb.Box({-0.036f, 0.135f, 0.100f}, {0.044f, 0.050f, 0.014f}, kBlack);
            mb.Box({ 0.036f, 0.135f, 0.100f}, {0.044f, 0.050f, 0.014f}, kBlack);
            mb.Box({0, 0.102f, 0.100f}, {0.022f, 0.030f, 0.014f}, kBlack);
            mb.Box({0, 0.060f, 0.097f}, {0.074f, 0.024f, 0.012f}, C(238, 236, 224));
            for (int k = 0; k < 5; k++) mb.Box({-0.030f + k * 0.015f, 0.060f, 0.101f}, {0.004f, 0.026f, 0.012f}, kBlack);
            break;
        case 10: case 11:   // gingerbread: icing eyes, smile and candy cheeks
            for (int sgn = -1; sgn <= 1; sgn += 2) {
                mb.Box({sgn * 0.034f, 0.135f, 0.100f}, {0.032f, 0.032f, 0.012f}, C(245, 245, 245));
                mb.Box({sgn * 0.034f, 0.135f, 0.107f}, {0.014f, 0.016f, 0.008f}, kBlack);
                mb.Box({sgn * 0.058f, 0.090f, 0.093f}, {0.020f, 0.020f, 0.012f}, C(220, 70, 70));
                mb.Box({sgn * 0.032f, 0.080f, 0.100f}, {0.010f, 0.012f, 0.012f}, kWhite);
            }
            mb.Box({0, 0.072f, 0.100f}, {0.060f, 0.010f, 0.012f}, kWhite);
            break;
        default: break;
    }
}

static void ExtraHair(int li, const Look& L, MeshBuilder& mb) {
    switch (li) {
        case 4: {   // Aerial Assault Trooper: olive helmet, brim, chin strap, goggles
            Color hm = C(66, 78, 52);
            Dome(mb, 1.0f, 0.14f, hm);
            mb.Box({0, 0.205f, 0.118f}, {0.20f, 0.014f, 0.045f}, Mul(hm, 0.85f));
            mb.Box({-0.108f, 0.10f, 0.0f}, {0.012f, 0.07f, 0.03f}, C(30, 30, 28));
            mb.Box({ 0.108f, 0.10f, 0.0f}, {0.012f, 0.07f, 0.03f}, C(30, 30, 28));
            mb.Box({-0.04f, 0.228f, 0.120f}, {0.05f, 0.03f, 0.02f}, C(150, 190, 200));
            mb.Box({ 0.04f, 0.228f, 0.120f}, {0.05f, 0.03f, 0.02f}, C(150, 190, 200));
            break;
        }
        case 5: {   // Renegade Raider: leather aviator cap, ear flaps, amber goggles
            Color lc = C(112, 74, 44);
            Dome(mb, 1.0f, 0.14f, lc);
            mb.Box({-0.112f, 0.10f, 0.0f}, {0.02f, 0.10f, 0.09f}, lc);
            mb.Box({ 0.112f, 0.10f, 0.0f}, {0.02f, 0.10f, 0.09f}, lc);
            Band(mb, 0.195f, 0.215f, 1.02f, C(60, 42, 30));
            for (int sgn = -1; sgn <= 1; sgn += 2) {
                mb.Box({sgn * 0.04f, 0.205f, 0.122f}, {0.055f, 0.050f, 0.025f}, C(40, 40, 44));
                mb.Box({sgn * 0.04f, 0.205f, 0.136f}, {0.040f, 0.036f, 0.010f}, C(240, 190, 70));
            }
            break;
        }
        case 6: {   // Ghoul Trooper: broken helmet shell + straggly hair
            Dome(mb, 1.0f, 0.18f, C(66, 72, 54));
            mb.Box({-0.10f, 0.08f, -0.04f}, {0.03f, 0.20f, 0.07f}, L.hair);
            mb.Box({ 0.10f, 0.06f, -0.05f}, {0.03f, 0.22f, 0.07f}, L.hair);
            mb.Box({0, 0.06f, -0.11f}, {0.16f, 0.22f, 0.03f}, L.hair);
            break;
        }
        case 7:     // Skull Trooper: black cowl
            Dome(mb, 1.0f, 0.14f, C(22, 22, 26));
            break;
        case 8: {   // Funk Ops: big afro, blue headband, sunglasses
            Color h = L.hair;
            mb.Loft({{0.06f, 0.10f, 0.07f, h, 0, -0.06f}, {0.12f, 0.19f, 0.13f, h, 0, -0.07f}, {0.19f, 0.22f, 0.15f, h, 0, -0.07f},
                     {0.25f, 0.21f, 0.17f, h, 0, -0.05f}, {0.31f, 0.16f, 0.15f, h, 0, -0.04f}, {0.36f, 0.08f, 0.08f, h, 0, -0.03f},
                     {0.385f, 0.0f, 0.0f, h, 0, -0.03f}}, 10, nullptr, false, true);
            Band(mb, 0.185f, 0.205f, 1.0f, C(60, 110, 235));
            Color gl = C(24, 24, 30);
            mb.Box({-0.036f, 0.138f, 0.104f}, {0.052f, 0.040f, 0.012f}, gl);
            mb.Box({ 0.036f, 0.138f, 0.104f}, {0.052f, 0.040f, 0.012f}, gl);
            mb.Box({0, 0.145f, 0.104f}, {0.020f, 0.010f, 0.012f}, gl);
            mb.Box({-0.092f, 0.140f, 0.05f}, {0.010f, 0.010f, 0.10f}, gl);
            mb.Box({ 0.092f, 0.140f, 0.05f}, {0.010f, 0.010f, 0.10f}, gl);
            break;
        }
        case 9: {   // Sparkle Specialist: cyan goggle band on the forehead + star hair clip
            Band(mb, 0.19f, 0.21f, 1.0f, L.accent);
            mb.Box({-0.04f, 0.225f, 0.112f}, {0.05f, 0.04f, 0.02f}, L.accent);
            mb.Box({ 0.04f, 0.225f, 0.112f}, {0.05f, 0.04f, 0.02f}, L.accent);
            mb.Box({-0.10f, 0.20f, 0.04f}, {0.02f, 0.04f, 0.04f}, C(255, 240, 120));
            break;
        }
        case 10: {  // Ginger Gunner: icing band + red candy bow
            Band(mb, 0.19f, 0.205f, 0.99f, kWhite);
            mb.Box({0, 0.285f, 0.02f}, {0.04f, 0.03f, 0.03f}, C(210, 40, 50));
            mb.Box({-0.04f, 0.285f, 0.02f}, {0.04f, 0.05f, 0.03f}, C(210, 40, 50));
            mb.Box({ 0.04f, 0.285f, 0.02f}, {0.04f, 0.05f, 0.03f}, C(210, 40, 50));
            break;
        }
        case 11: {  // Merry Marauder: red knit cap with white fur trim and pom
            Dome(mb, 1.0f, 0.14f, C(190, 36, 40));
            Band(mb, 0.13f, 0.19f, 1.05f, kWhite);
            mb.Loft({{0.29f, 0.03f, 0.03f, kWhite}, {0.31f, 0.05f, 0.05f, kWhite}, {0.335f, 0.04f, 0.04f, kWhite}, {0.35f, 0.0f, 0.0f, kWhite}}, 8, nullptr, false, true);
            break;
        }
        default: break;
    }
}

static void ExtraTorso(int li, const Look& L, MeshBuilder& mb) {
    switch (li) {
        case 4:     // chest pouches + belt pouch
            mb.Box({-0.09f, 0.34f, 0.115f}, {0.07f, 0.075f, 0.04f}, Mul(L.vest, 0.8f));
            mb.Box({ 0.09f, 0.34f, 0.115f}, {0.07f, 0.075f, 0.04f}, Mul(L.vest, 0.8f));
            mb.Box({0.13f, 0.12f, 0.04f}, {0.05f, 0.07f, 0.07f}, C(50, 50, 46));
            break;
        case 5: {   // bandolier across the chest
            Matrix m = MatrixMultiply(MatrixRotateZ(0.7f), MatrixTranslate(0, 0.34f, 0.112f));
            mb.Box({0, 0, 0}, {0.34f, 0.05f, 0.025f}, C(70, 48, 32), &m);
            mb.Box({-0.06f, 0.28f, 0.122f}, {0.03f, 0.05f, 0.02f}, L.accent);
            mb.Box({ 0.00f, 0.34f, 0.122f}, {0.03f, 0.05f, 0.02f}, L.accent);
            mb.Box({ 0.06f, 0.40f, 0.122f}, {0.03f, 0.05f, 0.02f}, L.accent);
            break;
        }
        case 6:     // torn shirt: a few exposed ribs
            for (int k = 0; k < 3; k++) mb.Box({0.03f, 0.32f + k * 0.045f, 0.110f}, {0.12f, 0.012f, 0.02f}, kBone);
            break;
        case 7:     // skeleton print: ribs, sternum, spine, pelvis, collar bones
            for (int k = 0; k < 4; k++) mb.Box({0, 0.30f + k * 0.045f, 0.108f}, {0.20f - k * 0.01f, 0.014f, 0.02f}, kBone);
            mb.Box({0, 0.37f, 0.112f}, {0.02f, 0.16f, 0.02f}, kBone);
            mb.Box({0, 0.33f, -0.108f}, {0.02f, 0.28f, 0.02f}, kBone);
            mb.Box({0, 0.12f, 0.100f}, {0.14f, 0.05f, 0.02f}, kBone);
            mb.Box({-0.07f, 0.53f, 0.09f}, {0.13f, 0.015f, 0.03f}, kBone);
            mb.Box({ 0.07f, 0.53f, 0.09f}, {0.13f, 0.015f, 0.03f}, kBone);
            break;
        case 8:     // sparkly shirt, open collar, gold chain
            for (int k = 0; k < 12; k++)
                mb.Box({(((k * 37) % 17) - 8) * 0.014f, 0.20f + ((k * 53) % 11) * 0.03f, 0.108f}, {0.012f, 0.012f, 0.012f}, C(255, 250, 220));
            mb.Box({0, 0.50f, 0.105f}, {0.07f, 0.09f, 0.02f}, L.skin);
            mb.Box({0, 0.46f, 0.116f}, {0.03f, 0.03f, 0.01f}, C(255, 215, 80));
            break;
        case 9: {   // chest star + shoulder pads
            mb.Box({0, 0.40f, 0.118f}, {0.07f, 0.07f, 0.012f}, C(240, 100, 180));
            Matrix m = MatrixMultiply(MatrixRotateZ(0.785f), MatrixTranslate(0, 0.40f, 0.119f));
            mb.Box({0, 0, 0}, {0.07f, 0.07f, 0.012f}, C(240, 100, 180), &m);
            mb.Box({-0.185f, 0.53f, 0.0f}, {0.06f, 0.03f, 0.12f}, L.accent);
            mb.Box({ 0.185f, 0.53f, 0.0f}, {0.06f, 0.03f, 0.12f}, L.accent);
            break;
        }
        case 10: case 11:   // icing collar + candy buttons
            for (int k = 0; k < 5; k++) mb.Box({-0.08f + k * 0.04f, 0.50f, 0.105f}, {0.035f, 0.02f, 0.015f}, kWhite);
            for (int k = 0; k < 3; k++) mb.Box({0, 0.42f - k * 0.09f, 0.115f}, {0.03f, 0.03f, 0.015f}, (k % 2 == 0) ? C(210, 40, 50) : C(60, 150, 70));
            break;
        default: break;
    }
}

void CharacterBuild(CharacterModel& cm, int li) {
    const Look& L = kLooks[li];
    cm.look = li;
    const bool F = L.female;
    const float shR = F ? 0.165f : 0.195f, chR = F ? 0.155f : 0.185f, waR = F ? 0.125f : 0.145f, hipR = F ? 0.158f : 0.150f;
    const Color belt = C(58, 44, 34), skinD = Mul(L.skin, 0.82f), dark = C(30, 25, 25);
    const Color shirt = L.top, vest = L.hasVest ? L.vest : L.top, collar = L.hasVest ? L.accent : L.top;
    Color upC = L.skin, foreC = L.skin;
    ArmColors(li, L, upC, foreC);

    { MeshBuilder mb;
        mb.Loft({{-0.10f, hipR, 0.10f, L.pants}, {0.05f, waR + 0.01f, 0.10f, L.pants}, {0.05f, waR + 0.012f, 0.102f, belt}, {0.10f, waR + 0.012f, 0.102f, belt},
                 {0.10f, waR, 0.095f, shirt}, {0.28f, waR + 0.005f, 0.100f, shirt}, {0.28f, waR + 0.010f, 0.104f, vest}, {0.46f, chR, 0.118f, vest},
                 {0.55f, shR, 0.105f, vest}, {0.58f, 0.085f, 0.07f, collar}, {0.62f, 0.07f, 0.06f, collar}, {0.62f, 0.052f, 0.05f, L.skin}, {0.68f, 0.05f, 0.05f, L.skin}}, 10);
        if (L.hasVest) mb.Box({0, 0.17f, -0.105f}, {0.2f, 0.22f, 0.05f}, Mul(L.vest, 0.7f));
        ExtraTorso(li, L, mb);
        cm.torso = Mk(mb);
    }
    { MeshBuilder mb;
        mb.Loft({{0.00f, 0.052f, 0.05f, L.skin}, {0.05f, 0.056f, 0.056f, L.skin}, {0.09f, 0.075f, 0.085f, L.skin}, {0.13f, 0.085f, 0.095f, L.skin},
                 {0.19f, 0.09f, 0.10f, L.skin}, {0.24f, 0.08f, 0.09f, L.skin}, {0.27f, 0.05f, 0.06f, L.skin}, {0.285f, 0.0f, 0.0f, L.skin}}, 9);
        mb.Box({0, 0.105f, 0.100f}, {0.028f, 0.045f, 0.03f}, skinD);
        mb.Box({-0.034f, 0.135f, 0.097f}, {0.026f, 0.012f, 0.01f}, dark);
        mb.Box({0.034f, 0.135f, 0.097f}, {0.026f, 0.012f, 0.01f}, dark);
        mb.Box({-0.034f, 0.158f, 0.096f}, {0.034f, 0.008f, 0.01f}, Mul(L.hair, 0.7f));
        mb.Box({0.034f, 0.158f, 0.096f}, {0.034f, 0.008f, 0.01f}, Mul(L.hair, 0.7f));
        mb.Box({0, 0.062f, 0.092f}, {0.05f, 0.01f, 0.01f}, F ? C(170, 70, 80) : skinD);
        mb.Box({-0.088f, 0.12f, 0.0f}, {0.014f, 0.04f, 0.03f}, skinD);
        mb.Box({0.088f, 0.12f, 0.0f}, {0.014f, 0.04f, 0.03f}, skinD);
        ExtraHead(li, mb);
        cm.head = Mk(mb);
    }
    { MeshBuilder mb; Color h = L.hair;
        mb.Loft({{0.08f, 0.092f, 0.10f, h, 0, -0.03f}, {0.14f, 0.096f, 0.106f, h}, {0.19f, 0.10f, 0.11f, h}, {0.24f, 0.09f, 0.10f, h},
                 {0.275f, 0.058f, 0.068f, h}, {0.295f, 0.0f, 0.0f, h}}, 9, nullptr, false, true);
        if (F) { mb.Box({-0.098f, 0.11f, -0.01f}, {0.02f, 0.13f, 0.1f}, h); mb.Box({0.098f, 0.11f, -0.01f}, {0.02f, 0.13f, 0.1f}, h); }
        else   { mb.Box({0, 0.19f, 0.075f}, {0.17f, 0.03f, 0.06f}, Mul(h, 1.08f)); }
        ExtraHair(li, L, mb);
        cm.hair = Mk(mb);
    }
    { MeshBuilder mb;
        if (F && L.ponytail) mb.Loft({{0.02f, 0.03f, 0.03f, L.accent}, {0.00f, 0.034f, 0.034f, L.accent}, {-0.01f, 0.04f, 0.04f, L.hair}, {-0.10f, 0.05f, 0.045f, L.hair},
                                      {-0.22f, 0.042f, 0.038f, L.hair}, {-0.33f, 0.0f, 0.0f, L.hair}}, 6);
        else mb.Box({0, 0, 0}, {0.001f, 0.001f, 0.001f}, L.hair);
        cm.tail = Mk(mb);
    }
    for (int i = 0; i < 2; i++) {
        { MeshBuilder mb;
            mb.Loft({{0.04f, 0.062f, 0.062f, upC}, {-0.03f, 0.056f, 0.056f, upC}, {-0.15f, 0.05f, 0.05f, upC}, {-0.28f, 0.043f, 0.043f, upC}}, 7);
            cm.armU[i] = Mk(mb);
        }
        { MeshBuilder mb;
            mb.Loft({{0.0f, 0.043f, 0.043f, foreC}, {-0.10f, 0.038f, 0.038f, foreC}, {-0.15f, 0.034f, 0.034f, foreC}, {-0.15f, 0.037f, 0.037f, L.accent},
                     {-0.19f, 0.037f, 0.037f, L.accent}, {-0.19f, 0.035f, 0.030f, L.gloves}, {-0.25f, 0.040f, 0.022f, L.gloves}, {-0.31f, 0.036f, 0.020f, L.gloves},
                     {-0.34f, 0.0f, 0.0f, L.gloves}}, 7);
            cm.armL[i] = Mk(mb);
        }
        { MeshBuilder mb;
            mb.Loft({{0.0f, 0.090f, 0.090f, L.pants}, {-0.15f, 0.085f, 0.085f, L.pants}, {-0.30f, 0.072f, 0.072f, L.pants}, {-0.45f, 0.062f, 0.062f, L.pants}}, 8);
            if (i == 0 && !F) mb.Box({-0.088f, -0.2f, 0.0f}, {0.04f, 0.14f, 0.075f}, C(45, 36, 28));
            cm.legU[i] = Mk(mb);
        }
        { MeshBuilder mb;
            mb.Loft({{0.0f, 0.062f, 0.062f, L.pants}, {-0.15f, 0.052f, 0.052f, L.pants}, {-0.28f, 0.045f, 0.045f, L.pants}, {-0.28f, 0.052f, 0.052f, L.boots},
                     {-0.41f, 0.044f, 0.044f, L.boots}, {-0.44f, 0.044f, 0.044f, L.boots}}, 7);
            Matrix xf = MatrixMultiply(MatrixRotateX(kPi / 2), MatrixTranslate(0, -0.455f, 0));
            mb.Loft({{-0.07f, 0.034f, 0.034f, L.boots}, {-0.02f, 0.046f, 0.040f, L.boots}, {0.10f, 0.050f, 0.036f, L.boots}, {0.19f, 0.042f, 0.026f, L.boots}, {0.25f, 0.018f, 0.014f, L.boots}}, 7, &xf);
            cm.legL[i] = Mk(mb);
        }
    }
}

void CharacterUnload(CharacterModel& cm) {
    gR.FreeModel(cm.head); gR.FreeModel(cm.hair); gR.FreeModel(cm.tail); gR.FreeModel(cm.torso);
    for (int i = 0; i < 2; i++) { gR.FreeModel(cm.armU[i]); gR.FreeModel(cm.armL[i]); gR.FreeModel(cm.legU[i]); gR.FreeModel(cm.legL[i]); }
}

// ---------------------------------------------------------------------------------------- guns
// All guns: barrel along +Z, top along +Y, origin at the grip. Built from boxes + tubes (low-poly, flat colour).
static void Tube(MeshBuilder& mb, float x, float y, float z0, float z1, float r, Color c, int sides = 8) {
    Matrix xf = MatrixMultiply(MatrixRotateX(kPi / 2), MatrixTranslate(x, y, 0));
    mb.Loft({{z0, r, r, c}, {z1, r, r, c}}, sides, &xf, true, true);
}
static void TubeT(MeshBuilder& mb, float x, float y, float z0, float z1, float r0, float r1, Color c, int sides = 8) {
    Matrix xf = MatrixMultiply(MatrixRotateX(kPi / 2), MatrixTranslate(x, y, 0));
    mb.Loft({{z0, r0, r0, c}, {z1, r1, r1, c}}, sides, &xf, true, true);
}
static void Tilt(MeshBuilder& mb, Vector3 c, Vector3 s, float rx, Color col) {   // box tilted about X around its own centre
    Matrix m = MatrixMultiply(MatrixRotateX(rx), MatrixTranslate(c.x, c.y, c.z));
    mb.Box({0, 0, 0}, s, col, &m);
}

static Model GunPistol() {
    MeshBuilder mb; Color sl = C(78, 80, 90), fr = C(44, 44, 50), gp = C(110, 74, 46), st = C(150, 152, 160);
    mb.Box({0, 0.040f, 0.07f}, {0.030f, 0.046f, 0.215f}, sl);               // slide
    mb.Box({0, 0.004f, 0.06f}, {0.034f, 0.030f, 0.180f}, fr);               // frame
    Tilt(mb, {0, -0.055f, -0.012f}, {0.034f, 0.115f, 0.052f}, -0.22f, gp);   // raked grip
    mb.Box({0, -0.028f, 0.075f}, {0.010f, 0.012f, 0.075f}, fr);             // trigger guard
    mb.Box({0, -0.012f, 0.045f}, {0.008f, 0.024f, 0.008f}, fr);             // trigger
    Tube(mb, 0, 0.040f, 0.175f, 0.205f, 0.0095f, st);                       // barrel tip
    mb.Box({0, 0.068f, -0.025f}, {0.012f, 0.012f, 0.014f}, fr);             // rear sight
    mb.Box({0, 0.066f, 0.17f}, {0.006f, 0.012f, 0.008f}, fr);               // front sight
    for (int k = 0; k < 4; k++) mb.Box({0, 0.040f, -0.02f + k * 0.012f}, {0.032f, 0.040f, 0.004f}, Mul(sl, 0.7f)); // slide serrations
    return gR.MakeModel(mb);
}
static Model GunPump() {
    MeshBuilder mb; Color wd = C(112, 74, 44), steel = C(104, 108, 116), dk = C(40, 40, 46);
    Tilt(mb, {0, -0.015f, -0.22f}, {0.046f, 0.095f, 0.32f}, 0.06f, wd);       // wooden stock
    mb.Box({0, -0.01f, -0.395f}, {0.05f, 0.11f, 0.03f}, C(30, 28, 28));      // butt pad
    mb.Box({0, 0.030f, 0.07f}, {0.052f, 0.078f, 0.25f}, steel);              // receiver
    mb.Box({0.028f, 0.035f, 0.07f}, {0.006f, 0.02f, 0.07f}, dk);             // ejection port
    Tilt(mb, {0, -0.03f, -0.07f}, {0.034f, 0.09f, 0.045f}, -0.15f, wd);       // wrist
    mb.Box({0, -0.045f, 0.06f}, {0.010f, 0.016f, 0.08f}, dk);                // trigger guard
    Tube(mb, 0, 0.048f, 0.19f, 0.93f, 0.0165f, dk);                          // barrel
    Tube(mb, 0, -0.002f, 0.20f, 0.80f, 0.0145f, steel);                      // magazine tube
    mb.Box({0, -0.002f, 0.44f}, {0.060f, 0.058f, 0.22f}, wd);                // pump forend
    for (int k = 0; k < 5; k++) mb.Box({0, -0.002f, 0.36f + k * 0.04f}, {0.064f, 0.062f, 0.008f}, Mul(wd, 0.7f)); // grooves
    Tube(mb, 0, -0.002f, 0.80f, 0.84f, 0.020f, dk);                          // tube cap
    mb.Box({0, 0.071f, 0.91f}, {0.006f, 0.012f, 0.008f}, C(230, 220, 160));  // bead sight
    return gR.MakeModel(mb);
}
static Model GunTactical() {
    MeshBuilder mb; Color bk = C(34, 35, 40), dk = C(52, 54, 60), st = C(110, 114, 122);
    mb.Box({0, 0.030f, 0.12f}, {0.062f, 0.095f, 0.44f}, bk);                 // long receiver
    mb.Box({0, 0.084f, 0.14f}, {0.026f, 0.014f, 0.36f}, dk);                 // top rail
    for (int k = 0; k < 7; k++) mb.Box({0, 0.092f, -0.02f + k * 0.05f}, {0.028f, 0.006f, 0.012f}, st); // rail teeth
    mb.Box({0, 0.030f, 0.47f}, {0.066f, 0.080f, 0.20f}, dk);                 // shroud
    for (int k = 0; k < 3; k++) mb.Box({0.034f, 0.030f, 0.41f + k * 0.06f}, {0.004f, 0.04f, 0.03f}, C(14, 14, 16)); // vents
    Tube(mb, 0, 0.040f, 0.57f, 0.74f, 0.017f, st);                           // barrel + muzzle
    Tube(mb, 0, 0.040f, 0.72f, 0.76f, 0.022f, bk);
    Tilt(mb, {0, -0.07f, -0.01f}, {0.036f, 0.125f, 0.05f}, -0.2f, bk);       // pistol grip
    mb.Box({0, -0.05f, 0.22f}, {0.044f, 0.13f, 0.10f}, C(66, 66, 72));       // box magazine
    mb.Box({0, 0.020f, -0.27f}, {0.040f, 0.072f, 0.30f}, dk);                // fixed stock
    mb.Box({0, 0.020f, -0.425f}, {0.046f, 0.09f, 0.03f}, C(24, 24, 26));
    mb.Box({0, 0.082f, 0.55f}, {0.01f, 0.04f, 0.012f}, bk);                  // front post
    return gR.MakeModel(mb);
}
static Model GunAR() {
    MeshBuilder mb; Color ol = C(78, 90, 62), dk = C(40, 42, 46), bk = C(28, 28, 32), st = C(112, 116, 124);
    Tilt(mb, {0, 0.0f, -0.30f}, {0.046f, 0.088f, 0.25f}, 0.05f, dk);         // stock
    mb.Box({0, 0.005f, -0.435f}, {0.05f, 0.105f, 0.03f}, bk);                // butt pad
    mb.Box({0, 0.042f, 0.09f}, {0.052f, 0.072f, 0.34f}, ol);                 // upper receiver
    mb.Box({0, -0.005f, 0.06f}, {0.050f, 0.060f, 0.28f}, dk);                // lower receiver
    Tube(mb, 0, 0.038f, 0.25f, 0.57f, 0.031f, ol, 8);                        // handguard
    for (int k = 0; k < 4; k++) Tube(mb, 0, 0.038f, 0.29f + k * 0.07f, 0.31f + k * 0.07f, 0.034f, dk, 8); // handguard rings
    Tube(mb, 0, 0.040f, 0.57f, 0.83f, 0.0115f, st);                          // barrel
    Tube(mb, 0, 0.040f, 0.83f, 0.89f, 0.0165f, bk);                          // flash hider
    mb.Box({0, 0.100f, 0.05f}, {0.022f, 0.040f, 0.22f}, dk);                 // carry handle
    mb.Box({0, 0.092f, 0.57f}, {0.012f, 0.050f, 0.012f}, bk);                // front sight post
    Tilt(mb, {0, -0.065f, -0.02f}, {0.034f, 0.12f, 0.05f}, -0.25f, bk);      // pistol grip
    Tilt(mb, {0, -0.095f, 0.11f}, {0.032f, 0.15f, 0.052f}, 0.16f, bk);       // curved magazine
    Tilt(mb, {0, -0.17f, 0.095f}, {0.032f, 0.06f, 0.052f}, 0.30f, bk);
    return gR.MakeModel(mb);
}
static Model GunSMG() {
    MeshBuilder mb; Color bd = C(70, 72, 86), dk = C(36, 36, 42), bk = C(26, 26, 30);
    mb.Box({0, 0.025f, 0.12f}, {0.052f, 0.072f, 0.30f}, bd);                 // body
    mb.Box({0, 0.066f, 0.12f}, {0.040f, 0.014f, 0.28f}, dk);                 // top cover
    Tube(mb, 0, 0.030f, 0.27f, 0.43f, 0.021f, dk);                           // shroud
    Tube(mb, 0, 0.030f, 0.43f, 0.48f, 0.0135f, bk);                          // muzzle
    mb.Box({0, 0.020f, -0.14f}, {0.012f, 0.012f, 0.24f}, bk);                // wire stock
    mb.Box({0, -0.010f, -0.26f}, {0.012f, 0.06f, 0.012f}, bk);
    Tilt(mb, {0, -0.055f, 0.0f}, {0.034f, 0.11f, 0.05f}, -0.12f, bk);        // grip
    mb.Box({0, -0.105f, 0.14f}, {0.030f, 0.19f, 0.042f}, bk);                // long straight mag
    mb.Box({0, -0.045f, 0.285f}, {0.026f, 0.06f, 0.032f}, dk);               // foregrip
    mb.Box({0, 0.092f, 0.07f}, {0.032f, 0.032f, 0.052f}, bk);                // red dot
    mb.Box({0, 0.092f, 0.097f}, {0.022f, 0.022f, 0.008f}, C(230, 50, 50));
    return gR.MakeModel(mb);
}
// Scope: body tube, objective bell (front), eyepiece (rear), turrets, mounts and glass lenses.
static void Scope(MeshBuilder& mb, float y, float z0, float z1, float r, float bell, Color body) {
    Color bk = C(24, 24, 28), glass = C(90, 170, 255);
    Tube(mb, 0, y, z0, z1, r, body, 10);
    TubeT(mb, 0, y, z1, z1 + bell, r, r * 1.55f, bk, 10);                    // objective bell
    Tube(mb, 0, y, z1 + bell, z1 + bell + 0.006f, r * 1.45f, glass, 10);    // front lens
    TubeT(mb, 0, y, z0 - 0.07f, z0, r * 1.25f, r, bk, 10);                   // eyepiece
    Tube(mb, 0, y, z0 - 0.075f, z0 - 0.07f, r * 1.1f, C(60, 110, 160), 10);  // rear lens
    mb.Box({0, y + r + 0.008f, z0 + (z1 - z0) * 0.5f}, {0.018f, 0.018f, 0.018f}, bk);          // elevation turret
    mb.Box({r + 0.008f, y, z0 + (z1 - z0) * 0.5f}, {0.018f, 0.018f, 0.018f}, bk);              // windage turret
    mb.Box({0, y - r - 0.012f, z0 + (z1 - z0) * 0.2f}, {0.026f, 0.026f, 0.03f}, bk);           // rear mount
    mb.Box({0, y - r - 0.012f, z0 + (z1 - z0) * 0.8f}, {0.026f, 0.026f, 0.03f}, bk);           // front mount
}
static Model GunHunting() {
    MeshBuilder mb; Color wd = C(118, 80, 48), st = C(100, 104, 112), dk = C(36, 36, 42);
    Tilt(mb, {0, -0.02f, -0.22f}, {0.05f, 0.105f, 0.42f}, 0.07f, wd);          // wooden stock
    mb.Box({0, 0.04f, -0.31f}, {0.045f, 0.03f, 0.18f}, Mul(wd, 1.1f));         // cheek piece
    mb.Box({0, -0.02f, -0.435f}, {0.052f, 0.115f, 0.03f}, C(30, 26, 24));      // butt pad
    mb.Box({0, -0.006f, 0.40f}, {0.044f, 0.048f, 0.52f}, wd);                  // fore-stock
    mb.Box({0, 0.030f, 0.06f}, {0.040f, 0.066f, 0.24f}, st);                   // receiver
    Tube(mb, 0, 0.036f, 0.28f, 1.00f, 0.0125f, dk);                            // barrel
    mb.Box({0.040f, 0.038f, 0.02f}, {0.04f, 0.012f, 0.012f}, st);              // bolt handle
    mb.Box({0.062f, 0.034f, 0.02f}, {0.022f, 0.022f, 0.022f}, st);             // bolt knob
    mb.Box({0, -0.04f, 0.05f}, {0.010f, 0.016f, 0.08f}, dk);                   // trigger guard
    Scope(mb, 0.096f, 0.0f, 0.34f, 0.020f, 0.09f, dk);                         // hunting scope
    return gR.MakeModel(mb);
}
static Model GunSniper() {
    MeshBuilder mb; Color od = C(66, 72, 62), bk = C(28, 30, 34), st = C(108, 112, 120);
    Tilt(mb, {0, -0.01f, -0.24f}, {0.05f, 0.125f, 0.46f}, 0.04f, od);          // chassis stock
    mb.Box({0, 0.075f, -0.31f}, {0.042f, 0.030f, 0.20f}, Mul(od, 1.15f));      // adjustable cheek rest
    mb.Box({0, -0.01f, -0.48f}, {0.052f, 0.13f, 0.03f}, bk);                   // butt pad
    mb.Box({0, 0.030f, 0.12f}, {0.050f, 0.072f, 0.42f}, bk);                   // receiver / chassis
    mb.Box({0, -0.035f, 0.12f}, {0.034f, 0.070f, 0.10f}, C(50, 52, 56));       // magazine
    Tube(mb, 0, 0.040f, 0.33f, 1.10f, 0.0145f, st);                            // heavy barrel
    Tube(mb, 0, 0.040f, 1.10f, 1.19f, 0.0225f, bk);                            // muzzle brake
    for (int k = 0; k < 3; k++) mb.Box({0, 0.040f, 1.12f + k * 0.025f}, {0.05f, 0.01f, 0.008f}, C(8, 8, 8)); // brake slots
    mb.Box({0.050f, 0.040f, 0.00f}, {0.06f, 0.012f, 0.012f}, st);              // bolt handle
    mb.Box({0.085f, 0.036f, 0.00f}, {0.024f, 0.024f, 0.024f}, bk);             // bolt knob
    mb.Box({0.026f, -0.060f, 0.78f}, {0.008f, 0.11f, 0.008f}, st);             // bipod legs
    mb.Box({-0.026f, -0.060f, 0.78f}, {0.008f, 0.11f, 0.008f}, st);
    mb.Box({0, -0.004f, 0.78f}, {0.07f, 0.014f, 0.02f}, bk);
    Tilt(mb, {0, -0.065f, -0.03f}, {0.034f, 0.11f, 0.05f}, -0.2f, bk);         // grip
    Scope(mb, 0.108f, -0.02f, 0.50f, 0.026f, 0.13f, bk);                       // long high-power scope
    return gR.MakeModel(mb);
}
static Model GunRocket() {
    MeshBuilder mb; Color ol = C(86, 94, 70), dk = C(44, 46, 50), rd = C(180, 48, 40);
    Tube(mb, 0, 0.06f, -0.32f, 0.76f, 0.075f, ol, 10);                         // launch tube
    TubeT(mb, 0, 0.06f, -0.45f, -0.32f, 0.10f, 0.075f, dk, 10);                // rear flare
    TubeT(mb, 0, 0.06f, 0.76f, 0.92f, 0.075f, 0.098f, dk, 10);                 // front flare
    Tube(mb, 0, 0.06f, 0.30f, 0.37f, 0.079f, rd, 10);                          // warning band
    TubeT(mb, 0, 0.06f, 0.90f, 1.06f, 0.060f, 0.0f, rd, 10);                   // rocket nose
    Tilt(mb, {0, -0.065f, 0.0f}, {0.036f, 0.12f, 0.05f}, -0.15f, dk);          // trigger grip
    Tilt(mb, {0, -0.065f, 0.38f}, {0.034f, 0.11f, 0.05f}, 0.05f, dk);          // fore grip
    mb.Box({0, 0.150f, 0.10f}, {0.020f, 0.050f, 0.14f}, dk);                   // optic
    mb.Box({0, 0.165f, 0.10f}, {0.014f, 0.020f, 0.02f}, C(90, 170, 255));
    mb.Box({0, 0.095f, 0.62f}, {0.012f, 0.045f, 0.014f}, dk);                  // front sight
    mb.Box({0, 0.07f, -0.34f}, {0.12f, 0.15f, 0.04f}, C(30, 28, 28));          // shoulder pad
    return gR.MakeModel(mb);
}

void HeldBuild(HeldModels& h) {
    { MeshBuilder mb; Color wd = C(120, 80, 45), mt = C(120, 125, 135), rd = C(165, 40, 40);
        mb.Loft({{-0.10f, 0.018f, 0.018f, wd}, {0.55f, 0.016f, 0.016f, wd}, {0.62f, 0.02f, 0.02f, wd}}, 6);
        Matrix hx = MatrixMultiply(MatrixRotateZ(kPi / 2), MatrixTranslate(0, 0.58f, 0));
        mb.Loft({{-0.30f, 0.004f, 0.004f, mt}, {-0.17f, 0.024f, 0.014f, mt}, {0.0f, 0.03f, 0.018f, rd}, {0.17f, 0.024f, 0.014f, mt}, {0.30f, 0.004f, 0.004f, mt}}, 4, &hx);
        h.pick = Mk(mb);
    }
    h.guns[I_PISTOL]   = GunPistol();
    h.guns[I_PUMP]     = GunPump();
    h.guns[I_TACTICAL] = GunTactical();
    h.guns[I_AR]       = GunAR();
    h.guns[I_SMG]      = GunSMG();
    h.guns[I_HUNTING]  = GunHunting();
    h.guns[I_SNIPER]   = GunSniper();
    h.guns[I_ROCKET]   = GunRocket();
}
void HeldUnload(HeldModels& h) { for (int i = 0; i < I_COUNT; i++) gR.FreeModel(h.guns[i]); gR.FreeModel(h.pick); }

static void Part(const Model& m, const Matrix& mat, Color tint) {
    Model t = m; t.transform = mat;
    DrawModel(t, Vector3{0, 0, 0}, 1.0f, tint);
}
static Matrix At(Matrix local, const Matrix& parent) { return MatrixMultiply(local, parent); }

void CharacterDraw(const CharacterModel& cm, const HeldModels& held, Vector3 pos, float yaw, const Pose& p, Color tint) {
    const Look& L = kLooks[cm.look];
    const float shX = L.female ? 0.165f : 0.19f, m = p.moveAmt, ph = p.phase;
    const float sw = sinf(ph) * m;
    const float bob = fabsf(cosf(ph)) * 0.035f * m, idle = sinf(p.idle * 1.7f) * 0.006f;

    Matrix root = MatrixMultiply(MatrixRotateY(yaw), MatrixTranslate(pos.x, pos.y, pos.z));
    Matrix pelvis = At(MatrixTranslate(0, 0.93f + bob + idle, 0), root);
    Matrix torso = At(MatrixRotateX(0.10f * m + (p.gun ? 0.05f : 0.0f)), pelvis);
    Part(cm.torso, torso, tint);

    Matrix hd = At(MatrixMultiply(MatrixRotateX(-p.pitch * 0.6f), MatrixTranslate(0, 0.62f, 0)), torso);
    Part(cm.head, hd, tint); Part(cm.hair, hd, tint);
    float tailA = 0.35f + 0.3f * m * sinf(ph * 2.0f) + 0.1f * sinf(p.idle * 2.0f);
    Part(cm.tail, At(MatrixMultiply(MatrixRotateX(tailA), MatrixTranslate(0, 0.2f, -0.09f)), hd), tint);

    for (int i = 0; i < 2; i++) {
        float lp = ph + (i == 0 ? 0.0f : kPi);
        float th = -sinf(lp) * 0.75f * m, kn = fmaxf(0.0f, cosf(lp)) * 1.0f * m;
        if (p.airborne) { th = (i == 0) ? -0.5f : 0.25f; kn = (i == 0) ? 0.9f : 0.3f; }
        float hx = (i == 0) ? -0.085f : 0.085f;
        Matrix t = At(MatrixMultiply(MatrixRotateX(th), MatrixTranslate(hx, 0, 0)), pelvis);
        Part(cm.legU[i], t, tint);
        Matrix s = At(MatrixMultiply(MatrixRotateX(kn), MatrixTranslate(0, -0.45f, 0)), t);
        Part(cm.legL[i], s, tint);
    }

    float aU[2], aF[2];
    aU[0] = sw * 0.6f; aU[1] = -sw * 0.6f; aF[0] = aF[1] = -0.2f - 0.4f * m;
    if (p.gun) {
        if (p.ads) {
            aU[0] = -(kPi / 2 * 0.9f + p.pitch); aF[0] = -0.4f;
            aU[1] = -(1.1f + p.pitch);            aF[1] = -0.6f;
        } else {
            aU[0] = -(kPi / 2 * 0.97f + p.pitch); aF[0] = -0.1f;
            aU[1] = -(1.25f + p.pitch);            aF[1] = -0.35f;
        }
    } else if (p.pick) {
        aU[0] = -0.35f + sw * 0.15f; aF[0] = -1.1f;
        if (p.swing >= 0) { aU[0] = Lerp(-2.7f, 0.1f, p.swing); aF[0] = Lerp(-0.5f, -0.3f, p.swing); }
    }
    
    Matrix fore[2];
    for (int i = 0; i < 2; i++) {
        float sx = (i == 0) ? -1.0f : 1.0f;
        float splay = (p.gun ? (p.ads ? 0.0f : sx * -0.06f) : sx * -0.06f);
        Matrix u = At(MatrixMultiply(MatrixMultiply(MatrixRotateZ(splay), MatrixRotateX(aU[i])), MatrixTranslate(sx * shX, 0.54f, 0)), torso);
        Part(cm.armU[i], u, tint);
        fore[i] = At(MatrixMultiply(MatrixRotateX(aF[i]), MatrixTranslate(0, -0.28f, 0)), u);
        Part(cm.armL[i], fore[i], tint);
    }
    if (p.gun) {
        Matrix g = MatrixMultiply(MatrixMultiply(MatrixScale(p.gunScale, p.gunScale, p.gunScale), MatrixRotateX(kPi / 2)), MatrixTranslate(0, -0.30f, 0));
        if (p.ads) g = MatrixMultiply(g, MatrixTranslate(0, 0.1f, 0.1f)); // pull closer to face
        const Model* gm = (p.weapon > 0 && p.weapon < I_COUNT && held.guns[p.weapon].meshCount > 0) ? &held.guns[p.weapon] : &held.guns[I_AR];
        Part(*gm, At(g, fore[0]), Mix(WHITE, p.gunTint, 0.35f));
    } else if (p.pick) {
        Matrix g = MatrixMultiply(MatrixRotateX(2.3f), MatrixTranslate(0, -0.30f, 0));
        Part(held.pick, At(g, fore[0]), tint);
    }
}

// ---------------------------------------------------------------------------------------- skydiving / gliding pose
// blend 0: freefall - belly-down, arms and legs spread, head lifted to look forward.
// blend 1: hanging from a glider - arms overhead on the grip, legs dangling and swaying.
// The whole body pivots around a point that slides from the pelvis (freefall) up to the grip (glider), so tilt/bank
// look right in both modes. Returns the grip-frame matrix (feed it to DrawGlider).
Matrix CharacterDrawAir(const CharacterModel& cm, Vector3 pos, float yaw, const AirPose& p, Color tint) {
    const Look& L = kLooks[cm.look];
    const float shX = L.female ? 0.165f : 0.19f;
    const float k = Clamp(p.blend, 0.0f, 1.0f), t = p.time;
    const float gripH = Lerp(0.93f, 2.05f, k);

    Matrix grip = MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixRotateX(p.tilt), MatrixRotateZ(p.roll)), MatrixRotateY(yaw)),
                                 MatrixTranslate(pos.x, pos.y + gripH, pos.z));
    Matrix root = MatrixMultiply(MatrixTranslate(0, -gripH, 0), grip);

    const float flap = sinf(t * 11.0f) * (1.0f - k);      // wind flutter while falling
    const float sway = sinf(t * 1.7f) * k;                // gentle swing under the canopy

    Matrix pelvis = At(MatrixTranslate(0, 0.93f, 0), root);
    Matrix torso = At(MatrixRotateX(Lerp(-0.22f, 0.04f, k)), pelvis);                 // slight arch in freefall
    Part(cm.torso, torso, tint);

    const float headRot = -(p.tilt - 0.45f) * 0.8f * (1.0f - k);                       // lift the head to look ahead
    Matrix hd = At(MatrixMultiply(MatrixRotateX(headRot), MatrixTranslate(0, 0.62f, 0)), torso);
    Part(cm.head, hd, tint); Part(cm.hair, hd, tint);
    const float tailA = Lerp(0.70f + 0.15f * flap, 0.30f + 0.10f * sway, k);
    Part(cm.tail, At(MatrixMultiply(MatrixRotateX(tailA), MatrixTranslate(0, 0.2f, -0.09f)), hd), tint);

    for (int i = 0; i < 2; i++) {
        const float sx = (i == 0) ? -1.0f : 1.0f;
        const float spread = Lerp(0.30f + 0.05f * flap, 0.05f, k);
        const float th = Lerp(0.35f, 0.12f + 0.10f * sinf(t * 1.6f + i * 2.3f), k);
        const float kn = Lerp(0.70f, 0.35f + 0.25f * fmaxf(0.0f, sinf(t * 1.6f + i * 2.3f + 1.0f)), k);
        Matrix thigh = At(MatrixMultiply(MatrixMultiply(MatrixRotateZ(sx * spread), MatrixRotateX(th)), MatrixTranslate(sx * 0.085f, 0, 0)), pelvis);
        Part(cm.legU[i], thigh, tint);
        Part(cm.legL[i], At(MatrixMultiply(MatrixRotateX(kn), MatrixTranslate(0, -0.45f, 0)), thigh), tint);
    }

    for (int i = 0; i < 2; i++) {
        const float sx = (i == 0) ? -1.0f : 1.0f;
        const float aZ = Lerp(sx * (1.5708f + 0.45f + 0.10f * flap), sx * p.splay, k);   // out to the sides -> onto the grip
        const float aX = Lerp(0.0f, -(kPi - 0.25f), k);                                   // hanging down -> straight overhead
        Matrix u = At(MatrixMultiply(MatrixMultiply(MatrixRotateZ(aZ), MatrixRotateX(aX)), MatrixTranslate(sx * shX, 0.54f, 0)), torso);
        Part(cm.armU[i], u, tint);
        Matrix f = At(MatrixMultiply(MatrixRotateX(Lerp(0.15f, -0.12f, k)), MatrixTranslate(0, -0.28f, 0)), u);
        Part(cm.armL[i], f, tint);
    }
    return grip;
}
