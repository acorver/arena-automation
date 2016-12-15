#pragma once

#include "motion.h"

namespace motion {
	namespace tracker_velocity {
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
			int lastTakeOffStartFrame;
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
				takeOffStartFrame = lastTakeOffStartFrame = -1;
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
	}

	void ProcessFrame_VelocityThreshold(CortexFrame *pCortexFrame);
}