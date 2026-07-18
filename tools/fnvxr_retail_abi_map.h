#pragma once

#include "fnvxr_retail_engine_abi.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace fnvxr::probe::abi
{
inline constexpr std::uintptr_t RenderWorldPreferredAddress = 0x00873200u;
inline constexpr std::size_t RenderWorldFunctionBytes = 4698u;

enum class RenderWorldContext : std::uint8_t
{
    Setup,
    SelectorAlternate,
    SelectorNormal,
    SelectorCommon,
    AuxiliaryPass0,
    AuxiliaryPass1,
};

constexpr const char* contextName(RenderWorldContext context) noexcept
{
    switch (context)
    {
        case RenderWorldContext::Setup:
            return "setup";
        case RenderWorldContext::SelectorAlternate:
            return "selector_alternate";
        case RenderWorldContext::SelectorNormal:
            return "selector_normal";
        case RenderWorldContext::SelectorCommon:
            return "selector_common";
        case RenderWorldContext::AuxiliaryPass0:
            return "auxiliary_pass_0";
        case RenderWorldContext::AuxiliaryPass1:
            return "auxiliary_pass_1";
    }
    return "unknown";
}

struct RenderWorldDirectCall
{
    const char* name = nullptr;
    std::size_t functionOffset = 0;
    std::uintptr_t preferredTargetAddress = 0;
    RenderWorldContext context = RenderWorldContext::Setup;
};

// These are instruction boundaries from the exact 4698-byte
// RenderWorldSceneGraph body in RetailEngineManifest. They are evidence for
// diagnostics only; matching them does not authorize patching or calling the
// functions. In particular, the two B6BFC0 calls and the B6BEE0 call are the
// mutually exclusive stock accumulation strategies selected earlier in the
// function. The later pairs are auxiliary passes, not selector alternatives.
inline constexpr std::array<RenderWorldDirectCall, 10>
    RenderWorldDirectCallContract {{
        {
            "BSCullingProcess::ctor",
            0x0E6Du,
            0x004A0EB0u,
            RenderWorldContext::Setup,
        },
        {
            "AlternateAccumulateScene[0]",
            0x0ECEu,
            0x00B6BFC0u,
            RenderWorldContext::SelectorAlternate,
        },
        {
            "AlternateAccumulateScene[1]",
            0x0EF2u,
            0x00B6BFC0u,
            RenderWorldContext::SelectorAlternate,
        },
        {
            "AccumulateScene",
            0x0F5Bu,
            0x00B6BEE0u,
            RenderWorldContext::SelectorNormal,
        },
        {
            "RenderAccumulatorWithoutFinalize",
            0x0F80u,
            0x00B6BA20u,
            RenderWorldContext::SelectorCommon,
        },
        {
            "FinalizeAccumulator",
            0x0FDBu,
            0x00B6B930u,
            RenderWorldContext::SelectorCommon,
        },
        {
            "AccumulateScene",
            0x10B7u,
            0x00B6BEE0u,
            RenderWorldContext::AuxiliaryPass0,
        },
        {
            "RenderAndFinalizeAccumulator",
            0x10D9u,
            0x00B6C0D0u,
            RenderWorldContext::AuxiliaryPass0,
        },
        {
            "AccumulateScene",
            0x116Du,
            0x00B6BEE0u,
            RenderWorldContext::AuxiliaryPass1,
        },
        {
            "RenderAndFinalizeAccumulator",
            0x118Fu,
            0x00B6C0D0u,
            RenderWorldContext::AuxiliaryPass1,
        },
    }};

enum class BranchEncoding : std::uint8_t
{
    NearConditionalRel32,
    ShortUnconditionalRel8,
};

struct RenderWorldBranch
{
    const char* name = nullptr;
    std::size_t functionOffset = 0;
    std::size_t targetFunctionOffset = 0;
    BranchEncoding encoding = BranchEncoding::NearConditionalRel32;
    std::uint8_t conditionOpcode = 0;
};

inline constexpr std::array<RenderWorldBranch, 2> RenderWorldBranchContract {{
    {
        "select_normal_accumulation_strategy",
        0x01B7u,
        0x0F4Fu,
        BranchEncoding::NearConditionalRel32,
        0x84u,
    },
    {
        "alternate_strategy_skip_normal_accumulate",
        0x0F4Du,
        0x0F63u,
        BranchEncoding::ShortUnconditionalRel8,
        0,
    },
}};

// Keep the probe and future engine gate on one exact lifecycle/culling/render
// inventory. A private five-entry probe list previously omitted destructors,
// virtual dispatch targets, and the second stock branch's full lifecycle.
inline constexpr const auto& RetailAbiEvidenceInventory =
    fnvxr::engine::abi::RetailFunctionAbiInventory;
inline constexpr const auto& RetailVtableBlockEvidenceInventory =
    fnvxr::engine::abi::RetailVtableBlocks;

inline bool finalizeComputedSha256Digest(
    bool success,
    fnvxr::engine::Sha256Digest& digest) noexcept
{
    if (!success)
    {
        digest = {};
        return false;
    }
    digest.valid = true;
    return true;
}

struct DirectCallObservation
{
    bool instructionReadable = false;
    bool opcodeMatches = false;
    bool targetDecoded = false;
    std::uintptr_t targetAddress = 0;
    bool targetMatches = false;

    bool complete() const noexcept
    {
        return instructionReadable
            && opcodeMatches
            && targetDecoded
            && targetMatches;
    }
};

struct BranchObservation
{
    bool instructionReadable = false;
    bool opcodeMatches = false;
    bool targetDecoded = false;
    std::uintptr_t targetAddress = 0;
    bool targetMatches = false;

    bool complete() const noexcept
    {
        return instructionReadable
            && opcodeMatches
            && targetDecoded
            && targetMatches;
    }
};

struct ByteWindow
{
    std::size_t offset = 0;
    std::size_t byteCount = 0;
};

inline bool checkedAddAddress(
    std::uintptr_t address,
    std::size_t byteCount,
    std::uintptr_t& result) noexcept
{
    const std::uintptr_t maximum = (std::numeric_limits<std::uintptr_t>::max)();
    if (byteCount > maximum || address > maximum - byteCount)
        return false;
    result = address + static_cast<std::uintptr_t>(byteCount);
    return true;
}

inline bool checkedAddDisplacement(
    std::uintptr_t instructionEnd,
    std::int64_t displacement,
    std::uintptr_t& result) noexcept
{
    const std::uintptr_t maximum = (std::numeric_limits<std::uintptr_t>::max)();
    if (displacement >= 0)
    {
        const std::uint64_t distance = static_cast<std::uint64_t>(displacement);
        if (distance > maximum || instructionEnd > maximum - distance)
            return false;
        result = instructionEnd + static_cast<std::uintptr_t>(distance);
        return true;
    }

    if (displacement == (std::numeric_limits<std::int64_t>::min)())
        return false;
    const std::uint64_t distance = static_cast<std::uint64_t>(-displacement);
    if (distance > instructionEnd)
        return false;
    result = instructionEnd - static_cast<std::uintptr_t>(distance);
    return true;
}

inline bool relocateFromFunction(
    std::uintptr_t runtimeFunctionAddress,
    std::uintptr_t preferredFunctionAddress,
    std::uintptr_t preferredTargetAddress,
    std::uintptr_t& result) noexcept
{
    const std::uintptr_t maximum = (std::numeric_limits<std::uintptr_t>::max)();
    if (runtimeFunctionAddress >= preferredFunctionAddress)
    {
        const std::uintptr_t delta = runtimeFunctionAddress - preferredFunctionAddress;
        if (preferredTargetAddress > maximum - delta)
            return false;
        result = preferredTargetAddress + delta;
        return true;
    }

    const std::uintptr_t delta = preferredFunctionAddress - runtimeFunctionAddress;
    if (preferredTargetAddress < delta)
        return false;
    result = preferredTargetAddress - delta;
    return true;
}

inline bool readRel32(
    const std::uint8_t* bytes,
    std::size_t byteCount,
    std::size_t offset,
    std::int32_t& result) noexcept
{
    if (!bytes || offset > byteCount || byteCount - offset < 4u)
        return false;
    const std::uint32_t raw = static_cast<std::uint32_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1u]) << 8u)
        | (static_cast<std::uint32_t>(bytes[offset + 2u]) << 16u)
        | (static_cast<std::uint32_t>(bytes[offset + 3u]) << 24u);
    if (raw <= static_cast<std::uint32_t>(
                   (std::numeric_limits<std::int32_t>::max)()))
    {
        result = static_cast<std::int32_t>(raw);
    }
    else
    {
        const std::uint32_t magnitude = (~raw) + 1u;
        result = magnitude == 0x80000000u
            ? (std::numeric_limits<std::int32_t>::min)()
            : -static_cast<std::int32_t>(magnitude);
    }
    return true;
}

