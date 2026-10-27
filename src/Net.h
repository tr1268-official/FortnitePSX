#pragma once
#include <string>

// Minimal HTTP client (GET/POST) for talking to the local Node backend.
// Returns empty string on failure. Body is returned; status code via outStatus if non-null.
std::string HttpRequest(const std::string& method, const std::string& host, int port,
                        const std::string& path, const std::string& body = "",
                        int* outStatus = nullptr);

// Convenience wrappers (host defaults to 127.0.0.1, port 3847)
std::string HttpGet(const std::string& path, int* outStatus = nullptr);
std::string HttpPost(const std::string& path, const std::string& jsonBody, int* outStatus = nullptr);

// Matchmaker / game server TCP (port 3848). Opaque handle kept inside Net.cpp.
// Returns true if connected (or if not needed). errorOut optional message.
bool MatchmakerConnect(const std::string& playerName, const std::string& modeName, std::string* errorOut = nullptr);
void MatchmakerDisconnect();
bool MatchmakerIsConnected();
