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

#pragma once

#include <string>
#include <vector>
#include <map>
#include "../third-party/nlohmann_json/json.hpp"

class Node;
class Job;

struct ShutdownWindow {
    int startHour;
    int endHour;
    double targetFraction;
    
    bool isInWindow(double currentTime) const;
    bool isInDrainLeadTime(double currentTime, int drainLeadTime) const;
    double getNextShutdownTime(double currentTime, int drainLeadTime) const;
};

class ShutdownPolicyManager {
public:
    static void init(const std::string& policyFilePath);
    static void update(double currentTime, const std::vector<Node*>& allNodes);
    static void applyMalleableShutdownPolicy(const std::vector<Job*>& jobs,
                                             const std::vector<Node*>& allNodes,
                                             double currentTime);
    
    static bool isActive() { return policyLoaded; }
    static bool canNodeAcceptJob(Node* node,
                                 double predictedRuntime,
                                 double currentTime,
                                 bool ignoreTargetPool = false);
    
private:
    // Policy loaded flag
    static bool policyLoaded;
    
    // JSON config values
    static std::string policyType;
    static std::string policyName;
    static std::vector<ShutdownWindow> shutdownWindows;
    static std::string drainingStrategy;
    static double corePoolFraction;
    
    // Timing parameters (from JSON)
    static int drainLeadTime;
    static int T_boot;
    static int T_down;
    static int maxDrainDuration;
    
    // Power parameters (from JSON)
    static double P_boot;
    static double P_down;
    static double P_off;
    
    // Policy flags (from JSON)
    static bool forceClearOnWindowEnd;
    static int jobAcceptanceMargin;
    static bool useTimeWindow;
    static int windowStart;
    static int windowEnd;
    static bool malleableShrinkToTarget;
    static int malleableShrinkLeadTime;
    static std::string malleableShrinkPriority;
    static bool shutdownTargetPool;
    static double lastShrinkLogTime;
    
    // Node pool assignments
    static std::map<Node*, bool> isNodeInCorePool;
    static std::map<Node*, double> nodeDrainStartTime;
    static std::map<Node*, bool> nodeTargetedForShutdown;  // NEW: Track which nodes should shutdown
    
    // Window tracking for dynamic target reassignment
    static int currentActiveWindowIndex;  // Track which window is currently active/draining (-1 = none)
    static int currentPoolWindowIndex;
    
    // Helper methods
    static void assignNodePools(const std::vector<Node*>& allNodes);
    static void assignShutdownTargets(const std::vector<Node*>& allNodes, double currentTime);  // NEW
    static void assignShutdownTargetsForFraction(const std::vector<Node*>& allNodes, double targetFraction);
    static void updateNodeStates(double currentTime, const std::vector<Node*>& allNodes);
    static bool shouldDrainNode(Node* node, double currentTime);
    static bool shouldShutdownNode(Node* node, double currentTime);
    static bool shouldBootNode(Node* node, double currentTime);
    static bool shouldResetDrainingNode(Node* node, double currentTime);  // NEW
    static bool isPolicyActiveAtTime(double currentTime);
    static double getNextShutdownWindowStart(double currentTime);  // NEW
};
