#include "Renderer.h"
Renderer gR;

// Pull in the OpenGL function pointers we need for polygon offset.
// Raylib uses GLAD internally, and it's exposed through this header.
#include "external/glad.h"

static const char* kVS = R"GLSL(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform vec2 uRes;
out vec2 fragUV;
out vec4 fragColor;
out float fragFog;
void main() {
    vec4 p = mvp * vec4(vertexPosition, 1.0);
    fragFog = clamp((p.w - 22.0) / (105.0 - 22.0), 0.0, 1.0);
    // PS1 "wobble": snap every vertex to the low-res pixel grid
    vec2 grid = uRes * 0.5;
    vec2 ndc = p.xy / p.w;
    ndc = floor(ndc * grid + 0.5) / grid;
    p.xy = ndc * p.w;
    gl_Position = p;
    vec3 n = normalize(mat3(matModel) * vertexNormal);
    float l = 0.52 + 0.48 * abs(dot(n, normalize(vec3(-0.4, 0.8, 0.35))));
    fragColor = vec4(vertexColor.rgb * l, vertexColor.a);
    fragUV = vertexTexCoord;
}
)GLSL";

static const char* kFS = R"GLSL(#version 330
in vec2 fragUV;
in vec4 fragColor;
in float fragFog;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 uFog;
out vec4 finalColor;
const float bayer[16] = float[16](0.,8.,2.,10., 12.,4.,14.,6., 3.,11.,1.,9., 15.,7.,13.,5.);
void main() {
    vec4 t = texture(texture0, fragUV) * colDiffuse * fragColor;
    if (t.a < 0.04) discard;
    vec3 c = mix(t.rgb, uFog, fragFog);
    int i = int(mod(gl_FragCoord.x, 4.0)) + 4 * int(mod(gl_FragCoord.y, 4.0));
    float d = (bayer[i] / 16.0 - 0.5) / 31.0;
    c = floor((c + d) * 31.0 + 0.5) / 31.0;          // 5 bits per channel
    finalColor = vec4(c, t.a);
}
)GLSL";


static const char* kStormVS = R"GLSL(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform vec2 uRes;
out vec2 fragUV;
out float fragMode;
out float fragH;
void main() {
    vec4 p = mvp * vec4(vertexPosition, 1.0);
    vec2 grid = uRes * 0.5;
    vec2 ndc = p.xy / p.w;
    ndc = floor(ndc * grid + 0.5) / grid;          // same PS1 vertex snap as the world
    p.xy = ndc * p.w;
    gl_Position = p;
    fragUV = vertexTexCoord;
    fragMode = vertexColor.r;                      // 1 = storm wall, 0 = ground haze outside the wall
    fragH = (matModel * vec4(vertexPosition, 1.0)).y;
}
)GLSL";

static const char* kStormFS = R"GLSL(#version 330
in vec2 fragUV;
in float fragMode;
in float fragH;
uniform float uTime;
out vec4 finalColor;
const float bayer[16] = float[16](0.,8.,2.,10., 12.,4.,14.,6., 3.,11.,1.,9., 15.,7.,13.,5.);
float hash(float n) { return fract(sin(n * 12.9898) * 43758.5453); }
void main() {
    vec3 c; float a;
    if (fragMode > 0.5) {
        // Fortnite storm wall: deep violet curtain with hard vertical streaks and bright bands rising up it
        float colId = floor(fragUV.x * 40.0);
        float sp = 0.6 + 0.4 * hash(colId);
        float stripe = step(0.55, fract(fragUV.x * 40.0 + uTime * 0.02));
        float streak = step(0.70, hash(colId * 1.7 + floor(uTime * 0.7 * sp)));
        float rise = step(0.80, fract(fragH * 0.035 - uTime * 0.35 * sp + hash(colId)));
        float k = clamp(fragH / 70.0, 0.0, 1.0);
        c = mix(vec3(0.40, 0.09, 0.68), vec3(0.66, 0.34, 0.92), k);
        c += stripe * vec3(0.06, 0.02, 0.10);
        c += rise * streak * vec3(0.45, 0.30, 0.55);
        a = mix(0.82, 0.50, clamp(fragH / 150.0, 0.0, 1.0)) + 0.08 * rise;
    } else {
        c = vec3(0.28, 0.07, 0.48) + 0.03 * sin(uTime * 0.8);
        a = 0.55;
    }
    c = floor(c * 15.0 + 0.5) / 15.0;                // chunky banding
    int i = int(mod(gl_FragCoord.x, 4.0)) + 4 * int(mod(gl_FragCoord.y, 4.0));
    float d = (bayer[i] / 16.0 - 0.5) / 31.0;
    c = floor((c + d) * 31.0 + 0.5) / 31.0;          // 5 bits per channel
    finalColor = vec4(c, clamp(a, 0.0, 1.0));
}
)GLSL";

