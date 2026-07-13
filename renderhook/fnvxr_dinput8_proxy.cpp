#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800

#include <windows.h>
#include <dinput.h>

#include "../protocol/fnvxr_shared_state.h"

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>

namespace
{
constexpr std::uint32_t DInputSharedMagic = fnvxr::shared::DInputSharedMagic;
constexpr std::uint32_t DInputSharedVersion = fnvxr::shared::DInputSharedVersion;
constexpr std::uint32_t InputEventSharedMagic = fnvxr::shared::InputEventSharedMagic;
constexpr std::uint32_t InputEventSharedVersion = fnvxr::shared::InputEventSharedVersion;
constexpr std::uint32_t InputEventQueueLength = fnvxr::shared::InputEventQueueLength;
constexpr std::uint32_t XInputSharedMagic = fnvxr::shared::XInputSharedMagic;
constexpr std::uint32_t XInputSharedVersion = fnvxr::shared::XInputSharedVersion;
constexpr std::uint32_t DInputGameplayFlagAimHeld = fnvxr::shared::DInputGameplayFlagAimHeld;
constexpr std::uint32_t DInputGameplayFlagThirdPersonZoomHeld = fnvxr::shared::DInputGameplayFlagThirdPersonZoomHeld;
constexpr std::uint32_t DInputGameplayFlagWeaponOut = fnvxr::shared::DInputGameplayFlagWeaponOut;
constexpr std::uint8_t XInputReservedRetailConsumed = fnvxr::shared::XInputReservedRetailConsumed;
constexpr std::uint8_t XInputReservedAutoRun = fnvxr::shared::XInputReservedAutoRun;

const GUID FNVXR_GUID_SysMouse = { 0x6F1D2B60, 0xD5A0, 0x11CF, { 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 } };
const GUID FNVXR_GUID_SysKeyboard = { 0x6F1D2B61, 0xD5A0, 0x11CF, { 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 } };

using fnvxr::shared::SharedDInputState;
using fnvxr::shared::SharedInputEvent;
using fnvxr::shared::SharedInputEventQueue;
using fnvxr::shared::SharedXInputState;

HMODULE g_realDInput = nullptr;
HANDLE g_mapping = nullptr;
SharedDInputState* g_shared = nullptr;
HANDLE g_inputEventMapping = nullptr;
SharedInputEventQueue* g_inputEvents = nullptr;
HANDLE g_xinputMapping = nullptr;
SharedXInputState* g_xinputShared = nullptr;
bool g_loggedSharedOpen = false;
bool g_loggedSharedOpenFailed = false;
bool g_loggedInputEventOpen = false;
bool g_loggedInputEventOpenFailed = false;
bool g_loggedXInputSharedOpen = false;
bool g_loggedXInputSharedOpenFailed = false;
bool g_loggedXInputConsumed = false;
bool g_xinputConsumptionLatched = false;
SRWLOCK g_dinputSnapshotLock = SRWLOCK_INIT;
SharedDInputState g_lastStableDInput {};
bool g_haveLastStableDInput = false;
bool g_dinputReturningNeutral = false;
std::uint32_t g_lastAdvancingDInputFrame = 0;
ULONGLONG g_lastDInputAdvanceMs = 0;

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
        return strcat_s(path, pathSize, "fnvxr_dinput_proxy.log") == 0;
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
    if (!buildLogPath(path, sizeof(path), "Data\\NVSE\\Plugins\\fnvxr_dinput_proxy.log"))
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

HMODULE realDInput()
{
    if (!g_realDInput)
    {
        char path[MAX_PATH] {};
        GetSystemDirectoryA(path, sizeof(path));
        lstrcatA(path, "\\dinput8.dll");
        g_realDInput = LoadLibraryA(path);
    }
    return g_realDInput;
}

SharedDInputState* sharedState()
{
    if (g_shared)
        return g_shared;

    g_mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, fnvxr::shared::DInputSharedMappingName);
    if (!g_mapping)
    {
        if (!g_loggedSharedOpenFailed)
        {
            g_loggedSharedOpenFailed = true;
            logLine("shared open failed err=%lu\n", GetLastError());
        }
        return nullptr;
    }

    g_shared = static_cast<SharedDInputState*>(
        MapViewOfFile(g_mapping, FILE_MAP_READ, 0, 0, sizeof(SharedDInputState)));
    if (!g_shared)
    {
        const DWORD error = GetLastError();
        CloseHandle(g_mapping);
        g_mapping = nullptr;
        logLine("shared map failed err=%lu\n", error);
        return nullptr;
    }
    if (g_shared && !g_loggedSharedOpen)
    {
        g_loggedSharedOpen = true;
        logLine("shared mapped state=%p\n", g_shared);
    }
    return g_shared;
}

SharedInputEventQueue* inputEventQueue()
{
    if (g_inputEvents)
        return g_inputEvents;

    g_inputEventMapping = OpenFileMappingA(FILE_MAP_READ, FALSE, "Local\\FNVXR_Input_Events");
    if (!g_inputEventMapping)
    {
        if (!g_loggedInputEventOpenFailed)
        {
            g_loggedInputEventOpenFailed = true;
            logLine("input events open failed err=%lu\n", GetLastError());
        }
        return nullptr;
    }

    g_inputEvents = static_cast<SharedInputEventQueue*>(
        MapViewOfFile(g_inputEventMapping, FILE_MAP_READ, 0, 0, sizeof(SharedInputEventQueue)));
    if (g_inputEvents && !g_loggedInputEventOpen)
    {
        g_loggedInputEventOpen = true;
        logLine("input events mapped queue=%p\n", g_inputEvents);
    }
    return g_inputEvents;
}

SharedXInputState* xinputSharedState()
{
    if (g_xinputShared)
        return g_xinputShared;

    g_xinputMapping = OpenFileMappingA(FILE_MAP_READ, FALSE, fnvxr::shared::XInputSharedMappingName);
    if (!g_xinputMapping)
    {
        if (!g_loggedXInputSharedOpenFailed)
        {
            g_loggedXInputSharedOpenFailed = true;
            logLine("xinput shared open failed err=%lu\n", GetLastError());
        }
        return nullptr;
    }

    g_xinputShared = static_cast<SharedXInputState*>(
        MapViewOfFile(g_xinputMapping, FILE_MAP_READ, 0, 0, sizeof(SharedXInputState)));
    if (!g_xinputShared)
    {
        const DWORD error = GetLastError();
        CloseHandle(g_xinputMapping);
        g_xinputMapping = nullptr;
        logLine("xinput shared map failed err=%lu\n", error);
        return nullptr;
    }
    if (g_xinputShared && !g_loggedXInputSharedOpen)
    {
        g_loggedXInputSharedOpen = true;
        logLine("xinput shared mapped state=%p\n", g_xinputShared);
    }
    return g_xinputShared;
}

bool xinputAnalogConsumed()
{
    if (g_xinputConsumptionLatched)
        return true;

    SharedXInputState* mapped = xinputSharedState();
    SharedXInputState snapshot {};
    const bool stable = fnvxr::shared::readSequencedSharedSnapshot(mapped, snapshot);
    const bool consumed = stable
        && snapshot.magic == XInputSharedMagic
        && snapshot.version == XInputSharedVersion
        && *reinterpret_cast<volatile std::uint8_t*>(
            &mapped->reserved[XInputReservedRetailConsumed]) != 0;
    if (consumed && !g_loggedXInputConsumed)
    {
        g_loggedXInputConsumed = true;
        logLine("xinput analog consumed: disabling DInput keyboard movement fallback packet=%lu ls=%d,%d rs=%d,%d\n",
            snapshot.packet,
            static_cast<int>(snapshot.leftThumbX),
            static_cast<int>(snapshot.leftThumbY),
            static_cast<int>(snapshot.rightThumbX),
            static_cast<int>(snapshot.rightThumbY));
    }
    if (consumed)
        g_xinputConsumptionLatched = true;
    return g_xinputConsumptionLatched;
}

bool xinputAutoRunEnabled()
{
    SharedXInputState* mapped = xinputSharedState();
    if (!mapped
        || mapped->magic != XInputSharedMagic
        || mapped->version != XInputSharedVersion)
    {
        return false;
    }
    return *reinterpret_cast<volatile std::uint8_t*>(
        &mapped->reserved[XInputReservedAutoRun]) != 0;
}

bool validShared(const SharedDInputState* shared)
{
    return shared
        && shared->magic == DInputSharedMagic
        && shared->version == DInputSharedVersion;
}

LONG envLong(const char* name, LONG fallback);

bool readSharedSnapshot(SharedDInputState& snapshot)
{
    SharedDInputState* mapped = sharedState();
    if (!mapped)
        return false;

    const ULONGLONG nowMs = GetTickCount64();
    const ULONGLONG staleMs = static_cast<ULONGLONG>(
        std::max<LONG>(50, envLong("FNVXR_DINPUT_STALE_FRAME_MS", 250)));

    AcquireSRWLockExclusive(&g_dinputSnapshotLock);
    SharedDInputState current {};
    const bool currentValid = fnvxr::shared::readSequencedSharedSnapshot(mapped, current)
        && validShared(&current);
    if (currentValid)
    {
        if (!g_haveLastStableDInput || current.frame != g_lastAdvancingDInputFrame)
        {
            g_lastAdvancingDInputFrame = current.frame;
            g_lastDInputAdvanceMs = nowMs;
        }
        g_lastStableDInput = current;
        g_haveLastStableDInput = true;
    }

    const bool fresh = g_haveLastStableDInput
        && g_lastDInputAdvanceMs != 0
        && nowMs - g_lastDInputAdvanceMs <= staleMs;
    if (fresh)
    {
        snapshot = g_lastStableDInput;
        g_dinputReturningNeutral = false;
        ReleaseSRWLockExclusive(&g_dinputSnapshotLock);
        return true;
    }

    snapshot = {};
    snapshot.magic = DInputSharedMagic;
    snapshot.version = DInputSharedVersion;
    if (g_haveLastStableDInput)
    {
        snapshot.frame = g_lastStableDInput.frame;
        snapshot.mouseClickPacket = g_lastStableDInput.mouseClickPacket;
        snapshot.keyboardAcceptPacket = g_lastStableDInput.keyboardAcceptPacket;
        snapshot.headLookX = g_lastStableDInput.headLookX;
        snapshot.headLookY = g_lastStableDInput.headLookY;
        snapshot.gyroLookX = g_lastStableDInput.gyroLookX;
        snapshot.gyroLookY = g_lastStableDInput.gyroLookY;
    }
    g_dinputReturningNeutral = true;
    ReleaseSRWLockExclusive(&g_dinputSnapshotLock);
    return true;
}

