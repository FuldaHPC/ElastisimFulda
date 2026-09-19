/*
 * This file is part of the ElastiSim SC26 research extension
 * (scheduler-integrated recurring node shutdown exploiting malleable jobs).
 *
 * C++ implementation adapted from the shrink, expand and head-reservation helper routines shared
 * by the Wagomu project's MalleableJobScheduling strategies
 * (scheduling_algorithms/*.py)
 * https://github.com/ProjectWagomu/MalleableJobScheduling
 * Copyright (c) 2023 Wagomu project
 *
 * C++ implementation Copyright (c) 2026 Fulda University of Applied Sciences, Germany
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0. See the LICENSE.EPL-2.0 file in the
 * repository root for the license text and README.md for component origins.
 */

#include "MalleabilityHelper.h"
#include "AgreementHandler.h"
#include "../../software/Job.h"
#include "../../system/Node.h"
#include "../../system/ShutdownPolicyManager.h"
#include "../../util/EventLogger.h"
#include "../../util/PerformanceTracer.h"
#include "../utils/JobUtils.h"
#include "../utils/NodeCache.h"
#include <algorithm>
#include <simgrid/s4u.hpp>

XBT_LOG_NEW_DEFAULT_CATEGORY(MalleabilityHelper, "Malleability helper functions");

std::vector<std::pair<Job*, std::vector<Node*>>> MalleabilityHelper::selectShrinkJobs(
    const std::vector<Job*>& runningMalleableJobs,
    int requiredNodes,
    JobPriorityFunc priorityFunc,
    const AgreementHandler& agreements) {
    PerformanceTracer::ScopedPhase phase("shrink_selection");
    
    std::vector<std::pair<Job*, std::vector<Node*>>> jobsToShrink;
    
    // 1. Sort jobs by priority (highest first)
    auto sortedJobs = runningMalleableJobs;
    std::stable_sort(sortedJobs.begin(), sortedJobs.end(),
                     [&priorityFunc](Job* a, Job* b) {
                         return priorityFunc(a) > priorityFunc(b);
                     });
    
    // 2. Collect nodes from jobs until we have enough
    int nodesCollected = 0;
    for (Job* job : sortedJobs) {
        if (nodesCollected >= requiredNodes) {
            break;
        }
        
        int nodesStillNeeded = requiredNodes - nodesCollected;
        auto nodesToRemove = allocateResourcesFromJob(job, nodesStillNeeded, agreements);
        
        if (!nodesToRemove.empty()) {
            jobsToShrink.emplace_back(job, nodesToRemove);
            nodesCollected += nodesToRemove.size();
        }
    }
    
    // 3. Return only if we collected enough nodes
    if (nodesCollected >= requiredNodes) {
        return jobsToShrink;
    }
    
    // Not enough resources available
    return {};
}

std::vector<std::pair<Job*, std::vector<Node*>>> MalleabilityHelper::selectShrinkJobsOrdered(
    const std::vector<Job*>& orderedJobs,
    int requiredNodes,
    const AgreementHandler& agreements) {
    PerformanceTracer::ScopedPhase phase("shrink_selection");

    std::vector<std::pair<Job*, std::vector<Node*>>> jobsToShrink;

    int nodesCollected = 0;
    for (Job* job : orderedJobs) {
        if (nodesCollected >= requiredNodes) {
            break;
        }

        int nodesStillNeeded = requiredNodes - nodesCollected;
        auto nodesToRemove = allocateResourcesFromJob(job, nodesStillNeeded, agreements);

        if (!nodesToRemove.empty()) {
            jobsToShrink.emplace_back(job, nodesToRemove);
            nodesCollected += nodesToRemove.size();
        }
    }

    if (nodesCollected >= requiredNodes) {
        return jobsToShrink;
    }

    return {};
}

void MalleabilityHelper::applyShrinking(
    const std::vector<std::pair<Job*, std::vector<Node*>>>& jobsToShrink,
    Job* pendingJob,
    AgreementHandler& agreements) {
    
    if (jobsToShrink.empty() || !pendingJob) {
        return;
    }

    PerformanceTracer::ScopedPhase phase("shrink_apply");
    const double currentTime = simgrid::s4u::Engine::get_clock();
    
    for (const auto& entry : jobsToShrink) {
        Job* job = entry.first;
        const std::vector<Node*>& rawNodesToRemove = entry.second;
        if (rawNodesToRemove.empty()) {
            continue;
        }

        std::vector<Node*> nodesToRemove(rawNodesToRemove.begin(), rawNodesToRemove.end());
        while (!nodesToRemove.empty()) {
            std::vector<Node*> filteredNodes = JobUtils::filterNodesForPendingAllocation(
                nodesToRemove,
                pendingJob,
                static_cast<int>(nodesToRemove.size()),
                currentTime);
            if (filteredNodes.size() == nodesToRemove.size()) {
                break;
            }
            nodesToRemove = std::move(filteredNodes);
        }

        if (nodesToRemove.empty()) {
            continue;
        }
        
        // 1. Create agreement (reserve nodes for pending job)
        agreements.addAgreement(pendingJob, nodesToRemove);
        
        // 2. Log agreement event
        EventLogger::logAgreementAdded(job, pendingJob, nodesToRemove);
        XBT_DEBUG("Agreement added: pending job %d reserved %zu node(s) from job %d",
                 pendingJob->getId(), nodesToRemove.size(), job->getId());
        
        // 3. Remove nodes from job
        job->removeNodes(nodesToRemove);
        PerformanceTracer::recordShrink(1, nodesToRemove.size());
        
        // 4. Log shrink event
        EventLogger::logJobShrink(job, nodesToRemove);
        XBT_DEBUG("Job %d shrunk by %zu node(s)", job->getId(), nodesToRemove.size());
        
        XBT_DEBUG("Shrunk job %d by %zu nodes for pending job %d at t=%.1f",
                 job->getId(), nodesToRemove.size(), pendingJob->getId(),
                 simgrid::s4u::Engine::get_clock());
    }
}

