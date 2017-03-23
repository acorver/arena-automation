#pragma once

#include "common.h"
#include "settings.h"

namespace hardware {

	void Init( boost::thread* pThread );

	void SendTrigger();

	const char* SendFlySimCommand(const char* cmd);

	void RotatePerch(int perchIdx, int dir);
	void SetPerchSpeed(int minvel, int maxvel);

	void UpdateFlySim();
	long GetTimeUntilNextFlysimTrialInMS();

	void NotifyDfOnPerch(bool anyDfReady);

}