#!/usr/bin/env python3
"""Generates assets/icon.png (48x48), assets/banner.png (256x128) and a short
silent assets/audio.wav using only the standard library. Replace these with
your own artwork any time."""
import os, struct, wave, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'assets')
os.makedirs(OUT, exist_ok=True)

BLACK, WHITE, ORANGE = (18, 18, 24), (244, 244, 238), (255, 140, 20)


def png(path, w, h, pixel):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            raw += bytes(pixel(x, y)) + b'\xff'

    def chunk(tag, data):
        c = struct.pack('>I', len(data)) + tag + data
        return c + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)

    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)))
        f.write(chunk(b'IDAT', zlib.compress(bytes(raw), 9)))
        f.write(chunk(b'IEND', b''))


def checker(w, h, cell):
    def px(x, y):
        # black/white checker with an orange "player" square in the middle
        if abs(x - w // 2) < h // 6 and abs(y - h // 2) < h // 6:
            return ORANGE
        return BLACK if ((x // cell) + (y // cell)) % 2 == 0 else WHITE
    return px


png(os.path.join(OUT, 'icon.png'), 48, 48, checker(48, 48, 12))
png(os.path.join(OUT, 'banner.png'), 256, 128, checker(256, 128, 32))

with wave.open(os.path.join(OUT, 'audio.wav'), 'wb') as wf:
    wf.setnchannels(1)
    wf.setsampwidth(2)
    wf.setframerate(32728)
    wf.writeframes(b'\x00\x00' * 32728)  # 1 second of silence

print('wrote assets/icon.png, assets/banner.png, assets/audio.wav')
