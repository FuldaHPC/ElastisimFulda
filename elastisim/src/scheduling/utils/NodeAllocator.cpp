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

#include "NodeAllocator.h"
#include "NodeCache.h"
#include "../system/Node.h"
#include "../system/PlatformManager.h"
#include "../system/ShutdownPolicyManager.h"
#include <simgrid/s4u.hpp>
#include <algorithm>
#include <set>

XBT_LOG_NEW_DEFAULT_CATEGORY(NodeAllocator, "Node allocation utility");

std::vector<Node*> NodeAllocator::findFreeNodes(
    const AllocationOptions& options,
    const std::vector<Node*>& excludeNodes) {
    
    std::vector<Node*> availableNodes;
    std::set<Node*> excludeSet(excludeNodes.begin(), excludeNodes.end());
    
    // Use cached free nodes instead of iterating all nodes
    const std::vector<Node*>& freeNodes = NodeCache::getFreeNodes();
    
    for (Node* node : freeNodes) {
        if (!node || excludeSet.count(node)) {
            continue;
        }
        
        if (isNodeAvailable(node, options)) {
            availableNodes.push_back(node);
            
            if (availableNodes.size() >= static_cast<size_t>(options.requiredNodes)) {
                break;
            }
        }
    }
    
    if (availableNodes.size() < static_cast<size_t>(options.requiredNodes)) {
        XBT_DEBUG("Insufficient nodes: needed %d, found %zu", 
                 options.requiredNodes, availableNodes.size());
        return {};
    }
    
    return availableNodes;
}

bool NodeAllocator::canAllocate(
    int requiredNodes,
    const std::vector<Node*>& excludeNodes) {
    
    std::set<Node*> excludeSet(excludeNodes.begin(), excludeNodes.end());
    int freeCount = 0;
    
    // Use cached free nodes instead of iterating all nodes
    const std::vector<Node*>& freeNodes = NodeCache::getFreeNodes();
    
    for (Node* node : freeNodes) {
        if (!node || excludeSet.count(node)) {
            continue;
        }
        
        freeCount++;
        if (freeCount >= requiredNodes) {
            return true;
        }
    }
    
    return false;
}

bool NodeAllocator::isNodeAvailable(
    Node* node,
    const AllocationOptions& options) {
    
    if (!node) {
        return false;
    }
    
    nlohmann::json nodeJson = node->toJson();
    int state = nodeJson["state"];
    
    // Only FREE nodes are available
    if (state != NODE_FREE) {
        return false;
    }
    
    // Check shutdown policy if enabled and runtime is known
    if (options.respectShutdownPolicy && 
        options.predictedRuntime > 0.0 && 
        options.currentTime >= 0.0) {
        
        if (!ShutdownPolicyManager::canNodeAcceptJob(
                node, 
                options.predictedRuntime, 
                options.currentTime)) {
            XBT_DEBUG("Node %s rejected by shutdown policy", node->getHostName().c_str());
            return false;
        }
    }
    
    return true;
}
