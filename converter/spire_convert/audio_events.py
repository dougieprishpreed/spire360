"""Compile the offline Studio graph into small, lazily loaded bank programs.

The parser exposes controller owners as a flat list. CTRO follows its
timeline/parameter-layout body in the RIFF; retain that association here.
Timeline coordinates are 48 kHz ticks (including the bit-cast curve X).
"""
import json
import struct
import uuid

ZERO = '00000000-0000-0000-0000-000000000000'

# FModBankParser 1.2.0 names these with the newer enum, which removed three
# plugin slots and ENVELOPEFOLLOWER. v142 banks store the earlier numeric IDs.
PARSER_DSP_NAMES = ('UNKNOWN MIXER OSCILLATOR LOWPASS ITLOWPASS HIGHPASS ECHO '
    'FADER FLANGE DISTORTION NORMALIZE LIMITER PARAMEQ PITCHSHIFT CHORUS '
    'ITECHO COMPRESSOR SFXREVERB LOWPASS_SIMPLE DELAY TREMOLO SEND RETURN '
    'HIGHPASS_SIMPLE PAN THREE_EQ FFT LOUDNESS_METER CONVOLUTIONREVERB '
    'CHANNELMIX TRANSCEIVER OBJECTPAN MULTIBAND_EQ MULTIBAND_DYNAMICS').split()
BANK_DSP_NAMES = PARSER_DSP_NAMES[:15] + ['VSTPLUGIN', 'WINAMPPLUGIN'] + \
    PARSER_DSP_NAMES[15:21] + ['LADSPAPLUGIN'] + PARSER_DSP_NAMES[21:28] + \
    ['ENVELOPEFOLLOWER'] + PARSER_DSP_NAMES[28:]


def effect_type(value, version):
    if version != 142:
        raise ValueError(f'unverified Studio DSP enum for bank version {version}')
    number = PARSER_DSP_NAMES.index(value.removeprefix('FMOD_DSP_TYPE_')) if isinstance(value, str) else value
    names = BANK_DSP_NAMES
    if not 0 <= number < len(names):
        raise ValueError(f'unsupported Studio DSP type {number}, bank version {version}')
    return names[number]


def effect_parameter(p):
    if p['type'] == 1:
        # The parser reads the union's integer member as float. Preserve bits,
        # including tiny denormals such as an enum value of one.
        return struct.unpack('<i', struct.pack('<f', p['floatValue']))[0]
    if p['type'] == 2:
        return bool(p['floatValue'])
    return p['buffer'] if p['type'] == 3 else p['floatValue']


def controller_sources(raw, chunks, start=12, end=None):
    result = {}
    owner = None
    for tag, start, end in chunks(raw, start, end):
        if tag == b'LIST':
            result.update(controller_sources(raw, chunks, start + 4, end))
        if tag in (b'TLNB', b'PMLB'):
            owner = str(uuid.UUID(bytes_le=raw[start:start + 16]))
        elif tag == b'CTRO' and owner:
            count, = struct.unpack_from('<H', raw, start)
            pos = start + 2
            if count & 0x8000:
                high, = struct.unpack_from('<H', raw, pos)
                count = (count & 0x7fff) | (high << 15)
                pos += 2
            count >>= 1
            if count:
                pos += 2  # fixed element size
            if pos + count * 16 > end:
                raise ValueError('truncated Studio controller owner')
            for i in range(count):
                result[str(uuid.UUID(bytes_le=raw[pos + i * 16:pos + (i + 1) * 16]))] = owner
    return result