inline DirectCallObservation observeDirectCall(
    const std::uint8_t* functionBytes,
    std::size_t functionByteCount,
    std::uintptr_t runtimeFunctionAddress,
    std::uintptr_t preferredFunctionAddress,
    const RenderWorldDirectCall& contract) noexcept
{
    DirectCallObservation result {};
    std::uintptr_t instructionAddress = 0;
    std::uintptr_t instructionEnd = 0;
    if (!functionBytes
        || contract.functionOffset > functionByteCount
        || functionByteCount - contract.functionOffset < 5u
        || !checkedAddAddress(
            runtimeFunctionAddress,
            contract.functionOffset,
            instructionAddress)
        || !checkedAddAddress(instructionAddress, 5u, instructionEnd))
    {
        return result;
    }

    result.instructionReadable = true;
    result.opcodeMatches = functionBytes[contract.functionOffset] == 0xE8u;
    std::int32_t displacement = 0;
    result.targetDecoded = readRel32(
                               functionBytes,
                               functionByteCount,
                               contract.functionOffset + 1u,
                               displacement)
        && checkedAddDisplacement(
            instructionEnd,
            displacement,
            result.targetAddress);

    std::uintptr_t expectedTarget = 0;
    const bool expectedRelocated = relocateFromFunction(
        runtimeFunctionAddress,
        preferredFunctionAddress,
        contract.preferredTargetAddress,
        expectedTarget);
    result.targetMatches = result.targetDecoded
        && expectedRelocated
        && result.targetAddress == expectedTarget;
    return result;
}

