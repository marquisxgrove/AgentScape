---
inclusion: always
---

# Technology Stack

## Language and toolchain

- **C++20** for all application code. C is used only by third-party libraries (SDL3, SQLite).
  - Supported compilers: MSVC (Visual Studio 2022 17.8+), Clang 17+, GCC 13+.
  - Build with warnings as errors: `/W4 /permissive-` on MSVC, `-Wall -Wextra -Wpedantic -Wconversion -Wshadow` on Clang and GCC.
- **CMake 3.25+** with **CMakePresets.json** and the **Ninja** generator. Presets: `dev` (Debug), `release`, `asan` (AddressSanitizer + UBSan), and `web` (Emscripten).
- **Emscripten 4.x** (emsdk) for the WebAssembly build. The `web` preset finds the toolchain through `$EMSDK`.
- Dependencies are fetched with CMake `FetchContent`, pinned to exact tags or commits in `cmake/Dependencies.cmake`. Don't vendor third-party source into `src/`.

## Libraries

| Library | Use |
|---|---|
| **SDL3** | Window, input (mouse, keyboard, touch, gamepad), events, timing, audio, clipboard, file dialogs, `SDL_OpenURL`, `SDL_GetPrefPath` |
| **SDL3_image** | Loads PNG sprites. Indexed (8-bit) PNGs keep their palette, which the palette-swap recolouring depends on. Also saves PNG exports. |
| **Dear ImGui** | All UI, through the `imgui_impl_sdl3` and `imgui_impl_sdlrenderer3` backends, with a custom retro theme and widgets |
| **SQLite 3** (amalgamation) | Local database, wrapped in a small RAII layer in `src/db/` |
| **nlohmann/json** | JSON columns, seed data, and sprite atlas metadata |
| **tl::expected** | `as::Result<T, E>`. We target C++20, so `std::expected` isn't available yet. |
| **libcurl** (desktop only) | HTTPS calls for the GitHub device-flow login |
| **Catch2 v3** + **RapidCheck** | Unit tests and property-based tests |

## App model

- Use **SDL3 main callbacks** (`SDL_MAIN_USE_CALLBACKS`: `SDL_AppInit`, `SDL_AppEvent`, `SDL_AppIterate`, `SDL_AppQuit`), so the same code runs on desktop and in the browser without a blocking loop.
- The app is single-threaded. Database calls run synchronously on the main thread because the data set is small. The only background work is the GitHub device-flow HTTP polling, which runs on a worker thread and posts results back as SDL user events.
- Rendering uses **`SDL_Renderer`**, not SDL_GPU, because it works the same on desktop and WebGL.
  - The whole game frame, including ImGui, is drawn into a **765×503 render target texture** (times a render resolution of 1 to 4, with `SDL_SetRenderScale`, so code still works in frame pixels and fonts are rasterised at that resolution) and then presented scaled to fit the window ("sharp bilinear": a stretch of less than 2× with linear filtering), or at the largest integer scale with `SDL_SCALEMODE_NEAREST` when "Fullscreen" is off.
  - Every texture uses `SDL_SCALEMODE_NEAREST`. ImGui anti-aliasing is off (`AntiAliasedLines`, `AntiAliasedFill`, and `AntiAliasedLinesUseTex` are all false).
  - Palette-swap recolouring happens on the CPU: the app swaps `SDL_Palette` entries on 8-bit indexed surfaces, composites the layers, and caches the result as a texture.

## Data

- **SQLite**, one database file per install:
  - Desktop: `SDL_GetPrefPath("AgentScape", "AgentScape")/agentscape.db`, or `--data-dir`.
  - Web: `/persist/agentscape.db` on an Emscripten **IDBFS** mount. The app syncs it to IndexedDB after every committed write transaction (debounced to 250 ms) and on `pagehide`.
- Tables are `STRICT`. Run `PRAGMA foreign_keys = ON` on every connection. Desktop uses `journal_mode=WAL`, and web uses `journal_mode=DELETE`.
- **Migrations** are numbered SQL files in `data/migrations/` (`0001_init.sql`, …). They're embedded at build time and applied in order inside a transaction, and `PRAGMA user_version` tracks progress. Never edit a migration after it's merged. Add a new one instead.
- Store JSON as `TEXT`. Validate it with the domain validators on every read and write.
- Timestamps are ISO-8601 UTC `TEXT`. IDs are ULIDs (`TEXT`) generated in `src/core/ids`.
- **Always bind parameters** (`sqlite3_bind_*`). Never build SQL by concatenating strings.
- **Seed data** lives in `data/seed/*.json` (Agents, NPC adventurers, sample Quest Logs and Notes) and is embedded at build time. Seeding runs on first launch and with `--reseed`, and it must be idempotent: upsert by slug or natural key.
- There is **no server**. The Codex is a seeded community snapshot plus the player's local contributions.

## Auth (local profiles)

- A **User** is a local player profile in the database. `kind` is `PLAYER`, `GUEST`, or `NPC` (seeded community members). `role` is `USER` or `KEEPER` (local moderator).
- **Play as Guest** is always available. Each press creates a fresh, isolated `GUEST` user seeded just below Questing level 14. The new Guest is remembered on this device and signed back in at launch until it Logs out. At startup, the app deletes Guest users that haven't been used for 24 h, except the remembered one.
- **Create a profile** is always available too: a permanent local `PLAYER` with a chosen name (Adventurer name rules; unique against other Players and the seeded NPCs, ignoring case and separators; no reserved names, nothing that reads as `Guest-…`). There is no password, because all data is local to the device. `ProfileService::createPlayer`.
- **Sign in with GitHub** (desktop only) uses the **OAuth device flow**. It needs only a public client ID, never a client secret.
  - The client ID also pulls in libcurl, built with the platform TLS (Schannel / Secure Transport / OpenSSL) and HTTP only. Release builds read it from the repository variable `AS_GITHUB_CLIENT_ID`.
  - Failures show the player one game-voice line; the log names the step, the HTTP status and GitHub's `error` code, and never a response body or token.
  - The flow fetches `GET /user` once, then **discards the access token**. We store only `github_id`, `github_login`, and the display name.
  - The web build hides this button because GitHub's device-flow endpoints don't allow browser (CORS) requests.
  - The OAuth App must have **Enable Device Flow** turned on.

