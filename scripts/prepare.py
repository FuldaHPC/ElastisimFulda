#!/usr/bin/env python3
# Copyright (c) 2026 Fulda University of Applied Sciences.
# SPDX-License-Identifier: BSD-3-Clause
"""Prepare self-contained ElastiSim inputs from existing Cori/Theta CSVs.

The retained converter runs in a fresh process for each jobset. This command
does not run simulations or download traces. Outputs must not already exist.
"""

import argparse
import csv
import datetime as dt
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


SCRIPT_DIR = Path(__file__).resolve().parent
PRODUCTION_CLUSTERS = ("cori_haswell", "cori_knl", "theta")
MATRIX_SEEDS = ("S0", "S3", "S7")
MATRIX_SHARES = (0, 25, 50, 75, 100)
COLUMN_NAMES = {
    "id": ("COBALT_JOBID", "Job", "job_id"),
    "submit_time": ("QUEUED_TIMESTAMP", "Submit", "submit_time"),
    "num_nodes": ("NODES_USED", "Nodes Allocated", "num_nodes"),
    "runtime": ("RUNTIME_SECONDS", "Elapsed Secs", "runtime_seconds"),
    "timelimit": ("WALLTIME_SECONDS", "timelimit"),
}


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8")


def validate_csv(path, profile):
    """Validate before creating any output; preserve converter column precedence."""
    if not path.is_file():
        raise ValueError(f"CSV not found: {path}")
    start, end = [
        dt.datetime.strptime(value, "%Y-%m-%d_%H:%M:%S")
        for value in profile["parameters"]["time_window"].split(",")
    ]
    if start >= end:
        raise ValueError("The profile time window must have positive duration")
    selected = 0
    with path.open(newline="", encoding="utf-8-sig") as handle:
        reader = csv.DictReader(handle)
        if not reader.fieldnames or len(set(reader.fieldnames)) != len(reader.fieldnames):
            raise ValueError(f"CSV has missing or duplicate column headers: {path}")
        columns = {}
        for key, aliases in COLUMN_NAMES.items():
            for alias in aliases:
                if alias in reader.fieldnames:
                    columns[key] = alias
        missing = set(COLUMN_NAMES) - {"timelimit"} - columns.keys()
        if missing:
            raise ValueError(f"CSV missing required columns for {', '.join(sorted(missing))}: {path}")
        for line, row in enumerate(reader, 2):
            try:
                if None in row or any(value is None for value in row.values()):
                    raise ValueError("row length differs from the header")
                # Source IDs may repeat (e.g. Cori trace records). Keep every row:
                # the historical converter assigns separate simulator IDs later.
                int(row[columns["id"]])
                submit = dt.datetime.strptime(row[columns["submit_time"]], "%Y-%m-%d %H:%M:%S")
                if not start <= submit <= end:
                    continue
                for key in ("num_nodes", "runtime", "timelimit"):
                    if key in columns:
                        value = float(row[columns[key]])
                        if not math.isfinite(value) or int(value) <= 0:
                            raise ValueError(f"{key} must convert to a finite positive integer")
                selected += 1
            except (ValueError, TypeError) as exc:
                raise ValueError(f"Invalid CSV row {line} in {path}: {exc}") from exc
    if selected == 0:
        raise ValueError(f"CSV contains no jobs in profile window {start} through {end}: {path}")
    return selected