inline BranchObservation observeBranch(
    const std::uint8_t* functionBytes,
    std::size_t functionByteCount,
    std::uintptr_t runtimeFunctionAddress,
    const RenderWorldBranch& contract) noexcept
{
    BranchObservation result {};
    const std::size_t instructionBytes = contract.encoding
            == BranchEncoding::NearConditionalRel32
        ? 6u
        : 2u;
    std::uintptr_t instructionAddress = 0;
    std::uintptr_t instructionEnd = 0;
    if (!functionBytes
        || contract.functionOffset > functionByteCount
        || functionByteCount - contract.functionOffset < instructionBytes
        || !checkedAddAddress(
            runtimeFunctionAddress,
            contract.functionOffset,
            instructionAddress)
        || !checkedAddAddress(
            instructionAddress,
            instructionBytes,
            instructionEnd))
    {
        return result;
    }

    result.instructionReadable = true;
    std::int64_t displacement = 0;
    if (contract.encoding == BranchEncoding::NearConditionalRel32)
    {
        result.opcodeMatches = functionBytes[contract.functionOffset] == 0x0Fu
            && functionBytes[contract.functionOffset + 1u]
                == contract.conditionOpcode;
        std::int32_t rel32 = 0;
        result.targetDecoded = readRel32(
            functionBytes,
            functionByteCount,
            contract.functionOffset + 2u,
            rel32);
        displacement = rel32;
    }
    else
    {
        result.opcodeMatches = functionBytes[contract.functionOffset] == 0xEBu;
        displacement = static_cast<std::int8_t>(
            functionBytes[contract.functionOffset + 1u]);
        result.targetDecoded = true;
    }

    if (result.targetDecoded)
    {
        result.targetDecoded = checkedAddDisplacement(
            instructionEnd,
            displacement,
            result.targetAddress);
    }

    if (contract.targetFunctionOffset
        <= (std::numeric_limits<std::uintptr_t>::max)() - runtimeFunctionAddress)
    {
        result.targetMatches = result.targetDecoded
            && result.targetAddress
                == runtimeFunctionAddress + contract.targetFunctionOffset;
    }
    return result;
}

constexpr ByteWindow contextWindow(
    std::size_t totalBytes,
    std::size_t instructionOffset,
    std::size_t instructionBytes,
    std::size_t bytesBefore,
    std::size_t bytesAfter) noexcept
{
    if (instructionOffset >= totalBytes || instructionBytes == 0u)
        return {};
    const std::size_t begin = instructionOffset > bytesBefore
        ? instructionOffset - bytesBefore
        : 0u;
    const std::size_t availableInstructionBytes = totalBytes - instructionOffset;
    const std::size_t boundedInstructionBytes = instructionBytes < availableInstructionBytes
        ? instructionBytes
        : availableInstructionBytes;
    const std::size_t instructionEnd = instructionOffset + boundedInstructionBytes;
    const std::size_t availableAfter = totalBytes - instructionEnd;
    const std::size_t boundedAfter = bytesAfter < availableAfter
        ? bytesAfter
        : availableAfter;
    return { begin, instructionEnd + boundedAfter - begin };
}

