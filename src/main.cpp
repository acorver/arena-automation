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
#include "rotatingperch.h"
#include "log.h"

int main() {

	boost::thread t1;
	boost::thread t2;
	boost::thread t3;
	boost::thread t4;
	boost::thread t5;

	int nResult;

	// Initialize settings
	settings::Init();

	// Initialize the common output prefix (would have been set without calling 
	// this in advance, but this prevents asynchronous race conditions).
	common::GetCommonOutputPrefix();

	// Initialize the log
	logging::Init();

	// Log current directory (TODO)
	//boost::filesystem::path cwd(boost::filesystem::current_path());
	//logging::Log("Current path is: %s", cwd.string());

	// Attempt to initialize server communication
	t1 = server::Init();

	// Attempt to initialize hardware interfaces
	hardware::Init( &t5 );

	// Attempt to initialize USB cameras
	//nResult = usbcamera::Init(&t4);

	// Attempt to initialize motion processing
	nResult = motion::Init( &t2 );

	// Attempt to initialize perches
	rotatingperch::Init();

	// Attempt to initialize photron cameras
	if (settings::GetSettings()["settings"]["photron"]["enabled"] ) {
		if (!_s<bool>("photron.use_clients")) {
			nResult = photron::Init(&t3);
		}
		else {
			for (int clientID = 0; clientID < 1024; clientID++) {
				if (!photron::AttemptToStartClient(clientID)) { break; }
			}
		}
	}

	// TODO: Auto-spawn browser window...

	// Join on all threads (prevent program from exiting)
	t1.join();
	t2.join();
	t5.join();
	t3.join();
}
