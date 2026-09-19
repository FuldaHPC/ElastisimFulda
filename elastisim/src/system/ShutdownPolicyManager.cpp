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

#include "ShutdownPolicyManager.h"
#include "Node.h"
#include "../software/Job.h"
#include "../util/EventLogger.h"
#include "../util/PerformanceTracer.h"
#include <fstream>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <simgrid/s4u.hpp>

XBT_LOG_NEW_DEFAULT_CATEGORY(ShutdownPolicyManager, "Shutdown policy manager");

// Static member initialization - no defaults, must come from config
bool ShutdownPolicyManager::policyLoaded = false;
std::string ShutdownPolicyManager::policyType;
std::string ShutdownPolicyManager::policyName;
std::vector<ShutdownWindow> ShutdownPolicyManager::shutdownWindows;
std::string ShutdownPolicyManager::drainingStrategy;
double ShutdownPolicyManager::corePoolFraction = 0.0;
int ShutdownPolicyManager::drainLeadTime = 0;
int ShutdownPolicyManager::T_boot = 0;
int ShutdownPolicyManager::T_down = 0;
int ShutdownPolicyManager::maxDrainDuration = 0;
double ShutdownPolicyManager::P_boot = 0.0;
double ShutdownPolicyManager::P_down = 0.0;
double ShutdownPolicyManager::P_off = 0.0;
bool ShutdownPolicyManager::forceClearOnWindowEnd = false;
int ShutdownPolicyManager::jobAcceptanceMargin = 0;
bool ShutdownPolicyManager::useTimeWindow = false;
int ShutdownPolicyManager::windowStart = 0;
int ShutdownPolicyManager::windowEnd = 0;
bool ShutdownPolicyManager::malleableShrinkToTarget = false;
int ShutdownPolicyManager::malleableShrinkLeadTime = 0;
std::string ShutdownPolicyManager::malleableShrinkPriority = "min";
bool ShutdownPolicyManager::shutdownTargetPool = false;
double ShutdownPolicyManager::lastShrinkLogTime = -1.0;
std::map<Node*, bool> ShutdownPolicyManager::isNodeInCorePool;
std::map<Node*, double> ShutdownPolicyManager::nodeDrainStartTime;
std::map<Node*, bool> ShutdownPolicyManager::nodeTargetedForShutdown;
int ShutdownPolicyManager::currentActiveWindowIndex = -1;
int ShutdownPolicyManager::currentPoolWindowIndex = -1;

