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

#ifndef ELASTISIM_STEALAGREEMENTHANDLER_H
#define ELASTISIM_STEALAGREEMENTHANDLER_H

#include "AgreementHandler.h"

/**
 * StealAgreementHandler: Can swap reservations to let a job use
 * free nodes reserved for other jobs.
 *
 * Python equivalent: StealAgreementHandler in AgreementHandler.py
 */
class StealAgreementHandler : public AgreementHandler {
public:
    std::vector<Job*> resolveAgreements(std::vector<Job*>& pendingJobs,
                                        std::vector<Node*>& freeNodes,
                                        std::vector<Node*>& nodesAssignedThisRound) override;

private:
    void swapNodes(int node1Id, int node2Id);
    void stealAgreementNodes(Job* job, const std::vector<Node*>& freeAgreementNodes);
};

#endif // ELASTISIM_STEALAGREEMENTHANDLER_H
