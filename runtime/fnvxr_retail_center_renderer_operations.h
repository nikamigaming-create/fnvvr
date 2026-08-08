#pragma once

#include "fnvxr_center_renderer_backend.h"
#include "fnvxr_private_geometry_collector.h"
#include "fnvxr_retail_engine_calls.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace fnvxr::engine
{
inline constexpr std::size_t RetailFirstPersonRootCapacity = 8u;

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
    // The world transaction clears a fresh target before each eye.  A later
    // first-person pass must bind that same target without erasing the world
    // pixels it is meant to augment.
    bool (*bindPreservingContents)(
        void*,
        CenterRendererEye,
        CenterRendererEyeIsolation&) noexcept = nullptr;
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
    DiagnosticStop,
    NoQueueSafeGeometry,
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
    std::uint32_t queueSafeItemCount = 0u;
    std::uint32_t immediateGeometryRejectedCount = 0u;
    std::uint32_t firstPersonTraversalVisits = 0u;
    std::uint32_t firstPersonQueueSafeItemCount = 0u;
    std::uint32_t firstPersonImmediateItemCount = 0u;
    abi::RetailPointer32 firstPersonRootNode = 0u;
    std::uint32_t firstPersonPopulateCalls = 0u;
    std::uint32_t firstPersonRightQueueReplayCount = 0u;
    std::uint32_t firstPersonRightImmediateCallbackCount = 0u;
    std::uint8_t privateAccumulatorMode = 0u;
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
    RenderingLaneOwnershipRejected,
};

// Bounded live diagnostics can stop after an exact renderer boundary.  Every
// stop is reported as failure so it exercises the normal rollback/restore
// path and can never be mistaken for a completed stereo frame.
enum class RetailCenterRendererDiagnosticStop : std::uint8_t
{
    None = 0u,
    AfterSnapshot,
    AfterVisibility,
    AfterLeftBind,
    AfterLeftCamera,
    AfterLeftPopulate,
    AfterLeftRender,
    AfterLeftFinalize,
    AfterLeftEye,
};

constexpr bool retailCenterRendererDiagnosticStopValid(
    RetailCenterRendererDiagnosticStop stop) noexcept
{
    return stop >= RetailCenterRendererDiagnosticStop::None
        && stop <= RetailCenterRendererDiagnosticStop::AfterLeftEye;
}

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
    std::uint32_t renderingAccumulatorReferenceCount = 0u;
    bool distinctRenderingAccumulatorRetained = false;
    bool captured = false;
};

struct RetailCenterRendererTimingDiagnostics
{
    double snapshotMilliseconds = 0.0;
    double collectMilliseconds = 0.0;
    double leftBindMilliseconds = 0.0;
    double leftCameraMilliseconds = 0.0;
    double leftPopulateMilliseconds = 0.0;
    double leftRenderMilliseconds = 0.0;
    double leftFinalizeMilliseconds = 0.0;
    double leftEndMilliseconds = 0.0;
    double rightBindMilliseconds = 0.0;
    double rightCameraMilliseconds = 0.0;
    double rightPopulateMilliseconds = 0.0;
    double rightRenderMilliseconds = 0.0;
    double rightFinalizeMilliseconds = 0.0;
    double rightEndMilliseconds = 0.0;
    double restoreMilliseconds = 0.0;
    double rollbackMilliseconds = 0.0;
    bool captured = false;

    double totalMilliseconds() const noexcept
    {
        return snapshotMilliseconds
            + collectMilliseconds
            + leftBindMilliseconds
            + leftCameraMilliseconds
            + leftPopulateMilliseconds
            + leftRenderMilliseconds
            + leftFinalizeMilliseconds
            + leftEndMilliseconds
            + rightBindMilliseconds
            + rightCameraMilliseconds
            + rightPopulateMilliseconds
            + rightRenderMilliseconds
            + rightFinalizeMilliseconds
            + rightEndMilliseconds
            + restoreMilliseconds
            + rollbackMilliseconds;
    }
};

// Read-only evidence that the exact per-eye accumulator and its private
// culler consumed the same camera pointer during the audited eye transaction.
// The stock culler remains the one conservative visibility pass; these fields
// describe the subsequent private-eye accumulations only.
struct RetailCenterEyeCameraDiagnostics
{
    abi::RetailPointer32 leftRendererCamera = 0u;
    abi::RetailPointer32 rightRendererCamera = 0u;
    abi::RetailPointer32 leftCullerCamera = 0u;
    abi::RetailPointer32 rightCullerCamera = 0u;
    bool leftRendererCameraMatches = false;
    bool rightRendererCameraMatches = false;
    bool leftCullerCameraMatches = false;
    bool rightCullerCameraMatches = false;
    bool captured = false;

