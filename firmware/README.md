# Pruefstand firmware (Raspberry Pi Pico, Pico SDK / C)

Milestone 1: project skeleton and the USB serial command protocol. The PC side
(`../host/bench.py`) and later Claude Code drive the bench through this.

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
| Crank / Hall signal (`rpm`) | stub, stores setpoint | GP2 → Q1 (PIO, next) |
| Sensor DACs (`dac`) | stub, stores setpoint | MCP4728 @ 0x60 |
| Current/voltage sense (`read` → `ecu_*`, `valve_*`) | stub, reports `na` | INA226 @ 0x40 / 0x41 |
| Knock DDS (`knock`) | stub, stores setpoint | AD9833 on SPI0 |

`status` lists the modules that are still stubs under `stubs=`.

Power-up state, set before any pin becomes an output: ECU **off**, idle switch
**open**, all three sensors **connected**, crank output low. The bench keeps its
state if USB is unplugged.

Signal polarities to keep in mind (see `src/pins.h`): GP2 high pulls VW-18
*low* (Q1 is an open-drain pull-down), and the ignition/injector capture inputs
(GP14/GP15) see *active-low* pulses.

## Protocol

ASCII over USB CDC (baud rate is ignored). One command per line (`\n` or `\r\n`),
case-insensitive. Every command gets exactly one reply line: `OK ...` with
`key=value` fields, or `ERR <reason>`. No echo.

| Command | Reply |
|---|---|
| `ping` | `OK pong fw=0.1.0` |
| `help` | `OK <usage of every command>` |
| `status` | `OK fw=… ecu=on/off idle=on/off rpm=… sensor_<air/water/lambda>=conn/open dac_<air/water/afm/lambda>=<mV> knock_hz=… stubs=…` |
| `read` | `OK afm_ref_mv=… ecu_v=… ecu_a=… valve_v=… valve_a=…` (`na` if not available) |
| `ecu on\|off` | `OK ecu=…` |
| `idle on\|off` | `OK idle=…` |
| `sensor air\|water\|lambda conn\|open` | `OK sensor_<name>=…` |
| `rpm <0..8000>` | `OK rpm=…` |
| `dac air\|water\|afm\|lambda <0..3300 mV>` | `OK dac_<name>=…` |
| `knock off\|<1..20000 Hz>` | `OK knock_hz=…` |

## Testing without hardware

`cmd.c` has no Pico dependencies of its own, so the parser builds for the PC
against `test/fake_board.c` (in-memory GPIO state, VW-17 fixed at 5000 mV):

```sh
gcc -Isrc -o /tmp/bench-sim test/host_main.c test/fake_board.c \
    src/cmd.c src/crank.c src/dac.c src/sense.c src/knock.c
printf 'ecu on\nstatus\n' | /tmp/bench-sim

# or behind a virtual serial port, to exercise host/bench.py too:
socat PTY,link=/tmp/ttyBench,raw,echo=0 EXEC:/tmp/bench-sim,pty,raw,echo=0 &
python3 ../host/bench.py --port /tmp/ttyBench "rpm 850" read
```

That's how milestone 1 was checked. Nothing has run on a real Pico yet.
