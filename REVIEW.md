# Review findings

Review of firmware, schematic source (`digifant2_pruefstand.py`), PCB
(`board/digifant2-pruefstand.kicad_pcb`), BOM and docs, 2026-11-16.
Firmware 0.10.0, commit `4ba191b` plus uncommitted `BRINGUP.md` / `DESIGN.md` edits.

Evidence marks: **V** = verified in the code or the PCB file, **M** = measured
on the bench during bring-up, **A** = assumption that still needs the check
given under "Verify".

Pin assignments in `firmware/src/pins.h` match the PCB nets on every pin (V),
and no GPIO is used in a way the RP2350 does not support (V).

## Order and dependencies

| Step | Findings | Why this order |
|---|---|---|
| 1 | F01, F02 | Firmware safety. Small, no hardware needed. F02 builds on F01's trip path. |
| 2 | F03, F05, F10 | Scope measurements before more hardware is connected. F03 before any 12 V pull-up experiments, F05 before the real idle valve is driven. |
| 3 | F06 | Bodges must be on the board before U4 and Y1 are fitted. |
| 4 | F11, then F01 again | Dummy-load calibration. Its result replaces the single-point factor F01 depends on. |
| 5 | F09, F14, F15, F16 to F21 | Firmware robustness and cleanups. F09's priority change must keep the GPIO and timer interrupts equal (see F09). |
| 6 | F04, F07, F08, F12, F13, F22, F23 | Rev-B schematic and layout changes, done in one pass. F03 and F05 add their parts here too. |

---

## Critical

### F01 — Over-current trip cannot fire above about 2.77 A

- [x] Done
- **Priority:** Critical
- **Evidence:** V (code), M (shunt factor)
- **Files:** `firmware/src/ecu.h:14-16`, `firmware/src/sense.c:23-24`, `firmware/src/ecu.c:43-48`
- **Problem:** `ECU_TRIP_MA_MAX` is 4000 mA and its comment says U1's range is
  4.09 A. That was true for a 0.020 Ω shunt. U1 really sees 0.0296 Ω
  (`RS1_EFFECTIVE_OHM`), so its ±81.92 mV input saturates at 2.77 A. Any trip
  setting above that can never be reached, and the reading clamps silently.
- **Why it matters:** `ecu trip 3000` is accepted and gives no protection. The
  default 2500 mA has only 10 % headroom, and the extra 9.6 mΩ is copper trace,
  which drifts with temperature.
- **Fix:** Derive the maximum from the shunt value (`81.92 mV / shunt × 0.9`),
  which gives about 2490 mA now. Treat a shunt reading at or near full scale
  (raw ≥ 0x7F00) as over-current regardless of the limit. Update the comment.
- **Verify:** Host test in `firmware/test/test_ecu.c`: a full-scale reading
  trips at any limit; a limit above the derived maximum is refused. On the
  bench: `ecu trip 3000` returns `ERR`.
- **Depends on:** F11 (the factor changes after calibration and after the
  rev-B layout fix; keep the limit derived, not hard-coded).

### F02 — No trip when U1 does not answer

- [ ] Done
- **Priority:** Critical
- **Evidence:** V
- **Files:** `firmware/src/ecu.c:41-42`, `firmware/src/ecu.h:9-13`
- **Problem:** `ecu_poll` returns when the INA226 reading is invalid. The ECU
  stays on with no current supervision. The header documents this, but nothing
  reports it except `ecu_a=na` in `read`.
- **Why it matters:** An I2C fault or a failed U1 leaves a 3 A soldered fuse as
  the only protection, and the operator is not told.
- **Fix:** Count consecutive invalid readings. After about 5 (750 ms), switch
  the ECU off and latch a fault `sense_lost`, cleared by `ecu reset`. Show it in
  `status` (`ecu_fault=sense_lost`) and on the status LED.
- **Verify:** Host test with the fake sense returning invalid: the ECU switches
  off after the count and `ecu on` is refused until reset. Bench: lift U1's
  address by shorting SDA briefly, the ECU switches off.
- **Depends on:** F01 (same trip and latch code).

### F03 — Capture inputs have no over-voltage clamp

