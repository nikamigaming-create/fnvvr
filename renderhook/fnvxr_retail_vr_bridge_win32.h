#pragma once

#include "fnvxr_gpu_color_publisher_win32.h"

#include "../runtime/fnvxr_retail_center_runtime.h"
#include "../runtime/fnvxr_retail_first_person_hook_lease.h"
#include "../runtime/fnvxr_retail_runtime_authority.h"
#include "../runtime/fnvxr_retail_tracked_frame_win32.h"
#include "../runtime/fnvxr_retail_world_accumulation_controller.h"
#include "../runtime/fnvxr_retail_world_accumulation_hook_win32.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace fnvxr::d3d9
{
// The two routes have materially different guarantees.  Keep their selection
// explicit: adding a GPU callback to a CPU bridge must never silently leave
// the CPU publication active, and a GPU bridge must never retain a CPU
// fallback that can hide a failed shared-texture transport.
enum class RetailVrPublicationTransport : std::uint8_t
{
    Unspecified = 0u,
    CpuReadback,
    GpuSharedTextures,
};

struct RetailVrBridgeOperations
{
    void* context = nullptr;
    RetailVrPublicationTransport publicationTransport =
        RetailVrPublicationTransport::Unspecified;
    engine::RetailEyeTargetOperations eyeTargets {};
    // The replacement E8 must enter the verified stock target with the
    // caller's original register/stack context.  The proxy owns that naked
    // relay, while the bridge owns the only authenticated target address and
    // arms it immediately before the accumulation calls are patched.
    bool (*armAccumulationCallRelay)(void*, std::uintptr_t) noexcept =
        nullptr;
    void (*disarmAccumulationCallRelay)(void*) noexcept = nullptr;
    // The branch-local post-accumulation relays split the stock render and
    // finalize phases without globally hooking either shared helper.
    bool (*armRenderPhaseCallRelays)(
        void*,
        std::uintptr_t,
        std::uintptr_t) noexcept = nullptr;
    void (*disarmRenderPhaseCallRelays)(void*) noexcept = nullptr;
    // RenderFirstPerson is sealed to three exact local E8 callsites. The
    // proxy relay enters the normal stock call first, then asks the bridge to
    // replay that same verified call frame once per eye before publication.
    bool (*armFirstPersonCallRelay)(void*, std::uintptr_t) noexcept = nullptr;
    void (*disarmFirstPersonCallRelay)(void*) noexcept = nullptr;
    // The sole persistent accumulator B6C0D0 call inside RenderFirstPerson
    // has a distinct relay.  During the bridge's two private stock calls it
    // runs the stock non-finalizing half; the following ordinary desktop call
    // still invokes the exact stock render-and-finalize wrapper.
    bool (*armFirstPersonPreparedRenderRelay)(
        void*, std::uintptr_t) noexcept = nullptr;
    void (*disarmFirstPersonPreparedRenderRelay)(void*) noexcept = nullptr;
    // A stock first-person invocation can temporarily rebind the desktop
    // backbuffer it saved before this bridge bound a private eye target. The
    // proxy owns the narrow, per-eye redirect lease for that exact saved
    // target. It is optional for non-D3D9 test backends, but when present it
    // brackets one complete stock RenderFirstPerson invocation.
    bool (*beginFirstPersonEyeTargetRedirect)(
        void*, engine::CenterRendererEye) noexcept = nullptr;
    void (*endFirstPersonEyeTargetRedirect)(void*) noexcept = nullptr;
    // The desktop first-person pass begins against a fresh depth buffer. The
    // private eye must preserve its already-rendered world color while giving
    // the weapon pass that same depth-only starting condition.
    bool (*prepareFirstPersonEyeDepth)(
        void*, engine::CenterRendererEye) noexcept = nullptr;
    // The stock first-person pass is a complete D3D9 transaction. The proxy
    // may restore the pre-pass device state between private eyes so each
    // invocation starts from the same proven GPU pipeline boundary.
    bool (*restoreFirstPersonEyeD3dState)(void*) noexcept = nullptr;
    // Read-only compatibility evidence for the exact JIP first-person call
    // normalization.  The bridge never writes this state; exposing it here
    // lets an eye transaction prove whether the compatibility guard or one of
    // the engine-owned accumulators changed between the two stock calls.
    std::int32_t (*readFirstPersonCompatibilityGuard)(void*) noexcept =
        nullptr;
    // xNVSE publishes the authoritative gameplay + weapon-out state.  The
    // outer first-person lease stays stock until that state is stable.
    bool (*firstPersonGameplayLeaseReady)(void*) noexcept = nullptr;
    bool (*prepareDistinctCameraFrame)(
        void*,
        const engine::RetailWorldAccumulationCallFrame&,
        const engine::RetailTrackedFrame&,
        std::uint64_t,
        engine::RetailCenterRuntimeFrame&) noexcept = nullptr;
    bool (*prepareColorProducer)(void*, std::uint64_t) noexcept = nullptr;
    color_transport::ProducerPublication (*produceColorPair)(
        void*,
        const color_transport::ProducerFrameIdentity&) noexcept = nullptr;
    // Ordinary D3D9 cannot export the eye textures to D3D11. The exact-retail
    // bridge may instead synchronously read back and publish the two private
    // engine targets after the same world transaction completes.
    bool (*publishCpuPair)(
        void*,
        const engine::RetailTrackedFrame&,
        std::uint64_t) noexcept = nullptr;
    // A confirmed retail menu is copied to the same private ordinary-D3D9
    // targets, then published as a mono record through the CPU transport.
    // It must share the exact UI/world transaction domain with publishCpuPair
    // so a host can never revive a world pair captured before the menu.
    bool (*publishCpuMonoUiQuad)(
        void*,
        const engine::RetailTrackedFrame&,
        std::uint64_t) noexcept = nullptr;
};

constexpr bool retailVrBridgeOperationsComplete(
    const RetailVrBridgeOperations& operations) noexcept
{
    const bool gpuPublicationComplete = operations.prepareColorProducer
        && operations.produceColorPair
        && !operations.publishCpuPair
        && !operations.publishCpuMonoUiQuad;
    const bool cpuPublicationComplete = operations.publishCpuPair
        && operations.publishCpuMonoUiQuad
        && !operations.prepareColorProducer
        && !operations.produceColorPair;
    const bool selectedPublicationComplete =
        (operations.publicationTransport
                == RetailVrPublicationTransport::CpuReadback
            && cpuPublicationComplete)
        || (operations.publicationTransport
                == RetailVrPublicationTransport::GpuSharedTextures
            && gpuPublicationComplete);
    return operations.context
        && engine::retailEyeTargetOperationsComplete(operations.eyeTargets)
        && operations.eyeTargets.bindPreservingContents
        && operations.armAccumulationCallRelay
        && operations.disarmAccumulationCallRelay
        && operations.armRenderPhaseCallRelays
        && operations.disarmRenderPhaseCallRelays
        && operations.armFirstPersonCallRelay
        && operations.disarmFirstPersonCallRelay
        && operations.armFirstPersonPreparedRenderRelay
        && operations.disarmFirstPersonPreparedRenderRelay
        && operations.firstPersonGameplayLeaseReady
        && ((operations.beginFirstPersonEyeTargetRedirect == nullptr)
            == (operations.endFirstPersonEyeTargetRedirect == nullptr))
        && operations.prepareDistinctCameraFrame
        && selectedPublicationComplete;
}

class RetailV5PublicationSequence final
{
public:
    bool claim(
        gpu::color_v5::PresentationMode mode,
        std::uint64_t& transactionId) noexcept
    {
        transactionId = 0u;
        if (!gpu::color_v5::validPresentationMode(mode)
            || mNext == 0u
            || mNext == (std::numeric_limits<std::uint64_t>::max)())
        {
            return false;
        }
        transactionId = mNext++;
        return true;
    }

private:
    std::uint64_t mNext = 1u;
};

enum class RetailVrBridgeFailure : std::uint8_t
{
    None = 0u,
    UnsupportedArchitecture,
    OperationsIncomplete,
    AlreadyInitialized,
    RuntimeAuthorityRejected,
    EngineCallsRejected,
    CullerVtableRelocationRejected,
    CenterRuntimeRejected,
    TrackedFrameReaderRejected,
    ColorPublisherRejected,
    ColorProducerRejected,
    AccumulationControllerRejected,
    AccumulationHookMemoryRejected,
    AccumulationHookInstallRejected,
    FirstPersonHookMemoryRejected,
    FirstPersonHookInstallRejected,
};

struct RetailVrBridgeFrameDiagnostics
{
    struct EyeCamera
    {
        engine::RetailEyeCameraFailure failure =
            engine::RetailEyeCameraFailure::FactoryUnavailable;
        std::uint32_t stockVtable = 0u;
        engine::RetailNiTransformLayout stockWorld {};
        engine::abi::RetailNiFrustumLayout stockFrustum {};
        float gameUnitsPerMeter = 0.0f;
        float hmdRot[4] {};
        float hmdPos[3] {};
        float leftEyePos[3] {};
        float rightEyePos[3] {};
        float leftFov[4] {};
        float rightFov[4] {};
        float eyeBaselineMeters = 0.0f;
        float eyeMidpointDistanceMeters = 0.0f;
        bool stockTransformUsable = false;
        bool stockFrustumUsable = false;
        bool leftFovUsable = false;
        bool rightFovUsable = false;
        bool eyeBaselineValid = false;
        bool captured = false;
    };

    engine::RetailWorldAccumulationControllerResult controller {};
    engine::RetailCenterRuntimeFrameResult renderer {};
    EyeCamera eyeCamera {};
    engine::RetailCenterVisibilityDiagnostics visibility {};
    engine::RetailCenterAccumulatorSnapshotDiagnostics accumulatorSnapshot {};
    engine::RetailCenterRendererTimingDiagnostics rendererTiming {};
    engine::RetailCenterEyeCameraDiagnostics rendererCullerCamera {};
    struct FirstPerson
    {
        struct EngineState
        {
            struct ContentDelta
            {
                std::uint32_t beforeHash = 0u;
                std::uint32_t afterHash = 0u;
                std::uint32_t changedByteCount = 0u;
                std::uint16_t firstChangedOffset =
                    (std::numeric_limits<std::uint16_t>::max)();
                std::uint32_t firstWordBefore = 0u;
                std::uint32_t firstWordAfter = 0u;
                bool captured = false;
            };

            std::uint32_t rendererAddress = 0u;
            std::uint32_t rendererAccumulator = 0u;
            std::uint32_t accumulatingAccumulator = 0u;
            std::uint32_t renderingAccumulator = 0u;
            std::uint32_t rendererAccumulatorReferenceCount = 0u;
            std::uint32_t renderingAccumulatorReferenceCount = 0u;
            std::int32_t compatibilityGuard =
                (std::numeric_limits<std::int32_t>::min)();
            ContentDelta rendererInstanceContents {};
            ContentDelta cullingProcessContents {};
            ContentDelta rendererAccumulatorContents {};
            ContentDelta renderingAccumulatorContents {};
            bool captured = false;
        };

        std::uint64_t stagedGeneration = 0u;
        std::uint64_t stagedTransactionId = 0u;
        std::uint64_t stagedPoseFrame = 0u;
        std::int32_t stagedPoseSequence = 0;
        std::uint32_t callerId = 0u;
        std::uint64_t dispatchCount = 0u;
        std::uint64_t completeCount = 0u;
        engine::RetailEyeCameraFailure cameraFailure =
            engine::RetailEyeCameraFailure::StockCameraRejected;
        bool leftRendered = false;
        bool rightRendered = false;
        bool targetsRestored = false;
        bool published = false;
        bool leftDepthPrepared = false;
        bool rightDepthPrepared = false;
        bool leftAccumulatorTransactionEntered = false;
        bool rightAccumulatorTransactionEntered = false;
        bool leftAccumulatorRestored = false;
        bool rightAccumulatorRestored = false;
        bool interEyeD3dStateRestored = false;
        bool privatePairReady = false;
        bool desktopFinalizerCompleted = false;
        EngineState leftBefore {};
        EngineState leftAfter {};
        EngineState leftRestored {};
        EngineState rightBefore {};
        EngineState rightAfter {};
        EngineState rightRestored {};
    } firstPerson {};
    std::uint64_t dispatchCount = 0u;
    std::uint64_t stereoCompleteCount = 0u;
};

// Private-eye rendering can itself enter the retail AccumulateScene callsites
// (shadow/auxiliary scene passes).  Those nested stock calls must still run,
// but they must not recursively capture into or dispatch the one bridge
// transaction that is currently consuming the sealed visible set.
class RetailVrPrivateRenderDispatchGate final
{
public:
    bool tryEnter() noexcept
    {
        if (mActive)
            return false;
        mActive = true;
        return true;
    }

    void leave() noexcept
    {
        mActive = false;
    }

    bool active() const noexcept
    {
        return mActive;
    }

private:
    bool mActive = false;
};

template <std::size_t CollectorCapacity>
class RetailVrBridgeWin32 final
{
public:
    RetailVrBridgeWin32() noexcept = default;
    ~RetailVrBridgeWin32() noexcept
    {
        delete mFirstPersonHookLease;
        mFirstPersonHookLease = nullptr;
        if (mFirstPersonPreparedRenderRelayArmed
            && mOperations.disarmFirstPersonPreparedRenderRelay)
        {
            mOperations.disarmFirstPersonPreparedRenderRelay(
                mOperations.context);
            mFirstPersonPreparedRenderRelayArmed = false;
        }
        if (mFirstPersonRelayArmed
            && mOperations.disarmFirstPersonCallRelay)
        {
            mOperations.disarmFirstPersonCallRelay(mOperations.context);
            mFirstPersonRelayArmed = false;
        }
        delete mAccumulationHookLease;
        mAccumulationHookLease = nullptr;
        if (mRenderPhaseRelaysArmed
            && mOperations.disarmRenderPhaseCallRelays)
        {
            mOperations.disarmRenderPhaseCallRelays(mOperations.context);
            mRenderPhaseRelaysArmed = false;
        }
        if (mAccumulationRelayArmed
            && mOperations.disarmAccumulationCallRelay)
        {
            mOperations.disarmAccumulationCallRelay(mOperations.context);
            mAccumulationRelayArmed = false;
        }
    }

    RetailVrBridgeWin32(const RetailVrBridgeWin32&) = delete;
    RetailVrBridgeWin32& operator=(const RetailVrBridgeWin32&) = delete;

    bool initialize(
        const RetailVrBridgeOperations& operations,
        std::uintptr_t accumulationAdapterAddress,
        std::uintptr_t renderWithoutFinalizeAdapterAddress,
        std::uintptr_t finalizeAdapterAddress,
        std::uintptr_t renderAndFinalizeAdapterAddress,
        std::uintptr_t firstPersonAdapterAddress,
        std::uintptr_t firstPersonPreparedRenderAdapterAddress,
        std::uintptr_t firstPersonPrimaryAdapterAddress = 0u,
        std::uintptr_t firstPersonAlternateAdapterAddress = 0u,
        std::uintptr_t firstPersonThirdAdapterAddress = 0u) noexcept
    {
#if !defined(_WIN32) || !defined(_M_IX86)
        (void)operations;
        (void)accumulationAdapterAddress;
        (void)renderWithoutFinalizeAdapterAddress;
        (void)finalizeAdapterAddress;
        (void)renderAndFinalizeAdapterAddress;
        (void)firstPersonAdapterAddress;
        (void)firstPersonPreparedRenderAdapterAddress;
        (void)firstPersonPrimaryAdapterAddress;
        (void)firstPersonAlternateAdapterAddress;
        (void)firstPersonThirdAdapterAddress;
        mFailure = RetailVrBridgeFailure::UnsupportedArchitecture;
        return false;
#else
        using engine::abi::BSCullingProcessVtableAddress;
        using engine::SupportedImageBase;
        if (mInitialized || mAccumulationHookLease || mFirstPersonHookLease)
            return fail(RetailVrBridgeFailure::AlreadyInitialized);
        if (!retailVrBridgeOperationsComplete(operations)
            || accumulationAdapterAddress == 0u
            || accumulationAdapterAddress > 0xFFFFFFFFu
            || renderWithoutFinalizeAdapterAddress == 0u
            || renderWithoutFinalizeAdapterAddress > 0xFFFFFFFFu
            || finalizeAdapterAddress == 0u
            || finalizeAdapterAddress > 0xFFFFFFFFu
            || renderAndFinalizeAdapterAddress == 0u
            || renderAndFinalizeAdapterAddress > 0xFFFFFFFFu
            || firstPersonAdapterAddress == 0u
            || firstPersonAdapterAddress > 0xFFFFFFFFu
            || firstPersonPreparedRenderAdapterAddress == 0u
            || firstPersonPreparedRenderAdapterAddress > 0xFFFFFFFFu
            || firstPersonPrimaryAdapterAddress == 0u
            || firstPersonPrimaryAdapterAddress > 0xFFFFFFFFu
            || firstPersonAlternateAdapterAddress == 0u
            || firstPersonAlternateAdapterAddress > 0xFFFFFFFFu
            || firstPersonThirdAdapterAddress == 0u
            || firstPersonThirdAdapterAddress > 0xFFFFFFFFu)
        {
            return fail(RetailVrBridgeFailure::OperationsIncomplete);
        }

        mOperations = operations;
        mAuthority = engine::authorizeCurrentRetailRuntimeAtDecisionPoint();
        if (!mAuthority.complete())
            return fail(RetailVrBridgeFailure::RuntimeAuthorityRejected);
        const auto& authority = mAuthority.authority;
        const auto& metadata = authority.metadata();
        mCalls = engine::resolveRetailEngineCalls(
            authority.engineCalls(),
            metadata.runtimeImageBase);
        if (!mCalls.complete()
            || !mCalls.calls.firstPersonRenderComplete())
            return fail(RetailVrBridgeFailure::EngineCallsRejected);

        if (BSCullingProcessVtableAddress < SupportedImageBase
            || metadata.runtimeImageBase
                > (std::numeric_limits<std::uintptr_t>::max)()
                    - (BSCullingProcessVtableAddress - SupportedImageBase))
        {
            return fail(
                RetailVrBridgeFailure::CullerVtableRelocationRejected);
        }
        const std::uintptr_t cullerVtableAddress =
            metadata.runtimeImageBase
            + (BSCullingProcessVtableAddress - SupportedImageBase);
        if (cullerVtableAddress == 0u || cullerVtableAddress > 0xFFFFFFFFu)
        {
            return fail(
                RetailVrBridgeFailure::CullerVtableRelocationRejected);
        }
        const auto* cullerVtable = reinterpret_cast<
            const engine::abi::RetailPointer32*>(cullerVtableAddress);
        if (!mCenterRuntime.initialize(
                mCalls,
                metadata.runtimeImageBase,
                cullerVtable,
                metadata.generation,
                authority.stereoResources(),
                operations.eyeTargets))
        {
            return fail(RetailVrBridgeFailure::CenterRuntimeRejected);
        }
        // NVSE plugins are preloaded before WinMain, but the plugin-owned
        // runtime-state mapping is first published from its authenticated
        // main-loop callback.  The D3D device and this world hook can therefore
        // exist earlier.  Initialize the reader's mapping names now and let
        // readPublishedFrame() reopens the two mappings lazily; the controller
        // then makes the one authoritative UI-versus-world route decision.
        // Both routes remain fail-closed until stable publications exist.
        static_cast<void>(mTrackedFrames.initialize());
        mResourceSetId = metadata.generation;
        if (mResourceSetId == 0u)
            mResourceSetId = 1u;
        mCpuPublication = mOperations.publicationTransport
            == RetailVrPublicationTransport::CpuReadback;
        if (!mCpuPublication && !mPublisher.initialize())
            return fail(RetailVrBridgeFailure::ColorPublisherRejected);
        if (!mCpuPublication
            && !mOperations.prepareColorProducer(
                mOperations.context,
                mResourceSetId))
        {
            return fail(RetailVrBridgeFailure::ColorProducerRejected);
        }

        engine::RetailWorldAccumulationControllerOperations controllerOperations {};
        controllerOperations.context = this;
        controllerOperations.readTrackedFrame = &readTrackedFrame;
        controllerOperations.claimWorldTransaction = &claimWorldTransaction;
        controllerOperations.prepareDistinctCameraFrame = &prepareCameras;
        controllerOperations.renderStereoWorld = &renderStereo;
        controllerOperations.publishGpuPair = &publishGpuPair;
        if (!mController.initialize(controllerOperations))
            return fail(RetailVrBridgeFailure::AccumulationControllerRejected);

        if (!mAccumulationHookMemory.initialize(metadata.runtimeWorldAddress))
        {
            return fail(
                RetailVrBridgeFailure::AccumulationHookMemoryRejected);
        }
        // The relay is armed before the E8 writes.  This eliminates the
        // transient in which a render thread could land in the relay without
        // its exact stock target, and keeps the target authority inside this
        // already-revalidated bridge.
        if (!mOperations.armAccumulationCallRelay(
                mOperations.context,
                reinterpret_cast<std::uintptr_t>(mCalls.calls.accumulateScene)))
        {
            return fail(RetailVrBridgeFailure::AccumulationHookInstallRejected);
        }
        mAccumulationRelayArmed = true;
        if (!mOperations.armRenderPhaseCallRelays(
                mOperations.context,
                reinterpret_cast<std::uintptr_t>(
                    mCalls.calls.renderAccumulatorWithoutFinalize),
                reinterpret_cast<std::uintptr_t>(
                    mCalls.calls.finalizeAccumulator)))
        {
            return fail(RetailVrBridgeFailure::AccumulationHookInstallRejected);
        }
        mRenderPhaseRelaysArmed = true;
        engine::RetailWorldAccumulationHookInstallResult installed =
            engine::installRetailWorldAccumulationHook(
                authority.worldHook(),
                mAccumulationHookMemory.operations(),
                metadata.runtimeWorldAddress,
                accumulationAdapterAddress,
                renderWithoutFinalizeAdapterAddress,
                finalizeAdapterAddress,
                renderAndFinalizeAdapterAddress);
        if (!installed.complete())
        {
            return fail(
                RetailVrBridgeFailure::AccumulationHookInstallRejected);
        }
        mAccumulationHookLease = new (std::nothrow)
            engine::RetailWorldAccumulationHookLease(
            std::move(installed.lease));
        if (!mAccumulationHookLease || !mAccumulationHookLease->installed())
        {
            delete mAccumulationHookLease;
            mAccumulationHookLease = nullptr;
            return fail(
                RetailVrBridgeFailure::AccumulationHookInstallRejected);
        }
        // Lease the three authenticated outer RenderFirstPerson callers so a
        // completed world pair can receive the controller-driven stock weapon
        // pass before publication.  The lease is installed only after xNVSE
        // proves gameplay + weapon-out, so the inner persistent-accumulator
        // relay cannot affect menu/loading world output.  During the two
        // private eye passes it invokes the non-finalizing stock render half;
        // the following desktop call remains the one stock finalizer.
        if (!mFirstPersonHookMemory.initialize(metadata.runtimeImageBase))
        {
            return fail(RetailVrBridgeFailure::FirstPersonHookMemoryRejected);
        }
        if (!mOperations.armFirstPersonCallRelay(
                mOperations.context,
                reinterpret_cast<std::uintptr_t>(mCalls.calls.renderFirstPerson)))
        {
            return fail(RetailVrBridgeFailure::FirstPersonHookInstallRejected);
        }
        mFirstPersonRelayArmed = true;
        std::uintptr_t stockPreparedRenderAddress = 0u;
        if (!engine::relocateRetailFirstPersonPreferredAddress(
                metadata.runtimeImageBase,
                engine::RetailFirstPersonPreparedRenderAndFinalizeAddress,
                stockPreparedRenderAddress)
            || !mOperations.armFirstPersonPreparedRenderRelay(
                mOperations.context,
                stockPreparedRenderAddress))
        {
            return fail(RetailVrBridgeFailure::FirstPersonHookInstallRejected);
        }
        mFirstPersonPreparedRenderRelayArmed = true;
        mFirstPersonRelays = {
            firstPersonAdapterAddress,
            firstPersonPreparedRenderAdapterAddress,
            firstPersonPrimaryAdapterAddress,
            firstPersonAlternateAdapterAddress,
            firstPersonThirdAdapterAddress,
        };
        // Keep every outer caller stock through menu/loading.  The lease is
        // installed lazily after the first complete gameplay world pair, so
        // synchronous fixture loading can never enter a private render relay.
        mFailure = RetailVrBridgeFailure::None;
        mInitialized = true;
        return true;
#endif
    }

    bool ready() const noexcept
    {
        return mInitialized
            && mFailure == RetailVrBridgeFailure::None
            && mAuthority.complete()
            && mCenterRuntime.ready()
            && (mCpuPublication || mPublisher.ready())
            && mController.ready()
            && mAccumulationHookLease
            && mAccumulationHookLease->installed();
    }

    RetailVrBridgeFailure failure() const noexcept
    {
        return mFailure;
    }

    engine::RetailFirstPersonHookInstallFailure
    firstPersonInstallFailure() const noexcept
    {
        return mFirstPersonInstallFailure;
    }

    std::size_t firstPersonInstallFailedCallSiteIndex() const noexcept
    {
        return mFirstPersonInstallFailedCallSiteIndex;
    }

    const engine::RetailRuntimeAuthorityDecision& authorityDecision() const noexcept
    {
        return mAuthority;
    }

    RetailVrBridgeFrameDiagnostics frameDiagnostics() const noexcept
    {
        return mFrameDiagnostics;
    }

    bool privateRenderDispatchActive() const noexcept
    {
        return mPrivateRenderDispatchGate.active();
    }

    bool firstPersonPublicationPendingForCurrentThread(
        std::uint32_t callerId = 0u) const noexcept
    {
        return ready()
            && !mPrivateRenderDispatchGate.active()
            && mPendingFirstPerson.valid
            && mPendingFirstPerson.publicationStaged
            && mPendingFirstPerson.threadId == GetCurrentThreadId()
            && callerId != 0u;
    }

    // Physical engine-center rendering already builds the live, calibrated
    // first-person branch into both eye accumulators. Publish that completed
    // pair synchronously at the authenticated RenderFirstPerson seam, before
    // the untouched desktop-only stock invocation can restore or draw through
    // one of its stale captured bindings. This performs no additional
    // private-eye RenderFirstPerson calls.
    bool publishPendingCenterIntegratedFirstPerson(
        std::uint32_t callerId = 0u) noexcept
    {
        mFrameDiagnostics.firstPerson = {};
        if (!ready()
            || mPrivateRenderDispatchGate.active()
            || !mPendingFirstPerson.valid
            || !mPendingFirstPerson.publicationStaged
            || mPendingFirstPerson.threadId != GetCurrentThreadId()
            || callerId == 0u)
        {
            mPendingFirstPerson = {};
            return false;
        }

        const PendingFirstPerson pending = mPendingFirstPerson;
        mPendingFirstPerson = {};
        mDeferredFirstPerson = {};
        auto& diagnostics = mFrameDiagnostics.firstPerson;
        diagnostics.stagedGeneration = pending.generation;
        diagnostics.stagedTransactionId = pending.transactionId;
        diagnostics.stagedPoseFrame = pending.tracked.pose.frame;
        diagnostics.stagedPoseSequence = pending.tracked.poseSequence;
        diagnostics.callerId = callerId;
        diagnostics.published = publishStagedFirstPersonPair(pending);
        if (diagnostics.published)
            ++diagnostics.completeCount;
        return diagnostics.published;
    }

    // Called only by the exact first-person E8 relay immediately before its
    // unmodified desktop invocation. The bridge reuses the live world frame
    // staged on this thread and renders the authentic weapon pass into each
    // already-populated eye target while the stock invocation's draw state is
    // still live. The relay then performs the untouched desktop call.
    bool dispatchPendingFirstPersonFromRelay(
        void* rendererInstance,
        std::uint32_t argument0,
        std::uint32_t argument1,
        std::uint32_t argument2,
        std::uint32_t argument3,
        std::uint32_t callerId = 0u) noexcept
    {
        mFrameDiagnostics.firstPerson = {};
        if (!ready()
            || !rendererInstance
            || !mCalls.calls.renderFirstPerson
            || !mCalls.calls.renderAccumulatorWithoutFinalize
            || mPrivateRenderDispatchGate.active()
            || !mPendingFirstPerson.valid
            || !mPendingFirstPerson.publicationStaged
            || mPendingFirstPerson.threadId != GetCurrentThreadId()
            || callerId == 0u)
        {
            mPendingFirstPerson = {};
            return false;
        }

        const PendingFirstPerson pending = mPendingFirstPerson;
        // Consume before calling into the engine. A nested or duplicate stock
        // call may still render normally, but it can never publish a stale
        // world/weapon combination.
        mPendingFirstPerson = {};
        auto& diagnostics = mFrameDiagnostics.firstPerson;
        diagnostics.stagedGeneration = pending.generation;
        diagnostics.stagedTransactionId = pending.transactionId;
        diagnostics.stagedPoseFrame = pending.tracked.pose.frame;
        diagnostics.stagedPoseSequence = pending.tracked.poseSequence;
        diagnostics.callerId = callerId;
        ++diagnostics.dispatchCount;
        if (!mPrivateRenderDispatchGate.tryEnter())
            return false;

        bool targetsRestored = false;
        bool complete = false;
        if (mOperations.eyeTargets.snapshot(mOperations.eyeTargets.context))
        {
            const bool left = renderFirstPersonEye(
                engine::CenterRendererEye::Left,
                pending.stockCenterCamera,
                pending.cameraRig.left,
                rendererInstance,
                argument0,
                argument1,
                argument2,
                argument3,
                diagnostics.cameraFailure,
                diagnostics.leftBefore,
                diagnostics.leftAfter,
                diagnostics.leftRestored,
                diagnostics.leftDepthPrepared,
                diagnostics.leftAccumulatorTransactionEntered,
                diagnostics.leftAccumulatorRestored);
            const bool interEyeStateRestored = left
                && (!mOperations.restoreFirstPersonEyeD3dState
                    || mOperations.restoreFirstPersonEyeD3dState(
                        mOperations.context));
            diagnostics.interEyeD3dStateRestored = interEyeStateRestored;
            const bool right = interEyeStateRestored && renderFirstPersonEye(
                engine::CenterRendererEye::Right,
                pending.stockCenterCamera,
                pending.cameraRig.right,
                rendererInstance,
                argument0,
                argument1,
                argument2,
                argument3,
                diagnostics.cameraFailure,
                diagnostics.rightBefore,
                diagnostics.rightAfter,
                diagnostics.rightRestored,
                diagnostics.rightDepthPrepared,
                diagnostics.rightAccumulatorTransactionEntered,
                diagnostics.rightAccumulatorRestored);
            diagnostics.leftRendered = left;
            diagnostics.rightRendered = right;
            targetsRestored = mOperations.eyeTargets.restore(
                mOperations.eyeTargets.context);
            complete = left && right && targetsRestored;
        }
        diagnostics.targetsRestored = targetsRestored;
        mPrivateRenderDispatchGate.leave();
        if (!complete)
            return false;

        // Publication is intentionally deferred until the untouched desktop
        // RenderFirstPerson invocation returns.  That invocation owns the
        // one ordinary C0D0 finalization which closes the stock one-shot
        // accumulator lifecycle; publishing here would expose a pair before
        // the desktop pass had consumed that exact finalizer.
        mDeferredFirstPerson = pending;
        diagnostics.privatePairReady = true;
        return true;
    }

    bool publishPendingFirstPersonAfterStockRender() noexcept
    {
        if (!ready()
            || mPrivateRenderDispatchGate.active()
            || !mDeferredFirstPerson.valid
            || !mDeferredFirstPerson.publicationStaged
            || mDeferredFirstPerson.threadId != GetCurrentThreadId())
        {
            mDeferredFirstPerson = {};
            return false;
        }

        const PendingFirstPerson pending = mDeferredFirstPerson;
        mDeferredFirstPerson = {};
        auto& diagnostics = mFrameDiagnostics.firstPerson;
        diagnostics.desktopFinalizerCompleted = true;
        diagnostics.published = publishStagedFirstPersonPair(pending);
        if (diagnostics.published)
            ++diagnostics.completeCount;
        return diagnostics.published;
    }

    void rejectPendingFirstPersonAfterStockRender() noexcept
    {
        mDeferredFirstPerson = {};
        mFrameDiagnostics.firstPerson.published = false;
        mFrameDiagnostics.firstPerson.privatePairReady = false;
    }

    void beginStockCullerCaptureFromAccumulationAdapter(
        engine::abi::RetailBSCullingProcessLayout* stockCullingProcess)
        noexcept
    {
        if (ready() && !mPrivateRenderDispatchGate.active())
        {
            // Accumulation can begin again before the stock first-person pass
            // for the most recently completed eye pair. Keep that pair alive
            // until the exact first-person relay consumes it; renderStereo
            // replaces it only after a newer complete pair exists. Clearing it
            // here would make every intervening rejected/partial accumulation
            // erase a valid world/weapon transaction.
            mPendingAccumulation = {};
            static_cast<void>(
                mCenterRuntime.beginStockCullerCapture(stockCullingProcess));
        }
    }

    bool finishStockCullerCaptureFromAccumulationAdapter(
        engine::abi::RetailBSCullingProcessLayout* stockCullingProcess)
        noexcept
    {
        return ready()
            && !mPrivateRenderDispatchGate.active()
            && mCenterRuntime.finishStockCullerCapture(stockCullingProcess);
    }

    bool stageFromAccumulationAdapter(
        engine::abi::RetailNiCameraLayout* stockCenterCamera,
        void* sceneObject,
        engine::abi::RetailBSCullingProcessLayout* stockCullingProcess)
        noexcept
    {
        return stageFromAccumulationAdapter(
            stockCenterCamera,
            sceneObject,
            stockCullingProcess,
            false,
            false,
            engine::RetailCenterRendererDiagnosticStop::None);
    }

    bool stageDiagnosticFromAccumulationAdapter(
        engine::abi::RetailNiCameraLayout* stockCenterCamera,
        void* sceneObject,
        engine::abi::RetailBSCullingProcessLayout* stockCullingProcess,
        bool suppressPrivateRender,
        bool suppressWorldPublication,
        engine::RetailCenterRendererDiagnosticStop rendererStop =
            engine::RetailCenterRendererDiagnosticStop::None) noexcept
    {
        return stageFromAccumulationAdapter(
            stockCenterCamera,
            sceneObject,
            stockCullingProcess,
            suppressPrivateRender,
            suppressWorldPublication,
            rendererStop);
    }

    bool dispatchPendingAfterStockRenderAdapter() noexcept
    {
        if (!ready()
            || mPrivateRenderDispatchGate.active()
            || !mPendingAccumulation.valid
            || mPendingAccumulation.threadId != GetCurrentThreadId())
        {
            mPendingAccumulation = {};
            return false;
        }

        const PendingAccumulation pending = mPendingAccumulation;
        // Clear before entering the controller. Private-eye rendering may
        // execute nested stock accumulation/render helpers; those relays must
        // never observe the outer transaction as pending.
        mPendingAccumulation = {};
        if (!mCenterRuntime.setRendererDiagnosticStop(pending.rendererStop))
            return false;
        mSuppressPrivateRenderForDiagnostic = pending.suppressPrivateRender;
        mSuppressWorldPublicationForDiagnostic =
            pending.suppressWorldPublication;
        mFrameDiagnostics.renderer = {};
        mFrameDiagnostics.eyeCamera = {};
        mFrameDiagnostics.visibility = {};
        mFrameDiagnostics.accumulatorSnapshot = {};
        mFrameDiagnostics.rendererTiming = {};
        mFrameDiagnostics.rendererCullerCamera = {};
        const engine::RetailWorldAccumulationControllerResult result =
            mController.dispatch(pending.frame);
        mFrameDiagnostics.controller = result;
        ++mFrameDiagnostics.dispatchCount;
        if (result.complete()
            && result.disposition
                == engine::RetailWorldHookDisposition::StereoWorldComplete)
        {
            ++mFrameDiagnostics.stereoCompleteCount;
        }
        mSuppressWorldPublicationForDiagnostic = false;
        mSuppressPrivateRenderForDiagnostic = false;
        static_cast<void>(mCenterRuntime.setRendererDiagnosticStop(
            engine::RetailCenterRendererDiagnosticStop::None));
        return true;
    }

    bool publishMonoUiQuadFromPresent(
        const engine::RetailTrackedFrame& tracked) noexcept
    {
        if (!ready()
            || !engine::validateRetailTrackedUiFrame(tracked).complete())
        {
            return false;
        }
        std::uint64_t transactionId = 0u;
        if (!mPublicationSequence.claim(
                gpu::color_v5::PresentationMode::MonoUiQuad,
                transactionId))
        {
            return false;
        }
        if (mCpuPublication)
        {
            return mOperations.publishCpuMonoUiQuad
                && mOperations.publishCpuMonoUiQuad(
                    mOperations.context,
                    tracked,
                    transactionId);
        }
        color_transport::ProducerFrameIdentity identity {};
        identity.presentationMode =
            gpu::color_v5::PresentationMode::MonoUiQuad;
        identity.producerEpoch = tracked.pose.producerEpoch;
        identity.producerProcessId = GetCurrentProcessId();
        identity.transactionId = transactionId;
        identity.sourceFrame = tracked.pose.frame;
        identity.poseSequence =
            shared::sequencedValueBits(tracked.poseSequence);
        identity.runtimeStateSample = tracked.runtime.frame;
        identity.renderedDisplayTime = tracked.pose.predictedDisplayTime;
        return produceAndPublish(identity);
    }

    // Simulator recording may use the fully completed stock gameplay frame
    // as a source. The caller copies the retail backbuffer only after the
    // stock renderer (including RenderFirstPerson) has finished, then asks
    // this bridge to publish that read-only copy. This does not enter, alter,
    // or replay any retail rendering pass.
    bool publishFinalStockFramePairFromPresent(
        const engine::RetailTrackedFrame& tracked) noexcept
    {
        // A completed desktop backbuffer is not evidence that the private
        // engine transaction rendered a weapon-bearing pair.  Keep this API
        // as an explicit hard stop so an old caller cannot silently revive
        // the inadmissible StretchRect route.
        (void)tracked;
        return false;
    }

private:
    struct PendingFirstPerson
    {
        engine::abi::RetailNiCameraLayout* stockCenterCamera = nullptr;
        engine::RetailDerivedEyeCameraRig cameraRig {};
        engine::RetailTrackedFrame tracked {};
        std::uint64_t generation = 0u;
        std::uint64_t transactionId = 0u;
        DWORD threadId = 0u;
        bool publicationStaged = false;
        bool valid = false;
    };

    struct PendingAccumulation
    {
        engine::RetailWorldAccumulationCallFrame frame {};
        engine::RetailCenterRendererDiagnosticStop rendererStop =
            engine::RetailCenterRendererDiagnosticStop::None;
        DWORD threadId = 0u;
        bool suppressPrivateRender = false;
        bool suppressWorldPublication = false;
        bool valid = false;
    };

    using FirstPersonEngineState =
        RetailVrBridgeFrameDiagnostics::FirstPerson::EngineState;

    // RenderFirstPerson owns an accumulator/finalize pass. Its first call
    // replaces the live, distinct rendering lane with the renderer-owned
    // accumulator, which leaves a later eye with no first-person geometry to
    // consume. Preserve that narrow owner state per eye, including one
    // temporary lane reference, so both authentic stock calls begin from the
    // same engine state and the ordinary desktop invocation sees it restored.
    struct FirstPersonAccumulatorSnapshot
    {
        engine::abi::RetailRendererAccumulatorOwnerLayout* renderer = nullptr;
        engine::abi::RetailBSShaderAccumulatorLayout* rendererAccumulator =
            nullptr;
        engine::abi::RetailBSShaderAccumulatorLayout* accumulatingAccumulator =
            nullptr;
        engine::abi::RetailBSShaderAccumulatorLayout* renderingAccumulator =
            nullptr;
        engine::abi::RetailPointer32 rendererAccumulatorAddress = 0u;
        engine::abi::RetailPointer32 accumulatingAccumulatorAddress = 0u;
        engine::abi::RetailPointer32 renderingAccumulatorAddress = 0u;
        std::uint32_t rendererAccumulatorReferenceCount = 0u;
        std::uint32_t renderingAccumulatorReferenceCount = 0u;
        engine::abi::RetailBSShaderAccumulatorLayout* eyeAccumulator =
            nullptr;
        engine::abi::RetailPointer32 eyeAccumulatorAddress = 0u;
        std::uint32_t eyeAccumulatorReferenceCount = 0u;
        bool eyeAccumulatorBound = false;
        bool renderingAccumulatorRetained = false;
        bool active = false;
    };

    // The accumulator-pointer transaction restores the ownership lanes that
    // the first stock call consumes.  Keep this read-only snapshot alongside
    // it so a failure of the second eye can be attributed to a concrete
    // mutated accumulator payload instead of guessed from the visible image.
    struct FirstPersonAccumulatorContentsSnapshot
    {
        const engine::abi::RetailBSShaderAccumulatorLayout*
            rendererAccumulator = nullptr;
        const engine::abi::RetailBSShaderAccumulatorLayout*
            renderingAccumulator = nullptr;
        std::uint8_t rendererBytes[
            sizeof(engine::abi::RetailBSShaderAccumulatorLayout)] {};
        std::uint8_t renderingBytes[
            sizeof(engine::abi::RetailBSShaderAccumulatorLayout)] {};
        bool captured = false;
    };

    // RenderFirstPerson's ECX object owns the live first-person camera/culler
    // handles at offsets through +0xA0. Its fixed 0xB0-byte prefix is proven
    // readable by the stock function itself, so a read-only comparison can
    // identify a once-per-frame flag without speculating about its layout.
    struct FirstPersonRendererInstanceContentsSnapshot
    {
        const std::uint8_t* instance = nullptr;
        std::uint8_t bytes[0xB0u] {};
        bool captured = false;
    };

    struct FirstPersonCullingProcessContentsSnapshot
    {
        const engine::abi::RetailBSCullingProcessLayout* cullingProcess =
            nullptr;
        std::uint8_t bytes[
            sizeof(engine::abi::RetailBSCullingProcessLayout)] {};
        bool captured = false;
    };

    static std::uint32_t firstPersonContentHash(
        const std::uint8_t* bytes,
        std::size_t byteCount) noexcept
    {
        if (!bytes || byteCount == 0u)
            return 0u;
        std::uint32_t hash = 2166136261u;
        for (std::size_t index = 0u; index < byteCount; ++index)
        {
            hash ^= bytes[index];
            hash *= 16777619u;
        }
        return hash;
    }

    static void describeFirstPersonContentDelta(
        const std::uint8_t* before,
        const std::uint8_t* after,
        std::size_t byteCount,
        FirstPersonEngineState::ContentDelta& result) noexcept
    {
        result = {};
        result.firstChangedOffset =
            (std::numeric_limits<std::uint16_t>::max)();
        if (!before || !after || byteCount == 0u)
            return;

        result.beforeHash = firstPersonContentHash(before, byteCount);
        result.afterHash = firstPersonContentHash(after, byteCount);
        for (std::size_t index = 0u; index < byteCount; ++index)
        {
            if (before[index] == after[index])
                continue;
            ++result.changedByteCount;
            if (result.firstChangedOffset
                != (std::numeric_limits<std::uint16_t>::max)())
            {
                continue;
            }
            result.firstChangedOffset = static_cast<std::uint16_t>(index);
            const std::size_t wordByteCount =
                (std::min)(sizeof(result.firstWordBefore), byteCount - index);
            std::memcpy(
                &result.firstWordBefore,
                before + index,
                wordByteCount);
            std::memcpy(
                &result.firstWordAfter,
                after + index,
                wordByteCount);
        }
        result.captured = true;
    }

    static bool captureFirstPersonAccumulatorContents(
        const FirstPersonAccumulatorSnapshot& accumulatorSnapshot,
        FirstPersonAccumulatorContentsSnapshot& contents) noexcept
    {
        contents = {};
        if (!accumulatorSnapshot.active
            || !accumulatorSnapshot.rendererAccumulator
            || !accumulatorSnapshot.renderingAccumulator)
        {
            return false;
        }
        contents.rendererAccumulator = accumulatorSnapshot.rendererAccumulator;
        contents.renderingAccumulator = accumulatorSnapshot.renderingAccumulator;
        std::memcpy(
            contents.rendererBytes,
            contents.rendererAccumulator,
            sizeof(contents.rendererBytes));
        std::memcpy(
            contents.renderingBytes,
            contents.renderingAccumulator,
            sizeof(contents.renderingBytes));
        contents.captured = true;
        return true;
    }

    static void observeFirstPersonAccumulatorContents(
        const FirstPersonAccumulatorContentsSnapshot& contents,
        FirstPersonEngineState& result) noexcept
    {
        if (!contents.captured
            || !contents.rendererAccumulator
            || !contents.renderingAccumulator)
        {
            return;
        }
        describeFirstPersonContentDelta(
            contents.rendererBytes,
            reinterpret_cast<const std::uint8_t*>(
                contents.rendererAccumulator),
            sizeof(contents.rendererBytes),
            result.rendererAccumulatorContents);
        describeFirstPersonContentDelta(
            contents.renderingBytes,
            reinterpret_cast<const std::uint8_t*>(
                contents.renderingAccumulator),
            sizeof(contents.renderingBytes),
            result.renderingAccumulatorContents);
    }

    static bool captureFirstPersonRendererInstanceContents(
        const void* rendererInstance,
        FirstPersonRendererInstanceContentsSnapshot& contents) noexcept
    {
        contents = {};
        if (!rendererInstance)
            return false;
        contents.instance = static_cast<const std::uint8_t*>(rendererInstance);
        std::memcpy(contents.bytes, contents.instance, sizeof(contents.bytes));
        contents.captured = true;
        return true;
    }

    static void observeFirstPersonRendererInstanceContents(
        const FirstPersonRendererInstanceContentsSnapshot& contents,
        FirstPersonEngineState& result) noexcept
    {
        if (!contents.captured || !contents.instance)
            return;
        describeFirstPersonContentDelta(
            contents.bytes,
            contents.instance,
            sizeof(contents.bytes),
            result.rendererInstanceContents);
    }

    const engine::abi::RetailBSCullingProcessLayout*
    firstPersonCullingProcess() const noexcept
    {
        const std::uintptr_t runtimeImageBase =
            mAuthority.authority.metadata().runtimeImageBase;
        constexpr std::uintptr_t singletonRva =
            engine::abi::SceneGraphSingletonPointerAddress
            - engine::SupportedImageBase;
        if (runtimeImageBase == 0u
            || runtimeImageBase
                > (std::numeric_limits<std::uintptr_t>::max)()
                    - singletonRva)
        {
            return nullptr;
        }
        const auto* sceneGraphSingleton = reinterpret_cast<
            const engine::abi::RetailPointer32*>(
                runtimeImageBase + singletonRva);
        __try
        {
            if (!sceneGraphSingleton || *sceneGraphSingleton == 0u)
                return nullptr;
            const auto* sceneGraph = reinterpret_cast<
                const engine::abi::RetailSceneGraphLayout*>(
                    static_cast<std::uintptr_t>(*sceneGraphSingleton));
            if (!sceneGraph || sceneGraph->cullingProcess == 0u)
                return nullptr;
            return reinterpret_cast<
                const engine::abi::RetailBSCullingProcessLayout*>(
                    static_cast<std::uintptr_t>(sceneGraph->cullingProcess));
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return nullptr;
        }
    }

    // RenderFirstPerson owns two retained pointers that the loaded function
    // dereferences through the trivial NiPointer getter at 0x00559450:
    //   ECX + 0x8C -> prepared first-person BSShaderAccumulator
    //   ECX + 0xA0 -> first-person NiCamera
    // The persistent accumulator is prepared by the full stock function for
    // each private eye.  Its exact in-body B6C0D0 finalizer call is deferred
    // by a separate sealed relay until the following desktop invocation.
    // These offsets are revalidated from the loaded RenderFirstPerson body;
    // this helper only reads the two NiPointer payloads and never writes them.
    bool firstPersonPreparedPass(
        void* rendererInstance,
        engine::abi::RetailNiCameraLayout*& camera,
        engine::abi::RetailBSShaderAccumulatorLayout*& accumulator) const
        noexcept
    {
        camera = nullptr;
        accumulator = nullptr;
        if (!rendererInstance)
            return false;

        constexpr std::size_t accumulatorOffset = 0x8Cu;
        constexpr std::size_t cameraOffset = 0xA0u;
        const auto* const bytes = static_cast<const std::uint8_t*>(
            rendererInstance);
        __try
        {
            engine::abi::RetailPointer32 accumulatorAddress = 0u;
            engine::abi::RetailPointer32 cameraAddress = 0u;
            std::memcpy(
                &accumulatorAddress,
                bytes + accumulatorOffset,
                sizeof(accumulatorAddress));
            std::memcpy(
                &cameraAddress,
                bytes + cameraOffset,
                sizeof(cameraAddress));
            if (accumulatorAddress == 0u || cameraAddress == 0u)
                return false;

            auto* const candidateAccumulator = reinterpret_cast<
                engine::abi::RetailBSShaderAccumulatorLayout*>(
                static_cast<std::uintptr_t>(accumulatorAddress));
            auto* const candidateCamera = reinterpret_cast<
                engine::abi::RetailNiCameraLayout*>(
                static_cast<std::uintptr_t>(cameraAddress));
            const auto* const cameraHeader = reinterpret_cast<
                const engine::abi::RetailNiAccumulatorLayout*>(candidateCamera);
            if (!candidateAccumulator
                || !candidateCamera
                || candidateAccumulator->vtable == 0u
                || candidateAccumulator->referenceCount == 0u
                || cameraHeader->vtable == 0u
                || cameraHeader->referenceCount == 0u)
            {
                return false;
            }
            camera = candidateCamera;
            accumulator = candidateAccumulator;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            camera = nullptr;
            accumulator = nullptr;
            return false;
        }
    }

    bool captureFirstPersonCullingProcessContents(
        FirstPersonCullingProcessContentsSnapshot& contents) const noexcept
    {
        contents = {};
        const auto* cullingProcess = firstPersonCullingProcess();
        if (!cullingProcess)
            return false;
        __try
        {
            contents.cullingProcess = cullingProcess;
            std::memcpy(
                contents.bytes,
                contents.cullingProcess,
                sizeof(contents.bytes));
            contents.captured = true;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            contents = {};
            return false;
        }
    }

    static void observeFirstPersonCullingProcessContents(
        const FirstPersonCullingProcessContentsSnapshot& contents,
        FirstPersonEngineState& result) noexcept
    {
        if (!contents.captured || !contents.cullingProcess)
            return;
        __try
        {
            describeFirstPersonContentDelta(
                contents.bytes,
                reinterpret_cast<const std::uint8_t*>(contents.cullingProcess),
                sizeof(contents.bytes),
                result.cullingProcessContents);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            result.cullingProcessContents = {};
        }
    }

    bool beginFirstPersonAccumulatorTransaction(
        engine::CenterRendererEye eye,
        FirstPersonAccumulatorSnapshot& snapshot) noexcept
    {
        snapshot = {};
        const engine::RetailEngineCalls& calls = mCalls.calls;
        if (!calls.privateStereoRegistrationComplete())
            return false;

        const engine::abi::RetailPointer32 rendererAddress =
            *calls.rendererSingleton;
        if (rendererAddress == 0u)
            return false;
        auto* renderer = reinterpret_cast<
            engine::abi::RetailRendererAccumulatorOwnerLayout*>(
            static_cast<std::uintptr_t>(rendererAddress));
        const engine::abi::RetailPointer32 rendererAccumulatorAddress =
            renderer->accumulator;
        const engine::abi::RetailPointer32 accumulatingAccumulatorAddress =
            *calls.accumulatingAccumulator;
        const engine::abi::RetailPointer32 renderingAccumulatorAddress =
            *calls.renderingAccumulator;
        if (rendererAccumulatorAddress == 0u
            || accumulatingAccumulatorAddress != rendererAccumulatorAddress
            || renderingAccumulatorAddress == 0u
            || renderingAccumulatorAddress == rendererAccumulatorAddress)
        {
            return false;
        }

        auto* rendererAccumulator = reinterpret_cast<
            engine::abi::RetailBSShaderAccumulatorLayout*>(
            static_cast<std::uintptr_t>(rendererAccumulatorAddress));
        auto* accumulatingAccumulator = reinterpret_cast<
            engine::abi::RetailBSShaderAccumulatorLayout*>(
            static_cast<std::uintptr_t>(accumulatingAccumulatorAddress));
        auto* renderingAccumulator = reinterpret_cast<
            engine::abi::RetailBSShaderAccumulatorLayout*>(
            static_cast<std::uintptr_t>(renderingAccumulatorAddress));
        if (!rendererAccumulator
            || !accumulatingAccumulator
            || !renderingAccumulator
            || rendererAccumulator->referenceCount < 3u
            || renderingAccumulator->referenceCount < 2u
            || renderingAccumulator->referenceCount
                == (std::numeric_limits<std::uint32_t>::max)())
        {
            return false;
        }

        // RenderFirstPerson owns and consumes the accumulator it finds in
        // the renderer/accumulating lanes.  Restoring only those global
        // pointers after the left invocation leaves the consumed pass lists
        // behind, which makes the right invocation a valid call with no
        // weapon pixels.  The center runtime already owns two independently
        // constructed, lifetime-managed accumulators for the two real eyes.
        // Bind the matching one for this bounded first-person transaction;
        // never clone or memcpy the retail accumulator's opaque pass lists.
        const auto& resources = mCenterRuntime.resources();
        auto* const eyeAccumulator = eye == engine::CenterRendererEye::Left
            ? resources.leftAccumulator()
            : resources.rightAccumulator();
        const std::uintptr_t eyeAccumulatorAddress =
            reinterpret_cast<std::uintptr_t>(eyeAccumulator);
        if (!eyeAccumulator
            || eyeAccumulatorAddress == 0u
            || eyeAccumulatorAddress
                > (std::numeric_limits<engine::abi::RetailPointer32>::max)()
            || eyeAccumulator == rendererAccumulator
            || eyeAccumulator == renderingAccumulator
            || eyeAccumulator->vtable == 0u
            || eyeAccumulator->referenceCount == 0u)
        {
            return false;
        }

        // Snapshot every ownership count before either official setter can
        // retain/release an accumulator.  The transaction's restore check is
        // only meaningful against this pre-bind state.
        const std::uint32_t rendererAccumulatorReferenceCount =
            rendererAccumulator->referenceCount;
        const std::uint32_t renderingAccumulatorReferenceCount =
            renderingAccumulator->referenceCount;
        const std::uint32_t eyeAccumulatorReferenceCount =
            eyeAccumulator->referenceCount;

        // The first-person function's render/finalize wrapper selects the
        // rendering lane, not merely the renderer/accumulating owners.  All
        // three lanes therefore have to name this eye's constructed
        // accumulator.  Hold the stock rendering lane while it is unbound so
        // restoration never follows a released raw pointer.
        ++renderingAccumulator->referenceCount;

        // The owner setters retain/release their lane references.  Do not
        // write raw pointers: doing so would bypass retail ownership and make
        // the next stock first-person pass unsafe.  A complete stock
        // RenderFirstPerson invocation must see one eye-owned accumulator in
        // every active lane; leaving the stock rendering lane installed makes
        // the first call consume it and turns the second eye into a no-op.
        calls.setRenderingAccumulator(eyeAccumulator);
        calls.rendererSetAccumulator(renderer, eyeAccumulator);
        calls.setAccumulatingAccumulator(eyeAccumulator);
        const engine::abi::RetailPointer32 eyeAccumulatorPointer =
            static_cast<engine::abi::RetailPointer32>(eyeAccumulatorAddress);
        const bool eyeBound = renderer->accumulator == eyeAccumulatorPointer
            && *calls.accumulatingAccumulator == eyeAccumulatorPointer
            && *calls.renderingAccumulator == eyeAccumulatorPointer;
        if (!eyeBound)
        {
            if (*calls.renderingAccumulator != renderingAccumulatorAddress)
                calls.setRenderingAccumulator(renderingAccumulator);
            if (*calls.accumulatingAccumulator
                != accumulatingAccumulatorAddress)
            {
                calls.setAccumulatingAccumulator(accumulatingAccumulator);
            }
            if (renderer->accumulator != rendererAccumulatorAddress)
                calls.rendererSetAccumulator(renderer, rendererAccumulator);
            if (renderingAccumulator->referenceCount > 0u)
                --renderingAccumulator->referenceCount;
            return false;
        }
        snapshot = {
            renderer,
            rendererAccumulator,
            accumulatingAccumulator,
            renderingAccumulator,
            rendererAccumulatorAddress,
            accumulatingAccumulatorAddress,
            renderingAccumulatorAddress,
            rendererAccumulatorReferenceCount,
            renderingAccumulatorReferenceCount,
            eyeAccumulator,
            eyeAccumulatorPointer,
            eyeAccumulatorReferenceCount,
            true,
            true,
            true,
        };
        return true;
    }

    bool restoreFirstPersonAccumulatorTransaction(
        FirstPersonAccumulatorSnapshot& snapshot) noexcept
    {
        if (!snapshot.active)
            return false;

        const engine::RetailEngineCalls& calls = mCalls.calls;
        if (*calls.renderingAccumulator
            != snapshot.renderingAccumulatorAddress)
        {
            calls.setRenderingAccumulator(snapshot.renderingAccumulator);
        }
        if (*calls.accumulatingAccumulator
            != snapshot.accumulatingAccumulatorAddress)
        {
            calls.setAccumulatingAccumulator(snapshot.accumulatingAccumulator);
        }
        if (snapshot.renderer->accumulator
            != snapshot.rendererAccumulatorAddress)
        {
            calls.rendererSetAccumulator(
                snapshot.renderer,
                snapshot.rendererAccumulator);
        }

        const bool ownersRestored = snapshot.renderer->accumulator
                == snapshot.rendererAccumulatorAddress
            && *calls.accumulatingAccumulator
                == snapshot.accumulatingAccumulatorAddress
            && *calls.renderingAccumulator
                == snapshot.renderingAccumulatorAddress;
        bool temporaryReferenceReleased = !snapshot.renderingAccumulatorRetained;
        if (snapshot.renderingAccumulatorRetained
            && snapshot.renderingAccumulator
            && snapshot.renderingAccumulator->referenceCount > 0u)
        {
            --snapshot.renderingAccumulator->referenceCount;
            temporaryReferenceReleased = true;
        }
        const bool referenceCountsRestored = temporaryReferenceReleased
            && snapshot.rendererAccumulator->referenceCount
                == snapshot.rendererAccumulatorReferenceCount
            && snapshot.renderingAccumulator->referenceCount
                == snapshot.renderingAccumulatorReferenceCount;
        const bool privateEyeLaneReleased = !snapshot.eyeAccumulatorBound
            || (snapshot.eyeAccumulator
                && snapshot.eyeAccumulator->referenceCount
                    == snapshot.eyeAccumulatorReferenceCount);
        snapshot = {};
        return ownersRestored && referenceCountsRestored
            && privateEyeLaneReleased;
    }

    FirstPersonEngineState observeFirstPersonEngineState() const noexcept
    {
        FirstPersonEngineState observation {};
        const engine::RetailEngineCalls& calls = mCalls.calls;
        if (!calls.privateStereoRegistrationComplete())
            return observation;

        observation.rendererAddress = *calls.rendererSingleton;
        observation.accumulatingAccumulator = *calls.accumulatingAccumulator;
        observation.renderingAccumulator = *calls.renderingAccumulator;
        if (observation.rendererAddress != 0u)
        {
            const auto* renderer = reinterpret_cast<
                const engine::abi::RetailRendererAccumulatorOwnerLayout*>(
                static_cast<std::uintptr_t>(observation.rendererAddress));
            observation.rendererAccumulator = renderer->accumulator;
        }
        const auto referenceCountFor = [](std::uint32_t address) noexcept {
            if (address == 0u)
                return 0u;
            const auto* accumulator = reinterpret_cast<
                const engine::abi::RetailBSShaderAccumulatorLayout*>(
                static_cast<std::uintptr_t>(address));
            return accumulator->referenceCount;
        };
        observation.rendererAccumulatorReferenceCount = referenceCountFor(
            observation.rendererAccumulator);
        observation.renderingAccumulatorReferenceCount = referenceCountFor(
            observation.renderingAccumulator);
        if (mOperations.readFirstPersonCompatibilityGuard)
        {
            observation.compatibilityGuard =
                mOperations.readFirstPersonCompatibilityGuard(
                    mOperations.context);
        }
        observation.captured = true;
        return observation;
    }

    bool renderFirstPersonEye(
        engine::CenterRendererEye eye,
        engine::abi::RetailNiCameraLayout* stockCenterCamera,
        const engine::RetailCameraMutableState& eyeCamera,
        void* rendererInstance,
        std::uint32_t argument0,
        std::uint32_t argument1,
        std::uint32_t argument2,
        std::uint32_t argument3,
        engine::RetailEyeCameraFailure& cameraFailure,
        FirstPersonEngineState& before,
        FirstPersonEngineState& after,
        FirstPersonEngineState& restored,
        bool& depthPrepared,
        bool& accumulatorTransactionEntered,
        bool& accumulatorTransactionRestored) noexcept
    {
        accumulatorTransactionEntered = false;
        accumulatorTransactionRestored = false;
        const bool targetRedirectActive =
            mOperations.beginFirstPersonEyeTargetRedirect != nullptr;
        if (targetRedirectActive
            && !mOperations.beginFirstPersonEyeTargetRedirect(
                mOperations.context,
                eye))
        {
            return false;
        }
        const auto endTargetRedirect = [this, targetRedirectActive]() noexcept {
            if (targetRedirectActive)
            {
                mOperations.endFirstPersonEyeTargetRedirect(
                    mOperations.context);
            }
        };
        engine::CenterRendererEyeIsolation isolation {};
        if (!mOperations.eyeTargets.bindPreservingContents(
                mOperations.eyeTargets.context,
                eye,
                isolation))
        {
            endTargetRedirect();
            return false;
        }
        if (mOperations.prepareFirstPersonEyeDepth
            && !mOperations.prepareFirstPersonEyeDepth(
                mOperations.context,
                eye))
        {
            mOperations.eyeTargets.rollback(
                mOperations.eyeTargets.context,
                eye,
                isolation);
            endTargetRedirect();
            return false;
        }
        depthPrepared = mOperations.prepareFirstPersonEyeDepth != nullptr;

        engine::abi::RetailNiCameraLayout* firstPersonCamera = nullptr;
        engine::abi::RetailBSShaderAccumulatorLayout* firstPersonAccumulator =
            nullptr;
        if (!firstPersonPreparedPass(
                rendererInstance,
                firstPersonCamera,
                firstPersonAccumulator))
        {
            mOperations.eyeTargets.rollback(
                mOperations.eyeTargets.context,
                eye,
                isolation);
            endTargetRedirect();
            return false;
        }
        // firstPersonPreparedPass verifies that this instance contains the
        // authenticated persistent list consumed by the exact internal
        // B6C0D0 site.  RenderFirstPerson itself must build that list; a
        // direct pre-call to its lower-level wrapper is too early and can
        // produce a syntactically successful eye transaction with no weapon
        // draw calls.
        (void)firstPersonAccumulator;

        // The Third caller arrives with the complete stock first-person
        // accumulator already installed in the renderer.  Do not replace its
        // ownership lanes with the world-eye accumulators: live evidence
        // shows those globals are no longer in the world-phase relationship
        // required by beginFirstPersonAccumulatorTransaction.  The leased
        // inner seam makes this scope safe: each private invocation renders
        // the prepared list through B6BA20 without finalizing it, and the
        // following untouched desktop invocation performs the one B6C0D0
        // finalization.  Since this scope does not mutate accumulator owners,
        // entry and restoration are the same proven stock state.
        before = observeFirstPersonEngineState();
        if (!before.captured)
        {
            mOperations.eyeTargets.rollback(
                mOperations.eyeTargets.context,
                eye,
                isolation);
            endTargetRedirect();
            return false;
        }
        accumulatorTransactionEntered = true;
        mCalls.calls.renderFirstPerson(
            rendererInstance,
            argument0,
            argument1,
            argument2,
            argument3);
        after = observeFirstPersonEngineState();
        // B6BA20 selects the renderer accumulator as its active rendering
        // lane even though it deliberately does not finalize. Restore each
        // owner through the official retain/release setters before the next
        // eye so both calls begin from the exact stock Third-caller state.
        if (after.captured)
        {
            if (after.renderingAccumulator != before.renderingAccumulator)
            {
                mCalls.calls.setRenderingAccumulator(reinterpret_cast<
                    engine::abi::RetailBSShaderAccumulatorLayout*>(
                    static_cast<std::uintptr_t>(before.renderingAccumulator)));
            }
            if (after.accumulatingAccumulator
                != before.accumulatingAccumulator)
            {
                mCalls.calls.setAccumulatingAccumulator(reinterpret_cast<
                    engine::abi::RetailBSShaderAccumulatorLayout*>(
                    static_cast<std::uintptr_t>(before.accumulatingAccumulator)));
            }
            if (after.rendererAccumulator != before.rendererAccumulator
                && before.rendererAddress != 0u)
            {
                mCalls.calls.rendererSetAccumulator(
                    reinterpret_cast<
                        engine::abi::RetailRendererAccumulatorOwnerLayout*>(
                        static_cast<std::uintptr_t>(before.rendererAddress)),
                    reinterpret_cast<
                        engine::abi::RetailBSShaderAccumulatorLayout*>(
                        static_cast<std::uintptr_t>(before.rendererAccumulator)));
            }
        }
        restored = observeFirstPersonEngineState();
        accumulatorTransactionRestored = restored.captured
            && restored.rendererAccumulator == before.rendererAccumulator
            && restored.accumulatingAccumulator == before.accumulatingAccumulator
            && restored.renderingAccumulator == before.renderingAccumulator;
        if (!accumulatorTransactionRestored
            || !mOperations.eyeTargets.end(
                mOperations.eyeTargets.context,
                eye,
                isolation))
        {
            mOperations.eyeTargets.rollback(
                mOperations.eyeTargets.context,
                eye,
                isolation);
            endTargetRedirect();
            return false;
        }
        endTargetRedirect();
        cameraFailure = engine::RetailEyeCameraFailure::None;
        return true;

#if 0

        FirstPersonAccumulatorSnapshot accumulatorTransaction {};
        if (!beginFirstPersonAccumulatorTransaction(
                eye,
                accumulatorTransaction))
        {
            mOperations.eyeTargets.rollback(
                mOperations.eyeTargets.context,
                eye,
                isolation);
            endTargetRedirect();
            return false;
        }
        accumulatorTransactionEntered = true;
        if (firstPersonAccumulator
            != accumulatorTransaction.rendererAccumulator)
        {
            static_cast<void>(restoreFirstPersonAccumulatorTransaction(
                accumulatorTransaction));
            mOperations.eyeTargets.rollback(
                mOperations.eyeTargets.context,
                eye,
                isolation);
            endTargetRedirect();
            return false;
        }

        // The owner setter must update the exact +0x8C accumulator consumed
        // by RenderFirstPerson.  If it does not, the global lanes and the
        // function-local persistent list are different transactions; refuse
        // the pass instead of rendering an unproven list.
        engine::abi::RetailNiCameraLayout* boundFirstPersonCamera = nullptr;
        engine::abi::RetailBSShaderAccumulatorLayout*
            boundFirstPersonAccumulator = nullptr;
        if (!firstPersonPreparedPass(
                rendererInstance,
                boundFirstPersonCamera,
                boundFirstPersonAccumulator)
            || boundFirstPersonAccumulator
                != accumulatorTransaction.eyeAccumulator)
        {
            static_cast<void>(restoreFirstPersonAccumulatorTransaction(
                accumulatorTransaction));
            mOperations.eyeTargets.rollback(
                mOperations.eyeTargets.context,
                eye,
                isolation);
            endTargetRedirect();
            return false;
        }
        firstPersonCamera = boundFirstPersonCamera;

        // RenderFirstPerson initializes its live view-model camera inside the
        // stock call, immediately before the authenticated prepared-render
        // seam.  Writing either the earlier world camera or the not-yet-built
        // view-model camera here is premature.  The world eye is already in
        // the bound target and xNVSE owns the controller-driven weapon node;
        // let the complete stock call prepare its camera before the inner
        // non-finalizing relay consumes the list.
        (void)stockCenterCamera;
        (void)eyeCamera;
        engine::RetailScopedStockEyeCameraTransaction cameraTransaction;
        engine::RetailScopedStockEyeCameraTransaction
            firstPersonCameraTransaction;

        before = observeFirstPersonEngineState();
        FirstPersonAccumulatorContentsSnapshot accumulatorContents {};
        FirstPersonCullingProcessContentsSnapshot cullingContents {};
        if (!before.captured
            || !captureFirstPersonAccumulatorContents(
                accumulatorTransaction,
                accumulatorContents)
            || !captureFirstPersonCullingProcessContents(cullingContents))
        {
            firstPersonCameraTransaction.restore();
            cameraTransaction.restore();
            static_cast<void>(restoreFirstPersonAccumulatorTransaction(
                accumulatorTransaction));
            mOperations.eyeTargets.rollback(
                mOperations.eyeTargets.context,
                eye,
                isolation);
            endTargetRedirect();
            return false;
        }
        FirstPersonRendererInstanceContentsSnapshot rendererInstanceContents {};
        if (!captureFirstPersonRendererInstanceContents(
                rendererInstance,
                rendererInstanceContents))
        {
            firstPersonCameraTransaction.restore();
            cameraTransaction.restore();
            static_cast<void>(restoreFirstPersonAccumulatorTransaction(
                accumulatorTransaction));
            mOperations.eyeTargets.rollback(
                mOperations.eyeTargets.context,
                eye,
                isolation);
            endTargetRedirect();
            return false;
        }

        // Run the complete verified stock function so each eye builds the
        // real view-model list and emits the weapon draw calls.  Its one
        // persistent B6C0D0 accumulator call is independently leased: while
        // the private dispatch gate is active, that exact relay invokes the
        // stock B6BA20 render half only.  The ordinary desktop invocation
        // that follows this pair runs unchanged and performs the one normal
        // B6C0D0 finalization.  No desktop pixels or bitmap contents are ever
        // copied into either eye.
        mCalls.calls.renderFirstPerson(
            rendererInstance,
            argument0,
            argument1,
            argument2,
            argument3);
        after = observeFirstPersonEngineState();
        observeFirstPersonRendererInstanceContents(
            rendererInstanceContents,
            after);
        observeFirstPersonAccumulatorContents(accumulatorContents, after);
        observeFirstPersonCullingProcessContents(cullingContents, after);
        const bool accumulatorRestored =
            restoreFirstPersonAccumulatorTransaction(
                accumulatorTransaction);
        accumulatorTransactionRestored = accumulatorRestored;
        // Observe only after the official owner setters and temporary
        // reference release.  The old implementation captured this state
        // before restore and therefore could not prove that the stock desktop
        // invocation received its original lanes.
        restored = observeFirstPersonEngineState();
        firstPersonCameraTransaction.restore();
        cameraTransaction.restore();
        if (!accumulatorRestored)
        {
            mOperations.eyeTargets.rollback(
                mOperations.eyeTargets.context,
                eye,
                isolation);
            endTargetRedirect();
            return false;
        }
        if (!mOperations.eyeTargets.end(
                mOperations.eyeTargets.context,
                eye,
                isolation))
        {
            mOperations.eyeTargets.rollback(
                mOperations.eyeTargets.context,
                eye,
                isolation);
            endTargetRedirect();
            return false;
        }
        endTargetRedirect();
        cameraFailure = engine::RetailEyeCameraFailure::None;
        return true;
#endif
    }

    bool publishStagedFirstPersonPair(
        const PendingFirstPerson& pending) noexcept
    {
        if (!pending.valid
            || !pending.publicationStaged
            || pending.transactionId == 0u)
        {
            return false;
        }
        if (mCpuPublication)
        {
            return mOperations.publishCpuPair
                && mOperations.publishCpuPair(
                    mOperations.context,
                    pending.tracked,
                    pending.transactionId);
        }
        color_transport::ProducerFrameIdentity identity {};
        identity.presentationMode = gpu::color_v5::PresentationMode::BinocularWorld;
        identity.producerEpoch = pending.tracked.pose.producerEpoch;
        identity.producerProcessId = GetCurrentProcessId();
        identity.transactionId = pending.transactionId;
        identity.sourceFrame = pending.tracked.pose.frame;
        identity.poseSequence = shared::sequencedValueBits(
            pending.tracked.poseSequence);
        identity.runtimeStateSample = pending.tracked.runtime.frame;
        identity.renderedDisplayTime = pending.tracked.pose.predictedDisplayTime;
        return produceAndPublish(identity);
    }

    bool stageFromAccumulationAdapter(
        engine::abi::RetailNiCameraLayout* stockCenterCamera,
        void* sceneObject,
        engine::abi::RetailBSCullingProcessLayout* stockCullingProcess,
        bool suppressPrivateRender,
        bool suppressWorldPublication,
        engine::RetailCenterRendererDiagnosticStop rendererStop) noexcept
    {
        if (!ready()
            || mPrivateRenderDispatchGate.active()
            || !stockCenterCamera
            || !sceneObject
            || !stockCullingProcess
            || !mCenterRuntime.finishStockCullerCapture(stockCullingProcess))
        {
            mPendingAccumulation = {};
            return false;
        }
        mPendingAccumulation.frame = {
            stockCenterCamera,
            sceneObject,
            stockCullingProcess,
        };
        mPendingAccumulation.rendererStop = rendererStop;
        mPendingAccumulation.threadId = GetCurrentThreadId();
        mPendingAccumulation.suppressPrivateRender = suppressPrivateRender;
        mPendingAccumulation.suppressWorldPublication =
            suppressWorldPublication;
        mPendingAccumulation.valid = true;
        return true;
    }

    bool fail(RetailVrBridgeFailure failure) noexcept
    {
        mFailure = failure;
        return false;
    }

    bool ensureFirstPersonGameplayLease() noexcept
    {
        if (mFirstPersonHookLease && mFirstPersonHookLease->installed())
            return true;
        const auto& metadata = mAuthority.authority.metadata();
        engine::RetailFirstPersonHookInstallResult installed =
            engine::installRetailFirstPersonHook(
                mAuthority.authority.engineCalls(),
                mFirstPersonHookMemory.operations(),
                metadata.runtimeImageBase,
                mFirstPersonRelays);
        mFirstPersonInstallFailure = installed.failure;
        mFirstPersonInstallFailedCallSiteIndex = installed.failedCallSiteIndex;
        if (!installed.complete())
            return false;
        mFirstPersonHookLease = new (std::nothrow)
            engine::RetailFirstPersonHookLease(std::move(installed.lease));
        if (!mFirstPersonHookLease || !mFirstPersonHookLease->installed())
        {
            delete mFirstPersonHookLease;
            mFirstPersonHookLease = nullptr;
            return false;
        }
        return true;
    }

    static RetailVrBridgeWin32* checked(void* opaque) noexcept
    {
        auto* bridge = static_cast<RetailVrBridgeWin32*>(opaque);
        return bridge && bridge->ready() ? bridge : nullptr;
    }

    static bool readTrackedFrame(
        void* opaque,
        engine::RetailTrackedFrame& frame) noexcept
    {
        RetailVrBridgeWin32* bridge = checked(opaque);
        return bridge && bridge->mTrackedFrames.readPublishedFrame(frame);
    }

    static bool prepareCameras(
        void* opaque,
        const engine::RetailWorldAccumulationCallFrame& call,
        const engine::RetailTrackedFrame& tracked,
        std::uint64_t transactionId,
        engine::RetailCenterRuntimeFrame& frame) noexcept
    {
        RetailVrBridgeWin32* bridge = checked(opaque);
        return bridge
            && bridge->mOperations.prepareDistinctCameraFrame(
                bridge->mOperations.context,
                call,
                tracked,
                transactionId,
                frame);
    }

    static bool claimWorldTransaction(
        void* opaque,
        std::uint64_t& transactionId) noexcept
    {
        RetailVrBridgeWin32* bridge = checked(opaque);
        return bridge
            && bridge->mPublicationSequence.claim(
                gpu::color_v5::PresentationMode::BinocularWorld,
                transactionId);
    }

    static engine::RetailCenterRuntimeFrameResult renderStereo(
        void* opaque,
        const engine::RetailCenterRuntimeFrame& frame) noexcept
    {
        RetailVrBridgeWin32* bridge = checked(opaque);
        if (!bridge)
            return {};
        // A new center transaction invalidates any private pair whose stock
        // desktop continuation did not reach the post-finalizer adapter.
        bridge->mDeferredFirstPerson = {};
        if (bridge->mSuppressPrivateRenderForDiagnostic)
        {
            const engine::RetailCenterRuntimeFrameResult result {
                engine::RetailWorldHookDisposition::RejectGameplayFrame,
                engine::RetailCenterRuntimeFailure::StereoRenderRejected,
                {},
            };
            bridge->mFrameDiagnostics.renderer = result;
            return result;
        }
        if (!bridge->mPrivateRenderDispatchGate.tryEnter())
        {
            const engine::RetailCenterRuntimeFrameResult result {
                engine::RetailWorldHookDisposition::RejectGameplayFrame,
                engine::RetailCenterRuntimeFailure::StereoRenderRejected,
                {},
            };
            bridge->mFrameDiagnostics.renderer = result;
            return result;
        }
        const engine::RetailCenterRuntimeFrameResult result =
            bridge->mCenterRuntime.renderWorld(
            bridge->mAuthority.authority.centerRenderer(),
            frame);
        bridge->mPrivateRenderDispatchGate.leave();
        bridge->mFrameDiagnostics.renderer = result;
        bridge->mFrameDiagnostics.accumulatorSnapshot =
            bridge->mCenterRuntime.accumulatorSnapshotDiagnostics();
        bridge->mFrameDiagnostics.rendererTiming =
            bridge->mCenterRuntime.timingDiagnostics();
        bridge->mFrameDiagnostics.rendererCullerCamera =
            bridge->mCenterRuntime.eyeCameraDiagnostics();
        // Keep the successful population diagnostics too.  Restricting this
        // copy to visibility failures hid the live first-person root and both
        // per-eye traversal results behind the default diagnostic value.
        bridge->mFrameDiagnostics.visibility =
            bridge->mCenterRuntime.visibilityDiagnostics();
        auto& diagnostics = bridge->mFrameDiagnostics.eyeCamera;
        diagnostics = {};
        diagnostics.failure = bridge->mCenterRuntime.eyeCameraFailure();
        diagnostics.gameUnitsPerMeter = frame.gameUnitsPerMeter;
        if (frame.stockCenterCamera)
        {
            diagnostics.stockVtable =
                engine::detail::retailCameraReadPointer32(
                    frame.stockCenterCamera,
                    0u);
            diagnostics.stockWorld =
                engine::detail::retailCameraReadTransform(
                    frame.stockCenterCamera,
                    engine::RetailNiAvObjectWorldTransformOffset);
            diagnostics.stockFrustum = frame.stockCenterCamera->frustum;
            diagnostics.stockTransformUsable =
                engine::detail::retailTransformUsable(
                    diagnostics.stockWorld);
            diagnostics.stockFrustumUsable =
                engine::detail::retailFrustumUsable(
                    diagnostics.stockFrustum);
        }
        std::memcpy(
            diagnostics.hmdRot,
            frame.tracked.pose.hmdRot,
            sizeof(diagnostics.hmdRot));
        std::memcpy(
            diagnostics.hmdPos,
            frame.tracked.pose.hmdPos,
            sizeof(diagnostics.hmdPos));
        std::memcpy(
            diagnostics.leftEyePos,
            frame.tracked.pose.leftEyePos,
            sizeof(diagnostics.leftEyePos));
        std::memcpy(
            diagnostics.rightEyePos,
            frame.tracked.pose.rightEyePos,
            sizeof(diagnostics.rightEyePos));
        std::memcpy(
            diagnostics.leftFov,
            frame.tracked.pose.leftFov,
            sizeof(diagnostics.leftFov));
        std::memcpy(
            diagnostics.rightFov,
            frame.tracked.pose.rightFov,
            sizeof(diagnostics.rightFov));
        const stereo::EyeBaselineValidation baseline =
            stereo::validateEyeBaseline(
                engine::detail::poseQuaternion(
                    frame.tracked.pose.hmdRot),
                engine::detail::posePosition(
                    frame.tracked.pose.leftEyePos),
                engine::detail::posePosition(
                    frame.tracked.pose.rightEyePos),
                engine::detail::posePosition(
                    frame.tracked.pose.hmdPos));
        diagnostics.eyeBaselineMeters = baseline.lengthMeters;
        diagnostics.eyeMidpointDistanceMeters =
            baseline.midpointDistanceMeters;
        diagnostics.eyeBaselineValid = baseline.valid;
        diagnostics.leftFovUsable = stereo::openXrFovAnglesUsable(
            diagnostics.leftFov);
        diagnostics.rightFovUsable = stereo::openXrFovAnglesUsable(
            diagnostics.rightFov);
        diagnostics.captured = true;
        if (result.disposition
                == engine::RetailWorldHookDisposition::StereoWorldComplete
            && result.failure == engine::RetailCenterRuntimeFailure::None
            && result.renderer.complete
            && result.cameraRig.complete()
            && frame.stockCenterCamera
            && frame.generation != 0u
            && bridge->mOperations.firstPersonGameplayLeaseReady(
                bridge->mOperations.context)
            && bridge->ensureFirstPersonGameplayLease())
        {
            // A new world transaction supersedes any pair whose stock
            // desktop finalizer did not return.  Never let that stale private
            // pair publish after a later frame.
            bridge->mDeferredFirstPerson = {};
            bridge->mPendingFirstPerson = {
                frame.stockCenterCamera,
                result.cameraRig,
                frame.tracked,
                frame.generation,
                0u,
                GetCurrentThreadId(),
                false,
                true,
            };
            bridge->mFrameDiagnostics.firstPerson.stagedGeneration =
                frame.generation;
        }
        return result;
    }

    static bool publishGpuPair(
        void* opaque,
        const engine::RetailTrackedFrame& tracked,
        std::uint64_t transactionId) noexcept
    {
        RetailVrBridgeWin32* bridge = checked(opaque);
        if (!bridge)
            return false;
        if (bridge->mSuppressWorldPublicationForDiagnostic)
            return false;
        PendingFirstPerson& pending = bridge->mPendingFirstPerson;
        if (transactionId == 0u
            || !pending.valid
            || !pending.cameraRig.complete()
            || pending.generation != transactionId
            || pending.transactionId != 0u
            || pending.threadId != GetCurrentThreadId()
            || pending.tracked.pose.frame != tracked.pose.frame
            || pending.tracked.poseSequence != tracked.poseSequence)
        {
            pending = {};
            return false;
        }
        // The controller has completed the world pair, but capture is
        // intentionally deferred. RenderFirstPerson happens later in the
        // retail frame; publishing here would freeze a real eye texture just
        // before the actual weapon pass lands in it.
        pending.transactionId = transactionId;
        pending.publicationStaged = true;
        bridge->mFrameDiagnostics.firstPerson.stagedTransactionId =
            transactionId;
        return true;
    }

    bool produceAndPublish(
        const color_transport::ProducerFrameIdentity& identity) noexcept
    {
        if (!ready()
            || mCpuPublication
            || !color_transport::frameIdentityComplete(identity))
        {
            return false;
        }
        const color_transport::ProducerPublication publication =
            mOperations.produceColorPair(
                mOperations.context,
                identity);
        if (publication.failure
                == color_transport::ProducerFailure::ConsumerReleasePending)
        {
            const ULONGLONG now = GetTickCount64();
            if (mReleasePendingSince == 0u)
                mReleasePendingSince = now;
            if (now - mReleasePendingSince >= 500u)
            {
                ++mResourceSetId;
                if (mResourceSetId == 0u)
                    ++mResourceSetId;
                static_cast<void>(mOperations.prepareColorProducer(
                    mOperations.context,
                    mResourceSetId));
                mReleasePendingSince = 0u;
            }
            return false;
        }
        mReleasePendingSince = 0u;
        return mPublisher.publish(publication);
    }

    RetailVrBridgeOperations mOperations {};
    engine::RetailRuntimeAuthorityDecision mAuthority {};
    engine::RetailEngineCallResolution mCalls {};
    engine::RetailCenterStereoRuntime<CollectorCapacity> mCenterRuntime {};
    engine::RetailTrackedFrameWin32Reader mTrackedFrames {};
    color_transport::Win32Publisher mPublisher {};
    engine::RetailWorldAccumulationController mController {};
    engine::RetailWorldAccumulationHookWin32Memory mAccumulationHookMemory {};
    engine::RetailWorldAccumulationHookLease* mAccumulationHookLease = nullptr;
    engine::RetailFirstPersonHookWin32Memory mFirstPersonHookMemory {};
    engine::RetailFirstPersonHookLease* mFirstPersonHookLease = nullptr;
    engine::RetailFirstPersonRelayAddresses mFirstPersonRelays {};
    std::uint64_t mResourceSetId = 0u;
    RetailV5PublicationSequence mPublicationSequence {};
    ULONGLONG mReleasePendingSince = 0u;
    RetailVrBridgeFrameDiagnostics mFrameDiagnostics {};
    RetailVrBridgeFailure mFailure =
        RetailVrBridgeFailure::UnsupportedArchitecture;
    bool mCpuPublication = false;
    bool mInitialized = false;
    bool mAccumulationRelayArmed = false;
    bool mRenderPhaseRelaysArmed = false;
    bool mFirstPersonRelayArmed = false;
    bool mFirstPersonPreparedRenderRelayArmed = false;
    engine::RetailFirstPersonHookInstallFailure mFirstPersonInstallFailure =
        engine::RetailFirstPersonHookInstallFailure::None;
    std::size_t mFirstPersonInstallFailedCallSiteIndex =
        engine::RetailFirstPersonCallSiteContractInventory.size();
    RetailVrPrivateRenderDispatchGate mPrivateRenderDispatchGate {};
    PendingFirstPerson mPendingFirstPerson {};
    PendingFirstPerson mDeferredFirstPerson {};
    PendingAccumulation mPendingAccumulation {};
    bool mSuppressPrivateRenderForDiagnostic = false;
    bool mSuppressWorldPublicationForDiagnostic = false;
};
}
