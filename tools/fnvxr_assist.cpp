#include "fnvxr_assist_scenario.h"
#include "fnvxr_shared_state.h"
#include "../runtime/fnvxr_desktop_assist_ui_evidence.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

namespace
{
constexpr float Pi = 3.14159265358979323846f;
constexpr float BodyRotationToleranceDegrees = 3.0f;
// Desktop-assist body-root evidence is in Gamebryo world units. A real head
// lean is roughly eleven units at the nominal 70 units/meter scale, so this
// leaves a small sampling/idle margin while still rejecting a head pose that
// has been applied to the player/body transform.
constexpr float BodyPositionToleranceUnits = 0.25f;
constexpr float MinimumCameraRotationResponseDegrees = 5.0f;
constexpr float MinimumExpectedRotationResponseRatio = 0.35f;
constexpr float MinimumDistinctPhaseCameraRotationDegrees = 2.0f;
constexpr float MinimumCameraTranslationResponse = 0.001f;

struct PublishedPose
{
    std::uint64_t frame = 0u;
    std::uint32_t sequence = 0u;
};

struct Options
{
    int stepMilliseconds = 1200;
    int periodMilliseconds = 20;
    int cycles = 1;
    double ipdMeters = 0.064;
    std::string reportPath;
    bool overwriteReport = false;
    bool requireRetailObservation = false;
    bool requireDesktopAssist = false;
    bool requireTranslationResponse = false;
    bool expectUiTransition = false;
    bool requireUiQuadTransition = false;
    bool observeRetail = true;
    bool validateScenario = false;
    bool selfTest = false;
    bool emitTrackedPropControllers = false;
};

struct StepMetrics
{
    std::uint64_t poseWrites = 0;
    std::uint64_t bodySamples = 0;
    std::uint64_t cameraSamples = 0;
    std::uint64_t desktopAssistSamples = 0;
    std::uint64_t desktopAssistCurrentPhasePoseSamples = 0;
    std::uint64_t runtimeSamples = 0;
    std::uint32_t firstPublishedPoseSequence = 0u;
    std::uint32_t lastPublishedPoseSequence = 0u;
    float maximumBodyRotationDegrees = 0.0f;
    float maximumCameraRotationDegrees = 0.0f;
    float maximumBodyPositionDelta = 0.0f;
    float maximumCameraPositionDelta = 0.0f;
    bool representativeCameraRotationValid = false;
    float representativeCameraRotation[9] {};
    bool sawUi = false;
    bool sawGameplay = false;
};

struct Baseline
{
    bool body = false;
    bool camera = false;
    float bodyRotation[9] {};
    float bodyPosition[3] {};
    float cameraRotation[9] {};
    float cameraPosition[3] {};
};

enum class ObservedRetailMode
{
    Unknown,
    Ui,
    Gameplay,
};

enum class CameraEvidenceSource
{
    None,
    WorldCamera,
    DesktopAssistLocalCamera,
};

const char* cameraEvidenceSourceName(CameraEvidenceSource source)
{
    switch (source)
    {
    case CameraEvidenceSource::WorldCamera:
        return "world-camera-mapping";
    case CameraEvidenceSource::DesktopAssistLocalCamera:
        return "desktop-assist-local-camera";
    default:
        return "none";
    }
}

struct ObservationState
{
    Baseline baseline {};
    ObservedRetailMode lastMode = ObservedRetailMode::Unknown;
    CameraEvidenceSource cameraSource = CameraEvidenceSource::None;
    bool sawGameplayToUi = false;
    bool sawUiToGameplay = false;
    bool desktopAssistStateObserved = false;
    bool desktopAssistCurrentPhasePoseAppliedObserved = false;
    bool desktopAssistUiQuadStateObserved = false;
    bool desktopAssistUiQuadCurrentPhasePoseObserved = false;
    bool desktopAssistUiQuadMatchedRuntimeObserved = false;
    bool desktopAssistUiQuadPixelsVerifiedObserved = false;
    bool desktopAssistUiQuadInvalidatedAfterUiObserved = false;
    bool desktopAssistUiQuadDuringActiveUiVisit = false;
    bool desktopAssistUiQuadExitPendingInvalidation = false;
    bool desktopAssistUiQuadTransitionObserved = false;
};

struct RetailSnapshot
{
    bool player = false;
    bool camera = false;
    bool runtime = false;
    bool runtimeBeforeUiQuad = false;
    bool desktopAssistStatePresent = false;
    bool desktopAssistCameraApplied = false;
    bool desktopAssistUiQuadStatePresent = false;
    bool desktopAssistUiQuadCaptured = false;
    // This is normally false. Live observations verify the matching pixel
    // mapping just-in-time; the self-test uses it to inject a separately
    // validated synthetic payload without creating global mappings.
    bool desktopAssistUiQuadPixelsVerified = false;
    fnvxr::shared::SharedPlayerState playerState {};
    fnvxr::shared::SharedCameraState cameraState {};
    fnvxr::shared::SharedRuntimeState runtimeState {};
    fnvxr::shared::SharedRuntimeState runtimeStateBeforeUiQuad {};
    fnvxr::shared::SharedDesktopAssistState desktopAssistState {};
    fnvxr::shared::SharedDesktopAssistUiQuadHeader desktopAssistUiQuadHeader {};
};

struct RunResult
{
    std::array<StepMetrics, fnvxr::assist::HeadBodyScenarioStepCount> steps {};
    ObservationState observation {};
    std::uint64_t expectedPoseWrites = 0;
    std::uint64_t actualPoseWrites = 0;
    std::uint64_t expectedPoseProducerEpoch = 0;
    std::uint32_t currentPhaseFirstPublishedPoseSequence = 0u;
    std::uint32_t currentPhaseLastPublishedPoseSequence = 0u;
    bool allEyeCentersValid = true;
};

bool parseInt(const char* text, int& value)
{
    if (!text)
        return false;
    char* end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (end == text || *end != '\0'
        || parsed < static_cast<long>((std::numeric_limits<int>::min)())
        || parsed > static_cast<long>((std::numeric_limits<int>::max)()))
    {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

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

void printUsage()
{
    std::cout
        << "usage: fnvxr_assist [--scenario head-body] [--step-ms N] [--period-ms N] [--cycles N]\n"
           "                    [--ipd-meters N] [--report PATH] [--overwrite-report]\n"
           "                    [--require-retail-observation] [--require-desktop-assist] [--require-6dof]\n"
           "                    [--expect-ui-transition] [--require-ui-quad-transition]\n"
           "                    [--tracked-prop] [--no-observe] [--validate-scenario] [--self-test]\n\n"
           "Runs a deterministic, headset-free pose trace. It never opens an OpenXR runtime\n"
           "or enables a retail hook. With a desktop game already running, it observes the\n"
           "published player/camera/runtime mappings and reports whether head/body behavior\n"
           "was actually evidenced.\n";
}

bool parseOptions(int argc, char** argv, Options& options)
{
    for (int index = 1; index < argc; ++index)
    {
        const std::string argument = argv[index];
        if (argument == "--help" || argument == "-h")
        {
            printUsage();
            std::exit(EXIT_SUCCESS);
        }
        if (argument == "--overwrite-report")
        {
            options.overwriteReport = true;
            continue;
        }
        if (argument == "--require-retail-observation")
        {
            options.requireRetailObservation = true;
            continue;
        }
        if (argument == "--require-desktop-assist")
        {
            options.requireRetailObservation = true;
            options.requireDesktopAssist = true;
            continue;
        }
        if (argument == "--require-6dof")
        {
            options.requireRetailObservation = true;
            options.requireTranslationResponse = true;
            continue;
        }
        if (argument == "--expect-ui-transition")
        {
            options.requireRetailObservation = true;
            options.expectUiTransition = true;
            continue;
        }
        if (argument == "--require-ui-quad-transition")
        {
            options.requireRetailObservation = true;
            options.requireDesktopAssist = true;
            options.requireUiQuadTransition = true;
            continue;
        }
        if (argument == "--no-observe")
        {
            options.observeRetail = false;
            continue;
        }
        if (argument == "--tracked-prop")
        {
            options.emitTrackedPropControllers = true;
            continue;
        }
        if (argument == "--validate-scenario")
        {
            options.validateScenario = true;
            continue;
        }
        if (argument == "--self-test")
        {
            options.selfTest = true;
            continue;
        }
        if (index + 1 >= argc)
            return false;

        const char* value = argv[++index];
        bool parsed = false;
        if (argument == "--scenario")
        {
            parsed = std::strcmp(value, "head-body") == 0;
        }
        else if (argument == "--step-ms")
        {
            parsed = parseInt(value, options.stepMilliseconds);
        }
        else if (argument == "--period-ms")
        {
            parsed = parseInt(value, options.periodMilliseconds);
        }
        else if (argument == "--cycles")
        {
            parsed = parseInt(value, options.cycles);
        }
        else if (argument == "--ipd-meters")
        {
            parsed = parseDouble(value, options.ipdMeters);
        }
        else if (argument == "--report")
        {
            options.reportPath = value;
            parsed = !options.reportPath.empty();
        }

        if (!parsed)
        {
            std::cerr << "invalid argument: " << argument << " " << value << "\n";
            return false;
        }
    }

    options.stepMilliseconds = (std::max)(1, (std::min)(600000, options.stepMilliseconds));
    options.periodMilliseconds = (std::max)(1, (std::min)(1000, options.periodMilliseconds));
    options.cycles = (std::max)(1, (std::min)(100, options.cycles));
    options.ipdMeters = (std::max)(0.03, (std::min)(0.12, options.ipdMeters));

    if (!options.observeRetail
        && (options.requireRetailObservation || options.requireTranslationResponse || options.expectUiTransition
            || options.requireUiQuadTransition))
    {
        std::cerr << "retail proof options cannot be used with --no-observe\n";
        return false;
    }
    return true;
}

bool reportPathIsSafe(const Options& options)
{
    if (options.reportPath.empty())
        return true;
    const DWORD attributes = GetFileAttributesA(options.reportPath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        const DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
    }
    if (!options.overwriteReport)
    {
        std::cerr << "report already exists; use --overwrite-report to replace it: "
                  << options.reportPath << "\n";
        return false;
    }
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

void normalize(float value[4])
{
    const float length = std::sqrt(value[0] * value[0] + value[1] * value[1]
        + value[2] * value[2] + value[3] * value[3]);
    if (length <= 0.000001f)
    {
        value[0] = 0.0f;
        value[1] = 0.0f;
        value[2] = 0.0f;
        value[3] = 1.0f;
        return;
    }
    for (int index = 0; index < 4; ++index)
        value[index] /= length;
}

void multiplyQuaternion(const float left[4], const float right[4], float result[4])
{
    result[0] = left[3] * right[0] + left[0] * right[3] + left[1] * right[2] - left[2] * right[1];
    result[1] = left[3] * right[1] - left[0] * right[2] + left[1] * right[3] + left[2] * right[0];
    result[2] = left[3] * right[2] + left[0] * right[1] - left[1] * right[0] + left[2] * right[3];
    result[3] = left[3] * right[3] - left[0] * right[0] - left[1] * right[1] - left[2] * right[2];
    normalize(result);
}

void quaternionFromEulerDegrees(
    float yawDegrees,
    float pitchDegrees,
    float rollDegrees,
    float result[4])
{
    const float yaw = yawDegrees * Pi / 180.0f;
    const float pitch = pitchDegrees * Pi / 180.0f;
    const float roll = rollDegrees * Pi / 180.0f;
    const float yawQuaternion[4] { 0.0f, std::sin(yaw * 0.5f), 0.0f, std::cos(yaw * 0.5f) };
    const float pitchQuaternion[4] { std::sin(pitch * 0.5f), 0.0f, 0.0f, std::cos(pitch * 0.5f) };
    const float rollQuaternion[4] { 0.0f, 0.0f, std::sin(roll * 0.5f), std::cos(roll * 0.5f) };
    float yawPitch[4] {};
    multiplyQuaternion(yawQuaternion, pitchQuaternion, yawPitch);
    multiplyQuaternion(yawPitch, rollQuaternion, result);
}

void quaternionFromEuler(const fnvxr::assist::PoseStep& step, float result[4])
{
    quaternionFromEulerDegrees(
        step.yawDegrees,
        step.pitchDegrees,
        step.rollDegrees,
        result);
}

void rotateVector(const float quaternion[4], const float vector[3], float result[3])
{
    const float xx = quaternion[0] * quaternion[0];
    const float yy = quaternion[1] * quaternion[1];
    const float zz = quaternion[2] * quaternion[2];
    const float xy = quaternion[0] * quaternion[1];
    const float xz = quaternion[0] * quaternion[2];
    const float yz = quaternion[1] * quaternion[2];
    const float wx = quaternion[3] * quaternion[0];
    const float wy = quaternion[3] * quaternion[1];
    const float wz = quaternion[3] * quaternion[2];
    result[0] = (1.0f - 2.0f * (yy + zz)) * vector[0]
        + 2.0f * (xy - wz) * vector[1]
        + 2.0f * (xz + wy) * vector[2];
    result[1] = 2.0f * (xy + wz) * vector[0]
        + (1.0f - 2.0f * (xx + zz)) * vector[1]
        + 2.0f * (yz - wx) * vector[2];
    result[2] = 2.0f * (xz - wy) * vector[0]
        + 2.0f * (yz + wx) * vector[1]
        + (1.0f - 2.0f * (xx + yy)) * vector[2];
}

class PoseProducer final
{
public:
    ~PoseProducer()
    {
        if (mState)
            UnmapViewOfFile(mState);
        if (mMapping)
            CloseHandle(mMapping);
        if (mOwnsMutex && mMutex)
            ReleaseMutex(mMutex);
        if (mMutex)
            CloseHandle(mMutex);
    }

    bool open(std::string& error)
    {
        mMutex = CreateMutexA(nullptr, TRUE, fnvxr::shared::InputCoreProducerMutexName);
        if (!mMutex)
        {
            error = "CreateMutex failed error=" + std::to_string(GetLastError());
            return false;
        }
        const bool mutexAlreadyExisted = GetLastError() == ERROR_ALREADY_EXISTS;
        mOwnsMutex = !mutexAlreadyExisted;
        if (mutexAlreadyExisted)
        {
            const DWORD wait = WaitForSingleObject(mMutex, 0);
            if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED)
            {
                error = "a live OpenXR host owns the pose producer lease";
                return false;
            }
            mOwnsMutex = true;
        }

        mMapping = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            0,
            static_cast<DWORD>(sizeof(fnvxr::shared::SharedVrPoseState)),
            fnvxr::shared::VrPoseSharedMappingName);
        if (!mMapping)
        {
            error = "CreateFileMapping failed error=" + std::to_string(GetLastError());
            return false;
        }
        const bool mappingAlreadyExisted = GetLastError() == ERROR_ALREADY_EXISTS;
        mState = static_cast<fnvxr::shared::SharedVrPoseState*>(MapViewOfFile(
            mMapping,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            sizeof(fnvxr::shared::SharedVrPoseState)));
        if (!mState)
        {
            error = "MapViewOfFile failed error=" + std::to_string(GetLastError());
            return false;
        }

        const bool existingStateUsable = mappingAlreadyExisted
            && mState->magic == fnvxr::shared::VrPoseSharedMagic
            && mState->version == fnvxr::shared::VrPoseSharedVersion;
        if (!existingStateUsable)
        {
            std::memset(mState, 0, sizeof(*mState));
            mState->magic = fnvxr::shared::VrPoseSharedMagic;
            mState->version = fnvxr::shared::VrPoseSharedVersion;
        }
        mOwnsInheritedOddWrite = existingStateUsable && (mState->sequence & 1) != 0;
        const std::uint64_t previousProducerEpoch = existingStateUsable
            ? static_cast<std::uint64_t>(InterlockedCompareExchange64(
                reinterpret_cast<volatile LONG64*>(&mState->producerEpoch), 0, 0))
            : 0;
        if (previousProducerEpoch == (std::numeric_limits<std::uint64_t>::max)())
        {
            error = "synthetic pose producer epoch is exhausted";
            return false;
        }
        mProducerEpoch = existingStateUsable ? previousProducerEpoch + 1u : 1u;
        mFrame = existingStateUsable && !mOwnsInheritedOddWrite
            ? static_cast<std::uint64_t>(InterlockedCompareExchange64(
                reinterpret_cast<volatile LONG64*>(&mState->frame), 0, 0))
            : 0;
        return true;
    }

    std::uint64_t producerEpoch() const noexcept
    {
        return mProducerEpoch;
    }

    bool publish(
        const fnvxr::assist::PoseStep& step,
        double ipdMeters,
        bool emitTrackedPropControllers,
        bool& eyeCenterValid,
        PublishedPose& publication)
    {
        publication = {};
        if (!mState)
            return false;
        if (!mOwnsInheritedOddWrite
            && !fnvxr::shared::beginSequencedSharedWrite(mState->sequence))
        {
            return false;
        }

        float rotation[4] {};
        quaternionFromEuler(step, rotation);
        const float hmdPosition[3] {
            step.positionX,
            fnvxr::assist::HeadHeightMeters + step.positionY,
            step.positionZ,
        };
        const float halfIpd = static_cast<float>(ipdMeters * 0.5);
        const float leftOffset[3] { -halfIpd, 0.0f, 0.0f };
        const float rightOffset[3] { halfIpd, 0.0f, 0.0f };
        float leftEyeOffset[3] {};
        float rightEyeOffset[3] {};
        rotateVector(rotation, leftOffset, leftEyeOffset);
        rotateVector(rotation, rightOffset, rightEyeOffset);
        const float leftEyePosition[3] {
            hmdPosition[0] + leftEyeOffset[0],
            hmdPosition[1] + leftEyeOffset[1],
            hmdPosition[2] + leftEyeOffset[2],
        };
        const float rightEyePosition[3] {
            hmdPosition[0] + rightEyeOffset[0],
            hmdPosition[1] + rightEyeOffset[1],
            hmdPosition[2] + rightEyeOffset[2],
        };
        const float eyeCenter[3] {
            (leftEyePosition[0] + rightEyePosition[0]) * 0.5f,
            (leftEyePosition[1] + rightEyePosition[1]) * 0.5f,
            (leftEyePosition[2] + rightEyePosition[2]) * 0.5f,
        };
        eyeCenterValid = std::fabs(eyeCenter[0] - hmdPosition[0]) < 0.00001f
            && std::fabs(eyeCenter[1] - hmdPosition[1]) < 0.00001f
            && std::fabs(eyeCenter[2] - hmdPosition[2]) < 0.00001f;

        mState->magic = fnvxr::shared::VrPoseSharedMagic;
        mState->version = fnvxr::shared::VrPoseSharedVersion;
        mState->frame = ++mFrame;
        mState->predictedDisplayTime = static_cast<std::int64_t>(GetTickCount64()) * 1000000ll + 11000000ll;
        std::memcpy(mState->hmdRot, rotation, sizeof(rotation));
        std::memcpy(mState->hmdPos, hmdPosition, sizeof(hmdPosition));
        std::memcpy(mState->leftEyeRot, rotation, sizeof(rotation));
        std::memcpy(mState->rightEyeRot, rotation, sizeof(rotation));
        std::memcpy(mState->leftEyePos, leftEyePosition, sizeof(leftEyePosition));
        std::memcpy(mState->rightEyePos, rightEyePosition, sizeof(rightEyePosition));

        // The normal head/body fixture leaves controllers inactive.  The
        // explicit tracked-prop fixture instead supplies a stable right grip
        // and aim pose, with two controller-only probes in the shared trace.
        const float identity[4] { 0.0f, 0.0f, 0.0f, 1.0f };
        float rightControllerRotation[4] {};
        quaternionFromEulerDegrees(
            step.controllerYawDegrees,
            step.controllerPitchDegrees,
            step.controllerRollDegrees,
            rightControllerRotation);
        const float rightControllerPosition[3] {
            0.32f + step.controllerPositionX,
            1.30f + step.controllerPositionY,
            -0.38f + step.controllerPositionZ,
        };
        std::memcpy(mState->leftRot, identity, sizeof(identity));
        std::memcpy(mState->leftAimRot, identity, sizeof(identity));
        std::memcpy(mState->leftPos, hmdPosition, sizeof(hmdPosition));
        std::memcpy(mState->leftAimPos, hmdPosition, sizeof(hmdPosition));
        if (emitTrackedPropControllers)
        {
            std::memcpy(mState->rightRot, rightControllerRotation, sizeof(rightControllerRotation));
            std::memcpy(mState->rightAimRot, rightControllerRotation, sizeof(rightControllerRotation));
            std::memcpy(mState->rightPos, rightControllerPosition, sizeof(rightControllerPosition));
            std::memcpy(mState->rightAimPos, rightControllerPosition, sizeof(rightControllerPosition));
        }
        else
        {
            std::memcpy(mState->rightRot, identity, sizeof(identity));
            std::memcpy(mState->rightAimRot, identity, sizeof(identity));
            std::memcpy(mState->rightPos, hmdPosition, sizeof(hmdPosition));
            std::memcpy(mState->rightAimPos, hmdPosition, sizeof(hmdPosition));
        }
        const float fov[4] { -0.9f, 0.9f, 0.9f, -0.9f };
        std::memcpy(mState->leftFov, fov, sizeof(fov));
        std::memcpy(mState->rightFov, fov, sizeof(fov));
        mState->trackingFlags = fnvxr::shared::VrPoseTrackingHmd;
        if (emitTrackedPropControllers)
        {
            mState->trackingFlags |= fnvxr::shared::VrPoseTrackingRightGripActive
                | fnvxr::shared::VrPoseTrackingRightGripCurrent
                | fnvxr::shared::VrPoseTrackingRightAimActive
                | fnvxr::shared::VrPoseTrackingRightAimCurrent;
        }
        mState->referenceSpaceGeneration = 1;
        InterlockedExchange64(
            reinterpret_cast<volatile LONG64*>(&mState->producerEpoch),
            static_cast<LONG64>(mProducerEpoch));
        mState->recenterRequestId = 0;
        mState->reserved = static_cast<std::uint32_t>(GetTickCount64());
        fnvxr::shared::endSequencedSharedWrite(mState->sequence);
        mOwnsInheritedOddWrite = false;
        publication.frame = mFrame;
        publication.sequence = fnvxr::shared::sequencedValueBits(mState->sequence);
        return publication.frame != 0u
            && publication.sequence != 0u
            && (publication.sequence & 1u) == 0u;
    }

private:
    HANDLE mMutex = nullptr;
    HANDLE mMapping = nullptr;
    fnvxr::shared::SharedVrPoseState* mState = nullptr;
    std::uint64_t mProducerEpoch = 0;
    std::uint64_t mFrame = 0;
    bool mOwnsMutex = false;
    bool mOwnsInheritedOddWrite = false;
};

bool finite3(const float values[3])
{
    for (int index = 0; index < 3; ++index)
    {
        if (!std::isfinite(values[index]))
            return false;
    }
    return true;
}

bool finite9(const float values[9])
{
    for (int index = 0; index < 9; ++index)
    {
        if (!std::isfinite(values[index]))
            return false;
    }
    return true;
}

template <typename T>
bool readStableSharedState(const char* mappingName, T& result)
{
    HANDLE mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, mappingName);
    if (!mapping)
        return false;
    const auto closeMapping = [&mapping]() { CloseHandle(mapping); };
    const auto* state = static_cast<const T*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(T)));
    if (!state)
    {
        closeMapping();
        return false;
    }

    bool stable = false;
    for (int attempt = 0; attempt < 3 && !stable; ++attempt)
    {
        const LONG sequenceBefore = state->sequence;
        MemoryBarrier();
        std::memcpy(&result, state, sizeof(result));
        MemoryBarrier();
        const LONG sequenceAfter = state->sequence;
        stable = (sequenceBefore & 1) == 0 && sequenceBefore == sequenceAfter;
    }
    UnmapViewOfFile(state);
    closeMapping();
    return stable;
}

