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
    void PlayRandom(const std::string& folderBase, float volume = 1.0f, float pitch = 1.0f);
    void PlayRandomAt(const std::string& folderBase, Vector3 pos, Vector3 listener, float volume = 1.0f, float pitch = 1.0f);

    void StartAmbience(const std::string& name);
    void StopAmbience();

    // ---- music (streamed, for long looping tracks like lobby music) ----
    // Loads `assets/sounds/<name>.<ext>` using the same search roots as Load().
    // Returns false if the file wasn't found (safe to call repeatedly).
    bool LoadMusic(const std::string& name);
    void PlayMusic();                          // no-op if not loaded
    void StopMusic();
    void SetMusicVolume(float v);              // 0..1
    void SetMusicPitch(float p);               // 1.0 = normal; raylib >= 4.5
    void UpdateMusic();                        // MUST be called once per frame while loaded
    bool MusicLoaded() const   { return musicLoaded; }
    bool MusicPlaying() const  { return musicLoaded && IsMusicStreamPlaying(music); }

private:
    Sound Load(const std::string& name);
    std::unordered_map<std::string, Sound> cache;
    Sound currentAmbience = {};
    bool hasAmbience = false;

    Music music = {};
    bool  musicLoaded = false;
    float musicVolume = 1.0f;                  // last value passed to SetMusicVolume
};

extern Audio gAudio;