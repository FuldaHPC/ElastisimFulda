#!/usr/bin/env python3
# Copyright (c) 2026 Fulda University of Applied Sciences.
# SPDX-License-Identifier: BSD-3-Clause
"""Aggregate per-run metrics into the paper's baseline deltas and main matrix.

Reads run_metrics.csv (one row per simulation run) and writes, without any run
discovery or raw simulation output:

  <results>/run_deltas.csv            seed-paired deltas against m0 EBF F0
  <results>/main_matrix.csv           seed means of the MSA F0/F30 cells
  <results>/resource_bar_metrics.csv  seed means of energy, cost and CO2
  <results>/plot_data/                main-matrix plot data (TSV)
  <results>/paper_export/main_matrix/ CSV + epslatex Gnuplot bundle

Usage:
  python3 aggregate.py                      # analysis/data -> analysis/results
  python3 aggregate.py --data-dir D --results-dir R --partial
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "lib"))
import main_matrix as mm  # noqa: E402


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--data-dir", type=Path, default=mm.DEFAULT_DATA_DIR, help="Directory containing run_metrics.csv")
    parser.add_argument("--run-metrics", type=Path, default=None, help="Explicit run_metrics.csv (overrides --data-dir)")
    parser.add_argument("--results-dir", type=Path, default=mm.DEFAULT_RESULTS_DIR, help="Output directory")
    parser.add_argument("--partial", action="store_true", help="Accept an incomplete run matrix (missing cells are skipped)")
    parser.add_argument(
        "--reprice",
        action="store_true",
        help="Recompute the cost columns from the stored energy sums with the prices in lib/main_matrix.py "
        "and write the repriced run_metrics.csv to the results directory",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    strict = not args.partial
    metrics_path = args.run_metrics or (args.data_dir / "run_metrics.csv")
    if not metrics_path.exists():
        raise SystemExit(f"Missing run metrics: {metrics_path}")
    results = args.results_dir
    results.mkdir(parents=True, exist_ok=True)

    metrics = mm.filter_analysis_rows(mm.load_dict_csv(metrics_path), "run_metrics")
    mm.validate_run_matrix(metrics, strict, "run_metrics")
    if args.reprice:
        metrics = mm.reprice_rows(metrics)
        mm.write_dict_csv(results / "run_metrics.csv", mm.RUN_METRIC_FIELDS, metrics)
        print(f"repriced {len(metrics)} runs -> {results / 'run_metrics.csv'}")

    mm.write_dict_csv(results / "resource_bar_metrics.csv", mm.RESOURCE_BAR_METRIC_FIELDS, mm.aggregate_resource_bar_metrics(metrics, strict))
    deltas = mm.compute_deltas(metrics, strict)
    mm.write_dict_csv(results / "run_deltas.csv", mm.DELTA_FIELDS, deltas)
    main_rows = mm.aggregate_main_matrix(deltas, strict)
    mm.write_dict_csv(results / "main_matrix.csv", mm.MAIN_MATRIX_FIELDS, main_rows)
    mm.write_plot_data(main_rows, results / "plot_data")
    export_dir = mm.write_paper_export_bundle(results)
    print(f"wrote {len(deltas)} deltas, {len(main_rows)} main-matrix cells and the export bundle under {results}")
    print(f"optional colored matrix figure: cd {export_dir} && gnuplot main_matrix_all_clusters_epslatex.gp")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
