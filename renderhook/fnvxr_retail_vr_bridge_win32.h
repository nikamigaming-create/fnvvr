#pragma once

#include "fnvxr_gpu_color_publisher_win32.h"

#include "../runtime/fnvxr_retail_center_runtime.h"
#include "../runtime/fnvxr_retail_runtime_authority.h"
#include "../runtime/fnvxr_retail_tracked_frame_win32.h"
#include "../runtime/fnvxr_retail_world_accumulation_controller.h"
#include "../runtime/fnvxr_retail_world_accumulation_hook_win32.h"

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
    // arms it immediately before the three calls are patched.
    bool (*armAccumulationCallRelay)(void*, std::uintptr_t) noexcept =
        nullptr;
    void (*disarmAccumulationCallRelay)(void*) noexcept = nullptr;
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
        && operations.armAccumulationCallRelay
        && operations.disarmAccumulationCallRelay
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
    std::uint64_t dispatchCount = 0u;
    std::uint64_t stereoCompleteCount = 0u;
};

template <std::size_t CollectorCapacity>
class RetailVrBridgeWin32 final
{
public:
    RetailVrBridgeWin32() noexcept = default;
    ~RetailVrBridgeWin32() noexcept
    {
        delete mAccumulationHookLease;
        mAccumulationHookLease = nullptr;
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
        std::uintptr_t cdeclAdapterAddress) noexcept
    {
#if !defined(_WIN32) || !defined(_M_IX86)
        (void)operations;
        (void)cdeclAdapterAddress;
        mFailure = RetailVrBridgeFailure::UnsupportedArchitecture;
        return false;
#else
        using engine::abi::BSCullingProcessVtableAddress;
        using engine::SupportedImageBase;
        if (mInitialized || mAccumulationHookLease)
            return fail(RetailVrBridgeFailure::AlreadyInitialized);
        if (!retailVrBridgeOperationsComplete(operations)
            || cdeclAdapterAddress == 0u
            || cdeclAdapterAddress > 0xFFFFFFFFu)
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
        if (!mCalls.complete())
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
        engine::RetailWorldAccumulationHookInstallResult installed =
            engine::installRetailWorldAccumulationHook(
                authority.worldHook(),
                mAccumulationHookMemory.operations(),
                metadata.runtimeWorldAddress,
                cdeclAdapterAddress);
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

    const engine::RetailRuntimeAuthorityDecision& authorityDecision() const noexcept
    {
        return mAuthority;
    }

    RetailVrBridgeFrameDiagnostics frameDiagnostics() const noexcept
    {
        return mFrameDiagnostics;
    }

    void beginStockCullerCaptureFromAccumulationAdapter(
        engine::abi::RetailBSCullingProcessLayout* stockCullingProcess)
        noexcept
    {
        if (ready())
        {
            static_cast<void>(
                mCenterRuntime.beginStockCullerCapture(stockCullingProcess));
        }
    }

    void dispatchFromAccumulationAdapter(
        engine::abi::RetailNiCameraLayout* stockCenterCamera,
        void* sceneObject,
        engine::abi::RetailBSCullingProcessLayout* stockCullingProcess)
        noexcept
    {
        // The proxy's stack/register-preserving relay has already returned
        // from the exact stock AccumulateScene target.  Run optional private
        // work only after that return, while the enclosing
        // RenderWorldSceneGraph scope is still live.
        if (ready())
        {
            static_cast<void>(
                mCenterRuntime.finishStockCullerCapture(stockCullingProcess));
            mFrameDiagnostics.renderer = {};
            mFrameDiagnostics.eyeCamera = {};
            mFrameDiagnostics.visibility = {};
            mFrameDiagnostics.accumulatorSnapshot = {};
            const engine::RetailWorldAccumulationControllerResult result =
                mController.dispatch({
                    stockCenterCamera,
                    sceneObject,
                    stockCullingProcess,
                });
            mFrameDiagnostics.controller = result;
            ++mFrameDiagnostics.dispatchCount;
            if (result.complete()
                && result.disposition
                    == engine::RetailWorldHookDisposition::StereoWorldComplete)
            {
                ++mFrameDiagnostics.stereoCompleteCount;
            }
        }

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

private:
    bool fail(RetailVrBridgeFailure failure) noexcept
    {
        mFailure = failure;
        return false;
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
        const engine::RetailCenterRuntimeFrameResult result =
            bridge->mCenterRuntime.renderWorld(
            bridge->mAuthority.authority.centerRenderer(),
            frame);
        bridge->mFrameDiagnostics.renderer = result;
        bridge->mFrameDiagnostics.accumulatorSnapshot =
            bridge->mCenterRuntime.accumulatorSnapshotDiagnostics();
        if (result.renderer.failure == engine::CenterRendererFailure::Visibility)
        {
            bridge->mFrameDiagnostics.visibility =
                bridge->mCenterRuntime.visibilityDiagnostics();
        }
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
        if (bridge->mCpuPublication)
        {
            return bridge->mOperations.publishCpuPair
                && bridge->mOperations.publishCpuPair(
                    bridge->mOperations.context,
                    tracked,
                    transactionId);
        }
        color_transport::ProducerFrameIdentity identity {};
        identity.presentationMode =
            gpu::color_v5::PresentationMode::BinocularWorld;
        identity.producerEpoch = tracked.pose.producerEpoch;
        identity.producerProcessId = GetCurrentProcessId();
        if (transactionId == 0u)
            return false;
        // The controller claimed this exact shared identity before the eye
        // cameras rendered. Do not allocate another ID here: a UI quad may
        // have consumed an intervening value, but this pair must describe the
        // rendered world transaction rather than publication timing.
        identity.transactionId = transactionId;
        identity.sourceFrame = tracked.pose.frame;
        identity.poseSequence =
            shared::sequencedValueBits(tracked.poseSequence);
        identity.runtimeStateSample = tracked.runtime.frame;
        identity.renderedDisplayTime = tracked.pose.predictedDisplayTime;
        return bridge->produceAndPublish(identity);
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
    std::uint64_t mResourceSetId = 0u;
    RetailV5PublicationSequence mPublicationSequence {};
    ULONGLONG mReleasePendingSince = 0u;
    RetailVrBridgeFrameDiagnostics mFrameDiagnostics {};
    RetailVrBridgeFailure mFailure =
        RetailVrBridgeFailure::UnsupportedArchitecture;
    bool mCpuPublication = false;
    bool mInitialized = false;
    bool mAccumulationRelayArmed = false;
};
}
