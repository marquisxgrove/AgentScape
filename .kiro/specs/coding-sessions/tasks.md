# Implementation Plan: Coding Sessions

> Depends on: `game-shell`, `character-creator`, `agent-codex`, `quest-logs`.

- [x] 1. Implement the session rules
  - [x] 1.1 Implement `domain::coding_session`: the limits, `codingCountedUntil`, `creditCodingInterval`, `codingXpFor`, and the day/duration formatting.
    - _Requirements: 1.5, 2.1, 2.2, 2.4_
  - [x]* 1.2 Write property tests (Properties 1–4) and the midnight example.
    - **Property 1: Settling is path-independent** / **Property 2: No day counts more than the daily limit** / **Property 3: Counted time never exceeds real time** / **Property 4: A session counts at most four hours**
    - **Validates: Requirements 1.5, 2.1, 2.2, 2.4**

- [x] 2. Add persistence and services
  - [x] 2.1 Add migration `0007_coding_sessions.sql` (`coding_session`, `coding_session_day`, `CODING` in `xp_entry`).
    - _Requirements: 1.2, 1.4, 2.2, 2.5_
  - [x] 2.2 Add `XpService::raiseTo` (shared `applyGain` with `award`).
    - _Requirements: 2.5_
  - [x] 2.3 Implement `CodingService` start / settle / stop / running / sessionXp / todayXp.
    - _Requirements: 1.1–1.5, 2.1–2.5_
  - [x]* 2.4 Write integration tests.
    - _Requirements: 1.2, 1.4, 1.5, 2.1–2.5_
  - _Note: QuestService, NoteService and KeeperService had the same sibling-reference pattern as CodingService (an XpService holding a reference to the service's own ActivityService) with a defaulted move, so moving the App's service bundle left it dangling. All four now have rebuilding move constructors._

- [x] 3. Build The Wilds
  - [x] 3.1 Implement `game::wilds_sim` (bestiary, fights, skilling, tallies) with unit tests.
    - _Requirements: 3.1, 3.2_
  - [x] 3.2 Draw the art (`tools/gen_wilds_art.py`, the map signpost) and log it in `docs/ASSETS.md`.
    - _Requirements: 3.1, 3.6_
  - [x] 3.3 Implement `WildsScreen` (picker, Start/Stop, scene, summary with "Write a Quest Log") and register it, with dev views `--screen=wilds` and `--screen=wilds-gryphon`.
    - _Requirements: 1.1, 1.3, 1.6, 3.1–3.4, 3.6_
  - [x] 3.4 Add The Wilds hotspot and Places entry, and the town status line.
    - _Requirements: 3.5, 3.6_
  - [x] 3.5 Settle from the App every minute and on sign-in, with the "While you were away…" and 4-hour messages.
    - _Requirements: 1.5, 2.3_
  - _Note: checked in the real desktop app: the familiar picker, a Gryphon session (bug, health bar, wisp, hit numbers, "Squashed!"), 30 minutes away credited on relaunch with the away message and the town status, Stop with the summary, and a solo session woodcutting. No headless smoke test covers The Wilds yet._
