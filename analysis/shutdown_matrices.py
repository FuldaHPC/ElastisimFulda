#!/usr/bin/env python3
# Copyright (c) 2026 Fulda University of Applied Sciences.
# SPDX-License-Identifier: BSD-3-Clause
"""Shutdown-policy target metrics aggregated from shutdown_run_metrics.csv.

Reads the per-run shutdown metrics of the F10 and F30 runs (10 % and 30 % of
the nodes off at night) and writes

  <results>/shutdown_policy_mal_matrix.csv               mean over seeds and schedulers per cluster, policy, mal
  <results>/shutdown_policy_cluster_matrix.csv           additionally mean over the mal levels
  <results>/shutdown_policy_scheduler_mal_matrix.csv     mean over seeds per cluster, policy, scheduler, mal
  <results>/shutdown_policy_scheduler_cluster_matrix.csv additionally mean over the mal levels
  <figure-data>/shutdown_policy_cluster_F30_{ebf,msa}.csv

The last two CSVs feed figures/shutdown_policy_matrices_by_cluster_epslatex.gp
(F30 mean on-window utilization and normalized node overrun per cluster and
scheduler). Per-run definitions:

  mean_on_util_percent      time-weighted all-node CPU utilization over the complete
                            on-windows between two shutdown windows
  mean_off_util_percent     the same over the complete shutdown windows
  node_off_fraction_percent off node seconds / (nodes * shutdown-window seconds)
  node_overrun_node_hours_per_window
                            integral of max(0, active nodes - target * nodes) over the
                            shutdown windows, in node hours per shutdown window
  node_overrun_normalized_hours_per_window
                            the overrun divided by the number of nodes that should be off

Only complete shutdown windows inside the dynamic analysis window count.
"""

from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path
from statistics import mean, stdev

sys.path.insert(0, str(Path(__file__).resolve().parent / "lib"))
import main_matrix as mm  # noqa: E402


# Policies with shutdown windows, in the row order of the matrices (F30, F10).
POOL_POLICIES = ("F30", "F10")
TARGET_ACTIVE_PERCENT = {"F30": 70.0, "F10": 90.0}
METRIC_SPECS = (
    ("mean_on_util_percent", "Mean On Util [%]", "percent"),
    ("mean_off_util_percent", "Mean Off Util [%]", "percent"),
    ("node_off_fraction_percent", "Node Off Fraction [%]", "percent"),
    ("node_overrun_node_hours_per_window", "Node Overrun [node-h/window]", "node_hours"),
)
SUMMARY_METRIC_SPECS = (
    ("mean_on_util_percent", "Mean On Util [%]", "percent"),
    ("mean_off_util_percent", "Mean Off Util [%]", "percent"),
    ("node_off_fraction_percent", "Node Off Fraction [%]", "percent"),
    ("node_overrun_normalized_hours_per_window", "Node Overrun [h/window]", "hours"),
)
SCHEDULER_SUMMARY_PLOT_METRIC_SPECS = (
    ("mean_on_util_percent", "Mean On Util [%]", "percent"),
    ("node_overrun_normalized_hours_per_window", "Node Overrun [h/window]", "hours"),
)
PALETTE_GOOD = "#1a9850"
PALETTE_MID = "#f7f7f7"
PALETTE_BAD = "#b2182b"
CELL_PASTEL_FRACTION = 0.62

AGG_FIELDS = [
    "cluster",
    "policy",
    "mal",
    "sample_count",
    "mean_on_util_percent_mean",
    "mean_on_util_percent_std",
    "mean_off_util_percent_mean",
    "mean_off_util_percent_std",
    "node_off_fraction_percent_mean",
    "node_off_fraction_percent_std",
    "node_overrun_node_hours_per_window_mean",
    "node_overrun_node_hours_per_window_std",
]

SUMMARY_FIELDS = [
    "policy",
    "cluster",
    "sample_count",
    "mean_on_util_percent_mean",
    "mean_on_util_percent_std",
    "mean_off_util_percent_mean",
    "mean_off_util_percent_std",
    "node_off_fraction_percent_mean",
    "node_off_fraction_percent_std",
    "node_overrun_normalized_hours_per_window_mean",
    "node_overrun_normalized_hours_per_window_std",
]

SCHEDULER_MAL_FIELDS = [
    "cluster",
    "policy",
    "scheduler",
    "mal",
    "sample_count",
    "mean_on_util_percent_mean",
    "mean_on_util_percent_std",
    "mean_off_util_percent_mean",
    "mean_off_util_percent_std",
    "node_off_fraction_percent_mean",
    "node_off_fraction_percent_std",
    "node_overrun_node_hours_per_window_mean",
    "node_overrun_node_hours_per_window_std",
]

