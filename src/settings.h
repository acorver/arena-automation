#pragma once

#include "common.h"

#define CORTEX_INVALID_MARKER 9999999

extern std::map<const char*, json> g_SettingsMap;

namespace settings {
	
	void Init();
	
	void RegisterHandler(const char* strKey, void(*fHandler)(void* pValue));

	void LoadSettings();
	void SaveSettings();
	
	void SetSettings(json s);

	template<typename T> 
	T GetSetting(const char* key) {
		return g_SettingsMap[key].get<T>();
	}

	json GetSettings();

	std::string GetSettingsJSON();
	void SetSettingsJSON(std::string);
}
