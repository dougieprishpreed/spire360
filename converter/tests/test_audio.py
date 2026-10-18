"""Binary bank boundaries and exact Vorbis end positions (no game fixtures)."""
from pathlib import Path
import struct
import sys
import unittest
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from spire_convert import audio, audio_events


def fsb():
    # Stereo, default 48 kHz, overridden to 44.1 kHz; ten frames,
    # loop [2, 9). The two extra chunks have different sizes.
    sample = struct.pack('<Q', (10 << 34) | (1 << 5) | (9 << 1) | 1)
    sample += struct.pack('<II', (2 << 25) | (4 << 1) | 1, 44100)
    sample += struct.pack('<III', (3 << 25) | (8 << 1), 2, 8)
    names = struct.pack('<I', 4) + b'synthetic\0'
    head = struct.pack('<4s6I', b'FSB5', 1, 1, len(sample), len(names), 4, 15)
    return head.ljust(60, b'\0') + sample + names + b'abcd'


def page(granule, final=False):
    return struct.pack('<4sBBQIIIB', b'OggS', 0, 4 if final else 0,
                       granule, 10, 0, 0, 1) + b'\x04test'


class Audio(unittest.TestCase):
    def test_controller_context_survives_flat_transition_chunks(self):
        timeline, controller = uuid.UUID(int=1), uuid.UUID(int=2)
        def chunk(tag, data):
            return tag + struct.pack('<I', len(data)) + data + (b'\0' if len(data) & 1 else b'')
        body = chunk(b'TLNB', timeline.bytes_le) + chunk(b'TRNB', uuid.UUID(int=3).bytes_le)
        body += chunk(b'CTRO', struct.pack('<HH', 3, 16) + controller.bytes_le)
        raw = b'RIFF' + struct.pack('<I', len(body) + 4) + b'FEV ' + body
        self.assertEqual(audio_events.controller_sources(raw, audio.chunks), {str(controller): str(timeline)})

    def test_imported_pcm_stream_has_wave_header(self):
        raw = audio.stream_payload(dict(type='AudioStreamWAV', props=dict(data=b'\x00\x7f\x80\xff', format=0,
                                                                          mix_rate=22050, stereo=False)))
        self.assertEqual(raw[:4], b'RIFF')
        self.assertEqual(struct.unpack_from('<4h', raw, 44), (0, 32512, -32768, -256))
        self.assertEqual(struct.unpack_from('<I', raw, 24)[0], 22050)

    def test_variable_sample_headers_and_loop_end(self):
        sample, = audio.fsb_samples(fsb())
        self.assertEqual((sample['rate'], sample['channels'], sample['frames']), (44100, 2, 10))
        self.assertEqual((sample['loop_start'], sample['loop_end']), (2, 9))
        self.assertEqual(sample['name'], 'synthetic')

    def test_bank_padding_and_fsb_alignment(self):
        first = b'TEST' + struct.pack('<I', 1) + b'x\0'
        body = b'\0' * 22 + fsb()
        payload = first + b'SND ' + struct.pack('<I', len(body)) + body
        bank = b'RIFF' + struct.pack('<I', 4 + len(payload)) + b'FEV ' + payload
        result, = audio.bank_fsbs(bank)
        self.assertEqual(result[0], fsb())
        self.assertEqual(result[1][0]['frames'], 10)
        with self.assertRaises(ValueError):
            audio.bank_fsbs(bank[:-1])

    def test_reject_truncated_sample_headers(self):
        data = bytearray(fsb())
        struct.pack_into('<I', data, 68, (2 << 25) | (500 << 1))
        with self.assertRaises(ValueError):
            audio.fsb_samples(data)

    def test_ogg_trim_keeps_packets_and_is_idempotent(self):
        original = page(80) + page(120, True)
        fixed = audio.trim_ogg(original, 100)
        self.assertEqual(fixed[:len(page(80))], page(80))
        self.assertEqual(fixed[-5:], original[-5:])
        self.assertEqual(struct.unpack_from('<Q', fixed, len(page(80)) + 6)[0], 100)
        self.assertEqual(audio.trim_ogg(fixed, 100), fixed)
        with self.assertRaises(ValueError):
            audio.trim_ogg(original, 79)
        with self.assertRaises(ValueError):
            audio.trim_ogg(original[:-1], 100)


if __name__ == '__main__':
    unittest.main()
