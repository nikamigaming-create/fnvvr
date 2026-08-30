#define XR_USE_PLATFORM_WIN32

#include "../host/fnvxr_source_pose_history.h"

#include <cstdlib>
#include <iostream>

namespace
{
int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}

XrPosef pose(float x)
{
    return { { 0.0F, 0.0F, 0.0F, 1.0F }, { x, 1.0F, -0.5F } };
}

std::array<XrView, 2> views()
{
    std::array<XrView, 2> result {
        XrView { XR_TYPE_VIEW }, XrView { XR_TYPE_VIEW } };
    result[0].pose = pose(-0.032F);
    result[1].pose = pose(0.032F);
    for (XrView& view : result)
    {
        view.fov = { -0.8F, 0.8F, 0.8F, -0.8F };
    }
    return result;
}

void record(
    fnvxr::host::SourcePoseHistory& history,
    std::uint64_t sequence,
    XrTime displayTime,
    std::uint64_t epoch,
    std::uint32_t generation)
{
    const auto eyeViews = views();
    history.record(
        sequence,
        displayTime,
        epoch,
        generation,
        eyeViews.data(),
        pose(0.0F),
        pose(-0.2F),
        pose(0.2F),
        pose(-0.2F),
        pose(0.2F),
        0x7FU);
}
}

int main()
{
    constexpr std::uint64_t epoch = 41;
    constexpr std::uint32_t generation = 3;
    fnvxr::host::SourcePoseHistory history;
    record(history, 1, 1000, epoch, generation);

    fnvxr::host::SourceViewPublication found {};
    if (!history.findPoseSequence(1, 1000, epoch, generation, found)
        || found.poseSequence != 1 || found.predictedDisplayTime != 1000
        || found.leftAimPose.position.x != -0.2F)
    {
        return fail("exact source pose was not recovered");
    }
    if (history.findPoseSequence(1, 1000, epoch + 1, generation, found)
        || history.findPoseSequence(1, 1000, epoch, generation + 1, found))
    {
        return fail("epoch or reference-space mismatch did not fail closed");
    }

    fnvxr::host::gpu_color::ConsumerFrame frame {};
    frame.producerEpoch = 17;
    frame.sourceFrame = 23;
    frame.transactionId = 29;
    frame.poseSequence = 1;
    frame.renderedDisplayTime = 1000;
    if (!history.find(frame, epoch, generation, found))
        return fail("exact submitted-frame lineage was rejected");
    frame.renderedDisplayTime = 1001;
    if (history.find(frame, epoch, generation, found))
        return fail("display-time mismatch did not fail closed");

    for (std::uint64_t sequence = 2; sequence <= 129; ++sequence)
        record(history, sequence, static_cast<XrTime>(1000 + sequence), epoch, generation);
    if (history.findPoseSequence(1, 1000, epoch, generation, found))
        return fail("overwritten source pose was returned");

    history.reset();
    if (history.findPoseSequence(129, 1129, epoch, generation, found))
        return fail("reference-space reset retained old source poses");
    return EXIT_SUCCESS;
}
