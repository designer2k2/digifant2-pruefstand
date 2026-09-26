# Pruefstand firmware (Raspberry Pi Pico, Pico SDK / C)

USB serial command protocol plus the hardware blocks behind it. The PC side
(`../host/bench.py`) and later Claude Code drive the bench through this.
Milestones so far: 1 skeleton and protocol, 2 crank signal, 3 sensor DAC.

## Build and flash

```sh
git clone --depth 1 --branch 2.1.1 https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk
(cd ~/pico-sdk && git submodule update --init --depth 1 lib/tinyusb)
export PICO_SDK_PATH=~/pico-sdk

cmake -S . -B build -DPICO_BOARD=pico
make -C build -j
```

Needs `cmake` and `gcc-arm-none-eabi` (plus `libnewlib-arm-none-eabi`). To flash,
hold BOOTSEL while plugging in the Pico and copy `build/pruefstand.uf2` onto the
`RPI-RP2` drive.

## What is real and what is a stub

| Module | State | Hardware |
|---|---|---|
| ECU power, idle switch, sensor disconnect | **real** | GP13, GP6, GP8/9/11 |
| VW-17 reference (`read` → `afm_ref_mv`) | **real** | GP26/ADC0, ×3 divider |
| Crank / Hall signal (`rpm`, `crank`) | **real** | GP2 → Q1, PIO0 SM0 |
| Sensor DACs (`dac`) | **real** | MCP4728 @ 0x60 |
| Current/voltage sense (`read` → `ecu_*`, `valve_*`) | stub, reports `na` | INA226 @ 0x40 / 0x41 |
| Knock DDS (`knock`) | stub, stores setpoint | AD9833 on SPI0 |

`status` lists the modules that are still stubs under `stubs=`.

Power-up state, set before any pin becomes an output: ECU **off**, idle switch
**open**, all three sensors **connected**, crank stopped (VW-18 high). The bench keeps its
state if USB is unplugged.

Signal polarities to keep in mind (see `src/pins.h`): GP2 high pulls VW-18
*low* (Q1 is an open-drain pull-down), and the ignition/injector capture inputs
(GP14/GP15) see *active-low* pulses.

## Crank / Hall signal

A PIO state machine generates the square wave on GP2 with cycle-exact timing.
Each phase length is queued in the TX FIFO and topped up from an interrupt,
so a busy main loop (e.g. USB output) can't stretch a period. Range: `rpm`
0–8000, where 0 stops the wave with VW-18 held high. The 32-bit phase counters
reach down to about 1 rpm, so cranking speeds are no problem (the RP2040's
PWM block can't go below ~7 Hz, which is why it isn't used).

Waveform, adjustable at runtime:

- `crank ppr <n>`: pulses per **crank** revolution. Default **2**: a
  4-cylinder distributor Hall sender with 4 vanes, turning at half crank speed.
- `crank duty <pct>`: percent of each period VW-18 is **high**. Default **50**.

**Both defaults are assumptions and still need confirming against a real
2H Hall sender or earlier HiL measurements.** They only change the timing;
nothing else depends on them.

A new `rpm`/`ppr`/`duty` takes effect once the phases already queued in the
FIFO have played out: up to about two and a half periods (~90 ms at 850 rpm,
2 ppr).

Timing is `crank_timing.c`, plain C: period = 60 s / (rpm × ppr) in system
clock cycles, split by duty, minus the program's fixed 4-cycle overhead per
phase. `test/test_crank_timing.c` checks it against hand-computed values, and
`test/emu_crank_pio.py` runs the assembled `crank.pio` in an independent PIO
emulator to confirm the N + 4 cycles-per-phase model it relies on.

## Sensor DAC (MCP4728)

All four channels use the chip's internal 2.048 V reference, which is more
accurate than the Pico's 3V3 rail:

| Channel | Output | Gain | Step | Range |
|---|---|---|---|---|
| `air` | VOUTA → R2 220 Ω → VW-9 | ×2 | 1 mV | 0–3300 mV |
| `water` | VOUTB → R3 220 Ω → VW-10 | ×2 | 1 mV | 0–3300 mV |
| `afm` | VOUTC → R4 220 Ω → VW-21 | ×2 | 1 mV | 0–3300 mV |
| `lambda` | VOUTD → R5 1 kΩ → VW-2 | ×1 | 0.5 mV | 0–2047 mV |

