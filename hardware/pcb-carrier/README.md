# kbdrelay carrier board (hardware)

Single-sided, through-hole KiCad carrier PCB that hosts two socketed Waveshare
ESP32-S3-Zero modules (KBD + TGT) and joins them into the kbdrelay bridge.

**v0.1 is shipped and hardware-verified** (tag `v0.1-board`). The
`pcb-carrier-board` spec is **closed as the v0.1 record** — do not amend it.
New work happens in the v0.2 spec.

| What | Where |
|------|-------|
| **Board of record (v0.1, as etched)** | `kbdrelay-carrier/kbdrelay-carrier-v0.1.kicad_pcb` — hand-routed, 0 DRC errors, zero jumpers |
| Schematic (shared by both layouts) | `kbdrelay-carrier/kbdrelay-carrier.kicad_sch` — ERC clean |
| Superseded scripted layout | `kbdrelay-carrier/kbdrelay-carrier.kicad_pcb` — ⚠ 4 DRC errors, still has the dropped LED. **Never export fab output from it.** |
| v0.1 retrospective | [`RETRO-v0.1.md`](./RETRO-v0.1.md) |
| v0.1 spec (closed) | [`../../.specs/pcb-carrier-board/`](../../.specs/pcb-carrier-board/) |

## Layout / conventions

```
hardware/pcb-carrier/
├── kbdrelay-carrier/
│   ├── kbdrelay-carrier.kicad_sch          # the schematic (one, shared)
│   ├── kbdrelay-carrier-v0.1.kicad_pcb     # BOARD OF RECORD (hand-routed)
│   ├── kbdrelay-carrier-v0.1.kicad_pro
│   ├── kbdrelay-carrier.kicad_pcb/.kicad_pro  # superseded scripted layout
│   ├── symbols/kbdrelay.kicad_sym          # project-local symbols (S3-Zero)
│   ├── footprints/kbdrelay.pretty/         # project-local footprints (.kicad_mod)
│   └── sym-lib-table / fp-lib-table        # register the above via ${KIPRJMOD}
├── fab/gerbers/                            # kbdrelay-carrier-v0.1-*.gbr (as etched)
├── place.py / route.py                     # scripted layout (superseded by hand route)
├── bridge_risk.py / audit_gaps.py          # etch-gap audits
├── make_lcd_art.sh                         # LCD photoresist artwork
├── BOM.md                                 # bill of materials
├── BUILD.md                               # fab + assembly + bring-up notes
└── RETRO-v0.1.md                          # what v0.1 taught us
```

- Use **project-local libraries** (Project-scope in KiCad) so the design is
  self-contained for anyone who clones the repo.
- Board is **single copper layer** (bottom), all discretes **through-hole**.
  Jumper-link budget was ≤ ~8; **v0.1 shipped with zero** — the hand route needed
  none, at wider traces (1.0 mm) than the script ever managed.
- Firmware is unchanged from v1 (same S3-Zero pinout: SPI GP4–GP8, USB on the
  module USB-C, UART0 TX/RX pads, WS2812 on GPIO21).
