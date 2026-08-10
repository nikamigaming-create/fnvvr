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
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(mapping);
        mapping = nullptr;
        std::cerr << "host-owned fixture mapping already exists: " << mapName << "\n";
        return false;
    }
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

template <typename State>
void reportFirstFixtureDifference(
    const char* name,
    const State* view,
    const std::array<std::uint8_t, sizeof(State)>& before)
{
    if (!view)
    {
        std::cerr << " " << name << "=unmapped";
        return;
    }
    const auto* after = reinterpret_cast<const std::uint8_t*>(view);
    for (std::size_t offset = 0; offset < sizeof(State); ++offset)
    {
        if (after[offset] != before[offset])
        {
            std::cerr
                << " " << name << "Offset=" << offset
                << " before=" << static_cast<unsigned>(before[offset])
                << " after=" << static_cast<unsigned>(after[offset]);
            return;
        }
    }
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
        HANDLE inputProducerLease = CreateMutexA(
            nullptr,
            TRUE,
            fnvxr::shared::InputCoreProducerMutexName);
        if (!inputProducerLease)
            return fail("host input-producer lease creation failed");
        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            const DWORD wait = WaitForSingleObject(inputProducerLease, 0);
            if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED)
                return fail("host input-producer lease is owned by another live process");
        }
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

        const bool xinputUnchanged = fixtureUnchanged(xinputView, xinputBefore);
        const bool dinputUnchanged = fixtureUnchanged(dinputView, dinputBefore);
        const bool poseUnchanged = fixtureUnchanged(poseView, poseBefore);
        if (!xinputUnchanged || !dinputUnchanged || !poseUnchanged)
        {
            std::cerr
                << "plugin mutated a host-owned input/pose mapping"
                << " xinput=" << xinputUnchanged
                << " dinput=" << dinputUnchanged
                << " pose=" << poseUnchanged;
            if (!xinputUnchanged)
                reportFirstFixtureDifference("xinput", xinputView, xinputBefore);
            if (!dinputUnchanged)
                reportFirstFixtureDifference("dinput", dinputView, dinputBefore);
            if (!poseUnchanged)
                reportFirstFixtureDifference("pose", poseView, poseBefore);
            std::cerr << "\n";
            return 1;
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
