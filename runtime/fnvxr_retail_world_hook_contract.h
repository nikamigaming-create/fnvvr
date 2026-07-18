#pragma once

#include "fnvxr_engine_capability.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace fnvxr::engine
{
// This file describes the inert x86 detour seam for the exact retail
// RenderWorldSceneGraph body. It deliberately contains no memory writer,
// allocator, installer, or authority issuer; those live in separate layers.
inline constexpr std::size_t RetailWorldFunctionByteCount = 4698u;
inline constexpr std::size_t RetailWorldHookStolenByteCount = 5u;
inline constexpr std::size_t RetailWorldHookPatchByteCount = 5u;
inline constexpr std::size_t RetailWorldHookTrampolineByteCount = 10u;
inline constexpr std::size_t RetailWorldReturnInstructionOffset = 0x1257u;
inline constexpr std::uint32_t RetailWorldStackArgumentBytes = 0x10u;
inline constexpr std::uint32_t RetailWorldHookIndependentProcessSamples = 2u;
inline constexpr bool RetailWorldHookContractHeaderContainsIssuer = false;

// The five stolen bytes are three complete instructions:
//   push ebp; mov ebp, esp; push -1
// Copying six bytes, as the retired DoRenderFrame detour did, would split the
// following `push imm32` instruction and is invalid at this boundary.
inline constexpr std::array<std::uint8_t, RetailWorldHookStolenByteCount>
    RetailWorldHookStolenBytes {{ 0x55u, 0x8Bu, 0xECu, 0x6Au, 0xFFu }};
inline constexpr std::array<std::uint8_t, 3> RetailWorldHookStolenInstructionBytes {
    { 1u, 2u, 2u }
};
inline constexpr std::array<std::uint8_t, 3> RetailWorldReturnBytes {
    { 0xC2u, 0x10u, 0x00u }
};

constexpr std::size_t retailWorldStolenInstructionByteCount() noexcept
{
    std::size_t result = 0u;
    for (const std::uint8_t byteCount : RetailWorldHookStolenInstructionBytes)
        result += byteCount;
    return result;
}

static_assert(WorldRenderAddress == 0x00873200u);
static_assert(retailWorldStolenInstructionByteCount() == 5u);
static_assert(
    RetailWorldReturnInstructionOffset + RetailWorldReturnBytes.size()
    == RetailWorldFunctionByteCount);

// Raw stack slots are represented as dwords even when the retail body reads a
// slot as a byte.  That preserves the proven `ret 0x10` ABI on every compiler
// used to test this contract.
struct RetailWorldRenderStackArguments
{
    std::uint32_t sharedRenderObjectAddress = 0u;
    std::uint32_t callerModePredicate = 0u;
    std::uint32_t stockWorldPathSelector = 0u;
    std::uint32_t postWorldOption = 0u;
};

static_assert(sizeof(RetailWorldRenderStackArguments) == 0x10u);
static_assert(offsetof(RetailWorldRenderStackArguments, callerModePredicate) == 0x04u);
static_assert(offsetof(RetailWorldRenderStackArguments, stockWorldPathSelector) == 0x08u);
static_assert(offsetof(RetailWorldRenderStackArguments, postWorldOption) == 0x0Cu);

#if defined(_MSC_VER) && defined(_M_IX86)
// A C++ entry hook must use this exact fastcall adapter shape: ECX carries the
// retail `this`, EDX is the fastcall placeholder, and the four original dword
// slots remain on the stack.  Both the original and adapter therefore pop 16
// stack bytes.
using RetailWorldRenderFunction = void (__thiscall*)(
    void*,
    void*,
    std::uint32_t,
    std::uint32_t,
    std::uint32_t);
using RetailWorldRenderHookFunction = void (__fastcall*)(
    void*,
    void*,
    void*,
    std::uint32_t,
    std::uint32_t,
    std::uint32_t);
inline constexpr bool RetailWorldRenderCallableAbiAvailable = true;
#else
inline constexpr bool RetailWorldRenderCallableAbiAvailable = false;
#endif

struct RetailWorldStackArgumentUse
{
    std::uint32_t stackOffset = 0u;
    bool readByRetailBody = false;
};

// Complete disassembly of the exact 4698-byte body shows that the caller-mode
// predicate occupies a real stack slot but is not read by this retail body.
// The caller itself uses the same predicate immediately after return.
inline constexpr std::array<RetailWorldStackArgumentUse, 4>
    RetailWorldStackArgumentUseContract {{
        { 0x08u, true },
        { 0x0Cu, false },
        { 0x10u, true },
        { 0x14u, true },
    }};

struct RetailWorldCallSiteContract
{
    const char* name = nullptr;
    std::uintptr_t preferredWindowAddress = 0u;
    std::uintptr_t preferredCallAddress = 0u;
    std::array<std::uint8_t, 24> bytes {};
    std::int32_t thisFrameOffset = 0;
    std::int32_t sharedRenderObjectFrameOffset = 0;
    std::int32_t callerModePredicateFrameOffset = 0;
    std::int32_t postWorldOptionFrameOffset = 0;
    std::uint32_t stockWorldPathSelector = 0u;
    std::uintptr_t sharedRenderObjectProducerAddress = 0u;
    std::uintptr_t postWorldOptionProducerAddress = 0u;
    std::uint32_t independentLoadedProcessSamples = 0u;
};

inline constexpr std::size_t RetailWorldCallInstructionWindowOffset = 0x13u;

// Exact windows observed in two independently loaded retail processes.  The
// first is the requested 0x00870AE8 caller.  A scan of the loaded executable
// found the second stock direct caller at 0x00870E18; an entry detour covers
// both without globally hooking shared accumulation helpers.
inline constexpr std::array<RetailWorldCallSiteContract, 2>
    RetailWorldCallSiteContractInventory {{
        {
            "primary world caller",
            0x00870AD5u,
            0x00870AE8u,
            {{
                0x0Fu, 0xB6u, 0x4Du, 0xF7u, // movzx ecx, byte [ebp-9]
                0x51u,                         // push postWorldOption
                0x6Au, 0x01u,                 // push selector literal 1
                0x0Fu, 0xB6u, 0x55u, 0xEFu, // movzx edx, byte [ebp-0x11]
                0x52u,                         // push callerModePredicate
                0x8Bu, 0x45u, 0xF0u,         // mov eax, [ebp-0x10]
                0x50u,                         // push sharedRenderObject
                0x8Bu, 0x4Du, 0xE0u,         // mov ecx, [ebp-0x20]
                0xE8u, 0x13u, 0x27u, 0x00u, 0x00u,
            }},
            -0x20,
            -0x10,
            -0x11,
            -0x09,
            1u,
            0x00872B00u,
            0x008749B0u,
            2u,
        },
        {
            "alternate world caller",
            0x00870E05u,
            0x00870E18u,
            {{
                0x0Fu, 0xB6u, 0x4Du, 0xF6u, // movzx ecx, byte [ebp-0xA]
                0x51u,                         // push postWorldOption
                0x6Au, 0x00u,                 // push selector literal 0
                0x0Fu, 0xB6u, 0x55u, 0xEDu, // movzx edx, byte [ebp-0x13]
                0x52u,                         // push callerModePredicate
                0x8Bu, 0x45u, 0xF0u,         // mov eax, [ebp-0x10]
                0x50u,                         // push sharedRenderObject
                0x8Bu, 0x4Du, 0x88u,         // mov ecx, [ebp-0x78]
                0xE8u, 0xE3u, 0x23u, 0x00u, 0x00u,
            }},
            -0x78,
            -0x10,
            -0x13,
            -0x0A,
            0u,
            0x00872B00u,
            0x008749B0u,
            2u,
        },
    }};

constexpr bool retailWorldBytesEqual(
    const std::uint8_t* bytes,
    std::size_t byteCount,
    const std::uint8_t* expected,
    std::size_t expectedByteCount) noexcept
{
    if (!bytes || !expected || byteCount < expectedByteCount)
        return false;
    for (std::size_t index = 0u; index < expectedByteCount; ++index)
    {
        if (bytes[index] != expected[index])
            return false;
    }
    return true;
}

constexpr bool retailWorldCheckedAdd(
    std::uintptr_t value,
    std::uintptr_t increment,
    std::uintptr_t& result) noexcept
{
    if (value > (std::numeric_limits<std::uintptr_t>::max)() - increment)
        return false;
    result = value + increment;
    return true;
}

constexpr bool relocateRetailWorldPreferredAddress(
    std::uintptr_t runtimeWorldAddress,
    std::uintptr_t preferredAddress,
    std::uintptr_t& result) noexcept
{
    if (runtimeWorldAddress >= WorldRenderAddress)
    {
        return retailWorldCheckedAdd(
            preferredAddress,
            runtimeWorldAddress - WorldRenderAddress,
            result);
    }

    const std::uintptr_t delta = WorldRenderAddress - runtimeWorldAddress;
    if (preferredAddress < delta)
        return false;
    result = preferredAddress - delta;
    return true;
}

constexpr std::int64_t retailWorldSignedRel32(std::uint32_t raw) noexcept
{
    if (raw <= 0x7FFFFFFFu)
        return static_cast<std::int64_t>(raw);
    return -static_cast<std::int64_t>((~raw) + 1u);
}

inline bool decodeRetailWorldRel32Target(
    const std::uint8_t* instruction,
    std::size_t instructionByteCount,
    std::uintptr_t instructionAddress,
    std::uintptr_t& targetAddress) noexcept
{
    targetAddress = 0u;
    if (!instruction
        || instructionByteCount < 5u
        || instruction[0] != 0xE8u)
    {
        return false;
    }

    const std::uint32_t raw = static_cast<std::uint32_t>(instruction[1])
        | (static_cast<std::uint32_t>(instruction[2]) << 8u)
        | (static_cast<std::uint32_t>(instruction[3]) << 16u)
        | (static_cast<std::uint32_t>(instruction[4]) << 24u);
    std::uintptr_t instructionEnd = 0u;
    if (!retailWorldCheckedAdd(instructionAddress, 5u, instructionEnd))
        return false;

    const std::int64_t displacement = retailWorldSignedRel32(raw);
    if (displacement >= 0)
    {
        const auto distance = static_cast<std::uintptr_t>(displacement);
        return retailWorldCheckedAdd(instructionEnd, distance, targetAddress);
    }

    const auto distance = static_cast<std::uintptr_t>(-displacement);
    if (instructionEnd < distance)
        return false;
    targetAddress = instructionEnd - distance;
    return true;
}

struct RetailWorldCallSiteObservation
{
    bool bytesReadable = false;
    bool exactSequenceMatches = false;
    bool callAddressMatches = false;
    bool targetDecoded = false;
    std::uintptr_t targetAddress = 0u;
    bool targetMatches = false;

    bool complete() const noexcept
    {
        return bytesReadable
            && exactSequenceMatches
            && callAddressMatches
            && targetDecoded
            && targetMatches;
    }
};

inline RetailWorldCallSiteObservation observeRetailWorldCallSite(
    const std::uint8_t* bytes,
    std::size_t byteCount,
    std::uintptr_t runtimeWindowAddress,
    std::uintptr_t runtimeWorldAddress,
    const RetailWorldCallSiteContract& contract) noexcept
{
    RetailWorldCallSiteObservation result {};
    result.bytesReadable = bytes && byteCount >= contract.bytes.size();
    if (!result.bytesReadable)
        return result;

    result.exactSequenceMatches = retailWorldBytesEqual(
        bytes,
        byteCount,
        contract.bytes.data(),
        contract.bytes.size());

    std::uintptr_t runtimeCallAddress = 0u;
    const bool runtimeCallAddressAvailable = retailWorldCheckedAdd(
        runtimeWindowAddress,
        RetailWorldCallInstructionWindowOffset,
        runtimeCallAddress);
    std::uintptr_t expectedRuntimeCallAddress = 0u;
    result.callAddressMatches = runtimeCallAddressAvailable
        && relocateRetailWorldPreferredAddress(
            runtimeWorldAddress,
            contract.preferredCallAddress,
            expectedRuntimeCallAddress)
        && runtimeCallAddress == expectedRuntimeCallAddress;
    result.targetDecoded = runtimeCallAddressAvailable
        && decodeRetailWorldRel32Target(
            bytes + RetailWorldCallInstructionWindowOffset,
            byteCount - RetailWorldCallInstructionWindowOffset,
            runtimeCallAddress,
            result.targetAddress);
    result.targetMatches = result.targetDecoded
        && result.targetAddress == runtimeWorldAddress;
    return result;
}

struct RetailWorldHookSeamObservation
{
    bool bodyReadable = false;
    bool entryMatches = false;
    bool returnMatches = false;
    bool stolenInstructionsWhole = false;

    bool complete() const noexcept
    {
        return bodyReadable
            && entryMatches
            && returnMatches
            && stolenInstructionsWhole;
    }
};

inline RetailWorldHookSeamObservation observeRetailWorldHookSeam(
    const std::uint8_t* functionBytes,
    std::size_t functionByteCount) noexcept
{
    RetailWorldHookSeamObservation result {};
    result.bodyReadable = functionBytes
        && functionByteCount >= RetailWorldFunctionByteCount;
    if (!result.bodyReadable)
        return result;
    result.entryMatches = retailWorldBytesEqual(
        functionBytes,
        functionByteCount,
        RetailWorldHookStolenBytes.data(),
        RetailWorldHookStolenBytes.size());
    result.returnMatches = retailWorldBytesEqual(
        functionBytes + RetailWorldReturnInstructionOffset,
        functionByteCount - RetailWorldReturnInstructionOffset,
        RetailWorldReturnBytes.data(),
        RetailWorldReturnBytes.size());
    result.stolenInstructionsWhole =
        retailWorldStolenInstructionByteCount()
        == RetailWorldHookStolenByteCount;
    return result;
}

struct RetailWorldRelativeJump
{
    bool valid = false;
    std::array<std::uint8_t, 5> bytes {};
};

inline RetailWorldRelativeJump encodeRetailWorldX86Jump(
    std::uintptr_t instructionAddress,
    std::uintptr_t targetAddress) noexcept
{
    RetailWorldRelativeJump result {};
    if (instructionAddress > 0xFFFFFFFFu || targetAddress > 0xFFFFFFFFu)
        return result;

    const std::uint32_t source = static_cast<std::uint32_t>(instructionAddress);
    const std::uint32_t target = static_cast<std::uint32_t>(targetAddress);
    const std::uint32_t displacement = target - (source + 5u);
    result.bytes = {{
        0xE9u,
        static_cast<std::uint8_t>(displacement & 0xFFu),
        static_cast<std::uint8_t>((displacement >> 8u) & 0xFFu),
        static_cast<std::uint8_t>((displacement >> 16u) & 0xFFu),
        static_cast<std::uint8_t>((displacement >> 24u) & 0xFFu),
    }};
    result.valid = true;
    return result;
}

struct RetailWorldHookPlan
{
    bool valid = false;
    std::uint32_t continuationAddress = 0u;
    std::array<std::uint8_t, RetailWorldHookPatchByteCount> entryPatch {};
    std::array<std::uint8_t, RetailWorldHookTrampolineByteCount> trampoline {};
};

// Produces bytes only.  Applying them remains impossible through this API and
// must eventually require a separate same-process authorization capability.
inline RetailWorldHookPlan buildRetailWorldHookPlan(
    const std::uint8_t* functionBytes,
    std::size_t functionByteCount,
    std::uintptr_t runtimeFunctionAddress,
    std::uintptr_t hookAddress,
    std::uintptr_t trampolineAddress) noexcept
{
    RetailWorldHookPlan result {};
    if (!observeRetailWorldHookSeam(functionBytes, functionByteCount).complete()
        || runtimeFunctionAddress > 0xFFFFFFFFu
        || hookAddress > 0xFFFFFFFFu
        || trampolineAddress > 0xFFFFFFFFu)
    {
        return result;
    }

    const RetailWorldRelativeJump entryJump = encodeRetailWorldX86Jump(
        runtimeFunctionAddress,
        hookAddress);
    const RetailWorldRelativeJump returnJump = encodeRetailWorldX86Jump(
        trampolineAddress + RetailWorldHookStolenByteCount,
        runtimeFunctionAddress + RetailWorldHookStolenByteCount);
    if (!entryJump.valid || !returnJump.valid)
        return result;

    result.entryPatch = entryJump.bytes;
    for (std::size_t index = 0u; index < RetailWorldHookStolenByteCount; ++index)
        result.trampoline[index] = RetailWorldHookStolenBytes[index];
    for (std::size_t index = 0u; index < returnJump.bytes.size(); ++index)
    {
        result.trampoline[RetailWorldHookStolenByteCount + index] =
            returnJump.bytes[index];
    }
    result.continuationAddress = static_cast<std::uint32_t>(
        runtimeFunctionAddress + RetailWorldHookStolenByteCount);
    result.valid = true;
    return result;
}
}
