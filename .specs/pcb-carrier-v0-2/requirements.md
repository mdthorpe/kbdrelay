# PCB Carrier Board v0.2 Requirements

Spec: `pcb-carrier-v0-2`  
Status: requirements-approved  
Created: 2026-08-29  
Brainstorm: `./brainstorm.md`

> **ID namespace:** v0.2 requirement IDs are freshly numbered and do **not**
> correspond to v0.1's `FR-001..FR-032` / `NFR-001..NFR-008`. When referring to a
> v0.1 requirement, always qualify it as `v0.1 FR-nnn`. v1 *system* requirements
> from the root `spec.md` are cited as `v1 FR5.1` etc. and remain binding.

## Overview

Clean-sheet redesign of the kbdrelay carrier PCB for professional fabrication at
PCBWay (2-layer, 1 oz) and reproduction by third-party builders. New schematic
and layout; the Waveshare ESP32-S3-Zero modules, their pin mapping, and the v1
firmware are unchanged. Barrel-jack 5 V input with re-derived protection,
continuous ground plane, THT-only discretes sourceable from Digikey-class
retailers, free board outline driving a 3D-printed enclosure with a removable
lid. Retires the entire home-etch toolchain and its constraints.

## Scope

### In Scope

- A new schematic, drawn from scratch. The v0.1 netlist is not carried over.
- The KBD 5 V power path and its protection, re-derived: reverse polarity,
  overcurrent, and inrush.
- A barrel-jack 5 V input, replacing v0.1's 2-pin header.
- A 2-layer PCBWay-manufacturable layout with a continuous ground plane.
- PCBWay design rules vendored into the repo as a project-local DRC gate.
- A board outline free of external mechanical constraint, which the 3D-printed
  enclosure is then designed to fit.
- Retained per-module debug UART headers, reachable by removing the lid.
- THT-only discrete components with verified, orderable manufacturer part
  numbers.
- Build, assembly, and bring-up documentation written for a third party.
- A versioned fabrication package (Gerbers, drill, BOM) in the repo.

### Out of Scope

- **Firmware changes.** The S3-Zero pin mapping is fixed and this board adapts
  to it, not the reverse.
- **Module substitution.** The Waveshare ESP32-S3-Zero stays.
- **Board-mounted USB connectors** of any kind. Physically impossible with this
  module — see DC-002.
- **Board-mounted BOOT/RESET buttons.** Also physically impossible — see DC-002.
- **Revisiting the v1 FR5 5 V-isolation architecture.** It holds unchanged.
- **Surface-mount components.**
- **Detailed enclosure CAD.** The enclosure follows the finished board and is
  not specified here beyond the constraints it imposes on the board.
- **The entire home-etch toolchain**, explicitly retired: LCD photoresist
  exposure artwork, `bridge_risk.py`, `audit_gaps.py`, the wire-jumper budget,
  the `BOTTOM` copper label, and single-sided routing.
- **Selling or shipping assembled units.**
- **Overvoltage protection.** Deliberately excluded; see FR-016 and EC-002.

## User Stories

- As a **retrocomputing hobbyist**, I want to clone the repo, order the board
  from PCBWay, and buy every component from a mainstream electronics retailer,
  so that I can build a working kbdrelay without sourcing detective work.
- As a **first-time builder**, I want assembly and bring-up documentation that
  assumes no prior knowledge of this project, so that I can tell whether my
  board works before I connect it to a machine I care about.
- As a **builder who owns a 3D printer**, I want the board's connectors on its
  edges and its outline documented, so that a printed case fits it and the
  external connections line up.
- As **someone with a drawer full of wall adapters**, I want a wrong supply to
  fail safe rather than destroy the boards, so that a careless mistake costs
  nothing.

## Functional Requirements

### Module interface

- **FR-001**: THE carrier SHALL mechanically and electrically host exactly two
  Waveshare ESP32-S3-Zero modules, designated U1 (KBD) and U2 (TGT).
