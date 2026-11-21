#!/usr/bin/env python3
"""Apply the reaction-time checker's findings to sonic_watch/sonic_levels.h (rulebook 67, 124, 130,
131, 141). Reads 'FIX <act> <kind> <design x> <blind|fast>' lines on stdin (out/sight with FIXLIST=1).
  fast  -> add a rising ring arc that peaks over the hazard (the classic 'jump here' cue)
  blind -> move the hazard 48 design px further on, out of the landing spot; with --remove-blind,
           delete it instead (used on a second pass for ones that are still blind)
"""
import re, sys
P = 'sonic_watch/sonic_levels.h'
KIND = {0: ('L_SPIKE', 'L_SPIKES'), 2: ('L_CRAB',), 4: ('L_BEE',), 8: ('L_FLAME',), 10: ('L_SAW',)}
remove_blind = '--remove-blind' in sys.argv
s = open(P).read()
ENT = re.compile(r'\{(-?\d+), (-?\d+), (L_[A-Z]+), (-?\w+)\}')
def act_span(act):
    i = s.index('static const LvlObj %s_O[] = {' % act); return i, s.index('\n};', i)
done, arcs = set(), {}
for line in sys.stdin:
    p = line.split()
    if len(p) != 5 or p[0] != 'FIX': continue
    act, kind, dx, why = p[1], int(p[2]), int(p[3]), p[4]
    if kind not in KIND: continue
    a, b = act_span(act)
    best = None
    for m in ENT.finditer(s, a, b):
        x, h, k, arg = int(m.group(1)), int(m.group(2)), m.group(3), m.group(4)
        if k not in KIND[kind]: continue
        n = int(arg) if k == 'L_SPIKES' else 1
        lo, hi = (x - 8, x + 12 * n + 8) if kind == 0 else (x - 6, x + 6)
        if lo <= dx <= hi and (best is None or abs(x - dx) < abs(best[0] - dx)): best = (x, h, k, arg, m.start(), m.end(), n)
    if not best: print('no entry for', line.strip()); continue
    x, h, k, arg, ms, me, n = best
    if (act, ms) in done: continue
    done.add((act, ms))
    old = s[ms:me]
    if why == 'fast':
        centre = x + 6 * n if kind == 0 else x
        cnt = max(5, (12 * n + 36) // 12 + 1) | 1            # odd, so the middle ring is the peak
        arcs.setdefault(act, []).append('{%d, 34, L_ARC, %d}' % (centre - 6 * (cnt - 1), cnt))
    elif remove_blind:
        end = me + 1 if s[me:me + 1] == ',' else me               # take its comma with it
        s = s[:ms] + '/* removed: blind (rule 130) */' + s[end:]
    else:
        s = s[:ms] + '{%d, %s, %s, %s}' % (x + 48, h, k, arg) + s[me:]
    print(act, why, old, '->', 'arc' if why == 'fast' else ('removed' if remove_blind else '+48'))
for act, lst in arcs.items():
    a, b = act_span(act)
    s = s[:b] + '\n  // ring arcs: jump cues before hazards met at speed (rulebook 131, 141)\n  ' + ', '.join(lst) + ',' + s[b:]
open(P, 'w').write(s)
