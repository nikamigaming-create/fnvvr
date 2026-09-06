#include "../kernel/presentation/coordinator.h"

#include <cstdlib>
#include <iostream>

namespace p = fnvxr::kernel::presentation;

namespace
{
int failures = 0;

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

p::SourceKey source(std::uint64_t frame)
{
    return { 11, frame, frame + 1000 };
}

p::GpuFrameProof gpu(const p::SourceKey& key)
{
    return { key, true, true, true, true, true };
}

p::PresentationInput completeWorld(std::uint64_t frame = 100)
{
    p::PresentationInput input {};
    const p::SourceKey key = source(frame);
    input.runtime = { 50, p::RuntimePhase::Running,
        p::UiClassification::None, true };
    input.world.source = key;
    input.world.runtimeSample = input.runtime.sample;
    input.world.stereoIdentity = { key, key, true };
    input.world.gpu = gpu(key);
    input.world.retail = { true, true, true, true, true, true, true, true, true };
    input.world.completeStereoPair = true;
    input.world.distinctEyeViews = true;
    input.world.runtimeLineageVerified = true;
    input.world.fresh = true;
    input.poseHistory = { key, true };
    return input;
}

p::PresentationInput completeUi(std::uint64_t frame = 200)
{
    p::PresentationInput input {};
    const p::SourceKey key = source(frame);
    input.runtime = { 60, p::RuntimePhase::Running,
        p::UiClassification::Blocking, true };
    input.ui.source = key;
    input.ui.runtimeSample = input.runtime.sample;
    input.ui.gpu = gpu(key);
    input.ui.completeRetailColor = true;
    input.ui.monoRetailView = true;
    input.ui.runtimeLineageVerified = true;
    input.ui.fresh = true;
    return input;
}

void expectWorldRejected(
    const p::PresentationInput& input,
    p::DecisionReason reason,
    const char* message)
{
    p::Coordinator coordinator;
    const p::PresentationDecision decision = coordinator.advance(input);
    require(decision.mode == p::PresentationMode::SafetyBlank
            && decision.reason == reason,
        message);
}

void expectUiRejected(
    const p::PresentationInput& input,
    p::DecisionReason reason,
    const char* message)
{
    p::Coordinator coordinator({ 0, false });
    const p::PresentationDecision decision = coordinator.advance(input);
    require(decision.mode == p::PresentationMode::SafetyBlank
            && decision.reason == reason,
        message);
}
}

