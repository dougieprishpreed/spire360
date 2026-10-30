#!/usr/bin/env python3
"""The relics whose text names a room, each checked against its own text: the
room with the relic beside the same room without it (same seed). A relic
whose room comes out the same as without it is reported: its text says the
room changes, so the driver (or the client's data) left out what it does,
as the rest site's driver did Miniature Tent's.

The rooms: the rest site (its options; after resting, what is left and the
player), the shop (its stock and prices, the player as it opens), the
treasure chest (opened: gold and relics), a fight's rewards (won: the
reward list, the card reward's cards and buttons), an elite fight (its
start: energy, hand, powers; then its rewards) for the relics whose text
names Elites, a ? room (the player as it opens). HP starts low where the
text heals. Not covered here (a SAME there is the probe's, not the
relic's): effects tied to the map (a ? map point, the floors climbed, an
act's map) or to a later combat (every other one).

Reads work/text_audit.tsv (tools/text_audit.py). Writes
work/relic_flow_probe.tsv: relic, room, verdict (changed / SAME / error),
what differed.

Usage: relic_flow_probe.py [--cmd CMD] [--data DIR] [--only ID,...]
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import content_coverage as cc  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOMS = {'rest': 'rest', 'shop': 'shop', 'treasure': 'treasure', 'rewards': 'fight', 'map': 'event'}
SEED = 'relicflow'


def rest_summary(run, case, lg, state):
    opts = [o.get('option_id') for o in state.get('options') or []]
    out = {'options': opts, 'hp': (state.get('player') or {}).get('hp')}
    heal = [o for o in state.get('options') or [] if o.get('option_id') == 'HEAL']
    if heal:
        after = run.step(case, lg, {'cmd': 'action', 'action': 'choose_option', 'args': {'option_index': heal[0]['index']}})
        p = after.get('player') or {}
        out['after'] = {'decision': after.get('decision'), 'options': [o.get('option_id') for o in after.get('options') or []],
                        'proceed': after.get('proceed'), 'hp': p.get('hp'), 'max_hp': p.get('max_hp'),
                        'potions': len(p.get('potions') or []), 'relics': len(p.get('relics') or []),
                        'deck': p.get('deck_size'), 'cards': len(after.get('cards') or [])}
    return out


def shop_summary(run, case, lg, state):
    p = state.get('player') or {}
    stock = []
    for kind in ('cards', 'relics', 'potions'):
        for e in state.get(kind) or []:
            stock.append((kind, e.get('id'), e.get('cost'), e.get('is_stocked')))
    return {'hp': p.get('hp'), 'gold': p.get('gold'), 'stock': stock,
            'removal': state.get('card_removal_cost'), 'decision': state.get('decision')}


def treasure_summary(run, case, lg, state):
    opened = run.step(case, lg, {'cmd': 'action', 'action': 'open_chest', 'args': {}})
    return {'gold': opened.get('gold'), 'relics': [r.get('id') for r in opened.get('relics') or []],
            'stage': opened.get('stage')}


def fight_summary(run, case, lg, state):
    run.step(case, lg, {'cmd': 'set_player', 'enemy_hp': 1})
    for _ in range(6):
        if state.get('decision') != 'combat_play':
            break
        state = run.step(case, lg, {'cmd': 'add_to_hand', 'cards': ['STRIKE_IRONCLAD'], 'energy': 9})
        mine = [h for h in state.get('hand') or [] if (h.get('id') or '').endswith('STRIKE_IRONCLAD')]
        alive = [e for e in state.get('enemies') or [] if e.get('hp', 0) > 0]
        if not mine or not alive:
            break
        state = run.step(case, lg, {'cmd': 'action', 'action': 'play_card',
                                    'args': {'card_index': mine[-1]['index'], 'target_index': alive[0].get('index', 0)}})
    rewards = [(r.get('type'), r.get('label')) for r in state.get('rewards') or []]
    deck = run.step(case, lg, {'cmd': 'view_pile', 'pile': 'deck'})
    out = {'decision': state.get('decision'), 'rewards': rewards,
           'deck_upgrades': sum(1 for c in deck.get('cards') or [] if (c.get('upgrade_level') or 0) > 0)}
    cards = [r for r in state.get('rewards') or [] if r.get('type') == 'card']
    if cards:
        picked = run.step(case, lg, {'cmd': 'action', 'action': 'take_reward', 'args': {'reward_index': cards[0]['index']}})
        out['card_reward'] = [(c.get('id'), c.get('upgrade_level'), json.dumps(c.get('enchantment_view'))[:40], c.get('pool'), c.get('type'))
                              for c in picked.get('cards') or []]
        out['alternatives'] = [a.get('id') or a.get('type') for a in picked.get('alternatives') or []]
        out['can_skip'] = picked.get('can_skip')
    return out


def settle(run, case, lg):
    """A card reward or selection left open (a reward looked at, Dream
    Catcher's card after resting) closed, so the next run can start."""
    for action in ('skip_card_reward', 'skip_select'):
        run.step(case, lg, {'cmd': 'action', 'action': action, 'args': {}})


def elite_summary(run, case, lg, state):
    """An elite fight's start (the player's energy, hand and powers, the
    enemies' powers), then its win's rewards"""
    p = state.get('player') or {}
    start = {'energy': state.get('energy') if state.get('energy') is not None else p.get('energy'),
             'hand': len(state.get('hand') or []),
             'player_powers': sorted((q.get('id'), q.get('amount')) for q in state.get('player_powers') or []),
             'enemy_powers': sorted((q.get('id'), q.get('amount')) for e in state.get('enemies') or []
                                    for q in e.get('powers') or [])}
    out = fight_summary(run, case, lg, state)
    out['start'] = start
    return out


def event_summary(run, case, lg, state):
    p = state.get('player') or {}
    return {'hp': p.get('hp'), 'decision': state.get('decision')}


SUMMARY = {'rest': rest_summary, 'shop': shop_summary, 'treasure': treasure_summary, 'fight': fight_summary,
           'event': event_summary, 'elite': elite_summary}
ENTER = {'rest': {'type': 'rest_site'}, 'shop': {'type': 'shop'}, 'treasure': {'type': 'treasure'},
         'fight': {'type': 'monster', 'encounter': 'SHRINKER_BEETLE_WEAK'},
         'event': {'type': 'event', 'event': 'AROMA_OF_CHAOS'},
         'elite': {'type': 'elite', 'encounter': 'BYGONE_EFFIGY_ELITE'}}


def probe(run, lg, relic, room):
    case = cc.Case('relic_flow', f'{relic or "none"}-{room}')
    run.new_run(case, lg, character='Ironclad', seed=SEED)
    # low HP: a heal shows; gold for the shop
    run.step(case, lg, {'cmd': 'set_player', 'hp': 20, 'gold': 500})
    if relic:
        state = run.step(case, lg, {'cmd': 'obtain', 'relics': [relic]})
        if state.get('type') == 'error':
            return None, state.get('message')
        held = run.step(case, lg, {'cmd': 'get_state'})
        ids = [(r.get('id') or '').split('.')[-1] for r in (held.get('player') or {}).get('relics') or []]
        if relic not in ids:
            # text in the tables, no model in this version (Glowing Orb)
            return None, 'not obtained: no such relic in this version'
    state = run.step(case, lg, {'cmd': 'enter_room', **ENTER[room]})
    if state.get('type') == 'error':
        return None, state.get('message')
    got = SUMMARY[room](run, case, lg, state)
    settle(run, case, lg)
    return got, None


def diff(a, b, path=''):
    if isinstance(a, dict) and isinstance(b, dict):
        out = []
        for k in sorted(set(a) | set(b)):
            out += diff(a.get(k), b.get(k), f'{path}.{k}' if path else k)
        return out
    return [] if a == b else [f'{path}: {json.dumps(a)[:80]} -> {json.dumps(b)[:80]}']


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cmd', default='dotnet out/logic/jit/Spire.Logic.dll')
    ap.add_argument('--data', default='work/play-data')
    ap.add_argument('--audit', default=os.path.join(ROOT, 'work', 'text_audit.tsv'))
    ap.add_argument('--out', default=os.path.join(ROOT, 'work', 'relic_flow_probe.tsv'))
    ap.add_argument('--work', default=os.path.join(ROOT, 'work', 'relic_flow'))
    ap.add_argument('--only', default='')
    a = ap.parse_args()
    only = set(filter(None, a.only.split(',')))
    todo = []
    with open(a.audit, encoding='utf-8') as fh:
        cols = fh.readline().rstrip('\n').split('\t')
        for line in fh:
            r = dict(zip(cols, line.rstrip('\n').split('\t')))
            if r['kind'] != 'relic' or (only and r['id'] not in only):
                continue
            rooms = []
            for f in r['flows'].split(','):
                if f in ROOMS and ROOMS[f] not in rooms:
                    rooms.append(ROOMS[f])
            # what the text says of Elites shows in an elite fight, not a ? room
            if 'elite' in r['text'].lower():
                rooms = [x for x in rooms if x != 'event'] + ['elite']
            todo += [(r['id'], room) for room in rooms]
    run = cc.Runner(argparse.Namespace(cmd=a.cmd, data=a.data, work=a.work, limit=0, fight='BYGONE_EFFIGY_ELITE',
                                       seed=1, act_runs=0, suite=''))
    lg = run.logic('relic_flow', 'probe')
    base, rows, same = {}, [], 0
    for relic, room in todo:
        if room not in base:
            base[room], err = probe(run, lg, None, room)
            if err:
                print(f'baseline {room}: {err}', flush=True)
        got, err = probe(run, lg, relic, room)
        if err or got is None or base.get(room) is None:
            verdict, what = 'error', err or 'no baseline'
        else:
            d = diff(base[room], got)
            verdict, what = ('changed', '; '.join(d)) if d else ('SAME', 'the room is as without the relic')
        same += verdict != 'changed'
        rows.append((relic, room, verdict, what))
        print(f'{relic:28} {room:9} {verdict:8} {what[:150]}', flush=True)
        if lg.dead:
            lg = run.logic('relic_flow', 'probe')
    lg.close()
    with open(a.out, 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('relic\troom\tverdict\twhat\n')
        for r in rows:
            fh.write('\t'.join(r) + '\n')
    print(f'{len(rows)} relic rooms, {same} the same as without the relic or failed; wrote {a.out}')
    return 1 if same else 0


if __name__ == '__main__':
    sys.exit(main())
