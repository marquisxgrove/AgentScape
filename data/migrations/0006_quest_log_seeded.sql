-- 0006_quest_log_seeded: marks the sample Quest Logs that ship with the app.
-- Append-only: never edit after merge; add 0007_*.sql instead.
--
-- A seeded log is display-only. Its ratings are already part of the sample rating baseline, so it is always stored
-- excluded as well, and the services refuse to count it in (a Keeper may still hide it).
ALTER TABLE quest_log ADD COLUMN seeded INTEGER NOT NULL DEFAULT 0 CHECK (seeded IN (0,1));
