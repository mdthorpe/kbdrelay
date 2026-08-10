# Tasks: USB HID Keyboard Bridge

**Status:** Draft v0.1 · **Implements:** plan.md v0.2 / spec.md v0.3

Tasks are ordered but T0 can run in parallel with anything. Each task is
independently completable and has a concrete "done" check. FR/AC references
trace back to spec.md.

---

## T0 — Power topology verification  [VERIFY-1, VERIFY-2, VERIFY-3]

Desk research, no code. Blocks **T5 (full-chain power-up) only** — firmware
work can proceed in parallel.

- [ ] From the Waveshare ESP32-S3-Zero schematic/wiki: confirm whether the
      5V pin ↔ USB-C VBUS are directly connected (no blocking diode).
- [ ] Record the KBD keyboard-power ceiling in plan.md's Power section (FR5.2).
- [ ] Confirm the TGT bench rule ("TGT 5V pin never on the rail") is
      necessary, or relax it if a diode exists (FR5.1, FR5.3).
- [ ] Decide on series resistors (~470 Ω) for SPI lines re: unpowered-peer
      tolerance (FR5.4); note decision in plan.md.

**Done when:** plan.md Power section has no remaining [VERIFY] tags.

---

## T1 — TGT proof of concept: the 'a' typer  (FR3.1 subset, AC5 de-risk)

First firmware. No SPI, no protocol — just prove TGT enumerates as a vanilla
boot keyboard that real hosts accept.

- [ ] ESP-IDF project `tgt/` with `esp_tinyusb`, descriptor per plan.md
      Firmware Stack (single interface, boot-protocol keyboard, minimal
      strings, generic VID/PID).
- [ ] Once per second: send 'a' press report, then all-keys-up release
      report (release matters — a lone press report leaves the key held and
      typematic repeat floods the target).
- [ ] WS2812 blinks on each report pair (first use of the status LED).
- [ ] Verify on a modern PC first, then on the oldest bench machine.

**Done when:** the oldest bench machine prints one 'a' per second. This is
AC5's enumeration half proven before any bridge code exists.

---

## T2 — Repo scaffold + `bridge_proto` component  (FR2.3, FR2.4)

- [x] Repo layout per plan.md (spec/plan/tasks at root, `components/
      bridge_proto/`, `src/{kbd,tgt,mock_kbd,mock_tgt}`, `test/`).
- [x] `bridge_proto`: 12-byte frame encode/decode, CRC8 (poly 0x07), message
      types IDLE / KEY_REPORT / LED_REPORT / HEARTBEAT per plan.md.
- [x] Host-side unit tests: round-trip encode/decode, CRC rejection,
      unknown-type tolerance (forward compatibility per FR2.3), truncated/
      corrupt frame handling (FR2.4).

**Done when:** unit tests pass on the host machine with no hardware attached.
**DONE (2026-08-08):** `pio test -e native` → 6/6 pass; all four ESP-IDF envs
build and link bridge_proto; per-env app selection verified.

---

## T3 — TGT bridge firmware + standalone test  (FR3, FR4.1, FR4.3; AC3, AC6)

Extends T1's project with the SPI slave + watchdog.

- [x] SPI slave (pins per plan.md), always re-armed with a preloaded TX frame
      (LED_REPORT if LED state changed since last exchange, else IDLE).
- [x] Replay valid KEY_REPORT payloads verbatim to USB (FR3.2). Drop invalid
      frames silently (FR2.4).
- [x] Watchdog: no valid frame for 75 ms ⇒ empty HID report, all keys up
      (FR4.1). WS2812 indicates watchdog state.
- [x] Enumeration is independent of SPI state: TGT stays on the USB bus with
      no master attached (FR4.3).
- [ ] `mock_kbd` firmware (XIAO or spare Zero): SPI master that scripts
      KEY_REPORT sequences + heartbeats over the harness.
- [ ] Test: scripted typing appears on a PC; kill mock_kbd mid-hold ⇒ key
      releases within 100 ms (AC3 path); corrupt-CRC frames produce no
      keystrokes.

