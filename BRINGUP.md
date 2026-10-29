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

- [ ] `python3 host/bench.py ping` → `OK pong fw=0.10.0`
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

- [ ] Pico enumerates, `ping` → `OK pong fw=0.10.0`
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
> **Already-fabricated boards:** U6/U7/U8 need a wire from pin 4 to pin 3
> first, or VW-9, VW-10 and VW-2 float (COM was wired to NC; see DESIGN.md).
> VW-21 is unaffected. Fixed directly for the next fab run (schematic + PCB,
> DRC/ERC-verified) -- revision not bumped yet.

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

The SK6812 status LED (D2) follows the bench state: dim white with the ECU
off, green with the ECU on and the crank stopped, blue with the crank running,
red after an over-current trip.

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
uses 0.033 Ω for U3 and, for U1, an *effective* 0.0296 Ω (not RS1's 0.020 Ω):
U1's IN- sense trace picks up part of the power trace's drop, found with a
meter in the + lead (see DESIGN.md "Pending changes"). Your table below should
now show U1 close to 0 %; a clear remaining error means the 0.0296 Ω single
point was off or the error isn't linear. **Expect U3 to read about 20–45 %
high** (its IN+ sense tap sits 17.8 mm downstream of RS2, see DESIGN.md
"Pending changes"); that is a prediction to confirm here, and the data point
for a firmware correction.

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
> **Already-fabricated boards:** if `inj_n` stays flat and VW-12 sits at about
> 0.8 V, R10 (1 k) is too weak for the ECU's injector stage. Solder a 220 Ω
> resistor from J0 VW-12 to the J3 `3V3` pin (in parallel with R10), see
> DESIGN.md. Fixed directly for the next fab run (R10 = 220 Ω) -- revision not
> bumped yet.

- [ ] `capture` → `ign_n` and `inj_n` count up; `ign_period_us` ≈ the crank
      reference period (35294 µs at 850 rpm, 2 ppr)
- [ ] Scope VW-25 (ignition) and VW-12 (injector) against VW-18 and compare
      with `capture`'s `_low_us` and angles (within a few µs / ~0.1°)

**Settle the open questions** (write the answers in the table below):

1. **Hall pulses per crank revolution and duty.** Per the user's own
   distributor HiL notes ("DIZZY_FOUR_CYLINDER"): a 30 Hz, 50% duty square
   wave at 900 rpm, i.e. 2 pulses per crank rev -- matching the firmware
   defaults already. Confirm anyway with a scope on VW-18 at idle: ppr = Hall
   frequency ÷ (rpm ÷ 60); duty = high time ÷ period. At the expected idle of
   800 rpm that's 26.7 Hz. Set them with `crank ppr` / `crank duty` if different.
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
| Hall pulses per crank revolution (expected 2, per DIZZY_FOUR_CYLINDER notes) | 2 works: the ECU fires ignition (period = ref period) and injection (1 pulse/rev) at `crank ppr 2`. Not checked against a real distributor. | 2026-10-28 |
| Hall duty, % high (expected 50, per DIZZY_FOUR_CYLINDER notes) | 50 works. 30 and 70 also run, but ignition dwell and edges shift strongly (see reference edge). | 2026-10-28 |
| Reference edge (firmware uses VW-18 falling) | **Rising**, at least for the injector: `inj_fall_deg` = rising-edge angle + 1.3° (~0.25 ms) at duty 30/50/70 (127.3 / 91.3 / 55.3°). Ignition follows no single edge: dwell start 74.6° at 50 and 70 %, 15.1° at 30 %; spark-side 143.0 / 127.1 / 176.1°. Angles in the firmware are still measured from the falling edge. | 2026-10-28 |
| Spark edge (`ign_fall_deg` or `ign_rise_deg`) | **`ign_rise_deg` (end of the low phase).** The low phase is dwell: the coil charges while it's low, and the spark fires on the cutoff -- the rising edge -- by the ordinary inductive-coil principle (also why a longer dwell gives a stronger spark). Settled on that basis rather than the knock-retard test; the angle-constant dwell (69-72° across 850-4000 rpm, not constant in time) is consistent with an external module holding dwell to a fixed crank-angle window rather than a fixed time. The knock-retard test (question 5) is still worth running on its own merits, to check the ECU's actual knock response, not to identify this edge. | 2026-10-28 |
| NTC pull-up in the ECU (V_open, R) — air / water | | |
| ECU current at idle | **0.36–0.38 A** at 11.65 V (corrected 2026-10-29; the 0.53–0.56 A first logged here was U1 reading 48 % high, see DESIGN.md "Pending changes"; BM869S in the + lead agrees: 0.359 A vs `ecu_a` 0.3587 A). VW-17 reference `afm_ref_mv` ≈ 5.83 V (ADC scale not yet checked with a meter). Sensors set to a warm idle, injector pulse 3.5–4.6 ms. Closing the idle contact adds ~12 mA. | 2026-10-28 / 29 |
| `dac air 3300` actual output | | |
| Knock amplitude at TP1, 1 kHz / 7 kHz | **Blocked:** Y1 arrived as a 6-pin part, won't fit the 4-pin footprint -- wrong part ordered, see `BOM.md`. U4 (AD9833) can't run without MCLK until a correct Y1 is in hand. | 2026-10-28 |
| Knock detected by the ECU (retard seen?) | **Blocked**, same reason as above. | 2026-10-28 |
| OLED controller (SSD1306 / SH1106) | | |
| Current error, U1 / U3 (average %, from the calibration table) | U1 before correction: +48 % (0.542 A vs 0.367 A BM869S in the + lead, 7.32 mV across RS1 = 0.366 A). Firmware now uses 0.0296 Ω effective for U1; one point only, linearity not yet checked. U3 not measured yet. | 2026-10-29 |
| ECU reaction to lambda (VW-2) at warm idle | **None detectable.** Steps to 100, 300, 700 and 900 mV, the sensor opened, a 5 min lean hold at 100 mV and a 150 / 750 mV square wave at 2 Hz for 2 min: injector time stayed within ±1.2 µs (≤ 0.05 %) of the 2622.8 µs baseline, spark within ±0.03°, current unchanged, all inside the noise of two sham steps (sham \|t\| ≤ 1.0, lambda \|t\| ≤ 2.3 before correcting for 30 comparisons; the 5 min and switching tests t = −1.3 and −1.7, no trend). Opening the lambda input changes the ECU current by +1.2 mA only (the DAC's load). Extended below. | 2026-10-29 |
| ECU reaction to lambda: frequency sweep with the Pico's `wave lambda lam` | **None.** Lambda 1.00 ± 0.03 through the sensor curve at 0.2, 0.5, 1, 2, 3 and 5 Hz, 60 s each, every injector pulse logged (850 each, `host/examples/lambda_sweep.py`): response amplitude 0.06 to 0.22 µs (≤ 0.008 %), z ≤ 1.7, the same as the wave-off baseline (0.03 to 0.24 µs). Mean injector time stepped down 1.5 % (2680 to 2641 µs) between the 0.5 and 1 Hz runs and stayed there with the wave off, so an ECU state changed independently of the wave (possibly a timer such as after-start enrichment; not established). | 2026-10-29 |
| ECU reaction to lambda: closed-loop emulation (`wave lambda loop`) | **None.** The emulation told the ECU the mixture was fully lean (λ0 1.05, DAC 1 mV) and then fully rich (λ0 0.95, DAC 899 mV) for 90 s each, with baselines between: injector time stayed within ±1.6 µs (≤ 0.06 %) of the baselines (lean 2641.5 against 2641.3 µs; rich 2645.7 against 2644.2 and 2645.8 µs). A control loop would have moved it about 5 % (130 µs). No limit cycle in the spectrum (strongest line 0.25 µs = noise floor). Baselines drifted up 4.5 µs (0.17 %) over the 13 minutes. | 2026-10-29 |
| ECU reaction to lambda: 7 conditions (idle at coolant 1000 / 1200 / 1500 / 1800 mV; 2000 rpm, idle contact open, airflow 1500 mV at coolant 1200 / 1500 and 2000 mV at 1200) | **None in any.** Loop told lean (λ0 1.05, DAC 1 mV) against baselines before and after, random order: effect 0.0 to ±0.1 µs (≤ 0.003 %) in six conditions; −2.2 µs in the seventh, smaller than the 4.6 µs baseline drift of that condition. A control loop would add about 5 %. Injector times in these conditions: 4167, 2622, 2724, 4708 µs at idle (coolant 1000, 1200, 1500, 1800 mV) and 2715, 2820, 3313 µs at 2000 rpm. So the ECU does not regulate lambda in any condition tried. Not excluded: a coolant reading or other state that this setup has not produced, a long wait after start, or an ECU variant without lambda control (check the part number). | 2026-10-29 |
| ECU reaction to intake-air temperature (VW-9) | Injector time +3.1 % (500 mV), +4.4 % (3000 mV) and +6.0 % (sensor open) against the 1800 mV baseline, within 1 s and fully reversible; spark and current unchanged. Rising in both directions from 1800 mV is not monotonic: either a fault substitution at the extremes or a map minimum near 1.8 V. Needs a denser sweep (1000 to 2600 mV). | 2026-10-29 |
| ECU reaction to an open coolant sensor (VW-10) | Injector time +32.6 %, spark 6.8° earlier (142.8° to 136.0°), within 1 s, fully reversible. The 3300 mV point of the earlier sweep gave the same spark angle (135.9°) but a larger fuel change, so an open input is handled as its own condition, not as the coldest reading. | 2026-10-29 |
| ECU reaction to the idle contact (VW-11 open) at 850 rpm | Injector time +61.8 % (2622 to 4241 µs) with a 13 s time constant, spark 0.6° later, ECU current −11.8 mA; fully reversible. | 2026-10-29 |
| Anything that needed rework | (1) U6–U8: COM wired to NC, bodge wire pin 4→3 on each. (2) R10 1 k too weak for the ECU's injector stage (VW-12 stuck at 0.8 V, no pulses); added 220 Ω from VW-12 to J3 3V3. (3) VW-11 leakage 45 kΩ off, 374 kΩ after cleaning flux. (4) Y1 ordered as a 6-pin part (BOM mislabeled a Renesas XLH73 part as IQD); the real 4-pin equivalent is IQD CFPS-73 (LFSPXO018077). See DESIGN.md / BOM.md. | 2026-10-28 |

**Idle step tests (2026-10-29).** Baseline: 850 rpm, idle contact closed, airflow 1000 mV,
coolant 1200 mV, air 1800 mV, lambda 450 mV, 150 s settling (injector time 2622.8 ± 1.7 µs,
spark 142.78 ± 0.14°, current 0.3703 A, n = 40). Twelve tests in a seeded random order, each
20 s before, 60 s with one input moved and 45 s after returning to baseline, logged at 1 Hz;
two of the twelve were sham steps (no change) as the null. A response counts only when it is
far outside the sham steps (Welch t of the last 20 s of the step against the last 15 s before
it; shams gave \|t\| ≤ 1.0) and reverses on return. Scripts and data are not in the repo.

Once the answers are in, the firmware defaults (`CRANK_PPR_DEFAULT`,
`CRANK_DUTY_DEFAULT` in `firmware/src/crank.h`) and the open-question notes in
`firmware/README.md` get updated to match. That's also the point where a
first tagged firmware release makes sense.
