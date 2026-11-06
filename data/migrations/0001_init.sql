-- 0001_init: local profiles, app key/value state, and the chatbox activity feed.
-- Append-only: never edit after merge; add 0002_*.sql instead.

CREATE TABLE user (
  id           TEXT PRIMARY KEY,                       -- ULID
  name         TEXT NOT NULL,
  kind         TEXT NOT NULL DEFAULT 'PLAYER' CHECK (kind IN ('PLAYER','GUEST','NPC')),
  role         TEXT NOT NULL DEFAULT 'USER'   CHECK (role IN ('USER','KEEPER')),
  github_id    INTEGER UNIQUE,                         -- NULL unless linked through GitHub_Login
  github_login TEXT,
  settings     TEXT NOT NULL DEFAULT '{}',             -- JSON { plainFont, reduceMotion, sound }
  created_at   TEXT NOT NULL,                          -- ISO-8601 UTC
  last_seen_at TEXT NOT NULL
) STRICT;

CREATE TABLE app_state (                               -- key/value, e.g. remembered_user_id
  key   TEXT PRIMARY KEY,
  value TEXT NOT NULL
) STRICT;

CREATE TABLE activity_event (
  id         INTEGER PRIMARY KEY AUTOINCREMENT,
  kind       TEXT NOT NULL CHECK (kind IN ('QUEST_LOG_CREATED','LEVEL_UP','AGENT_APPROVED','NOTE_CREATED')),
  text       TEXT NOT NULL,                            -- pre-rendered game-voice text
  created_at TEXT NOT NULL
) STRICT;
CREATE INDEX activity_event_created_at ON activity_event(created_at);
