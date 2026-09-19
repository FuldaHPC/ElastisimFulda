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

#ifndef ELASTISIM_DIRECTAGREEMENTHANDLER_H
#define ELASTISIM_DIRECTAGREEMENTHANDLER_H

#include "AgreementHandler.h"

/**
 * Direct Agreement Handler: Conservative strategy.
 * 
 * Only starts jobs when ALL reserved nodes become available.
 * Waits for exact nodes that were agreed upon.
 * 
 * Use case: Strict resource guarantees
 * 
 * Python equivalent: DirectAgreementHandler in AgreementHandler.py
 */
class DirectAgreementHandler : public AgreementHandler {
public:
    /**
     * Resolve agreements by waiting for exact reserved nodes.
     * 
     * Algorithm:
     * 1. Find jobs with agreements
     * 2. For each job: Check if ALL agreed nodes are free
     * 3. If yes: Apply agreement and start job
     * 4. If no: Wait (don't start job)
     * 
     * @return Jobs that were started
     * 
     * Python equivalent:
     *   direct_handler.resolve_agreements(p_jobs, f_nodes)
     */
    std::vector<Job*> resolveAgreements(std::vector<Job*>& pendingJobs,
                                         std::vector<Node*>& freeNodes,
                                         std::vector<Node*>& nodesAssignedThisRound) override;
};

#endif //ELASTISIM_DIRECTAGREEMENTHANDLER_H
