#include "stdafx.h"

// TODO: 
//    o Use acceleration threshold: one marker, center derivative
//    o COmpute Z range based on history, then do threshold e.g. 1 mm above that
//    o Make it easier to plot internal velocity/takeoff stats
//    o 

// ============================================================================
// Name: Motion.cpp
// Desc: This file handles connection and processing of high-speed motion  
//       capture data.
// ============================================================================

#include "motion.h"
#include "settings.h"
#include "photron.h"
#include "hardware.h"
#include "motiontracker_velocity.h"
#include "motiontracker_z.h"
#include "rotatingperch.h"

// Global variables
boost::lockfree::spsc_queue<motion::CortexFrame*,
	boost::lockfree::capacity<1000000> > 
	                         g_FrameBuffer, 
	                         g_FrameBufferSave,
							 g_FrameBufferMisc;

boost::atomic<unsigned long> g_FrameBufferSize, g_FrameBufferSaveSize, g_FrameBufferMiscSize;

boost::atomic<int>           g_nLastFrameIndex;
int                          saveFramesMaxIndex = -1;
std::queue<sFrameOfData>     saveBuffer;
int                          totalBufferedFrames = 0;
unsigned long                lastSavedFrameID;
std::deque<sFrameOfData*>    recentlySavedFrames;
unsigned int                 g_nFramesSinceRawCortexCameraSave;
boost::mutex				 g_RawCortexCameraSaveLock;
bool                         g_bCortexRecording;

// Initialize global variables
std::ofstream g_fsMotionDebugInfo;
bool g_bMotionDebugEnabled = true;
bool g_bMotionTriggerEnabled = true;

// Helper function
vector<float> motion::CortexToBoostVector(tMarkerData d) {
	vector<float> m(3);
	m[0] = d[0];
	m[1] = d[1];
	m[2] = d[2];
	return m;
}

// ----------------------------------------------
// Name:  init()
// Desc:  Initialize the motion system
// ----------------------------------------------

int motion::Init( boost::thread* pThread ) {
    
    int nResult = -1;
    sHostInfo hostInfo;

    // Set variables
    g_nLastFrameIndex = -1;
	g_bCortexRecording = false;

	g_FrameBufferSize = g_FrameBufferSaveSize = g_FrameBufferMiscSize = 0;

    // Register the function that Cortex calls when new data arrives
    nResult = Cortex_SetDataHandlerFunc(motion::BufferFrame);

    if (nResult != RC_Okay) {
        logging::Log("[MOTION] Error in Cortex_SetDataHandlerFunc: %d", nResult);
        return 1;
    }

    // Enable sending Sky commands to Cortex
    Cortex_SetClientCommunicationEnabled(1);

    // Start Cortex interface and get information on the connection found
	//nResult = Cortex_Initialize("10.101.30.47", "10.101.30.48", "225.1.1.1");
	nResult = Cortex_Initialize(
		strdup(_s<std::string>("cortex.szTalkToHostNicCardAddress").c_str()),
		strdup(_s<std::string>("cortex.szHostNicCardAddress").c_str()),
		strdup(_s<std::string>("cortex.szHostMulticastAddress").c_str()));
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

	// Start recording raw Cortex data
	motion::RecordCortex();

    // Initialize debug info for evaluating motion tracking data in real time
    g_fsMotionDebugInfo = std::ofstream( (common::GetCommonOutputPrefix()+".motionlog") );

    // Initialize separate threads
    boost::thread t1(motion::WatchFrameBuffer);
	boost::thread t2(motion::WatchFrameBufferSave);
	boost::thread t3(motion::WatchFrameBufferMisc);
	boost::thread t4(motion::MonitorStatus);
	boost::thread t5(motion::ContinuouslyRecordCortex);

    // Done!
    return 0;
}

void motion::SetLogEnabled(bool enabled) {
    g_bMotionDebugEnabled = enabled;
}

void motion::EnableMotionTrigger(bool enabled) {
    g_bMotionTriggerEnabled = enabled;
}

void motion::MonitorStatus() {
	while (true) {

		std::string msg =
			std::string("FrameBufferSize = ")     + std::to_string(g_FrameBufferSize    ) + std::string(", ") +
			std::string("FrameBufferSaveSize = ") + std::to_string(g_FrameBufferSaveSize) + std::string(", ") +
			std::string("FrameBufferMiscSize = ") + std::to_string(g_FrameBufferMiscSize) + std::string(".");
		logging::Log(msg.c_str());
		
		boost::this_thread::sleep_for(boost::chrono::seconds(10));
	}
}


void motion::ContinuouslyRecordCortex() {

	while(true) {
		// Save Cortex raw camera data
		motion::RecordCortex();
		
		boost::this_thread::sleep_for(boost::chrono::seconds(_s<int>("cortex.seconds_between_raw_saves")));
	}
}

