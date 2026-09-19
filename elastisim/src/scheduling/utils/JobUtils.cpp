/*
 * This file is part of the ElastiSim SC26 research extension
 * (scheduler-integrated recurring node shutdown exploiting malleable jobs).
 *
 * Copyright (c) 2026 Fulda University of Applied Sciences, Germany
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This software may be modified and distributed under the terms of the 3-Clause
 * BSD License. See the LICENSE file in the repository root for the license text
 * and README.md for the origin of each component.
 */

#include "JobUtils.h"

#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>

#include "../../software/Job.h"
#include "../../util/Configuration.h"
#include "../../system/ShutdownPolicyManager.h"
#include "../../system/Node.h"
#include "../../util/PerformanceTracer.h"
#include "../../util/Utility.h"
#include <simgrid/s4u.hpp>

XBT_LOG_NEW_DEFAULT_CATEGORY(JobUtils, "Job utility functions");

namespace {

std::string runtimePredictionFormula;
bool runtimePredictionFormulaCached = false;

std::string loadRuntimePredictionFormulaFromJobsFile() {
    if (!Configuration::exists("jobs_file")) {
        return {};
    }

    const std::string jobsFile = Configuration::get("jobs_file");
    if (jobsFile.empty()) {
        return {};
    }

    std::ifstream stream(jobsFile);
    if (!stream.is_open()) {
        xbt_die("Could not open jobs_file %s while loading runtime prediction formula", jobsFile.c_str());
    }

    nlohmann::json jobsJson = nlohmann::json::parse(stream);
    if (!jobsJson.contains("runtime_prediction_scaling_formula")) {
        return {};
    }
    if (!jobsJson["runtime_prediction_scaling_formula"].is_string()) {
        xbt_die("jobs_file %s contains non-string runtime_prediction_scaling_formula", jobsFile.c_str());
    }

    return jobsJson["runtime_prediction_scaling_formula"];
}

[[noreturn]] void dieInvalidPredictionInput(const Job* job,
                                           const std::string& keyPath,
                                           const std::string& message) {
    const int jobId = job ? job->getId() : -1;
    xbt_die("Runtime prediction input invalid for job %d (%s): %s",
            jobId,
            keyPath.c_str(),
            message.c_str());
}

}

void JobUtils::validateRuntimePredictionConfig() {
    if (runtimePredictionFormulaCached) {
        return;
    }

    const std::string jobsFileFormula = loadRuntimePredictionFormulaFromJobsFile();
    const bool configHasFormula = Configuration::exists("runtime_prediction_scaling_formula");
    const std::string configFormula = configHasFormula ?
        static_cast<std::string>(Configuration::get("runtime_prediction_scaling_formula")) :
        std::string();

    if (!jobsFileFormula.empty() && configHasFormula && jobsFileFormula != configFormula) {
        xbt_die("Mismatch between jobs_file runtime_prediction_scaling_formula and main config runtime_prediction_scaling_formula");
    }

    const std::string formula = !jobsFileFormula.empty() ? jobsFileFormula : configFormula;
    if (formula.empty()) {
        xbt_die("Missing required runtime_prediction_scaling_formula in jobs_file metadata or main config");
    }

    Configuration::set("runtime_prediction_scaling_formula", formula);
    runtimePredictionFormula = formula;
    runtimePredictionFormulaCached = true;
}

void JobUtils::validateRuntimePredictionJob(const Job* job) {
    if (!job) {
        xbt_die("Runtime prediction validation received null job");
    }

    validateRuntimePredictionConfig();
    (void) getPredictedRuntime(job);
    (void) getPredictionBaseNodes(job);
    (void) parseRequiredDouble(job, job->findArgument("parallel_percentage"), "arguments.parallel_percentage");
    (void) getSpeedup(job, getPredictionBaseNodes(job));
}

void JobUtils::validateRuntimePredictionJobs(const std::vector<std::unique_ptr<Job>>& jobs) {
    validateRuntimePredictionConfig();
    for (const auto& job : jobs) {
        validateRuntimePredictionJob(job.get());
    }
}

void JobUtils::updateRunningJobsPrediction(const std::vector<Job*>& jobs, double currentTime) {
    for (Job* job : jobs) {
        if (!job) {
            continue;
        }
        if (job->getState() != RUNNING) {
            continue;
        }
        updatePredictionProgress(job, currentTime);
    }
}

double JobUtils::getPredictedRuntime(const Job* job) {
    PerformanceTracer::recordJobUtilsRuntimeCall();
    if (!job) {
        xbt_die("JobUtils::getPredictedRuntime called with null job");
    }

    const double runtime = parseRequiredDouble(job, job->findAttribute("predicted_runtime"), "attributes.predicted_runtime");
    if (runtime <= 0.0) {
        dieInvalidPredictionInput(job, "attributes.predicted_runtime", "value must be > 0");
    }
    return runtime;
}

