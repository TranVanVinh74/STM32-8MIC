"""Capture protocol 8MICPCM1. Requires the matching firmware in this branch.
Usage: python record_8mic.py --port COM8 --seconds 5
Only pyserial is required for recording; protocol tests use the standard library.
"""
import argparse
import json
import math
from pathlib import Path
import struct
import time
import wave
import zlib

MAGIC = b'8MICPCM1'
HEADER = struct.Struct('<8sIIHHIII')
CHANNELS, RATE, BLOCK_FRAMES = 8, 16000, 512
PAYLOAD_BYTES = BLOCK_FRAMES * CHANNELS * 2


class CaptureError(RuntimeError):
    pass


class PacketParser:
    def __init__(self):
        self.buffer = bytearray()
        self.locked = False
        self.skipped_bytes = 0
        self.bad_candidates = 0

    def feed(self, data):
        self.buffer.extend(data)
        while True:
            if self.locked:
                if len(self.buffer) < len(MAGIC):
                    return
                if not self.buffer.startswith(MAGIC):
                    raise CaptureError('Lost packet alignment after capture started.')
            else:
                idx = self.buffer.find(MAGIC)
                if idx < 0:
                    discard = max(0, len(self.buffer) - len(MAGIC) + 1)
                    self.skipped_bytes += discard
                    del self.buffer[:discard]
                    return
                self.skipped_bytes += idx
                del self.buffer[:idx]
            if len(self.buffer) < HEADER.size:
                return
            magic, seq, rate, channels, frames, size, flags, crc = HEADER.unpack_from(self.buffer)
            valid = (rate == RATE and channels == CHANNELS and
                     ((flags == 0 and frames == BLOCK_FRAMES and size == PAYLOAD_BYTES) or
                      (flags != 0 and frames == 0 and size == 0)))
            if not valid:
                if self.locked:
                    raise CaptureError('Invalid packet header (format mismatch or corrupted UART).')
                self.bad_candidates += 1
                del self.buffer[0]
                continue
            if len(self.buffer) < HEADER.size + size:
                return
            payload = bytes(self.buffer[HEADER.size:HEADER.size + size])
            actual_crc = zlib.crc32(payload, zlib.crc32(self.buffer[:28])) & 0xffffffff
            if actual_crc != crc:
                if self.locked:
                    raise CaptureError(f'CRC error at sequence {seq}; capture stopped.')
                self.bad_candidates += 1
                del self.buffer[0]
                continue
            del self.buffer[:HEADER.size + size]
            self.locked = True
            yield seq, flags, payload


def fault_description(flags):
    names = []
    for bit, pair in enumerate(('I2S2', 'I2S1', 'I2S3', 'I2S5')):
        if flags & (1 << bit):
            names.append(f'{pair} DMA error')
        if flags & (1 << (4 + bit)):
            names.append(f'{pair} I2S overrun')
    for bit, name in ((8, 'DMA block overwritten/out of order'),
                      (9, 'timeout waiting for all four DMA streams'),
                      (10, 'WS not high at slave inputs during startup')):
        if flags & (1 << bit):
            names.append(name)
    return ', '.join(names) or 'unknown device fault'


class WavCapture:
    def __init__(self, writer, target_frames):
        self.writer = writer
        self.target_frames = target_frames
        self.frames = 0
        self.blocks = 0
        self.last_sequence = None
        self.first_sequence = None
        self.minimum = [32767] * CHANNELS
        self.maximum = [-32768] * CHANNELS
        self.square_sum = [0] * CHANNELS
        self.clipped = [0] * CHANNELS

    @property
    def done(self):
        return self.frames >= self.target_frames

    def accept(self, seq, flags, payload):
        if self.done:
            return
        if flags:
            raise CaptureError(f'MCU fault 0x{flags:08x}: {fault_description(flags)}. Reset board after repair.')
        if len(payload) != PAYLOAD_BYTES:
            raise CaptureError('Incorrect PCM payload size.')
        if self.last_sequence is not None and seq != ((self.last_sequence + 1) & 0xffffffff):
            raise CaptureError(f'Sequence discontinuity: {self.last_sequence} -> {seq} (lost block or MCU reset).')
        if self.first_sequence is None:
            self.first_sequence = seq
        self.last_sequence = seq
        count = min(BLOCK_FRAMES, self.target_frames - self.frames)
        pcm = payload[:count * CHANNELS * 2]
        self.writer.writeframesraw(pcm)
        for frame in struct.iter_unpack('<8h', pcm):
            for ch, value in enumerate(frame):
                self.minimum[ch] = min(self.minimum[ch], value)
                self.maximum[ch] = max(self.maximum[ch], value)
                self.square_sum[ch] += value * value
                self.clipped[ch] += value in (-32768, 32767)
        self.frames += count
        self.blocks += 1

    def stats(self):
        return [{'channel': i + 1,
                 'min': self.minimum[i] if self.frames else None,
                 'max': self.maximum[i] if self.frames else None,
                 'rms': math.sqrt(self.square_sum[i] / self.frames) if self.frames else 0,
                 'clipped_samples': self.clipped[i]}
                for i in range(CHANNELS)]


