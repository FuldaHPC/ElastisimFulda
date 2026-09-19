#!/usr/bin/env python3
# Copyright (c) 2026 Fulda University of Applied Sciences.
# SPDX-License-Identifier: BSD-3-Clause
"""Evaluate explicit completed runs; never launch a simulator or discover a matrix.

The job, sampled-power, solar, and shutdown calculations follow the paper's
analysis pipeline. In paper mode its submit-window and left-sample integration
conventions are retained. Observed mode includes all completed jobs and reports
the actual energy sample coverage, without extrapolating a missing final sample.
Energy costs are reported in USD using the grid and solar prices per kWh from
the run's input/evaluation.json. No plots are generated.

Every requested run produces one CSV row. Missing/malformed/unfinished runs are
errors; insufficient energy coverage produces a partial row with available job
statistics. Either makes the command exit 1 after writing all rows. Inapplicable
shutdown metrics are empty with an explicit reason and do not fail evaluation.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import sys
from pathlib import Path
from statistics import mean


FIELDS = """run_id run_dir mode analysis_status analysis_reason run_status exit_code
cluster scheduler seed malleable_share policy expected_jobs jobs_seen jobs_completed
jobs_killed jobs_selected job_window_start_s job_window_end_s wait_threshold_s
wait_mean_s wait_p95_s runtime_mean_s runtime_p95_s turnaround_mean_s turnaround_p95_s
energy_status energy_window_start_s energy_window_end_s energy_duration_s
cpu_first_sample_s cpu_last_sample_s cpu_samples energy_coverage_fraction
unobserved_head_s unobserved_tail_s num_nodes grid_cost_usd_per_kwh
solar_cost_usd_per_kwh solar_peak_kw solar_sunrise_hour solar_sunset_hour
solar_night_fraction energy_total_kwh energy_on_kwh energy_boot_kwh
energy_shutdown_kwh energy_off_kwh grid_energy_kwh solar_used_kwh
solar_curtailed_kwh cost_total_usd energy_per_day_kwh cost_per_day_usd
power_avg_kw power_peak_kw off_fraction_mean boot_fraction_mean
shutdown_fraction_mean shutdown_status shutdown_reason full_shutdown_windows_count
on_windows_count mean_on_util_status mean_on_util_percent mean_off_util_percent
node_off_fraction_percent target_active_fraction target_off_fraction target_off_nodes
node_overrun_node_hours_total node_overrun_node_hours_per_window
node_overrun_normalized_hours_per_window""".split()

STATE_CATEGORY = {
    "free": "on", "allocated": "on", "draining": "on", "reserved": "on",
    "booting": "boot", "shutting_down": "shutdown", "off": "off",
}
ENERGY_FIELDS = [
    "cpu_power_idle", "cpu_power_peak", "cpu_power_boot", "cpu_power_shutdown",
    "cpu_power_off", "node_power_without_cpus_idle", "node_power_without_cpus_peak",
    "node_power_without_cpus_boot", "node_power_without_cpus_shutdown",
    "node_power_without_cpus_off", "cpus_per_node",
]
EVALUATION_FIELDS = [
    "wait_threshold_s", "grid_cost_usd_per_kwh", "solar_cost_usd_per_kwh",
    "solar_peak_kw", "solar_sunrise_hour", "solar_sunset_hour", "solar_night_fraction",
]


def number(value: object, label: str, minimum: float = 0.0) -> float:
    if isinstance(value, bool):
        raise ValueError(f"{label}: expected a finite number")
    result = float(value)
    if not math.isfinite(result) or result < minimum:
        raise ValueError(f"{label}: expected a finite number >= {minimum}")
    return result


def read_json(path: Path) -> dict:
    with path.open(encoding="utf-8") as stream:
        value = json.load(stream)
    if not isinstance(value, dict):
        raise ValueError(f"{path.name}: expected a JSON object")
    return value


def load_parameters(run: Path) -> tuple[dict[str, float], dict[str, float]]:
    energy_json = read_json(run / "input" / "energy.json")
    evaluation_json = read_json(run / "input" / "evaluation.json")
    energy = {key: number(energy_json[key], key) for key in ENERGY_FIELDS}
    evaluation = {key: number(evaluation_json[key], key) for key in EVALUATION_FIELDS}
    if energy["cpus_per_node"] <= 0:
        raise ValueError("cpus_per_node must be positive")
    if (energy["cpu_power_peak"] < energy["cpu_power_idle"] or
            energy["node_power_without_cpus_peak"] < energy["node_power_without_cpus_idle"]):
        raise ValueError("Peak power must be at least idle power")
    if not (0 <= evaluation["solar_sunrise_hour"] < evaluation["solar_sunset_hour"] <= 24):
        raise ValueError("Solar hours must satisfy 0 <= sunrise < sunset <= 24")
    if evaluation["solar_night_fraction"] > 1:
        raise ValueError("solar_night_fraction must be between 0 and 1")
    return energy, evaluation


def read_jobs(path: Path) -> list[dict]:
    required = {"ID", "Submit Time", "Start Time", "End Time", "Wait Time",
                "Makespan", "Turnaround Time", "Status"}
    jobs = []
    ids = set()
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        if not required.issubset(reader.fieldnames or []):
            raise ValueError("job_statistics.csv: missing required columns")
        for line, row in enumerate(reader, 2):
            if None in row or any(row.get(key) is None for key in required):
                raise ValueError(f"job_statistics.csv:{line}: malformed row")
            if not row["ID"] or row["ID"] in ids:
                raise ValueError(f"job_statistics.csv:{line}: missing or duplicate job ID")
            ids.add(row["ID"])
            status = row["Status"]
            if status not in {"completed", "killed"}:
                raise ValueError(f"job_statistics.csv:{line}: unfinished job status {status!r}")
            job = {"ID": row["ID"], "Status": status}
            for key in required - {"ID", "Status"}:
                # The simulator derives the times from floating-point clocks, so
                # rounding noise just below zero is clamped to 0.
                job[key] = max(0.0, number(row[key], f"job_statistics.csv:{line}:{key}", minimum=-1e-6))
            if job["End Time"] < job["Start Time"] or job["Start Time"] < job["Submit Time"]:
                raise ValueError(f"job_statistics.csv:{line}: inconsistent job times")
            jobs.append(job)
    if not jobs:
        raise ValueError("job_statistics.csv: no jobs")
    return jobs


def percentile(values: list[float], q: float) -> float | None:
    """Retain the paper's lower-index percentile, without interpolation."""
    ordered = sorted(values)
    return ordered[int(q * (len(ordered) - 1))] if ordered else None


