# PCB Carrier Board v0.2 Tasks

Spec: `pcb-carrier-v0-2`  
Status: tasks-draft  
Created: 2026-08-29  
Brainstorm: `./brainstorm.md`
Requirements: `./requirements.md`
Design: `./design.md`

## Implementation Plan

Ordered PCB tasks, each traceable to requirement IDs, each with a concrete
"done when". New KiCad project at `hardware/pcb-carrier-v0.2/`; **no v0.1 file is
touched** — v0.1 is frozen at tag `v0.1-board`.

> ⚠ **T-002 is a hard gate on T-005.** Per design decision D-004, the power-path
> measurement happens on a breadboard **before** layout, not at bring-up. If it
> fails, the correct response is to return to the design gate — or the
> requirements gate if the fix is a supply above 5.00 V, which contradicts D-003.
> Do not begin layout on the strength of the budget arithmetic alone. That is
> exactly the mistake v0.1 made (retro F-8).

- [ ] 1. Project, design rules, and libraries
  - ID: T-001
  - Requirement(s): NFR-001, NFR-002, NFR-006, NFR-011
  - Files/areas: `hardware/pcb-carrier-v0.2/` (new KiCad project),
    `kbdrelay-carrier-v0.2.kicad_dru`, `sym-lib-table`, `fp-lib-table`
  - Work: create the KiCad project with version in the filename; vendor
    PCBWay's rules as the project-local `.kicad_dru` with the **2-layer 1 oz**
    rule block active and the others left commented; set board stackup to
    2 layers; register the project-local S3-Zero symbol/footprint library.
    Confirm the seven stock footprints named in design.md resolve.
  - Validation: `kicad-cli pcb drc` runs and *enforces* the vendored rules —
    prove enforcement with a temporary canary rule that must fail, then remove
    it. A rules file that silently fails to load is worse than none.
  - Done when: empty board DRCs clean against PCBWay rules, canary proved
    enforcement, all footprint references resolve.

- [ ] 2. **GATE** — Breadboard power-path validation
  - ID: T-002
  - Requirement(s): FR-013, FR-014, FR-017, FR-018, NFR-014
  - Files/areas: bench; results recorded in `design.md` and
    `hardware/pcb-carrier-v0.2/MEASUREMENTS.md`
  - Work: build `J1 → F1 → D1(shunt) → C1/C2 → module VDD` on a breadboard with
    the real parts. At **exactly 5.00 V** input and a **500 mA** load at the
    keyboard port, measure and record all three: voltage at the jack, at
    U1.VDD, and at keyboard VBUS. **The unknown being measured is the module's
    internal VDD→VBUS drop** — the ~0.255 V of budget nobody has ever checked.
    Then: apply reverse polarity and confirm D1 conducts, F1 trips, and normal
    operation resumes once corrected with nothing replaced. Also confirm a
    high-inrush keyboard enumerates first try with C1 fitted.
  - Validation: keyboard VBUS ≥ 4.40 V at 500 mA from 5.00 V in, **measured and
    written down**, not inferred.
  - Done when: the measurement is recorded and either (a) VBUS ≥ 4.40 V, so
    layout may proceed, or (b) it is short, and the fallback chain in design.md
    §Risks is worked in order — swap F1 to Littelfuse 60R090 for +0.055 V, else
    return to the requirements gate on D-003. **Recording a failure and
    proceeding anyway is not an acceptable outcome.**

- [ ] 3. Schematic capture
  - ID: T-003
  - Requirement(s): FR-006, FR-007, FR-008, FR-011, FR-012, FR-015, FR-020,
    FR-021, FR-022, NFR-004, NFR-007
  - Files/areas: `hardware/pcb-carrier-v0.2/kbdrelay-carrier-v0.2.kicad_sch`
  - Work: capture the net list in design.md §Schematic Design exactly. All five
    SPI lines through R1–R5 on the GP4–GP8 mapping; common GND; power chain
    J1→F1→D1→C1/C2→U1.VDD with D1 as a **shunt** (cathode to rail, anode to
    GND) and F1 **upstream** of D1; both UART headers through R6–R9 with **no
    supply pin**; **U2.VDD carrying an explicit no-connect flag** so ERC records
    the 5 V isolation as intentional. Every part gets a value and a footprint.
  - Validation: `kicad-cli sch erc` → **zero violations** (AC-10, ERC half).
  - Done when: ERC is clean, U2.VDD is explicitly no-connect, and no net joins
    the two 5 V domains.

