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

#ifndef ELASTISIM_CONFIGURATION_H
#define ELASTISIM_CONFIGURATION_H


#include <json.hpp>

class Configuration {

private:
	static nlohmann::json configuration;
	static bool initialized;

public:

	static void init(const std::string& configurationFilePath);

	[[nodiscard]] static nlohmann::basic_json<> get(const std::string& key);

	[[nodiscard]] static bool exists(const std::string& key);

	[[nodiscard]] static bool getBoolIfExists(const std::string& key);

	static void set(const std::string& key, const nlohmann::json& value);

};


#endif //ELASTISIM_CONFIGURATION_H
