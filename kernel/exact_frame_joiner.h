#pragma once

#include "frame_join_types.h"

#include <array>
#include <cstddef>
#include <limits>
#include <mutex>
#include <optional>

namespace fnvxr::kernel
{
// Pose samples are indexed directly by sequence. Image keys remain a distinct
// identity and are attached only after an exact pose-domain/sequence/time match.
template <std::size_t Capacity>
class ExactFrameJoiner final
{
    static_assert(Capacity > 0, "join history capacity must be non-zero");

public:
    void clear() noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        clearSlots();
        activeDomain_ = {};
        ++telemetry_.resets;
    }

    [[nodiscard]] bool reset(PoseDomain domain) noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!isValid(domain) || domain == activeDomain_)
        {
            ++telemetry_.resetRejections;
            return false;
        }
        clearSlots();
        activeDomain_ = domain;
        ++telemetry_.resets;
        return true;
    }

    [[nodiscard]] PoseStoreOutcome record(
        PoseSampleIdentity identity,
        const PoseViewFrame& pose) noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!isValid(identity))
            return reject(PoseStoreOutcome::invalid_identity);
        if (pose.data().sampleTime.domain != ClockDomain::OpenXr ||
            pose.data().sampleTime.nanoseconds != identity.displayTime)
            return reject(PoseStoreOutcome::payload_time_mismatch);
        if (identity.producerEpoch != activeDomain_.producerEpoch)
            return reject(PoseStoreOutcome::stale_epoch);
        if (identity.referenceSpaceGeneration != activeDomain_.referenceSpaceGeneration)
            return reject(PoseStoreOutcome::stale_reference_space);
        if (keySpaceExhausted_)
            return reject(PoseStoreOutcome::key_space_exhausted);
        if (isValid(latestIdentity_) && !isStrictlyNewer(identity, latestIdentity_))
            return reject(PoseStoreOutcome::regression);

        auto& slot = slots_[indexFor(identity.poseSequence)];
        if (slot.sample.has_value())
            ++telemetry_.slotOverwrites;
        slot.sample.emplace(identity, pose);
        latestIdentity_ = identity;
        keySpaceExhausted_ =
            identity.poseSequence == std::numeric_limits<std::uint64_t>::max() ||
            identity.displayTime == std::numeric_limits<OpenXrTime>::max();
        ++telemetry_.poseStores;
        return PoseStoreOutcome::stored;
    }

    [[nodiscard]] FrameJoinResult join(
        presentation::SourceKey image,
        PoseSampleIdentity requestedPose) const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++telemetry_.joins;
        if (!presentation::isValid(image))
            return result(FrameJoinOutcome::invalid_image_key, telemetry_.invalidImageKey);
        if (!isValid(requestedPose))
            return result(
                FrameJoinOutcome::invalid_pose_identity, telemetry_.invalidPoseIdentity);
        if (requestedPose.producerEpoch != activeDomain_.producerEpoch)
            return result(FrameJoinOutcome::stale_pose_epoch, telemetry_.stalePoseEpoch);
        if (requestedPose.referenceSpaceGeneration !=
            activeDomain_.referenceSpaceGeneration)
        {
            return result(FrameJoinOutcome::stale_reference_space,
                telemetry_.staleReferenceSpace);
        }

        const auto& slot = slots_[indexFor(requestedPose.poseSequence)];
        if (slot.sample.has_value() &&
            slot.sample->identity.poseSequence == requestedPose.poseSequence)
        {
            if (slot.sample->identity.displayTime != requestedPose.displayTime)
                return result(FrameJoinOutcome::time_mismatch, telemetry_.timeMismatch);
            ++telemetry_.exact;
            return FrameJoinResult(JoinedFrame(image, requestedPose, slot.sample->pose));
        }

        if (latestIdentity_.poseSequence >= Capacity &&
            requestedPose.poseSequence <= latestIdentity_.poseSequence - Capacity)
        {
            return result(
                FrameJoinOutcome::overwritten_pose, telemetry_.overwrittenPose);
        }
        return result(FrameJoinOutcome::missing_pose, telemetry_.missingPose);
    }

    [[nodiscard]] FrameJoinTelemetry telemetry() const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return telemetry_;
    }

private:
    void clearSlots() noexcept
    {
        for (auto& slot : slots_)
            slot.sample.reset();
        latestIdentity_ = {};
        keySpaceExhausted_ = false;
    }

    struct PoseSample final
    {
        PoseSample(PoseSampleIdentity source, const PoseViewFrame& value) noexcept
            : identity(source), pose(value)
        {
        }
        PoseSampleIdentity identity {};
        PoseViewFrame pose {};
    };

    struct Slot final
    {
        std::optional<PoseSample> sample;
    };

    [[nodiscard]] static constexpr std::size_t indexFor(
        std::uint64_t sequence) noexcept
    {
        return static_cast<std::size_t>(sequence % Capacity);
    }

    [[nodiscard]] PoseStoreOutcome reject(PoseStoreOutcome outcome) noexcept
    {
        ++telemetry_.poseStoreRejections;
        return outcome;
    }

    [[nodiscard]] FrameJoinResult result(
        FrameJoinOutcome outcome,
        std::uint64_t& counter) const noexcept
    {
        ++counter;
        return FrameJoinResult(outcome);
    }

    mutable std::mutex mutex_;
    std::array<Slot, Capacity> slots_ {};
    PoseDomain activeDomain_ {};
    PoseSampleIdentity latestIdentity_ {};
    bool keySpaceExhausted_ = false;
    mutable FrameJoinTelemetry telemetry_ {};
};
}
