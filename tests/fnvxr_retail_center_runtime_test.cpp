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

namespace fnvxr::engine::geometry
{
template <std::size_t Capacity>
struct PrivateGeometryCollectorBindingTestAuthority
{
    static void setOriginalAppendOverride(
        PrivateGeometryCollectorBinding<Capacity>& binding,
        std::uintptr_t address) noexcept
    {
        binding.mStorage.originalAppendOverrideForTest = address;
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
    FinishLeft,
    RenderLeft,
    FinalizeLeft,
    EndLeft,
    BindRight,
    AddRight,
    FinishRight,
    RenderRight,
    FinalizeRight,
    EndRight,
    Restore,
};

struct QueueGeometryFixture
{
    std::array<std::uint8_t, 0xA0u> geometry {};
    std::array<std::uint8_t, 0x1Au> property {};

    void initialize() noexcept
    {
#if defined(_MSC_VER) && defined(_M_IX86)
        const abi::RetailPointer32 propertyAddress =
            static_cast<abi::RetailPointer32>(
                reinterpret_cast<std::uintptr_t>(property.data()));
        const std::uint16_t propertyFlags = 0x0001u;
        std::memcpy(
            geometry.data() + 0x9Cu,
            &propertyAddress,
            sizeof(propertyAddress));
        std::memcpy(
            property.data() + 0x18u,
            &propertyFlags,
            sizeof(propertyFlags));
#endif
    }
};

struct FakeState
{
    std::vector<Event> events {};
    Binding* binding = nullptr;
    abi::RetailBSShaderAccumulatorLayout* left = nullptr;
    abi::RetailBSShaderAccumulatorLayout* right = nullptr;
    int accumulatorConstructionCount = 0;
    QueueGeometryFixture geometry[3] {};
    abi::RetailPointer32 stockGeometryPointers[3] {};
    abi::RetailNiVisibleArrayLayout stockVisibleArray {};
    std::uint32_t addCount = 0u;
    std::uint32_t stockAppendCount = 0u;
    abi::RetailNiCameraLayout* cullCamera = nullptr;
    abi::RetailNiCameraLayout* eyeCameras[2] {};
    abi::RetailRendererAccumulatorOwnerLayout renderer {};
    abi::RetailBSShaderAccumulatorLayout stockAccumulator {};
    abi::RetailPointer32 rendererSingleton = 0u;
    abi::RetailPointer32 accumulatingAccumulator = 0u;
    abi::RetailPointer32 renderingAccumulator = 0u;
    abi::RetailPointer32 populationRenderingAccumulator = 0u;
    abi::RetailPointer32 stockCullerVtable = 0u;
};

FakeState* gState = nullptr;

abi::RetailPointer32 accumulatorAddress(
    abi::RetailBSShaderAccumulatorLayout* accumulator)
{
    return static_cast<abi::RetailPointer32>(
        reinterpret_cast<std::uintptr_t>(accumulator));
}

void replaceAccumulatorOwner(
    abi::RetailPointer32& owner,
    abi::RetailBSShaderAccumulatorLayout* accumulator)
{
    const abi::RetailPointer32 replacement = accumulatorAddress(accumulator);
    if (owner == replacement)
        return;
#if defined(_MSC_VER) && defined(_M_IX86)
    if (owner != 0u)
    {
        auto* previous = reinterpret_cast<
            abi::RetailBSShaderAccumulatorLayout*>(
                static_cast<std::uintptr_t>(owner));
        require(previous->referenceCount > 0u,
            "an accumulator owner released an unowned reference");
        --previous->referenceCount;
    }
#endif
    owner = replacement;
#if defined(_MSC_VER) && defined(_M_IX86)
    if (accumulator)
        ++accumulator->referenceCount;
#endif
}

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
#if defined(_MSC_VER) && defined(_M_IX86)
    storage->base.vtable = gState->stockCullerVtable;
#else
    storage->base.vtable = static_cast<abi::RetailPointer32>(
        abi::BSCullingProcessVtableAddress);
#endif
    storage->base.useAppendFunction = 0u;
    return storage;
}

void FNVXR_TEST_THISCALL_IMPL fakeCullerDestroy(
    abi::RetailBSCullingProcessLayout* culler
    FNVXR_TEST_TRAILING_EDX)
{
    require(
        culler && culler->base.vtable == gState->stockCullerVtable,
        "runtime did not restore the stock culler vtable before teardown");
    if (culler->shaderAccumulator != 0u)
    {
        auto* accumulator = reinterpret_cast<
            abi::RetailBSShaderAccumulatorLayout*>(
                static_cast<std::uintptr_t>(culler->shaderAccumulator));
        require(
            accumulator->referenceCount > 0u,
            "culler teardown released an unowned accumulator");
        --accumulator->referenceCount;
        culler->shaderAccumulator = 0u;
    }
}

void FNVXR_TEST_THISCALL_IMPL fakeSetCullerAccumulator(
    abi::RetailBSCullingProcessLayout* culler,
    FNVXR_TEST_EDX_ARGUMENT
    abi::RetailBSShaderAccumulatorLayout* accumulator)
{
    if (culler->shaderAccumulator != 0u)
    {
        auto* previous = reinterpret_cast<
            abi::RetailBSShaderAccumulatorLayout*>(
                static_cast<std::uintptr_t>(
                    culler->shaderAccumulator));
        require(previous->referenceCount > 0u,
            "the runtime culler released an unowned accumulator");
        --previous->referenceCount;
    }
    culler->shaderAccumulator = static_cast<abi::RetailPointer32>(
        reinterpret_cast<std::uintptr_t>(accumulator));
    if (accumulator)
        ++accumulator->referenceCount;
}

void FNVXR_TEST_THISCALL_IMPL fakeRendererSetAccumulator(
    abi::RetailRendererAccumulatorOwnerLayout* renderer,
    FNVXR_TEST_EDX_ARGUMENT
    abi::RetailBSShaderAccumulatorLayout* accumulator)
{
    replaceAccumulatorOwner(renderer->accumulator, accumulator);
}

void FNVXR_TEST_CDECL fakeSetAccumulatingAccumulator(
    abi::RetailBSShaderAccumulatorLayout* accumulator)
{
    replaceAccumulatorOwner(gState->accumulatingAccumulator, accumulator);
}

void FNVXR_TEST_CDECL fakeSetRenderingAccumulator(
    abi::RetailBSShaderAccumulatorLayout* accumulator)
{
    replaceAccumulatorOwner(gState->renderingAccumulator, accumulator);
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
    storage->referenceCount = 0u;
    storage->renderMode = 0u;
    return storage;
}

abi::RetailBSShaderAccumulatorLayout* FNVXR_TEST_THISCALL_IMPL
fakeAccumulatorDestroy(
    abi::RetailBSShaderAccumulatorLayout* accumulator,
    FNVXR_TEST_EDX_ARGUMENT
    std::uint32_t flags)
{
    require(flags == 1u, "runtime changed scalar deleting-destructor flags");
    require(
        accumulator->referenceCount == 0u,
        "runtime did not consume its accumulator ownership reference before destruction");
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
    abi::RetailNiVisibleArrayLayout* visible)
{
    static_cast<void>(culler);
    static_cast<void>(camera);
    static_cast<void>(visible);
    require(false, "post-stock runtime work must not replay ProcessAlt");
}

#if defined(_MSC_VER) && defined(_M_IX86)
void __fastcall fakeOriginalAppend(
    abi::RetailBSCullingProcessLayout* culler,
    void*,
    void*)
{
    require(culler != nullptr, "the stock append wrapper lost its culler");
    ++gState->stockAppendCount;
}
#endif

void FNVXR_TEST_THISCALL_IMPL fakeAddVisible(
    abi::RetailNiAccumulatorLayout*,
    FNVXR_TEST_EDX_ARGUMENT
    abi::RetailNiVisibleArrayLayout*)
{
    require(false, "runtime must not replay AddVisibleArray for private eyes");
}

void FNVXR_TEST_CDECL fakeRender(
    abi::RetailNiCameraLayout* camera,
    abi::RetailBSShaderAccumulatorLayout* accumulator,
    std::uint32_t context)
{
    require(context == RetailWorldRenderContext, "wrong render context");
    fakeSetRenderingAccumulator(accumulator);
    const bool left = accumulator == gState->left;
    require(
        camera == gState->eyeCameras[left ? 0 : 1],
        "render did not receive the accumulator's exact eye camera");
    gState->events.push_back(left ? Event::RenderLeft : Event::RenderRight);
}

void FNVXR_TEST_THISCALL_IMPL fakeFinishAccumulating(
    abi::RetailNiAccumulatorLayout* base
    FNVXR_TEST_TRAILING_EDX)
{
    auto* accumulator =
        reinterpret_cast<abi::RetailBSShaderAccumulatorLayout*>(base);
    const bool left = accumulator == gState->left;
    const abi::RetailPointer32 expected = accumulatorAddress(accumulator);
    require(gState->renderer.accumulator == expected
            && gState->accumulatingAccumulator == expected
            && gState->renderingAccumulator
                == gState->populationRenderingAccumulator,
        "runtime changed accumulator ownership before finish-accumulating");
    gState->events.push_back(
        left ? Event::FinishLeft : Event::FinishRight);
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

void FNVXR_TEST_CDECL fakeAccumulateScene(
    abi::RetailNiCameraLayout* camera,
    void* scene,
    abi::RetailBSCullingProcessLayout* culler)
{
    require(
        camera && scene && culler
            && culler == gState->binding->cullingProcess()
            && culler->base.vtable == gState->stockCullerVtable,
        "runtime did not use its constructor-owned culler for eye accumulation");
    auto* accumulator = reinterpret_cast<
        abi::RetailBSShaderAccumulatorLayout*>(
            static_cast<std::uintptr_t>(culler->shaderAccumulator));
    require(
        accumulator
            && camera
                == gState->eyeCameras[
                    accumulator == gState->left ? 0 : 1]
            && accumulator->shadowScene
                == gState->stockAccumulator.shadowScene,
        "runtime did not inherit the exact stock frame state");
    culler->base.camera = static_cast<abi::RetailPointer32>(
        reinterpret_cast<std::uintptr_t>(camera));
    replaceAccumulatorOwner(gState->renderer.accumulator, accumulator);
    fakeSetAccumulatingAccumulator(accumulator);
    require(
        gState->renderingAccumulator
            == gState->populationRenderingAccumulator,
        "private accumulation changed the prepared stock rendering lane");
    gState->events.push_back(
        accumulator == gState->left ? Event::AddLeft : Event::AddRight);
    ++gState->addCount;
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
    calls.cullingProcessSetAccumulator = engineFunction<
        abi::CullingProcessSetAccumulatorFunction>(
            &fakeSetCullerAccumulator);
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
    calls.accumulateScene = engineFunction<abi::AccumulateSceneFunction>(
        &fakeAccumulateScene);
    calls.accumulatorAddVisibleArray = engineFunction<
        abi::AccumulatorAddVisibleArrayFunction>(&fakeAddVisible);
    calls.accumulatorFinishAccumulating = engineFunction<
        abi::AccumulatorFinishAccumulatingFunction>(
            &fakeFinishAccumulating);
    calls.rendererSetAccumulator = engineFunction<
        abi::RendererSetAccumulatorFunction>(&fakeRendererSetAccumulator);
    calls.setAccumulatingAccumulator = &fakeSetAccumulatingAccumulator;
    calls.setRenderingAccumulator = &fakeSetRenderingAccumulator;
    calls.renderAccumulatorWithoutFinalize = &fakeRender;
    calls.finalizeAccumulator = &fakeFinalize;
    calls.rendererSingleton = &gState->rendererSingleton;
    calls.accumulatingAccumulator = &gState->accumulatingAccumulator;
    calls.renderingAccumulator = &gState->renderingAccumulator;
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
    state.stockAccumulator.referenceCount = 4u;
    state.stockAccumulator.shadowScene = 0x00ABC000u;
    state.rendererSingleton = static_cast<abi::RetailPointer32>(
        reinterpret_cast<std::uintptr_t>(&state.renderer));
    const abi::RetailPointer32 stockAccumulatorAddress =
        static_cast<abi::RetailPointer32>(
            reinterpret_cast<std::uintptr_t>(&state.stockAccumulator));
    state.renderer.accumulator = stockAccumulatorAddress;
    state.accumulatingAccumulator = stockAccumulatorAddress;
    state.renderingAccumulator = stockAccumulatorAddress;
    state.populationRenderingAccumulator = stockAccumulatorAddress;
    auto vtable = stockVtable();
#if defined(_MSC_VER) && defined(_M_IX86)
    state.stockCullerVtable = static_cast<abi::RetailPointer32>(
        reinterpret_cast<std::uintptr_t>(vtable.data()));
#else
    state.stockCullerVtable = static_cast<abi::RetailPointer32>(
        abi::BSCullingProcessVtableAddress);
#endif
    const RetailEngineCallResolution resolution = fakeResolution();
    const bool admitted = true;
    RetailEyeTargetOperations targets {
        &state,
        &snapshot,
        &bind,
        &end,
        &rollback,
        &restore,
        &bind,
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
    state.binding = runtime.resources().collectorBinding();
#if defined(_MSC_VER) && defined(_M_IX86)
    geometry::PrivateGeometryCollectorBindingTestAuthority<Capacity>
        ::setOriginalAppendOverride(
            *state.binding,
            reinterpret_cast<std::uintptr_t>(&fakeOriginalAppend));
#endif
    require(
        runtime.eyeCameraFailure() == RetailEyeCameraFailure::None,
        "successful eye-camera initialization must clear diagnostic failure state");

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
    sceneGraph.opaqueUnknownB8 = 0xDEADBEEFu;
    RetailTrackedFrame uiTracked = trackedFrame(9u, 1);
    uiTracked.runtime.phase = fnvxr::shared::RuntimePhaseMenu;
    const RetailCenterRuntimeFrame uiFrame {
        &sceneObject,
        &sceneGraph,
        &stockCamera,
        uiTracked,
        70.0f,
        1u,
    };
    const RetailCenterRuntimeFrameResult ui = runtime.renderWorld(
        CenterRendererLifecycleTestAuthority::issue(admitted),
        uiFrame);
    require(
        ui.disposition == RetailWorldHookDisposition::RejectGameplayFrame
#if defined(_MSC_VER) && defined(_M_IX86)
            && ui.failure == RetailCenterRuntimeFailure::TrackedFrameRejected
#else
            // The x64 structural test intentionally cannot truncate an
            // arbitrary host pointer into the retail 32-bit camera field.
            // The Win32 branch below proves the tracked-UI rejection.
            && ui.failure == RetailCenterRuntimeFailure::CameraPointerMismatch
#endif
            && state.events.empty(),
        "world-only runtime must reject confirmed UI without interpreting the opaque SceneGraph field");

#if defined(_MSC_VER) && defined(_M_IX86)
    abi::RetailBSCullingProcessLayout stockCuller {};
    stockCuller.base.vtable = state.stockCullerVtable;
    stockCuller.base.useAppendFunction = 0u;
    for (std::size_t index = 0u; index < 3u; ++index)
    {
        state.geometry[index].initialize();
        state.stockGeometryPointers[index] = static_cast<abi::RetailPointer32>(
            reinterpret_cast<std::uintptr_t>(
                state.geometry[index].geometry.data()));
    }
    state.stockVisibleArray = {
        static_cast<abi::RetailPointer32>(
            reinterpret_cast<std::uintptr_t>(state.stockGeometryPointers)),
        3u,
        3u,
        0u,
    };
    stockCuller.base.visibleArray = static_cast<abi::RetailPointer32>(
        reinterpret_cast<std::uintptr_t>(&state.stockVisibleArray));
    fakeSetCullerAccumulator(&stockCuller, nullptr, &state.stockAccumulator);
    sceneGraph.cullingProcess = static_cast<abi::RetailPointer32>(
        reinterpret_cast<std::uintptr_t>(&stockCuller));
    const abi::RetailNiCameraLayout stockBefore = stockCamera;
    const abi::RetailBSCullingProcessLayout stockCullerBefore = stockCuller;
    const RetailCenterRuntimeFrame gameplay {
        &sceneObject,
        &sceneGraph,
        &stockCamera,
        trackedFrame(10u, 2),
        70.0f,
        2u,
        &stockCuller,
    };
    require(
        runtime.beginStockCullerCapture(&stockCuller),
        "runtime did not install its temporary live stock-culler dispatch");
    state.events.push_back(Event::Cull);
    const auto* captureVtable = reinterpret_cast<const abi::RetailPointer32*>(
        static_cast<std::uintptr_t>(stockCuller.base.vtable));
    const auto captureAppend = reinterpret_cast<
        geometry::PrivateGeometryCollectorVslotCallbackFunction<Capacity>>(
            static_cast<std::uintptr_t>(
                captureVtable[Binding::AppendVtableEntryIndex]));
    for (QueueGeometryFixture& geometry : state.geometry)
        captureAppend(&stockCuller, nullptr, geometry.geometry.data());
    const bool captureFinished = runtime.finishStockCullerCapture(&stockCuller);
    require(
        captureFinished
            && stockCuller.base.vtable == state.stockCullerVtable
            && state.stockAppendCount == 3u,
        "runtime did not restore the stock culler and forward each Append call");
    const RetailCenterRuntimeFrameResult stereo = runtime.renderWorld(
        CenterRendererLifecycleTestAuthority::issue(admitted),
        gameplay);
    require(
        stereo.disposition == RetailWorldHookDisposition::StereoWorldComplete
            && stereo.failure == RetailCenterRuntimeFailure::None
            && stereo.renderer.complete
            && stereo.renderer.visibleGeometryCount == 3u,
        "gameplay did not complete one cull and two engine renders");
    require(
        runtime.eyeCameraDiagnostics().complete(),
        "runtime did not retain matching per-eye renderer/culler cameras");
    const std::vector<Event> expected {
        Event::Cull,
        Event::Snapshot,
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
    require(state.events == expected,
        "runtime did not render each eye from its own population");
    require(
        state.addCount == 2u,
        "left and right did not each execute one stock accumulation");
    require(
        state.eyeCameras[0]
            && state.eyeCameras[1]
            && state.eyeCameras[0] != state.eyeCameras[1]
            && state.eyeCameras[0] != &stockCamera
            && state.eyeCameras[1] != &stockCamera,
        "runtime did not use distinct private eye cameras");
    require(
        std::memcmp(&stockCamera, &stockBefore, sizeof(stockCamera)) == 0,
        "runtime camera transaction mutated the stock camera");
    require(
        std::memcmp(
            &stockCuller,
            &stockCullerBefore,
            sizeof(stockCuller)) == 0,
        "runtime did not restore the prepared stock culler after collection");
    require(
        state.stockAccumulator.referenceCount == 5u,
        "runtime did not restore the stock culler accumulator ownership reference");
    require(state.renderer.accumulator == stockAccumulatorAddress
            && state.accumulatingAccumulator == stockAccumulatorAddress
            && state.renderingAccumulator == stockAccumulatorAddress,
        "runtime did not restore the stock renderer accumulator state");

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
