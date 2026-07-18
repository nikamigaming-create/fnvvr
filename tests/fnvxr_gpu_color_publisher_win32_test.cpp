#include "fnvxr_gpu_color_publisher_win32.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

fnvxr::d3d9::color_transport::ProducerPublication publication()
{
    using namespace fnvxr;
    d3d9::color_transport::ProducerPublication result {};
    result.complete = true;
    result.failure = d3d9::color_transport::ProducerFailure::None;
    auto& value = result.payload;
    value.magic = gpu::color_v5::SharedStereoColorMagic;
    value.version = gpu::color_v5::SharedStereoColorVersion;
    value.descriptorBytes = sizeof(gpu::color_v5::SharedStereoColorDescriptor);
    value.gpuCompletionHandle = 11u;
    value.gpuReadySequence = 1u;
    value.gpuConsumerReleaseSequence = 2u;
    value.gpuCompletionPrimitive = gpu::GpuCompletionPrimitive::D3D11SharedFence;
    value.presentationMode = gpu::color_v5::PresentationMode::BinocularWorld;
    value.resourceSetId = 12u;
    value.producerEpoch = 13u;
    value.producerProcessId = GetCurrentProcessId();
    value.adapterLuid = 14u;
    value.transactionId = 15u;
    value.sourceFrame = 16u;
    value.poseSequence = 18u;
    value.runtimeStateSample = 20u;
    value.renderedDisplayTime = 21;
    value.leftColor = {
        22u,
        1920u,
        1080u,
        gpu::GpuInteropFormat::R8G8B8A8Unorm,
        gpu::GpuSharedHandleType::D3D11NtHandle,
    };
    value.rightColor = value.leftColor;
    value.rightColor.sharedHandle = 23u;
    return result;
}
}

int main()
{
    using namespace fnvxr;
    const DWORD pid = GetCurrentProcessId();
    char mappingName[112] {};
    char mutexName[112] {};
    sprintf_s(mappingName, "Local\\FNVXR_Test_Color_%lu", pid);
    sprintf_s(mutexName, "Local\\FNVXR_Test_Color_Mutex_%lu", pid);

    d3d9::color_transport::Win32Publisher publisher;
    require(
        publisher.initialize(mappingName, mutexName),
        "publisher initialization failed");
    require(publisher.publish(publication()), "valid publication failed");

    gpu::color_v5::SharedStereoColorDescriptor snapshot {};
    require(
        gpu::color_v5::readStableSnapshot(
            publisher.descriptor(),
            snapshot),
        "published descriptor was not stable");
    require(
        gpu::color_v5::validatePayloadMetadata(snapshot)
            == gpu::color_v5::Validation::Valid
            && snapshot.transactionId == 15u,
        "published descriptor identity changed");

    d3d9::color_transport::Win32Publisher rival;
    require(
        !rival.initialize(mappingName, mutexName)
            && rival.failure()
                == d3d9::color_transport::Win32PublisherFailure::ProducerLeaseUnavailable,
        "second producer acquired the lifetime lease");

    std::cout << "GPU color Win32 publisher passed\n";
    return EXIT_SUCCESS;
}
