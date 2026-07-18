#include "fnvxr_gpu_frame_transport.h"

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <type_traits>

namespace
{
int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}

template <typename FrameLike>
void populateValidDescriptorPayload(FrameLike& value)
{
    using namespace fnvxr::gpu;
    value.magic = SharedStereoFrameMagic;
    value.version = SharedStereoFrameVersion;
    value.descriptorBytes = sizeof(value);
    value.gpuCompletionHandle = 0x201;
    value.gpuReadySequence = 41;
    value.gpuConsumerReleaseSequence = 42;
    value.gpuCompletionPrimitive = GpuCompletionPrimitive::D3D11SharedFence;
    value.resourceSetId = 0x301;
    value.producerEpoch = 9;
    value.producerProcessId = 123;
    value.adapterLuid = 0x1122334455667788ull;
    value.transactionId = 1001;
    value.sourceFrame = 2002;
    value.poseSequence = 3003;
    value.runtimeStateSample = 3503;
    value.renderedDisplayTime = 4004;
    value.depthEncoding = GpuDepthEncoding::LinearViewDepthMetersNormalized;
    value.depthNearMeters = 0.05f;
    value.depthFarMeters = 1000.0f;
    value.leftColor = {
        0x101, 2064, 2208, GpuInteropFormat::R8G8B8A8Unorm,
        GpuSharedHandleType::D3D11NtHandle,
    };
    value.rightColor = {
        0x102, 2064, 2208, GpuInteropFormat::R8G8B8A8Unorm,
        GpuSharedHandleType::D3D11NtHandle,
    };
    value.leftDepth = {
        0x103, 2064, 2208, GpuInteropFormat::R16G16B16A16Float,
        GpuSharedHandleType::D3D11NtHandle,
    };
    value.rightDepth = {
        0x104, 2064, 2208, GpuInteropFormat::R16G16B16A16Float,
        GpuSharedHandleType::D3D11NtHandle,
    };
}

void populateValidDescriptor(fnvxr::gpu::SharedStereoFrameDescriptor& value)
{
    populateValidDescriptorPayload(value);
    value.publicationSequence.store(2u, std::memory_order_release);
}

fnvxr::gpu::GpuCompletionObservation validCompletionObservation()
{
    using namespace fnvxr::gpu;
    GpuCompletionObservation observation {};
    observation.primitive = GpuCompletionPrimitive::D3D11SharedFence;
    observation.declaredCompletionHandle = 0x201;
    observation.reachedSequence = 41;
    observation.resourceSetId = 0x301;
    observation.producerEpoch = 9;
    observation.producerProcessId = 123;
    observation.adapterLuid = 0x1122334455667788ull;
    observation.transactionId = 1001;
    observation.sourceFrame = 2002;
    observation.poseSequence = 3003;
    observation.runtimeStateSample = 3503;
    observation.consumerObservedReached = true;
    return observation;
}

fnvxr::gpu::GpuConsumerReleaseObservation validConsumerReleaseObservation()
{
    using namespace fnvxr::gpu;
    GpuConsumerReleaseObservation observation {};
    observation.primitive = GpuCompletionPrimitive::D3D11SharedFence;
    observation.declaredCompletionHandle = 0x201;
    observation.reachedSequence = 42;
    observation.resourceSetId = 0x301;
    observation.producerEpoch = 9;
    observation.producerProcessId = 123;
    observation.adapterLuid = 0x1122334455667788ull;
    observation.transactionId = 1001;
    observation.sourceFrame = 2002;
    observation.poseSequence = 3003;
    observation.runtimeStateSample = 3503;
    observation.producerObservedReached = true;
    return observation;
}
}

