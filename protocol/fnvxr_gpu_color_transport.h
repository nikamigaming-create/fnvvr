#pragma once

#include "fnvxr_gpu_frame_transport.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace fnvxr::gpu::color_v5
{
// ABI v5 is the production stereo-color path. The D3D9 depth surfaces remain
// private render resources; a correct left/right engine render does not need
// fabricated depth textures in the OpenXR process.
inline constexpr std::uint32_t SharedStereoColorMagic = 0x43585646u; // FVXC
inline constexpr std::uint32_t SharedStereoColorVersion = 5u;
inline constexpr char SharedStereoColorMappingName[] =
    "Local\\FNVXR_GPU_StereoColor_v5";
inline constexpr std::uint32_t MaximumTextureDimension = 16384u;

enum class PresentationMode : std::uint32_t
{
    Unknown = 0u,
    BinocularWorld = 1u,
    MonoUiQuad = 2u,
};

enum class Validation : std::uint32_t
{
    Valid = 0u,
    InvalidHeader,
    UnstablePublication,
    MissingIdentity,
    InvalidCompletionMetadata,
    InvalidConsumerReleaseMetadata,
    MissingResourceOwnership,
    InvalidPresentationMode,
    MissingResource,
    InvalidSharedHandleType,
    InvalidColorFormat,
    MismatchedColorFormats,
    MismatchedDimensions,
    AliasedEyeResources,
    GpuWorkIncomplete,
    ConsumerOwnershipNotReleased,
};

struct alignas(8) SharedStereoColorDescriptor
{
    std::uint32_t magic = 0u;
    std::uint32_t version = 0u;
    std::uint32_t descriptorBytes = 0u;
    std::atomic<std::uint32_t> publicationSequence { 0u };
    std::uint64_t gpuCompletionHandle = 0u;
    std::uint64_t gpuReadySequence = 0u;
    std::uint64_t gpuConsumerReleaseSequence = 0u;
    GpuCompletionPrimitive gpuCompletionPrimitive =
        GpuCompletionPrimitive::Unknown;
    PresentationMode presentationMode = PresentationMode::Unknown;
    std::uint64_t resourceSetId = 0u;
    std::uint64_t producerEpoch = 0u;
    std::uint32_t producerProcessId = 0u;
    std::uint32_t reserved = 0u;
    std::uint64_t adapterLuid = 0u;
    std::uint64_t transactionId = 0u;
    std::uint64_t sourceFrame = 0u;
    std::uint64_t poseSequence = 0u;
    std::uint64_t runtimeStateSample = 0u;
    std::int64_t renderedDisplayTime = 0;
    GpuTextureDescriptor leftColor {};
    GpuTextureDescriptor rightColor {};
};

// Process-local staging deliberately has no atomic sequence member. Alignment
// leaves the same four bytes unused at offset 12, so all guarded payload fields
// retain the exact shared-layout offsets.
struct alignas(8) SharedStereoColorPayload
{
    std::uint32_t magic = 0u;
    std::uint32_t version = 0u;
    std::uint32_t descriptorBytes = 0u;
    std::uint64_t gpuCompletionHandle = 0u;
    std::uint64_t gpuReadySequence = 0u;
    std::uint64_t gpuConsumerReleaseSequence = 0u;
    GpuCompletionPrimitive gpuCompletionPrimitive =
        GpuCompletionPrimitive::Unknown;
    PresentationMode presentationMode = PresentationMode::Unknown;
    std::uint64_t resourceSetId = 0u;
    std::uint64_t producerEpoch = 0u;
    std::uint32_t producerProcessId = 0u;
    std::uint32_t reserved = 0u;
    std::uint64_t adapterLuid = 0u;
    std::uint64_t transactionId = 0u;
    std::uint64_t sourceFrame = 0u;
    std::uint64_t poseSequence = 0u;
    std::uint64_t runtimeStateSample = 0u;
    std::int64_t renderedDisplayTime = 0;
    GpuTextureDescriptor leftColor {};
    GpuTextureDescriptor rightColor {};
};

static_assert(sizeof(std::atomic<std::uint32_t>) == 4u);
static_assert(alignof(std::atomic<std::uint32_t>) == 4u);
static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
static_assert(std::is_standard_layout_v<SharedStereoColorDescriptor>);
static_assert(std::is_standard_layout_v<SharedStereoColorPayload>);
static_assert(sizeof(SharedStereoColorDescriptor) == 168u);
static_assert(sizeof(SharedStereoColorPayload) == 168u);
static_assert(alignof(SharedStereoColorDescriptor) == 8u);
static_assert(offsetof(SharedStereoColorDescriptor, publicationSequence) == 12u);
static_assert(offsetof(SharedStereoColorDescriptor, gpuCompletionHandle) == 16u);
static_assert(offsetof(SharedStereoColorDescriptor, presentationMode) == 44u);
static_assert(offsetof(SharedStereoColorDescriptor, resourceSetId) == 48u);
static_assert(offsetof(SharedStereoColorDescriptor, adapterLuid) == 72u);
static_assert(offsetof(SharedStereoColorDescriptor, leftColor) == 120u);
static_assert(offsetof(SharedStereoColorDescriptor, rightColor) == 144u);
static_assert(
    offsetof(SharedStereoColorPayload, gpuCompletionHandle)
    == offsetof(SharedStereoColorDescriptor, gpuCompletionHandle));
static_assert(
    offsetof(SharedStereoColorPayload, rightColor)
    == offsetof(SharedStereoColorDescriptor, rightColor));

constexpr bool validPresentationMode(PresentationMode mode) noexcept
{
    return mode == PresentationMode::BinocularWorld
        || mode == PresentationMode::MonoUiQuad;
}

template <typename Frame>
inline Validation validatePayloadMetadata(const Frame& value) noexcept
{
    if (value.magic != SharedStereoColorMagic
        || value.version != SharedStereoColorVersion
        || value.descriptorBytes != sizeof(SharedStereoColorDescriptor))
    {
        return Validation::InvalidHeader;
    }
    if (value.producerEpoch == 0u
        || value.producerProcessId == 0u
        || value.adapterLuid == 0u
        || value.transactionId == 0u
        || value.sourceFrame == 0u
        || value.poseSequence == 0u
        || value.runtimeStateSample == 0u
        || value.renderedDisplayTime == 0)
    {
        return Validation::MissingIdentity;
    }
    if (value.gpuCompletionHandle == 0u
        || value.gpuReadySequence == 0u
        || !validCompletionPrimitive(value.gpuCompletionPrimitive))
    {
        return Validation::InvalidCompletionMetadata;
    }
    if (value.gpuConsumerReleaseSequence == 0u
        || value.gpuReadySequence
            == (std::numeric_limits<std::uint64_t>::max)()
        || value.gpuConsumerReleaseSequence != value.gpuReadySequence + 1u)
    {
        return Validation::InvalidConsumerReleaseMetadata;
    }
    if (value.resourceSetId == 0u)
        return Validation::MissingResourceOwnership;
    if (!validPresentationMode(value.presentationMode))
        return Validation::InvalidPresentationMode;

    const GpuTextureDescriptor* colors[] = {
        &value.leftColor,
        &value.rightColor,
    };
    for (const GpuTextureDescriptor* color : colors)
    {
        if (color->sharedHandle == 0u
            || color->width == 0u
            || color->height == 0u)
        {
            return Validation::MissingResource;
        }
        if (!validSharedHandleType(color->handleType))
            return Validation::InvalidSharedHandleType;
        if (!validColorFormat(color->format))
            return Validation::InvalidColorFormat;
        if (color->width > MaximumTextureDimension
            || color->height > MaximumTextureDimension)
        {
            return Validation::MismatchedDimensions;
        }
    }
    if (value.leftColor.format != value.rightColor.format)
        return Validation::MismatchedColorFormats;
    if (value.leftColor.width != value.rightColor.width
        || value.leftColor.height != value.rightColor.height)
    {
        return Validation::MismatchedDimensions;
    }
    if (value.leftColor.sharedHandle == value.rightColor.sharedHandle)
        return Validation::AliasedEyeResources;
    return Validation::Valid;
}

namespace detail
{
inline void copyPayloadToShared(
    SharedStereoColorDescriptor& destination,
    const SharedStereoColorPayload& source) noexcept
{
    constexpr std::size_t sequenceOffset =
        offsetof(SharedStereoColorDescriptor, publicationSequence);
    constexpr std::size_t guardedOffset =
        offsetof(SharedStereoColorDescriptor, gpuCompletionHandle);
    std::memcpy(&destination, &source, sequenceOffset);
    std::memcpy(
        reinterpret_cast<unsigned char*>(&destination) + guardedOffset,
        reinterpret_cast<const unsigned char*>(&source) + guardedOffset,
        sizeof(SharedStereoColorDescriptor) - guardedOffset);
}

inline void copySharedPayload(
    SharedStereoColorDescriptor& destination,
    const SharedStereoColorDescriptor& source) noexcept
{
    constexpr std::size_t sequenceOffset =
        offsetof(SharedStereoColorDescriptor, publicationSequence);
    constexpr std::size_t guardedOffset =
        sequenceOffset + sizeof(std::atomic<std::uint32_t>);
    std::memcpy(&destination, &source, sequenceOffset);
    std::memcpy(
        reinterpret_cast<unsigned char*>(&destination) + guardedOffset,
        reinterpret_cast<const unsigned char*>(&source) + guardedOffset,
        sizeof(SharedStereoColorDescriptor) - guardedOffset);
}
}

template <typename WritePayload>
inline bool publish(
    SharedStereoColorDescriptor* shared,
    WritePayload&& writePayload)
{
    if (!shared)
        return false;
    std::uint32_t observed =
        shared->publicationSequence.load(std::memory_order_seq_cst);
    if ((observed & 1u) != 0u
        || observed >= (std::numeric_limits<std::uint32_t>::max)() - 1u)
    {
        return false;
    }
    const std::uint32_t writerOwned = observed + 1u;
    if (!shared->publicationSequence.compare_exchange_strong(
            observed,
            writerOwned,
            std::memory_order_seq_cst,
            std::memory_order_seq_cst))
    {
        return false;
    }

    SharedStereoColorPayload payload {};
    std::atomic_thread_fence(std::memory_order_seq_cst);
    bool complete = false;
    try
    {
        complete = writePayload(payload);
    }
    catch (...)
    {
        complete = false;
    }
    std::atomic_thread_fence(std::memory_order_seq_cst);
    if (!complete || validatePayloadMetadata(payload) != Validation::Valid)
        return false;

    detail::copyPayloadToShared(*shared, payload);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    shared->publicationSequence.store(
        writerOwned + 1u,
        std::memory_order_seq_cst);
    return true;
}

inline bool readStableSnapshot(
    const SharedStereoColorDescriptor* shared,
    SharedStereoColorDescriptor& snapshot,
    int attempts = 4) noexcept
{
    snapshot.publicationSequence.store(0u, std::memory_order_relaxed);
    if (!shared || shared == &snapshot || attempts <= 0)
        return false;
    for (int attempt = 0; attempt < attempts; ++attempt)
    {
        const std::uint32_t before =
            shared->publicationSequence.load(std::memory_order_acquire);
        if (before == 0u || (before & 1u) != 0u)
            continue;
        detail::copySharedPayload(snapshot, *shared);
        std::atomic_thread_fence(std::memory_order_acquire);
        const std::uint32_t after =
            shared->publicationSequence.load(std::memory_order_acquire);
        if (before == after && (after & 1u) == 0u)
        {
            snapshot.publicationSequence.store(after, std::memory_order_relaxed);
            return true;
        }
    }
    return false;
}

struct CompletionObservation
{
    GpuCompletionPrimitive primitive = GpuCompletionPrimitive::Unknown;
    std::uint64_t declaredCompletionHandle = 0u;
    std::uint64_t reachedSequence = 0u;
    std::uint64_t resourceSetId = 0u;
    std::uint64_t producerEpoch = 0u;
    std::uint32_t producerProcessId = 0u;
    std::uint64_t adapterLuid = 0u;
    std::uint64_t transactionId = 0u;
    std::uint64_t sourceFrame = 0u;
    std::uint64_t poseSequence = 0u;
    std::uint64_t runtimeStateSample = 0u;
    bool consumerObservedReached = false;
};

inline bool observationMatches(
    const SharedStereoColorDescriptor& value,
    const CompletionObservation& observation) noexcept
{
    return observation.primitive == value.gpuCompletionPrimitive
        && observation.declaredCompletionHandle == value.gpuCompletionHandle
        && observation.resourceSetId == value.resourceSetId
        && observation.producerEpoch == value.producerEpoch
        && observation.producerProcessId == value.producerProcessId
        && observation.adapterLuid == value.adapterLuid
        && observation.transactionId == value.transactionId
        && observation.sourceFrame == value.sourceFrame
        && observation.poseSequence == value.poseSequence
        && observation.runtimeStateSample == value.runtimeStateSample;
}

inline Validation validateReady(
    const SharedStereoColorDescriptor& value,
    const CompletionObservation& observation) noexcept
{
    const std::uint32_t sequence =
        value.publicationSequence.load(std::memory_order_relaxed);
    if (sequence == 0u || (sequence & 1u) != 0u)
        return Validation::UnstablePublication;
    const Validation metadata = validatePayloadMetadata(value);
    if (metadata != Validation::Valid)
        return metadata;
    if (!observation.consumerObservedReached
        || !observationMatches(value, observation)
        || observation.reachedSequence < value.gpuReadySequence)
    {
        return Validation::GpuWorkIncomplete;
    }
    return Validation::Valid;
}

inline Validation validateReleasedForReuse(
    const SharedStereoColorDescriptor& value,
    const CompletionObservation& observation) noexcept
{
    const std::uint32_t sequence =
        value.publicationSequence.load(std::memory_order_relaxed);
    if (sequence == 0u || (sequence & 1u) != 0u)
        return Validation::UnstablePublication;
    const Validation metadata = validatePayloadMetadata(value);
    if (metadata != Validation::Valid)
        return metadata;
    if (!observation.consumerObservedReached
        || !observationMatches(value, observation)
        || observation.reachedSequence
            < value.gpuConsumerReleaseSequence)
    {
        return Validation::ConsumerOwnershipNotReleased;
    }
    return Validation::Valid;
}
}
