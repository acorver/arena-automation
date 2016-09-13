#include "stdafx.h"

#include "server.h"
#include "photron.h"
#include "hardware.h"
#include "usbcamera.h"
#include "settings.h"

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
	app.get_middleware<server::DocumentMiddleWare>().setDocumentRoot("/www/");

	CROW_ROUTE(app, "/")
		([]() {
		return CreateCrowResponse("This server reports the status of ongoing experiments in the dragonfly arena.");
	});

	CROW_ROUTE(app, "/api/camera/photron/<int>")
		([](int camera) {

		return CreateCrowResponse(photron::GetLiveImage(camera));
	});

	CROW_ROUTE(app, "/api/camera/usb/<int>")
		([](int camera) {

		return CreateCrowResponse(usbcamera::GetLiveImage(camera));
	});

	CROW_ROUTE(app, "/api/camera/save")
		([]() {

		std::string prefix = common::GetTimeStr("./data/manualsave_%Y-%m-%d %H-%M-%S_");
		
		photron::Save(prefix,1,0);

		return CreateCrowResponse("OK");
	});

	CROW_ROUTE(app, "/api/hardware/trigger")
		([]() {

		hardware::SendTrigger();

		return CreateCrowResponse("OK");
	});

	CROW_ROUTE(app, "/api/log")
		([]() {
		// return the entire cache, in JSON format (TODO)
		std::string s = "{\"log\": [";

		for (int i = 0; i < logging::GetCacheSize(); i++) {
			s += "\"" +logging::GetCache(i) + "\"";
			if (i < logging::GetCacheSize() - 1) { s += ','; }
		}

		return CreateCrowResponse(s + "]}");
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

	//app.port(8000).multithreaded().run();
	app.port(1000).multithreaded().run();
}