#!/usr/bin/env python3
"""Single-sided (B.Cu) maze router for the kbdrelay carrier board.

Run with KiCad's bundled python:
  .../KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 route.py

- All signal/power routing on B.Cu (home-etch, single sided).
- GND is not routed with tracks: a B.Cu pour covers the board.
- Nets the router cannot complete on B.Cu become F.Cu segments, which on this
  board mean top-side wire jumpers (pad to pad).
- Randomized restarts; the attempt with the fewest jumpers (then shortest
  copper) wins. Deterministic via a fixed seed.
"""
import sys, math, heapq, random
import pcbnew

PCB = "kbdrelay-carrier/kbdrelay-carrier.kicad_pcb"
for a in sys.argv[1:]:
    if a.endswith(".kicad_pcb"):
        PCB = a
ATTEMPTS = int(next((a.split("=")[1] for a in sys.argv if a.startswith("--attempts=")), 40))
SEED = int(next((a.split("=")[1] for a in sys.argv if a.startswith("--seed=")), 7))

GRID = 0.2      # mm routing grid
CLR = float(next((a.split("=")[1] for a in sys.argv if a.startswith("--clearance=")), 0.5))
# Pour-to-everything gap. Independent of routing: raising it only makes the GND
# fill retreat, so it buys etch/exposure tolerance for free. Routing clearance
# (CLR) is capped near 0.5 mm because adjacent module pads are only 0.54 mm apart.
ZONE_CLR = float(next((a.split("=")[1] for a in sys.argv if a.startswith("--zone-clearance=")), 0.8))
EDGE = 1.45     # mm keep-out for grid cell centres (0.5 rule + widest trace/2 + margin)
W_SIG = 0.7
W_PWR = 1.5
# board extents are read from Edge.Cuts (the board no longer sits at 0,0)
POWER_NETS = {"/+5V_IN", "/+5V_PROT", "/+5V_KBD"}
# GND-only keep-clear channels, in BOARD-LOCAL mm: without them the pour cannot
# reach the cap grounds (C1.2 / C2.2) once the +5V_PROT run walls them off.
GND_CHANNELS_LOCAL = [(11.9, 48.5, 14.1, 54.0),    # C1.2 down to the bottom pour
                      (23.9, 48.5, 26.1, 54.0),    # C2.2 down to the bottom pour
                      (81.0, 1.5, 83.6, 20.0)]     # U2.2 up to the top pour
POUR_NET = "GND"

def strip_routing(path):
    """Textually remove existing tracks/vias/zones so the script is re-runnable.
    (pcbnew's board.Remove() corrupts the SWIG proxy registry, so we do it on
    the file instead of the in-memory board.)"""
    txt = open(path).read()
    out, i, n = [], 0, len(txt)
    while i < n:
        hit = None
        for kw in ("\n\t(segment", "\n\t(via", "\n\t(zone", "\n\t(arc"):
            if txt.startswith(kw, i) and txt[i + len(kw)] in " \n\t(":
                hit = kw
                break
        if hit:
            d, j = 0, i + 2
            while j < n:
                if txt[j] == "(":
                    d += 1
                elif txt[j] == ")":
                    d -= 1
                    if d == 0:
                        break
                elif txt[j] == '"':
                    j += 1
                    while txt[j] != '"' or txt[j - 1] == "\\":
                        j += 1
                j += 1
            i = j + 1
            continue
        out.append(txt[i])
        i += 1
    open(path, "w").write("".join(out))

strip_routing(PCB)
board = pcbnew.LoadBoard(PCB)
mm = pcbnew.FromMM

# ---------------------------------------------------------------- board extent
_ex = [d for d in board.GetDrawings() if d.GetLayer() == pcbnew.Edge_Cuts]
if not _ex:
    raise SystemExit("no Edge.Cuts outline")
_bb = _ex[0].GetBoundingBox()
for _d in _ex[1:]:
    _bb.Merge(_d.GetBoundingBox())
