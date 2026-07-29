#include "../protocol/fnvxr_shared_state.h"
#include "../runtime/fnvxr_desktop_assist_ui_evidence.h"

#include <windows.h>

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

namespace
{
// This fixture deliberately exercises the real named-mapping reader in
// fnvxr_assist without launching Fallout, opening OpenXR, or requiring a
// headset. It is a mapping-contract test, not a claim of optical/tracking
// headset acceptance.
constexpr char RuntimeStateMappingName[] = "Local\\FNVXR_Runtime_State";
constexpr DWORD UiPhaseStartMilliseconds = 750u;
constexpr DWORD UiPhaseEndMilliseconds = 2800u;
constexpr DWORD AssistTimeoutMilliseconds = 20000u;

class NamedMapping final
{
public:
    ~NamedMapping()
    {
        if (mView)
            UnmapViewOfFile(mView);
        if (mHandle)
            CloseHandle(mHandle);
    }

    bool create(const char* name, std::size_t bytes)
    {
        if (!name || name[0] == '\0' || bytes == 0u
            || bytes > static_cast<std::size_t>(0xffffffffu))
        {
            return false;
        }

        mHandle = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            0u,
            static_cast<DWORD>(bytes),
            name);
        if (!mHandle)
            return false;
        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            CloseHandle(mHandle);
            mHandle = nullptr;
            return false;
        }

        mView = MapViewOfFile(mHandle, FILE_MAP_ALL_ACCESS, 0u, 0u, bytes);
        if (!mView)
        {
            CloseHandle(mHandle);
            mHandle = nullptr;
            return false;
        }
        return true;
    }

    template <typename T>
    T* as() const noexcept
    {
        return static_cast<T*>(mView);
    }

    std::uint8_t* bytes() const noexcept
    {
        return static_cast<std::uint8_t*>(mView);
    }

private:
    HANDLE mHandle = nullptr;
    void* mView = nullptr;
};

class TemporaryReport final
{
public:
    ~TemporaryReport()
    {
        if (mPath[0] != '\0')
            DeleteFileA(mPath);
    }

    bool create()
    {
        char directory[MAX_PATH] {};
        const DWORD directoryLength = GetTempPathA(MAX_PATH, directory);
        if (directoryLength == 0u || directoryLength >= MAX_PATH)
            return false;
        return GetTempFileNameA(directory, "fxa", 0u, mPath) != 0u;
    }

    const char* path() const noexcept
    {
        return mPath;
    }

private:
    char mPath[MAX_PATH] {};
};

bool readStablePose(fnvxr::shared::SharedVrPoseState& result)
{
    HANDLE mapping = OpenFileMappingA(
        FILE_MAP_READ,
        FALSE,
        fnvxr::shared::VrPoseSharedMappingName);
    if (!mapping)
        return false;

    const auto* state = static_cast<const fnvxr::shared::SharedVrPoseState*>(
        MapViewOfFile(mapping, FILE_MAP_READ, 0u, 0u, sizeof(result)));
    if (!state)
    {
        CloseHandle(mapping);
        return false;
    }

    bool stable = false;
    for (int attempt = 0; attempt < 3 && !stable; ++attempt)
    {
        const LONG before = state->sequence;
        if ((fnvxr::shared::sequencedValueBits(before) & 1u) != 0u)
            continue;
        MemoryBarrier();
        std::memcpy(&result, state, sizeof(result));
        MemoryBarrier();
        const LONG after = state->sequence;
        stable = before == after
            && (fnvxr::shared::sequencedValueBits(after) & 1u) == 0u;
    }

    UnmapViewOfFile(state);
    CloseHandle(mapping);
    return stable;
}

void setIdentity(float matrix[9])
{
    std::memset(matrix, 0, sizeof(float) * 9u);
    matrix[0] = 1.0f;
    matrix[4] = 1.0f;
    matrix[8] = 1.0f;
}

