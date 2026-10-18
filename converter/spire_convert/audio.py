"""Offline FMOD bank conversion. Game DLLs are never loaded.

FmodMetadata preserves the Studio graph and rebuilds embedded FSB5 Vorbis
samples as standard Ogg streams without re-encoding. Inputs and converted
cache are player data, kept outside git. Loop end is exclusive. vgmstream
can independently validate the converted samples against the source bank.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess

from . import s3p

ROOT = Path(__file__).resolve().parents[2]
CACHE = ROOT / 'converter/.cache/audio'
RATES = (4000, 8000, 11000, 11025, 16000, 22050, 24000, 32000, 44100, 48000, 96000)


def chunks(data, start=12, end=None):
    """RIFF chunks, retaining absolute offsets; LIST contents are recursive."""
    if end is None:
        if len(data) < 12 or data[:4] != b'RIFF' or data[8:12] != b'FEV ':
            raise ValueError('not an FMOD Studio bank')
        end = struct.unpack_from('<I', data, 4)[0] + 8
    if end > len(data):
        raise ValueError('truncated bank')
    pos = start
    while pos + 8 <= end:
        if data[pos] == 0:
            pos += 1
        if pos + 8 > end:
            break
        tag, size = struct.unpack_from('<4sI', data, pos)
        stop = pos + 8 + size
        if stop > end:
            raise ValueError('bank chunk exceeds parent')
        if tag == b'LIST' and size >= 4:
            yield from chunks(data, pos + 12, stop)
        else:
            yield tag, pos + 8, stop
        pos = stop


def fsb_samples(data):
    """FSB5 sample metadata, including loop points the decoder may suppress."""
    if len(data) < 60 or data[:4] != b'FSB5':
        raise ValueError('not FSB5')
    version, count, headers, names, payload = struct.unpack_from('<5I', data, 4)
    if version not in (0, 1):
        raise ValueError('unsupported FSB5 version')
    base = 60 if version else 64
    if base + headers + names + payload != len(data) or count > headers // 8:
        raise ValueError('invalid FSB5 sizes')
    pos, limit = base, base + headers
    result = []
    for index in range(count):
        if pos + 8 > limit:
            raise ValueError('truncated FSB5 sample header')
        bits, = struct.unpack_from('<Q', data, pos)
        pos += 8
        freq = (bits >> 1) & 15
        if freq >= len(RATES):
            raise ValueError('invalid FSB5 rate')
        info = dict(index=index, frames=bits >> 34, rate=RATES[freq],
                    channels=(1, 2, 6, 8)[(bits >> 5) & 3], loop_start=0, loop_end=0)
        more = bits & 1
        while more:
            if pos + 4 > limit:
                raise ValueError('truncated FSB5 extra header')
            extra, = struct.unpack_from('<I', data, pos)
            size, kind, more = (extra >> 1) & 0xffffff, extra >> 25, extra & 1
            pos += 4
            if pos + size > limit:
                raise ValueError('FSB5 extra header exceeds sample headers')
            if kind == 1 and size >= 1:
                info['channels'] = data[pos]
            elif kind == 2 and size >= 4:
                info['rate'], = struct.unpack_from('<I', data, pos)
            elif kind == 3 and size >= 8:
                begin, last = struct.unpack_from('<II', data, pos)
                if begin <= last < info['frames']:
                    info.update(loop_start=begin, loop_end=last + 1)
            elif kind == 14 and size >= 4:
                info['channels'] *= struct.unpack_from('<I', data, pos)[0]
            pos += size
        if names:
            if names < count * 4:
                raise ValueError('truncated FSB5 name offsets')
            off, = struct.unpack_from('<I', data, limit + index * 4)
            stop = data.find(b'\0', limit + off, limit + names)
            if off >= names or stop < 0:
                raise ValueError('invalid FSB5 sample name')
            info['name'] = data[limit + off:stop].decode('utf-8', errors='replace')
        result.append(info)
    return result


def bank_fsbs(data):
    result = []
    for tag, start, end in chunks(data):
        if tag != b'SND ':
            continue
        # Studio aligns each FSB to 32 bytes inside its SND chunk.
        pos = data.find(b'FSB5', start, min(start + 64, end))
        if pos < 0:
            raise ValueError('SND chunk has no unencrypted FSB5 header')
        version, = struct.unpack_from('<I', data, pos + 4)
        sizes = struct.unpack_from('<3I', data, pos + 12)
        size = (60 if version else 64) + sum(sizes)
        if pos + size > end:
            raise ValueError('truncated bank FSB5')
        fsb = data[pos:pos + size]
        result.append((fsb, fsb_samples(fsb)))
    return result


def trim_ogg(data, frames):
    """Keep the original packets; the FSB frame count trims decoder padding.

    The rebuilder estimates its final Ogg granule from packet block sizes.
    FSB's explicit count is authoritative (including a partial last block).
    Only the final page's granule and its Ogg CRC change.
    """
    pos, previous, last = 0, 0, None
    while pos < len(data):
        if data[pos:pos + 4] != b'OggS' or pos + 27 > len(data):
            raise ValueError('invalid rebuilt Ogg page')
        segments = data[pos + 26]
        if pos + 27 + segments > len(data):
            raise ValueError('truncated Ogg segment table')
        end = pos + 27 + segments + sum(data[pos + 27:pos + 27 + segments])
        if end > len(data):
            raise ValueError('truncated Ogg body')
        granule, = struct.unpack_from('<Q', data, pos + 6)
        if data[pos + 5] & 4:
            if end != len(data) or granule < frames or frames < previous:
                raise ValueError('FSB frame count is outside the final Ogg page')
            last = pos
        elif granule != 0xffffffffffffffff:
            previous = granule
        pos = end
    if last is None:
        raise ValueError('rebuilt Ogg has no final page')
    if struct.unpack_from('<Q', data, last + 6)[0] == frames:
        return data
    page = bytearray(data[last:])
    struct.pack_into('<Q', page, 6, frames)
    page[22:26] = b'\0' * 4
    # Ogg uses a non-reflected CRC, unlike zlib.crc32.
    crc = 0
    for byte in page:
        crc ^= byte << 24
        for _ in range(8):
            crc = ((crc << 1) ^ (0x04c11db7 if crc & 0x80000000 else 0)) & 0xffffffff
    struct.pack_into('<I', page, 22, crc)
    return data[:last] + page


def find_decoder(explicit=None):
    candidates = (explicit, os.environ.get('SPIRE_VGMSTREAM'),
                  shutil.which('vgmstream-cli'), CACHE / 'tools/vgmstream-cli.exe',
                  CACHE / 'tools/vgmstream-cli')
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return str(Path(candidate).resolve())
    if explicit:
        raise RuntimeError('vgmstream-cli not found: ' + explicit)
    return None


class Baker:
    def __init__(self, src, banks, decoder=None, cache=CACHE):
        self.src, self.banks, self.cache = src, sorted(banks), Path(cache)
        self.decoder = find_decoder(decoder)

    def write(self, add):
        self.cache.mkdir(parents=True, exist_ok=True)
        source_hash = hashlib.sha256(b'Spire360 bank metadata 2')
        for source in sorted((ROOT / 'tools/fmod_metadata').iterdir()):
            if source.suffix in ('.cs', '.csproj'):
                source_hash.update(source.read_bytes())
        raw_banks = {}
        for name in self.banks:
            raw = self.src.read(name)
            raw_banks[name] = raw
            source_hash.update(name.encode('utf-8'))
            source_hash.update(hashlib.sha256(raw).digest())
        folder = self.cache / source_hash.hexdigest()
        bank_folder = folder / 'banks'
        bank_folder.mkdir(parents=True, exist_ok=True)
        for name, raw in raw_banks.items():
            dest = bank_folder / Path(name).name
            if dest.exists() and dest.read_bytes() == raw:
                continue
            dest.write_bytes(raw)
        metadata_path = folder / 'metadata.json'
        tool = ROOT / 'out/tools/fmod_metadata/FmodMetadata.dll'
        sources = ROOT / 'tools/fmod_metadata'
        if not tool.exists() or any(p.stat().st_mtime > tool.stat().st_mtime
                                   for p in sources.iterdir() if p.suffix in ('.cs', '.csproj')):
            subprocess.run(['dotnet', 'build', str(sources / 'FmodMetadata.csproj'), '-c', 'Release',
                            '-o', str(tool.parent), '--nologo', '--disable-build-servers'], check=True)
        if not metadata_path.exists():
            subprocess.run(['dotnet', str(tool), str(bank_folder), str(metadata_path)], check=True)
        metadata = json.loads(metadata_path.read_text(encoding='utf-8'))
        if metadata.get('version') != 1:
            raise ValueError('unsupported audio metadata version')
        by_name = {bank['file']: bank for bank in metadata['banks']}
        written = set()
        for name, raw in raw_banks.items():
            bank = by_name[Path(name).name]
            graph = bank['graph']
            sample_entries = {}
            bank['samples'] = sample_entries
            fsbs = bank_fsbs(raw)
            for fsb_index, (fsb, samples) in enumerate(fsbs):
                key = hashlib.sha256(fsb + b'Fmod5Sharp 3.0.1 Ogg 1').hexdigest()
                sample_folder = self.cache / 'samples' / key
                sample_folder.mkdir(parents=True, exist_ok=True)
                fsb_path = sample_folder / 'bank.fsb'
                if not fsb_path.exists():
                    fsb_path.write_bytes(fsb)
                exports = sample_folder / 'files.json'
                if not exports.exists():
                    subprocess.run(['dotnet', str(tool), '--fsb', str(fsb_path), str(sample_folder)], check=True)
                exported = {item['index']: sample_folder / item['file']
                            for item in json.loads(exports.read_text(encoding='utf-8'))}
                resources = [(guid, n['data']) for guid, n in graph['wavEntries'].items()
                             if n['data']['soundBankIndex'] == fsb_index]
                needed = sorted({data['subsoundIndex'] for _, data in resources})
                for index in needed:
                    if not 0 <= index < len(samples):
                        raise ValueError(f'{name}: sample {index} outside FSB5')
                    info = samples[index]
                    converted = exported[index]
                    payload = converted.read_bytes()
                    if converted.suffix == '.ogg':
                        fixed = trim_ogg(payload, info['frames'])
                        if fixed != payload:
                            temp = converted.with_suffix('.tmp')
                            temp.write_bytes(fixed)
                            temp.replace(converted)
                            payload = fixed
                    if self.decoder:
                        check = json.loads(subprocess.check_output(
                            [self.decoder, '-m', '-I', str(converted)], text=True))
                        if (check['sampleRate'], check['channels'], check['numberOfSamples']) != \
                                (info['rate'], info['channels'], info['frames']):
                            raise ValueError(f'{name}: rebuilt sample {index} metadata differs from FSB5')
                    entry = 'audio/samples/' + hashlib.sha256(payload).hexdigest() + converted.suffix
                    if entry not in written:
                        add(entry, s3p.TYPE_AUDIO, payload, info)
                        written.add(entry)
                    for guid, resource in resources:
                        if resource['subsoundIndex'] != index:
                            continue
                        sample = dict(path=entry, stream=resource['loadingMode'].endswith('StreamFromDisk'), **info)
                        sample_entries[guid] = sample
                print(f'  audio {Path(name).name} FSB {fsb_index}: {len(needed)} samples', flush=True)
            if len(sample_entries) != len(graph['wavEntries']):
                raise ValueError(f'{name}: waveform resources refer to a missing FSB bank')
        metadata['rebuilder'] = 'Fmod5Sharp 3.0.1'
        add('audio/events.json', s3p.TYPE_BLOB,
            json.dumps(metadata, separators=(',', ':')).encode('utf-8'),
            {'events': sum(p.startswith('event:') for p in metadata['paths'].values()), 'samples': len(written)})
