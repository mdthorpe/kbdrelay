#!/bin/sh
# Regenerate LCD photoresist exposure artwork (Elegoo Mars 4 Ultra:
# 8520 x 4320 px over 153.36 x 77.76 mm = exactly 18.0 um/px).
#
# White = copper (negative-acting dry film). Pad centres stay solid copper
# (--drill-shape-opt 0); KiCad's default would etch them out.
#
# Usage: ./make_lcd_art.sh [board.kicad_pcb]
set -e
K=/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli
KPY=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
P="${1:-kbdrelay-carrier/kbdrelay-carrier-v0.1.kicad_pcb}"
OUT=fab/lcd
PX=0.018; CW=8520; CH=4320
mkdir -p "$OUT"

# crop window is derived from the board's real Edge.Cuts box, not hardcoded
eval "$($KPY - "$P" <<'PY' 2>/dev/null | grep '^[XYWH]'
import pcbnew, sys
b = pcbnew.LoadBoard(sys.argv[1])
ex = [d for d in b.GetDrawings() if d.GetLayer() == pcbnew.Edge_Cuts]
bb = ex[0].GetBoundingBox()
for d in ex[1:]:
    bb.Merge(d.GetBoundingBox())
px = 0.018
print("X0=%d" % round(pcbnew.ToMM(bb.GetLeft()) / px))
print("Y0=%d" % round(pcbnew.ToMM(bb.GetTop()) / px))
print("W=%d" % round(pcbnew.ToMM(bb.GetWidth()) / px))
print("H=%d" % round(pcbnew.ToMM(bb.GetHeight()) / px))
PY
)"
echo "board crop: ${W}x${H} px at (${X0},${Y0})  [18 um/px]"

$K pcb export svg --layers "B.Cu" --exclude-drawing-sheet --black-and-white \
   --drill-shape-opt 0 -o /tmp/kbd_bcu.svg "$P" >/dev/null
rsvg-convert -w 16500 -h 11667 -b white /tmp/kbd_bcu.svg -o /tmp/kbd_bcu.png
magick /tmp/kbd_bcu.png -crop ${W}x${H}+${X0}+${Y0} +repage -colorspace Gray \
   -negate -threshold 50% -background black -gravity center -extent ${CW}x${CH} \
   -depth 8 PNG8:"$OUT/kbdrelay-carrier_B.Cu_LCD_${CW}x${CH}.png"
magick "$OUT/kbdrelay-carrier_B.Cu_LCD_${CW}x${CH}.png" -flop \
   "$OUT/kbdrelay-carrier_B.Cu_LCD_${CW}x${CH}_MIRRORED.png"
magick /tmp/kbd_bcu.png -crop ${W}x${H}+${X0}+${Y0} +repage -colorspace Gray \
   -negate -threshold 50% -depth 8 PNG8:"$OUT/kbdrelay-carrier_B.Cu_boardonly_${W}x${H}.png"
echo "wrote $OUT"
