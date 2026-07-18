#include "fnvxr_gpu_frame_transport.h"

#include <cstdlib>
#include <iostream>

namespace
{
int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}

fnvxr::gpu::SharedStereoFrameDescriptor validDescriptor()
{
    using namespace fnvxr::gpu;
    SharedStereoFrameDescriptor value {};
    value.magic = SharedStereoFrameMagic;
    value.version = SharedStereoFrameVersion;
    value.descriptorBytes = sizeof(value);
    value.publicationSequence = 2;
    value.gpuReadySequence = 41;
    value.producerEpoch = 9;
    value.producerProcessId = 123;
    value.adapterLuid = 0x1122334455667788ull;
    value.transactionId = 1001;
    value.sourceFrame = 2002;
    value.poseSequence = 3003;
    value.renderedDisplayTime = 4004;
    value.leftColor = { 0x101, 2064, 2208, GpuInteropFormat::R8G8B8A8Unorm };
    value.rightColor = { 0x102, 2064, 2208, GpuInteropFormat::R8G8B8A8Unorm };
    value.leftDepth = { 0x103, 2064, 2208, GpuInteropFormat::R16G16B16A16Float };
    value.rightDepth = { 0x104, 2064, 2208, GpuInteropFormat::R16G16B16A16Float };
    return value;
}
}

int main()
{
    using namespace fnvxr::gpu;

    static_assert(
        sizeof(SharedStereoFrameDescriptor) <= 256,
        "GPU transport metadata must never grow into a CPU pixel payload");

    {
        const SharedStereoFrameDescriptor descriptor = validDescriptor();
        if (validateSharedStereoFrame(descriptor) != GpuFrameValidation::Valid)
            return fail("complete GPU stereo descriptor must validate");
    }

    {
        SharedStereoFrameDescriptor descriptor = validDescriptor();
        descriptor.rightColor.sharedHandle = descriptor.leftColor.sharedHandle;
        if (validateSharedStereoFrame(descriptor) != GpuFrameValidation::AliasedEyeResources)
            return fail("left and right eyes must not alias one GPU texture");
    }

    {
        SharedStereoFrameDescriptor descriptor = validDescriptor();
        descriptor.rightDepth.height--;
        if (validateSharedStereoFrame(descriptor) != GpuFrameValidation::MismatchedDimensions)
            return fail("all color/depth surfaces must use identical eye dimensions");
    }

    {
        SharedStereoFrameDescriptor descriptor = validDescriptor();
        descriptor.leftDepth.format = GpuInteropFormat::R8G8B8A8Unorm;
        if (validateSharedStereoFrame(descriptor) != GpuFrameValidation::InvalidDepthFormat)
            return fail("depth must use the declared D3D9/D3D11 interop encoding");
    }

    {
        SharedStereoFrameDescriptor descriptor = validDescriptor();
        descriptor.gpuReadySequence = 0;
        if (validateSharedStereoFrame(descriptor) != GpuFrameValidation::GpuWorkIncomplete)
            return fail("a shared handle without producer GPU completion must be rejected");
    }

    {
        SharedStereoFrameDescriptor descriptor = validDescriptor();
        descriptor.adapterLuid = 0;
        if (validateSharedStereoFrame(descriptor) != GpuFrameValidation::MissingIdentity)
            return fail("cross-process textures require an explicit adapter identity");
    }

    std::cout << "fnvxr GPU frame transport contract PASS\n";
    return EXIT_SUCCESS;
}
