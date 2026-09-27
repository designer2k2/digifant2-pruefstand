#!/usr/bin/env python3
"""Sweeps the simulated engine speed and logs what the ECU does at each step:
ignition angles and dwell, injector open time, ECU current. Writes a CSV.

    python3 host/examples/rpm_sweep.py                    # auto-detect the Pico
    python3 host/examples/rpm_sweep.py --port /dev/ttyACM0 --out sweep.csv

Sets a warm-idle sensor picture first; adjust WARM_IDLE to taste.
"""
import argparse
import csv
import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from bench import Bench  # noqa: E402

WARM_IDLE = ["dac water 800", "dac air 1800", "dac afm 1000", "dac lambda 450", "idle on"]
FIELDS = ["rpm", "ign_fall_deg", "ign_rise_deg", "ign_low_us", "inj_low_us", "inj_fall_deg",
          "ecu_a", "ecu_v"]


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port", help="serial port (default: auto-detect)")
    ap.add_argument("--out", default="rpm_sweep.csv")
    ap.add_argument("--start", type=int, default=850)
    ap.add_argument("--stop", type=int, default=4000)
    ap.add_argument("--step", type=int, default=250)
    ap.add_argument("--settle", type=float, default=1.5,
                    help="seconds to wait after each rpm change (ECU and averaging settle)")
    args = ap.parse_args()

    with Bench(args.port) as b, open(args.out, "w", newline="") as f:
        out = csv.DictWriter(f, fieldnames=FIELDS)
        out.writeheader()
        b.cmd("ecu on")
        for c in WARM_IDLE:
            b.cmd(c)
        try:
            for rpm in range(args.start, args.stop + 1, args.step):
                b.cmd(f"rpm {rpm}")
                time.sleep(args.settle)
                row = {"rpm": rpm, **b.cmd("capture"), **b.cmd("read")}
                out.writerow({k: row.get(k, "na") for k in FIELDS})
                print(f"{rpm:5d} rpm  ign {row['ign_fall_deg']:>6}..{row['ign_rise_deg']:>6} deg"
                      f"  dwell {row['ign_low_us']:>6} us  inj {row['inj_low_us']:>6} us"
                      f"  ecu {row['ecu_a']} A")
        finally:
            b.cmd("rpm 0")
            b.cmd("ecu off")
    print(f"wrote {args.out}")


if __name__ == "__main__":
    main()
