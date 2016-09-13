#include "stdafx.h"

#include "usbcamera.h"
#include "settings.h"

// Variables
std::deque<Mat> bufferFrames;
std::deque<Mat> bufferFramesDebug;
std::deque<common::ImageBuffer> vLiveImageBuf; 
std::deque<usbcamera::Camera> vCameras;

// 
int usbcamera::Init(boost::thread* pThread) {

	// 
	int nDevices = videoInput::listDevices();
	for (int i = 0; i < nDevices; i++) {
		usbcamera::Camera c;
		c.m_nCameraID = i;
		c.m_strName = videoInput::getDeviceName(i);
		c.m_bRunning = false;
		vCameras.push_back(c);
	}

	// Start updates
	*pThread = boost::thread(usbcamera::UpdateLiveImage);
	
	return 0;
}

void usbcamera::Activate(const char* name) {

	for (int i = 0; i < vCameras.size(); i++) {
		if (vCameras[i].m_strName.find(name) != std::string::npos) {
			
			// TODO: SETUP DEVICE...
			
			//c.m_nWidth = vi.getWidth(i);
			//c.m_nHeight = vi.getHeight(i);
			//c.m_Buffer = common::ImageBuffer();

			vCameras[i].m_bRunning = true;
		}
	}
}

// 
void usbcamera::UpdateLiveImage() {

	// Variables
	Mat frame, gray, canny, mask, foreground, edges, element, debug;
	int iFrame = 0, numFrames, thresh = 50, morph_size = 4, nWidth, nHeight;
	char buffer[256];
	char key;
	float r;

	std::vector<std::vector<cv::Point> > contours;
	std::vector<cv::Vec4i> hierarchy;
	std::vector<Point2f>center;
	std::vector<float>radius;
	cv::Point2f c;
		
	RNG rng(12345);
	
	Scalar color = Scalar(rng.uniform(0, 255), rng.uniform(0, 255), rng.uniform(0, 255));
	
	std::vector<cv::Ptr<BackgroundSubtractorMOG2>> vpMOG2(vCameras.size());
	
	boost::posix_time::ptime timeNow = boost::posix_time::not_a_date_time;
	boost::posix_time::ptime timeMovementStart = boost::posix_time::not_a_date_time;

	// Open video capture
	//VideoCapture cap(2);
	
	//VideoCapture cap("Z:/people/Abel/ArenaAutomation/deploy/data/takeoff_short.mp4");
	//nWidth  = cap.get(CV_CAP_PROP_FRAME_WIDTH);
	//nHeight = cap.get(CV_CAP_PROP_FRAME_HEIGHT);
	
	// Allocate image buffer
	//vLiveImageBuf.push_back(common::ImageBuffer(nWidth, nHeight));

	/*
	if (!cap.isOpened()) {
		logging::Log("[USB CAMERAS] Could not connect to USB cameras.");
		return;
	} else {
		logging::Log("[USB CAMERAS] Successfully connected to USB cameras. Dimension are %d x %d.",
			nWidth, nHeight);
	}
	*/

	//Create infinte loop for live streaming
	while (1) { 

		// Loop through all cameras
		for (int deviceID = 0; deviceID < vCameras.size(); deviceID++) {
			
			// init background subtractor?
			if (vpMOG2[deviceID] == 0) {
				vpMOG2[deviceID] = createBackgroundSubtractorMOG2();
				vpMOG2[deviceID]->setDetectShadows(false);
			}

			// Loop
			//iFrame = cap.get(CV_CAP_PROP_POS_FRAMES);
			//numFrames = cap.get(CV_CAP_PROP_FRAME_COUNT);
			//if (iFrame == numFrames) {
			//	cap.set(CV_CAP_PROP_POS_FRAMES, 0);
			//}

			// Read frame from camera
			//cap >> frame; 
			// [TODO: GET FRAME!!]
        
			// Convert to grayscale
			cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

			// Save in buffer
			memcpy(vLiveImageBuf[0].pBuffer, gray.data,
				sizeof(unsigned char) * vLiveImageBuf[0].nWidth * vLiveImageBuf[0].nHeight);

			// Do background subtraction
			vpMOG2[deviceID]->apply(frame, mask);
		
			// Get rid of noise (i.e. expand regions)
			element = getStructuringElement(MORPH_RECT, Size(2 * morph_size + 1, 2 * morph_size + 1), Point(morph_size, morph_size));
			morphologyEx(mask, mask, cv::MORPH_CLOSE, element);

			// Canny edge detection
			Canny(mask, canny, thresh, thresh * 2, 3);

			// Find contours 
			findContours(canny.clone(), contours, hierarchy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

			center = std::vector<Point2f>();
			radius = std::vector<float>	 ();

			// Keep only contours of a minimum size
			for (int i = 0; i < contours.size(); i++) {
			
				c = cv::Point2f();
				minEnclosingCircle(contours[i], c, r);
				if (r >= 20) {
					center.push_back(c);
					radius.push_back(r);
				}
			}

			// Draw contours
			for (int i = 0; i < center.size(); i++)
			{
				circle(canny, center[i], (int) radius[i], color, 2, 8, 0);
				// putText(canny, std::string(i), center[i], ...)
			}


			// Store the frame in the cyclic buffer
			cvtColor(canny, debug, CV_GRAY2BGR);
			bufferFrames.push_front(frame.clone()); // TODO: Is clone() necessary?
			bufferFramesDebug.push_front(debug.clone()); // TODO: Is clone() necessary?
			if (bufferFrames.size() > USBCAMERA_BUFFER_SIZE) {
				bufferFrames.pop_back();
				bufferFramesDebug.pop_back();
			}

			timeNow = boost::posix_time::second_clock::local_time();

			// Trigger?
			// Note: Enforce minimum buffer size, as the algorithm (especially background subtraction) is unstable for few frames
			if (radius.size() > 0 && radius.size() < 5 && 
				bufferFrames.size() > 150) {

				// Start recording
				if (timeMovementStart.is_not_a_date_time()) {
					timeMovementStart = timeNow;
				}
			} else if (!timeMovementStart.is_not_a_date_time() && 
				timeNow - timeMovementStart > boost::posix_time::seconds(1) ) {
			
				// If too much time passed since the last movement, stop the recording
				float startTimeAgo = 0.001f * (timeNow - timeMovementStart).total_milliseconds();
				float endTimeAgo   = 0.0f;
				timeMovementStart = boost::posix_time::not_a_date_time;
				common::Save( startTimeAgo, endTimeAgo );
			}
		}
	}
}

std::string usbcamera::GetLiveImage(int index) {

	if (index >= vLiveImageBuf.size()) { return ""; }
	return vLiveImageBuf[index].ToJPEG();
}

void usbcamera::Save( std::string prefix, float startTimeAgo, float endTimeAgo ) {

	int startFrame = int(startTimeAgo * USBCAMERA_FPS);
	int   endFrame = int(  endTimeAgo * USBCAMERA_FPS);

	if (bufferFrames.size() == 0) {
		return;
	}

	// Write relevant segment
	cv::VideoWriter vw( prefix + "usbcamera.avi", cv::VideoWriter::fourcc('X','V','I','D'), 
		30.0, bufferFrames[0].size() );
	for (int iFrame = startFrame; iFrame >= endFrame; iFrame--) {
		if (iFrame > 0 && iFrame < bufferFrames.size()) {
			vw << bufferFrames[iFrame];
		}
	}
	vw.release();

	// Store debug videos
	cv::VideoWriter vw2( prefix + "usbcamera_debug_1.avi",
		cv::VideoWriter::fourcc('X', 'V', 'I', 'D'), 30.0, bufferFrames[0].size());
	cv::VideoWriter vw3( prefix + "usbcamera_debug_2.avi",
		cv::VideoWriter::fourcc('X', 'V', 'I', 'D'), 30.0, bufferFrames[0].size());
	for (int iFrame = bufferFrames.size()-1; iFrame >= 0; iFrame--) {
		vw2 << bufferFrames[iFrame]; 
		vw3 << bufferFramesDebug[iFrame];
	}
	vw2.release();
	vw3.release();
}
