#include "Net.h"
#include <cstring>
#include <sstream>
#include <cstdlib>
#include <vector>
#include <mutex>
#include <chrono>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socklen_t = int;
#define CLOSESOCK closesocket
#define SOCK_ERR SOCKET_ERROR
static bool g_wsa;
static void EnsureWSA() {
    if (!g_wsa) { WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); g_wsa = true; }
}
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define CLOSESOCK close
#define SOCK_ERR (-1)
static void EnsureWSA() {}
#endif

static std::string g_host = "26.155.48.164";
static const int kHttpPort = 3847;
static const int kGamePort = 3848;

void NetSetHost(const std::string& host) {
    if (!host.empty()) g_host = host;
}
const std::string& NetGetHost() { return g_host; }

static std::string JStr(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t p = json.find(needle);
    if (p == std::string::npos) return "";
    p = json.find(':', p + needle.size());
    if (p == std::string::npos) return "";
    p = json.find_first_not_of(" \t\r\n", p + 1);
    if (p == std::string::npos) return "";
    if (json[p] == '"') {
        size_t end = json.find('"', p + 1);
        if (end == std::string::npos) return "";
        return json.substr(p + 1, end - p - 1);
    }
    size_t end = json.find_first_of(",}]\n\r ", p);
    return json.substr(p, end == std::string::npos ? std::string::npos : end - p);
}
static int JInt(const std::string& json, const std::string& key, int def = 0) {
    std::string s = JStr(json, key);
    if (s.empty()) return def;
    return atoi(s.c_str());
}
static float JFloat(const std::string& json, const std::string& key, float def = 0) {
    std::string s = JStr(json, key);
    if (s.empty()) return def;
    return (float)atof(s.c_str());
}
static bool JBool(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t p = json.find(needle);
    if (p == std::string::npos) return false;
    p = json.find(':', p + needle.size());
    if (p == std::string::npos) return false;
    p = json.find_first_not_of(" \t\r\n", p + 1);
    if (p == std::string::npos) return false;
    return json.compare(p, 4, "true") == 0 || json[p] == '1';
}

// ---------- HTTP ----------
static std::string HttpRequestRaw(const std::string& method, const std::string& host, int port,
                                  const std::string& path, const std::string& body, int* outStatus) {
    EnsureWSA();
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return "";
#ifdef _WIN32
    DWORD timeout = 2000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv; tv.tv_sec = 2; tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) { CLOSESOCK(sock); return ""; }
    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCK_ERR) { CLOSESOCK(sock); return ""; }

    std::ostringstream req;
    req << method << " " << path << " HTTP/1.1\r\n";
    req << "Host: " << host << "\r\n";
    req << "Connection: close\r\n";
    req << "User-Agent: FortnitePSX/1.0\r\n";
    if (!body.empty()) {
        req << "Content-Type: application/json\r\n";
        req << "Content-Length: " << body.size() << "\r\n";
    }
    req << "\r\n";
    if (!body.empty()) req << body;
    std::string request = req.str();
#ifdef _WIN32
    if (send(sock, request.c_str(), (int)request.size(), 0) == SOCK_ERR) { CLOSESOCK(sock); return ""; }
#else
    if (send(sock, request.c_str(), request.size(), 0) < 0) { CLOSESOCK(sock); return ""; }
#endif
    std::string response;
    char buf[4096];
#ifdef _WIN32
    int n;
    while ((n = recv(sock, buf, (int)sizeof(buf) - 1, 0)) > 0) { buf[n] = 0; response.append(buf, n); }
#else
    ssize_t n;
    while ((n = recv(sock, buf, sizeof(buf) - 1, 0)) > 0) { buf[n] = 0; response.append(buf, (size_t)n); }