bool readStableDesktopAssistUiQuadHeader(
    fnvxr::shared::SharedDesktopAssistUiQuadHeader& result)
{
    HANDLE mapping = OpenFileMappingA(
        FILE_MAP_READ,
        FALSE,
        fnvxr::shared::DesktopAssistUiQuadSharedMappingName);
    if (!mapping)
        return false;
    const auto closeMapping = [&mapping]() { CloseHandle(mapping); };
    const auto* header = static_cast<const fnvxr::shared::SharedDesktopAssistUiQuadHeader*>(
        MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(result)));
    if (!header)
    {
        closeMapping();
        return false;
    }

    bool stable = false;
    for (int attempt = 0; attempt < 3 && !stable; ++attempt)
    {
        if (header->writing != 0)
            continue;
        const LONG sequenceBefore = header->sequence;
        MemoryBarrier();
        std::memcpy(&result, header, sizeof(result));
        MemoryBarrier();
        const LONG sequenceAfter = header->sequence;
        stable = header->writing == 0
            && result.writing == 0
            && sequenceBefore == sequenceAfter
            && result.sequence == sequenceBefore;
    }
    UnmapViewOfFile(header);
    closeMapping();
    return stable;
}

bool desktopAssistUiQuadPixelsMatchHeader(
    const fnvxr::shared::SharedDesktopAssistUiQuadHeader& expected)
{
    if (!fnvxr::engine::desktopAssistUiQuadHeaderIsComplete(expected))
        return false;

    HANDLE mapping = OpenFileMappingA(
        FILE_MAP_READ,
        FALSE,
        fnvxr::shared::DesktopAssistUiQuadSharedMappingName);
    if (!mapping)
        return false;
    const auto closeMapping = [&mapping]() { CloseHandle(mapping); };
    const auto* sharedHeader =
        static_cast<const fnvxr::shared::SharedDesktopAssistUiQuadHeader*>(
            MapViewOfFile(
                mapping,
                FILE_MAP_READ,
                0,
                0,
                fnvxr::engine::desktopAssistUiQuadMappingBytes()));
    if (!sharedHeader)
    {
        closeMapping();
        return false;
    }

    bool matched = false;
    for (int attempt = 0; attempt < 3 && !matched; ++attempt)
    {
        if (sharedHeader->writing != 0)
            continue;
        const LONG sequenceBefore = sharedHeader->sequence;
        MemoryBarrier();
        fnvxr::shared::SharedDesktopAssistUiQuadHeader observed {};
        std::memcpy(&observed, sharedHeader, sizeof(observed));
        MemoryBarrier();
        const LONG sequenceAfterHeader = sharedHeader->sequence;
        if (sharedHeader->writing != 0
            || observed.writing != 0
            || sequenceBefore != sequenceAfterHeader
            || observed.sequence != sequenceBefore
            || std::memcmp(&expected, &observed, sizeof(observed)) != 0)
        {
            continue;
        }

        std::size_t payloadBytes = 0u;
        if (!fnvxr::engine::desktopAssistUiQuadPayloadLayoutIsValid(
                observed,
                &payloadBytes))
        {
            continue;
        }
        const auto* pixels = reinterpret_cast<const std::uint8_t*>(sharedHeader)
            + observed.headerBytes;
        std::uint32_t nonBlack = 0u;
        const std::uint32_t hash = fnvxr::engine::desktopAssistUiQuadPixelHash(
            pixels,
            payloadBytes,
            &nonBlack);
        MemoryBarrier();
        const LONG sequenceAfterPixels = sharedHeader->sequence;
        if (sharedHeader->writing == 0
            && sequenceAfterPixels == sequenceAfterHeader
            && hash == observed.pixelHash
            && nonBlack == observed.nonBlackSampleCount)
        {
            matched = true;
        }
    }
    UnmapViewOfFile(sharedHeader);
    closeMapping();
    return matched;
}

