#include "fnvxr_stereo_visual_trial.h"

#include <cstdlib>
#include <iostream>
#include <type_traits>
#include <utility>

struct ID3D11ShaderResourceView
{
};

namespace
{
namespace trial = fnvxr::host::stereo_visual_trial;
namespace color = fnvxr::host::gpu_color;

template <typename T, typename = void>
struct HasProductGameplayAccepted : std::false_type
{
};

template <typename T>
struct HasProductGameplayAccepted<T,
    std::void_t<decltype(std::declval<T>().productGameplayAccepted)>>
    : std::true_type
{
};

template <typename T, typename = void>
struct HasControllerInput : std::false_type
{
};

template <typename T>
struct HasControllerInput<T,
    std::void_t<decltype(std::declval<T>().controllerInput)>>
    : std::true_type
{
};

template <typename T, typename = void>
struct HasWeapon : std::false_type
{
};

template <typename T>
struct HasWeapon<T, std::void_t<decltype(std::declval<T>().weapon)>>
    : std::true_type
{
};

template <typename T, typename = void>
struct HasMuzzle : std::false_type
{
};

template <typename T>
struct HasMuzzle<T, std::void_t<decltype(std::declval<T>().muzzle)>>
    : std::true_type
{
};

template <typename T, typename = void>
struct HasFullProductSuccess : std::false_type
{
};

template <typename T>
struct HasFullProductSuccess<T,
    std::void_t<decltype(std::declval<T>().fullProductSuccess)>>
    : std::true_type
{
};

static_assert(trial::CompiledStereoVisualTrialAuthorization);
static_assert(!HasProductGameplayAccepted<trial::Decision>::value);
static_assert(!HasControllerInput<trial::Decision>::value);
static_assert(!HasWeapon<trial::Decision>::value);
static_assert(!HasMuzzle<trial::Decision>::value);
static_assert(!HasFullProductSuccess<trial::Decision>::value);
static_assert(!HasProductGameplayAccepted<trial::Bindings>::value);
static_assert(!HasControllerInput<trial::Bindings>::value);
static_assert(!HasWeapon<trial::Bindings>::value);
static_assert(!HasMuzzle<trial::Bindings>::value);
static_assert(!HasFullProductSuccess<trial::Bindings>::value);

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

color::ConsumerFrame completeFrame()
{
    color::ConsumerFrame frame {};
    frame.presentationMode =
        fnvxr::gpu::color_v5::PresentationMode::BinocularWorld;
    frame.format = fnvxr::gpu::GpuInteropFormat::R8G8B8A8Unorm;
    frame.width = 2064u;
    frame.height = 2208u;
    frame.resourceSetId = 0x10u;
    frame.producerEpoch = 0x20u;
    frame.producerProcessId = 0x30u;
    frame.adapterLuid = 0x40u;
    frame.transactionId = 0x50u;
    frame.sourceFrame = 0x60u;
    frame.poseSequence = 0x70u;
    frame.runtimeStateSample = 0x80u;
    frame.renderedDisplayTime = 0x90;
    frame.readySequence = 1u;
    frame.releaseSequence = 2u;
    return frame;
}

trial::Input completeInput(
    ID3D11ShaderResourceView& left,
    ID3D11ShaderResourceView& right)
{
    trial::Input input {};
    input.consumed = {
        color::ConsumeDisposition::FrameReady,
        color::ConsumerFailure::None,
        completeFrame(),
    };
    input.sourcePose.poseSequence = input.consumed.frame.poseSequence;
    input.sourcePose.predictedDisplayTime =
        input.consumed.frame.renderedDisplayTime;
    input.sourcePose.hostProducerEpoch = 0xa0u;
    input.sourcePose.currentHostProducerEpoch = 0xa0u;
    input.sourcePose.referenceSpaceGeneration = 4u;
    input.sourcePose.currentReferenceSpaceGeneration = 4u;
    input.sourcePose.completeEyePosesAndFov = true;
    input.sourcePose.distinctBinocularViews = true;
    input.sourcePoseWithinFreshnessBudget = true;
    input.sourceRuntime.sample = input.consumed.frame.runtimeStateSample;
    input.sourceRuntime.phase = fnvxr::shared::RuntimePhaseGameplay;
    input.sourceRuntime.menuBits = 0u;
    input.sourceRuntime.showroomActive = 0u;
    input.sourceRuntime.cameraActive = true;
    input.sourceRuntime.fresh = true;
    input.currentRuntime = input.sourceRuntime;
    input.currentRuntime.sample += 2u;
    input.leftEyeSrv = &left;
    input.rightEyeSrv = &right;
    return input;
}

void requireRejected(
    const trial::Decision& decision,
    trial::Failure expected,
    const char* message)
{
    require(decision.failure == expected, message);
    require(!decision.bindsStereoVisuals(),
        "rejected decision reported visual bindings");
    require(!decision.bindings.leftEyeSrv
            && !decision.bindings.rightEyeSrv
            && !decision.bindings.bindLeftEyeSrv
            && !decision.bindings.bindRightEyeSrv,
        "rejected decision leaked an eye binding");
    require(trial::failureName(decision.failure)[0] != '\0'
            && decision.failure != trial::Failure::None,
        "rejected decision lacked a telemetry failure name");
}
}

