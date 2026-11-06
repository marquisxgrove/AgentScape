# Kiroween 2026 Submission Kit: AgentScape

This file has the pre-submission checklist, a draft Devpost description, the demo video storyboard, and the "How we used Kiro" write-up. Lines marked **✏️ TODO** must be filled in with real experience before submitting. Judges value specifics.

---

## 1. Pre-submission checklist

### Repository

- [ ] The repo is **public on GitHub** (not GitLab or Bitbucket).
- [ ] An **OSI open-source `LICENSE`** is at the root and detected by GitHub (it shows in the *About* sidebar). ✏️ TODO: choose a license (MIT recommended) and add it.
- [ ] **`/.kiro` is at the repo root** with `steering/`, `specs/`, and `hooks/` committed.
- [ ] `.kiro` is **NOT** in `.gitignore` (enforced by `.kiro/hooks/submission-guard.json`).
- [ ] The README has setup instructions that work from a clean clone on Windows, macOS, and Linux (`cmake --preset dev`, build, run), plus the web build steps.
- [ ] All assets are logged in [`docs/ASSETS.md`](./ASSETS.md). There is no third-party copyrighted or trademarked material.

### Working app

- [ ] Web build URL (GitHub Pages): ✏️ TODO `https://…`
- [ ] Release binaries for Windows, macOS, and Linux are attached to a GitHub Release: ✏️ TODO link.
- [ ] Judging builds (web and release) are built with `AS_KIROWEEN_DEFAULT=on`, so the spooky layer is visible for the whole judging period.
- [ ] "Play as Guest" works in the web build in a private window (in-memory fallback warning is acceptable there) and in a normal window, and data survives a reload.
- [ ] On first launch, the seed data loads, so most Agents show ranked Skills and the Hiscores and Tavern are populated.
- [ ] The headless smoke test passes in CI on all three desktop platforms, and the demo path was checked manually in the web build in Chrome, Firefox, and Safari.

### Devpost form

- [ ] Category: **Resurrection**.
- [ ] Bonus prize: **Best Demo** (no extra steps needed).
- [ ] GitHub URL, app URL, and text description (section 3).
- [ ] Judge instructions in the **private** form field: "Open the web build link and click *Play as Guest*. No install or credentials needed. Desktop builds are on the Releases page." To try moderation, mention Settings → *Codex Keeper tools*.
- [ ] Demo video on YouTube, **public**, **under 3:00**, with no copyrighted music.
- [ ] "How we used Kiro" write-up (section 5).

---

## 2. Category justification: Resurrection

AgentScape brings back a dead era of the web: the fixed-size, plugin-based fantasy browser MMORPG client and its fansite item databases. Stone UI panels, a yellow-text chatbox, right-click "Examine", the paper-doll character creator, and Hiscores all return.

We rebuilt them with today's tech: a native C++20 client on SDL3 and Dear ImGui, with true 8-bit palette swaps, keyboard and gamepad navigation, and statistically robust scoring. Then we compiled it to WebAssembly, so the browser MMO runs in the browser again. We pointed all of it at a problem from tomorrow: deciding which AI model or tool to trust for which task.

---

## 3. Text description (Devpost draft)

