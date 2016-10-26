#include "stdafx.h"

#include "common.h"
#include "hardware.h"
#include "photron.h"
#include "usbcamera.h"
#include "motion.h"
#include "log.h"

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

std::string common::toStr(const char* str, ...) {
	char buffer[2048];
	va_list args;
	va_start(args, str);
	vsprintf(buffer, str, args);
	va_end(args);
	return std::string(buffer);
}

std::string common::GetTimeStr(const char* pattern) {
	// Note that time_facet's do not have to be deleted:
	// http://stackoverflow.com/questions/17779660/who-is-responsible-for-deleting-the-facet
	std::stringstream ss;
	boost::posix_time::time_facet *pFileFacet =
		new boost::posix_time::time_facet(pattern);
	ss.imbue(std::locale(ss.getloc(), pFileFacet));
	ss << boost::posix_time::second_clock::local_time();
	return std::string(ss.str().c_str());
}

long long common::GetTimestamp() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string common::GetTimestampStr() {
	long long x = common::GetTimestamp();
	char str[64];
	sprintf(str, "%lld", x);
	return std::string(str);
}

void _Save(float startTimeAgo, float endTimeAgo) {
	
	// TODO: Log on different thread
	// Log
	logging::Log("Saving [%f, %f].", startTimeAgo, endTimeAgo);

	// Determine the prefix to use for saving the data files
	std::string prefix = common::GetTimeStr("./data/%Y-%m-%d %H-%M-%S_");

	// Save motion capture data
	motion::Save(prefix, startTimeAgo, endTimeAgo);

	// Save USB camera data
	usbcamera::Save(prefix, startTimeAgo, endTimeAgo);

	// Save high-speed camera data
	if (photron::IsInitialized()) {
		photron::Save(prefix, startTimeAgo, endTimeAgo);
	}
}

void common::Save(float startTimeAgo, float endTimeAgo) {

	// Trigger recording!!
	hardware::SendTrigger();

	// Maybe send a system timestamp to thread? That way we can take additional delays into account... (TODO)
	startTimeAgo += _s<float>("tracking.known_trigger_delay");
	endTimeAgo   += _s<float>("tracking.known_trigger_delay");

	logging::Log("Starting saving thread [%f, %f].", startTimeAgo, endTimeAgo);
	boost::thread t(_Save, startTimeAgo, endTimeAgo);
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