#!/usr/bin/env python3
# Copyright (c) 2026 Fulda University of Applied Sciences.
# SPDX-License-Identifier: BSD-3-Clause
"""Seed means of MSA wait, runtime and turnaround time per cluster and policy.

Reads run_metrics.csv and writes

  <results>/job_bar_metrics.csv
  <figure-data>/<cluster>_{wait,runtime,turnaround}_mean_h.csv

The CSVs under figure-data feed figures/job_bar_charts_all_clusters_epslatex.gp
(the turnaround time decomposition figure). Job selection is the same as for
the turnaround values of the main matrix: completed jobs submitted inside the
dynamic analysis window, with their full wait, runtime and turnaround time.
Values are converted from seconds to hours and averaged over the seeds.
"""

from __future__ import annotations

import argparse
import csv
import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "lib"))
import main_matrix as mm  # noqa: E402


@dataclass(frozen=True)
class BarSeries:
    key: str
    label: str
    scheduler: str
    policy: str


METRIC_KEYS = ("wait_mean_h_mean", "runtime_mean_h_mean", "turnaround_mean_h_mean")

JOB_BAR_METRIC_FIELDS = [
    "cluster",
    "mal",
    "scheduler",
    "policy",
    "bar_order",
    "bar_label",
    "seed_count",
    "wait_mean_h_mean",
    "wait_mean_h_std",
    "runtime_mean_h_mean",
    "runtime_mean_h_std",
    "turnaround_mean_h_mean",
    "turnaround_mean_h_std",
]
JOB_COMPOSITION_POLICIES = mm.POLICIES


def bar_series_for_cluster(cluster: str) -> tuple[BarSeries, ...]:
    malleable = mm.selected_malleable_scheduler(cluster)
    label = mm.scheduler_label(malleable)
    # Column order of the figure CSVs (msa_F0, msa_F30, msa_F10); the Gnuplot
    # file reads the columns by position.
    return tuple(
        BarSeries(f"msa_{policy}", f"{label} {mm.policy_label(policy)}", malleable, policy)
        for policy in JOB_COMPOSITION_POLICIES
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--data-dir", type=Path, default=mm.DEFAULT_DATA_DIR, help="Directory containing run_metrics.csv")
    parser.add_argument("--run-metrics", type=Path, default=None, help="Explicit run_metrics.csv (overrides --data-dir)")
    parser.add_argument("--results-dir", type=Path, default=mm.DEFAULT_RESULTS_DIR, help="Output directory for job_bar_metrics.csv")
    parser.add_argument("--figure-data-dir", type=Path, default=mm.DEFAULT_FIGURE_DATA_DIR, help="Output directory for the figure CSVs")
    parser.add_argument("--partial", action="store_true", help="Accept an incomplete run matrix")
    return parser.parse_args()


def aggregate_job_bar_metrics(rows: list[dict[str, object]], strict: bool) -> list[dict[str, object]]:
    selected = [
        row
        for row in rows
        if str(row["scheduler"]) == mm.selected_malleable_scheduler(str(row["cluster"]))
        and str(row["policy"]) in JOB_COMPOSITION_POLICIES
    ]
    grouped = mm.group_by_cell(selected)

    def bar_order_for(policy: str) -> int:
        return JOB_COMPOSITION_POLICIES.index(policy) + 1

    def hours(row: dict[str, object], key: str) -> float:
        return mm.as_float(row, key) / 3600.0

    out: list[dict[str, object]] = []
    for key in sorted(grouped, key=lambda x: (mm.CLUSTERS.index(x[0]), x[1], bar_order_for(x[3]))):
        group = grouped[key]
        mm.check_seed_count(key, group, strict)
        cluster, mal, scheduler, policy = key
        wait = mm.mean_std([hours(row, "wait_mean_s") for row in group])
        runtime = mm.mean_std([hours(row, "runtime_mean_s") for row in group])
        turnaround = mm.mean_std([hours(row, "turnaround_mean_s") for row in group])
        series = next(s for s in bar_series_for_cluster(cluster) if s.scheduler == scheduler and s.policy == policy)
        out.append(
            {
                "cluster": cluster,
                "mal": mal,
                "scheduler": scheduler,
                "policy": policy,
                "bar_order": bar_order_for(policy),
                "bar_label": series.label,
                "seed_count": len(group),
                "wait_mean_h_mean": wait[0],
                "wait_mean_h_std": wait[1],
                "runtime_mean_h_mean": runtime[0],
                "runtime_mean_h_std": runtime[1],
                "turnaround_mean_h_mean": turnaround[0],
                "turnaround_mean_h_std": turnaround[1],
            }
        )
    return out


def write_figure_data(job_rows: list[dict[str, object]], figure_data_dir: Path) -> list[Path]:
    """One CSV per cluster and metric: mal_idx, mal_label, one column per policy series."""
    figure_data_dir.mkdir(parents=True, exist_ok=True)
    by_key = {(str(r["cluster"]), int(r["mal"]), str(r["scheduler"]), str(r["policy"])): r for r in job_rows}
    paths: list[Path] = []
    for cluster in mm.CLUSTERS:
        series_for_cluster = bar_series_for_cluster(cluster)
        for metric in METRIC_KEYS:
            path = figure_data_dir / f"{cluster}_{metric.removesuffix('_mean')}.csv"
            with path.open("w", newline="", encoding="utf-8") as f:
                writer = csv.writer(f, lineterminator="\n")
                writer.writerow(["mal_idx", "mal_label", *(series.key for series in series_for_cluster)])
                for idx, mal in enumerate(mm.MAL_LEVELS):
                    values = []
                    for series in series_for_cluster:
                        row = by_key.get((cluster, mal, series.scheduler, series.policy))
                        values.append("NaN" if row is None else f"{float(row[metric]):.12g}")
                    writer.writerow([idx, f"mal{mal}", *values])
            paths.append(path)
    return paths


def main() -> int:
    args = parse_args()
    strict = not args.partial
    metrics_path = args.run_metrics or (args.data_dir / "run_metrics.csv")
    if not metrics_path.exists():
        raise SystemExit(f"Missing run metrics: {metrics_path}")
    rows = mm.filter_analysis_rows(mm.load_dict_csv(metrics_path), "run_metrics")
    mm.validate_run_matrix(rows, strict, "run_metrics")
    job_rows = aggregate_job_bar_metrics(rows, strict)
    mm.write_dict_csv(args.results_dir / "job_bar_metrics.csv", JOB_BAR_METRIC_FIELDS, job_rows)
    paths = write_figure_data(job_rows, args.figure_data_dir)
    print(f"wrote job_bar_metrics.csv ({len(job_rows)} cells) and {len(paths)} figure CSVs under {args.figure_data_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
