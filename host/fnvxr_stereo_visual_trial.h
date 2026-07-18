#pragma once

#include "fnvxr_gpu_color_consumer.h"
#include "../protocol/fnvxr_shared_state.h"

#include <cstdint>
#include <limits>

namespace fnvxr::host::stereo_visual_trial
{
// Auditable source authorization for one bounded, visual-only headset trial.
// This authorizes only the two eye-view bindings returned by this file.  It is
// deliberately unrelated to product acceptance, input, HUD, weapon, or muzzle
// authority, none of which this interface can express.
inline constexpr bool CompiledStereoVisualTrialAuthorization = true;

enum class Failure : std::uint8_t
{
    None,
    CompiledTrialNotAuthorized,
    ConsumerDispositionRejected,
    ConsumerFailureReported,
    PresentationNotBinocularWorld,
    ConsumerIdentityIncomplete,
    PoseSequenceMismatch,
    DisplayTimeMismatch,
    HostProducerEpochMissing,
    HostProducerEpochChanged,
    ReferenceSpaceGenerationMissing,
    ReferenceSpaceGenerationChanged,
    EyePoseOrFovIncomplete,
    SourceViewsNotDistinct,
    SourcePoseOutsideFreshnessBudget,
    RuntimeSourceUnavailable,
    RuntimeCurrentUnavailable,
    RuntimeSourceSampleMismatch,
    RuntimeSampleRegressed,
    RuntimePresentationChanged,
    RuntimeNotGameplay,
    RuntimeCameraInactive,
    LeftEyeSrvUnavailable,
    RightEyeSrvUnavailable,
    EyeSrvsAliased,
};

inline constexpr const char* failureName(Failure failure) noexcept
{
    switch (failure)
    {
    case Failure::None:
        return "none";
    case Failure::CompiledTrialNotAuthorized:
        return "compiled_trial_not_authorized";
    case Failure::ConsumerDispositionRejected:
        return "consumer_disposition_rejected";
    case Failure::ConsumerFailureReported:
        return "consumer_failure_reported";
    case Failure::PresentationNotBinocularWorld:
        return "presentation_not_binocular_world";
    case Failure::ConsumerIdentityIncomplete:
        return "consumer_identity_incomplete";
    case Failure::PoseSequenceMismatch:
        return "pose_sequence_mismatch";
    case Failure::DisplayTimeMismatch:
        return "display_time_mismatch";
    case Failure::HostProducerEpochMissing:
        return "host_producer_epoch_missing";
    case Failure::HostProducerEpochChanged:
        return "host_producer_epoch_changed";
    case Failure::ReferenceSpaceGenerationMissing:
        return "reference_space_generation_missing";
    case Failure::ReferenceSpaceGenerationChanged:
        return "reference_space_generation_changed";
    case Failure::EyePoseOrFovIncomplete:
        return "eye_pose_or_fov_incomplete";
    case Failure::SourceViewsNotDistinct:
        return "source_views_not_distinct";
    case Failure::SourcePoseOutsideFreshnessBudget:
        return "source_pose_outside_freshness_budget";
    case Failure::RuntimeSourceUnavailable:
        return "runtime_source_unavailable";
    case Failure::RuntimeCurrentUnavailable:
        return "runtime_current_unavailable";
    case Failure::RuntimeSourceSampleMismatch:
        return "runtime_source_sample_mismatch";
    case Failure::RuntimeSampleRegressed:
        return "runtime_sample_regressed";
    case Failure::RuntimePresentationChanged:
        return "runtime_presentation_changed";
    case Failure::RuntimeNotGameplay:
        return "runtime_not_gameplay";
    case Failure::RuntimeCameraInactive:
        return "runtime_camera_inactive";
    case Failure::LeftEyeSrvUnavailable:
        return "left_eye_srv_unavailable";
    case Failure::RightEyeSrvUnavailable:
        return "right_eye_srv_unavailable";
    case Failure::EyeSrvsAliased:
        return "eye_srvs_aliased";
    }
    return "unknown_stereo_visual_trial_failure";
}

struct SourcePoseLineage
{
    std::uint64_t poseSequence = 0u;
    std::int64_t predictedDisplayTime = 0;
    std::uint64_t hostProducerEpoch = 0u;
    std::uint64_t currentHostProducerEpoch = 0u;
    std::uint32_t referenceSpaceGeneration = 0u;
    std::uint32_t currentReferenceSpaceGeneration = 0u;
    bool completeEyePosesAndFov = false;
    bool distinctBinocularViews = false;
};

struct RuntimeLineageSample
{
    std::uint64_t sample = 0u;
    std::uint32_t phase = shared::RuntimePhaseUnknown;
    std::uint32_t menuBits = 0u;
    std::uint32_t showroomActive = 0u;
    bool cameraActive = false;
    bool fresh = false;
};

struct Input
{
    gpu_color::ConsumeResult consumed {};
    SourcePoseLineage sourcePose {};
    bool sourcePoseWithinFreshnessBudget = false;
    RuntimeLineageSample sourceRuntime {};
    RuntimeLineageSample currentRuntime {};
    ID3D11ShaderResourceView* leftEyeSrv = nullptr;
    ID3D11ShaderResourceView* rightEyeSrv = nullptr;
};

struct Bindings
{
    gpu_color::ConsumerFrame frame {};
    ID3D11ShaderResourceView* leftEyeSrv = nullptr;
    ID3D11ShaderResourceView* rightEyeSrv = nullptr;
    bool bindLeftEyeSrv = false;
    bool bindRightEyeSrv = false;
    bool retainedConsumerFrame = false;
};

struct Decision
{
    Failure failure = Failure::CompiledTrialNotAuthorized;
    gpu_color::ConsumerFailure reportedConsumerFailure =
        gpu_color::ConsumerFailure::NotInitialized;
    Bindings bindings {};

