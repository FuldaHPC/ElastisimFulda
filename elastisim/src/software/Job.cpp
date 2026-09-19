/*
 * This file is part of the ElastiSim software.
 *
 * Copyright (c) 2022, Technical University of Darmstadt, Germany
 * Modifications Copyright (c) 2026 Fulda University of Applied Sciences, Germany
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This software may be modified and distributed under the terms of the 3-Clause
 * BSD License. See the LICENSE file in the base directory for details.
 *
 */


#include "Job.h"

#include <utility>
#include "Workload.h"
#include "Phase.h"
#include "Node.h"
#include "Task.h"
#include "Utility.h"
#include "Configuration.h"
#include "PlatformManager.h"
#include "PerformanceTracer.h"
#include "../scheduling/utils/JobUtils.h"

Job::Job(int walltime, int numNodes, int numGpusPerNode, double submitTime,
	std::map<std::string, std::string> arguments, std::map<std::string, std::string> attributes,
			 std::unique_ptr<Workload> workload) :
			id(-1), type(RIGID), state(PENDING_SUBMISSION), walltime(walltime), numNodes(numNodes),
			numGpusPerNode(numGpusPerNode), numNodesMin(-1), numNodesMax(-1), numGpusPerNodeMin(-1), numGpusPerNodeMax(-1),
				submitTime(submitTime), startTime(-1), endTime(-1), waitTime(-1), makespan(-1), turnaroundTime(-1),
				workload(std::move(workload)), arguments(std::move(arguments)), attributes(std::move(attributes)),
				runtimeArgumentsMutex(s4u_Mutex::create()), assignedNumGpusPerNode(0), executingNumGpusPerNode(0),
				clipEvolvingRequests(false), schedulingPointDecision(SCHEDULING_POINT_DECISION_UNSET),
				predictionStateInitialized(false), predictionReferenceNodes(0), predictionReferenceRuntime(0.0),
				predictedRemainingWorkRef(0.0), lastPredictionUpdateTime(0.0), lastPredictionNodes(0) {
	checkSpecification();
}

Job::Job(int walltime, JobType type, int numNodesMin, int numNodesMax, int numGpusPerNodeMin, int numGpusPerNodeMax,
		 double submitTime, std::map<std::string, std::string> arguments, std::map<std::string, std::string> attributes,
		 std::unique_ptr<Workload> workload) :
		id(-1), type(type), state(PENDING_SUBMISSION), walltime(walltime),
			numNodes(-1), numGpusPerNode(-1), numNodesMin(numNodesMin), numNodesMax(numNodesMax),
			numGpusPerNodeMin(numGpusPerNodeMin), numGpusPerNodeMax(numGpusPerNodeMax), submitTime(submitTime),
				startTime(-1), endTime(-1), waitTime(-1), makespan(-1), turnaroundTime(-1), workload(std::move(workload)),
				arguments(std::move(arguments)), attributes(std::move(attributes)), runtimeArgumentsMutex(s4u_Mutex::create()),
				assignedNumGpusPerNode(0), executingNumGpusPerNode(0),
				clipEvolvingRequests(!Configuration::exists("clip_evolving_requests") ||
									 (bool) Configuration::get("clip_evolving_requests")),
				schedulingPointDecision(SCHEDULING_POINT_DECISION_UNSET),
				predictionStateInitialized(false), predictionReferenceNodes(0), predictionReferenceRuntime(0.0),
				predictedRemainingWorkRef(0.0), lastPredictionUpdateTime(0.0), lastPredictionNodes(0) {
	checkSpecification();
	additionalArguments["num_nodes_min"] = std::to_string(numNodesMin);
	additionalArguments["num_nodes_max"] = std::to_string(numNodesMax);
}

int Job::getId() const {
	return id;
}

void Job::setId(int id) {
	Job::id = id;
}

JobType Job::getType() const {
	return type;
}

JobState Job::getState() const {
	return state;
}

