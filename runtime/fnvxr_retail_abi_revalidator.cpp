#include "fnvxr_retail_abi_revalidator.h"

#include "fnvxr_module_inventory_acceptance.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <limits>
#include <new>
#include <string_view>
#include <vector>

namespace fnvxr::engine::abi::revalidation
{
namespace
{
constexpr std::uint32_t X86PageBytes = 0x1000u;
constexpr std::uint32_t SectionMemExecute = 0x20000000u;
constexpr std::uint32_t SectionMemWrite = 0x80000000u;
constexpr std::size_t MaximumPeSectionCount = 96u;

struct MemoryRegion
{
    std::uintptr_t address = 0;
    std::size_t byteCount = 0;
    bool committed = false;
    bool readable = false;
    bool writable = false;
    bool executable = false;
    bool guard = false;
};

class MemoryReader
{
public:
    virtual ~MemoryReader() = default;
    virtual bool read(
        std::uintptr_t address,
        void* destination,
        std::size_t byteCount) const noexcept = 0;
    virtual bool query(
        std::uintptr_t address,
        MemoryRegion& region) const noexcept = 0;
};

struct ExecutableSectionLayoutInternal
{
    std::array<std::uint8_t, 8> name {};
    std::uint32_t rva = 0;
    std::uint32_t virtualBytes = 0;
    std::uint32_t mappedBytes = 0;
    std::uint32_t protectionBytes = 0;
    std::uint32_t rawBytes = 0;
    std::uint32_t characteristics = 0;
    std::uint8_t independentLayoutSamples = 0;
};

struct RevalidationContract
{
    std::uintptr_t preferredImageBase = 0;
    LoadedExecutableIdentity loadedIdentity {};
    std::uint32_t sizeOfImage = 0;

    const ExecutableSectionLayoutInternal* executableSections = nullptr;
    std::size_t executableSectionCount = 0;
    const LoadedFunctionManifestEntry* coreManifest = nullptr;
    std::size_t coreManifestCount = 0;
    const RetailFunctionAbiDescriptor* functionInventory = nullptr;
    std::size_t functionInventoryCount = 0;
    const RetailVtableSlotDescriptor* vtableSlots = nullptr;
    std::size_t vtableSlotCount = 0;
    const RetailVtableBlockDescriptor* vtableBlocks = nullptr;
    std::size_t vtableBlockCount = 0;

    std::uintptr_t sceneGraphSingletonPointerAddress = 0;
    std::uintptr_t bSCullingProcessVtableAddress = 0;
};

struct LoadedSection
{
    std::array<std::uint8_t, 8> name {};
    std::uint32_t rva = 0;
    std::uint32_t virtualBytes = 0;
    std::uint32_t mappedBytes = 0;
    std::uint32_t protectionBytes = 0;
    std::uint32_t rawBytes = 0;
    std::uint32_t characteristics = 0;
};

struct LoadedPe32
{
    std::uint16_t machine = 0;
    std::uint32_t timeDateStamp = 0;
    std::uint32_t checksum = 0;
    std::uint32_t preferredImageBase = 0;
    std::uint32_t sectionAlignment = 0;
    std::uint32_t sizeOfHeaders = 0;
    std::uint32_t sizeOfImage = 0;
    std::vector<LoadedSection> sections {};
};

class CurrentProcessReader final : public MemoryReader
{
public:
    bool read(
        std::uintptr_t address,
        void* destination,
        std::size_t byteCount) const noexcept override
    {
        if (!address || !destination || byteCount == 0u)
            return false;
        SIZE_T transferred = 0u;
        return ReadProcessMemory(
                   GetCurrentProcess(),
                   reinterpret_cast<const void*>(address),
                   destination,
                   byteCount,
                   &transferred)
            && transferred == byteCount;
    }

    bool query(
        std::uintptr_t address,
        MemoryRegion& region) const noexcept override
    {
        MEMORY_BASIC_INFORMATION information {};
        if (!address
            || VirtualQuery(
                   reinterpret_cast<const void*>(address),
                   &information,
                   sizeof(information))
                != sizeof(information))
        {
            return false;
        }

        const DWORD protection = information.Protect;
        const DWORD baseProtection = protection & 0xFFu;
        region.address = reinterpret_cast<std::uintptr_t>(
            information.BaseAddress);
        region.byteCount = information.RegionSize;
        region.committed = information.State == MEM_COMMIT;
        region.guard = (protection & PAGE_GUARD) != 0u;
        region.readable = baseProtection == PAGE_READONLY
            || baseProtection == PAGE_READWRITE
            || baseProtection == PAGE_WRITECOPY
            || baseProtection == PAGE_EXECUTE
            || baseProtection == PAGE_EXECUTE_READ
            || baseProtection == PAGE_EXECUTE_READWRITE
            || baseProtection == PAGE_EXECUTE_WRITECOPY;
        region.writable = baseProtection == PAGE_READWRITE
            || baseProtection == PAGE_WRITECOPY
            || baseProtection == PAGE_EXECUTE_READWRITE
            || baseProtection == PAGE_EXECUTE_WRITECOPY;
        region.executable = baseProtection == PAGE_EXECUTE
            || baseProtection == PAGE_EXECUTE_READ
            || baseProtection == PAGE_EXECUTE_READWRITE
            || baseProtection == PAGE_EXECUTE_WRITECOPY;
        return region.address != 0u && region.byteCount != 0u;
    }
};

bool checkedAdd(
    std::uintptr_t address,
    std::size_t bytes,
    std::uintptr_t& result) noexcept
{
    if (bytes > (std::numeric_limits<std::uintptr_t>::max)() - address)
        return false;
    result = address + bytes;
    return true;
}

bool checkedAddU32(
    std::uint32_t left,
    std::uint32_t right,
    std::uint32_t& result) noexcept
{
    if (right > (std::numeric_limits<std::uint32_t>::max)() - left)
        return false;
    result = left + right;
    return true;
}

bool rangeWithinImage(
    std::uintptr_t imageBase,
    std::size_t imageBytes,
    std::uintptr_t address,
    std::size_t bytes) noexcept
{
    if (!imageBase || !address || !bytes || address < imageBase)
        return false;
    const std::uintptr_t offset = address - imageBase;
    return offset <= imageBytes && bytes <= imageBytes - offset;
}

bool rangeHasAccess(
    const MemoryReader& reader,
    std::uintptr_t address,
    std::size_t byteCount,
    bool requireExecutable,
    bool forbidWritable) noexcept
{
    std::uintptr_t end = 0;
    if (!address || !byteCount || !checkedAdd(address, byteCount, end))
        return false;

    std::uintptr_t cursor = address;
    while (cursor < end)
    {
        MemoryRegion region {};
        if (!reader.query(cursor, region)
            || !region.committed
            || !region.readable
            || region.guard
            || (requireExecutable && !region.executable)
            || (forbidWritable && region.writable)
            || cursor < region.address)
        {
            return false;
        }
        std::uintptr_t regionEnd = 0;
        if (!checkedAdd(region.address, region.byteCount, regionEnd)
            || regionEnd <= cursor)
        {
            return false;
        }
        cursor = (std::min)(regionEnd, end);
    }
    return true;
}

bool digestEqual(const Sha256Digest& left, const Sha256Digest& right) noexcept
{
    if (!left.valid || !right.valid)
        return false;
    std::uint8_t difference = 0u;
    for (std::size_t index = 0u; index < left.bytes.size(); ++index)
    {
        difference |= static_cast<std::uint8_t>(
            left.bytes[index] ^ right.bytes[index]);
    }
    return difference == 0u;
}

bool sha256Bytes(
    const std::uint8_t* bytes,
    std::size_t byteCount,
    Sha256Digest& digest) noexcept
{
    digest = {};
    if (!bytes || byteCount == 0u)
        return false;

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectBytes = 0u;
    DWORD digestBytes = 0u;
    DWORD transferred = 0u;
    std::vector<std::uint8_t> object;

    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &algorithm,
        BCRYPT_SHA256_ALGORITHM,
        nullptr,
        0u);
    if (status < 0)
        return false;

