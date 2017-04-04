#include "stdafx.h"

/*
* This script implements a particular motion triggering algorithm based 
* on simple detection of a height threshold
*
* Author: Abel Corver, December 2016
*         abel.corver@gmail.com
*         (Anthony Leonardo Lab)
*
*/

// Includes
#include "motiontracker_z.h"
#include "settings.h"

// ==================================================================
// See if any Yframes are more than 4 cm above the platform 
// ==================================================================

void motion::ProcessFrame_Z(motion::CortexFrame *pCortexFrame) {

	sFrameOfData* frameOfData = pCortexFrame->pFrame;
	
	for (int i = 0; i < frameOfData->nBodies; i++) {

		// Only process YFrame's 
		if (std::string(frameOfData->BodyData[i].szName).find(_s<std::string>("tracking.body_name")) == std::string::npos &&
			_s<std::string>("tracking.body_name") != "") {
			continue;
		}

		if (frameOfData->BodyData[i].nMarkers == 0) { continue; }

		for (int j = 0; j < 3; j++) {
			if (frameOfData->BodyData[i].Markers[j][0] != CORTEX_INVALID_MARKER) {
				if (frameOfData->BodyData[i].Markers[j][2] > _s<float>("tracking.tracker_z.threshold")) {
					
					if (g_bMotionTriggerEnabled) {
						common::Trigger(0.0f, 0.0f, false);
					}
					else {
						logging::Log("[MOTION] Not saving takeoff as motion triggering is currently disabled.");
					}
				}
			}
		}
	}
}