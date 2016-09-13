#pragma once

#include "common.h"

using namespace cv;

namespace usbcamera {

	typedef struct Camera {
		
		unsigned long m_nCameraID;
		unsigned long m_nWidth;
		unsigned long m_nHeight;
		bool m_bRunning;
		common::ImageBuffer m_Buffer;
		std::string m_strName;

		Camera() { 
			
		}

	} Camera;

	int Init(boost::thread* pThread);

	void Activate(const char* name);

	void UpdateLiveImage();

	std::string GetLiveImage(int index);

	void Save(std::string prefix, float startTimeAgo, float endTimeAgo);
}
