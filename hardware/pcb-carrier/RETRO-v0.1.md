# kbdrelay carrier v0.1 — retrospective

**Status: DRAFT — objective findings complete, bench/experiential findings pending.**

Purpose: this is the seed document for the v0.2 spec's brainstorm gate. v0.1 is
frozen at tag `v0.1-board`; the `pcb-carrier-board` spec is closed as its record.
Everything v0.2 changes should trace back to a numbered finding here.

Sections marked **[NEEDS BENCH DATA]** are gaps the repo cannot answer — they
require the measurements and impressions from the person who etched, assembled
and ran the board.

---

## 1. What v0.1 actually is (verified from the files, 2026-08-27)

| Property | Value |
|----------|-------|
| Board of record | `kbdrelay-carrier/kbdrelay-carrier-v0.1.kicad_pcb` |
| Outline | 88 × 55 mm, landscape, 4× M3 corner holes |
| Layers | single-sided, **B.Cu only** — 73 track segments, 0 on F.Cu |
| Track widths | 70 × 1.0 mm, 2 × 0.8 mm, 1 × 1.5 mm |
| Pour clearance | 1.4 mm; min copper gap 0.605 mm (adjacent module pads) |
| Wire jumpers | **0** |
| Footprints | 22 — U1 U2 D1 F1 C1 C2 R1–R9 J1–J3 H1–H4 |
| DRC | **0 violations**; 1 known `unconnected_items` (split GND pour) |
| ERC | 0 violations |
| Fab artwork as etched | `fab/gerbers/kbdrelay-carrier-v0.1-*.gbr` |
| Process | home etch, LCD photoresist exposure (Elegoo Mars 4 Ultra) |

Verified hardware function: v1 firmware, AC1–AC5, real keyboards, and a
Commodore Amiga 1200 (1992) via a USB keyboard adapter.

---

## 2. Process findings — what the v0.1 *method* got wrong

These are about how the design was produced, not the board. They are the
highest-value carry-forwards because they will otherwise repeat.

**F-1. The scripted router was a dead end, and its "impossible" claims were
false.** `place.py`/`route.py` produced a board that needed 2 wire jumpers at
0.7 mm traces / 0.8 mm clearance, and design.md amendment 3 declared zero
jumpers "structural — nine signals cannot fan out of U1's rows". A human then
hand-routed **zero jumpers at 1.0 mm traces and 1.4 mm pour clearance** — better
on every axis simultaneously. *Carry-forward: never let a failed search become a
recorded impossibility claim. This is the second retracted impossibility proof in
this spec (the first was the annulus argument in the original design).*

**F-2. Docs drifted from the board in five separate places, and no check caught
it.** At close-out the repo simultaneously claimed: design status `design-draft`
(it was approved), 0.4 mm clearance (shipped 1.4 mm pour), a power-good LED
(dropped), "layout is scripted — do not hand-edit" (shipped board is hand-routed),
and "T-005 next" (T-006 was already done). BOM.md still listed D2/R12.
*Carry-forward: v0.2 needs a single generated as-built table, extracted from the
`.kicad_pcb` by script, that no prose is allowed to restate.*

**F-3. Two layouts with no naming discipline nearly shipped the wrong artwork.**
The board of record was called `-test2` while a stale, DRC-failing board held the
canonical name. BUILD.md needed an explicit warning paragraph to compensate.
*Carry-forward: one board file per revision, versioned in the filename, and the
superseded one gets archived or deleted, not left adjacent.*

**F-4. Verification happened on the bench but was never written down.** T-005,
T-007 and T-008 sat unchecked while a working, verified board existed. T-005 has
now been reconstructed from the files; T-007/T-008 evidence is
**[NEEDS BENCH DATA]** and may be unrecoverable. *Carry-forward: bring-up
measurements get recorded at the bench, in the task, or they did not happen.*

