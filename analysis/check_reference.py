#!/usr/bin/env python3
# Copyright (c) 2026 Fulda University of Applied Sciences.
# SPDX-License-Identifier: BSD-3-Clause
"""Compare regenerated tables with the reference copies shipped in analysis/data.

Compares run_deltas.csv, main_matrix.csv and job_bar_metrics.csv in the
results directory with the same files in the data directory. Numeric cells
are compared with a tolerance, all other cells literally. Reports the largest
absolute difference per file and exits 1 if any cell differs.

Usage: python3 check_reference.py [--results-dir R] [--data-dir D] [FILE ...]
"""

from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "lib"))
import main_matrix as mm  # noqa: E402

DEFAULT_FILES = ("run_deltas.csv", "main_matrix.csv", "job_bar_metrics.csv")


def load(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    with path.open(newline="") as f:
        reader = csv.DictReader(f)
        return list(reader.fieldnames or []), list(reader)


def compare(reference: Path, candidate: Path, tolerance: float) -> tuple[bool, float, str]:
    ref_fields, ref_rows = load(reference)
    new_fields, new_rows = load(candidate)
    if ref_fields != new_fields:
        return False, float("inf"), f"column mismatch: {ref_fields} != {new_fields}"
    if len(ref_rows) != len(new_rows):
        return False, float("inf"), f"row count {len(new_rows)} != reference {len(ref_rows)}"
    max_diff = 0.0
    mismatches = 0
    for idx, (a, b) in enumerate(zip(ref_rows, new_rows), start=2):
        for column in ref_fields:
            fa, fb = mm.maybe_float(a[column]), mm.maybe_float(b[column])
            if fa is not None and fb is not None:
                diff = abs(fa - fb)
                max_diff = max(max_diff, diff)
                if diff > tolerance:
                    mismatches += 1
            elif a[column] != b[column]:
                mismatches += 1
                max_diff = float("inf")
    return mismatches == 0, max_diff, f"{mismatches} differing cells"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("files", nargs="*", default=list(DEFAULT_FILES), help="File names to compare")
    parser.add_argument("--results-dir", type=Path, default=mm.DEFAULT_RESULTS_DIR)
    parser.add_argument("--data-dir", type=Path, default=mm.DEFAULT_DATA_DIR)
    parser.add_argument("--tolerance", type=float, default=1e-9, help="Absolute tolerance for numeric cells")
    args = parser.parse_args()
    failed = False
    for name in args.files:
        reference = args.data_dir / name
        candidate = args.results_dir / name
        if not reference.exists() or not candidate.exists():
            print(f"{name}: missing ({reference if not reference.exists() else candidate})")
            failed = True
            continue
        ok, max_diff, detail = compare(reference, candidate, args.tolerance)
        print(f"{name}: {'identical' if ok else 'DIFFERENT'} (max abs difference {max_diff:.3g}, {detail})")
        failed |= not ok
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