void Job::setState(JobState newState) {
	PerformanceTracer::ScopedComponent component("job_set_state");
	if (state == PENDING_ALLOCATION) {
		if (newState == RUNNING) {
			startTime = simgrid::s4u::Engine::get_clock();
			waitTime = startTime - submitTime;
			executingNodes = assignedNodes;
			if (type == RIGID) {
				executingNumGpusPerNode = numGpusPerNode;
				} else {
					size_t numNodes = executingNodes.size();
					executingNumGpusPerNode = assignedNumGpusPerNode;
					workload->scaleTo(numNodes, executingNumGpusPerNode, runtimeArguments);
					workload->scaleInitPhaseTo(numNodes, executingNumGpusPerNode, runtimeArguments);
				}
				JobUtils::synchronizePredictionStateForCurrentAllocation(this, startTime);
			}
		} else if (state == PENDING_RECONFIGURATION) {
			if (newState == IN_RECONFIGURATION) {
				executingNodes = assignedNodes;
				for (const auto& node: assignedNodes) {
				node->removeExpectedJob(this);
			}
				executingNumGpusPerNode = assignedNumGpusPerNode;
				size_t numNodes = executingNodes.size();
				workload->scaleTo(numNodes, executingNumGpusPerNode, runtimeArguments);
				workload->scaleReconfigurationPhaseTo(numNodes, executingNumGpusPerNode, runtimeArguments);
				JobUtils::synchronizePredictionStateForCurrentAllocation(this, simgrid::s4u::Engine::get_clock());
			}
		}
		if (newState == COMPLETED || newState == KILLED) {
			endTime = simgrid::s4u::Engine::get_clock();
			makespan = endTime - startTime;
		turnaroundTime = endTime - submitTime;
			for (const auto& node: assignedNodes) {
				node->removeExpectedJob(this);
			}
			clearPredictionState();
		}
		state = newState;
		PlatformManager::addModifiedJob(this);
}

double Job::getWalltime() const {
	return walltime;
}

double Job::getSubmitTime() const {
	return submitTime;
}

double Job::getStartTime() const {
	return startTime;
}


double Job::getEndTime() const {
	return endTime;
}

double Job::getWaitTime() const {
	return waitTime;
}

double Job::getMakespan() const {
	return makespan;
}

double Job::getTurnaroundTime() const {
	return turnaroundTime;
}

const Workload* Job::getWorkload() const {
	return workload.get();
}

const std::vector<Node*>& Job::getExecutingNodes() const {
	return executingNodes;
}

const std::vector<Node*>& Job::getExpandingNodes() const {
	return expandingNodes;
}

void Job::setExpandNodes(const std::vector<Node*> expandingNodes) {
	PerformanceTracer::ScopedComponent component("job_set_expand_nodes");
	Job::expandingNodes = expandingNodes;
	workload->scaleExpandPhaseTo(expandingNodes.size(), executingNumGpusPerNode, runtimeArguments);
}

int Job::calculateEvolvingRequest(const std::string& evolvingModel, int phaseIteration) {
	additionalArguments["phase_iteration"] = std::to_string(phaseIteration);
	int numberOfNodes = (int) Utility::evaluateFormula(evolvingModel, getNumberOfExecutingNodes(),
													   executingNumGpusPerNode, runtimeArguments, additionalArguments);
	if (clipEvolvingRequests) {
		numberOfNodes = std::max(std::min(numberOfNodes, numNodesMax), numNodesMin);
	} else {
		if (numberOfNodes < numNodesMin) {
			xbt_die("Evolving requests can not be smaller than the minimum number of requested nodes "
					"(request model ⌊%s⌋ results in %d, minimum number of nodes is %d)",
					evolvingModel.c_str(), numberOfNodes, numNodesMin);
		}
		if (numberOfNodes > numNodesMax) {
			xbt_die("Evolving requests can not be greater than the maximum number of requested nodes "
					"(request model ⌊%s⌋ results in %d, maximum number of nodes is %d)",
					evolvingModel.c_str(), numberOfNodes, numNodesMax);
		}
	}

	return numberOfNodes;
}