def job_metrics(jobs: list[dict], mode: str, threshold: float) -> dict:
    result = {
        "jobs_seen": len(jobs),
        "jobs_completed": sum(job["Status"] == "completed" for job in jobs),
        "jobs_killed": sum(job["Status"] == "killed" for job in jobs),
    }
    if mode == "paper":
        delayed = [job["Submit Time"] for job in jobs if job["Wait Time"] > threshold]
        end = max(job["Submit Time"] for job in jobs)
        if not delayed or end <= min(delayed):
            raise ValueError("paper_window_unavailable: no positive submit window above the wait threshold")
        start = min(delayed)
    else:
        # Keep the simulation's t=0 origin, including idle time before submission.
        start, end = 0.0, max(job["End Time"] for job in jobs)
    selected = [job for job in jobs if job["Status"] == "completed" and
                (mode == "observed" or start <= job["Submit Time"] <= end)]
    result.update(job_window_start_s=start, job_window_end_s=end,
                  wait_threshold_s=threshold if mode == "paper" else None,
                  jobs_selected=len(selected))
    for prefix, column in [("wait", "Wait Time"), ("runtime", "Makespan"),
                           ("turnaround", "Turnaround Time")]:
        values = [job[column] for job in selected]
        result[f"{prefix}_mean_s"] = mean(values) if values else None
        result[f"{prefix}_p95_s"] = percentile(values, 0.95)
    return result


def cpu_header(path: Path) -> list[str]:
    with path.open(newline="", encoding="utf-8") as stream:
        header = next(csv.reader(stream), [])
    if (len(header) < 2 or header[0] != "Time" or
            any(not name for name in header[1:]) or len(set(header[1:])) != len(header) - 1):
        raise ValueError("cpu_utilization.csv: invalid Time/node header")
    return header[1:]


