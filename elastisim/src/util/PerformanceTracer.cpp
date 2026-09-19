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

#include "PerformanceTracer.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "Configuration.h"

bool PerformanceTracer::initialized = false;
bool PerformanceTracer::enabled = false;
std::ofstream PerformanceTracer::output;
std::ofstream PerformanceTracer::componentOutput;
std::size_t PerformanceTracer::flushInterval = 1;
std::size_t PerformanceTracer::rowsSinceFlush = 0;
double PerformanceTracer::componentFlushIntervalSeconds = 5.0;
std::chrono::steady_clock::time_point PerformanceTracer::componentTraceStart;
std::chrono::steady_clock::time_point PerformanceTracer::lastComponentFlush;
double PerformanceTracer::lastObservedSimTime = -1.0;

bool PerformanceTracer::invocationActive = false;
std::chrono::steady_clock::time_point PerformanceTracer::invocationStart;
std::string PerformanceTracer::currentSchedulerName;
int PerformanceTracer::currentInvocationType = -1;
double PerformanceTracer::currentSimTime = 0.0;
std::size_t PerformanceTracer::currentQueueSize = 0;
std::size_t PerformanceTracer::currentPendingJobs = 0;
std::size_t PerformanceTracer::currentRunningJobs = 0;
std::size_t PerformanceTracer::currentRunningMalleableJobs = 0;
long long PerformanceTracer::currentFreeNodesCached = -1;
int PerformanceTracer::currentRequestingJobId = -1;
int PerformanceTracer::currentRequestedNodes = -1;
PerformanceTracer::Counters PerformanceTracer::invocationStartCounters;
std::map<std::string, std::int64_t> PerformanceTracer::currentPhaseNs;
PerformanceTracer::Counters PerformanceTracer::counters;
std::map<std::string, PerformanceTracer::ComponentAggregate> PerformanceTracer::componentCounters;
std::map<std::string, PerformanceTracer::ComponentAggregate> PerformanceTracer::lastComponentSnapshot;

namespace {
const std::vector<std::string>& phaseColumns() {
	static const std::vector<std::string> columns{
			"shutdown_update",
			"shutdown_malleable_policy",
			"node_transition_check",
			"native_scheduler",
			"forward_allocation",
			"forward_reconfiguration",
			"forward_kill",
			"node_cache_rebuild",
			"shutdown_filter",
			"jobqueue_pending",
			"jobqueue_running",
			"jobqueue_running_malleable",
			"sort_running",
			"sort_malleable",
			"agreement_resolve",
			"agreement_filter",
			"head_delay_check",
			"initial_allocation",
			"shrink_selection",
			"shrink_apply",
			"expand_selection",
			"expand_apply",
			"event_log_write"
	};
	return columns;
}

const std::vector<std::string>& componentColumns() {
	static const std::vector<std::string> columns{
			"scheduler_schedule",
			"scheduler_handle_message",
			"scheduler_handle_processed_workload",
			"scheduler_handle_reconfiguration",
			"scheduler_forward_allocation",
			"scheduler_forward_kill",
			"sensing_setup",
			"sensing_iteration",
			"simulation_engine_handle_message",
			"simulation_engine_finalize_stats",
			"job_submitter_read_jobs",
			"job_submitter_sort_jobs",
			"job_submitter_submit_message",
			"workload_scale",
			"workload_scale_init",
			"workload_scale_reconfiguration",
			"workload_scale_expand",
			"workload_advance",
			"workload_complete",
			"phase_scale",
			"task_scale",
			"job_set_state",
			"job_set_expand_nodes",
			"job_update_state",
			"job_remove_nodes",
			"job_clear_assigned_nodes",
			"node_collect_statistics",
			"node_allocate_job",
			"node_continue_job",
			"node_reconfigure_job",
			"node_expand_job",
			"node_complete_job",
			"node_kill_job",
			"node_set_state",
			"node_expect_job",
			"node_remove_expected_job",
			"event_log_write",
			"application_actor_start",
			"application_init_phase",
			"application_reconfiguration_phase",
			"application_expansion_phase",
			"application_scheduling_point",
			"application_scheduling_point_fast_continue",
			"application_scheduling_point_fast_continue_nodes",
			"application_scheduling_point_fast_scheduler_fallback",
			"application_scheduling_point_fast_blocked_config_disabled",
			"application_scheduling_point_fast_blocked_schedule_on_sp",
			"application_scheduling_point_fast_blocked_job_type",
			"application_scheduling_point_fast_blocked_walltime",
			"application_scheduling_point_fast_blocked_periodic_tick",
			"application_scheduling_point_fast_blocked_state",
			"application_evolving_request",
			"application_workload_processed",
			"application_task_iterations",
			"application_barrier_phase",
			"application_async_wait"
	};
	return columns;
}
}