bool readRetailRuntimeState(fnvxr::shared::SharedRuntimeState& result)
{
    return readStableSharedState("Local\\FNVXR_Runtime_State", result)
        && result.magic == fnvxr::shared::RuntimeSharedMagic
        && result.version == fnvxr::shared::RuntimeSharedVersion;
}

RetailSnapshot readRetailSnapshot()
{
    RetailSnapshot snapshot {};
    fnvxr::shared::SharedPlayerState player {};
    if (readStableSharedState("Local\\FNVXR_Player_State", player)
        && player.magic == fnvxr::shared::PlayerSharedMagic
        && player.version == fnvxr::shared::PlayerSharedVersion
        && (player.flags & fnvxr::shared::PlayerSharedFlagPlayerNodeValid) != 0u
        && finite3(player.playerWorldPos)
        && finite9(player.playerWorldRot))
    {
        snapshot.player = true;
        snapshot.playerState = player;
    }

    fnvxr::shared::SharedCameraState camera {};
    if (readStableSharedState("Local\\FNVXR_Camera_State", camera)
        && camera.magic == fnvxr::shared::CameraSharedMagic
        && camera.version == fnvxr::shared::CameraSharedVersion
        && camera.active != 0u
        && finite3(camera.worldPos)
        && finite9(camera.worldRot))
    {
        snapshot.camera = true;
        snapshot.cameraState = camera;
    }

    fnvxr::shared::SharedDesktopAssistState desktopAssist {};
    if (readStableSharedState(
            fnvxr::shared::DesktopAssistSharedMappingName,
            desktopAssist)
        && desktopAssist.magic == fnvxr::shared::DesktopAssistSharedMagic
        && desktopAssist.version == fnvxr::shared::DesktopAssistSharedVersion)
    {
        snapshot.desktopAssistStatePresent = true;
        snapshot.desktopAssistState = desktopAssist;
        constexpr std::uint32_t requiredFlags =
            fnvxr::shared::DesktopAssistFlagLeaseCurrent
            | fnvxr::shared::DesktopAssistFlagCameraHookInstalled
            | fnvxr::shared::DesktopAssistFlagCameraPoseApplied
            | fnvxr::shared::DesktopAssistFlagFirstPerson
            | fnvxr::shared::DesktopAssistFlagPlayerTransformValid
            | fnvxr::shared::DesktopAssistFlagCameraLocalTransformValid
            | fnvxr::shared::DesktopAssistFlagBodyRootTransformValid;
        snapshot.desktopAssistCameraApplied =
            (desktopAssist.flags & requiredFlags) == requiredFlags
            && desktopAssist.poseSequence != 0u
            && desktopAssist.poseProducerEpoch != 0u
            && finite3(desktopAssist.playerWorldPos)
            && finite9(desktopAssist.playerWorldRot)
            && finite3(desktopAssist.cameraLocalPos)
            && finite9(desktopAssist.cameraLocalRot)
            && desktopAssist.bodyRootAddress != 0u
            && finite3(desktopAssist.bodyRootWorldPos)
            && finite9(desktopAssist.bodyRootWorldRot);
    }

    fnvxr::shared::SharedRuntimeState runtimeBeforeUiQuad {};
    if (readRetailRuntimeState(runtimeBeforeUiQuad))
    {
        snapshot.runtimeBeforeUiQuad = true;
        snapshot.runtimeStateBeforeUiQuad = runtimeBeforeUiQuad;
    }

    fnvxr::shared::SharedDesktopAssistUiQuadHeader uiQuad {};
    if (readStableDesktopAssistUiQuadHeader(uiQuad)
        && uiQuad.magic == fnvxr::shared::DesktopAssistUiQuadSharedMagic
        && uiQuad.version == fnvxr::shared::DesktopAssistUiQuadSharedVersion)
    {
        snapshot.desktopAssistUiQuadStatePresent = true;
        snapshot.desktopAssistUiQuadHeader = uiQuad;
        snapshot.desktopAssistUiQuadCaptured =
            fnvxr::engine::desktopAssistUiQuadHeaderIsComplete(uiQuad);
    }

    fnvxr::shared::SharedRuntimeState runtimeAfterUiQuad {};
    if (readRetailRuntimeState(runtimeAfterUiQuad))
    {
        snapshot.runtime = true;
        snapshot.runtimeState = runtimeAfterUiQuad;
    }
    else if (snapshot.runtimeBeforeUiQuad)
    {
        snapshot.runtime = true;
        snapshot.runtimeState = snapshot.runtimeStateBeforeUiQuad;
    }
    return snapshot;
}

