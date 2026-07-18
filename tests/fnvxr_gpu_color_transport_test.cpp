#include "fnvxr_gpu_color_transport.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>

namespace
{
using namespace fnvxr::gpu;
using namespace fnvxr::gpu::color_v5;

[[noreturn]] void fail(const char* message)
{
    std::cerr << "FAIL: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void require(bool condition, const char* message)
{
    if (!condition)
        fail(message);
}

SharedStereoColorPayload completePayload()
{
    SharedStereoColorPayload value {};
    value.magic = SharedStereoColorMagic;
    value.version = SharedStereoColorVersion;
    value.descriptorBytes = sizeof(SharedStereoColorDescriptor);
    value.gpuCompletionHandle = 0x1000u;
    value.gpuReadySequence = 40u;
    value.gpuConsumerReleaseSequence = 41u;
    value.gpuCompletionPrimitive = GpuCompletionPrimitive::D3D11SharedFence;
    value.presentationMode = PresentationMode::BinocularWorld;
    value.resourceSetId = 7u;
    value.producerEpoch = 8u;
    value.producerProcessId = 9u;
    value.adapterLuid = 10u;
    value.transactionId = 11u;
    value.sourceFrame = 12u;
    value.poseSequence = 13u;
    value.runtimeStateSample = 14u;
    value.renderedDisplayTime = 15;
    value.leftColor = {
        0x2000u,
        2016u,
        2240u,
        GpuInteropFormat::R8G8B8A8Unorm,
        GpuSharedHandleType::D3D11NtHandle,
    };
    value.rightColor = value.leftColor;
    value.rightColor.sharedHandle = 0x3000u;
    return value;
}

CompletionObservation completeObservation(
    const SharedStereoColorDescriptor& value,
    std::uint64_t reached)
{
    return {
        value.gpuCompletionPrimitive,
        value.gpuCompletionHandle,
        reached,
        value.resourceSetId,
        value.producerEpoch,
        value.producerProcessId,
        value.adapterLuid,
        value.transactionId,
        value.sourceFrame,
        value.poseSequence,
        value.runtimeStateSample,
        true,
    };
}
}

int main()
{
    using namespace fnvxr::gpu;
    using namespace fnvxr::gpu::color_v5;

    static_assert(sizeof(SharedStereoColorDescriptor) == 168u);
    static_assert(sizeof(SharedStereoColorPayload) == 168u);
    static_assert(offsetof(SharedStereoColorDescriptor, leftColor) == 120u);
    static_assert(offsetof(SharedStereoColorDescriptor, rightColor) == 144u);
    require(
        std::strcmp(
            SharedStereoColorMappingName,
            "Local\\FNVXR_GPU_StereoColor_v5") == 0,
        "v5 mapping name drifted");

    SharedStereoColorPayload value = completePayload();
    require(
        validatePayloadMetadata(value) == Validation::Valid,
        "complete color-only metadata was rejected");
    value.presentationMode = PresentationMode::MonoUiQuad;
    require(
        validatePayloadMetadata(value) == Validation::Valid,
        "mono UI quad must use the same owned color transport");

    const auto expect = [&](Validation expected, const char* message) {
        require(validatePayloadMetadata(value) == expected, message);
        value = completePayload();
    };
    value.presentationMode = PresentationMode::Unknown;
    expect(Validation::InvalidPresentationMode, "unknown presentation mode passed");
    value.leftColor.sharedHandle = 0u;
    expect(Validation::MissingResource, "missing left texture passed");
    value.rightColor.handleType = GpuSharedHandleType::Unknown;
    expect(Validation::InvalidSharedHandleType, "non-NT handle passed");
    value.rightColor.format = GpuInteropFormat::R16G16B16A16Float;
    expect(Validation::MismatchedColorFormats, "unlike eye formats passed");
    value.rightColor.width += 1u;
    expect(Validation::MismatchedDimensions, "unlike eye dimensions passed");
    value.rightColor.sharedHandle = value.leftColor.sharedHandle;
    expect(Validation::AliasedEyeResources, "aliased eye textures passed");
    value.gpuConsumerReleaseSequence = value.gpuReadySequence + 2u;
    expect(
        Validation::InvalidConsumerReleaseMetadata,
        "nonadjacent fence ownership interval passed");
    value.leftColor.width = MaximumTextureDimension + 1u;
    expect(Validation::MismatchedDimensions, "oversized texture passed");

    SharedStereoColorDescriptor shared {};
    require(
        publish(&shared, [](SharedStereoColorPayload& payload) {
            payload = completePayload();
            return true;
        }),
        "complete publication failed");
    require(
        shared.publicationSequence.load() == 2u,
        "publication did not commit a nonzero even sequence");

    SharedStereoColorDescriptor snapshot {};
    require(
        readStableSnapshot(&shared, snapshot),
        "stable color descriptor was unreadable");
    require(
        snapshot.leftColor.sharedHandle == 0x2000u
            && snapshot.rightColor.sharedHandle == 0x3000u,
        "stable snapshot lost an eye handle");

    CompletionObservation observed = completeObservation(
        snapshot,
        snapshot.gpuReadySequence);
    require(
        validateReady(snapshot, observed) == Validation::Valid,
        "consumer-observed ready fence was rejected");
    observed.reachedSequence = snapshot.gpuReadySequence - 1u;
    require(
        validateReady(snapshot, observed) == Validation::GpuWorkIncomplete,
        "metadata alone substituted for GPU completion");
    observed = completeObservation(
        snapshot,
        snapshot.gpuConsumerReleaseSequence);
    require(
        validateReleasedForReuse(snapshot, observed) == Validation::Valid,
        "consumer release fence was rejected");
    observed.reachedSequence = snapshot.gpuConsumerReleaseSequence - 1u;
    require(
        validateReleasedForReuse(snapshot, observed)
            == Validation::ConsumerOwnershipNotReleased,
        "producer reused an owned resource set");

    SharedStereoColorDescriptor abandoned {};
    require(
        !publish(&abandoned, [](SharedStereoColorPayload&) { return false; }),
        "incomplete publication committed");
    require(
        (abandoned.publicationSequence.load() & 1u) != 0u,
        "failed publication did not poison the mapping");
    require(
        !readStableSnapshot(&abandoned, snapshot),
        "consumer read an abandoned publication");
    require(
        snapshot.publicationSequence.load() == 0u,
        "failed read retained a stale snapshot sequence");

    SharedStereoColorDescriptor overflow {};
    overflow.publicationSequence.store(
        (std::numeric_limits<std::uint32_t>::max)() - 1u);
    require(
        !publish(&overflow, [](SharedStereoColorPayload& payload) {
            payload = completePayload();
            return true;
        }),
        "sequence overflow was accepted");

    std::cout << "GPU stereo-color transport v5 tests passed\n";
    return EXIT_SUCCESS;
}
