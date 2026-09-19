#!/usr/bin/env python3
# Copyright (c) 2026 Fulda University of Applied Sciences.
# SPDX-License-Identifier: BSD-3-Clause
"""Shared definitions of the paper's aggregation: matrix layout, energy prices,
seed-paired baseline deltas, per-cell means and the main-matrix export bundle.

Everything here works on the aggregated per-run metrics (run_metrics.csv).
Raw simulation outputs are never read; run discovery does not exist. The
functions are the ones used to produce the paper's numbers, reduced to the
CSV-to-CSV path.
"""

from __future__ import annotations

import csv
import re
import sys
from pathlib import Path
from statistics import mean, stdev


CLUSTERS = ("cori_haswell", "cori_knl", "theta")
MAL_LEVELS = (0, 25, 50, 75, 100)
SEEDS = ("S0", "S3", "S7")
BASELINE_SCHEDULER = "rigid_easy_backfill"
MALLEABLE_SCHEDULER = "min_steal_agreement"
ANALYSIS_SCHEDULERS = (BASELINE_SCHEDULER, MALLEABLE_SCHEDULER)
# Shutdown policies with the paper's names: F0 = no shutdown, F10 = 10% of
# the nodes off at night, F30 = 30% of the nodes off at night. The tuple order
# is the bar order of the tables (F0, F30, F10).
POLICIES = ("F0", "F30", "F10")
MAIN_MATRIX_POLICIES = ("F0", "F30")

# Names used by older evaluation CSVs (policy_name values such as
# theta_70pct_pool); canonical_policy() maps them to the paper names.
LEGACY_POLICY_NAMES = {"no_shutdown": "F0", "90pct_pool": "F10", "70pct_pool": "F30"}

WAIT_THRESHOLDS = {"cori_haswell": 2.0, "cori_knl": 2.0, "theta": 20.0}

# Electricity prices in USD/kWh and emission factors in kgCO2e/kWh.
# Grid: U.S. Bureau of Labor Statistics average price data, electricity per
# kWh, West region November 2022 (series APU040072610, 0.181) for Cori and
# Midwest region January 2023 (series APU020072610, 0.148) for Theta.
# Solar: levelized cost of electricity of large rooftop and ground-mounted PV,
# Fraunhofer ISE 2024.
CLUSTER_ENERGY_FACTORS = {
    "cori_haswell": {
        "grid_cost_usd_per_kwh": 0.18,
        "solar_cost_usd_per_kwh": 0.06,
        "grid_co2e_kg_per_kwh": 0.264,
        "solar_co2e_kg_per_kwh": 0.04,
    },
    "cori_knl": {
        "grid_cost_usd_per_kwh": 0.18,
        "solar_cost_usd_per_kwh": 0.06,
        "grid_co2e_kg_per_kwh": 0.264,
        "solar_co2e_kg_per_kwh": 0.04,
    },
    "theta": {
        "grid_cost_usd_per_kwh": 0.15,
        "solar_cost_usd_per_kwh": 0.06,
        "grid_co2e_kg_per_kwh": 0.501,
        "solar_co2e_kg_per_kwh": 0.04,
    },
}

ANALYSIS_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DATA_DIR = ANALYSIS_ROOT / "data"
DEFAULT_RESULTS_DIR = ANALYSIS_ROOT / "results"
DEFAULT_FIGURE_DATA_DIR = ANALYSIS_ROOT / "figures" / "data"

RUN_METRIC_FIELDS = [
    "cluster",
    "mal",
    "seed",
    "scheduler",
    "policy",
    "run",
    "window_start_s",
    "window_end_s",
    "window_days",
    "wait_threshold_s",
    "jobs_seen",
    "jobs_completed_window_submit",
    "wait_mean_s",
    "wait_p95_s",
    "turnaround_mean_s",
    "turnaround_p95_s",
    "runtime_mean_s",
    "runtime_p95_s",
    "throughput_jobs_per_hour_submit_window",
    "num_nodes",
    "window_node_hours",
    "grid_cost_usd_per_kwh",
    "solar_cost_usd_per_kwh",
    "grid_co2e_kg_per_kwh",
    "solar_co2e_kg_per_kwh",
    "energy_total_kwh",
    "energy_on_kwh",
    "energy_boot_kwh",
    "energy_shutdown_kwh",
    "energy_off_kwh",
    "grid_energy_kwh",
    "solar_used_kwh",
    "solar_curtailed_kwh",
    "renewable_share_percent",
    "cost_total_usd",
    "co2_total_kg",
    "cost_per_day_usd",
    "co2_per_day_kg",
    "energy_per_day_kwh",
    "energy_per_node_hour_kwh",
    "cost_per_node_hour_usd",
    "co2_per_node_hour_kg",
    "power_avg_kw",
    "power_peak_kw",
    "off_fraction_mean",
    "boot_fraction_mean",
    "shutdown_fraction_mean",
]