float rotationDistanceDegrees(const float baseline[9], const float current[9])
{
    double dotProduct = 0.0;
    for (int index = 0; index < 9; ++index)
        dotProduct += static_cast<double>(baseline[index]) * static_cast<double>(current[index]);
    const double cosine = (std::max)(-1.0, (std::min)(1.0, (dotProduct - 1.0) * 0.5));
    return static_cast<float>(std::acos(cosine) * 180.0 / static_cast<double>(Pi));
}

float positionDistance(const float baseline[3], const float current[3])
{
    const float x = baseline[0] - current[0];
    const float y = baseline[1] - current[1];
    const float z = baseline[2] - current[2];
    return std::sqrt(x * x + y * y + z * z);
}

bool poseSequenceMatchesCurrentPhase(
    const RunResult& result,
    std::uint32_t observedSequence)
{
    const std::uint32_t first = result.currentPhaseFirstPublishedPoseSequence;
    const std::uint32_t last = result.currentPhaseLastPublishedPoseSequence;
    if (first == 0u || last == 0u || observedSequence == 0u
        || (first & 1u) != 0u || (last & 1u) != 0u || (observedSequence & 1u) != 0u)
    {
        return false;
    }

    // The pose mapping uses an unsigned modulo-2^32 publication sequence.
    // A phase spans only a short forward range, including across a possible
    // wrap, so an old same-epoch pose cannot be mistaken for this phase.
    const std::uint32_t phaseSpan = last - first;
    const std::uint32_t observedOffset = observedSequence - first;
    return phaseSpan < 0x80000000u && observedOffset <= phaseSpan;
}

float requestedRotationMagnitudeDegrees(const fnvxr::assist::PoseStep& step)
{
    return std::sqrt(
        step.yawDegrees * step.yawDegrees
        + step.pitchDegrees * step.pitchDegrees
        + step.rollDegrees * step.rollDegrees);
}

float minimumExpectedCameraRotationDegrees(const fnvxr::assist::PoseStep& step)
{
    return (std::max)(
        MinimumCameraRotationResponseDegrees,
        requestedRotationMagnitudeDegrees(step) * MinimumExpectedRotationResponseRatio);
}

ObservedRetailMode classifyObservedRetailMode(const fnvxr::shared::SharedRuntimeState& runtime)
{
    if (fnvxr::shared::runtimeGameplayPhase(
            runtime.phase,
            runtime.menuBits,
            runtime.showroomActive))
    {
        return ObservedRetailMode::Gameplay;
    }
    if (fnvxr::shared::runtimeUiActive(
            runtime.phase,
            runtime.menuBits,
            runtime.showroomActive))
    {
        return ObservedRetailMode::Ui;
    }
    return ObservedRetailMode::Unknown;
}

bool desktopAssistUiQuadMatchesRuntime(
    const fnvxr::shared::SharedDesktopAssistUiQuadHeader& uiQuad,
    const fnvxr::shared::SharedRuntimeState& runtime)
{
    return uiQuad.runtimeStateSample == runtime.frame
        && uiQuad.runtimePhase == runtime.phase
        && uiQuad.runtimeMenuBits == runtime.menuBits
        && classifyObservedRetailMode(runtime) == ObservedRetailMode::Ui;
}

