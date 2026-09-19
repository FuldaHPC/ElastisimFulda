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

#ifndef ELASTISIM_PERFORMANCETRACER_H
#define ELASTISIM_PERFORMANCETRACER_H

#include <chrono>
#include <cstdint>
#include <cstddef>
#include <fstream>
#include <map>
#include <string>

class PerformanceTracer {
public:
	struct Counters {
		std::uint64_t nodeCacheHits = 0;
		std::uint64_t nodeCacheMisses = 0;
		std::uint64_t nodeCacheRebuilds = 0;
		std::uint64_t nodeCacheRebuildNodesScanned = 0;
		std::uint64_t shutdownPolicyAcceptChecks = 0;
		std::uint64_t shutdownFilterCalls = 0;
		std::uint64_t shutdownFilterNodesIn = 0;
		std::uint64_t shutdownFilterNodesOut = 0;
		std::uint64_t shutdownUpdateNodesScanned = 0;
		std::uint64_t jobToJsonCalls = 0;
		std::uint64_t nodeToJsonCalls = 0;
		std::uint64_t jobutilsRuntimeCalls = 0;
		std::uint64_t eventLogWrites = 0;
		std::uint64_t nodeUtilizationRows = 0;
		std::uint64_t jobsShrunk = 0;
		std::uint64_t nodesShrunk = 0;
		std::uint64_t jobsExpanded = 0;
		std::uint64_t nodesExpanded = 0;
		std::uint64_t agreementsAdded = 0;
		std::uint64_t agreementsFulfilled = 0;
	};

	class ScopedPhase {
	public:
		explicit ScopedPhase(std::string phaseName);
		~ScopedPhase();

		ScopedPhase(const ScopedPhase&) = delete;
		ScopedPhase& operator=(const ScopedPhase&) = delete;

	private:
		bool active;
		std::string phaseName;
		std::chrono::steady_clock::time_point start;
	};

	class ScopedComponent {
	public:
		explicit ScopedComponent(std::string componentName);
		~ScopedComponent();

		ScopedComponent(const ScopedComponent&) = delete;
		ScopedComponent& operator=(const ScopedComponent&) = delete;

	private:
		bool active;
		std::string componentName;
		std::chrono::steady_clock::time_point start;
	};

	static void initialize();
	static void shutdown();
	static bool isEnabled();

	static void beginInvocation(const std::string& schedulerName,
								int invocationType,
								double simTime,
								std::size_t queueSize,
								std::size_t pendingJobs,
								std::size_t runningJobs,
								std::size_t runningMalleableJobs,
								int requestingJobId,
								int requestedNodes);
	static void setFreeNodesCached(std::size_t freeNodes);
	static void endInvocation(std::size_t scheduledJobs);
	static void addPhaseNs(const std::string& phaseName, std::int64_t nanoseconds);
	static void addComponentNs(const std::string& componentName, std::int64_t nanoseconds);
	static void recordComponentEvent(const std::string& componentName, std::uint64_t count = 1);
	static void setObservedSimTime(double simTime);

	static void recordNodeCacheHit();
	static void recordNodeCacheMiss();
	static void recordNodeCacheRebuild(std::size_t nodesScanned);
	static void recordShutdownPolicyAcceptCheck();
	static void recordShutdownFilter(std::size_t nodesIn, std::size_t nodesOut);
	static void recordShutdownUpdateNodesScanned(std::size_t nodesScanned);
	static void recordJobToJsonCall();
	static void recordNodeToJsonCall();
	static void recordJobUtilsRuntimeCall();
	static void recordEventLogWrite();
	static void recordNodeUtilizationRow();
	static void recordShrink(std::size_t jobs, std::size_t nodes);
	static void recordExpand(std::size_t jobs, std::size_t nodes);
	static void recordAgreementAdded();
	static void recordAgreementFulfilled();

private:
	struct ComponentAggregate {
		std::uint64_t count = 0;
		std::int64_t nanoseconds = 0;
	};

	static void writeHeader();
	static void writeComponentHeader();
	static void maybeWriteComponentSnapshot(bool force = false);
	static double nsToMs(std::int64_t nanoseconds);
	static std::uint64_t delta(std::uint64_t current, std::uint64_t previous);

	static bool initialized;
	static bool enabled;
	static std::ofstream output;
	static std::ofstream componentOutput;
	static std::size_t flushInterval;
	static std::size_t rowsSinceFlush;
	static double componentFlushIntervalSeconds;
	static std::chrono::steady_clock::time_point componentTraceStart;
	static std::chrono::steady_clock::time_point lastComponentFlush;
	static double lastObservedSimTime;

	static bool invocationActive;
	static std::chrono::steady_clock::time_point invocationStart;
	static std::string currentSchedulerName;
	static int currentInvocationType;
	static double currentSimTime;
	static std::size_t currentQueueSize;
	static std::size_t currentPendingJobs;
	static std::size_t currentRunningJobs;
	static std::size_t currentRunningMalleableJobs;
	static long long currentFreeNodesCached;
	static int currentRequestingJobId;
	static int currentRequestedNodes;
	static Counters invocationStartCounters;
	static std::map<std::string, std::int64_t> currentPhaseNs;
	static Counters counters;
	static std::map<std::string, ComponentAggregate> componentCounters;
	static std::map<std::string, ComponentAggregate> lastComponentSnapshot;
};

#endif // ELASTISIM_PERFORMANCETRACER_H
