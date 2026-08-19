# Pcb Carrier Board Design

Spec: `pcb-carrier-board`
Status: design-approved
Created: 2026-08-18
Brainstorm: `./brainstorm.md`
Requirements: `./requirements.md`

## Summary

A single-sided, through-hole "carrier" PCB that mechanically and electrically
joins two socketed Waveshare ESP32-S3-Zero modules (KBD + TGT) into the kbdrelay
bridge, replacing the breadboard harness. USB stays on each module's USB-C
(keyboard → KBD USB-C via adapter; TGT USB-C → target). The carrier provides:
the SPI interconnect (with series resistors), KBD's protected 5 V power path
(P-FET reverse polarity → bulk cap → polyfuse), strict isolation of the TGT 5 V
domain, a power-good LED, per-module UART headers (with series resistors), and a
reserved (unpopulated) soft-reset header. Firmware is unchanged from v1.

Design tool: KiCad (single copper layer routed on the bottom; components +
wire-jumper links on top). All discretes through-hole.

## Goals and Non-Goals

### Goals

- Reproduce verified v1 behavior with socketed modules, no hand-wiring
  (FR-001..FR-005).
- Enforce the FR5 power rules and back-power protection by construction
  (FR-010..FR-013, FR-020..FR-026).
- Be etchable at home on a single copper layer with THT parts (NFR-001,
  NFR-006), while giving the keyboard full 5 V (P-FET, no diode drop).

### Non-Goals

- Fab-house/production optimization, SMD, multi-layer, integrated USB
  connectors, enclosure — deferred to a future "product" spec.
- Any firmware change (soft-reset feature is only *reserved* in hardware).

## Architecture

```mermaid
flowchart LR
  KB[USB keyboard] -->|USB-A→C adapter| KBDUSB[KBD USB-C]
  subgraph CARRIER[Single-sided THT carrier PCB]
    PWR[2-pin 5V header] --> RP[P-FET reverse-polarity]
    RP --> BULK[470µF + 0.1µF]
    BULK --> PF[polyfuse ~0.5A]
    PF --> KBD5V[KBD module 5V pin]
    SPI[5x 2.2kΩ series R on SCK/MOSI/MISO/CS/DATA_READY]
    RST[reserved RST header - GP9 both, unpopulated]
    UART1[KBD UART hdr +1kΩ]
    UART2[TGT UART hdr +1kΩ]
    LED[power-good LED]
  end
  KBDUSB --- KBDMOD[KBD S3-Zero module]
  KBD5V --- KBDMOD
  KBDMOD <-->|GP4-8| SPI
  SPI <-->|GP4-8| TGTMOD[TGT S3-Zero module]
  TGTMOD --- TGTUSB[TGT USB-C]
  TGTUSB -->|USB-C→A| TARGET[Target machine]
  TARGET -.VBUS powers TGT.-> TGTMOD
  GND[(common ground)] --- CARRIER
```

- The carrier carries **power + SPI + GND + debug**, never USB data.
- **KBD** is powered by the carrier (from the 5 V header) and sources the
  keyboard's VBUS out its own USB-C. **TGT** is powered only by the target
  through its USB-C; the carrier does **not** connect to the TGT 5V pin.
- Both module USB-C ports face board edges for cabling.

## Schematic design (nets, values, candidate parts)

### Power path (KBD) — FR-010..FR-013, FR-020..FR-022

```
J_PWR(+5V) ──D──[ P-FET ]──S──┬── C_bulk(470µF) ──┬── F_poly(PPTC ~0.5A) ── KBD.5V
                gate            │   C_dec(0.1µF)   │
                 └─[100k]─ GND  │                  │
J_PWR(GND) ───────────────────┴── common GND ─────┴──────────────────── KBD.GND
```