def compile_bank(bank, sources):
    g = bank['graph']
    data = lambda group: {k: v['data'] for k, v in g[group].items()}
    params, layouts = data('parameterNodes'), data('parameterLayoutNodes')
    timelines, curves = data('timelineNodes'), data('curveNodes')
    names = {k: p['name'] for k, p in params.items()}
    program = {'parameters': {p['name']: {k: p[k] for k in
                ('minimum', 'maximum', 'defaultValue', 'seekSpeed', 'seekSpeedDown', 'labels')}
                for p in params.values()}, 'nodes': {}, 'buses': {}, 'events': {}}

    def conditions(evaluators):
        # The bank's evaluator stream gives [min, max] ranges (the engine
        # excludes max for a discrete parameter: a label k is k..k+1);
        # Basic2/Type30 join two alternatives (the treasure/unknown Progress
        # branch).
        result, parameter = [], None
        for e in evaluators:
            if e['type'] == 'Type11':
                parameter = names[e['data']]
            elif e['type'] == 'Type20':
                result.append([parameter, *e['data']])
            elif e['type'] not in ('Basic2', 'Type30'):
                raise ValueError('unsupported Studio condition ' + e['type'])
        return result

    def controls(owner, allowed=(0, 1)):
        out = []
        for key, c in data('controllerNodes').items():
            if c['propertyOwnerGuid'] != owner or allowed is not None and c['propertyIndex'] not in allowed:
                continue
            source = sources.get(key)
            if source not in timelines and source not in layouts:
                continue  # transition crossfade curves are on the transition itself
            param = names[layouts[source]['parameterGuid']] if source in layouts else ''
            points = []
            for p in curves[c['curveGuid']]['curvePoints']:
                x = p['x'] if param else struct.unpack('<I', struct.pack('<f', p['x']))[0] / 48000
                points.append([x, p['y'], p['shape'], p['type']])
            control = {'property': c['propertyIndex'], 'parameter': param, 'points': points}
            if allowed is None:
                for prop in data('propertyNodes').values():
                    if key in prop['controllers']:
                        mapping = data('mappingNodes').get(prop['mappingGuid'], {})
                        control.update(method=prop['method'], mapping=[[p['x'], p['y']]
                                       for p in mapping.get('mappingPoints', [])])
            out.append(control)
        return out

    def properties(owner, allowed=(0, 1)):
        result = {'controls': controls(owner, allowed), 'mods': []}
        for m in data('modulatorNodes').values():
            if m['ownerGuid'] == owner:
                result['mods'].append({'kind': m['type'], 'property': m['propertyIndex'], **m['subnode']})
        return {k: v for k, v in result.items() if v}

    def fades(entries):
        return {f['busGuid']: [[struct.unpack('<I', struct.pack('<f', p['x']))[0] / 48000,
                              p['y'], p['shape'], p['type']]
                             for p in curves[f['curveGuid']]['curvePoints']] for f in entries}

    for key, wrapper in g['instrumentNodes'].items():
        n, kind = wrapper['data'], wrapper['type'].replace('InstrumentNode', '').lower()
        b = n['instrumentBody']
        node = {'kind': kind, 'volume': b['volume'], 'pitch': b['pitch'], 'bus': b['routable']['baseGuid'],
                'loops': b['loopCount'], 'flags': b['flags'], 'chance': b['triggerChancePercent'],
                'delay': [b['triggerDelay']['min'] / 1000, b['triggerDelay']['max'] / 1000],
                'offset': b['leftTrimOffset'] / 48000, 'seek': b['initialSeekPosition'] / 48000,
                'seek_percent': b['initialSeekPercent'], 'conditions': conditions(b['evaluators']),
                **properties(key)}
        if kind == 'waveform':
            sample = bank['samples'][n['waveformResourceGuid']]
            node['sample'] = {k: sample[k] for k in ('path', 'frames', 'rate', 'loop_start', 'loop_end')}
        elif kind in ('multi', 'scatterer'):
            p = n['playlistBody']
            node.update(mode=p['playMode'].split('_')[-1], entries=p['entries'])
            if kind == 'scatterer':
                node.update(spawn=[n['spawnTime']['minimum'], n['spawnTime']['maximum']],
                            polyphony=n['maximumSpawnPolyphony'], count=-1 if n['spawnCount'] == 33 else n['spawnCount'])
        elif kind == 'event':
            node['event'] = n['eventGuid']
            node['params'] = {names[p['parameterGuid']]: p['initialValue'] for p in n['eventParameterStubs']
                              if p['initialValue'] < 1e30}
        elif kind == 'silence':
            node['duration'] = n['duration'] / 1000
        elif kind == 'command':
            node.update(command=n['commandType'], target=names.get(n['targetGuid'], n['targetGuid']), value=n['value'])
        else:
            raise ValueError('unsupported Studio instrument ' + kind)
        defaults = dict(volume=0, pitch=0, bus=ZERO, loops=0, flags=0, chance=100,
                        delay=[0, 0], offset=0, seek=0, seek_percent=0, conditions=[])
        program['nodes'][key] = {k: v for k, v in node.items() if k not in defaults or v != defaults[k]}
    for key, b in data('busNodes').items():
        strip = b['busBody']['mixerStrip']
        program['buses'][key] = {'parent': b['routable']['baseGuid'], 'volume': strip['volume'], 'pitch': strip['pitch'], **properties(key)}
        for field, source in (('pre', 'preFaderEffects'), ('post', 'postFaderEffects')):
            if b['busBody'][source]:
                program['buses'][key][field] = b['busBody'][source]
    program['effects'] = {}
    for key, wrapper in g['effectNodes'].items():
        n = wrapper['data']
        kind = wrapper['type'].removesuffix('EffectNode')
        effect = {'kind': effect_type(n['dspType'], bank['version']) if kind == 'BuiltIn' else kind,
                  **properties(key, None)}
        if kind == 'BuiltIn':
            effect['params'] = [effect_parameter(p) for p in n['paramEffectBody']['parameters']]
        elif kind == 'Send':
            effect.update(target=n['returnGuid'], level=n['sendLevel'])
        if n.get('effectBody'):
            effect['mix'] = n['effectBody']
        program['effects'][key] = {k: v for k, v in effect.items() if v != []}
    transitions = data('transitionNodes')
    for key, e in data('eventNodes').items():
        t = timelines[e['timelineGuid']]
        markers = {m['baseGuid']: m for m in t['timelineNamedMarkers']}
        boxes = [{'id': b['guid'], 'at': b['startTime'] / 48000, 'length': b['length'] / 48000,
                  'locked': locked} for locked, field in ((False, 'triggerBoxes'), (True, 'timeLockedTriggerBoxes'))
                 for b in t[field]]
        trans = []
        for tr in transitions.values():
            if tr['destinationGuid'] not in markers:
                continue
            body = tr['transitionBody'] or {}
            trans.append({'at': tr['start'] / 48000, 'end': tr['end'] / 48000,
                          'to': markers[tr['destinationGuid']]['position'] / 48000,
                          'flags': tr['flags'], 'conditions': conditions(tr['evaluators']),
                          'chance': tr['transitionChancePercent'], 'quantization': tr['quantization'],
                          'fade': max(body.get('leadInLength', 0), body.get('leadOutLength', 0)) / 48000,
                          'lead_in': body.get('leadInLength', 0) / 48000,
                          'lead_out': body.get('leadOutLength', 0) / 48000,
                          'source_fades': fades(body.get('leadInCurves', [])),
                          'destination_fades': fades(body.get('leadOutCurves', [])),
                          'length': body.get('length', 0) / 48000,
                          'boxes': [{'id': b['guid'], 'at': b['startTime'] / 48000,
                                     'length': b['length'] / 48000, 'locked': True}
                                    for b in body.get('timeLockedTriggerBoxes', []) + body.get('triggeredTriggerBoxes', [])]})
        program['events'][key] = {'bus': e['inputBusGuid'], 'master': e['masterTrackGuid'],
            'triggered': e['eventTriggeredInstruments'], 'boxes': boxes, 'transitions': trans,
            'parameters': [names[layouts[p]['parameterGuid']] for p in e['parameterLayouts']],
            'parameter_instruments': list(dict.fromkeys(i for p in e['parameterLayouts'] for i in layouts[p]['instruments'])),
            'tempos': [{'at': p['position'] / 48000, 'bpm': p['tempo']} for p in t['timelineTempoMarkers']],
            'end': max([0] + [b['at'] + b['length'] for b in boxes] +
                       [m['position'] / 48000 for m in markers.values()]),
            'polyphony': e['maximumPolyphony'], 'cooldown': e['triggerCooldown'] / 1000}
    return program


def write_programs(metadata, raw_banks, add, chunks, blob_type):
    paths = {}
    for bank in metadata['banks']:
        if not bank['graph']['eventNodes'] and not bank['graph']['busNodes']:
            continue
        raw = next(raw for path, raw in raw_banks.items() if path.rsplit('/', 1)[-1] == bank['file'])
        program = compile_bank(bank, controller_sources(raw, chunks))
        name = 'audio/runtime/' + bank['file'] + '.json'
        add(name, blob_type, json.dumps(program, separators=(',', ':')).encode(), {'bank': bank['file']})
        for guid in bank['graph']['eventNodes']:
            paths.setdefault(metadata['paths'].get(guid, guid), []).append([bank['file'], guid])
    add('audio/runtime.json', blob_type, json.dumps({'version': 1, 'events': paths,
        'paths': metadata['paths']}, separators=(',', ':')).encode(), {'events': len(paths)})
