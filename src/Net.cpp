#include "Net.h"
#include <cstring>
#include <sstream>
#include <cstdlib>
#include <vector>
#include <mutex>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
static bool g_wsaInit = false;
static void EnsureWSA() {
    if (!g_wsaInit) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        g_wsaInit = true;
    }
}
typedef int socklen_t;
#define CLOSESOCK closesocket
#define SOCK_ERR SOCKET_ERROR
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
static void EnsureWSA() {}
#define CLOSESOCK close
#define SOCK_ERR (-1)
#define SOCKET int
#define INVALID_SOCKET (-1)
#endif

// Same host as the backend (override with NetSetHost).
static std::string g_host = "26.155.48.164";
static const int kHttpPort = 3847;
static const int kGamePort = 3848;

void NetSetHost(const std::string& host) {
    if (!host.empty()) g_host = host;
}
const std::string& NetGetHost() { return g_host; }

// ---------- minimal JSON helpers (local copies) ----------
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
    size_t end = json.find_first_of(",}\n\r ", p);
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
    std::string s = JStr(json, key);
    return s == "true" || s == "1";
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
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        CLOSESOCK(sock);
        return "";
    }
    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCK_ERR) {
        CLOSESOCK(sock);
        return "";
    }

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
    if (send(sock, request.c_str(), (int)request.size(), 0) == SOCK_ERR) {
#else
    if (send(sock, request.c_str(), request.size(), 0) < 0) {
#endif
        CLOSESOCK(sock);
        return "";
    }

    std::string response;
    char buf[4096];
#ifdef _WIN32
    int n;
    while ((n = recv(sock, buf, (int)sizeof(buf) - 1, 0)) > 0) {
#else
    ssize_t n;
    while ((n = recv(sock, buf, sizeof(buf) - 1, 0)) > 0) {
#endif
        buf[n] = 0;
        response.append(buf, n);
    }
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
#ifdef _WIN32
static SOCKET g_mmSock = INVALID_SOCKET;
#else
static int g_mmSock = -1;
#endif
static bool g_mmConnected = false;
static std::string g_recvBuf;
static int g_localId = -1;
static int g_matchId = 0;
static NetSnapshot g_snap;
static std::vector<NetEvent> g_events;
static bool g_kicked = false;
static std::string g_kickReason;

static void SetNonBlocking(SOCKET s) {
#ifdef _WIN32
    u_long nb = 1;
    ioctlsocket(s, FIONBIO, &nb);
#else
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
}

static void RawSend(const std::string& line) {
    if (!g_mmConnected) return;
    std::string msg = line;
    if (msg.empty() || msg.back() != '\n') msg.push_back('\n');
#ifdef _WIN32
    if (send(g_mmSock, msg.c_str(), (int)msg.size(), 0) == SOCK_ERR) {
        g_mmConnected = false;
    }
#else
    if (send(g_mmSock, msg.c_str(), msg.size(), 0) < 0) {
        g_mmConnected = false;
    }
#endif
}

bool MatchmakerIsConnected() { return g_mmConnected; }
int NetLocalId() { return g_localId; }
int NetMatchId() { return g_matchId; }
const NetSnapshot& NetGetSnapshot() { return g_snap; }

bool NetWasKicked(std::string* reasonOut) {
    if (!g_kicked) return false;
    if (reasonOut) *reasonOut = g_kickReason;
    return true;
}

void MatchmakerDisconnect() {
    if (g_mmConnected) {
        RawSend("{\"type\":\"leave\"}");
    }
    if (g_mmConnected || g_mmSock != INVALID_SOCKET) {
#ifdef _WIN32
        if (g_mmSock != INVALID_SOCKET) { closesocket(g_mmSock); g_mmSock = INVALID_SOCKET; }
#else
        if (g_mmSock >= 0) { close(g_mmSock); g_mmSock = -1; }
#endif
    }
    g_mmConnected = false;
    g_recvBuf.clear();
    g_localId = -1;
    g_matchId = 0;
    g_snap = NetSnapshot();
    g_events.clear();
    g_kicked = false;
    g_kickReason.clear();
}

static NetRemotePlayer ParsePlayerObj(const std::string& obj) {
    NetRemotePlayer p;
    p.id = JInt(obj, "id");
    p.name = JStr(obj, "name");
    p.x = JFloat(obj, "x");
    p.y = JFloat(obj, "y");
    p.z = JFloat(obj, "z");
    p.yaw = JFloat(obj, "yaw");
    p.pitch = JFloat(obj, "pitch");
    p.weapon = JStr(obj, "weapon");
    if (p.weapon.empty()) p.weapon = "pickaxe";
    p.health = JFloat(obj, "health", 100);
    p.shield = JFloat(obj, "shield");
    p.alive = JBool(obj, "alive") || JStr(obj, "alive") != "false";
    // JBool returns false for missing; treat missing as alive
    if (obj.find("\"alive\"") == std::string::npos) p.alive = true;
    p.team = JInt(obj, "team");
    p.shooting = JBool(obj, "shooting");
    return p;
}

static std::vector<std::string> SplitPlayerArray(const std::string& json) {
    std::vector<std::string> out;
    size_t p = json.find("\"players\"");
    if (p == std::string::npos) return out;
    p = json.find('[', p);
    if (p == std::string::npos) return out;
    size_t i = p + 1;
    while (i < json.size()) {
        while (i < json.size() && (json[i] == ' ' || json[i] == '\n' || json[i] == '\r' || json[i] == ',' || json[i] == '\t')) i++;
        if (i >= json.size() || json[i] == ']') break;
        if (json[i] != '{') { i++; continue; }
        int depth = 0;
        size_t start = i;
        for (; i < json.size(); i++) {
            if (json[i] == '{') depth++;
            else if (json[i] == '}') {
                depth--;
                if (depth == 0) {
                    out.push_back(json.substr(start, i - start + 1));
                    i++;
                    break;
                }
            }
        }
    }
    return out;
}

static void HandleLine(const std::string& line) {
    if (line.empty()) return;
    std::string type = JStr(line, "type");

    if (type == "welcome") {
        g_localId = JInt(line, "id", -1);
        g_matchId = JInt(line, "matchId");
        g_snap.mode = JStr(line, "mode");
        return;
    }

    if (type == "snapshot") {
        g_snap.matchId = JInt(line, "matchId", g_matchId);
        g_matchId = g_snap.matchId;
        g_snap.mode = JStr(line, "mode");
        g_snap.stormR = JFloat(line, "stormR", g_snap.stormR);
        g_snap.stormPhase = JInt(line, "stormPhase", g_snap.stormPhase);
        g_snap.stormT = JFloat(line, "stormT", g_snap.stormT);
        g_snap.shrinking = JBool(line, "shrinking");
        g_snap.players.clear();
        for (auto& obj : SplitPlayerArray(line))
            g_snap.players.push_back(ParsePlayerObj(obj));
        return;
    }

    if (type == "kick") {
        g_kicked = true;
        g_kickReason = JStr(line, "reason");
        if (g_kickReason.empty()) g_kickReason = "Kicked by server";
        g_mmConnected = false;
        NetEvent e;
        e.event = "kick";
        e.reason = g_kickReason;
        g_events.push_back(e);
        return;
    }

    if (type == "event") {
        NetEvent e;
        e.event = JStr(line, "event");
        e.id = JInt(line, "id", -1);
        e.victim = JInt(line, "victim", -1);
        e.killer = JInt(line, "killer", -1);
        e.from = JInt(line, "from", -1);
        e.dmg = JFloat(line, "dmg");
        e.x = JFloat(line, "x");
        e.y = JFloat(line, "y");
        e.z = JFloat(line, "z");
        e.dx = JFloat(line, "dx");
        e.dy = JFloat(line, "dy");
        e.dz = JFloat(line, "dz");
        e.name = JStr(line, "name");
        e.weapon = JStr(line, "weapon");
        e.reason = JStr(line, "reason");
        e.mode = JStr(line, "mode");
        e.phase = JInt(line, "phase");
        e.t = JFloat(line, "t");
        e.r = JFloat(line, "r");
        e.shrinking = JBool(line, "shrinking");
        // match_reset carries matchId
        if (e.event == "match_reset") {
            g_matchId = JInt(line, "matchId", g_matchId);
        }
        g_events.push_back(e);
        return;
    }
}

void NetPoll() {
    if (!g_mmConnected || g_mmSock == INVALID_SOCKET) return;

    char buf[4096];
    for (;;) {
#ifdef _WIN32
        int n = recv(g_mmSock, buf, (int)sizeof(buf) - 1, 0);
#else
        int n = (int)recv(g_mmSock, buf, sizeof(buf) - 1, 0);
#endif
        if (n > 0) {
            buf[n] = 0;
            g_recvBuf.append(buf, n);
            size_t pos;
            while ((pos = g_recvBuf.find('\n')) != std::string::npos) {
                std::string line = g_recvBuf.substr(0, pos);
                g_recvBuf.erase(0, pos + 1);
                HandleLine(line);
            }
        } else if (n == 0) {
            g_mmConnected = false;
            break;
        } else {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK) g_mmConnected = false;
#else
            if (errno != EWOULDBLOCK && errno != EAGAIN) g_mmConnected = false;
#endif
            break;
        }
    }
}

std::vector<NetEvent> NetDrainEvents() {
    std::vector<NetEvent> out;
    out.swap(g_events);
    return out;
}

bool MatchmakerConnect(const std::string& playerName, const std::string& modeName,
                       std::string* errorOut, int* localIdOut) {
    MatchmakerDisconnect();
    EnsureWSA();
    g_kicked = false;
    g_kickReason.clear();

#ifdef _WIN32
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        if (errorOut) *errorOut = "socket failed";
        return false;
    }
    // Short blocking timeout for connect only
    DWORD timeout = 2500;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        if (errorOut) *errorOut = "socket failed";
        return false;
    }
    struct timeval tv;
    tv.tv_sec = 2; tv.tv_usec = 500000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)kGamePort);
    if (inet_pton(AF_INET, g_host.c_str(), &addr.sin_addr) <= 0) {
        CLOSESOCK(s);
        if (errorOut) *errorOut = "inet_pton failed (bad host)";
        return false;
    }
    if (connect(s, (sockaddr*)&addr, sizeof(addr)) == SOCK_ERR) {
        CLOSESOCK(s);
        if (errorOut) *errorOut = "No game server — start GameServer or switch to Local";
        return false;
    }

    // Send join immediately (server also accepts bare connections and waits for join)
    std::string join = "{\"type\":\"join\",\"name\":\"" + playerName + "\",\"mode\":\"" + modeName + "\"}\n";
