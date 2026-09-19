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

#ifndef SCHEDULINGCONTEXT_H
#define SCHEDULINGCONTEXT_H

#include <vector>
#include <stdexcept>
#include "../../system/Scheduler.h"

class Job;

// Context object for scheduler invocations (replaces 5+ parameters)
struct SchedulingContext {
    InvocationType invocationType;
    const std::vector<Job*>& jobQueue;
    const std::vector<Job*>& modifiedJobs;
    const Job* requestingJob;
    int numberOfNodes;
    double currentTime;
    
    // Validate context before scheduling
    void validate() const {
        if (currentTime < 0) {
            throw std::runtime_error("SchedulingContext: invalid currentTime (negative)");
        }
        // numberOfNodes can be -1 for periodic/general invocations (only used for specific job requests)
    }
};

#endif // SCHEDULINGCONTEXT_H
