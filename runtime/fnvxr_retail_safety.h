#pragma once

#include "fnvxr_retail_engine_manifest.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace fnvxr::safety
{
// xNVSE's packed runtime identifiers for the two 1.4.0.525 executables. The
// project has evidence only for the standard retail build, not the no-gore
// variant, so they are deliberately not treated as interchangeable.
inline constexpr std::uint32_t SupportedRuntimeVersion = 0x040020D0u;
inline constexpr std::uint32_t SupportedRuntimeVersionNoGore = 0x040020D1u;

// xNVSE 6.4.8's own nvse_version.h defines this as
// MAKE_NEW_VEGAS_VERSION(6, 4, 8). The packed value was also observed in the
// local retail loader log. A dotted-version-looking value such as 0x060408 is
// not the value supplied through NVSEInterface::nvseVersion.
inline constexpr std::uint32_t SupportedNvseVersion = 0x06040080u;
static_assert(
    SupportedNvseVersion
        == ((6u << 24u) | (4u << 16u) | (8u << 4u)),
    "xNVSE packed version must follow MAKE_NEW_VEGAS_VERSION");

// This is a source fuse, not a configuration default. It remains false until
// engine stereo, GPU color/depth transport, authoritative weapon, and UI mode
// transition gates all pass. An environment variable can request a feature
// but can never override this.
inline constexpr bool RetailMutationProofComplete = false;

// This record is intentionally explicit and fail-closed. The production
// in-process validator must populate it from the currently loaded module and
// from completed acceptance gates; configuration is never evidence. Binding
// the record to a process ID prevents a retained result from a probe process
// from being reused by the retail process.
struct RetailMutationEvidenceToken
{
    std::uint32_t validatingProcessId = 0;
    std::uint32_t nvseVersion = 0;
    std::uint32_t runtimeVersion = 0;
    std::uintptr_t loadedImageBase = 0;
    engine::LoadedExecutableIdentity loadedExecutable {};
    std::size_t loadedFunctionCount = 0;
    std::array<engine::Sha256Digest, engine::RetailEngineManifest.size()>
        loadedFunctionDigests {};
    bool engineStereoComplete = false;
    bool gpuColorTransportComplete = false;
    bool gpuDepthTransportComplete = false;
    bool authoritativeWeaponComplete = false;
    bool uiModeTransitionsComplete = false;
};

// This callback is part of the mutation decision, not a background probe. It
// must synchronously re-read the current process's loaded PE identity,
// protected function bytes, and compatibility-module inventory before it
// returns. A cached result is not a valid implementation. The opaque context
// is owned by the caller and exists only to supply the live validator's state.
using CurrentProcessIntegrityValidator = bool (*)(
    const RetailMutationEvidenceToken& evidence,
    void* context) noexcept;

inline bool compatibleNvseRuntime(
    bool isEditor,
    std::uint32_t nvseVersion,
    std::uint32_t runtimeVersion)
{
    return !isEditor
        && nvseVersion == SupportedNvseVersion
        && runtimeVersion == SupportedRuntimeVersion;
}

inline bool loadedFunctionManifestMatches(const RetailMutationEvidenceToken& evidence)
{
    if (evidence.loadedFunctionCount != engine::RetailEngineManifest.size())
        return false;

    for (std::size_t index = 0; index < engine::RetailEngineManifest.size(); ++index)
    {
        const engine::Sha256Digest& actual = evidence.loadedFunctionDigests[index];
        if (!actual.valid
            || !engine::digestMatches(
                engine::RetailEngineManifest[index].sha256,
                actual.bytes.data(),
                actual.bytes.size()))
        {
            return false;
        }
    }
    return true;
}

inline bool retailMutationEvidenceComplete(
    const RetailMutationEvidenceToken& evidence,
    std::uint32_t currentProcessId)
{
    return currentProcessId != 0
        && evidence.validatingProcessId == currentProcessId
        && compatibleNvseRuntime(false, evidence.nvseVersion, evidence.runtimeVersion)
        && evidence.loadedImageBase == engine::SupportedImageBase
        && engine::matchesLoadedExecutableIdentity(evidence.loadedExecutable)
        && loadedFunctionManifestMatches(evidence)
        && evidence.engineStereoComplete
        && evidence.gpuColorTransportComplete
        && evidence.gpuDepthTransportComplete
        && evidence.authoritativeWeaponComplete
        && evidence.uiModeTransitionsComplete;
}

inline bool retailMutationEvidenceCurrent(
    const RetailMutationEvidenceToken& evidence,
    std::uint32_t currentProcessId,
    CurrentProcessIntegrityValidator validateCurrentProcessIntegrity,
    void* validatorContext)
{
    // Validate the retained evidence first, then require one synchronous live
    // integrity check for this decision. A missing or rejecting callback is a
    // hard failure; retained evidence can never authorize mutation by itself.
    return retailMutationEvidenceComplete(evidence, currentProcessId)
        && validateCurrentProcessIntegrity != nullptr
        && validateCurrentProcessIntegrity(evidence, validatorContext);
}

inline bool retailMutationAllowed(
    bool requested,
    const RetailMutationEvidenceToken& evidence,
    std::uint32_t currentProcessId,
    CurrentProcessIntegrityValidator validateCurrentProcessIntegrity,
    void* validatorContext)
{
    // The source fuse is necessary but never sufficient. Flipping it cannot
    // activate mutation without current-process executable/function evidence,
    // an immediate live integrity recheck, and every production acceptance
    // gate above.
    return RetailMutationProofComplete
        && requested
        && retailMutationEvidenceCurrent(
            evidence,
            currentProcessId,
            validateCurrentProcessIntegrity,
            validatorContext);
}
}