X0, Y0 = pcbnew.ToMM(_bb.GetLeft()), pcbnew.ToMM(_bb.GetTop())
X1, Y1 = pcbnew.ToMM(_bb.GetRight()), pcbnew.ToMM(_bb.GetBottom())
BX, BY = X1 - X0, Y1 - Y0
NX, NY = int(BX / GRID) + 1, int(BY / GRID) + 1
GND_CHANNELS = [(x0 + X0, y0 + Y0, x1 + X0, y1 + Y0) for (x0, y0, x1, y1) in GND_CHANNELS_LOCAL]
print("board %.1f x %.1f mm at (%.1f, %.1f); clearance %.2f mm, pour clearance %.2f mm"
      % (BX, BY, X0, Y0, CLR, ZONE_CLR))

def cx(i):
    return X0 + i * GRID

def cy(j):
    return Y0 + j * GRID

# ---------------------------------------------------------------- pad harvest
pads = []
for fp in board.GetFootprints():
    for p in fp.Pads():
        pos, sz = p.GetPosition(), p.GetSize()
        sx, sy = pcbnew.ToMM(sz.x), pcbnew.ToMM(sz.y)
        # rect / roundrect pads (pin-1 markers) reach sqrt(2)x further at the
        # corners than a circle of the same width -- use the circumscribed radius
        rect = p.GetShape() in (pcbnew.PAD_SHAPE_RECT, pcbnew.PAD_SHAPE_ROUNDRECT,
                                pcbnew.PAD_SHAPE_CHAMFERED_RECT)
        r = math.hypot(sx, sy) / 2.0 if rect else max(sx, sy) / 2.0
        pads.append(dict(name="%s.%s" % (fp.GetReference(), p.GetNumber()),
                         net=p.GetNetname(), nc=p.GetNetCode(),
                         x=pcbnew.ToMM(pos.x), y=pcbnew.ToMM(pos.y), r=r))

nets = {}
for p in pads:
    if p["net"] and not p["net"].startswith("unconnected-"):
        nets.setdefault(p["net"], []).append(p)
nets = dict((k, v) for k, v in nets.items() if len(v) > 1)
GND_PADS = [p for p in pads if p["net"] == POUR_NET]

def disc(x, y, rad):
    i0 = max(0, int((x - rad - X0) / GRID)); i1 = min(NX - 1, int((x + rad - X0) / GRID) + 1)
    j0 = max(0, int((y - rad - Y0) / GRID)); j1 = min(NY - 1, int((y + rad - Y0) / GRID) + 1)
    r2 = rad * rad
    out = []
    for i in range(i0, i1 + 1):
        dx = cx(i) - x
        for j in range(j0, j1 + 1):
            dy = cy(j) - y
            if dx * dx + dy * dy <= r2:
                out.append((i, j))
    return out

CORE = dict((p["name"], set(disc(p["x"], p["y"], max(p["r"] * 0.55, GRID)))) for p in pads)
CORE_OWNER = {}
for p in pads:
    for c in CORE[p["name"]]:
        CORE_OWNER[c] = (p["x"], p["y"])

def seg_pt_dist(a, b, p):
    ax, ay = a; bx, by = b; px, py = p
    dx, dy = bx - ax, by - ay
    L2 = dx * dx + dy * dy
    t = 0.0 if L2 == 0 else max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / L2))
    return math.hypot(ax + t * dx - px, ay + t * dy - py)

def snap(cell, endcell=None, prev=None, w=W_SIG, netname=None):
    """grid cell -> mm. Track ends land exactly on the pad centre so copper does
    not visibly stop short inside the pad -- but only when the resulting final
    segment still clears every foreign pad, since snapping moves geometry
    outside the routing grid model."""
    raw = (cx(cell[0]), cy(cell[1]))
    tgt = CORE_OWNER.get(endcell if endcell is not None else cell)
    if tgt is None:
        return raw
    if prev is None:
        return tgt
    for q in pads:
        if netname is not None and q["net"] == netname:
            continue
        if seg_pt_dist(prev, tgt, (q["x"], q["y"])) < q["r"] + CLR + w / 2.0:
            return raw
    return tgt
