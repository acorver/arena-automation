#include "stdafx.h"
#include "settings.h"

json g_Settings;
std::map<std::string, json> g_SettingsMap;

void settings::Init() {

	LoadSettings();
}

void settings::LoadSettings() {

	std::ifstream i("./settings/settings.json");
	std::stringstream buffer;
	buffer << i.rdbuf();
	i.close();
	settings::SetSettingsJSON(buffer.str());
}

void settings::SaveSettings() {

	std::ofstream o("./settings/settings.json");
	o << settings::GetSettingsJSON();
	o.close();

}

void _CallSettingsChangedHandler(std::string path, json* oldS, json* newS) {
	// ...
}

void _CompareSettings(std::string path, json* oldS, json* newS) {

	json::diff(*oldS, *newS);
}

void _Index(std::string path, json s) {

	if (s.is_object()) {
		for (json::iterator it = s.begin(); it != s.end(); ++it) {
			_Index(path + (path=="" ? "" : ".") + it.key(), it.value());
		}
	} else {
		g_SettingsMap[path] = s;
	}
}

void settings::SetSettings(json s) {

	json oldSettings(g_Settings);
	g_Settings = s;

	// Update key-value pairs for quick indexing
	g_SettingsMap.clear();
	_Index("",g_Settings["settings"]);

	// Compute difference
	_CompareSettings("", &oldSettings, &g_Settings);
}

json settings::GetSettings() {
	
	// TODO: Does this copy the settings? We don't want to pass the settings by 
	//       reference (SetSettings has to be called!)
	json c(g_Settings);
	return c;
}

std::string settings::GetSettingsJSON() {

	return g_Settings.dump();
}

void settings::SetSettingsJSON(std::string str) {

	settings::SetSettings(json::parse(str));
}

void settings::RegisterHandler(const char* strKey, void(*fHandler)(void* pValue)) {

	// TODO
}
