# Kiroween 2026 Submission Kit: AgentScape

This file has the pre-submission checklist, the Devpost description, the demo video storyboard, and the "How we used Kiro" write-up. Lines marked **✏ YOU** can only be filled in by the team (links that don't exist yet, or experience only you had). Everything else is filled in from the project's history.

---

## 1. Pre-submission checklist

### Repository

- [x] The repo is **public on GitHub**: <https://github.com/marquisxgrove/AgentScape>.
- [x] An **OSI open-source `LICENSE`** (MIT) is at the root. After it reaches `main`, check that GitHub shows "MIT license" in the *About* sidebar.
- [x] **`/.kiro` is at the repo root** with `steering/`, `specs/`, and `hooks/` committed.
- [x] `.kiro` is **NOT** in `.gitignore` (checked with `git check-ignore`; also guarded by `.kiro/hooks/submission-guard.json`).
- [x] The README has setup instructions for Windows, macOS and Linux (`cmake --preset dev`, build, run), plus the web build steps. CI builds exactly those presets from a clean checkout on all three platforms.
- [x] Every file in `assets/` is logged in [`docs/ASSETS.md`](./ASSETS.md); libraries and fonts are in [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md). There is no third-party copyrighted or trademarked material, and the seed data is fictional.

### Working app

- [ ] **Web build URL (GitHub Pages):** `https://marquisxgrove.github.io/AgentScape/`. ✏ YOU: enable *Settings → Pages → Source: GitHub Actions* once; `.github/workflows/pages.yml` deploys on every push to `main`. Then open the link and tick this.
- [ ] **Release binaries** for Windows, macOS and Linux on a GitHub Release. ✏ YOU: `git tag v1.0.0 && git push origin v1.0.0`; `.github/workflows/release.yml` builds and attaches them. Then paste the link: `https://github.com/marquisxgrove/AgentScape/releases/latest`.
- [x] Judging builds (Pages and release) are built with `AS_KIROWEEN_DEFAULT=on`, so the spooky layer is visible for the whole judging period.
- [ ] ✏ YOU: "Play as Guest" works in the web build in a private window (an in-memory warning is acceptable there) and in a normal window, and data survives a reload. *Not checked by automation; nobody has run the web build in a browser yet.*
- [x] On first launch, the seed data loads: every familiar is ranked, and the Hiscores, the Tavern (12 sample Quest Logs) and the Hall of Hallucinations (13 graves) are populated.
- [x] The headless smoke tests pass in CI on Windows, macOS and Linux (and under AddressSanitizer + UBSan on Linux).
- [ ] ✏ YOU: the demo path checked by hand in the web build in Chrome, Firefox and Safari.

### Devpost form

- [ ] Category: **Resurrection**.
- [ ] Bonus prize: **Best Demo** (no extra steps needed).
- [ ] GitHub URL, app URL, and text description (section 3).
- [ ] Judge instructions in the **private** form field (section 6).
- [ ] Demo video on YouTube, **public**, **under 3:00**, with no copyrighted music. ✏ YOU: record it (section 4) and paste the link.
- [ ] "How we used Kiro" write-up (section 5).

---

## 2. Category justification: Resurrection

AgentScape brings back a dead era of the web: the fixed-size, plugin-based fantasy browser MMORPG client and its fansite item databases. Stone UI panels, a yellow-text chatbox, right-click "Examine", the paper-doll character creator, and Hiscores all return.

We rebuilt them with today's tech: a native C++20 client on SDL3 and Dear ImGui, with true 8-bit palette swaps, keyboard and gamepad navigation, and statistically robust scoring. Then we compiled it to WebAssembly, so the browser MMO runs in the browser again. We pointed all of it at a problem from tomorrow: deciding which AI model or tool to trust for which task.

---

## 3. Text description (Devpost)

> **AgentScape** is an AI tool review site disguised as a lost early-2000s browser MMO.
>
> Create a pixel-art Adventurer, dress them in cosmetics (including Kiroween gear like a Witch's Hat, Jack-o'-Lantern Mask, and Reaper's Scythe), and walk into the town of Promptmoor:
>
> - **The Grand Codex**: every AI model and tool has an *examine card* with an Overall level and ten Skill levels (Coding, Reasoning, Writing, Research, Creativity, Tool Use, Context, Reliability, Speed, Value) from 1 to 99. Levels come from community ratings with Bayesian smoothing, and you can scope them to a task type.
> - **The Tavern**: write a *Quest Log* about a real task you did with an AI. Rate it with gems, tag pros and cons, and earn Questing XP. Level up (with fireworks) to unlock new cosmetics.
> - **Boons and Banes**: short, community-voted strengths and weaknesses, ranked by Wilson score.
> - **The Proving Grounds**: compare up to three Agents side by side for a given task type.
> - **Hiscores**: leaderboards for Agents by any Skill and for the most helpful adventurers.
> - **The Hall of Hallucinations** (Kiroween only): a graveyard where the community's most-agreed AI failures rest under tombstones.
>
> It ships with a fictional sample community (14 made-up familiars, 8 townsfolk, their notes and quest logs), so every screen is alive on first run, and your own logs update it instantly.
>
> Under the nostalgic skin, it's a native C++20 app (SDL3 + Dear ImGui) that also runs in the browser through WebAssembly. It works offline, supports full keyboard and gamepad navigation, and is backed by 29 property-based correctness properties and 393 automated tests, including a headless run of the whole demo path.
>
> **Built with:** C++20 · SDL3 · Dear ImGui · SQLite · Emscripten (WebAssembly) · Catch2 · RapidCheck · CMake · GitHub Actions.

Screenshots for the gallery: `docs/screenshots/readme/` (world map, Costumer, Codex, examine card, quest scroll, level-up, Tavern, Hiscores, Hall of Hallucinations). ✏ YOU: add the video link.

---

## 4. Demo video storyboard (target 2:45, hard cap 3:00)

Every shot below exists in the app as built. The smoke test `smoke_demo_path` plays the same route headlessly, so it is a good rehearsal: its snapshots are in `build/dev/smoke-path/` after `ctest --preset dev -R smoke_demo_path`.

| Time | Shot | Voice-over / on-screen text |
|---|---|---|
| 0:00–0:12 | Foggy Kiroween login screen, bats, "Welcome to AgentScape" → Play as Guest | "Remember logging into a fantasy MMO in your browser in 2004? We brought it back, natively and in the browser, to help you choose AI tools." |
| 0:12–0:40 | Costumer: arrow through hair and colours, rotate, then Gear: claim and equip Jack-o'-Lantern Mask, Vampire Cape, Reaper's Scythe | "Create an Adventurer and dress up for Kiroween. The paper doll follows classic rules: two-handers drop your shield, and masks hide your hair." |
| 0:40–0:55 | World map: hover text, right-click → *Examine* on the Grand Codex → chatbox message → Enter | "Promptmoor is the hub. Right-click anything to examine it." |
| 0:55–1:25 | Grand Codex bookshelf → Gryphon's examine card → switch Quest Type from All to Debugging → levels change → Boons and Banes, top Quest Logs | "Every AI model and tool has an Overall level and ten Skill levels, built from real experiences and scoped to the task." |
| 1:25–1:40 | Proving Grounds with 3 familiars, green highlights and `*` marks | "Compare up to three side by side." |
| 1:40–2:10 | "Write a Quest Log" → 3-step scroll → submit → **+250 XP** drop → level-up message → fireworks → "You have unlocked: …" | "Log a quest, earn XP, and level up. The Codex updates instantly." |
| 2:10–2:25 | Tavern board with the townsfolk's notes, then Hiscores: familiars by Reliability, then Adventurers with the search jump | "Read other adventurers' quests, and climb the Hiscores." |
| 2:25–2:40 | Hall of Hallucinations tombstones | "And during Kiroween, the community's most-agreed AI failures rest in the Hall of Hallucinations." |
| 2:40–2:55 | Montage: desktop window and browser tab side by side, a spec's `tasks.md`, the CI matrix all green, property tests passing | "One C++ codebase, desktop and web. Built spec-first with Kiro: steering, specs, hooks, and property-based tests." |

Recording notes: record the desktop build at 1080p, with the game frame at 2× scale (1530×1006 inside the window). Use no music, or original/CC0 chiptune only. Turn on *Codex Keeper tools* beforehand only if you want to show moderation. The sample familiars are fictional, so there are no third-party logos or names on screen.

---

## 5. How we used Kiro

> Facts below are from the project's own history (35 pull requests, the specs and their task notes). Parts marked ✏ YOU are the team's own experience.

### Kiro surfaces used

- **Kiro Web (autonomous agent in a cloud sandbox).** Most of the implementation (the large majority of commits across PRs #1–#35) was written by the Kiro agent working in the browser-based environment: reading the specs, implementing one task slice per pull request, running the build and the whole test suite in its sandbox, rendering the real app headlessly to check every screen, and opening each PR with what it had and had not verified. The team reviewed and merged every PR, and steered between them in plain language ("use a branch and PR approach", "switch the sample data to fictional agents", "seed sample logs, then the final checkpoint", "increase the font size").
- **Human-authored changes** sat between agent PRs where the team wanted control: for example CI hardening and test fixes (#31) and Guest remember-me with Back navigation (#34).
- ✏ YOU: if you also used the Kiro IDE or CLI (for the specs, steering or hooks), say which tasks you preferred on each surface.

### Spec-driven development

- Four feature specs, each with `requirements.md` (EARS-style acceptance criteria), `design.md`, and `tasks.md`: [`game-shell`](../.kiro/specs/game-shell/), [`character-creator`](../.kiro/specs/character-creator/), [`agent-codex`](../.kiro/specs/agent-codex/), [`quest-logs`](../.kiro/specs/quest-logs/). They are layered by dependency (shell → creator → codex → quests), share one domain glossary, and spell out cross-spec contracts such as `applyRatingDelta` and the XP ledger.
- Each `design.md` lists numbered **Correctness Properties** mapped to requirement IDs (29 in total); every one became a RapidCheck test named `Property <n>: <title>`.
- **The spec shaped the order of work.** Every slice started with the pure domain and its properties, then persistence and services, then screens, then a smoke test. Each PR said which tasks it closed and what was still unverified, and `tasks.md` carries the notes.
- **Design reviews that changed the design, recorded back into the specs:**
  - `applyRatingDelta` had to be UPDATE-then-INSERT: SQLite checks a table's CHECK constraint on the proposed row before it notices the key conflict, so a single upsert rejected negative deltas (agent-codex design notes).
  - The Proving Grounds selection got its own table instead of living in the settings JSON: the Settings tab rewrites the whole JSON on every toggle and would have silently overwritten it.
  - Sample Quest Logs are stored `seeded` and `excluded`, because their ratings are already in the sample baseline and would otherwise be counted twice.
  - Migration numbers moved (0004 became the Proving Grounds), and the change was written into quest-logs' design so the spec stays true.

### Steering docs

- `product.md`, `tech.md`, and `structure.md` are always included. `art-direction.md` is included for UI and asset files, and `testing.md` for tests and domain code (fileMatch).
- The always-on glossary kept names consistent across ~400 tests and every screen (`Agent`, `QuestLog`, `Adventurer`, `Note` for Boons and Banes), and the layering rule kept `src/domain` pure C++ (no SDL, ImGui or SQLite), which is what made the property tests possible.
- The steering was updated when reality disagreed: the art direction now says the pixel font is drawn at 12 px and that layouts must measure text rather than assume a width.
- ✏ YOU: the steering change that made the biggest difference for you, with a before and after.

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

In the cloud sandbox, the agent ran the same gates the hooks encode by hand on every slice: a full build with warnings as errors, the whole CTest suite, migrations applied to a fresh database, and every asset checked against the ledger. ✏ YOU: which hook saved you the most time in the IDE, with an incident.

### Property-based testing, and testing the tests

- 29 properties across the four specs (4 + 9 + 8 + 8), for example: two-handed and shield exclusivity; incremental rating aggregates equal a full recompute after any create, edit, delete or exclude sequence (model-based); Skill-level bounds; the helpful-XP daily cap; competition ranking; and the round-trip of every validated input.
- **A real bug a property caught:** the chat-log ordering property found a tie-break bug in how the chatbox orders messages, in the second slice, before any screen depended on it.
- **Mutation checks.** After each slice the agent deliberately broke each rule (removed a guard, flipped a comparison, skipped a write) and confirmed a test failed, then restored the code. Over the project, well over a hundred such breaks. Several survived at first and exposed weak tests, which were then strengthened:
  - The duplicate-slug test still passed with the check removed, because the database's UNIQUE constraint refused the insert instead. The test now requires the friendly error.
  - A graveyard ranking test passed with a raw agree ratio instead of the Wilson score, because the test data ranked the same either way. A 3-of-3 vs 90-of-100 case now tells them apart.
  - The Hiscores showed retired (inactive) Adventurers, and ignored the Quest Type scope, without any test noticing. Both now have tests.
  - The "nobody edits a sample Quest Log" test only tried a non-author; the NPC-author case was missing.
- **The demo path is a test.** `smoke_demo_path` plays Guest → Costumer → Codex card → Quest Log → +250 XP and level-up → Hiscores in the real app, then restarts it on the same database and checks everything survived.

### Bugs found along the way (the honest list)

- The avatar renderer destroyed its whole texture cache when full, including textures already queued for the current frame. It became reachable when the Tavern drew twelve thumbnails at once; textures are now retired and freed after the frame.
- Deleting a user cascaded their Note votes away but left the agree/disagree counts inflated. Idle Guests are now purged through the normal delete paths.
- Reserving the townsfolk's names broke an older smoke test that created an Adventurer called "Mossbeard". It was caught before the PR was opened (after one red push), fixed, and pinned with a test.
- A stacked PR (#33) was merged into a branch that had already merged, so the final checkpoint missed `main` until #35. Lesson recorded: don't stack PRs on branches that may merge first.

### Custom agents

- ✏ YOU: for example, a "Pixel Smith" agent with only the art-direction and assets steering for sprite work. If you add one, its steering must be listed in its `resources`.

### MCP and powers

- The cloud agent worked with GitHub through the `gh` CLI (branches, pull requests and CI status).
- ✏ YOU: any MCP servers you used in the IDE (for example a browser MCP for checking the web build).

---

## 6. Judge instructions (private Devpost field)

> Open **https://marquisxgrove.github.io/AgentScape/** and click **Play as Guest**. No install or account needed; everything is saved in your browser. A Guest starts at Questing level 13, so writing your first Quest Log (Grand Codex → any familiar → *Write a Quest Log*) shows a level-up with fireworks.
>
> Desktop builds (Windows, macOS, Linux) are on the Releases page; they are unsigned (right-click → Open on macOS, "Run anyway" on Windows).
>
> To try moderation, turn on *Settings → Codex Keeper tools*. Kiroween is on in these builds. All familiars and townsfolk are fictional sample data.
