#include "stdafx.h"

#include "common.h"
#include "hardware.h"
#include "photron.h"
#include "usbcamera.h"
#include "motion.h"
#include "log.h"
#include "settings.h"

// This global variable contains the file prefix for the most recent trigger, ensuring all data files corresponding to that trigger 
// have the same prefix, even though some data, such as high speed video, may only be actually saved to disk some time later (as it 
// may potentially be discarded, despite the camera having been triggered).
std::string g_LastTriggerPrefix = "";
float g_LastTriggerStartTimeAgo, g_LastTriggerEndTimeAgo;

// This function returns a prefix for all data files to use
std::string g_CommonOutputPrefix = "";

// Convert a narrow char to a wide char string
std::wstring common::s2ws(const std::string& s)
{
	int len;
	int slength = (int)s.length() + 1;
	len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
	wchar_t* buf = new wchar_t[len];
	MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
	std::wstring r(buf);
	delete[] buf;
	return r;
}

// Convert a wide char to a narrow char string
std::string common::ws2s(const std::wstring& s)
{
	int len;
	int slength = (int)s.length() + 1;
	len = WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, 0, 0, 0, 0);
	char* buf = new char[len];
	WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, buf, len, 0, 0);
	std::string r(buf);
	delete[] buf;
	return r;
}

// Helper function for easily formatting strings containing numerical variables
std::string common::toStr(const char* str, ...) {
	char buffer[2048];
	va_list args;
	va_start(args, str);
	vsprintf(buffer, str, args);
	va_end(args);
	return std::string(buffer);
}

// Helper function for incorporating date/time information into a string
std::string common::GetTimeStr(const char* pattern) {
	// Note that time_facet's do not have to be deleted:
	// http://stackoverflow.com/questions/17779660/who-is-responsible-for-deleting-the-facet
	std::stringstream ss;
	boost::posix_time::time_facet *pFileFacet =
		new boost::posix_time::time_facet(pattern);
	ss.imbue(std::locale(ss.getloc(), pFileFacet));
	ss << boost::posix_time::second_clock::local_time();
	return std::string(ss.str());
}

// Get the current timestamp
long long common::GetTimestamp() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

// Get the current timestamp as a string
std::string common::GetTimestampStr() {
	long long x = common::GetTimestamp();
	char str[64];
	sprintf(str, "%lld", x);
	return std::string(str);
}

// Get a shared output file prefix for all data files
std::string common::GetCommonOutputPrefix() {

	if (g_CommonOutputPrefix == "") {
		std::string outputPrefix = _s<std::string>("output_prefix");
		g_CommonOutputPrefix = common::GetTimeStr(outputPrefix.c_str());

		// Ensure that the directory exists
		boost::filesystem::create_directories(common::GetCommonOutputDirectory());
	}
	
	return g_CommonOutputPrefix;
}

// Get a shared output directory for all data files
std::string common::GetCommonOutputDirectory() {

	boost::system::error_code err;
	int i = boost::replace_all_copy(g_CommonOutputPrefix, "\\", "/").rfind('/');
	return g_CommonOutputPrefix.substr(0, i+1);
}

// Save data to file
void _Save(float startTimeAgo, float endTimeAgo) {
	
	// TODO: Log on different thread
	// Log
	logging::Log("Saving [%f, %f].", startTimeAgo, endTimeAgo);

	// Save motion capture data
	//motion::Save(prefix, startTimeAgo, endTimeAgo);

	// Save USB camera data
	usbcamera::Save(g_LastTriggerPrefix, startTimeAgo, endTimeAgo);
}

