#include "stdafx.h"

/*
* This script implements a particular motion triggering algorithm.
*
* Author: Abel Corver, December 2016
*         abel.corver@gmail.com
*         (Anthony Leonardo Lab)
*
*/

// Includes
#include "motiontracker_velocity.h"
#include "settings.h"

// Include definitions for this tracker type in main namespace
using namespace motion::tracker_velocity;

// ==================================================================
// Global variables 
// ==================================================================

std::vector<Body>    bodies;
int                  nextBodyIndex = 0;
int                  g_nLastTakeoffFrame = -1;
unsigned long        g_nPendingDataSaveTriggerFrame = -1;

void ComputeMotionProps(std::deque<PositionHistory> &markers, int offset,
	int windowSize, float *avgSpeed, float *avgAcceleration, float *avgMarkerNum) {

	int window = std::min(windowSize, int(markers.size()) - offset) - 1;

	*avgMarkerNum = 0.0f;
	*avgSpeed = 0.0f;

	// Only start computing when complete window data has been gathered to prevent statistical decisions 
	// based on little data // TODO: Have some kind of more meaningful function behavior...
	if (markers.size() < windowSize) { return; }

	for (int j = 0; j < window; j++) {

		*avgMarkerNum = *avgMarkerNum + markers[offset + j].numMarkers / float(window);
		*avgSpeed = *avgSpeed + markers[offset + j].velocity / float(window);
	}
}

void Body::Update() {

	if (this->positionHistory.size() < _s<int>("tracking.takeoff_detection_window") ||
		this->positionHistory.size() < _s<int>("tracking.stationary_detection_window") ||
		this->positionHistory.size() < _s<int>("tracking.takeoff_detection_velocity_span")) {
		return;
	}

	PositionHistory* p = &(this->positionHistory.front());
	PositionHistory* pt = &(this->positionHistory[_s<int>("tracking.takeoff_detection_velocity_span") - 1]);
	
	// Use only Z for velocity computation
	p->velocity = (p->position[2] - pt->position[2]) * _s<int>("cortex.fps_analysis") / float(_s<int>("tracking.takeoff_detection_window"));
	//p->velocity = norm_2(p->position - pt->position) * _s<int>("cortex.fps_analysis") / float(_s<int>("tracking.takeoff_detection_window"));
	
	PositionHistory* ps = &(this->positionHistory[_s<int>("tracking.stationary_detection_window") - 1]);

	// Occasionally, we fully recompute the averages to prevent accumulated floating precision errors
	// from biasing our values
	numUpdates++;
	if (numUpdates >= 0 || this->positionHistory.size() == _s<int>("tracking.takeoff_detection_window") ||
		this->positionHistory.size() == _s<int>("tracking.stationary_detection_window")) {
		numUpdates = 0;

		// Compute takeoff statistics
		ComputeMotionProps(this->positionHistory, 0, _s<int>("tracking.takeoff_detection_window"),
			&this->avgTakeoffSpeed, &this->avgTakeoffAcceleration, &this->avgTakeoffMarkerNum);

		// Compute stationary statistics
		ComputeMotionProps(this->positionHistory, 0, _s<int>("tracking.stationary_detection_window"),
			&this->avgStationarySpeed, &this->avgStationaryAcceleration, &this->avgStationaryMarkerNum);

		return;
	}

	// Average the newly added frame (NOTE: Currently the various variables (speed, etc.) are 
	// not valid until the *_DETECTION_WINDOW buffer size is reached, as we divide by a constant
	// window size rather than actual buffer. Potential TODO.)
	p = &(this->positionHistory.front());

	this->avgTakeoffSpeed += p->velocity / float(_s<int>("tracking.takeoff_detection_window"));
	this->avgTakeoffMarkerNum += p->numMarkers / float(_s<int>("tracking.takeoff_detection_window"));

	this->avgStationarySpeed += p->velocity / float(_s<int>("tracking.stationary_detection_window"));
	this->avgStationaryMarkerNum += p->numMarkers / float(_s<int>("tracking.stationary_detection_window"));

	// De-average frames that just moved out of the averaging window
	if (this->positionHistory.size() >= _s<int>("tracking.takeoff_detection_window")) {

		p = &(this->positionHistory[_s<int>("tracking.takeoff_detection_window") - 1]);

		this->avgTakeoffSpeed -= p->velocity / float(_s<int>("tracking.takeoff_detection_window"));
		this->avgTakeoffMarkerNum -= p->numMarkers / float(_s<int>("tracking.takeoff_detection_window"));
	}

	if (this->positionHistory.size() >= _s<int>("tracking.stationary_detection_window")) {

		p = &(this->positionHistory[_s<int>("tracking.stationary_detection_window") - 1]);

		this->avgStationaryMarkerNum -= p->numMarkers / float(_s<int>("tracking.stationary_detection_window"));
	}

	// Remove points that are too old
	if (this->positionHistory.size() > _s<int>("tracking.max_body_tracking_history")) {
		this->positionHistory.pop_back();
	}
}