    bool bindsStereoVisuals() const noexcept
    {
        return failure == Failure::None
            && bindings.bindLeftEyeSrv
            && bindings.bindRightEyeSrv
            && bindings.leftEyeSrv
            && bindings.rightEyeSrv
            && bindings.leftEyeSrv != bindings.rightEyeSrv;
    }
};

namespace detail
{
inline bool consumerIdentityComplete(
    const gpu_color::ConsumerFrame& frame) noexcept
{
    return (frame.format == gpu::GpuInteropFormat::R8G8B8A8Unorm
            || frame.format == gpu::GpuInteropFormat::R10G10B10A2Unorm
            || frame.format == gpu::GpuInteropFormat::R16G16B16A16Float)
        && frame.width != 0u
        && frame.height != 0u
        && frame.width <= gpu::color_v5::MaximumTextureDimension
        && frame.height <= gpu::color_v5::MaximumTextureDimension
        && frame.resourceSetId != 0u
        && frame.producerEpoch != 0u
        && frame.producerProcessId != 0u
        && frame.adapterLuid != 0u
        && frame.transactionId != 0u
        && frame.sourceFrame != 0u
        && frame.poseSequence != 0u
        && frame.runtimeStateSample != 0u
        && frame.renderedDisplayTime != 0
        && frame.readySequence != 0u
        && (frame.readySequence & 1u) != 0u
        && frame.readySequence
            != (std::numeric_limits<std::uint64_t>::max)()
        && frame.releaseSequence == frame.readySequence + 1u;
}

inline bool sameRuntimePresentationState(
    const RuntimeLineageSample& source,
    const RuntimeLineageSample& current) noexcept
{
    return source.phase == current.phase
        && source.menuBits == current.menuBits
        && source.showroomActive == current.showroomActive
        && source.cameraActive == current.cameraActive;
}

inline Decision reject(const Input& input, Failure failure) noexcept
{
    Decision decision {};
    decision.failure = failure;
    decision.reportedConsumerFailure = input.consumed.failure;
    return decision;
}

inline Decision evaluateAuthorized(const Input& input) noexcept
{
    if (input.consumed.disposition
            != gpu_color::ConsumeDisposition::FrameReady
        && input.consumed.disposition
            != gpu_color::ConsumeDisposition::NoNewFrame)
    {
        return reject(input, Failure::ConsumerDispositionRejected);
    }
    if (input.consumed.failure != gpu_color::ConsumerFailure::None)
        return reject(input, Failure::ConsumerFailureReported);

    const gpu_color::ConsumerFrame& frame = input.consumed.frame;
    if (frame.presentationMode
        != gpu::color_v5::PresentationMode::BinocularWorld)
    {
        return reject(input, Failure::PresentationNotBinocularWorld);
    }
    if (!consumerIdentityComplete(frame))
        return reject(input, Failure::ConsumerIdentityIncomplete);

    const SourcePoseLineage& pose = input.sourcePose;
    if (frame.poseSequence != pose.poseSequence)
        return reject(input, Failure::PoseSequenceMismatch);
    if (frame.renderedDisplayTime != pose.predictedDisplayTime)
        return reject(input, Failure::DisplayTimeMismatch);
    if (pose.hostProducerEpoch == 0u
        || pose.currentHostProducerEpoch == 0u)
    {
        return reject(input, Failure::HostProducerEpochMissing);
    }
    if (pose.hostProducerEpoch != pose.currentHostProducerEpoch)
        return reject(input, Failure::HostProducerEpochChanged);
    if (pose.referenceSpaceGeneration == 0u
        || pose.currentReferenceSpaceGeneration == 0u)
    {
        return reject(input, Failure::ReferenceSpaceGenerationMissing);
    }
    if (pose.referenceSpaceGeneration
        != pose.currentReferenceSpaceGeneration)
    {
        return reject(input, Failure::ReferenceSpaceGenerationChanged);
    }
    if (!pose.completeEyePosesAndFov)
        return reject(input, Failure::EyePoseOrFovIncomplete);
    if (!pose.distinctBinocularViews)
        return reject(input, Failure::SourceViewsNotDistinct);
    if (!input.sourcePoseWithinFreshnessBudget)
        return reject(input, Failure::SourcePoseOutsideFreshnessBudget);

    const RuntimeLineageSample& sourceRuntime = input.sourceRuntime;
    const RuntimeLineageSample& currentRuntime = input.currentRuntime;
    if (!sourceRuntime.fresh || sourceRuntime.sample == 0u)
        return reject(input, Failure::RuntimeSourceUnavailable);
    if (!currentRuntime.fresh || currentRuntime.sample == 0u)
        return reject(input, Failure::RuntimeCurrentUnavailable);
    if (frame.runtimeStateSample != sourceRuntime.sample)
        return reject(input, Failure::RuntimeSourceSampleMismatch);
    if (sourceRuntime.sample > currentRuntime.sample)
        return reject(input, Failure::RuntimeSampleRegressed);
    if (!sameRuntimePresentationState(sourceRuntime, currentRuntime))
        return reject(input, Failure::RuntimePresentationChanged);
    if (!shared::runtimeGameplayPhase(
            sourceRuntime.phase,
            sourceRuntime.menuBits,
            sourceRuntime.showroomActive)
        || !shared::runtimeGameplayPhase(
            currentRuntime.phase,
            currentRuntime.menuBits,
            currentRuntime.showroomActive))
    {
        return reject(input, Failure::RuntimeNotGameplay);
    }
    if (!sourceRuntime.cameraActive || !currentRuntime.cameraActive)
        return reject(input, Failure::RuntimeCameraInactive);

    if (!input.leftEyeSrv)
        return reject(input, Failure::LeftEyeSrvUnavailable);
    if (!input.rightEyeSrv)
        return reject(input, Failure::RightEyeSrvUnavailable);
    if (input.leftEyeSrv == input.rightEyeSrv)
        return reject(input, Failure::EyeSrvsAliased);

    Decision decision {};
    decision.failure = Failure::None;
    decision.reportedConsumerFailure = gpu_color::ConsumerFailure::None;
    decision.bindings.frame = frame;
    decision.bindings.leftEyeSrv = input.leftEyeSrv;
    decision.bindings.rightEyeSrv = input.rightEyeSrv;
    decision.bindings.bindLeftEyeSrv = true;
    decision.bindings.bindRightEyeSrv = true;
    decision.bindings.retainedConsumerFrame = input.consumed.disposition
        == gpu_color::ConsumeDisposition::NoNewFrame;
    return decision;
}

template <bool CompiledAuthorization>
inline Decision evaluate(const Input& input) noexcept
{
    if constexpr (!CompiledAuthorization)
    {
        return reject(input, Failure::CompiledTrialNotAuthorized);
    }
    else
    {
        return evaluateAuthorized(input);
    }
}
}

inline Decision decide(const Input& input) noexcept
{
    return detail::evaluate<CompiledStereoVisualTrialAuthorization>(input);
}
}
