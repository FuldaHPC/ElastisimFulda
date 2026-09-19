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

#ifndef ELASTISIM_POOLAGREEMENTHANDLER_H
#define ELASTISIM_POOLAGREEMENTHANDLER_H

#include "AgreementHandler.h"

/**
 * Pool Agreement Handler: Flexible strategy.
 * 
 * Uses a pool of nodes for agreements:
 * - Prefers agreed nodes if available
 * - Falls back to any free nodes if needed
 * - Starts jobs as soon as enough nodes are available (not strict)
 * 
 * Use case: Fast scheduling with flexible resource allocation
 * 
 * Python equivalent: PoolAgreementHandler in AgreementHandler.py
 */
class PoolAgreementHandler : public AgreementHandler {
public:
    /**
     * Resolve agreements using flexible node pool.
     * 
     * Algorithm:
     * 1. Find jobs with agreements
     * 2. For each job: Try to collect enough nodes from pool
     * 3. Pool priority: Agreement nodes first, then any free nodes
     * 4. Start job if enough nodes collected
     * 5. Clean up partial agreements
     * 
     * @return Jobs that were started
     * 
     * Python equivalent:
     *   pool_handler.resolve_agreements(p_jobs, f_nodes)
     */
    std::vector<Job*> resolveAgreements(std::vector<Job*>& pendingJobs,
                                         std::vector<Node*>& freeNodes,
                                         std::vector<Node*>& nodesAssignedThisRound) override;

private:
    /**
     * Get nodes from pool for a job.
     * 
     * Priority:
     * 1. Free nodes with agreement for this job
     * 2. Other free nodes without agreements
     * 
     * @param job Job to get nodes for
     * @param freeNodes Available nodes
     * @return Vector of nodes to assign
     * 
     * Python equivalent:
     *   get_nodes_from_pool(job, f_nodes)
     */
    std::vector<Node*> selectNodesFromPool(Job* job,
                                           const std::vector<Node*>& freeNodes) const;
    void finalizePoolAssignments(Job* job, const std::vector<Node*>& nodesToAssign);
};

#endif //ELASTISIM_POOLAGREEMENTHANDLER_H
