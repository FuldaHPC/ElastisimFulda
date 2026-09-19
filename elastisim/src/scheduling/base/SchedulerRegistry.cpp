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

#include "SchedulerRegistry.h"
#include <simgrid/s4u.hpp>

XBT_LOG_NEW_DEFAULT_CATEGORY(SchedulerRegistry, "Scheduler Registry");

std::map<std::string, SchedulerRegistry::SchedulerFactory>& SchedulerRegistry::getRegistry() {
    static std::map<std::string, SchedulerFactory> registry;
    return registry;
}

void SchedulerRegistry::registerScheduler(const std::string& name, SchedulerFactory factory) {
    auto& registry = getRegistry();
    
    if (registry.find(name) != registry.end()) {
        XBT_WARN("Scheduler '%s' is already registered, overwriting", name.c_str());
    }
    
    registry[name] = factory;
    XBT_DEBUG("Registered scheduler: %s", name.c_str());
}

std::unique_ptr<IScheduler> SchedulerRegistry::create(const std::string& name) {
    auto& registry = getRegistry();
    
    auto it = registry.find(name);
    if (it == registry.end()) {
        throw std::runtime_error("Scheduler '" + name + "' not registered. Available: " + 
                                [&]() {
                                    std::string available;
                                    for (const auto& pair : registry) {
                                        if (!available.empty()) available += ", ";
                                        available += pair.first;
                                    }
                                    return available.empty() ? "(none)" : available;
                                }());
    }
    
    return it->second();
}

std::vector<std::string> SchedulerRegistry::listAvailable() {
    auto& registry = getRegistry();
    std::vector<std::string> names;
    names.reserve(registry.size());
    
    for (const auto& pair : registry) {
        names.push_back(pair.first);
    }
    
    return names;
}

bool SchedulerRegistry::isRegistered(const std::string& name) {
    auto& registry = getRegistry();
    return registry.find(name) != registry.end();
}
