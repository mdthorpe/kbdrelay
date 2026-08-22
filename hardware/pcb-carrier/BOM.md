# kbdrelay carrier board — BOM

Source of truth: `kbdrelay-carrier/kbdrelay-carrier.kicad_sch` (ERC clean, 0 errors).
All discretes are **through-hole (THT)**. Quantities are per board.

## Active / modules

| Ref | Qty | Value | Footprint | Notes |
|-----|-----|-------|-----------|-------|
| U1, U2 | 2 | Waveshare ESP32-S3-Zero | `kbdrelay:ESP32-S3-Zero` (project library) | U1 = KBD (keyboard side), U2 = TGT (target side). Socketed, not soldered. 2×9 THT pads, 2.54 mm pitch, 15.24 mm rows. |
| D1 | 1 | **1N5817** (1 A / 20 V Schottky) | `Diode_THT:D_DO-201AD_P12.70mm_Horizontal` | Reverse-polarity protection. 1N5819 (40 V) is a drop-in. Footprint is intentionally the larger DO-201AD/12.70 mm so an **SB540/SB560** lower-Vf part fits with no board respin. **Band (cathode) faces the load.** |
| D2 | 1 | LED, any colour | `LED_THT:LED_D5.0mm` | Power-good indicator on the fused KBD rail. |

## Passives

| Ref | Qty | Value | Footprint | Purpose |
|-----|-----|-------|-----------|---------|
| C1 | 1 | 470 µF / 16 V electrolytic | `Capacitor_THT:CP_Radial_D10.0mm_P5.00mm` | Bulk cap — the validated fix for keyboard inrush brownout (FR-021). **Polarised: stripe = negative = GND.** |
| C2 | 1 | 100 nF ceramic disc | `Capacitor_THT:C_Disc_D5.0mm_W2.5mm_P5.00mm` | HF decoupling alongside C1. |
| F1 | 1 | **Littelfuse 60R090** PPTC (0.90 A hold / 1.80 A trip, 60 V) | `kbdrelay:PPTC_Littelfuse_60R_P5.08mm` (project library) | Resettable fuse on keyboard VBUS (FR-022). Rₘᵢₙ 0.20 Ω / R₁ₘₐₓ 0.47 Ω. Straight in-line leads, 5.08 mm pitch, 0.51 mm lead dia, body 11.2 × 3.1 mm. **Not** the 0.5 A part — see "why 0.9 A" below. |
| R1–R5 | 5 | 2.2 kΩ | `Resistor_THT:R_Axial_DIN0207_..._P10.16mm_Horizontal` | SPI series resistors — one per line (SCK/MOSI/MISO/CS/DATA_READY), FR-023. |
| R6–R9 | 4 | 1 kΩ | same as above | UART TX/RX series, 2 per module (FR-026). |
| R12 | 1 | 1 kΩ | same as above | LED current limit (~3 mA at 5 V). |

*(R10/R11 and J4 — the shared GP9 soft-reset — were removed by design amendment.
FR-030 is met by the modules' own onboard BOOT/RESET buttons, which the layout
must leave accessible. GP9 is unconnected on both modules.)*

## Connectors