def iter_cpu_rows(path: Path, node_count: int):
    previous = None
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.reader(stream)
        next(reader, None)
        for line, cells in enumerate(reader, 2):
            if not cells:
                continue
            if len(cells) != node_count + 1:
                raise ValueError(f"cpu_utilization.csv:{line}: wrong node column count")
            time = number(cells[0], f"cpu_utilization.csv:{line}:Time")
            if previous is not None and time <= previous:
                raise ValueError(f"cpu_utilization.csv:{line}: sample times must increase")
            # Empty utilization cells are interpreted as zero, as in the paper's pipeline.
            utilization = sum(number(cell or 0, f"cpu_utilization.csv:{line}:utilization")
                              for cell in cells[1:])
            previous = time
            yield time, utilization


def cpu_coverage(path: Path, node_count: int) -> dict:
    first = last = None
    count = 0
    for time, _util in iter_cpu_rows(path, node_count):
        if first is None:
            first = time
        last = time
        count += 1
    if not count:
        raise ValueError("cpu_utilization.csv: no samples")
    return {"cpu_first_sample_s": first, "cpu_last_sample_s": last, "cpu_samples": count}


def iter_node_events(path: Path, node_names: list[str]):
    known_nodes = set(node_names)
    previous = None
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.reader(stream)
        if next(reader, [])[:3] != ["Time", "Node", "State"]:
            raise ValueError("node_utilization.csv: invalid Time,Node,State header")
        for line, cells in enumerate(reader, 2):
            if not cells:
                continue
            if len(cells) < 3:
                raise ValueError(f"node_utilization.csv:{line}: malformed event")
            time = number(cells[0], f"node_utilization.csv:{line}:Time")
            state = cells[2].lower()
            if cells[1] not in known_nodes or state not in STATE_CATEGORY:
                raise ValueError(f"node_utilization.csv:{line}: unknown node or state")
            if previous is not None and time < previous:
                raise ValueError(f"node_utilization.csv:{line}: event times must not decrease")
            previous = time
            yield time, cells[1], STATE_CATEGORY[state]


def sampled_intervals(run: Path, node_names: list[str]):
    """Retain the paper pipeline's state-at-left-CPU-sample integration convention."""
    events = iter_node_events(run / "node_utilization.csv", node_names)
    try:
        event = next(events, None)
        states = {name: "on" for name in node_names}
        counts = {"on": len(node_names), "boot": 0, "shutdown": 0, "off": 0}
        previous = None
        for time, utilization in iter_cpu_rows(run / "cpu_utilization.csv", len(node_names)):
            while event is not None and event[0] <= time:
                _, node, category = event
                counts[states[node]] -= 1
                counts[category] += 1
                states[node] = category
                event = next(events, None)
            if previous is not None:
                left, old_utilization, old_counts = previous
                yield left, time, old_utilization, old_counts
            previous = time, utilization, counts.copy()
        # Validate trailing state events too; they do not extend CPU coverage.
        for _event in events:
            pass
    finally:
        events.close()


def solar_energy_kwh(start_s: float, end_s: float, parameters: dict[str, float]) -> float:
    peak_kw = parameters["solar_peak_kw"]
    if end_s <= start_s or peak_kw <= 0:
        return 0.0
    sunrise = parameters["solar_sunrise_hour"] * 3600.0
    sunset = parameters["solar_sunset_hour"] * 3600.0
    night_kw = peak_kw * parameters["solar_night_fraction"]
    daylight_seconds = 0.0
    first_day = math.floor((start_s - sunset) / 86400.0)
    last_day = math.floor((end_s - sunrise) / 86400.0) + 1
    for day in range(first_day, last_day + 1):
        a = max(start_s, day * 86400.0 + sunrise)
        b = min(end_s, day * 86400.0 + sunset)
        if b > a:
            daylight_seconds += b - a
    total_kw_s = night_kw * (end_s - start_s) + (peak_kw - night_kw) * daylight_seconds
    return total_kw_s / 3600.0


