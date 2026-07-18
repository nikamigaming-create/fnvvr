#pragma once

#include <cstddef>
#include <cstdint>

namespace fnvxr::gpu
{
inline constexpr std::uint32_t SharedStereoFrameMagic = 0x47585646u; // FNVG
inline constexpr std::uint32_t SharedStereoFrameVersion = 1;
inline constexpr char SharedStereoFrameMappingName[] =
    "Local\\FNVXR_GPU_StereoFrame_v1";

// These are the portable contract values, not raw DXGI enum ordinals. The
// backend maps them explicitly after checking adapter support.
enum class GpuInteropFormat : std::uint32_t
{
    Unknown = 0,
    R8G8B8A8Unorm = 1,
    R10G10B10A2Unorm = 2,
    R16G16B16A16Float = 3,
};

struct GpuTextureDescriptor
{
    std::uint64_t sharedHandle = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    GpuInteropFormat format = GpuInteropFormat::Unknown;
};

// This mapping contains metadata only. Eye pixels stay in GPU-owned shared
// textures; adding a CPU image array here is an architecture violation.
struct SharedStereoFrameDescriptor
{
    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    std::uint32_t descriptorBytes = 0;
    std::uint32_t publicationSequence = 0;
    std::uint64_t gpuReadySequence = 0;
    std::uint64_t producerEpoch = 0;
    std::uint32_t producerProcessId = 0;
    std::uint32_t reserved = 0;
    std::uint64_t adapterLuid = 0;
    std::uint64_t transactionId = 0;
    std::uint64_t sourceFrame = 0;
    std::uint64_t poseSequence = 0;
    std::int64_t renderedDisplayTime = 0;
    GpuTextureDescriptor leftColor {};
    GpuTextureDescriptor rightColor {};
    GpuTextureDescriptor leftDepth {};
    GpuTextureDescriptor rightDepth {};
};

static_assert(
    sizeof(SharedStereoFrameDescriptor) <= 256,
    "GPU transport descriptor must remain metadata-only");

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

inline GpuFrameValidation validateSharedStereoFrame(
    const SharedStereoFrameDescriptor& value)
{
    if (value.magic != SharedStereoFrameMagic
        || value.version != SharedStereoFrameVersion
        || value.descriptorBytes != sizeof(SharedStereoFrameDescriptor))
    {
        return GpuFrameValidation::InvalidHeader;
    }

    if (value.publicationSequence == 0u
        || (value.publicationSequence & 1u) != 0u)
    {
        return GpuFrameValidation::UnstablePublication;
    }

    if (value.producerEpoch == 0
        || value.producerProcessId == 0
        || value.adapterLuid == 0
        || value.transactionId == 0
        || value.sourceFrame == 0
        || value.poseSequence == 0
        || value.renderedDisplayTime == 0)
    {
        return GpuFrameValidation::MissingIdentity;
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
    }

    if (!validColorFormat(value.leftColor.format)
        || !validColorFormat(value.rightColor.format))
    {
        return GpuFrameValidation::InvalidColorFormat;
    }

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

    if (value.gpuReadySequence == 0)
        return GpuFrameValidation::GpuWorkIncomplete;

    return GpuFrameValidation::Valid;
}
}