    status = BCryptGetProperty(
        algorithm,
        BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&objectBytes),
        sizeof(objectBytes),
        &transferred,
        0u);
    if (status >= 0)
    {
        status = BCryptGetProperty(
            algorithm,
            BCRYPT_HASH_LENGTH,
            reinterpret_cast<PUCHAR>(&digestBytes),
            sizeof(digestBytes),
            &transferred,
            0u);
    }
    if (status >= 0 && digestBytes == digest.bytes.size())
    {
        try
        {
            object.resize(objectBytes);
        }
        catch (...)
        {
            status = static_cast<NTSTATUS>(-1);
        }
    }
    else
    {
        status = static_cast<NTSTATUS>(-1);
    }
    if (status >= 0)
    {
        status = BCryptCreateHash(
            algorithm,
            &hash,
            object.data(),
            objectBytes,
            nullptr,
            0u,
            0u);
    }

    std::size_t offset = 0u;
    while (status >= 0 && offset < byteCount)
    {
        const std::size_t remaining = byteCount - offset;
        const ULONG chunk = static_cast<ULONG>((std::min)(
            remaining,
            static_cast<std::size_t>((std::numeric_limits<ULONG>::max)())));
        status = BCryptHashData(
            hash,
            const_cast<PUCHAR>(bytes + offset),
            chunk,
            0u);
        offset += chunk;
    }
    if (status >= 0)
    {
        status = BCryptFinishHash(
            hash,
            digest.bytes.data(),
            static_cast<ULONG>(digest.bytes.size()),
            0u);
    }

    if (hash)
        BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0u);
    digest.valid = status >= 0;
    if (!digest.valid)
        digest.bytes.fill(0u);
    return digest.valid;
}

bool hashMemory(
    const MemoryReader& reader,
    std::uintptr_t address,
    std::size_t byteCount,
    Sha256Digest& digest) noexcept
{
    digest = {};
    try
    {
        std::vector<std::uint8_t> bytes(byteCount);
        return reader.read(address, bytes.data(), bytes.size())
            && sha256Bytes(bytes.data(), bytes.size(), digest);
    }
    catch (...)
    {
        return false;
    }
}

bool relocate(
    const RevalidationContract& contract,
    std::uintptr_t runtimeBase,
    std::uintptr_t preferredAddress,
    std::uintptr_t& runtimeAddress) noexcept
{
    runtimeAddress = 0u;
    if (preferredAddress < contract.preferredImageBase)
        return false;
    const std::uintptr_t offset = preferredAddress - contract.preferredImageBase;
    if (offset >= contract.sizeOfImage
        || !checkedAdd(runtimeBase, static_cast<std::size_t>(offset), runtimeAddress))
    {
        return false;
    }
    return runtimeAddress <= 0xFFFFFFFFu;
}

