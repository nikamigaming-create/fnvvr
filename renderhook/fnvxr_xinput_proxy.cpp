#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <xinput.h>

#include "../protocol/fnvxr_shared_state.h"

#include <cstdarg>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstring>

#ifndef VK_PAD_A
#define VK_PAD_A 0x5800
#endif
#ifndef VK_PAD_B
#define VK_PAD_B 0x5801
#endif
#ifndef VK_PAD_X
#define VK_PAD_X 0x5802
#endif
#ifndef VK_PAD_Y
#define VK_PAD_Y 0x5803
#endif
#ifndef VK_PAD_DPAD_UP
#define VK_PAD_DPAD_UP 0x5810
#endif
#ifndef VK_PAD_DPAD_DOWN
#define VK_PAD_DPAD_DOWN 0x5811
#endif
#ifndef VK_PAD_DPAD_LEFT
#define VK_PAD_DPAD_LEFT 0x5812
#endif
#ifndef VK_PAD_DPAD_RIGHT
#define VK_PAD_DPAD_RIGHT 0x5813
#endif
#ifndef VK_PAD_START
#define VK_PAD_START 0x5814
#endif
#ifndef VK_PAD_BACK
#define VK_PAD_BACK 0x5815
#endif
#ifndef VK_PAD_LTHUMB_PRESS
#define VK_PAD_LTHUMB_PRESS 0x5816
#endif
#ifndef VK_PAD_RTHUMB_PRESS
#define VK_PAD_RTHUMB_PRESS 0x5817
#endif