- **FR-002**: THE carrier SHALL mount both modules in **sockets**, such that
  either module can be removed and refitted without desoldering.
- **FR-003**: THE carrier SHALL position both modules so that each module's
  USB-C connector sits at a board edge, clear of the board outline.
- **FR-004**: THE carrier SHALL leave both modules' onboard BOOT and RESET
  buttons physically operable while both modules are seated, unobstructed by
  any board-mounted component.

*(FR-005 and FR-023 were withdrawn during requirements review. FR-005 required
mounting provision — a reflex carried over from v0.1, which needed corner holes
because it was a bare board; an enclosure designed to fit this board can capture
it without fixings, and mounting is deferred until a case actually exists.
FR-023 required the modules' status LEDs to face the lid, which is automatic:
modules socket onto the top face, so their LEDs already point up. A requirement
that cannot be violated is not a requirement.)*

### Bridge interconnect

- **FR-006**: THE carrier SHALL connect the five SPI signals between U1 and U2
  using the firmware's fixed pin mapping: SCK on GP4, MOSI on GP5, MISO on GP6,
  CS on GP7, and DATA_READY on GP8.
- **FR-007**: THE carrier SHALL connect U1 GND and U2 GND to a common ground
  (v1 FR5.4).
- **FR-008**: THE carrier SHALL place a series resistance in each of the five
  SPI signal paths, sized to limit backfeed current into an unpowered peer
  module (v1 FR5.4).
- **FR-009**: WHEN one module is powered and the other is not, THE carrier SHALL
  NOT allow either module to be damaged or latched up (v1 FR5.4).
- **FR-010**: WHEN the two modules are powered in any order, THE bridge SHALL
  reach normal operation without manual intervention.

### Power domains and protection

- **FR-011**: THE carrier SHALL accept 5 V input through a single **2.1 × 5.5 mm
  centre-positive** DC barrel jack, which SHALL be the board's only external
  power input and SHALL sit at a board edge.
- **FR-012**: THE carrier SHALL route the barrel-jack supply to U1 (KBD) only,
  and SHALL leave U2's 5 V pin unconnected, so that the two 5 V domains are
  never joined (v1 FR5.1, v1 FR5.3).
- **FR-013**: IF a reverse-polarity supply is connected to the barrel jack, THE
  carrier SHALL prevent damage to either module, to the carrier, and to an
  attached keyboard.
- **FR-014**: WHEN a reverse-polarity supply is corrected, THE carrier SHALL
  return to normal operation without replacing any component.
- **FR-015**: THE carrier SHALL provide resettable overcurrent protection on the
  keyboard 5 V rail, such that a sustained overcurrent or a short at the
  keyboard port is interrupted and recovers without component replacement.
- **FR-016**: THE carrier SHALL carry silkscreen marking at the barrel jack
  stating the required supply voltage and polarity (`5V ⏜ centre +`).
- **FR-017**: THE carrier SHALL provide bulk capacitance on the KBD 5 V rail
  sufficient that a high-inrush USB keyboard enumerates on first plug-in without
  browning out the module.
- **FR-018**: WHILE supplying a keyboard drawing up to 500 mA, THE carrier SHALL
  maintain keyboard VBUS at or above **4.40 V**, measured at the keyboard port,
  from a **5.00 V** input at the barrel jack. *(5.00 V is the design point, not
  a recommended supply voltage: it is the honest worst case for a builder using
  a generic adapter. Any higher input is headroom, never a requirement — see
  D-003.)*

*(FR-019 was withdrawn during requirements review: it capped protection losses
at 0.35 V, which double-specified the same physical quantity FR-018 already
bounds. Allocating that budget across protection, copper and socket contact
resistance is design work, not a requirement. IDs are never reused.)*

### Debug and diagnostics

- **FR-020**: THE carrier SHALL provide one debug UART header per module,
  exposing that module's TX, RX, and GND.
