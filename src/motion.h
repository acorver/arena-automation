#pragma once

#include "common.h"
#include "settings.h"
#include "log.h"

using namespace boost::numeric::ublas;

extern int nextBodyIndex;

namespace motion {

	struct PointCloud
	{
		std::vector<tMarkerData>  pts;
		inline size_t kdtree_get_point_count() const { return pts.size(); }
	};

	typedef struct PositionHistory {
		boost::numeric::ublas::vector<float> position;
		float velocity;
		float acceleration;
		int numMarkers;
		int iFrame;

		PositionHistory(int frame) {
			position = zero_vector<float>(3);
			velocity = 0.0f;
			acceleration = 0.0f;
			numMarkers = 0;
			iFrame = frame;
		}

	} PositionHistory;

	typedef struct Body { 
		
		int iBody;
		int takeOffStartFrame;
		vector<float> takeOffPosition;
		int numUpdates;
		std::string strName;

		std::deque<PositionHistory> positionHistory;

		// Statistics for take-off detection
		float avgTakeoffSpeed;
		float avgTakeoffAcceleration;
		float avgTakeoffMarkerNum;

		// Statistics for stationary detection
		float avgStationarySpeed;
		float avgStationaryAcceleration;
		float avgStationaryMarkerNum;

		Body() {
			takeOffStartFrame = -1;
			takeOffPosition = zero_vector<float>(3);
			iBody = nextBodyIndex;
			nextBodyIndex += 1;
			numUpdates = 0;
			strName = "";

			avgTakeoffSpeed = avgTakeoffAcceleration = avgTakeoffMarkerNum = 0.0f;
			avgStationarySpeed = avgStationaryAcceleration = avgStationaryMarkerNum = 0.0f;
		}

		bool IsVirtualMarker() {
			return !strName.empty();
		}

		void Update();

	} Body;

	int Init(boost::thread* pThread);

	void Test();

	void SetLogEnabled(bool enabled);

	void BufferFrame(sFrameOfData* FrameOfData);

	// Load/Save .cap frame data
	void SaveFrames(int finalFrameIndex);
	void LoadFrames();

	void WatchFrameBuffer();
	void WatchFrameBufferSave();

	void ProcessFrame(sFrameOfData* FrameOfData);

	void GetMarkers();

	void EnableMotionTrigger(bool enabled);

	void Save(std::string prefix, float startTimeAgo, float endTimeAgo);

	void SaveFrameMsgPack(std::ofstream& o, sFrameOfData* pF);
	void SaveFrameMsgPackExperimental(std::ofstream& o, sFrameOfData* pF);

}
