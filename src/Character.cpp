#include "Character.h"
#include "Skydive.h"   // full AirPose definition
#include "Items.h"
#include "rlgl.h"
#include <cstring>
#include <cmath>

// Display names for locker / UI (must stay in sync with kOutfitNames in Game.cpp)
const LookInfo kLooks[kLookCount] = {
    { "Jonesy",              "jonesy" },
    { "Ramirez",             "ramirez" },
    { "Headhunter",          "headhunter" },
    { "Wildcat",             "wildcat" },
    { "Aerial Assault",      "arial_assault_trooper" },
    { "Renegade Raider",     "renegade_raider" },
    { "Ghoul Trooper",       "ghoul_trooper" },
    { "Skull Trooper",       "skull_trooper" },
    { "Brite Bomber",        "brite_bomber" },
    { "Punk",                "punk_funk" },
    { "Gingerbread",         "ginge" },
    { "Merry Marauder",      "merry_maruader" },
};

// ---------------------------------------------------------------------------
// Outfit → GLB filename map  (Season 0/1 names)
// Put the files under assets/models/ or assets/characters/
// ---------------------------------------------------------------------------
static const char* kLookGLB[kLookCount] = {
    "M_Med_Soldier_04.glb",           // 0  jonesy  (use the file you sent for now)
    "F_Med_Soldier_01.glb",           // 1  ramirez
    "F_Med_Soldier_01.glb",           // 2  headhunter
    "F_Med_Soldier_01.glb",           // 3  wildcat
    "M_Med_Soldier_04.glb",           // 4  arial_assault_trooper
    "F_Med_Soldier_01.glb",           // 5  renegade_raider
    "F_Med_Soldier_01.glb",           // 6  ghoul_trooper
    "M_Med_Soldier_04.glb",           // 7  skull_trooper
    "F_Med_Soldier_01.glb",           // 8  brite_bomber
    "M_Med_Soldier_04.glb",           // 9  punk_funk
    "F_Med_Soldier_01.glb",           // 10 ginge
    "M_Med_Soldier_04.glb",           // 11 merry_marauder
};

// ---------------------------------------------------------------------------
// Texture sets per look.
// Files are expected as  <folder>/<prefix>_baseColor.png
//                        <folder>/<prefix>_metallicRoughness.png
//                        <folder>/<prefix>_normal.png
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Texture sets per look. Each slot carries the exact filenames.
// A nullptr means "don't touch that map" (leaves whatever the GLB had).
// ---------------------------------------------------------------------------
struct LookTextures {
    const char* folder;          // subfolder under models/
    const char* bodyAlbedo;
    const char* bodyMR;
    const char* bodyNormal;
    const char* headAlbedo;
    const char* headMR;
    const char* headNormal;
    const char* hairAlbedo;
    const char* hairMR;
    const char* hairNormal;
};

// Ramirez — F_MED_Commando PBR set
static const LookTextures kRamirezTex = {
    "F_Med_Soldier_01",
    "F_MED_Commando_01_baseColor.png",
    "F_MED_Commando_01_metallicRoughness.png",
    "F_MED_Commando_01_normal.png",
    "F_MED_Commando_Head_01_baseColor.png",
    "F_MED_Commando_Head_01_metallicRoughness.png",
    "F_MED_Commando_Head_01_normal.png",
    "F_MED_Commando_Hair_01_baseColor.png",
    "F_MED_Commando_Hair_01_metallicRoughness.png",
    "F_MED_Commando_Hair_01_normal.png",
};

// Jonesy — M_Med_Soldier_04 set.  No metallicRoughness maps provided;
// hair has diffuse only.
static const LookTextures kJonesyTex = {
    "M_Med_Soldier_04",
    "M_Med_Soldier_Body_01_C.png",
    nullptr,                                   // no body MR
    "M_Med_Soldier_Body_01_N.png",
    "M_Med_Soldier_Head_01_C.png",
    nullptr,                                   // no head MR
    "M_Med_Soldier_Head_01_N.png",
    "M_SML_Survivor_Hair_02_d.png",
    nullptr,                                   // no hair MR
    nullptr,                                   // no hair normal
};

