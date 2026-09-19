/*
 * This file is part of the ElastiSim SC26 research extension
 * (scheduler-integrated recurring node shutdown exploiting malleable jobs).
 *
 * C++ implementation adapted from the Wagomu project's MalleableJobScheduling strategy
 * scheduling_algorithms/rigid_fifo.py
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

#include "RigidFifoScheduler.h"
#include "../utils/JobQueue.h"
#include "../utils/NodeAllocator.h"
#include "../utils/JobUtils.h"
#include "../../system/Node.h"
#include <simgrid/s4u.hpp>

XBT_LOG_NEW_DEFAULT_CATEGORY(RigidFifoScheduler, "Refactored FIFO scheduler");

std::vector<Job*> RigidFifoScheduler::schedule(const SchedulingContext& ctx) {
    
    ctx.validate();
    
    std::vector<Job*> scheduledJobs;
    std::vector<Node*> nodesAssignedThisRound;
    
    XBT_DEBUG("RigidFifoScheduler invoked: type=%d, queue_size=%zu",
              ctx.invocationType, ctx.jobQueue.size());
    
    // Get pending jobs
    auto pendingJobs = JobQueue::getPending(ctx.jobQueue);
    
    // Sort by submit time
    JobQueue::sortBySubmitTime(pendingJobs);
    
    // Try to schedule each job in FIFO order
    for (Job* job : pendingJobs) {
        // Get job requirements
        int requiredNodes = JobUtils::getRequiredNodes(job);
        if (requiredNodes <= 0) {
            XBT_WARN("Job %d has invalid node count: %d, skipping", job->getId(), requiredNodes);
            continue;
        }
        
        // Get predicted runtime for shutdown policy
        double predictedRuntime = JobUtils::getPredictedRuntimeForNodes(job, requiredNodes);
        
        // Find available nodes (with policy check)
        NodeAllocator::AllocationOptions options;
        options.requiredNodes = requiredNodes;
        options.predictedRuntime = predictedRuntime;
        options.currentTime = ctx.currentTime;
        options.respectShutdownPolicy = true;
        
        auto availableNodes = NodeAllocator::findFreeNodes(options, nodesAssignedThisRound);
        
        if (availableNodes.size() >= static_cast<size_t>(requiredNodes)) {
            // Allocate nodes to job
            job->clearAssignedNodes();
            for (int i = 0; i < requiredNodes; i++) {
                job->assignNode(availableNodes[i]);
                nodesAssignedThisRound.push_back(availableNodes[i]);
            }
            
            // Handle GPU assignment for non-rigid jobs
            if (job->getType() != RIGID) {
                job->assignNumGpusPerNode(job->getNumGpusPerNodeMin());
            }
            
            job->setState(PENDING_ALLOCATION);
            scheduledJobs.push_back(job);
            
            XBT_INFO("Scheduled job %d on %d nodes (FIFO)", job->getId(), requiredNodes);
        } else {
            // Not enough nodes - FIFO blocks here (no backfilling)
            XBT_DEBUG("Not enough nodes for job %d (need %d, have %zu), blocking",
                     job->getId(), requiredNodes, availableNodes.size());
            break;
        }
    }
    
    XBT_DEBUG("RigidFifoScheduler completed: scheduled %zu jobs", scheduledJobs.size());
    
    return scheduledJobs;
}
