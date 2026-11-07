-- 0004_proving_grounds: each user's last Proving Grounds comparison, restored when they come back.
-- Append-only: never edit after merge; add 0005_*.sql instead.

CREATE TABLE proving_selection (
  user_id    TEXT PRIMARY KEY REFERENCES user(id) ON DELETE CASCADE,
  slugs      TEXT NOT NULL,                              -- JSON array of 0..3 Agent slugs
  quest_type TEXT,                                       -- a Quest Type storage name, or NULL for "All"
  updated_at TEXT NOT NULL
) STRICT;
