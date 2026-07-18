#include "fnvxr_jip_normalization.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool calculateTestSha256(
    const std::uint8_t* bytes,
    std::size_t byteCount,
    std::array<std::uint8_t, 32>& digest)
{
    digest = {};
    if (!bytes
        || byteCount == 0u
        || byteCount > (std::numeric_limits<ULONG>::max)())
    {
        return false;
    }

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    const NTSTATUS openStatus = BCryptOpenAlgorithmProvider(
        &algorithm,
        BCRYPT_SHA256_ALGORITHM,
        nullptr,
        0);
    if (openStatus < 0)
        return false;
    const NTSTATUS hashStatus = BCryptHash(
        algorithm,
        nullptr,
        0,
        const_cast<PUCHAR>(reinterpret_cast<const UCHAR*>(bytes)),
        static_cast<ULONG>(byteCount),
        digest.data(),
        static_cast<ULONG>(digest.size()));
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return hashStatus >= 0;
}

void writeRel32Call(
    std::vector<std::uint8_t>& bytes,
    std::size_t offset,
    std::uintptr_t instructionBase,
    std::uintptr_t target)
{
    require(offset + 5u <= bytes.size(), "test rel32 call must fit");
    const std::int64_t displacement = static_cast<std::int64_t>(target)
        - static_cast<std::int64_t>(instructionBase + offset + 5u);
    require(
        displacement >= (std::numeric_limits<std::int32_t>::min)()
            && displacement <= (std::numeric_limits<std::int32_t>::max)(),
        "test rel32 displacement must fit");
    const std::uint32_t encoded = static_cast<std::uint32_t>(
        static_cast<std::int32_t>(displacement));
    bytes[offset] = 0xE8u;
    bytes[offset + 1u] = static_cast<std::uint8_t>(encoded);
    bytes[offset + 2u] = static_cast<std::uint8_t>(encoded >> 8u);
    bytes[offset + 3u] = static_cast<std::uint8_t>(encoded >> 16u);
    bytes[offset + 4u] = static_cast<std::uint8_t>(encoded >> 24u);
}

std::array<std::uint8_t, fnvxr::probe::jip::Jip5730StubBytes> makeStub(
    std::uintptr_t moduleBase)
{
    using namespace fnvxr::probe::jip;
    std::array<std::uint8_t, Jip5730StubBytes> result {
        0x83u, 0x3Du, 0u, 0u, 0u, 0u, 0x00u, 0x74u, 0x07u,
        0xB8u, 0xC0u, 0x48u, 0x71u, 0x00u, 0xFFu, 0xE0u,
        0xC2u, 0x08u, 0x00u,
    };
    const std::uintptr_t absolute = moduleBase + Jip5730GuardVariableRva;
    require(absolute <= 0xFFFFFFFFu, "test JIP absolute address must fit x86");
    const std::uint32_t encoded = static_cast<std::uint32_t>(absolute);
    result[2] = static_cast<std::uint8_t>(encoded);
    result[3] = static_cast<std::uint8_t>(encoded >> 8u);
    result[4] = static_cast<std::uint8_t>(encoded >> 16u);
    result[5] = static_cast<std::uint8_t>(encoded >> 24u);
    return result;
}
}

