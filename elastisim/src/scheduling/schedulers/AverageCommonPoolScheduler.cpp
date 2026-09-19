/*
 * This file is part of the ElastiSim SC26 research extension
 * (scheduler-integrated recurring node shutdown exploiting malleable jobs).
 *
 * C++ implementation adapted from the Wagomu project's MalleableJobScheduling strategy
 * scheduling_algorithms/average_common_pool.py
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

#include "AverageCommonPoolScheduler.h"
#include "../utils/JobQueue.h"
#include "../utils/NodeCache.h"
#include "../utils/JobUtils.h"
#include "../malleability/MalleabilityHelper.h"
#include "../../util/EventLogger.h"
#include "../../software/Job.h"
#include <algorithm>
#include <memory>
#include <simgrid/s4u.hpp>

XBT_LOG_NEW_DEFAULT_CATEGORY(AverageCommonPoolScheduler, "AverageCommonPool scheduler");

AverageCommonPoolScheduler::AverageCommonPoolScheduler()
    : agreements(std::make_unique<PoolAgreementHandler>()) {
    XBT_INFO("AverageCommonPoolScheduler created!");
}

double AverageCommonPoolScheduler::getAveragePriority(Job* job, int adjustAssigned) const {
    if (!job) {
        return 0.0;
    }

    int minNodes = job->getNumNodesMin();
    int maxNodes = job->getNumNodesMax();
    int nodeRange = maxNodes - minNodes;
    if (nodeRange <= 0) {
        return 0.0;
    }

    int currentAmount = static_cast<int>(job->getAssignedNodes().size()) - adjustAssigned;
    return static_cast<double>(currentAmount - minNodes) / static_cast<double>(nodeRange);
}

Node* AverageCommonPoolScheduler::selectAvailableNode(
    Job* job,
    const std::vector<std::pair<Job*, std::vector<Node*>>>& shrinkNodes,
    const AgreementHandler& agreements) const {
    if (!job) {
        return nullptr;
    }

    std::vector<Node*> preShrunkNodes;
    for (const auto& entry : shrinkNodes) {
        for (Node* node : entry.second) {
            preShrunkNodes.push_back(node);
        }
    }

    const auto& assigned = job->getAssignedNodes();
    std::vector<Node*> candidates;
    candidates.reserve(assigned.size());
    for (Node* node : assigned) {
        if (std::find(preShrunkNodes.begin(), preShrunkNodes.end(), node) != preShrunkNodes.end()) {
            continue;
        }
        if (agreements.hasAgreement(node)) {
            continue;
        }
        candidates.push_back(node);
    }

    int minNodes = job->getNumNodesMin();
    if (static_cast<int>(candidates.size()) > minNodes) {
        return candidates[static_cast<size_t>(minNodes)];
    }
    return nullptr;
}

std::vector<std::pair<Job*, std::vector<Node*>>> AverageCommonPoolScheduler::selectShrinkJobs(
    const std::vector<Job*>& runningMalleableJobs,
    int requiredNodes,
    const AgreementHandler& agreements) const {
    if (requiredNodes <= 0) {
        return {};
    }

    std::vector<std::pair<Job*, std::vector<Node*>>> shrinkNodes;
    shrinkNodes.reserve(runningMalleableJobs.size());
    for (Job* job : runningMalleableJobs) {
        shrinkNodes.emplace_back(job, std::vector<Node*>{});
    }

    for (int i = 0; i < requiredNodes; ++i) {
        Job* bestJob = nullptr;
        double bestPriority = -1e9;

        for (auto& entry : shrinkNodes) {
            Job* job = entry.first;
            if (!selectAvailableNode(job, shrinkNodes, agreements)) {
                continue;
            }
            int adjust = -static_cast<int>(entry.second.size());
            double priority = getAveragePriority(job, adjust);
            if (!bestJob || priority > bestPriority) {
                bestJob = job;
                bestPriority = priority;
            }
        }

        if (!bestJob) {
            return {};
        }

        for (auto& entry : shrinkNodes) {
            if (entry.first == bestJob) {
                Node* node = selectAvailableNode(bestJob, shrinkNodes, agreements);
                if (node) {
                    entry.second.push_back(node);
                }
                break;
            }
        }
    }

    std::vector<std::pair<Job*, std::vector<Node*>>> result;
    result.reserve(shrinkNodes.size());
    for (const auto& entry : shrinkNodes) {
        if (!entry.second.empty()) {
            result.push_back(entry);
        }
    }
    return result;
}

void AverageCommonPoolScheduler::expandAverageJobs(
    const std::vector<Job*>& runningMalleableJobs,
    std::vector<Node*>& freeNodes) {
    if (freeNodes.empty() || runningMalleableJobs.empty()) {
        return;
    }

    std::vector<Job*> orderedJobs = runningMalleableJobs;
    std::vector<int> expandAmount(orderedJobs.size(), 0);

    for (size_t i = 0; i < freeNodes.size(); ++i) {
        size_t bestIdx = 0;
        double bestPriority = 0.0;
        bool bestSet = false;

        for (size_t idx = 0; idx < orderedJobs.size(); ++idx) {
            Job* job = orderedJobs[idx];
            double priority = getAveragePriority(job, expandAmount[idx]);
            if (!bestSet || priority < bestPriority) {
                bestPriority = priority;
                bestIdx = idx;
                bestSet = true;
            }
        }

        Job* chosen = orderedJobs[bestIdx];
        if (static_cast<int>(chosen->getAssignedNodes().size()) == chosen->getNumNodesMax()) {
            break;
        }
        expandAmount[bestIdx] += 1;
    }

    for (size_t idx = 0; idx < orderedJobs.size(); ++idx) {
        int amount = expandAmount[idx];
        if (amount <= 0) {
            continue;
        }
        Job* job = orderedJobs[idx];
        int maxNewNodes = job->getNumNodesMax() - static_cast<int>(job->getAssignedNodes().size());
        int nodesToAssign = std::min(maxNewNodes, amount);
        if (nodesToAssign <= 0) {
            continue;
        }
        const int currentNodes = static_cast<int>(job->getAssignedNodes().size());
        std::vector<Node*> nodes;
        for (int targetNodes = currentNodes + nodesToAssign; targetNodes > currentNodes; --targetNodes) {
            std::vector<Node*> eligibleNodes = JobUtils::filterNodesForRunningJob(
                freeNodes, job, targetNodes, simgrid::s4u::Engine::get_clock(), true);
            const int candidateNodesToAssign = targetNodes - currentNodes;
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
        job->updateState();
    }
}

std::vector<Job*> AverageCommonPoolScheduler::schedule(const SchedulingContext& context) {
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
        for (Job* pendingJob : pendingNoAgree) {
            int requiredNodes = pendingJob->getNumNodesMin();
            auto jobsToShrink = selectShrinkJobs(runningMalleableJobs, requiredNodes, *agreements);
            if (!jobsToShrink.empty()) {
                MalleabilityHelper::applyShrinking(jobsToShrink, pendingJob, *agreements);
            }
        }
    }

    if (!freeNoAgree.empty() && !runningMalleableJobs.empty()) {
        expandAverageJobs(runningMalleableJobs, freeNoAgree);
    }

    return scheduledJobs;
}