    bool complete() const noexcept
    {
        return captured
            && leftRendererCameraMatches
            && rightRendererCameraMatches
            && leftCullerCameraMatches
            && rightCullerCameraMatches;
    }
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
        mDiagnosticStop = RetailCenterRendererDiagnosticStop::None;
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
        mDiagnosticStop = RetailCenterRendererDiagnosticStop::None;
        return true;
    }

    bool setDiagnosticStop(
        RetailCenterRendererDiagnosticStop stop) noexcept
    {
        if (!retailCenterRendererDiagnosticStopValid(stop)
            || mEngineSnapshot.active)
        {
            return false;
        }
        mDiagnosticStop = stop;
        return true;
    }

    bool setFirstPersonRootNodes(
        const std::array<abi::RetailPointer32,
            RetailFirstPersonRootCapacity>& roots,
        std::uint32_t count) noexcept
    {
        if (!ready() || mEngineSnapshot.active
            || count > RetailFirstPersonRootCapacity)
            return false;
        mFirstPersonRootNodes = roots;
        mFirstPersonRootNodeCount = count;
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

    const RetailCenterRendererTimingDiagnostics& timingDiagnostics() const
        noexcept
    {
        return mLastTiming;
    }

    const RetailCenterEyeCameraDiagnostics& eyeCameraDiagnostics() const
        noexcept
    {
        return mLastEyeCamera;
    }

private:
    struct FirstPersonGeometryMutation
    {
        abi::RetailPointer32 geometry = 0u;
        abi::RetailPointer32 property = 0u;
        std::uint32_t geometryFlags = 0u;
        std::uint16_t propertyFlags = 0u;
        bool applied = false;
    };

    struct EngineAccumulatorSnapshot
    {
        abi::RetailRendererAccumulatorOwnerLayout* renderer = nullptr;
        abi::RetailPointer32 rendererAccumulator = 0u;
        abi::RetailPointer32 accumulatingAccumulator = 0u;
        abi::RetailPointer32 renderingAccumulator = 0u;
        bool distinctRenderingAccumulatorRetained = false;
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
    RetailCenterRendererDiagnosticStop mDiagnosticStop =
        RetailCenterRendererDiagnosticStop::None;
    EngineAccumulatorSnapshot mEngineSnapshot {};
    StockCullerCapture mStockCapture {};
    std::array<abi::RetailPointer32, CollectorCapacity>
        mQueueSafeGeometryPointers {};
    std::array<abi::RetailPointer32, CollectorCapacity>
        mFirstPersonImmediateGeometryPointers {};
    std::array<abi::RetailPointer32, CollectorCapacity>
        mFirstPersonQueueSafeGeometryPointers {};
    std::array<FirstPersonGeometryMutation, CollectorCapacity>
        mFirstPersonGeometryMutations {};
    std::uint32_t mFirstPersonImmediateGeometryCount = 0u;
    std::uint32_t mFirstPersonQueueSafeGeometryCount = 0u;
    std::array<abi::RetailPointer32, RetailFirstPersonRootCapacity>
        mFirstPersonRootNodes {};
    std::uint32_t mFirstPersonRootNodeCount = 0u;
    void* mFrameSceneObject = nullptr;
    abi::RetailNiCameraLayout* mActiveEyeCamera = nullptr;
    std::uint64_t mNextStockCaptureGeneration = 0u;
    RetailCenterVisibilityDiagnostics mLastVisibility {};
    RetailCenterAccumulatorSnapshotDiagnostics mLastAccumulatorSnapshot {};
    RetailCenterRendererTimingDiagnostics mLastTiming {};
    RetailCenterEyeCameraDiagnostics mLastEyeCamera {};
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

    class ScopedStageTimer final
    {
    public:
        explicit ScopedStageTimer(double& destination) noexcept
            : mDestination(destination)
            , mStarted(std::chrono::steady_clock::now())
        {
        }

        ~ScopedStageTimer() noexcept
        {
            mDestination = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - mStarted).count();
        }

        ScopedStageTimer(const ScopedStageTimer&) = delete;
        ScopedStageTimer& operator=(const ScopedStageTimer&) = delete;

    private:
        double& mDestination;
        std::chrono::steady_clock::time_point mStarted;
    };

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

    static bool geometryQueuesWithoutImmediateDispatch(
        const abi::RetailBSShaderAccumulatorLayout* accumulator,
        abi::RetailPointer32 geometryAddress) noexcept
    {
        if (!accumulator || geometryAddress == 0u)
            return false;

        // Exact NiAccumulator::AddVisibleArray branch contract at 0x00A9B790.
        // Any geometry that fails this predicate is immediately dispatched
        // through vslot +0xDC. Replaying that class after stock AccumulateScene
        // invalidates per-object render-pass data used by a later retail frame,
        // so the private-eye transaction admits only the list-queue branch.
        constexpr std::size_t GeometryFlagsOffset = 0x30u;
        constexpr std::size_t GeometryPropertyOffset = 0x9Cu;
        constexpr std::size_t PropertyFlagsOffset = 0x18u;
        constexpr std::size_t AccumulatorModeOffset = 0x30u;
        constexpr std::uint16_t PropertyAccumulatorQueue = 0x0001u;
        constexpr std::uint16_t PropertyModeExcluded = 0x2000u;
        constexpr std::uint32_t GeometryImmediateDispatch = 0x00000040u;

        const auto* geometry = reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(geometryAddress));
        abi::RetailPointer32 propertyAddress = 0u;
        std::uint32_t geometryFlags = 0u;
        std::memcpy(
            &propertyAddress,
            geometry + GeometryPropertyOffset,
            sizeof(propertyAddress));
        std::memcpy(
            &geometryFlags,
            geometry + GeometryFlagsOffset,
            sizeof(geometryFlags));
        if (propertyAddress == 0u
            || (geometryFlags & GeometryImmediateDispatch) != 0u)
        {
            return false;
        }

        const auto* property = reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(propertyAddress));
        std::uint16_t propertyFlags = 0u;
        std::memcpy(
            &propertyFlags,
            property + PropertyFlagsOffset,
            sizeof(propertyFlags));
        const auto* accumulatorBytes =
            reinterpret_cast<const std::uint8_t*>(accumulator);
        const std::uint8_t accumulatorMode =
            accumulatorBytes[AccumulatorModeOffset];
        return (propertyFlags & PropertyAccumulatorQueue) != 0u
            && (accumulatorMode == 0u
                || (propertyFlags & PropertyModeExcluded) == 0u);
    }

    static bool geometryDescendsFrom(
        abi::RetailPointer32 geometryAddress,
        abi::RetailPointer32 rootAddress) noexcept
    {
#if defined(_MSC_VER) && defined(_M_IX86)
        constexpr std::size_t ParentOffset = 0x18u;
        if (geometryAddress == 0u || rootAddress == 0u)
            return false;
        abi::RetailPointer32 current = geometryAddress;
        __try
        {
            for (std::uint32_t depth = 0u;
                 current != 0u && depth < 64u;
                 ++depth)
            {
                if (current == rootAddress)
                    return true;
                std::memcpy(
                    &current,
                    reinterpret_cast<const std::uint8_t*>(
                        static_cast<std::uintptr_t>(current)) + ParentOffset,
                    sizeof(current));
            }
        }
        __except (1)
        {
            return false;
        }
#else
        static_cast<void>(geometryAddress);
        static_cast<void>(rootAddress);
#endif
        return false;
    }

    static bool geometryDescendsFromAnyFirstPersonRoot(
        const Context& context,
        abi::RetailPointer32 geometryAddress) noexcept
    {
        for (std::uint32_t index = 0u;
             index < context.mFirstPersonRootNodeCount;
             ++index)
        {
            if (geometryDescendsFrom(
                    geometryAddress,
                    context.mFirstPersonRootNodes[index]))
                return true;
        }
        return false;
    }

    static bool firstPersonGeometryAlreadyCollected(
        const Context& context,
        abi::RetailPointer32 geometryAddress) noexcept
    {
        for (std::uint32_t index = 0u;
             index < context.mFirstPersonQueueSafeGeometryCount;
             ++index)
        {
            if (context.mFirstPersonQueueSafeGeometryPointers[index]
                == geometryAddress)
                return true;
        }
        for (std::uint32_t index = 0u;
             index < context.mFirstPersonImmediateGeometryCount;
             ++index)
        {
            if (context.mFirstPersonImmediateGeometryPointers[index]
                == geometryAddress)
                return true;
        }
        return false;
    }

    static bool isDirectFirstPersonLeafRoot(
        const Context& context,
        abi::RetailPointer32 geometryAddress) noexcept
    {
        if (niObjectKind(geometryAddress) != 1)
            return false;
        for (std::uint32_t index = 0u;
             index < context.mFirstPersonRootNodeCount;
             ++index)
        {
            if (context.mFirstPersonRootNodes[index] == geometryAddress)
                return true;
        }
        return false;
    }

    static bool isRetailFirstPersonArmsGeometry(
        abi::RetailPointer32 geometryAddress) noexcept
    {
#if defined(_MSC_VER) && defined(_M_IX86)
        constexpr std::size_t ObjectNameOffset = 0x08u;
        __try
        {
            const char* name = nullptr;
            std::memcpy(
                &name,
                reinterpret_cast<const std::uint8_t*>(
                    static_cast<std::uintptr_t>(geometryAddress))
                    + ObjectNameOffset,
                sizeof(name));
            return name != nullptr
                && (std::strcmp(name, "Arms:0") == 0
                    || std::strcmp(name, "Arms:1") == 0);
        }
        __except (1)
        {
            return false;
        }
#else
        static_cast<void>(geometryAddress);
        return false;
#endif
    }

    static int niObjectKind(abi::RetailPointer32 objectAddress) noexcept
    {
#if defined(_MSC_VER) && defined(_M_IX86)
        struct NiRttiRaw
        {
            const char* name;
            NiRttiRaw* parent;
        };
        constexpr std::uintptr_t NiRttiNiAvObjectAddress = 0x011F4280u;
        constexpr std::uintptr_t NiRttiNiNodeAddress = 0x011F4428u;
        if (objectAddress == 0u)
            return 0;
        __try
        {
            void* object = reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(objectAddress));
            void** vtable = *reinterpret_cast<void***>(object);
            if (!vtable || !vtable[2])
                return 0;
            using GetTypeFunction = NiRttiRaw* (__thiscall*)(void*);
            NiRttiRaw* rtti = reinterpret_cast<GetTypeFunction>(
                vtable[2])(object);
            for (std::uint32_t depth = 0u; rtti && depth < 32u; ++depth)
            {
                const std::uintptr_t address =
                    reinterpret_cast<std::uintptr_t>(rtti);
                if (address == NiRttiNiNodeAddress)
                    return 2;
                if (address == NiRttiNiAvObjectAddress)
                    return 1;
                rtti = rtti->parent;
            }
        }
        __except (1)
        {
            return 0;
        }
