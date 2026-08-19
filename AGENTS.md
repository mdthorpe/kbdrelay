# AGENTS.md — kbdrelay onboarding for AI agents

Read this first. It's the fast path to being useful on this repo. (This file is
auto-loaded by pi/Claude-style agents as project context — it's the
"README.AGENT" you were looking for.)

## What this project is

**kbdrelay** — a transparent two-board USB HID keyboard bridge. Any USB keyboard
plugs into **KBD** (ESP32-S3, USB *host*), which forwards key events over a
12-byte framed **SPI** link to **TGT** (ESP32-S3, USB *device*), which replays
them to a target machine as a **vanilla USB 1.1 boot keyboard**. Bidirectional:
Caps/Num/Scroll-Lock LED state flows back TGT→KBD to the keyboard.

Naming is strict: **KBD** = keyboard-facing, **TGT** = target-facing.
"sender/receiver" is banned (both send and receive on SPI).

## Status (2026-08)

- **v1 firmware: COMPLETE and hardware-verified** — AC1–AC5 passed, including a
  real run on a **Commodore Amiga 1200 (1992)** via a USB keyboard adapter, with
  modern and vintage keyboards. Pushed to `github.com/mdthorpe/kbdrelay`.
- **PCB carrier board: IN PROGRESS** (spec-driven, see `.specs/pcb-carrier-board/`):
  brainstorm + requirements + design **approved**; `tasks.md` still `tasks-draft`;
  KiCad project started; currently selecting the reverse-polarity P-FET.

## Repo map

- **Firmware (root):** `platformio.ini`, `Makefile`, `boards/`,
  `components/bridge_proto/` (shared 12-byte framing + CRC8, host unit-tested),
  `src/{kbd,tgt,mock_kbd,mock_tgt}/main.c`, `test/`.
- **v1 design docs (root, freeform, historical):** `spec.md` (what/why, FR IDs),
  `plan.md` (how — wiring, protocol, hardware findings), `tasks.md` (build log).
- **PCB effort (spec-driven):** `.specs/pcb-carrier-board/`
  (`brainstorm|requirements|design|tasks.md`) + `hardware/pcb-carrier/`
  (KiCad `kbdrelay-carrier`, `schematic-concept.svg/png`, future `BOM.md`/`BUILD.md`).
- **Durable notes:** `~/.pi/memory-md/kbdrelay/core/HARDWARE.md` — **read this**;
  it has every hard-won gotcha and the running progress log.

## Build / test (firmware) — use the Makefile

PlatformIO lives in its **own penv** (`~/.platformio/penv/bin/platformio`,
Python 3.13 — NOT the system Python 3.14, which PlatformIO rejects). The
`Makefile` wraps it, and the host guardrails prefer `make` over raw commands:

```
make build ENV=kbd|tgt|mock_kbd|mock_tgt   # compile
make upload ENV=<env>                       # flash over USB-C
make monitor [MONITOR_PORT=/dev/cu.usbserial-XXXX]
make test                                   # host bridge_proto unit tests (native)
make reconfigure ENV=<env>                  # clean rebuild (regenerates sdkconfig)
```

Toolchain: **ESP-IDF 6.x** via PlatformIO's official `espressif32` platform.
Board def is in-repo (`boards/waveshare_esp32_s3_zero.json`). Firmware is
**unchanged** for the PCB effort (same S3-Zero pinout).

## Hardware gotchas (learned on the bench — don't relearn them)

- **Flashing an S3-Zero:** download mode = hold **RESET+BOOT → release RESET →
  pause → release BOOT**. After flashing, **physically reset** (unplug/replug or
  RESET); a software/line reset over native USB won't boot the app.
- **Console is UART0** (TX/RX pads, 3.3 V USB-UART @115200). The USB-C is the
  app's USB (host on KBD / device on TGT); USB-Serial-JTAG shares the S3 PHY so
  it can't also be the console. `CONFIG_ESP_CONSOLE_UART_DEFAULT=y`.
- **Onboard WS2812 is RGB order** (not GRB) → `LED_STRIP_COLOR_COMPONENT_FMT_RGB`.
- **SPI pin map:** SCK=GP4, MOSI=GP5, MISO=GP6, CS=GP7, DATA_READY=GP8, common
  GND. Avoid strapping pins GPIO0/3/45/46.
- **Power (FR5):** KBD from an external 5 V feed; TGT from the target only;
  grounds common, **5 V never merged**. A **bulk cap (≥470 µF)** on KBD's 5 V is
  required or high-inrush keyboards brown out at plug-in.
- **Backfeed:** the two boards share SPI signal lines; **~2.2 kΩ series R** on
  each limits backfeed/latch-up and the power-up-order pre-charge. A powered
  USB-UART debug adapter can back-power a board (dev-only artifact).
- **Keyboard detection:** identify keyboards by parsing the HID **report
  descriptor** (Usage Page 0x01 / Usage 0x06), not just the boot proto byte
  (handles composite / hubbed / non-boot keyboards); **dedup** identical reports
  (some keyboards free-run and flood).

## Working conventions

- **Spec-driven for new efforts:** `.specs/<feature>/` with gated
  brainstorm → requirements → design → tasks; **each gate needs its own explicit
  approval — never bundle**. Update the `Status:` line on approval. The PCB
  effort follows this.
- Prefer `make …` and other allowlisted commands; the host runs a guardrails
  system that blocks risky raw commands (e.g. `rm -rf` chains).
- One writer per working tree.
- Record hardware findings in `~/.pi/memory-md/kbdrelay/core/HARDWARE.md` **and**
  the relevant spec/plan so they aren't lost.
- **An agent can't drive KiCad.** Help with symbols/values/BOM, review Gerber/
  print exports, draft `BUILD.md`, and reason through DRC / bring-up results.

## Current open task (PCB)

Selecting the KBD **reverse-polarity P-FET**: needs a **logic-level POWER P-FET
in TO-220 THT** — Vgs(th) ≤ 2 V, Rds(on) ≤ ~100 mΩ @ Vgs=−4.5 V, Id ≥ 1 A,
Vds ≥ −20 V (e.g. Infineon `IPPxxP03P4L` or `NDP6020P`). **Avoid small-signal
TO-92 parts** (Supertex VP2106 / TP2104 — ohms of Rds, too lossy at ~0.3–0.4 A).
KiCad symbol `Q_PMOS_GDS`, footprint `Package_TO_SOT_THT:TO-220-3_Vertical`.
Wiring: Drain→+5 V in, Source→load (+5V_KBD), Gate→100 kΩ→GND.
