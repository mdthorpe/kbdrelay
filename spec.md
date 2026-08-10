# Spec: USB HID Keyboard Bridge

**Status:** Draft v0.3 · **Level:** Functional (what/why — no implementation choices here)

## Naming

- **KBD** — the keyboard-facing board. USB host; the keyboard plugs into it.
  (Formerly "receiver.")
- **TGT** — the target-facing board. USB device; it plugs into the target
  machine and presents as a keyboard. (Formerly "sender.")

Boards are named for what they face, which never inverts. "Sender/receiver"
is banned vocabulary in this project: both boards send and receive on the
bidirectional SPI link, so those words are ambiguous.

## Overview

A transparent two-board bridge that lets any USB keyboard type into a target
device as if it were a plain USB 1.1 boot-protocol keyboard.

- **KBD** reads key events from the keyboard and forwards them over SPI.
- **TGT** replays those events to the target as a minimal boot-protocol
  keyboard.

Primary motivation: two cheap single-USB-controller boards are simpler and
more cost-effective than one board with dual USB controllers, and the split
makes each half independently developable and testable.

## Constitution (project principles)

1. Cheapest commonly available parts that meet requirements; no unused capability.
2. Each board is independently developable and testable (SPI peer can be mocked).
3. Prefer boring, well-documented libraries and stacks over clever code.
4. Artifacts stay proportional to the problem: one spec, one plan, one task list.

## Functional Requirements

### FR1 — Keyboard input (KBD)
- **FR1.1** Accept exactly one USB keyboard, plugged directly into KBD.
- **FR1.2** Keyboards that internally enumerate as a hub + HID device MUST work.
  KBD's host stack must traverse one hub level to reach the keyboard
  interface. (Many modern keyboards are internally hub devices.)
- **FR1.3** Boot-protocol keyboard support (8 modifiers + 6-key rollover) is the
  fidelity bar. NKRO, media keys, and vendor extensions are out of scope; if a
  keyboard offers boot protocol, use it.
- **FR1.4** Keyboard hot-plug MUST work: unplug/replug at any time without
  restarting either board.

### FR2 — Bridge link (SPI)
- **FR2.1** Key events are forwarded KBD → TGT over SPI.
- **FR2.2** The link is bidirectional: HID output reports (LED state — Caps/Num/
  Scroll Lock) flow TGT → KBD and MUST reach the keyboard, so lock LEDs
  reflect the target's state.
- **FR2.3** Messages carry a type identifier. v1 defines types for (a) keyboard
  input report and (b) LED output report. The framing MUST allow new types to be
  added without breaking existing ones (future: synthetic/custom key events —
  see Design Considerations).
- **FR2.4** Message integrity MUST be verifiable (e.g., checksum); corrupt
  messages are dropped, never replayed as keystrokes.

### FR3 — Keyboard output (TGT)
- **FR3.1** TGT presents to the target as the most vanilla USB low-speed
  boot-protocol keyboard possible: single interface, no composite device, no
  extra endpoints. Reference bar: works on hosts that support only USB 1.1-era
  keyboards.
- **FR3.2** Reports from KBD are replayed transparently — no remapping,
  no filtering, no reordering.

### FR4 — Failure behavior
- **FR4.1** On keyboard disconnect, SPI link loss, or KBD power loss, TGT
  MUST release all keys (send an empty report) within **100 ms**. No
  stuck keys, ever.
- **FR4.2** Either board MUST tolerate the other being unpowered or absent
  indefinitely, and recover automatically when it returns.
- **FR4.3** TGT remains enumerated on the target even when the keyboard
  or KBD is absent — it never disconnects/reconnects on the USB bus, it
  simply types nothing. (To the target: a keyboard nobody is touching. Distinct
  from FR2.4, which governs dropping corrupt messages.)

### FR5 — Power
- **FR5.1** Each board has exactly ONE 5 V source, and two sources are never
  connected together. KBD is powered from a dedicated power-only USB feed
  ("bus power"). TGT is powered solely by the target machine's USB port.
- **FR5.2** KBD MUST supply 5 V VBUS to the keyboard port, with ≥ 250 mA
  guaranteed (covers any conventional keyboard). Power is routed through the
  dev board's normal power path (onboard connector); the actual ceiling
  therefore depends on the board's power-path components and MUST be
  documented in plan.md. High-draw RGB keyboards at full brightness are
  best-effort. (Future option: USB-C PD supply and/or direct-rail feed can
  raise the budget without spec changes.)
- **FR5.3** TGT receives VBUS from the target device; this MUST NOT backfeed
  into or conflict with bus power. The target's port is never asked to power
  anything beyond TGT itself (tens of mA).
- **FR5.4** Boards share ground via the SPI harness (all grounds in the
  system are common). Driving SPI lines into an unpowered peer MUST NOT
  damage or latch up either board.

### FR6 — Latency
- **FR6.1** Added end-to-end latency (keyboard event received by KBD →
  report available to target) ≤ **10 ms**, excluding USB polling intervals
  outside our control. Human typing speed is the use case; gaming is not.

## Out of Scope (v1)

- External hubs / multiple keyboards / mouse or other HID device classes
- NKRO, media keys, consumer-control pages, vendor-specific HID
- Key remapping, macros, synthetic events (noted below as a design consideration)
- Wireless anything
- Configuration UI of any kind

## Design Considerations (non-binding, informs plan.md)

- **Future synthetic events:** a later version may inject target-specific
  combinations (e.g., Fn+Key unique to the target device). FR2.3's typed message
  framing exists so this can be added as a new message type with no protocol
  break. Do not otherwise design for it.
- **SPI direction asymmetry:** SPI is master/slave and the slave cannot initiate
  transfers. The LED backchannel (FR2.2) will need a data-ready signal line or
  polling; the spec requires the behavior, not the mechanism.

## Acceptance Criteria

- **AC1** A plain boot-protocol keyboard and a hub-type keyboard both type
  correctly into the target, including modifiers and 6-key chords.
- **AC2** Caps Lock pressed on the keyboard toggles the target's caps state AND
  the keyboard's Caps Lock LED.
- **AC3** Holding a key and yanking the keyboard (or the SPI harness, or KBD
  power) releases the key on the target within 100 ms.
- **AC4** Keyboard replug and KBD power-cycle both recover to a working
  state with no interaction on the TGT/target side.
- **AC5** TGT enumerates and types correctly on a USB 1.1-era reference
  target.
- **AC6** Each board passes its tests standalone with the SPI peer mocked.

## Open Questions

- ~~Name a specific reference target device for AC5?~~ **RESOLVED:** v1 was
  validated against a **Commodore Amiga 1200 (1992) with a USB keyboard
  adapter**, using both modern and vintage USB keyboards. If a machine of that
  era accepts TGT, the boot-keyboard descriptor is effectively universal.

## Observations (v1 bring-up)

- **Numpad on nav/symbol keys (target-side limit):** numpad-only keyboards
  produce digits with Num Lock on, but the navigation/math-symbol *Keypad*
  usages don't reach the Amiga. HID keypad keys always send Keypad usages; the
  *host* maps them to digits vs arrows per Num Lock. The bridge replays
  keycodes verbatim (FR3.2), so this is the target adapter's interpretation,
  not a bridge fault. A future synthetic-event type (see Design Considerations)
  could remap if ever desired.