PerformanceTracer::ScopedPhase::ScopedPhase(std::string phaseName) :
		active(PerformanceTracer::isEnabled()),
		phaseName(std::move(phaseName)),
		start(std::chrono::steady_clock::now()) {}

PerformanceTracer::ScopedPhase::~ScopedPhase() {
	if (!active) {
		return;
	}
	const auto end = std::chrono::steady_clock::now();
	const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
	PerformanceTracer::addPhaseNs(phaseName, ns);
}

PerformanceTracer::ScopedComponent::ScopedComponent(std::string componentName) :
		active(PerformanceTracer::isEnabled()),
		componentName(std::move(componentName)),
		start(std::chrono::steady_clock::now()) {}

PerformanceTracer::ScopedComponent::~ScopedComponent() {
	if (!active) {
		return;
	}
	const auto end = std::chrono::steady_clock::now();
	const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
	PerformanceTracer::addComponentNs(componentName, ns);
}

void PerformanceTracer::initialize() {
	if (initialized) {
		return;
	}
	initialized = true;
	enabled = Configuration::getBoolIfExists("performance_trace");
	if (!enabled) {
		return;
	}

	std::string path = "performance_trace.csv";
	if (Configuration::exists("performance_trace_file")) {
		path = std::string(Configuration::get("performance_trace_file"));
	}
	if (Configuration::exists("performance_trace_flush_interval")) {
		flushInterval = std::max<std::size_t>(
				1, Configuration::get("performance_trace_flush_interval").get<std::size_t>());
	}
	if (Configuration::exists("performance_component_flush_interval_seconds")) {
		componentFlushIntervalSeconds = std::max<double>(
				1.0, Configuration::get("performance_component_flush_interval_seconds").get<double>());
	}

	output.open(path);
	if (!output.is_open()) {
		enabled = false;
		return;
	}
	writeHeader();
	output.flush();

	std::string componentPath = "performance_component_trace.csv";
	if (Configuration::exists("performance_component_trace_file")) {
		componentPath = std::string(Configuration::get("performance_component_trace_file"));
	}
	componentOutput.open(componentPath);
	if (componentOutput.is_open()) {
		componentTraceStart = std::chrono::steady_clock::now();
		lastComponentFlush = componentTraceStart;
		writeComponentHeader();
		componentOutput.flush();
	}
}

void PerformanceTracer::shutdown() {
	maybeWriteComponentSnapshot(true);
	if (componentOutput.is_open()) {
		componentOutput.flush();
		componentOutput.close();
	}
	if (output.is_open()) {
		output.flush();
		output.close();
	}
}

bool PerformanceTracer::isEnabled() {
	return enabled;
}

