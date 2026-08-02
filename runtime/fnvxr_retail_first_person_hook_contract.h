#pragma once

#include "fnvxr_retail_engine_abi.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace fnvxr::engine
{
// The three direct RenderFirstPerson callers and one internal persistent-list
// finalizer are patched.  They were independently observed in two loaded
// FalloutNV.exe 1.4.0.525 processes; every instruction is an exact five-byte
// E8 to the fully revalidated retail target.  This intentionally excludes
// entry detours and shared render helpers.  The internal call is the one
// persistent first-person accumulator render/finalize site at +0x4E4 in
// RenderFirstPerson; it is what lets the two private eye calls render the
// stock list without consuming it between eyes.
inline constexpr std::size_t RetailFirstPersonCallPatchByteCount = 5u;

inline constexpr std::uintptr_t RetailFirstPersonPreparedRenderAndFinalizeAddress =
    0x00B6C0D0u;

enum class RetailFirstPersonCallRelayKind : std::uint8_t
{
    RenderFirstPerson = 0u,
    RenderPreparedAccumulator,
};

// Keep the three audited outer callers distinguishable all the way through
// the local-E8 lease.  A common relay made it impossible to tell which stock
// path actually consumed the first-person work, so a visually plausible
// result could not be attributed to a specific caller.
enum class RetailFirstPersonCallerId : std::uint32_t
{
    Unknown = 0u,
    Primary = 1u,
    Alternate = 2u,
    Third = 3u,
};

struct RetailFirstPersonCallSiteContract
{
    const char* name = nullptr;
    RetailFirstPersonCallRelayKind relayKind =
        RetailFirstPersonCallRelayKind::RenderFirstPerson;
    std::uintptr_t preferredCallAddress = 0u;
    std::array<std::uint8_t, RetailFirstPersonCallPatchByteCount> bytes {};
    std::uintptr_t preferredTargetAddress = 0u;
    std::uint32_t independentLoadedProcessSamples = 0u;
};

inline constexpr std::array<RetailFirstPersonCallSiteContract, 4>
    RetailFirstPersonCallSiteContractInventory {{
        // Install this inner relay first. Until an outer RenderFirstPerson
        // caller is patched it is a gate-closed, byte-for-byte stock B6C0D0
        // call; after an outer call is patched, a private eye can never reach
        // an unprepared finalizer.
        {
            "persistent first-person accumulator finalizer",
            RetailFirstPersonCallRelayKind::RenderPreparedAccumulator,
            0x008755F4u,
            {{ 0xE8u, 0xD7u, 0x6Au, 0x2Fu, 0x00u }},
            RetailFirstPersonPreparedRenderAndFinalizeAddress,
            2u,
        },
        {
            "primary first-person caller",
            RetailFirstPersonCallRelayKind::RenderFirstPerson,
            0x0087093Du,
            {{ 0xE8u, 0xCEu, 0x47u, 0x00u, 0x00u }},
            FirstPersonRenderAddress,
            2u,
        },
        {
            "alternate first-person caller",
            RetailFirstPersonCallRelayKind::RenderFirstPerson,
            0x00870B21u,
            {{ 0xE8u, 0xEAu, 0x45u, 0x00u, 0x00u }},
            FirstPersonRenderAddress,
            2u,
        },
        {
            "third first-person caller",
            RetailFirstPersonCallRelayKind::RenderFirstPerson,
            0x00870F74u,
            {{ 0xE8u, 0x97u, 0x41u, 0x00u, 0x00u }},
            FirstPersonRenderAddress,
            2u,
        },
    }};

constexpr std::uintptr_t retailFirstPersonExpectedTargetAddress(
    RetailFirstPersonCallRelayKind relayKind) noexcept
{
    switch (relayKind)
    {
    case RetailFirstPersonCallRelayKind::RenderFirstPerson:
        return FirstPersonRenderAddress;
    case RetailFirstPersonCallRelayKind::RenderPreparedAccumulator:
        return RetailFirstPersonPreparedRenderAndFinalizeAddress;
    }
    return 0u;
}

constexpr bool retailFirstPersonCheckedAdd(
    std::uintptr_t value,
    std::uintptr_t increment,
    std::uintptr_t& result) noexcept
{
    if (value > (std::numeric_limits<std::uintptr_t>::max)() - increment)
        return false;
    result = value + increment;
    return true;
}

constexpr bool relocateRetailFirstPersonPreferredAddress(
    std::uintptr_t runtimeImageBase,
    std::uintptr_t preferredAddress,
    std::uintptr_t& result) noexcept
{
    result = 0u;
    if (runtimeImageBase == 0u
        || preferredAddress < SupportedImageBase
        || preferredAddress >= SupportedImageBase + SupportedSizeOfImage)
    {
        return false;
    }
    return retailFirstPersonCheckedAdd(
        runtimeImageBase,
        preferredAddress - SupportedImageBase,
        result);
}

constexpr std::int64_t retailFirstPersonSignedRel32(
    std::uint32_t raw) noexcept
{
    return raw <= 0x7FFFFFFFu
        ? static_cast<std::int64_t>(raw)
        : -static_cast<std::int64_t>((~raw) + 1u);
}

inline bool decodeRetailFirstPersonRel32Target(
    const std::uint8_t* instruction,
    std::size_t instructionByteCount,
    std::uintptr_t instructionAddress,
    std::uintptr_t& targetAddress) noexcept
{
    targetAddress = 0u;
    if (!instruction
        || instructionByteCount < RetailFirstPersonCallPatchByteCount
        || instruction[0] != 0xE8u)
    {
        return false;
    }
    const std::uint32_t raw = static_cast<std::uint32_t>(instruction[1])
        | (static_cast<std::uint32_t>(instruction[2]) << 8u)
        | (static_cast<std::uint32_t>(instruction[3]) << 16u)
        | (static_cast<std::uint32_t>(instruction[4]) << 24u);
    std::uintptr_t instructionEnd = 0u;
    if (!retailFirstPersonCheckedAdd(
            instructionAddress,
            RetailFirstPersonCallPatchByteCount,
            instructionEnd))
    {
        return false;
    }
    const std::int64_t displacement = retailFirstPersonSignedRel32(raw);
    if (displacement >= 0)
    {
        return retailFirstPersonCheckedAdd(
            instructionEnd,
            static_cast<std::uintptr_t>(displacement),
            targetAddress);
    }
    const std::uintptr_t distance = static_cast<std::uintptr_t>(-displacement);
    if (instructionEnd < distance)
        return false;
    targetAddress = instructionEnd - distance;
    return true;
}

inline bool retailFirstPersonBytesMatch(
    const std::uint8_t* actual,
    const std::uint8_t* expected,
    std::size_t byteCount) noexcept
{
    if (!actual || !expected)
        return false;
    std::uint8_t difference = 0u;
    for (std::size_t index = 0u; index < byteCount; ++index)
        difference |= static_cast<std::uint8_t>(actual[index] ^ expected[index]);
    return difference == 0u;
}

struct RetailFirstPersonCallSiteObservation
{
    bool bytesReadable = false;
    bool exactInstructionMatches = false;
    bool callAddressMatches = false;
    bool targetDecoded = false;
    std::uintptr_t targetAddress = 0u;
    bool targetMatches = false;

    constexpr bool complete() const noexcept
    {
        return bytesReadable
            && exactInstructionMatches
            && callAddressMatches
            && targetDecoded
            && targetMatches;
    }
};

inline RetailFirstPersonCallSiteObservation observeRetailFirstPersonCallSite(
    const std::uint8_t* bytes,
    std::size_t byteCount,
    std::uintptr_t runtimeCallAddress,
    std::uintptr_t runtimeImageBase,
    const RetailFirstPersonCallSiteContract& contract) noexcept
{
    RetailFirstPersonCallSiteObservation result {};
    result.bytesReadable = bytes
        && byteCount >= RetailFirstPersonCallPatchByteCount;
    if (!result.bytesReadable)
        return result;

    result.exactInstructionMatches = retailFirstPersonBytesMatch(
        bytes,
        contract.bytes.data(),
        RetailFirstPersonCallPatchByteCount);
    std::uintptr_t expectedCallAddress = 0u;
    result.callAddressMatches = relocateRetailFirstPersonPreferredAddress(
            runtimeImageBase,
            contract.preferredCallAddress,
            expectedCallAddress)
        && runtimeCallAddress == expectedCallAddress;
    result.targetDecoded = decodeRetailFirstPersonRel32Target(
        bytes,
        byteCount,
        runtimeCallAddress,
        result.targetAddress);
    std::uintptr_t expectedTargetAddress = 0u;
    result.targetMatches = result.targetDecoded
        && relocateRetailFirstPersonPreferredAddress(
            runtimeImageBase,
            contract.preferredTargetAddress,
            expectedTargetAddress)
        && result.targetAddress == expectedTargetAddress;
    return result;
}

struct RetailFirstPersonRelativeCall
{
    bool valid = false;
    std::array<std::uint8_t, RetailFirstPersonCallPatchByteCount> bytes {};
};

inline RetailFirstPersonRelativeCall encodeRetailFirstPersonX86Call(
    std::uintptr_t instructionAddress,
    std::uintptr_t targetAddress) noexcept
{
    RetailFirstPersonRelativeCall result {};
    if (instructionAddress == 0u
        || targetAddress == 0u
        || instructionAddress > 0xFFFFFFFFu
        || targetAddress > 0xFFFFFFFFu)
    {
        return result;
    }
    const std::uint32_t source = static_cast<std::uint32_t>(instructionAddress);
    const std::uint32_t target = static_cast<std::uint32_t>(targetAddress);
    const std::uint32_t displacement = target
        - (source + static_cast<std::uint32_t>(
            RetailFirstPersonCallPatchByteCount));
    result.valid = true;
    result.bytes = {{
        0xE8u,
        static_cast<std::uint8_t>(displacement & 0xFFu),
        static_cast<std::uint8_t>((displacement >> 8u) & 0xFFu),
        static_cast<std::uint8_t>((displacement >> 16u) & 0xFFu),
        static_cast<std::uint8_t>((displacement >> 24u) & 0xFFu),
    }};
    return result;
}

constexpr bool retailFirstPersonCallSiteContractInventoryProductionProven()
    noexcept
{
    for (const RetailFirstPersonCallSiteContract& contract
         : RetailFirstPersonCallSiteContractInventory)
    {
        if (!contract.name || contract.name[0] == '\0'
            || contract.preferredCallAddress < SupportedImageBase
            || contract.preferredCallAddress >= SupportedImageBase + SupportedSizeOfImage
            || contract.bytes[0] != 0xE8u
            || contract.preferredTargetAddress
                != retailFirstPersonExpectedTargetAddress(contract.relayKind)
            || contract.independentLoadedProcessSamples < 2u)
        {
            return false;
        }
    }
    return true;
}

static_assert(retailFirstPersonCallSiteContractInventoryProductionProven());
}
