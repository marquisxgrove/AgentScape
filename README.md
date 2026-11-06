# AgentScape

> *Welcome to AgentScape.* Rate the AI models and tools you use, and build a codex of their strengths and weaknesses, inside a lovingly resurrected early-2000s fantasy MMO client. It runs natively on your desktop and in your browser.

Built for **Kiroween 2026**, **Resurrection** category.

<!-- ✏️ TODO: hero GIF of the Kiroween login → Costumer → level-up -->

**▶ Play in your browser:** ✏️ TODO `https://<you>.github.io/agentscape/` · **Download:** ✏️ TODO [latest release](https://github.com/<you>/agentscape/releases/latest) (Windows, macOS, Linux)

## What you can do

- **Create and dress up Adventurers.** A classic arrow-button character designer, a 9-slot paper-doll, and unlockable cosmetics, including **Kiroween gear** like the Witch's Hat, Jack-o'-Lantern Mask, Vampire Cape, and Reaper's Scythe.
- **Explore Promptmoor.** A clickable pixel town with hover text, right-click menus, *Examine*, and a living chatbox.
- **Consult the Grand Codex.** Every AI model and tool has an *examine card* with an Overall level and **10 Skill levels (1–99)** built from ratings, filterable by task type. It also shows community-voted **Boons** (strengths) and **Banes** (weaknesses). The Codex ships with a seeded community snapshot, and your own ratings update it instantly.
- **Write Quest Logs at the Tavern.** Review a real task you did with an AI. Earn **Questing XP**, level up (fireworks included), and unlock cosmetics.
- **Compare Agents** side by side in the Proving Grounds, and climb the **Hiscores**.
- **Visit the Hall of Hallucinations** during Kiroween, a graveyard of the community's most-agreed AI failures.

Everything is stored locally in SQLite (in IndexedDB on the web). No server, no AI API calls, and it works offline.

## Tech stack

C++20 · SDL3 · SDL3_image · Dear ImGui · SQLite · nlohmann/json · libcurl (desktop GitHub sign-in) · CMake + Ninja · Emscripten (WebAssembly) · Catch2 · RapidCheck.

## Getting started

> Implementation is in progress. These steps describe the target setup.

### Prerequisites

- **CMake 3.25+** and **Ninja**
- A C++20 compiler: **Visual Studio 2022 17.8+** (Windows), **Clang 17+** (macOS/Linux), or **GCC 13+** (Linux)
- **Git** (dependencies are fetched at configure time)
- Linux only: the SDL3 build dependencies for your distro (X11/Wayland, ALSA/PulseAudio/PipeWire dev packages)
- For the web build: **Emscripten 4.x** via [emsdk](https://emscripten.org/docs/getting_started/downloads.html), with `EMSDK` set

### Desktop

```bash
git clone https://github.com/<you>/agentscape.git
cd agentscape
cmake --preset dev
cmake --build --preset dev
./build/dev/agentscape          # Windows: .\build\dev\agentscape.exe
```

Click **Play as Guest** to explore without an account. The database and seed data are created automatically on first launch. Pass `--kiroween=on` to force the spooky layer outside of October and November.

To enable **Sign in with GitHub**, create a GitHub OAuth App with **Enable Device Flow** turned on, then configure with `cmake --preset dev -DAS_GITHUB_CLIENT_ID=<client id>`. No client secret is needed.

### Web

```bash
cmake --preset web
cmake --build --preset web
emrun build/web/agentscape.html   # add ?kiroween=on to the URL to force Kiroween mode
```

### Commands

| Command | Description |
|---|---|
| `cmake --preset dev` · `cmake --build --preset dev` | Configure and build (Debug) |
| `ctest --preset dev` | All unit, integration, property, and smoke tests |
| `ctest --preset dev -L property` | Property-based tests only |
| `cmake --build --preset dev --target format` / `tidy` | `clang-format` / `clang-tidy` |
| `cmake --preset asan` · `cmake --build --preset asan` · `ctest --preset asan` | Sanitizer build and tests |
| `cmake --preset web` · `cmake --build --preset web` | WebAssembly build |
| `cmake --preset release` · `cmake --build --preset release` · `cpack --preset release` | Release build and packages |
| `agentscape --reseed` / `--reset-data` / `--data-dir <path>` | Re-run the seed / back up and reset local data / use another data folder |

### Unsigned builds

Release binaries aren't code-signed yet. On macOS, right-click the app and choose **Open** the first time. On Windows, choose **More info → Run anyway** in SmartScreen. Or just use the web build.

## Documentation

| Doc | Purpose |
|---|---|
| [docs/DESIGN.md](docs/DESIGN.md) | Vision, world, systems, architecture, milestones |
| [docs/SUBMISSION.md](docs/SUBMISSION.md) | Hackathon checklist, demo storyboard, Kiro write-up |
| [docs/ASSETS.md](docs/ASSETS.md) | Asset provenance and licenses |
| [.kiro/steering/](.kiro/steering/) | Product, tech, structure, art direction, testing guidance for Kiro |
| [.kiro/specs/](.kiro/specs/) | Feature specs: game-shell, character-creator, agent-codex, quest-logs |
| [.kiro/hooks/](.kiro/hooks/) | Automated format, build-and-test, property-test, migration-sync, art-review, and submission guards |

## How Kiro was used

AgentScape is built spec-first with Kiro: always-on steering for the domain glossary and architecture, four layered feature specs with property-based correctness properties, and agent hooks that automate quality gates. See the full write-up in [docs/SUBMISSION.md](docs/SUBMISSION.md#5-how-we-used-kiro-write-up-draft).

## Credits and disclaimer

All art, names, and UI assets are original or permissively licensed (see [docs/ASSETS.md](docs/ASSETS.md)). AgentScape is inspired by the general style of early-2000s browser MMORPGs and isn't affiliated with any game studio or AI vendor. AI product names are used only to identify the products being reviewed.

## License

✏️ TODO: add an OSI-approved `LICENSE` file (required by the hackathon rules).