void Job::assignNode(Node* node) {
	if (state == PENDING) {
		assignedNodes.push_back(node);
	} else if (type == MALLEABLE || type == EVOLVING || type == ADAPTIVE) {
		assignedNodes.push_back(node);
		node->expectJob(this);
	} else {
		xbt_die("Assigning nodes during runtime not allowed for rigid/moldable job %d", id);
	}
}

void Job::assignNumGpusPerNode(int numGpusPerNode) {
	Job::assignedNumGpusPerNode = numGpusPerNode;
}

int Job::getNumberOfExecutingNodes() const {
	return executingNodes.size();
}

int Job::getExecutingNumGpusPerNode() const {
	return executingNumGpusPerNode;
}

void Job::advanceWorkload(int completedPhases, int remainingIterations) {
	workload->advance(completedPhases, remainingIterations);
}

void Job::completeWorkload() {
	workload->complete();
}

void Job::setSchedulingPointDecision(SchedulingPointDecision decision) {
	schedulingPointDecision = decision;
}

void Job::clearSchedulingPointDecision() {
	schedulingPointDecision = SCHEDULING_POINT_DECISION_UNSET;
}

SchedulingPointDecision Job::getSchedulingPointDecision() const {
	return schedulingPointDecision;
}

void Job::updateState() {
	PerformanceTracer::ScopedComponent component("job_update_state");
	if (assignedNodes != executingNodes) {
		if (state == PENDING) {
			state = PENDING_ALLOCATION;
			PlatformManager::addModifiedJob(this);
		} else if (state == RUNNING) {
			state = PENDING_RECONFIGURATION;
			PlatformManager::addModifiedJob(this);
		}
	} else {
		if (state == PENDING_RECONFIGURATION) {
			state = RUNNING;
			PlatformManager::addModifiedJob(this);
		}
	}
}

void Job::clearAssignedNodes() {
	PerformanceTracer::ScopedComponent component("job_clear_assigned_nodes");
	for (const auto& node: assignedNodes) {
		node->removeExpectedJob(this);
	}
	assignedNodes.clear();
}

// ============================================================================
// MALLEABILITY SUPPORT: New methods for shrinking/expanding jobs
// ============================================================================

void Job::removeNodes(const std::vector<Node*>& nodesToRemove) {
	PerformanceTracer::ScopedComponent component("job_remove_nodes");
	// 1. Validation
	if (nodesToRemove.empty()) {
		return;
	}
	
	// 2. Check that we don't fall below min_nodes
	int remainingNodes = assignedNodes.size() - nodesToRemove.size();
	if (remainingNodes < numNodesMin) {
		xbt_die("Cannot shrink job %d below num_nodes_min (%d). Current: %zu, trying to remove: %zu",
				id, numNodesMin, assignedNodes.size(), nodesToRemove.size());
	}
	
	// 3. Remove nodes from assignedNodes
	for (Node* nodeToRemove : nodesToRemove) {
		auto it = std::find(assignedNodes.begin(), assignedNodes.end(), nodeToRemove);
		if (it != assignedNodes.end()) {
			assignedNodes.erase(it);
			// Remove expected job from node
			nodeToRemove->removeExpectedJob(this);
		}
	}
	
	// 4. Trigger reconfiguration if job is running
	if (state == RUNNING) {
		// Update state to trigger reconfiguration
		setState(PENDING_RECONFIGURATION);
		// The scheduler handles the PENDING_RECONFIGURATION state on its next invocation.
	}
	
	// 5. Notify PlatformManager about modification
	PlatformManager::addModifiedJob(this);
}

const std::vector<Node*>& Job::getAssignedNodes() const {
	return assignedNodes;
}

int Job::getNumNodesMin() const {
	if (type == RIGID) {
		return numNodes;
	}
	return numNodesMin;
}