bool parseLoadedPe32(
    const MemoryReader& reader,
    std::uintptr_t imageBase,
    LoadedPe32& result) noexcept
{
    result = {};
    IMAGE_DOS_HEADER dos {};
    if (!reader.read(imageBase, &dos, sizeof(dos))
        || dos.e_magic != IMAGE_DOS_SIGNATURE
        || dos.e_lfanew < static_cast<LONG>(sizeof(dos)))
    {
        return false;
    }

    std::uintptr_t ntAddress = 0u;
    if (!checkedAdd(
            imageBase,
            static_cast<std::size_t>(dos.e_lfanew),
            ntAddress))
    {
        return false;
    }
    DWORD signature = 0u;
    IMAGE_FILE_HEADER file {};
    IMAGE_OPTIONAL_HEADER32 optional {};
    if (!reader.read(ntAddress, &signature, sizeof(signature))
        || signature != IMAGE_NT_SIGNATURE
        || !reader.read(
            ntAddress + sizeof(signature),
            &file,
            sizeof(file))
        || file.Machine != IMAGE_FILE_MACHINE_I386
        || file.NumberOfSections == 0u
        || file.NumberOfSections > MaximumPeSectionCount
        || file.SizeOfOptionalHeader < sizeof(optional)
        || !reader.read(
            ntAddress + sizeof(signature) + sizeof(file),
            &optional,
            sizeof(optional))
        || optional.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC
        || optional.SectionAlignment != X86PageBytes
        || optional.SizeOfImage == 0u
        || optional.SizeOfImage % X86PageBytes != 0u
        || optional.SizeOfHeaders == 0u
        || optional.SizeOfHeaders > optional.SizeOfImage)
    {
        return false;
    }

    std::uintptr_t sectionAddress = ntAddress + sizeof(signature) + sizeof(file)
        + file.SizeOfOptionalHeader;
    try
    {
        result.sections.reserve(file.NumberOfSections);
    }
    catch (...)
    {
        return false;
    }

    for (std::size_t index = 0u; index < file.NumberOfSections; ++index)
    {
        IMAGE_SECTION_HEADER header {};
        if (!reader.read(
                sectionAddress + index * sizeof(header),
                &header,
                sizeof(header)))
        {
            return false;
        }
        LoadedSection section {};
        std::copy_n(
            header.Name,
            section.name.size(),
            section.name.begin());
        section.rva = header.VirtualAddress;
        section.virtualBytes = header.Misc.VirtualSize;
        section.rawBytes = header.SizeOfRawData;
        section.mappedBytes = (std::max)(
            section.virtualBytes,
            section.rawBytes);
        section.characteristics = header.Characteristics;
        if (section.mappedBytes == 0u
            || section.rva < optional.SizeOfHeaders
            || section.rva % optional.SectionAlignment != 0u
            || static_cast<std::uint64_t>(section.rva)
                    + section.mappedBytes
                > optional.SizeOfImage
            || (!result.sections.empty()
                && section.rva <= result.sections.back().rva)
            || ((section.characteristics & SectionMemExecute) != 0u
                && (section.characteristics & SectionMemWrite) != 0u))
        {
            return false;
        }
        if (!result.sections.empty())
        {
            const LoadedSection& previous = result.sections.back();
            if (static_cast<std::uint64_t>(previous.rva)
                    + previous.mappedBytes
                > section.rva)
            {
                return false;
            }
        }
        result.sections.push_back(section);
    }

    for (std::size_t index = 0u; index < result.sections.size(); ++index)
    {
        LoadedSection& section = result.sections[index];
        if (section.mappedBytes
            > (std::numeric_limits<std::uint32_t>::max)() - (X86PageBytes - 1u))
        {
            return false;
        }
        const std::uint32_t rounded =
            (section.mappedBytes + X86PageBytes - 1u) & ~(X86PageBytes - 1u);
        const std::uint32_t boundary = index + 1u < result.sections.size()
            ? result.sections[index + 1u].rva
            : optional.SizeOfImage;
        const std::uint64_t candidateEnd =
            static_cast<std::uint64_t>(section.rva) + rounded;
        const std::uint32_t clippedEnd = candidateEnd < boundary
            ? static_cast<std::uint32_t>(candidateEnd)
            : boundary;
        if (clippedEnd < section.rva
            || clippedEnd - section.rva < section.mappedBytes)
        {
            return false;
        }
        section.protectionBytes = clippedEnd - section.rva;
    }

    result.machine = file.Machine;
    result.timeDateStamp = file.TimeDateStamp;
    result.checksum = optional.CheckSum;
    result.preferredImageBase = optional.ImageBase;
    result.sectionAlignment = optional.SectionAlignment;
    result.sizeOfHeaders = optional.SizeOfHeaders;
    result.sizeOfImage = optional.SizeOfImage;
    return true;
}

bool sameExecutableSectionLayout(
    const LoadedSection& loaded,
    const ExecutableSectionLayoutInternal& expected) noexcept
{
    return loaded.name == expected.name
        && loaded.rva == expected.rva
        && loaded.virtualBytes == expected.virtualBytes
        && loaded.mappedBytes == expected.mappedBytes
        && loaded.protectionBytes == expected.protectionBytes
        && loaded.rawBytes == expected.rawBytes
        && loaded.characteristics == expected.characteristics;
}

bool verifyExecutableSectionLayoutAndProtections(
    const MemoryReader& reader,
    std::uintptr_t imageBase,
    const LoadedPe32& pe,
    const RevalidationContract& contract,
    RetailAbiRevalidationDiagnostics& diagnostics) noexcept
{
    std::size_t executableIndex = 0u;
    bool allMatched = contract.executableSections
        && contract.executableSectionCount != 0u;
    for (const LoadedSection& section : pe.sections)
    {
        if ((section.characteristics & SectionMemExecute) == 0u)
            continue;

        ++diagnostics.loadedExecutableSectionCount;
        std::uintptr_t address = 0u;
        Sha256Digest inspectionDigest {};
        const bool observed = checkedAdd(imageBase, section.rva, address)
            && rangeWithinImage(
                imageBase,
                pe.sizeOfImage,
                address,
                section.protectionBytes)
            && rangeHasAccess(
                reader,
                address,
                section.protectionBytes,
                true,
                true)
            && hashMemory(
                reader,
                address,
                section.protectionBytes,
                inspectionDigest);
        if (observed)
        {
            diagnostics.executableSectionBytesInspected +=
                section.protectionBytes;
        }

        if (executableIndex >= contract.executableSectionCount)
        {
            allMatched = false;
        }
        else
        {
            const ExecutableSectionLayoutInternal& expected =
                contract.executableSections[executableIndex];
            allMatched = allMatched
                && observed
                && inspectionDigest.valid
                && expected.independentLayoutSamples
                    >= MinimumIndependentLoadedSamples
                && sameExecutableSectionLayout(section, expected);
        }
        ++executableIndex;
    }
    return executableIndex != 0u
        && executableIndex == contract.executableSectionCount
        && allMatched;
}

