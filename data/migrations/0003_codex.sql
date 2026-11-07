-- 0003_codex: Agents, rating aggregates, Boons/Banes with votes, and reports.
-- Append-only: never edit after merge; add 0004_*.sql instead.

CREATE TABLE agent (
  id             TEXT PRIMARY KEY,                       -- ULID, or 'seed:agent:<slug>' for seeded rows
  slug           TEXT NOT NULL UNIQUE,
  name           TEXT NOT NULL,
  vendor         TEXT NOT NULL,
  kind           TEXT NOT NULL CHECK (kind IN ('MODEL','TOOL')),
  examine        TEXT NOT NULL,
  about          TEXT NOT NULL DEFAULT '',
  website        TEXT,                                   -- https: only
  status         TEXT NOT NULL DEFAULT 'PENDING' CHECK (status IN ('PENDING','APPROVED','REJECTED')),
  proposed_by_id TEXT REFERENCES user(id) ON DELETE SET NULL,
  created_at     TEXT NOT NULL,
  updated_at     TEXT NOT NULL
) STRICT;
CREATE INDEX agent_status ON agent(status);

-- One row per (Agent, stat, Quest Type): any scope is computed by adding rows (Requirement 3.5).
-- sum/count = seeded baseline + everything players added. seed_sum/seed_count remember how much of that is the
-- seeded community snapshot, so a reseed swaps only that part and never erases player ratings.
CREATE TABLE agent_skill_stat (
  agent_id   TEXT NOT NULL REFERENCES agent(id) ON DELETE CASCADE,
  skill      TEXT NOT NULL,                              -- a Skill storage name, or 'OVERALL'
  quest_type TEXT NOT NULL,
  sum        INTEGER NOT NULL DEFAULT 0,
  count      INTEGER NOT NULL DEFAULT 0,
  seed_sum   INTEGER NOT NULL DEFAULT 0,
  seed_count INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY (agent_id, skill, quest_type),
  -- Every aggregate must be a possible sum of `count` ratings in 1..5; a buggy delta fails loudly.
  CHECK (count >= 0 AND sum >= count AND sum <= 5 * count),
  CHECK (seed_count >= 0 AND seed_sum >= seed_count AND seed_sum <= 5 * seed_count)
) STRICT;

CREATE TABLE note (
  id         TEXT PRIMARY KEY,
  agent_id   TEXT NOT NULL REFERENCES agent(id) ON DELETE CASCADE,
  author_id  TEXT NOT NULL REFERENCES user(id) ON DELETE CASCADE,
  kind       TEXT NOT NULL CHECK (kind IN ('BOON','BANE')),
  skill      TEXT,
  version    TEXT,
  text       TEXT NOT NULL,
  agree      INTEGER NOT NULL DEFAULT 0,                 -- denormalised; updated with each note_vote change
  disagree   INTEGER NOT NULL DEFAULT 0,
  hidden     INTEGER NOT NULL DEFAULT 0 CHECK (hidden IN (0,1)),
  created_at TEXT NOT NULL
) STRICT;
CREATE INDEX note_agent_kind_hidden ON note(agent_id, kind, hidden);

CREATE TABLE note_vote (
  note_id TEXT NOT NULL REFERENCES note(id) ON DELETE CASCADE,
  user_id TEXT NOT NULL REFERENCES user(id) ON DELETE CASCADE,
  value   INTEGER NOT NULL CHECK (value IN (-1, 1)),
  PRIMARY KEY (note_id, user_id)
) STRICT;

CREATE TABLE report (
  id          TEXT PRIMARY KEY,
  target_type TEXT NOT NULL CHECK (target_type IN ('AGENT','NOTE','QUEST_LOG')),
  target_id   TEXT NOT NULL,
  reporter_id TEXT NOT NULL REFERENCES user(id) ON DELETE CASCADE,
  reason      TEXT NOT NULL,
  resolved    INTEGER NOT NULL DEFAULT 0 CHECK (resolved IN (0,1)),
  created_at  TEXT NOT NULL,
  UNIQUE (target_type, target_id, reporter_id)
) STRICT;
