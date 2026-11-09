# Doom3BFG-Pro agent notes

## Project

Darklight: a Win32 (x86) CMake build of the DOOM 3: BFG Edition engine with idTech 5 work on top (AAS2 pathfinding, virtual texturing, idStudio editor integration). Engine code lives in neo/ (idlib, framework, renderer, cm, sound, swf, sys, ui, aas2file, gamelib, tools). The game (neo/game) builds as an object library and links straight into the exe; there are no game DLLs. Third-party code lives in neo/extern/ (zlib, jpeg-6, jxrlib) and is never edited. Game data lives in base/.

## Build facts

- The CMake source directory is neo/; the repo root has no CMakeLists.txt. Configure with: cmake -S neo -B out -A Win32. cmake_msvc.cmd wraps this and picks the installed VS generator (2019/2022/2026).
- No hardcoded generator: the installed Visual Studio is the one that works, whatever version ships.
- Compiles with /W3; C4244, C4267, C4305 and C4996 are disabled project-wide in neo/CMakeLists.txt (darklight_configure_target). Treat any remaining warning as a fix item, not noise.
- Static runtime (/MT) by default; toggle with DARKLIGHT_MSVC_STATIC_RUNTIME.
- idStudio (MFC) is OFF by default; enable with -DDARKLIGHT_BUILD_IDSTUDIO=ON (Windows + MSVC only).
- Exe output: out/bin/Darklight.exe, every config. The engine runs from the repo root (base/ next to it).
- Only the Windows platform layer exists (neo/sys/win32, XAudio2 sound backend). There is no posix/linux/osx/macos system layer yet, so non-Windows builds cannot link; CI is Windows-only until one lands.

## CI rules

- CI runs on every push unless the commit message contains [skip ci]. Intermediate commits on a branch should carry [skip ci]; only the final push of a change lets CI run.
- After any CI run, read build-digest.md first (repo root on main, ci-logs/ branch otherwise), then the matching section of build-windows.log. Fix ALL errors AND warnings, commit, push, repeat until green with zero warnings.
- Never edit build-digest.md, build-windows.log or any CI-published file by hand.

## Workflow

- Work on a feature branch, open a PR into main, squash-merge with a plain-English commit message.
- Commit messages: short plain-language summaries, no ticket noise.
- docs/ holds one progress file per milestone, stamped with the finish time in 12-hour format. Update the relevant file when a milestone completes.

## Code conventions

- Comments only where they earn their place, in plain English.
- Keep the existing file layout: engine code in neo/, game code in neo/game, tools in neo/tools. Never move third-party code out of neo/extern/.
