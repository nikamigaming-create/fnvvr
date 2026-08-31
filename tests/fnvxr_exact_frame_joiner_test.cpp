#include "../kernel/exact_frame_joiner.h"

#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
using Joiner = fnvxr::kernel::ExactFrameJoiner<2>;
using fnvxr::kernel::FrameJoinOutcome;
using fnvxr::kernel::PoseSampleIdentity;
using fnvxr::kernel::PoseStoreOutcome;
using fnvxr::kernel::PoseViewFrame;
using fnvxr::kernel::PoseViewFrameData;
using fnvxr::kernel::presentation::SourceKey;

int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}

PoseViewFrame pose(std::uint64_t time, float x)
{
    PoseViewFrameData data {};
    data.sampleTime = { static_cast<std::int64_t>(time), fnvxr::kernel::ClockDomain::OpenXr };
    data.head.position.x = x;
    return PoseViewFrame(data);
}

PoseSampleIdentity identity(
    std::uint64_t epoch,
    std::uint64_t generation,
    std::uint64_t sequence,
    fnvxr::kernel::OpenXrTime time)
{
    return { epoch, generation, sequence, time };
}
}

int main()
{
    constexpr SourceKey image { 70, 80, 90 };
    Joiner joiner;
    if (!joiner.reset({ 10, 20 }) || joiner.reset({ 10, 20 }) || joiner.reset({ 0, 1 }))
        return fail("pose-domain reset validation failed");

    const auto first = identity(10, 20, 1, 1000);
    if (joiner.record(first, pose(1000, 1.5F)) != PoseStoreOutcome::stored)
        return fail("exact pose sample was not recorded");
    if (joiner.record(identity(10, 20, 2, 2000), pose(2001, 2.0F)) !=
        PoseStoreOutcome::payload_time_mismatch)
        return fail("pose payload with a different OpenXR time was accepted");
    const auto exact = joiner.join(image, first);
    if (exact.outcome() != FrameJoinOutcome::exact || !exact.hasValue() ||
        exact.value() == nullptr || exact.value()->image() != image ||
        exact.value()->poseIdentity() != first ||
        exact.value()->pose().data().head.position.x != 1.5F)
        return fail("exact image-to-pose join failed");

    if (joiner.join(image, identity(11, 20, 1, 1000)).outcome() !=
        FrameJoinOutcome::stale_pose_epoch)
        return fail("wrong producer epoch was not classified");
    if (joiner.join(image, identity(10, 21, 1, 1000)).outcome() !=
        FrameJoinOutcome::stale_reference_space)
        return fail("wrong reference-space generation was not classified");
    const auto wrongTime = joiner.join(image, identity(10, 20, 1, 1001));
    if (wrongTime.outcome() != FrameJoinOutcome::time_mismatch ||
        wrongTime.hasValue() || wrongTime.value() != nullptr)
        return fail("wrong OpenXR time did not fail closed");
    if (joiner.join(image, identity(10, 20, 2, 2000)).outcome() !=
        FrameJoinOutcome::missing_pose)
        return fail("missing pose was not classified");

    if (joiner.record(identity(10, 20, 2, 2000), pose(2000, 2.0F)) !=
            PoseStoreOutcome::stored ||
        joiner.record(identity(10, 20, 3, 3000), pose(3000, 3.0F)) !=
            PoseStoreOutcome::stored)
        return fail("monotonic pose recording failed");
    const auto overwritten = joiner.join(image, first);
    if (overwritten.outcome() != FrameJoinOutcome::overwritten_pose ||
        overwritten.hasValue() || overwritten.value() != nullptr)
        return fail("overwritten pose did not fail closed");
    if (joiner.record(identity(10, 20, 2, 4000), pose(4000, 4.0F)) !=
        PoseStoreOutcome::regression)
        return fail("pose-sequence regression was accepted");

    if (!joiner.reset({ 10, 21 }) ||
        joiner.record(identity(10, 21, 1, 4000), pose(4000, 4.0F)) !=
            PoseStoreOutcome::stored ||
        joiner.join(image, identity(10, 20, 3, 3000)).outcome() !=
            FrameJoinOutcome::stale_reference_space)
        return fail("reference-space reset did not isolate and reuse sequence numbers");

    if (!joiner.reset({ 11, 1 }))
        return fail("producer epoch reset failed");
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    const auto maximumTime = std::numeric_limits<fnvxr::kernel::OpenXrTime>::max();
    if (joiner.record(identity(11, 1, maximum, maximumTime),
            pose(static_cast<std::uint64_t>(maximumTime), 5.0F)) !=
            PoseStoreOutcome::stored ||
        joiner.record(identity(11, 1, 1, 1), pose(1, 6.0F)) !=
            PoseStoreOutcome::key_space_exhausted)
        return fail("pose identity wrap was not rejected");

    const auto invalidImage = joiner.join({}, identity(11, 1, maximum, maximumTime));
    if (invalidImage.outcome() != FrameJoinOutcome::invalid_image_key ||
        invalidImage.value() != nullptr)
        return fail("invalid image identity did not fail closed");

    const auto telemetry = joiner.telemetry();
    if (telemetry.poseStores != 5 || telemetry.poseStoreRejections != 3 ||
        telemetry.slotOverwrites != 1 || telemetry.resets != 3 ||
        telemetry.currentOccupancy != 1 || telemetry.maximumOccupancy != 2 ||
        telemetry.resetRejections != 2 || telemetry.joins != 8 ||
        telemetry.exact != 1 || telemetry.invalidImageKey != 1 ||
        telemetry.stalePoseEpoch != 1 || telemetry.staleReferenceSpace != 2 ||
        telemetry.missingPose != 1 || telemetry.overwrittenPose != 1 ||
        telemetry.timeMismatch != 1)
        return fail("frame-join telemetry snapshot was incorrect");

    return EXIT_SUCCESS;
}
