#!/usr/bin/env python3
"""Generates the Codex seed data: data/seed/agents.json, ratings.json and notes.json.

Standard library only and deterministic. Review the AGENTS and NOTES tables below; ratings are derived from
a fixed formula.

IMPORTANT - everything here is FICTIONAL SAMPLE DATA for the demo. The agents, vendors and websites are
invented (websites use the reserved .example domain), and the ratings and notes are generated from a hash. They
describe no real product. The app labels them as a sample snapshot.

    python3 tools/gen_seed_codex.py
"""
import hashlib
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'data', 'seed')
SNAPSHOT = '2026-10-01T00:00:00Z'

SKILLS = ['CODING', 'REASONING', 'WRITING', 'RESEARCH', 'CREATIVITY', 'TOOL_USE', 'CONTEXT', 'RELIABILITY', 'SPEED', 'VALUE']
QUEST_TYPES = ['CODING', 'DEBUGGING', 'WRITING', 'RESEARCH', 'DATA', 'BRAINSTORMING', 'AGENTIC', 'OTHER']
NPCS = ['mossbeard', 'quillwright', 'tokenmancer', 'lady-lambda', 'stacktrace', 'hexlint', 'brambleprompt', 'nullwyn']

# slug, name, vendor, kind, examine (<= 200 chars), about, website.
# Every agent, vendor and website here is FICTIONAL (websites use the reserved .example domain).
AGENTS = [
    ('quillback', 'Quillback', 'Hollowmere Labs', 'MODEL', 'A well-read language model that never leaves home without a spare quill.',
     'Quillback answers questions and drafts text for Hollowmere Labs customers. Made-up for the AgentScape demo.', 'https://quillback.example'),
    ('gryphon', 'Gryphon', 'Lanternfold', 'MODEL', 'A sharp-eyed language model that circles a problem before it strikes.',
     'Gryphon is the flagship model of Lanternfold. Made-up for the AgentScape demo.', 'https://gryphon.example'),
    ('twinwick', 'Twinwick', 'Ashgrove Systems', 'MODEL', 'A two-flame model that reads pictures as well as words.',
     'Twinwick is a multimodal model from Ashgrove Systems. Made-up for the AgentScape demo.', 'https://twinwick.example'),
    ('mossling', 'Mossling', 'Fen Commons', 'MODEL', 'A small, open-handed model that anyone may carry and run.',
     'Mossling is published by the Fen Commons collective for anyone to run. Made-up for the AgentScape demo.', 'https://mossling.example'),
    ('mistwren', 'Mistwren', 'Brindle Co', 'MODEL', 'A quick little model that flits between tasks.',
     'Mistwren comes in several sizes from Brindle Co. Made-up for the AgentScape demo.', 'https://mistwren.example'),
    ('deepwell', 'Deepwell', 'Underhill Research', 'MODEL', 'A patient model that likes to think a question all the way down.',
     'Deepwell is a reasoning-focused model from Underhill Research. Made-up for the AgentScape demo.', 'https://deepwell.example'),
    ('grimble', 'Grimble', 'Cinderpost', 'MODEL', 'A cheeky model with strong opinions and a short fuse.',
     'Grimble is the chatty model from Cinderpost. Made-up for the AgentScape demo.', 'https://grimble.example'),
    ('quartzen', 'Quartzen', 'Tallow Cloud', 'MODEL', 'A many-faceted model offered in a great range of sizes.',
     'Quartzen is a family of models from Tallow Cloud. Made-up for the AgentScape demo.', 'https://quartzen.example'),
    ('spellforge', 'Spellforge', 'Anvilworks', 'TOOL', 'An agentic coding editor that plans before it hammers.',
     'Spellforge turns a request into a plan and a task list, then works through it with you. Made-up for the AgentScape demo.', 'https://spellforge.example'),
    ('pocket-familiar', 'Pocket Familiar', 'Hearthbyte', 'TOOL', 'A little coding companion that sits in your editor and mutters suggestions.',
     'Pocket Familiar offers completions, chat and helper modes inside popular editors. Made-up for the AgentScape demo.', 'https://pocket-familiar.example'),
    ('cairn', 'Cairn', 'Pebblecraft', 'TOOL', 'A code editor that stacks AI help one stone at a time.',
     'Cairn is an editor with chat, completion and agent features. Made-up for the AgentScape demo.', 'https://cairn.example'),
    ('gryphon-chat', 'Gryphon Chat', 'Lanternfold', 'TOOL', 'The conversational assistant app built on Gryphon.',
     'Gryphon Chat answers questions, drafts text and analyses files. Made-up for the AgentScape demo.', 'https://gryphon-chat.example'),
    ('pathfinder', 'Pathfinder', 'Wayfarer Labs', 'TOOL', 'An answer engine that shows the trail it followed.',
     'Pathfinder searches the web and links the sources it used. Made-up for the AgentScape demo.', 'https://pathfinder.example'),
    ('sketchling', 'Sketchling', 'Pebblecraft', 'TOOL', 'A tool that sketches web interfaces from a few words.',
     'Sketchling turns text prompts into interface code you can edit. Made-up for the AgentScape demo.', 'https://sketchling.example'),
]