- **Reverse polarity (P-FET ideal diode):** high-side P-channel MOSFET, **drain
  → +5 V input, source → load rail, gate → GND via 100 kΩ**. Correct polarity:
  body diode + Vgs≈−5 V turns the FET fully on (near-zero drop, keyboard gets
  full 5 V). Reversed input: body diode blocks, FET stays off → protected.
  Part: a **logic-level POWER P-channel MOSFET in TO-220 THT** (Vgs(th) ≈
  −1..−2 V, Rds(on) specified at Vgs=−4.5 V and ≤~100 mΩ, Id ≥1 A, Vds ≥−20 V),
  e.g. **Infineon IPPxxP03P4L** (OptiMOS logic-level) or **NDP6020P**.
  ⚠ **Do NOT use small-signal TO-92 P-FETs** (Supertex VP2106 / TP2104-class):
  their Rds is in the *ohms*, which drops >1 V and overheats at the ~0.3–0.4 A
  this FET carries (KBD module + keyboard). Symbol: `Q_PMOS_GDS` (TO-220 is
  G-D-S); footprint: `Package_TO_SOT_THT:TO-220-3_Vertical`. Exact part at BOM.
- **Bulk cap:** 470 µF (≥10 V) radial electrolytic + 0.1 µF ceramic, placed at
  the KBD 5V pin — the validated fix for keyboard inrush brownout.
- **Polyfuse:** radial THT PPTC, hold ≈0.5 A / trip ≈1 A, in series to the KBD
  5V pin (protects the source/board on a keyboard or cable short; passes any
  normal keyboard ≥250 mA).
- **TGT power:** the TGT module's 5V pin is **left unconnected** on the carrier.
  Only GND + SPI reach TGT → FR5.1/FR5.3 by construction.

### SPI interconnect — FR-002, FR-023

- Straight pin-to-pin between modules on the locked map, each line through a
  **2.2 kΩ series resistor** placed near the driving module:

  | Signal | KBD pin | TGT pin |
  |--------|---------|---------|
  | SCK    | GP4     | GP4     |
  | MOSI   | GP5     | GP5     |
  | MISO   | GP6     | GP6     |
  | CS     | GP7     | GP7     |
  | DATA_READY | GP8 | GP8     |

- Series R limits inter-module backfeed/latch-up and cures the power-up-order
  pre-charge; at ≤1 MHz the RC with short traces is negligible.

### Reserved soft-reset — FR-030 (reserve only)

- **GP9 on both modules** tied to a shared node, each via a **1 kΩ series R**
  (backfeed-safe), brought to an **unpopulated 2-pin header (`RST_BTN`, `GND`)**.
- A future button shorts the node to GND; firmware (later) enables internal
  pull-ups and calls `esp_restart()` on low → one button resets both modules.
  No component populated for v1; costs only board area.

### Debug UART — FR-026, FR-031

- Per module: **3-pin header (TX, RX, GND)**; **1 kΩ series R** on TX and RX so
  a plugged-in USB-UART adapter cannot back-power/sustain the module (the bench
  lesson). VCC intentionally absent from the header.

### Indication — FR-032

- One **power-good LED + ~1 kΩ resistor** across the protected 5 V rail.
- Per-role status remains on each module's onboard WS2812 (GPIO21).

## Connectors & pinouts (external interfaces)

- **J_PWR** — 2-pin (5V, GND) THT header; external 5 V source (user's choice).
- **Module sockets** — female 0.1" headers matching the S3-Zero (≈1×9 + 1×12
  per module); modules removable.
- **J_UART_KBD / J_UART_TGT** — 3-pin (TX/RX/GND) each.
- **J_RST** — 2-pin (RST_BTN/GND), unpopulated.
- **USB** — not on the carrier; each module's USB-C, edge-accessible.

## Error handling → Fault & protection behavior

- **Reversed 5 V input:** P-FET blocks; no current, no damage (FR-020).
- **Keyboard inrush:** bulk cap holds the rail; no brownout (FR-021).
- **Keyboard/cable short:** polyfuse trips, protecting source/board; resets when
  cleared (FR-022).
- **One module unpowered / any power-up order:** 2.2 kΩ series R limits backfeed
  so neither chip latches and an unpowered TGT still cold-starts cleanly when
  the target powers it (FR-023..FR-025).
- **Debug adapter attached:** 1 kΩ series on UART + no VCC pin prevents
  back-powering (FR-026).

## Security and Privacy → Target-hardware safety

- Not applicable in the software sense. The relevant safety property is
  **never sourcing power into the (possibly vintage) target**: guaranteed by
  leaving the TGT 5V pin unconnected and by the series-R'd SPI (only GND + weak,
  resistor-limited signals reach the TGT side). FR5.3.

## Testing Strategy

