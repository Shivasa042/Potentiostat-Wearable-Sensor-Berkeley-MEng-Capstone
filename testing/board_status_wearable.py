#!/usr/bin/env python3
"""Print STATUS from HELPStat, enable WEARABLE:ON, optionally confirm after sweep ends."""
from __future__ import annotations

import argparse
import sys
import time

import serial
import serial.tools.list_ports


def find_esp_port() -> str | None:
    for p in serial.tools.list_ports.comports():
        if "303A" in p.hwid:
            return p.device
    return None


def read_status_block(ser: serial.Serial) -> None:
    ser.write(b"STATUS\n")
    seen = False
    for _ in range(120):
        raw = ser.readline()
        if not raw:
            continue
        s = raw.decode("utf-8", errors="replace").rstrip()
        if s:
            print(s)
        if "=== STATUS ===" in s:
            seen = True
        if seen and s.strip() == "================":
            break


def wait_ready(ser: serial.Serial, max_s: float) -> bool:
    print(f"\nWaiting up to {max_s:.0f} s for idle (Ready for next measurement) …")
    deadline = time.time() + max_s
    last_note = 0.0
    while time.time() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        s = raw.decode("utf-8", errors="replace").rstrip()
        if s and "Ready for next measurement" in s:
            print(s)
            return True
        now = time.time()
        if now - last_note > 45:
            print("… sweep still running …")
            last_note = now
    return False


def main() -> int:
    ap = argparse.ArgumentParser(description="STATUS + WEARABLE:ON for HELPStat USB serial")
    ap.add_argument("port", nargs="?", default=None, help="COM port (default: auto ESP32-S3)")
    ap.add_argument(
        "--wait-ready",
        type=float,
        default=0,
        metavar="SEC",
        help="After WEARABLE:ON, wait for idle then print STATUS again (0 = skip)",
    )
    args = ap.parse_args()

    port = args.port or find_esp_port()
    if not port:
        print("No COM port (pass COM4 or connect ESP32-S3).", file=sys.stderr)
        return 1

    ser = serial.Serial(port=port, baudrate=115200, timeout=2.0, write_timeout=2)
    time.sleep(0.35)
    ser.reset_input_buffer()

    print(f"--- STATUS ({port}) ---")
    read_status_block(ser)

    print("\n--- WEARABLE:ON ---")
    ser.write(b"WEARABLE:ON\n")
    for _ in range(15):
        raw = ser.readline()
        if not raw:
            continue
        s = raw.decode("utf-8", errors="replace").rstrip()
        if s:
            print(s)
        if "ENABLED" in s:
            break

    if args.wait_ready > 0:
        ser.reset_input_buffer()
        if wait_ready(ser, args.wait_ready):
            print("\n--- STATUS (after idle) ---")
            read_status_block(ser)
        else:
            print("Timed out before idle; board may still be sweeping.", file=sys.stderr)

    ser.close()
    print(
        "\nWearable ON is in RAM until power-cycle/reflash (firmware default is OFF).",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
