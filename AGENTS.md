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

## Status (2026-08-27)

- **v1 firmware: COMPLETE and hardware-verified** — AC1–AC5 passed, including a
  real run on a **Commodore Amiga 1200 (1992)** via a USB keyboard adapter, with
  modern and vintage keyboards. Pushed to `github.com/mdthorpe/kbdrelay`.
- **PCB carrier v0.1: SHIPPED and hardware-verified.** Home-etched, assembled,
  runs the bridge. Frozen at git tag **`v0.1-board`**. The
  `.specs/pcb-carrier-board/` spec is **CLOSED as the v0.1 record — do not amend
  it**.
- **PCB carrier v0.2: being scoped.** Read
  [`hardware/pcb-carrier/RETRO-v0.1.md`](hardware/pcb-carrier/RETRO-v0.1.md)
  first — it is the seed for the v0.2 brainstorm gate and lists the 8 numbered
  findings v0.2 exists to address. The v0.2 spec does not exist yet.

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

**v0.1 is done. Next work is the v0.2 spec**, starting from a brainstorm gate
seeded by `hardware/pcb-carrier/RETRO-v0.1.md`. Nothing in
`.specs/pcb-carrier-board/` should be edited except to correct the v0.1 record.

v0.1 close-out state: T-001–T-006 checked. **T-007/T-008 (bare-board and
populated bring-up) remain unchecked** — the work demonstrably happened on the
bench but the measurements were never written down, so the evidence is missing.
See RETRO-v0.1.md §4 for the specific numbers still needed.

### v0.1 part decisions (carried, unless v0.2 revisits them)

**Reverse-polarity part: series Schottky, not a P-FET.** Logic-level
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
headers with 1 k series and no VCC, TGT 5V no-connected. **No power-good LED** —
FR-032 was withdrawn and D2/R12 deleted. Every part has a value + footprint;
`hardware/pcb-carrier/BOM.md` is generated from it (22 refs, matches the board).

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

**Spec gates (v0.1, closed):** requirements-approved, design-approved,
tasks-approved, implementation-in-progress. T-001–T-006 checked;
T-007/T-008 unchecked for lack of recorded bench evidence.

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

## PCB layout — v0.1 AS BUILT (verified from the file, 2026-08-27)

⚠ **The board of record is HAND-ROUTED, not scripted.** The old "layout is
scripted and idempotent — do not hand-edit" rule is **retracted**: re-running
`place.py`/`route.py` would *destroy* the shipped layout, which is strictly
better than anything the router produced.

| File | Role |
|------|------|
| `kbdrelay-carrier/kbdrelay-carrier-v0.1.kicad_pcb` | **BOARD OF RECORD.** Hand-routed. 0 DRC errors. |
| `kbdrelay-carrier/kbdrelay-carrier.kicad_sch` | the schematic — one, shared by both layouts. ERC clean. |
| `kbdrelay-carrier/kbdrelay-carrier.kicad_pcb` | superseded scripted layout. ⚠ **4 courtyard-overlap DRC errors**, still carries the dropped D2/R12 LED. **Never export fab output from it.** |
| `fab/gerbers/kbdrelay-carrier-v0.1-*.gbr` | the artwork actually etched. |

As-built numbers (these supersede every earlier figure in the spec):

| Parameter | v0.1 as built |
|-----------|---------------|
| Outline | 88 × 55 mm landscape, 4× M3 corner holes |
| Copper | **B.Cu only**, 73 segments, **0 on F.Cu** |
| Track widths | 70 × **1.0 mm**, 2 × 0.8 mm, 1 × 1.5 mm |
| Pour clearance | **1.4 mm** |
| Min copper gap | **0.605 mm** (module pad rows) — below the 0.8 mm NFR-007 target |
| Module pads | 1.70 mm, 1.0 mm drill |
| Wire jumpers | **0** |
| Footprints | 22 — U1 U2 D1 F1 C1 C2 R1–R9 J1–J3 H1–H4 (**no LED**) |
| DRC | 0 violations + **1 known defect: the GND pour is split into two islands** |

