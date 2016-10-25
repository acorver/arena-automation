#include "stdafx.h"

// ============================================================================
// 
// Name: Motion.cpp
// Desc:
//       This file handles connection and processing of high-speed motion  
//       capture data.
//
// ============================================================================

#include "motion.h"
#include "settings.h"
#include "photron.h"
#include "hardware.h"

// Global variables
boost::lockfree::spsc_queue<sFrameOfData*, 
	boost::lockfree::capacity<1000000> > g_FrameBuffer, g_FrameBufferSave;

motion::PointCloud g_RecentPoints;

nanoflann::KDTreeSingleIndexAdaptor<
	nanoflann::L2_Simple_Adaptor<
		float, 
		motion::PointCloud >,
	motion::PointCloud,
	3 /* dim */
> g_RecentPointsIndex (
	3 /*dim*/, 
	g_RecentPoints,
	nanoflann::KDTreeSingleIndexAdaptorParams(10 /* max leaf */));

boost::atomic<int> g_nLastFrameIndex;

//std::deque<sFrameOfData>	bufferedFrames;
//sFrameOfData	bufferedFrames[settings::GetSetting("cortex.frame_buffer_size")];
//int				bufferedFramesStart;
//int				bufferedFramesEnd;
std::vector<motion::Body> bodies;

int				saveFramesMaxIndex = -1;
std::queue<sFrameOfData> saveBuffer;
int				nextBodyIndex = 0;

int				totalBufferedFrames = 0;

unsigned long lastSavedFrameID;
std::deque<sFrameOfData*> recentlySavedFrames;

bool g_bMotionTriggerEnabled = true;

std::ofstream g_fsMotionDebugInfo;
bool g_bMotionDebugEnabled = true;

// ----------------------------------------------
// Name:  init()
// Desc:  Initialize the motion system
// ----------------------------------------------

int motion::Init( boost::thread* pThread ) {
	
	int nResult = -1;
	sHostInfo hostInfo;

	// Set variables
	//bufferedFramesStart = bufferedFramesEnd = 0;
	g_nLastFrameIndex = -1;

	// Register the function that Cortex calls when new data arrives
	nResult = Cortex_SetDataHandlerFunc(motion::BufferFrame);

	if (nResult != RC_Okay) {
		logging::Log("[MOTION] Error in Cortex_SetDataHandlerFunc: %d", nResult);
		return 1;
	}

	// Enable sending Sky commands to Cortex
	Cortex_SetClientCommunicationEnabled(1);

	// Start Cortex interface and get information on the connection found
	nResult = Cortex_Initialize("10.101.30.47", "10.101.30.48", "225.1.1.1");
	//nResult = Cortex_Initialize("localhost", "localhost");
	nResult = Cortex_GetHostInfo(&hostInfo);

	int portTalkToHost, portHost, portHostMulticast, portTalkToClientsRequest, portTalktoClientsMulticast, portClientsMulticast;
	nResult = Cortex_GetPortNumbers(&portTalkToHost, &portHost, &portHostMulticast, &portTalkToClientsRequest, &portTalktoClientsMulticast, &portClientsMulticast);

	char szTalkToHostNicCardAddress[64];
	char szHostNicCardAddress[64];
	char szHostMulticastAddress[64];
	char szTalkToClientsNicCardAddress[64];
	char szClientsMulticastAddress[64];
		
	nResult = Cortex_GetAddresses(
		szTalkToHostNicCardAddress,
		szHostNicCardAddress,
		szHostMulticastAddress,
		szTalkToClientsNicCardAddress,
		szClientsMulticastAddress);

	if (nResult == RC_Okay && hostInfo.bFoundHost) {
		logging::Log("[MOTION] Cortex initialized successfully.");
		logging::Log("[MOTION] Cortex machine name: %s", hostInfo.szHostMachineName);
		logging::Log("[MOTION] {portTalkToHost, portHost, portHostMulticast, portTalkToClientsRequest, portTalktoClientsMulticast, portClientsMulticast} = {%d,%d,%d,%d,%d,%d}",
			portTalkToHost, portHost, portHostMulticast, portTalkToClientsRequest, portTalktoClientsMulticast, portClientsMulticast);
		logging::Log("[MOTION] {szTalkToHostNicCardAddress,szHostNicCardAddress,szHostMulticastAddress,szTalkToClientsNicCardAddress,szClientsMulticastAddress} = {%s,%s,%s,%s,%s}",
			szTalkToHostNicCardAddress, szHostNicCardAddress, szHostMulticastAddress, szTalkToClientsNicCardAddress, szClientsMulticastAddress);
	}
	else {
		logging::Log("[MOTION] Cortex failed to initialize.");
		return 1;
	}

	// Initialize debug info for evaluating motion tracking data in real time
	g_fsMotionDebugInfo = std::ofstream("data/motionlog.txt");

	// Initialize separate thread
	boost::thread t1(motion::WatchFrameBuffer);
	boost::thread t2(motion::WatchFrameBufferSave);

	// Done!
	return 0;
}

