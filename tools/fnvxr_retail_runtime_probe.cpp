#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>

#include "fnvxr_engine_capability.h"
#include "fnvxr_jip_normalization.h"
#include "fnvxr_probe_module_lookup.h"
#include "fnvxr_retail_abi_map.h"
#include "fnvxr_retail_engine_manifest.h"
#include "fnvxr_showoff_compatibility.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cwctype>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace
{
constexpr std::uintptr_t PreferredWorldSceneGraphPointer = 0x011DEB7Cu;
constexpr std::size_t SceneGraphCameraOffset = 0xACu;
constexpr std::size_t SceneGraphCullerOffset = 0xB4u;

struct ProcessModule
{
    DWORD processId = 0;
    std::uintptr_t base = 0;
    std::size_t size = 0;
    std::wstring path;
};

struct LoadedModulePeIdentity
{
    std::uint32_t timeDateStamp = 0;
    std::uint32_t sizeOfImage = 0;
    std::uint32_t preferredImageBase = 0;
};

std::wstring lower(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(std::towlower(c));
    });
    return value;
}

std::vector<DWORD> findFalloutProcesses()
{
    std::vector<DWORD> result;
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return result;

    PROCESSENTRY32W entry {};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (lower(entry.szExeFile) == L"falloutnv.exe")
                result.push_back(entry.th32ProcessID);
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return result;
}

fnvxr::probe::ModuleLookupResult findModule(
    DWORD processId,
    const wchar_t* moduleName,
    ProcessModule& result)
{
    result = {};
    if (!moduleName || !*moduleName)
        return fnvxr::probe::ModuleLookupResult::EnumerationFailed;
    const std::wstring wanted = lower(moduleName);
    const HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
        processId);
    if (snapshot == INVALID_HANDLE_VALUE)
        return fnvxr::probe::ModuleLookupResult::EnumerationFailed;

    MODULEENTRY32W entry {};
    entry.dwSize = sizeof(entry);
    if (!Module32FirstW(snapshot, &entry))
    {
        CloseHandle(snapshot);
        return fnvxr::probe::ModuleLookupResult::EnumerationFailed;
    }

    for (;;)
    {
        if (lower(entry.szModule) == wanted)
        {
            result.processId = processId;
            result.base = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
            result.size = static_cast<std::size_t>(entry.modBaseSize);
            result.path = entry.szExePath;
            CloseHandle(snapshot);
            return fnvxr::probe::ModuleLookupResult::Found;
        }

        SetLastError(ERROR_SUCCESS);
        if (Module32NextW(snapshot, &entry))
            continue;

        const DWORD enumerationError = GetLastError();
        CloseHandle(snapshot);
        return enumerationError == ERROR_NO_MORE_FILES
            ? fnvxr::probe::ModuleLookupResult::NotFound
            : fnvxr::probe::ModuleLookupResult::EnumerationFailed;
    }
}

std::uintptr_t relocate(std::uintptr_t moduleBase, std::uintptr_t preferredAddress)
{
    return moduleBase + (preferredAddress - fnvxr::engine::SupportedImageBase);
}

bool addressInModule(const ProcessModule& module, std::uintptr_t address)
{
    return fnvxr::probe::abi::rangeContained(
        module.base,
        module.size,
        address,
        1u);
}

bool modulesAreDisjoint(const ProcessModule& left, const ProcessModule& right)
{
    if (left.base == 0u || left.size == 0u || right.base == 0u || right.size == 0u)
        return false;
    if (left.size > (std::numeric_limits<std::uintptr_t>::max)() - left.base
        || right.size > (std::numeric_limits<std::uintptr_t>::max)() - right.base)
    {
        return false;
    }
    const std::uintptr_t leftEnd = left.base + left.size;
    const std::uintptr_t rightEnd = right.base + right.size;
    return leftEnd <= right.base || rightEnd <= left.base;
}

const char* showOffFailureName(
    fnvxr::probe::showoff::ShowOff184CompatibilityFailure failure)
{
    using Failure = fnvxr::probe::showoff::ShowOff184CompatibilityFailure;
    switch (failure)
    {
        case Failure::None: return "NONE";
        case Failure::ModuleNotLoaded: return "MODULE_NOT_LOADED";
        case Failure::FileHashUnavailable: return "FILE_HASH_UNAVAILABLE";
        case Failure::FileHashMismatch: return "FILE_HASH_MISMATCH";
        case Failure::FileBytesMismatch: return "FILE_BYTES_MISMATCH";
        case Failure::PeTimeDateStampMismatch: return "PE_TIMESTAMP_MISMATCH";
        case Failure::PeSizeOfImageMismatch: return "PE_IMAGE_SIZE_MISMATCH";
        case Failure::PePreferredImageBaseMismatch: return "PE_IMAGE_BASE_MISMATCH";
        case Failure::RuntimeModuleBoundsInvalid: return "RUNTIME_BOUNDS_INVALID";
        case Failure::SynchronousEvidenceNotCaptured: return "SYNC_EVIDENCE_MISSING";
        case Failure::LoadedExecutableSectionsUnverified: return "LOADED_EXECUTABLE_SECTIONS_UNVERIFIED";
        case Failure::ProtectedManifestUnverified: return "MANIFEST_UNVERIFIED";
        case Failure::ProtectedAbiRangesUnverified: return "ABI_RANGES_UNVERIFIED";
        case Failure::ProtectedVtableSlotsUnverified: return "VTABLES_UNVERIFIED";
        case Failure::JipFirstPersonProofInconsistent: return "JIP_PROOF_INCONSISTENT";
        case Failure::SceneGraphSingletonUnverified: return "SCENE_GRAPH_UNVERIFIED";
        case Failure::ProtectedTargetScanUnverified: return "PROTECTED_TARGET_SCAN_UNVERIFIED";
    }
    return "UNKNOWN";
}

const char* retailEngineAbiFailureName(
    fnvxr::engine::abi::RetailEngineAbiFailure failure)
{
    using Failure = fnvxr::engine::abi::RetailEngineAbiFailure;
    switch (failure)
    {
        case Failure::None: return "NONE";
        case Failure::UnsupportedExecutable: return "UNSUPPORTED_EXECUTABLE";
        case Failure::LoadedExecutableSectionsUnverified: return "LOADED_EXECUTABLE_SECTIONS_UNVERIFIED";
        case Failure::CoreManifestUnverified: return "CORE_MANIFEST_UNVERIFIED";
        case Failure::StaticFunctionInventoryIncomplete: return "STATIC_FUNCTION_INVENTORY_INCOMPLETE";
        case Failure::StaticVtableBlockInventoryIncomplete: return "STATIC_VTABLE_BLOCK_INVENTORY_INCOMPLETE";
        case Failure::RuntimeFunctionInventoryUnverified: return "RUNTIME_FUNCTION_INVENTORY_UNVERIFIED";
        case Failure::RuntimeVtableSlotsUnverified: return "RUNTIME_VTABLE_SLOTS_UNVERIFIED";
        case Failure::RuntimeVtableBlocksUnverified: return "RUNTIME_VTABLE_BLOCKS_UNVERIFIED";
        case Failure::LiveObjectLayoutsUnverified: return "LIVE_OBJECT_LAYOUTS_UNVERIFIED";
        case Failure::ConstructorOwnershipUnverified: return "CONSTRUCTOR_OWNERSHIP_UNVERIFIED";
        case Failure::BothWorldBranchesUnverified: return "BOTH_WORLD_BRANCHES_UNVERIFIED";
        case Failure::CompatibilityModulesUnverified: return "COMPATIBILITY_MODULES_UNVERIFIED";
        case Failure::SynchronousRuntimeRevalidationMissing: return "SYNCHRONOUS_REVALIDATION_MISSING";
    }
    return "UNKNOWN";
}