- [ ] 4. BOM and sourcing verification
  - ID: T-004
  - Requirement(s): NFR-004, NFR-005, NFR-006
  - Files/areas: `hardware/pcb-carrier-v0.2/BOM.md`
  - Work: generate the BOM from the schematic. For each of the twelve
    non-module line items, confirm the MPN is **currently in stock at one
    distributor** and record the ordering part number. Confirm every part is
    through-hole and that only the module uses a hand-built footprint.
  - Validation: a single cart at one distributor covers every non-module line
    item (AC-12).
  - Done when: BOM is generated from the schematic rather than hand-written,
    every MPN is confirmed orderable, and any substitution is recorded.

- [ ] 5. Two-layer layout *(blocked by T-002)*
  - ID: T-005
  - Requirement(s): FR-003, FR-004, FR-016, NFR-002, NFR-003, NFR-008, NFR-016
  - Files/areas: `hardware/pcb-carrier-v0.2/kbdrelay-carrier-v0.2.kicad_pcb`
  - Work: place U1/U2 at opposite board edges with USB-C over the 1.5 mm
    overhang notch; J1 on the front edge near U1; power cluster between them;
    R1–R5 mid-board. Route **signals on F.Cu only**, leaving **B.Cu as an
    uninterrupted ground pour** — the structural fix for v0.1's split pour.
    Minimise outline subject to iron access (NFR-003 vs NFR-016; NFR-016 wins).
    Silkscreen `5V ⏜ centre +` at J1, plus the version identifier. Verify
    nothing obstructs either module's BOOT/RESET buttons.
  - Validation: `kicad-cli pcb drc` against the vendored PCBWay rules → **zero
    violations** (AC-10, DRC half); a `pcbnew` script fills zones and asserts
    the GND net resolves to **exactly one** connected region with zero
    unconnected items (AC-11).
  - Done when: DRC clean, ground verified as a single region **by script and not
    by eye**, buttons unobstructed, outline measured and recorded.

- [ ] 6. Fabrication package and build documentation
  - ID: T-006
  - Requirement(s): NFR-010, NFR-011, NFR-012, NFR-013
  - Files/areas: `hardware/pcb-carrier-v0.2/fab/`, `BUILD.md`, `README.md`,
    an as-built extraction script
  - Work: export Gerbers and drill to **exactly one** canonical directory, with
    the version in every filename. Write `BUILD.md` for a builder with no prior
    knowledge: order, assemble in a stated sequence, bring up, and how to tell
    it works. Write the script that extracts the as-built table (outline, track
    widths, clearances, layer count, footprint count) **from the board file**, so
    no as-built number is ever restated by hand (NFR-013).
  - Validation: Gerbers render correctly in an independent viewer; the as-built
    script output matches the board; no fab output exists anywhere else in the
    tree.
  - Done when: the package is orderable without opening KiCad, and every
    as-built figure in the docs is script-generated.

- [ ] 7. Order and assemble
  - ID: T-007
  - Requirement(s): FR-002, NFR-005, NFR-016
  - Files/areas: physical hardware; notes to `BUILD.md`
  - Work: order boards from PCBWay and parts from the verified BOM. Assemble
    following `BUILD.md` **as written**, recording every place the
    documentation was wrong or insufficient.
  - Validation: assembly completes with no undocumented step; every joint was
    reachable with a conventional iron with neighbours already fitted (AC-15).
  - Done when: at least one board is populated, `BUILD.md` is corrected from
    real experience, and any NFR-016 violation is recorded even if tolerable.

- [ ] 8. Bare-board bring-up *(no modules seated)*
  - ID: T-008
  - Requirement(s): FR-007, FR-008, FR-012, FR-013, FR-014, FR-015, NFR-014
  - Files/areas: physical board; results to `MEASUREMENTS.md`
  - Work: continuity on all five SPI lines through their series resistors;
    GND common; **confirm the barrel-jack rail reaches U1's socket only and
    U2's 5 V socket pin is isolated**. Apply 5.00 V and measure the protected
    rail. Apply **reverse polarity deliberately** and confirm F1 trips with no
    damage and self-recovers. Short the keyboard port briefly and confirm F1
    trips and recovers. Measure F1's in-circuit resistance.
  - Validation: all isolation, polarity and overcurrent checks pass with no
    modules fitted (AC-3, AC-4, AC-5, AC-6).
  - Done when: every check passes and **every measured value is written down**,
    not just observed.