#ifdef _WIN32
    send(s, join.c_str(), (int)join.size(), 0);
#else
    send(s, join.c_str(), join.size(), 0);
#endif

    g_mmSock = s;
    g_mmConnected = true;
    g_recvBuf.clear();

    // Briefly wait for welcome / first snapshot (blocking recv with short timeout)
    char buf[2048];
    for (int attempt = 0; attempt < 8 && g_localId < 0; attempt++) {
#ifdef _WIN32
        int n = recv(s, buf, (int)sizeof(buf) - 1, 0);
#else
        int n = (int)recv(s, buf, sizeof(buf) - 1, 0);
#endif
        if (n > 0) {
            buf[n] = 0;
            g_recvBuf.append(buf, n);
            size_t pos;
            while ((pos = g_recvBuf.find('\n')) != std::string::npos) {
                std::string line = g_recvBuf.substr(0, pos);
                g_recvBuf.erase(0, pos + 1);
                HandleLine(line);
            }
        } else {
            break;
        }
    }

    // Switch to non-blocking for the rest of the match
    SetNonBlocking(s);

    if (localIdOut) *localIdOut = g_localId;
    // Even if welcome was missed, connection is valid — server assigned an id and will snapshot
    return true;
}

void NetSendInput(float x, float y, float z, float yaw, float pitch,
                  const std::string& weapon, bool shooting) {
    if (!g_mmConnected) return;
    char buf[512];
    snprintf(buf, sizeof buf,
        "{\"type\":\"input\",\"x\":%.3f,\"y\":%.3f,\"z\":%.3f,\"yaw\":%.4f,\"pitch\":%.4f,"
        "\"weapon\":\"%s\",\"shooting\":%s}",
        x, y, z, yaw, pitch, weapon.c_str(), shooting ? "true" : "false");
    RawSend(buf);
}

void NetSendFire(float x, float y, float z, float dx, float dy, float dz, const std::string& weapon) {
    if (!g_mmConnected) return;
    char buf[512];
    snprintf(buf, sizeof buf,
        "{\"type\":\"fire\",\"x\":%.3f,\"y\":%.3f,\"z\":%.3f,\"dx\":%.3f,\"dy\":%.3f,\"dz\":%.3f,"
        "\"weapon\":\"%s\"}",
        x, y, z, dx, dy, dz, weapon.c_str());
    RawSend(buf);
}

void NetSendDamage(int targetId, float dmg) {
    if (!g_mmConnected || targetId < 0) return;
    char buf[128];
    snprintf(buf, sizeof buf, "{\"type\":\"damage\",\"targetId\":%d,\"dmg\":%.2f}", targetId, dmg);
    RawSend(buf);
}

void NetSendLeave() {
    if (!g_mmConnected) return;
    RawSend("{\"type\":\"leave\"}");
}
