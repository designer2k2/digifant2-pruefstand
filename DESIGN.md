# Design notes

What every block does and why the parts were chosen. The schematic is generated
by `digifant2_pruefstand.py`; each function there is one block below.

## Overview

A Raspberry Pi Pico 2 drives a set of analog and digital front-ends that stand in
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
**C9 (100 nF)** bypasses `VS` to GND, per the INA226 datasheet's layout
guidance — added late (U1 originally had none, unlike U2/U4 which always had
theirs); tapped off the +3 V3 trunk and the GND pour a few mm south of the
packed U1/U3/RS1/RS2 pocket since there's no free room right at the pins.

### `idle_valve` — real N71 valve as load, low-side current sense (U3)
The bench-mounted real idle-air-control valve plugs into **J2** (JST-XH,
3 A/contact), placed right next to RS2/U3 (not off in the button row) to keep
the valve's drive/return/sense traces short. The ECU's own low-side driver on
VW-23 PWMs it; the bench watches the current.

| ref | part | why |
|-----|------|-----|
| J2 | JST-XH 2-pin | to the real valve; ~1 A average, ~1.8 A peak — within XH rating |
| RS2 | **0.033 Ω** 1 % shunt, 2512 | 0.1 Ω would clip the INA226 above ~0.8 A; 0.033 Ω gives ~60 mV at 1.8 A peak |
| U3 | INA226, address **0x41** (A0 = 3 V3) | low-side sense of the valve current |

RS2 sits truly **in series** between the valve (J2) and VW-23 — not as a
separate tap to GND running in parallel with the ECU's own low-side driver
(the original layout's bug: both paths shared the same GND, so return current
split between RS2 and the ECU's internal switch, and U3 only ever saw whatever
fraction happened through RS2 — understating the real current, worst exactly
when the ECU driver is on). U3's IN+ stays on VW-23's net (the pin that
already had a routed path to J0) and IN− moved to the valve-side node
(`VALVE_RETURN_SENSE`) — current-reading sign is a firmware convention either
way, and keeping IN+ where it was avoided re-routing a long existing run.
**C10 (100 nF)** bypasses `VS`, same reasoning and placement approach as U1's C9.

**Worth confirming before trusting `valve_a` at face value** -- an old
reverse-engineered schematic (unconfirmed provenance, and this reading is
less certain than the knock one above) separately labels the ECU's two
valve pins "idle valve output" and "idle valve flyback", with what look
like two actively-driven transistor paths rather than one switch plus a
passive diode. Two readings are possible: (a) a genuinely bidirectional
drive, in which case current could return a way RS2's single series tap
doesn't fully capture; or (b) a conventional single PWM'd low-side switch
with an *active* (transistor-based, not just a diode) flyback/recirculation
clamp for efficiency, in which case RS2 still sees the whole picture same
as designed, just with a steeper turn-off than a passive diode would give.
The schematic alone doesn't settle which -- scoping both VW-22 and VW-23
while the ECU drives the real valve (`BRINGUP.md` question 6) should.

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
sensor connected; GPIO low → COM–NC (NC left open) → the ECU sees an **open sensor** and
should set the corresponding fault code. R22–R24 pull the control lines up so all
three read connected at power-on; C6 decouples the DAC and switches. Lambda's
series R is **1 k** (not 220 Ω) so it looks less like an ideal voltage source to
the ECU's O₂ input. Set MCP4728 channel D to the internal 2.048 V reference for
fine resolution in the 0–1 V lambda window.

**A DAC millivolt setpoint on `AIR`/`WAT` is not yet a known temperature**
(see `firmware/OPERATION.md`, "Known limits"): the DAC drives into the same
node the ECU's own internal pull-up (if it has one) also drives, so the pin
sits at a divider between the two, not at the setpoint. Two things are
needed to turn that into a real °C value:

1. **The real sensor's resistance-vs-temperature curve** -- found 2026-10-30
   in VW's own *Digifant I / Digifant II ProTraining* manual (a factory
   service-training document covering the 2H, among other Digifant
   applications). Both the coolant temperature sensor and the intake air
   temperature sensor are plain 2-terminal NTC resistors, and **the manual
   uses the identical graph for both** -- same part family, not just a
   similar shape. Read from that graph (a scanned curve, so ±15-20 %, not a
   digital table) and cross-checked against independently-found forum
   figures that turn out to be citing the same document:

   | T (°C) | R (Ω), approx |
   |---|---|
   | 0 | 5800 |
   | 10 | 3800 |
   | 20 | 2500 |
   | 30 | 1700 |
   | 40 | 1200 |
   | 50 | 900 |
   | 60 | 650 |
   | 70 | 470 |
   | 80 | 340 |
   | 90 | 250 |
   | 100 | 190 |

   No standalone VW/Bosch part number is given for either sensor in this
   manual; the intake air sensor is described as built into the airflow
   sensor housing, so it may not be independently orderable at all.

   The same manual's wiring diagram is also a strong independent check on
   this project's whole VW-pin map, sourced completely separately from the
   user's own HiL notes: ECU terminal 2 = oxygen sensor, 6 = shared sensor
   ground, 9 = intake air temperature, 10 = coolant temperature -- exactly
   VW-2/6/9/10 as used throughout this design.