Modules sit at the left and right short edges so USB-C exits each end (cable
straight through), discretes in the middle, per `hardware/v0.1_board_design.png`.
U1 rot 90 anchor (1.5, 36.53); U2 rot 270 anchor (86.5, 18.47), board-local
(board origin (104.5, 77.5) on the A4 sheet). 5V In is bottom-left, not top-left,
because U1's VDD/GND land on the bottom row at that end.

`place.py` / `route.py` are kept as the **superseded scripted path** — useful
reference for pcbnew scripting technique, not for producing a board. Run with
KiCad's bundled python
(`/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3`).
Check any board with the file-based `kicad-cli pcb drc`, which is the only
trustworthy verdict.

- Top-left: KBD UART (R6/R7 → J1). Bottom-left: power chain J3→D1→F1→U1.VDD,
  plus C1/C2. Middle: SPI resistors in **one column** at x=45, 5 mm pitch.
  Bottom-right: TGT UART (J2 + R8/R9).
- Widths and clearances: see the as-built table above. The **0.7 mm signal /
  0.4 mm clearance** figures that used to live here described the scripted
  layout and were **never fabricated** — don't quote them.

⚠ **Two impossibility claims in this spec have now been retracted.** First, the
"zero crossings is provably impossible" annulus argument (it ignored routing
around a module and under its body between the pad rows). Second, design.md
amendment 3's "zero jumpers is structural — nine signals cannot fan out of U1's
rows at 0.8 mm clearance", which a human disproved by hand-routing zero jumpers
at *wider* traces. **Pattern: when the router fails, that is a fact about the
router, not about the board.** Do not record search failures as proofs.

### Layout gotchas paid for in blood

- **Module rotation is easy to get backwards.** The original placement had U1/U2
  rotated 270/90 instead of 90/270, which put *every module pad off the board*
  (x −20…0 and 122…140 on a 0–120 board) while still looking plausible in the
  file. Always verify pad positions via pcbnew, not by eye:
  `p.GetPosition()` is authoritative.
- **A GND pour silently strands pads.** C1.2, C2.2 and U2.2 each ended up on
  isolated pour islands once the +5 V / SPI runs walled them off. `route.py`
  keeps three GND-only keep-clear channels and runs a fill-verify-repair pass;
  it fails loudly rather than shipping a stranded ground.
- **Thermal reliefs starved/isolated GND pads** on this single-sided board; the
  pour uses **solid** pad connections instead.
- **Rect pads reach √2× further than a circle of the same width.** Pin-1 markers
  are square, so a circle model under-clears their corners by up to 0.41 mm.
- **Snapping track ends to pad centres can break clearance** because it moves
  geometry outside the routing grid model — `route.py` geometry-checks each
  snapped end and falls back to the grid point.

## KiCad tooling notes (hard-won)

- **Konnect schematic + board-file tools are file-based** (they take a path) and
  need no running KiCad. **Routing tools (`route_trace`, `add_via`, `query_traces`)
  need KiCad's IPC** — which needs pi launched with
  `KICAD_API_SOCKET=ipc:///tmp/kicad/api.sock`, since pi-kicad resolves the socket
  from the environment once at extension load. Not needed so far: tracks can be
  written directly as `(segment ...)` S-expressions.
- **`kicad-cli pcb drc` / `sch erc` are file-based** — the reliable way to check
  work without trusting the tools' own success messages.
- **KiCad ships a full `pcbnew` Python** at
  `/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3`
  (KiCad 10.0.5). It is far better than Konnect for scripted layout: real pad
  positions, tracks, zones, `ZONE_FILLER`, `board.Save()`. No running KiCad, no
  IPC socket.
- ⚠ **`board.Remove()` corrupts the SWIG proxy registry** — after removing items,
  later `GetFootprints()` / `FindNet()` calls return bare `SwigPyObject`s with no
  methods. Workaround used by `place.py`/`route.py`: **delete blocks textually
  from the .kicad_pcb before `LoadBoard()`**, then only ever `Add()`.
- Konnect-written board files can carry **name-only nets** (`(net "GND")` with no
  net table). KiCad tolerates them, but you cannot author tracks against them;
  loading and re-saving through pcbnew normalises the file.
- Close the KiCad editor for the file being edited, or whoever saves last wins.
- KiCad auto-backups live in `*-backups/` (gitignored) and have saved us once.
