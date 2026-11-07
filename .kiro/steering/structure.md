---
inclusion: always
---

# Project Structure

```
/
├─ .kiro/                     # Steering, specs, hooks. Never gitignore.
├─ .github/workflows/         # CI (desktop matrix + web), Pages deploy, release packaging
├─ docs/                      # DESIGN.md, SUBMISSION.md, ASSETS.md
├─ CMakeLists.txt
├─ CMakePresets.json          # dev, release, asan, web
├─ .clang-format / .clang-tidy
├─ cmake/
│  ├─ Dependencies.cmake      # FetchContent pins: SDL3, SDL3_image, imgui, sqlite, json, expected, curl, Catch2, rapidcheck
│  ├─ Warnings.cmake
│  └─ EmbedFiles.cmake        # Embeds data/migrations and data/seed into the binary
├─ assets/                    # Packaged next to the binary (desktop) or into the .data file (web)
│  ├─ sprites/                # Original indexed-PNG pixel art + atlas JSON: avatar layers, items, map, effects
│  ├─ ui/                     # 9-slice stone panels, buttons, icons
│  ├─ fonts/                  # OFL pixel fonts (TTF)
│  └─ audio/                  # Original or CC0 sound effects (WAV)
├─ data/
│  ├─ migrations/             # 0001_init.sql, 0002_….sql (append-only)
│  └─ seed/                   # agents.json, npcs.json, quest_logs.json, notes.json
├─ web/
│  └─ shell.html              # Emscripten HTML shell (canvas, loading bar, WebGL error message)
├─ src/
│  ├─ main.cpp                # SDL_App* callbacks only. Forwards to app::App.
│  ├─ app/                    # App, Config (CLI/env/URL), ScreenStack, EventBus, FrameCtx; smoke_runs.cpp (scripted --smoke-* runs)
│  ├─ screens/                # One folder per screen; thin. Reads services, draws widgets.
│  │  ├─ login/
│  │  ├─ world/               # Promptmoor hub
│  │  ├─ costumer/            # Character creator and wardrobe
│  │  ├─ codex/               # List, examine card, Proving Grounds, propose
│  │  ├─ tavern/              # Quest board, quest scroll, quest detail
│  │  ├─ hiscores/
│  │  ├─ graveyard/           # Kiroween Hall of Hallucinations
│  │  ├─ wilds/               # Coding sessions: start/stop, the fight or skilling scene
│  │  ├─ keeper/              # Codex Keeper queue (local moderation)
│  │  └─ dev/                 # UI and avatar galleries (AS_ENABLE_DEV_SCREENS only)
│  ├─ ui/                     # ImGui theme and retro widgets: StonePanel, PixelButton, ContextMenuHost,
│  │                          # Chatbox, HoverText, TabPanel, GemRating, markup parser
│  ├─ game/                   # WorldMap, AvatarRenderer, Fireworks, Fog, Bats, hotspot data
│  ├─ render/                 # GameFrame (render target + integer scaling), TextureCache, Atlas, NineSlice, palette surfaces
│  ├─ services/               # Orchestrate domain + db inside transactions. Publish EventBus events after commit.
│  ├─ db/                     # SQLite RAII (Database, Statement, Transaction), migration runner, repositories, seed loader
│  ├─ platform/               # Paths, IDBFS persistence, HTTP (curl), GitHub device flow, open URL, file export
│  ├─ domain/                 # PURE C++20: no SDL, ImGui, SQLite, curl, or I/O
│  │  ├─ skills.*             # Skill list, level formula, aggregates
│  │  ├─ ratings.*            # Rating deltas
│  │  ├─ loadout.*            # Equip rules, hidden layers, draw order
│  │  ├─ appearance.*         # Appearance options and validation
│  │  ├─ palette.*            # Indexed placeholder colours → appearance palettes
│  │  ├─ items.*              # Item catalog (code constants)
│  │  ├─ xp.*                 # Questing XP curve and unlocks
│  │  ├─ voting.*             # Wilson score, vote model
│  │  ├─ ranking.*            # Competition ranking
│  │  ├─ rate_limit.*         # Sliding-window limiter core
│  │  ├─ frame.*, kiroween.*, context_menu.*, chat_log.*, slug.*, adventurer_name.*
│  │  └─ *_json.*             # to_json / from_json for domain value types
│  └─ core/                   # Result<T,E>, GameError, Clock and Rng interfaces, ULID ids, string utils
└─ tests/
   ├─ generators/             # Shared RapidCheck generators and rc::Arbitrary<> specialisations
   ├─ property/               # <spec>_props.cpp
   ├─ unit/                   # <module>_test.cpp, regressions_<spec>.cpp
   └─ integration/            # Services and repositories against :memory: SQLite
                              # (the headless smoke runs live in src/app/smoke_runs.cpp; cmake/RunSmoke*.cmake drive them)
```

## Layering rules

1. `main.cpp` → `src/app` → `src/screens` → (`src/ui`, `src/game`, `src/render`) → `src/services` → (`src/domain`, `src/db`).
2. **`src/domain` is pure.** It depends only on the C++ standard library, `src/core`, and nlohmann/json (for value-type serialisation). Its functions are deterministic and side-effect free, so they can be property-tested. Pass time and randomness in as arguments (`std::chrono::sys_seconds now`, `core::Rng& rng`).
3. Screens and widgets never include SQLite or `src/db` headers. They call services and read from view models.
4. SDL, ImGui, and Emscripten headers are allowed only in `app`, `screens`, `ui`, `game`, `render`, and `platform`. Wrap every `#ifdef __EMSCRIPTEN__` in `src/platform`, not in feature code.
5. Services own transactions. A service call either fully commits or leaves the database unchanged. Services publish `EventBus` events only **after** commit.

## Naming conventions

- Files use `snake_case.hpp` / `snake_case.cpp`. One primary type or module per file pair.
- Namespaces: `as::domain`, `as::db`, `as::services`, `as::ui`, `as::game`, `as::render`, `as::platform`, `as::app`, `as::core`.
- Types use `PascalCase`. Functions and variables use `camelCase`. Domain constants use `UPPER_SNAKE_CASE` (`XP_TABLE`, `SKILLS`, `PRIOR_MEAN`). Enumerators are `PascalCase` inside `enum class`.
- SQL tables and columns use `snake_case` (`quest_log`, `agent_skill_stat`).
- Unit tests are `tests/unit/<module>_test.cpp`. Property tests are `tests/property/<spec>_props.cpp`.
- Domain terms come from the glossary in `product.md`. Use `QuestLog`, not `Review`, in code; for example, the table is `quest_log`.
- Service commands use verb-first names: `createQuestLog`, `equipItem`, `voteOnNote`.