2. **The ECU's internal pull-up resistance** -- still missing confirmation.
   A vehicle wiring diagram (including the one above) only shows external
   wiring, never what's inside the ECU box, so this genuinely can't come
   from documentation; it needs the bench measurement already described in
   `BRINGUP.md` stage 6, question 4, which hasn't been run yet. There is now
   a **candidate value from an old internal schematic of unconfirmed
   provenance: 8.25 kΩ to 5 V**, with an 18.2 kΩ + 0.22 µF anti-alias filter
   (τ = 4.0 ms) between that node and an ADC0809CCN channel -- high-impedance
   enough that it doesn't add to the DC loading, so 8.25 kΩ would be the
   whole story if the schematic is right. The bench measurement in
   `BRINGUP.md` is the cross-check.

Even with the curve in hand, nothing is usable as a temperature mapping
until that second measurement exists -- it's the step in the middle, between
"pin voltage" and "equivalent sensor resistance."

**If the 8.25 kΩ candidate holds**, R2/R3 (220 Ω) and it form an ordinary
two-source divider on the VW-9/VW-10 node (the 18.2 kΩ + 0.22 µF stage still
doesn't enter, same reasoning as above):

```
Vnode = Vdac * G1/(G1+G2) + 5V * G2/(G1+G2),  G1 = 1/220 Ω, G2 = 1/8.25 kΩ
      ≈ 0.974 * Vdac + 0.130 V
```

i.e. a small, fully invertible 2.6 % gain error plus a ~130 mV floor --
not an unknown arbitrary split. `Vdac = (Vnode_target - 0.130 V) / 0.974`
would turn a target node voltage straight back into a DAC setpoint.
Combined with the R-T curve above, that's the full chain from a target °C
to the DAC code that reproduces it -- *if* both the curve and the pull-up
are correct for this specific ECU, neither of which is bench-confirmed yet.

It also sets the simulatable range, given the MCP4728's AIR/WAT ceiling of
3300 mV (`dac_codec.c`, VDD-capped) and 0 mV floor:

- **Vdac = 0 -> Vnode = 0.130 V -> R ~= 220 Ω -> ~95 °C.** The DAC can't go
  negative, so this is the hottest the sensor can be made to look -- about
  95-100 °C on the curve above, not the full range an open sensor at 100 °C+
  would otherwise reach. (A genuinely open/dead-short sensor is still
  handled separately, by the TS5A3159A fault switch, not by this divider.)
- **Vdac = 3300 mV -> Vnode ~= 3.344 V -> R ~= 16.7 kΩ**, which is colder
  than the coldest point actually read off the manual's graph (5800 Ω at
  0 °C) -- so the cold end has headroom past 0 °C, just not a calibrated
  number to put on it from the data in hand.

So, candidate pull-up and curve both holding: **roughly 0 °C up to ~95 °C**
is simulatable with a known, correctable mapping -- covering ordinary
warm-up and warm-idle testing -- with only the last few degrees toward a
fully-open-sensor reading out of reach of the DAC alone, and the cold end
unconfirmed below 0 °C purely for lack of curve data there, not an
electrical limit.

