#pragma once
#include "Common.h"
#include "Items.h"

constexpr int kLookCount = 12;
struct Look {
    const char* name; Color skin, hair, top, vest, pants, boots, gloves, accent; bool female, ponytail, hasVest;
};
extern const Look kLooks[kLookCount];

struct CharacterModel {
    Model head{}, hair{}, tail{}, torso{}, armU[2]{}, armL[2]{}, legU[2]{}, legL[2]{};
    int look = 0;
};
struct HeldModels { Model guns[I_COUNT]{}, pick{}; };   // one unique model per weapon, indexed by ItemId

struct Pose {
    float phase = 0, moveAmt = 0, swing = -1, pitch = 0, idle = 0;
    bool airborne = false, gun = false, pick = false;
    bool ads = false;
    float gunScale = 1.0f; Color gunTint = WHITE;
    int weapon = -1;            // ItemId of the held weapon (selects its model)
};

void CharacterBuild(CharacterModel& cm, int look);
void CharacterUnload(CharacterModel& cm);
void HeldBuild(HeldModels& h);
void HeldUnload(HeldModels& h);
void CharacterDraw(const CharacterModel& cm, const HeldModels& held, Vector3 pos, float yaw, const Pose& p, Color tint);