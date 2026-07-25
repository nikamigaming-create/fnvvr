#include "fnvxr_retail_center_renderer_operations.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace fnvxr::engine
{
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

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

#if defined(_MSC_VER) && defined(_M_IX86)
namespace
{
constexpr std::size_t CollectorCapacity = 32u;
using Binding = geometry::PrivateGeometryCollectorBinding<CollectorCapacity>;

enum Call : int
{
    Snapshot = 1,
    Cull,
    BindLeft,
    SetLeft,
    AddLeft,
    RenderLeft,
    FinalizeLeft,
    EndLeft,
    BindRight,
    SetRight,
    AddRight,
    RenderRight,
    FinalizeRight,
    EndRight,
    Restore,
};

struct State
{
    std::vector<int> calls;
    abi::RetailBSShaderAccumulatorLayout* left = nullptr;
    abi::RetailBSShaderAccumulatorLayout* right = nullptr;
    abi::RetailPointer32 visiblePointers[2] {};
    std::uint32_t visibleCounts[2] {};
    int addCount = 0;
    int geometry[3] { 11, 22, 33 };
    abi::RetailRendererAccumulatorOwnerLayout renderer {};
    abi::RetailBSShaderAccumulatorLayout stockAccumulator {};
    abi::RetailPointer32 rendererSingleton = 0u;
    abi::RetailPointer32 accumulatingAccumulator = 0u;
    abi::RetailPointer32 renderingAccumulator = 0u;
};

State* gState = nullptr;

void* __cdecl unusedAllocate(std::uint32_t) { return nullptr; }
void __cdecl unusedFree(void*, std::uint32_t) {}
abi::RetailNiCameraLayout* __cdecl unusedCameraCreate() { return nullptr; }
abi::RetailBSCullingProcessLayout* __fastcall unusedCullerConstruct(
    abi::RetailBSCullingProcessLayout*, void*, std::uint32_t) { return nullptr; }
void __fastcall unusedCullerDestroy(
    abi::RetailBSCullingProcessLayout*, void*) {}
void __fastcall fakeSetCullerAccumulator(
    abi::RetailBSCullingProcessLayout* culler,
    void*,
    abi::RetailBSShaderAccumulatorLayout* accumulator)
{
    if (culler->shaderAccumulator != 0u)
    {
        auto* previous = reinterpret_cast<
            abi::RetailBSShaderAccumulatorLayout*>(
                static_cast<std::uintptr_t>(
                    culler->shaderAccumulator));
        require(previous->referenceCount > 0u,
            "the culler released an unowned accumulator");
        --previous->referenceCount;
    }
    culler->shaderAccumulator = static_cast<abi::RetailPointer32>(
        reinterpret_cast<std::uintptr_t>(accumulator));
    if (accumulator)
        ++accumulator->referenceCount;
}
void __fastcall fakeRendererSetAccumulator(
    abi::RetailRendererAccumulatorOwnerLayout* renderer,
    void*,
    abi::RetailBSShaderAccumulatorLayout* accumulator)
{
    renderer->accumulator = static_cast<abi::RetailPointer32>(
        reinterpret_cast<std::uintptr_t>(accumulator));
}
void __cdecl fakeSetAccumulatingAccumulator(
    abi::RetailBSShaderAccumulatorLayout* accumulator)
{
    gState->accumulatingAccumulator = static_cast<abi::RetailPointer32>(
        reinterpret_cast<std::uintptr_t>(accumulator));
}
void __cdecl fakeSetRenderingAccumulator(
    abi::RetailBSShaderAccumulatorLayout* accumulator)
{
    gState->renderingAccumulator = static_cast<abi::RetailPointer32>(
        reinterpret_cast<std::uintptr_t>(accumulator));
}
abi::RetailBSShaderAccumulatorLayout* __fastcall unusedAccumulatorConstruct(
    abi::RetailBSShaderAccumulatorLayout*,
    void*,
    std::uint32_t,
    std::uint32_t,
    std::uint32_t) { return nullptr; }
abi::RetailBSShaderAccumulatorLayout* __fastcall unusedAccumulatorDestroy(
    abi::RetailBSShaderAccumulatorLayout*, void*, std::uint32_t) { return nullptr; }
void __fastcall unusedRefFree(abi::RetailNiAccumulatorLayout*, void*) {}

void __fastcall fakeSetCamera(
    abi::RetailNiAccumulatorLayout* accumulator,
    void*,
    abi::RetailNiCameraLayout* camera)
{
    auto* typed = reinterpret_cast<abi::RetailBSShaderAccumulatorLayout*>(
        accumulator);
    const bool left = typed == gState->left;
    gState->calls.push_back(left ? SetLeft : SetRight);
    typed->camera = static_cast<abi::RetailPointer32>(
        reinterpret_cast<std::uintptr_t>(camera));
}

void __fastcall fakeProcessAlt(
    abi::RetailBSCullingProcessLayout* culler,
    void*,
    abi::RetailNiCameraLayout*,
    void*,
    abi::RetailNiVisibleArrayLayout*)
{
    gState->calls.push_back(Cull);
    require(
        culler->shaderAccumulator
            == static_cast<abi::RetailPointer32>(
                reinterpret_cast<std::uintptr_t>(gState->left)),
        "the private culler must hold the collection accumulator during traversal");
    auto* vtable = reinterpret_cast<abi::RetailPointer32*>(
        static_cast<std::uintptr_t>(culler->base.vtable));
    const auto callback = reinterpret_cast<
        geometry::PrivateGeometryCollectorVslotCallbackFunction<
            CollectorCapacity>>(
                static_cast<std::uintptr_t>(
                    vtable[Binding::AppendVtableEntryIndex]));
    callback(culler, nullptr, &gState->geometry[0]);
    callback(culler, nullptr, &gState->geometry[1]);
    callback(culler, nullptr, &gState->geometry[2]);
}

void __fastcall fakeAddVisible(
    abi::RetailNiAccumulatorLayout* accumulator,
    void*,
    abi::RetailNiVisibleArrayLayout* visible)
{
    auto* typed = reinterpret_cast<abi::RetailBSShaderAccumulatorLayout*>(
        accumulator);
    const bool left = typed == gState->left;
    const abi::RetailPointer32 expected = static_cast<abi::RetailPointer32>(
        reinterpret_cast<std::uintptr_t>(typed));
    require(gState->renderer.accumulator == expected
            && gState->accumulatingAccumulator == expected
            && gState->renderingAccumulator == expected,
        "the eye accumulator was not fully registered before visible admission");
    gState->calls.push_back(left ? AddLeft : AddRight);
    const int index = gState->addCount++;
    gState->visiblePointers[index] = visible->geometryPointers;
    gState->visibleCounts[index] = visible->itemCount;
}

void __cdecl fakeRender(
    abi::RetailNiCameraLayout*,
    abi::RetailBSShaderAccumulatorLayout* accumulator,
    std::uint32_t context)
{
    require(context == RetailWorldRenderContext, "render used the wrong retail context");
    fakeSetRenderingAccumulator(accumulator);
    gState->calls.push_back(accumulator == gState->left ? RenderLeft : RenderRight);
}

void __cdecl fakeFinalize(
    abi::RetailNiCameraLayout*,
    abi::RetailBSShaderAccumulatorLayout* accumulator,
    std::uint32_t context)
{
    require(context == RetailWorldRenderContext, "finalize used the wrong retail context");
    gState->calls.push_back(accumulator == gState->left ? FinalizeLeft : FinalizeRight);
}

template <typename Target, typename Source>
Target asEngineFunction(Source source) noexcept
{
    return reinterpret_cast<Target>(source);
}

RetailEngineCalls fakeCalls()
{
    RetailEngineCalls calls {};
    calls.niAllocate = &unusedAllocate;
    calls.niFree = &unusedFree;
    calls.niCameraCreate = &unusedCameraCreate;
    calls.cullingProcessConstruct = asEngineFunction<
        abi::BSCullingProcessConstructorFunction>(&unusedCullerConstruct);
    calls.cullingProcessDestroy = asEngineFunction<
        abi::BSCullingProcessDestructorBodyFunction>(&unusedCullerDestroy);
    calls.cullingProcessSetAccumulator = asEngineFunction<
        abi::CullingProcessSetAccumulatorFunction>(
            &fakeSetCullerAccumulator);
    calls.shaderAccumulatorConstruct = asEngineFunction<
        abi::BSShaderAccumulatorConstructorFunction>(
            &unusedAccumulatorConstruct);
    calls.shaderAccumulatorDestroy = asEngineFunction<
        abi::BSShaderAccumulatorScalarDestructorFunction>(
            &unusedAccumulatorDestroy);
    calls.niRefObjectFree = asEngineFunction<abi::NiRefObjectFreeFunction>(
        &unusedRefFree);
    calls.accumulatorSetCamera = asEngineFunction<
        abi::AccumulatorSetCameraFunction>(&fakeSetCamera);
    calls.cullingProcessAlt = asEngineFunction<abi::CullingProcessAltFunction>(
        &fakeProcessAlt);
    calls.accumulatorAddVisibleArray = asEngineFunction<
        abi::AccumulatorAddVisibleArrayFunction>(&fakeAddVisible);
    calls.rendererSetAccumulator = asEngineFunction<
        abi::RendererSetAccumulatorFunction>(&fakeRendererSetAccumulator);
    calls.setAccumulatingAccumulator = &fakeSetAccumulatingAccumulator;
    calls.setRenderingAccumulator = &fakeSetRenderingAccumulator;
    calls.renderAccumulatorWithoutFinalize = &fakeRender;
    calls.finalizeAccumulator = &fakeFinalize;
    calls.rendererSingleton = &gState->rendererSingleton;
    calls.accumulatingAccumulator = &gState->accumulatingAccumulator;
    calls.renderingAccumulator = &gState->renderingAccumulator;
    return calls;
}

bool snapshotTargets(void* opaque) noexcept
{
    static_cast<State*>(opaque)->calls.push_back(Snapshot);
    return true;
}

bool bindTargets(
    void* opaque,
    CenterRendererEye eye,
    CenterRendererEyeIsolation& isolation) noexcept
{
    auto* state = static_cast<State*>(opaque);
    state->calls.push_back(
        eye == CenterRendererEye::Left ? BindLeft : BindRight);
    isolation.token = eye == CenterRendererEye::Left ? 0x111u : 0x222u;
    return true;
}

bool endTargets(
    void* opaque,
    CenterRendererEye eye,
    CenterRendererEyeIsolation& isolation) noexcept
{
    auto* state = static_cast<State*>(opaque);
    state->calls.push_back(
        eye == CenterRendererEye::Left ? EndLeft : EndRight);
    isolation = {};
    return true;
}

void rollbackTargets(
    void*,
    CenterRendererEye,
    CenterRendererEyeIsolation& isolation) noexcept
{
    isolation = {};
}

bool restoreTargets(void* opaque) noexcept
{
    static_cast<State*>(opaque)->calls.push_back(Restore);
    return true;
}
}
#endif

int main()
{
#if !defined(_MSC_VER) || !defined(_M_IX86)
    static_assert(!RetailEngineCallArchitectureSupported);
    std::cout << "retail center renderer operations compile gate passed (non-x86 audit build)\n";
    return EXIT_SUCCESS;
#else
    State state;
    gState = &state;

    Binding binding;
    std::array<abi::RetailPointer32, Binding::OwnedVtableEntryCount> stock {};
    for (std::size_t index = 0u; index < stock.size(); ++index)
        stock[index] = static_cast<abi::RetailPointer32>(0x00500000u + index * 0x100u);
    stock[Binding::AppendVtableEntryIndex] =
        geometry::PrivateGeometryCollectorX86Abi::preferredTargetAddress;
    binding.cullingProcess()->base.vtable =
        static_cast<abi::RetailPointer32>(abi::BSCullingProcessVtableAddress);
    require(
        binding.installOwnedVtableClone(
            stock.data(),
            stock.size(),
            static_cast<abi::RetailPointer32>(abi::BSCullingProcessVtableAddress),
            static_cast<abi::RetailPointer32>(
                reinterpret_cast<std::uintptr_t>(&binding)),
            static_cast<abi::RetailPointer32>(
                reinterpret_cast<std::uintptr_t>(
                    &geometry::privateGeometryCollectorVslotCallback<
                        CollectorCapacity>)))
            == geometry::GeometryVtableInstallResult::Installed,
        "the test private culler clone did not install");

    abi::RetailNiCameraLayout camera {};
    abi::RetailNiCameraLayout leftCamera {};
    abi::RetailNiCameraLayout rightCamera {};
    abi::RetailBSShaderAccumulatorLayout left {};
    abi::RetailBSShaderAccumulatorLayout right {};
    left.referenceCount = 1u;
    right.referenceCount = 1u;
    state.left = &left;
    state.right = &right;
    state.stockAccumulator.referenceCount = 4u;
    state.rendererSingleton = static_cast<abi::RetailPointer32>(
        reinterpret_cast<std::uintptr_t>(&state.renderer));
    const abi::RetailPointer32 stockAccumulatorAddress =
        static_cast<abi::RetailPointer32>(
            reinterpret_cast<std::uintptr_t>(&state.stockAccumulator));
    state.renderer.accumulator = stockAccumulatorAddress;
    state.accumulatingAccumulator = stockAccumulatorAddress;
    state.renderingAccumulator = stockAccumulatorAddress;

    RetailEyeTargetOperations targets {
        &state,
        &snapshotTargets,
        &bindTargets,
        &endTargets,
        &rollbackTargets,
        &restoreTargets,
    };
    RetailCenterRendererOperationsContext<CollectorCapacity> context;
    require(
        context.initialize(fakeCalls(), binding, left, targets),
        "complete engine calls, private culler, and targets must initialize");
    const CenterRendererOperations operations =
        makeRetailCenterRendererOperations(context);
    require(centerRendererOperationsComplete(operations),
        "the concrete center renderer operation table must be complete");

    int scene = 0;
    const bool admitted = true;
    const CenterRendererResult result = executeCenterRendererFrame(
        operations,
        CenterRendererLifecycleTestAuthority::issue(admitted),
        {
            &scene,
            &camera,
            &leftCamera,
            &rightCamera,
            binding.cullingProcess(),
            &left,
            &right,
            0x1234u,
        });
    require(result.complete
            && result.failure == CenterRendererFailure::None
            && result.visibleGeometryCount == 3u
            && result.visibleSetGeneration == 0x1234u,
        "the concrete retail center/center transaction did not complete");

    const std::vector<int> expected {
        Snapshot,
        SetLeft,
        Cull,
        BindLeft,
        SetLeft,
        AddLeft,
        RenderLeft,
        FinalizeLeft,
        EndLeft,
        BindRight,
        SetRight,
        AddRight,
        RenderRight,
        FinalizeRight,
        EndRight,
        Restore,
    };
    require(state.calls == expected,
        "the concrete renderer did not preserve collect-once then left/right order");
    require(
        binding.cullingProcess()->shaderAccumulator == 0u,
        "the collection accumulator reference must be released after traversal");
    require(
        left.referenceCount == 1u,
        "the scoped culler binding must preserve the owner's accumulator reference");
    require(state.renderer.accumulator == stockAccumulatorAddress
            && state.accumulatingAccumulator == stockAccumulatorAddress
            && state.renderingAccumulator == stockAccumulatorAddress,
        "the private eye transaction did not restore every stock accumulator owner");
    require(state.visiblePointers[0] != 0u
            && state.visiblePointers[0] == state.visiblePointers[1]
            && state.visibleCounts[0] == 3u
            && state.visibleCounts[1] == 3u,
        "both accumulators must consume the identical sealed visible array");
    require(binding.phase() == geometry::GeometryCollectorPhase::Inactive
            && binding.ownedVtableCloneInstalled()
            && binding.ownedVtableIntegrityValid(),
        "frame discard must clear geometry while preserving private dispatch ownership");

    std::cout << "retail center renderer operations passed\n";
    return EXIT_SUCCESS;
#endif
}