constexpr bool rangeContained(
    std::uintptr_t imageBase,
    std::size_t imageBytes,
    std::uintptr_t rangeAddress,
    std::size_t rangeBytes) noexcept
{
    if (imageBytes == 0u
        || rangeBytes == 0u
        || imageBytes > (std::numeric_limits<std::uintptr_t>::max)() - imageBase
        || rangeAddress < imageBase)
    {
        return false;
    }
    const std::uintptr_t offset = rangeAddress - imageBase;
    return offset < imageBytes
        && rangeBytes <= imageBytes - static_cast<std::size_t>(offset);
}

struct VtableSlotObservation
{
    bool slotAddressRelocated = false;
    std::uintptr_t slotAddress = 0;
    bool slotInMainModule = false;
    bool valueReadable = false;
    std::uintptr_t actualTargetAddress = 0;
    bool expectedTargetRelocated = false;
    std::uintptr_t expectedTargetAddress = 0;
    bool targetInMainModule = false;
    bool targetMatches = false;

    bool complete() const noexcept
    {
        return slotAddressRelocated
            && slotInMainModule
            && valueReadable
            && expectedTargetRelocated
            && targetInMainModule
            && targetMatches;
    }
};

inline VtableSlotObservation observeVtableSlot(
    std::uintptr_t runtimeImageBase,
    std::size_t runtimeImageBytes,
    const fnvxr::engine::abi::RetailVtableSlotDescriptor& descriptor,
    bool valueReadable,
    std::uint32_t loadedTargetAddress) noexcept
{
    VtableSlotObservation result {};
    std::uintptr_t preferredSlotAddress = 0;
    result.slotAddressRelocated = checkedAddAddress(
                                      descriptor.vtableAddress,
                                      descriptor.slotByteOffset,
                                      preferredSlotAddress)
        && relocateFromFunction(
            runtimeImageBase,
            fnvxr::engine::SupportedImageBase,
            preferredSlotAddress,
            result.slotAddress);
    result.slotInMainModule = result.slotAddressRelocated
        && rangeContained(
            runtimeImageBase,
            runtimeImageBytes,
            result.slotAddress,
            sizeof(std::uint32_t));
    result.valueReadable = valueReadable;
    result.actualTargetAddress = loadedTargetAddress;
    result.expectedTargetRelocated = relocateFromFunction(
        runtimeImageBase,
        fnvxr::engine::SupportedImageBase,
        descriptor.preferredTargetAddress,
        result.expectedTargetAddress);
    result.targetInMainModule = valueReadable
        && rangeContained(
            runtimeImageBase,
            runtimeImageBytes,
            result.actualTargetAddress,
            1u);
    result.targetMatches = valueReadable
        && result.expectedTargetRelocated
        && result.actualTargetAddress == result.expectedTargetAddress;
    return result;
}

struct VtableBlockObservation
{
    bool blockAddressRelocated = false;
    std::uintptr_t blockAddress = 0;
    bool blockInMainModule = false;
    bool bytesReadable = false;
    bool hashAvailable = false;
    bool hashMatches = false;

    bool complete() const noexcept
    {
        return blockAddressRelocated
            && blockInMainModule
            && bytesReadable
            && hashAvailable
            && hashMatches;
    }
};

inline VtableBlockObservation observeVtableBlock(
    std::uintptr_t runtimeImageBase,
    std::size_t runtimeImageBytes,
    const fnvxr::engine::abi::RetailVtableBlockDescriptor& descriptor,
    bool bytesReadable,
    const fnvxr::engine::Sha256Digest& loadedSha256) noexcept
{
    VtableBlockObservation result {};
    result.blockAddressRelocated = relocateFromFunction(
        runtimeImageBase,
        fnvxr::engine::SupportedImageBase,
        descriptor.preferredAddress,
        result.blockAddress);
    result.blockInMainModule = result.blockAddressRelocated
        && rangeContained(
            runtimeImageBase,
            runtimeImageBytes,
            result.blockAddress,
            descriptor.byteCount);
    result.bytesReadable = bytesReadable;
    result.hashAvailable = bytesReadable && loadedSha256.valid;
    result.hashMatches = result.hashAvailable
        && fnvxr::engine::digestMatches(
            descriptor.sha256,
            loadedSha256.bytes.data(),
            loadedSha256.bytes.size());
    return result;
}
}