**Worked example, and a discrepancy it turned up:** `dac water 1200` -- the
coolant baseline used throughout the lambda bring-up tests
(`BRINGUP_REPORT.html`) -- maps to `Vnode ~= 1.299 V`, `R ~= 2.89 kΩ`, which
interpolates to **~17 °C** on the curve above. That reads as a cold/ambient
engine, not the "right warm-idle range" the report inferred from matching
injector pulse width to the user's own distributor notes alone. The two
readings disagree if the pull-up candidate and curve both hold -- worth a
retest of the lambda loop nearer `dac water 65-70` (the voltage ~80-90 °C
would actually need), since a cold-engine open loop is an entirely ordinary
reason for an ECU to show no lambda response, and would mean this was never
really a warm-idle lambda test at all.

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
ignition, **R10 220 Ω** on VW-12 injector) with a **330 Ω** series resistor
(R9 / R11) as an ESD / current guard into the GPIO. `GP14` = ignition,
`GP15` = injector. R10 is **220 Ω** (Rev 1 had 1 k): the ECU's injector stage
has to sink real current. The user's earlier bench notes showed 10 k and 2.2 k
too weak for the ~2.4 ms idle pulse's edges, and 1 k looked fine on the line --
but on the real ECU (2026-10-28 bring-up) a 1 k pull-up (3.3 mA) left VW-12
sitting at **0.8 V** with no injector pulses at all (`inj_n` stuck), while the
old 12 V test rig (1 k + LED, ~10 mA) showed a clean flicker. Adding 220 Ω in
parallel (about 180 Ω, ~18 mA) made it inject at once: one pulse per crank
revolution, ~3.5–3.9 ms at 850 rpm. 3.3 V / 220 Ω = 15 mA is well within the
Pico's 3V3 rail. **Fixed directly in `board/digifant2-pruefstand.kicad_sch`**
(R10's value, hand-edited, not a circuit-synth regeneration) -- still queued
for the next fab run, revision not bumped yet pending more bring-up findings.
**The already-fabricated board:** solder a 220 Ω resistor from J0 VW-12 to the
+3V3 pin of J3 (or across R10).

### `knock_sim` — knock-sensor burst generator (U4)
AD9833 DDS on SPI (`GP16` FSYNC / `GP18` SCLK / `GP19` SDATA). **Y1**, a real
4-pin **XO91 25 MHz** active oscillator (not a bare crystal — MCLK is a clock
*input*), feeds MCLK. The output passes an **R12 (200 Ω) + C2 (100 nF)** RC to
**TP1** (signal) and **TP2** (ground) — a defined pair of test points to wire
to the ECU harness's VW-4 (signal) / VW-5 (ground, 2H) via a short external
pigtail, rather than dead-ending at a single pad with no return reference. J0
is already full at 15/15 positions (VW-4/5/7, the knock sensor's real pins,
were never broken out there), so this stays a small standalone tap rather than
expanding J0. A proper 2-pin JST connector (matching J2's style) was tried
first, but this corner of the board is criss-crossed with existing traces on
both layers (`+12V_ECU_SW`, `VW_PIN10/11/12`, `GP8/GP9/GP13/GP26` all thread
through here) — every placement tried for a through-hole 2-pin connector
shorted a different existing net, so two small bare test pads (same
`TestPoint_Pad_D1.5mm` footprint as the original TP1) were used instead; they
fit where the bulkier connector wouldn't. AD9833 `COMP` and `CAP_2V5` are
decoupling-only pins; each gets its own **100 nF cap to AGND (C7, C8)**, per
ADI's recommended application circuit.

**How the real ECU likely detects knock** -- from an old reverse-engineered
schematic of unconfirmed provenance, so treat this as a working theory to
test, not a confirmed fact. The knock input appears to go through a
two-stage active bandpass filter, then a precision rectifier (op-amp +
diode pair), then a long-time-constant integrator (~40 ms) whose output
feeds the ECU's own ADC -- so the actual "knocking or not" decision looks
like it's made in software on a digitised energy value, not by an analog
threshold trip. There also seems to be a gating signal around the
integrator, consistent with only integrating during a specific crank-angle
window rather than continuously. **If that's right, `burst start_deg` /
`len_deg` timing likely matters a lot** once U4 is running -- a burst that
lands outside whatever window the ECU gates on may never register, no
matter how correct its frequency or amplitude is. Worth keeping in mind
when `BRINGUP.md` question 5 is finally reachable, rather than assuming any
burst timing will do.

### `operator_ui` — local controls
**J3**, a 4-pin header, carries `GND / +3V3 / SCL / SDA` for an I²C OLED
(SSD1306 at 0x3C — no bus conflict with the INA226s or DAC). Check the module's
pin order before wiring. J3 has its own ~40 mm-deep clear band at the bottom of
the board (nothing else placed within it) since a 0.96″ OLED module (~27–33 mm)
plugged in flat would otherwise overhang neighbouring parts. **SW1–SW3** are
momentary buttons to ground on `GP20 / GP21 / GP22` (menu / − / +).

### `debug_test_points` — scope probe points (TP3–TP14)
THT test pads (`TestPoint_THTPad_D1.5mm_Drill0.7mm` — a real 0.7 mm drilled
hole, same as the knock output's TP1/TP2; **not** the bare SMD pad this board
used earlier, corrected after review) on the signals most likely to get
probed while debugging firmware: crank drive (TP3), idle-switch drive (TP4),
ignition/injector capture (TP5/TP6), the SPI bus to the AD9833 (TP7 CS / TP8
SCK / TP9 MOSI), the I²C bus (TP10 SDA / TP11 SCL), the status LED data line
(TP12), and both 12 V rails — sensed (TP13, `+12V_ECU`) and switched (TP14,
`+12V_ECU_SW`). TP3–TP12 sit in the clear margin strip west of the Pico
module (x < 10 mm) rather than near their signal's own Pico pin — the Pico's
real footprint courtyard is a physical keepout once it's soldered down, and
the first placement pass put 10 of these test points *underneath* it,
unreachable with a probe. **+3V3 was deliberately left without a dedicated
point**: it was tried at four different anchor locations (three different
Pico pins, then a decoupling cap elsewhere) and every one routed through the
same congested corridor near the Pico, each time producing a real
clearance/short against unrelated nets. Not worth forcing — +3V3 is already
exposed at many existing pins (R6/R7, C2–C6, any INA226 `VS` pin) if you need
to probe it.

**Resorting TP3–TP12 to declutter (avoid crossing traces)**: the original
placement order didn't match the Pico pins' physical top-to-bottom order,
guaranteeing crossing wires in the west margin. Resorted to
`TP3, TP10, TP11, TP4, TP12, TP9, TP8, TP5, TP6, TP7` (matching pin order) and
ripped up all 10 nets to reroute clean. The user hand-routed 7 of them; the
last 3 (TP9/GP19_SPI_MOSI, TP8/GP18_SPI_SCK, TP7/GP16_SPI_CS — all on U5's
*east* side, the far side of the Pico from the TP margin) needed real
pathfinding, not hand-placement — direct probing confirmed the Pico's pin
column (x≈12.16) has **zero** usable gap anywhere along its length for a
second trace: adjacent pins are 2.54mm apart, and while two bare THT pads
alone would in theory leave a 0.49mm gap at the midpoint, every one of those
gaps is already occupied by some other GPIO's own escape trace by this point
in the board's routing, closing it off entirely. Both TP8 and TP9 had to
detour around the *south* end of the Pico footprint (the only other opening,
confirmed the same way), and since even that detour corridor is only wide
enough for one trace, they needed genuinely different crossing points, not
parallel lanes 0.4mm apart.

Routed with a purpose-built A* pathfinder (0.1mm grid, spatial-bucket-indexed
obstacle lookup for speed, shortcutting the raw grid path down to a handful of
straight segments afterward) rather than more manual attempts, after
Freerouting's auto-router reliably completed in ~3 minutes but then hung
indefinitely in its own route-optimization phase on this exact 10-net batch
(tried 180s/400s/580s timeouts, never produced a `.ses`). Two bugs in the
pathfinder's own clearance model cost real iteration time and are worth
remembering for any future scripted routing here:
1. **Non-circular pads approximated by half-width, not half-diagonal** — a
   1.7×1.7mm square pad modeled as a r=0.85mm circle understates its real
   reach along the diagonal (true corner distance ≈1.2mm), so a track that
   looked clear by ~0.14mm was actually a real DRC short once diagonal
   approach was accounted for.
2. **Hole-to-hole spacing wasn't checked for a net's own via near its own
   pad** — same-net copper can touch/overlap freely for *clearance* purposes,
   but a via's *drill hole* still needs the board's minimum hole-to-hole
   spacing from any other hole, including one on the same net (TestPoint THT
   pad ⌀0.7mm + via drill ⌀0.3mm need ≥0.65mm center-to-center, not just
   >0mm).

### `status_led` — one addressable RGB pixel (D2)
**SK6812** (5050) on a single GPIO (`GP10`), driven by the Pico's PIO. SK6812 is
picked over WS2812B because its data threshold is in spec at 3.3 V — no level
shifter, no 5 V rail, and it frees two GPIOs versus a plain 3-pin RGB LED.
**R17 (330 Ω)** on the data line, **C4 (100 nF)** decoupling. Chain from `DOUT`
to add more pixels. Firmware (`status_led.c`, PIO1) shows: dim white = ECU off,
green = ECU on / crank stopped, blue = crank running, red = over-current trip.

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
**R20 (20 k) / R21 (10 k)** divide VW-17 (~5 V nominal, headroom to ~9 V) into
the Pico's `GP26 / ADC0`; **C5 (100 nF)** filters. Ratio is 1:3 (not the
original 1:2.5) so the stated 9 V worst case lands at 3.0 V — 0.6 V of real
margin below the ADC's absolute-max input rating (unchanged between RP2040 and
RP2350: both clamp to VDDIO + ~0.3 V), instead of landing right on it. Sense-only, ~30 kΩ load on
the ECU's reference.

