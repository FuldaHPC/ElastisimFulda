/*
 * This file is part of the ElastiSim SC26 research extension
 * (scheduler-integrated recurring node shutdown exploiting malleable jobs).
 *
 * C++ implementation adapted from the Wagomu project's MalleableJobScheduling strategy
 * scheduling_algorithms/pref_steal_agreement.py
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

#include "PrefStealAgreementScheduler.h"
#include "../utils/JobQueue.h"
#include "../utils/NodeCache.h"
#include "../utils/JobUtils.h"
#include "../malleability/MalleabilityHelper.h"
#include "../../util/EventLogger.h"
#include "../../util/PerformanceTracer.h"
#include "../../software/Job.h"
#include <algorithm>
#include <memory>
#include <simgrid/s4u.hpp>

XBT_LOG_NEW_DEFAULT_CATEGORY(PrefStealAgreementScheduler, "PrefStealAgreement scheduler");

PrefStealAgreementScheduler::PrefStealAgreementScheduler()
    : agreements(std::make_unique<StealAgreementHandler>()) {
    XBT_INFO("PrefStealAgreementScheduler created!");
}

double PrefStealAgreementScheduler::getPrefPriority(Job* job) const {
    if (!job) {
        return 0.0;
    }
    int currentNodes = job->getAssignedNodes().size();
    int prefNodes = job->getNumNodesPref();
    return static_cast<double>(currentNodes - prefNodes);
}

std::vector<std::pair<Job*, std::vector<Node*>>> PrefStealAgreementScheduler::selectShrinkJobs(
    const std::vector<Job*>& runningMalleableJobs,
    int requiredNodes,
    const std::function<int(Job*)>& nodeTarget,
    const AgreementHandler& agreementsHandler) const {
    PerformanceTracer::ScopedPhase shrinkPhase("shrink_selection");
    if (requiredNodes <= 0) {
        return {};
    }

    std::vector<Job*> sortedJobs = runningMalleableJobs;
    {
        PerformanceTracer::ScopedPhase phase("sort_malleable");
        std::stable_sort(sortedJobs.begin(), sortedJobs.end(),
                         [this](Job* a, Job* b) {
                             return getPrefPriority(a) > getPrefPriority(b);
                         });
    }

    std::vector<std::pair<Job*, std::vector<Node*>>> jobsToShrink;
    int nodesCollected = 0;
    for (Job* job : sortedJobs) {
        if (nodesCollected >= requiredNodes) {
            break;
        }

        int target = nodeTarget(job);
        if (target >= static_cast<int>(job->getAssignedNodes().size())) {
            continue;
        }

        int nodesStillNeeded = requiredNodes - nodesCollected;
        std::vector<Node*> nodesToRemove;
        const auto& assigned = job->getAssignedNodes();
        for (size_t i = static_cast<size_t>(target);
             i < assigned.size() && static_cast<int>(nodesToRemove.size()) < nodesStillNeeded;
             ++i) {
            Node* node = assigned[i];
            if (!agreementsHandler.hasAgreement(node)) {
                nodesToRemove.push_back(node);
            }
        }

        if (!nodesToRemove.empty()) {
            jobsToShrink.emplace_back(job, nodesToRemove);
            nodesCollected += static_cast<int>(nodesToRemove.size());
        }
    }

    if (nodesCollected >= requiredNodes) {
        return jobsToShrink;
    }

    return {};
}

void PrefStealAgreementScheduler::expandJobsToTarget(
    const std::vector<Job*>& runningMalleableJobs,
    std::vector<Node*>& freeNodes,
    const std::function<int(Job*)>& nodeTarget) {
    if (freeNodes.empty()) {
        return;
    }

    std::vector<Job*> sortedJobs = runningMalleableJobs;
    {
        PerformanceTracer::ScopedPhase phase("sort_malleable");
        std::stable_sort(sortedJobs.begin(), sortedJobs.end(),
                         [this](Job* a, Job* b) {
                             return getPrefPriority(a) < getPrefPriority(b);
                         });
    }

    for (Job* job : sortedJobs) {
        if (freeNodes.empty()) {
            break;
        }

        int target = nodeTarget(job);
        int currentNodes = static_cast<int>(job->getAssignedNodes().size());
        int newNodes = target - currentNodes;
        if (newNodes <= 0) {
            continue;
        }

        PerformanceTracer::ScopedPhase phase("expand_apply");
        std::vector<Node*> nodes;
        for (int candidateTarget = target; candidateTarget > currentNodes; --candidateTarget) {
            std::vector<Node*> eligibleNodes = JobUtils::filterNodesForRunningJob(
                freeNodes, job, candidateTarget, simgrid::s4u::Engine::get_clock(), true);
            const int candidateNodesToAssign = candidateTarget - currentNodes;
            if (static_cast<int>(eligibleNodes.size()) < candidateNodesToAssign) {
                continue;
            }
            nodes.assign(eligibleNodes.begin(), eligibleNodes.begin() + candidateNodesToAssign);
            break;
        }
        if (nodes.empty()) {
            continue;
        }
        for (Node* node : nodes) {
            job->assignNode(node);
        }
        for (Node* node : nodes) {
            auto it = std::find(freeNodes.begin(), freeNodes.end(), node);
            if (it != freeNodes.end()) {
                freeNodes.erase(it);
            }
        }
        EventLogger::logJobExpand(job, nodes);
        PerformanceTracer::recordExpand(1, nodes.size());
        job->updateState();
    }
}

std::vector<Job*> PrefStealAgreementScheduler::schedule(const SchedulingContext& context) {
    context.validate();

    auto pendingJobs = JobQueue::getPending(context.jobQueue);
    auto runningJobs = JobQueue::getRunning(context.jobQueue);
    auto runningMalleableJobs = JobQueue::getRunningMalleable(context.jobQueue);
    auto freeNodes = NodeCache::getFreeNodes();

    std::vector<Job*> scheduledJobs;
    std::vector<Node*> nodesAssignedThisRound;

    std::vector<Job*> pendingMut = pendingJobs;
    std::vector<Node*> freeMut(freeNodes.begin(), freeNodes.end());

    if (agreements) {
        std::vector<Job*> started;
        {
            PerformanceTracer::ScopedPhase phase("agreement_resolve");
            started = agreements->resolveAgreements(pendingMut, freeMut, nodesAssignedThisRound);
        }
        scheduledJobs.insert(scheduledJobs.end(), started.begin(), started.end());
    }

    std::vector<Job*> pendingNoAgree;
    pendingNoAgree.reserve(pendingMut.size());
    {
        PerformanceTracer::ScopedPhase phase("agreement_filter");
        for (Job* job : pendingMut) {
            if (!agreements->hasAgreement(job)) {
                pendingNoAgree.push_back(job);
            }
        }
    }

    std::vector<Node*> freeNoAgree;
    freeNoAgree.reserve(freeMut.size());
    {
        PerformanceTracer::ScopedPhase phase("agreement_filter");
        for (Node* node : freeMut) {
            if (!agreements->hasAgreement(node)) {
                freeNoAgree.push_back(node);
            }
        }
    }

    if (!pendingNoAgree.empty() && !freeNoAgree.empty()) {
        std::vector<Job*> sortedRunning = runningJobs;
        auto remainingRuntime = [&](Job* runningJob) -> double {
            return JobUtils::getPredictedRemainingRuntime(runningJob, context.currentTime);
        };
        {
            PerformanceTracer::ScopedPhase phase("sort_running");
            std::stable_sort(sortedRunning.begin(), sortedRunning.end(),
                             [&remainingRuntime](Job* a, Job* b) {
                                 return remainingRuntime(a) < remainingRuntime(b);
                             });
        }

        auto delaysHead = [&](Job* job, int reqNodes, Job* head, int freeForHead, double headRuntime) -> bool {
            if (!job || !head || job == head) {
                return false;
            }
            if (headRuntime <= 0.0) {
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

            return nodesNeeded <= 0 && headStartTime < headRuntime;
        };

        for (size_t i = 0; i < pendingNoAgree.size();) {
            if (freeNoAgree.empty()) {
                break;
            }

            Job* job = pendingNoAgree[i];
            int minNodes = job->getNumNodesMin();
            int prefNodes = job->getNumNodesPref();
            std::vector<Node*> eligibleNodes = JobUtils::filterNodesForPendingAllocation(
                freeNoAgree, job, prefNodes, context.currentTime);
            int reqNodes = 0;
            if (eligibleNodes.size() >= static_cast<size_t>(prefNodes)) {
                reqNodes = prefNodes;
            } else {
                eligibleNodes = JobUtils::filterNodesForPendingAllocation(
                    freeNoAgree, job, minNodes, context.currentTime);
                if (eligibleNodes.size() >= static_cast<size_t>(minNodes)) {
                    reqNodes = minNodes;
                }
            }
            if (reqNodes > 0) {

                int freeForHead = 0;
                double headRuntime = 0.0;
                if (!pendingNoAgree.empty()) {
                    Job* head = pendingNoAgree.front();
                    int headPrefNodes = head->getNumNodesPref();
                    int headMinNodes = head->getNumNodesMin();
                    std::vector<Node*> headEligiblePref = JobUtils::filterNodesForPendingAllocation(
                        freeNoAgree, head, headPrefNodes, context.currentTime);
                    if (headEligiblePref.size() >= static_cast<size_t>(headPrefNodes)) {
                        freeForHead = static_cast<int>(headEligiblePref.size());
                        headRuntime = JobUtils::getPredictedRuntimeForNodes(head, headPrefNodes);
                    } else {
                        std::vector<Node*> headEligibleMin = JobUtils::filterNodesForPendingAllocation(
                            freeNoAgree, head, headMinNodes, context.currentTime);
                        freeForHead = static_cast<int>(headEligibleMin.size());
                        if (headEligibleMin.size() >= static_cast<size_t>(headMinNodes)) {
                            headRuntime = JobUtils::getPredictedRuntimeForNodes(head, headMinNodes);
                        }
                    }
                }

                bool delayed = false;
                {
                    PerformanceTracer::ScopedPhase phase("head_delay_check");
                    delayed = delaysHead(job, reqNodes, pendingNoAgree.front(), freeForHead, headRuntime);
                }
                if (delayed) {
                    ++i;
                    continue;
                }

                std::vector<Node*> nodesToAssign;
                {
                    PerformanceTracer::ScopedPhase phase("initial_allocation");
                    nodesToAssign.assign(eligibleNodes.begin(), eligibleNodes.begin() + reqNodes);
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
        for (Job* pendingJob : pendingNoAgree) {
            auto prefTarget = [](Job* job) { return job->getNumNodesPref(); };
            auto minTarget = [](Job* job) { return job->getNumNodesMin(); };

            auto jobsToShrink = selectShrinkJobs(runningMalleableJobs,
                                                 pendingJob->getNumNodesPref(),
                                                 prefTarget,
                                                 *agreements);
            if (jobsToShrink.empty()) {
                jobsToShrink = selectShrinkJobs(runningMalleableJobs,
                                                pendingJob->getNumNodesMin(),
                                                prefTarget,
                                                *agreements);
            }
            if (jobsToShrink.empty()) {
                jobsToShrink = selectShrinkJobs(runningMalleableJobs,
                                                pendingJob->getNumNodesMin(),
                                                minTarget,
                                                *agreements);
            }

            if (!jobsToShrink.empty()) {
                MalleabilityHelper::applyShrinking(jobsToShrink, pendingJob, *agreements);
            }
        }
    }

    if (!freeNoAgree.empty() && !runningMalleableJobs.empty()) {
        auto prefTarget = [](Job* job) { return job->getNumNodesPref(); };
        auto maxTarget = [](Job* job) { return job->getNumNodesMax(); };

        expandJobsToTarget(runningMalleableJobs, freeNoAgree, prefTarget);
        expandJobsToTarget(runningMalleableJobs, freeNoAgree, maxTarget);
    }

    return scheduledJobs;
}
