#include "fnvxr_mapped_image_proof.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
using namespace fnvxr::probe::mapped_image;

constexpr std::size_t NtOffset = 0x80u;
constexpr std::size_t OptionalOffset = NtOffset + 24u;
constexpr std::size_t SectionTableOffset = OptionalOffset + 224u;
constexpr std::size_t TextSectionHeader = SectionTableOffset;
constexpr std::size_t RelocSectionHeader = SectionTableOffset + 40u;
constexpr std::uint32_t PreferredBase = 0x10000000u;
constexpr std::uint32_t RuntimeBase = 0x20000000u;
constexpr std::uint32_t TextRva = 0x1000u;
constexpr std::uint32_t TextVirtualBytes = 0x240u;
constexpr std::uint32_t TextExecutableProtectionBytes = 0x1000u;
constexpr std::uint32_t TextRawBytes = 0x200u;
constexpr std::uint32_t TextRawOffset = 0x200u;
constexpr std::uint32_t RelocRva = 0x2000u;
constexpr std::uint32_t RelocRawBytes = 0x200u;
constexpr std::uint32_t RelocRawOffset = 0x400u;
constexpr std::uint32_t ImageBytes = 0x3000u;
constexpr std::uint32_t RelocatedPointerRva = TextRva + 0x10u;

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void writeU16(
    std::vector<std::uint8_t>& bytes,
    std::size_t offset,
    std::uint16_t value)
{
    require(offset <= bytes.size() && 2u <= bytes.size() - offset,
        "test fixture 16-bit write must be in bounds");
    bytes[offset] = static_cast<std::uint8_t>(value & 0xFFu);
    bytes[offset + 1u] =
        static_cast<std::uint8_t>((value >> 8u) & 0xFFu);
}

void writeU32(
    std::vector<std::uint8_t>& bytes,
    std::size_t offset,
    std::uint32_t value)
{
    require(offset <= bytes.size() && 4u <= bytes.size() - offset,
        "test fixture 32-bit write must be in bounds");
    bytes[offset] = static_cast<std::uint8_t>(value & 0xFFu);
    bytes[offset + 1u] =
        static_cast<std::uint8_t>((value >> 8u) & 0xFFu);
    bytes[offset + 2u] =
        static_cast<std::uint8_t>((value >> 16u) & 0xFFu);
    bytes[offset + 3u] =
        static_cast<std::uint8_t>((value >> 24u) & 0xFFu);
}

struct Fixture
{
    std::vector<std::uint8_t> disk;
    std::vector<std::uint8_t> loaded;
    std::vector<LoadedMemoryProtectionRange> protections;
    CaptureEvidence evidence;
};

