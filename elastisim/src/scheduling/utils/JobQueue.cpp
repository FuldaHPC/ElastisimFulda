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

#include "JobQueue.h"
#include "JobUtils.h"
#include "../software/Job.h"
#include "../malleability/AgreementHandler.h"
#include "../../util/PerformanceTracer.h"
#include <simgrid/s4u.hpp>
#include <algorithm>

XBT_LOG_NEW_DEFAULT_CATEGORY(JobQueue, "Job queue utility functions");

std::vector<Job*> JobQueue::getPending(const std::vector<Job*>& jobs) {
    PerformanceTracer::ScopedPhase phase("jobqueue_pending");
    std::vector<Job*> pending;
    for (Job* job : jobs) {
        if (job && job->getState() == PENDING) {
            pending.push_back(job);
        }
    }
    return pending;
}

std::vector<Job*> JobQueue::getRunning(const std::vector<Job*>& jobs) {
    PerformanceTracer::ScopedPhase phase("jobqueue_running");
    std::vector<Job*> running;
    for (Job* job : jobs) {
        if (job && job->getState() == RUNNING) {
            running.push_back(job);
        }
    }
    return running;
}

std::vector<Job*> JobQueue::getMalleable(const std::vector<Job*>& jobs) {
    std::vector<Job*> malleable;
    for (Job* job : jobs) {
        if (job && job->getType() == MALLEABLE) {
            malleable.push_back(job);
        }
    }
    return malleable;
}

std::vector<Job*> JobQueue::getRunningMalleable(const std::vector<Job*>& jobs) {
    PerformanceTracer::ScopedPhase phase("jobqueue_running_malleable");
    std::vector<Job*> runningMalleable;
    for (Job* job : jobs) {
        if (job && job->getState() == RUNNING && job->getType() == MALLEABLE) {
            runningMalleable.push_back(job);
        }
    }
    return runningMalleable;
}

std::vector<Job*> JobQueue::filterWithoutAgreement(
    const std::vector<Job*>& jobs,
    const AgreementHandler& agreements) {
    std::vector<Job*> filtered;
    for (Job* job : jobs) {
        if (job && !agreements.hasAgreement(job)) {
            filtered.push_back(job);
        }
    }
    return filtered;
}

void JobQueue::sortBySubmitTime(std::vector<Job*>& jobs) {
    std::sort(jobs.begin(), jobs.end(), [](const Job* a, const Job* b) {
        if (!a || !b) return false;
        // Primary: submit time
        if (a->getSubmitTime() != b->getSubmitTime()) {
            return a->getSubmitTime() < b->getSubmitTime();
        }
        // Secondary: job ID (for deterministic tie-breaking)
        return a->getId() < b->getId();
    });
}

void JobQueue::sortByWalltime(std::vector<Job*>& jobs) {
    std::sort(jobs.begin(), jobs.end(), [](const Job* a, const Job* b) {
        if (!a || !b) return false;
        double walltime_a = JobUtils::getTimelimit(a);
        double walltime_b = JobUtils::getTimelimit(b);
        return walltime_a < walltime_b;
    });
}

void JobQueue::sortByPredictedRuntime(std::vector<Job*>& jobs) {
    std::sort(jobs.begin(), jobs.end(), [](const Job* a, const Job* b) {
        if (!a || !b) return false;
        double runtime_a = JobUtils::getPredictedRuntimeForNodes(a, a->getNumNodesPref());
        double runtime_b = JobUtils::getPredictedRuntimeForNodes(b, b->getNumNodesPref());
        return runtime_a < runtime_b;
    });
}

void JobQueue::validateJobQueue(const std::vector<Job*>& jobs) {
    int invalidCount = 0;
    
    for (Job* job : jobs) {
        if (!job) {
            XBT_WARN("Job queue contains null pointer");
            invalidCount++;
            continue;
        }
        
        if (!JobUtils::hasValidAttributes(job)) {
            invalidCount++;
        }
    }
    
    if (invalidCount > 0) {
        XBT_WARN("Job queue validation: %d invalid jobs found", invalidCount);
    }
}