static const LookTextures* kLookTex[kLookCount] = {
    &kJonesyTex,     // 0  jonesy
    &kRamirezTex,    // 1  ramirez
    &kRamirezTex,    // 2  headhunter
    &kRamirezTex,    // 3  wildcat
    &kJonesyTex,    // 4  arial_assault_trooper
    &kRamirezTex,    // 5  renegade_raider
    &kRamirezTex,    // 6  ghoul_trooper
    &kJonesyTex,    // 7  skull_trooper
    &kRamirezTex,    // 8  brite_bomber
    &kJonesyTex,    // 9  punk_funk
    &kRamirezTex,    // 10 ginge
    &kJonesyTex,    // 11 merry_marauder
};

// Head/hair are told apart by vertex count (head = fewer verts than hair).
// If the head shows hair texture or vice versa, flip this.
static const bool kSwapHeadHair = false;

// Per-look scale / feet offset if a model needs tweaking
static const float   kLookScale [kLookCount] = { 1.0f,1.0f,1.0f,1.0f,1.0f,1.0f,1.0f,1.0f,1.0f,1.0f,1.0f,1.0f };
static const Vector3 kLookOffset[kLookCount] = {};

static std::vector<std::string> ModelRoots() {
    std::vector<std::string> roots;
    roots.push_back("");
    roots.push_back("assets/models/");
    roots.push_back("assets/characters/");
    roots.push_back("assets/");
    const char* appDir = GetApplicationDirectory();
    if (appDir && appDir[0]) {
        std::string d = appDir;
        if (!d.empty() && d.back() != '/' && d.back() != '\\') d += '/';
        roots.push_back(d);
        roots.push_back(d + "assets/models/");
        roots.push_back(d + "assets/characters/");
        roots.push_back(d + "../assets/models/");
        roots.push_back(d + "../");
    }
    return roots;
}

// ---------------------------------------------------------------------------
// Texture helpers
// ---------------------------------------------------------------------------
static bool LoadTexFromRoots(const char* folder, const char* file, Texture2D& out) {
    for (const auto& root : ModelRoots()) {
        std::string candidates[2] = {
            root + folder + "/" + file,
            root + file
        };
        for (const auto& p : candidates) {
            if (!FileExists(p.c_str())) continue;

            // Load the raw image so we can flip it (glTF/Unreal UV origin is
            // bottom-left; raylib's image loader assumes top-left).
            Image img = LoadImage(p.c_str());
            if (img.data == nullptr) continue;

            ImageFlipVertical(&img);   // <-- fix: match glTF V convention

            Texture2D t = LoadTextureFromImage(img);
            UnloadImage(img);

            if (t.id == 0) continue;
            GenTextureMipmaps(&t);
            SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
            SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
            out = t;
            return true;
        }
    }
    return false;
}

// Replace a material map's texture, freeing the old one (unless it is raylib's default).
static void SetMapTexture(Material& mat, int mapIndex, Texture2D t) {
    Texture2D old = mat.maps[mapIndex].texture;
    if (old.id != 0 && old.id != rlGetTextureIdDefault()) UnloadTexture(old);
    mat.maps[mapIndex].texture = t;
}

// Assign one texture set to a material. Each material loads its own copies
// because UnloadModel() unloads the textures held in the material maps.
// Assign one albedo/MR/normal triple.  Any nullptr filename skips that map,
// leaving the texture loaded by the GLB in place.
static void ApplyTextureSet(Material& mat,
                            const char* folder,
                            const char* albedo,
                            const char* mr,
                            const char* nrm)
{
    Texture2D t;

    if (albedo && LoadTexFromRoots(folder, albedo, t)) {
        SetMapTexture(mat, MATERIAL_MAP_ALBEDO, t);
        mat.maps[MATERIAL_MAP_ALBEDO].color = WHITE;
    } else if (albedo) {
        TraceLog(LOG_WARNING, "Character: missing albedo %s/%s", folder, albedo);
    }

    if (mr && LoadTexFromRoots(folder, mr, t)) {
        SetMapTexture(mat, MATERIAL_MAP_METALNESS, t);
        mat.maps[MATERIAL_MAP_METALNESS].value = 1.0f;
    } else if (mr) {
        TraceLog(LOG_WARNING, "Character: missing MR %s/%s", folder, mr);
    }

    if (nrm && LoadTexFromRoots(folder, nrm, t)) {
        SetMapTexture(mat, MATERIAL_MAP_NORMAL, t);
    } else if (nrm) {
        TraceLog(LOG_WARNING, "Character: missing normal %s/%s", folder, nrm);
    }
}

