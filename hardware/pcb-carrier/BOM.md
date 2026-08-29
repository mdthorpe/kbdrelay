# kbdrelay carrier board — BOM

Source of truth: `kbdrelay-carrier/kbdrelay-carrier.kicad_sch` (ERC clean, 0 errors).
Board of record: `kbdrelay-carrier/kbdrelay-carrier-v0.1.kicad_pcb` (22 footprints).
All discretes are **through-hole (THT)**. Quantities are per board.

## Active / modules

| Ref | Qty | Value | Footprint | Notes |
|-----|-----|-------|-----------|-------|
| U1, U2 | 2 | Waveshare ESP32-S3-Zero | `kbdrelay:ESP32-S3-Zero` (project library) | U1 = KBD (keyboard side), U2 = TGT (target side). Socketed, not soldered. 2×9 THT pads, 2.54 mm pitch, 15.24 mm rows. |
| D1 | 1 | **1N5817** (1 A / 20 V Schottky) | `Diode_THT:D_DO-201AD_P12.70mm_Horizontal` | Reverse-polarity protection. 1N5819 (40 V) is a drop-in. Footprint is intentionally the larger DO-201AD/12.70 mm so an **SB540/SB560** lower-Vf part fits with no board respin. **Band (cathode) faces the load.** |

## Passives

| Ref | Qty | Value | Footprint | Purpose |
|-----|-----|-------|-----------|---------|
| C1 | 1 | 470 µF / 16 V electrolytic | `Capacitor_THT:CP_Radial_D10.0mm_P5.00mm` | Bulk cap — the validated fix for keyboard inrush brownout (FR-021). **Polarised: stripe = negative = GND.** |
| C2 | 1 | 100 nF ceramic disc | `Capacitor_THT:C_Disc_D5.0mm_W2.5mm_P5.00mm` | HF decoupling alongside C1. |
| F1 | 1 | **Littelfuse 60R090** PPTC (0.90 A hold / 1.80 A trip, 60 V) | `kbdrelay:PPTC_Littelfuse_60R_P5.08mm` (project library) | Resettable fuse on keyboard VBUS (FR-022). Rₘᵢₙ 0.20 Ω / R₁ₘₐₓ 0.47 Ω. Straight in-line leads, 5.08 mm pitch, 0.51 mm lead dia, body 11.2 × 3.1 mm. **Not** the 0.5 A part — see "why 0.9 A" below. |
| R1–R5 | 5 | 2.2 kΩ | `Resistor_THT:R_Axial_DIN0207_..._P10.16mm_Horizontal` | SPI series resistors — one per line (SCK/MOSI/MISO/CS/DATA_READY), FR-023. |
| R6–R9 | 4 | 1 kΩ | same as above | UART TX/RX series, 2 per module (FR-026). |

*(D2 + R12 — the power-good LED — were removed by design amendment: FR-032 was
withdrawn. Bring-up uses a meter reading of the protected rail, which it already
required, and an LED only reports "something is live". Neither part is on the
shipped v0.1 board.)*

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

## Purchasing table (PCBWay assembly BOM)

**Machine-readable copy: [`kbdrelay-carrier-v0.1-BOM.csv`](kbdrelay-carrier-v0.1-BOM.csv)** — that
file is the one to upload. PCBWay accepts `.csv`/`.xls`/`.xlsx` only; markdown is
for humans. The CSV carries the full **turn-key** column set (Line#, Qty per P/N,
Reference Designator, Part Number, Part Description, Package, Type, Manufacturers
Name, Manufacturers Part Number, Distributors Part Number) per
<https://www.pcbway.com/assembly-file-requirements.html>, so it also satisfies the
shorter consigned/kitted set.

✅ **Every Digi-Key SKU below was resolved against the live Product Information V4
API on 2026-08-27** and is recorded with its package type, MOQ and stock at time
of lookup. All ten purchasable lines are **Active, MOQ 1, and in stock**.

⚠ **Three MPNs in the first draft were wrong.** The API caught all three; they were
plausible-looking guesses that would each have cost a re-order:

