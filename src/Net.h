#pragma once
#include <string>
#include <vector>
#include <functional>

// HTTP client for the Node backend (accounts / friends / party / shop).
// Host defaults to the same machine as the game server.
std::string HttpRequest(const std::string& method, const std::string& host, int port,
                        const std::string& path, const std::string& body = "",
                        int* outStatus = nullptr);

std::string HttpGet(const std::string& path, int* outStatus = nullptr);
std::string HttpPost(const std::string& path, const std::string& jsonBody, int* outStatus = nullptr);

// Override backend / game-server host (default: 26.155.48.164)
void NetSetHost(const std::string& host);
const std::string& NetGetHost();

// ---------- Game server TCP client (port 3848) ----------
// Protocol (line-delimited JSON):
//   Client -> Server: join | input | fire | damage | leave
//   Server -> Client: welcome | snapshot | event | kick

struct NetRemotePlayer {
    int id = 0;
    std::string name;
    float x = 0, y = 0, z = 0;
    float yaw = 0, pitch = 0;
    std::string weapon = "pickaxe";
    float health = 100, shield = 0;
    bool alive = true;
    int team = 0;
    bool shooting = false;
};

struct NetSnapshot {
    int matchId = 0;
    std::string mode;
    float stormR = 85;
    int stormPhase = 0;
    float stormT = 80;
    bool shrinking = false;
    std::vector<NetRemotePlayer> players;
};

struct NetEvent {
    std::string event;   // player_joined | player_left | fire | elim | hurt | match_reset | storm | mode | rename | give | kick
    int id = -1;
    int victim = -1;
    int killer = -1;
    int from = -1;
    float dmg = 0;
    float x = 0, y = 0, z = 0;
    float dx = 0, dy = 0, dz = 0;
    std::string name;
    std::string weapon;
    std::string reason;
    std::string mode;
    int phase = 0;
    float t = 0;
    float r = 0;
    bool shrinking = false;
};

// Connect to game server, send join, wait briefly for welcome.
// Returns true on success. localIdOut receives our server-assigned id when available.
bool MatchmakerConnect(const std::string& playerName, const std::string& modeName,
                       std::string* errorOut = nullptr, int* localIdOut = nullptr);
void MatchmakerDisconnect();
bool MatchmakerIsConnected();

// Call every frame while in an online match.
// Drains the socket, updates the latest snapshot, and queues events.
void NetPoll();

// Latest snapshot (empty players if none yet).
const NetSnapshot& NetGetSnapshot();
int NetLocalId();           // -1 until welcome
int NetMatchId();
bool NetWasKicked(std::string* reasonOut = nullptr);

// Drain pending events (clears the queue).
std::vector<NetEvent> NetDrainEvents();

// Send packets (no-ops if not connected).
void NetSendInput(float x, float y, float z, float yaw, float pitch,
                  const std::string& weapon, bool shooting);
void NetSendFire(float x, float y, float z, float dx, float dy, float dz, const std::string& weapon);
void NetSendDamage(int targetId, float dmg);
void NetSendLeave();
