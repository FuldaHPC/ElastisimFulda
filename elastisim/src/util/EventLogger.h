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

#ifndef ELASTISIM_EVENTLOGGER_H
#define ELASTISIM_EVENTLOGGER_H

#include <vector>
#include <string>
#include <fstream>

class Job;
class Node;

/**
 * Event logger for malleability events.
 * Logs shrink/expand and agreement events to CSV in Python-compatible format.
 * 
 * Events are written to: event.csv (configured via "event_log" setting)
 * Format: timestamp,event_type,job_id,node_ids,additional_info
 */
class EventLogger {
public:
    /**
     * Log job shrink event (nodes removed from running job).
     * 
     * Python equivalent:
     *   event.csv: time,shrink,job_id,[removed_node_ids]
     */
    static void logJobShrink(Job* job, const std::vector<Node*>& removedNodes);
    
    /**
     * Log job expand event (nodes added to running job).
     * 
     * Python equivalent:
     *   event.csv: time,expand,job_id,[added_node_ids]
     */
    static void logJobExpand(Job* job, const std::vector<Node*>& addedNodes);

    /**
     * Log job start event (initial allocation).
     *
     * Python equivalent:
     *   event.csv: time,START,J<job_id>,[node_ids]
     */
    static void logJobStart(Job* job, const std::vector<Node*>& assignedNodes);
    
    /**
     * Log agreement added event (nodes reserved for pending job).
     * 
     * @param shrinkJob Job that was shrunk to provide nodes
     * @param pendingJob Job that will receive the nodes
     * @param nodes Nodes that were reserved
     * 
     * Python equivalent:
     *   event.csv: time,agreement_added,pending_job_id,[node_ids],from_job_id=shrink_job_id
     */
    static void logAgreementAdded(Job* shrinkJob, Job* pendingJob, 
                                  const std::vector<Node*>& nodes);
    
    /**
     * Log agreement fulfilled event (reserved nodes allocated to job).
     * 
     * Python equivalent:
     *   event.csv: time,agreement_fulfilled,job_id,[node_ids]
     */
    static void logAgreementFulfilled(Job* job, const std::vector<Node*>& nodes);
    static void logShutdownShrink(int jobsShrunk, int nodesRemoved);

private:
    /**
     * Get CSV file path from configuration.
     * Returns: Path to event.csv (default: "runtime_files/event.csv")
     */
    static std::string getEventLogPath();
    
    /**
     * Convert vector of nodes to comma-separated ID string.
     * Example: [node1, node2, node3] -> "1,2,3"
     */
    static std::string nodeIdsToString(const std::vector<Node*>& nodes);

    /**
     * Format job id for event log (Python format: J<id>).
     */
    static std::string formatJobId(int jobId);
    
    /**
     * Write event line to CSV file.
     * Format: timestamp,event_type,job_id,node_ids,additional_info
     */
    static void writeEvent(const std::string& eventType,
                          const std::string& jobLabel,
                          const std::string& nodeIds);
};

#endif //ELASTISIM_EVENTLOGGER_H