### Bus-voltage measurement
Both INA226s report **bus voltage** as well as current. **U1**'s VBUS is on the
load side of RS1, so it reads the **ECU supply voltage** directly (minus a
~0.1 V drop across the Q2 switch to VW-14 — correct for in firmware, or move
VBUS to `+12V_ECU_SW` for the exact pin-14 voltage). **U3**'s VBUS is on the
valve supply rail, so it reads the **voltage the idle valve is fed**. No extra
parts — it's an INA226 register read.

### Controller — Raspberry Pi Pico 2 (U5)
Module footprint (castellated + through-hole). All 8 GND pins tied to the plane.
`RUN` and `ADC_VREF` to 3 V3, `AGND` to GND. `VBUS / VSYS / 3V3_EN / SWCLK /
SWDIO` left unconnected — the Pico is powered and programmed over its own USB.
I²C pull-ups **R6 / R7 (4.7 k)** on `GP4` (SDA) / `GP5` (SCL).

**Socketed, not soldered directly** (per the user's own assembly report,
2026-10-08): solder two 1×20 + a short 1×3 female header strip into U5's
existing through-hole pads instead of the module's own leads, so the Pico 2
can be swapped without desoldering. The footprint (`RPi_Pico_SMD_TH`) was
designed for direct solder, either castellated-edge SMD or through-hole, not
specifically for header posts — but its pads happen to already be at the
genuine 2.54 mm pitch and 17.78 mm row spacing, with a 1.02 mm drill that's
close to (if a little tight for) a standard header pin. This works on the
already-fabbed board as a practical retrofit; **test-fit one header pin in a
spare hole first**, since the hole size was never dimensioned for this use.
No footprint/board change was needed for it. See `BOM.md` for the header
part.

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

