# Plan: USB HID Keyboard Bridge

**Status:** Draft v0.2 · **Level:** Technical (how) · **Implements:** spec.md v0.3

Naming per spec: **KBD** = keyboard-facing board (USB host, SPI master).
**TGT** = target-facing board (USB device, SPI slave).

## Hardware

| Role | Board                          | Why |
|------|--------------------------------|-----|
| KBD  | Waveshare ESP32-S3-Zero        | Native USB OTG host; ESP-IDF host stack has external-hub support (FR1.2) |
| TGT  | Waveshare ESP32-S3-Zero (2nd)  | Native USB OTG device; TinyUSB HID keyboard; single BOM/toolchain |

(Both bridge boards are S3-Zeros. A Seeed XIAO ESP32-S3 on hand is NOT part
of the bridge — it's an optional third board for the test bench, running
mock_master/mock_slave firmware so each Zero can be tested standalone
without reflashing its partner. Same chip; the mock firmware differs only
in pin assignments.)

Both Zeros' onboard WS2812 LED (GPIO21) serves as a status indicator:
KBD = keyboard enumerated / hub traversed / error; TGT = SPI alive /
watchdog tripped.

Wiring harness between boards (6 wires):

| Signal      | KBD (master)  | TGT (slave)    | Notes |
|-------------|---------------|----------------|-------|
| SCK         | GPIO4 (GP4)   | GPIO4 (GP4)    | 1 MHz to start; plenty for FR6.1 |
| MOSI        | GPIO5 (GP5)   | GPIO5 (GP5)    | key reports, heartbeats → |
| MISO        | GPIO6 (GP6)   | GPIO6 (GP6)    | ← LED reports |
| CS          | GPIO7 (GP7)   | GPIO7 (GP7)    | |
| DATA_READY  | GPIO8 (input) | GPIO8 (output) | TGT asserts when it has an LED report queued (FR2.2) |
| GND         | GND           | GND            | Common ground (FR5.4) |

Wire same-name to same-name, straight across (SPI names already encode
direction — do NOT cross MOSI/MISO). Avoided GPIO3 (an ESP32-S3 strapping pin).

**Series resistors (ADOPTED, not optional):** put ~470 Ω in series on each
signal line (SCK, MOSI, MISO, CS, DATA_READY) at the driving end. This protects
GPIOs when one board is powered and the other isn't (FR5.4, closes the
unpowered-peer concern from [VERIFY-3]) and tames breadboard ringing. Include
them before soldering a permanent version. GND is a direct connection (no R).
The v0.1 board fits **2.2 kΩ**; keep that value (see below).

**CS is open-drain (ADOPTED, 2026-09-17 — series resistors are NOT enough):**
series resistance limits backfeed *current* but does not block the DC path into
an unpowered peer's ESD clamps. Of the five signals, **CS is the only one that
idles HIGH**, so it injects continuously; SCK and MOSI idle low and only inject
for the 384 µs a frame is in flight (~1.5 % duty). Measured: with KBD powered
and TGT unpowered, CS alone parked TGT's 3V3 rail at **1.68 V** — above the POR
release threshold and below the flash's minimum, so TGT came out of reset into a
hung boot. Fix, firmware-only, no BOM change:

- **KBD** drives CS by hand as `GPIO_MODE_OUTPUT_OD` (`spics_io_num = -1`),
  asserting low around each transfer with 2 µs setup/hold. Open-drain can only
  sink, so it cannot inject.
- **TGT** supplies the idle-high level from **its own rail** via
  `gpio_set_pull_mode(PIN_CS, GPIO_PULLUP_ONLY)` (~45 kΩ internal). An
  unpowered TGT therefore presents 0 V at CS and there is nothing to feed back.

