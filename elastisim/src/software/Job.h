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

#ifndef ELASTISIM_JOB_H
#define ELASTISIM_JOB_H

#include <memory>
#include <simgrid/s4u.hpp>
#include <list>
#include <json.hpp>

class Node;

class Workload;

enum JobType {
	RIGID = 0,
	MOLDABLE = 1,
	MALLEABLE = 2,
	EVOLVING = 3,
	ADAPTIVE = 4
};

enum JobState {
	PENDING_SUBMISSION = 0,
	PENDING = 1,
	PENDING_ALLOCATION = 2,
	PENDING_KILL = 3,
	RUNNING = 4,
	PENDING_RECONFIGURATION = 5,
	IN_RECONFIGURATION = 6,
	COMPLETED = 7,
	KILLED = 8
};

enum SchedulingPointDecision {
	SCHEDULING_POINT_DECISION_UNSET = 0,
	SCHEDULING_POINT_USE_SCHEDULER = 1,
	SCHEDULING_POINT_FAST_CONTINUE = 2
};

class Job {

private:
	int id;
	const JobType type;
	JobState state;
	const double walltime;
	const int numNodes;
	const int numGpusPerNode;
	const int numNodesMin;
	const int numNodesMax;
	const int numGpusPerNodeMin;
	const int numGpusPerNodeMax;
	const double submitTime;
	double startTime;
	double endTime;
	double waitTime;
	double makespan;
	double turnaroundTime;
	std::unique_ptr<Workload> workload;
	std::vector<Node*> assignedNodes;
	std::vector<Node*> executingNodes;
	std::vector<Node*> expandingNodes;
	std::map<std::string, std::string> arguments;
	std::map<std::string, std::string> attributes;
		std::map<std::string, std::string> runtimeArguments;
		std::map<std::string, std::string> additionalArguments;
		simgrid::s4u::MutexPtr runtimeArgumentsMutex;
		int assignedNumGpusPerNode;
		int executingNumGpusPerNode;
		const bool clipEvolvingRequests;
		SchedulingPointDecision schedulingPointDecision;
		bool predictionStateInitialized;
		int predictionReferenceNodes;
		double predictionReferenceRuntime;
		double predictedRemainingWorkRef;
		double lastPredictionUpdateTime;
		int lastPredictionNodes;

	public:
	Job(int walltime, int numNodes, int numGpusPerNode, double submitTime,
		std::map<std::string, std::string> arguments, std::map<std::string, std::string> attributes,
		std::unique_ptr<Workload> workload);

	Job(int walltime, JobType type, int numNodesMin, int numNodesMax, int numGpusPerNodeMin, int numGpusPerNodeMax,
		double submitTime, std::map<std::string, std::string> arguments, std::map<std::string, std::string> attributes,
		std::unique_ptr<Workload> workload);

	[[nodiscard]] int getId() const;

	void setId(int id);

	[[nodiscard]] JobState getState() const;

	[[nodiscard]] JobType getType() const;

	void setState(JobState newState);

	[[nodiscard]] double getWalltime() const;

	[[nodiscard]] double getSubmitTime() const;

	[[nodiscard]] double getStartTime() const;

	[[nodiscard]] double getEndTime() const;

	[[nodiscard]] double getWaitTime() const;

	[[nodiscard]] double getMakespan() const;

	[[nodiscard]] double getTurnaroundTime() const;

	[[nodiscard]] const Workload* getWorkload() const;

	[[nodiscard]] const std::vector<Node*>& getExecutingNodes() const;

	[[nodiscard]] const std::vector<Node*>& getExpandingNodes() const;

	// Malleability: Get assigned nodes (not yet executing)
	[[nodiscard]] const std::vector<Node*>& getAssignedNodes() const;

	// Malleability: Get node count properties
	[[nodiscard]] int getNumNodesMin() const;
	[[nodiscard]] int getNumNodesMax() const;
	[[nodiscard]] int getNumNodesPref() const;
		[[nodiscard]] int getNumGpusPerNodeMin() const;
		[[nodiscard]] int getNumGpusPerNodeMax() const;
		[[nodiscard]] const std::map<std::string, std::string>& getArguments() const;
		[[nodiscard]] const std::map<std::string, std::string>& getAttributes() const;
		[[nodiscard]] const std::string* findArgument(const std::string& key) const;
		[[nodiscard]] const std::string* findAttribute(const std::string& key) const;

		// Malleability: Get estimated runtime for scheduling decisions
		[[nodiscard]] double getEstimatedRuntime() const;

	void setExpandNodes(std::vector<Node*> expandingNodes);

	[[nodiscard]] int calculateEvolvingRequest(const std::string& evolvingModel, int phaseIteration);

	void assignNode(Node* node);

	// Malleability: Remove nodes from job (shrinking)
	void removeNodes(const std::vector<Node*>& nodesToRemove);

	void assignNumGpusPerNode(int numGpusPerNode);

	[[nodiscard]] int getNumberOfExecutingNodes() const;

	[[nodiscard]] int getExecutingNumGpusPerNode() const;

	void advanceWorkload(int completedPhases, int remainingIterations);

	void completeWorkload();

	void setSchedulingPointDecision(SchedulingPointDecision decision);

	void clearSchedulingPointDecision();

	[[nodiscard]] SchedulingPointDecision getSchedulingPointDecision() const;

	void updateState();

	void clearAssignedNodes();

	void updateRuntimeArguments(const std::string& key, const std::string& value);

	void clearRuntimeArguments();

		void checkSpecification() const;

		void checkConfigurationValidity() const;

		[[nodiscard]] bool isPredictionStateInitialized() const;

		[[nodiscard]] int getPredictionReferenceNodes() const;

		[[nodiscard]] double getPredictionReferenceRuntime() const;

		[[nodiscard]] double getPredictedRemainingWorkRef() const;

		[[nodiscard]] double getLastPredictionUpdateTime() const;

		[[nodiscard]] int getLastPredictionNodes() const;

		void initializePredictionState(int referenceNodes, double referenceRuntime, double now, int currentNodes);

		void updatePredictionState(double remainingWorkRef, double now, int currentNodes);

		void clearPredictionState();

		[[nodiscard]] nlohmann::json toJson() const;

};


#endif //ELASTISIM_JOB_H