bool verifyCoreManifest(
    const MemoryReader& reader,
    std::uintptr_t imageBase,
    const RevalidationContract& contract,
    RetailAbiRevalidationDiagnostics& diagnostics) noexcept
{
    if (!contract.coreManifest
        || contract.coreManifestCount != RetailEngineManifest.size())
    {
        return false;
    }
    for (std::size_t index = 0u; index < contract.coreManifestCount; ++index)
    {
        const LoadedFunctionManifestEntry& entry = contract.coreManifest[index];
        std::uintptr_t address = 0u;
        Sha256Digest digest {};
        if (!entry.name || entry.name[0] == '\0'
            || !entry.sha256.valid
            || entry.byteCount == 0u
            || !relocate(contract, imageBase, entry.preferredAddress, address)
            || !rangeWithinImage(
                imageBase,
                contract.sizeOfImage,
                address,
                entry.byteCount)
            || !rangeHasAccess(reader, address, entry.byteCount, true, true)
            || !hashMemory(reader, address, entry.byteCount, digest)
            || !digestEqual(entry.sha256, digest))
        {
            return false;
        }
        ++diagnostics.functionBodiesHashed;
    }
    return true;
}

bool descriptorStructurallySealed(
    const RetailFunctionAbiDescriptor& descriptor,
    const RevalidationContract& contract) noexcept
{
    if (!descriptor.name || descriptor.name[0] == '\0'
        || !descriptor.sha256.valid
        || !descriptor.exactInstructionBoundary
        || !descriptor.stackContractProven
        || !descriptor.argumentSemanticsProven
        || descriptor.independentLoadedSamples < MinimumIndependentLoadedSamples
        || descriptor.preferredAddress < contract.preferredImageBase
        || descriptor.byteCount == 0u)
    {
        return false;
    }
    const std::uintptr_t offset =
        descriptor.preferredAddress - contract.preferredImageBase;
    if (offset > contract.sizeOfImage
        || descriptor.byteCount > contract.sizeOfImage - offset)
    {
        return false;
    }
    if (descriptor.callingConvention == RetailX86CallingConvention::Cdecl)
        return descriptor.calleePopBytes == 0u;
    return descriptor.calleePopBytes
        == static_cast<std::uint8_t>(descriptor.stackArgumentCount * 4u);
}

bool verifyFunctionInventory(
    const MemoryReader& reader,
    std::uintptr_t imageBase,
    const RevalidationContract& contract,
    RetailAbiRevalidationDiagnostics& diagnostics) noexcept
{
    if (!contract.functionInventory
        || contract.functionInventoryCount != RetailFunctionAbiInventory.size())
    {
        return false;
    }
    for (std::size_t index = 0u; index < contract.functionInventoryCount; ++index)
    {
        const RetailFunctionAbiDescriptor& descriptor =
            contract.functionInventory[index];
        std::uintptr_t address = 0u;
        Sha256Digest digest {};
        if (!descriptorStructurallySealed(descriptor, contract)
            || !relocate(
                contract,
                imageBase,
                descriptor.preferredAddress,
                address)
            || !rangeHasAccess(
                reader,
                address,
                descriptor.byteCount,
                true,
                true)
            || !hashMemory(reader, address, descriptor.byteCount, digest)
            || !digestEqual(descriptor.sha256, digest))
        {
            return false;
        }
        ++diagnostics.functionBodiesHashed;
    }
    return true;
}

bool verifyVtableSlots(
    const MemoryReader& reader,
    std::uintptr_t imageBase,
    const RevalidationContract& contract,
    RetailAbiRevalidationDiagnostics& diagnostics) noexcept
{
    if (!contract.vtableSlots
        || contract.vtableSlotCount != RetailVtableSlots.size())
    {
        return false;
    }
    for (std::size_t index = 0u; index < contract.vtableSlotCount; ++index)
    {
        const RetailVtableSlotDescriptor& descriptor =
            contract.vtableSlots[index];
        std::uintptr_t vtable = 0u;
        std::uintptr_t slot = 0u;
        std::uintptr_t target = 0u;
        RetailPointer32 loadedTarget = 0u;
        if (!descriptor.owner || descriptor.owner[0] == '\0'
            || descriptor.slotByteOffset % sizeof(RetailPointer32) != 0u
            || !relocate(contract, imageBase, descriptor.vtableAddress, vtable)
            || !checkedAdd(vtable, descriptor.slotByteOffset, slot)
            || !relocate(
                contract,
                imageBase,
                descriptor.preferredTargetAddress,
                target)
            || !rangeWithinImage(
                imageBase,
                contract.sizeOfImage,
                slot,
                sizeof(loadedTarget))
            || !rangeWithinImage(
                imageBase,
                contract.sizeOfImage,
                target,
                1u)
            || !rangeHasAccess(
                reader,
                slot,
                sizeof(loadedTarget),
                false,
                true)
            || !rangeHasAccess(reader, target, 1u, true, true)
            || !reader.read(slot, &loadedTarget, sizeof(loadedTarget))
            || loadedTarget != static_cast<RetailPointer32>(target))
        {
            return false;
        }
        ++diagnostics.vtableSlotsRead;
    }
    return true;
}

bool verifyVtableBlocks(
    const MemoryReader& reader,
    std::uintptr_t imageBase,
    const RevalidationContract& contract,
    RetailAbiRevalidationDiagnostics& diagnostics) noexcept
{
    if (!contract.vtableBlocks
        || contract.vtableBlockCount != RetailVtableBlocks.size())
    {
        return false;
    }
    for (std::size_t index = 0u; index < contract.vtableBlockCount; ++index)
    {
        const RetailVtableBlockDescriptor& descriptor =
            contract.vtableBlocks[index];
        std::uintptr_t address = 0u;
        Sha256Digest digest {};
        if (!descriptor.owner || descriptor.owner[0] == '\0'
            || descriptor.byteCount != 0x50u
            || !descriptor.sha256.valid
            || descriptor.independentLoadedSamples < MinimumIndependentLoadedSamples
            || !relocate(contract, imageBase, descriptor.preferredAddress, address)
            || !rangeWithinImage(
                imageBase,
                contract.sizeOfImage,
                address,
                descriptor.byteCount)
            || !rangeHasAccess(
                reader,
                address,
                descriptor.byteCount,
                false,
                true)
            || !hashMemory(reader, address, descriptor.byteCount, digest)
            || !digestEqual(descriptor.sha256, digest))
        {
            return false;
        }
        diagnostics.vtableBlockBytesHashed += descriptor.byteCount;
    }
    return true;
}

bool finiteFloat(float value) noexcept
{
    return std::isfinite(value) != 0;
}

