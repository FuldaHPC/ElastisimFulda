/*
 * This file is part of the ElastiSim SC26 research extension
 * (scheduler-integrated recurring node shutdown exploiting malleable jobs).
 *
 * C++ implementation adapted from the Wagomu project's MalleableJobScheduling strategy
 * scheduling_algorithms/rigid_shortest_job_first.py
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

#include "RigidShortestJobFirstScheduler.h"
#include "../utils/JobQueue.h"
#include "../utils/JobUtils.h"
#include "../utils/NodeAllocator.h"
#include "../utils/NodeCache.h"
#include "../../system/Node.h"
#include <algorithm>
#include <simgrid/s4u.hpp>

XBT_LOG_NEW_DEFAULT_CATEGORY(RigidShortestJobFirstScheduler, "Rigid Shortest-Job-First scheduler");

std::vector<Job*> RigidShortestJobFirstScheduler::schedule(const SchedulingContext& ctx) {
    ctx.validate();

    std::vector<Job*> scheduledJobs;
    std::vector<Node*> nodesAssignedThisRound;

    auto pending = JobQueue::getPending(ctx.jobQueue);

    auto runtimeKey = [](Job* job) -> double {
        return JobUtils::getPredictedRuntimeForNodes(job, job->getNumNodesPref());
    };

    std::stable_sort(pending.begin(), pending.end(),
                     [&runtimeKey](Job* a, Job* b) {
                         return runtimeKey(a) < runtimeKey(b);
                     });

    const std::vector<Node*>& freeNodes = NodeCache::getFreeNodes();
    std::vector<Node*> availableNodes = freeNodes;

    for (Job* job : pending) {
        int minNodes = job->getNumNodesMin();
        if (static_cast<int>(availableNodes.size()) < minNodes) {
            break;
        }

        int maxNodes = job->getNumNodesMax();
        int nodesToAssign = std::min(static_cast<int>(availableNodes.size()), maxNodes);
        if (nodesToAssign <= 0) {
            continue;
        }

        double predictedRuntime = JobUtils::getPredictedRuntimeForNodes(job, nodesToAssign);

        NodeAllocator::AllocationOptions options;
        options.requiredNodes = nodesToAssign;
        options.predictedRuntime = predictedRuntime;
        options.currentTime = ctx.currentTime;
        options.respectShutdownPolicy = true;

        auto allocatedNodes = NodeAllocator::findFreeNodes(options, nodesAssignedThisRound);
        if (allocatedNodes.empty()) {
            continue;
        }

        job->clearAssignedNodes();
        for (int i = 0; i < nodesToAssign; i++) {
            job->assignNode(allocatedNodes[i]);
            nodesAssignedThisRound.push_back(allocatedNodes[i]);
        }

        if (job->getType() != RIGID) {
            job->assignNumGpusPerNode(job->getNumGpusPerNodeMin());
        }

        job->setState(PENDING_ALLOCATION);
        scheduledJobs.push_back(job);

        for (Node* node : allocatedNodes) {
            auto it = std::find(availableNodes.begin(), availableNodes.end(), node);
            if (it != availableNodes.end()) {
                availableNodes.erase(it);
            }
        }
    }

    return scheduledJobs;
}