bool validInputEvents(const SharedInputEventQueue* queue)
{
    return queue
        && queue->magic == InputEventSharedMagic
        && queue->version == InputEventSharedVersion
        && queue->writeLock == 0;
}

LONG inputEventStartSequence(LONG lastSequence, LONG writeSequence)
{
    if (writeSequence <= 0)
        return 1;
    const LONG oldestAvailable = std::max<LONG>(1, writeSequence - static_cast<LONG>(InputEventQueueLength) + 1);
    if (lastSequence <= 0)
        return writeSequence + 1;
    return std::max<LONG>(lastSequence + 1, oldestAvailable);
}

bool readInputEvent(const SharedInputEventQueue* queue, LONG sequence, SharedInputEvent& event)
{
    if (!queue || sequence <= 0)
        return false;

    const std::uint32_t index = static_cast<std::uint32_t>(sequence - 1) & (InputEventQueueLength - 1u);
    std::memcpy(&event, &queue->events[index], sizeof(event));
    MemoryBarrier();
    return event.sequence == static_cast<std::uint32_t>(sequence);
}

bool validPointer(const SharedDInputState* shared)
{
    return validShared(shared) && shared->pointerActive && shared->menuInputActive;
}

bool gameplayControlsActive(const SharedDInputState* shared)
{
    return validShared(shared) && shared->gameplayControlsActive != 0;
}

bool envEnabled(const char* name, bool fallback)
{
    char buffer[16] {};
    const DWORD length = GetEnvironmentVariableA(name, buffer, static_cast<DWORD>(sizeof(buffer)));
    if (length == 0 || length >= sizeof(buffer))
        return fallback;

    return buffer[0] == '1' || buffer[0] == 't' || buffer[0] == 'T' || buffer[0] == 'y' || buffer[0] == 'Y';
}

