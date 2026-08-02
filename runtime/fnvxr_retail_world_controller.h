#pragma once

#include "fnvxr_retail_center_runtime.h"
#include "fnvxr_retail_tracked_frame.h"
#include "fnvxr_retail_world_hook_lease.h"

#include <cmath>
#include <cstdint>
#include <limits>

namespace fnvxr::engine
{
enum class RetailWorldControllerFailure : std::uint8_t
{
    None = 0u,
    OperationsIncomplete,
    InvalidHookFrame,
    OriginalRetailPassRejected,
    TrackedFrameRejected,
    TransactionClaimRejected,
    CameraFrameRejected,
    StereoRenderRejected,
    GpuPublicationRejected,
};

struct RetailWorldControllerResult
{
    RetailWorldHookDisposition disposition =
        RetailWorldHookDisposition::RejectGameplayFrame;
    RetailWorldControllerFailure failure =
        RetailWorldControllerFailure::OperationsIncomplete;
    std::uint64_t transactionId = 0u;

    constexpr bool complete() const noexcept
    {
        return failure == RetailWorldControllerFailure::None
            && (disposition
                    == RetailWorldHookDisposition::CallOriginalForUi
                || disposition
                    == RetailWorldHookDisposition::StereoWorldComplete);
    }
};

struct RetailWorldControllerOperations
{
    void* context = nullptr;
    // The exact trampoline runs one stock retail pass after the authenticated
    // route decision. In UI it is the UI pass; in gameplay it is an internal
    // renderer prelude that initializes the stock world state before the
    // isolated eye transaction. The prelude is never itself publishable.
    bool (*callOriginalRetailPass)(
        void*,
        const RetailWorldHookDispatchFrame&) noexcept = nullptr;
    // The controller owns the final UI-versus-world presentation decision;
    // providers therefore supply a validated published snapshot rather than
    // pre-classifying it as gameplay.
    bool (*readTrackedFrame)(
        void*,
        RetailTrackedFrame&) noexcept = nullptr;
    // UI and world publications share one monotonically increasing identity
    // domain. Claim the world identity before deriving/rendering either eye so
    // the renderer generation and the published pair can never diverge after
    // an intervening UI quad.
    bool (*claimWorldTransaction)(
        void*,
        std::uint64_t&) noexcept = nullptr;
    bool (*prepareDistinctCameraFrame)(
        void*,
        const RetailWorldHookDispatchFrame&,
        const RetailTrackedFrame&,
        std::uint64_t,
        RetailCenterRuntimeFrame&) noexcept = nullptr;
    RetailCenterRuntimeFrameResult (*renderStereoWorld)(
        void*,
        const RetailCenterRuntimeFrame&) noexcept = nullptr;
    bool (*publishGpuPair)(
        void*,
        const RetailTrackedFrame&,
        std::uint64_t) noexcept = nullptr;
};

constexpr bool retailWorldControllerOperationsComplete(
    const RetailWorldControllerOperations& operations) noexcept
{
    return operations.context
        && operations.callOriginalRetailPass
        && operations.readTrackedFrame
        && operations.claimWorldTransaction
        && operations.prepareDistinctCameraFrame
        && operations.renderStereoWorld
        && operations.publishGpuPair;
}

class RetailWorldController final
{
public:
    bool initialize(
        const RetailWorldControllerOperations& operations) noexcept
    {
        if (!retailWorldControllerOperationsComplete(operations))
        {
            return false;
        }
        mOperations = operations;
        mLastClaimedTransactionId = 0u;
        mInitialized = true;
        return true;
    }

    bool ready() const noexcept
    {
        return mInitialized
            && retailWorldControllerOperationsComplete(mOperations);
    }