bool finiteFrustum(const RetailNiFrustumLayout& value) noexcept
{
    return finiteFloat(value.left)
        && finiteFloat(value.right)
        && finiteFloat(value.top)
        && finiteFloat(value.bottom)
        && finiteFloat(value.nearDistance)
        && finiteFloat(value.farDistance)
        && value.left < value.right
        && value.bottom < value.top
        && value.nearDistance > 0.0f
        && value.farDistance > value.nearDistance
        && value.orthographic <= 1u;
}

bool verifyLiveLayouts(
    const MemoryReader& reader,
    std::uintptr_t imageBase,
    const RevalidationContract& contract) noexcept
{
    std::uintptr_t singletonAddress = 0u;
    std::uintptr_t expectedCullerVtable = 0u;
    RetailPointer32 sceneAddress32 = 0u;
    if (!relocate(
            contract,
            imageBase,
            contract.sceneGraphSingletonPointerAddress,
            singletonAddress)
        || !relocate(
            contract,
            imageBase,
            contract.bSCullingProcessVtableAddress,
            expectedCullerVtable)
        || !rangeHasAccess(
            reader,
            singletonAddress,
            sizeof(sceneAddress32),
            false,
            false)
        || !reader.read(
            singletonAddress,
            &sceneAddress32,
            sizeof(sceneAddress32))
        || sceneAddress32 == 0u)
    {
        return false;
    }

    const std::uintptr_t sceneAddress = sceneAddress32;
    RetailSceneGraphLayout scene {};
    RetailNiCameraLayout camera {};
    RetailBSCullingProcessLayout culler {};
    RetailNiVisibleArrayLayout visible {};
    if (!rangeHasAccess(
            reader,
            sceneAddress,
            sizeof(scene),
            false,
            false)
        || !reader.read(sceneAddress, &scene, sizeof(scene))
        || scene.camera == 0u
        || scene.visibleArray == 0u
        || scene.cullingProcess == 0u
        || !rangeHasAccess(
            reader,
            scene.camera,
            sizeof(camera),
            false,
            false)
        || !rangeHasAccess(
            reader,
            scene.cullingProcess,
            sizeof(culler),
            false,
            false)
        || !rangeHasAccess(
            reader,
            scene.visibleArray,
            sizeof(visible),
            false,
            false)
        || !reader.read(scene.camera, &camera, sizeof(camera))
        || !reader.read(scene.cullingProcess, &culler, sizeof(culler))
        || !reader.read(scene.visibleArray, &visible, sizeof(visible)))
    {
        return false;
    }

    if (scene.isMenuSceneGraph > 1u
        || !finiteFloat(scene.cameraFov)
        || scene.cameraFov <= 0.0f
        || scene.cameraFov >= 180.0f
        || culler.base.vtable != static_cast<RetailPointer32>(
            expectedCullerVtable)
        || culler.base.useAppendFunction > 1u
        || culler.base.camera != scene.camera
        || culler.base.visibleArray != scene.visibleArray
        || culler.cullModeStackSize > 10u
        || !finiteFrustum(camera.frustum)
        || !finiteFrustum(culler.base.frustum)
        || !finiteFloat(camera.minimumNearPlane)
        || camera.minimumNearPlane <= 0.0f
        || !finiteFloat(camera.maximumFarNearRatio)
        || camera.maximumFarNearRatio <= 0.0f
        || !finiteFloat(camera.viewport.left)
        || !finiteFloat(camera.viewport.right)
        || !finiteFloat(camera.viewport.top)
        || !finiteFloat(camera.viewport.bottom)
        || camera.viewport.left == camera.viewport.right
        || camera.viewport.top == camera.viewport.bottom
        || !finiteFloat(camera.lodAdjust)
        || camera.lodAdjust <= 0.0f
        || visible.itemCount > visible.capacity
        || visible.capacity > 10000000u
        || (visible.itemCount != 0u && visible.geometryPointers == 0u))
    {
        return false;
    }
    for (float value : camera.worldToCamera)
    {
        if (!finiteFloat(value))
            return false;
    }
    if (visible.itemCount != 0u)
    {
        const std::size_t pointerBytes =
            static_cast<std::size_t>(visible.itemCount) * sizeof(RetailPointer32);
        if (!rangeHasAccess(
                reader,
                visible.geometryPointers,
                pointerBytes,
                false,
                false))
        {
            return false;
        }
    }
    return true;
}

const RetailFunctionAbiDescriptor* findFunction(
    const RevalidationContract& contract,
    std::string_view name) noexcept
{
    if (!contract.functionInventory)
        return nullptr;
    for (std::size_t index = 0u; index < contract.functionInventoryCount; ++index)
    {
        const RetailFunctionAbiDescriptor& descriptor =
            contract.functionInventory[index];
        if (descriptor.name && name == descriptor.name)
            return &descriptor;
    }
    return nullptr;
}

bool sealedFunctionsPresent(
    const RevalidationContract& contract,
    const char* const* names,
    std::size_t nameCount) noexcept
{
    for (std::size_t index = 0u; index < nameCount; ++index)
    {
        const RetailFunctionAbiDescriptor* descriptor =
            findFunction(contract, names[index]);
        if (!descriptor || !descriptorStructurallySealed(*descriptor, contract))
            return false;
    }
    return true;
}

bool sealedConstructorOwnership(const RevalidationContract& contract) noexcept
{
    constexpr const char* Required[] = {
        "Ni_Alloc",
        "Ni_Free",
        "BSCullingProcess::BSCullingProcess",
        "BSCullingProcess::~BSCullingProcess body",
        "BSCullingProcess scalar deleting destructor",
        "BSShaderAccumulator::BSShaderAccumulator",
        "BSShaderAccumulator scalar deleting destructor",
        "NiRefObject::Free thunk",
    };
    return sealedFunctionsPresent(
        contract,
        Required,
        sizeof(Required) / sizeof(Required[0]));
}

