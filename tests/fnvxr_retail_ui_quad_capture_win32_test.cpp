#define WIN32_LEAN_AND_MEAN

#include "../renderhook/fnvxr_retail_ui_quad_capture_win32.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace
{
using fnvxr::d3d9::RetailD3D9PresentFunction;

struct TestState
{
    fnvxr::engine::RetailTrackedFrame frame {};
    bool readSucceeds = true;
    bool copySucceeds = true;
    bool publishSucceeds = true;
    unsigned reads = 0u;
    unsigned copies = 0u;
    unsigned publications = 0u;
    unsigned withholds = 0u;
    unsigned presents = 0u;
    IDirect3DDevice9* forwardedDevice = nullptr;
    const RECT* forwardedSource = nullptr;
    const RECT* forwardedDestination = nullptr;
    HWND forwardedWindow = nullptr;
    const RGNDATA* forwardedDirty = nullptr;
    fnvxr::d3d9::RetailUiQuadCaptureFailure lastWithhold =
        fnvxr::d3d9::RetailUiQuadCaptureFailure::None;
};

TestState* gState = nullptr;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

fnvxr::engine::RetailTrackedFrame validFrame(std::uint32_t phase)
{
    using namespace fnvxr;
    engine::RetailTrackedFrame frame {};
    frame.pose.magic = shared::VrPoseSharedMagic;
    frame.pose.version = shared::VrPoseSharedVersion;
    frame.pose.frame = 91u;
    frame.pose.predictedDisplayTime = 1234;
    frame.pose.hmdRot[3] = 1.0f;
    frame.pose.leftEyeRot[3] = 1.0f;
    frame.pose.rightEyeRot[3] = 1.0f;
    frame.pose.leftEyePos[0] = -0.032f;
    frame.pose.rightEyePos[0] = 0.032f;
    const float fov[4] { -0.8f, 0.8f, 0.75f, -0.75f };
    for (std::size_t index = 0u; index < 4u; ++index)
    {
        frame.pose.leftFov[index] = fov[index];
        frame.pose.rightFov[index] = fov[index];
    }
    frame.pose.trackingFlags = shared::VrPoseTrackingHmd;
    frame.pose.referenceSpaceGeneration = 7u;
    frame.pose.producerEpoch = 9u;
    frame.runtime.magic = shared::RuntimeSharedMagic;
    frame.runtime.version = shared::RuntimeSharedVersion;
    frame.runtime.frame = 88u;
    frame.runtime.phase = phase;
    frame.runtime.cameraActive = 1u;
    frame.poseSequence = 2;
    frame.runtimeSequence = 2;
    return frame;
}

bool readFrame(
    void* opaque,
    fnvxr::engine::RetailTrackedFrame& frame) noexcept
{
    auto* state = static_cast<TestState*>(opaque);
    ++state->reads;
    if (!state->readSucceeds)
        return false;
    frame = state->frame;
    return true;
}

bool copyBackBuffer(void* opaque, void* device) noexcept
{
    auto* state = static_cast<TestState*>(opaque);
    ++state->copies;
    return device && state->copySucceeds;
}

bool publishUi(
    void* opaque,
    const fnvxr::engine::RetailTrackedFrame& frame) noexcept
{
    auto* state = static_cast<TestState*>(opaque);
    ++state->publications;
    return frame.runtime.frame == state->frame.runtime.frame
        && state->publishSucceeds;
}

void withholdUi(
    void* opaque,
    fnvxr::d3d9::RetailUiQuadCaptureFailure failure) noexcept
{
    auto* state = static_cast<TestState*>(opaque);
    ++state->withholds;
    state->lastWithhold = failure;
}

HRESULT WINAPI originalPresent(
    IDirect3DDevice9* device,
    const RECT* source,
    const RECT* destination,
    HWND window,
    const RGNDATA* dirty) noexcept
{
    if (!gState)
        return E_UNEXPECTED;
    ++gState->presents;
    gState->forwardedDevice = device;
    gState->forwardedSource = source;
    gState->forwardedDestination = destination;
    gState->forwardedWindow = window;
    gState->forwardedDirty = dirty;
    return S_FALSE;
}

void* entryFromPresent(RetailD3D9PresentFunction function) noexcept
{
    void* entry = nullptr;
    static_assert(sizeof(entry) == sizeof(function));
    std::memcpy(&entry, &function, sizeof(entry));
    return entry;
}

RetailD3D9PresentFunction presentFromEntry(void* entry) noexcept
{
    RetailD3D9PresentFunction function = nullptr;
    static_assert(sizeof(entry) == sizeof(function));
    std::memcpy(&function, &entry, sizeof(function));
    return function;
}

struct FakeDevice
{
    void** vtable = nullptr;
};
}