// Unreal exports often carry vertex colours used as masks (black / red).
// raylib's shader multiplies the texture by vertex colour, which gives a black
// model with red patches. Force every vertex colour to white and re-upload.
static void ForceWhiteVertexColors(Model& model) {
    for (int i = 0; i < model.meshCount; i++) {
        Mesh& m = model.meshes[i];
        if (!m.colors) {
            TraceLog(LOG_INFO, "Character: mesh %d has no vertex colours", i);
            continue;
        }
        TraceLog(LOG_INFO, "Character: mesh %d vertex colour[0] = (%d,%d,%d,%d) -> forcing white",
                 i, m.colors[0], m.colors[1], m.colors[2], m.colors[3]);
        for (int v = 0; v < m.vertexCount * 4; v++) m.colors[v] = 255;
        // vbo index 3 = colours
        UpdateMeshBuffer(m, 3, m.colors, m.vertexCount * 4, 0);
    }
}

// Work out which material is body / head / hair and apply the texture sets.
// raylib's glTF loader puts a default material at index 0 that no mesh uses,
// so only materials actually referenced by meshes are considered.
static void ApplyLookTextures(Model& model, const LookTextures& tex) {
    int matCount = model.materialCount;
    if (matCount <= 0 || !model.meshMaterial) return;

    std::vector<float> minY(matCount,  1e30f);
    std::vector<float> maxY(matCount, -1e30f);
    std::vector<int>   verts(matCount, 0);

    for (int i = 0; i < model.meshCount; i++) {
        int mi = model.meshMaterial[i];
        if (mi < 0 || mi >= matCount) continue;
        BoundingBox bb = GetMeshBoundingBox(model.meshes[i]);
        if (bb.min.y < minY[mi]) minY[mi] = bb.min.y;
        if (bb.max.y > maxY[mi]) maxY[mi] = bb.max.y;
        verts[mi] += model.meshes[i].vertexCount;
    }

    std::vector<float> height(matCount, 0.0f);
    std::vector<int>   used;   // materials referenced by at least one mesh
    for (int m = 0; m < matCount; m++) {
        if (verts[m] > 0) {
            height[m] = maxY[m] - minY[m];
            used.push_back(m);
        }
    }
    if (used.empty()) return;

    // 0 = body, 1 = head, 2 = hair, -1 = unused
    std::vector<int> part(matCount, -1);

    // body = tallest used material
    int bodyMat = used[0];
    for (int m : used) if (height[m] > height[bodyMat]) bodyMat = m;
    part[bodyMat] = 0;

    // remaining used materials: fewer verts = head, more verts = hair
    std::vector<int> rest;
    for (int m : used) if (m != bodyMat) rest.push_back(m);

    if (rest.size() == 1) {
        part[rest[0]] = kSwapHeadHair ? 2 : 1;
    } else if (rest.size() >= 2) {
        int a = rest[0], b = rest[1];
        int headMat = (verts[a] <= verts[b]) ? a : b;
        int hairMat = (headMat == a) ? b : a;
        if (kSwapHeadHair) { int tmp = headMat; headMat = hairMat; hairMat = tmp; }
        part[headMat] = 1;
        part[hairMat] = 2;
    }

        static const char* kPartNames[3] = { "body", "head", "hair" };
    for (int m = 0; m < matCount; m++) {
        TraceLog(LOG_INFO, "Character: material %d  verts=%d  height=%.2f  -> %s",
                 m, verts[m], height[m],
                 part[m] >= 0 ? kPartNames[part[m]] : "(unused)");
        if (part[m] < 0) continue;

        const char* a = nullptr;
        const char* r = nullptr;
        const char* n = nullptr;
        switch (part[m]) {
            case 0: a = tex.bodyAlbedo; r = tex.bodyMR; n = tex.bodyNormal; break;
            case 1: a = tex.headAlbedo; r = tex.headMR; n = tex.headNormal; break;
            case 2: a = tex.hairAlbedo; r = tex.hairMR; n = tex.hairNormal; break;
        }
        ApplyTextureSet(model.materials[m], tex.folder, a, r, n);
    }
}

