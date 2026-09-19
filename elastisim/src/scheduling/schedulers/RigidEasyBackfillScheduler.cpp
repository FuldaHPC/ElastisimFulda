/*
 * This file is part of the ElastiSim SC26 research extension
 * (scheduler-integrated recurring node shutdown exploiting malleable jobs).
 *
 * C++ implementation adapted from the Wagomu project's MalleableJobScheduling strategy
 * scheduling_algorithms/rigid_easy_backfill.py
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

#include "RigidEasyBackfillScheduler.h"
#include "../utils/JobQueue.h"
#include "../utils/JobUtils.h"
#include "../utils/NodeAllocator.h"
#include "../utils/NodeCache.h"
#include "../../system/Node.h"
#include "../../util/PerformanceTracer.h"
#include <algorithm>
#include <simgrid/s4u.hpp>

XBT_LOG_NEW_DEFAULT_CATEGORY(RigidEasyBackfillScheduler, "Rigid Easy-Backfill scheduler");

std::vector<Job*> RigidEasyBackfillScheduler::schedule(const SchedulingContext& ctx) {
    ctx.validate();

    std::vector<Job*> scheduledJobs;
    std::vector<Node*> nodesAssignedThisRound;

    auto pending = JobQueue::getPending(ctx.jobQueue);
    auto running = JobQueue::getRunning(ctx.jobQueue);
    JobQueue::sortBySubmitTime(pending);

    const std::vector<Node*>& freeNodes = NodeCache::getFreeNodes();
    std::vector<Node*> availableNodes = freeNodes;

    auto pendingRuntime = [](Job* job) -> double {
        return JobUtils::getPredictedRuntimeForNodes(job, job->getNumNodesPref());
    };

    std::vector<Job*> sortedRunning = running;
    auto remainingRuntime = [&](Job* runningJob) -> double {
        return JobUtils::getPredictedRemainingRuntime(runningJob, ctx.currentTime);
    };
    {
        PerformanceTracer::ScopedPhase phase("sort_running");
        std::stable_sort(sortedRunning.begin(), sortedRunning.end(),
                         [&remainingRuntime](Job* a, Job* b) {
                             return remainingRuntime(a) < remainingRuntime(b);
                         });
    }

    auto delaysHead = [&](Job* job, int reqNodes, Job* head) -> bool {
        if (!job || !head || job == head) {
            return false;
        }

        int nodesNeeded = reqNodes - static_cast<int>(availableNodes.size());
        double headStartTime = ctx.currentTime;

        for (Job* runningJob : sortedRunning) {
            if (nodesNeeded <= 0) {
                break;
            }
            nodesNeeded -= static_cast<int>(runningJob->getAssignedNodes().size());
            headStartTime = ctx.currentTime + remainingRuntime(runningJob);
        }

        return nodesNeeded <= 0 && headStartTime < pendingRuntime(head);
    };

    for (Job* job : pending) {
        if (availableNodes.empty()) {
            break;
        }

        int requiredNodes = job->getNumNodesPref();
        if (requiredNodes <= 0) {
            continue;
        }

        if (requiredNodes <= static_cast<int>(availableNodes.size())) {
            bool delayed = false;
            {
                PerformanceTracer::ScopedPhase phase("head_delay_check");
                delayed = delaysHead(job, requiredNodes, pending.front());
            }
            if (delayed) {
                continue;
            }

            double predictedRuntime = JobUtils::getPredictedRuntimeForNodes(job, requiredNodes);

            NodeAllocator::AllocationOptions options;
            options.requiredNodes = requiredNodes;
            options.predictedRuntime = predictedRuntime;
            options.currentTime = ctx.currentTime;
            options.respectShutdownPolicy = true;

            std::vector<Node*> allocatedNodes;
            {
                PerformanceTracer::ScopedPhase phase("initial_allocation");
                allocatedNodes = NodeAllocator::findFreeNodes(options, nodesAssignedThisRound);
            }
            if (allocatedNodes.empty()) {
                continue;
            }

            job->clearAssignedNodes();
            for (int i = 0; i < requiredNodes; i++) {
                job->assignNode(allocatedNodes[i]);
                nodesAssignedThisRound.push_back(allocatedNodes[i]);
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
    }

    return scheduledJobs;
}
