#pragma once

#include "../protocol/fnvxr_shared_state.h"

#include <cstdint>

namespace fnvxr::engine
{
enum class RetailRuntimePublicationReadinessFailure : std::uint8_t
{
    None = 0u,
    ReaderNotInitialized,
    MappingUnavailable,
    SequenceUnpublished,
    PublicationChanged,
    HeaderInvalid,
    NeutralPublication,
    PayloadInvalid,
    PublicationUnstable,
};

struct RetailRuntimePublicationObservation
{
    shared::SharedRuntimeState state {};
    LONG sequenceBefore = 0;
    LONG sequenceAfter = 0;
};

struct RetailRuntimePublicationReadiness
{
    RetailRuntimePublicationReadinessFailure failure =
        RetailRuntimePublicationReadinessFailure::ReaderNotInitialized;

    constexpr bool complete() const noexcept
    {
        return failure == RetailRuntimePublicationReadinessFailure::None;
    }
};

enum class RetailPluginMainLoopDisposition : std::uint8_t
{
    FullBridge = 0u,
    RejectVisualTrial,
    PublishRuntimeOnly,
};

struct RetailPluginMainLoopRequest
{
    bool stereoVisualTrialProfileSelected = false;
    bool engineCenterStereoRequested = false;
};

constexpr RetailPluginMainLoopDisposition
retailPluginMainLoopDisposition(
    const RetailPluginMainLoopRequest& request) noexcept
{
    if (!request.stereoVisualTrialProfileSelected)
        return RetailPluginMainLoopDisposition::FullBridge;
    return request.engineCenterStereoRequested
        ? RetailPluginMainLoopDisposition::PublishRuntimeOnly
        : RetailPluginMainLoopDisposition::RejectVisualTrial;
}

// The normal bridge derives this bit from its separately published camera
// observation. Explicitly isolated publication-only profiles deliberately do
// not create that wider bridge, so they may substitute only a current
// read-only camera-object observation made on the already authenticated
// main-loop thread.
constexpr bool retailRuntimeCameraActive(
    bool sharedCameraObservationActive,
    bool readOnlyCameraPublicationAuthorized,
    bool currentCameraObjectObserved) noexcept
{
    return readOnlyCameraPublicationAuthorized
        ? currentCameraObjectObserved
        : sharedCameraObservationActive;
}

inline RetailRuntimePublicationReadiness
assessRetailRuntimePublicationReadiness(
    const RetailRuntimePublicationObservation& observation) noexcept
{
    if (!shared::sequencedValueIsPublished(observation.sequenceBefore)
        || !shared::sequencedValueIsPublished(observation.sequenceAfter))
    {
        return {
            RetailRuntimePublicationReadinessFailure::SequenceUnpublished,
        };
    }
    if (observation.sequenceBefore != observation.sequenceAfter
        || observation.state.sequence != observation.sequenceBefore)
    {
        return {
            RetailRuntimePublicationReadinessFailure::PublicationChanged,
        };
    }
    if (observation.state.magic != shared::RuntimeSharedMagic
        || observation.state.version != shared::RuntimeSharedVersion)
    {
        return {
            RetailRuntimePublicationReadinessFailure::HeaderInvalid,
        };
    }

    // publishNeutralPluginOwnedState deliberately leaves frame and phase at
    // zero. A nonzero frame proves that the authenticated main-loop observer
    // has advanced this plugin-owned mapping at least once after neutral
    // initialization.
    if (observation.state.frame == 0u
        || observation.state.phase == shared::RuntimePhaseUnknown)
    {
        return {
            RetailRuntimePublicationReadinessFailure::NeutralPublication,
        };
    }

    constexpr std::uint32_t KnownMenuBits =
        shared::RuntimeMenuModeBit
        | shared::RuntimeStartMenuBit
        | shared::RuntimeRaceSexMenuBit
        | shared::RuntimeDialogMenuBit
        | shared::RuntimeVatsMenuBit
        | shared::RuntimeLoadingMenuBit
        | shared::RuntimePipBoyMenuBit
        | shared::RuntimeGenericMenuBit;
    std::uint32_t expectedPhase = shared::RuntimePhaseGameplay;
    if ((observation.state.menuBits
            & shared::RuntimeLoadingMenuBit)
        != 0u)
    {
        expectedPhase = shared::RuntimePhaseLoading;
    }
    else if ((observation.state.menuBits
                 & shared::RuntimeBlockingMenuBits)
        != 0u)
    {
        expectedPhase = shared::RuntimePhaseMenu;
    }
    if ((observation.state.menuBits & ~KnownMenuBits) != 0u
        || observation.state.phase != expectedPhase
        || observation.state.uiInputAllowed > 1u
        || observation.state.cameraActive > 1u
        || observation.state.showroomActive > 1u
        || observation.state.uiInputAllowed
            != (shared::runtimeUiInputAllowed(
                    observation.state.menuBits)
                    ? 1u
                    : 0u))
    {
        return {
            RetailRuntimePublicationReadinessFailure::PayloadInvalid,
        };
    }

    return { RetailRuntimePublicationReadinessFailure::None };
}
}