LONG envLong(const char* name, LONG fallback)
{
    char buffer[32] {};
    const DWORD length = GetEnvironmentVariableA(name, buffer, static_cast<DWORD>(sizeof(buffer)));
    if (length == 0 || length >= sizeof(buffer))
        return fallback;

    char* end = nullptr;
    const long value = std::strtol(buffer, &end, 10);
    return end && *end == '\0' ? static_cast<LONG>(value) : fallback;
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

bool headLookInputActive(const SharedDInputState* shared)
{
    return gameplayControlsActive(shared)
        && shared->headLookActive != 0
        && envEnabled("FNVXR_DINPUT_HEAD_LOOK_ENABLE", true);
}

bool aimLookActive(const SharedDInputState* shared)
{
    return validShared(shared)
        && (shared->gameplayFlags & DInputGameplayFlagWeaponOut) != 0
        && envEnabled("FNVXR_DINPUT_HEAD_LOOK_AIM_ENABLE", true);
}

bool precisionLookActive(const SharedDInputState* shared)
{
    return validShared(shared)
        && gameplayControlsActive(shared)
        && (shared->gameplayFlags & DInputGameplayFlagWeaponOut) != 0;
}

bool gyroAimLookActive(const SharedDInputState* shared)
{
    return validShared(shared)
        && gameplayControlsActive(shared)
        && shared->gyroLookActive != 0
        && precisionLookActive(shared)
        && envEnabled("FNVXR_DINPUT_GYRO_AIM_ENABLE", true);
}

bool handspaceLookActive(const SharedDInputState* shared)
{
    return validShared(shared)
        && gameplayControlsActive(shared)
        && shared->gyroLookActive != 0
        && !precisionLookActive(shared)
        && envEnabled("FNVXR_DINPUT_HANDSPACE_LOOK_ENABLE", true);
}

std::uint32_t gameplayLookModeSignature(const SharedDInputState* shared)
{
    if (!validShared(shared))
        return 0;

    std::uint32_t signature = 0;
    if (gameplayControlsActive(shared))
        signature |= 1u << 0;
    if (shared->headLookActive != 0)
        signature |= 1u << 1;
    if (shared->gyroLookActive != 0)
        signature |= 1u << 2;
    if ((shared->gameplayFlags & DInputGameplayFlagWeaponOut) != 0)
        signature |= 1u << 3;
    return signature;
}

bool keyboardMovementEnabled()
{
    char buffer[16] {};
    const DWORD length = GetEnvironmentVariableA("FNVXR_DINPUT_KEYBOARD_MOVEMENT_ENABLE", buffer, static_cast<DWORD>(sizeof(buffer)));
    if (length == 0 || length >= sizeof(buffer))
        return true;
    if (_stricmp(buffer, "auto") == 0)
        return !xinputAnalogConsumed();
    return buffer[0] == '1' || buffer[0] == 't' || buffer[0] == 'T' || buffer[0] == 'y' || buffer[0] == 'Y';
}

LONG clampLong(LONG value, LONG minValue, LONG maxValue)
{
    if (value < minValue)
        return minValue;
    if (value > maxValue)
        return maxValue;
    return value;
}

LONG effectiveLeftStickY(const SharedDInputState* shared)
{
    if (!shared)
        return 0;

    if (envEnabled("FNVXR_DINPUT_AUTO_RUN_LEFT_THUMB_ENABLE", true)
        && xinputAutoRunEnabled()
        && shared->leftStickY > -8000)
    {
        return 32767;
    }
    return shared->leftStickY;
}

LONG forwardAxisValue(const SharedDInputState* shared)
{
    if (!shared)
        return 0;

    const LONG sign = envLong("FNVXR_DINPUT_FORWARD_AXIS_SIGN", 1) < 0 ? -1 : 1;
    return effectiveLeftStickY(shared) * sign;
}

int forwardMoveTier(const SharedDInputState* shared)
{
    if (!gameplayControlsActive(shared))
        return 0;

    const LONG walkThreshold = envLong("FNVXR_DINPUT_WALK_THRESHOLD", 6500);
    const LONG normalThreshold = envLong("FNVXR_DINPUT_NORMAL_THRESHOLD", 18000);
    const LONG runThreshold = envLong("FNVXR_DINPUT_RUN_THRESHOLD", 27500);
    const LONG forwardAxis = forwardAxisValue(shared);
    if (forwardAxis <= walkThreshold)
        return 0;
    if (forwardAxis < normalThreshold)
        return 1;
    if (forwardAxis < runThreshold)
        return 2;
    return 3;
}

bool slowMovePulseActive()
{
    if (!envEnabled("FNVXR_DINPUT_SLOW_PULSE_ENABLE", true))
        return true;

    LONG period = envLong("FNVXR_DINPUT_SLOW_PULSE_MS", 180);
    if (period < 1)
        period = 1;
    LONG dutyPercent = envLong("FNVXR_DINPUT_SLOW_DUTY_PERCENT", 42);
    dutyPercent = clampLong(dutyPercent, 1, 100);
    return static_cast<LONG>(GetTickCount() % static_cast<DWORD>(period)) < (period * dutyPercent) / 100;
}

DWORD movementModifierDik(const char* envName, DWORD fallback)
{
    const LONG value = envLong(envName, static_cast<LONG>(fallback));
    if (value <= 0 || value >= 256)
        return 0;
    return static_cast<DWORD>(value);
}

POINT mapSharedPointerToInput(const SharedDInputState* shared)
{
    POINT point {};
    if (!shared)
        return point;

    const LONG sourceWidth = envLong("FNVXR_UI_SHARED_WIDTH", 1280);
    const LONG sourceHeight = envLong("FNVXR_UI_SHARED_HEIGHT", 720);
    const LONG inputWidth = envLong("FNVXR_UI_INPUT_WIDTH", sourceWidth);
    const LONG inputHeight = envLong("FNVXR_UI_INPUT_HEIGHT", sourceHeight);
    if (sourceWidth <= 1 || sourceHeight <= 1 || inputWidth <= 1 || inputHeight <= 1)
    {
        point.x = shared->clientX;
        point.y = shared->clientY;
        return point;
    }

    point.x = clampLong(
        static_cast<LONG>((static_cast<double>(shared->clientX) * static_cast<double>(inputWidth))
            / static_cast<double>(sourceWidth)),
        0,
        inputWidth - 1);
    point.y = clampLong(
        static_cast<LONG>((static_cast<double>(shared->clientY) * static_cast<double>(inputHeight))
            / static_cast<double>(sourceHeight)),
        0,
        inputHeight - 1);
    return point;
}

void computeGameplayLookDeltas(
    const SharedDInputState* shared,
    LONG& lookX,
    LONG& lookY,
    LONG& stickX,
    LONG& stickY,
    LONG& headX,
    LONG& headY,
    LONG& gyroX,
    LONG& gyroY,
    bool& aimHeld,
    float& headScale,
    float& gyroScale)
{
    lookX = 0;
    lookY = 0;
    stickX = 0;
    stickY = 0;
    headX = 0;
    headY = 0;
    gyroX = 0;
    gyroY = 0;
    aimHeld = false;
    headScale = 0.0f;
    gyroScale = 0.0f;
    if (!gameplayControlsActive(shared))
        return;

    const LONG stickDeadzone = envLong("FNVXR_DINPUT_LOOK_DEADZONE", 6500);
    const float stickScale = envFloat("FNVXR_DINPUT_LOOK_SCALE", 11.2f);
    const bool thirdPersonZoomHeld =
        (shared->gameplayFlags & DInputGameplayFlagThirdPersonZoomHeld) != 0
        && envEnabled("FNVXR_THIRD_PERSON_L3_ENABLE", true);
    if (!thirdPersonZoomHeld)
    {
        const bool rightStickLookEnabled = envEnabled("FNVXR_DINPUT_RIGHT_STICK_LOOK_ENABLE", true);
        if (rightStickLookEnabled && std::abs(shared->rightStickX) > stickDeadzone)
            stickX = static_cast<LONG>(std::lround(static_cast<float>(shared->rightStickX) / 32767.0f * stickScale));
        if (rightStickLookEnabled
            && envEnabled("FNVXR_DINPUT_RIGHT_STICK_PITCH_ENABLE", true)
            && std::abs(shared->rightStickY) > stickDeadzone)
        {
            stickY = static_cast<LONG>(std::lround(static_cast<float>(-shared->rightStickY) / 32767.0f * stickScale));
        }
    }

    if (headLookInputActive(shared))
    {
        aimHeld = aimLookActive(shared);
        const bool suppressHeadForGyroAim =
            gyroAimLookActive(shared)
            && envEnabled("FNVXR_DINPUT_GYRO_AIM_SUPPRESS_HEAD", true);
        const bool suppressHeadForHandspace =
            handspaceLookActive(shared)
            && envEnabled("FNVXR_DINPUT_HANDSPACE_LOOK_SUPPRESS_HEAD", false);
        if (!suppressHeadForGyroAim && !suppressHeadForHandspace)
        {
            const float fallbackScale = envFloat("FNVXR_DINPUT_HEAD_LOOK_SCALE", 800.0f);
            headScale = aimHeld
                ? envFloat("FNVXR_DINPUT_HEAD_LOOK_AIM_SCALE", fallbackScale)
                : envFloat("FNVXR_DINPUT_HEAD_LOOK_NORMAL_SCALE", fallbackScale);
            headX = static_cast<LONG>(std::lround(static_cast<float>(shared->headLookX) * 0.000001f * headScale));
            headY = static_cast<LONG>(std::lround(static_cast<float>(-shared->headLookY) * 0.000001f * headScale));
        }
    }

    if (gyroAimLookActive(shared))
    {
        gyroScale = envFloat("FNVXR_DINPUT_GYRO_AIM_SCALE", 920.0f);
        gyroX = static_cast<LONG>(std::lround(static_cast<float>(shared->gyroLookX) * 0.000001f * gyroScale));
        gyroY = static_cast<LONG>(std::lround(static_cast<float>(-shared->gyroLookY) * 0.000001f * gyroScale));
        aimHeld = true;
    }
    else if (handspaceLookActive(shared))
    {
        gyroScale = envFloat("FNVXR_DINPUT_HANDSPACE_LOOK_SCALE", 720.0f);
        gyroX = static_cast<LONG>(std::lround(static_cast<float>(shared->gyroLookX) * 0.000001f * gyroScale));
        gyroY = static_cast<LONG>(std::lround(static_cast<float>(-shared->gyroLookY) * 0.000001f * gyroScale));
    }

    lookX = stickX + headX + gyroX;
    lookY = stickY + headY + gyroY;
}

void cleanup()
{
    AcquireSRWLockExclusive(&g_dinputSnapshotLock);
    g_lastStableDInput = {};
    g_haveLastStableDInput = false;
    g_dinputReturningNeutral = false;
    g_lastAdvancingDInputFrame = 0;
    g_lastDInputAdvanceMs = 0;
    ReleaseSRWLockExclusive(&g_dinputSnapshotLock);
    g_xinputConsumptionLatched = false;
    if (g_shared)
    {
        UnmapViewOfFile(g_shared);
        g_shared = nullptr;
    }
    if (g_xinputShared)
    {
        UnmapViewOfFile(g_xinputShared);
        g_xinputShared = nullptr;
    }
    if (g_inputEvents)
    {
        UnmapViewOfFile(g_inputEvents);
        g_inputEvents = nullptr;
    }
    if (g_mapping)
    {
        CloseHandle(g_mapping);
        g_mapping = nullptr;
    }
    if (g_xinputMapping)
    {
        CloseHandle(g_xinputMapping);
        g_xinputMapping = nullptr;
    }
    if (g_inputEventMapping)
    {
        CloseHandle(g_inputEventMapping);
        g_inputEventMapping = nullptr;
    }
    if (g_realDInput)
    {
        FreeLibrary(g_realDInput);
        g_realDInput = nullptr;
    }
}

class WrappedDirectInputDevice8A final : public IDirectInputDevice8A
{
public:
    WrappedDirectInputDevice8A(IDirectInputDevice8A* realDevice, bool mouse, bool keyboard)
        : m_real(realDevice)
        , m_mouse(mouse)
        , m_keyboard(keyboard)
    {
        logLine("wrap device mouse=%d keyboard=%d real=%p\n", mouse ? 1 : 0, keyboard ? 1 : 0, realDevice);
        SharedDInputState initial {};
        if (readSharedSnapshot(initial))
        {
            m_lastMouseClickPacket = initial.mouseClickPacket;
            m_lastKeyboardAcceptPacket = initial.keyboardAcceptPacket;
            commitGameplayLookSample(&initial);
        }
        if (const SharedInputEventQueue* queue = inputEventQueue(); validInputEvents(queue))
            m_lastInputEventSequence = queue->writeSequence;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID* out) override
    {
        if (!out)
            return E_POINTER;
        if (IsEqualGUID(riid, IID_IUnknown) || IsEqualGUID(riid, IID_IDirectInputDevice8A))
        {
            *out = static_cast<IDirectInputDevice8A*>(this);
            AddRef();
            return S_OK;
        }
        return m_real->QueryInterface(riid, out);
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_refs); }
    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG refs = InterlockedDecrement(&m_refs);
        if (!refs)
        {
            m_real->Release();
            delete this;
        }
        return refs;
    }

    HRESULT STDMETHODCALLTYPE GetCapabilities(LPDIDEVCAPS value) override { return m_real->GetCapabilities(value); }
    HRESULT STDMETHODCALLTYPE EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA cb, LPVOID ref, DWORD flags) override { return m_real->EnumObjects(cb, ref, flags); }
    HRESULT STDMETHODCALLTYPE GetProperty(REFGUID guid, LPDIPROPHEADER header) override { return m_real->GetProperty(guid, header); }
    HRESULT STDMETHODCALLTYPE SetProperty(REFGUID guid, LPCDIPROPHEADER header) override { return m_real->SetProperty(guid, header); }
    HRESULT STDMETHODCALLTYPE Acquire() override { return m_real->Acquire(); }
    HRESULT STDMETHODCALLTYPE Unacquire() override { return m_real->Unacquire(); }

    HRESULT STDMETHODCALLTYPE GetDeviceState(DWORD bytes, LPVOID data) override
    {
        HRESULT hr = m_real->GetDeviceState(bytes, data);
        if (FAILED(hr) || !data)
            return hr;

        SharedDInputState snapshot {};
        const SharedDInputState* shared = readSharedSnapshot(snapshot) ? &snapshot : nullptr;
        if (m_mouse && !m_loggedMouseStatePoll)
        {
            m_loggedMouseStatePoll = true;
            logLine(
                "GetDeviceState mouse bytes=%lu shared=%p valid=%d active=%lu menu=%lu gameplay=%lu flags=0x%lx aimTrigger=%lu client=%ld,%ld mouseClick=%lu ls=%ld,%ld rs=%ld,%ld head=%lu,%ld,%ld grip=%ld,%ld\n",
                bytes,
                shared,
                validShared(shared) ? 1 : 0,
                shared ? shared->pointerActive : 0,
                shared ? shared->menuInputActive : 0,
                shared ? shared->gameplayControlsActive : 0,
                shared ? static_cast<unsigned long>(shared->gameplayFlags) : 0,
                shared ? shared->aimTrigger : 0,
                shared ? shared->clientX : 0,
                shared ? shared->clientY : 0,
                shared ? shared->mouseClickPacket : 0,
                shared ? static_cast<LONG>(shared->leftStickX) : 0,
                shared ? static_cast<LONG>(shared->leftStickY) : 0,
                shared ? static_cast<LONG>(shared->rightStickX) : 0,
                shared ? static_cast<LONG>(shared->rightStickY) : 0,
                shared ? shared->headLookActive : 0,
                shared ? static_cast<LONG>(shared->headLookX) : 0,
                shared ? static_cast<LONG>(shared->headLookY) : 0,
                shared ? static_cast<LONG>(shared->leftGrip) : 0,
                shared ? static_cast<LONG>(shared->rightGrip) : 0);
        }
        if (m_keyboard && !m_loggedKeyboardStatePoll)
        {
            m_loggedKeyboardStatePoll = true;
            logLine(
                "GetDeviceState keyboard bytes=%lu shared=%p valid=%d active=%lu menu=%lu gameplay=%lu flags=0x%lx aimTrigger=%lu keyboardAccept=%lu ls=%ld,%ld rs=%ld,%ld head=%lu,%ld,%ld grip=%ld,%ld\n",
                bytes,
                shared,
                validShared(shared) ? 1 : 0,
                shared ? shared->pointerActive : 0,
                shared ? shared->menuInputActive : 0,
                shared ? shared->gameplayControlsActive : 0,
                shared ? static_cast<unsigned long>(shared->gameplayFlags) : 0,
                shared ? shared->aimTrigger : 0,
                shared ? shared->keyboardAcceptPacket : 0,
                shared ? static_cast<LONG>(shared->leftStickX) : 0,
                shared ? static_cast<LONG>(shared->leftStickY) : 0,
                shared ? static_cast<LONG>(shared->rightStickX) : 0,
                shared ? static_cast<LONG>(shared->rightStickY) : 0,
                shared ? shared->headLookActive : 0,
                shared ? static_cast<LONG>(shared->headLookX) : 0,
                shared ? static_cast<LONG>(shared->headLookY) : 0,
                shared ? static_cast<LONG>(shared->leftGrip) : 0,
                shared ? static_cast<LONG>(shared->rightGrip) : 0);
        }
        if (m_mouse && bytes >= sizeof(DIMOUSESTATE2))
        {
            auto* mouse = static_cast<DIMOUSESTATE2*>(data);
            injectMouseState2(shared, mouse);
        }
        else if (m_mouse && bytes >= sizeof(DIMOUSESTATE))
        {
            auto* mouse = static_cast<DIMOUSESTATE*>(data);
            injectMouseState(shared, mouse);
        }
        if (m_keyboard && bytes >= 256)
        {
            injectKeyboardState(shared, static_cast<BYTE*>(data));
        }
        return hr;
    }

    HRESULT STDMETHODCALLTYPE GetDeviceData(DWORD dataSize, LPDIDEVICEOBJECTDATA outData, LPDWORD count, DWORD flags) override
    {
        if (!count)
            return m_real->GetDeviceData(dataSize, outData, count, flags);

        DWORD requested = *count;
        HRESULT hr = m_real->GetDeviceData(dataSize, outData, count, flags);
        if (FAILED(hr) || !outData || dataSize != sizeof(DIDEVICEOBJECTDATA) || requested <= *count || (flags & DIGDD_PEEK))
            return hr;

        DWORD written = *count;
        SharedDInputState snapshot {};
        const SharedDInputState* shared = readSharedSnapshot(snapshot) ? &snapshot : nullptr;
        if (!m_loggedBufferedPoll)
        {
            m_loggedBufferedPoll = true;
            logLine(
                "GetDeviceData mouse=%d keyboard=%d requested=%lu real=%lu flags=0x%lx shared=%p valid=%d active=%lu menu=%lu gameplay=%lu gameplayFlags=0x%lx aimTrigger=%lu client=%ld,%ld mouseClick=%lu keyboardAccept=%lu ls=%ld,%ld rs=%ld,%ld head=%lu,%ld,%ld grip=%ld,%ld\n",
                m_mouse ? 1 : 0,
                m_keyboard ? 1 : 0,
                requested,
                *count,
                flags,
                shared,
                validShared(shared) ? 1 : 0,
                shared ? shared->pointerActive : 0,
                shared ? shared->menuInputActive : 0,
                shared ? shared->gameplayControlsActive : 0,
                shared ? static_cast<unsigned long>(shared->gameplayFlags) : 0,
                shared ? shared->aimTrigger : 0,
                shared ? shared->clientX : 0,
                shared ? shared->clientY : 0,
                shared ? shared->mouseClickPacket : 0,
                shared ? shared->keyboardAcceptPacket : 0,
                shared ? static_cast<LONG>(shared->leftStickX) : 0,
                shared ? static_cast<LONG>(shared->leftStickY) : 0,
                shared ? static_cast<LONG>(shared->rightStickX) : 0,
                shared ? static_cast<LONG>(shared->rightStickY) : 0,
                shared ? shared->headLookActive : 0,
                shared ? static_cast<LONG>(shared->headLookX) : 0,
                shared ? static_cast<LONG>(shared->headLookY) : 0,
                shared ? static_cast<LONG>(shared->leftGrip) : 0,
                shared ? static_cast<LONG>(shared->rightGrip) : 0);
        }
        if (m_mouse)
            appendMouseEvents(shared, outData, requested, written);
        if (m_keyboard)
            appendKeyboardEvents(shared, outData, requested, written);
        *count = written;
        return hr;
    }

    HRESULT STDMETHODCALLTYPE SetDataFormat(LPCDIDATAFORMAT value) override { return m_real->SetDataFormat(value); }
    HRESULT STDMETHODCALLTYPE SetEventNotification(HANDLE value) override { return m_real->SetEventNotification(value); }
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND hwnd, DWORD flags) override
    {
        DWORD patchedFlags = flags;
        if ((m_mouse || m_keyboard) && envEnabled("FNVXR_DINPUT_FORCE_BACKGROUND", true))
        {
            patchedFlags &= ~DISCL_FOREGROUND;
            patchedFlags |= DISCL_BACKGROUND;
        }

        DWORD appliedFlags = patchedFlags;
        HRESULT hr = m_real->SetCooperativeLevel(hwnd, patchedFlags);
        if (FAILED(hr) && patchedFlags != flags && (patchedFlags & DISCL_NOWINKEY) != 0)
        {
            const HRESULT firstHr = hr;
            const DWORD retryFlags = patchedFlags & ~DISCL_NOWINKEY;
            hr = m_real->SetCooperativeLevel(hwnd, retryFlags);
            if (SUCCEEDED(hr))
            {
                appliedFlags = retryFlags;
                logLine(
                    "SetCooperativeLevel retry without NOWINKEY mouse=%d keyboard=%d flags=0x%lx retry=0x%lx firstHr=0x%08lx hr=0x%08lx\n",
                    m_mouse ? 1 : 0,
                    m_keyboard ? 1 : 0,
                    flags,
                    retryFlags,
                    static_cast<unsigned long>(firstHr),
                    static_cast<unsigned long>(hr));
            }
        }
        if (FAILED(hr) && patchedFlags != flags)
        {
            const HRESULT patchedHr = hr;
            hr = m_real->SetCooperativeLevel(hwnd, flags);
            appliedFlags = flags;
            logLine(
                "SetCooperativeLevel fallback original mouse=%d keyboard=%d flags=0x%lx patched=0x%lx patchedHr=0x%08lx hr=0x%08lx\n",
                m_mouse ? 1 : 0,
                m_keyboard ? 1 : 0,
                flags,
                patchedFlags,
                static_cast<unsigned long>(patchedHr),
                static_cast<unsigned long>(hr));
        }
        if (!m_loggedCooperativeLevel)
        {
            m_loggedCooperativeLevel = true;
            logLine(
                "SetCooperativeLevel mouse=%d keyboard=%d hwnd=%p flags=0x%lx patched=0x%lx applied=0x%lx hr=0x%08lx\n",
                m_mouse ? 1 : 0,
                m_keyboard ? 1 : 0,
                hwnd,
                flags,
                patchedFlags,
                appliedFlags,
                static_cast<unsigned long>(hr));
        }
        return hr;
    }
    HRESULT STDMETHODCALLTYPE GetObjectInfo(LPDIDEVICEOBJECTINSTANCEA value, DWORD object, DWORD how) override { return m_real->GetObjectInfo(value, object, how); }
    HRESULT STDMETHODCALLTYPE GetDeviceInfo(LPDIDEVICEINSTANCEA value) override { return m_real->GetDeviceInfo(value); }
    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND owner, DWORD flags) override { return m_real->RunControlPanel(owner, flags); }
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE instance, DWORD version, REFGUID guid) override { return m_real->Initialize(instance, version, guid); }
    HRESULT STDMETHODCALLTYPE CreateEffect(REFGUID guid, LPCDIEFFECT effect, LPDIRECTINPUTEFFECT* out, LPUNKNOWN outer) override { return m_real->CreateEffect(guid, effect, out, outer); }
    HRESULT STDMETHODCALLTYPE EnumEffects(LPDIENUMEFFECTSCALLBACKA cb, LPVOID ref, DWORD type) override { return m_real->EnumEffects(cb, ref, type); }
    HRESULT STDMETHODCALLTYPE GetEffectInfo(LPDIEFFECTINFOA value, REFGUID guid) override { return m_real->GetEffectInfo(value, guid); }
    HRESULT STDMETHODCALLTYPE GetForceFeedbackState(LPDWORD out) override { return m_real->GetForceFeedbackState(out); }
    HRESULT STDMETHODCALLTYPE SendForceFeedbackCommand(DWORD flags) override { return m_real->SendForceFeedbackCommand(flags); }
    HRESULT STDMETHODCALLTYPE EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK cb, LPVOID ref, DWORD flags) override { return m_real->EnumCreatedEffectObjects(cb, ref, flags); }
    HRESULT STDMETHODCALLTYPE Escape(LPDIEFFESCAPE value) override { return m_real->Escape(value); }
    HRESULT STDMETHODCALLTYPE Poll() override { return m_real->Poll(); }
    HRESULT STDMETHODCALLTYPE SendDeviceData(DWORD dataSize, LPCDIDEVICEOBJECTDATA data, LPDWORD count, DWORD flags) override { return m_real->SendDeviceData(dataSize, data, count, flags); }
    HRESULT STDMETHODCALLTYPE EnumEffectsInFile(LPCSTR file, LPDIENUMEFFECTSINFILECALLBACK cb, LPVOID ref, DWORD flags) override { return m_real->EnumEffectsInFile(file, cb, ref, flags); }
    HRESULT STDMETHODCALLTYPE WriteEffectToFile(LPCSTR file, DWORD entries, LPDIFILEEFFECT effects, DWORD flags) override { return m_real->WriteEffectToFile(file, entries, effects, flags); }
    HRESULT STDMETHODCALLTYPE BuildActionMap(LPDIACTIONFORMATA action, LPCSTR user, DWORD flags) override { return m_real->BuildActionMap(action, user, flags); }
    HRESULT STDMETHODCALLTYPE SetActionMap(LPDIACTIONFORMATA action, LPCSTR user, DWORD flags) override { return m_real->SetActionMap(action, user, flags); }
    HRESULT STDMETHODCALLTYPE GetImageInfo(LPDIDEVICEIMAGEINFOHEADERA value) override { return m_real->GetImageInfo(value); }

