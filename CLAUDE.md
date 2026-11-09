# SCP-idTech3 agent notes

## Project

A Win32 (x86) CMake build of the RTCW single-player engine. Modules: engine/ (wolf exe), gamedll/game|cgame|ui|botlib (DLLs), tools/bspc, tools/extractfuncs. Third-party code lives in external/ and is never edited.

## Build facts

- Generator: Visual Studio 17 2022, architecture Win32. Presets in CMakePresets.json: cmake --preset visual-studio then cmake --build --preset visual-studio-release.
- Release compiles with /W4, Debug with /W3. Zero warnings is the bar; treat every warning as a fix item, not noise.
- Engine exe: bin/WolfSP.exe. Game DLLs: main/qagamex86.dll, cgamex86.dll, uix86.dll, botlibx86.dll.
- Shared config (defines, warning level, output dirs) is the configure_target() function in the root CMakeLists.txt.

## CI rules

- CI runs on every push unless the commit message contains [skip ci]. Intermediate commits on a branch should carry [skip ci]; only the final push of a change lets CI run.
- After any CI run, read build-digest.md first (repo root on main, ci-logs/ branch otherwise), then the matching section of build-windows.log. Fix ALL errors AND warnings, commit, push, repeat until green with zero warnings.
- Never edit build-digest.md, build-windows.log or any CI-published file by hand.
- Do not edit anything under external/ or references/. References are read-only study material.

## Workflow

- Work on a feature branch, open a PR into main, squash-merge with a plain-English commit message.
- Commit messages: short plain-language summaries, no ticket noise.
- docs/ holds one progress file per milestone, stamped with the finish time in 12-hour format. Update the relevant file when a milestone completes.

## Code conventions

- Comments only where they earn their place, in plain English.
- Keep the existing file layout: engine code in engine/, game code in gamedll/, tools in tools/. Never move third-party code out of external/.