Fixture makeFixture()
{
    Fixture fixture {
        std::vector<std::uint8_t>(0x600u, 0u),
        std::vector<std::uint8_t>(ImageBytes, 0u),
        {},
        {},
    };

    fixture.disk[0] = static_cast<std::uint8_t>('M');
    fixture.disk[1] = static_cast<std::uint8_t>('Z');
    writeU32(fixture.disk, 0x3Cu, static_cast<std::uint32_t>(NtOffset));
    fixture.disk[NtOffset] = static_cast<std::uint8_t>('P');
    fixture.disk[NtOffset + 1u] = static_cast<std::uint8_t>('E');
    writeU16(fixture.disk, NtOffset + 4u, Pe32MachineI386);
    writeU16(fixture.disk, NtOffset + 6u, 2u);
    writeU16(fixture.disk, NtOffset + 20u, 224u);

    writeU16(fixture.disk, OptionalOffset, Pe32OptionalHeaderMagic);
    writeU32(fixture.disk, OptionalOffset + 28u, PreferredBase);
    writeU32(fixture.disk, OptionalOffset + 32u, 0x1000u);
    writeU32(fixture.disk, OptionalOffset + 36u, 0x200u);
    writeU32(fixture.disk, OptionalOffset + 56u, ImageBytes);
    writeU32(fixture.disk, OptionalOffset + 60u, 0x200u);
    writeU32(fixture.disk, OptionalOffset + 92u, 16u);
    writeU32(fixture.disk, OptionalOffset + 96u + 5u * 8u, RelocRva);
    writeU32(fixture.disk, OptionalOffset + 96u + 5u * 8u + 4u, 12u);

    writeU32(fixture.disk, TextSectionHeader + 8u, TextVirtualBytes);
    writeU32(fixture.disk, TextSectionHeader + 12u, TextRva);
    writeU32(fixture.disk, TextSectionHeader + 16u, TextRawBytes);
    writeU32(fixture.disk, TextSectionHeader + 20u, TextRawOffset);
    writeU32(
        fixture.disk,
        TextSectionHeader + 36u,
        SectionMemExecute | 0x60000020u);

    writeU32(fixture.disk, RelocSectionHeader + 8u, 0x100u);
    writeU32(fixture.disk, RelocSectionHeader + 12u, RelocRva);
    writeU32(fixture.disk, RelocSectionHeader + 16u, RelocRawBytes);
    writeU32(fixture.disk, RelocSectionHeader + 20u, RelocRawOffset);
    writeU32(fixture.disk, RelocSectionHeader + 36u, 0x42000040u);

    for (std::uint32_t index = 0; index < TextRawBytes; ++index)
    {
        fixture.disk[TextRawOffset + index] =
            static_cast<std::uint8_t>((index * 37u + 11u) & 0xFFu);
    }
    writeU32(
        fixture.disk,
        TextRawOffset + (RelocatedPointerRva - TextRva),
        PreferredBase + 0x1234u);

    writeU32(fixture.disk, RelocRawOffset, TextRva);
    writeU32(fixture.disk, RelocRawOffset + 4u, 12u);
    writeU16(fixture.disk, RelocRawOffset + 8u, 0x3010u);
    writeU16(fixture.disk, RelocRawOffset + 10u, 0u);

    for (std::uint32_t index = 0; index < TextRawBytes; ++index)
    {
        fixture.loaded[TextRva + index] =
            fixture.disk[TextRawOffset + index];
    }
    for (std::uint32_t index = 0; index < RelocRawBytes; ++index)
    {
        fixture.loaded[RelocRva + index] =
            fixture.disk[RelocRawOffset + index];
    }
    writeU32(
        fixture.loaded,
        RelocatedPointerRva,
        RuntimeBase + 0x1234u);

    fixture.protections = {
        {
            0u,
            Pe32X86PageBytes,
            LoadedMemoryState::Committed,
            LoadedMemoryProtection::ReadOnly,
            false,
        },
        {
            TextRva,
            TextExecutableProtectionBytes,
            LoadedMemoryState::Committed,
            LoadedMemoryProtection::ExecuteRead,
            false,
        },
        {
            RelocRva,
            Pe32X86PageBytes,
            LoadedMemoryState::Committed,
            LoadedMemoryProtection::ReadOnly,
            false,
        },
    };

    fixture.evidence.exactDiskFileIdentityVerified = true;
    fixture.evidence.diskFileReadComplete = true;
    fixture.evidence.loadedImageSnapshotComplete = true;
    fixture.evidence.loadedImageSnapshotStableBeforeAfter = true;
    fixture.evidence.loadedMemoryProtectionSnapshotStableBeforeAfter = true;
    fixture.evidence.executableSectionCoverageComplete = true;
    fixture.evidence.relocationDirectoryCoverageComplete = true;
    fixture.evidence.exactPatchInventoryComplete = true;
    return fixture;
}

MappedImageProofInput makeInput(const Fixture& fixture)
{
    MappedImageProofInput input {};
    input.exactDiskFile = { fixture.disk.data(), fixture.disk.size() };
    input.loadedImageSnapshot = {
        fixture.loaded.data(),
        fixture.loaded.size(),
    };
    input.runtimeImageBase = RuntimeBase;
    input.memoryProtectionRanges = fixture.protections.data();
    input.memoryProtectionRangeCount = fixture.protections.size();
    input.evidence = fixture.evidence;
    return input;
}

void expectFailure(
    const MappedImageProofInput& input,
    MappedImageProofFailure expected,
    const char* message)
{
    const MappedImageProofAssessment result =
        proveLoadedExecutableSections(input);
    require(!result.loadedExecutableSectionsMatched, message);
    require(!result.productionAuthorized, message);
    require(result.failure == expected, message);
}

template <std::size_t Size>
BytesView view(const std::array<std::uint8_t, Size>& bytes)
{
    return { bytes.data(), bytes.size() };
}
}