## Schematic readability (open, 2026-10-08)

**Confirmed unreadable, per a real assembly report** — rendered the current
flat single-sheet schematic to check: nearly every connection is drawn as a
disconnected net-label flag rather than a real wire (circuit-synth's flat
placement output), so there's no signal flow to follow visually at all, just
scattered text pairs to match by eye. No fix attempted yet; this needs its
own dedicated, iteratively-verified session, not a rushed pass, because:
- The obvious fix (hierarchical sheets, grouping by function) runs straight
  into circuit-synth's hierarchical label/pin bugs (see `UPSTREAM_BUGS.md`,
  issue #619 and the sheet-pin investigation) — real electrical risk if
  applied without an ERC check after every change.
- A safer flat-sheet reorganization (visually separated, labelled zones per
  function; real wires for short local connections, net labels only for
  genuinely long-distance ones) is still a large, component-by-component
  manual job best done with its own ERC-verified passes, not scripted blind.

## Board

144 × 150 mm, 2-layer, 4 × M3 corner holes (non-plated, 4.5 mm inset) — plain
clearance holes, no threads cut into the board itself; add your own M3
standoffs and screws (see `BOM.md`). No board change needed, they were
already there.

**Bug found at bring-up (2026-10-28): U6/U7/U8 wired COM to the wrong pin.**
On the TS5A3159A (SOT-23-6) the pins are 1 NO, 2 GND, 3 NC, 4 **COM**, 5 V+, 6 IN.
Rev 1 connected the ECU-side output (VW-9 / VW-10 / VW-2) to pin 3 -- **NC** --
and left pin 4 (COM) open. With IN high the switch joins COM–NO, so the output
sat on an open NC pin and floated: with ECU on, VW-9 read 0.3 V (expected 1.000
V), VW-10 0.4 V (2.500 V), VW-2 0.1 V (0.450 V), while VW-21 (no switch) was
exact. Confirmed by measuring 1.000 V on U6 pin 4. The generator script is
fixed (`sw[4] += vw_net`), and **`board/digifant2-pruefstand.kicad_sch` and
`.kicad_pcb` now carry the same fix directly** (hand-edited in KiCad, same as
every other post-generation layout change -- not a circuit-synth regeneration,
which would discard the hand-routing). On the PCB this moved the net from pad
3 to pad 4 on each of U6, U7 and U8: a short reroute, since the pads sit
directly across from each other on the same row. Verified both ways: the
schematic's ERC is unchanged (same 66 warnings, same categories, 0 errors)
and the PCB's DRC with schematic-parity is unchanged too (112 violations,
same categories/counts as before the fix; 0 unconnected items). Revision not
bumped yet -- more bring-up findings are still coming in before the next fab
run. **The already-fabricated board** still needs the bodge: a short wire from
**pin 4 to pin 3** on each of U6, U7 and U8 (the pads are on opposite sides of
the body). That puts COM on the VW net; with IN low, COM–NC joins the net to
itself, so the output still floats (open-sensor test works as designed).