private:
    void commitGameplayLookSample(const SharedDInputState* shared)
    {
        if (!validShared(shared))
            return;
        m_lastLookFrame = shared->frame;
        m_lastHeadLookX = shared->headLookX;
        m_lastHeadLookY = shared->headLookY;
        m_lastGyroLookX = shared->gyroLookX;
        m_lastGyroLookY = shared->gyroLookY;
        m_lastLookModeSignature = gameplayLookModeSignature(shared);
        m_haveLookSample = true;
    }

    bool prepareGameplayLookSample(
        const SharedDInputState* shared,
        SharedDInputState& sample)
    {
        if (!validShared(shared))
            return false;

        if (!m_haveLookSample)
        {
            commitGameplayLookSample(shared);
            return false;
        }
        if (shared->frame == m_lastLookFrame)
            return false;

        // The cumulative hand-look counters intentionally carry either
        // precision gyro or hand-space motion. Rebaseline at a source/scale
        // transition so skipped producer frames cannot be replayed using the
        // new mode's sensitivity.
        if (gameplayLookModeSignature(shared) != m_lastLookModeSignature)
        {
            commitGameplayLookSample(shared);
            return false;
        }

        sample = *shared;
        sample.headLookX = fnvxr::shared::wrappedInt32Delta(shared->headLookX, m_lastHeadLookX);
        sample.headLookY = fnvxr::shared::wrappedInt32Delta(shared->headLookY, m_lastHeadLookY);
        sample.gyroLookX = fnvxr::shared::wrappedInt32Delta(shared->gyroLookX, m_lastGyroLookX);
        sample.gyroLookY = fnvxr::shared::wrappedInt32Delta(shared->gyroLookY, m_lastGyroLookY);
        if (!gameplayControlsActive(&sample))
        {
            commitGameplayLookSample(shared);
            return false;
        }
        return true;
    }

    void injectMouseStateFields(const SharedDInputState* shared, LONG& lX, LONG& lY, LONG& lZ, BYTE* buttons)
    {
        consumeInputEventsForMouseState(lX, lY, lZ, buttons);

        if (!validPointer(shared))
        {
            m_hasLastPointer = false;
        }
        else
        {
            const POINT mapped = mapSharedPointerToInput(shared);
            if (!m_hasLastPointer)
            {
                m_lastX = mapped.x;
                m_lastY = mapped.y;
                m_hasLastPointer = true;
            }
            else
            {
                const LONG dx = mapped.x - m_lastX;
                const LONG dy = mapped.y - m_lastY;
                m_lastX = mapped.x;
                m_lastY = mapped.y;
                lX += dx;
                lY += dy;
                if ((dx || dy) && !m_loggedMouseMove)
                {
                    m_loggedMouseMove = true;
                    logLine(
                        "inject state mouse dx=%ld dy=%ld raw=%ld,%ld mapped=%ld,%ld\n",
                        dx,
                        dy,
                        shared->clientX,
                        shared->clientY,
                        mapped.x,
                        mapped.y);
                }
            }
        }

        if (validShared(shared) && shared->mouseClickPacket != m_lastMouseClickPacket)
        {
            m_lastMouseClickPacket = shared->mouseClickPacket;
            buttons[0] = 0x80;
            logLine("inject state mouse button0 packet=%lu\n", shared->mouseClickPacket);
        }

        SharedDInputState lookSample {};
        if (prepareGameplayLookSample(shared, lookSample))
        {
            const SharedDInputState* look = &lookSample;
            LONG lookX = 0;
            LONG lookY = 0;
            LONG stickX = 0;
            LONG stickY = 0;
            LONG headX = 0;
            LONG headY = 0;
            LONG gyroX = 0;
            LONG gyroY = 0;
            bool aimHeld = false;
            float headScale = 0.0f;
            float gyroScale = 0.0f;
            computeGameplayLookDeltas(look, lookX, lookY, stickX, stickY, headX, headY, gyroX, gyroY, aimHeld, headScale, gyroScale);
            lX += lookX;
            lY += lookY;
            if ((lookX || lookY) && !m_loggedGameplayMouseLook)
            {
                m_loggedGameplayMouseLook = true;
                logLine(
                    "inject state gameplay mouse-look dx=%ld dy=%ld stick=%ld,%ld head=%ld,%ld gyro=%ld,%ld aim=%d aimTrigger=%lu headScale=%ld gyroScale=%ld flags=0x%lx rs=%ld,%ld headDelta=%lu,%ld,%ld gyroDelta=%lu,%ld,%ld active=%lu menu=%lu\n",
                    lookX,
                    lookY,
                    stickX,
                    stickY,
                    headX,
                    headY,
                    gyroX,
                    gyroY,
                    aimHeld ? 1 : 0,
                    look->aimTrigger,
                    static_cast<LONG>(std::lround(headScale)),
                    static_cast<LONG>(std::lround(gyroScale)),
                    static_cast<unsigned long>(look->gameplayFlags),
                    static_cast<LONG>(look->rightStickX),
                    static_cast<LONG>(look->rightStickY),
                    look->headLookActive,
                    static_cast<LONG>(look->headLookX),
                    static_cast<LONG>(look->headLookY),
                    look->gyroLookActive,
                    static_cast<LONG>(look->gyroLookX),
                    static_cast<LONG>(look->gyroLookY),
                    look->gameplayControlsActive,
                    look->menuInputActive);
            }
            commitGameplayLookSample(shared);
        }
    }

    void injectMouseState(const SharedDInputState* shared, DIMOUSESTATE* mouse)
    {
        injectMouseStateFields(shared, mouse->lX, mouse->lY, mouse->lZ, mouse->rgbButtons);
    }

    void injectMouseState2(const SharedDInputState* shared, DIMOUSESTATE2* mouse)
    {
        injectMouseStateFields(shared, mouse->lX, mouse->lY, mouse->lZ, mouse->rgbButtons);
    }

    void injectKeyboardState(const SharedDInputState* shared, BYTE* keys)
    {
        consumeInputEventsForKeyboardState(keys);

        if (!validShared(shared))
            return;

        injectGameplayKeyboardState(shared, keys);

        if (shared->keyboardAcceptPacket != m_lastKeyboardAcceptPacket)
        {
            m_lastKeyboardAcceptPacket = shared->keyboardAcceptPacket;
            keys[DIK_RETURN] = 0x80;
            logLine("inject state keyboard accept packet=%lu\n", shared->keyboardAcceptPacket);
        }
    }

    void injectGameplayKeyboardState(const SharedDInputState* shared, BYTE* keys)
    {
        if (!gameplayControlsActive(shared) || !keyboardMovementEnabled())
            return;

        const LONG deadzone = envLong("FNVXR_DINPUT_MOVE_DEADZONE", 6500);
        const LONG forwardAxis = forwardAxisValue(shared);
        const LONG effectiveY = effectiveLeftStickY(shared);
        const bool autoRun = xinputAutoRunEnabled();
        const int forwardTier = forwardMoveTier(shared);
        const bool forward = forwardTier > 0 && (forwardTier != 1 || slowMovePulseActive());
        const bool backward = forwardAxis < -deadzone;
        const bool right = shared->leftStickX > deadzone;
        const bool left = shared->leftStickX < -deadzone;
        const DWORD walkModifier = movementModifierDik("FNVXR_DINPUT_WALK_MODIFIER_DIK", 0);
        const DWORD runModifier = movementModifierDik("FNVXR_DINPUT_RUN_MODIFIER_DIK", 0);
        if (forward)
            keys[DIK_W] = 0x80;
        if (backward)
            keys[DIK_S] = 0x80;
        if (left)
            keys[DIK_A] = 0x80;
        if (right)
            keys[DIK_D] = 0x80;
        if (forwardTier == 1 && walkModifier)
            keys[walkModifier] = 0x80;
        if (forwardTier == 3 && runModifier)
            keys[runModifier] = 0x80;
        if ((forward || backward || left || right || forwardTier != m_lastStateMoveTier) && forwardTier != m_lastStateMoveTier)
        {
            m_lastStateMoveTier = forwardTier;
            logLine(
                "inject state gameplay keyboard wasd=%d%d%d%d moveTier=%d walkMod=0x%02lx runMod=0x%02lx ls=%ld,%ld effectiveY=%ld autoRun=%d forwardAxis=%ld active=%lu menu=%lu\n",
                forward ? 1 : 0,
                backward ? 1 : 0,
                left ? 1 : 0,
                right ? 1 : 0,
                forwardTier,
                static_cast<unsigned long>((forwardTier == 1 && walkModifier) ? walkModifier : 0),
                static_cast<unsigned long>((forwardTier == 3 && runModifier) ? runModifier : 0),
                static_cast<LONG>(shared->leftStickX),
                static_cast<LONG>(shared->leftStickY),
                effectiveY,
                autoRun ? 1 : 0,
                forwardAxis,
                shared->gameplayControlsActive,
                shared->menuInputActive);
        }
    }

    void appendEvent(LPDIDEVICEOBJECTDATA outData, DWORD requested, DWORD& written, DWORD offset, DWORD value)
    {
        if (written >= requested)
            return;

        DIDEVICEOBJECTDATA& event = outData[written++];
        std::memset(&event, 0, sizeof(event));
        event.dwOfs = offset;
        event.dwData = value;
        event.dwTimeStamp = GetTickCount();
        event.uAppData = static_cast<ULONG_PTR>(-1);
    }

    void consumeInputEventsForKeyboardState(BYTE* keys)
    {
        if (!keys)
            return;

        const SharedInputEventQueue* queue = inputEventQueue();
        if (!validInputEvents(queue))
            return;

        const LONG writeSequence = queue->writeSequence;
        for (LONG sequence = inputEventStartSequence(m_lastInputEventSequence, writeSequence);
             sequence <= writeSequence;
             ++sequence)
        {
            SharedInputEvent event {};
            if (!readInputEvent(queue, sequence, event) || event.code >= 256)
                continue;

            switch (event.type)
            {
                case fnvxr::shared::InputEventTypeKeyDown:
                    m_forcedInputKeys[event.code] = true;
                    logInputEventOnce("state key down", event);
                    break;
                case fnvxr::shared::InputEventTypeKeyUp:
                    m_forcedInputKeys[event.code] = false;
                    logInputEventOnce("state key up", event);
                    break;
                case fnvxr::shared::InputEventTypeKeyTap:
                    keys[event.code] = 0x80;
                    logInputEventOnce("state key tap", event);
                    break;
                default:
                    break;
            }
        }
        m_lastInputEventSequence = writeSequence;

        for (std::uint32_t code = 0; code < 256; ++code)
        {
            if (m_forcedInputKeys[code])
                keys[code] = 0x80;
        }
    }

    void consumeInputEventsForMouseState(LONG& lX, LONG& lY, LONG& lZ, BYTE* buttons)
    {
        if (!buttons)
            return;

        const SharedInputEventQueue* queue = inputEventQueue();
        if (!validInputEvents(queue))
            return;

        const LONG writeSequence = queue->writeSequence;
        for (LONG sequence = inputEventStartSequence(m_lastInputEventSequence, writeSequence);
             sequence <= writeSequence;
             ++sequence)
        {
            SharedInputEvent event {};
            if (!readInputEvent(queue, sequence, event))
                continue;

            switch (event.type)
            {
                case fnvxr::shared::InputEventTypeMouseButtonDown:
                    if (event.code < 8)
                    {
                        m_forcedInputMouseButtons[event.code] = true;
                        logInputEventOnce("state mouse down", event);
                    }
                    break;
                case fnvxr::shared::InputEventTypeMouseButtonUp:
                    if (event.code < 8)
                    {
                        m_forcedInputMouseButtons[event.code] = false;
                        logInputEventOnce("state mouse up", event);
                    }
                    break;
                case fnvxr::shared::InputEventTypeMouseButtonTap:
                    if (event.code < 8)
                    {
                        buttons[event.code] = 0x80;
                        logInputEventOnce("state mouse tap", event);
                    }
                    break;
                case fnvxr::shared::InputEventTypeMouseMove:
                    lX += event.value0;
                    lY += event.value1;
                    logInputEventOnce("state mouse move", event);
                    break;
                case fnvxr::shared::InputEventTypeMouseWheel:
                    lZ += event.value0;
                    logInputEventOnce("state mouse wheel", event);
                    break;
                default:
                    break;
            }
        }
        m_lastInputEventSequence = writeSequence;

        for (std::uint32_t button = 0; button < 8; ++button)
        {
            if (m_forcedInputMouseButtons[button])
                buttons[button] = 0x80;
        }
    }

    void appendInputQueueKeyboardEvents(LPDIDEVICEOBJECTDATA outData, DWORD requested, DWORD& written)
    {
        for (std::uint32_t code = 0; code < 256; ++code)
        {
            if (m_pendingBufferedInputKeyRelease[code])
            {
                const DWORD before = written;
                appendEvent(outData, requested, written, code, 0x00);
                if (written > before)
                    m_pendingBufferedInputKeyRelease[code] = false;
                else
                    return;
            }
        }

        const SharedInputEventQueue* queue = inputEventQueue();
        if (!validInputEvents(queue))
            return;

        const LONG writeSequence = queue->writeSequence;
        LONG lastRepresentedSequence = m_lastInputEventSequence;
        for (LONG sequence = inputEventStartSequence(m_lastInputEventSequence, writeSequence);
             sequence <= writeSequence;
             ++sequence)
        {
            SharedInputEvent event {};
            if (!readInputEvent(queue, sequence, event) || event.code >= 256)
            {
                lastRepresentedSequence = sequence;
                continue;
            }

            if (event.type == fnvxr::shared::InputEventTypeKeyDown)
            {
                if (written >= requested)
                    break;
                appendEvent(outData, requested, written, event.code, 0x80);
                m_forcedInputKeys[event.code] = true;
                logInputEventOnce("buffered key down", event);
            }
            else if (event.type == fnvxr::shared::InputEventTypeKeyUp)
            {
                if (written >= requested)
                    break;
                appendEvent(outData, requested, written, event.code, 0x00);
                m_forcedInputKeys[event.code] = false;
                logInputEventOnce("buffered key up", event);
            }
            else if (event.type == fnvxr::shared::InputEventTypeKeyTap)
            {
                if (written >= requested)
                    break;
                const DWORD before = written;
                appendEvent(outData, requested, written, event.code, 0x80);
                if (written > before)
                {
                    if (written < requested)
                        appendEvent(outData, requested, written, event.code, 0x00);
                    else
                        m_pendingBufferedInputKeyRelease[event.code] = true;
                    logInputEventOnce("buffered key tap", event);
                }
            }
            lastRepresentedSequence = sequence;
        }
        m_lastInputEventSequence = lastRepresentedSequence;
    }

    void appendInputQueueMouseEvents(LPDIDEVICEOBJECTDATA outData, DWORD requested, DWORD& written)
    {
        for (std::uint32_t button = 0; button < 8; ++button)
        {
            if (m_pendingBufferedInputMouseRelease[button])
            {
                const DWORD before = written;
                appendEvent(outData, requested, written, DIMOFS_BUTTON0 + button, 0x00);
                if (written > before)
                    m_pendingBufferedInputMouseRelease[button] = false;
                else
                    return;
            }
        }

        const SharedInputEventQueue* queue = inputEventQueue();
        if (!validInputEvents(queue))
            return;

        const LONG writeSequence = queue->writeSequence;
        LONG lastRepresentedSequence = m_lastInputEventSequence;
        for (LONG sequence = inputEventStartSequence(m_lastInputEventSequence, writeSequence);
             sequence <= writeSequence;
             ++sequence)
        {
            SharedInputEvent event {};
            if (!readInputEvent(queue, sequence, event))
            {
                lastRepresentedSequence = sequence;
                continue;
            }

            if (event.type == fnvxr::shared::InputEventTypeMouseButtonDown && event.code < 8)
            {
                if (written >= requested)
                    break;
                appendEvent(outData, requested, written, DIMOFS_BUTTON0 + event.code, 0x80);
                m_forcedInputMouseButtons[event.code] = true;
                logInputEventOnce("buffered mouse down", event);
            }
            else if (event.type == fnvxr::shared::InputEventTypeMouseButtonUp && event.code < 8)
            {
                if (written >= requested)
                    break;
                appendEvent(outData, requested, written, DIMOFS_BUTTON0 + event.code, 0x00);
                m_forcedInputMouseButtons[event.code] = false;
                logInputEventOnce("buffered mouse up", event);
            }
            else if (event.type == fnvxr::shared::InputEventTypeMouseButtonTap && event.code < 8)
            {
                if (written >= requested)
                    break;
                const DWORD before = written;
                appendEvent(outData, requested, written, DIMOFS_BUTTON0 + event.code, 0x80);
                if (written > before)
                {
                    if (written < requested)
                        appendEvent(outData, requested, written, DIMOFS_BUTTON0 + event.code, 0x00);
                    else
                        m_pendingBufferedInputMouseRelease[event.code] = true;
                    logInputEventOnce("buffered mouse tap", event);
                }
            }
            else if (event.type == fnvxr::shared::InputEventTypeMouseMove)
            {
                const DWORD requiredSlots = (event.value0 != 0 ? 1u : 0u) + (event.value1 != 0 ? 1u : 0u);
                if (requiredSlots > requested - written)
                    break;
                if (event.value0)
                    appendEvent(outData, requested, written, DIMOFS_X, static_cast<DWORD>(event.value0));
                if (event.value1)
                    appendEvent(outData, requested, written, DIMOFS_Y, static_cast<DWORD>(event.value1));
                logInputEventOnce("buffered mouse move", event);
            }
            else if (event.type == fnvxr::shared::InputEventTypeMouseWheel)
            {
                if (written >= requested)
                    break;
                appendEvent(outData, requested, written, DIMOFS_Z, static_cast<DWORD>(event.value0));
                logInputEventOnce("buffered mouse wheel", event);
            }
            lastRepresentedSequence = sequence;
        }
        m_lastInputEventSequence = lastRepresentedSequence;
    }

    void logInputEventOnce(const char* label, const SharedInputEvent& event)
    {
        if (m_loggedInputEvents >= 64)
            return;
        ++m_loggedInputEvents;
        logLine(
            "input event %s seq=%lu type=%lu code=%lu value=%ld,%ld flags=0x%lx frame=%llu\n",
            label,
            static_cast<unsigned long>(event.sequence),
            static_cast<unsigned long>(event.type),
            static_cast<unsigned long>(event.code),
            static_cast<LONG>(event.value0),
            static_cast<LONG>(event.value1),
            static_cast<unsigned long>(event.flags),
            static_cast<unsigned long long>(event.frame));
    }

    void appendMouseEvents(const SharedDInputState* shared, LPDIDEVICEOBJECTDATA outData, DWORD requested, DWORD& written)
    {
        appendInputQueueMouseEvents(outData, requested, written);

        if (m_pendingBufferedMouseButton0Release)
        {
            const DWORD beforeRelease = written;
            appendEvent(outData, requested, written, DIMOFS_BUTTON0, 0x00);
            if (written > beforeRelease)
                m_pendingBufferedMouseButton0Release = false;
            else
                return;
        }

        if (!validPointer(shared))
        {
            m_hasLastPointer = false;
        }
        else
        {
            const POINT mapped = mapSharedPointerToInput(shared);
            if (!m_hasLastPointer)
            {
                m_lastX = mapped.x;
                m_lastY = mapped.y;
                m_hasLastPointer = true;
            }

            const LONG dx = mapped.x - m_lastX;
            const LONG dy = mapped.y - m_lastY;
            const DWORD requiredPointerSlots = (dx != 0 ? 1u : 0u) + (dy != 0 ? 1u : 0u);
            if (requiredPointerSlots <= requested - written)
            {
                m_lastX = mapped.x;
                m_lastY = mapped.y;
                if (dx)
                {
                    appendEvent(outData, requested, written, DIMOFS_X, static_cast<DWORD>(dx));
                    if (!m_loggedBufferedMove)
                    {
                        m_loggedBufferedMove = true;
                        logLine(
                            "inject buffered mouse dx=%ld dy=%ld raw=%ld,%ld mapped=%ld,%ld\n",
                            dx,
                            dy,
                            shared->clientX,
                            shared->clientY,
                            mapped.x,
                            mapped.y);
                    }
                }
                if (dy)
                    appendEvent(outData, requested, written, DIMOFS_Y, static_cast<DWORD>(dy));
            }
        }

        if (validShared(shared) && shared->mouseClickPacket != m_lastMouseClickPacket)
        {
            if (written < requested)
            {
                appendEvent(outData, requested, written, DIMOFS_BUTTON0, 0x80);
                m_lastMouseClickPacket = shared->mouseClickPacket;
                m_pendingBufferedMouseButton0Release = true;
                if (written < requested)
                {
                    appendEvent(outData, requested, written, DIMOFS_BUTTON0, 0x00);
                    m_pendingBufferedMouseButton0Release = false;
                }
                logLine("inject buffered mouse button0 packet=%lu pendingRelease=%d\n", shared->mouseClickPacket, m_pendingBufferedMouseButton0Release ? 1 : 0);
            }
        }

        SharedDInputState lookSample {};
        if (prepareGameplayLookSample(shared, lookSample))
        {
            const SharedDInputState* look = &lookSample;
            LONG lookX = 0;
            LONG lookY = 0;
            LONG stickX = 0;
            LONG stickY = 0;
            LONG headX = 0;
            LONG headY = 0;
            LONG gyroX = 0;
            LONG gyroY = 0;
            bool aimHeld = false;
            float headScale = 0.0f;
            float gyroScale = 0.0f;
            computeGameplayLookDeltas(look, lookX, lookY, stickX, stickY, headX, headY, gyroX, gyroY, aimHeld, headScale, gyroScale);
            const DWORD requiredLookSlots = (lookX != 0 ? 1u : 0u) + (lookY != 0 ? 1u : 0u);
            const bool delivered = requiredLookSlots <= requested - written;
            if (delivered)
            {
                if (lookX)
                    appendEvent(outData, requested, written, DIMOFS_X, static_cast<DWORD>(lookX));
                if (lookY)
                    appendEvent(outData, requested, written, DIMOFS_Y, static_cast<DWORD>(lookY));
                commitGameplayLookSample(shared);
                if ((lookX || lookY) && !m_loggedGameplayBufferedMouseLook)
                {
                    m_loggedGameplayBufferedMouseLook = true;
                    logLine(
                        "inject buffered gameplay mouse-look dx=%ld dy=%ld stick=%ld,%ld head=%ld,%ld gyro=%ld,%ld aim=%d aimTrigger=%lu headScale=%ld gyroScale=%ld flags=0x%lx rs=%ld,%ld headDelta=%lu,%ld,%ld gyroDelta=%lu,%ld,%ld active=%lu menu=%lu\n",
                        lookX,
                        lookY,
                        stickX,
                        stickY,
                        headX,
                        headY,
                        gyroX,
                        gyroY,
                        aimHeld ? 1 : 0,
                        look->aimTrigger,
                        static_cast<LONG>(std::lround(headScale)),
                        static_cast<LONG>(std::lround(gyroScale)),
                        static_cast<unsigned long>(look->gameplayFlags),
                        static_cast<LONG>(look->rightStickX),
                        static_cast<LONG>(look->rightStickY),
                        look->headLookActive,
                        static_cast<LONG>(look->headLookX),
                        static_cast<LONG>(look->headLookY),
                        look->gyroLookActive,
                        static_cast<LONG>(look->gyroLookX),
                        static_cast<LONG>(look->gyroLookY),
                        look->gameplayControlsActive,
                        look->menuInputActive);
                }
            }
        }
    }

    void appendKeyboardEvents(const SharedDInputState* shared, LPDIDEVICEOBJECTDATA outData, DWORD requested, DWORD& written)
    {
        appendInputQueueKeyboardEvents(outData, requested, written);
        appendGameplayKeyboardEvents(shared, outData, requested, written);

        if (m_pendingBufferedKeyboardAcceptRelease)
        {
            const DWORD beforeRelease = written;
            appendEvent(outData, requested, written, DIK_RETURN, 0x00);
            if (written > beforeRelease)
                m_pendingBufferedKeyboardAcceptRelease = false;
            else
                return;
        }

        if (!validShared(shared))
            return;

        if (shared->keyboardAcceptPacket != m_lastKeyboardAcceptPacket)
        {
            if (written < requested)
            {
                appendEvent(outData, requested, written, DIK_RETURN, 0x80);
                m_lastKeyboardAcceptPacket = shared->keyboardAcceptPacket;
                m_pendingBufferedKeyboardAcceptRelease = true;
                if (written < requested)
                {
                    appendEvent(outData, requested, written, DIK_RETURN, 0x00);
                    m_pendingBufferedKeyboardAcceptRelease = false;
                }
                logLine("inject buffered keyboard accept packet=%lu pendingRelease=%d\n", shared->keyboardAcceptPacket, m_pendingBufferedKeyboardAcceptRelease ? 1 : 0);
            }
        }
    }

    void appendGameplayKeyEvent(
        LPDIDEVICEOBJECTDATA outData,
        DWORD requested,
        DWORD& written,
        DWORD offset,
        bool pressed,
        bool& previous,
        bool& pendingRelease)
    {
        if (pendingRelease)
        {
            const DWORD beforeRelease = written;
            appendEvent(outData, requested, written, offset, 0x00);
            if (written > beforeRelease)
                pendingRelease = false;
            else
                return;
        }
        if (pressed == previous)
            return;

        if (written >= requested)
            return;
        appendEvent(outData, requested, written, offset, pressed ? 0x80 : 0x00);
        previous = pressed;
        if (pressed)
            pendingRelease = false;
    }

    void appendGameplayKeyboardEvents(const SharedDInputState* shared, LPDIDEVICEOBJECTDATA outData, DWORD requested, DWORD& written)
    {
        const LONG deadzone = envLong("FNVXR_DINPUT_MOVE_DEADZONE", 6500);
        const bool active = gameplayControlsActive(shared) && keyboardMovementEnabled();
        const LONG forwardAxis = active ? forwardAxisValue(shared) : 0;
        const LONG effectiveY = active ? effectiveLeftStickY(shared) : 0;
        const bool autoRun = active && xinputAutoRunEnabled();
        const int forwardTier = active ? forwardMoveTier(shared) : 0;
        const bool forward = active && forwardTier > 0 && (forwardTier != 1 || slowMovePulseActive());
        const bool backward = active && forwardAxis < -deadzone;
        const bool right = active && shared->leftStickX > deadzone;
        const bool left = active && shared->leftStickX < -deadzone;
        const DWORD walkModifier = movementModifierDik("FNVXR_DINPUT_WALK_MODIFIER_DIK", 0);
        const DWORD runModifier = movementModifierDik("FNVXR_DINPUT_RUN_MODIFIER_DIK", 0);
        const bool walkModifierDown = active && forwardTier == 1 && walkModifier != 0;
        const bool runModifierDown = active && forwardTier == 3 && runModifier != 0;

        appendGameplayKeyEvent(outData, requested, written, DIK_W, forward, m_bufferedForwardDown, m_pendingBufferedForwardRelease);
        appendGameplayKeyEvent(outData, requested, written, DIK_S, backward, m_bufferedBackwardDown, m_pendingBufferedBackwardRelease);
        appendGameplayKeyEvent(outData, requested, written, DIK_A, left, m_bufferedLeftDown, m_pendingBufferedLeftRelease);
        appendGameplayKeyEvent(outData, requested, written, DIK_D, right, m_bufferedRightDown, m_pendingBufferedRightRelease);
        if (walkModifier)
            appendGameplayKeyEvent(outData, requested, written, walkModifier, walkModifierDown, m_bufferedWalkModifierDown, m_pendingBufferedWalkModifierRelease);
        if (runModifier)
            appendGameplayKeyEvent(outData, requested, written, runModifier, runModifierDown, m_bufferedRunModifierDown, m_pendingBufferedRunModifierRelease);

        if (forwardTier != m_lastBufferedMoveTier)
        {
            m_lastBufferedMoveTier = forwardTier;
            logLine(
                "inject buffered gameplay keyboard wasd=%d%d%d%d moveTier=%d walkMod=0x%02lx runMod=0x%02lx ls=%ld,%ld effectiveY=%ld autoRun=%d forwardAxis=%ld active=%lu menu=%lu\n",
                forward ? 1 : 0,
                backward ? 1 : 0,
                left ? 1 : 0,
                right ? 1 : 0,
                forwardTier,
                static_cast<unsigned long>(walkModifierDown ? walkModifier : 0),
                static_cast<unsigned long>(runModifierDown ? runModifier : 0),
                static_cast<LONG>(shared ? shared->leftStickX : 0),
                static_cast<LONG>(shared ? shared->leftStickY : 0),
                effectiveY,
                autoRun ? 1 : 0,
                forwardAxis,
                shared ? shared->gameplayControlsActive : 0,
                shared ? shared->menuInputActive : 0);
        }
    }

    IDirectInputDevice8A* m_real = nullptr;
    LONG m_refs = 1;
    bool m_mouse = false;
    bool m_keyboard = false;
    bool m_hasLastPointer = false;
    LONG m_lastX = 0;
    LONG m_lastY = 0;
    std::uint32_t m_lastMouseClickPacket = 0;
    std::uint32_t m_lastKeyboardAcceptPacket = 0;
    bool m_haveLookSample = false;
    std::uint32_t m_lastLookFrame = 0;
    std::int32_t m_lastHeadLookX = 0;
    std::int32_t m_lastHeadLookY = 0;
    std::int32_t m_lastGyroLookX = 0;
    std::int32_t m_lastGyroLookY = 0;
    std::uint32_t m_lastLookModeSignature = 0;
    bool m_loggedMouseMove = false;
    bool m_loggedBufferedMove = false;
    bool m_loggedGameplayMouseLook = false;
    bool m_loggedGameplayBufferedMouseLook = false;
    int m_lastStateMoveTier = -1;
    int m_lastBufferedMoveTier = -1;
    bool m_loggedMouseStatePoll = false;
    bool m_loggedKeyboardStatePoll = false;
    bool m_loggedBufferedPoll = false;
    bool m_loggedCooperativeLevel = false;
    std::uint32_t m_loggedInputEvents = 0;
    LONG m_lastInputEventSequence = 0;
    bool m_forcedInputKeys[256] {};
    bool m_pendingBufferedInputKeyRelease[256] {};
    bool m_forcedInputMouseButtons[8] {};
    bool m_pendingBufferedInputMouseRelease[8] {};
    bool m_pendingBufferedMouseButton0Release = false;
    bool m_pendingBufferedKeyboardAcceptRelease = false;
    bool m_bufferedForwardDown = false;
    bool m_bufferedBackwardDown = false;
    bool m_bufferedLeftDown = false;
    bool m_bufferedRightDown = false;
    bool m_bufferedWalkModifierDown = false;
    bool m_bufferedRunModifierDown = false;
    bool m_pendingBufferedForwardRelease = false;
    bool m_pendingBufferedBackwardRelease = false;
    bool m_pendingBufferedLeftRelease = false;
    bool m_pendingBufferedRightRelease = false;
    bool m_pendingBufferedWalkModifierRelease = false;
    bool m_pendingBufferedRunModifierRelease = false;
};

