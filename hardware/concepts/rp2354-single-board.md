# Concept: single-board RP2354 kbdrelay

**Status: CONCEPT / SCRATCH — PARKED. Not a spec. No gates, no approvals, not
tracked in `.specs/`.**

> **Parked 2026-09-XX.** Assorted RP2350 dev boards ordered (see §7) to try the
> firmware path. Pick this back up when they arrive — start at §8 step 1.
> The active hardware effort remains `.specs/pcb-carrier-v0-2/`.
 This is a "what would we build with no inherited requirements"
sketch from a discussion on 2026-09-XX. Nothing here supersedes
`.specs/pcb-carrier-v0-2/`, which remains the real, active effort.

---

## 1. Premise

Throw away every constraint the current design inherited — two boards, an SPI
link, THT parts, home etching, the ESP32-S3 — and ask what the device actually
wants to be. Answer: **one chip, one board, no inter-board protocol.**

The ESP32-S3 forced the two-board architecture because it has a single USB OTG
controller: it can be host *or* device, not both. The RP2040/RP2350 family has a
hardware USB controller **plus** PIO, and `pico-pio-usb` bit-bangs a second
full-/low-speed USB port out of PIO state machines. One chip, two USB ports, no
bridge protocol at all.

### What this deletes

| Gone | Why |
|---|---|
| Second MCU | one chip has both PHYs |
| SPI link (SCK/MOSI/MISO/CS/DATA_READY) | no inter-board comms |
| 12-byte framing + CRC8 | ditto |
| 2.2 kΩ series resistors ×5 | ditto |
| Open-drain CS backfeed workaround | one rail, no peer to backfeed |
| Power-sequencing failure class entirely | one rail |
| Reverse-polarity Schottky D1 + its unmeasured ~0.3 V Vf | USB-C can't be reversed |
| PPTC F1 | replaced by an active current-limit switch |
| 470 µF bulk inrush cap | replaced by the switch's soft-start |
| UART console + BOOT/RESET flashing ritual | UF2 mass-storage bootloader |

### What it costs

A firmware rewrite, against a different USB stack, replacing code that is
verified on real hardware including a 1992 Amiga 1200. The bridge logic itself
is trivial (TinyUSB host HID → shared report buffer → TinyUSB device HID, ~300
lines), but the **descriptor-parsing keyboard detection** and **report dedup**
logic were paid for in bench time and have to be re-earned.

---

## 2. Core architecture (shared by both variants)

```
                  ┌──────────── RP2354A ────────────┐
 USB-A recept.    │                                 │   captive USB-A
 (keyboard)  ─────┤ PIO-USB host      native USB dev ├────── pigtail
   D+/D-          │ (2 consecutive     (hardware PHY)│       (target)
                  │  GPIO, 27R + 15k)               │
                  └─────────────────────────────────┘
```

**PHY assignment is deliberate and non-negotiable:** the *target* side gets the
hardware USB controller, because that is the side that must present as a
bulletproof vanilla USB 1.1 boot keyboard to a 1992 machine via an adapter. The
*keyboard* side gets PIO-USB, because that peer is swappable and observable.

**MCU: RP2354A** — RP2350 with 2 MB flash stacked in package. Drops the external
QSPI flash IC and its four routed traces. Dual Cortex-M33, 12 PIO state
machines. RP2040 + W25Q16 works identically and is ~$0.30 cheaper; RP2354A is
chosen for fewer parts and because learning the current chip is worth more than
learning the five-year-old one.

**Signal integrity:** this is USB **full speed, 12 Mbps**, rise times ~4–20 ns.
Controlled-impedance 90 Ω differential pairs and a 4-layer stackup are
*optional practice*, not a requirement. Every cheap USB dongle on earth is
2-layer. Keep pairs short, roughly matched, over unbroken ground, and stop
worrying. Hand-soldered pigtails are electrically fine at FS.

**Common parts:**

| Ref | Part | Note |
|---|---|---|
| U1 | RP2354A (QFN-60, 7×7) | 2 MB stacked flash |
| Y1 | 12 MHz crystal, 3225 | PIO-USB needs a 120/240 MHz sysclk multiple |
| U2 | AP2112K-3.3 LDO | MCU only, ~50 mA |
| U3 | TPS2051C / MIC2005 | keyboard-port current limit, soft-start, FAULT |
| D1,D2 | USBLC6-2SC6 ×2 | ESD, both ports |
| D3 | WS2812B | status; light pipe in the case |
| SW1,SW2 | BOOTSEL, RESET | pinholes in the case bottom |
| J1 | USB-A receptacle | keyboard |
| — | Tag-Connect TC2030 pads | SWD, underside |
| — | 2× UART test pads | console via CH340 dongle |
| — | 0402 passives | |

