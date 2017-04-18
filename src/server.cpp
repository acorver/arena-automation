#include "stdafx.h"

#include "server.h"
#include "motion.h"
#include "photron.h"
#include "hardware.h"
#include "usbcamera.h"
#include "settings.h"

struct PhotronClient {
	std::string address;
	unsigned int numCameras;
};
std::vector<PhotronClient> g_vPhotronClients;

boost::thread server::Init() {

	return boost::thread( server::Run );
}

crow::response CreateCrowResponse(std::string str) {

	crow::response r(str);
	r.add_header("Access-Control-Allow-Origin", "*");
	r.add_header("Access-Control-Allow-Headers", "origin, x-requested-with, content-type");
	r.add_header("Access-Control-Allow-Methods", "PUT, GET, POST, DELETE, OPTIONS");
	return r;
}

void server::Run() {

	crow::App<server::DocumentMiddleWare> app;
	//app.get_middleware<server::DocumentMiddleWare>().setDocumentRoot("/www/");

	CROW_ROUTE(app, "/")
		([]() {
		return CreateCrowResponse("This server reports the status of ongoing experiments in the dragonfly arena.");
	});

	CROW_ROUTE(app, "/api/takeoff-trigger/<int>")
		([](int enable) {

		motion::EnableMotionTrigger(enable == 1);

		return CreateCrowResponse("OK");
	});

	CROW_ROUTE(app, "/api/camera/photron_count/<int>")
		([](int unused) {

		// Make inventory of photron clients
		g_vPhotronClients.clear();
		for (int clientID = 0; clientID < 1024; clientID++) {
			
			std::string sIP = std::string("photron.client_") +
				std::to_string(clientID) + std::string(".client_ip");
			std::string sPort = std::string("photron.client_") +
				std::to_string(clientID) + std::string(".port");
			
			if (_ss<std::string>(sIP.c_str()) == "") { break; }
			
			PhotronClient pc;
			pc.address = std::string("http://") + _s<std::string>(sIP.c_str()) + std::string(":") +
				std::to_string(_s<int>(sPort.c_str())) + std::string("/");
			pc.numCameras = 0;

			// Query the number of cameras this client has
			cpr::Response r = cpr::Get(cpr::Url{ (pc.address + std::string("camera_count")).c_str() });
			if (r.status_code == 200) {
				pc.numCameras = atoi(r.text.c_str());
			}

			g_vPhotronClients.push_back(pc);
		}

		// Now compute number of cameras
		unsigned int numCameras = 0;
		for (int i = 0; i < g_vPhotronClients.size(); i++) {
			numCameras += g_vPhotronClients[i].numCameras;
		}

		return CreateCrowResponse(std::to_string(numCameras).c_str());
	});

	CROW_ROUTE(app, "/api/camera/photron/<int>/<int>")
		([](int camera, int unused) {

		if (!_s<bool>("photron.use_clients")) {

			return CreateCrowResponse(photron::GetLiveImage(camera));

		} else {

			unsigned int relCamIdx = camera;
			for (int i = 0; i < g_vPhotronClients.size(); i++) {
				if (relCamIdx > g_vPhotronClients[i].numCameras) {
					relCamIdx -= g_vPhotronClients[i].numCameras;
				} else {
					std::string url = g_vPhotronClients[i].address +
						std::string("live/") + std::to_string(relCamIdx) + 
						std::string("/") + std::to_string(unused);
					cpr::Response r = cpr::Get(cpr::Url{ url.c_str() });
					if (r.status_code == 200) {
						return CreateCrowResponse(r.text);
					} else {
						return CreateCrowResponse("");
					}
				}
			}
			return CreateCrowResponse("");
		}
	});

	CROW_ROUTE(app, "/api/camera/usb/<int>/<int>")
		([](int camera, int unused) {

		return CreateCrowResponse(usbcamera::GetLiveImage(camera));
	});

	CROW_ROUTE(app, "/api/camera/save")
		([]() {

		std::string prefix = common::GetCommonOutputDirectory() + 
			common::GetTimeStr("manualsave_%Y-%m-%d %H-%M-%S");
		
		hardware::SendTrigger();

		common::SaveToDisk(true, prefix.c_str());

		return CreateCrowResponse("OK");
	});

	CROW_ROUTE(app, "/api/hardware/trigger")
		([]() {

		hardware::SendTrigger(true);

		return CreateCrowResponse("OK");
	});

	CROW_ROUTE(app, "/api/flysim/<string>")
		([](const std::string& str) {

		hardware::SendFlySimCommand(str.c_str());

		return CreateCrowResponse("OK");
	});

	CROW_ROUTE(app, "/api/log/<string>/<int>")
		([](const std::string& query, int start) {

		return CreateCrowResponse(logging::QueryToJSON(query, start));
	});

	CROW_ROUTE(app, "/api/settings/get")
		([]() {

		return CreateCrowResponse(settings::GetSettingsJSON());
	});

	CROW_ROUTE(app, "/api/settings/set")
		.methods("POST"_method)
		([](const crow::request& req) {
		
		settings::SetSettingsJSON(req.body);
		return CreateCrowResponse("ok");
	});

	logging::Log("[SERVER] API Server started on port 1000.");

	app.port(_s<int>("server.port")).multithreaded().run();
}