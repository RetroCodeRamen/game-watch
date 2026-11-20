# python3 mont.py out.png prefix  — montage of prefix_*.pbm at 3× (lit pixels white on dark)
import sys, glob
from PIL import Image
fs = sorted(glob.glob(sys.argv[2] + '_*.pbm')); ims = [Image.open(f).convert('L') for f in fs]
cols = 3 if len(ims) >= 3 else len(ims); rows = (len(ims) + cols - 1) // cols
out = Image.new('L', (cols * 390, rows * 198), 60)
for i, im in enumerate(ims):
    im = im.point(lambda v: 255 - v).resize((384, 192), Image.NEAREST)
    out.paste(im, ((i % cols) * 390, (i // cols) * 198))
out.save(sys.argv[1])