void ShutdownPolicyManager::init(const std::string& policyFilePath) {
    if (policyFilePath.empty()) {
        policyLoaded = false;
        return;
    }
    
    std::ifstream file(policyFilePath);
    if (!file.is_open()) {
        std::cerr << "ERROR: Cannot open shutdown policy file: " << policyFilePath << std::endl;
        throw std::runtime_error("Shutdown policy file not found: " + policyFilePath);
    }
    
    nlohmann::json policyJson;
    try {
        file >> policyJson;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Parsing shutdown policy JSON: " << e.what() << std::endl;
        throw std::runtime_error("Invalid JSON in policy file: " + policyFilePath);
    }
    
    // Validate required fields
    if (!policyJson.contains("policy_type")) {
        throw std::runtime_error("Missing required field: policy_type");
    }
    if (!policyJson.contains("shutdown_windows")) {
        throw std::runtime_error("Missing required field: shutdown_windows");
    }
    if (!policyJson.contains("timing")) {
        throw std::runtime_error("Missing required field: timing");
    }
    
    // Load basic policy info
    policyType = policyJson["policy_type"];
    policyName = policyJson.value("policy_name", "unnamed_policy");
    drainingStrategy = policyJson.value("draining_strategy", "simple");
    
    // Load core pool fraction if using flex_core_pool strategy
    if (drainingStrategy == "flex_core_pool") {
        if (!policyJson.contains("core_pool_fraction")) {
            throw std::runtime_error("flex_core_pool strategy requires core_pool_fraction");
        }
        corePoolFraction = policyJson["core_pool_fraction"];
        if (corePoolFraction <= 0.0 || corePoolFraction >= 1.0) {
            throw std::runtime_error("core_pool_fraction must be between 0 and 1");
        }
    }
    
    // Load timing parameters (all required)
    const auto& timing = policyJson["timing"];
    if (!timing.contains("drain_lead_time")) {
        throw std::runtime_error("Missing required timing field: drain_lead_time");
    }
    if (!timing.contains("T_boot")) {
        throw std::runtime_error("Missing required timing field: T_boot");
    }
    if (!timing.contains("T_down")) {
        throw std::runtime_error("Missing required timing field: T_down");
    }
    if (!timing.contains("max_drain_duration")) {
        throw std::runtime_error("Missing required timing field: max_drain_duration");
    }
    
    drainLeadTime = timing["drain_lead_time"];
    T_boot = timing["T_boot"];
    T_down = timing["T_down"];
    maxDrainDuration = timing["max_drain_duration"];
    
    if (drainLeadTime < 0 || T_boot < 0 || T_down < 0 || maxDrainDuration < 0) {
        throw std::runtime_error("Timing parameters must be non-negative");
    }
    
    // Load power parameters (optional with zero defaults)
    if (policyJson.contains("power")) {
        const auto& power = policyJson["power"];
        P_boot = power.value("P_boot", 0.0);
        P_down = power.value("P_down", 0.0);
        P_off = power.value("P_off", 0.0);
    }
    
    // Load policy flags
    if (policyJson.contains("flags")) {
        const auto& flags = policyJson["flags"];
        forceClearOnWindowEnd = flags.value("force_clear_on_window_end", false);
        jobAcceptanceMargin = flags.value("job_acceptance_margin", 0);
        malleableShrinkToTarget = flags.value("malleable_shrink_to_target", false);
        malleableShrinkLeadTime = flags.value("malleable_shrink_lead_time", 0);
        malleableShrinkPriority = flags.value("malleable_shrink_priority", "min");
        shutdownTargetPool = flags.value("shutdown_target_pool", false);
    }
    
    useTimeWindow = policyJson.value("use_time_window", false);
    if (useTimeWindow) {
        if (!policyJson.contains("window_start") || !policyJson.contains("window_end")) {
            throw std::runtime_error("use_time_window=true requires window_start and window_end");
        }
        windowStart = policyJson["window_start"];
        windowEnd = policyJson["window_end"];
    }
    
    // Load shutdown windows (required, at least one)
    const auto& windowsArray = policyJson["shutdown_windows"];
    if (windowsArray.empty()) {
        throw std::runtime_error("shutdown_windows must contain at least one window");
    }
    
    for (const auto& windowJson : windowsArray) {
        if (!windowJson.contains("start_hour") || !windowJson.contains("end_hour") || 
            !windowJson.contains("target_fraction")) {
            throw std::runtime_error("Each shutdown_window must have start_hour, end_hour, and target_fraction");
        }
        
        ShutdownWindow window;
        window.startHour = windowJson["start_hour"];
        window.endHour = windowJson["end_hour"];
        window.targetFraction = windowJson["target_fraction"];
        
        if (window.targetFraction <= 0.0 || window.targetFraction > 1.0) {
            throw std::runtime_error("target_fraction must be between 0 and 1");
        }
        if (window.startHour < 0 || window.startHour >= 24) {
            throw std::runtime_error("start_hour must be between 0 and 23");
        }
        if (window.endHour < 0 || window.endHour >= 24) {
            throw std::runtime_error("end_hour must be between 0 and 23");
        }
        
        shutdownWindows.push_back(window);
    }
    
    policyLoaded = true;
    
    // Log loaded configuration for verification
    std::cout << "Shutdown policy loaded: " << policyName << " (" << policyType << ")" << std::endl;
    std::cout << "  Strategy: " << drainingStrategy;
    if (drainingStrategy == "flex_core_pool") {
        std::cout << " (core: " << (corePoolFraction * 100) << "%, flex: " << ((1.0 - corePoolFraction) * 100) << "%)";
    }
    std::cout << std::endl;
    std::cout << "  Windows: " << shutdownWindows.size() << std::endl;
    for (size_t i = 0; i < shutdownWindows.size(); ++i) {
        std::cout << "    [" << i << "] " << shutdownWindows[i].startHour << ":00-" 
                  << shutdownWindows[i].endHour << ":00 (target: " 
                  << (shutdownWindows[i].targetFraction * 100) << "% active)" << std::endl;
    }
    std::cout << "  Timing: drain_lead=" << drainLeadTime << "s, max_drain=" << maxDrainDuration 
              << "s, T_boot=" << T_boot << "s, T_down=" << T_down << "s" << std::endl;
    std::cout << "  Flags: force_clear=" << (forceClearOnWindowEnd ? "true" : "false")
              << ", job_margin=" << jobAcceptanceMargin << "s"
              << ", malleable_shrink=" << (malleableShrinkToTarget ? "true" : "false")
              << ", shrink_lead=" << malleableShrinkLeadTime << "s"
              << ", shrink_prio=" << malleableShrinkPriority
              << ", shutdown_pool=" << (shutdownTargetPool ? "true" : "false") << std::endl;
}