- [ ] 9. Populated bring-up and acceptance
  - ID: T-009
  - Requirement(s): FR-001, FR-004, FR-009, FR-010, FR-017, FR-018, FR-020,
    FR-021, FR-022, NFR-014
  - Files/areas: physical board + two S3-Zero modules running v1 firmware
  - Work: seat both modules; **reflash in situ using the modules' own
    BOOT/RESET buttons** with the board assembled (AC-2). Measure and record
    loaded keyboard VBUS with the highest-draw keyboard available — this is the
    on-board confirmation of the T-002 breadboard figure. Test **all** power-up
    orderings, especially KBD-first with TGT cold. Connect a powered USB-UART
    adapter to an **unpowered** module and confirm no damage; confirm the debug
    headers carry no supply voltage. Reproduce the v1 acceptance tests AC1–AC5:
    typing, Caps/Num LED backchannel, hot-plug recovery, watchdog.
  - Validation: AC-1, AC-2, AC-7, AC-8, AC-9, AC-13 all pass, with the VBUS
    figure recorded.
  - Done when: the bridge works on v0.2 hardware and the recorded VBUS
    measurement either confirms or corrects the T-002 prediction. **A
    discrepancy between breadboard and board is a finding to write up, not a
    rounding error to ignore.**

- [ ] 10. Reproduction check and spec close-out
  - ID: T-010
  - Requirement(s): NFR-012, NFR-013, NFR-014
  - Files/areas: `BUILD.md`, `README.md`, `design.md`, this file,
    `AGENTS.md`, `~/.pi/memory-md/kbdrelay/core/HARDWARE.md`
  - Work: verify a board can be built from the repo documentation alone
    (AC-14). Regenerate the as-built table from the final board file and confirm
    every documented figure matches. Fold the recorded measurements into
    `design.md` so the accepted risks are either closed or restated with
    evidence. Update `AGENTS.md` and the durable hardware notes. Tag the
    release.
  - Validation: no documented as-built figure differs from the board file; no
    acceptance criterion is left unrecorded.
  - Done when: the spec reads as a true account of what was built — the thing
    v0.1 failed at, requiring a retroactive reconstruction.

## Verification Checklist

- [x] Requirements IDs are referenced by tasks. All 20 FR and 14 NFR appear in
      at least one task.
- [x] Design testing strategy is represented by tasks. Its eight gates map to
      T-002 (breadboard), T-003 (ERC), T-004 (BOM), T-005 (DRC + ground),
      T-008 (bare board), T-009 (populated + system), T-010 (reproduction).
- [x] Each task has a clear validation command or manual check.
- [x] Rollback/migration tasks are included when applicable. **N/A by
      construction** — new hardware in a new directory; v0.1 is frozen at tag
      `v0.1-board` and no v0.1 file is modified. Firmware is unchanged, so both
      board revisions run identical binaries.

## Execution Notes

- Keep `Status: tasks-draft` until the user explicitly approves this task plan.
- After approval, change status to `tasks-approved` and run
  implementation-readiness validation before starting work.
- **T-002 gates T-005.** Layout must not begin before the breadboard
  measurement is recorded. T-003 and T-004 may proceed in parallel with T-002,
  since the net list does not depend on the measured value — only the choice of
  F1 does, and a substitution there is a value change, not a topology change.
- Record deviations from design in `design.md` **before** implementing them.
- Measurements are recorded when taken (NFR-014). An unrecorded measurement
  counts as not having happened — this is the v0.1 failure mode this spec is
  built to avoid.
- Where an automated attempt fails, record it as a limitation of that attempt,
  never as a proof of impossibility. Two such claims were made during v0.1 and
  both were later disproved by hand.
- The `make test` hook covers host firmware unit tests and is **not relevant to
  these tasks** — firmware is unchanged. PCB tasks are hardware-verified.
