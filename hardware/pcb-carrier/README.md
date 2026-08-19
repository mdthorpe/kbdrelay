# kbdrelay carrier board (hardware)

Single-sided, through-hole KiCad carrier PCB that hosts two socketed Waveshare
ESP32-S3-Zero modules (KBD + TGT) and joins them into the kbdrelay bridge.

**Spec (source of truth):** [`../../.specs/pcb-carrier-board/`](../../.specs/pcb-carrier-board/)
— requirements, design, and the ordered build tasks live there.

## Layout / conventions

```
hardware/pcb-carrier/
├── kbdrelay-carrier.kicad_pro / .kicad_sch / .kicad_pcb
├── libs/kbdrelay.kicad_sym      # project-local symbols (S3-Zero, etc.)
├── libs/kbdrelay.pretty/        # project-local footprints (.kicad_mod)
├── fab/                         # Gerbers, drill, 1:1 etch print (T-006)
├── BOM.md                       # bill of materials (T-003)
└── BUILD.md                     # fab + assembly + bring-up notes (T-006)
```

- Use **project-local libraries** (Project-scope in KiCad) so the design is
  self-contained for anyone who clones the repo.
- Board is **single copper layer** (bottom), all discretes **through-hole**,
  crossings via top-side **wire-jumper links** (target ≤ ~8, documented).
- Firmware is unchanged from v1 (same S3-Zero pinout: SPI GP4–GP8, USB on the
  module USB-C, UART0 TX/RX pads, WS2812 on GPIO21).
