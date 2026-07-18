#pragma once

#include "../runtime/fnvxr_retail_tracked_frame.h"

#include <cstdint>

namespace fnvxr::d3d9
{
enum class RetailUiQuadCaptureFailure : std::uint8_t
{
    None = 0u,
    NotInitialized,
    MissingDevice,
    RuntimeSampleUnavailable,
    RuntimeNotConfirmedUi,
    BackBufferCopyFailed,
    PublicationFailed,
};

struct RetailUiQuadCaptureOperations
{
    void* context = nullptr;
    bool (*readPublishedFrame)(
        void*,
        engine::RetailTrackedFrame&) noexcept = nullptr;
    bool (*copyBackBufferToMonoTargets)(void*, void*) noexcept = nullptr;
    bool (*publishMonoUiQuad)(
        void*,
        const engine::RetailTrackedFrame&) noexcept = nullptr;
    void (*withholdMonoUiQuad)(
        void*,
        RetailUiQuadCaptureFailure) noexcept = nullptr;
};

constexpr bool retailUiQuadCaptureOperationsComplete(
    const RetailUiQuadCaptureOperations& operations) noexcept
{
    return operations.context
        && operations.readPublishedFrame
        && operations.copyBackBufferToMonoTargets
        && operations.publishMonoUiQuad
        && operations.withholdMonoUiQuad;
}

class RetailUiQuadCaptureController final
{
public:
    bool initialize(
        const RetailUiQuadCaptureOperations& operations) noexcept
    {
        if (mInitialized || !retailUiQuadCaptureOperationsComplete(operations))
            return false;
        mOperations = operations;
        mFailure = RetailUiQuadCaptureFailure::None;
        mInitialized = true;
        return true;
    }

    bool ready() const noexcept
    {
        return mInitialized
            && retailUiQuadCaptureOperationsComplete(mOperations);
    }

    bool beforePresent(void* authorizedDevice) noexcept
    {
        if (!ready())
            return withhold(RetailUiQuadCaptureFailure::NotInitialized);
        if (!authorizedDevice)
            return withhold(RetailUiQuadCaptureFailure::MissingDevice);

        engine::RetailTrackedFrame frame {};
        if (!mOperations.readPublishedFrame(mOperations.context, frame))
        {
            return withhold(
                RetailUiQuadCaptureFailure::RuntimeSampleUnavailable);
        }
        if (!engine::validateRetailTrackedUiFrame(frame).complete())
        {
            return withhold(
                RetailUiQuadCaptureFailure::RuntimeNotConfirmedUi);
        }
        if (!mOperations.copyBackBufferToMonoTargets(
                mOperations.context,
                authorizedDevice))
        {
            return withhold(
                RetailUiQuadCaptureFailure::BackBufferCopyFailed);
        }
        if (!mOperations.publishMonoUiQuad(mOperations.context, frame))
        {
            return withhold(RetailUiQuadCaptureFailure::PublicationFailed);
        }
        mFailure = RetailUiQuadCaptureFailure::None;
        return true;
    }

    RetailUiQuadCaptureFailure failure() const noexcept
    {
        return mFailure;
    }

private:
    bool withhold(RetailUiQuadCaptureFailure failure) noexcept
    {
        mFailure = failure;
        if (mOperations.withholdMonoUiQuad)
            mOperations.withholdMonoUiQuad(mOperations.context, failure);
        return false;
    }

    RetailUiQuadCaptureOperations mOperations {};
    RetailUiQuadCaptureFailure mFailure =
        RetailUiQuadCaptureFailure::NotInitialized;
    bool mInitialized = false;
};
}
