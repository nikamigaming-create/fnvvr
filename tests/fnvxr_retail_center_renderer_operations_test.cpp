#include "fnvxr_retail_center_renderer_operations.h"

#include <array>
#include <cstring>
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
    Binding* binding = nullptr;
    abi::RetailBSShaderAccumulatorLayout* left = nullptr;
    abi::RetailBSShaderAccumulatorLayout* right = nullptr;
    abi::RetailPointer32 visiblePointers[2] {};
    std::uint32_t visibleCounts[2] {};
    int addCount = 0;
    int stockAppendCount = 0;
    int geometry[3] { 11, 22, 33 };
    abi::RetailPointer32 stockGeometryPointers[3] {};
    abi::RetailNiVisibleArrayLayout stockVisibleArray {};
    abi::RetailRendererAccumulatorOwnerLayout renderer {};
    abi::RetailBSShaderAccumulatorLayout stockAccumulator {};
    abi::RetailPointer32 rendererSingleton = 0u;
    abi::RetailPointer32 accumulatingAccumulator = 0u;
    abi::RetailPointer32 renderingAccumulator = 0u;
};

State* gState = nullptr;

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
    if (owner != 0u)
    {
        auto* previous = reinterpret_cast<
            abi::RetailBSShaderAccumulatorLayout*>(
                static_cast<std::uintptr_t>(owner));
        require(previous->referenceCount > 0u,
            "an accumulator owner released an unowned reference");
        --previous->referenceCount;
    }
    owner = replacement;
    if (accumulator)
        ++accumulator->referenceCount;
}

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
    replaceAccumulatorOwner(renderer->accumulator, accumulator);
}
void __cdecl fakeSetAccumulatingAccumulator(
    abi::RetailBSShaderAccumulatorLayout* accumulator)
{
    replaceAccumulatorOwner(gState->accumulatingAccumulator, accumulator);
}
void __cdecl fakeSetRenderingAccumulator(
    abi::RetailBSShaderAccumulatorLayout* accumulator)
{
    replaceAccumulatorOwner(gState->renderingAccumulator, accumulator);
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
    abi::RetailNiVisibleArrayLayout* visible)
{
    static_cast<void>(culler);
    static_cast<void>(visible);
    require(false, "post-stock collection must not replay ProcessAlt");
}

void __fastcall fakeOriginalAppend(
    abi::RetailBSCullingProcessLayout* culler,
    void*,
    void*)
{
    require(culler != nullptr, "the stock append wrapper lost its culler");
    ++gState->stockAppendCount;
}