- [ ] Done
- **Priority:** Critical
- **Evidence:** V (circuit), A (the ECU's high level on these pins)
- **Files:** `digifant2_pruefstand.py:314-332` (R8 to R11), `firmware/src/board.c:29-35`
- **Problem:** VW-25 and VW-12 reach GP14 / GP15 through 330 Ω only. RP2350
  pins tolerate 5 V while powered, not 12 V. Nothing limits the pin voltage if
  the ECU output, a miswire or an added 12 V pull-up (as on the old bench) drives
  the line high.
- **Why it matters:** One wrong pull-up or harness slip destroys the Pico's
  pins, and the Pico controls ECU power.
- **Fix:** Now: scope both pins before changing any pull-up; never fit a 12 V
  pull-up on VW-12 or VW-25. Rev B: R9 / R11 to 10 kΩ and a BAT54S clamp to 3V3
  and GND at each GPIO. Keep R10 (220 Ω to 3V3) for the injector load.
- **Verify:** Scope VW-25 and VW-12 against GND with the ECU running: high
  level must stay ≤ 3.6 V. After the rev-B change, apply 12 V through 1 kΩ to
  each input and measure ≤ 3.7 V at the GPIO.

### F04 — 12 V terminals sit beside unprotected DAC outputs on J0

- [ ] Done
- **Priority:** Critical
- **Evidence:** V (PCB pad nets)
- **Files:** `digifant2_pruefstand.py:256-261, 481-496`, PCB J0 pads 8, 9, 12, 13
- **Problem:** J0 pad 13 (VW-22, +12 V switched) is next to pad 12 (VW-21, AFM
  DAC output through 220 Ω, no switch). Pad 8 (VW-14, +12 V switched) is next to
  pad 9 (VW-17). None of the four DAC lines has a clamp. U2 and the TS5A3159A
  switches are rated to about 6.5 V.
- **Why it matters:** A stray wire strand at the screw terminals puts 12 V into
  U2 through 220 Ω (about 40 mA into its protection diodes).
- **Fix:** Rev B: a BAT54S clamp to 3V3 / GND on the ECU side of each series
  resistor (VW-2, VW-9, VW-10, VW-21), and reorder J0 or add a spare terminal
  so 12 V pins are not beside signal pins. Now: ferrules on every harness wire.
- **Verify:** After the change, apply 12 V through 220 Ω to each DAC line with
  the board powered: U2 still answers and `dac` values read back correctly at
  the terminal.

### F05 — Idle-valve flyback may exceed U3's input rating

- [ ] Done
- **Priority:** Critical (before the real valve is driven)
- **Evidence:** V (no diode on the board), A (the ECU's clamp voltage)
- **Files:** `digifant2_pruefstand.py:141-191`, `DESIGN.md:81-93`
- **Problem:** J2 has no freewheel diode. When the ECU's low-side driver turns
  off, VW-23 rises to whatever the ECU clamps at. U3's IN+ / IN− are on that
  node and are rated to 36 V (40 V absolute).
- **Why it matters:** A clamp above 36 V damages U3. A missing clamp in the
  ECU damages more than U3.
- **Fix:** Scope VW-23 with the 22 Ω dummy load first, then with the valve. If
  the peak exceeds about 30 V, fit a diode across J2 (cathode to +12 V switched)
  or a 27 V TVS from VW-23 to GND. Add the footprint in rev B either way.
- **Verify:** Scope VW-23 at idle with the valve connected: peak below 30 V.
  U3 keeps answering on I2C over a 10 minute run.

---

## Should fix

### F06 — AD9833 section: COMP capacitor, decoupling, output coupling

- [ ] Done
- **Priority:** Should fix (before U4 and Y1 are fitted)
- **Evidence:** V (wiring), A (datasheet requirement for COMP)
- **Files:** `digifant2_pruefstand.py:335-392` (C7 at 372-376, R12 / C2 at 380-392), PCB U4 pads 1, 2, 3, 10
- **Problem:**
  1. C7 connects COMP to GND. From memory, the AD9833 datasheet connects the
     COMP decoupling capacitor to VDD.
  2. U4 pin 2 (VDD) and Y1 pin 4 (V+) have no decoupling capacitor.
  3. The output is DC-coupled (about 0.04 to 0.65 V, 0.34 V offset) into
     R12 200 Ω and C2 100 nF to GND, a low-pass with an 8 kHz corner. There is
     no amplitude control. RG1 is Q1's gate resistor, not a trimmer.
- **Why it matters:** Wrong DAC bias decoupling and missing supply decoupling
  give a noisy or distorted tone. The DC offset lands on the ECU's knock input,
  and the level cannot be matched to a real sensor.
- **Fix:** Check the datasheet's COMP figure first. If confirmed, bodge C7 from
  COMP to VDD. Add 100 nF at U4 pin 2 and at Y1 (bodge across the pads). Rev B:
  move C2 to 10 nF, add a 1 µF series capacitor and a 10 kΩ trimmer divider
  before TP1.
- **Verify:** Scope TP1 with `knock 7000`: clean sine, no clock feedthrough
  above a few mV. After rev B: no DC at TP1, amplitude adjustable.

### F07 — AFM channel stops at 3.3 V and is not ratiometric

- [ ] Done
- **Priority:** Should fix (rev B)
- **Evidence:** V
- **Files:** `firmware/src/dac_codec.c:14-19`, `digifant2_pruefstand.py:194-212, 259`, `firmware/src/board.c:118-122`
- **Problem:** The AFM channel is limited to 3300 mV and uses the DAC's
  internal reference. The real wiper works against the 5 V on VW-17 and goes
  well above 3.3 V at high airflow. `afm_ref_mv` is read but not used to scale
  the output.
- **Why it matters:** The upper load range cannot be simulated, and the signal
  does not follow a drooping VW-17 as a real potentiometer would.
- **Fix:** Rev B: a rail-to-rail op-amp with gain 1.5 on channel C, supplied
  from VBUS 5 V. Firmware: an optional `afm` mode that takes a percentage of
  `afm_ref_mv`.
- **Verify:** `dac afm` sweep to the new maximum reads ≥ 4.5 V at VW-21 with
  the ECU connected. Compare `afm_ref_mv` against a meter on VW-17 first.

### F08 — Sensor ground shares the power return

- [ ] Done
- **Priority:** Should fix
- **Evidence:** M (18 % fuel change with VW-6 open), A (size of the offset)
- **Files:** `digifant2_pruefstand.py:482, 487, 491`, `BRINGUP.md:13`
- **Problem:** VW-6 (sensor ground), VW-13 and VW-19 are one net on the board.
  The DAC is referenced to board GND, so the ECU supply current's drop in the
  ground wires adds to every sensor voltage. Nothing detects a missing VW-6.
- **Why it matters:** With VW-6 open, all absolute results before 2026-11-02
  evening were wrong. With it connected, an offset of some tens of mV remains
  possible, which is large against the 0 to 1 V lambda signal.
- **Fix:** Now: add "VW-6 to VW-19 continuity" as a checkbox in `BRINGUP.md`
  stage 6, and keep the three ground wires short and equal. Rev B: route VW-6 as
  its own return to U2's ground pin (single-point connection).
- **Verify:** With the ECU running, measure VW-6 against U2 pin 4 with a
  meter in mV range: below 5 mV, also while the idle valve is driven.

### F09 — Timing that depends on interrupts and the main loop

- [ ] Done
- **Priority:** Should fix
- **Evidence:** V
- **Files:** `firmware/src/capture.c:27-58`, `firmware/src/knock.c:52-88`, `firmware/src/crank.c:48-49`, `firmware/src/wave.c:97-107`, `firmware/src/main.c:58-67`, `firmware/src/ui.c:53-58`
- **Problem:**
  1. Capture timestamps are taken on entry to the GPIO interrupt, which has the
     same priority as USB. A USB interrupt in progress delays the timestamp.
  2. The wave generator (200 Hz), the over-current poll and DAC writes run from
     the main loop. `log read` (up to 200 lines) and an OLED page (about 3 ms)
     stall them.
  3. A new crank speed takes effect only after up to two queued periods.
  The crank output itself is PIO-timed and exact.
- **Why it matters:** Occasional late timestamps look like ECU jitter. Waves
  above a few Hz get uneven steps.
- **Fix:** Set `IO_IRQ_BANK0` and the knock alarm's timer interrupt to the same
  higher priority than USB (they must stay equal: `knock_on_crank_ref` and
  `alarm_isr` share state without locking). Longer term, measure pulses in a
  PIO state machine. Print `log read` in chunks across loop passes.
- **Verify:** Log 10 000 injector pulses at a fixed condition before and
  after: the maximum deviation from the mean drops, the standard deviation
  (about 1.5 µs now) does not rise. `firmware/test/run_all.sh` passes.

### F10 — Hall signal: reference edge and duty are unverified

- [ ] Done
- **Priority:** Should fix
- **Evidence:** M (injection follows the rising edge), A (real distributor duty)
- **Files:** `firmware/src/capture.c:22-29`, `firmware/src/crank.h:14`, `firmware/src/crank_timing.c:15-19`
- **Problem:** Capture angles are measured from the falling edge of VW-18. The
  ECU starts injection 0.25 ms after the rising edge. So reported angles move
  with `crank duty`, and the 50 % default is not checked against a real
  distributor's vane-to-window ratio.
- **Why it matters:** Ignition angles from the bench cannot be compared with
  workshop values until the edge and the duty are right.
- **Fix:** Measure the duty of a real Digifant distributor (scope, or the
  vane geometry). Set that as the default. Report angles from the edge the ECU
  uses, or report both.
- **Verify:** Change `crank duty` from 30 to 70: the reported ignition angle
  relative to the chosen edge stays within 0.5°.

### F11 — U3 has no correction factor; shunt factors are tied to the board revision

- [ ] Done
- **Priority:** Should fix
- **Evidence:** V (code), A (20 to 45 % predicted error from `DESIGN.md`)
- **Files:** `firmware/src/sense.c:16-25`, `DESIGN.md` "Pending changes for next revision"
- **Problem:** U3 uses the nominal 0.033 Ω although its IN+ sense trace has the
  same non-Kelvin flaw as U1 had. U1's 0.0296 Ω rests on one measurement point
  and must be removed when the fixed layout is built.
- **Why it matters:** `valve_a` reads high by an unknown amount, and a rev-B
  board with the old constant would read 32 % low on `ecu_a`.
- **Fix:** Calibrate both with the 47 Ω and 22 Ω dummy loads at three currents.
  Put both effective values behind one `BOARD_REV` define.
- **Verify:** `read` against a meter in series at 0.25, 0.5 and 1.0 A: within
  2 % for both channels.

### F12 — Switch-on inrush through a soldered fuse

- [ ] Done
- **Priority:** Should fix
- **Evidence:** V (values), A (inrush estimate)
- **Files:** `digifant2_pruefstand.py:22-23, 35, 51, 57-93`
- **Problem:** Q2's gate network (R13 10 kΩ, R14 4.7 kΩ, C3 10 nF) turns the
  switch on in about 30 µs into C1 (100 µF) plus the ECU's own capacitance.
  F1 is a 3 A fast 1206 fuse soldered to the board.
- **Why it matters:** Each `ecu on` stresses F1 near its pulse rating on a
  stiff supply. A blown fuse means rework.
- **Fix:** C3 to 470 nF (about 1.5 ms ramp). Rev B: a fuse holder or a
  time-lag fuse.
- **Verify:** Current probe or scope across RS1 at `ecu on` from a supply
  without current limit: peak below 6 A.

### F13 — Q1 gate floats before boot; 2N7002 at 3.3 V drive

- [ ] Done
- **Priority:** Should fix
- **Evidence:** V (no pull-down), A (drive margin)
- **Files:** `digifant2_pruefstand.py:289-311`
- **Problem:** Q1 has gate resistor RG1 but no gate pull-down, unlike Q3 (R16)
  and Q5 (R19). The 2N7002's on-resistance is specified at 4.5 V and above; its
  threshold can be up to 2.5 V.
- **Why it matters:** VW-18 is undefined while the Pico is in reset or
  BOOTSEL, and a weak part may not pull VW-18 fully low at 12 mA.
- **Fix:** 100 kΩ from Q1 gate to GND. Rev B: BSS138 or another FET specified
  at 2.5 V gate drive for Q1, Q3 and Q5.
- **Verify:** VW-18 low level below 0.3 V at `rpm 800` on the scope; VW-18
  stays high with the Pico held in BOOTSEL and 12 V applied to the pull-up.

### F14 — I2C bus recovery runs only at boot

- [ ] Done
- **Priority:** Should fix
- **Evidence:** V
- **Files:** `firmware/src/board.c:37-60, 83-86`, `firmware/src/sense.c:27-38`, `firmware/src/dac.c:15-20`, `firmware/src/oled.c:10`
- **Problem:** `i2c_bus_recover` frees a stuck SDA once in `board_init`. A bus
  that locks up later (hot-plugging the OLED on J3, a glitch) is never
  recovered; every transfer then times out (5 ms, 50 ms for the OLED).
- **Why it matters:** The DAC stops updating and current sensing is lost (see
  F02) until a power cycle.
- **Fix:** After several consecutive failures across devices, deinit the I2C
  block, run the recovery, re-init. Lower the OLED timeout to 10 ms.
- **Verify:** Short SDA to GND for one second while running: `status` shows
  `dac_i2c=ok` again within two seconds after release.

### F15 — The checked-in netlist is stale

- [ ] Done
- **Priority:** Should fix
- **Evidence:** V
- **Files:** `board/digifant2-pruefstand.net`
- **Problem:** The file still has VW-9 / VW-10 / VW-2 on pin 3 of U6 to U8, RS2
  to GND in parallel with the ECU driver, U3 IN− on GND, and no C7 to C10 or
  TP2 to TP14. The PCB file and the Python source are correct.
- **Why it matters:** Anyone checking the design against this file finds bugs
  that are already fixed, or misses real ones.
- **Fix:** Regenerate it from the current schematic, or delete it and
  `.gitignore` it.
- **Verify:** In the new file: `VW_PIN9` is on `U6.4`, `RS2.2` is on
  `VW_PIN23_VALVE_RETURN`.

---

## Nice to have

### F16 — RP2350 notes and stale RP2040 comments

- [ ] Done
- **Priority:** Nice to have
- **Evidence:** V
- **Files:** `firmware/src/board.c:29-30`, `digifant2_pruefstand.py:96-101, 289-293, 541-549`
- **Problem:** No internal pull-downs are used (buttons use pull-ups, capture
  inputs have external pull-ups, the ADC pin has its digital input off), so
  erratum RP2350-E9 does not apply. But comments still say RP2040, the SK6812
  docstring claims 3V3 supply is in spec, and the crank docstring names the
  wrong rail.
- **Why it matters:** A later change that adds an internal pull-down input
  would hit E9 without warning.
- **Fix:** Correct the comments and add one line in `pins.h`: "RP2350-E9: no
  internal pull-downs on inputs; use an external ≤ 8.2 kΩ".
- **Verify:** `grep -rn pull_down firmware/src` stays empty; build passes.

### F17 — SK6812 supply and bit timing

- [ ] Done
- **Priority:** Nice to have
- **Evidence:** V (circuit, timing), M (works on the built board)
- **Files:** `digifant2_pruefstand.py:96-111`, `firmware/src/status_led.pio`, `BOM.md` (D2 row)
- **Problem:** D2 runs from 3V3, below the part's 3.5 V minimum. The data
  level is fine because the supply is also 3V3. The 1-bit high time is 875 ns,
  above the SK6812's 750 ns maximum (within WS2812B's).
- **Why it matters:** Another LED batch may not start or may show wrong
  colours.
- **Fix:** Rev B: supply D2 from VBUS 5 V with a 74AHCT1G125 on the data line,
  or keep 3V3 and shorten the 1-bit to 6 of 10 cycles.
- **Verify:** Colours correct for all four states with a second LED sample.

### F18 — ADC_VREF and RUN tied directly to 3V3

- [ ] Done
- **Priority:** Nice to have
- **Evidence:** V
- **Files:** `digifant2_pruefstand.py:557-562`, `firmware/src/board.c:118-122`
- **Problem:** Tying ADC_VREF to 3V3 bypasses the Pico's own reference filter.
  RUN tied to 3V3 prevents a reset button. `afm_ref_mv` is a single
  unaveraged sample.
- **Why it matters:** `afm_ref_mv` is noisier than needed, and BOOTSEL needs a
  USB replug.
- **Fix:** Rev B: leave ADC_VREF open, RUN to a button to GND. Firmware:
  average 16 samples.
- **Verify:** 100 `read` calls: `afm_ref_mv` spread below 10 mV; compare the
  mean with a meter on VW-17.

### F19 — DAC outputs before `dac_init`

- [ ] Done
- **Priority:** Nice to have
- **Evidence:** A (EEPROM default not checked)
- **Files:** `firmware/src/dac.c:22-34`
- **Problem:** From power-up until `dac_init`, U2 drives whatever its EEPROM
  holds into the unpowered ECU inputs.
- **Why it matters:** Small, short back-feed; harmless if the default is 0 V.
- **Fix:** Measure the outputs with the Pico held in BOOTSEL. If not 0 V,
  add a one-time command that writes 0 V and the internal reference to the
  EEPROM.
- **Verify:** VW-21 at 0 V with the Pico in BOOTSEL after a power cycle.

### F20 — INA226 ALERT pins unused

- [ ] Done
- **Priority:** Nice to have (rev B)
- **Evidence:** V
- **Files:** `digifant2_pruefstand.py:133-135, 188`
- **Problem:** Both ALERT pins are unconnected, so the trip is software-only
  and takes at least 300 ms.
- **Why it matters:** A hard short is left to F1.
- **Fix:** Rev B: U1 ALERT to a free GPIO (GP7 or GP12) with a pull-up, and
  program the shunt over-voltage limit with latch.
- **Verify:** Short the switched rail through 2 Ω: the rail is off within
  20 ms on the scope.

### F21 — No speed ramp in the firmware

- [ ] Done
- **Priority:** Nice to have
- **Evidence:** V
- **Files:** `firmware/src/crank.c:91-94`, `firmware/src/cmd.c:178-191`
- **Problem:** `rpm` takes whole numbers and steps. Ramps must come from the
  host, with USB timing.
- **Why it matters:** Deceleration fuel cut-off and acceleration enrichment
  tests need smooth, repeatable ramps.
- **Fix:** `rpm ramp <from> <to> <seconds>`, updated once per crank period.
- **Verify:** Host test of the ramp steps; on the bench the injector period
  log shows a monotonic change.

---

## Rev-B mechanical and BOM

### F22 — Rev-B silkscreen and mechanical list

- [ ] Done
- **Priority:** Should fix (rev B)
- **Evidence:** V for J3 labels and H1 to H4 in the PCB file; U6 to U8 pin-1 marks and moved J0 labels are stated in `DESIGN.md:566-583` and not checked on the silkscreen layer
- **Files:** `board/digifant2-pruefstand.kicad_pcb`, `DESIGN.md:445-452, 566-583`
- **Problem:** Items from assembly: pin-1 marks on U6 to U8, J0 labels under
  the cables, J3 pin order, Pico on sockets, mounting holes. The Pico pads have
  a 1.02 mm drill, which is tight for header posts.
- **Why it matters:** Assembly errors on the next build.
- **Fix:** Render the silkscreen and confirm the three label fixes. Enlarge
  the Pico pad drill to 1.1 mm. Add a pin-1 mark for Y1 and polarity marks for
  D1, D2 and C1 if missing.
- **Verify:** Rendered F.Silkscreen checked against this list; a header strip
  fits a test coupon or the old board's spare holes.

### F23 — BOM gaps

- [ ] Done
- **Priority:** Should fix
- **Evidence:** V (`BOM.md` against the PCB references); `mouser_cart.csv` not checked
- **Files:** `BOM.md`, `mouser_cart.csv`
- **Problem:** Every reference on the PCB has a `BOM.md` row, including R9,
  R11, R17, R12, RG1, U4, both INA226, D2 and the socket headers. Not listed:
  the OLED module for J3, M3 screws and nuts for the standoffs, the 47 Ω and
  22 Ω dummy loads, a pigtail for TP1 / TP2, spare F1 fuses, and the parts
  this review adds (F03 to F06, F12, F13).
- **Why it matters:** The next order is incomplete again.
- **Fix:** Add the rows. Check `mouser_cart.csv` line by line against
  `BOM.md`.
- **Verify:** A script or manual pass: every `BOM.md` reference appears in the
  cart with the right quantity.
