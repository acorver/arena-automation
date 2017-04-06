#include "stdafx.h"

#include "server.h"
#include "motion.h"
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

	CROW_ROUTE(app, "/api/camera/photron/<int>/<int>")
		([](int camera, int unused) {

		return CreateCrowResponse(photron::GetLiveImage(camera));
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

		common::SaveToDisk(true);

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

	app.port(1000).multithreaded().run();
}