#pragma once
#include "Common.h"
#include <string>
#include <unordered_map>

class IconCache {
public:
    void Init();
    void Shutdown();
    // Returns nullptr if the file doesn't exist or failed to load.
    const Texture2D* Item(const std::string& itemName);
    const Texture2D* UI(const std::string& uiName);
    const Texture2D* UIItem(const std::string& uiItemName);
    // Fortnite-style cosmetic icon: assets/items/cosmetics/{CID}.png
    const Texture2D* Cosmetic(const std::string& cid);
private:
    Texture2D* Load(const std::string& path);
    std::unordered_map<std::string, Texture2D*> cache;
};
extern IconCache gIcons;
