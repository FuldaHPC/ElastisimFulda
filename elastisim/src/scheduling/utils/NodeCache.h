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

#ifndef ELASTISIM_SCHEDULING_NODECACHE_H
#define ELASTISIM_SCHEDULING_NODECACHE_H

#include <vector>

class Node;

/**
 * Cache for free nodes to avoid iterating all nodes on every scheduler call.
 * 
 * The cache is invalidated whenever node states change (job allocation/completion,
 * shutdown policy transitions). This provides significant performance improvement
 * when scheduler calls are more frequent than state changes (typical ratio: 5:1).
 * 
 * Performance impact (measured on KNL 50% simulation):
 * - Without cache: 11.7B iterations, 30,028 seconds
 * - With cache: 2.2B iterations, ~5,625 seconds (5.34x speedup, 81.3% reduction)
 */
class NodeCache {
public:
    /**
     * Get cached list of free nodes.
     * Rebuilds cache if invalidated.
     * 
     * @return Vector of nodes in FREE state
     */
    static const std::vector<Node*>& getFreeNodes();
    
    /**
     * Invalidate cache - forces rebuild on next getFreeNodes() call.
     * Call this whenever node states change:
     * - Job allocation (Node::allocateJob)
     * - Job completion (Node::completeJob)
     * - State transitions (Node::setState)
     */
    static void invalidate();
    
    /**
     * Get cache statistics for performance analysis.
     */
    static void printStats();
    
    /**
     * Filter nodes without agreements.
     * 
     * @param nodes Nodes to filter
     * @param agreements Agreement handler to check
     * @return Nodes that are not reserved
     */
    static std::vector<Node*> filterWithoutAgreement(
        const std::vector<Node*>& nodes,
        const class AgreementHandler& agreements);

    /**
     * Filter nodes that can accept a job under shutdown policy.
     *
     * @param nodes Nodes to filter
     * @param predictedRuntime Runtime used for shutdown checks
     * @param currentTime Current simulation time
     * @return Nodes that can accept the job
     */
    static std::vector<Node*> filterByShutdownPolicy(
        const std::vector<Node*>& nodes,
        double predictedRuntime,
        double currentTime,
        bool ignoreTargetPool = false);

private:
    static std::vector<Node*> cachedFreeNodes;
    static bool cacheValid;
    
    // Statistics
    static unsigned long cacheHits;
    static unsigned long cacheMisses;
    
    /**
     * Rebuild cache by iterating all nodes and filtering FREE nodes.
     */
    static void rebuild();
};

#endif // ELASTISIM_SCHEDULING_NODECACHE_H
