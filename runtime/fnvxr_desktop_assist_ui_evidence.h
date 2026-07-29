#pragma once

#include "../protocol/fnvxr_shared_state.h"

#include <cstddef>
#include <cstdint>

namespace fnvxr::engine
{
// D3DFMT_A8R8G8B8 and D3DFMT_X8R8G8B8 are the only CPU pixel layouts the
// desktop-assist UI producer accepts. Keep their wire values in the protocol
// boundary so a reader never treats an arbitrary four-byte surface as proof.
constexpr LONG DesktopAssistUiQuadPixelFormatA8R8G8B8 = 21;
constexpr LONG DesktopAssistUiQuadPixelFormatX8R8G8B8 = 22;

constexpr std::uint32_t DesktopAssistUiQuadRequiredFlags =
    shared::DesktopAssistUiQuadFlagLeaseCurrent
    | shared::DesktopAssistUiQuadFlagPresentHookInstalled
    | shared::DesktopAssistUiQuadFlagRuntimeUiConfirmed
    | shared::DesktopAssistUiQuadFlagPixelCopyComplete
    | shared::DesktopAssistUiQuadFlagPixelContentNonBlack
    | shared::DesktopAssistUiQuadFlagPoseEpochCurrent;

constexpr std::size_t desktopAssistUiQuadMappingBytes() noexcept
{
    return sizeof(shared::SharedDesktopAssistUiQuadHeader)
        + static_cast<std::size_t>(shared::D3D9SharedFrameMaxWidth)
            * static_cast<std::size_t>(shared::D3D9SharedFrameMaxHeight)
            * 4u;
}

constexpr bool desktopAssistUiQuadPixelFormatAccepted(LONG format) noexcept
{
    return format == DesktopAssistUiQuadPixelFormatA8R8G8B8
        || format == DesktopAssistUiQuadPixelFormatX8R8G8B8;
}

constexpr bool desktopAssistUiQuadRuntimeConfirmedUi(
    const shared::SharedDesktopAssistUiQuadHeader& header) noexcept
{
    return header.runtimePhase == shared::RuntimePhaseMenu
        || header.runtimePhase == shared::RuntimePhaseLoading
        || (header.runtimeMenuBits
                & (shared::RuntimeInteractiveMenuBits
                    | shared::RuntimeLoadingMenuBit))
            != 0u;
}

inline bool desktopAssistUiQuadPayloadLayoutIsValid(
    const shared::SharedDesktopAssistUiQuadHeader& header,
    std::size_t* payloadBytes = nullptr) noexcept
{
    if (header.width <= 0
        || header.height <= 0
        || header.width > static_cast<LONG>(shared::D3D9SharedFrameMaxWidth)
        || header.height > static_cast<LONG>(shared::D3D9SharedFrameMaxHeight)
        || header.pitchBytes != header.width * 4
        || !desktopAssistUiQuadPixelFormatAccepted(header.format))
    {
        return false;
    }

    const std::size_t rowBytes = static_cast<std::size_t>(header.pitchBytes);
    const std::size_t rows = static_cast<std::size_t>(header.height);
    const std::size_t bytes = rowBytes * rows;
    if (header.headerBytes != sizeof(header)
        || bytes > desktopAssistUiQuadMappingBytes() - sizeof(header))
    {
        return false;
    }
    if (payloadBytes)
        *payloadBytes = bytes;
    return true;
}

inline bool desktopAssistUiQuadHeaderIsComplete(
    const shared::SharedDesktopAssistUiQuadHeader& header) noexcept
{
    return header.writing == 0
        && shared::sequencedValueIsPublished(header.sequence)
        && header.magic == shared::DesktopAssistUiQuadSharedMagic
        && header.version == shared::DesktopAssistUiQuadSharedVersion
        && (header.flags & DesktopAssistUiQuadRequiredFlags)
            == DesktopAssistUiQuadRequiredFlags
        && header.runtimeStateSample != 0u
        && header.poseFrame != 0u
        && shared::sequencedValueIsPublished(header.poseSequence)
        && desktopAssistUiQuadRuntimeConfirmedUi(header)
        && header.captureFailure == 0u
        && header.nonBlackSampleCount != 0u
        && header.poseProducerEpoch != 0u
        && header.captureOrdinal != 0u
        && desktopAssistUiQuadPayloadLayoutIsValid(header);
}

inline std::uint32_t desktopAssistUiQuadPixelHash(
    const std::uint8_t* pixels,
    std::size_t pixelBytes,
    std::uint32_t* nonBlackSampleCount = nullptr) noexcept
{
    std::uint32_t hash = 2166136261u;
    std::uint32_t nonBlack = 0u;
    if (!pixels || pixelBytes == 0u || pixelBytes % 4u != 0u)
    {
        if (nonBlackSampleCount)
            *nonBlackSampleCount = 0u;
        return 0u;
    }

    for (std::size_t offset = 0u; offset < pixelBytes; offset += 4u)
    {
        if (pixels[offset] != 0u
            || pixels[offset + 1u] != 0u
            || pixels[offset + 2u] != 0u)
        {
            ++nonBlack;
        }
        for (std::size_t channel = 0u; channel < 4u; ++channel)
        {
            hash ^= pixels[offset + channel];
            hash *= 16777619u;
        }
    }
    if (nonBlackSampleCount)
        *nonBlackSampleCount = nonBlack;
    return hash;
}
}
