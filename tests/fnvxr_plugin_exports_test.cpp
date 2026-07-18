#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <cstdint>
#include <cstdlib>
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

PluginHandle testPluginHandle()
{
    return 7;
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
        return fail("usage: fnvxr_plugin_exports_test <plugin dll>");

    HMODULE plugin = LoadLibraryA(argv[1]);
    if (!plugin)
        return fail("LoadLibraryA failed");

    using QueryFn = bool (*)(const NVSEInterface*, PluginInfo*);
    using LoadFn = bool (*)(const NVSEInterface*);
    using RetailMutationProofFn = bool (*)();

    auto query = reinterpret_cast<QueryFn>(GetProcAddress(plugin, "NVSEPlugin_Query"));
    auto load = reinterpret_cast<LoadFn>(GetProcAddress(plugin, "NVSEPlugin_Load"));
    auto retailMutationProof = reinterpret_cast<RetailMutationProofFn>(
        GetProcAddress(plugin, "FNVXR_RetailMutationProofComplete"));

    if (!query)
        return fail("missing NVSEPlugin_Query export");

    if (!load)
        return fail("missing NVSEPlugin_Load export");

    if (!retailMutationProof)
        return fail("missing retail mutation proof export");

    if (retailMutationProof())
        return fail("retail mutation proof must remain source-blocked");

    NVSEInterface nvse {};
    nvse.nvseVersion = 0x060408;
    nvse.runtimeVersion = 0x040020d0;
    nvse.GetPluginHandle = testPluginHandle;

    PluginInfo info {};
    if (!query(&nvse, &info))
        return fail("NVSEPlugin_Query rejected test runtime");

    NVSEInterface wrongRuntime = nvse;
    wrongRuntime.runtimeVersion = 1;
    if (query(&wrongRuntime, &info))
        return fail("NVSEPlugin_Query accepted an unsupported runtime");

    NVSEInterface noGoreRuntime = nvse;
    noGoreRuntime.runtimeVersion = 0x040020d1;
    if (query(&noGoreRuntime, &info))
        return fail("NVSEPlugin_Query accepted the unproven no-gore runtime");

    NVSEInterface editorRuntime = nvse;
    editorRuntime.isEditor = 1;
    if (query(&editorRuntime, &info))
        return fail("NVSEPlugin_Query accepted the editor");

    if (!info.name || info.version == 0)
        return fail("NVSEPlugin_Query did not fill PluginInfo");

    _putenv_s("FNVXR_DISABLE_BRIDGE", "1");
    if (!load(&nvse))
        return fail("NVSEPlugin_Load rejected test runtime");
    _putenv_s("FNVXR_DISABLE_BRIDGE", "");

    FreeLibrary(plugin);
    std::cout << "plugin exports ok\n";
    return 0;
}
