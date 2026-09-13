#include "../host/fnvxr_world_continuity.h"
#include <cstdlib>
#include <iostream>

int main()
{
    using namespace fnvxr::host;
    int failures = 0;
    const auto expect = [&](bool value, const char* message) {
        if (!value) { std::cerr << message << '\n'; ++failures; }
    };
    WorldContinuityInput input{true, false, true, true, false,
        2000000000LL, 1000000000LL, 0};
    expect(retainSubmittedWorld(input), "native pre-loading stall lost stereo");
    input.runtimeFresh = false;
    input.worldContext = false;
    expect(retainSubmittedWorld(input), "suspended runtime publisher lost accepted eyes");
    input.currentWorldReady = true;
    expect(!retainSubmittedWorld(input), "fresh joined world must replace retained pair");
    input.currentRigCalibrationReady = false;
    expect(retainSubmittedWorld(input),
        "fresh arm pixels without their mount replaced the complete submitted rig");
    input.lifetimeMatches = false;
    expect(!retainSubmittedWorld(input),
        "missing calibration reused a rig across a producer or tracking restart");
    input.lifetimeMatches = true;
    input.currentRigCalibrationReady = true;
    expect(!retainSubmittedWorld(input),
        "recovered exact calibration failed to resume the current world");
    input.currentWorldReady = false;
    input.lifetimeMatches = false;
    expect(!retainSubmittedWorld(input), "tracking or producer restart retained an old pair");
    input.lifetimeMatches = true;
    input.runtimeFresh = true;
    expect(!retainSubmittedWorld(input), "confirmed departure from world context retained eyes");
    input.worldContext = true;
    input.displayTime = 31000000001LL;
    expect(!retainSubmittedWorld(input), "unresponsive producer exceeded suspension bound");
    input.menuActive = true;
    expect(retainSubmittedWorld(input), "live long menu must retain its world");
    input.menuActive = false;
    input.lastMenuDisplayTime = input.displayTime - 500000000LL;
    expect(retainSubmittedWorld(input), "long menu close lost stereo handoff");
    input.displayTime = input.submittedDisplayTime - 1;
    expect(!retainSubmittedWorld(input), "backwards display time retained an old pair");
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
