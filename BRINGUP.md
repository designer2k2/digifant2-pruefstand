# Bring-up: first power-on of the Prüfstand

A step-by-step checklist from bare board to a real ECU running on the bench.
Each stage only adds one new risk, so if something is wrong you find it while
it's cheap. Tick the boxes as you go and fill in the results table at the end;
those answers settle the open questions the firmware still guesses at.

Commands are sent with `python3 host/bench.py` (interactive prompt; see
[`firmware/README.md`](firmware/README.md) for the full protocol). "J0 VW-18"
means the J0 screw terminal labelled `VW-18` on the silkscreen. J0's power
terminals are labelled by function instead: the two `+12V SW` terminals are
VW-14 (ECU supply) and VW-22 (idle-valve feed), and the three `GND` terminals
are VW-6, VW-13 and VW-19.

## What you need

| Tool | Used for |
|---|---|
| **Lab power supply, 12–14 V, adjustable current limit** | All 12 V stages. The current limit is the main safety net: start at 100 mA. |
| **Multimeter** | Continuity before power, rails, DAC outputs, series current. |
| **Oscilloscope, ≥ 2 channels** (20 MHz is plenty) | Crank signal on VW-18, knock tone/bursts on TP1, ignition/injector timing. |
| **Micro-USB *data* cable + PC** with Python 3 and `pip install pyserial` | Flashing and all commands. Charge-only cables are a classic trap. |
| Logic analyzer, 8 ch / 24 MHz clone + PulseView (optional, ~€10) | Decoding I²C (TP10/TP11) and SPI (TP7–TP9) if a chip doesn't answer. |
| **Power resistors: 1 × 47 Ω / 5 W, 2 × 22 Ω / 10 W** (5 % is fine; aluminium-housed, e.g. the Arcol HS series, screwed to a bit of aluminium) | Dummy loads: ~0.25 A, ~0.55 A and ~1.1 A at 12 V, for the over-current trip and the current calibration. |
| Hook-clip test leads, jumper wire, a 1.5 V AA cell | Probing, faking an ECU output pulse, testing the VW-17 ADC input. |
| 0.96″ SSD1306 OLED module (I²C, 4 pins) | Stage 4. |
| Magnifier / loupe | Checking solder joints on the SMD parts. |
| For stage 6: the J0 harness to the ECU connector, a short pigtail from TP1/TP2 to VW-4/VW-5, the idle valve for J2, ideally a **spare** Digifant-2 ECU | Real ECU. |

Test points (by their reference designators on the silkscreen): TP1/TP2 knock out/ground, TP3 crank
drive (GP2, 3.3 V logic, *inverted* vs VW-18), TP4 idle-switch drive, TP5/TP6
ignition/injector capture, TP7–TP9 SPI (FSYNC/SCK/MOSI), TP10/TP11 I²C
(SDA/SCL), TP12 LED data, TP13 `+12V_ECU` (always live after the fuse/diode),
TP14 `+12V_ECU_SW` (switched ECU rail). Ground: J1 −, any J0 `GND` terminal, or TP2.

**Never:**
- put any voltage on J0 VW-12 or VW-25: they are pulled up to 3.3 V and go
  (through 330 Ω) straight to Pico pins. The ECU only ever pulls them low.
  Don't connect a real injector or ignition module to them either.
- put more than ~9 V on J0 VW-17: its 20k/10k divider feeds the Pico's ADC,
  which tops out at 3.3 V. The ECU's ~5 V reference is fine; 12 V is not.
- power an OLED module whose pin order doesn't match J3 (see stage 4).

---

## Stage 0 — Flash the Pico before it goes on the board (optional)

Catches a dead Pico or a bad cable before soldering.

1. Firmware: build it (`firmware/README.md`, "Build and flash") or download
   `pruefstand-firmware` from the latest green run on the repo's **Actions**
   tab.
2. Hold BOOTSEL, plug in USB, copy `pruefstand.uf2` onto the `RPI-RP2` drive.

- [ ] `python3 host/bench.py ping` → `OK pong fw=0.9.0`
- [ ] `status` → `boot=power ecu=off …`; `dac_i2c=err` is expected here (no DAC on a bare Pico)

## Stage 1 — Visual and continuity, no power

- [ ] Polarity/orientation: D1 (SS54, band), C1 (electrolytic, − stripe),
      every IC's pin 1 (U1–U4, U6–U8), Y1 oscillator's pin-1 dot, Q2 (DPAK)
