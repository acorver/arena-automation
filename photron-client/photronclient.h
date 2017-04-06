#pragma once

#include "stdafx.h"
#include "../src/settings.h"

namespace photronclient {

	void StartServer();
	void Log(const char* msg, ...);

	int Init(boost::thread* pThread);

	void Configure();

	bool IsInitialized();

	void UpdateLiveImage();

	unsigned long StartRecording();

	unsigned long SoftwareTrigger();

	void Save(std::string prefix, float startTimeAgo, float endTimeAgo);

	std::string GetLiveImage(int index);

	unsigned long GetStatus(unsigned long nDeviceNo);
	unsigned long SetLiveMode(unsigned long nDeviceNo, unsigned long *nStatus, unsigned long *nErrorCode);
	unsigned long SetPlaybackMode(unsigned long nDeviceNo, unsigned long *nStatus, unsigned long *nErrorCode);
	unsigned long SetLutMode(unsigned long nDeviceNo, unsigned long nChildNo, unsigned long nMode, unsigned long *nErrorCode);
	unsigned long SetExternalInMode(unsigned long deviceNo, unsigned long extInPortNo, unsigned long mode);


	typedef struct CameraInfo {
		unsigned long nDeviceNo;
		std::deque<unsigned long> nChildNos;

		CameraInfo(unsigned long deviceNo) {
			nDeviceNo = deviceNo;
		}

		void AddChild(unsigned long nChildNo) {
			nChildNos.push_back(nChildNo);
		}

	} CameraInfo;
}