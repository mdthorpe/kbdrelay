#!/bin/sh
# Regenerate the LCD photoresist exposure artwork (Elegoo Mars 4 Ultra:
# 8520 x 4320 px over 153.36 x 77.76 mm = exactly 18.0 um/px).
#
# White = copper (negative-acting dry film). Pad centres stay solid copper
# (--drill-shape-opt 0); KiCad's default would etch them out.
set -e
K=/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli
P=kbdrelay-carrier/kbdrelay-carrier.kicad_pcb
OUT=fab/lcd
# board origin on the A4 sheet, in px at 18 um: (104.5, 77.5) mm; board 88 x 55 mm
X0=5806; Y0=4306; W=4889; H=3056; CW=8520; CH=4320
mkdir -p "$OUT"
$K pcb export svg --layers "B.Cu" --exclude-drawing-sheet --black-and-white \
   --drill-shape-opt 0 -o /tmp/kbd_bcu.svg "$P"
rsvg-convert -w 16500 -h 11667 -b white /tmp/kbd_bcu.svg -o /tmp/kbd_bcu.png
magick /tmp/kbd_bcu.png -crop ${W}x${H}+${X0}+${Y0} +repage -colorspace Gray \
   -negate -threshold 50% -background black -gravity center -extent ${CW}x${CH} \
   -depth 8 PNG8:"$OUT/kbdrelay-carrier_B.Cu_LCD_${CW}x${CH}.png"
magick "$OUT/kbdrelay-carrier_B.Cu_LCD_${CW}x${CH}.png" -flop \
   "$OUT/kbdrelay-carrier_B.Cu_LCD_${CW}x${CH}_MIRRORED.png"
magick /tmp/kbd_bcu.png -crop ${W}x${H}+${X0}+${Y0} +repage -colorspace Gray \
   -negate -threshold 50% -depth 8 PNG8:"$OUT/kbdrelay-carrier_B.Cu_boardonly_${W}x${H}.png"
echo "wrote $OUT"