SHUTDOWN_RUN_METRIC_FIELDS = [
    "cluster",
    "mal",
    "seed",
    "scheduler",
    "policy",
    "run",
    "window_start_s",
    "window_end_s",
    "full_shutdown_windows_count",
    "on_windows_count",
    "target_active_fraction",
    "target_off_fraction",
    "target_off_nodes",
    "mean_on_util_percent",
    "mean_off_util_percent",
    "node_off_fraction_percent",
    "node_overrun_node_hours_total",
    "node_overrun_node_hours_per_window",
    "node_overrun_normalized_hours_per_window",
]

DELTA_FIELDS = [
    "cluster",
    "mal",
    "seed",
    "scheduler",
    "policy",
    "baseline_cluster",
    "baseline_mal",
    "baseline_seed",
    "baseline_scheduler",
    "baseline_policy",
    "cost_savings_delta_pct",
    "co2_savings_delta_pct",
    "energy_savings_delta_pct",
    "turnaround_delta_pct",
    "turnaround_delta_s",
    "run_cost_per_day_usd",
    "baseline_cost_per_day_usd",
    "run_turnaround_mean_s",
    "baseline_turnaround_mean_s",
]

MAIN_MATRIX_FIELDS = [
    "cluster",
    "mal",
    "scheduler",
    "policy",
    "seed_count",
    "cost_savings_delta_pct_mean",
    "cost_savings_delta_pct_std",
    "turnaround_delta_pct_mean",
    "turnaround_delta_pct_std",
    "energy_savings_delta_pct_mean",
    "co2_savings_delta_pct_mean",
]

RESOURCE_BAR_METRIC_FIELDS = [
    "cluster",
    "mal",
    "scheduler",
    "policy",
    "bar_order",
    "bar_label",
    "seed_count",
    "energy_per_day_kwh_mean",
    "energy_per_day_kwh_std",
    "cost_per_day_usd_mean",
    "cost_per_day_usd_std",
    "co2_per_day_kg_mean",
    "co2_per_day_kg_std",
    "energy_per_node_hour_kwh_mean",
    "energy_per_node_hour_kwh_std",
    "cost_per_node_hour_usd_mean",
    "cost_per_node_hour_usd_std",
    "co2_per_node_hour_kg_mean",
    "co2_per_node_hour_kg_std",
]


# ---------------------------------------------------------------------------
# Small helpers
# ---------------------------------------------------------------------------


def warn(message: str) -> None:
    print(f"warning: {message}", file=sys.stderr)


def selected_malleable_scheduler(cluster: str) -> str:
    return MALLEABLE_SCHEDULER


def analysis_schedulers_for_cluster(cluster: str) -> tuple[str, str]:
    return ANALYSIS_SCHEDULERS


def scheduler_sort_index(cluster: str, scheduler: str) -> int:
    if scheduler not in ANALYSIS_SCHEDULERS:
        raise ValueError(f"Scheduler {scheduler} is not part of the analysis")
    return ANALYSIS_SCHEDULERS.index(scheduler)


def expected_run_count() -> int:
    return len(CLUSTERS) * len(MAL_LEVELS) * len(SEEDS) * len(ANALYSIS_SCHEDULERS) * len(POLICIES)


def energy_factors_for_cluster(cluster: str) -> dict[str, float]:
    return CLUSTER_ENERGY_FACTORS[cluster]


def canonical_policy(name: str) -> str:
    """Map a policy name to F0 / F10 / F30.

    Accepts the paper names themselves, policy_name values of the shipped
    templates (<cluster>_F0, <cluster>_F10, <cluster>_F30, matched as a
    separate token, case-insensitively) and, for older evaluation CSVs, any
    name containing no_shutdown, 90pct_pool or 70pct_pool.
    """
    text = str(name).strip()
    tokens = {token.upper() for token in re.split(r"[^A-Za-z0-9]+", text)}
    for policy in POLICIES:
        if policy in tokens:
            return policy
    lowered = text.lower()
    for legacy, policy in LEGACY_POLICY_NAMES.items():
        if legacy in lowered:
            return policy
    raise ValueError(f"Cannot map policy name to F0/F10/F30: {name!r}")


