#!/usr/bin/env python3
"""Report every pad pair closer than a threshold, and what a bridge there costs.

For a hand-etched board the useful question is not "how wide are the gaps" but
"how many tight gaps are there, and which ones matter". Run with KiCad's
bundled python:
  .../Versions/Current/bin/python3 bridge_risk.py [--under=0.6]
"""
import pcbnew, itertools, math, sys

PCB = "kbdrelay-carrier/kbdrelay-carrier.kicad_pcb"
UNDER = float(next((a.split("=")[1] for a in sys.argv if a.startswith("--under=")), 0.6))
b = pcbnew.LoadBoard(PCB)

pads = []
for fp in b.GetFootprints():
    for p in fp.Pads():
        if p.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH:
            continue
        pads.append((fp.GetReference() + "." + p.GetNumber(), p, p.GetNetname()))

def gap(pa, pb):
    sa, sb = pa.GetEffectiveShape(pcbnew.B_Cu), pb.GetEffectiveShape(pcbnew.B_Cu)
    hi = pcbnew.FromMM(1.2)
    if not sa.Collide(sb, hi):
        return None
    lo = 0
    for _ in range(22):
        mid = (lo + hi) // 2
        if sa.Collide(sb, mid):
            hi = mid
        else:
            lo = mid
    return pcbnew.ToMM(hi)

def kind(n):
    if n.startswith("unconnected-"):
        return "FLOAT"
    return "GND" if n == "GND" else ("5V" if "5V" in n else "SIG")

def consequence(k1, k2):
    if "FLOAT" in (k1, k2):
        return "harmless - pin is unconnected on the carrier"
    if {k1, k2} == {"5V", "GND"}:
        return "*** DEAD SHORT of a 5 V rail ***"
    if {k1, k2} == {"SIG", "GND"}:
        return "signal pulled to GND - link dead"
    if k1 == k2 == "SIG":
        return "two signals shorted - link dead"
    return "review"

rows = []
for (na, pa, ta), (nb, pb, tb) in itertools.combinations(pads, 2):
    if ta == tb:
        continue
    if math.hypot(pa.GetPosition().x - pb.GetPosition().x,
                  pa.GetPosition().y - pb.GetPosition().y) > pcbnew.FromMM(5):
        continue
    g = gap(pa, pb)
    if g is not None and g < UNDER:
        rows.append((g, na, nb, ta, tb, consequence(kind(ta), kind(tb))))
rows.sort()
crit = [r for r in rows if "harmless" not in r[5]]
for g, na, nb, ta, tb, c in rows:
    print("%.3f mm  %-7s <-> %-7s  %s" % (g, na, nb, c))
print("\n%d pairs under %.2f mm; %d matter electrically:" % (len(rows), UNDER, len(crit)))
for g, na, nb, ta, tb, c in crit:
    print("   %s <-> %s   (%s / %s)" % (na, nb, ta, tb))
