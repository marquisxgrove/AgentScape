# Implementation Plan: Quest Logs, Questing XP and Hiscores

> Depends on: `game-shell`, `character-creator` (items, unlocks, snapshot), and `agent-codex` (skills, `applyRatingDelta`).

- [ ] 1. Implement ratings domain
  - [ ] 1.1 Implement `ratings`: `RatingSet`, `ratingDeltas`, `sumDeltas`, `QuestLogInput`, `validateQuestLogInput`, its JSON codec, and the quick-tag lists.
    - _Requirements: 1.1, 2.1, 2.3_
  - [ ]* 1.2 Write property test for delta algebra (Property 2).
    - **Property 2: Delta algebra**
    - **Validates: Requirement 2.1**
  - [ ]* 1.3 Write model-based property test for incremental aggregates vs. recompute (Property 1).
    - **Property 1: Incremental aggregates equal recompute**
    - **Validates: Requirements 2.1–2.3**
  - [ ]* 1.4 Write property test for input validation (Property 8).
    - **Property 8: Input validation**
    - **Validates: Requirement 1.1**

- [ ] 2. Implement XP and ranking domain
  - [ ] 2.1 Implement `xp` (`XP_TABLE`, `levelForXp`, `XP_REWARDS`, `helpfulXpAllowed`, `newUnlocks`) and `ranking` (`competitionRank`).
    - _Requirements: 5.1, 5.4, 6.1–6.3, 7.3_
  - [ ]* 2.2 Write property tests for the XP table, level monotonicity, unlocks, helpful cap, and ranking (Properties 3–7).
    - **Property 3: XP table and level** / **Property 4: Level monotonicity** / **Property 5: Unlocks are monotone and idempotent** / **Property 6: Helpful XP daily cap** / **Property 7: Competition ranking**
    - **Validates: Requirements 5.1, 5.4, 6.1, 6.3, 7.3**

- [ ] 3. Add persistence and services
  - [ ] 3.1 Add migration `0004_quests.sql` (`quest_log`, `helpful_vote`, `xp_entry`, `user.total_xp`) and the repositories.
    - _Requirements: 1.3, 4.1, 5.2_
  - [ ] 3.2 Implement `XpService::award` / `revoke` with `ON CONFLICT DO NOTHING` idempotency, level recompute, unlock grants, and level-milestone ActivityEvents.
    - _Requirements: 5.1–5.4, 6.1–6.3, 8.2_
  - [ ] 3.3 Implement `QuestService` create, update, and delete (single transaction: ratings → `applyRatingDelta` → XP → unlocks → ActivityEvent; events published after commit), returning `QuestOutcome`.
    - _Requirements: 1.2–1.5, 2.1–2.3, 5.3, 8.1_
  - [ ] 3.4 Implement `toggleHelpful` with the self-vote guard and daily cap, plus Quest Log moderation (hide/exclude, Keeper only).
    - _Requirements: 2.3, 4.1, 4.2, 5.1_
  - [ ] 3.5 Wire XP awards into agent-codex `createNote` (50) and `moderateAgent` approval (500).
    - _Requirements: 5.1_
  - [ ]* 3.6 Write an integration test: 50 random operations against `:memory:`, then aggregates equal the SQL recompute and `total_xp` equals the ledger sum.
    - _Requirements: 2.2, 5.4_

- [ ] 4. Build the quest scroll
  - Build the 3-step quest scroll screen with an Agent picker, gem rating radio groups, skill icons, quick tags, and the existing-log redirect. The Codex card button pre-selects the Agent.
  - _Requirements: 1.1–1.6_

- [ ] 5. Build the Tavern board
  - Build the Tavern board (notice board, `QuestLogCard`, filters, sort, pagination), the quest detail screen (helpful, report, edit/delete), and the top-5 slot on the Codex card.
  - Implement `AvatarThumb` on top of the shared `AvatarRenderer` and `TextureCache`.
  - _Requirements: 3.1–3.4, 4.1, 4.2_

- [ ] 6. Add game feedback
  - Add `XpDrop`, the level-up chatbox message, `Fireworks` (reduced-motion aware), and "You have unlocked: …" messages driven by `QuestOutcome`. Add optional 8-bit level-up sound (off by default).
  - _Requirements: 5.5, 5.6, 6.1_

- [ ] 7. Build the Hiscores
  - Build the Hiscores screen with the Agents and Adventurers tabs, board selector, Quest_Type scope, competition ranks, pagination, and search-to-page.
  - _Requirements: 7.1–7.4_

- [ ] 8. Set up guest and demo data
  - On Guest User creation, seed `XP_TABLE[14] − 100` XP (`xp_entry` reason `SEED`).
  - Add Guest cleanup at startup through the normal delete path, and the `--repair-xp` flag.
  - _Requirements: 9.1, 9.2_

- [ ] 9. Final checkpoint: full demo path
  - Complete the smoke test: guest login → create Adventurer → equip Kiroween gear → Codex card → write Quest Log → "+250 XP" and level-up message → Hiscores. Save snapshots of every screen.
  - Build the `web` preset, run the demo path manually in Chrome, Firefox, and Safari, and reload to confirm persistence.
  - Make sure all tests pass, and ask the user if questions arise.