void motion::SetLogEnabled(bool enabled) {
	g_bMotionDebugEnabled = enabled;
}

void motion::EnableMotionTrigger(bool enabled) {
	g_bMotionTriggerEnabled = enabled;
}

// ----------------------------------------------
// Name:  Save Frames()
// Desc:  Save frames
// ----------------------------------------------

void motion::SaveFrames(int finalFrameIndex) {
	saveFramesMaxIndex = finalFrameIndex;
}

// ----------------------------------------------
// Name:  BufferFrame()
// Desc:  This function is called by Cortex when a new frame 
//        has arrived. In order not to block Cortex's smooth 
//        operations, analysis of the incoming motion data
//        is performed on a separate thread. See ProcessFrameBuffer().
// ----------------------------------------------

void motion::BufferFrame(sFrameOfData* frameOfData) {

	// For some reason Cortex is sending frames twice, so check if we've already processed this one
	if (g_nLastFrameIndex == frameOfData->iFrame) { return; }

	// Save copy of frame data
	sFrameOfData* pFrame = new sFrameOfData();
	memset(pFrame, 0, sizeof(sFrameOfData));
	Cortex_CopyFrame(frameOfData, pFrame);
	g_FrameBuffer.push(pFrame);
	
	// Save copy for the saving thread (TODO: Perhaps don't copy the frame twice?)
	pFrame = new sFrameOfData();
	memset(pFrame, 0, sizeof(sFrameOfData));
	Cortex_CopyFrame(frameOfData, pFrame);
	g_FrameBufferSave.push(pFrame);

	// Record the most recent frame index
	g_nLastFrameIndex = frameOfData->iFrame;
	
	// Log stats
	totalBufferedFrames += 1;
	if ((totalBufferedFrames % 1000) == 0) {
		// DEBUG:
		//logging::Log("[MOTION] Received %d frames from cortex.", totalBufferedFrames);
	}
}

// ----------------------------------------------
// Name:  WatchFrameBuffer()
// Desc:  This function is running continuously 
//        on a separate thread to watch for new 
//        incoming motion data
// Note:  Notice how WatchFrameBuffer only modifies 
//        the start of the frame, and only reads bufferFramesEnd
//        once. This way the Cortex interface and WatchFrameBuffer 
//        threads don't interfere.
// ----------------------------------------------

void motion::WatchFrameBuffer() {

	int iFrame = 0;

	sFrameOfData* pFrame = 0;

	while (true) {
		if (g_FrameBuffer.pop(pFrame)) {
			motion::ProcessFrame(pFrame);

			Cortex_FreeFrame(pFrame);
			delete pFrame;

			iFrame += 1;

			if ((iFrame % 1000) == 0) {
				logging::Log("Processing queue: %d", g_FrameBuffer.read_available());
			}
		}
		else {
			boost::this_thread::sleep_for(boost::chrono::milliseconds(1));
		}
	}
}