void __cdecl fakeAccumulateScene(
    abi::RetailNiCameraLayout*,
    void*,
    abi::RetailBSCullingProcessLayout*)
{
    require(false, "the collector cull must not call AccumulateScene");
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
    calls.accumulateScene = asEngineFunction<abi::AccumulateSceneFunction>(
        &fakeAccumulateScene);
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
    const abi::RetailPointer32 stockVtableAddress =
        static_cast<abi::RetailPointer32>(
            reinterpret_cast<std::uintptr_t>(stock.data()));
    binding.cullingProcess()->base.vtable = stockVtableAddress;
    binding.cullingProcess()->base.useAppendFunction = 0u;
    abi::RetailBSCullingProcessLayout stockCuller {};
    stockCuller.base.vtable = stockVtableAddress;
    stockCuller.base.useAppendFunction = 0u;
    for (std::size_t index = 0u; index < 3u; ++index)
    {
        state.stockGeometryPointers[index] = static_cast<abi::RetailPointer32>(
            reinterpret_cast<std::uintptr_t>(&state.geometry[index]));
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
    fakeSetCullerAccumulator(&stockCuller, nullptr, &state.stockAccumulator);
    const abi::RetailBSCullingProcessLayout stockCullerSnapshot = stockCuller;

    RetailEyeTargetOperations targets {
        &state,
        &snapshotTargets,
        &bindTargets,
        &endTargets,
        &rollbackTargets,
        &restoreTargets,
    };
    RetailCenterRendererOperationsContext<CollectorCapacity> context;
    state.binding = &binding;
    require(
        context.initialize(fakeCalls(), binding, left, stock.data(), targets),
        "complete engine calls, private culler, and targets must initialize");
    geometry::PrivateGeometryCollectorBindingTestAuthority<CollectorCapacity>
        ::setOriginalAppendOverride(
            binding,
            reinterpret_cast<std::uintptr_t>(&fakeOriginalAppend));
    const CenterRendererOperations operations =
        makeRetailCenterRendererOperations(context);
    require(centerRendererOperationsComplete(operations),
        "the concrete center renderer operation table must be complete");

    int scene = 0;
    const bool admitted = true;
    require(
        context.beginStockCullerCapture(&stockCuller),
        "the exact stock culler did not accept the temporary owned dispatch");
    state.calls.push_back(Cull);
    const auto* captureVtable = reinterpret_cast<const abi::RetailPointer32*>(
        static_cast<std::uintptr_t>(stockCuller.base.vtable));
    const auto captureAppend = reinterpret_cast<
        geometry::PrivateGeometryCollectorVslotCallbackFunction<CollectorCapacity>>(
            static_cast<std::uintptr_t>(
                captureVtable[Binding::AppendVtableEntryIndex]));
    for (int& geometry : state.geometry)
        captureAppend(&stockCuller, nullptr, &geometry);
    const bool captureFinished = context.finishStockCullerCapture(&stockCuller);
    require(
        captureFinished
            && stockCuller.base.vtable == stockVtableAddress
            && state.stockAppendCount == 3,
        "the forwarding capture did not restore the stock culler and Append call");
    const CenterRendererResult result = executeCenterRendererFrame(
        operations,
        CenterRendererLifecycleTestAuthority::issue(admitted),
        {
            &scene,
            &camera,
            &leftCamera,
            &rightCamera,
            &stockCuller,
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
        Cull,
        Snapshot,
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
    const RetailCenterVisibilityDiagnostics completedVisibility =
        context.visibilityDiagnostics();
    require(
        completedVisibility.captured
            && completedVisibility.failure
                == RetailCenterVisibilityFailure::None
            && completedVisibility.collectionAccumulatorReferenceCount
                == 1u
            && completedVisibility.sealedItemCount == 3u
            && completedVisibility.sealedGeneration == 1u,
        "a completed stock traversal capture did not expose exact diagnostics");
    CenterRendererVisibleSet rejectedVisibleSet {};
    require(
        !operations.collectConservativeVisibleSet(
            operations.context,
            nullptr,
            &scene,
            &stockCuller,
            0x1235u,
            rejectedVisibleSet),
        "a null center camera reached the private culler");
    require(
        context.visibilityDiagnostics().captured
            && context.visibilityDiagnostics().failure
                == RetailCenterVisibilityFailure::InvalidInput,
        "the conservative collection diagnostic did not classify invalid input");
    require(
        binding.cullingProcess()->shaderAccumulator == 0u,
        "the unused private culler must not retain a collection accumulator");
    require(
        left.referenceCount == 1u,
        "the scoped culler binding must preserve the owner's accumulator reference");
    require(
        std::memcmp(
            &stockCuller,
            &stockCullerSnapshot,
            sizeof(stockCuller)) == 0
            && state.stockAccumulator.referenceCount == 5u,
        "the scoped collection pass did not restore the live stock culler exactly");
    require(state.renderer.accumulator == stockAccumulatorAddress
            && state.accumulatingAccumulator == stockAccumulatorAddress
            && state.renderingAccumulator == stockAccumulatorAddress,
        "the private eye transaction did not restore every stock accumulator owner");

    abi::RetailBSShaderAccumulatorLayout stockRenderingLane {};
    stockRenderingLane.referenceCount = 1u;
    const abi::RetailPointer32 stockRenderingLaneAddress =
        static_cast<abi::RetailPointer32>(
            reinterpret_cast<std::uintptr_t>(&stockRenderingLane));
    state.renderingAccumulator = stockRenderingLaneAddress;
    require(
        operations.snapshotAuthoritativeState(operations.context),
        "a distinct non-null stock rendering lane was rejected at the audited call boundary");
    require(
        context.accumulatorSnapshotDiagnostics().captured
            && context.accumulatorSnapshotDiagnostics().failure
                == RetailCenterAccumulatorSnapshotFailure::None
            && context.accumulatorSnapshotDiagnostics().rendererAccumulator
                == stockAccumulatorAddress
            && context.accumulatorSnapshotDiagnostics().accumulatingAccumulator
                == stockAccumulatorAddress
            && context.accumulatorSnapshotDiagnostics().renderingAccumulator
                == stockRenderingLaneAddress,
        "the accumulator snapshot did not retain the distinct stock rendering lane");
    require(
        operations.restoreAuthoritativeState(operations.context)
            && state.renderer.accumulator == stockAccumulatorAddress
            && state.accumulatingAccumulator == stockAccumulatorAddress
            && state.renderingAccumulator == stockRenderingLaneAddress,
        "the distinct stock rendering lane was not restored exactly");
    state.renderingAccumulator = stockAccumulatorAddress;
    require(state.visiblePointers[0] != 0u
            && state.visiblePointers[0] == state.visiblePointers[1]
            && state.visibleCounts[0] == 3u
            && state.visibleCounts[1] == 3u,
        "both accumulators must consume the identical sealed visible array");
    require(binding.phase() == geometry::GeometryCollectorPhase::Inactive
            && binding.ownedVtableCloneInstalled()
            && binding.ownedVtableIntegrityValid(),
        "frame discard must clear geometry while preserving the private dispatch table");

    std::cout << "retail center renderer operations passed\n";
    return EXIT_SUCCESS;
#endif
}
