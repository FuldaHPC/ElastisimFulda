#!/usr/bin/env bash
# Copyright (c) 2026 Fulda University of Applied Sciences.
# SPDX-License-Identifier: BSD-3-Clause
#
# Regenerate the paper's three result figures and all aggregated tables from
# the per-run metrics in analysis/data (default) or from metrics collected
# with collect_metrics.py.
#
#   make_figures.sh [--data-dir DIR] [--out-dir DIR] [--partial] [--no-check]
#
# --data-dir DIR  directory with run_metrics.csv and shutdown_run_metrics.csv;
#                 default analysis/data
# --out-dir DIR   where results/ and figures/ are written; default analysis/
#                 (a different directory receives a copy of the Gnuplot files)
# --partial       accept an incomplete 270-run matrix (missing cells are skipped)
# --no-check      do not compare the regenerated tables with analysis/data
#
# Produces <out>/figures/op_scatter_all_clusters.pdf,
# job_bar_charts_all_clusters.pdf and shutdown_policy_matrices_by_cluster.pdf
# (the epstopdf graphics included by the .tex files, as in the paper build)
# and, if pdflatex is available, *_preview.pdf with the LaTeX labels.
# Requires python3, gnuplot and epstopdf.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
analysis="$(cd "$here/.." && pwd)"
data_dir="$analysis/data"
out_dir="$analysis"
partial=()
check=1

while [ $# -gt 0 ]; do
  case "$1" in
    --data-dir) data_dir="$(cd "$2" && pwd)"; shift 2 ;;
    --out-dir) mkdir -p "$2"; out_dir="$(cd "$2" && pwd)"; shift 2 ;;
    --partial) partial=(--partial); shift ;;
    --no-check) check=0; shift ;;
    -h|--help) sed -n '5,22p' "$0"; exit 0 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

for tool in python3 gnuplot epstopdf; do
  command -v "$tool" >/dev/null 2>&1 || { echo "missing required tool: $tool" >&2; exit 1; }
done
for file in run_metrics.csv shutdown_run_metrics.csv; do
  [ -f "$data_dir/$file" ] || { echo "missing $data_dir/$file" >&2; exit 1; }
done

results="$out_dir/results"
figures="$out_dir/figures"
figure_data="$figures/data"
mkdir -p "$results" "$figure_data"
if [ "$figures" != "$here" ]; then
  cp "$here"/*.gp "$figures/"
fi

echo "== aggregating $data_dir -> $results"
python3 "$analysis/aggregate.py" --data-dir "$data_dir" --results-dir "$results" "${partial[@]}"
python3 "$analysis/job_bar_charts.py" --data-dir "$data_dir" --results-dir "$results" --figure-data-dir "$figure_data" "${partial[@]}"
python3 "$analysis/shutdown_matrices.py" --data-dir "$data_dir" --results-dir "$results" --figure-data-dir "$figure_data" "${partial[@]}"
python3 "$analysis/generate_op_scatter_data.py" --run-deltas "$results/run_deltas.csv" --figure-data-dir "$figure_data" "${partial[@]}"
python3 "$analysis/generate_turnaround_range_data.py" --data-dir "$data_dir" --figure-data-dir "$figure_data" "${partial[@]}"

echo "== rendering figures in $figures"
cd "$figures"
for name in op_scatter_all_clusters job_bar_charts_all_clusters shutdown_policy_matrices_by_cluster; do
  if ! gnuplot "${name}_epslatex.gp"; then
    if [ ${#partial[@]} -gt 0 ]; then
      echo "warning: gnuplot failed for ${name}; figure skipped (no data for at least one system)" >&2
      continue
    fi
    exit 1
  fi
  epstopdf "${name}.eps"
  echo "wrote $figures/${name}.pdf"
  if command -v pdflatex >/dev/null 2>&1; then
    printf '%s\n' '\documentclass[preview,border={48pt 4pt 4pt 6pt}]{standalone}' '\usepackage{graphicx,xcolor}' \
      '\begin{document}' "\\input{${name}.tex}" '\end{document}' > "${name}_preview.tex"
    if pdflatex -interaction=batchmode -halt-on-error "${name}_preview.tex" >/dev/null 2>&1; then
      echo "wrote $figures/${name}_preview.pdf"
      rm -f "${name}_preview.log"
    else
      echo "pdflatex failed for ${name}_preview.tex (see ${name}_preview.log)" >&2
    fi
    rm -f "${name}_preview.tex" "${name}_preview.aux"
  fi
done

if [ "$check" -eq 1 ] && [ ${#partial[@]} -eq 0 ] && [ "$data_dir" = "$analysis/data" ]; then
  echo "== comparing regenerated tables with the reference copies in analysis/data"
  python3 "$analysis/check_reference.py" --results-dir "$results" --data-dir "$data_dir"
fi
