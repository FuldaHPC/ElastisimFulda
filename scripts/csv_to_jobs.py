# Copyright (c) 2023 Wagomu project.
# Modifications Copyright (c) 2026 Fulda University of Applied Sciences, Germany.
# Adapted from MalleableJobScheduling scripts/input_generation/jsonGenerator.py
# and jobGenerator.py (randomizer, scaling, job encoding, application model).
# SPDX-License-Identifier: EPL-2.0
# See ../LICENSE.EPL-2.0.

import json
import random
import os
import getopt
import sys
import datetime
import csv
import math


DATE_FORMAT = "%Y-%m-%d %H:%M:%S"
ARGS_DATE_FORMAT = "%Y-%m-%d_%H:%M:%S"

COLUMNNAMES = dict()
COLUMNNAMES["id"] = ["COBALT_JOBID", "Job", "job_id"]
COLUMNNAMES["submit_time"] = ["QUEUED_TIMESTAMP", "Submit", "submit_time"]
COLUMNNAMES["num_nodes"] = ["NODES_USED", "Nodes Allocated", "num_nodes"]
COLUMNNAMES["runtime"] = ["RUNTIME_SECONDS", "Elapsed Secs", "runtime_seconds"]
COLUMNNAMES["timelimit"] = ["WALLTIME_SECONDS", "timelimit"]


def static_vars(**kwargs):
    def decorate(func):
        for k in kwargs:
            setattr(func, k, kwargs[k])
        return func

    return decorate


def write_to_file(path, file_name, json):
    path += "/" + file_name
    f = open(path, "w")
    f.write(json)
    f.close()


class Randomizer:
    def __init__(self, seed):
        self.randomizer = random.Random()
        self.randomizer.seed(seed)

    def get_random_value(self, values):
        if type(values) is range:
            return self.get_value_in_range(values)
        elif type(values) in (list, tuple, set):
            return self.get_value_in_iterable(values)
        else:
            raise TypeError("Unknown Random Type: " + str(type(values)))

    def get_random_value_in_weighted_dict(self, weighted_dict: dict):
        values = list(weighted_dict.keys())
        weights = list(weighted_dict.values())
        return self.randomizer.choices(values, weights, k=1)[0]

    def get_value_in_range(self, range: range):
        return self.randomizer.choice(range)

    def get_value_in_iterable(self, iterable):
        return self.randomizer.choice(iterable)


def sample_prediction_factor(randomizer):
    """
    Samples a prediction factor based on ML prediction error distribution.
    Represents how accurate the runtime prediction is compared to true runtime.
    
    Distribution:
    - 10%: strong underestimation [0.5, 0.7]
    - 15%: slight underestimation [0.7, 0.95]
    - 30%: very good estimation [0.95, 1.05]
    - 25%: slight overestimation [1.05, 1.5]
    - 20%: strong overestimation [1.5, 3.0]
    """
    u = randomizer.randomizer.random()
    
    if u < 0.10:
        # Strong underestimation: R in [0.5, 0.7]
        return randomizer.randomizer.uniform(0.5, 0.7)
    elif u < 0.25:  # 0.10 + 0.15
        # Slight underestimation: R in [0.7, 0.95]
        return randomizer.randomizer.uniform(0.7, 0.95)
    elif u < 0.55:  # 0.10 + 0.15 + 0.30
        # Very good estimation: R in [0.95, 1.05]
        return randomizer.randomizer.uniform(0.95, 1.05)
    elif u < 0.80:  # 0.10 + 0.15 + 0.30 + 0.25
        # Slight overestimation: R in [1.05, 1.5]
        return randomizer.randomizer.uniform(1.05, 1.5)
    else:
        # Strong overestimation: R in [1.5, 3.0]
        return randomizer.randomizer.uniform(1.5, 3.0)


@static_vars(memo_dict=dict())
def get_scaling_factor(formula, num_nodes, parallel_percentage):
    key = f"{num_nodes}|{parallel_percentage}"
    if key not in get_scaling_factor.memo_dict:
        value_map = {"parallel_percentage": parallel_percentage, "num_nodes": num_nodes}
        value = eval(formula, {}, value_map) / num_nodes
        get_scaling_factor.memo_dict[key] = value
    return get_scaling_factor.memo_dict[key]


