/*
 * This file is part of the ElastiSim SC26 research extension
 * (scheduler-integrated recurring node shutdown exploiting malleable jobs).
 *
 * C++ implementation adapted from the Wagomu project's MalleableJobScheduling strategy
 * scheduling_algorithms/rigid_easy_backfill.py
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

#ifndef ELASTISIM_RIGIDEASYBACKFILLSCHEDULER_H
#define ELASTISIM_RIGIDEASYBACKFILLSCHEDULER_H

#include "../base/IScheduler.h"

/**
 * Rigid Easy-Backfill Scheduler.
 *
 * Python equivalent: rigid_easy_backfill.py
 */
class RigidEasyBackfillScheduler : public IScheduler {
public:
    std::string getName() const override {
        return "RigidEasyBackfillScheduler";
    }

    std::vector<Job*> schedule(const SchedulingContext& context) override;
};

#endif // ELASTISIM_RIGIDEASYBACKFILLSCHEDULER_H
