# Requirements Document: Coding Sessions

## Introduction

Players already rate the AI familiars they code with. This spec rewards the coding itself: a player starts a **Coding_Session** before they sit down to work, either with a familiar from the Codex or solo, and stops it when they are done. The time in between earns Questing XP passively, even while the game is closed. Meanwhile their Adventurer is busy in **The Wilds**: fending off bug monsters alongside the familiar, or, when coding solo, chopping, mining and fishing on its own.

XP from coding is deliberately modest (1 XP a minute, at most 8 hours a day), so writing Quest Logs stays the faster way to level, and a forgotten timer can't run away with the Hiscores.

## Glossary

- **Coding_Session**: one stretch of coding, from "Start session" to "Stop session" (or its 4-hour limit). It is solo or with one approved familiar (Agent).
- **Counted time**: the part of a session that earns XP, after the session and daily limits.
- **Settling**: crediting the counted time since the last settle as XP. It happens while the game runs, on sign-in (for time spent away) and on Stop.
- **The Wilds**: the place outside Promptmoor where sessions are started, stopped and watched.
- **Bug monster**: an original, coding-themed foe (for example the Null-Pointer Imp). Purely cosmetic.

## Requirements

### Requirement 1: Start and stop a session

**User Story:** As a player, I want to tell the game when I'm coding and who with, so that the time counts.

#### Acceptance Criteria

1. THE Wilds SHALL offer a choice of "Solo" or any approved familiar, and a "Start session" button.
2. A user SHALL have at most one running Coding_Session. Starting another while one runs SHALL be refused with a game-voice message.
3. WHEN the user presses "Stop session", THE system SHALL settle the session, end it, and show a summary: duration, XP earned and the activity tallies. WHEN the session was with a familiar, THE summary SHALL offer "Write a Quest Log" for that familiar.
4. A running session SHALL keep counting while the app is closed, across restarts and page reloads.
5. A session SHALL count at most 4 hours. WHEN it reaches 4 hours, THE system SHALL end it at that moment and say so in the Chatbox.
6. An Adventurer is needed to go to The Wilds; a user without one SHALL be sent to the Costumer, as for questing.

### Requirement 2: Passive XP

**User Story:** As a player, I want to level up a little just by coding, without it overshadowing Quest Logs.

#### Acceptance Criteria

1. Counted time SHALL earn 1 Questing XP per whole minute.
2. At most 8 hours of counted time SHALL earn XP per UTC day, across all of the user's sessions. Time beyond that SHALL still run the session but earn nothing. A session crossing midnight SHALL count each part against its own day.
3. WHILE the app runs and a user with a running session is signed in, THE system SHALL settle at least once a minute. ON sign-in, THE system SHALL settle the time spent away and say "While you were away…" with the XP and tallies.
4. Settling SHALL be path-independent: settling at any set of moments SHALL credit exactly the same XP per day as settling once at the end.
5. Coding XP SHALL be recorded in the XP ledger with the reason `CODING`, one entry per session per UTC day that grows as time is settled, and SHALL raise the level, grant level unlocks and post milestones exactly like other XP.

### Requirement 3: The Wilds

**User Story:** As a player, I want to see my Adventurer working while I code, so that the session feels alive.

#### Acceptance Criteria

1. WHILE a session with a familiar runs, THE Wilds SHALL show the Adventurer and the familiar fighting bug monsters, one after another, with hit numbers and a health bar. WHILE a solo session runs, THE Wilds SHALL show the Adventurer skilling, rotating between woodcutting, mining and fishing.
2. What happens SHALL be determined by the session and its elapsed time only, so the scene and its tallies (bugs squashed; logs, ore and fish gathered) are the same after a restart.
3. THE Wilds SHALL show the elapsed time, the XP this session, and today's coding XP against the daily limit.
4. WHILE "Reduce motion" is on, THE scene SHALL be still: no lunges, bobbing or floating numbers.
5. WHILE a session runs, THE World SHALL show a short line saying so, with the elapsed time.
6. THE World_Map SHALL show The Wilds as a Hotspot at the south road out of town, and in the Places list. THE Wilds SHALL have "Back", and `Escape` SHALL go back (game-shell Requirement 7.7).
