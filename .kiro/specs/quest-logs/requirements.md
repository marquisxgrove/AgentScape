# Requirements Document: Quest Logs, Questing XP and Hiscores

## Introduction

A **Quest Log** is a player's review of a real experience using an Agent for a specific task. Players write Quest Logs at the **Tavern** quest board. The overall and per-Skill ratings feed the Grand Codex Skill levels.

Contributions earn **Questing XP**. XP raises the player's **Questing level**, with classic level-up messages, and automatically unlocks cosmetics in the Costumer. The **Hiscores** rank Agents by Skill and players by Questing XP.

## Glossary

- **Quest_Log**: A review with an Agent, optional Agent version, Quest_Type, overall rating (1–5), optional per-Skill ratings (1–5), title, body, "would recommend", and quick tags.
- **Adventurer_Snapshot**: The author's active Adventurer name, Appearance, and Loadout, frozen when the Quest Log was written.
- **Helpful_Vote**: A user marking another user's Quest Log as helpful.
- **XP_Entry**: An append-only ledger row awarding Questing XP to a user for a reason.
- **Questing_Level**: A level from 1 to 99 derived from total XP through the XP_Table.
- **XP_Table**: `XP_TABLE[L] = floor(40 · (L − 1)^1.75)` for L = 1..99 (level 1 = 0 XP).
- **Tavern**: The Tavern quest board screen.
- **Hiscores**: The Hiscores leaderboards screen.

## Requirements

### Requirement 1: Write a Quest Log

**User Story:** As a player who just used an AI tool, I want to log how it went, so that others can learn from my experience.

#### Acceptance Criteria

1. THE quest scroll form SHALL collect the following in three steps:
   1. Agent (approved only), optional version (≤ 40 chars), and Quest_Type.
   2. Overall rating 1–5 (required), plus 0–10 Skill ratings (1–5 each).
   3. Title (5–80 chars), body (20–2,000 chars), "would recommend" (yes/no), and up to 3 pro tags and 3 con tags from fixed lists.
2. WHEN the user submits a valid Quest Log, THE system SHALL save it with an Adventurer_Snapshot of the active Adventurer.
3. THE system SHALL allow one Quest Log per (user, Agent, version). IF one already exists, THEN THE form SHALL open it for editing instead.
4. THE system SHALL limit each user to 5 new Quest Logs per rolling hour.
5. IF the user has no Adventurer, THEN THE system SHALL send them to the Costumer first with the message "You need an Adventurer to go questing."
6. The body SHALL be rendered as plain text with line breaks preserved (no HTML).

### Requirement 2: Ratings feed the Codex

**User Story:** As a player, I want Codex levels to update right after I log a quest, so that my contribution feels immediate.

#### Acceptance Criteria

1. WHEN a Quest Log is created, edited, or deleted, THE system SHALL update `agent_skill_stat` for every affected (Skill, Quest_Type) in the same database transaction. This includes the pseudo-skill `OVERALL` for the overall rating.
2. After any sequence of creates, edits, and deletes, the stored aggregates SHALL equal the aggregates recomputed from all non-deleted Quest Logs.
3. Hidden Quest Logs SHALL remain counted in aggregates unless a Codex Keeper also marks them as "excluded". Excluding SHALL remove their ratings from aggregates.

### Requirement 3: Tavern quest board

**User Story:** As a player, I want to browse Quest Logs on a tavern notice board, so that I can read other adventurers' experiences.

#### Acceptance Criteria

1. THE Tavern SHALL list non-hidden Quest Logs as pinned parchment notes, 12 per page, filterable by Agent and Quest_Type and sortable by newest or most helpful.
2. Each note SHALL show the Adventurer_Snapshot avatar thumbnail and name, Agent, version, Quest_Type, overall rating as 1–5 gems, recommend flag, tags, title, and a body excerpt.
3. THE Agent examine card SHALL show that Agent's 5 most helpful Quest Logs.
4. Authors SHALL be able to edit or delete their own Quest Logs.