#endif
    CLOSESOCK(sock);
    if (outStatus) {
        *outStatus = 0;
        if (response.size() > 12 && response.compare(0, 5, "HTTP/") == 0) {
            size_t sp = response.find(' ');
            if (sp != std::string::npos) *outStatus = atoi(response.c_str() + sp + 1);
        }
    }
    size_t bodyStart = response.find("\r\n\r\n");
    if (bodyStart == std::string::npos) return "";
    return response.substr(bodyStart + 4);
}

std::string HttpRequest(const std::string& method, const std::string& host, int port,
                        const std::string& path, const std::string& body, int* outStatus) {
    return HttpRequestRaw(method, host, port, path, body, outStatus);
}
std::string HttpGet(const std::string& path, int* outStatus) {
    return HttpRequest("GET", g_host, kHttpPort, path, "", outStatus);
}
std::string HttpPost(const std::string& path, const std::string& jsonBody, int* outStatus) {
    return HttpRequest("POST", g_host, kHttpPort, path, jsonBody, outStatus);
}

// ---------- Game server TCP ----------
static SOCKET g_sock = INVALID_SOCKET;
static std::string g_buf;
static int g_localId = -1;
static NetSnapshot g_snap;
static std::vector<NetEvent> g_events;
static bool g_kicked = false;
static std::string g_kickReason;
static int g_look = 0;
static std::string g_skin;

static void SetNonBlock(SOCKET s) {
#ifdef _WIN32
    u_long nb = 1; ioctlsocket(s, FIONBIO, &nb);
#else
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
}

static bool SendLine(const std::string& line) {
    if (g_sock == INVALID_SOCKET) return false;
    std::string msg = line;
    if (msg.empty() || msg.back() != '\n') msg.push_back('\n');
#ifdef _WIN32
    return send(g_sock, msg.c_str(), (int)msg.size(), 0) != SOCKET_ERROR;
#else
    return send(g_sock, msg.c_str(), msg.size(), 0) >= 0;
#endif
}

static std::vector<std::string> SplitArrayObjects(const std::string& json, const std::string& key) {
    std::vector<std::string> out;
    std::string needle = "\"" + key + "\"";
    size_t p = json.find(needle);
    if (p == std::string::npos) return out;
    p = json.find('[', p);
    if (p == std::string::npos) return out;
    int depth = 0;
    size_t objStart = std::string::npos;
    for (size_t i = p; i < json.size(); i++) {
        char c = json[i];
        if (c == '[') { depth++; continue; }
        if (c == ']') {
            depth--;
            if (depth == 0) break;
            continue;
        }
        if (c == '{') {
            if (depth == 1) objStart = i;
            depth++;
            continue;
        }
        if (c == '}') {
            depth--;
            if (depth == 1 && objStart != std::string::npos) {
                out.push_back(json.substr(objStart, i - objStart + 1));
                objStart = std::string::npos;
            }
            continue;
        }
    }
    return out;
}

static NetRemotePlayer ParsePlayer(const std::string& obj) {
    NetRemotePlayer p;
    p.id = JInt(obj, "id", -1);
    p.name = JStr(obj, "name");
    p.x = JFloat(obj, "x"); p.y = JFloat(obj, "y"); p.z = JFloat(obj, "z");
    p.yaw = JFloat(obj, "yaw"); p.pitch = JFloat(obj, "pitch");
    p.weapon = JStr(obj, "weapon");
    if (p.weapon.empty()) p.weapon = "pickaxe";
    p.look = JInt(obj, "look", 0);
    p.skin = JStr(obj, "skin");
    p.health = JFloat(obj, "health", 100);
    p.shield = JFloat(obj, "shield", 0);
    p.alive = JBool(obj, "alive") || obj.find("\"alive\":false") == std::string::npos;
    if (obj.find("\"alive\":false") != std::string::npos) p.alive = false;
    p.team = JInt(obj, "team", 0);
    p.shooting = JBool(obj, "shooting");
    p.isBot = JBool(obj, "isBot");
    p.onBus = JBool(obj, "onBus");
    p.skydiving = JBool(obj, "skydiving");
    p.gliding = JBool(obj, "gliding");
    return p;
}

