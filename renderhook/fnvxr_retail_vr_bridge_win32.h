#pragma once

#include "fnvxr_gpu_color_publisher_win32.h"

#include "../runtime/fnvxr_retail_center_runtime.h"
#include "../runtime/fnvxr_retail_runtime_authority.h"
#include "../runtime/fnvxr_retail_tracked_frame_win32.h"
#include "../runtime/fnvxr_retail_world_controller.h"
#include "../runtime/fnvxr_retail_world_hook_win32.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <utility>

namespace fnvxr::d3d9
{
struct RetailVrBridgeOperations
{
    void* context = nullptr;
    engine::RetailEyeTargetOperations eyeTargets {};
    bool (*prepareDistinctCameraFrame)(
        void*,
        const engine::RetailWorldHookDispatchFrame&,
        const engine::RetailTrackedFrame&,
        std::uint64_t,
        engine::RetailCenterRuntimeFrame&) noexcept = nullptr;
    bool (*prepareColorProducer)(void*, std::uint64_t) noexcept = nullptr;
    color_transport::ProducerPublication (*produceColorPair)(
        void*,
        const color_transport::ProducerFrameIdentity&) noexcept = nullptr;
};

constexpr bool retailVrBridgeOperationsComplete(
    const RetailVrBridgeOperations& operations) noexcept
{
    return operations.context
        && engine::retailEyeTargetOperationsComplete(operations.eyeTargets)
        && operations.prepareDistinctCameraFrame
        && operations.prepareColorProducer
        && operations.produceColorPair;
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
    WorldControllerRejected,
    HookMemoryRejected,
    HookInstallRejected,
};

template <std::size_t CollectorCapacity>
class RetailVrBridgeWin32 final
{
public:
    RetailVrBridgeWin32() noexcept = default;
    ~RetailVrBridgeWin32() noexcept
    {
        delete mHookLease;
        mHookLease = nullptr;
    }

    RetailVrBridgeWin32(const RetailVrBridgeWin32&) = delete;
    RetailVrBridgeWin32& operator=(const RetailVrBridgeWin32&) = delete;