    RetailWorldControllerResult dispatch(
        const RetailWorldHookDispatchFrame& hookFrame) noexcept
    {
        if (!ready())
            return failure(RetailWorldControllerFailure::OperationsIncomplete);
        if (!hookFrame.retailThis
            || hookFrame.arguments.sharedRenderObjectAddress == 0u
            || hookFrame.originalTrampolineAddress == 0u)
        {
            return failure(RetailWorldControllerFailure::InvalidHookFrame);
        }

        RetailTrackedFrame tracked {};
        if (!mOperations.readTrackedFrame(
                mOperations.context,
                tracked))
        {
            return failure(
                RetailWorldControllerFailure::TrackedFrameRejected);
        }

        // The SceneGraph word at 0xE8 is undocumented and cannot classify a
        // menu safely. Route only from the authenticated published runtime
        // state, so UI and world remain mutually exclusive without guessing
        // from private engine memory.
        const RetailTrackedPresentationRoute route =
            retailTrackedPresentationRoute(tracked);
        if (route == RetailTrackedPresentationRoute::MonoUiQuad)
        {
            if (!mOperations.callOriginalRetailPass(
                    mOperations.context,
                    hookFrame))
            {
                return failure(
                    RetailWorldControllerFailure::OriginalRetailPassRejected);
            }
            return {
                RetailWorldHookDisposition::CallOriginalForUi,
                RetailWorldControllerFailure::None,
                0u,
            };
        }
        if (route != RetailTrackedPresentationRoute::BinocularWorld)
        {
            return failure(RetailWorldControllerFailure::TrackedFrameRejected);
        }

        // The entry detour otherwise suppresses the stock body completely.
        // Run its exact trampoline once before building the private visible
        // list so per-frame renderer/culling setup has occurred. This pass is
        // not sent to the headset and cannot satisfy a stereo transaction;
        // only the later explicit left/right eye pair may be published.
        if (!mOperations.callOriginalRetailPass(
                mOperations.context,
                hookFrame))
        {
            return failure(
                RetailWorldControllerFailure::OriginalRetailPassRejected);
        }

        std::uint64_t transactionId = 0u;
        if (!mOperations.claimWorldTransaction(
                mOperations.context,
                transactionId)
            || transactionId == 0u
            || transactionId == (std::numeric_limits<std::uint64_t>::max)()
            || transactionId <= mLastClaimedTransactionId)
        {
            return failure(
                RetailWorldControllerFailure::TransactionClaimRejected);
        }
        // Claiming reserves the identity even if camera preparation or
        // rendering later fails. The next completed frame must still never
        // reuse an identity that was already associated with an attempted
        // world transaction.
        mLastClaimedTransactionId = transactionId;
        RetailCenterRuntimeFrame frame {};
        if (!mOperations.prepareDistinctCameraFrame(
                mOperations.context,
                hookFrame,
                tracked,
                transactionId,
                frame))
        {
            return failure(
                RetailWorldControllerFailure::CameraFrameRejected,
                transactionId);
        }
        // The exact stock body obtains its culling root independently from
        // its stack-side shared render object before it calls
        // AccumulateScene. The hook's `this` is renderer-owned, and the
        // shared stack object is likewise not a substitute scene root. The
        // provider has already resolved the same live global SceneGraph and
        // stock camera that the stock body uses, so preserve that root for
        // the private conservative traversal.
        frame.sceneObject = const_cast<abi::RetailSceneGraphLayout*>(
            frame.sceneGraph);
        frame.tracked = tracked;
        frame.generation = transactionId;
        if (!frame.sceneObject
            || !frame.sceneGraph
            || !frame.stockCenterCamera
            || !std::isfinite(frame.gameUnitsPerMeter)
            || frame.gameUnitsPerMeter <= 0.0f)
        {
            return failure(
                RetailWorldControllerFailure::CameraFrameRejected,
                transactionId);
        }
        const RetailCenterRuntimeFrameResult rendered =
            mOperations.renderStereoWorld(mOperations.context, frame);
        if (rendered.disposition
                != RetailWorldHookDisposition::StereoWorldComplete
            || rendered.failure != RetailCenterRuntimeFailure::None
            || !rendered.renderer.complete)
        {
            return failure(
                RetailWorldControllerFailure::StereoRenderRejected,
                transactionId);
        }
        if (!mOperations.publishGpuPair(
                mOperations.context,
                tracked,
                transactionId))
        {
            return failure(
                RetailWorldControllerFailure::GpuPublicationRejected,
                transactionId);
        }
        return {
            RetailWorldHookDisposition::StereoWorldComplete,
            RetailWorldControllerFailure::None,
            transactionId,
        };
    }

private:
    static constexpr RetailWorldControllerResult failure(
        RetailWorldControllerFailure value,
        std::uint64_t transactionId = 0u) noexcept
    {
        return {
            RetailWorldHookDisposition::RejectGameplayFrame,
            value,
            transactionId,
        };
    }

    RetailWorldControllerOperations mOperations {};
    std::uint64_t mLastClaimedTransactionId = 0u;
    bool mInitialized = false;
};
}
