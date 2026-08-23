#!/usr/bin/env python3
"""Measure real copper gaps and trace widths on a board.

kicad-cli DRC only checks against the netclass rule; this reports the actual
geometry so a manufacturability target (NFR-007) can be verified directly.
Usage: <kicad python> audit_gaps.py [board.kicad_pcb]
"""
import pcbnew, itertools, math, collections, sys

P = sys.argv[1] if len(sys.argv) > 1 else "kbdrelay-carrier/kbdrelay-carrier.kicad_pcb"
b = pcbnew.LoadBoard(P)

pads = [(f.GetReference() + "." + p.GetNumber(), p, p.GetNetname())
        for f in b.GetFootprints() for p in f.Pads()
        if p.GetAttribute() != pcbnew.PAD_ATTRIB_NPTH]
T = [(t.GetNetname(), pcbnew.ToMM(t.GetStart().x), pcbnew.ToMM(t.GetStart().y),
      pcbnew.ToMM(t.GetEnd().x), pcbnew.ToMM(t.GetEnd().y),
      pcbnew.ToMM(t.GetWidth()), t.GetLayer()) for t in b.GetTracks()]

def sd(ax, ay, bx, by, px, py):
    dx, dy = bx - ax, by - ay
    L = dx * dx + dy * dy
    t = 0 if L == 0 else max(0, min(1, ((px - ax) * dx + (py - ay) * dy) / L))
    return math.hypot(ax + t * dx - px, ay + t * dy - py)

def ss(a, c):
    return min(sd(a[1], a[2], a[3], a[4], c[1], c[2]), sd(a[1], a[2], a[3], a[4], c[3], c[4]),
               sd(c[1], c[2], c[3], c[4], a[1], a[2]), sd(c[1], c[2], c[3], c[4], a[3], a[4]))

def pg(pa, pb):
    sa, sb = pa.GetEffectiveShape(pcbnew.B_Cu), pb.GetEffectiveShape(pcbnew.B_Cu)
    hi = pcbnew.FromMM(1.5)
    if not sa.Collide(sb, hi):
        return None
    lo = 0
    for _ in range(20):
        m = (lo + hi) // 2
        if sa.Collide(sb, m):
            hi = m
        else:
            lo = m
    return pcbnew.ToMM(hi)

g = []
for (na, pa, ta), (nb, pb, tb) in itertools.combinations(pads, 2):
    if ta == tb:
        continue
    if math.hypot(pa.GetPosition().x - pb.GetPosition().x,
                  pa.GetPosition().y - pb.GetPosition().y) > pcbnew.FromMM(6):
        continue
    d = pg(pa, pb)
    if d is not None:
        g.append((d, "pad-pad", na + " / " + nb))
for t in T:
    for nm, p, net in pads:
        if net == t[0]:
            continue
        sz = p.GetSize()
        r = max(pcbnew.ToMM(sz.x), pcbnew.ToMM(sz.y)) / 2
        d = sd(t[1], t[2], t[3], t[4], pcbnew.ToMM(p.GetPosition().x),
               pcbnew.ToMM(p.GetPosition().y)) - t[5] / 2 - r
        if d < 1.5:
            g.append((d, "trace-pad", "%s / %s" % (t[0], nm)))
for a, c in itertools.combinations(T, 2):
    if a[0] == c[0] or a[6] != c[6]:
        continue
    d = ss(a, c) - a[5] / 2 - c[5] / 2
    if d < 1.5:
        g.append((d, "trace-trace", a[0] + " / " + c[0]))
g.sort()

print("board: %s" % P)
ex = [d for d in b.GetDrawings() if d.GetLayer() == pcbnew.Edge_Cuts]
bb = ex[0].GetBoundingBox()
for d in ex[1:]:
    bb.Merge(d.GetBoundingBox())
print("outline: %.1f x %.1f mm" % (pcbnew.ToMM(bb.GetWidth()), pcbnew.ToMM(bb.GetHeight())))
refs = sorted(f.GetReference() for f in b.GetFootprints())
print("footprints (%d): %s" % (len(refs), ", ".join(refs)))
bcu = [t for t in T if t[6] == pcbnew.B_Cu]
fcu = [t for t in T if t[6] == pcbnew.F_Cu]
print("tracks: %d on B.Cu, %d on F.Cu (F.Cu = wire jumpers)" % (len(bcu), len(fcu)))
w = collections.Counter(round(t[5], 2) for t in bcu)
print("trace widths: %s" % ", ".join("%.2f mm x%d" % (k, v) for k, v in sorted(w.items())))
for z in b.Zones():
    print("zone '%s' local clearance: %.2f mm" % (z.GetNetname(), pcbnew.ToMM(z.GetLocalClearance())))
h = collections.Counter()
for d, k, _ in g:
    h["<0.60" if d < 0.60 else "0.60-0.70" if d < 0.70 else "0.70-0.80" if d < 0.80
      else "0.80-1.00" if d < 1.00 else ">=1.00"] += 1
print("\ngap distribution (features under 1.5 mm):")
for k in ["<0.60", "0.60-0.70", "0.70-0.80", "0.80-1.00", ">=1.00"]:
    print("   %-10s %4d" % (k, h[k]))
print("tightest 10:")
for d, k, w_ in g[:10]:
    print("   %.3f  %-11s %s" % (d, k, w_))