void motion::WatchFrameBufferSave() {

	int iFrame = 0;

	std::string file = common::GetTimeStr("./data/%Y-%m-%d %H-%M-%S_Cortex.msgpack");
	std::ofstream fo(file.c_str(), std::ofstream::binary);

	std::string file2 = common::GetTimeStr("./data/%Y-%m-%d %H-%M-%S_Cortex_experimental.msgpack");
	std::ofstream fo2(file.c_str(), std::ofstream::binary);

	sFrameOfData* pFrame = 0;

	while (true) {
		if (g_FrameBufferSave.pop(pFrame)) {

			motion::SaveFrameMsgPack(fo, pFrame);

			motion::SaveFrameMsgPackExperimental(fo2, pFrame);

			// SaveFrameMsgPack(Experimental) is now in charge of 
			// freeing memory.
			//Cortex_FreeFrame(pFrame);
			//delete pFrame;

			if (iFrame == 1000) { fo.flush(); iFrame = 0; }
			iFrame += 1;

			if ( (iFrame % 1000) == 0) {
				logging::Log("Saving queue: %d", g_FrameBufferSave.read_available());
			}
		}
		else {
			boost::this_thread::sleep_for(boost::chrono::milliseconds(1));
		}
	}
	fo.close();
	fo2.close();
}

// ----------------------------------------------
// Name:  ProcessFrameBuffer()
// Desc:  This function will decide if the dragonfly
//        has made a trajectory that warrants saving high-speed 
//        recordings.
//
//        we have to record:
//
//        1. the last moment (frame number?) the animal was not flying
//             a. based on altitude? (preferably not)
//             b. based on average speed over 10-50ms window
//
//        2. Whether that last position was in an appropriate location (e.g. perch)
//
//        3. the current acceleration to detect animal slowing down, and 
//           determine when to stop recording. 
//           This could be modified by / replaced by, a timeout or fixed 
//           recording window.
// ----------------------------------------------

vector<float> CortexToBoostVector(tMarkerData d) {
	vector<float> m(3);
	m[0] = d[0];
	m[1] = d[1];
	m[2] = d[2];
	return m;
}

void ComputeMotionProps(std::deque<motion::PositionHistory> &markers, int offset, 
	int windowSize, float *avgSpeed, float *avgAcceleration, float *avgMarkerNum) {
	
	int window = std::min(windowSize, int(markers.size()) - offset) - 1;

	*avgMarkerNum	 = 0.0f;
	*avgSpeed		 = 0.0f;
	//*avgAcceleration = 0.0f;

	// Only start computing when complete window data has been gathered to prevent statistical decisions 
	// based on little data // TODO: Have some kind of more meaningful function behavior...
	if (markers.size() < windowSize) { return; }

	for (int j = 0; j < window; j++) {
		
		*avgMarkerNum	 = *avgMarkerNum	+ markers[offset + j].numMarkers	/ float(window);
		*avgSpeed		 = *avgSpeed		+ markers[offset + j].velocity		/ float(window);
		//*avgAcceleration = *avgAcceleration + markers[offset + j].acceleration	/ float(window); // TODO: Why did this use to be window-1 ??
	}
}

