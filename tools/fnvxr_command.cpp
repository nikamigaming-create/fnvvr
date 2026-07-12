#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include "fnvxr_shared_state.h"

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace
{
using fnvxr::shared::SharedCommandState;

constexpr const char* CommandMapName = "Local\\FNVXR_Command_State";

struct CommandMapping
{
    HANDLE handle = nullptr;
    SharedCommandState* state = nullptr;

    ~CommandMapping()
    {
        if (state)
            UnmapViewOfFile(state);
        if (handle)
            CloseHandle(handle);
    }
};

int fail(const char* message)
{
    std::cerr << message << "\n";
    return 1;
}

void usage()
{
    std::cout
        << "usage:\n"
        << "  fnvxr_command save [saveName] [--wait-ms N]\n"
        << "  fnvxr_command quit [--wait-ms N]\n"
        << "  fnvxr_command console <line> [--wait-ms N]\n"
        << "  fnvxr_command save-and-quit [saveName] [--wait-ms N] [--delay-ms N]\n";
}

const char* statusName(std::uint32_t status)
{
    switch (status)
    {
        case fnvxr::shared::CommandStatusIdle: return "idle";
        case fnvxr::shared::CommandStatusPending: return "pending";
        case fnvxr::shared::CommandStatusRunning: return "running";
        case fnvxr::shared::CommandStatusSucceeded: return "succeeded";
        case fnvxr::shared::CommandStatusFailed: return "failed";
        default: return "unknown";
    }
}

const char* commandName(std::uint32_t command)
{
    switch (command)
    {
        case fnvxr::shared::CommandTypeSave: return "save";
        case fnvxr::shared::CommandTypeQuit: return "quit";
        case fnvxr::shared::CommandTypeConsole: return "console";
        default: return "none";
    }
}

std::string sanitizedPayload(const std::string& input, const std::string& fallback)
{
    const std::string source = input.empty() ? fallback : input;
    std::string output;
    output.reserve(source.size());
    for (unsigned char ch : source)
    {
        if (std::isalnum(ch) || ch == '_' || ch == '-')
            output.push_back(static_cast<char>(ch));
        else if (ch == ' ' || ch == '.' || ch == ':' || ch == '/' || ch == '\\')
            output.push_back(static_cast<char>(ch));
        if (output.size() >= sizeof(SharedCommandState::saveName) - 1)
            break;
    }
    if (output.empty())
        output = fallback;
    return output;
}

std::string sanitizedSaveName(const std::string& input)
{
    std::string output = sanitizedPayload(input, "FNVXR_QuickSave");
    for (char& ch : output)
    {
        if (ch == ' ' || ch == '.' || ch == ':' || ch == '/' || ch == '\\')
            ch = '_';
    }
    return output;
}

std::string sanitizedConsoleLine(const std::string& input)
{
    return sanitizedPayload(input, "");
}

bool openCommandMapping(CommandMapping& mapping)
{
    mapping.handle = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(SharedCommandState),
        CommandMapName);
    if (!mapping.handle)
        return false;

    mapping.state = static_cast<SharedCommandState*>(
        MapViewOfFile(mapping.handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedCommandState)));
    if (!mapping.state)
        return false;

    if (mapping.state->magic != fnvxr::shared::CommandSharedMagic
        || mapping.state->version != fnvxr::shared::CommandSharedVersion)
    {
        std::memset(mapping.state, 0, sizeof(*mapping.state));
        mapping.state->magic = fnvxr::shared::CommandSharedMagic;
        mapping.state->version = fnvxr::shared::CommandSharedVersion;
        mapping.state->status = fnvxr::shared::CommandStatusIdle;
    }
    return true;
}

bool readSnapshot(const SharedCommandState* state, SharedCommandState& snapshot)
{
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        const LONG begin = state->sequence;
        MemoryBarrier();
        std::memcpy(&snapshot, state, sizeof(snapshot));
        MemoryBarrier();
        const LONG end = state->sequence;
        if (begin == end && (end % 2) == 0)
            return true;
        Sleep(0);
    }
    return false;
}

std::uint32_t nextRequestId(std::uint32_t current)
{
    ++current;
    return current == 0 ? 1 : current;
}

