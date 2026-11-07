-- 0005_quests: Quest Logs, helpful votes, the Questing XP ledger.
-- Append-only: never edit after merge; add 0006_*.sql instead.

CREATE TABLE quest_log (
  id            TEXT PRIMARY KEY,
  author_id     TEXT NOT NULL REFERENCES user(id) ON DELETE CASCADE,
  agent_id      TEXT NOT NULL REFERENCES agent(id) ON DELETE CASCADE,
  agent_version TEXT NOT NULL DEFAULT '',
  quest_type    TEXT NOT NULL,
  overall       INTEGER NOT NULL CHECK (overall BETWEEN 1 AND 5),
  ratings       TEXT NOT NULL DEFAULT '{}',             -- JSON map<Skill, 1..5>
  title         TEXT NOT NULL,
  body          TEXT NOT NULL,
  recommend     INTEGER NOT NULL CHECK (recommend IN (0,1)),
  tags          TEXT NOT NULL DEFAULT '[]',             -- JSON string[] (pros then cons)
  snapshot      TEXT NOT NULL,                          -- JSON Adventurer_Snapshot, frozen at creation
  helpful_count INTEGER NOT NULL DEFAULT 0 CHECK (helpful_count >= 0),
  hidden        INTEGER NOT NULL DEFAULT 0 CHECK (hidden IN (0,1)),
  excluded      INTEGER NOT NULL DEFAULT 0 CHECK (excluded IN (0,1)),
  created_at    TEXT NOT NULL,
  updated_at    TEXT NOT NULL,
  UNIQUE (author_id, agent_id, agent_version)
) STRICT;
CREATE INDEX quest_log_agent_hidden ON quest_log(agent_id, hidden);
CREATE INDEX quest_log_created_at ON quest_log(created_at);

CREATE TABLE helpful_vote (
  quest_log_id TEXT NOT NULL REFERENCES quest_log(id) ON DELETE CASCADE,
  user_id      TEXT NOT NULL REFERENCES user(id) ON DELETE CASCADE,
  created_at   TEXT NOT NULL,
  PRIMARY KEY (quest_log_id, user_id)
) STRICT;

CREATE TABLE xp_entry (
  id         TEXT PRIMARY KEY,
  user_id    TEXT NOT NULL REFERENCES user(id) ON DELETE CASCADE,
  reason     TEXT NOT NULL CHECK (reason IN ('QUEST_LOG','NOTE','HELPFUL_RECEIVED','AGENT_APPROVED','SEED')),
  ref_id     TEXT NOT NULL,   -- questLogId, noteId, '<questLogId>:<voterId>', agentId, 'seed'
  amount     INTEGER NOT NULL,
  created_at TEXT NOT NULL,
  UNIQUE (user_id, reason, ref_id)
) STRICT;
CREATE INDEX xp_entry_user ON xp_entry(user_id);

-- Denormalised sum of the user's xp_entry rows, kept in step inside the same transaction as each ledger write.
-- The ledger is the source of truth; `--repair-xp` recomputes this column from it.
ALTER TABLE user ADD COLUMN total_xp INTEGER NOT NULL DEFAULT 0;
