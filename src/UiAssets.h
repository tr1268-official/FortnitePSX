#pragma once
// Lazy-loaded textures for UI art that is addressed by its path under assets/ (minimap, reticles, loading screens,
// phase icons). Independent of IconCache so nothing there has to change.
//
//   const Texture2D* t = UiTex("assets/ui/reticles/HUD_Xhair_Shotgun_Frame.png");
//   if (t) DrawTexturePro(*t, ...);          // nullptr when the file is missing -> callers draw a fallback
//
// Paths are relative to the working directory; "../" prefixes are tried too so it also works when the exe is
// started from a build folder (e.g. x64/Debug).
#include "Common.h"
#include <map>
#include <string>

namespace uiassets {
    inline std::map<std::string, Texture2D>& Cache() { static std::map<std::string, Texture2D> c; return c; }
    inline std::string Resolve(const std::string& rel) {
        static const char* roots[] = {"", "../", "../../", "../../../"};
        for (const char* r : roots) {
            std::string p = std::string(r) + rel;
            if (FileExists(p.c_str())) return p;
        }
        return std::string();
    }
}

inline const Texture2D* UiTex(const char* rel) {
    auto& c = uiassets::Cache();
    auto it = c.find(rel);
    if (it != c.end()) return it->second.id ? &it->second : nullptr;
    Texture2D t{};
    std::string p = uiassets::Resolve(rel);
    if (!p.empty()) {
        t = LoadTexture(p.c_str());
        if (t.id) SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
        else TraceLog(LOG_WARNING, "UiTex: failed to load %s", p.c_str());
    } else {
        TraceLog(LOG_WARNING, "UiTex: not found: %s", rel);
    }
    auto ins = c.emplace(rel, t);
    return ins.first->second.id ? &ins.first->second : nullptr;
}

inline void UiTexUnloadAll() {
    for (auto& kv : uiassets::Cache()) if (kv.second.id) UnloadTexture(kv.second);
    uiassets::Cache().clear();
}
