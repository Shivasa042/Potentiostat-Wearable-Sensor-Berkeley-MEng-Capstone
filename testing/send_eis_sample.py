#!/usr/bin/env python3
"""
Send a single EIS sample command to the HELPStat board over USB-serial.

This script sends:
  MEASURE:SAMPLE

Then it captures the firmware's CSV output from:
  "=== EIS DATA CSV ==="
until:
  "=== END CSV ==="

Example:
  py -3 testing/send_eis_sample.py --port COM3
  py -3 testing/send_eis_sample.py --port COM3 --out eis_sample.csv
"""

from __future__ import annotations

import argparse
import os
import sys
import time
from typing import List, Tuple, Optional

import serial


EIS_DATA_START = "=== EIS DATA CSV ==="
END_CSV = "=== END CSV ==="


def _try_parse_numeric_row(line: str) -> Optional[Tuple[float, float, float, float, float]]:
    """
    Attempt to parse a firmware CSV data row.

    The firmware prints 5 columns per row:
      freq, real, imag, magnitude, phaseDeg
    """
    parts = [p.strip() for p in line.split(",")]
    if len(parts) < 5:
        return None
    try:
        return (
            float(parts[0]),
            float(parts[1]),
            float(parts[2]),
            float(parts[3]),
            float(parts[4]),
        )
    except ValueError:
        return None


def main() -> None:
    parser = argparse.ArgumentParser(description="Send one EIS sample to HELPStat over serial.")
    parser.add_argument("--port", default="COM4", help="Serial port (e.g. COM3 or /dev/ttyUSB0). Default: COM4")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate (default: 115200)")
    parser.add_argument("--timeout", type=int, default=120, help="Max wait time in seconds (default: 120)")
    parser.add_argument(
        "--out",
        default="",
        help="Optional output filename. Use .csv to save the raw firmware block, or .xlsx to save parsed sweep data.",
    )
    parser.add_argument(
        "--start-hz",
        type=float,
        default=10.0,
        help="EIS start frequency in Hz (default: 10.0).",
    )
    parser.add_argument(
        "--end-hz",
        type=float,
        default=100000.0,
        help="EIS end frequency in Hz (default: 100,000).",
    )
    parser.add_argument(
        "--amplitude-mv",
        type=float,
        default=200.0,
        help="Signal amplitude in mV (default: 200.0). Valid range is firmware-clamped to 0-800.",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print all incoming serial lines while capturing (debugging).",
    )
    args = parser.parse_args()

    if args.out:
        out_dir = os.path.dirname(os.path.abspath(args.out))
        if out_dir and not os.path.exists(out_dir):
            raise SystemExit(f"Output directory does not exist: {out_dir}")

    rows: List[str] = []
    numeric_points: List[Tuple[float, float, float, float, float]] = []
    calculated_lines: List[str] = []

    ser = serial.Serial()
    ser.port = args.port
    ser.baudrate = args.baud
    ser.timeout = 1
    ser.write_timeout = 2
    ser.dsrdtr = False
    ser.rtscts = False
    ser.open()

    with ser:

        # Use full parameter list to "lock in" the required sweep band (10 Hz - 100 kHz)
        # while keeping other values at known sample-friendly defaults.
        #
        # Firmware parameter order (14 values, recommended format):
        # MEASURE:mode,startFreq,endFreq,numPoints,biasVolt,zeroVolt,rcalVal,extGain,dacGain,rct_est,rs_est,numCycles,delaySecs,amplitude
        command = (
            "MEASURE:0,"
            f"{args.start_hz},"
            f"{args.end_hz},"
            "10,"      # numPoints per decade
            "0.0,"     # biasVolt
            "0.0,"     # zeroVolt
            "100.0,"  # rcalVal (on-board RCAL, nominal 100 Ω)
            "1,"       # extGain
            "1,"       # dacGain
            "127000.0,"# rct_est
            "150.0,"   # rs_est
            "0,"       # numCycles
            "0,"       # delaySecs
            f"{args.amplitude_mv}"  # amplitude mV
        )
        ser.write((command + "\n").encode("utf-8"))

        start = time.time()
        capturing = False
        while True:
            if time.time() - start > args.timeout:
                print("Timeout waiting for EIS CSV output.", file=sys.stderr)
                break
            try:
                raw_bytes = ser.readline()
            except (serial.SerialException, OSError):
                time.sleep(0.1)
                continue
            if not raw_bytes:
                continue

            raw = raw_bytes.decode("utf-8", errors="ignore").strip()
            if not raw:
                continue
            if args.verbose:
                print(raw)

            if not capturing:
                if EIS_DATA_START in raw:
                    capturing = True
                    rows.append(raw)
                continue

            # Capturing the full printed block until the end marker.
            rows.append(raw)
            if raw == END_CSV:
                break

            # Best-effort parsing of rows (skip non-numeric lines).
            parsed = _try_parse_numeric_row(raw)
            if parsed is not None:
                numeric_points.append(parsed)

            # Capture Rct/Rs calculated lines (printed after '=== CALCULATED PARAMETERS ===')
            if "Rct(Ohm),Rs(Ohm)" in raw or ("," in raw and "=== " not in raw and "EIS DATA" not in raw):
                # One of:
                #  - 'Rct(Ohm),Rs(Ohm)'
                #  - '<rct>,<rs>'
                if any(ch.isdigit() for ch in raw):
                    calculated_lines.append(raw)

    if not rows:
        print("No EIS output captured. Is the board connected and running the firmware?", file=sys.stderr)
        sys.exit(2)

    if args.out:
        out_lower = args.out.lower()
        if out_lower.endswith(".xlsx"):
            # Write parsed impedance sweep to Excel.
            try:
                from openpyxl import Workbook
            except Exception as e:
                raise SystemExit(f"Excel export requested but openpyxl is not available: {e}") from e

            wb = Workbook()
            ws = wb.active
            ws.title = "Data"
            ws.append(["Frequency(Hz)", "Real(Ohm)", "Imaginary(Ohm)", "Magnitude(Ohm)", "Phase(Degrees)"])
            for (f_hz, real_ohm, imag_ohm, mag_ohm, phase_deg) in numeric_points:
                ws.append([f_hz, real_ohm, imag_ohm, mag_ohm, phase_deg])

            # Optional summary sheet with calculated parameters.
            ws2 = wb.create_sheet("Summary")
            ws2.append(["Calculated parameters (raw lines)"])
            for line in calculated_lines:
                ws2.append([line])

            wb.save(args.out)
            print(f"Wrote parsed EIS sweep to Excel: {args.out}")
        else:
            # Write the captured block verbatim (so headers/calculated parameters are preserved).
            with open(args.out, "w", newline="", encoding="utf-8") as f:
                for line in rows:
                    f.write(line + "\n")
            print(f"Wrote captured EIS output to: {args.out}")

    # Print a compact summary.
    if numeric_points:
        print(f"Captured {len(numeric_points)} impedance points (freq, real, imag, magnitude, phase).")
        first = numeric_points[0]
        last = numeric_points[-1]
        print(
            f"First point: f={first[0]:.6g} Hz, real={first[1]:.6g} Ohm, imag={first[2]:.6g} Ohm"
        )
        print(
            f"Last point:  f={last[0]:.6g} Hz, real={last[1]:.6g} Ohm, imag={last[2]:.6g} Ohm"
        )
    else:
        print("Captured EIS output, but no numeric impedance rows were parsed.")

    # If the firmware printed calculated parameters, show the last candidate.
    if calculated_lines:
        print("Calculated parameters line(s):")
        for line in calculated_lines[-3:]:
            print(line)


if __name__ == "__main__":
    main()

