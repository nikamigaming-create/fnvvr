#include "fnvxr_showoff_compatibility.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
using fnvxr::probe::showoff::ShowOff184CompatibilityFailure;
using fnvxr::probe::showoff::ShowOff184CompatibilityInput;

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

constexpr ShowOff184CompatibilityInput makeAcceptedJipInput()
{
    using namespace fnvxr::probe::showoff;

    ShowOff184CompatibilityInput input {};
    input.moduleLoaded = true;
    input.fileHashAvailable = true;
    input.fileSha256 = ShowOff184FileSha256.bytes;
    input.fileBytes = ShowOff184FileBytes;
    input.loadedPeTimeDateStamp = ShowOff184PeTimeDateStamp;
    input.loadedPeSizeOfImage = ShowOff184SizeOfImage;
    input.loadedPePreferredImageBase = ShowOff184PreferredImageBase;
    input.runtimeModuleBase = 0x0AE20000u;
    input.runtimeModuleBytes = ShowOff184SizeOfImage;
    input.synchronousEvidenceCaptured = true;
    input.synchronousLoadedExecutableSectionsMatched = true;
    input.synchronousProtectedManifestVerified = true;
    input.synchronousProtectedAbiRangesVerified = true;
    input.synchronousAllProtectedVtableSlotsVerified = true;
    input.jipNormalizedFirstPersonApplicable = true;
    input.synchronousJipNormalizedFirstPersonVerified = true;
    input.synchronousSceneGraphSingletonIntegrityVerified = true;
    input.synchronousNoProtectedTargetIntoShowOffVerified = true;
    return input;
}

void expectRejected(
    const ShowOff184CompatibilityInput& input,
    ShowOff184CompatibilityFailure expected,
    const char* message)
{
    const auto result =
        fnvxr::probe::showoff::assessShowOff184Compatibility(input);
    require(!result.showOff184Accepted, message);
    require(!result.compatibilityModuleInventoryProductionProofComplete, message);
    require(result.failure == expected, message);
}
}