# Generic observations in the community's voice. They are deliberately about common traits of AI assistants
# and are attached to fictional agents by the generator.
BOONS = [
    ('CODING', 'Writes working boilerplate quickly, which saves a lot of typing on routine tasks.'),
    ('REASONING', 'Explains its reasoning step by step, which makes mistakes easier to spot.'),
    ('WRITING', 'Produces clear, readable drafts that need only light editing.'),
    ('RESEARCH', 'Good at summarising a long document into the few points that matter.'),
    ('CREATIVITY', 'Offers plenty of varied ideas when brainstorming, including some surprising ones.'),
    ('TOOL_USE', 'Uses connected tools sensibly and reports back on what it did.'),
    ('CONTEXT', 'Keeps track of details across a long conversation without being reminded.'),
    ('SPEED', 'Responds fast enough that it stays in the flow of the work.'),
    ('VALUE', 'Gives a lot of useful output for the price.'),
    ('RELIABILITY', 'Usually admits when it is unsure instead of guessing.'),
]
BANES = [
    ('RELIABILITY', 'Sometimes states things with total confidence that turn out to be wrong.'),
    ('RELIABILITY', 'Can invent a source or a function name that looks real but does not exist.'),
    ('CONTEXT', 'Loses track of earlier instructions when a conversation gets very long.'),
    ('WRITING', 'Long answers can bury the one line you actually needed.'),
    ('CODING', 'Generated code may compile but quietly miss an edge case, so it needs real tests.'),
    ('TOOL_USE', 'Occasionally calls a tool with the wrong arguments and then repeats the mistake.'),
    ('SPEED', 'Can be slow on big requests, which breaks concentration.'),
    ('VALUE', 'Heavy use adds up quickly, so costs are worth watching.'),
    ('REASONING', 'Can follow a plausible chain of logic to a wrong conclusion.'),
    ('CREATIVITY', 'Ideas can feel samey after a few rounds of brainstorming.'),
    ('RESEARCH', 'Summaries sometimes flatten important caveats from the original text.'),
    ('RELIABILITY', 'Answers to the same question can differ from one run to the next.'),
    ('CONTEXT', 'Needs to be told the same project rules again in each new session.'),
    ('CODING', 'Tends to rewrite more code than the change actually required.'),
]


def h(*parts):
    """A stable pseudo-random integer from the parts."""
    return int.from_bytes(hashlib.sha256('|'.join(map(str, parts)).encode()).digest()[:8], 'big')