template <typename T>
bool readValue(HANDLE process, std::uintptr_t address, T& value)
{
    SIZE_T read = 0;
    return ReadProcessMemory(
               process,
               reinterpret_cast<const void*>(address),
               &value,
               sizeof(value),
               &read)
        && read == sizeof(value);
}

bool readBytes(
    HANDLE process,
    std::uintptr_t address,
    std::size_t byteCount,
    std::vector<std::uint8_t>& result)
{
    result.clear();
    if (!process || byteCount == 0u)
        return false;
    result.resize(byteCount);
    SIZE_T read = 0;
    const bool complete = ReadProcessMemory(
                              process,
                              reinterpret_cast<const void*>(address),
                              result.data(),
                              result.size(),
                              &read)
            != FALSE
        && read == result.size();
    if (!complete)
        result.clear();
    return complete;
}

bool isExecutableProtection(DWORD protection)
{
    if ((protection & (PAGE_GUARD | PAGE_NOACCESS)) != 0u)
        return false;
    switch (protection & 0xFFu)
    {
        case PAGE_EXECUTE:
        case PAGE_EXECUTE_READ:
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY:
            return true;
        default:
            return false;
    }
}

bool isCommittedExecutableRange(
    HANDLE process,
    std::uintptr_t address,
    std::size_t byteCount)
{
    if (!process
        || byteCount == 0u
        || byteCount > (std::numeric_limits<std::uintptr_t>::max)() - address)
    {
        return false;
    }

    const std::uintptr_t end = address + byteCount;
    std::uintptr_t cursor = address;
    while (cursor < end)
    {
        MEMORY_BASIC_INFORMATION region {};
        if (VirtualQueryEx(
                process,
                reinterpret_cast<const void*>(cursor),
                &region,
                sizeof(region))
            != sizeof(region))
        {
            return false;
        }

        const std::uintptr_t regionBase = reinterpret_cast<std::uintptr_t>(
            region.BaseAddress);
        if (cursor < regionBase
            || region.RegionSize == 0u
            || region.RegionSize
                > (std::numeric_limits<std::uintptr_t>::max)() - regionBase
            || region.State != MEM_COMMIT
            || !isExecutableProtection(region.Protect))
        {
            return false;
        }
        const std::uintptr_t regionEnd = regionBase + region.RegionSize;
        if (cursor >= regionEnd)
            return false;
        cursor = regionEnd < end ? regionEnd : end;
    }
    return true;
}

std::string hexBytes(const std::uint8_t* bytes, std::size_t count)
{
    std::ostringstream output;
    output << std::hex << std::uppercase << std::setfill('0');
    for (std::size_t index = 0; index < count; ++index)
    {
        if (index != 0)
            output << ' ';
        output << std::setw(2) << static_cast<unsigned>(bytes[index]);
    }
    return output.str();
}

bool calculateSha256(
    const std::uint8_t* bytes,
    std::size_t byteCount,
    fnvxr::engine::Sha256Digest& result)
{
    result = {};
    if (!bytes
        || byteCount == 0
        || byteCount > (std::numeric_limits<ULONG>::max)())
        return false;

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectBytes = 0;
    DWORD digestBytes = 0;
    DWORD copied = 0;
    std::vector<std::uint8_t> object;

    bool success = BCryptOpenAlgorithmProvider(
                       &algorithm,
                       BCRYPT_SHA256_ALGORITHM,
                       nullptr,
                       0)
            >= 0
        && BCryptGetProperty(
               algorithm,
               BCRYPT_OBJECT_LENGTH,
               reinterpret_cast<PUCHAR>(&objectBytes),
               sizeof(objectBytes),
               &copied,
               0)
            >= 0
        && BCryptGetProperty(
               algorithm,
               BCRYPT_HASH_LENGTH,
               reinterpret_cast<PUCHAR>(&digestBytes),
               sizeof(digestBytes),
               &copied,
               0)
            >= 0
        && digestBytes == result.bytes.size();

    if (success)
    {
        object.resize(objectBytes);
        success = BCryptCreateHash(
                      algorithm,
                      &hash,
                      object.data(),
                      static_cast<ULONG>(object.size()),
                      nullptr,
                      0,
                      0)
                >= 0
            && BCryptHashData(
                   hash,
                   const_cast<PUCHAR>(reinterpret_cast<const UCHAR*>(bytes)),
                   static_cast<ULONG>(byteCount),
                   0)
                >= 0
            && BCryptFinishHash(
                   hash,
                   result.bytes.data(),
                   static_cast<ULONG>(result.bytes.size()),
                   0)
                >= 0;
    }

    if (hash)
        BCryptDestroyHash(hash);
    if (algorithm)
        BCryptCloseAlgorithmProvider(algorithm, 0);
    result.valid = success;
    return fnvxr::probe::abi::finalizeComputedSha256Digest(success, result);
}