    bool initialize(
        const RetailVrBridgeOperations& operations,
        std::uintptr_t fastcallAdapterAddress) noexcept
    {
#if !defined(_WIN32) || !defined(_M_IX86)
        (void)operations;
        (void)fastcallAdapterAddress;
        mFailure = RetailVrBridgeFailure::UnsupportedArchitecture;
        return false;
#else
        using engine::abi::BSCullingProcessVtableAddress;
        using engine::SupportedImageBase;
        if (mInitialized || mHookLease)
            return fail(RetailVrBridgeFailure::AlreadyInitialized);
        if (!retailVrBridgeOperationsComplete(operations)
            || fastcallAdapterAddress == 0u
            || fastcallAdapterAddress > 0xFFFFFFFFu)
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
        // readGameplayFrame() reopen the two mappings lazily; gameplay remains
        // fail-closed until both stable publications exist.
        static_cast<void>(mTrackedFrames.initialize());
        if (!mPublisher.initialize())
            return fail(RetailVrBridgeFailure::ColorPublisherRejected);

        mResourceSetId = metadata.generation;
        if (mResourceSetId == 0u)
            mResourceSetId = 1u;
        if (!mOperations.prepareColorProducer(
                mOperations.context,
                mResourceSetId))
        {
            return fail(RetailVrBridgeFailure::ColorProducerRejected);
        }

        engine::RetailWorldControllerOperations controllerOperations {};
        controllerOperations.context = this;
        controllerOperations.callOriginalForUi = &callOriginalForUi;
        controllerOperations.readTrackedGameplayFrame = &readTrackedFrame;
        controllerOperations.prepareDistinctCameraFrame = &prepareCameras;
        controllerOperations.renderStereoWorld = &renderStereo;
        controllerOperations.publishGpuPair = &publishGpuPair;
        if (!mController.initialize(controllerOperations))
            return fail(RetailVrBridgeFailure::WorldControllerRejected);

        if (!mHookMemory.initialize(metadata.runtimeWorldAddress))
            return fail(RetailVrBridgeFailure::HookMemoryRejected);
        const engine::RetailWorldHookDispatchOperations dispatch {
            this,
            &dispatchHookFrame,
        };
        engine::RetailWorldHookInstallResult installed =
            engine::installRetailWorldHook(
                authority.worldHook(),
                mHookMemory.operations(),
                dispatch,
                metadata.runtimeWorldAddress,
                fastcallAdapterAddress);
        if (!installed.complete())
            return fail(RetailVrBridgeFailure::HookInstallRejected);
        mHookLease = new (std::nothrow) engine::RetailWorldHookLease(
            std::move(installed.lease));
        if (!mHookLease || !mHookLease->installed())
        {
            delete mHookLease;
            mHookLease = nullptr;
            return fail(RetailVrBridgeFailure::HookInstallRejected);
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
            && mPublisher.ready()
            && mController.ready()
            && mHookLease
            && mHookLease->installed();
    }

    RetailVrBridgeFailure failure() const noexcept
    {
        return mFailure;
    }

    bool dispatchFromFastcallAdapter(
        void* retailThis,
        void* sharedRenderObject,
        std::uint32_t callerModePredicate,
        std::uint32_t stockWorldPathSelector,
        std::uint32_t postWorldOption) const noexcept
    {
        return ready()
            && mHookLease->dispatchFromFastcallAdapter(
                retailThis,
                sharedRenderObject,
                callerModePredicate,
                stockWorldPathSelector,
                postWorldOption);
    }

    bool publishMonoUiQuadFromPresent(
        const engine::RetailTrackedFrame& tracked) noexcept
    {
        if (!ready()
            || !engine::validateRetailTrackedUiFrame(tracked).complete())
        {
            return false;
        }
        color_transport::ProducerFrameIdentity identity {};
        identity.presentationMode =
            gpu::color_v5::PresentationMode::MonoUiQuad;
        identity.producerEpoch = tracked.pose.producerEpoch;
        identity.producerProcessId = GetCurrentProcessId();
        if (!mPublicationSequence.claim(
                identity.presentationMode,
                identity.transactionId))
        {
            return false;
        }
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

    static void dispatchHookFrame(
        void* opaque,
        const engine::RetailWorldHookDispatchFrame& frame) noexcept
    {
        RetailVrBridgeWin32* bridge = checked(opaque);
        if (bridge)
            static_cast<void>(bridge->mController.dispatch(frame));
    }

    static bool callOriginalForUi(
        void*,
        const engine::RetailWorldHookDispatchFrame& frame) noexcept
    {
#if defined(_MSC_VER) && defined(_M_IX86)
        if (frame.originalTrampolineAddress == 0u || !frame.retailThis)
            return false;
        auto original = reinterpret_cast<engine::RetailWorldRenderFunction>(
            static_cast<std::uintptr_t>(frame.originalTrampolineAddress));
        original(
            frame.retailThis,
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(
                frame.arguments.sharedRenderObjectAddress)),
            frame.arguments.callerModePredicate,
            frame.arguments.stockWorldPathSelector,
            frame.arguments.postWorldOption);
        return true;
#else
        (void)frame;
        return false;
#endif
    }

    static bool readTrackedFrame(
        void* opaque,
        engine::RetailTrackedFrame& frame) noexcept
    {
        RetailVrBridgeWin32* bridge = checked(opaque);
        return bridge && bridge->mTrackedFrames.readGameplayFrame(frame);
    }

    static bool prepareCameras(
        void* opaque,
        const engine::RetailWorldHookDispatchFrame& hook,
        const engine::RetailTrackedFrame& tracked,
        std::uint64_t transactionId,
        engine::RetailCenterRuntimeFrame& frame) noexcept
    {
        RetailVrBridgeWin32* bridge = checked(opaque);
        return bridge
            && bridge->mOperations.prepareDistinctCameraFrame(
                bridge->mOperations.context,
                hook,
                tracked,
                transactionId,
                frame);
    }

    static engine::RetailCenterRuntimeFrameResult renderStereo(
        void* opaque,
        const engine::RetailCenterRuntimeFrame& frame) noexcept
    {
        RetailVrBridgeWin32* bridge = checked(opaque);
        if (!bridge)
            return {};
        return bridge->mCenterRuntime.renderWorld(
            bridge->mAuthority.authority.centerRenderer(),
            frame);
    }

    static bool publishGpuPair(
        void* opaque,
        const engine::RetailTrackedFrame& tracked,
        std::uint64_t transactionId) noexcept
    {
        RetailVrBridgeWin32* bridge = checked(opaque);
        if (!bridge)
            return false;
        color_transport::ProducerFrameIdentity identity {};
        identity.presentationMode =
            gpu::color_v5::PresentationMode::BinocularWorld;
        identity.producerEpoch = tracked.pose.producerEpoch;
        identity.producerProcessId = GetCurrentProcessId();
        if (transactionId == 0u
            || !bridge->mPublicationSequence.claim(
                identity.presentationMode,
                identity.transactionId))
        {
            return false;
        }
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
            || !gpu::color_v5::validPresentationMode(
                identity.presentationMode))
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
    engine::RetailWorldController mController {};
    engine::RetailWorldHookWin32Memory mHookMemory {};
    engine::RetailWorldHookLease* mHookLease = nullptr;
    std::uint64_t mResourceSetId = 0u;
    RetailV5PublicationSequence mPublicationSequence {};
    ULONGLONG mReleasePendingSince = 0u;
    RetailVrBridgeFailure mFailure =
        RetailVrBridgeFailure::UnsupportedArchitecture;
    bool mInitialized = false;
};
}