def make_bundle(directory, cluster, source, seed, share, profile, expected_jobs):
    directory.mkdir()
    parameters = dict(profile["parameters"])
    parameters.update(seed=seed, type_probabilities=f"{100 - share},0,{share}")
    converter = SCRIPT_DIR / "csv_to_jobs.py"
    command = [sys.executable, str(converter), "-q", "-d", str(directory), "-p", str(source)]
    for key, value in parameters.items():
        command.extend((f"--{key}", str(value)))
    # The converter has module-level caches and globals: do not import/reuse it.
    completed = subprocess.run(command, capture_output=True, text=True, timeout=300)
    if completed.returncode:
        detail = (completed.stderr or completed.stdout).strip()
        raise ValueError(f"CSV conversion failed for {cluster}/{seed}/mal{share}: {detail}")

    jobs = json.loads((directory / "jobs.json").read_text(encoding="utf-8"))
    if jobs.get("jobs_generated") != expected_jobs or len(jobs.get("jobs", [])) != expected_jobs:
        raise ValueError("Converter job count disagrees with the validated CSV")
    for job in jobs["jobs"]:
        if job.get("application_model") != "application_model.json":
            raise ValueError("Converter returned an application model outside the input bundle")
        attributes = job.get("attributes", {})
        if attributes.get("predicted_runtime", 0) <= 0 or attributes.get("prediction_base_nodes", 0) <= 0:
            raise ValueError("Converter returned invalid native runtime-prediction attributes")

    templates = SCRIPT_DIR / "templates" / cluster
    shutil.copyfile(templates / "crossbar.xml", directory / "crossbar.xml")
    shutil.copyfile(templates / "energy.json", directory / "energy.json")
    # Retain the effective historical main template instead of the converter default.
    (directory / "configuration.json").unlink()
    (directory / "policies").mkdir()
    (directory / "main_config").mkdir()
    template_main = json.loads((templates / "main.json").read_text(encoding="utf-8"))
    output_fields = (
        "job_statistics", "cpu_utilization", "node_utilization", "network_activity",
        "pfs_utilization", "gpu_utilization", "event_log",
    )
    for policy in sorted((templates / "policies").glob("*.json")):
        shutil.copyfile(policy, directory / "policies" / policy.name)
        main = dict(template_main)
        main.update(jobs_file="../jobs.json", platform_file="../crossbar.xml",
                    shutdown_policy_file=f"../policies/{policy.name}")
        for field in output_fields:
            if field in main:
                main[field] = Path(main[field]).name
        write_json(directory / "main_config" / policy.name, main)
    write_json(directory / "evaluation.json", profile["evaluation"])
    provenance = {
        "cluster": cluster,
        "seed": seed,
        "malleable_share": share,
        "input_filename": source.name,
        "input_sha256": sha256(source),
        "parameters": parameters,
        "job_count": expected_jobs,
        "generator": "csv_to_jobs.py",
        "generator_sha256": sha256(converter),
        "python_version": sys.version.split()[0],
        "main_scheduling_interval": template_main["scheduling_interval"],
        "main_min_scheduling_interval": template_main["min_scheduling_interval"],
        "time_origin": "earliest selected submission is shifted to zero by the retained converter",
        "template_sha256": {
            str(path.relative_to(templates)): sha256(path)
            for path in sorted(templates.rglob("*")) if path.is_file()
        },
    }
    write_json(directory / "provenance.json", provenance)
    if cluster == "small":
        shutil.copyfile(source, directory / "source.csv")


def parser():
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--cluster", choices=(*PRODUCTION_CLUSTERS, "small"))
    result.add_argument("--csv", type=Path, help="existing Cori-clean/Theta CSV, or the synthetic small CSV")
    result.add_argument("--seed", help="seed label, e.g. S0 (single-bundle default: S0)")
    result.add_argument("--malleable-share", type=int, help="malleable probability in percent (default: 50)")
    result.add_argument("--output", type=Path, required=True, help="new bundle directory, or matrix parent directory")
    result.add_argument("--matrix", action="store_true", help="prepare 45 jobsets; never runs simulations")
    result.add_argument("--data-dir", type=Path, help="matrix CSV root with cori_haswell/, cori_knl/, theta/")
    return result


def main(argv=None):
    cli = parser()
    args = cli.parse_args(argv)
    profiles = json.loads((SCRIPT_DIR / "profiles.json").read_text(encoding="utf-8"))
    # lexists also rejects dangling links, which resolve()/exists() alone would miss.
    output = Path(os.path.abspath(args.output.expanduser()))
    if os.path.lexists(output):
        cli.error(f"Output already exists; choose a new directory: {output}")
    plans = []
    if args.matrix:
        if not args.data_dir or any(value is not None for value in
                                    (args.cluster, args.csv, args.seed, args.malleable_share)):
            cli.error("--matrix requires --data-dir and cannot be combined with single-bundle options")
        for cluster in PRODUCTION_CLUSTERS:
            profile = profiles[cluster]
            source = (args.data_dir / cluster / profile["input_filename"]).expanduser().resolve()
            for seed in MATRIX_SEEDS:
                for share in MATRIX_SHARES:
                    plans.append((cluster, source, seed, share, profile))
    else:
        if not args.cluster or not args.csv or args.data_dir:
            cli.error("Single-bundle preparation requires --cluster and --csv; --data-dir is for --matrix")
        seed = args.seed or "S0"
        share = 50 if args.malleable_share is None else args.malleable_share
        if not re.fullmatch(r"S[0-9]+", seed):
            cli.error("--seed must be a seed label such as S0, S3 or S7")
        if not 0 <= share <= 100:
            cli.error("--malleable-share must be between 0 and 100")
        plans.append((args.cluster, args.csv.expanduser().resolve(), seed, share, profiles[args.cluster]))

    counts = {}
    try:
        # Validate all three sources before creating any matrix output.
        for cluster, source, _, _, profile in plans:
            if cluster not in counts:
                counts[cluster] = validate_csv(source, profile)
        output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix=".prepare-", dir=output.parent) as temporary:
            staged = Path(temporary) / "bundle"
            if args.matrix:
                staged.mkdir()
            for cluster, source, seed, share, profile in plans:
                destination = staged / f"{cluster}_mal{share}_{seed}" if args.matrix else staged
                make_bundle(destination, cluster, source, seed, share, profile, counts[cluster])
            if os.path.lexists(output):
                raise ValueError(f"Output appeared while preparing; refusing to replace it: {output}")
            staged.rename(output)
    except (ValueError, OSError, subprocess.TimeoutExpired) as exc:
        print(f"Preparation failed: {exc}", file=sys.stderr)
        return 1
    print(f"Prepared {len(plans)} input bundle(s): {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
