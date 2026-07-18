#include "fnvxr_retail_tracked_frame_win32.h"

#include <cstring>

namespace fnvxr::engine
{
namespace
{
constexpr char DefaultRuntimeMappingName[] =
    "Local\\FNVXR_Runtime_State";

template <typename State>
bool readExactStableState(
    const State* sharedState,
    State& snapshot,
    LONG& sequence) noexcept
{
    snapshot = {};
    sequence = 0;
    if (!sharedState)
        return false;
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        const LONG before = sharedState->sequence;
        if (!shared::sequencedValueIsPublished(before))
        {
            YieldProcessor();
            continue;
        }
        MemoryBarrier();
        std::memcpy(&snapshot, sharedState, sizeof(snapshot));
        MemoryBarrier();
        const LONG after = sharedState->sequence;
        if (before == after && shared::sequencedValueIsPublished(after))
        {
            sequence = after;
            return true;
        }
        YieldProcessor();
    }
    snapshot = {};
    return false;
}
}

RetailTrackedFrameWin32Reader::~RetailTrackedFrameWin32Reader() noexcept
{
    reset();
}

bool RetailTrackedFrameWin32Reader::initialize(
    const char* poseMappingName,
    const char* runtimeMappingName) noexcept
{
    reset();
    mPoseMappingName = poseMappingName && poseMappingName[0] != '\0'
        ? poseMappingName
        : shared::VrPoseSharedMappingName;
    mRuntimeMappingName = runtimeMappingName && runtimeMappingName[0] != '\0'
        ? runtimeMappingName
        : DefaultRuntimeMappingName;
    mInitialized = true;
    return openMappings();
}

void RetailTrackedFrameWin32Reader::reset() noexcept
{
    if (mPose)
        UnmapViewOfFile(mPose);
    if (mRuntime)
        UnmapViewOfFile(mRuntime);
    if (mPoseMapping)
        CloseHandle(mPoseMapping);
    if (mRuntimeMapping)
        CloseHandle(mRuntimeMapping);
    mPoseMappingName = nullptr;
    mRuntimeMappingName = nullptr;
    mPoseMapping = nullptr;
    mRuntimeMapping = nullptr;
    mPose = nullptr;
    mRuntime = nullptr;
    mFailure = RetailTrackedFrameFailure::PosePublicationInvalid;
    mInitialized = false;
}

bool RetailTrackedFrameWin32Reader::ready() const noexcept
{
    return mInitialized && mPose && mRuntime;
}

bool RetailTrackedFrameWin32Reader::openMappings() noexcept
{
#if defined(_WIN32)
    if (!mInitialized || !mPoseMappingName || !mRuntimeMappingName)
        return false;
    if (!mPoseMapping)
    {
        mPoseMapping = OpenFileMappingA(
            FILE_MAP_READ,
            FALSE,
            mPoseMappingName);
        if (!mPoseMapping)
            return false;
        mPose = static_cast<const shared::SharedVrPoseState*>(
            MapViewOfFile(
                mPoseMapping,
                FILE_MAP_READ,
                0u,
                0u,
                sizeof(shared::SharedVrPoseState)));
        if (!mPose)
        {
            CloseHandle(mPoseMapping);
            mPoseMapping = nullptr;
            return false;
        }
    }
    if (!mRuntimeMapping)
    {
        mRuntimeMapping = OpenFileMappingA(
            FILE_MAP_READ,
            FALSE,
            mRuntimeMappingName);
        if (!mRuntimeMapping)
            return false;
        mRuntime = static_cast<const shared::SharedRuntimeState*>(
            MapViewOfFile(
                mRuntimeMapping,
                FILE_MAP_READ,
                0u,
                0u,
                sizeof(shared::SharedRuntimeState)));
        if (!mRuntime)
        {
            CloseHandle(mRuntimeMapping);
            mRuntimeMapping = nullptr;
            return false;
        }
    }
    return ready();
#else
    return false;
#endif
}

bool RetailTrackedFrameWin32Reader::readStableFrame(
    RetailTrackedFrame& frame) noexcept
{
    frame = {};
    if (!ready() && !openMappings())
    {
        mFailure = RetailTrackedFrameFailure::PosePublicationInvalid;
        return false;
    }

    for (int attempt = 0; attempt < 4; ++attempt)
    {
        RetailTrackedFrame before {};
        RetailTrackedFrame after {};
        if (!readExactStableState(
                mPose,
                before.pose,
                before.poseSequence)
            || !readExactStableState(
                mRuntime,
                before.runtime,
                before.runtimeSequence)
            || !readExactStableState(
                mPose,
                after.pose,
                after.poseSequence)
            || !readExactStableState(
                mRuntime,
                after.runtime,
                after.runtimeSequence))
        {
            continue;
        }
        if (before.poseSequence != after.poseSequence
            || before.pose.frame != after.pose.frame
            || before.pose.producerEpoch != after.pose.producerEpoch
            || before.runtimeSequence != after.runtimeSequence
            || before.runtime.frame != after.runtime.frame)
        {
            continue;
        }
        frame = before;
        return true;
    }
    mFailure = RetailTrackedFrameFailure::PosePublicationInvalid;
    return false;
}

bool RetailTrackedFrameWin32Reader::readPublishedFrame(
    RetailTrackedFrame& frame) noexcept
{
    if (!readStableFrame(frame))
        return false;
    const RetailTrackedFrameValidation validation =
        validateRetailTrackedPublishedFrame(frame);
    mFailure = validation.failure;
    if (!validation.complete())
    {
        frame = {};
        return false;
    }
    return true;
}

bool RetailTrackedFrameWin32Reader::readGameplayFrame(
    RetailTrackedFrame& frame) noexcept
{
    if (!readStableFrame(frame))
        return false;
    const RetailTrackedFrameValidation validation =
        validateRetailTrackedGameplayFrame(frame);
    mFailure = validation.failure;
    if (!validation.complete())
    {
        frame = {};
        return false;
    }
    return true;
}

RetailTrackedFrameFailure RetailTrackedFrameWin32Reader::failure() const
    noexcept
{
    return mFailure;
}
}
