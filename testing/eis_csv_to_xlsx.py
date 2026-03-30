#!/usr/bin/env python3
"""
Convert HELPStat SD-card EIS CSV output to an Excel (.xlsx) file.

The firmware saveDataEIS() writes lines like:
  Freq, Magnitude, Phase (rad), Phase (deg), Real, Imag,

It may also include repeated header lines and a trailing comma after Imag.
This script extracts numeric rows and exports them to an .xlsx.

Usage:
  py -3 testing/eis_csv_to_xlsx.py --in path/to/file.csv --out path/to/file.xlsx
"""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import List, Optional, Tuple

from openpyxl import Workbook


def _try_float(x: str) -> Optional[float]:
    x = x.strip()
    if x == "":
        return None
    try:
        return float(x)
    except ValueError:
        return None


def _parse_numeric_row(line: str) -> Optional[Tuple[float, ...]]:
    # Split on commas and collect all floats from the beginning of the row.
    parts = [p.strip() for p in line.split(",")]
    floats: List[float] = []
    for p in parts:
        v = _try_float(p)
        if v is None:
            # Once we hit a non-float token, the row is likely a header or end marker.
            # (We also tolerate trailing commas which produce empty string tokens.)
            if p == "":
                continue
            break
        floats.append(v)

        # saveDataEIS typically needs 6 numeric values; printDataCSV sometimes has 5.
        if len(floats) >= 6:
            break

    if len(floats) == 6:
        # SD file format:
        # freq, magnitude, phaseRad, phaseDeg, real, imag
        return tuple(floats)
    if len(floats) == 5:
        # Some other format (e.g. live serial CSV) could be:
        # freq, real, imag, magnitude, phaseDeg
        return tuple(floats)
    return None


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert HELPStat EIS CSV to XLSX")
    parser.add_argument("--in", dest="in_path", required=True, help="Input CSV path from SD card")
    parser.add_argument("--out", dest="out_path", required=True, help="Output XLSX path")
    parser.add_argument(
        "--sheet",
        default="Data",
        help="Worksheet name (default: Data)",
    )
    args = parser.parse_args()

    in_path = Path(args.in_path)
    out_path = Path(args.out_path)
    if not in_path.exists():
        raise SystemExit(f"Input CSV does not exist: {in_path}")

    rows_6: List[Tuple[float, float, float, float, float, float]] = []
    rows_5: List[Tuple[float, float, float, float, float]] = []

    with in_path.open("r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            parsed = _parse_numeric_row(line)
            if parsed is None:
                continue
            if len(parsed) == 6:
                rows_6.append(parsed)  # type: ignore[arg-type]
            elif len(parsed) == 5:
                rows_5.append(parsed)  # type: ignore[arg-type]

    wb = Workbook()
    ws = wb.active
    ws.title = args.sheet

    if rows_6:
        ws.append(["Frequency(Hz)", "Magnitude", "Phase(rad)", "Phase(deg)", "Real(Ohm)", "Imag(Ohm)"])
        for (freq, mag, phase_rad, phase_deg, real, imag) in rows_6:
            ws.append([freq, mag, phase_rad, phase_deg, real, imag])
    else:
        # Fallback: assume 5-value format (freq, real, imag, magnitude, phaseDeg)
        ws.append(["Frequency(Hz)", "Real(Ohm)", "Imag(Ohm)", "Magnitude(Ohm)", "Phase(deg)"])
        for (freq, real, imag, mag, phase_deg) in rows_5:
            ws.append([freq, real, imag, mag, phase_deg])

    out_path.parent.mkdir(parents=True, exist_ok=True)
    wb.save(out_path)

    if rows_6:
        print(f"Wrote XLSX: {out_path} ({len(rows_6)} rows)")
    else:
        print(f"Wrote XLSX: {out_path} ({len(rows_5)} rows)")


if __name__ == "__main__":
    main()