- **FR-021**: THE debug UART headers SHALL NOT expose any supply voltage.
- **FR-022**: THE carrier SHALL place a series resistance in each debug UART
  signal path, sized to survive a powered USB-UART adapter being connected to an
  unpowered module.

## Non-Functional Requirements

### Manufacturability

- **NFR-001**: THE board SHALL be manufacturable by PCBWay's standard 2-layer,
  1 oz copper service.
- **NFR-002**: THE repo SHALL contain PCBWay's design rules as a project-local
  `.kicad_dru`, and the board SHALL report **zero DRC violations** against them
  via `kicad-cli pcb drc`.
- **NFR-003**: THE board outline SHALL be no larger than 100 × 100 mm.
- **NFR-004**: ALL board-mounted components other than the two modules SHALL be
  through-hole. No surface-mount parts.
- **NFR-005**: EVERY board-mounted component other than the two modules SHALL be
  orderable from a single mainstream electronics distributor, identified in the
  BOM by manufacturer part number. The ESP32-S3-Zero modules are exempt from
  this requirement.
- **NFR-006**: WHERE a stock KiCad library footprint exists for a part, THE
  design SHALL use it in preference to a hand-built footprint.
- **NFR-007**: THE schematic SHALL report zero ERC violations via
  `kicad-cli sch erc`.

### Electrical integrity

- **NFR-008**: THE ground net SHALL form a single electrically continuous
  region, verified as such rather than assumed. *(Addresses v0.1 retro finding
  F-6, where the ground pour silently split into two islands.)*
*(NFR-009 was withdrawn during requirements review: "no copper gap below the
PCBWay minimum" is precisely what NFR-002's DRC run against the vendored PCBWay
rules checks. v0.1 retro finding F-7 is covered by NFR-002.)*

### Reproducibility and documentation

- **NFR-010**: THE repo SHALL contain a fabrication package sufficient to order
  the board without opening KiCad, stored in exactly one canonical location.
- **NFR-011**: THE board files, fabrication outputs, and documentation SHALL
  carry an unambiguous version identifier, such that no artifact can be confused
  with a different revision. *(Addresses v0.1 retro finding F-3.)*
- **NFR-012**: THE build documentation SHALL be sufficient for a builder with no
  prior knowledge of this project to fabricate, assemble, and verify a board.
- **NFR-013**: ANY as-built parameter stated in documentation SHALL be derivable
  from the board files by a repeatable command, not restated by hand.
  *(Addresses v0.1 retro finding F-2, where prose drifted from copper in five
  places simultaneously.)*
- **NFR-014**: BRING-UP measurements SHALL be recorded in the spec at the time
  they are taken. *(Addresses v0.1 retro finding F-4, where verification
  happened but was never written down, and F-8, where a voltage margin was
  accepted without ever being measured.)*

*(NFR-015 was withdrawn during requirements review: "never record a failed
search as a proof of impossibility" is a working convention, not a property of
the board, and it already lives in `AGENTS.md` where it will actually be read.
v0.1 retro finding F-1 is addressed there.)*

## Acceptance Criteria

- [ ] **AC-1** — Two modules seat in sockets, are removable, and both USB-C
      connectors sit at board edges clear of the outline, as does the barrel
      jack. *(FR-001, FR-002, FR-003, FR-011)*
- [ ] **AC-2** — A firmware reflash is performed with both modules seated on the
      assembled board, using the modules' own BOOT/RESET buttons. *(FR-004)*
- [ ] **AC-3** — Continuity confirms all five SPI lines on the GP4–GP8 mapping,
      each through its series resistance, and a common ground. *(FR-006, FR-007,
      FR-008)*
- [ ] **AC-4** — Isolation confirms the barrel-jack rail reaches U1 only and
      U2's 5 V pin is unconnected. *(FR-012)*