bool sealedBothWorldBranches(const RevalidationContract& contract) noexcept
{
    constexpr const char* Required[] = {
        "AccumulateScene",
        "AccumulateSecondWorldBranch",
        "RenderAccumulatorWithoutFinalize",
        "FinalizeAccumulator",
        "RenderAndFinalizeAccumulator",
        "BSShaderAccumulator::Render",
        "BSShaderAccumulator::Finalize",
    };
    if (!sealedFunctionsPresent(
            contract,
            Required,
            sizeof(Required) / sizeof(Required[0])))
    {
        return false;
    }
    for (std::size_t index = 0u; index < contract.coreManifestCount; ++index)
    {
        const LoadedFunctionManifestEntry& entry = contract.coreManifest[index];
        if (entry.name
            && std::string_view(entry.name) == "RenderWorldSceneGraph"
            && entry.sha256.valid
            && entry.byteCount != 0u)
        {
            return true;
        }
    }
    return false;
}

bool censusContainsCurrentMainModule(
    const inventory::ModuleInventoryEvidence& census,
    std::uintptr_t runtimeImageBase) noexcept
{
    if (!census.modules || !census.postProofModules
        || census.moduleCount == 0u
        || census.moduleCount > inventory::MaximumDiagnosticModuleCount
        || census.moduleCount != census.postProofModuleCount)
    {
        return false;
    }
    std::size_t primaryMatches = 0u;
    std::size_t postMatches = 0u;
    for (std::size_t index = 0u; index < census.moduleCount; ++index)
    {
        const inventory::ModuleObservation& module = census.modules[index];
        if (module.normalizedModuleName.valid()
            && inventory::asciiCaseInsensitiveEqual(
                module.normalizedModuleName.view(),
                "falloutnv.exe")
            && module.runtimeModuleBase == runtimeImageBase)
        {
            ++primaryMatches;
        }
    }
    for (std::size_t index = 0u; index < census.postProofModuleCount; ++index)
    {
        const inventory::ModuleObservation& module =
            census.postProofModules[index];
        if (module.normalizedModuleName.valid()
            && inventory::asciiCaseInsensitiveEqual(
                module.normalizedModuleName.view(),
                "falloutnv.exe")
            && module.runtimeModuleBase == runtimeImageBase)
        {
            ++postMatches;
        }
    }
    return primaryMatches == 1u && postMatches == 1u;
}

bool censusBoundToDecisionPoint(
    const inventory::ModuleInventoryEvidence& census,
    const inventory::ProcessCreationIdentity& identity,
    std::uint64_t generation,
    std::uintptr_t runtimeImageBase) noexcept
{
    return inventory::processIdentityObserved(identity)
        && generation != 0u
        && inventory::sameProcessCreation(
            census.transactionProcessIdentity,
            identity)
        && census.transactionGeneration == generation
        && censusContainsCurrentMainModule(census, runtimeImageBase);
}

RetailAbiRevalidationResult revalidateSnapshot(
    const MemoryReader& reader,
    std::uintptr_t imageBase,
    const RevalidationContract& contract,
    const inventory::ModuleInventoryEvidence* census,
    const inventory::ProcessCreationIdentity& processIdentity,
    std::uint64_t evidenceGeneration) noexcept
{
    RetailAbiRevalidationResult result {};
    result.failure = RetailAbiRevalidationFailure::InvalidLoadedPe;
    result.diagnostics.runtimeImageBase = imageBase;
    result.diagnostics.processId = processIdentity.processId;
    result.diagnostics.processCreationTime100ns =
        processIdentity.creationTime100ns;

    LoadedPe32 pe {};
    if (!parseLoadedPe32(reader, imageBase, pe))
    {
        result.assessment = assessRetailEngineAbi(result.evidence);
        return result;
    }

    result.evidence.loadedExecutableIdentityMatched =
        pe.machine == IMAGE_FILE_MACHINE_I386
        && pe.timeDateStamp == contract.loadedIdentity.timeDateStamp
        && pe.checksum == contract.loadedIdentity.checksum
        && pe.sizeOfImage == contract.loadedIdentity.sizeOfImage
        && pe.sizeOfImage == contract.sizeOfImage
        && pe.preferredImageBase == contract.preferredImageBase;
    if (!result.evidence.loadedExecutableIdentityMatched)
    {
        result.failure = RetailAbiRevalidationFailure::RuntimeEvidenceRejected;
        result.assessment = assessRetailEngineAbi(result.evidence);
        return result;
    }

    result.evidence.loadedExecutableSectionLayoutAndProtectionsVerified =
        verifyExecutableSectionLayoutAndProtections(
            reader,
            imageBase,
            pe,
            contract,
            result.diagnostics);
    result.evidence.coreManifestMatched = verifyCoreManifest(
        reader,
        imageBase,
        contract,
        result.diagnostics);
    result.evidence.fullFunctionInventoryMatched = verifyFunctionInventory(
        reader,
        imageBase,
        contract,
        result.diagnostics);
    result.evidence.vtableSlotsMatched = verifyVtableSlots(
        reader,
        imageBase,
        contract,
        result.diagnostics);
    result.evidence.vtableBlocksMatched = verifyVtableBlocks(
        reader,
        imageBase,
        contract,
        result.diagnostics);
    result.evidence.liveObjectLayoutsVerified = verifyLiveLayouts(
        reader,
        imageBase,
        contract);
    result.evidence.constructorOwnershipVerified =
        result.evidence.fullFunctionInventoryMatched
        && sealedConstructorOwnership(contract);
    result.evidence.bothWorldBranchesVerified =
        result.evidence.coreManifestMatched
        && result.evidence.fullFunctionInventoryMatched
        && sealedBothWorldBranches(contract);

    const bool bound = census
        && censusBoundToDecisionPoint(
            *census,
            processIdentity,
            evidenceGeneration,
            imageBase);
    const bool censusAccepted = census
        && inventory::assessModuleInventory(*census).overallAccepted;
    const bool compatibilityAccepted = bound && censusAccepted;
    result.evidence.compatibilityModulesVerified = compatibilityAccepted;
    // Synthetic authority requires its owned census to be bound here. The
    // production entry promotes these two fields only after its separate
    // current-process compatibility proof passes below.
    result.evidence.synchronousRuntimeRevalidation = compatibilityAccepted;

    result.assessment = assessRetailEngineAbi(result.evidence);
    result.failure = result.assessment.engineCallsAuthorized
        ? RetailAbiRevalidationFailure::None
        : RetailAbiRevalidationFailure::RuntimeEvidenceRejected;
    return result;
}