| Ref | Qty | Value | Footprint | Notes |
|-----|-----|-------|-----------|-------|
| J3 | 1 | 5V_IN (1×2) | `Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical` | Pin 1 = +5 V, pin 2 = GND. **KBD's only 5 V source.** |
| J1, J2 | 2 | UART_KBD / UART_TGT (1×3) | `..._PinHeader_1x03_P2.54mm_Vertical` | Pin1 = module TX, pin2 = module RX, pin3 = GND. **No VCC pin — deliberate** (FR-026). |
| — | 4 | **1×9** female headers, 2.54 mm | (part of the module footprint) | Two per module so the S3-Zeros are removable. Not a schematic symbol. Rows are 15.24 mm (0.6") apart. |
| H1–H4 | 4 | M3 mounting holes (3.2 mm, non-plated) | `MountingHole:MountingHole_3.2mm_M3` | Board corners, 3.5 mm in. Not a schematic symbol. H3 (bottom-left) sits close to C1's body — use a low-profile screw/washer. |

## Purchasing table

Quantities are per board. Every board-mounted item is **through-hole**; there
are no surface-mount parts. Part numbers marked *(rep.)* are representative
jellybean MPNs — any equivalent from a reputable maker is fine. Part numbers in
**bold** are design-critical: substitute only per the notes in this file.

| Line# | Qty per P/N | Reference Designator | Part Number | Part Description | Package | Type |
|------:|------------:|----------------------|-------------|------------------|---------|------|
| 1 | 2 | U1, U2 | Waveshare ESP32-S3-Zero | ESP32-S3FH4R2 module, USB-C, 2×9 castellated/THT pads | Module, 18.06 × 23.53 mm, 2×9 @ 2.54 mm, 15.24 mm rows | Thru-hole (socketed) |
| 2 | 1 | D1 | **1N5817** | Schottky rectifier, 1 A / 20 V, reverse-polarity protection | DO-41 (board pads are DO-201AD, 12.70 mm pitch) | Thru-hole |
| 3 | 1 | D2 | WP7113ID *(rep.)* | LED, 5 mm, any colour — power-good indicator | LED 5 mm radial, 2.54 mm pitch | Thru-hole |
| 4 | 1 | C1 | UVR1C471MPD *(rep.)* | Aluminium electrolytic capacitor, 470 µF / 16 V — inrush bulk | Radial, D10.0 mm, 5.00 mm pitch | Thru-hole |
| 5 | 1 | C2 | K104K15X7RF5TL2 *(rep.)* | Ceramic capacitor, 100 nF / 50 V, X7R — HF decoupling | Disc, D5.0 mm, 5.00 mm pitch | Thru-hole |
| 6 | 1 | F1 | **60R090XU** (Littelfuse) | PPTC resettable fuse, 0.90 A hold / 1.80 A trip, 60 V, R₁ₘₐₓ 0.47 Ω | Radial, 11.2 × 3.1 mm body, 5.08 mm pitch | Thru-hole |
| 7 | 5 | R1, R2, R3, R4, R5 | CFR-25JB-52-2K2 *(rep.)* | Resistor, 2.2 kΩ ±5 %, 1/4 W carbon film — SPI series | Axial DIN0207, 10.16 mm pitch | Thru-hole |
| 8 | 5 | R6, R7, R8, R9, R12 | CFR-25JB-52-1K0 *(rep.)* | Resistor, 1 kΩ ±5 %, 1/4 W carbon film — UART series (R6–R9), LED limit (R12) | Axial DIN0207, 10.16 mm pitch | Thru-hole |
| 9 | 1 | J3 | PH1-02-UA *(rep.)* | Pin header, 1×2, 2.54 mm, vertical — 5 V input | THT header, 1×2 @ 2.54 mm | Thru-hole |
| 10 | 2 | J1, J2 | PH1-03-UA *(rep.)* | Pin header, 1×3, 2.54 mm, vertical — debug UART (TX/RX/GND, no VCC) | THT header, 1×3 @ 2.54 mm | Thru-hole |
| 11 | 4 | (U1, U2 sockets) | PPTC091LFBN-RC *(rep.)* | Female header, 1×9, 2.54 mm — module sockets, 2 per module | THT socket strip, 1×9 @ 2.54 mm | Thru-hole |
| 12 | 4 | (U1, U2 pins) | PREC009SAAN-RC *(rep.)* | Male pin header, 1×9, 2.54 mm — soldered into the modules' pad rows | THT header strip, 1×9 @ 2.54 mm | Thru-hole |
| 13 | 4 | H1, H2, H3, H4 | M3×8 pan head + nut *(rep.)* | Mounting hardware, M3 — board corner holes (3.2 mm NPTH) | M3 screw / nut | Hardware (not soldered) |

**Notes on the two module lines (11 and 12):** the S3-Zeros are socketed, not
soldered, so each module needs a 1×9 male strip in each of its two pad rows
(line 12) mating with a 1×9 female strip on the carrier (line 11) — 4 strips of
each per board. Board holes are drilled 1.0 mm so a 0.64 mm square pin
(0.905 mm diagonal) passes.

**H3 caveat:** the bottom-left M3 hole sits close to C1's body — use a
low-profile screw head or a washer-free fixing there.

## Library provenance

The ESP32-S3-Zero symbol and footprint come from
<https://github.com/jtomka/kicad-esp32-s3-zero> and are vendored into the project
at `kbdrelay-carrier/symbols/` and `kbdrelay-carrier/footprints/kbdrelay.pretty/`,
registered through the project `sym-lib-table` / `fp-lib-table` with `${KIPRJMOD}`
so the repo is self-contained.

⚠ That upstream repo contains **no LICENSE file**, so its redistribution terms are
unstated. Fine for private use; clarify with the author before publishing the
board files.

## Ordering notes

- **Cheapest-that-meets-spec** per the project constitution: every part here is a
  jellybean — an industry-standard, second-sourced commodity part that many
  vendors make interchangeably, so it's always in stock and costs pennies.
- Resistors: 1/4 W carbon film is fine everywhere. Nothing here dissipates
  meaningful power (worst case R12 at ~9 mW).
- D1 alternates, in order of preference if VBUS headroom is tight:
  **SB560 > SB540 > 1N5817 > 1N5819**. Higher-current Schottkys have a *lower*
  forward drop at our ~0.3 A, which is exactly what we want.

## Why 0.9 A and not 0.5 A (F1)

A 0.5 A PPTC looks like the obvious pick for a 500 mA USB port. It isn't, for two
reasons that only show up in the datasheet's fine print:

- **Hold current derates with temperature** — 83 % at 40 °C, 76 % at 50 °C. A
  0.5 A part holds only ~0.42 A in a warm enclosure. A USB device may legally
  draw **500 mA**, and the KBD module adds ~60–80 mA, so the legitimate load can
  reach **~0.58 A**. The 0.5 A part would nuisance-trip on a legal keyboard.
  0.90 A derates to ~0.75 A → **29 % margin**.
- **Resistance scales inversely with hold rating**, and that resistance sits in
  series with keyboard VBUS. 60R050 is R₁ₘₐₓ **1.17 Ω**; 60R090 is **0.47 Ω**.

Not taken higher: at 60R110's 2.2 A trip, a typical 5 V brick current-limits
before the fuse acts, moving protection from the fuse to the supply.

## ⚠ Power-budget check — measure this at bring-up

D1's forward drop and F1's resistance both eat keyboard VBUS. USB's floor is
**4.40 V**. Worst case uses R₁ₘₐₓ — which is the honest long-term figure, since a
PPTC's resistance rises with self-heating and drifts up after trip events.

**Typical keyboard (~0.3 A total):**

| Element | Best | Worst |
|---------|------|-------|
| D1 1N5817 Vf | 0.28 V | 0.35 V |
| F1 60R090 (R × 0.3 A) | 0.06 V | 0.14 V |
| **Total** | 0.34 V | 0.49 V |
| **VBUS from 5.00 V** | 4.66 V ✓ | **4.51 V ✓** |

**Max-draw keyboard (500 mA + module ≈ 0.58 A):**

| Element | Best | Worst |
|---------|------|-------|
| D1 1N5817 Vf | 0.33 V | 0.40 V |
| F1 60R090 (R × 0.58 A) | 0.12 V | 0.27 V |
| **Total** | 0.45 V | 0.67 V |
| **VBUS from 5.00 V** | 4.55 V ✓ | **4.33 V ✗** |
| **VBUS from 5.25 V** | 4.80 V ✓ | 4.58 V ✓ |

**Conclusion:** with 60R090 a plain 5.00 V supply is fine for ordinary keyboards.
A **max-draw keyboard on worst-case parts still dips below 4.40 V**, so
**feeding J3 at 5.1–5.25 V remains the recommendation** — it is now insurance
rather than a hard requirement.

If a specific keyboard misbehaves, the remaining lever is D1: an **SB540/SB560**
fits the same pads and gives back ~0.1 V.

**Either way: measure loaded keyboard VBUS at bring-up (T-007/T-008). It must
read ≥4.40 V.**
