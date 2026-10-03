# Pixel engine starter

Custom C++20 engine on SDL3 for pixel-art games, Windows first, iPhone later (SDL3 uses Metal on iOS).
No game is in this repo right now; it is a clean starting point.

## What's here
- `engine/pix.*`    pixel renderer: 480x270 logical canvas, integer-scaled with nearest filtering, rects, text,
                    `Canvas` (CPU pixel buffer) -> `Tex` baking so terrain/sprites can be drawn from code, streaming minimap texture,
                    screenshot readback (`Pix::screenshot`) for visual checks
- `engine/audio.*`  software synth + procedural music, thread-safe `play()`, no audio files (add `Sfx` entries as needed)
- `engine/font5x7.h` embedded 5x7 bitmap font (uppercase, digits, light punctuation)
- `engine/mathx.h`  `Vec2`, `Rng` (seedable xorshift), angle helpers; `engine/color.h` float RGBA
- `tools/build.bat` MSVC + Ninja build (finds Visual Studio 2026 Community, CMake, Ninja); `CMakeLists.txt` fetches SDL3 3.4.16 statically

## Conventions that worked well
- Keep the simulation in pure C++ with no SDL (fixed 120 Hz step) so it can run headless: unit tests + a bot player + `--ff N` fast-forward.
- Add test flags to the app: `--bot`, `--seed`, `--shot out.png --after SECS`, `--ff N`, `--perf`; tests must never touch the player's save.
- Verify visuals by screenshot, not by assumption. Save files go to `SDL_GetPrefPath`.

## iOS plan
GitHub Actions macOS runner + fastlane to TestFlight, copying The Fort's flow (`the-fort/CI_SETUP.md`,
`.github/workflows/ios.yml`, `native/ios/fastlane/Fastfile`, shared Apple team secrets). Not started.