> **AgentScape** is an AI tool review site disguised as a lost early-2000s browser MMO.
>
> Create a pixel-art Adventurer, dress them in cosmetics (including Kiroween gear like a Witch's Hat, Jack-o'-Lantern Mask, and Reaper's Scythe), and walk into the town of Promptmoor:
>
> - **The Grand Codex**: every AI model and tool has an *examine card* with an Overall level and ten Skill levels (Coding, Reasoning, Writing, Research, Creativity, Tool Use, Context, Reliability, Speed, Value) from 1 to 99. Levels come from community ratings with Bayesian smoothing, and you can filter them by task type.
> - **The Tavern**: write a *Quest Log* about a real task you did with an AI. Rate it, tag pros and cons, and earn Questing XP. Level up (with fireworks) to unlock new cosmetics.
> - **Boons and Banes**: short, community-voted strengths and weaknesses, ranked by Wilson score.
> - **The Proving Grounds**: compare up to three Agents side by side for a given task type.
> - **Hiscores**: leaderboards for Agents by Skill and for the most helpful adventurers.
> - **The Hall of Hallucinations** (Kiroween only): a graveyard where the community's most-agreed AI failures rest under tombstones.
>
> Under the nostalgic skin, it's a native C++20 app (SDL3 + Dear ImGui) that also runs in the browser through WebAssembly. It works offline, supports full keyboard and gamepad navigation, and is backed by property-based tests of every scoring and equipment rule.

✏️ TODO: add the tech stack line (C++20 · SDL3 · Dear ImGui · SQLite · Emscripten · RapidCheck), a link to the video, and 3–5 screenshots/GIFs.

---

## 4. Demo video storyboard (target 2:45, hard cap 3:00)

| Time | Shot | Voice-over / on-screen text |
|---|---|---|
| 0:00–0:12 | Foggy Kiroween login screen, bats, "Welcome to AgentScape" | "Remember logging into a fantasy MMO in your browser in 2004? We brought it back, natively and in the browser, to help you choose AI tools." |
| 0:12–0:40 | Costumer: arrow through hair and colours, rotate, then equip Jack-o'-Lantern Mask, Vampire Cape, Reaper's Scythe | "Create an Adventurer and dress up for Kiroween. The paper doll follows classic rules: two-handers drop your shield, and masks hide your hair." |
| 0:40–0:55 | World map: hover text, right-click → *Examine* on the library → chatbox message → Enter | "Promptmoor is the hub. Right-click anything to examine it." |
| 0:55–1:25 | Grand Codex → an Agent's examine card → switch Quest Type from All to Debugging → levels change → Boons and Banes | "Every AI model and tool has an Overall level and ten Skill levels, built from real experiences and scoped to the task." |
| 1:25–1:40 | Proving Grounds compare of 3 Agents, with green highlights | "Compare up to three side by side." |
| 1:40–2:10 | "Write a Quest Log" → 3-step scroll → submit → **+250 XP** drop → level-up message → fireworks → "You have unlocked: …" | "Log a quest, earn XP, and level up. The Codex updates instantly." |
| 2:10–2:25 | Hiscores: Agents by Reliability, then Adventurers | "Hiscores for Agents and adventurers." |
| 2:25–2:40 | Hall of Hallucinations tombstones | "And during Kiroween, the community's most-agreed AI failures rest in the Hall of Hallucinations." |
| 2:40–2:55 | Quick montage: the same build running as a desktop window and in a browser tab, Kiro specs, hooks firing, property tests passing | "One C++ codebase, desktop and web. Built spec-first with Kiro: steering, specs, hooks, and property-based tests." |

Recording notes: record the desktop build at 1080p, with the game frame at 2× scale (1530×1006 inside the window). Use no music, or original/CC0 chiptune only. Avoid showing any third-party logos.

---

## 5. How we used Kiro (write-up draft)

> Structure mirrors the judging prompts. Keep the planned content that turns out to be true, replace ✏️ TODOs with concrete anecdotes, and add metrics (task counts, bugs caught, time saved) wherever possible.

### Kiro surfaces used (IDE / CLI / Web / Crew)

- Planned: the **Kiro IDE** for spec authoring and task execution, and the **Kiro CLI** for running CMake builds, CTest, and hooks in the terminal.
- ✏️ TODO: describe how the surfaces differed in practice and which tasks you preferred on each.

### Spec-driven development

- We wrote four feature specs, each with `requirements.md` (EARS-style acceptance criteria), `design.md`, and `tasks.md`:
  - [`game-shell`](../.kiro/specs/game-shell/)
  - [`character-creator`](../.kiro/specs/character-creator/)
  - [`agent-codex`](../.kiro/specs/agent-codex/)
  - [`quest-logs`](../.kiro/specs/quest-logs/)
- Specs are layered by dependency (shell → creator → codex → quests). They share one domain glossary, and cross-spec contracts are explicit (for example, `applyRatingDelta` and `xpService.award`).
- Each `design.md` lists numbered **Correctness Properties** that map to requirement IDs. Kiro turned these into property-test tasks.
- ✏️ TODO: compare this to vibe coding. Where did the spec prevent rework? Give one example where a design review caught a bug before code existed (for example, a shared guest account would have broken the level-up demo, so we switched to per-session guest accounts).

### Steering docs

- `product.md`, `tech.md`, and `structure.md` are always included. `art-direction.md` is included for UI and asset files, and `testing.md` for tests and domain code (fileMatch).
- Strategy: put the **domain glossary** and **layering rules** in always-on steering. That way Kiro consistently names things `QuestLog` and `Adventurer` and keeps `src/domain` pure C++ (no SDL, ImGui, or SQLite) and testable. Put the heavy visual rules in a fileMatch doc, so they only cost context when needed.
- ✏️ TODO: name the steering change that made the biggest difference, with a before and after.

### Agent hooks

| Hook | Trigger | What it automates |
|---|---|---|
| Format C++ on save | `PostFileSave` `*.cpp`, `*.hpp` | `clang-format` via the `format` target |
| Build and test on stop | `Stop` | Debug build (warnings as errors) + full CTest suite before control returns |
| Property tests on domain change | `PostFileSave` `src/domain/**` | Rebuilds and re-runs the RapidCheck suite |
| Spec task test-first | `PreTaskExec` | Re-read requirements, write property tests first, keep the domain pure |
| Migration ↔ spec sync | `PostFileSave` `data/migrations/*.sql` | Applies all migrations to a fresh database, enforces append-only migrations, and updates the data-model sections in the specs |
| Art-direction review | `PostFileSave` UI, game, render, and screen files | Checks palette constants, nearest scaling, nav focus, plain-text rendering, and reduced motion |
| Asset provenance | `PostFileSave` `assets/**` | Requires an `ASSETS.md` entry and blocks unlicensed assets |
| `.kiro` gitignore guard | `PostFileSave` `.gitignore` | Fails if `git check-ignore` matches `.kiro` (disqualification guard) |

✏️ TODO: describe which hook saved the most time, with a concrete incident.

### Property-based testing

- 29 properties across the four specs (4 + 9 + 8 + 8). Examples:
  - The two-handed and shield exclusivity rule.
  - Incremental rating aggregates must equal a full recompute after any create, edit, or delete sequence.
  - Skill-level bounds and monotonicity.
  - The helpful-XP daily cap.
  - Competition ranking.
- ✏️ TODO: describe at least one real bug a property caught that unit tests missed, and include the shrunk counterexample.

### Custom agents

- ✏️ TODO: for example, a "Pixel Smith" agent with only the art-direction and assets steering, used for sprite and atlas work, or a "Quest Master" agent for spec review. If you create one, note that steering must be added to its `resources`.

### MCP

- ✏️ TODO: list the MCP servers you used and what they enabled. Candidates:
  - A browser MCP, for visual checks of the web build.
  - A docs MCP, for current SDL3, Dear ImGui, SQLite, and Emscripten APIs.
  - A GitHub MCP, for issues and PRs.
  - A SQLite MCP, to inspect aggregates while debugging.

### Kiro powers

- ✏️ TODO: list any powers used (for example, framework or testing best-practice bundles) and what they changed.
