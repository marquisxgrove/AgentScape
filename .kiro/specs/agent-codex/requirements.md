# Requirements Document: Grand Codex (Agent Knowledge Base)

## Introduction

The **Grand Codex** is AgentScape's knowledge base of AI models and AI tools (**Agents**). It works like an early-2000s fansite item database, styled as an in-game library. Each Agent has an **examine card** with:

- factual details
- **Skill levels (1–99)** computed from community Quest Log ratings
- community-voted **Boons** (strengths) and **Banes** (weaknesses)

Players can compare Agents side by side at the **Proving Grounds**. During Kiroween, the top Banes appear as tombstones in the **Hall of Hallucinations**.

Quest Logs (the source of ratings) are specified in `quest-logs`. This spec owns the Skill list, the level formula, and the `agent_skill_stat` aggregate that Quest Logs update.

The Codex starts from a seeded **community snapshot** (Agents, NPC Adventurers, their Quest Logs and Notes). The player's contributions are stored locally and update it immediately.

## Glossary

- **Agent**: A reviewable AI model (`MODEL`) or AI tool (`TOOL`). Its status is `PENDING`, `APPROVED`, or `REJECTED`.
- **Skill**: One of the 10 rating dimensions listed in `product.md`.
- **Skill_Level**: An integer from 1 to 99 derived from ratings, or *unranked* when there are fewer than 3 ratings.
- **Total_Level**: The sum of an Agent's Skill_Levels, where unranked Skills count as 1.
- **Quest_Type**: The task category of a rating: `CODING`, `DEBUGGING`, `WRITING`, `RESEARCH`, `DATA`, `BRAINSTORMING`, `AGENTIC`, or `OTHER`.
- **Note**: A Boon or Bane (≤ 280 characters), optionally tagged with a Skill and an Agent version.
- **Note_Score**: The Wilson score lower bound (95%) of agree votes out of total votes.
- **Codex_Keeper**: A user with role `KEEPER`, the local moderator.

## Requirements

### Requirement 1: Browse the Codex

**User Story:** As a player, I want to browse and search all Agents, so that I can find the right AI for my task.

#### Acceptance Criteria

1. THE Codex list SHALL show only `APPROVED` Agents, 20 per page, each with name, vendor, kind, Total_Level, and Quest Log count.
2. THE Codex list SHALL support case-insensitive search over name and vendor.
3. THE Codex list SHALL support filtering by kind and vendor and SHALL support selecting a Quest_Type that scopes all displayed levels.
4. THE Codex list SHALL support sorting by Total_Level, a chosen Skill_Level, name, or Quest Log count. Unranked levels sort last.
5. THE Codex SHALL be rendered as a library bookshelf in the Viewport, with an equivalent keyboard- and gamepad-navigable table.

### Requirement 2: Agent examine card

**User Story:** As a player, I want a detailed card for each Agent, so that I can quickly understand what it's good and bad at.

#### Acceptance Criteria

1. THE examine card screen SHALL show the name, vendor, kind, examine text (≤ 200 chars), about text, an optional official website link, and the date the card was last updated.
   - WHEN the user activates the website link, THE system SHALL ask "Leave AgentScape to visit <host>?" and, on confirmation, open it in the default browser.
2. THE examine card SHALL show an **Overall level** (from the pseudo-skill `OVERALL`, which holds Quest Log overall ratings) prominently, like a combat level. It SHALL also show a skills grid of all 10 Skills with Skill_Level (or "–" when unranked), the number of ratings, and Total_Level. The Overall level is not part of Total_Level.
3. WHEN the user selects a Quest_Type, THE skills grid SHALL show levels computed only from ratings of that Quest_Type.
4. THE examine card SHALL show the top 5 Boons and top 5 Banes by Note_Score, with buttons to view all.
5. THE examine card SHALL not display any vendor logos or third-party images.

### Requirement 3: Skill level formula

**User Story:** As a player, I want Skill levels that are fair and hard to game, so that I can trust them.

#### Acceptance Criteria

1. GIVEN `n` ratings `r_i ∈ {1..5}` for an (Agent, Skill, Quest_Type scope), THE system SHALL compute `bayes = (C·m + Σr_i) / (C + n)` with prior mean `m = 3` and weight `C = 5`.
2. THE system SHALL compute `Skill_Level = clamp(1 + round((bayes − 1) / 4 × 98), 1, 99)`.
3. IF `n < 3`, THEN THE Skill_Level SHALL be unranked.
4. THE "All quest types" scope SHALL equal the computation over the union of all Quest_Type ratings.
5. THE stored aggregate SHALL be `(sum, count)` per (Agent, Skill, Quest_Type), so that any scope can be computed by adding aggregates.

