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

#include "Application.h"

#include "Job.h"
#include "Phase.h"
#include "Workload.h"
#include "Node.h"
#include "Task.h"
#include "SchedMsg.h"
#include "Utility.h"
#include "Configuration.h"
#include "PerformanceTracer.h"

#include <cmath>


XBT_LOG_NEW_DEFAULT_CATEGORY(Application, "Messages within the application");

namespace {
constexpr double SCHEDULING_POINT_FAST_PATH_TIME_EPSILON = 0.001;

enum SchedulingPointFastPathEligibility {
	SCHEDULING_POINT_FAST_PATH_ALLOWED,
	SCHEDULING_POINT_FAST_PATH_BLOCKED_CONFIG_DISABLED,
	SCHEDULING_POINT_FAST_PATH_BLOCKED_SCHEDULE_ON_SP,
	SCHEDULING_POINT_FAST_PATH_BLOCKED_JOB_TYPE,
	SCHEDULING_POINT_FAST_PATH_BLOCKED_WALLTIME,
	SCHEDULING_POINT_FAST_PATH_BLOCKED_PERIODIC_TICK
};

bool isPeriodicSchedulingTick() {
	static const double schedulingInterval = Configuration::exists("scheduling_interval") ?
											 (double) Configuration::get("scheduling_interval") : 0;
	if (schedulingInterval <= 0) {
		return false;
	}

	const double clock = simgrid::s4u::Engine::get_clock();
	double remainder = std::fmod(clock, schedulingInterval);
	if (remainder < 0) {
		remainder += schedulingInterval;
	}

	return remainder <= SCHEDULING_POINT_FAST_PATH_TIME_EPSILON ||
		   schedulingInterval - remainder <= SCHEDULING_POINT_FAST_PATH_TIME_EPSILON;
}

SchedulingPointFastPathEligibility getSchedulingPointFastPathEligibility(const Job* job) {
	static const bool fastPathEnabled = Configuration::getBoolIfExists("enable_scheduling_point_fast_path");
	static const bool scheduleOnSchedulingPoint = Configuration::getBoolIfExists("schedule_on_scheduling_point");

	if (!fastPathEnabled) {
		return SCHEDULING_POINT_FAST_PATH_BLOCKED_CONFIG_DISABLED;
	}
	if (scheduleOnSchedulingPoint) {
		return SCHEDULING_POINT_FAST_PATH_BLOCKED_SCHEDULE_ON_SP;
	}
	if (job->getType() != MALLEABLE) {
		return SCHEDULING_POINT_FAST_PATH_BLOCKED_JOB_TYPE;
	}
	if (job->getWalltime() > 0) {
		return SCHEDULING_POINT_FAST_PATH_BLOCKED_WALLTIME;
	}
	if (isPeriodicSchedulingTick()) {
		return SCHEDULING_POINT_FAST_PATH_BLOCKED_PERIODIC_TICK;
	}
	return SCHEDULING_POINT_FAST_PATH_ALLOWED;
}

void recordSchedulingPointFastPathBlocked(SchedulingPointFastPathEligibility eligibility) {
	switch (eligibility) {
		case SCHEDULING_POINT_FAST_PATH_ALLOWED:
			return;
		case SCHEDULING_POINT_FAST_PATH_BLOCKED_CONFIG_DISABLED:
			PerformanceTracer::recordComponentEvent("application_scheduling_point_fast_blocked_config_disabled");
			return;
		case SCHEDULING_POINT_FAST_PATH_BLOCKED_SCHEDULE_ON_SP:
			PerformanceTracer::recordComponentEvent("application_scheduling_point_fast_blocked_schedule_on_sp");
			return;
		case SCHEDULING_POINT_FAST_PATH_BLOCKED_JOB_TYPE:
			PerformanceTracer::recordComponentEvent("application_scheduling_point_fast_blocked_job_type");
			return;
		case SCHEDULING_POINT_FAST_PATH_BLOCKED_WALLTIME:
			PerformanceTracer::recordComponentEvent("application_scheduling_point_fast_blocked_walltime");
			return;
		case SCHEDULING_POINT_FAST_PATH_BLOCKED_PERIODIC_TICK:
			PerformanceTracer::recordComponentEvent("application_scheduling_point_fast_blocked_periodic_tick");
			return;
	}
	xbt_die("Unknown scheduling-point fast-path eligibility %d", eligibility);
}

SchedulingPointDecision synchronizeSchedulingPointDecision(Job* job, int rank,
														   const simgrid::s4u::BarrierPtr& barrier) {
	PerformanceTracer::recordComponentEvent("application_barrier_phase");
	barrier->wait();

	const SchedulingPointDecision decision = job->getSchedulingPointDecision();

	PerformanceTracer::recordComponentEvent("application_barrier_phase");
	barrier->wait();
	if (rank == 0) {
		job->clearSchedulingPointDecision();
	}

	if (decision == SCHEDULING_POINT_DECISION_UNSET) {
		xbt_die("Unset scheduling-point fast-path decision for job %d", job->getId());
	}
	return decision;
}

void forwardSchedulingPointToScheduler(Job* job) {
	s4u_Mailbox* mailboxScheduler = s4u_Mailbox::by_name("Scheduler");
	mailboxScheduler->put_init(new SchedMsg(SCHEDULING_POINT, job), 0)->detach();
}
}