static NetBuildPiece ParseBuild(const std::string& obj) {
    NetBuildPiece b;
    b.id = JInt(obj, "id", 0);
    b.ownerId = JInt(obj, "ownerId", -1);
    b.kind = JStr(obj, "kind");
    b.mat = JStr(obj, "mat");
    b.x = JFloat(obj, "x"); b.y = JFloat(obj, "y"); b.z = JFloat(obj, "z");
    b.yaw = JFloat(obj, "yaw");
    b.hp = JFloat(obj, "hp", 100);
    return b;
}

static NetBullet ParseBullet(const std::string& obj) {
    NetBullet b;
    b.id = JInt(obj, "id", 0);
    b.ownerId = JInt(obj, "ownerId", -1);
    b.x = JFloat(obj, "x"); b.y = JFloat(obj, "y"); b.z = JFloat(obj, "z");
    b.dx = JFloat(obj, "dx"); b.dy = JFloat(obj, "dy"); b.dz = JFloat(obj, "dz");
    b.speed = JFloat(obj, "speed", 120);
    b.life = JFloat(obj, "life", 1.5f);
    b.weapon = JStr(obj, "weapon");
    return b;
}

static NetBattleBus ParseBus(const std::string& json) {
    NetBattleBus b;
    size_t p = json.find("\"bus\"");
    if (p == std::string::npos) return b;
    p = json.find('{', p);
    if (p == std::string::npos) return b;
    int depth = 0;
    size_t end = p;
    for (size_t i = p; i < json.size(); i++) {
        if (json[i] == '{') depth++;
        else if (json[i] == '}') { depth--; if (depth == 0) { end = i + 1; break; } }
    }
    std::string obj = json.substr(p, end - p);
    b.active = JBool(obj, "active");
    b.x = JFloat(obj, "x"); b.y = JFloat(obj, "y"); b.z = JFloat(obj, "z");
    b.dx = JFloat(obj, "dx"); b.dy = JFloat(obj, "dy"); b.dz = JFloat(obj, "dz");
    b.speed = JFloat(obj, "speed", 35);
    b.progress = JFloat(obj, "progress");
    b.dropOpen = JFloat(obj, "dropOpen", 0.12f);
    b.dropClose = JFloat(obj, "dropClose", 0.88f);
    return b;
}

static void HandleLine(const std::string& line) {
    std::string type = JStr(line, "type");
    if (type == "welcome") {
        g_localId = JInt(line, "id", g_localId);
        g_snap.matchId = JInt(line, "matchId", g_snap.matchId);
        g_snap.mode = JStr(line, "mode");
        return;
    }
    if (type == "snapshot") {
        NetSnapshot snap;
        snap.matchId = JInt(line, "matchId");
        snap.mode = JStr(line, "mode");
        snap.stormR = JFloat(line, "stormR", 85);
        snap.stormPhase = JInt(line, "stormPhase");
        snap.stormT = JFloat(line, "stormT", 80);
        snap.shrinking = JBool(line, "shrinking");
        snap.bus = ParseBus(line);
        for (auto& o : SplitArrayObjects(line, "players")) {
            NetRemotePlayer rp = ParsePlayer(o);
            if (rp.id >= 0) snap.players.push_back(rp);
        }
        for (auto& o : SplitArrayObjects(line, "builds"))
            snap.builds.push_back(ParseBuild(o));
        for (auto& o : SplitArrayObjects(line, "bullets"))
            snap.bullets.push_back(ParseBullet(o));
        g_snap = std::move(snap);
        return;
    }
    if (type == "kick") {
        g_kicked = true;
        g_kickReason = JStr(line, "reason");
        if (g_kickReason.empty()) g_kickReason = "Kicked";
        return;
    }
    if (type == "event") {
        NetEvent e;
        e.event = JStr(line, "event");
        e.id = JInt(line, "id", -1);
        e.victim = JInt(line, "victim", -1);
        e.killer = JInt(line, "killer", -1);
        e.x = JFloat(line, "x"); e.y = JFloat(line, "y"); e.z = JFloat(line, "z");
        e.dx = JFloat(line, "dx"); e.dy = JFloat(line, "dy"); e.dz = JFloat(line, "dz");
        e.weapon = JStr(line, "weapon");
        e.name = JStr(line, "name");
        e.reason = JStr(line, "reason");
        e.mode = JStr(line, "mode");
        e.phase = JInt(line, "phase");
        e.t = JFloat(line, "t"); e.r = JFloat(line, "r");
        e.shrinking = JBool(line, "shrinking");
        e.buildId = JInt(line, "buildId", e.id);
        e.ownerId = JInt(line, "ownerId", -1);
        e.kind = JStr(line, "kind");
        e.mat = JStr(line, "mat");
        e.yaw = JFloat(line, "yaw");
        e.hp = JFloat(line, "hp", 100);
        e.speed = JFloat(line, "speed");
        e.targetId = JInt(line, "targetId", -1);
        g_events.push_back(e);
        return;
    }
}

