#pragma once
#include "Common.h"

enum Rarity { R_COMMON, R_UNCOMMON, R_RARE, R_EPIC, R_LEGENDARY };
enum AmmoType { AM_NONE, AM_LIGHT, AM_MEDIUM, AM_HEAVY, AM_SHELLS, AM_ROCKETS, AM_COUNT };
enum ItemKind { K_PICKAXE, K_WEAPON, K_HEAL, K_AMMO, K_THROWABLE };
enum ItemId {
    I_PICKAXE, I_PISTOL, I_PUMP, I_TACTICAL, I_AR, I_SMG, I_HUNTING, I_SNIPER, I_ROCKET,
    I_BANDAGE, I_MEDKIT, I_SMALL_SHIELD, I_SHIELD, I_GRENADE,
    I_AMMO_LIGHT, I_AMMO_MEDIUM, I_AMMO_HEAVY, I_AMMO_SHELLS, I_AMMO_ROCKETS,
    I_COUNT
};

struct ItemDef {
    const char* name; ItemKind kind; Rarity baseRarity; float weight; int stack;
    float dmg, rate, reload, spread, range; int mag, pellets; AmmoType ammo; bool autoFire, explosive; float radius;
    float heal, healMax, shield, shieldMax, useTime; int ammoPack; Color icon;
};

const ItemDef& GetItemDef(ItemId id);
Color RarityColor(Rarity r);
float RarityMult(Rarity r);

struct Slot {
    ItemId id = I_COUNT; Rarity rar = R_COMMON; int mag = 0; int count = 0;
    bool Empty() const { return id == I_COUNT; }
};
struct LootRoll { ItemId id = I_COUNT; Rarity rar = R_COMMON; int count = 1; };
LootRoll RollLoot(bool chest);