namespace
{
constexpr std::uint32_t XInputSharedMagic = fnvxr::shared::XInputSharedMagic;
constexpr std::uint32_t XInputSharedVersion = fnvxr::shared::XInputSharedVersion;
constexpr std::uint8_t XInputReservedRetailConsumed = fnvxr::shared::XInputReservedRetailConsumed;
constexpr std::uint8_t XInputReservedAutoRun = fnvxr::shared::XInputReservedAutoRun;
constexpr std::uint8_t XInputReservedMovementMode = fnvxr::shared::XInputReservedMovementMode;

using fnvxr::shared::SharedXInputState;

HMODULE g_realXInput = nullptr;
HANDLE g_mapping = nullptr;
SharedXInputState* g_shared = nullptr;
std::uint32_t g_lastKeystrokePacket = 0;
std::uint16_t g_lastKeystrokeButtons = 0;
XINPUT_KEYSTROKE g_keystrokeQueue[32] {};
std::uint8_t g_keystrokeHead = 0;
std::uint8_t g_keystrokeTail = 0;
LONG g_loggedSharedOpen = 0;
LONG g_loggedSharedOpenFailed = 0;
LONG g_loggedGetState = 0;
LONG g_loggedGetStateVirtual = 0;
LONG g_loggedGetCapabilities = 0;
LONG g_loggedGetKeystroke = 0;
LONG g_loggedGetKeystrokeEvent = 0;
SRWLOCK g_sharedSnapshotLock = SRWLOCK_INIT;
SharedXInputState g_lastStableShared {};
bool g_haveLastStableShared = false;
bool g_returningNeutralShared = false;
std::uint32_t g_lastAdvancingPacket = 0;
std::uint32_t g_lastReturnedSourcePacket = 0;
std::uint32_t g_effectivePacket = 0;
ULONGLONG g_lastPacketAdvanceMs = 0;

bool envEnabled(const char* name, bool fallback)
{
    char buffer[8] {};
    const DWORD length = GetEnvironmentVariableA(name, buffer, static_cast<DWORD>(sizeof(buffer)));
    if (length == 0 || length >= sizeof(buffer))
        return fallback;
    return buffer[0] != '0';
}

int envInt(const char* name, int fallback)
{
    char buffer[32] {};
    const DWORD length = GetEnvironmentVariableA(name, buffer, static_cast<DWORD>(sizeof(buffer)));
    if (length == 0 || length >= sizeof(buffer))
        return fallback;
    char* end = nullptr;
    const long value = std::strtol(buffer, &end, 10);
    return end && *end == '\0' ? static_cast<int>(value) : fallback;
}

float envFloat(const char* name, float fallback)
{
    char buffer[32] {};
    const DWORD length = GetEnvironmentVariableA(name, buffer, static_cast<DWORD>(sizeof(buffer)));
    if (length == 0 || length >= sizeof(buffer))
        return fallback;
    char* end = nullptr;
    const float value = std::strtof(buffer, &end);
    return end && *end == '\0' ? value : fallback;
}

bool buildLogPath(char* path, size_t pathSize, const char* relativePath)
{
    if (!path || pathSize == 0 || !relativePath)
        return false;

    path[0] = '\0';
    const DWORD runDirLength = GetEnvironmentVariableA("FNVXR_RUN_LOG_DIR", path, static_cast<DWORD>(pathSize));
    if (runDirLength > 0)
    {
        if (runDirLength >= pathSize)
            return false;
        const size_t length = std::strlen(path);
        if (length > 0 && path[length - 1] != '\\' && path[length - 1] != '/')
        {
            if (strcat_s(path, pathSize, "\\") != 0)
                return false;
        }
        return strcat_s(path, pathSize, "fnvxr_xinput_proxy.log") == 0;
    }

    DWORD length = GetModuleFileNameA(nullptr, path, static_cast<DWORD>(pathSize));
    if (length == 0 || length >= pathSize)
        return false;
    char* slash = strrchr(path, '\\');
    if (!slash)
        return false;
    *(slash + 1) = '\0';
    return strcat_s(path, pathSize, relativePath) == 0;
}

void logLine(const char* format, ...)
{
    char path[MAX_PATH] {};
    if (!buildLogPath(path, sizeof(path), "Data\\NVSE\\Plugins\\fnvxr_xinput_proxy.log"))
        return;

    HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;

    char message[512] {};
    va_list args;
    va_start(args, format);
    wvsprintfA(message, format, args);
    va_end(args);

    DWORD written = 0;
    WriteFile(file, message, lstrlenA(message), &written, nullptr);
    CloseHandle(file);
}

template <typename Fn>
Fn realProc(const char* name)
{
    if (!g_realXInput)
    {
        char path[MAX_PATH] {};
        GetSystemDirectoryA(path, sizeof(path));
        lstrcatA(path, "\\xinput1_3.dll");
        g_realXInput = LoadLibraryA(path);
    }

    return g_realXInput ? reinterpret_cast<Fn>(GetProcAddress(g_realXInput, name)) : nullptr;
}

template <typename Fn>
Fn realProcOrdinal(WORD ordinal)
{
    if (!g_realXInput)
    {
        char path[MAX_PATH] {};
        GetSystemDirectoryA(path, sizeof(path));
        lstrcatA(path, "\\xinput1_3.dll");
        g_realXInput = LoadLibraryA(path);
    }

    return g_realXInput ? reinterpret_cast<Fn>(GetProcAddress(g_realXInput, MAKEINTRESOURCEA(ordinal))) : nullptr;
}

SharedXInputState* sharedState()
{
    if (g_shared)
        return g_shared;

    g_mapping = OpenFileMappingA(
        FILE_MAP_READ | FILE_MAP_WRITE,
        FALSE,
        fnvxr::shared::XInputSharedMappingName);
    if (!g_mapping)
    {
        if (InterlockedCompareExchange(&g_loggedSharedOpenFailed, 1, 0) == 0)
            logLine("shared open failed err=%lu\n", GetLastError());
        return nullptr;
    }

    g_shared = static_cast<SharedXInputState*>(
        MapViewOfFile(g_mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(SharedXInputState)));
    if (!g_shared)
    {
        const DWORD error = GetLastError();
        CloseHandle(g_mapping);
        g_mapping = nullptr;
        logLine("shared map failed err=%lu\n", error);
        return nullptr;
    }
    if (g_shared && InterlockedCompareExchange(&g_loggedSharedOpen, 1, 0) == 0)
    {
        SharedXInputState snapshot {};
        const bool stable = fnvxr::shared::readSequencedSharedSnapshot(g_shared, snapshot);
        logLine(
            "shared mapped state=%p stable=%d magic=0x%08lx version=%lu connected=%lu packet=%lu buttons=0x%04x\n",
            g_shared,
            stable ? 1 : 0,
            stable ? snapshot.magic : 0,
            stable ? snapshot.version : 0,
            stable ? static_cast<unsigned long>(snapshot.connected) : 0,
            stable ? snapshot.packet : 0,
            stable ? static_cast<unsigned>(snapshot.buttons) : 0);
    }
    return g_shared;
}

bool readEffectiveSharedState(SharedXInputState& effective)
{
    SharedXInputState* mapped = sharedState();
    if (!mapped)
        return false;

    const ULONGLONG nowMs = GetTickCount64();
    const ULONGLONG staleMs = static_cast<ULONGLONG>(
        std::max(50, envInt("FNVXR_XINPUT_STALE_PACKET_MS", 250)));

    AcquireSRWLockExclusive(&g_sharedSnapshotLock);
    SharedXInputState current {};
    const bool currentValid = fnvxr::shared::readSequencedSharedSnapshot(mapped, current)
        && current.magic == XInputSharedMagic
        && current.version == XInputSharedVersion;
    if (currentValid)
    {
        if (!g_haveLastStableShared || current.packet != g_lastAdvancingPacket)
        {
            g_lastAdvancingPacket = current.packet;
            g_lastPacketAdvanceMs = nowMs;
        }
        g_lastStableShared = current;
        g_haveLastStableShared = true;
    }

    const bool fresh = g_haveLastStableShared
        && g_lastPacketAdvanceMs != 0
        && nowMs - g_lastPacketAdvanceMs <= staleMs
        && g_lastStableShared.connected != 0;
    if (fresh)
    {
        effective = g_lastStableShared;
        if (g_returningNeutralShared
            || g_effectivePacket == 0
            || g_lastReturnedSourcePacket != g_lastStableShared.packet)
        {
            ++g_effectivePacket;
            g_lastReturnedSourcePacket = g_lastStableShared.packet;
        }
        effective.packet = g_effectivePacket;
        g_returningNeutralShared = false;
        ReleaseSRWLockExclusive(&g_sharedSnapshotLock);
        return true;
    }

    if (!g_returningNeutralShared)
    {
        ++g_effectivePacket;
        g_returningNeutralShared = true;
    }
    effective = {};
    effective.magic = XInputSharedMagic;
    effective.version = XInputSharedVersion;
    effective.packet = g_effectivePacket;
    // Keep the virtual device owned by this proxy while neutral. Falling
    // through to a physical controller would create an unrelated input source.
    effective.connected = 1;
    if (g_haveLastStableShared)
    {
        // Preserve only the proxy-owned acknowledgement. Auto-run and movement
        // mode must be neutral or a dead host could synthesize full-forward LY.
        effective.reserved[XInputReservedRetailConsumed] =
            g_lastStableShared.reserved[XInputReservedRetailConsumed];
    }
    ReleaseSRWLockExclusive(&g_sharedSnapshotLock);
    return true;
}

bool fillVirtualState(DWORD userIndex, XINPUT_STATE* state)
{
    if (userIndex != 0 || !state)
        return false;

    SharedXInputState snapshot {};
    if (!readEffectiveSharedState(snapshot))
    {
        return false;
    }
    const SharedXInputState* shared = &snapshot;

    std::memset(state, 0, sizeof(*state));
    state->dwPacketNumber = shared->packet;
    std::uint16_t buttons = shared->buttons;
    if (envEnabled("FNVXR_XINPUT_MASK_X", true))
        buttons &= ~XINPUT_GAMEPAD_X;
    if (envEnabled("FNVXR_XINPUT_MASK_B", true))
        buttons &= ~XINPUT_GAMEPAD_B;
    if (envEnabled("FNVXR_XINPUT_MASK_Y", true))
        buttons &= ~XINPUT_GAMEPAD_Y;
    if (envEnabled("FNVXR_XINPUT_MASK_THUMBSTICK_CLICKS", true))
        buttons &= static_cast<std::uint16_t>(~(XINPUT_GAMEPAD_LEFT_THUMB | XINPUT_GAMEPAD_RIGHT_THUMB));
    state->Gamepad.wButtons = buttons;
    state->Gamepad.bLeftTrigger = shared->leftTrigger;
    state->Gamepad.bRightTrigger = shared->rightTrigger;
    state->Gamepad.sThumbLX = shared->leftThumbX;
    state->Gamepad.sThumbLY = shared->leftThumbY;
    const double rightStickScale = std::clamp(envFloat("FNVXR_XINPUT_RIGHT_STICK_SCALE", 1.12f), 0.25f, 2.50f);
    const bool thirdPersonZoomHeld =
        (shared->buttons & XINPUT_GAMEPAD_LEFT_THUMB) != 0
        && envEnabled("FNVXR_THIRD_PERSON_L3_ENABLE", true);
    state->Gamepad.sThumbRX = thirdPersonZoomHeld
        ? 0
        : static_cast<SHORT>(std::clamp(
            static_cast<int>(std::lround(static_cast<double>(shared->rightThumbX) * rightStickScale)),
            -32767,
            32767));
    state->Gamepad.sThumbRY =
        (!thirdPersonZoomHeld && envEnabled("FNVXR_XINPUT_RIGHT_STICK_Y_ENABLE", true))
            ? static_cast<SHORT>(std::clamp(
                static_cast<int>(std::lround(static_cast<double>(shared->rightThumbY) * rightStickScale)),
                -32767,
                32767))
            : 0;
    const std::uint8_t movementMode = shared->reserved[XInputReservedMovementMode];
    const int movementDeadzone = envInt("FNVXR_XINPUT_MOVEMENT_MODE_DEADZONE", 4500);
    const int stickMagnitude = std::max(std::abs(static_cast<int>(state->Gamepad.sThumbLX)), std::abs(static_cast<int>(state->Gamepad.sThumbLY)));
    if (envEnabled("FNVXR_XINPUT_MOVEMENT_MODE_SCALE_ENABLE", false) && stickMagnitude > movementDeadzone)
    {
        if (movementMode == 1)
        {
            const double walkScale = std::clamp(envFloat("FNVXR_XINPUT_WALK_SCALE", 0.45f), 0.10f, 0.95f);
            state->Gamepad.sThumbLX = static_cast<SHORT>(std::lround(static_cast<double>(state->Gamepad.sThumbLX) * walkScale));
            state->Gamepad.sThumbLY = static_cast<SHORT>(std::lround(static_cast<double>(state->Gamepad.sThumbLY) * walkScale));
        }
        else if (movementMode == 2)
        {
            const double runFloor = std::clamp(envFloat("FNVXR_XINPUT_RUN_FLOOR", 0.75f), 0.45f, 1.0f);
            const double desired = 32767.0 * runFloor;
            const double scale = std::max(1.0, desired / static_cast<double>(stickMagnitude));
            state->Gamepad.sThumbLX = static_cast<SHORT>(std::clamp(
                static_cast<int>(std::lround(static_cast<double>(state->Gamepad.sThumbLX) * scale)),
                -32767,
                32767));
            state->Gamepad.sThumbLY = static_cast<SHORT>(std::clamp(
                static_cast<int>(std::lround(static_cast<double>(state->Gamepad.sThumbLY) * scale)),
                -32767,
                32767));
        }
    }
    if (envEnabled("FNVXR_XINPUT_AUTO_RUN_LEFT_THUMB_ENABLE", true)
        && shared->reserved[XInputReservedAutoRun] != 0
        && state->Gamepad.sThumbLY > -8000)
    {
        state->Gamepad.sThumbLY = 32767;
    }
    // This byte is consumer-owned acknowledgement metadata. Producers never
    // include it in their sequenced frame mutation after initialization.
    if (g_shared)
        InterlockedExchange8(
            reinterpret_cast<volatile char*>(&g_shared->reserved[XInputReservedRetailConsumed]),
            1);
    return true;
}

WORD virtualKeyForButton(std::uint16_t button)
{
    switch (button)
    {
        case XINPUT_GAMEPAD_DPAD_UP:
            return VK_PAD_DPAD_UP;
        case XINPUT_GAMEPAD_DPAD_DOWN:
            return VK_PAD_DPAD_DOWN;
        case XINPUT_GAMEPAD_DPAD_LEFT:
            return VK_PAD_DPAD_LEFT;
        case XINPUT_GAMEPAD_DPAD_RIGHT:
            return VK_PAD_DPAD_RIGHT;
        case XINPUT_GAMEPAD_START:
            return VK_PAD_START;
        case XINPUT_GAMEPAD_BACK:
            return VK_PAD_BACK;
        case XINPUT_GAMEPAD_LEFT_THUMB:
            return VK_PAD_LTHUMB_PRESS;
        case XINPUT_GAMEPAD_RIGHT_THUMB:
            return VK_PAD_RTHUMB_PRESS;
        case XINPUT_GAMEPAD_A:
            return VK_PAD_A;
        case XINPUT_GAMEPAD_B:
            return VK_PAD_B;
        case XINPUT_GAMEPAD_X:
            return VK_PAD_X;
        case XINPUT_GAMEPAD_Y:
            return VK_PAD_Y;
        default:
            return 0;
    }
}

void queueKeystroke(WORD virtualKey, WORD flags, BYTE userIndex)
{
    if (!virtualKey)
        return;

    const std::uint8_t nextTail = static_cast<std::uint8_t>((g_keystrokeTail + 1) % 32);
    if (nextTail == g_keystrokeHead)
        g_keystrokeHead = static_cast<std::uint8_t>((g_keystrokeHead + 1) % 32);

    XINPUT_KEYSTROKE& key = g_keystrokeQueue[g_keystrokeTail];
    std::memset(&key, 0, sizeof(key));
    key.VirtualKey = virtualKey;
    key.Flags = flags;
    key.UserIndex = userIndex;
    g_keystrokeTail = nextTail;
}

bool popKeystroke(PXINPUT_KEYSTROKE key)
{
    if (g_keystrokeHead == g_keystrokeTail)
        return false;

    *key = g_keystrokeQueue[g_keystrokeHead];
    g_keystrokeHead = static_cast<std::uint8_t>((g_keystrokeHead + 1) % 32);
    return true;
}

bool sharedConnected(const SharedXInputState* shared)
{
    return shared
        && shared->magic == XInputSharedMagic
        && shared->version == XInputSharedVersion
        && shared->connected;
}

bool synthesizeKeystrokesFromShared(DWORD userIndex)
{
    if (userIndex != 0)
        return false;

    SharedXInputState snapshot {};
    if (!readEffectiveSharedState(snapshot)
        || !sharedConnected(&snapshot))
        return false;
    const SharedXInputState* shared = &snapshot;

    if (shared->packet == g_lastKeystrokePacket)
        return true;

    std::uint16_t currentButtons = shared->buttons;
    if (envEnabled("FNVXR_XINPUT_MASK_X", true))
        currentButtons &= ~XINPUT_GAMEPAD_X;
    if (envEnabled("FNVXR_XINPUT_MASK_B", true))
        currentButtons &= ~XINPUT_GAMEPAD_B;
    if (envEnabled("FNVXR_XINPUT_MASK_Y", true))
        currentButtons &= ~XINPUT_GAMEPAD_Y;
    if (envEnabled("FNVXR_XINPUT_MASK_THUMBSTICK_CLICKS", true))
        currentButtons &= static_cast<std::uint16_t>(~(XINPUT_GAMEPAD_LEFT_THUMB | XINPUT_GAMEPAD_RIGHT_THUMB));
    const std::uint16_t changed = currentButtons ^ g_lastKeystrokeButtons;
    constexpr std::uint16_t buttons[] = {
        XINPUT_GAMEPAD_DPAD_UP,
        XINPUT_GAMEPAD_DPAD_DOWN,
        XINPUT_GAMEPAD_DPAD_LEFT,
        XINPUT_GAMEPAD_DPAD_RIGHT,
        XINPUT_GAMEPAD_START,
        XINPUT_GAMEPAD_BACK,
        XINPUT_GAMEPAD_LEFT_THUMB,
        XINPUT_GAMEPAD_RIGHT_THUMB,
        XINPUT_GAMEPAD_A,
        XINPUT_GAMEPAD_B,
        XINPUT_GAMEPAD_X,
        XINPUT_GAMEPAD_Y
    };

    for (std::uint16_t button : buttons)
    {
        if (!(changed & button))
            continue;

        const WORD flags = (currentButtons & button) ? XINPUT_KEYSTROKE_KEYDOWN : XINPUT_KEYSTROKE_KEYUP;
        queueKeystroke(virtualKeyForButton(button), flags, static_cast<BYTE>(userIndex));
    }

    g_lastKeystrokeButtons = currentButtons;
    g_lastKeystrokePacket = shared->packet;
    return true;
}

void cleanup()
{
    if (g_shared)
    {
        UnmapViewOfFile(g_shared);
        g_shared = nullptr;
    }
    if (g_mapping)
    {
        CloseHandle(g_mapping);
        g_mapping = nullptr;
    }
    if (g_realXInput)
    {
        FreeLibrary(g_realXInput);
        g_realXInput = nullptr;
    }
    g_keystrokeHead = 0;
    g_keystrokeTail = 0;
    AcquireSRWLockExclusive(&g_sharedSnapshotLock);
    g_lastStableShared = {};
    g_haveLastStableShared = false;
    g_returningNeutralShared = false;
    g_lastAdvancingPacket = 0;
    g_lastReturnedSourcePacket = 0;
    g_effectivePacket = 0;
    g_lastPacketAdvanceMs = 0;
    ReleaseSRWLockExclusive(&g_sharedSnapshotLock);
}
}

