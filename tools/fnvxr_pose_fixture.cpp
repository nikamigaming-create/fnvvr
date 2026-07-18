#include "fnvxr_shared_state.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>

namespace
{
constexpr const char* MappingName = fnvxr::shared::VrPoseSharedMappingName;
constexpr float Pi = 3.14159265358979323846f;

struct Options
{
    double yawDegrees = 0.0;
    double pitchDegrees = 0.0;
    double rollDegrees = 0.0;
    double ipdMeters = 0.064;
    int periodMs = 11;
    int durationSeconds = 0;
};

bool parseDouble(const char* text, double& value)
{
    if (!text)
        return false;
    char* end = nullptr;
    const double parsed = std::strtod(text, &end);
    if (end == text || *end != '\0' || !std::isfinite(parsed))
        return false;
    value = parsed;
    return true;
}

bool parseInt(const char* text, int& value)
{
    if (!text)
        return false;
    char* end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (end == text || *end != '\0')
        return false;
    value = static_cast<int>(parsed);
    return true;
}

void normalize(float q[4])
{
    const float length = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (length <= 0.000001f)
    {
        q[0] = q[1] = q[2] = 0.0f;
        q[3] = 1.0f;
        return;
    }
    for (int index = 0; index < 4; ++index)
        q[index] /= length;
}

void multiplyQuat(const float a[4], const float b[4], float out[4])
{
    out[0] = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
    out[1] = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
    out[2] = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
    out[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
    normalize(out);
}

void quaternionFromEuler(const Options& options, float out[4])
{
    const float yaw = static_cast<float>(options.yawDegrees) * Pi / 180.0f;
    const float pitch = static_cast<float>(options.pitchDegrees) * Pi / 180.0f;
    const float roll = static_cast<float>(options.rollDegrees) * Pi / 180.0f;
    const float qYaw[4] { 0.0f, std::sin(yaw * 0.5f), 0.0f, std::cos(yaw * 0.5f) };
    const float qPitch[4] { std::sin(pitch * 0.5f), 0.0f, 0.0f, std::cos(pitch * 0.5f) };
    const float qRoll[4] { 0.0f, 0.0f, std::sin(roll * 0.5f), std::cos(roll * 0.5f) };
    float yawPitch[4] {};
    multiplyQuat(qYaw, qPitch, yawPitch);
    multiplyQuat(yawPitch, qRoll, out);
}

void copyQuat(float destination[4], const float source[4])
{
    std::memcpy(destination, source, sizeof(float) * 4);
}

void printUsage()
{
    std::cout
        << "usage: fnvxr_pose_fixture [--yaw-deg N] [--pitch-deg N] [--roll-deg N] "
           "[--ipd-meters N] [--period-ms N] [--duration-seconds N]\n";
}
}

int main(int argc, char** argv)
{
    Options options {};
    for (int index = 1; index < argc; ++index)
    {
        const std::string argument = argv[index];
        if (argument == "--help" || argument == "-h")
        {
            printUsage();
            return 0;
        }
        if (index + 1 >= argc)
        {
            printUsage();
            return 2;
        }
        const char* value = argv[++index];
        bool parsed = false;
        if (argument == "--yaw-deg")
            parsed = parseDouble(value, options.yawDegrees);
        else if (argument == "--pitch-deg")
            parsed = parseDouble(value, options.pitchDegrees);
        else if (argument == "--roll-deg")
            parsed = parseDouble(value, options.rollDegrees);
        else if (argument == "--ipd-meters")
            parsed = parseDouble(value, options.ipdMeters);
        else if (argument == "--period-ms")
            parsed = parseInt(value, options.periodMs);
        else if (argument == "--duration-seconds")
            parsed = parseInt(value, options.durationSeconds);
        if (!parsed)
        {
            std::cerr << "invalid argument: " << argument << " " << value << "\n";
            return 2;
        }
    }

    options.ipdMeters = (std::max)(0.03, (std::min)(0.12, options.ipdMeters));
    options.periodMs = (std::max)(1, options.periodMs);
    options.durationSeconds = (std::max)(0, options.durationSeconds);

    HANDLE producerMutex = CreateMutexA(
        nullptr, TRUE, fnvxr::shared::InputCoreProducerMutexName);
    if (!producerMutex)
    {
        std::cerr << "CreateMutex failed error=" << GetLastError() << "\n";
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        const DWORD wait = WaitForSingleObject(producerMutex, 0);
        if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED)
        {
            std::cerr << "live host owns the input/pose producer lease\n";
            CloseHandle(producerMutex);
            return 1;
        }
    }

