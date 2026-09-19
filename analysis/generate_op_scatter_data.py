#!/usr/bin/env python3
# Copyright (c) 2026 Fulda University of Applied Sciences.
# SPDX-License-Identifier: BSD-3-Clause
"""Data for the operating-point figure (turnaround time reduction and daily cost
saving of MSA relative to EBF F0 at m0, per cluster, malleability and policy).

Reads run_deltas.csv (written by aggregate.py) and writes one CSV per cluster,
<figure-data>/op_scatter_<cluster>.csv, with the seed mean and the min-max
range over the seeds:

  mal_idx, mal, policy, ta_mean, ta_min, ta_max, cost_mean, cost_min, cost_max

ta_* is the turnaround time reduction in percent (-turnaround_delta_pct),
cost_* the daily cost saving in percent (cost_savings_delta_pct).
"""

from __future__ import annotations

import argparse
import csv
import sys
from collections import defaultdict
from pathlib import Path
from statistics import mean

sys.path.insert(0, str(Path(__file__).resolve().parent / "lib"))
import main_matrix as mm  # noqa: E402

POLS = ("F0", "F10", "F30")  # row order per malleability level


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--run-deltas", type=Path, default=mm.DEFAULT_RESULTS_DIR / "run_deltas.csv", help="run_deltas.csv from aggregate.py")
    parser.add_argument("--figure-data-dir", type=Path, default=mm.DEFAULT_FIGURE_DATA_DIR, help="Output directory")
    parser.add_argument("--partial", action="store_true", help="Accept cells with fewer than three seeds; empty cells are omitted")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not args.run_deltas.exists():
        raise SystemExit(f"Missing run deltas: {args.run_deltas}")
    args.figure_data_dir.mkdir(parents=True, exist_ok=True)

    cells: dict[tuple[str, int, str], dict[str, tuple[float, float]]] = defaultdict(dict)
    with args.run_deltas.open(newline="") as f:
        for r in csv.DictReader(f):
            if r["scheduler"] != mm.selected_malleable_scheduler(r["cluster"]):
                continue
            cells[(r["cluster"], int(r["mal"]), r["policy"])][r["seed"]] = (
                -float(r["turnaround_delta_pct"]),
                float(r["cost_savings_delta_pct"]),
            )

    for cl in mm.CLUSTERS:
        path = args.figure_data_dir / f"op_scatter_{cl}.csv"
        with path.open("w", newline="") as f:
            w = csv.writer(f, lineterminator="\n")
            w.writerow(["mal_idx", "mal", "policy", "ta_mean", "ta_min", "ta_max", "cost_mean", "cost_min", "cost_max"])
            for i, m in enumerate(mm.MAL_LEVELS):
                for pol in POLS:
                    s = cells[(cl, m, pol)]
                    if set(s) != set(mm.SEEDS):
                        if not args.partial:
                            raise SystemExit(f"{cl} m{m} {pol}: expected seeds {mm.SEEDS}, found {sorted(s)} (use --partial)")
                        if not s:
                            continue
                    seeds = [x for x in mm.SEEDS if x in s]
                    tas = [s[x][0] for x in seeds]
                    cos = [s[x][1] for x in seeds]
                    w.writerow(
                        [i, m, pol, f"{mean(tas):.4f}", f"{min(tas):.4f}", f"{max(tas):.4f}", f"{mean(cos):.4f}", f"{min(cos):.4f}", f"{max(cos):.4f}"]
                    )
        print("wrote", path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