Keep the 2.2 kΩ in place: with open-drain CS it is no longer in the rise path
(that is TGT's pull-up into its own pin capacitance, ~200 ns), the fall gives
3.3 × 2.2/(2.2+45) ≈ **150 mV** at TGT's pin, and the resistor still limits
fault current and the reverse path. ⚠ Do **not** solve this by raising the
series resistance — see the failed 22 kΩ experiment below.

**Reverse path, measured 2026-09-17 (benign, but characterise it).** The fix
moves a small current the other way: TGT powered from the Amiga, KBD's 5 V off,
TGT's internal pull-up feeds CS → 2.2 kΩ → KBD's clamps. KBD's 3V3 floats at
**0.9–1.1 V**, wandering because an undriven rail has no defined sink. Lifting
CS collapses it to 0 V over ~1 s (decoupling discharging through the module's
~19 kΩ leakage), confirming CS is the *only* path in this direction too.

That current is ~2–3× what a nominal 45 kΩ pull-up should deliver, so the S3's
internal pull-up is well below its nominal value on these parts. **Verdict:
harmless** — with everything connected to a real A1200, cycling KBD's 5 V
(off / 1 s / on, i.e. starting from the pre-charged state) booted and linked
**5/5**, and cycling the Amiga likewise **5/5**.

⚠ **Rejected: bleeder resistors.** 10 kΩ from 3V3 to GND on both boards was
tried to give the injected current a defined sink. It **did not move the
voltage** — still ~1 V with and without — because the source is stiffer than
modelled. Removed. Fix the source, not the sink. (Also: do not trust in-circuit
resistance readings on these rails. The LDO, ESD diodes and bulk caps make them
nonlinear and polarity-dependent — the same node read 6.6 kΩ then 3.7 kΩ.)

**v0.2 option, not required:** replace TGT's loosely-specified internal pull-up
with an explicit **100 kΩ** from CS to TGT's 3V3 for a deterministic ~17 µA.

- KBD is SPI **master** (it produces the frequent traffic). TGT is slave.
- The keyboard plugs into KBD's USB-C port; TGT's USB-C port plugs into the
  target.
- **Consequence:** with both USB-C ports occupied by their jobs, routine
  flashing and debug consoles go over UART pins (see Dev & Debug).

## Power (FR5)

**The one rule: every 5 V rail has exactly one source. Grounds all connect;
5 V never merges.**

| Board | 5 V source                       | On the breadboard rail? |
|-------|----------------------------------|-------------------------|
| KBD   | Bus power (power-only USB feed) into its 5V pin | 5 V + GND |
| TGT   | The target machine's USB port    | **GND only** — never 5 V |

- **[VERIFY-2] RESOLVED (hardware):** the 5V pin feeds USB-C VBUS directly —
  keyboards plugged into KBD enumerate and draw power with no board mod, so
  bus power in the 5V pin flows straight out to the keyboard. The onboard LDO
  (ME6217C33M5G, 3.3 V, 800 mA) powers only the ESP32; keyboard current
  **bypasses** it, so the 800 mA figure is NOT the keyboard ceiling — the
  bench/bus supply + wiring + bulk cap are. Current ceiling is supply-limited
  (FR5.2 ≥250 mA easily met).
- **Bulk cap REQUIRED on KBD 5V (hardware finding):** simpler keyboards' inrush
  at plug-in sags the shared 5 V rail enough to dip the LDO input and trip the
  ESP32 brownout detector (`BOD: Brownout detected` → CHECK_ADDR enum failure).
  Fix: ~220 µF (or larger) electrolytic + 0.1 µF across 5V↔GND at the board,
  a stiff 5.0 V supply at ≥1 A, and short/thick power leads. Keyboards with
  their own regulator/caps (e.g. hub keyboards) mask this; bench-simple ones
  expose it.
- The same direct connection is exactly why TGT's 5V pin must stay OFF the
  rail: it would hard-wire the target's VBUS to bench power, two supplies
  fighting, with backfeed risk into a vintage USB port (FR5.3).
  **[VERIFY-1]** Confirm the same schematic detail before first full-chain
  power-up.
- **Power-up order was order-dependent, and is no longer (2026-09-17).** Before
  the open-drain CS fix, powering KBD first and then plugging TGT left TGT dead.
  Two distinct mechanisms, both traced to the CS pre-charge above:
  - *On a crude 5 V source* (phone charger, A-to-C, and by extension the
    A1200's linear supply) the surge is absorbed and the board boots anyway —
    which is why v0.1 appeared to work. "It boots because the supply is too
    crude to object" is luck, not margin.
  - *On a strict USB Type-C source* (MacBook, C-to-C) the pre-charged die draws
    a crowbar surge on the VBUS ramp that trips the port's OCP; it retries
    forever and VBUS never exceeds ~1 V. **A MacBook over C-to-C is therefore
    the regression rig for this class of bug** — it fails deterministically on
    defects every other supply hides.
- ⚠ **Raising the series resistors is NOT the fix (failed experiment,
  2026-09-17).** 22.2 kΩ on all five lines dropped the pre-charge from 1.68 V to
  420 mV and did fix the power-up problem — and broke the link completely. SCK
  at TGT's pin measured **1 V low / 2.8 V high**: the RC never settles, and 1 V
  is above the S3's ~0.83 V V_IL, so the slave never sees a clock edge at all.
  Dropping SPI to 250 kHz did not rescue it. ESP32-S3 GPIOs have no Schmitt
  hysteresis, so slow edges are doubly hostile. Fix the *path*, not the current.
- **Bench workflow:** while developing TGT with no target attached, power it
  via its USB-C from any charger. The rail is safe for TGT only while its
  USB-C is empty — which is a rule someone forgets at 11pm, hence the
  standing rule instead. (Optional hardening: a Schottky diode from rail →
  TGT 5V pin makes simultaneous connection safe; not needed for v1.)

## Firmware Stack

- **Framework:** ESP-IDF **v6.x** (6.0.1) for BOTH boards, built via
  PlatformIO (official `espressif32` platform, `framework = espidf`). Not
  Arduino — the external-hub support (FR1.2) lives in ESP-IDF's USB Host
  Library (`CONFIG_USB_HOST_HUBS_SUPPORTED`), which is more mature in 6.x, and
  staying in one framework keeps the shared protocol component trivial.
  Toolchain: Python 3.13 + PlatformIO Core 6.1.x. USB Host / `esp_tinyusb`
  APIs to be validated against 6.x during firmware tasks.
- **KBD:** ESP-IDF USB Host Library + `usb_host_hid` class driver
  (ESP Component Registry). Hub support enabled with depth 1. Request boot
  protocol from the keyboard's HID interface (FR1.3). Hot-plug via host
  library device events (FR1.4).
- **TGT:** `esp_tinyusb` HID device. Descriptor: single interface,
  boot-protocol keyboard, one IN endpoint + control, bcdUSB 1.1-compatible
  full-speed, generic VID/PID, minimal strings (FR3.1).
- **Shared component:** `bridge_proto` — frame encode/decode + CRC, compiled
  into both firmwares and into host-side unit tests (Constitution #2).

Repo layout (Option A: one PlatformIO project, one env per firmware):

```
kbdrelay/
├── spec.md / plan.md / tasks.md
├── platformio.ini               # envs: kbd, tgt, mock_kbd, mock_tgt, native
├── CMakeLists.txt               # IDF project (pins PROJECT_VER)
├── boards/waveshare_esp32_s3_zero.json   # board def (not in official platform)
├── sdkconfig.defaults           # shared IDF config (USB-JTAG console)
├── components/bridge_proto/     # shared framing + CRC (IDF component + PIO lib)
│   ├── include/bridge_proto.h
│   ├── bridge_proto.c
│   ├── CMakeLists.txt           # idf_component_register
│   └── library.json             # lets the native test env consume it
├── src/                         # one subdir per firmware; app chosen per-env
│   ├── CMakeLists.txt           # SRCS ${KBDRELAY_APP}/main.c, REQUIRES bridge_proto
│   ├── kbd/main.c               # keyboard-facing (USB host, SPI master)
│   ├── tgt/main.c               # target-facing (USB device, SPI slave)
│   ├── mock_kbd/main.c          # SPI-master stand-in (test TGT standalone)
│   └── mock_tgt/main.c          # SPI-slave stand-in (test KBD standalone)
└── test/test_bridge_proto/      # Unity host tests (`pio test -e native`)
```

Build notes (PlatformIO + ESP-IDF):
- ESP-IDF ignores `build_src_filter`; the firmware an env builds is selected
  in `src/CMakeLists.txt` via `-DKBDRELAY_APP=<name>` set per env in
  `platformio.ini` (`board_build.cmake_extra_args`).
- `components/bridge_proto` doubles as an ESP-IDF component (auto-discovered)
  and a PlatformIO library (via `library.json` + `lib_extra_dirs = components`
  in the `native` env), so the same source is unit-tested on the host and
  linked into firmware (Constitution #2 / AC6).
- Build: `pio run -e kbd` (or `tgt`/`mock_kbd`/`mock_tgt`). Test: `pio test -e native`.

## SPI Protocol (FR2)

Fixed-size 12-byte frames, full-duplex: every transaction exchanges one frame
in each direction simultaneously. Fixed size sidesteps ESP32 SPI-slave
variable-length quirks and makes the slave's preloaded TX buffer trivial.

```
Byte 0     SYNC   (0xA5)
Byte 1     TYPE
Byte 2     SEQ    (wrapping counter, debug aid)
Bytes 3-10 PAYLOAD (8 bytes, zero-padded)
Byte 11    CRC8   (poly 0x07 over bytes 0-10)
```

Message types (FR2.3 — new types append without breaking old):

| Type | Name        | Direction | Payload |
|------|-------------|-----------|---------|
| 0x00 | IDLE        | both      | none — the "nothing to say" frame |
| 0x01 | KEY_REPORT  | KBD → TGT | 8-byte boot keyboard report, verbatim |
| 0x02 | LED_REPORT  | TGT → KBD | byte 0 = HID LED bitmap |
| 0x03 | HEARTBEAT   | KBD → TGT | none |
| —    | (reserved)  |           | future synthetic events per spec |

Rules:
- CRC failure or bad SYNC ⇒ frame dropped silently (FR2.4). No retry in v1;
  the next report or heartbeat self-heals state.
- Master (KBD) initiates a transaction when: (a) it has a KEY_REPORT, (b) the
  DATA_READY line is asserted, or (c) 25 ms elapsed since last transaction
  (HEARTBEAT). Every transaction also harvests whatever TGT preloaded,
  so LED reports ride along even without DATA_READY edge detection working.

## Failure Behavior (FR4)

- **TGT watchdog:** if no valid frame (any type) arrives for **75 ms**,
  send an empty HID report — all keys up (FR4.1's 100 ms bound with margin).
  Covers SPI harness loss, KBD power loss, KBD crash.
- **KBD power-loss watchdog — VERIFIED (with a bench caveat):** the watchdog
  fires only once KBD *stops* sending. On real power loss (only the SPI harness
  attached) KBD dies promptly, TGT goes red-blink within ~75 ms and releases
  all keys — AC3 satisfied. **Bench caveat:** the USB-UART **debug adapter**
  (powered by the dev laptop) backfeeds KBD via its VCC and/or TX/RX lines,
  keeping KBD alive after the bench supply is cut, which *masks* the watchdog
  and looks like a stuck key. It's a dev-only artifact (no adapter in the
  product). For power-loss testing, unplug the adapter; or add ~1 kΩ in series
  on the adapter TX/RX lines. (Earlier "keyboard VBUS capacitance" theory was
  wrong — cap size made no difference; the adapter was the culprit.)
- **KBD:** on keyboard disconnect event, immediately send a KEY_REPORT
  of all-zeros (belt and suspenders on top of the watchdog), keep
  heartbeating, resume on re-enumeration (FR1.4, FR4.2).
- **TGT USB presence:** TGT's enumeration on the target is independent of
  SPI/KBD state — it enumerates at power-on and stays (FR4.3). An SPI slave
  with no master clocking it is inert by nature. FR5.4 caveat: ESP32 GPIO
  tolerance of driven lines while unpowered is NOT guaranteed —
  **[VERIFY-3]** check latch-up guidance; mitigation is series resistors
  (~470 Ω) on SPI lines, cheap insurance either way.

## Latency Budget (FR6)

Keyboard interrupt IN poll (host stack) → SPI transaction (≈0.1 ms at 1 MHz)
→ TGT queues report on next USB frame (≤1 ms full-speed). Bridge-added
latency well under 10 ms; dominated by USB polling on both ends, as expected.

## Dev & Debug

- The S3-Zero has no separate USB-UART bridge chip — its USB-C goes to the
  S3's native USB (Serial/JTAG). Since both USB-C ports are occupied by
  their day jobs (and the S3's OTG and Serial-JTAG share one PHY anyway),
  routine flashing and log consoles go over UART pins via one external
  USB-UART adapter (TX/RX/GND).
- First-flash can still use each board's USB-C before the harness exists.

### S3-Zero flashing quirks (VERIFIED on hardware)

The S3-Zero's native USB does **not** honor a software/line reset the way a
USB-UART bridge board does. Two manual steps are required:

- **Enter upload/download mode before flashing:** hold **RESET + BOOT**,
  release **RESET**, pause, then release **BOOT**. (esptool's auto-download
  sometimes works, but this manual sequence is the reliable method.)
- **After flashing, the app will not auto-run** — `esptool` "hard reset via
  RTS" does not boot the app over native USB, and it can even leave the chip
  "waiting for download". **Physically unplug/replug (or press RESET)** to
  run the firmware.
- **Serial console over USB-C:** because it's native USB Serial/JTAG (not a
  UART bridge), set `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` to see logs over
  the USB-C cable, and open the monitor with `monitor_dtr = 0` /
  `monitor_rts = 0` so opening the port doesn't reset the chip into download.
  A host-side reset re-enumerates the USB device, dropping the connection.

Toolchain smoke test PASSED: hello_world (USB-JTAG log + WS2812 blink on
GPIO21) compiled, flashed, and ran on a real S3-Zero.

## Test Strategy (Constitution #2, AC6)

1. **bridge_proto unit tests** — encode/decode/CRC on host machine, no
   hardware.
2. **TGT standalone:** a `mock_kbd` firmware (on the XIAO or the other Zero)
   acts as SPI master injecting scripted KEY_REPORTs. Verify the target (or
   any PC) sees keystrokes; verify watchdog by killing the mock (AC3 path).
3. **KBD standalone:** a `mock_tgt` firmware acts as SPI slave, logging
   decoded frames over UART and queueing LED_REPORTs. Verify plain + hub
   keyboards enumerate and reports flow (AC1 path); verify LED frames reach
   the keyboard (AC2 path).
4. **Integration:** full chain against the oldest bench machine (AC1–AC5).

## Risks / Open Verifications

- **[VERIFY-1]** S3-Zero VBUS ↔ 5V-pin topology, TGT side: confirm TGT must
  be excluded from bus power (schematic check, before first full-chain
  power-up).
- **[VERIFY-2] RESOLVED:** 5V pin feeds keyboard VBUS directly (keyboards work);
  ceiling is supply-limited, not the 800 mA LDO. Bulk cap on 5V required for
  inrush (see Power section).
- **[VERIFY-3] RESOLVED (adopting fix):** ~470 Ω series resistors on all five
  SPI signal lines — limits backfeed between boards when one is unpowered/
  browned-out (the two chips' GPIOs are otherwise hard-wired through the
  harness). See wiring table.
- **Risk (HANDLED, verified on hardware):** some keyboards put HID behind
  interface 1+ or expose only non-boot descriptors (sub_class=0/proto=0). KBD
  now identifies keyboards by parsing each interface's HID report descriptor
  for the Generic Desktop/Keyboard usage (0x01/0x06), not just the boot proto
  byte; non-keyboard interfaces (mouse, pointing, consumer) are closed/ignored.
  Verified with a boot keyboard, a composite keyboard+mouse, and a hubbed
  non-boot keyboard (the hub path also exercises FR1.2).
- **Report floods / free-running keyboards:** some report-protocol keyboards
  ignore Set Idle(0) and stream the held-key report every poll. KBD applies
  software change-detection (only act when a report differs from the previous
  one for that interface), which also matches the bridge's need to forward
  only real changes.
- **T4 note — multiple keyboard interfaces:** some keyboards expose more than
  one keyboard-usage interface (e.g. 6KRO + NKRO). Observed one such device
  emitting only on iface 2, but for SPI forwarding T4 must pick/prefer a single
  keyboard interface (boot interface when present) to avoid double-typing.
- **Console vs USB (verified):** the S3's USB-Serial/JTAG and USB-OTG share one
  PHY, so kbd (USB host) and tgt (USB device) cannot use USB-C for logs. Both
  force `CONFIG_ESP_CONSOLE_UART_DEFAULT=y`; logs go to UART0 (TX/RX pads) via a
  3.3 V USB-UART adapter.
- **Risk:** ESP32 SPI slave setup latency (slave must arm a transaction
  before master clocks). Mitigated by fixed-size frames + slave always
  re-arming immediately; heartbeat cadence gives 25 ms slack.
