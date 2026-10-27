#include "Character.h"
#include "MeshBuilder.h"
#include "Renderer.h"

const Look kLooks[kLookCount] = {
    {"Trooper", C(214,160,120), C(224,192,92),  C(205,200,170), C(78,92,58),   C(138,108,64),  C(72,56,40),  C(38,38,40),  C(222,216,190), false, false, true},
    {"Ranger",  C(206,150,114), C(104,58,38),   C(58,54,50),    C(58,54,50),   C(150,116,70),  C(82,62,42),  C(60,50,42),  C(90,80,70),    true,  true,  false},
    {"Scout",   C(150,104,74),  C(26,20,20),    C(96,42,42),    C(62,62,72),   C(62,72,92),    C(40,40,46),  C(30,30,30),  C(204,62,50),   false, false, true},
    {"Medic",   C(236,190,160), C(190,62,40),   C(230,230,235), C(230,230,235),C(92,92,102),   C(50,50,56),  C(200,200,200),C(220,40,40),   true,  true,  false},
};

static Model Mk(MeshBuilder& mb) { return gR.MakeModel(mb); }

void CharacterBuild(CharacterModel& cm, int li) {
    const Look& L = kLooks[li];
    cm.look = li;
    const bool F = L.female;
    const float shR = F ? 0.165f : 0.195f, chR = F ? 0.155f : 0.185f, waR = F ? 0.125f : 0.145f, hipR = F ? 0.158f : 0.150f;
    const Color belt = C(58, 44, 34), skinD = Mul(L.skin, 0.82f), dark = C(30, 25, 25);
    const Color shirt = L.top, vest = L.hasVest ? L.vest : L.top, collar = L.hasVest ? L.accent : L.top;

    { MeshBuilder mb;
        mb.Loft({{-0.10f, hipR, 0.10f, L.pants}, {0.05f, waR + 0.01f, 0.10f, L.pants}, {0.05f, waR + 0.012f, 0.102f, belt}, {0.10f, waR + 0.012f, 0.102f, belt},
                 {0.10f, waR, 0.095f, shirt}, {0.28f, waR + 0.005f, 0.100f, shirt}, {0.28f, waR + 0.010f, 0.104f, vest}, {0.46f, chR, 0.118f, vest},
                 {0.55f, shR, 0.105f, vest}, {0.58f, 0.085f, 0.07f, collar}, {0.62f, 0.07f, 0.06f, collar}, {0.62f, 0.052f, 0.05f, L.skin}, {0.68f, 0.05f, 0.05f, L.skin}}, 10);
        if (L.hasVest) mb.Box({0, 0.17f, -0.105f}, {0.2f, 0.22f, 0.05f}, Mul(L.vest, 0.7f));
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
        cm.head = Mk(mb);
    }
    { MeshBuilder mb; Color h = L.hair;
        mb.Loft({{0.08f, 0.092f, 0.10f, h, 0, -0.03f}, {0.14f, 0.096f, 0.106f, h}, {0.19f, 0.10f, 0.11f, h}, {0.24f, 0.09f, 0.10f, h},
                 {0.275f, 0.058f, 0.068f, h}, {0.295f, 0.0f, 0.0f, h}}, 9, nullptr, false, true);
        if (F) { mb.Box({-0.098f, 0.11f, -0.01f}, {0.02f, 0.13f, 0.1f}, h); mb.Box({0.098f, 0.11f, -0.01f}, {0.02f, 0.13f, 0.1f}, h); }
        else   { mb.Box({0, 0.19f, 0.075f}, {0.17f, 0.03f, 0.06f}, Mul(h, 1.08f)); }
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
            mb.Loft({{0.04f, 0.062f, 0.062f, L.skin}, {-0.03f, 0.056f, 0.056f, L.skin}, {-0.15f, 0.05f, 0.05f, L.skin}, {-0.28f, 0.043f, 0.043f, L.skin}}, 7);
            cm.armU[i] = Mk(mb);
        }
        { MeshBuilder mb;
            mb.Loft({{0.0f, 0.043f, 0.043f, L.skin}, {-0.10f, 0.038f, 0.038f, L.skin}, {-0.15f, 0.034f, 0.034f, L.skin}, {-0.15f, 0.037f, 0.037f, L.accent},
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