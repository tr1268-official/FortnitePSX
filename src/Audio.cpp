#include "Audio.h"

Audio gAudio;

static std::vector<std::string> BaseRoots() {
    std::vector<std::string> roots;
    roots.push_back(""); // Current working directory
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

void Audio::Init() { cache.clear(); }

void Audio::Shutdown() {
    StopAmbience();
    for (auto& kv : cache) UnloadSound(kv.second);
    cache.clear();
}

Sound Audio::Load(const std::string& name) {
    auto it = cache.find(name);
    if (it != cache.end()) return it->second;

    Sound snd = {};
    // List of extensions to try, in order of preference
    const char* exts[] = {".ogg", ".wav", ".mp3", ".flac"};
    
    for (const auto& root : BaseRoots()) {
        for (const char* ext : exts) {
            std::string full = root + "assets/sounds/" + name + ext;
            if (FileExists(full.c_str())) {
                snd = LoadSound(full.c_str());
                if (snd.frameCount > 0) {
                    cache[name] = snd;
                    return snd;
                }
            }
        }
    }
    
    // Not found, cache empty sound to avoid repeated file system checks
    cache[name] = snd;
    return snd;
}

void Audio::Play(const std::string& name, float volume, float pitch) {
    Sound s = Load(name);
    if (s.frameCount > 0) {
        SetSoundPitch(s, pitch);
        SetSoundVolume(s, volume);
        PlaySound(s);
    }
}

void Audio::PlayAt(const std::string& name, Vector3 pos, Vector3 listener, float volume, float pitch) {
    float dist = Vector3Distance(pos, listener);
    if (dist > 55.0f) return; // Max hear distance
    
    float atten = 1.0f - (dist / 55.0f);
    Play(name, volume * atten * atten, pitch); // Inverse square falloff
}

void Audio::PlayRandom(const std::string& folderBase, float volume, float pitch) {
    // Pick a random number 1-5
    int choice = GetRandomValue(1, 5);
    
    // Try folder/name_0X (e.g. _01, _02)
    std::string p = folderBase + "_0" + std::to_string(choice);
    Sound s = Load(p);
    
    // If that fails, try folder/name_X (e.g. _1, _2)
    if (s.frameCount == 0) {
        p = folderBase + "_" + std::to_string(choice);
        s = Load(p);
    }
    
    // If that fails, try folder/name
    if (s.frameCount == 0) {
        s = Load(folderBase);
    }

    if (s.frameCount > 0) {
        SetSoundPitch(s, pitch);
        SetSoundVolume(s, volume);
        PlaySound(s);
    }
}

void Audio::PlayRandomAt(const std::string& folderBase, Vector3 pos, Vector3 listener, float volume, float pitch) {
    float dist = Vector3Distance(pos, listener);
    if (dist > 55.0f) return;
    
    float atten = 1.0f - (dist / 55.0f);
    PlayRandom(folderBase, volume * atten * atten, pitch);
}

void Audio::StartAmbience(const std::string& name) {
    StopAmbience();
    currentAmbience = Load(name);
    if (currentAmbience.frameCount > 0) {
        SetSoundVolume(currentAmbience, 0.4f);
        PlaySound(currentAmbience);
        hasAmbience = true;
    }
}

void Audio::StopAmbience() {
    if (hasAmbience) {
        StopSound(currentAmbience);
        hasAmbience = false;
    }
}