// ----------------------------------------------
// Name:  motion::RecordCortexrtex()
// Desc:  Send a request for Cortex to record data. The recorded data is later post-processed to 
//        extract the raw data and merge it with the other dataset.
// Note:  This function can be called from multiple threads at once, and needs 
//        to explicitly enforce thread safety.
// ----------------------------------------------

void motion::RecordCortex() {

	// Lock to enforce thread-safety
	boost::mutex::scoped_lock local_lock(g_RawCortexCameraSaveLock);
	
	int   nRet = RC_GeneralError;
	void *pResponse;
	int   nBytes;

	logging::Log("[MOTION] (Re-)Triggering Cortex raw recording...");

	// Stop & save previous recording block
	if (g_bCortexRecording) {
		for (int tries = 0; tries < 20; tries++) {
			nRet = Cortex_Request("StopRecording", &pResponse, &nBytes);
			if (nRet == RC_Okay) {
				break;
			}
			boost::this_thread::sleep_for(boost::chrono::milliseconds(200));
		}
	}

	g_bCortexRecording = true;

	// Start (new) recording block
	for (int tries = 0; tries < 13; tries++) {
		nRet = Cortex_Request("StartRecording", &pResponse, &nBytes);
		boost::this_thread::sleep_for(boost::chrono::milliseconds(333));
	}

	boost::this_thread::sleep_for(boost::chrono::milliseconds(3000));
	nRet = Cortex_Request("StartRecording", &pResponse, &nBytes);
		
	// Reset frames
	g_nFramesSinceRawCortexCameraSave = 0;
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
//        is performed on a separate thread. See WatchFrameBuffer().
// ----------------------------------------------

void motion::BufferFrame(sFrameOfData* frameOfData) {

    // For some reason Cortex is sending frames twice, so check if we've already processed this one
    if (g_nLastFrameIndex == frameOfData->iFrame) { return; }

    // Save copy of frame data
	CortexFrame* pCortexFrame = new CortexFrame();
	if (pCortexFrame) {
		pCortexFrame->pFrame = new sFrameOfData();
		memset(pCortexFrame->pFrame, 0, sizeof(sFrameOfData));
		Cortex_CopyFrame(frameOfData, pCortexFrame->pFrame);
		pCortexFrame->nTimestampReceived = common::GetTimestamp(); // still error.... is this a timestamp issue?
		g_FrameBuffer.push(pCortexFrame);
		g_FrameBufferSize++;
	}

	// Save copy for the saving thread (TODO: Perhaps don't copy the frame twice?)
	CortexFrame* pCortexFrame2 = new CortexFrame();
	if (pCortexFrame2) {
		pCortexFrame2->pFrame = new sFrameOfData();
		memset(pCortexFrame2->pFrame, 0, sizeof(sFrameOfData));
		Cortex_CopyFrame(frameOfData, pCortexFrame2->pFrame);
		pCortexFrame2->nTimestampReceived = pCortexFrame->nTimestampReceived;
		g_FrameBufferSave.push(pCortexFrame2);
		g_FrameBufferSaveSize++;
	}

	// Save copy for the saving thread (TODO: Perhaps don't copy the frame thrice?)
	CortexFrame* pCortexFrame3 = new CortexFrame();
	if (pCortexFrame3) {
		pCortexFrame3->pFrame = new sFrameOfData();
		memset(pCortexFrame3->pFrame, 0, sizeof(sFrameOfData));
		Cortex_CopyFrame(frameOfData, pCortexFrame3->pFrame);
		pCortexFrame3->nTimestampReceived = pCortexFrame->nTimestampReceived;
		g_FrameBufferMisc.push(pCortexFrame3);
		g_FrameBufferMiscSize++;
	}

    // Record the most recent frame index
    g_nLastFrameIndex = frameOfData->iFrame;
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

	CortexFrame *pCortexFrame;

    while (true) {
		// Make sure this buffer never gets too big
		while (g_FrameBufferSize > 10) {
			g_FrameBuffer.pop(pCortexFrame);
			g_FrameBufferSize--;

			int testDbg = g_FrameBufferSize;
			long testDbg2 = long(pCortexFrame->pFrame);
			Cortex_FreeFrame(pCortexFrame->pFrame);
			delete pCortexFrame->pFrame;
			delete pCortexFrame;
		}

        if (g_FrameBuffer.pop(pCortexFrame)) {
			g_FrameBufferSize--;

            motion::ProcessFrame(pCortexFrame);

            Cortex_FreeFrame(pCortexFrame->pFrame);
            delete pCortexFrame->pFrame;
			delete pCortexFrame;

            iFrame += 1;
        }
        else {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(1));
        }
    }
}