**Debug:** BOOTSEL gives UF2 drag-and-drop flashing over the native USB port —
unplug from the target, plug into a laptop, hold one button, drag a file. No
UART dance, no physical power-cycle. The USB CDC console is **not** available,
because the native port is busy being a keyboard and adding a composite CDC
interface would break the "vanilla boot keyboard" contract the Amiga adapter
depends on. Hence the UART pads, and hence SWD being worth the footprint —
stepping through TinyUSB enumeration beats printf by a wide margin.

**GPIO budget** is not close to tight: 2 for PIO-USB D+/D−, 1 VBUS sense, 1
TPS2051 EN, 1 FAULT, 1 WS2812, 2 UART, 1 mode strap, SWD dedicated. Break out 8
spare on a castellated edge or 1.27 mm header anyway.

---

## 3. Variant A — externally powered puck

**The chosen primary concept.**

### Power

```
USB-C 5V (5.1k CC pulldowns) ─┬─ AP2112K-3.3 ── RP2354A
                              └─ TPS2051C ───── USB-A VBUS (keyboard)

target VBUS ── divider ── GPIO   (SENSE ONLY, zero draw)
target D+/D-/GND ─────────────── native USB
```

The target side is a **self-powered USB device**: it takes D+/D−/GND and senses
VBUS through a divider, drawing exactly nothing from the target. Descriptor
reports self-powered, `bMaxPower = 0`.

⚠ **The one rule:** do not assert the D+ pull-up until VBUS is detected. Assert
it early and you back-power the host's bus through the pull-up, and the target
sees a phantom device while it's powered off. RP2350's native USB has VBUS
detect and TinyUSB handles this — it's a config item, not a design problem.

This is what makes the entire v0.1 backfeed saga (the 1.68 V latch, the
open-drain CS fix, the rejected bleeder resistors) structurally impossible:
there is one rail, and the target contributes nothing to it.

USB-C input over a barrel jack because it can't be inserted backwards (deleting
D1 and its Vf penalty), every phone charger is a valid supply, and 5.1 kΩ CC
pulldowns let you read the advertised current if you ever care.

Keyboard-port current limit can be set generously (~1.2–1.5 A) since the supply
is external. The TPS2051C's controlled slew-rate soft-start is the *correct*
fix for the high-inrush-keyboard brownout that the 470 µF cap was papering over,
and its FAULT pin gives a deterministic failure: LED goes red, log says
"keyboard exceeded limit", firmware can retry by toggling EN.

### Form factor

~45 × 30 × 12 mm block, two-part 3D-printed case, four M2 heat-set inserts.

- **Front edge:** USB-A receptacle (keyboard).
- **Rear edge:** captive USB-A male pigtail to target, plus USB-C power jack.
  Both cables leave rearward; the keyboard sits clean in front.
- **Front face:** RGB LED behind a light pipe.
- **Bottom:** BOOTSEL/RESET pinholes, SWD pads.

Silhouette matches the period-correct Amiga/PS2/ADB adapter dongles, which is
half the appeal. It reads as a piece of kit, not a dev board in a box.

**Pigtail assembly is self-service** — PCBWay won't attach cables. Put a 4-pad
2.54 mm footprint at the board edge with two slots either side for a zip-tie
strain relief; cut the end off a USB-A extension cable and solder
D+/D−/VBUS-sense/GND. A 4-pin JST-PH hidden inside the case is the detachable
alternative. Budget ~10 minutes and one moment of regret.

### Cost

BOM ~$4.50. Five assembled boards from PCBWay including setup/stencil/
engineering: **$120–150 total**, i.e. $25–30 each. Setup fees dominate at this
quantity; at qty 50 it'd be $10–15 each.

### Trade-off

Three cables. That's the honest cost of the always-externally-powered choice —
it buys total immunity from the power-sequencing bug class and unlimited
keyboard headroom, and it pays in desk clutter and a board that's dead without
a brick.

---

## 4. Variant B — 500 mA bus-powered stick

**Worth building too. Prettier object, tighter constraints.**

Drop the USB-C jack entirely. The whole device runs from the target's VBUS.

### Power

