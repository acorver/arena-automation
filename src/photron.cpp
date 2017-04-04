#include "stdafx.h"

// ============================================================================
// 
// Name: photron.cpp
// Desc:
//       This file handles connection and processing of Photron high-speed 
//       camera footage.
//
// ============================================================================

#include "photron.h"
#include "settings.h"

std::deque<photron::CameraInfo> cameraInfo;
std::deque<unsigned char*>      vLiveImageBuf;            /* Memory sequence pointer for storing a live image */

int nJPEGBufSize = 1024 * 1024 * 2;
char      pJPEGBuf[1024 * 1024 * 2];
bool saving = false;

bool g_bPhotronIsInit = false;

boost::atomic<unsigned int> numPhotronRetrieving;

bool photron::IsInitialized() {
	return g_bPhotronIsInit;
}

// ============================================================================
// Name: GetStatus
// Desc: 
// ============================================================================

unsigned long photron::GetStatus(unsigned long nDeviceNo) {
	unsigned long nRet, nStatus, nErrorCode;

	for (int i = 0; i < 10; i++) {
		nRet = PDC_GetStatus(
			nDeviceNo,
			&nStatus,
			&nErrorCode);

		if (nRet == PDC_FAILED) {
			logging::Log("[CAMERA] PDC_GetStatus Error %d", nErrorCode);
		}
		else {
			break;
		}
	}

	return nStatus;
}

// ============================================================================
// Name: GetStatus
// Desc: 
// ============================================================================

unsigned long photron::SoftwareTrigger() {

	unsigned long nRet, nErrorCode, nStatus;
	bool ready;

	// Stop recording
	for (int i = 0; i < cameraInfo.size(); i++) {

		nRet = PDC_TriggerIn(cameraInfo[i].nDeviceNo, &nErrorCode);

		if (nRet == PDC_FAILED) {
			logging::Log("[CAMERA] PDC_SetTriggerIn Error %d", nErrorCode);
			return 1;
		}
	}

	// Confirms the operating mode of each device
	for (int i = 0; i < cameraInfo.size(); i++) {

		SetPlaybackMode(cameraInfo[i].nDeviceNo, &nStatus, &nErrorCode);

		/*
		ready = false;
		while (!ready) {
			nRet = PDC_GetStatus(
				cameraInfo[i].nDeviceNo,
				&nStatus,
				&nErrorCode);

			if (nRet == PDC_FAILED) {
				logging::Log("[CAMERA] PDC_GetStatus Error %d", nErrorCode);
			} else {
				if (nStatus != PDC_STATUS_REC && nStatus != PDC_STATUS_ENDLESS)
				{
					logging::Log("[CAMERA] Camera %d successfully stopped recording. Status %d.", cameraInfo[i].nDeviceNo, nStatus);
					ready = true;
				}
				else {
					logging::Log("[CAMERA] Camera %d failed to stop recording. It's current status is %d. Retrying...", cameraInfo[i].nDeviceNo, nStatus);

					photron::SetPlaybackMode(cameraInfo[i].nDeviceNo, &nStatus, &nErrorCode);
				}
			}
		}
		*/
	}
}

// ============================================================================
// Name: SetLiveMode
// Desc: 
// ============================================================================

unsigned long photron::SetLiveMode(unsigned long nDeviceNo, unsigned long *nStatus, unsigned long *nErrorCode) {
	unsigned long nRet;

	while (1) {

		nRet = PDC_GetStatus(
			nDeviceNo,
			nStatus,
			nErrorCode);

		if (*nStatus == PDC_STATUS_LIVE && nRet != PDC_FAILED) { break; }

		if (*nStatus == PDC_STATUS_PLAYBACK) {

			nRet = PDC_SetStatus(
				nDeviceNo,
				PDC_STATUS_LIVE,
				nErrorCode);

			if (nRet == PDC_FAILED) {
				logging::Log("[CAMERA] PDC_SetStatus Error %d", *nErrorCode);
				return PDC_FAILED;
			}
		}
	}

	return PDC_SUCCEEDED;
}

// ============================================================================
// Name: SetExternalInMode
// Desc: 
// ============================================================================

