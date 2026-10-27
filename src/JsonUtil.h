#pragma once
#include <string>
#include <vector>
#include <cstdlib>

// Extremely minimal JSON helpers (string extract only – enough for our backend replies).
inline std::string JsonGetString(const std::string& json, const std::string& key) {
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
    // number / bool / null
    size_t end = json.find_first_of(",}\n\r ", p);
    return json.substr(p, end == std::string::npos ? std::string::npos : end - p);
}

inline int JsonGetInt(const std::string& json, const std::string& key, int def = 0) {
    std::string s = JsonGetString(json, key);
    if (s.empty()) return def;
    return atoi(s.c_str());
}

inline bool JsonGetBool(const std::string& json, const std::string& key) {
    std::string s = JsonGetString(json, key);
    return s == "true" || s == "1";
}

// Extract object that follows "key":
inline std::string JsonGetObject(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t p = json.find(needle);
    if (p == std::string::npos) return "";
    p = json.find('{', p + needle.size());
    if (p == std::string::npos) return "";
    int depth = 0;
    for (size_t i = p; i < json.size(); i++) {
        if (json[i] == '{') depth++;
        else if (json[i] == '}') {
            depth--;
            if (depth == 0) return json.substr(p, i - p + 1);
        }
    }
    return "";
}

// Very simple array-of-objects splitter for leaderboards / shop items
inline std::vector<std::string> JsonGetArrayObjects(const std::string& json, const std::string& key) {
    std::vector<std::string> out;
    std::string needle = "\"" + key + "\"";
    size_t p = json.find(needle);
    if (p == std::string::npos) return out;
    p = json.find('[', p + needle.size());
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
