#!/bin/bash
# Renders the README screenshots from the current dev build into docs/screenshots/readme/.
#
#   cmake --build --preset dev && ctest --preset dev -R smoke_demo_path   # the level-up shot comes from this run
#   tools/render_readme_shots.sh build/my-save
#
# The argument is a data folder with a remembered player whose active Adventurer is dressed for the shots (make one
# with `agentscape --data-dir <dir> --smoke` and then `python3 tools/seed_dev_save.py <dir> <Player> <Adventurer>`;
# equip gear in the Costumer). The script works on copies, so that folder is never changed. Headless: SDL's dummy
# video driver and the software renderer.
set -e
cd "$(dirname "$0")/.."
SAVE=${1:?usage: tools/render_readme_shots.sh <data-dir with a remembered player>}
export SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software
APP=./build/dev/agentscape
out=docs/screenshots/readme
rm -rf "$out" && mkdir -p "$out"

rm -rf build/rs && cp -r "$SAVE" build/rs
$APP --data-dir build/rs --smoke > /dev/null 2>&1   # apply any new migrations first
shot() {   # data-dir name [args]
  local dir=$1 name=$2; shift 2
  $APP --data-dir "$dir" --kiroween=on "$@" --snapshot="$out/$name.png" > /dev/null 2>&1
}

# A brand-new install: the login screen with its bats.
rm -rf build/rs-fresh && mkdir -p build/rs-fresh
shot build/rs-fresh 00-login

shot build/rs 01-world
shot build/rs 02-costumer --screen=costumer-gear
shot build/rs 03-codex --screen=codex
shot build/rs 04-codex-card --screen=codex-card
shot build/rs 05-proving-grounds --screen=proving
shot build/rs 06-quest-scroll --screen=quest-scroll-rating
shot build/rs 08-tavern --screen=tavern
shot build/rs 09-hiscores --screen=hiscores-adventurers
shot build/rs 10-hall-of-hallucinations --screen=graveyard
shot build/rs 11-wilds --screen=wilds-gryphon

# The Wilds with a session running: one with a familiar (bug fights), one solo (skilling). Dev fixture rows,
# started a few minutes ago by the wall clock, as the screen reads it.
python3 - <<'PY'
import sqlite3, datetime, shutil
started = (datetime.datetime.now(datetime.timezone.utc) - datetime.timedelta(minutes=7, seconds=13)).strftime('%Y-%m-%dT%H:%M:%SZ')
for d, agent in (('build/rs-fight', 'seed:agent:gryphon'), ('build/rs-solo', None)):
    shutil.rmtree(d, ignore_errors=True)
    shutil.copytree('build/rs', d)
    c = sqlite3.connect(d + '/agentscape.db')
    uid = c.execute("select value from app_state where key = 'remembered_user_id'").fetchone()[0]
    c.execute("insert into coding_session(id, user_id, agent_id, started_at, settled_until, ended_at) values (?, ?, ?, ?, ?, null)",
              ('01DEVSESSION' + ('F' if agent else 'S'), uid, agent, started, started))
    c.commit()
PY
shot build/rs-fight 12-wilds-session --screen=wilds
shot build/rs-solo 13-wilds-solo --screen=wilds

# The level-up comes from the end-to-end smoke test (2x window capture), run just before this script.
cp build/dev/smoke-path/p8-level-up.png "$out/07-level-up.png"
ls "$out"