int main()
{
    static_assert(!ProductionAuthorizationEnabled);
    static_assert(!ShowOffPatchAllowlistProductionProven);

    const Fixture pristine = makeFixture();
    const MappedImageProofAssessment pristineResult =
        proveLoadedExecutableSections(makeInput(pristine));
    require(
        pristineResult.loadedExecutableSectionsMatched,
        "a complete stable relocation-normalized executable snapshot must match");
    require(
        !pristineResult.productionAuthorized,
        "the isolated byte proof must never enable production by itself");
    require(pristineResult.failure == MappedImageProofFailure::None,
        "a matching image must have no proof failure");
    require(pristineResult.executableSectionCount == 1u,
        "the proof must account for the one executable section");
    require(
        pristineResult.executableBytesCompared
            == TextExecutableProtectionBytes,
        "the proof must compare the complete executable protection page");
    require(
        pristineResult.executableProtectionBytesVerified
            == TextExecutableProtectionBytes,
        "every compared byte must have committed executable protection proof");
    require(pristineResult.memoryProtectionRangeCount == 3u,
        "the complete loaded memory-protection partition must be consumed");
    require(pristineResult.highLowRelocationCount == 1u,
        "the proof must normalize the one exact HIGHLOW relocation");
    require(pristineResult.exactPatchCount == 0u,
        "a pristine image must require no patch allowances");

    Fixture changed = pristine;
    MappedImageProofInput changedInput = makeInput(changed);
    changedInput.evidence.exactDiskFileIdentityVerified = false;
    expectFailure(
        changedInput,
        MappedImageProofFailure::ExactDiskIdentityUnverified,
        "disk bytes without exact external identity proof must fail closed");
    changedInput = makeInput(changed);
    changedInput.evidence.diskFileReadComplete = false;
    expectFailure(
        changedInput,
        MappedImageProofFailure::DiskFileReadIncomplete,
        "a partial disk read must fail closed");
    changedInput = makeInput(changed);
    changedInput.evidence.loadedImageSnapshotComplete = false;
    expectFailure(
        changedInput,
        MappedImageProofFailure::LoadedSnapshotIncomplete,
        "an incomplete loaded snapshot must fail closed");
    changedInput = makeInput(changed);
    changedInput.evidence.loadedImageSnapshotStableBeforeAfter = false;
    expectFailure(
        changedInput,
        MappedImageProofFailure::LoadedSnapshotUnstable,
        "a snapshot without stable before/after evidence must fail closed");
    changedInput = makeInput(changed);
    changedInput.evidence.loadedMemoryProtectionSnapshotStableBeforeAfter =
        false;
    expectFailure(
        changedInput,
        MappedImageProofFailure::LoadedProtectionSnapshotUnstable,
        "an unstable memory-protection census must fail closed");
    changedInput = makeInput(changed);
    changedInput.evidence.executableSectionCoverageComplete = false;
    expectFailure(
        changedInput,
        MappedImageProofFailure::ExecutableCoverageIncomplete,
        "incomplete executable-section coverage must fail closed");
    changedInput = makeInput(changed);
    changedInput.evidence.relocationDirectoryCoverageComplete = false;
    expectFailure(
        changedInput,
        MappedImageProofFailure::RelocationCoverageIncomplete,
        "incomplete relocation coverage must fail closed");
    changedInput = makeInput(changed);
    changedInput.evidence.exactPatchInventoryComplete = false;
    expectFailure(
        changedInput,
        MappedImageProofFailure::PatchInventoryIncomplete,
        "an incomplete patch census must fail closed even when no patch was seen");

    changedInput = makeInput(pristine);
    changedInput.patchAllowances = nullptr;
    changedInput.patchAllowanceCount = MaximumPatchAllowanceCount + 1u;
    expectFailure(
        changedInput,
        MappedImageProofFailure::PatchCountOutOfBounds,
        "the patch count must be bounded before its pointer can be dereferenced");
    changedInput = makeInput(pristine);
    changedInput.memoryProtectionRanges = nullptr;
    changedInput.memoryProtectionRangeCount =
        MaximumMemoryProtectionRangeCount + 1u;
    expectFailure(
        changedInput,
        MappedImageProofFailure::ProtectionRangeCountOutOfBounds,
        "the protection count must be bounded before its pointer is used");
    changedInput = makeInput(pristine);
    changedInput.memoryProtectionRanges = nullptr;
    changedInput.memoryProtectionRangeCount = 0u;
    expectFailure(
        changedInput,
        MappedImageProofFailure::ProtectionMapMissing,
        "a generic coverage boolean without explicit protection records must fail");

    changed = pristine;
    changed.protections[1].protection = LoadedMemoryProtection::ReadOnly;
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::ExpectedExecutableProtectionMissing,
        "every compared executable page must be committed executable");
    changed = pristine;
    changed.protections[1].state = LoadedMemoryState::Reserved;
    changed.protections[1].protection = LoadedMemoryProtection::NoAccess;
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::ExpectedExecutableProtectionMissing,
        "reserved address space must not satisfy executable protection proof");
    changed = pristine;
    changed.protections[1].protection =
        LoadedMemoryProtection::ExecuteReadWrite;
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::WritableExecutableProtection,
        "RWX executable image pages must be rejected");
    changed = pristine;
    changed.protections[0].protection = LoadedMemoryProtection::ExecuteRead;
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::UnexpectedExecutableProtection,
        "executable image headers outside an executable section must be rejected");
    changed = pristine;
    changed.protections[2].protection = LoadedMemoryProtection::ExecuteRead;
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::UnexpectedExecutableProtection,
        "an unexpected executable data/relocation page must be rejected");
    changed = pristine;
    changed.protections[1].guard = true;
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::MalformedProtectionRange,
        "guarded image pages must not be treated as stable protection evidence");
    changedInput = makeInput(pristine);
    changedInput.memoryProtectionRangeCount = 2u;
    expectFailure(
        changedInput,
        MappedImageProofFailure::IncompleteProtectionMap,
        "a protection census that stops before SizeOfImage must fail closed");
    changed = pristine;
    changed.protections[1].rva += Pe32X86PageBytes;
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::MalformedProtectionRange,
        "gapped or overlapping protection records must be rejected");

    changed = pristine;
    changed.loaded[TextRva + TextVirtualBytes] ^= 0x01u;
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::ExecutableByteMismatch,
        "the first byte beyond VirtualSize on an executable page must be proved");
    changed = pristine;
    changed.loaded[RelocRva - 1u] ^= 0x01u;
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::ExecutableByteMismatch,
        "the final byte before the next section must be proved when executable");
    changed = pristine;
    changed.loaded[RelocRva] ^= 0x01u;
    require(
        proveLoadedExecutableSections(makeInput(changed))
            .loadedExecutableSectionsMatched,
        "the first byte of the non-executable next section is not misclassified");

    // This exhaustive one-bit sample covers raw bytes, VirtualSize zero-fill,
    // and all executable page-alignment padding. No executable byte escapes.
    for (std::uint32_t relative = 0;
         relative < TextExecutableProtectionBytes;
         ++relative)
    {
        changed = pristine;
        changed.loaded[TextRva + relative] ^= 0x01u;
        expectFailure(
            makeInput(changed),
            MappedImageProofFailure::ExecutableByteMismatch,
            "every changed executable byte must be detected");
    }

    changed = pristine;
    changed.loaded[RelocRva + 0x40u] ^= 0x01u;
    require(
        proveLoadedExecutableSections(makeInput(changed))
            .loadedExecutableSectionsMatched,
        "non-executable section bytes are outside this exact executable proof");

    changed = pristine;
    changed.disk[0] = 0u;
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::MalformedDosHeader,
        "a malformed DOS header must be rejected");
    changed = pristine;
    writeU16(changed.disk, NtOffset + 4u, 0x8664u);
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::UnsupportedMachine,
        "a non-I386 image must be rejected");
    changed = pristine;
    writeU32(changed.disk, OptionalOffset + 32u, 0x200u);
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::UnsupportedProtectionGeometry,
        "low-alignment images with unlike sections sharing pages must fail closed");
    changed = pristine;
    writeU32(changed.disk, OptionalOffset + 32u, 0x2000u);
    writeU32(changed.disk, OptionalOffset + 56u, 0x4000u);
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::UnsupportedProtectionGeometry,
        "nonstandard larger section alignment must await exact loader-gap proof");
    changed = pristine;
    writeU32(
        changed.disk,
        TextSectionHeader + 36u,
        0xE0000020u);
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::WritableExecutableSection,
        "a PE section requesting writable executable protection must be rejected");
    changed = pristine;
    writeU32(changed.disk, RelocSectionHeader + 12u, TextRva);
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::OverlappingSections,
        "overlapping mapped section ranges must be rejected");
    changed = pristine;
    writeU32(changed.disk, RelocSectionHeader + 20u, TextRawOffset);
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::OverlappingSections,
        "overlapping raw section ranges must be rejected");
    changed = pristine;
    writeU32(changed.disk, TextSectionHeader + 12u, ImageBytes);
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::SectionRangeOutOfBounds,
        "a virtual section crossing SizeOfImage must be rejected");
    changed = pristine;
    writeU32(changed.disk, RelocSectionHeader + 20u, 0x600u);
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::SectionRangeOutOfBounds,
        "a raw section crossing the exact disk buffer must be rejected");

    changed = pristine;
    writeU32(changed.disk, RelocRawOffset + 4u, 10u);
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::MalformedRelocationBlock,
        "a mis-sized relocation block must be rejected");
    changed = pristine;
    writeU16(changed.disk, RelocRawOffset + 8u, 0x5010u);
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::UnsupportedRelocationType,
        "an unimplemented relocation type must be rejected, not ignored");
    changed = pristine;
    writeU32(changed.disk, RelocRawOffset, ImageBytes);
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::RelocationTargetOutOfBounds,
        "a relocation target outside every mapped section must be rejected");
    changed = pristine;
    writeU16(changed.disk, RelocRawOffset + 10u, 0x3010u);
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::DuplicateRelocationTarget,
        "duplicate relocation coverage must be rejected");
    changed = pristine;
    writeU16(changed.disk, RelocRawOffset + 10u, 0x3012u);
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::DuplicateRelocationTarget,
        "partially overlapping HIGHLOW relocation ranges must be rejected");
    changed = pristine;
    writeU32(
        changed.disk,
        TextRawOffset + (RelocatedPointerRva - TextRva),
        0xF0001234u);
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::RelocationArithmeticOverflow,
        "relocation arithmetic outside PE32 address space must be rejected");
    changed = pristine;
    writeU32(changed.disk, OptionalOffset + 96u + 5u * 8u, 0u);
    writeU32(changed.disk, OptionalOffset + 96u + 5u * 8u + 4u, 0u);
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::RelocationDirectoryMissing,
        "a rebased image without relocation coverage must be rejected");
    changed = pristine;
    writeU32(changed.disk, OptionalOffset + 96u + 5u * 8u, RelocRva + 0x1F8u);
    expectFailure(
        makeInput(changed),
        MappedImageProofFailure::RelocationDirectoryOutOfBounds,
        "a relocation directory crossing its file-backed section must be rejected");

    constexpr std::uint32_t PatchRva = TextRva + 0x30u;
    std::array<std::uint8_t, 5> exactOriginal {};
    for (std::size_t index = 0; index < exactOriginal.size(); ++index)
        exactOriginal[index] = pristine.loaded[PatchRva + index];
    const std::array<std::uint8_t, 5> exactReplacement {
        0xE9u, 0x44u, 0x33u, 0x22u, 0x11u,
    };
    constexpr char JipPatchDescription[] =
        "JIP 57.30 exact synthetic rel32 rewrite";
    ExactPatchAllowance jipPatch {
        PatchAuthority::Jip,
        PatchRva,
        view(exactOriginal),
        view(exactReplacement),
        JipPatchDescription,
        sizeof(JipPatchDescription) - 1u,
    };

    changed = pristine;
    for (std::size_t index = 0; index < exactReplacement.size(); ++index)
        changed.loaded[PatchRva + index] = exactReplacement[index];
    changed.evidence.exactJipPatchAuthorityVerified = true;
    changedInput = makeInput(changed);
    changedInput.patchAllowances = &jipPatch;
    changedInput.patchAllowanceCount = 1u;
    const MappedImageProofAssessment jipResult =
        proveLoadedExecutableSections(changedInput);
    require(jipResult.loadedExecutableSectionsMatched,
        "one exact, described, authority-proven JIP rewrite must normalize");
    require(!jipResult.productionAuthorized,
        "a successful JIP byte normalization must not activate production");
    require(jipResult.exactPatchCount == 1u,
        "the exact JIP patch must be counted once");
    require(jipResult.exactPatchBytesCompared == exactReplacement.size(),
        "every exact patch byte must be counted");

    changed.evidence.exactJipPatchAuthorityVerified = false;
    changedInput = makeInput(changed);
    changedInput.patchAllowances = &jipPatch;
    changedInput.patchAllowanceCount = 1u;
    expectFailure(
        changedInput,
        MappedImageProofFailure::PatchAuthorityUnverified,
        "a byte-shaped patch without exact JIP authority proof must fail closed");

    changed = pristine;
    for (std::size_t index = 0; index < exactReplacement.size(); ++index)
        changed.loaded[PatchRva + index] = exactReplacement[index];
    changed.evidence.exactJipPatchAuthorityVerified = true;
    ExactPatchAllowance malformedPatch = jipPatch;
    malformedPatch.description = nullptr;
    malformedPatch.descriptionBytes = 0u;
    changedInput = makeInput(changed);
    changedInput.patchAllowances = &malformedPatch;
    changedInput.patchAllowanceCount = 1u;
    expectFailure(
        changedInput,
        MappedImageProofFailure::PatchDescriptionInvalid,
        "a JIP patch without an explicit human description must be rejected");

    constexpr char VagueDescription[] = "patch";
    malformedPatch = jipPatch;
    malformedPatch.description = VagueDescription;
    malformedPatch.descriptionBytes = sizeof(VagueDescription) - 1u;
    changedInput.patchAllowances = &malformedPatch;
    expectFailure(
        changedInput,
        MappedImageProofFailure::PatchDescriptionInvalid,
        "a vague underspecified JIP patch description must be rejected");

    malformedPatch = jipPatch;
    malformedPatch.exactUnpatchedBytes.data = nullptr;
    changedInput.patchAllowances = &malformedPatch;
    expectFailure(
        changedInput,
        MappedImageProofFailure::PatchArrayInvalid,
        "a malformed exact-byte patch descriptor must be rejected");

    malformedPatch = jipPatch;
    malformedPatch.authority = PatchAuthority::Unknown;
    changedInput.patchAllowances = &malformedPatch;
    expectFailure(
        changedInput,
        MappedImageProofFailure::PatchAuthorityUnverified,
        "an exact patch with unknown provenance must be rejected");

    malformedPatch = jipPatch;
    std::array<std::uint8_t, 5> wrongOriginal = exactOriginal;
    wrongOriginal[0] ^= 0x01u;
    malformedPatch.exactUnpatchedBytes = view(wrongOriginal);
    changedInput.patchAllowances = &malformedPatch;
    expectFailure(
        changedInput,
        MappedImageProofFailure::PatchOriginalBytesMismatch,
        "a broad or stale original-byte allowance must be rejected");

    malformedPatch = jipPatch;
    std::array<std::uint8_t, 5> wrongReplacement = exactReplacement;
    wrongReplacement.back() ^= 0x01u;
    malformedPatch.exactPatchedBytes = view(wrongReplacement);
    changedInput.patchAllowances = &malformedPatch;
    expectFailure(
        changedInput,
        MappedImageProofFailure::PatchReplacementBytesMismatch,
        "a replacement-byte drift must be rejected");

    changed = pristine;
    changed.evidence.exactJipPatchAuthorityVerified = true;
    malformedPatch = jipPatch;
    malformedPatch.exactPatchedBytes = view(exactOriginal);
    changedInput = makeInput(changed);
    changedInput.patchAllowances = &malformedPatch;
    changedInput.patchAllowanceCount = 1u;
    expectFailure(
        changedInput,
        MappedImageProofFailure::PatchReplacementInvalid,
        "an allowance that changes no bytes must be rejected as meaningless");

    changed = pristine;
    changed.evidence.exactJipPatchAuthorityVerified = true;
    std::array<std::uint8_t, 4> relocationOriginal {};
    for (std::size_t index = 0; index < relocationOriginal.size(); ++index)
        relocationOriginal[index] = pristine.loaded[RelocatedPointerRva + index];
    std::array<std::uint8_t, 4> relocationReplacement = relocationOriginal;
    relocationReplacement[0] ^= 0x01u;
    ExactPatchAllowance relocationPatch {
        PatchAuthority::Jip,
        RelocatedPointerRva,
        view(relocationOriginal),
        view(relocationReplacement),
        JipPatchDescription,
        sizeof(JipPatchDescription) - 1u,
    };
    for (std::size_t index = 0; index < relocationReplacement.size(); ++index)
    {
        changed.loaded[RelocatedPointerRva + index] =
            relocationReplacement[index];
    }
    changedInput = makeInput(changed);
    changedInput.patchAllowances = &relocationPatch;
    changedInput.patchAllowanceCount = 1u;
    expectFailure(
        changedInput,
        MappedImageProofFailure::PatchOverlapsRelocation,
        "patch and relocation normalization ranges must never overlap");

    changed = pristine;
    changed.evidence.exactJipPatchAuthorityVerified = true;
    std::array<std::uint8_t, 5> nonExecutableOriginal {};
    for (std::size_t index = 0; index < nonExecutableOriginal.size(); ++index)
        nonExecutableOriginal[index] = pristine.loaded[RelocRva + 0x20u + index];
    std::array<std::uint8_t, 5> nonExecutableReplacement =
        nonExecutableOriginal;
    nonExecutableReplacement[0] ^= 0x01u;
    ExactPatchAllowance nonExecutablePatch {
        PatchAuthority::Jip,
        RelocRva + 0x20u,
        view(nonExecutableOriginal),
        view(nonExecutableReplacement),
        JipPatchDescription,
        sizeof(JipPatchDescription) - 1u,
    };
    changedInput = makeInput(changed);
    changedInput.patchAllowances = &nonExecutablePatch;
    changedInput.patchAllowanceCount = 1u;
    expectFailure(
        changedInput,
        MappedImageProofFailure::PatchRangeNotExecutable,
        "an allowance outside executable coverage must be rejected");

    std::array<ExactPatchAllowance, 2> overlappingPatches { jipPatch, jipPatch };
    overlappingPatches[1].rva = PatchRva + 1u;
    std::array<std::uint8_t, 5> overlapOriginal {};
    std::array<std::uint8_t, 5> overlapReplacement {};
    for (std::size_t index = 0; index < overlapOriginal.size(); ++index)
    {
        overlapOriginal[index] = pristine.loaded[PatchRva + 1u + index];
        overlapReplacement[index] =
            static_cast<std::uint8_t>(0x80u + index);
    }
    overlappingPatches[1].exactUnpatchedBytes = view(overlapOriginal);
    overlappingPatches[1].exactPatchedBytes = view(overlapReplacement);
    changed = pristine;
    changed.evidence.exactJipPatchAuthorityVerified = true;
    for (std::size_t index = 0; index < exactReplacement.size(); ++index)
        changed.loaded[PatchRva + index] = exactReplacement[index];
    changedInput = makeInput(changed);
    changedInput.patchAllowances = overlappingPatches.data();
    changedInput.patchAllowanceCount = overlappingPatches.size();
    expectFailure(
        changedInput,
        MappedImageProofFailure::OverlappingPatches,
        "overlapping exact patch rules must be rejected");

    malformedPatch = jipPatch;
    malformedPatch.rva = 0xFFFFFFFFu;
    changedInput.patchAllowances = &malformedPatch;
    changedInput.patchAllowanceCount = 1u;
    expectFailure(
        changedInput,
        MappedImageProofFailure::PatchRangeOutOfBounds,
        "an overflowing patch range must be rejected");

    changed = pristine;
    for (std::size_t index = 0; index < exactReplacement.size(); ++index)
        changed.loaded[PatchRva + index] = exactReplacement[index];
    changed.evidence.exactShowOffPatchAuthorityVerified = true;
    ExactPatchAllowance showOffPatch = jipPatch;
    showOffPatch.authority = PatchAuthority::ShowOff;
    changedInput = makeInput(changed);
    changedInput.patchAllowances = &showOffPatch;
    changedInput.patchAllowanceCount = 1u;
    expectFailure(
        changedInput,
        MappedImageProofFailure::ShowOffPatchAllowlistUnproven,
        "ShowOff must have no patch allowances until a checked-in set is proven");

    std::cout << "pure PE32 mapped executable image proof contract passed\n";
    return EXIT_SUCCESS;
}
