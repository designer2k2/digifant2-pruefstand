# Use-case verification matrix

Software-engineering approach applied to the hardware: every real bench
operation is a "requirement," traced through the actual schematic to concrete
numbers (voltage, current, timing, GPIO budget), the way a test matrix traces
requirements to acceptance tests. ✅ verified against real numbers below, ⚠️
works but with a caveat worth knowing, ❌ genuine gap.

This complements DESIGN.md's "Known open points" rather than duplicating it —
where a use case exposes one of those points, it's cited; three items below
were **new findings** from this pass, not previously written down (one, the
VW-17 divider, has since been fixed).

## Power / sequencing

| Use case | Path | Status | Verification |
|---|---|---|---|
| Cold-boot the ECU on firmware command | `ecu_power_switch` (Q2/Q3/R13-16/C3) | ✅ | Q3 ON drives Q2 gate to ~3.84 V (12 V × R14/(R13+R14)), Vgs ≈ −8.2 V — comfortably past a P-FET's few-volt Vgs(th). R13·C3 ≈ 10 kΩ × 10 nF = 100 µs turn-on slope, tames inrush. |
| Fail-safe OFF when Pico is unpowered/USB unplugged | R13 (gate→source), R16 (Q3 gate→GND) | ✅ | Both pull-downs reference the 12 V rail or GND directly, not +3V3 — stay valid with zero board logic power. Q2 cannot turn on without an explicit GP13 command, which itself requires +3V3 (Pico running) to issue — so "12 V applied before USB" is inherently safe: by the time the rail *could* come up, +3V3 (and therefore the DAC/sensor-sim outputs) is already live too. |
| Overcurrent shutdown | U1 (INA226 on RS1) → firmware → GP13 | ⚠️ | Hardware gives firmware everything needed (continuous current read, rail cutoff); the trip threshold/response itself is a firmware policy, not verifiable from the board alone. |
| Sustained near-max-current operation (ECU + valve inrush, ~2.5 A) | F1, D1, RS1 | ⚠️ **ties to an existing open point** | RS1 dissipates only ~0.13 W (2.5 A² × 0.02 Ω) — fine. **D1 (SS54) dissipates ~1.25 W at 2.5 A** (≈0.5 V forward drop) in an SMC package with no thermal copper added yet (DESIGN.md's open point) — this is the concrete consequence of that gap: without the copper pour, D1 running near the fuse limit for any length of time is a real thermal risk, not just a nice-to-have. |

## ECU current/voltage sensing

| Use case | Path | Status | Verification |
|---|---|---|---|
| Measure total ECU current, 0–2.5 A | U1 + RS1 (0.02 Ω) | ✅ | INA226 PGA range ±81.92 mV / 0.02 Ω = ±4.10 A — 64% headroom over the 2.5 A worst case. |
| Measure ECU supply voltage | U1 VBUS (on `+12V_ECU`) | ⚠️ | Reads upstream of Q2, so it's off by the switch's ~0.1 V drop from the actual VW-14 voltage — documented already, just firmware-correctable. |
| Measure idle-valve current, 0–1.8 A peak | U3 + RS2 (0.033 Ω) | ⚠️ **new finding** | INA226 range ±81.92 mV / 0.033 Ω = ±2.48 A — only ~38% headroom over the 1.8 A nominal peak. A **valve fault (stalled/locked coil, higher inrush)** — exactly the condition you'd most want to *see* — could clip this reading. Worth keeping in mind if fault-injection testing on the valve itself is ever a goal; not a problem for normal PWM operation. |
| Observe the ECU's own idle-valve PWM waveform cleanly | RS2, U3, VW-23 | ❌ (already known) | RS2 is a permanent parallel ground path alongside the ECU's own VW-23 driver — current-sensing works, but it's not an isolated view of just the ECU's drive. Documented in DESIGN.md. |
| Measure idle-valve supply voltage | U3 VBUS (on switched rail) | ✅ | Independent of the PWM node, reads what the valve is actually fed. |

## Sensor simulation

| Use case | Path | Status | Verification |
|---|---|---|---|
| Simulate intake-air-temp (NTC) value | U2 VOUTA → R2 (220 Ω) → VW-9 | ✅ | 12-bit DAC, 0–VDD range through a low-impedance series R — no loading concern against a typical ECU NTC input (MΩ-class). |
| Simulate coolant-temp (NTC) value | U2 VOUTB → R3 (220 Ω) → VW-10 | ✅ | Same as above. |
| Simulate airflow-meter (LMM) output | U2 VOUTC → R4 (220 Ω) → VW-21 | ✅ | Same; no fault-injection switch on this channel (by design — LMM doesn't have a documented open-sensor fault mode in scope here). |
| Simulate lambda/O₂ voltage | U2 VOUTD → R5 (1 kΩ) → VW-2 | ✅ | 1 kΩ (not 220 Ω) specifically so the source doesn't look like an ideal voltage source to the ECU's O₂ input — matches a real sensor's output impedance better. |
| Inject open-sensor fault: air, coolant, lambda | U6/U7/U8 (TS5A3159A) | ✅ | GPIO low → COM floats (NC pin left open) → ECU sees a genuine open circuit, not just a forced voltage — this is what actually exercises the ECU's fault-detection path, not just a wrong reading. |
| Sensors read "connected" at power-on (no false fault before firmware runs) | R22–R24 (100 kΩ pull-ups to +3V3) | ✅ | But **contingent on +3V3 being present** — see the sequencing note above; the whole DAC/switch subsystem is unpowered without it anyway, so there's no window where a false "open" fault could appear without also having no signal at all. |
| Read the airflow-pot reference (VW-17) to scale the injected wiper voltage | R20/R21 divider → GP26/ADC0 | ✅ **fixed** | R20 was 15 k (ratio 0.4, worst case 9 V → 3.6 V — at the RP2040 ADC's absolute-max rating, zero margin). Now 20 k (ratio 0.333): 9 V → 3.0 V, 0.6 V of real margin; nominal 5 V → 1.67 V, still comfortably mid-range. |

## Digital I/O emulation

| Use case | Path | Status | Verification |
|---|---|---|---|
| Drive crank/Hall signal at cranking-to-redline frequencies | Q1 (open-drain) + RPU1 (1 kΩ→12 V) | ✅ | RC-limited rise time is ~1 kΩ × (trace+input capacitance, tens of pF) ≈ tens of ns — no issue up into the MHz range, far beyond any real crank signal frequency (Hz–low kHz). Amplitude ≥8–10 V confirmed against the user's own HiL notes. |
| Idle-switch emulation (open/closed) | Q5 (open-drain) + R19 (100 kΩ→GND) | ✅ | Fails safe to "not idle" (open) with no board power, same reasoning as the ECU power switch. |
| Capture ignition edges | R8 (10 kΩ pull-up) + R9 (330 Ω series) → GP14 | ✅ | 0.32 mA load on the ECU's output when pulled low — negligible burden. |
| Capture injector edges, including the ~2.4 ms idle pulse | R10 (1 kΩ) + R11 (330 Ω) → GP15 | ✅ | Already bench-validated by the user (10 kΩ too weak, even 2.2 kΩ marginal) — 1 kΩ has real margin. |
| Measure ignition angle | Q1 crank drive (GP2) + R8/R9 ignition capture (GP14) | ✅ (firmware) | The bench *generates* the crank signal itself, so firmware already has an exact timestamp for every crank edge — no separate crank-sensing path needed. Angle = time from that known crank-edge timestamp to the captured ignition-edge timestamp, converted with the RPM the bench is already simulating. R9's 330 Ω + GPIO input capacitance adds tens of ns of propagation delay — negligible next to one crank-degree (27.8 µs at 6000 RPM). This gives angle *relative to the bench's own injected trigger edge*; an absolute "degrees BTDC" figure additionally needs Digifant-2's real trigger-wheel convention, a calibration/firmware detail, not a hardware one. |
| Generate a knock-sensor burst (5–15 kHz typical resonant range) | U4 (AD9833) + Y1 | ✅ (generation) / ❌ (delivery) | AD9833 covers that range trivially (up to ~12.5 MHz max). But the output only reaches **TP1**, not the VW connector — already documented; the DDS itself works, there's just no defined path to the ECU yet. |
| Local UI: menu/±  buttons | SW1–SW3 → GND | ⚠️ | No external pull-up in hardware — relies on firmware enabling the RP2040's internal pull-ups. Standard practice, just a firmware dependency to remember, not a board defect. |
| I²C bus: OLED + both INA226s + DAC coexist | 0x3C / 0x40 / 0x41 / 0x60, shared SDA/SCL | ✅ | No address collisions. 4.7 kΩ pull-ups are a normal Fast-Mode (400 kHz) value for a short on-board bus with this few devices. |
| GPIO budget for the two "not yet in this design" ideas (per-signal LEDs, fuel-pump monitor) | Pico GPIO map | ✅ | 8 GPIOs free (0, 1, 3, 7, 12, 17, 27, 28) — plenty of headroom if those get added later. |

## Explicitly out of scope (verified as correctly excluded, not gaps)

| Use case | Why it's out of scope |
|---|---|
| Digifant I (G40/G60) compatibility | Real, documented pin differences (VW-5 knock-gnd vs. CO-pot, inverted G60 ignition polarity) — DESIGN.md already states a variant needs its own harness/board revision, not firmware. |
| EPROM emulator / address tracing | Separate future board by design; this one is signal-sim + capture only. |

## Summary: what to act on before you'd call every use case covered

1. ~~**VW-17 divider headroom**~~ — fixed (R20 15k→20k).
2. **D1 thermal copper** — already on the to-do list, but now tied to a specific consequence: don't run sustained near-3 A tests until it's added.
3. **RS2's shared ground path** and **AD9833→ECU coupling** — both already known, both are "finish the design intent" items rather than layout bugs.
4. Everything else in this matrix is either ✅ verified with real numbers, or an inherent/expected limitation (average-current sensing on a PWM node, firmware-side pull-ups, firmware-defined trip thresholds) rather than something the hardware needs to change for.