def run_name(cluster: str, mal: int, seed: str, scheduler: str, policy: str) -> str:
    return f"{cluster}_mal{int(mal)}_{seed}/{scheduler}/{policy}"


def run_key(row: dict[str, object]) -> tuple[str, int, str, str, str]:
    return (str(row["cluster"]), int(row["mal"]), str(row["seed"]), str(row["scheduler"]), str(row["policy"]))


def scheduler_label(scheduler: str) -> str:
    return {
        "rigid_easy_backfill": "EBF",
        "pref_steal_agreement": "PSA",
        "min_steal_agreement": "MSA",
    }[scheduler]


def policy_label(policy: str) -> str:
    return {
        "F0": "F0 (no shutdown)",
        "F10": "F10 (10% off)",
        "F30": "F30 (30% off)",
    }[policy]


def cluster_label(cluster: str) -> str:
    return {"cori_haswell": "Haswell", "cori_knl": "KNL", "theta": "Theta"}[cluster]


def load_dict_csv(path: Path) -> list[dict[str, object]]:
    with Path(path).open(newline="") as f:
        return list(csv.DictReader(f))


def write_dict_csv(path: Path, fieldnames: list[str], rows: list[dict[str, object]]) -> None:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames, lineterminator="\n")
        writer.writeheader()
        for row in rows:
            writer.writerow({k: row.get(k) for k in fieldnames})


def as_float(row: dict[str, object], key: str) -> float:
    value = row[key]
    if value in (None, ""):
        raise ValueError(f"Missing numeric value for {key} in {row}")
    return float(value)


def maybe_float(value: object) -> float | None:
    if value in (None, ""):
        return None
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def mean_std(values: list[float]) -> tuple[float | None, float | None]:
    """Seed mean and sample standard deviation; std is empty for a single seed."""
    if not values:
        return None, None
    return mean(values), (stdev(values) if len(values) > 1 else None)


# ---------------------------------------------------------------------------
# Matrix validation
# ---------------------------------------------------------------------------


def expected_keys() -> set[tuple[str, int, str, str, str]]:
    return {
        (cluster, mal, seed, scheduler, policy)
        for cluster in CLUSTERS
        for mal in MAL_LEVELS
        for seed in SEEDS
        for scheduler in ANALYSIS_SCHEDULERS
        for policy in POLICIES
    }


def filter_analysis_rows(rows: list[dict[str, object]], what: str) -> list[dict[str, object]]:
    """Keep rows of the analysed schedulers only and reject duplicate runs."""
    kept: list[dict[str, object]] = []
    seen: set[tuple[str, int, str, str, str]] = set()
    dropped = 0
    for row in rows:
        if str(row["scheduler"]) not in ANALYSIS_SCHEDULERS:
            dropped += 1
            continue
        key = run_key(row)
        if key in seen:
            raise ValueError(f"{what}: duplicate run {run_name(*key)}")
        seen.add(key)
        kept.append(row)
    if dropped:
        warn(f"{what}: ignored {dropped} rows of schedulers outside {ANALYSIS_SCHEDULERS}")
    return kept


def validate_run_matrix(rows: list[dict[str, object]], strict: bool, what: str) -> None:
    """Report the coverage of the 270-run matrix; raise in strict mode if incomplete."""
    actual = {run_key(r) for r in rows}
    expected = expected_keys()
    missing = sorted(expected - actual)
    extra = sorted(actual - expected)
    if not missing and not extra:
        print(f"{what}: complete matrix, {len(rows)} runs")
        return
    lines = [
        f"{what}: {len(actual & expected)} of {len(expected)} matrix runs present, "
        f"{len(missing)} missing, {len(extra)} outside the matrix"
    ]
    if missing:
        lines.append("missing: " + ", ".join(run_name(*k) for k in missing[:12]) + (" ..." if len(missing) > 12 else ""))
    if extra:
        lines.append("outside: " + ", ".join(run_name(*k) for k in extra[:12]) + (" ..." if len(extra) > 12 else ""))
    if strict:
        raise SystemExit("\n".join(["Run matrix validation failed (use --partial for incomplete matrices)."] + lines))
    for line in lines:
        warn(line)