**Done when:** all three tests pass with TGT + mock_kbd only (AC6, TGT half).
**INTEGRATION-VERIFIED (2026-08-08):** TGT slave + replay + watchdog proven
against the *real* KBD (stronger than the mock path): end-to-end typing works,
TGT LED red→green on link up. mock_kbd standalone test still outstanding.

---

## T4 — KBD firmware + standalone test  (FR1, FR2.2; AC1/AC2 paths, AC6)

- [x] ESP-IDF project `kbd/`: USB Host Library + `usb_host_hid`, hub support
      enabled depth 1 (FR1.2), boot protocol requested (FR1.3), hot-plug via
      host events (FR1.4).
- [x] SPI master loop per plan.md rules: transact on key event or on 25 ms
      heartbeat timer (DATA_READY harvested every exchange; edge-trigger TODO).
- [x] On keyboard disconnect: send all-zeros KEY_REPORT immediately, keep
      heartbeating (FR4.2).
- [x] Deliver received LED_REPORT bitmaps to the keyboard as HID output
      reports (FR2.2).
- [ ] `mock_tgt` firmware: SPI slave logging decoded frames over UART,
      queueing scripted LED_REPORTs.
- [x] Test: plain AND hub-type keyboards enumerate and produce correct frames
      (AC1 path); unplug/replug recovers (FR1.4); LED_REPORT toggles the
      physical Caps Lock LED (AC2 path).

**Done when:** all tests pass with KBD + mock_tgt only (AC6, KBD half).
**INTEGRATION-VERIFIED (2026-08-08):** full bidirectional bridge works with the
real TGT — typing (incl. Caps Lock 0x39) crosses, Caps/Num LED backchannel
lights the physical keyboard, hot-plug recovers in any order, higher-draw
keyboards enumerate after the bulk-cap fix. Outstanding: standalone mock_tgt
test, and single-keyboard-interface selection for multi-kbd-iface devices.
**IN PROGRESS (2026-08-08):** KBD read path done on real hardware — enumerates
boot / composite / hubbed-non-boot keyboards, boot protocol requested, reports
logged over UART, hot-plug + status LED, report-descriptor keyboard detection,
change-detection dedup. Remaining: SPI master + forward KEY_REPORTs, LED_REPORT
delivery to keyboard, single-keyboard-interface selection, mock_tgt test.

---

## T5 — Integration  (AC1–AC5)  — blocked by T0

- [x] Wire the real harness per plan.md power table: KBD on bus power,
      TGT powered by the target, GND common, TGT's 5V pin untouched.
- [x] AC1: plain + hub keyboards type correctly into the target, modifiers
      and 6-key chords included.
- [x] AC2: Caps Lock toggles target state AND the keyboard's LED.
- [x] AC3: yank keyboard / harness / KBD power while holding a key ⇒
      released on target within 100 ms.
- [x] AC4: keyboard replug and KBD power-cycle recover with no action on
      the target side.
- [x] AC5: everything above on the oldest bench machine (also names the
      reference target, closing spec.md's open question).

**Done when:** AC1–AC5 all pass on the reference target. v1 complete.
**v1 ACHIEVED (2026-08-08):** full chain verified on an **Amiga 1200 (1992) via
a USB keyboard adapter** with both modern and vintage keyboards. Caveats:
(1) power-up ordering — power TGT (target) before KBD to avoid SPI backfeed
pre-charging TGT (raise series R to ~2.2k to harden); (2) numpad-only keyboards:
digits work with Num Lock on, but nav/math-symbol keypad usages don't reach the
Amiga — target-side HID interpretation limit, we replay verbatim (FR3.2).
Remaining polish: single keyboard-interface guard; optional mock firmwares (AC6
literal); 2.2k series R for permanent build.

---

## Deferred (explicitly not tasks)

- Synthetic/custom key events (spec Design Considerations — protocol slot
  reserved, nothing else).
- Schottky diode bench hardening (plan.md Power, optional).
- USB-C PD power upgrade (spec FR5.2 note).
