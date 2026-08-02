#pragma once

#include "fnvxr_retail_center_runtime.h"
#include "fnvxr_retail_tracked_frame.h"

#include <cmath>
#include <cstdint>
#include <limits>

namespace fnvxr::engine
{
// This is the exact cdecl argument triple at an audited in-body
// `call AccumulateScene` instruction. It intentionally has no renderer
// owner, selector, or entry-trampoline fields: those are outside the live
// stock accumulation scope and must not be substituted for this scene.
struct RetailWorldAccumulationCallFrame
{
    abi::RetailNiCameraLayout* stockCenterCamera = nullptr;
    void* sceneObject = nullptr;
    abi::RetailBSCullingProcessLayout* stockCullingProcess = nullptr;
};

enum class RetailWorldAccumulationControllerFailure : std::uint8_t
{
    None = 0u,
    OperationsIncomplete,
    InvalidCallFrame,
    TrackedFrameRejected,
    TransactionClaimRejected,
    CameraFrameRejected,
    CallArgumentsMismatch,
    StereoRenderRejected,
    GpuPublicationRejected,
};

struct RetailWorldAccumulationControllerResult
{
    RetailWorldHookDisposition disposition =
        RetailWorldHookDisposition::RejectGameplayFrame;
    RetailWorldAccumulationControllerFailure failure =
        RetailWorldAccumulationControllerFailure::OperationsIncomplete;
    std::uint64_t transactionId = 0u;

    constexpr bool complete() const noexcept
    {
        return failure == RetailWorldAccumulationControllerFailure::None
            && (disposition
                    == RetailWorldHookDisposition::CallOriginalForUi
                || disposition
                    == RetailWorldHookDisposition::StereoWorldComplete);
    }
};

struct RetailWorldAccumulationControllerOperations
{
    void* context = nullptr;
    bool (*readTrackedFrame)(void*, RetailTrackedFrame&) noexcept = nullptr;
    bool (*claimWorldTransaction)(void*, std::uint64_t&) noexcept = nullptr;
    bool (*prepareDistinctCameraFrame)(
        void*,
        const RetailWorldAccumulationCallFrame&,
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

constexpr bool retailWorldAccumulationControllerOperationsComplete(
    const RetailWorldAccumulationControllerOperations& operations) noexcept
{
    return operations.context
        && operations.readTrackedFrame
        && operations.claimWorldTransaction
        && operations.prepareDistinctCameraFrame
        && operations.renderStereoWorld
        && operations.publishGpuPair;
}

// The audited stock call completes before this controller runs. A rejected
// private transaction therefore never suppresses or replaces the retail world
// pass, while the enclosing RenderWorldSceneGraph state remains live.
class RetailWorldAccumulationController final
{
public:
    bool initialize(
        const RetailWorldAccumulationControllerOperations& operations) noexcept
    {
        if (!retailWorldAccumulationControllerOperationsComplete(operations))
            return false;
        mOperations = operations;
        mLastClaimedTransactionId = 0u;
        mInitialized = true;
        return true;
    }

    bool ready() const noexcept
    {
        return mInitialized
            && retailWorldAccumulationControllerOperationsComplete(mOperations);
    }

    RetailWorldAccumulationControllerResult dispatch(
        const RetailWorldAccumulationCallFrame& call) noexcept
    {
        if (!ready())
        {
            return failure(
                RetailWorldAccumulationControllerFailure::OperationsIncomplete);
        }
        if (!call.stockCenterCamera
            || !call.sceneObject
            || !call.stockCullingProcess)
        {
            return failure(
                RetailWorldAccumulationControllerFailure::InvalidCallFrame);
        }

        RetailTrackedFrame tracked {};
        if (!mOperations.readTrackedFrame(mOperations.context, tracked))
        {
            return failure(
                RetailWorldAccumulationControllerFailure::TrackedFrameRejected);
        }

        // A confirmed UI still reaches the unmodified stock AccumulateScene
        // call after this dispatch. It must never enter the private world
        // transaction merely because an opaque scene pointer is non-null.
        const RetailTrackedPresentationRoute route =
            retailTrackedPresentationRoute(tracked);
        if (route == RetailTrackedPresentationRoute::MonoUiQuad)
        {
            return {
                RetailWorldHookDisposition::CallOriginalForUi,
                RetailWorldAccumulationControllerFailure::None,
                0u,
            };
        }
        if (route != RetailTrackedPresentationRoute::BinocularWorld)
        {
            return failure(
                RetailWorldAccumulationControllerFailure::TrackedFrameRejected);
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
                RetailWorldAccumulationControllerFailure::
                    TransactionClaimRejected);
        }
        mLastClaimedTransactionId = transactionId;

        RetailCenterRuntimeFrame frame {};
        if (!mOperations.prepareDistinctCameraFrame(
                mOperations.context,
                call,
                tracked,
                transactionId,
                frame))
        {
            return failure(
                RetailWorldAccumulationControllerFailure::CameraFrameRejected,
                transactionId);
        }

        const auto* expectedSceneGraph =
            reinterpret_cast<const abi::RetailSceneGraphLayout*>(
                call.sceneObject);
        if (frame.sceneGraph != expectedSceneGraph
            || frame.stockCenterCamera != call.stockCenterCamera)
        {
            return failure(
                RetailWorldAccumulationControllerFailure::
                    CallArgumentsMismatch,
                transactionId);
        }
        frame.sceneObject = call.sceneObject;
        frame.stockCullingProcess = call.stockCullingProcess;
        frame.tracked = tracked;
        frame.generation = transactionId;
        if (!std::isfinite(frame.gameUnitsPerMeter)
            || frame.gameUnitsPerMeter <= 0.0f)
        {
            return failure(
                RetailWorldAccumulationControllerFailure::CameraFrameRejected,
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
                RetailWorldAccumulationControllerFailure::StereoRenderRejected,
                transactionId);
        }
        if (!mOperations.publishGpuPair(
                mOperations.context,
                tracked,
                transactionId))
        {
            return failure(
                RetailWorldAccumulationControllerFailure::GpuPublicationRejected,
                transactionId);
        }
        return {
            RetailWorldHookDisposition::StereoWorldComplete,
            RetailWorldAccumulationControllerFailure::None,
            transactionId,
        };
    }

private:
    static constexpr RetailWorldAccumulationControllerResult failure(
        RetailWorldAccumulationControllerFailure value,
        std::uint64_t transactionId = 0u) noexcept
    {
        return {
            RetailWorldHookDisposition::RejectGameplayFrame,
            value,
            transactionId,
        };
    }

    RetailWorldAccumulationControllerOperations mOperations {};
    std::uint64_t mLastClaimedTransactionId = 0u;
    bool mInitialized = false;
};
}
