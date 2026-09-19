#!/usr/bin/env bash
# Copyright (c) 2026 Fulda University of Applied Sciences.
# SPDX-License-Identifier: BSD-3-Clause
# Run prepared native ElastiSim cases, serially, in isolated output directories.
set -uo pipefail

if (( BASH_VERSINFO[0] < 4 )); then
    echo "Error: Bash 4 or newer is required." >&2
    exit 2
fi
ROOT_PATH="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)" || exit 1

usage() {
    cat <<'HELP'
Usage: run_sim.sh CONFIG [CONFIG ...] --scheduler NAME --output NEW_DIRECTORY

Runs prepared cases serially; stops at the first failure. For multiple configs,
--output is a new parent directory containing one directory per run.
ELASTISIM_BIN overrides the default elastisim/build/elastisim binary.
Use an external timeout if a wall-clock limit is needed.
Requires Bash 4+, Python 3 for JSON staging, and standard GNU command-line tools.

Schedulers: rigid_fifo, rigid_backfill, rigid_easy_backfill,
rigid_shortest_job_first, min_agreement, min_common_pool, average_common_pool,
min_steal_agreement, pref_steal_agreement.
HELP
}

fail() { printf 'Error: %s\n' "$*" >&2; exit 1; }
configs=()
targets=()
scheduler=""
output_arg=""
while (( $# )); do
    case "$1" in
        -h|--help) usage; exit 0 ;;
        --scheduler|--output)
            (( $# >= 2 )) || fail "Missing value for $1"
            if [[ "$1" == --scheduler ]]; then scheduler=$2; else output_arg=$2; fi
            shift 2 ;;
        --) shift; configs+=("$@"); break ;;
        -*) fail "Unknown option: $1" ;;
        *) configs+=("$1"); shift ;;
    esac
