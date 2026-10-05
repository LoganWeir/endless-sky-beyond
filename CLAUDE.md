# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Endless Sky: a C++20 / SDL / OpenGL 2D space-trading game. Game logic lives in `source/`, and almost all game content (ships, outfits, missions, systems, UI layouts) is plain-text data in `data/` parsed at runtime. Upstream is `endless-sky/endless-sky`; this repo is a fork of it. Full build instructions are in `docs/readme-developer.md`; contribution rules are in `docs/CONTRIBUTING.md`.

Note: upstream's CONTRIBUTING.md states that AI-generated or AI-assisted content is forbidden in Endless Sky development. Keep this in mind for anything intended to be sent upstream.

## Build, test, lint

CMake with presets (CMake 3.21+). On this machine (macOS, Apple Silicon) the preset is `macos-arm`; other presets are `macos`, `linux`, `linux-gles`, `linux-armv7`, `clang-cl`, `mingw`, `mingw32`. Dependencies on macOS come from Homebrew (`brew install cmake ninja mad libpng jpeg-turbo sdl2 minizip libavif catch2 flac`).

```bash
cmake --preset macos-arm                                   # configure once; add -DES_USE_SDL3=ON for SDL3
cmake --build --preset macos-arm-debug                     # build game + unit tests
cmake --build --preset macos-arm-debug --target EndlessSky # game only
ctest --preset macos-arm-test                              # unit tests (Catch2)
ctest --preset macos-arm-benchmark                         # Catch2 [!benchmark] cases
ctest --preset macos-arm-integration                       # integration tests (CI runs these on Linux only)
ctest --preset macos-arm-integration-debug -R <name>       # one integration test, with a visible window
ctest --preset macos-arm-integration-debug -N              # list integration tests
```

Binaries land in `build/<preset>/Debug/` (or `Release/`). Debug builds enable ASan/UBSan; Release uses LTO.

Run a single unit test directly with Catch2 filters:

```bash
build/macos-arm/Debug/EndlessSkyTests "Scenario name"      # or a [tag]; -l lists all
```

