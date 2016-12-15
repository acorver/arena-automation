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

// Global variables
boost::lockfree::spsc_queue<motion::CortexFrame*,
	boost::lockfree::capacity<1000000> > 
	                         g_FrameBuffer, 
	                         g_FrameBufferSave;
boost::atomic<int>           g_nLastFrameIndex;
int                          saveFramesMaxIndex = -1;
std::queue<sFrameOfData>     saveBuffer;
int                          totalBufferedFrames = 0;
unsigned long                lastSavedFrameID;
std::deque<sFrameOfData*>    recentlySavedFrames;

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

	// Start recording Cortex
	RecordCortex(true);

    // Initialize debug info for evaluating motion tracking data in real time
    g_fsMotionDebugInfo = std::ofstream( (common::GetCommonOutputPrefix()+".motionlog") );

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
// Name:  RecordCortex()
// Desc:  Send a request for Cortex to record data. The recorded data is later post-processed to 
//        extract the raw data and merge it with the other dataset.
// ----------------------------------------------

void motion::RecordCortex(bool record) {

	int   nRet = RC_GeneralError;
	void *pResponse;
	int   nBytes;

	for (int tries = 0; tries < 5; tries++) {
		if (record) {
			nRet = Cortex_Request("StartRecording", &pResponse, &nBytes);
		} else {
			nRet = Cortex_Request("StopRecording", &pResponse, &nBytes);
		}
		if (nRet == RC_Okay) {
			break;
		}
	}
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
	CortexFrame* pCortexFrame = new CortexFrame();
	pCortexFrame->nTimestampReceived = common::GetTimestamp(); // still error.... is this a timestamp issue?
	pCortexFrame->pFrame = new sFrameOfData();
    memset(pCortexFrame->pFrame, 0, sizeof(sFrameOfData));
    Cortex_CopyFrame(frameOfData, pCortexFrame->pFrame);
    g_FrameBuffer.push(pCortexFrame);
    
    // Save copy for the saving thread (TODO: Perhaps don't copy the frame twice?)
	CortexFrame* pCortexFrame2 = new CortexFrame();
	pCortexFrame2->nTimestampReceived = common::GetTimestamp();
	pCortexFrame2->pFrame = new sFrameOfData();
    memset(pCortexFrame2->pFrame, 0, sizeof(sFrameOfData));
    Cortex_CopyFrame(frameOfData, pCortexFrame2->pFrame);
    g_FrameBufferSave.push(pCortexFrame2);

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
        if (g_FrameBuffer.pop(pCortexFrame)) {
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
        if (g_FrameBufferSave.pop(pCortexFrame)) {

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

    logging::Log("[MOTION] Saving Cortex data.");

    std::ofstream fo( prefix + "_cortex.msgpack", std::ofstream::binary);
    
    msgpack::pack(fo, int(g_nLastFrameIndex));

    fo.close();

    logging::Log("[MOTION] Finished saving Cortex data.");
}