void PerformanceTracer::beginInvocation(const std::string& schedulerName,
										int invocationType,
										double simTime,
										std::size_t queueSize,
										std::size_t pendingJobs,
										std::size_t runningJobs,
										std::size_t runningMalleableJobs,
										int requestingJobId,
										int requestedNodes) {
	if (!enabled) {
		return;
	}
	invocationActive = true;
	invocationStart = std::chrono::steady_clock::now();
	currentSchedulerName = schedulerName;
	currentInvocationType = invocationType;
	currentSimTime = simTime;
	currentQueueSize = queueSize;
	currentPendingJobs = pendingJobs;
	currentRunningJobs = runningJobs;
	currentRunningMalleableJobs = runningMalleableJobs;
	currentFreeNodesCached = -1;
	currentRequestingJobId = requestingJobId;
	currentRequestedNodes = requestedNodes;
	lastObservedSimTime = simTime;
	invocationStartCounters = counters;
	currentPhaseNs.clear();
}

void PerformanceTracer::setFreeNodesCached(std::size_t freeNodes) {
	if (enabled && invocationActive) {
		currentFreeNodesCached = static_cast<long long>(freeNodes);
	}
}

void PerformanceTracer::endInvocation(std::size_t scheduledJobs) {
	if (!enabled || !invocationActive || !output.is_open()) {
		return;
	}
	const auto end = std::chrono::steady_clock::now();
	const auto totalNs = std::chrono::duration_cast<std::chrono::nanoseconds>(end - invocationStart).count();

	output << currentSimTime << ","
		   << currentSchedulerName << ","
		   << currentInvocationType << ","
		   << nsToMs(totalNs) << ","
		   << currentQueueSize << ","
		   << currentPendingJobs << ","
		   << currentRunningJobs << ","
		   << currentRunningMalleableJobs << ","
		   << currentFreeNodesCached << ","
		   << scheduledJobs << ","
		   << currentRequestingJobId << ","
		   << currentRequestedNodes;

	for (const auto& phase : phaseColumns()) {
		auto it = currentPhaseNs.find(phase);
		output << "," << nsToMs(it == currentPhaseNs.end() ? 0 : it->second);
	}

	output << ","
		   << delta(counters.nodeCacheHits, invocationStartCounters.nodeCacheHits) << ","
		   << delta(counters.nodeCacheMisses, invocationStartCounters.nodeCacheMisses) << ","
		   << delta(counters.nodeCacheRebuilds, invocationStartCounters.nodeCacheRebuilds) << ","
		   << delta(counters.nodeCacheRebuildNodesScanned, invocationStartCounters.nodeCacheRebuildNodesScanned) << ","
		   << delta(counters.shutdownPolicyAcceptChecks, invocationStartCounters.shutdownPolicyAcceptChecks) << ","
		   << delta(counters.shutdownFilterCalls, invocationStartCounters.shutdownFilterCalls) << ","
		   << delta(counters.shutdownFilterNodesIn, invocationStartCounters.shutdownFilterNodesIn) << ","
		   << delta(counters.shutdownFilterNodesOut, invocationStartCounters.shutdownFilterNodesOut) << ","
		   << delta(counters.shutdownUpdateNodesScanned, invocationStartCounters.shutdownUpdateNodesScanned) << ","
		   << delta(counters.jobToJsonCalls, invocationStartCounters.jobToJsonCalls) << ","
		   << delta(counters.nodeToJsonCalls, invocationStartCounters.nodeToJsonCalls) << ","
		   << delta(counters.jobutilsRuntimeCalls, invocationStartCounters.jobutilsRuntimeCalls) << ","
		   << delta(counters.eventLogWrites, invocationStartCounters.eventLogWrites) << ","
		   << delta(counters.nodeUtilizationRows, invocationStartCounters.nodeUtilizationRows) << ","
		   << delta(counters.jobsShrunk, invocationStartCounters.jobsShrunk) << ","
		   << delta(counters.nodesShrunk, invocationStartCounters.nodesShrunk) << ","
		   << delta(counters.jobsExpanded, invocationStartCounters.jobsExpanded) << ","
		   << delta(counters.nodesExpanded, invocationStartCounters.nodesExpanded) << ","
		   << delta(counters.agreementsAdded, invocationStartCounters.agreementsAdded) << ","
		   << delta(counters.agreementsFulfilled, invocationStartCounters.agreementsFulfilled) << "\n";

	rowsSinceFlush += 1;
	if (rowsSinceFlush >= flushInterval) {
		output.flush();
		rowsSinceFlush = 0;
	}
	invocationActive = false;
}

