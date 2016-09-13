#pragma once

#include "common.h"

#define IP_MOTION	0x00000000
#define IP_CAMERAS  0x00000000

#define IP_PHOTRON    0x0A010100 // 10.1.1.xxx
#define IP_PHOTRON_1  0x0A01010B // 10.001.001.011
#define IP_PHOTRON_2  0x0A010116 // 10.001.001.022

#define NUM_CAMERAS 2
#define CORTEX_FRAME_BUFFER_SIZE 20000
#define CORTEX_FPS 200
#define CORTEX_ANALYSIS_FPS 100
#define PHOTRON_FPS 1000
#define PHOTRON_EXTRA_FRAMES_BEFORE 50
#define PHOTRON_EXTRA_FRAMES_AFTER 0
#define CORTEX_IDENTIFIED_MARKER_INVALID 9999999

#define CORTEX_ENABLE_UNMARKED_DETECTION 0

// We need approximately 5 seconds of buffer
#define MAX_BODY_TRACKING_HISTORY 1000
#define MAX_BODY_TRACKING_DIST 200 // (in mm (?)) Tune this number
#define MAX_BODY_TRACKING_GAP 500

#define TAKEOFF_DETECTION_WINDOW 100 // 1/10th of a second (assuming 200 fps)
#define TAKEOFF_SPEED_THRESHOLD 50.0f // Tune this number (Units are in millimeter/second)
#define TAKEOFF_DETECTION_VELOCITY_SPAN 100

#define STATIONARY_DETECTION_WINDOW 150
#define STATIONARY_ACCELERATION_THRESHOLD 20.0f
#define STATIONARY_SPEED_THRESHOLD 10.0f

// TODO: If threshold is reached, prevent many takeoff/save pairs
// TODO: (esp. If segment is very short,) wait a few more seconds until deciding to save segment and not a 
// longer segment.
// TODO: Motion is caused by markers coming in and out, causing different averaging...
//       instead, compute statistically significant difference threshold  (stationary_speed_threshold ~= MAX_BODY_TRACKING_DIST/2)

#define RECORDING_THRESHOLD 200 * 4 // Four seconds?
#define DRAGONFLY_DETECTION_MARKER_THRESHOLD 1.9f

#define USBCAMERA_BUFFER_SIZE 300
#define USBCAMERA_FPS 30

namespace settings {
	
	void Init();
	
	void RegisterHandler(const char* strKey, void(*fHandler)(void* pValue));

	void LoadSettings();
	void SaveSettings();
	
	void SetSettings(json s);
	json GetSettings();

	std::string GetSettingsJSON();
	void SetSettingsJSON(std::string);
}