bool ShutdownPolicyManager::isPolicyActiveAtTime(double currentTime) {
    if (!useTimeWindow) return true;
    return currentTime >= windowStart && currentTime <= windowEnd;
}

void ShutdownPolicyManager::update(double currentTime, const std::vector<Node*>& allNodes) {
    if (!policyLoaded) return;
    if (!isPolicyActiveAtTime(currentTime)) return;
    
    // Lazy initialization: Assign core/flex pools on first call
    if (isNodeInCorePool.empty() && drainingStrategy == "flex_core_pool") {
        assignNodePools(allNodes);
    }
    
    // For "simple" strategy: Check if we need to reassign shutdown targets
    if (drainingStrategy == "simple") {
        // Determine which window is currently in drain lead time or active
        int newWindowIndex = -1;
        for (size_t i = 0; i < shutdownWindows.size(); ++i) {
            if (shutdownWindows[i].isInDrainLeadTime(currentTime, drainLeadTime) ||
                shutdownWindows[i].isInWindow(currentTime)) {
                newWindowIndex = static_cast<int>(i);
                break;
            }
        }
        
        // Reassign targets if window changed OR first time
        if (newWindowIndex != currentActiveWindowIndex) {
            std::cout << "[ShutdownPolicy] Window transition from " << currentActiveWindowIndex 
                      << " to " << newWindowIndex << " at t=" << (currentTime/3600.0) << "h" << std::endl;
            currentActiveWindowIndex = newWindowIndex;
            if (newWindowIndex >= 0) {
                assignShutdownTargets(allNodes, currentTime);
            }
        }
    }

    if (shutdownTargetPool) {
        int nextWindowIndex = -1;
        double nextShutdownTime = std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < shutdownWindows.size(); ++i) {
            double candidate = shutdownWindows[i].getNextShutdownTime(currentTime, drainLeadTime);
            if (candidate < nextShutdownTime) {
                nextShutdownTime = candidate;
                nextWindowIndex = static_cast<int>(i);
            }
        }

        if (nextWindowIndex >= 0 &&
            (nextWindowIndex != currentPoolWindowIndex || nodeTargetedForShutdown.empty())) {
            assignShutdownTargetsForFraction(allNodes, shutdownWindows[nextWindowIndex].targetFraction);
            currentPoolWindowIndex = nextWindowIndex;
        }
    }
    
    updateNodeStates(currentTime, allNodes);
}

