#pragma once
#include "Actor.h"

struct FeedMsg { std::string s; float t; };
struct HudInfo { int alive; float stormT; bool shrinking; float stormR; const std::vector<FeedMsg>* feed; bool over; bool won; int placement; };

// Draws the 2017-style HUD in low-res render-texture space (W x H)
void DrawHud(const Actor& a, const World& w, int W, int H, const HudInfo& info);