void motion::Body::Update() {

	if (this->positionHistory.size() < _s<int>("tracking.takeoff_detection_window") ||
		this->positionHistory.size() < _s<int>("tracking.stationary_detection_window") || 
		this->positionHistory.size() < _s<int>("tracking.takeoff_detection_velocity_span")) { return; }

	PositionHistory* p = &(this->positionHistory.front());
	PositionHistory* pt = &(this->positionHistory[_s<int>("tracking.takeoff_detection_velocity_span") - 1]);
	p->velocity = norm_2(p->position - pt->position) * _s<int>("cortex.fps_analysis") / float(_s<int>("tracking.takeoff_detection_window"));
	PositionHistory* ps = &(this->positionHistory[_s<int>("tracking.stationary_detection_window")-1]);
	
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

	this->avgTakeoffSpeed        += p->velocity     / float(_s<int>("tracking.takeoff_detection_window"));
	//this->avgTakeoffAcceleration += p->acceleration / float(settings::GetSetting("tracking.takeoff_detection_window"));
	this->avgTakeoffMarkerNum    += p->numMarkers   / float(_s<int>("tracking.takeoff_detection_window"));

	//this->avgStationarySpeed        += p->velocity     / float(settings::GetSetting("tracking.stationary_detection_window"));
	//this->avgStationaryAcceleration += p->acceleration / float(settings::GetSetting("tracking.stationary_detection_window"));
	this->avgStationaryMarkerNum    += p->numMarkers   / float(_s<int>("tracking.stationary_detection_window"));

	// De-average frames that just moved out of the averaging window
	if (this->positionHistory.size() >= _s<int>("tracking.takeoff_detection_window")) {
		p = &(this->positionHistory[_s<int>("tracking.takeoff_detection_window")-1]);
	
		this->avgTakeoffSpeed        -= p->velocity     / float(_s<int>("tracking.takeoff_detection_window"));
		//this->avgTakeoffAcceleration -= p->acceleration / float(settings::GetSetting("tracking.takeoff_detection_window"));
		this->avgTakeoffMarkerNum    -= p->numMarkers   / float(_s<int>("tracking.takeoff_detection_window"));
	}

	if (this->positionHistory.size() >= _s<int>("tracking.stationary_detection_window")) {
		p = &(this->positionHistory[_s<int>("tracking.stationary_detection_window")-1]);

		//this->avgStationarySpeed        -= p->velocity     / float(settings::GetSetting("tracking.stationary_detection_window"));
		//this->avgStationaryAcceleration -= p->acceleration / float(settings::GetSetting("tracking.stationary_detection_window"));
		this->avgStationaryMarkerNum    -= p->numMarkers   / float(_s<int>("tracking.stationary_detection_window"));
	}

	this->avgStationarySpeed = this->avgTakeoffSpeed;

	// Remove points that are too old
	if (this->positionHistory.size() > _s<int>("tracking.max_body_tracking_history")) {
		this->positionHistory.pop_back();
	}
}