Application::Application(Node* node, Job* job, int rank, bool logTaskTimes) :
	node(node), job(job), rank(rank), logTaskTimes(logTaskTimes){}

void Application::waitForAsyncActivities(const std::vector<simgrid::s4u::ActivityPtr>& asyncActivities) {
	PerformanceTracer::recordComponentEvent("application_async_wait", asyncActivities.size());
	for (const auto& activity: asyncActivities) {
		activity->wait();
	}
}

void
Application::executeOneTimePhase(const Phase* phase, const Node* node, const Job* job, const std::vector<Node*>& nodes,
								 int rank, const simgrid::s4u::BarrierPtr& barrier) {

	if (phase == nullptr) {
		return;
	}
	std::vector<simgrid::s4u::ActivityPtr> asyncActivities;
	for (int i = 0; i < phase->getIterations(); ++i) {
		for (const auto& task: phase->getTasks()) {
			int iterations = task->getIterations();
			PerformanceTracer::recordComponentEvent("application_task_iterations", iterations);
			double taskStart = Utility::logTaskStart(task, iterations);
			for (int j = 0; j < iterations; ++j) {
				double iterationStart = Utility::logIterationStart(iterations, j);
				if (task->isSynchronized()) {
					PerformanceTracer::recordComponentEvent("application_barrier_phase");
					barrier->wait();
				}
				if (task->isAsynchronous()) {
					std::vector<simgrid::s4u::ActivityPtr> activities = task->executeAsync(node, job, nodes, rank);
					asyncActivities.insert(std::end(asyncActivities), std::begin(activities), std::end(activities));
				} else {
					task->execute(node, job, nodes, rank, barrier);
				}
				Utility::logIterationEnd(iterations, j, iterationStart);
			}
			double taskEnd = Utility::logTaskEnd(task, taskStart);
			if (logTaskTimes) {
				node->logTaskTime(job, task, taskEnd);
			}
		}
	}
	for (const auto& activity: asyncActivities) {
		activity->wait();
	}
}