- [ ] **AC-5** — A reverse-polarity supply is applied deliberately; no damage
      results, and normal operation resumes once corrected, with no parts
      replaced. *(FR-013, FR-014)*
- [ ] **AC-6** — A short at the keyboard port trips protection and recovers with
      no parts replaced. *(FR-015)*
- [ ] **AC-7** — The highest-draw available keyboard enumerates on first plug-in,
      and **keyboard VBUS is measured and recorded** at or above 4.40 V.
      *(FR-017, FR-018, NFR-014)*
- [ ] **AC-8** — The bridge reaches normal operation from every power-up
      ordering, including KBD-first with TGT cold. *(FR-010)*
- [ ] **AC-9** — A powered USB-UART adapter is connected to an unpowered module
      without damage, and the debug headers are confirmed to carry no supply
      voltage. *(FR-020, FR-021, FR-022)*
- [ ] **AC-10** — `kicad-cli sch erc` and `kicad-cli pcb drc` both report zero
      violations, the latter against the vendored PCBWay rules. *(NFR-002,
      NFR-007)*
- [ ] **AC-11** — The ground net is verified to be a single continuous region by
      a repeatable check. *(NFR-008)*
- [ ] **AC-12** — The BOM lists a manufacturer part number for every part, and
      every non-module part is confirmed orderable from one distributor.
      *(NFR-005)*
- [ ] **AC-13** — The v1 firmware reproduces the v1 acceptance tests AC1–AC5 on
      this board: typing, the Caps/Num LED backchannel, hot-plug recovery, and
      watchdog behavior.
- [ ] **AC-14** — A board is fabricated and assembled following only the repo's
      documentation, with no undocumented steps required.

## Edge Cases

*(EC-001, EC-003, EC-004 and EC-005 were withdrawn during requirements review:
each restated a functional requirement and pointed at its acceptance criterion
without adding information. Reverse polarity is FR-013/FR-014, keyboard
overcurrent is FR-015, inrush is FR-017, and the powered/unpowered peer case is
FR-008/FR-009. The edge cases below are the ones that are **not** already fully
stated by a requirement.)*

- **EC-002 — Wrong-voltage supply.** A 2.1 × 5.5 mm barrel plug is a mechanical
  standard only, so any 9 V, 12 V or 19 V adapter physically fits. **This is
  deliberately unprotected**: no through-hole-only network survives a
  high-current 19 V supply, so the mitigation is FR-016's silkscreen marking
  plus the documented supply specification. Accepted risk, recorded here so it
  is never mistaken for an oversight.
- **EC-006 — Powered debug adapter on an unpowered module.** A USB-UART adapter
  can back-power a module through its TX pin. Covered by FR-022; verified by
  AC-9.
- **EC-007 — Undervoltage supply.** A nominally 5 V adapter sagging under load,
  or a long thin cable, erodes the FR-018 margin. The design must state the
  input voltage its budget assumes.
- **EC-008 — Keyboard exceeding 500 mA.** RGB keyboards at full brightness can
  exceed the FR-018 figure. Best-effort per v1 FR5.2; protection should trip
  rather than sag indefinitely.

## Dependencies and Constraints

- **DC-001 — Firmware is fixed.** SPI is SCK/GP4, MOSI/GP5, MISO/GP6, CS/GP7,
  DATA_READY/GP8; the console is UART0. The board adapts to firmware.
- **DC-002 — The S3-Zero exposes only 18 pins**: VDD, GND, 3V3, GPIO1–13, RX,
  TX. It does **not** break out GPIO19/20 (native USB D±), nor EN/RESET, nor
  GPIO0. Consequences, both binding: USB exists only at each module's own USB-C
  jack, so board-mounted USB receptacles are impossible; and BOOT/RESET exist
  only as the modules' own buttons, so download mode requires physical access to
  them.
- **DC-003 — Module USB-C jacks are on the modules, not the PCB.** Both modules
  must therefore sit at board edges, and enclosure cutouts follow the layout
  rather than constraining it.