void ShutdownPolicyManager::applyMalleableShutdownPolicy(const std::vector<Job*>& jobs,
                                                         const std::vector<Node*>& allNodes,
                                                         double currentTime) {
    if (!policyLoaded || !malleableShrinkToTarget) {
        return;
    }
    if (!isPolicyActiveAtTime(currentTime)) {
        return;
    }

    bool inShrinkPhase = false;
    int shrinkWindowIndex = -1;
    bool inShutdownWindow = false;
    for (size_t i = 0; i < shutdownWindows.size(); ++i) {
        const auto& window = shutdownWindows[i];
        if (window.isInWindow(currentTime)) {
            inShutdownWindow = true;
            inShrinkPhase = true;
            shrinkWindowIndex = static_cast<int>(i);
            break;
        }

        if (malleableShrinkLeadTime > 0) {
            double shutdownTime = window.getNextShutdownTime(currentTime, drainLeadTime);
            if (currentTime >= shutdownTime - malleableShrinkLeadTime &&
                currentTime < shutdownTime) {
                inShrinkPhase = true;
                shrinkWindowIndex = static_cast<int>(i);
                break;
            }
        }
    }

    if (!inShrinkPhase) {
        return;
    }

    if (nodeTargetedForShutdown.empty() && shrinkWindowIndex >= 0) {
        assignShutdownTargetsForFraction(allNodes, shutdownWindows[shrinkWindowIndex].targetFraction);
    }

    if (nodeTargetedForShutdown.empty()) {
        return;
    }

    std::vector<Job*> shrinkJobs;
    shrinkJobs.reserve(jobs.size());
    for (Job* job : jobs) {
        if (!job) {
            continue;
        }
        if (job->getState() != RUNNING) {
            continue;
        }
        if (job->getType() != MALLEABLE) {
            continue;
        }
        shrinkJobs.push_back(job);
    }

    auto shrinkPriority = [](Job* job) -> double {
        if (!job) {
            return 0.0;
        }
        int assigned = static_cast<int>(job->getAssignedNodes().size());
        int minNodes = job->getNumNodesMin();
        if (malleableShrinkPriority == "pref") {
            return static_cast<double>(assigned - job->getNumNodesPref());
        }
        if (malleableShrinkPriority == "average") {
            int range = job->getNumNodesMax() - minNodes;
            if (range <= 0) {
                return 0.0;
            }
            return static_cast<double>(assigned - minNodes) / static_cast<double>(range);
        }
        return static_cast<double>(assigned - minNodes);
    };

    std::stable_sort(shrinkJobs.begin(), shrinkJobs.end(),
                     [&shrinkPriority](Job* a, Job* b) {
                         return shrinkPriority(a) > shrinkPriority(b);
                     });

    bool anyShrink = false;
    int totalRemovedNodes = 0;
    int jobsShrunk = 0;
    for (Job* job : shrinkJobs) {
        const auto& assigned = job->getAssignedNodes();
        int minNodes = job->getNumNodesMin();
        if (static_cast<int>(assigned.size()) <= minNodes) {
            continue;
        }

        std::vector<Node*> nodesToRemove;
        for (size_t i = static_cast<size_t>(minNodes); i < assigned.size(); ++i) {
            Node* node = assigned[i];
            auto it = nodeTargetedForShutdown.find(node);
            if (it != nodeTargetedForShutdown.end() && it->second) {
                nodesToRemove.push_back(node);
            }
        }

        if (!nodesToRemove.empty()) {
            job->removeNodes(nodesToRemove);
            anyShrink = true;
            jobsShrunk += 1;
            totalRemovedNodes += static_cast<int>(nodesToRemove.size());
        }
    }

    if (anyShrink && inShutdownWindow) {
        updateNodeStates(currentTime, allNodes);
    }

    if (anyShrink) {
        PerformanceTracer::recordShrink(static_cast<size_t>(jobsShrunk),
                                        static_cast<size_t>(totalRemovedNodes));
        int now = static_cast<int>(currentTime);
        if (lastShrinkLogTime != static_cast<double>(now)) {
            lastShrinkLogTime = static_cast<double>(now);
            XBT_INFO("[ShutdownPolicy] Malleable shrink: jobs=%d nodes=%d (prio=%s lead=%ds)",
                     jobsShrunk, totalRemovedNodes, malleableShrinkPriority.c_str(),
                     malleableShrinkLeadTime);
            EventLogger::logShutdownShrink(jobsShrunk, totalRemovedNodes);
        }
    }
}