bool LoadCharacterGLB(CharacterModel& c, const char* filename, float scale) {
    if (c.loaded) {
        CharacterUnload(c);
    }
    c = {};
    c.scale = scale;

    for (const auto& root : ModelRoots()) {
        std::string path = root + filename;
        if (!FileExists(path.c_str())) continue;

        Model m = LoadModel(path.c_str());
        if (m.meshCount <= 0) {
            UnloadModel(m);
            continue;
        }

        c.model  = m;
        c.loaded = true;

        int animCount = 0;
        ModelAnimation* anims = LoadModelAnimations(path.c_str(), &animCount);
        if (anims && animCount > 0) {
            c.anims     = anims;
            c.animCount = animCount;
            c.animFrame = 0;
        }

        TraceLog(LOG_INFO, "Character: loaded %s  meshes=%d  materials=%d  anims=%d",
                 path.c_str(), c.model.meshCount, c.model.materialCount, c.animCount);
        return true;
    }

    TraceLog(LOG_WARNING, "Character: could not load %s (searched assets/models, assets/characters, cwd)", filename);
    return false;
}

void CharacterBuild(CharacterModel& c, int lookIndex) {
    if (lookIndex < 0 || lookIndex >= kLookCount) lookIndex = 0;
    const char* file = kLookGLB[lookIndex];
    float scale = kLookScale[lookIndex];
    if (!LoadCharacterGLB(c, file, scale)) {
        c.scale  = 1.0f;
        c.offset = kLookOffset[lookIndex];
    } else {
        c.offset = kLookOffset[lookIndex];
        ForceWhiteVertexColors(c.model);
        if (kLookTex[lookIndex]) {
            ApplyLookTextures(c.model, *kLookTex[lookIndex]);
        }
    }
}

void CharacterUnload(CharacterModel& c) {
    if (c.anims && c.animCount > 0) {
        UnloadModelAnimations(c.anims, c.animCount);
        c.anims = nullptr;
        c.animCount = 0;
    }
    if (c.loaded) {
        // UnloadModel also unloads the textures stored in the material maps
        UnloadModel(c.model);
        c.model  = {};
        c.loaded = false;
    }
}

// Simple capsule fallback when no GLB is present
static void DrawFallbackBody(Vector3 pos, float yaw, const Pose& p, Color tint) {
    float bodyH = 1.15f;
    float headR = 0.18f;
    Vector3 feet = pos;
    Vector3 hips = { pos.x, pos.y + 0.55f, pos.z };
    Vector3 head = { pos.x, pos.y + 1.45f, pos.z };

    // slight bob when moving
    float bob = p.moveAmt * 0.03f * sinf(p.phase * 2.0f);
    hips.y += bob;
    head.y += bob;

    DrawCylinderEx(feet, hips, 0.22f, 0.28f, 8, tint);
    DrawCylinderEx(hips, Vector3{hips.x, hips.y + bodyH * 0.55f, hips.z}, 0.28f, 0.24f, 8, tint);
    DrawSphere(head, headR, tint);

    // arms
    float armSwing = p.moveAmt * 0.4f * sinf(p.phase);
    Vector3 rShoulder = { hips.x + cosf(yaw) * 0.32f, hips.y + 0.9f, hips.z - sinf(yaw) * 0.32f };
    Vector3 lShoulder = { hips.x - cosf(yaw) * 0.32f, hips.y + 0.9f, hips.z + sinf(yaw) * 0.32f };
    Vector3 rHand = {
        rShoulder.x + sinf(yaw) * 0.35f,
        rShoulder.y - 0.45f + armSwing,
        rShoulder.z + cosf(yaw) * 0.35f
    };
    Vector3 lHand = {
        lShoulder.x - sinf(yaw) * 0.35f,
        lShoulder.y - 0.45f - armSwing,
        lShoulder.z - cosf(yaw) * 0.35f
    };
    DrawCylinderEx(rShoulder, rHand, 0.07f, 0.06f, 6, tint);
    DrawCylinderEx(lShoulder, lHand, 0.07f, 0.06f, 6, tint);
}