class WrappedDirectInput8A final : public IDirectInput8A
{
public:
    explicit WrappedDirectInput8A(IDirectInput8A* realInput)
        : m_real(realInput)
    {
        logLine("wrap DirectInput8A real=%p\n", realInput);
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID* out) override
    {
        if (!out)
            return E_POINTER;
        if (IsEqualGUID(riid, IID_IUnknown) || IsEqualGUID(riid, IID_IDirectInput8A))
        {
            *out = static_cast<IDirectInput8A*>(this);
            AddRef();
            return S_OK;
        }
        return m_real->QueryInterface(riid, out);
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_refs); }
    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG refs = InterlockedDecrement(&m_refs);
        if (!refs)
        {
            m_real->Release();
            delete this;
        }
        return refs;
    }

    HRESULT STDMETHODCALLTYPE CreateDevice(REFGUID guid, LPDIRECTINPUTDEVICE8A* device, LPUNKNOWN outer) override
    {
        IDirectInputDevice8A* realDevice = nullptr;
        HRESULT hr = m_real->CreateDevice(guid, &realDevice, outer);
        if (FAILED(hr))
            return hr;

        const bool mouse = IsEqualGUID(guid, FNVXR_GUID_SysMouse) != FALSE;
        const bool keyboard = IsEqualGUID(guid, FNVXR_GUID_SysKeyboard) != FALSE;
        logLine("CreateDevice mouse=%d keyboard=%d real=%p\n", mouse ? 1 : 0, keyboard ? 1 : 0, realDevice);
        if (mouse || keyboard)
            *device = new WrappedDirectInputDevice8A(realDevice, mouse, keyboard);
        else
            *device = realDevice;
        return hr;
    }

    HRESULT STDMETHODCALLTYPE EnumDevices(DWORD type, LPDIENUMDEVICESCALLBACKA cb, LPVOID ref, DWORD flags) override { return m_real->EnumDevices(type, cb, ref, flags); }
    HRESULT STDMETHODCALLTYPE GetDeviceStatus(REFGUID guid) override { return m_real->GetDeviceStatus(guid); }
    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND owner, DWORD flags) override { return m_real->RunControlPanel(owner, flags); }
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE instance, DWORD version) override { return m_real->Initialize(instance, version); }
    HRESULT STDMETHODCALLTYPE FindDevice(REFGUID guid, LPCSTR name, LPGUID out) override { return m_real->FindDevice(guid, name, out); }
    HRESULT STDMETHODCALLTYPE EnumDevicesBySemantics(LPCSTR user, LPDIACTIONFORMATA action, LPDIENUMDEVICESBYSEMANTICSCBA cb, LPVOID ref, DWORD flags) override { return m_real->EnumDevicesBySemantics(user, action, cb, ref, flags); }
    HRESULT STDMETHODCALLTYPE ConfigureDevices(LPDICONFIGUREDEVICESCALLBACK cb, LPDICONFIGUREDEVICESPARAMSA params, DWORD flags, LPVOID ref) override { return m_real->ConfigureDevices(cb, params, flags, ref); }