- **Bare-board checks:** visual/continuity — 5 V and TGT-5V domains isolated;
  GND common; SPI pin-to-pin; P-FET orientation; polyfuse in series.
- **Power-on (no modules):** apply 5 V, verify protected rail voltage ≈ input
  (P-FET low drop), power-good LED on; apply reversed 5 V, verify no rail.
- **Populated bring-up:** install modules + v1 firmware; reproduce AC1–AC5
  (typing, Caps/Num LED, hot-plug, watchdog) — these are the acceptance tests.
- **Fault tests:** high-inrush keyboard enumerates first try; brief keyboard-VBUS
  short trips the polyfuse; verify all power-up orders (esp. KBD-first).
- No automated tests (hardware); `bridge_proto` host tests remain green as the
  firmware is unchanged.

## Rollout and Migration → Fabrication & assembly plan

- **Single copper layer** (bottom) routed in KiCad; components + **wire-jumper
  links** on top; **no plated vias**. Target **≤ ~8 jumpers**, each listed in
  build notes (NFR-001, NFR-006).
- **Home-fab profile (starting targets, finalized in layout):** signal traces
  ~0.5 mm, power traces ~1.0 mm, clearance ~0.5 mm, drills ~0.9 mm with generous
  ~2 mm pads.
- **Outputs:** KiCad project, a 1:1 bottom-copper print for toner-transfer/home
  etch, and standard Gerbers for a PCB house.
- **BOM (all THT):** P-FET, 100 k, 470 µF, 0.1 µF, PPTC ~0.5 A, 5× 2.2 k,
  4× 1 k (UART) + 2× 1 k (reset series) , LED + ~1 k, J_PWR (1×2), 2× UART (1×3),
  J_RST (1×2, unpopulated), module socket headers. Board outline + 4× M3 holes.

## Requirements Traceability

| Requirement | Design Decision | Validation |
| ----------- | --------------- | ---------- |
| FR-001 | Two 0.1" module sockets (removable) | Bring-up: modules seat/run |
| FR-002 | SPI pins GP4–GP8 straight, series R | Continuity + typing works |
| FR-003 | Single common GND net | Continuity |
| FR-004 | No USB data on carrier | Layout review |
| FR-005 | Modules run v1 firmware | AC1–AC5 reproduced |
| FR-010/011 | 2-pin 5V header → protected → KBD 5V → keyboard VBUS | Meter + keyboard powers |
| FR-012/013 | TGT 5V pin unconnected; domains separate | Continuity (isolated) |
| FR-020 | P-FET reverse polarity | Reversed-input test |
| FR-021 | 470 µF + 0.1 µF bulk | High-inrush kb enumerates |
| FR-022 | ~0.5 A polyfuse on keyboard VBUS | Short test trips fuse |
| FR-023/024/025 | 2.2 kΩ SPI series R | Any power-up order works |
| FR-026 | UART 1 kΩ series, no VCC | Adapter can't back-power |
| FR-030 | Reserved GP9 RST header (unpop.) | Footprint present |
| FR-031 | Per-module UART header | Console works |
| FR-032 | Power-good LED | LED on with 5 V |
| FR-033 | USB-C edge access | Mechanical review |
| NFR-001/006 | Single-sided, jumpers, no vias | DRC + jumper list |
| NFR-002 | Cheap THT parts | BOM review |
| NFR-003 | Silkscreen labels | Layout review |
| NFR-004 | BOM + build notes | Docs present |
| NFR-005 | R/C values vs ≤1 MHz SPI | Typing reliable |

## Risks and Trade-offs

- **P-FET selection (THT logic-level):** smaller THT selection than SMD; a
  wrong (non-logic-level) part won't fully enhance at 5 V. Mitigation: specify
  Vgs(th) and Rds(on)@−4.5 V in the BOM; alternative was a Schottky (rejected —
  eats voltage margin).
- **Single-sided routing with two 21-pin modules + power:** may need several
  jumpers. Mitigation: place modules to keep SPI + power on the bottom layer;
  budget/document jumpers; the circuit is electrically simple.
- **No USB ESD on carrier:** data is on the modules; carrier can't add it.
  Accepted for the DIY board; revisit in the product version.
- **Keyboard current beyond polyfuse hold (RGB):** fuse trips (best-effort per
  FR5.2); documented.