## Validation and errors

- Domain validators (`validateName`, `validateLoadout`, `validateQuestLogInput`, …) check every input at the service boundary and every JSON column on load.
- Services return `as::Result<T, GameError>`. Don't throw across layers. Exceptions are allowed only inside `src/db` and `src/platform`, and they're converted to `GameError` at those boundaries.
- User-facing error text uses the game voice (for example, "You can't do that right now."). Technical detail goes to the log through `SDL_Log*`.

## UI and text

- All UI is **Dear ImGui** with a custom theme in `src/ui/theme.*`. Use zero rounding everywhere, and draw panels, buttons, and frames with the retro widgets in `src/ui/`. Don't use stock ImGui window chrome.
- **Never pass user text as a format string.** Render it with `ImGui::TextUnformatted` (or the `ui::` wrappers built on it).
- Fonts are OFL TTF files in `assets/fonts/`. They're rasterised at the font's native pixel size, with no oversampling and pixel snapping on. Text input accepts UTF-8, and glyphs outside the font's ranges render as the fallback glyph.
- Turn on keyboard and gamepad navigation (`ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad`).

## Testing

- **Catch2 v3** for unit and integration tests. **RapidCheck** (with its Catch2 integration) for property-based tests of all pure domain logic in `src/domain/`.
- Integration tests run services against an in-memory SQLite database (`:memory:`) with all migrations applied.
- **Headless smoke tests** (`SDL_VIDEO_DRIVER=dummy`, software renderer) run scripted paths through the real app (`src/app/smoke_runs.cpp`, selected by a `--smoke-*` flag) and save screen snapshots as CI artifacts.
- See `testing.md` for the full rules.

## Quality

- `clang-format` (repo `.clang-format`) and `clang-tidy` (repo `.clang-tidy`, with `bugprone-*`, `cppcoreguidelines-*`, `modernize-*`, and `performance-*` enabled, plus a few documented exclusions).
- No raw `new` or `delete`. Use RAII wrappers for every SDL, SQLite, and ImGui resource. Prefer `enum class`, `std::span`, `std::string_view`, and `std::variant` over flags and raw pointers.
- CI runs the `asan` preset on Linux and macOS.

## Configuration

| Setting | How to set it | Purpose |
|---|---|---|
| Data directory | `--data-dir <path>` or `AGENTSCAPE_DATA_DIR` | Where `agentscape.db` lives (desktop) |
| Kiroween mode | `--kiroween=auto\|on\|off`, then `KIROWEEN_MODE`, then the web URL query `?kiroween=`, then the build default | Forces the seasonal layer on or off |
| `AS_KIROWEEN_DEFAULT` | CMake option (`auto`, `on`, or `off`; default `auto`) | Baked-in default for judging builds |
| `AS_GITHUB_CLIENT_ID` | CMake option or CI variable | Public OAuth App client ID for device flow. If it's empty, the GitHub button is hidden. |
| `AS_ENABLE_DEV_SCREENS` | CMake option (`ON` in `dev`) | Enables the UI and avatar gallery screens for visual QA |
| Reseed | `--reseed` | Re-runs the idempotent seed |
| Reset | `--reset-data` | Backs up and recreates the local database |
| Repair XP | `--repair-xp` | Recomputes `user.total_xp` from the XP ledger |

## Common commands

| Command | What it does |
|---|---|
| `cmake --preset dev` | Configure a Debug build in `build/dev` |
| `cmake --build --preset dev` | Build the app and all tests |
| `ctest --preset dev` | Run all tests |
| `ctest --preset dev -L property` | Run only property tests |
| `cmake --build --preset dev --target format` | Run `clang-format` on all sources |
| `cmake --build --preset dev --target tidy` | Run `clang-tidy` |
| `cmake --preset asan` · `cmake --build --preset asan` · `ctest --preset asan` | Sanitizer build and tests |
| `cmake --preset web` · `cmake --build --preset web` | WebAssembly build in `build/web` |
| `emrun build/web/agentscape.html` | Serve and open the web build locally |
| `cmake --preset release` · `cmake --build --preset release` · `cpack --preset release` | Release build and packages |

## Constraints

- The app must run with **no paid services, no server, and no network AI calls**. The only network call is the optional GitHub login on desktop.
- Budgets:
  - The web download (`.wasm` + `.data` + `.js`, compressed) stays under **5 MB**.
  - The desktop app reaches the login screen in under **1 s** on a mid-range laptop.
  - Frames render in under **16 ms** at 2× scale.
- Distribution:
  - The web build is hosted on **GitHub Pages**. `.github/workflows/pages.yml` deploys it after CI passes on a push to `main`, building the commit CI tested. The repository's Pages source must be **GitHub Actions**; on "Deploy from a branch" the site shows the README instead.
  - Desktop builds (Windows x64, macOS universal, Linux x64 AppImage) are attached to **GitHub Releases** by CI.