int Job::getNumNodesMax() const {
	if (type == RIGID) {
		return numNodes;
	}
	return numNodesMax;
}

int Job::getNumNodesPref() const {
	// Preferred node count: between min and max
	// Default: middle of range
	if (type == RIGID) {
		return numNodes;
	}
	
	// Check if explicitly set in arguments
	if (arguments.count("num_nodes_pref")) {
		return std::stoi(arguments.at("num_nodes_pref"));
	}
	
	// Default: average of min and max
	return (numNodesMin + numNodesMax) / 2;
}

int Job::getNumGpusPerNodeMin() const {
	if (type == RIGID) {
		return numGpusPerNode;
	}
	return numGpusPerNodeMin;
}

int Job::getNumGpusPerNodeMax() const {
	if (type == RIGID) {
		return numGpusPerNode;
	}
	return numGpusPerNodeMax;
}

const std::map<std::string, std::string>& Job::getArguments() const {
	return arguments;
}

const std::map<std::string, std::string>& Job::getAttributes() const {
	return attributes;
}

const std::string* Job::findArgument(const std::string& key) const {
	auto it = arguments.find(key);
	if (it == arguments.end()) {
		return nullptr;
	}
	return &it->second;
}

const std::string* Job::findAttribute(const std::string& key) const {
	auto it = attributes.find(key);
	if (it == attributes.end()) {
		return nullptr;
	}
	return &it->second;
}

double Job::getEstimatedRuntime() const {
	// Check if explicitly set in arguments
	auto runtimeIt = arguments.find("runtime");
	if (runtimeIt != arguments.end()) {
		return std::stod(runtimeIt->second);
	}

	// Match Python: derive runtime from flops * iterations / num_nodes_min
	auto flopsIt = arguments.find("flops");
	if (flopsIt != arguments.end()) {
		double flops = std::stod(flopsIt->second);
		double iterations = 1.0;
		auto iterIt = arguments.find("iterations");
		if (iterIt != arguments.end()) {
			iterations = std::stod(iterIt->second);
		}
		int minNodes = getNumNodesMin();
		if (minNodes > 0) {
			return (flops * iterations) / static_cast<double>(minNodes);
		}
	}
	
	// Fallback to walltime
	return walltime;
}

// ============================================================================

void Job::updateRuntimeArguments(const std::string& key, const std::string& value) {
	runtimeArgumentsMutex->lock();
	runtimeArguments[key] = value;
	runtimeArgumentsMutex->unlock();
}

void Job::clearRuntimeArguments() {
	runtimeArgumentsMutex->lock();
	runtimeArguments.clear();
	runtimeArgumentsMutex->unlock();
}

void Job::checkSpecification() const {
	if (type != RIGID) {
		if (numNodesMin < 1) {
			xbt_die("Invalid specification for non-rigid job: number of minimum nodes cannot be less than 1");
		}
		if (numNodesMax < 1) {
			xbt_die("Invalid specification for non-rigid job: number of maximum nodes cannot be less than 1");
		}
		if (numNodesMin > numNodesMax) {
			xbt_die("Invalid specification for non-rigid job: minimum number of nodes (%d) is greater than the maximum number of nodes (%d).",
					numNodesMin, numNodesMax);
		}
		if (numGpusPerNodeMin > numGpusPerNodeMax) {
			xbt_die("Invalid specification for non-rigid job: minimum number of GPUs per node (%d) is greater than the maximum number of GPUs per node (%d).",
					numGpusPerNodeMin, numGpusPerNodeMax);
		}
	} else {
		if (numNodes < 1) {
			xbt_die("Invalid specification for rigid job: number of nodes cannot be less than 1");
		}
	}
}

