#include "fnvxr_retail_tracked_frame_win32.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

template <typename State>
struct Mapping
{
    HANDLE handle = nullptr;
    State* state = nullptr;

    ~Mapping()
    {
        if (state)
            UnmapViewOfFile(state);
        if (handle)
            CloseHandle(handle);
    }
};

template <typename State>
Mapping<State> makeMapping(const char* name)
{
    Mapping<State> result {};
    result.handle = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0u,
        sizeof(State),
        name);
    if (result.handle)
    {
        result.state = static_cast<State*>(MapViewOfFile(
            result.handle,
            FILE_MAP_ALL_ACCESS,
            0u,
            0u,
            sizeof(State)));
    }
    return result;
}
}

int main()
{
    using namespace fnvxr;
#if defined(_WIN32)
    const DWORD pid = GetCurrentProcessId();
    char poseName[96] {};
    char runtimeName[96] {};
    sprintf_s(poseName, "Local\\FNVXR_Test_Pose_%lu", pid);
    sprintf_s(runtimeName, "Local\\FNVXR_Test_Runtime_%lu", pid);
    auto pose = makeMapping<shared::SharedVrPoseState>(poseName);
    auto runtime = makeMapping<shared::SharedRuntimeState>(runtimeName);
    require(pose.state && runtime.state, "test mappings unavailable");

    *pose.state = {};
    pose.state->magic = shared::VrPoseSharedMagic;
    pose.state->version = shared::VrPoseSharedVersion;
    pose.state->frame = 7u;
    pose.state->predictedDisplayTime = 88;
    pose.state->hmdRot[3] = 1.0f;
    pose.state->leftEyeRot[3] = 1.0f;
    pose.state->rightEyeRot[3] = 1.0f;
    pose.state->leftEyePos[0] = -0.032f;
    pose.state->rightEyePos[0] = 0.032f;
    const float fov[4] { -0.8f, 0.8f, 0.75f, -0.75f };
    for (std::size_t index = 0u; index < 4u; ++index)
    {
        pose.state->leftFov[index] = fov[index];
        pose.state->rightFov[index] = fov[index];
    }
    pose.state->trackingFlags = shared::VrPoseTrackingHmd;
    pose.state->referenceSpaceGeneration = 1u;
    pose.state->producerEpoch = 2u;
    pose.state->sequence = 2;

    *runtime.state = {};
    runtime.state->magic = shared::RuntimeSharedMagic;
    runtime.state->version = shared::RuntimeSharedVersion;
    runtime.state->frame = 7u;
    runtime.state->phase = shared::RuntimePhaseGameplay;
    runtime.state->cameraActive = 1u;
    runtime.state->sequence = 2;

    engine::RetailTrackedFrameWin32Reader reader;
    require(reader.initialize(poseName, runtimeName), "reader initialization failed");
    engine::RetailTrackedFrame frame {};
    require(reader.readGameplayFrame(frame), "coherent mapped frame rejected");
    require(
        frame.pose.frame == 7u && frame.runtime.frame == 7u,
        "reader returned wrong mapped identity");

    runtime.state->menuBits = shared::RuntimeDialogMenuBit;
    require(
        reader.readPublishedFrame(frame)
            && engine::validateRetailTrackedUiFrame(frame).complete(),
        "dialogue mapping rejected as stable UI publication");
    require(
        !reader.readGameplayFrame(frame)
            && reader.failure()
                == engine::RetailTrackedFrameFailure::RuntimeNotGameplay,
        "dialogue mapping admitted as gameplay");

    char latePoseName[96] {};
    char lateRuntimeName[96] {};
    sprintf_s(latePoseName, "Local\\FNVXR_Test_Late_Pose_%lu", pid);
    sprintf_s(lateRuntimeName, "Local\\FNVXR_Test_Late_Runtime_%lu", pid);
    engine::RetailTrackedFrameWin32Reader lateReader;
    require(
        !lateReader.initialize(latePoseName, lateRuntimeName)
            && !lateReader.ready(),
        "reader unexpectedly required mappings to pre-exist");
    auto latePose = makeMapping<shared::SharedVrPoseState>(latePoseName);
    auto lateRuntime = makeMapping<shared::SharedRuntimeState>(lateRuntimeName);
    require(latePose.state && lateRuntime.state, "late mappings unavailable");
    *latePose.state = *pose.state;
    *lateRuntime.state = *runtime.state;
    lateRuntime.state->menuBits = 0u;
    require(
        lateReader.readGameplayFrame(frame) && lateReader.ready(),
        "reader did not acquire mappings published after initialization");
#else
    static_assert(!engine::RetailTrackedFrameWin32ReaderAvailable);
#endif
    std::cout << "retail tracked frame Win32 reader passed\n";
    return EXIT_SUCCESS;
}