int JobUtils::getPredictionBaseNodes(const Job* job) {
    if (!job) {
        xbt_die("JobUtils::getPredictionBaseNodes called with null job");
    }

    return parseRequiredInt(job, job->findAttribute("prediction_base_nodes"), "attributes.prediction_base_nodes");
}

double JobUtils::getSpeedup(const Job* job, int nodes) {
    if (!job) {
        xbt_die("JobUtils::getSpeedup called with null job");
    }

    ensurePositiveNodes(job, nodes, "speedup");
    validateRuntimePredictionConfig();

    const double speedup = Utility::evaluateFormula(
        getRuntimePredictionFormula(),
        nodes,
        job->getNumGpusPerNodeMax(),
        job->getArguments());

    if (!std::isfinite(speedup) || speedup <= 0.0) {
        dieInvalidPredictionInput(job,
                                  "runtime_prediction_scaling_formula",
                                  "evaluated speedup must be finite and > 0");
    }

    return speedup;
}

double JobUtils::getRelativeRate(const Job* job, int nodes) {
    if (!job) {
        xbt_die("JobUtils::getRelativeRate called with null job");
    }

    const int referenceNodes = getPredictionBaseNodes(job);
    const double referenceSpeedup = getSpeedup(job, referenceNodes);
    const double candidateSpeedup = getSpeedup(job, nodes);
    const double rate = candidateSpeedup / referenceSpeedup;

    if (!std::isfinite(rate) || rate <= 0.0) {
        dieInvalidPredictionInput(job, "relative_rate", "computed relative rate must be finite and > 0");
    }

    return rate;
}

double JobUtils::getPredictedRuntimeForNodes(const Job* job, int plannedNodes) {
    if (!job) {
        xbt_die("JobUtils::getPredictedRuntimeForNodes called with null job");
    }

    ensurePositiveNodes(job, plannedNodes, "predicted runtime");
    return getPredictedRuntime(job) / getRelativeRate(job, plannedNodes);
}

double JobUtils::getPredictedRemainingRuntime(const Job* job, double currentTime) {
    if (!job) {
        xbt_die("JobUtils::getPredictedRemainingRuntime called with null job");
    }

    return getPredictedRemainingRuntimeForNodes(job, job->getLastPredictionNodes(), currentTime);
}

double JobUtils::getPredictedRemainingRuntimeForNodes(const Job* job, int plannedNodes, double currentTime) {
    if (!job) {
        xbt_die("JobUtils::getPredictedRemainingRuntimeForNodes called with null job");
    }
    if (!job->isPredictionStateInitialized()) {
        dieInvalidPredictionInput(job,
                                  "prediction_state",
                                  "remaining runtime requested before prediction state initialization");
    }

    ensurePositiveNodes(job, plannedNodes, "remaining runtime");

    const int currentNodes = job->getLastPredictionNodes();
    ensurePositiveNodes(job, currentNodes, "current prediction node count");

    double remainingWorkRef = job->getPredictedRemainingWorkRef();
    const double lastUpdate = job->getLastPredictionUpdateTime();
    if (currentTime > lastUpdate) {
        const double dt = currentTime - lastUpdate;
        remainingWorkRef = std::max(0.0, remainingWorkRef - dt * getRelativeRate(job, currentNodes));
    }

    return remainingWorkRef / getRelativeRate(job, plannedNodes);
}

void JobUtils::updatePredictionProgress(Job* job, double currentTime) {
    if (!job) {
        return;
    }
    if (!job->isPredictionStateInitialized()) {
        return;
    }

    const double lastUpdate = job->getLastPredictionUpdateTime();
    if (currentTime <= lastUpdate) {
        return;
    }

    const int currentNodes = job->getLastPredictionNodes();
    ensurePositiveNodes(job, currentNodes, "prediction progress");

    const double dt = currentTime - lastUpdate;
    const double updatedWorkRef = std::max(
        0.0,
        job->getPredictedRemainingWorkRef() - dt * getRelativeRate(job, currentNodes));
    job->updatePredictionState(updatedWorkRef, currentTime, currentNodes);
}

void JobUtils::synchronizePredictionStateForCurrentAllocation(Job* job, double currentTime) {
    if (!job) {
        return;
    }

    const int currentNodes = job->getNumberOfExecutingNodes();
    ensurePositiveNodes(job, currentNodes, "prediction state synchronization");

    if (!job->isPredictionStateInitialized()) {
        job->initializePredictionState(
            getPredictionBaseNodes(job),
            getPredictedRuntime(job),
            currentTime,
            currentNodes);
        return;
    }

    updatePredictionProgress(job, currentTime);
    job->updatePredictionState(job->getPredictedRemainingWorkRef(), currentTime, currentNodes);
}

