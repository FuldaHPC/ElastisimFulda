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

#ifndef SCHEDULERREGISTRY_H
#define SCHEDULERREGISTRY_H

#include <string>
#include <memory>
#include <map>
#include <vector>
#include <functional>
#include <stdexcept>
#include "IScheduler.h"
#include "../../third-party/nlohmann_json/json.hpp"

// Factory for creating schedulers by name (Factory Pattern)
class SchedulerRegistry {
public:
    using SchedulerFactory = std::function<std::unique_ptr<IScheduler>()>;
    
    // Register a scheduler with a factory function
    static void registerScheduler(const std::string& name, SchedulerFactory factory);
    
    // Create scheduler instance by name
    static std::unique_ptr<IScheduler> create(const std::string& name);
    
    // List all registered scheduler names
    static std::vector<std::string> listAvailable();
    
    // Check if scheduler is registered
    static bool isRegistered(const std::string& name);
    
private:
    static std::map<std::string, SchedulerFactory>& getRegistry();
};

#endif // SCHEDULERREGISTRY_H