unsigned long photron::SetExternalInMode(unsigned long deviceNo, unsigned long extInPortNo, unsigned long mode) {
	
	unsigned long nRet, nMode, nErrorCode;

	while (true) {

		nRet = PDC_GetExternalInMode(
			deviceNo,
			extInPortNo,
			&nMode,
			&nErrorCode
		);

		if (nRet != PDC_SUCCEEDED || nMode != mode) {
			
			nRet = PDC_SetExternalInMode(
				deviceNo,
				extInPortNo,
				mode,
				&nErrorCode
			);

			if (nRet != PDC_SUCCEEDED) {
				printf("[CAMERA] Failed to set external port %d to mode %d. Error code %d. Retrying...\n", extInPortNo, mode, nErrorCode);
			}

		} else if (nRet == PDC_SUCCEEDED && nMode == mode) {
			break;
		}
	}

	printf("[CAMERA] Successfully set external port %d to mode %d\n", extInPortNo, mode);
	return PDC_SUCCEEDED;
}

// ============================================================================
// Name: SetPlaybackMode
// Desc: 
// ============================================================================

unsigned long photron::SetPlaybackMode(unsigned long nDeviceNo, unsigned long *nStatus, unsigned long *nErrorCode) {
	unsigned long nRet, tries = 0;

	while (true) {

		nRet = PDC_GetStatus(
			nDeviceNo,
			nStatus,
			nErrorCode);

		if ( (*nStatus) == PDC_STATUS_PLAYBACK && nRet != PDC_FAILED) { break; }
			
		if ( (*nStatus) != PDC_STATUS_PLAYBACK ) {

			nRet = PDC_SetStatus(
				nDeviceNo,
				PDC_STATUS_PLAYBACK,
				nErrorCode);

			if (nRet == PDC_FAILED) {
				logging::Log("[CAMERA] PDC_SetStatus Error %d. Retrying...", *nErrorCode);
				tries += 1;
				if (tries > _s<int>("photron.set_status_max_retries")) { 
					logging::Log("[CAMERA] After %d tries, camera %d could not be switched to playback mode (error %d). Aborting...", 
						_s<int>("photron.set_status_max_retries"), nDeviceNo, *nErrorCode);
					return PDC_FAILED;
				}
			}
		}
	}

	logging::Log("[CAMERA] Camera %d successfully switched to playback mode.", nDeviceNo);

	return PDC_SUCCEEDED;
}

// ============================================================================
// Name: GetStatus
// Desc: 
// ============================================================================

unsigned long photron::SetLutMode(unsigned long nDeviceNo, unsigned long nChildNo, unsigned long nMode, unsigned long *nErrorCode) {
	unsigned long nRet;
	unsigned long nLutMode;

	while (1) {

		nRet = PDC_GetLUTMode(
			nDeviceNo,
			nChildNo,
			&nLutMode,
			nErrorCode);

		if (nLutMode != nMode) {

			nRet = PDC_SetLUTUser(
				nDeviceNo,
				nChildNo,
				nMode,
				nErrorCode
			);

			if (nRet == PDC_FAILED) {
				logging::Log("[CAMERA] PDC_SetLUTUser Error %d", *nErrorCode);
				continue;
			}

			nRet = PDC_SetLUTMode(
				nDeviceNo,
				nChildNo,
				nMode,
				nErrorCode
			);

			if (nRet == PDC_FAILED) {
				logging::Log("[CAMERA] PDC_SetLUTMode Error %d", *nErrorCode);
				continue;
			}
		}
		else {
			logging::Log("[CAMERA] PDC_SetLUTUser Successful.");
			break;
		}
	}

	return PDC_SUCCEEDED;
}

// ============================================================================
// Name: GetStatus
// Desc: 
// ============================================================================

