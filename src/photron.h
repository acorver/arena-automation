#pragma once

#include "common.h"

namespace photron {

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

	int Init( boost::thread* pThread );

	bool IsInitialized();

	void UpdateLiveImage();

	unsigned long StartRecording();

	unsigned long SoftwareTrigger();

	bool AttemptToStartClient(int clientID);

	void Save(std::string prefix, float startTimeAgo, float endTimeAgo);

	std::string GetLiveImage(int index);

	int  NumberBusy();
	void AddBusy();
	void AddNonBusy();

	/* ========================================================================
	   Photron helper function 
	======================================================================== */

	unsigned long GetStatus(unsigned long nDeviceNo);
	unsigned long SetLiveMode(unsigned long nDeviceNo, unsigned long *nStatus, unsigned long *nErrorCode);
	unsigned long SetPlaybackMode(unsigned long nDeviceNo, unsigned long *nStatus, unsigned long *nErrorCode);
	unsigned long SetLutMode(unsigned long nDeviceNo, unsigned long nChildNo, unsigned long nMode, unsigned long *nErrorCode);
	unsigned long SetExternalInMode(unsigned long deviceNo, unsigned long extInPortNo, unsigned long mode);

}