# Pcb Carrier Board Tasks

Spec: `pcb-carrier-board`
Status: implementation-in-progress
Created: 2026-08-18
Brainstorm: `./brainstorm.md`
Requirements: `./requirements.md`
Design: `./design.md`

## Implementation Plan

Ordered, independently-checkable PCB tasks traced to requirement IDs. "Done
when" is the concrete check for each. Board is designed in KiCad, single-sided
(bottom copper), all-THT discretes, two socketed S3-Zero modules.

- [x] 1. KiCad project + library setup  *(work complete; awaiting gate approval)*
  - ID: T-001
  - Requirement(s): NFR-001, NFR-002
  - Files/areas: `hardware/pcb-carrier/` (new KiCad project in-repo)
  - **Status:** done. Libraries are vendored in-project and resolve via
    `${KIPRJMOD}`, so the repo is self-contained:
    `kbdrelay-carrier/symbols/kbdrelay.kicad_sym` (`sym-lib-table`) and
    `kbdrelay-carrier/footprints/kbdrelay.pretty/` (`fp-lib-table`).
    Source: <https://github.com/jtomka/kicad-esp32-s3-zero> (no LICENSE
    upstream — clarify before publishing board files).
    Two module footprints are kept: upstream `ESP32-S3-Zero` (1.524 mm pad /
    0.762 mm drill, for direct castellated soldering) and
    **`ESP32-S3-Zero-Socketed` (2.0 mm pad / 1.0 mm drill) — the one in use**,
    because a standard 0.64 mm square header pin has a 0.905 mm diagonal and
    will not pass a 0.762 mm hole. The larger pad also gives a 0.5 mm annular
    ring for hand drilling. Tightest pad-to-pad copper gap 0.54 mm (≥ 0.5 mm
    clearance target).
  - Work: create the KiCad project; gather/verify symbols + THT footprints,
    especially the **Waveshare ESP32-S3-Zero** footprint (≈1×9 + 1×12, 0.1"),
    plus the reverse-polarity Schottky (`Device:D_Schottky` /
    `Diode_THT:D_DO-201AD_P12.70mm_Horizontal` — 12.70 mm pitch so the DO-41
    1N5817 and a DO-201AD SB5x0 both fit), radial electrolytic, PPTC, axial
    resistors, LED, 0.1" headers.
  - Done when: project opens; S3-Zero footprint + all discrete footprints exist
    and match real parts (pin count/pitch/pad size confirmed against datasheets).

- [x] 2. Schematic capture
  - ID: T-002
  - Requirement(s): FR-001..FR-005, FR-010..FR-013, FR-020..FR-026, FR-030..FR-033
  - Files/areas: KiCad schematic
  - Work: draw both module sockets; SPI net GP4–GP8 straight + 5× 2.2 kΩ series;
    power path `5V hdr → D1 Schottky (anode→+in, cathode→load) →
    470µF+0.1µF → ~0.5A polyfuse → KBD.5V`; **TGT.5V left unconnected**; common
    GND; per-module UART header (TX/RX/GND) with 1 kΩ series on TX/RX (no VCC);
    reserved GP9 reset header (each via 1 kΩ, unpopulated); power-good LED + R.
  - Done when: **ERC passes**; a manual net review confirms the two 5 V domains
    are isolated (only GND common) and SPI/series-R/polarity match design.md.
  - **Status:** done — **ERC 0 errors**; netlist reviewed against design.md
    (`+5V_KBD` = {D2.2, F1.2, U1.1}; `GND` common; `U2.VDD` unconnected).
    Two capture bugs were found and fixed: SPI was wired to GPIO1/2/3 instead
    of GP4–GP8, and both UART headers had no series resistors. Remaining ERC
    warnings are a headless-daemon library-path artifact, not design issues.

- [x] 3. Footprints + BOM finalization
  - ID: T-003
  - Requirement(s): NFR-002, NFR-004
  - Files/areas: KiCad symbols↔footprints, `hardware/pcb-carrier/BOM.md`
  - Work: assign footprints to every symbol; pick concrete, in-stock THT parts
    (D1 = 1N5817 1 A/20 V DO-41, on hand, SB540/SB560 as lower-Vf alternate on
    the same 12.70 mm footprint; 470 µF≥10 V; PPTC
    ~0.5 A hold; resistor values per design); record part numbers + sources.
  - Done when: every symbol has a footprint; BOM.md lists qty/value/part#/source,
    all THT (except modules), each part confirmed orderable (e.g., Digikey).
  - **Status:** done. Every symbol has a value + footprint;
    `hardware/pcb-carrier/BOM.md` is written. PPTC resolved to **Littelfuse
    60R090** (0.90 A hold / 1.80 A trip, 60 V, R₁ₘₐₓ 0.47 Ω, body 11.2 × 3.1 mm,
    leads on 5.08 mm centres) per the approved design amendment — the initially
    sourced 0.5 A part (60R050 / Bourns MF-RHT050) would nuisance-trip a legal
    500 mA keyboard once temperature derating is applied, and its 1.17 Ω R₁ₘₐₓ
    pushed keyboard VBUS below the 4.40 V USB floor.
  - The stock `Fuse_Bourns_MF-RHT050` footprint was **rejected** — its pads are
    staggered 1.2 mm in Y for Bourns' kinked leads. Replaced with project
    footprint `kbdrelay:PPTC_Littelfuse_60R_P5.08mm` (in-line pads on 5.08 mm,
    2.0 mm / 1.0 mm to match the home-etch drill, silk sized for the 11.2 mm
    60R090 body).
  - ⚠ **Carried into bring-up:** worst-case D1+F1 drop is 0.49 V at 0.3 A
    (VBUS 4.51 V ✓) but 0.67 V at a max-draw 0.58 A (VBUS **4.33 V ✗**).
    Feeding J3 at **5.1–5.25 V** remains recommended. See BOM.md.

- [x] 4. Single-sided layout + routing
  - ID: T-004
  - Requirement(s): FR-033, NFR-001, NFR-005, NFR-006
  - Files/areas: KiCad PCB
  - Work: place both modules so USB-C ports are edge-accessible; route on the
    **bottom copper only**; power traces ~1.0 mm, signals ~0.5 mm, clearance
    ~0.5 mm, drills ~0.9 mm / ~2 mm pads; crossings via **top-side wire jumpers**
    (keep ≤ ~8, place/label them); board outline + 4× M3 holes; silkscreen
    labels (KBD/TGT, 5V polarity, pin 1, headers, RST/BOOT notes).
  - **Form factor (agreed):** a plain rectangle used **inline** between keyboard
    and target — **U1 and U2 rotated so their USB-C ends face opposite short
    edges**, keyboard cable in one end, target cable out the other (FR-033).
    Exact dimensions are loose; let placement decide, keep it tight.
  - Consequence to watch: back-to-back module orientation means the two GP4–GP6
    pin rows no longer face each other, so the straight pin-to-pin SPI runs from
    the schematic will need real routing and may raise the jumper count. The
    ≤~8 jumper figure is an estimate, never validated against a placement.
  - ~~**A crossing-free SPI link is topologically impossible as specified.**~~
    **RETRACTED 2026-08-21 — the board routes with zero crossings.** The
    argument treated each module as a closed annulus and ignored routing
    *around* the module and *under* its body between the two pad rows. Both
    escape hatches (flip U2 to the back layer; reverse TGT's SPI pin order in
    firmware) are therefore **unnecessary** and the design gate stays closed.
  - Layout is scripted and reproducible: `hardware/pcb-carrier/place.py`
    (placement, outline, mounting holes, silkscreen, sheet centering) then
    `hardware/pcb-carrier/route.py` (single-layer maze router + GND pour).
    Both run under KiCad's bundled pcbnew Python and are idempotent — re-running
    them regenerates the board from scratch. The user reviews the result.
  - **FR-030 is now a placement constraint, not a circuit:** the shared GP9
    soft-reset (J4 + R10/R11) was removed by design amendment, so layout SHALL
    leave **both modules' onboard BOOT and RESET buttons operable with the
    modules seated** — no tall neighbouring parts blocking access.
  - Single-sided convention: **all footprints on the FRONT** (components and
    silkscreen on top, modules socketed on 0.1" headers), **all routing on
    B.Cu**. Through-hole pads span every copper layer, so no vias exist and
    none are needed — crossings are solved by top-side jumper links.
  - Add a **`BOTTOM` text item on B.Cu**. KiCad mirrors back-layer text, so on
    the finished board it reads correctly when viewed from the copper side — a
    one-glance check that the artwork was not accidentally flipped.
  - Orientation sanity check: the S3-Zero footprint is asymmetric (1.5 mm USB-C
    overhang notch), so confirm on the artwork that both USB-C ports face off
    the board edges (FR-033).
  - Done when: 100% routed on one layer with jumpers listed; matches the home-fab
    trace/space/drill profile; USB-C ports clear the outline.
  - **Status: done (2026-08-21).** Board **88 × 55 mm** (was 120 × 55 — see the
    layout amendment in design.md), **100% routed on B.Cu, zero jumpers, zero
    unconnected**, and `kicad-cli pcb drc` reports **no clearance, short,
    crossing or copper-edge violations**. Widths 0.7 mm signal / 1.5 mm power +
    GND, clearance 0.4 mm. GND is a B.Cu pour with solid pad connections and
    three keep-clear channels (C1.2, C2.2, U2.2 would otherwise be stranded on
    pour islands). 4× M3 holes, silkscreen labels, and the mirrored `BOTTOM`
    B.Cu label are placed; the board is centred on the A4 sheet.
  - **Bug found and fixed while picking this up:** both modules were placed
    *off the board* (U1 pads at x −20…0, U2 at 122…140 on a 0–120 board). The
    anchors were correct; the rotations were inverted (U1 had 270° where it
    needs 90°, U2 the reverse). The old DRC's "4 copper-edge corner artifacts"
    were this. Every discrete had been placed around the *intended* pad grid,
    so only the two rotations needed correcting.
  - Remaining DRC output is cosmetic/expected: 6× silk clipped by board edge
    (the intentional USB-C overhang notches), 4× courtyard overlap in the dense
    THT power cluster, 1× silk overlap (F1 ref vs C1 body), and 13×
    `lib_footprint_mismatch` (board copies differ from library copies; predates
    this work, not fabrication-relevant).

- [ ] 5. DRC + design review vs traceability
  - ID: T-005
  - Requirement(s): FR-013, FR-023, FR-020, NFR-001
  - Files/areas: KiCad DRC, `design.md` traceability table
  - Work: run DRC (clean); walk the traceability table confirming each FR is
    physically present (5 V isolation, series R on all 5 SPI lines, D1 Schottky
    orientation — silkscreen band/cathode toward the load, polyfuse in series,
    TGT.5V unconnected, UART series R).
  - Done when: **DRC passes**; every traceability row visually verified on-board.

- [ ] 6. Fabrication outputs + build notes
  - ID: T-006
  - Requirement(s): NFR-001, NFR-004, NFR-006
  - Files/areas: `hardware/pcb-carrier/fab/` (Gerbers), 1:1 etch print, `BUILD.md`
  - Work: export standard Gerbers (for a PCB house) and a **1:1 bottom-copper
    print** (toner-transfer/home etch); finish `hardware/pcb-carrier/BUILD.md`
    (drafted): etch/drill steps, jumper-link list, assembly order, bring-up.
  - Mirroring for toner transfer is handled by the existing home-etch process;
    the `BOTTOM` copper label from T-004 is the post-etch confirmation.
  - Drill note: module holes are **1.0 mm**; all other THT parts use their
    library defaults. Have a 1.0 mm bit on hand.
  - Done when: Gerbers render correctly in a viewer; 1:1 print measures true to
    scale; BUILD.md covers fab → assembly → bring-up; `BOTTOM` label reads
    correctly on the etched board.

- [ ] 7. Bare-board bring-up
  - ID: T-007
  - Requirement(s): FR-003, FR-010..FR-013, FR-020, FR-021, FR-032
  - Files/areas: physical board (no modules yet)
  - Work: continuity/isolation checks (GND common; 5V vs TGT-5V isolated; SPI
    pin-to-pin); apply 5 V → verify protected rail ≈ input − ~0.3 V (Schottky
    Vf) + power-good LED; apply **reversed 5 V** → verify no rail, no damage.
    Also measure F1's actual DC resistance in-circuit (expect 0.5–1.2 Ω) so the
    VBUS budget is known before a keyboard is attached.
  - Done when: isolation + polarity + LED checks all pass with no modules seated.

- [ ] 8. Populated bring-up + acceptance
  - ID: T-008
  - Requirement(s): FR-001, FR-002, FR-005, FR-022, FR-024, FR-025, FR-026,
    FR-030 (download-mode flash in situ using the modules' own buttons)
  - Files/areas: physical board + two S3-Zero modules (v1 firmware)
  - Work: seat modules; reproduce **AC1–AC5** (typing, Caps/Num LED backchannel,
    hot-plug, watchdog); fault tests — high-inrush keyboard enumerates first try,
    keyboard VBUS measures **≥4.40 V** under the highest-draw keyboard (Schottky
    Vf headroom check — if low, raise J_PWR to ~5.2 V or fit the SB5x0),
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

- ✅ Approved by the user on 2026-08-20. T-001, T-002 and T-003 were completed
  during planning (project + libraries, ERC-clean schematic, BOM); they remain
  unchecked until each is validated per its own "Done when".
- Implementation-readiness validation passes; layout (T-004) is the next task.
- When work starts, change status to `implementation-in-progress`.
- When all tasks are checked and validated, change status to `implementation-complete`.
- Complete tasks in order unless the user approves reordering.
- Mark tasks complete only after validation passes.
- Record deviations from design in design.md before implementing them.
- Tooling update: the agent **can** now drive KiCad through the pi-kicad /
  Konnect bridge. Schematic edits, ERC, netlist export and library authoring are
  file-based and need no KiCad UI; **PCB layout/DRC tools require KiCad 10 open
  with its API enabled and the board loaded**. Physical work (etch, drill,
  solder, meter, bring-up) is the user's.
