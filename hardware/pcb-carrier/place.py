#!/usr/bin/env python3
"""Placement / board-outline / silkscreen generator for the kbdrelay carrier.

Run with KiCad's bundled python, then run route.py:
  .../Versions/Current/bin/python3 place.py && .../python3 route.py

Everything is expressed in BOARD-LOCAL coordinates (0,0 = top-left board
corner). The board is then placed centred on the A4 drawing sheet, so the
title block frame surrounds it instead of the board sitting at the sheet
origin.
"""
import pcbnew

PCB = "kbdrelay-carrier/kbdrelay-carrier.kicad_pcb"
BW, BH = 88.0, 55.0                  # board size (was 120 x 55)
PAPER_W, PAPER_H = 297.0, 210.0      # A4
OX, OY = round((PAPER_W - BW) / 2, 2), round((PAPER_H - BH) / 2, 2)

# ref -> (x, y, rotation) in board-local mm
PLACEMENT = {
    # --- modules: USB-C overhang lands exactly on the short edges (FR-033)
    "U1": (1.5, 36.53, 90),          # KBD, left edge
    "U2": (BW - 1.5, 18.47, 270),    # TGT, right edge
    # --- power path: J3 -> D1 -> C1/C2 -> F1 -> U1.VDD (bottom left)
    "F1": (6.0, 42.0, 180),
    "D1": (16.0, 42.0, 0),
    "C1": (8.0, 49.0, 0),
    "C2": (20.0, 49.0, 0),
    "J3": (33.0, 42.0, 0),      # 1xNN vertical headers: pads already run along +Y
    "D2": (31.46, 49.0, 180),
    "R12": (34.0, 49.0, 0),
    # --- KBD UART header (top left)
    "R6": (8.0, 6.0, 0),
    "R7": (8.0, 11.0, 0),
    "J1": (21.0, 6.0, 0),
    # --- SPI series resistors: one column, five rows (was two columns)
    "R1": (45.0, 34.0, 0),           # SCK   GP4
    "R2": (45.0, 29.0, 0),           # MOSI  GP5
    "R3": (45.0, 24.0, 0),           # MISO  GP6
    "R4": (45.0, 19.0, 0),           # CS    GP7
    "R5": (45.0, 14.0, 0),           # DRDY  GP8
    # --- TGT UART header (bottom right, mirrors the KBD side)
    "J2": (60.0, 42.0, 0),
    "R8": (74.16, 42.0, 180),
    "R9": (74.16, 47.0, 180),
}

HOLES = [(3.5, 3.5), (BW - 3.5, 3.5), (3.5, BH - 3.5), (BW - 3.5, BH - 3.5)]

SILK = [  # (text, x, y, size, layer)
    ("KBD", 13.0, 15.0, 2.0, "F.SilkS"),
    ("TGT", 74.0, 39.5, 2.0, "F.SilkS"),
    ("5V IN", 30.0, 39.0, 1.4, "F.SilkS"),
    ("+", 36.0, 42.0, 1.4, "F.SilkS"),
    ("G", 36.0, 44.54, 1.4, "F.SilkS"),
    ("TX", 24.4, 6.0, 1.2, "F.SilkS"),
    ("RX", 24.4, 8.54, 1.2, "F.SilkS"),
    ("G", 24.4, 11.08, 1.2, "F.SilkS"),
    ("TX", 57.2, 42.0, 1.2, "F.SilkS"),
    ("RX", 57.2, 44.54, 1.2, "F.SilkS"),
    ("G", 57.2, 47.08, 1.2, "F.SilkS"),
    ("USE MODULE BOOT+RST", 33.0, 4.0, 1.2, "F.SilkS"),
    ("kbdrelay carrier v0.1", 22.0, 53.5, 1.2, "F.SilkS"),
    ("BOTTOM", 44.0, 52.5, 2.0, "B.Cu"),   # reads correctly from the copper side
]

def strip_blocks(path):
    """Remove old outline/text/mounting-hole blocks textually. pcbnew's
    board.Remove() corrupts the SWIG proxy registry, so edit the file first."""
    txt = open(path).read()
    out, i, n = [], 0, len(txt)
    while i < n:
        hit = None
        for kw in ("\n\t(gr_line", "\n\t(gr_rect", "\n\t(gr_poly", "\n\t(gr_text",
                   "\n\t(footprint"):
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
            block = txt[i:j + 1]
            if hit == "\n\t(footprint" and "MountingHole" not in block:
                out.append(block)      # keep real components
            i = j + 1
            continue
        out.append(txt[i])
        i += 1
    open(path, "w").write("".join(out))

strip_blocks(PCB)
board = pcbnew.LoadBoard(PCB)
mm = pcbnew.FromMM
LAYER = {"F.SilkS": pcbnew.F_SilkS, "B.Cu": pcbnew.B_Cu}

# ------------------------------------------------------------------ footprints
seen = set()
for fp in board.GetFootprints():
    ref = fp.GetReference()
    if ref in PLACEMENT:
        x, y, rot = PLACEMENT[ref]
        fp.SetPosition(pcbnew.VECTOR2I(mm(x + OX), mm(y + OY)))
        fp.SetOrientationDegrees(rot)
        seen.add(ref)
missing = set(PLACEMENT) - seen
if missing:
    raise SystemExit("footprints not on board: %s" % sorted(missing))

# ------------------------------------------------------------------ board edge
pts = [(0, 0), (BW, 0), (BW, BH), (0, BH)]
for k in range(4):
    seg = pcbnew.PCB_SHAPE(board)
    seg.SetShape(pcbnew.SHAPE_T_SEGMENT)
    seg.SetStart(pcbnew.VECTOR2I(mm(pts[k][0] + OX), mm(pts[k][1] + OY)))
    seg.SetEnd(pcbnew.VECTOR2I(mm(pts[(k + 1) % 4][0] + OX), mm(pts[(k + 1) % 4][1] + OY)))
    seg.SetLayer(pcbnew.Edge_Cuts)
    seg.SetWidth(mm(0.1))
    board.Add(seg)

# ------------------------------------------------------- mounting holes (M3)
MHLIB = "/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints/MountingHole.pretty"
for n, (x, y) in enumerate(HOLES, 1):
    fp = pcbnew.FootprintLoad(MHLIB, "MountingHole_3.2mm_M3")
    if fp is None:
        raise SystemExit("mounting hole footprint not found")
    fp.SetPosition(pcbnew.VECTOR2I(mm(x + OX), mm(y + OY)))
    fp.SetReference("H%d" % n)
    fp.Reference().SetVisible(False)
    board.Add(fp)

# ------------------------------------------------------------------ silkscreen
for (txt, x, y, size, layer) in SILK:
    t = pcbnew.PCB_TEXT(board)
    t.SetText(txt)
    t.SetPosition(pcbnew.VECTOR2I(mm(x + OX), mm(y + OY)))
    t.SetLayer(LAYER[layer])
    t.SetTextSize(pcbnew.VECTOR2I(mm(size * 0.8), mm(size)))
    t.SetTextThickness(mm(size * 0.15))
    if layer == "B.Cu":
        t.SetMirrored(True)     # reads correctly when viewed from the copper side
    board.Add(t)

board.Save(PCB)
print("board %.0f x %.0f mm, origin (%.1f, %.1f) on A4" % (BW, BH, OX, OY))
print("placed %d footprints" % len(seen))
print("mounting holes: %s" % (HOLES,))
print("silk/copper text items: %d" % len(SILK))
