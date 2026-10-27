#pragma once
#include "Actor.h"
#include "Character.h"
#include "Hud.h"
#include "VFX.h"
#include <deque>
#include <string>
#include <vector>

struct Grenade { Vector3 p, v; float t; int owner; };
struct Fx { Vector3 p; float t, life, r; };
struct Hit3 { int kind = 0; int idx = -1; Vector3 p{}; float d = 0; bool head = false; };

enum class State { Login, Lobby, Match, Over };
enum class LobbyTab { Play = 0, BattlePass, Locker, ItemShop, Profile, COUNT };
enum class GameMode { Solo = 0, Duo, Private1v1, Private2v2, COUNT };
enum class PlayType { Local = 0, Online };

struct Account {
    std::string id, username, displayName;
    int vbucks = 0, level = 1, xp = 0, wins = 0, kills = 0, matches = 0;
    int battlePassTier = 1;
    int battleStars = 0;
    bool battlePassOwned = true;
    std::vector<std::string> owned;
    std::string outfit = "jonesy", pickaxe = "default_pick", glider = "default_glider";
    std::string banner = "Banner_Default";
    bool loggedIn = false;
};

struct ShopItem {
    std::string id, name, type, rarity, description, cid;
    int price = 0;
};

struct BpReward {
    std::string id, name, type, rarity, desc, image, cid;
    int amount = 0;
    bool valid = false;
};

struct BpTier {
    int tier = 0;
    BpReward paid, free;
};

struct FriendEntry {
    std::string username, displayName;
    std::string status;
    std::string activity;
    bool joinable = false;
    std::string outfit = "jonesy";
    int lookIndex = 0;
};

struct FriendRequest {
    std::string id, from, to;
};

struct Notify {
    std::string title;
    std::string body;
    float life = 3.5f;
};

struct LbEntry {
    int rank = 0;
    std::string username;
    int value = 0, level = 0;
};

class Game {
public:
    Game();
    ~Game();
    void Run();
private:
    void UpdateLogin(float dt);
    void DrawLogin();
    void TryLogin();
    void TrySignup();
    void LoadAccountFromJson(const std::string& json);
    void SaveCredentials();
    void LoadCredentials();
    void RefreshShop();
    void RefreshLeaderboards();
    void RefreshBattlePass();
    void RefreshFriends();
    void SetPresence(const char* status, const char* activity, bool joinable);
    void PushNotify(const std::string& title, const std::string& body);
    void DrawNotifications(int W);
    void SendFriendRequest();
    void RespondFriendRequest(const FriendRequest& req, bool accept);
    void JoinParty(const FriendEntry& f);
    void LeaveParty();
    void AwardMatchRewards();
    bool ConnectMatchmaker();
    void DisconnectMatchmaker();

    void UpdateLobby(float dt);
    void DrawLobby();
    void DrawLobbyNav(int W, int H);
    void DrawTabPlay(int W, int H);
    void DrawLobbyBackground(int W, int H, float t);
    void DrawModePicker(int W, int H);
    void DrawProgressionWidget(int W, int H);
    void DrawTabBattlePass(int W, int H);
    void DrawTabLocker(int W, int H);
    void DrawTabShop(int W, int H);
    void DrawTabProfile(int W, int H);

    void BeginLobbyCanvas();
    void EndLobbyCanvas();
    int LobbyW() const { return 960; }
    int LobbyH() const { return 720; }
    // Mouse in lobby canvas coords
    Vector2 LobbyMouse() const;

    void StartMatch();
    void EndMatch();
    void UpdateMatch(float dt);
    void DrawMatch();
    void RenderView(RenderTexture2D& rt, int W, int H, int actorIdx);
    void UpdateActor(int i, const Input& in, float dt);
    void BotThink(int i, float dt, Input& in);
    Input ReadInput(int device, float dt);
    void ComputeCamera(Actor& a);
    Hit3 TraceRay(Ray r, float range, int ignore);
    void FireWeapon(int i, Slot& s, const ItemDef& d);
    void Harvest(int i);
    void Explode(Vector3 p, float radius, float dmg, int owner);
    void Damage(int victim, float dmg, int attacker);
    void Eliminate(int victim, int attacker);
    void PickUp(Actor& a, int dropIdx);
    bool AddToInventory(Actor& a, const LootRoll& r, bool allowSwap);
    void OpenChest(int ci);
    void UpdateStorm(float dt);
    void Toast(Actor& a, const std::string& s) { a.toast = s; a.toastT = 2.0f; }
    int AliveCount() const;
    int HumansForMode() const;
    bool IsPrivateMode() const {
        return gameMode == GameMode::Private1v1 || gameMode == GameMode::Private2v2;
    }

    State state = State::Login;
    LobbyTab tab = LobbyTab::Play;
    GameMode gameMode = GameMode::Solo;
    PlayType playType = PlayType::Local;
    bool modePickerOpen = false;
    bool quit = false;
    bool pauseMenuOpen = false;
    MapId map = MAP_DUSTY;
    int p1Look = 0, p2Look = 1; bool p2Joined = false; int humans = 1;
    bool inParty = false;
    bool isPartyLeader = true;
    std::string partyMemberName;
    std::string partyMemberUser;
    int partyMemberLook = 1;

    CharacterModel chars[kLookCount];
    HeldModels held;
    Model padOn{}, padOff{};
    RenderTexture2D rtFull{}, rtHalf[2]{}, rtLobby{};
    World world;
    std::vector<Actor> actors;
    std::vector<Grenade> grenades;
    std::vector<Fx> fx;
    std::vector<FeedMsg> feed;
    float matchT = 0, stormR = 85, stormT = 80; int stormPhase = 0; bool shrinking = false;
    bool victory = false; int humanPlacement = 0;
    int matchKills = 0;

    Account account;
    char loginUser[32] = {};
    char loginPass[32] = {};
    char loginDisplay[32] = {};
    int loginFocus = 0;
    bool signupMode = false;
    bool rememberMe = true;
    std::string authError;
    float authMsgT = 0;

    std::vector<ShopItem> shopFeatured, shopDaily;
    std::vector<LbEntry> lbWins, lbKills;
    std::vector<FriendEntry> friends;
    std::vector<FriendRequest> incomingReqs;
    int friendSel = 0;
    char addFriendBuf[32] = {};
    bool addFriendFocus = false;
    int lbMode = 0;
    int shopSel = 0;
    int lockerSel = 0;
    int lockerCat = 0; // 0 outfit 1 glider 2 pickaxe 3 emote 4 banner
    bool lockerBrowsing = false;
    int equippedPick = 0;
    int equippedGlider = 0;
    int equippedEmote = 0;
    int equippedBanner = 0;
    std::string statusMsg;
    float statusMsgT = 0;
    bool backendOk = false;

    std::vector<Notify> notifications;

    std::vector<BpTier> bpTiers;
    std::string bpTitle, bpSubtitle;
    int bpSelTier = 0;
    int bpSelRow = 0;
    int bpMaxTier = 70;

    // Matchmaking
    bool matchmaking = false;
    float matchmakeT = 0;
    float matchmakeMinWait = 2.0f;

#ifdef _WIN32
    uintptr_t mmSock = (uintptr_t)-1;
#else
    int mmSock = -1;
#endif
    bool mmConnected = false;
};
