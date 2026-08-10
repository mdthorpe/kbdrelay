# kbdrelay — USB HID keyboard bridge

A transparent two-board bridge that lets any USB keyboard type into a target
device as if it were a plain USB 1.1 boot-protocol keyboard. Two cheap
single-USB-controller boards, joined by a short SPI harness.

- **KBD** — keyboard-facing board (USB **host**, SPI **master**). The keyboard
  plugs into it; it reads key events and forwards them.
- **TGT** — target-facing board (USB **device**, SPI **slave**). It plugs into
  the target machine and presents as a vanilla boot keyboard, replaying what
  KBD sends.

> Boards are named for what they face. "Sender/receiver" is banned vocabulary
> here — both boards send and receive over the bidirectional SPI link.

See [`spec.md`](spec.md) (what/why), [`plan.md`](plan.md) (how), and
[`tasks.md`](tasks.md) (build log / progress).

## Status

**v1 achieved on the bench** — typing crosses the bridge, Caps/Num Lock LEDs
round-trip, hot-plug recovers, and the whole chain was validated on a
**Commodore Amiga 1200 (1992)** via a USB keyboard adapter, with modern and
vintage keyboards. Verified: AC1–AC5 (see `tasks.md` for caveats).

## Hardware

- 2× **Waveshare ESP32-S3-Zero** (one KBD, one TGT).
- Onboard WS2812 status LED (GPIO21, **RGB** order).
- Power: KBD from a dedicated 5 V feed (bus power / USB-PD); TGT from the target
  machine's USB. **Grounds common; 5 V never merged** (FR5).
- Keyboard-power inrush needs a **bulk cap (~220–1000 µF) on KBD's 5 V**.

### SPI harness (wire same-name to same-name — SPI names encode direction)

| Signal      | KBD (master)  | TGT (slave)   | Notes |
|-------------|---------------|---------------|-------|
| SCK         | GPIO4 (GP4)   | GPIO4         | 1 MHz |
| MOSI        | GPIO5 (GP5)   | GPIO5         | key reports, heartbeats → |
| MISO        | GPIO6 (GP6)   | GPIO6         | ← LED reports |
| CS          | GPIO7 (GP7)   | GPIO7         | |
| DATA_READY  | GPIO8 (in)    | GPIO8 (out)   | TGT signals a queued LED report |
| GND         | GND           | GND           | common |

Put **~470 Ω (→2.2 kΩ recommended) in series** on each signal line — limits
backfeed between boards when one is unpowered (FR5.4) and tames ringing.

## Firmware

ESP-IDF **6.x** via **PlatformIO** (`framework = espidf`). Shared framing +
CRC lives in [`components/bridge_proto`](components/bridge_proto) (also unit-
tested on the host). One project, one env per firmware:

| env        | board role | what it is |
|------------|-----------|------------|
| `kbd`      | KBD       | USB host → reads keyboard → SPI master forwards KEY_REPORTs |
| `tgt`      | TGT       | SPI slave → replays as boot keyboard; watchdog releases keys |
| `mock_kbd` | test      | SPI-master stand-in (scaffold) |
| `mock_tgt` | test      | SPI-slave stand-in (scaffold) |
| `native`   | host      | `bridge_proto` unit tests (no hardware) |

### SPI protocol

Fixed 12-byte frames, full-duplex: `SYNC(0xA5) TYPE SEQ PAYLOAD[8] CRC8`.
Types: `IDLE`, `KEY_REPORT` (KBD→TGT), `LED_REPORT` (TGT→KBD), `HEARTBEAT`.
KBD sends on key change or every 25 ms; TGT releases all keys if no valid frame
for 75 ms (FR4.1).

## Build / flash / test

```sh
make build ENV=kbd        # compile (kbd | tgt | mock_kbd | mock_tgt)
make upload ENV=kbd       # flash over USB-C (see flashing quirks below)
make monitor              # serial monitor on the USB-UART adapter
make test                 # host unit tests (native, no hardware)
make reconfigure ENV=kbd  # clean rebuild of one env
```

Toolchain: PlatformIO Core (Python 3.13 penv) + ESP-IDF 6.x, board def
`boards/waveshare_esp32_s3_zero.json` (the official platform doesn't ship it).

### S3-Zero flashing quirks

- **Download mode:** hold **RESET + BOOT**, release RESET, pause, release BOOT.
- **After flashing, physically reset** (unplug/replug or RESET) — a software
  reset over native USB does not boot the app.
- **Logs go over UART** (TX/RX pads, 3.3 V USB-UART adapter, 115200): the USB-C
  is used by the app (host on KBD, device on TGT), and USB-Serial/JTAG shares
  the S3's USB PHY. Console is forced to UART (`CONFIG_ESP_CONSOLE_UART_DEFAULT`).
- **Power-up order on the bench:** power TGT (into the target) before KBD.

## License

TBD.