unsigned long photron::StartRecording() {

	unsigned long nRet = 0;
	unsigned long nStatus = 0;
	unsigned long nErrorCode = 0;
	unsigned long nRate;

	logging::Log("[CAMERA] Initializing recording...");

	for (int i = 0; i < cameraInfo.size(); i++) {

		// Set device into live mode
		nRet = photron::SetLiveMode(cameraInfo[i].nDeviceNo, &nStatus, &nErrorCode);

		// Configure child devices
		for (int c = 0; c < cameraInfo[i].nChildNos.size(); c++) {

			// Set the recording rate [TODO!!! --> (DEPRECATED: Cameras are synced with external signal)
			/*
			nRet = PDC_SetRecordRate(
				cameraInfo[i].nDeviceNo,
				cameraInfo[i].nChildNos[c],
				1000,
				&nErrorCode);

			if (nRet == PDC_FAILED) {
				logging::Log("[CAMERA] PDC_SetRecordRate %d", nErrorCode);
				return 1;
			}
			*/

			nRet = PDC_GetRecordRate(
				cameraInfo[i].nDeviceNo,
				cameraInfo[i].nChildNos[c],
				&nRate,
				&nErrorCode);

			if (nRet == PDC_FAILED) {
				logging::Log("[CAMERA] PDC_GetRecordRate %d", nErrorCode);
				return 1;
			} else {
				logging::Log("[CAMERA] Detected a framerate of %d fps for camera %d (child %d).", 
					nRate, cameraInfo[i].nDeviceNo, cameraInfo[i].nChildNos[c]);
			}

			// Set shutter speed
			nRet = PDC_SetShutterSpeedFps(
				cameraInfo[i].nDeviceNo,
				cameraInfo[i].nChildNos[c],
				1000,
				&nErrorCode);

			if (nRet == PDC_FAILED) {
				logging::Log("[CAMERA] PDC_SetShutterSpeedFps %d", nErrorCode);
				return 1;
			}

			// Set resolution
			nRet = PDC_SetResolution(
				cameraInfo[i].nDeviceNo,
				cameraInfo[i].nChildNos[c],
				1024, 1024,
				&nErrorCode);

			if (nRet == PDC_FAILED) {
				logging::Log("[CAMERA] PDC_SetResolution %d", nErrorCode);
				return 1;
			}

			char nDepth = 0;
			nRet = PDC_GetBitDepth(
				cameraInfo[i].nDeviceNo,
				cameraInfo[i].nChildNos[c],
				&nDepth,
				&nErrorCode
			);

			if (nRet == PDC_FAILED)
			{
				logging::Log("PDC_GetBitDepth Error %d", nErrorCode);
			}

			nRet = PDC_SetBitDepth(
				cameraInfo[i].nDeviceNo,
				cameraInfo[i].nChildNos[c],
				12,
				&nErrorCode
			);

			if (nRet == PDC_FAILED) 
			{
				logging::Log("PDC_SetBitDepth Error %d", nErrorCode);
			}

			// Set transfer mode
			nRet = PDC_SetTransferOption(
				cameraInfo[i].nDeviceNo,
				cameraInfo[i].nChildNos[c],
				PDC_8BITSEL_12UPPER,
				PDC_FUNCTION_OFF,
				PDC_COLORDATA_NOCOLOR,
				&nErrorCode);

			if (nRet == PDC_FAILED)
			{
				logging::Log("PDC_SetTransferOption Error %d", nErrorCode);
				return 1;
			}

		}

		// Puts a device into end trigger mode (TODO: CHECK THE PARAMETERS)
		nRet = PDC_SetTriggerMode(
			cameraInfo[i].nDeviceNo,
			PDC_TRIGGER_END,
			0,
			0,
			0,
			&nErrorCode);

		if (nRet == PDC_FAILED) {
			logging::Log("[CAMERA] PDC_SetTriggerMode Error %d", nErrorCode);
			return 1;
		}

		// Put camera in live mode (DEPRECATED: I believe the live mode has to be activated earlier in order  
		// for SetXYZ calls to work. Once this is confirmed, remove the line below):
		// SetLiveMode(cameraInfo[i].nDeviceNo, &nStatus, &nErrorCode);

		// Put camera in "recording ready" mode
		while (1) {

			nRet = PDC_GetStatus(
				cameraInfo[i].nDeviceNo,
				&nStatus,
				&nErrorCode);

			if (nStatus == PDC_STATUS_RECREADY && nRet != PDC_FAILED) { break; }

			nRet = PDC_SetRecReady(cameraInfo[i].nDeviceNo, &nErrorCode);

			if (nRet == PDC_FAILED) {
				logging::Log("[CAMERA] PDC_SetRecReady Error %d", nErrorCode);
			}

			boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
		}

		// Start endless recording
		while (1) {

			nRet = PDC_GetStatus(
				cameraInfo[i].nDeviceNo,
				&nStatus,
				&nErrorCode);

			nRet = PDC_SetEndless(cameraInfo[i].nDeviceNo, &nErrorCode);

			if (nRet == PDC_FAILED) {
				logging::Log("[CAMERA] PDC_SetEndless Error %d", nErrorCode);
			}

			if ((nStatus == PDC_STATUS_ENDLESS) ||
				(nStatus == PDC_STATUS_REC))
			{
				break;
			}

			boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
		}

		logging::Log("[CAMERA] Camera %d successfully started recording.", cameraInfo[i].nDeviceNo);

		// For loop: Go to the next device
	}

	return PDC_SUCCEEDED;
}

// ============================================================================
// Name: GetStatus
// Desc: 
// ============================================================================

