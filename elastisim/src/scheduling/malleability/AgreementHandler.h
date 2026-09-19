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

#ifndef ELASTISIM_AGREEMENTHANDLER_H
#define ELASTISIM_AGREEMENTHANDLER_H

#include <map>
#include <set>
#include <vector>

class Job;
class Node;

/**
 * Base class for agreement handlers (Strategy Pattern).
 * 
 * Agreements track resource reservations:
 * - When a job shrinks, nodes are reserved for a specific pending job
 * - The agreement handler decides when/how to fulfill these reservations
 * 
 * Python equivalent: AgreementHandler class in AgreementHandler.py
 */
class AgreementHandler {
protected:
    // Job ID -> Set of reserved Node IDs
    std::map<int, std::set<int>> jobToNodeIds;
    
    // Node ID -> Job ID that reserved this node
    std::map<int, int> nodeToJobId;

    // Preserve insertion order to emulate Python dict popitem()
    std::vector<int> nodeAgreementOrder;

public:
    virtual ~AgreementHandler() = default;
    
    /**
     * Add agreement: Reserve nodes for a specific job.
     * 
     * @param job Job that will receive the nodes
     * @param nodes Nodes that are reserved for this job
     * 
     * Python equivalent:
     *   agreements.add_agreement(job, nodes)
     */
    void addAgreement(Job* job, const std::vector<Node*>& nodes);
    
    /**
     * Remove agreement for a job (cleanup after fulfillment).
     * 
     * Python equivalent:
     *   agreements.remove_agreement(job)
     */
    void removeAgreement(Job* job);
    
    /**
     * Check if job/node has an agreement.
     * 
     * Python equivalent:
     *   agreements.has_agreement(obj)
     */
    bool hasAgreement(Job* job) const;
    bool hasAgreement(Node* node) const;
    
    /**
     * Get node IDs reserved for a job.
     * 
     * Python equivalent:
     *   agreements.get_job_agreement_nodes(job)
     */
    const std::set<int>& getJobAgreementNodes(Job* job) const;
    
    /**
     * Resolve agreements: Try to start jobs with reserved nodes.
     * 
     * Different strategies implement this differently:
     * - DirectAgreementHandler: Wait for exact nodes
     * - PoolAgreementHandler: Use any available nodes
     * - StealAgreementHandler: Can swap reservations
     * 
     * @param pendingJobs Jobs waiting to be scheduled (modified in-place)
     * @param freeNodes Available nodes (modified in-place)
     * @param nodesAssignedThisRound Nodes assigned across all phases (prevents double-assignment)
     * @return Jobs that were started via agreement resolution
     * 
     * Python equivalent:
     *   agreements.resolve_agreements(p_jobs, f_nodes)
     */
    virtual std::vector<Job*> resolveAgreements(std::vector<Job*>& pendingJobs,
                                                 std::vector<Node*>& freeNodes,
                                                 std::vector<Node*>& nodesAssignedThisRound) = 0;

protected:
    /**
     * Helper: Apply agreement by starting a job.
     * 
     * Removes job from pendingJobs, removes nodes from freeNodes,
     * assigns nodes to job, and logs fulfillment event.
     * Also tracks assigned nodes in nodesAssignedThisRound.
     * 
     * @param nodesAssignedThisRound Nodes assigned across all phases (prevents double-assignment)
     * @return The job that was started
     * 
     * Python equivalent:
     *   agreements.apply_agreement(job, nodes, p_jobs, f_nodes)
     */
    Job* applyAgreement(Job* jobToStart, 
                        const std::vector<Node*>& nodesToAssign,
                        std::vector<Job*>& pendingJobs,
                        std::vector<Node*>& freeNodes,
                        std::vector<Node*>& nodesAssignedThisRound);

    // Remove node mapping; optionally update job->nodes mapping too.
    void removeNodeMapping(int nodeId, bool updateJobMap);

    // Pop last inserted node mapping (Python dict popitem). Returns node id or -1.
    int popAnyNodeMapping(bool updateJobMap);
};

#endif //ELASTISIM_AGREEMENTHANDLER_H
