#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>

#include "fnvxr_engine_capability.h"
#include "fnvxr_retail_engine_manifest.h"

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
constexpr std::uintptr_t PreferredPlayerUpdateCameraAddress = 0x0094AE40u;
constexpr std::size_t SceneGraphCameraOffset = 0xACu;
constexpr std::size_t SceneGraphCullerOffset = 0xB4u;

constexpr std::array<std::uint8_t, 5> ExpectedPlayerUpdateCamera {
    0x55, 0x8B, 0xEC, 0x6A, 0xFF,
};

struct ProcessModule
{
    DWORD processId = 0;
    std::uintptr_t base = 0;
    std::size_t size = 0;
    std::wstring path;
};

struct ProbeTarget
{
    const char* name = nullptr;
    std::uintptr_t preferredAddress = 0;
    const std::uint8_t* expected = nullptr;
    std::size_t expectedSize = 0;
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

bool findMainModule(DWORD processId, ProcessModule& result)
{
    const HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
        processId);
    if (snapshot == INVALID_HANDLE_VALUE)
        return false;

    MODULEENTRY32W entry {};
    entry.dwSize = sizeof(entry);
    bool found = false;
    if (Module32FirstW(snapshot, &entry))
    {
        do
        {
            if (lower(entry.szModule) == L"falloutnv.exe")
            {
                result.processId = processId;
                result.base = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
                result.size = static_cast<std::size_t>(entry.modBaseSize);
                result.path = entry.szExePath;
                found = true;
                break;
            }
        } while (Module32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return found;
}

std::uintptr_t relocate(std::uintptr_t moduleBase, std::uintptr_t preferredAddress)
{
    return moduleBase + (preferredAddress - fnvxr::engine::SupportedImageBase);
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
    return success;
}

bool readLoadedExecutableIdentity(
    HANDLE process,
    std::uintptr_t moduleBase,
    fnvxr::engine::LoadedExecutableIdentity& result)
{
    IMAGE_DOS_HEADER dos {};
    if (!readValue(process, moduleBase, dos)
        || dos.e_magic != IMAGE_DOS_SIGNATURE
        || dos.e_lfanew <= 0)
    {
        return false;
    }

    IMAGE_NT_HEADERS32 nt {};
    if (!readValue(
            process,
            moduleBase + static_cast<std::uintptr_t>(dos.e_lfanew),
            nt)
        || nt.Signature != IMAGE_NT_SIGNATURE
        || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
    {
        return false;
    }

    result.timeDateStamp = nt.FileHeader.TimeDateStamp;
    result.checksum = nt.OptionalHeader.CheckSum;
    result.sizeOfImage = nt.OptionalHeader.SizeOfImage;
    return true;
}

bool printManifestProof(
    HANDLE process,
    std::uintptr_t moduleBase,
    const fnvxr::engine::LoadedFunctionManifestEntry& entry)
{
    std::vector<std::uint8_t> bytes(entry.byteCount);
    SIZE_T read = 0;
    const std::uintptr_t address = relocate(moduleBase, entry.preferredAddress);
    const bool readable = ReadProcessMemory(
                              process,
                              reinterpret_cast<const void*>(address),
                              bytes.data(),
                              bytes.size(),
                              &read)
            != FALSE
        && read == bytes.size();

    fnvxr::engine::Sha256Digest actual {};
    const bool hashed = readable && calculateSha256(bytes.data(), bytes.size(), actual);
    const bool matches = hashed
        && fnvxr::engine::digestMatches(
            entry.sha256,
            actual.bytes.data(),
            actual.bytes.size());

    std::cout << "manifest=" << entry.name
              << " preferred=0x" << std::hex << std::uppercase << entry.preferredAddress
              << " runtime=0x" << address << std::dec
              << " bytes=" << entry.byteCount
              << " readable=" << (readable ? 1 : 0)
              << " sha256=" << (hashed
                      ? hexBytes(actual.bytes.data(), actual.bytes.size())
                      : "UNAVAILABLE")
              << " proof=" << (matches ? "MATCH" : "MISMATCH") << '\n';
    return matches;
}

void printTarget(HANDLE process, std::uintptr_t moduleBase, const ProbeTarget& target)
{
    constexpr std::size_t CaptureBytes = 96;
    std::array<std::uint8_t, CaptureBytes> bytes {};
    SIZE_T read = 0;
    const std::uintptr_t address = relocate(moduleBase, target.preferredAddress);
    const bool readable = ReadProcessMemory(
                              process,
                              reinterpret_cast<const void*>(address),
                              bytes.data(),
                              bytes.size(),
                              &read)
        != FALSE;
    bool signatureMatches = false;
    if (readable && target.expected && read >= target.expectedSize)
    {
        signatureMatches = std::equal(
            target.expected,
            target.expected + target.expectedSize,
            bytes.begin());
    }

    std::cout << "target=" << target.name
              << " preferred=0x" << std::hex << std::uppercase << target.preferredAddress
              << " runtime=0x" << address << std::dec
              << " readable=" << (readable ? 1 : 0)
              << " captured=" << read;
    if (target.expected)
        std::cout << " signature=" << (signatureMatches ? "MATCH" : "MISMATCH");
    else
        std::cout << " signature=UNPROVEN";
    std::cout << " bytes=" << hexBytes(bytes.data(), static_cast<std::size_t>(read)) << '\n';
}

void printUsage()
{
    std::cout
        << "Usage: fnvxr_retail_runtime_probe [--pid <FalloutNV process id>]\n"
        << "Read-only probe of the loaded FalloutNV.exe render boundaries.\n"
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
}

int main(int argc, char** argv)
{
    DWORD processId = 0;
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
    if (!findMainModule(processId, module))
    {
        std::cerr << "could not resolve FalloutNV.exe module for pid " << processId << '\n';
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

    fnvxr::engine::LoadedExecutableIdentity identity {};
    const bool identityReadable = readLoadedExecutableIdentity(process, module.base, identity);
    const bool identityMatches = identityReadable
        && fnvxr::engine::matchesLoadedExecutableIdentity(identity);
    std::cout << "loaded_pe timestamp=0x" << std::hex << std::uppercase
              << identity.timeDateStamp
              << " checksum=0x" << identity.checksum
              << " image_size=0x" << identity.sizeOfImage << std::dec
              << " readable=" << (identityReadable ? 1 : 0)
              << " proof=" << (identityMatches ? "MATCH" : "MISMATCH") << '\n';

    bool manifestMatches = true;
    for (const fnvxr::engine::LoadedFunctionManifestEntry& entry
         : fnvxr::engine::RetailEngineManifest)
    {
        manifestMatches = printManifestProof(process, module.base, entry)
            && manifestMatches;
    }

    const ProbeTarget targets[] = {
        { "RenderFirstPersonDiagnostic", fnvxr::engine::FirstPersonRenderAddress,
          nullptr, 0 },
        { "PlayerCharacterUpdateCamera", PreferredPlayerUpdateCameraAddress,
          ExpectedPlayerUpdateCamera.data(), ExpectedPlayerUpdateCamera.size() },
    };
    for (const ProbeTarget& target : targets)
        printTarget(process, module.base, target);

    std::uint32_t sceneGraph = 0;
    std::uint32_t camera = 0;
    std::uint32_t culler = 0;
    const std::uintptr_t singletonAddress = relocate(
        module.base,
        PreferredWorldSceneGraphPointer);
    const bool singletonReadable = readValue(process, singletonAddress, sceneGraph);
    const bool cameraReadable = sceneGraph != 0
        && readValue(process, static_cast<std::uintptr_t>(sceneGraph) + SceneGraphCameraOffset, camera);
    const bool cullerReadable = sceneGraph != 0
        && readValue(process, static_cast<std::uintptr_t>(sceneGraph) + SceneGraphCullerOffset, culler);
    std::cout << "scene_graph_singleton=0x" << std::hex << std::uppercase << singletonAddress
              << " readable=" << std::dec << (singletonReadable ? 1 : 0)
              << " scene_graph=0x" << std::hex << sceneGraph
              << " camera_readable=" << std::dec << (cameraReadable ? 1 : 0)
              << " camera=0x" << std::hex << camera
              << " culler_readable=" << std::dec << (cullerReadable ? 1 : 0)
              << " culler=0x" << std::hex << culler << std::dec << '\n';

    CloseHandle(process);
    return identityMatches && manifestMatches ? EXIT_SUCCESS : EXIT_FAILURE;
}
