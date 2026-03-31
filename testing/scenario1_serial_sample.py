#!/usr/bin/env python3
"""
Scenario 1 (laptop + USB serial): send MEASURE over 115200 baud and capture
=== EIS DATA CSV === ... === END CSV === from firmware.

Default is a very short multi-point sweep (2 frequencies, 10 kHz–1 kHz only, for speed).
Pass a frequency (Hz) as the last argument to request a single-frequency sample.
The firmware maps (start,end,numPoints) to
SweepPoints via a log formula; start==end yields 0 points, so single-frequency
mode uses start and end=start*1.035 with numPoints=100 so SweepPoints==1 (first
measurement is at start).

Examples:
  py -3 testing/scenario1_serial_sample.py COM4
  py -3 testing/scenario1_serial_sample.py COM4 10000
"""
from __future__ import annotations

import sys
import time

import serial
import serial.tools.list_ports


def find_esp_port() -> str | None:
    for p in serial.tools.list_ports.comports():
        if "303A" in p.hwid:
            return p.device
    return None


def measure_cmd_single_frequency(f_hz: float) -> str:
    start = float(f_hz)
    end = start * 1.035
    # numPoints=100 with ~3.5% span gives log10 ratio * 100 ≈ 1.5 → SweepPoints == 1
    return (
        f"MEASURE:0,{start:g},{end:g},100,0.0,0.0,100.0,1,1,127000.0,150.0,0,0,200\n"
    )


def main() -> int:
    argv = list(sys.argv[1:])
    single_hz: float | None = None
    if argv:
        try:
            single_hz = float(argv[-1])
            argv.pop()
        except ValueError:
            pass
    port = argv[0] if argv else find_esp_port()
    if not port:
        print("No ESP32-S3 (VID 303A) found. Plug in the board or pass COM port.", file=sys.stderr)
        return 1

    if single_hz is not None:
        cmd = measure_cmd_single_frequency(single_hz)
        out_name = "scenario1_eis_sample_single.csv"
    else:
        # Minimal sweep: one decade (10 kHz → 1 kHz), 2 points/decade → 2 DFT points only; HF band keeps each step short.
        cmd = (
            "MEASURE:0,10000,1000,2,0.0,0.0,100.0,1,1,127000.0,150.0,0,0,150\n"
        )
        out_name = "scenario1_eis_sample.csv"

    print(f"Opening {port} @ 115200 …")
    ser = serial.Serial(port=port, baudrate=115200, timeout=2, write_timeout=2)
    time.sleep(0.3)
    ser.reset_input_buffer()
    # Queue ASAP so the first parseSerialCommand() after a blocking sweep sees it before another auto-sweep arms.
    ser.write(b"WEARABLE:OFF\n")
    ser.flush()

    # While runSweep() is active, serial is not parsed — wait until idle before MEASURE.
    idle_deadline = time.time() + 420.0
    print("Waiting for firmware idle (Ready / WEARABLE:OFF ack) …")
    drain_n = 0
    while time.time() < idle_deadline:
        raw = ser.readline()
        if not raw:
            continue
        s = raw.decode("utf-8", errors="replace").rstrip()
        if s:
            drain_n += 1
            # Do not print every sweep row while draining a long in-progress sweep.
            if drain_n <= 3 or "Ready for next measurement" in s or "DISABLED" in s:
                print(s)
            elif drain_n % 200 == 0:
                print(f"… still draining serial ({drain_n} lines) …")
        if "Ready for next measurement" in s or "Wearable auto-sweep DISABLED" in s:
            break
    else:
        print("Timed out waiting for idle; sending MEASURE anyway.", file=sys.stderr)

    time.sleep(0.15)
    ser.reset_input_buffer()
    print("Sending:", cmd.strip())
    ser.write(cmd.encode("utf-8"))

    capturing = False
    lines: list[str] = []
    # Single-point and tiny default sweep finish quickly; allow headroom if a wearable sweep was in progress.
    wait_s = 180.0 if single_hz is not None else 240.0
    deadline = time.time() + wait_s

    while time.time() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        s = raw.decode("utf-8", errors="replace").rstrip()
        print(s)
        if "=== EIS DATA CSV ===" in s:
            capturing = True
        if capturing:
            lines.append(s)
        if "=== END CSV ===" in s:
            break

    ser.close()

    if not lines or "=== END CSV ===" not in "\n".join(lines):
        print("\nNo complete CSV block (timeout or sweep did not finish).", file=sys.stderr)
        return 2

    with open(out_name, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print(f"\nWrote {out_name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