# ---------------------------------------------------------------------------
# Repricing (optional): cost from stored energy sums and the current prices
# ---------------------------------------------------------------------------


def reprice_rows(rows: list[dict[str, object]]) -> list[dict[str, object]]:
    """Recompute the cost columns from grid/solar energy with CLUSTER_ENERGY_FACTORS.

    Electricity prices enter the analysis only here:
    cost = grid_price * grid_energy_kwh + solar_price * solar_used_kwh.
    """
    out: list[dict[str, object]] = []
    for row in rows:
        r = dict(row)
        factors = energy_factors_for_cluster(str(r["cluster"]))
        grid = as_float(r, "grid_energy_kwh")
        solar = as_float(r, "solar_used_kwh")
        days = as_float(r, "window_days")
        node_hours = as_float(r, "window_node_hours")
        cost = grid * factors["grid_cost_usd_per_kwh"] + solar * factors["solar_cost_usd_per_kwh"]
        co2 = grid * factors["grid_co2e_kg_per_kwh"] + solar * factors["solar_co2e_kg_per_kwh"]
        r.update(factors)
        r["cost_total_usd"] = cost
        r["co2_total_kg"] = co2
        r["cost_per_day_usd"] = cost / days if days > 0 else None
        r["co2_per_day_kg"] = co2 / days if days > 0 else None
        r["cost_per_node_hour_usd"] = cost / node_hours if node_hours > 0 else None
        r["co2_per_node_hour_kg"] = co2 / node_hours if node_hours > 0 else None
        out.append(r)
    return out


# ---------------------------------------------------------------------------
# Seed-paired deltas against EBF F0 at m0 and the per-cell aggregation
# ---------------------------------------------------------------------------


def compute_deltas(metrics: list[dict[str, object]], strict: bool = True) -> list[dict[str, object]]:
    """Compare every run with the same-cluster, same-seed m0 EBF F0 run.

    Deltas are computed per seed; seeds are aggregated only afterwards.
    Sign conventions: cost/CO2/energy savings positive is better,
    turnaround_delta_pct positive is worse.
    """
    by_key = {run_key(r): r for r in metrics}
    rows: list[dict[str, object]] = []
    skipped: list[str] = []
    for row in metrics:
        cluster = str(row["cluster"])
        seed = str(row["seed"])
        base_key = (cluster, 0, seed, BASELINE_SCHEDULER, "F0")
        base = by_key.get(base_key)
        if base is None:
            if strict:
                raise ValueError(f"Missing baseline run {run_name(*base_key)} for {run_name(*run_key(row))}")
            skipped.append(run_name(*run_key(row)))
            continue
        base_cost = as_float(base, "cost_per_day_usd")
        run_cost = as_float(row, "cost_per_day_usd")
        base_co2 = as_float(base, "co2_per_day_kg")
        run_co2 = as_float(row, "co2_per_day_kg")
        base_energy = as_float(base, "energy_per_day_kwh")
        run_energy = as_float(row, "energy_per_day_kwh")
        base_ta = as_float(base, "turnaround_mean_s")
        run_ta = as_float(row, "turnaround_mean_s")
        rows.append(
            {
                "cluster": cluster,
                "mal": int(row["mal"]),
                "seed": seed,
                "scheduler": row["scheduler"],
                "policy": row["policy"],
                "baseline_cluster": cluster,
                "baseline_mal": 0,
                "baseline_seed": seed,
                "baseline_scheduler": BASELINE_SCHEDULER,
                "baseline_policy": "F0",
                "cost_savings_delta_pct": (base_cost - run_cost) / base_cost * 100.0,
                "co2_savings_delta_pct": (base_co2 - run_co2) / base_co2 * 100.0,
                "energy_savings_delta_pct": (base_energy - run_energy) / base_energy * 100.0,
                "turnaround_delta_pct": (run_ta - base_ta) / base_ta * 100.0,
                "turnaround_delta_s": run_ta - base_ta,
                "run_cost_per_day_usd": run_cost,
                "baseline_cost_per_day_usd": base_cost,
                "run_turnaround_mean_s": run_ta,
                "baseline_turnaround_mean_s": base_ta,
            }
        )
    if skipped:
        warn(f"deltas: skipped {len(skipped)} runs without an m0 EBF F0 baseline of the same seed: " + ", ".join(skipped[:8]))
    return rows


