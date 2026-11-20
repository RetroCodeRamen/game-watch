#!/usr/bin/env python3
"""Convert the Metroid II (Game Boy) Samus sheet into 1-bit watch sprites.

    python3 scripts/gen_metroid_sprites.py            # writes metroid_watch/metroid_gb_sprites.h
    python3 scripts/gen_metroid_sprites.py --preview  # also writes assets/metroid_gb_preview.png

Frames are picked from the sheet by position (facing right), scaled so standing Samus is
22 px tall (the ball to 7 px), light shades become lit pixels, dark shades and black stay off,
then stray single pixels are removed. The morph ball is hand-drawn (HAND). PIXEL_FIXES below are hand touch-ups applied after scaling.
"""
import sys
from collections import deque
from pathlib import Path
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SHEET = ROOT / 'metroid_watch' / 'Game Boy _ GBC - Metroid II_ Return of Samus - Playable Characters - Samus Aran.png'
OUT = ROOT / 'metroid_watch' / 'metroid_gb_sprites.h'

BODY_H = 22          # standing Samus on the watch
STAND_SRC_H = 37     # standing Samus on the sheet
BALL_PX = 7          # morph ball fits a 1-tile duct

# name: (approx centre x, centre y on the sheet, target scale or None for body scale)
FRAMES = {
    'STAND':  (48, 37, None),
    'RUN0':   (153, 37, None),
    'RUN1':   (188, 37, None),
    'RUN2':   (233, 37, None),
    'JUMP':   (360, 34, None),
    'CROUCH': (113, 42, None),
}
# The Game Boy ball is a plain white dot at 7 px, so the ball is drawn here: a disc with a dark
# seam that turns a step per frame, which reads as rolling. (The somersault is the JUMP frame
# turned in quarter steps by the game, so it stays a crouched Samus rather than a ball.)
_DISC = ['..###..', '.#####.', '#######', '#######', '#######', '.#####.', '..###..']
_SEAMS = [[(2, 2), (3, 3), (4, 4)], [(3, 1), (3, 3), (3, 5)], [(4, 2), (3, 3), (2, 4)], [(1, 3), (3, 3), (5, 3)]]
HAND = {f'BALL{i}': [''.join('.' if (x, y) in seam else c for x, c in enumerate(row)) for y, row in enumerate(_DISC)]
        for i, seam in enumerate(_SEAMS)}
# hand touch-ups after scaling: name -> list of (x, y, on)
PIXEL_FIXES = {}


def components(mask):
    h, w = mask.shape
    seen = np.zeros_like(mask, bool)
    out = []
    for y0, x0 in zip(*np.nonzero(mask)):
        if seen[y0, x0]:
            continue
        q = deque([(y0, x0)]); seen[y0, x0] = True
        y1 = y2 = y0; x1 = x2 = x0
        while q:
            y, x = q.popleft()
            y1, y2, x1, x2 = min(y1, y), max(y2, y), min(x1, x), max(x2, x)
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < h and 0 <= xx < w and mask[yy, xx] and not seen[yy, xx]:
                        seen[yy, xx] = True; q.append((yy, xx))
        out.append((x1, y1, x2 + 1, y2 + 1))
    return out


def pick(boxes, cx, cy):
    best = None
    for b in boxes:
        if b[0] - 4 <= cx <= b[2] + 4 and b[1] - 4 <= cy <= b[3] + 4:
            d = abs((b[0] + b[2]) / 2 - cx) + abs((b[1] + b[3]) / 2 - cy)
            if best is None or d < best[0]:
                best = (d, b)
    if not best:
        sys.exit(f'no sprite near {cx},{cy}')
    return best[1]


