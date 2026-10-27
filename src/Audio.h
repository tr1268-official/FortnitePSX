#pragma once
#include "Common.h"
#include "Items.h"
#include <string>
#include <vector>
#include <unordered_map>

class Audio {
public:
    void Init();
    void Shutdown();
    
    void Play(const std::string& name, float volume = 1.0f, float pitch = 1.0f);
    void PlayAt(const std::string& name, Vector3 pos, Vector3 listener, float volume = 1.0f, float pitch = 1.0f);
    
    // Tries `folder/base_01.wav`, `folder/base_02.wav`, etc. Picks a random one.
    void PlayRandom(const std::string& folderBase, float volume = 1.0f, float pitch = 1.0f);
    void PlayRandomAt(const std::string& folderBase, Vector3 pos, Vector3 listener, float volume = 1.0f, float pitch = 1.0f);
    
    void StartAmbience(const std::string& name);
    void StopAmbience();
    
private:
    Sound Load(const std::string& name);
    std::unordered_map<std::string, Sound> cache;
    Sound currentAmbience = {};
    bool hasAmbience = false;
};

extern Audio gAudio;