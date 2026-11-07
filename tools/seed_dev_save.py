#!/usr/bin/env python3
"""Dev/QA helper: writes a remembered PLAYER (and optionally an Adventurer) into an existing AgentScape
save so screens that need a signed-in user can be rendered headlessly.

    python3 tools/seed_dev_save.py <dir-with-agentscape.db> <PlayerName> [AdventurerName]

Run the app once with --data-dir <dir> first so the database and its schema exist.
"""
import json
import os
import sqlite3
import sys

if len(sys.argv) < 3:
    sys.exit(__doc__)
db = os.path.join(sys.argv[1], 'agentscape.db')
name = sys.argv[2]
adventurer = sys.argv[3] if len(sys.argv) > 3 else None

c = sqlite3.connect(db)
uid = '01P00000000000000000000' + name[:3].upper()
stamp = '2026-11-01T00:00:00Z'
c.execute("INSERT OR REPLACE INTO user(id,name,kind,role,settings,created_at,last_seen_at) "
          "VALUES (?,?,'PLAYER','USER','{}',?,?)", (uid, name, stamp, stamp))
c.execute("INSERT OR REPLACE INTO app_state(key,value) VALUES ('remembered_user_id',?)", (uid,))
if adventurer:
    look = {"bodyType": "A", "hair": 4, "facialHair": 2, "torso": 2, "arms": 1, "legs": 0,
            "colors": {"hair": 3, "torso": 7, "legs": 12, "feet": 1, "skin": 2}}
    gear = {"Weapon": "wooden_stick", "Body": "plain_tunic", "Legs": "plain_trousers", "Feet": "leather_boots"}
    c.execute("INSERT OR REPLACE INTO adventurer(id,user_id,name,name_key,appearance,loadout,is_active,"
              "created_at,updated_at) VALUES (?,?,?,?,?,?,1,?,?)",
              ('01A0000000000000000000ADV1', uid, adventurer, adventurer.lower(),
               json.dumps(look), json.dumps(gear), stamp, stamp))
c.commit()
print('seeded', name, adventurer or '(no adventurer)')