Texture2D Renderer::MakeTex(Color a, Color b, int pat) {
    const int S = 32;
    Image img = GenImageColor(S, S, a);
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            float t = Rand01(), f = 1.0f;
            if (pat == 1) { if (y % 8 == 0) f = 0.55f; f *= 0.9f + 0.1f * (float)((y / 8) % 3); }          // planks
            if (pat == 2) f = (x % 4 < 2) ? 1.0f : 0.80f;                                                  // corrugated metal
            if (pat == 3) t = Rand01() * Rand01();                                                         // grass
            ImageDrawPixel(&img, x, y, Mul(Mix(a, b, t), f));
        }
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    return tex;
}

void Renderer::Init() {
    shader = LoadShaderFromMemory(kVS, kFS);
    locRes = GetShaderLocation(shader, "uRes");
    int locFog = GetShaderLocation(shader, "uFog");
    float f[3] = {fogColor.r / 255.0f, fogColor.g / 255.0f, fogColor.b / 255.0f};
    SetShaderValue(shader, locFog, f, SHADER_UNIFORM_VEC3);
    rlDisableBackfaceCulling();
    rlSetClipPlanes(0.1, 300.0);
    
    // Floor layering: there is NO global polygon offset any more (it shifted every surface and the old
    // 0.2 m raised "second ground" sank feet/shadows into the floor). Ground decals are drawn flush with
    // the ground at y = 0 and are pulled forward only while they are drawn, via DecalBegin/DecalEnd.
    stormShader = LoadShaderFromMemory(kStormVS, kStormFS);
    locStormRes = GetShaderLocation(stormShader, "uRes");
    locStormTime = GetShaderLocation(stormShader, "uTime");
    
    SetRandomSeed(7);
    texGrass = MakeTex(C(70, 95, 30), C(120, 140, 55), 3);
    texSand = MakeTex(C(176, 150, 112), C(210, 186, 145), 0);
    texDirt = MakeTex(C(120, 92, 66), C(160, 128, 92), 0);
    texWood = MakeTex(C(190, 130, 70), C(225, 165, 95), 1);
    texMetal = MakeTex(C(170, 170, 175), C(215, 215, 220), 2);
    texConcrete = MakeTex(C(130, 130, 128), C(170, 168, 165), 0);
    texNoise = MakeTex(C(190, 190, 190), C(255, 255, 255), 0);
}

void Renderer::Shutdown() {
    UnloadTexture(texGrass); UnloadTexture(texSand); UnloadTexture(texDirt); UnloadTexture(texWood);
    UnloadTexture(texMetal); UnloadTexture(texConcrete); UnloadTexture(texNoise);
    UnloadShader(shader); UnloadShader(stormShader);
}

void Renderer::SetResolution(float w, float h) {
    float r[2] = {w, h};
    SetShaderValue(shader, locRes, r, SHADER_UNIFORM_VEC2);
    SetShaderValue(stormShader, locStormRes, r, SHADER_UNIFORM_VEC2);
}

void Renderer::DecalBegin(float factor, float units) {
    rlDrawRenderBatchActive();
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(factor, units);
}
void Renderer::DecalEnd() {
    rlDrawRenderBatchActive();
    glDisable(GL_POLYGON_OFFSET_FILL);
}

Model Renderer::MakeStormModel(MeshBuilder& mb) {
    Mesh m = mb.Build();
    Model model = LoadModelFromMesh(m);
    model.materials[0].shader = stormShader;
    return model;
}

Model Renderer::MakeModel(MeshBuilder& mb, const Texture2D* tex) {
    Mesh m = mb.Build();
    Model model = LoadModelFromMesh(m);
    model.materials[0].shader = shader;
    if (tex) SetMaterialTexture(&model.materials[0], MATERIAL_MAP_DIFFUSE, *tex);
    return model;
}

void Renderer::FreeModel(Model& m) {
    if (m.meshCount == 0) return;
    for (int i = 0; i < m.meshCount; i++) UnloadMesh(m.meshes[i]);
    MemFree(m.meshes); MemFree(m.materials[0].maps); MemFree(m.materials); MemFree(m.meshMaterial);
    m = Model{};
}