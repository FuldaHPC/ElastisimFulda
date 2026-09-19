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

#include "JobSubmitter.h"

#include <simgrid/s4u.hpp>
#include "Job.h"
#include "Workload.h"
#include "Phase.h"
#include "Utility.h"
#include "SimMsg.h"
#include "Configuration.h"
#include "PerformanceTracer.h"
#include "../scheduling/utils/JobUtils.h"

XBT_LOG_NEW_DEFAULT_CATEGORY(User, "Messages within the JobSubmitter actor");

void JobSubmitter::operator()() {

	s4u_Mailbox* mailboxSimulator = s4u_Mailbox::by_name("SimulationEngine");
	std::vector<std::unique_ptr<Job>> jobs;
		{
			PerformanceTracer::ScopedComponent component("job_submitter_read_jobs");
			jobs = Utility::readJobs(Configuration::get("jobs_file"));
		}
		{
			PerformanceTracer::ScopedComponent component("job_submitter_validate_runtime_prediction");
			JobUtils::validateRuntimePredictionJobs(jobs);
		}
		{
			PerformanceTracer::ScopedComponent component("job_submitter_sort_jobs");
			std::stable_sort(std::begin(jobs), std::end(jobs), [](const std::unique_ptr<Job>& a, const std::unique_ptr<Job>& b) {
			return a->getSubmitTime() < b->getSubmitTime();
		});
	}
	PerformanceTracer::recordComponentEvent("job_submitter_submit_message");
	mailboxSimulator->put(new SimMsg(NUMBER_OF_JOBS, jobs.size()), 0);

	for (auto& job: jobs) {
		simgrid::s4u::this_actor::sleep_until(job->getSubmitTime());
		PerformanceTracer::recordComponentEvent("job_submitter_submit_message");
		mailboxSimulator->put(new SimMsg(SUBMIT_JOB, std::move(job)), 0);
	}

}