void motion::WatchFrameBufferSave() {

    int iFrame = 0;

	std::string file = (common::GetCommonOutputPrefix() + ".msgpack");
    std::ofstream fo(file.c_str(), std::ofstream::binary);

	std::string file2 = (common::GetCommonOutputPrefix() + "experimental.msgpack");
    std::ofstream fo2(file.c_str(), std::ofstream::binary);

	CortexFrame *pCortexFrame;

    while (true) {

		// Make sure this buffer never gets too big
		while (g_FrameBufferSaveSize > 10000) {
			g_FrameBufferSave.pop(pCortexFrame);
			g_FrameBufferSaveSize--;

			// TODO: Report loss of data!

			Cortex_FreeFrame(pCortexFrame->pFrame);
			delete pCortexFrame->pFrame;
			delete pCortexFrame;
		}

        if (g_FrameBufferSave.pop(pCortexFrame)) {
			g_FrameBufferSaveSize--;

			// Save this frame
            motion::SaveFrameMsgPack(fo, pCortexFrame);

            Cortex_FreeFrame(pCortexFrame->pFrame);
            delete pCortexFrame->pFrame;
			delete pCortexFrame;

            if (iFrame == 1000) { fo.flush(); iFrame = 0; }
            iFrame += 1;
        }
        else {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(1));
        }
    }
    fo.close();
    fo2.close();
}

void motion::WatchFrameBufferMisc() {

	CortexFrame *pCortexFrame;

	while (true) {
		
		// Make sure this buffer never gets too big
		while (g_FrameBufferMiscSize > 5) {
			g_FrameBufferMisc.pop(pCortexFrame);
			g_FrameBufferMiscSize--;

			Cortex_FreeFrame(pCortexFrame->pFrame);
			delete pCortexFrame->pFrame;
			delete pCortexFrame;
		}
		
		if (g_FrameBufferMisc.pop(pCortexFrame)) {
			g_FrameBufferMiscSize--;

			rotatingperch::updatePerch(pCortexFrame);
			
			Cortex_FreeFrame(pCortexFrame->pFrame);
			delete pCortexFrame->pFrame;
			delete pCortexFrame;
		}
		else {
			boost::this_thread::sleep_for(boost::chrono::milliseconds(1));
		}
	}
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

void motion::ProcessFrame(CortexFrame *pCortexFrame) {

	if (_s<std::string>("tracking.method") == "velocity") {
		motion::ProcessFrame_VelocityThreshold(pCortexFrame);

	} else if (_s<std::string>("tracking.method") == "z") {
		motion::ProcessFrame_Z(pCortexFrame);
	}

	// Log info on frame processing time
	if (g_bMotionDebugEnabled) {
		g_fsMotionDebugInfo << "delay_frame," << pCortexFrame->pFrame->iFrame << "," << 
			pCortexFrame->nTimestampReceived << "," << common::GetTimestamp() << "\n";
	}
}

// ----------------------------------------------
// Serialize
// ----------------------------------------------

std::vector<std::vector<float>> _MsgPack_UnidentifiedMarkers(sFrameOfData* pF) {
    std::vector<std::vector<float>> markers;
    
    // Skip unidentified markers if they're too numerous (i.e. noisy)
    // Prevents overflowing harddrive accidentally
    for (int i = 0; i < std::min(_s<int>("tracking.unid_marker_save_maximum"), pF->nUnidentifiedMarkers); i++) {
        markers.push_back(std::vector<float>(
            pF->UnidentifiedMarkers[i], pF->UnidentifiedMarkers[i] + 3));
    }

    return markers;
}

std::vector<
    msgpack::type::tuple<
        std::string,                     // szName
        std::vector<                     // Markers
            std::vector<float>>>         // x,y,z
        > _MsgPack_Bodies(sFrameOfData* pF) {

    std::vector<
        msgpack::type::tuple<
        std::string,                     // szName
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

void motion::SaveFrameMsgPack(std::ofstream& o, CortexFrame *pCortexFrame) {

	sFrameOfData* pF = pCortexFrame->pFrame;

    msgpack::type::tuple<
        int,                        // iFrame
        float,                      // fDelay
        std::vector<                // BodyData
        msgpack::type::tuple<
        std::string,                // szName
        std::vector<                // Markers
        std::vector<float>>         // x,y,z
        >
        >,
        std::vector<                // UnidentifiedMarkers
        std::vector<float>          // x,y,z
        >,
        std::vector<int>,            // TimeCode  (Cortex) 
		long long                    // Timestamp (System)
        > d(
			pF->iFrame,
            pF->fDelay,
            _MsgPack_Bodies(pF),
            _MsgPack_UnidentifiedMarkers(pF),
            _MsgPack_TimeCode(pF),
			pCortexFrame->nTimestampReceived
        );
    msgpack::pack(o, d);
}

void motion::Save(std::string prefix, float startTimeAgo, float endTimeAgo) {

	// WE no longer separately save Cortex frame data... As all frames are recorded, as well as trigger signals, 
	// this information can be more accurateoy reconstructed that way... (i.e. without the delays, duplication, etc. 
	// that this function would introduce...)

	/*
    logging::Log("[MOTION] Saving Cortex data.");

    std::ofstream fo( prefix + "_cortex.msgpack", std::ofstream::binary);
    
    msgpack::pack(fo, int(g_nLastFrameIndex));

    fo.close();

    logging::Log("[MOTION] Finished saving Cortex data.");
	*/
}