def group_by_cell(rows: list[dict[str, object]]) -> dict[tuple[str, int, str, str], list[dict[str, object]]]:
    grouped: dict[tuple[str, int, str, str], list[dict[str, object]]] = {}
    for row in rows:
        key = (str(row["cluster"]), int(row["mal"]), str(row["scheduler"]), str(row["policy"]))
        grouped.setdefault(key, []).append(row)
    return grouped


def check_seed_count(key: tuple, rows: list[dict[str, object]], strict: bool) -> None:
    if strict and len(rows) != len(SEEDS):
        raise ValueError(f"Expected exactly {len(SEEDS)} seeds for {key}, got {len(rows)}")


def aggregate_main_matrix(deltas: list[dict[str, object]], strict: bool = True) -> list[dict[str, object]]:
    """Seed means of the MSA F0 and F30 cells (the main matrix of the paper)."""
    selected = [
        row
        for row in deltas
        if str(row["scheduler"]) == selected_malleable_scheduler(str(row["cluster"]))
        and str(row["policy"]) in MAIN_MATRIX_POLICIES
    ]
    grouped = group_by_cell(selected)
    out: list[dict[str, object]] = []
    for key in sorted(grouped, key=lambda x: (CLUSTERS.index(x[0]), x[1], MAIN_MATRIX_POLICIES.index(x[3]))):
        rows = grouped[key]
        check_seed_count(key, rows, strict)
        cluster, mal, scheduler, policy = key
        cost = mean_std([float(r["cost_savings_delta_pct"]) for r in rows])
        ta = mean_std([float(r["turnaround_delta_pct"]) for r in rows])
        energy = mean([float(r["energy_savings_delta_pct"]) for r in rows])
        co2 = mean([float(r["co2_savings_delta_pct"]) for r in rows])
        out.append(
            {
                "cluster": cluster,
                "mal": mal,
                "scheduler": scheduler,
                "policy": policy,
                "seed_count": len(rows),
                "cost_savings_delta_pct_mean": cost[0],
                "cost_savings_delta_pct_std": cost[1],
                "turnaround_delta_pct_mean": ta[0],
                "turnaround_delta_pct_std": ta[1],
                "energy_savings_delta_pct_mean": energy,
                "co2_savings_delta_pct_mean": co2,
            }
        )
    return out


def aggregate_resource_bar_metrics(metrics: list[dict[str, object]], strict: bool = True) -> list[dict[str, object]]:
    """Seed means of the absolute energy, cost and CO2 metrics per matrix cell."""
    grouped = group_by_cell(metrics)

    def bar_order_for(cluster: str, scheduler: str, policy: str) -> int:
        return POLICIES.index(policy) * 2 + scheduler_sort_index(cluster, scheduler) + 1

    out: list[dict[str, object]] = []
    for key in sorted(grouped, key=lambda x: (CLUSTERS.index(x[0]), x[1], bar_order_for(x[0], x[2], x[3]))):
        rows = grouped[key]
        check_seed_count(key, rows, strict)
        cluster, mal, scheduler, policy = key
        record: dict[str, object] = {
            "cluster": cluster,
            "mal": mal,
            "scheduler": scheduler,
            "policy": policy,
            "bar_order": bar_order_for(cluster, scheduler, policy),
            "bar_label": f"{scheduler_label(scheduler)} {policy_label(policy)}",
            "seed_count": len(rows),
        }
        for metric in (
            "energy_per_day_kwh",
            "cost_per_day_usd",
            "co2_per_day_kg",
            "energy_per_node_hour_kwh",
            "cost_per_node_hour_usd",
            "co2_per_node_hour_kg",
        ):
            record[f"{metric}_mean"], record[f"{metric}_std"] = mean_std([as_float(r, metric) for r in rows])
        out.append(record)
    return out


# ---------------------------------------------------------------------------
# Main-matrix plot data and the epslatex export bundle
# ---------------------------------------------------------------------------