MARGIN = GRID * 0.85          # covers 45-deg corner cutting on the routing grid
WIDTHS = (W_SIG, W_PWR)
# one obstacle map per trace width: a 0.7 mm signal may pass where 1.5 mm cannot
PAD_HALO = dict((w, [(p, disc(p["x"], p["y"], p["r"] + CLR + w / 2.0 + MARGIN)) for p in pads])
                for w in WIDTHS)
def edge_cells(w):
    e = 0.5 + w / 2.0 + MARGIN
    return [(i, j) for i in range(NX) for j in range(NY)
            if cx(i) < X0 + e or cy(j) < Y0 + e or cx(i) > X1 - e or cy(j) > Y1 - e]
EDGE_CELLS = dict((w, edge_cells(w)) for w in WIDTHS)

BLOCK = -1
DIRS = [(1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0),
        (1, 1, 1.414), (1, -1, 1.414), (-1, 1, 1.414), (-1, -1, 1.414)]

# copper text on B.Cu ("BOTTOM" orientation check) is real copper: keep off it
BCU_TEXT = []
for d in board.GetDrawings():
    try:
        if d.GetLayer() == pcbnew.B_Cu and d.GetClass() in ("PCB_TEXT", "PTEXT"):
            bb = d.GetBoundingBox()
            BCU_TEXT.append((pcbnew.ToMM(bb.GetLeft()), pcbnew.ToMM(bb.GetTop()),
                             pcbnew.ToMM(bb.GetRight()), pcbnew.ToMM(bb.GetBottom())))
    except Exception:
        pass

def fresh_grid_w(w):
    g = [[0] * NY for _ in range(NX)]
    for i, j in EDGE_CELLS[w]:
        g[i][j] = BLOCK
    for p, cells in PAD_HALO[w]:
        nc = p["nc"] if (p["net"] and not p["net"].startswith("unconnected-")) else BLOCK
        for i, j in cells:
            v = g[i][j]
            if v == 0:
                g[i][j] = nc
            elif v != nc:
                g[i][j] = BLOCK
    for (x0, y0, x1, y1) in BCU_TEXT:
        pad = CLR + w / 2.0 + MARGIN
        for i in range(max(0, int((x0 - pad - X0) / GRID)), min(NX - 1, int((x1 + pad - X0) / GRID)) + 1):
            for j in range(max(0, int((y0 - pad - Y0) / GRID)), min(NY - 1, int((y1 + pad - Y0) / GRID)) + 1):
                g[i][j] = BLOCK
    return g

def fresh_grid():
    G = dict((w, fresh_grid_w(w)) for w in WIDTHS)
    gnd_nc = next((p["nc"] for p in pads if p["net"] == POUR_NET), 0)
    for (x0, y0, x1, y1) in GND_CHANNELS:
        # widen channels so a pour neck survives the larger zone clearance
        pad_extra = max(0.0, ZONE_CLR - 0.4)
        x0, x1 = x0 - pad_extra, x1 + pad_extra
        for i in range(max(0, int((x0 - X0) / GRID)), min(NX - 1, int((x1 - X0) / GRID)) + 1):
            for j in range(max(0, int((y0 - Y0) / GRID)), min(NY - 1, int((y1 - Y0) / GRID)) + 1):
                for w in WIDTHS:
                    if G[w][i][j] == 0:
                        G[w][i][j] = gnd_nc
    return G

