#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>

#include "fnvxr_gpu_color_consumer.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
using namespace fnvxr::host::gpu_color;
using Descriptor = fnvxr::gpu::color_v5::SharedStereoColorDescriptor;
using Payload = fnvxr::gpu::color_v5::SharedStereoColorPayload;

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
    Open,
    Close,
    Alive,
    Wait,
    Left,
    Right,
    Signal,
};

Payload frame(
    std::uint64_t ready = 1u,
    std::uint64_t ordinal = 1u,
    std::uint64_t epoch = 0x40u,
    std::uint64_t resourceSet = 0x50u,
    std::uint64_t leftHandle = 0x101u,
    std::uint64_t rightHandle = 0x102u,
    std::uint64_t fenceHandle = 0x103u,
    std::uint32_t producerPid = 4321u)
{
    Payload value {};
    value.magic = fnvxr::gpu::color_v5::SharedStereoColorMagic;
    value.version = fnvxr::gpu::color_v5::SharedStereoColorVersion;
    value.descriptorBytes = sizeof(Descriptor);
    value.gpuCompletionHandle = fenceHandle;
    value.gpuReadySequence = ready;
    value.gpuConsumerReleaseSequence = ready + 1u;
    value.gpuCompletionPrimitive =
        fnvxr::gpu::GpuCompletionPrimitive::D3D11SharedFence;
    value.presentationMode =
        fnvxr::gpu::color_v5::PresentationMode::BinocularWorld;
    value.resourceSetId = resourceSet;
    value.producerEpoch = epoch;
    value.producerProcessId = producerPid;
    value.adapterLuid = 0xAABBCCDDu;
    value.transactionId = 0x1000u + ordinal;
    value.sourceFrame = 0x2000u + ordinal;
    value.poseSequence = 0x3000u + ordinal;
    value.runtimeStateSample = 0x4000u + ordinal;
    value.renderedDisplayTime = static_cast<std::int64_t>(0x5000u + ordinal);
    value.leftColor = {
        leftHandle,
        2064u,
        2208u,
        fnvxr::gpu::GpuInteropFormat::R8G8B8A8Unorm,
        fnvxr::gpu::GpuSharedHandleType::D3D11NtHandle,
    };
    value.rightColor = {
        rightHandle,
        2064u,
        2208u,
        fnvxr::gpu::GpuInteropFormat::R8G8B8A8Unorm,
        fnvxr::gpu::GpuSharedHandleType::D3D11NtHandle,
    };
    return value;
}

void publish(Descriptor& shared, const Payload& value)
{
    require(fnvxr::gpu::color_v5::publish(
            &shared,
            [&value](Payload& destination) {
                destination = value;
                return true;
            }),
        "test descriptor publication failed");
}

struct State
{
    std::vector<Call> calls;
    Descriptor* shared = nullptr;
    Payload mutation {};
    bool mutateOnOpen = false;
    bool openSucceeds = true;
    bool alive = true;
    bool waitSucceeds = true;
    bool leftSucceeds = true;
    bool rightSucceeds = true;
    bool signalSucceeds = true;
    std::uint32_t expectedProducerPid = 4321u;
    std::uint64_t waited = 0u;
    std::uint64_t signaled = 0u;
};

bool openResource(void* opaque, const Descriptor&) noexcept
{
    auto* state = static_cast<State*>(opaque);
    state->calls.push_back(Call::Open);
    if (state->mutateOnOpen && state->shared)
        publish(*state->shared, state->mutation);
    return state->openSucceeds;
}

void closeResource(void* opaque) noexcept
{
    static_cast<State*>(opaque)->calls.push_back(Call::Close);
}

bool producerAlive(void* opaque, std::uint32_t pid) noexcept
{
    auto* state = static_cast<State*>(opaque);
    state->calls.push_back(Call::Alive);
    return state->alive && pid == state->expectedProducerPid;
}

