# Implementation Plan: Game Shell

- [x] 1. Scaffold the project
  - Create `CMakeLists.txt`, `CMakePresets.json` (`dev`, `release`, `asan`, `web`), `cmake/Dependencies.cmake` with pinned `FetchContent` dependencies, `cmake/Warnings.cmake`, `.clang-format`, and `.clang-tidy`.
  - Add the `agentscape` app target, the `property_tests`, `unit_tests`, `integration_tests`, and `smoke_test` targets with CTest labels, and the `format` and `tidy` targets.
  - Add `.gitignore` (must NOT ignore `.kiro/`; ignore `build/`), `web/shell.html`, and `docs/ASSETS.md`.
  - Add the GitHub Actions CI: a desktop matrix (Windows MSVC, macOS Clang, Linux GCC), `asan` on Linux, and a `web` build.
  - _Requirements: all (foundation)_

- [x] 2. Build the app skeleton
  - [x] 2.1 Implement `main.cpp` with SDL3 main callbacks, `app::App`, `app::Config`, the SDL window and renderer (software fallback), and ImGui with the SDL3 and SDL_Renderer backends.
    - _Requirements: 1.2, 6.1_
  - [x] 2.2 Implement `render::GameFrame`: the 765×503 target texture, integer-scaled present with letterbox, and pointer-coordinate conversion.
    - _Requirements: 1.1–1.5_
  - [x] 2.3 Implement `app::EventBus` and `app::ScreenStack`.
    - _Requirements: 5.4_

