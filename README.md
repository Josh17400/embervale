# TAILSPIN

Encirclement arena roguelite. Steer a comet; its tail is your weapon. Loop the tail around enemies to
implode them, chain blasts through the crowd, level up, survive the bosses.

## Layout
- `engine/`   custom engine pieces: math, `Gfx` (2D renderer over SDL3), `Audio` (software synth + music)
- `game/`     `game.*` pure simulation (no SDL, fixed 120 Hz), `render.*` camera/particles/HUD/menus, `bot.h` autoplay
- `main.cpp`  SDL3 platform layer: window, mouse/keyboard/touch, fixed-timestep loop, save file
- `tools/`    `sim.cpp` headless tests + bot runs, `build.bat` MSVC/Ninja build

## Build / run (Windows)
    tools\build.bat                      # builds build\tailspin.exe and build\tailspin_sim.exe
    build\tailspin_sim.exe --test        # unit tests (loop geometry, chain blast, level-up flow)
    build\tailspin_sim.exe 600 1 5       # bot plays 5 runs (seconds, seed, runs) -> balance numbers
    build\tailspin.exe --bot --shot out.png --after 20   # autoplay + screenshot (visual checks)

Controls: mouse steers, Space / right click boosts, A/D or arrows turn, Esc pauses, F11 fullscreen.
Touch (iOS): first finger = floating stick, second finger = boost.

## iOS plan
Game code is platform-neutral C++; SDL3 provides window/GPU/audio/touch on iOS (Metal). Builds run on a
GitHub Actions macOS runner and ship to TestFlight with fastlane, the same flow as The Fort
(`the-fort/CI_SETUP.md`, `.github/workflows/ios.yml`, `native/ios/fastlane/Fastfile`): manual
`workflow_dispatch`, shared Apple Developer team secrets (ASC_KEY_*, APPLE_TEAM_ID, IOS_CERT_P12_B64,
CERT_EXPORT_PASS), run number = build number. Differences: this repo has an Xcode project that builds
the C++ core + SDL3 (CMake -G Xcode) instead of Capacitor. Not started yet: Windows build comes first.

## HOLDLINE (second game, same engine) - pixel open-world merge-and-defend
`holdline.exe`: 480x270 pixel canvas (integer scaled), procedural 224x144-tile world, fog of war, hero camera,
villages with inns, wild units to find, drag-merge in the world, goblin camps, raids, autosave.
Files: `engine/pix.*` (pixel renderer + sprite baking), `game2/mg_game.*` (sim), `game2/mg_render.*` (look),
`game2/mg_bot.h`, `main_mg.cpp`, `tools/mg_sim.cpp` (`holdline_sim --test`, `holdline_sim 900 1`).
App flags: `--bot --god --ff N --seed S --shot out.png --after SECS --perf --novsync` (test runs never touch the save).
Save: SDL pref path `Josh17400/Holdline/save.bin`.