void observeRetail(
    std::size_t stepIndex,
    const RetailSnapshot& snapshot,
    const Options& options,
    RunResult& result)
{
    StepMetrics& metrics = result.steps[stepIndex];
    Baseline& baseline = result.observation.baseline;
    ObservationState& observation = result.observation;
    observation.desktopAssistStateObserved = observation.desktopAssistStateObserved
        || snapshot.desktopAssistStatePresent;
    const bool desktopAssistCurrentPhasePose = snapshot.desktopAssistCameraApplied
        && result.expectedPoseProducerEpoch != 0u
        && snapshot.desktopAssistState.poseProducerEpoch
            == result.expectedPoseProducerEpoch
        && poseSequenceMatchesCurrentPhase(
            result,
            snapshot.desktopAssistState.poseSequence);
    observation.desktopAssistCurrentPhasePoseAppliedObserved =
        observation.desktopAssistCurrentPhasePoseAppliedObserved
        || desktopAssistCurrentPhasePose;
    observation.desktopAssistUiQuadStateObserved = observation.desktopAssistUiQuadStateObserved
        || snapshot.desktopAssistUiQuadStatePresent;
    const bool desktopAssistUiQuadCurrentPhasePose = snapshot.desktopAssistUiQuadCaptured
        && fnvxr::engine::desktopAssistUiQuadHeaderIsComplete(
            snapshot.desktopAssistUiQuadHeader)
        && result.expectedPoseProducerEpoch != 0u
        && snapshot.desktopAssistUiQuadHeader.poseProducerEpoch
            == result.expectedPoseProducerEpoch
        && poseSequenceMatchesCurrentPhase(
            result,
            fnvxr::shared::sequencedValueBits(
                snapshot.desktopAssistUiQuadHeader.poseSequence));
    observation.desktopAssistUiQuadCurrentPhasePoseObserved =
        observation.desktopAssistUiQuadCurrentPhasePoseObserved
        || desktopAssistUiQuadCurrentPhasePose;

    if (observation.cameraSource == CameraEvidenceSource::None)
    {
        if (desktopAssistCurrentPhasePose)
            observation.cameraSource = CameraEvidenceSource::DesktopAssistLocalCamera;
        else if (!options.requireDesktopAssist && snapshot.player && snapshot.camera)
            observation.cameraSource = CameraEvidenceSource::WorldCamera;
    }

    const bool useDesktopAssist = observation.cameraSource
        == CameraEvidenceSource::DesktopAssistLocalCamera
        && desktopAssistCurrentPhasePose;
    const bool useWorldCamera = observation.cameraSource
        == CameraEvidenceSource::WorldCamera
        && snapshot.player
        && snapshot.camera;

    const float* bodyRotation = nullptr;
    const float* bodyPosition = nullptr;
    const float* cameraRotation = nullptr;
    const float* cameraPosition = nullptr;
    if (useDesktopAssist)
    {
        bodyRotation = snapshot.desktopAssistState.bodyRootWorldRot;
        bodyPosition = snapshot.desktopAssistState.bodyRootWorldPos;
        cameraRotation = snapshot.desktopAssistState.cameraLocalRot;
        cameraPosition = snapshot.desktopAssistState.cameraLocalPos;
        ++metrics.desktopAssistSamples;
        ++metrics.desktopAssistCurrentPhasePoseSamples;
    }
    else if (useWorldCamera)
    {
        bodyRotation = snapshot.playerState.playerWorldRot;
        bodyPosition = snapshot.playerState.playerWorldPos;
        cameraRotation = snapshot.cameraState.worldRot;
        cameraPosition = snapshot.cameraState.worldPos;
    }

    if (bodyRotation && bodyPosition)
    {
        ++metrics.bodySamples;
        if (stepIndex == 0 && !baseline.body)
        {
            baseline.body = true;
            std::memcpy(baseline.bodyRotation, bodyRotation, sizeof(baseline.bodyRotation));
            std::memcpy(baseline.bodyPosition, bodyPosition, sizeof(baseline.bodyPosition));
        }
        if (baseline.body)
        {
            metrics.maximumBodyRotationDegrees = (std::max)(
                metrics.maximumBodyRotationDegrees,
                rotationDistanceDegrees(baseline.bodyRotation, bodyRotation));
            metrics.maximumBodyPositionDelta = (std::max)(
                metrics.maximumBodyPositionDelta,
                positionDistance(baseline.bodyPosition, bodyPosition));
        }
    }

    if (cameraRotation && cameraPosition)
    {
        ++metrics.cameraSamples;
        if (stepIndex == 0 && !baseline.camera)
        {
            baseline.camera = true;
            std::memcpy(baseline.cameraRotation, cameraRotation, sizeof(baseline.cameraRotation));
            std::memcpy(baseline.cameraPosition, cameraPosition, sizeof(baseline.cameraPosition));
        }
        if (baseline.camera)
        {
            const float cameraRotationDegrees = rotationDistanceDegrees(
                baseline.cameraRotation,
                cameraRotation);
            if (!metrics.representativeCameraRotationValid
                || cameraRotationDegrees > metrics.maximumCameraRotationDegrees)
            {
                metrics.representativeCameraRotationValid = true;
                std::memcpy(
                    metrics.representativeCameraRotation,
                    cameraRotation,
                    sizeof(metrics.representativeCameraRotation));
            }
            metrics.maximumCameraRotationDegrees = (std::max)(
                metrics.maximumCameraRotationDegrees,
                cameraRotationDegrees);
            metrics.maximumCameraPositionDelta = (std::max)(
                metrics.maximumCameraPositionDelta,
                positionDistance(baseline.cameraPosition, cameraPosition));
        }
    }

    if (!snapshot.runtime)
        return;

    ++metrics.runtimeSamples;
    const ObservedRetailMode mode = classifyObservedRetailMode(snapshot.runtimeState);
    const bool uiQuadMatchesCurrentRuntime = desktopAssistUiQuadCurrentPhasePose
        && desktopAssistUiQuadMatchesRuntime(
            snapshot.desktopAssistUiQuadHeader,
            snapshot.runtimeState);
    const bool uiQuadMatchesRuntimeBeforeRead = desktopAssistUiQuadCurrentPhasePose
        && snapshot.runtimeBeforeUiQuad
        && desktopAssistUiQuadMatchesRuntime(
            snapshot.desktopAssistUiQuadHeader,
            snapshot.runtimeStateBeforeUiQuad);
    const bool uiQuadMatchedRuntime = uiQuadMatchesCurrentRuntime
        || uiQuadMatchesRuntimeBeforeRead;
    observation.desktopAssistUiQuadMatchedRuntimeObserved =
        observation.desktopAssistUiQuadMatchedRuntimeObserved
        || uiQuadMatchedRuntime;
    metrics.sawUi = metrics.sawUi || mode == ObservedRetailMode::Ui;
    metrics.sawGameplay = metrics.sawGameplay || mode == ObservedRetailMode::Gameplay;
    if (mode == ObservedRetailMode::Ui
        && result.observation.lastMode == ObservedRetailMode::Gameplay)
    {
        result.observation.sawGameplayToUi = true;
        result.observation.desktopAssistUiQuadDuringActiveUiVisit = false;
        result.observation.desktopAssistUiQuadExitPendingInvalidation = false;
    }
    if (mode == ObservedRetailMode::Ui
        && uiQuadMatchedRuntime
        && !result.observation.desktopAssistUiQuadDuringActiveUiVisit)
    {
        const bool pixelsVerified = snapshot.desktopAssistUiQuadPixelsVerified
            || desktopAssistUiQuadPixelsMatchHeader(
                snapshot.desktopAssistUiQuadHeader);
        observation.desktopAssistUiQuadPixelsVerifiedObserved =
            observation.desktopAssistUiQuadPixelsVerifiedObserved
            || pixelsVerified;
        result.observation.desktopAssistUiQuadDuringActiveUiVisit = pixelsVerified;
    }
    if (mode == ObservedRetailMode::Gameplay
        && result.observation.lastMode == ObservedRetailMode::Ui)
    {
        result.observation.sawUiToGameplay = true;
        result.observation.desktopAssistUiQuadExitPendingInvalidation =
            result.observation.desktopAssistUiQuadDuringActiveUiVisit;
        result.observation.desktopAssistUiQuadDuringActiveUiVisit = false;
    }
    if (mode == ObservedRetailMode::Gameplay
        && result.observation.desktopAssistUiQuadExitPendingInvalidation)
    {
        const bool invalidatedAfterUi = snapshot.desktopAssistUiQuadStatePresent
            && !snapshot.desktopAssistUiQuadCaptured
            && snapshot.desktopAssistUiQuadHeader.captureFailure != 0u;
        if (invalidatedAfterUi)
        {
            result.observation.desktopAssistUiQuadInvalidatedAfterUiObserved = true;
            result.observation.desktopAssistUiQuadTransitionObserved = true;
            result.observation.desktopAssistUiQuadExitPendingInvalidation = false;
        }
    }
    if (mode != ObservedRetailMode::Unknown)
        result.observation.lastMode = mode;
}

bool headTranslationBodyDecoupled(const RunResult& result)
{
    if (!result.observation.baseline.body || !result.observation.baseline.camera)
        return false;

    bool allTranslationStepsSampled = true;
    float maximumBodyPosition = 0.0f;
    for (std::size_t index = 0; index < fnvxr::assist::HeadBodyScenario.size(); ++index)
    {
        const fnvxr::assist::PoseStep& step = fnvxr::assist::HeadBodyScenario[index];
        if (!step.expectsTranslationResponse)
            continue;
        const StepMetrics& metrics = result.steps[index];
        allTranslationStepsSampled = allTranslationStepsSampled
            && metrics.bodySamples != 0
            && metrics.cameraSamples != 0;
        maximumBodyPosition = (std::max)(
            maximumBodyPosition,
            metrics.maximumBodyPositionDelta);
    }
    return allTranslationStepsSampled
        && maximumBodyPosition <= BodyPositionToleranceUnits;
}

bool headBodyDecoupled(const RunResult& result)
{
    if (!result.observation.baseline.body || !result.observation.baseline.camera)
        return false;

    bool allHeadMotionStepsSampled = true;
    float maximumBodyRotation = 0.0f;
    float maximumCameraRotation = 0.0f;
    for (std::size_t index = 0; index < fnvxr::assist::HeadBodyScenario.size(); ++index)
    {
        const fnvxr::assist::PoseStep& step = fnvxr::assist::HeadBodyScenario[index];
        const StepMetrics& metrics = result.steps[index];
        if (step.expectsRotationResponse)
        {
            allHeadMotionStepsSampled = allHeadMotionStepsSampled
                && metrics.bodySamples != 0
                && metrics.cameraSamples != 0;
            maximumBodyRotation = (std::max)(
                maximumBodyRotation,
                metrics.maximumBodyRotationDegrees);
            maximumCameraRotation = (std::max)(
                maximumCameraRotation,
                metrics.maximumCameraRotationDegrees);
        }
        if (step.expectsTranslationResponse)
        {
            // A lean may be intentionally disabled in the first
            // rotation-only assist profile, but its body-root observation is
            // still mandatory. This catches the more dangerous outcome: head
            // translation leaking into player/body world position.
            allHeadMotionStepsSampled = allHeadMotionStepsSampled
                && metrics.bodySamples != 0
                && metrics.cameraSamples != 0;
        }
    }
    return allHeadMotionStepsSampled
        && maximumBodyRotation <= BodyRotationToleranceDegrees
        && maximumCameraRotation >= MinimumCameraRotationResponseDegrees
        && headTranslationBodyDecoupled(result);
}

