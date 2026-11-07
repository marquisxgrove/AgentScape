# Implementation Plan: Grand Codex

> Depends on: `game-shell` (tasks 1–8). Coordinates with `quest-logs` through `applyRatingDelta`.

- [x] 1. Implement scoring domain
  - [x] 1.1 Implement `skills`: Skill and Quest_Type constants, `aggOf`, `mergeAgg`, `skillLevel`, `totalLevel`.
    - _Requirements: 3.1–3.5_
  - [x]* 1.2 Write property tests for level bounds, order independence, monotonicity, and scope additivity (Properties 1–4).
    - **Property 1: Level bounds and ranking threshold** / **Property 2: Order independence** / **Property 3: Monotonicity** / **Property 4: Scope additivity**
    - **Validates: Requirements 3.1–3.5**
  - [x] 1.3 Implement `voting` (`wilsonLowerBound`, `applyVote`, `tally`), `slug`, and `rate_limit`.
    - _Requirements: 4.2, 4.3, 5.2–5.5_
  - [x]* 1.4 Write property tests for Wilson, the vote model, slugs, and rate limits (Properties 5–8).
    - **Property 5: Wilson bounds and ordering** / **Property 6: Vote model** / **Property 7: Slug normalization** / **Property 8: Rate limit**
    - **Validates: Requirements 4.2, 4.3, 5.2–5.5**

- [~] 2. Add persistence (2.3: Agents, ratings and Notes done; Quest Logs and NPC Adventurers wait for their specs)
  - [x] 2.1 Add migration `0003_codex.sql` (`agent`, `agent_skill_stat`, `note`, `note_vote`, `report`) and the repositories.
    - _Requirements: 2.1, 3.5, 5.1, 8.2_
    - _Note: the stat table gained `seed_sum`/`seed_count` (so a reseed swaps only the seeded baseline and never erases player ratings) and CHECK constraints (every aggregate must be a possible set of 1-5 ratings). Repositories are plain functions in `services/codex_store` (read side: `findAgentBy*`, `listAgents`, `loadStats`) plus `domain::StatTable`/`levelsFor` for the pure level computation._
  - [x] 2.2 Implement `applyRatingDelta(tx, …)` with the after-commit `RatingsChanged` event and `CodexCache` invalidation.
    - _Requirements: 3.5_
    - _Note: UPDATE-then-INSERT rather than a single upsert, because SQLite checks CHECK constraints against the proposed row before it detects a key conflict (found by the randomised create/edit/delete test). Events: `RatingsChanged` (existing), plus new `AgentChanged` and `NotesChanged`; `CodexCache` bumps a generation counter on all three._
  - [~] 2.3 Write `data/seed/*.json` (≥ 12 Agents, NPC Adventurers, sample Quest Logs and Notes) and the idempotent seed loader. Add an integration test that seeding twice is a no-op.
    - _Requirements: 9.1–9.3_
    - _Done: `agents.json` (14 Agents), `ratings.json` (synthetic baseline for every Agent/Skill/Quest Type, all Skills ranked in the All scope), `notes.json` (49 Boons/Banes; 21 Banes are Graveyard-ready) via `tools/gen_seed_codex.py`; the loader validates through the same domain validators proposals use, rejects bad files whole, and is idempotent. **Deferred:** sample Quest Logs and NPC Adventurers/XP need the quest-logs and Hiscores specs (tables don't exist yet). When quest-logs seeds Quest Logs they must be display-only: their ratings are already inside the snapshot baseline._
    - _**The agents, ratings and notes are fictional sample data** (invented agents and vendors, ratings generated from a hash; flagged in the files). The Codex UI should label them "sample snapshot"._

- [x] 3. Build the Codex list
  - Implement `CodexService::list` with search, filters, sort (unranked last), pagination, and Quest_Type scope.
  - Build the bookshelf scene and the navigable table.
  - _Requirements: 1.1–1.5_

- [x] 4. Build the examine card
  - Add the parchment card, skills grid with original pixel skill icons, Quest_Type selector, top Boons/Banes, "Write a Quest Log" button, and the website link with confirmation (`platform::openUrl`).
  - _Requirements: 2.1–2.5_

- [x] 5. Add Boons, Banes, and votes
  - [x] 5.1 Implement the `createNote` and `voteOnNote` commands with rate limiting, the self-vote guard, and transactional count updates.
    - _Requirements: 5.1–5.6_
  - [x] 5.2 Build the Note list UI with agree/disagree buttons, "view all", and plain-text rendering (`TextUnformatted`).
    - _Requirements: 5.5, 5.6_

- [x] 6. Build proposals and Keeper tools
  - [x] 6.1 Build the Propose screen and `proposeAgent` command with the slug duplicate check, https-only website, and 3/day limit.
    - _Requirements: 4.1–4.3, 4.5_
  - [x] 6.2 Add the "Codex Keeper tools" setting (`setKeeper`) and the Keeper queue screen: approve/reject proposals, hide/unhide Notes, resolve Reports. Post an ActivityEvent on approval.
    - _Requirements: 4.4, 8.1–8.4_

- [x] 7. Build the Proving Grounds
  - Add the Proving Grounds screen with 2–3 Agent pickers, Quest_Type scope, per-row highlight (ties included), remembered selection, and "Copy" (`SDL_SetClipboardText`).
  - _Requirements: 6.1–6.4_

- [ ] 8. Build the Hall of Hallucinations
  - Add the Graveyard screen (Kiroween only) with up to 13 tombstones from top non-hidden Banes with ≥ 3 votes, epitaphs, open-card actions, and a navigable list.
  - _Requirements: 7.1–7.4_

- [ ] 9. Checkpoint
  - Extend the smoke test with the Codex flow. Make sure all tests pass, and ask the user if questions arise.
