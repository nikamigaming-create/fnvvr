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
    OriginalUiCallRejected,
    TrackedFrameRejected,
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
    bool (*callOriginalForUi)(
        void*,
        const RetailWorldHookDispatchFrame&) noexcept = nullptr;
    bool (*readTrackedGameplayFrame)(
        void*,
        RetailTrackedFrame&) noexcept = nullptr;
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
        && operations.callOriginalForUi
        && operations.readTrackedGameplayFrame
        && operations.prepareDistinctCameraFrame
        && operations.renderStereoWorld
        && operations.publishGpuPair;
}

class RetailWorldController final
{
public:
    bool initialize(
        const RetailWorldControllerOperations& operations,
        std::uint64_t firstTransactionId = 1u) noexcept
    {
        if (!retailWorldControllerOperationsComplete(operations)
            || firstTransactionId == 0u)
        {
            return false;
        }
        mOperations = operations;
        mNextTransactionId = firstTransactionId;
        mInitialized = true;
        return true;
    }

    bool ready() const noexcept
    {
        return mInitialized
            && mNextTransactionId != 0u
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

        const auto* sceneGraph =
            static_cast<const abi::RetailSceneGraphLayout*>(
                hookFrame.retailThis);
        if (sceneGraph->isMenuSceneGraph != 0u)
        {
            if (!mOperations.callOriginalForUi(
                    mOperations.context,
                    hookFrame))
            {
                return failure(
                    RetailWorldControllerFailure::OriginalUiCallRejected);
            }
            return {
                RetailWorldHookDisposition::CallOriginalForUi,
                RetailWorldControllerFailure::None,
                0u,
            };
        }

        if (mNextTransactionId
            == (std::numeric_limits<std::uint64_t>::max)())
        {
            return failure(RetailWorldControllerFailure::CameraFrameRejected);
        }
        const std::uint64_t transactionId = mNextTransactionId++;
        RetailTrackedFrame tracked {};
        if (!mOperations.readTrackedGameplayFrame(
                mOperations.context,
                tracked))
        {
            return failure(
                RetailWorldControllerFailure::TrackedFrameRejected,
                transactionId);
        }
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
        // Hook identity and the exact validated shared snapshot are
        // authoritative. A provider may supply calibrated scale or private
        // camera preparation state, but it cannot substitute a different
        // scene, camera, pose, or transaction lineage.
        frame.sceneObject = reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(
                hookFrame.arguments.sharedRenderObjectAddress));
        frame.sceneGraph = sceneGraph;
        frame.stockCenterCamera = reinterpret_cast<
            abi::RetailNiCameraLayout*>(static_cast<std::uintptr_t>(
                sceneGraph->camera));
        frame.tracked = tracked;
        frame.generation = transactionId;
        if (!frame.sceneObject
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
    std::uint64_t mNextTransactionId = 0u;
    bool mInitialized = false;
};
}