- [ ] No solder bridges on the fine-pitch parts (U1/U3 INA226, U4 AD9833, U2 MCP4728)
- [ ] Resistance to ground (multimeter, either polarity; anything below ~10 Ω is a short):
  - [ ] J1 + to J1 −
  - [ ] TP13 (`+12V_ECU`) to GND
  - [ ] TP14 (`+12V_ECU_SW`) to GND
  - [ ] +3V3 (J3's 3V3 pin, or either end of R6/R7) to GND
- [ ] F1 (fuse) has continuity

## Stage 2 — USB only, no 12 V

The Pico runs from USB alone and powers all the logic (INA226s, DAC, AD9833
and switches run on +3V3), so everything on the I²C bus can be checked
without any 12 V risk.

- [ ] Pico enumerates, `ping` → `OK pong fw=0.9.0`
- [ ] `status` → `dac_i2c=ok` (MCP4728 answers at 0x60)
- [ ] `read` → `ecu_v` and `valve_v` are **numbers**, not `na` (both INA226s
      answer), both ≈ 0 V; `afm_ref_mv` ≈ 0
- [ ] Idle switch: ohmmeter, red lead on J0 VW-11, black on a J0 `GND` terminal (the
      other way round the MOSFET's body diode conducts and it always reads low)
  - [ ] `idle on` → a few Ω
  - [ ] `idle off` → open
- [ ] VW-17 input: AA cell between J0 VW-17 (+) and GND (−) → `read`
      `afm_ref_mv` ≈ the cell's voltage (~1500 mV), ±50 mV. Remove the cell.
- [ ] `capture` works without an ECU: tap a jumper from J0 VW-25 to GND a few
      times → `ign_n` counts up (switch bounce may also raise `ign_glitch`,
      that's fine). Same for VW-12 → `inj_n`.

If a chip shows `na`/`err`: check its solder joints first, then look at TP10/TP11
with the logic analyzer (PulseView I²C decoder; addresses 0x40, 0x41, 0x60).

## Stage 3 — 12 V on, current limit 100 mA, no ECU

Set the supply to 12.0 V, **limit 100 mA**, then connect it to J1 with USB
already plugged in.

- [ ] Supply current with the ECU off: a few mA at most (not in current limit)
- [ ] TP13 ≈ supply minus ~0.2–0.5 V (D1 drop); TP14 ≈ 0 V
- [ ] `read` → `ecu_v` ≈ TP13 (±0.05 V), `ecu_a` ≈ 0

**ECU switch (still no ECU connected):**
- [ ] `ecu on` → TP14 ≈ TP13; `read` `valve_v` ≈ `ecu_v`; `ecu_a` ~1 mA (Q2's gate drive)
- [ ] `ecu off` → TP14 falls back to ~0 V
- [ ] Fail-safe: `ecu on`, then **unplug USB** → TP14 drops to 0 (R16 holds
      the switch off). Plug USB back in → `status` shows `boot=power ecu=off`.

**Crank signal** (needs `ecu on`: its pull-up is on the switched rail).
Scope on J0 VW-18 to GND:
- [ ] `rpm 0` → VW-18 steady high, ≈ 12 V
- [ ] `rpm 850` → square wave 0 V / ≈ 12 V, **28.3 Hz** (35.3 ms period), 50 % high
- [ ] `rpm 6000` → 200 Hz, clean edges, low level below ~0.3 V
- [ ] `crank duty 30` → high for 30 % of the period; then `crank duty 50`
- [ ] `read` `ecu_a` rises by ~6 mA with the crank running (1 k pull-up, 50 % duty)
- [ ] `rpm 0`

**Sensor DACs** (outputs are held at 0 V while the ECU is off, so `ecu on`
first). Multimeter on the J0 terminals to GND, nothing else connected:
- [ ] `dac air 1000` → VW-9 ≈ 1.000 V (within ~20 mV)
- [ ] `dac water 2500` → VW-10 ≈ 2.500 V
- [ ] `dac afm 1500` → VW-21 ≈ 1.500 V
- [ ] `dac lambda 450` → VW-2 ≈ 0.450 V
- [ ] `sensor air open` → VW-9 no longer driven (meter reads ~0 V or drifts); `sensor air conn` → 1.000 V again.
      Same for `water` (VW-10) and `lambda` (VW-2).
- [ ] `ecu off` → all four terminals ≈ 0 V, `status` still shows the setpoints;
      `ecu on` → the voltages come back
- [ ] `dac air 3300` → note the value (may clip a few tens of mV below 3.3 V)

**Knock generator** (also held off while the ECU is off). Scope on TP1, ground TP2:
- [ ] `knock 1000` → 1 kHz sine, ≈ 0.6 V peak-to-peak
- [ ] `knock 7000` → 7 kHz, smaller (the R12/C2 low-pass: ~75 % of the 1 kHz level)
- [ ] Bursts: channel 1 on J0 VW-18 (trigger on the falling edge), channel 2 on TP1.
      `rpm 850`, `knock 7000`, `burst 30 20`
      → a burst starts ≈ 5.9 ms after each VW-18 falling edge and lasts ≈ 3.9 ms
- [ ] `burst 30 20 2` → bursts on every second falling edge only
- [ ] `burst off`, `knock off`, `rpm 0`, `ecu off`

## Stage 4 — OLED and buttons

**Check the module's pin order first.** J3 is `GND / 3V3 / SCL / SDA`. Many
modules are `VCC / GND / SCL / SDA`, which would put 3.3 V onto the module's
ground: wire it with jumpers in that case, don't plug it straight in.

Unplug USB, fit the OLED, plug USB back in.
- [ ] Display shows the status screen within 2 s (if it stays dark or shows
      garbage it may be an SH1106 module, see `firmware/README.md`)
- [ ] MENU steps through RPM → ECU → IDLE → KN → AIR → WAT → AFM → LAM, the selected field inverted
- [ ] +/− on RPM changes it by 50, holding repeats; `status` agrees
- [ ] ECU: + switches on, − switches off (no toggle)
- [ ] With the display unplugged while running, the buttons still work, and
      plugging it back in brings the screen back within 2 s

The SK6812 status LED (D2) has no firmware driver yet; nothing to test there.

## Stage 5 — Dummy load: current reading and over-current trip

Supply limit **500 mA**. 47 Ω / 5 W resistor between a J0 `+12V SW`
terminal and a J0 `GND` terminal; multimeter in series in the supply lead, on its A range.

- [ ] `ecu on`, `read` → `ecu_a` ≈ 0.25 A and matches the series meter within
      ~2 % (remember the meter also sees the few mA of the board itself)
- [ ] `ecu_v` matches TP13 within ±0.05 V
- [ ] Trip: `ecu trip 100` → within ~0.3 s the ECU switches off by itself
  - [ ] `status` → `ecu=off ecu_fault=overcurrent`
  - [ ] `ecu on` → `ERR ecu tripped at … mA, send 'ecu reset' first`
  - [ ] OLED shows `ECU TRIP`; − on the ECU item clears it
- [ ] `ecu reset`, `ecu trip 2500`, `ecu on` → stays on with the 0.25 A load
- [ ] `ecu off`, remove the resistor

### Current calibration (U1 and U3)

The multimeter in series is the reference; the resistors only set the test
currents, so their exact values don't matter. One setup loads both current
monitors at once:

- load resistor on **J2** (where the idle valve plugs in)
- jumper from J0 **VW-23** to a J0 `GND` terminal (stands in for the ECU's
  low-side valve driver)
- multimeter in series with the supply lead, on its 10 A range; supply limit **1.5 A**

Current then flows `+12V SW` → J2 → load → RS2 → VW-23 → GND: U3 reads it as
`valve_a`, U1 as `ecu_a` (plus ~1 mA of the board's own draw). At each point,
`ecu on`, wait a second, then `read` three times and note the meter at the
same moment. The resistors heat up and the current drifts slightly, and the
INA226 averages over ~0.14 s, so take readings, not one snapshot. `ecu off`
before changing the load.

| Load | Expected | Meter (A) | `ecu_a` | `valve_a` | U1 error % | U3 error % |
|---|---|---|---|---|---|---|
| none (`ecu on`, J2 empty) | ~0.001 A | | | | — | — |
| 47 Ω | ~0.25 A | | | | | |
| 22 Ω | ~0.55 A | | | | | |
| 2 × 22 Ω in parallel | ~1.1 A | | | | | |

Error % = (reading − meter) ÷ meter × 100.

**What counts as good enough:** RS1/RS2 are 1 % parts and the INA226 adds
~0.1 %, so up to about ±1 % is expected. A typical multimeter's A range is
itself only 0.5–1.5 % accurate, so errors inside the meter's own tolerance
are noise, not something to correct. Worth correcting: an error that is
**consistent** across the three loads (same sign, similar size) and clearly
larger than the meter's tolerance. Note the per-sensor average; the firmware
uses the nominal 0.020 Ω / 0.033 Ω and has no correction factor yet, which
would be a small addition once these numbers exist.

- [ ] Calibration table filled in; `ecu off`, remove the load and the VW-23 jumper

## Stage 6 — Real ECU

Use a spare ECU first if you can get one. Supply limit **1 A**, J2 (idle valve)
still empty, knock pigtail not yet connected. Wire J0 to the ECU connector.

Before switching on, with the ECU connected and the bench unpowered, check
the harness once with the ohmmeter: each J0 `GND` terminal to the ECU's
ground pins (VW-6/13/19), and no short between `+12V SW` and ground.

- [ ] `ecu on` → ECU current (`read` `ecu_a`): record it (expected a few hundred mA)
- [ ] `read` `afm_ref_mv` ≈ 5000 (the ECU's airflow-meter reference)
- [ ] Set starting values for a warm idle: `dac water 800`, `dac air 1800`,
      `dac afm 1000`, `dac lambda 450`, `idle on`, then `rpm 850`. These are
      guesses; how DAC millivolts map to temperatures depends on question 4.
- [ ] `capture` → `ign_n` and `inj_n` count up; `ign_period_us` ≈ the crank
      reference period (35294 µs at 850 rpm, 2 ppr)
- [ ] Scope VW-25 (ignition) and VW-12 (injector) against VW-18 and compare
      with `capture`'s `_low_us` and angles (within a few µs / ~0.1°)

**Settle the open questions** (write the answers in the table below):

1. **Hall pulses per crank revolution and duty.** Most reliable: scope VW-18
   on the real car at idle and read the rev counter. ppr = Hall frequency ÷
   (rpm ÷ 60); duty = high time ÷ period. Set them with `crank ppr` / `crank duty`.
2. **Which VW-18 edge is the reference.** With the crank running, note
   `ign_fall_deg`/`ign_rise_deg` at `crank duty 50`, then at `crank duty 30`
   and `crank duty 70`. If the angles stay put, the ECU times from the falling
   edge (what the firmware assumes). If they shift with the duty, it uses
   the rising edge.
3. **Which ignition edge is the spark.** Raise `rpm` in steps from 850 to
   4000 and note `ign_low_us` and both angles. The coil's dwell is held
   roughly constant in *time*: if `ign_low_us` stays roughly constant (a few
   ms) while rpm changes, the low phase is the dwell and its end,
   `ign_rise_deg`, is the spark. If instead the high phase stays constant in
   time, it's the other way round. The spark angle should then show a
   plausible advance curve as rpm rises.
4. **NTC pull-up inside the ECU.** `sensor water open` → measure J0 VW-10 to
   GND: ~5 V means the ECU has a pull-up. Then `sensor water conn` with
   `dac water 1000` and measure VW-10 again (V₁):
   pull-up R ≈ (V_open − V₁) × 220 Ω ÷ (V₁ − 1.000 V). Same for VW-9 (air).
5. **Knock input.** Wire TP1 → VW-4 and TP2 → VW-5. At `rpm 3000`,
   `knock 7000`, `burst 10 30`, compare the spark angle (from question 3)
   with `burst off`: if the ECU's knock control reacts, the spark retards
   (moves later). No change can also mean the amplitude is too low or the
   window/frequency is wrong; try other `burst` angles and `knock` 5000–9000.
6. **Idle valve.** Plug the valve into J2, raise the supply limit to 3 A.
   `read` `valve_a` should follow what the ECU does, e.g. more current with a
   cold engine (`dac water 3000`) than warm (`dac water 800`).

## Results

| Question | Result | Date |
|---|---|---|
| Hall pulses per crank revolution (firmware default 2) | | |
| Hall duty, % high (default 50) | | |
| Reference edge (firmware uses VW-18 falling) | | |
| Spark edge (`ign_fall_deg` or `ign_rise_deg`) | | |
| NTC pull-up in the ECU (V_open, R) — air / water | | |
| ECU current at idle | | |
| `dac air 3300` actual output | | |
| Knock amplitude at TP1, 1 kHz / 7 kHz | | |
| Knock detected by the ECU (retard seen?) | | |
| OLED controller (SSD1306 / SH1106) | | |
| Current error, U1 / U3 (average %, from the calibration table) | | |
| Anything that needed rework | | |

Once the answers are in, the firmware defaults (`CRANK_PPR_DEFAULT`,
`CRANK_DUTY_DEFAULT` in `firmware/src/crank.h`) and the open-question notes in
`firmware/README.md` get updated to match. That's also the point where a
first tagged firmware release makes sense.
