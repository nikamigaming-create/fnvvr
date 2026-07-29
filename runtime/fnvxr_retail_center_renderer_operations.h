#pragma once

#include "fnvxr_center_renderer_backend.h"
#include "fnvxr_private_geometry_collector.h"
#include "fnvxr_retail_engine_calls.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace fnvxr::engine
{
struct RetailEyeTargetOperations
{
    void* context = nullptr;
    bool (*snapshot)(void*) noexcept = nullptr;
    bool (*bind)(
        void*,
        CenterRendererEye,
        CenterRendererEyeIsolation&) noexcept = nullptr;
    bool (*end)(
        void*,
        CenterRendererEye,
        CenterRendererEyeIsolation&) noexcept = nullptr;
    void (*rollback)(
        void*,
        CenterRendererEye,
        CenterRendererEyeIsolation&) noexcept = nullptr;
    bool (*restore)(void*) noexcept = nullptr;
};

constexpr bool retailEyeTargetOperationsComplete(
    const RetailEyeTargetOperations& operations) noexcept
{
    return operations.snapshot
        && operations.bind
        && operations.end
        && operations.rollback
        && operations.restore;
}

// This is intentionally a diagnostic-only classification for the one
// conservative center cull. It does not authorize a fallback or alter either
// eye transaction; it lets the in-process bridge distinguish an empty engine
// traversal from an explicit-visible-list/culling failure.
enum class RetailCenterVisibilityFailure : std::uint8_t
{
    None = 0u,
    ContextNotReady,
    InvalidInput,
    CollectionAccumulatorAddressOutOfRange,
    CullerAlreadyHasAccumulator,
    BeginCollectionRejected,
    CullerAccumulatorInstallRejected,
    CullerAccumulatorReleaseRejected,
    SealCollectionRejected,
    SealedViewRejected,
    NoVisibleGeometry,
    GeometryAddressOutOfRange,
    UnsupportedArchitecture,
    CullerVtableRejected,
    PrivateCullerRejected,
    CullerListModeRejected,
    CullerAccumulatorDetachRejected,
    CullerStateRestoreRejected,
    VisibleArrayStorageAddressOutOfRange,
    VisibleArrayRejected,
    CullerAccumulatorUnavailable,
};

struct RetailCenterVisibilityDiagnostics
{
    RetailCenterVisibilityFailure failure =
        RetailCenterVisibilityFailure::ContextNotReady;
    std::uintptr_t cameraAddress = 0u;
    std::uintptr_t sceneObjectAddress = 0u;
    std::uintptr_t cullerAddress = 0u;
    std::uint64_t generation = 0u;
    std::uint64_t sealedGeneration = 0u;
    std::uint32_t collectionAccumulatorReferenceCount = 0u;
    abi::RetailPointer32 cullerVisibleArray = 0u;
    abi::RetailPointer32 cullerCamera = 0u;
    abi::RetailPointer32 cullerShaderAccumulator = 0u;
    std::uint32_t cullerTopCullMode = 0u;
    std::uint32_t cullerCullModeStackSize = 0u;
    std::uint32_t sealedItemCount = 0u;
    std::uint8_t collectorPhase = 0xFFu;
    std::uint8_t collectorFailure = 0xFFu;
    std::uint8_t sealResult = 0xFFu;
    bool captured = false;
};

enum class RetailCenterAccumulatorSnapshotFailure : std::uint8_t
{
    NotAttempted = 0u,
    None,
    AlreadyActive,
    RegistrationIncomplete,
    RendererUnavailable,
    OwnerStateRejected,
    EyeTargetsRejected,
};

// Read-only evidence from the exact accumulator-owner snapshot that guards
// the private-eye transaction. This separates a changed retail owner
// relationship from a D3D eye-target snapshot rejection without weakening
// either condition.
struct RetailCenterAccumulatorSnapshotDiagnostics
{
    RetailCenterAccumulatorSnapshotFailure failure =
        RetailCenterAccumulatorSnapshotFailure::NotAttempted;
    abi::RetailPointer32 rendererAddress = 0u;
    abi::RetailPointer32 rendererAccumulator = 0u;
    abi::RetailPointer32 accumulatingAccumulator = 0u;
    abi::RetailPointer32 renderingAccumulator = 0u;
    std::uint32_t rendererAccumulatorReferenceCount = 0u;
    bool captured = false;
};

template <std::size_t CollectorCapacity>
class RetailCenterRendererOperationsContext;

namespace detail
{
template <std::size_t CollectorCapacity>
struct RetailCenterRendererOperationsAdapter;
}

template <std::size_t CollectorCapacity>
class RetailCenterRendererOperationsContext final
{
public:
    static_assert(
        CollectorCapacity > 0u
            && CollectorCapacity
                <= (std::numeric_limits<std::uint32_t>::max)(),
        "the retail collector must have a representable capacity");

    using Binding =
        geometry::PrivateGeometryCollectorBinding<CollectorCapacity>;

    RetailCenterRendererOperationsContext() noexcept = default;
    RetailCenterRendererOperationsContext(
        const RetailCenterRendererOperationsContext&) = delete;
    RetailCenterRendererOperationsContext& operator=(
        const RetailCenterRendererOperationsContext&) = delete;

    ~RetailCenterRendererOperationsContext() noexcept
    {
#if defined(_MSC_VER) && defined(_M_IX86)
        discardStockCullerCapture();
        if (mBinding && mBinding->ownedVtableCloneInstalled())
            static_cast<void>(mBinding->reset());
#endif
    }

    bool initialize(
        const RetailEngineCalls& calls,
        Binding& binding,
        abi::RetailBSShaderAccumulatorLayout& collectionAccumulator,
        const abi::RetailPointer32* verifiedCullerVtable,
        const RetailEyeTargetOperations& targets) noexcept
    {
        const abi::RetailPointer32 stockCullerVtable =
            binding.cullingProcess()->base.vtable;
        if (!calls.privateStereoComplete()
            || binding.ownedVtableCloneInstalled()
            || !binding.ownedVtableIntegrityValid()
            || !retailEyeTargetOperationsComplete(targets))
        {
            return false;
        }
#if defined(_MSC_VER) && defined(_M_IX86)
        const std::uintptr_t sourceVtableAddress =
            reinterpret_cast<std::uintptr_t>(verifiedCullerVtable);
        const std::uintptr_t bindingAddress =
            reinterpret_cast<std::uintptr_t>(&binding);
        const std::uintptr_t callbackAddress =
            reinterpret_cast<std::uintptr_t>(
                &geometry::privateGeometryCollectorVslotCallback<
                    CollectorCapacity>);
        if (!verifiedCullerVtable
            || sourceVtableAddress
                > (std::numeric_limits<abi::RetailPointer32>::max)()
            || bindingAddress
                > (std::numeric_limits<abi::RetailPointer32>::max)()
            || callbackAddress
                > (std::numeric_limits<abi::RetailPointer32>::max)()
            || stockCullerVtable
                != static_cast<abi::RetailPointer32>(sourceVtableAddress)
            || binding.installOwnedVtableClone(
                    verifiedCullerVtable,
                    Binding::OwnedVtableEntryCount,
                    static_cast<abi::RetailPointer32>(sourceVtableAddress),
                    static_cast<abi::RetailPointer32>(bindingAddress),
                    static_cast<abi::RetailPointer32>(callbackAddress))
                != geometry::GeometryVtableInstallResult::Installed)
        {
            return false;
        }
#else
        static_cast<void>(verifiedCullerVtable);
#endif
        mCalls = calls;
        mBinding = &binding;
        mCollectionAccumulator = &collectionAccumulator;
        mStockCullerVtable = stockCullerVtable;
        mTargets = targets;
        mInitialized = true;
        return true;
    }

    bool rebind(
        const RetailEngineCalls& calls,
        const RetailEyeTargetOperations& targets) noexcept
    {
        if (!mInitialized || !calls.privateStereoComplete()
            || !mBinding || !mCollectionAccumulator
            || !mBinding->ownedVtableIntegrityValid()
#if defined(_MSC_VER) && defined(_M_IX86)
            || !mBinding->ownedVtableCloneInstalled()
#else
            || mBinding->ownedVtableCloneInstalled()
#endif
            || !retailEyeTargetOperationsComplete(targets))
        {
            return false;
        }
        mCalls = calls;
        mTargets = targets;
        mEngineSnapshot = {};
        return true;
    }

    // Record the exact stock culler that is about to traverse the scene.  For
    // this one synchronous traversal, replace only its verified Append slot
    // with the binding-owned forwarding shim.  The shim records every visible
    // geometry then tail-enters the exact stock routine with its original x86
    // call frame intact; finishStockCullerCapture restores the stock vtable
    // before any private-eye work begins.
    bool beginStockCullerCapture(
        abi::RetailBSCullingProcessLayout* culler) noexcept
    {
#if !defined(_MSC_VER) || !defined(_M_IX86)
        static_cast<void>(culler);
        return false;
#else
        if (!ready() || !culler || mStockCapture.active)
            return false;
        if (culler == mBinding->cullingProcess()
            || culler->base.vtable != mStockCullerVtable)
        {
            return false;
        }
        if (!mBinding->resetCollectedGeometryPreservingOwnedVtable())
            return false;
        const std::uintptr_t cullerAddress =
            reinterpret_cast<std::uintptr_t>(culler);
        const abi::RetailPointer32 cloneVtable =
            mBinding->ownedVtableAddressForDispatch();
        if (cullerAddress
                > (std::numeric_limits<abi::RetailPointer32>::max)()
            || cloneVtable == 0u)
        {
            return false;
        }
        ++mNextStockCaptureGeneration;
        if (mNextStockCaptureGeneration == 0u)
            ++mNextStockCaptureGeneration;
        if (!mBinding->beginCollectionFor(
                culler,
                mNextStockCaptureGeneration,
                true))
        {
            return false;
        }
        const abi::RetailPointer32 sourceVtable = culler->base.vtable;
        culler->base.vtable = cloneVtable;
        if (culler->base.vtable != cloneVtable)
        {
            static_cast<void>(
                mBinding->resetCollectedGeometryPreservingOwnedVtable());
            return false;
        }
        mStockCapture = {
            culler,
            sourceVtable,
            mNextStockCaptureGeneration,
            true,
            false,
        };
        return true;
#endif
    }

    bool finishStockCullerCapture(
        abi::RetailBSCullingProcessLayout* culler) noexcept
    {
#if !defined(_MSC_VER) || !defined(_M_IX86)
        static_cast<void>(culler);
        return false;
#else
        if (!mBinding || !mStockCapture.active
            || !culler || culler != mStockCapture.culler)
        {
            return false;
        }
        const abi::RetailPointer32 cloneVtable =
            mBinding->ownedVtableAddressForDispatch();
        const bool cloneStillInstalled = cloneVtable != 0u
            && culler->base.vtable == cloneVtable;
        if (cloneStillInstalled)
            culler->base.vtable = mStockCapture.sourceVtable;
        const bool stockVtableRestored = cloneStillInstalled
            && culler->base.vtable == mStockCapture.sourceVtable;
        const geometry::GeometrySealResult sealResult = stockVtableRestored
            ? mBinding->sealCollectionFor(culler)
            : geometry::GeometrySealResult::OwnerMismatchInvalidated;
        mStockCapture.active = false;
        mStockCapture.sealed = stockVtableRestored
            && sealResult == geometry::GeometrySealResult::Sealed;
        if (!mStockCapture.sealed)
        {
            static_cast<void>(
                mBinding->resetCollectedGeometryPreservingOwnedVtable());
            mStockCapture = {};
            return false;
        }
        return true;
#endif
    }

    bool ready() const noexcept
    {
        return mInitialized
            && mCalls.privateStereoComplete()
            && mBinding
            && mCollectionAccumulator
            && mStockCullerVtable != 0u
            && mBinding->ownedVtableIntegrityValid()
#if defined(_MSC_VER) && defined(_M_IX86)
            && mBinding->ownedVtableCloneInstalled()
#else
            && !mBinding->ownedVtableCloneInstalled()
#endif
            && retailEyeTargetOperationsComplete(mTargets);
    }

    const RetailCenterVisibilityDiagnostics& visibilityDiagnostics() const
        noexcept
    {
        return mLastVisibility;
    }

    const RetailCenterAccumulatorSnapshotDiagnostics&
    accumulatorSnapshotDiagnostics() const noexcept
    {
        return mLastAccumulatorSnapshot;
    }

private:
    struct EngineAccumulatorSnapshot
    {
        abi::RetailRendererAccumulatorOwnerLayout* renderer = nullptr;
        abi::RetailPointer32 rendererAccumulator = 0u;
        abi::RetailPointer32 accumulatingAccumulator = 0u;
        abi::RetailPointer32 renderingAccumulator = 0u;
        bool active = false;
    };

    struct StockCullerCapture
    {
        abi::RetailBSCullingProcessLayout* culler = nullptr;
        abi::RetailPointer32 sourceVtable = 0u;
        std::uint64_t generation = 0u;
        bool active = false;
        bool sealed = false;
    };

    void discardStockCullerCapture() noexcept
    {
#if defined(_MSC_VER) && defined(_M_IX86)
        if (mBinding && mStockCapture.active && mStockCapture.culler)
        {
            const abi::RetailPointer32 cloneVtable =
                mBinding->ownedVtableAddressForDispatch();
            if (cloneVtable != 0u
                && mStockCapture.culler->base.vtable == cloneVtable)
            {
                mStockCapture.culler->base.vtable =
                    mStockCapture.sourceVtable;
            }
        }
        if (mBinding && mBinding->ownedVtableCloneInstalled())
        {
            static_cast<void>(
                mBinding->resetCollectedGeometryPreservingOwnedVtable());
        }
#endif
        mStockCapture = {};
    }

    bool mInitialized = false;
    RetailEngineCalls mCalls {};
    Binding* mBinding = nullptr;
    abi::RetailBSShaderAccumulatorLayout* mCollectionAccumulator = nullptr;
    abi::RetailPointer32 mStockCullerVtable = 0u;
    RetailEyeTargetOperations mTargets {};
    EngineAccumulatorSnapshot mEngineSnapshot {};
    StockCullerCapture mStockCapture {};
    std::uint64_t mNextStockCaptureGeneration = 0u;
    RetailCenterVisibilityDiagnostics mLastVisibility {};
    RetailCenterAccumulatorSnapshotDiagnostics mLastAccumulatorSnapshot {};
    // Never replay ProcessAlt on a copied culler. The stock culler is captured
    // only during its own AccumulateScene call and restored before eye work.

    friend struct detail::RetailCenterRendererOperationsAdapter<
        CollectorCapacity>;
};

namespace detail
{
template <std::size_t CollectorCapacity>
struct RetailCenterRendererOperationsAdapter
{
    using Context = RetailCenterRendererOperationsContext<CollectorCapacity>;

    static Context* checked(void* opaque) noexcept
    {
        auto* context = static_cast<Context*>(opaque);
        return context && context->ready() ? context : nullptr;
    }

    static abi::RetailBSShaderAccumulatorLayout* accumulatorFromAddress(
        abi::RetailPointer32 address) noexcept
    {
        return reinterpret_cast<abi::RetailBSShaderAccumulatorLayout*>(
            static_cast<std::uintptr_t>(address));
    }

    static void captureVisibilityDiagnostics(
        Context& context,
        abi::RetailNiCameraLayout* camera,
        void* sceneObject,
        abi::RetailBSCullingProcessLayout* culler,
        std::uint64_t generation) noexcept
    {
        RetailCenterVisibilityDiagnostics& diagnostics =
            context.mLastVisibility;
        diagnostics = {};
        diagnostics.failure = RetailCenterVisibilityFailure::None;
        diagnostics.cameraAddress = reinterpret_cast<std::uintptr_t>(camera);
        diagnostics.sceneObjectAddress =
            reinterpret_cast<std::uintptr_t>(sceneObject);
        diagnostics.cullerAddress = reinterpret_cast<std::uintptr_t>(culler);
        diagnostics.generation = generation;
        diagnostics.captured = true;
        if (context.mCollectionAccumulator)
        {
            diagnostics.collectionAccumulatorReferenceCount =
                context.mCollectionAccumulator->referenceCount;
        }
        if (context.mBinding)
        {
            diagnostics.collectorPhase = static_cast<std::uint8_t>(
                context.mBinding->phase());
            diagnostics.collectorFailure = static_cast<std::uint8_t>(
                context.mBinding->failure());
            if (culler)
            {
                diagnostics.cullerVisibleArray = culler->base.visibleArray;
                diagnostics.cullerCamera = culler->base.camera;
                diagnostics.cullerShaderAccumulator =
                    culler->shaderAccumulator;
                diagnostics.cullerTopCullMode = culler->topCullMode;
                diagnostics.cullerCullModeStackSize = culler->cullModeStackSize;
            }
        }
    }

    static bool rejectVisibility(
        Context& context,
        abi::RetailBSCullingProcessLayout* culler,
        RetailCenterVisibilityFailure failure) noexcept
    {
        RetailCenterVisibilityDiagnostics& diagnostics =
            context.mLastVisibility;
        if (context.mCollectionAccumulator)
        {
            diagnostics.collectionAccumulatorReferenceCount =
                context.mCollectionAccumulator->referenceCount;
        }
        if (context.mBinding)
        {
            diagnostics.collectorPhase = static_cast<std::uint8_t>(
                context.mBinding->phase());
            diagnostics.collectorFailure = static_cast<std::uint8_t>(
                context.mBinding->failure());
            if (culler)
            {
                diagnostics.cullerVisibleArray = culler->base.visibleArray;
                diagnostics.cullerCamera = culler->base.camera;
                diagnostics.cullerShaderAccumulator =
                    culler->shaderAccumulator;
                diagnostics.cullerTopCullMode = culler->topCullMode;
                diagnostics.cullerCullModeStackSize = culler->cullModeStackSize;
            }
        }
        diagnostics.failure = failure;
        diagnostics.captured = true;
        return false;
    }

    static bool restorePrivateCullerState(
        Context& context,
        const abi::RetailBSCullingProcessLayout& snapshot) noexcept
    {
        if (!context.mBinding)
            return false;
        auto* privateCuller = context.mBinding->cullingProcess();
        if (!privateCuller)
            return false;
        *privateCuller = snapshot;
        return std::memcmp(
                   privateCuller,
                   &snapshot,
                   sizeof(snapshot)) == 0
            && context.mBinding->ownedVtableIntegrityValid();
    }

    static bool snapshotEngineAccumulatorState(Context& context) noexcept
    {
        RetailCenterAccumulatorSnapshotDiagnostics& diagnostics =
            context.mLastAccumulatorSnapshot;
        diagnostics = {};
        diagnostics.captured = true;
        if (context.mEngineSnapshot.active)
        {
            diagnostics.failure =
                RetailCenterAccumulatorSnapshotFailure::AlreadyActive;
            return false;
        }
        if (!context.mCalls.privateStereoRegistrationComplete())
        {
            diagnostics.failure =
                RetailCenterAccumulatorSnapshotFailure::RegistrationIncomplete;
            return false;
        }
        const abi::RetailPointer32 rendererAddress =
            *context.mCalls.rendererSingleton;
        diagnostics.rendererAddress = rendererAddress;
        if (rendererAddress == 0u)
        {
            diagnostics.failure =
                RetailCenterAccumulatorSnapshotFailure::RendererUnavailable;
            return false;
        }
        auto* renderer =
            reinterpret_cast<abi::RetailRendererAccumulatorOwnerLayout*>(
                static_cast<std::uintptr_t>(rendererAddress));
        const abi::RetailPointer32 rendererAccumulator =
            renderer->accumulator;
        diagnostics.rendererAccumulator = rendererAccumulator;
        auto* ownedAccumulator = accumulatorFromAddress(rendererAccumulator);
        const abi::RetailPointer32 accumulatingAccumulator =
            *context.mCalls.accumulatingAccumulator;
        diagnostics.accumulatingAccumulator = accumulatingAccumulator;
        const abi::RetailPointer32 renderingAccumulator =
            *context.mCalls.renderingAccumulator;
        diagnostics.renderingAccumulator = renderingAccumulator;
        if (ownedAccumulator)
        {
            diagnostics.rendererAccumulatorReferenceCount =
                ownedAccumulator->referenceCount;
        }
        // The renderer and accumulating globals share the coordinator-owned
        // accumulator at this call boundary. Retail may keep a distinct,
        // live rendering lane after its AccumulateScene call; preserve that
        // non-null lane exactly and restore it after the private eye work.
        if (!ownedAccumulator
            || ownedAccumulator->referenceCount < 4u
            || accumulatingAccumulator != rendererAccumulator
            || renderingAccumulator == 0u)
        {
            diagnostics.failure =
                RetailCenterAccumulatorSnapshotFailure::OwnerStateRejected;
            return false;
        }

        context.mEngineSnapshot = {
            renderer,
            rendererAccumulator,
            accumulatingAccumulator,
            renderingAccumulator,
            true,
        };
        diagnostics.failure = RetailCenterAccumulatorSnapshotFailure::None;
        return true;
    }

    static bool restoreEngineAccumulatorState(Context& context) noexcept
    {
        if (!context.mEngineSnapshot.active)
            return true;
        const typename Context::EngineAccumulatorSnapshot snapshot =
            context.mEngineSnapshot;
        auto* rendererAccumulator = accumulatorFromAddress(
            snapshot.rendererAccumulator);
        auto* accumulatingAccumulator = accumulatorFromAddress(
            snapshot.accumulatingAccumulator);
        auto* renderingAccumulator = accumulatorFromAddress(
            snapshot.renderingAccumulator);
        context.mCalls.setRenderingAccumulator(renderingAccumulator);
        context.mCalls.setAccumulatingAccumulator(accumulatingAccumulator);
        context.mCalls.rendererSetAccumulator(
            snapshot.renderer,
            rendererAccumulator);
        const bool restored = snapshot.renderer->accumulator
                == snapshot.rendererAccumulator
            && *context.mCalls.accumulatingAccumulator
                == snapshot.accumulatingAccumulator
            && *context.mCalls.renderingAccumulator
                == snapshot.renderingAccumulator;
        if (restored)
            context.mEngineSnapshot = {};
        return restored;
    }

    static bool registerEyeAccumulator(
        Context& context,
        abi::RetailBSShaderAccumulatorLayout* accumulator) noexcept
    {
        if (!context.mEngineSnapshot.active || !accumulator)
            return false;
        const std::uintptr_t accumulatorAddress =
            reinterpret_cast<std::uintptr_t>(accumulator);
        if (accumulatorAddress
            > (std::numeric_limits<abi::RetailPointer32>::max)())
        {
            return false;
        }
        const abi::RetailPointer32 address =
            static_cast<abi::RetailPointer32>(accumulatorAddress);
        context.mCalls.rendererSetAccumulator(
            context.mEngineSnapshot.renderer,
            accumulator);
        context.mCalls.setAccumulatingAccumulator(accumulator);
        // AddVisibleArray is permitted to draw immediately, before the
        // explicit render call. Its rendering-global must therefore already
        // name this eye's private accumulator; leaving the stock accumulator
        // installed sends those draws through the wrong camera/state lane and
        // produced the all-black private-eye payload seen in the live runs.
        context.mCalls.setRenderingAccumulator(accumulator);
        return context.mEngineSnapshot.renderer->accumulator == address
            && *context.mCalls.accumulatingAccumulator == address
            && *context.mCalls.renderingAccumulator == address;
    }

    static bool snapshot(void* opaque) noexcept
    {
        Context* context = checked(opaque);
        if (!context || !snapshotEngineAccumulatorState(*context))
            return false;
        if (!context->mTargets.snapshot(context->mTargets.context))
        {
            context->mEngineSnapshot = {};
            context->mLastAccumulatorSnapshot.failure =
                RetailCenterAccumulatorSnapshotFailure::EyeTargetsRejected;
            return false;
        }
        return true;
    }

    static bool collect(
        void* opaque,
        abi::RetailNiCameraLayout* camera,
        void* sceneObject,
        abi::RetailBSCullingProcessLayout* culler,
        std::uint64_t generation,
        CenterRendererVisibleSet& visibleSet) noexcept
    {
        visibleSet = {};
        Context* context = static_cast<Context*>(opaque);
        if (!context)
            return false;
        captureVisibilityDiagnostics(
            *context,
            camera,
            sceneObject,
            culler,
            generation);
        if (!context->ready())
        {
            return rejectVisibility(
                *context,
                culler,
                RetailCenterVisibilityFailure::ContextNotReady);
        }
        if (!context
            || !camera
            || !sceneObject
            || !culler
            || !context->mCollectionAccumulator
            || generation == 0u)
        {
            return rejectVisibility(
                *context,
                culler,
                RetailCenterVisibilityFailure::InvalidInput);
        }

#if defined(_MSC_VER) && defined(_M_IX86)
        if (culler->base.vtable != context->mStockCullerVtable)
        {
            return rejectVisibility(
                *context,
                culler,
                RetailCenterVisibilityFailure::CullerVtableRejected);
        }
        if (culler == context->mBinding->cullingProcess())
        {
            return rejectVisibility(
                *context,
                culler,
                RetailCenterVisibilityFailure::PrivateCullerRejected);
        }
        if (!context->mBinding->ownedVtableCloneInstalled()
            || !context->mBinding->ownedVtableIntegrityValid())
        {
            return rejectVisibility(
                *context,
                culler,
                RetailCenterVisibilityFailure::PrivateCullerRejected);
        }
        if (context->mStockCapture.active
            || !context->mStockCapture.sealed
            || context->mStockCapture.culler != culler
            || context->mStockCapture.sourceVtable
                != context->mStockCullerVtable
            || context->mStockCapture.generation == 0u)
        {
            return rejectVisibility(
                *context,
                culler,
                RetailCenterVisibilityFailure::CullerStateRestoreRejected);
        }
        geometry::PrivateGeometrySealedView view {};
        if (!context->mBinding->tryGetSealedView(view))
        {
            return rejectVisibility(
                *context,
                culler,
                RetailCenterVisibilityFailure::SealedViewRejected);
        }
        const std::uintptr_t geometryAddress =
            reinterpret_cast<std::uintptr_t>(view.geometryPointers);
        if (geometryAddress
            > (std::numeric_limits<abi::RetailPointer32>::max)())
        {
            return rejectVisibility(
                *context,
                culler,
                RetailCenterVisibilityFailure::VisibleArrayStorageAddressOutOfRange);
        }
        context->mLastVisibility.sealedItemCount = view.itemCount;
        context->mLastVisibility.sealedGeneration = view.generation;
        if (!view.geometryPointers
            || view.generation != context->mStockCapture.generation
            || view.itemCount > static_cast<std::uint32_t>(CollectorCapacity))
        {
            return rejectVisibility(
                *context,
                culler,
                RetailCenterVisibilityFailure::SealedViewRejected);
        }
        if (view.itemCount == 0u)
        {
            return rejectVisibility(
                *context,
                culler,
                RetailCenterVisibilityFailure::NoVisibleGeometry);
        }
        for (std::uint32_t index = 0u; index < view.itemCount; ++index)
        {
            if (view.geometryPointers[index] == 0u)
            {
                return rejectVisibility(
                    *context,
                    culler,
                    RetailCenterVisibilityFailure::VisibleArrayRejected);
            }
        }

        visibleSet.array = {
            static_cast<abi::RetailPointer32>(geometryAddress),
            view.itemCount,
            static_cast<std::uint32_t>(CollectorCapacity),
            0u,
        };
        visibleSet.generation = generation;
        static_cast<void>(rejectVisibility(
            *context,
            culler,
            RetailCenterVisibilityFailure::None));
        return true;
#else
        return rejectVisibility(
            *context,
            culler,
            RetailCenterVisibilityFailure::UnsupportedArchitecture);
#endif
    }

    static bool bind(
        void* opaque,
        CenterRendererEye eye,
        CenterRendererEyeIsolation& isolation) noexcept
    {
        Context* context = checked(opaque);
        return context
            && context->mTargets.bind(
                context->mTargets.context,
                eye,
                isolation);
    }

    static bool setCamera(
        void* opaque,
        abi::RetailBSShaderAccumulatorLayout* accumulator,
        abi::RetailNiCameraLayout* camera) noexcept
    {
        Context* context = checked(opaque);
        if (!context || !accumulator || !camera)
            return false;
        if (!registerEyeAccumulator(*context, accumulator))
            return false;
        context->mCalls.accumulatorSetCamera(
            reinterpret_cast<abi::RetailNiAccumulatorLayout*>(accumulator),
            camera);
        return true;
    }

    static bool addVisible(
        void* opaque,
        abi::RetailBSShaderAccumulatorLayout* accumulator,
        const abi::RetailNiVisibleArrayLayout* visibleArray) noexcept
    {
        Context* context = checked(opaque);
        if (!context || !accumulator || !visibleArray)
            return false;
        context->mCalls.accumulatorAddVisibleArray(
            reinterpret_cast<abi::RetailNiAccumulatorLayout*>(accumulator),
            const_cast<abi::RetailNiVisibleArrayLayout*>(visibleArray));
        return true;
    }

    static bool render(
        void* opaque,
        abi::RetailNiCameraLayout* camera,
        abi::RetailBSShaderAccumulatorLayout* accumulator,
        std::uint32_t renderContext) noexcept
    {
        Context* context = checked(opaque);
        if (!context
            || !camera
            || !accumulator
            || renderContext != RetailWorldRenderContext)
        {
            return false;
        }
        context->mCalls.renderAccumulatorWithoutFinalize(
            camera,
            accumulator,
            renderContext);
        const std::uintptr_t accumulatorAddress =
            reinterpret_cast<std::uintptr_t>(accumulator);
        return accumulatorAddress
                <= (std::numeric_limits<abi::RetailPointer32>::max)()
            && *context->mCalls.renderingAccumulator
                == static_cast<abi::RetailPointer32>(accumulatorAddress);
    }

    static bool finalize(
        void* opaque,
        abi::RetailNiCameraLayout* camera,
        abi::RetailBSShaderAccumulatorLayout* accumulator,
        std::uint32_t renderContext) noexcept
    {
        Context* context = checked(opaque);
        if (!context
            || !camera
            || !accumulator
            || renderContext != RetailWorldRenderContext)
        {
            return false;
        }
        context->mCalls.finalizeAccumulator(
            camera,
            accumulator,
            renderContext);
        return true;
    }

    static bool end(
        void* opaque,
        CenterRendererEye eye,
        CenterRendererEyeIsolation& isolation) noexcept
    {
        Context* context = checked(opaque);
        return context
            && context->mTargets.end(
                context->mTargets.context,
                eye,
                isolation);
    }

    static void rollback(
        void* opaque,
        CenterRendererEye eye,
        CenterRendererEyeIsolation& isolation) noexcept
    {
        Context* context = checked(opaque);
        if (context)
        {
            context->mTargets.rollback(
                context->mTargets.context,
                eye,
                isolation);
            restoreEngineAccumulatorState(*context);
        }
    }

    static bool restore(void* opaque) noexcept
    {
        Context* context = checked(opaque);
        if (!context)
            return false;
        const bool targetsRestored =
            context->mTargets.restore(context->mTargets.context);
        const bool engineRestored = restoreEngineAccumulatorState(*context);
        return targetsRestored && engineRestored;
    }

    static void discard(
        void* opaque,
        CenterRendererVisibleSet& visibleSet) noexcept
    {
        Context* context = checked(opaque);
        if (context)
        {
            context->discardStockCullerCapture();
        }
        visibleSet = {};
    }

    static CenterRendererOperations operations(Context& context) noexcept
    {
        CenterRendererOperations result {};
        result.context = &context;
        result.snapshotAuthoritativeState = &snapshot;
        result.collectConservativeVisibleSet = &collect;
        result.bindEyeTargets = &bind;
        result.setAccumulatorCamera = &setCamera;
        result.addVisibleArray = &addVisible;
        result.renderAccumulatorWithoutFinalize = &render;
        result.finalizeAccumulator = &finalize;
        result.endEyeTargets = &end;
        result.rollbackEyeTargets = &rollback;
        result.restoreAuthoritativeState = &restore;
        result.discardVisibleSet = &discard;
        return result;
    }
};
}

template <std::size_t CollectorCapacity>
CenterRendererOperations makeRetailCenterRendererOperations(
    RetailCenterRendererOperationsContext<CollectorCapacity>& context) noexcept
{
    return detail::RetailCenterRendererOperationsAdapter<CollectorCapacity>
        ::operations(context);
}
}