void CharacterDraw(const CharacterModel& c, const HeldModels& held,
                   Vector3 pos, float yaw, const Pose& p, Color tint)
{
    Vector3 drawPos = {
        pos.x + c.offset.x,
        pos.y + c.offset.y,
        pos.z + c.offset.z
    };

    if (c.loaded) {
        // Advance animation if we have clips
        if (c.anims && c.animCount > 0) {
            ModelAnimation& anim = c.anims[0];
            int frame = (int)(GetTime() * 30.0f) % anim.frameCount;
            UpdateModelAnimation(*(Model*)&c.model, anim, frame);
        }

        // Raylib models usually face +Z; Fortnite/Unreal often face +X – adjust if needed
        float rotDeg = yaw * RAD2DEG;
        DrawModelEx(c.model, drawPos, {0, 1, 0}, rotDeg, {c.scale, c.scale, c.scale}, tint);
    } else {
        DrawFallbackBody(drawPos, yaw, p, tint);
    }

    // Held weapon / pickaxe (simple billboard or small model if available)
    if (p.gun && held.ready && p.weapon >= 0 && p.weapon < 16) {
        const Model& g = held.guns[p.weapon];
        if (g.meshCount > 0) {
            Vector3 hand = {
                pos.x + sinf(yaw) * 0.45f,
                pos.y + 1.05f + (p.ads ? 0.05f : 0.0f),
                pos.z + cosf(yaw) * 0.45f
            };
            float s = 0.55f * p.gunScale;
            DrawModelEx(g, hand, {0, 1, 0}, yaw * RAD2DEG, {s, s, s}, p.gunTint);
        }
    } else if (p.pick && held.ready && held.pick.meshCount > 0) {
        Vector3 hand = {
            pos.x + sinf(yaw) * 0.4f,
            pos.y + 1.0f,
            pos.z + cosf(yaw) * 0.4f
        };
        DrawModelEx(held.pick, hand, {0, 1, 0}, yaw * RAD2DEG, {0.5f, 0.5f, 0.5f}, WHITE);
    }
}

Matrix CharacterDrawAir(const CharacterModel& c, Vector3 pos, float yaw,
                        const AirPose& p, Color tint)
{
    Vector3 drawPos = {
        pos.x + c.offset.x,
        pos.y + c.offset.y,
        pos.z + c.offset.z
    };

    if (c.loaded) {
        // Apply yaw + skydive tilt/roll
        Matrix transform = MatrixIdentity();
        transform = MatrixMultiply(transform, MatrixScale(c.scale, c.scale, c.scale));
        transform = MatrixMultiply(transform, MatrixRotateX(p.tilt));
        transform = MatrixMultiply(transform, MatrixRotateZ(p.roll));
        transform = MatrixMultiply(transform, MatrixRotateY(yaw));
        transform = MatrixMultiply(transform, MatrixTranslate(drawPos.x, drawPos.y, drawPos.z));

        float rotDeg = yaw * RAD2DEG;
        DrawModelEx(c.model, drawPos, {0, 1, 0}, rotDeg, {c.scale, c.scale, c.scale}, tint);
    } else {
        Pose ground{};
        ground.airborne = true;
        ground.moveAmt = 0;
        DrawFallbackBody(drawPos, yaw, ground, tint);
    }

    // Grip matrix for glider attachment (chest height, slightly above with splay)
    float gripY = 1.4f + p.blend * 0.15f;
    return MatrixMultiply(MatrixRotateY(yaw), MatrixTranslate(pos.x, pos.y + gripY, pos.z));
}

// ---- Held items (guns / pickaxe) – minimal placeholders ----
void HeldBuild(HeldModels& h) {
    h = {};
    h.ready = true;
}

void HeldUnload(HeldModels& h) {
    for (int i = 0; i < 16; i++) {
        if (h.guns[i].meshCount > 0) UnloadModel(h.guns[i]);
        h.guns[i] = {};
    }
    if (h.pick.meshCount > 0) UnloadModel(h.pick);
    h.pick = {};
    h.ready = false;
}