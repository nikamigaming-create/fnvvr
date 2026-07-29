#include "fnvxr_retail_engine_resource_adapter.h"

#include <array>
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
}

namespace
{
using namespace fnvxr::engine;
constexpr std::size_t CollectorCapacity = 64u;
using Binding = geometry::PrivateGeometryCollectorBinding<CollectorCapacity>;

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

enum Call : int
{
    CameraCreate = 1,
    BindingAllocate,
    CullerConstruct,
    LeftAllocate,
    LeftConstruct,
    RightAllocate,
    RightConstruct,
    RightDestroy,
    LeftDestroy,
    CullerDestroy,
    BindingFree,
    CameraFree,
};

struct FakeEngine
{
    std::vector<int> calls;
    std::size_t bindingBytes = 0u;
    std::uint32_t cullerArgument = 0xFFFFFFFFu;
    std::uint32_t accumulatorMode[2] {};
    std::uint32_t accumulatorBatches[2] {};
    std::uint32_t accumulatorPasses[2] {};
    std::uint32_t scalarDeletingFlags[2] {};
    int accumulatorConstructionCount = 0;
    int accumulatorDestructionCount = 0;
    bool cullerVtableRestoredBeforeDestroy = false;
    bool failAllocation = false;
    bool misalignBindingAllocation = false;
};

FakeEngine* gFake = nullptr;
abi::RetailPointer32 gVerifiedCullerVtable = static_cast<abi::RetailPointer32>(
    abi::BSCullingProcessVtableAddress);

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

void* FNVXR_TEST_CDECL fakeAllocate(std::uint32_t byteCount)
{
    if (gFake->failAllocation)
        return nullptr;
    constexpr std::size_t BindingAllocationBytes =
        sizeof(Binding) + alignof(Binding) - 1u;
    void* result = nullptr;
    if (byteCount == BindingAllocationBytes
        && gFake->misalignBindingAllocation)
    {
        auto* base = static_cast<std::byte*>(
            ::operator new(byteCount + alignof(Binding), std::nothrow));
        result = base ? base + 4u : nullptr;
    }
    else
    {
        result = ::operator new(byteCount, std::nothrow);
    }
    if (byteCount == BindingAllocationBytes)
    {
        gFake->calls.push_back(BindingAllocate);
        gFake->bindingBytes = byteCount;
    }
    else
    {
        gFake->calls.push_back(
            gFake->accumulatorConstructionCount == 0
                ? LeftAllocate
                : RightAllocate);
    }
    return result;
}

void FNVXR_TEST_CDECL fakeFree(void* allocation, std::uint32_t byteCount)
{
    constexpr std::size_t BindingAllocationBytes =
        sizeof(Binding) + alignof(Binding) - 1u;
    if (byteCount == BindingAllocationBytes)
    {
        gFake->calls.push_back(BindingFree);
        if (gFake->misalignBindingAllocation)
        {
            ::operator delete(
                static_cast<std::byte*>(allocation) - 4u);
            return;
        }
    }
    ::operator delete(allocation);
}

abi::RetailNiCameraLayout* FNVXR_TEST_CDECL fakeCameraCreate()
{
    gFake->calls.push_back(CameraCreate);
    return new (std::nothrow) abi::RetailNiCameraLayout {};
}

abi::RetailBSCullingProcessLayout* FNVXR_TEST_THISCALL_IMPL fakeCullerConstruct(
    abi::RetailBSCullingProcessLayout* storage,
    FNVXR_TEST_EDX_ARGUMENT
    std::uint32_t argument)
{
    gFake->calls.push_back(CullerConstruct);
    gFake->cullerArgument = argument;
    storage->base.vtable = gVerifiedCullerVtable;
    return storage;
}

void FNVXR_TEST_THISCALL_IMPL fakeCullerDestroy(
    abi::RetailBSCullingProcessLayout* culler
    FNVXR_TEST_TRAILING_EDX)
{
    gFake->calls.push_back(CullerDestroy);
    gFake->cullerVtableRestoredBeforeDestroy = culler
        && culler->base.vtable == gVerifiedCullerVtable;
    require(
        !culler || culler->shaderAccumulator == 0u,
        "the adapter must release a stale culler accumulator reference before culler destruction");
}

abi::RetailBSShaderAccumulatorLayout* FNVXR_TEST_THISCALL_IMPL
fakeAccumulatorConstruct(
    abi::RetailBSShaderAccumulatorLayout* storage,
    FNVXR_TEST_EDX_ARGUMENT
    std::uint32_t mode,
    std::uint32_t batches,
    std::uint32_t passes)
{
    const int index = gFake->accumulatorConstructionCount++;
    gFake->calls.push_back(index == 0 ? LeftConstruct : RightConstruct);
    gFake->accumulatorMode[index] = mode;
    gFake->accumulatorBatches[index] = batches;
    gFake->accumulatorPasses[index] = passes;
    storage->vtable = static_cast<abi::RetailPointer32>(
        abi::BSShaderAccumulatorVtableAddress);
    storage->referenceCount = 0u;
    return storage;
}

abi::RetailBSShaderAccumulatorLayout* FNVXR_TEST_THISCALL_IMPL
fakeAccumulatorDestroy(
    abi::RetailBSShaderAccumulatorLayout* accumulator,
    FNVXR_TEST_EDX_ARGUMENT
    std::uint32_t flags)
{
    const int index = gFake->accumulatorDestructionCount++;
    require(
        accumulator->referenceCount == 0u,
        "the adapter did not consume its accumulator ownership reference before destruction");
    gFake->calls.push_back(index == 0 ? RightDestroy : LeftDestroy);
    gFake->scalarDeletingFlags[index] = flags;
    ::operator delete(accumulator);
    return accumulator;
}

void FNVXR_TEST_THISCALL_IMPL fakeRefObjectFree(
    abi::RetailNiAccumulatorLayout* instance
    FNVXR_TEST_TRAILING_EDX)
{
    gFake->calls.push_back(CameraFree);
    delete reinterpret_cast<abi::RetailNiCameraLayout*>(instance);
}

void FNVXR_TEST_THISCALL_IMPL fakeSetCamera(
    abi::RetailNiAccumulatorLayout*,
    FNVXR_TEST_EDX_ARGUMENT
    abi::RetailNiCameraLayout*)
{
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
            "the fake culler released an unowned accumulator");
        --previous->referenceCount;
    }
    culler->shaderAccumulator = static_cast<abi::RetailPointer32>(
        reinterpret_cast<std::uintptr_t>(accumulator));
    if (accumulator)
        ++accumulator->referenceCount;
}

