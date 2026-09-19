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
// DEPRECATED: This scheduler is kept for backward compatibility only.
// Please use: src/scheduling/schedulers/RigidFifoScheduler instead.
//
// The new scheduler uses the refactored infrastructure with:
// - JobUtils, JobQueue, NodeAllocator utilities
// - IScheduler interface for extensibility
// - SchedulerRegistry for dynamic scheduler selection
// - Cleaner separation of concerns (~80 lines vs 170 lines)
// ============================================================================

#ifndef ELASTISIM_NATIVE_FIFO_SCHEDULER_H
#define ELASTISIM_NATIVE_FIFO_SCHEDULER_H

#include <vector>
#include "Scheduler.h"

class Job;
class Node;

/**
 * Native FIFO Scheduler - runs entirely within ElastiSim.
 * 
 * Implements a simple First-In-First-Out scheduling policy:
 * - Jobs are scheduled in submission order
 * - Allocates first N available nodes to each job
 * - No backfilling, no priorities
 * 
 * This implementation is retained for compatibility with old native configs.
 */
class NativeFifoScheduler {

public:
    /**
     * Main scheduling function - called from Scheduler.cpp
     * 
     * @param invocationType Why scheduling was invoked (job_submit, periodic, etc.)
     * @param jobQueue All pending and running jobs
     * @param modifiedJobs Jobs that changed state since last invocation
     * @param requestingJob Job that triggered this scheduling call (can be nullptr)
     * @param numberOfNodes For evolving requests: requested number of nodes
     * @return Vector of jobs that were scheduled (state changed to PENDING_ALLOCATION or PENDING_KILL)
     */
    static std::vector<Job*> schedule(
        InvocationType invocationType,
        const std::vector<Job*>& jobQueue,
        const std::vector<Job*>& modifiedJobs,
        const Job* requestingJob,
        int numberOfNodes
    );

private:
    /**
     * Find available nodes for job allocation
     * @param job Job being scheduled (for walltime info)
     * @param requiredNodes Number of nodes needed
     * @param alreadyAssigned Nodes already assigned in this scheduling round
     * @return Vector of available Node pointers, empty if not enough nodes
     */
    static std::vector<Node*> findAvailableNodes(const Job* job,
                                                 int requiredNodes, 
                                                 const std::vector<Node*>& alreadyAssigned);

    /**
     * Check if a job can be allocated (enough free nodes)
     * @param job Job to check
     * @return true if job can be allocated now
     */
    static bool canAllocateJob(const Job* job);
};

#endif //ELASTISIM_NATIVEFIFOSCHEDUL_H
