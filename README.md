# Scheduler-Integrated Recurring Node Shutdown in ElastiSim

This repository contains the simulator, the scripts and the aggregated results of the SC26 EESP workshop paper
*Saving Energy Costs and Time on Supercomputers: Scheduler-Integrated Recurring Node Shutdown Exploiting Malleable Jobs*.
It extends [ElastiSim](https://github.com/elastisim/elastisim) with node power states, recurring shutdown policies, dynamic runtime prediction and a native C++ scheduler interface, on which the scheduling strategies of [MalleableJobScheduling](https://github.com/ProjectWagomu/MalleableJobScheduling) are re-implemented (see [Acknowledgement](#acknowledgement)).

## Dependencies

- C++17 compiler, CMake 3.16 or newer, [SimGrid 4.1](https://framagit.org/simgrid/simgrid/-/archive/v4.1/simgrid-v4.1.tar.gz) and the Boost headers SimGrid needs
- Python 3.10 or newer, standard library only
- gnuplot 6, `epstopdf` and `pdflatex` for the figures

Tested with GCC 13.3, CMake 3.28, SimGrid 4.1, Boost 1.83, Python 3.12 and gnuplot 6.0 on Linux. The paper runs were built on Otus with GCC 14.3, Boost 1.88 and `-DELASTISIM_NATIVE_MARCH=ON`, which optimizes for the CPU of the build machine.

## Build

Run all commands in this README from the repository root.
Build SimGrid 4.1 into an install prefix, then the simulator:

```sh
cmake -S /path/to/simgrid-v4.1 -B /path/to/simgrid-build \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/path/to/simgrid-install
cmake --build /path/to/simgrid-build --parallel 2 --target install
cmake -S elastisim -B elastisim/build \
  -DCMAKE_BUILD_TYPE=Release -DELASTISIM_NATIVE_MARCH=OFF \
  -DSIMGRID_ROOT=/path/to/simgrid-install
cmake --build elastisim/build --parallel 2
```

If the Boost headers are not in the compiler's search path, add `-DBOOST_ROOT=/path/to/boost` to the third command.

## Small example

Four jobs on eight nodes, two of them malleable, once without and once with a shutdown policy that switches two nodes off during the first simulated hour. This takes seconds and needs no trace data.

```sh
bash scripts/run_sim.sh input_files/small/main_config/baseline.json \
  --scheduler min_steal_agreement --output output_files/small-baseline
bash scripts/run_sim.sh input_files/small/main_config/shutdown.json \
  --scheduler min_steal_agreement --output output_files/small-shutdown
python3 scripts/evaluate.py output_files/small-baseline output_files/small-shutdown \
  --mode observed --output output_files/small-metrics.csv
```

Each run directory contains the job statistics, `event.csv` with the shrink, expand and agreement events, `node_utilization.csv` with the node states and a snapshot of the inputs.
`--scheduler rigid_easy_backfill` selects EASY backfilling (EBF), `min_steal_agreement` is MSA. Every output directory must be new. `ELASTISIM_BIN` points the runner to a simulator binary built elsewhere.
`--mode observed` evaluates all completed jobs of a run. `--mode paper` uses the paper's evaluation window.

## Reproducing the paper

### Job traces

The traces are not part of this repository. The paper uses

- Theta (ALCF): dataset [ANL-ALCF-DJC-THETA_20230101_20231231](https://reports.alcf.anl.gov/data/ANL-ALCF-DJC-THETA_20230101_20231231.html), jobs submitted from 12 to 25 January 2023.
- Cori Haswell and Cori KNL (NERSC): job logs of November 2022, cleaned as described by Zojer et al. (arXiv:2602.17318). The paper uses the jobs submitted from 7 to 11 November 2022 on Haswell and from 6 to 11 November 2022 on KNL.

Place the CSV files at

```text
data/theta/ANL-ALCF-DJC-THETA_20230101_20231231.csv
data/cori_haswell/11-07-22_to_11-11-22_clean.csv
data/cori_knl/11-07-22_to_11-11-22_clean.csv
```

The generated job sets are byte-identical to the paper's when the files match these SHA-256 sums:

```text
154705e28c81a04c55f90b906b7274ac4e3554202efa258f85d088cba10ce5c1  theta
3161af144c2e6a0ae1b56412624fdb7ec6125264e3292d49e1bd455ec3b982fa  cori_haswell
e14b8ea23ccd3b5c7286cc0e88533f0e52fd323c2fa8fbbf74f2a327793dbe7d  cori_knl
```

### Job sets, simulations, evaluation, figures

```sh
#Generate the 45 configurations of the simulations
python3 scripts/prepare.py --matrix --data-dir data --output input_files/matrix

#Start the 45 configurations with each policy (F0, F10, F30) and EBF, 135 simulations
bash scripts/run_sim.sh input_files/matrix/*/main_config/*.json \
  --scheduler rigid_easy_backfill --output output_files/matrix-easy

#Start the 45 configurations with each policy (F0, F10, F30) and MSA, 135 simulations
bash scripts/run_sim.sh input_files/matrix/*/main_config/*.json \
  --scheduler min_steal_agreement --output output_files/matrix-msa

#When all simulations have finished, compute the metrics of every run
python3 scripts/evaluate.py output_files/matrix-easy/* output_files/matrix-msa/* \
  --mode paper --output output_files/matrix-metrics.csv

#Collect the metrics into the tables of analysis/data
python3 analysis/collect_metrics.py output_files/matrix-metrics.csv \
  --out-dir output_files/analysis-data

#Compare with the EBF F0 baseline and draw Figures 3 to 5
bash analysis/figures/make_figures.sh --data-dir output_files/analysis-data \
  --out-dir output_files/analysis --partial
```

The 45 configurations are three systems, malleable shares 0, 25, 50, 75 and 100 % and seeds S0, S3 and S7. F0 is no shutdown, F10 and F30 switch 10 % and 30 % of the nodes off from 22:00 to 06:00. The parameters are in `scripts/profiles.json` and `scripts/templates/`.

One simulation needs one CPU core, 0.5 to 2.5 GB of memory and 0.3 to 2.3 GB of disk space, the full matrix about 310 GB. A Theta simulation takes minutes to hours, a KNL simulation with all jobs malleable more than a week. On two nodes of [Otus](https://pc2.uni-paderborn.de/systems-and-services/otus) with 192 cores each, the 270 simulations in parallel took about three weeks. On a cluster, start the simulations as separate batch tasks, one runner call per simulation.

`evaluate.py` writes one row per simulation with the metrics of the paper (waiting time, runtime, turnaround time, energy, cost, shutdown measurements). `make_figures.sh` compares every run with the EBF F0 run at 0 % malleable jobs of the same system and seed and draws Figures 3 to 5, `--partial` accepts an incomplete matrix.
Built as on Otus (GCC 14.3, `-march=native`, SimGrid 4.1), the simulator reproduces the paper's simulations exactly. Other compilers or CPUs can give small floating-point differences.

## Figures from the included results

`analysis/data/` contains the evaluated metrics of all 270 paper runs. This command regenerates the three result figures and the aggregated tables in `analysis/` and compares them with the included reference tables:

```sh
bash analysis/figures/make_figures.sh
```

The `.pdf` files hold the graphics and the `.tex` files the labels, as the paper includes them (gnuplot epslatex). With `pdflatex` installed the script also writes `<name>_preview.pdf` with the labels.

## Layout

```text
elastisim/            simulator, ElastiSim fork with native schedulers, shutdown and prediction
scripts/              prepare.py, csv_to_jobs.py, run_sim.sh, evaluate.py, profiles.json, templates/
input_files/small/    synthetic example
analysis/             collect_metrics.py, aggregate.py, figure scripts, data/ with the paper's metrics
data/, output_files/  traces and simulation outputs, ignored by git
```

## Acknowledgement

This repository builds on [ElastiSim](https://github.com/elastisim/elastisim) by Taylan Özden (Technical University of Darmstadt).
The scheduling strategies and the agreement handlers are re-implemented in C++ from the Python strategies of [MalleableJobScheduling](https://github.com/ProjectWagomu/MalleableJobScheduling) (Wagomu project), which used ElastiSim's Python interface. The job set generator `scripts/csv_to_jobs.py` is adapted from its input generators.

## License

- The ElastiSim core in `elastisim/` is BSD 3-Clause, Copyright (c) 2022 Technical University of Darmstadt ([elastisim/LICENSE](elastisim/LICENSE)). Modified upstream files say so in their header.
- The scheduling strategies in `elastisim/src/scheduling/schedulers/` (every file except `SchedulerRegistration.cpp`), everything in `elastisim/src/scheduling/malleability/` and `scripts/csv_to_jobs.py`, all adapted from MalleableJobScheduling, are Eclipse Public License 2.0, Copyright (c) 2023 Wagomu project and Copyright (c) 2026 Fulda University of Applied Sciences ([LICENSE.EPL-2.0](LICENSE.EPL-2.0)).
- All other files are BSD 3-Clause, Copyright (c) 2026 Fulda University of Applied Sciences ([LICENSE](LICENSE)).
- The bundled headers in `elastisim/third-party/` are MIT. SimGrid (LGPL-2.1) is not included.

Every source file names its license in its header.

## Publication

Paul Schäfer, Anna-Lena Roth, Peter Arzt, Felix Krull, Sebastian Krenz, Felix Wolf, and Jonas Posner.
*Saving Energy Costs and Time on Supercomputers: Scheduler-Integrated Recurring Node Shutdown Exploiting Malleable Jobs*.
In Workshops of the International Conference for High Performance Computing, Networking, Storage, and Analysis (SC26), 3rd International Workshop on Energy Efficiency with Sustainable Performance: Techniques, Tools, and Best Practices (EESP), 2026. To appear.