**F-5. The KiCad files carry name-only nets** (`(net "GND")` with no numeric net
table) — an artifact of tool-assisted authoring. Checked and found **harmless**:
`GetNetCount()` = 42 and DRC is byte-identical before and after a `pcbnew`
load/re-save. Logged so nobody re-investigates it. *Carry-forward: normalize
through `pcbnew` once at the start of v0.2 anyway.*

---

## 3. Design findings — what the v0.1 *board* got wrong

**F-6. The GND pour is split into two islands** that do not connect to each
other (DRC `[unconnected_items]`, two `Zone [GND] on B.Cu` regions). Benign in
practice on this layout — the board works — but it is an unintended, undocumented
discontinuity in the ground return. *v0.2 must produce a single contiguous GND
pour, or an explicit deliberate split with a stated stitch point.*

**F-7. The 0.605 mm minimum gap sits below the 0.8 mm etch requirement
(NFR-007).** The hand route bought 1.4 mm almost everywhere but the module pad
rows are still 0.605 mm apart, because pad-to-pad spacing on a 2.54 mm 2×9
module is fixed by the part. *v0.2 decision needed: accept it, shrink the pads
further, or change the module interface.*

**F-8. Schottky Vf eats the USB voltage margin — ACCEPTED, UNMEASURED.**
D1 = 1N5817 drops ~0.3 V at 0.3 A, plus F1's 0.2–0.47 Ω, so keyboard VBUS is
calculated to land near 4.6 V from a 5.00 V feed against the 4.40 V USB floor.

**Decision (2026-08-27, user):** carry it forward unchanged. VBUS was **never
measured**; no misbehaviour was observed with the keyboards tried, and a
through-hole logic-level P-FET is unsourceable at hobby quantity, so the series
Schottky stands.

⚠ Recorded honestly: "no odd behaviour observed" is **not** a margin
measurement. If the real figure is ~4.45 V the board works today and fails on
the first hungrier keyboard or slightly sagging feed. This is an accepted risk,
not a closed question.

**Two cheap escape hatches exist, neither requiring a P-FET or a respin:**
1. **Fit an SB540/SB560** — the D1 footprint is DO-201AD / 12.70 mm *specifically*
   so this drops in. Roughly half the 1N5817's Vf at this current, worth ~0.15 V,
   and it retrofits to the existing v0.1 board.
2. **Feed 5.1–5.25 V at J3** — already the documented recommendation.

*v0.2 action: keep the series Schottky and the drop-in-compatible footprint, and
make "measure loaded keyboard VBUS" a one-line bring-up step so the number
finally exists. Breadboarding an ideal-diode alternative is parked, not rejected.*

---

## 4. [NEEDS BENCH DATA] — open questions for v0.2

Answer these and the v0.2 brainstorm can start from evidence instead of guesses.

1. ~~Keyboard VBUS under load~~ — **answered: never measured, risk accepted.**
   See F-8. Still worth a 2-minute meter check on the existing board if it is
   ever out on the bench, since it closes the question permanently.
2. **F1 in-circuit DC resistance** — the T-007 measurement (expected 0.2–1.2 Ω).
   Cheap to take alongside item 1.
3. **Did anything nuisance-trip, brown out, or fail to enumerate?** Including
   the high-inrush keyboard, and whether the 470 µF bulk cap was sufficient.
4. **Power-up ordering** — did all orders work, especially KBD-first → TGT
   cold-start?
5. **Etch quality** — where did the artwork actually bridge or under-develop, if
   anywhere? Which gaps were marginal in reality vs. predicted by `bridge_risk.py`?
6. **Assembly pain** — socket strips, the H3/C1 clearance, drilling 1.0 mm,
   anything that made the board annoying to build.
7. **Ergonomics in use** — USB-C exit direction, board size, mounting, whether
   the debug UART headers earned their place.
8. **What do you actually want different in v0.2?** Scope question: is this a
   fix-the-defects respin, or a change in form factor / power path / module
   choice?