def astar(g, starts, goals, nc, extra):
    def ok(i, j):
        if i < 0 or j < 0 or i >= NX or j >= NY:
            return False
        if (i, j) in extra:
            return True
        v = g[i][j]
        return v == 0 or v == nc
    goalset = set(goals)
    gx = sum(a for a, b in goalset) / float(len(goalset))
    gy = sum(b for a, b in goalset) / float(len(goalset))
    openq, best, came = [], {}, {}
    for s in starts:
        if ok(*s):
            heapq.heappush(openq, (math.hypot(s[0] - gx, s[1] - gy), 0.0, s, None))
    while openq:
        f, gc, cur, prev = heapq.heappop(openq)
        if cur in best and best[cur] <= gc:
            continue
        best[cur] = gc
        came[cur] = prev
        if cur in goalset:
            path = [cur]
            while came[path[-1]] is not None:
                path.append(came[path[-1]])
            path.reverse()
            return path
        i, j = cur
        for di, dj, c in DIRS:
            ni, nj = i + di, j + dj
            if not ok(ni, nj):
                continue
            if di and dj and not (ok(i + di, j) and ok(i, j + dj)):
                continue
            turn = 0.0
            if prev is not None and (i - prev[0], j - prev[1]) != (di, dj):
                turn = 0.9
            ng = gc + c + turn
            if ng < best.get((ni, nj), 1e18):
                heapq.heappush(openq, (ng + math.hypot(ni - gx, nj - gy), ng, (ni, nj), cur))
    return None

def commit(G, path, nc, w):
    for wq in WIDTHS:
        g = G[wq]
        rad = w / 2.0 + CLR + wq / 2.0 + MARGIN
        for (i, j) in path:
            for a, b in disc(cx(i), cy(j), rad):
                v = g[a][b]
                if v == 0:
                    g[a][b] = nc
                elif v != nc:
                    g[a][b] = BLOCK
        for (i, j) in path:
            g[i][j] = nc

def simplify(path):
    pts = [path[0]]
    for k in range(1, len(path) - 1):
        if (path[k][0] - path[k - 1][0], path[k][1] - path[k - 1][1]) != \
           (path[k + 1][0] - path[k][0], path[k + 1][1] - path[k][1]):
            pts.append(path[k])
    pts.append(path[-1])
    return pts

