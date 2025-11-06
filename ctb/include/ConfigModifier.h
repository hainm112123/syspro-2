#pragma once
#ifndef CONFIG_MODIFIER_H
#define CONFIG_MODIFIER_H
#include "utils.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#endif

#include "nlohmann/json.hpp"
#include <vector>
#include <iostream>

class ConfigModifier {
private:
	const std::string configFilePath = "config.json";
	const uint32_t port = 8000;
public:
	ConfigModifier() {}
	~ConfigModifier() {}

	std::string loadConfigAsString();
	bool start();
};

#endif