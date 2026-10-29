#!/usr/bin/env python3
"""Does the ECU follow a swept lambda signal? Drives the lambda channel with the
Pico's wave generator at a list of frequencies, records every injector pulse the
Pico sees (the pulse log, so no aliasing from polling), and fits the response at
each drive frequency: amplitude, phase and a z-score against the noise.

    python3 host/examples/lambda_sweep.py                       # 0.2 .. 5 Hz, warm idle
    python3 host/examples/lambda_sweep.py --freqs 0.5,1,2 --seconds 90
    python3 host/examples/lambda_sweep.py --selftest            # no hardware: checks the fit

A wave of lambda 1.00 +- 0.03 through the sensor curve swings the DAC between lean
and rich like a working sensor. The injector fires once per revolution, so the
log has about rpm/60 points per second (14 at 850 rpm): drive frequencies above
about a third of that are unreliable. A control loop would show up as a clear
amplitude (z well above 5) that depends on frequency; no loop gives z near 0.
"""
import argparse
import csv
import math
import os
import random
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from bench import Bench  # noqa: E402

WARM_IDLE = ["dac water 1200", "dac air 1800", "dac afm 1000", "idle on"]


def fit_sine(ts, ys, f_hz):
    """Least squares y = c + a sin(2 pi f t) + b cos(2 pi f t) on irregular samples.
    Returns (mean, amplitude, phase_deg, z) with z = amplitude / its standard error."""
    n = len(ts)
    if n < 8:
        return None
    w = 2 * math.pi * f_hz
    cols = [[1.0] * n, [math.sin(w * t) for t in ts], [math.cos(w * t) for t in ts]]
    # normal equations A x = r
    A = [[sum(ci[k] * cj[k] for k in range(n)) for cj in cols] for ci in cols]
    r = [sum(ci[k] * ys[k] for k in range(n)) for ci in cols]
    # Gauss elimination, 3x3
    for i in range(3):
        piv = max(range(i, 3), key=lambda j: abs(A[j][i]))
        A[i], A[piv] = A[piv], A[i]
        r[i], r[piv] = r[piv], r[i]
        if abs(A[i][i]) < 1e-12:
            return None
        for j in range(i + 1, 3):
            m = A[j][i] / A[i][i]
            for k in range(i, 3):
                A[j][k] -= m * A[i][k]
            r[j] -= m * r[i]
    x = [0.0] * 3
    for i in (2, 1, 0):
        x[i] = (r[i] - sum(A[i][k] * x[k] for k in range(i + 1, 3))) / A[i][i]
    c, a, b = x
    resid = [ys[k] - (c + a * cols[1][k] + b * cols[2][k]) for k in range(n)]
    sigma = math.sqrt(sum(e * e for e in resid) / (n - 3))
    amp = math.hypot(a, b)
    se = sigma * math.sqrt(2.0 / n)
    return c, amp, math.degrees(math.atan2(b, a)), (amp / se if se > 0 else float("inf"))


def unwrap(stamps):
    """The pulse log carries the low 32 bits of the microsecond clock."""
    out, base, prev = [], 0, None
    for s in stamps:
        if prev is not None and s < prev - (1 << 31):
            base += 1 << 32
        out.append(s + base)
        prev = s
    return out


def drain(b, name="inj"):
    """Reads and clears everything pending in the log: (stamps, widths)."""
    stamps, widths = [], []
    while True:
        head, body = b.block(f"log read {name} 200")
        for line in body:
            t, w = line.split()
            stamps.append(int(t))
            widths.append(int(w))
        if int(head["pending"]) == 0:
            return stamps, widths, int(head["dropped"])


def record(b, seconds):
    """Logs for `seconds`, emptying the ring every 5 s.
    Returns (unwrapped timestamps in us, widths in us, dropped)."""
    b.cmd("log start")
    stamps, widths, dropped = [], [], 0
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        time.sleep(min(5.0, max(0.1, end - time.monotonic())))
        s, w, dropped = drain(b)
        stamps += s
        widths += w
    s, w, dropped = drain(b)
    stamps += s
    widths += w
    b.cmd("log stop")
    return unwrap(stamps), widths, dropped


def to_seconds(stamps, origin_us):
    """Seconds since origin_us. The origin is a raw 32-bit clock value, so put it on
    the same 32-bit turn as the (unwrapped) stamps before subtracting."""
    if not stamps:
        return []
    turn = round((stamps[0] - origin_us) / (1 << 32))
    base = origin_us + turn * (1 << 32)
    return [(s - base) / 1e6 for s in stamps]