void ShutdownPolicyManager::assignNodePools(const std::vector<Node*>& allNodes) {
    int corePoolSize = static_cast<int>(allNodes.size() * corePoolFraction);
    
    std::cout << "Assigning node pools: " << corePoolSize << " core, " 
              << (allNodes.size() - corePoolSize) << " flex" << std::endl;
    
    for (size_t i = 0; i < allNodes.size(); ++i) {
        isNodeInCorePool[allNodes[i]] = (i < static_cast<size_t>(corePoolSize));
    }
}

void ShutdownPolicyManager::assignShutdownTargets(const std::vector<Node*>& allNodes, double currentTime) {
    // Determine how many nodes should be shutdown based on target_fraction
    double targetFraction = -1.0;
    for (const auto& window : shutdownWindows) {
        if (window.isInDrainLeadTime(currentTime, drainLeadTime)) {
            targetFraction = window.targetFraction;
            break;
        }
    }
    
    if (targetFraction < 0.0) {
        std::cout << "[ShutdownPolicy] Window active without drain lead time at t=" << currentTime << "s, drain target assignment skipped" << std::endl;
        return;
    }
    
    assignShutdownTargetsForFraction(allNodes, targetFraction);
}

void ShutdownPolicyManager::assignShutdownTargetsForFraction(const std::vector<Node*>& allNodes,
                                                             double targetFraction) {
    int targetShutdownCount = static_cast<int>(allNodes.size() * (1.0 - targetFraction));

    nodeTargetedForShutdown.clear();
    for (size_t i = 0; i < allNodes.size(); ++i) {
        nodeTargetedForShutdown[allNodes[i]] = (i < static_cast<size_t>(targetShutdownCount));
    }
}

void ShutdownPolicyManager::updateNodeStates(double currentTime, const std::vector<Node*>& allNodes) {
    PerformanceTracer::recordShutdownUpdateNodesScanned(allNodes.size());
    for (Node* node : allNodes) {
        NodeState currentState = node->getState();
        
        // State machine transitions
        switch (currentState) {
            case NODE_FREE:
            case NODE_ALLOCATED:
            case NODE_RESERVED:
                // Priority 1: If shutdown window is active AND node is idle AND targeted for shutdown
                // → Go directly to OFF (skip DRAINING)
                if (node->isIdle() && nodeTargetedForShutdown[node]) {
                    bool inShutdownWindow = false;
                    for (const auto& window : shutdownWindows) {
                        if (window.isInWindow(currentTime)) {
                            inShutdownWindow = true;
                            break;
                        }
                    }
                    
                    if (inShutdownWindow) {
                        node->setState(NODE_SHUTTING_DOWN);
                        node->scheduleTransition(NODE_OFF, T_down);
                        break;
                    }
                }
                
                // Priority 2: Normal drain lead time behavior
                if (shouldDrainNode(node, currentTime)) {
                    node->setState(NODE_DRAINING);
                    nodeDrainStartTime[node] = currentTime;
                }
                break;
                
            case NODE_DRAINING:
                // Priority 1: Check max_drain_duration timeout (highest priority)
                if (nodeDrainStartTime.count(node) > 0) {
                    double drainDuration = currentTime - nodeDrainStartTime[node];
                    if (drainDuration >= maxDrainDuration) {
                        node->setState(NODE_FREE);
                        nodeDrainStartTime.erase(node);
                        break;
                    }
                }
                
                // Priority 2: Try shutdown if idle and in window
                if (shouldShutdownNode(node, currentTime)) {
                    node->setState(NODE_SHUTTING_DOWN);
                    node->scheduleTransition(NODE_OFF, T_down);
                }
                // Priority 3: Check force_clear_on_window_end
                else if (forceClearOnWindowEnd) {
                    bool inAnyWindow = false;
                    for (const auto& window : shutdownWindows) {
                        if (window.isInWindow(currentTime) || window.isInDrainLeadTime(currentTime, drainLeadTime)) {
                            inAnyWindow = true;
                            break;
                        }
                    }
                    if (!inAnyWindow) {
                        node->setState(NODE_FREE);
                        nodeDrainStartTime.erase(node);
                    }
                }
                break;
                
            case NODE_SHUTTING_DOWN:
                // Transition to OFF handled by scheduleTransition callback
                break;
                
            case NODE_OFF:
                if (shouldBootNode(node, currentTime)) {
                    node->setState(NODE_BOOTING);
                    node->scheduleTransition(NODE_FREE, T_boot);
                }
                break;
                
            case NODE_BOOTING:
                // Transition to FREE handled by scheduleTransition callback
                break;
        }
    }
}

