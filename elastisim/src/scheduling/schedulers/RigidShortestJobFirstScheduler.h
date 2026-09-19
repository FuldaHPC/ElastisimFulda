/*
 * This file is part of the ElastiSim SC26 research extension
 * (scheduler-integrated recurring node shutdown exploiting malleable jobs).
 *
 * C++ implementation adapted from the Wagomu project's MalleableJobScheduling strategy
 * scheduling_algorithms/rigid_shortest_job_first.py
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

#ifndef ELASTISIM_RIGIDSHORTESTJOBFIRSTSCHEDULER_H
#define ELASTISIM_RIGIDSHORTESTJOBFIRSTSCHEDULER_H

#include "../base/IScheduler.h"

/**
 * Rigid Shortest-Job-First Scheduler.
 *
 * Python equivalent: rigid_shortest_job_first.py
 */
class RigidShortestJobFirstScheduler : public IScheduler {
public:
    std::string getName() const override {
        return "RigidShortestJobFirstScheduler";
    }

    std::vector<Job*> schedule(const SchedulingContext& context) override;
};

#endif // ELASTISIM_RIGIDSHORTESTJOBFIRSTSCHEDULER_H
