# FORTNITE – PSX Edition (Season 1 / 2017 look) — C++ / raylib

Unofficial fan prototype. All geometry, textures and the HUD are generated in code (no Epic assets).

## Build
Requires CMake 3.16+, a C++17 compiler, OpenGL 3.3. raylib 5.5 is fetched automatically.

    cmake -B build && cmake --build build --config Release
    ./build/FortnitePSX            (Windows: build\Release\FortnitePSX.exe)

## Controls
Lobby: A/D character · J (or pad START) add Player 2 · Q/E (or pad D-pad) P2 character · TAB map · ENTER play · ESC quit
Player 1 (keyboard+mouse): WASD move · mouse look · LMB fire/harvest/place · SPACE jump · E interact · R reload ·
1-6 slots/wheel · Q build mode · Z/X/C/V wall/floor/stair/roof · G material
Player 2 (gamepad 0, split-screen): L-stick move · R-stick look · RT fire · A jump · B interact · X reload · Y build ·
D-pad piece · RB/LB slot · R3 material

## Files
Renderer (PSX shader) · MeshBuilder · Character (lofted humanoid) · World (maps, collision, building) ·
Items (Season 1 loot table – edit numbers there) · Game (lobby, match, bots, storm) · Hud
