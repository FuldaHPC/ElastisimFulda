/*
 * This file is part of the ElastiSim SC26 research extension
 * (scheduler-integrated recurring node shutdown exploiting malleable jobs).
 *
 * C++ implementation adapted from the Wagomu project's MalleableJobScheduling strategy
 * scheduling_algorithms/min_common_pool.py
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

#ifndef ELASTISIM_MINCOMMONPOOLSCHEDULER_H
#define ELASTISIM_MINCOMMONPOOLSCHEDULER_H

#include "../base/IScheduler.h"
#include "../malleability/PoolAgreementHandler.h"
#include <memory>

/**
 * MinCommonPool Scheduler: Same priority as MinAgreement but with flexible pool.
 * 
 * Strategy:
 * - Shrinking: Prioritize jobs with most nodes above min (shrink those first)
 * - Expanding: Prioritize jobs with fewest nodes above min (expand those first)
 * - Agreement: PoolAgreementHandler (flexible pool, uses any available nodes)
 * 
 * Difference to MinAgreement:
 * - MinAgreement: Uses DirectAgreementHandler (waits for exact reserved nodes)
 * - MinCommonPool: Uses PoolAgreementHandler (uses any available nodes from pool)
 * 
 * This allows faster job starts as it doesn't wait for specific nodes to become free.
 * 
 * Python equivalent: min_common_pool.py
 */
class MinCommonPoolScheduler : public IScheduler {
public:
    MinCommonPoolScheduler();
    
    std::string getName() const override { 
        return "MinCommonPoolScheduler"; 
    }
    
    std::vector<Job*> schedule(const SchedulingContext& context) override;

private:
    /**
     * Expand priority: Fewest nodes above min first.
     * 
     * Priority = assigned_nodes - min_nodes
     * Lower value = expand first
     * 
     * Same as MinAgreementScheduler, only agreement handler differs.
     * 
     * Python equivalent:
     *   def get_min_job_priority(job):
     *       return len(job.assigned_nodes) - job.num_nodes_min
     */
    double getExpandPriority(Job* job);
    double getShrinkPriority(Job* job);

    std::unique_ptr<AgreementHandler> agreements;
};

#endif //ELASTISIM_MINCOMMONPOOLSCHEDULER_H
