# Design notes

What every block does and why the parts were chosen. The schematic is generated
by `digifant2_pruefstand.py`; each function there is one block below.

## Overview

A Raspberry Pi Pico drives a set of analog and digital front-ends that stand in
for every sensor and signal the Digifant-2 ECU expects, while two INA226 current
monitors and a switchable 12 V rail let the bench watch what the ECU does and
cut its power. Everything the ECU sees goes through **J0**, a 12-position 5 mm
screw-terminal block wired to the real VW-155906373 harness connector.

Rails: `VIN_12V` (bench supply) → `+12V_PROT` (post-fuse) → `+12V_POST_D1`
(post reverse-polarity diode) → `+12V_ECU` (post shunt, *always* live, this is
what U1 measures) → **Q2 load switch** → `+12V_ECU_SW` (feeds the ECU harness,
the idle valve, and the crank pull-up). Logic runs off the Pico's own `+3V3`,
which the Pico regulates from its USB connection to the host PC — this board
never powers the Pico.

## Blocks

### `power_section` — bench input and protection
| ref | part | why |
|-----|------|-----|
| J1 | 5.08 mm screw terminal | bench 12 V in; ~10 A rated, no concern |
| F1 | **3 A** fuse, 1206 | worst case is ECU (~0.6 A) + cold idle-valve inrush (~1.8 A) + logic ≈ 2.5 A — a 2 A fuse nuisance-trips |
| D1 | **SS54** Schottky, SMC | reverse-polarity protection; SS54 is 5 A — SS34's 3 A has no margin and runs ~1.3 W hot at 2.5 A |
| RS1 | **0.02 Ω** 1 % shunt, 2512 | 40 mV at 2 A, well inside the INA226 ±81.92 mV range; 0.08 W in a 1 W part |
| C1 | 100 µF / 25 V | bulk on the *switched* rail so it also supplies (and RS1 measures) the ECU's cold-boot inrush |

`RS1` uses a plain 2-pin resistor symbol — there is no stock 4-pad Kelvin 2512
footprint. For an accurate reading, route U1's `IN+/IN−` as their own thin traces
tapped right at the RS1 pads during layout, not off the power copper.

### `ecu_power_switch` — high-side load switch to the ECU
Lets firmware cold-boot the ECU and drop the rail on an overcurrent reading from
U1 (RS1 sees the switch plus every downstream load).

| ref | part | role |
|-----|------|------|
| Q2 | P-MOS, −30 V, ≥5 A, DPAK (TO-252) | high-side pass element between `+12V_ECU` and `+12V_ECU_SW`; give the drain tab a copper pour |
| Q3 | 2N7002, SOT-23 | level-shifts the 3 V3 `GP13` enable to the 12 V gate swing |
| R13 | 10 k | pulls Q2 gate to its source → **off by default** |
| R14 | 4.7 k | limits Q3 sink current |
| R15 | 1 k | series into Q3 gate |
| R16 | **100 k** | holds Q3 gate low when the Pico is unpowered — fail-safe: the ECU rail only comes up on an explicit command |
| C3 | 10 nF | with R13, ~100 µs turn-on slope to tame inrush |

### `ecu_current_sense` — total ECU current (U1)
INA226 across RS1, I²C address **0x40** (A0 = A1 = GND). Reads bus voltage and
current for the whole switched rail. `VS` from 3 V3. `ALERT` unused (NC).

### `idle_valve` — real N71 valve as load, low-side current sense (U3)
The bench-mounted real idle-air-control valve plugs into **J2** (JST-XH,
3 A/contact). The ECU's own low-side driver on VW-23 PWMs it; the bench watches
the current.

| ref | part | why |
|-----|------|-----|
| J2 | JST-XH 2-pin | to the real valve; ~1 A average, ~1.8 A peak — within XH rating |
| RS2 | **0.033 Ω** 1 % shunt, 2512 | 0.1 Ω would clip the INA226 above ~0.8 A; 0.033 Ω gives ~60 mV at 1.8 A peak |
| U3 | INA226, address **0x41** (A0 = 3 V3) | low-side sense of the valve current |

