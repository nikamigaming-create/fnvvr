#pragma once

#include "pose_sample_identity.h"
#include "pose_view_frame.h"
#include "presentation/source_key.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace fnvxr::kernel
{
enum class PoseStoreOutcome : std::uint8_t
{
    stored,
    invalid_identity,
    payload_time_mismatch,
    stale_epoch,
    stale_reference_space,
    regression,
    key_space_exhausted,
};

enum class FrameJoinOutcome : std::uint8_t
{
    exact,
    invalid_image_key,
    invalid_pose_identity,
    stale_pose_epoch,
    stale_reference_space,
    missing_pose,
    overwritten_pose,
    time_mismatch,
};

struct FrameJoinTelemetry final
{
    std::uint64_t poseStores = 0;
    std::uint64_t poseStoreRejections = 0;
    std::uint64_t slotOverwrites = 0;
    std::uint64_t currentOccupancy = 0;
    std::uint64_t maximumOccupancy = 0;
    std::uint64_t resets = 0;
    std::uint64_t resetRejections = 0;
    std::uint64_t joins = 0;
    std::uint64_t exact = 0;
    std::uint64_t invalidImageKey = 0;
    std::uint64_t invalidPoseIdentity = 0;
    std::uint64_t stalePoseEpoch = 0;
    std::uint64_t staleReferenceSpace = 0;
    std::uint64_t missingPose = 0;
    std::uint64_t overwrittenPose = 0;
    std::uint64_t timeMismatch = 0;
};

class JoinedFrame final
{
public:
    JoinedFrame(
        presentation::SourceKey image,
        PoseSampleIdentity poseIdentity,
        const PoseViewFrame& pose) noexcept
        : image_(image), poseIdentity_(poseIdentity), pose_(pose)
    {
    }

    [[nodiscard]] presentation::SourceKey image() const noexcept { return image_; }
    [[nodiscard]] PoseSampleIdentity poseIdentity() const noexcept { return poseIdentity_; }
    [[nodiscard]] const PoseViewFrame& pose() const noexcept { return pose_; }

private:
    presentation::SourceKey image_ {};
    PoseSampleIdentity poseIdentity_ {};
    PoseViewFrame pose_ {};
};

class FrameJoinResult final
{
public:
    [[nodiscard]] FrameJoinOutcome outcome() const noexcept { return outcome_; }
    [[nodiscard]] bool hasValue() const noexcept
    {
        return outcome_ == FrameJoinOutcome::exact;
    }
    [[nodiscard]] const JoinedFrame* value() const noexcept
    {
        return hasValue() ? &*value_ : nullptr;
    }

private:
    template <std::size_t>
    friend class ExactFrameJoiner;

    explicit FrameJoinResult(FrameJoinOutcome outcome) noexcept : outcome_(outcome) {}
    explicit FrameJoinResult(JoinedFrame value) noexcept
        : outcome_(FrameJoinOutcome::exact), value_(value)
    {
    }

    FrameJoinOutcome outcome_ = FrameJoinOutcome::missing_pose;
    std::optional<JoinedFrame> value_;
};
}