def shutdown_windows(policy: dict, start: float, end: float) -> tuple[list, list, dict]:
    result = {"shutdown_status": "not_applicable", "shutdown_reason": "no_shutdown_windows",
              "full_shutdown_windows_count": 0, "on_windows_count": 0,
              "mean_on_util_status": "no_complete_on_window"}
    if policy.get("policy_type", "static") != "static":
        raise ValueError("Only static shutdown policies are supported")
    raw_windows = policy.get("shutdown_windows", [])
    if not isinstance(raw_windows, list):
        raise ValueError("shutdown_windows must be a list")
    if not raw_windows:
        return [], [], result
    use_time_window = policy.get("use_time_window", False)
    if not isinstance(use_time_window, bool):
        raise ValueError("use_time_window must be boolean")
    active_start, active_end = 0.0, math.inf
    if use_time_window:
        active_start = number(policy["window_start"], "window_start")
        active_end = number(policy["window_end"], "window_end")
        if active_end < active_start:
            raise ValueError("Policy window_end precedes window_start")
    if active_start >= end or active_end <= start:
        result["shutdown_reason"] = "policy_inactive_in_window"
        return [], [], result
    windows = []
    for item in raw_windows:
        a = number(item["start_hour"], "shutdown start_hour")
        b = number(item["end_hour"], "shutdown end_hour")
        target = number(item["target_fraction"], "target_fraction")
        if a >= 24 or b > 24 or target > 1 or a == b:
            raise ValueError("Invalid daily shutdown window or target")
        windows.append((a, b, target))
    if len({target for _a, _b, target in windows}) != 1:
        raise ValueError("Shutdown metrics require one static target fraction per run")
    if windows[0][2] == 1:
        result["shutdown_reason"] = "no_off_node_target"
        return [], [], result
    instances = []
    for day in range(math.floor(start / 86400.0) - 1, math.floor(end / 86400.0) + 2):
        for a, b, target in windows:
            left = day * 86400.0 + a * 3600.0
            right = day * 86400.0 + b * 3600.0 + (86400.0 if a > b else 0.0)
            if left >= max(start, active_start) and right <= min(end, active_end):
                instances.append((left, right, target))
    instances.sort()
    if any(right > following[0] for (_left, right, _target), following in zip(instances, instances[1:])):
        raise ValueError("Overlapping shutdown windows are not supported")
    if not instances:
        result["shutdown_reason"] = "no_complete_shutdown_window"
        return [], [], result
    lead = number(policy.get("timing", {}).get("drain_lead_time", 0), "drain_lead_time")
    on_windows = []
    for (_a, b, _target), following in zip(instances, instances[1:]):
        on_end = following[0] - lead if lead > 1 else following[0]
        if on_end > b and b >= start and on_end <= end:
            on_windows.append((b, on_end))
    target = mean(item[2] for item in instances)
    result.update(shutdown_status="ok", shutdown_reason="", target_active_fraction=target,
                  target_off_fraction=1 - target, full_shutdown_windows_count=len(instances),
                  on_windows_count=len(on_windows),
                  mean_on_util_status="ok" if on_windows else "no_complete_on_window")
    return [(a, b) for a, b, _target in instances], on_windows, result


def overlap_seconds(start: float, end: float, windows: list[tuple[float, float]]) -> float:
    if end <= start:
        return 0.0
    return sum(max(0.0, min(end, b) - max(start, a)) for a, b in windows)