std::vector<Node*> JobUtils::filterNodesForPendingAllocation(const std::vector<Node*>& nodes,
                                                             const Job* job,
                                                             int plannedNodes,
                                                             double currentTime,
                                                             bool ignoreTargetPool) {
    const double runtime = getPredictedRuntimeForNodes(job, plannedNodes);
    std::vector<Node*> filtered;
    filtered.reserve(nodes.size());
    for (Node* node : nodes) {
        if (!node) {
            continue;
        }
        if (ShutdownPolicyManager::canNodeAcceptJob(node, runtime, currentTime, ignoreTargetPool)) {
            filtered.push_back(node);
        }
    }
    return filtered;
}

std::vector<Node*> JobUtils::filterNodesForRunningJob(const std::vector<Node*>& nodes,
                                                      const Job* job,
                                                      int plannedNodes,
                                                      double currentTime,
                                                      bool ignoreTargetPool) {
    const double runtime = getPredictedRemainingRuntimeForNodes(job, plannedNodes, currentTime);
    std::vector<Node*> filtered;
    filtered.reserve(nodes.size());
    for (Node* node : nodes) {
        if (!node) {
            continue;
        }
        if (ShutdownPolicyManager::canNodeAcceptJob(node, runtime, currentTime, ignoreTargetPool)) {
            filtered.push_back(node);
        }
    }
    return filtered;
}

double JobUtils::getTimelimit(const Job* job) {
    if (!job) {
        xbt_die("JobUtils::getTimelimit called with null job");
    }

    const std::string* rawValue = job->findArgument("timelimit");
    if (!rawValue) {
        logMissingAttribute(job, "arguments.timelimit");
        return -1.0;
    }

    try {
        const double timelimit = std::stod(*rawValue);
        if (timelimit > 0.0 && std::isfinite(timelimit)) {
            return timelimit;
        }
    } catch (const std::exception&) {
    }

    logMissingAttribute(job, "arguments.timelimit");
    return -1.0;
}

int JobUtils::getRequiredNodes(const Job* job) {
    if (!job) {
        xbt_die("JobUtils::getRequiredNodes called with null job");
    }

    const int requiredNodes = (job->getType() == RIGID) ? job->getNumNodesMax() : job->getNumNodesMin();
    if (requiredNodes <= 0) {
        XBT_WARN("Job %d has invalid node count: %d", job->getId(), requiredNodes);
        return -1;
    }

    return requiredNodes;
}

bool JobUtils::hasValidAttributes(const Job* job) {
    if (!job) {
        return false;
    }

    if (getRequiredNodes(job) <= 0) {
        return false;
    }

    const std::string* predictedRuntime = job->findAttribute("predicted_runtime");
    const std::string* baseNodes = job->findAttribute("prediction_base_nodes");
    const std::string* parallelPercentage = job->findArgument("parallel_percentage");
    if (!predictedRuntime || !baseNodes || !parallelPercentage) {
        return false;
    }

    return true;
}

const std::string& JobUtils::getRuntimePredictionFormula() {
    if (!runtimePredictionFormulaCached) {
        validateRuntimePredictionConfig();
    }
    return runtimePredictionFormula;
}

double JobUtils::parseRequiredDouble(const Job* job,
                                     const std::string* rawValue,
                                     const std::string& keyPath) {
    if (!rawValue) {
        dieInvalidPredictionInput(job, keyPath, "missing required numeric value");
    }

    try {
        const double parsedValue = std::stod(*rawValue);
        if (!std::isfinite(parsedValue)) {
            throw std::runtime_error("value is not finite");
        }
        return parsedValue;
    } catch (const std::exception& e) {
        dieInvalidPredictionInput(job, keyPath, std::string("failed to parse numeric value: ") + e.what());
    }
}

int JobUtils::parseRequiredInt(const Job* job,
                               const std::string* rawValue,
                               const std::string& keyPath) {
    const double parsedValue = parseRequiredDouble(job, rawValue, keyPath);
    const double roundedValue = std::round(parsedValue);
    if (std::fabs(parsedValue - roundedValue) > 1e-9) {
        dieInvalidPredictionInput(job, keyPath, "value must be an integer");
    }
    const long long integerValue = static_cast<long long>(roundedValue);
    if (integerValue <= 0 || integerValue > std::numeric_limits<int>::max()) {
        dieInvalidPredictionInput(job, keyPath, "value must be a positive integer");
    }
    return static_cast<int>(integerValue);
}

void JobUtils::ensurePositiveNodes(const Job* job, int nodes, const std::string& context) {
    if (nodes <= 0) {
        dieInvalidPredictionInput(job, context, "node count must be > 0");
    }
}

void JobUtils::logMissingAttribute(const Job* job, const std::string& attributeName) {
    XBT_WARN("Job %d missing attribute: %s", job ? job->getId() : -1, attributeName.c_str());
}
