#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace fnvxr::gpu
{
inline constexpr std::uint32_t SharedStereoFrameMagic = 0x47585646u; // FNVG
inline constexpr std::uint32_t SharedStereoFrameVersion = 4;
inline constexpr char SharedStereoFrameMappingName[] =
    "Local\\FNVXR_GPU_StereoFrame_v4";

// These are portable contract values, not raw DXGI enum ordinals. The backend
// maps them explicitly after checking adapter support.
enum class GpuInteropFormat : std::uint32_t
{
    Unknown = 0,
    R8G8B8A8Unorm = 1,
    R10G10B10A2Unorm = 2,
    R16G16B16A16Float = 3,
};

// Every texture handle in ABI v4 is an NT handle created from an ID3D11Texture2D
// carrying D3D11_RESOURCE_MISC_SHARED_NTHANDLE. The producer creates it with
// IDXGIResource1::CreateSharedHandle and keeps the source HANDLE alive for the
// publication lifetime; the consumer duplicates it from producerProcessId and
// opens the duplicate with ID3D11Device1::OpenSharedResource1. Legacy KMT
// handles and OpenSharedResource are not interchangeable with this contract.
enum class GpuSharedHandleType : std::uint32_t
{
    Unknown = 0,
    D3D11NtHandle = 1,
};

// Depth is transported in a shareable floating-point color texture because a
// native D3D9 depth surface cannot be opened by the D3D11 consumer. Version 4
// deliberately has one canonical encoding: R is normalized linear view-space
// depth in meters, G/B are zero, and A is one for covered geometry. The clear
// value is R=1, G=0, B=0, A=0 so background remains distinguishable from valid
// geometry at the far plane.
enum class GpuDepthEncoding : std::uint32_t
{
    Unknown = 0,
    LinearViewDepthMetersNormalized = 1,
};

inline constexpr float CanonicalEncodedDepthMin = 0.0f;
inline constexpr float CanonicalEncodedDepthMax = 1.0f;
inline constexpr float CanonicalEncodedDepthBackgroundR =
    CanonicalEncodedDepthMax;
inline constexpr float CanonicalEncodedDepthBackgroundA = 0.0f;

inline bool validLinearViewDepthRange(float nearMeters, float farMeters)
{
    return std::isfinite(nearMeters)
        && std::isfinite(farMeters)
        && nearMeters > 0.0f
        && farMeters > nearMeters;
}

inline float encodeLinearViewDepthMeters(
    float viewDepthMeters,
    float nearMeters,
    float farMeters)
{
    if (!std::isfinite(viewDepthMeters)
        || !validLinearViewDepthRange(nearMeters, farMeters))
    {
        return (std::numeric_limits<float>::quiet_NaN)();
    }

    return std::clamp(
        (viewDepthMeters - nearMeters) / (farMeters - nearMeters),
        CanonicalEncodedDepthMin,
        CanonicalEncodedDepthMax);
}

inline float decodeLinearViewDepthMeters(
    float encodedDepth,
    float nearMeters,
    float farMeters)
{
    if (!std::isfinite(encodedDepth)
        || !validLinearViewDepthRange(nearMeters, farMeters))
    {
        return (std::numeric_limits<float>::quiet_NaN)();
    }

    const float normalized = std::clamp(
        encodedDepth,
        CanonicalEncodedDepthMin,
        CanonicalEncodedDepthMax);
    return nearMeters + normalized * (farMeters - nearMeters);
}

// Version 4 deliberately defines one bidirectional ownership mechanism and
// binds each GPU resource set to the exact classified runtime-state sample. A
// producer publishes its D3D11 shared-fence NT HANDLE value and signals the
// ready value only after all eye work completes. The consumer uses
// producerProcessId to duplicate that handle into its own process before
// calling OpenSharedFence; the producer keeps the source handle alive while
// the publication can be consumed. The consumer signals the immediately
// following release value only after its GPU no longer references those
// textures (or after copying them into private textures). The producer must
// observe release before writing that resource set again.
//
// A fence interval is exclusive to one resourceSetId. A ring of reusable sets
// therefore needs an independent fence timeline per set, or strict serialized
// use that prevents a later ready signal from passing an unobserved release.
// Adding another mechanism is an ABI revision, not an interpretation of these
// fields.
enum class GpuCompletionPrimitive : std::uint32_t
{
    Unknown = 0,
    D3D11SharedFence = 1,
};

struct alignas(8) GpuTextureDescriptor
{
    std::uint64_t sharedHandle = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    GpuInteropFormat format = GpuInteropFormat::Unknown;
    GpuSharedHandleType handleType = GpuSharedHandleType::Unknown;
};

// This mapping contains metadata only. Eye pixels stay in GPU-owned shared
// textures; adding a CPU image array here is an architecture violation.
//
// publicationSequence is a process-shared seqlock. Producers must publish only
// through publishSharedStereoFrame below; direct payload writes are forbidden.
// That helper owns an odd sequence for the entire write and commits a non-zero
// even sequence only after the callback reports a complete descriptor.
//
// This is an explicit Windows/MSVC shared-memory contract: the mapping owner
// constructs this object before sharing it, every participant uses the same
// ABI, and the sequence uses lock-free 32-bit atomics plus sequentially
// consistent operations and fences. The C++ abstract machine does not itself
// define object synchronization between processes. The ABI assertions below
// reject a standard library where the required atomic representation differs.
struct alignas(8) SharedStereoFrameDescriptor
{
    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    std::uint32_t descriptorBytes = 0;
    std::atomic<std::uint32_t> publicationSequence { 0 };
    std::uint64_t gpuCompletionHandle = 0;
    std::uint64_t gpuReadySequence = 0;
    std::uint64_t gpuConsumerReleaseSequence = 0;
    GpuCompletionPrimitive gpuCompletionPrimitive = GpuCompletionPrimitive::Unknown;
    std::uint32_t completionReserved = 0;
    std::uint64_t resourceSetId = 0;
    std::uint64_t producerEpoch = 0;
    std::uint32_t producerProcessId = 0;
    std::uint32_t reserved = 0;
    std::uint64_t adapterLuid = 0;
    std::uint64_t transactionId = 0;
    std::uint64_t sourceFrame = 0;
    std::uint64_t poseSequence = 0;
    std::uint64_t runtimeStateSample = 0;
    std::int64_t renderedDisplayTime = 0;
    // These planes are the exact metric endpoints used by the canonical
    // encoder. A consumer decodes from them; it must not infer replacement
    // values from its own projection configuration.
    GpuDepthEncoding depthEncoding = GpuDepthEncoding::Unknown;
    std::uint32_t depthReserved = 0;
    float depthNearMeters = 0.0f;
    float depthFarMeters = 0.0f;
    GpuTextureDescriptor leftColor {};
    GpuTextureDescriptor rightColor {};
    GpuTextureDescriptor leftDepth {};
    GpuTextureDescriptor rightDepth {};
};

// Process-local producer staging. Its layout mirrors every guarded mapping
// field but deliberately has no publicationSequence member. A producer fills
// a zero-initialized instance, so an omitted field cannot inherit bytes from a
// prior shared publication and the callback cannot corrupt writer ownership.
struct alignas(8) SharedStereoFramePayload
{
    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    std::uint32_t descriptorBytes = 0;
    std::uint64_t gpuCompletionHandle = 0;
    std::uint64_t gpuReadySequence = 0;
    std::uint64_t gpuConsumerReleaseSequence = 0;
    GpuCompletionPrimitive gpuCompletionPrimitive = GpuCompletionPrimitive::Unknown;
    std::uint32_t completionReserved = 0;
    std::uint64_t resourceSetId = 0;
    std::uint64_t producerEpoch = 0;
    std::uint32_t producerProcessId = 0;
    std::uint32_t reserved = 0;
    std::uint64_t adapterLuid = 0;
    std::uint64_t transactionId = 0;
    std::uint64_t sourceFrame = 0;
    std::uint64_t poseSequence = 0;
    std::uint64_t runtimeStateSample = 0;
    std::int64_t renderedDisplayTime = 0;
    GpuDepthEncoding depthEncoding = GpuDepthEncoding::Unknown;
    std::uint32_t depthReserved = 0;
    float depthNearMeters = 0.0f;
    float depthFarMeters = 0.0f;
    GpuTextureDescriptor leftColor {};
    GpuTextureDescriptor rightColor {};
    GpuTextureDescriptor leftDepth {};
    GpuTextureDescriptor rightDepth {};
};

static_assert(sizeof(std::atomic<std::uint32_t>) == 4u,
    "shared publication sequence must have a four-byte representation");
static_assert(alignof(std::atomic<std::uint32_t>) == 4u,
    "shared publication sequence must have four-byte alignment");
static_assert(std::atomic<std::uint32_t>::is_always_lock_free,
    "shared publication sequence cannot use a process-local atomic lock");

static_assert(std::is_standard_layout<GpuTextureDescriptor>::value,
    "GPU texture descriptor must remain standard-layout");
static_assert(sizeof(GpuTextureDescriptor) == 24u,
    "GPU texture descriptor ABI must remain exactly 24 bytes");
static_assert(alignof(GpuTextureDescriptor) == 8u,
    "GPU texture descriptor ABI must remain eight-byte aligned");
static_assert(offsetof(GpuTextureDescriptor, sharedHandle) == 0u,
    "GPU texture sharedHandle offset changed");
static_assert(offsetof(GpuTextureDescriptor, width) == 8u,
    "GPU texture width offset changed");
static_assert(offsetof(GpuTextureDescriptor, height) == 12u,
    "GPU texture height offset changed");
static_assert(offsetof(GpuTextureDescriptor, format) == 16u,
    "GPU texture format offset changed");
static_assert(offsetof(GpuTextureDescriptor, handleType) == 20u,
    "GPU texture shared-handle type offset changed");

static_assert(std::is_standard_layout<SharedStereoFrameDescriptor>::value,
    "GPU transport descriptor must remain standard-layout");
static_assert(sizeof(SharedStereoFrameDescriptor) == 232u,
    "GPU transport descriptor ABI must remain exactly 232 bytes");
static_assert(alignof(SharedStereoFrameDescriptor) == 8u,
    "GPU transport descriptor ABI must remain eight-byte aligned");
static_assert(offsetof(SharedStereoFrameDescriptor, magic) == 0u,
    "GPU transport magic offset changed");
static_assert(offsetof(SharedStereoFrameDescriptor, version) == 4u,
    "GPU transport version offset changed");
static_assert(offsetof(SharedStereoFrameDescriptor, descriptorBytes) == 8u,
    "GPU transport descriptorBytes offset changed");
static_assert(offsetof(SharedStereoFrameDescriptor, publicationSequence) == 12u,
    "GPU transport publicationSequence offset changed");
static_assert(offsetof(SharedStereoFrameDescriptor, gpuCompletionHandle) == 16u,
    "GPU transport completion handle offset changed");
static_assert(offsetof(SharedStereoFrameDescriptor, gpuReadySequence) == 24u,
    "GPU transport completion sequence offset changed");
static_assert(offsetof(SharedStereoFrameDescriptor, gpuConsumerReleaseSequence) == 32u,
    "GPU transport consumer release sequence offset changed");
static_assert(offsetof(SharedStereoFrameDescriptor, gpuCompletionPrimitive) == 40u,
    "GPU transport completion primitive offset changed");
static_assert(offsetof(SharedStereoFrameDescriptor, resourceSetId) == 48u,
    "GPU transport resource set offset changed");
static_assert(offsetof(SharedStereoFrameDescriptor, producerEpoch) == 56u,
    "GPU transport producer epoch offset changed");
static_assert(offsetof(SharedStereoFrameDescriptor, producerProcessId) == 64u,
    "GPU transport producer PID offset changed");
static_assert(offsetof(SharedStereoFrameDescriptor, adapterLuid) == 72u,
    "GPU transport adapter LUID offset changed");
static_assert(offsetof(SharedStereoFrameDescriptor, transactionId) == 80u,
    "GPU transport transaction ID offset changed");
static_assert(offsetof(SharedStereoFrameDescriptor, sourceFrame) == 88u,
    "GPU transport source frame offset changed");
static_assert(offsetof(SharedStereoFrameDescriptor, poseSequence) == 96u,
    "GPU transport pose sequence offset changed");
static_assert(offsetof(SharedStereoFrameDescriptor, runtimeStateSample) == 104u,
    "GPU transport runtime-state sample offset changed");
static_assert(offsetof(SharedStereoFrameDescriptor, renderedDisplayTime) == 112u,
    "GPU transport display time offset changed");
static_assert(offsetof(SharedStereoFrameDescriptor, depthEncoding) == 120u,
    "GPU transport depth encoding offset changed");
static_assert(offsetof(SharedStereoFrameDescriptor, depthReserved) == 124u,
    "GPU transport depth reserved offset changed");
static_assert(offsetof(SharedStereoFrameDescriptor, depthNearMeters) == 128u,
    "GPU transport depth near offset changed");
static_assert(offsetof(SharedStereoFrameDescriptor, depthFarMeters) == 132u,
    "GPU transport depth far offset changed");
static_assert(offsetof(SharedStereoFrameDescriptor, leftColor) == 136u,
    "GPU transport left color offset changed");
static_assert(offsetof(SharedStereoFrameDescriptor, rightColor) == 160u,
    "GPU transport right color offset changed");
static_assert(offsetof(SharedStereoFrameDescriptor, leftDepth) == 184u,
    "GPU transport left depth offset changed");
static_assert(offsetof(SharedStereoFrameDescriptor, rightDepth) == 208u,
    "GPU transport right depth offset changed");

static_assert(std::is_standard_layout<SharedStereoFramePayload>::value,
    "GPU producer payload must remain standard-layout");
static_assert(sizeof(SharedStereoFramePayload) == sizeof(SharedStereoFrameDescriptor),
    "GPU producer payload must mirror the shared descriptor byte layout");
static_assert(alignof(SharedStereoFramePayload) == 8u,
    "GPU producer payload must remain eight-byte aligned");
static_assert(offsetof(SharedStereoFramePayload, magic) == 0u,
    "GPU producer payload magic offset changed");
static_assert(offsetof(SharedStereoFramePayload, descriptorBytes) == 8u,
    "GPU producer payload header offset changed");
static_assert(offsetof(SharedStereoFramePayload, gpuCompletionHandle) == 16u,
    "GPU producer payload completion offset changed");
static_assert(offsetof(SharedStereoFramePayload, gpuConsumerReleaseSequence) == 32u,
    "GPU producer payload consumer release offset changed");
static_assert(offsetof(SharedStereoFramePayload, resourceSetId) == 48u,
    "GPU producer payload resource set offset changed");
static_assert(offsetof(SharedStereoFramePayload, depthEncoding) == 120u,
    "GPU producer payload depth encoding offset changed");
static_assert(offsetof(SharedStereoFramePayload, rightDepth) == 208u,
    "GPU producer payload resource offsets changed");

#define FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(field) \
    static_assert( \
        offsetof(SharedStereoFramePayload, field) \
            == offsetof(SharedStereoFrameDescriptor, field), \
        "GPU local/shared payload offset mismatch: " #field)
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(magic);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(version);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(descriptorBytes);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(gpuCompletionHandle);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(gpuReadySequence);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(gpuConsumerReleaseSequence);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(gpuCompletionPrimitive);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(completionReserved);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(resourceSetId);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(producerEpoch);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(producerProcessId);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(reserved);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(adapterLuid);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(transactionId);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(sourceFrame);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(poseSequence);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(runtimeStateSample);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(renderedDisplayTime);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(depthEncoding);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(depthReserved);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(depthNearMeters);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(depthFarMeters);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(leftColor);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(rightColor);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(leftDepth);
FNVXR_GPU_ASSERT_PAYLOAD_OFFSET(rightDepth);
#undef FNVXR_GPU_ASSERT_PAYLOAD_OFFSET

enum class GpuFrameValidation : std::uint32_t
{
    Valid = 0,
    InvalidHeader = 1,
    UnstablePublication = 2,
    MissingIdentity = 3,
    MissingResource = 4,
    InvalidColorFormat = 5,
    InvalidDepthFormat = 6,
    MismatchedDimensions = 7,
    AliasedEyeResources = 8,
    GpuWorkIncomplete = 9,
    MismatchedColorFormats = 10,
    InvalidCompletionMetadata = 11,
    InvalidDepthEncoding = 12,
    InvalidDepthRange = 13,
    InvalidConsumerReleaseMetadata = 14,
    MissingResourceOwnership = 15,
    ConsumerOwnershipNotReleased = 16,
    InvalidSharedHandleType = 17,
};

inline bool validColorFormat(GpuInteropFormat format)
{
    return format == GpuInteropFormat::R8G8B8A8Unorm
        || format == GpuInteropFormat::R10G10B10A2Unorm
        || format == GpuInteropFormat::R16G16B16A16Float;
}

inline bool validEncodedDepthFormat(GpuInteropFormat format)
{
    // D3D9-to-D3D11 shared textures cannot expose a native D3D9 depth format
    // through OpenSharedResource. Depth is linearized into the R channel of a
    // shareable floating-point color texture by the engine renderer backend.
    return format == GpuInteropFormat::R16G16B16A16Float;
}

inline bool validSharedHandleType(GpuSharedHandleType handleType)
{
    return handleType == GpuSharedHandleType::D3D11NtHandle;
}

inline bool validDepthEncoding(GpuDepthEncoding encoding)
{
    return encoding == GpuDepthEncoding::LinearViewDepthMetersNormalized;
}

inline bool validCompletionPrimitive(GpuCompletionPrimitive primitive)
{
    return primitive == GpuCompletionPrimitive::D3D11SharedFence;
}

template <typename FrameLike>
inline GpuFrameValidation validateStereoFramePayloadMetadata(
    const FrameLike& value)
{
    if (value.magic != SharedStereoFrameMagic
        || value.version != SharedStereoFrameVersion
        || value.descriptorBytes != sizeof(SharedStereoFrameDescriptor))
    {
        return GpuFrameValidation::InvalidHeader;
    }

    if (value.producerEpoch == 0
        || value.producerProcessId == 0
        || value.adapterLuid == 0
        || value.transactionId == 0
        || value.sourceFrame == 0
        || value.poseSequence == 0
        || value.runtimeStateSample == 0
        || value.renderedDisplayTime == 0)
    {
        return GpuFrameValidation::MissingIdentity;
    }

    if (value.gpuCompletionHandle == 0
        || value.gpuReadySequence == 0
        || !validCompletionPrimitive(value.gpuCompletionPrimitive))
    {
        return GpuFrameValidation::InvalidCompletionMetadata;
    }

    if (value.gpuConsumerReleaseSequence == 0
        || value.gpuReadySequence
            == (std::numeric_limits<std::uint64_t>::max)()
        || value.gpuConsumerReleaseSequence != value.gpuReadySequence + 1u)
    {
        return GpuFrameValidation::InvalidConsumerReleaseMetadata;
    }

    if (value.resourceSetId == 0)
        return GpuFrameValidation::MissingResourceOwnership;

    if (!validDepthEncoding(value.depthEncoding))
        return GpuFrameValidation::InvalidDepthEncoding;

    if (!validLinearViewDepthRange(
            value.depthNearMeters,
            value.depthFarMeters))
    {
        return GpuFrameValidation::InvalidDepthRange;
    }

    const GpuTextureDescriptor* resources[] = {
        &value.leftColor,
        &value.rightColor,
        &value.leftDepth,
        &value.rightDepth,
    };
    for (const GpuTextureDescriptor* resource : resources)
    {
        if (resource->sharedHandle == 0
            || resource->width == 0
            || resource->height == 0)
        {
            return GpuFrameValidation::MissingResource;
        }
        if (!validSharedHandleType(resource->handleType))
            return GpuFrameValidation::InvalidSharedHandleType;
    }

    if (!validColorFormat(value.leftColor.format)
        || !validColorFormat(value.rightColor.format))
    {
        return GpuFrameValidation::InvalidColorFormat;
    }
    if (value.leftColor.format != value.rightColor.format)
        return GpuFrameValidation::MismatchedColorFormats;

    if (!validEncodedDepthFormat(value.leftDepth.format)
        || !validEncodedDepthFormat(value.rightDepth.format))
    {
        return GpuFrameValidation::InvalidDepthFormat;
    }

    const std::uint32_t width = value.leftColor.width;
    const std::uint32_t height = value.leftColor.height;
    for (const GpuTextureDescriptor* resource : resources)
    {
        if (resource->width != width || resource->height != height)
            return GpuFrameValidation::MismatchedDimensions;
    }

    for (std::size_t left = 0; left < 4; ++left)
    {
        for (std::size_t right = left + 1; right < 4; ++right)
        {
            if (resources[left]->sharedHandle == resources[right]->sharedHandle)
                return GpuFrameValidation::AliasedEyeResources;
        }
    }

    return GpuFrameValidation::Valid;
}

namespace detail
{
inline void copyProducerPayloadToShared(
    SharedStereoFrameDescriptor& destination,
    const SharedStereoFramePayload& source) noexcept
{
    constexpr std::size_t sequenceOffset =
        offsetof(SharedStereoFrameDescriptor, publicationSequence);
    constexpr std::size_t sharedPayloadOffset =
        offsetof(SharedStereoFrameDescriptor, gpuCompletionHandle);
    constexpr std::size_t localPayloadOffset =
        offsetof(SharedStereoFramePayload, gpuCompletionHandle);
    static_assert(sharedPayloadOffset == localPayloadOffset,
        "local/shared GPU payload offsets must match");

    std::memcpy(&destination, &source, sequenceOffset);
    std::memcpy(
        reinterpret_cast<unsigned char*>(&destination) + sharedPayloadOffset,
        reinterpret_cast<const unsigned char*>(&source) + localPayloadOffset,
        sizeof(SharedStereoFrameDescriptor) - sharedPayloadOffset);
}
}

// This is the only supported producer publication path. writePayload runs
// after exclusive writer ownership is visible as an odd sequence, but receives
// only a zero-initialized process-local payload with no sequence member. It
// must return true only when every field is ready; the helper independently
// validates that payload before copying all guarded bytes. Returning false,
// throwing, or producing incomplete metadata abandons the mapping with an odd
// sequence, so every consumer fails closed.
// A producer must replace/reinitialize that mapping before attempting another
// publication. Full sequentially consistent barriers deliberately prevent
// payload stores from escaping the odd/even publication interval on MSVC.
template <typename WritePayload>
inline bool publishSharedStereoFrame(
    SharedStereoFrameDescriptor* shared,
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

    SharedStereoFramePayload payload {};
    std::atomic_thread_fence(std::memory_order_seq_cst);
    const bool payloadComplete = writePayload(payload);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    if (!payloadComplete
        || validateStereoFramePayloadMetadata(payload) != GpuFrameValidation::Valid)
    {
        return false;
    }

    detail::copyProducerPayloadToShared(*shared, payload);
    std::atomic_thread_fence(std::memory_order_seq_cst);

    shared->publicationSequence.store(
        writerOwned + 1u,
        std::memory_order_seq_cst);
    return true;
}

namespace detail
{
// Copy every byte except the atomic sequence itself. The sequence is written
// to the local snapshot only after matching acquire loads prove that the
// guarded bytes came from one completed publication.
inline void copySharedStereoFramePayload(
    SharedStereoFrameDescriptor& destination,
    const SharedStereoFrameDescriptor& source) noexcept
{
    constexpr std::size_t sequenceOffset =
        offsetof(SharedStereoFrameDescriptor, publicationSequence);
    constexpr std::size_t payloadOffset =
        sequenceOffset + sizeof(std::atomic<std::uint32_t>);

    std::memcpy(&destination, &source, sequenceOffset);
    std::memcpy(
        reinterpret_cast<unsigned char*>(&destination) + payloadOffset,
        reinterpret_cast<const unsigned char*>(&source) + payloadOffset,
        sizeof(SharedStereoFrameDescriptor) - payloadOffset);
}

template <typename CopyPayload>
inline bool readStableSharedStereoFrameSnapshotWithCopy(
    const SharedStereoFrameDescriptor* shared,
    SharedStereoFrameDescriptor& snapshot,
    int attempts,
    CopyPayload&& copyPayload)
{
    // A failed read must revoke any older snapshot held by the caller. Leaving
    // its prior even sequence intact would allow stale bytes to validate after
    // the current mapping became odd, torn, or unavailable.
    snapshot.publicationSequence.store(0u, std::memory_order_relaxed);
    if (!shared || shared == &snapshot || attempts <= 0)
        return false;

    for (int attempt = 0; attempt < attempts; ++attempt)
    {
        const std::uint32_t sequenceBefore =
            shared->publicationSequence.load(std::memory_order_acquire);
        if (sequenceBefore == 0u || (sequenceBefore & 1u) != 0u)
            continue;

        copyPayload(snapshot, *shared);

        // Keep all payload reads before the validating sequence load. This is
        // in addition to, not a substitute for, the required acquire load.
        std::atomic_thread_fence(std::memory_order_acquire);
        const std::uint32_t sequenceAfter =
            shared->publicationSequence.load(std::memory_order_acquire);
        if (sequenceBefore == sequenceAfter && (sequenceAfter & 1u) == 0u)
        {
            snapshot.publicationSequence.store(
                sequenceAfter,
                std::memory_order_relaxed);
            return true;
        }
    }
    return false;
}
}

// Produces a process-local descriptor only when one complete, non-zero even
// publication guards the entire copy. Validation must operate on this local
// snapshot, never directly on a concurrently writable mapping.
inline bool readStableSharedStereoFrameSnapshot(
    const SharedStereoFrameDescriptor* shared,
    SharedStereoFrameDescriptor& snapshot,
    int attempts = 4)
{
    return detail::readStableSharedStereoFrameSnapshotWithCopy(
        shared,
        snapshot,
        attempts,
        [](SharedStereoFrameDescriptor& destination,
            const SharedStereoFrameDescriptor& source) noexcept {
            detail::copySharedStereoFramePayload(destination, source);
        });
}

// This is consumer-side evidence, not producer metadata. A caller may set
// consumerObservedReached only after it successfully duplicated/opened the
// declared primitive and observed that primitive reach reachedSequence.
// declaredCompletionHandle records the producer-process handle identity from
// the descriptor, not the consumer's local duplicate. Merely copying
// gpuReadySequence from the descriptor is explicitly insufficient.
struct GpuCompletionObservation
{
    GpuCompletionPrimitive primitive = GpuCompletionPrimitive::Unknown;
    std::uint64_t declaredCompletionHandle = 0;
    std::uint64_t reachedSequence = 0;
    std::uint64_t resourceSetId = 0;
    std::uint64_t producerEpoch = 0;
    std::uint32_t producerProcessId = 0;
    std::uint64_t adapterLuid = 0;
    std::uint64_t transactionId = 0;
    std::uint64_t sourceFrame = 0;
    std::uint64_t poseSequence = 0;
    std::uint64_t runtimeStateSample = 0;
    bool consumerObservedReached = false;
};

inline GpuFrameValidation validateSharedStereoFrame(
    const SharedStereoFrameDescriptor& value,
    const GpuCompletionObservation& completionObservation)
{
    if (value.magic != SharedStereoFrameMagic
        || value.version != SharedStereoFrameVersion
        || value.descriptorBytes != sizeof(SharedStereoFrameDescriptor))
    {
        return GpuFrameValidation::InvalidHeader;
    }

    const std::uint32_t publicationSequence =
        value.publicationSequence.load(std::memory_order_relaxed);
    if (publicationSequence == 0u || (publicationSequence & 1u) != 0u)
        return GpuFrameValidation::UnstablePublication;

    const GpuFrameValidation metadata = validateStereoFramePayloadMetadata(value);
    if (metadata != GpuFrameValidation::Valid)
        return metadata;

    if (!completionObservation.consumerObservedReached
        || completionObservation.primitive != value.gpuCompletionPrimitive
        || completionObservation.declaredCompletionHandle != value.gpuCompletionHandle
        || completionObservation.resourceSetId != value.resourceSetId
        || completionObservation.producerEpoch != value.producerEpoch
        || completionObservation.producerProcessId != value.producerProcessId
        || completionObservation.adapterLuid != value.adapterLuid
        || completionObservation.transactionId != value.transactionId
        || completionObservation.sourceFrame != value.sourceFrame
        || completionObservation.poseSequence != value.poseSequence
        || completionObservation.runtimeStateSample != value.runtimeStateSample
        || completionObservation.reachedSequence < value.gpuReadySequence)
    {
        return GpuFrameValidation::GpuWorkIncomplete;
    }

    return GpuFrameValidation::Valid;
}

// Producer-side evidence that the consumer has returned ownership of one
// reusable resource set. The consumer must signal gpuConsumerReleaseSequence
// only after every GPU command that reads the published textures has completed
// (or after a completed copy to private consumer textures). The producer may
// call this validator only after it has independently observed that shared
// fence reach reachedSequence; descriptor metadata alone never proves release.
struct GpuConsumerReleaseObservation
{
    GpuCompletionPrimitive primitive = GpuCompletionPrimitive::Unknown;
    std::uint64_t declaredCompletionHandle = 0;
    std::uint64_t reachedSequence = 0;
    std::uint64_t resourceSetId = 0;
    std::uint64_t producerEpoch = 0;
    std::uint32_t producerProcessId = 0;
    std::uint64_t adapterLuid = 0;
    std::uint64_t transactionId = 0;
    std::uint64_t sourceFrame = 0;
    std::uint64_t poseSequence = 0;
    std::uint64_t runtimeStateSample = 0;
    bool producerObservedReached = false;
};

inline GpuFrameValidation validateResourceSetReleasedForProducerReuse(
    const SharedStereoFrameDescriptor& value,
    const GpuConsumerReleaseObservation& releaseObservation)
{
    if (value.magic != SharedStereoFrameMagic
        || value.version != SharedStereoFrameVersion
        || value.descriptorBytes != sizeof(SharedStereoFrameDescriptor))
    {
        return GpuFrameValidation::InvalidHeader;
    }

    const std::uint32_t publicationSequence =
        value.publicationSequence.load(std::memory_order_relaxed);
    if (publicationSequence == 0u || (publicationSequence & 1u) != 0u)
        return GpuFrameValidation::UnstablePublication;

    const GpuFrameValidation metadata = validateStereoFramePayloadMetadata(value);
    if (metadata != GpuFrameValidation::Valid)
        return metadata;

    if (!releaseObservation.producerObservedReached
        || releaseObservation.primitive != value.gpuCompletionPrimitive
        || releaseObservation.declaredCompletionHandle != value.gpuCompletionHandle
        || releaseObservation.resourceSetId != value.resourceSetId
        || releaseObservation.producerEpoch != value.producerEpoch
        || releaseObservation.producerProcessId != value.producerProcessId
        || releaseObservation.adapterLuid != value.adapterLuid
        || releaseObservation.transactionId != value.transactionId
        || releaseObservation.sourceFrame != value.sourceFrame
        || releaseObservation.poseSequence != value.poseSequence
        || releaseObservation.runtimeStateSample != value.runtimeStateSample
        || releaseObservation.reachedSequence
            < value.gpuConsumerReleaseSequence)
    {
        return GpuFrameValidation::ConsumerOwnershipNotReleased;
    }

    return GpuFrameValidation::Valid;
}
}
