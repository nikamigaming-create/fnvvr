#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include "fnvxr_shared_state.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace
{
using fnvxr::shared::SharedInputEventQueue;

constexpr const char* InputMapName = "Local\\FNVXR_Input_Events";

struct InputMapping
{
    HANDLE handle = nullptr;
    SharedInputEventQueue* queue = nullptr;

    ~InputMapping()
    {
        if (queue)
            UnmapViewOfFile(queue);
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
        << "  fnvxr_input key <tap|down|up> <dik>\n"
        << "  fnvxr_input mouse <tap|down|up> <button>\n"
        << "  fnvxr_input move <dx> <dy>\n"
        << "  fnvxr_input wheel <delta>\n";
}

bool openInputMapping(InputMapping& mapping)
{
    mapping.handle = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(SharedInputEventQueue),
        InputMapName);
    if (!mapping.handle)
        return false;

    mapping.queue = static_cast<SharedInputEventQueue*>(
        MapViewOfFile(mapping.handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedInputEventQueue)));
    if (!mapping.queue)
        return false;

    if (mapping.queue->magic != fnvxr::shared::InputEventSharedMagic
        || mapping.queue->version != fnvxr::shared::InputEventSharedVersion)
    {
        std::memset(mapping.queue, 0, sizeof(*mapping.queue));
        mapping.queue->magic = fnvxr::shared::InputEventSharedMagic;
        mapping.queue->version = fnvxr::shared::InputEventSharedVersion;
    }
    return true;
}

bool publishInputEvent(
    SharedInputEventQueue* queue,
    std::uint32_t type,
    std::uint32_t code,
    std::int32_t value0 = 0,
    std::int32_t value1 = 0)
{
    if (!queue)
        return false;

    std::uint32_t spins = 0;
    while (InterlockedCompareExchange(&queue->writeLock, 1, 0) != 0)
    {
        if (++spins > 1024)
        {
            InterlockedIncrement(&queue->droppedEvents);
            return false;
        }
        Sleep(0);
    }

    const LONG sequence = queue->writeSequence + 1;
    const std::uint32_t index =
        static_cast<std::uint32_t>(sequence - 1) & (fnvxr::shared::InputEventQueueLength - 1u);
    auto& event = queue->events[index];
    event.sequence = 0;
    event.type = type;
    event.code = code;
    event.value0 = value0;
    event.value1 = value1;
    event.flags = 0;
    event.frame = 0;
    MemoryBarrier();
    event.sequence = static_cast<std::uint32_t>(sequence);
    MemoryBarrier();
    queue->writeSequence = sequence;
    InterlockedExchange(&queue->writeLock, 0);
    FlushViewOfFile(queue, sizeof(*queue));

    std::cout
        << "published seq=" << sequence
        << " type=" << type
        << " code=" << code
        << " value=" << value0 << "," << value1 << "\n";
    return true;
}

std::uint32_t keyEventType(const char* action)
{
    if (std::strcmp(action, "down") == 0)
        return fnvxr::shared::InputEventTypeKeyDown;
    if (std::strcmp(action, "up") == 0)
        return fnvxr::shared::InputEventTypeKeyUp;
    if (std::strcmp(action, "tap") == 0)
        return fnvxr::shared::InputEventTypeKeyTap;
    return fnvxr::shared::InputEventTypeNone;
}

std::uint32_t mouseEventType(const char* action)
{
    if (std::strcmp(action, "down") == 0)
        return fnvxr::shared::InputEventTypeMouseButtonDown;
    if (std::strcmp(action, "up") == 0)
        return fnvxr::shared::InputEventTypeMouseButtonUp;
    if (std::strcmp(action, "tap") == 0)
        return fnvxr::shared::InputEventTypeMouseButtonTap;
    return fnvxr::shared::InputEventTypeNone;
}

long parseLong(const char* text)
{
    return std::strtol(text, nullptr, 0);
}
}

int main(int argc, char** argv)
{
    if (argc < 2 || std::strcmp(argv[1], "--help") == 0 || std::strcmp(argv[1], "-h") == 0)
    {
        usage();
        return argc < 2 ? 1 : 0;
    }

    InputMapping mapping;
    if (!openInputMapping(mapping))
        return fail("failed to open Local\\FNVXR_Input_Events");

    const char* target = argv[1];
    if (std::strcmp(target, "key") == 0 && argc == 4)
    {
        const std::uint32_t type = keyEventType(argv[2]);
        const long code = parseLong(argv[3]);
        if (type == fnvxr::shared::InputEventTypeNone || code < 0 || code > 255)
            return fail("invalid key event");
        return publishInputEvent(mapping.queue, type, static_cast<std::uint32_t>(code)) ? 0 : 2;
    }

    if (std::strcmp(target, "mouse") == 0 && argc == 4)
    {
        const std::uint32_t type = mouseEventType(argv[2]);
        const long button = parseLong(argv[3]);
        if (type == fnvxr::shared::InputEventTypeNone || button < 0 || button > 7)
            return fail("invalid mouse event");
        return publishInputEvent(mapping.queue, type, static_cast<std::uint32_t>(button)) ? 0 : 2;
    }

    if (std::strcmp(target, "move") == 0 && argc == 4)
    {
        return publishInputEvent(
            mapping.queue,
            fnvxr::shared::InputEventTypeMouseMove,
            0,
            static_cast<std::int32_t>(parseLong(argv[2])),
            static_cast<std::int32_t>(parseLong(argv[3]))) ? 0 : 2;
    }

    if (std::strcmp(target, "wheel") == 0 && argc == 3)
    {
        return publishInputEvent(
            mapping.queue,
            fnvxr::shared::InputEventTypeMouseWheel,
            0,
            static_cast<std::int32_t>(parseLong(argv[2]))) ? 0 : 2;
    }

    usage();
    return 1;
}
