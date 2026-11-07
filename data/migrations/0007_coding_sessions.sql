-- 0007_coding_sessions: passive XP for time spent coding (coding-sessions spec).
-- Append-only: never edit after merge; add 0008_*.sql instead.

CREATE TABLE coding_session (
  id            TEXT PRIMARY KEY,                                    -- ULID
  user_id       TEXT NOT NULL REFERENCES user(id) ON DELETE CASCADE,
  agent_id      TEXT REFERENCES agent(id) ON DELETE SET NULL,        -- NULL: coding solo
  started_at    TEXT NOT NULL,
  settled_until TEXT NOT NULL,                                       -- counted (and paid) up to this moment
  ended_at      TEXT                                                 -- NULL while running
) STRICT;
-- One running session per user (Requirement 1.2).
CREATE UNIQUE INDEX coding_session_running ON coding_session(user_id) WHERE ended_at IS NULL;
CREATE INDEX coding_session_user ON coding_session(user_id, started_at);

-- Counted seconds per session per UTC day. The daily limit (Requirement 2.2) is checked against the sum over a
-- user's sessions for that day.
CREATE TABLE coding_session_day (
  session_id TEXT NOT NULL REFERENCES coding_session(id) ON DELETE CASCADE,
  user_id    TEXT NOT NULL REFERENCES user(id) ON DELETE CASCADE,
  day        TEXT NOT NULL,                                          -- UTC date, YYYY-MM-DD
  seconds    INTEGER NOT NULL CHECK (seconds >= 0),
  PRIMARY KEY (session_id, day)
) STRICT;
CREATE INDEX coding_session_day_user ON coding_session_day(user_id, day);

-- xp_entry gains the CODING reason. SQLite can't alter a CHECK constraint, so the table is rebuilt; nothing
-- references it, and every row is copied as is.
CREATE TABLE xp_entry_new (
  id         TEXT PRIMARY KEY,
  user_id    TEXT NOT NULL REFERENCES user(id) ON DELETE CASCADE,
  reason     TEXT NOT NULL CHECK (reason IN ('QUEST_LOG','NOTE','HELPFUL_RECEIVED','AGENT_APPROVED','SEED','CODING')),
  ref_id     TEXT NOT NULL,   -- questLogId, noteId, '<questLogId>:<voterId>', agentId, 'seed', '<sessionId>:<day>'
  amount     INTEGER NOT NULL,
  created_at TEXT NOT NULL,
  UNIQUE (user_id, reason, ref_id)
) STRICT;
INSERT INTO xp_entry_new(id, user_id, reason, ref_id, amount, created_at)
  SELECT id, user_id, reason, ref_id, amount, created_at FROM xp_entry;
DROP TABLE xp_entry;
ALTER TABLE xp_entry_new RENAME TO xp_entry;
CREATE INDEX xp_entry_user ON xp_entry(user_id);
