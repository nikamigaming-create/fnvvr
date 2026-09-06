#pragma once

#include "fnvxr_gpu_color_consumer.h"
#include "fnvxr_runtime_evidence_history.h"
#include "../protocol/fnvxr_product_contract.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace fnvxr::host::gpu_color
{
enum class RoutedContent : std::uint8_t
{
    SafetyBlank,
    BinocularWorld,
    MonoUiQuad,
};

struct RoutedFrame
{
    RoutedContent content = RoutedContent::SafetyBlank;
    ConsumerFrame frame {};
    bool bindLeftEyeView = false;
    bool bindRightEyeView = false;
    bool bindMonoUiView = false;
    bool retainedConsumerFrame = false;
};

struct SourcePoseLineage
{
    std::uint64_t poseSequence = 0u;
    std::int64_t predictedDisplayTime = 0;
    std::uint64_t hostProducerEpoch = 0u;
    std::uint32_t referenceSpaceGeneration = 0u;
    bool completeEyePosesAndFov = false;
    bool distinctBinocularViews = false;
};

// Evidence that only the retail producer can establish.  A v5 color
// publication proves synchronized GPU ownership and identity; it does not by
// itself prove the private depth/cull/resource transaction or the tracked
// first-person weapon.  Keep those claims explicit so the host cannot promote
// "two textures arrived" into full product gameplay acceptance.
struct ProducerWorldEvidence
{
    // This proves per-eye depth/stencil correctness inside the retail render
    // transaction. ABI v5 does not export depth to the host.
    bool renderLocalDepthPairComplete = false;
    bool sameSimulationTick = false;
    bool conservativeVisibilityComplete = false;
    bool resourceGraphComplete = false;
    bool exactShaderSemantics = false;
    bool independentTranslational6Dof = false;
    bool independentRotational6Dof = false;
    bool authoritativeTrackedRetailWeapon = false;
    bool authoritativeMuzzleAlignment = false;
    bool gameplayHudExcluded = false;
};

// Final product binding is a pure projection of PresentationController's
// decision plus concrete resource readiness. It cannot promote runtime intent,
// a transport mode, or a retained texture into another presentation mode.
struct ProductCompositionBindings
{
    RoutedContent content = RoutedContent::SafetyBlank;
    bool bindLeftEyeView = false;
    bool bindRightEyeView = false;
    bool bindMonoUiView = false;
    bool pointerEnabled = false;
};

constexpr ProductCompositionBindings selectProductComposition(
    const product::PresentationDecision& decision,
    bool worldResourcesReady,
    bool selectedUiResourceReady) noexcept
{
    if (decision.hudVisible)
        return {};
    if (decision.mode == product::PresentationMode::WorldStereo
        && (decision.stereoPresentationReady || decision.gameplayVrAccepted)
        && worldResourcesReady)
    {
        return {
            RoutedContent::BinocularWorld,
            true,
            true,
            false,
            false,
        };
    }
    if (decision.mode == product::PresentationMode::UiQuad
        && decision.presentedUiSourceFrame != 0u
        && selectedUiResourceReady)
    {
        return {
            RoutedContent::MonoUiQuad,
            false,
            false,
            true,
            decision.pointerEnabled,
        };
    }
    return {};
}

constexpr bool exactSourcePoseLineage(
    const ConsumerFrame& frame,
    const SourcePoseLineage& lineage,
    std::uint64_t currentHostProducerEpoch,
    std::uint32_t currentReferenceSpaceGeneration) noexcept
{
    return frame.poseSequence != 0u
        && frame.poseSequence == lineage.poseSequence
        && frame.renderedDisplayTime != 0
        && frame.renderedDisplayTime == lineage.predictedDisplayTime
        && lineage.hostProducerEpoch != 0u
        && lineage.hostProducerEpoch == currentHostProducerEpoch
        && lineage.referenceSpaceGeneration != 0u
        && lineage.referenceSpaceGeneration
            == currentReferenceSpaceGeneration
        && lineage.completeEyePosesAndFov
        && lineage.distinctBinocularViews;
}

inline product::PresentationInput makePresentationInput(
    const RuntimeEvidence& runtime,
    const RoutedFrame& routed,
    bool sourcePoseLineageComplete,
    bool sourceFresh,
    ProducerWorldEvidence producerEvidence = {}) noexcept
{
    product::PresentationInput input {};
    input.runtimeStateSample = runtime.sample;
    input.runtimePhase = runtime.phase;
    input.menuBits = runtime.menuBits;
    input.showroomActive = runtime.showroomActive;
    input.runtimeFresh = runtime.fresh;
    input.cameraActive = runtime.cameraActive;

    const bool sampleMatched = runtime.fresh
        && runtime.sample != 0u
        && routed.frame.runtimeStateSample == runtime.sample;
    if (routed.content == RoutedContent::MonoUiQuad
        && sampleMatched
        && sourceFresh)
    {
        input.ui.sourceFrame = routed.frame.sourceFrame;
        input.ui.runtimeStateSample = routed.frame.runtimeStateSample;
        input.ui.retailColorComplete = true;
        input.ui.fresh = true;
        input.ui.retailOwned = true;
    }
    if (routed.content != RoutedContent::BinocularWorld
        || !sampleMatched
        || !sourcePoseLineageComplete
        || !sourceFresh)
    {
        return input;
    }

    product::StereoFrameProof& proof = input.stereo;
    proof.transactionId = routed.frame.transactionId;
    proof.sourceFrame = routed.frame.sourceFrame;
    proof.poseSequence = routed.frame.poseSequence;
    proof.runtimeStateSample = routed.frame.runtimeStateSample;
    // The host independently establishes color transport, GPU completion,
    // distinct views, and exact shared-pose lineage.  Producer-only facts are
    // copied solely from explicit evidence; presentation mode is not evidence.
    proof.colorPairComplete = true;
    proof.renderLocalDepthPairComplete =
        producerEvidence.renderLocalDepthPairComplete;
    proof.retailRenderTransactionComplete = (routed.frame.renderFlags
        & gpu::color_v5::RetailWorldTransactionComplete) != 0u;
    proof.sameSimulationTick = producerEvidence.sameSimulationTick
        || proof.retailRenderTransactionComplete;
    proof.poseMatched = true;
    proof.conservativeVisibilityComplete =
        producerEvidence.conservativeVisibilityComplete;
    proof.resourceGraphComplete = producerEvidence.resourceGraphComplete;
    proof.exactShaderSemantics = producerEvidence.exactShaderSemantics;
    proof.gpuSynchronized = true;
    proof.distinctBinocularViews = true;
    proof.independentTranslational6Dof =
        producerEvidence.independentTranslational6Dof;
    proof.independentRotational6Dof =
        producerEvidence.independentRotational6Dof;
    proof.authoritativeTrackedRetailWeapon =
        producerEvidence.authoritativeTrackedRetailWeapon;
    proof.authoritativeMuzzleAlignment =
        producerEvidence.authoritativeMuzzleAlignment;
    proof.gameplayHudExcluded = producerEvidence.gameplayHudExcluded;
    proof.fresh = true;
    return input;
}

constexpr bool consumerFrameIdentityComplete(
    const ConsumerFrame& frame) noexcept
{
    return gpu::color_v5::validPresentationMode(frame.presentationMode)
        && (frame.format == gpu::GpuInteropFormat::R8G8B8A8Unorm
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

// This adapter deliberately has no retained image state of its own. A
// NoNewFrame disposition may use only the exact ConsumerFrame returned by the
// GPU consumer for this call. A Rejected call therefore removes every world
// binding immediately, even if the concrete consumer still owns private GPU
// textures from an earlier accepted publication.
inline RoutedFrame routeConsumedFrame(
    const ConsumeResult& consumed,
    bool leftEyeViewAvailable,
    bool rightEyeViewAvailable) noexcept
{
    if ((consumed.disposition != ConsumeDisposition::FrameReady
            && consumed.disposition != ConsumeDisposition::NoNewFrame)
        || consumed.failure != ConsumerFailure::None
        || !consumerFrameIdentityComplete(consumed.frame))
    {
        return {};
    }

    RoutedFrame routed {};
    routed.frame = consumed.frame;
    routed.retainedConsumerFrame =
        consumed.disposition == ConsumeDisposition::NoNewFrame;
    if (!leftEyeViewAvailable || !rightEyeViewAvailable)
        return {};
    if (consumed.frame.presentationMode
        == gpu::color_v5::PresentationMode::MonoUiQuad)
    {
        routed.content = RoutedContent::MonoUiQuad;
        routed.bindMonoUiView = true;
        return routed;
    }
    if (consumed.frame.presentationMode
        != gpu::color_v5::PresentationMode::BinocularWorld)
    {
        return {};
    }

    routed.content = RoutedContent::BinocularWorld;
    routed.bindLeftEyeView = true;
    routed.bindRightEyeView = true;
    return routed;
}
}
