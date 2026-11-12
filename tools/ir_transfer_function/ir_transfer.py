#!/usr/bin/env python3
"""Measure and plot the Sharp IR rangefinder's transfer function.

Two subcommands:

  capture  Reads the ESP32 logger (esp32_ir_logger/). You place a target
           at a measured distance, type that distance, and the script averages N
           samples there. Results are appended to a CSV as you go.

  plot     Reads the CSV, plots distance (x) vs sensor output voltage in mV (y) with
           the sample spread as error bars, fits V = k / (d + d0) + c, and reports the
           distance error of the inverse fit (d = k / (V - c) - d0) at each point.

Distances can be in cm (default) or inches (--units in on capture). The unit is
stored in the CSV header and the plot uses it automatically.

Examples:
  python3 ir_transfer.py capture --port /dev/cu.usbserial-0001 --out data/ir_run1.csv
  python3 ir_transfer.py capture --port /dev/cu.usbserial-0001 --units in --out data/ir_run1.csv
  python3 ir_transfer.py plot data/ir_run1.csv --out ir_transfer_function.png
"""

import argparse
import csv
import os
import sys
import time

import numpy as np

FIELDS = ["mean_mv", "std_mv", "min_mv", "max_mv", "mean_counts", "std_counts", "n"]


# ---------------------------------------------------------------- capture

def read_samples(ser, n, timeout_s=5.0):
    """Collect n readings from lines of the form 'IR,<raw counts>,<millivolts>'.

    Returns an (n, 2) array of [counts, mV]."""
    ser.reset_input_buffer()
    ser.readline()  # drop a possibly partial first line
    values = []
    deadline = time.time() + timeout_s
    while len(values) < n:
        if time.time() > deadline:
            raise TimeoutError(f"only got {len(values)}/{n} samples; is esp32_ir_logger flashed "
                               "and the serial monitor closed?")
        parts = ser.readline().decode(errors="ignore").strip().split(",")
        if len(parts) == 3 and parts[0] == "IR":
            try:
                values.append((int(parts[1]), int(parts[2])))
            except ValueError:
                pass
    return np.array(values)


def capture(args):
    import serial  # pyserial

    new_file = not os.path.exists(args.out) or os.path.getsize(args.out) == 0
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    ser = serial.Serial()
    ser.port, ser.baudrate, ser.timeout = args.port, args.baud, 1
    # Keep DTR/RTS released so the ESP32 dev board's auto-reset circuit doesn't hold it in
    # reset or bootloader mode while the port is open.
    ser.dtr = False
    ser.rts = False
    ser.open()
    with ser, open(args.out, "a", newline="") as f:
        time.sleep(1.0)
        u = args.units
        dist_field = f"distance_{u}"
        if not new_file:
            with open(args.out, newline="") as existing:
                header = next(csv.reader(existing), [])
            if header and header[0] != dist_field:
                sys.exit(f"{args.out} already uses {header[0]}; use a new --out file for {u}")
        writer = csv.DictWriter(f, fieldnames=[dist_field] + FIELDS)
        if new_file:
            writer.writeheader()
        print(f"Logging to {args.out}. Place the target, type its distance in {u}, press Enter.")
        print("Measure from the sensor's front face. Empty line or 'q' to finish.\n")
        while True:
            entry = input(f"distance {u}> ").strip()
            if entry in ("", "q", "quit"):
                break
            try:
                d = float(entry)
            except ValueError:
                print("  not a number")
                continue
            s = read_samples(ser, args.samples)
            counts, mv = s[:, 0], s[:, 1]
            row = {
                dist_field: d,
                "mean_mv": round(float(mv.mean()), 1),
                "std_mv": round(float(mv.std(ddof=1)), 1),
                "min_mv": int(mv.min()),
                "max_mv": int(mv.max()),
                "mean_counts": round(float(counts.mean()), 1),
                "std_counts": round(float(counts.std(ddof=1)), 1),
                "n": len(s),
            }
            writer.writerow(row)
            f.flush()
            print(f"  {d:6.1f} {u} -> {row['mean_mv']:7.1f} mV (std {row['std_mv']:.1f}), "
                  f"{row['mean_counts']:.0f} counts, n={row['n']}")


# ---------------------------------------------------------------- fit + plot

def load(path):
    with open(path, newline="") as f:
        rows = list(csv.DictReader(f))
    if not rows:
        sys.exit(f"{path} has no data rows")
    unit = "in" if "distance_in" in rows[0] else "cm"
    d = np.array([float(r[f"distance_{unit}"]) for r in rows])
    c = np.array([float(r["mean_mv"]) for r in rows])
    s = np.array([float(r["std_mv"]) for r in rows])
    order = np.argsort(d)
    return d[order], c[order], s[order], unit


def fit_inverse(d, y):
    """Fit y = k / (d + d0) + c. Linear least squares in (k, c) over a grid of d0."""
    best = None
    for d0 in np.arange(-min(d) + 0.5, 30.0, 0.05):
        x = 1.0 / (d + d0)
        A = np.column_stack([x, np.ones_like(x)])
        (k, c), *_ = np.linalg.lstsq(A, y, rcond=None)
        sse = float(np.sum((A @ [k, c] - y) ** 2))
        if best is None or sse < best[0]:
            best = (sse, k, d0, c)
    _, k, d0, c = best
    return k, d0, c