def write_plot_data(main_rows: list[dict[str, object]], plot_data_dir: Path) -> None:
    """One TSV per cluster with the cost (left) and turnaround (right) half cells."""
    plot_data_dir.mkdir(parents=True, exist_ok=True)
    y_by_policy = {"F30": 1, "F0": 2}
    rows_by_cluster: dict[str, list[dict[str, object]]] = {c: [] for c in CLUSTERS}
    for row in main_rows:
        rows_by_cluster[str(row["cluster"])].append(row)

    for cluster, rows in rows_by_cluster.items():
        path = plot_data_dir / f"main_matrix_{cluster}.tsv"
        with path.open("w") as f:
            f.write("# x\ty\txlow\txhigh\tylow\tyhigh\tcolor_value\tlabel\tmetric\n")
            x_by_mal = {mal: i + 1 for i, mal in enumerate(MAL_LEVELS)}
            for row in rows:
                x = x_by_mal[int(row["mal"])]
                y = y_by_policy[str(row["policy"])]
                cost = float(row["cost_savings_delta_pct_mean"])
                turnaround_reduction = -float(row["turnaround_delta_pct_mean"])
                f.write(
                    f"{x - 0.225:.3f}\t{y}\t{x - 0.45:.3f}\t{x:.3f}\t{y - 0.45:.3f}\t{y + 0.45:.3f}\t"
                    f"{cost:.8f}\t{cost:+.1f}\tcost\n"
                )
                f.write(
                    f"{x + 0.225:.3f}\t{y}\t{x:.3f}\t{x + 0.45:.3f}\t{y - 0.45:.3f}\t{y + 0.45:.3f}\t"
                    f"{turnaround_reduction:.8f}\t{turnaround_reduction:+.1f}\tturnaround_reduction\n"
                )


PAPER_STYLE_GP = """# Shared paper-export Gnuplot helpers.
# Plot-specific font sizes, margins, and labels remain in each *_epslatex.gp file.
set datafile separator ","
GP_GOOD = "#1a9850"
GP_MID = "#f7f7f7"
GP_BAD = "#b2182b"
GP_TEXT = "#111111"
GP_GRID = "#d0d0d0"
GP_BORDER = "#777777"
GP_CELL_BORDER = "#666666"
GP_NEUTRAL = "#666666"
# Policy colors (F0/F10/F30) shared by all bar figures
# diverging value palette (GP_GOOD/GP_BAD) so policy identity never implies a rating.
GP_F0  = "#55A868"
GP_F10 = "#C44E52"
GP_F30 = "#4C72B0"
GP_LATEX_TEXT(size, baseline, text) = sprintf("%cfontsize{%s}{%s}%cselectfont %s", 92, size, baseline, 92, text)
GP_LATEX_LABEL(size, baseline, text) = sprintf("%c%cfontsize{%s}{%s}%c%cselectfont %s", 92, 92, size, baseline, 92, 92, text)
GP_TIC_FORMAT(fmt, size, baseline) = sprintf("%cfontsize{%s}{%s}%cselectfont %s", 92, size, baseline, 92, fmt)
"""


def write_paper_style_file(export_root: Path) -> Path:
    """Write the shared Gnuplot helpers next to an export bundle."""
    export_root.mkdir(parents=True, exist_ok=True)
    style_path = export_root / "paper_style.gp"
    style_path.write_text(PAPER_STYLE_GP, encoding="utf-8")
    return style_path


MAIN_COMBINED_FONT_BLOCK = """# Font controls for this figure.
GP_MAIN_COMBINED_CELL_FONT_SIZE = "6.6pt"
GP_MAIN_COMBINED_CELL_FONT_BASELINE = "6.6pt"
GP_MAIN_COMBINED_TIC_FONT_SIZE = "8.0pt"
GP_MAIN_COMBINED_TIC_FONT_BASELINE = "9.0pt"
GP_MAIN_COMBINED_CBLABEL_FONT_SIZE = "8.5pt"
GP_MAIN_COMBINED_CBLABEL_FONT_BASELINE = "7.8pt"
GP_MAIN_COMBINED_TITLE_FONT_SIZE = "10.0pt"
GP_MAIN_COMBINED_TITLE_FONT_BASELINE = "11.5pt"
GP_MAIN_COMBINED_HEADER_FONT_SIZE = "8.5pt"
GP_MAIN_COMBINED_HEADER_FONT_BASELINE = "9.5pt"
GP_MAIN_COMBINED_NOTE_FONT_SIZE = "6.5pt"
GP_MAIN_COMBINED_NOTE_FONT_BASELINE = "7.5pt"
GP_MAIN_COMBINED_CELL_LABEL(text) = GP_LATEX_LABEL(GP_MAIN_COMBINED_CELL_FONT_SIZE, GP_MAIN_COMBINED_CELL_FONT_BASELINE, text)
"""


