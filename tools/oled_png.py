#!/usr/bin/env python3
# Turn simulator frames (128x64 .pbm) into README screenshots that look like the watch's OLED:
#   python3 tools/oled_png.py out.png frame.pbm            one screen
#   python3 tools/oled_png.py out.png a.pbm b.pbm c.pbm    a row of screens
# Lit pixels are a soft white with a faint pixel grid, on a black glass panel in a dark bezel.
import sys
from PIL import Image, ImageChops, ImageDraw, ImageFilter
S, GAP, PAD, BEZ = 4, 1, 10, 6           # px per pixel, grid gap, glass margin, bezel
LIT, DIM, GLASS, BEZEL = (236, 244, 255), (11, 13, 16), (5, 6, 8), (38, 40, 46)

def screen(pbm):
    src = Image.open(pbm).convert('1')
    w, h = 128 * S + 2 * PAD, 64 * S + 2 * PAD
    img = Image.new('RGB', (w + 2 * BEZ, h + 2 * BEZ), (13, 17, 23))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, img.width - 1, img.height - 1], 18, fill=BEZEL)
    d.rounded_rectangle([BEZ, BEZ, BEZ + w - 1, BEZ + h - 1], 12, fill=GLASS)
    lit = Image.new('RGB', img.size, (0, 0, 0)); ld = ImageDraw.Draw(lit)
    px = src.load()
    for y in range(64):
        for x in range(128):
            x0, y0 = BEZ + PAD + x * S, BEZ + PAD + y * S
            on = px[x, y] == 0                     # PBM: 1 (black) = lit pixel
            (ld if on else d).rectangle([x0, y0, x0 + S - 1 - GAP, y0 + S - 1 - GAP], fill=LIT if on else DIM)
    glow = lit.filter(ImageFilter.GaussianBlur(3)).point(lambda v: int(v * 0.35))   # a little OLED bloom
    out = Image.composite(lit, img, lit.convert('L').point(lambda v: 255 if v else 0))
    return ImageChops.add(out, glow)

ims = [screen(p) for p in sys.argv[2:]]
gap = 16
out = Image.new('RGB', (sum(i.width for i in ims) + gap * (len(ims) - 1), ims[0].height), (13, 17, 23))
x = 0
for i in ims: out.paste(i, (x, 0)); x += i.width + gap
out.save(sys.argv[1], optimize=True)