int main()
{
    using namespace fnvxr::probe::jip;

    require(
        !CompatibilityModuleInventoryProductionProofComplete,
        "one normalized JIP call must not authorize the compatibility inventory");

    constexpr std::uintptr_t MainBase = 0x00400000u;
    constexpr std::uintptr_t JipBase = 0x0ADA0000u;
    constexpr std::uintptr_t FunctionAddress = 0x00875110u;
    constexpr std::uintptr_t JipTarget = JipBase + Jip5730RenderFirstPersonStubRva;

    std::vector<std::uint8_t> function(Jip5730RenderFirstPersonBytes, 0x90u);
    for (std::size_t index = 0; index < function.size(); ++index)
        function[index] = static_cast<std::uint8_t>((index * 29u + 7u) & 0xFFu);
    writeRel32Call(
        function,
        Jip5730RenderFirstPersonCallOffset,
        FunctionAddress,
        JipTarget);
    const std::vector<std::uint8_t> originalFunction = function;
    std::array<std::uint8_t, Jip5730StubBytes> stub = makeStub(JipBase);

    Jip5730NormalizationInput input {};
    input.fileHashAvailable = true;
    input.fileSha256 = Jip5730FileSha256.bytes;
    input.fileBytes = Jip5730FileBytes;
    input.loadedPeTimeDateStamp = Jip5730PeTimeDateStamp;
    input.loadedPeSizeOfImage = Jip5730SizeOfImage;
    input.loadedPePreferredImageBase = Jip5730PreferredImageBase;
    input.runtimeModuleBase = JipBase;
    input.runtimeModuleBytes = Jip5730SizeOfImage;
    input.runtimeMainModuleBase = MainBase;
    input.runtimeMainModuleBytes = fnvxr::engine::SupportedSizeOfImage;
    input.runtimeFunctionAddress = FunctionAddress;
    input.functionBytes = function.data();
    input.functionByteCount = function.size();
    input.stubRuntimeAddress = JipTarget;
    input.stubBytes = stub.data();
    input.stubByteCount = stub.size();

    std::vector<std::uint8_t> normalized;
    const Jip5730NormalizationObservation observation =
        normalizeJip5730RenderFirstPerson(input, normalized);
    require(observation.fileHashMatches, "the exact JIP file hash must match");
    require(observation.fileBytesMatch, "the exact JIP file size must match");
    require(observation.loadedPeMatches, "the exact loaded JIP PE identity must match");
    require(observation.moduleBoundsMatch, "the relocated JIP addresses must be bounded");
    require(observation.functionBoundsMatch, "the protected retail body must be bounded");
    require(observation.callOpcodeMatches, "the protected instruction must remain E8 rel32");
    require(observation.callTargetMatches, "the call must target exact JIP RVA 0x10880");
    require(observation.stubAddressMatches, "the supplied stub must be the live call target");
    require(observation.stubBytesMatch, "all 19 relocated stub bytes must match");
    require(observation.preconditionsComplete(), "all exact normalization preconditions must pass");
    require(observation.normalizedCopyCreated, "normalization must create a private output copy");
    require(normalized.size() == function.size(), "normalization must preserve function size");
    require(function == originalFunction, "normalization must never modify the source bytes");

    std::size_t changedBytes = 0u;
    for (std::size_t index = 0; index < normalized.size(); ++index)
    {
        if (normalized[index] != originalFunction[index])
        {
            ++changedBytes;
            require(
                index > Jip5730RenderFirstPersonCallOffset
                    && index < Jip5730RenderFirstPersonCallOffset + 5u,
                "only the four rel32 displacement bytes may change");
        }
    }
    require(changedBytes != 0u && changedBytes <= 4u, "the private call displacement must change");

    std::uintptr_t normalizedTarget = 0;
    require(
        decodeRel32CallTarget(
            normalized.data(),
            normalized.size(),
            FunctionAddress,
            Jip5730RenderFirstPersonCallOffset,
            normalizedTarget),
        "the normalized private call must remain decodable");
    require(
        normalizedTarget == Jip5730StockRenderFirstPersonTarget,
        "the private call must normalize to the exact stock target");

    fnvxr::engine::Sha256Digest syntheticNormalizedDigest {};
    require(
        calculateTestSha256(
            normalized.data(),
            normalized.size(),
            syntheticNormalizedDigest.bytes),
        "the synthetic normalized fixture must be hashable");
    syntheticNormalizedDigest.valid = true;
    require(
        normalizedFunctionMatchesExpectedDigest(
            observation,
            normalized,
            syntheticNormalizedDigest,
            calculateTestSha256),
        "the digest-bound helper must exercise its positive acceptance branch");
    fnvxr::engine::Sha256Digest wrongSyntheticDigest = syntheticNormalizedDigest;
    wrongSyntheticDigest.bytes[11] ^= 0x01u;
    require(
        !normalizedFunctionMatchesExpectedDigest(
            observation,
            normalized,
            wrongSyntheticDigest,
            calculateTestSha256),
        "the digest-bound helper must reject a one-bit expected-digest drift");
    require(
        !normalizationAccepted(
            observation,
            normalized,
            calculateTestSha256),
        "synthetic function bytes must not be accepted as the retail body");
    require(
        !normalizationAccepted(
            observation,
            normalized,
            [](const std::uint8_t*,
               std::size_t,
               std::array<std::uint8_t, 32>&) { return false; }),
        "a failed digest operation must fail closed");

    const auto expectRejected = [&](const Jip5730NormalizationInput& changed,
                                    const char* message) {
        std::vector<std::uint8_t> rejected { 0xA5u };
        const Jip5730NormalizationObservation rejectedObservation =
            normalizeJip5730RenderFirstPerson(changed, rejected);
        require(!rejectedObservation.preconditionsComplete(), message);
        require(!rejectedObservation.normalizedCopyCreated, message);
        require(rejected.empty(), message);
    };

    Jip5730NormalizationInput wrongHash = input;
    wrongHash.fileSha256[0] ^= 0x01u;
    expectRejected(wrongHash, "a one-byte DLL hash drift must fail closed");

    Jip5730NormalizationInput wrongIdentity = input;
    wrongIdentity.loadedPeTimeDateStamp ^= 0x01u;
    expectRejected(wrongIdentity, "a wrong loaded DLL timestamp must fail closed");
    wrongIdentity = input;
    wrongIdentity.loadedPePreferredImageBase += 0x10000u;
    expectRejected(wrongIdentity, "a wrong preferred DLL base must fail closed");
    wrongIdentity = input;
    wrongIdentity.loadedPeSizeOfImage -= 0x1000u;
    expectRejected(wrongIdentity, "a wrong loaded DLL image size must fail closed");
    wrongIdentity = input;
    wrongIdentity.fileBytes -= 1u;
    expectRejected(wrongIdentity, "a wrong DLL file size must fail closed");

    std::vector<std::uint8_t> wrongRvaFunction = function;
    writeRel32Call(
        wrongRvaFunction,
        Jip5730RenderFirstPersonCallOffset,
        FunctionAddress,
        JipTarget + 1u);
    Jip5730NormalizationInput wrongRva = input;
    wrongRva.functionBytes = wrongRvaFunction.data();
    expectRejected(wrongRva, "a one-byte target RVA drift must fail closed");

    std::array<std::uint8_t, Jip5730StubBytes> wrongStub = stub;
    wrongStub.back() ^= 0x01u;
    Jip5730NormalizationInput wrongStubInput = input;
    wrongStubInput.stubBytes = wrongStub.data();
    expectRejected(wrongStubInput, "a one-byte live stub drift must fail closed");
    wrongStubInput = input;
    wrongStubInput.stubRuntimeAddress += 1u;
    expectRejected(wrongStubInput, "a stub supplied from the wrong address must fail closed");

    Jip5730NormalizationInput wrongBounds = input;
    wrongBounds.runtimeModuleBytes = Jip5730RenderFirstPersonStubRva + Jip5730StubBytes - 1u;
    expectRejected(wrongBounds, "a JIP stub crossing module bounds must fail closed");
    wrongBounds = input;
    wrongBounds.runtimeFunctionAddress += 1u;
    expectRejected(wrongBounds, "a protected function at the wrong main-module RVA must fail closed");

    std::vector<std::uint8_t> extraRewrite = function;
    extraRewrite[0x20u] ^= 0x01u;
    Jip5730NormalizationInput extraRewriteInput = input;
    extraRewriteInput.functionBytes = extraRewrite.data();
    std::vector<std::uint8_t> normalizedExtraRewrite;
    const Jip5730NormalizationObservation extraRewriteObservation =
        normalizeJip5730RenderFirstPerson(extraRewriteInput, normalizedExtraRewrite);
    require(
        extraRewriteObservation.preconditionsComplete(),
        "an extra protected-byte rewrite is detected by the final full-body hash gate");
    require(
        !normalizationAccepted(
            extraRewriteObservation,
            normalizedExtraRewrite,
            calculateTestSha256),
        "an extra protected-byte rewrite must fail the normalized full-body hash");

    std::cout << "JIP 57.30 read-only normalization contract passed\n";
    return EXIT_SUCCESS;
}
