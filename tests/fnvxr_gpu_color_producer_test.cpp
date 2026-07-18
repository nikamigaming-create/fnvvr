#include "fnvxr_gpu_color_producer.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
using namespace fnvxr::d3d9::color_transport;

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

enum class Call : std::uint8_t
{
    Release,
    Synchronize,
    Left,
    Right,
    Signal,
};

struct State
{
    std::vector<Call> calls;
    std::uint64_t completedFence = 0u;
    std::uint64_t signaledFence = 0u;
    bool synchronizationSucceeds = true;
    bool leftSucceeds = true;
    bool rightSucceeds = true;
    bool signalSucceeds = true;
};

bool releaseReached(void* opaque, std::uint64_t value) noexcept
{
    auto* state = static_cast<State*>(opaque);
    state->calls.push_back(Call::Release);
    return state->completedFence >= value;
}

bool synchronize(void* opaque) noexcept
{
    auto* state = static_cast<State*>(opaque);
    state->calls.push_back(Call::Synchronize);
    return state->synchronizationSucceeds;
}

bool copyLeft(void* opaque) noexcept
{
    auto* state = static_cast<State*>(opaque);
    state->calls.push_back(Call::Left);
    return state->leftSucceeds;
}

bool copyRight(void* opaque) noexcept
{
    auto* state = static_cast<State*>(opaque);
    state->calls.push_back(Call::Right);
    return state->rightSucceeds;
}

bool signal(void* opaque, std::uint64_t value) noexcept
{
    auto* state = static_cast<State*>(opaque);
    state->calls.push_back(Call::Signal);
    if (!state->signalSucceeds)
        return false;
    state->signaledFence = value;
    return true;
}

ProducerOperations operations(State& state)
{
    return {
        &state,
        &releaseReached,
        &synchronize,
        &copyLeft,
        &copyRight,
        &signal,
    };
}

constexpr ProducerCapabilities capabilities()
{
    return { true, true, true, true, true, true, true };
}

constexpr ProducerResources resources()
{
    return {
        0x101u,
        0x102u,
        0x103u,
        0x201u,
        0xAABBCCDDu,
        2064u,
        2208u,
        fnvxr::gpu::GpuInteropFormat::R8G8B8A8Unorm,
    };
}

constexpr ProducerFrameIdentity frame(std::uint64_t transaction = 0x301u)
{
    return {
        fnvxr::gpu::color_v5::PresentationMode::BinocularWorld,
        0x401u,
        1234u,
        transaction,
        transaction + 1u,
        transaction + 2u,
        transaction + 3u,
        static_cast<std::int64_t>(transaction + 4u),
    };
}
}

int main()
{
    static_assert(capabilitiesComplete(capabilities()));
    static_assert(resourcesComplete(resources()));
    static_assert(frameIdentityComplete(frame()));

    {
        State state;
        Producer producer;
        require(producer.initialize(
                operations(state),
                capabilities(),
                resources()),
            "complete GPU-only color transport did not initialize");
        const ProducerPublication first = producer.produce(frame());
        require(first.complete && first.failure == ProducerFailure::None,
            "the first color pair was not produced");
        require(state.calls == std::vector<Call> {
                Call::Synchronize,
                Call::Left,
                Call::Right,
                Call::Signal,
            },
            "D3D9 sync, two GPU copies, and D3D11 fence signal order changed");
        require(state.signaledFence == 1u
                && first.payload.gpuReadySequence == 1u
                && first.payload.gpuConsumerReleaseSequence == 2u,
            "the first shared-fence interval is not [1,2]");
        require(first.payload.leftColor.sharedHandle == 0x101u
                && first.payload.rightColor.sharedHandle == 0x102u
                && first.payload.gpuCompletionHandle == 0x103u
                && first.payload.leftColor.sharedHandle
                    != first.payload.rightColor.sharedHandle,
            "v5 did not receive two distinct NT color handles and one fence");
        require(fnvxr::gpu::color_v5::validatePayloadMetadata(first.payload)
                == fnvxr::gpu::color_v5::Validation::Valid,
            "producer output is not valid ABI-v5 color metadata");

        state.calls.clear();
        const ProducerPublication busy = producer.produce(frame(0x501u));
        require(!busy.complete
                && busy.failure == ProducerFailure::ConsumerReleasePending
                && state.calls == std::vector<Call> { Call::Release },
            "an unreleased resource set was overwritten");

        state.completedFence = 2u;
        state.calls.clear();
        const ProducerPublication second = producer.produce(frame(0x601u));
        require(second.complete
                && second.payload.gpuReadySequence == 3u
                && second.payload.gpuConsumerReleaseSequence == 4u,
            "the second shared-fence interval is not [3,4]");
        require(state.calls == std::vector<Call> {
                Call::Release,
                Call::Synchronize,
                Call::Left,
                Call::Right,
                Call::Signal,
            },
            "consumer release was not proven before producer reuse");
    }

    {
        State state;
        Producer producer;
        ProducerCapabilities incomplete = capabilities();
        incomplete.cpuPixelTransferAbsent = false;
        require(!producer.initialize(
                operations(state),
                incomplete,
                resources()),
            "a CPU pixel-transfer implementation was admitted");
        incomplete = capabilities();
        incomplete.depthRemainsRenderLocal = false;
        require(!producer.initialize(
                operations(state),
                incomplete,
                resources()),
            "a producer attempting to transport depth was admitted");
        ProducerResources aliased = resources();
        aliased.rightColorNtHandle = aliased.leftColorNtHandle;
        require(!producer.initialize(
                operations(state),
                capabilities(),
                aliased),
            "aliased eye handles were admitted");
        ProducerResources unsupportedFormat = resources();
        unsupportedFormat.format = fnvxr::gpu::GpuInteropFormat::Unknown;
        require(!producer.initialize(
                operations(state),
                capabilities(),
                unsupportedFormat),
            "an unsupported source color format was admitted");
    }

    {
        State state;
        Producer producer;
        require(producer.initialize(
                operations(state),
                capabilities(),
                resources()),
            "failure producer did not initialize");
        ProducerFrameIdentity invalid = frame();
        invalid.runtimeStateSample = 0u;
        const ProducerPublication rejected = producer.produce(invalid);
        require(!rejected.complete
                && rejected.failure == ProducerFailure::InvalidFrameIdentity
                && state.calls.empty(),
            "invalid frame identity reached GPU operations");

        state.synchronizationSucceeds = false;
        const ProducerPublication unsynchronized = producer.produce(frame());
        require(!unsynchronized.complete
                && unsynchronized.failure == ProducerFailure::D3D9Synchronization
                && state.calls == std::vector<Call> { Call::Synchronize },
            "unsynchronized D3D9 writes reached a D3D11 copy");
    }

    {
        State state;
        Producer producer;
        require(producer.initialize(
                operations(state),
                capabilities(),
                resources()),
            "right-copy failure producer did not initialize");
        state.rightSucceeds = false;
        const ProducerPublication failed = producer.produce(frame());
        require(!failed.complete
                && failed.failure == ProducerFailure::RightGpuCopy
                && state.calls == std::vector<Call> {
                    Call::Synchronize,
                    Call::Left,
                    Call::Right,
                }
                && state.signaledFence == 0u,
            "a partial eye copy was signaled as a complete color pair");
    }

    std::cout << "GPU-only ABI-v5 color producer contract passed\n";
    return EXIT_SUCCESS;
}