SCHEDULER_CLUSTER_FIELDS = [
    "policy",
    "scheduler",
    "cluster",
    "sample_count",
    "mean_on_util_percent_mean",
    "mean_on_util_percent_std",
    "mean_off_util_percent_mean",
    "mean_off_util_percent_std",
    "node_off_fraction_percent_mean",
    "node_off_fraction_percent_std",
    "node_overrun_normalized_hours_per_window_mean",
    "node_overrun_normalized_hours_per_window_std",
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--data-dir", type=Path, default=mm.DEFAULT_DATA_DIR, help="Directory containing shutdown_run_metrics.csv")
    parser.add_argument("--shutdown-metrics", type=Path, default=None, help="Explicit shutdown_run_metrics.csv (overrides --data-dir)")
    parser.add_argument("--results-dir", type=Path, default=mm.DEFAULT_RESULTS_DIR, help="Output directory for the matrix CSVs")
    parser.add_argument("--figure-data-dir", type=Path, default=mm.DEFAULT_FIGURE_DATA_DIR, help="Output directory for the figure CSVs")
    parser.add_argument("--partial", action="store_true", help="Accept an incomplete run matrix")
    return parser.parse_args()


def enrich_normalized_overrun(rows: list[dict[str, object]]) -> list[dict[str, object]]:
    for row in rows:
        target_off_nodes = mm.maybe_float(row.get("target_off_nodes"))
        if target_off_nodes is None:
            raise ValueError(f"Missing target_off_nodes for {row.get('run')}")
        if mm.maybe_float(row.get("node_overrun_normalized_hours_per_window")) is None:
            overrun_per_window = mm.maybe_float(row.get("node_overrun_node_hours_per_window"))
            if overrun_per_window is not None and target_off_nodes > 0:
                row["node_overrun_normalized_hours_per_window"] = overrun_per_window / target_off_nodes
    return rows


def validate_shutdown_matrix(rows: list[dict[str, object]], strict: bool) -> None:
    expected = {k for k in mm.expected_keys() if k[4] in POOL_POLICIES}
    actual = {mm.run_key(r) for r in rows}
    missing = sorted(expected - actual)
    extra = sorted(actual - expected)
    if not missing and not extra:
        print(f"shutdown_run_metrics: complete matrix, {len(rows)} runs")
        return
    lines = [f"shutdown_run_metrics: {len(actual & expected)} of {len(expected)} shutdown runs present, {len(missing)} missing, {len(extra)} outside the matrix"]
    if missing:
        lines.append("missing: " + ", ".join(mm.run_name(*k) for k in missing[:12]) + (" ..." if len(missing) > 12 else ""))
    if strict:
        raise SystemExit("\n".join(["Shutdown run matrix validation failed (use --partial for incomplete matrices)."] + lines))
    for line in lines:
        mm.warn(line)


def aggregate(
    rows: list[dict[str, object]],
    group_keys: tuple[str, ...],
    metric_specs: tuple[tuple[str, str, str], ...],
) -> list[dict[str, object]]:
    grouped: dict[tuple[object, ...], list[dict[str, object]]] = {}
    for row in rows:
        grouped.setdefault(tuple(row[key] for key in group_keys), []).append(row)

    out: list[dict[str, object]] = []
    for key in sorted(grouped):
        group = grouped[key]
        agg = {name: value for name, value in zip(group_keys, key)}
        agg["sample_count"] = len(group)
        for metric, _label, _unit in metric_specs:
            values = [mm.maybe_float(row.get(metric)) for row in group]
            clean = [value for value in values if value is not None]
            agg[f"{metric}_mean"] = mean(clean) if clean else None
            agg[f"{metric}_std"] = stdev(clean) if len(clean) > 1 else 0.0 if clean else None
        out.append(agg)
    return out


def check_samples(rows: list[dict[str, object]], expected_count: int, strict: bool, what: str) -> None:
    for row in rows:
        if int(row["sample_count"]) != expected_count:
            message = f"{what}: expected {expected_count} samples, got {row['sample_count']} for " + " ".join(
                str(row[k]) for k in ("cluster", "policy", "scheduler", "mal") if k in row
            )
            if strict:
                raise ValueError(message)
            mm.warn(message)


def aggregate_by_mal(rows: list[dict[str, object]], strict: bool) -> list[dict[str, object]]:
    out = aggregate(rows, ("cluster", "policy", "mal"), METRIC_SPECS)
    check_samples(out, len(mm.SEEDS) * len(mm.ANALYSIS_SCHEDULERS), strict, "by mal")
    return sorted(out, key=lambda r: (mm.CLUSTERS.index(str(r["cluster"])), POOL_POLICIES.index(str(r["policy"])), int(r["mal"])))


def aggregate_by_cluster(rows: list[dict[str, object]], strict: bool) -> list[dict[str, object]]:
    out = aggregate(rows, ("policy", "cluster"), SUMMARY_METRIC_SPECS)
    check_samples(out, len(mm.MAL_LEVELS) * len(mm.SEEDS) * len(mm.ANALYSIS_SCHEDULERS), strict, "by cluster")
    return sorted(out, key=lambda r: (POOL_POLICIES.index(str(r["policy"])), mm.CLUSTERS.index(str(r["cluster"]))))


def aggregate_by_scheduler_mal(rows: list[dict[str, object]], strict: bool) -> list[dict[str, object]]:
    out = aggregate(rows, ("cluster", "policy", "scheduler", "mal"), METRIC_SPECS)
    check_samples(out, len(mm.SEEDS), strict, "by scheduler and mal")
    return sorted(
        out,
        key=lambda r: (
            mm.CLUSTERS.index(str(r["cluster"])),
            POOL_POLICIES.index(str(r["policy"])),
            mm.scheduler_sort_index(str(r["cluster"]), str(r["scheduler"])),
            int(r["mal"]),
        ),
    )


def aggregate_by_scheduler_cluster(rows: list[dict[str, object]], strict: bool) -> list[dict[str, object]]:
    out = aggregate(rows, ("policy", "scheduler", "cluster"), SUMMARY_METRIC_SPECS)
    check_samples(out, len(mm.MAL_LEVELS) * len(mm.SEEDS), strict, "by scheduler and cluster")
    return sorted(
        out,
        key=lambda r: (
            POOL_POLICIES.index(str(r["policy"])),
            mm.CLUSTERS.index(str(r["cluster"])),
            mm.scheduler_sort_index(str(r["cluster"]), str(r["scheduler"])),
        ),
    )


def color_hex_for(metric: str, value: float | None, policy: str, overrun_max: float) -> str:
    if value is None:
        return "#d9d9d9"
    target_active = TARGET_ACTIVE_PERCENT[policy]
    target_off = 100.0 - target_active
    if metric == "mean_on_util_percent":
        ratio = min(abs(100.0 - value) / 25.0, 1.0)
    elif metric == "mean_off_util_percent":
        ratio = min(abs(target_active - value) / max(target_active, 100.0 - target_active), 1.0)
    elif metric == "node_off_fraction_percent":
        ratio = min(abs(target_off - value) / max(target_off, 100.0 - target_off), 1.0)
    elif metric in {"node_overrun_node_hours_per_window", "node_overrun_normalized_hours_per_window"}:
        ratio = 0.0 if overrun_max <= 0 else min(value / overrun_max, 1.0)
    else:
        ratio = 1.0
    return soften_color(blend_color(PALETTE_GOOD, PALETTE_MID, PALETTE_BAD, ratio), CELL_PASTEL_FRACTION)


def blend_color(good: str, mid: str, bad: str, ratio: float) -> str:
    ratio = max(0.0, min(1.0, ratio))
    a = hex_to_rgb(good if ratio <= 0.5 else mid)
    b = hex_to_rgb(mid if ratio <= 0.5 else bad)
    local = ratio / 0.5 if ratio <= 0.5 else (ratio - 0.5) / 0.5
    rgb = tuple(round(a[i] * (1.0 - local) + b[i] * local) for i in range(3))
    return f"#{rgb[0]:02x}{rgb[1]:02x}{rgb[2]:02x}"


def soften_color(color: str, white_fraction: float) -> str:
    rgb = hex_to_rgb(color)
    white_fraction = max(0.0, min(1.0, white_fraction))
    mixed = tuple(round(rgb[i] * (1.0 - white_fraction) + 255 * white_fraction) for i in range(3))
    return f"#{mixed[0]:02x}{mixed[1]:02x}{mixed[2]:02x}"


def hex_to_rgb(value: str) -> tuple[int, int, int]:
    value = value.lstrip("#")
    return int(value[0:2], 16), int(value[2:4], 16), int(value[4:6], 16)


def format_value(metric: str, value: float | None) -> str:
    if value is None:
        return "n/a"
    if metric == "node_overrun_node_hours_per_window":
        return f"{value:.1f}"
    if metric == "node_overrun_normalized_hours_per_window":
        return f"{value:.2f}h"
    return f"{value:.1f}%"


def selected_scheduler_summary_rows(
    summary_rows: list[dict[str, object]], strict: bool
) -> tuple[str, list[dict[str, object]], list[dict[str, object]], float]:
    policy = "F30"
    ebf_rows = [row for row in summary_rows if row["policy"] == policy and row["scheduler"] == mm.BASELINE_SCHEDULER]
    msa_rows = [
        row
        for row in summary_rows
        if row["policy"] == policy and row["scheduler"] == mm.selected_malleable_scheduler(str(row["cluster"]))
    ]
    rows_for_plot = ebf_rows + msa_rows
    clusters = {str(row["cluster"]) for row in rows_for_plot}
    if clusters != set(mm.CLUSTERS) or len(ebf_rows) != len(mm.CLUSTERS) or len(msa_rows) != len(mm.CLUSTERS):
        message = "F30 summary rows are not available for every cluster and both schedulers"
        if strict:
            raise ValueError(message)
        mm.warn(message + "; the figure will have empty cells and its color scale differs from the paper")
    overrun_max = max(
        (mm.maybe_float(row.get("node_overrun_normalized_hours_per_window_mean")) or 0.0 for row in rows_for_plot),
        default=0.0,
    )
    return policy, ebf_rows, msa_rows, overrun_max


def write_figure_data(scheduler_summary_rows: list[dict[str, object]], figure_data_dir: Path, strict: bool) -> list[Path]:
    """CSV cells (position, color, label, value) of the F30 EBF and MSA panels."""
    figure_data_dir.mkdir(parents=True, exist_ok=True)
    policy, ebf_rows, msa_rows, overrun_max = selected_scheduler_summary_rows(scheduler_summary_rows, strict)
    fieldnames = ["x", "y", "xlow", "xhigh", "ylow", "yhigh", "color_rgb", "label", "metric", "cluster", "value"]

    def write_panel_csv(path: Path, rows: list[dict[str, object]]) -> None:
        row_map = {str(row["cluster"]): row for row in rows}
        with path.open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames, lineterminator="\n")
            writer.writeheader()
            for col_idx, cluster in enumerate(mm.CLUSTERS, start=1):
                row = row_map.get(cluster)
                if row is None:
                    continue
                for metric_idx, (metric, _metric_label, _unit) in enumerate(SCHEDULER_SUMMARY_PLOT_METRIC_SPECS):
                    y = len(SCHEDULER_SUMMARY_PLOT_METRIC_SPECS) - metric_idx
                    value = mm.maybe_float(row.get(f"{metric}_mean"))
                    color = color_hex_for(metric, value, policy, overrun_max)
                    writer.writerow(
                        {
                            "x": col_idx,
                            "y": y,
                            "xlow": col_idx - 0.48,
                            "xhigh": col_idx + 0.48,
                            "ylow": y - 0.46,
                            "yhigh": y + 0.46,
                            "color_rgb": int(color.lstrip("#"), 16),
                            "label": format_value(metric, value).replace("%", r"\\char37{}"),
                            "metric": metric,
                            "cluster": cluster,
                            "value": "" if value is None else f"{value:.12g}",
                        }
                    )

    ebf_csv = figure_data_dir / f"shutdown_policy_cluster_{policy}_ebf.csv"
    msa_csv = figure_data_dir / f"shutdown_policy_cluster_{policy}_msa.csv"
    write_panel_csv(ebf_csv, ebf_rows)
    write_panel_csv(msa_csv, msa_rows)
    return [ebf_csv, msa_csv]


