#include "fnvxr_retail_compatibility_proof.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <TlHelp32.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <cwchar>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace fnvxr::engine::compatibility
{
namespace
{
constexpr std::size_t MaximumModuleCount = 512u;
constexpr std::uint16_t PeMachineI386 = 0x014Cu;
constexpr std::uint16_t Pe32Magic = 0x010Bu;

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

struct ModuleRecord
{
    std::wstring baseName {};
    std::wstring path {};
    std::uintptr_t runtimeBase = 0;
    std::size_t runtimeBytes = 0;
    const std::uint8_t* syntheticFileBytes = nullptr;
    std::size_t syntheticFileByteCount = 0;
};

struct ExactModuleSeal
{
    const wchar_t* baseName = nullptr;
    std::uint64_t fileBytes = 0;
    Sha256Digest fileSha256 {};
    std::uint32_t loadedPeTimeDateStamp = 0;
    std::uint32_t loadedPeSizeOfImage = 0;
    std::uint32_t loadedPePreferredImageBase = 0;
};

struct JipRewriteSeal
{
    std::uintptr_t functionPreferredAddress = 0;
    std::size_t functionBytes = 0;
    std::size_t callOffset = 0;
    std::uintptr_t stockTargetPreferredAddress = 0;
    std::uint32_t stubRva = 0;
    std::uint32_t guardVariableRva = 0;
    std::size_t stubBytes = 0;
};

struct CompatibilityContract
{
    std::uintptr_t preferredImageBase = 0;
    LoadedExecutableIdentity loadedIdentity {};
    std::uint32_t sizeOfImage = 0;
    ExactModuleSeal jip {};
    ExactModuleSeal showOff {};
    JipRewriteSeal jipRewrite {};
    const LoadedFunctionManifestEntry* coreManifest = nullptr;
    std::size_t coreManifestCount = 0;
    const abi::RetailFunctionAbiDescriptor* functionInventory = nullptr;
    std::size_t functionInventoryCount = 0;
    const abi::RetailVtableSlotDescriptor* vtableSlots = nullptr;
    std::size_t vtableSlotCount = 0;
    const abi::RetailVtableBlockDescriptor* vtableBlocks = nullptr;
    std::size_t vtableBlockCount = 0;
};

struct LoadedPeIdentity
{
    std::uint16_t machine = 0;
    std::uint32_t timeDateStamp = 0;
    std::uint32_t checksum = 0;
    std::uint32_t sizeOfImage = 0;
    std::uint32_t preferredImageBase = 0;
};

class EvidenceReader
{
public:
    virtual ~EvidenceReader() = default;
    virtual bool read(
        std::uintptr_t address,
        void* destination,
        std::size_t byteCount) const noexcept = 0;
    virtual bool query(
        std::uintptr_t address,
        MemoryRegion& region) const noexcept = 0;
    virtual bool hashModuleFile(
        const ModuleRecord& module,
        Sha256Digest& digest,
        std::uint64_t& fileBytes) const noexcept = 0;
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

bool rangeWithin(
    std::uintptr_t base,
    std::size_t totalBytes,
    std::uintptr_t address,
    std::size_t bytes) noexcept
{
    if (!base || !address || !bytes || address < base)
        return false;
    const std::uintptr_t offset = address - base;
    return offset <= totalBytes && bytes <= totalBytes - offset;
}

bool rangeHasAccess(
    const EvidenceReader& reader,
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

class Sha256Hasher final
{
public:
    Sha256Hasher() noexcept
    {
        DWORD transferred = 0u;
        DWORD objectBytes = 0u;
        DWORD hashBytes = 0u;
        NTSTATUS status = BCryptOpenAlgorithmProvider(
            &algorithm_,
            BCRYPT_SHA256_ALGORITHM,
            nullptr,
            0u);
        if (status >= 0)
        {
            status = BCryptGetProperty(
                algorithm_,
                BCRYPT_OBJECT_LENGTH,
                reinterpret_cast<PUCHAR>(&objectBytes),
                sizeof(objectBytes),
                &transferred,
                0u);
        }
        if (status >= 0)
        {
            status = BCryptGetProperty(
                algorithm_,
                BCRYPT_HASH_LENGTH,
                reinterpret_cast<PUCHAR>(&hashBytes),
                sizeof(hashBytes),
                &transferred,
                0u);
        }
        if (status < 0 || hashBytes != 32u)
            return;
        try
        {
            object_.resize(objectBytes);
        }
        catch (...)
        {
            return;
        }
        status = BCryptCreateHash(
            algorithm_,
            &hash_,
            object_.data(),
            objectBytes,
            nullptr,
            0u,
            0u);
        valid_ = status >= 0;
    }

    ~Sha256Hasher()
    {
        if (hash_)
            BCryptDestroyHash(hash_);
        if (algorithm_)
            BCryptCloseAlgorithmProvider(algorithm_, 0u);
    }

    Sha256Hasher(const Sha256Hasher&) = delete;
    Sha256Hasher& operator=(const Sha256Hasher&) = delete;

    bool update(const std::uint8_t* bytes, std::size_t byteCount) noexcept
    {
        if (!valid_ || (!bytes && byteCount != 0u))
            return false;
        std::size_t offset = 0u;
        while (offset < byteCount)
        {
            const ULONG chunk = static_cast<ULONG>((std::min)(
                byteCount - offset,
                static_cast<std::size_t>((std::numeric_limits<ULONG>::max)())));
            if (BCryptHashData(
                    hash_,
                    const_cast<PUCHAR>(bytes + offset),
                    chunk,
                    0u)
                < 0)
            {
                valid_ = false;
                return false;
            }
            offset += chunk;
        }
        return true;
    }

    bool finish(Sha256Digest& digest) noexcept
    {
        digest = {};
        if (!valid_
            || BCryptFinishHash(
                   hash_,
                   digest.bytes.data(),
                   static_cast<ULONG>(digest.bytes.size()),
                   0u)
                < 0)
        {
            return false;
        }
        digest.valid = true;
        valid_ = false;
        return true;
    }

private:
    BCRYPT_ALG_HANDLE algorithm_ = nullptr;
    BCRYPT_HASH_HANDLE hash_ = nullptr;
    std::vector<std::uint8_t> object_ {};
    bool valid_ = false;
};

bool sha256Bytes(
    const std::uint8_t* bytes,
    std::size_t byteCount,
    Sha256Digest& digest) noexcept
{
    if (!bytes || byteCount == 0u)
    {
        digest = {};
        return false;
    }
    Sha256Hasher hasher;
    return hasher.update(bytes, byteCount) && hasher.finish(digest);
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

bool hashMemory(
    const EvidenceReader& reader,
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
    const CompatibilityContract& contract,
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

bool readLoadedPeIdentity(
    const EvidenceReader& reader,
    const ModuleRecord& module,
    LoadedPeIdentity& identity) noexcept
{
    identity = {};
    IMAGE_DOS_HEADER dos {};
    if (!rangeWithin(
            module.runtimeBase,
            module.runtimeBytes,
            module.runtimeBase,
            sizeof(dos))
        || !reader.read(module.runtimeBase, &dos, sizeof(dos))
        || dos.e_magic != IMAGE_DOS_SIGNATURE
        || dos.e_lfanew < static_cast<LONG>(sizeof(dos)))
    {
        return false;
    }
    std::uintptr_t ntAddress = 0u;
    if (!checkedAdd(
            module.runtimeBase,
            static_cast<std::size_t>(dos.e_lfanew),
            ntAddress)
        || !rangeWithin(
            module.runtimeBase,
            module.runtimeBytes,
            ntAddress,
            sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER)
                + sizeof(IMAGE_OPTIONAL_HEADER32)))
    {
        return false;
    }
    DWORD signature = 0u;
    IMAGE_FILE_HEADER file {};
    IMAGE_OPTIONAL_HEADER32 optional {};
    if (!reader.read(ntAddress, &signature, sizeof(signature))
        || signature != IMAGE_NT_SIGNATURE
        || !reader.read(ntAddress + sizeof(signature), &file, sizeof(file))
        || file.Machine != IMAGE_FILE_MACHINE_I386
        || file.SizeOfOptionalHeader < sizeof(optional)
        || !reader.read(
            ntAddress + sizeof(signature) + sizeof(file),
            &optional,
            sizeof(optional))
        || optional.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
    {
        return false;
    }
    identity.machine = file.Machine;
    identity.timeDateStamp = file.TimeDateStamp;
    identity.checksum = optional.CheckSum;
    identity.sizeOfImage = optional.SizeOfImage;
    identity.preferredImageBase = optional.ImageBase;
    return true;
}

std::wstring lower(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t value) {
        return value >= L'A' && value <= L'Z'
            ? static_cast<wchar_t>(value + (L'a' - L'A'))
            : value;
    });
    return value;
}

bool isProhibitedModule(std::wstring_view name) noexcept
{
    return name == L"gameoverlayrenderer.dll"
        || name == L"gameoverlayrenderer64.dll"
        || name == L"nvspcap.dll"
        || name == L"nvspcap64.dll";
}

bool moduleRecordValid(const ModuleRecord& module) noexcept
{
    if (module.baseName.empty()
        || module.baseName.size() > MAX_PATH
        || module.path.empty()
        || module.runtimeBase == 0u
        || module.runtimeBytes == 0u
        || module.runtimeBase > 0xFFFFFFFFu
        || module.runtimeBytes > 0xFFFFFFFFu)
    {
        return false;
    }
    const std::size_t slash = module.path.find_last_of(L"\\/");
    const std::wstring_view pathName = slash == std::wstring::npos
        ? std::wstring_view(module.path)
        : std::wstring_view(module.path).substr(slash + 1u);
    return pathName == module.baseName;
}

bool moduleSnapshotValid(const std::vector<ModuleRecord>& modules) noexcept
{
    if (modules.empty() || modules.size() > MaximumModuleCount)
        return false;
    for (std::size_t index = 0u; index < modules.size(); ++index)
    {
        if (!moduleRecordValid(modules[index]))
            return false;
        for (std::size_t other = index + 1u; other < modules.size(); ++other)
        {
            if (modules[index].runtimeBase == modules[other].runtimeBase)
                return false;
        }
    }
    return true;
}

bool sameModuleSnapshot(
    std::vector<ModuleRecord> left,
    std::vector<ModuleRecord> right)
{
    if (!moduleSnapshotValid(left)
        || !moduleSnapshotValid(right)
        || left.size() != right.size())
    {
        return false;
    }
    const auto order = [](const ModuleRecord& first, const ModuleRecord& second) {
        if (first.runtimeBase != second.runtimeBase)
            return first.runtimeBase < second.runtimeBase;
        return first.baseName < second.baseName;
    };
    std::sort(left.begin(), left.end(), order);
    std::sort(right.begin(), right.end(), order);
    for (std::size_t index = 0u; index < left.size(); ++index)
    {
        if (left[index].baseName != right[index].baseName
            || left[index].path != right[index].path
            || left[index].runtimeBase != right[index].runtimeBase
            || left[index].runtimeBytes != right[index].runtimeBytes)
        {
            return false;
        }
    }
    return true;
}

const ModuleRecord* uniqueModule(
    const std::vector<ModuleRecord>& modules,
    std::wstring_view name,
    std::size_t& count) noexcept
{
    count = 0u;
    const ModuleRecord* result = nullptr;
    for (const ModuleRecord& module : modules)
    {
        if (module.baseName == name)
        {
            ++count;
            result = &module;
        }
    }
    return result;
}

bool verifyExactModule(
    const EvidenceReader& reader,
    const ModuleRecord& module,
    const ExactModuleSeal& seal) noexcept
{
    if (!seal.baseName
        || module.baseName != lower(seal.baseName)
        || module.runtimeBytes != seal.loadedPeSizeOfImage
        || module.runtimeBase > 0xFFFFFFFFu
        || module.runtimeBytes > 0x100000000ull - module.runtimeBase)
    {
        return false;
    }
    Sha256Digest fileDigest {};
    std::uint64_t fileBytes = 0u;
    LoadedPeIdentity loaded {};
    return reader.hashModuleFile(module, fileDigest, fileBytes)
        && fileBytes == seal.fileBytes
        && digestEqual(fileDigest, seal.fileSha256)
        && readLoadedPeIdentity(reader, module, loaded)
        && loaded.machine == PeMachineI386
        && loaded.timeDateStamp == seal.loadedPeTimeDateStamp
        && loaded.sizeOfImage == seal.loadedPeSizeOfImage
        && loaded.preferredImageBase == seal.loadedPePreferredImageBase;
}

bool descriptorSealed(
    const abi::RetailFunctionAbiDescriptor& descriptor,
    const CompatibilityContract& contract) noexcept
{
    if (!descriptor.name || descriptor.name[0] == '\0'
        || !descriptor.sha256.valid
        || descriptor.byteCount == 0u
        || !descriptor.exactInstructionBoundary
        || !descriptor.stackContractProven
        || !descriptor.argumentSemanticsProven
        || descriptor.independentLoadedSamples
            < abi::MinimumIndependentLoadedSamples
        || descriptor.preferredAddress < contract.preferredImageBase)
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
    if (descriptor.callingConvention == abi::RetailX86CallingConvention::Cdecl)
        return descriptor.calleePopBytes == 0u;
    return descriptor.calleePopBytes
        == static_cast<std::uint8_t>(descriptor.stackArgumentCount * 4u);
}

bool makeExpectedJipStub(
    std::uintptr_t jipRuntimeBase,
    const JipRewriteSeal& rewrite,
    std::array<std::uint8_t, 19>& bytes) noexcept
{
    if (rewrite.stubBytes != bytes.size())
        return false;
    std::uintptr_t guardAddress = 0u;
    if (!checkedAdd(
            jipRuntimeBase,
            rewrite.guardVariableRva,
            guardAddress)
        || guardAddress > 0xFFFFFFFFu)
    {
        return false;
    }
    bytes = {
        0x83u, 0x3Du, 0u, 0u, 0u, 0u, 0x00u, 0x74u, 0x07u,
        0xB8u, 0xC0u, 0x48u, 0x71u, 0x00u, 0xFFu, 0xE0u,
        0xC2u, 0x08u, 0x00u,
    };
    const auto encoded = static_cast<std::uint32_t>(guardAddress);
    bytes[2] = static_cast<std::uint8_t>(encoded);
    bytes[3] = static_cast<std::uint8_t>(encoded >> 8u);
    bytes[4] = static_cast<std::uint8_t>(encoded >> 16u);
    bytes[5] = static_cast<std::uint8_t>(encoded >> 24u);
    return true;
}

bool encodeRel32(
    std::uintptr_t instructionEnd,
    std::uintptr_t target,
    std::array<std::uint8_t, 4>& bytes) noexcept
{
    const std::int64_t displacement = static_cast<std::int64_t>(target)
        - static_cast<std::int64_t>(instructionEnd);
    if (displacement < (std::numeric_limits<std::int32_t>::min)()
        || displacement > (std::numeric_limits<std::int32_t>::max)())
    {
        return false;
    }
    const auto encoded = static_cast<std::uint32_t>(
        static_cast<std::int32_t>(displacement));
    bytes[0] = static_cast<std::uint8_t>(encoded);
    bytes[1] = static_cast<std::uint8_t>(encoded >> 8u);
    bytes[2] = static_cast<std::uint8_t>(encoded >> 16u);
    bytes[3] = static_cast<std::uint8_t>(encoded >> 24u);
    return true;
}

bool readRel32Target(
    const std::vector<std::uint8_t>& function,
    std::uintptr_t functionAddress,
    std::size_t callOffset,
    std::uintptr_t& target) noexcept
{
    target = 0u;
    if (callOffset > function.size()
        || function.size() - callOffset < 5u
        || function[callOffset] != 0xE8u)
    {
        return false;
    }
    std::uint32_t encoded = 0u;
    for (std::size_t index = 0u; index < 4u; ++index)
    {
        encoded |= static_cast<std::uint32_t>(function[callOffset + 1u + index])
            << (index * 8u);
    }
    const std::int32_t displacement = static_cast<std::int32_t>(encoded);
    std::uintptr_t instructionEnd = 0u;
    if (!checkedAdd(functionAddress, callOffset + 5u, instructionEnd))
        return false;
    const std::int64_t decoded = static_cast<std::int64_t>(instructionEnd)
        + displacement;
    if (decoded <= 0
        || static_cast<std::uint64_t>(decoded)
            > (std::numeric_limits<std::uintptr_t>::max)())
    {
        return false;
    }
    target = static_cast<std::uintptr_t>(decoded);
    return true;
}

bool verifyRenderFirstPerson(
    const EvidenceReader& reader,
    const ModuleRecord& mainModule,
    const ModuleRecord* jipModule,
    bool jipExact,
    const CompatibilityContract& contract,
    const LoadedFunctionManifestEntry& manifest,
    bool& normalizationApplied) noexcept
{
    normalizationApplied = false;
    const JipRewriteSeal& rewrite = contract.jipRewrite;
    std::uintptr_t functionAddress = 0u;
    if (manifest.preferredAddress != rewrite.functionPreferredAddress
        || manifest.byteCount != rewrite.functionBytes
        || rewrite.callOffset > rewrite.functionBytes
        || rewrite.functionBytes - rewrite.callOffset < 5u
        || !relocate(
            contract,
            mainModule.runtimeBase,
            rewrite.functionPreferredAddress,
            functionAddress)
        || !rangeWithin(
            mainModule.runtimeBase,
            mainModule.runtimeBytes,
            functionAddress,
            rewrite.functionBytes)
        || !rangeHasAccess(
            reader,
            functionAddress,
            rewrite.functionBytes,
            true,
            true))
    {
        return false;
    }

    try
    {
        std::vector<std::uint8_t> function(rewrite.functionBytes);
        Sha256Digest rawDigest {};
        if (!reader.read(
                functionAddress,
                function.data(),
                function.size())
            || !sha256Bytes(function.data(), function.size(), rawDigest))
        {
            return false;
        }
        if (digestEqual(rawDigest, manifest.sha256))
            return true;
        if (!jipModule || !jipExact)
            return false;

        std::uintptr_t expectedStubAddress = 0u;
        std::uintptr_t actualTarget = 0u;
        if (!checkedAdd(
                jipModule->runtimeBase,
                rewrite.stubRva,
                expectedStubAddress)
            || !rangeWithin(
                jipModule->runtimeBase,
                jipModule->runtimeBytes,
                expectedStubAddress,
                rewrite.stubBytes)
            || !rangeHasAccess(
                reader,
                expectedStubAddress,
                rewrite.stubBytes,
                true,
                true)
            || !readRel32Target(
                function,
                functionAddress,
                rewrite.callOffset,
                actualTarget)
            || actualTarget != expectedStubAddress)
        {
            return false;
        }

        std::array<std::uint8_t, 19> expectedStub {};
        std::array<std::uint8_t, 19> actualStub {};
        if (!makeExpectedJipStub(
                jipModule->runtimeBase,
                rewrite,
                expectedStub)
            || !reader.read(
                expectedStubAddress,
                actualStub.data(),
                actualStub.size())
            || actualStub != expectedStub)
        {
            return false;
        }

        std::uintptr_t stockTarget = 0u;
        std::uintptr_t instructionEnd = 0u;
        std::array<std::uint8_t, 4> stockDisplacement {};
        if (!relocate(
                contract,
                mainModule.runtimeBase,
                rewrite.stockTargetPreferredAddress,
                stockTarget)
            || !checkedAdd(
                functionAddress,
                rewrite.callOffset + 5u,
                instructionEnd)
            || !encodeRel32(
                instructionEnd,
                stockTarget,
                stockDisplacement))
        {
            return false;
        }
        std::copy(
            stockDisplacement.begin(),
            stockDisplacement.end(),
            function.begin() + rewrite.callOffset + 1u);
        Sha256Digest normalizedDigest {};
        normalizationApplied = sha256Bytes(
                                   function.data(),
                                   function.size(),
                                   normalizedDigest)
            && digestEqual(normalizedDigest, manifest.sha256);
        return normalizationApplied;
    }
    catch (...)
    {
        return false;
    }
}

struct PassResult
{
    RetailCompatibilityEvidence evidence {};
    RetailCompatibilityDiagnostics diagnostics {};
    RetailCompatibilityFailure failure = RetailCompatibilityFailure::None;
};

PassResult evaluatePass(
    const EvidenceReader& reader,
    const std::vector<ModuleRecord>& modules,
    const CompatibilityContract& contract) noexcept
{
    PassResult result {};
    result.diagnostics.moduleCount = modules.size();
    if (!moduleSnapshotValid(modules))
    {
        result.failure = RetailCompatibilityFailure::ModuleEnumerationFailed;
        return result;
    }

    std::size_t mainCount = 0u;
    const ModuleRecord* mainModule = uniqueModule(
        modules,
        L"falloutnv.exe",
        mainCount);
    if (mainCount != 1u || !mainModule)
    {
        result.failure = mainCount == 0u
            ? RetailCompatibilityFailure::MainModuleIsNotFallout
            : RetailCompatibilityFailure::MainModuleUnavailable;
        return result;
    }
    result.diagnostics.runtimeImageBase = mainModule->runtimeBase;
    LoadedPeIdentity mainPe {};
    result.evidence.retailExecutableIdentityMatched =
        mainModule->runtimeBytes == contract.sizeOfImage
        && readLoadedPeIdentity(reader, *mainModule, mainPe)
        && mainPe.machine == PeMachineI386
        && mainPe.timeDateStamp == contract.loadedIdentity.timeDateStamp
        && mainPe.checksum == contract.loadedIdentity.checksum
        && mainPe.sizeOfImage == contract.loadedIdentity.sizeOfImage
        && mainPe.sizeOfImage == contract.sizeOfImage
        && mainPe.preferredImageBase == contract.preferredImageBase;
    if (!result.evidence.retailExecutableIdentityMatched)
    {
        result.failure = RetailCompatibilityFailure::RetailExecutableIdentityMismatch;
        return result;
    }

    result.evidence.prohibitedModulesAbsent = true;
    for (const ModuleRecord& module : modules)
    {
        if (isProhibitedModule(module.baseName))
        {
            result.evidence.prohibitedModulesAbsent = false;
            result.failure =
                RetailCompatibilityFailure::ProhibitedOverlayOrCaptureLoaded;
            return result;
        }
    }

    std::size_t jipCount = 0u;
    const ModuleRecord* jipModule = uniqueModule(
        modules,
        lower(contract.jip.baseName ? contract.jip.baseName : L""),
        jipCount);
    result.diagnostics.jipPresent = jipCount != 0u;
    if (jipCount > 1u)
    {
        result.failure = RetailCompatibilityFailure::DuplicateJipModule;
        return result;
    }
    result.evidence.jip5730ExactOrAbsent = jipCount == 0u
        || (jipModule && verifyExactModule(reader, *jipModule, contract.jip));
    if (!result.evidence.jip5730ExactOrAbsent)
    {
        result.failure = RetailCompatibilityFailure::Jip5730IdentityMismatch;
        return result;
    }

    std::size_t showOffCount = 0u;
    const ModuleRecord* showOffModule = uniqueModule(
        modules,
        lower(contract.showOff.baseName ? contract.showOff.baseName : L""),
        showOffCount);
    result.diagnostics.showOffPresent = showOffCount != 0u;
    if (showOffCount > 1u)
    {
        result.failure = RetailCompatibilityFailure::DuplicateShowOffModule;
        return result;
    }
    result.evidence.showOff184ExactOrAbsent = showOffCount == 0u
        || (showOffModule
            && verifyExactModule(reader, *showOffModule, contract.showOff));
    if (!result.evidence.showOff184ExactOrAbsent)
    {
        result.failure = RetailCompatibilityFailure::ShowOff184IdentityMismatch;
        return result;
    }

    if (!contract.coreManifest
        || contract.coreManifestCount != RetailEngineManifest.size())
    {
        result.failure = RetailCompatibilityFailure::ProtectedCoreBodyMismatch;
        return result;
    }
    bool coreMatched = true;
    bool renderFirstPersonFound = false;
    for (std::size_t index = 0u; index < contract.coreManifestCount; ++index)
    {
        const LoadedFunctionManifestEntry& entry = contract.coreManifest[index];
        if (!entry.name || entry.name[0] == '\0'
            || !entry.sha256.valid
            || entry.byteCount == 0u)
        {
            coreMatched = false;
            break;
        }
        if (entry.preferredAddress == contract.jipRewrite.functionPreferredAddress)
        {
            bool normalizationApplied = false;
            const bool matched = verifyRenderFirstPerson(
                reader,
                *mainModule,
                jipModule,
                jipCount == 0u || result.evidence.jip5730ExactOrAbsent,
                contract,
                entry,
                normalizationApplied);
            result.evidence.renderFirstPersonStockOrJipNormalized = matched;
            result.diagnostics.jipNormalizationApplied = normalizationApplied;
            renderFirstPersonFound = true;
            coreMatched = coreMatched && matched;
            if (matched)
                ++result.diagnostics.protectedCoreBodiesHashed;
            continue;
        }
        std::uintptr_t address = 0u;
        Sha256Digest digest {};
        const bool matched = relocate(
                                 contract,
                                 mainModule->runtimeBase,
                                 entry.preferredAddress,
                                 address)
            && rangeWithin(
                mainModule->runtimeBase,
                mainModule->runtimeBytes,
                address,
                entry.byteCount)
            && rangeHasAccess(reader, address, entry.byteCount, true, true)
            && hashMemory(reader, address, entry.byteCount, digest)
            && digestEqual(entry.sha256, digest);
        coreMatched = coreMatched && matched;
        if (matched)
            ++result.diagnostics.protectedCoreBodiesHashed;
    }
    result.evidence.protectedCoreBodiesMatched =
        coreMatched && renderFirstPersonFound;
    if (!result.evidence.renderFirstPersonStockOrJipNormalized)
    {
        result.failure =
            RetailCompatibilityFailure::JipRenderFirstPersonRewriteMismatch;
        return result;
    }
    if (!result.evidence.protectedCoreBodiesMatched)
    {
        result.failure = RetailCompatibilityFailure::ProtectedCoreBodyMismatch;
        return result;
    }

    if (!contract.functionInventory
        || contract.functionInventoryCount != abi::RetailFunctionAbiInventory.size())
    {
        result.failure = RetailCompatibilityFailure::ProtectedFunctionMismatch;
        return result;
    }
    result.evidence.protectedFunctionInventoryMatched = true;
    for (std::size_t index = 0u; index < contract.functionInventoryCount; ++index)
    {
        const abi::RetailFunctionAbiDescriptor& descriptor =
            contract.functionInventory[index];
        std::uintptr_t address = 0u;
        Sha256Digest digest {};
        const bool matched = descriptorSealed(descriptor, contract)
            && relocate(
                contract,
                mainModule->runtimeBase,
                descriptor.preferredAddress,
                address)
            && rangeWithin(
                mainModule->runtimeBase,
                mainModule->runtimeBytes,
                address,
                descriptor.byteCount)
            && rangeHasAccess(
                reader,
                address,
                descriptor.byteCount,
                true,
                true)
            && hashMemory(reader, address, descriptor.byteCount, digest)
            && digestEqual(descriptor.sha256, digest);
        result.evidence.protectedFunctionInventoryMatched =
            result.evidence.protectedFunctionInventoryMatched && matched;
        if (matched)
            ++result.diagnostics.protectedFunctionsHashed;
    }
    if (!result.evidence.protectedFunctionInventoryMatched)
    {
        result.failure = RetailCompatibilityFailure::ProtectedFunctionMismatch;
        return result;
    }

    if (!contract.vtableSlots
        || contract.vtableSlotCount != abi::RetailVtableSlots.size())
    {
        result.failure = RetailCompatibilityFailure::ProtectedVtableSlotMismatch;
        return result;
    }
    result.evidence.protectedVtableSlotsMatched = true;
    for (std::size_t index = 0u; index < contract.vtableSlotCount; ++index)
    {
        const abi::RetailVtableSlotDescriptor& descriptor =
            contract.vtableSlots[index];
        std::uintptr_t vtable = 0u;
        std::uintptr_t slot = 0u;
        std::uintptr_t target = 0u;
        abi::RetailPointer32 loadedTarget = 0u;
        const bool matched = descriptor.owner
            && descriptor.owner[0] != '\0'
            && descriptor.slotByteOffset % sizeof(loadedTarget) == 0u
            && relocate(
                contract,
                mainModule->runtimeBase,
                descriptor.vtableAddress,
                vtable)
            && checkedAdd(vtable, descriptor.slotByteOffset, slot)
            && relocate(
                contract,
                mainModule->runtimeBase,
                descriptor.preferredTargetAddress,
                target)
            && rangeWithin(
                mainModule->runtimeBase,
                mainModule->runtimeBytes,
                slot,
                sizeof(loadedTarget))
            && rangeWithin(
                mainModule->runtimeBase,
                mainModule->runtimeBytes,
                target,
                1u)
            && rangeHasAccess(
                reader,
                slot,
                sizeof(loadedTarget),
                false,
                true)
            && rangeHasAccess(reader, target, 1u, true, true)
            && reader.read(slot, &loadedTarget, sizeof(loadedTarget))
            && loadedTarget == static_cast<abi::RetailPointer32>(target);
        result.evidence.protectedVtableSlotsMatched =
            result.evidence.protectedVtableSlotsMatched && matched;
        if (matched)
            ++result.diagnostics.protectedVtableSlotsRead;
    }
    if (!result.evidence.protectedVtableSlotsMatched)
    {
        result.failure = RetailCompatibilityFailure::ProtectedVtableSlotMismatch;
        return result;
    }

    if (!contract.vtableBlocks
        || contract.vtableBlockCount != abi::RetailVtableBlocks.size())
    {
        result.failure = RetailCompatibilityFailure::ProtectedVtableBlockMismatch;
        return result;
    }
    result.evidence.protectedVtableBlocksMatched = true;
    for (std::size_t index = 0u; index < contract.vtableBlockCount; ++index)
    {
        const abi::RetailVtableBlockDescriptor& descriptor =
            contract.vtableBlocks[index];
        std::uintptr_t address = 0u;
        Sha256Digest digest {};
        const bool matched = descriptor.owner
            && descriptor.owner[0] != '\0'
            && descriptor.byteCount == 0x50u
            && descriptor.sha256.valid
            && descriptor.independentLoadedSamples
                >= abi::MinimumIndependentLoadedSamples
            && relocate(
                contract,
                mainModule->runtimeBase,
                descriptor.preferredAddress,
                address)
            && rangeWithin(
                mainModule->runtimeBase,
                mainModule->runtimeBytes,
                address,
                descriptor.byteCount)
            && rangeHasAccess(
                reader,
                address,
                descriptor.byteCount,
                false,
                true)
            && hashMemory(reader, address, descriptor.byteCount, digest)
            && digestEqual(descriptor.sha256, digest);
        result.evidence.protectedVtableBlocksMatched =
            result.evidence.protectedVtableBlocksMatched && matched;
        if (matched)
            result.diagnostics.protectedVtableBytesHashed += descriptor.byteCount;
    }
    if (!result.evidence.protectedVtableBlocksMatched)
        result.failure = RetailCompatibilityFailure::ProtectedVtableBlockMismatch;
    return result;
}

RetailCompatibilityEvidence combineEvidence(
    const PassResult& before,
    const PassResult& after,
    bool moduleStable,
    bool processStable) noexcept
{
    return {
        before.evidence.retailExecutableIdentityMatched
            && after.evidence.retailExecutableIdentityMatched,
        moduleStable,
        before.evidence.prohibitedModulesAbsent
            && after.evidence.prohibitedModulesAbsent,
        before.evidence.jip5730ExactOrAbsent
            && after.evidence.jip5730ExactOrAbsent,
        before.evidence.showOff184ExactOrAbsent
            && after.evidence.showOff184ExactOrAbsent,
        before.evidence.renderFirstPersonStockOrJipNormalized
            && after.evidence.renderFirstPersonStockOrJipNormalized,
        before.evidence.protectedCoreBodiesMatched
            && after.evidence.protectedCoreBodiesMatched,
        before.evidence.protectedFunctionInventoryMatched
            && after.evidence.protectedFunctionInventoryMatched,
        before.evidence.protectedVtableSlotsMatched
            && after.evidence.protectedVtableSlotsMatched,
        before.evidence.protectedVtableBlocksMatched
            && after.evidence.protectedVtableBlocksMatched,
        moduleStable && processStable
            && before.failure == RetailCompatibilityFailure::None
            && after.failure == RetailCompatibilityFailure::None,
    };
}

bool allEvidence(const RetailCompatibilityEvidence& evidence) noexcept
{
    return evidence.retailExecutableIdentityMatched
        && evidence.moduleSnapshotStable
        && evidence.prohibitedModulesAbsent
        && evidence.jip5730ExactOrAbsent
        && evidence.showOff184ExactOrAbsent
        && evidence.renderFirstPersonStockOrJipNormalized
        && evidence.protectedCoreBodiesMatched
        && evidence.protectedFunctionInventoryMatched
        && evidence.protectedVtableSlotsMatched
        && evidence.protectedVtableBlocksMatched
        && evidence.synchronousSameProcess;
}

RetailCompatibilityProof assembleProof(
    const PassResult& before,
    const PassResult& after,
    bool moduleStable,
    bool processStable) noexcept
{
    RetailCompatibilityProof proof {};
    proof.evidence = combineEvidence(before, after, moduleStable, processStable);
    proof.diagnostics = before.diagnostics;
    proof.compatible = allEvidence(proof.evidence);
    if (proof.compatible)
    {
        proof.failure = RetailCompatibilityFailure::None;
    }
    else if (!moduleStable)
    {
        proof.failure = RetailCompatibilityFailure::ModuleSnapshotChanged;
    }
    else if (!processStable)
    {
        proof.failure = RetailCompatibilityFailure::ProcessIdentityUnavailable;
    }
    else if (before.failure != RetailCompatibilityFailure::None)
    {
        proof.failure = before.failure;
    }
    else if (after.failure != RetailCompatibilityFailure::None)
    {
        proof.failure = after.failure;
    }
    else
    {
        proof.failure = RetailCompatibilityFailure::ProtectedMemoryUnstable;
    }
    return proof;
}

inline constexpr ExactModuleSeal Jip5730 {
    L"jip_nvse.dll",
    502272u,
    sha256FromHex(
        "9D2779647ED0CE63043390F47FC978E3234AF8E558DC6CB6BCB231478A2D74D4"),
    0x665225A8u,
    0x00080000u,
    0x10000000u,
};

inline constexpr ExactModuleSeal ShowOff184 {
    L"showoffnvse.dll",
    1091584u,
    sha256FromHex(
        "37CB22C5288FEDD0D57196C8C2F6BBABA5A1DAFD9CE58F14DAC9410DBEE7EF3E"),
    0x69C84C9Bu,
    0x00110000u,
    0x10000000u,
};

inline constexpr JipRewriteSeal Jip5730RenderFirstPerson {
    FirstPersonRenderAddress,
    3361u,
    0x00B6u,
    0x007148C0u,
    0x00010880u,
    0x0006A188u,
    19u,
};

const CompatibilityContract& productionContract() noexcept
{
    static constexpr CompatibilityContract Contract {
        SupportedImageBase,
        {
            SupportedPeTimeDateStamp,
            SupportedPeChecksum,
            SupportedSizeOfImage,
        },
        SupportedSizeOfImage,
        Jip5730,
        ShowOff184,
        Jip5730RenderFirstPerson,
        RetailEngineManifest.data(),
        RetailEngineManifest.size(),
        abi::RetailFunctionAbiInventory.data(),
        abi::RetailFunctionAbiInventory.size(),
        abi::RetailVtableSlots.data(),
        abi::RetailVtableSlots.size(),
        abi::RetailVtableBlocks.data(),
        abi::RetailVtableBlocks.size(),
    };
    return Contract;
}

class CurrentProcessReader final : public EvidenceReader
{
public:
    bool read(
        std::uintptr_t address,
        void* destination,
        std::size_t byteCount) const noexcept override
    {
        if (!address || !destination || !byteCount)
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
        const DWORD base = protection & 0xFFu;
        region.address = reinterpret_cast<std::uintptr_t>(
            information.BaseAddress);
        region.byteCount = information.RegionSize;
        region.committed = information.State == MEM_COMMIT;
        region.guard = (protection & PAGE_GUARD) != 0u;
        region.readable = base == PAGE_READONLY
            || base == PAGE_READWRITE
            || base == PAGE_WRITECOPY
            || base == PAGE_EXECUTE
            || base == PAGE_EXECUTE_READ
            || base == PAGE_EXECUTE_READWRITE
            || base == PAGE_EXECUTE_WRITECOPY;
        region.writable = base == PAGE_READWRITE
            || base == PAGE_WRITECOPY
            || base == PAGE_EXECUTE_READWRITE
            || base == PAGE_EXECUTE_WRITECOPY;
        region.executable = base == PAGE_EXECUTE
            || base == PAGE_EXECUTE_READ
            || base == PAGE_EXECUTE_READWRITE
            || base == PAGE_EXECUTE_WRITECOPY;
        return region.address != 0u && region.byteCount != 0u;
    }

    bool hashModuleFile(
        const ModuleRecord& module,
        Sha256Digest& digest,
        std::uint64_t& fileBytes) const noexcept override
    {
        digest = {};
        fileBytes = 0u;
        const HANDLE file = CreateFileW(
            module.path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;
        LARGE_INTEGER size {};
        if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0)
        {
            CloseHandle(file);
            return false;
        }
        fileBytes = static_cast<std::uint64_t>(size.QuadPart);
        Sha256Hasher hasher;
        std::array<std::uint8_t, 65536> buffer {};
        bool complete = true;
        for (;;)
        {
            DWORD readBytes = 0u;
            if (!ReadFile(
                    file,
                    buffer.data(),
                    static_cast<DWORD>(buffer.size()),
                    &readBytes,
                    nullptr))
            {
                complete = false;
                break;
            }
            if (readBytes == 0u)
                break;
            if (!hasher.update(buffer.data(), readBytes))
            {
                complete = false;
                break;
            }
        }
        CloseHandle(file);
        return complete && hasher.finish(digest);
    }
};

bool enumerateCurrentModules(std::vector<ModuleRecord>& modules) noexcept
{
    modules.clear();
    const HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
        GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE)
        return false;
    MODULEENTRY32W entry {};
    entry.dwSize = sizeof(entry);
    if (!Module32FirstW(snapshot, &entry))
    {
        CloseHandle(snapshot);
        return false;
    }
    bool complete = true;
    try
    {
        do
        {
            if (modules.size() == MaximumModuleCount)
            {
                complete = false;
                break;
            }
            ModuleRecord module {};
            module.baseName = lower(entry.szModule);
            module.path = lower(entry.szExePath);
            module.runtimeBase = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
            module.runtimeBytes = entry.modBaseSize;
            modules.push_back(std::move(module));
            entry.dwSize = sizeof(entry);
        } while (Module32NextW(snapshot, &entry));
        if (complete && GetLastError() != ERROR_NO_MORE_FILES)
            complete = false;
    }
    catch (...)
    {
        complete = false;
    }
    CloseHandle(snapshot);
    return complete && moduleSnapshotValid(modules);
}

bool currentProcessIdentity(
    std::uint32_t& processId,
    std::uint64_t& creationTime) noexcept
{
    processId = 0u;
    creationTime = 0u;
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
    processId = GetCurrentProcessId();
    creationTime = value.QuadPart;
    return processId != 0u && creationTime != 0u;
}

bool currentMainModuleIsFallout(HMODULE module) noexcept
{
    if (!module)
        return false;
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

bool snapshotMainBaseMatches(
    const std::vector<ModuleRecord>& modules,
    HMODULE mainModule) noexcept
{
    std::size_t count = 0u;
    const ModuleRecord* record = uniqueModule(
        modules,
        L"falloutnv.exe",
        count);
    return count == 1u
        && record
        && record->runtimeBase
            == reinterpret_cast<std::uintptr_t>(mainModule);
}
}

RetailCompatibilityProof proveCurrentRetailCompatibilityAtDecisionPoint()
    noexcept
{
    RetailCompatibilityProof rejected {};
#if !defined(_M_IX86)
    rejected.failure = RetailCompatibilityFailure::UnsupportedHostArchitecture;
    return rejected;
#else
    const HMODULE mainModule = GetModuleHandleW(nullptr);
    if (!currentMainModuleIsFallout(mainModule))
    {
        rejected.failure = RetailCompatibilityFailure::MainModuleIsNotFallout;
        return rejected;
    }
    std::uint32_t processBefore = 0u;
    std::uint64_t creationBefore = 0u;
    if (!currentProcessIdentity(processBefore, creationBefore))
    {
        rejected.failure = RetailCompatibilityFailure::ProcessIdentityUnavailable;
        return rejected;
    }
    std::vector<ModuleRecord> modulesBefore;
    if (!enumerateCurrentModules(modulesBefore))
    {
        rejected.failure = RetailCompatibilityFailure::ModuleEnumerationFailed;
        return rejected;
    }
    if (!snapshotMainBaseMatches(modulesBefore, mainModule))
    {
        rejected.failure = RetailCompatibilityFailure::MainModuleUnavailable;
        return rejected;
    }

    const CurrentProcessReader reader;
    const PassResult before = evaluatePass(
        reader,
        modulesBefore,
        productionContract());

    std::vector<ModuleRecord> modulesAfter;
    if (!enumerateCurrentModules(modulesAfter))
    {
        rejected.failure = RetailCompatibilityFailure::ModuleEnumerationFailed;
        return rejected;
    }
    if (!snapshotMainBaseMatches(modulesAfter, mainModule))
    {
        rejected.failure = RetailCompatibilityFailure::MainModuleUnavailable;
        return rejected;
    }
    const PassResult after = evaluatePass(
        reader,
        modulesAfter,
        productionContract());

    std::uint32_t processAfter = 0u;
    std::uint64_t creationAfter = 0u;
    const bool processStable = currentProcessIdentity(
                                   processAfter,
                                   creationAfter)
        && processBefore == processAfter
        && creationBefore == creationAfter;
    RetailCompatibilityProof proof = assembleProof(
        before,
        after,
        sameModuleSnapshot(modulesBefore, modulesAfter),
        processStable);
    proof.diagnostics.processId = processBefore;
    proof.diagnostics.processCreationTime100ns = creationBefore;
    return proof;
#endif
}

#if defined(FNVXR_RETAIL_COMPATIBILITY_PROOF_TEST_AUTHORITY)
namespace testing
{
namespace
{
class SyntheticReader final : public EvidenceReader
{
public:
    explicit SyntheticReader(const SyntheticRetailCompatibilitySnapshot& snapshot)
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

    bool hashModuleFile(
        const ModuleRecord& module,
        Sha256Digest& digest,
        std::uint64_t& fileBytes) const noexcept override
    {
        fileBytes = module.syntheticFileByteCount;
        return sha256Bytes(
            module.syntheticFileBytes,
            module.syntheticFileByteCount,
            digest);
    }

private:
    const SyntheticRetailCompatibilitySnapshot& snapshot_;
};

bool convertModules(
    const SyntheticModule* source,
    std::size_t count,
    std::vector<ModuleRecord>& destination)
{
    destination.clear();
    if (!source || count == 0u || count > MaximumModuleCount)
        return false;
    destination.reserve(count);
    for (std::size_t index = 0u; index < count; ++index)
    {
        if (!source[index].baseName || !source[index].path)
            return false;
        destination.push_back({
            lower(source[index].baseName),
            lower(source[index].path),
            source[index].runtimeBase,
            source[index].runtimeBytes,
            source[index].exactFileBytes,
            source[index].exactFileByteCount,
        });
    }
    return moduleSnapshotValid(destination);
}
}

Sha256Digest sha256ForSyntheticCompatibilityAuthority(
    const std::uint8_t* bytes,
    std::size_t byteCount) noexcept
{
    Sha256Digest digest {};
    (void)sha256Bytes(bytes, byteCount, digest);
    return digest;
}

RetailCompatibilityProof proveSyntheticRetailCompatibilityAtDecisionPoint(
    const SyntheticRetailCompatibilitySnapshot& snapshot,
    const SyntheticRetailCompatibilityContract& contract) noexcept
{
    RetailCompatibilityProof rejected {};
    try
    {
        std::vector<ModuleRecord> beforeModules;
        std::vector<ModuleRecord> afterModules;
        if (!convertModules(
                snapshot.modulesBefore,
                snapshot.moduleCountBefore,
                beforeModules)
            || !convertModules(
                snapshot.modulesAfter,
                snapshot.moduleCountAfter,
                afterModules))
        {
            rejected.failure = RetailCompatibilityFailure::ModuleEnumerationFailed;
            return rejected;
        }
        const CompatibilityContract internal {
            contract.preferredImageBase,
            contract.loadedIdentity,
            contract.sizeOfImage,
            {
                contract.jip.baseName,
                contract.jip.fileBytes,
                contract.jip.fileSha256,
                contract.jip.loadedPeTimeDateStamp,
                contract.jip.loadedPeSizeOfImage,
                contract.jip.loadedPePreferredImageBase,
            },
            {
                contract.showOff.baseName,
                contract.showOff.fileBytes,
                contract.showOff.fileSha256,
                contract.showOff.loadedPeTimeDateStamp,
                contract.showOff.loadedPeSizeOfImage,
                contract.showOff.loadedPePreferredImageBase,
            },
            {
                contract.jipRewrite.functionPreferredAddress,
                contract.jipRewrite.functionBytes,
                contract.jipRewrite.callOffset,
                contract.jipRewrite.stockTargetPreferredAddress,
                contract.jipRewrite.stubRva,
                contract.jipRewrite.guardVariableRva,
                contract.jipRewrite.stubBytes,
            },
            contract.coreManifest,
            contract.coreManifestCount,
            contract.functionInventory,
            contract.functionInventoryCount,
            contract.vtableSlots,
            contract.vtableSlotCount,
            contract.vtableBlocks,
            contract.vtableBlockCount,
        };
        const SyntheticReader reader(snapshot);
        const PassResult before = evaluatePass(reader, beforeModules, internal);
        const PassResult after = evaluatePass(reader, afterModules, internal);
        const bool processStable = snapshot.processBefore.processId != 0u
            && snapshot.processBefore.creationTime100ns != 0u
            && snapshot.processBefore.processId == snapshot.processAfter.processId
            && snapshot.processBefore.creationTime100ns
                == snapshot.processAfter.creationTime100ns;
        return assembleProof(
            before,
            after,
            sameModuleSnapshot(beforeModules, afterModules),
            processStable);
    }
    catch (...)
    {
        rejected.failure = RetailCompatibilityFailure::ProtectedMemoryUnstable;
        return rejected;
    }
}
}
#endif
}
