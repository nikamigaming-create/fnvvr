#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include "fnvxr_shared_state.h"

#include <cstdlib>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace
{
using UInt32 = std::uint32_t;
using PluginHandle = UInt32;

struct NVSEInterface
{
    UInt32 nvseVersion;
    UInt32 runtimeVersion;
    UInt32 editorVersion;
    UInt32 isEditor;
    bool (*RegisterCommand)(void* info);
    void (*SetOpcodeBase)(UInt32 opcode);
    void* (*QueryInterface)(UInt32 id);
    PluginHandle (*GetPluginHandle)();
    bool (*RegisterTypedCommand)(void* info, UInt32 returnType);
    const char* (*GetRuntimeDirectory)();
};

struct PluginInfo
{
    UInt32 infoVersion;
    const char* name;
    UInt32 version;
};

using fnvxr::shared::SharedXInputState;
using fnvxr::shared::SharedDInputState;
using fnvxr::shared::SharedVrPoseState;
using fnvxr::shared::SharedCameraState;
using fnvxr::shared::SharedRuntimeState;
using fnvxr::shared::SharedPlayerState;
using fnvxr::shared::SharedCommandState;
using fnvxr::shared::SharedInputEventQueue;

template <typename Header>
bool validateSharedHeader(const char* mapName, UInt32 expectedMagic, UInt32 expectedVersion)
{
    HANDLE mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, mapName);
    if (!mapping)
        return false;

    Header* view = static_cast<Header*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(Header)));
    if (!view)
    {
        CloseHandle(mapping);
        return false;
    }

    Header header {};
    std::memcpy(&header, view, sizeof(Header));
    UnmapViewOfFile(view);
    CloseHandle(mapping);

    return header.magic == expectedMagic && header.version == expectedVersion;
}

bool mappingAbsent(const char* mapName)
{
    HANDLE mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, mapName);
    if (!mapping)
        return GetLastError() == ERROR_FILE_NOT_FOUND;
    CloseHandle(mapping);
    return false;
}

PluginHandle testPluginHandle()
{
    return 11;
}

int fail(const char* message)
{
    std::cerr << message << "\n";
    return 1;
}

template <typename State>
bool createHostOwnedFixture(
    const char* mapName,
    UInt32 magic,
    UInt32 version,
    HANDLE& mapping,
    State*& view,
    std::array<std::uint8_t, sizeof(State)>& before)
{
    mapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(State),
        mapName);
    if (!mapping)
        return false;
    view = static_cast<State*>(MapViewOfFile(
        mapping,
        FILE_MAP_READ | FILE_MAP_WRITE,
        0,
        0,
        sizeof(State)));
    if (!view)
        return false;

    std::memset(view, 0, sizeof(State));
    view->magic = magic;
    view->version = version;
    view->sequence = 2;
    std::memset(
        reinterpret_cast<std::uint8_t*>(view) + sizeof(State) / 2,
        0x5a,
        sizeof(State) - sizeof(State) / 2);
    std::memcpy(before.data(), view, sizeof(State));
    return true;
}

template <typename State>
bool fixtureUnchanged(const State* view, const std::array<std::uint8_t, sizeof(State)>& before)
{
    return view && std::memcmp(view, before.data(), sizeof(State)) == 0;
}
}

int main(int argc, char** argv)
{
    if (argc != 2)
        return fail("usage: fnvxr_nvse_bridge_test <plugin dll>");

    try
    {
        // XInput, DInput, and VR pose are host-owned. The plugin is a reader
        // and must neither create nor initialize them. Supply sentinel records
        // exactly as the host would and prove plugin load leaves every byte
        // unchanged.
        HANDLE xinputMapping = nullptr;
        HANDLE dinputMapping = nullptr;
        HANDLE poseMapping = nullptr;
        SharedXInputState* xinputView = nullptr;
        SharedDInputState* dinputView = nullptr;
        SharedVrPoseState* poseView = nullptr;
        std::array<std::uint8_t, sizeof(SharedXInputState)> xinputBefore {};
        std::array<std::uint8_t, sizeof(SharedDInputState)> dinputBefore {};
        std::array<std::uint8_t, sizeof(SharedVrPoseState)> poseBefore {};
        if (!createHostOwnedFixture(
                fnvxr::shared::XInputSharedMappingName,
                fnvxr::shared::XInputSharedMagic,
                fnvxr::shared::XInputSharedVersion,
                xinputMapping,
                xinputView,
                xinputBefore)
            || !createHostOwnedFixture(
                fnvxr::shared::DInputSharedMappingName,
                fnvxr::shared::DInputSharedMagic,
                fnvxr::shared::DInputSharedVersion,
                dinputMapping,
                dinputView,
                dinputBefore)
            || !createHostOwnedFixture(
                fnvxr::shared::VrPoseSharedMappingName,
                fnvxr::shared::VrPoseSharedMagic,
                fnvxr::shared::VrPoseSharedVersion,
                poseMapping,
                poseView,
                poseBefore))
        {
            return fail("host-owned fixture creation failed");
        }

        HMODULE plugin = LoadLibraryA(argv[1]);
        if (!plugin)
            return fail("LoadLibraryA failed");

        using QueryFn = bool (*)(const NVSEInterface*, PluginInfo*);
        using LoadFn = bool (*)(const NVSEInterface*);

        auto query = reinterpret_cast<QueryFn>(GetProcAddress(plugin, "NVSEPlugin_Query"));
        auto load = reinterpret_cast<LoadFn>(GetProcAddress(plugin, "NVSEPlugin_Load"));
        if (!query || !load)
            return fail("missing xNVSE exports");

        NVSEInterface nvse {};
        nvse.nvseVersion = 0x06040080;
        nvse.runtimeVersion = 0x040020d0;
        nvse.GetPluginHandle = testPluginHandle;

        PluginInfo info {};
        if (!query(&nvse, &info))
            return fail("query failed");

        if (!load(&nvse))
            return fail("load failed");

        if (!fixtureUnchanged(xinputView, xinputBefore)
            || !fixtureUnchanged(dinputView, dinputBefore)
            || !fixtureUnchanged(poseView, poseBefore))
        {
            return fail("plugin mutated a host-owned input/pose mapping");
        }

        if (!validateSharedHeader<SharedXInputState>(fnvxr::shared::XInputSharedMappingName, fnvxr::shared::XInputSharedMagic, fnvxr::shared::XInputSharedVersion))
            return fail("xinput shared map missing or invalid");
        if (!validateSharedHeader<SharedDInputState>(fnvxr::shared::DInputSharedMappingName, fnvxr::shared::DInputSharedMagic, fnvxr::shared::DInputSharedVersion))
            return fail("dinput shared map missing or invalid");
        if (!validateSharedHeader<SharedVrPoseState>(fnvxr::shared::VrPoseSharedMappingName, fnvxr::shared::VrPoseSharedMagic, fnvxr::shared::VrPoseSharedVersion))
            return fail("vr pose shared map missing or invalid");
        if (!mappingAbsent("Local\\FNVXR_Camera_State")
            || !mappingAbsent("Local\\FNVXR_Runtime_State")
            || !mappingAbsent("Local\\FNVXR_Player_State")
            || !mappingAbsent(fnvxr::shared::CommandSharedMappingName)
            || !mappingAbsent("Local\\FNVXR_Input_Events"))
        {
            return fail("source-fused plugin created a plugin-owned shared mapping");
        }

        // The plugin is process-lifetime and deliberately does no loader-lock
        // cleanup. Do not dynamically unload it; normal process teardown owns
        // all handles and mappings.
        std::cout << "nvse plugin is inert while retail mutation is source-fused\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
