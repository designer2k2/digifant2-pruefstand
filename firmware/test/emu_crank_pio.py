# Runs the assembled crank.pio in an independent PIO emulator and checks each
# phase lasts N + 4 cycles (the model crank_timing.c relies on).
# Needs: pip install git+https://github.com/NathanY3G/rp2040-pio-emulator
# Usage: python3 test/emu_crank_pio.py [build/crank.pio.h]
import re
import sys
from collections import deque
from pioemu import emulate, State

hdr = open(sys.argv[1] if len(sys.argv) > 1 else "build/crank.pio.h").read()
body = hdr.split("crank_program_instructions[] = {")[1].split("};")[0]
opcodes = [int(x, 16) for x in re.findall(r"0x([0-9a-f]{4})", body)]
wrap_target = int(re.search(r"crank_wrap_target (\d+)", hdr).group(1))
wrap_top = int(re.search(r"crank_wrap (\d+)", hdr).group(1))

def run(words, cycles):
    state = State(transmit_fifo=deque(words))
    edges = []  # (clock, new pin value)
    last = 0
    for before, after in emulate(opcodes, stop_when=lambda op, s: s.clock >= cycles,
                                 initial_state=state, set_base=0, set_count=1,
                                 wrap_target=wrap_target, wrap_top=wrap_top):
        v = after.pin_values & 1
        if v != last:
            edges.append((after.clock, v))
            last = v
    return edges

# alternate loop counts: pin-high phase N=10, pin-low phase N=20 -> expect 14 and 24 cycles
edges = run([10, 20, 10, 20, 10, 20], 300)
print("edges (clock, pin):", edges)
durations = [b[0] - a[0] for a, b in zip(edges, edges[1:])]
print("phase durations:", durations, "(expected alternating 14 = 10+4 high, 24 = 20+4 low)")
assert durations[1:5] == [24, 14, 24, 14] or durations[0:4] == [14, 24, 14, 24], durations

# zero-loop phases: N=0 -> 4 cycles each
edges = run([0, 0, 0, 0, 0, 0], 60)
d = [b[0] - a[0] for a, b in zip(edges, edges[1:])]
print("N=0 durations:", d)
assert all(x == 4 for x in d[:4]), d
print("PIO timing model N+4 confirmed")