// These are exact loaded-image geometry/protection descriptors, observed in
// independent retail processes. They intentionally do not claim that all
// executable bytes are immutable: xNVSE/JIP/ShowOff legitimately patch
// unrelated .text sites, including ASLR-dependent branch targets. Engine-call
// content authority is instead provided by the complete function, core-body,
// vtable-slot, and cloned-vtable-block inventories verified below.
inline constexpr std::array<ExecutableSectionLayoutInternal, 2>
    RetailLoadedExecutableSections {{
        {
            { '.', 't', 'e', 'x', 't', 0u, 0u, 0u },
            0x00001000u,
            0x00BDD38Bu,
            0x00BDD400u,
            0x00BDE000u,
            0x00BDD400u,
            0x60000020u,
            2u,
        },
        {
            { '.', 'b', 'i', 'n', 'd', 0u, 0u, 0u },
            0x01009000u,
            0x00072000u,
            0x00072000u,
            0x00072000u,
            0x00071800u,
            0x60000040u,
            2u,
        },
    }};
static_assert(RetailLoadedExecutableSections.size() == 2u);
static_assert(RetailLoadedExecutableSections[0].rva == 0x00001000u);
static_assert(
    RetailLoadedExecutableSections[0].protectionBytes == 0x00BDE000u);
static_assert(RetailLoadedExecutableSections[1].rva == 0x01009000u);
static_assert(
    RetailLoadedExecutableSections[1].protectionBytes == 0x00072000u);
static_assert(
    RetailLoadedExecutableSections[1].rva
            + RetailLoadedExecutableSections[1].protectionBytes
        == SupportedSizeOfImage);

const RevalidationContract& productionContract() noexcept
{
    static constexpr RevalidationContract Contract {
        SupportedImageBase,
        {
            SupportedPeTimeDateStamp,
            SupportedPeChecksum,
            SupportedSizeOfImage,
        },
        SupportedSizeOfImage,
        RetailLoadedExecutableSections.data(),
        RetailLoadedExecutableSections.size(),
        RetailEngineManifest.data(),
        RetailEngineManifest.size(),
        RetailFunctionAbiInventory.data(),
        RetailFunctionAbiInventory.size(),
        RetailVtableSlots.data(),
        RetailVtableSlots.size(),
        RetailVtableBlocks.data(),
        RetailVtableBlocks.size(),
        SceneGraphSingletonPointerAddress,
        BSCullingProcessVtableAddress,
    };
    return Contract;
}

bool currentProcessIdentity(
    inventory::ProcessCreationIdentity& identity) noexcept
{
    identity = {};
    FILETIME creation {};
    FILETIME exit {};
    FILETIME kernel {};
    FILETIME user {};
    if (!GetProcessTimes(
            GetCurrentProcess(),
            &creation,
            &exit,
            &kernel,
            &user))
    {
        return false;
    }
    ULARGE_INTEGER value {};
    value.LowPart = creation.dwLowDateTime;
    value.HighPart = creation.dwHighDateTime;
    identity.processId = GetCurrentProcessId();
    identity.creationTime100ns = value.QuadPart;
    return inventory::processIdentityObserved(identity);
}

bool currentMainModuleIsFallout(HMODULE module) noexcept
{
    std::array<wchar_t, 32768> path {};
    const DWORD length = GetModuleFileNameW(
        module,
        path.data(),
        static_cast<DWORD>(path.size()));
    if (length == 0u || length >= path.size())
        return false;
    const wchar_t* name = std::wcsrchr(path.data(), L'\\');
    name = name ? name + 1u : path.data();
    return _wcsicmp(name, L"FalloutNV.exe") == 0;
}
}

RetailAbiRevalidationResult revalidateCurrentRetailEngineAbiAtDecisionPoint()
    noexcept
{
    RetailAbiRevalidationResult rejected {};
#if !defined(_M_IX86)
    rejected.failure = RetailAbiRevalidationFailure::UnsupportedHostArchitecture;
    rejected.assessment = assessRetailEngineAbi(rejected.evidence);
    return rejected;
#else
    inventory::ProcessCreationIdentity before {};
    if (!currentProcessIdentity(before))
    {
        rejected.failure = RetailAbiRevalidationFailure::ProcessIdentityUnavailable;
        rejected.assessment = assessRetailEngineAbi(rejected.evidence);
        return rejected;
    }
    HMODULE module = GetModuleHandleW(nullptr);
    if (!module)
    {
        rejected.failure = RetailAbiRevalidationFailure::MainModuleUnavailable;
        rejected.assessment = assessRetailEngineAbi(rejected.evidence);
        return rejected;
    }
    if (!currentMainModuleIsFallout(module))
    {
        rejected.failure = RetailAbiRevalidationFailure::MainModuleIsNotFallout;
        rejected.assessment = assessRetailEngineAbi(rejected.evidence);
        return rejected;
    }

    const CurrentProcessReader reader;
    RetailAbiRevalidationResult result = revalidateSnapshot(
        reader,
        reinterpret_cast<std::uintptr_t>(module),
        productionContract(),
        nullptr,
        before,
        0u);

    // Run the module/overlay proof last. Its two current-process module
    // snapshots surround a second verification of every protected function
    // and vtable range, closing the compatibility window immediately before
    // capability issuance instead of trusting a pre-existing census.
    const compatibility::RetailCompatibilityProof compatibilityProof =
        compatibility::proveCurrentRetailCompatibilityAtDecisionPoint();
    result.compatibilityProof = compatibilityProof;
    if (!compatibilityProof.compatible
        || compatibilityProof.failure
            != compatibility::RetailCompatibilityFailure::None
        || compatibilityProof.diagnostics.runtimeImageBase
            != reinterpret_cast<std::uintptr_t>(module)
        || compatibilityProof.diagnostics.processId != before.processId
        || compatibilityProof.diagnostics.processCreationTime100ns
            != before.creationTime100ns)
    {
        result.evidence.compatibilityModulesVerified = false;
        result.evidence.synchronousRuntimeRevalidation = false;
        result.assessment = assessRetailEngineAbi(result.evidence);
        result.failure =
            RetailAbiRevalidationFailure::CompatibilityProofRejected;
        return result;
    }
    result.evidence.compatibilityModulesVerified = true;
    result.evidence.synchronousRuntimeRevalidation = true;
    result.assessment = assessRetailEngineAbi(result.evidence);
    result.failure = result.assessment.engineCallsAuthorized
        ? RetailAbiRevalidationFailure::None
        : RetailAbiRevalidationFailure::RuntimeEvidenceRejected;

    inventory::ProcessCreationIdentity after {};
    if (!currentProcessIdentity(after)
        || !inventory::sameProcessCreation(before, after))
    {
        result.evidence.synchronousRuntimeRevalidation = false;
        result.evidence.compatibilityModulesVerified = false;
        result.assessment = assessRetailEngineAbi(result.evidence);
        result.failure = RetailAbiRevalidationFailure::ProcessIdentityUnavailable;
    }
    return result;
#endif
}

