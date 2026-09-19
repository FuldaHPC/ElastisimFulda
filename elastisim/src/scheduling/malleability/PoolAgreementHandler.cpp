/*
 * This file is part of the ElastiSim SC26 research extension
 * (scheduler-integrated recurring node shutdown exploiting malleable jobs).
 *
 * C++ implementation adapted from the agreement handling in the Wagomu project's
 * MalleableJobScheduling, scheduling_algorithms/extension/AgreementHandler.py
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

#include "PoolAgreementHandler.h"
#include "../../software/Job.h"
#include "../../system/Node.h"
#include "../../system/ShutdownPolicyManager.h"
#include "../utils/JobUtils.h"
#include <algorithm>
#include <simgrid/s4u.hpp>

XBT_LOG_NEW_DEFAULT_CATEGORY(PoolAgreementHandler, "Pool agreement handler");

std::vector<Job*> PoolAgreementHandler::resolveAgreements(std::vector<Job*>& pendingJobs,
                                                            std::vector<Node*>& freeNodes,
                                                            std::vector<Node*>& nodesAssignedThisRound) {
    std::vector<Job*> startedJobs;
    
    // 1. Find jobs with agreements
    std::vector<Job*> targetJobs;
    for (Job* job : pendingJobs) {
        if (hasAgreement(job)) {
            targetJobs.push_back(job);
        }
    }
    
    // 2. Try to start each job using pool
    for (Job* job : targetJobs) {
        if (freeNodes.empty()) {
            break;
        }
        
        size_t nodesNeeded = getJobAgreementNodes(job).size();
        
        // Check if enough nodes available
        if (nodesNeeded <= freeNodes.size()) {
            double now = simgrid::s4u::Engine::get_clock();
            std::vector<Node*> eligibleFreeNodes = JobUtils::filterNodesForPendingAllocation(
                freeNodes, job, static_cast<int>(nodesNeeded), now);

            auto nodesToAssign = selectNodesFromPool(job, eligibleFreeNodes);
            if (nodesToAssign.size() < nodesNeeded) {
                continue;
            }

            Job* startedJob = applyAgreement(job, nodesToAssign, pendingJobs, freeNodes, nodesAssignedThisRound);
            if (startedJob != nullptr) {
                finalizePoolAssignments(job, nodesToAssign);
                startedJobs.push_back(startedJob);
                XBT_DEBUG("Agreement fulfilled (pool): job %d assigned %zu node(s)",
                         startedJob->getId(), nodesToAssign.size());
            }
        }
    }
    
    return startedJobs;
}

std::vector<Node*> PoolAgreementHandler::selectNodesFromPool(Job* job,
                                                             const std::vector<Node*>& freeNodes) const {
    const std::set<int>& agreementNodeIds = getJobAgreementNodes(job);
    size_t nodesNeeded = agreementNodeIds.size();
    
    std::vector<Node*> freeNodesWithAgreement;
    std::vector<int> freeNodeIdsWithAgreement;
    std::vector<Node*> freeNodesWithoutAgreement;
    
    for (Node* node : freeNodes) {
        if (hasAgreement(node)) {
            freeNodesWithAgreement.push_back(node);
            freeNodeIdsWithAgreement.push_back(node->getId());
        } else {
            freeNodesWithoutAgreement.push_back(node);
        }
    }
    
    // Use free agreement nodes first, then any free nodes
    std::vector<Node*> result;
    for (size_t i = 0; i < nodesNeeded && i < freeNodesWithAgreement.size(); i++) {
        result.push_back(freeNodesWithAgreement[i]);
    }
    size_t remaining = nodesNeeded - result.size();
    for (size_t i = 0; i < remaining && i < freeNodesWithoutAgreement.size(); i++) {
        result.push_back(freeNodesWithoutAgreement[i]);
    }
    
    return result;
}

void PoolAgreementHandler::finalizePoolAssignments(Job* job, const std::vector<Node*>& nodesToAssign) {
    if (!job) {
        return;
    }

    jobToNodeIds.erase(job->getId());

    for (Node* node : nodesToAssign) {
        if (!node) {
            continue;
        }
        int nodeId = node->getId();
        if (hasAgreement(node)) {
            removeNodeMapping(nodeId, false);
        } else if (!nodeToJobId.empty()) {
            popAnyNodeMapping(false);
        }
    }
}