```
target VBUS ─┬─ AP2112K-3.3 ── RP2354A   (~40 mA)
             └─ TPS2051C ────── USB-A VBUS (keyboard, limit ~400 mA)
```

Now a **bus-powered** device: descriptor declares `bMaxPower = 250` (500 mA),
and it must draw ≤100 mA until configured — which it does comfortably, since
the bridge itself is ~40 mA and the keyboard port stays disabled (TPS2051 `EN`
low) until enumeration completes. That gating is mandatory, not optional.

### Power budget

| Load | Current |
|---|---|
| RP2354A @ 120 MHz + PIO-USB | ~35–40 mA |
| WS2812B (dim / single colour) | ~5–20 mA |
| Regulator + leakage | ~5 mA |
| **Available for keyboard** | **~430 mA** |

Reality check on what that covers:

| Device | Typical draw | Works? |
|---|---|---|
| Vintage / membrane keyboard | 20–50 mA | yes, trivially |
| Modern mechanical, no backlight | 50–100 mA | yes |
| Single-colour backlit | 100–250 mA | yes |
| RGB gaming keyboard | 300–500 mA | marginal to no |
| Keyboard with integrated USB hub | 500 mA+ | no |
| Game controller with rumble | 500 mA spikes | no |

So: **~80% of devices, and the excluded 20% is knowable in advance.** Set the
TPS2051C limit to ~400 mA with its ILIM resistor. A device over budget trips the
switch, FAULT asserts, LED goes red — a clean refusal at plug-in, not a
brownout thirty minutes into a session. That deterministic failure mode is the
entire reason the active switch is worth $0.40 over a PPTC.

**Open risk:** the target must actually supply 500 mA. A modern PC will. A 1992
A1200 via a USB-to-Amiga adapter is an unknown — depends entirely on that
adapter's regulator, and the A1200's own supply is a beefy linear unit so the
upstream side is probably fine. **This is the thing to measure first if Variant
B gets built.**

### Form factor

~35 × 18 × 10 mm — genuinely a USB stick. USB-A receptacle one end, captive
USB-A male pigtail the other (or a board-edge male plug, which needs a 2.0–2.4
mm board or a shim and turns it into something rigid hanging off the machine —
the pigtail is more practical). Two cables total, no brick, nothing to plug in.

### Cost

BOM ~$3.80. Same assembly economics as A.

### Trade-off

More elegant object, one fewer cable, no wall wart — and a device that silently
excludes a whole class of modern keyboards, plus an unverified assumption about
what the target port can source.

---

## 5. Variant comparison

| | A: externally powered | B: bus-powered |
|---|---|---|
| Cables | 3 (keyboard, target, power) | 2 |
| Size | ~45 × 30 × 12 mm puck | ~35 × 18 × 10 mm stick |
| Keyboard budget | ~1.2 A, essentially unlimited | ~430 mA |
| RGB / hub keyboards | yes | no |
| Works without a brick | no | yes |
| Unverified assumptions | none material | target port sources 500 mA |
| BOM | ~$4.50 | ~$3.80 |
| Failure mode | none expected | clean refusal via FAULT |

They share ~90% of the schematic. Building A first and B second is cheap;
building both in one PCBWay order is cheaper still, and directly answers the
"can the A1200 adapter actually feed this" question with hardware.

---

## 6. Deliberately excluded

**Full HID dev/lab tool** (capture, replay, descriptor dumping, protocol
fuzzing, OLED, broken-out GPIO banks). Products already exist for this. Not
worth building.

**Still open, and cheap to defer:** transparent-only versus a compiled-in
keymap remap table. Pure firmware. The interesting version is **layout
translation** — the Amiga adapter maps HID usages to Amiga scancodes
positionally, so a modern PC keyboard puts the wrong keys in the wrong places;
remapping in the bridge makes any keyboard an Amiga keyboard with no adapter
changes. Hardware cost is one GPIO mode strap, or just reuse BOOTSEL as a
runtime input. Wire the strap regardless and decide after the board boots.

---

## 7. Off-the-shelf shortcut — Waveshare RP2350-USB-A

**Found 2026-09-XX. This may make Variant B a firmware-only project.**

<https://www.waveshare.com/rp2350-usb-a.htm> — RP2350A, 2 MB flash, LDO 500 mA,
listed as *"with PIO-USB Type-A and USB Type-C port"*. Under $10. That is a
native USB device port **and** a PIO-USB Type-A host receptacle on one
off-the-shelf board — i.e. the entire Variant B topology, already built.