    HANDLE mapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        static_cast<DWORD>(sizeof(fnvxr::shared::SharedVrPoseState)),
        MappingName);
    if (!mapping)
    {
        std::cerr << "CreateFileMapping failed error=" << GetLastError() << "\n";
        ReleaseMutex(producerMutex);
        CloseHandle(producerMutex);
        return 1;
    }
    const bool mappingAlreadyExisted = GetLastError() == ERROR_ALREADY_EXISTS;
    auto* state = static_cast<fnvxr::shared::SharedVrPoseState*>(
        MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(fnvxr::shared::SharedVrPoseState)));
    if (!state)
    {
        std::cerr << "MapViewOfFile failed error=" << GetLastError() << "\n";
        CloseHandle(mapping);
        ReleaseMutex(producerMutex);
        CloseHandle(producerMutex);
        return 1;
    }

    const bool existingStateUsable = mappingAlreadyExisted
        && state->magic == fnvxr::shared::VrPoseSharedMagic
        && state->version == fnvxr::shared::VrPoseSharedVersion;
    if (!existingStateUsable)
    {
        std::memset(state, 0, sizeof(*state));
        state->magic = fnvxr::shared::VrPoseSharedMagic;
        state->version = fnvxr::shared::VrPoseSharedVersion;
        state->hmdRot[3] = 1.0f;
        state->leftRot[3] = 1.0f;
        state->rightRot[3] = 1.0f;
        state->leftAimRot[3] = 1.0f;
        state->rightAimRot[3] = 1.0f;
        state->leftEyeRot[3] = 1.0f;
        state->rightEyeRot[3] = 1.0f;
    }
    const bool inheritedOddWrite = existingStateUsable && (state->sequence & 1) != 0;
    const std::uint64_t previousProducerEpoch = existingStateUsable
        ? static_cast<std::uint64_t>(InterlockedCompareExchange64(
            reinterpret_cast<volatile LONG64*>(&state->producerEpoch), 0, 0))
        : 0;
    if (previousProducerEpoch == (std::numeric_limits<std::uint64_t>::max)())
    {
        std::cerr << "pose fixture producer epoch exhausted\n";
        UnmapViewOfFile(state);
        CloseHandle(mapping);
        ReleaseMutex(producerMutex);
        CloseHandle(producerMutex);
        return 1;
    }
    const std::uint64_t producerEpoch = existingStateUsable
        ? previousProducerEpoch + 1u
        : 1u;
    float rotation[4] {};
    quaternionFromEuler(options, rotation);
    const float halfIpd = static_cast<float>(options.ipdMeters * 0.5);
    const ULONGLONG started = GetTickCount64();
    // An inherited odd sequence means the previous producer may have torn any
    // payload field, including frame.  The first recovery write owns that odd
    // sequence until the complete payload is replaced, so it must not derive
    // the new lineage from a possibly torn frame value.
    std::uint64_t frame = existingStateUsable && !inheritedOddWrite
        ? static_cast<std::uint64_t>(InterlockedCompareExchange64(
            reinterpret_cast<volatile LONG64*>(&state->frame), 0, 0))
        : 0;
    bool ownsInheritedOddWrite = inheritedOddWrite;

    std::cout
        << "pose fixture ready mapping=" << MappingName
        << " yaw=" << options.yawDegrees
        << " pitch=" << options.pitchDegrees
        << " roll=" << options.rollDegrees
        << " ipd=" << options.ipdMeters
        << " attached=" << (existingStateUsable ? 1 : 0) << "\n";
    std::cout.flush();

    while (options.durationSeconds == 0
        || GetTickCount64() - started < static_cast<ULONGLONG>(options.durationSeconds) * 1000ull)
    {
        if (!ownsInheritedOddWrite
            && !fnvxr::shared::beginSequencedSharedWrite(state->sequence))
        {
            std::cerr << "pose fixture could not acquire shared write sequence\n";
            break;
        }
        state->magic = fnvxr::shared::VrPoseSharedMagic;
        state->version = fnvxr::shared::VrPoseSharedVersion;
        state->referenceSpaceGeneration = 1;
        InterlockedExchange64(
            reinterpret_cast<volatile LONG64*>(&state->producerEpoch),
            static_cast<LONG64>(producerEpoch));
        state->recenterRequestId = 0;
        state->reserved = static_cast<std::uint32_t>(GetTickCount64());
        state->trackingFlags =
            fnvxr::shared::VrPoseTrackingHmd
            | fnvxr::shared::VrPoseTrackingLeftGripActive
            | fnvxr::shared::VrPoseTrackingRightGripActive
            | fnvxr::shared::VrPoseTrackingLeftGripCurrent
            | fnvxr::shared::VrPoseTrackingRightGripCurrent
            | fnvxr::shared::VrPoseTrackingLeftAimActive
            | fnvxr::shared::VrPoseTrackingRightAimActive
            | fnvxr::shared::VrPoseTrackingLeftAimCurrent
            | fnvxr::shared::VrPoseTrackingRightAimCurrent;
        state->frame = ++frame;
        state->predictedDisplayTime = static_cast<std::int64_t>(GetTickCount64()) * 1000000ll + 11000000ll;
        copyQuat(state->hmdRot, rotation);
        copyQuat(state->leftRot, rotation);
        copyQuat(state->rightRot, rotation);
        copyQuat(state->leftAimRot, rotation);
        copyQuat(state->rightAimRot, rotation);
        copyQuat(state->leftEyeRot, rotation);
        copyQuat(state->rightEyeRot, rotation);
        state->hmdPos[0] = 0.0f;
        state->hmdPos[1] = 1.65f;
        state->hmdPos[2] = 0.0f;
        state->leftPos[0] = state->leftEyePos[0] = -halfIpd;
        state->leftPos[1] = state->leftEyePos[1] = 1.65f;
        state->leftPos[2] = state->leftEyePos[2] = 0.0f;
        state->rightPos[0] = state->rightEyePos[0] = halfIpd;
        state->rightPos[1] = state->rightEyePos[1] = 1.65f;
        state->rightPos[2] = state->rightEyePos[2] = 0.0f;
        std::memcpy(state->leftAimPos, state->leftPos, sizeof(state->leftAimPos));
        std::memcpy(state->rightAimPos, state->rightPos, sizeof(state->rightAimPos));
        const float fov[4] { -0.9f, 0.9f, 0.9f, -0.9f };
        std::memcpy(state->leftFov, fov, sizeof(fov));
        std::memcpy(state->rightFov, fov, sizeof(fov));
        fnvxr::shared::endSequencedSharedWrite(state->sequence);
        ownsInheritedOddWrite = false;
        Sleep(static_cast<DWORD>(options.periodMs));
    }

    UnmapViewOfFile(state);
    CloseHandle(mapping);
    ReleaseMutex(producerMutex);
    CloseHandle(producerMutex);
    return 0;
}