| Ref | Draft MPN | Problem | Corrected to |
|-----|-----------|---------|--------------|
| C1 | `UVR1C471MPD` | Real part, but **8.00 mm dia / 3.50 mm lead spacing** — does not fit the `D10.0mm_P5.00mm` footprint | `EEU-FC1C471` (Panasonic FC, **10.00 mm dia / 5.00 mm LS**, 12.5 mm H) |
| C1 | `UVR1C471MED` | **Does not exist.** Invented by guessing at Nichicon's size-code scheme | — as above |
| C2 | `K104K15X7RF5TL2` | Real part, but **2.50 mm lead spacing** — footprint wants 5.00 mm | `K104K15X7RF53H5` (same Vishay K-series, **5.00 mm LS**) |
| R6–R9 | `CFR-25JB-52-1K0` | **Does not exist.** Yageo's 1 k suffix is `-1K`, not `-1K0` | `CFR-25JB-52-1K` |

The C1/C2 errors are the interesting ones: both draft parts were *real, orderable
parts with the right electrical spec and the wrong mechanical footprint*. A BOM
checked only on capacitance and voltage would have passed them.

**Centroid file: not applicable.** PCBWay only wants pick-and-place data for
surface-mount parts, and this board has none. Send Gerbers + BOM + the assembly
drawing; state "100% through-hole, no SMT" on the order.

**Line 1 (the modules) and line 11 (their pin strips) must be consigned** —
Waveshare S3-Zeros aren't a distributor stock item, and line 11 solders to the
*module*, not the carrier. Line 12 is hardware: do not assemble.

Quantities are per board. Every board-mounted item is **through-hole**; there
are no surface-mount parts. Part numbers marked *(rep.)* are representative
jellybean MPNs — any equivalent from a reputable maker is fine. Part numbers in
**bold** are design-critical: substitute only per the notes in this file.

| Line# | Qty per P/N | Reference Designator | Part Number | Part Description | Package | Type |
|------:|------------:|----------------------|-------------|------------------|---------|------|
| Line# | Qty | Ref | Manufacturer | MPN | **Digi-Key SKU** | Pkg | Stock @ lookup |
|------:|----:|-----|--------------|-----|------------------|-----|---------------:|
| 1 | 2 | U1, U2 | Waveshare | ESP32-S3-Zero | *not stocked — buy direct* | — | — |
| 2 | 1 | D1 | Taiwan Semiconductor | **1N5817** | `1801-1N5817CT-ND` | Cut Tape | 3 080 |
| 3 | 1 | C1 | Panasonic Industry | EEU-FC1C471 | `P10248-ND` | Bulk | 15 960 |
| 4 | 1 | C2 | Vishay | K104K15X7RF53H5 | `BC3323-ND` | Bulk | 38 769 |
| 5 | 1 | F1 | Littelfuse | **60R090XU** | `F1923-ND` | Bulk | 1 048 |
| 6 | 5 | R1–R5 | YAGEO | CFR-25JB-52-2K2 | `2.2KQBK-ND` | Bulk | 69 020 |
| 7 | 4 | R6–R9 | YAGEO | CFR-25JB-52-1K | `1.0KQBK-ND` | Bulk | 241 553 |
| 8 | 1 | J3 | Sullins | PREC002SAAN-RC | `35-PREC002SAAN-RC-ND` | Bulk | 3 341 |
| 9 | 2 | J1, J2 | Sullins | PREC003SAAN-RC | `35-PREC003SAAN-RC-ND` | Bulk | 605 |
| 10 | 4 | U1/U2 sockets | Sullins | PPTC091LFBN-RC | `S7007-ND` | Tray | 4 548 |
| 11 | 4 | U1/U2 pins | Sullins | PREC009SAAN-RC | `35-PREC009SAAN-RC-ND` | Bulk | 853 |
| 12 | 4 | H1–H4 | — | M3×8 pan head + nut | *hardware, not assembled* | — | — |

Every line above is **MOQ 1** and **status Active**. D1 and C2/C1 notes:

- **D1** resolves to Taiwan Semiconductor's `1N5817` in DO-204AL/DO-41, the only
  exact-MPN part Digi-Key stocks under that number. It ships **Cut Tape**; the
  Tape & Reel variant (`1801-1N5817TR-ND`) is MOQ 5 000 and out of stock — do not
  order that one by mistake.
- **C1** `EEU-FC1C471` is 12.50 mm seated height. Same 10 mm diameter as assumed,
  so the **H3 clearance caveat below still applies unchanged**.

**Notes on the two module lines (10 and 11):** the S3-Zeros are socketed, not
soldered, so each module needs a 1×9 male strip in each of its two pad rows
(line 11) mating with a 1×9 female strip on the carrier (line 10) — 4 strips of
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
  meaningful power (worst case a 1 k UART series R at ~1 mW).
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
