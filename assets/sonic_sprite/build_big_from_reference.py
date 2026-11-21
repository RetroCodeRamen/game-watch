"""How sonic_big.txt was made from the reference sheet (reference_29.webp: Stand 1-2 and
Run 1-3, decoded cell by cell into reference_29.json). The sheet says 25 px; the drawings are
really 26-29 rows. Stand 1 is the standing frame, Stand 2's head gives the blink, the three
run frames cycle 1-2-3-2; the top-speed figure-8, skid and spin-ball are built around Run 1.
Run from this folder; it writes sonic_big.txt and frames.png. Hand edits to sonic_big.txt
afterwards are fine (this script is a record, not part of the build)."""
import json, math, os
from PIL import Image, ImageDraw
HERE = os.path.dirname(os.path.abspath(__file__))
R = json.load(open(os.path.join(HERE, 'reference_29.json')))
W, H = 24, 29
FACE = 22                      # every frame's face (top rows' rightmost pixel) lands on this column

def place(rows, face=FACE, top_rows=13):
    g = [['.'] * W for _ in range(H)]
    right = max(r.rfind('#') for r in rows[:top_rows])
    dx, dy = face - right, H - len(rows)
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            if c == '#' and 0 <= x + dx < W: g[y + dy][x + dx] = '#'
    return g
def txt(g): return [''.join(r) for r in g]

frames = {}
stand = place(R['stand1']); frames['stand'] = txt(stand)
s2 = place(R['stand2'])
blink = [r[:] for r in stand]
for y in range(0, 13): blink[y] = s2[y][:]           # Stand 2's head: eyes shut
frames['blink'] = txt(blink)
run = [place(R['run1']), place(R['run2']), place(R['run3'])]
for i, k in enumerate((0, 1, 2, 1)): frames['run%d' % i] = txt(run[k])
LEGS = 18                                            # rows 18.. are legs in Run 1
def fast(phase):
    g = [r[:] for r in run[0]]
    for y in range(LEGS, H):
        for x in range(W): g[y][x] = '.'
    for cx, cy, r in ((10, 24, 4.0), (15, 24, 4.0)):
        for a in range(0, 360, 6):
            t = math.radians(a + phase * 30)
            x = int(round(cx + r * math.cos(t))); y = int(round(cy + r * 0.9 * math.sin(t)))
            if 0 <= y < H and 0 <= x < W and (a // 30) % 3 != phase % 3: g[y][x] = '#'
    for x in range(6, 20): g[H - 1][x] = '#' if (x + phase) % 3 else '.'
    return txt(g)
frames['fast0'] = fast(0); frames['fast1'] = fast(1)
g = [['.'] * W for _ in range(H)]                    # skid: lean back, front foot braced
for y in range(LEGS):
    for x in range(W):
        if run[0][y][x] == '#' and x - 2 >= 0: g[y][x - 2] = '#'
for y, r in enumerate([
    "........##.....##.......",
    ".......##.......##......",
    "......##.........##.....",
    ".....##...........#.###.",
    "....##............##...#",
    "..###..............#...#",
    ".#...#..............###.",
    ".#...###................",
    ".######.................",
    "........................",
    "........................"][:H - LEGS]):
    for x, c in enumerate(r):
        if c == '#': g[LEGS + y][x] = '#'
frames['skid'] = txt(g)
def roll(k):                                         # spiked ball, three spiral seams
    g = [['.'] * W for _ in range(H)]
    cx, cy, rad = 11.5, 17.5, 10.0                   # bottom one row above the feet
    for y in range(H):
        for x in range(W):
            dx, dy = x - cx, y - cy; r = math.hypot(dx, dy)
            if r <= rad:
                a = math.degrees(math.atan2(dy, dx)) - k * 90
                if r > 2.5 and 52 < (a + r * 16) % 120 < 72: continue
                g[y][x] = '#'
    return txt(g)
for k in range(4): frames['roll%d' % k] = roll(k)
# hurt: knocked back - the standing frame leaning back, arms flung up, legs splayed
g = [['.'] * W for _ in range(H)]
for y in range(0, 20):
    lean = (20 - y) // 5                              # top of him tips back (to the left)
    for x in range(W):
        if stand[y][x] == '#' and 0 <= x - lean < W: g[y][x - lean] = '#'
for y, r in enumerate([                                 # arms thrown up and back, gloves open
    "..##..................##",
    ".#..#................#..",
    ".#..#...............#...",
    "..##.#.............#....",
    ".....#............#.....",
    "......#..........#......"]):
    for x, c in enumerate(r):
        if c == '#': g[8 + y][x] = '#'
for y, r in enumerate([                                 # legs kicked out front and back
    "........##....##........",
    ".......##......##.......",
    "......##........##......",
    ".....##..........##.....",
    "..####............####..",
    ".#...#............#...#.",
    ".#####............#####.",
    "........................",
    "........................"]):
    for x, c in enumerate(r):
        g[20 + y][x] = '#' if c == '#' else '.'
frames['hurt'] = txt(g)
order = ['stand', 'blink', 'run0', 'run1', 'run2', 'run3', 'fast0', 'fast1', 'skid', 'roll0', 'roll1', 'roll2', 'roll3', 'hurt']
with open(os.path.join(HERE, 'sonic_big.txt'), 'w') as f:
    for n in order: f.write('= %s\n' % n + '\n'.join(frames[n]) + '\n')
Z = 6; img = Image.new('L', ((W + 2) * Z * 7, (H + 3) * Z * 2), 25); d = ImageDraw.Draw(img)
for i, n in enumerate(order):
    ox, oy = (i % 7) * (W + 2) * Z, (i // 7) * (H + 3) * Z
    for y, r in enumerate(frames[n]):
        for x, c in enumerate(r):
            if c == '#': d.rectangle([ox + x * Z, oy + y * Z, ox + x * Z + Z - 2, oy + y * Z + Z - 2], fill=240)
    d.text((ox + 2, oy + H * Z + 2), n, fill=160)
img.save(os.path.join(HERE, 'frames.png'))