def sampled_metrics(run: Path, node_names: list[str], energy: dict[str, float],
                    evaluation: dict[str, float], start: float, end: float,
                    policy: dict) -> dict:
    off_windows, on_windows, result = shutdown_windows(policy, start, end)
    cpus = energy["cpus_per_node"]
    idle = cpus * energy["cpu_power_idle"] + energy["node_power_without_cpus_idle"]
    slope = (cpus * (energy["cpu_power_peak"] - energy["cpu_power_idle"]) +
             (energy["node_power_without_cpus_peak"] - energy["node_power_without_cpus_idle"]))
    powers = {state: cpus * energy[f"cpu_power_{state}"] + energy[f"node_power_without_cpus_{state}"]
              for state in ("boot", "shutdown", "off")}
    energies = {state: 0.0 for state in ("on", "boot", "shutdown", "off")}
    node_seconds = {state: 0.0 for state in energies}
    total_energy = grid_energy = solar_used = solar_curtailed = peak = 0.0
    on_util_seconds = off_util_seconds = off_node_seconds = overrun = 0.0
    on_seconds = off_seconds = 0.0
    n_nodes = len(node_names)
    for left, right, util_sum, counts in sampled_intervals(run, node_names):
        a, b = max(left, start), min(right, end)
        if b <= a:
            continue
        duration = b - a
        state_power = {"on": counts["on"] * idle + util_sum * slope}
        state_power.update({state: counts[state] * powers[state] for state in powers})
        power_kw = sum(state_power.values()) / 1000.0
        demand = power_kw * duration / 3600.0
        available_solar = solar_energy_kwh(a, b, evaluation)
        used = min(demand, available_solar)
        total_energy += demand
        grid_energy += demand - used
        solar_used += used
        solar_curtailed += max(0.0, available_solar - used)
        peak = max(peak, power_kw)
        for state in energies:
            energies[state] += state_power[state] * duration / 3_600_000.0
            node_seconds[state] += counts[state] * duration
        dt_on = overlap_seconds(a, b, on_windows)
        dt_off = overlap_seconds(a, b, off_windows)
        on_util_seconds += util_sum / n_nodes * dt_on
        off_util_seconds += util_sum / n_nodes * dt_off
        on_seconds += dt_on
        off_seconds += dt_off
        off_node_seconds += counts["off"] * dt_off
        if dt_off > 0:
            overrun += max(0.0, counts["on"] - result["target_active_fraction"] * n_nodes) * (dt_off / 3600.0)
    duration = end - start
    days = duration / 86400.0
    cost = (grid_energy * evaluation["grid_cost_usd_per_kwh"] +
            solar_used * evaluation["solar_cost_usd_per_kwh"])
    result.update(energy_total_kwh=total_energy, grid_energy_kwh=grid_energy,
                  solar_used_kwh=solar_used, solar_curtailed_kwh=solar_curtailed,
                  cost_total_usd=cost, energy_per_day_kwh=total_energy / days,
                  cost_per_day_usd=cost / days, power_avg_kw=total_energy / (duration / 3600.0),
                  power_peak_kw=peak)
    for state, value in energies.items():
        result[f"energy_{state}_kwh"] = value
    for state in ("off", "boot", "shutdown"):
        result[f"{state}_fraction_mean"] = node_seconds[state] / (n_nodes * duration)
    if off_windows:
        target_off_nodes = result["target_off_fraction"] * n_nodes
        overrun_per_window = overrun / len(off_windows)
        result.update(target_off_nodes=target_off_nodes,
                      mean_on_util_percent=on_util_seconds / on_seconds * 100.0 if on_seconds else None,
                      mean_off_util_percent=off_util_seconds / off_seconds * 100.0,
                      node_off_fraction_percent=off_node_seconds / (n_nodes * off_seconds) * 100.0,
                      node_overrun_node_hours_total=overrun,
                      node_overrun_node_hours_per_window=overrun_per_window,
                      node_overrun_normalized_hours_per_window=overrun_per_window / target_off_nodes)
    return result


