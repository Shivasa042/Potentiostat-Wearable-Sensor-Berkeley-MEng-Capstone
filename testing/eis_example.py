#!/usr/bin/env python3
"""Run a default EIS sweep on the HELPStat board and plot results.

This script opens a serial connection to the board, sends the
"MEASURE:SAMPLE" command (same as pressing the pushbutton), and then
captures all of the CSV lines that the firmware prints.  When the sweep
is complete the data are parsed and displayed with Nyquist and Bode plots.

Usage example:
    python testing/eis_example.py --port COM3

Dependencies (also listed in requirements.txt):
    pip install pyserial numpy matplotlib
"""

import argparse
import serial
import time
import numpy as np
import matplotlib.pyplot as plt
import csv
import os

CSV_HEADER = None


def read_measurement(ser, timeout=30):
    """Read lines from serial until measurement completes.
    Returns list of CSV lines (strings) with numeric data.
    """
    lines = []
    start = time.time()
    while True:
        if ser.in_waiting:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            # ignore empty lines and informational text
            if line.startswith('Freq') or line.startswith('==='):
                # header or separators
                if line.startswith('Freq') and CSV_HEADER is None:
                    # static, not used here
                    pass
                if 'ALL_ADC_SAMPLES_COMPLETE' in line:
                    break
                continue
            # attempt to parse numeric row (comma-separated)
            parts = line.split(',')
            if len(parts) >= 3:
                try:
                    [float(p) for p in parts[:3]]
                    lines.append(line)
                except ValueError:
                    pass
        # timeout guard
        if time.time() - start > timeout:
            print('Timeout waiting for measurement data')
            break
    return lines


def parse_csv_rows(rows):
    freq = []
    real = []
    imag = []
    for row in rows:
        parts = row.split(',')
        freq.append(float(parts[0]))
        real.append(float(parts[1]))
        imag.append(float(parts[2]))
    return np.array(freq), np.array(real), np.array(imag)


def plot_results(freq, real, imag):
    # Nyquist plot
    fig, axs = plt.subplots(1, 2, figsize=(14, 6))
    axs[0].plot(real, -imag, 'o-')
    axs[0].set_xlabel("Real Z (Ω)")
    axs[0].set_ylabel("-Imag Z (Ω)")
    axs[0].set_title("Nyquist")
    axs[0].grid(True)
    axs[0].set_aspect('equal', 'box')

    # Bode magnitude + phase
    mag = np.sqrt(real**2 + imag**2)
    phase = np.degrees(np.arctan2(imag, real))
    ax2 = axs[1]
    ax2.semilogx(freq, mag, 'g-', label='|Z| (Ω)')
    ax2.set_xlabel('Frequency (Hz)')
    ax2.set_ylabel('Magnitude (Ω)', color='g')
    ax2.grid(True, which='both')
    ax3 = ax2.twinx()
    ax3.semilogx(freq, phase, 'r--', label='Phase (deg)')
    ax3.set_ylabel('Phase (°)', color='r')
    ax2.set_title('Bode Plot')

    fig.legend(loc='upper right')
    plt.tight_layout()
    plt.show()


def main():
    parser = argparse.ArgumentParser(description='Run EIS on HELPStat board')
    parser.add_argument('--port', required=True, help='Serial port (e.g. COM3 or /dev/ttyUSB0)')
    parser.add_argument('--baud', type=int, default=115200, help='Serial baud rate')
    parser.add_argument('--times', type=int, default=1,
                        help='Number of back-to-back sweeps to perform')
    parser.add_argument('--out', default='measurement.csv',
                        help='Filename to append CSV results to')
    args = parser.parse_args()

    with serial.Serial(args.port, args.baud, timeout=1) as ser:
        # flush startup messages
        time.sleep(2)
        ser.reset_input_buffer()
        all_rows = []
        for i in range(args.times):
            print(f'Starting sweep {i+1}/{args.times}')
            ser.write(b'MEASURE:SAMPLE\n')
            rows = read_measurement(ser, timeout=120)
            if not rows:
                print('No data returned for sweep', i+1)
                break
            all_rows.extend(rows)
            # small pause between sweeps
            time.sleep(1)
        rows = all_rows

    if rows and args.out:
        # append to csv file
        write_header = not os.path.exists(args.out)
        with open(args.out, 'a', newline='') as f:
            writer = csv.writer(f)
            if write_header:
                writer.writerow(['Frequency','Real','Imag'])
            for r in rows:
                writer.writerow(r.split(','))
        print('Saved measurements to', args.out)

    if not rows:
        print('No data captured')
        return

    freq, real, imag = parse_csv_rows(rows)
    plot_results(freq, real, imag)


if __name__ == '__main__':
    main()