def convert(src, box, scale):
    x0, y0, x1, y1 = box
    a = np.array(src.crop(box).convert('L')).astype(float) / 255
    sil = (a > 0.05).astype(float)                 # anything not black is part of the sprite
    lit = (a > 0.5).astype(float)                  # light grey and white light up
    nw, nh = max(1, round((x1 - x0) * scale)), max(1, round((y1 - y0) * scale))
    S = np.array(Image.fromarray((sil * 255).astype(np.uint8)).resize((nw, nh), Image.BOX)) / 255
    L = np.array(Image.fromarray((lit * 255).astype(np.uint8)).resize((nw, nh), Image.BOX)) / 255
    on = (S > 0.45) & (L >= 0.5 * np.maximum(S, 1e-3))
    # drop stray pixels with no lit neighbour
    p = np.pad(on, 1)
    nb = sum(np.roll(np.roll(p, dy, 0), dx, 1) for dy in (-1, 0, 1) for dx in (-1, 0, 1))[1:-1, 1:-1] - on
    on &= nb > 0
    # trim empty borders
    ys, xs = np.nonzero(on)
    return on[ys.min():ys.max() + 1, xs.min():xs.max() + 1]


def main():
    src = Image.open(SHEET).convert('RGB')
    g = np.array(src).astype(int)
    mask = g.sum(2) > 0
    mask[:12] = False; mask[:, 410:] = False; mask[400:] = False
    boxes = components(mask)
    body = BODY_H / STAND_SRC_H
    frames = {}
    for name, (cx, cy, kind) in FRAMES.items():
        box = pick(boxes, cx, cy)
        scale = body if kind is None else BALL_PX / (box[2] - box[0])
        f = convert(src, box, scale)
        for x, y, v in PIXEL_FIXES.get(name, []):
            f[y, x] = v
        frames[name] = f
    for name, rows in HAND.items():
        frames[name] = np.array([[c == '#' for c in r] for r in rows])

    lines = ['#pragma once',
             '// Samus — converted from the Metroid II (Game Boy) sprite sheet by',
             '// scripts/gen_metroid_sprites.py. Do not edit by hand: re-run the script.',
             '// Facing RIGHT. Rows are MSB-first bytes. Anchor: ax = body centre column, feet = last row.',
             '#include <Arduino.h>',
             '',
             'struct SprFrame { uint8_t w, h; int8_t ax; const uint8_t *bits; };',
             '']
    for name, f in frames.items():
        h, w = f.shape
        rb = (w + 7) // 8
        data = []
        for row in f:
            for b in range(rb):
                v = 0
                for i in range(8):
                    c = b * 8 + i
                    if c < w and row[c]:
                        v |= 0x80 >> i
                data.append(v)
        # body centre: mean column of the top third (the helmet), so the cannon doesn't shift her
        top = f[: max(1, h // 3)]
        ax = int(round(np.nonzero(top)[1].mean())) if top.any() else w // 2
        if name.startswith('BALL'):
            ax = w // 2
        lines.append(f'// {name}: {w}x{h}')
        for r in range(h):
            row = ''.join('#' if f[r, c] else '.' for c in range(w))
            lines.append(f'//   {row}')
        lines.append(f'static const uint8_t {name}_BITS[] = {{' + ', '.join(f'0x{v:02X}' for v in data) + '};')
        lines.append(f'static const SprFrame SPR_{name} = {{{w}, {h}, {ax}, {name}_BITS}};')
        lines.append('')
    lines.append('static const SprFrame *const SPR_RUN[3] = {&SPR_RUN0, &SPR_RUN1, &SPR_RUN2};')
    lines.append('static const SprFrame *const SPR_BALL[4] = {&SPR_BALL0, &SPR_BALL1, &SPR_BALL2, &SPR_BALL3};')
    OUT.write_text('\n'.join(lines) + '\n')
    print(f'wrote {OUT.relative_to(ROOT)} ({len(frames)} frames)')

    if '--preview' in sys.argv:
        P = 6
        W = sum(f.shape[1] + 4 for f in frames.values()) + 4
        H = max(f.shape[0] for f in frames.values()) + 4
        im = np.zeros((H, W), np.uint8); x = 2
        for f in frames.values():
            im[H - 2 - f.shape[0]:H - 2, x:x + f.shape[1]] = f * 255; x += f.shape[1] + 4
        Image.fromarray(im).resize((W * P, H * P), Image.NEAREST).save(ROOT / 'assets' / 'metroid_gb_preview.png')
        print('wrote assets/metroid_gb_preview.png')


if __name__ == '__main__':
    main()
