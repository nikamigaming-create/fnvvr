#include "../protocol/fnvxr_shared_state.h"

#include <windows.h>

#include <cstdlib>
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
            static_cast<DWORD>(sizeof(fnvxr::shared::SharedDesktopAssistState)),
            fnvxr::shared::DesktopAssistSharedMappingName);
        if (!mHandle || GetLastError() == ERROR_ALREADY_EXISTS)
            return false;
        mView = static_cast<fnvxr::shared::SharedDesktopAssistState*>(MapViewOfFile(
            mHandle,
            FILE_MAP_ALL_ACCESS,
            0u,
            0u,
            sizeof(fnvxr::shared::SharedDesktopAssistState)));
        return mView != nullptr;
    }

    fnvxr::shared::SharedDesktopAssistState* view() const noexcept
    {
        return mView;
    }

private:
    HANDLE mHandle = nullptr;
    fnvxr::shared::SharedDesktopAssistState* mView = nullptr;
};

bool runProbe(const char* probePath, const char* requirement)
{
    if (!probePath || probePath[0] == '\0' || !requirement || requirement[0] == '\0')
        return false;
    std::string command = "\"";
    command += probePath;
    command += "\" ";
    command += requirement;
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

void identity(float matrix[9])
{
    std::memset(matrix, 0, sizeof(float) * 9u);
    matrix[0] = 1.0f;
    matrix[4] = 1.0f;
    matrix[8] = 1.0f;
}

void writeReadyState(fnvxr::shared::SharedDesktopAssistState* state)
{
    std::memset(state, 0, sizeof(*state));
    InterlockedExchange(&state->sequence, 1);
    MemoryBarrier();
    state->magic = fnvxr::shared::DesktopAssistSharedMagic;
    state->version = fnvxr::shared::DesktopAssistSharedVersion;
    state->flags = fnvxr::shared::DesktopAssistFlagLeaseCurrent
        | fnvxr::shared::DesktopAssistFlagCameraHookInstalled
        | fnvxr::shared::DesktopAssistFlagFirstPerson
        | fnvxr::shared::DesktopAssistFlagPlayerTransformValid
        | fnvxr::shared::DesktopAssistFlagCameraLocalTransformValid
        | fnvxr::shared::DesktopAssistFlagBodyRootTransformValid;
    state->frame = 7u;
    state->cameraNodeAddress = 0x01111111u;
    state->bodyRootAddress = 0x02222222u;
    identity(state->playerWorldRot);
    identity(state->cameraLocalRot);
    identity(state->bodyRootWorldRot);
    state->playerWorldPos[1] = 1.0f;
    state->cameraLocalPos[1] = 1.0f;
    state->bodyRootWorldPos[1] = 1.0f;
    MemoryBarrier();
    InterlockedExchange(&state->sequence, 2);
}
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: fnvxr_shared_state_probe_desktop_assist_ready_test <probe-exe>\n";
        return EXIT_FAILURE;
    }

    Mapping mapping;
    if (!mapping.create())
    {
        std::cerr << "could not create isolated desktop-assist state mapping\n";
        return EXIT_FAILURE;
    }
    writeReadyState(mapping.view());

    if (!runProbe(argv[1], "--require-desktop-assist-ready"))
    {
        std::cerr << "probe rejected a desktop-assist state ready for its first synthetic pose\n";
        return EXIT_FAILURE;
    }
    if (runProbe(argv[1], "--require-desktop-assist"))
    {
        std::cerr << "probe accepted a ready state with no applied synthetic pose\n";
        return EXIT_FAILURE;
    }

    auto* state = mapping.view();
    InterlockedExchange(&state->sequence, 3);
    MemoryBarrier();
    state->flags &= ~fnvxr::shared::DesktopAssistFlagBodyRootTransformValid;
    state->bodyRootAddress = 0u;
    MemoryBarrier();
    InterlockedExchange(&state->sequence, 4);
    if (runProbe(argv[1], "--require-desktop-assist-ready"))
    {
        std::cerr << "probe accepted a desktop-assist state with no explicit body-root evidence\n";
        return EXIT_FAILURE;
    }

    std::cout << "desktop-assist ready probe mapping test passed\n";
    return EXIT_SUCCESS;
}
