#include "../kernel/wrist/content_readiness.h"

#include <cstdlib>
#include <iostream>

namespace w = fnvxr::kernel::wrist;

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

w::ContentReadinessInput readyInput()
{
    w::ContentReadinessInput input {};
    input.menuActive = true;
    input.screenFocused = true;
    input.retained = {
        17, 100, 1'000, 41, 9, true, true, true };
    input.runtime.source = { 100, true, true, true };
    input.runtime.current = { 103, true, true, true };
    input.runtime.stable = true;
    input.currentProducerProcessId = 41;
    input.currentRendererProducerEpoch = 9;
    input.nowMilliseconds = 1'200;
    input.maximumAgeMilliseconds = 250;
    return input;
}

void expectReason(
    const w::ContentReadinessInput& input,
    w::ContentReadinessReason reason,
    const char* message)
{
    const auto decision = w::assessContentReadiness(input);
    expect(!decision.ready && decision.reason == reason, message);
    expect(decision.resourceGeneration == 0 && decision.runtimeSample == 0,
        "rejected content leaked render authority");
}
}

int main()
{
    {
        const auto decision = w::assessContentReadiness(readyInput());
        expect(decision.ready
                && decision.reason == w::ContentReadinessReason::Ready,
            "complete retained Pip-Boy content was rejected");
        expect(decision.ageMilliseconds == 200
                && decision.resourceGeneration == 17
                && decision.runtimeSample == 100,
            "ready decision lost its exact retained identity");
    }

    {
        auto input = readyInput();
        input.menuActive = false;
        expectReason(input, w::ContentReadinessReason::MenuInactive,
            "inactive menu was accepted");
        input = readyInput();
        input.screenFocused = false;
        expectReason(input, w::ContentReadinessReason::ScreenNotFocused,
            "unfocused screen was accepted");
    }

    {
        auto input = readyInput();
        input.retained.resourceAvailable = false;
        expectReason(input, w::ContentReadinessReason::ResourceUnavailable,
            "missing retained resource was accepted");
        input = readyInput();
        input.retained.resourceGeneration = 0;
        expectReason(input,
            w::ContentReadinessReason::ResourceIdentityUnavailable,
            "unidentified retained resource was accepted");
        input = readyInput();
        input.retained.pixelsProven = false;
        expectReason(input, w::ContentReadinessReason::PixelsUnproven,
            "resource without pixel proof was accepted");
    }

    {
        auto input = readyInput();
        input.retained.acceptanceRecorded = false;
        expectReason(input, w::ContentReadinessReason::AcceptanceUnavailable,
            "content without an acceptance timestamp was accepted");
        input = readyInput();
        input.retained.producerProcessId = 0;
        expectReason(input,
            w::ContentReadinessReason::ProducerIdentityUnavailable,
            "content without a producer identity was accepted");
        input = readyInput();
        input.currentRendererProducerEpoch = 10;
        expectReason(input,
            w::ContentReadinessReason::ProducerOwnershipMismatch,
            "content from a replaced renderer epoch was accepted");
        input = readyInput();
        input.currentProducerProcessId = 42;
        expectReason(input,
            w::ContentReadinessReason::ProducerOwnershipMismatch,
            "content from a replaced producer process was accepted");
        input = readyInput();
        input.nowMilliseconds = 999;
        expectReason(input, w::ContentReadinessReason::ClockRegressed,
            "future acceptance timestamp was accepted");
        input = readyInput();
        input.nowMilliseconds = 1'251;
        expectReason(input, w::ContentReadinessReason::ContentStale,
            "content beyond the maximum age was accepted");
        input = readyInput();
        input.nowMilliseconds = 1'250;
        expect(w::assessContentReadiness(input).ready,
            "content at the inclusive age boundary was rejected");
        input.retained.acceptedAtMilliseconds = 0;
        input.nowMilliseconds = 0;
        input.maximumAgeMilliseconds = 0;
        expect(w::assessContentReadiness(input).ready,
            "explicitly recorded zero-time content was treated as missing");
    }

    {
        auto input = readyInput();
        input.runtime.source.fresh = false;
        expectReason(input,
            w::ContentReadinessReason::SourceRuntimeUnavailable,
            "stale source runtime was accepted");
        input = readyInput();
        input.runtime.current.sample = 0;
        expectReason(input,
            w::ContentReadinessReason::CurrentRuntimeUnavailable,
            "missing current runtime was accepted");
        input = readyInput();
        input.retained.runtimeSample = 99;
        expectReason(input, w::ContentReadinessReason::ContentSourceMismatch,
            "content from a different runtime sample was accepted");
        input = readyInput();
        input.runtime.current.sample = 99;
        expectReason(input, w::ContentReadinessReason::RuntimeOrderInvalid,
            "runtime history moving backwards was accepted");
        input = readyInput();
        input.runtime.stable = false;
        expectReason(input,
            w::ContentReadinessReason::RuntimeLineageUnproven,
            "unproven source/current lineage was accepted");
    }

    {
        auto input = readyInput();
        input.runtime.source.pipBoyMenu = false;
        expectReason(input, w::ContentReadinessReason::SourceNotPipBoy,
            "non-Pip-Boy source content was accepted");
        input = readyInput();
        input.runtime.current.pipBoyMenu = false;
        expectReason(input, w::ContentReadinessReason::CurrentNotPipBoy,
            "content retained into a different current menu was accepted");
        input = readyInput();
        input.runtime.source.worldPresentationContinues = false;
        expectReason(input,
            w::ContentReadinessReason::SourceWorldDoesNotContinue,
            "source without world continuation was accepted");
        input = readyInput();
        input.runtime.current.worldPresentationContinues = false;
        expectReason(input,
            w::ContentReadinessReason::CurrentWorldDoesNotContinue,
            "current state without world continuation was accepted");
    }

    {
        const auto input = readyInput();
        const auto baseline = w::assessContentReadiness(input);
        for (int iteration = 0; iteration < 100; ++iteration)
        {
            const auto repeated = w::assessContentReadiness(input);
            expect(repeated.ready == baseline.ready
                    && repeated.reason == baseline.reason
                    && repeated.ageMilliseconds == baseline.ageMilliseconds
                    && repeated.resourceGeneration
                        == baseline.resourceGeneration
                    && repeated.runtimeSample == baseline.runtimeSample,
                "identical content evidence produced unstable decisions");
        }
    }

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
