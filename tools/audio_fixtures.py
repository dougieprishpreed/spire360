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
    for key in ('a', 'b', 'quiet'):
        nodes[key]['bus'] = 'track'
    nodes['tiny'] = wave('tiny.wav'); nodes['tiny']['sample']['frames'] = 1
    nodes['scatter'] = dict(kind='scatterer', mode='PlaySequential', count=-1, polyphony=1,
                            spawn=[0.0005, 0.0005], entries=[dict(guid='tiny', weight=1)])

    def event(ids=(), **extra):
        return dict(triggered=list(ids), boxes=[], parameter_instruments=[], end=0, **extra)

    events = {'layer': event(['quiet', 'quiet']), 'delay': event(['delay']),
              'nested': event(['nested']), 'choice': event(['choice']),
              'condition': event(['conditional'], parameters=['intensity']), 'repeat': event(['repeat'])}
    events['timeline'] = dict(triggered=[], parameter_instruments=[], end=0.003,
        boxes=[dict(id='a', at=0.001, length=0.001, locked=True)])
    events['music'] = dict(triggered=[], parameter_instruments=[], end=0.02,
        parameters=['ramp'],
        boxes=[dict(id='a', at=0, length=0.01, locked=True), dict(id='b', at=0.01, length=0.01, locked=True)],
        transitions=[dict(at=0.01, end=0.01, to=0, chance=100, conditions=[['Progress', 0, 0]])])
    events['bridge'] = dict(triggered=[], parameter_instruments=[], end=0.05,
        boxes=[dict(id='a', at=0, length=0.01, locked=True), dict(id='b', at=0.04, length=0.01, locked=True)],
        transitions=[dict(at=0.005, end=0.005, to=0.04, chance=100, length=0.002, lead_in=0, lead_out=0,
                         conditions=[], boxes=[dict(id='quiet', at=0, length=0.002, locked=True)])])
    events['fade'] = dict(triggered=[], parameter_instruments=[], end=0.05,
        boxes=[dict(id='a', at=0, length=0.01, locked=True), dict(id='b', at=0.04, length=0.01, locked=True)],
        transitions=[dict(at=0.005, end=0.005, to=0.04, chance=100, length=0.002, lead_in=0.002, lead_out=0.002,
            conditions=[], source_fades={'track': [[0, 1, 0, 0], [0.002, 0, 0, 0]]},
            destination_fades={'track': [[0, 0, 0, 0], [0.002, 1, 0, 0]]})])
    events['scatter'] = event(['scatter'])
    program = dict(parameters={'intensity': dict(minimum=0, maximum=2, defaultValue=0),
                               'ramp': dict(minimum=0, maximum=1, defaultValue=0, seekSpeed=2, seekSpeedDown=1)},
                   nodes=nodes, buses={'track': dict(parent='', volume=0, pitch=0)}, events=events)
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
        for name, value, frames in [('a.wav', 8192, 480), ('b.wav', -8192, 480), ('tiny.wav', 8192, 1)]:
            raw = struct.pack(f'<{frames}h', *([value] * frames))
            writer.add(name, s3p.TYPE_AUDIO, struct.pack('<4sI4s4sIHHIIHH4sI', b'RIFF', len(raw)+36,
                b'WAVE', b'fmt ', 16, 1, 1, 48000, 96000, 2, 16, b'data', len(raw)) + raw)


if __name__ == '__main__':
    build(sys.argv[1])
