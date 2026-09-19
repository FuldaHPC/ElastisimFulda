/*
 * This file is part of the ElastiSim SC26 research extension
 * (scheduler-integrated recurring node shutdown exploiting malleable jobs).
 *
 * C++ implementation adapted from the Wagomu project's MalleableJobScheduling strategy
 * scheduling_algorithms/min_agreement.py
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

#include "MinAgreementScheduler.h"
#include "../utils/JobQueue.h"
#include "../utils/NodeCache.h"
#include "../utils/JobUtils.h"
#include "../malleability/MalleabilityHelper.h"
#include "../../util/EventLogger.h"
#include "../../software/Job.h"
#include <algorithm>
#include <memory>
#include <simgrid/s4u.hpp>

XBT_LOG_NEW_DEFAULT_CATEGORY(MinAgreementScheduler, "MinAgreement scheduler");

MinAgreementScheduler::MinAgreementScheduler()
    : agreements(std::make_unique<DirectAgreementHandler>()) {
    XBT_INFO("MinAgreementScheduler created!");
}

double MinAgreementScheduler::getExpandPriority(Job* job) {
    if (!job) {
        return 0.0;
    }

    int currentNodes = job->getAssignedNodes().size();
    int minNodes = job->getNumNodesMin();
    return static_cast<double>(currentNodes - minNodes);
}

double MinAgreementScheduler::getShrinkPriority(Job* job) {
    return getExpandPriority(job);
}

std::vector<Job*> MinAgreementScheduler::schedule(const SchedulingContext& context) {
    context.validate();

    auto pendingJobs = JobQueue::getPending(context.jobQueue);
    auto runningJobs = JobQueue::getRunning(context.jobQueue);
    auto runningMalleableJobs = JobQueue::getRunningMalleable(context.jobQueue);
    auto freeNodes = NodeCache::getFreeNodes();

    std::vector<Job*> scheduledJobs;
    std::vector<Node*> nodesAssignedThisRound;

    std::vector<Job*> pendingMut = pendingJobs;
    std::vector<Node*> freeMut(freeNodes.begin(), freeNodes.end());

    XBT_DEBUG("MinAgreement schedule at t=%.1f: pending=%zu running=%zu malleable=%zu free=%zu",
              context.currentTime, pendingJobs.size(), runningJobs.size(),
              runningMalleableJobs.size(), freeMut.size());

    if (agreements) {
        auto started = agreements->resolveAgreements(pendingMut, freeMut, nodesAssignedThisRound);
        scheduledJobs.insert(scheduledJobs.end(), started.begin(), started.end());
    }

    std::vector<Job*> pendingNoAgree;
    pendingNoAgree.reserve(pendingMut.size());
    for (Job* job : pendingMut) {
        if (!agreements->hasAgreement(job)) {
            pendingNoAgree.push_back(job);
        }
    }

    std::vector<Node*> freeNoAgree;
    freeNoAgree.reserve(freeMut.size());
    for (Node* node : freeMut) {
        if (!agreements->hasAgreement(node)) {
            freeNoAgree.push_back(node);
        }
    }

    if (!pendingNoAgree.empty() && !freeNoAgree.empty()) {
        std::vector<Job*> sortedRunning = runningJobs;
        auto remainingRuntime = [&](Job* runningJob) -> double {
            return JobUtils::getPredictedRemainingRuntime(runningJob, context.currentTime);
        };
        std::stable_sort(sortedRunning.begin(), sortedRunning.end(),
                         [&remainingRuntime](Job* a, Job* b) {
                             return remainingRuntime(a) < remainingRuntime(b);
                         });

        auto delaysHead = [&](Job* job, int reqNodes, Job* head, int freeForHead) -> bool {
            if (!job || !head || job == head) {
                return false;
            }

            int nodesNeeded = reqNodes - freeForHead;
            double headStartTime = context.currentTime;

            for (Job* runningJob : sortedRunning) {
                if (nodesNeeded <= 0) {
                    break;
                }
                nodesNeeded -= static_cast<int>(runningJob->getAssignedNodes().size());
                headStartTime = context.currentTime + remainingRuntime(runningJob);
            }

            return nodesNeeded <= 0 &&
                   headStartTime < JobUtils::getPredictedRuntimeForNodes(head, head->getNumNodesMin());
        };

        for (size_t i = 0; i < pendingNoAgree.size();) {
            if (freeNoAgree.empty()) {
                break;
            }

            Job* job = pendingNoAgree[i];
            int reqNodes = job->getNumNodesMin();

            std::vector<Node*> eligibleNodes = JobUtils::filterNodesForPendingAllocation(
                freeNoAgree, job, reqNodes, context.currentTime);
            if (eligibleNodes.size() >= static_cast<size_t>(reqNodes)) {
                int freeForHead = 0;
                if (!pendingNoAgree.empty()) {
                    Job* head = pendingNoAgree.front();
                    freeForHead = static_cast<int>(JobUtils::filterNodesForPendingAllocation(
                        freeNoAgree, head, head->getNumNodesMin(), context.currentTime).size());
                }

                if (delaysHead(job, reqNodes, pendingNoAgree.front(), freeForHead)) {
                    ++i;
                    continue;
                }

                std::vector<Node*> nodesToAssign(eligibleNodes.begin(),
                                                eligibleNodes.begin() + reqNodes);
                for (Node* node : nodesToAssign) {
                    job->assignNode(node);
                    nodesAssignedThisRound.push_back(node);
                }
                for (Node* node : nodesToAssign) {
                    auto it = std::find(freeNoAgree.begin(), freeNoAgree.end(), node);
                    if (it != freeNoAgree.end()) {
                        freeNoAgree.erase(it);
                    }
                }

                if (job->getType() != RIGID) {
                    job->assignNumGpusPerNode(job->getNumGpusPerNodeMax());
                }
                EventLogger::logJobStart(job, nodesToAssign);
                job->setState(PENDING_ALLOCATION);
                scheduledJobs.push_back(job);

                pendingNoAgree.erase(pendingNoAgree.begin() + static_cast<long>(i));
                ++i;
                continue;
            }

            ++i;
        }
    }

    if (!pendingNoAgree.empty() && !runningMalleableJobs.empty()) {
        auto shrinkPriority = [this](Job* job) { return getShrinkPriority(job); };
        std::vector<Job*> sortedMalleable = runningMalleableJobs;
        std::stable_sort(sortedMalleable.begin(), sortedMalleable.end(),
                         [&shrinkPriority](Job* a, Job* b) {
                             return shrinkPriority(a) > shrinkPriority(b);
                         });

        for (Job* pendingJob : pendingNoAgree) {
            int requiredNodes = pendingJob->getNumNodesMin();
            auto jobsToShrink = MalleabilityHelper::selectShrinkJobsOrdered(
                sortedMalleable,
                requiredNodes,
                *agreements);

            if (!jobsToShrink.empty()) {
                MalleabilityHelper::applyShrinking(jobsToShrink, pendingJob, *agreements);
            }
        }
    }

    if (!freeNoAgree.empty() && !runningMalleableJobs.empty()) {
        auto sortedJobs = runningMalleableJobs;
        std::stable_sort(sortedJobs.begin(), sortedJobs.end(),
                         [this](Job* a, Job* b) {
                             return getExpandPriority(a) < getExpandPriority(b);
                         });

        for (Job* job : sortedJobs) {
            if (freeNoAgree.empty()) {
                break;
            }
            MalleabilityHelper::expandJob(job, freeNoAgree);
        }
    }

    return scheduledJobs;
}