**Silkscreen fixes (2026-10-08, from a real assembly report — PCB-level, so
only on the *next* fab run, not retrofittable to a board already printed):**
- **U6/U7/U8 (TS5A3159A, SOT-23-6) were missing their pin-1 marker.** The
  stock `Package_TO_SOT_SMD:SOT-23-6` footprint has one (a small silkscreen
  triangle); it was silently dropped when circuit-synth embedded its own
  simplified copy of the footprint into the board, a known failure mode for
  this tool (see `UPSTREAM_BUGS.md`). Restored directly in the `.kicad_pcb`
  via a `pcbnew` script (not a full regeneration, which would also discard
  the hand-routing and GND pour). **On the board you already have:** the
  physical chip's own package marking (per TI's datasheet) is the reliable
  way to find pin 1, not the PCB silkscreen.
- **J0's per-pin function labels (`VW-25`, `GND`, …) sat on the side the
  harness bundle lies across once wired**, becoming unreadable after
  installation. Moved to the other side of the connector, clear of the
  R21–R23/U6–U8 row beside it (one label, `+12V SW` at pin 8, shortened to
  `+12VSW` and set 0.85 mm instead of 1.0 mm tall — the only spot too tight
  for the full-size text). **On the board you already have:** use the VW
  harness pin map above instead.
- **J3 had no per-pin labels at all**, just the bare `J3` reference. Added
  `GND`/`3V3`/`SCL`/`SDA` next to each pad (top to bottom, matching the
  schematic). **On the board you already have:** pin 1 (top) is `GND`.

All three checked DRC-clean (same pre-existing cosmetic violation count, zero
new) and confirmed by rendering the actual silkscreen layer before committing.

Was 144 × 169 mm — trimmed 19mm off the bottom, which was entirely empty (bare
GND pour, no components or traces below PROTO1/J3 at y≈135 until the old
mounting holes at y=164.5). H3/H4 moved up to y=145.5 to keep the same
4.5mm hole-to-edge margin as H1/H2; GND zone outline and fill updated to
match, verified DRC-clean (99 violations, same pre-existing cosmetic set,
zero new issues). U5's USB
end has a 23 mm clear band above it toward J1 (was only ~7.5 mm, X-overlapping —
a plugged-in USB cable would have fouled J1's screw terminal). J3 (OLED
header) has its own ~40 mm clear band at the bottom, isolated from every other
part, so a 0.96″ module can plug in flat without overhanging D2/J2. General
passives are 0805 for hand assembly.

**PROTO1** — a 12×16 grid of isolated 1.6 mm/0.8 mm-drill THT pads (2.54 mm /
0.1″ pitch, ~30×38 mm) in the empty area bottom-right of J0, below where J0's
own footprint ends (y > 92 mm) and clear of the H4 mounting hole's keepout.
Perfboard-style: every pad is its own net (unconnected), solder in whatever
you need for one-off additions or bodge wires. PCB-only, like the mounting
holes — no schematic symbol, since it's not part of the actual circuit
(`extra_footprint` is the one expected DRC warning this produces, matching
what H1–H4 would also show if their `exclude_from_bom`/`exclude_from_pos_files`
attributes didn't already suppress it).

**Netclasses** (2026-09-11, following a deep layout review): `Power` (0.8 mm
track — VIN_12V, +12V_PROT, +12V_POST_D1, +12V_ECU, +12V_ECU_SW,
VW_PIN23_VALVE_RETURN) and `Gnd` (0.5 mm track — GND) are now real KiCad
netclasses in the project, not just a one-off width baked into the autorouter
session — any further hand-routing on these nets inherits the right width.
Clearance stays 0.15 mm across all classes; some MSOP-10/SOT-23-6 pads are only
~0.15 mm apart, so a tighter class clearance fails DRC against the part's own
adjacent pins.

**GND** was previously an ad-hoc daisy chain that split into 4 disconnected
islands (the whole operator-UI cluster — buttons, status LED, OLED header —
had no ground return except through the Pico module's internal plane) and
physically routed the ECU's full return current *through* U1/U3's ground pads,
corrupting the current-sense reference. It's ripped up and re-routed as part
of the normal autoroute pass now, giving one connected net with sane 0.5 mm
copper. A literal B.Cu ground pour is still worth adding by hand in the KiCad
GUI (Draw Filled Zone, B.Cu, GND net) once you're routing — scripted pour +
via-stitching attempts kept landing on other nets' copper in this dense a
board and weren't worth fighting further.

**Kelvin sensing**: U1/U3's IN+/IN− now get short, dedicated 0.2 mm traces
straight to RS1/RS2's pads instead of being unrouted or (U3's case) sharing
18.7 mm of the general GND daisy chain. This is genuinely the tightest spot on
the board — 0.5 mm-pitch MSOP-10 pins right next to a 2512 shunt — and a
handful of DRC items (a couple of clearance/crossing flags right around
U1/U3/RS1/RS2) are left for a quick manual nudge in KiCad's interactive
router, same as the historical INA226-fanout items.

**+12V_ECU_SW** no longer detours 135 mm through J2 with two extra vias — it's
ripped up and re-routed directly from Q2's tab as part of the same autoroute
pass, ~65 mm point-to-point.

**Thermal copper** added for D1 and Q2 (F.Cu zones, `ZONE_CONNECTION_FULL`
so they're solidly tied to the pad, not thermal-relief spokes). D1 got the
generous treatment (~7mm×9mm and ~5mm×9mm either side of its two pads) since
it's the documented real risk — F1 and RS1 sit on the same nets as D1's two
pads with open board space between them, so each pour spans pad-to-neighbor
with room to spare. Verified electrically connected via
`GetConnectivity().GetConnectedItems()`, not just visually overlapping
(a previous scripted attempt landed disconnected from the pad — this time
filled only the two/three new zones directly, `ZONE_FILLER.Fill([zone,
...])`, rather than a whole-board refill, and confirmed connectivity before
accepting it). Q2's pour is much smaller (~2.7mm×2.8mm) — the DPAK tab area is
boxed in on every side by other nets' traces (Q2_GATE, +12V_ECU,
+12V_POST_D1) with almost no clear board space, so this is a modest
supplement on top of the footprint's own 5-pad thermal tab design (which is
the primary heatsinking mechanism for a TO-252 part) rather than a real pour.