def attempt(order):
    G = fresh_grid()
    GF = fresh_grid_w(W_SIG)          # front side, used only for wire jumpers
    tracks, jumpers, length = [], [], 0.0
    for name in order:
        ps = nets[name]
        nc = ps[0]["nc"]
        w = W_PWR if name in POWER_NETS else W_SIG
        ps = sorted(ps, key=lambda p: (p["x"], p["y"]))
        connected = set(CORE[ps[0]["name"]])
        done = [ps[0]]
        for p in ps[1:]:
            goal = CORE[p["name"]]
            g = G[w]
            path = astar(g, connected, goal, nc, connected | goal) or \
                   astar(g, goal, connected, nc, connected | goal)
            if path is None and name == POUR_NET:
                connected |= goal
                done.append(p)
                continue
            if path is None:
                a = min(done, key=lambda q: math.hypot(q["x"] - p["x"], q["y"] - p["y"]))
                jp = astar(GF, CORE[a["name"]], goal, nc, CORE[a["name"]] | goal)
                if jp is None:
                    tracks.append(((a["x"], a["y"]), (p["x"], p["y"]), w, nc, pcbnew.F_Cu))
                else:
                    jpts = simplify(jp)
                    jxy = [snap(q) for q in jpts]
                    jxy[0] = snap(jpts[0], jp[0], jxy[1] if len(jxy) > 1 else None, w, name)
                    jxy[-1] = snap(jpts[-1], jp[-1], jxy[-2] if len(jxy) > 1 else None, w, name)
                    for k in range(len(jxy) - 1):
                        tracks.append((jxy[k], jxy[k + 1], w, nc, pcbnew.F_Cu))
                    rad = w / 2.0 + CLR + W_SIG / 2.0 + MARGIN
                    for (i, j) in jp:
                        for aa, bb in disc(cx(i), cy(j), rad):
                            GF[aa][bb] = BLOCK
                jumpers.append((name, a["name"], p["name"],
                                math.hypot(a["x"] - p["x"], a["y"] - p["y"])))
                connected |= goal
                done.append(p)
                continue
            pts = simplify(path)
            xy = [snap(q) for q in pts]
            xy[0] = snap(pts[0], path[0], xy[1] if len(xy) > 1 else None, w, name)
            xy[-1] = snap(pts[-1], path[-1], xy[-2] if len(xy) > 1 else None, w, name)
            for k in range(len(xy) - 1):
                s = xy[k]
                e = xy[k + 1]
                length += math.hypot(e[0] - s[0], e[1] - s[1])
                tracks.append((s, e, w, nc, pcbnew.B_Cu))
            commit(G, path, nc, w)
            connected |= set(path) | goal
            done.append(p)
    # ---- virtual pour: free cells become GND copper. Stitch any GND pad that
    #      the pour cannot reach, and weld pour islands that hold GND pads.
    g = G[W_PWR]

    def flood():
        comp = [[0] * NY for _ in range(NX)]
        cid = 0
        sizes = {}
        for i in range(NX):
            for j in range(NY):
                if g[i][j] != 0 or comp[i][j]:
                    continue
                cid += 1
                stack, n = [(i, j)], 0
                comp[i][j] = cid
                while stack:
                    a, b = stack.pop()
                    n += 1
                    for da, db in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        na, nb = a + da, b + db
                        if 0 <= na < NX and 0 <= nb < NY and g[na][nb] == 0 and not comp[na][nb]:
                            comp[na][nb] = cid
                            stack.append((na, nb))
                sizes[cid] = n
        return comp, sizes

    def pad_touch(p, comp):
        out = set()
        for i, j in disc(p["x"], p["y"], p["r"] + CLR + W_PWR / 2.0 + MARGIN + GRID * 2):
            if comp[i][j]:
                out.add(comp[i][j])
        return out

    gnd_nc = GND_PADS[0]["nc"] if GND_PADS else 0
    for _ in range(6):
        comp, sizes = flood()
        main = max(sizes, key=lambda c: sizes[c]) if sizes else 0
        stranded = [p for p in GND_PADS if main not in pad_touch(p, comp)]
        if not stranded:
            break
        p = stranded[0]
        goal = set()
        for i in range(NX):
            for j in range(NY):
                if comp[i][j] == main:
                    goal.add((i, j))
        start = CORE[p["name"]]
        path = astar(g, start, goal, gnd_nc, start)
        if path is None:
            break
        pts = simplify(path)
        xy = [snap(q) for q in pts]
        xy[0] = snap(pts[0], path[0], xy[1] if len(xy) > 1 else None, W_PWR, POUR_NET)
        for k in range(len(xy) - 1):
            s0 = xy[k]
            e0 = xy[k + 1]
            length += math.hypot(e0[0] - s0[0], e0[1] - s0[1])
            tracks.append((s0, e0, W_PWR, gnd_nc, pcbnew.B_Cu))
        commit(G, path, gnd_nc, W_PWR)

    return tracks, jumpers, length, G

names = list(nets.keys())
power = [n for n in names if n in POWER_NETS]
signal = [n for n in names if n not in POWER_NETS and n != POUR_NET]
random.seed(SEED)
best = None
for k in range(ATTEMPTS):
    order = list(power) + list(signal)
    if k:
        random.shuffle(order)
        order = [n for n in order if n in POWER_NETS] + [n for n in order if n not in POWER_NETS]

    res = attempt(order)
    key = (len(res[1]), res[2])
    if best is None or key < best[0]:
        best = (key, res, order)
        print("attempt %2d: jumpers=%d copper=%.0f mm  <-- best" % (k, len(res[1]), res[2]))
    else:
        print("attempt %2d: jumpers=%d copper=%.0f mm" % (k, len(res[1]), res[2]))
    if len(res[1]) == 0:
        break

tracks, jumpers, length, G = best[1]

# ------------------------------------------------------------------ write out
GND_NC = next((p["nc"] for p in pads if p["net"] == POUR_NET), 0)

