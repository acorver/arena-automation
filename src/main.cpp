#include "stdafx.h"

#pragma comment(linker, "/STACK:2000000000")
#pragma comment(linker, "/HEAP:40000000000")

#include "common.h"
#include "server.h"
#include "photron.h"
#include "motion.h"
#include "hardware.h"
#include "usbcamera.h"
#include "settings.h"
#include "log.h"

int main() {

	boost::thread t1;
	boost::thread t2;
	boost::thread t3;
	boost::thread t4;

	int nResult;

	// Initialize settings
	settings::Init();

	// Initialize the log
	logging::Init();

	// Log current directory (TODO)
	//boost::filesystem::path cwd(boost::filesystem::current_path());
	//logging::Log("Current path is: %s", cwd.string());

	// Attempt to initialize server communication
	t1 = server::Init();

	// Attempt to initialize hardware interfaces
	hardware::Init();

	// Attempt to initialize USB cameras
	//nResult = usbcamera::Init(&t4);

	// Attempt to initialize motion processing
	//nResult = motion::Init( &t2 );

	// Attempt to initialize photron cameras
	nResult = photron::Init(&t3);

	// Join on all threads (prevent program from exiting)
	t1.join();
	t2.join();
	t3.join();
}