int main()
{
    using namespace fnvxr;
    TestState state {};
    state.frame = validFrame(shared::RuntimePhaseMenu);
    gState = &state;

    std::array<void*, d3d9::RetailD3D9ExDeviceMethodCount> originalVtable {};
    originalVtable[d3d9::RetailD3D9PresentVtableSlot] =
        entryFromPresent(&originalPresent);
    originalVtable.back() = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(0x12345678u));
    FakeDevice fake { originalVtable.data() };
    FakeDevice other { originalVtable.data() };
    auto* device = reinterpret_cast<IDirect3DDevice9*>(&fake);
    auto* otherDevice = reinterpret_cast<IDirect3DDevice9*>(&other);

    d3d9::RetailUiQuadCaptureOperations operations {};
    operations.context = &state;
    operations.readPublishedFrame = &readFrame;
    operations.copyBackBufferToMonoTargets = &copyBackBuffer;
    operations.publishMonoUiQuad = &publishUi;
    operations.withholdMonoUiQuad = &withholdUi;

    d3d9::RetailUiQuadPresentHookWin32 hook {};
    require(
        hook.initializeAuthorizedDevice(device, operations),
        "authorized per-device Present hook installation failed");
    require(hook.ready(), "installed Present hook is not ready");
    void** privateVtable = fake.vtable;
    require(
        privateVtable != originalVtable.data(),
        "shared D3D vtable was mutated instead of cloning the device vtable");
    require(
        other.vtable == originalVtable.data(),
        "unrelated device vtable pointer changed");
    require(
        privateVtable[d3d9::RetailD3D9ExDeviceMethodCount - 1u]
            == originalVtable.back(),
        "D3D9Ex tail method was not preserved");
    require(
        hook.initializeAuthorizedDevice(device, operations)
            && fake.vtable == privateVtable,
        "Present hook installation was not idempotent");
    require(
        !hook.initializeAuthorizedDevice(otherDevice, operations),
        "installed hook admitted another device");

    RetailD3D9PresentFunction present = presentFromEntry(
        privateVtable[d3d9::RetailD3D9PresentVtableSlot]);
    require(present != nullptr, "private Present entry is null");
    RECT source { 1, 2, 3, 4 };
    RECT destination { 5, 6, 7, 8 };
    RGNDATA dirty {};
    HWND window = reinterpret_cast<HWND>(
        static_cast<std::uintptr_t>(0x4444u));
    require(
        present(device, &source, &destination, window, &dirty) == S_FALSE,
        "original Present result was not forwarded");
    require(
        state.reads == 1u
            && state.copies == 1u
            && state.publications == 1u
            && state.withholds == 0u,
        "confirmed retail UI did not produce exactly one UI capture");
    require(
        state.presents == 1u
            && state.forwardedDevice == device
            && state.forwardedSource == &source
            && state.forwardedDestination == &destination
            && state.forwardedWindow == window
            && state.forwardedDirty == &dirty,
        "Present arguments were not forwarded exactly");

    state.frame = validFrame(shared::RuntimePhaseGameplay);
    require(
        present(device, nullptr, nullptr, nullptr, nullptr) == S_FALSE,
        "gameplay Present was not forwarded");
    require(
        state.reads == 2u
            && state.copies == 1u
            && state.publications == 1u
            && state.withholds == 1u
            && state.lastWithhold
                == d3d9::RetailUiQuadCaptureFailure::RuntimeNotConfirmedUi,
        "gameplay reached the UI backbuffer capture path");

    state.frame = validFrame(shared::RuntimePhaseGameplay);
    state.frame.runtime.menuBits = shared::RuntimeDialogMenuBit;
    require(
        present(device, nullptr, nullptr, nullptr, nullptr) == S_FALSE
            && state.copies == 2u
            && state.publications == 2u,
        "confirmed dialogue was not admitted as MonoUiQuad");

    state.readSucceeds = false;
    require(
        present(device, nullptr, nullptr, nullptr, nullptr) == S_FALSE
            && state.copies == 2u
            && state.publications == 2u
            && state.lastWithhold
                == d3d9::RetailUiQuadCaptureFailure::RuntimeSampleUnavailable,
        "unstable runtime evidence reached UI capture");
    state.readSucceeds = true;
    state.frame = validFrame(shared::RuntimePhaseMenu);
    state.copySucceeds = false;
    require(
        present(device, nullptr, nullptr, nullptr, nullptr) == S_FALSE
            && state.publications == 2u
            && state.lastWithhold
                == d3d9::RetailUiQuadCaptureFailure::BackBufferCopyFailed,
        "failed backbuffer copy was published");
    state.copySucceeds = true;
    state.publishSucceeds = false;
    require(
        present(device, nullptr, nullptr, nullptr, nullptr) == S_FALSE
            && state.lastWithhold
                == d3d9::RetailUiQuadCaptureFailure::PublicationFailed,
        "publication failure suppressed original Present forwarding");

    require(
        hook.detachWhileDeviceAlive(),
        "test device hook did not detach cleanly");
    require(
        fake.vtable == originalVtable.data(),
        "test detach did not restore the original device vtable");

    std::cout << "retail UI-only v5 Present capture passed\n";
    return EXIT_SUCCESS;
}
