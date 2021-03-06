#pragma once

#define CORTEX_INVALID_MARKER 9999999

extern std::map<std::string, json> g_SettingsMap;

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

// Copy GetSetting for shorthand use (make this code duplication more elegant in the future)
template<typename T>
T _s(const char* key) {

	return g_SettingsMap[key].get<T>();
}
// Safe version of _s, which checks for existence first
template<typename T>
T _ss(const char* key) {

	if (g_SettingsMap.find(key) == g_SettingsMap.end()) {
		return T();
	}
	else {
		return g_SettingsMap[key].get<T>();
	}
}
