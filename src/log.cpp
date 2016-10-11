#include "stdafx.h"

#include "log.h"

#ifndef ELPP_INITIALIZED
#define ELPP_INITIALIZED
INITIALIZE_EASYLOGGINGPP
#endif

std::vector<std::string> vCache;
bool vCacheLock;
//std::ofstream outFile;

/*
bool logLocked;
std::string buffer[1000];
*/

void logging::Init() {

	// Load configuration from file
	el::Configurations conf("./settings/log.conf");
	el::Loggers::reconfigureAllLoggers(conf);

	vCacheLock = false;

	//outFile.open("C:/Users/Inigo/Desktop/log.txt");
	// TODO: Append date/time to log filename
	// TODO: Print to command line
	// outFile.open("./data/log.txt");

	// Initialize separate thread
	//boost::thread t(logging::WatchLogBuffer);
}

/*
void logging::WatchLogBuffer() {
	while (true) {

	}
}
*/

void logging::Log(const char* msg, ...) {

	// TMP:
	//if (std::string(msg).find("[MOTION]") == std::string::npos &&
	//	std::string(msg).find("[D]") == std::string::npos) { return; }

	char buffer[4096];

	va_list args;
	va_start(args, msg);
	vsprintf(buffer, msg, args);
	va_end(args);

	LOG(INFO) << std::string(buffer).c_str();

	// TODO: Use more feature-rich logger in the future, and better log querying system (e.g. logstash?)
	if (std::string(msg).find("[D]") == std::string::npos) { 
		while (vCacheLock) {  
			boost::this_thread::sleep_for(boost::chrono::milliseconds(1));
		}
		vCacheLock = true;
		vCache.push_back(std::string(buffer));
		vCacheLock = false;
	}

	//outFile << buffer; 
	//outFile << std::endl;
	//outFile.flush();
}

std::vector<std::string> logging::GetCache() {
	while (vCacheLock) { boost::this_thread::sleep_for(boost::chrono::milliseconds(1)); }
	return vCache;
}

std::string logging::GetCache(int index) {
	while (vCacheLock) { boost::this_thread::sleep_for(boost::chrono::milliseconds(1)); }
	return vCache[index];
}

int logging::GetCacheSize() {
	while (vCacheLock) { boost::this_thread::sleep_for(boost::chrono::milliseconds(1)); }
	return vCache.size();
}