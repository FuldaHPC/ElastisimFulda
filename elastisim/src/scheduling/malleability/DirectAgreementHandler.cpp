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

#include "DirectAgreementHandler.h"
#include "../../software/Job.h"
#include "../../system/Node.h"
#include "../../system/ShutdownPolicyManager.h"
#include "../utils/JobUtils.h"
#include <algorithm>
#include <simgrid/s4u.hpp>

std::vector<Job*> DirectAgreementHandler::resolveAgreements(std::vector<Job*>& pendingJobs,
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
    
    // 2. For each job: Check if all agreement nodes are free
    for (Job* job : targetJobs) {
        const std::set<int>& agreementNodeIds = getJobAgreementNodes(job);
        
        // Collect free nodes that match agreement
        std::vector<Node*> freeAgreementNodes;
        for (Node* node : freeNodes) {
            if (agreementNodeIds.count(node->getId())) {
                freeAgreementNodes.push_back(node);
            }
        }
        
        // 3. If ALL agreement nodes are free: Start job
        if (freeAgreementNodes.size() == agreementNodeIds.size()) {
            double now = simgrid::s4u::Engine::get_clock();
            std::vector<Node*> eligibleNodes = JobUtils::filterNodesForPendingAllocation(
                freeAgreementNodes, job, static_cast<int>(agreementNodeIds.size()), now);
            const bool allowed = eligibleNodes.size() == agreementNodeIds.size();

            if (allowed) {
                Job* startedJob = applyAgreement(job, freeAgreementNodes, pendingJobs, freeNodes, nodesAssignedThisRound);
                if (startedJob != nullptr) {
                    startedJobs.push_back(startedJob);
                    removeAgreement(job);
                }
            } else {
                // Agreement nodes are free but blocked by policy; drop agreement to avoid deadlock.
                removeAgreement(job);
            }
        }
        // If not all nodes free: Wait (don't start job)
    }
    
    return startedJobs;
}
