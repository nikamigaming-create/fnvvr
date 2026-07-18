#pragma once

#include "../protocol/fnvxr_gpu_color_transport.h"

#include <cstdint>
#include <limits>

namespace fnvxr::d3d9::color_transport
{
struct ProducerCapabilities
{
    bool sourceDeviceIsD3D9Ex = false;
    bool d3d11DeviceUsesSourceAdapter = false;
    bool d3d9TexturesOpenedByD3D11 = false;
    bool destinationTexturesUseNtHandles = false;
    bool completionUsesSharedD3D11Fence = false;
    bool cpuPixelTransferAbsent = false;
    bool depthRemainsRenderLocal = false;
};

constexpr bool capabilitiesComplete(
    const ProducerCapabilities& capabilities) noexcept
{
    return capabilities.sourceDeviceIsD3D9Ex
        && capabilities.d3d11DeviceUsesSourceAdapter
        && capabilities.d3d9TexturesOpenedByD3D11
        && capabilities.destinationTexturesUseNtHandles
        && capabilities.completionUsesSharedD3D11Fence
        && capabilities.cpuPixelTransferAbsent
        && capabilities.depthRemainsRenderLocal;
}

struct ProducerResources
{
    std::uint64_t leftColorNtHandle = 0u;
    std::uint64_t rightColorNtHandle = 0u;
    std::uint64_t sharedFenceNtHandle = 0u;
    std::uint64_t resourceSetId = 0u;
    std::uint64_t adapterLuid = 0u;
    std::uint32_t width = 0u;
    std::uint32_t height = 0u;
    gpu::GpuInteropFormat format = gpu::GpuInteropFormat::Unknown;
};

constexpr bool producerColorFormatValid(
    gpu::GpuInteropFormat format) noexcept
{
    return format == gpu::GpuInteropFormat::R8G8B8A8Unorm
        || format == gpu::GpuInteropFormat::R10G10B10A2Unorm
        || format == gpu::GpuInteropFormat::R16G16B16A16Float;
}

constexpr bool resourcesComplete(const ProducerResources& resources) noexcept
{
    return resources.leftColorNtHandle != 0u
        && resources.rightColorNtHandle != 0u
        && resources.sharedFenceNtHandle != 0u
        && resources.leftColorNtHandle != resources.rightColorNtHandle
        && resources.leftColorNtHandle != resources.sharedFenceNtHandle
        && resources.rightColorNtHandle != resources.sharedFenceNtHandle
        && resources.resourceSetId != 0u
        && resources.adapterLuid != 0u
        && resources.width != 0u
        && resources.height != 0u
        && resources.width <= gpu::color_v5::MaximumTextureDimension
        && resources.height <= gpu::color_v5::MaximumTextureDimension
        && producerColorFormatValid(resources.format);
}

struct ProducerFrameIdentity
{
    gpu::color_v5::PresentationMode presentationMode =
        gpu::color_v5::PresentationMode::Unknown;
    std::uint64_t producerEpoch = 0u;
    std::uint32_t producerProcessId = 0u;
    std::uint64_t transactionId = 0u;
    std::uint64_t sourceFrame = 0u;
    std::uint64_t poseSequence = 0u;
    std::uint64_t runtimeStateSample = 0u;
    std::int64_t renderedDisplayTime = 0;
};

constexpr bool frameIdentityComplete(
    const ProducerFrameIdentity& identity) noexcept
{
    return gpu::color_v5::validPresentationMode(identity.presentationMode)
        && identity.producerEpoch != 0u
        && identity.producerProcessId != 0u
        && identity.transactionId != 0u
        && identity.sourceFrame != 0u
        && identity.poseSequence != 0u
        && identity.runtimeStateSample != 0u
        && identity.renderedDisplayTime != 0;
}

struct ProducerOperations
{
    void* context = nullptr;
    bool (*consumerReleaseReached)(void*, std::uint64_t) noexcept = nullptr;
    bool (*synchronizeD3D9ColorWrites)(void*) noexcept = nullptr;
    bool (*copyLeftColorOnGpu)(void*) noexcept = nullptr;
    bool (*copyRightColorOnGpu)(void*) noexcept = nullptr;
    bool (*signalD3D11Fence)(void*, std::uint64_t) noexcept = nullptr;
};

constexpr bool operationsComplete(const ProducerOperations& operations) noexcept
{
    return operations.consumerReleaseReached
        && operations.synchronizeD3D9ColorWrites
        && operations.copyLeftColorOnGpu
        && operations.copyRightColorOnGpu
        && operations.signalD3D11Fence;
}

enum class ProducerFailure : std::uint8_t
{
    None,
    NotInitialized,
    InvalidFrameIdentity,
    ConsumerReleasePending,
    SequenceExhausted,
    D3D9Synchronization,
    LeftGpuCopy,
    RightGpuCopy,
    FenceSignal,
    InvalidPublicationMetadata,
};

struct ProducerPublication
{
    bool complete = false;
    ProducerFailure failure = ProducerFailure::NotInitialized;
    gpu::color_v5::SharedStereoColorPayload payload {};
};

class Producer final
{
public:
    Producer() noexcept = default;
    Producer(const Producer&) = delete;
    Producer& operator=(const Producer&) = delete;