void motion::ProcessFrame(sFrameOfData* frameOfData) {

	// Variables
	std::vector<vector<float>> markers;
	motion::Body* pBody = 0;

	// Skip every other frame
	if ( (frameOfData->iFrame % 2) == 0) { return;  }

	//   o Add unlabeled markers
	if (_s<bool>("cortex.track_unidentified_markers")) {
		for (int i = 0; i < frameOfData->nUnidentifiedMarkers; i++) {
			markers.push_back(CortexToBoostVector(frameOfData->UnidentifiedMarkers[i]));
		}
	}

	//   o Add identified markers
	for (int i = 0; i < frameOfData->nBodies; i++) {

		if (frameOfData->BodyData[i].nMarkers == 0) { continue; }

		// Only process YFrame's 
		if (std::string(frameOfData->BodyData[i].szName).find(_s<std::string>("tracking.body_name")) == std::string::npos &&
			_s<std::string>("tracking.body_name") != "") {
			continue;
		}

		// Average position
		//vector<float> m = CortexToBoostVector(frameOfData->BodyData[i].Markers);

		vector<float> m = boost::numeric::ublas::zero_vector<float>(3);
		/*
		int valid = 0;
		for (int j = 0; j < frameOfData->BodyData[i].nMarkers; j++) {
			if (frameOfData->BodyData[i].Markers[j][0] == CORTEX_INVALID_MARKER) { continue; }
			m += CortexToBoostVector(frameOfData->BodyData[i].Markers[j]);
			valid++;
		}
		m /= valid;
		*/

		for (int j = 0; j < frameOfData->BodyData[i].nMarkers; j++) {
			if (frameOfData->BodyData[i].Markers[j][0] == CORTEX_INVALID_MARKER) { continue; }
			m = CortexToBoostVector(frameOfData->BodyData[i].Markers[j]);
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
			bodies.push_back(motion::Body());
			pBody = &bodies.back();
			pBody->strName = std::string(frameOfData->BodyData[i].szName);
		} else {
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

	//   o Identify dragonflies as objects (should have 3 (or maybe at least 2?) markers)
	//     Use a basic clustering logic: 
	//       - cluster all points that are within a certain radius of each other
	//   Note: this is not a very good algorithm, as it might cluster multiple objects 
	//         together leading to missing data frames for some bodies

	// ...

	// Attempt to assign each unidentified marker to an existing object based on proximity, etc.
	for (int j = 0; j < markers.size(); j++) {
		
		int closestBody = -1; int closestDistance = INT_MAX;

		for (int i = 0; i < bodies.size(); i++) {
			
			// Skip virtual markers
			if (bodies[i].IsVirtualMarker()) { continue; }

			int dist = INT_MAX;
			if (bodies[i].positionHistory.size() >= 1) { dist = norm_2( bodies[i].positionHistory.front().position - markers[j] ); }
		
			if (dist < closestDistance) {
				closestBody = i;
				closestDistance = dist;
			}

		}
		
		if ( closestDistance > _s<int>("tracking.max_body_tracking_dist") ) {
		
			// Create new tracked body
			bodies.push_back( motion::Body() );
			pBody = &bodies.back();

		} else {

			pBody = &(bodies[closestBody]);
		}

		// Append marker to body:
		if (pBody->positionHistory.size() == 0 || 
			pBody->positionHistory.front().iFrame !=
				frameOfData->iFrame) {
			pBody->positionHistory.push_front(PositionHistory(frameOfData->iFrame));
		}

		PositionHistory* pPosHist = &(pBody->positionHistory.front());
			
		// Re-average position based on new marker
		pPosHist->position = (pPosHist->position * pPosHist->numMarkers + markers[j]) / 
			( pPosHist->numMarkers + 1 );
		pPosHist->numMarkers += 1;

		// Write each marker and the body it was assigned to
		if (g_bMotionDebugEnabled) {
			g_fsMotionDebugInfo << "marker" << frameOfData->iFrame << "," << pBody->iBody << "," << markers[j][0] << "," << markers[j][1] << "," << markers[j][2] << "\n";
		}
	}

	/*
	// Compute motion properties
	for (int i = 0; i < bodies.size(); i++) {
		PositionHistory* pPosHist = &(bodies[i].positionHistory.front());

		// Compute velocity
		if (bodies[i].positionHistory.size() >= 2) {
			pPosHist->velocity = norm_2(pPosHist->position - bodies[i].positionHistory[1].position) * settings::GetSettings<int>("cortex.fps_analysis");
		}

		// Compute acceleration
		if (bodies[i].positionHistory.size() >= 3) {
			pPosHist->acceleration = (pPosHist->velocity - bodies[i].positionHistory[1].velocity) * settings::GetSettings<int>("cortex.fps_analysis");
		}
	}
	*/

	for (int i = 0; i < bodies.size(); i++) {
		
		// Delete bodies that haven't been extended with new marker frames for a while
		if ( frameOfData->iFrame - bodies[i].positionHistory.front().iFrame > _s<int>("tracking.max_body_tracking_gap") && 
				bodies[i].takeOffStartFrame == -1) {
			bodies.erase( bodies.begin() + i );
			i -= 1;
		} else {
			// Otherwise update the tracking info associated with the body
			bodies[i].Update();
		}
	}

	// Determine if take-off occured over particular time window
	for (int i = 0; i < bodies.size(); i++) {

		// Detect take-off based on peak motion velocity
		if (bodies[i].avgTakeoffSpeed > _s<float>("tracking.takeoff_speed_threshold") && bodies[i].takeOffStartFrame == -1) {
			if (bodies[i].avgTakeoffMarkerNum > _s<float>("tracking.dragonfly_marker_minimum")) {
				// Detect the actual takeoff based on (STRIKE)near-zero acceleration and(/STRIKE) near-zero speed
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
			} else {
				logging::Log("[MOTION] Would've detected takeoff, but body doesn't have the right number of markers to be a dragonfly...");
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
			} else {
				takeOffStartFrame = std::min(takeOffStartFrame, bodies[i].takeOffStartFrame);
			}

			// TODO: Add MarkerNum criteria here?? Probably not?
			if ((bodies[i].avgStationarySpeed > _s<float>("tracking.stationary_speed_threshold") /*||
				bodies[i].avgStationaryAcceleration > settings::GetSetting("tracking.stationary_acceleration_threshold")*/)
				&& (frameOfData->iFrame - bodies[i].takeOffStartFrame < _s<int>("tracking.landing_timeout"))) {
				allStationary = false;
			} else {
				if (frameOfData->iFrame - bodies[i].takeOffStartFrame > _s<int>("tracking.landing_timeout") && 
					bodies[i].avgStationarySpeed > _s<float>("tracking.stationary_speed_threshold")) {
					logging::Log("[MOTION] Forced detection of landing as recording duration maximum (%d frames) was reached.", _s<int>("tracking.landing_timeout"));
				}
				// Log data
				if (g_bMotionDebugEnabled) {
					g_fsMotionDebugInfo << "landing," << frameOfData->iFrame << "," << bodies[i].iBody << "," << frameOfData->iFrame - _s<int>("tracking.stationary_detection_window") << "\n";
				}
			}
		}
	}

	if (allStationary && takeOffStartFrame!=-1) {

		int takeOffEndFrame = frameOfData->iFrame - _s<int>("tracking.stationary_detection_window");

		// Retrieve recording from camera
		float startTimeAgo = (frameOfData->iFrame - takeOffStartFrame) / float(_s<int>("cortex.fps"));
		float endTimeAgo = (frameOfData->iFrame - takeOffEndFrame) / float(_s<int>("cortex.fps"));

		// Always log basic information
		logging::Log("[D] Takeoff window [-%f,-%f] with respect to now (Cortex frame %d).", startTimeAgo, endTimeAgo, frameOfData->iFrame);

		// Save data from this frame
		if (g_bMotionTriggerEnabled) {
			common::Save(startTimeAgo, endTimeAgo);
		} else {
			logging::Log("[MOTION] Not saving takeoff as motion triggering is currently disabled.");
		}

		if (g_bMotionDebugEnabled) {
			g_fsMotionDebugInfo << "alllanded," << frameOfData->iFrame << "\n";
		}

		// Reset all
		for (int i = 0; i < bodies.size(); i++) {
			bodies[i].takeOffStartFrame = -1;
		}
	}
}

// ----------------------------------------------
// Name:  GetMarkers()
// Desc:  Return the cached trajectory markers (and corresponding bodies) in the specified range
//        (e.g. for online display). FPS parameter can be used for downsampling
// ----------------------------------------------

void motion::GetMarkers() { //int start, int number, int fps

}


// ----------------------------------------------
// Serialize
// ----------------------------------------------

std::vector<std::vector<float>> _MsgPack_UnidentifiedMarkers(sFrameOfData* pF) {
	std::vector<std::vector<float>> markers;
	
	// Skip unidentified markers if they're too numerous (i.e. noisy)
	// Prevents overflowing harddrive accidentally
	if (pF->nUnidentifiedMarkers < 20) {
		for (int i = 0; i < pF->nUnidentifiedMarkers; i++) {
			markers.push_back(std::vector<float>(
				pF->UnidentifiedMarkers[i], pF->UnidentifiedMarkers[i] + 3));
		}
	}

	return markers;
}

std::vector<
	msgpack::type::tuple<
		std::string,				// szName
		std::vector<				// Markers
			std::vector<float>>>	// x,y,z
		> _MsgPack_Bodies(sFrameOfData* pF) {

	std::vector<
		msgpack::type::tuple<
		std::string,				     // szName
		std::vector<std::vector<float>>> // x,y,z
		> bodies;

	for (int i = 0; i < pF->nBodies; i++) {

		std::vector<std::vector<float>> markers;
		for (int j = 0; j < pF->BodyData[i].nMarkers; j++) {
			markers.push_back(std::vector<float> (
				pF->BodyData[i].Markers[j], pF->BodyData[i].Markers[j]+3));
		}

		bodies.push_back(
			msgpack::type::tuple<
			std::string, std::vector<std::vector<float>>>(
				pF->BodyData[i].szName,
				markers));
	}

	return bodies;
}

std::vector<int> _MsgPack_TimeCode(sFrameOfData* pF) {
	std::vector<int> t;
	t.push_back(pF->TimeCode.iStandard);   //!< 0=None, 1=SMPTE, 2=FILM, 3=EBU, 4=SystemClock
	t.push_back(pF->TimeCode.iHours);
	t.push_back(pF->TimeCode.iMinutes);
	t.push_back(pF->TimeCode.iSeconds);
	t.push_back(pF->TimeCode.iFrames);
	return t;
}

void motion::SaveFrameMsgPackExperimental(std::ofstream& o, sFrameOfData* pF) {

	Cortex_FreeFrame(pF);
	delete pF;

	/*
	// Variables
	bool                           different    = false;

	const size_t                   num_results  = 1;
	std::vector<size_t>            ret_index    (num_results);
	std::vector<nanoflann::num_t>  out_dist_sqr (num_results);

	// See if any identified markers have new positions from any existing point
	for (int i = 0; i < pF->nBodies; i++) {
		for (int j = 0; j < pF->BodyData[i].nMarkers; j++) {
			
			g_RecentPointsIndex.knnSearch(
				&(pF->BodyData[i].Markers[j]),
				num_results, 
				&ret_index[0], 
				&out_dist_sqr[0]);
			
			if (out_dist_sqr[0] > 1.0f) {
				different = true;
				goto Next;
			}
		}
	}

	// Skip unidentified markers if they're too numerous (i.e. noisy)
	// Prevents overflowing harddrive accidentally
	if (pF->nUnidentifiedMarkers < 20) {

		g_RecentPointsIndex.knnSearch(
			&(pF->UnidentifiedMarkers[j]),
			num_results,
			&ret_index[0],
			&out_dist_sqr[0]);

		if (out_dist_sqr[0] > 1.0f) {
			different = true;
			goto Next;
		}
	}

Next:
	// Delete points from frames that are too old
	
	// Now delete the old frame itself from the queueu

	// Add this new frame to the list of frames
	recentlySavedFrames.push_back(pF);

	// Add this frame's points to the recent points cloud
	g_RecentPoints.

	// Rebuild index
	index.buildIndex();

	msgpack::type::tuple<
			int,								// iFrame
			float,								// fDelay
			std::vector<						// BodyData
			msgpack::type::tuple<
			std::string,				// szName
			std::vector<				// Markers
			std::vector<float>>		// x,y,z
			>
			>,
			std::vector<					// UnidentifiedMarkers
			std::vector<float>			// x,y,z
			>,
			std::vector<int>				// TimeCode: 
			> d(
				pF->iFrame,
				pF->fDelay,
				_MsgPack_Bodies(pF),
				_MsgPack_UnidentifiedMarkers(pF),
				_MsgPack_TimeCode(pF)
			);
	msgpack::pack(o, d);
	*/
}

void motion::SaveFrameMsgPack(std::ofstream& o, sFrameOfData* pF) {

	msgpack::type::tuple<
		int,								// iFrame
		float,								// fDelay
		std::vector<						// BodyData
		msgpack::type::tuple<
		std::string,				// szName
		std::vector<				// Markers
		std::vector<float>>		// x,y,z
		>
		>,
		std::vector<					// UnidentifiedMarkers
		std::vector<float>			// x,y,z
		>,
		std::vector<int>				// TimeCode: 
		> d(
			pF->iFrame,
			pF->fDelay,
			_MsgPack_Bodies(pF),
			_MsgPack_UnidentifiedMarkers(pF),
			_MsgPack_TimeCode(pF)
		);
	msgpack::pack(o, d);
}


unsigned long numStillFrames = 0;

/*
void motion::SaveFrameMsgPack(std::ofstream& o, sFrameOfData* pF) {

	// Find the last frame similar enough to this one (-1 indicates save this as a new frame)
	long iSimilarFrame = -1;

	std::pair<int, int> key = std::make_pair(pF->nBodies, pF->nUnidentifiedMarkers);

	// Ensure key exists in database
	if (recentlySavedFrames.find(key) == recentlySavedFrames.end()) {
		recentlySavedFrames[key] = std::deque<sFrameOfData>();
	}

	std::deque<sFrameOfData>* pRFs = &(recentlySavedFrames[key]);

	// Compare this frame with recently saved frames
	for (int f = 0; f < pRFs->size(); f++) {

		bool different = false;

		for (int i = 0; i < pF->nBodies; i++) {
			for (int j = 0; j < pF->BodyData[i].nMarkers; j++) {
				if (norm_2(CortexToBoostVector(pF->BodyData[i].Markers[j]) -
						    CortexToBoostVector(pRFs->operator[](f).BodyData[i].Markers[j])) > 6.0f) {
					different = true; goto Next;
				}
			}
		}

		// Skip unidentified markers if they're too numerous (i.e. noisy)
		// Prevents overflowing harddrive accidentally
		if (pF->nUnidentifiedMarkers < 20) {

			for (int j = 0; j < pF->nUnidentifiedMarkers; j++) {
				if (norm_2(CortexToBoostVector(pF->UnidentifiedMarkers[j]) -
						   CortexToBoostVector(pRFs->operator[](f).UnidentifiedMarkers[j])) > 6.0f) {
					different = true; goto Next;
				}
			}
		}
			
	Next:
		if (!different) {
			iSimilarFrame = pRFs->operator[](f).iFrame; break; 			
		}
	}
	
	if (iSimilarFrame != -1) {
		numStillFrames++;
	} else {
		numStillFrames = 0;
	}

	if (iSimilarFrame == -1 || numStillFrames < 100) {
		if (iSimilarFrame == -1) {
			lastSavedFrameID = pF->iFrame;
		}

		sFrameOfData frame = sFrameOfData();
		recentlySavedFrames[key].push_front(frame);
		Cortex_CopyFrame(pF, &(recentlySavedFrames[key].front()));
		
		if (recentlySavedFrames[key].size() > 10 ) {
			Cortex_FreeFrame(&recentlySavedFrames[key].back());
			recentlySavedFrames[key].pop_back();
		}
	
		if (iSimilarFrame != lastSavedFrameID) {
			msgpack::type::tuple<
				int,								// iFrame
				float,								// fDelay
				std::vector<						// BodyData
					msgpack::type::tuple<
						std::string,				// szName
						std::vector<				// Markers
							std::vector<float>>		// x,y,z
					>
				>,
				std::vector<					// UnidentifiedMarkers
					std::vector<float>			// x,y,z
				>,
				std::vector<int>				// TimeCode: 
			> d(
				pF->iFrame,
				pF->fDelay,
				_MsgPack_Bodies(pF),
				_MsgPack_UnidentifiedMarkers(pF),
				_MsgPack_TimeCode(pF)
			);
			msgpack::pack(o, d);
		} else {
			msgpack::pack(o, lastSavedFrameID);
		}
	} else {
		if (iSimilarFrame != lastSavedFrameID) {
			lastSavedFrameID = iSimilarFrame;
			msgpack::pack(o, lastSavedFrameID);
		}
	}
}
*/

void motion::Save(std::string prefix, float startTimeAgo, float endTimeAgo) {

	// TODO! Save frames in saveBuffer, to make sure they're not discarded/overwritten!!!!!!!!!!!

	logging::Log("[MOTION] Saving Cortex data.");

	std::ofstream fo( prefix + "_cortex.msgpack", std::ofstream::binary);
	
	msgpack::pack(fo, int(g_nLastFrameIndex));

    fo.close();

	logging::Log("[MOTION] Finished saving Cortex data.");
}