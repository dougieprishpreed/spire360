#!/usr/bin/env python3
"""What each card, relic, enchantment and potion says it does, sorted into
what a test has to look at.

The game's own code runs every effect, so a card's numbers are right by
construction; what breaks is where this port stands in for the game: the
driver (rest sites, shops, rewards, treasure, events, the card screens)
and the presentation (cues, positions, sounds, intents). Miniature Tent's
text ("rest site", "options") named the flow the driver got wrong, and a
check from the text alone would have found it. This reads the English
tables, tags each entry by the flows its text names (rest, shop,
rewards, treasure, map, event, deck changes, potions, gold, max HP,
pickup) and by what it interacts with (powers, keywords, card types,
piles, orbs, Osty and summons, stars, energy, block, timing), and writes
work/text_audit.tsv (the game's text: never committed). tools/relic_flow_probe.py then checks the relics
whose text names a room: the room with the relic against the room
without it.

Usage: text_audit.py [--data data/localization/eng] [--out work/text_audit.tsv]
"""
import argparse
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

FLOWS = [
    ('rest', r'\brest site|\brest\b|\bsmith|campfire'),
    ('shop', r'merchant|\bshop|purchase|\bprice|\bbuy'),
    ('rewards', r'card reward|\breward|after combat|combat rewards'),
    ('treasure', r'\bchest|treasure'),
    ('map', r'\bmap\b|\? ?room|unknown room|\belite\b|\bpath\b|\bfloor'),
    ('event', r'\bevent'),
    ('deck_upgrade', r'\bupgrade'),
    ('deck_transform', r'\btransform'),
    ('deck_remove', r'\bremove'),
    ('deck_enchant', r'\benchant'),
    ('deck_add', r'add .* (to|into) your deck|obtain .*card|to your deck'),
    ('potion', r'\bpotion'),
    ('gold', r'\bgold\b'),
    ('max_hp', r'max hp'),
    ('heal', r'\bheal'),
    ('pickup', r'upon pickup|when picked up|on pickup'),
    ('boss', r'\bboss'),
]

TIMING = [
    ('combat_start', r'start of (each|every) combat|at the start of combat'),
    ('turn_start', r'start of (your|each) turn'),
    ('turn_end', r'end of (your|each) turn'),
    ('whenever', r'\bwhenever|\bevery time|each time'),
    ('first', r'\bfirst (time|card|attack|skill|power)'),
    ('combat_end', r'end of combat|after combat'),
    ('death', r'\bdies|\bkill|\bfatal'),
]

THINGS = [
    ('attack', r'\battacks?\b'), ('skill', r'\bskills?\b'), ('power_card', r'\bpower cards?\b|\bpowers?\b'),
    ('status', r'\bstatus\b'), ('curse', r'\bcurses?\b'),
    ('draw_pile', r'draw pile'), ('discard_pile', r'discard pile'), ('exhaust_pile', r'exhaust pile'),
    ('hand', r'\bhand\b'), ('orb', r'\borbs?\b|channel|evoke'), ('osty', r'\bosty\b|summon'),
    ('star', r'\bstars?\b|forge'), ('energy', r'\benergy\b'), ('block', r'\bblock\b'), ('damage', r'\bdamage\b'),
    ('x_cost', r'\bx\b'), ('random', r'\brandom'), ('copy', r'\bcop(y|ies)\b|duplicate|play .* twice|played twice'),
]


def clean(text):
    text = re.sub(r'\[/?[a-z_]+(=[^\]]*)?\]', '', text)
    return re.sub(r'\s+', ' ', text).strip()


def tags(text, table):
    low = text.lower()
    return [name for name, pattern in table if re.search(pattern, low)]


def load(data, name):
    path = os.path.join(data, name)
    with open(path, encoding='utf-8') as fh:
        return json.load(fh)


def entries(table, kind):
    out = {}
    for key, value in table.items():
        if '.' not in key:
            continue
        ident, field = key.split('.', 1)
        e = out.setdefault(ident, {'kind': kind, 'id': ident})
        if field in ('title', 'description', 'upgradeDescription', 'eventDescription'):
            e[field] = clean(value)
    return [e for e in out.values() if e.get('description')]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--data', default=os.path.join(ROOT, 'data', 'localization', 'eng'))
    ap.add_argument('--out', default=os.path.join(ROOT, 'work', 'text_audit.tsv'))
    a = ap.parse_args()
    keywords = [clean(v) for k, v in load(a.data, 'card_keywords.json').items() if k.endswith('.title')]
    powers = [clean(v) for k, v in load(a.data, 'powers.json').items() if k.endswith('.title')]
    rows = []
    for name, kind in (('cards.json', 'card'), ('relics.json', 'relic'), ('enchantments.json', 'enchantment'),
                       ('afflictions.json', 'affliction'), ('potions.json', 'potion')):
        for e in entries(load(a.data, name), kind):
            text = ' '.join(e.get(f, '') for f in ('description', 'eventDescription'))
            low = text.lower()
            kws = [k for k in keywords if k and re.search(r'\b' + re.escape(k.lower()) + r'\b', low)]
            pws = [p for p in powers if p and len(p) > 3 and re.search(r'\b' + re.escape(p.lower()) + r'\b', low)]
            flows = tags(text, FLOWS)
            timing = tags(text, TIMING)
            things = tags(text, THINGS)
            # Second-order: what it touches beyond its own number (another
            # card, a power, a pile, a timing other effects also use)
            reach = len(set(kws) | set(pws)) + len(things) + len(timing)
            rows.append({'kind': kind, 'id': e['id'], 'title': e.get('title', ''), 'flows': ','.join(flows),
                         'timing': ','.join(timing), 'things': ','.join(things), 'keywords': ','.join(sorted(set(kws))),
                         'powers': ','.join(sorted(set(pws))), 'reach': reach, 'text': text})
    rows.sort(key=lambda r: (r['kind'], -r['reach'], r['id']))
    cols = ['kind', 'id', 'title', 'flows', 'timing', 'things', 'keywords', 'powers', 'reach', 'text']
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    with open(a.out, 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('\t'.join(cols) + '\n')
        for r in rows:
            fh.write('\t'.join(str(r[c]).replace('\t', ' ') for c in cols) + '\n')
    by_kind = {}
    for r in rows:
        k = by_kind.setdefault(r['kind'], {'all': 0, 'flows': 0, 'second_order': 0})
        k['all'] += 1
        k['flows'] += bool(r['flows'])
        k['second_order'] += r['reach'] >= 2
    for kind, k in sorted(by_kind.items()):
        print(f"{kind}: {k['all']}, naming an out-of-fight flow {k['flows']}, second-order (2+ things it touches) {k['second_order']}")
    flow_counts = {}
    for r in rows:
        for f in filter(None, r['flows'].split(',')):
            flow_counts.setdefault((r['kind'], f), 0)
            flow_counts[(r['kind'], f)] += 1
    for (kind, f), n in sorted(flow_counts.items()):
        print(f'  {kind} {f}: {n}')
    print('wrote', a.out)
    return 0


if __name__ == '__main__':
    sys.exit(main())