### Requirement 4: Helpful votes and reports

**User Story:** As a player, I want to mark Quest Logs as helpful, so that the best experiences rise to the top.

#### Acceptance Criteria

1. A user SHALL be able to toggle one Helpful_Vote per Quest Log, but not on their own.
2. A user SHALL be able to report a Quest Log once. Codex Keepers SHALL be able to hide or exclude it.

### Requirement 5: Questing XP

**User Story:** As a player, I want to earn XP for contributing, so that writing good Quest Logs feels rewarding.

#### Acceptance Criteria

1. THE system SHALL award XP through XP_Entries:
   - 250 for creating a Quest Log
   - 50 for creating a Note
   - 25 to the author per Helpful_Vote received, capped at 250 XP per author per UTC day
   - 500 for a proposal approved by a Codex Keeper
2. Each XP_Entry SHALL be unique by (user, reason, reference), so that retries never double-award.
3. WHEN a Quest Log is deleted, THE system SHALL delete the XP_Entries that reference it (creation and helpful XP).
4. A user's total XP SHALL equal the sum of their XP_Entries, and their Questing_Level SHALL be the largest L with `XP_TABLE[L] ≤ totalXP`.
5. WHEN an action raises the user's Questing_Level, THE Chatbox SHALL print "Congratulations, you just advanced a Questing level! You are now level N." and play a fireworks animation (static when "Reduce motion" is on).
6. WHEN XP is awarded, THE UI SHALL show a floating "+N XP" drop near the XP counter.

### Requirement 6: Level unlocks

**User Story:** As a player, I want new cosmetics to unlock as I level up, so that I have something to show for my contributions.

#### Acceptance Criteria

1. WHEN a user's Questing_Level reaches the level in an Item's `level` unlock rule, THE system SHALL create an ItemUnlock (source `LEVEL`) and print "You have unlocked: <Item>!".
2. Unlocks SHALL be permanent even if XP later decreases.
3. Granting unlocks SHALL be idempotent.

### Requirement 7: Hiscores

**User Story:** As a player, I want leaderboards, so that I can see top Agents and top contributors.

#### Acceptance Criteria

1. THE Agent Hiscores SHALL rank approved Agents by Overall level, Total_Level, or any Skill_Level, with an optional Quest_Type scope. Unranked Agents SHALL be excluded from that board.
2. THE Adventurer Hiscores SHALL rank users (players, Guests, and NPC Adventurers) by total Questing XP and show their active Adventurer's name, thumbnail, Questing_Level, and XP.
3. Ranks SHALL use standard competition ranking (equal scores share a rank, and the next rank skips; for example, 1, 2, 2, 4). Ties are ordered by name.
4. Hiscores SHALL be paginated 25 per page and support "search by name" jumping to that entry's page.

### Requirement 8: Activity feed

**User Story:** As a player, I want community actions to appear in the chatbox, so that the world feels alive.

#### Acceptance Criteria

1. WHEN a Quest Log is created, THE system SHALL post an ActivityEvent such as "Mira completed a Coding quest with Kiro (4/5)."
2. WHEN a user reaches a Questing_Level that is a multiple of 10, or 99, THE system SHALL post an ActivityEvent announcing it.

### Requirement 9: Guest and demo data

**User Story:** As a judge, I want to try every feature immediately, so that I can evaluate the app quickly.

#### Acceptance Criteria

1. WHEN a Guest User is created, THE system SHALL seed it with `XP_TABLE[14] − 100` XP (Questing level 13). This makes all early unlocks and the Reaper's Scythe claimable, and the first Quest Log triggers a level-up.
2. WHEN the app starts, THE system SHALL delete Guest Users whose last activity is older than 24 hours, together with their content, except the Guest remembered on this device (game-shell Requirement 2.1) until it Logs out. Their ratings SHALL be removed from aggregates through the normal delete path.