### `dac_analog_sim` — sensor spoofing (U2)
MCP4728, 4-channel 12-bit I²C DAC, address **0x60**, `~LDAC` low. Each output goes
through a series resistor into the harness:

| DAC out | R | VW pin | simulates | disconnect |
|---------|---|--------|-----------|-----------|
| VOUTA | R2 220 Ω | VW-9 | intake-air temperature (NTC) | U6, GP8 |
| VOUTB | R3 220 Ω | VW-10 | coolant temperature (NTC) | U7, GP9 |
| VOUTC | R4 220 Ω | VW-21 | air-mass meter (LMM) | — |
| VOUTD | R5 **1 k** | VW-2 | lambda / O₂ | U8, GP11 |

**Sensor-fault simulation:** the two NTC channels and the lambda channel each pass
a **TS5A3159A** SPDT analog switch (~1 Ω, SOT-23-6, U6–U8). GPIO high → COM–NO →
sensor connected; GPIO low → COM–NC (open) → the ECU sees an **open sensor** and
should set the corresponding fault code. R22–R24 pull the control lines up so all
three read connected at power-on; C6 decouples the DAC and switches. Lambda's
series R is **1 k** (not 220 Ω) so it looks less like an ideal voltage source to
the ECU's O₂ input. Set MCP4728 channel D to the internal 2.048 V reference for
fine resolution in the 0–1 V lambda window.

### `crank_driver` — crank / Hall signal (Q1)
Open-drain 2N7002 with **RG1 (100 Ω)** gate resistor and **RPU1 (1 k)** pull-up to
the switched 12 V rail. Pico `GP2` toggles it → a clean 12 V square wave on VW-18
for the ECU's engine-speed input. ~12 mA when on. **VW-18 confirmed** by the
user's own working HiL notes: the crank signal needs ≥8–10 V amplitude (5 V is
not enough), which only makes sense pulled up to the 12 V rail as done here —
this also resolves the earlier VW-8-vs-18 ambiguity from public pinout sources
in favour of VW-18.

