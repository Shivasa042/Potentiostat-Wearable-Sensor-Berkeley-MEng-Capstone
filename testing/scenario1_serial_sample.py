#!/usr/bin/env python3
"""
Scenario 1 (laptop + USB serial): send MEASURE over 115200 baud and capture
=== EIS DATA CSV === ... === END CSV === from firmware.

Uses a narrower band and fewer points per decade for a faster sample sweep.
Override port:  py -3 testing/scenario1_serial_sample.py COM4
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


def main() -> int:
    port = sys.argv[1] if len(sys.argv) > 1 else find_esp_port()
    if not port:
        print("No ESP32-S3 (VID 303A) found. Plug in the board or pass COM port.", file=sys.stderr)
        return 1

    # Quick sample: 10 kHz down to 100 Hz, 2 points/decade, 200 mV (matches firmware style)
    cmd = (
        "MEASURE:0,10000,100,2,0.0,0.0,1000.0,1,1,127000.0,150.0,0,0,200\n"
    )

    print(f"Opening {port} @ 115200 …")
    ser = serial.Serial(port=port, baudrate=115200, timeout=2, write_timeout=2)
    time.sleep(0.4)
    ser.reset_input_buffer()
    print("Sending:", cmd.strip())
    ser.write(cmd.encode("utf-8"))

    out_name = "scenario1_eis_sample.csv"
    capturing = False
    lines: list[str] = []
    deadline = time.time() + 420.0

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