bool MatchmakerConnect(const std::string& playerName, const std::string& modeName,
                       std::string* errOut, int* outLocalId) {
    EnsureWSA();
    MatchmakerDisconnect();
    g_kicked = false; g_kickReason.clear(); g_events.clear();
    g_localId = -1; g_buf.clear(); g_snap = NetSnapshot();

    const char* hostEnv = getenv("GAME_HOST");
    std::string host = (hostEnv && hostEnv[0]) ? hostEnv : g_host;
    int port = kGamePort;
    if (const char* ep = getenv("GAME_PORT")) port = atoi(ep);

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        if (errOut) *errOut = "socket() failed";
        return false;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        CLOSESOCK(s);
        if (errOut) *errOut = "bad host " + host;
        return false;
    }
    SetNonBlock(s);
#ifdef _WIN32
    int cr = connect(s, (sockaddr*)&addr, sizeof(addr));
    if (cr == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK) {
            CLOSESOCK(s);
            if (errOut) *errOut = "connect failed";
            return false;
        }
        fd_set w; FD_ZERO(&w); FD_SET(s, &w);
        timeval tv{3, 0};
        if (select(0, nullptr, &w, nullptr, &tv) <= 0) {
            CLOSESOCK(s);
            if (errOut) *errOut = "connect timeout — is GameServer running?";
            return false;
        }
    }
#else
    int cr = connect(s, (sockaddr*)&addr, sizeof(addr));
    if (cr < 0 && errno != EINPROGRESS) {
        CLOSESOCK(s);
        if (errOut) *errOut = "connect failed";
        return false;
    }
    if (cr < 0) {
        fd_set w; FD_ZERO(&w); FD_SET(s, &w);
        timeval tv{3, 0};
        if (select(s + 1, nullptr, &w, nullptr, &tv) <= 0) {
            CLOSESOCK(s);
            if (errOut) *errOut = "connect timeout — is GameServer running?";
            return false;
        }
    }
#endif
    g_sock = s;
    char join[512];
    snprintf(join, sizeof join,
        "{\"type\":\"join\",\"name\":\"%s\",\"mode\":\"%s\",\"look\":%d,\"skin\":\"%s\"}\n",
        playerName.c_str(), modeName.c_str(), g_look, g_skin.c_str());
    SendLine(join);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
    while (std::chrono::steady_clock::now() < deadline && g_localId < 0) {
        NetPoll();
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
    if (outLocalId) *outLocalId = g_localId;
    return true;
}

void MatchmakerDisconnect() {
    if (g_sock != INVALID_SOCKET) {
        CLOSESOCK(g_sock);
        g_sock = INVALID_SOCKET;
    }
    g_buf.clear();
    g_localId = -1;
}

bool MatchmakerIsConnected() { return g_sock != INVALID_SOCKET; }
int  NetLocalId() { return g_localId; }