Data validation without a GUI (these are what CI's "Data Files" job runs):

```bash
build/macos-arm/Debug/endless-sky -p                       # parse data + most recent save, report errors
build/macos-arm/Debug/endless-sky --parse-assets           # also load every image/sound and check references
build/macos-arm/Debug/endless-sky -p --config tests/integration/config   # parse the integration-test plugin
```

Other useful flags: `--test <name>` runs one integration test, `--tests` lists them, `-d` enables debug mode, `--rngseed <n>` fixes RNG, `-r <path>` / `-c <path>` override resource and config dirs.

Style and project checks (all run in CI via `.github/workflows/projects_check.yml`):

```bash
python3 utils/check_code_style.py            # C++ formatting rules (needs `pip install regex`); accepts glob args
python3 utils/check_content_style.py         # data/*.txt formatting per utils/contentStyle.json; -a auto-corrects
python3 utils/check_copyright.py             # every asset must be attributed in `copyright`
./utils/check_cmake.sh                       # every source/test file must be listed in a CMakeLists.txt
./utils/check_shaders.sh
editorconfig-checker                         # .editorconfig compliance
codespell                                    # uses .codespell.exclude / .codespell.words.exclude
```

## Adding files

`source/CMakeLists.txt` and `tests/CMakeLists.txt` list every file explicitly (no globbing). When adding a `.cpp`/`.h` under `source/`, a unit test under `tests/unit/`, or an integration test `.txt`, add it to the matching list or `check_cmake.sh` fails.

## C++ style (enforced by `utils/check_code_style.py`)

Rules follow the wiki C++ Style Guide; the checker enforces the mechanical ones:

- Tabs for indentation, LF line endings, ASCII charset, no trailing whitespace.
- Every file starts with the GPL header block (`/* FileName.h` + `Copyright (c) YEAR by NAME` + license text); copy it from any existing file. Headers use `#pragma once`.
- Braces on their own line after `if`/`for`/`while`/`switch`/`else`. No space between the keyword and `(`: write `if(x)`, `for(...)`. `try {` and `do {` keep the brace on the same line.
- Spaces around binary operators, after commas; no space inside parentheses; no `(void)` parameter lists.
- Includes: a `.cpp` includes its own header first, then a blank line, then project `"..."` includes alphabetically, blank line, then `<...>` system includes alphabetically. Forward-declare classes in headers in alphabetical order.
- Three blank lines between function definitions (see any `.cpp`).
- Compiler flags are `-Wall -pedantic-errors -Wold-style-cast`; Release builds use `-fno-rtti`, so avoid `dynamic_cast`/`typeid`.

## Architecture

**Startup (`source/main.cpp`).** Parses flags, calls `GameData::BeginLoad` (asynchronous via `TaskQueue`), then either runs a console-only task (`-p`, `--parse-assets`, `--tests`, print-data) and exits, or opens the SDL window and enters `GameLoop`. `GameLoop` owns two `UI` stacks: `menuPanels` (loading, pilot creation, preferences, load/save) and `gamePanels` (flight and everything on top of it). Menu panels take priority while non-empty.

**Panels and UI.** Everything drawn is a `Panel` (`source/Panel.h`), stacked in a `UI` (`source/UI.h`). Events go to the top panel; drawing starts from the bottom. `MainPanel` is the flight view and delegates simulation to `Engine`. Most screen layouts are not hard-coded: `Interface` objects are defined in `data/_ui/interfaces.txt` and filled from an `Information` object at draw time, so UI positioning changes often belong in data, not C++.

**Engine vs. drawing.** `Engine` (`source/Engine.h`) steps all ships/projectiles/effects at 60 Hz on a separate calculation thread, one step ahead of the render thread, filling `DrawList`/`BatchDrawList` that the shaders in `source/shader/` consume. `AI` issues commands to non-player ships; `Ship` is used for both player and NPC ships.

**Game data.** `GameData` is a global static store of everything loaded from `data/` and plugins; `UniverseObjects` holds the universe (systems, planets, governments, fleets, missions, ships, outfits, etc.) and its `LoadFile` dispatches on each top-level node keyword (`ship`, `outfit`, `system`, `mission`, `event`, `test`, ...). Game events mutate this state at runtime, so `GameData` keeps a pristine copy and reverts before loading a different pilot. Resource sources are the base `data/` folder plus every enabled plugin folder or `.zip` from the global and user plugin directories (`PluginManager`, `GameData::LoadSources`); later sources override earlier definitions by name.

**Data file format.** `DataFile`/`DataNode` (`source/DataFile.h`, `source/DataNode.h`): indentation-based hierarchy, whitespace-separated tokens, quotes to group words, backticks when a token contains quotes, `#` comments. Each data object has a `Load(const DataNode &)` and often a `Save(DataWriter &)`. Content style for these files (tabs, quoting, spacing) is checked by `check_content_style.py` against `utils/contentStyle.json`.

**Player state and conditions.** `PlayerInfo` holds the pilot: ships, accounts, visited systems, active missions, universe changes, and handles save/load. `ConditionsStore` is the key/value (int64) condition system that missions, conversations, and events test and set; "derived" conditions are provided on demand by code (e.g. `flagship planet: Mars`), and integration tests assert against them.

**Gamepad.** Controller support (merged from upstream PR #12016, not in upstream master) lives in `source/gamepad/`. `GamePad` wraps SDL game-controller state; `UI` turns axis and button events into `Panel::Controller*` virtuals, falls back to a zone-snapping `GamepadCursor` for panels without native handling, and `RadialSelectionPanel` is the in-flight radial menu, opened by the rebindable "Open radial menu" command (LB by default). Command-to-button bindings load from `controller.txt` in the config dir, then the repo root, then built-in defaults in `GameData::LoadSettings`; the file format is `"<command description>" controller_button N` or `controller_trigger AXIS 0|1`. SDL mappings and dead zones persist separately via `GamePad::SaveMapping`/`SaveConfig`. The gamepad code targets SDL2 and is not guarded for the SDL3 build.

**Platform/IO.** `Files` abstracts resource and config paths per OS. `GameWindow` wraps the SDL window. SDL2 is default; `ES_USE_SDL3` switches to SDL3 with `#ifdef ES_USE_SDL3` branches at the few call sites that differ.

## Tests

- **Unit tests** (`tests/unit/src/test_*.cpp`): Catch2, link against `EndlessSkyLib` objects. Start from `tests/unit/src/test_template.txt`. Wrap everything in an anonymous namespace, prefer `SCENARIO`/`GIVEN`/`WHEN`/`THEN`, use `CHECK` for probing and `REQUIRE` for preconditions. There is no dependency injection: anything that touches `GameData`, SDL, or OpenGL is effectively untestable at the unit level, so target self-contained classes.
- **Integration tests** (`tests/integration/config/plugins/integration-tests/data/tests/tests_*.txt`): written in the game's data language as `test` nodes with a `sequence` of steps (`inject`, `call`, `navigate`, `input`, `assert`, ...). They run the real game headless from a scratch config dir. Shared helpers and save-game fixtures live in `tests_common.txt`; the plugin also disables flaky missions in `disabling troublesome missions.txt`. Add new test files to the `INTEGRATION_TESTS` list in `tests/CMakeLists.txt`.
- Prefer a unit test over an integration test when coverage would be equal.

## Content and assets

- Any new or modified artwork/sound must have a license entry in `copyright` (DFSG-compatible licenses only; see CONTRIBUTING.md). `check_copyright.py` enforces the listing.
- Image assets live in `images/`; `@2x` variants and source files go to the separate `endless-sky-high-dpi` and `endless-sky-assets` repos.
- `changelog` is UTF-8 with 2-space indentation (unlike the rest of the repo, which is ASCII with tabs).
