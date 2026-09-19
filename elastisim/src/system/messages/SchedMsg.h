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

#ifndef ELASTISIM_SCHEDMSG_H
#define ELASTISIM_SCHEDMSG_H


class Job;

class Node;

enum SchedEventType {
	INVOKE_SCHEDULING,
	JOB_SUBMIT,
	WALLTIME_EXCEEDED,
	SCHEDULING_POINT,
	EVOLVING_REQUEST,
	WORKLOAD_PROCESSED,
	SCHEDULER_FINALIZE,
	// NEW: Malleability events
	JOB_SHRINK,              // Job was shrunk (nodes removed)
	JOB_EXPAND,              // Job was expanded (nodes added)
	AGREEMENT_ADDED,         // Agreement created (nodes reserved)
	AGREEMENT_FULFILLED      // Agreement fulfilled (job started with reserved nodes)
};

class SchedMsg {

private:
	const SchedEventType type;
	Job* job;
	const int numberOfNodes;

public:

	explicit SchedMsg(SchedEventType type);

	SchedMsg(SchedEventType type, Job* job);

	SchedMsg(SchedEventType type, Job* job, int numberOfNodes);

	[[nodiscard]] SchedEventType getType() const;

	[[nodiscard]] Job* getJob() const;

	[[nodiscard]] int getNumberOfNodes() const;

};


#endif //ELASTISIM_SCHEDMSG_H
