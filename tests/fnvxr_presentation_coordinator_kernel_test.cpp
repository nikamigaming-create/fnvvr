#include "../kernel/presentation/coordinator.h"

#include <cstdlib>
#include <iostream>

namespace p = fnvxr::kernel::presentation;

namespace
{
int failures = 0;

void expect(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

p::SourceKey key(std::uint64_t frame, std::uint64_t transaction = 0)
{
    return { 7, frame, transaction == 0 ? frame + 100 : transaction };
}

p::PresentationInput running(std::uint64_t sample = 10)
{
    p::PresentationInput input {};
    input.runtime = { sample, p::RuntimePhase::Running,
        p::UiClassification::None, true };
    return input;
}

void addWorld(p::PresentationInput& input, p::SourceKey source)
{
    input.world.source = source;
    input.world.runtimeSample = input.runtime.sample;
    input.world.stereoIdentity = { source, source, true };
    input.world.gpu = { source, true, true, true, true, true };
    input.world.retail = { true, true, true, true, true, true, true, true, true };
    input.world.completeStereoPair = true;
    input.world.distinctEyeViews = true;
    input.world.runtimeLineageVerified = true;
    input.world.fresh = true;
    input.poseHistory = { source, true };
}

void addUi(p::PresentationInput& input, p::SourceKey source)
{
    input.ui.source = source;
    input.ui.runtimeSample = input.runtime.sample;
    input.ui.gpu = { source, true, true, true, true, true };
    input.ui.completeRetailColor = true;
    input.ui.monoRetailView = true;
    input.ui.runtimeLineageVerified = true;
    input.ui.fresh = true;
}
}

int main()
{
    {
        p::Coordinator coordinator;
        auto input = running();
        addWorld(input, key(20));
        const auto decision = coordinator.advance(input);
        expect(decision.mode == p::PresentationMode::WorldStereo,
            "complete binocular gameplay must select WorldStereo");
        expect(!decision.spatialRigMayRender && !decision.wristScreenMayRender,
            "world pixels do not authorize unproven spatial overlays");
    }

    {
        p::Coordinator coordinator;
        auto input = running();
        addWorld(input, key(21));
        input.poseHistory = { key(21), true };
        input.trackedRigReady = true;
        const auto decision = coordinator.advance(input);
        expect(decision.spatialRigMayRender,
            "ready rig with exact frame lineage must render");

        addWorld(input, key(22));
        input.poseHistory.source = key(20);
        const auto mismatch = coordinator.advance(input);
        expect(mismatch.mode == p::PresentationMode::SafetyBlank
                && mismatch.reason == p::DecisionReason::WorldPoseJoinMissing,
            "pose history for another source must reject the joined world frame");
    }

    {
        p::Coordinator coordinator;
        auto pipBoy = running();
        pipBoy.runtime.ui = p::UiClassification::PipBoySpatial;
        addWorld(pipBoy, key(30));
        pipBoy.poseHistory = { key(30), true };
        pipBoy.trackedRigReady = true;
        pipBoy.wristContentReady = true;
        const auto ready = coordinator.advance(pipBoy);
        expect(ready.mode == p::PresentationMode::WorldStereo,
            "Pip-Boy must remain binocular world-spatial content");
        expect(ready.spatialRigMayRender && ready.wristScreenMayRender,
            "ready Pip-Boy may render only on an authorized tracked rig");

        addWorld(pipBoy, key(31));
        pipBoy.wristContentReady = false;
        const auto missingContent = coordinator.advance(pipBoy);
        expect(missingContent.mode == p::PresentationMode::WorldStereo
                && missingContent.spatialRigMayRender
                && !missingContent.wristScreenMayRender,
            "missing wrist content must not invent a UI quad");
    }

    {
        p::Coordinator coordinator({ 2, true });
        auto menu = running(50);
        menu.runtime.ui = p::UiClassification::Blocking;
        addUi(menu, key(100));
        const auto shown = coordinator.advance(menu);
        expect(shown.mode == p::PresentationMode::UiQuad
                && shown.selectedSource == key(100),
            "blocking UI must select its proven retail frame");

        auto gameplay = running(51);
        addWorld(gameplay, key(100, 201));
        const auto equalFrame = coordinator.advance(gameplay);
        expect(equalFrame.mode == p::PresentationMode::UiQuad,
            "UI exit must hold when world source frame is not strictly newer");

        addWorld(gameplay, key(101));
        const auto resumed = coordinator.advance(gameplay);
        expect(resumed.mode == p::PresentationMode::WorldStereo
                && resumed.selectedSource == key(101),
            "strictly newer world proof must atomically exit UI");
    }

    {
        p::Coordinator coordinator({ 2, true });
        auto menu = running(60);
        menu.runtime.ui = p::UiClassification::Blocking;
        addUi(menu, key(200));
        coordinator.advance(menu);

        auto gameplay = running(61);
        const auto hold1 = coordinator.advance(gameplay);
        const auto hold2 = coordinator.advance(gameplay);
        const auto blank = coordinator.advance(gameplay);
        expect(hold1.mode == p::PresentationMode::UiQuad
                && hold2.mode == p::PresentationMode::UiQuad,
            "policy must bound an explicit UI-exit hold");
        expect(blank.mode == p::PresentationMode::SafetyBlank
                && blank.reason == p::DecisionReason::HoldExpired,
            "missing world proof must blank when the hold expires");
    }

    {
        p::Coordinator coordinator({ 2, true });
        auto menu = running(62);
        menu.runtime.ui = p::UiClassification::Blocking;
        addUi(menu, key(210));
        coordinator.advance(menu);

        auto gameplay = running(63);
        const p::SourceKey otherEpoch { 8, 211, 311 };
        addWorld(gameplay, otherEpoch);
        coordinator.advance(gameplay);
        coordinator.advance(gameplay);
        const auto rejected = coordinator.advance(gameplay);
        expect(rejected.mode == p::PresentationMode::SafetyBlank
                && rejected.reason == p::DecisionReason::WorldNotNewerThanUi,
            "numeric values from another epoch must not imply source ordering");
    }

    {
        p::Coordinator coordinator;
        auto gameplay = running();
        addUi(gameplay, key(400));
        const auto decision = coordinator.advance(gameplay);
        expect(decision.mode == p::PresentationMode::SafetyBlank,
            "mono retail pixels must never become gameplay presentation");
    }

    {
        p::Coordinator coordinator;
        auto input = running();
        addWorld(input, key(500));
        input.world.distinctEyeViews = false;
        expect(coordinator.advance(input).mode == p::PresentationMode::SafetyBlank,
            "unproven distinct eye views must fail closed");
        input.world.distinctEyeViews = true;
        input.world.runtimeSample = input.runtime.sample - 1;
        expect(coordinator.advance(input).mode == p::PresentationMode::SafetyBlank,
            "runtime-sample mismatch must fail closed");
        input.world.runtimeSample = input.runtime.sample;
        input.world.fresh = false;
        expect(coordinator.advance(input).mode == p::PresentationMode::SafetyBlank,
            "stale world proof must fail closed");
    }

    {
        p::Coordinator coordinator({ 2, true });
        auto menu = running(70);
        menu.runtime.ui = p::UiClassification::Blocking;
        addUi(menu, key(600));
        coordinator.advance(menu);
        menu.ui.fresh = false;
        expect(coordinator.advance(menu).mode == p::PresentationMode::UiQuad,
            "same-state stale UI may be held under explicit policy");
        menu.runtime.sample = 71;
        expect(coordinator.advance(menu).mode == p::PresentationMode::SafetyBlank,
            "a retained UI frame must not cross runtime-sample identity");
    }

    {
        p::Coordinator coordinator({ 2, false });
        auto menu = running(80);
        menu.runtime.ui = p::UiClassification::Blocking;
        addUi(menu, key(700));
        coordinator.advance(menu);
        menu.ui.fresh = false;
        expect(coordinator.advance(menu).mode == p::PresentationMode::SafetyBlank,
            "policy may require immediate blank on missing blocking-UI evidence");
    }

    {
        p::Coordinator coordinator;
        auto unknown = running();
        unknown.runtime.fresh = false;
        expect(coordinator.advance(unknown).mode == p::PresentationMode::SafetyBlank,
            "stale runtime evidence must fail closed");
    }

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