bool readLoadedNtHeaders32(
    HANDLE process,
    std::uintptr_t moduleBase,
    std::size_t moduleBytes,
    IMAGE_NT_HEADERS32& result)
{
    result = {};
    if (!fnvxr::probe::abi::rangeContained(
            moduleBase,
            moduleBytes,
            moduleBase,
            sizeof(IMAGE_DOS_HEADER)))
    {
        return false;
    }
    IMAGE_DOS_HEADER dos {};
    if (!readValue(process, moduleBase, dos)
        || dos.e_magic != IMAGE_DOS_SIGNATURE
        || dos.e_lfanew <= 0)
    {
        return false;
    }

    std::uintptr_t ntAddress = 0;
    if (!fnvxr::probe::abi::checkedAddAddress(
            moduleBase,
            static_cast<std::uintptr_t>(dos.e_lfanew),
            ntAddress)
        || !fnvxr::probe::abi::rangeContained(
            moduleBase,
            moduleBytes,
            ntAddress,
            sizeof(IMAGE_NT_HEADERS32))
        || !readValue(process, ntAddress, result)
        || result.Signature != IMAGE_NT_SIGNATURE
        || result.FileHeader.Machine != IMAGE_FILE_MACHINE_I386
        || result.FileHeader.SizeOfOptionalHeader != sizeof(IMAGE_OPTIONAL_HEADER32)
        || result.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
    {
        return false;
    }
    return true;
}

bool readLoadedExecutableIdentity(
    HANDLE process,
    std::uintptr_t moduleBase,
    std::size_t moduleBytes,
    fnvxr::engine::LoadedExecutableIdentity& result)
{
    IMAGE_NT_HEADERS32 nt {};
    if (!readLoadedNtHeaders32(process, moduleBase, moduleBytes, nt))
        return false;

    result.timeDateStamp = nt.FileHeader.TimeDateStamp;
    result.checksum = nt.OptionalHeader.CheckSum;
    result.sizeOfImage = nt.OptionalHeader.SizeOfImage;
    return true;
}

bool readLoadedModulePeIdentity(
    HANDLE process,
    std::uintptr_t moduleBase,
    std::size_t moduleBytes,
    LoadedModulePeIdentity& result)
{
    result = {};
    IMAGE_NT_HEADERS32 nt {};
    if (!readLoadedNtHeaders32(process, moduleBase, moduleBytes, nt))
        return false;

    result.timeDateStamp = nt.FileHeader.TimeDateStamp;
    result.sizeOfImage = nt.OptionalHeader.SizeOfImage;
    result.preferredImageBase = nt.OptionalHeader.ImageBase;
    return true;
}

bool hashFileSha256(
    const std::wstring& path,
    std::uint64_t& fileBytes,
    fnvxr::engine::Sha256Digest& digest)
{
    fileBytes = 0;
    digest = {};
    const HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    LARGE_INTEGER size {};
    bool success = GetFileSizeEx(file, &size) != FALSE
        && size.QuadPart > 0
        && static_cast<unsigned long long>(size.QuadPart)
            <= (std::numeric_limits<std::size_t>::max)()
        && static_cast<unsigned long long>(size.QuadPart)
            <= (std::numeric_limits<ULONG>::max)();
    std::vector<std::uint8_t> bytes;
    if (success)
    {
        fileBytes = static_cast<std::uint64_t>(size.QuadPart);
        bytes.resize(static_cast<std::size_t>(fileBytes));
        std::size_t offset = 0;
        while (offset < bytes.size())
        {
            const std::size_t remaining = bytes.size() - offset;
            const DWORD requested = static_cast<DWORD>(
                remaining < (std::numeric_limits<DWORD>::max)()
                    ? remaining
                    : (std::numeric_limits<DWORD>::max)());
            DWORD read = 0;
            if (!ReadFile(
                    file,
                    bytes.data() + offset,
                    requested,
                    &read,
                    nullptr)
                || read != requested)
            {
                success = false;
                break;
            }
            offset += read;
        }
    }
    CloseHandle(file);

    return success
        && calculateSha256(bytes.data(), bytes.size(), digest);
}

bool printManifestProof(
    HANDLE process,
    std::uintptr_t moduleBase,
    std::size_t moduleBytes,
    const fnvxr::engine::LoadedFunctionManifestEntry& entry)
{
    const std::uintptr_t address = relocate(moduleBase, entry.preferredAddress);
    const bool inMainModule = fnvxr::probe::abi::rangeContained(
        moduleBase,
        moduleBytes,
        address,
        entry.byteCount);
    const bool executable = inMainModule
        && isCommittedExecutableRange(process, address, entry.byteCount);
    std::vector<std::uint8_t> bytes;
    const bool readable = executable
        && readBytes(process, address, entry.byteCount, bytes);

    fnvxr::engine::Sha256Digest actual {};
    const bool hashed = readable && calculateSha256(bytes.data(), bytes.size(), actual);
    const bool matches = executable
        && hashed
        && fnvxr::engine::digestMatches(
            entry.sha256,
            actual.bytes.data(),
            actual.bytes.size());

    std::cout << "manifest=" << entry.name
              << " preferred=0x" << std::hex << std::uppercase << entry.preferredAddress
              << " runtime=0x" << address << std::dec
              << " bytes=" << entry.byteCount
              << " in_main_module=" << (inMainModule ? 1 : 0)
              << " executable=" << (executable ? 1 : 0)
              << " readable=" << (readable ? 1 : 0)
              << " sha256=" << (hashed
                      ? hexBytes(actual.bytes.data(), actual.bytes.size())
                      : "UNAVAILABLE")
              << " proof=" << (matches ? "MATCH" : "MISMATCH") << '\n';
    return matches;
}

template <typename Entry>
bool printAbiEvidenceProof(
    HANDLE process,
    std::uintptr_t moduleBase,
    std::size_t moduleBytes,
    const Entry& entry)
{
    const std::uintptr_t address = relocate(moduleBase, entry.preferredAddress);
    const bool inMainModule = fnvxr::probe::abi::rangeContained(
        moduleBase,
        moduleBytes,
        address,
        entry.byteCount);
    const bool executable = inMainModule
        && isCommittedExecutableRange(process, address, entry.byteCount);
    std::vector<std::uint8_t> bytes;
    const bool readable = inMainModule
        && readBytes(process, address, entry.byteCount, bytes);
    fnvxr::engine::Sha256Digest actual {};
    const bool hashed = readable
        && calculateSha256(bytes.data(), bytes.size(), actual);
    const bool matches = executable
        && hashed
        && fnvxr::engine::digestMatches(
            entry.sha256,
            actual.bytes.data(),
            actual.bytes.size());

    std::cout << "abi_range=" << entry.name
              << " preferred=0x" << std::hex << std::uppercase
              << entry.preferredAddress
              << " runtime=0x" << address << std::dec
              << " bytes=" << entry.byteCount
              << " in_main_module=" << (inMainModule ? 1 : 0)
              << " executable=" << (executable ? 1 : 0)
              << " readable=" << (readable ? 1 : 0)
              << " sha256=" << (hashed
                      ? hexBytes(actual.bytes.data(), actual.bytes.size())
                      : "UNAVAILABLE")
              << " proof=" << (matches ? "MATCH" : "MISMATCH") << '\n';
    return matches;
}

bool printVtableSlotProof(
    HANDLE process,
    std::uintptr_t moduleBase,
    std::size_t moduleBytes,
    const fnvxr::engine::abi::RetailVtableSlotDescriptor& slot)
{
    std::uintptr_t preferredSlotAddress = 0;
    std::uintptr_t runtimeSlotAddress = 0;
    const bool slotAddressAvailable = fnvxr::probe::abi::checkedAddAddress(
                                          slot.vtableAddress,
                                          slot.slotByteOffset,
                                          preferredSlotAddress)
        && fnvxr::probe::abi::relocateFromFunction(
            moduleBase,
            fnvxr::engine::SupportedImageBase,
            preferredSlotAddress,
            runtimeSlotAddress);
    std::uint32_t loadedTarget = 0;
    const bool readable = slotAddressAvailable
        && readValue(process, runtimeSlotAddress, loadedTarget);
    const fnvxr::probe::abi::VtableSlotObservation observation =
        fnvxr::probe::abi::observeVtableSlot(
            moduleBase,
            moduleBytes,
            slot,
            readable,
            loadedTarget);
    const bool targetExecutable = observation.targetInMainModule
        && isCommittedExecutableRange(
            process,
            observation.actualTargetAddress,
            1u);
    const bool matches = observation.complete() && targetExecutable;

    std::cout << "abi_vtable_slot=" << slot.owner
              << " vtable_preferred=0x" << std::hex << std::uppercase
              << slot.vtableAddress
              << " slot_offset=0x" << slot.slotByteOffset
              << " slot_runtime=0x" << observation.slotAddress
              << " expected_target=0x" << observation.expectedTargetAddress
              << " actual_target=0x" << observation.actualTargetAddress << std::dec
              << " readable=" << (readable ? 1 : 0)
              << " slot_main_module=" << (observation.slotInMainModule ? 1 : 0)
              << " target_main_module=" << (observation.targetInMainModule ? 1 : 0)
              << " target_executable=" << (targetExecutable ? 1 : 0)
              << " proof=" << (matches ? "MATCH" : "MISMATCH") << '\n';
    return matches;
}

bool printVtableBlockProof(
    HANDLE process,
    std::uintptr_t moduleBase,
    std::size_t moduleBytes,
    const fnvxr::engine::abi::RetailVtableBlockDescriptor& block)
{
    std::uintptr_t runtimeBlockAddress = 0;
    const bool addressAvailable = fnvxr::probe::abi::relocateFromFunction(
        moduleBase,
        fnvxr::engine::SupportedImageBase,
        block.preferredAddress,
        runtimeBlockAddress);
    const bool inMainModule = addressAvailable
        && fnvxr::probe::abi::rangeContained(
            moduleBase,
            moduleBytes,
            runtimeBlockAddress,
            block.byteCount);
    std::vector<std::uint8_t> bytes;
    const bool readable = inMainModule
        && readBytes(process, runtimeBlockAddress, block.byteCount, bytes);
    fnvxr::engine::Sha256Digest actual {};
    const bool hashed = readable
        && calculateSha256(bytes.data(), bytes.size(), actual);
    const fnvxr::probe::abi::VtableBlockObservation observation =
        fnvxr::probe::abi::observeVtableBlock(
            moduleBase,
            moduleBytes,
            block,
            readable,
            actual);
    const bool matches = hashed && observation.complete();

    std::cout << "abi_vtable_block=" << block.owner
              << " preferred=0x" << std::hex << std::uppercase
              << block.preferredAddress
              << " runtime=0x" << runtimeBlockAddress << std::dec
              << " bytes=" << block.byteCount
              << " in_main_module=" << (inMainModule ? 1 : 0)
              << " readable=" << (readable ? 1 : 0)
              << " sha256=" << (hashed
                      ? hexBytes(actual.bytes.data(), actual.bytes.size())
                      : "UNAVAILABLE")
              << " proof=" << (matches ? "MATCH" : "MISMATCH") << '\n';
    return matches;
}

const fnvxr::engine::LoadedFunctionManifestEntry* findManifestEntry(
    std::uintptr_t preferredAddress)
{
    const auto found = std::find_if(
        fnvxr::engine::RetailEngineManifest.begin(),
        fnvxr::engine::RetailEngineManifest.end(),
        [preferredAddress](const fnvxr::engine::LoadedFunctionManifestEntry& entry) {
            return entry.preferredAddress == preferredAddress;
        });
    return found == fnvxr::engine::RetailEngineManifest.end()
        ? nullptr
        : &*found;
}

bool printRenderWorldAbiMap(
    HANDLE process,
    std::uintptr_t moduleBase,
    std::size_t moduleBytes)
{
    using namespace fnvxr::probe::abi;

    const fnvxr::engine::LoadedFunctionManifestEntry* manifest =
        findManifestEntry(RenderWorldPreferredAddress);
    if (!manifest || manifest->byteCount != RenderWorldFunctionBytes)
    {
        std::cout << "render_world_abi_map_source manifest=ABSENT proof=MISMATCH\n"
                  << "render_world_abi_map_proof=FAIL\n";
        return false;
    }

    const std::uintptr_t functionAddress = relocate(
        moduleBase,
        RenderWorldPreferredAddress);
    const bool inMainModule = rangeContained(
        moduleBase,
        moduleBytes,
        functionAddress,
        RenderWorldFunctionBytes);
    const bool executable = inMainModule
        && isCommittedExecutableRange(
            process,
            functionAddress,
            RenderWorldFunctionBytes);
    std::vector<std::uint8_t> bytes;
    const bool readable = inMainModule
        && readBytes(process, functionAddress, RenderWorldFunctionBytes, bytes);
    fnvxr::engine::Sha256Digest actual {};
    const bool hashed = readable
        && calculateSha256(bytes.data(), bytes.size(), actual);
    const bool manifestMatches = executable
        && hashed
        && fnvxr::engine::digestMatches(
            manifest->sha256,
            actual.bytes.data(),
            actual.bytes.size());

    std::cout << "render_world_abi_map_source preferred=0x"
              << std::hex << std::uppercase << RenderWorldPreferredAddress
              << " runtime=0x" << functionAddress << std::dec
              << " bytes=" << RenderWorldFunctionBytes
              << " in_main_module=" << (inMainModule ? 1 : 0)
              << " executable=" << (executable ? 1 : 0)
              << " readable=" << (readable ? 1 : 0)
              << " sha256=" << (hashed
                      ? hexBytes(actual.bytes.data(), actual.bytes.size())
                      : "UNAVAILABLE")
              << " proof=" << (manifestMatches ? "MATCH" : "MISMATCH") << '\n';

    bool callsMatch = manifestMatches;
    for (const RenderWorldDirectCall& contract : RenderWorldDirectCallContract)
    {
        const DirectCallObservation observation = observeDirectCall(
            bytes.data(),
            bytes.size(),
            functionAddress,
            RenderWorldPreferredAddress,
            contract);
        std::uintptr_t expectedTarget = 0;
        const bool expectedRelocated = relocateFromFunction(
            functionAddress,
            RenderWorldPreferredAddress,
            contract.preferredTargetAddress,
            expectedTarget);
        const bool targetInMainModule = observation.targetDecoded
            && rangeContained(
                moduleBase,
                moduleBytes,
                observation.targetAddress,
                1u);
        const bool callMatches = manifestMatches
            && expectedRelocated
            && targetInMainModule
            && observation.complete();
        callsMatch = callMatches && callsMatch;

        const ByteWindow context = contextWindow(
            bytes.size(),
            contract.functionOffset,
            5u,
            16u,
            16u);
        std::cout << "render_world_callsite=" << contract.name
                  << " branch_context=" << contextName(contract.context)
                  << " function_offset=0x" << std::hex << std::uppercase
                  << contract.functionOffset
                  << " preferred=0x"
                  << RenderWorldPreferredAddress + contract.functionOffset
                  << " runtime=0x" << functionAddress + contract.functionOffset
                  << " expected_target=0x" << expectedTarget
                  << " actual_target=0x" << observation.targetAddress
                  << " context_offset=0x" << context.offset << std::dec
                  << " opcode_e8=" << (observation.opcodeMatches ? 1 : 0)
                  << " target_decoded=" << (observation.targetDecoded ? 1 : 0)
                  << " target_main_module=" << (targetInMainModule ? 1 : 0)
                  << " context_bytes=" << (context.byteCount != 0u
                          ? hexBytes(
                                bytes.data() + context.offset,
                                context.byteCount)
                          : "UNAVAILABLE")
                  << " proof=" << (callMatches ? "MATCH" : "MISMATCH") << '\n';
    }

    bool branchesMatch = manifestMatches;
    for (const RenderWorldBranch& contract : RenderWorldBranchContract)
    {
        const std::size_t instructionBytes = contract.encoding
                == BranchEncoding::NearConditionalRel32
            ? 6u
            : 2u;
        const BranchObservation observation = observeBranch(
            bytes.data(),
            bytes.size(),
            functionAddress,
            contract);
        const bool branchMatches = manifestMatches && observation.complete();
        branchesMatch = branchMatches && branchesMatch;
        const ByteWindow context = contextWindow(
            bytes.size(),
            contract.functionOffset,
            instructionBytes,
            16u,
            16u);

        std::cout << "render_world_branch=" << contract.name
                  << " function_offset=0x" << std::hex << std::uppercase
                  << contract.functionOffset
                  << " preferred=0x"
                  << RenderWorldPreferredAddress + contract.functionOffset
                  << " runtime=0x" << functionAddress + contract.functionOffset
                  << " expected_target=0x"
                  << functionAddress + contract.targetFunctionOffset
                  << " actual_target=0x" << observation.targetAddress
                  << " context_offset=0x" << context.offset << std::dec
                  << " opcode_match=" << (observation.opcodeMatches ? 1 : 0)
                  << " target_decoded=" << (observation.targetDecoded ? 1 : 0)
                  << " context_bytes=" << (context.byteCount != 0u
                          ? hexBytes(
                                bytes.data() + context.offset,
                                context.byteCount)
                          : "UNAVAILABLE")
                  << " proof=" << (branchMatches ? "MATCH" : "MISMATCH") << '\n';
    }

    const bool complete = manifestMatches && callsMatch && branchesMatch;
    std::cout << "render_world_abi_map_proof="
              << (complete ? "PASS" : "FAIL") << '\n';
    return complete;
}

bool printJip5730NormalizationProof(
    HANDLE process,
    const ProcessModule& mainModule,
    const ProcessModule& jipModule)
{
    using namespace fnvxr::probe::jip;

    std::uint64_t fileBytes = 0;
    fnvxr::engine::Sha256Digest fileSha256 {};
    const bool fileHashed = hashFileSha256(
        jipModule.path,
        fileBytes,
        fileSha256);

    LoadedModulePeIdentity loadedPe {};
    const bool loadedPeReadable = readLoadedModulePeIdentity(
        process,
        jipModule.base,
        jipModule.size,
        loadedPe);

    const std::uintptr_t functionAddress = relocate(
        mainModule.base,
        Jip5730RenderFirstPersonPreferredAddress);
    const bool functionInMainModule = fnvxr::probe::abi::rangeContained(
        mainModule.base,
        mainModule.size,
        functionAddress,
        Jip5730RenderFirstPersonBytes);
    const bool functionExecutable = functionInMainModule
        && isCommittedExecutableRange(
            process,
            functionAddress,
            Jip5730RenderFirstPersonBytes);
    std::vector<std::uint8_t> functionBytes;
    const bool functionReadable = functionExecutable && readBytes(
        process,
        functionAddress,
        Jip5730RenderFirstPersonBytes,
        functionBytes);

    std::uintptr_t stubAddress = 0;
    const bool stubAddressAvailable = fnvxr::probe::abi::checkedAddAddress(
        jipModule.base,
        Jip5730RenderFirstPersonStubRva,
        stubAddress);
    const bool stubInJipModule = stubAddressAvailable
        && fnvxr::probe::abi::rangeContained(
            jipModule.base,
            jipModule.size,
            stubAddress,
            Jip5730StubBytes);
    const bool stubExecutable = stubInJipModule
        && isCommittedExecutableRange(process, stubAddress, Jip5730StubBytes);
    std::vector<std::uint8_t> stubBytes;
    const bool stubReadable = stubExecutable
        && readBytes(
            process,
            stubAddress,
            Jip5730StubBytes,
            stubBytes);

    Jip5730NormalizationInput input {};
    input.fileHashAvailable = fileHashed;
    input.fileSha256 = fileSha256.bytes;
    input.fileBytes = fileBytes;
    input.loadedPeTimeDateStamp = loadedPe.timeDateStamp;
    input.loadedPeSizeOfImage = loadedPe.sizeOfImage;
    input.loadedPePreferredImageBase = loadedPe.preferredImageBase;
    input.runtimeModuleBase = jipModule.base;
    input.runtimeModuleBytes = jipModule.size;
    input.runtimeMainModuleBase = mainModule.base;
    input.runtimeMainModuleBytes = mainModule.size;
    input.runtimeFunctionAddress = functionAddress;
    input.functionBytes = functionReadable ? functionBytes.data() : nullptr;
    input.functionByteCount = functionReadable ? functionBytes.size() : 0u;
    input.stubRuntimeAddress = stubAddress;
    input.stubBytes = stubReadable ? stubBytes.data() : nullptr;
    input.stubByteCount = stubReadable ? stubBytes.size() : 0u;

    std::vector<std::uint8_t> normalizedFunction;
    const Jip5730NormalizationObservation observation =
        normalizeJip5730RenderFirstPerson(input, normalizedFunction);
    fnvxr::engine::Sha256Digest normalizedSha256 {};
    bool normalizedHashed = false;
    const bool normalizedDigestMatched = normalizationAccepted(
        observation,
        normalizedFunction,
        [&](const std::uint8_t* bytes,
            std::size_t byteCount,
            std::array<std::uint8_t, 32>& digest) {
            normalizedHashed = calculateSha256(
                bytes,
                byteCount,
                normalizedSha256);
            if (normalizedHashed)
                digest = normalizedSha256.bytes;
            return normalizedHashed;
        });
    std::vector<std::uint8_t> functionBytesAfter;
    std::vector<std::uint8_t> stubBytesAfter;
    const bool stableBeforeAfter = functionReadable
        && stubReadable
        && readBytes(
            process,
            functionAddress,
            Jip5730RenderFirstPersonBytes,
            functionBytesAfter)
        && readBytes(
            process,
            stubAddress,
            Jip5730StubBytes,
            stubBytesAfter)
        && functionBytesAfter == functionBytes
        && stubBytesAfter == stubBytes;
    const bool accepted = loadedPeReadable
        && functionExecutable
        && stubExecutable
        && stableBeforeAfter
        && normalizedDigestMatched;

    std::cout << "jip_5730_file bytes=" << fileBytes
              << " readable=" << (fileHashed ? 1 : 0)
              << " sha256=" << (fileHashed
                      ? hexBytes(fileSha256.bytes.data(), fileSha256.bytes.size())
                      : "UNAVAILABLE")
              << " hash_match=" << (observation.fileHashMatches ? 1 : 0)
              << " size_match=" << (observation.fileBytesMatch ? 1 : 0)
              << " proof="
              << (observation.fileHashMatches && observation.fileBytesMatch
                      ? "MATCH"
                      : "MISMATCH")
              << '\n';
    std::cout << "jip_5730_loaded_pe timestamp=0x" << std::hex << std::uppercase
              << loadedPe.timeDateStamp
              << " preferred_base=0x" << loadedPe.preferredImageBase
              << " image_size=0x" << loadedPe.sizeOfImage
              << " runtime_base=0x" << jipModule.base
              << " runtime_size=0x" << jipModule.size << std::dec
              << " readable=" << (loadedPeReadable ? 1 : 0)
              << " identity_match=" << (observation.loadedPeMatches ? 1 : 0)
              << " bounds_match=" << (observation.moduleBoundsMatch ? 1 : 0)
              << " proof="
              << (loadedPeReadable
                         && observation.loadedPeMatches
                         && observation.moduleBoundsMatch
                      ? "MATCH"
                      : "MISMATCH")
              << '\n';
    std::cout << "jip_5730_patch function_runtime=0x" << std::hex
              << std::uppercase << functionAddress
              << " call_runtime=0x"
              << functionAddress + Jip5730RenderFirstPersonCallOffset
              << " expected_target=0x" << stubAddress
              << " actual_target=0x" << observation.actualCallTarget << std::dec
              << " function_executable=" << (functionExecutable ? 1 : 0)
              << " function_readable=" << (functionReadable ? 1 : 0)
              << " opcode_e8=" << (observation.callOpcodeMatches ? 1 : 0)
              << " target_match=" << (observation.callTargetMatches ? 1 : 0)
              << " stub_executable=" << (stubExecutable ? 1 : 0)
              << " stub_readable=" << (stubReadable ? 1 : 0)
              << " stub_bytes=" << (stubReadable
                      ? hexBytes(stubBytes.data(), stubBytes.size())
                      : "UNAVAILABLE")
              << " stub_match=" << (observation.stubBytesMatch ? 1 : 0)
              << " proof="
              << (observation.callOpcodeMatches
                         && observation.callTargetMatches
                         && observation.stubAddressMatches
                         && observation.stubBytesMatch
                      ? "MATCH"
                      : "MISMATCH")
              << '\n';
    std::cout << "jip_5730_normalization normalized_sha256="
              << (normalizedHashed
                      ? hexBytes(
                            normalizedSha256.bytes.data(),
                            normalizedSha256.bytes.size())
                      : "UNAVAILABLE")
              << " private_copy=" << (observation.normalizedCopyCreated ? 1 : 0)
              << " stable_before_after=" << (stableBeforeAfter ? 1 : 0)
              << " loaded_executable_sections_matched=0"
              << " target_process_writes=0"
              << " production_mutation_authorized=0"
              << " observation=" << (accepted ? "MATCH" : "MISMATCH")
              << " compatibility_authorized=0\n";
    return accepted;
}

void printUsage()
{
    std::cout
        << "Usage: fnvxr_retail_runtime_probe [--pid <process id>] [--wait-ms <0..60000>]\n"
        << "Read-only proof of the loaded engine manifest, ABI map, and live world objects.\n"
        << "It never patches, suspends, resumes, or terminates the target process.\n";
}

bool parsePid(const char* text, DWORD& result)
{
    if (!text || !*text)
        return false;
    char* end = nullptr;
    const unsigned long value = std::strtoul(text, &end, 10);
    if (!end || *end != '\0' || value == 0 || value > MAXDWORD)
        return false;
    result = static_cast<DWORD>(value);
    return true;
}

bool parseWaitMilliseconds(const char* text, DWORD& result)
{
    if (!text || !*text)
        return false;
    char* end = nullptr;
    const unsigned long value = std::strtoul(text, &end, 10);
    if (!end || *end != '\0' || value > 60000ul)
        return false;
    result = static_cast<DWORD>(value);
    return true;
}

struct LiveSceneGraphPointerObservation
{
    bool singletonReadable = false;
    std::uint32_t sceneGraph = 0;
    bool cameraReadable = false;
    std::uint32_t camera = 0;
    bool cullerReadable = false;
    std::uint32_t culler = 0;

    bool nonNullPointersObserved() const noexcept
    {
        return singletonReadable
            && sceneGraph != 0
            && cameraReadable
            && camera != 0
            && cullerReadable
            && culler != 0;
    }
};

LiveSceneGraphPointerObservation readLiveSceneGraph(
    HANDLE process,
    std::uintptr_t singletonAddress)
{
    LiveSceneGraphPointerObservation observation {};
    observation.singletonReadable = readValue(
        process,
        singletonAddress,
        observation.sceneGraph);
    observation.cameraReadable = observation.sceneGraph != 0
        && readValue(
            process,
            static_cast<std::uintptr_t>(observation.sceneGraph) + SceneGraphCameraOffset,
            observation.camera);
    observation.cullerReadable = observation.sceneGraph != 0
        && readValue(
            process,
            static_cast<std::uintptr_t>(observation.sceneGraph) + SceneGraphCullerOffset,
            observation.culler);
    return observation;
}
}

