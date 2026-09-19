#!/usr/bin/env python3
# Copyright (c) 2026 Fulda University of Applied Sciences.
# SPDX-License-Identifier: BSD-3-Clause
"""Seed min-max ranges of the MSA mean turnaround time [h] for the
turnaround time decomposition figure (the whiskers on the stacked bars).

Reads run_metrics.csv and writes one CSV per cluster,
<figure-data>/<cluster>_turnaround_range_h.csv, with column pairs per policy
in the bar order of the figure: F0, F10, F30.
"""

from __future__ import annotations

import argparse
import csv
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "lib"))
import main_matrix as mm  # noqa: E402

POLS = ("F0", "F10", "F30")  # -> f0, f10, f30 column pairs


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--data-dir", type=Path, default=mm.DEFAULT_DATA_DIR, help="Directory containing run_metrics.csv")
    parser.add_argument("--run-metrics", type=Path, default=None, help="Explicit run_metrics.csv (overrides --data-dir)")
    parser.add_argument("--figure-data-dir", type=Path, default=mm.DEFAULT_FIGURE_DATA_DIR, help="Output directory")
    parser.add_argument("--partial", action="store_true", help="Accept cells with fewer than three seeds; empty cells become NaN")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    metrics_path = args.run_metrics or (args.data_dir / "run_metrics.csv")
    if not metrics_path.exists():
        raise SystemExit(f"Missing run metrics: {metrics_path}")
    args.figure_data_dir.mkdir(parents=True, exist_ok=True)

    cells: dict[tuple[str, int, str], dict[str, float]] = defaultdict(dict)
    with metrics_path.open(newline="") as f:
        for r in csv.DictReader(f):
            if r["scheduler"] != mm.selected_malleable_scheduler(r["cluster"]):
                continue
            cells[(r["cluster"], int(r["mal"]), r["policy"])][r["seed"]] = float(r["turnaround_mean_s"]) / 3600.0

    for cl in mm.CLUSTERS:
        path = args.figure_data_dir / f"{cl}_turnaround_range_h.csv"
        with path.open("w", newline="") as f:
            w = csv.writer(f, lineterminator="\n")
            w.writerow(["mal_idx", "f0_min", "f0_max", "f10_min", "f10_max", "f30_min", "f30_max"])
            for i, m in enumerate(mm.MAL_LEVELS):
                row: list[object] = [i]
                for pol in POLS:
                    s = cells[(cl, m, pol)]
                    if set(s) != set(mm.SEEDS) and not args.partial:
                        raise SystemExit(f"{cl} m{m} {pol}: expected seeds {mm.SEEDS}, found {sorted(s)} (use --partial)")
                    vals = list(s.values())
                    row += [f"{min(vals):.6f}", f"{max(vals):.6f}"] if vals else ["NaN", "NaN"]
                w.writerow(row)
        print("wrote", path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
