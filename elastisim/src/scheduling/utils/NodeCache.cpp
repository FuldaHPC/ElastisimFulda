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

#include "NodeCache.h"
#include "../system/Node.h"
#include "../system/PlatformManager.h"
#include "../system/ShutdownPolicyManager.h"
#include "../malleability/AgreementHandler.h"
#include "../../util/PerformanceTracer.h"
#include <simgrid/s4u.hpp>
#include <iostream>

XBT_LOG_NEW_DEFAULT_CATEGORY(NodeCache, "Node cache utility");

// Static member initialization
std::vector<Node*> NodeCache::cachedFreeNodes;
bool NodeCache::cacheValid = false;
unsigned long NodeCache::cacheHits = 0;
unsigned long NodeCache::cacheMisses = 0;

const std::vector<Node*>& NodeCache::getFreeNodes() {
    if (!cacheValid) {
        rebuild();
        cacheMisses++;
        PerformanceTracer::recordNodeCacheMiss();
    } else {
        cacheHits++;
        PerformanceTracer::recordNodeCacheHit();
    }
    PerformanceTracer::setFreeNodesCached(cachedFreeNodes.size());
    return cachedFreeNodes;
}

void NodeCache::invalidate() {
    cacheValid = false;
}

void NodeCache::rebuild() {
    PerformanceTracer::ScopedPhase phase("node_cache_rebuild");
    cachedFreeNodes.clear();
    
    const std::vector<Node*>& allNodes = PlatformManager::getComputeNodes();
    for (Node* node : allNodes) {
        if (!node) {
            continue;
        }
        
        // Check if node is FREE
        nlohmann::json nodeJson = node->toJson();
        int state = nodeJson["state"];
        
        if (state == NODE_FREE) {
            cachedFreeNodes.push_back(node);
        }
    }
    
    cacheValid = true;
    PerformanceTracer::recordNodeCacheRebuild(allNodes.size());
    
    XBT_DEBUG("Cache rebuilt: %zu free nodes of %zu total at t=%.1f",
             cachedFreeNodes.size(), allNodes.size(), 
             simgrid::s4u::Engine::get_clock());
}

void NodeCache::printStats() {
    unsigned long totalAccesses = cacheHits + cacheMisses;
    double hitRate = totalAccesses > 0 ? (100.0 * cacheHits / totalAccesses) : 0.0;
    
    std::cout << "\n=== NodeCache Statistics ===" << std::endl;
    std::cout << "Cache hits:    " << cacheHits << std::endl;
    std::cout << "Cache misses:  " << cacheMisses << " (rebuilds)" << std::endl;
    std::cout << "Total access:  " << totalAccesses << std::endl;
    std::cout << "Hit rate:      " << hitRate << "%" << std::endl;
    std::cout << "Expected ratio: ~80% (5:1 scheduler calls per state change)" << std::endl;
    std::cout << "==========================\n" << std::endl;
}

std::vector<Node*> NodeCache::filterWithoutAgreement(
    const std::vector<Node*>& nodes,
    const AgreementHandler& agreements) {
    std::vector<Node*> filtered;
    for (Node* node : nodes) {
        if (node && !agreements.hasAgreement(node)) {
            filtered.push_back(node);
        }
    }
    return filtered;
}

std::vector<Node*> NodeCache::filterByShutdownPolicy(
    const std::vector<Node*>& nodes,
    double predictedRuntime,
    double currentTime,
    bool ignoreTargetPool) {
    PerformanceTracer::ScopedPhase phase("shutdown_filter");
    std::vector<Node*> filtered;
    filtered.reserve(nodes.size());
    for (Node* node : nodes) {
        if (!node) {
            continue;
        }
        if (ShutdownPolicyManager::canNodeAcceptJob(
                node, predictedRuntime, currentTime, ignoreTargetPool)) {
            filtered.push_back(node);
        }
    }
    PerformanceTracer::recordShutdownFilter(nodes.size(), filtered.size());
    return filtered;
}
