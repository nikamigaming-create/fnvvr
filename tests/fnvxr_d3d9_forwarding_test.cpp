#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <d3d9.h>

#include <cstdlib>
#include <cstring>
#include <iostream>

namespace
{
using Direct3DCreate9Fn = IDirect3D9* (WINAPI*)(UINT);
using Direct3DCreate9ExFn = HRESULT(WINAPI*)(UINT, IDirect3D9Ex**);

struct FileSnapshot
{
    bool exists = false;
    DWORD sizeHigh = 0;
    DWORD sizeLow = 0;
    FILETIME writeTime {};
};

int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}

bool executableSiblingPath(char* path, std::size_t pathSize, const char* leaf)
{
    const DWORD length = GetModuleFileNameA(nullptr, path, static_cast<DWORD>(pathSize));
    if (length == 0 || length >= pathSize)
        return false;
    char* slash = std::strrchr(path, '\\');
    if (!slash)
        return false;
    slash[1] = '\0';
    return strcat_s(path, pathSize, leaf) == 0;
}

FileSnapshot snapshotFile(const char* path)
{
    WIN32_FILE_ATTRIBUTE_DATA data {};
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data))
        return {};
    return { true, data.nFileSizeHigh, data.nFileSizeLow, data.ftLastWriteTime };
}

bool sameSnapshot(const FileSnapshot& left, const FileSnapshot& right)
{
    return left.exists == right.exists
        && left.sizeHigh == right.sizeHigh
        && left.sizeLow == right.sizeLow
        && CompareFileTime(&left.writeTime, &right.writeTime) == 0;
}

void** interfaceVtable(void* interfacePointer)
{
    return interfacePointer ? *reinterpret_cast<void***>(interfacePointer) : nullptr;
}

HMODULE interfaceOwner(void* interfacePointer)
{
    void** vtable = interfaceVtable(interfacePointer);
    if (!vtable || !vtable[0])
        return nullptr;

    HMODULE owner = nullptr;
    const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
        | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    if (!GetModuleHandleExA(flags, reinterpret_cast<LPCSTR>(vtable[0]), &owner))
        return nullptr;
    return owner;
}
}

int main(int argc, char** argv)
{
    if (argc != 2)
        return fail("expected the built D3D9 proxy path");

    // Runtime/environment requests must not weaken the compiled false proof.
    SetEnvironmentVariableA("FNVXR_DISABLE_STEREO_WORLD", "0");
    SetEnvironmentVariableA("FNVXR_D3D9_SHARED_STEREO", "1");
    SetEnvironmentVariableA("FNVXR_D3D9_NATIVE_STEREO", "1");
    SetEnvironmentVariableA("FNVXR_D3D9_NATIVE_SINGLE_TRAVERSAL_REPLAY", "1");
    SetEnvironmentVariableA("FNVXR_D3D9_STEREO_READBACK", "1");
    SetEnvironmentVariableA("FNVXR_D3D9_TELEMETRY_HAMMER", "1");

    char logPath[MAX_PATH] {};
    if (!executableSiblingPath(logPath, sizeof(logPath), "fnvxr_d3d9_proxy.log"))
        return fail("could not resolve the proxy log path");
    const FileSnapshot logBefore = snapshotFile(logPath);

    char systemPath[MAX_PATH] {};
    const UINT systemLength = GetSystemDirectoryA(systemPath, static_cast<UINT>(sizeof(systemPath)));
    if (systemLength == 0 || systemLength >= sizeof(systemPath)
        || strcat_s(systemPath, "\\d3d9.dll") != 0)
    {
        return fail("could not resolve system d3d9.dll");
    }

    HMODULE systemD3D9 = LoadLibraryA(systemPath);
    HMODULE proxyD3D9 = LoadLibraryA(argv[1]);
    if (!systemD3D9 || !proxyD3D9)
        return fail("could not load system and proxy D3D9 modules");

    const auto proxyCreate = reinterpret_cast<Direct3DCreate9Fn>(
        GetProcAddress(proxyD3D9, "Direct3DCreate9"));
    const auto systemCreate = reinterpret_cast<Direct3DCreate9Fn>(
        GetProcAddress(systemD3D9, "Direct3DCreate9"));
    if (!proxyCreate || !systemCreate)
        return fail("Direct3DCreate9 export missing");

    IDirect3D9* throughProxy = proxyCreate(D3D_SDK_VERSION);
    IDirect3D9* directSystem = systemCreate(D3D_SDK_VERSION);
    if (!throughProxy || !directSystem)
        return fail("Direct3DCreate9 failed");
    void** proxyVtable = interfaceVtable(throughProxy);
    void** directVtable = interfaceVtable(directSystem);
    const HMODULE proxyOwner = interfaceOwner(throughProxy);
    const HMODULE directOwner = interfaceOwner(directSystem);
    const bool interfacesMatch = proxyVtable && proxyVtable == directVtable
        && proxyOwner && proxyOwner == directOwner
        && proxyOwner != proxyD3D9;
    throughProxy->Release();
    directSystem->Release();
    if (!interfacesMatch)
        return fail("fused Direct3DCreate9 returned an interposed interface");

    const auto proxyCreateEx = reinterpret_cast<Direct3DCreate9ExFn>(
        GetProcAddress(proxyD3D9, "Direct3DCreate9Ex"));
    const auto systemCreateEx = reinterpret_cast<Direct3DCreate9ExFn>(
        GetProcAddress(systemD3D9, "Direct3DCreate9Ex"));
    if (proxyCreateEx && systemCreateEx)
    {
        IDirect3D9Ex* throughProxyEx = nullptr;
        IDirect3D9Ex* directSystemEx = nullptr;
        const HRESULT proxyResult = proxyCreateEx(D3D_SDK_VERSION, &throughProxyEx);
        const HRESULT systemResult = systemCreateEx(D3D_SDK_VERSION, &directSystemEx);
        const bool resultsMatch = proxyResult == systemResult;
        const bool exInterfacesMatch = FAILED(proxyResult)
            || (interfaceVtable(throughProxyEx) == interfaceVtable(directSystemEx)
                && interfaceOwner(throughProxyEx) == interfaceOwner(directSystemEx)
                && interfaceOwner(throughProxyEx) != proxyD3D9);
        if (throughProxyEx)
            throughProxyEx->Release();
        if (directSystemEx)
            directSystemEx->Release();
        if (!resultsMatch || !exInterfacesMatch)
            return fail("fused Direct3DCreate9Ex was not a transparent system forward");
    }

    const FileSnapshot logAfter = snapshotFile(logPath);
    if (!sameSnapshot(logBefore, logAfter))
        return fail("fused proxy wrote a log while forwarding D3D9 creation");

    std::cout << "fnvxr D3D9 transparent forwarding PASS\n";
    return EXIT_SUCCESS;
}