bool waitReady(void* opaque, std::uint64_t value) noexcept
{
    auto* state = static_cast<State*>(opaque);
    state->calls.push_back(Call::Wait);
    state->waited = value;
    return state->waitSucceeds;
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

bool signalRelease(void* opaque, std::uint64_t value) noexcept
{
    auto* state = static_cast<State*>(opaque);
    state->calls.push_back(Call::Signal);
    state->signaled = value;
    return state->signalSucceeds;
}

ConsumerOperations operations(State& state)
{
    return {
        &state,
        0xAABBCCDDu,
        1234u,
        &openResource,
        &closeResource,
        &producerAlive,
        &waitReady,
        &copyLeft,
        &copyRight,
        &signalRelease,
    };
}

ConsumeResult firstConsume(
    Consumer& consumer,
    State& state,
    Descriptor& shared)
{
    state.shared = &shared;
    require(consumer.initialize(operations(state)),
        "consumer did not initialize with complete GPU operations");
    publish(shared, frame());
    return consumer.consume(&shared);
}
}

int main()
{
    static_assert(operationsComplete(ConsumerOperations {
        nullptr,
        1u,
        1u,
        &openResource,
        &closeResource,
        &producerAlive,
        &waitReady,
        &copyLeft,
        &copyRight,
        &signalRelease,
    }));

    {
        Descriptor shared {};
        State state;
        Consumer consumer;
        const ConsumeResult result = firstConsume(
            consumer,
            state,
            shared);
        require(result.disposition == ConsumeDisposition::FrameReady
                && result.failure == ConsumerFailure::None,
            "valid first stereo frame was rejected");
        require(state.calls == std::vector<Call> {
                Call::Open,
                Call::Alive,
                Call::Wait,
                Call::Left,
                Call::Right,
                Call::Signal,
            },
            "resource open, GPU wait/copies, and release signal order changed");
        require(state.waited == 1u
                && state.signaled == 2u
                && result.frame.readySequence == 1u
                && result.frame.releaseSequence == 2u,
            "the first [ready,release] fence interval was not [1,2]");

        state.calls.clear();
        const ConsumeResult unchanged = consumer.consume(&shared);
        require(unchanged.disposition == ConsumeDisposition::NoNewFrame
                && unchanged.failure == ConsumerFailure::None
                && state.calls == std::vector<Call> { Call::Alive },
            "an unchanged publication was recopied or treated as a new frame");

        state.calls.clear();
        publish(shared, frame(3u, 2u));
        const ConsumeResult second = consumer.consume(&shared);
        require(second.disposition == ConsumeDisposition::FrameReady
                && second.frame.readySequence == 3u
                && state.calls == std::vector<Call> {
                    Call::Alive,
                    Call::Wait,
                    Call::Left,
                    Call::Right,
                    Call::Signal,
                },
            "a monotonic second frame reopened resources or skipped GPU work");

        state.calls.clear();
        publish(shared, frame(
            1u,
            3u,
            0x41u,
            0x51u,
            0x201u,
            0x202u,
            0x203u,
            5432u));
        state.expectedProducerPid = 5432u;
        const ConsumeResult reopened = consumer.consume(&shared);
        require(reopened.disposition == ConsumeDisposition::FrameReady
                && state.calls == std::vector<Call> {
                    Call::Close,
                    Call::Open,
                    Call::Alive,
                    Call::Wait,
                    Call::Left,
                    Call::Right,
                    Call::Signal,
                },
            "producer PID/epoch/handle change did not cleanly close and reopen");
    }

    {
        Descriptor shared {};
        State state;
        state.shared = &shared;
        state.mutateOnOpen = true;
        state.mutation = frame(3u, 2u);
        Consumer consumer;
        require(consumer.initialize(operations(state)),
            "publication-race consumer did not initialize");
        publish(shared, frame());
        const ConsumeResult result = consumer.consume(&shared);
        require(result.failure
                    == ConsumerFailure::PublicationChangedDuringOpen
                && state.calls == std::vector<Call> {
                    Call::Open,
                    Call::Close,
                },
            "a publication change during handle duplication was admitted");
    }

    {
        Descriptor shared {};
        State state;
        state.openSucceeds = false;
        Consumer consumer;
        const ConsumeResult result = firstConsume(
            consumer,
            state,
            shared);
        require(result.failure == ConsumerFailure::ResourceOpen
                && state.calls == std::vector<Call> {
                    Call::Open,
                    Call::Close,
                },
            "partial resource-open failure was not cleaned up");
    }

    struct InjectedFailure
    {
        bool State::*flag;
        ConsumerFailure expected;
        std::vector<Call> expectedCalls;
    };
    const std::vector<InjectedFailure> injected {
        {
            &State::alive,
            ConsumerFailure::ProducerExited,
            { Call::Open, Call::Alive, Call::Close },
        },
        {
            &State::waitSucceeds,
            ConsumerFailure::FenceWait,
            { Call::Open, Call::Alive, Call::Wait, Call::Close },
        },
        {
            &State::leftSucceeds,
            ConsumerFailure::LeftGpuCopy,
            {
                Call::Open,
                Call::Alive,
                Call::Wait,
                Call::Left,
                Call::Close,
            },
        },
        {
            &State::rightSucceeds,
            ConsumerFailure::RightGpuCopy,
            {
                Call::Open,
                Call::Alive,
                Call::Wait,
                Call::Left,
                Call::Right,
                Call::Close,
            },
        },
        {
            &State::signalSucceeds,
            ConsumerFailure::FenceReleaseSignal,
            {
                Call::Open,
                Call::Alive,
                Call::Wait,
                Call::Left,
                Call::Right,
                Call::Signal,
                Call::Close,
            },
        },
    };
    for (const InjectedFailure& fault : injected)
    {
        Descriptor shared {};
        State state;
        state.*(fault.flag) = false;
        Consumer consumer;
        const ConsumeResult result = firstConsume(
            consumer,
            state,
            shared);
        require(result.disposition == ConsumeDisposition::Rejected
                && result.failure == fault.expected
                && state.calls == fault.expectedCalls
                && consumer.lastFrame().readySequence == 0u,
            "fault-injected GPU step exposed a partial stereo frame");
    }

    {
        Descriptor shared {};
        State state;
        Consumer consumer;
        require(firstConsume(consumer, state, shared).disposition
                == ConsumeDisposition::FrameReady,
            "sequence test first frame failed");
        state.calls.clear();
        publish(shared, frame(5u, 2u));
        const ConsumeResult skipped = consumer.consume(&shared);
        require(skipped.failure == ConsumerFailure::FenceSequenceInvalid
                && state.calls == std::vector<Call> {
                    Call::Alive,
                    Call::Close,
                },
            "a skipped shared-fence ownership interval was admitted");
    }

    {
        Descriptor shared {};
        State state;
        Consumer consumer;
        require(firstConsume(consumer, state, shared).disposition
                == ConsumeDisposition::FrameReady,
            "identity-regression test first frame failed");
        state.calls.clear();
        Payload regressed = frame(3u, 2u);
        regressed.transactionId = frame().transactionId;
        publish(shared, regressed);
        const ConsumeResult result = consumer.consume(&shared);
        require(result.failure == ConsumerFailure::FrameIdentityRegression
                && state.calls == std::vector<Call> {
                    Call::Alive,
                    Call::Close,
                },
            "a frame identity rollback reached GPU copy operations");
    }

    {
        Descriptor shared {};
        State state;
        Consumer consumer;
        require(firstConsume(consumer, state, shared).disposition
                == ConsumeDisposition::FrameReady,
            "same-source-frame test first frame failed");
        state.calls.clear();
        Payload sameSource = frame(3u, 2u);
        sameSource.sourceFrame = frame().sourceFrame;
        publish(shared, sameSource);
        require(consumer.consume(&shared).disposition
                == ConsumeDisposition::FrameReady,
            "a new transaction from the same OpenXR pose frame was rejected");

        state.calls.clear();
        Payload sourceRollback = frame(5u, 3u);
        sourceRollback.sourceFrame = frame().sourceFrame - 1u;
        publish(shared, sourceRollback);
        const ConsumeResult rollback = consumer.consume(&shared);
        require(rollback.failure == ConsumerFailure::FrameIdentityRegression
                && state.calls == std::vector<Call> {
                    Call::Alive,
                    Call::Close,
                },
            "a source pose frame rollback reached GPU copy operations");
    }

    {
        Descriptor shared {};
        State state;
        Consumer consumer;
        require(consumer.initialize(operations(state)),
            "adapter-mismatch consumer did not initialize");
        Payload wrongAdapter = frame();
        wrongAdapter.adapterLuid = 0xBADu;
        publish(shared, wrongAdapter);
        const ConsumeResult result = consumer.consume(&shared);
        require(result.failure == ConsumerFailure::AdapterMismatch
                && state.calls == std::vector<Call> { Call::Close },
            "a producer on the wrong adapter reached handle duplication");
    }

    {
        Descriptor shared {};
        State state;
        Consumer consumer;
        require(consumer.initialize(operations(state)),
            "unstable-publication consumer did not initialize");
        publish(shared, frame());
        const std::uint32_t stable = shared.publicationSequence.load();
        shared.publicationSequence.store(stable + 1u);
        const ConsumeResult result = consumer.consume(&shared);
        require(result.failure == ConsumerFailure::UnstablePublication
                && state.calls.empty(),
            "an odd seqlock publication reached resource operations");
    }

    {
        Win32Consumer consumer;
        require(!consumer.initialize(nullptr)
                && consumer.backendFailure()
                    == Win32ConsumerFailure::InvalidDevice
                && !consumer.eyeTexture(0u)
                && !consumer.eyeView(1u),
            "the concrete Windows backend admitted a null D3D11 device");
    }

    {
        ID3D11Device* device = nullptr;
        D3D_FEATURE_LEVEL createdLevel {};
        const D3D_FEATURE_LEVEL levels[] {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
        };
        HRESULT result = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            levels,
            static_cast<UINT>(std::size(levels)),
            D3D11_SDK_VERSION,
            &device,
            &createdLevel,
            nullptr);
        if (FAILED(result))
        {
            result = D3D11CreateDevice(
                nullptr,
                D3D_DRIVER_TYPE_WARP,
                nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                levels,
                static_cast<UINT>(std::size(levels)),
                D3D11_SDK_VERSION,
                &device,
                &createdLevel,
                nullptr);
        }
        require(SUCCEEDED(result) && device,
            "could not create the concrete consumer test device");

        wchar_t absentMapping[96] {};
        swprintf_s(
            absentMapping,
            L"Local\\FNVXR_GPU_Color_Consumer_Absent_%lu",
            static_cast<unsigned long>(GetCurrentProcessId()));
        Win32Consumer consumer;
        require(consumer.initialize(device, absentMapping)
                && consumer.ready(),
            "consumer initialization incorrectly required a producer mapping");
        const ConsumeResult absent = consumer.consume();
        require(absent.disposition == ConsumeDisposition::Rejected
                && absent.failure == ConsumerFailure::NotInitialized
                && consumer.backendFailure()
                    == Win32ConsumerFailure::MappingUnavailable
                && consumer.ready(),
            "an absent producer mapping did not fail one consume without deinitializing the device consumer");
        consumer.reset();
        device->Release();
    }

    std::cout
        << "GPU-only ABI-v5 host consumer and fault-injection tests passed\n";
    return EXIT_SUCCESS;
}
