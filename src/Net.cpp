#include "Net.h"
#include <cstring>
#include <sstream>
#include <cstdlib>

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
static void EnsureWSA() {}
#define CLOSESOCK close
#define SOCK_ERR (-1)
#define SOCKET int
#define INVALID_SOCKET (-1)
#endif

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
    return HttpRequest("GET", "26.155.48.164", 3847, path, "", outStatus);
}

std::string HttpPost(const std::string& path, const std::string& jsonBody, int* outStatus) {
    return HttpRequest("POST", "26.155.48.164", 3847, path, jsonBody, outStatus);
}


// ---------- Matchmaker (game server TCP :3848) ----------
#ifdef _WIN32
static SOCKET g_mmSock = INVALID_SOCKET;
#else
static int g_mmSock = -1;
#endif
static bool g_mmConnected = false;

bool MatchmakerIsConnected() { return g_mmConnected; }

void MatchmakerDisconnect() {
    if (!g_mmConnected) return;
#ifdef _WIN32
    if (g_mmSock != INVALID_SOCKET) { closesocket(g_mmSock); g_mmSock = INVALID_SOCKET; }
#else
    if (g_mmSock >= 0) { close(g_mmSock); g_mmSock = -1; }
#endif
    g_mmConnected = false;
}

bool MatchmakerConnect(const std::string& playerName, const std::string& modeName, std::string* errorOut) {
    MatchmakerDisconnect();
    EnsureWSA();

#ifdef _WIN32
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        if (errorOut) *errorOut = "socket failed";
        return false;
    }
    DWORD timeout = 1500;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        if (errorOut) *errorOut = "socket failed";
        return false;
    }
    struct timeval tv;
    tv.tv_sec = 1; tv.tv_usec = 500000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(3848);
    if (inet_pton(AF_INET, "26.155.48.164", &addr.sin_addr) <= 0) {
        CLOSESOCK(s);
        if (errorOut) *errorOut = "inet_pton failed";
        return false;
    }
    if (connect(s, (sockaddr*)&addr, sizeof(addr)) == SOCK_ERR) {
        CLOSESOCK(s);
        if (errorOut) *errorOut = "No game server — start GameServer or switch to Local";
        return false;
    }

    std::string join = "{\"type\":\"join\",\"name\":\"" + playerName + "\",\"mode\":\"" + modeName + "\"}\n";
#ifdef _WIN32
    send(s, join.c_str(), (int)join.size(), 0);
    g_mmSock = s;
#else
    send(s, join.c_str(), join.size(), 0);
    g_mmSock = s;
#endif
    g_mmConnected = true;
    return true;
}