int photron::Init(boost::thread* pThread) {

	// Declare variables
	unsigned long nRet;
	unsigned long nNumChildren;
	unsigned long nErrorCode;
	unsigned long nStatus;
	unsigned long nSize = 0;
	unsigned long nChildNo = 0;

	unsigned long* nDeviceNos = new unsigned long[_s<int>("photron.camera_count")];  /* Device numbers */
	unsigned long nChildNos[PDC_MAX_DEVICE];
	unsigned long list[PDC_MAX_LIST_NUMBER];
	unsigned long IPList[PDC_MAX_DEVICE];   /* IP address to be searched */

	unsigned long nExtInPortNo; /* Input Terminal Port Number */
	unsigned long nMode;        /* External Input Signal Setting Parameter Value */

	PDC_DETECT_NUM_INFO DetectNumInfo;      /* Search result */
	PDC_LUT_PARAMS lutParams;
	PDC_LUT_PARAMS lutParams2;
	unsigned long nLutUser;

	TCHAR strName[256];

	// Set global variables
	numPhotronRetrieving = 0;

	// CLear detection info structure
	memset(&DetectNumInfo, 0, sizeof(PDC_DETECT_NUM_INFO));

	// Set the IP to be searched
	IPList[0] = _s<unsigned long>("photron.ip");

	// Initialize Photron
	nRet = PDC_Init(&nErrorCode);
	if (nRet == PDC_FAILED) {
		logging::Log("[CAMERA] PDC_Init Error %d", nErrorCode);
		return 1;
	}

	// Detect cameras
	while (DetectNumInfo.m_nDeviceNum != _s<int>("photron.camera_count")) {
		nRet = PDC_DetectDevice(
			PDC_INTTYPE_G_ETHER, /* Gigabit-Ether I/F */
			IPList,             /* IP address */
			_s<int>("photron.camera_count"),         /* Maximum number of searched devices */
			PDC_DETECT_AUTO,     /* Specifies an IP address explicitly */
			&DetectNumInfo,
			&nErrorCode);

		// Check status
		if (nRet == PDC_FAILED) {
			logging::Log("[CAMERA] PDC_DetectDevice Error %d", nErrorCode);
			return 1;
		}
		else {
			logging::Log("[CAMERA] Detected %d cameras.", DetectNumInfo.m_nDeviceNum);
		}
	}

	// When two devices are not detected 
	if (DetectNumInfo.m_nDeviceNum != _s<int>("photron.camera_count")) {
		logging::Log("[CAMERA] Unexpected number of cameras found. Is this a problem?");
	}

	// Open all devices
	for (int i = 0; i < DetectNumInfo.m_nDeviceNum; i++) {

		nRet = PDC_OpenDevice(
			&(DetectNumInfo.m_DetectInfo[i]), /* Object device information */
			&(nDeviceNos[i]),			      /* Device number */
			&nErrorCode);

		// Check for errors during opening of device
		if (nRet == PDC_FAILED) {
			logging::Log("[CAMERA] PDC_OpenDeviceError %d", nErrorCode);
			return 1;
		}

		// get the child device
		nRet = PDC_GetExistChildDeviceList(nDeviceNos[i],
			&nNumChildren,
			nChildNos,
			&nErrorCode);

		if (nRet == PDC_FAILED) {
			logging::Log("[CAMERA] PDC_GetExistChildDeviceList %d", nErrorCode);
			return 1;
		}

		// Get device name
		nRet = PDC_GetDeviceName(nDeviceNos[i],
			0,
			strName,
			&nErrorCode);

		if (nRet == PDC_FAILED) {
			logging::Log("[CAMERA] PDC_GetDeviceName %d", nErrorCode);
			return 1;
		}
		else {
			logging::Log("[CAMERA] Found camera device: %s", common::ws2s(std::wstring(strName)).c_str());
		}

		// Add device to list
		cameraInfo.push_back(CameraInfo(nDeviceNos[i]));

		// Iterate through child nodes
		for (int c = 0; c < nNumChildren; c++) {

			// Add to list
			cameraInfo.back().AddChild(nChildNos[c]);

			// Get the recording rate list
			nRet = PDC_GetRecordRateList(
				nDeviceNos[i],
				nChildNos[c],
				&nSize,
				list,
				&nErrorCode);

			if (nRet == PDC_FAILED) {
				logging::Log("[CAMERA] PDC_GetRecordRateList %d", nErrorCode);
				return 1;
			}

			// Get LUT modes
			nRet = PDC_GetLUTModeList(
				nDeviceNos[i],
				nChildNos[c],
				&nSize,
				list,
				&nErrorCode
			);

			if (nRet == PDC_FAILED) {
				logging::Log("[CAMERA] PDC_GetLUTModeList %d", nErrorCode);
				return 1;
			}

			// Get shutter speed list
			nRet = PDC_GetShutterSpeedFpsList(
				nDeviceNos[i],
				nChildNos[c],
				&nSize,
				list,
				&nErrorCode);

			if (nRet == PDC_FAILED) {
				logging::Log("[CAMERA] PDC_GetShutterSpeedFpsList %d", nErrorCode);
				return 1;
			}

			// Get LUT params
			nRet = PDC_GetLUTParams(
				nDeviceNos[i],
				nChildNos[c],
				PDC_LUT_DEFAULT1,
				&lutParams,
				&nErrorCode);

			if (nRet == PDC_FAILED) {
				logging::Log("[CAMERA] PDC_GetLUTParams %d", nErrorCode);
				return 1;
			}

			lutParams.m_nBrightnessR = 100;
			lutParams.m_nBrightnessG = 100;
			lutParams.m_nBrightnessB = 100;

			lutParams.m_nGammaR = 0.45;
			lutParams.m_nGammaG = 0.45;
			lutParams.m_nGammaB = 0.45;

			nRet = PDC_SetLUTUserParams(
				nDeviceNos[i],
				nChildNos[c],
				PDC_LUT_USER1,
				&lutParams,
				&nErrorCode
			);

			if (nRet == PDC_FAILED) {
				logging::Log("[CAMERA] PDC_SetLUTUserParams %d", nErrorCode);
				return 1;
			}

			nRet = PDC_GetLUTParams(
				nDeviceNos[i],
				nChildNos[c],
				PDC_LUT_USER1,
				&lutParams2,
				&nErrorCode
			);

			if (nRet == PDC_FAILED) {
				logging::Log("[CAMERA] PDC_GetLUTParams %d", nErrorCode);
				return 1;
			}

			photron::SetLutMode(nDeviceNos[i], nChildNos[c], PDC_LUT_USER1, &nErrorCode);
		}

		// Get valid inputs (currently for debug only --- set a breakpoint after these statements to discover valid values to use below...)
		nRet = PDC_GetExternalInModeList(
			nDeviceNos[i],
			1,
			&nSize,
			list,
			&nErrorCode
		);

		nRet = PDC_GetExternalInModeList(
			nDeviceNos[i],
			3,
			&nSize,
			list,
			&nErrorCode
		);

		// Set synchronization clock
		nExtInPortNo = 1;			   /* FASTCAM SA1/SA1.1 Input Terminal Port1 : INPUT 1 */
		nMode = PDC_EXT_IN_OTHERSSYNC_POSI;
		photron::SetExternalInMode(nDeviceNos[i], nExtInPortNo, nMode);

		// Check that the sync in signal is recognized (debug only)
		nRet = PDC_GetSyncInSignalStatus(
			cameraInfo[i].nDeviceNo,
			&nStatus,
			&nErrorCode
		);

		// Set TTL signal
		nExtInPortNo = 3;
		nMode = PDC_EXT_IN_TRIGGER_POSI;  /* TTL Trigger Input Signal */
		photron::SetExternalInMode(nDeviceNos[i], nExtInPortNo, nMode);
	}

	// Configure the device for recording
	nRet = StartRecording();

	// Start asynchronous live recording loop
	if (nRet == PDC_SUCCEEDED) {
		*pThread = boost::thread(photron::UpdateLiveImage);
	}

	g_bPhotronIsInit = true;

	// Clean up
	delete[] nDeviceNos;

	// Done!
	return 0;
}