bool desktopAssistHeadBodyDecoupled(const RunResult& result)
{
    if (result.observation.cameraSource
        != CameraEvidenceSource::DesktopAssistLocalCamera
        || !headBodyDecoupled(result))
    {
        return false;
    }
    for (std::size_t index = 0; index < fnvxr::assist::HeadBodyScenario.size(); ++index)
    {
        const fnvxr::assist::PoseStep& step = fnvxr::assist::HeadBodyScenario[index];
        const StepMetrics& metrics = result.steps[index];
        if ((step.expectsRotationResponse || step.expectsTranslationResponse)
            && (metrics.desktopAssistSamples == 0u
                || metrics.desktopAssistCurrentPhasePoseSamples == 0u))
        {
            return false;
        }
        if (step.expectsRotationResponse
            && (!metrics.representativeCameraRotationValid
                || metrics.maximumCameraRotationDegrees
                    < minimumExpectedCameraRotationDegrees(step)))
        {
            return false;
        }
    }

    for (std::size_t first = 0; first < fnvxr::assist::HeadBodyScenario.size(); ++first)
    {
        const fnvxr::assist::PoseStep& firstStep = fnvxr::assist::HeadBodyScenario[first];
        if (!firstStep.expectsRotationResponse)
            continue;
        for (std::size_t second = first + 1; second < fnvxr::assist::HeadBodyScenario.size(); ++second)
        {
            const fnvxr::assist::PoseStep& secondStep = fnvxr::assist::HeadBodyScenario[second];
            if (!secondStep.expectsRotationResponse)
                continue;
            if (rotationDistanceDegrees(
                    result.steps[first].representativeCameraRotation,
                    result.steps[second].representativeCameraRotation)
                < MinimumDistinctPhaseCameraRotationDegrees)
            {
                return false;
            }
        }
    }
    return true;
}

bool translationResponseObserved(const RunResult& result)
{
    if (!headTranslationBodyDecoupled(result))
        return false;
    for (std::size_t index = 0; index < fnvxr::assist::HeadBodyScenario.size(); ++index)
    {
        if (fnvxr::assist::HeadBodyScenario[index].expectsTranslationResponse
            && result.steps[index].bodySamples != 0
            && result.steps[index].cameraSamples != 0
            && result.steps[index].maximumBodyPositionDelta
                <= BodyPositionToleranceUnits
            && result.steps[index].maximumCameraPositionDelta
                >= MinimumCameraTranslationResponse)
        {
            return true;
        }
    }
    return false;
}

bool runtimeUiTransitionObserved(const RunResult& result)
{
    return result.observation.sawGameplayToUi && result.observation.sawUiToGameplay;
}

bool desktopAssistUiQuadTransitionObserved(const RunResult& result)
{
    return runtimeUiTransitionObserved(result)
        && result.observation.desktopAssistUiQuadTransitionObserved;
}

std::string makeReport(const Options& options, const RunResult& result)
{
    const bool scenarioValid = options.emitTrackedPropControllers
        ? fnvxr::assist::trackedPropScenarioIsValid()
        : fnvxr::assist::headBodyScenarioIsValid();
    const bool syntheticPosePass = scenarioValid
        && result.actualPoseWrites >= result.expectedPoseWrites
        && result.allEyeCentersValid;
    const bool retailObservationAvailable = result.observation.baseline.body
        && result.observation.baseline.camera;
    const bool headBodyPass = headBodyDecoupled(result);
    const bool headTranslationBodyPass = headTranslationBodyDecoupled(result);
    const bool desktopAssistHeadBodyPass = desktopAssistHeadBodyDecoupled(result);
    const bool translationPass = translationResponseObserved(result);
    const bool uiPass = runtimeUiTransitionObserved(result);
    const bool uiQuadPass = desktopAssistUiQuadTransitionObserved(result);
    bool requestedPass = syntheticPosePass;
    if (options.requireRetailObservation)
        requestedPass = requestedPass && headBodyPass;
    if (options.requireDesktopAssist)
        requestedPass = requestedPass && desktopAssistHeadBodyPass;
    if (options.requireTranslationResponse)
        requestedPass = requestedPass && translationPass;
    if (options.expectUiTransition)
        requestedPass = requestedPass && uiPass;
    if (options.requireUiQuadTransition)
        requestedPass = requestedPass && uiQuadPass;

    std::ostringstream report;
    report << std::fixed << std::setprecision(4);
    report << "{\n";
    report << "  \"schema\": \"fnvxr-assist-report-v5\",\n";
    report << "  \"scenario\": \"head-body\",\n";
    report << "  \"trackedPropControllers\": "
           << (options.emitTrackedPropControllers ? "true" : "false") << ",\n";
    report << "  \"scope\": \"synthetic pose producer plus read-only desktop observation; this executable does not open OpenXR, require a headset, activate a hook, or activate a renderer\",\n";
    report << "  \"syntheticPosePass\": " << (syntheticPosePass ? "true" : "false") << ",\n";
    report << "  \"retailObservationAvailable\": " << (retailObservationAvailable ? "true" : "false") << ",\n";
    report << "  \"cameraEvidenceSource\": \""
           << cameraEvidenceSourceName(result.observation.cameraSource) << "\",\n";
    report << "  \"expectedPoseProducerEpoch\": "
           << result.expectedPoseProducerEpoch << ",\n";
    report << "  \"desktopAssistStateObserved\": "
           << (result.observation.desktopAssistStateObserved ? "true" : "false") << ",\n";
    report << "  \"desktopAssistCurrentPhasePoseAppliedObserved\": "
           << (result.observation.desktopAssistCurrentPhasePoseAppliedObserved ? "true" : "false") << ",\n";
    report << "  \"desktopAssistUiQuadStateObserved\": "
           << (result.observation.desktopAssistUiQuadStateObserved ? "true" : "false") << ",\n";
    report << "  \"desktopAssistUiQuadCurrentPhasePoseObserved\": "
           << (result.observation.desktopAssistUiQuadCurrentPhasePoseObserved ? "true" : "false") << ",\n";
    report << "  \"desktopAssistUiQuadMatchedRuntimeObserved\": "
           << (result.observation.desktopAssistUiQuadMatchedRuntimeObserved ? "true" : "false") << ",\n";
    report << "  \"desktopAssistUiQuadPixelsVerifiedObserved\": "
           << (result.observation.desktopAssistUiQuadPixelsVerifiedObserved ? "true" : "false") << ",\n";
    report << "  \"desktopAssistUiQuadInvalidatedAfterUiObserved\": "
           << (result.observation.desktopAssistUiQuadInvalidatedAfterUiObserved ? "true" : "false") << ",\n";
    report << "  \"headBodyDecoupled\": " << (headBodyPass ? "true" : "false") << ",\n";
    report << "  \"headTranslationBodyDecoupled\": "
           << (headTranslationBodyPass ? "true" : "false") << ",\n";
    report << "  \"desktopAssistHeadBodyDecoupled\": "
           << (desktopAssistHeadBodyPass ? "true" : "false") << ",\n";
    report << "  \"translationResponseObserved\": " << (translationPass ? "true" : "false") << ",\n";
    report << "  \"runtimeUiTransitionObserved\": " << (uiPass ? "true" : "false") << ",\n";
    report << "  \"desktopAssistUiQuadTransitionObserved\": "
           << (uiQuadPass ? "true" : "false") << ",\n";
    report << "  \"requestedRetailProof\": " << (options.requireRetailObservation ? "true" : "false") << ",\n";
    report << "  \"requestedDesktopAssistProof\": " << (options.requireDesktopAssist ? "true" : "false") << ",\n";
    report << "  \"requested6DofProof\": " << (options.requireTranslationResponse ? "true" : "false") << ",\n";
    report << "  \"requestedUiTransitionProof\": " << (options.expectUiTransition ? "true" : "false") << ",\n";
    report << "  \"requestedUiQuadTransitionProof\": "
           << (options.requireUiQuadTransition ? "true" : "false") << ",\n";
    report << "  \"requestedChecksPass\": " << (requestedPass ? "true" : "false") << ",\n";
    report << "  \"thresholds\": {\"bodyRotationDegrees\": " << BodyRotationToleranceDegrees
           << ", \"bodyPositionUnits\": " << BodyPositionToleranceUnits
           << ", \"cameraRotationDegrees\": " << MinimumCameraRotationResponseDegrees
           << ", \"cameraTranslationUnits\": " << MinimumCameraTranslationResponse << "},\n";
    report << "  \"steps\": [\n";
    for (std::size_t index = 0; index < fnvxr::assist::HeadBodyScenario.size(); ++index)
    {
        const fnvxr::assist::PoseStep& step = fnvxr::assist::HeadBodyScenario[index];
        const StepMetrics& metrics = result.steps[index];
        report << "    {\"name\": \"" << step.name << "\", \"yawDegrees\": " << step.yawDegrees
               << ", \"pitchDegrees\": " << step.pitchDegrees
               << ", \"rollDegrees\": " << step.rollDegrees
                << ", \"positionMeters\": [" << step.positionX << ", " << step.positionY << ", " << step.positionZ << "]"
               << ", \"controllerPositionMeters\": [" << step.controllerPositionX << ", " << step.controllerPositionY << ", " << step.controllerPositionZ << "]"
               << ", \"controllerYawDegrees\": " << step.controllerYawDegrees
               << ", \"controllerProbe\": " << (step.expectsControllerResponse ? "true" : "false")
               << ", \"poseWrites\": " << metrics.poseWrites
               << ", \"bodySamples\": " << metrics.bodySamples
               << ", \"cameraSamples\": " << metrics.cameraSamples
               << ", \"desktopAssistSamples\": " << metrics.desktopAssistSamples
               << ", \"desktopAssistCurrentPhasePoseSamples\": "
               << metrics.desktopAssistCurrentPhasePoseSamples
               << ", \"runtimeSamples\": " << metrics.runtimeSamples
               << ", \"firstPublishedPoseSequence\": " << metrics.firstPublishedPoseSequence
               << ", \"lastPublishedPoseSequence\": " << metrics.lastPublishedPoseSequence
               << ", \"maximumBodyRotationDegrees\": " << metrics.maximumBodyRotationDegrees
               << ", \"maximumCameraRotationDegrees\": " << metrics.maximumCameraRotationDegrees
               << ", \"maximumBodyPositionDelta\": " << metrics.maximumBodyPositionDelta
               << ", \"maximumCameraPositionDelta\": " << metrics.maximumCameraPositionDelta
               << ", \"sawUi\": " << (metrics.sawUi ? "true" : "false")
               << ", \"sawGameplay\": " << (metrics.sawGameplay ? "true" : "false") << "}";
        if (index + 1 != fnvxr::assist::HeadBodyScenario.size())
            report << ',';
        report << "\n";
    }
    report << "  ],\n";
    report << "  \"baseline\": {\"body\": " << (result.observation.baseline.body ? "true" : "false")
           << ", \"camera\": " << (result.observation.baseline.camera ? "true" : "false") << "},\n";
    report << "  \"runtimeTransitions\": {\"gameplayToUi\": "
           << (result.observation.sawGameplayToUi ? "true" : "false")
           << ", \"uiToGameplay\": " << (result.observation.sawUiToGameplay ? "true" : "false")
           << ", \"uiQuadPixelVerifiedAndInvalidated\": "
           << (result.observation.desktopAssistUiQuadTransitionObserved ? "true" : "false") << "}\n";
    report << "}\n";
    return report.str();
}

