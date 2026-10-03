# HOLDLINE

Pixel open-world merge-and-defend. Explore a fogged world with a hero, find sleeping allies, drag matching
units together to merge them into stronger ones, house them in village inns, clear goblin camps and hold
off raids. Custom C++20 engine on SDL3, built for Windows first and iPhone later.

## Layout
- `engine/`  `pix.*` pixel renderer (480x270 canvas, integer scaled, sprites/terrain baked from code),
             `audio.*` software synth + procedural music (no audio files), `font5x7.h`, `color.h`, `mathx.h`
- `game2/`   `mg_game.*` pure simulation (no SDL), `mg_render.*` look/HUD/fog, `mg_bot.h` autoplay bot
- `main_mg.cpp`  SDL3 platform layer: window, keyboard/mouse/touch, fixed 120 Hz timestep, autosave
- `tools/`   `mg_sim.cpp` headless tests + bot runs, `build.bat` (MSVC + Ninja)

## Build / run (Windows)
    tools\build.bat                       # build\holdline.exe and build\holdline_sim.exe
    build\holdline_sim.exe --test         # world gen, reachability, merge rules, recruit, inn, save/load
    build\holdline_sim.exe 900 1          # bot plays 15 min on seed 1 (LOG=1 prints events)
    build\holdline.exe --bot --ff 120 --shot out.png --after 2   # simulate 120s, then screenshot

Flags: `--bot --god --ff N --seed S --shot FILE --after SECS --perf --novsync --fresh`
(test runs with --bot/--shot/--seed never read or write your save).
Controls: WASD move, drag a unit onto a matching one to merge, M merge all, E lodge party at an inn,
B hire at the home inn, click a unit to lodge/join, Esc pause, F11 fullscreen. Touch: finger on empty
ground steers, finger on a unit drags it.
Save: SDL pref path `Josh17400/Holdline/save.bin`.

## iOS plan
Game code is platform-neutral C++; SDL3 provides window/GPU/audio/touch on iOS (Metal). Builds run on a
GitHub Actions macOS runner and ship to TestFlight with fastlane, the same flow as The Fort
(`the-fort/CI_SETUP.md`, `.github/workflows/ios.yml`, `native/ios/fastlane/Fastfile`): manual
`workflow_dispatch`, shared Apple Developer team secrets, run number = build number. Not started yet.