// ============================================================================
// Name: UpdateLiveImage
// Desc: 
// ============================================================================

void photron::UpdateLiveImage() {
	unsigned long nRet;
	unsigned long nWidth, nHeight;  /* Width and height of the image */
	unsigned long nErrorCode, nStatus, nFrames, nBlocks, nRate, nMode;
	int           i, c, k, counter = 0;
	PDC_FRAME_INFO frameInfo;

	// specify parameters and allocate memory
	nWidth = 1024;
	nHeight = 1024;

	for (i = 0; i < cameraInfo.size(); i++) {
		for (c = 0; c < cameraInfo[i].nChildNos.size(); c++) {
			vLiveImageBuf.push_back((unsigned char*)malloc(nWidth * nHeight));
		}
	}

	while (true) {

		if (saving) {
			boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
			continue;
		}

		k = 0;
		for (i = 0; i < cameraInfo.size(); i++) {
			for (c = 0; c < cameraInfo[i].nChildNos.size(); c++) {

				// Request the live image
				nRet = PDC_GetLiveImageData(
					cameraInfo[i].nDeviceNo,
					cameraInfo[i].nChildNos[c],
					8,  /* 8 bits */
					vLiveImageBuf[k],
					&nErrorCode);

				k++;

				// Check for errors
				if (nRet == PDC_FAILED) {
					logging::Log("[CAMERA] PDC_GetLiveImageData Error %d", nErrorCode);
				}

				// Update camera info once in a while
				if ((counter % 10) == 0) {

					nRet = PDC_FAILED;
					while (nRet == PDC_FAILED) {
						nRet = PDC_GetStatus(
							cameraInfo[i].nDeviceNo,
							&nStatus,
							&nErrorCode);
						boost::this_thread::sleep_for(boost::chrono::milliseconds(200));
					}

					nRet = PDC_GetMaxFrames(
						cameraInfo[i].nDeviceNo,
						cameraInfo[i].nChildNos[c],
						&nFrames,
						&nBlocks,
						&nErrorCode
					);

					if (nRet == PDC_FAILED) {
						logging::Log("[CAMERA] PDC_GetMaxFrames Error %d", nErrorCode);
					}

					nRet = PDC_GetRecordRate(
						cameraInfo[i].nDeviceNo,
						cameraInfo[i].nChildNos[c],
						&nRate,
						&nErrorCode
					);

					if (nRet == PDC_FAILED) {
						logging::Log("[CAMERA] PDC_GetRecordRate Error %d", nErrorCode);
					}

					nRet = PDC_GetCamMode(
						cameraInfo[i].nDeviceNo,
						cameraInfo[i].nChildNos[c],
						&nMode,
						&nErrorCode
					);

					if (nRet == PDC_FAILED) {
						logging::Log("[CAMERA] PDC_GetCamMode Error %d", nErrorCode);
					}

					logging::Log("[CAMERA] Camera %d (child %d) recorded %d. Rate %d. Mode %d. Status %d.",
						cameraInfo[i].nDeviceNo, cameraInfo[i].nChildNos[c],
						nFrames, nRate, nMode, nStatus);
				}
			}
		}

		counter++;
		boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
	}
}

