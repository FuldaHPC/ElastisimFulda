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

#include "StealAgreementHandler.h"
#include "../../software/Job.h"
#include "../../system/Node.h"
#include "../../system/ShutdownPolicyManager.h"
#include "../utils/JobUtils.h"
#include <algorithm>
#include <simgrid/s4u.hpp>

XBT_LOG_NEW_DEFAULT_CATEGORY(StealAgreementHandler, "Steal agreement handler");

void StealAgreementHandler::swapNodes(int node1Id, int node2Id) {
    auto it1 = nodeToJobId.find(node1Id);
    auto it2 = nodeToJobId.find(node2Id);
    if (it1 == nodeToJobId.end() || it2 == nodeToJobId.end()) {
        return;
    }

    int job1Id = it1->second;
    int job2Id = it2->second;

    if (job1Id == job2Id) {
        return;
    }

    auto jt1 = jobToNodeIds.find(job1Id);
    auto jt2 = jobToNodeIds.find(job2Id);
    if (jt1 != jobToNodeIds.end()) {
        jt1->second.erase(node1Id);
        jt1->second.insert(node2Id);
    }
    if (jt2 != jobToNodeIds.end()) {
        jt2->second.erase(node2Id);
        jt2->second.insert(node1Id);
    }

    nodeToJobId[node1Id] = job2Id;
    nodeToJobId[node2Id] = job1Id;
}

void StealAgreementHandler::stealAgreementNodes(Job* job,
                                                const std::vector<Node*>& freeAgreementNodes) {
    if (!job) {
        return;
    }

    auto it = jobToNodeIds.find(job->getId());
    if (it == jobToNodeIds.end()) {
        return;
    }

    const std::set<int>& agreementNodeIds = it->second;
    std::vector<int> freeAgreementNodeIds;
    freeAgreementNodeIds.reserve(freeAgreementNodes.size());
    for (Node* node : freeAgreementNodes) {
        freeAgreementNodeIds.push_back(node->getId());
    }

    std::vector<int> usedAgreementNodeIds;
    std::vector<int> freeOtherAgreementNodeIds;

    for (int nodeId : agreementNodeIds) {
        if (std::find(freeAgreementNodeIds.begin(),
                      freeAgreementNodeIds.end(),
                      nodeId) == freeAgreementNodeIds.end()) {
            usedAgreementNodeIds.push_back(nodeId);
        }
    }

    for (int nodeId : freeAgreementNodeIds) {
        if (agreementNodeIds.find(nodeId) == agreementNodeIds.end()) {
            freeOtherAgreementNodeIds.push_back(nodeId);
        }
    }

    size_t swapCount = std::min(usedAgreementNodeIds.size(),
                                freeOtherAgreementNodeIds.size());
    for (size_t i = 0; i < swapCount; ++i) {
        swapNodes(usedAgreementNodeIds[i], freeOtherAgreementNodeIds[i]);
    }
}

std::vector<Job*> StealAgreementHandler::resolveAgreements(std::vector<Job*>& pendingJobs,
                                                           std::vector<Node*>& freeNodes,
                                                           std::vector<Node*>& nodesAssignedThisRound) {
    std::vector<Job*> startedJobs;

    std::vector<Job*> targetJobs;
    for (Job* job : pendingJobs) {
        if (hasAgreement(job)) {
            targetJobs.push_back(job);
        }
    }

    for (Job* job : targetJobs) {
        if (freeNodes.empty()) {
            break;
        }

        std::vector<Node*> freeAgreementNodes;
        for (Node* node : freeNodes) {
            if (hasAgreement(node)) {
                freeAgreementNodes.push_back(node);
            }
        }

        if (freeAgreementNodes.empty()) {
            break;
        }

        size_t nodesNeeded = getJobAgreementNodes(job).size();
        if (nodesNeeded > freeAgreementNodes.size()) {
            continue;
        }

        stealAgreementNodes(job, freeAgreementNodes);

        std::vector<Node*> nodesToAssign;
        nodesToAssign.reserve(nodesNeeded);
        const auto& agreementNodes = getJobAgreementNodes(job);
        for (Node* node : freeAgreementNodes) {
            if (agreementNodes.find(node->getId()) != agreementNodes.end()) {
                nodesToAssign.push_back(node);
            }
        }

        if (nodesToAssign.size() < nodesNeeded) {
            continue;
        }

        double now = simgrid::s4u::Engine::get_clock();
        std::vector<Node*> eligibleNodes = JobUtils::filterNodesForPendingAllocation(
            nodesToAssign, job, static_cast<int>(nodesNeeded), now);
        const bool allowed = eligibleNodes.size() == nodesNeeded;

        if (!allowed) {
            if (freeAgreementNodes.size() >= nodesNeeded) {
                removeAgreement(job);
            }
            continue;
        }

        Job* started = applyAgreement(job, nodesToAssign, pendingJobs, freeNodes, nodesAssignedThisRound);
        if (started != nullptr) {
            startedJobs.push_back(started);
            removeAgreement(job);
        }
    }

    return startedJobs;
}
