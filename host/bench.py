#!/usr/bin/env python3
"""Host-side client for the Pruefstand firmware's USB serial command protocol.

    python3 bench.py                  interactive prompt
    python3 bench.py status           one command, exit 1 on ERR
    python3 bench.py "ecu on" "rpm 850" read

As a library:
    with Bench() as b:
        b.cmd("ecu on")
        print(b.cmd("read"))          # {'afm_ref_mv': '4987', 'ecu_v': 'na', ...}
"""
import argparse
import sys

import serial
import serial.tools.list_ports

PICO_USB_VID = 0x2E8A


class BenchError(RuntimeError):
    pass


def find_port():
    for p in serial.tools.list_ports.comports():
        if p.vid == PICO_USB_VID:
            return p.device
    raise BenchError("no Raspberry Pi Pico found, pass --port")


def parse_reply(line):
    """'OK a=1 b=x' -> {'a': '1', 'b': 'x'}; bare words go under 'text'."""
    if line.startswith("ERR"):
        raise BenchError(line[4:] if len(line) > 4 else line)
    if not line.startswith("OK"):
        raise BenchError(f"unexpected reply: {line!r}")
    fields, words = {}, []
    for tok in line[2:].split():
        key, sep, val = tok.partition("=")
        if sep:
            fields[key] = val
        else:
            words.append(tok)
    if words:
        fields["text"] = " ".join(words)
    return fields


class Bench:
    def __init__(self, port=None, timeout=2.0):
        self.ser = serial.Serial(port or find_port(), 115200, timeout=timeout)
        self.ser.reset_input_buffer()

    def raw(self, command):
        # Strictly one reply per command, so anything already waiting is a late
        # reply to an earlier command that timed out: drop it, or it would be
        # taken as the answer to this one.
        self.ser.reset_input_buffer()
        self.ser.write((command.strip() + "\n").encode())
        line = self.ser.readline().decode(errors="replace").strip()
        if not line:
            raise BenchError(f"no reply to {command!r} (timeout)")
        return line

    def cmd(self, command):
        return parse_reply(self.raw(command))

    def close(self):
        self.ser.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port", help="serial port (default: auto-detect Pico)")
    ap.add_argument("commands", nargs="*", help="commands to send; none = interactive")
    args = ap.parse_args()

    try:
        bench = Bench(args.port)
    except (BenchError, serial.SerialException) as e:
        sys.exit(f"error: {e}")

    with bench:
        if args.commands:
            failed = False
            for c in args.commands:
                try:
                    line = bench.raw(c)
                except BenchError as e:
                    print(f"error: {e}", file=sys.stderr)
                    sys.exit(1)
                print(line)
                failed |= line.startswith("ERR")
            sys.exit(1 if failed else 0)

        print(f"connected to {bench.ser.port}, 'help' for commands, Ctrl-D to quit")
        while True:
            try:
                c = input("bench> ")
            except (EOFError, KeyboardInterrupt):
                print()
                return
            if c.strip():
                try:
                    print(bench.raw(c))
                except BenchError as e:
                    print(f"error: {e}")


if __name__ == "__main__":
    main()
