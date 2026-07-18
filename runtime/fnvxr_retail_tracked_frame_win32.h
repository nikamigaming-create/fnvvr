#pragma once

#include "fnvxr_retail_tracked_frame.h"

namespace fnvxr::engine
{
#if defined(_WIN32)
inline constexpr bool RetailTrackedFrameWin32ReaderAvailable = true;
#else
inline constexpr bool RetailTrackedFrameWin32ReaderAvailable = false;
#endif

class RetailTrackedFrameWin32Reader final
{
public:
    RetailTrackedFrameWin32Reader() noexcept = default;
    ~RetailTrackedFrameWin32Reader() noexcept;

    RetailTrackedFrameWin32Reader(
        const RetailTrackedFrameWin32Reader&) = delete;
    RetailTrackedFrameWin32Reader& operator=(
        const RetailTrackedFrameWin32Reader&) = delete;

    bool initialize(
        const char* poseMappingName = nullptr,
        const char* runtimeMappingName = nullptr) noexcept;
    void reset() noexcept;
    bool ready() const noexcept;
    bool readPublishedFrame(RetailTrackedFrame& frame) noexcept;
    bool readGameplayFrame(RetailTrackedFrame& frame) noexcept;
    RetailTrackedFrameFailure failure() const noexcept;

private:
    bool openMappings() noexcept;
    bool readStableFrame(RetailTrackedFrame& frame) noexcept;

    const char* mPoseMappingName = nullptr;
    const char* mRuntimeMappingName = nullptr;
    HANDLE mPoseMapping = nullptr;
    HANDLE mRuntimeMapping = nullptr;
    const shared::SharedVrPoseState* mPose = nullptr;
    const shared::SharedRuntimeState* mRuntime = nullptr;
    RetailTrackedFrameFailure mFailure =
        RetailTrackedFrameFailure::PosePublicationInvalid;
    bool mInitialized = false;
};
}