int main()
{
    {
        const auto input = completeWorld();
        p::Coordinator coordinator;
        const auto decision = coordinator.advance(input);
        require(decision.mode == p::PresentationMode::WorldStereo
                && decision.reason == p::DecisionReason::WorldFrameReady,
            "all production world evidence must be admitted");
    }

    {
        struct Gate
        {
            bool p::RetailWorldProof::* member;
            const char* message;
        };
        const Gate gates[] = {
            { &p::RetailWorldProof::renderLocalDepthPairComplete,
                "missing retail depth pair was admitted" },
            { &p::RetailWorldProof::conservativeVisibilityComplete,
                "incomplete conservative visibility was admitted" },
            { &p::RetailWorldProof::resourceGraphComplete,
                "incomplete retail resource graph was admitted" },
            { &p::RetailWorldProof::exactShaderSemantics,
                "inexact shader semantics were admitted" },
            { &p::RetailWorldProof::independentTranslational6Dof,
                "unproven translational 6DoF was admitted" },
            { &p::RetailWorldProof::independentRotational6Dof,
                "unproven rotational 6DoF was admitted" },
            { &p::RetailWorldProof::authoritativeTrackedRetailWeapon,
                "non-authoritative retail weapon was admitted" },
            { &p::RetailWorldProof::authoritativeMuzzleAlignment,
                "unproven muzzle alignment was admitted" },
            { &p::RetailWorldProof::gameplayHudExcluded,
                "gameplay HUD contamination was admitted" },
        };
        for (const Gate& gate : gates)
        {
            auto input = completeWorld();
            input.world.retail.*(gate.member) = false;
            expectWorldRejected(input,
                p::DecisionReason::WorldRetailPrerequisitesInvalid,
                gate.message);
        }
    }

    {
        struct Gate
        {
            bool p::GpuFrameProof::* member;
            const char* message;
        };
        const Gate gates[] = {
            { &p::GpuFrameProof::retailProducerOwned,
                "unproven retail GPU ownership was admitted" },
            { &p::GpuFrameProof::producerCompletionObserved,
                "unobserved producer completion was admitted" },
            { &p::GpuFrameProof::consumerOwnershipAcquired,
                "unacquired consumer ownership was admitted" },
            { &p::GpuFrameProof::exclusiveOwnershipInterval,
                "non-exclusive fence interval was admitted" },
            { &p::GpuFrameProof::synchronized,
                "unsynchronized GPU frame was admitted" },
        };
        for (const Gate& gate : gates)
        {
            auto world = completeWorld();
            world.world.gpu.*(gate.member) = false;
            expectWorldRejected(world,
                p::DecisionReason::WorldGpuEvidenceInvalid,
                gate.message);

            auto ui = completeUi();
            ui.ui.gpu.*(gate.member) = false;
            expectUiRejected(ui, p::DecisionReason::UiGpuEvidenceInvalid,
                gate.message);
        }
    }

    {
        auto input = completeWorld();
        input.world.stereoIdentity.leftEye = source(99);
        expectWorldRejected(input,
            p::DecisionReason::WorldStereoIdentityInvalid,
            "left eye from another source was admitted");
        input = completeWorld();
        input.world.stereoIdentity.rightEye = source(99);
        expectWorldRejected(input,
            p::DecisionReason::WorldStereoIdentityInvalid,
            "right eye from another source was admitted");
        input = completeWorld();
        input.world.stereoIdentity.sameSimulationTick = false;
        expectWorldRejected(input,
            p::DecisionReason::WorldStereoIdentityInvalid,
            "cross-tick eye pair was admitted");
    }

    {
        auto input = completeWorld();
        input.world.runtimeLineageVerified = false;
        expectWorldRejected(input,
            p::DecisionReason::WorldRuntimeLineageInvalid,
            "unverified world runtime lineage was admitted");
        input = completeWorld();
        --input.world.runtimeSample;
        expectWorldRejected(input,
            p::DecisionReason::WorldRuntimeLineageInvalid,
            "world from another runtime sample was admitted");
        input = completeWorld();
        input.poseHistory.source = source(99);
        expectWorldRejected(input, p::DecisionReason::WorldPoseJoinMissing,
            "pose from another source was admitted");
        input = completeWorld();
        input.poseHistory.exact = false;
        expectWorldRejected(input, p::DecisionReason::WorldPoseJoinMissing,
            "inexact pose-history lookup was admitted");
    }

    {
        auto input = completeWorld();
        input.world.gpu.source = source(99);
        expectWorldRejected(input, p::DecisionReason::WorldGpuEvidenceInvalid,
            "GPU payload from another world source was admitted");
        auto ui = completeUi();
        ui.ui.gpu.source = source(199);
        expectUiRejected(ui, p::DecisionReason::UiGpuEvidenceInvalid,
            "GPU payload from another UI source was admitted");
    }

    {
        auto ui = completeUi();
        ui.ui.monoRetailView = false;
        expectUiRejected(ui, p::DecisionReason::UiFrameIncomplete,
            "unproven mono UI view was admitted");
        ui = completeUi();
        ui.ui.runtimeLineageVerified = false;
        expectUiRejected(ui, p::DecisionReason::UiRuntimeLineageInvalid,
            "unverified UI runtime lineage was admitted");
        ui = completeUi();
        --ui.ui.runtimeSample;
        expectUiRejected(ui, p::DecisionReason::UiRuntimeLineageInvalid,
            "UI from another runtime sample was admitted");
    }

    {
        p::Coordinator coordinator;
        auto first = completeWorld(300);
        require(coordinator.advance(first).mode == p::PresentationMode::WorldStereo,
            "world ordering fixture failed to establish a watermark");
        auto repeated = completeWorld(300);
        const auto repeatDecision = coordinator.advance(repeated);
        require(repeatDecision.mode == p::PresentationMode::WorldStereo,
            "still-fresh exact world frame must survive faster host cadence");
        repeated.world.fresh = false;
        require(coordinator.advance(repeated).mode == p::PresentationMode::SafetyBlank,
            "expired retained world frame was admitted");
        auto regressed = completeWorld(299);
        require(coordinator.advance(regressed).reason == p::DecisionReason::WorldNotNewerThanAccepted,
            "a regressed world identity was admitted");

        auto oldUi = completeUi(299);
        const auto oldUiDecision = coordinator.advance(oldUi);
        require(oldUiDecision.mode == p::PresentationMode::SafetyBlank
                && oldUiDecision.reason
                    == p::DecisionReason::UiSourceOlderThanWorld,
            "UI older than accepted world was admitted");
    }

    {
        p::Coordinator coordinator;
        auto menu = completeUi(400);
        const auto interactive = coordinator.advance(menu);
        require(interactive.pointerMayRender,
            "interactive blocking UI did not authorize its pointer");

        coordinator.reset();
        menu.runtime.phase = p::RuntimePhase::Loading;
        const auto loading = coordinator.advance(menu);
        require(loading.mode == p::PresentationMode::UiQuad
                && !loading.pointerMayRender,
            "loading UI incorrectly authorized a pointer");
    }

    {
        p::Coordinator coordinator({ 1, true });
        require(coordinator.advance(completeUi(500)).mode
                == p::PresentationMode::UiQuad,
            "UI watermark fixture was not established");
        require(coordinator.advance(completeWorld(499)).mode
                == p::PresentationMode::UiQuad,
            "bounded UI hold did not retain its first transition frame");
        const auto expired = coordinator.advance(completeWorld(499));
        require(expired.mode == p::PresentationMode::SafetyBlank
                && expired.reason == p::DecisionReason::WorldNotNewerThanUi,
            "older world was not blanked when the UI hold expired");
        const auto resurrected = coordinator.advance(completeWorld(499));
        require(resurrected.mode == p::PresentationMode::SafetyBlank
                && resurrected.reason
                    == p::DecisionReason::WorldNotNewerThanUi,
            "expired UI watermark allowed an older world to resurrect");
    }

    {
        p::Coordinator coordinator({ 0, false });
        require(coordinator.advance(completeUi(600)).mode
                == p::PresentationMode::UiQuad,
            "UI ordering fixture was not established");
        const auto regressed = coordinator.advance(completeUi(599));
        require(regressed.mode == p::PresentationMode::SafetyBlank
                && regressed.reason == p::DecisionReason::UiSourceRegression,
            "older UI source replaced the accepted UI watermark");
    }

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
