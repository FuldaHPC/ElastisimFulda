/*
 * This file is part of the ElastiSim SC26 research extension
 * (scheduler-integrated recurring node shutdown exploiting malleable jobs).
 *
 * C++ implementation adapted from the Wagomu project's MalleableJobScheduling strategy
 * scheduling_algorithms/rigid_backfill.py
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

#include "RigidBackfillScheduler.h"
#include "../utils/JobQueue.h"
#include "../utils/NodeAllocator.h"
#include "../utils/JobUtils.h"
#include "../utils/NodeCache.h"
#include "../../system/Node.h"
#include <simgrid/s4u.hpp>

XBT_LOG_NEW_DEFAULT_CATEGORY(RigidBackfillScheduler, "Rigid Backfill scheduler");

std::vector<Job*> RigidBackfillScheduler::schedule(const SchedulingContext& ctx) {
    ctx.validate();
    
    std::vector<Job*> scheduledJobs;
    std::vector<Node*> nodesAssignedThisRound;
    
    XBT_DEBUG("RigidBackfillScheduler invoked: type=%d, queue_size=%zu",
              ctx.invocationType, ctx.jobQueue.size());
    
    // Get pending jobs
    auto pending = JobQueue::getPending(ctx.jobQueue);
    
    // Sort by submit time (FIFO order)
    JobQueue::sortBySubmitTime(pending);
    
    // Get free nodes via cache
    const std::vector<Node*>& freeNodes = NodeCache::getFreeNodes();
    std::vector<Node*> availableNodes = freeNodes;  // Copy for modification
    
    XBT_DEBUG("Found %zu pending jobs, %zu free nodes", pending.size(), availableNodes.size());
    
    // BACKFILL LOOP: Try to schedule each job (no blocking!)
    for (Job* job : pending) {
        // If no nodes left, stop
        if (availableNodes.empty()) {
            break;
        }
        
        // Get job requirements
        int requiredNodes = JobUtils::getRequiredNodes(job);
        if (requiredNodes <= 0) {
            XBT_WARN("Job %d has invalid node count: %d, skipping", job->getId(), requiredNodes);
            continue;
        }
        
        // BACKFILL: Skip if not enough nodes, but continue checking others
        if (requiredNodes > static_cast<int>(availableNodes.size())) {
            XBT_DEBUG("Job %d needs %d nodes, only %zu available - SKIPPING (backfill)",
                     job->getId(), requiredNodes, availableNodes.size());
            continue;  // ← KEY DIFFERENCE from FIFO: Don't block!
        }
        
        // Get predicted runtime for shutdown policy
        double predictedRuntime = JobUtils::getPredictedRuntimeForNodes(job, requiredNodes);
        
        // Find available nodes (with policy check)
        NodeAllocator::AllocationOptions options;
        options.requiredNodes = requiredNodes;
        options.predictedRuntime = predictedRuntime;
        options.currentTime = ctx.currentTime;
        options.respectShutdownPolicy = true;
        
        auto allocatedNodes = NodeAllocator::findFreeNodes(options, nodesAssignedThisRound);
        
        if (allocatedNodes.empty()) {
            XBT_DEBUG("Job %d: NodeAllocator returned no nodes (shutdown policy)", job->getId());
            continue;  // Shutdown policy blocked - try next job
        }
        
        // Allocate nodes to job
        job->clearAssignedNodes();
        for (int i = 0; i < requiredNodes; i++) {
            job->assignNode(allocatedNodes[i]);
            nodesAssignedThisRound.push_back(allocatedNodes[i]);
        }
        
        // Handle GPU assignment for non-rigid jobs
        if (job->getType() != RIGID) {
            job->assignNumGpusPerNode(job->getNumGpusPerNodeMin());
        }
        
        job->setState(PENDING_ALLOCATION);
        scheduledJobs.push_back(job);
        
        // Remove allocated nodes from available pool
        for (Node* node : allocatedNodes) {
            auto it = std::find(availableNodes.begin(), availableNodes.end(), node);
            if (it != availableNodes.end()) {
                availableNodes.erase(it);
            }
        }
        
        XBT_INFO("Scheduled job %d on %d nodes (backfill)", job->getId(), requiredNodes);
    }
    
    XBT_DEBUG("RigidBackfillScheduler completed: scheduled %zu jobs", scheduledJobs.size());
    
    return scheduledJobs;
}
