#include "fnvxr_retail_runtime_publication.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

namespace
{
int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}

LONG sequenceFromBits(std::uint32_t bits)
{
    LONG sequence = 0;
    std::memcpy(&sequence, &bits, sizeof(sequence));
    return sequence;
}

fnvxr::engine::RetailRuntimePublicationObservation readyObservation()
{
    fnvxr::engine::RetailRuntimePublicationObservation observation {};
    observation.sequenceBefore = 4;
    observation.sequenceAfter = 4;
    observation.state.magic = fnvxr::shared::RuntimeSharedMagic;
    observation.state.version = fnvxr::shared::RuntimeSharedVersion;
    observation.state.sequence = 4;
    observation.state.frame = 1u;
    observation.state.phase = fnvxr::shared::RuntimePhaseGameplay;
    return observation;
}
}

int main()
{
    using fnvxr::engine::RetailPluginMainLoopDisposition;
    using fnvxr::engine::RetailPluginMainLoopRequest;
    using fnvxr::engine::RetailRuntimePublicationReadinessFailure;
    using fnvxr::engine::assessRetailRuntimePublicationReadiness;
    using fnvxr::engine::retailPluginMainLoopDisposition;
    using fnvxr::engine::retailRuntimeCameraActive;

    if (retailPluginMainLoopDisposition({ false, false })
            != RetailPluginMainLoopDisposition::FullBridge
        || retailPluginMainLoopDisposition({ false, true })
            != RetailPluginMainLoopDisposition::FullBridge)
    {
        return fail("the visual-trial policy altered another run profile");
    }
    if (retailPluginMainLoopDisposition(
            RetailPluginMainLoopRequest { true, false })
        != RetailPluginMainLoopDisposition::RejectVisualTrial)
    {
        return fail("visual-trial profile without engine-center opt-in was admitted");
    }
    if (retailPluginMainLoopDisposition(
            RetailPluginMainLoopRequest { true, true })
        != RetailPluginMainLoopDisposition::PublishRuntimeOnly)
    {
        return fail("visual trial did not isolate the plugin to runtime publication");
    }
    if (!retailRuntimeCameraActive(true, false, false))
        return fail("the normal bridge lost its shared camera observation");
    if (retailRuntimeCameraActive(false, false, true))
    {
        return fail(
            "an unapproved profile borrowed a read-only live camera observation");
    }
    if (!retailRuntimeCameraActive(false, true, true))
    {
        return fail(
            "an approved publication-only profile withheld an observed live camera");
    }
    if (retailRuntimeCameraActive(true, true, false))
    {
        return fail(
            "an approved publication-only profile reused a stale shared camera");
    }
    if (retailRuntimeCameraActive(false, true, false))
    {
        return fail(
            "an approved publication-only profile invented a missing live camera");
    }

    auto ready = readyObservation();
    if (!assessRetailRuntimePublicationReadiness(ready).complete())
        return fail("a stable main-loop runtime publication was rejected");

    auto neutral = ready;
    neutral.sequenceBefore = 2;
    neutral.sequenceAfter = 2;
    neutral.state.sequence = 2;
    neutral.state.frame = 0u;
    neutral.state.phase = fnvxr::shared::RuntimePhaseUnknown;
    if (assessRetailRuntimePublicationReadiness(neutral).failure
        != RetailRuntimePublicationReadinessFailure::NeutralPublication)
    {
        return fail("the plugin's neutral publication was accepted");
    }

    auto unpublished = ready;
    unpublished.sequenceBefore = 3;
    unpublished.sequenceAfter = 3;
    unpublished.state.sequence = 3;
    if (assessRetailRuntimePublicationReadiness(unpublished).failure
        != RetailRuntimePublicationReadinessFailure::SequenceUnpublished)
    {
        return fail("an in-progress runtime publication was accepted");
    }

    auto changed = ready;
    changed.sequenceAfter = 6;
    if (assessRetailRuntimePublicationReadiness(changed).failure
        != RetailRuntimePublicationReadinessFailure::PublicationChanged)
    {
        return fail("a changing runtime publication was accepted");
    }

    auto wrongSnapshotSequence = ready;
    wrongSnapshotSequence.state.sequence = 2;
    if (assessRetailRuntimePublicationReadiness(wrongSnapshotSequence).failure
        != RetailRuntimePublicationReadinessFailure::PublicationChanged)
    {
        return fail("a snapshot from another sequence was accepted");
    }

    auto invalidHeader = ready;
    invalidHeader.state.version += 1u;
    if (assessRetailRuntimePublicationReadiness(invalidHeader).failure
        != RetailRuntimePublicationReadinessFailure::HeaderInvalid)
    {
        return fail("a runtime publication with the wrong header was accepted");
    }

    auto loading = ready;
    loading.state.phase = fnvxr::shared::RuntimePhaseLoading;
    loading.state.menuBits = fnvxr::shared::RuntimeLoadingMenuBit;
    if (!assessRetailRuntimePublicationReadiness(loading).complete())
        return fail("a coherent loading publication was rejected");

    auto menu = ready;
    menu.state.phase = fnvxr::shared::RuntimePhaseMenu;
    menu.state.menuBits = fnvxr::shared::RuntimeDialogMenuBit;
    menu.state.uiInputAllowed = 1u;
    if (!assessRetailRuntimePublicationReadiness(menu).complete())
        return fail("a coherent menu publication was rejected");

    auto phaseMismatch = menu;
    phaseMismatch.state.phase = fnvxr::shared::RuntimePhaseGameplay;
    if (assessRetailRuntimePublicationReadiness(phaseMismatch).failure
        != RetailRuntimePublicationReadinessFailure::PayloadInvalid)
    {
        return fail("a phase/menu mismatch was accepted");
    }

    auto uiMismatch = menu;
    uiMismatch.state.uiInputAllowed = 0u;
    if (assessRetailRuntimePublicationReadiness(uiMismatch).failure
        != RetailRuntimePublicationReadinessFailure::PayloadInvalid)
    {
        return fail("an inconsistent UI-input observation was accepted");
    }

    auto wrapped = ready;
    wrapped.sequenceBefore = sequenceFromBits(0x80000000u);
    wrapped.sequenceAfter = wrapped.sequenceBefore;
    wrapped.state.sequence = wrapped.sequenceBefore;
    if (!assessRetailRuntimePublicationReadiness(wrapped).complete())
        return fail("a valid sequence above LONG_MAX was rejected");

    std::cout << "retail runtime publication contract PASS\n";
    return EXIT_SUCCESS;
}
