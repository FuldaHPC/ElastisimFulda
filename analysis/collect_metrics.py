#!/usr/bin/env python3
# Copyright (c) 2026 Fulda University of Applied Sciences.
# SPDX-License-Identifier: BSD-3-Clause
"""Convert evaluate.py CSVs of re-run simulations into the aggregation schemas.

Reads one or more CSVs written by `scripts/evaluate.py --mode paper` and writes

  <out-dir>/run_metrics.csv           one row per run, schema of analysis/data/run_metrics.csv
  <out-dir>/shutdown_run_metrics.csv  one row per F10/F30 run, schema of
                                      analysis/data/shutdown_run_metrics.csv

so that aggregate.py, job_bar_charts.py, shutdown_matrices.py and the
generate_*.py scripts accept the new runs. Rows with analysis_status other
than "ok" or a mode other than "paper" are skipped with a warning.

Costs are recomputed in USD from the grid and solar energy sums with the
prices in lib/main_matrix.py (the paper's prices), independent of the currency
or price recorded by the evaluation; --keep-costs takes the evaluation's own
price and cost columns instead. Emission
factors always come from lib/main_matrix.py.
"""

from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "lib"))
import main_matrix as mm  # noqa: E402


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("csvs", metavar="EVALUATION_CSV", nargs="+", type=Path, help="CSV files written by scripts/evaluate.py --mode paper")
    parser.add_argument("--out-dir", type=Path, required=True, help="Directory for run_metrics.csv and shutdown_run_metrics.csv")
    parser.add_argument("--keep-costs", action="store_true", help="Use the evaluation's price and cost columns instead of repricing")
    return parser.parse_args()


def first(row: dict[str, str], *names: str) -> str:
    """Value of the first present column among alternative spellings."""
    for name in names:
        if name in row and row[name] not in (None, ""):
            return row[name]
    return ""


def num(row: dict[str, str], *names: str) -> float:
    value = first(row, *names)
    if value == "":
        raise ValueError(f"missing numeric column {names[0]}")
    return float(value)


def opt(row: dict[str, str], *names: str) -> float | None:
    value = first(row, *names)
    return None if value == "" else float(value)


