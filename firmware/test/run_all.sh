#!/usr/bin/env bash
# Runs every no-hardware test: the PC unit tests, the command parser behind a
# simulated serial port with host/bench.py, and (given the firmware build dir)
# the PIO program in the emulator. Used by CI; run it locally the same way:
#
#   firmware/test/run_all.sh [firmware/build]
#
# Needs gcc, python3 with pyserial and rp2040-pio-emulator (see
# requirements-test.txt), and socat for the bench.py check. With STRICT=1
# (as in CI) a skipped check counts as a failure.
set -euo pipefail

FW="$(cd "$(dirname "$0")/.." && pwd)"
REPO="$(dirname "$FW")"
BUILD="${1:-$FW/build}"
BUILD="$(cd "$BUILD" 2>/dev/null && pwd || echo "$BUILD")"
OUT="$(mktemp -d)"
trap 'kill "${SOCAT_PID:-}" 2>/dev/null || true; rm -rf "$OUT"' EXIT
cd "$FW"

CFLAGS=(-std=c11 -Wall -Wextra -Werror -Isrc)
SIM=(-Itest/fake_sdk test/fake_sdk/sim.c)
FAKES=(test/fake_board.c test/fake_crank.c test/fake_dac.c test/fake_sense.c test/fake_capture.c)
failed=0

skip() {
    echo "skipped ($1)"
    if [ "${STRICT:-0}" = 1 ]; then failed=1; fi
}

# run <name> <sources...>: builds test <name> and runs it, keeping its output
# only on failure.
run() {
    local name="$1"; shift
    printf '%-22s ' "$name"
    if ! gcc "${CFLAGS[@]}" -o "$OUT/$name" "$@" -lm 2>"$OUT/$name.log"; then
        echo "BUILD FAILED"; cat "$OUT/$name.log"; failed=1; return
    fi
    if "$OUT/$name" "$OUT/$name.pbm" >"$OUT/$name.log" 2>&1; then
        echo "ok"
    else
        echo "FAILED"; grep -v '^[.#]*$' "$OUT/$name.log"; failed=1
    fi
}

run test_crank_timing  test/test_crank_timing.c src/crank_timing.c
run test_dac_codec     test/test_dac_codec.c src/dac_codec.c
run test_dac_driver    test/test_dac_driver.c src/dac.c src/dac_codec.c "${SIM[@]}"
run test_sense_codec   test/test_sense_codec.c src/sense_codec.c
run test_knock_codec   test/test_knock_codec.c src/knock_codec.c
run test_knock_burst   test/test_knock_burst.c src/knock.c src/knock_codec.c "${SIM[@]}"
run test_capture_core  test/test_capture_core.c src/capture_core.c
run test_ecu           test/test_ecu.c src/ecu.c test/fake_board.c test/fake_sense.c
run test_ui            test/test_ui.c "${SIM[@]}" "${FAKES[@]}" src/ui_core.c src/gfx.c \
                       src/crank_timing.c src/dac_codec.c src/ecu.c src/knock.c src/knock_codec.c

# Command parser on the PC, fed the same way the Pico is.
printf '%-22s ' "command parser"
gcc "${CFLAGS[@]}" -o "$OUT/bench-sim" test/host_main.c "${SIM[@]}" "${FAKES[@]}" \
    src/cmd.c src/crank_timing.c src/dac_codec.c src/ecu.c src/knock.c src/knock_codec.c
expected='OK ecu=on
OK rpm=850
ERR rpm must be 0..8000
OK ecu_trip_ma=3000
ERR unknown command '"'"'bogus'"'"', try help'
got="$(printf 'ECU on\nrpm 850\nrpm -1\necu trip 3000\nbogus\n' | "$OUT/bench-sim")"
if [ "$got" = "$expected" ]; then echo "ok"; else
    echo "FAILED"; diff <(echo "$expected") <(echo "$got") || true; failed=1
fi

# host/bench.py against the parser behind a virtual serial port.
printf '%-22s ' "bench.py via socat"
if command -v socat >/dev/null; then
    socat "PTY,link=$OUT/tty,raw,echo=0" "EXEC:$OUT/bench-sim,pty,raw,echo=0" &
    SOCAT_PID=$!
    for _ in $(seq 50); do [ -e "$OUT/tty" ] && break; sleep 0.1; done
    if python3 "$REPO/host/bench.py" --port "$OUT/tty" "ecu on" status >"$OUT/bench.log" 2>&1 &&
       grep -q '^OK fw=.* ecu=on ' "$OUT/bench.log" &&
       ! python3 "$REPO/host/bench.py" --port "$OUT/tty" "rpm 99999" >/dev/null 2>&1; then
        echo "ok"
    else
        echo "FAILED"; cat "$OUT/bench.log"; failed=1
    fi
else
    skip "no socat"
fi

# The assembled PIO program in an independent emulator.
printf '%-22s ' "crank.pio emulator"
if [ -f "$BUILD/crank.pio.h" ]; then
    if python3 test/emu_crank_pio.py "$BUILD/crank.pio.h" >"$OUT/emu.log" 2>&1; then echo "ok"
    else echo "FAILED"; cat "$OUT/emu.log"; failed=1; fi
else
    skip "no $BUILD/crank.pio.h, build the firmware first"
fi

if [ "$failed" -ne 0 ]; then echo "SOME TESTS FAILED"; exit 1; fi
echo "all tests passed"