def emit(tracks):
    """Write tracks + GND pour to the board, fill, save, and report pour islands."""
    strip_routing(PCB)
    bd = pcbnew.LoadBoard(PCB)
    for s0, e0, w, nc, layer in tracks:
        t = pcbnew.PCB_TRACK(bd)
        t.SetStart(pcbnew.VECTOR2I(mm(s0[0]), mm(s0[1])))
        t.SetEnd(pcbnew.VECTOR2I(mm(e0[0]), mm(e0[1])))
        t.SetWidth(mm(w))
        t.SetLayer(layer)
        t.SetNetCode(nc)
        bd.Add(t)
    if GND_NC:
        z = pcbnew.ZONE(bd)
        z.SetLayer(pcbnew.B_Cu)
        z.SetNetCode(GND_NC)
        z.SetLocalClearance(mm(ZONE_CLR))
        z.SetMinThickness(mm(0.3))
        z.SetThermalReliefGap(mm(0.4))
        z.SetThermalReliefSpokeWidth(mm(0.8))
        # solid pad connections: home-etched single-sided board, no plated
        # barrels, and thermal spokes were starving/isolating GND pads
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
        try:
            z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
        except AttributeError:
            pass
        o = z.Outline(); o.NewOutline()
        m = 0.3
        for (x, y) in [(X0 + m, Y0 + m), (X1 - m, Y0 + m), (X1 - m, Y1 - m), (X0 + m, Y1 - m)]:
            o.Append(mm(x), mm(y))
        bd.Add(z)
        pcbnew.ZONE_FILLER(bd).Fill(bd.Zones())
    bd.Save(PCB)

    gp = []
    for fp in bd.GetFootprints():
        for p in fp.Pads():
            if p.GetNetname() == POUR_NET:
                gp.append((fp.GetReference() + "." + p.GetNumber(), p.GetPosition()))
    islands = []
    for z in bd.Zones():
        fl = z.GetFilledPolysList(pcbnew.B_Cu)
        for i in range(fl.OutlineCount()):
            poly = pcbnew.SHAPE_POLY_SET()
            poly.AddOutline(fl.Outline(i))
            islands.append(dict(area=poly.Area() / 1e12, poly=poly,
                                pads=[n for n, pos in gp
                                      if poly.Contains(pcbnew.VECTOR2I(pos.x, pos.y))]))
    return islands

byname = dict((p["name"], p) for p in pads)
stitches = []
for it in range(5):
    islands = emit(tracks)
    if not islands:
        break
    main = max(range(len(islands)), key=lambda i: islands[i]["area"])
    stranded = [(i, n) for i, isl in enumerate(islands) if i != main for n in isl["pads"]]
    if not stranded:
        break
    print("pour repair pass %d: stranded GND pads %s" % (it, [n for _, n in stranded]))
    goal = set()
    mp = islands[main]["poly"]
    g = G[W_SIG]
    for i in range(NX):
        for j in range(NY):
            if g[i][j] == 0 and mp.Contains(pcbnew.VECTOR2I(mm(cx(i)), mm(cy(j)))):
                goal.add((i, j))
    progressed = False
    for _, nm in stranded:
        start = CORE[nm]
        path = astar(g, start, goal, GND_NC, start)
        if path is None:
            print("   !! could not stitch %s on B.Cu" % nm)
            continue
        pts = simplify(path)
        xy = [snap(q) for q in pts]
        xy[0] = snap(pts[0], path[0], xy[1] if len(xy) > 1 else None, W_SIG, POUR_NET)
        for k in range(len(xy) - 1):
            tracks.append((xy[k], xy[k + 1], W_SIG, GND_NC, pcbnew.B_Cu))
        commit(G, path, GND_NC, W_SIG)
        stitches.append(nm)
        progressed = True
    if not progressed:
        break

if stitches is not None and any(True for _ in []):
    pass
print("\nBEST: %d jumpers, %d track segments, GND stitches: %s"
      % (len(jumpers), len(tracks), stitches or "none"))
for j in jumpers:
    print("   JUMPER %-16s %s -> %s  (%.1f mm)" % j)