int main()
{
    using namespace fnvxr::probe::showoff;

    static_assert(ShowOff184FileBytes == 1091584u);
    static_assert(ShowOff184PeTimeDateStamp == 0x69C84C9Bu);
    static_assert(ShowOff184SizeOfImage == 0x00110000u);
    static_assert(ShowOff184PreferredImageBase == 0x10000000u);
    static_assert(ShowOff184FileSha256.valid);
    static_assert(!CompatibilityModuleInventoryProductionProofComplete);

    constexpr ShowOff184CompatibilityInput AcceptedJip = makeAcceptedJipInput();
    constexpr auto AcceptedJipResult =
        assessShowOff184Compatibility(AcceptedJip);
    static_assert(AcceptedJipResult.showOff184Accepted);
    static_assert(!AcceptedJipResult.compatibilityModuleInventoryProductionProofComplete);
    static_assert(AcceptedJipResult.failure == ShowOff184CompatibilityFailure::None);

    ShowOff184CompatibilityInput changed = AcceptedJip;
    changed.moduleLoaded = false;
    expectRejected(
        changed,
        ShowOff184CompatibilityFailure::ModuleNotLoaded,
        "an absent module must not be mistaken for a verified ShowOff module");

    changed = AcceptedJip;
    changed.fileHashAvailable = false;
    expectRejected(
        changed,
        ShowOff184CompatibilityFailure::FileHashUnavailable,
        "a loaded module without a file digest must fail closed");

    for (std::size_t byteIndex = 0;
         byteIndex < AcceptedJip.fileSha256.size();
         ++byteIndex)
    {
        for (unsigned bit = 0; bit < 8u; ++bit)
        {
            changed = AcceptedJip;
            changed.fileSha256[byteIndex] ^= static_cast<std::uint8_t>(1u << bit);
            expectRejected(
                changed,
                ShowOff184CompatibilityFailure::FileHashMismatch,
                "every one-bit DLL digest drift must fail closed");
        }
    }

    for (unsigned bit = 0; bit < 64u; ++bit)
    {
        changed = AcceptedJip;
        changed.fileBytes ^= (std::uint64_t { 1 } << bit);
        expectRejected(
            changed,
            ShowOff184CompatibilityFailure::FileBytesMismatch,
            "every one-bit DLL file-size drift must fail closed");
    }

    for (unsigned bit = 0; bit < 32u; ++bit)
    {
        changed = AcceptedJip;
        changed.loadedPeTimeDateStamp ^= (std::uint32_t { 1 } << bit);
        expectRejected(
            changed,
            ShowOff184CompatibilityFailure::PeTimeDateStampMismatch,
            "every one-bit PE timestamp drift must fail closed");

        changed = AcceptedJip;
        changed.loadedPeSizeOfImage ^= (std::uint32_t { 1 } << bit);
        expectRejected(
            changed,
            ShowOff184CompatibilityFailure::PeSizeOfImageMismatch,
            "every one-bit PE image-size drift must fail closed");

        changed = AcceptedJip;
        changed.loadedPePreferredImageBase ^= (std::uint32_t { 1 } << bit);
        expectRejected(
            changed,
            ShowOff184CompatibilityFailure::PePreferredImageBaseMismatch,
            "every one-bit PE preferred-base drift must fail closed");
    }

    constexpr unsigned SizeBits =
        static_cast<unsigned>(sizeof(std::size_t) * 8u);
    for (unsigned bit = 0; bit < SizeBits; ++bit)
    {
        changed = AcceptedJip;
        changed.runtimeModuleBytes ^=
            (std::size_t { 1 } << bit);
        expectRejected(
            changed,
            ShowOff184CompatibilityFailure::RuntimeModuleBoundsInvalid,
            "every one-bit mapped module-size drift must fail closed");
    }

    changed = AcceptedJip;
    changed.runtimeModuleBase = 0;
    expectRejected(
        changed,
        ShowOff184CompatibilityFailure::RuntimeModuleBoundsInvalid,
        "a null mapped module base must fail closed");

    changed = AcceptedJip;
    changed.runtimeModuleBase = static_cast<std::uintptr_t>(
        0x100000000ull - ShowOff184SizeOfImage + 1u);
    expectRejected(
        changed,
        ShowOff184CompatibilityFailure::RuntimeModuleBoundsInvalid,
        "a PE32 mapped range crossing 4 GiB must fail closed");

    changed = AcceptedJip;
    changed.synchronousEvidenceCaptured = false;
    expectRejected(
        changed,
        ShowOff184CompatibilityFailure::SynchronousEvidenceNotCaptured,
        "identity without a synchronous observation must fail closed");

    changed = AcceptedJip;
    changed.synchronousLoadedExecutableSectionsMatched = false;
    expectRejected(
        changed,
        ShowOff184CompatibilityFailure::LoadedExecutableSectionsUnverified,
        "disk identity without relocation-normalized loaded executable sections must fail closed");

    changed = AcceptedJip;
    changed.synchronousProtectedManifestVerified = false;
    expectRejected(
        changed,
        ShowOff184CompatibilityFailure::ProtectedManifestUnverified,
        "an unverified protected function manifest must fail closed");

    changed = AcceptedJip;
    changed.synchronousProtectedAbiRangesVerified = false;
    expectRejected(
        changed,
        ShowOff184CompatibilityFailure::ProtectedAbiRangesUnverified,
        "unverified protected ABI ranges must fail closed");

    changed = AcceptedJip;
    changed.synchronousAllProtectedVtableSlotsVerified = false;
    expectRejected(
        changed,
        ShowOff184CompatibilityFailure::ProtectedVtableSlotsUnverified,
        "one incomplete protected-vtable proof must fail closed");

    changed = AcceptedJip;
    changed.jipNormalizedFirstPersonApplicable = false;
    expectRejected(
        changed,
        ShowOff184CompatibilityFailure::JipFirstPersonProofInconsistent,
        "a stale JIP normalization proof must fail closed");

    changed = AcceptedJip;
    changed.synchronousJipNormalizedFirstPersonVerified = false;
    expectRejected(
        changed,
        ShowOff184CompatibilityFailure::JipFirstPersonProofInconsistent,
        "an applicable but unverified JIP normalization must fail closed");

    changed = AcceptedJip;
    changed.synchronousSceneGraphSingletonIntegrityVerified = false;
    expectRejected(
        changed,
        ShowOff184CompatibilityFailure::SceneGraphSingletonUnverified,
        "an unverified scene-graph singleton must fail closed");

    changed = AcceptedJip;
    changed.synchronousNoProtectedTargetIntoShowOffVerified = false;
    expectRejected(
        changed,
        ShowOff184CompatibilityFailure::ProtectedTargetScanUnverified,
        "an incomplete protected target scan must fail closed");

    ShowOff184CompatibilityInput pristineFirstPerson = AcceptedJip;
    pristineFirstPerson.jipNormalizedFirstPersonApplicable = false;
    pristineFirstPerson.synchronousJipNormalizedFirstPersonVerified = false;
    const auto pristineResult =
        assessShowOff184Compatibility(pristineFirstPerson);
    require(
        pristineResult.showOff184Accepted,
        "the exact pristine first-person path must not require JIP normalization");
    require(
        !pristineResult.compatibilityModuleInventoryProductionProofComplete,
        "a pristine first-person path must not claim a complete module inventory");

    ShowOff184CompatibilityInput loadedButUnverified = AcceptedJip;
    loadedButUnverified.synchronousEvidenceCaptured = false;
    loadedButUnverified.synchronousLoadedExecutableSectionsMatched = false;
    loadedButUnverified.synchronousProtectedManifestVerified = false;
    loadedButUnverified.synchronousProtectedAbiRangesVerified = false;
    loadedButUnverified.synchronousAllProtectedVtableSlotsVerified = false;
    loadedButUnverified.synchronousJipNormalizedFirstPersonVerified = false;
    loadedButUnverified.synchronousSceneGraphSingletonIntegrityVerified = false;
    loadedButUnverified.synchronousNoProtectedTargetIntoShowOffVerified = false;
    expectRejected(
        loadedButUnverified,
        ShowOff184CompatibilityFailure::SynchronousEvidenceNotCaptured,
        "an exact loaded DLL with no runtime proof must fail closed");

    loadedButUnverified.synchronousEvidenceCaptured = true;
    expectRejected(
        loadedButUnverified,
        ShowOff184CompatibilityFailure::LoadedExecutableSectionsUnverified,
        "a synchronous shell without loaded executable-section proof must fail closed");

    loadedButUnverified.synchronousLoadedExecutableSectionsMatched = true;
    expectRejected(
        loadedButUnverified,
        ShowOff184CompatibilityFailure::ProtectedManifestUnverified,
        "a synchronous shell with no completed proof bits must fail closed");

    expectRejected(
        ShowOff184CompatibilityInput {},
        ShowOff184CompatibilityFailure::ModuleNotLoaded,
        "the default contract input must fail closed");

    std::cout << "ShowOff 1.84 pure fail-closed compatibility contract passed\n";
    return EXIT_SUCCESS;
}
