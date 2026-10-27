#include "IconCache.h"

IconCache gIcons;

static std::string Sanitise(const std::string& s) {
    std::string out; out.reserve(s.size());
    for (char c : s) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') continue;
        out.push_back(c);
    }
    return out;
}

static std::vector<std::string> BaseRoots() {
    std::vector<std::string> roots;
    roots.push_back("");
    const char* appDir = GetApplicationDirectory();
    if (appDir && appDir[0]) {
        std::string d = appDir;
        if (!d.empty() && d.back() != '/' && d.back() != '\\') d += '/';
        roots.push_back(d);
        roots.push_back(d + "../");
        roots.push_back(d + "../../");
    }
    return roots;
}

void IconCache::Init() { cache.clear(); }

void IconCache::Shutdown() {
    for (auto& kv : cache) { if (kv.second) { UnloadTexture(*kv.second); delete kv.second; } }
    cache.clear();
}

Texture2D* IconCache::Load(const std::string& relPath) {
    auto it = cache.find(relPath);
    if (it != cache.end()) return it->second;

    Texture2D* tex = nullptr;
    for (const auto& root : BaseRoots()) {
        std::string full = root + relPath;
        if (!FileExists(full.c_str())) continue;
        Texture2D t = LoadTexture(full.c_str());
        if (t.id == 0) continue;
        SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
        tex = new Texture2D(t);
        break;
    }
    cache[relPath] = tex;
    return tex;
}

const Texture2D* IconCache::Item(const std::string& itemName) {
    return Load("assets/items/icons/" + Sanitise(itemName) + ".png");
}

const Texture2D* IconCache::UI(const std::string& uiName) {
    return Load("assets/ui/" + Sanitise(uiName) + ".png");
}

const Texture2D* IconCache::UIItem(const std::string& uiItemName) {
    return Load("assets/ui/icons/" + Sanitise(uiItemName) + ".png");
}