#else
        static_cast<void>(objectAddress);
#endif
        return 0;
    }

    static bool isRetailNiGeometry(
        abi::RetailPointer32 objectAddress) noexcept
    {
#if defined(_MSC_VER) && defined(_M_IX86)
        constexpr std::size_t OnVisibleVtableOffset = 0xDCu;
        constexpr std::size_t PropertyOffset = 0x9Cu;
        constexpr std::uintptr_t RetailImageStart = 0x00400000u;
        constexpr std::uintptr_t RetailImageEnd = 0x01200000u;
        if (objectAddress == 0u)
            return false;
        __try
        {
            const auto* vtable = *reinterpret_cast<void* const* const*>(
                static_cast<std::uintptr_t>(objectAddress));
            const std::uintptr_t vtableAddress =
                reinterpret_cast<std::uintptr_t>(vtable);
            const std::uintptr_t onVisibleAddress = vtable
                ? reinterpret_cast<std::uintptr_t>(
                    vtable[OnVisibleVtableOffset / sizeof(void*)])
                : 0u;
            abi::RetailPointer32 propertyAddress = 0u;
            std::memcpy(
                &propertyAddress,
                reinterpret_cast<const std::uint8_t*>(
                    static_cast<std::uintptr_t>(objectAddress))
                    + PropertyOffset,
                sizeof(propertyAddress));
            if (propertyAddress < 0x00010000u)
                return false;
            const std::uintptr_t propertyVtable =
                static_cast<std::uintptr_t>(*reinterpret_cast<
                    const abi::RetailPointer32*>(
                        static_cast<std::uintptr_t>(propertyAddress)));
            return vtableAddress >= RetailImageStart
                && vtableAddress < RetailImageEnd
                && onVisibleAddress >= RetailImageStart
                && onVisibleAddress < RetailImageEnd
                && propertyVtable >= RetailImageStart
                && propertyVtable < RetailImageEnd;
        }
        __except (1)
        {
            return false;
        }
#else
        static_cast<void>(objectAddress);
        return false;
#endif
    }

    static void collectFirstPersonQueueSafeLeaves(
        Context& context,
        abi::RetailPointer32 objectAddress,
        std::uint32_t depth,
        std::uint32_t& visits) noexcept
    {
#if defined(_MSC_VER) && defined(_M_IX86)
        if (objectAddress == 0u || depth > 64u
            || ++visits > 4096u
            || context.mFirstPersonQueueSafeGeometryCount
                >= CollectorCapacity)
        {
            return;
        }
        constexpr std::size_t AvObjectFlagsOffset = 0x30u;
        std::uint32_t objectFlags = 0u;
        __try
        {
            std::memcpy(
                &objectFlags,
                reinterpret_cast<const std::uint8_t*>(
                    static_cast<std::uintptr_t>(objectAddress))
                    + AvObjectFlagsOffset,
                sizeof(objectFlags));
        }
        __except (1)
        {
            return;
        }
        const int kind = niObjectKind(objectAddress);
        // Fallout marks the separate first-person container hidden while
        // leaving its renderable arm children live for the dedicated view-
        // model pass. Traverse hidden nodes, but never replay a hidden
        // geometry leaf such as a body/limb cap.
        if (kind == 1 && (objectFlags & 1u) != 0u
            && !isRetailFirstPersonArmsGeometry(objectAddress))
            return;
        if (kind == 1)
        {
            // NiAVObject leaves also include non-geometry render helpers.
            // Admit only objects whose authenticated +0xDC virtual is the
            // retail NiGeometry::OnVisible implementation.
            if (!isRetailNiGeometry(objectAddress))
                return;
            if (firstPersonGeometryAlreadyCollected(context, objectAddress))
                return;
            if (geometryQueuesWithoutImmediateDispatch(
                    context.mCollectionAccumulator,
                    objectAddress))
            {
                context.mFirstPersonQueueSafeGeometryPointers[
                    context.mFirstPersonQueueSafeGeometryCount++] =
                        objectAddress;
            }
            else if (context.mFirstPersonImmediateGeometryCount
                < CollectorCapacity)
            {
                context.mFirstPersonImmediateGeometryPointers[
                    context.mFirstPersonImmediateGeometryCount++] =
                        objectAddress;
            }
            return;
        }
        if (kind != 2)
            return;
        struct NiTArrayRaw
        {
            abi::RetailPointer32 vtable;
            abi::RetailPointer32 data;
            std::uint16_t capacity;
            std::uint16_t firstFreeEntry;
            std::uint16_t numObjects;
            std::uint16_t growSize;
        };
        constexpr std::size_t ChildrenOffset = 0x9Cu;
        __try
        {
            const auto* children = reinterpret_cast<const NiTArrayRaw*>(
                static_cast<std::uintptr_t>(objectAddress) + ChildrenOffset);
            if (children->data == 0u
                || children->firstFreeEntry > children->capacity
                || children->firstFreeEntry > 1024u)
            {
                return;
            }
            const auto* entries = reinterpret_cast<
                const abi::RetailPointer32*>(
                static_cast<std::uintptr_t>(children->data));
            for (std::uint16_t index = 0u;
                 index < children->firstFreeEntry;
                 ++index)
            {
                collectFirstPersonQueueSafeLeaves(
                    context, entries[index], depth + 1u, visits);
            }
        }
        __except (1)
        {
            return;
        }
#else
        static_cast<void>(context);
        static_cast<void>(objectAddress);
        static_cast<void>(depth);
        static_cast<void>(visits);
#endif
    }

    static std::uint32_t collectAllFirstPersonQueueSafeLeaves(
        Context& context) noexcept
    {
        std::uint32_t totalVisits = 0u;
        for (std::uint32_t index = 0u;
             index < context.mFirstPersonRootNodeCount;
             ++index)
        {
            std::uint32_t rootVisits = 0u;
            collectFirstPersonQueueSafeLeaves(
                context,
                context.mFirstPersonRootNodes[index],
                0u,
                rootVisits);
            totalVisits += rootVisits;
        }
        return totalVisits;
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
        const std::uint32_t firstPersonTraversalVisits =
            diagnostics.firstPersonTraversalVisits;
        const std::uint32_t firstPersonQueueSafeItemCount =
            diagnostics.firstPersonQueueSafeItemCount;
        const std::uint32_t firstPersonImmediateItemCount =
            diagnostics.firstPersonImmediateItemCount;
        const abi::RetailPointer32 firstPersonRootNode =
            diagnostics.firstPersonRootNode;
        const std::uint32_t firstPersonPopulateCalls =
            diagnostics.firstPersonPopulateCalls;
        const std::uint32_t firstPersonRightQueueReplayCount =
            diagnostics.firstPersonRightQueueReplayCount;
        const std::uint32_t firstPersonRightImmediateCallbackCount =
            diagnostics.firstPersonRightImmediateCallbackCount;
        diagnostics = {};
        diagnostics.firstPersonTraversalVisits =
            firstPersonTraversalVisits;
        diagnostics.firstPersonQueueSafeItemCount =
            firstPersonQueueSafeItemCount;
        diagnostics.firstPersonImmediateItemCount =
            firstPersonImmediateItemCount;
        diagnostics.firstPersonRootNode = firstPersonRootNode;
        diagnostics.firstPersonPopulateCalls = firstPersonPopulateCalls;
        diagnostics.firstPersonRightQueueReplayCount =
            firstPersonRightQueueReplayCount;
        diagnostics.firstPersonRightImmediateCallbackCount =
            firstPersonRightImmediateCallbackCount;
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
        auto* renderingLane =
            accumulatorFromAddress(renderingAccumulator);
        if (ownedAccumulator)
        {
            diagnostics.rendererAccumulatorReferenceCount =
                ownedAccumulator->referenceCount;
        }
        if (renderingLane)
        {
            diagnostics.renderingAccumulatorReferenceCount =
                renderingLane->referenceCount;
        }
        // The renderer and accumulating globals share the coordinator-owned
        // accumulator at this call boundary. Retail may keep a distinct,
        // live rendering lane after its AccumulateScene call; preserve that
        // non-null lane exactly and restore it after the private eye work.
        if (!ownedAccumulator
            || ownedAccumulator->referenceCount < 4u
            || accumulatingAccumulator != rendererAccumulator
            || !renderingLane
            || renderingLane->referenceCount == 0u)
        {
            diagnostics.failure =
                RetailCenterAccumulatorSnapshotFailure::OwnerStateRejected;
            return false;
        }

        const bool retainDistinctRenderingLane =
            renderingAccumulator != rendererAccumulator;
        if (retainDistinctRenderingLane)
        {
            if (renderingLane->referenceCount
                == (std::numeric_limits<std::uint32_t>::max)())
            {
                diagnostics.failure =
                    RetailCenterAccumulatorSnapshotFailure::
                        RenderingLaneOwnershipRejected;
                return false;
            }
            const std::uint32_t retained =
                ++renderingLane->referenceCount;
            if (retained <= 1)
            {
                --renderingLane->referenceCount;
                diagnostics.failure =
                    RetailCenterAccumulatorSnapshotFailure::
                        RenderingLaneOwnershipRejected;
                return false;
            }
            diagnostics.renderingAccumulatorReferenceCount =
                retained;
            diagnostics.distinctRenderingAccumulatorRetained = true;
        }
        context.mEngineSnapshot = {
            renderer,
            rendererAccumulator,
            accumulatingAccumulator,
            renderingAccumulator,
            retainDistinctRenderingLane,
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
        // The retail owner setters are replacement operations, not
        // self-assignment-safe smart-pointer stores.  Calling one with its
        // already-installed accumulator can release the object before it
        // reloads the same raw address, leaving a null vtable for the next
        // stock RenderWorld pass.  Restore only owners that the private-eye
        // transaction actually changed.
        if (*context.mCalls.renderingAccumulator
            != snapshot.renderingAccumulator)
        {
            context.mCalls.setRenderingAccumulator(renderingAccumulator);
        }
        if (*context.mCalls.accumulatingAccumulator
            != snapshot.accumulatingAccumulator)
        {
            context.mCalls.setAccumulatingAccumulator(
                accumulatingAccumulator);
        }
        if (snapshot.renderer->accumulator
            != snapshot.rendererAccumulator)
        {
            context.mCalls.rendererSetAccumulator(
                snapshot.renderer,
                rendererAccumulator);
        }
        const bool restored = snapshot.renderer->accumulator
                == snapshot.rendererAccumulator
            && *context.mCalls.accumulatingAccumulator
                == snapshot.accumulatingAccumulator
            && *context.mCalls.renderingAccumulator
                == snapshot.renderingAccumulator;
        if (!restored)
            return false;
        if (snapshot.distinctRenderingAccumulatorRetained)
        {
            // Restoring the rendering owner reacquires its ordinary engine
            // reference before this temporary hold is released.  The
            // authenticated NiRefObject::Free thunk is a deallocator used for
            // factory-owned objects; it is not a reference decrement.  Match
            // the explicit increment from snapshot with one direct decrement.
            if (!renderingAccumulator
                || renderingAccumulator->referenceCount < 2u)
            {
                return false;
            }
            --renderingAccumulator->referenceCount;
        }
        context.mEngineSnapshot = {};
        return restored;
    }

    static void abandonEngineAccumulatorSnapshot(Context& context) noexcept
    {
        if (!context.mEngineSnapshot.active)
            return;
        const typename Context::EngineAccumulatorSnapshot snapshot =
            context.mEngineSnapshot;
        if (snapshot.distinctRenderingAccumulatorRetained)
        {
            auto* renderingAccumulator = accumulatorFromAddress(
                snapshot.renderingAccumulator);
            if (renderingAccumulator
                && renderingAccumulator->referenceCount >= 2u)
            {
                --renderingAccumulator->referenceCount;
            }
        }
        context.mEngineSnapshot = {};
    }

    static bool snapshot(void* opaque) noexcept
    {
        Context* context = checked(opaque);
        if (!context)
            return false;
        context->mLastTiming = {};
        context->mLastTiming.captured = true;
        context->mLastEyeCamera = {};
        ScopedStageTimer timer(
            context->mLastTiming.snapshotMilliseconds);
        context->mFrameSceneObject = nullptr;
        context->mActiveEyeCamera = nullptr;
        if (!snapshotEngineAccumulatorState(*context))
            return false;
        if (!context->mTargets.snapshot(context->mTargets.context))
        {
            abandonEngineAccumulatorSnapshot(*context);
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
        ScopedStageTimer timer(
            context->mLastTiming.collectMilliseconds);
        captureVisibilityDiagnostics(
            *context,
            camera,
            sceneObject,
            culler,
            generation);
        if (context->mDiagnosticStop
            == RetailCenterRendererDiagnosticStop::AfterSnapshot)
        {
            return rejectVisibility(
                *context,
                culler,
                RetailCenterVisibilityFailure::DiagnosticStop);
        }
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
        context->mLastVisibility.privateAccumulatorMode =
            reinterpret_cast<const std::uint8_t*>(
                context->mCollectionAccumulator)[0x30u];
        std::uint32_t queueSafeItemCount = 0u;
        for (std::uint32_t index = 0u; index < view.itemCount; ++index)
        {
            const abi::RetailPointer32 geometry =
                view.geometryPointers[index];
            if (geometry == 0u)
            {
                return rejectVisibility(
                    *context,
                    culler,
                    RetailCenterVisibilityFailure::VisibleArrayRejected);
            }
            if (geometryQueuesWithoutImmediateDispatch(
                    context->mCollectionAccumulator,
                    geometry))
            {
                context->mQueueSafeGeometryPointers[queueSafeItemCount] =
                    geometry;
                ++queueSafeItemCount;
            }
        }
        context->mLastVisibility.queueSafeItemCount = queueSafeItemCount;
        context->mLastVisibility.immediateGeometryRejectedCount =
            view.itemCount - queueSafeItemCount;
        context->mFirstPersonImmediateGeometryCount = 0u;
        context->mFirstPersonImmediateGeometryPointers.fill(0u);
        context->mFirstPersonQueueSafeGeometryCount = 0u;
        context->mFirstPersonQueueSafeGeometryPointers.fill(0u);
        if (context->mFirstPersonRootNodeCount != 0u)
        {
            for (std::uint32_t index = 0u; index < view.itemCount; ++index)
            {
                const abi::RetailPointer32 geometry =
                    view.geometryPointers[index];
                const bool firstPerson =
                    geometryDescendsFromAnyFirstPersonRoot(*context, geometry);
                const bool queueSafe = geometryQueuesWithoutImmediateDispatch(
                    context->mCollectionAccumulator,
                    geometry);
                if (firstPerson && queueSafe)
                {
                    context->mFirstPersonQueueSafeGeometryPointers[
                        context->mFirstPersonQueueSafeGeometryCount++] =
                            geometry;
                }
                else if (firstPerson)
                {
                    context->mFirstPersonImmediateGeometryPointers[
                        context->mFirstPersonImmediateGeometryCount++] =
                            geometry;
                }
            }
            // The stock center visible list intentionally excludes the
            // separate first-person branch. Walk that authenticated live
            // root directly so the right eye can receive its queue-safe
            // leaves even when the second per-frame cull stamp suppresses
            // the branch.
            context->mFirstPersonQueueSafeGeometryCount = 0u;
            context->mFirstPersonQueueSafeGeometryPointers.fill(0u);
            const std::uint32_t visits =
                collectAllFirstPersonQueueSafeLeaves(*context);
            context->mLastVisibility.firstPersonTraversalVisits = visits;
            context->mLastVisibility.firstPersonQueueSafeItemCount =
                context->mFirstPersonQueueSafeGeometryCount;
            context->mLastVisibility.firstPersonImmediateItemCount =
                context->mFirstPersonImmediateGeometryCount;
        }
        if (queueSafeItemCount == 0u)
        {
            return rejectVisibility(
                *context,
                culler,
                RetailCenterVisibilityFailure::NoQueueSafeGeometry);
        }

        const std::uintptr_t queueSafeGeometryAddress =
            reinterpret_cast<std::uintptr_t>(
                context->mQueueSafeGeometryPointers.data());
        if (queueSafeGeometryAddress
            > (std::numeric_limits<abi::RetailPointer32>::max)())
        {
            return rejectVisibility(
                *context,
                culler,
                RetailCenterVisibilityFailure::
                    VisibleArrayStorageAddressOutOfRange);
        }
        visibleSet.array = {
            static_cast<abi::RetailPointer32>(queueSafeGeometryAddress),
            queueSafeItemCount,
            static_cast<std::uint32_t>(CollectorCapacity),
            0u,
        };
        visibleSet.generation = generation;
        context->mFrameSceneObject = sceneObject;
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
        if (!context)
            return false;
        ScopedStageTimer timer(
            eye == CenterRendererEye::Left
                ? context->mLastTiming.leftBindMilliseconds
                : context->mLastTiming.rightBindMilliseconds);
        if (context
            && ((eye == CenterRendererEye::Left
                    && context->mDiagnosticStop
                        == RetailCenterRendererDiagnosticStop::
                            AfterVisibility)
                || (eye == CenterRendererEye::Right
                    && context->mDiagnosticStop
                        == RetailCenterRendererDiagnosticStop::
                            AfterLeftEye)))
        {
            return false;
        }
        return context->mTargets.bind(
                context->mTargets.context,
                eye,
                isolation);
    }

    static bool bindPreserving(
        void* opaque,
        CenterRendererEye eye,
        CenterRendererEyeIsolation& isolation) noexcept
    {
        Context* context = checked(opaque);
        return context
            && context->mTargets.bindPreservingContents
            && context->mTargets.bindPreservingContents(
                context->mTargets.context, eye, isolation);
    }

    static bool setCamera(
        void* opaque,
        abi::RetailBSShaderAccumulatorLayout* accumulator,
        abi::RetailNiCameraLayout* camera) noexcept
    {
        Context* context = checked(opaque);
        if (!context || !accumulator || !camera)
            return false;
        ScopedStageTimer timer(
            accumulator == context->mCollectionAccumulator
                ? context->mLastTiming.leftCameraMilliseconds
                : context->mLastTiming.rightCameraMilliseconds);
        if (accumulator == context->mCollectionAccumulator
            && context->mDiagnosticStop
                == RetailCenterRendererDiagnosticStop::AfterLeftBind)
        {
            return false;
        }
        if (!context->mEngineSnapshot.active)
            return false;
        auto* preparedRenderingLane = accumulatorFromAddress(
            context->mEngineSnapshot.renderingAccumulator);
        if (!preparedRenderingLane)
            return false;
        if (*context->mCalls.renderingAccumulator
            != context->mEngineSnapshot.renderingAccumulator)
        {
            context->mCalls.setRenderingAccumulator(
                preparedRenderingLane);
        }
        context->mActiveEyeCamera = camera;
        context->mCalls.accumulatorSetCamera(
            reinterpret_cast<abi::RetailNiAccumulatorLayout*>(accumulator),
            camera);
        const abi::RetailPointer32 expectedCamera =
            static_cast<abi::RetailPointer32>(
                reinterpret_cast<std::uintptr_t>(camera));
        RetailCenterEyeCameraDiagnostics& diagnostics =
            context->mLastEyeCamera;
        diagnostics.captured = true;
        if (accumulator == context->mCollectionAccumulator)
        {
            diagnostics.leftRendererCamera = accumulator->camera;
            diagnostics.leftRendererCameraMatches =
                accumulator->camera == expectedCamera;
        }
        else
        {
            diagnostics.rightRendererCamera = accumulator->camera;
            diagnostics.rightRendererCameraMatches =
                accumulator->camera == expectedCamera;
        }
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
        const auto populateStarted =
            std::chrono::steady_clock::now();
        if (accumulator == context->mCollectionAccumulator
            && context->mDiagnosticStop
                == RetailCenterRendererDiagnosticStop::AfterLeftCamera)
        {
            return false;
        }
        const auto* geometryPointers =
            reinterpret_cast<const abi::RetailPointer32*>(
                static_cast<std::uintptr_t>(
                    visibleArray->geometryPointers));
        if (!geometryPointers
            || visibleArray->itemCount == 0u
            || visibleArray->itemCount > visibleArray->capacity)
        {
            return false;
        }
        for (std::uint32_t index = 0u;
             index < visibleArray->itemCount;
             ++index)
        {
            if (!geometryQueuesWithoutImmediateDispatch(
                    accumulator,
                    geometryPointers[index]))
            {
                return false;
            }
        }
#if defined(_MSC_VER) && defined(_M_IX86)
        if (!context->mFrameSceneObject
            || !context->mActiveEyeCamera
            || !context->mBinding
            || !context->mEngineSnapshot.active)
        {
            return false;
        }
        // The production center path can consume its authenticated visible
        // array without entering the optional sealed-visibility diagnostic.
        // Walk xNVSE's exact live Weapon node here, in the real per-eye
        // population function, so the right eye always has the geometry
        // callbacks before its independent accumulator is rendered.
        context->mFirstPersonQueueSafeGeometryCount = 0u;
        context->mFirstPersonQueueSafeGeometryPointers.fill(0u);
        context->mFirstPersonImmediateGeometryCount = 0u;
        context->mFirstPersonImmediateGeometryPointers.fill(0u);
        std::uint32_t firstPersonVisits = 0u;
        context->mLastVisibility.firstPersonRootNode =
            context->mFirstPersonRootNodeCount != 0u
                ? context->mFirstPersonRootNodes[0]
                : 0u;
        ++context->mLastVisibility.firstPersonPopulateCalls;
        firstPersonVisits = collectAllFirstPersonQueueSafeLeaves(*context);
        context->mLastVisibility.firstPersonTraversalVisits =
            firstPersonVisits;
        context->mLastVisibility.firstPersonQueueSafeItemCount =
            context->mFirstPersonQueueSafeGeometryCount;
        context->mLastVisibility.firstPersonImmediateItemCount =
            context->mFirstPersonImmediateGeometryCount;
        auto* stockAccumulator = accumulatorFromAddress(
            context->mEngineSnapshot.rendererAccumulator);
        if (!stockAccumulator
            || stockAccumulator == accumulator
            || stockAccumulator->shadowScene == 0u
            || !context->mStockCapture.culler
            || context->mStockCapture.culler->shaderAccumulator
                != context->mEngineSnapshot.rendererAccumulator)
        {
            return false;
        }
        auto* privateCuller = context->mBinding->cullingProcess();
        const abi::RetailPointer32 cloneVtable =
            context->mBinding->ownedVtableAddressForDispatch();
        if (!privateCuller
            || cloneVtable == 0u
            || privateCuller->base.vtable != cloneVtable)
        {
            return false;
        }

        const std::uintptr_t accumulatorAddress =
            reinterpret_cast<std::uintptr_t>(accumulator);
        if (accumulatorAddress
            > (std::numeric_limits<abi::RetailPointer32>::max)())
        {
            return false;
        }
        const abi::RetailPointer32 accumulatorPointer =
            static_cast<abi::RetailPointer32>(accumulatorAddress);

        // Re-run the exact stock accumulation wrapper for each derived eye.
        // A visible-array replay reconstructs only NiAccumulator's sorted
        // geometry list; it does not execute the geometry/property callbacks
        // that populate BSShaderAccumulator's mode-0 render passes. The
        // private, constructor-owned culler gives each eye that complete stock
        // preparation without touching the live stock culler a second time.
        //
        // The BSShaderAccumulator constructor does not initialize +0x194,
        // while stock AccumulateScene immediately treats it as the raw
        // ShadowScene coordinator. The destructor body at 0x00B656E0 releases
        // the owned +0x198 and +0x1A0 fields but never releases +0x194. Copy
        // only this proven non-owning frame dependency; copying the complete
        // stock accumulator would alias its engine-owned pass lists.
        accumulator->shadowScene = stockAccumulator->shadowScene;
        bool accumulated = false;
        const abi::RetailPointer32 expectedCamera =
            static_cast<abi::RetailPointer32>(
                reinterpret_cast<std::uintptr_t>(context->mActiveEyeCamera));
        privateCuller->base.vtable = context->mStockCullerVtable;
        std::uint32_t mutationCount = 0u;
        __try
        {
            context->mCalls.cullingProcessSetAccumulator(
                privateCuller,
                accumulator);
            // First-person weapon leaves use NiAccumulator's immediate path.
            // Fallout stamps that path during the stock traversal, so either
            // private eye can accept a later OnVisible call while emitting no
            // pixels. Give both private eyes the complete path that proved
            // correct in the right eye: temporarily convert the authenticated
            // weapon leaves to the queued branch and restore the exact bytes
            // in the __finally block below.
            if (context->mFirstPersonRootNodeCount != 0u)
            {
                constexpr std::uint32_t GeometryImmediateDispatch = 0x40u;
                constexpr std::uint16_t PropertyAccumulatorQueue = 0x0001u;
                constexpr std::uint16_t PropertyModeExcluded = 0x2000u;
                for (std::uint32_t index = 0u;
                     index < context->mFirstPersonImmediateGeometryCount;
                     ++index)
                {
                    const abi::RetailPointer32 geometryAddress =
                        context->mFirstPersonImmediateGeometryPointers[index];
                    auto* geometry = reinterpret_cast<std::uint8_t*>(
                        static_cast<std::uintptr_t>(geometryAddress));
                    abi::RetailPointer32 propertyAddress = 0u;
                    std::memcpy(
                        &propertyAddress,
                        geometry + 0x9Cu,
                        sizeof(propertyAddress));
                    if (propertyAddress < 0x00010000u)
                        continue;
                    auto* property = reinterpret_cast<std::uint8_t*>(
                        static_cast<std::uintptr_t>(propertyAddress));
                    auto& mutation = context->mFirstPersonGeometryMutations[
                        mutationCount];
                    mutation.geometry = geometryAddress;
                    mutation.property = propertyAddress;
                    std::memcpy(
                        &mutation.geometryFlags,
                        geometry + 0x30u,
                        sizeof(mutation.geometryFlags));
                    std::memcpy(
                        &mutation.propertyFlags,
                        property + 0x18u,
                        sizeof(mutation.propertyFlags));
                    const std::uint32_t queuedGeometryFlags =
                        mutation.geometryFlags & ~GeometryImmediateDispatch;
                    const std::uint16_t queuedPropertyFlags =
                        static_cast<std::uint16_t>(
                            (mutation.propertyFlags
                                | PropertyAccumulatorQueue)
                            & ~PropertyModeExcluded);
                    std::memcpy(
                        geometry + 0x30u,
                        &queuedGeometryFlags,
                        sizeof(queuedGeometryFlags));
                    std::memcpy(
                        property + 0x18u,
                        &queuedPropertyFlags,
                        sizeof(queuedPropertyFlags));
                    mutation.applied = true;
                    ++mutationCount;
                }
            }
            context->mCalls.accumulateScene(
                context->mActiveEyeCamera,
                context->mFrameSceneObject,
                privateCuller);
            if (context->mFirstPersonRootNodeCount != 0u)
            {
                // The view model is a distinct live scene branch. Running the
                // authenticated wrapper with that root lets Fallout build its
                // weapon shader passes against the right-eye camera instead of
                // trying to smuggle view-model leaves through the world scene.
                for (std::uint32_t rootIndex = 0u;
                     rootIndex < context->mFirstPersonRootNodeCount;
                     ++rootIndex)
                {
                    const abi::RetailPointer32 rootAddress =
                        context->mFirstPersonRootNodes[rootIndex];
                    // AccumulateScene expects an NiNode-compatible scene
                    // branch. A standalone skinned NiGeometry (Fallout's
                    // RightHand sibling is one) must be admitted only through
                    // the authenticated visible-array/OnVisible replay below;
                    // traversing the leaf as a scene root emits a second,
                    // unskinned copy whose vertices stretch across the eye.
                    if (niObjectKind(rootAddress) != 2)
                        continue;
                    context->mCalls.accumulateScene(
                        context->mActiveEyeCamera,
                        reinterpret_cast<void*>(static_cast<std::uintptr_t>(
                            rootAddress)),
                        privateCuller);
                }
            }
            // A second stock cull in the same retail frame may reject the
            // first-person subtree through NiAVObject's per-frame cull stamp.
            // The stock center traversal already proved these exact geometry
            // pointers visible, and the ancestry check above proves they are
            // inside xNVSE's current first-person root. Replay only that
            // queue-safe subset into each eye's already-prepared accumulator.
            if (context->mFirstPersonQueueSafeGeometryCount != 0u)
            {
                const abi::RetailNiVisibleArrayLayout firstPersonVisible {
                    static_cast<abi::RetailPointer32>(
                        reinterpret_cast<std::uintptr_t>(
                            context->mFirstPersonQueueSafeGeometryPointers.data())),
                    context->mFirstPersonQueueSafeGeometryCount,
                    static_cast<std::uint32_t>(CollectorCapacity),
                    0u,
                };
                context->mCalls.accumulatorAddVisibleArray(
                    reinterpret_cast<abi::RetailNiAccumulatorLayout*>(
                        accumulator),
                    const_cast<abi::RetailNiVisibleArrayLayout*>(
                        &firstPersonVisible));
                context->mLastVisibility.firstPersonRightQueueReplayCount =
                    context->mFirstPersonQueueSafeGeometryCount;
            }
            if (mutationCount != 0u)
            {
                const abi::RetailNiVisibleArrayLayout firstPersonImmediate {
                    static_cast<abi::RetailPointer32>(
                        reinterpret_cast<std::uintptr_t>(
                            context->mFirstPersonImmediateGeometryPointers.data())),
                    mutationCount,
                    static_cast<std::uint32_t>(CollectorCapacity),
                    0u,
                };
                context->mCalls.accumulatorAddVisibleArray(
                    reinterpret_cast<abi::RetailNiAccumulatorLayout*>(
                        accumulator),
                    const_cast<abi::RetailNiVisibleArrayLayout*>(
                        &firstPersonImmediate));
                context->mLastVisibility.firstPersonRightQueueReplayCount +=
                    mutationCount;
            }
            if (context->mFirstPersonRootNodeCount != 0u)
            {
                // Immediate first-person meshes are emitted through the
                // authenticated NiGeometry::OnVisible virtual, not the
                // accumulator's queued list. Re-run that exact engine
                // callback with the private right-eye culler so its current
                // accumulator receives a complete independent weapon pass.
                for (std::uint32_t index = mutationCount;
                     index < context->mFirstPersonImmediateGeometryCount;
                     ++index)
                {
                    void* geometry = reinterpret_cast<void*>(
                        static_cast<std::uintptr_t>(
                            context->mFirstPersonImmediateGeometryPointers[
                                index]));
                    void** vtable = *reinterpret_cast<void***>(geometry);
                    auto onVisible = reinterpret_cast<
                        abi::GeometryOnVisibleFunction>(vtable[0xDCu / 4u]);
                    onVisible(
                        geometry,
                        reinterpret_cast<abi::RetailNiCullingProcessLayout*>(
                            privateCuller));
                    ++context->mLastVisibility
                          .firstPersonRightImmediateCallbackCount;
                }
            }
            accumulated =
                privateCuller->shaderAccumulator == accumulatorPointer
                && context->mEngineSnapshot.renderer->accumulator
                    == accumulatorPointer
                && *context->mCalls.accumulatingAccumulator
                    == accumulatorPointer
                && *context->mCalls.renderingAccumulator
                    == context->mEngineSnapshot.renderingAccumulator;
        }
        __finally
        {
            while (mutationCount > 0u)
            {
                auto& mutation = context->mFirstPersonGeometryMutations[
                    --mutationCount];
                if (mutation.applied)
                {
                    auto* geometry = reinterpret_cast<std::uint8_t*>(
                        static_cast<std::uintptr_t>(mutation.geometry));
                    auto* property = reinterpret_cast<std::uint8_t*>(
                        static_cast<std::uintptr_t>(mutation.property));
                    std::memcpy(
                        geometry + 0x30u,
                        &mutation.geometryFlags,
                        sizeof(mutation.geometryFlags));
                    std::memcpy(
                        property + 0x18u,
                        &mutation.propertyFlags,
                        sizeof(mutation.propertyFlags));
                    mutation = {};
                }
            }
            privateCuller->base.vtable = cloneVtable;
        }
        RetailCenterEyeCameraDiagnostics& diagnostics =
            context->mLastEyeCamera;
        diagnostics.captured = true;
        if (accumulator == context->mCollectionAccumulator)
        {
            diagnostics.leftCullerCamera = privateCuller->base.camera;
            diagnostics.leftCullerCameraMatches =
                privateCuller->base.camera == expectedCamera;
        }
        else
        {
            diagnostics.rightCullerCamera = privateCuller->base.camera;
            diagnostics.rightCullerCameraMatches =
                privateCuller->base.camera == expectedCamera;
        }
        context->mActiveEyeCamera = nullptr;
        const double populateMilliseconds =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - populateStarted).count();
        if (accumulator == context->mCollectionAccumulator)
        {
            context->mLastTiming.leftPopulateMilliseconds =
                populateMilliseconds;
        }
        else
        {
            context->mLastTiming.rightPopulateMilliseconds =
                populateMilliseconds;
        }
        return accumulated
            && context->mBinding->ownedVtableIntegrityValid();
#else
        return false;
#endif
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
        ScopedStageTimer timer(
            accumulator == context->mCollectionAccumulator
                ? context->mLastTiming.leftRenderMilliseconds
                : context->mLastTiming.rightRenderMilliseconds);
        if (accumulator == context->mCollectionAccumulator
            && context->mDiagnosticStop
                == RetailCenterRendererDiagnosticStop::AfterLeftPopulate)
        {
            return false;
        }
        // The private accumulator contains the same queue-safe world geometry
        // consumed by stock mode 0. Enter the authenticated retail world
        // wrapper once and let BSShaderAccumulator::Render select its native
        // mode-0 path. An auxiliary mode-10 prepass is not part of
        // RenderWorldSceneGraph: it enters a shader callback which assumes a
        // separate renderer-global array was initialized by that auxiliary
        // pipeline. Replaying it here faults before the world render begins.
        constexpr std::uint32_t WorldRenderMode = 0u;
        if (accumulator->renderMode != WorldRenderMode)
            return false;
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
        ScopedStageTimer timer(
            accumulator == context->mCollectionAccumulator
                ? context->mLastTiming.leftFinalizeMilliseconds
                : context->mLastTiming.rightFinalizeMilliseconds);
        if (accumulator == context->mCollectionAccumulator
            && context->mDiagnosticStop
                == RetailCenterRendererDiagnosticStop::AfterLeftRender)
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
        if (!context)
            return false;
        ScopedStageTimer timer(
            eye == CenterRendererEye::Left
                ? context->mLastTiming.leftEndMilliseconds
                : context->mLastTiming.rightEndMilliseconds);
        if (context
            && eye == CenterRendererEye::Left
            && context->mDiagnosticStop
                == RetailCenterRendererDiagnosticStop::AfterLeftFinalize)
        {
            return false;
        }
        return context->mTargets.end(
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
            ScopedStageTimer timer(
                context->mLastTiming.rollbackMilliseconds);
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
        ScopedStageTimer timer(
            context->mLastTiming.restoreMilliseconds);
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
            context->mQueueSafeGeometryPointers.fill(0u);
            context->mFrameSceneObject = nullptr;
            context->mActiveEyeCamera = nullptr;
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
        result.bindEyeTargetsPreservingContents = &bindPreserving;
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