- [x] 3. Set up local data
  - [x] 3.1 Implement `db::Database`, `Statement`, and `Transaction` (RAII), the embedded migration runner using `PRAGMA user_version`, and `0001_init.sql` (`user`, `app_state`, `activity_event`).
    - _Requirements: 8.1, 8.2_
  - [x] 3.2 Implement `platform::paths` and `platform::persist` (IDBFS mount, load-before-open, debounced flush, `pagehide`).
    - _Requirements: 8.1, 8.3, 8.4_
    - _Note: the IDBFS path is compiled only under Emscripten and is verified at the task 11 checkpoint (web build + reload)._
  - [x] 3.3 Write an idempotent seed loader skeleton for `data/seed/*.json` and the `--reseed` and `--reset-data` flags.
    - _Requirements: 8.1_
    - _Seed rows use stable `seed:<kind>:<slug>` ids (never valid ULIDs, so they can't collide with player rows). Bump `db::SEED_VERSION` when `data/seed/` changes. agent-codex and quest-logs add their files to `KNOWN_FILES` in `src/db/seed_loader.cpp`._

- [x] 4. Implement shell domain logic
  - [x] 4.1 Implement `computeFrameScale`, `layoutFrame`, `toFramePoint`, `parseKiroweenSetting`, `resolveKiroweenSetting`, `isKiroweenActive`, `buildMenu`, and `appendMessages` in `src/domain/`.
    - _Requirements: 1.1, 1.5, 4.1, 4.2, 5.4, 5.5, 6.1–6.3_
  - [x]* 4.2 Write property test for frame scale (Property 1).
    - **Property 1: Frame scale is the largest integer that fits**
    - **Validates: Requirement 1.1**
  - [x]* 4.3 Write property test for Kiroween resolution (Property 2).
    - **Property 2: Kiroween resolution**
    - **Validates: Requirements 6.1, 6.2, 6.3**
  - [x]* 4.4 Write property test for context menu shape (Property 3).
    - **Property 3: Context menu shape**
    - **Validates: Requirements 4.1, 4.2**
  - [x]* 4.5 Write property test for chat log append (Property 4).
    - **Property 4: Chat log append**
    - **Validates: Requirements 5.4, 5.5**

- [x] 5. Build UI primitives
  - Add the theme constants and `ui::applyTheme`, load the OFL pixel and plain fonts, and build `StonePanel`, `PixelButton`, `ShadowText`, `RichText` (with `parseMarkup`), and `HoverText`.
  - Add the dev-only UI gallery screen (`AS_ENABLE_DEV_SCREENS`).
  - _Requirements: 1.4, 7.2, 7.3_
  - _Notes: fonts are Silkscreen (pixel, native 8 px) and Atkinson Hyperlegible (plain). Text inside parchment panels switches to dark colours (`ui::textColorOn`) because the light text colours are below 2:1 on PARCHMENT. Visual QA: `agentscape --screen=ui-gallery --snapshot=gallery.png`._

- [x] 6. Build the TabPanel and Settings
  - Build the TabPanel with a Settings tab that persists toggles through `ProfileService.updateSettings`, plus "Log out". In the web build, default "Reduce motion" from `prefers-reduced-motion`.
  - _Requirements: 2.8, 7.1, 7.2, 7.5_
  - _Note: only the Settings tab exists; Equipment/Skills/Quest Log tabs arrive with their specs. The panel is shown once signed in._

- [x] 7. Build the login screen
  - [x] 7.1 Build the login screen with the stone panel, "Play as Guest", the local User list, the remembered-User auto sign-in, error display, and Kiroween bats when active. Add `ProfileService.createGuest` and `cleanupGuests`.
    - _Requirements: 2.1–2.3, 2.6, 2.7, 8.5_
    - _Note: login panel, Play as Guest, continue-as list, remembered-player auto sign-in, guest cleanup, and settings-apply-on-sign-in are done. Kiroween bats come with task 10 (login effects). Everyone lands on World until character-creator adds the "no Adventurer -> Costumer" branch (Req 2.6)._
  - [x] 7.2 Implement `platform::github_device_flow` (desktop only; libcurl on a worker thread; token discarded after the profile fetch) and the code-display panel. Hide the button on web or when `AS_GITHUB_CLIENT_ID` is empty.
    - _Requirements: 2.4, 2.5, 2.7_
    - _Note: libcurl is fetched only when `AS_GITHUB_CLIENT_ID` is set (so the default/CI-restricted build stays curl-free). Worker-thread HTTP lives in `github_device_flow_curl.inc` behind `AS_HAVE_CURL`; the token is used once for GET /user, then dropped. Parsing/timing is pure in `domain::device_flow`._
  - [x]* 7.3 Write unit tests for device-flow response parsing, the `slow_down` interval, expiry, and denial.
    - _Requirements: 2.4, 2.7_

- [x] 8. Build the ContextMenu and Chatbox
  - [x] 8.1 Implement `ContextMenuHost` and `ContextMenuModel` with pointer, long-press, `Menu`/`Shift+F10`, and gamepad opening, keyboard and gamepad navigation, and Examine → Chatbox.
    - _Requirements: 4.1–4.5_
    - _Note: ContextMenuModel (open/navigate/confirm/cancel, wrapping, Cancel = no side effect) is pure and unit-tested; ContextMenuHost drives it from ImGui (arrows/Enter/Esc, D-pad/A/B, click, click-outside). Screens request it via `app::ContextRequest`. Touch long-press (≥500 ms) pending the World_Map input layer (task 9)._
  - [x] 8.2 Implement `Chatbox` with tabs, `ui::pushGameMessage`, the initial load of 20 ActivityEvents, and the `ActivityPosted` subscription.
    - _Requirements: 5.1–5.6_
    - _Note: pure `ChatModel` (dedupe/cap-100/filter) + `Chatbox` widget subscribing to ActivityPosted and GameMessage; `ActivityService.post/latest` publishes after commit. Welcome + 20 recent Tavern events load on sign-in._
  - [x]* 8.3 Write unit tests for context menu navigation and Chatbox filtering.
    - _Requirements: 4.5, 5.2_

- [x] 9. Build the Promptmoor world map
  - [x] 9.1 Draw original pixel-art map and building sprites, and log them in `docs/ASSETS.md`.
    - _Requirements: 3.1_
    - _Note: single 380×300 composed map PNG from `tools/gen_map_art.py` (deterministic). Animated overlays (fog/pumpkins/bats) and the standing Adventurer come with task 10 / character-creator._
  - [x] 9.2 Implement `WorldMap` with hotspot polygons, hover text, left-click default, and right-click menu.
    - _Requirements: 3.1–3.5_
    - _Note: pure `domain::geometry` (point-in-polygon, bbox, unit-tested) + `game::hotspots` data; the World screen draws the map, outlines the hovered hotspot, shows hover text, left-clicks the default action, right-clicks/Menu-key opens the context menu._
  - [x] 9.3 Add the keyboard- and gamepad-navigable hotspot list beside the map.
    - _Requirements: 3.6, 7.3, 7.4_
    - _Note: the Places list (right column of the viewport) is driven by the same hotspot data; each entry offers the default action and opens the menu via Menu/Shift+F10/gamepad Y. Graveyard appears only during Kiroween._

- [x] 10. Add Kiroween mode
  - Add jack-o'-lantern overlays, the fog overlay (static when "Reduce motion" is on), the Graveyard hotspot, and accent palette swaps.
  - _Requirements: 6.1–6.5_
  - _Note: pure `game::kiroween_fx` (fog drift, bat flight, pumpkin flicker; reduce-motion = static) is unit-tested; `game::KiroweenLayer` renders fog + pumpkins on the World map and bats on the login screen. Graveyard hotspot + accent theme already wired (tasks 9 and 5). Resolution/precedence was task 4 (Property 2)._

- [ ] 11. Checkpoint: shell smoke test
  - Add the headless smoke test: guest login → World → open the Tavern hotspot's context menu → Enter. Save screen snapshots.
  - Confirm the `web` preset builds and runs in Chrome and Firefox, and that data survives a page reload.
  - Make sure all tests pass, and ask the user if questions arise.
  - _Requirements: 2.3, 3.3, 3.4, 8.4_