void FNVXR_TEST_THISCALL_IMPL fakeProcessAlt(
    abi::RetailBSCullingProcessLayout*,
    FNVXR_TEST_EDX_ARGUMENT
    abi::RetailNiCameraLayout*,
    void*,
    abi::RetailNiVisibleArrayLayout*)
{
}

void FNVXR_TEST_CDECL fakeAccumulateScene(
    abi::RetailNiCameraLayout*,
    void*,
    abi::RetailBSCullingProcessLayout*)
{
}

void FNVXR_TEST_THISCALL_IMPL fakeAddVisible(
    abi::RetailNiAccumulatorLayout*,
    FNVXR_TEST_EDX_ARGUMENT
    abi::RetailNiVisibleArrayLayout*)
{
}

void FNVXR_TEST_CDECL fakeRender(
    abi::RetailNiCameraLayout*,
    abi::RetailBSShaderAccumulatorLayout*,
    std::uint32_t)
{
}

void FNVXR_TEST_THISCALL_IMPL fakeFinishAccumulating(
    abi::RetailNiAccumulatorLayout*
    FNVXR_TEST_TRAILING_EDX)
{
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
    calls.renderAccumulatorWithoutFinalize = &fakeRender;
    calls.finalizeAccumulator = &fakeRender;
    return { calls, RetailEngineCallResolutionFailure::None };
}