void Job::checkConfigurationValidity() const {
	size_t numAssignedNodes = assignedNodes.size();
	if (type != RIGID) {
		if (numAssignedNodes < numNodesMin || numAssignedNodes > numNodesMax) {
			xbt_die("Invalid configuration for job %d: Number of assigned nodes is expected to be [%d-%d] but is %zu",
					id, numNodesMin, numNodesMax, numAssignedNodes);
		}
		if (assignedNumGpusPerNode < numGpusPerNodeMin || assignedNumGpusPerNode > numGpusPerNodeMax) {
			xbt_die("Invalid configuration for job %d: Number of assigned GPUs per node is expected to be [%d-%d] but is %u",
					id, numGpusPerNodeMin, numGpusPerNodeMax, assignedNumGpusPerNode);
		}
	} else {
		if (numAssignedNodes != numNodes) {
			xbt_die("Invalid configuration for job %d: Number of assigned nodes is expected to be %d but is %zu",
					id, numNodes, numAssignedNodes);
		}
	}
}

bool Job::isPredictionStateInitialized() const {
	return predictionStateInitialized;
}

int Job::getPredictionReferenceNodes() const {
	return predictionReferenceNodes;
}

double Job::getPredictionReferenceRuntime() const {
	return predictionReferenceRuntime;
}

double Job::getPredictedRemainingWorkRef() const {
	return predictedRemainingWorkRef;
}

double Job::getLastPredictionUpdateTime() const {
	return lastPredictionUpdateTime;
}

int Job::getLastPredictionNodes() const {
	return lastPredictionNodes;
}

void Job::initializePredictionState(int referenceNodes, double referenceRuntime, double now, int currentNodes) {
	predictionStateInitialized = true;
	predictionReferenceNodes = referenceNodes;
	predictionReferenceRuntime = referenceRuntime;
	predictedRemainingWorkRef = referenceRuntime;
	lastPredictionUpdateTime = now;
	lastPredictionNodes = currentNodes;
}

void Job::updatePredictionState(double remainingWorkRef, double now, int currentNodes) {
	predictionStateInitialized = true;
	predictedRemainingWorkRef = remainingWorkRef;
	lastPredictionUpdateTime = now;
	lastPredictionNodes = currentNodes;
}

void Job::clearPredictionState() {
	predictionStateInitialized = false;
	predictionReferenceNodes = 0;
	predictionReferenceRuntime = 0.0;
	predictedRemainingWorkRef = 0.0;
	lastPredictionUpdateTime = 0.0;
	lastPredictionNodes = 0;
}

nlohmann::json Job::toJson() const {
	PerformanceTracer::recordJobToJsonCall();
	nlohmann::json json;
	json["id"] = id;
	json["state"] = state;
	json["type"] = type;
	json["walltime"] = walltime;
	if (type != RIGID) {
		json["num_nodes_min"] = numNodesMin;
		json["num_nodes_max"] = numNodesMax;
		json["num_gpus_per_node_min"] = numGpusPerNodeMin;
		json["num_gpus_per_node_max"] = numGpusPerNodeMax;
	} else {
		json["num_nodes"] = numNodes;
		json["num_gpus_per_node"] = numGpusPerNode;
	}
	json["submit_time"] = submitTime;
	json["start_time"] = startTime;
	json["end_time"] = endTime;
	json["wait_time"] = waitTime;
	json["makespan"] = makespan;
	json["turnaround_time"] = turnaroundTime;
	json["assigned_nodes"] = nlohmann::json::array();
	for (const auto& node: assignedNodes) {
		json["assigned_nodes"].push_back(node->getId());
	}
	json["assigned_num_gpus_per_node"] = assignedNumGpusPerNode;
	for (const auto& [key, value]: arguments) {
		json["arguments"][key] = value;
	}
	for (const auto& [key, value]: attributes) {
		json["attributes"][key] = value;
	}
	for (const auto& [key, value]: runtimeArguments) {
		json["runtime_arguments"][key] = value;
	}
	json["total_phase_count"] = workload->getTotalPhaseCount();
	json["completed_phases"] = workload->getCompletedPhases();
	return json;
}