int main()
{
    using namespace fnvxr::gpu;

    static_assert(SharedStereoFrameVersion == 4u, "GPU ABI revision must be explicit");
    static_assert(std::is_standard_layout<GpuTextureDescriptor>::value,
        "texture descriptor must have a C-compatible layout");
    static_assert(sizeof(GpuTextureDescriptor) == 24u,
        "texture descriptor size must be identical on Win32 and x64");
    static_assert(alignof(GpuTextureDescriptor) == 8u,
        "texture descriptor alignment must be identical on Win32 and x64");
    static_assert(offsetof(GpuTextureDescriptor, sharedHandle) == 0u, "texture handle ABI drift");
    static_assert(offsetof(GpuTextureDescriptor, width) == 8u, "texture width ABI drift");
    static_assert(offsetof(GpuTextureDescriptor, height) == 12u, "texture height ABI drift");
    static_assert(offsetof(GpuTextureDescriptor, format) == 16u, "texture format ABI drift");
    static_assert(offsetof(GpuTextureDescriptor, handleType) == 20u,
        "texture shared-handle type ABI drift");

    static_assert(std::is_standard_layout<SharedStereoFrameDescriptor>::value,
        "shared stereo descriptor must have a C-compatible layout");
    static_assert(sizeof(SharedStereoFrameDescriptor) == 232u,
        "shared stereo ABI size must be identical on Win32 and x64");
    static_assert(alignof(SharedStereoFrameDescriptor) == 8u,
        "shared stereo ABI alignment must be identical on Win32 and x64");
    static_assert(offsetof(SharedStereoFrameDescriptor, publicationSequence) == 12u,
        "publication sequence ABI drift");
    static_assert(offsetof(SharedStereoFrameDescriptor, gpuCompletionHandle) == 16u,
        "GPU completion handle ABI drift");
    static_assert(offsetof(SharedStereoFrameDescriptor, gpuReadySequence) == 24u,
        "GPU completion value ABI drift");
    static_assert(offsetof(SharedStereoFrameDescriptor, gpuConsumerReleaseSequence) == 32u,
        "GPU consumer-release value ABI drift");
    static_assert(offsetof(SharedStereoFrameDescriptor, gpuCompletionPrimitive) == 40u,
        "GPU completion primitive ABI drift");
    static_assert(offsetof(SharedStereoFrameDescriptor, resourceSetId) == 48u,
        "resource-set identity ABI drift");
    static_assert(offsetof(SharedStereoFrameDescriptor, producerEpoch) == 56u,
        "producer epoch ABI drift");
    static_assert(offsetof(SharedStereoFrameDescriptor, adapterLuid) == 72u,
        "adapter LUID ABI drift");
    static_assert(offsetof(SharedStereoFrameDescriptor, runtimeStateSample) == 104u,
        "runtime-state sample ABI drift");
    static_assert(offsetof(SharedStereoFrameDescriptor, depthEncoding) == 120u,
        "encoded-depth semantic ABI drift");
    static_assert(offsetof(SharedStereoFrameDescriptor, depthNearMeters) == 128u,
        "encoded-depth near plane ABI drift");
    static_assert(offsetof(SharedStereoFrameDescriptor, depthFarMeters) == 132u,
        "encoded-depth far plane ABI drift");
    static_assert(offsetof(SharedStereoFrameDescriptor, leftColor) == 136u,
        "left color ABI drift");
    static_assert(offsetof(SharedStereoFrameDescriptor, rightColor) == 160u,
        "right color ABI drift");
    static_assert(offsetof(SharedStereoFrameDescriptor, leftDepth) == 184u,
        "left depth ABI drift");
    static_assert(offsetof(SharedStereoFrameDescriptor, rightDepth) == 208u,
        "right depth ABI drift");

    if (std::strcmp(SharedStereoFrameMappingName, "Local\\FNVXR_GPU_StereoFrame_v4") != 0)
        return fail("wire-layout revision must use a new mapping name");

    {
        const float nearMeters = 0.05f;
        const float farMeters = 1000.0f;
        const float midpoint = (nearMeters + farMeters) * 0.5f;
        if (std::fabs(encodeLinearViewDepthMeters(nearMeters, nearMeters, farMeters) - 0.0f) > 1e-6f
            || std::fabs(encodeLinearViewDepthMeters(farMeters, nearMeters, farMeters) - 1.0f) > 1e-6f
            || std::fabs(encodeLinearViewDepthMeters(midpoint, nearMeters, farMeters) - 0.5f) > 1e-6f
            || std::fabs(decodeLinearViewDepthMeters(0.5f, nearMeters, farMeters) - midpoint) > 1e-3f)
        {
            return fail("canonical encoded depth must be normalized linear view depth in meters");
        }
        if (CanonicalEncodedDepthMin != 0.0f
            || CanonicalEncodedDepthMax != 1.0f
            || CanonicalEncodedDepthBackgroundR != CanonicalEncodedDepthMax
            || CanonicalEncodedDepthBackgroundA != 0.0f)
        {
            return fail("canonical encoded-depth clear/background values must be explicit");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        if (validateSharedStereoFrame(descriptor, validCompletionObservation())
            != GpuFrameValidation::Valid)
        {
            return fail("complete GPU stereo descriptor with observed completion must validate");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        descriptor.leftColor.handleType = GpuSharedHandleType::Unknown;
        if (validateSharedStereoFrame(descriptor, validCompletionObservation())
            != GpuFrameValidation::InvalidSharedHandleType)
        {
            return fail("texture publication must require D3D11 NT shared handles");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        descriptor.rightColor.format = GpuInteropFormat::R10G10B10A2Unorm;
        if (validateSharedStereoFrame(descriptor, validCompletionObservation())
            != GpuFrameValidation::MismatchedColorFormats)
        {
            return fail("left and right color formats must match exactly");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        descriptor.rightColor.sharedHandle = descriptor.leftColor.sharedHandle;
        if (validateSharedStereoFrame(descriptor, validCompletionObservation())
            != GpuFrameValidation::AliasedEyeResources)
        {
            return fail("left and right eyes must not alias one GPU texture");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        descriptor.rightDepth.height--;
        if (validateSharedStereoFrame(descriptor, validCompletionObservation())
            != GpuFrameValidation::MismatchedDimensions)
        {
            return fail("all color/depth surfaces must use identical eye dimensions");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        descriptor.leftDepth.format = GpuInteropFormat::R8G8B8A8Unorm;
        if (validateSharedStereoFrame(descriptor, validCompletionObservation())
            != GpuFrameValidation::InvalidDepthFormat)
        {
            return fail("depth must use the declared D3D9/D3D11 interop encoding");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        descriptor.depthEncoding = GpuDepthEncoding::Unknown;
        if (validateSharedStereoFrame(descriptor, validCompletionObservation())
            != GpuFrameValidation::InvalidDepthEncoding)
        {
            return fail("encoded depth requires one explicit canonical semantic");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        descriptor.depthNearMeters = 0.0f;
        if (validateSharedStereoFrame(descriptor, validCompletionObservation())
            != GpuFrameValidation::InvalidDepthRange)
        {
            return fail("encoded depth requires a positive metric near plane");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        descriptor.depthFarMeters = descriptor.depthNearMeters;
        if (validateSharedStereoFrame(descriptor, validCompletionObservation())
            != GpuFrameValidation::InvalidDepthRange)
        {
            return fail("encoded depth far plane must be strictly beyond the near plane");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        descriptor.depthFarMeters = (std::numeric_limits<float>::quiet_NaN)();
        if (validateSharedStereoFrame(descriptor, validCompletionObservation())
            != GpuFrameValidation::InvalidDepthRange)
        {
            return fail("encoded depth range must reject non-finite values");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        const GpuCompletionObservation notObserved {};
        if (validateSharedStereoFrame(descriptor, notObserved)
            != GpuFrameValidation::GpuWorkIncomplete)
        {
            return fail("producer completion metadata alone must not prove consumer GPU readiness");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        GpuCompletionObservation observation = validCompletionObservation();
        observation.consumerObservedReached = false;
        observation.reachedSequence = descriptor.gpuReadySequence;
        if (validateSharedStereoFrame(descriptor, observation)
            != GpuFrameValidation::GpuWorkIncomplete)
        {
            return fail("a fence value is not evidence unless the consumer reports observing it");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        GpuCompletionObservation observation = validCompletionObservation();
        observation.reachedSequence--;
        if (validateSharedStereoFrame(descriptor, observation)
            != GpuFrameValidation::GpuWorkIncomplete)
        {
            return fail("consumer observation below the declared ready sequence must be rejected");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        GpuCompletionObservation observation = validCompletionObservation();
        observation.primitive = GpuCompletionPrimitive::Unknown;
        if (validateSharedStereoFrame(descriptor, observation)
            != GpuFrameValidation::GpuWorkIncomplete)
        {
            return fail("consumer must observe the exact declared completion primitive");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        GpuCompletionObservation observation = validCompletionObservation();
        observation.declaredCompletionHandle++;
        if (validateSharedStereoFrame(descriptor, observation)
            != GpuFrameValidation::GpuWorkIncomplete)
        {
            return fail("completion observation must identify the declared shared primitive");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        descriptor.gpuCompletionHandle = 0;
        if (validateSharedStereoFrame(descriptor, validCompletionObservation())
            != GpuFrameValidation::InvalidCompletionMetadata)
        {
            return fail("completion primitive requires an explicit shared handle");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        descriptor.gpuReadySequence = 0;
        if (validateSharedStereoFrame(descriptor, validCompletionObservation())
            != GpuFrameValidation::InvalidCompletionMetadata)
        {
            return fail("completion primitive requires a non-zero declared ready sequence");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        descriptor.gpuConsumerReleaseSequence = 0;
        if (validateSharedStereoFrame(descriptor, validCompletionObservation())
            != GpuFrameValidation::InvalidConsumerReleaseMetadata)
        {
            return fail("continuous transport requires an explicit consumer-release sequence");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        descriptor.gpuConsumerReleaseSequence = descriptor.gpuReadySequence + 2;
        if (validateSharedStereoFrame(descriptor, validCompletionObservation())
            != GpuFrameValidation::InvalidConsumerReleaseMetadata)
        {
            return fail("consumer-release sequence must be exactly the next fence value");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        descriptor.gpuReadySequence = (std::numeric_limits<std::uint64_t>::max)();
        descriptor.gpuConsumerReleaseSequence = 1;
        if (validateSharedStereoFrame(descriptor, validCompletionObservation())
            != GpuFrameValidation::InvalidConsumerReleaseMetadata)
        {
            return fail("consumer-release sequence must fail closed instead of wrapping");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        descriptor.resourceSetId = 0;
        if (validateSharedStereoFrame(descriptor, validCompletionObservation())
            != GpuFrameValidation::MissingResourceOwnership)
        {
            return fail("reusable eye resources require a non-zero resource-set identity");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        GpuCompletionObservation observation = validCompletionObservation();
        ++observation.resourceSetId;
        if (validateSharedStereoFrame(descriptor, observation)
            != GpuFrameValidation::GpuWorkIncomplete)
        {
            return fail("producer-ready evidence must bind the exact resource set");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        if (validateResourceSetReleasedForProducerReuse(
                descriptor,
                validConsumerReleaseObservation()) != GpuFrameValidation::Valid)
        {
            return fail("producer may reuse a resource set only after observing its exact release value");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        GpuConsumerReleaseObservation observation = validConsumerReleaseObservation();
        observation.reachedSequence = descriptor.gpuReadySequence;
        if (validateResourceSetReleasedForProducerReuse(descriptor, observation)
            != GpuFrameValidation::ConsumerOwnershipNotReleased)
        {
            return fail("producer-ready completion alone must not authorize resource reuse");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        GpuConsumerReleaseObservation observation = validConsumerReleaseObservation();
        observation.producerObservedReached = false;
        if (validateResourceSetReleasedForProducerReuse(descriptor, observation)
            != GpuFrameValidation::ConsumerOwnershipNotReleased)
        {
            return fail("consumer release metadata is not evidence until the producer observes the fence");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        GpuConsumerReleaseObservation observation = validConsumerReleaseObservation();
        ++observation.resourceSetId;
        if (validateResourceSetReleasedForProducerReuse(descriptor, observation)
            != GpuFrameValidation::ConsumerOwnershipNotReleased)
        {
            return fail("a release observation for another resource set must not authorize reuse");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        GpuConsumerReleaseObservation observation = validConsumerReleaseObservation();
        ++observation.runtimeStateSample;
        if (validateResourceSetReleasedForProducerReuse(descriptor, observation)
            != GpuFrameValidation::ConsumerOwnershipNotReleased)
        {
            return fail("consumer release must bind the exact published transaction lineage");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        descriptor.adapterLuid = 0;
        if (validateSharedStereoFrame(descriptor, validCompletionObservation())
            != GpuFrameValidation::MissingIdentity)
        {
            return fail("cross-process textures require an explicit adapter identity");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        SharedStereoFrameDescriptor snapshot {};
        populateValidDescriptor(descriptor);
        if (!readStableSharedStereoFrameSnapshot(&descriptor, snapshot))
            return fail("stable even publication must produce a local snapshot");
        if (snapshot.transactionId != descriptor.transactionId
            || snapshot.publicationSequence.load(std::memory_order_relaxed) != 2u)
        {
            return fail("stable snapshot must contain the guarded descriptor payload");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        SharedStereoFrameDescriptor snapshot {};
        populateValidDescriptor(descriptor);
        descriptor.publicationSequence.store(3u, std::memory_order_release);
        if (readStableSharedStereoFrameSnapshot(&descriptor, snapshot, 1))
            return fail("odd writer-owned publication must be rejected");
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        SharedStereoFrameDescriptor snapshot {};
        populateValidDescriptor(descriptor);
        const bool accepted = detail::readStableSharedStereoFrameSnapshotWithCopy(
            &descriptor,
            snapshot,
            1,
            [&](SharedStereoFrameDescriptor& destination,
                const SharedStereoFrameDescriptor& source) {
                detail::copySharedStereoFramePayload(destination, source);
                descriptor.publicationSequence.store(4u, std::memory_order_release);
            });
        if (accepted)
            return fail("publication sequence change across the copy must be rejected");
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        SharedStereoFrameDescriptor snapshot {};
        bool callbackObservedWriterOwnership = false;
        const bool published = publishSharedStereoFrame(
            &descriptor,
            [&](SharedStereoFramePayload& writable) {
                callbackObservedWriterOwnership =
                    (descriptor.publicationSequence.load(std::memory_order_seq_cst) & 1u) != 0u;
                populateValidDescriptorPayload(writable);
                return true;
            });
        if (!published || !callbackObservedWriterOwnership)
            return fail("producer callback must run only while the sequence is writer-owned and odd");
        if (descriptor.publicationSequence.load(std::memory_order_seq_cst) != 2u)
            return fail("successful publication must commit one non-zero even sequence");
        if (!readStableSharedStereoFrameSnapshot(&descriptor, snapshot))
            return fail("fully ordered producer publication must produce a stable snapshot");
        if (snapshot.transactionId != descriptor.transactionId)
            return fail("producer helper must publish the callback payload atomically");
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        SharedStereoFrameDescriptor snapshot {};
        if (!publishSharedStereoFrame(
                &descriptor,
                [](SharedStereoFramePayload& writable) {
                    populateValidDescriptorPayload(writable);
                    return true;
                }))
        {
            return fail("stale-payload fixture must publish one complete baseline");
        }

        const bool partialAccepted = publishSharedStereoFrame(
            &descriptor,
            [](SharedStereoFramePayload& writable) {
                writable.transactionId = 901;
                writable.sourceFrame = 902;
                writable.poseSequence = 903;
                return true;
            });
        if (partialAccepted
            || (descriptor.publicationSequence.load(std::memory_order_seq_cst) & 1u) == 0u
            || readStableSharedStereoFrameSnapshot(&descriptor, snapshot, 1))
        {
            return fail("a partial later producer payload must not inherit and republish stale fields");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        SharedStereoFrameDescriptor snapshot {};
        const bool published = publishSharedStereoFrame(
            &descriptor,
            [](SharedStereoFramePayload& writable) {
                writable.transactionId = 9001;
                return false;
            });
        if (published)
            return fail("producer callback failure must fail publication closed");
        if ((descriptor.publicationSequence.load(std::memory_order_seq_cst) & 1u) == 0u)
            return fail("failed producer callback must leave the sequence odd");
        if (readStableSharedStereoFrameSnapshot(&descriptor, snapshot, 1))
            return fail("snapshot must reject a publication abandoned by its producer");
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        descriptor.publicationSequence.store(3u, std::memory_order_seq_cst);
        bool callbackInvoked = false;
        if (publishSharedStereoFrame(
                &descriptor,
                [&](SharedStereoFramePayload&) {
                    callbackInvoked = true;
                    return true;
                }))
        {
            return fail("a second producer must not acquire an odd writer-owned sequence");
        }
        if (callbackInvoked)
            return fail("producer callback must not run without exclusive sequence ownership");
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        SharedStereoFrameDescriptor snapshot {};
        populateValidDescriptor(descriptor);
        if (!readStableSharedStereoFrameSnapshot(&descriptor, snapshot, 1))
            return fail("snapshot invalidation fixture must first capture a valid publication");

        descriptor.publicationSequence.store(3u, std::memory_order_release);
        if (readStableSharedStereoFrameSnapshot(&descriptor, snapshot, 1))
            return fail("odd current publication must fail the second snapshot read");
        if (snapshot.publicationSequence.load(std::memory_order_relaxed) != 0u
            || validateSharedStereoFrame(snapshot, validCompletionObservation())
                != GpuFrameValidation::UnstablePublication)
        {
            return fail("failed snapshot read must invalidate a caller's prior valid output");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        const GpuCompletionObservation priorEpochObservation = validCompletionObservation();
        ++descriptor.producerEpoch;
        descriptor.gpuReadySequence = 1;
        descriptor.gpuConsumerReleaseSequence = 2;
        if (validateSharedStereoFrame(descriptor, priorEpochObservation)
            != GpuFrameValidation::GpuWorkIncomplete)
        {
            return fail("cached fence observation must not survive producer epoch and handle reuse");
        }
    }

    {
        SharedStereoFrameDescriptor descriptor {};
        populateValidDescriptor(descriptor);
        GpuCompletionObservation otherRuntimeSample = validCompletionObservation();
        ++otherRuntimeSample.runtimeStateSample;
        if (validateSharedStereoFrame(descriptor, otherRuntimeSample)
            != GpuFrameValidation::GpuWorkIncomplete)
        {
            return fail("GPU completion evidence must match the exact runtime-state sample");
        }
    }

    std::cout << "fnvxr GPU frame transport contract PASS\n";
    return EXIT_SUCCESS;
}