bool ShutdownPolicyManager::shouldDrainNode(Node* node, double currentTime) {
    // Check if we're in drain lead time before shutdown window
    bool inDrainLeadTime = false;
    for (const auto& window : shutdownWindows) {
        if (window.isInDrainLeadTime(currentTime, drainLeadTime)) {
            inDrainLeadTime = true;
            break;
        }
    }
    
    if (!inDrainLeadTime) return false;
    
    // Only drain if idle
    if (!node->isIdle()) return false;
    
    // Strategy-specific logic
    if (drainingStrategy == "flex_core_pool") {
        // Core pool nodes never drain
        return !isNodeInCorePool[node];
    } else if (drainingStrategy == "simple") {
        // Only drain if targeted for shutdown (respects target_fraction)
        return nodeTargetedForShutdown[node];
    }
    
    return false;
}

bool ShutdownPolicyManager::shouldShutdownNode(Node* node, double currentTime) {
    // Node must be in DRAINING state and have no running jobs
    if (!node->isIdle()) return false;
    
    // Check if shutdown window is active
    for (const auto& window : shutdownWindows) {
        if (window.isInWindow(currentTime)) {
            return true;
        }
    }
    
    return false;
}

bool ShutdownPolicyManager::shouldResetDrainingNode(Node* node, double currentTime) {
    // Check if max_drain_duration exceeded
    if (nodeDrainStartTime.find(node) == nodeDrainStartTime.end()) {
        return false;
    }
    
    double drainDuration = currentTime - nodeDrainStartTime[node];
    return drainDuration >= maxDrainDuration;
}

double ShutdownPolicyManager::getNextShutdownWindowStart(double currentTime) {
    // Calculate when next shutdown window starts (in seconds since epoch)
    int currentHour = (static_cast<int>(currentTime / 3600) % 24);
    int hoursUntilWindow = 0;
    
    for (const auto& window : shutdownWindows) {
        int startHour = window.startHour;
        if (currentHour < startHour) {
            hoursUntilWindow = startHour - currentHour;
        } else {
            hoursUntilWindow = (24 - currentHour) + startHour;
        }
        break;
    }
    
    return currentTime + (hoursUntilWindow * 3600);
}

bool ShutdownPolicyManager::shouldBootNode(Node* node, double currentTime) {
    // Check if shutdown window has ended
    for (const auto& window : shutdownWindows) {
        if (!window.isInWindow(currentTime) && !window.isInDrainLeadTime(currentTime, drainLeadTime)) {
            return true;
        }
    }
    
    return false;
}