void quaternionToMatrix(const float quaternion[4], float result[9])
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

    result[0] = 1.0f - 2.0f * (yy + zz);
    result[1] = 2.0f * (xy - wz);
    result[2] = 2.0f * (xz + wy);
    result[3] = 2.0f * (xy + wz);
    result[4] = 1.0f - 2.0f * (xx + zz);
    result[5] = 2.0f * (yz - wx);
    result[6] = 2.0f * (xz - wy);
    result[7] = 2.0f * (yz + wx);
    result[8] = 1.0f - 2.0f * (xx + yy);
}

LONG longFromBits(std::uint32_t bits)
{
    LONG result = 0;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

enum class RetailMode
{
    Unknown,
    Gameplay,
    Ui,
};

class SyntheticRetailPublisher final
{
public:
    SyntheticRetailPublisher(
        fnvxr::shared::SharedDesktopAssistState* desktopAssist,
        fnvxr::shared::SharedRuntimeState* runtime,
        std::uint8_t* uiQuad,
        bool injectBodyTranslationLeak = false,
        bool requireTrackedProp = false)
        : mDesktopAssist(desktopAssist)
        , mRuntime(runtime)
        , mUiQuad(uiQuad)
        , mInjectBodyTranslationLeak(injectBodyTranslationLeak)
        , mRequireTrackedProp(requireTrackedProp)
    {
    }

    ~SyntheticRetailPublisher()
    {
        stop();
    }

    bool initialize()
    {
        if (!mDesktopAssist || !mRuntime || !mUiQuad)
            return false;
        std::memset(mDesktopAssist, 0, sizeof(*mDesktopAssist));
        std::memset(mRuntime, 0, sizeof(*mRuntime));
        std::memset(
            mUiQuad,
            0,
            sizeof(fnvxr::shared::SharedDesktopAssistUiQuadHeader));
        return writeInvalidUiQuad(1u);
    }

    void start()
    {
        mThread = std::thread(&SyntheticRetailPublisher::run, this);
    }

    void stop()
    {
        mStopRequested.store(true);
        if (mThread.joinable())
            mThread.join();
    }

    bool writeFailed() const noexcept
    {
        return mWriteFailed.load();
    }

    bool sawPose() const noexcept
    {
        return mSawPose.load();
    }

    bool sawTrackedPropGripAndAim() const noexcept
    {
        return mSawTrackedPropGripAndAim.load();
    }

    bool sawTrackedPropControllerTranslation() const noexcept
    {
        return mSawTrackedPropControllerTranslation.load();
    }

    bool sawTrackedPropControllerAim() const noexcept
    {
        return mSawTrackedPropControllerAim.load();
    }

private:
    bool writeRuntime(RetailMode mode)
    {
        if (!fnvxr::shared::beginSequencedSharedWrite(mRuntime->sequence))
            return false;
        ++mRuntimeFrame;
        if (mRuntimeFrame == 0u)
            ++mRuntimeFrame;
        mRuntime->magic = fnvxr::shared::RuntimeSharedMagic;
        mRuntime->version = fnvxr::shared::RuntimeSharedVersion;
        mRuntime->frame = mRuntimeFrame;
        mRuntime->menuBits = mode == RetailMode::Ui
            ? fnvxr::shared::RuntimeGenericMenuBit
            : 0u;
        mRuntime->phase = mode == RetailMode::Ui
            ? fnvxr::shared::RuntimePhaseMenu
            : fnvxr::shared::RuntimePhaseGameplay;
        mRuntime->uiInputAllowed = mode == RetailMode::Ui ? 1u : 0u;
        mRuntime->cameraActive = 1u;
        mRuntime->showroomActive = 0u;
        mRuntime->showroomPhase = 0u;
        mRuntime->showroomSceneIndex = 0u;
        mRuntime->showroomCellFormId = 0u;
        std::memset(mRuntime->reserved, 0, sizeof(mRuntime->reserved));
        fnvxr::shared::endSequencedSharedWrite(mRuntime->sequence);
        return true;
    }

    bool writeDesktopAssist(const fnvxr::shared::SharedVrPoseState& pose)
    {
        if (!fnvxr::shared::beginSequencedSharedWrite(mDesktopAssist->sequence))
            return false;

        mDesktopAssist->magic = fnvxr::shared::DesktopAssistSharedMagic;
        mDesktopAssist->version = fnvxr::shared::DesktopAssistSharedVersion;
        mDesktopAssist->flags = fnvxr::shared::DesktopAssistFlagLeaseCurrent
            | fnvxr::shared::DesktopAssistFlagCameraHookInstalled
            | fnvxr::shared::DesktopAssistFlagCameraPoseApplied
            | fnvxr::shared::DesktopAssistFlagFirstPerson
            | fnvxr::shared::DesktopAssistFlagPlayerTransformValid
            | fnvxr::shared::DesktopAssistFlagCameraLocalTransformValid
            | fnvxr::shared::DesktopAssistFlagBodyRootTransformValid;
        mDesktopAssist->frame = pose.frame;
        mDesktopAssist->cameraNodeAddress = 0x01111111u;
        mDesktopAssist->poseSequence =
            fnvxr::shared::sequencedValueBits(pose.sequence);
        mDesktopAssist->poseProducerEpoch = pose.producerEpoch;
        setIdentity(mDesktopAssist->playerWorldRot);
        mDesktopAssist->playerWorldPos[0] = 10.0f;
        mDesktopAssist->playerWorldPos[1] = 20.0f;
        mDesktopAssist->playerWorldPos[2] = 30.0f;
        quaternionToMatrix(pose.hmdRot, mDesktopAssist->cameraLocalRot);
        std::memcpy(
            mDesktopAssist->cameraLocalPos,
            pose.hmdPos,
            sizeof(mDesktopAssist->cameraLocalPos));
        setIdentity(mDesktopAssist->cameraWorldRot);
        std::memcpy(
            mDesktopAssist->cameraWorldPos,
            mDesktopAssist->playerWorldPos,
            sizeof(mDesktopAssist->cameraWorldPos));
        mDesktopAssist->bodyRootAddress = 0x02222222u;
        mDesktopAssist->bodyRootReserved = 0u;
        setIdentity(mDesktopAssist->bodyRootWorldRot);
        std::memcpy(
            mDesktopAssist->bodyRootWorldPos,
            mDesktopAssist->playerWorldPos,
            sizeof(mDesktopAssist->bodyRootWorldPos));
        if (mInjectBodyTranslationLeak
            && (pose.hmdPos[0] != 0.0f || pose.hmdPos[2] != 0.0f))
        {
            // Deliberately model the regression the assist check exists to
            // catch: a head lean has been composed into the player/body root.
            // This is much larger than the real observer's idle tolerance.
            mDesktopAssist->bodyRootWorldPos[0] += 12.0f;
        }
        fnvxr::shared::endSequencedSharedWrite(mDesktopAssist->sequence);
        return true;
    }

    bool writeCompleteUiQuad(const fnvxr::shared::SharedVrPoseState& pose)
    {
        auto* header = reinterpret_cast<fnvxr::shared::SharedDesktopAssistUiQuadHeader*>(
            mUiQuad);
        auto* pixels = mUiQuad + sizeof(*header);
        static constexpr std::uint8_t sourcePixels[] {
            0u, 0u, 0u, 255u,
            24u, 32u, 48u, 255u,
        };

        InterlockedExchange(&header->writing, 1);
        MemoryBarrier();
        header->magic = fnvxr::shared::DesktopAssistUiQuadSharedMagic;
        header->version = fnvxr::shared::DesktopAssistUiQuadSharedVersion;
        header->headerBytes = sizeof(*header);
        header->flags = fnvxr::engine::DesktopAssistUiQuadRequiredFlags;
        header->width = 2;
        header->height = 1;
        header->pitchBytes = 8;
        header->format = fnvxr::engine::DesktopAssistUiQuadPixelFormatX8R8G8B8;
        header->runtimeStateSample = mRuntimeFrame;
        header->poseFrame = pose.frame;
        header->poseSequence = longFromBits(
            fnvxr::shared::sequencedValueBits(pose.sequence));
        header->runtimePhase = fnvxr::shared::RuntimePhaseMenu;
        header->runtimeMenuBits = fnvxr::shared::RuntimeGenericMenuBit;
        std::memcpy(pixels, sourcePixels, sizeof(sourcePixels));
        header->pixelHash = fnvxr::engine::desktopAssistUiQuadPixelHash(
            pixels,
            sizeof(sourcePixels),
            &header->nonBlackSampleCount);
        header->captureFailure = 0u;
        header->poseProducerEpoch = pose.producerEpoch;
        ++mCaptureOrdinal;
        if (mCaptureOrdinal == 0u)
            ++mCaptureOrdinal;
        header->captureOrdinal = mCaptureOrdinal;
        MemoryBarrier();
        LONG sequence = fnvxr::shared::incrementNonzeroSharedCounter(header->sequence);
        // The reader treats the counter as a published seqlock value. While
        // `writing` owns this transaction, advance past an odd transition so
        // every complete fixture record has an even stable identity.
        if (!fnvxr::shared::sequencedValueIsPublished(sequence))
            sequence = fnvxr::shared::incrementNonzeroSharedCounter(header->sequence);
        if (!fnvxr::shared::sequencedValueIsPublished(sequence))
            return false;
        MemoryBarrier();
        InterlockedExchange(&header->writing, 0);
        return true;
    }

    bool writeInvalidUiQuad(std::uint32_t failure)
    {
        auto* header = reinterpret_cast<fnvxr::shared::SharedDesktopAssistUiQuadHeader*>(
            mUiQuad);
        InterlockedExchange(&header->writing, 1);
        MemoryBarrier();
        header->magic = fnvxr::shared::DesktopAssistUiQuadSharedMagic;
        header->version = fnvxr::shared::DesktopAssistUiQuadSharedVersion;
        header->headerBytes = sizeof(*header);
        header->flags = fnvxr::shared::DesktopAssistUiQuadFlagLeaseCurrent
            | fnvxr::shared::DesktopAssistUiQuadFlagPresentHookInstalled;
        header->width = 0;
        header->height = 0;
        header->pitchBytes = 0;
        header->format = 0;
        header->runtimeStateSample = 0u;
        header->poseFrame = 0u;
        header->poseSequence = 0;
        header->runtimePhase = fnvxr::shared::RuntimePhaseGameplay;
        header->runtimeMenuBits = 0u;
        header->pixelHash = 0u;
        header->captureFailure = failure;
        header->nonBlackSampleCount = 0u;
        header->poseProducerEpoch = 0u;
        header->captureOrdinal = mCaptureOrdinal;
        MemoryBarrier();
        fnvxr::shared::incrementNonzeroSharedCounter(header->sequence);
        MemoryBarrier();
        InterlockedExchange(&header->writing, 0);
        return true;
    }

    void update()
    {
        fnvxr::shared::SharedVrPoseState pose {};
        if (!readStablePose(pose)
            || pose.magic != fnvxr::shared::VrPoseSharedMagic
            || pose.version != fnvxr::shared::VrPoseSharedVersion
            || pose.producerEpoch == 0u
            || pose.frame == 0u
            || !fnvxr::shared::sequencedValueIsPublished(pose.sequence))
        {
            return;
        }

        const std::uint32_t poseSequence =
            fnvxr::shared::sequencedValueBits(pose.sequence);
        if (mRequireTrackedProp)
        {
            const std::uint32_t requiredTracking =
                fnvxr::shared::VrPoseTrackingRightGripActive
                | fnvxr::shared::VrPoseTrackingRightGripCurrent
                | fnvxr::shared::VrPoseTrackingRightAimActive
                | fnvxr::shared::VrPoseTrackingRightAimCurrent;
            if ((pose.trackingFlags & requiredTracking) == requiredTracking)
            {
                mSawTrackedPropGripAndAim.store(true);
                // The tracked-prop fixture's neutral grip is x=0.32 m and
                // its isolated controller phase moves it +0.12 m. Its aim
                // phase is a +20 degree yaw, whose Y quaternion term is
                // comfortably above this threshold.
                if (pose.rightPos[0] >= 0.40f
                    && std::fabs(pose.rightAimPos[0] - pose.rightPos[0]) < 0.0001f)
                {
                    mSawTrackedPropControllerTranslation.store(true);
                }
                if (std::fabs(pose.rightAimRot[1]) >= 0.10f
                    && std::fabs(pose.rightRot[1] - pose.rightAimRot[1]) < 0.0001f)
                {
                    mSawTrackedPropControllerAim.store(true);
                }
            }
        }
        mSawPose.store(true);
        const ULONGLONG now = GetTickCount64();
        if (mFirstPoseTimestamp == 0u)
            mFirstPoseTimestamp = now;
        const ULONGLONG elapsed = now - mFirstPoseTimestamp;
        const RetailMode mode = elapsed >= UiPhaseStartMilliseconds
                && elapsed < UiPhaseEndMilliseconds
            ? RetailMode::Ui
            : RetailMode::Gameplay;
        const RetailMode priorMode = mMode;
        const bool modeChanged = mode != mMode;
        if (modeChanged)
        {
            mMode = mode;
            if (!writeRuntime(mode))
            {
                mWriteFailed.store(true);
                return;
            }
        }

        const bool poseChanged = !mHasPublishedPose
            || poseSequence != mLastPoseSequence
            || pose.producerEpoch != mLastPoseProducerEpoch;
        if (poseChanged)
        {
            if (!writeDesktopAssist(pose))
            {
                mWriteFailed.store(true);
                return;
            }
            mHasPublishedPose = true;
            mLastPoseSequence = poseSequence;
            mLastPoseProducerEpoch = pose.producerEpoch;
        }

        if (mode == RetailMode::Ui && (modeChanged || poseChanged))
        {
            if (!writeCompleteUiQuad(pose))
                mWriteFailed.store(true);
        }
        else if (mode == RetailMode::Gameplay && priorMode == RetailMode::Ui)
        {
            if (!writeInvalidUiQuad(2u))
                mWriteFailed.store(true);
        }
    }

    void run()
    {
        while (!mStopRequested.load())
        {
            update();
            Sleep(1u);
        }
    }

    fnvxr::shared::SharedDesktopAssistState* mDesktopAssist = nullptr;
    fnvxr::shared::SharedRuntimeState* mRuntime = nullptr;
    std::uint8_t* mUiQuad = nullptr;
    std::thread mThread;
    std::atomic<bool> mStopRequested { false };
    std::atomic<bool> mWriteFailed { false };
    std::atomic<bool> mSawPose { false };
    std::atomic<bool> mSawTrackedPropGripAndAim { false };
    std::atomic<bool> mSawTrackedPropControllerTranslation { false };
    std::atomic<bool> mSawTrackedPropControllerAim { false };
    RetailMode mMode = RetailMode::Unknown;
    ULONGLONG mFirstPoseTimestamp = 0u;
    std::uint64_t mRuntimeFrame = 0u;
    std::uint64_t mLastPoseProducerEpoch = 0u;
    std::uint64_t mCaptureOrdinal = 0u;
    std::uint32_t mLastPoseSequence = 0u;
    bool mHasPublishedPose = false;
    bool mInjectBodyTranslationLeak = false;
    bool mRequireTrackedProp = false;
};

bool runAssist(const char* assistPath, const char* reportPath, DWORD& exitCode)
{
    if (!assistPath || assistPath[0] == '\0' || !reportPath || reportPath[0] == '\0')
        return false;

    std::string command = "\"";
    command += assistPath;
    command += "\" --scenario head-body --step-ms 500 --period-ms 10 --cycles 1";
    command += " --require-desktop-assist --require-ui-quad-transition";
    command += " --report \"";
    command += reportPath;
    command += "\" --overwrite-report";
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

    const DWORD wait = WaitForSingleObject(process.hProcess, AssistTimeoutMilliseconds);
    if (wait != WAIT_OBJECT_0)
    {
        TerminateProcess(process.hProcess, EXIT_FAILURE);
        WaitForSingleObject(process.hProcess, 1000u);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return false;
    }
    const bool gotExitCode = GetExitCodeProcess(process.hProcess, &exitCode) != FALSE;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return gotExitCode;
}

bool runTrackedPropAssist(const char* assistPath, DWORD& exitCode)
{
    if (!assistPath || assistPath[0] == '\0')
        return false;

    std::string command = "\"";
    command += assistPath;
    command += "\" --scenario head-body --tracked-prop --step-ms 80 --period-ms 5 --cycles 1 --no-observe";
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

    const DWORD wait = WaitForSingleObject(process.hProcess, AssistTimeoutMilliseconds);
    if (wait != WAIT_OBJECT_0)
    {
        TerminateProcess(process.hProcess, EXIT_FAILURE);
        WaitForSingleObject(process.hProcess, 1000u);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return false;
    }
    const bool gotExitCode = GetExitCodeProcess(process.hProcess, &exitCode) != FALSE;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return gotExitCode;
}

bool reportContainsExpectedEvidence(const char* path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return false;
    const std::string report {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>() };
    const char* required[] {
        "\"schema\": \"fnvxr-assist-report-v5\"",
        "\"cameraEvidenceSource\": \"desktop-assist-local-camera\"",
        "\"desktopAssistHeadBodyDecoupled\": true",
        "\"headTranslationBodyDecoupled\": true",
        "\"runtimeUiTransitionObserved\": true",
        "\"desktopAssistUiQuadPixelsVerifiedObserved\": true",
        "\"desktopAssistUiQuadInvalidatedAfterUiObserved\": true",
        "\"desktopAssistUiQuadTransitionObserved\": true",
        "\"requestedChecksPass\": true",
    };
    for (const char* expected : required)
    {
        if (report.find(expected) == std::string::npos)
        {
            std::cerr << "assist report omitted expected evidence: " << expected << "\n";
            return false;
        }
    }
    return true;
}

bool reportContainsExpectedBodyLeakRejection(const char* path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return false;
    const std::string report {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>() };
    const char* required[] {
        "\"cameraEvidenceSource\": \"desktop-assist-local-camera\"",
        "\"desktopAssistHeadBodyDecoupled\": false",
        "\"headTranslationBodyDecoupled\": false",
        "\"requestedChecksPass\": false",
    };
    for (const char* expected : required)
    {
        if (report.find(expected) == std::string::npos)
        {
            std::cerr << "assist report omitted body-leak rejection: " << expected << "\n";
            return false;
        }
    }
    return true;
}
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: fnvxr_assist_mapping_integration_test <fnvxr-assist-exe>\n";
        return EXIT_FAILURE;
    }

    NamedMapping desktopAssist;
    NamedMapping runtime;
    NamedMapping uiQuad;
    if (!desktopAssist.create(
            fnvxr::shared::DesktopAssistSharedMappingName,
            sizeof(fnvxr::shared::SharedDesktopAssistState))
        || !runtime.create(
            RuntimeStateMappingName,
            sizeof(fnvxr::shared::SharedRuntimeState))
        || !uiQuad.create(
            fnvxr::shared::DesktopAssistUiQuadSharedMappingName,
            fnvxr::engine::desktopAssistUiQuadMappingBytes()))
    {
        std::cerr << "could not create isolated synthetic desktop-assist mappings; "
                     "another producer may be active\n";
        return EXIT_FAILURE;
    }

    TemporaryReport report;
    if (!report.create())
    {
        std::cerr << "could not create temporary assist report path\n";
        return EXIT_FAILURE;
    }

    DWORD exitCode = EXIT_FAILURE;
    {
        SyntheticRetailPublisher publisher(
            desktopAssist.as<fnvxr::shared::SharedDesktopAssistState>(),
            runtime.as<fnvxr::shared::SharedRuntimeState>(),
            uiQuad.bytes());
        if (!publisher.initialize())
        {
            std::cerr << "could not initialize synthetic desktop-assist mappings\n";
            return EXIT_FAILURE;
        }
        publisher.start();
        const bool completed = runAssist(argv[1], report.path(), exitCode);
        publisher.stop();
        if (!completed || exitCode != EXIT_SUCCESS)
        {
            std::cerr << "fnvxr_assist did not pass the synthetic mapping fixture";
            if (completed)
                std::cerr << " (exit code " << exitCode << ')';
            std::cerr << "\n";
            return EXIT_FAILURE;
        }
        if (publisher.writeFailed() || !publisher.sawPose())
        {
            std::cerr << "synthetic retail publisher did not produce a complete pose-bound fixture\n";
            return EXIT_FAILURE;
        }
    }

    if (!reportContainsExpectedEvidence(report.path()))
        return EXIT_FAILURE;

    exitCode = EXIT_SUCCESS;
    {
        SyntheticRetailPublisher bodyLeakingPublisher(
            desktopAssist.as<fnvxr::shared::SharedDesktopAssistState>(),
            runtime.as<fnvxr::shared::SharedRuntimeState>(),
            uiQuad.bytes(),
            true);
        if (!bodyLeakingPublisher.initialize())
        {
            std::cerr << "could not initialize body-leak desktop-assist fixture\n";
            return EXIT_FAILURE;
        }
        bodyLeakingPublisher.start();
        const bool completed = runAssist(argv[1], report.path(), exitCode);
        bodyLeakingPublisher.stop();
        if (!completed || exitCode == EXIT_SUCCESS)
        {
            std::cerr << "fnvxr_assist accepted a synthetic head-to-body translation leak";
            if (completed)
                std::cerr << " (exit code " << exitCode << ')';
            std::cerr << "\n";
            return EXIT_FAILURE;
        }
        if (bodyLeakingPublisher.writeFailed() || !bodyLeakingPublisher.sawPose())
        {
            std::cerr << "body-leak publisher did not produce a complete pose-bound fixture\n";
            return EXIT_FAILURE;
        }
    }
    if (!reportContainsExpectedBodyLeakRejection(report.path()))
    {
        return EXIT_FAILURE;
    }

    exitCode = EXIT_FAILURE;
    {
        SyntheticRetailPublisher trackedPropPublisher(
            desktopAssist.as<fnvxr::shared::SharedDesktopAssistState>(),
            runtime.as<fnvxr::shared::SharedRuntimeState>(),
            uiQuad.bytes(),
            false,
            true);
        if (!trackedPropPublisher.initialize())
        {
            std::cerr << "could not initialize tracked-prop mapping observer\n";
            return EXIT_FAILURE;
        }
        trackedPropPublisher.start();
        const bool completed = runTrackedPropAssist(argv[1], exitCode);
        trackedPropPublisher.stop();
        if (!completed || exitCode != EXIT_SUCCESS)
        {
            std::cerr << "fnvxr_assist tracked-prop fixture did not complete";
            if (completed)
                std::cerr << " (exit code " << exitCode << ')';
            std::cerr << "\n";
            return EXIT_FAILURE;
        }
        if (trackedPropPublisher.writeFailed() || !trackedPropPublisher.sawPose()
            || !trackedPropPublisher.sawTrackedPropGripAndAim()
            || !trackedPropPublisher.sawTrackedPropControllerTranslation()
            || !trackedPropPublisher.sawTrackedPropControllerAim())
        {
            std::cerr << "tracked-prop fixture did not publish current right grip/aim plus isolated controller position and aim changes\n";
            return EXIT_FAILURE;
        }
    }

    std::cout << "fnvxr assist named-mapping integration test passed (including body-leak rejection and tracked-prop controller probes)\n";
    return EXIT_SUCCESS;
}