extern "C" DWORD WINAPI FNVXR_XInputGetState(DWORD dwUserIndex, XINPUT_STATE* pState)
{
    if (InterlockedCompareExchange(&g_loggedGetState, 1, 0) == 0)
        logLine("XInputGetState user=%lu\n", dwUserIndex);

    if (fillVirtualState(dwUserIndex, pState))
    {
        if ((pState->Gamepad.wButtons || pState->Gamepad.sThumbLX || pState->Gamepad.sThumbLY || pState->Gamepad.sThumbRX || pState->Gamepad.sThumbRY)
            || InterlockedCompareExchange(&g_loggedGetStateVirtual, 1, 0) == 0)
        {
            logLine(
                "XInputGetState virtual user=%lu packet=%lu buttons=0x%04x lt=%u rt=%u ls=%d,%d rs=%d,%d\n",
                dwUserIndex,
                pState->dwPacketNumber,
                static_cast<unsigned>(pState->Gamepad.wButtons),
                static_cast<unsigned>(pState->Gamepad.bLeftTrigger),
                static_cast<unsigned>(pState->Gamepad.bRightTrigger),
                static_cast<int>(pState->Gamepad.sThumbLX),
                static_cast<int>(pState->Gamepad.sThumbLY),
                static_cast<int>(pState->Gamepad.sThumbRX),
                static_cast<int>(pState->Gamepad.sThumbRY));
        }
        return ERROR_SUCCESS;
    }

    using Fn = DWORD (WINAPI*)(DWORD, XINPUT_STATE*);
    if (Fn fn = realProc<Fn>("XInputGetState"))
        return fn(dwUserIndex, pState);
    return ERROR_DEVICE_NOT_CONNECTED;
}

