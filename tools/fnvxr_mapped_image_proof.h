#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace fnvxr::probe::mapped_image
{
// This file is a pure byte-validation contract. It performs no process access,
// file access, writes, hooks, loading, or activation. Production callers must
// capture both byte ranges and every evidence fact independently.
inline constexpr bool ProductionAuthorizationEnabled = false;

// ShowOff has no established code-patch allowlist. Changing this value requires
// checked-in, exact-version evidence for every allowed site and replacement.
inline constexpr bool ShowOffPatchAllowlistProductionProven = false;

inline constexpr std::uint16_t Pe32MachineI386 = 0x014Cu;
inline constexpr std::uint16_t Pe32OptionalHeaderMagic = 0x010Bu;
inline constexpr std::uint32_t Pe32X86PageBytes = 0x1000u;
inline constexpr std::uint32_t SectionMemExecute = 0x20000000u;
inline constexpr std::uint32_t SectionMemWrite = 0x80000000u;
inline constexpr std::uint16_t RelocationAbsolute = 0u;
inline constexpr std::uint16_t RelocationHighLow = 3u;
inline constexpr std::size_t MinimumPatchDescriptionBytes = 8u;
inline constexpr std::size_t MaximumPatchDescriptionBytes = 256u;
inline constexpr std::size_t MaximumPatchAllowanceCount = 4096u;
inline constexpr std::size_t MaximumMemoryProtectionRangeCount = 65536u;

struct BytesView
{
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
};

enum class PatchAuthority
{
    Unknown,
    Jip,
    ShowOff,
};

// The unpatched bytes are the exact post-relocation bytes expected at the site,
// not a wildcard or a mask. The replacement is likewise exact. A description
// is mandatory so a JIP allowance names why that exact rewrite exists.
struct ExactPatchAllowance
{
    PatchAuthority authority = PatchAuthority::Unknown;
    std::uint32_t rva = 0;
    BytesView exactUnpatchedBytes {};
    BytesView exactPatchedBytes {};
    const char* description = nullptr;
    std::size_t descriptionBytes = 0;
};

enum class LoadedMemoryState
{
    Unknown,
    Reserved,
    Committed,
};

enum class LoadedMemoryProtection
{
    Unknown,
    NoAccess,
    ReadOnly,
    ReadWrite,
    WriteCopy,
    Execute,
    ExecuteRead,
    ExecuteReadWrite,
    ExecuteWriteCopy,
};

// The caller translates one stable VirtualQuery-style census into a complete,
// ordered partition of [0, SizeOfImage). The proof does not query memory.
struct LoadedMemoryProtectionRange
{
    std::uint32_t rva = 0;
    std::uint32_t byteCount = 0;
    LoadedMemoryState state = LoadedMemoryState::Unknown;
    LoadedMemoryProtection protection = LoadedMemoryProtection::Unknown;
    bool guard = false;
};

struct CaptureEvidence
{
    bool exactDiskFileIdentityVerified = false;
    bool diskFileReadComplete = false;
    bool loadedImageSnapshotComplete = false;
    bool loadedImageSnapshotStableBeforeAfter = false;
    bool loadedMemoryProtectionSnapshotStableBeforeAfter = false;
    bool executableSectionCoverageComplete = false;
    bool relocationDirectoryCoverageComplete = false;
    bool exactPatchInventoryComplete = false;

    // A patch allowance also requires an independently proven exact producer
    // identity. These facts default false and do not discover that identity.
    bool exactJipPatchAuthorityVerified = false;
    bool exactShowOffPatchAuthorityVerified = false;
};

struct MappedImageProofInput
{
    BytesView exactDiskFile {};
    BytesView loadedImageSnapshot {};
    std::uint64_t runtimeImageBase = 0;
    const LoadedMemoryProtectionRange* memoryProtectionRanges = nullptr;
    std::size_t memoryProtectionRangeCount = 0;
    const ExactPatchAllowance* patchAllowances = nullptr;
    std::size_t patchAllowanceCount = 0;
    CaptureEvidence evidence {};
};

enum class MappedImageProofFailure
{
    None,
    ExactDiskIdentityUnverified,
    DiskFileReadIncomplete,
    LoadedSnapshotIncomplete,
    LoadedSnapshotUnstable,
    LoadedProtectionSnapshotUnstable,
    ExecutableCoverageIncomplete,
    RelocationCoverageIncomplete,
    PatchInventoryIncomplete,
    InputRangeInvalid,
    MalformedDosHeader,
    MalformedNtHeaders,
    UnsupportedMachine,
    UnsupportedOptionalHeader,
    InvalidImageGeometry,
    InvalidSectionTable,
    SectionRangeOutOfBounds,
    OverlappingSections,
    NoExecutableSections,
    UnsupportedProtectionGeometry,
    WritableExecutableSection,
    RuntimeImageBoundsInvalid,
    ProtectionRangeCountOutOfBounds,
    ProtectionMapMissing,
    MalformedProtectionRange,
    IncompleteProtectionMap,
    WritableExecutableProtection,
    ExpectedExecutableProtectionMissing,
    UnexpectedExecutableProtection,
    RelocationDirectoryMissing,
    RelocationDirectoryOutOfBounds,
    MalformedRelocationBlock,
    UnsupportedRelocationType,
    RelocationTargetOutOfBounds,
    DuplicateRelocationTarget,
    RelocationArithmeticOverflow,
    PatchCountOutOfBounds,
    PatchArrayInvalid,
    PatchDescriptionInvalid,
    PatchAuthorityUnverified,
    ShowOffPatchAllowlistUnproven,
    PatchRangeOutOfBounds,
    PatchRangeNotExecutable,
    OverlappingPatches,
    PatchOverlapsRelocation,
    PatchOriginalBytesMismatch,
    PatchReplacementInvalid,
    PatchReplacementBytesMismatch,
    ExecutableByteMismatch,
};

struct MappedImageProofAssessment
{
    bool loadedExecutableSectionsMatched = false;
    bool productionAuthorized = false;
    MappedImageProofFailure failure =
        MappedImageProofFailure::ExactDiskIdentityUnverified;
    std::size_t executableSectionCount = 0;
    std::size_t executableBytesCompared = 0;
    std::size_t executableProtectionBytesVerified = 0;
    std::size_t memoryProtectionRangeCount = 0;
    std::size_t highLowRelocationCount = 0;
    std::size_t exactPatchCount = 0;
    std::size_t exactPatchBytesCompared = 0;
};

namespace detail
{
struct Section
{
    std::uint32_t virtualAddress = 0;
    // Bytes defined by the PE section's initialized/virtual content.
    std::uint32_t mappedBytes = 0;
    // Page-rounded bytes that can inherit this section's executable protection,
    // clipped at the next section or SizeOfImage.
    std::uint32_t protectionBytes = 0;
    std::uint32_t rawBytes = 0;
    std::uint32_t rawOffset = 0;
    std::uint32_t characteristics = 0;

    bool executable() const noexcept
    {
        return (characteristics & SectionMemExecute) != 0u;
    }

    bool writable() const noexcept
    {
        return (characteristics & SectionMemWrite) != 0u;
    }
};

struct ParsedPe32
{
    std::uint32_t preferredImageBase = 0;
    std::uint32_t sectionAlignment = 0;
    std::uint32_t fileAlignment = 0;
    std::uint32_t sizeOfImage = 0;
    std::uint32_t sizeOfHeaders = 0;
    std::uint32_t relocationDirectoryRva = 0;
    std::uint32_t relocationDirectoryBytes = 0;
    std::vector<Section> sections {};
};

struct Relocation
{
    std::uint32_t rva = 0;
};

inline bool rangeWithin(
    std::size_t total,
    std::size_t offset,
    std::size_t bytes) noexcept
{
    return offset <= total && bytes <= total - offset;
}

inline bool addU32(
    std::uint32_t left,
    std::uint32_t right,
    std::uint32_t& result) noexcept
{
    if (right > std::numeric_limits<std::uint32_t>::max() - left)
        return false;
    result = left + right;
    return true;
}

inline bool rangesOverlap(
    std::uint32_t firstStart,
    std::uint32_t firstBytes,
    std::uint32_t secondStart,
    std::uint32_t secondBytes) noexcept
{
    if (firstBytes == 0u || secondBytes == 0u)
        return false;
    const std::uint64_t firstEnd =
        static_cast<std::uint64_t>(firstStart) + firstBytes;
    const std::uint64_t secondEnd =
        static_cast<std::uint64_t>(secondStart) + secondBytes;
    return static_cast<std::uint64_t>(firstStart) < secondEnd
        && static_cast<std::uint64_t>(secondStart) < firstEnd;
}

inline bool isPowerOfTwo(std::uint32_t value) noexcept
{
    return value != 0u && (value & (value - 1u)) == 0u;
}

inline bool alignUpU32(
    std::uint32_t value,
    std::uint32_t alignment,
    std::uint32_t& result) noexcept
{
    if (!isPowerOfTwo(alignment))
        return false;
    const std::uint32_t mask = alignment - 1u;
    if (value > std::numeric_limits<std::uint32_t>::max() - mask)
        return false;
    result = (value + mask) & ~mask;
    return true;
}

inline bool protectionIsExecutable(
    LoadedMemoryProtection protection) noexcept
{
    return protection == LoadedMemoryProtection::Execute
        || protection == LoadedMemoryProtection::ExecuteRead
        || protection == LoadedMemoryProtection::ExecuteReadWrite
        || protection == LoadedMemoryProtection::ExecuteWriteCopy;
}

inline bool protectionIsWritable(
    LoadedMemoryProtection protection) noexcept
{
    return protection == LoadedMemoryProtection::ReadWrite
        || protection == LoadedMemoryProtection::WriteCopy
        || protection == LoadedMemoryProtection::ExecuteReadWrite
        || protection == LoadedMemoryProtection::ExecuteWriteCopy;
}

inline bool readU16(
    BytesView bytes,
    std::size_t offset,
    std::uint16_t& value) noexcept
{
    if (!bytes.data || !rangeWithin(bytes.size, offset, 2u))
        return false;
    value = static_cast<std::uint16_t>(bytes.data[offset])
        | static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(bytes.data[offset + 1u]) << 8u);
    return true;
}

inline bool readU32(
    BytesView bytes,
    std::size_t offset,
    std::uint32_t& value) noexcept
{
    if (!bytes.data || !rangeWithin(bytes.size, offset, 4u))
        return false;
    value = static_cast<std::uint32_t>(bytes.data[offset])
        | (static_cast<std::uint32_t>(bytes.data[offset + 1u]) << 8u)
        | (static_cast<std::uint32_t>(bytes.data[offset + 2u]) << 16u)
        | (static_cast<std::uint32_t>(bytes.data[offset + 3u]) << 24u);
    return true;
}

inline bool readVectorU32(
    const std::vector<std::uint8_t>& bytes,
    std::size_t offset,
    std::uint32_t& value) noexcept
{
    return readU32({ bytes.data(), bytes.size() }, offset, value);
}

inline bool writeVectorU32(
    std::vector<std::uint8_t>& bytes,
    std::size_t offset,
    std::uint32_t value) noexcept
{
    if (!rangeWithin(bytes.size(), offset, 4u))
        return false;
    bytes[offset] = static_cast<std::uint8_t>(value & 0xFFu);
    bytes[offset + 1u] =
        static_cast<std::uint8_t>((value >> 8u) & 0xFFu);
    bytes[offset + 2u] =
        static_cast<std::uint8_t>((value >> 16u) & 0xFFu);
    bytes[offset + 3u] =
        static_cast<std::uint8_t>((value >> 24u) & 0xFFu);
    return true;
}

inline MappedImageProofFailure parsePe32(
    BytesView disk,
    ParsedPe32& result)
{
    if (!disk.data || disk.size < 0x40u)
        return MappedImageProofFailure::InputRangeInvalid;
    if (disk.data[0] != static_cast<std::uint8_t>('M')
        || disk.data[1] != static_cast<std::uint8_t>('Z'))
    {
        return MappedImageProofFailure::MalformedDosHeader;
    }

    std::uint32_t ntOffset32 = 0;
    if (!readU32(disk, 0x3Cu, ntOffset32))
        return MappedImageProofFailure::MalformedDosHeader;
    const std::size_t ntOffset = static_cast<std::size_t>(ntOffset32);
    if (!rangeWithin(disk.size, ntOffset, 24u)
        || disk.data[ntOffset] != static_cast<std::uint8_t>('P')
        || disk.data[ntOffset + 1u] != static_cast<std::uint8_t>('E')
        || disk.data[ntOffset + 2u] != 0u
        || disk.data[ntOffset + 3u] != 0u)
    {
        return MappedImageProofFailure::MalformedNtHeaders;
    }

    std::uint16_t machine = 0;
    std::uint16_t sectionCount = 0;
    std::uint16_t optionalHeaderBytes = 0;
    if (!readU16(disk, ntOffset + 4u, machine)
        || !readU16(disk, ntOffset + 6u, sectionCount)
        || !readU16(disk, ntOffset + 20u, optionalHeaderBytes))
    {
        return MappedImageProofFailure::MalformedNtHeaders;
    }
    if (machine != Pe32MachineI386)
        return MappedImageProofFailure::UnsupportedMachine;
    if (sectionCount == 0u || optionalHeaderBytes < 224u)
        return MappedImageProofFailure::UnsupportedOptionalHeader;

    const std::size_t optionalOffset = ntOffset + 24u;
    if (!rangeWithin(disk.size, optionalOffset, optionalHeaderBytes))
        return MappedImageProofFailure::UnsupportedOptionalHeader;
    std::uint16_t optionalMagic = 0;
    std::uint32_t numberOfDataDirectories = 0;
    if (!readU16(disk, optionalOffset, optionalMagic)
        || !readU32(disk, optionalOffset + 28u, result.preferredImageBase)
        || !readU32(disk, optionalOffset + 32u, result.sectionAlignment)
        || !readU32(disk, optionalOffset + 36u, result.fileAlignment)
        || !readU32(disk, optionalOffset + 56u, result.sizeOfImage)
        || !readU32(disk, optionalOffset + 60u, result.sizeOfHeaders)
        || !readU32(disk, optionalOffset + 92u, numberOfDataDirectories))
    {
        return MappedImageProofFailure::UnsupportedOptionalHeader;
    }
    if (optionalMagic != Pe32OptionalHeaderMagic
        || numberOfDataDirectories < 6u
        || optionalHeaderBytes < 136u)
    {
        return MappedImageProofFailure::UnsupportedOptionalHeader;
    }
    if (!readU32(
            disk,
            optionalOffset + 96u + 5u * 8u,
            result.relocationDirectoryRva)
        || !readU32(
            disk,
            optionalOffset + 96u + 5u * 8u + 4u,
            result.relocationDirectoryBytes))
    {
        return MappedImageProofFailure::UnsupportedOptionalHeader;
    }

    if (!isPowerOfTwo(result.fileAlignment)
        || result.fileAlignment < 512u
        || result.fileAlignment > 65536u
        || !isPowerOfTwo(result.sectionAlignment)
        || result.sectionAlignment < result.fileAlignment
        || result.sizeOfHeaders == 0u
        || result.sizeOfHeaders % result.fileAlignment != 0u
        || result.sizeOfImage == 0u
        || result.sizeOfImage % result.sectionAlignment != 0u
        || result.sizeOfHeaders > result.sizeOfImage
        || result.sizeOfHeaders > disk.size)
    {
        return MappedImageProofFailure::InvalidImageGeometry;
    }
    // Low-alignment images can place unlike sections on one hardware page;
    // larger alignment can leave loader-specific gaps between page-rounded
    // content and the next section. The exact retail targets use the ordinary
    // x86 page alignment, so reject both ambiguous geometries until proven.
    if (result.sectionAlignment != Pe32X86PageBytes)
    {
        return MappedImageProofFailure::UnsupportedProtectionGeometry;
    }

    const std::size_t sectionTableOffset = optionalOffset + optionalHeaderBytes;
    const std::size_t sectionTableBytes =
        static_cast<std::size_t>(sectionCount) * 40u;
    if (!rangeWithin(disk.size, sectionTableOffset, sectionTableBytes)
        || sectionTableOffset > result.sizeOfHeaders
        || sectionTableBytes > result.sizeOfHeaders - sectionTableOffset)
    {
        return MappedImageProofFailure::InvalidSectionTable;
    }

    result.sections.reserve(sectionCount);
    for (std::size_t index = 0; index < sectionCount; ++index)
    {
        const std::size_t offset = sectionTableOffset + index * 40u;
        std::uint32_t virtualBytes = 0;
        Section section {};
        if (!readU32(disk, offset + 8u, virtualBytes)
            || !readU32(disk, offset + 12u, section.virtualAddress)
            || !readU32(disk, offset + 16u, section.rawBytes)
            || !readU32(disk, offset + 20u, section.rawOffset)
            || !readU32(disk, offset + 36u, section.characteristics))
        {
            return MappedImageProofFailure::InvalidSectionTable;
        }
        section.mappedBytes = std::max(virtualBytes, section.rawBytes);
        if (section.mappedBytes == 0u
            || section.virtualAddress < result.sizeOfHeaders
            || section.virtualAddress % result.sectionAlignment != 0u
            || static_cast<std::uint64_t>(section.virtualAddress)
                    + section.mappedBytes
                > result.sizeOfImage)
        {
            return MappedImageProofFailure::SectionRangeOutOfBounds;
        }
        if (section.rawBytes != 0u)
        {
            if (section.rawOffset < result.sizeOfHeaders
                || section.rawOffset % result.fileAlignment != 0u
                || section.rawBytes % result.fileAlignment != 0u
                || !rangeWithin(
                    disk.size,
                    section.rawOffset,
                    section.rawBytes))
            {
                return MappedImageProofFailure::SectionRangeOutOfBounds;
            }
        }

        if (section.executable() && section.writable())
        {
            return MappedImageProofFailure::WritableExecutableSection;
        }

        for (const Section& prior : result.sections)
        {
            if (rangesOverlap(
                    section.virtualAddress,
                    section.mappedBytes,
                    prior.virtualAddress,
                    prior.mappedBytes)
                || (section.rawBytes != 0u
                    && prior.rawBytes != 0u
                    && rangesOverlap(
                        section.rawOffset,
                        section.rawBytes,
                        prior.rawOffset,
                        prior.rawBytes)))
            {
                return MappedImageProofFailure::OverlappingSections;
            }
        }
        if (!result.sections.empty()
            && section.virtualAddress
                <= result.sections.back().virtualAddress)
        {
            return MappedImageProofFailure::InvalidSectionTable;
        }
        result.sections.push_back(section);
    }

    for (std::size_t index = 0; index < result.sections.size(); ++index)
    {
        Section& section = result.sections[index];
        std::uint32_t pageRoundedBytes = 0;
        if (!alignUpU32(
                section.mappedBytes,
                Pe32X86PageBytes,
                pageRoundedBytes))
        {
            return MappedImageProofFailure::SectionRangeOutOfBounds;
        }
        const std::uint32_t boundary = index + 1u < result.sections.size()
            ? result.sections[index + 1u].virtualAddress
            : result.sizeOfImage;
        const std::uint64_t candidateEnd =
            static_cast<std::uint64_t>(section.virtualAddress)
            + pageRoundedBytes;
        const std::uint32_t clippedEnd = candidateEnd < boundary
            ? static_cast<std::uint32_t>(candidateEnd)
            : boundary;
        const std::uint64_t contentEnd =
            static_cast<std::uint64_t>(section.virtualAddress)
            + section.mappedBytes;
        if (clippedEnd < contentEnd || clippedEnd < section.virtualAddress)
            return MappedImageProofFailure::OverlappingSections;
        section.protectionBytes = clippedEnd - section.virtualAddress;
    }

    const auto executableCount = std::count_if(
        result.sections.begin(),
        result.sections.end(),
        [](const Section& section) { return section.executable(); });
    if (executableCount == 0)
        return MappedImageProofFailure::NoExecutableSections;
    return MappedImageProofFailure::None;
}

inline const Section* findContainingSection(
    const std::vector<Section>& sections,
    std::uint32_t rva,
    std::uint32_t bytes,
    bool requireRaw,
    bool requireExecutable) noexcept
{
    const Section* result = nullptr;
    for (const Section& section : sections)
    {
        if (requireExecutable && !section.executable())
            continue;
        const std::uint32_t available =
            requireRaw ? section.rawBytes : section.mappedBytes;
        if (rva < section.virtualAddress)
            continue;
        const std::uint32_t relative = rva - section.virtualAddress;
        if (relative > available || bytes > available - relative)
            continue;
        if (result)
            return nullptr;
        result = &section;
    }
    return result;
}

inline bool rvaToDiskOffset(
    const ParsedPe32& pe,
    BytesView disk,
    std::uint32_t rva,
    std::uint32_t bytes,
    std::size_t& fileOffset) noexcept
{
    const Section* section =
        findContainingSection(pe.sections, rva, bytes, true, false);
    if (!section)
        return false;
    const std::uint32_t relative = rva - section->virtualAddress;
    const std::uint64_t calculated =
        static_cast<std::uint64_t>(section->rawOffset) + relative;
    if (calculated > std::numeric_limits<std::size_t>::max())
        return false;
    fileOffset = static_cast<std::size_t>(calculated);
    return rangeWithin(disk.size, fileOffset, bytes);
}

inline MappedImageProofFailure parseAndApplyRelocations(
    const ParsedPe32& pe,
    BytesView disk,
    std::int64_t baseDelta,
    std::vector<std::uint8_t>& expectedImage,
    std::vector<Relocation>& relocations)
{
    if ((pe.relocationDirectoryRva == 0u)
        != (pe.relocationDirectoryBytes == 0u))
    {
        return MappedImageProofFailure::RelocationDirectoryOutOfBounds;
    }
    if (pe.relocationDirectoryBytes == 0u)
    {
        return baseDelta == 0
            ? MappedImageProofFailure::None
            : MappedImageProofFailure::RelocationDirectoryMissing;
    }
    if (pe.relocationDirectoryBytes % 4u != 0u)
        return MappedImageProofFailure::MalformedRelocationBlock;

    std::size_t directoryFileOffset = 0;
    if (!rvaToDiskOffset(
            pe,
            disk,
            pe.relocationDirectoryRva,
            pe.relocationDirectoryBytes,
            directoryFileOffset))
    {
        return MappedImageProofFailure::RelocationDirectoryOutOfBounds;
    }

    std::uint32_t consumed = 0;
    std::vector<std::uint32_t> blockPages;
    while (consumed < pe.relocationDirectoryBytes)
    {
        const std::size_t blockOffset = directoryFileOffset + consumed;
        std::uint32_t pageRva = 0;
        std::uint32_t blockBytes = 0;
        if (!readU32(disk, blockOffset, pageRva)
            || !readU32(disk, blockOffset + 4u, blockBytes)
            || pageRva % 0x1000u != 0u
            || blockBytes < 12u
            || blockBytes % 4u != 0u
            || blockBytes > pe.relocationDirectoryBytes - consumed)
        {
            return MappedImageProofFailure::MalformedRelocationBlock;
        }
        if (std::find(blockPages.begin(), blockPages.end(), pageRva)
            != blockPages.end())
        {
            return MappedImageProofFailure::MalformedRelocationBlock;
        }
        blockPages.push_back(pageRva);

        const std::uint32_t entryCount = (blockBytes - 8u) / 2u;
        for (std::uint32_t entryIndex = 0;
             entryIndex < entryCount;
             ++entryIndex)
        {
            std::uint16_t encoded = 0;
            const std::size_t entryOffset = blockOffset + 8u
                + static_cast<std::size_t>(entryIndex) * 2u;
            if (!readU16(disk, entryOffset, encoded))
                return MappedImageProofFailure::MalformedRelocationBlock;
            const std::uint16_t type =
                static_cast<std::uint16_t>(encoded >> 12u);
            if (type == RelocationAbsolute)
                continue;
            if (type != RelocationHighLow)
                return MappedImageProofFailure::UnsupportedRelocationType;

            std::uint32_t targetRva = 0;
            if (!addU32(
                    pageRva,
                    static_cast<std::uint32_t>(encoded & 0x0FFFu),
                    targetRva)
                || !findContainingSection(
                    pe.sections,
                    targetRva,
                    4u,
                    false,
                    false)
                || !rangeWithin(expectedImage.size(), targetRva, 4u))
            {
                return MappedImageProofFailure::RelocationTargetOutOfBounds;
            }
            const auto duplicate = std::find_if(
                relocations.begin(),
                relocations.end(),
                [targetRva](const Relocation& value) {
                    return rangesOverlap(value.rva, 4u, targetRva, 4u);
                });
            if (duplicate != relocations.end())
                return MappedImageProofFailure::DuplicateRelocationTarget;

            std::uint32_t original = 0;
            if (!readVectorU32(expectedImage, targetRva, original))
                return MappedImageProofFailure::RelocationTargetOutOfBounds;
            const std::int64_t relocated =
                static_cast<std::int64_t>(original) + baseDelta;
            if (relocated < 0
                || relocated
                    > static_cast<std::int64_t>(
                        std::numeric_limits<std::uint32_t>::max()))
            {
                return MappedImageProofFailure::RelocationArithmeticOverflow;
            }
            if (!writeVectorU32(
                    expectedImage,
                    targetRva,
                    static_cast<std::uint32_t>(relocated)))
            {
                return MappedImageProofFailure::RelocationTargetOutOfBounds;
            }
            relocations.push_back({ targetRva });
        }
        consumed += blockBytes;
    }
    return consumed == pe.relocationDirectoryBytes
        ? MappedImageProofFailure::None
        : MappedImageProofFailure::MalformedRelocationBlock;
}

inline bool validDescription(const ExactPatchAllowance& patch) noexcept
{
    if (!patch.description
        || patch.descriptionBytes < MinimumPatchDescriptionBytes
        || patch.descriptionBytes > MaximumPatchDescriptionBytes)
    {
        return false;
    }
    bool hasNonSpace = false;
    for (std::size_t index = 0; index < patch.descriptionBytes; ++index)
    {
        const unsigned char value =
            static_cast<unsigned char>(patch.description[index]);
        if (value < 0x20u || value > 0x7Eu)
            return false;
        hasNonSpace = hasNonSpace || value != 0x20u;
    }
    return hasNonSpace
        && patch.description[0] != ' '
        && patch.description[patch.descriptionBytes - 1u] != ' ';
}

inline MappedImageProofFailure validateProtectionMap(
    const LoadedMemoryProtectionRange* ranges,
    std::size_t rangeCount,
    std::uint32_t imageBytes,
    const std::vector<std::uint8_t>& expectedExecutable,
    std::size_t& verifiedExecutableBytes) noexcept
{
    if (!ranges || rangeCount == 0u)
        return MappedImageProofFailure::ProtectionMapMissing;

    std::uint32_t cursor = 0;
    for (std::size_t index = 0; index < rangeCount; ++index)
    {
        const LoadedMemoryProtectionRange& range = ranges[index];
        std::uint32_t end = 0;
        if (range.rva != cursor
            || range.byteCount == 0u
            || range.rva % Pe32X86PageBytes != 0u
            || range.byteCount % Pe32X86PageBytes != 0u
            || !addU32(range.rva, range.byteCount, end)
            || end > imageBytes
            || range.state == LoadedMemoryState::Unknown
            || range.protection == LoadedMemoryProtection::Unknown
            || range.guard
            || (range.state == LoadedMemoryState::Reserved
                && range.protection != LoadedMemoryProtection::NoAccess))
        {
            return MappedImageProofFailure::MalformedProtectionRange;
        }

        const bool executable = range.state == LoadedMemoryState::Committed
            && protectionIsExecutable(range.protection);
        if (executable && protectionIsWritable(range.protection))
            return MappedImageProofFailure::WritableExecutableProtection;

        for (std::uint32_t rva = range.rva; rva < end; ++rva)
        {
            const bool expected = expectedExecutable[rva] != 0u;
            if (expected && !executable)
            {
                return MappedImageProofFailure::
                    ExpectedExecutableProtectionMissing;
            }
            if (!expected && executable)
            {
                return MappedImageProofFailure::
                    UnexpectedExecutableProtection;
            }
            if (expected)
                ++verifiedExecutableBytes;
        }
        cursor = end;
    }
    return cursor == imageBytes
        ? MappedImageProofFailure::None
        : MappedImageProofFailure::IncompleteProtectionMap;
}
}