void Application::operator()() {
	PerformanceTracer::recordComponentEvent("application_actor_start");

	if (node->isInitializing(job)) {
		PerformanceTracer::recordComponentEvent("application_init_phase");
		executeOneTimePhase(job->getWorkload()->getInitPhase(), node, job, job->getExecutingNodes(), rank,
							node->getBarrier(job));
		node->markInitialized(job);
	}

	if (node->isReconfiguring(job)) {
		PerformanceTracer::recordComponentEvent("application_reconfiguration_phase");
		executeOneTimePhase(job->getWorkload()->getReconfigurationPhase(), node, job, job->getExecutingNodes(), rank,
							node->getBarrier(job));
		node->markReconfigured(job);
	}

	const simgrid::s4u::BarrierPtr& barrier = node->getBarrier(job);
	PerformanceTracer::recordComponentEvent("application_barrier_phase");
	barrier->wait();
	if (rank == 0) {
		job->setState(RUNNING);
	}

	if (node->isExpanding(job)) {
		PerformanceTracer::recordComponentEvent("application_expansion_phase");
		executeOneTimePhase(job->getWorkload()->getExpansionPhase(), node, job, job->getExpandingNodes(),
							node->getExpandRank(job), node->getExpandBarrier(job));
		node->markExpanded(job);
	}

	std::deque<const Phase*> phaseQueue = job->getWorkload()->getPhases();
	const Phase* phase = phaseQueue.front();
	int remainingIterations = phase->getIterations();
	int completedPhases = 0;

	std::vector<simgrid::s4u::ActivityPtr> asyncActivities;

	bool initialPhase = true;
	while (remainingIterations > 0) {

		if (!initialPhase) {
			if ((job->getType() == EVOLVING || job->getType() == ADAPTIVE) && phase->hasEvolvingRequest()) {
				const int numberOfNodes = job->calculateEvolvingRequest(phase->getEvolvingModel(),
																		phase->getInitialIterations() -
																		remainingIterations);
				if (numberOfNodes != job->getNumberOfExecutingNodes()) {
					waitForAsyncActivities(asyncActivities);
					asyncActivities.clear();
					PerformanceTracer::recordComponentEvent("application_barrier_phase");
					barrier->wait();
					if (rank == 0) {
						job->advanceWorkload(completedPhases, remainingIterations);
						PerformanceTracer::recordComponentEvent("application_evolving_request");
						s4u_Mailbox* mailboxScheduler = s4u_Mailbox::by_name("Scheduler");
						mailboxScheduler->put_init(new SchedMsg(EVOLVING_REQUEST, job, numberOfNodes), 0)->detach();
					}
					break;
				}
			} else if ((job->getType() == MALLEABLE || job->getType() == ADAPTIVE) && phase->hasSchedulingPoint()) {
				waitForAsyncActivities(asyncActivities);
				asyncActivities.clear();
				PerformanceTracer::recordComponentEvent("application_barrier_phase");
				barrier->wait();

				const SchedulingPointFastPathEligibility fastPathEligibility = getSchedulingPointFastPathEligibility(job);
				if (fastPathEligibility == SCHEDULING_POINT_FAST_PATH_ALLOWED) {
					if (rank == 0) {
						job->clearSchedulingPointDecision();
						job->advanceWorkload(completedPhases, remainingIterations);
						PerformanceTracer::recordComponentEvent("application_scheduling_point");
						if (job->getState() == RUNNING) {
							job->setSchedulingPointDecision(SCHEDULING_POINT_FAST_CONTINUE);
							PerformanceTracer::recordComponentEvent("application_scheduling_point_fast_continue");
							PerformanceTracer::recordComponentEvent("application_scheduling_point_fast_continue_nodes",
																	job->getExecutingNodes().size());
						} else {
							job->setSchedulingPointDecision(SCHEDULING_POINT_USE_SCHEDULER);
							PerformanceTracer::recordComponentEvent("application_scheduling_point_fast_scheduler_fallback");
							PerformanceTracer::recordComponentEvent("application_scheduling_point_fast_blocked_state");
						}
					}

					const SchedulingPointDecision decision = synchronizeSchedulingPointDecision(job, rank, barrier);
					if (decision == SCHEDULING_POINT_FAST_CONTINUE) {
						phaseQueue = job->getWorkload()->getPhases();
						if (phaseQueue.empty()) {
							xbt_die("Fast scheduling-point continuation for job %d found an empty workload", job->getId());
						}
						phase = phaseQueue.front();
						remainingIterations = phase->getIterations();
						completedPhases = 0;
						initialPhase = true;
						continue;
					}
					if (decision == SCHEDULING_POINT_USE_SCHEDULER) {
						if (rank == 0) {
							forwardSchedulingPointToScheduler(job);
						}
					} else {
						xbt_die("Invalid scheduling-point fast-path decision %d for job %d", decision, job->getId());
					}
				} else {
					if (rank == 0) {
						job->advanceWorkload(completedPhases, remainingIterations);
						PerformanceTracer::recordComponentEvent("application_scheduling_point");
						recordSchedulingPointFastPathBlocked(fastPathEligibility);
						forwardSchedulingPointToScheduler(job);
					}
				}
				break;
			}
		}

		if (phase->hasBarrier()) {
			waitForAsyncActivities(asyncActivities);
			asyncActivities.clear();
			PerformanceTracer::recordComponentEvent("application_barrier_phase");
			barrier->wait();
		}

		std::deque<const Task*> taskQueue = phase->getTasks();
		while (!taskQueue.empty()) {
			const Task* task = taskQueue.front();
			int iterations = task->getIterations();
			PerformanceTracer::recordComponentEvent("application_task_iterations", iterations);
			double taskStart = Utility::logTaskStart(task, iterations);
			for (int i = 0; i < iterations; ++i) {
				double iterationStart = Utility::logIterationStart(iterations, i);
				if (task->isSynchronized()) {
					PerformanceTracer::recordComponentEvent("application_barrier_phase");
					barrier->wait();
				}
				if (task->isAsynchronous()) {
					std::vector<simgrid::s4u::ActivityPtr> activities =
							task->executeAsync(node, job, job->getExecutingNodes(), rank);
					asyncActivities.insert(std::end(asyncActivities), std::begin(activities), std::end(activities));
				} else {
					task->execute(node, job, job->getExecutingNodes(), rank, barrier);
				}
				Utility::logIterationEnd(iterations, i, iterationStart);
			}
			double taskEnd = Utility::logTaskEnd(task, taskStart);
			if (logTaskTimes) {
				node->logTaskTime(job, task, taskEnd);
			}
			taskQueue.pop_front();
		}

		--remainingIterations;
		initialPhase = false;
		if (remainingIterations == 0) {
			phaseQueue.pop_front();
			if (phaseQueue.empty()) {
				waitForAsyncActivities(asyncActivities);
				asyncActivities.clear();
				PerformanceTracer::recordComponentEvent("application_barrier_phase");
				barrier->wait();
				if (rank == 0) {
					PerformanceTracer::recordComponentEvent("application_workload_processed");
					s4u_Mailbox* mailboxScheduler = s4u_Mailbox::by_name("Scheduler");
					mailboxScheduler->put_init(new SchedMsg(WORKLOAD_PROCESSED, job), 0)->detach();
				}
			} else {
				++completedPhases;
				phase = phaseQueue.front();
				remainingIterations = phase->getIterations();
			}
		}
	}

}
