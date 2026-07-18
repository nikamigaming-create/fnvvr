#include "fnvxr_retail_center_runtime.h"

#include <array>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <new>
#include <vector>

namespace fnvxr::engine
{
struct StereoResourceLifecycleTestAuthority final
{
    static bool validate(const void* evidence) noexcept
    {
        return evidence && *static_cast<const bool*>(evidence);
    }

    static StereoResourceAuthorization issue(const bool& evidence) noexcept
    {
        return StereoResourceAuthorization(&evidence, &validate);
    }
};

struct CenterRendererLifecycleTestAuthority final
{
    static bool validate(const void* evidence) noexcept
    {
        return evidence && *static_cast<const bool*>(evidence);
    }

    static CenterRendererAuthorization issue(const bool& evidence) noexcept
    {
        return CenterRendererAuthorization(&evidence, &validate);
    }
};
}

namespace
{
using namespace fnvxr::engine;
constexpr std::size_t Capacity = 32u;
using Binding = geometry::PrivateGeometryCollectorBinding<Capacity>;

[[noreturn]] void fail(const char* message)
{
    std::cerr << "FAIL: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void require(bool condition, const char* message)
{
    if (!condition)
        fail(message);
}

#if defined(_MSC_VER) && defined(_M_IX86)
#define FNVXR_TEST_CDECL __cdecl
#define FNVXR_TEST_THISCALL_IMPL __fastcall
#define FNVXR_TEST_EDX_ARGUMENT void*,
#define FNVXR_TEST_TRAILING_EDX , void*
#else
#define FNVXR_TEST_CDECL
#define FNVXR_TEST_THISCALL_IMPL
#define FNVXR_TEST_EDX_ARGUMENT
#define FNVXR_TEST_TRAILING_EDX
#endif

enum class Event : std::uint8_t
{
    Cull,
    Snapshot,
    BindLeft,
    AddLeft,
    RenderLeft,
    FinalizeLeft,
    EndLeft,
    BindRight,
    AddRight,
    RenderRight,
    FinalizeRight,
    EndRight,
    Restore,
};

struct FakeState
{
    std::vector<Event> events {};
    abi::RetailBSShaderAccumulatorLayout* left = nullptr;
    abi::RetailBSShaderAccumulatorLayout* right = nullptr;
    int accumulatorConstructionCount = 0;
    int geometry[3] { 1, 2, 3 };
    std::uint32_t addCount = 0u;
    abi::RetailPointer32 visiblePointers[2] {};
    abi::RetailNiCameraLayout* cullCamera = nullptr;
    abi::RetailNiCameraLayout* eyeCameras[2] {};
};

FakeState* gState = nullptr;

void* FNVXR_TEST_CDECL fakeAllocate(std::uint32_t byteCount)
{
    return ::operator new(byteCount, std::nothrow);
}

void FNVXR_TEST_CDECL fakeFree(void* allocation, std::uint32_t)
{
    ::operator delete(allocation);
}

abi::RetailNiCameraLayout* FNVXR_TEST_CDECL fakeCameraCreate()
{
    auto* camera = new (std::nothrow) abi::RetailNiCameraLayout {};
    if (!camera)
        return nullptr;
    const abi::RetailPointer32 vtable = 0x0109CB9Cu;
    std::memcpy(camera, &vtable, sizeof(vtable));
    RetailNiTransformLayout transform {};
    transform.rotation[0] = 1.0f;
    transform.rotation[4] = 1.0f;
    transform.rotation[8] = 1.0f;
    transform.scale = 1.0f;
    detail::retailCameraWriteTransform(
        camera,
        RetailNiAvObjectLocalTransformOffset,
        transform);
    detail::retailCameraWriteTransform(
        camera,
        RetailNiAvObjectWorldTransformOffset,
        transform);
    camera->frustum.left = -1.0f;
    camera->frustum.right = 1.0f;
    camera->frustum.top = 1.0f;
    camera->frustum.bottom = -1.0f;
    camera->frustum.nearDistance = 5.0f;
    camera->frustum.farDistance = 1000.0f;
    camera->viewport.right = 1.0f;
    camera->viewport.top = 1.0f;
    camera->lodAdjust = 1.0f;
    return camera;
}

abi::RetailBSCullingProcessLayout* FNVXR_TEST_THISCALL_IMPL fakeCullerConstruct(
    abi::RetailBSCullingProcessLayout* storage,
    FNVXR_TEST_EDX_ARGUMENT
    std::uint32_t argument)
{
    require(
        argument == RetailWorldCullerConstructorArgument,
        "runtime changed the culler constructor argument");
    storage->base.vtable = static_cast<abi::RetailPointer32>(
        abi::BSCullingProcessVtableAddress);
    return storage;
}

void FNVXR_TEST_THISCALL_IMPL fakeCullerDestroy(
    abi::RetailBSCullingProcessLayout* culler
    FNVXR_TEST_TRAILING_EDX)
{
    require(
        culler && culler->base.vtable == abi::BSCullingProcessVtableAddress,
        "runtime did not restore the stock culler vtable before teardown");
}

abi::RetailBSShaderAccumulatorLayout* FNVXR_TEST_THISCALL_IMPL
fakeAccumulatorConstruct(
    abi::RetailBSShaderAccumulatorLayout* storage,
    FNVXR_TEST_EDX_ARGUMENT
    std::uint32_t mode,
    std::uint32_t batches,
    std::uint32_t passes)
{
    require(
        mode == RetailWorldAccumulatorConstructorMode
            && batches == RetailWorldAccumulatorBatchRendererCount
            && passes == RetailWorldAccumulatorMaximumPassCount,
        "runtime changed the accumulator constructor triple");
    if (gState->accumulatorConstructionCount++ == 0)
        gState->left = storage;
    else
        gState->right = storage;
    storage->vtable = static_cast<abi::RetailPointer32>(
        abi::BSShaderAccumulatorVtableAddress);
    return storage;
}

abi::RetailBSShaderAccumulatorLayout* FNVXR_TEST_THISCALL_IMPL
fakeAccumulatorDestroy(
    abi::RetailBSShaderAccumulatorLayout* accumulator,
    FNVXR_TEST_EDX_ARGUMENT
    std::uint32_t flags)
{
    require(flags == 1u, "runtime changed scalar deleting-destructor flags");
    ::operator delete(accumulator);
    return accumulator;
}

void FNVXR_TEST_THISCALL_IMPL fakeRefObjectFree(
    abi::RetailNiAccumulatorLayout* instance
    FNVXR_TEST_TRAILING_EDX)
{
    delete reinterpret_cast<abi::RetailNiCameraLayout*>(instance);
}

void FNVXR_TEST_THISCALL_IMPL fakeSetCamera(
    abi::RetailNiAccumulatorLayout* accumulator,
    FNVXR_TEST_EDX_ARGUMENT
    abi::RetailNiCameraLayout* camera)
{
    const bool left = reinterpret_cast<abi::RetailBSShaderAccumulatorLayout*>(
        accumulator) == gState->left;
    gState->eyeCameras[left ? 0 : 1] = camera;
    accumulator->camera = static_cast<abi::RetailPointer32>(
        reinterpret_cast<std::uintptr_t>(camera));
}

void FNVXR_TEST_THISCALL_IMPL fakeProcessAlt(
    abi::RetailBSCullingProcessLayout* culler,
    FNVXR_TEST_EDX_ARGUMENT
    abi::RetailNiCameraLayout* camera,
    void*,
    abi::RetailNiVisibleArrayLayout*)
{
    gState->events.push_back(Event::Cull);
    gState->cullCamera = camera;
#if defined(_MSC_VER) && defined(_M_IX86)
    auto* vtable = reinterpret_cast<abi::RetailPointer32*>(
        static_cast<std::uintptr_t>(culler->base.vtable));
    const auto callback = reinterpret_cast<
        geometry::PrivateGeometryCollectorVslotCallbackFunction<Capacity>>(
            static_cast<std::uintptr_t>(
                vtable[Binding::AppendVtableEntryIndex]));
    callback(culler, nullptr, &gState->geometry[0]);
    callback(culler, nullptr, &gState->geometry[1]);
    callback(culler, nullptr, &gState->geometry[2]);
#else
    (void)culler;
#endif
}

void FNVXR_TEST_THISCALL_IMPL fakeAddVisible(
    abi::RetailNiAccumulatorLayout* accumulator,
    FNVXR_TEST_EDX_ARGUMENT
    abi::RetailNiVisibleArrayLayout* visible)
{
    const bool left = reinterpret_cast<abi::RetailBSShaderAccumulatorLayout*>(
        accumulator) == gState->left;
    gState->events.push_back(left ? Event::AddLeft : Event::AddRight);
    require(gState->addCount < 2u, "visible set added more than twice");
    gState->visiblePointers[gState->addCount++] = visible->geometryPointers;
}

void FNVXR_TEST_CDECL fakeRender(
    abi::RetailNiCameraLayout* camera,
    abi::RetailBSShaderAccumulatorLayout* accumulator,
    std::uint32_t context)
{
    require(context == RetailWorldRenderContext, "wrong render context");
    const bool left = accumulator == gState->left;
    require(
        camera == gState->eyeCameras[left ? 0 : 1],
        "render did not receive the accumulator's exact eye camera");
    gState->events.push_back(left ? Event::RenderLeft : Event::RenderRight);
}

void FNVXR_TEST_CDECL fakeFinalize(
    abi::RetailNiCameraLayout* camera,
    abi::RetailBSShaderAccumulatorLayout* accumulator,
    std::uint32_t context)
{
    require(context == RetailWorldRenderContext, "wrong finalize context");
    const bool left = accumulator == gState->left;
    require(
        camera == gState->eyeCameras[left ? 0 : 1],
        "finalize did not receive the accumulator's exact eye camera");
    gState->events.push_back(left ? Event::FinalizeLeft : Event::FinalizeRight);
}

template <typename Target, typename Source>
Target engineFunction(Source source) noexcept
{
    return reinterpret_cast<Target>(source);
}

RetailEngineCallResolution fakeResolution()
{
    RetailEngineCalls calls {};
    calls.niAllocate = &fakeAllocate;
    calls.niFree = &fakeFree;
    calls.niCameraCreate = &fakeCameraCreate;
    calls.cullingProcessConstruct = engineFunction<
        abi::BSCullingProcessConstructorFunction>(&fakeCullerConstruct);
    calls.cullingProcessDestroy = engineFunction<
        abi::BSCullingProcessDestructorBodyFunction>(&fakeCullerDestroy);
    calls.shaderAccumulatorConstruct = engineFunction<
        abi::BSShaderAccumulatorConstructorFunction>(
            &fakeAccumulatorConstruct);
    calls.shaderAccumulatorDestroy = engineFunction<
        abi::BSShaderAccumulatorScalarDestructorFunction>(
            &fakeAccumulatorDestroy);
    calls.niRefObjectFree = engineFunction<abi::NiRefObjectFreeFunction>(
        &fakeRefObjectFree);
    calls.accumulatorSetCamera = engineFunction<
        abi::AccumulatorSetCameraFunction>(&fakeSetCamera);
    calls.cullingProcessAlt = engineFunction<abi::CullingProcessAltFunction>(
        &fakeProcessAlt);
    calls.accumulatorAddVisibleArray = engineFunction<
        abi::AccumulatorAddVisibleArrayFunction>(&fakeAddVisible);
    calls.renderAccumulatorWithoutFinalize = &fakeRender;
    calls.finalizeAccumulator = &fakeFinalize;
    return { calls, RetailEngineCallResolutionFailure::None };
}

std::array<abi::RetailPointer32, Binding::OwnedVtableEntryCount>
stockVtable()
{
    std::array<abi::RetailPointer32, Binding::OwnedVtableEntryCount> result {};
    for (std::size_t index = 0u; index < result.size(); ++index)
    {
        result[index] = static_cast<abi::RetailPointer32>(
            0x00500000u + index * 0x100u);
    }
    result[Binding::AppendVtableEntryIndex] =
        geometry::PrivateGeometryCollectorX86Abi::preferredTargetAddress;
    return result;
}

bool snapshot(void* opaque) noexcept
{
    static_cast<FakeState*>(opaque)->events.push_back(Event::Snapshot);
    return true;
}

bool bind(
    void* opaque,
    CenterRendererEye eye,
    CenterRendererEyeIsolation& isolation) noexcept
{
    static_cast<FakeState*>(opaque)->events.push_back(
        eye == CenterRendererEye::Left
            ? Event::BindLeft
            : Event::BindRight);
    isolation.token = eye == CenterRendererEye::Left ? 1u : 2u;
    return true;
}

bool end(
    void* opaque,
    CenterRendererEye eye,
    CenterRendererEyeIsolation& isolation) noexcept
{
    static_cast<FakeState*>(opaque)->events.push_back(
        eye == CenterRendererEye::Left ? Event::EndLeft : Event::EndRight);
    isolation = {};
    return true;
}

void rollback(
    void*,
    CenterRendererEye,
    CenterRendererEyeIsolation& isolation) noexcept
{
    isolation = {};
}

bool restore(void* opaque) noexcept
{
    static_cast<FakeState*>(opaque)->events.push_back(Event::Restore);
    return true;
}

RetailTrackedFrame trackedFrame(std::uint64_t frameNumber, LONG sequence)
{
    RetailTrackedFrame frame {};
    frame.pose.magic = fnvxr::shared::VrPoseSharedMagic;
    frame.pose.version = fnvxr::shared::VrPoseSharedVersion;
    frame.pose.frame = frameNumber;
    frame.pose.predictedDisplayTime = static_cast<std::int64_t>(frameNumber);
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
    frame.pose.trackingFlags = fnvxr::shared::VrPoseTrackingHmd;
    frame.pose.referenceSpaceGeneration = 1u;
    frame.pose.producerEpoch = 2u;
    frame.runtime.magic = fnvxr::shared::RuntimeSharedMagic;
    frame.runtime.version = fnvxr::shared::RuntimeSharedVersion;
    frame.runtime.frame = frameNumber;
    frame.runtime.phase = fnvxr::shared::RuntimePhaseGameplay;
    frame.runtime.cameraActive = 1u;
    frame.poseSequence = sequence;
    frame.runtimeSequence = sequence;
    return frame;
}
}

int main()
{
    using namespace fnvxr::engine;
    FakeState state;
    gState = &state;
    auto vtable = stockVtable();
    const RetailEngineCallResolution resolution = fakeResolution();
    const bool admitted = true;
    RetailEyeTargetOperations targets {
        &state,
        &snapshot,
        &bind,
        &end,
        &rollback,
        &restore,
    };

    RetailCenterStereoRuntime<Capacity> runtime;
    require(
        runtime.initialize(
            resolution,
            SupportedImageBase,
            vtable.data(),
            100u,
            StereoResourceLifecycleTestAuthority::issue(admitted),
            targets),
        "complete private resources and eye targets did not initialize");
    require(runtime.ready(), "initialized center runtime is not ready");

    int sceneObject = 0;
    abi::RetailNiCameraLayout stockCamera {};
    {
        const abi::RetailPointer32 cameraVtable = 0x0109CB9Cu;
        std::memcpy(&stockCamera, &cameraVtable, sizeof(cameraVtable));
        RetailNiTransformLayout transform {};
        transform.rotation[0] = 1.0f;
        transform.rotation[4] = 1.0f;
        transform.rotation[8] = 1.0f;
        transform.scale = 1.0f;
        detail::retailCameraWriteTransform(
            &stockCamera,
            RetailNiAvObjectLocalTransformOffset,
            transform);
        detail::retailCameraWriteTransform(
            &stockCamera,
            RetailNiAvObjectWorldTransformOffset,
            transform);
        stockCamera.frustum.left = -1.0f;
        stockCamera.frustum.right = 1.0f;
        stockCamera.frustum.top = 1.0f;
        stockCamera.frustum.bottom = -1.0f;
        stockCamera.frustum.nearDistance = 5.0f;
        stockCamera.frustum.farDistance = 1000.0f;
    }
    abi::RetailSceneGraphLayout sceneGraph {};
    sceneGraph.camera = static_cast<abi::RetailPointer32>(
        reinterpret_cast<std::uintptr_t>(&stockCamera));
    sceneGraph.isMenuSceneGraph = 1u;
    const RetailCenterRuntimeFrame uiFrame {
        &sceneObject,
        &sceneGraph,
        &stockCamera,
        {},
        0.0f,
        1u,
    };
    const RetailCenterRuntimeFrameResult ui = runtime.renderWorld(
        CenterRendererLifecycleTestAuthority::issue(admitted),
        uiFrame);
    require(
        ui.disposition == RetailWorldHookDisposition::CallOriginalForUi
            && ui.failure == RetailCenterRuntimeFailure::None
            && state.events.empty(),
        "menu scene must call the original without touching stereo resources");

#if defined(_MSC_VER) && defined(_M_IX86)
    sceneGraph.isMenuSceneGraph = 0u;
    const abi::RetailNiCameraLayout stockBefore = stockCamera;
    const RetailCenterRuntimeFrame gameplay {
        &sceneObject,
        &sceneGraph,
        &stockCamera,
        trackedFrame(10u, 2),
        70.0f,
        2u,
    };
    const RetailCenterRuntimeFrameResult stereo = runtime.renderWorld(
        CenterRendererLifecycleTestAuthority::issue(admitted),
        gameplay);
    require(
        stereo.disposition == RetailWorldHookDisposition::StereoWorldComplete
            && stereo.failure == RetailCenterRuntimeFailure::None
            && stereo.renderer.complete
            && stereo.renderer.visibleGeometryCount == 3u,
        "gameplay did not complete one cull and two engine renders");
    const std::vector<Event> expected {
        Event::Snapshot,
        Event::Cull,
        Event::BindLeft,
        Event::AddLeft,
        Event::RenderLeft,
        Event::FinalizeLeft,
        Event::EndLeft,
        Event::BindRight,
        Event::AddRight,
        Event::RenderRight,
        Event::FinalizeRight,
        Event::EndRight,
        Event::Restore,
    };
    require(state.events == expected, "runtime changed collect/left/right order");
    require(
        state.visiblePointers[0] != 0u
            && state.visiblePointers[0] == state.visiblePointers[1],
        "left and right did not consume the identical visible set");
    require(
        state.cullCamera
            && state.eyeCameras[0]
            && state.eyeCameras[1]
            && state.cullCamera != state.eyeCameras[0]
            && state.cullCamera != state.eyeCameras[1]
            && state.eyeCameras[0] != state.eyeCameras[1]
            && state.cullCamera != &stockCamera
            && state.eyeCameras[0] != &stockCamera
            && state.eyeCameras[1] != &stockCamera,
        "runtime did not use three distinct private cameras");
    require(
        std::memcmp(&stockCamera, &stockBefore, sizeof(stockCamera)) == 0,
        "runtime camera transaction mutated the stock camera");

    const RetailCenterRuntimeFrameResult stale = runtime.renderWorld(
        CenterRendererLifecycleTestAuthority::issue(admitted),
        gameplay);
    require(
        stale.disposition == RetailWorldHookDisposition::RejectGameplayFrame
            && stale.failure == RetailCenterRuntimeFailure::StaleFrameGeneration,
        "replayed frame generation was accepted");
#else
    static_assert(!RetailEngineCallArchitectureSupported);
#endif

    std::cout << "retail center runtime integration passed\n";
    return EXIT_SUCCESS;
}

#undef FNVXR_TEST_CDECL
#undef FNVXR_TEST_THISCALL_IMPL
#undef FNVXR_TEST_EDX_ARGUMENT
#undef FNVXR_TEST_TRAILING_EDX
