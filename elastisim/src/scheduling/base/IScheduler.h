/*
 * This file is part of the ElastiSim SC26 research extension
 * (scheduler-integrated recurring node shutdown exploiting malleable jobs).
 *
 * Copyright (c) 2026 Fulda University of Applied Sciences, Germany
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This software may be modified and distributed under the terms of the 3-Clause
 * BSD License. See the LICENSE file in the repository root for the license text
 * and README.md for the origin of each component.
 */

#ifndef ISCHEDULER_H
#define ISCHEDULER_H

#include <vector>
#include <string>
#include "SchedulingContext.h"
#include "../../software/Job.h"

// Abstract interface for all scheduler implementations (Strategy Pattern)
class IScheduler {
public:
    virtual ~IScheduler() = default;
    
    // Main scheduling method - all schedulers implement this
    // Returns jobs that were successfully scheduled
    virtual std::vector<Job*> schedule(const SchedulingContext& context) = 0;
    
    // Scheduler name for logging and identification
    virtual std::string getName() const = 0;
};

#endif // ISCHEDULER_H