def evaluate_run(run: Path, mode: str) -> dict:
    row = {"run_id": run.name, "run_dir": str(run), "mode": mode,
           "analysis_status": "error", "analysis_reason": ""}
    try:
        metadata = read_json(run / "run.json")
        for key in ("cluster", "scheduler", "seed", "malleable_share", "exit_code", "expected_jobs"):
            row[key] = metadata.get(key)
        row["run_status"] = metadata.get("status")
        if metadata.get("status") != "completed" or metadata.get("exit_code") != 0:
            raise ValueError("run_not_completed: run.json must record completed with exit_code 0")
        expected = metadata["expected_jobs"]
        if isinstance(expected, bool) or not isinstance(expected, int) or expected <= 0:
            raise ValueError("run.json: expected_jobs must be a positive integer")
        config = read_json(run / "config.json")
        if config.get("sensing") is False:
            raise ValueError("Sensing was disabled; energy and shutdown metrics are unavailable")
        energy, evaluation = load_parameters(run)
        row.update({key: value for key, value in evaluation.items() if key != "wait_threshold_s"})
        policy = read_json(run / "input" / "policy.json")
        row["policy"] = policy.get("policy_name", metadata.get("policy", ""))
        jobs = read_jobs(run / "job_statistics.csv")
        row.update(jobs_seen=len(jobs), jobs_completed=sum(j["Status"] == "completed" for j in jobs),
                   jobs_killed=sum(j["Status"] == "killed" for j in jobs))
        if len(jobs) != expected:
            raise ValueError(f"job_count_mismatch: expected {expected}, found {len(jobs)}")
        row.update(job_metrics(jobs, mode, evaluation["wait_threshold_s"]))
        cpu_path = run / "cpu_utilization.csv"
        node_names = cpu_header(cpu_path)
        row["num_nodes"] = len(node_names)
        row.update(cpu_coverage(cpu_path, len(node_names)))
        requested_start, requested_end = row["job_window_start_s"], row["job_window_end_s"]
        start = max(requested_start, row["cpu_first_sample_s"])
        end = min(requested_end, row["cpu_last_sample_s"])
        duration = max(0.0, end - start)
        requested_duration = requested_end - requested_start
        row.update(energy_window_start_s=start, energy_window_end_s=max(start, end),
                   energy_duration_s=duration,
                   energy_coverage_fraction=duration / requested_duration if requested_duration > 0 else None,
                   unobserved_head_s=min(max(0.0, start - requested_start), requested_duration),
                   unobserved_tail_s=min(max(0.0, requested_end - end), requested_duration))
        if duration <= 0 or row["cpu_samples"] < 2:
            row.update(analysis_status="partial", analysis_reason="insufficient_cpu_coverage",
                       energy_status="unavailable", shutdown_status="not_available",
                       shutdown_reason="insufficient_cpu_coverage", mean_on_util_status="not_available")
            # Even with insufficient samples, malformed node traces are errors.
            for _event in iter_node_events(run / "node_utilization.csv", node_names):
                pass
            return row
        if mode == "paper" and (start > requested_start or end < requested_end):
            row.update(analysis_status="partial", analysis_reason="paper_window_not_fully_sampled",
                       energy_status="unavailable", shutdown_status="not_available",
                       shutdown_reason="paper_window_not_fully_sampled", mean_on_util_status="not_available")
            return row
        row.update(sampled_metrics(run, node_names, energy, evaluation, start, end, policy))
        row.update(analysis_status="ok", energy_status="complete" if duration == requested_duration else "observed_only")
    except (OSError, ValueError, KeyError, TypeError, csv.Error, OverflowError) as error:
        row.update(analysis_status="error", analysis_reason=str(error))
    return row


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runs", metavar="RUN", nargs="+", type=Path, help="Explicit completed run directories")
    parser.add_argument("--output", required=True, type=Path, help="Write one CSV row per requested run")
    parser.add_argument("--mode", choices=("paper", "observed"), required=True,
                        help="paper: dynamic submit window; observed: all jobs and measured energy coverage")
    args = parser.parse_args()
    runs = [path.resolve() for path in args.runs]
    output = args.output.resolve()
    if len(set(runs)) != len(runs):
        parser.error("Each run directory must be listed only once")
    # A typo in --output must not destroy any simulation input or raw output.
    protected_names = {
        "run.json", "config.json", "job_statistics.csv", "cpu_utilization.csv",
        "node_utilization.csv", "event.csv", "simulation.log", "network_activity.csv",
        "pfs_utilization.csv", "gpu_utilization.csv", "task_times.csv",
        "performance_trace.csv", "performance_component_trace.csv",
    }
    if any(output in {run / name for name in protected_names} or (run / "input") in output.parents for run in runs):
        parser.error("--output must not overwrite run metadata, raw traces, or the input directory")
    failed = False
    try:
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open("w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=FIELDS, extrasaction="ignore", lineterminator="\n")
            writer.writeheader()
            for run in runs:
                row = evaluate_run(run, args.mode)
                writer.writerow(row)
                stream.flush()
                if row["analysis_status"] != "ok":
                    failed = True
                    print(f"{run.name}: {row['analysis_status']}: {row['analysis_reason']}", file=sys.stderr)
    except OSError as error:
        print(f"Cannot write evaluation CSV: {error}", file=sys.stderr)
        return 1
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
