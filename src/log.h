#pragma once

#include "common.h"

namespace logging {

	void Init();
	void Log(const char* msg, ...);
	std::string QueryToJSON(std::string query, int start);
}