The PHY assignment falls out correct: **native hardware USB on the Type-C →
target** (via a C-to-A cable), **PIO-USB on the Type-A receptacle → keyboard**.
That matches §2's non-negotiable split exactly.

Sibling products, for reference:

| Board | Ports | Fit |
|---|---|---|
| **RP2350-USB-A** | PIO-USB Type-A host + native Type-C | **the one** |
| RP2350-USB-C (`sku=34641`/`34953`) | PIO-USB on a Type-C, female or male | needs an A-to-C adapter for the keyboard; worse for no gain |
| RP2350-One | Type-A male, single port | USB-stick form factor, only one port |
| RP2350-PiZero | native Type-C + PIO-USB port + DVI/TF | works, but far more board than needed |

**RP2354 is a non-issue here.** Stacked flash was only ever a part-count
optimisation on a board *we* lay out. On a module someone else assembled, an
external W25Q is two parts we never touch.

### Check these on the schematic PDF before ordering

These decide whether "add external power" is a jumper or a trace cut:

1. **Is the Type-A port's VBUS tied straight to Type-C VBUS, with any current
   limit?** Almost certainly a direct tie with no limiting. That means a greedy
   keyboard browns out the MCU — precisely the failure the TPS2051C exists to
   prevent. Fine for an experiment, not for a thing you actually use.
2. **Is there a broken-out 5 V/VBUS pad, and a diode between it and the
   Type-C?** If there's no diode, injecting external 5 V there **back-drives
   VBUS onto the target's port** — the exact bug class v0.1 spent months on.
   Adding external power would mean cutting the VBUS trace and feeding
   downstream of the cut. Doable, ugly, and it turns a clean dev board into a
   bodge.
3. **CC pulldowns on the Type-C.** Near-certain since it's a device port, but
   confirm if the plan is ever to power it from a charger.

No ESD protection on either port, obviously. Irrelevant at this price.

### Consequence for this concept

Buy one and prove the firmware first (see §8). If it works and 500 mA covers
the keyboards you care about, **you may simply be done** and never need a
custom board at all. If the power ceiling bites, the answer is the custom
Variant A board — *not* a VBUS bodge on a dev board.

---

## 8. Breadboard / prototyping path

Nothing here needs a fabbed board to validate. The whole architecture can be
proven on off-the-shelf parts first, and **should be** — specifically the
PIO-USB hub and low-speed handling, which is where the bring-up risk is
concentrated.

Cheapest credible rigs, roughly in order of convenience:

| Option | What it is | Note |
|---|---|---|
| **Waveshare RP2350-USB-A** | RP2350 + PIO-USB Type-A host + native Type-C | see §7 — this *is* Variant B, not just a rig for it |
| Adafruit Feather RP2040 **with USB Type A Host** (#5723) | RP2040 + a real USB-A host port already wired to PIO with the 27 Ω/15 k network | closest thing to the concept in a dev board; just add power |
| Adafruit RP2040/RP2350 board + **USB Host BFF / Stemma host breakout** | host port as an add-on | flexible, works with RP2350 for the actual target chip |
| Pico 2 (RP2350) + USB-A breakout + 2× 27 Ω + 2× 15 kΩ | fully manual | cheapest, ~$8 total, and you learn the host-port network properly |
| Waveshare RP2040-PiZero | has a USB-A host port | fine, less documented for PIO-USB |

Software: `pico-pio-usb` + TinyUSB, either the Pico SDK directly or the Arduino
core (`arduino-pico` bundles both and has working host-HID examples). Validate
in this order:

1. Enumerate one plain keyboard over PIO-USB, print reports to UART/SWD.
2. Simultaneously present as a boot keyboard over native USB to a laptop.
3. Pass keystrokes end to end. This is the whole product.
4. **Low-speed keyboard** over PIO-USB (the vintage ones).
5. **Hubbed / composite keyboard** — the known-weak case.
6. Measure actual current draw of the bridge, and of each keyboard you own, to
   settle Variant B's budget with numbers instead of a table of guesses.
7. If an A1200 is to hand: measure what the USB adapter will source. That single
   number decides whether Variant B is real.

Only after 1–5 pass is there any point ordering a board.

---

## 9. Provenance

Discussion-only, outside the spec process, at the user's explicit request. The
active hardware effort remains `.specs/pcb-carrier-v0-2/` (two-board ESP32-S3
carrier, PCBWay 2-layer, THT discretes, 3D-printed case). Nothing in this file
is approved, gated, or scheduled.