At gain ×2 the chip could reach 4.096 V, but the output can't exceed its
3.3 V supply, hence the 3300 mV limit (the top few tens of mV may clip).

Every write is an MCP4728 Multi-Write (datasheet DS22187E, fig. 5-8) that
carries the reference and gain bits along with the value. The chip powers up
from its own EEPROM settings, so this way a DAC reset can't leave a channel
silently mis-scaled. EEPROM is never written. At boot all four channels are set
to 0 V. `status` shows `dac_i2c=ok|err` for the last transaction, and a failed
write returns `ERR dac not responding on i2c`.

**`dac` sets the DAC output pin, not the voltage the ECU measures.** There's a
series resistor between them (220 Ω, or 1 kΩ for lambda), so the two only match
if the ECU input draws no current. If the ECU's NTC inputs have an internal
pull-up to 5 V (common for temperature inputs), the voltage at VW-9/VW-10 will
sit above the setpoint. That offset needs calibrating against the real ECU
(measure the pin with the sensor switched to `open`, then again under load)
before `dac` values can be mapped to temperatures.

## Protocol

ASCII over USB CDC (baud rate is ignored). One command per line (`\n` or `\r\n`),
case-insensitive. Every command gets exactly one reply line: `OK ...` with
`key=value` fields, or `ERR <reason>`. No echo.

| Command | Reply |
|---|---|
| `ping` | `OK pong fw=0.3.0` |
| `help` | `OK <usage of every command>` |
| `status` | `OK fw=… ecu=on/off idle=on/off rpm=… crank_ppr=… crank_duty=… sensor_<air/water/lambda>=conn/open dac_<air/water/afm/lambda>=<mV> dac_i2c=ok/err knock_hz=… stubs=…` |
| `read` | `OK afm_ref_mv=… ecu_v=… ecu_a=… valve_v=… valve_a=…` (`na` if not available) |
| `ecu on\|off` | `OK ecu=…` |
| `idle on\|off` | `OK idle=…` |
| `sensor air\|water\|lambda conn\|open` | `OK sensor_<name>=…` |
| `rpm <0..8000>` | `OK rpm=…` |
| `crank ppr <1..60>` / `crank duty <1..99>` | `OK crank_ppr=…` / `OK crank_duty=…` |
| `dac air\|water\|afm\|lambda <mV>` | `OK dac_<name>=…` (ranges per channel, see above) |
| `knock off\|<1..20000 Hz>` | `OK knock_hz=…` |

## Testing without hardware

`cmd.c`, `crank_timing.c` and `dac_codec.c` have no Pico dependencies, so they
build for the PC against the fakes in `test/` (in-memory state, VW-17 fixed at
5000 mV):

```sh
gcc -Isrc -o /tmp/bench-sim test/host_main.c test/fake_board.c test/fake_crank.c \
    test/fake_dac.c src/cmd.c src/crank_timing.c src/dac_codec.c src/sense.c src/knock.c
printf 'ecu on\nstatus\n' | /tmp/bench-sim

# crank timing math, and the PIO program in an emulator:
gcc -Isrc -o /tmp/test_crank test/test_crank_timing.c src/crank_timing.c && /tmp/test_crank
python3 test/emu_crank_pio.py

# MCP4728 frame bytes, decoded back to volts with the datasheet formula:
gcc -Isrc -o /tmp/test_dac test/test_dac_codec.c src/dac_codec.c && /tmp/test_dac

# or behind a virtual serial port, to exercise host/bench.py too:
socat PTY,link=/tmp/ttyBench,raw,echo=0 EXEC:/tmp/bench-sim,pty,raw,echo=0 &
python3 ../host/bench.py --port /tmp/ttyBench "rpm 850" read
```

That's how milestones 1–3 were checked. Nothing has run on a real Pico
yet: the crank wave needs a scope check on GP2/VW-18 and the DAC a multimeter
check on each VOUT once the board is built.