// ============================================================================
// Name: Save
// Get the live image from RAM, as the image has already been transferred from the camera
// to this PC by updateLiveImage()
// ============================================================================

std::string photron::GetLiveImage(int index) {

	bool ret;
	int bufSize = nJPEGBufSize;

	// return HTTP-response
	if (index < vLiveImageBuf.size()) {

		ret = jpge::compress_image_to_jpeg_file_in_memory(
			pJPEGBuf, bufSize, 1024, 1024, 1, vLiveImageBuf[index]);

		if (ret) {
			return std::string(reinterpret_cast<char const*>(pJPEGBuf), bufSize);
		}
	}

	return "";
}

// ============================================================================
// Name: Save
// Desc: Save movie from camera
// ============================================================================

void _SaveToFile(std::string const& file, int i, int c, int iStartFrame, int iEndFrame, std::ptrdiff_t nByteSize) {

	// Variables
	FILE *pPipe;
	unsigned long nRet, nErrorCode, nMode, nStatus = -1, nSize;
	std::chrono::high_resolution_clock::time_point startTime =
		std::chrono::high_resolution_clock::now();
	
	//std::ptrdiff_t nSize = (nByteSize / _s<int>("photron.saved_frame_increment")) + 1024*1024;
	
	//unsigned char *pBuf = (unsigned char*)malloc(nSize); // new unsigned char[nByteSize];

	int imgcols = 1024, imgrows = 1024, elemSize = 1;
	//std::ptrdiff_t lBlockSize = std::ptrdiff_t(imgrows * imgcols * elemSize) * std::ptrdiff_t(1024);
	//std::ptrdiff_t pEnd = std::ptrdiff_t(pBuf) + nSize;

	// Set playback mode
	photron::SetPlaybackMode(cameraInfo[i].nDeviceNo, &nStatus, &nErrorCode);

	// Start burst transfer mode
	while (true) {
		nRet = PDC_GetBurstTransfer(
			cameraInfo[i].nDeviceNo,
			&nMode,
			&nErrorCode);

		if (nRet == PDC_FAILED) {
			logging::Log("[CAMERA] PDC_GetBurstTransfer Error %d", nErrorCode);
		}
		else {
			if (nMode == PDC_FUNCTION_ON) {
				logging::Log("[CAMERA] Successfully enabled burst transfer.");
				break;
			} else {
				nRet = PDC_SetBurstTransfer(
					cameraInfo[i].nDeviceNo,
					PDC_FUNCTION_ON,
					&nErrorCode);

				if (nRet == PDC_FAILED) {
					logging::Log("[CAMERA] PDC_SetBurstTransfer Error %d", nErrorCode);
				}
			}
		}
	}

	// Start downloading
	logging::Log("[CAMERA] Camera %d started downloading frames [%d to %d].", cameraInfo[i].nDeviceNo, iStartFrame, iEndFrame);

	// Save AVI file
	nErrorCode = PDC_SUCCEEDED;
	PDC_AVIFileSaveOpenA(
		cameraInfo[i].nDeviceNo,
		cameraInfo[i].nChildNos[c],
		file.c_str(),
		_s<int>("photron.fps"),
		PDC_COMPRESS_DIALOG_HIDE,
		&nErrorCode
	);

	for (int f = iStartFrame; f <= iEndFrame; f += _s<int>("photron.saved_frame_increment")) {
		PDC_AVIFileSave(
			cameraInfo[i].nDeviceNo,
			cameraInfo[i].nChildNos[c],
			f,
			&nSize,
			&nErrorCode
		);
	}

	PDC_AVIFileSaveClose(
		cameraInfo[i].nDeviceNo,
		cameraInfo[i].nChildNos[c],
		&nErrorCode
	);

	// Download each frame to RAM
	/*
	for (int f = iStartFrame; f <= iEndFrame; f+=_s<int>("photron.saved_frame_increment")) {
		while (true) {
			nRet = PDC_GetMemImageData(
				cameraInfo[i].nDeviceNo,
				cameraInfo[i].nChildNos[c],
				f,
				8, // TODO: CHANGE THIS TO 10 OR 12 LATER!!! (??)
				pBuf + (std::ptrdiff_t(f - iStartFrame)/_s<int>("photron.saved_frame_increment")) * std::ptrdiff_t(1024 * 1024),
				&nErrorCode);

			if ((f - iStartFrame) % 100 == 0) {
				logging::Log("[P] Downloaded %d/%d frames from camera %d.%d",
					f - iStartFrame, iEndFrame - iStartFrame, i, c);
			}

			if (nRet == PDC_FAILED) {
				logging::Log("[CAMERA] PDC_GetMemImageData Error %d. Retrying...", nErrorCode);
				if (nErrorCode == 111) {
					photron::SetPlaybackMode(cameraInfo[i].nDeviceNo, &nStatus, &nErrorCode);
				}
			}
			else {
				break;
			}
		}
	}
	*/

	numPhotronRetrieving--;

	logging::Log("[CAMERA] Camera %d downloaded frames [%d to %d] in %f seconds.",
		cameraInfo[i].nDeviceNo, iStartFrame, iEndFrame,
		std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::high_resolution_clock::now() - startTime).count());


	/*
	// Construct command
	std::stringstream sstm;
	sstm << "ffmpeg -y -f rawvideo -vcodec rawvideo -s " << imgcols << "x" << imgrows << 
		" -pix_fmt gray -i - -c:v libx264 -shortest -preset veryslow -crf 15 \"" << file.c_str() << "\"";

	// open a pipe to FFmpeg
	if (!(pPipe = _popen(sstm.str().c_str(), "wb"))) {
		logging::Log("[VIDEO ENCODING] Error in popen!");
		return;
	}

	// write to pipe
	for (std::ptrdiff_t pCur = std::ptrdiff_t(pBuf); pCur < pEnd; pCur += lBlockSize) {
		std::fwrite((const void*) pCur, std::min(lBlockSize, pEnd-pCur), 1, pPipe);
	}
	_pclose(pPipe);

	// Free memory!!
	delete[] pBuf;
	*/
}

