#!/usr/bin/env python3
"""Original, synthetic audio fixtures for the portable event verifier."""
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'converter'))
from spire_convert import s3p


def build(path):
    def wave(name, **extra):
        return dict(kind='waveform', sample=dict(path=name, frames=480, rate=48000,
                    loop_start=0, loop_end=0), chance=100, **extra)

    nodes = {'a': wave('a.wav'), 'b': wave('b.wav'), 'quiet': wave('a.wav', volume=-6.020599913),
             'delay': wave('a.wav', delay=[0.001, 0.001]), 'loop': wave('a.wav', loops=-1),
             'choice': dict(kind='multi', chance=100, mode='Random',
                            entries=[dict(guid='a', weight=0), dict(guid='b', weight=100)]),
             'nested': dict(kind='event', chance=100, event='layer', params={}),
             'conditional': wave('a.wav', conditions=[['intensity', 2, 2]]),
             'repeat': dict(kind='multi', chance=100, mode='PlaySequential', loops=1,
                            entries=[dict(guid='a', weight=1), dict(guid='b', weight=1)])}

    def event(ids=(), **extra):
        return dict(triggered=list(ids), boxes=[], parameter_instruments=[], end=0, **extra)

    events = {'layer': event(['quiet', 'quiet']), 'delay': event(['delay']),
              'nested': event(['nested']), 'choice': event(['choice']),
              'condition': event(['conditional']), 'repeat': event(['repeat'])}
    events['timeline'] = dict(triggered=[], parameter_instruments=[], end=0.003,
        boxes=[dict(id='a', at=0.001, length=0.001, locked=True)])
    events['music'] = dict(triggered=[], parameter_instruments=[], end=0.02,
        boxes=[dict(id='a', at=0, length=0.01, locked=True), dict(id='b', at=0.01, length=0.01, locked=True)],
        transitions=[dict(at=0.01, end=0.01, to=0, chance=100, conditions=[['Progress', 0, 0]])])
    events['bridge'] = dict(triggered=[], parameter_instruments=[], end=0.05,
        boxes=[dict(id='a', at=0, length=0.01, locked=True), dict(id='b', at=0.04, length=0.01, locked=True)],
        transitions=[dict(at=0.005, end=0.005, to=0.04, chance=100, length=0.002, lead_in=0, lead_out=0,
                         conditions=[], boxes=[dict(id='quiet', at=0, length=0.002, locked=True)])])
    program = dict(parameters={'intensity': dict(minimum=0, maximum=2, defaultValue=0)},
                   nodes=nodes, buses={}, events=events)
    index = {f'event:/test/{key}': [['Master.bank', key]] for key in events}
    index['event:/test/bank'] = [['Master.bank', 'bank'], ['other.bank', 'bank']]
    program['events']['bank'] = event(['a'])
    other = dict(parameters={}, nodes=nodes, buses={}, events={'bank': event(['b'])})
    with s3p.Writer(path) as writer:
        def blob(name, obj):
            writer.add(name, s3p.TYPE_BLOB, json.dumps(obj).encode())
        blob('audio/runtime.json', dict(version=1, events=index, paths={}))
        blob('audio/runtime/Master.bank.json', program)
        blob('audio/runtime/other.bank.json', other)
        for name, value in [('a.wav', 8192), ('b.wav', -8192)]:
            raw = struct.pack('<480h', *([value] * 480))
            writer.add(name, s3p.TYPE_AUDIO, struct.pack('<4sI4s4sIHHIIHH4sI', b'RIFF', len(raw)+36,
                b'WAVE', b'fmt ', 16, 1, 1, 48000, 96000, 2, 16, b'data', len(raw)) + raw)


if __name__ == '__main__':
    build(sys.argv[1])