@static_vars(memo_dict=dict())
def get_parallel_percentage(pref_nodes, job_dict):
    if pref_nodes not in get_parallel_percentage.memo_dict:
	# choose lowest parallel_percentage for pref_nodes == 1 to prevent extrem high max_nodes
        if pref_nodes == 1:
            get_parallel_percentage.memo_dict[pref_nodes] = min(job_dict["parallel_percentage"])
        else:
            pref_threshold = job_dict["pref_node_efficiency_threshold"]
            formula = job_dict["scaling_formula"]
            closest_pp = (None, 1)
            for parallel_percentage in job_dict["parallel_percentage"]:
                factor = get_scaling_factor(formula, pref_nodes, parallel_percentage)
                if factor > pref_threshold and factor - pref_threshold < closest_pp[1]:
                    closest_pp = (parallel_percentage, factor - pref_threshold)
            pp = closest_pp[0] or max(job_dict["parallel_percentage"])
            get_parallel_percentage.memo_dict[pref_nodes] = pp

    return get_parallel_percentage.memo_dict[pref_nodes]


@static_vars(memo_dict=dict())
def get_min_max_nodes(
    pref_nodes, p_percentage, cluster_nodes, job_dict, min_scaling=0.2
):
    key = f"{pref_nodes}|{p_percentage}"
    if key not in get_min_max_nodes.memo_dict:
        formula = job_dict["scaling_formula"]
        max_threshold = job_dict["max_node_efficiency_threshold"]

        min_nodes = max(1, int(pref_nodes * min_scaling)) # min_nodes follows min_scaling; min_node_efficiency_threshold is not used here
        max_nodes = pref_nodes + 1
        while get_scaling_factor(formula, max_nodes, p_percentage) > max_threshold:
            max_nodes += 1
        min_nodes = max(1, min_nodes)
        max_nodes = min(cluster_nodes, max_nodes - 1)
        get_min_max_nodes.memo_dict[key] = (min_nodes, max_nodes)
    return get_min_max_nodes.memo_dict[key]