bool writeReport(const Options& options, const std::string& report)
{
    if (options.reportPath.empty())
        return true;
    std::ofstream output(options.reportPath, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        std::cerr << "could not write report: " << options.reportPath << "\n";
        return false;
    }
    output << report;
    return static_cast<bool>(output);
}

bool reportPassesRequestedChecks(const Options& options, const RunResult& result)
{
    const bool syntheticPosePass = fnvxr::assist::headBodyScenarioIsValid()
        && result.actualPoseWrites >= result.expectedPoseWrites
        && result.allEyeCentersValid;
    if (!syntheticPosePass)
        return false;
    if (options.requireRetailObservation && !headBodyDecoupled(result))
        return false;
    if (options.requireDesktopAssist && !desktopAssistHeadBodyDecoupled(result))
        return false;
    if (options.requireTranslationResponse && !translationResponseObserved(result))
        return false;
    if (options.expectUiTransition && !runtimeUiTransitionObserved(result))
        return false;
    return !options.requireUiQuadTransition || desktopAssistUiQuadTransitionObserved(result);
}

void setIdentityMatrix(float matrix[9])
{
    std::memset(matrix, 0, sizeof(float) * 9);
    matrix[0] = 1.0f;
    matrix[4] = 1.0f;
    matrix[8] = 1.0f;
}

void setAxisRotationMatrix(float matrix[9], int axis, float degrees)
{
    setIdentityMatrix(matrix);
    const float radians = degrees * Pi / 180.0f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    if (axis == 0)
    {
        matrix[4] = cosine;
        matrix[5] = -sine;
        matrix[7] = sine;
        matrix[8] = cosine;
    }
    else if (axis == 1)
    {
        matrix[0] = cosine;
        matrix[2] = sine;
        matrix[6] = -sine;
        matrix[8] = cosine;
    }
    else
    {
        matrix[0] = cosine;
        matrix[1] = -sine;
        matrix[3] = sine;
        matrix[4] = cosine;
    }
}

fnvxr::shared::SharedDesktopAssistUiQuadHeader completeUiQuadHeader(
    std::uint64_t runtimeFrame,
    std::uint64_t poseProducerEpoch)
{
    fnvxr::shared::SharedDesktopAssistUiQuadHeader header {};
    header.magic = fnvxr::shared::DesktopAssistUiQuadSharedMagic;
    header.version = fnvxr::shared::DesktopAssistUiQuadSharedVersion;
    header.headerBytes = sizeof(header);
    header.sequence = 2;
    header.flags = fnvxr::engine::DesktopAssistUiQuadRequiredFlags;
    header.width = 1;
    header.height = 1;
    header.pitchBytes = 4;
    header.format = fnvxr::engine::DesktopAssistUiQuadPixelFormatX8R8G8B8;
    header.runtimeStateSample = runtimeFrame;
    header.poseFrame = 9u;
    header.poseSequence = 2;
    header.runtimePhase = fnvxr::shared::RuntimePhaseMenu;
    header.runtimeMenuBits = fnvxr::shared::RuntimeGenericMenuBit;
    header.pixelHash = 1u;
    header.captureFailure = 0u;
    header.nonBlackSampleCount = 1u;
    header.poseProducerEpoch = poseProducerEpoch;
    header.captureOrdinal = 1u;
    return header;
}