    bool initialize(
        const ProducerOperations& operations,
        const ProducerCapabilities& capabilities,
        const ProducerResources& resources) noexcept
    {
        reset();
        if (!operationsComplete(operations)
            || !capabilitiesComplete(capabilities)
            || !resourcesComplete(resources))
        {
            return false;
        }
        mOperations = operations;
        mCapabilities = capabilities;
        mResources = resources;
        mNextReadySequence = 1u;
        mInitialized = true;
        return true;
    }

    void reset() noexcept
    {
        mInitialized = false;
        mOperations = {};
        mCapabilities = {};
        mResources = {};
        mNextReadySequence = 0u;
        mConsumerReleaseSequence = 0u;
        mAwaitingConsumerRelease = false;
    }

    bool ready() const noexcept
    {
        return mInitialized
            && operationsComplete(mOperations)
            && capabilitiesComplete(mCapabilities)
            && resourcesComplete(mResources)
            && mNextReadySequence != 0u;
    }

    ProducerPublication produce(
        const ProducerFrameIdentity& identity) noexcept
    {
        if (!ready())
            return failure(ProducerFailure::NotInitialized);
        if (!frameIdentityComplete(identity))
            return failure(ProducerFailure::InvalidFrameIdentity);
        if (mNextReadySequence
            >= (std::numeric_limits<std::uint64_t>::max)())
        {
            return failure(ProducerFailure::SequenceExhausted);
        }

        if (mAwaitingConsumerRelease)
        {
            if (!mOperations.consumerReleaseReached(
                    mOperations.context,
                    mConsumerReleaseSequence))
            {
                return failure(ProducerFailure::ConsumerReleasePending);
            }
            mAwaitingConsumerRelease = false;
            mConsumerReleaseSequence = 0u;
        }

        if (!mOperations.synchronizeD3D9ColorWrites(mOperations.context))
            return failure(ProducerFailure::D3D9Synchronization);
        if (!mOperations.copyLeftColorOnGpu(mOperations.context))
            return failure(ProducerFailure::LeftGpuCopy);
        if (!mOperations.copyRightColorOnGpu(mOperations.context))
            return failure(ProducerFailure::RightGpuCopy);

        const std::uint64_t readySequence = mNextReadySequence;
        if (!mOperations.signalD3D11Fence(
                mOperations.context,
                readySequence))
        {
            return failure(ProducerFailure::FenceSignal);
        }

        gpu::color_v5::SharedStereoColorPayload payload {};
        payload.magic = gpu::color_v5::SharedStereoColorMagic;
        payload.version = gpu::color_v5::SharedStereoColorVersion;
        payload.descriptorBytes =
            sizeof(gpu::color_v5::SharedStereoColorDescriptor);
        payload.gpuCompletionHandle = mResources.sharedFenceNtHandle;
        payload.gpuReadySequence = readySequence;
        payload.gpuConsumerReleaseSequence = readySequence + 1u;
        payload.gpuCompletionPrimitive =
            gpu::GpuCompletionPrimitive::D3D11SharedFence;
        payload.presentationMode = identity.presentationMode;
        payload.resourceSetId = mResources.resourceSetId;
        payload.producerEpoch = identity.producerEpoch;
        payload.producerProcessId = identity.producerProcessId;
        payload.adapterLuid = mResources.adapterLuid;
        payload.transactionId = identity.transactionId;
        payload.sourceFrame = identity.sourceFrame;
        payload.poseSequence = identity.poseSequence;
        payload.runtimeStateSample = identity.runtimeStateSample;
        payload.renderedDisplayTime = identity.renderedDisplayTime;
        payload.leftColor = {
            mResources.leftColorNtHandle,
            mResources.width,
            mResources.height,
            mResources.format,
            gpu::GpuSharedHandleType::D3D11NtHandle,
        };
        payload.rightColor = {
            mResources.rightColorNtHandle,
            mResources.width,
            mResources.height,
            mResources.format,
            gpu::GpuSharedHandleType::D3D11NtHandle,
        };
        if (gpu::color_v5::validatePayloadMetadata(payload)
            != gpu::color_v5::Validation::Valid)
        {
            return failure(ProducerFailure::InvalidPublicationMetadata);
        }

        mConsumerReleaseSequence = readySequence + 1u;
        mAwaitingConsumerRelease = true;
        mNextReadySequence = readySequence + 2u;
        return { true, ProducerFailure::None, payload };
    }

private:
    static ProducerPublication failure(ProducerFailure value) noexcept
    {
        return { false, value, {} };
    }

    bool mInitialized = false;
    ProducerOperations mOperations {};
    ProducerCapabilities mCapabilities {};
    ProducerResources mResources {};
    std::uint64_t mNextReadySequence = 0u;
    std::uint64_t mConsumerReleaseSequence = 0u;
    bool mAwaitingConsumerRelease = false;
};
}
