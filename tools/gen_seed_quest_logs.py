#!/usr/bin/env python3
"""Generates data/seed/quest_logs.json: sample Quest Logs written by the fictional townsfolk (the seed NPCs).

Standard library only and deterministic.

IMPORTANT - these are FICTIONAL SAMPLE DATA about FICTIONAL agents, so the Tavern board is alive on first run. They
are DISPLAY-ONLY: their ratings are already inside the sample rating baseline (data/seed/ratings.json), so the seed
stores them as `seeded` and `excluded`, and a Keeper cannot count them in again. They carry no XP.

    python3 tools/gen_seed_quest_logs.py
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'data', 'seed', 'quest_logs.json')
SNAPSHOT = '2026-10-01T00:00:00Z'

# (npc slug, agent slug, version, quest type, overall, skills, title, body, recommend, pros, cons, created, helpful)
LOGS = [
    ('mossbeard', 'gryphon', 'v3', 'CODING', 5, {'CODING': 5, 'RELIABILITY': 4, 'SPEED': 3},
     'Untangled my parser in one sitting',
     'I fed it a 400-line parser that had grown a bug nobody could find. It asked two sharp questions, then pointed at the '
     'off-by-one on the first read.\nThe fix came with a test. It was a little slow on the long file, but worth the wait.',
     True, ['Accurate', 'Clean code'], ['Slow'], '2026-09-28T16:20:00Z', 14),
    ('quillwright', 'quillback', '', 'WRITING', 4, {'WRITING': 5, 'CREATIVITY': 4},
     'A patient editor for long drafts',
     'Gave it the second chapter of my guide to the Codex and asked it to tighten the prose without changing my voice. '
     'It kept the jokes and cut the padding.\nI did have to tell it twice not to add a moral at the end.',
     True, ['Clear explanations', 'Creative'], ['Too verbose'], '2026-09-27T09:05:00Z', 9),
    ('tokenmancer', 'spellforge', '', 'AGENTIC', 4, {'TOOL_USE': 5, 'CONTEXT': 4, 'VALUE': 3},
     'Plans first, then hammers',
     'It wrote a plan, a task list and only then started editing. Following along felt like pair programming with someone '
     'who actually reads the ticket.\nIt lost the thread once after about forty steps, but picked it up when I pasted the plan back.',
     True, ['Followed instructions', 'Good with large context'], ['Lost context'], '2026-09-26T19:40:00Z', 11),
    ('lady-lambda', 'deepwell', '', 'RESEARCH', 5, {'REASONING': 5, 'RESEARCH': 5, 'SPEED': 2},
     'Thinks a question all the way down',
     'I asked why two of my measurements disagreed. It took its time, laid out three explanations and told me which '
     'experiment would tell them apart.\nNot quick, and not cheap, but it saved me a week of guessing.',
     True, ['Accurate', 'Clear explanations'], ['Slow', 'Expensive'], '2026-09-25T11:15:00Z', 12),
    ('stacktrace', 'grimble', '', 'DEBUGGING', 2, {'RELIABILITY': 2, 'CODING': 3},
     'Very sure of itself, and wrong',
     'It blamed my cache for a crash that was really a missing null check, and defended its answer twice when I pushed back.\n'
     'Once I pasted the stack trace it came round, but I would not trust it without one.',
     False, ['Fast'], ['Hallucinated', 'Ignored instructions'], '2026-09-24T21:00:00Z', 6),
    ('hexlint', 'pathfinder', '', 'RESEARCH', 4, {'RESEARCH': 4, 'RELIABILITY': 4},
     'Shows the trail it followed',
     'Every claim came with a source I could open, and two of them corrected my assumptions.\nOne link was dead, so I '
     'still check the ones that matter.',
     True, ['Accurate', 'Clear explanations'], [], '2026-09-23T08:30:00Z', 8),
    ('brambleprompt', 'mossling', '', 'DATA', 3, {'VALUE': 5, 'CONTEXT': 2},
     'Small, free and good enough for chores',
     'I run it on my own machine to tidy spreadsheets. It handles the dull jobs well and costs me nothing.\nIt forgets '
     'the start of a long table, so I feed it in pieces.',
     True, ['Great value'], ['Lost context'], '2026-09-22T13:45:00Z', 5),
    ('nullwyn', 'sketchling', '', 'BRAINSTORMING', 4, {'CREATIVITY': 5, 'SPEED': 4},
     'Fifty ideas before the tea went cold',
     'I needed names and layouts for a tiny shop page. It sketched a dozen directions in a minute and two of them were '
     'better than what I had.\nThe code it produced needed a tidy-up, but as a spark it was great.',
     True, ['Creative', 'Fast'], [], '2026-09-21T17:10:00Z', 7),
    ('mossbeard', 'cairn', '', 'CODING', 3, {'CODING': 4, 'TOOL_USE': 3},
     'Handy stack of helpers, uneven results',
     'The completion and chat in the editor are smooth and I like how it stacks help one piece at a time.\nSome '
     'suggestions were wonderful and some were confidently off, so I read everything.',
     True, ['Fast'], ['Hallucinated'], '2026-09-20T10:25:00Z', 4),
    ('quillwright', 'twinwick', '', 'OTHER', 4, {'CONTEXT': 4, 'WRITING': 3},
     'Reads a picture as well as a page',
     'I showed it a photo of a whiteboard and asked for the notes. It got the boxes and arrows right and even kept my '
     'terrible handwriting straight.\nIt shortened one item too much, but that is easy to fix.',
     True, ['Accurate'], [], '2026-09-19T15:50:00Z', 3),
    ('tokenmancer', 'mistwren', '', 'WRITING', 3, {'SPEED': 5, 'WRITING': 3},
     'Lightning quick, a bit thin',
     'Replies arrive before I finish my sentence, which is lovely for quick emails.\nThe wording is plain and I always '
     'add my own flourish, but for first drafts it is hard to beat.',
     True, ['Fast', 'Great value'], [], '2026-09-18T12:00:00Z', 2),
    ('stacktrace', 'pocket-familiar', '', 'CODING', 4, {'CODING': 4, 'SPEED': 5},
     'Mutters useful suggestions',
     'It sits in the editor and finishes my lines. Most of the time it reads my mind; some of the time it invents a '
     'function that does not exist.\nI keep it on and I keep my tests on, too.',
     True, ['Fast', 'Clean code'], ['Hallucinated'], '2026-09-17T09:20:00Z', 10),
]


def main():
    doc = {
        '_note': 'FICTIONAL SAMPLE DATA: Quest Logs by fictional townsfolk about fictional agents. DISPLAY-ONLY: their ratings are '
                 'already in the sample baseline (ratings.json), so they are stored seeded and excluded and never counted twice. '
                 'Generated by tools/gen_seed_quest_logs.py.',
        'snapshotAt': SNAPSHOT,
        'questLogs': [
            {
                'id': f'seed:quest:{npc}-{agent}',
                'author': f'seed:npc:{npc}',
                'agent': agent,
                'version': version,
                'questType': quest_type,
                'overall': overall,
                'skills': skills,
                'title': title,
                'body': body,
                'recommend': recommend,
                'pros': pros,
                'cons': cons,
                'createdAt': created,
                'helpful': helpful,
            }
            for (npc, agent, version, quest_type, overall, skills, title, body, recommend, pros, cons, created, helpful) in LOGS
        ],
    }
    with open(OUT, 'w') as f:
        json.dump(doc, f, indent=1)
        f.write('\n')
    print(f"wrote {len(LOGS)} sample Quest Logs")


if __name__ == '__main__':
    main()
