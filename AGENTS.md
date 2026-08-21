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
- ⚠ **Never interleave raw file edits with Konnect writes on the same KiCad
  file.** Konnect writes from a cached parse, so a later Konnect write silently
  reverts hand edits made in between (observed: footprint assignments rolled
  back to earlier values with no error). Do all Konnect edits first, then any
  raw edits, then only read-only Konnect calls — and re-read the file to confirm.
  `kicad_status restart:true` clears the cache.
- One writer per working tree.
- Record hardware findings in `~/.pi/memory-md/kbdrelay/core/HARDWARE.md` **and**
  the relevant spec/plan so they aren't lost.
- **An agent can't drive KiCad.** Help with symbols/values/BOM, review Gerber/
  print exports, draft `BUILD.md`, and reason through DRC / bring-up results.

## Current open task (PCB)

**Reverse-polarity part: SETTLED — series Schottky, not a P-FET.** Logic-level
THT POWER P-FETs proved unsourceable at hobby quantity, so `D1 = 1N5817`
(1 A / 20 V, DO-41, on hand; 1N5819 drop-in). Symbol `Device:D_Schottky`,
footprint `Diode_THT:D_DO-201AD_P12.70mm_Horizontal` — the 12.70 mm pitch is
deliberate so a lower-Vf **SB540/SB560** (DO-201AD) drops in with no respin.
Wiring: anode→+5 V in, cathode→load (+5V_KBD). The 100 kΩ gate resistor is gone.

⚠ **Cost of the swap:** ~0.3 V Vf at 0.3 A → keyboard VBUS ≈4.6 V from a 5.00 V
input (USB floor is 4.40 V). Margin is finite now. Prefer a **5.1–5.25 V** feed
at J_PWR, and bring-up **must measure loaded keyboard VBUS ≥ 4.40 V**.

**Schematic: COMPLETE and ERC-clean (0 errors).** Full capture per design.md —
power path (J3→D1→C1/C2→F1→U1.5V), SPI GP4–GP8 with 2.2 k series each, both UART
headers with 1 k series and no VCC, power-good LED, TGT 5V no-connected. Every
part has a value + footprint; `hardware/pcb-carrier/BOM.md` is generated from it.

**Approved design amendments since the first draft** (all gated individually):
1. **D1 = 1N5817 series Schottky** replaces the P-FET (sourcing).
2. **F1 = Littelfuse 60R090** (0.9 A hold / 1.8 A trip, R₁ₘₐₓ 0.47 Ω) replaces the
   0.5 A part. A 0.5 A PPTC derates to ~0.42 A at 40 °C and would nuisance-trip a
   legal 500 mA keyboard, and its 1.17 Ω pushed VBUS under the 4.40 V USB floor.
3. **Shared GP9 soft-reset removed** (J4 + R10/R11 deleted, GP9 now NC both
   modules). It never satisfied FR-030 — download mode needs the hardware
   BOOT+RESET sequence, which a firmware `esp_restart()` cannot do. FR-030 is now
   a **placement constraint**: layout must leave both modules' onboard BOOT/RESET
   buttons reachable with the modules seated.

**Spec gates:** requirements-approved, design-approved, **tasks-approved**
(`spec_validate --phase implementation` = 14/14). T-001/T-002/T-003 are done in
substance but remain unchecked pending their own "Done when" validation.

**Libraries are project-local and self-contained — no blockers left for layout.**
Both S3-Zero library files live in the repo and resolve via `${KIPRJMOD}`:

| File | Registered by | lib_id |
|------|---------------|--------|
| `kbdrelay-carrier/symbols/kbdrelay.kicad_sym` | `sym-lib-table` | `kbdrelay:ESP32-S3-Zero` |
| `kbdrelay-carrier/footprints/kbdrelay.pretty/ESP32-S3-Zero.kicad_mod` | `fp-lib-table` | `kbdrelay:ESP32-S3-Zero` |

**Source:** <https://github.com/jtomka/kicad-esp32-s3-zero> (the old `MySymbols`
library on the unmounted `/Volumes/pool-data1_root/...` was a copy of that same
symbol — verified pin-for-pin identical). ⚠ **That repo ships no LICENSE file**,
so redistribution terms are unstated; worth asking upstream before publishing.
Do not reintroduce `MySymbols`.

**Module footprint facts** (supersedes the old "≈1×9 + 1×12" note, which was
wrong): the S3-Zero is **2×9 = 18 THT pads**, 2.54 mm pitch, **15.24 mm (0.6")
row spacing**, body 18.06 × 23.53 mm plus a 1.5 mm USB-C overhang notch.
Upstream pads are 1.524 mm / 0.762 mm drill — **too small for 0.64 mm square
header pins (0.905 mm diagonal)**. In use is the forked
`kbdrelay:ESP32-S3-Zero-Socketed` at **2.0 mm pad / 1.0 mm drill**; the upstream
footprint is kept alongside for direct castellated soldering.
`kbdrelay:PPTC_Littelfuse_60R_P5.08mm` was likewise hand-built — the stock
Bourns footprint staggers its pads 1.2 mm for kinked leads, which the Littelfuse
part does not have.

## PCB layout — IN PROGRESS (T-004)

**Placement done, routing NOT started.** Board is **120 × 55 mm landscape**, per
the user's sketch (`hardware/v0.1_board_design.png`): modules at the left and
right edges rotated 90° so USB-C exits each short end (cable straight through),
components in the middle.

- U1 rot 270 anchor (1.5, 36.53); U2 rot 90 anchor (118.5, 18.47)
- Top-left: KBD UART (R6/R7 → J1). Bottom-left: power chain J3→D1→F1→U1.VDD,
  plus C1/C2 and the LED. Middle: SPI resistors. Bottom-right: TGT UART.
- Deviation from the sketch: **5V In is bottom-left, not top-left**, because
  U1's VDD/GND land on the bottom row at that end.
- DRC: 32 unconnected (no routing yet), 4 copper-edge corner artifacts,
  3 courtyard overlaps in the power cluster (fine for hand-soldered THT).

**Routing plan:** B.Cu only, 1.5 mm power / 0.7 mm signal / 0.4 mm clearance
(clearance must stay ≤0.5 mm — module pads leave only 0.54 mm between neighbours).
GND via a pour. **Expect 2 jumpers**, where GP7/GP8 hop the GP4/5/6 bundle near
U1: the two SPI bundles run in opposite directions across the middle and must
cross, but one wire link can hop several traces.

⚠ **Earlier claim that zero crossings was "provably impossible" was WRONG** — it
over-applied an annulus theorem that ignores routing *around* a module. Two
parked escape hatches if the jumper count ever matters: flip U2 to the back
layer (mirroring reverses its clockwise pin order), or reverse TGT's SPI pin
assignment in firmware. Both need the design gate reopened; see tasks.md T-004.

## KiCad tooling notes (hard-won)

- **Konnect schematic + board-file tools are file-based** (they take a path) and
  need no running KiCad. **Routing tools (`route_trace`, `add_via`, `query_traces`)
  need KiCad's IPC** — which needs pi launched with
  `KICAD_API_SOCKET=ipc:///tmp/kicad/api.sock`, since pi-kicad resolves the socket
  from the environment once at extension load. Not needed so far: tracks can be
  written directly as `(segment ...)` S-expressions.
- **`kicad-cli pcb drc` / `sch erc` are file-based** — the reliable way to check
  work without trusting the tools' own success messages.
- Close the KiCad editor for the file being edited, or whoever saves last wins.
- KiCad auto-backups live in `*-backups/` (gitignored) and have saved us once.
