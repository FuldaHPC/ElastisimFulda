/*
 * This file is part of the ElastiSim SC26 research extension
 * (scheduler-integrated recurring node shutdown exploiting malleable jobs).
 *
 * C++ implementation adapted from the shrink, expand and head-reservation helper routines shared
 * by the Wagomu project's MalleableJobScheduling strategies
 * (scheduling_algorithms/*.py)
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

#ifndef ELASTISIM_MALLEABILITYHELPER_H
#define ELASTISIM_MALLEABILITYHELPER_H

#include <vector>
#include <map>
#include <functional>

class Job;
class Node;
class AgreementHandler;

/**
 * Helper functions for malleable job scheduling.
 * Provides core shrink/expand logic for malleability-aware schedulers.
 * 
 * Python equivalent: Functions in malleable scheduler implementations
 */
class MalleabilityHelper {
public:
    // Priority function type: Job* -> priority value (higher = higher priority)
    using JobPriorityFunc = std::function<double(Job*)>;
    
    /**
     * Select jobs to shrink to free up resources.
     * 
     * Algorithm:
     * 1. Sort jobs by priority (highest first)
     * 2. Iteratively collect nodes from jobs
     * 3. Stop when enough nodes collected
     * 
     * @param runningMalleableJobs Jobs that can be shrunk
     * @param requiredNodes Number of nodes needed
     * @param priorityFunc Priority function (higher = shrink first)
     * @param agreements Agreement handler to check reservations
     * @return Map of job -> nodes to remove (empty if not enough resources)
     * 
     * Python equivalent:
     *   select_shrink_jobs(rm_jobs, required_nodes, priority_func, agreements)
     */
    static std::vector<std::pair<Job*, std::vector<Node*>>> selectShrinkJobs(
        const std::vector<Job*>& runningMalleableJobs,
        int requiredNodes,
        JobPriorityFunc priorityFunc,
        const AgreementHandler& agreements);

    /**
     * Select jobs to shrink using a pre-ordered list.
     * Avoids repeated sorting when the priority order is unchanged.
     *
     * @param orderedJobs Jobs already ordered by shrink priority (highest first)
     * @param requiredNodes Number of nodes needed
     * @param agreements Agreement handler to check reservations
     * @return Map of job -> nodes to remove (empty if not enough resources)
     */
    static std::vector<std::pair<Job*, std::vector<Node*>>> selectShrinkJobsOrdered(
        const std::vector<Job*>& orderedJobs,
        int requiredNodes,
        const AgreementHandler& agreements);
    
    /**
     * Apply shrinking: Remove nodes from jobs and create agreements.
     * 
     * For each job in jobsToShrink:
     * 1. Remove nodes from job (job->removeNodes())
     * 2. Create agreement (reserve nodes for pending job)
     * 3. Log shrink + agreement events
     * 
     * @param jobsToShrink Map of job -> nodes to remove
     * @param pendingJob Job that will receive the nodes
     * @param agreements Agreement handler to add reservations
     * 
     * Python equivalent:
     *   for job, nodes in jobs_to_shrink.items():
     *       job.remove(nodes)
     *       agreements.add_agreement(pending_job, nodes)
     */
    static void applyShrinking(
        const std::vector<std::pair<Job*, std::vector<Node*>>>& jobsToShrink,
        Job* pendingJob,
        AgreementHandler& agreements);
    
    /**
     * Expand a job with additional nodes.
     * 
     * Algorithm:
     * 1. Calculate how many nodes to add (up to max_nodes)
     * 2. Take nodes from freeNodes
     * 3. Assign nodes to job
     * 4. Log expand event
     * 
     * @param job Job to expand
     * @param freeNodes Available nodes (modified: removes used nodes)
     * 
     * Python equivalent:
     *   nodes_to_add = min(len(free_nodes), job.num_nodes_max - len(job.assigned_nodes))
     *   for node in free_nodes[:nodes_to_add]:
     *       job.assign(node)
     */
    static void expandJob(Job* job, std::vector<Node*>& freeNodes);
    
    /**
     * Allocate resources from a single job (for shrinking).
     * 
     * @param job Job to shrink
     * @param requiredNodes Number of nodes to take
     * @param agreements Agreement handler to check reservations
     * @return Nodes that can be removed (empty if can't shrink enough)
     * 
     * Python equivalent:
     *   allocate_resources(job, required_nodes, agreements)
     */
    static std::vector<Node*> allocateResourcesFromJob(
        Job* job,
        int requiredNodes,
        const AgreementHandler& agreements);
    
    /**
     * Check if scheduling a job would delay the queue head.
     * Used for backfilling decisions.
     * 
     * @param job Job to potentially schedule
     * @param reqNodes Nodes required for job
     * @param head Queue head job
     * @param runningJobs Currently running jobs
     * @param freeNodes Available nodes
     * @param currentTime Current simulation time
     * @return true if scheduling would delay head
     * 
     * Python equivalent:
     *   would_delay_head(job, req_nodes, head, r_jobs, f_nodes, system)
     */
    static bool wouldDelayHead(
        Job* job,
        int reqNodes,
        Job* head,
        const std::vector<Job*>& runningJobs,
        const std::vector<Node*>& freeNodes,
        double currentTime);

private:
    /**
     * Get number of nodes that can be removed from job.
     * Respects min_nodes constraint and existing agreements.
     */
    static int getAvailableNodesForShrinking(
        Job* job,
        const AgreementHandler& agreements);
};

#endif //ELASTISIM_MALLEABILITYHELPER_H
