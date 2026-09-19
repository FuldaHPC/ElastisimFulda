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

#ifndef ELASTISIM_SCHEDULING_JOBUTILS_H
#define ELASTISIM_SCHEDULING_JOBUTILS_H

#include <memory>
#include <string>
#include <vector>

class Job;
class Node;

/**
 * Utility functions for extracting and validating job attributes.
 * Centralizes runtime prediction, validation, and admission helpers.
 */
class JobUtils {
public:
    static void validateRuntimePredictionConfig();

    static void validateRuntimePredictionJob(const Job* job);

    static void validateRuntimePredictionJobs(const std::vector<std::unique_ptr<Job>>& jobs);

    static void updateRunningJobsPrediction(const std::vector<Job*>& jobs, double currentTime);

    /**
     * Extract the reference predicted runtime from job attributes.
     * 
     * @param job Job to extract runtime from
     * @return Reference predicted runtime in seconds
     */
    static double getPredictedRuntime(const Job* job);

    static int getPredictionBaseNodes(const Job* job);

    static double getSpeedup(const Job* job, int nodes);

    static double getRelativeRate(const Job* job, int nodes);

    static double getPredictedRuntimeForNodes(const Job* job, int plannedNodes);

    static double getPredictedRemainingRuntime(const Job* job, double currentTime);

    static double getPredictedRemainingRuntimeForNodes(const Job* job, int plannedNodes, double currentTime);

    static void updatePredictionProgress(Job* job, double currentTime);

    static void synchronizePredictionStateForCurrentAllocation(Job* job, double currentTime);

    static std::vector<Node*> filterNodesForPendingAllocation(const std::vector<Node*>& nodes,
                                                              const Job* job,
                                                              int plannedNodes,
                                                              double currentTime,
                                                              bool ignoreTargetPool = false);

    static std::vector<Node*> filterNodesForRunningJob(const std::vector<Node*>& nodes,
                                                       const Job* job,
                                                       int plannedNodes,
                                                       double currentTime,
                                                       bool ignoreTargetPool = false);
    
    /**
     * Extract timelimit from job arguments.
     * 
     * @param job Job to extract timelimit from
     * @return Timelimit in seconds, or -1.0 if not available
     */
    static double getTimelimit(const Job* job);
    
    /**
     * Get required number of nodes based on job type.
     * For RIGID jobs: returns num_nodes
     * For flexible jobs: returns num_nodes_min
     * 
     * @param job Job to extract node count from
     * @return Required nodes, or -1 if invalid
     */
    static int getRequiredNodes(const Job* job);
    
    /**
     * Validate that job has all required attributes for scheduling.
     * 
     * @param job Job to validate
     * @return true if job is valid for scheduling
     */
    static bool hasValidAttributes(const Job* job);
    
private:
    static const std::string& getRuntimePredictionFormula();

    static double parseRequiredDouble(const Job* job,
                                      const std::string* rawValue,
                                      const std::string& keyPath);

    static int parseRequiredInt(const Job* job,
                                const std::string* rawValue,
                                const std::string& keyPath);

    static void ensurePositiveNodes(const Job* job, int nodes, const std::string& context);

    /**
     * Log warning when required attribute is missing.
     */
    static void logMissingAttribute(const Job* job, const std::string& attributeName);
};

#endif // ELASTISIM_SCHEDULING_JOBUTILS_H