// Uses job.attributes["predicted_runtime"] for draining decisions
bool ShutdownPolicyManager::canNodeAcceptJob(Node* node,
                                             double predictedRuntime,
                                             double currentTime,
                                             bool ignoreTargetPool) {
    PerformanceTracer::recordShutdownPolicyAcceptCheck();
    if (!policyLoaded) return true;
    if (!isPolicyActiveAtTime(currentTime)) return true;
    
    NodeState state = node->getState();
    
    // OFF and transitioning nodes cannot accept jobs
    if (state == NODE_OFF || state == NODE_SHUTTING_DOWN || state == NODE_BOOTING) {
        return false;
    }
    
    // Core pool nodes can always accept jobs
    if (drainingStrategy == "flex_core_pool" && isNodeInCorePool[node]) {
        return true;
    }

    bool inShutdownWindow = false;
    for (const auto& window : shutdownWindows) {
        if (window.isInWindow(currentTime)) {
            inShutdownWindow = true;
            break;
        }
    }

    bool isTargeted = false;
    auto targetIt = nodeTargetedForShutdown.find(node);
    if (targetIt != nodeTargetedForShutdown.end()) {
        isTargeted = targetIt->second;
    }

    if (shutdownTargetPool && isTargeted && !ignoreTargetPool) {
        if (inShutdownWindow) {
            return false;
        }

        double nextShutdownTime = std::numeric_limits<double>::infinity();
        for (const auto& window : shutdownWindows) {
            double candidate = window.getNextShutdownTime(currentTime, drainLeadTime);
            if (candidate < nextShutdownTime) {
                nextShutdownTime = candidate;
            }
        }

        if (currentTime + predictedRuntime > nextShutdownTime) {
            return false;
        }
    }
    
    // For DRAINING nodes: Check if job would finish before shutdown window starts
    // (with job_acceptance_margin applied)
    if (state == NODE_DRAINING || isTargeted) {
        for (const auto& window : shutdownWindows) {
            double nextShutdownTime = window.getNextShutdownTime(currentTime, drainLeadTime);
            double jobEndTime = currentTime + predictedRuntime;
            
            // Apply job_acceptance_margin:
            // Negative margin = job must finish BEFORE shutdown (more restrictive)
            // Positive margin = job can overrun INTO shutdown (less restrictive)
            double effectiveShutdownTime = nextShutdownTime + jobAcceptanceMargin;
            
            if (jobEndTime > effectiveShutdownTime) {
                return false; // Job would overlap with shutdown
            }
        }
    }
    
    // Default: accept job
    double jobEndTime = currentTime + predictedRuntime + jobAcceptanceMargin;
    
    for (const auto& window : shutdownWindows) {
        if (window.isInDrainLeadTime(currentTime, drainLeadTime)) {
            double shutdownTime = window.getNextShutdownTime(currentTime, drainLeadTime);
            
            if (jobEndTime > shutdownTime) {
                return false; // Job would not finish before shutdown
            }
        }
    }
    
    return true;
}

// ShutdownWindow helper methods
bool ShutdownWindow::isInWindow(double currentTime) const {
    int secondsInDay = 86400;
    int currentSecond = static_cast<int>(currentTime) % secondsInDay;
    int currentHour = currentSecond / 3600;
    
    if (startHour < endHour) {
        return currentHour >= startHour && currentHour < endHour;
    } else {
        // Overnight window (e.g., 22:00-6:00)
        return currentHour >= startHour || currentHour < endHour;
    }
}

bool ShutdownWindow::isInDrainLeadTime(double currentTime, int drainLeadTime) const {
    if (drainLeadTime <= 0) {
        return false;
    }
    int secondsInDay = 86400;
    int currentSecond = static_cast<int>(currentTime) % secondsInDay;
    int shutdownSecond = startHour * 3600;
    int drainStartSecond = (shutdownSecond - drainLeadTime + secondsInDay) % secondsInDay;
    
    // Check if current time is within drain lead time
    if (drainStartSecond < shutdownSecond) {
        return currentSecond >= drainStartSecond && currentSecond < shutdownSecond;
    } else {
        // Overnight drain period
        return currentSecond >= drainStartSecond || currentSecond < shutdownSecond;
    }
}

double ShutdownWindow::getNextShutdownTime(double currentTime, int drainLeadTime) const {
    int secondsInDay = 86400;
    int currentSecond = static_cast<int>(currentTime) % secondsInDay;
    int shutdownSecond = startHour * 3600;
    
    // Calculate how many full days have passed
    int fullDays = static_cast<int>(currentTime) / secondsInDay;
    
    // Next shutdown is at start_hour of current or next day
    if (currentSecond < shutdownSecond) {
        // Shutdown is later today
        return fullDays * secondsInDay + shutdownSecond;
    } else {
        // Shutdown is tomorrow
        return (fullDays + 1) * secondsInDay + shutdownSecond;
    }
}