// Trigger all data sources
bool common::Trigger(float startTimeAgo, float endTimeAgo, bool allowPendingSave) {

	// Optionally, only allow trigger if photrons are availble
	if (_s<bool>("tracking.trigger_requires_photrons_ready")) {
		if (photron::NumberBusy() > 0) {
			logging::Log("[HARDWARE] Could not send hardware trigger --- Photrons not ready...");
			return false;
		}
	}

	// Is there currently a conflicting trigger queued? If so, don't trigger
	/* This condition is already enforced by "trigger_requires_photrons_ready"... or not? (TODO)
	if (g_LastTriggerPrefix != "" && allowPendingSave) {
		logging::Log("[HARDWARE] Could not send hardware trigger --- other trigger in progress...");
		return  false;
	}
	*/

	// Trigger recording!!
	hardware::SendTrigger();
	logging::Log("Sent trigger");

	// Determine the prefix to use for saving the data files
	g_LastTriggerPrefix = common::GetCommonOutputDirectory() +
		common::GetTimeStr("%Y-%m-%d %H-%M-%S");

	// Maybe send a system timestamp to thread? That way we can take additional delays into account... (TODO)
	g_LastTriggerStartTimeAgo = startTimeAgo;
	g_LastTriggerEndTimeAgo = endTimeAgo;
	g_LastTriggerStartTimeAgo += _s<float>("tracking.known_trigger_delay");
	g_LastTriggerEndTimeAgo   += _s<float>("tracking.known_trigger_delay");

	logging::Log("Starting saving thread [%f, %f].", g_LastTriggerStartTimeAgo, g_LastTriggerEndTimeAgo);
	boost::thread t(_Save, g_LastTriggerStartTimeAgo, g_LastTriggerEndTimeAgo);

	// For code that doesn't want to support handling pending saves, we immediately save everything to disk
	if (!allowPendingSave) {
		common::SaveToDisk(true, "");
	}

	// Successfully triggered
	return true;
}

void common::SaveToDisk(bool save, const char* prefix) {

	// Note: This function can be called separately to decide whether to actually commit the triggered data 
	// to memory

	if (save) {
		logging::Log("[COMMON] Saving pending data to disk!");
	} else {
		logging::Log("[COMMON] Pending data discarded, not saved to disk...");
	}

	// Save high-speed camera data (or not, and simply reset the system)
	if (_s<bool>("photron.use_clients")) {
		
		// Send messages to each client
		for (int clientID = 0; clientID < 1024; clientID++) {

			const char* pref = (prefix != ""?prefix:g_LastTriggerPrefix.c_str());

			std::string prefixEncoded = boost::replace_all_copy( 
				boost::replace_all_copy(boost::replace_all_copy(
					std::string(pref), "\\", "/"), " ", "%20"), "/", "%2f");
			
			std::string sIP = std::string("photron.client_") + std::to_string(clientID) + std::string(".client_ip");
			std::string sPort = std::string("photron.client_") + std::to_string(clientID) + std::string(".port");
			
			if (_ss<std::string>(sIP.c_str()) == "") {
				break;
			} else {
				std::string command = std::string("http://") + _s<std::string>(sIP.c_str()) + std::string(":") +
					std::to_string(_s<int>(sPort.c_str())) + std::string("/save/") + prefixEncoded + std::string("/") + 
					std::string(save?"save":"abortsave") + std::string("/-1/0");
			
				boost::thread t([](std::string cmd) {
					cpr::GetCallback([](cpr::Response r) {

						// Currently, it is unclear if the system returns after the save, or when the device is actually ready...
						// Temporarily, we therefore wait another 2 seconds...
						// TODO: Clear up this issue...
						boost::this_thread::sleep_for(boost::chrono::milliseconds(2000));

						// Signal that this Photron is ready
						photron::AddNonBusy();

						// Signal if all Photrons are ready
						if (photron::NumberBusy() == 0) {
							logging::Log("[PHOTRON] All photrons ready!");
						}

					}, cpr::Url{ cmd.c_str() });

				}, command);

				logging::Log("[PHOTRON] Notified photron client %d", clientID);

				photron::AddBusy();
			}
		}

	} else {
		if (photron::IsInitialized()) {
			if (save) {
				photron::Save(g_LastTriggerPrefix, g_LastTriggerStartTimeAgo, g_LastTriggerEndTimeAgo);
			}
			else {
				photron::StartRecording();
			}
		}
	}
}

std::string common::ImageBuffer::ToJPEG() {
	bool ret;
	int nJPEGBufSize = 1024 * 1024 * 2;
	unsigned char *pJPEGBuf = (unsigned char*) malloc(nJPEGBufSize);

	ret = jpge::compress_image_to_jpeg_file_in_memory(
			pJPEGBuf, nJPEGBufSize, this->nWidth, this->nHeight, 
			1, this->pBuffer);

	if (ret) {
		return std::string(reinterpret_cast<char const*>(pJPEGBuf), 
			nJPEGBufSize);
	}

	return "";
}