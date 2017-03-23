#pragma once

#include "hardware.h"
#include "motion.h"
#include "settings.h"

namespace rotatingperch {

	void updatePerch(motion::CortexFrame *pCortexFrame);
	void Init();
}