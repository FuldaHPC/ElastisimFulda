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

// ============================================================================
// DEPRECATED: This implementation is kept for backward compatibility only.
// New code should use: src/scheduling/schedulers/RigidFifoScheduler
//
// Migration path:
// 1. Set "use_native_scheduler": true in your config
// 2. Set "native_scheduler_name": "rigid_fifo" (default)
// 3. The new RigidFifoScheduler provides identical behavior
// ============================================================================

#include "NativeFifoScheduler.h"
#include "Job.h"
#include "Node.h"
#include "PlatformManager.h"
#include "system/ShutdownPolicyManager.h"
#include <simgrid/s4u.hpp>
#include <algorithm>

XBT_LOG_NEW_DEFAULT_CATEGORY(NativeFifoScheduler, "Messages within the native FIFO scheduler");

std::vector<Job*> NativeFifoScheduler::schedule(
    InvocationType invocationType,
    const std::vector<Job*>& jobQueue,
    const std::vector<Job*>& modifiedJobs,
    const Job* requestingJob,
    int numberOfNodes) {
    
    std::vector<Job*> scheduledJobs;
    std::vector<Node*> nodesAssignedThisRound;  // Track nodes assigned during this scheduling round
    
    XBT_DEBUG("Native FIFO Scheduler invoked: type=%d, queue_size=%zu, modified=%zu",
              invocationType, jobQueue.size(), modifiedJobs.size());
    
    // Simple FIFO: Try to allocate jobs in submission order (queue order)
    for (Job* job : jobQueue) {
        // Only consider PENDING jobs (not already running/completed/killed)
        if (job->getState() != PENDING) {
            continue;
        }
        
        // Get required number of nodes from the job JSON used by the refactored schedulers.
        nlohmann::json jobJson = job->toJson();
        int requiredNodes = -1;
        
        // Handle different job types
        if (job->getType() == RIGID) {
            requiredNodes = jobJson["num_nodes"];
        } else {
            // For flexible jobs (moldable/malleable/evolving/adaptive)
            // Use minimum for now - proper implementation would need more logic
            requiredNodes = jobJson["num_nodes_min"];
            XBT_DEBUG("Flexible job %d: using num_nodes_min=%d", job->getId(), requiredNodes);
        }
        
        if (requiredNodes <= 0) {
            XBT_WARN("Job %d has invalid node count: %d, skipping", job->getId(), requiredNodes);
            continue;
        }
        
        // Find available nodes (excluding ones already assigned this round)
        std::vector<Node*> availableNodes = findAvailableNodes(job, requiredNodes, nodesAssignedThisRound);
        
        if (availableNodes.size() >= static_cast<size_t>(requiredNodes)) {
            // Allocate nodes to job
            job->clearAssignedNodes();
            for (int i = 0; i < requiredNodes; i++) {
                job->assignNode(availableNodes[i]);
                nodesAssignedThisRound.push_back(availableNodes[i]);  // Track assignment
            }
            
            // For non-rigid jobs, also need to set GPU count
            if (job->getType() != RIGID) {
                int gpusPerNode = jobJson["num_gpus_per_node_min"];
                job->assignNumGpusPerNode(gpusPerNode);
            }
            
            // Mark job for allocation
            job->setState(PENDING_ALLOCATION);
            scheduledJobs.push_back(job);
            
            XBT_INFO("Scheduled job %d on %d nodes (FIFO)", job->getId(), requiredNodes);
        } else {
            // Not enough nodes available, FIFO blocks here
            // (no backfilling - wait for current jobs to finish)
            XBT_DEBUG("Not enough nodes for job %d (need %d, have %zu), blocking",
                     job->getId(), requiredNodes, availableNodes.size());
            break;  // Stop trying to schedule - FIFO waits for head of queue
        }
    }
    
    XBT_DEBUG("Native FIFO Scheduler completed: scheduled %zu jobs", scheduledJobs.size());
    
    return scheduledJobs;
}

std::vector<Node*> NativeFifoScheduler::findAvailableNodes(const Job* job,
                                                           int requiredNodes, 
                                                           const std::vector<Node*>& alreadyAssigned) {
    // Get ALL compute nodes - we must scan all to find free ones
    // Native schedulers scan all compute nodes to avoid depending on incremental
    // modified-node bookkeeping from the removed external scheduler interface.
    const std::vector<Node*>& allNodes = PlatformManager::getComputeNodes();
    
    std::vector<Node*> availableNodes;
    
    // Build set of already assigned nodes for fast lookup
    std::set<Node*> assignedSet(alreadyAssigned.begin(), alreadyAssigned.end());
    
    // Iterate in platform order (nodes are added to computeNodes in ID order)
    for (Node* node : allNodes) {
        // Skip nodes already assigned in this scheduling round
        if (assignedSet.find(node) != assignedSet.end()) {
            continue;
        }
        
        // Get node state from JSON (state is an enum integer: 0=free, 1=allocated, 2=reserved, 3=draining, 4=shutting_down, 5=off, 6=booting)
        nlohmann::json nodeJson = node->toJson();
        int state = nodeJson["state"];
        
        // Node is available if it's in NODE_FREE (0) state
        if (state == 0) {  // NODE_FREE
            // Check shutdown policy if active
            if (ShutdownPolicyManager::isActive()) {
                double currentTime = simgrid::s4u::Engine::get_clock();
                // Use job's predicted_runtime from attributes (ML prediction with error variance)
                // Note: walltime is set to 0 to disable WalltimeMonitor killing jobs
                nlohmann::json jobJson = job->toJson();
                double jobRuntime = 3600.0; // Default: 1 hour
                if (jobJson.contains("attributes") && jobJson["attributes"].contains("predicted_runtime")) {
                    // attributes is stored as map<string, string>, so we need to get as string and convert
                    std::string runtimeStr = jobJson["attributes"]["predicted_runtime"];
                    jobRuntime = std::stod(runtimeStr);
                }
                if (!ShutdownPolicyManager::canNodeAcceptJob(node, jobRuntime, currentTime)) {
                    continue; // Skip nodes that cannot accept jobs due to shutdown policy
                }
            }
            
            availableNodes.push_back(node);
            
            // Stop once we have enough
            if (availableNodes.size() >= static_cast<size_t>(requiredNodes)) {
                break;
            }
        }
    }
    
    return availableNodes;
}

bool NativeFifoScheduler::canAllocateJob(const Job* job) {
    nlohmann::json jobJson = job->toJson();
    int requiredNodes = -1;
    
    if (job->getType() == RIGID) {
        requiredNodes = jobJson["num_nodes"];
    } else {
        // For flexible jobs: use minimum
        requiredNodes = jobJson["num_nodes_min"];
    }
    
    if (requiredNodes <= 0) {
        return false;  // Invalid job specification
    }
    
    std::vector<Node*> emptyAssigned;  // No nodes assigned yet when just checking
    std::vector<Node*> available = findAvailableNodes(job, requiredNodes, emptyAssigned);
    return available.size() >= static_cast<size_t>(requiredNodes);
}
