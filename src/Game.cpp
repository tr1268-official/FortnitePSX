#include "Game.h"
#include "Renderer.h"
#include "IconCache.h"
#include "Audio.h"
#include "Net.h"
#include "JsonUtil.h"
#include <cstring>

static const float kWait[4] = {80, 50, 40, 30}, kShrink[4] = {50, 40, 30, 25}, kRad[5] = {85, 52, 32, 16, 4};
static const char* kTabNames[] = { "LOBBY", "BATTLE PASS", "LOCKER", "ITEM SHOP", "PROFILE" };
static const char* kModeNames[] = { "SOLO", "DUO", "1V1 PRIVATE", "2V2 PRIVATE" };
static const bool kUseBattleBus = true;     // false = spawn on the ground like before


static void Present(RenderTexture2D& rt, Rectangle dest) {
    Rectangle src = {0, 0, (float)rt.texture.width, -(float)rt.texture.height};
    DrawTexturePro(rt.texture, src, dest, {0, 0}, 0, WHITE);
}

Game::Game() {
    gR.Init();
    InitAudioDevice();
    gAudio.Init();
    gIcons.Init();
    SetExitKey(KEY_NULL);
    for (int i = 0; i < kLookCount; i++) CharacterBuild(chars[i], i);
    HeldBuild(held);
    SkydiveBuild(skyModels);
    world.gunModel = &held.guns[I_AR];
    { MeshBuilder a, b;
        a.Loft({{0, 1.0f, 1.0f, C(150, 150, 155)}, {0.14f, 1.0f, 1.0f, C(190, 190, 195)}, {0.14f, 0.82f, 0.82f, C(90, 235, 255)}}, 8);
        b.Loft({{0, 1.0f, 1.0f, C(150, 150, 155)}, {0.14f, 1.0f, 1.0f, C(190, 190, 195)}, {0.14f, 0.82f, 0.82f, C(55, 62, 68)}}, 8);
        padOn = gR.MakeModel(a); padOff = gR.MakeModel(b);
    }
    rtFull = LoadRenderTexture(1920, 1080);
    rtHalf[0] = LoadRenderTexture(960, 1080);
    rtHalf[1] = LoadRenderTexture(960, 1080);
    rtLobby = LoadRenderTexture(1280, 720);
    SetTextureFilter(rtLobby.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(rtFull.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(rtHalf[0].texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(rtHalf[1].texture, TEXTURE_FILTER_BILINEAR);
    LoadCredentials();
}

Game::~Game() {
    if (account.loggedIn) SetPresence("offline", "none", false);
    DisconnectMatchmaker();
    world.Unload();
    for (int i = 0; i < kLookCount; i++) CharacterUnload(chars[i]);
    HeldUnload(held);
    SkydiveUnload(skyModels);
    gR.FreeModel(padOn); gR.FreeModel(padOff);
    UnloadRenderTexture(rtFull); UnloadRenderTexture(rtHalf[0]); UnloadRenderTexture(rtHalf[1]); UnloadRenderTexture(rtLobby);
    gIcons.Shutdown();
    gAudio.Shutdown();
    CloseAudioDevice();
    gR.Shutdown();
}

void Game::Run() {
    while (!WindowShouldClose() && !quit) {
        float dt = fminf(GetFrameTime(), 0.05f);
        // Heartbeat presence so other clients see us online / in lobby / in match
        static float presenceT = 0, friendsT = 0;
        presenceT += dt; friendsT += dt;
        if (account.loggedIn && presenceT > 2.0f) {
            presenceT = 0;
            if (state == State::Lobby)
                SetPresence("online", "lobby", true);
            else if (state == State::Match || state == State::Over)
                SetPresence("online", "match", false);
        }
        if (account.loggedIn && state == State::Lobby && friendsT > 2.5f) {
            friendsT = 0;
            RefreshFriends();
        }
        if (state == State::Login) { UpdateLogin(dt); DrawLogin(); }
        else if (state == State::Lobby) { UpdateLobby(dt); DrawLobby(); }
        else { UpdateMatch(dt); DrawMatch(); }
    }
    if (account.loggedIn) SetPresence("offline", "none", false);
    DisconnectMatchmaker();
}


// =====================================================================
//  LOGIN / ACCOUNT
// =====================================================================

// Canonical outfit names in the SAME ORDER as kLooks[] / chars[] / outfitCid[].
// Used for name<->index mapping when saving/loading equipped outfits.
static const char* kOutfitNames[kLookCount] = {
    "jonesy",                 // 0
    "ramirez",                // 1
    "headhunter",             // 2
    "wildcat",                // 3
    "arial_assault_trooper",  // 4
    "renegade_raider",        // 5
    "ghoul_trooper",          // 6
    "skull_trooper",          // 7
    "brite_bomber",           // 8
    "punk_funk",              // 9
    "ginge",                  // 10
    "merry_maruader"          // 11
};

// Convert an outfit name (with a few legacy aliases) to a look index in [0, kLookCount).
static int OutfitNameToIndex(const std::string& name) {
    if (name.empty()) return 0;
    for (int i = 0; i < kLookCount; i++)
        if (name == kOutfitNames[i]) return i;
    // legacy aliases seen in older JSON
    if (name == "ranger")  return 1;
    if (name == "scout")   return 2;
    if (name == "medic")   return 3;
    return 0;
}

// Minimal JSON string-array reader: finds "key":[ "a", "b", ... ] and returns the strings.
static std::vector<std::string> JsonGetStringArray(const std::string& json, const std::string& key) {
    std::vector<std::string> out;
    std::string pat = "\"" + key + "\"";
    size_t k = json.find(pat);
    if (k == std::string::npos) return out;
    size_t lb = json.find('[', k);
    if (lb == std::string::npos) return out;
    size_t i = lb + 1;
    while (i < json.size()) {
        while (i < json.size() &&
               (json[i]==' '||json[i]==','||json[i]=='\n'||json[i]=='\r'||json[i]=='\t'))
            i++;
        if (i >= json.size() || json[i] == ']') break;
        if (json[i] != '"') break;                 // not a string array
        size_t end = json.find('"', i + 1);
        if (end == std::string::npos) break;
        out.push_back(json.substr(i + 1, end - i - 1));
        i = end + 1;
    }
    return out;
}

void Game::LoadAccountFromJson(const std::string& json) {
    account.id = JsonGetString(json, "id");
    account.username = JsonGetString(json, "username");
    account.displayName = JsonGetString(json, "displayName");
    if (account.displayName.empty()) account.displayName = account.username;
    account.vbucks = JsonGetInt(json, "vbucks");
    account.level = JsonGetInt(json, "level", 1);
    account.xp = JsonGetInt(json, "xp");
    account.wins = JsonGetInt(json, "wins");
    account.kills = JsonGetInt(json, "kills");
    account.matches = JsonGetInt(json, "matches");
    account.battlePassTier = JsonGetInt(json, "battlePassTier", 1);
    account.battlePassOwned = JsonGetBool(json, "battlePassOwned");
    account.loggedIn = !account.id.empty();

    // --- owned cosmetics ---
    account.owned.clear();
    for (auto& s : JsonGetStringArray(json, "owned"))
        account.owned.push_back(s);
    // Always own the four defaults + anything the server omitted
    for (int i = 0; i < 4; i++)
        if (std::find(account.owned.begin(), account.owned.end(), kOutfitNames[i]) == account.owned.end())
            account.owned.push_back(kOutfitNames[i]);

    // --- equipped outfit ---
    std::string eq = JsonGetObject(json, "equipped");
    std::string outfit = JsonGetString(eq, "outfit");
    int idx = OutfitNameToIndex(outfit);

    // Guard: if the mapped index isn't actually owned, fall back to jonesy (0).
    bool ownsEquipped = false;
    for (auto& o : account.owned)
        if (o == kOutfitNames[idx]) { ownsEquipped = true; break; }
    if (!ownsEquipped) idx = 0;

    p1Look = idx;
    account.outfit = kOutfitNames[p1Look];

    // Optional: also honor equipped glider/pickaxe/emote/banner if present in JSON.
    if (!eq.empty()) {
        std::string g = JsonGetString(eq, "glider");
        std::string pk = JsonGetString(eq, "pickaxe");
        std::string em = JsonGetString(eq, "emote");
        std::string bn = JsonGetString(eq, "banner");
        if (g == "mako")            equippedGlider = 1;
        else if (g == "founder" || g == "founders_glider") equippedGlider = 2;
        if (pk == "raiders_revenge" || pk == "raider_revenge") equippedPick = 1;
        else if (pk == "candy_axe") equippedPick = 2;
        if (em == "default_dance")  equippedEmote = 1;
        else if (em == "ride_the_pony") equippedEmote = 2;
        if (bn == "Banner_Trophy")  equippedBanner = 1;
        else if (bn == "Banner_Season1") equippedBanner = 2;
        if (!bn.empty()) account.banner = bn;
    }
}
void Game::TryLogin() {
    authError.clear();
    if (loginUser[0] == 0 || loginPass[0] == 0) { authError = "Enter username and password"; authMsgT = 3; return; }
    std::string body = std::string("{\"username\":\"") + loginUser + "\",\"password\":\"" + loginPass + "\"}";
    int status = 0;
    std::string resp = HttpPost("/api/login", body, &status);
    if (status == 200 && !resp.empty()) {
        std::string userObj = JsonGetObject(resp, "user");
        if (userObj.empty()) userObj = resp;
        LoadAccountFromJson(userObj);
        if (account.loggedIn) {
            state = State::Lobby;
            tab = LobbyTab::Play;
            {
                char b[128]; snprintf(b,sizeof b,"{\"username\":\"%s\"}", account.username.c_str());
                int stc=0; HttpPost("/api/party/clear", b, &stc);
                inParty=false; isPartyLeader=true; p2Joined=false; partyMemberUser.clear(); partyMemberName.clear();
            }
            RefreshShop(); RefreshLeaderboards(); RefreshBattlePass();
            SetPresence("online","lobby",true);
            RefreshFriends();
            SaveCredentials();
            statusMsg = "Welcome back, " + account.displayName + "!";
            statusMsgT = 3;
            return;
        }
    }
    // Offline fallback for demo
    if (strcmp(loginUser, "Player1") == 0 && strcmp(loginPass, "password") == 0) {
        account.id = "user_default"; account.username = "Player1"; account.displayName = "Player1";
        account.vbucks = 2500; account.level = 15; account.xp = 3200; account.wins = 3; account.kills = 47; account.matches = 28;
        account.battlePassTier = 12; account.battlePassOwned = false; account.loggedIn = true;
        account.owned = {"jonesy","ramirez","headhunter","wildcat", "arial_assault_trooper", "renegade_raider", "ghoul_trooper", "skull_trooper", "brite_bomber", "punk_funk", "ginge", "merry_maruader"};
        state = State::Lobby; tab = LobbyTab::Play;
        inParty=false; isPartyLeader=true; p2Joined=false; partyMemberUser.clear(); partyMemberName.clear();
        RefreshShop(); RefreshLeaderboards(); RefreshBattlePass(); RefreshFriends();
        SaveCredentials(); SetPresence("online","lobby",true);
        statusMsg = "Logged in offline (backend offline)"; statusMsgT = 3;
        return;
    }
    authError = status == 401 ? "Invalid credentials" : "Cannot reach server (start backend)";
    authMsgT = 3;
}

void Game::TrySignup() {
    authError.clear();
    if (loginUser[0] == 0 || loginPass[0] == 0) { authError = "Enter username and password"; authMsgT = 3; return; }
    std::string disp = loginDisplay[0] ? loginDisplay : loginUser;
    std::string body = std::string("{\"username\":\"") + loginUser + "\",\"password\":\"" + loginPass + "\",\"displayName\":\"" + disp + "\"}";
    int status = 0;
    std::string resp = HttpPost("/api/signup", body, &status);
    if (status == 200 && !resp.empty()) {
        std::string userObj = JsonGetObject(resp, "user");
        if (userObj.empty()) userObj = resp;
        LoadAccountFromJson(userObj);
        if (account.loggedIn) {
            state = State::Lobby; tab = LobbyTab::Play;
            RefreshShop(); RefreshLeaderboards(); RefreshBattlePass(); RefreshFriends();
            SaveCredentials(); SetPresence("online","lobby",true);
            statusMsg = "Account created!"; statusMsgT = 3;
            return;
        }
    }
    if (status == 409) authError = "Username already taken";
    else authError = "Signup failed – is the backend running?";
    authMsgT = 3;
}

void Game::RefreshShop() {
    shopFeatured.clear(); shopDaily.clear();
    int st = 0;
    std::string resp = HttpGet("/api/shop", &st);
    if (st != 200 || resp.empty()) {
        // offline demo stock
        shopFeatured.push_back({"ghoul_trooper","Ghoul Trooper","outfit","epic","Nightmares.","CID_029_Athena_Commando_F_Halloween",1200});
        shopFeatured.push_back({"skull_trooper","Skull Trooper","outfit","legendary","Skulls.","CID_030_Athena_Commando_M_Halloween",2000});
        shopDaily.push_back({"raider_revenge","Raider's Revenge","pickaxe","uncommon","Swing with style.","Pickaxe_Lockjaw",800});
        shopDaily.push_back({"candy_axe","Candy Axe","pickaxe","uncommon","Sweet swings.","Pickaxe_ID_015_HolidayPickaxe",800});
        shopDaily.push_back({"mako","Mako","glider","uncommon","Sleek and fast.","Glider_ID_002_Mako",500});
        shopDaily.push_back({"default_dance","Default Dance","emote","common","The classic.","EID_DanceMoves",200});
        backendOk = false;
        return;
    }
    backendOk = true;
    auto parseList = [&](const std::string& key, std::vector<ShopItem>& out) {
        for (auto& obj : JsonGetArrayObjects(resp, key)) {
            ShopItem it;
            it.id = JsonGetString(obj, "id");
            it.name = JsonGetString(obj, "name");
            it.type = JsonGetString(obj, "type");
            it.rarity = JsonGetString(obj, "rarity");
            it.description = JsonGetString(obj, "description");
            it.cid = JsonGetString(obj, "cid");
            if (it.cid.empty()) it.cid = JsonGetString(obj, "image");
            it.price = JsonGetInt(obj, "price");
            out.push_back(it);
        }
    };
    parseList("featured", shopFeatured);
    parseList("daily", shopDaily);
}

void Game::RefreshLeaderboards() {
    lbWins.clear(); lbKills.clear();
    int st = 0;
    std::string resp = HttpGet("/api/leaderboards", &st);
    if (st != 200 || resp.empty()) {
        lbWins = {{1,"Ninja",142,88},{2,"Tfue",128,92},{3,"Myth",115,85},{4,"Player1",3,15}};
        lbKills = {{1,"Ninja",1840,88},{2,"Tfue",1620,92},{3,"Myth",1510,85},{4,"Player1",47,15}};
        return;
    }
    for (auto& obj : JsonGetArrayObjects(resp, "wins"))
        lbWins.push_back({JsonGetInt(obj,"rank"), JsonGetString(obj,"username"), JsonGetInt(obj,"value"), JsonGetInt(obj,"level")});
    for (auto& obj : JsonGetArrayObjects(resp, "kills"))
        lbKills.push_back({JsonGetInt(obj,"rank"), JsonGetString(obj,"username"), JsonGetInt(obj,"value"), JsonGetInt(obj,"level")});
}

void Game::RefreshBattlePass() {
    bpTiers.clear();
    bpTitle = "SEASON 1 BATTLE PASS";
    bpSubtitle = "Free for every player: both rows unlock as you level up. Nothing to buy.";
    bpMaxTier = 70;
    int st = 0;
    std::string resp = HttpGet("/api/battlepass", &st);
    auto parseReward = [](const std::string& obj) -> BpReward {
        BpReward r;
        if (obj.empty() || obj == "null") return r;
        r.id = JsonGetString(obj, "id");
        r.name = JsonGetString(obj, "name");
        r.type = JsonGetString(obj, "type");
        r.rarity = JsonGetString(obj, "rarity");
        r.desc = JsonGetString(obj, "desc");
        if (r.desc.empty()) r.desc = JsonGetString(obj, "description");
        r.image = JsonGetString(obj, "image");
        r.cid = JsonGetString(obj, "cid");
        if (r.cid.empty()) r.cid = r.image;
        r.amount = JsonGetInt(obj, "amount");
        r.valid = !r.id.empty() || !r.name.empty();
        return r;
    };
    if (st == 200 && !resp.empty()) {
        bpTitle = JsonGetString(resp, "name");
        if (bpTitle.empty()) bpTitle = "SEASON 1 BATTLE PASS";
        bpSubtitle = JsonGetString(resp, "subtitle");
        bpMaxTier = JsonGetInt(resp, "maxTier", 70);
        for (auto& obj : JsonGetArrayObjects(resp, "tiers")) {
            BpTier t;
            t.tier = JsonGetInt(obj, "tier");
            // paid / free may be nested objects
            std::string paid = JsonGetObject(obj, "paid");
            std::string free = JsonGetObject(obj, "free");
            t.paid = parseReward(paid);
            t.free = parseReward(free);
            bpTiers.push_back(t);
        }
    }
    if (bpTiers.empty()) {
        // offline fallback matching the reference layout
        auto R = [](const char* id, const char* n, const char* ty, const char* rar, const char* d, const char* img, const char* cid, int amt=0) {
            BpReward r; r.id=id; r.name=n; r.type=ty; r.rarity=rar; r.desc=d; r.image=img; r.cid=cid?cid:img; r.amount=amt; r.valid=true; return r;
        };
        bpTiers.push_back({1, R("aerial_assault_trooper","Aerial Assault Trooper","Outfit","rare","Rare aerial assault trooper outfit.","CID_017_Athena_Commando_M","CID_017_Athena_Commando_M"), {}});
        bpTiers.push_back({2, R("trophy","Victory Trophy","Banner","uncommon","Celebrate your wins.","Banner_Trophy","Banner_Trophy"), R("raiders_revenge","Raider's Revenge","Pickaxe","uncommon","Swing with style.","Pickaxe_Lockjaw","Pickaxe_Lockjaw")});
        bpTiers.push_back({3, R("renegade_raider","Renegade Raider","Outfit","rare","Classic Season 1 raider.","CID_028_Athena_Commando_F","CID_028_Athena_Commando_F"), R("vb50a","50 V-Bucks","Currency","common","50 V-Bucks.","VBucks","VBucks",50)});
        bpTiers.push_back({4, R("helmet","Trooper Helmet","Style","uncommon","Helmet style.","CID_017_Athena_Commando_M_Helmet","CID_017_Athena_Commando_M_Helmet"), R("Founder","Founder Glider","Glider","uncommon","Founder's Pack.","Glider_ID_003_FounderGlider","Glider_ID_003_FounderGlider")});
        bpTiers.push_back({5, R("bush","Bush","Outfit","uncommon","Blend into the foliage.","CID_029_Athena_Commando_F_Halloween","CID_029_Athena_Commando_F_Halloween"), {}});
        bpTiers.push_back({6, R("ride_the_pony","Ride The Pony","Emote","rare","Express yourself on the battlefield.","EID_RideThePony","EID_RideThePony"), R("vb50b","50 V-Bucks","Currency","common","50 V-Bucks.","VBucks","VBucks",50)});
        bpTiers.push_back({7, R("llama","Loot Llama","Back Bling","rare","Pinata-powered back bling.","BID_001_Llama","BID_001_Llama"), {}});
    }
    bpSelTier = 0;
    bpSelRow = 0;
}


void Game::SaveCredentials() {
    if (!rememberMe || !account.loggedIn) return;
    // simple local file next to cwd
    FILE* f = fopen("save_credentials.txt", "w");
    if (!f) return;
    fprintf(f, "%s\n%s\n%s\n", account.username.c_str(), loginPass, account.id.c_str());
    fclose(f);
}

void Game::LoadCredentials() {
    FILE* f = fopen("save_credentials.txt", "r");
    if (!f) return;
    char u[64] = {}, pw[64] = {}, id[64] = {};
    if (fgets(u, sizeof u, f)) {
        // strip newline
        size_t n = strlen(u); while (n && (u[n-1]=='\n'||u[n-1]=='\r')) u[--n]=0;
        strncpy(loginUser, u, 31);
    }
    if (fgets(pw, sizeof pw, f)) {
        size_t n = strlen(pw); while (n && (pw[n-1]=='\n'||pw[n-1]=='\r')) pw[--n]=0;
        strncpy(loginPass, pw, 31);
    }
    if (fgets(id, sizeof id, f)) {
        size_t n = strlen(id); while (n && (id[n-1]=='\n'||id[n-1]=='\r')) id[--n]=0;
    }
    fclose(f);
    // auto-attempt login once at startup (called from ctor before window loop is fine)
    if (loginUser[0] && loginPass[0]) {
        // deferred: flag; actual TryLogin in first frame via empty check in UpdateLogin
    }
}

void Game::RefreshFriends() {
    friends.clear();
    incomingReqs.clear();
    int st = 0;
    std::string path = "/api/friends/" + (account.username.empty() ? std::string("Player1") : account.username);
    std::string resp = HttpGet(path, &st);
    if (st != 200 || resp.empty()) {
        friends.push_back({"Ninja","Ninja","online","lobby",true,"ramirez",1});
        friends.push_back({"Tfue","Tfue","online","match",false,"headhunter",2});
        friends.push_back({"Myth","Myth","offline","none",false,"wildcat",3});
        return;
    }
    for (auto& obj : JsonGetArrayObjects(resp, "friends")) {
        FriendEntry e;
        e.username = JsonGetString(obj, "username");
        e.displayName = JsonGetString(obj, "displayName");
        if (e.displayName.empty()) e.displayName = e.username;
        e.status = JsonGetString(obj, "status");
        e.activity = JsonGetString(obj, "activity");
        e.joinable = JsonGetBool(obj, "joinable");
        e.outfit = JsonGetString(obj, "outfit");
        e.lookIndex = JsonGetInt(obj, "lookIndex", 0);
        if (e.lookIndex < 0 || e.lookIndex >= kLookCount) e.lookIndex = 0;
        friends.push_back(e);
    }
    static std::string seenReq;
    for (auto& obj : JsonGetArrayObjects(resp, "incoming")) {
        FriendRequest r;
        r.id = JsonGetString(obj, "id");
        r.from = JsonGetString(obj, "from");
        r.to = JsonGetString(obj, "to");
        incomingReqs.push_back(r);
        if (r.id != seenReq) {
            PushNotify("Friend Request Received", std::string("From ") + r.from);
            seenReq = r.id;
        }
    }
    // Party sync — only if server still has a valid party with an online member
    std::string party = JsonGetObject(resp, "party");
    bool hadParty = inParty;
    if (!party.empty() && party != "null" && party.find("leader") != std::string::npos) {
        std::string leader = JsonGetString(party, "leader");
        std::string member = JsonGetString(party, "member");
        std::string other = (leader == account.username) ? member : leader;
        // Only keep party if other is in our friends list AND currently online+lobby
        bool otherOk = false;
        for (auto& f : friends) {
            if (f.username == other && f.status == "online" && f.activity != "match") {
                otherOk = true;
                partyMemberUser = other;
                partyMemberName = f.displayName;
                partyMemberLook = f.lookIndex;
                if (partyMemberLook < 0 || partyMemberLook >= kLookCount) partyMemberLook = 0;
                break;
            }
        }
        // fallback to party payload display if friend list missing but presence ok
        if (!otherOk) {
            // treat as invalid — don't restore
            otherOk = false;
        }
        if (otherOk) {
            inParty = true;
            isPartyLeader = (leader == account.username);
            p2Joined = true;
            p2Look = partyMemberLook;
        } else {
            inParty = false;
            isPartyLeader = true;
            p2Joined = false;
            partyMemberUser.clear();
            partyMemberName.clear();
            // clear stale server party
            char body[128];
            snprintf(body, sizeof body, "{\"username\":\"%s\"}", account.username.c_str());
            int st2 = 0;
            HttpPost("/api/party/clear", body, &st2);
        }
    } else {
        // No party from server
        if (hadParty) {
            // keep local party only if we just created it this session; otherwise clear
        }
        inParty = false;
        isPartyLeader = true;
        if (!inParty) {
            // don't force-clear p2Joined if local split-screen
            // only clear if it was a network party
            if (hadParty) {
                p2Joined = false;
                partyMemberUser.clear();
                partyMemberName.clear();
            }
        }
    }
}

void Game::SetPresence(const char* status, const char* activity, bool joinable) {
    if (account.username.empty()) return;
    char body[384];
    snprintf(body, sizeof body,
        "{\"username\":\"%s\",\"status\":\"%s\",\"activity\":\"%s\",\"joinable\":%s,"
        "\"displayName\":\"%s\",\"outfit\":\"%s\",\"lookIndex\":%d}",
        account.username.c_str(), status, activity, joinable ? "true" : "false",
        account.displayName.c_str(), account.outfit.c_str(), p1Look);
    int st = 0;
    HttpPost("/api/presence", body, &st);
}

void Game::PushNotify(const std::string& title, const std::string& body) {
    notifications.push_back({title, body, 3.8f});
    if (notifications.size() > 5) notifications.erase(notifications.begin());
}

void Game::DrawNotifications(int W) {
    float y = 44;
    for (int i = (int)notifications.size() - 1; i >= 0; --i) {
        auto& n = notifications[i];
        float alpha = n.life > 0.4f ? 1.0f : n.life / 0.4f;
        // slide in from left based on remaining life near start
        float slide = 0;
        if (n.life > 3.4f) slide = (n.life - 3.4f) / 0.4f; // 0..1 early
        float x = -180.0f * slide;
        Color bg = Color{20, 28, 50, (unsigned char)(230 * alpha)};
        Color titleC = Color{250, 200, 40, (unsigned char)(255 * alpha)};
        Color bodyC = Color{200, 210, 230, (unsigned char)(255 * alpha)};
        DrawRectangle((int)x + 8, (int)y, 220, 48, bg);
        DrawRectangle((int)x + 8, (int)y, 4, 48, Color{250, 200, 40, (unsigned char)(255 * alpha)});
        DrawTextEx(gFont, n.title.c_str(), {x + 20, y + 6}, 14, 1, titleC);
        DrawTextEx(gFont, n.body.c_str(), {x + 20, y + 26}, 12, 1, bodyC);
        y += 54;
    }
}

void Game::SendFriendRequest() {
    if (addFriendBuf[0] == 0) {
        statusMsg = "Enter a username"; statusMsgT = 2; return;
    }
    if (account.username.empty()) {
        statusMsg = "Not logged in"; statusMsgT = 2; return;
    }
    char body[256];
    snprintf(body, sizeof body, "{\"from\":\"%s\",\"to\":\"%s\"}", account.username.c_str(), addFriendBuf);
    int st = 0;
    std::string resp = HttpPost("/api/friends/request", body, &st);
    if (st == 200) {
        PushNotify("Friend Request Sent", std::string("To ") + addFriendBuf);
        statusMsg = std::string("Request sent to ") + addFriendBuf; statusMsgT = 2;
        addFriendBuf[0] = 0;
    } else {
        std::string err = JsonGetString(resp, "error");
        if (err.empty()) err = "Could not send request";
        statusMsg = err; statusMsgT = 2.5f;
        PushNotify("Request Failed", err);
    }
}

void Game::RespondFriendRequest(const FriendRequest& req, bool accept) {
    char body[256];
    snprintf(body, sizeof body, "{\"requestId\":\"%s\",\"accept\":%s,\"username\":\"%s\"}",
             req.id.c_str(), accept ? "true" : "false", account.username.c_str());
    int st = 0;
    HttpPost("/api/friends/respond", body, &st);
    if (st == 200) {
        if (accept) PushNotify("Friend Added", std::string("You are now friends with ") + req.from);
        else PushNotify("Request Declined", std::string("From ") + req.from);
        RefreshFriends();
    }
}

void Game::JoinParty(const FriendEntry& f) {
    if (f.status != "online") {
        statusMsg = f.displayName + " is offline"; statusMsgT = 2; return;
    }
    if (f.activity == "match") {
        statusMsg = f.displayName + " is in a match"; statusMsgT = 2; return;
    }
    if (!f.joinable) {
        statusMsg = f.displayName + " is not joinable"; statusMsgT = 2; return;
    }
    if (inParty) {
        statusMsg = "Leave your current party first (L)"; statusMsgT = 2; return;
    }
    char body[256];
    snprintf(body, sizeof body, "{\"from\":\"%s\",\"to\":\"%s\"}", account.username.c_str(), f.username.c_str());
    int st = 0;
    std::string resp = HttpPost("/api/party/invite", body, &st);
    if (st == 200) {
        inParty = true;
        partyMemberUser = f.username;
        partyMemberName = f.displayName;
        partyMemberLook = f.lookIndex;
        if (partyMemberLook < 0 || partyMemberLook >= kLookCount) partyMemberLook = 0;
        p2Joined = true;
        p2Look = partyMemberLook;
        isPartyLeader = true; // inviter is leader
        if (gameMode == GameMode::Solo) gameMode = GameMode::Duo;
        PushNotify("Party Joined", std::string("With ") + f.displayName);
        statusMsg = std::string("In party with ") + f.displayName; statusMsgT = 2.5f;
    } else {
        std::string err = JsonGetString(resp, "error");
        if (err.empty()) err = "Join failed";
        statusMsg = err; statusMsgT = 2;
    }
}

void Game::LeaveParty() {
    char body[128];
    snprintf(body, sizeof body, "{\"username\":\"%s\"}", account.username.c_str());
    int st = 0;
    HttpPost("/api/party/leave", body, &st);
    inParty = false;
    isPartyLeader = true;
    partyMemberName.clear();
    partyMemberUser.clear();
    p2Joined = false;
    PushNotify("Party Left", "You left the party");
}

void Game::AwardMatchRewards() {
    if (IsPrivateMode()) {
        statusMsg = "Private match — no V-Bucks"; statusMsgT = 2.5f;
        return;
    }
    int k = matchKills;
    bool win = victory;
    int gain = k * 10 + (win ? 50 : 0);
    if (gain <= 0 && !win) {
        // still count match
    }
    account.vbucks += gain;
    account.kills += k;
    account.matches += 1;
    if (win) account.wins += 1;

    if (!account.id.empty()) {
        char body[256];
        snprintf(body, sizeof body,
            "{\"userId\":\"%s\",\"kills\":%d,\"win\":%s,\"privateMatch\":false}",
            account.id.c_str(), k, win ? "true" : "false");
        int st = 0;
        std::string resp = HttpPost("/api/stats/reward", body, &st);
        if (st == 200) {
            int serverVb = JsonGetInt(JsonGetObject(resp, "user"), "vbucks", account.vbucks);
            account.vbucks = serverVb;
            int gained = JsonGetInt(resp, "vbucksGained", gain);
            gain = gained;
        }
    }
    if (gain > 0)
        PushNotify("V-Bucks Earned", TextFormat("+%d V-Bucks (%d kills%s)", gain, k, win ? ", WIN" : ""));
    matchKills = 0;
}



bool Game::ConnectMatchmaker() {
    if (playType == PlayType::Local) {
        mmConnected = false;
        MatchmakerDisconnect();
        return true;
    }
    std::string err;
    if (!MatchmakerConnect(account.displayName, kModeNames[(int)gameMode], &err)) {
        statusMsg = err.empty() ? "Matchmaking failed" : err;
        statusMsgT = 3.5f;
        PushNotify("Matchmaking Failed", "Game server offline");
        mmConnected = false;
        return false;
    }
    mmConnected = true;
    PushNotify("Match Found", "Connected to game server");
    return true;
}

void Game::DisconnectMatchmaker() {
    MatchmakerDisconnect();
    mmConnected = false;
}


void Game::DrawLobbyBackground(int W, int H, float t) {
    // Deep blue gradient matching the geometric network reference
    DrawRectangleGradientV(0, 0, W, H, Color{10, 40, 90, 255}, Color{2, 6, 20, 255});
    // Animated network mesh
    const int cols = 14, rows = 10;
    float cw = W / (float)(cols - 1);
    float ch = H / (float)(rows - 1);
    auto node = [&](int i, int j) -> Vector2 {
        float ox = sinf(t * 0.35f + i * 0.7f + j * 0.4f) * 6.0f;
        float oy = cosf(t * 0.28f + i * 0.5f + j * 0.6f) * 5.0f;
        return { i * cw + ox, j * ch + oy + 20 };
    };
    // edges
    for (int j = 0; j < rows; j++) {
        for (int i = 0; i < cols; i++) {
            Vector2 a = node(i, j);
            if (i + 1 < cols) {
                Vector2 b = node(i + 1, j);
                float pulse = 0.35f + 0.25f * sinf(t * 1.2f + i * 0.3f + j * 0.2f);
                DrawLineEx(a, b, 1.0f, Color{40, 120, 200, (unsigned char)(90 * pulse)});
            }
            if (j + 1 < rows) {
                Vector2 b = node(i, j + 1);
                float pulse = 0.35f + 0.25f * sinf(t * 1.1f + i * 0.2f + j * 0.3f);
                DrawLineEx(a, b, 1.0f, Color{40, 120, 200, (unsigned char)(80 * pulse)});
            }
            if (i + 1 < cols && j + 1 < rows) {
                Vector2 b = node(i + 1, j + 1);
                DrawLineEx(a, b, 1.0f, Color{30, 100, 180, 35});
            }
        }
    }
    // nodes
    for (int j = 0; j < rows; j++) {
        for (int i = 0; i < cols; i++) {
            Vector2 p = node(i, j);
            float r = 1.5f + 0.8f * sinf(t * 2.0f + i + j);
            DrawCircleV(p, r, Color{80, 170, 255, 160});
        }
    }
    // soft vignette bottom
    DrawRectangleGradientV(0, H * 2 / 3, W, H / 3, Color{0, 0, 0, 0}, Color{0, 0, 0, 160});
}

// ---- Friends panel (centred on screen) ----
struct FriendsGeo { int px, py, pw, ph, listY, rowH, visible, fieldX, fieldY, fieldW, fieldH; };
static FriendsGeo FriendsLayout(int W, int H) {
    FriendsGeo g;
    g.pw = 480; g.ph = (H - 60 < 440) ? H - 60 : 440;
    g.px = (W - g.pw) / 2; g.py = (H - g.ph) / 2;
    g.rowH = 34; g.listY = g.py + 52;
    g.fieldX = g.px + 20; g.fieldW = g.pw - 40; g.fieldH = 28; g.fieldY = g.py + g.ph - 50;
    int listH = g.fieldY - 24 - g.listY;
    g.visible = listH / g.rowH; if (g.visible < 1) g.visible = 1;
    return g;
}

void Game::DrawFriendsPanel(int W, int H) {
    FriendsGeo g = FriendsLayout(W, H);
    DrawRectangle(0, 0, W, H, Color{0, 0, 0, 180});
    DrawRectangle(g.px, g.py, g.pw, g.ph, Color{18, 28, 52, 255});
    DrawRectangleLines(g.px, g.py, g.pw, g.ph, Color{250, 200, 40, 255});
    DrawTextEx(gFont, "FRIENDS", {(float)(g.px + 20), (float)(g.py + 14)}, 22, 1, WHITE);

    std::string partyTxt = inParty ? ("In party with " + partyMemberName) : std::string("Not in a party");
    float ptw = MeasureTextEx(gFont, partyTxt.c_str(), 13, 1).x;
    DrawTextEx(gFont, partyTxt.c_str(), {(float)(g.px + g.pw - 20) - ptw, (float)(g.py + 20)}, 13, 1, Color{160, 175, 200, 255});

    int nReq = (int)incomingReqs.size(), nFr = (int)friends.size(), total = 1 + nReq + nFr;
    if (friendSel >= total) friendSel = total - 1;
    if (friendSel < 0) friendSel = 0;
    int first = friendSel < g.visible ? 0 : friendSel - g.visible + 1;

    for (int v = 0; v < g.visible; v++) {
        int idx = first + v;
        if (idx >= total) break;
        int y = g.listY + v * g.rowH;
        bool sel = idx == friendSel;
        DrawRectangle(g.px + 20, y, g.pw - 40, g.rowH - 4, sel ? Color{250, 200, 30, 255} : Color{30, 45, 80, 255});
        Color tc = sel ? BLACK : WHITE;
        Color sc = sel ? Color{40, 40, 40, 255} : Color{160, 175, 200, 255};
        std::string left, right;
        int tx = g.px + 32;
        if (idx == 0) {
            left = "Local Player 2 (split-screen)";
            right = inParty ? "LOCKED" : (p2Joined ? "ON" : "OFF");
        } else if (idx - 1 < nReq) {
            left = incomingReqs[idx - 1].from + " sent a friend request";
            right = "A accept / X decline";
        } else {
            const FriendEntry& f = friends[idx - 1 - nReq];
            Color dot = f.status != "online" ? Color{120, 125, 135, 255}
                      : (f.activity == "match" ? Color{250, 200, 40, 255} : Color{90, 220, 90, 255});
            DrawCircle(g.px + 36, y + 13, 5, dot);
            tx = g.px + 50;
            left = f.displayName;
            if (inParty && f.username == partyMemberUser) right = "IN PARTY";
            else if (f.status != "online") right = "Offline";
            else if (f.activity == "match") right = "In match";
            else if (f.activity == "lobby") right = f.joinable ? "In lobby - invite" : "In lobby";
            else right = f.activity;
        }
        DrawTextEx(gFont, left.c_str(), {(float)tx, (float)(y + 6)}, 15, 1, tc);
        float rw = MeasureTextEx(gFont, right.c_str(), 12, 1).x;
        DrawTextEx(gFont, right.c_str(), {(float)(g.px + g.pw - 32) - rw, (float)(y + 8)}, 12, 1, sc);
    }
    if (nFr == 0 && nReq == 0)
        DrawTextEx(gFont, "No friends yet - add one below", {(float)(g.px + 24), (float)(g.listY + g.rowH + 8)}, 13, 1, Color{150, 165, 190, 255});

    // Add friend field
    DrawTextEx(gFont, "ADD FRIEND", {(float)g.fieldX, (float)(g.fieldY - 18)}, 12, 1, Color{160, 175, 200, 255});
    DrawRectangle(g.fieldX, g.fieldY, g.fieldW, g.fieldH, addFriendFocus ? C(40, 60, 100) : C(25, 35, 60));
    DrawRectangleLines(g.fieldX, g.fieldY, g.fieldW, g.fieldH, addFriendFocus ? C(250, 200, 40) : C(70, 90, 130));
    std::string ft = addFriendBuf;
    if (addFriendFocus && fmodf((float)GetTime(), 1.0f) < 0.5f) ft += "|";
    if (ft.empty()) DrawTextEx(gFont, "username  (TAB to type, ENTER to send)", {(float)(g.fieldX + 8), (float)(g.fieldY + 7)}, 13, 1, Color{110, 125, 150, 255});
    else DrawTextEx(gFont, ft.c_str(), {(float)(g.fieldX + 8), (float)(g.fieldY + 6)}, 15, 1, WHITE);

    DrawTextEx(gFont, "W/S select   ENTER/A invite   L leave party   R refresh   F/ESC/Menu close",
               {(float)(g.px + 20), (float)(g.py + g.ph - 16)}, 11, 1, Color{150, 160, 180, 255});
}

void Game::DrawModePicker(int W, int H) {
    DrawRectangle(0, 0, W, H, Color{0, 0, 0, 180});
    int pw = 360, ph = 280;
    int px = (W - pw) / 2, py = (H - ph) / 2;
    DrawRectangle(px, py, pw, ph, Color{18, 28, 52, 255});
    DrawRectangleLines(px, py, pw, ph, Color{250, 200, 40, 255});
    DrawTextEx(gFont, "SELECT GAME MODE", {(float)(px + 20), (float)(py + 16)}, 20, 1, WHITE);

    const char* modes[] = { "SOLO", "DUO", "1V1 PRIVATE", "2V2 PRIVATE" };
    for (int i = 0; i < 4; i++) {
        int y = py + 55 + i * 36;
        bool sel = (int)gameMode == i;
        DrawRectangle(px + 20, y, pw - 40, 30, sel ? Color{250, 200, 30, 255} : Color{30, 45, 80, 255});
        DrawTextEx(gFont, modes[i], {(float)(px + 36), (float)(y + 6)}, 16, 1, sel ? BLACK : WHITE);
    }

    // Local / Online
    int y2 = py + 210;
    DrawTextEx(gFont, "PLAY TYPE", {(float)(px + 20), (float)y2}, 13, 1, Color{160, 175, 200, 255});
    bool loc = playType == PlayType::Local;
    DrawRectangle(px + 20, y2 + 20, 140, 28, loc ? Color{250, 200, 30, 255} : Color{30, 45, 80, 255});
    DrawTextEx(gFont, "LOCAL", {(float)(px + 60), (float)(y2 + 26)}, 15, 1, loc ? BLACK : WHITE);
    DrawRectangle(px + 180, y2 + 20, 140, 28, !loc ? Color{250, 200, 30, 255} : Color{30, 45, 80, 255});
    DrawTextEx(gFont, "ONLINE", {(float)(px + 215), (float)(y2 + 26)}, 15, 1, !loc ? BLACK : WHITE);

    DrawTextEx(gFont, "1-4 mode   L/O local/online   ENTER close", {(float)(px + 20), (float)(py + ph - 22)}, 12, 1, Color{150, 160, 180, 255});
}

Vector2 Game::LobbyMouse() const {
    // Lobby canvas covers the whole window; map window pixels to canvas coords.
    int W = LobbyW(), H = LobbyH();
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Vector2 m = GetMousePosition();
    return { m.x * (W / (float)sw), m.y * (H / (float)sh) };
}

void Game::BeginLobbyCanvas() {
    // Keep the lobby render target in step with the window's aspect ratio
    int lw = LobbyW(), lh = LobbyH();
    if (rtLobby.texture.width != lw || rtLobby.texture.height != lh) {
        if (rtLobby.id) UnloadRenderTexture(rtLobby);
        rtLobby = LoadRenderTexture(lw, lh);
        SetTextureFilter(rtLobby.texture, TEXTURE_FILTER_BILINEAR);
    }
    BeginTextureMode(rtLobby);
    ClearBackground(Color{10, 14, 28, 255});
}

void Game::EndLobbyCanvas() {
    EndTextureMode();
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    // Native full-window draw — no letterbox, no black bars.
    Rectangle dest = { 0, 0, (float)sw, (float)sh };
    Rectangle src = { 0, 0, (float)rtLobby.texture.width, -(float)rtLobby.texture.height };
    DrawTexturePro(rtLobby.texture, src, dest, {0, 0}, 0, WHITE);
}

int Game::HumansForMode() const {
    switch (gameMode) {
        case GameMode::Solo: return 1;
        case GameMode::Duo: return p2Joined ? 2 : 1;
        case GameMode::Private1v1: return 1; // local host + bots or second pad
        case GameMode::Private2v2: return p2Joined ? 2 : 1;
        default: return 1;
    }
}

void Game::UpdateLogin(float dt) {
    if (authMsgT > 0) authMsgT -= dt;
    static bool triedAuto = false;
    if (!triedAuto && loginUser[0] && loginPass[0] && !account.loggedIn) {
        triedAuto = true;
        TryLogin();
        if (account.loggedIn) return;
    }
    // text input
    int key = GetCharPressed();
    while (key > 0) {
        char* buf = (loginFocus == 0) ? loginUser : (loginFocus == 1) ? loginPass : loginDisplay;
        int len = (int)strlen(buf);
        if (key >= 32 && key < 127 && len < 30) { buf[len] = (char)key; buf[len+1] = 0; }
        key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE)) {
        char* buf = (loginFocus == 0) ? loginUser : (loginFocus == 1) ? loginPass : loginDisplay;
        int len = (int)strlen(buf);
        if (len > 0) buf[len-1] = 0;
    }
    if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_DOWN)) loginFocus = (loginFocus + 1) % (signupMode ? 3 : 2);
    if (IsKeyPressed(KEY_UP)) loginFocus = (loginFocus + (signupMode ? 2 : 1)) % (signupMode ? 3 : 2);
    if (IsKeyPressed(KEY_ENTER)) { if (signupMode) TrySignup(); else TryLogin(); }
    if (IsKeyPressed(KEY_F1)) signupMode = !signupMode;
    if (IsKeyPressed(KEY_ESCAPE)) quit = true;
}

