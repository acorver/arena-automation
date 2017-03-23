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
Serial* pSerialPerch = 0;

boost::mutex g_mutexFlySim;

boost::atomic<bool>  g_bIsFlySimBusy;

boost::posix_time::ptime g_NextFlysimTrialTime;

int portTrigger, portFlySim, portPerch;

bool g_DfReady = false;

boost::lockfree::spsc_queue<std::string, boost::lockfree::capacity<256>> g_FlySimCommandQueue;

Serial* openDevice(int portNumber) {
	
	wchar_t buffer[128];
	swprintf(buffer, L"\\\\.\\COM%d", portNumber);
	Serial* pSerial = new Serial(buffer);

	boost::this_thread::sleep_for(boost::chrono::milliseconds(500));

	if (!pSerial->IsConnected()) {
		delete pSerial;
		pSerial = 0;
	}

	return pSerial;
}

Serial* findDevice(const char* devName, int* pPort) {

	Serial* pSerial = 0;
	char	serialBuffer[128];

	for (int i = 0; i < 20; i++) {

		pSerial = openDevice(i);
		if (!pSerial) {
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

	// Set globals
	g_bIsFlySimBusy = false;
	g_NextFlysimTrialTime = boost::posix_time::second_clock::local_time();

	// Find the right device
	pSerialTrigger = findDevice("Teensy TTL Controller"    , &portTrigger);
	pSerialFlySim  = findDevice("FlySim Arduino Controller", &portFlySim);
	pSerialPerch   = findDevice("Perch Controller", &portPerch);

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

	if (!pSerialPerch) {
		logging::Log("[HARDWARE] Could not establish connection with Perch interface.");
	}
	else {
		logging::Log("[HARDWARE] Connection with Perch interface established on COM port %d.", portPerch);
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

void hardware::NotifyDfOnPerch(bool anyDfReady) {

	// Notify FlySim? (In the future, notify whatever system is in charge of trial runs.)
	if (g_DfReady != anyDfReady) {

		g_DfReady = anyDfReady;

		g_FlySimCommandQueue.push(std::string("df_ready_on_perch,") + std::to_string(anyDfReady?1:0));
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

			// Write any commands to flysim
			std::string cmdFs;
			while (g_FlySimCommandQueue.pop(&cmdFs)) {
				cmdFs += "\n";
				pSerialFlySim->WriteData(strdup(cmdFs.c_str()), cmdFs.size());
			}

			// Read FlySim info
			memset(serialBuffer, 0, BUF_SIZE);
			numBytesRead = pSerialFlySim->ReadData(serialBuffer, BUF_SIZE - 1);
			std::string strBuf = std::string(serialBuffer, numBytesRead);

			// Make note if flysim trial is about to start (The 5 seconds headsup string is hardcoded 
			// should not be changed here or in the Arduino .ino file!)
			if (strBuf.find("Starting new trial in 2 seconds.") != std::string::npos) {

				g_NextFlysimTrialTime = boost::posix_time::second_clock::local_time() +
					boost::posix_time::millisec(2000);
			}

			// Log any messages from FlySim
			if (numBytesRead > 0) {
				
				std::string msg = std::string("[FLYSIM] ") + strBuf;
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

long hardware::GetTimeUntilNextFlysimTrialInMS() {

	return (boost::posix_time::second_clock::local_time() - 
		g_NextFlysimTrialTime).total_milliseconds();
}

void hardware::SendTrigger() {

	char serialBuffer[128];

	if (_s<bool>("tracking.trigger_hardware_only_on_flysim_trials")) {
		if (GetTimeUntilNextFlysimTrialInMS() > _s<int>("tracking.trigger_hardware_after_flysim_max")) {
			logging::Log("[HARDWARE] Not triggering hardware because takeoff did not occur soon enough after a FlySim trial.");
		}
	}

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

void hardware::RotatePerch(int perchIdx, int dir) {

	if (pSerialPerch) {
		if (!pSerialPerch->IsConnected()) {
			delete pSerialPerch;
			pSerialPerch = openDevice(portPerch);
			std::cout << "Resetting perch interface...\n";
		}

		if (pSerialPerch->IsConnected()) {
		
			char buffer[4096];
			int n = sprintf(buffer, "rotate %d %d\n", perchIdx, dir);

			// Read buffer --- even though this data is not currently used, if we don't read it, the buffer 
			// will fill up and stall reading!
			const int BUF_SIZE = 1024 * 256;
			char serialBuffer[BUF_SIZE];
			int numBytesRead = pSerialPerch->ReadData(serialBuffer, BUF_SIZE - 1);

			std::cout << serialBuffer << std::endl;

			boost::posix_time::ptime hpct = boost::posix_time::second_clock::local_time();

			pSerialPerch->WriteData(buffer, n);

			std::cout << "Hardware perch command took : " << (boost::posix_time::second_clock::local_time() - hpct).total_milliseconds() << " milliseconds (read "<< numBytesRead<<" bytes).\n";
		}
	}
}

void hardware::SetPerchSpeed(int minvel, int maxvel) {

	if (pSerialPerch) {
		char buffer[4096];
		int n = sprintf(buffer, "vel %d %d\n", minvel, maxvel);
		pSerialPerch->WriteData(buffer, n);

		std::cout << "Set perch speed. \n";
	}
}
