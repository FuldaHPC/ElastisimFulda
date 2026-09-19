/*
 * This file is part of the ElastiSim SC26 research extension
 * (scheduler-integrated recurring node shutdown exploiting malleable jobs).
 *
 * C++ implementation adapted from the Wagomu project's MalleableJobScheduling strategy
 * scheduling_algorithms/average_common_pool.py
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

#ifndef ELASTISIM_AVERAGECOMMONPOOLSCHEDULER_H
#define ELASTISIM_AVERAGECOMMONPOOLSCHEDULER_H

#include "../base/IScheduler.h"
#include "../malleability/PoolAgreementHandler.h"
#include <memory>

/**
 * AverageCommonPool Scheduler: balances node usage across malleable jobs.
 *
 * Python equivalent: average_common_pool.py
 */
class AverageCommonPoolScheduler : public IScheduler {
public:
    AverageCommonPoolScheduler();

    std::string getName() const override {
        return "AverageCommonPoolScheduler";
    }

    std::vector<Job*> schedule(const SchedulingContext& context) override;

private:
    double getAveragePriority(Job* job, int adjustAssigned) const;
    Node* selectAvailableNode(Job* job,
                              const std::vector<std::pair<Job*, std::vector<Node*>>>& shrinkNodes,
                              const AgreementHandler& agreements) const;
    std::vector<std::pair<Job*, std::vector<Node*>>> selectShrinkJobs(
        const std::vector<Job*>& runningMalleableJobs,
        int requiredNodes,
        const AgreementHandler& agreements) const;

    void expandAverageJobs(const std::vector<Job*>& runningMalleableJobs,
                           std::vector<Node*>& freeNodes);

    std::unique_ptr<AgreementHandler> agreements;
};

#endif // ELASTISIM_AVERAGECOMMONPOOLSCHEDULER_H
