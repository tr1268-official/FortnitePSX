#pragma once
#include "Common.h"
#include "MeshBuilder.h"

// PSX-style rendering: vertex snapping to the low-res pixel grid, 15-bit colour + ordered dither,
// per-vertex lighting, distance haze, point-filtered procedural textures.
class Renderer {
public:
    void Init();
    void Shutdown();
    void SetResolution(float w, float h);
    Model MakeModel(MeshBuilder& mb, const Texture2D* tex = nullptr);
    Model MakeStormModel(MeshBuilder& mb);   // uses the animated storm shader
    // Flush the batch and push following draws slightly towards the camera, so flush ground decals
    // (roads, lots, lines) never z-fight with the ground. Always pair with DecalEnd().
    void DecalBegin(float factor, float units);
    void DecalEnd();
    void FreeModel(Model& m);               // unloads meshes only (shader/textures are shared)
    Texture2D MakeTex(Color a, Color b, int pattern);

    Shader shader{}, stormShader{};
    int locRes = -1, locStormRes = -1, locStormTime = -1;
    Texture2D texGrass{}, texSand{}, texDirt{}, texWood{}, texMetal{}, texConcrete{}, texNoise{};
    Color fogColor = {189, 163, 179, 255};
};
extern Renderer gR;