void Game::DrawLogin() {
    BeginDrawing();
    ClearBackground(C(8, 12, 28));
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    // centred card
    int cw = 360, ch = signupMode ? 320 : 280;
    int cx = (sw - cw) / 2, cy = (sh - ch) / 2;
    DrawRectangle(cx, cy, cw, ch, C(18, 24, 48));
    DrawRectangleLines(cx, cy, cw, ch, C(80, 120, 200));
    DrawTextEx(gFont, "FORTNITE", {(float)(cx + 20), (float)(cy + 16)}, 36, 1, WHITE);
    DrawTextEx(gFont, "PSX Edition  -  Season 1", {(float)(cx + 22), (float)(cy + 52)}, 14, 1, C(160, 180, 220));

    auto field = [&](const char* label, char* buf, int y, int focusId, bool pass) {
        DrawTextEx(gFont, label, {(float)(cx + 30), (float)y}, 14, 1, C(180, 190, 210));
        Color bg = loginFocus == focusId ? C(40, 60, 100) : C(25, 35, 60);
        DrawRectangle(cx + 30, y + 18, cw - 60, 28, bg);
        DrawRectangleLines(cx + 30, y + 18, cw - 60, 28, loginFocus == focusId ? C(250, 200, 40) : C(70, 90, 130));
        std::string shown = pass ? std::string(strlen(buf), '*') : buf;
        DrawTextEx(gFont, shown.c_str(), {(float)(cx + 38), (float)(y + 22)}, 16, 1, WHITE);
    };
    field("USERNAME", loginUser, cy + 90, 0, false);
    field("PASSWORD", loginPass, cy + 150, 1, true);
    if (signupMode) field("DISPLAY NAME", loginDisplay, cy + 210, 2, false);

    int by = cy + (signupMode ? 270 : 220);
    DrawRectangle(cx + 30, by, 140, 32, C(250, 200, 30));
    DrawTextEx(gFont, signupMode ? "CREATE" : "LOGIN", {(float)(cx + 60), (float)(by + 6)}, 18, 1, BLACK);
    DrawTextEx(gFont, signupMode ? "F1: Login instead" : "F1: Create account", {(float)(cx + 180), (float)(by + 8)}, 12, 1, C(160, 170, 200));

    if (authMsgT > 0 && !authError.empty()) {
        DrawTextEx(gFont, authError.c_str(), {(float)(cx + 30), (float)(cy + ch - 22)}, 13, 1, C(255, 100, 100));
    }
    DrawTextEx(gFont, "Default: Player1 / password   |   Backend: localhost:3847", {(float)(cx), (float)(cy + ch + 10)}, 12, 1, C(100, 110, 140));
    EndDrawing();
}

