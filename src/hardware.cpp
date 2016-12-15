#include "stdafx.h"

//
//
// See:
//      C:\Users\Public\Documents\National Instruments\NI-DAQ\Examples\DAQmx ANSI C\Digital\Generate Values\Write Dig Chan
//

#include "hardware.h"
#include "photron.h"

Serial* pSerialTrigger = 0;
Serial* pSerialFlySim = 0;

boost::mutex g_mutexFlySim;

boost::atomic<bool>  g_bIsFlySimBusy;

Serial* findDevice(const char* devName, int* pPort) {

	Serial* pSerial = 0;
	wchar_t buffer[128];
	char	serialBuffer[128];

	for (int i = 0; i < 20; i++) {

		swprintf(buffer, L"\\\\.\\COM%d", i);
		pSerial = new Serial(buffer);

		boost::this_thread::sleep_for(boost::chrono::milliseconds(500));

		if (!pSerial->IsConnected()) {
			delete pSerial;
			pSerial = 0;
			continue;
		}

		pSerial->WriteData("h\n", 2);

		boost::this_thread::sleep_for(boost::chrono::milliseconds(500));

		pSerial->ReadData(serialBuffer, 128);

		if (std::string(serialBuffer).find(devName) != std::string::npos) {
			// We've found our device!
			*pPort = i;
			break;
		}

		// Disconnect
		delete pSerial; pSerial = 0;
	}

	return pSerial;
}

void hardware::Init(boost::thread* pThread) {

	// Variables
	char serialBuffer[128];
	int portTrigger, portFlySim;;

	// Set globals
	g_bIsFlySimBusy = false;

	// Find the right device
	pSerialTrigger = findDevice("Teensy TTL Controller"    , &portTrigger);
	pSerialFlySim  = findDevice("FlySim Arduino Controller", &portFlySim);

	if (!pSerialTrigger) {
		logging::Log("[HARDWARE] Could not establish connection with Teensy TTL trigger interface.");
	} else {
		logging::Log("[HARDWARE] Connection with Teensy TTL trigger interface established on COM port %d.", portTrigger);
	}

	if (!pSerialFlySim) {
		logging::Log("[HARDWARE] Could not establish connection with FlySim interface.");
	} else {
		logging::Log("[HARDWARE] Connection with FlySim interface established on COM port %d.", portFlySim);
	}

	// Continually read data from FlySim
	*pThread = boost::thread(hardware::UpdateFlySim); 
}

const char* hardware::SendFlySimCommand(const char* cmd) {

	// Gain exclusive access
	boost::unique_lock<boost::mutex> scoped_lock(g_mutexFlySim);

	std::string cmdStr = boost::replace_all_copy(std::string(cmd), "%20", " ");

	if (pSerialFlySim) {

		logging::Log("[HARDWARE] Sending FlySim command: %s", cmdStr.c_str());

		char buffer[4096];
		int n = sprintf(buffer, "%s\n", cmdStr.c_str());

		pSerialFlySim->WriteData(buffer, n);

		return "OK";

	} else {
		return "ERROR";
	}
}

void hardware::UpdateFlySim() {

	const int BUF_SIZE = 1024 * 256;
	char serialBuffer[BUF_SIZE];
	int  numBytesRead = 0;
	int  portFlySim = 0;

	while (true) {

		if (pSerialFlySim) {

			// Gain exclusive access
			boost::unique_lock<boost::mutex> scoped_lock(g_mutexFlySim);

			// Read FlySim info
			memset(serialBuffer, 0, BUF_SIZE);
			numBytesRead = pSerialFlySim->ReadData(serialBuffer, BUF_SIZE - 1);

			// Log any messages from FlySim
			if (numBytesRead > 0) {
				
				std::string msg = std::string("[FLYSIM] ") + std::string(serialBuffer, numBytesRead);
				logging::Log(msg.c_str());
			}
		} else {
			pSerialFlySim = findDevice("FlySim Arduino Controller", &portFlySim);
			if (!pSerialFlySim) {
				logging::Log("[HARDWARE] Failed to reconnect to FlySim interface.");
			}
			else {
				logging::Log("[HARDWARE] Reconnected with FlySim interface on COM port %d.", portFlySim);
			}
			boost::this_thread::sleep_for(boost::chrono::milliseconds(5000));
		}

		// Don't unnecessarily poll Arduino (data throughput now quite low)
		boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
	}
}

void hardware::SendTrigger() {

	char serialBuffer[128];

	if (!pSerialTrigger) {
		logging::Log("[HARDWARE] Attempting to send TTL trigger, but serial communication is not initialized.");
		return;
	}

	if (!pSerialTrigger->IsConnected()) {
		logging::Log("[HARDWARE] Error connecting to serial port.");
	}

	pSerialTrigger->WriteData("t", 1);

	for (int i = 0; i < 10; i++) {
		pSerialTrigger->ReadData(serialBuffer, 128);
		if (serialBuffer[0] == '!') {
			break;
		} 
	}
}
