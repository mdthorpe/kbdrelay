# Pcb Carrier Board Requirements

Spec: `pcb-carrier-board`
Status: requirements-approved
Closed: 2026-08-27 — **this spec is the v0.1 record. Do not amend it.** v0.1 is
shipped and hardware-verified (git tag `v0.1-board`). New work goes in the v0.2
spec, seeded by `../../hardware/pcb-carrier/RETRO-v0.1.md`.

Created: 2026-08-18
Brainstorm: `./brainstorm.md`

## Overview

A single through-hole "carrier" PCB that hosts two socketed Waveshare
ESP32-S3-Zero modules (one in the **KBD** role, one in the **TGT** role) and
wires them together with on-board SPI traces, power, and back-power/backfeed
protection. It replaces the hand-wired breadboard harness from v1 while running
the **unchanged v1 firmware**. This is the **DIY / home-fab build** (made for
fun): through-hole discretes only, **single-sided (single copper layer) so it
is home-etchable**. A fab-house-optimized "product" version (possibly SMD,
multi-layer, different form factor) is a **future, separate effort** and is not
a constraint here. (A PCB house could still fabricate this single-sided design
as-is, but we don't optimize for that.)

Traceability note: "FRx.y" references are the v1 functional requirements in the
root `spec.md`. This document adds board-level requirements (FR-0xx / NFR-0xx).

## Scope

### In Scope

- A **single-sided** carrier PCB (one copper layer, home-etchable; also
  orderable from a PCB house) with two 0.1" pin-header sockets for S3-Zero
  modules.
- On-board SPI interconnect (SCK/MOSI/MISO/CS/DATA_READY) + common ground.
- KBD power path from an external 5 V input, including protection.
- Back-power / backfeed protection between the two modules and toward the
  target port.
- Flashing/reset ergonomics (BOOT/RESET access) and power-good indication.
- Bill of materials and assembly/fab documentation aimed at hobbyists.

### Out of Scope

- A fab-house-optimized **"product" version** (SMD, multi-layer, integrated
  connectors, enclosure, panelization) — a future, separate spec.
- Firmware changes — v1 firmware runs unmodified (identical S3-Zero pinout).
- On-board USB connectors for the keyboard or target; USB uses each module's
  own USB-C port (D+/D- are not exposed on the module headers).
- On-board USB-PD trigger or specific 5 V source; the 5 V feed is external.
- Enclosure/mechanical design beyond ensuring USB-C ports are edge-accessible.
- Bare ESP32-S3 chip design (SMD); modules only.

## User Stories

- As a **hobbyist builder**, I want a single board I can populate with cheap
  through-hole parts and two S3-Zero modules, so that I can build kbdrelay
  without hand-wiring a breadboard.
- As a **home PCB etcher**, I want a single-sided layout with generous
  trace/space and wire-jumper links (no plated-through vias), so that I can
  fabricate the board myself with a single copper layer.
- As a **tinkerer**, I want to build this for fun with cheap through-hole parts
  and my two S3-Zero modules, so that I have a real board instead of a
  breadboard. (A polished "product" version is deferred to a future spec.)
- As a **user with a vintage target**, I want the board to never push power
  back into the target's USB port, so that I don't risk old hardware.

## Functional Requirements

Use stable IDs and testable EARS-style statements.

### Modules & interconnect

- **FR-001**: THE carrier SHALL provide two 0.1" pin-header sockets that accept
  Waveshare ESP32-S3-Zero modules, one designated KBD and one designated TGT,
  removable/reworkable.
- **FR-002**: THE carrier SHALL connect the two modules' SPI signals per the
  locked pin map — SCK=GP4, MOSI=GP5, MISO=GP6, CS=GP7, DATA_READY=GP8 — pin
  N to pin N (no crossing), matching the v1 firmware (FR2).
- **FR-003**: THE carrier SHALL tie all module grounds and the 5 V-input ground
  to a single common ground net (FR5.4).
- **FR-004**: THE carrier SHALL NOT route any USB data (D+/D-) signals; each
  module's USB-C port serves its USB role (KBD host / TGT device).
- **FR-005**: THE populated carrier SHALL reproduce verified v1 behavior
  (AC1–AC5) with the modules running unmodified firmware.

### Power (FR5)

- **FR-010**: THE carrier SHALL accept external 5 V through a single 2-pin
  through-hole header (5V, GND); this is KBD's sole 5 V source (FR5.1).
- **FR-011**: THE carrier SHALL deliver the input 5 V to the KBD module's 5V
  pin, thereby powering the keyboard's VBUS through the KBD module's USB-C, and
  SHALL support ≥250 mA continuous to the keyboard (FR5.2).
- **FR-012**: THE carrier SHALL power the TGT module solely from the target
  (via the TGT module's own USB-C) and SHALL NOT connect 5 V to the TGT module;
  only GND and the SPI signals reach TGT (FR5.1, FR5.3).
- **FR-013**: THE KBD 5 V domain and the TGT/target VBUS domain SHALL never be
  electrically connected; only ground is common (FR5.1, FR5.4).

### Back-power / backfeed protection

- **FR-020**: THE carrier SHALL include reverse-polarity protection on the 5 V
  input so that reversed input causes no damage.
- **FR-021**: THE carrier SHALL include bulk capacitance (≥470 µF plus a
  0.1 µF ceramic) on the KBD 5 V rail sufficient to prevent brownout from
  keyboard inrush at plug-in (validated failure mode in v1).
- **FR-022**: THE carrier SHALL include overcurrent protection (resettable
  polyfuse) on the keyboard VBUS path that passes ≥250 mA and typical keyboards
  but limits fault current on a short (FR5.2).
- **FR-023**: THE carrier SHALL place series resistors (target ~2.2 kΩ, value
  finalized in design) on every inter-module SPI signal line (SCK, MOSI, MISO,
  CS, DATA_READY) to limit backfeed/latch-up into an unpowered module and to
  prevent power-up-order pre-charge (FR5.4).
- **FR-024**: WHEN either module is powered while the other is unpowered or
  absent, THE carrier SHALL not allow damage or latch-up of either module, and
  the system SHALL recover when power is restored (FR4.2, FR4.3, FR5.4).
- **FR-025**: WHEN 5 V is applied to KBD while TGT is not yet powered, THE TGT
  module SHALL still be able to cold-start cleanly when subsequently powered by
  the target (i.e., KBD's driven SPI lines SHALL not hold TGT in a partial-power
  state) — the power-up-ordering issue observed in v1.
- **FR-026**: THE dev-only UART console headers SHALL be arranged so a plugged-in
  USB-UART adapter cannot back-power or sustain a module against the intended
  power state (e.g., series resistance on RX/TX; VCC pin not connected).

### Flashing, reset, indication

- **FR-030**: THE carrier SHALL provide access to each module's BOOT and RESET
  functions (buttons and/or labeled pads) to support the download-mode flashing
  sequence.
- **FR-031**: THE carrier SHALL expose each module's UART0 (TX/RX) and GND on a
  labeled header for console/debug.
- ~~**FR-032**: THE carrier SHALL provide a power-good/5 V-present
  indication.~~ **WITHDRAWN (amendment 2026-08-22).** Each module's onboard
  WS2812 already shows status under the v1 firmware, so a carrier LED is
  redundant once a module is seated. Removing D2 + R12 deletes two parts, four
  drilled holes and several tight copper features from the most congested
  corner of the board, which directly serves NFR-007.
  **Accepted cost:** there is no longer any *bare-board* power indication, so
  the T-007 rail check becomes meter-only. This is not a real loss — a meter
  reports the actual voltage, which the bring-up procedure requires anyway
  (VBUS ≥ 4.40 V), whereas an LED only reports "something is live".
- **FR-033**: THE carrier SHALL keep both module USB-C ports edge-accessible for
  cabling.

## Non-Functional Requirements

- **NFR-001** (manufacturability): THE carrier SHALL be routable as a
  **single-sided** board (one copper layer, components through-hole on the
  opposite side) so it can be etched at home, using **wire-jumper links** where
  trace crossings are unavoidable and relying on **no plated-through vias**.
  Trace/space and drill sizes SHALL be chosen for home-fab compatibility
  (specific targets set in design). Fab-house/production optimization is
  explicitly NOT a goal (deferred to the future product version).
- **NFR-006** (routability): THE number of wire-jumper links SHALL be kept low
  (target set in design) and each link SHALL be documented in the BOM/build
  notes, so single-sided assembly stays practical for hobbyists.
- **NFR-002** (cost/availability): THE design SHALL use cheap, commonly
  available components (project constitution).
- **NFR-003** (usability): THE silkscreen SHALL clearly label roles (KBD/TGT),
  the 5 V input polarity, header pin 1, BOOT/RESET, and connector functions.
- **NFR-004** (documentation): THE effort SHALL produce a BOM and build/fab
  notes sufficient for a hobbyist to fabricate/order, populate, and bring up the
  board.
- **NFR-005** (electrical margin): SPI series resistance and bulk capacitance
  SHALL be chosen so SPI still operates reliably at ≤1 MHz.
- **NFR-007** (etch robustness, amendment 2026-08-22): THE minimum copper gap
  anywhere on the board SHALL be **≥ 0.8 mm**, including between adjacent
  module pin pads, so the resist develops and the board etches clean without
  over-etching and thinning the traces. Where a 2.54 mm pin pitch cannot meet
  this with round pads, pad geometry SHALL be changed (e.g. oval pads narrow
  along the pin row) rather than accepting a tighter gap. Annular ring MAY be
  reduced to achieve this but SHALL remain sufficient to survive hand drilling
  (target ≥ 0.3 mm on the narrow axis).
- **NFR-008** (stock size, amendment 2026-08-22): THE finished board outline
  SHALL fit within **90 × 60 mm**, being a 100 × 70 mm copper sheet less a 5 mm
  working margin on every edge.

## Acceptance Criteria

- [ ] **AC-1 (FR-001..FR-005)**: A populated carrier with two S3-Zeros running
  v1 firmware reproduces AC1–AC5 (typing, Caps/Num LED backchannel, hot-plug,
  watchdog) with no hand-wiring.
- [ ] **AC-2 (FR-010..FR-013)**: Meter confirms KBD 5 V reaches the KBD module +
  keyboard VBUS, TGT's 5 V pin is isolated from the carrier rail, and the two
  5 V domains are not connected (ground is common).
- [ ] **AC-3 (FR-020)**: Applying reversed polarity to the 5 V header causes no
  damage (protection blocks/limits).
- [ ] **AC-4 (FR-021, FR-022)**: A high-inrush keyboard enumerates first try
  (no brownout); a shorted keyboard VBUS trips the polyfuse without damage.
- [ ] **AC-5 (FR-023..FR-025)**: The board works with any power-up order,
  including KBD-powered-first then TGT plugged into the target (TGT cold-starts
  cleanly); an unpowered module is not damaged by the powered one.
- [ ] **AC-6 (NFR-001, NFR-006)**: The layout is single-sided (one copper
  layer) with any crossings handled by documented wire-jumper links and no
  plated-through vias, meets the stated home-fab trace/space/drill profile, and
  exports valid fabrication artifacts (Gerbers and/or a 1:1 etch print); BOM is
  all THT (except modules).

- [ ] **AC-7 (NFR-007)**: A copper-gap audit of the finished layout reports no
  gap below 0.8 mm anywhere, including adjacent module pins, and an etched
  board shows fully cleared channels with no visibly thinned or broken traces.
- [ ] **AC-8 (NFR-008)**: The board outline measures ≤ 90 × 60 mm.

## Edge Cases

- Keyboard drawing more than the polyfuse hold current (RGB/high-power): degrade
  gracefully (fuse trips) rather than browning out the whole board; document as
  best-effort per FR5.2.
- 5 V input above/below nominal (e.g., a cheap wall wart): bulk cap + the module
  LDO tolerate reasonable ripple; note input voltage range in design.
- Only one module populated (developing one half): the board SHALL be safe and
  the populated half functional (mirrors v1 mock/standalone intent).
- Hot-plugging the keyboard and target in any order (already handled in firmware
  FR4.x; the board must not impede).

## Dependencies and Constraints

- Depends on the Waveshare ESP32-S3-Zero module footprint/pinout (5V, GND, 3V3,
  GP1–GP16, TX/RX pads) and its USB-C being the only USB access.
- Constrained by home-fabrication limits: **single copper layer**, no plated
  vias (crossings via wire-jumper links), generous minimum feature sizes, and
  through-hole-only sourcing for discretes.
- Firmware pin map (GP4–GP8 for SPI; GPIO0/3/45/46 avoided as strapping) is
  fixed; the layout must match it.

## Open Questions

- [ ] Keyboard connection ergonomics: rely on an external USB-A→USB-C adapter
  into KBD's USB-C, or provide a board pigtail? (D+/D- can't route via headers.)
- [ ] Include USB data-line ESD protection (bulkier THT TVS) or omit for THT
  simplicity? (Design decision; affects the vintage-target safety story.)
- [ ] Exact keyboard-VBUS current strategy: polyfuse alone vs a THT P-FET
  soft-start/load-switch (better inrush + short behavior).
- [ ] Home-fab profile to target (min trace/space/drill) and an acceptable
  wire-jumper-link budget — sets single-sided layout rules.
- [ ] Mechanical/enclosure expectations and board outline/mounting holes.
