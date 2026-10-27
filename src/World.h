#pragma once
#include "Common.h"
#include "Items.h"
#include "MeshBuilder.h"

enum MapId { MAP_DUSTY = 0, MAP_CONTAINER = 1, MAP_SALTY = 2, MAP_RETAIL = 3, MAP_COUNT = 4 };
enum Mat { MAT_WOOD = 0, MAT_BRICK = 1, MAT_METAL = 2, MAT_COUNT = 3 };
enum PieceType { P_WALL = 0, P_FLOOR, P_STAIR, P_ROOF, P_COUNT };
enum SolidKind { SK_STATIC = 0, SK_CRATE, SK_TREE, SK_CONTAINER };

extern const Color kMatColor[3];
const char* MapName(MapId m);

struct Solid { AABB box; SolidKind kind = SK_STATIC; bool alive = true; float hp = 100; Vector3 pos{}; Vector3 scale{1, 1, 1}; float yaw = 0; Color tint = WHITE; };
struct Piece { PieceType type; Mat mat; Vector3 pos; int dir; float hp; AABB box; bool alive = true; };
struct Chest { Vector3 pos; float yaw = 0; bool opened = false; float t = 0; };
struct Drop { LootRoll item; Vector3 pos; bool alive = true; float spin = 0; };
struct RayHit { bool hit = false; float dist = 1e9f; Vector3 point{}; int solid = -1; int piece = -1; };

// Building footprint traced from an overhead screenshot (pixels). roofType: 0 = flat slab, 1 = gable.
struct PoiB { float x, y, w, h, ht; Color wall, roof; int roofType; float door; int chests; };

class World {
public:
    MapId id = MAP_DUSTY;
    float half = 60.0f;
    std::vector<Solid> solids;
    std::vector<Piece> pieces;
    std::vector<Chest> chests;
    std::vector<Drop> drops;
    std::vector<Vector3> spawns;
    const Model* gunModel = nullptr;

    void Build(MapId id);
    void Unload();
    void Update(float dt);
    void Draw(Vector3 cam, float stormR, const Camera3D& view);     // opaque world (ground, decals, buildings, props)
    void DrawStorm(float stormR, const Camera3D& view);             // translucent storm: call AFTER actors / opaque VFX

    float GroundY(Vector3 p, float footY, float step) const;
    Vector3 MoveSlide(Vector3 pos, Vector3 delta, float radius, float height, float step) const;
    RayHit Raycast(Ray r, float maxD) const;

    void GetPlacement(PieceType t, Vector3 feet, float yaw, Vector3& pos, int& dir) const;
    AABB PieceBox(PieceType t, Vector3 pos, int dir) const;
    bool Place(PieceType t, Mat m, Vector3 feet, float yaw);
    void AddDrop(LootRoll r, Vector3 p);

private:
    bool loaded = false;
    // Ground is ONE grass plane at y = 0. Roads, lots, sand/concrete yards are flush decals (mDecal) and painted
    // lines (mDecalTop) drawn with a depth bias, so nothing ever sits above or z-fights with the floor.
    Model mGround{}, mDecal{}, mDecalTop{}, mStatic{}, mCrate{}, mTree{}, mContainer{}, mChestBody{}, mChestLid{}, mStorm{}, mStormHaze{}, mPiece[P_COUNT]{};

    bool Blocked(Vector3 p, float r, float h, float step) const;
    bool Free(float x, float z, float margin) const;
    bool FindFree(float x0, float x1, float z0, float z1, float margin, Vector3& out) const;
    void BuildShared();
    void BuildDusty();
    void BuildYard();
    void BuildSalty();
    void BuildRetail();
    void ScatterLoot(float x0, float x1, float z0, float z1, int floorLoot, int nChests);
    void ScatterTrees(int n, float roadMargin);
    void Block(MeshBuilder& mb, Vector3 c, Vector3 s, Color col, bool collide = true);
    void Building(MeshBuilder& mb, float cx, float cz, float w, float d, float h, Color wc, float door, int roof, Color rc);
    void ArchRoof(MeshBuilder& mb, float cx, float cz, float len, float hw, float baseY, float rise, Color a, Color b);
    void Scaffold(MeshBuilder& mb, float cx, float cz, float w, float d, float h, Color col);
    void Gable(MeshBuilder& mb, float cx, float cz, float w, float d, float baseY, float rise, Color roof, Color wall);
    void Windows(MeshBuilder& mb, float cx, float cz, float w, float d, float h, float door, Color glass);
    void Car(MeshBuilder& mb, float x, float z, bool alongZ, Color c);
    void Fence(MeshBuilder& mb, float cx, float cz, float w, float d, Color c);
    void PoiBuild(MeshBuilder& mb, const PoiB* b, int n, float cxp, float cyp, float mpp);
    void AddCrate(float x, float y, float z, float size);
    void AddTree(float x, float z, float s);
    void AddContainer(float x, float y, float z, float yaw, Color tint);
};
