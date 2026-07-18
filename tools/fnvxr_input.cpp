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
    HANDLE writerMutex = nullptr;
    SharedInputEventQueue* queue = nullptr;

    ~InputMapping()
    {
        if (queue)
            UnmapViewOfFile(queue);
        if (handle)
            CloseHandle(handle);
        if (writerMutex)
            CloseHandle(writerMutex);
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
    mapping.writerMutex = CreateMutexA(
        nullptr,
        FALSE,
        fnvxr::shared::InputEventWriterMutexName);
    if (!mapping.writerMutex)
        return false;
    const DWORD writerWait = WaitForSingleObject(mapping.writerMutex, 2000);
    if (writerWait != WAIT_OBJECT_0 && writerWait != WAIT_ABANDONED)
        return false;

    mapping.handle = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(SharedInputEventQueue),
        InputMapName);
    if (!mapping.handle)
    {
        ReleaseMutex(mapping.writerMutex);
        return false;
    }

    mapping.queue = static_cast<SharedInputEventQueue*>(
        MapViewOfFile(mapping.handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedInputEventQueue)));
    if (!mapping.queue)
    {
        ReleaseMutex(mapping.writerMutex);
        return false;
    }

    if (mapping.queue->magic != fnvxr::shared::InputEventSharedMagic
        || mapping.queue->version != fnvxr::shared::InputEventSharedVersion)
    {
        std::memset(mapping.queue, 0, sizeof(*mapping.queue));
        mapping.queue->magic = fnvxr::shared::InputEventSharedMagic;
        mapping.queue->version = fnvxr::shared::InputEventSharedVersion;
    }
    InterlockedExchange(&mapping.queue->writeLock, 0);
    ReleaseMutex(mapping.writerMutex);
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

    // Every writer holds the same abandoned-detecting role mutex, so a dead
    // process cannot leave the queue permanently locked.
    HANDLE writerMutex = CreateMutexA(
        nullptr,
        FALSE,
        fnvxr::shared::InputEventWriterMutexName);
    if (!writerMutex)
        return false;
    const DWORD writerWait = WaitForSingleObject(writerMutex, 2000);
    if (writerWait != WAIT_OBJECT_0 && writerWait != WAIT_ABANDONED)
    {
        CloseHandle(writerMutex);
        InterlockedIncrement(&queue->droppedEvents);
        return false;
    }
    InterlockedExchange(&queue->writeLock, 1);

    std::uint32_t sequence = fnvxr::shared::sequencedValueBits(queue->writeSequence) + 1u;
    if (sequence == 0u)
        sequence = 1u;
    const std::uint32_t index =
        (sequence - 1u) & (fnvxr::shared::InputEventQueueLength - 1u);
    auto& event = queue->events[index];
    InterlockedExchange(&event.sequence, 0);
    event.type = type;
    event.code = code;
    event.value0 = value0;
    event.value1 = value1;
    event.flags = 0;
    event.frame = 0;
    MemoryBarrier();
    LONG publishedSequence = 0;
    std::memcpy(&publishedSequence, &sequence, sizeof(publishedSequence));
    InterlockedExchange(&event.sequence, publishedSequence);
    MemoryBarrier();
    InterlockedExchange(&queue->writeSequence, publishedSequence);
    InterlockedExchange(&queue->writeLock, 0);
    ReleaseMutex(writerMutex);
    CloseHandle(writerMutex);
    FlushViewOfFile(queue, sizeof(*queue));

    std::cout
        << "published seq=" << static_cast<unsigned long>(sequence)
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
