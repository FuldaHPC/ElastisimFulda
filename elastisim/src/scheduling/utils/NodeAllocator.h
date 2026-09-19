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

#ifndef ELASTISIM_SCHEDULING_NODEALLOCATOR_H
#define ELASTISIM_SCHEDULING_NODEALLOCATOR_H

#include <vector>

class Node;

/**
 * Utility for finding and allocating compute nodes.
 * Integrates shutdown policy checks for intelligent node selection.
 */
class NodeAllocator {
public:
    /**
     * Options for node allocation.
     */
    struct AllocationOptions {
        int requiredNodes;
        double predictedRuntime;
        double currentTime;
        bool respectShutdownPolicy;
        
        AllocationOptions() 
            : requiredNodes(0)
            , predictedRuntime(-1.0)
            , currentTime(-1.0)
            , respectShutdownPolicy(true)
        {}
    };
    
    /**
     * Find free nodes for job allocation.
     * Filters nodes by state and optionally checks shutdown policy.
     * 
     * @param options Allocation options including runtime and policy check
     * @param excludeNodes Nodes to exclude (already assigned this round)
     * @return Vector of available nodes, empty if insufficient nodes available
     */
    static std::vector<Node*> findFreeNodes(
        const AllocationOptions& options,
        const std::vector<Node*>& excludeNodes = {}
    );
    
    /**
     * Check if sufficient nodes are available for allocation.
     * Does not perform shutdown policy checks.
     * 
     * @param requiredNodes Number of nodes needed
     * @param excludeNodes Nodes to exclude from count
     * @return true if enough free nodes exist
     */
    static bool canAllocate(
        int requiredNodes,
        const std::vector<Node*>& excludeNodes = {}
    );
    
private:
    /**
     * Check if individual node is available for job.
     * Includes shutdown policy check if enabled.
     */
    static bool isNodeAvailable(
        Node* node,
        const AllocationOptions& options
    );
};

#endif // ELASTISIM_SCHEDULING_NODEALLOCATOR_H