def frac(*parts):
    return (h(*parts) % 10_000) / 10_000.0


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def gen_ratings():
    out = {}
    for slug, *_ in AGENTS:
        base = 3.1 + frac(slug, 'base') * 1.0                      # every agent sits in a similar band
        stats = {}
        for stat in ['OVERALL'] + SKILLS:
            skill_off = (frac(slug, stat, 'skill') - 0.5) * 0.8
            by_type = {}
            for qt in QUEST_TYPES:
                logs = 4 + h(slug, qt, 'logs') % 13                # 4..16 Quest Logs in this scope
                if stat == 'OVERALL':
                    n = logs
                else:
                    n = 2 + h(slug, stat, qt, 'n') % (logs - 1)    # a subset rate each Skill: 2..logs
                type_off = (frac(slug, qt, 'type') - 0.5) * 0.6
                mean = clamp(base + skill_off + type_off, 1.6, 4.8)
                total = 0
                for i in range(n):
                    noise = (frac(slug, stat, qt, i) - 0.5) * 2.2
                    total += int(clamp(round(mean + noise), 1, 5))
                by_type[qt] = [total, n]
            stats[stat] = by_type
        out[slug] = stats
    return out


def gen_notes():
    notes = []
    for ai, (slug, *_rest) in enumerate(AGENTS):
        for k in range(2):                                          # two Boons
            skill, text = BOONS[(ai * 3 + k * 5) % len(BOONS)]
            agree = 3 + h(slug, 'boon', k, 'a') % 28
            disagree = h(slug, 'boon', k, 'd') % 6
            notes.append((slug, 'BOON', skill, text, agree, disagree, f'{slug}-boon-{k + 1}', k))
        skill, text = BANES[ai % len(BANES)]                       # one Bane each (>= 3 votes: Graveyard-ready)
        agree = 3 + h(slug, 'bane', 0, 'a') % 20
        disagree = h(slug, 'bane', 0, 'd') % 5
        notes.append((slug, 'BANE', skill, text, agree, disagree, f'{slug}-bane-1', 2))
        if ai % 2 == 0:                                             # and a second Bane on every other agent
            skill, text = BANES[(ai + 7) % len(BANES)]
            notes.append((slug, 'BANE', skill, text, 1 + h(slug, 'bane', 1, 'a') % 6, h(slug, 'bane', 1, 'd') % 4,
                          f'{slug}-bane-2', 3))
    return notes


def main():
    os.makedirs(OUT, exist_ok=True)
    agents = {
        '_note': 'FICTIONAL agents and vendors invented for the demo. They describe no real product.',
        'snapshotAt': SNAPSHOT,
        'agents': [{'slug': s, 'name': n, 'vendor': v, 'kind': k, 'examine': e, 'about': a, 'website': w}
                   for (s, n, v, k, e, a, w) in AGENTS],
    }
    ratings = {
        '_note': 'SYNTHETIC SAMPLE DATA for FICTIONAL agents. Not real measurements or opinions. Generated by '
                 'tools/gen_seed_codex.py from a hash.',
        'snapshotAt': SNAPSHOT,
        'synthetic': True,
        'agents': gen_ratings(),
    }
    notes = {
        '_note': 'SYNTHETIC SAMPLE DATA: generic observations attached to FICTIONAL agents for the demo. '
                 'They are not claims about any real product.',
        'snapshotAt': SNAPSHOT,
        'synthetic': True,
        'notes': [{'id': f'seed:note:{key}', 'agent': slug, 'author': f'seed:npc:{NPCS[(i + idx) % len(NPCS)]}',
                   'kind': kind, 'skill': skill, 'text': text, 'agree': agree, 'disagree': disagree}
                  for idx, (slug, kind, skill, text, agree, disagree, key, i) in enumerate(gen_notes())],
    }
    for name, doc in (('agents.json', agents), ('ratings.json', ratings), ('notes.json', notes)):
        with open(os.path.join(OUT, name), 'w') as f:
            json.dump(doc, f, indent=1, sort_keys=False)
            f.write('\n')
    banes = [n for n in notes['notes'] if n['kind'] == 'BANE' and n['agree'] + n['disagree'] >= 3]
    print(f"wrote {len(agents['agents'])} agents, {len(notes['notes'])} notes ({len(banes)} Banes with >= 3 votes), ratings for "
          f"{len(ratings['agents'])} agents")


if __name__ == '__main__':
    main()
