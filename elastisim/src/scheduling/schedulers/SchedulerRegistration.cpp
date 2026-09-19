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

#include "../base/SchedulerRegistry.h"
#include "RigidFifoScheduler.h"
#include "RigidBackfillScheduler.h"
#include "RigidEasyBackfillScheduler.h"
#include "RigidShortestJobFirstScheduler.h"
#include "MinAgreementScheduler.h"
#include "MinCommonPoolScheduler.h"
#include "AverageCommonPoolScheduler.h"
#include "MinStealAgreementScheduler.h"
#include "PrefStealAgreementScheduler.h"
#include <memory>

// Auto-register schedulers when this file is loaded
namespace {
    struct RigidFifoSchedulerRegistration {
        RigidFifoSchedulerRegistration() {
            SchedulerRegistry::registerScheduler("rigid_fifo", 
                []() -> std::unique_ptr<IScheduler> {
                    return std::make_unique<RigidFifoScheduler>();
                });
        }
    };
    
    struct RigidBackfillSchedulerRegistration {
        RigidBackfillSchedulerRegistration() {
            SchedulerRegistry::registerScheduler("rigid_backfill", 
                []() -> std::unique_ptr<IScheduler> {
                    return std::make_unique<RigidBackfillScheduler>();
                });
        }
    };
    
    struct RigidEasyBackfillSchedulerRegistration {
        RigidEasyBackfillSchedulerRegistration() {
            SchedulerRegistry::registerScheduler("rigid_easy_backfill",
                []() -> std::unique_ptr<IScheduler> {
                    return std::make_unique<RigidEasyBackfillScheduler>();
                });
        }
    };
    
    struct RigidShortestJobFirstSchedulerRegistration {
        RigidShortestJobFirstSchedulerRegistration() {
            SchedulerRegistry::registerScheduler("rigid_shortest_job_first",
                []() -> std::unique_ptr<IScheduler> {
                    return std::make_unique<RigidShortestJobFirstScheduler>();
                });
        }
    };
    struct MinAgreementSchedulerRegistration {
        MinAgreementSchedulerRegistration() {
            SchedulerRegistry::registerScheduler("min_agreement", 
                []() -> std::unique_ptr<IScheduler> {
                    return std::make_unique<MinAgreementScheduler>();
                });
        }
    };
    
    struct MinCommonPoolSchedulerRegistration {
        MinCommonPoolSchedulerRegistration() {
            SchedulerRegistry::registerScheduler("min_common_pool", 
                []() -> std::unique_ptr<IScheduler> {
                    return std::make_unique<MinCommonPoolScheduler>();
                });
        }
    };

    struct AverageCommonPoolSchedulerRegistration {
        AverageCommonPoolSchedulerRegistration() {
            SchedulerRegistry::registerScheduler("average_common_pool",
                []() -> std::unique_ptr<IScheduler> {
                    return std::make_unique<AverageCommonPoolScheduler>();
                });
        }
    };

    struct MinStealAgreementSchedulerRegistration {
        MinStealAgreementSchedulerRegistration() {
            SchedulerRegistry::registerScheduler("min_steal_agreement",
                []() -> std::unique_ptr<IScheduler> {
                    return std::make_unique<MinStealAgreementScheduler>();
                });
        }
    };

    struct PrefStealAgreementSchedulerRegistration {
        PrefStealAgreementSchedulerRegistration() {
            SchedulerRegistry::registerScheduler("pref_steal_agreement",
                []() -> std::unique_ptr<IScheduler> {
                    return std::make_unique<PrefStealAgreementScheduler>();
                });
        }
    };
    
    static RigidFifoSchedulerRegistration rigidFifoRegistration;
    static RigidBackfillSchedulerRegistration rigidBackfillRegistration;
    static RigidEasyBackfillSchedulerRegistration rigidEasyBackfillRegistration;
    static RigidShortestJobFirstSchedulerRegistration rigidShortestJobFirstRegistration;
    static MinAgreementSchedulerRegistration minAgreementRegistration;
    static MinCommonPoolSchedulerRegistration minCommonPoolRegistration;
    static AverageCommonPoolSchedulerRegistration averageCommonPoolRegistration;
    static MinStealAgreementSchedulerRegistration minStealAgreementRegistration;
    static PrefStealAgreementSchedulerRegistration prefStealAgreementRegistration;
}