std::array<abi::RetailPointer32, Binding::OwnedVtableEntryCount>
stockCullerVtable()
{
    std::array<abi::RetailPointer32, Binding::OwnedVtableEntryCount> result {};
    for (std::size_t index = 0u; index < result.size(); ++index)
        result[index] = static_cast<abi::RetailPointer32>(0x00500000u + index * 0x100u);
    result[Binding::AppendVtableEntryIndex] =
        static_cast<abi::RetailPointer32>(
            geometry::PrivateGeometryCollectorX86Abi::preferredTargetAddress);
    return result;
}

void requireExactLifecycle(const FakeEngine& fake)
{
    const std::vector<int> expected {
        CameraCreate,
        BindingAllocate,
        CullerConstruct,
        LeftAllocate,
        LeftConstruct,
        RightAllocate,
        RightConstruct,
        RightDestroy,
        LeftDestroy,
        CullerDestroy,
        BindingFree,
        CameraFree,
    };
    require(fake.calls == expected, "retail resources must use exact construction and reverse teardown order");
}
}

int main()
{
    using namespace fnvxr::engine;

    const RetailEngineCallResolution resolution = fakeResolution();
    require(resolution.complete(), "the fake call surface must cover all 15 retail calls");

    auto vtable = stockCullerVtable();
#if defined(_MSC_VER) && defined(_M_IX86)
    gVerifiedCullerVtable = static_cast<abi::RetailPointer32>(
        reinterpret_cast<std::uintptr_t>(vtable.data()));
#endif
    RetailEngineResourceContext<CollectorCapacity> context;
    require(
        context.initialize(
            resolution,
            SupportedImageBase,
            vtable.data(),
            41u),
        "a complete fixed-base call table and verified vtable must initialize");
    const auto operations = makeRetailEngineStereoResourceOperations(context);
    require(stereoResourceOperationsComplete(operations), "the concrete resource operation table must be complete");

    FakeEngine fake;
    gFake = &fake;
    const bool admitted = true;
    {
        auto result = acquireCenterStereoResources<CollectorCapacity>(
            operations,
            StereoResourceLifecycleTestAuthority::issue(admitted),
            RetailWorldStereoResourceConstructionParameters);
        require(result.succeeded(), "exact retail construction must acquire every private resource");
        require(result.resources.viewPlan().left == StereoResourceView::Center
                && result.resources.viewPlan().right == StereoResourceView::Center,
            "the first executable renderer milestone must remain center/center");
        require(!result.resources.collectorBinding()->ownedVtableCloneInstalled(),
            "direct visible-list collection must leave the retail culler vtable unmodified");
        require(result.resources.collectorBinding()->ownedVtableIntegrityValid(),
            "an unmodified private culler binding must pass its integrity check");
        const auto& clone = result.resources.collectorBinding()->ownedVtableCloneForAudit();
        for (std::size_t index = 0u; index < clone.size(); ++index)
            require(clone[index] == 0u,
                "direct visible-list collection must not retain a private culler vtable clone");
#if defined(_MSC_VER) && defined(_M_IX86)
        fakeSetCullerAccumulator(
            result.resources.cullingProcess(),
            nullptr,
            result.resources.leftAccumulator());
        require(
            result.resources.leftAccumulator()->referenceCount == 2u,
            "the teardown test must begin with a live culler NiPointer reference");
#endif
    }

    requireExactLifecycle(fake);
    require(
        fake.bindingBytes == sizeof(Binding) + alignof(Binding) - 1u,
        "Ni_Alloc must include enough padding to align the private binding");
    require(fake.cullerArgument == RetailWorldCullerConstructorArgument,
        "the culler constructor must receive the captured zero argument");
    for (int index = 0; index < 2; ++index)
    {
        require(fake.accumulatorMode[index] == RetailWorldAccumulatorConstructorMode
                && fake.accumulatorBatches[index] == RetailWorldAccumulatorBatchRendererCount
                && fake.accumulatorPasses[index] == RetailWorldAccumulatorMaximumPassCount,
            "both accumulators must receive the exact captured stock constructor triple");
        require(fake.scalarDeletingFlags[index] == 1u,
            "constructed accumulators must use the retail scalar destructor's deleting path");
    }
    require(fake.cullerVtableRestoredBeforeDestroy,
        "the stock culler vtable must remain intact before the engine destructor");
    require(context.failure() == RetailEngineResourceAdapterFailure::None,
        "a complete lifecycle must leave the concrete adapter healthy");

    RetailEngineResourceContext<CollectorCapacity> misalignedContext;
    require(
        misalignedContext.initialize(
            resolution,
            SupportedImageBase,
            vtable.data(),
            42u),
        "the misaligned-allocation adapter context must initialize");
    FakeEngine misalignedFake;
    misalignedFake.misalignBindingAllocation = true;
    gFake = &misalignedFake;
    {
        auto result = acquireCenterStereoResources<CollectorCapacity>(
            makeRetailEngineStereoResourceOperations(misalignedContext),
            StereoResourceLifecycleTestAuthority::issue(admitted),
            RetailWorldStereoResourceConstructionParameters);
        require(
            result.succeeded(),
            "a four-byte-aligned retail heap result must be promoted to the binding's eight-byte alignment");
        require(
            reinterpret_cast<std::uintptr_t>(
                result.resources.collectorBinding())
                    % alignof(Binding) == 0u,
            "the exposed private binding address must satisfy its exact alignment");
    }
    requireExactLifecycle(misalignedFake);
    require(
        misalignedContext.failure()
            == RetailEngineResourceAdapterFailure::None,
        "aligned binding teardown must release the original retail allocation base");

    RetailEngineResourceContext<CollectorCapacity> incompleteContext;
    RetailEngineCallResolution incomplete = resolution;
    incomplete.calls.niRefObjectFree = nullptr;
    require(!incompleteContext.initialize(
                incomplete,
                SupportedImageBase,
                vtable.data(),
                1u)
            && incompleteContext.failure()
                == RetailEngineResourceAdapterFailure::InvalidInitialization,
        "a missing camera release thunk must reject initialization");

    RetailEngineResourceContext<CollectorCapacity> relocatedContext;
    require(!relocatedContext.initialize(
                resolution,
                SupportedImageBase + RetailPeAllocationAlignment,
                vtable.data(),
                2u),
        "the culler path must reject an unproven relocated retail image");

    RetailEngineResourceContext<CollectorCapacity> unauthorizedContext;
    require(unauthorizedContext.initialize(
            resolution,
            SupportedImageBase,
            vtable.data(),
            3u),
        "a second clean adapter context must initialize");
    FakeEngine unauthorizedFake;
    gFake = &unauthorizedFake;
    const StereoResourceAuthorization noAuthorization {};
    const auto unauthorized = acquireCenterStereoResources<CollectorCapacity>(
        makeRetailEngineStereoResourceOperations(unauthorizedContext),
        noAuthorization,
        RetailWorldStereoResourceConstructionParameters);
    require(!unauthorized.succeeded()
            && unauthorized.failure == StereoResourceFailure::Unauthorized
            && unauthorizedFake.calls.empty(),
        "an operations table alone must never authorize a retail engine call");

    std::cout << "retail engine resource adapter passed\n";
    return EXIT_SUCCESS;
}

#undef FNVXR_TEST_CDECL
#undef FNVXR_TEST_THISCALL_IMPL
#undef FNVXR_TEST_EDX_ARGUMENT
#undef FNVXR_TEST_TRAILING_EDX