def convert_run(row: dict[str, str], keep_costs: bool) -> tuple[dict[str, object], dict[str, object] | None]:
    cluster = row["cluster"]
    mal = int(float(row["malleable_share"]))
    seed = row["seed"]
    scheduler = row["scheduler"]
    policy = mm.canonical_policy(row["policy"])
    name = mm.run_name(cluster, mal, seed, scheduler, policy)

    start = num(row, "job_window_start_s")
    end = num(row, "job_window_end_s")
    duration_s = end - start
    if duration_s <= 0:
        raise ValueError("empty analysis window")
    window_days = duration_s / 86400.0
    window_hours = duration_s / 3600.0
    n_nodes = int(float(row["num_nodes"]))
    node_hours = n_nodes * duration_s / 3600.0
    completed = int(float(row["jobs_selected"]))

    energy_total = num(row, "energy_total_kwh")
    grid = num(row, "grid_energy_kwh")
    solar = num(row, "solar_used_kwh")
    factors = mm.CLUSTER_ENERGY_FACTORS.get(cluster)
    if keep_costs:
        grid_price = num(row, "grid_cost_usd_per_kwh")
        solar_price = num(row, "solar_cost_usd_per_kwh")
        cost = num(row, "cost_total_usd")
    else:
        if factors is None:
            raise ValueError(f"no prices known for cluster {cluster!r}; use --keep-costs")
        grid_price = factors["grid_cost_usd_per_kwh"]
        solar_price = factors["solar_cost_usd_per_kwh"]
        cost = grid * grid_price + solar * solar_price
    if factors is None:
        grid_co2 = solar_co2 = co2 = None
    else:
        grid_co2 = factors["grid_co2e_kg_per_kwh"]
        solar_co2 = factors["solar_co2e_kg_per_kwh"]
        co2 = grid * grid_co2 + solar * solar_co2

    def per_day(value: float | None) -> float | None:
        return None if value is None else value / window_days

    def per_node_hour(value: float | None) -> float | None:
        return None if value is None else value / node_hours

    metrics: dict[str, object] = {
        "cluster": cluster,
        "mal": mal,
        "seed": seed,
        "scheduler": scheduler,
        "policy": policy,
        "run": name,
        "window_start_s": start,
        "window_end_s": end,
        "window_days": window_days,
        "wait_threshold_s": num(row, "wait_threshold_s"),
        "jobs_seen": int(float(row["jobs_seen"])),
        "jobs_completed_window_submit": completed,
        "wait_mean_s": opt(row, "wait_mean_s"),
        "wait_p95_s": opt(row, "wait_p95_s"),
        "turnaround_mean_s": opt(row, "turnaround_mean_s"),
        "turnaround_p95_s": opt(row, "turnaround_p95_s"),
        "runtime_mean_s": opt(row, "runtime_mean_s"),
        "runtime_p95_s": opt(row, "runtime_p95_s"),
        "throughput_jobs_per_hour_submit_window": completed / window_hours,
        "num_nodes": n_nodes,
        "window_node_hours": node_hours,
        "grid_cost_usd_per_kwh": grid_price,
        "solar_cost_usd_per_kwh": solar_price,
        "grid_co2e_kg_per_kwh": grid_co2,
        "solar_co2e_kg_per_kwh": solar_co2,
        "energy_total_kwh": energy_total,
        "energy_on_kwh": num(row, "energy_on_kwh"),
        "energy_boot_kwh": num(row, "energy_boot_kwh"),
        "energy_shutdown_kwh": num(row, "energy_shutdown_kwh"),
        "energy_off_kwh": num(row, "energy_off_kwh"),
        "grid_energy_kwh": grid,
        "solar_used_kwh": solar,
        "solar_curtailed_kwh": num(row, "solar_curtailed_kwh"),
        "renewable_share_percent": (solar / energy_total * 100.0) if energy_total > 0 else None,
        "cost_total_usd": cost,
        "co2_total_kg": co2,
        "cost_per_day_usd": per_day(cost),
        "co2_per_day_kg": per_day(co2),
        "energy_per_day_kwh": per_day(energy_total),
        "energy_per_node_hour_kwh": per_node_hour(energy_total),
        "cost_per_node_hour_usd": per_node_hour(cost),
        "co2_per_node_hour_kg": per_node_hour(co2),
        "power_avg_kw": energy_total / window_hours,
        "power_peak_kw": num(row, "power_peak_kw"),
        "off_fraction_mean": num(row, "off_fraction_mean"),
        "boot_fraction_mean": num(row, "boot_fraction_mean"),
        "shutdown_fraction_mean": num(row, "shutdown_fraction_mean"),
    }

    shutdown: dict[str, object] | None = None
    if row.get("shutdown_status") == "ok":
        shutdown = {
            "cluster": cluster,
            "mal": mal,
            "seed": seed,
            "scheduler": scheduler,
            "policy": policy,
            "run": name,
            "window_start_s": start,
            "window_end_s": end,
            "full_shutdown_windows_count": int(float(row["full_shutdown_windows_count"])),
            "on_windows_count": int(float(row["on_windows_count"])),
            "target_active_fraction": num(row, "target_active_fraction"),
            "target_off_fraction": num(row, "target_off_fraction"),
            "target_off_nodes": num(row, "target_off_nodes"),
            "mean_on_util_percent": opt(row, "mean_on_util_percent"),
            "mean_off_util_percent": opt(row, "mean_off_util_percent"),
            "node_off_fraction_percent": opt(row, "node_off_fraction_percent"),
            "node_overrun_node_hours_total": opt(row, "node_overrun_node_hours_total"),
            "node_overrun_node_hours_per_window": opt(row, "node_overrun_node_hours_per_window"),
            "node_overrun_normalized_hours_per_window": opt(row, "node_overrun_normalized_hours_per_window"),
        }
    return metrics, shutdown


def main() -> int:
    args = parse_args()
    metrics: list[dict[str, object]] = []
    shutdown_rows: list[dict[str, object]] = []
    seen: dict[tuple[str, int, str, str, str], str] = {}
    skipped = 0
    for path in args.csvs:
        with path.open(newline="", encoding="utf-8") as f:
            for line, row in enumerate(csv.DictReader(f), start=2):
                where = f"{path}:{line} ({row.get('run_id', '?')})"
                if row.get("analysis_status") != "ok":
                    mm.warn(f"{where}: skipped, analysis_status={row.get('analysis_status')!r} {row.get('analysis_reason', '')}")
                    skipped += 1
                    continue
                if row.get("mode") != "paper":
                    mm.warn(f"{where}: skipped, only --mode paper evaluations can be aggregated (mode={row.get('mode')!r})")
                    skipped += 1
                    continue
                try:
                    run_row, shutdown_row = convert_run(row, args.keep_costs)
                except (KeyError, ValueError) as error:
                    mm.warn(f"{where}: skipped, {error}")
                    skipped += 1
                    continue
                key = mm.run_key(run_row)
                if key in seen:
                    raise SystemExit(f"{where}: duplicate run {run_row['run']} (first seen in {seen[key]})")
                seen[key] = where
                metrics.append(run_row)
                if shutdown_row is not None:
                    shutdown_rows.append(shutdown_row)
    if not metrics:
        raise SystemExit("no usable runs found")
    metrics.sort(key=mm.run_key)
    shutdown_rows.sort(key=mm.run_key)
    args.out_dir.mkdir(parents=True, exist_ok=True)
    mm.write_dict_csv(args.out_dir / "run_metrics.csv", mm.RUN_METRIC_FIELDS, metrics)
    mm.write_dict_csv(args.out_dir / "shutdown_run_metrics.csv", mm.SHUTDOWN_RUN_METRIC_FIELDS, shutdown_rows)
    print(f"collected {len(metrics)} runs ({len(shutdown_rows)} with shutdown metrics, {skipped} skipped) into {args.out_dir}")
    mm.validate_run_matrix(metrics, strict=False, what="collected runs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
