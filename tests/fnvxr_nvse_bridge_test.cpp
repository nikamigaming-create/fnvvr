#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include "fnvxr_shared_state.h"

#include <cstdlib>
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

PluginHandle testPluginHandle()
{
    return 11;
}

int fail(const char* message)
{
    std::cerr << message << "\n";
    return 1;
}
}

int main(int argc, char** argv)
{
    if (argc != 2)
        return fail("usage: fnvxr_nvse_bridge_test <plugin dll>");

    try
    {
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
        nvse.nvseVersion = 0x060408;
        nvse.runtimeVersion = 0x040020d0;
        nvse.GetPluginHandle = testPluginHandle;

        PluginInfo info {};
        if (!query(&nvse, &info))
            return fail("query failed");

        if (!load(&nvse))
            return fail("load failed");

        if (!validateSharedHeader<SharedXInputState>("Local\\FNVXR_XInput_State", fnvxr::shared::XInputSharedMagic, fnvxr::shared::XInputSharedVersion))
            return fail("xinput shared map missing or invalid");
        if (!validateSharedHeader<SharedDInputState>("Local\\FNVXR_DInput_State", fnvxr::shared::DInputSharedMagic, fnvxr::shared::DInputSharedVersion))
            return fail("dinput shared map missing or invalid");
        if (!validateSharedHeader<SharedVrPoseState>("Local\\FNVXR_VR_Pose_State", fnvxr::shared::VrPoseSharedMagic, fnvxr::shared::VrPoseSharedVersion))
            return fail("vr pose shared map missing or invalid");
        if (!validateSharedHeader<SharedCameraState>("Local\\FNVXR_Camera_State", fnvxr::shared::CameraSharedMagic, fnvxr::shared::CameraSharedVersion))
            return fail("camera shared map missing or invalid");
        if (!validateSharedHeader<SharedRuntimeState>("Local\\FNVXR_Runtime_State", fnvxr::shared::RuntimeSharedMagic, fnvxr::shared::RuntimeSharedVersion))
            return fail("runtime shared map missing or invalid");
        if (!validateSharedHeader<SharedPlayerState>("Local\\FNVXR_Player_State", fnvxr::shared::PlayerSharedMagic, fnvxr::shared::PlayerSharedVersion))
            return fail("player shared map missing or invalid");
        if (!validateSharedHeader<SharedCommandState>("Local\\FNVXR_Command_State", fnvxr::shared::CommandSharedMagic, fnvxr::shared::CommandSharedVersion))
            return fail("command shared map missing or invalid");
        if (!validateSharedHeader<SharedInputEventQueue>("Local\\FNVXR_Input_Events", fnvxr::shared::InputEventSharedMagic, fnvxr::shared::InputEventSharedVersion))
            return fail("input event shared map missing or invalid");

        FreeLibrary(plugin);
        std::cout << "nvse bridge shared-memory init ok\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
