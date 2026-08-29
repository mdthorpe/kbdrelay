# PCB Carrier Board v0.2 Brainstorm

Spec: `pcb-carrier-v0-2`  
Status: brainstorm-complete  
Created: 2026-08-29

## User Intent

## Brainstorm Summary

- **User intent:** Redesign the kbdrelay carrier board as v0.2 from a clean sheet — new schematic, nothing carried over from the v0.1 netlist or layout — targeting professional fabrication at PCBWay and reproduction by other builders, while keeping the hardware-proven Waveshare ESP32-S3-Zero modules and the v1 firmware untouched.

- **Target users/stakeholders:** Other hobbyist / retrocomputing builders who clone the repo, order boards from PCBWay, buy the BOM from online electronics retailers, and assemble a working bridge. The author is the first builder and maintainer.

- **Problem and desired outcome:** v0.1 proved the concept but is fundamentally a home-etch artifact — single-sided, hand-routed, 0.605 mm minimum copper gap, a split GND pour, hand-built footprints, a 2-pin power header, and no enclosure story. It is realistically reproducible only by its author. v0.2 should be a professionally fabbed 2-layer board living in a small 3D-printed box with three external connections (two module USB-C, one barrel jack), a sourceable BOM, and documentation a stranger can follow to a working unit.

