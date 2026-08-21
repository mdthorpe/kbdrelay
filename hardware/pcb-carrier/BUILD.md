# kbdrelay carrier board — build & bring-up

**Status: draft.** Fab and assembly sections firm up when layout (T-004) and fab
outputs (T-006) are done. Bring-up procedure is already usable.

Parts list: [`BOM.md`](./BOM.md). Schematic: [`schematic-v1.pdf`](./schematic-v1.pdf).

---

## 0. What this board is

A single-sided, all-through-hole carrier that joins two socketed Waveshare
ESP32-S3-Zero modules into the kbdrelay bridge:

- **U1 = KBD** — keyboard plugs into *its* USB-C. Powered from the J3 5 V header.
- **U2 = TGT** — plugs into the target machine. Powered **only** by the target.
- The two 5 V domains never touch. **Ground is the only shared power net.**

## 1. Board conventions (single-sided)

- **Copper is on the bottom. Components sit on the top.** Leads drop through the
  holes; you solder on the underside.
- There are **no vias** — with one copper layer there is nothing to connect
  between layers. Trace crossings are handled by **top-side wire jumpers**.
- Both module footprints and all silkscreen are on the **front**; all routing is
  on **B.Cu**.

### Post-etch orientation check

A `BOTTOM` text label is placed on the copper layer. KiCad mirrors back-layer
text, so:

> Flip the etched board over and look at the copper. **If `BOTTOM` reads
> normally, the artwork orientation is correct.** If it reads backwards, the
> board is a mirror image — stop before drilling.

Secondary check: both USB-C notches (the 1.5 mm cut-outs in the module outlines)
must face **off the board edges** so cables can reach them (FR-033).

## 2. Fabrication

1. Print the 1:1 bottom-copper artwork (mirroring per your existing toner-transfer
   process) and verify scale with calipers against a known dimension before
   etching — module pad rows are exactly **15.24 mm** apart, pitch **2.54 mm**.
2. Etch, clean, and run the `BOTTOM` check above.
3. Drill:
   - **module pads: 1.0 mm** (sized for 0.64 mm square header pins, whose
     diagonal is 0.905 mm — a 0.762 mm hole will *not* take them)
   - other THT parts: per their library footprints
4. Cut the outline and drill the 4× M3 mounting holes.

## 3. Assembly order

Solder shortest/flattest first so the board sits flat on the bench.

1. Wire jumper links (top side) — do these before anything blocks access.
2. Resistors R1–R12 (all axial, lie flat).
3. Diode **D1 (1N5817)** — **band = cathode = toward the load / away from J3.**
   Getting this backwards means the board simply never powers up (which is the
   protection working, not a fault).
4. Polyfuse F1, ceramic C2.
5. Female headers for U1/U2 — tack one pin per strip, check they are square and
   flush, then solder the rest.
6. Electrolytic **C1 (470 µF)** — **polarised: the stripe is the negative leg,
   goes to GND.**
7. LED **D2** — longer leg is the anode, toward the 5 V rail.
8. Headers J1, J2, J3.

## 4. Bare-board bring-up (no modules) — T-007

Do all of this before a module ever goes near the board.

| # | Check | Expected |
|---|-------|----------|
| 1 | Continuity: GND across J1.3 / J2.3 / J3.2 / both module GND pads | connected |
| 2 | **Isolation: J3 5 V rail ↔ U2's 5V pad** | **open circuit — this is FR-013, the vintage-target safety property** |
| 3 | Continuity: each SPI pad pair through its 2.2 k | ≈2.2 kΩ, not 0 Ω |
| 4 | Apply **+5.0 V** to J3 | rail ≈ input − ~0.3 V (Schottky drop); power-good LED on |
| 5 | Apply **reversed** 5 V to J3 | **no rail, no heat, no damage** (FR-020) |
| 6 | Measure U1 5V pad | ≈4.6 V (see headroom note below) |

## 5. Populated bring-up + acceptance — T-008

1. Seat both modules (**check pin 1 orientation on each**), flash v1 firmware.
   There is no board-level reset: use each module's **own onboard BOOT/RESET
   buttons** for the download-mode sequence, or power-cycle. Confirm both
   modules' buttons are reachable with everything seated (FR-030).
2. Reproduce **AC1–AC5**: typing passes through; Caps/Num/Scroll-Lock LED state
   flows back TGT→KBD; keyboard hot-plug recovers; watchdog behaves.
3. Fault and ordering tests:
   - high-inrush keyboard enumerates on the **first** plug-in (that's what C1 is for)
   - brief keyboard-VBUS short trips F1, and it recovers when cleared
   - **every power-up order**, especially KBD-first → TGT must still cold-start
   - a powered USB-UART adapter on J1/J2 cannot back-power a module

### ⚠ Voltage headroom check (do this with the hungriest keyboard attached)

The 1N5817 drops ~0.3 V at 0.3 A, so a 5.00 V supply lands the keyboard at
**≈4.6 V**. USB's floor is **4.40 V**.

> **Measure keyboard VBUS under load. It must read ≥ 4.40 V.**

If it's marginal, in order of preference:
1. feed J3 from **5.1–5.25 V**
2. fit an **SB540/SB560** in D1's place — same pads, ~0.1 V less drop
3. shorten/thicken the 5 V run

## 6. Known gotchas inherited from v1

- **Flashing an S3-Zero:** hold RESET+BOOT → release RESET → pause → release
  BOOT. After flashing, **physically reset** (unplug/replug); a software reset
  over native USB will not boot the app.
- **Console is UART0** (J1/J2), 3.3 V USB-UART at 115200. The USB-C is the
  application's USB, not a console.
- A powered USB-UART adapter can back-power a board. The 1 k series resistors and
  the absent VCC pin exist to prevent that — don't "fix" them by adding VCC.