void photron::Save(std::string prefix, float startTimeAgo, float endTimeAgo) {

	// Variables
	unsigned long nRet = 1;
	unsigned long nErrorCode = 1;
	unsigned long nAVIFileSize = 0;
	long iStartFrame = 0;
	long iEndFrame = 0;
	unsigned long nStatus = 0;
	bool		  ready, stillLive, bTriggerSuccess;
	PDC_FRAME_INFO frameInfo;

	if (saving) {
		logging::Log("[CAMERA] Can't save Photron videos. Save already in progress.");
		return;
	}

	// Update state
	saving = true;

	// Check that the cameras are off. Otherwise the trigger system didn't work
	for (int attempt = 0; attempt < 20; attempt++) {
		bTriggerSuccess = true;
		for (int i = 0; i < cameraInfo.size(); i++) {
			nStatus = photron::GetStatus(cameraInfo[i].nDeviceNo);
			if (nStatus == PDC_STATUS_REC || nStatus == PDC_STATUS_ENDLESS) {
				bTriggerSuccess = false; break;
			}
		}
		if (bTriggerSuccess) { break; }
		boost::this_thread::sleep_for(boost::chrono::milliseconds(50));
	}
	if (!bTriggerSuccess) {
		logging::Log("[CAMERA] Error: Hardware trigger did not successfully stop camera recording. Using software trigger instead.");
	} else {
		logging::Log("[CAMERA] Success: Hardware trigger successfully stopped camera recording");
	}

	// do software trigger if necessary
	if (!bTriggerSuccess) {
		nRet = photron::SoftwareTrigger();
	}

	unsigned int camI = -1;
	for (int i = 0; i < cameraInfo.size(); i++) {

		photron::SetPlaybackMode(
			cameraInfo[i].nDeviceNo,
			&nStatus,
			&nErrorCode
		);

		for (int c = 0; c < cameraInfo[i].nChildNos.size(); c++) {

			camI++;

			// Determine frames to download by turning the "X milliseconds ago - Y milliseconds ago"
			// timeframe into a range of Photron frame numbers
			memset(&frameInfo, 0, sizeof(PDC_FRAME_INFO));
			nRet = PDC_GetMemFrameInfo(
				cameraInfo[i].nDeviceNo,
				cameraInfo[i].nChildNos[c],
				&frameInfo,
				&nErrorCode);

			if (nRet == PDC_FAILED) {
				logging::Log("[CAMERA] Failed to obtain camera frame info! Cannot save from this camera this time. Error=%d", nErrorCode);
				continue;
			}
			else {
				logging::Log("[CAMERA] Camera %d, child %d, has recorded %d frames.",
					cameraInfo[i].nDeviceNo, cameraInfo[i].nChildNos[c], frameInfo.m_nRecordedFrames);
			}

			// For now, we ignore startTimeAgo / endTimeAgo (it is not currently used by the program)
			//iStartFrame = long(frameInfo.m_nEnd - startTimeAgo * _s<int>("photron.fps") - _s<int>("photron.saved_frames_before"));
			//iEndFrame = long(frameInfo.m_nEnd - endTimeAgo   * _s<int>("photron.fps") + _s<int>("photron.saved_frames_after"));
			iStartFrame = long(frameInfo.m_nTrigger - _s<int>("photron.saved_frames_before"));
			iEndFrame   = long(frameInfo.m_nTrigger + _s<int>("photron.saved_frames_after"));

			// Make sure the start/end frame don't exceed the recorded range
			if (iStartFrame < frameInfo.m_nStart) {
				iStartFrame = frameInfo.m_nStart;
				logging::Log("[CAMERA] Start frame exceeds range recorded by Photron. Trimming the recording...");
			}

			if (iEndFrame < frameInfo.m_nStart) {
				iEndFrame = frameInfo.m_nEnd;
				logging::Log("[CAMERA] End frame exceeds range recorded by Photron. Trimming the recording...");
			}

			if (iEndFrame > iStartFrame + _s<int>("photron.max_frames_saved")) {
				iEndFrame = iStartFrame + _s<int>("photron.max_frames_saved");
				logging::Log("[CAMERA] Photron set to record maximum of %d frames. Trimming the recording...", _s<int>("photron.max_frames_saved"));

			}

			// Allocate image buffer
			std::ptrdiff_t nByteSize = std::ptrdiff_t(1024 * 1024) * std::ptrdiff_t(iEndFrame + 2 - iStartFrame); // Should this be iEndFrame+1 or +2?

			// Get filename
			std::string fileStr = prefix + common::toStr("_%d_%d.photron.avi",
						cameraInfo[i].nDeviceNo, cameraInfo[i].nChildNos[c]);

			// Save to file on separate thread!
			numPhotronRetrieving++;
			boost::thread(_SaveToFile, fileStr, i, c, iStartFrame, iEndFrame, nByteSize);
		}
	}

	// Wait for all cameras to finish downloading
	while (numPhotronRetrieving > 0) {
		boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
	}

	// Start recording again
	StartRecording();
	// Finish state (TODO: Only switch to non-saving state once recording has started)
	saving = false;
}
