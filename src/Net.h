#pragma once
#include <string>
#include <vector>

// HTTP client for the Node backend (accounts / friends / party / shop).
// Host defaults to the same machine as the game server (port 3847).
std::string HttpRequest(const std::string& method, const std::string& host, int port,
                        const std::string& path, const std::string& body = "",
                        int* outStatus = nullptr);
std::string HttpGet(const std::string& path, int* outStatus = nullptr);
std::string HttpPost(const std::string& path, const std::string& jsonBody, int* outStatus = nullptr);
void NetSetHost(const std::string& host);
const std::string& NetGetHost();


// Client -> Server: join | input | fire | damage | build | destroy_build | bus_jump | leave
// Server -> Client: welcome | snapshot | event | kick

struct NetRemotePlayer {
    int id = -1;
    std::string name;
    float x = 0, y = 0, z = 0;
    float yaw = 0, pitch = 0;
    std::string weapon = "pickaxe";
    int look = 0;
    std::string skin;
    float health = 100, shield = 0;
    bool alive = true;
    int team = 0;
    bool shooting = false;
    bool isBot = false;
    bool onBus = false;
    bool skydiving = false;
    bool gliding = false;
};

struct NetBuildPiece {
    int id = 0;
    int ownerId = -1;
    std::string kind;
    std::string mat;
    float x = 0, y = 0, z = 0;
    float yaw = 0;
    float hp = 100;
};

struct NetBullet {
    int id = 0;
    int ownerId = -1;
    float x = 0, y = 0, z = 0;
    float dx = 0, dy = 0, dz = 0;
    float speed = 120;
    float life = 1.5f;
    std::string weapon;
};

struct NetBattleBus {
    bool active = false;
    float x = 0, y = 80, z = 0;
    float dx = 0, dy = 0, dz = 1;
    float speed = 35;
    float progress = 0;
    float dropOpen = 0.12f;
    float dropClose = 0.88f;
};

struct NetSnapshot {
    int matchId = 0;
    std::string mode;
    float stormR = 85, stormT = 80;
    int stormPhase = 0;
    bool shrinking = false;
    NetBattleBus bus;
    std::vector<NetRemotePlayer> players;
    std::vector<NetBuildPiece> builds;
    std::vector<NetBullet> bullets;
};

struct NetEvent {
    std::string event;
    int id = -1;
    int victim = -1, killer = -1;
    float x = 0, y = 0, z = 0;
    float dx = 0, dy = 0, dz = 0;
    std::string weapon;
    std::string name;
    std::string reason;
    std::string mode;
    int phase = 0;
    float t = 0, r = 0;
    bool shrinking = false;
    int buildId = -1;
    int ownerId = -1;
    std::string kind;
    std::string mat;
    float yaw = 0;
    float hp = 100;
    float speed = 0;
    int targetId = -1;
};

bool MatchmakerConnect(const std::string& playerName, const std::string& modeName,
                       std::string* errOut = nullptr, int* outLocalId = nullptr);
void MatchmakerDisconnect();
bool MatchmakerIsConnected();
int  NetLocalId();

void NetPoll();
const NetSnapshot& NetGetSnapshot();
std::vector<NetEvent> NetDrainEvents();
bool NetWasKicked(std::string* reasonOut = nullptr);

void NetSendInput(float x, float y, float z, float yaw, float pitch,
                  const std::string& weapon, bool shooting,
                  int look = 0, const std::string& skin = "",
                  bool skydiving = false, bool gliding = false);
void NetSendFire(float x, float y, float z, float dx, float dy, float dz, const std::string& weapon);
void NetSendDamage(int targetId, float dmg);
void NetSendBuild(const std::string& kind, const std::string& mat,
                  float x, float y, float z, float yaw, float hp = 100);
void NetSendDestroyBuild(int buildId);
void NetSendBusJump();
void NetSendLeave();
void NetSetLocalLook(int lookIndex, const std::string& skinId = "");
