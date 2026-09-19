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

#include "EventLogger.h"
#include "../software/Job.h"
#include "../system/Node.h"
#include "Configuration.h"
#include "PerformanceTracer.h"
#include <simgrid/s4u.hpp>
#include <sstream>
#include <fstream>

void EventLogger::logJobShrink(Job* job, const std::vector<Node*>& removedNodes) {
    std::string nodeIds = nodeIdsToString(removedNodes);
    writeEvent("SHRINK", formatJobId(job->getId()), nodeIds);
}

void EventLogger::logJobExpand(Job* job, const std::vector<Node*>& addedNodes) {
    std::string nodeIds = nodeIdsToString(addedNodes);
    writeEvent("EXPAND", formatJobId(job->getId()), nodeIds);
}

void EventLogger::logJobStart(Job* job, const std::vector<Node*>& assignedNodes) {
    std::string nodeIds = nodeIdsToString(assignedNodes);
    writeEvent("START", formatJobId(job->getId()), nodeIds);
}

void EventLogger::logAgreementAdded(Job* shrinkJob, Job* pendingJob, 
                                    const std::vector<Node*>& nodes) {
    std::string nodeIds = nodeIdsToString(nodes);
    std::string jobLabel = formatJobId(shrinkJob->getId()) + " -> " + formatJobId(pendingJob->getId());
    writeEvent("AGREEMENT_ADDED", jobLabel, nodeIds);
}

void EventLogger::logAgreementFulfilled(Job* job, const std::vector<Node*>& nodes) {
    std::string nodeIds = nodeIdsToString(nodes);
    writeEvent("AGREEMENT_FULLFILLED", formatJobId(job->getId()), nodeIds);
}

void EventLogger::logShutdownShrink(int jobsShrunk, int nodesRemoved) {
    if (jobsShrunk <= 0 || nodesRemoved <= 0) {
        return;
    }
    std::string jobLabel = "jobs=" + std::to_string(jobsShrunk);
    std::string nodeLabel = "nodes=" + std::to_string(nodesRemoved);
    writeEvent("SHUTDOWN_SHRINK", jobLabel, nodeLabel);
}

std::string EventLogger::getEventLogPath() {
    // Check if configured
    if (Configuration::exists("event_log")) {
        return std::string(Configuration::get("event_log"));
    }
    // Default path
    return "runtime_files/event.csv";
}

std::string EventLogger::nodeIdsToString(const std::vector<Node*>& nodes) {
    if (nodes.empty()) {
        return "";
    }
    
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (i > 0) {
            oss << ", ";
        }
        oss << "N" << nodes[i]->getId();
    }
    oss << "]";
    return oss.str();
}

std::string EventLogger::formatJobId(int jobId) {
    return "J" + std::to_string(jobId);
}

void EventLogger::writeEvent(const std::string& eventType,
                             const std::string& jobLabel,
                             const std::string& nodeIds) {
    PerformanceTracer::ScopedPhase phase("event_log_write");
    PerformanceTracer::ScopedComponent component("event_log_write");
    std::string logPath = getEventLogPath();
    
    // Open file in append mode
    std::ofstream file(logPath, std::ios::app);
    if (!file.is_open()) {
        // Silent fail - don't crash simulation if logging fails
        return;
    }

    if (file.tellp() == 0) {
        file << "Time,Event,Jobs,Nodes\n";
    }
    
    // Get current simulation time
    int currentTime = static_cast<int>(simgrid::s4u::Engine::get_clock());
    
    // Write CSV line: timestamp,event_type,job_id,node_ids,additional_info
    file << currentTime << ","
         << eventType << ","
         << jobLabel << ",";

    if (nodeIds.find(',') != std::string::npos) {
        file << "\"" << nodeIds << "\"";
    } else {
        file << nodeIds;
    }
    file << "\n";
    file.close();
    PerformanceTracer::recordEventLogWrite();
}
