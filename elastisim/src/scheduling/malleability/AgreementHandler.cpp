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

#include "AgreementHandler.h"
#include "../../software/Job.h"
#include "../../system/Node.h"
#include "../../util/EventLogger.h"
#include "../../util/PerformanceTracer.h"
#include <algorithm>

void AgreementHandler::addAgreement(Job* job, const std::vector<Node*>& nodes) {
    if (nodes.empty() || job == nullptr) {
        return;
    }
    
    int jobId = job->getId();
    
    // Add nodes to job's reservation set
    for (Node* node : nodes) {
        int nodeId = node->getId();
        auto existing = nodeToJobId.find(nodeId);
        if (existing != nodeToJobId.end()) {
            int oldJobId = existing->second;
            if (oldJobId != jobId) {
                auto it = jobToNodeIds.find(oldJobId);
                if (it != jobToNodeIds.end()) {
                    it->second.erase(nodeId);
                    if (it->second.empty()) {
                        jobToNodeIds.erase(it);
                    }
                }
            }
        } else {
            nodeAgreementOrder.push_back(nodeId);
        }
        jobToNodeIds[jobId].insert(nodeId);
        nodeToJobId[nodeId] = jobId;
    }
    PerformanceTracer::recordAgreementAdded();
}

void AgreementHandler::removeAgreement(Job* job) {
    if (job == nullptr) {
        return;
    }
    
    int jobId = job->getId();
    
    // Find all nodes reserved for this job
    auto it = jobToNodeIds.find(jobId);
    if (it != jobToNodeIds.end()) {
        // Copy node IDs to avoid invalidation while updating maps.
        std::vector<int> nodeIds(it->second.begin(), it->second.end());
        for (int nodeId : nodeIds) {
            removeNodeMapping(nodeId, false);
        }
        jobToNodeIds.erase(it);
    }
}

bool AgreementHandler::hasAgreement(Job* job) const {
    if (job == nullptr) {
        return false;
    }
    return jobToNodeIds.count(job->getId()) > 0;
}

bool AgreementHandler::hasAgreement(Node* node) const {
    if (node == nullptr) {
        return false;
    }
    return nodeToJobId.count(node->getId()) > 0;
}

const std::set<int>& AgreementHandler::getJobAgreementNodes(Job* job) const {
    static const std::set<int> emptySet;
    
    if (job == nullptr) {
        return emptySet;
    }
    
    auto it = jobToNodeIds.find(job->getId());
    if (it != jobToNodeIds.end()) {
        return it->second;
    }
    return emptySet;
}

Job* AgreementHandler::applyAgreement(Job* jobToStart, 
                                       const std::vector<Node*>& nodesToAssign,
                                       std::vector<Job*>& pendingJobs,
                                       std::vector<Node*>& freeNodes,
                                       std::vector<Node*>& nodesAssignedThisRound) {
    if (jobToStart == nullptr || nodesToAssign.empty()) {
        return nullptr;
    }
    
    // 1. Assign nodes to job
    for (Node* node : nodesToAssign) {
        jobToStart->assignNode(node);
        nodesAssignedThisRound.push_back(node);  // Track assigned nodes
    }

    if (jobToStart->getType() != RIGID) {
        jobToStart->assignNumGpusPerNode(jobToStart->getNumGpusPerNodeMax());
    }
    
    // 2. Mark job as pending allocation (ready to start)
    jobToStart->setState(PENDING_ALLOCATION);
    
    // 3. Remove job from pending queue
    auto jobIt = std::find(pendingJobs.begin(), pendingJobs.end(), jobToStart);
    if (jobIt != pendingJobs.end()) {
        pendingJobs.erase(jobIt);
    }
    
    // 4. Remove nodes from free list
    for (Node* node : nodesToAssign) {
        auto nodeIt = std::find(freeNodes.begin(), freeNodes.end(), node);
        if (nodeIt != freeNodes.end()) {
            freeNodes.erase(nodeIt);
        }
    }
    
    // 5. Log agreement fulfilled event
    EventLogger::logAgreementFulfilled(jobToStart, nodesToAssign);
    PerformanceTracer::recordAgreementFulfilled();
    
    return jobToStart;
}

void AgreementHandler::removeNodeMapping(int nodeId, bool updateJobMap) {
    if (updateJobMap) {
        auto it = nodeToJobId.find(nodeId);
        if (it != nodeToJobId.end()) {
            int jobId = it->second;
            auto jt = jobToNodeIds.find(jobId);
            if (jt != jobToNodeIds.end()) {
                jt->second.erase(nodeId);
                if (jt->second.empty()) {
                    jobToNodeIds.erase(jt);
                }
            }
        }
    }

    nodeToJobId.erase(nodeId);
    auto orderIt = std::find(nodeAgreementOrder.begin(), nodeAgreementOrder.end(), nodeId);
    if (orderIt != nodeAgreementOrder.end()) {
        nodeAgreementOrder.erase(orderIt);
    }
}

int AgreementHandler::popAnyNodeMapping(bool updateJobMap) {
    if (nodeAgreementOrder.empty()) {
        return -1;
    }
    int nodeId = nodeAgreementOrder.back();
    removeNodeMapping(nodeId, updateJobMap);
    return nodeId;
}
