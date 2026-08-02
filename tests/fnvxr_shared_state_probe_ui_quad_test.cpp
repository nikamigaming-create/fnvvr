#include "../runtime/fnvxr_desktop_assist_ui_evidence.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace
{
class Mapping final
{
public:
    ~Mapping()
    {
        if (mView)
            UnmapViewOfFile(mView);
        if (mHandle)
            CloseHandle(mHandle);
    }

    bool create()
    {
        mHandle = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            0u,
            static_cast<DWORD>(fnvxr::engine::desktopAssistUiQuadMappingBytes()),
            fnvxr::shared::DesktopAssistUiQuadSharedMappingName);
        if (!mHandle || GetLastError() == ERROR_ALREADY_EXISTS)
            return false;
        mView = static_cast<std::uint8_t*>(MapViewOfFile(
            mHandle,
            FILE_MAP_ALL_ACCESS,
            0u,
            0u,
            fnvxr::engine::desktopAssistUiQuadMappingBytes()));
        return mView != nullptr;
    }

    std::uint8_t* view() const noexcept
    {
        return mView;
    }

private:
    HANDLE mHandle = nullptr;
    std::uint8_t* mView = nullptr;
};

bool runProbe(const char* probePath)
{
    if (!probePath || probePath[0] == '\0')
        return false;
    std::string command = "\"";
    command += probePath;
    command += "\" --require-desktop-assist-ui-quad";
    std::vector<char> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back('\0');

    STARTUPINFOA startup {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process {};
    if (!CreateProcessA(
            nullptr,
            mutableCommand.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &startup,
            &process))
    {
        return false;
    }
    const DWORD wait = WaitForSingleObject(process.hProcess, 10000u);
    DWORD exitCode = EXIT_FAILURE;
    const bool complete = wait == WAIT_OBJECT_0
        && GetExitCodeProcess(process.hProcess, &exitCode) != FALSE;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return complete && exitCode == EXIT_SUCCESS;
}

void writeCompleteUiQuad(std::uint8_t* view)
{
    std::memset(view, 0, fnvxr::engine::desktopAssistUiQuadMappingBytes());
    auto* header = reinterpret_cast<fnvxr::shared::SharedDesktopAssistUiQuadHeader*>(view);
    auto* pixels = view + sizeof(*header);
    InterlockedExchange(&header->writing, 1);
    MemoryBarrier();
    header->magic = fnvxr::shared::DesktopAssistUiQuadSharedMagic;
    header->version = fnvxr::shared::DesktopAssistUiQuadSharedVersion;
    header->headerBytes = sizeof(*header);
    header->sequence = 2;
    header->flags = fnvxr::engine::DesktopAssistUiQuadRequiredFlags;
    header->width = 2;
    header->height = 1;
    header->pitchBytes = 8;
    header->format = fnvxr::engine::DesktopAssistUiQuadPixelFormatX8R8G8B8;
    header->runtimeStateSample = 17u;
    header->poseFrame = 18u;
    header->poseSequence = 2;
    header->runtimePhase = fnvxr::shared::RuntimePhaseMenu;
    header->runtimeMenuBits = fnvxr::shared::RuntimeGenericMenuBit;
    header->captureFailure = 0u;
    header->poseProducerEpoch = 19u;
    header->captureOrdinal = 20u;
    const std::uint8_t sourcePixels[] {
        0u, 0u, 0u, 255u,
        8u, 9u, 10u, 255u,
    };
    std::memcpy(pixels, sourcePixels, sizeof(sourcePixels));
    header->pixelHash = fnvxr::engine::desktopAssistUiQuadPixelHash(
        pixels,
        sizeof(sourcePixels),
        &header->nonBlackSampleCount);
    MemoryBarrier();
    InterlockedExchange(&header->writing, 0);
}
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: fnvxr_shared_state_probe_ui_quad_test <probe-exe>\n";
        return EXIT_FAILURE;
    }

    Mapping mapping;
    if (!mapping.create())
    {
        std::cerr << "could not create isolated desktop-assist UI mapping\n";
        return EXIT_FAILURE;
    }
    writeCompleteUiQuad(mapping.view());
    if (!runProbe(argv[1]))
    {
        std::cerr << "probe rejected a complete desktop-assist UI mapping\n";
        return EXIT_FAILURE;
    }

    auto* header = reinterpret_cast<fnvxr::shared::SharedDesktopAssistUiQuadHeader*>(
        mapping.view());
    InterlockedExchange(&header->writing, 1);
    MemoryBarrier();
    header->flags &= ~fnvxr::shared::DesktopAssistUiQuadFlagPixelContentNonBlack;
    header->captureFailure = 1u;
    header->sequence = 4;
    MemoryBarrier();
    InterlockedExchange(&header->writing, 0);
    if (runProbe(argv[1]))
    {
        std::cerr << "probe accepted an invalidated desktop-assist UI mapping\n";
        return EXIT_FAILURE;
    }

    std::cout << "desktop assist UI probe mapping test passed\n";
    return EXIT_SUCCESS;
}
