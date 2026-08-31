#pragma once

#include "fnvxr_gpu_color_route.h"
#include "fnvxr_stereo_math.h"
#include "../kernel/exact_frame_joiner.h"

#include <openxr/openxr.h>

#include <array>
#include <cmath>
#include <cstdint>

namespace fnvxr::host
{
struct SourceViewPublication final
{
    bool valid = false;
    std::uint64_t poseSequence = 0;
    XrTime predictedDisplayTime = 0;
    std::uint64_t hostProducerEpoch = 0;
    std::uint32_t referenceSpaceGeneration = 0;
    std::array<XrView, 2> views {
        XrView { XR_TYPE_VIEW }, XrView { XR_TYPE_VIEW } };
    XrPosef hmdPose { { 0.0F, 0.0F, 0.0F, 1.0F }, {} };
    XrPosef leftGripPose { { 0.0F, 0.0F, 0.0F, 1.0F }, {} };
    XrPosef rightGripPose { { 0.0F, 0.0F, 0.0F, 1.0F }, {} };
    XrPosef leftAimPose { { 0.0F, 0.0F, 0.0F, 1.0F }, {} };
    XrPosef rightAimPose { { 0.0F, 0.0F, 0.0F, 1.0F }, {} };
    std::uint32_t trackingFlags = 0;
};

// OpenXR translation only. Image identity, pose identity, bounded history,
// and fail-closed joining are owned by the platform-neutral kernel.
class SourcePoseHistory final
{
public:
    void reset() noexcept
    {
        history_.clear();
        activeDomain_ = {};
    }

    void record(
        std::uint64_t poseSequence,
        XrTime predictedDisplayTime,
        std::uint64_t hostProducerEpoch,
        std::uint32_t referenceSpaceGeneration,
        const XrView* views,
        const XrPosef& hmdPose,
        const XrPosef& leftGripPose,
        const XrPosef& rightGripPose,
        const XrPosef& leftAimPose,
        const XrPosef& rightAimPose,
        std::uint32_t trackingFlags) noexcept
    {
        if (poseSequence == 0 || predictedDisplayTime == 0
            || hostProducerEpoch == 0 || referenceSpaceGeneration == 0
            || views == nullptr || !viewUsable(views[0])
            || !viewUsable(views[1]) || !eyesDistinct(views[0], views[1])
            || !poseUsable(hmdPose) || !poseUsable(leftGripPose)
            || !poseUsable(rightGripPose))
        {
            return;
        }

        const kernel::PoseDomain domain {
            hostProducerEpoch, referenceSpaceGeneration };
        if (domain != activeDomain_)
        {
            if (!history_.reset(domain))
                return;
            activeDomain_ = domain;
        }

        kernel::PoseViewFrameData data {};
        data.sampleTime = {
            static_cast<std::int64_t>(predictedDisplayTime),
            kernel::ClockDomain::OpenXr };
        data.head = toKernel(hmdPose);
        data.grips = { toKernel(leftGripPose), toKernel(rightGripPose) };
        data.aims = { toKernel(leftAimPose), toKernel(rightAimPose) };
        data.views = { toKernel(views[0]), toKernel(views[1]) };
        data.trackingFlags = trackingFlags;
        (void)history_.record(
            { hostProducerEpoch, referenceSpaceGeneration, poseSequence,
                static_cast<std::int64_t>(predictedDisplayTime) },
            kernel::PoseViewFrame(data));
    }

    [[nodiscard]] bool findPoseSequence(
        std::uint64_t poseSequence,
        XrTime predictedDisplayTime,
        std::uint64_t currentHostProducerEpoch,
        std::uint32_t currentReferenceSpaceGeneration,
        SourceViewPublication& found) const noexcept
    {
        found = {};
        if (poseSequence == 0 || predictedDisplayTime == 0
            || currentHostProducerEpoch == 0
            || currentReferenceSpaceGeneration == 0)
        {
            return false;
        }

        return join(
            { currentHostProducerEpoch, poseSequence, poseSequence },
            { currentHostProducerEpoch, currentReferenceSpaceGeneration,
                poseSequence, static_cast<std::int64_t>(predictedDisplayTime) },
            found);
    }