inline MappedImageProofAssessment proveLoadedExecutableSections(
    const MappedImageProofInput& input)
{
    MappedImageProofAssessment assessment {};
    const auto reject = [&](MappedImageProofFailure failure) {
        assessment.loadedExecutableSectionsMatched = false;
        assessment.productionAuthorized = false;
        assessment.failure = failure;
        return assessment;
    };

    if (!input.evidence.exactDiskFileIdentityVerified)
        return reject(MappedImageProofFailure::ExactDiskIdentityUnverified);
    if (!input.evidence.diskFileReadComplete)
        return reject(MappedImageProofFailure::DiskFileReadIncomplete);
    if (!input.evidence.loadedImageSnapshotComplete)
        return reject(MappedImageProofFailure::LoadedSnapshotIncomplete);
    if (!input.evidence.loadedImageSnapshotStableBeforeAfter)
        return reject(MappedImageProofFailure::LoadedSnapshotUnstable);
    if (!input.evidence.loadedMemoryProtectionSnapshotStableBeforeAfter)
        return reject(MappedImageProofFailure::LoadedProtectionSnapshotUnstable);
    if (!input.evidence.executableSectionCoverageComplete)
        return reject(MappedImageProofFailure::ExecutableCoverageIncomplete);
    if (!input.evidence.relocationDirectoryCoverageComplete)
        return reject(MappedImageProofFailure::RelocationCoverageIncomplete);
    if (!input.evidence.exactPatchInventoryComplete)
        return reject(MappedImageProofFailure::PatchInventoryIncomplete);
    // Bound externally supplied counts before consulting either array.
    if (input.patchAllowanceCount > MaximumPatchAllowanceCount)
        return reject(MappedImageProofFailure::PatchCountOutOfBounds);
    if (input.memoryProtectionRangeCount
        > MaximumMemoryProtectionRangeCount)
    {
        return reject(MappedImageProofFailure::ProtectionRangeCountOutOfBounds);
    }
    if (!input.exactDiskFile.data
        || input.exactDiskFile.size == 0u
        || !input.loadedImageSnapshot.data
        || input.loadedImageSnapshot.size == 0u
        || (input.memoryProtectionRangeCount != 0u
            && !input.memoryProtectionRanges)
        || (input.patchAllowanceCount != 0u && !input.patchAllowances))
    {
        return reject(MappedImageProofFailure::InputRangeInvalid);
    }

    detail::ParsedPe32 pe {};
    const MappedImageProofFailure parseFailure =
        detail::parsePe32(input.exactDiskFile, pe);
    if (parseFailure != MappedImageProofFailure::None)
        return reject(parseFailure);

    constexpr std::uint64_t Pe32AddressLimit = 0x100000000ull;
    if (input.loadedImageSnapshot.size != pe.sizeOfImage
        || input.runtimeImageBase == 0u
        || input.runtimeImageBase >= Pe32AddressLimit
        || input.runtimeImageBase > Pe32AddressLimit - pe.sizeOfImage
        || static_cast<std::uint64_t>(pe.preferredImageBase)
            > Pe32AddressLimit - pe.sizeOfImage)
    {
        return reject(MappedImageProofFailure::RuntimeImageBoundsInvalid);
    }

    std::vector<std::uint8_t> expectedImage(pe.sizeOfImage, 0u);
    std::vector<std::uint8_t> expectedExecutable(pe.sizeOfImage, 0u);
    std::copy_n(
        input.exactDiskFile.data,
        pe.sizeOfHeaders,
        expectedImage.data());
    for (const detail::Section& section : pe.sections)
    {
        if (section.rawBytes != 0u)
        {
            std::copy_n(
                input.exactDiskFile.data + section.rawOffset,
                section.rawBytes,
                expectedImage.data() + section.virtualAddress);
        }
        if (section.executable())
        {
            ++assessment.executableSectionCount;
            assessment.executableBytesCompared += section.protectionBytes;
            std::fill_n(
                expectedExecutable.data() + section.virtualAddress,
                section.protectionBytes,
                static_cast<std::uint8_t>(1u));
        }
    }

    const MappedImageProofFailure protectionFailure =
        detail::validateProtectionMap(
            input.memoryProtectionRanges,
            input.memoryProtectionRangeCount,
            pe.sizeOfImage,
            expectedExecutable,
            assessment.executableProtectionBytesVerified);
    if (protectionFailure != MappedImageProofFailure::None)
        return reject(protectionFailure);
    assessment.memoryProtectionRangeCount =
        input.memoryProtectionRangeCount;

    const std::int64_t baseDelta =
        static_cast<std::int64_t>(input.runtimeImageBase)
        - static_cast<std::int64_t>(pe.preferredImageBase);
    std::vector<detail::Relocation> relocations;
    const MappedImageProofFailure relocationFailure =
        detail::parseAndApplyRelocations(
            pe,
            input.exactDiskFile,
            baseDelta,
            expectedImage,
            relocations);
    if (relocationFailure != MappedImageProofFailure::None)
        return reject(relocationFailure);
    assessment.highLowRelocationCount = relocations.size();

    for (std::size_t patchIndex = 0;
         patchIndex < input.patchAllowanceCount;
         ++patchIndex)
    {
        const ExactPatchAllowance& patch = input.patchAllowances[patchIndex];
        if (!patch.exactUnpatchedBytes.data
            || !patch.exactPatchedBytes.data
            || patch.exactUnpatchedBytes.size == 0u
            || patch.exactUnpatchedBytes.size != patch.exactPatchedBytes.size
            || patch.exactUnpatchedBytes.size
                > std::numeric_limits<std::uint32_t>::max())
        {
            return reject(MappedImageProofFailure::PatchArrayInvalid);
        }
        if (!detail::validDescription(patch))
            return reject(MappedImageProofFailure::PatchDescriptionInvalid);
        if (patch.authority == PatchAuthority::Unknown)
            return reject(MappedImageProofFailure::PatchAuthorityUnverified);
        if (patch.authority == PatchAuthority::Jip
            && !input.evidence.exactJipPatchAuthorityVerified)
        {
            return reject(MappedImageProofFailure::PatchAuthorityUnverified);
        }
        if (patch.authority == PatchAuthority::ShowOff)
        {
            if (!input.evidence.exactShowOffPatchAuthorityVerified
                || !ShowOffPatchAllowlistProductionProven)
            {
                return reject(
                    MappedImageProofFailure::ShowOffPatchAllowlistUnproven);
            }
        }

        const std::uint32_t patchBytes =
            static_cast<std::uint32_t>(patch.exactUnpatchedBytes.size);
        std::uint32_t patchEnd = 0;
        if (!detail::addU32(patch.rva, patchBytes, patchEnd)
            || !detail::rangeWithin(
                expectedImage.size(),
                patch.rva,
                patchBytes))
        {
            return reject(MappedImageProofFailure::PatchRangeOutOfBounds);
        }
        (void)patchEnd;
        if (!detail::findContainingSection(
                pe.sections,
                patch.rva,
                patchBytes,
                false,
                true))
        {
            return reject(MappedImageProofFailure::PatchRangeNotExecutable);
        }
        for (std::size_t priorIndex = 0;
             priorIndex < patchIndex;
             ++priorIndex)
        {
            const ExactPatchAllowance& prior =
                input.patchAllowances[priorIndex];
            if (prior.exactUnpatchedBytes.size
                    <= std::numeric_limits<std::uint32_t>::max()
                && detail::rangesOverlap(
                    patch.rva,
                    patchBytes,
                    prior.rva,
                    static_cast<std::uint32_t>(
                        prior.exactUnpatchedBytes.size)))
            {
                return reject(MappedImageProofFailure::OverlappingPatches);
            }
        }
        for (const detail::Relocation& relocation : relocations)
        {
            if (detail::rangesOverlap(
                    patch.rva,
                    patchBytes,
                    relocation.rva,
                    4u))
            {
                return reject(MappedImageProofFailure::PatchOverlapsRelocation);
            }
        }

        bool replacementChangesBytes = false;
        for (std::size_t byteIndex = 0;
             byteIndex < patch.exactUnpatchedBytes.size;
             ++byteIndex)
        {
            const std::size_t imageIndex =
                static_cast<std::size_t>(patch.rva) + byteIndex;
            if (expectedImage[imageIndex]
                != patch.exactUnpatchedBytes.data[byteIndex])
            {
                return reject(
                    MappedImageProofFailure::PatchOriginalBytesMismatch);
            }
            replacementChangesBytes = replacementChangesBytes
                || patch.exactUnpatchedBytes.data[byteIndex]
                    != patch.exactPatchedBytes.data[byteIndex];
            if (input.loadedImageSnapshot.data[imageIndex]
                != patch.exactPatchedBytes.data[byteIndex])
            {
                return reject(
                    MappedImageProofFailure::PatchReplacementBytesMismatch);
            }
        }
        if (!replacementChangesBytes)
            return reject(MappedImageProofFailure::PatchReplacementInvalid);
        ++assessment.exactPatchCount;
        assessment.exactPatchBytesCompared +=
            patch.exactUnpatchedBytes.size;
    }

    for (const detail::Section& section : pe.sections)
    {
        if (!section.executable())
            continue;
        for (std::uint32_t relative = 0;
             relative < section.protectionBytes;
             ++relative)
        {
            const std::uint32_t rva = section.virtualAddress + relative;
            std::uint8_t expected = expectedImage[rva];
            for (std::size_t patchIndex = 0;
                 patchIndex < input.patchAllowanceCount;
                 ++patchIndex)
            {
                const ExactPatchAllowance& patch =
                    input.patchAllowances[patchIndex];
                const std::size_t patchRelative =
                    static_cast<std::size_t>(rva - patch.rva);
                if (rva >= patch.rva
                    && patchRelative < patch.exactPatchedBytes.size)
                {
                    expected = patch.exactPatchedBytes.data[patchRelative];
                    break;
                }
            }
            if (input.loadedImageSnapshot.data[rva] != expected)
                return reject(MappedImageProofFailure::ExecutableByteMismatch);
        }
    }

    assessment.loadedExecutableSectionsMatched = true;
    assessment.productionAuthorized = ProductionAuthorizationEnabled;
    assessment.failure = MappedImageProofFailure::None;
    return assessment;
}
}