MAIN_SINGLE_FONT_BLOCK = """# Font controls shared by the three per-cluster main matrices.
GP_MAIN_SINGLE_CELL_FONT_SIZE = "6.6pt"
GP_MAIN_SINGLE_CELL_FONT_BASELINE = "6.6pt"
GP_MAIN_SINGLE_TIC_FONT_SIZE = "7.0pt"
GP_MAIN_SINGLE_TIC_FONT_BASELINE = "8.0pt"
GP_MAIN_SINGLE_CBLABEL_FONT_SIZE = "6.5pt"
GP_MAIN_SINGLE_CBLABEL_FONT_BASELINE = "5.6pt"
GP_MAIN_SINGLE_TITLE_FONT_SIZE = "9.5pt"
GP_MAIN_SINGLE_TITLE_FONT_BASELINE = "11.5pt"
GP_MAIN_SINGLE_CELL_LABEL(text) = GP_LATEX_LABEL(GP_MAIN_SINGLE_CELL_FONT_SIZE, GP_MAIN_SINGLE_CELL_FONT_BASELINE, text)
"""


def main_matrix_epslatex_preamble(
    *,
    output_name: str,
    size: str,
    xtics: str,
    ytics: str,
    title_label: str | None,
    legend_label: str | None,
    multiplot: str | None,
    colorbox_origin: str,
    colorbox_size: str,
    cblabel_offset: str,
    font_scope: str,
    font_block: str,
) -> str:
    labels = ""
    if title_label is not None:
        labels += (
            f'set label 1000 GP_LATEX_TEXT({font_scope}_HEADER_FONT_SIZE, {font_scope}_HEADER_FONT_BASELINE, "{title_label}") at screen 0.50, screen 0.985 center front\n'
        )
    if legend_label is not None:
        labels += (
            f'set label 1001 GP_LATEX_TEXT({font_scope}_NOTE_FONT_SIZE, {font_scope}_NOTE_FONT_BASELINE, "{legend_label}") at screen 0.50, screen 0.960 center front\n'
        )
    multiplot_line = f"{multiplot}\n" if multiplot is not None else ""
    return f"""set terminal epslatex color size {size} font ",8"
set output "{output_name}.tex"
load "../paper_style.gp"
{font_block}
{labels}{multiplot_line}set xrange [0.5:5.5]
set yrange [0.5:2.5]
set xtics ({xtics}) scale 0
set ytics ({ytics}) scale 0
set cbrange [-50:50]
set format cb GP_TIC_FORMAT("%g", {font_scope}_TIC_FONT_SIZE, {font_scope}_TIC_FONT_BASELINE)
set cblabel GP_LATEX_TEXT({font_scope}_CBLABEL_FONT_SIZE, {font_scope}_CBLABEL_FONT_BASELINE, "green=better, red=worse") offset {cblabel_offset}
set colorbox user origin {colorbox_origin} size {colorbox_size}
set palette defined (-50 GP_BAD, 0 GP_MID, 50 GP_GOOD)
set style fill solid 1.0 border rgb GP_CELL_BORDER
set key off
do for [x=1.5:4.5:1] {{
  set arrow from x,0.5 to x,2.5 nohead dt 2 lc rgb GP_BORDER lw 0.5
}}
"""


def main_matrix_epslatex_plot(
    cluster: str,
    label_function: str = "GP_MAIN_COMBINED_CELL_LABEL",
    font_scope: str = "GP_MAIN_COMBINED",
) -> str:
    data = f"main_matrix_{cluster}.csv"
    return (
        f'set title GP_LATEX_TEXT({font_scope}_TITLE_FONT_SIZE, {font_scope}_TITLE_FONT_BASELINE, "{cluster_label(cluster)}")\n'
        f'plot "{data}" every ::1 using 1:2:3:4:5:6:7 with boxxyerrorbars lc palette notitle, \\\n'
        f'     "{data}" every ::1 using 1:2:({label_function}(strcol(8))) with labels center tc rgb GP_TEXT notitle\n'
    )