void NetPoll() {
    if (g_sock == INVALID_SOCKET) return;
    char buf[8192];
#ifdef _WIN32
    int n = recv(g_sock, buf, (int)sizeof(buf) - 1, 0);
#else
    int n = (int)recv(g_sock, buf, sizeof(buf) - 1, 0);
#endif
    if (n > 0) {
        buf[n] = 0;
        g_buf.append(buf, n);
        size_t pos;
        while ((pos = g_buf.find('\n')) != std::string::npos) {
            std::string line = g_buf.substr(0, pos);
            g_buf.erase(0, pos + 1);
            if (!line.empty()) HandleLine(line);
        }
    } else if (n == 0) {
        MatchmakerDisconnect();
    } else {
#ifdef _WIN32
        if (WSAGetLastError() != WSAEWOULDBLOCK) MatchmakerDisconnect();
#else
        if (errno != EWOULDBLOCK && errno != EAGAIN) MatchmakerDisconnect();
#endif
    }
}

const NetSnapshot& NetGetSnapshot() { return g_snap; }

std::vector<NetEvent> NetDrainEvents() {
    std::vector<NetEvent> out;
    out.swap(g_events);
    return out;
}

bool NetWasKicked(std::string* reasonOut) {
    if (!g_kicked) return false;
    if (reasonOut) *reasonOut = g_kickReason;
    g_kicked = false;
    return true;
}

void NetSetLocalLook(int lookIndex, const std::string& skinId) {
    g_look = lookIndex;
    g_skin = skinId;
}

void NetSendInput(float x, float y, float z, float yaw, float pitch,
                  const std::string& weapon, bool shooting,
                  int look, const std::string& skin,
                  bool skydiving, bool gliding) {
    char b[640];
    snprintf(b, sizeof b,
        "{\"type\":\"input\",\"x\":%.3f,\"y\":%.3f,\"z\":%.3f,\"yaw\":%.3f,\"pitch\":%.3f,"
        "\"weapon\":\"%s\",\"shooting\":%s,\"look\":%d,\"skin\":\"%s\","
        "\"skydiving\":%s,\"gliding\":%s}",
        x, y, z, yaw, pitch, weapon.c_str(), shooting ? "true" : "false",
        look, skin.c_str(), skydiving ? "true" : "false", gliding ? "true" : "false");
    SendLine(b);
}

void NetSendFire(float x, float y, float z, float dx, float dy, float dz, const std::string& weapon) {
    char b[512];
    snprintf(b, sizeof b,
        "{\"type\":\"fire\",\"x\":%.3f,\"y\":%.3f,\"z\":%.3f,\"dx\":%.3f,\"dy\":%.3f,\"dz\":%.3f,"
        "\"weapon\":\"%s\"}",
        x, y, z, dx, dy, dz, weapon.c_str());
    SendLine(b);
}

void NetSendDamage(int targetId, float dmg) {
    char b[128];
    snprintf(b, sizeof b, "{\"type\":\"damage\",\"targetId\":%d,\"dmg\":%.2f}", targetId, dmg);
    SendLine(b);
}

void NetSendBuild(const std::string& kind, const std::string& mat,
                  float x, float y, float z, float yaw, float hp) {
    char b[384];
    snprintf(b, sizeof b,
        "{\"type\":\"build\",\"kind\":\"%s\",\"mat\":\"%s\",\"x\":%.3f,\"y\":%.3f,\"z\":%.3f,"
        "\"yaw\":%.3f,\"hp\":%.1f}",
        kind.c_str(), mat.c_str(), x, y, z, yaw, hp);
    SendLine(b);
}

void NetSendDestroyBuild(int buildId) {
    char b[96];
    snprintf(b, sizeof b, "{\"type\":\"destroy_build\",\"buildId\":%d}", buildId);
    SendLine(b);
}

void NetSendBusJump() {
    SendLine("{\"type\":\"bus_jump\"}");
}

void NetSendLeave() {
    SendLine("{\"type\":\"leave\"}");
}
