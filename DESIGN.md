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
MCP4728, 4-channel 12-bit I²C DAC, address **0x60**. Each output goes through a
**220 Ω** series resistor (R2–R5) for short-circuit protection into the harness:

| DAC out | R | VW pin | simulates |
|---------|---|--------|-----------|
| VOUTA | R2 | VW-9 | intake-air temperature (NTC) |
| VOUTB | R3 | VW-10 | coolant temperature (NTC) |
| VOUTC | R4 | VW-21 | air-mass meter (LMM) |
| VOUTD | R5 | VW-2 | lambda / O₂ |

`~LDAC` tied low (outputs update immediately).

### `crank_driver` — crank / Hall signal (Q1)
Open-drain 2N7002 with **RG1 (100 Ω)** gate resistor and **RPU1 (1 k)** pull-up to
the switched 12 V rail. Pico `GP2` toggles it → a clean 12 V square wave on VW-18
for the ECU's engine-speed input. ~12 mA when on.

### `edge_capture` — read the ECU's ignition & injector outputs
The ECU's output pins are pulled up to the Pico's 3 V3 (**R8 10 k** on VW-25
ignition, **R10 1 k** on VW-12 injector) with a **330 Ω** series resistor
(R9 / R11) as an ESD / current guard into the GPIO. `GP14` = ignition,
`GP15` = injector.

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
pin order before wiring. **SW1–SW3** are momentary buttons to ground on
`GP20 / GP21 / GP22` (menu / − / +).

### `status_led` — one addressable RGB pixel (D2)
**SK6812** (5050) on a single GPIO (`GP10`), driven by the Pico's PIO. SK6812 is
picked over WS2812B because its data threshold is in spec at 3.3 V — no level
shifter, no 5 V rail, and it frees two GPIOs versus a plain 3-pin RGB LED.
**R17 (330 Ω)** on the data line, **C4 (100 nF)** decoupling. Chain from `DOUT`
to add more pixels. Colour meaning is up to firmware.

### Controller — Raspberry Pi Pico (U5)
Module footprint (castellated + through-hole). All 8 GND pins tied to the plane.
`RUN` and `ADC_VREF` to 3 V3, `AGND` to GND (ADC unused — sensing is external via
I²C). `VBUS / VSYS / 3V3_EN / SWCLK / SWDIO` left unconnected — the Pico is
powered and programmed over its own USB. I²C pull-ups **R6 / R7 (4.7 k)** on
`GP4` (SDA) / `GP5` (SCL).

## GPIO map

| GP | function |
|----|----------|
| 2  | crank / Hall drive → Q1 |
| 4  | I²C0 SDA |
| 5  | I²C0 SCL |
| 10 | SK6812 status LED (PIO) |
| 13 | ECU power-switch enable (high = ECU on) |
| 14 | ignition edge capture (in) |
| 15 | injector edge capture (in) |
| 16 | AD9833 FSYNC |
| 18 | AD9833 SCLK |
| 19 | AD9833 SDATA |
| 20 / 21 / 22 | buttons: menu / − / + |

## I²C bus (GP4/GP5, 3 V3, 4.7 k pull-ups)

| addr | device |
|------|--------|
| 0x3C | OLED (external, on J3) |
| 0x40 | U1 INA226 — total ECU current |
| 0x41 | U3 INA226 — idle-valve current |
| 0x60 | U2 MCP4728 — sensor-sim DAC |

## VW harness (J0 → VW-155906373)

| VW pin | net | direction | function |
|--------|-----|-----------|----------|
| 2  | lambda sim   | out to ECU | O₂ sensor voltage (DAC) |
| 9  | NTC air      | out to ECU | intake-air temp (DAC) |
| 10 | NTC water    | out to ECU | coolant temp (DAC) |
| 12 | injector     | in from ECU | injector drive, captured |
| 13 | GND | — | ground |
| 14 | +12V | in | ECU main power (switched rail) |
| 18 | crank | out to ECU | engine-speed square wave (Q1) |
| 19 | GND | — | ground |
| 21 | LMM | out to ECU | air-mass meter (DAC) |
| 22 | +12V | in | idle-valve feed (switched rail) |
| 23 | valve return | in from ECU | idle-valve low-side, ECU PWM — sensed by U3 |
| 25 | ignition | in from ECU | coil driver output, captured |

## Board

144 × 114 mm, 2-layer, 4 × M3 corner holes (non-plated, 4.5 mm inset). General
passives are 0805 for hand assembly. Wide traces (≥0.6 mm) on the 12 V and
valve-return nets; a GND pour on the bottom layer is recommended.

## Known open points

- ~12 connections in the U1/U3 INA226 0.5 mm-pitch fanout are left for KiCad's
  interactive router.
- AD9833 knock output has no defined path to the ECU yet.
- RS2 gives the idle valve a permanent path to ground in parallel with the ECU's
  VW-23 driver; if you want to *observe* the ECU's PWM cleanly, VW-23 should be
  the only return with the shunt in series.
