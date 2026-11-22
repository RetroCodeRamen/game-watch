#!/usr/bin/env python3
# Turn a run of simulator frames (128x64 .pbm) into an animated GIF that looks like the watch's OLED:
#   python3 tools/oled_gif.py out.gif <ms per frame> a_000.pbm a_001.pbm ...
# Same look as oled_png.py (soft white pixels, faint grid, black glass, dark bezel), 3x scale, no bloom
# so the GIF stays small. Identical consecutive frames are merged into one longer frame.
import sys
from PIL import Image, ImageDraw
S, GAP, PAD, BEZ = 3, 1, 8, 5
LIT, DIM, GLASS, BEZEL, BG = (236, 244, 255), (14, 16, 20), (5, 6, 8), (38, 40, 46), (13, 17, 23)
W, H = 128 * S + 2 * PAD + 2 * BEZ, 64 * S + 2 * PAD + 2 * BEZ

base = Image.new('RGB', (W, H), BG); d = ImageDraw.Draw(base)
d.rounded_rectangle([0, 0, W - 1, H - 1], 14, fill=BEZEL)
d.rounded_rectangle([BEZ, BEZ, W - BEZ - 1, H - BEZ - 1], 9, fill=GLASS)
for y in range(64):
    for x in range(128):
        x0, y0 = BEZ + PAD + x * S, BEZ + PAD + y * S
        d.rectangle([x0, y0, x0 + S - 1 - GAP, y0 + S - 1 - GAP], fill=DIM)

def screen(pbm):
    img = base.copy(); dd = ImageDraw.Draw(img); px = Image.open(pbm).convert('1').load()
    for y in range(64):
        for x in range(128):
            if px[x, y] == 0:                       # PBM: 1 (black) = lit pixel
                x0, y0 = BEZ + PAD + x * S, BEZ + PAD + y * S
                dd.rectangle([x0, y0, x0 + S - 1 - GAP, y0 + S - 1 - GAP], fill=LIT)
    return img

out, ms, files = sys.argv[1], int(sys.argv[2]), sys.argv[3:]
frames, durs, last = [], [], None
for f in files:
    raw = open(f, 'rb').read()
    if raw == last: durs[-1] += ms; continue
    last = raw; frames.append(screen(f).quantize(colors=8, method=Image.Quantize.MEDIANCUT)); durs.append(ms)
pal = frames[0]
frames = [fr.convert('RGB').quantize(palette=pal, dither=Image.Dither.NONE) for fr in frames]
frames[0].save(out, save_all=True, append_images=frames[1:], duration=durs, loop=0, optimize=True, disposal=1)
print(f"  {out}: {len(frames)} frames, {sum(durs) / 1000:.1f} s")
