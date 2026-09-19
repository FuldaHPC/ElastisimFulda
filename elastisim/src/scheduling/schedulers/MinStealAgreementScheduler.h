/*
 * This file is part of the ElastiSim SC26 research extension
 * (scheduler-integrated recurring node shutdown exploiting malleable jobs).
 *
 * C++ implementation adapted from the Wagomu project's MalleableJobScheduling strategy
 * scheduling_algorithms/min_steal_agreement.py
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

#ifndef ELASTISIM_MINSTEALAGREEMENTSCHEDULER_H
#define ELASTISIM_MINSTEALAGREEMENTSCHEDULER_H

#include "../base/IScheduler.h"
#include "../malleability/StealAgreementHandler.h"
#include <memory>

/**
 * MinStealAgreement Scheduler: min-node priority with steal agreements.
 *
 * Python equivalent: min_steal_agreement.py
 */
class MinStealAgreementScheduler : public IScheduler {
public:
    MinStealAgreementScheduler();

    std::string getName() const override {
        return "MinStealAgreementScheduler";
    }

    std::vector<Job*> schedule(const SchedulingContext& context) override;

private:
    double getExpandPriority(Job* job);
    double getShrinkPriority(Job* job);

    std::unique_ptr<AgreementHandler> agreements;
};

#endif // ELASTISIM_MINSTEALAGREEMENTSCHEDULER_H