def write_paper_export_bundle(results_root: Path) -> Path:
    """Write the self-contained main-matrix CSV + epslatex Gnuplot bundle.

    The bundle renders the colored cost/turnaround matrix of MSA F0 and F30
    against m0 EBF F0. Run it with `gnuplot main_matrix_all_clusters_epslatex.gp`
    from the bundle directory.
    """
    export_dir = results_root / "paper_export" / "main_matrix"
    export_dir.mkdir(parents=True, exist_ok=True)
    write_paper_style_file(results_root / "paper_export")
    (export_dir / "main_matrix_single_fonts.gp").write_text(MAIN_SINGLE_FONT_BLOCK, encoding="utf-8")
    plot_data_dir = results_root / "plot_data"

    columns = ("x", "y", "xlow", "xhigh", "ylow", "yhigh", "color_value", "label", "metric")
    for cluster in CLUSTERS:
        src = plot_data_dir / f"main_matrix_{cluster}.tsv"
        dst = export_dir / f"main_matrix_{cluster}.csv"
        with src.open() as f_in, dst.open("w", newline="") as f_out:
            writer = csv.writer(f_out, lineterminator="\n")
            writer.writerow(columns)
            for line in f_in:
                if not line.strip() or line.startswith("#"):
                    continue
                writer.writerow(line.rstrip("\n").split("\t"))

    # epslatex labels are LaTeX strings. The Gnuplot source keeps a literal
    # backslash pair so that \char37{} (a percent sign) survives both parsers.
    legend_label = "left = cost savings [\\\\char37{{}} / day], right = turnaround reduction [\\\\char37{{}}]"
    single_font_block = 'load "main_matrix_single_fonts.gp"\n'

    def xtics_for(font_scope: str) -> str:
        return ", ".join(
            f'GP_LATEX_TEXT({font_scope}_TIC_FONT_SIZE, {font_scope}_TIC_FONT_BASELINE, "m{mal}") {i + 1}'
            for i, mal in enumerate(MAL_LEVELS)
        )

    def ytics_for(font_scope: str) -> str:
        return (
            f'GP_LATEX_TEXT({font_scope}_TIC_FONT_SIZE, {font_scope}_TIC_FONT_BASELINE, "F30") 1, '
            f'GP_LATEX_TEXT({font_scope}_TIC_FONT_SIZE, {font_scope}_TIC_FONT_BASELINE, "F0") 2'
        )

    combined_preamble = main_matrix_epslatex_preamble(
        output_name="main_matrix_all_clusters",
        size="16cm,15cm",
        xtics=xtics_for("GP_MAIN_COMBINED"),
        ytics=ytics_for("GP_MAIN_COMBINED"),
        title_label="MSA cost savings and turnaround reduction vs m0 EBF F0",
        legend_label=legend_label,
        multiplot="set multiplot layout 3,1 margins 0.11,0.865,0.060,0.885 spacing 0.00,0.095",
        colorbox_origin="0.900,0.145",
        colorbox_size="0.025,0.680",
        cblabel_offset="-12.0,0",
        font_scope="GP_MAIN_COMBINED",
        font_block=MAIN_COMBINED_FONT_BLOCK,
    )
    gp = export_dir / "main_matrix_all_clusters_epslatex.gp"
    with gp.open("w", encoding="utf-8") as f:
        f.write("# Run from this directory with: gnuplot main_matrix_all_clusters_epslatex.gp\n")
        f.write(combined_preamble + "\n")
        for cluster in CLUSTERS:
            f.write(main_matrix_epslatex_plot(cluster, font_scope="GP_MAIN_COMBINED"))
        f.write("unset multiplot\nunset label 1000\nunset label 1001\n")

    for cluster in CLUSTERS:
        single_preamble = main_matrix_epslatex_preamble(
            output_name=f"main_matrix_{cluster}",
            size="12cm,5.2cm",
            xtics=xtics_for("GP_MAIN_SINGLE"),
            ytics=ytics_for("GP_MAIN_SINGLE"),
            title_label=None,
            legend_label=None,
            multiplot=None,
            colorbox_origin="0.805,0.200",
            colorbox_size="0.030,0.580",
            cblabel_offset="-12.0,0",
            font_scope="GP_MAIN_SINGLE",
            font_block=single_font_block,
        )
        cluster_gp = export_dir / f"main_matrix_{cluster}_epslatex.gp"
        with cluster_gp.open("w", encoding="utf-8") as f:
            f.write(f"# Run from this directory with: gnuplot main_matrix_{cluster}_epslatex.gp\n")
            f.write(single_preamble + "\n")
            f.write("set lmargin at screen 0.145\nset rmargin at screen 0.770\n")
            f.write("set bmargin at screen 0.180\nset tmargin at screen 0.820\n")
            f.write(main_matrix_epslatex_plot(cluster, label_function="GP_MAIN_SINGLE_CELL_LABEL", font_scope="GP_MAIN_SINGLE"))
            f.write("\n")
    return export_dir