### `edge_capture` — read the ECU's ignition & injector outputs
The ECU's output pins are pulled up to the Pico's 3 V3 (**R8 10 k** on VW-25
ignition, **R10 1 k** on VW-12 injector) with a **330 Ω** series resistor
(R9 / R11) as an ESD / current guard into the GPIO. `GP14` = ignition,
`GP15` = injector. R10's 1 k on the injector line is validated by the user's own
bench notes: a 10 k pull-up was too weak for the ~2.4 ms idle injector pulse
(didn't fully charge/discharge between cycles); even 2.2 k still showed some
droop, so 1 k here has real margin.

### `knock_sim` — knock-sensor burst generator (U4)
AD9833 DDS on SPI (`GP16` FSYNC / `GP18` SCLK / `GP19` SDATA). **Y1**, a real
4-pin **XO91 25 MHz** active oscillator (not a bare crystal — MCLK is a clock
*input*), feeds MCLK. The output passes an **R12 (200 Ω) + C2 (100 nF)** RC to
**TP1**, a test point — the coupling to a real knock-sensor input isn't defined
yet, so it stops at the pad. AD9833 `COMP` and `CAP_2V5` are decoupling-only
pins; add a small cap to ground on each in a real build.

### `operator_ui` — local controls
**J3**, a 4-pin header, carries `GND / +3V3 / SCL / SDA` for an I²C OLED
(SSD1306 at 0x3C — no bus conflict with the INA226s or DAC). Check the module's
pin order before wiring. J3 has its own ~40 mm-deep clear band at the bottom of
the board (nothing else placed within it) since a 0.96″ OLED module (~27–33 mm)
plugged in flat would otherwise overhang neighbouring parts. **SW1–SW3** are
momentary buttons to ground on `GP20 / GP21 / GP22` (menu / − / +).

### `status_led` — one addressable RGB pixel (D2)
**SK6812** (5050) on a single GPIO (`GP10`), driven by the Pico's PIO. SK6812 is
picked over WS2812B because its data threshold is in spec at 3.3 V — no level
shifter, no 5 V rail, and it frees two GPIOs versus a plain 3-pin RGB LED.
**R17 (330 Ω)** on the data line, **C4 (100 nF)** decoupling. Chain from `DOUT`
to add more pixels. Colour meaning is up to firmware.

### `throttle_switches` — idle-switch emulation
**Corrected by the user's own HiL notes** ("F60 Leerlaufschalter Pin 6 Masse und
Pin 11"): VW-6 is **not** a second switch contact, it's the sensor / idle-switch
**common ground** — wired straight to GND in `main_circuit`. There is only
**one** idle switch, on **VW-11**, which the ECU pulls up internally; closing it
(or here, Q5) pulls VW-11 to VW-6/GND ("Pin 11 auf Pin 6 verbinden für Leerlauf
aktiv"). No full-throttle switch is broken out — the earlier "two switches"
reading of the public pinout tables was wrong. **Q5** (2N7002, open-drain)
emulates the contact under Pico control (`GP6`); **R19 (100 k)** holds the gate
low so it reads "not idle" when the Pico is unpowered.

### `afm_ref_sense` — airflow-pot reference measurement
The airflow-meter potentiometer is read *ratiometrically*: the ECU sources a
reference on VW-17 and reads the wiper on VW-21. Since the bench injects the
wiper voltage with the DAC, firmware needs VW-17's actual level to scale it
correctly — and a drooping VW-17 is itself a useful fault indicator.
**R20 (15 k) / R21 (10 k)** divide VW-17 (~5 V nominal, headroom to ~9 V) into
the Pico's `GP26 / ADC0`; **C5 (100 nF)** filters. Sense-only, ~25 kΩ load on
the ECU's reference.

### Bus-voltage measurement
Both INA226s report **bus voltage** as well as current. **U1**'s VBUS is on the
load side of RS1, so it reads the **ECU supply voltage** directly (minus a
~0.1 V drop across the Q2 switch to VW-14 — correct for in firmware, or move
VBUS to `+12V_ECU_SW` for the exact pin-14 voltage). **U3**'s VBUS is on the
valve supply rail, so it reads the **voltage the idle valve is fed**. No extra
parts — it's an INA226 register read.

### Controller — Raspberry Pi Pico (U5)
Module footprint (castellated + through-hole). All 8 GND pins tied to the plane.
`RUN` and `ADC_VREF` to 3 V3, `AGND` to GND. `VBUS / VSYS / 3V3_EN / SWCLK /
SWDIO` left unconnected — the Pico is powered and programmed over its own USB.
I²C pull-ups **R6 / R7 (4.7 k)** on `GP4` (SDA) / `GP5` (SCL).

## GPIO map

| GP | function |
|----|----------|
| 2  | crank / Hall drive → Q1 |
| 4  | I²C0 SDA |
| 5  | I²C0 SCL |
| 6  | idle switch (VW-11) → Q5 — high = idle active |
| 7  | free (VW-6 turned out to be ground, not a 2nd switch) |
| 8  | intake-air-temp sensor connect (U6) — low = open-circuit |
| 9  | coolant-temp sensor connect (U7) — low = open-circuit |
| 10 | SK6812 status LED (PIO) |
| 11 | lambda sensor connect (U8) — low = open-circuit |
| 13 | ECU power-switch enable (high = ECU on) |
| 14 | ignition edge capture (in) |
| 15 | injector edge capture (in) |
| 16 | AD9833 FSYNC |
| 18 | AD9833 SCLK |
| 19 | AD9833 SDATA |
| 20 / 21 / 22 | buttons: menu / − / + |
| 26 / ADC0 | airflow-pot reference (VW-17), divided |

## I²C bus (GP4/GP5, 3 V3, 4.7 k pull-ups)

| addr | device | reads |
|------|--------|-------|
| 0x3C | OLED (external, on J3) | — |
| 0x40 | U1 INA226 | total ECU current **+ ECU supply voltage** |
| 0x41 | U3 INA226 | idle-valve current **+ valve supply voltage** |
| 0x60 | U2 MCP4728 | (sensor-sim DAC, write-only) |

## VW harness (J0, 15 of the 25 ECU pins)

Verified against the cabby-info.com Digifant II 25-pin reference (VW Cabriolet,
engine 2H, ECU 037906022xx) **and the user's own hands-on Digifant HiL notes**
(2024–2026 bench work on real ECUs) — the latter settled two points the public
pinout tables left ambiguous. ✅ = confirmed, ⚠️ = see notes.

| VW pin | function | bench use | |
|--------|----------|-----------|---|
| 2  | oxygen sensor input | lambda sim out (DAC → R5 → U8) | ✅ |
| 6  | sensor / idle-switch common ground | wired to GND | ✅ own HiL notes ("F60 ... Pin 6 Masse") |
| 9  | intake-air-temp sensor | NTC air sim (DAC → R2 → U6) | ✅ |
| 10 | coolant-temp sensor | NTC water sim (DAC → R3 → U7) | ✅ |
| 11 | idle switch (single contact, pulls to VW-6/GND) | Q5 open-drain to GND | ✅ own HiL notes |
| 12 | injector drive (ECU output) | edge capture → GP15 | ✅ |
| 13 | ground (battery −) | GND | ✅ |
| 14 | ECU main power (relay T87) | switched +12 V rail | ✅ |
| 17 | airflow-pot reference (ECU source, ~5 V) | divided → GP26/ADC0 | ✅ sense-only, confirmed by own HiL notes |
| 18 | Hall sender / crank signal | crank square wave (Q1) | ✅ own HiL notes: needs ≥8–10 V amplitude, resolves the earlier VW-8-vs-18 ambiguity |
| 19 | ground (engine / sensors) | GND | ✅ |
| 21 | airflow-pot wiper (ECU input) | LMM sim out (DAC → R4) | ✅ |
| 22 | idle-valve feed | switched +12 V rail | ✅ |
| 23 | idle-valve return (ECU PWM, low-side) | through RS2, sensed by U3 | ✅ |
| 25 | to ignition control unit (ECU output) | edge capture → GP14 | ✅ |

Not broken out (add terminals if needed): VW-1 (start/circuit 50), 3 (fuel-pump
relay, switches ground — a nice future diagnostic LED per the user's own HiL:
"Fuelpump Out BLUE, Pin 3 Masse geschalten"), 4/5/7 (knock sensor + / ground /
shield), 8, 16 (A/C), 20 (MIL).

## Board

144 × 154 mm, 2-layer, 4 × M3 corner holes (non-plated, 4.5 mm inset). J3 (OLED
header) has its own ~40 mm clear band at the bottom, isolated from every other
part, so a 0.96″ module can plug in flat without overhanging D2/J2. General
passives are 0805 for hand assembly. Wide traces (≥0.6 mm) on the 12 V and
valve-return nets; a GND pour on the bottom layer is recommended.

## Ideas from the user's own HiL notes, not yet in this design

- **Per-signal indicator LEDs / test points.** The user's earlier hand-built HiL
  had a labelled LED + test point on each key signal (600 Ω series each): power
  in (green), Hall/crank in (green), Hall supply out (green), ignition out
  (red), injector out (blue), idle-valve out (yellow), fuel-pump out (blue).
  This board currently has one shared status LED (D2) instead — cheap to add
  discrete ones later if per-signal visibility is worth the board space.
- **VW-3 (fuel pump relay) monitoring** — not broken out here; easy add if
  wanted, per the note above.

## Known open points

- ~2–3 connections in the U1/U3 INA226 0.5 mm-pitch fanout are left for KiCad's
  interactive router.
- AD9833 knock output has no defined path to the ECU yet (stops at TP1).
- RS2 gives the idle valve a permanent path to ground in parallel with the ECU's
  VW-23 driver; to *observe* the ECU's PWM cleanly, VW-23 should be the only
  return with the shunt in series.
- AD9833 `COMP` / `CAP_2V5` want a decoupling cap to ground each on a real build.