done
(( ${#configs[@]} )) || { usage >&2; exit 2; }
[[ -n "$output_arg" && -n "$scheduler" ]] || fail "--output and --scheduler are required"
case "$scheduler" in
    rigid_fifo|rigid_backfill|rigid_easy_backfill|rigid_shortest_job_first|\
    min_agreement|min_common_pool|average_common_pool|min_steal_agreement|pref_steal_agreement) ;;
    *) fail "Unknown scheduler: $scheduler" ;;
esac
command -v python3 >/dev/null || fail "Python 3 is required for JSON staging"
binary="$(realpath -e -- "${ELASTISIM_BIN:-$ROOT_PATH/elastisim/build/elastisim}")" ||
    fail "Simulator binary not found; set ELASTISIM_BIN"
[[ -f "$binary" && -x "$binary" ]] || fail "Simulator is not executable: $binary"
output="$(realpath -m -- "$output_arg")" || fail "Cannot resolve output path"
[[ ! -e "$output" && ! -L "$output" ]] || fail "Output already exists: $output"

# Python handles JSON and input snapshots only. Process control stays in Bash.
prepare_json() {
    python3 - "$1" "$2" "$3" "$binary" "$scheduler" <<'PY'
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import shutil
import sys

mode, config_arg, output_arg, binary_arg, scheduler = sys.argv[1:]
config_path, output, binary = map(Path, (config_arg, output_arg, binary_arg))

def read(path):
    with path.open(encoding="utf-8") as stream:
        value = json.load(stream)
    if not isinstance(value, dict):
        raise ValueError(f"Expected a JSON object in {path}")
    return value

def write(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")

def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()

try:
    config = read(config_path)
    sources = {}
    fields = (("jobs_file", "jobs.json"), ("platform_file", "crossbar.xml"),
              ("shutdown_policy_file", "policy.json"))
    for key, name in fields:
        sources[name] = (config_path.parent / config[key]).resolve(strict=True)
    bundle = sources["jobs.json"].parent
    if output == bundle or bundle in output.parents:
        raise ValueError("Output must be outside the source input bundle")
    if not config.get("sensing", False):
        raise ValueError("This workflow requires sensing for energy evaluation")
    for name in ("energy.json", "evaluation.json", "provenance.json"):
        sources[name] = (bundle / name).resolve(strict=True)
    jobs = read(sources["jobs.json"])
    if not isinstance(jobs.get("jobs"), list) or not jobs["jobs"]:
        raise ValueError("The prepared bundle contains no jobs")
    models = {(bundle / job["application_model"]).resolve(strict=True) for job in jobs["jobs"]}
    if len(models) != 1:
        raise ValueError("Expected one shared application model per prepared bundle")
    sources["application_model.json"] = models.pop()
    provenance = read(sources["provenance.json"])
    for name in ("energy.json", "evaluation.json", "policy.json", "application_model.json"):
        read(sources[name])
    if mode == "check":
        sys.exit(0)
    for name, source in sources.items():
        shutil.copyfile(source, output / "input" / name)
    for job in jobs["jobs"]:
        job["application_model"] = "input/application_model.json"
    write(output / "input/jobs.json", jobs)
    for key, name in fields:
        config[key] = "input/" + name
    output_fields = ("job_statistics", "cpu_utilization", "node_utilization",
                     "network_activity", "pfs_utilization", "gpu_utilization", "event_log",
                     "task_times", "performance_trace_file", "performance_component_trace_file")
    for key in output_fields:
        if key in config:
            config[key] = Path(config[key]).name
    for key in ("job_statistics", "cpu_utilization", "node_utilization"):
        config[key] = key + ".csv"
    config["native_scheduler_name"] = scheduler
    # Keep the simulator's progress bar out of simulation.log.
    config["show_progress_bar"] = False
    write(output / "config.json", config)
    write(output / "run.json", {
        "status": "running", "exit_code": None,
        "started_at": datetime.now(timezone.utc).isoformat(),
        "scheduler": scheduler, "expected_jobs": len(jobs["jobs"]),
        "cluster": provenance.get("cluster"), "seed": provenance.get("seed"),
        "malleable_share": provenance.get("malleable_share"),
        "policy": config_path.stem, "source_config": config_path.name,
        "source_config_sha256": sha256(config_path), "binary_sha256": sha256(binary),
        "enable_scheduling_point_fast_path": config.get("enable_scheduling_point_fast_path", False),
        "command": [binary.name, "config.json", "--native-scheduler", scheduler, "--log=root.thresh:warning"],
    })
except (OSError, ValueError, KeyError, TypeError) as error:
    print(f"Error preparing input: {error}", file=sys.stderr)
    sys.exit(1)
PY
}

update_status() {
    [[ -n "$current_output" && -f "$current_output/run.json" ]] || return 0
    python3 - "$current_output/run.json" "$1" "$2" "${3:-}" <<'PY'
from pathlib import Path
from datetime import datetime, timezone
import json
import sys
path = Path(sys.argv[1])
try:
    value = json.loads(path.read_text(encoding="utf-8"))
    value.update(status=sys.argv[2], exit_code=int(sys.argv[3]),
                 finished_at=datetime.now(timezone.utc).isoformat())
    if sys.argv[4]:
        value["abort_reason"] = sys.argv[4]
    temporary = path.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)
except (OSError, ValueError, TypeError) as error:
    print(f"Error updating run status: {error}", file=sys.stderr)
    sys.exit(1)
PY
}

child_pid=""
current_output=""
stop_child() {
    local signal=$1 attempt
    # jobs also covers a signal arriving just after launch, before $! is saved.
    [[ -n "$child_pid" ]] || child_pid="$(jobs -pr)"
    [[ -n "$child_pid" ]] || return 0
    # Monitor mode gives the simulator and its descendants a separate group.
    kill -s "$signal" -- "-$child_pid" 2>/dev/null || true
    for ((attempt=0; attempt<50; attempt++)); do
        kill -0 -- "-$child_pid" 2>/dev/null || break
        sleep 0.1
    done
    kill -KILL -- "-$child_pid" 2>/dev/null || true
    wait "$child_pid" 2>/dev/null || true
    child_pid=""
}

on_signal() {
    local signal=$1 code=$2
    trap '' INT TERM HUP
    stop_child "$signal"
    update_status aborted "$code" "Received SIG$signal" || true
    printf 'Run aborted by SIG%s; output and log retained: %s\n' "$signal" "$current_output" >&2
    exit "$code"
}

on_exit() {
    local code=$?
    trap - EXIT
    if [[ -n "$child_pid" ]]; then
        trap '' INT TERM HUP
        stop_child TERM
        (( code != 0 )) || code=1
        update_status incomplete "$code" "Runner exited before recording completion" || true
    fi
    exit "$code"
}
trap 'on_signal INT 130' INT
trap 'on_signal TERM 143' TERM
trap 'on_signal HUP 129' HUP
trap on_exit EXIT

# Validate every requested input before creating any output.
for i in "${!configs[@]}"; do
    configs[i]="$(realpath -e -- "${configs[i]}")" || fail "Config not found: ${configs[i]}"
    if (( ${#configs[@]} == 1 )); then
        targets[i]=$output
    else
        config_name="$(basename -- "${configs[i]}")"
        case_name="$(basename -- "$(dirname -- "$(dirname -- "${configs[i]}")")")"
        printf -v label '%03d_%s_%s' "$((i + 1))" "$case_name" "${config_name%.*}"
        targets[i]="$output/$label"
    fi
    prepare_json check "${configs[i]}" "${targets[i]}" || exit 1
done

for i in "${!configs[@]}"; do
    current_output="${targets[i]}"
    mkdir -p -- "$(dirname -- "$current_output")" || fail "Cannot create output parent"
    mkdir -- "$current_output" || fail "Cannot create new output: $current_output"
    mkdir -- "$current_output/input" || exit 1
    prepare_json stage "${configs[i]}" "$current_output" || exit 1
    printf 'Running %s with %s: %s\n' "${configs[i]}" "$scheduler" "$current_output"

    # Actual simulator invocation: no Python wrapper. A separate job group lets
    # traps terminate both the simulator and any descendants without a PID leak.
    set -m
    (
        cd -- "$current_output" || exit 1
        # SimGrid INFO logging would write about 100 GB per production run.
        exec "$binary" config.json --native-scheduler "$scheduler" --log=root.thresh:warning
    ) </dev/null >"$current_output/simulation.log" 2>&1 &
    child_pid=$!
    set +m
    wait "$child_pid"
    result=$?
    # A completed simulator must not leave background descendants behind.
    stop_child TERM
    if (( result != 0 )); then
        update_status failed "$result" || true
        printf 'Simulation failed (%s); see %s/simulation.log\n' "$result" "$current_output" >&2
        exit "$result"
    fi
    update_status completed 0 || exit 1
    printf 'Completed: %s\n' "$current_output"
    current_output=""
done
