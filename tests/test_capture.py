import ctypes
import importlib.util
import io
from pathlib import Path
import random
import shutil
import struct
import subprocess
import tempfile
import unittest
import wave
import zlib

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('capture', ROOT / 'Python/record_8mic.py')
r = importlib.util.module_from_spec(spec)
spec.loader.exec_module(r)
PCM = struct.pack('<8h', -32768, -100, -1, 0, 1, 100, 1000, 32767) * 512


def packet(seq=0, payload=PCM, flags=0):
    h = struct.pack('<8sIIHHII', r.MAGIC, seq, 16000, 8, 0 if flags else 512, len(payload), flags)
    return h + struct.pack('<I', zlib.crc32(payload, zlib.crc32(h))) + payload


class ProtocolTests(unittest.TestCase):
    def test_random_chunks_and_initial_partial_packet(self):
        wire = packet(90)[100:] + packet(91) + packet(92)
        parser = r.PacketParser()
        got = []
        rng = random.Random(32)
        while wire:
            n = rng.randint(1, 500)
            got.extend(parser.feed(wire[:n])); wire = wire[n:]
        self.assertEqual([p[0] for p in got], [91, 92])
        self.assertEqual(got[0][2], PCM)
        self.assertGreater(parser.skipped_bytes, 0)

    def test_every_header_split(self):
        wire = packet(1)
        for i in range(1, r.HEADER.size + 1):
            parser = r.PacketParser()
            self.assertEqual(list(parser.feed(wire[:i])), [])
            self.assertEqual(list(parser.feed(wire[i:])), [(1, 0, PCM)])

    def test_magic_inside_payload_is_not_a_header(self):
        payload = r.MAGIC + PCM[8:]
        got = list(r.PacketParser().feed(packet(8, payload) + packet(9)))
        self.assertEqual(got, [(8, 0, payload), (9, 0, PCM)])

    def test_bad_crc_after_lock(self):
        parser = r.PacketParser(); list(parser.feed(packet()))
        bad = bytearray(packet(1)); bad[-1] ^= 0x40
        with self.assertRaisesRegex(r.CaptureError, 'CRC'):
            list(parser.feed(bad))

    def test_dropped_byte_after_lock(self):
        parser = r.PacketParser(); list(parser.feed(packet()))
        wire = packet(1); wire = wire[:100] + wire[101:] + packet(2)
        with self.assertRaisesRegex(r.CaptureError, 'CRC'):
            list(parser.feed(wire))

    def test_corrupt_header_after_lock(self):
        parser = r.PacketParser(); list(parser.feed(packet()))
        bad = bytearray(packet(1)); bad[20] ^= 1
        with self.assertRaises(r.CaptureError): list(parser.feed(bad))

    def test_startup_false_magic_and_bounded_junk(self):
        parser = r.PacketParser()
        self.assertEqual(list(parser.feed(b'x' * 100000)), [])
        self.assertLessEqual(len(parser.buffer), 7)
        got = list(parser.feed(r.MAGIC + b'\0'*24 + packet(20)))
        self.assertEqual(got, [(20, 0, PCM)])
        self.assertEqual(parser.bad_candidates, 1)

    def test_fault_packet(self):
        got = list(r.PacketParser().feed(packet(12, b'', 1 << 9)))
        self.assertEqual(got, [(12, 1 << 9, b'')])
        with self.assertRaisesRegex(r.CaptureError, 'timeout'):
            r.WavCapture(io.BytesIO(), 1000).accept(*got[0])

    def test_missing_and_duplicate_blocks(self):
        for second in (4, 6, 0):
            buf = io.BytesIO()
            with wave.open(buf, 'wb') as wf:
                wf.setparams((8, 2, 16000, 0, 'NONE', 'not compressed'))
                capture = r.WavCapture(wf, 16000)
                capture.accept(4, 0, PCM)
                with self.assertRaisesRegex(r.CaptureError, 'Sequence'):
                    capture.accept(second, 0, PCM)

    def test_sequence_wrap(self):
        buf = io.BytesIO()
        with wave.open(buf, 'wb') as wf:
            wf.setparams((8, 2, 16000, 0, 'NONE', 'not compressed'))
            cap = r.WavCapture(wf, 1024)
            cap.accept(0xffffffff, 0, PCM); cap.accept(0, 0, PCM)
            self.assertTrue(cap.done)

    def test_exact_five_seconds_and_channel_order(self):
        buf = io.BytesIO()
        with wave.open(buf, 'wb') as wf:
            wf.setparams((8, 2, 16000, 0, 'NONE', 'not compressed'))
            cap = r.WavCapture(wf, 80000)
            for seq in range(160): cap.accept(seq, 0, PCM)
            self.assertEqual(cap.blocks, 157)
            self.assertEqual(cap.frames, 80000)
        buf.seek(0)
        with wave.open(buf, 'rb') as wf:
            self.assertEqual((wf.getnchannels(), wf.getsampwidth(), wf.getframerate(), wf.getnframes()), (8, 2, 16000, 80000))
            self.assertEqual(wf.readframes(80001), (PCM * 157)[:80000 * 16])

    def test_firmware_dma_guards_and_interleaving(self):
        gcc = shutil.which('gcc')
        if not gcc: self.skipTest('gcc needed for DMA register-mock tests')
        with tempfile.TemporaryDirectory() as tmp:
            binary = str(Path(tmp) / 'dma_test')
            subprocess.run([gcc, '-std=c99', '-Wall', '-Wextra', '-Werror',
                            '-Wno-pointer-to-int-cast', '-I'+str(ROOT/'tests/mock'),
                            str(ROOT/'tests/test_dma.c'), '-o', binary], check=True)
            subprocess.run([binary], check=True)

    def test_firmware_crc_and_wire_format(self):
        gcc = shutil.which('gcc')
        if not gcc: self.skipTest('gcc needed for C/Python wire-format cross-check')
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / 'test.c'; libpath = Path(tmp) / 'test.so'
            src.write_text('#include "audio_protocol.h"\nvoid make_header(uint8_t *h,uint32_t seq,uint16_t frames,uint32_t flags,const uint8_t *pcm) {audio_header(h,seq,16000,frames,flags,pcm);}\n')
            subprocess.run([gcc, '-std=c99', '-shared', '-fPIC', '-Wall', '-Wextra', '-Werror', '-I'+str(ROOT/'FFT/B1/lib1'), str(src), '-o', str(libpath)], check=True)
            lib = ctypes.CDLL(str(libpath)); lib.make_header.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint16, ctypes.c_uint32, ctypes.c_void_p]
            for frames, flags, payload in ((512, 0, PCM), (0, 512, b'')):
                header = ctypes.create_string_buffer(32)
                lib.make_header(header, 12345, frames, flags, payload)
                self.assertEqual(header.raw + payload, packet(12345, payload, flags))


if __name__ == '__main__': unittest.main()
