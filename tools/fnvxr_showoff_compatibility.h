#pragma once

#include "fnvxr_retail_engine_manifest.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace fnvxr::probe::showoff
{
inline constexpr std::uint64_t ShowOff184FileBytes = 1091584u;
inline constexpr std::uint32_t ShowOff184PeTimeDateStamp = 0x69C84C9Bu;
inline constexpr std::uint32_t ShowOff184SizeOfImage = 0x00110000u;
inline constexpr std::uint32_t ShowOff184PreferredImageBase = 0x10000000u;

inline constexpr fnvxr::engine::Sha256Digest ShowOff184FileSha256 =
    fnvxr::engine::sha256FromHex(
        "37CB22C5288FEDD0D57196C8C2F6BBABA5A1DAFD9CE58F14DAC9410DBEE7EF3E");

// Exact ShowOff verification proves only this one loaded module. It does not
// prove that every other code-mutating compatibility module has been found.
inline constexpr bool CompatibilityModuleInventoryProductionProofComplete = false;

enum class ShowOff184CompatibilityFailure
{
    None,
    ModuleNotLoaded,
    FileHashUnavailable,
    FileHashMismatch,
    FileBytesMismatch,
    PeTimeDateStampMismatch,
    PeSizeOfImageMismatch,
    PePreferredImageBaseMismatch,
    RuntimeModuleBoundsInvalid,
    SynchronousEvidenceNotCaptured,
    LoadedExecutableSectionsUnverified,
    ProtectedManifestUnverified,
    ProtectedAbiRangesUnverified,
    ProtectedVtableSlotsUnverified,
    JipFirstPersonProofInconsistent,
    SceneGraphSingletonUnverified,
    ProtectedTargetScanUnverified,
};

struct ShowOff184CompatibilityInput
{
    bool moduleLoaded = false;
    bool fileHashAvailable = false;
    std::array<std::uint8_t, 32> fileSha256 {};
    std::uint64_t fileBytes = 0;

    std::uint32_t loadedPeTimeDateStamp = 0;
    std::uint32_t loadedPeSizeOfImage = 0;
    std::uint32_t loadedPePreferredImageBase = 0;
    std::uintptr_t runtimeModuleBase = 0;
    std::size_t runtimeModuleBytes = 0;

    // These results must all come from the same synchronous, read-only live
    // observation. The protected-manifest result is the exact retail result
    // after the separately proven JIP normalization, when that path applies.
    bool synchronousEvidenceCaptured = false;
    // Disk identity and mapped PE headers do not prove the bytes that are
    // executing. This must compare every loaded executable section against a
    // relocation-normalized disk image with an explicit patch allowlist.
    bool synchronousLoadedExecutableSectionsMatched = false;
    bool synchronousProtectedManifestVerified = false;
    bool synchronousProtectedAbiRangesVerified = false;
    bool synchronousAllProtectedVtableSlotsVerified = false;

    // Both false is the pristine first-person path. Both true is the only
    // accepted JIP path. A disagreement is an incomplete or stale proof.
    bool jipNormalizedFirstPersonApplicable = false;
    bool synchronousJipNormalizedFirstPersonVerified = false;

    bool synchronousSceneGraphSingletonIntegrityVerified = false;
    bool synchronousNoProtectedTargetIntoShowOffVerified = false;
};

struct ShowOff184CompatibilityAssessment
{
    bool showOff184Accepted = false;
    bool compatibilityModuleInventoryProductionProofComplete = false;
    ShowOff184CompatibilityFailure failure =
        ShowOff184CompatibilityFailure::ModuleNotLoaded;
};

constexpr bool showOff184DigestMatches(
    const std::array<std::uint8_t, 32>& actual) noexcept
{
    if (!ShowOff184FileSha256.valid)
        return false;

    for (std::size_t index = 0; index < actual.size(); ++index)
    {
        if (actual[index] != ShowOff184FileSha256.bytes[index])
            return false;
    }
    return true;
}

constexpr ShowOff184CompatibilityAssessment rejectShowOff184(
    ShowOff184CompatibilityFailure failure) noexcept
{
    return { false, false, failure };
}

// This contract is deliberately pure: it consumes already captured values and
// performs no module discovery, process reads, process writes, or hook installs.
constexpr ShowOff184CompatibilityAssessment assessShowOff184Compatibility(
    const ShowOff184CompatibilityInput& input) noexcept
{
    if (!input.moduleLoaded)
    {
        return rejectShowOff184(
            ShowOff184CompatibilityFailure::ModuleNotLoaded);
    }
    if (!input.fileHashAvailable)
    {
        return rejectShowOff184(
            ShowOff184CompatibilityFailure::FileHashUnavailable);
    }
    if (!showOff184DigestMatches(input.fileSha256))
    {
        return rejectShowOff184(
            ShowOff184CompatibilityFailure::FileHashMismatch);
    }
    if (input.fileBytes != ShowOff184FileBytes)
    {
        return rejectShowOff184(
            ShowOff184CompatibilityFailure::FileBytesMismatch);
    }
    if (input.loadedPeTimeDateStamp != ShowOff184PeTimeDateStamp)
    {
        return rejectShowOff184(
            ShowOff184CompatibilityFailure::PeTimeDateStampMismatch);
    }
    if (input.loadedPeSizeOfImage != ShowOff184SizeOfImage)
    {
        return rejectShowOff184(
            ShowOff184CompatibilityFailure::PeSizeOfImageMismatch);
    }
    if (input.loadedPePreferredImageBase != ShowOff184PreferredImageBase)
    {
        return rejectShowOff184(
            ShowOff184CompatibilityFailure::PePreferredImageBaseMismatch);
    }

    constexpr std::uint64_t Pe32AddressLimit = 0x100000000ull;
    const std::uint64_t runtimeBase =
        static_cast<std::uint64_t>(input.runtimeModuleBase);
    if (input.runtimeModuleBytes != ShowOff184SizeOfImage
        || runtimeBase == 0u
        || runtimeBase >= Pe32AddressLimit
        || runtimeBase > Pe32AddressLimit - ShowOff184SizeOfImage)
    {
        return rejectShowOff184(
            ShowOff184CompatibilityFailure::RuntimeModuleBoundsInvalid);
    }

    if (!input.synchronousEvidenceCaptured)
    {
        return rejectShowOff184(
            ShowOff184CompatibilityFailure::SynchronousEvidenceNotCaptured);
    }
    if (!input.synchronousLoadedExecutableSectionsMatched)
    {
        return rejectShowOff184(
            ShowOff184CompatibilityFailure::LoadedExecutableSectionsUnverified);
    }
    if (!input.synchronousProtectedManifestVerified)
    {
        return rejectShowOff184(
            ShowOff184CompatibilityFailure::ProtectedManifestUnverified);
    }
    if (!input.synchronousProtectedAbiRangesVerified)
    {
        return rejectShowOff184(
            ShowOff184CompatibilityFailure::ProtectedAbiRangesUnverified);
    }
    if (!input.synchronousAllProtectedVtableSlotsVerified)
    {
        return rejectShowOff184(
            ShowOff184CompatibilityFailure::ProtectedVtableSlotsUnverified);
    }
    if (input.jipNormalizedFirstPersonApplicable
        != input.synchronousJipNormalizedFirstPersonVerified)
    {
        return rejectShowOff184(
            ShowOff184CompatibilityFailure::JipFirstPersonProofInconsistent);
    }
    if (!input.synchronousSceneGraphSingletonIntegrityVerified)
    {
        return rejectShowOff184(
            ShowOff184CompatibilityFailure::SceneGraphSingletonUnverified);
    }
    if (!input.synchronousNoProtectedTargetIntoShowOffVerified)
    {
        return rejectShowOff184(
            ShowOff184CompatibilityFailure::ProtectedTargetScanUnverified);
    }

    return {
        true,
        CompatibilityModuleInventoryProductionProofComplete,
        ShowOff184CompatibilityFailure::None,
    };
}
}