void MalleabilityHelper::expandJob(Job* job, std::vector<Node*>& freeNodes) {
    if (!job || freeNodes.empty()) {
        return;
    }

    PerformanceTracer::ScopedPhase phase("expand_apply");
    const double currentTime = simgrid::s4u::Engine::get_clock();
    
    // 1. Calculate how many nodes to add
    int currentNodes = job->getAssignedNodes().size();
    int maxNodes = job->getNumNodesMax();
    int targetNodes = std::min(currentNodes + static_cast<int>(freeNodes.size()), maxNodes);
    std::vector<Node*> addedNodes;
    for (int candidateTarget = targetNodes; candidateTarget > currentNodes; --candidateTarget) {
        std::vector<Node*> eligibleNodes = JobUtils::filterNodesForRunningJob(
            freeNodes, job, candidateTarget, currentTime, true);
        int candidateNodesToAdd = candidateTarget - currentNodes;
        if (static_cast<int>(eligibleNodes.size()) < candidateNodesToAdd) {
            continue;
        }
        addedNodes.assign(eligibleNodes.begin(), eligibleNodes.begin() + candidateNodesToAdd);
        break;
    }
    
    if (addedNodes.empty()) {
        return;
    }
    int nodesToAdd = static_cast<int>(addedNodes.size());
    
    // 3. Assign nodes to job
    for (Node* node : addedNodes) {
        job->assignNode(node);
    }
    
    // 4. Remove from freeNodes
    for (Node* node : addedNodes) {
        auto it = std::find(freeNodes.begin(), freeNodes.end(), node);
        if (it != freeNodes.end()) {
            freeNodes.erase(it);
        }
    }
    
    // 5. Log expand event
    EventLogger::logJobExpand(job, addedNodes);
    PerformanceTracer::recordExpand(1, addedNodes.size());
    XBT_DEBUG("Job %d expanded by %d node(s)", job->getId(), nodesToAdd);

    // Trigger reconfiguration for running jobs (Python scheduler updates state on assignment)
    job->updateState();
    
    XBT_DEBUG("Expanded job %d by %d nodes (now %d/%d) at t=%.1f",
             job->getId(), nodesToAdd, currentNodes + nodesToAdd, maxNodes,
             simgrid::s4u::Engine::get_clock());
}

std::vector<Node*> MalleabilityHelper::allocateResourcesFromJob(
    Job* job,
    int requiredNodes,
    const AgreementHandler& agreements) {
    
    if (!job || requiredNodes <= 0) {
        return {};
    }
    
    // 1. Get available nodes for shrinking
    int availableNodes = getAvailableNodesForShrinking(job, agreements);
    
    if (availableNodes <= 0) {
        return {};
    }
    
    // 2. Take as many as needed (up to available)
    int nodesToTake = std::min(requiredNodes, availableNodes);
    
    // 3. Select nodes above min nodes without agreements
    const std::vector<Node*>& assignedNodes = job->getAssignedNodes();
    int minNodes = job->getNumNodesMin();
    std::vector<Node*> nodesToRemove;
    
    for (size_t i = static_cast<size_t>(minNodes); i < assignedNodes.size(); i++) {
        if (nodesToRemove.size() >= static_cast<size_t>(nodesToTake)) {
            break;
        }
        
        Node* node = assignedNodes[i];
        if (!agreements.hasAgreement(node)) {
            nodesToRemove.push_back(node);
        }
    }
    
    return nodesToRemove;
}

bool MalleabilityHelper::wouldDelayHead(
    Job* job,
    int reqNodes,
    Job* head,
    const std::vector<Job*>& runningJobs,
    const std::vector<Node*>& freeNodes,
    double currentTime) {
    
    if (!job || !head) {
        return false;
    }
    
    if (job == head) {
        return false;
    }
    
    int nodesNeeded = reqNodes - static_cast<int>(freeNodes.size());
    double headStartTime = currentTime;
    
    auto remainingRuntime = [currentTime](Job* runningJob) {
        return JobUtils::getPredictedRemainingRuntime(runningJob, currentTime);
    };
    
    std::vector<Job*> sortedJobs = runningJobs;
    std::stable_sort(sortedJobs.begin(), sortedJobs.end(),
                     [&remainingRuntime](Job* a, Job* b) {
                         return remainingRuntime(a) < remainingRuntime(b);
                     });
    
    for (Job* runningJob : sortedJobs) {
        if (nodesNeeded <= 0) {
            break;
        }
        nodesNeeded -= static_cast<int>(runningJob->getAssignedNodes().size());
        headStartTime = currentTime + remainingRuntime(runningJob);
    }
    
    return nodesNeeded <= 0 && headStartTime < JobUtils::getPredictedRuntimeForNodes(head, head->getNumNodesMin());
}

int MalleabilityHelper::getAvailableNodesForShrinking(
    Job* job,
    const AgreementHandler& agreements) {
    
    if (!job) {
        return 0;
    }
    
    const std::vector<Node*>& assignedNodes = job->getAssignedNodes();
    int minNodes = job->getNumNodesMin();
    
    if (static_cast<int>(assignedNodes.size()) <= minNodes) {
        return 0;
    }
    
    int nodesWithoutAgreement = 0;
    for (size_t i = static_cast<size_t>(minNodes); i < assignedNodes.size(); i++) {
        if (!agreements.hasAgreement(assignedNodes[i])) {
            nodesWithoutAgreement++;
        }
    }
    
    return nodesWithoutAgreement;
}