## Ideas from the user's own HiL notes, not yet in this design

- **Per-signal indicator LEDs / test points.** The user's earlier hand-built HiL
  had a labelled LED + test point on each key signal (600 Ω series each): power
  in (green), Hall/crank in (green), Hall supply out (green), ignition out
  (red), injector out (blue), idle-valve out (yellow), fuel-pump out (blue).
  This board currently has one shared status LED (D2) instead — cheap to add
  discrete ones later if per-signal visibility is worth the board space.
- **VW-3 (fuel pump relay) monitoring** — not broken out here; easy add if
  wanted, per the note above.

## Digifant 1 (G40/G60) — explicitly out of scope for this board

This board targets **Digifant II (2H)** only. Per the user's own repair notes,
Digifant I (G40/G60) has real, documented differences on this same connector —
do not assume pin-compatibility if a G40/G60 variant is ever built:
- **VW-5** is knock-sensor ground on 2H, but a **CO potentiometer** (0–3 kΩ,
  ~900 Ω nominal) on G40/G60.
- **Ignition output is inverted on G60** (its output stage is already inside the
  ECU) and wants a stronger pull-up than 2H.
- Fault-code readout differs too (K-Line on G40 vs. a blink-code method on G60) —
  irrelevant to this board's scope but relevant to a future shared bench.

A G40/G60 variant would need its own adapter harness (and probably a switched
pull-up / polarity option), not just different firmware on this board.

## Known open points

- ~~TP12/GND "imprecisely located" shorting_items~~ — turned out to have a
  real root cause, found and fixed: every footprint cloned via
  `pcbnew.FOOTPRINT(template)` this session (C7/C8, then all of TP1–TP14)
  inherited the **same UUID as the template**, since that constructor doesn't
  auto-generate a new one. All 14 TP footprints (and C7/C8) shared one UUID
  each with their siblings — which doesn't break connectivity, but corrupts
  DRC's item-identity tracking, so violations got reported against whichever
  sibling happened to be resolved first, at wildly wrong distances (one
  report cited two footprints 70mm apart as a "courtyard overlap"). Fixed
  with `footprint.ResetUuid()` (and each pad's) on every affected clone.
  **If you ever see a DRC violation citing two items an implausible distance
  apart, check for duplicate UUIDs before trusting the reported pairing** —
  `pcbnew.FOOTPRINT(existing_object)` needs an explicit `ResetUuid()` call
  after cloning, every time.
- ~~DRC violations around U1/U3/RS1/RS2~~ — fixed by hand in KiCad's
  interactive router; DRC is now clean (0 unconnected/shorts/clearance).
- ~~GND ground pour~~ — added (B.Cu). One pad (U5 GND pad 42) has only 1
  thermal-relief spoke into it instead of the zone's preferred 2
  (`starved_thermal` warning) — connected fine, optionally strengthen the
  spoke count in zone properties. D1/Q2 thermal copper still optional.
- ~~AD9833 knock output has no defined path to the ECU~~ — now reaches a
  defined pair of test points (TP1 signal, TP2 ground) for an external pigtail
  to VW-4/VW-5. See `knock_sim` above for why it's test points and not a
  proper connector.
- ~~RS2 gave the idle valve a permanent parallel-to-GND path~~ — fixed: RS2 is
  now truly in series between J2 and VW-23 (`VALVE_RETURN_SENSE` on the valve
  side, `VW_PIN23_VALVE_RETURN` on the ECU side). One new tight spot from this
  fix: U3 pin 9's own local GND-mesh neighbours (pin 1/pin 7's path back to the
  wider ground network) sit right where U3 pin 9's new sense connection also
  needs to run — 2 `shorting_items` confined to that one pocket. Tried by hand
  and via Freerouting from several angles; every attempt converges on the same
  conflict, meaning it's genuinely as tight as the U1/U3/RS1/RS2 pocket itself
  — same quick manual-nudge treatment.