def get_divide_amount(job_type, pref_nodes, flops_per_node, flops, jd, cal=True):
    if job_type == "RIGID":
        return 1
    if not cal:
        return jd["malleable_dividation_amount"]

    total_flops = pref_nodes * flops_per_node
    seconds = flops // total_flops
    calculated_div = max(1, seconds // jd["dividation_split_time"])
    if jd["malleable_dividation_amount"] <= 0:
        return calculated_div

    return min(calculated_div, jd["malleable_dividation_amount"])


def encode_job(
    id, job_type, submit_time, node_range, p_percentage, flops, divide_amount, timelimit, runtime, randomizer, jd
):
    """
    Encode job with predicted_runtime for draining algorithm.
    
    NOTE: walltime is set to 0 to DISABLE ElastiSim's WalltimeMonitor (prevents jobs being killed).
    Instead, predicted_runtime is stored in attributes for use by shutdown policy draining logic.
    """
    application_model = "application_model.json"  # relative to the input bundle directory
    min_nodes, pref_nodes, max_nodes = node_range
    
    # Validate runtime (critical: prevents invalid calculations)
    if runtime is None or runtime <= 0:
        raise ValueError(f"Job {id}: Invalid runtime={runtime}. Must be > 0.")
    
    # Calculate predicted runtime with ML prediction error
    WALLTIME_ALPHA = jd.get("walltime_alpha", 1.1)  # Default: 10% safety margin
    prediction_factor = sample_prediction_factor(randomizer)
    predicted_runtime_float = WALLTIME_ALPHA * prediction_factor * runtime
    
    # Ensure predicted runtime does not exceed timelimit (hard constraint)
    predicted_runtime = max(1, min(int(math.ceil(predicted_runtime_float)), int(timelimit)))

    arguments = {
        "divide": divide_amount,
        "parallel_percentage": p_percentage,
        "flops": flops,
        "timelimit": timelimit,
        "num_nodes_pref": pref_nodes,
        "id": id,
    }

    attributes = {
        "csv_id": id,
        "kassel_penalty_function": jd["penalty_formula"],
        "predicted_runtime": predicted_runtime,  # For draining algorithm (ML-based prediction)
        "prediction_base_nodes": pref_nodes,
    }

    if job_type == "RIGID":
        return {
            "type": job_type,
            "submit_time": submit_time,
            "num_nodes": pref_nodes,
            "application_model": application_model,
            "arguments": arguments,
            "attributes": attributes,
        }
    else:
        return {
            "type": job_type,
            "submit_time": submit_time,
            "num_nodes_min": min_nodes,
            "num_nodes_max": max_nodes,
            "application_model": application_model,
            "arguments": arguments,
            "attributes": attributes,
        }


def csv_flops(runtime, pref_nodes, flops_per_node):
    total_job_flops = pref_nodes * flops_per_node
    return runtime * total_job_flops


def generate_job(csv_job, jd, cluster_dict):
    id = csv_job["id"]
    seed = jd["seed"]
    flops_per_node = cluster_dict["flops_per_cluster_node"]
    cluster_nodes = cluster_dict["num_cluster_nodes"]
    randomizer = Randomizer(seed + str(id))
    job_type = randomizer.get_random_value_in_weighted_dict(jd["type_probabilities"])
    submit_time = csv_job["submit_time"]
    pref_nodes = csv_job["num_nodes"]
    p_percentage = get_parallel_percentage(pref_nodes, jd)
    min_nodes, max_nodes = get_min_max_nodes(
        pref_nodes, p_percentage, cluster_nodes, jd
    )
    scaling_factor = get_scaling_factor(jd["scaling_formula"], pref_nodes, p_percentage)
    # For CSV jobs: Use actual FLOPS without scaling factor
    # The job already ran with this node count, FLOPS scale linearly with nodes
    # scaling_factor is only for moldable/malleable job predictions
    flops = csv_flops(csv_job["runtime"], pref_nodes, flops_per_node)  # NO scaling_factor!
    node_range = (min_nodes, pref_nodes, max_nodes)
    divide_amount = get_divide_amount(job_type, pref_nodes, flops_per_node, flops, jd)
    runtime = csv_job["runtime"]

    if "timelimit" in csv_job.keys():
        timelimit = csv_job["timelimit"]
    else:
        timelimit = runtime * 1.25
        timelimit += 3600 - timelimit % 3600

    return encode_job(
        id, job_type, submit_time, node_range, p_percentage, flops, divide_amount, timelimit, runtime, randomizer, jd
    )


def parse_csv_row_to_job(row, cluster_dict, node_div, mapping):
    queued_time = datetime.datetime.strptime(row[mapping["submit_time"]], DATE_FORMAT)
    if time_window[0] > queued_time or time_window[1] < queued_time:
        return []
    time_since_timewindow_start = queued_time - time_window[0]
    job = dict()
    job["id"] = int(row[mapping["id"]])
    job["submit_time"] = time_since_timewindow_start.total_seconds()
    #job["num_nodes"] = int(float(row["NODES_USED"]))
    nn = int(float(row[mapping["num_nodes"]]))
    nn //= node_div
    if nn < 1:
        nn = 1
    #assert job["num_nodes"] % node_div == 0
    job["num_nodes"] = nn
    #job["num_nodes"] //= node_div
    job["runtime"] = int(float(row[mapping["runtime"]]))

    if "timelimit" in mapping.keys():
        job["timelimit"] = int(float(row[mapping["timelimit"]]))

    return [job]


def generate_jobs_from_csv(csv_file, time_window, job_dict, cluster_dict, node_div):
    job_csvs = []
    with open(csv_file, "r", encoding='utf-8-sig') as file:
        reader = csv.DictReader(file)
        csv_list = [row for row in reader]
        # get column names
        names = reader.fieldnames
        mapping = dict()
        for item in COLUMNNAMES:
            for name in COLUMNNAMES[item]:
                if name in names:
                    mapping[item] = name

        for row in sorted(csv_list, key=lambda r: r[mapping["id"]]):
            job_csvs.extend(parse_csv_row_to_job(row, cluster_dict, node_div, mapping))

    jobs_to_generate = [generate_job(j, job_dict, cluster_dict) for j in job_csvs]
    first_submit = min(job["submit_time"] for job in jobs_to_generate)
    for job in jobs_to_generate:
        job["submit_time"] = int(job["submit_time"] - first_submit)
    jobs_to_generate.sort(key=lambda j: j["submit_time"])
    for id, jd in enumerate(jobs_to_generate):
        jd["arguments"]["id"] = id

    # CRITICAL VALIDATION: Check for predicted_runtime in attributes (required for draining)
    invalid_jobs = [j for j in jobs_to_generate if j.get("attributes", {}).get("predicted_runtime", 0) <= 0]
    if invalid_jobs:
        print(f"ERROR: Found {len(invalid_jobs)} jobs with invalid predicted_runtime!")
        for j in invalid_jobs[:5]:  # Show first 5
            print(f"  Job {j['arguments']['id']}: predicted_runtime={j.get('attributes', {}).get('predicted_runtime')}, csv_id={j['attributes'].get('csv_id')}")
        raise ValueError(f"Job generation failed: {len(invalid_jobs)} jobs have predicted_runtime <= 0")

    total_runtime = 0

    for j in job_csvs:
        total_runtime += (j["runtime"]*j["num_nodes"])/cluster_dict["num_cluster_nodes"]

    simulation_time = int((time_window[1] - time_window[0]).total_seconds())
    
    jobs_json = {
        "jobs_generated": len(jobs_to_generate),
        "simulation_time": simulation_time,
        "simulation_time (in days)": simulation_time/86400,
        "total_job_time": total_runtime,
        "total_job_time_percent": total_runtime/(simulation_time/100),
        "flops_per_node": cluster_dict["flops_per_cluster_node"],
        "num_cluster_nodes": cluster_dict["num_cluster_nodes"],
        "runtime_prediction_scaling_formula": job_dict.get(
            "runtime_prediction_scaling_formula", job_dict["scaling_formula"]
        ),
        "generation values": str(job_dict),
        "jobs": jobs_to_generate,
    }
    return json.dumps(jobs_json, indent=4)


def generate_application_model(scaling_formula):
    am = {
        "phases": [
            {
                "iterations": "divide",
                "tasks": [
                    {
                        "type": "cpu",
                        "name": "Compute",
                        "flops": "(flops/divide)/" + scaling_formula,
                        "computation_pattern": "UNIFORM",
                    }
                ],
            }
        ]
    }
    return json.dumps(am, indent=4)


def generate_configuration(
    scheduling_interval=1,
    min_scheduling_interval=1,
    schedule_on_job_submit=True,
    schedule_on_job_finalize=True,
    schedule_on_reconfiguration=True,
    schedule_on_scheduling_point=False,
    sensing=True,
    sensing_interval=60,
):
    conf = {
        "scheduling_interval": scheduling_interval,
        "min_scheduling_interval": min_scheduling_interval,
        "schedule_on_job_submit": schedule_on_job_submit,
        "schedule_on_job_finalize": schedule_on_job_finalize,
        "schedule_on_reconfiguration": schedule_on_reconfiguration,
        "schedule_on_scheduling_point": schedule_on_scheduling_point,
        "allow_oversubscription": False,
        "clip_evolving_requests": True,
        "sensing": sensing,
        "sensing_interval": sensing_interval,
        "pfs_read_links": ["PFS_read"],
        "pfs_write_links": ["PFS_write"],
        "jobs_file": "jobs.json",
        "platform_file": "crossbar.xml",
        "job_statistics": "../output_files/job_statistics.csv",
        "cpu_utilization": "../output_files/cpu_utilization.csv",
        "node_utilization": "../output_files/node_utilization.csv",
        "network_activity": "../output_files/network_activity.csv",
        "pfs_utilization": "../output_files/pfs_utilization.csv",
        "gpu_utilization": "../output_files/gpu_utilization.csv",
    }
    return json.dumps(conf, indent=4)


def generate_crossbar(cluster_dict):
    out = r"""<?xml version='1.0'?>
    <!DOCTYPE platform SYSTEM "https://simgrid.org/simgrid.dtd">
        <platform version="4.1">
            <zone id="CSV" routing="Full">
                <zone id="Batch-system_zone" routing="Full">
                    <host id="Batch_system" speed="0Gf">
                        <prop id="batch_system" value="true"/>
                    </host>
                </zone>
                <cluster id="Crossbar" prefix="Mi_" radical="{cluster_nodes_min}-{cluster_nodes_max}" suffix=""
                        speed="{flops_per_node}f" bw="100Gbps" lat="50us">
                    <prop id="node_local_bb" value="false"/>
                    <prop id="pfs_targets" value="PFS"/>
                </cluster>
                <zone id="PFS_zone" routing="Full">
                    <host id="PFS" speed="0Gf">
                        <prop id="pfs_host" value="true"/>
                    </host>
                </zone>
                <link id="PFS_read" bandwidth="300GBps" latency="500us"/>
                <link id="PFS_write" bandwidth="300GBps" latency="500us"/>
                <zoneRoute src="PFS_zone" dst="Crossbar" gw_src="PFS"
                        gw_dst="Mi_Crossbar_router" symmetrical="NO">
                    <link_ctn id="PFS_read"/>
                </zoneRoute>
                <zoneRoute src="Crossbar" dst="PFS_zone" gw_src="Mi_Crossbar_router"
                        gw_dst="PFS" symmetrical="NO">
                    <link_ctn id="PFS_write"/>
                </zoneRoute>
            </zone>
        </platform>"""


    m = math.pow(10, len(str(cluster_dict["num_cluster_nodes"])) - 1)

    if len(str(m + cluster_dict["num_cluster_nodes"])) > len(str(m)):
        m *= 10

    return out.format(
        cluster_nodes_min=m + 1,
        cluster_nodes_max=m + cluster_dict["num_cluster_nodes"],
        flops_per_node=cluster_dict["flops_per_cluster_node"],
    )


def generate_json_files(path, csv_file, time_window, node_div, cluster_dict, job_dict):
    input_config = locals().copy()

    assert cluster_dict["num_cluster_nodes"] % node_div == 0
    cluster_dict["num_cluster_nodes"] //= node_div

    cluster_dict["flops_per_cluster_node"] = cluster_dict["cluster_flops"] // cluster_dict["num_cluster_nodes"]
    jobs_json = generate_jobs_from_csv(
        csv_file, time_window, job_dict, cluster_dict, node_div
    )
    am_json = generate_application_model(job_dict["scaling_formula"])
    configuration_json = generate_configuration(min_scheduling_interval=job_dict["min_scheduling_interval"])
    crossbar_xml = generate_crossbar(cluster_dict)

    write_to_file(path, "jobs.json", jobs_json)
    write_to_file(path, "application_model.json", am_json)
    write_to_file(path, "configuration.json", configuration_json)
    write_to_file(path, "crossbar.xml", crossbar_xml)
    return f"Generating json file with {input_config}"


def get_arguments(argv, vals):
    arg_dict = dict()
    csv_file = None
    directory = None
    quiet = False

    arg_names = ["quiet", "directory="] + [f"{v}=" for v in vals]
    opts, args = getopt.getopt(argv, "qd:p:", arg_names)
    for opt, arg in opts:
        if opt in ("-q", "--quiet"):
            quiet = True
        elif opt in ("-d", "--directory"):
            directory = arg
            if not os.path.isdir(directory):
                os.makedirs(directory)
        elif opt in ("-p", "--path"):
            assert os.path.isfile(arg)
            csv_file = arg
        elif opt == "--seed":
            arg_dict["seed"] = arg
        elif opt == "--node_divide_amount":
            arg_dict["node_divide_amount"] = int(float(arg))
        elif opt == "--time_window":
            tw = [s for s in arg.split(",")]
            assert len(tw) == 2
            start = datetime.datetime.strptime(tw[0], ARGS_DATE_FORMAT)
            end = datetime.datetime.strptime(tw[1], ARGS_DATE_FORMAT)
            arg_dict["time_window"] = (start, end)
        elif opt == "--type_probabilities":
            probs = [int(n) for n in arg.split(",")]
            assert len(probs) == 3
            arg_dict["type_probabilities"] = {
                "RIGID": probs[0],
                "MOLDABLE": probs[1],
                "MALLEABLE": probs[2],
            }
        elif opt == "--malleable_dividation_amount":
            arg_dict["malleable_dividation_amount"] = float(arg)
        elif opt == "--dividation_split_time":
            arg_dict["dividation_split_time"] = float(arg)
        elif opt == "--application_model":
            arg_dict["application_model"] = str(arg)
        elif opt == "--parallel_percentage":
            probs = [float(n) for n in arg.split(",")]
            arg_dict["parallel_percentage"] = probs
        elif opt == "--pref_node_efficiency_threshold":
            arg_dict["pref_node_efficiency_threshold"] = float(arg)
        elif opt == "--min_node_efficiency_threshold":
            arg_dict["min_node_efficiency_threshold"] = float(arg)
        elif opt == "--max_node_efficiency_threshold":
            arg_dict["max_node_efficiency_threshold"] = float(arg)
        elif opt == "--min_scheduling_interval":
            arg_dict["min_scheduling_interval"] = float(arg)
        elif opt == "--scaling_formula":
            arg_dict["scaling_formula"] = str(arg)
        elif opt == "--runtime_prediction_scaling_formula":
            arg_dict["runtime_prediction_scaling_formula"] = str(arg)
        elif opt == "--penalty_formula":
            arg_dict["penalty_formula"] = str(arg)
        elif opt == "--walltime_alpha":
            arg_dict["walltime_alpha"] = float(arg)
        elif opt == "--cluster_flops":
            arg_dict["cluster_flops"] = float(arg)
        elif opt == "--num_cluster_nodes":
            arg_dict["num_cluster_nodes"] = int(arg)
    return directory, csv_file, quiet, arg_dict


def get_arg(name, value, args, quiet=True):
    if name in args:
        return args[name]
    else:
        if not quiet:
            print(f"{name} not defined, falling back to default value: {value}")
        return value


if __name__ == "__main__":
    node_divide_amount = 2**6
    time_window = (
        datetime.datetime(2020, 1, 1, 0, 0, 0),
        datetime.datetime(2020, 1, 31, 23, 59, 59),
    )
    job_dict = {
        "seed": "DefaultSeed",
        "type_probabilities": {"RIGID": 50, "MOLDABLE": 20, "MALLEABLE": 30},
        "malleable_dividation_amount": 1000,
        "dividation_split_time": 60,
        "application_model": "data/input/application_model.json",
        "parallel_percentage": (0.99999, 0.9999, 0.999, 0.995, 0.99, 0.98, 0.97),
        "min_node_efficiency_threshold": 1,
        "pref_node_efficiency_threshold": 0.8,
        "max_node_efficiency_threshold": 0.5,
        "min_scheduling_interval": 0.1,
        "scaling_formula": "(1/((1-parallel_percentage) + parallel_percentage/num_nodes))",
        "runtime_prediction_scaling_formula": "(1/((1-parallel_percentage) + parallel_percentage/num_nodes))",
        "penalty_formula": "0",
        "walltime_alpha": 1.1
    }
    cluster_dict = {"cluster_flops": 10e16, "flops_per_cluster_node": 10e9, "num_cluster_nodes": 49_152}
    arg_names = (
        ["time_window", "node_divide_amount"]
        + list(job_dict.keys())
        + list(cluster_dict.keys())
    )
    path, csv_file, quiet, args = get_arguments(sys.argv[1:], arg_names)
    assert path != None
    assert csv_file != None
    time_window = get_arg("time_window", time_window, args)
    node_divide_amount = get_arg("node_divide_amount", node_divide_amount, args)
    for k, v in cluster_dict.items():
        cluster_dict[k] = get_arg(k, v, args)
    for k, v in job_dict.items():
        job_dict[k] = get_arg(k, v, args)
    out = generate_json_files(
        path, csv_file, time_window, node_divide_amount, cluster_dict, job_dict
    )
    if not quiet:
        print(out)
