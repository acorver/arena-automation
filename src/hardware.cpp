#include "stdafx.h"

//
//
// See:
//      C:\Users\Public\Documents\National Instruments\NI-DAQ\Examples\DAQmx ANSI C\Digital\Generate Values\Write Dig Chan
//

#include "hardware.h"
#include "photron.h"

Serial* pSerial = 0;
bool initialized = false;

void hardware::Init() {

	// Variables
	char	serialBuffer[128];
	wchar_t buffer[128];

	// Find the right device
	for (int i = 0; i < 20; i++) {

		swprintf(buffer, L"\\\\.\\COM%d", i);
		pSerial = new Serial(buffer);
		
		boost::this_thread::sleep_for(boost::chrono::milliseconds(500));

		if (!pSerial->IsConnected()) {
			delete pSerial;
			pSerial = 0;
			continue;
		}

		pSerial->WriteData("h\n", 1);

		boost::this_thread::sleep_for(boost::chrono::milliseconds(500));

		pSerial->ReadData(serialBuffer, 128);

		if (std::string(serialBuffer).find("Teensy TTL Controller") != std::string::npos) {
			// We've found our device!
			logging::Log("[HARDWARE] Connection with Teensy TTL trigger interface established on COM port %d.", i);
			break;
		}
	}

	if (!pSerial) {
		logging::Log("[HARDWARE] Could not establish connection with Teensy TTL trigger interface.");
	}

	initialized = true;
}

void hardware::SendTrigger() {

	char	serialBuffer[128];

	if (!initialized || !pSerial) { 
		logging::Log("[HARDWARE] Attempting to send TTL trigger, but serial communication is not initialized.");
		return;
	}

	if (!pSerial->IsConnected()) {
		logging::Log("[HARDWARE] Error connecting to serial port.");
	}

	pSerial->WriteData("t", 1);

	for (int i = 0; i < 10; i++) {
		pSerial->ReadData(serialBuffer, 128);
		if (serialBuffer[0] == '!') {
			break;
		} 
	}
}