private:
    IDirectInput8A* m_real = nullptr;
    LONG m_refs = 1;
};
}

extern "C" HRESULT WINAPI FNVXR_DirectInput8Create(HINSTANCE instance, DWORD version, REFIID riid, LPVOID* out, LPUNKNOWN outer)
{
    logLine("DirectInput8Create version=%lu out=%p\n", version, out);
    using Fn = HRESULT (WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
    auto* fn = realDInput() ? reinterpret_cast<Fn>(GetProcAddress(g_realDInput, "DirectInput8Create")) : nullptr;
    if (!fn)
        return DIERR_GENERIC;

    void* realOut = nullptr;
    HRESULT hr = fn(instance, version, riid, &realOut, outer);
    if (FAILED(hr) || !realOut || !out)
        return hr;

    if (IsEqualGUID(riid, IID_IDirectInput8A))
        *out = new WrappedDirectInput8A(static_cast<IDirectInput8A*>(realOut));
    else
        *out = realOut;
    return hr;
}

extern "C" HRESULT WINAPI FNVXR_DllCanUnloadNow()
{
    using Fn = HRESULT (WINAPI*)();
    if (auto* fn = realDInput() ? reinterpret_cast<Fn>(GetProcAddress(g_realDInput, "DllCanUnloadNow")) : nullptr)
        return fn();
    return S_FALSE;
}

extern "C" HRESULT WINAPI FNVXR_DllGetClassObject(REFCLSID clsid, REFIID riid, LPVOID* out)
{
    using Fn = HRESULT (WINAPI*)(REFCLSID, REFIID, LPVOID*);
    if (auto* fn = realDInput() ? reinterpret_cast<Fn>(GetProcAddress(g_realDInput, "DllGetClassObject")) : nullptr)
        return fn(clsid, riid, out);
    return CLASS_E_CLASSNOTAVAILABLE;
}

extern "C" HRESULT WINAPI FNVXR_DllRegisterServer()
{
    using Fn = HRESULT (WINAPI*)();
    if (auto* fn = realDInput() ? reinterpret_cast<Fn>(GetProcAddress(g_realDInput, "DllRegisterServer")) : nullptr)
        return fn();
    return S_OK;
}

extern "C" HRESULT WINAPI FNVXR_DllUnregisterServer()
{
    using Fn = HRESULT (WINAPI*)();
    if (auto* fn = realDInput() ? reinterpret_cast<Fn>(GetProcAddress(g_realDInput, "DllUnregisterServer")) : nullptr)
        return fn();
    return S_OK;
}

extern "C" HRESULT WINAPI FNVXR_GetdfDIJoystick(LPCDIDATAFORMAT* out)
{
    using Fn = HRESULT (WINAPI*)(LPCDIDATAFORMAT*);
    if (auto* fn = realDInput() ? reinterpret_cast<Fn>(GetProcAddress(g_realDInput, "GetdfDIJoystick")) : nullptr)
        return fn(out);
    return DIERR_GENERIC;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(module);
    // Do not block or call FreeLibrary while the loader lock is held. The
    // process owns this proxy for its lifetime and the OS releases resources.
    return TRUE;
}