bool runSelfTest()
{
    if (!fnvxr::assist::headBodyScenarioIsValid())
        return false;

    const float yawRadians = fnvxr::assist::ExpectedYawDegrees * Pi / 180.0f;
    const float identity[9] { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };
    const float yaw[9] {
        std::cos(yawRadians), 0.0f, std::sin(yawRadians),
        0.0f, 1.0f, 0.0f,
        -std::sin(yawRadians), 0.0f, std::cos(yawRadians),
    };
    if (std::fabs(rotationDistanceDegrees(identity, yaw) - fnvxr::assist::ExpectedYawDegrees) > 0.01f)
        return false;

    RunResult independent {};
    independent.observation.baseline.body = true;
    independent.observation.baseline.camera = true;
    setIdentityMatrix(independent.observation.baseline.bodyRotation);
    setIdentityMatrix(independent.observation.baseline.cameraRotation);
    for (std::size_t index = 0; index < fnvxr::assist::HeadBodyScenario.size(); ++index)
    {
        const fnvxr::assist::PoseStep& step = fnvxr::assist::HeadBodyScenario[index];
        StepMetrics& metrics = independent.steps[index];
        if (step.expectsRotationResponse)
        {
            metrics.bodySamples = 1;
            metrics.cameraSamples = 1;
            metrics.desktopAssistSamples = 1;
            metrics.desktopAssistCurrentPhasePoseSamples = 1;
            metrics.firstPublishedPoseSequence = static_cast<std::uint32_t>(2u + index * 2u);
            metrics.lastPublishedPoseSequence = metrics.firstPublishedPoseSequence;
            metrics.maximumBodyRotationDegrees = 0.0f;
            metrics.maximumCameraRotationDegrees = requestedRotationMagnitudeDegrees(step);
            metrics.representativeCameraRotationValid = true;
            if (step.yawDegrees != 0.0f)
                setAxisRotationMatrix(metrics.representativeCameraRotation, 1, step.yawDegrees);
            else if (step.pitchDegrees != 0.0f)
                setAxisRotationMatrix(metrics.representativeCameraRotation, 0, step.pitchDegrees);
            else
                setAxisRotationMatrix(metrics.representativeCameraRotation, 2, step.rollDegrees);
        }
        if (step.expectsTranslationResponse)
        {
            metrics.bodySamples = 1;
            metrics.cameraSamples = 1;
            metrics.maximumCameraPositionDelta = fnvxr::assist::ExpectedTranslationMeters;
        }
    }
    independent.observation.sawGameplayToUi = true;
    independent.observation.sawUiToGameplay = true;
    if (!headBodyDecoupled(independent)
        || !headTranslationBodyDecoupled(independent)
        || !translationResponseObserved(independent)
        || !runtimeUiTransitionObserved(independent))
    {
        return false;
    }

    RunResult desktopAssist = independent;
    desktopAssist.observation.cameraSource = CameraEvidenceSource::DesktopAssistLocalCamera;
    for (std::size_t index = 0; index < fnvxr::assist::HeadBodyScenario.size(); ++index)
    {
        if (fnvxr::assist::HeadBodyScenario[index].expectsRotationResponse
            || fnvxr::assist::HeadBodyScenario[index].expectsTranslationResponse)
        {
            desktopAssist.steps[index].desktopAssistSamples = 1;
            desktopAssist.steps[index].desktopAssistCurrentPhasePoseSamples = 1;
        }
    }
    if (!desktopAssistHeadBodyDecoupled(desktopAssist))
        return false;
    RunResult collapsedDesktopAssist = desktopAssist;
    std::memcpy(
        collapsedDesktopAssist.steps[2].representativeCameraRotation,
        collapsedDesktopAssist.steps[1].representativeCameraRotation,
        sizeof(collapsedDesktopAssist.steps[2].representativeCameraRotation));
    if (desktopAssistHeadBodyDecoupled(collapsedDesktopAssist))
        return false;

    RunResult translatedDesktopAssist = desktopAssist;
    for (std::size_t index = 0; index < fnvxr::assist::HeadBodyScenario.size(); ++index)
    {
        if (fnvxr::assist::HeadBodyScenario[index].expectsTranslationResponse)
        {
            translatedDesktopAssist.steps[index].maximumBodyPositionDelta =
                BodyPositionToleranceUnits + 0.01f;
        }
    }
    if (headTranslationBodyDecoupled(translatedDesktopAssist)
        || desktopAssistHeadBodyDecoupled(translatedDesktopAssist))
        return false;

    // A retained desktop mapping from a previous synthetic run is not evidence
    // for the pose producer this invocation owns. Verify that source selection
    // rejects the old epoch and accepts the matching one.
    Options desktopOptions {};
    desktopOptions.requireDesktopAssist = true;
    RunResult epochBound {};
    epochBound.expectedPoseProducerEpoch = 42u;
    epochBound.currentPhaseFirstPublishedPoseSequence = 2u;
    epochBound.currentPhaseLastPublishedPoseSequence = 2u;
    RetailSnapshot epochSnapshot {};
    epochSnapshot.desktopAssistStatePresent = true;
    epochSnapshot.desktopAssistCameraApplied = true;
    epochSnapshot.desktopAssistState.poseProducerEpoch = 41u;
    epochSnapshot.desktopAssistState.poseSequence = 2u;
    setIdentityMatrix(epochSnapshot.desktopAssistState.bodyRootWorldRot);
    setIdentityMatrix(epochSnapshot.desktopAssistState.cameraLocalRot);
    observeRetail(0, epochSnapshot, desktopOptions, epochBound);
    if (epochBound.observation.cameraSource != CameraEvidenceSource::None
        || epochBound.observation.desktopAssistCurrentPhasePoseAppliedObserved)
    {
        return false;
    }
    epochSnapshot.desktopAssistState.poseProducerEpoch = 42u;
    epochSnapshot.desktopAssistState.poseSequence = 4u;
    observeRetail(0, epochSnapshot, desktopOptions, epochBound);
    if (epochBound.observation.cameraSource != CameraEvidenceSource::None
        || epochBound.observation.desktopAssistCurrentPhasePoseAppliedObserved)
    {
        return false;
    }
    epochBound.currentPhaseFirstPublishedPoseSequence = 4u;
    epochBound.currentPhaseLastPublishedPoseSequence = 4u;
    observeRetail(0, epochSnapshot, desktopOptions, epochBound);
    if (epochBound.observation.cameraSource != CameraEvidenceSource::DesktopAssistLocalCamera
        || !epochBound.observation.desktopAssistCurrentPhasePoseAppliedObserved
        || epochBound.steps[0].desktopAssistSamples != 1u)
    {
        return false;
    }

    // The UI proof must reject both a retained frame from another pose
    // producer and a frame whose recorded runtime sample does not surround
    // this observation. Only a current-producer menu capture may complete a
    // gameplay -> UI -> gameplay visit.
    Options uiOptions {};
    uiOptions.requireDesktopAssist = true;
    uiOptions.requireUiQuadTransition = true;
    RunResult uiEvidence {};
    uiEvidence.expectedPoseProducerEpoch = 42u;
    uiEvidence.currentPhaseFirstPublishedPoseSequence = 2u;
    uiEvidence.currentPhaseLastPublishedPoseSequence = 2u;
    RetailSnapshot gameplay {};
    gameplay.runtime = true;
    gameplay.runtimeState.frame = 100u;
    gameplay.runtimeState.phase = fnvxr::shared::RuntimePhaseGameplay;
    observeRetail(0, gameplay, uiOptions, uiEvidence);

    RetailSnapshot ui {};
    ui.runtime = true;
    ui.runtimeState.frame = 101u;
    ui.runtimeState.phase = fnvxr::shared::RuntimePhaseMenu;
    ui.runtimeState.menuBits = fnvxr::shared::RuntimeGenericMenuBit;
    ui.desktopAssistUiQuadStatePresent = true;
    ui.desktopAssistUiQuadCaptured = true;
    ui.desktopAssistUiQuadHeader = completeUiQuadHeader(101u, 41u);
    observeRetail(0, ui, uiOptions, uiEvidence);
    if (uiEvidence.observation.desktopAssistUiQuadCurrentPhasePoseObserved
        || uiEvidence.observation.desktopAssistUiQuadMatchedRuntimeObserved)
    {
        return false;
    }

    ui.desktopAssistUiQuadHeader.poseProducerEpoch = 42u;
    ui.desktopAssistUiQuadHeader.poseSequence = 4;
    observeRetail(0, ui, uiOptions, uiEvidence);
    if (uiEvidence.observation.desktopAssistUiQuadCurrentPhasePoseObserved
        || uiEvidence.observation.desktopAssistUiQuadMatchedRuntimeObserved)
    {
        return false;
    }
    ui.desktopAssistUiQuadHeader.poseSequence = 2;
    ui.desktopAssistUiQuadHeader.runtimeStateSample = 102u;
    observeRetail(0, ui, uiOptions, uiEvidence);
    if (uiEvidence.observation.desktopAssistUiQuadMatchedRuntimeObserved)
        return false;

    ui.desktopAssistUiQuadHeader.runtimeStateSample = 101u;
    ui.desktopAssistUiQuadPixelsVerified = true;
    observeRetail(0, ui, uiOptions, uiEvidence);
    if (!uiEvidence.observation.desktopAssistUiQuadCurrentPhasePoseObserved
        || !uiEvidence.observation.desktopAssistUiQuadMatchedRuntimeObserved
        || !uiEvidence.observation.desktopAssistUiQuadPixelsVerifiedObserved)
    {
        return false;
    }

    gameplay.runtimeState.frame = 102u;
    observeRetail(0, gameplay, uiOptions, uiEvidence);
    if (desktopAssistUiQuadTransitionObserved(uiEvidence))
        return false;
    gameplay.desktopAssistUiQuadStatePresent = true;
    gameplay.desktopAssistUiQuadCaptured = false;
    gameplay.desktopAssistUiQuadHeader.magic = fnvxr::shared::DesktopAssistUiQuadSharedMagic;
    gameplay.desktopAssistUiQuadHeader.version = fnvxr::shared::DesktopAssistUiQuadSharedVersion;
    gameplay.desktopAssistUiQuadHeader.captureFailure = 2u;
    observeRetail(0, gameplay, uiOptions, uiEvidence);
    if (!desktopAssistUiQuadTransitionObserved(uiEvidence))
        return false;

    RunResult coupled = independent;
    for (std::size_t index = 0; index < fnvxr::assist::HeadBodyScenario.size(); ++index)
    {
        if (fnvxr::assist::HeadBodyScenario[index].expectsRotationResponse)
            coupled.steps[index].maximumBodyRotationDegrees = fnvxr::assist::ExpectedYawDegrees;
    }
    return !headBodyDecoupled(coupled);
}
}

int main(int argc, char** argv)
{
    Options options {};
    if (!parseOptions(argc, argv, options))
    {
        printUsage();
        return EXIT_FAILURE;
    }
    if (!reportPathIsSafe(options))
        return EXIT_FAILURE;

    if (options.validateScenario)
    {
        const bool valid = fnvxr::assist::headBodyScenarioIsValid()
            && fnvxr::assist::trackedPropScenarioIsValid();
        std::cout << "{\"scenario\":\"head-body\",\"valid\":"
                  << (valid ? "true" : "false")
                  << ",\"steps\":" << fnvxr::assist::HeadBodyScenarioStepCount << "}\n";
        return valid ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (options.selfTest)
    {
        const bool passed = runSelfTest();
        std::cout << "{\"selfTest\":\"fnvxr-assist\",\"pass\":"
                  << (passed ? "true" : "false") << "}\n";
        return passed ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    PoseProducer producer;
    std::string producerError;
    if (!producer.open(producerError))
    {
        std::cerr << "assist pose producer unavailable: " << producerError << "\n";
        return EXIT_FAILURE;
    }

    RunResult result {};
    result.expectedPoseProducerEpoch = producer.producerEpoch();
    result.expectedPoseWrites = static_cast<std::uint64_t>(options.cycles)
        * static_cast<std::uint64_t>(fnvxr::assist::HeadBodyScenarioStepCount);

    for (int cycle = 0; cycle < options.cycles; ++cycle)
    {
        for (std::size_t stepIndex = 0; stepIndex < fnvxr::assist::HeadBodyScenario.size(); ++stepIndex)
        {
            const fnvxr::assist::PoseStep& step = fnvxr::assist::HeadBodyScenario[stepIndex];
            result.currentPhaseFirstPublishedPoseSequence = 0u;
            result.currentPhaseLastPublishedPoseSequence = 0u;
            std::cout << "{\"event\":\"fnvxrAssistPhase\",\"cycle\":" << (cycle + 1)
                      << ",\"phase\":\"" << step.name << "\"}\n";
            const ULONGLONG started = GetTickCount64();
            for (;;)
            {
                bool eyeCenterValid = false;
                PublishedPose publication {};
                if (!producer.publish(
                        step,
                        options.ipdMeters,
                        options.emitTrackedPropControllers,
                        eyeCenterValid,
                        publication))
                {
                    std::cerr << "assist pose publication failed during phase " << step.name << "\n";
                    return EXIT_FAILURE;
                }
                ++result.actualPoseWrites;
                ++result.steps[stepIndex].poseWrites;
                StepMetrics& metrics = result.steps[stepIndex];
                if (metrics.firstPublishedPoseSequence == 0u)
                    metrics.firstPublishedPoseSequence = publication.sequence;
                metrics.lastPublishedPoseSequence = publication.sequence;
                if (result.currentPhaseFirstPublishedPoseSequence == 0u)
                    result.currentPhaseFirstPublishedPoseSequence = publication.sequence;
                result.currentPhaseLastPublishedPoseSequence = publication.sequence;
                result.allEyeCentersValid = result.allEyeCentersValid && eyeCenterValid;
                if (options.observeRetail)
                    observeRetail(stepIndex, readRetailSnapshot(), options, result);

                if (GetTickCount64() - started >= static_cast<ULONGLONG>(options.stepMilliseconds))
                    break;
                Sleep(static_cast<DWORD>(options.periodMilliseconds));
            }
        }
    }

    // Expected writes count marks the minimum one complete observation per
    // phase; real-time runs intentionally publish more than one pose per step.
    result.expectedPoseWrites = static_cast<std::uint64_t>(options.cycles)
        * static_cast<std::uint64_t>(fnvxr::assist::HeadBodyScenarioStepCount);
    const std::string report = makeReport(options, result);
    std::cout << report;
    if (!writeReport(options, report))
        return EXIT_FAILURE;
    return reportPassesRequestedChecks(options, result) ? EXIT_SUCCESS : 3;
}
