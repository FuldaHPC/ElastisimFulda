/*
 * This file is part of the ElastiSim SC26 research extension
 * (scheduler-integrated recurring node shutdown exploiting malleable jobs).
 *
 * C++ implementation adapted from the Wagomu project's MalleableJobScheduling strategy
 * scheduling_algorithms/pref_steal_agreement.py
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

#ifndef ELASTISIM_PREFSTEALAGREEMENTSCHEDULER_H
#define ELASTISIM_PREFSTEALAGREEMENTSCHEDULER_H

#include "../base/IScheduler.h"
#include "../malleability/StealAgreementHandler.h"
#include <memory>
#include <functional>

/**
 * PrefStealAgreement Scheduler: prefers pref nodes and steal agreements.
 *
 * Python equivalent: pref_steal_agreement.py
 */
class PrefStealAgreementScheduler : public IScheduler {
public:
    PrefStealAgreementScheduler();

    std::string getName() const override {
        return "PrefStealAgreementScheduler";
    }

    std::vector<Job*> schedule(const SchedulingContext& context) override;

private:
    double getPrefPriority(Job* job) const;
    std::vector<std::pair<Job*, std::vector<Node*>>> selectShrinkJobs(
        const std::vector<Job*>& runningMalleableJobs,
        int requiredNodes,
        const std::function<int(Job*)>& nodeTarget,
        const AgreementHandler& agreements) const;

    void expandJobsToTarget(const std::vector<Job*>& runningMalleableJobs,
                            std::vector<Node*>& freeNodes,
                            const std::function<int(Job*)>& nodeTarget);

    std::unique_ptr<AgreementHandler> agreements;
};

#endif // ELASTISIM_PREFSTEALAGREEMENTSCHEDULER_H