extern "C" DWORD WINAPI FNVXR_XInputSetState(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
{
    using Fn = DWORD (WINAPI*)(DWORD, XINPUT_VIBRATION*);
    if (Fn fn = realProc<Fn>("XInputSetState"))
        return fn(dwUserIndex, pVibration);
    return ERROR_DEVICE_NOT_CONNECTED;
}

extern "C" DWORD WINAPI FNVXR_XInputGetCapabilities(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES* pCapabilities)
{
    if (InterlockedCompareExchange(&g_loggedGetCapabilities, 1, 0) == 0)
        logLine("XInputGetCapabilities user=%lu flags=0x%lx\n", dwUserIndex, dwFlags);

    if (dwUserIndex == 0 && pCapabilities)
    {
        std::memset(pCapabilities, 0, sizeof(*pCapabilities));
        pCapabilities->Type = XINPUT_DEVTYPE_GAMEPAD;
        pCapabilities->SubType = XINPUT_DEVSUBTYPE_GAMEPAD;
        pCapabilities->Flags = XINPUT_CAPS_VOICE_SUPPORTED;
        pCapabilities->Gamepad.wButtons = 0xF3FF;
        pCapabilities->Gamepad.bLeftTrigger = 255;
        pCapabilities->Gamepad.bRightTrigger = 255;
        pCapabilities->Gamepad.sThumbLX = 32767;
        pCapabilities->Gamepad.sThumbLY = 32767;
        pCapabilities->Gamepad.sThumbRX = 32767;
        pCapabilities->Gamepad.sThumbRY = 32767;
        return ERROR_SUCCESS;
    }

    using Fn = DWORD (WINAPI*)(DWORD, DWORD, XINPUT_CAPABILITIES*);
    if (Fn fn = realProc<Fn>("XInputGetCapabilities"))
        return fn(dwUserIndex, dwFlags, pCapabilities);
    return ERROR_DEVICE_NOT_CONNECTED;
}

extern "C" void WINAPI FNVXR_XInputEnable(BOOL enable)
{
    using Fn = void (WINAPI*)(BOOL);
    if (Fn fn = realProc<Fn>("XInputEnable"))
        fn(enable);
}

extern "C" DWORD WINAPI FNVXR_XInputGetDSoundAudioDeviceGuids(DWORD dwUserIndex, GUID* pDSoundRenderGuid, GUID* pDSoundCaptureGuid)
{
    using Fn = DWORD (WINAPI*)(DWORD, GUID*, GUID*);
    if (Fn fn = realProc<Fn>("XInputGetDSoundAudioDeviceGuids"))
        return fn(dwUserIndex, pDSoundRenderGuid, pDSoundCaptureGuid);
    return ERROR_DEVICE_NOT_CONNECTED;
}

extern "C" DWORD WINAPI FNVXR_XInputGetBatteryInformation(DWORD dwUserIndex, BYTE devType, XINPUT_BATTERY_INFORMATION* pBatteryInformation)
{
    if (dwUserIndex == 0 && pBatteryInformation)
    {
        pBatteryInformation->BatteryType = BATTERY_TYPE_WIRED;
        pBatteryInformation->BatteryLevel = BATTERY_LEVEL_FULL;
        return ERROR_SUCCESS;
    }

    using Fn = DWORD (WINAPI*)(DWORD, BYTE, XINPUT_BATTERY_INFORMATION*);
    if (Fn fn = realProc<Fn>("XInputGetBatteryInformation"))
        return fn(dwUserIndex, devType, pBatteryInformation);
    return ERROR_DEVICE_NOT_CONNECTED;
}

extern "C" DWORD WINAPI FNVXR_XInputGetKeystroke(DWORD dwUserIndex, DWORD dwReserved, PXINPUT_KEYSTROKE pKeystroke)
{
    if (!pKeystroke)
        return ERROR_BAD_ARGUMENTS;

    if (InterlockedCompareExchange(&g_loggedGetKeystroke, 1, 0) == 0)
        logLine("XInputGetKeystroke user=%lu reserved=0x%lx\n", dwUserIndex, dwReserved);

    if (synthesizeKeystrokesFromShared(dwUserIndex))
    {
        if (popKeystroke(pKeystroke))
        {
            if (InterlockedIncrement(&g_loggedGetKeystrokeEvent) <= 64)
            {
                logLine(
                    "XInputGetKeystroke virtual user=%lu vk=0x%04x flags=0x%04x queue=%u,%u\n",
                    dwUserIndex,
                    static_cast<unsigned>(pKeystroke->VirtualKey),
                    static_cast<unsigned>(pKeystroke->Flags),
                    static_cast<unsigned>(g_keystrokeHead),
                    static_cast<unsigned>(g_keystrokeTail));
            }
            return ERROR_SUCCESS;
        }
        return ERROR_EMPTY;
    }

    using Fn = DWORD (WINAPI*)(DWORD, DWORD, PXINPUT_KEYSTROKE);
    if (Fn fn = realProc<Fn>("XInputGetKeystroke"))
        return fn(dwUserIndex, dwReserved, pKeystroke);
    return ERROR_EMPTY;
}

extern "C" DWORD WINAPI FNVXR_XInputGetStateEx(DWORD dwUserIndex, XINPUT_STATE* pState)
{
    return FNVXR_XInputGetState(dwUserIndex, pState);
}

extern "C" DWORD WINAPI FNVXR_XInputWaitForGuideButton(DWORD dwUserIndex, DWORD dwFlag, void* unknown)
{
    using Fn = DWORD (WINAPI*)(DWORD, DWORD, void*);
    if (Fn fn = realProcOrdinal<Fn>(101))
        return fn(dwUserIndex, dwFlag, unknown);
    return ERROR_DEVICE_NOT_CONNECTED;
}

extern "C" DWORD WINAPI FNVXR_XInputCancelGuideButtonWait(DWORD dwUserIndex)
{
    using Fn = DWORD (WINAPI*)(DWORD);
    if (Fn fn = realProcOrdinal<Fn>(102))
        return fn(dwUserIndex);
    return ERROR_DEVICE_NOT_CONNECTED;
}

extern "C" DWORD WINAPI FNVXR_XInputPowerOffController(DWORD dwUserIndex)
{
    using Fn = DWORD (WINAPI*)(DWORD);
    if (Fn fn = realProcOrdinal<Fn>(103))
        return fn(dwUserIndex);
    return ERROR_SUCCESS;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module);
        logLine("DllMain attach xinput proxy pid=%lu\n", GetCurrentProcessId());
    }
    // Process teardown releases mappings/modules. Blocking on SRW locks or
    // calling FreeLibrary from DllMain would run under the loader lock.
    return TRUE;
}
