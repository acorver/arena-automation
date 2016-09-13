#pragma once

#include "common.h"

namespace logging {

	void Init();

	void Log(const char* msg, ...);

	//void WatchLogBuffer();

	std::vector<std::string> GetCache();
	std::string GetCache(int index);
	int GetCacheSize();
}