#if defined(FNVXR_RETAIL_ABI_REVALIDATOR_TEST_AUTHORITY)
namespace testing
{
namespace
{
class SyntheticReader final : public MemoryReader
{
public:
    explicit SyntheticReader(const SyntheticRetailAbiSnapshot& snapshot)
        : snapshot_(snapshot)
    {
    }

    bool read(
        std::uintptr_t address,
        void* destination,
        std::size_t byteCount) const noexcept override
    {
        if (!address || !destination || !byteCount)
            return false;
        std::uintptr_t end = 0u;
        if (!checkedAdd(address, byteCount, end))
            return false;
        std::uintptr_t cursor = address;
        auto* output = static_cast<std::uint8_t*>(destination);
        while (cursor < end)
        {
            const SyntheticByteRange* found = nullptr;
            std::uintptr_t foundEnd = 0u;
            for (std::size_t index = 0u; index < snapshot_.byteRangeCount; ++index)
            {
                const SyntheticByteRange& range = snapshot_.byteRanges[index];
                std::uintptr_t rangeEnd = 0u;
                if (!range.address || !range.bytes || !range.byteCount
                    || !checkedAdd(range.address, range.byteCount, rangeEnd))
                {
                    return false;
                }
                if (cursor >= range.address && cursor < rangeEnd)
                {
                    if (found)
                        return false;
                    found = &range;
                    foundEnd = rangeEnd;
                }
            }
            if (!found)
                return false;
            const std::size_t chunk = static_cast<std::size_t>(
                (std::min)(foundEnd, end) - cursor);
            std::memcpy(
                output,
                found->bytes + (cursor - found->address),
                chunk);
            output += chunk;
            cursor += chunk;
        }
        return true;
    }

    bool query(
        std::uintptr_t address,
        MemoryRegion& region) const noexcept override
    {
        const SyntheticProtectionRange* found = nullptr;
        for (std::size_t index = 0u;
             index < snapshot_.protectionRangeCount;
             ++index)
        {
            const SyntheticProtectionRange& candidate =
                snapshot_.protectionRanges[index];
            std::uintptr_t end = 0u;
            if (!candidate.address || !candidate.byteCount
                || !checkedAdd(candidate.address, candidate.byteCount, end))
            {
                return false;
            }
            if (address >= candidate.address && address < end)
            {
                if (found)
                    return false;
                found = &candidate;
            }
        }
        if (!found)
            return false;

        region.address = found->address;
        region.byteCount = found->byteCount;
        region.committed = found->access != SyntheticMemoryAccess::NoAccess;
        region.readable = found->access != SyntheticMemoryAccess::NoAccess;
        region.writable = found->access == SyntheticMemoryAccess::ReadWrite
            || found->access == SyntheticMemoryAccess::ExecuteReadWrite;
        region.executable = found->access == SyntheticMemoryAccess::ExecuteRead
            || found->access == SyntheticMemoryAccess::ExecuteReadWrite;
        region.guard = found->guard;
        return true;
    }

private:
    const SyntheticRetailAbiSnapshot& snapshot_;
};
}

Sha256Digest sha256ForSyntheticAuthority(
    const std::uint8_t* bytes,
    std::size_t byteCount) noexcept
{
    Sha256Digest result {};
    (void)sha256Bytes(bytes, byteCount, result);
    return result;
}

RetailAbiRevalidationResult revalidateSyntheticRetailAbiAtDecisionPoint(
    const SyntheticRetailAbiSnapshot& snapshot,
    const SyntheticRetailAbiContract& contract) noexcept
{
    RetailAbiRevalidationResult rejected {};
    try
    {
        std::vector<ExecutableSectionLayoutInternal> executableSections;
        executableSections.reserve(contract.executableSectionCount);
        for (std::size_t index = 0u;
             index < contract.executableSectionCount;
             ++index)
        {
            if (!contract.executableSections)
                break;
            const ExecutableSectionLayout& source =
                contract.executableSections[index];
            executableSections.push_back({
                source.name,
                source.rva,
                source.virtualBytes,
                source.mappedBytes,
                source.protectionBytes,
                source.rawBytes,
                source.characteristics,
                source.independentLayoutSamples,
            });
        }
        const RevalidationContract internal {
            contract.preferredImageBase,
            contract.loadedIdentity,
            contract.sizeOfImage,
            executableSections.data(),
            executableSections.size(),
            contract.coreManifest,
            contract.coreManifestCount,
            contract.functionInventory,
            contract.functionInventoryCount,
            contract.vtableSlots,
            contract.vtableSlotCount,
            contract.vtableBlocks,
            contract.vtableBlockCount,
            contract.sceneGraphSingletonPointerAddress,
            contract.bSCullingProcessVtableAddress,
        };
        const SyntheticReader reader(snapshot);
        return revalidateSnapshot(
            reader,
            snapshot.runtimeImageBase,
            internal,
            snapshot.census,
            snapshot.processIdentity,
            snapshot.evidenceGeneration);
    }
    catch (...)
    {
        rejected.failure = RetailAbiRevalidationFailure::LoadedImageReadFailed;
        rejected.assessment = assessRetailEngineAbi(rejected.evidence);
        return rejected;
    }
}
}
#endif
}
