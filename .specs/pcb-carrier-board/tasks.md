# Pcb Carrier Board Tasks

Spec: `pcb-carrier-board`
Status: tasks-draft
Created: 2026-08-18
Brainstorm: `./brainstorm.md`
Requirements: `./requirements.md`
Design: `./design.md`

## Implementation Plan

Ordered, independently-checkable PCB tasks traced to requirement IDs. "Done
when" is the concrete check for each. Board is designed in KiCad, single-sided
(bottom copper), all-THT discretes, two socketed S3-Zero modules.

- [ ] 1. KiCad project + library setup
  - ID: T-001
  - Requirement(s): NFR-001, NFR-002
  - Files/areas: `hardware/pcb-carrier/` (new KiCad project in-repo)
  - Work: create the KiCad project; gather/verify symbols + THT footprints,
    especially the **Waveshare ESP32-S3-Zero** footprint (≈1×9 + 1×12, 0.1"),
    plus THT P-FET (TO-220/TO-92), radial electrolytic, PPTC, axial resistors,
    LED, 0.1" headers.
  - Done when: project opens; S3-Zero footprint + all discrete footprints exist
    and match real parts (pin count/pitch/pad size confirmed against datasheets).

- [ ] 2. Schematic capture
  - ID: T-002
  - Requirement(s): FR-001..FR-005, FR-010..FR-013, FR-020..FR-026, FR-030..FR-033
  - Files/areas: KiCad schematic
  - Work: draw both module sockets; SPI net GP4–GP8 straight + 5× 2.2 kΩ series;
    power path `5V hdr → P-FET (drain→+in, source→load, gate→GND via 100k) →
    470µF+0.1µF → ~0.5A polyfuse → KBD.5V`; **TGT.5V left unconnected**; common
    GND; per-module UART header (TX/RX/GND) with 1 kΩ series on TX/RX (no VCC);
    reserved GP9 reset header (each via 1 kΩ, unpopulated); power-good LED + R.
  - Done when: **ERC passes**; a manual net review confirms the two 5 V domains
    are isolated (only GND common) and SPI/series-R/polarity match design.md.

- [ ] 3. Footprints + BOM finalization
  - ID: T-003
  - Requirement(s): NFR-002, NFR-004
  - Files/areas: KiCad symbols↔footprints, `hardware/pcb-carrier/BOM.md`
  - Work: assign footprints to every symbol; pick concrete, in-stock THT parts
    (P-FET logic-level Vgs(th)≈−1..−2 V / low Rds@−4.5 V; 470 µF≥10 V; PPTC
    ~0.5 A hold; resistor values per design); record part numbers + sources.
  - Done when: every symbol has a footprint; BOM.md lists qty/value/part#/source,
    all THT (except modules), each part confirmed orderable (e.g., Digikey).

- [ ] 4. Single-sided layout + routing
  - ID: T-004
  - Requirement(s): FR-033, NFR-001, NFR-005, NFR-006
  - Files/areas: KiCad PCB
  - Work: place both modules so USB-C ports are edge-accessible; route on the
    **bottom copper only**; power traces ~1.0 mm, signals ~0.5 mm, clearance
    ~0.5 mm, drills ~0.9 mm / ~2 mm pads; crossings via **top-side wire jumpers**
    (keep ≤ ~8, place/label them); board outline + 4× M3 holes; silkscreen
    labels (KBD/TGT, 5V polarity, pin 1, headers, RST/BOOT notes).
  - Done when: 100% routed on one layer with jumpers listed; matches the home-fab
    trace/space/drill profile; USB-C ports clear the outline.

- [ ] 5. DRC + design review vs traceability
  - ID: T-005
  - Requirement(s): FR-013, FR-023, FR-020, NFR-001
  - Files/areas: KiCad DRC, `design.md` traceability table
  - Work: run DRC (clean); walk the traceability table confirming each FR is
    physically present (5 V isolation, series R on all 5 SPI lines, P-FET
    orientation, polyfuse in series, TGT.5V unconnected, UART series R).
  - Done when: **DRC passes**; every traceability row visually verified on-board.

- [ ] 6. Fabrication outputs + build notes
  - ID: T-006
  - Requirement(s): NFR-001, NFR-004, NFR-006
  - Files/areas: `hardware/pcb-carrier/fab/` (Gerbers), 1:1 etch print, `BUILD.md`
  - Work: export standard Gerbers (for a PCB house) and a **1:1 bottom-copper
    print** (toner-transfer/home etch); write BUILD.md: etch/drill steps,
    jumper-link list, assembly order, and bring-up procedure.
  - Done when: Gerbers render correctly in a viewer; 1:1 print measures true to
    scale; BUILD.md covers fab → assembly → bring-up.

- [ ] 7. Bare-board bring-up
  - ID: T-007
  - Requirement(s): FR-003, FR-010..FR-013, FR-020, FR-021, FR-032
  - Files/areas: physical board (no modules yet)
  - Work: continuity/isolation checks (GND common; 5V vs TGT-5V isolated; SPI
    pin-to-pin); apply 5 V → verify protected rail ≈ input (P-FET low drop) +
    power-good LED; apply **reversed 5 V** → verify no rail, no damage.
  - Done when: isolation + polarity + LED checks all pass with no modules seated.

- [ ] 8. Populated bring-up + acceptance
  - ID: T-008
  - Requirement(s): FR-001, FR-002, FR-005, FR-022, FR-024, FR-025, FR-026
  - Files/areas: physical board + two S3-Zero modules (v1 firmware)
  - Work: seat modules; reproduce **AC1–AC5** (typing, Caps/Num LED backchannel,
    hot-plug, watchdog); fault tests — high-inrush keyboard enumerates first try,
    brief keyboard-VBUS short trips the polyfuse, **all power-up orders** work
    (esp. KBD-first → TGT cold-starts); confirm a debug adapter can't back-power.
  - Done when: AC1–AC5 pass on the carrier and all fault/ordering tests pass;
    spec acceptance criteria AC-1..AC-6 satisfied.

## Verification Checklist

- [ ] Requirements IDs are referenced by tasks. (T-001..T-008 cover FR/NFR.)
- [ ] Design testing strategy is represented by tasks. (T-005, T-007, T-008.)
- [ ] Each task has a clear validation command or manual check. (ERC/DRC/meter/ACs.)
- [ ] Rollback/migration tasks are included when applicable. (N/A — new hardware.)

## Execution Notes

- Keep `Status: tasks-draft` until the user explicitly approves this task plan.
- After approval, change status to `tasks-approved` and run implementation-readiness validation before starting layout work.
- When work starts, change status to `implementation-in-progress`.
- When all tasks are checked and validated, change status to `implementation-complete`.
- Complete tasks in order unless the user approves reordering.
- Mark tasks complete only after validation passes.
- Record deviations from design in design.md before implementing them.
- Note: schematic/layout/fab are done in KiCad by the user (agent can't drive
  KiCad); the agent can help with symbols/values/BOM, review Gerbers/exports,
  draft BUILD.md, and reason through DRC/bring-up results.
