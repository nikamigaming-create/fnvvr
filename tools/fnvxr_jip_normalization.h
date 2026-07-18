#pragma once

#include "fnvxr_retail_abi_map.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace fnvxr::probe::jip
{
inline constexpr std::uint64_t Jip5730FileBytes = 502272u;
inline constexpr std::uint32_t Jip5730PeTimeDateStamp = 0x665225A8u;
inline constexpr std::uint32_t Jip5730SizeOfImage = 0x00080000u;
inline constexpr std::uint32_t Jip5730PreferredImageBase = 0x10000000u;
inline constexpr std::uint32_t Jip5730RenderFirstPersonStubRva = 0x00010880u;
inline constexpr std::uint32_t Jip5730GuardVariableRva = 0x0006A188u;
inline constexpr std::size_t Jip5730StubBytes = 19u;

inline constexpr std::uintptr_t Jip5730RenderFirstPersonPreferredAddress =
    0x00875110u;
inline constexpr std::size_t Jip5730RenderFirstPersonBytes = 3361u;
inline constexpr std::size_t Jip5730RenderFirstPersonCallOffset = 0x00B6u;
inline constexpr std::uintptr_t Jip5730StockRenderFirstPersonTarget =
    0x007148C0u;

inline constexpr fnvxr::engine::Sha256Digest Jip5730FileSha256 =
    fnvxr::engine::sha256FromHex(
        "9D2779647ED0CE63043390F47FC978E3234AF8E558DC6CB6BCB231478A2D74D4");
inline constexpr fnvxr::engine::Sha256Digest
    Jip5730NormalizedRenderFirstPersonSha256 = fnvxr::engine::sha256FromHex(
        "7F734D69C1C74C2099BE684FB4FE682BF84B3F75A108F109CCF1DF74EF9D55F2");

// Exact JIP normalization proves only this one protected rewrite. ShowOff and
// the complete code-mutating module inventory remain unnormalized.
inline constexpr bool CompatibilityModuleInventoryProductionProofComplete = false;

struct Jip5730NormalizationInput
{
    bool fileHashAvailable = false;
    std::array<std::uint8_t, 32> fileSha256 {};
    std::uint64_t fileBytes = 0;

    std::uint32_t loadedPeTimeDateStamp = 0;
    std::uint32_t loadedPeSizeOfImage = 0;
    std::uint32_t loadedPePreferredImageBase = 0;
    std::uintptr_t runtimeModuleBase = 0;
    std::size_t runtimeModuleBytes = 0;

    std::uintptr_t runtimeMainModuleBase = 0;
    std::size_t runtimeMainModuleBytes = 0;
    std::uintptr_t runtimeFunctionAddress = 0;
    const std::uint8_t* functionBytes = nullptr;
    std::size_t functionByteCount = 0;

    std::uintptr_t stubRuntimeAddress = 0;
    const std::uint8_t* stubBytes = nullptr;
    std::size_t stubByteCount = 0;
};

struct Jip5730NormalizationObservation
{
    bool fileHashMatches = false;
    bool fileBytesMatch = false;
    bool loadedPeMatches = false;
    bool moduleBoundsMatch = false;
    bool functionBoundsMatch = false;
    bool callOpcodeMatches = false;
    bool callTargetDecoded = false;
    std::uintptr_t actualCallTarget = 0;
    bool callTargetMatches = false;
    bool stubAddressMatches = false;
    bool stubBytesMatch = false;
    bool stockDisplacementEncodable = false;
    bool normalizedCopyCreated = false;

    bool preconditionsComplete() const noexcept
    {
        return fileHashMatches
            && fileBytesMatch
            && loadedPeMatches
            && moduleBoundsMatch
            && functionBoundsMatch
            && callOpcodeMatches
            && callTargetDecoded
            && callTargetMatches
            && stubAddressMatches
            && stubBytesMatch
            && stockDisplacementEncodable;
    }
};

inline bool decodeRel32CallTarget(
    const std::uint8_t* functionBytes,
    std::size_t functionByteCount,
    std::uintptr_t runtimeFunctionAddress,
    std::size_t callOffset,
    std::uintptr_t& result) noexcept
{
    result = 0;
    if (!functionBytes
        || callOffset > functionByteCount
        || functionByteCount - callOffset < 5u
        || functionBytes[callOffset] != 0xE8u)
    {
        return false;
    }

    std::uintptr_t instructionAddress = 0;
    std::uintptr_t instructionEnd = 0;
    std::int32_t displacement = 0;
    return fnvxr::probe::abi::checkedAddAddress(
               runtimeFunctionAddress,
               callOffset,
               instructionAddress)
        && fnvxr::probe::abi::checkedAddAddress(
            instructionAddress,
            5u,
            instructionEnd)
        && fnvxr::probe::abi::readRel32(
            functionBytes,
            functionByteCount,
            callOffset + 1u,
            displacement)
        && fnvxr::probe::abi::checkedAddDisplacement(
            instructionEnd,
            displacement,
            result);
}

inline bool encodeRel32Displacement(
    std::uintptr_t instructionEnd,
    std::uintptr_t target,
    std::array<std::uint8_t, 4>& result) noexcept
{
    std::int64_t displacement = 0;
    if (target >= instructionEnd)
    {
        const std::uintptr_t distance = target - instructionEnd;
        if (distance > static_cast<std::uintptr_t>(
                           (std::numeric_limits<std::int32_t>::max)()))
        {
            return false;
        }
        displacement = static_cast<std::int64_t>(distance);
    }
    else
    {
        const std::uintptr_t distance = instructionEnd - target;
        constexpr std::uint64_t MinimumMagnitude = 0x80000000ull;
        if (static_cast<std::uint64_t>(distance) > MinimumMagnitude)
            return false;
        displacement = -static_cast<std::int64_t>(distance);
    }

    const std::uint32_t encoded = static_cast<std::uint32_t>(
        static_cast<std::int32_t>(displacement));
    result[0] = static_cast<std::uint8_t>(encoded);
    result[1] = static_cast<std::uint8_t>(encoded >> 8u);
    result[2] = static_cast<std::uint8_t>(encoded >> 16u);
    result[3] = static_cast<std::uint8_t>(encoded >> 24u);
    return true;
}

inline bool makeExpectedStub(
    std::uintptr_t runtimeModuleBase,
    std::array<std::uint8_t, Jip5730StubBytes>& result) noexcept
{
    std::uintptr_t guardAddress = 0;
    if (!fnvxr::probe::abi::checkedAddAddress(
            runtimeModuleBase,
            Jip5730GuardVariableRva,
            guardAddress)
        || guardAddress > 0xFFFFFFFFu)
    {
        return false;
    }

    result = {
        0x83u, 0x3Du, 0u, 0u, 0u, 0u, 0x00u, 0x74u, 0x07u,
        0xB8u, 0xC0u, 0x48u, 0x71u, 0x00u, 0xFFu, 0xE0u,
        0xC2u, 0x08u, 0x00u,
    };
    const std::uint32_t encodedGuard = static_cast<std::uint32_t>(guardAddress);
    result[2] = static_cast<std::uint8_t>(encodedGuard);
    result[3] = static_cast<std::uint8_t>(encodedGuard >> 8u);
    result[4] = static_cast<std::uint8_t>(encodedGuard >> 16u);
    result[5] = static_cast<std::uint8_t>(encodedGuard >> 24u);
    return true;
}

inline Jip5730NormalizationObservation normalizeJip5730RenderFirstPerson(
    const Jip5730NormalizationInput& input,
    std::vector<std::uint8_t>& normalizedFunction)
{
    normalizedFunction.clear();
    Jip5730NormalizationObservation result {};

    result.fileHashMatches = input.fileHashAvailable
        && fnvxr::engine::digestMatches(
            Jip5730FileSha256,
            input.fileSha256.data(),
            input.fileSha256.size());
    result.fileBytesMatch = input.fileBytes == Jip5730FileBytes;
    result.loadedPeMatches = input.loadedPeTimeDateStamp == Jip5730PeTimeDateStamp
        && input.loadedPeSizeOfImage == Jip5730SizeOfImage
        && input.loadedPePreferredImageBase == Jip5730PreferredImageBase;

    std::uintptr_t expectedStubAddress = 0;
    std::uintptr_t guardAddress = 0;
    const bool moduleAddressesValid = fnvxr::probe::abi::checkedAddAddress(
                                          input.runtimeModuleBase,
                                          Jip5730RenderFirstPersonStubRva,
                                          expectedStubAddress)
        && fnvxr::probe::abi::checkedAddAddress(
            input.runtimeModuleBase,
            Jip5730GuardVariableRva,
            guardAddress);
    result.moduleBoundsMatch = input.runtimeModuleBytes == Jip5730SizeOfImage
        && moduleAddressesValid
        && fnvxr::probe::abi::rangeContained(
            input.runtimeModuleBase,
            input.runtimeModuleBytes,
            expectedStubAddress,
            Jip5730StubBytes)
        && fnvxr::probe::abi::rangeContained(
            input.runtimeModuleBase,
            input.runtimeModuleBytes,
            guardAddress,
            1u);

    std::uintptr_t expectedFunctionAddress = 0;
    const bool functionRelocated = fnvxr::probe::abi::relocateFromFunction(
        input.runtimeMainModuleBase,
        fnvxr::engine::SupportedImageBase,
        Jip5730RenderFirstPersonPreferredAddress,
        expectedFunctionAddress);
    result.functionBoundsMatch = input.functionBytes
        && input.functionByteCount == Jip5730RenderFirstPersonBytes
        && input.runtimeMainModuleBytes == fnvxr::engine::SupportedSizeOfImage
        && functionRelocated
        && input.runtimeFunctionAddress == expectedFunctionAddress
        && fnvxr::probe::abi::rangeContained(
            input.runtimeMainModuleBase,
            input.runtimeMainModuleBytes,
            input.runtimeFunctionAddress,
            input.functionByteCount);

    result.callOpcodeMatches = result.functionBoundsMatch
        && input.functionBytes[Jip5730RenderFirstPersonCallOffset] == 0xE8u;
    result.callTargetDecoded = result.functionBoundsMatch
        && decodeRel32CallTarget(
            input.functionBytes,
            input.functionByteCount,
            input.runtimeFunctionAddress,
            Jip5730RenderFirstPersonCallOffset,
            result.actualCallTarget);
    result.callTargetMatches = result.callTargetDecoded
        && moduleAddressesValid
        && result.actualCallTarget == expectedStubAddress;
    result.stubAddressMatches = moduleAddressesValid
        && input.stubRuntimeAddress == expectedStubAddress
        && input.stubRuntimeAddress == result.actualCallTarget;

    std::array<std::uint8_t, Jip5730StubBytes> expectedStub {};
    const bool expectedStubAvailable = makeExpectedStub(
        input.runtimeModuleBase,
        expectedStub);
    result.stubBytesMatch = result.moduleBoundsMatch
        && expectedStubAvailable
        && input.stubBytes
        && input.stubByteCount == expectedStub.size()
        && std::equal(
            expectedStub.begin(),
            expectedStub.end(),
            input.stubBytes);

    std::uintptr_t stockRuntimeTarget = 0;
    std::uintptr_t callInstruction = 0;
    std::uintptr_t callInstructionEnd = 0;
    std::array<std::uint8_t, 4> stockDisplacement {};
    result.stockDisplacementEncodable = functionRelocated
        && fnvxr::probe::abi::relocateFromFunction(
            input.runtimeMainModuleBase,
            fnvxr::engine::SupportedImageBase,
            Jip5730StockRenderFirstPersonTarget,
            stockRuntimeTarget)
        && fnvxr::probe::abi::checkedAddAddress(
            input.runtimeFunctionAddress,
            Jip5730RenderFirstPersonCallOffset,
            callInstruction)
        && fnvxr::probe::abi::checkedAddAddress(
            callInstruction,
            5u,
            callInstructionEnd)
        && encodeRel32Displacement(
            callInstructionEnd,
            stockRuntimeTarget,
            stockDisplacement);

    if (!result.preconditionsComplete())
        return result;

    normalizedFunction.assign(
        input.functionBytes,
        input.functionBytes + input.functionByteCount);
    std::copy(
        stockDisplacement.begin(),
        stockDisplacement.end(),
        normalizedFunction.begin() + Jip5730RenderFirstPersonCallOffset + 1u);
    result.normalizedCopyCreated = true;
    return result;
}

template <typename HashFunction>
inline bool normalizedFunctionMatchesExpectedDigest(
    const Jip5730NormalizationObservation& observation,
    const std::vector<std::uint8_t>& normalizedFunction,
    const fnvxr::engine::Sha256Digest& expectedDigest,
    HashFunction hashFunction)
{
    if (!observation.preconditionsComplete()
        || !observation.normalizedCopyCreated
        || !expectedDigest.valid
        || normalizedFunction.size() != Jip5730RenderFirstPersonBytes)
    {
        return false;
    }

    std::array<std::uint8_t, 32> digest {};
    if (!hashFunction(
            normalizedFunction.data(),
            normalizedFunction.size(),
            digest))
    {
        return false;
    }
    return fnvxr::engine::digestMatches(
        expectedDigest,
        digest.data(),
        digest.size());
}

// Production acceptance always remains pinned to the exact stock full-body
// digest. The injected expected-digest helper exists only so deterministic
// tests can exercise both branches without embedding a 3361-byte retail body.
template <typename HashFunction>
inline bool normalizationAccepted(
    const Jip5730NormalizationObservation& observation,
    const std::vector<std::uint8_t>& normalizedFunction,
    HashFunction hashFunction)
{
    return normalizedFunctionMatchesExpectedDigest(
        observation,
        normalizedFunction,
        Jip5730NormalizedRenderFirstPersonSha256,
        hashFunction);
}
}
