#include <iostream>
#include <openvr.h>
#include <vector>
#include <string>

#include <chrono>
#include <thread>

// just following https://developer.valvesoftware.com/wiki/SteamVR/chaperone_info.vrchap
#define CHAPERONE_HEIGHT 2.4300000667572021

inline void ratelimit_busy_loop() {
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

using namespace vr;

typedef struct point3d {
	float x;
	float y;
	float z;
} point3d;

int main(int argc, char **argv)
{
	// Init OpenVR
	HmdError HmdError;
	IVRSystem* VRSystem = VR_Init(&HmdError, VRApplication_Other);
	if (HmdError != vr::VRInitError_None)
	{
		VRSystem = nullptr;
		std::cout << "Error during OpenVR runtime init: " << VR_GetVRInitErrorAsEnglishDescription(HmdError) << std::endl;
		return -1;
	}


	vr::EVRInitError peError = vr::VRInitError_None;

	if ( !VRCompositor() )
	{
		printf( "Compositor initialization failed. See log file for details\n" );
		return 1;
	}

	ETrackingUniverseOrigin trackingSpace = VRCompositor()->GetTrackingSpace();
	switch (trackingSpace) {
		case TrackingUniverseSeated:
			std::cout << "Calibrating Seated tracking space\n" << std::endl;
			break;
		case TrackingUniverseStanding:
			std::cout << "Calibrating Standing tracking space\n" << std::endl;
			break;
		case TrackingUniverseRawAndUncalibrated:
			std::cout << "Calibrating Uncalibrated tracking space\n" << std::endl;
			break;
		default:
			break;
	}

	TrackedDeviceIndex_t devices[k_unMaxTrackedDeviceCount];

	std::vector<int> controllers;

	std::cout << "Waiting for controllers..." << std::endl;
	while (controllers.size() == 0) {
		for (int i = 0; i < k_unMaxTrackedDeviceCount; i++) {
			ETrackedDeviceClass trackedDeviceClass = VRSystem->GetTrackedDeviceClass(devices[i]);
			if (trackedDeviceClass == TrackedDeviceClass_Controller && VRSystem->IsTrackedDeviceConnected(devices[i])) {
				std::cout << "Found controller " << devices[i] << ", tracked device number " << i << std::endl;
				controllers.push_back(devices[i]);
				break;
			}
		}
		ratelimit_busy_loop();
	}
	std::cout << "Found " << controllers.size() <<  " controllers!" << std::endl;

	VRControllerState_t state;
	uint32_t controller = -1;
	std::cout << "Press trigger with controller you want to use..." << std::endl;
	while (controller == -1) {
		for (uint32_t c: controllers) {
			VRSystem->GetControllerState(c, &state, sizeof(state));
			bool trigger = state.ulButtonPressed & ButtonMaskFromId(k_EButton_SteamVR_Trigger);
			if (trigger) {
				controller = c;
				break;
			}
		}
		ratelimit_busy_loop();
	}

	// now wait until user releases trigger
	while (true) {
		VRSystem->GetControllerState(controller, &state, sizeof(state));
		bool trigger = state.ulButtonPressed & ButtonMaskFromId(k_EButton_SteamVR_Trigger);
		if (!trigger) {
			break;
		}
		ratelimit_busy_loop();
	}
	std::cout << "Okay, you want to use controller " << controller << std::endl;

	std::cout << "Now move the controller to the corners of your play space and press the trigger at each one of them" << std::endl;
	std::cout << "Press option finish, grip to restart" << std::endl;
	TrackedDevicePose_t pose;
	int count = 0;
	std::vector<point3d> points;
	while (true) {
		VRSystem->GetControllerStateWithPose(trackingSpace, controller, &state, sizeof(state), &pose);
		if (state.ulButtonPressed & ButtonMaskFromId(k_EButton_ApplicationMenu)) {
			std::cout << "You pressed the option button, so we finish..." << std::endl;
			break;
		}

		if (state.ulButtonPressed & ButtonMaskFromId(k_EButton_Grip)) {
			std::cout << "You pressed the grip button, so we start from the beginning..." << std::endl;
			count = 0;
			points.clear();
		}

		if (state.ulButtonPressed & ButtonMaskFromId(k_EButton_SteamVR_Trigger)) {
			ratelimit_busy_loop();
			if (!pose.bPoseIsValid) {
				std::cout << "trigger pressed, but pose not valid, try again later" << std::endl;
				continue;
			}
			if (pose.eTrackingResult != TrackingResult_Running_OK) {
				std::cout << "trigger pressed, but tracking result not ok, try again later" << std::endl;
				continue;
			}

			HmdMatrix34_t *p = &pose.mDeviceToAbsoluteTracking;
			point3d pos {
				.x = p->m[0][3],
				.y = p->m[1][3],
				.z = p->m[2][3]
			};

			// now wait until user releases trigger
			while (true) {
				VRSystem->GetControllerState(controller, &state, sizeof(state));
				bool trigger = state.ulButtonPressed & ButtonMaskFromId(k_EButton_SteamVR_Trigger);
				if (!trigger) {
					break;
				}
				ratelimit_busy_loop();
			}

			std::cout << "Your point nr " << count << ": " << pos.x << ", " << pos.y << ", " << pos.z << std::endl;;
			points.push_back(pos);
			count++;
		}
	}

	if (points.size() < 3) {
		std::cout << "Error, you only have " << points.size() << " points, but for chaperone you need at least 3" << std::endl;
		return 1;
	}

	std::cout << "         \"collision_bounds\": [" << std::endl;
	for (int i = 1; i < points.size(); i++) {
		point3d last = points[i-1];
		point3d curr = points[i];

		std::cout << "            " << std::endl;

		std::cout << "               [ " << curr.x << ", " << 0                << ", " << last.z << " ]," << std::endl;
		std::cout << "               [ " << curr.x << ", " << CHAPERONE_HEIGHT << ", " << last.z << " ]," << std::endl;

		std::cout << "               [ " << curr.x << ", " << 0                << ", " << curr.z << " ]," << std::endl;
		std::cout << "               [ " << curr.x << ", " << CHAPERONE_HEIGHT << ", " << curr.z << " ]," << std::endl;

		std::cout << "            ]," << std::endl;
	}

	VR_Shutdown();
}
