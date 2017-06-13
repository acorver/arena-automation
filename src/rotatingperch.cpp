#include "stdafx.h"
#include "rotatingperch.h"

namespace rotatingperch {

	using namespace linalg;
	using namespace linalg::aliases;

	typedef struct Perch {
		int   mMotorIdx;
		float mTargetAngle;
		float mTargetAngleWithoutDf;
		int   mRotateDir;

		Perch() { Perch(0, 0, 0, 1); }
		Perch(int m, float a, float a2, int d) {
			mMotorIdx = m;
			mTargetAngle = a;
			mTargetAngleWithoutDf = a2;
			mRotateDir = d;
		}
	} Perch;

	// Variables
	std::map<std::string, Perch> g_PerchLocations;
	boost::posix_time::ptime g_LastPerchUpdate;

	// Determine the location of each perch
	void Init() { 

		for (int i = 0; i < 1024; i++) {

			std::string bodyName = _ss<std::string>((std::string("rotatingperch.perch_") +
				std::to_string(i) + std::string("_body_name")).c_str());

			int motorIdx = _ss<int>((std::string("rotatingperch.perch_") +
				std::to_string(i) + std::string("_motor_idx")).c_str());

			float targetAngle = float(_ss<double>((std::string("rotatingperch.perch_") +
				std::to_string(i) + std::string("_target_angle")).c_str())) * PI / 180;
			float targetAngleWithoutDf = float(_ss<double>((std::string("rotatingperch.perch_") +
				std::to_string(i) + std::string("_target_angle_without_df")).c_str())) * PI / 180;

			// Convert target angle to [-PI,PI] range
			while (targetAngle >  PI) { targetAngle -= 2 * PI; }
			while (targetAngle < -PI) { targetAngle += 2 * PI; }
			while (targetAngleWithoutDf >  PI) { targetAngleWithoutDf -= 2 * PI; }
			while (targetAngleWithoutDf < -PI) { targetAngleWithoutDf += 2 * PI; }

			int rotateDir = _ss<int>((std::string("rotatingperch.perch_") +
				std::to_string(i) + std::string("_rotate_dir")).c_str());

			if ( bodyName != "") {
				g_PerchLocations[bodyName] = Perch(motorIdx, 
					targetAngle, targetAngleWithoutDf, rotateDir);
			} else {
				break;
			}
		}

		hardware::SetPerchSpeed(
			_s<int>("rotatingperch.speed_min"),
			_s<int>("rotatingperch.speed_max")
		);
	}

	// update perch rotation
	void updatePerch(motion::CortexFrame *pCortexFrame) {

		// Enforce minimum processing time to avoid crashes due to 
		// simultaneous writes to Serial interface
		boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
		if ((now - g_LastPerchUpdate).total_milliseconds() < 
			_s<int>("rotatingperch.min_update_delay_ms")) {
			return;
		}
		g_LastPerchUpdate = now;

		sFrameOfData* frameOfData = pCortexFrame->pFrame;

		// Get dF positions
		std::vector<std::vector<float3>> dfs;
		for (int j = 0; j < frameOfData->nBodies; j++) {
			if (!boost::algorithm::ifind_first(std::string(frameOfData->BodyData[j].szName), 
				_s<std::string>("tracking.body_name")).empty()) {

				// IMPORTANT NOTE: The 1,2,0 order is dependent on the particular Yframe
				// markerset, and the indices of the markers... The current Yframe 
				// markersets have the following marker order: BR, BF, BL
				float3 df = float3(frameOfData->BodyData[j].Markers[1]);
				float3 dl = float3(frameOfData->BodyData[j].Markers[2]);
				float3 dr = float3(frameOfData->BodyData[j].Markers[0]);

				if (dr[0] == CORTEX_INVALID_MARKER) { dr = dl; }
				if (dl[0] == CORTEX_INVALID_MARKER) { dl = dr; }
				if (df[0] == CORTEX_INVALID_MARKER) { df = 0.5f * (dl + dr) - cross(dr - dl, float3(0, 0, 1)); }

				float3 dc = 0.5f * (df + 0.5f * (dl + dr));
				dfs.push_back({ dc, df, dl, dr });
			}
		}

		// Align each perch
		bool anyDfReady = false;
		for (auto const &perch : g_PerchLocations) {

			// 
			for (int i = 0; i < frameOfData->nBodies; i++) {

				// Perch locations
				float3 pf = float3(frameOfData->BodyData[i].Markers[0]);
				float3 pl = float3(frameOfData->BodyData[i].Markers[1]);
				float3 pr = float3(frameOfData->BodyData[i].Markers[2]);
				float3 pc = 0.5f * (pf + 0.5f * (pl + pr));

				// Tracking triangle (assume perch, unless dF present)
				float3 tf = pf, tl = pl, tr = pr;

				// Is a dragonfly on the perch?
				bool isDfOnPerch = false;
				for (auto const &df : dfs) {
					if (length(df[0] - pc) < _s<double>("rotatingperch.max_on_perch_dist")) {
						tf = df[1];
						tl = df[2];
						tr = df[3];
						isDfOnPerch = true;
						break;
					}
				}

				// Align to either dF or perch
				if (std::string(frameOfData->BodyData[i].szName) == perch.first) {

					float3 d = tf - 0.5f * (tl + tr); d[2] = 0;
					d = normalize(d);
					float a = std::atan2(d[1], d[0]);
					float aTarget = (isDfOnPerch ? perch.second.mTargetAngle : perch.second.mTargetAngleWithoutDf);
					float diff = a - aTarget;

					// Optional debug output
					if (_s<bool>("rotatingperch.debug")) {
						std::cout << "Perch " << perch.first <<
							"'s orientation is " << a * 180 / PI <<
							(isDfOnPerch ? " (dF on perch)" : "") << " (Target is " << (aTarget * 180 / PI) << ")\n";
					}

					if (abs(diff) < _s<double>("rotatingperch.max_orientation_error") * PI / 180) {
						hardware::RotatePerch(perch.second.mMotorIdx, 0);

						// Notify other system components that a dF is on the perch, and has been aligned:
						if (isDfOnPerch) {
							anyDfReady = true;
						}
					}
					else if (diff > 0) {
						hardware::RotatePerch(perch.second.mMotorIdx, -1 * perch.second.mRotateDir);
					}
					else {
						hardware::RotatePerch(perch.second.mMotorIdx, 1 * perch.second.mRotateDir);
					}
				}
			}
		}

		// Notify other systems that at least one dF is aligned on the perch
		hardware::NotifyDfOnPerch(anyDfReady);

		std::cout << "Rotating perch update took: " << (boost::posix_time::second_clock::local_time() - now).total_milliseconds() << 
			" milliseconds" << (anyDfReady?" (dF ready)":"") << ".\n";
	}
}