    [[nodiscard]] bool find(
        const gpu_color::ConsumerFrame& frame,
        std::uint64_t currentHostProducerEpoch,
        std::uint32_t currentReferenceSpaceGeneration,
        SourceViewPublication& found) const noexcept
    {
        if (!join(
                { frame.producerEpoch, frame.sourceFrame, frame.transactionId },
                { currentHostProducerEpoch, currentReferenceSpaceGeneration,
                    frame.poseSequence, frame.renderedDisplayTime },
                found))
        {
            return false;
        }
        const gpu_color::SourcePoseLineage lineage {
            found.poseSequence,
            found.predictedDisplayTime,
            found.hostProducerEpoch,
            found.referenceSpaceGeneration,
            true,
            true,
        };
        if (!gpu_color::exactSourcePoseLineage(
                frame,
                lineage,
                currentHostProducerEpoch,
                currentReferenceSpaceGeneration))
        {
            found = {};
            return false;
        }
        return true;
    }

    [[nodiscard]] kernel::FrameJoinTelemetry telemetry() const noexcept
    {
        return history_.telemetry();
    }

private:
    using History = kernel::ExactFrameJoiner<128>;

    [[nodiscard]] bool join(
        kernel::presentation::SourceKey image,
        kernel::PoseSampleIdentity poseIdentity,
        SourceViewPublication& found) const noexcept
    {
        found = {};
        const kernel::FrameJoinResult result = history_.join(image, poseIdentity);
        const kernel::JoinedFrame* joined = result.value();
        if (joined == nullptr)
            return false;

        const kernel::PoseViewFrameData& data = joined->pose().data();
        found.valid = true;
        found.poseSequence = poseIdentity.poseSequence;
        found.predictedDisplayTime =
            static_cast<XrTime>(poseIdentity.displayTime);
        found.hostProducerEpoch = poseIdentity.producerEpoch;
        found.referenceSpaceGeneration = static_cast<std::uint32_t>(
            poseIdentity.referenceSpaceGeneration);
        found.views = { toOpenXr(data.views[0]), toOpenXr(data.views[1]) };
        found.hmdPose = toOpenXr(data.head);
        found.leftGripPose = toOpenXr(data.grips[0]);
        found.rightGripPose = toOpenXr(data.grips[1]);
        found.leftAimPose = toOpenXr(data.aims[0]);
        found.rightAimPose = toOpenXr(data.aims[1]);
        found.trackingFlags = data.trackingFlags;
        return true;
    }

    static kernel::Pose toKernel(const XrPosef& pose) noexcept
    {
        return {
            { pose.position.x, pose.position.y, pose.position.z },
            { pose.orientation.x, pose.orientation.y,
                pose.orientation.z, pose.orientation.w } };
    }

    static kernel::View toKernel(const XrView& view) noexcept
    {
        return {
            toKernel(view.pose),
            { view.fov.angleLeft, view.fov.angleRight,
                view.fov.angleUp, view.fov.angleDown } };
    }

    static XrPosef toOpenXr(const kernel::Pose& pose) noexcept
    {
        return {
            { pose.orientation.x, pose.orientation.y,
                pose.orientation.z, pose.orientation.w },
            { pose.position.x, pose.position.y, pose.position.z } };
    }

    static XrView toOpenXr(const kernel::View& view) noexcept
    {
        XrView result { XR_TYPE_VIEW };
        result.pose = toOpenXr(view.pose);
        result.fov = {
            view.fieldOfView.leftRadians,
            view.fieldOfView.rightRadians,
            view.fieldOfView.upRadians,
            view.fieldOfView.downRadians };
        return result;
    }

    static bool poseUsable(const XrPosef& pose) noexcept
    {
        const float lengthSquared =
            pose.orientation.x * pose.orientation.x
            + pose.orientation.y * pose.orientation.y
            + pose.orientation.z * pose.orientation.z
            + pose.orientation.w * pose.orientation.w;
        return std::isfinite(lengthSquared) && lengthSquared >= 0.25F
            && lengthSquared <= 4.0F && std::isfinite(pose.position.x)
            && std::isfinite(pose.position.y)
            && std::isfinite(pose.position.z);
    }

    static bool viewUsable(const XrView& view) noexcept
    {
        const float fov[4] {
            view.fov.angleLeft, view.fov.angleRight,
            view.fov.angleUp, view.fov.angleDown };
        return poseUsable(view.pose) && stereo::openXrFovAnglesUsable(fov);
    }

    static bool eyesDistinct(const XrView& left, const XrView& right) noexcept
    {
        const float x = right.pose.position.x - left.pose.position.x;
        const float y = right.pose.position.y - left.pose.position.y;
        const float z = right.pose.position.z - left.pose.position.z;
        const float distanceSquared = x * x + y * y + z * z;
        return std::isfinite(distanceSquared)
            && distanceSquared >= 0.03F * 0.03F
            && distanceSquared <= 0.12F * 0.12F;
    }

    History history_;
    kernel::PoseDomain activeDomain_ {};
};
}
