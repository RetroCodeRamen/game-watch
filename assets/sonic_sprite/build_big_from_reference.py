"""How sonic_big.txt was made from the reference drawing (reference_run.png, decoded to
reference_run.txt): run0 is the drawing itself; the other poses reuse its head and body.
Run from this folder; it writes sonic_big.txt and frames.png. Hand edits to sonic_big.txt
afterwards are fine (this script is a record, not part of the build)."""
import math, os
from PIL import Image, ImageDraw
W,H=32,32
ref=open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'reference_run.txt')).read().split('\n')[2:34]          # rows 2..33 -> 32 rows
ref=[(r+'.'*40)[:W] for r in ref]
G=[list(r) for r in ref]
for x in range(2,15): G[31][x]='.'                     # ground line under the back foot
RUN=[''.join(r) for r in G]
def grid(rows): return [list(r) for r in rows]
def txt(g): return [''.join(r) for r in g]
def paste(g,y0,rows,x0=0,clear=True):
    for j,r in enumerate(rows):
        for i,c in enumerate(r):
            y,x=y0+j,x0+i
            if 0<=y<H and 0<=x<W:
                if c=='#': g[y][x]='#'
                elif clear and c=='.': g[y][x]='.'
    return g
def clear(g,y0,y1,x0=0,x1=W):
    for y in range(y0,y1):
        for x in range(x0,x1): g[y][x]='.'
    return g
def shift(g,dx,dy,y0=0,y1=H):
    n=[['.']*W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            if g[y][x]=='#':
                if y0<=y<y1:
                    Y,X=y+dy,x+dx
                    if 0<=Y<H and 0<=X<W: n[Y][X]='#'
                else: n[y][x]='#'
    return n
UPPER=24          # rows 0..23 = head, arms, torso; 24.. = legs
frames={}
# run0: the reference stride
frames['run0']=txt(G)
# run1: passing pose - planted foot under the body, back foot tucked up behind
g=grid(RUN); clear(g,UPPER,H)
paste(g,UPPER,[
"..........##.......##...........",
".........##........##...........",
"....####.#.........#............",
"...#....##........##............",
"..#.....#.........#.####........",
"..#....#..........#.....##......",
"...####...........#.......#.....",
"..................#########.....",
])
frames['run1']=txt(shift(g,0,1,0,UPPER))      # body dips a row while the legs pass
# run2: stride again, arms swapped feel via a one-row bob up
frames['run2']=txt(G)
# run3: passing, other leg: knee forward, foot planted further back
g=grid(RUN); clear(g,UPPER,H)
paste(g,UPPER,[
"...........##.....##............",
"...........#......###...........",
"..........##.......#.##.........",
"..........#.........#..#........",
"..........#.####.....##.........",
".........##.....##..............",
".........#........#.............",
".........##########.............",
])
frames['run3']=txt(shift(g,0,1,0,UPPER))
# stand: legs straight, both shoes flat; arms in by the body
g=grid(RUN); clear(g,17,H,0,12); clear(g,16,H,24,W); clear(g,UPPER,H)
paste(g,17,[
"..........##",
".........###",
"........####",
"........#..#",
"........#..#",
".........##.",
],0,False)
paste(g,UPPER,[
"............##...##.............",
"............##...##.............",
"............##...##.............",
"............##...##.............",
"..........####...####...........",
".........#....#.#....##.........",
".........#.....##......#........",
".........#############.#........",
])
frames['stand']=txt(g)
b=grid(frames['stand'])
for y in range(8,13):
    for x in range(19,24): b[y][x]='#'
for x in range(19,24): b[11][x]='.'
frames['blink']=txt(b)
# fast: legs become a figure-8 blur, arms swept back
def fast(phase):
    g=grid(RUN); clear(g,UPPER-1,H); clear(g,16,UPPER,26,W)
    d=[['.']*W for _ in range(H)]
    for cx,cy,r in ((14,27,4.5),(20,27,4.5)):
        for a in range(0,360,6):
            t=math.radians(a+phase*30)
            x=int(round(cx+r*math.cos(t))); y=int(round(cy+r*0.85*math.sin(t)))
            if 0<=y<H and 0<=x<W and (a//30)%3!=phase%3: d[y][x]='#'
    for y in range(H):
        for x in range(W):
            if d[y][x]=='#': g[y][x]='#'
    for x in range(10,25): g[31][x]='#' if (x+phase)%3 else '.'
    return txt(g)
frames['fast0']=fast(0); frames['fast1']=fast(1)
# skid: lean back, front foot braced forward, dust behind
g=grid(RUN); g=shift(g,-2,0,0,UPPER); clear(g,UPPER,H)
paste(g,UPPER,[
"...........##.......##..........",
"...........##........##.........",
"..........##..........##........",
".........##............#.####...",
"........##.............##....#..",
".....####...............#....#..",
"....#....##..............####...",
"....#######.....................",
])
frames['skid']=txt(g)
# roll: a spiked ball, quills spiralling, four turns
def roll(k):
    g=[['.']*W for _ in range(H)]
    cx,cy,R=15.5,18.5,11.5
    for y in range(H):
        for x in range(W):
            dx,dy=x-cx,y-cy; r=math.hypot(dx,dy)
            if r<=R:
                a=math.degrees(math.atan2(dy,dx))-k*90
                s=(a+ r*14)%120          # three spiral quill seams
                if r>3 and 52<s<70: continue
                g[y][x]='#'
    return txt(g)
for k in range(4): frames['roll%d'%k]=roll(k)
order=['stand','blink','run0','run1','run2','run3','fast0','fast1','skid','roll0','roll1','roll2','roll3']
with open('sonic_big.txt','w') as f:
    for n in order:
        f.write('= %s\n'%n); f.write('\n'.join(frames[n])+'\n')
Z=6; img=Image.new('L',((W+2)*Z*7,(H+2)*Z*2),25); d=ImageDraw.Draw(img)
for i,n in enumerate(order):
    ox,oy=(i%7)*(W+2)*Z,(i//7)*(H+2)*Z
    for y,r in enumerate(frames[n]):
        for x,c in enumerate(r):
            if c=='#': d.rectangle([ox+x*Z,oy+y*Z,ox+x*Z+Z-2,oy+y*Z+Z-2],fill=240)
    d.text((ox+2,oy+(H)*Z+1),n,fill=160)
img.save('frames.png')