def plot(args):
    import matplotlib
    if not args.show:
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    d, mv, std, u = load(args.csv)

    # Below a few cm (~3 in) the Sharp output peaks and falls again (non-monotonic), so the
    # model is fitted only from the peak outward. That peak is the sensor's minimum range.
    peak = int(np.argmax(mv))
    usable = np.arange(len(d)) >= peak
    if args.min_dist is not None:
        usable &= d >= args.min_dist
    if args.max_dist is not None:
        usable &= d <= args.max_dist
    if usable.sum() < 4:
        sys.exit("need at least 4 points in the usable range to fit")

    k, d0, c = fit_inverse(d[usable], mv[usable])
    predicted = k / (mv[usable] - c) - d0
    err = predicted - d[usable]

    unit_name = "inches" if u == "in" else "cm"
    print(f"Fit: mV = {k:.1f} / (d {d0:+.2f}) {c:+.2f}    (d in {unit_name})")
    print(f"Inverse: d_{u} = {k:.1f} / (mV {-c:+.2f}) {-d0:+.2f}")
    print(f"Usable range used for fit: {d[usable].min():.1f}-{d[usable].max():.1f} {u}, "
          f"{usable.sum()} points")
    print(f"Distance error: mean |err| {np.mean(np.abs(err)):.2f} {u}, "
          f"RMS {np.sqrt(np.mean(err ** 2)):.2f} {u}, max |err| {np.max(np.abs(err)):.2f} {u}")
    print(f"\n  true {u}       mV   fit {u}   err {u}")
    for di, ci, pi, ei in zip(d[usable], mv[usable], predicted, err):
        print(f"  {di:7.1f}  {ci:7.1f}  {pi:7.1f}  {ei:+7.2f}")

    ink, ink2, grid = "#0b0b0b", "#52514e", "#e4e3df"
    meas_color, fit_color = "#2a78d6", "#eb6834"
    plt.rcParams.update({"font.size": 10, "axes.edgecolor": ink2, "axes.labelcolor": ink,
                         "xtick.color": ink2, "ytick.color": ink2})

    fig, (ax, axe) = plt.subplots(2, 1, figsize=(8, 7.5), sharex=True,
                                  gridspec_kw={"height_ratios": [3, 1.2], "hspace": 0.08})

    xs = np.linspace(d[usable].min(), d.max(), 400)
    ax.plot(xs, k / (xs + d0) + c, color=fit_color, lw=2,
            label=f"fit: mV = {k:.0f} / (d {d0:+.2f}) {c:+.0f}")
    ax.errorbar(d, mv, yerr=std, fmt="o", ms=6, color=meas_color, ecolor=meas_color,
                elinewidth=1, capsize=3, mec="white", mew=1.5,
                label=f"measured (mean ± 1 std of {args.samples_label} samples)")
    if peak > 0:
        pad = 0.0125 * d.max()
        ax.axvspan(d.min() - pad, d[peak], color=grid, alpha=0.6, lw=0)
        ax.text(d[peak] + pad, mv.max() * 0.04, "◂ shaded: below minimum range\n   (output not monotonic, not fitted)",
                color=ink2, fontsize=8, ha="left", va="bottom")
    ax.set_ylabel("Raw sensor output (mV at ESP32 ADC pin)")
    ax.set_title(args.title, color=ink, loc="left", fontsize=12)
    ax.set_ylim(bottom=0)
    ax.grid(True, color=grid, lw=0.8)
    ax.legend(frameon=False, loc="upper right")
    for spine in ("top", "right"):
        ax.spines[spine].set_visible(False)

    axe.axhline(0, color=ink2, lw=1)
    axe.plot(d[usable], err, "o", ms=6, color=meas_color, mec="white", mew=1.5)
    axe.set_ylabel(f"Distance error ({u})")
    axe.set_xlabel(f"Distance to target ({u}, from sensor face)")
    axe.grid(True, color=grid, lw=0.8)
    axe.text(0.01, 0.92, f"RMS {np.sqrt(np.mean(err ** 2)):.2f} {u}, "
             f"max {np.max(np.abs(err)):.2f} {u}", transform=axe.transAxes,
             color=ink2, fontsize=9, va="top")
    for spine in ("top", "right"):
        axe.spines[spine].set_visible(False)

    fig.savefig(args.out, dpi=200, bbox_inches="tight")
    print(f"\nSaved {args.out}")
    if args.show:
        plt.show()


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    c = sub.add_parser("capture", help="record distance vs sensor output from the ESP32")
    c.add_argument("--port", required=True, help="e.g. /dev/cu.usbserial-0001 (Mac) or /dev/ttyUSB0 (Linux)")
    c.add_argument("--baud", type=int, default=115200)
    c.add_argument("--units", choices=["cm", "in"], default="cm", help="unit you type distances in")
    c.add_argument("--samples", type=int, default=200, help="readings averaged per distance")
    c.add_argument("--out", default="data/ir_transfer.csv")
    c.set_defaults(func=capture)

    q = sub.add_parser("plot", help="plot the transfer function from a CSV")
    q.add_argument("csv")
    q.add_argument("--out", default="ir_transfer_function.png")
    q.add_argument("--title", default="Sharp IR rangefinder transfer function (measured)")
    q.add_argument("--min-dist", "--min-cm", type=float, help="ignore points closer than this when fitting (CSV's units)")
    q.add_argument("--max-dist", "--max-cm", type=float, help="ignore points farther than this when fitting (CSV's units)")
    q.add_argument("--samples-label", default="200", help="sample count shown in the legend")
    q.add_argument("--show", action="store_true", help="also open a window")
    q.set_defaults(func=plot)

    args = p.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
