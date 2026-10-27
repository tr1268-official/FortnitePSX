#include "Game.h"
#include <iostream>

Font gFont = {};

static Font TryLoadFont(const char* path) {
    if (!FileExists(path)) return Font{};
    
    // Load at 256px so it scales down smoothly without aliasing
    Font f = LoadFontEx(path, 256, 0, 250);
    
    if (f.texture.id == 0 || f.glyphCount == 0) {
        TraceLog(LOG_WARNING, "File found but failed to parse (0 glyphs): %s", path);
        if (f.texture.id != 0) UnloadFont(f);
        return Font{};
    }
    
    SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
    TraceLog(LOG_INFO, "Loaded custom font: %s (glyphs: %d)", path, f.glyphCount);
    return f;
}

int main() {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "FORTNITE - PSX Edition (Season 1 / 2017)");
    SetTargetFPS(60);
    
    const char* paths[] = {
        "assets/fonts/BurbankBigCondensed-Black.ttf",
        "assets/fonts/BurbankBigCondensed-Black.otf",
        "assets/fonts/Burbank Big Condensed Black.ttf",
        "assets/fonts/Burbank Big Condensed Black.otf",
        "assets/fonts/fortnite.ttf",
        "assets/fonts/fortnite.otf",
    };
    
    gFont = GetFontDefault();
    for (const char* p : paths) {
        Font f = TryLoadFont(p);
        if (f.texture.id != 0) { 
            gFont = f; 
            break; 
        }
    }
    
    if (gFont.texture.id == GetFontDefault().texture.id) {
        TraceLog(LOG_WARNING, "Could not load ANY custom font, using default.");
        if (DirectoryExists("assets/fonts")) {
            FilePathList list = LoadDirectoryFiles("assets/fonts");
            for (unsigned int i = 0; i < list.count; i++)
                TraceLog(LOG_INFO, "  Found: %s", list.paths[i]);
            UnloadDirectoryFiles(list);
        }
    }

    {
        Game g;
        g.Run();
    }
    
    if (gFont.texture.id != GetFontDefault().texture.id) UnloadFont(gFont);
    CloseWindow();
    return 0;
}