std::uint32_t submitCommand(
    CommandMapping& mapping,
    std::uint32_t command,
    const std::string& saveName)
{
    SharedCommandState snapshot {};
    readSnapshot(mapping.state, snapshot);

    const std::uint32_t requestId = nextRequestId(snapshot.requestId);
    InterlockedIncrement(&mapping.state->sequence);
    MemoryBarrier();
    mapping.state->magic = fnvxr::shared::CommandSharedMagic;
    mapping.state->version = fnvxr::shared::CommandSharedVersion;
    mapping.state->requestId = requestId;
    mapping.state->command = command;
    mapping.state->status = fnvxr::shared::CommandStatusPending;
    mapping.state->flags = 0;
    mapping.state->requestedFrame = 0;
    mapping.state->completedFrame = 0;
    mapping.state->resultCode = 0;
    std::memset(mapping.state->saveName, 0, sizeof(mapping.state->saveName));
    std::memset(mapping.state->lastCommand, 0, sizeof(mapping.state->lastCommand));
    if (command == fnvxr::shared::CommandTypeSave)
        strcpy_s(mapping.state->saveName, sanitizedSaveName(saveName).c_str());
    else if (command == fnvxr::shared::CommandTypeConsole)
        strcpy_s(mapping.state->saveName, sanitizedConsoleLine(saveName).c_str());
    MemoryBarrier();
    InterlockedIncrement(&mapping.state->sequence);
    FlushViewOfFile(mapping.state, sizeof(*mapping.state));

    std::cout
        << "requested command=" << commandName(command)
        << " requestId=" << requestId << "\n";
    return requestId;
}

int waitForCompletion(const CommandMapping& mapping, std::uint32_t requestId, DWORD waitMs)
{
    if (waitMs == 0)
        return 0;

    const ULONGLONG deadline = GetTickCount64() + waitMs;
    SharedCommandState snapshot {};
    do
    {
        if (readSnapshot(mapping.state, snapshot) && snapshot.requestId == requestId)
        {
            if (snapshot.status == fnvxr::shared::CommandStatusSucceeded
                || snapshot.status == fnvxr::shared::CommandStatusFailed)
            {
                std::cout
                    << "completed command=" << commandName(snapshot.command)
                    << " requestId=" << snapshot.requestId
                    << " status=" << statusName(snapshot.status)
                    << " result=" << snapshot.resultCode
                    << " lastCommand=" << snapshot.lastCommand << "\n";
                return snapshot.status == fnvxr::shared::CommandStatusSucceeded ? 0 : 3;
            }
        }
        Sleep(50);
    } while (GetTickCount64() < deadline);

    std::cerr << "timed out waiting for command requestId=" << requestId << "\n";
    return 4;
}

int sendAndMaybeWait(
    CommandMapping& mapping,
    std::uint32_t command,
    const std::string& saveName,
    DWORD waitMs)
{
    const std::uint32_t requestId = submitCommand(mapping, command, saveName);
    return waitForCompletion(mapping, requestId, waitMs);
}
}

int main(int argc, char** argv)
{
    if (argc < 2 || std::strcmp(argv[1], "--help") == 0 || std::strcmp(argv[1], "-h") == 0)
    {
        usage();
        return argc < 2 ? 1 : 0;
    }

    const std::string verb = argv[1];
    std::vector<std::string> positional;
    DWORD waitMs = 0;
    DWORD delayMs = 1500;
    for (int i = 2; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--wait-ms") == 0 && i + 1 < argc)
        {
            waitMs = static_cast<DWORD>(std::strtoul(argv[++i], nullptr, 10));
        }
        else if (std::strcmp(argv[i], "--delay-ms") == 0 && i + 1 < argc)
        {
            delayMs = static_cast<DWORD>(std::strtoul(argv[++i], nullptr, 10));
        }
        else if (!argv[i][0] || (argv[i][0] == '-' && verb != "console"))
        {
            usage();
            return 1;
        }
        else
        {
            positional.emplace_back(argv[i]);
        }
    }

    std::string payload;
    for (std::size_t i = 0; i < positional.size(); ++i)
    {
        if (i != 0)
            payload.push_back(' ');
        payload += positional[i];
    }

    CommandMapping mapping;
    if (!openCommandMapping(mapping))
        return fail("failed to open Local\\FNVXR_Command_State");

    if (verb == "save")
        return sendAndMaybeWait(mapping, fnvxr::shared::CommandTypeSave, payload, waitMs);

    if (verb == "quit")
        return sendAndMaybeWait(mapping, fnvxr::shared::CommandTypeQuit, "", waitMs);

    if (verb == "console")
    {
        if (payload.empty())
            return fail("missing console line");
        return sendAndMaybeWait(mapping, fnvxr::shared::CommandTypeConsole, payload, waitMs);
    }

    if (verb == "save-and-quit")
    {
        const DWORD saveWaitMs = waitMs == 0 ? 10000 : waitMs;
        int result = sendAndMaybeWait(mapping, fnvxr::shared::CommandTypeSave, payload, saveWaitMs);
        if (result != 0)
            return result;
        Sleep(delayMs);
        return sendAndMaybeWait(mapping, fnvxr::shared::CommandTypeQuit, "", waitMs);
    }

    usage();
    return 1;
}