void motion::ProcessFrame_VelocityThreshold(CortexFrame *pCortexFrame) {

	sFrameOfData* frameOfData = pCortexFrame->pFrame;

	// Variables
	std::vector<vector<float>> markers;
	Body* pBody = 0;

	// Skip every other frame
	// if ((frameOfData->iFrame % 2) == 0) { return; }

	//   o Add identified markers
	for (int i = 0; i < frameOfData->nBodies; i++) {

		if (frameOfData->BodyData[i].nMarkers == 0) { continue; }

		// Only process YFrame's 
		if (std::string(frameOfData->BodyData[i].szName).find(_s<std::string>("tracking.body_name")) == std::string::npos &&
			_s<std::string>("tracking.body_name") != "") {
			continue;
		}

		// First try center marker
		vector<float> m = CortexToBoostVector(frameOfData->BodyData[i].Markers[1]);
		if (m[0] == CORTEX_INVALID_MARKER) {
			m = CortexToBoostVector(frameOfData->BodyData[i].Markers[2]);
		}
		if (m[0] == CORTEX_INVALID_MARKER) {
			continue;
		}

		// Find the closest body
		int closestBody = -1; int closestDistance = INT_MAX;
		for (int j = 0; j < bodies.size(); j++) {

			// Only match against bodies with same name
			if (bodies[j].strName != std::string(frameOfData->BodyData[i].szName)) { continue; }

			int dist = INT_MAX;
			if (bodies[j].positionHistory.size() >= 1) {
				dist = norm_2(bodies[j].positionHistory.front().position - m);
			}

			if (dist < closestDistance) {
				closestBody = j;
				closestDistance = dist;
			}

		}

		if (closestDistance > _s<int>("tracking.max_body_tracking_dist")) {
			// Create new tracked body
			bodies.push_back(Body());
			pBody = &bodies.back();
			pBody->strName = std::string(frameOfData->BodyData[i].szName);
		}
		else {
			pBody = &(bodies[closestBody]);
		}

		// Append marker to body:
		PositionHistory p(frameOfData->iFrame);
		p.position = m;
		p.numMarkers = frameOfData->BodyData[i].nMarkers;
		pBody->positionHistory.push_front(p);

		// Write each marker and the body it was assigned to
		if (g_bMotionDebugEnabled) {
			g_fsMotionDebugInfo << "marker," << frameOfData->iFrame << "," << pBody->iBody << "," << m[0] << "," << m[1] << "," << m[2] << "\n";
		}
	}

	for (int i = 0; i < bodies.size(); i++) {

		// Delete bodies that haven't been extended with new marker frames for a while (except when we have a pending save and want to prevent 
		// the relevant dragonfly body from being deleted during the evaluation window).
		if (frameOfData->iFrame - bodies[i].positionHistory.front().iFrame > _s<int>("tracking.max_body_tracking_gap") &&
			bodies[i].takeOffStartFrame == -1 && g_nPendingDataSaveTriggerFrame == -1) {

			bodies.erase(bodies.begin() + i);
			i -= 1;
		}
		else {
			// Otherwise update the tracking info associated with the body
			bodies[i].Update();
		}
	}

	// Determine if take-off occured over particular time window
	for (int i = 0; i < bodies.size(); i++) {

		// Takeoffs only allowed for this body a certain frames after the last recording
		if (frameOfData->iFrame - g_nLastTakeoffFrame >= _s<int>("tracking.min_takeoff_cooldown")) {
			// Detect take-off based on peak motion velocity
			if (bodies[i].avgTakeoffSpeed > _s<float>("tracking.takeoff_speed_threshold") && bodies[i].takeOffStartFrame == -1) {
				if (bodies[i].avgTakeoffMarkerNum > _s<float>("tracking.dragonfly_marker_minimum")) {
					// Detect the actual takeoff based on near-zero speed
					int iTakeOff;
					float stationaryAvgA, stationaryAvgS, stationaryAvgM;

					for (iTakeOff = 0; iTakeOff < (_s<int>("tracking.max_body_tracking_history") - _s<int>("tracking.stationary_detection_window")); iTakeOff++) {

						ComputeMotionProps(bodies[i].positionHistory, iTakeOff, _s<int>("tracking.stationary_detection_window"),
							&stationaryAvgS, &stationaryAvgA, &stationaryAvgM);

						if (/*stationaryAvgA < settings::GetSetting("tracking.stationary_acceleration_threshold") && */
							stationaryAvgS < _s<float>("tracking.stationary_speed_threshold")) {

							break; // Landing detected!
						}
					}

					// Store take-off frame
					if (iTakeOff < bodies[i].positionHistory.size()) {
						bodies[i].takeOffStartFrame = bodies[i].positionHistory[iTakeOff].iFrame;
					}
					else {
						bodies[i].takeOffStartFrame = bodies[i].positionHistory.back().iFrame;
					}

					// Log data
					logging::Log("[MOTION] Detected takeoff with peak velocity %f, marker num %f", bodies[i].avgTakeoffSpeed, bodies[i].avgTakeoffMarkerNum);
					if (g_bMotionDebugEnabled) {
						g_fsMotionDebugInfo << "takeoff," << frameOfData->iFrame << "," << bodies[i].iBody << "," << bodies[i].takeOffStartFrame << "\n";
					}
				}
				else {
					logging::Log("[MOTION] Would've detected takeoff, but body doesn't have the right number of markers to be a dragonfly...");
				}
			}
		}

		// Log info
		if (g_bMotionDebugEnabled) {
			g_fsMotionDebugInfo << "body," <<
				frameOfData->iFrame << "," << bodies[i].iBody << "," <<
				bodies[i].positionHistory[0].position[0] << "," <<
				bodies[i].positionHistory[0].position[1] << "," <<
				bodies[i].positionHistory[0].position[2] << "," <<
				bodies[i].avgTakeoffSpeed << "," << bodies[i].avgTakeoffAcceleration << "," << bodies[i].avgTakeoffMarkerNum <<
				bodies[i].avgStationarySpeed << "," << bodies[i].avgStationaryAcceleration << "," << bodies[i].avgStationaryMarkerNum << "\n";
		}
	}

	// Detect if dragonfly is stationary
	bool allStationary = true;
	int takeOffStartFrame = -1;
	for (int i = 0; i < bodies.size(); i++) {
		if (bodies[i].takeOffStartFrame >= 0) {
			if (takeOffStartFrame == -1) {
				takeOffStartFrame = bodies[i].takeOffStartFrame;
			}
			else {
				takeOffStartFrame = std::min(takeOffStartFrame, bodies[i].takeOffStartFrame);
			}

			if (bodies[i].avgStationarySpeed > _s<float>("tracking.stationary_speed_threshold")
				&& (frameOfData->iFrame - bodies[i].takeOffStartFrame < _s<int>("tracking.landing_timeout"))) {
				allStationary = false;
			}
			else {
				if (frameOfData->iFrame - bodies[i].takeOffStartFrame > _s<int>("tracking.landing_timeout") &&
					bodies[i].avgStationarySpeed > _s<float>("tracking.stationary_speed_threshold")) {

					logging::Log("[MOTION] Forced detection of landing as recording duration maximum (%d frames) was reached.", _s<int>("tracking.landing_timeout"));
					allStationary = true;
				}
				// Log data
				if (g_bMotionDebugEnabled) {
					g_fsMotionDebugInfo << "landing," << frameOfData->iFrame << "," << bodies[i].iBody << "," << frameOfData->iFrame - _s<int>("tracking.stationary_detection_window") << "\n";
				}
			}
		}
	}

	// Process pending data save
	if (g_nPendingDataSaveTriggerFrame != -1 && frameOfData->iFrame - g_nPendingDataSaveTriggerFrame > _s<int>("tracking.pending_save_num_evaluation_frames")) {

		// Evaluate all body trajectories, and see if anything warrants a save
		bool saveToDisk = true;

		for (int i = 0; i < bodies.size(); i++) {

			// Whether to save pending data (i.e. high speed footage, and in future potentially other things) to disk or not
			bool saveBodyToDisk = true;

			// Compute total Z delta
			float minz = 1e9, maxz = -1e9;

			for (int j = 0; j < bodies[i].positionHistory.size(); j++) {
				float z = bodies[i].positionHistory[j].position[2];
				minz = std::min(minz, z);
				maxz = std::max(maxz, z);
			}

			// Trigger if this is appropriately large
			if (maxz - minz < _s<float>("tracking.pending_save_min_z_distance")) {
				saveBodyToDisk = false;
			}

			if (!saveBodyToDisk) {
				saveToDisk = false;
			}
		}

		// Process pending data
		common::SaveToDisk(saveToDisk);

		// Reset
		g_nPendingDataSaveTriggerFrame = -1;

	// Execute new trigger? :
	} else if (allStationary && takeOffStartFrame != -1) {

		int takeOffEndFrame = frameOfData->iFrame - _s<int>("tracking.stationary_detection_window");

		// Retrieve recording from camera
		float startTimeAgo = (frameOfData->iFrame - takeOffStartFrame) / float(_s<int>("cortex.fps"));
		float endTimeAgo = (frameOfData->iFrame - takeOffEndFrame) / float(_s<int>("cortex.fps"));

		// Always log basic information
		logging::Log("[D] Takeoff window [-%f,-%f] with respect to now (Cortex frame %d).", startTimeAgo, endTimeAgo, frameOfData->iFrame);

		// Reset all
		for (int i = 0; i < bodies.size(); i++) {
			bodies[i].lastTakeOffStartFrame = frameOfData->iFrame; // bodies[i].takeOffStartFrame;
			g_nLastTakeoffFrame = std::max(g_nLastTakeoffFrame, bodies[i].lastTakeOffStartFrame);
			bodies[i].takeOffStartFrame = -1;
		}

		// Save data from this frame
		if (g_bMotionTriggerEnabled) {

			common::Trigger(startTimeAgo, endTimeAgo);

			// Indicate pending data save (if enabled)
			if (_s<bool>("tracking.enable_pending_save")) {
				g_nPendingDataSaveTriggerFrame = frameOfData->iFrame;
			}
		}
		else {
			logging::Log("[MOTION] Not saving takeoff as motion triggering is currently disabled.");
		}

		if (g_bMotionDebugEnabled) {
			g_fsMotionDebugInfo << "alllanded," << frameOfData->iFrame << "\n";
		}
	}
}
