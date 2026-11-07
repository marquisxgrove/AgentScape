#!/usr/bin/env python3
"""Generates the Codex seed data: data/seed/agents.json, ratings.json and notes.json.

Standard library only and deterministic. Review the AGENTS and NOTES tables below; ratings are derived from
a fixed formula.

IMPORTANT - the ratings and notes are SYNTHETIC SAMPLE DATA for the demo. They are not real measurements or
real opinions, they say nothing about how any product actually performs, and they are generated from a hash
so that no product is deliberately ranked above another. The app labels them as a sample snapshot. Product
names are used only to identify what is being rated (docs/ASSETS.md: no logos, no vendor imagery).

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

# slug, name, vendor, kind, examine (<= 200 chars, neutral and factual), about, official website
AGENTS = [
    ('claude', 'Claude', 'Anthropic', 'MODEL', 'A family of large language models from Anthropic.',
     'Claude models are offered through Anthropic\'s apps and API. Capabilities differ between versions.', 'https://www.anthropic.com'),
    ('gpt', 'GPT', 'OpenAI', 'MODEL', 'A family of large language models from OpenAI.',
     'GPT models are offered through OpenAI\'s products and API. Capabilities differ between versions.', 'https://openai.com'),
    ('gemini', 'Gemini', 'Google', 'MODEL', 'A family of multimodal large language models from Google.',
     'Gemini models are offered through Google\'s apps and API. Capabilities differ between versions.', 'https://gemini.google.com'),
    ('llama', 'Llama', 'Meta', 'MODEL', 'A family of openly available large language models from Meta.',
     'Llama model weights are published by Meta and can be run by anyone under the published license terms.', 'https://www.llama.com'),
    ('mistral', 'Mistral', 'Mistral AI', 'MODEL', 'A family of large language models from Mistral AI.',
     'Mistral AI publishes some models openly and offers others through its API.', 'https://mistral.ai'),
    ('deepseek', 'DeepSeek', 'DeepSeek', 'MODEL', 'A family of large language models from DeepSeek.',
     'DeepSeek publishes model weights and offers an app and an API.', 'https://www.deepseek.com'),
    ('grok', 'Grok', 'xAI', 'MODEL', 'A family of large language models from xAI.',
     'Grok models are offered through xAI\'s products and API.', 'https://x.ai'),
    ('qwen', 'Qwen', 'Alibaba Cloud', 'MODEL', 'A family of large language models from Alibaba Cloud.',
     'Qwen publishes many model sizes, several with open weights.', 'https://qwenlm.github.io'),
    ('kiro', 'Kiro', 'AWS', 'TOOL', 'An agentic coding tool and editor built around specs, steering files and hooks.',
     'Kiro turns prompts into requirements, designs and task lists, then works through them with the developer.', 'https://kiro.dev'),
    ('github-copilot', 'GitHub Copilot', 'GitHub', 'TOOL', 'An AI coding assistant for editors and the command line.',
     'GitHub Copilot offers code suggestions, chat and agent modes inside popular editors.', 'https://github.com/features/copilot'),
    ('cursor', 'Cursor', 'Anysphere', 'TOOL', 'A code editor with built-in AI assistance.',
     'Cursor is an editor with AI chat, code completion and agent features.', 'https://cursor.com'),
    ('chatgpt', 'ChatGPT', 'OpenAI', 'TOOL', 'OpenAI\'s conversational assistant app.',
     'ChatGPT is a chat app that can answer questions, write, analyse files and use connected tools.', 'https://chatgpt.com'),
    ('perplexity', 'Perplexity', 'Perplexity AI', 'TOOL', 'An AI answer engine that cites its web sources.',
     'Perplexity answers questions by searching the web and linking the sources it used.', 'https://www.perplexity.ai'),
    ('v0', 'v0', 'Vercel', 'TOOL', 'A tool that generates web interface code from prompts.',
     'v0 turns text prompts into user-interface code that can be edited and deployed.', 'https://v0.dev'),
]

# Generic observations in the community's voice. They are deliberately about common traits of AI assistants
# and are attached to products by the generator, NOT claims researched about any product.
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
        '_note': 'Neutral, factual descriptions. No logos or vendor imagery. Names identify the product only.',
        'snapshotAt': SNAPSHOT,
        'agents': [{'slug': s, 'name': n, 'vendor': v, 'kind': k, 'examine': e, 'about': a, 'website': w}
                   for (s, n, v, k, e, a, w) in AGENTS],
    }
    ratings = {
        '_note': 'SYNTHETIC SAMPLE DATA for the demo. Not real measurements or opinions. Generated by '
                 'tools/gen_seed_codex.py from a hash; no product is deliberately ranked above another.',
        'snapshotAt': SNAPSHOT,
        'synthetic': True,
        'agents': gen_ratings(),
    }
    notes = {
        '_note': 'SYNTHETIC SAMPLE DATA: generic observations about AI assistants, attached to products for the '
                 'demo. They are not researched claims about any product.',
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