void PerformanceTracer::addPhaseNs(const std::string& phaseName, std::int64_t nanoseconds) {
	if (enabled && invocationActive) {
		currentPhaseNs[phaseName] += nanoseconds;
	}
}

void PerformanceTracer::addComponentNs(const std::string& componentName, std::int64_t nanoseconds) {
	if (!enabled) {
		return;
	}
	auto& aggregate = componentCounters[componentName];
	aggregate.count += 1;
	aggregate.nanoseconds += nanoseconds;
	maybeWriteComponentSnapshot();
}

void PerformanceTracer::recordComponentEvent(const std::string& componentName, std::uint64_t count) {
	if (!enabled || count == 0) {
		return;
	}
	componentCounters[componentName].count += count;
	maybeWriteComponentSnapshot();
}

void PerformanceTracer::setObservedSimTime(double simTime) {
	if (enabled) {
		lastObservedSimTime = simTime;
	}
}

void PerformanceTracer::recordNodeCacheHit() {
	if (enabled) {
		counters.nodeCacheHits += 1;
	}
}

void PerformanceTracer::recordNodeCacheMiss() {
	if (enabled) {
		counters.nodeCacheMisses += 1;
	}
}

void PerformanceTracer::recordNodeCacheRebuild(std::size_t nodesScanned) {
	if (enabled) {
		counters.nodeCacheRebuilds += 1;
		counters.nodeCacheRebuildNodesScanned += nodesScanned;
	}
}

void PerformanceTracer::recordShutdownPolicyAcceptCheck() {
	if (enabled) {
		counters.shutdownPolicyAcceptChecks += 1;
	}
}

void PerformanceTracer::recordShutdownFilter(std::size_t nodesIn, std::size_t nodesOut) {
	if (enabled) {
		counters.shutdownFilterCalls += 1;
		counters.shutdownFilterNodesIn += nodesIn;
		counters.shutdownFilterNodesOut += nodesOut;
	}
}

void PerformanceTracer::recordShutdownUpdateNodesScanned(std::size_t nodesScanned) {
	if (enabled) {
		counters.shutdownUpdateNodesScanned += nodesScanned;
	}
}

void PerformanceTracer::recordJobToJsonCall() {
	if (enabled) {
		counters.jobToJsonCalls += 1;
	}
}

void PerformanceTracer::recordNodeToJsonCall() {
	if (enabled) {
		counters.nodeToJsonCalls += 1;
	}
}

void PerformanceTracer::recordJobUtilsRuntimeCall() {
	if (enabled) {
		counters.jobutilsRuntimeCalls += 1;
	}
}

void PerformanceTracer::recordEventLogWrite() {
	if (enabled) {
		counters.eventLogWrites += 1;
	}
}

void PerformanceTracer::recordNodeUtilizationRow() {
	if (enabled) {
		counters.nodeUtilizationRows += 1;
	}
}

void PerformanceTracer::recordShrink(std::size_t jobs, std::size_t nodes) {
	if (enabled) {
		counters.jobsShrunk += jobs;
		counters.nodesShrunk += nodes;
	}
}

void PerformanceTracer::recordExpand(std::size_t jobs, std::size_t nodes) {
	if (enabled) {
		counters.jobsExpanded += jobs;
		counters.nodesExpanded += nodes;
	}
}

void PerformanceTracer::recordAgreementAdded() {
	if (enabled) {
		counters.agreementsAdded += 1;
	}
}

void PerformanceTracer::recordAgreementFulfilled() {
	if (enabled) {
		counters.agreementsFulfilled += 1;
	}
}

