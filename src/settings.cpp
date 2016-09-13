#include "stdafx.h"

#include "settings.h"

json g_Settings;

void settings::Init() {

	LoadSettings();
}

void settings::LoadSettings() {

	std::ifstream i("Z:/people/Abel/ArenaAutomation/deploy/settings/settings.json");
	std::stringstream buffer;
	buffer << i.rdbuf();
	i.close();
	settings::SetSettingsJSON(buffer.str());
}

void settings::SaveSettings() {

	std::ofstream o("Z:/people/Abel/ArenaAutomation/deploy/settings/settings.json");
	o << settings::GetSettingsJSON();
	o.close();

}

void _CallSettingsChangedHandler(std::string path, json* oldS, json* newS) {
	// ...
}

void _CompareSettings(std::string path, json* oldS, json* newS) {

	// Have the types of the settings changed? If so, they can't be the same
	if ( oldS->type() != newS->type() ) {
		_CallSettingsChangedHandler( path, oldS, newS );
		return;
	}

	// Are these non-iterable values to compare?
	if ( !newS->is_object() && !newS->is_array() ) {
		if ( *newS != *oldS ) {
			_CallSettingsChangedHandler(path, oldS, newS);
			return;
		}
	}

	// check for deleted settings in this object
	for (json::iterator it = oldS->begin(); it != oldS->end(); ++it) {
		if (newS->count(it.key()) == 0) {
			_CallSettingsChangedHandler(path + "." + it.key(), &it.value(), 0);
		}
	}

	// Check for new/changed settings in this object
	for (json::iterator it = newS->begin(); it != newS->end(); ++it) {
		if (oldS->count(it.key()) == 0) {
			_CallSettingsChangedHandler(path + "." + it.key(), 0, &it.value());
		} else {
			_CompareSettings(path + "." + it.key(), &((*oldS)[it.key()]), &it.value());
		}
	}
}

void settings::SetSettings(json s) {

	json oldSettings(g_Settings);
	g_Settings = s;

	// Compute difference
	_CompareSettings("", &oldSettings, &g_Settings);
}

json settings::GetSettings() {
	
	json c(g_Settings); // TODO: Does this copy the settings? We don't want to pass the settings by reference (SetSettings has to be called!)
	return c;
}

std::string settings::GetSettingsJSON() {

	return g_Settings.dump();
}

void settings::SetSettingsJSON(std::string str) {

	g_Settings = json::parse(str);
}

void settings::RegisterHandler(const char* strKey, void(*fHandler)(void* pValue)) {

	// TODO
}
