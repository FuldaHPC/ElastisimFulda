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

#ifndef ELASTISIM_SCHEDULING_JOBQUEUE_H
#define ELASTISIM_SCHEDULING_JOBQUEUE_H

#include <vector>

class Job;

/**
 * Utility functions for filtering and sorting job queues.
 * Provides common job queue operations for schedulers.
 */
class JobQueue {
public:
    /**
     * Filter jobs by state: PENDING.
     */
    static std::vector<Job*> getPending(const std::vector<Job*>& jobs);
    
    /**
     * Filter jobs by state: RUNNING.
     */
    static std::vector<Job*> getRunning(const std::vector<Job*>& jobs);
    
    /**
     * Filter jobs by type: MALLEABLE (from running jobs).
     */
    static std::vector<Job*> getMalleable(const std::vector<Job*>& jobs);
    
    /**
     * Filter running malleable jobs (convenience method).
     * Combines getRunning() + filter by MALLEABLE type.
     */
    static std::vector<Job*> getRunningMalleable(const std::vector<Job*>& jobs);
    
    /**
     * Filter jobs without agreements.
     * 
     * @param jobs Jobs to filter
     * @param agreements Agreement handler to check
     * @return Jobs that don't have resource reservations
     */
    static std::vector<Job*> filterWithoutAgreement(
        const std::vector<Job*>& jobs,
        const class AgreementHandler& agreements);
    
    /**
     * Sort jobs by submit time (FIFO order).
     * Modifies vector in-place.
     */
    static void sortBySubmitTime(std::vector<Job*>& jobs);
    
    /**
     * Sort jobs by walltime (Shortest Job First).
     * Modifies vector in-place.
     */
    static void sortByWalltime(std::vector<Job*>& jobs);
    
    /**
     * Sort jobs by predicted runtime.
     * Modifies vector in-place.
     */
    static void sortByPredictedRuntime(std::vector<Job*>& jobs);
    
    /**
     * Validate job queue for common issues.
     * Logs warnings for invalid jobs.
     * 
     * @param jobs Job queue to validate
     */
    static void validateJobQueue(const std::vector<Job*>& jobs);
};

#endif // ELASTISIM_SCHEDULING_JOBQUEUE_H