int main()
{
    ID3D11ShaderResourceView left;
    ID3D11ShaderResourceView right;
    const trial::Input complete = completeInput(left, right);

    const trial::Decision ready = trial::decide(complete);
    require(ready.failure == trial::Failure::None
            && ready.bindsStereoVisuals(),
        "complete visual-only stereo evidence did not bind both eyes");
    require(ready.bindings.leftEyeSrv == &left
            && ready.bindings.rightEyeSrv == &right
            && !ready.bindings.retainedConsumerFrame,
        "visual-only decision changed the concrete eye resources");
    require(ready.bindings.frame.transactionId
            == complete.consumed.frame.transactionId,
        "visual-only decision lost the exact Consumer frame identity");

    trial::Input changed = complete;
    changed.consumed.disposition = color::ConsumeDisposition::NoNewFrame;
    const trial::Decision retained = trial::decide(changed);
    require(retained.bindsStereoVisuals()
            && retained.bindings.retainedConsumerFrame,
        "exact retained Consumer frame was not identified as retained");

    requireRejected(
        trial::detail::evaluate<false>(complete),
        trial::Failure::CompiledTrialNotAuthorized,
        "compiled authorization false did not fail closed");

    changed = complete;
    changed.consumed.disposition = color::ConsumeDisposition::Rejected;
    requireRejected(trial::decide(changed),
        trial::Failure::ConsumerDispositionRejected,
        "rejected Consumer disposition was admitted");

    changed = complete;
    changed.consumed.failure = color::ConsumerFailure::FenceWait;
    const trial::Decision consumerFailure = trial::decide(changed);
    requireRejected(consumerFailure,
        trial::Failure::ConsumerFailureReported,
        "reported Consumer failure was admitted");
    require(consumerFailure.reportedConsumerFailure
            == color::ConsumerFailure::FenceWait,
        "upstream Consumer failure was not preserved for telemetry");

    changed = complete;
    changed.consumed.frame.presentationMode =
        fnvxr::gpu::color_v5::PresentationMode::MonoUiQuad;
    requireRejected(trial::decide(changed),
        trial::Failure::PresentationNotBinocularWorld,
        "MonoUiQuad was admitted to the world-eye trial");

    changed = complete;
    changed.consumed.frame.producerProcessId = 0u;
    requireRejected(trial::decide(changed),
        trial::Failure::ConsumerIdentityIncomplete,
        "missing Consumer PID was admitted");
    changed = complete;
    changed.consumed.frame.releaseSequence = 4u;
    requireRejected(trial::decide(changed),
        trial::Failure::ConsumerIdentityIncomplete,
        "invalid Consumer fence interval was admitted");
    changed = complete;
    changed.consumed.frame.format = fnvxr::gpu::GpuInteropFormat::Unknown;
    requireRejected(trial::decide(changed),
        trial::Failure::ConsumerIdentityIncomplete,
        "unknown Consumer format was admitted");

    changed = complete;
    ++changed.sourcePose.poseSequence;
    requireRejected(trial::decide(changed),
        trial::Failure::PoseSequenceMismatch,
        "pose sequence mismatch was admitted");
    changed = complete;
    ++changed.sourcePose.predictedDisplayTime;
    requireRejected(trial::decide(changed),
        trial::Failure::DisplayTimeMismatch,
        "display-time mismatch was admitted");
    changed = complete;
    changed.sourcePose.hostProducerEpoch = 0u;
    requireRejected(trial::decide(changed),
        trial::Failure::HostProducerEpochMissing,
        "missing source host epoch was admitted");
    changed = complete;
    ++changed.sourcePose.currentHostProducerEpoch;
    requireRejected(trial::decide(changed),
        trial::Failure::HostProducerEpochChanged,
        "changed host epoch was admitted");
    changed = complete;
    changed.sourcePose.currentReferenceSpaceGeneration = 0u;
    requireRejected(trial::decide(changed),
        trial::Failure::ReferenceSpaceGenerationMissing,
        "missing current reference-space generation was admitted");
    changed = complete;
    ++changed.sourcePose.currentReferenceSpaceGeneration;
    requireRejected(trial::decide(changed),
        trial::Failure::ReferenceSpaceGenerationChanged,
        "changed reference-space generation was admitted");
    changed = complete;
    changed.sourcePose.completeEyePosesAndFov = false;
    requireRejected(trial::decide(changed),
        trial::Failure::EyePoseOrFovIncomplete,
        "incomplete eye pose/FOV lineage was admitted");
    changed = complete;
    changed.sourcePose.distinctBinocularViews = false;
    requireRejected(trial::decide(changed),
        trial::Failure::SourceViewsNotDistinct,
        "non-distinct source views were admitted");
    changed = complete;
    changed.sourcePoseWithinFreshnessBudget = false;
    requireRejected(trial::decide(changed),
        trial::Failure::SourcePoseOutsideFreshnessBudget,
        "source pose outside the freshness budget was admitted");

    changed = complete;
    changed.sourceRuntime.fresh = false;
    requireRejected(trial::decide(changed),
        trial::Failure::RuntimeSourceUnavailable,
        "stale source runtime was admitted");
    changed = complete;
    changed.currentRuntime.sample = 0u;
    requireRejected(trial::decide(changed),
        trial::Failure::RuntimeCurrentUnavailable,
        "missing current runtime sample was admitted");
    changed = complete;
    ++changed.sourceRuntime.sample;
    requireRejected(trial::decide(changed),
        trial::Failure::RuntimeSourceSampleMismatch,
        "wrong source runtime sample was admitted");
    changed = complete;
    changed.currentRuntime.sample = changed.sourceRuntime.sample - 1u;
    requireRejected(trial::decide(changed),
        trial::Failure::RuntimeSampleRegressed,
        "regressed current runtime sample was admitted");
    changed = complete;
    changed.currentRuntime.menuBits = fnvxr::shared::RuntimePipBoyMenuBit;
    requireRejected(trial::decide(changed),
        trial::Failure::RuntimePresentationChanged,
        "runtime presentation transition was admitted");
    changed = complete;
    changed.sourceRuntime.phase = fnvxr::shared::RuntimePhaseMenu;
    changed.currentRuntime.phase = fnvxr::shared::RuntimePhaseMenu;
    requireRejected(trial::decide(changed),
        trial::Failure::RuntimeNotGameplay,
        "stable menu runtime was admitted as a world visual trial");
    changed = complete;
    changed.sourceRuntime.cameraActive = false;
    changed.currentRuntime.cameraActive = false;
    requireRejected(trial::decide(changed),
        trial::Failure::RuntimeCameraInactive,
        "stable inactive camera runtime was admitted");

    changed = complete;
    changed.leftEyeSrv = nullptr;
    requireRejected(trial::decide(changed),
        trial::Failure::LeftEyeSrvUnavailable,
        "missing left SRV was admitted");
    changed = complete;
    changed.rightEyeSrv = nullptr;
    requireRejected(trial::decide(changed),
        trial::Failure::RightEyeSrvUnavailable,
        "missing right SRV was admitted");
    changed = complete;
    changed.rightEyeSrv = changed.leftEyeSrv;
    requireRejected(trial::decide(changed),
        trial::Failure::EyeSrvsAliased,
        "aliased eye SRVs were admitted");

    for (std::uint8_t raw = 0u;
         raw <= static_cast<std::uint8_t>(trial::Failure::EyeSrvsAliased);
         ++raw)
    {
        require(trial::failureName(static_cast<trial::Failure>(raw))[0] != '\0',
            "enumerated failure lacked a telemetry name");
    }

    std::cout << "Stereo visual-only trial fail-closed tests passed\n";
    return EXIT_SUCCESS;
}
