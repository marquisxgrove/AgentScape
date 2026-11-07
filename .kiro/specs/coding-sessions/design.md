# Design Document: Coding Sessions

> Depends on: `game-shell` (screens, navigation, Chatbox), `character-creator` (the Adventurer and its renderer), `agent-codex` (approved familiars) and `quest-logs` (the XP ledger, levels and unlocks).

## Overview

A session is a start time, an optional familiar, a "settled up to" mark and, once over, an end time. Nothing runs in the background: XP is computed from timestamps whenever the app **settles** — about once a minute while the user is signed in, on sign-in (for time spent away), and on Stop. Because settling is path-independent (Property 1), it doesn't matter how often or how irregularly that happens, or whether the app was closed in between.

The Wilds scene is cosmetic and also computed from timestamps: `game::wilds_sim` turns (session seed, elapsed seconds) into the current fight or skilling action and the tallies so far, so the scene and the "While you were away…" tallies agree after any restart.

## Data

Migration `0007_coding_sessions.sql`:

| Table | Columns | Notes |
|---|---|---|
| `coding_session` | `id`, `user_id`, `agent_id` (NULL = solo), `started_at`, `settled_until`, `ended_at` (NULL while running) | Partial unique index: one running session per user. |
| `coding_session_day` | `session_id`, `user_id`, `day` (UTC `YYYY-MM-DD`), `seconds` | Counted seconds. The daily limit is checked against `SUM(seconds)` over a user's rows for a day. |
| `xp_entry` | `reason` gains `CODING` | Rebuilt (SQLite can't alter a CHECK). One entry per session per day, `ref_id = '<sessionId>:<day>'`, raised as the day's seconds grow. |

## Domain (`src/domain/coding_session.*`, pure)

- `CODING_XP_PER_MINUTE = 1`, `CODING_SESSION_MAX = 4 h`, `CODING_DAILY_CAP = 8 h`.
- `codingCountedUntil(start, now)` = `clamp(now, start, start + 4 h)`.
- `creditCodingInterval(used, from, to)` splits `[from, to)` at UTC midnights and adds to each day `min(part, cap − used[day])`, updating `used`. Since `min(a + b, r) = min(a, r) + min(b, r − min(a, r))`, crediting in pieces equals crediting at once.
- `codingXpFor(seconds)` = whole minutes × 1 XP. XP is derived from each (session, day)'s seconds, never accumulated from per-settle roundings, so partial minutes are never lost or double-paid.

## Services

- `XpService::raiseTo(user, reason, ref, amount)`: upserts the entry to `amount` and pays only the difference, through the same level / unlock / milestone path as `award` (shared `applyGain`).
- `CodingService` (`start`, `settle`, `stop`, `running`, `sessionXp`, `todayXp`). Each call is one transaction; XP events are published after commit with `publishGrant`. `settle` reads `used` for the touched days, calls `creditCodingInterval`, upserts `coding_session_day`, raises each day's XP entry, moves `settled_until`, and ends the session at its 4-hour limit. `start` first settles, so an expired session never blocks a new one.
- `QuestService`, `NoteService`, `KeeperService` and `CodingService` each own an `ActivityService` and an `XpService` that refers to it. Their move constructors rebuild both, so moving the App's service bundle can't leave `xp_` pointing into the moved-from object.

## App and screens

- `App::settleCoding(away)` runs from `drawShell` every 60 s and from `adoptUser`. On sign-in, if a minute or more was settled, it prints "While you were away…" with the XP and the scene's tallies over the settled stretch. Reaching 4 hours prints "Your coding session reached 4 hours and has ended."
- `WildsScreen`: Back, a 300×200 scene, and a column that is either the familiar picker + Start, the running status + Stop, or the summary (+ "Write a Quest Log" with the familiar preselected). It re-reads the status every 5 s; the scene animates from wall-clock time.
- `WorldScreen`: The Wilds hotspot (south road, with a signpost on the map) and Places entry; a two-line status under Places while a session runs.

## Scene (`src/game/wilds_sim.*`, pure)

- Combat: one bug every 12 s (`FIGHT_SECONDS`), 8 hits 1.4 s apart; `fightDamage` splits the bug's health over the hits (always summing exactly); with a familiar, every other hit is the familiar's. `squashedTotal` counts completed fights.
- Skilling: an action every 5 s; the spot (tree, rock, pond) rotates every 10 minutes from a seeded start; tallies are computed in O(1) from whole rotations.
- Seed: FNV-1a of the session id. Reduce motion: no lunges, bobbing, fading or floating numbers.

## Art

`tools/gen_wilds_art.py` (standard library, deterministic) draws `wilds.png`, `bugs.png` (5 bugs × 2 frames), `wisp.png`, `tree.png`, `rock.png` and `pond.png`; `tools/gen_map_art.py` adds the signpost. All logged in `docs/ASSETS.md`.

## Correctness Properties

**Property 1: Settling is path-independent.** *For any* interval, prior per-day usage and cut points, crediting the pieces in order yields the same per-day additions and usage as crediting the whole interval. **Validates: Requirement 2.4**

**Property 2: No day counts more than the daily limit.** *For any* crediting, every day's usage stays within 8 hours and each day adds at most what it had room for. **Validates: Requirement 2.2**

**Property 3: Counted time never exceeds real time.** Added time ≤ the interval, usage grows by exactly what was added, and a session-length interval on empty days counts in full. **Validates: Requirements 2.1, 2.2**

**Property 4: A session counts at most four hours.** `codingCountedUntil` stays within `[start, start + 4 h]` and equals `now` inside it. **Validates: Requirement 1.5**

## Testing

- Property tests: `tests/property/coding_sessions_props.cpp` (Properties 1–4, 1000 runs for the scoring ones) plus a midnight example.
- Unit tests: `tests/unit/wilds_sim_test.cpp` (damage sums, fight timeline, determinism, skilling tallies and rotation).
- Integration tests: `tests/integration/coding_service_test.cpp` (pay per minute, one session at a time, unknown familiars, time away and the 4-hour stop, midnight and the daily limit, settle-often equals settle-once, level-ups).
