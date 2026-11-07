-- 0002_adventurers: player characters and account-wide cosmetic unlocks.
-- Append-only: never edit after merge; add 0003_*.sql instead.

CREATE TABLE adventurer (
  id         TEXT PRIMARY KEY,                           -- ULID
  user_id    TEXT NOT NULL REFERENCES user(id) ON DELETE CASCADE,
  name       TEXT NOT NULL,
  name_key   TEXT NOT NULL UNIQUE,                       -- domain::nameKey(name): global uniqueness
  appearance TEXT NOT NULL,                              -- JSON Appearance
  loadout    TEXT NOT NULL,                              -- JSON Loadout
  is_active  INTEGER NOT NULL DEFAULT 0 CHECK (is_active IN (0,1)),
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL
) STRICT;
CREATE INDEX adventurer_user ON adventurer(user_id);
-- At most one active Adventurer per user, enforced by the database as well as the service.
CREATE UNIQUE INDEX adventurer_one_active ON adventurer(user_id) WHERE is_active = 1;

CREATE TABLE item_unlock (
  user_id     TEXT NOT NULL REFERENCES user(id) ON DELETE CASCADE,
  item_id     TEXT NOT NULL,
  source      TEXT NOT NULL CHECK (source IN ('LEVEL','KIROWEEN','GRANT')),
  unlocked_at TEXT NOT NULL,
  PRIMARY KEY (user_id, item_id)
) STRICT;