void PerformanceTracer::writeHeader() {
	output << "sim_time,scheduler,invocation_type,wall_time_ms_total,queue_size,pending_jobs,"
		   << "running_jobs,running_malleable_jobs,free_nodes_cached,scheduled_jobs,"
		   << "requesting_job_id,requested_nodes";
	for (const auto& phase : phaseColumns()) {
		output << "," << phase << "_ms";
	}
	output << ",node_cache_hits_delta,node_cache_misses_delta,node_cache_rebuilds_delta,"
		   << "node_cache_rebuild_nodes_scanned_delta,shutdown_policy_accept_checks_delta,"
		   << "shutdown_filter_calls_delta,shutdown_filter_nodes_in_delta,shutdown_filter_nodes_out_delta,"
		   << "shutdown_update_nodes_scanned_delta,job_to_json_calls_delta,node_to_json_calls_delta,"
		   << "jobutils_runtime_calls_delta,event_log_writes_delta,node_utilization_rows_delta,"
		   << "jobs_shrunk_delta,nodes_shrunk_delta,jobs_expanded_delta,nodes_expanded_delta,"
		   << "agreements_added_delta,agreements_fulfilled_delta\n";
}

void PerformanceTracer::writeComponentHeader() {
	componentOutput << "wall_elapsed_s,last_observed_sim_time";
	for (const auto& component : componentColumns()) {
		componentOutput << "," << component << "_ms_delta," << component << "_count_delta";
	}
	componentOutput << "\n";
}

void PerformanceTracer::maybeWriteComponentSnapshot(bool force) {
	if (!enabled || !componentOutput.is_open()) {
		return;
	}

	const auto now = std::chrono::steady_clock::now();
	const double elapsedSinceFlush = std::chrono::duration<double>(now - lastComponentFlush).count();
	if (!force && elapsedSinceFlush < componentFlushIntervalSeconds) {
		return;
	}

	bool hasDelta = false;
	for (const auto& component : componentColumns()) {
		const auto currentIt = componentCounters.find(component);
		const auto previousIt = lastComponentSnapshot.find(component);
		const ComponentAggregate current = currentIt == componentCounters.end() ? ComponentAggregate{} : currentIt->second;
		const ComponentAggregate previous = previousIt == lastComponentSnapshot.end() ? ComponentAggregate{} : previousIt->second;
		if (current.count != previous.count || current.nanoseconds != previous.nanoseconds) {
			hasDelta = true;
			break;
		}
	}
	if (!hasDelta && !force) {
		lastComponentFlush = now;
		return;
	}

	const double wallElapsed = std::chrono::duration<double>(now - componentTraceStart).count();
	componentOutput << wallElapsed << "," << lastObservedSimTime;
	for (const auto& component : componentColumns()) {
		const auto currentIt = componentCounters.find(component);
		const auto previousIt = lastComponentSnapshot.find(component);
		const ComponentAggregate current = currentIt == componentCounters.end() ? ComponentAggregate{} : currentIt->second;
		const ComponentAggregate previous = previousIt == lastComponentSnapshot.end() ? ComponentAggregate{} : previousIt->second;
		const std::int64_t nsDelta = current.nanoseconds >= previous.nanoseconds ?
									 current.nanoseconds - previous.nanoseconds : 0;
		const std::uint64_t countDelta = current.count >= previous.count ? current.count - previous.count : 0;
		componentOutput << "," << nsToMs(nsDelta) << "," << countDelta;
		lastComponentSnapshot[component] = current;
	}
	componentOutput << "\n";
	componentOutput.flush();
	lastComponentFlush = now;
}

double PerformanceTracer::nsToMs(std::int64_t nanoseconds) {
	return static_cast<double>(nanoseconds) / 1000000.0;
}

std::uint64_t PerformanceTracer::delta(std::uint64_t current, std::uint64_t previous) {
	return current >= previous ? current - previous : 0;
}
