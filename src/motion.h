#pragma once

#include "common.h"
#include "settings.h"
#include "log.h"

using namespace boost::numeric::ublas;

// Initialize global variables
extern int             nextBodyIndex;
extern std::ofstream   g_fsMotionDebugInfo;
extern bool            g_bMotionDebugEnabled;
extern bool            g_bMotionTriggerEnabled;

namespace motion {

	typedef struct CortexFrame {
		sFrameOfData* pFrame;
		long long nTimestampReceived;

		CortexFrame() {
			nTimestampReceived = 0;
		}
	};

	vector<float> CortexToBoostVector(tMarkerData d);

	int Init(boost::thread* pThread);

	void Test();

	void SetLogEnabled(bool enabled);
	
	void RecordCortex(bool record);
	
	void BufferFrame(sFrameOfData* FrameOfData);

	// Load/Save .cap frame data
	void SaveFrames(int finalFrameIndex);
	void LoadFrames();

	void WatchFrameBuffer();
	void WatchFrameBufferSave();

	void ProcessFrame(CortexFrame *pCortexFrame);

	void GetMarkers();

	void EnableMotionTrigger(bool enabled);

	void Save(std::string prefix, float startTimeAgo, float endTimeAgo);

	void SaveFrameMsgPack(std::ofstream& o, CortexFrame *pCortexFrame);

}