def record(args):
    import serial
    target = round(args.seconds * RATE)
    if target < 1 or not math.isfinite(args.timeout) or args.timeout <= 0:
        raise ValueError('Duration and timeout must be positive.')
    output = Path(args.output)
    if output.suffix.lower() != '.wav':
        raise ValueError('Output must have a .wav extension.')
    partial = output.with_name(output.stem + '.partial.wav')
    report = output.with_suffix('.json')
    for path in (output, partial, report):
        if path.exists():
            raise FileExistsError(f'{path} already exists; choose another --output.')
    output.parent.mkdir(parents=True, exist_ok=True)
    parser = PacketParser()
    started = time.monotonic()
    last_valid = started
    last_print = started
    error = None
    capture = None
    print(f'Opening {args.port}, {args.baud} baud; target {target / RATE:.3f}s, 8ch PCM16.')
    try:
        with serial.Serial(args.port, args.baud, timeout=0.1,
                           xonxoff=False, rtscts=False, dsrdtr=False) as ser:
            ser.reset_input_buffer()
            with wave.open(str(partial), 'wb') as wf:
                wf.setnchannels(CHANNELS)
                wf.setsampwidth(2)
                wf.setframerate(RATE)
                capture = WavCapture(wf, target)
                while not capture.done:
                    chunk = ser.read(min(max(ser.in_waiting, 1), 65536))
                    for seq, flags, payload in parser.feed(chunk):
                        capture.accept(seq, flags, payload)
                        last_valid = time.monotonic()
                        if capture.done:
                            break
                    now = time.monotonic()
                    if now - last_print >= 0.5:
                        print(f'Captured {capture.frames / RATE:.3f}/{target / RATE:.3f}s', end='\r')
                        last_print = now
                    if now - last_valid >= args.timeout:
                        raise CaptureError('No valid packets before timeout. Check matching firmware, COM, baud and wiring.')
    except (CaptureError, serial.SerialException, OSError, KeyboardInterrupt) as exc:
        error = str(exc) or 'Stopped by user.'
    success = capture is not None and capture.done and error is None
    if success:
        partial.rename(output)
    info = {
        'status': 'complete' if success else 'incomplete', 'error': error,
        'protocol': MAGIC.decode(), 'port': args.port, 'baud': args.baud,
        'sample_rate_nominal': RATE, 'channels': CHANNELS, 'sample_width_bytes': 2,
        'channel_order': ['I2S2 L','I2S2 R','I2S1 L','I2S1 R','I2S3 L','I2S3 R','I2S5 L','I2S5 R'],
        'target_frames': target, 'written_frames': capture.frames if capture else 0,
        'wall_seconds': time.monotonic() - started,
        'initial_skipped_bytes': parser.skipped_bytes,
        'initial_bad_candidates': parser.bad_candidates,
        'first_sequence': capture.first_sequence if capture else None,
        'last_sequence': capture.last_sequence if capture else None,
        'per_channel': capture.stats() if capture else [],
    }
    report.write_text(json.dumps(info, indent=2), encoding='utf-8')
    print()
    if success:
        print(f'Saved {output}: {capture.frames} frames, {capture.frames / RATE:.3f}s. Report: {report}')
    else:
        print(f'FAILED: {error}. Partial WAV (if opened): {partial}. Report: {report}')
    if capture:
        for ch in capture.stats():
            print(f"CH{ch['channel']}: min={ch['min']} max={ch['max']} RMS={ch['rms']:.1f} clipped={ch['clipped_samples']}")
    return 0 if success else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--port', default='COM8')
    ap.add_argument('--baud', type=int, default=4000000)
    ap.add_argument('--seconds', type=float, default=5.0)
    ap.add_argument('--timeout', type=float, default=5.0, help='Seconds without a valid packet before failing')
    ap.add_argument('--output', default='array_8mic_test.wav')
    args = ap.parse_args()
    if not math.isfinite(args.seconds) or args.seconds <= 0:
        ap.error('--seconds must be positive and finite')
    try:
        return record(args)
    except (ValueError, OSError, ImportError) as exc:
        print(f'ERROR: {exc}')
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