- ~~AD9833 `COMP` / `CAP_2V5` decoupling caps~~ — added (C7, C8), fully routed;
  C8's GND connection (previously a short unrouted ratsnest line) got picked
  up for free by the Freerouting pass used for the RS2 fix above.

## Pending changes for the next board revision

Collected during bring-up; the already-fabricated board lives with firmware
workarounds or bodges. Fixes already applied to the live schematic/PCB (U6–U8
COM pin, R10 = 220 Ω) are described above and are not repeated here.

- **U1 IN− sense line is not Kelvin (found 2026-10-29).** U1's `+12V_ECU` sense
  trace (0.2 mm) joins the 0.6 mm power trace at about (71.03, 20.00), ~29 mm
  of power copper downstream of RS1 pad 2, so U1 also sees that trace's IR
  drop: 7.32 mV across RS1 at 0.366 A (meter in the + lead: 0.367 A), but U1
  read 0.542 A, i.e. 48 % high (≈ 0.0296 Ω effective instead of 0.020 Ω).
  **Layout now fixed (hand-edited in `board/*.kicad_pcb`, 2026-10-29):** IN−/VBUS
  runs as its own 0.2 mm trace from U1 pins 8/9 to RS1 pad 2 (46.52, 14.95),
  alongside the IN+ trace, and the old tap onto the power trace is removed.
  The IN+ side was always fine: it ends on RS1 pad 1 and carries no current.
  **Workaround on the board you have:** `sense.c` divides by 0.0296 Ω
  (`RS1_EFFECTIVE_OHM`) for U1, from one measured point; linearity still to be
  checked with the Stage 5 dummy loads. Remove the workaround on a board built
  from the fixed layout.
- **U3 (RS2) has the same flaw on its IN+ side (checked 2026-10-29, not yet
  fixed).** IN− (`VALVE_RETURN_SENSE`) is fine: it leaves RS2 pad 1 and
  dead-ends at U3 pin 9, so it carries no current. IN+ (`VW_PIN23_VALVE_RETURN`)
  is not: U3 pin 10 is fed from a via at (80.90, 32.13) on the 0.6 mm return
  trace to J0 VW-23, about **17.8 mm of power copper downstream of RS2 pad 2**
  (two vias on the way), and all of the valve current crosses that stretch.
  That is roughly 30 squares of 0.6 mm copper, about 14.5 mΩ against RS2's
  33 mΩ if the copper is 1 oz. U1's equivalent stretch measured about 0.42 of
  its textbook value, so expect **`valve_a` to read roughly 20 % to 45 % high**.
  To confirm: Stage 5 with a dummy load on J2 and the meter in the + lead, or
  the millivolts across RS2 against `valve_a`. Fix for the layout: give
  IN+ its own trace straight from RS2 pad 2 (64.26, 34.95) to U3 pin 10. Until
  then correct U3 in firmware the same way as U1 once the measurement exists.
  VBUS on U3 (the switched rail, for the voltage reading only) carries no
  current, so its long thin route is harmless.
- **C9 / C10:** C9 (U1's 3V3 decoupling cap) was moved next to U3 during the U1
  reroute and now sits beside C10. Accepted for this board. U1's supply pin has
  no cap closer than 11 mm.
- **Over-current trip scale:** the trip limit acts on the U1 reading, which was
  1.48 × the true current, so before the workaround it fired at about 0.68 ×
  the set value in real current (e.g. 2.5 A default ≈ 1.7 A true). With the
  workaround the set value is the true current again.