- **DC-004 — Two MCUs are forced by silicon.** One ESP32-S3 has a single USB
  PHY, so host and device duty cannot share a chip. This is not a design choice
  and is not open for review.
- **DC-005 — PCBWay 2-layer 1 oz limits**, from the supplied rules file:
  0.127 mm track width and clearance, 0.25 mm minimum annular ring, 0.3 mm
  track-to-edge, 0.5 mm hole-to-hole across different nets, 0.5 mm minimum
  non-plated hole, vias 0.3 mm hole / 0.5 mm diameter. The file targets KiCad 7
  but has been verified to parse and enforce on KiCad 10.0.5.
- **DC-006 — The enclosure is downstream and imposes nothing.** See D-005. The
  board is constrained only by NFR-003's size limit and by keeping the three
  external connectors on board edges.
- **DC-007 — Sourcing rule scope.** NFR-005 governs electronic components. The
  ESP32-S3-Zero modules are explicitly exempt and are expected to be bought
  from general online retailers.
- **DC-008 — Bench-earned facts carried forward deliberately.** Although the
  schematic is a clean sheet, four findings were paid for in brownouts and
  latch-ups rather than chosen for etching convenience, and are retained as
  requirements above: bulk capacitance for inrush (FR-017), series resistance on
  SPI for backfeed (FR-008), no supply pin on debug headers (FR-021), and TGT's
  5 V left unconnected (FR-012).
*(DC-009 was withdrawn during requirements review: "v0.1 is a reference, not a
baseline" is already stated by the Overview and the Out of Scope list.)*

## Decisions

Resolved during requirements review. Recorded so the reasoning survives.

- **D-001 — No carrier-mounted status LED.** The modules' own firmware-driven
  RGB LEDs are the status indication. A carrier LED would duplicate them while
  telling the builder strictly less than a meter reading — the same reasoning
  that withdrew v0.1's power-good LED. No board-side obligation follows: the
  modules socket component-side up, so their LEDs already face away from the
  board.
- **D-005 — The enclosure is not a design input.** The board's only concession
  to a future case is that the three external connectors sit at board edges
  (FR-003, FR-011). Outline, mounting, cutouts and any light path are deferred
  until a case is actually built, and will adapt to the board rather than
  constrain it.
- **D-002 — Barrel jack is 2.1 × 5.5 mm, centre-positive.** 5.5 mm specifies
  only the outer diameter; the 2.1 mm centre pin must be stated because 5.5 mm
  jacks also exist with a 2.5 mm pin, and the two mismate badly — a 2.1 mm plug
  makes intermittent contact in a 2.5 mm jack, and a 2.5 mm plug will not enter
  a 2.1 mm jack at all. 2.1 × 5.5 mm is the common hobbyist standard.
- **D-003 — The power budget is designed against exactly 5.00 V.** Verification
  is by bench supply set to 5.00 V, which makes FR-018 directly measurable
  rather than aspirational. This deliberately rejects v0.1's approach of
  recommending a 5.1–5.25 V supply to buy margin: that pushes the design's
  weakness onto the builder's choice of adapter, and a stranger following the
  build will reach for a generic 5 V brick. Any input above 5.00 V is headroom,
  never a requirement.
- **D-004 — The FR-018 power budget is validated by breadboarding v0.2's
  candidate protection topology, before layout.** Not by measuring v0.1, whose
  hardware is not to hand, and not by arithmetic alone. This makes the
  design-gate protection choice an empirical result rather than an assertion,
  which is the specific failure v0.1 made when it accepted a VBUS margin nobody
  ever measured (retro finding F-8). The breadboard measurement is taken at
  5.00 V input per D-003, recorded per NFR-014, and is a **precondition for
  layout**, not a bring-up step.

## Open Questions

*None. All questions raised during brainstorm and requirements review are
resolved and recorded under Decisions above.*