// =====================================================================
//  LOBBY TABS
// =====================================================================
void Game::UpdateLobby(float dt) {
    if (statusMsgT > 0) statusMsgT -= dt;

    // Matchmaking timer (min wait then connect)
    if (matchmaking) {
        matchmakeT += dt;
        if (matchmakeT >= matchmakeMinWait) {
            if (ConnectMatchmaker()) {
                matchmaking = false;
                matchmakeT = 0;
                StartMatch();
            } else {
                matchmakeT = matchmakeMinWait - 0.5f;
            }
        }
    }

    for (size_t i = 0; i < notifications.size(); ) {
        notifications[i].life -= dt;
        if (notifications[i].life <= 0) notifications.erase(notifications.begin() + (int)i);
        else ++i;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (friendsOpen) { if (addFriendFocus) addFriendFocus = false; else friendsOpen = false; }
        else if (modePickerOpen) modePickerOpen = false;
        else if (lockerBrowsing) lockerBrowsing = false;
        else if (matchmaking) { matchmaking = false; matchmakeT = 0; }
    }

    auto padPressed = [](int btn) {
        for (int g = 0; g < 4; g++)
            if (IsGamepadAvailable(g) && IsGamepadButtonPressed(g, btn)) return true;
        return false;
    };

    // Tabs: controller LB/RB
    if (padPressed(GAMEPAD_BUTTON_LEFT_TRIGGER_1))
        tab = (LobbyTab)(((int)tab + (int)LobbyTab::COUNT - 1) % (int)LobbyTab::COUNT);
    if (padPressed(GAMEPAD_BUTTON_RIGHT_TRIGGER_1))
        tab = (LobbyTab)(((int)tab + 1) % (int)LobbyTab::COUNT);

    // Left stick digital
    static int stickDir = 0;
    float sx = 0, sy = 0;
    for (int gpad = 0; gpad < 4; gpad++) {
        if (!IsGamepadAvailable(gpad)) continue;
        sx = GetGamepadAxisMovement(gpad, GAMEPAD_AXIS_LEFT_X);
        sy = GetGamepadAxisMovement(gpad, GAMEPAD_AXIS_LEFT_Y);
        break;
    }
    int dir = 0;
    if (sx < -0.55f) dir = 1;
    else if (sx > 0.55f) dir = 2;
    else if (sy < -0.55f) dir = 3;
    else if (sy > 0.55f) dir = 4;
    bool stickLeft = (dir == 1 && stickDir != 1);
    bool stickRight = (dir == 2 && stickDir != 2);
    bool stickUp = (dir == 3 && stickDir != 3);
    bool stickDown = (dir == 4 && stickDir != 4);
    stickDir = dir;
    (void)stickUp; (void)stickDown;

    // Friends panel: F / controller Menu button opens a friends list in the centre of the screen
    {
        const int fw = LobbyW(), fh = LobbyH();
        if (!friendsOpen) {
            if (!modePickerOpen && (IsKeyPressed(KEY_F) || padPressed(GAMEPAD_BUTTON_MIDDLE_RIGHT))) {
                friendsOpen = true; friendSel = 0; addFriendFocus = false;
                while (GetCharPressed() > 0) {}
                RefreshFriends();
            }
        } else {
            const FriendsGeo g = FriendsLayout(fw, fh);
            int nReq = (int)incomingReqs.size(), nFr = (int)friends.size(), total = 1 + nReq + nFr;
            if (friendSel >= total) friendSel = total - 1;
            if (friendSel < 0) friendSel = 0;

            auto activate = [&](int idx) {
                if (idx == 0) {
                    if (inParty) { statusMsg = "Leave your party first (L)"; statusMsgT = 2; }
                    else p2Joined = !p2Joined;
                } else if (idx - 1 < nReq) {
                    FriendRequest r = incomingReqs[idx - 1];
                    RespondFriendRequest(r, true);
                } else {
                    FriendEntry f = friends[idx - 1 - nReq];
                    JoinParty(f);
                    if (inParty) friendsOpen = false;
                }
            };

            if (addFriendFocus) {
                int key = GetCharPressed();
                while (key > 0) {
                    int len = (int)strlen(addFriendBuf);
                    if (key >= 33 && key < 127 && len < 30) { addFriendBuf[len] = (char)key; addFriendBuf[len + 1] = 0; }
                    key = GetCharPressed();
                }
                if (IsKeyPressed(KEY_BACKSPACE)) { int len = (int)strlen(addFriendBuf); if (len > 0) addFriendBuf[len - 1] = 0; }
                if (IsKeyPressed(KEY_ENTER)) { SendFriendRequest(); addFriendFocus = false; }
            } else {
                while (GetCharPressed() > 0) {}
                if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W) || padPressed(GAMEPAD_BUTTON_LEFT_FACE_UP) || stickUp)
                    friendSel = (friendSel + total - 1) % total;
                if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) || padPressed(GAMEPAD_BUTTON_LEFT_FACE_DOWN) || stickDown)
                    friendSel = (friendSel + 1) % total;
                if (IsKeyPressed(KEY_ENTER) || padPressed(GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) activate(friendSel);
                if ((IsKeyPressed(KEY_X) || IsKeyPressed(KEY_DELETE) || padPressed(GAMEPAD_BUTTON_RIGHT_FACE_LEFT))
                    && friendSel >= 1 && friendSel - 1 < nReq) {
                    FriendRequest r = incomingReqs[friendSel - 1];
                    RespondFriendRequest(r, false);
                }
                if (IsKeyPressed(KEY_L) && inParty) LeaveParty();
                if (IsKeyPressed(KEY_R)) RefreshFriends();
                if (IsKeyPressed(KEY_TAB)) addFriendFocus = true;
                if (IsKeyPressed(KEY_F) || padPressed(GAMEPAD_BUTTON_MIDDLE_RIGHT) || padPressed(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))
                    friendsOpen = false;
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                Vector2 m = LobbyMouse();
                bool inPanel = m.x >= g.px && m.x <= g.px + g.pw && m.y >= g.py && m.y <= g.py + g.ph;
                if (!inPanel) { friendsOpen = false; addFriendFocus = false; }
                else if (m.x >= g.fieldX && m.x <= g.fieldX + g.fieldW && m.y >= g.fieldY && m.y <= g.fieldY + g.fieldH) addFriendFocus = true;
                else {
                    addFriendFocus = false;
                    if (m.x >= g.px + 20 && m.x <= g.px + g.pw - 20 && m.y >= g.listY && m.y < g.listY + g.visible * g.rowH) {
                        int first = friendSel < g.visible ? 0 : friendSel - g.visible + 1;
                        int idx = first + (int)((m.y - g.listY) / g.rowH);
                        if (idx >= 0 && idx < total) { friendSel = idx; activate(idx); }
                    }
                }
            }
            return;   // panel swallows all other lobby input
        }
    }

    if (tab == LobbyTab::Play) {
        if (modePickerOpen) {
            if (IsKeyPressed(KEY_ONE) || stickUp) gameMode = GameMode::Solo;
            if (IsKeyPressed(KEY_TWO)) gameMode = GameMode::Duo;
            if (IsKeyPressed(KEY_THREE)) gameMode = GameMode::Private1v1;
            if (IsKeyPressed(KEY_FOUR)) gameMode = GameMode::Private2v2;
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
                gameMode = (GameMode)(((int)gameMode + (int)GameMode::COUNT - 1) % (int)GameMode::COUNT);
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) || stickDown)
                gameMode = (GameMode)(((int)gameMode + 1) % (int)GameMode::COUNT);
            if (IsKeyPressed(KEY_L)) playType = PlayType::Local;
            if (IsKeyPressed(KEY_O)) playType = PlayType::Online;
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE) || padPressed(GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
                modePickerOpen = false;
        } else {
            if (IsKeyPressed(KEY_TAB)) map = (MapId)((map + 1) % MAP_COUNT);
            if ((!inParty || isPartyLeader) && (IsKeyPressed(KEY_M) || padPressed(GAMEPAD_BUTTON_LEFT_FACE_UP) || padPressed(GAMEPAD_BUTTON_RIGHT_FACE_LEFT)))
                modePickerOpen = true;
            if (!inParty && IsKeyPressed(KEY_J))
                p2Joined = !p2Joined;

            Vector2 m = LobbyMouse();
            int W = LobbyW(), H = LobbyH();
            int bx = W - 170, by = H - 88;
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (m.x >= bx && m.x <= bx + 140 && m.y >= by - 30 && m.y <= by - 4) {
                    if (!inParty || isPartyLeader) modePickerOpen = true;
                }
                if (m.x >= bx && m.x <= bx + 140 && m.y >= by && m.y <= by + 42) {
                    if (matchmaking) {
                        matchmaking = false; matchmakeT = 0;
                        statusMsg = "Matchmaking cancelled"; statusMsgT = 1.5f;
                    } else {
                        matchmaking = true; matchmakeT = 0;
                        statusMsg = "Searching..."; statusMsgT = 1.5f;
                    }
                }
                if (modePickerOpen) {
                    int pw = 360, ph = 280;
                    int px = (W - pw) / 2, py = (H - ph) / 2;
                    for (int i = 0; i < 4; i++) {
                        int y = py + 55 + i * 36;
                        if (m.x >= px + 20 && m.x <= px + pw - 20 && m.y >= y && m.y <= y + 30)
                            gameMode = (GameMode)i;
                    }
                    int y2 = py + 230;
                    if (m.x >= px + 20 && m.x <= px + 160 && m.y >= y2 && m.y <= y2 + 28) playType = PlayType::Local;
                    if (m.x >= px + 180 && m.x <= px + 320 && m.y >= y2 && m.y <= y2 + 28) playType = PlayType::Online;
                }
            }

            if (!modePickerOpen && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_P) || padPressed(GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || padPressed(GAMEPAD_BUTTON_RIGHT_FACE_UP))) {
                if (matchmaking) {
                    matchmaking = false; matchmakeT = 0;
                } else if (inParty && !isPartyLeader) {
                    statusMsg = "Only the party leader can start"; statusMsgT = 2;
                } else {
                    matchmaking = true; matchmakeT = 0;
                }
            }
        }
    } else if (tab == LobbyTab::ItemShop) {
        int total = (int)shopFeatured.size() + (int)shopDaily.size();
        if (total > 0) {
            if (IsKeyPressed(KEY_LEFT) || stickLeft || padPressed(GAMEPAD_BUTTON_LEFT_FACE_LEFT))
                shopSel = (shopSel + total - 1) % total;
            if (IsKeyPressed(KEY_RIGHT) || stickRight || padPressed(GAMEPAD_BUTTON_LEFT_FACE_RIGHT))
                shopSel = (shopSel + 1) % total;
            if (IsKeyPressed(KEY_ENTER) || padPressed(GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
                ShopItem* it = nullptr;
                if (shopSel < (int)shopFeatured.size()) it = &shopFeatured[shopSel];
                else it = &shopDaily[shopSel - (int)shopFeatured.size()];
                if (it && account.vbucks >= it->price) {
                    char body[256];
                    snprintf(body, sizeof body, "{\"userId\":\"%s\",\"itemId\":\"%s\"}", account.id.c_str(), it->id.c_str());
                    int st = 0;
                    std::string resp = HttpPost("/api/shop/buy", body, &st);
                    if (st == 200) {
                        account.vbucks = JsonGetInt(JsonGetObject(resp, "user"), "vbucks", account.vbucks - it->price);
                        statusMsg = std::string("Purchased ") + it->name; statusMsgT = 2.5f;
                    } else {
                        account.vbucks -= it->price;
                        statusMsg = std::string("Purchased ") + it->name + " (offline)"; statusMsgT = 2.5f;
                    }
                } else { statusMsg = "Not enough V-Bucks"; statusMsgT = 2; }
            }
        }
    } else if (tab == LobbyTab::Locker) {
        if (!lockerBrowsing) {
            if (IsKeyPressed(KEY_LEFT) || stickLeft || padPressed(GAMEPAD_BUTTON_LEFT_FACE_LEFT))
                lockerCat = (lockerCat + 4) % 5;
            if (IsKeyPressed(KEY_RIGHT) || stickRight || padPressed(GAMEPAD_BUTTON_LEFT_FACE_RIGHT))
                lockerCat = (lockerCat + 1) % 5;
            if (IsKeyPressed(KEY_ENTER) || padPressed(GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
                lockerBrowsing = true;
                lockerSel = (lockerCat == 0) ? p1Look : 0;
            }
        } else {
            int count = (lockerCat == 0) ? kLookCount : 3;
            const int COLS = 6;                                   // 6 items per row
            if (IsKeyPressed(KEY_LEFT) || stickLeft || padPressed(GAMEPAD_BUTTON_LEFT_FACE_LEFT))
                lockerSel = (lockerSel + count - 1) % count;
            if (IsKeyPressed(KEY_RIGHT) || stickRight || padPressed(GAMEPAD_BUTTON_LEFT_FACE_RIGHT))
                lockerSel = (lockerSel + 1) % count;
            if (IsKeyPressed(KEY_UP) || padPressed(GAMEPAD_BUTTON_LEFT_FACE_UP))
                lockerSel = (lockerSel - COLS + count) % count;
            if (IsKeyPressed(KEY_DOWN) || padPressed(GAMEPAD_BUTTON_LEFT_FACE_DOWN))
                lockerSel = (lockerSel + COLS) % count;
            if (IsKeyPressed(KEY_ESCAPE) || padPressed(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))
                lockerBrowsing = false;
            if (IsKeyPressed(KEY_ENTER) || padPressed(GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
                if (lockerCat == 0) {
                    p1Look = lockerSel;
                    const char* names[] = {"jonesy","ramirez","headhunter","wildcat", "arial_assault_trooper", "renegade_raider", "ghoul_trooper", "skull_trooper", "brite_bomber", "punk_funk", "ginge", "merry_maruader"};
                    account.outfit = names[lockerSel];
                    statusMsg = std::string("Equipped ") + kLooks[lockerSel].name; statusMsgT = 2;
                } else if (lockerCat == 1) {
                    equippedGlider = lockerSel;
                    static const char* n[] = {"Default Glider","Mako","Founder's Glider"};
                    statusMsg = std::string("Equipped ") + n[lockerSel]; statusMsgT = 2;
                } else if (lockerCat == 2) {
                    equippedPick = lockerSel;
                    static const char* n[] = {"Default Pickaxe","Raider's Revenge","Candy Axe"};
                    statusMsg = std::string("Equipped ") + n[lockerSel]; statusMsgT = 2;
                } else if (lockerCat == 3) {
                    equippedEmote = lockerSel;
                    static const char* n[] = {"Wave","Default Dance","Ride The Pony"};
                    statusMsg = std::string("Equipped ") + n[lockerSel]; statusMsgT = 2;
                } else {
                    equippedBanner = lockerSel;
                    static const char* n[] = {"Default","Victory Trophy","Season 1"};
                    static const char* ids[] = {"Banner_Default","Banner_Trophy","Banner_Season1"};
                    account.banner = ids[lockerSel];
                    statusMsg = std::string("Equipped banner ") + n[lockerSel]; statusMsgT = 2;
                }
            }
        }
    } else if (tab == LobbyTab::BattlePass) {
        int n = (int)bpTiers.size();
        if (n > 0) {
            if (IsKeyPressed(KEY_LEFT) || stickLeft || padPressed(GAMEPAD_BUTTON_LEFT_FACE_LEFT)) bpSelTier = (bpSelTier + n - 1) % n;
            if (IsKeyPressed(KEY_RIGHT) || stickRight || padPressed(GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) bpSelTier = (bpSelTier + 1) % n;
            if (IsKeyPressed(KEY_UP) || padPressed(GAMEPAD_BUTTON_LEFT_FACE_UP)) bpSelRow = 0;
            if (IsKeyPressed(KEY_DOWN) || padPressed(GAMEPAD_BUTTON_LEFT_FACE_DOWN)) {
                if (bpTiers[bpSelTier].free.valid) bpSelRow = 1;
            }
            if (bpSelRow == 1 && !bpTiers[bpSelTier].free.valid) bpSelRow = 0;
        }
    } else if (tab == LobbyTab::Profile) {
        if (IsKeyPressed(KEY_L) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            bool doLogout = IsKeyPressed(KEY_L);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                Vector2 m = LobbyMouse();
                int H = LobbyH();
                if (m.x >= 24 && m.x <= 184 && m.y >= H - 70 && m.y <= H - 34) doLogout = true;
            }
            if (doLogout) {
                SetPresence("offline", "none", false);
                account = Account{};
                account.loggedIn = false;
                inParty = false; p2Joined = false;
                state = State::Login; friendsOpen = false;
                statusMsg = "Logged out"; statusMsgT = 2;
            }
        }
    }
}

void Game::DrawLobbyNav(int W, int H) {
    const int barH = 40;
    DrawRectangle(0, 0, W, barH, Color{16, 20, 34, 250});
    DrawRectangle(0, barH - 1, W, 1, Color{45, 55, 85, 255});

    float total = 0;
    float gaps = 28.0f;
    for (int i = 0; i < (int)LobbyTab::COUNT; i++)
        total += MeasureTextEx(gFont, kTabNames[i], 18, 1).x;
    total += gaps * ((int)LobbyTab::COUNT - 1);

    float x = (W - total) * 0.5f;
        Vector2 mouse = GetMousePosition();
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    float mx = mouse.x * (W / (float)sw);
    float my = mouse.y * (H / (float)sh);

    for (int i = 0; i < (int)LobbyTab::COUNT; i++) {
        const char* name = kTabNames[i];
        float tw = MeasureTextEx(gFont, name, 18, 1).x;
        bool active = (int)tab == i;
        Rectangle hit = { x - 10, 7, tw + 20, (float)barH - 14 };
        bool hover = mx >= hit.x && mx <= hit.x + hit.width && my >= hit.y && my <= hit.y + hit.height;
        if (hover && !friendsOpen && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            tab = (LobbyTab)i;
        if (active) {
            DrawRectangle((int)x - 10, 7, (int)tw + 20, barH - 14, Color{250, 200, 30, 255});
            DrawTextEx(gFont, name, {x, 11}, 18, 1, BLACK);
        } else {
            DrawTextEx(gFont, name, {x, 11}, 18, 1, hover ? WHITE : Color{185, 195, 215, 255});
        }
        x += tw + gaps;
    }

    // V-Bucks chip top-right (always visible)
    char vb[32]; snprintf(vb, sizeof vb, "%d", account.vbucks);
    float vbw = MeasureTextEx(gFont, vb, 16, 1).x;
    int chipW = (int)vbw + 40;
    int chipX = W - chipW - 10;
    int chipY = 6;
    DrawRectangle(chipX, chipY, chipW, 28, Color{12, 18, 36, 230});
    DrawRectangleLines(chipX, chipY, chipW, 28, Color{60, 140, 200, 255});
    const Texture2D* vbIcon = gIcons.Cosmetic("VBucks");
    if (!vbIcon || !vbIcon->id) vbIcon = gIcons.Cosmetic("vbucks");
    if (!vbIcon || !vbIcon->id) vbIcon = gIcons.UIItem("vbucks");
    if (!vbIcon || !vbIcon->id) vbIcon = gIcons.Item("vbucks");
    if (vbIcon && vbIcon->id)
        DrawTexturePro(*vbIcon, {0,0,(float)vbIcon->width,(float)vbIcon->height},
            {(float)(chipX + 6), 10, 18, 18}, {0,0}, 0, WHITE);
    else {
        // fallback coin
        DrawCircle(chipX + 15, chipY + 14, 8, Color{40, 160, 255, 255});
        DrawCircleLines(chipX + 15, chipY + 14, 8, Color{180, 230, 255, 255});
        DrawTextEx(gFont, "V", {(float)(chipX + 11), (float)(chipY + 7)}, 12, 1, WHITE);
    }
    DrawTextEx(gFont, vb, {(float)(chipX + 28), (float)(chipY + 6)}, 16, 1, Color{120, 210, 255, 255});
}


void Game::DrawProgressionWidget(int W, int H) {
    // Season 1 level widget (bottom-left style reference)
    int x = 20, y = 96;   // sits under the LOCAL/ONLINE + "F / Menu: Friends" text
    int ww = 220, hh = 78;
    DrawRectangle(x, y, ww, hh, Color{12, 28, 55, 235});
    DrawRectangleLines(x, y, ww, hh, Color{50, 90, 150, 255});
    DrawTextEx(gFont, "SEASON 1", {(float)(x + 12), (float)(y + 8)}, 14, 1, Color{140, 180, 230, 255});
    // banner icon
    const Texture2D* ban = gIcons.Cosmetic(account.banner.empty() ? "Banner_Default" : account.banner.c_str());
    DrawRectangle(x + 12, y + 28, 36, 36, Color{8, 12, 24, 255});
    DrawRectangleLines(x + 12, y + 28, 36, 36, Color{80, 140, 220, 255});
    if (ban && ban->id)
        DrawTexturePro(*ban, {0,0,(float)ban->width,(float)ban->height},
            {(float)(x+14),(float)(y+30),32,32}, {0,0}, 0, WHITE);
    else {
        // chevron fallback like reference
        DrawTriangle({(float)(x+30),(float)(y+34)}, {(float)(x+20),(float)(y+54)},
                     {(float)(x+40),(float)(y+54)}, Color{200, 220, 255, 255});
    }
    DrawTextEx(gFont, TextFormat("LEVEL %d", account.level), {(float)(x + 56), (float)(y + 30)}, 18, 1, WHITE);
    // XP bar
    int xpNeed = 100;
    int xp = account.xp % xpNeed;
    float pct = (float)xp / (float)xpNeed;
    DrawRectangle(x + 56, y + 54, 150, 8, Color{30, 40, 60, 255});
    DrawRectangle(x + 56, y + 54, (int)(150 * pct), 8, Color{100, 180, 255, 255});
    DrawTextEx(gFont, TextFormat("%d / %d", xp, xpNeed), {(float)(x + 56), (float)(y + 64)}, 12, 1, Color{160, 190, 230, 255});
}

void Game::DrawTabPlay(int W, int H) {
    float t = (float)GetTime();
    DrawLobbyBackground(W, H, t);

    // 3D characters — camera further back for progression UI space
    Camera3D cam = {};
    cam.position = {0, 1.35f, 9.2f};
    cam.target = {0, 1.05f, 0};
    cam.up = {0, 1, 0};
    cam.fovy = 30;
    cam.projection = CAMERA_PERSPECTIVE;
    gR.SetResolution((float)W, (float)H);
    BeginMode3D(cam);
    float x1 = -1.55f, x2 = 1.55f;
    // Solid grey floor across the whole bottom of the screen; the animated background shows above the horizon
    DrawPlane({0, -0.01f, -20.0f}, {200.0f, 120.0f}, Color{96, 99, 107, 255});
    DrawModelEx(padOn, {x1, 0, 0}, {0, 1, 0}, 0, {1.1f, 1.1f, 1.1f}, WHITE);
    DrawModelEx((p2Joined || inParty) ? padOn : padOff, {x2, 0, 0}, {0, 1, 0}, 0, {1.1f, 1.1f, 1.1f}, WHITE);
    Pose p1; p1.idle = t;
    CharacterDraw(chars[p1Look], held, {x1, 0.14f, 0}, sinf(t * 0.8f) * 0.25f, p1, WHITE);
    Pose p2; p2.idle = t + 1;
    bool showP2 = p2Joined || inParty;
    CharacterDraw(chars[p2Look], held, {x2, 0.14f, 0}, sinf(t * 0.8f + 1) * 0.25f, p2, showP2 ? WHITE : Color{8, 8, 12, 255});
    EndMode3D();

    Vector2 l1 = GetWorldToScreenEx({x1, 2.35f, 0}, cam, W, H);
    Vector2 l2 = GetWorldToScreenEx({x2, 2.35f, 0}, cam, W, H);
    const char* n1 = account.displayName.c_str();
    DrawTextEx(gFont, n1, {l1.x - MeasureTextEx(gFont, n1, 18, 1).x / 2, l1.y}, 18, 1, WHITE);
    const char* n2 = inParty ? partyMemberName.c_str() : (p2Joined ? "Player 2" : "Invite");
    DrawTextEx(gFont, n2, {l2.x - MeasureTextEx(gFont, n2, 18, 1).x / 2, l2.y}, 18, 1, showP2 ? WHITE : C(160, 160, 180));

    DrawProgressionWidget(W, H);

    // Mode label above PLAY (clickable)
    const char* modeLabel = kModeNames[(int)gameMode];
    float mw = MeasureTextEx(gFont, modeLabel, 16, 1).x;
    int bx = W - 170, by = H - 88;
    int modeX = bx + (140 - (int)mw) / 2;
    int modeY = by - 28;
    DrawTextEx(gFont, modeLabel, {(float)modeX, (float)modeY}, 16, 1, Color{250, 200, 40, 255});

    // PLAY / CANCEL button
    Color btnCol = matchmaking ? Color{180, 60, 50, 255} : Color{250, 200, 30, 255};
    DrawRectangle(bx, by, 140, 42, btnCol);
    DrawRectangleLines(bx, by, 140, 42, Color{255, 240, 150, 255});
    const char* btnTxt = matchmaking ? "CANCEL" : "PLAY";
    float btw = MeasureTextEx(gFont, btnTxt, 22, 1).x;
    DrawTextEx(gFont, btnTxt, {(float)(bx + (140 - btw) / 2), (float)(by + 8)}, 22, 1, matchmaking ? WHITE : BLACK);
    if (matchmaking) {
        DrawTextEx(gFont, "waiting for server", {(float)(bx + 8), (float)(by + 46)}, 12, 1, Color{180, 200, 230, 255});
    }
    // Controller hints
    DrawTextEx(gFont, "Y  Ready", {(float)bx, (float)(by + 62)}, 12, 1, Color{250, 200, 40, 255});
    DrawTextEx(gFont, "X  Game mode", {(float)(bx + 62), (float)(by + 62)}, 12, 1, Color{160, 190, 230, 255});

    DrawTextEx(gFont, playType == PlayType::Online ? "ONLINE" : "LOCAL", {20, 52}, 14, 1, Color{160, 190, 230, 255});
    DrawTextEx(gFont, "F / Menu: Friends", {20, 72}, 12, 1, Color{130, 150, 185, 255});

    if (modePickerOpen) DrawModePicker(W, H);
}

static const char* kLookCid[12] = {
    "CID_010_Athena_Commando_M_Default",
    "CID_001_Athena_Commando_F_Default",
    "CID_016_Athena_Commando_F",
    "CID_013_Athena_Commando_F",
    "CID_017_Athena_Commando_M",
    "CID_028_Athena_Commando_F",
    "CID_029_Athena_Commando_F_Halloween",
    "CID_030_Athena_Commando_M_Halloween",
    "CID_038_Athena_Commando_M_Disco",
    "CID_044_Athena_Commando_F_SciPop",
    "CID_048_Athena_Commando_F_HolidayGingerbread",
    "CID_049_Athena_Commando_M_HolidayGingerbread"
};

static const Texture2D* LockerIcon(const char* cid) {
    if (!cid || !cid[0]) return nullptr;
    return gIcons.Cosmetic(cid);
}


static void DrawBpTile(int x, int y, int s, int tierNum, const BpReward& r, bool unlocked, bool selected, bool isPaidRow) {
    Color bg = Color{22, 28, 48, 255};
    DrawRectangle(x, y, s, s, bg);
    Color border = selected ? Color{250, 200, 30, 255} : Color{55, 70, 110, 255};
    DrawRectangleLines(x, y, s, s, border);
    if (selected) DrawRectangleLines(x + 1, y + 1, s - 2, s - 2, border);

    if (isPaidRow) {
        char tn[8]; snprintf(tn, sizeof tn, "%d", tierNum);
        int tw = (int)MeasureTextEx(gFont, tn, 14, 1).x;
        DrawRectangle(x + (s - tw) / 2 - 6, y - 18, tw + 12, 16, selected ? Color{250, 200, 30, 255} : Color{40, 50, 80, 255});
        DrawTextEx(gFont, tn, {(float)(x + (s - tw) / 2), (float)(y - 16)}, 14, 1, selected ? BLACK : WHITE);
    }

    if (!r.valid) return;

    if (unlocked) {
        const char* cid = !r.cid.empty() ? r.cid.c_str() : r.image.c_str();
        const Texture2D* tex = (cid && cid[0]) ? gIcons.Cosmetic(cid) : nullptr;
        if (tex && tex->id) {
            DrawTexturePro(*tex, {0,0,(float)tex->width,(float)tex->height},
                {(float)x+4,(float)y+4,(float)s-8,(float)s-8}, {0,0}, 0, WHITE);
        } else {
            Color ic = r.rarity == "rare" ? Color{60, 150, 255, 255} :
                       r.rarity == "uncommon" ? Color{90, 200, 60, 255} : Color{180, 180, 190, 255};
            DrawRectangle(x + s/4, y + s/4, s/2, s/2, ic);
        }
        if (r.type == "Currency" || r.cid == "VBucks" || r.image == "vbucks" || r.image == "VBucks") {
            char am[16]; snprintf(am, sizeof am, "%d", r.amount > 0 ? r.amount : 50);
            float aw = MeasureTextEx(gFont, am, 12, 1).x;
            DrawTextEx(gFont, am, {(float)(x + s/2 - aw/2), (float)(y + s - 16)}, 12, 1, WHITE);
        }
        if (selected) {
            DrawRectangle(x + 4, y + 4, 52, 16, Color{250, 200, 30, 230});
            DrawTextEx(gFont, "OWNED", {(float)(x + 8), (float)(y + 5)}, 12, 1, BLACK);
        }
    } else {
        DrawRectangle(x, y, s, s, Color{12, 16, 28, 200});
        DrawRectangleLines(x, y, s, s, Color{40, 50, 80, 255});
        int cx = x + s/2, cy = y + s/2;
        DrawRectangle(cx - 8, cy - 2, 16, 14, Color{160, 170, 190, 255});
        DrawCircleLines(cx, cy - 6, 7, Color{160, 170, 190, 255});
        DrawCircleLines(cx, cy - 6, 6, Color{160, 170, 190, 255});
    }
}

void Game::DrawTabBattlePass(int W, int H) {
    DrawRectangle(0, 40, W, H - 40, Color{14, 18, 32, 255});

    DrawTextEx(gFont, bpTitle.c_str(), {24, 52}, 24, 1, WHITE);
    DrawTextEx(gFont, bpSubtitle.c_str(), {24, 80}, 13, 1, Color{230, 200, 90, 255});

    char tierInfo[64];
    snprintf(tierInfo, sizeof tierInfo, "TIER %d", account.battlePassTier);
    float tiw = MeasureTextEx(gFont, tierInfo, 20, 1).x;
    DrawTextEx(gFont, tierInfo, {(float)(W - 24 - tiw), 52}, 20, 1, WHITE);
    char lvlInfo[64];
    snprintf(lvlInfo, sizeof lvlInfo, "%d / 10   LEVEL %d", account.battleStars % 10, account.level);
    float liw = MeasureTextEx(gFont, lvlInfo, 13, 1).x;
    DrawTextEx(gFont, lvlInfo, {(float)(W - 24 - liw), 76}, 13, 1, Color{180, 190, 210, 255});

    const int tile = 64;
    const int gap = 8;
    const int startX = 90;
    const int paidY = 120;
    const int freeY = paidY + tile + 24;

    DrawTextEx(gFont, "BATTLE", {16, (float)(paidY + tile/2 - 14)}, 11, 1, Color{160, 170, 200, 255});
    DrawTextEx(gFont, "PASS", {16, (float)(paidY + tile/2)}, 11, 1, Color{160, 170, 200, 255});
    DrawTextEx(gFont, "FREE", {22, (float)(freeY + tile/2 - 6)}, 12, 1, Color{160, 170, 200, 255});

    int n = (int)bpTiers.size();
    for (int i = 0; i < n; i++) {
        int x = startX + i * (tile + gap);
        bool unlocked = (bpTiers[i].tier <= account.battlePassTier);
        bool selPaid = (bpSelTier == i && bpSelRow == 0);
        bool selFree = (bpSelTier == i && bpSelRow == 1);
        DrawBpTile(x, paidY, tile, bpTiers[i].tier, bpTiers[i].paid, unlocked, selPaid, true);
        if (bpTiers[i].free.valid)
            DrawBpTile(x, freeY, tile, bpTiers[i].tier, bpTiers[i].free, unlocked, selFree, false);
        else {
            DrawRectangle(x, freeY, tile, tile, Color{18, 22, 38, 255});
            DrawRectangleLines(x, freeY, tile, tile, Color{35, 45, 70, 255});
        }
    }

    const BpReward* sel = nullptr;
    bool selUnlocked = false;
    if (n > 0 && bpSelTier >= 0 && bpSelTier < n) {
        sel = (bpSelRow == 0) ? &bpTiers[bpSelTier].paid : &bpTiers[bpSelTier].free;
        selUnlocked = bpTiers[bpSelTier].tier <= account.battlePassTier;
        if (sel && !sel->valid) sel = &bpTiers[bpSelTier].paid;
    }

    int detailY = freeY + tile + 20;
    if (sel && sel->valid) {
        DrawTextEx(gFont, sel->name.c_str(), {24, (float)detailY}, 20, 1, WHITE);
        char rarLine[64];
        snprintf(rarLine, sizeof rarLine, "%s %s", sel->rarity.c_str(), sel->type.c_str());
        Color rc = sel->rarity == "rare" ? Color{80, 160, 255, 255} :
                   sel->rarity == "uncommon" ? Color{100, 210, 80, 255} : Color{180, 180, 190, 255};
        DrawTextEx(gFont, rarLine, {24, (float)(detailY + 24)}, 13, 1, rc);
        DrawTextEx(gFont, sel->desc.c_str(), {24, (float)(detailY + 42)}, 12, 1, Color{170, 180, 200, 255});

        const char* status = selUnlocked ? "UNLOCKED" : "LOCKED";
        Color sc = selUnlocked ? Color{100, 220, 120, 255} : Color{200, 100, 100, 255};
        float sw = MeasureTextEx(gFont, status, 14, 1).x;
        DrawTextEx(gFont, status, {(float)(W - 24 - sw), (float)(detailY + 6)}, 14, 1, sc);
    }

    DrawTextEx(gFont, "A/D tiers   W/S rows", {24, (float)(H - 28)}, 12, 1, Color{160, 170, 190, 255});
}

void Game::DrawTabLocker(int W, int H) {
    float t = (float)GetTime();
    Camera3D cam = {};
    cam.position = {0.35f, 1.2f, 4.5f};
    cam.target = {0, 0.95f, 0};
    cam.up = {0, 1, 0};
    cam.fovy = 32;
    cam.projection = CAMERA_PERSPECTIVE;
    DrawRectangle(0, 40, W, H - 40, Color{18, 48, 105, 255});
    BeginMode3D(cam);
    DrawModelEx(padOn, {0, 0, 0}, {0, 1, 0}, 0, {1, 1, 1}, WHITE);
    Pose pose; pose.idle = t;
    CharacterDraw(chars[p1Look], held, {0, 0.14f, 0}, sinf(t * 0.7f) * 0.2f, pose, WHITE);
    EndMode3D();

    const char* cats[] = { "OUTFIT", "GLIDER", "PICKAXE", "EMOTE", "BANNER" };
    static const char* outfitCid[] = {
        "CID_010_Athena_Commando_M_Default",
        "CID_001_Athena_Commando_F_Default",
        "CID_016_Athena_Commando_F",
        "CID_013_Athena_Commando_F",
        "CID_017_Athena_Commando_M",
        "CID_028_Athena_Commando_F",
        "CID_029_Athena_Commando_F_Halloween",
        "CID_030_Athena_Commando_M_Halloween",
        "CID_038_Athena_Commando_M_Disco",
        "CID_044_Athena_Commando_F_SciPop",
        "CID_048_Athena_Commando_F_HolidayGingerbread",
        "CID_049_Athena_Commando_M_HolidayGingerbread"
    };
    static const char* gliderCid[] = { "Glider_ID_001", "Glider_ID_002_Mako", "Glider_ID_003_FounderGlider" };
    static const char* gliderName[] = { "Default Glider", "Mako", "FounderGlider" };
    static const char* pickCid[] = { "Pickaxe_ID_001", "Pickaxe_Lockjaw", "Pickaxe_ID_015_HolidayPickaxe" };
    static const char* pickName[] = { "Default Pickaxe", "Raider's Revenge", "Candy Axe" };
    static const char* emoteCid[] = { "EID_Wave", "EID_DanceMoves", "EID_RideThePony" };
    static const char* emoteName[] = { "Wave", "Default Dance", "Ride The Pony" };
    static const char* bannerCid[] = { "Banner_Default", "Banner_Trophy", "Banner_Season1" };
    static const char* bannerName[] = { "Default", "Victory Trophy", "Season 1" };

    auto drawIcon = [&](const char* cid, int x, int y, int s, bool sel, bool equipped) {
        DrawRectangle(x, y, s, s, Color{22, 32, 58, 255});
        DrawRectangleLines(x, y, s, s, sel ? Color{250, 200, 30, 255} : Color{70, 100, 150, 255});
        const Texture2D* tex = (cid && cid[0]) ? gIcons.Cosmetic(cid) : nullptr;
        if (tex && tex->id)
            DrawTexturePro(*tex, {0,0,(float)tex->width,(float)tex->height},
                {(float)x+3,(float)y+3,(float)s-6,(float)s-6}, {0,0}, 0, WHITE);
        else
            DrawRectangle(x + s/4, y + s/4, s/2, s/2, Color{100, 130, 180, 255});
        if (equipped) {
            DrawRectangle(x + 2, y + 2, 36, 12, Color{250, 200, 30, 240});
            DrawTextEx(gFont, "EQ", {(float)(x + 6), (float)(y + 2)}, 11, 1, BLACK);
        }
    };

    // Selection grid layout: 6 items per row
    const int COLS = 6;
    const int slot = 50;      // icon tile size
    const int gapX = 8;       // horizontal gap between tiles
    const int rowGap = 18;    // extra vertical gap to leave room for labels
    int count = (lockerCat == 0) ? kLookCount : 3;
    int rows = lockerBrowsing ? ((count + COLS - 1) / COLS) : 0;

    // Dynamic panel height so a 6-wide grid always fits (2 rows for outfits)
    int panelW = (int)(W * 0.50f);
    int baseHeader = 40;
    int baseFooter = 22;
    int gridH = lockerBrowsing
        ? (rows * (slot + rowGap))
        : 54 + 16;               // single preview row when not browsing
    int panelH = baseHeader + gridH + baseFooter;
    // Keep it from growing taller than the screen
    if (panelH > H - 60) panelH = H - 60;
    int panelX = 18;
    int panelY = H - panelH - 18;
    DrawRectangle(panelX, panelY, panelW, panelH, Color{10, 16, 32, 235});
    DrawRectangleLines(panelX, panelY, panelW, panelH, Color{250, 200, 40, 180});

    DrawTextEx(gFont, "LOCKER", {(float)(panelX + 10), (float)(panelY + 6)}, 15, 1, WHITE);

    if (!lockerBrowsing) {
        int y = panelY + 32;
        int startX = panelX + 12;
        for (int c = 0; c < 5; c++) {
            int x = startX + (c % 5) * (54 + 10);
            bool sel = (lockerCat == c);
            const char* cid = (c == 0) ? kLookCid[p1Look] :
                              (c == 1) ? gliderCid[equippedGlider] :
                              (c == 2) ? pickCid[equippedPick] :
                              (c == 3) ? emoteCid[equippedEmote] :
                                         bannerCid[equippedBanner];
            drawIcon(cid, x, y, 54, sel, true);
            DrawTextEx(gFont, cats[c], {(float)x, (float)(y + 54 + 3)}, 11, 1,
                       sel ? Color{250, 200, 40, 255} : Color{190, 200, 220, 255});
        }
        DrawTextEx(gFont, "LEFT/RIGHT  ENTER open", {(float)(panelX + 10), (float)(panelY + panelH - 18)}, 11, 1, Color{150, 165, 190, 255});
    } else {
        DrawTextEx(gFont, cats[lockerCat], {(float)(panelX + 90), (float)(panelY + 8)}, 14, 1, Color{250, 200, 40, 255});
        const char** cids = (lockerCat == 0) ? outfitCid : (lockerCat == 1) ? gliderCid : (lockerCat == 2) ? pickCid : (lockerCat == 3) ? emoteCid : bannerCid;
        const char** names = (lockerCat == 0) ? nullptr : (lockerCat == 1) ? gliderName : (lockerCat == 2) ? pickName : (lockerCat == 3) ? emoteName : bannerName;
        int equippedIdx = (lockerCat == 0) ? p1Look : (lockerCat == 1) ? equippedGlider : (lockerCat == 2) ? equippedPick : (lockerCat == 3) ? equippedEmote : equippedBanner;
        if (lockerSel < 0) lockerSel = 0;
        if (lockerSel >= count) lockerSel = count - 1;

        int startX = panelX + 12;
        int startY = panelY + 34;
        for (int i = 0; i < count; i++) {
            int col = i % COLS;
            int row = i / COLS;
            int x = startX + col * (slot + gapX);
            int y = startY + row * (slot + rowGap);
            // Bail if this row would overflow the panel (clamp panel already prevents most cases)
            if (y + slot > panelY + panelH - 18) break;
            drawIcon(cids[i], x, y, slot, i == lockerSel, i == equippedIdx);
            const char* nm = (lockerCat == 0) ? kLooks[i].name : names[i];
            DrawTextEx(gFont, nm, {(float)x, (float)(y + slot + 2)}, 10, 1,
                       (i == lockerSel) ? Color{250, 200, 40, 255} : Color{190, 200, 220, 255});
        }
        DrawTextEx(gFont, "ARROWS navigate   ENTER equip   ESC close", {(float)(panelX + 10), (float)(panelY + panelH - 18)}, 11, 1, Color{150, 165, 190, 255});
    }
}

void Game::DrawTabShop(int W, int H) {
    DrawRectangle(0, 40, W, H - 40, Color{16, 22, 40, 255});
    DrawTextEx(gFont, "ITEM SHOP", {24, 52}, 24, 1, WHITE);
    // V-Bucks
    const Texture2D* vb = gIcons.Cosmetic("VBucks");
    if (vb && vb->id)
        DrawTexturePro(*vb, {0,0,(float)vb->width,(float)vb->height}, { (float)(W - 120), 52, 22, 22}, {0,0}, 0, WHITE);
    DrawTextEx(gFont, TextFormat("%d", account.vbucks), {(float)(W - 92), 54}, 18, 1, Color{100, 200, 255, 255});

    auto rarityCol = [](const std::string& r) {
        if (r == "rare") return Color{60, 150, 255, 255};
        if (r == "uncommon") return Color{90, 200, 60, 255};
        if (r == "epic") return Color{180, 80, 255, 255};
        if (r == "legendary") return Color{255, 180, 40, 255};
        return Color{170, 170, 175, 255};
    };

    auto drawCard = [&](const ShopItem& it, int x, int y, int w, int h, bool sel) {
        Color rar = rarityCol(it.rarity);
        DrawRectangle(x, y, w, h, Color{28, 36, 58, 255});
        DrawRectangle(x, y + h - 6, w, 6, rar);
        if (sel) DrawRectangleLines(x - 1, y - 1, w + 2, h + 2, Color{250, 200, 30, 255});
        const Texture2D* tex = !it.cid.empty() ? gIcons.Cosmetic(it.cid) : nullptr;
        if (tex && tex->id) {
            float isz = (float)h - 50;
            DrawTexturePro(*tex, {0,0,(float)tex->width,(float)tex->height},
                {(float)x + (w - isz) / 2, (float)y + 8, isz, isz}, {0,0}, 0, WHITE);
        } else {
            DrawRectangle(x + w/4, y + h/5, w/2, h/2, rar);
        }
        DrawTextEx(gFont, it.name.c_str(), {(float)(x + 8), (float)(y + h - 40)}, 13, 1, WHITE);
        if (vb && vb->id)
            DrawTexturePro(*vb, {0,0,(float)vb->width,(float)vb->height},
                {(float)(x + 8), (float)(y + h - 22), 14, 14}, {0,0}, 0, WHITE);
        DrawTextEx(gFont, TextFormat("%d", it.price), {(float)(x + 26), (float)(y + h - 22)}, 14, 1, Color{100, 200, 255, 255});
    };

    DrawTextEx(gFont, "FEATURED", {24, 90}, 16, 1, Color{250, 200, 40, 255});
    int idx = 0;
    int cardW = 150, cardH = 200;
    for (size_t i = 0; i < shopFeatured.size(); i++) {
        int x = 24 + (int)i * (cardW + 16);
        drawCard(shopFeatured[i], x, 112, cardW, cardH, shopSel == idx);
        idx++;
    }
    DrawTextEx(gFont, "DAILY", {24, 330}, 16, 1, Color{250, 200, 40, 255});
    for (size_t i = 0; i < shopDaily.size(); i++) {
        int x = 24 + (int)i * (cardW + 16);
        drawCard(shopDaily[i], x, 352, cardW, cardH, shopSel == idx);
        idx++;
    }
    DrawTextEx(gFont, "ARROWS select   ENTER purchase", {24, (float)(H - 28)}, 13, 1, Color{160, 175, 200, 255});
}

void Game::DrawTabProfile(int W, int H) {
    DrawRectangle(0, 36, W, H - 36, Color{14, 20, 40, 255});
    DrawTextEx(gFont, "PROFILE", {24, 52}, 26, 1, WHITE);
    DrawTextEx(gFont, account.displayName.c_str(), {24, 96}, 28, 1, WHITE);
    DrawTextEx(gFont, TextFormat("@%s", account.username.c_str()), {24, 128}, 16, 1, Color{160, 175, 200, 255});
    DrawTextEx(gFont, TextFormat("Level %d", account.level), {24, 160}, 18, 1, C(250, 200, 40));
    DrawTextEx(gFont, TextFormat("Wins: %d", account.wins), {24, 198}, 17, 1, WHITE);
    DrawTextEx(gFont, TextFormat("Kills: %d", account.kills), {24, 224}, 17, 1, WHITE);
    DrawTextEx(gFont, TextFormat("Matches: %d", account.matches), {24, 250}, 17, 1, WHITE);
    DrawTextEx(gFont, TextFormat("V-Bucks: %d", account.vbucks), {24, 286}, 17, 1, C(100, 200, 255));

    // Logout button
    int bx = 24, by = H - 70, bw = 160, bh = 36;
    DrawRectangle(bx, by, bw, bh, Color{180, 50, 50, 255});
    DrawRectangleLines(bx, by, bw, bh, Color{255, 120, 120, 255});
    DrawTextEx(gFont, "LOG OUT", {(float)(bx + 36), (float)(by + 8)}, 18, 1, WHITE);
    DrawTextEx(gFont, "Click or press L", {24, (float)(H - 28)}, 12, 1, Color{150, 165, 190, 255});
}

void Game::DrawLobby() {
    const int W = LobbyW();
    const int H = LobbyH();
    BeginDrawing();
    ClearBackground(BLACK);
    BeginLobbyCanvas();
    switch (tab) {
        case LobbyTab::Play:        DrawTabPlay(W, H); break;
        case LobbyTab::BattlePass:  DrawTabBattlePass(W, H); break;
        case LobbyTab::Locker:      DrawTabLocker(W, H); break;
        case LobbyTab::ItemShop:    DrawTabShop(W, H); break;
        case LobbyTab::Profile:     DrawTabProfile(W, H); break;
        default: break;
    }
    DrawLobbyNav(W, H);
    if (friendsOpen) DrawFriendsPanel(W, H);
    DrawNotifications(W);
    if (statusMsgT > 0) {
        DrawRectangle(0, 40, W, 24, Color{0, 0, 0, 180});
        DrawTextEx(gFont, statusMsg.c_str(), {12, 44}, 14, 1, Color{250, 220, 100, 255});
    }
    EndLobbyCanvas();
    EndDrawing();
}

void Game::StartMatch() {
    friendsOpen = false; addFriendFocus = false;
    world.Build(map);
    const float ms = world.half / 60.0f;
    world.gunModel = &held.guns[I_AR];
    actors.clear(); grenades.clear(); fx.clear(); feed.clear();
    gVFX.Clear();

    // Local humans on pads / in party
    humans = 1;
    if (inParty || p2Joined) humans = 2;
    if (gameMode == GameMode::Solo && !inParty && !p2Joined) humans = 1;

    // How many total slots (no bots in private modes)
    int total = 8;
    bool allowBots = true;
    if (gameMode == GameMode::Private1v1) {
        // only lobby players (1v1 private)
        total = humans; // 1 or 2
        allowBots = false;
    } else if (gameMode == GameMode::Private2v2) {
        // 2 from lobby vs 2 others — no bots; fill only if online matchmaker connected
        total = 4;
        allowBots = false;
        if (playType == PlayType::Local) {
            // local private 2v2: only who is in the lobby
            total = humans;
        } else if (!mmConnected) {
            total = humans;
        }
        // if online and connected, still only spawn local humans here;
        // remote players would be replicated by the game server (same client only spawns local)
        total = humans;
    } else if (gameMode == GameMode::Solo) {
        total = allowBots ? 8 : 1;
        humans = 1;
    } else if (gameMode == GameMode::Duo) {
        humans = (inParty || p2Joined) ? 2 : 1;
        total = allowBots ? 8 : humans;
    }

    matchKills = 0;
    SetPresence("online", "match", false);

    std::vector<Vector3> sp = world.spawns;
    for (int i = (int)sp.size() - 1; i > 0; i--) std::swap(sp[i], sp[RandI(0, i)]);

    for (int i = 0; i < total; i++) {
        Actor a;
        a.isBot = allowBots && (i >= humans);
        a.device = (i == 0) ? 0 : (i == 1 && humans >= 2 ? 1 : -1);
        if (i == 0) {
            a.look = p1Look;
            a.name = account.displayName.empty() ? "Player 1" : account.displayName;
        } else if (i == 1 && humans >= 2) {
            a.look = p2Look;
            a.name = inParty && !partyMemberName.empty() ? partyMemberName : "Player 2";
        } else {
            a.look = RandI(0, kLookCount - 1);
            a.name = a.isBot ? TextFormat("Bot%d", i) : TextFormat("Player %d", i + 1);
        }
        a.pos = sp[i % (int)sp.size()];
        a.yaw = atan2f(-a.pos.x, -a.pos.z);
        a.slots[0].id = I_PICKAXE; a.slots[0].count = 1;
        a.goal = {RandF(-30, 30) * ms, 0, RandF(-30, 30) * ms}; a.thinkT = RandF(0, 0.4f);
        actors.push_back(a);
    }

    // ---- Battle bus: everyone starts aboard and jumps over the map
    air.assign(actors.size(), AirState());
    bus = BattleBus();
    if (kUseBattleBus) {
        bus.Start(world.half, 100.0f + 0.6f * world.half);
        for (int i = 0; i < (int)actors.size(); i++) {
            Actor& a = actors[i]; AirState& s = air[i];
            s.phase = DropPhase::Bus;
            s.glider = a.isBot ? RandI(0, kGliderCount - 1) : (i == 0 ? Clamp(equippedGlider, 0, kGliderCount - 1) : 0);
            s.deployAGL = a.isBot ? RandF(45.0f, 70.0f) : Sky::kAutoDeployAGL;
            s.jumpAt = RandF(0.04f, 0.92f);
            s.prevYaw = bus.yaw;
            a.pos = bus.Seat(i);
            a.yaw = bus.yaw; a.pitch = -0.18f; a.grounded = false;
            if (a.isBot && !world.chests.empty()) a.goal = world.chests[RandI(0, (int)world.chests.size() - 1)].pos;   // bots drop on loot
        }
    }

    stormR = kRad[0] * ms; stormT = kWait[0]; stormPhase = 0; shrinking = false; matchT = 0; victory = false; humanPlacement = 0;
    state = State::Match;
    DisableCursor();
    gAudio.StartAmbience("Wind_Falling/sfx_parachute_skydive_loop_3p");
}

void Game::EndMatch() {
    EnableCursor();
    gAudio.StopAmbience();
    world.Unload();
    actors.clear();
    air.clear(); bus = BattleBus(); gSkyHud = SkyHud();
    AwardMatchRewards();
    DisconnectMatchmaker();
    matchmaking = false;
    matchmakeT = 0;
    pauseMenuOpen = false;
    SetPresence("online", "lobby", true);
    state = State::Lobby;
}

Input Game::ReadInput(int device, float dt) {
    Input in;
    if (device == 0) {
        in.move.y = (IsKeyDown(KEY_W) ? 1.0f : 0.0f) - (IsKeyDown(KEY_S) ? 1.0f : 0.0f);
        in.move.x = (IsKeyDown(KEY_D) ? 1.0f : 0.0f) - (IsKeyDown(KEY_A) ? 1.0f : 0.0f);
        Vector2 md = GetMouseDelta(); in.look = {md.x * 0.0025f, md.y * 0.0025f};
        in.fire = IsMouseButtonDown(MOUSE_BUTTON_LEFT); in.firePressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        in.aim = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
        in.jump = IsKeyDown(KEY_SPACE); in.reload = IsKeyPressed(KEY_R); in.interact = IsKeyPressed(KEY_E);
        in.toggleBuild = IsKeyPressed(KEY_Q); in.cycleMat = IsKeyPressed(KEY_G);
        in.drop = IsKeyPressed(KEY_X);
        
        float wh = GetMouseWheelMove();
        if (actors[device].buildMode) {
            if (wh > 0) in.piece = ((int)actors[device].piece == 0) ? 3 : (int)actors[device].piece - 1;
            if (wh < 0) in.piece = ((int)actors[device].piece + 1) % 4;
        } else {
            in.next = wh < 0; in.prev = wh > 0;
        }
        for (int i = 0; i < 6; i++) if (IsKeyPressed(KEY_ONE + i)) in.slot = i;
        if (IsKeyPressed(KEY_Z)) in.piece = 0; if (IsKeyPressed(KEY_X)) in.piece = 1; if (IsKeyPressed(KEY_C)) in.piece = 2; if (IsKeyPressed(KEY_V)) in.piece = 3;
    } else {
        // Works across Xbox 360/One/Series and PlayStation-style pads via raylib/SDL mapping
        int g = device - 1;
        if (!IsGamepadAvailable(g)) return in;
        auto dz = [](float v) { return fabsf(v) < 0.18f ? 0.0f : v; };
        auto held = [&](int b) { return IsGamepadButtonDown(g, b); };
        auto pressed = [&](int b) { return IsGamepadButtonPressed(g, b); };
        static bool prevFire[4] = {false, false, false, false};
        in.move.x = dz(GetGamepadAxisMovement(g, GAMEPAD_AXIS_LEFT_X));
        in.move.y = -dz(GetGamepadAxisMovement(g, GAMEPAD_AXIS_LEFT_Y));
        in.look = {GetGamepadAxisMovement(g, GAMEPAD_AXIS_RIGHT_X) * 4.2f * dt, GetGamepadAxisMovement(g, GAMEPAD_AXIS_RIGHT_Y) * 4.2f * dt};
        // R2 / RT (axis or digital) — PSX/PS2/PS3/PS4/PS5 + Xbox
        float rt = GetGamepadAxisMovement(g, GAMEPAD_AXIS_RIGHT_TRIGGER);
        float lt = GetGamepadAxisMovement(g, GAMEPAD_AXIS_LEFT_TRIGGER);
        // Combat Pro: LT aim, RT fire (analog thresholds)
        in.fire = rt > 0.25f;
        in.aim  = lt > 0.25f;
        if (g < 4) { in.firePressed = in.fire && !prevFire[g]; prevFire[g] = in.fire; }
        // Face buttons: Cross/A jump, Square/X reload, Circle/B interact, Triangle/Y build
        in.jump = held(GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
        in.reload = pressed(GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
        in.interact = pressed(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
        in.toggleBuild = pressed(GAMEPAD_BUTTON_RIGHT_FACE_UP);
        in.drop = pressed(GAMEPAD_BUTTON_LEFT_FACE_DOWN);
        in.next = pressed(GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
        in.prev = pressed(GAMEPAD_BUTTON_LEFT_TRIGGER_1);
        if (pressed(GAMEPAD_BUTTON_LEFT_FACE_UP)) in.piece = 0;
        if (pressed(GAMEPAD_BUTTON_LEFT_FACE_LEFT)) in.piece = 1;
        if (pressed(GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) in.piece = 2;
        if (pressed(GAMEPAD_BUTTON_LEFT_FACE_DOWN)) in.piece = 3;
        in.cycleMat = pressed(GAMEPAD_BUTTON_RIGHT_THUMB) || pressed(GAMEPAD_BUTTON_LEFT_THUMB);
    }
    return in;
}

int Game::AliveCount() const { int n = 0; for (const auto& a : actors) if (a.alive) n++; return n; }

void Game::UpdateStorm(float dt) {
    const float ms = world.half / 60.0f;
    if (stormPhase >= 4) { stormR = kRad[4] * ms; return; }
    stormT -= dt;
    if (stormT <= 0) {
        if (!shrinking) { shrinking = true; stormT = kShrink[stormPhase]; gAudio.Play("Shield/SFX_SafeZone_BeginShrinking"); }
        else { shrinking = false; stormPhase++; stormT = stormPhase < 4 ? kWait[stormPhase] : 0; }
    }
    if (stormPhase >= 4) { stormR = kRad[4] * ms; return; }
    stormR = shrinking ? Lerp(kRad[stormPhase + 1] * ms, kRad[stormPhase] * ms, fmaxf(0.0f, stormT) / kShrink[stormPhase]) : kRad[stormPhase] * ms;
}

void Game::UpdateMatch(float dt) {
    auto padDown = [](int b) {
        for (int g = 0; g < 4; g++)
            if (IsGamepadAvailable(g) && IsGamepadButtonPressed(g, b)) return true;
        return false;
    };
    if (IsKeyPressed(KEY_ESCAPE) || padDown(GAMEPAD_BUTTON_MIDDLE_RIGHT) || padDown(GAMEPAD_BUTTON_MIDDLE_LEFT)) {
        if (state == State::Over) { EndMatch(); return; }
        pauseMenuOpen = !pauseMenuOpen;
        if (pauseMenuOpen) EnableCursor(); else DisableCursor();
        return;
    }
    if (pauseMenuOpen) {
        if (IsKeyPressed(KEY_ENTER) || padDown(GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
            pauseMenuOpen = false;
            DisableCursor();
        }
        // Leave match
        if (IsKeyPressed(KEY_L) || IsKeyPressed(KEY_BACKSPACE) || padDown(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) {
            pauseMenuOpen = false;
            EndMatch();
        }
        return;
    }
    if (state == State::Over && IsKeyPressed(KEY_ENTER)) { EndMatch(); return; }
    matchT += dt;
    UpdateBus(dt);
    if (state == State::Match && !bus.active) UpdateStorm(dt);       // storm timer starts once the bus is gone
    for (int i = 0; i < (int)actors.size(); i++) {
        Input in;
        if (state == State::Over && !actors[i].isBot) { UpdateActor(i, Input(), dt); continue; }
        if (!actors[i].isBot) in = ReadInput(actors[i].device, dt);
        else if (air[i].phase == DropPhase::Landed) BotThink(i, dt, in);     // bots in the air are steered by BotAerial
        UpdateActor(i, in, dt);
    }
    for (int i = (int)grenades.size() - 1; i >= 0; i--) {
        Grenade& g = grenades[i]; g.t -= dt; g.v.y -= 22 * dt;
        g.p = Vector3Add(g.p, Vector3Scale(g.v, dt));
        float gy = world.GroundY(g.p, g.p.y, 0.1f);
        if (g.p.y < gy + 0.12f) { g.p.y = gy + 0.12f; g.v.y = fabsf(g.v.y) * 0.4f; g.v.x *= 0.6f; g.v.z *= 0.6f; }
        if (g.t <= 0) { Explode(g.p, 4.5f, 100, g.owner); grenades.erase(grenades.begin() + i); }
    }
    for (int i = (int)fx.size() - 1; i >= 0; i--) { fx[i].t += dt; if (fx[i].t > fx[i].life) fx.erase(fx.begin() + i); }
    for (auto& m : feed) m.t -= dt;
    
    Camera3D mainCam = {};
    for (const auto& a : actors) if (!a.isBot && a.alive) { mainCam.position = a.camPos; mainCam.target = Vector3Add(a.camPos, a.camFwd); }
    gVFX.Update(dt, mainCam);
    
    world.Update(dt);
    if (state == State::Match) {
        bool humanAlive = false; for (const auto& a : actors) if (!a.isBot && a.alive) humanAlive = true;
        int alive = AliveCount();
        if (!humanAlive || alive <= 1) {
            victory = humanAlive && alive <= 1;
            for (auto& a : actors) if (!a.isBot) humanPlacement = (humanPlacement == 0) ? a.placement : std::min(humanPlacement, a.placement == 0 ? 99 : a.placement);
            state = State::Over;
            if (victory) gAudio.Play("Character/SFX_LastPlayerKilled");
            else gAudio.Play("Character/SFX_PlayerKilled_UILayer");
        }
    }
}

void Game::ComputeCamera(Actor& a) {
    Vector3 head = {a.pos.x, a.pos.y + 1.55f, a.pos.z};
    Vector3 fwd = Fwd(a.yaw, a.pitch), right = RightOf(a.yaw);
    if (a.isAiming) { a.camPos = Vector3Add(head, Vector3Scale(fwd, 0.25f)); a.camFwd = fwd; return; }   // looking down a scope: eye view
    Vector3 want = Vector3Add(Vector3Subtract(head, Vector3Scale(fwd, 3.0f)), Vector3Add(Vector3Scale(right, 0.9f), Vector3{0, 0.3f, 0}));
    Vector3 d = Vector3Subtract(want, head); float L = Vector3Length(d);
    if (L > 0.01f) {
        RayHit h = world.Raycast(Ray{head, Vector3Scale(d, 1.0f / L)}, L);
        if (h.hit) want = Vector3Add(head, Vector3Scale(d, fmaxf(0.4f, h.dist - 0.3f) / L));
    }
    want.y = fmaxf(want.y, 0.3f);
    a.camPos = want; a.camFwd = fwd;
}

// =====================================================================
//  BATTLE BUS / SKYDIVE
//  Phases per actor (AirState::phase): Bus -> Freefall -> Glide -> Landed.
//  While an actor is not Landed, UpdateActor hands over to UpdateAerial.
// =====================================================================
void Game::UpdateBus(float dt) {
    if (!bus.active) return;
    bus.Update(dt);
    if (bus.MapProgress() >= 1.0f)                       // bus is leaving the map: everyone still aboard is dropped
        for (int i = 0; i < (int)actors.size(); i++)
            if (air[i].phase == DropPhase::Bus && actors[i].alive) EjectFromBus(i);
    if (bus.Finished()) bus.active = false;
}

void Game::EjectFromBus(int idx) {
    Actor& a = actors[idx]; AirState& s = air[idx];
    s.phase = DropPhase::Freefall; s.t = 0; s.open = 0; s.fold = 0; s.tilt = 0.5f; s.roll = 0; s.prevYaw = a.yaw;
    s.hvel = Vector3Scale(bus.dir, bus.speed * 0.8f);            // keep some of the bus's momentum
    a.pos = {bus.pos.x + RandF(-0.8f, 0.8f), bus.pos.y - 0.5f, bus.pos.z + RandF(-0.8f, 0.8f)};
    a.vel = {0, -4.0f, 0};
    a.grounded = false;
}

void Game::ComputeBusCamera(Actor& a) {       // orbit the bus + balloon; mouse / right stick rotates the view
    Vector3 target = {bus.pos.x, bus.pos.y + 6.5f, bus.pos.z};
    Vector3 fwd = Fwd(a.yaw, a.pitch);
    a.camPos = Vector3Subtract(target, Vector3Scale(fwd, 26.0f));
    a.camFwd = fwd;
}

void Game::ComputeAirCamera(Actor& a, float dist) {       // third person, further back than on the ground
    Vector3 head = {a.pos.x, a.pos.y + 1.3f, a.pos.z};
    Vector3 fwd = Fwd(a.yaw, a.pitch), right = RightOf(a.yaw);
    Vector3 want = Vector3Add(Vector3Subtract(head, Vector3Scale(fwd, dist)), Vector3Add(Vector3Scale(right, 0.6f), Vector3{0, 0.6f, 0}));
    Vector3 d = Vector3Subtract(want, head); float L = Vector3Length(d);
    if (L > 0.01f) {
        RayHit h = world.Raycast(Ray{head, Vector3Scale(d, 1.0f / L)}, L);
        if (h.hit) want = Vector3Add(head, Vector3Scale(d, fmaxf(0.4f, h.dist - 0.3f) / L));
    }
    want.y = fmaxf(want.y, 0.3f);
    a.camPos = want; a.camFwd = fwd;
}

// Bots: steer towards their goal (a chest / landing spot). Dive when over it, glide level when far.
void Game::BotAerial(int i, float dt, Input& in) {
    Actor& b = actors[i]; AirState& s = air[i];
    float dx = b.goal.x - b.pos.x, dz = b.goal.z - b.pos.z;
    float d = sqrtf(dx * dx + dz * dz);
    b.yaw += Clamp(WrapPi(atan2f(dx, dz) - b.yaw), -2.4f * dt, 2.4f * dt);
    float tp;
    if (s.phase == DropPhase::Freefall) tp = d > 45.0f ? -0.05f : -1.0f;
    else tp = d > 28.0f ? -0.5f : (d > 10.0f ? -0.1f : 0.35f);
    b.pitch = Lerp(b.pitch, tp, fminf(1.0f, 2.5f * dt));
    in.move = {0.0f, 1.0f};
}

void Game::UpdateAerial(int idx, const Input& inIn, float dt) {
    Actor& a = actors[idx];
    AirState& s = air[idx];
    Input in = inIn;
    s.t += dt;
    a.isAiming = false; a.useT = 0; a.prompt.clear();

    const bool jumpPress = in.jump && !s.jumpPrev;     // jump / deploy is edge-triggered
    s.jumpPrev = in.jump;

    // ---- riding the bus: invisible, camera orbits the bus, jump when it is over the map
    if (s.phase == DropPhase::Bus) {
        a.pos = bus.Seat(idx); a.vel = {0, 0, 0}; a.grounded = false;
        if (a.isBot) a.yaw = bus.yaw;
        bool jump = false;
        if (bus.OverMap()) jump = a.isBot ? (bus.MapProgress() >= s.jumpAt) : jumpPress;
        if (jump) { EjectFromBus(idx); if (!a.isBot) ComputeAirCamera(a, 6.5f); }
        else if (!a.isBot) ComputeBusCamera(a);
        return;
    }

    if (a.isBot) BotAerial(idx, dt, in);

    const Vector3 fwd = {sinf(a.yaw), 0, cosf(a.yaw)}, right = RightOf(a.yaw);
    auto move = [&]() {
        a.pos = world.MoveSlide(a.pos, Vector3{s.hvel.x * dt, 0, s.hvel.z * dt}, 0.35f, 1.8f, 0.55f);
        a.pos.x = Clamp(a.pos.x, -world.half + 1, world.half - 1);
        a.pos.z = Clamp(a.pos.z, -world.half + 1, world.half - 1);
        a.pos.y += a.vel.y * dt;
    };
    float gy = 0.0f;
    auto land = [&]() {
        s.fold = s.open > 0.1f ? 1.0f : 0.0f;          // glider folds away
        s.phase = DropPhase::Landed; s.open = 0; s.t = 0; s.hvel = {0, 0, 0};
        a.pos.y = gy; a.vel = {0, 0, 0}; a.grounded = true;
        a.pitch = Clamp(a.pitch, -0.6f, 0.6f);
    };
    const float yawRate = WrapPi(a.yaw - s.prevYaw) / fmaxf(dt, 0.0001f);
    s.prevYaw = a.yaw;

    // ---- freefall: look down to dive (fast, short range), look up / level to cover distance
    if (s.phase == DropPhase::Freefall) {
        const float k = Clamp(-a.pitch / 1.1f, -0.5f, 1.0f), kd = fmaxf(k, 0.0f);
        const float vDown = 32.0f + 26.0f * k;
        const float hSpeed = Lerp(26.0f, 8.0f, kd);
        const float fw = Clamp(0.5f + 0.5f * in.move.y, 0.15f, 1.0f);
        Vector3 target = Vector3Add(Vector3Scale(fwd, hSpeed * fw), Vector3Scale(right, in.move.x * hSpeed * 0.4f));
        s.hvel = Vector3Lerp(s.hvel, target, fminf(1.0f, 2.0f * dt));
        a.vel.y = Lerp(a.vel.y, -vDown, fminf(1.0f, 2.5f * dt));
        move();
        gy = world.GroundY(a.pos, a.pos.y, 0.55f);
        float agl = a.pos.y - gy;
        s.tilt = Lerp(s.tilt, Clamp(1.40f + 0.85f * k, 1.0f, 2.3f), fminf(1.0f, 5.0f * dt));
        s.roll = Lerp(s.roll, Clamp(-yawRate * 0.10f, -0.5f, 0.5f) + in.move.x * 0.30f, fminf(1.0f, 6.0f * dt));
        if (agl <= 0.0f) land();
        else if ((jumpPress && s.t > 0.35f) || agl < s.deployAGL) { s.phase = DropPhase::Glide; s.t = 0; s.open = 0; }
        if (!a.isBot) { if (s.phase == DropPhase::Landed) ComputeCamera(a); else ComputeAirCamera(a, 6.5f); }
        return;
    }

    // ---- glider: steady descent, steer with the camera; look down to speed up, up to flare
    s.open = fminf(1.0f, s.open + dt / 0.45f);
    const float k = Clamp(-a.pitch / 0.9f, -1.0f, 1.0f);
    const float vDown = fmaxf(3.0f, 8.0f + 5.5f * k);
    const float hSpeed = 15.0f + 7.0f * k;
    const float fw = Clamp(0.8f + 0.2f * in.move.y, 0.5f, 1.0f);
    Vector3 target = Vector3Add(Vector3Scale(fwd, hSpeed * fw), Vector3Scale(right, in.move.x * 3.5f));
    s.hvel = Vector3Lerp(s.hvel, target, fminf(1.0f, 1.6f * dt));
    a.vel.y = Lerp(a.vel.y, -vDown, fminf(1.0f, 3.5f * dt));
    move();
    gy = world.GroundY(a.pos, a.pos.y, 0.55f);
    s.tilt = Lerp(s.tilt, 0.04f + 0.42f * fmaxf(k, 0.0f) - 0.18f * fmaxf(-k, 0.0f), fminf(1.0f, 4.0f * dt));
    s.roll = Lerp(s.roll, Clamp(-yawRate * 0.12f, -0.6f, 0.6f) + in.move.x * 0.25f, fminf(1.0f, 4.0f * dt));
    if (a.pos.y <= gy) land();
    if (!a.isBot) { if (s.phase == DropPhase::Landed) ComputeCamera(a); else ComputeAirCamera(a, 7.5f); }
}

void Game::DrawAirActor(const Actor& a, const AirState& s) {
    AirPose p;
    p.tilt = s.tilt; p.roll = s.roll; p.blend = s.open;
    p.time = (float)GetTime();
    p.splay = GliderGripSplay(s.glider);
    Color tint = a.hitFlash > 0 ? Color{255, 130, 130, 255} : WHITE;
    Matrix grip = CharacterDrawAir(chars[a.look], a.pos, a.yaw, p, tint);
    if (s.phase == DropPhase::Glide) DrawGlider(skyModels, s.glider, grip, s.open, true, WHITE);
}

void Game::DrawSkydiveHud(int ai, int W, int H) {
    const Actor& me = actors[ai];
    const AirState& s = air[ai];
    if (me.isBot || !me.alive || s.phase == DropPhase::Landed) return;
    const float u = H / 1080.0f;
    auto line = [&](const std::string& txt, float y, float sz, Color c) {
        float fs = sz * u;
        Vector2 m = MeasureTextEx(gFont, txt.c_str(), fs, 1.0f);
        float x = (W - m.x) / 2;
        DrawTextEx(gFont, txt.c_str(), {x + 2 * u, y + 2 * u}, fs, 1.0f, Color{0, 0, 0, 180});
        DrawTextEx(gFont, txt.c_str(), {x, y}, fs, 1.0f, c);
    };
    const char* key = me.device == 0 ? "SPACE" : "A";
    const Color hint = Color{255, 240, 120, 255};
    float y = H * 0.62f;
    if (s.phase == DropPhase::Bus) {
        if (!bus.OverMap()) line("BATTLE BUS - APPROACHING THE ISLAND", y, 30, WHITE);
        else {
            line(TextFormat("PRESS [%s] TO JUMP", key), y, 34, hint);
            line(TextFormat("The bus leaves the map in %d s", (int)ceilf(bus.SecondsToLeave())), y + 44 * u, 22, WHITE);
        }
    } else {
        if (s.phase == DropPhase::Freefall && s.t > 0.35f) line(TextFormat("PRESS [%s] TO DEPLOY GLIDER", key), y, 30, hint);
        float agl = fmaxf(0.0f, me.pos.y - world.GroundY(me.pos, me.pos.y, 0.55f));
        line(TextFormat("ALTITUDE %d m", (int)agl), H * 0.70f, 22, WHITE);
    }
}

Hit3 Game::TraceRay(Ray r, float range, int ignore) {
    Hit3 best; best.d = range; best.p = Vector3Add(r.position, Vector3Scale(r.direction, range));
    RayHit h = world.Raycast(r, range);
    if (h.hit) { best.kind = h.piece >= 0 ? 2 : (h.solid >= 0 ? 1 : 4); best.idx = h.piece >= 0 ? h.piece : h.solid; best.p = h.point; best.d = h.dist; }
    for (int j = 0; j < (int)actors.size(); j++) {
        if (j == ignore || !actors[j].alive || InBus(j)) continue;
        const Actor& o = actors[j];
        BoundingBox bb = {{o.pos.x - 0.35f, o.pos.y, o.pos.z - 0.35f}, {o.pos.x + 0.35f, o.pos.y + 1.8f, o.pos.z + 0.35f}};
        RayCollision c = GetRayCollisionBox(r, bb);
        if (c.hit && c.distance >= 0 && c.distance < best.d) { best.kind = 3; best.idx = j; best.p = c.point; best.d = c.distance; best.head = c.point.y > o.pos.y + 1.5f; }
    }
    return best;
}

void Game::FireWeapon(int i, Slot& s, const ItemDef& d) {
    Actor& a = actors[i];
    Hit3 ah = TraceRay(Ray{a.camPos, a.camFwd}, 300, i);
    Vector3 aim = ah.kind ? ah.p : Vector3Add(a.camPos, Vector3Scale(a.camFwd, 300));
    Vector3 muzzle = Vector3Add(Vector3{a.pos.x, a.pos.y + 1.35f, a.pos.z}, Vector3Scale(Fwd(a.yaw, 0), 0.5f));
    Vector3 base = Vector3Normalize(Vector3Subtract(aim, muzzle));
    
    std::string sndFolder = "Weapons/SMG/sfx_smg_singlefire_close";
    if (s.id == I_PISTOL) sndFolder = "Weapons/Pistol_SixShooter/Sixshooter_Fire_Medium_01";
    else if (s.id == I_PUMP) sndFolder = "Weapons/Scoped_AR/sfx_scopedAR_close";
    else if (s.id == I_TACTICAL) sndFolder = "Weapons/Scoped_AR/sfx_scopedAR_close";
    else if (s.id == I_AR) sndFolder = "Weapons/Striker_Auto/Single_Shot/sfx_striker_singlefire_close";
    else if (s.id == I_SMG) sndFolder = "Weapons/Tactical_SMG/sfx_tacsmg_singlefire_close";
    else if (s.id == I_SNIPER || s.id == I_HUNTING) sndFolder = "Weapons/AMR/sfx_amr_fire_thunder_02";
    
    gAudio.PlayRandomAt(sndFolder, muzzle, a.camPos, 1.0f, RandF(0.95f, 1.05f));
    
    for (int p = 0; p < d.pellets; p++) {
        Vector3 j = {RandF(-1, 1), RandF(-1, 1), RandF(-1, 1)};
        Vector3 dir = Vector3Normalize(Vector3Add(base, Vector3Scale(j, d.spread)));
        Hit3 h = TraceRay(Ray{muzzle, dir}, d.range, i);
        float dmg = d.dmg * RarityMult(s.rar);
        
        if (h.kind == 3) { 
            if (h.head && d.pellets == 1) dmg *= 1.5f; 
            Actor& v = actors[h.idx];
            bool hittingShield = v.shield > 0;
            Damage(h.idx, dmg, i); 
            if (!a.isBot) { 
                if (h.head) { gVFX.SpawnHitMarker(2); gAudio.Play("HitNotifications/sfx_hitmarker_notification_critical_layer", 0.6f); }
                else if (hittingShield) { gVFX.SpawnHitMarker(1); gAudio.Play("HitNotifications/sfx_hitmarker_bighit", 0.4f); }
                else { gVFX.SpawnHitMarker(0); gAudio.Play("HitNotifications/sfx_hitmarker_bighit", 0.4f); }
            }
            gVFX.SpawnDamage(h.p, dmg, h.head);
            gAudio.PlayRandomAt("Impacts/sniper_body_impact_01", h.p, a.camPos, 0.8f);
        }
        else if (h.kind == 2) { 
            Piece& pc = world.pieces[h.idx]; pc.hp -= dmg; if (pc.hp <= 0) pc.alive = false; 
            gVFX.SpawnImpact(h.p, Vector3Negate(dir), kMatColor[pc.mat]);
            gAudio.PlayRandomAt("Weapons/BulletWhips/bullet_whip", h.p, a.camPos, 0.6f);
        }
        else if (h.kind == 1) {
            gVFX.SpawnImpact(h.p, Vector3Negate(dir), WHITE);
            gAudio.PlayRandomAt("Weapons/BulletWhips/bullet_whip", h.p, a.camPos, 0.6f);
        }
        
        if (d.explosive) Explode(h.p, d.radius, dmg, i);
        if (p == 0) { a.tA = muzzle; a.tB = h.p; a.tracerT = 0.06f; }
    }
}

void Game::Harvest(int i) {
    Actor& a = actors[i];
    Hit3 h = TraceRay(Ray{a.camPos, a.camFwd}, 50, i);
    Vector3 head = {a.pos.x, a.pos.y + 1.5f, a.pos.z};
    if (h.kind == 0 || h.kind == 4 || Vector3Distance(head, h.p) > 3.8f) return;
    
    gAudio.PlayRandomAt("Weapons/Fire_Axe_Swing", h.p, a.camPos, 0.8f);
    
    if (h.kind == 1) {
        Solid& s = world.solids[h.idx];
        if (s.kind == SK_CRATE || s.kind == SK_TREE) { 
            s.hp -= 34; a.mats[MAT_WOOD] = std::min(999, a.mats[MAT_WOOD] + 10); 
            if (s.hp <= 0) s.alive = false; 
            gAudio.PlayRandomAt("Weapons/BulletWhips/bullet_whip_heavy", h.p, a.camPos, 0.8f);
        } else {
            gAudio.PlayRandomAt("Weapons/BulletWhips/bullet_whip_heavy", h.p, a.camPos, 0.8f);
        }
    } else if (h.kind == 2) {
        Piece& p = world.pieces[h.idx]; p.hp -= 34; a.mats[p.mat] = std::min(999, a.mats[p.mat] + 10); 
        if (p.hp <= 0) p.alive = false;
        gAudio.PlayRandomAt("Weapons/BulletWhips/bullet_whip_heavy", h.p, a.camPos, 0.8f);
    } else if (h.kind == 3) {
        Damage(h.idx, GetItemDef(I_PICKAXE).dmg, i);
        gAudio.PlayRandomAt("Impacts/sniper_body_impact_01", h.p, a.camPos, 0.8f);
    }
}

void Game::Explode(Vector3 p, float radius, float dmg, int owner) {
    fx.push_back(Fx{p, 0, 0.35f, radius});
    gAudio.PlayRandomAt("Weapons/Smoke_Grenade/sfx_SmokeGrenade_Explode", p, (owner >= 0 ? actors[owner].camPos : Vector3{0,0,0}), 1.5f);
    
    for (int j = 0; j < (int)actors.size(); j++) {
        if (!actors[j].alive) continue;
        Vector3 c = {actors[j].pos.x, actors[j].pos.y + 1.0f, actors[j].pos.z};
        float dist = Vector3Distance(c, p);
        if (dist < radius) Damage(j, dmg * (1.0f - dist / radius), owner);
    }
    for (auto& pc : world.pieces) if (pc.alive && Vector3Distance(pc.pos, p) < radius + 2) { pc.hp -= dmg; if (pc.hp <= 0) pc.alive = false; }
    for (auto& s : world.solids) if (s.alive && (s.kind == SK_CRATE || s.kind == SK_TREE) && Vector3Distance(s.pos, p) < radius) { s.hp -= dmg; if (s.hp <= 0) s.alive = false; }
}

void Game::Damage(int victim, float dmg, int attacker) {
    Actor& v = actors[victim];
    if (!v.alive || dmg <= 0) return;
    v.hitFlash = 0.25f;
    float sh = fminf(v.shield, dmg); v.shield -= sh; dmg -= sh; v.health -= dmg;
    if (v.health <= 0) Eliminate(victim, attacker);
}

void Game::Eliminate(int victim, int attacker) {
    if (attacker >= 0 && attacker < (int)actors.size() && victim >= 0 && victim < (int)actors.size()) {
        if (!actors[attacker].isBot && actors[victim].isBot) matchKills++;
        else if (!actors[attacker].isBot && !actors[victim].isBot && attacker != victim) matchKills++;
    }

    Actor& v = actors[victim];
    v.alive = false; v.health = 0;
    v.placement = AliveCount() + 1;
    for (int k = 1; k < 6; k++) if (!v.slots[k].Empty()) { world.AddDrop(LootRoll{v.slots[k].id, v.slots[k].rar, std::max(1, v.slots[k].count)}, Vector3{v.pos.x + RandF(-1, 1), 0, v.pos.z + RandF(-1, 1)}); v.slots[k] = Slot(); }
    std::string msg = (attacker >= 0 && attacker != victim) ? actors[attacker].name + " eliminated " + v.name : v.name + (attacker == victim ? " eliminated themself" : " was lost to the storm");
    if (attacker >= 0 && attacker != victim) actors[attacker].kills++;
    feed.push_back(FeedMsg{msg, 6.0f});
    if (feed.size() > 5) feed.erase(feed.begin());
    
    gAudio.Play("Character/SFX_PlayerKilled_UILayer", 1.0f);
}

bool Game::AddToInventory(Actor& a, const LootRoll& r, bool allowSwap) {
    const ItemDef& d = GetItemDef(r.id);
    if (d.kind == K_AMMO) { a.ammo[d.ammo] += r.count; return true; }
    if (d.kind == K_WEAPON) {
        int slot = -1; for (int k = 1; k < 6; k++) if (a.slots[k].Empty()) { slot = k; break; }
        if (slot < 0) {
            if (!allowSwap) return false;
            slot = a.sel > 0 ? a.sel : 1;
            Slot old = a.slots[slot];
            world.AddDrop(LootRoll{old.id, old.rar, 1}, Vector3Add(a.pos, Vector3Scale(Fwd(a.yaw, 0), 1.0f)));
        }
        a.slots[slot].id = r.id; a.slots[slot].rar = r.rar; a.slots[slot].mag = d.mag; a.slots[slot].count = 1;
        if (!a.isBot) a.sel = slot;
        return true;
    }
    for (int k = 1; k < 6; k++) if (a.slots[k].id == r.id && a.slots[k].count < d.stack) { a.slots[k].count = std::min(d.stack, a.slots[k].count + r.count); return true; }
    for (int k = 1; k < 6; k++) if (a.slots[k].Empty()) { a.slots[k].id = r.id; a.slots[k].rar = r.rar; a.slots[k].mag = 0; a.slots[k].count = r.count; return true; }
    return false;
}

void Game::PickUp(Actor& a, int di) {
    Drop& d = world.drops[di];
    if (AddToInventory(a, d.item, !a.isBot)) {
        d.alive = false;
        gAudio.PlayRandomAt("Items/Fort_Bandage_Use_Athena_01", d.pos, a.camPos, 0.6f);
    }
    else if (!a.isBot) Toast(a, "Inventory full");
}

void Game::OpenChest(int ci) {
    Chest& c = world.chests[ci]; c.opened = true;
    gAudio.PlayRandomAt("Items/Fort_Medkit_Use_Athena", c.pos, actors[0].camPos, 1.0f);
    for (int n = 0; n < 3; n++) {
        LootRoll r = RollLoot(true);
        if (n == 0) for (int t = 0; t < 8 && GetItemDef(r.id).kind != K_WEAPON; t++) r = RollLoot(true);
        world.AddDrop(r, Vector3{c.pos.x + RandF(-1.3f, 1.3f), 0, c.pos.z + RandF(-1.3f, 1.3f)});
    }
}

void Game::UpdateActor(int idx, const Input& in, float dt) {
    Actor& a = actors[idx];
    a.hitFlash = fmaxf(0, a.hitFlash - dt); a.toastT -= dt; a.tracerT -= dt;
    if (!a.alive) return;
    if (!a.isBot) { a.yaw -= in.look.x; a.pitch = Clamp(a.pitch - in.look.y, -1.3f, 1.3f); }
    {   // drop state: glider fold-away timer, and the whole bus / freefall / glide flight until landed
        AirState& as = air[idx];
        if (as.fold > 0) as.fold = fmaxf(0.0f, as.fold - dt / 0.55f);
        if (as.phase != DropPhase::Landed) { UpdateAerial(idx, in, dt); return; }
    }
    a.fireCd = fmaxf(0, a.fireCd - dt); a.placeCd -= dt; a.idle += dt;
    if (a.swing >= 0) { a.swing += dt / 0.32f; if (a.swing >= 1) a.swing = -1; }
    
    if (in.drop && a.sel > 0) {
        Slot& s = a.slots[a.sel];
        if (!s.Empty()) {
            Vector3 dropPos = Vector3Add(a.pos, Vector3Scale(Fwd(a.yaw, 0), 1.2f));
            dropPos.y = a.pos.y;
            world.AddDrop(LootRoll{s.id, s.rar, s.count}, dropPos);
            s = Slot();
            a.fireCd = 0.2f;
        }
    }

    auto select = [&](int s) { if (s != a.sel) { a.sel = s; a.reloadT = 0; a.useT = 0; } a.buildMode = false; };
    if (in.slot >= 0) select(in.slot);
    if (in.next) select((a.sel + 1) % 6);
    if (in.prev) select((a.sel + 5) % 6);
    if (in.toggleBuild) a.buildMode = !a.buildMode;
    if (in.piece >= 0) { 
        if (a.buildMode) a.piece = (PieceType)in.piece; 
        else { a.piece = (PieceType)in.piece; a.buildMode = true; } 
    }
    if (in.cycleMat) a.mat = (Mat)((a.mat + 1) % MAT_COUNT);

    a.isAiming = false;
    if (!a.buildMode && a.sel > 0) {
        const Slot& s = a.slots[a.sel];
        if (!s.Empty() && GetItemDef(s.id).kind == K_WEAPON) {
            ItemId id = s.id;
            if (id == I_SNIPER || id == I_HUNTING) {
                a.isAiming = in.aim;
            }
        }
    }

    Vector3 f = {sinf(a.yaw), 0, cosf(a.yaw)}, r = RightOf(a.yaw);
    Vector3 wish = Vector3Add(Vector3Scale(f, in.move.y), Vector3Scale(r, in.move.x));
    float wl = Vector3Length(wish); if (wl > 1) wish = Vector3Scale(wish, 1.0f / wl);
    
    float spd = 6.2f;
    if (a.useT > 0) spd *= 0.45f;
    if (a.isAiming) spd *= 0.4f;
    
    a.pos = world.MoveSlide(a.pos, Vector3{wish.x * spd * dt, 0, wish.z * spd * dt}, 0.35f, 1.8f, 0.55f);
    a.pos.x = Clamp(a.pos.x, -world.half + 1, world.half - 1); a.pos.z = Clamp(a.pos.z, -world.half + 1, world.half - 1);
    float gy = world.GroundY(a.pos, a.pos.y, 0.55f);
    a.vel.y -= 22.0f * dt; a.pos.y += a.vel.y * dt;
    if (a.pos.y <= gy && a.vel.y <= 0) { a.pos.y = gy; a.vel.y = 0; a.grounded = true; } else a.grounded = false;
    if (in.jump && a.grounded) { a.vel.y = 7.2f; a.grounded = false; }
    a.moveAmt = Lerp(a.moveAmt, (wl > 0.1f && a.grounded) ? fminf(wl, 1.0f) : 0.0f, fminf(1.0f, 10 * dt));
    a.phase += dt * 10.0f * a.moveAmt;

    if (!a.isBot) ComputeCamera(a);

    if (sqrtf(a.pos.x * a.pos.x + a.pos.z * a.pos.z) > stormR) { a.stormTick += dt; if (a.stormTick >= 1.0f) { a.stormTick = 0; Damage(idx, (float)(1 + stormPhase), -1); if (!a.alive) return; } }

    a.prompt.clear();
    int nd = -1; float dd = 2.4f;
    for (int k = 0; k < (int)world.drops.size(); k++) if (world.drops[k].alive) { float d = Vector3Distance(a.pos, world.drops[k].pos); if (d < dd) { dd = d; nd = k; } }
    int nc = -1; float cd = 2.8f;
    for (int k = 0; k < (int)world.chests.size(); k++) if (!world.chests[k].opened) { float d = Vector3Distance(a.pos, world.chests[k].pos); if (d < cd) { cd = d; nc = k; } }
    if (nd >= 0) {
        a.prompt = std::string("Pick up ") + GetItemDef(world.drops[nd].item.id).name;
        if (in.interact) PickUp(a, nd);
    } else if (nc >= 0) {
        a.prompt = "Open Chest";
        if (in.interact) OpenChest(nc);
    }

    if (a.buildMode) {
        if (in.fire && a.placeCd <= 0) {
            a.placeCd = 0.12f;
            if (a.mats[a.mat] < 10) { Toast(a, "Not enough materials"); }
            else if (world.Place(a.piece, a.mat, a.pos, a.yaw)) { 
                a.mats[a.mat] -= 10; 
                gAudio.PlayRandomAt("Items/Fort_Bandage_Use_Athena_01", a.pos, a.camPos, 0.8f);
            }
        }
        return;
    }
    if (a.sel == 0) {
        if (in.fire && a.swing < 0 && a.fireCd <= 0) { a.swing = 0; a.fireCd = 0.55f; Harvest(idx); }
        return;
    }
    Slot& s = a.slots[a.sel];
    if (s.Empty()) return;
    const ItemDef& d = GetItemDef(s.id);
    if (d.kind == K_WEAPON) {
        if (a.reloadT > 0) {
            a.reloadT -= dt;
            if (a.reloadT <= 0) { 
                int take = std::min(d.mag - s.mag, a.ammo[d.ammo]); 
                s.mag += take; 
                a.ammo[d.ammo] -= take; 
                gAudio.PlayRandomAt("Weapons/SMG/sfx_smg_singlefire_medium", a.pos, a.camPos, 0.8f);
            }
        } else if ((in.reload || (s.mag <= 0 && in.fire)) && s.mag < d.mag && a.ammo[d.ammo] > 0) {
            a.reloadT = d.reload;
            gAudio.PlayRandomAt("Weapons/SMG/sfx_smg_singlefire_medium", a.pos, a.camPos, 0.8f);
        } else if (in.fire && a.fireCd <= 0 && s.mag > 0 && (d.autoFire || in.firePressed)) {
            a.fireCd = 1.0f / d.rate; 
            s.mag--; 
            ItemDef dMod = d;
            if (a.isAiming) dMod.spread *= 0.1f;
            FireWeapon(idx, s, dMod);
        } else if (in.fire && s.mag <= 0 && a.ammo[d.ammo] <= 0 && in.firePressed) Toast(a, "Out of ammo");
    } else if (d.kind == K_HEAL) {
        bool can = (d.heal > 0 && a.health < d.healMax) || (d.shield > 0 && a.shield < d.shieldMax);
        if (in.fire && can) {
            a.useT += dt;
            if (a.useT >= d.useTime) {
                if (d.heal > 0 && a.health < d.healMax) a.health = fminf(d.healMax, a.health + d.heal);
                if (d.shield > 0 && a.shield < d.shieldMax) a.shield = fminf(d.shieldMax, a.shield + d.shield);
                if (--s.count <= 0) s = Slot();
                a.useT = 0;
                gAudio.PlayRandomAt("Items/sfx_MiniShieldPotion_Complete_01", a.pos, a.camPos, 0.8f);
            }
        } else a.useT = 0;
    } else if (d.kind == K_THROWABLE) {
        if (in.firePressed && a.fireCd <= 0) {
            a.fireCd = 0.6f;
            Vector3 m = Vector3{a.pos.x, a.pos.y + 1.4f, a.pos.z};
            Vector3 v = Vector3Add(Vector3Scale(a.camFwd, 16.0f), Vector3{0, 4.0f, 0});
            grenades.push_back(Grenade{m, v, 1.7f, idx});
            if (--s.count <= 0) s = Slot();
            gAudio.PlayRandomAt("Weapons/Smoke_Grenade/sfx_SmokeGrenade_Bounce_01", a.pos, a.camPos, 0.8f);
        }
    }
}

void Game::BotThink(int i, float dt, Input& in) {
    Actor& b = actors[i];
    Vector3 eye = {b.pos.x, b.pos.y + 1.5f, b.pos.z};
    b.thinkT -= dt;
    if (b.thinkT <= 0) {
        b.thinkT = 0.35f; b.enemy = -1; float best = 50;
        for (int j = 0; j < (int)actors.size(); j++) {
            if (j == i || !actors[j].alive) continue;
            Vector3 tp = {actors[j].pos.x, actors[j].pos.y + 1.2f, actors[j].pos.z};
            Vector3 dv = Vector3Subtract(tp, eye); float d = Vector3Length(dv);
            if (d >= best || d < 0.1f) continue;
            RayHit h = world.Raycast(Ray{eye, Vector3Scale(dv, 1.0f / d)}, d);
            if (!h.hit || h.dist > d - 0.6f) { best = d; b.enemy = j; }
        }
        int w = -1;
        for (int k = 1; k < 6; k++) if (!b.slots[k].Empty() && GetItemDef(b.slots[k].id).kind == K_WEAPON) { w = k; break; }
        b.sel = (w >= 0) ? w : 0;
        if (b.enemy < 0) {
            float bd = 70; bool found = false;
            for (const auto& d : world.drops) if (d.alive) { float dist = Vector3Distance(b.pos, d.pos); if (dist < bd) { bd = dist; b.goal = d.pos; found = true; } }
            if (w < 0) for (const auto& c : world.chests) if (!c.opened) { float dist = Vector3Distance(b.pos, c.pos); if (dist < bd) { bd = dist; b.goal = c.pos; found = true; } }
            if (!found && (Vector3Distance(b.pos, b.goal) < 2.0f || RandF(0, 1) < 0.05f)) {
                float R = fmaxf(8.0f, fminf(stormR - 8, 50.0f * world.half / 60.0f)); b.goal = {RandF(-R, R), 0, RandF(-R, R)};
            }
        }
    }
    Vector3 goal = b.goal;
    if (sqrtf(b.pos.x * b.pos.x + b.pos.z * b.pos.z) > stormR - 4) goal = {0, 0, 0};
    bool aiming = b.enemy >= 0 && actors[b.enemy].alive;
    Vector3 look, target = {0, 0, 0};
    if (aiming) { Actor& e = actors[b.enemy]; target = {e.pos.x, e.pos.y + 1.2f, e.pos.z}; look = target; }
    else look = {goal.x, eye.y, goal.z};
    Vector3 dv = Vector3Subtract(look, eye);
    float want = atan2f(dv.x, dv.z), dy = WrapPi(want - b.yaw);
    b.yaw += Clamp(dy, -6.0f * dt, 6.0f * dt);
    float hd = sqrtf(dv.x * dv.x + dv.z * dv.z);
    b.pitch = Lerp(b.pitch, Clamp(atan2f(dv.y, fmaxf(hd, 0.1f)), -1.0f, 1.0f), fminf(1.0f, 8 * dt));
    float gd = Vector3Distance(Vector3{b.pos.x, 0, b.pos.z}, Vector3{goal.x, 0, goal.z});
    bool gun = b.sel > 0 && !b.slots[b.sel].Empty() && GetItemDef(b.slots[b.sel].id).kind == K_WEAPON;
    float edist = aiming ? Vector3Distance(eye, target) : 0;
    in.move.y = (gd > 1.5f) ? 1.0f : 0.0f;
    if (aiming) {
        in.move.x = sinf((float)GetTime() * 1.3f + i) > 0 ? 0.6f : -0.6f;
        if (gun && edist < 14) in.move.y = 0;
        if (!gun) in.move.y = edist > 2.5f ? 1.0f : 0.0f;
        if (gun && fabsf(dy) < 0.25f && edist < 45) { in.fire = true; in.firePressed = true; }
        else if (!gun && edist < 3.2f && fabsf(dy) < 0.5f) in.fire = true;
    }
    Vector3 aimPt = aiming ? Vector3Add(target, Vector3{RandF(-0.6f, 0.6f), RandF(-0.4f, 0.4f), RandF(-0.6f, 0.6f)}) : Vector3Add(eye, Vector3Scale(Fwd(b.yaw, 0), 40));
    b.camPos = eye; b.camFwd = Vector3Normalize(Vector3Subtract(aimPt, eye));
    if (gun && b.slots[b.sel].mag <= 0) in.reload = true;
    if (Vector3Distance(b.pos, b.lastPos) < dt * 1.5f && in.move.y > 0) b.stuckT += dt; else b.stuckT = 0;
    b.lastPos = b.pos;
    if (b.stuckT > 0.35f) { in.jump = true; in.move.x = 1; if (b.stuckT > 1.2f) { b.goal = {RandF(-35, 35) * world.half / 60.0f, 0, RandF(-35, 35) * world.half / 60.0f}; b.stuckT = 0; } }
    b.interactT -= dt; if (b.interactT <= 0) { in.interact = true; b.interactT = 0.5f; }
}

// Sniper scope: black mask with a round lens, crosshair, mil-dots and thick outer posts.
static void DrawScope(int W, int H, bool bolt) {
    float cx = W / 2.0f, cy = H / 2.0f, R = H * 0.46f;
    DrawRing({cx, cy}, R, sqrtf((float)(W * W + H * H)) / 2.0f * 1.05f + 8.0f, 0, 360, 72, BLACK);
    DrawRing({cx, cy}, R - 2.0f, R, 0, 360, 72, C(30, 30, 34));
    Color rc = C(8, 8, 8, 240);
    DrawLineEx({cx - R, cy}, {cx + R, cy}, 1.0f, rc); DrawLineEx({cx, cy - R}, {cx, cy + R}, 1.0f, rc);
    float post = R * 0.5f, th = bolt ? 3.0f : 2.0f;
    DrawRectangle((int)(cx - R), (int)(cy - th / 2), (int)post, (int)th, BLACK); DrawRectangle((int)(cx + R - post), (int)(cy - th / 2), (int)post, (int)th, BLACK);
    DrawRectangle((int)(cx - th / 2), (int)(cy - R), (int)th, (int)post, BLACK); DrawRectangle((int)(cx - th / 2), (int)(cy + R - post), (int)th, (int)post, BLACK);
    for (int k = 1; k <= 4; k++) {
        float o = k * R * 0.11f;
        DrawRectangle((int)(cx + o), (int)(cy - 1), 2, 3, rc); DrawRectangle((int)(cx - o), (int)(cy - 1), 2, 3, rc);
        DrawRectangle((int)(cx - 1), (int)(cy + o), 3, 2, rc); DrawRectangle((int)(cx - 1), (int)(cy - o), 3, 2, rc);
    }
    DrawRectangle((int)cx - 1, (int)cy - 1, 3, 3, Color{220, 30, 30, 255});
}

void Game::RenderView(RenderTexture2D& rt, int W, int H, int ai) {
    Actor& me = actors[ai];
    Camera3D cam = {};
    cam.position = me.camPos; cam.target = Vector3Add(me.camPos, me.camFwd); cam.up = {0, 1, 0};
    bool scoped = me.isAiming && me.alive && !me.buildMode;
    float fov = 58.0f;
    if (scoped) fov = (me.slots[me.sel].id == I_SNIPER) ? 12.0f : 20.0f;     // bolt-action = high zoom, hunting rifle = medium
    cam.fovy = fov;
    cam.projection = CAMERA_PERSPECTIVE;
    
    BeginTextureMode(rt);
    ClearBackground(gR.fogColor);
    DrawRectangleGradientV(0, 0, W, H / 2, C(230, 140, 92), gR.fogColor);
    gR.SetResolution((float)W, (float)H);
    BeginMode3D(cam);
    world.Draw(cam.position, stormR, cam);
    if (bus.active) DrawBattleBus(skyModels, bus, WHITE);
    for (int ak = 0; ak < (int)actors.size(); ak++) {
        const Actor& a = actors[ak];
        const AirState& as = air[ak];
        if (!a.alive) continue;
        if (as.phase == DropPhase::Bus) continue;                // riding inside the bus
        if (scoped && &a == &me) continue;      // eye is inside the model while scoped
        float gy = world.GroundY(a.pos, a.pos.y, 0.55f);
        const bool flying = as.phase == DropPhase::Freefall || as.phase == DropPhase::Glide;
        DrawCylinder({a.pos.x, gy + 0.06f, a.pos.z}, flying ? 1.1f : 0.45f, flying ? 1.1f : 0.45f, 0.01f, 8, Color{0, 0, 0, (unsigned char)(flying ? 60 : 90)});
        if (flying) { DrawAirActor(a, as); continue; }
        Pose p; p.phase = a.phase; p.moveAmt = a.moveAmt; p.swing = a.swing; p.pitch = a.pitch; p.idle = a.idle; p.airborne = !a.grounded;
        const Slot& s = a.slots[a.sel];
        if (!a.buildMode && a.sel == 0) p.pick = true;
        else if (!a.buildMode && !s.Empty() && GetItemDef(s.id).kind == K_WEAPON) {
            p.gun = true; p.gunTint = RarityColor(s.rar);
            p.gunScale = 1.0f; p.weapon = (int)s.id;      // every weapon has its own model, built at true size
        }
        p.ads = a.isAiming;
        CharacterDraw(chars[a.look], held, a.pos, a.yaw, p, a.hitFlash > 0 ? Color{255, 130, 130, 255} : WHITE);
        if (a.tracerT > 0) DrawLine3D(a.tA, a.tB, Color{255, 240, 150, 255});
        if (as.fold > 0) {      // glider folding away above a player who just landed
            Matrix gf = MatrixMultiply(MatrixRotateY(a.yaw), MatrixTranslate(a.pos.x, a.pos.y + 2.05f + (1.0f - as.fold) * 1.2f, a.pos.z));
            DrawGlider(skyModels, as.glider, gf, as.fold, false, WHITE);
        }
    }
    for (const auto& g : grenades) DrawSphere(g.p, 0.12f, C(50, 80, 40));
    for (const auto& f : fx) { float k = f.t / f.life; DrawSphereEx(f.p, f.r * (0.3f + 0.7f * k), 6, 6, Color{255, (unsigned char)(190 - 120 * k), 60, (unsigned char)(200 * (1 - k))}); }
    
    gVFX.Draw3D();
    world.DrawStorm(stormR, cam);      // translucent: after every opaque thing
    
    if (!me.isBot && me.alive && me.buildMode) {
        Vector3 pos; int dir; world.GetPlacement(me.piece, me.pos, me.yaw, pos, dir);
        AABB bx = world.PieceBox(me.piece, pos, dir);
        Vector3 c = {(bx.min.x + bx.max.x) / 2, (bx.min.y + bx.max.y) / 2, (bx.min.z + bx.max.z) / 2}, sz = {bx.max.x - bx.min.x, bx.max.y - bx.min.y, bx.max.z - bx.min.z};
        DrawCubeWiresV(c, sz, me.mats[me.mat] >= 10 ? Color{120, 255, 160, 255} : Color{255, 90, 90, 255});
    }
    EndMode3D();
    {   // storm screen effect while standing in the storm + scope mask
        float dist = sqrtf(me.pos.x * me.pos.x + me.pos.z * me.pos.z);
        if (me.alive && dist > stormR) {
            float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 3.0f);
            DrawRectangle(0, 0, W, H, Color{120, 40, 190, (unsigned char)(48 + 28 * pulse)});
            const int e = 36; Color s = Color{70, 10, 120, 150}, z = Color{70, 10, 120, 0};
            DrawRectangleGradientH(0, 0, e, H, s, z); DrawRectangleGradientH(W - e, 0, e, H, z, s);
            DrawRectangleGradientV(0, 0, W, e, s, z); DrawRectangleGradientV(0, H - e, W, e, z, s);
        }
        if (scoped) DrawScope(W, H, me.slots[me.sel].id == I_SNIPER);
    }
    HudInfo info{AliveCount(), stormT, shrinking, stormR, &feed, state == State::Over, victory && me.alive, me.placement ? me.placement : humanPlacement};
    gSkyHud.busActive = bus.active; gSkyHud.x = bus.pos.x; gSkyHud.z = bus.pos.z; gSkyHud.dirx = bus.dir.x; gSkyHud.dirz = bus.dir.z;
    gSkyHud.airborne = air[ai].phase != DropPhase::Landed;
    DrawHud(me, world, W, H, info);
    DrawSkydiveHud(ai, W, H);
    if (!me.isBot && me.alive) gVFX.Draw2D(cam, W, H);
    EndTextureMode();
}

void Game::DrawMatch() {
    BeginDrawing();
    ClearBackground(BLACK);
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (humans == 1) {
        RenderView(rtFull, 1920, 1080, 0);
        // Fill the whole window, no letterbox.
        Present(rtFull, { 0, 0, (float)sw, (float)sh });
    } else {
        RenderView(rtHalf[0], 960, 1080, 0);
        RenderView(rtHalf[1], 960, 1080, 1);
        Present(rtHalf[0], { 0, 0, sw / 2.0f, (float)sh });
        Present(rtHalf[1], { sw / 2.0f, 0, sw / 2.0f, (float)sh });
        DrawRectangle(sw / 2 - 1, 0, 2, sh, BLACK);
    }

    if (pauseMenuOpen) {
        // ... unchanged pause menu drawing ...
        int sw2 = GetScreenWidth(), sh2 = GetScreenHeight();
        DrawRectangle(0, 0, sw2, sh2, Color{0, 0, 0, 160});
        int pw = 280, ph = 160;
        int px = (sw2 - pw) / 2, py = (sh2 - ph) / 2;
        DrawRectangle(px, py, pw, ph, Color{20, 28, 50, 245});
        DrawRectangleLines(px, py, pw, ph, Color{250, 200, 40, 255});
        DrawTextEx(gFont, "PAUSED", {(float)(px + 90), (float)(py + 20)}, 24, 1, WHITE);
        DrawRectangle(px + 40, py + 60, 200, 32, Color{250, 200, 30, 255});
        DrawTextEx(gFont, "RESUME (ENTER)", {(float)(px + 60), (float)(py + 66)}, 16, 1, BLACK);
        DrawRectangle(px + 40, py + 104, 200, 32, Color{180, 50, 50, 255});
        DrawTextEx(gFont, "LEAVE MATCH (L)", {(float)(px + 55), (float)(py + 110)}, 16, 1, WHITE);
    }
    EndDrawing();
}