- **In scope:**
  - New schematic drawn from scratch; the v0.1 netlist is not carried over
  - KBD 5 V protection re-derived (reverse polarity, overcurrent, inrush)
  - Barrel-jack 5 V input replacing v0.1's 2-pin header
  - 2-layer PCBWay-manufacturable layout with a continuous ground plane and vias
  - PCBWay `.kicad_dru` vendored into the repo as a project-local DRC gate
  - Free board outline; the 3D-printed enclosure is designed to fit the board afterwards
  - BOOT/RESET access via a removable lid (the modules' own buttons are the only option)
  - Internal debug UART headers retained
  - Onboard status LED(s) permitted
  - THT-only discrete components with verified, in-stock Digikey-class MPNs
  - Third-party build / assembly / bring-up documentation
  - A versioned fab package (Gerbers, drill, BOM)

- **Out of scope:**
  - Firmware changes; the S3-Zero pin mapping (SPI GP4–GP8, DATA_READY GP8, UART0 console) is fixed
  - Module substitution — the S3-Zero stays
  - Board-mounted USB connectors of any kind (physically impossible: see constraints)
  - Revisiting the FR5 5 V-isolation architecture (it holds unchanged)
  - Surface-mount parts
  - Detailed enclosure CAD (follows the board; may be a separate effort)
  - Home-etch support and its entire toolchain — LCD photoresist exposure artwork, `bridge_risk.py`, `audit_gaps.py`, the jumper budget, the `BOTTOM` copper label — all explicitly retired
  - Selling or shipping assembled units

- **Constraints/risks:**
  - **The S3-Zero exposes only 18 pins** — VDD, GND, 3V3, GPIO1–13, RX, TX. GPIO19/20 (native USB D±) are **not** broken out, so USB exists only at each module's own USB-C jack; a board-mounted USB-A receptacle for the keyboard is impossible without changing modules. EN/RESET and GPIO0 are likewise not exposed, so **board-mounted BOOT/RESET buttons are impossible** and download mode requires physical access to the modules' own buttons — hence the removable lid.
  - **Module USB-C jacks sit on the modules, not the PCB**, so both modules must sit at board edges using the footprint's 1.5 mm USB-C overhang notch, and the enclosure cutouts follow whatever the layout produces.
  - **PCBWay 2-layer 1 oz limits** (from the supplied `.kicad_dru`): 0.127 mm track width and clearance, 0.25 mm minimum annular ring, 0.3 mm track-to-edge, 0.5 mm hole-to-hole across different nets, 0.5 mm minimum non-plated hole, vias 0.3 mm hole / 0.5 mm diameter. The file is written for KiCad 7 but was verified to parse **and enforce** on KiCad 10.0.5 (canary rule test).
  - **Barrel jack has two independent failure modes.** Reverse polarity (centre-negative supply) is protectable cheaply. **Overvoltage is not** — a 2.1 × 5.5 mm plug is a mechanical standard only, so any 9/12/19 V brick fits and would destroy both modules; no THT-only protection network survives a high-current 19 V supply. Decision: protect reverse polarity, and mitigate overvoltage with silkscreen marking plus a documented PSU spec rather than parts.
  - **VBUS margin is unverified.** v0.1's loaded keyboard VBUS was never measured (retro finding F-8). Hardware is not to hand, but breadboarding is available, so the design gate can validate the power path empirically. Carried as an explicit open risk.
  - The two-MCU architecture is forced by silicon (one USB PHY per ESP32-S3), not a design choice.
  - PCBWay's minimum order quantity is a non-issue at these prices and is not a requirement.
  - The Waveshare S3-Zero is not a Digikey line item; it is **explicitly exempt** from the sourcing rule, which governs electronic components rather than dev modules.
  - The vendored S3-Zero symbol/footprint originates from a GitHub repo with no LICENSE file. Assessed by the user as not a concern; not tracked as a risk.

- **Proposed approach:** Create `.specs/pcb-carrier-v0-2/`. Re-derive requirements from the board's functional job (hold two modules; carry five SPI lines plus common ground; keep the two 5 V domains separate; feed KBD a protected external 5 V; expose debug access; keep BOOT/RESET reachable) plus the eight v0.1 retrospective findings plus the PCBWay and enclosure constraints, explicitly retiring the home-etch NFRs. The design gate then produces a fresh schematic, a re-derived protection topology chosen against an actual voltage budget, and a 2-layer layout with a continuous ground plane and edge-mounted modules, preferring stock KiCad THT footprints over hand-built ones. Vendor the PCBWay rules and gate on `kicad-cli pcb drc`. Ship a fab package and a BUILD.md written for a stranger; the enclosure STL follows the board.

- **Assumptions:**
  - PCBWay's cheapest standard service: 2-layer, 1 oz copper, board ≤ 100 × 100 mm
  - Firmware unchanged, so SPI stays GP4–GP8 with DATA_READY on GP8 and console on UART0
  - Modules remain socketed and removable rather than soldered down
  - Barrel jack is the common 2.1 × 5.5 mm centre-positive type
  - The four bench-earned facts from v1/v0.1 are deliberately carried forward as they were paid for in brownouts and latch-ups, not chosen for etching convenience: a ≥470 µF inrush bulk capacitor, series resistors on the SPI lines for backfeed/latch-up protection, no VCC pin on debug headers, and TGT's 5 V left unconnected

- **Remaining doubts:**
  - Exact reverse-polarity topology (series Schottky vs. shunt/crowbar Schottky downstream of the PPTC) is deliberately deferred to the design gate, to be chosen against a written VBUS budget rather than by assertion. The shunt arrangement is the leading candidate because it is the same part count as v0.1's series diode but has zero forward drop.
  - Loaded keyboard VBUS remains unmeasured; the design gate should either measure it on v0.1 or breadboard the candidate power path.
  - Whether the retained onboard status LED earns its place, given the modules already have firmware-driven WS2812 indicators visible through the case.

- **Suggested spec slug:** `pcb-carrier-v0-2`

## Clarifying Questions Asked

- TODO: Record key questions asked one at a time and the user's answers.

## Proposed Approach

- TODO: Summarize the recommended direction before requirements are drafted.

## Assumptions

- TODO: List assumptions accepted during brainstorming.

## Remaining Doubts

- TODO: List unresolved questions, if any, and whether they block requirements.

## Decision

- [ ] User approved creating the spec skeleton and drafting requirements.