def selftest():
    random.seed(1)
    f, amp, phase = 1.0, 40.0, 0.5
    ts, ys = [], []
    t = 0.0
    while t < 90:
        ts.append(t)
        ys.append(2600 + amp * math.sin(2 * math.pi * f * t + phase) + random.gauss(0, 3))
        t += 1 / 14.17 + random.gauss(0, 0.003)
    c, a, ph, z = fit_sine(ts, ys, f)
    assert abs(c - 2600) < 2 and abs(a - amp) < 2 and abs(ph - math.degrees(phase)) < 4 and z > 20, (c, a, ph, z)
    ys = [2600 + random.gauss(0, 3) for _ in ts]
    c, a, ph, z = fit_sine(ts, ys, f)
    assert z < 4, ("no signal should give a small z", z)
    # 32-bit clock wrap
    assert unwrap([4294967000, 100, 200]) == [4294967000, 4294967396, 4294967496]
    # a stamp stream that crosses the wrap, against a t0 on the other side of it
    ts2 = to_seconds(unwrap([4294967000, 100, 200]), 4294966000)
    assert abs(ts2[0] - 0.001) < 1e-9 and abs(ts2[2] - 0.001496) < 1e-9, ts2
    print("selftest ok")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port", help="serial port (default: auto-detect)")
    ap.add_argument("--freqs", default="0.2,0.5,1,2,3,5", help="drive frequencies, Hz")
    ap.add_argument("--amp", default="0.03", help="lambda amplitude")
    ap.add_argument("--offset", default="1.000", help="lambda centre")
    ap.add_argument("--seconds", type=float, default=60, help="recording time per frequency")
    ap.add_argument("--skip", type=float, default=10, help="seconds to ignore after each frequency change")
    ap.add_argument("--baseline", type=float, default=60, help="recording time with the wave off")
    ap.add_argument("--settle", type=float, default=60, help="seconds at the warm-idle baseline before starting")
    ap.add_argument("--rpm", type=int, default=850)
    ap.add_argument("--no-setup", action="store_true", help="leave ecu / rpm / sensors as they are")
    ap.add_argument("--out", default="lambda_sweep.csv")
    ap.add_argument("--selftest", action="store_true", help="check the fit on synthetic data and exit")
    args = ap.parse_args()
    if args.selftest:
        selftest()
        return
    freqs = [float(x) for x in args.freqs.split(",")]

    with Bench(args.port) as b:
        try:
            if not args.no_setup:
                b.cmd("ecu on")
                for c in WARM_IDLE:
                    b.cmd(c)
                b.cmd("dac lambda 450")
                b.cmd(f"rpm {args.rpm}")
                print(f"settling {args.settle:.0f} s at warm idle ...")
                time.sleep(args.settle)
            print(f"baseline, wave off, {args.baseline:.0f} s ...")
            b.cmd("wave off")
            bs, bw, _ = record(b, args.baseline)
            bt = to_seconds(bs, bs[0] if bs else 0)
            rows = []
            for f in freqs:
                reply = b.cmd(f"wave lambda lam {f} {args.amp} {args.offset}")
                t0 = int(reply["t0_us"])
                print(f"{f:5.2f} Hz ... ", end="", flush=True)
                time.sleep(args.skip)
                stamps, ws, dropped = record(b, args.seconds)
                ts = to_seconds(stamps, t0)  # the wave's phase 0 is its t0
                res = fit_sine(ts, ws, f)
                base = fit_sine(bt, bw, f)
                if res is None:
                    print("not enough pulses")
                    continue
                mean, amp_us, phase, z = res
                row = {"freq_hz": f, "pulses": len(ws), "mean_inj_us": round(mean, 1),
                       "amp_us": round(amp_us, 2), "amp_pct": round(100 * amp_us / mean, 3),
                       "phase_deg": round(phase, 1), "z": round(z, 1),
                       "baseline_amp_us": round(base[1], 2) if base else "na",
                       "dropped": dropped}
                rows.append(row)
                print(f"amp {amp_us:7.2f} us ({row['amp_pct']:.3f} %)  phase {phase:6.1f} deg  z {z:6.1f}"
                      f"  (baseline amp {row['baseline_amp_us']} us)")
        finally:
            b.cmd("wave off")
            b.cmd("dac lambda 450")
        with open(args.out, "w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=list(rows[0]) if rows else ["freq_hz"])
            w.writeheader()
            w.writerows(rows)
        print(f"wrote {args.out}")


if __name__ == "__main__":
    main()