### Requirement 4: Propose a new Agent

**User Story:** As a player, I want to add a missing AI tool to the Codex, so that the community can review it.

#### Acceptance Criteria

1. WHEN a user submits a proposal (name, vendor, kind, examine text, optional about text and website), THE system SHALL create a `PENDING` Agent.
2. THE system SHALL derive a slug from the name. IF an Agent with the same slug already exists (in any status), THEN THE system SHALL reject the proposal and offer to open the existing Agent.
3. THE system SHALL limit each user to 3 proposals per rolling 24 hours.
4. WHEN a Codex_Keeper approves or rejects a proposal, THE system SHALL update the status and post an ActivityEvent for approvals.
5. IF a website URL is provided, THEN THE system SHALL accept only `https:` URLs.

### Requirement 5: Boons and Banes

**User Story:** As a player, I want to post and vote on short strengths and weaknesses, so that the most agreed-upon insights rise to the top.

#### Acceptance Criteria

1. WHEN a user submits a Note (kind, text 10–280 chars, optional Skill, optional version), THE system SHALL attach it to the Agent.
2. THE system SHALL limit each user to 10 Notes per rolling hour.
3. Each user SHALL have at most one vote (`agree` or `disagree`) per Note and SHALL be able to change or retract it.
4. A user SHALL NOT be able to vote on their own Note.
5. THE system SHALL order Notes by Note_Score descending, then by newest.
6. Note text SHALL be rendered as plain text (no HTML or markdown).

### Requirement 6: Proving Grounds (compare)

**User Story:** As a player, I want to compare 2–3 Agents side by side, so that I can pick the best one for a task.

#### Acceptance Criteria

1. THE Proving Grounds SHALL let the user select 2 or 3 approved Agents and a Quest_Type scope.
2. THE comparison SHALL show each Skill as a row with each Agent's Skill_Level. The highest ranked level in each row SHALL be highlighted in green (ties highlight all).
3. THE Proving Grounds SHALL remember each user's last comparison (Agents and Quest_Type) and restore it when reopened.
4. WHEN the user presses "Copy", THE system SHALL copy the comparison to the clipboard as a plain-text table.

### Requirement 7: Hall of Hallucinations (Kiroween)

**User Story:** As a player during Kiroween, I want to visit a graveyard of famous AI failures, so that the community's best-known weaknesses are memorable and fun.

#### Acceptance Criteria

1. WHILE Kiroween_Mode is active, THE Graveyard SHALL display up to 13 tombstones, one for each top Bane by Note_Score that has at least 3 votes and is not hidden.
2. Each tombstone SHALL show the Agent name, the tagged Skill (if any), and the Bane text as an epitaph.
3. WHEN the user selects a tombstone, THE system SHALL open the Agent's examine card.
4. THE Graveyard SHALL provide an equivalent keyboard- and gamepad-navigable list of tombstones.

### Requirement 8: Codex Keeper tools (local moderation)

**User Story:** As the keeper of my local Codex, I want to handle reports and proposals, so that the Codex stays useful and respectful.

#### Acceptance Criteria

1. THE Settings tab SHALL offer "Codex Keeper tools". Turning it on (after a confirmation) SHALL give the current user the `KEEPER` role, and turning it off SHALL return them to `USER`.
2. WHEN a user reports a Note or Agent with a reason, THE system SHALL create a Report. Each user SHALL be able to report a target only once.
3. Codex_Keepers SHALL see a Keeper queue screen with unresolved Reports and pending proposals, and SHALL be able to approve or reject proposals, hide or unhide Notes, and resolve Reports.
4. Hidden Notes SHALL be excluded from all lists, Note_Score rankings, and the Graveyard.

### Requirement 9: Seed data

**User Story:** As a judge, I want the Codex to be populated on first run, so that the demo is meaningful immediately.

#### Acceptance Criteria

1. WHEN the Local_Store is created, and WHEN the app is started with `--reseed`, THE seed loader SHALL upsert at least 12 approved Agents (models and tools) from the embedded seed data, with neutral, factual examine text and no logos.
2. THE seed loader SHALL create NPC Adventurers (users of kind `NPC`) with Adventurers, Questing XP, Quest Logs, and Notes, so that most Skills are ranked and the Hiscores and Tavern are populated.
3. Running the seed loader repeatedly SHALL not create duplicates, and it SHALL not modify or delete the player's own content.