int main(int argc, char** argv)
{
    DWORD processId = 0;
    DWORD waitMilliseconds = 0;
    for (int index = 1; index < argc; ++index)
    {
        const std::string argument = argv[index];
        if (argument == "--help" || argument == "-h")
        {
            printUsage();
            return EXIT_SUCCESS;
        }
        if (argument == "--pid" && index + 1 < argc)
        {
            if (!parsePid(argv[++index], processId))
            {
                std::cerr << "invalid --pid value\n";
                return EXIT_FAILURE;
            }
            continue;
        }
        if (argument == "--wait-ms" && index + 1 < argc)
        {
            if (!parseWaitMilliseconds(argv[++index], waitMilliseconds))
            {
                std::cerr << "invalid --wait-ms value\n";
                return EXIT_FAILURE;
            }
            continue;
        }
        std::cerr << "unknown or incomplete argument: " << argument << '\n';
        printUsage();
        return EXIT_FAILURE;
    }

    if (processId == 0)
    {
        const std::vector<DWORD> processes = findFalloutProcesses();
        if (processes.empty())
        {
            std::cerr << "no running FalloutNV.exe process found\n";
            return EXIT_FAILURE;
        }
        if (processes.size() != 1)
        {
            std::cerr << "multiple FalloutNV.exe processes found; pass --pid explicitly\n";
            return EXIT_FAILURE;
        }
        processId = processes.front();
    }

    ProcessModule module;
    const fnvxr::probe::ModuleLookupResult mainModuleLookup = findModule(
        processId,
        L"falloutnv.exe",
        module);
    if (mainModuleLookup != fnvxr::probe::ModuleLookupResult::Found)
    {
        std::cerr << "could not resolve FalloutNV.exe module for pid " << processId
                  << " module_lookup="
                  << (mainModuleLookup == fnvxr::probe::ModuleLookupResult::NotFound
                          ? "NOT_FOUND"
                          : "ENUMERATION_FAILED")
                  << '\n';
        return EXIT_FAILURE;
    }

    const HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (!process)
    {
        std::cerr << "OpenProcess failed error=" << GetLastError() << '\n';
        return EXIT_FAILURE;
    }

    std::wcout << L"pid=" << module.processId
               << L" path=" << module.path
               << L" module_base=0x" << std::hex << std::uppercase << module.base
               << L" module_size=0x" << module.size << std::dec << L'\n';

    const std::uintptr_t singletonAddress = relocate(
        module.base,
        PreferredWorldSceneGraphPointer);
    const ULONGLONG deadline = GetTickCount64() + waitMilliseconds;
    LiveSceneGraphPointerObservation scene {};
    do
    {
        scene = readLiveSceneGraph(process, singletonAddress);
        if (scene.nonNullPointersObserved() || GetTickCount64() >= deadline)
            break;
        Sleep(50);
    } while (true);

    std::cout << "scene_graph_singleton=0x" << std::hex << std::uppercase << singletonAddress
              << " readable=" << std::dec << (scene.singletonReadable ? 1 : 0)
              << " scene_graph=0x" << std::hex << scene.sceneGraph
              << " camera_readable=" << std::dec << (scene.cameraReadable ? 1 : 0)
              << " camera=0x" << std::hex << scene.camera
              << " culler_readable=" << std::dec << (scene.cullerReadable ? 1 : 0)
              << " culler=0x" << std::hex << scene.culler << std::dec
              << " non_null_pointer_observation="
              << (scene.nonNullPointersObserved() ? "MATCH" : "MISMATCH") << '\n';

    // Hash only after the initialized world boundary is observable. Hashing
    // first and waiting afterward allowed NVSE plugins to patch code between
    // the proof and the final verdict.
    if (scene.nonNullPointersObserved())
        Sleep(250);

    fnvxr::engine::LoadedExecutableIdentity identity {};
    const bool identityReadable = readLoadedExecutableIdentity(
        process,
        module.base,
        module.size,
        identity);
    const bool identityMatches = identityReadable
        && fnvxr::engine::matchesLoadedExecutableIdentity(identity)
        && module.size == fnvxr::engine::SupportedSizeOfImage;
    std::cout << "loaded_pe timestamp=0x" << std::hex << std::uppercase
              << identity.timeDateStamp
              << " checksum=0x" << identity.checksum
              << " image_size=0x" << identity.sizeOfImage << std::dec
              << " readable=" << (identityReadable ? 1 : 0)
              << " proof=" << (identityMatches ? "MATCH" : "MISMATCH") << '\n';

    bool unnormalizedManifestEntriesMatch = true;
    bool rawFirstPersonManifestMatches = false;
    bool firstPersonManifestPresent = false;
    for (const fnvxr::engine::LoadedFunctionManifestEntry& entry
         : fnvxr::engine::RetailEngineManifest)
    {
        const bool entryMatches = printManifestProof(
            process,
            module.base,
            module.size,
            entry);
        if (entry.preferredAddress == fnvxr::probe::jip::
                Jip5730RenderFirstPersonPreferredAddress)
        {
            firstPersonManifestPresent = true;
            rawFirstPersonManifestMatches = entryMatches;
        }
        else
        {
            unnormalizedManifestEntriesMatch = entryMatches
                && unnormalizedManifestEntriesMatch;
        }
    }

    bool abiEvidenceMatches = true;
    for (const fnvxr::engine::abi::RetailFunctionAbiDescriptor& entry
         : fnvxr::probe::abi::RetailAbiEvidenceInventory)
    {
        abiEvidenceMatches = printAbiEvidenceProof(
                                 process,
                                 module.base,
                                 module.size,
                                 entry)
            && abiEvidenceMatches;
    }
    bool vtableSlotsMatch = true;
    for (const fnvxr::engine::abi::RetailVtableSlotDescriptor& slot
         : fnvxr::engine::abi::RetailVtableSlots)
    {
        vtableSlotsMatch = printVtableSlotProof(
                               process,
                               module.base,
                               module.size,
                               slot)
            && vtableSlotsMatch;
    }
    bool vtableBlocksMatch = true;
    for (const fnvxr::engine::abi::RetailVtableBlockDescriptor& block
         : fnvxr::probe::abi::RetailVtableBlockEvidenceInventory)
    {
        vtableBlocksMatch = printVtableBlockProof(
                                process,
                                module.base,
                                module.size,
                                block)
            && vtableBlocksMatch;
    }
    std::cout << "retail_abi_range_hash_observation="
              << (abiEvidenceMatches ? "PASS" : "FAIL")
              << " entries=" << fnvxr::probe::abi::RetailAbiEvidenceInventory.size()
              << " static_production_proven="
              << (fnvxr::engine::abi::retailFunctionInventoryProductionProven()
                      ? 1
                      : 0)
              << " authorization=0\n";
    std::cout << "retail_vtable_slots_proof="
              << (vtableSlotsMatch ? "PASS" : "FAIL")
              << " entries=" << fnvxr::engine::abi::RetailVtableSlots.size()
              << '\n';
    std::cout << "retail_vtable_block_hash_observation="
              << (vtableBlocksMatch ? "PASS" : "FAIL")
              << " entries="
              << fnvxr::probe::abi::RetailVtableBlockEvidenceInventory.size()
              << " static_production_proven="
              << (fnvxr::engine::abi::retailVtableBlockInventoryProductionProven()
                      ? 1
                      : 0)
              << " authorization=0\n";
    const bool renderWorldAbiMapMatches = printRenderWorldAbiMap(
        process,
        module.base,
        module.size);

    ProcessModule jipModule {};
    const fnvxr::probe::ModuleLookupResult jipLookup = findModule(
        processId,
        L"jip_nvse.dll",
        jipModule);
    bool jipNormalizationMatched = false;
    bool jipCompatibilityComplete = false;
    if (jipLookup == fnvxr::probe::ModuleLookupResult::Found)
    {
        std::wcout << L"compatibility_module=jip_nvse.dll loaded=1 base=0x"
                   << std::hex << std::uppercase << jipModule.base
                   << L" size=0x" << jipModule.size << std::dec
                   << L" path=" << jipModule.path
                   << L" proof=OBSERVED\n";
        jipNormalizationMatched = printJip5730NormalizationProof(
            process,
            module,
            jipModule);
        // One stable normalized call-site observation is not a complete
        // loaded-image or compatibility-module inventory proof.
        jipCompatibilityComplete = false;
        std::cout << "jip_5730_render_first_person_normalization_observation="
                  << (jipNormalizationMatched ? "MATCH" : "MISMATCH")
                  << " compatibility_authorized=0\n";
    }
    else if (jipLookup == fnvxr::probe::ModuleLookupResult::NotFound)
    {
        std::cout << "compatibility_module=jip_nvse.dll loaded=0 proof=NOT_REQUIRED\n";
        jipCompatibilityComplete = false;
        std::cout << "jip_5730_module_absence_observation=NOT_FOUND"
                  << " complete_census=0 compatibility_authorized=0\n";
    }
    else
    {
        std::cout << "compatibility_module=jip_nvse.dll loaded=UNKNOWN proof=MISMATCH\n";
        std::cout << "jip_5730_module_lookup_observation=FAILED"
                  << " compatibility_authorized=0\n";
    }

    const bool protectedManifestMatches = firstPersonManifestPresent
        && unnormalizedManifestEntriesMatch
        && (rawFirstPersonManifestMatches || jipNormalizationMatched);
    std::cout << "retail_engine_manifest_normalized raw_first_person="
              << (rawFirstPersonManifestMatches ? 1 : 0)
              << " jip_normalized=" << (jipNormalizationMatched ? 1 : 0)
              << " other_entries=" << (unnormalizedManifestEntriesMatch ? 1 : 0)
              << " proof=" << (protectedManifestMatches ? "PASS" : "FAIL")
              << '\n';

    ProcessModule showOffModule {};
    const fnvxr::probe::ModuleLookupResult showOffLookup = findModule(
        processId,
        L"showoffnvse.dll",
        showOffModule);
    bool showOffCompatibilityComplete = false;
    if (showOffLookup == fnvxr::probe::ModuleLookupResult::Found)
    {
        LoadedModulePeIdentity showOffPe {};
        const bool showOffPeReadable = readLoadedModulePeIdentity(
            process,
            showOffModule.base,
            showOffModule.size,
            showOffPe);
        std::uint64_t showOffFileBytes = 0;
        fnvxr::engine::Sha256Digest showOffFileSha256 {};
        const bool showOffFileHashed = hashFileSha256(
            showOffModule.path,
            showOffFileBytes,
            showOffFileSha256);

        const bool sceneObjectsOutsideShowOff = scene.nonNullPointersObserved()
            && !addressInModule(showOffModule, scene.sceneGraph)
            && !addressInModule(showOffModule, scene.camera)
            && !addressInModule(showOffModule, scene.culler);
        const bool protectedModulesDisjoint = modulesAreDisjoint(module, showOffModule)
            && (jipLookup != fnvxr::probe::ModuleLookupResult::Found
                || modulesAreDisjoint(jipModule, showOffModule));
        // Exact range hashes and disjoint module address ranges are useful
        // observations, but they are not a complete pointer/call-target scan.
        // Likewise this probe does not yet bracket every protected read with a
        // stable before/after generation. Keep both production facts false.
        constexpr bool synchronousEvidenceCaptured = false;
        constexpr bool loadedExecutableSectionsMatched = false;
        constexpr bool noProtectedTargetIntoShowOff = false;

        fnvxr::probe::showoff::ShowOff184CompatibilityInput showOffInput {};
        showOffInput.moduleLoaded = true;
        showOffInput.fileHashAvailable = showOffFileHashed;
        showOffInput.fileSha256 = showOffFileSha256.bytes;
        showOffInput.fileBytes = showOffFileBytes;
        showOffInput.loadedPeTimeDateStamp = showOffPe.timeDateStamp;
        showOffInput.loadedPeSizeOfImage = showOffPe.sizeOfImage;
        showOffInput.loadedPePreferredImageBase = showOffPe.preferredImageBase;
        showOffInput.runtimeModuleBase = showOffModule.base;
        showOffInput.runtimeModuleBytes = showOffModule.size;
        showOffInput.synchronousEvidenceCaptured = synchronousEvidenceCaptured;
        showOffInput.synchronousLoadedExecutableSectionsMatched =
            loadedExecutableSectionsMatched;
        showOffInput.synchronousProtectedManifestVerified = protectedManifestMatches;
        showOffInput.synchronousProtectedAbiRangesVerified = abiEvidenceMatches;
        showOffInput.synchronousAllProtectedVtableSlotsVerified =
            vtableSlotsMatch && vtableBlocksMatch;
        showOffInput.jipNormalizedFirstPersonApplicable =
            jipLookup == fnvxr::probe::ModuleLookupResult::Found;
        showOffInput.synchronousJipNormalizedFirstPersonVerified =
            jipLookup == fnvxr::probe::ModuleLookupResult::Found
            && jipNormalizationMatched;
        showOffInput.synchronousSceneGraphSingletonIntegrityVerified =
            sceneObjectsOutsideShowOff;
        showOffInput.synchronousNoProtectedTargetIntoShowOffVerified =
            noProtectedTargetIntoShowOff;
        const fnvxr::probe::showoff::ShowOff184CompatibilityAssessment
            showOffAssessment = fnvxr::probe::showoff::assessShowOff184Compatibility(
                showOffInput);
        showOffCompatibilityComplete = showOffAssessment.showOff184Accepted;

        std::wcout << L"compatibility_module=ShowOffNVSE.dll loaded=1 base=0x"
                   << std::hex << std::uppercase << showOffModule.base
                   << L" size=0x" << showOffModule.size << std::dec
                   << L" path=" << showOffModule.path
                   << L" proof="
                   << (showOffCompatibilityComplete ? L"MATCH" : L"MISMATCH")
                   << L"\n";
        std::cout << "showoff_184_identity file_hash="
                  << (showOffFileHashed
                          ? hexBytes(
                                showOffFileSha256.bytes.data(),
                                showOffFileSha256.bytes.size())
                          : "UNAVAILABLE")
                  << " file_bytes=" << showOffFileBytes
                  << " pe_readable=" << (showOffPeReadable ? 1 : 0)
                  << " pe_timestamp=0x" << std::hex << std::uppercase
                  << showOffPe.timeDateStamp
                  << " pe_image_size=0x" << showOffPe.sizeOfImage
                  << " pe_preferred_base=0x" << showOffPe.preferredImageBase
                  << std::dec
                  << " proof=" << (showOffCompatibilityComplete ? "PASS" : "FAIL")
                  << " failure=" << showOffFailureName(showOffAssessment.failure)
                  << '\n';
        std::cout << "showoff_184_synchronous manifest="
                  << (protectedManifestMatches ? 1 : 0)
                  << " abi_ranges=" << (abiEvidenceMatches ? 1 : 0)
                  << " vtables=" << (vtableSlotsMatch ? 1 : 0)
                  << " vtable_blocks=" << (vtableBlocksMatch ? 1 : 0)
                  << " jip=" << (jipNormalizationMatched ? 1 : 0)
                  << " scene_outside=" << (sceneObjectsOutsideShowOff ? 1 : 0)
                  << " modules_disjoint=" << (protectedModulesDisjoint ? 1 : 0)
                  << " protected_target_scan_complete="
                  << (noProtectedTargetIntoShowOff ? 1 : 0)
                  << " stable_snapshot="
                  << (synchronousEvidenceCaptured ? 1 : 0)
                  << " loaded_executable_sections_matched="
                  << (loadedExecutableSectionsMatched ? 1 : 0)
                  << " proof=" << (showOffCompatibilityComplete ? "PASS" : "FAIL")
                  << '\n';
    }
    else if (showOffLookup == fnvxr::probe::ModuleLookupResult::NotFound)
    {
        std::cout << "compatibility_module=ShowOffNVSE.dll loaded=0"
                  << " observation=NOT_FOUND complete_census=0"
                  << " compatibility_authorized=0\n";
        showOffCompatibilityComplete = false;
    }
    else
    {
        std::cout << "compatibility_module=ShowOffNVSE.dll loaded=UNKNOWN proof=MISMATCH\n";
    }

    CloseHandle(process);
    constexpr bool compatibilityModuleInventoryProductionProofComplete =
        fnvxr::probe::jip::CompatibilityModuleInventoryProductionProofComplete
        && fnvxr::probe::showoff::CompatibilityModuleInventoryProductionProofComplete;
    const bool compatibilityComplete = jipCompatibilityComplete
        && showOffCompatibilityComplete
        && compatibilityModuleInventoryProductionProofComplete;
    std::cout << "compatibility_inventory jip_exact="
              << (jipCompatibilityComplete ? 1 : 0)
              << " showoff_exact_or_absent="
              << (showOffCompatibilityComplete ? 1 : 0)
              << " complete_module_inventory="
              << (compatibilityModuleInventoryProductionProofComplete ? 1 : 0)
              << " proof=" << (compatibilityComplete ? "PASS" : "FAIL")
              << '\n';

    fnvxr::engine::abi::RetailEngineAbiEvidence engineAbiEvidence {};
    engineAbiEvidence.loadedExecutableIdentityMatched = identityMatches;
    engineAbiEvidence.loadedExecutableSectionsMatched = false;
    engineAbiEvidence.coreManifestMatched = protectedManifestMatches;
    engineAbiEvidence.fullFunctionInventoryMatched = abiEvidenceMatches;
    engineAbiEvidence.vtableSlotsMatched = vtableSlotsMatch;
    engineAbiEvidence.vtableBlocksMatched = vtableBlocksMatch;
    // Non-null pointers and static branch bytes do not prove live layout,
    // construction ownership, both branch semantics, or a stable transaction-
    // adjacent snapshot. Dedicated probes must establish these facts.
    engineAbiEvidence.liveObjectLayoutsVerified = false;
    engineAbiEvidence.constructorOwnershipVerified = false;
    engineAbiEvidence.bothWorldBranchesVerified = false;
    engineAbiEvidence.compatibilityModulesVerified = compatibilityComplete;
    engineAbiEvidence.synchronousRuntimeRevalidation = false;
    const fnvxr::engine::abi::RetailEngineAbiAssessment engineAbiAssessment =
        fnvxr::engine::abi::assessRetailEngineAbi(engineAbiEvidence);
    std::cout << "retail_engine_abi_authorization="
              << (engineAbiAssessment.engineCallsAuthorized ? "PASS" : "FAIL")
              << " failure=" << retailEngineAbiFailureName(engineAbiAssessment.failure)
              << " static_inventory="
              << (fnvxr::engine::abi::retailFunctionInventoryProductionProven()
                      ? 1
                      : 0)
              << " loaded_executable_sections=0"
              << " static_vtable_blocks="
              << (fnvxr::engine::abi::retailVtableBlockInventoryProductionProven()
                      ? 1
                      : 0)
              << " live_layouts=0 constructor_ownership=0"
              << " both_branch_semantics=0 synchronous_revalidation=0\n";

    const bool complete = engineAbiAssessment.engineCallsAuthorized
        && renderWorldAbiMapMatches
        && scene.nonNullPointersObserved();
    std::cout << "retail_engine_capability_proof="
              << (complete ? "PASS" : "FAIL") << '\n';
    return complete ? EXIT_SUCCESS : EXIT_FAILURE;
}
