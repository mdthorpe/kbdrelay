# Pcb Carrier Board Brainstorm

Spec: `pcb-carrier-board`  
Status: brainstorm-complete  
Created: 2026-08-18

## User Intent

INTENT: Design a custom single PCB ("carrier board") for kbdrelay that is hobbyist-shareable: easy-to-source through-hole (THT) parts, layout simple enough for a home PCB fab (2-layer, wide traces) yet also easy to order from a PCB house. Must include proper back-power / backfeed protection so the board is safe against power-up ordering, unpowered peers, and a vintage target port.

DECISIONS (locked in brainstorm):
- Single carrier PCB hosting BOTH roles (KBD + TGT). The SPI "harness" becomes on-board traces.
- Standardize on two Waveshare ESP32-S3-Zero modules, mounted on standard 0.1" PIN HEADERS (socketed, reworkable). All SMD complexity stays inside the pre-made modules.
- Every discrete/passive component on the carrier is THROUGH-HOLE (resistors, caps, diodes, polyfuses, buttons, LEDs, connectors).
- Firmware is unchanged: identical S3-Zero pinout (SPI GP4-8, USB on module USB-C, UART0 TX/RX pads, WS2812 GPIO21).
- KBD external 5V comes in through a SIMPLE 2-PIN THT HEADER (5V + GND). The 5V source is off-board and user's choice (external USB-PD daughterboard, barrel jack + 5V wall wart, or bench supply). This header's GND is the system common ground. Confirms FR5.1: KBD's single 5V source is this header.

KEY ARCHITECTURE REALIZATIONS:
- USB data does NOT cross the carrier. The S3-Zero exposes D+/D- only on its USB-C connector, not on header pins. So each module's own USB-C is the USB port: keyboard -> KBD module USB-C (via USB-A->C adapter/cable); TGT module USB-C -> target machine. Both USB-C ports must be edge-accessible for mechanical/enclosure.
- POWER: carrier bus-power input -> [protection] -> KBD module 5V pin -> (KBD USB-C VBUS) -> keyboard. TGT is powered SOLELY by the target through its own USB-C; carrier connects only GND + the 5 SPI signals to TGT, NEVER 5V. This enforces FR5.1 (one source per rail) and FR5.3 (no backfeed into the target) by construction.
- Common ground across carrier + both modules.

PROTECTION SCOPE (THT-friendly):
- KBD 5V input: reverse-polarity protection (THT Schottky or P-FET) + bulk cap (~470uF + 0.1uF, proven necessary for keyboard inrush brownout).
- Keyboard VBUS path: current limiting / inrush handling. Prefer simple THT (polyfuse + bulk cap); a THT P-FET soft-start/load-switch is a nice-to-have. Guarantee >=250mA (FR5.2).
- Inter-board SPI traces: ~2.2k series resistors on SCK/MOSI/MISO/CS/DATA_READY (limit backfeed/latch-up with an unpowered peer, FR5.4; fixes the power-up-ordering pre-charge seen on the bench). Keep SPI <=1MHz.
- TGT 5V isolation from the carrier rail (wiring rule) = FR5.3 backfeed protection.
- BOOT + RESET access per module (buttons or pads); status LEDs; UART console headers (dev-only) optionally with ~1k series R so a plugged-in USB-UART adapter can't backfeed/sustain a board.
- USB data-line ESD (TVS array) is SMD-only in nice forms; flagged OPTIONAL / bulkier THT TVS as a tradeoff for the hobbyist goal.

SUCCESS CRITERIA:
- A 2-layer, home-etchable, all-THT-except-modules carrier that reproduces verified v1 behavior with correct back-power protection, documented well enough for a hobbyist to fab/order and assemble.

OPEN QUESTIONS (for requirements/design):
- RESOLVED - Bus-power input: a simple 2-pin THT header (5V + GND); 5V source is external/off-board (USB-PD daughterboard, barrel + wall wart, or bench). No on-board PD trigger.
- How the keyboard's USB-A connects given USB is on the module's USB-C (adapter/pigtail vs a panel USB-A that can't route through the header) - likely an external USB-A->USB-C adapter.
- Include USB ESD or omit for THT simplicity?
- Mechanical: edge access for both module USB-C ports; power switch/indicator; enclosure.
- Fab constraints to target (min trace/space, drill sizes) for home-etch compatibility.

## Clarifying Questions Asked

- TODO: Record key questions asked one at a time and the user's answers.

## Proposed Approach

- TODO: Summarize the recommended direction before requirements are drafted.

## Assumptions

- TODO: List assumptions accepted during brainstorming.

## Remaining Doubts

- TODO: List unresolved questions, if any, and whether they block requirements.

## Decision

- Pending: awaiting user approval to create the spec skeleton and draft requirements.md.
