#pragma once

#include "common.h"

namespace hardware {

	void Init( boost::thread* pThread );

	void SendTrigger();

	const char* SendFlySimCommand(const char* cmd);

	void UpdateFlySim();

}