def main() -> int:
    args = parse_args()
    strict = not args.partial
    metrics_path = args.shutdown_metrics or (args.data_dir / "shutdown_run_metrics.csv")
    if not metrics_path.exists():
        raise SystemExit(f"Missing shutdown run metrics: {metrics_path}")
    rows = mm.filter_analysis_rows(mm.load_dict_csv(metrics_path), "shutdown_run_metrics")
    rows = [row for row in rows if row["policy"] in POOL_POLICIES]
    validate_shutdown_matrix(rows, strict)
    rows = enrich_normalized_overrun(rows)

    by_mal = aggregate_by_mal(rows, strict)
    by_cluster = aggregate_by_cluster(rows, strict)
    by_scheduler_mal = aggregate_by_scheduler_mal(rows, strict)
    by_scheduler_cluster = aggregate_by_scheduler_cluster(rows, strict)
    results = args.results_dir
    mm.write_dict_csv(results / "shutdown_policy_mal_matrix.csv", AGG_FIELDS, by_mal)
    mm.write_dict_csv(results / "shutdown_policy_cluster_matrix.csv", SUMMARY_FIELDS, by_cluster)
    mm.write_dict_csv(results / "shutdown_policy_scheduler_mal_matrix.csv", SCHEDULER_MAL_FIELDS, by_scheduler_mal)
    mm.write_dict_csv(results / "shutdown_policy_scheduler_cluster_matrix.csv", SCHEDULER_CLUSTER_FIELDS, by_scheduler_cluster)
    paths = write_figure_data(by_scheduler_cluster, args.figure_data_dir, strict)
    print(f"wrote four shutdown policy matrices under {results} and {len(paths)} figure CSVs under {args.figure_data_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
