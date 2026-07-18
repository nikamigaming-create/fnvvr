#include "fnvxr_engine_capability.h"

#include <cstdlib>
#include <iostream>

namespace
{
int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}

fnvxr::engine::StereoCapabilityEvidence completeEvidence()
{
    fnvxr::engine::StereoCapabilityEvidence value {};
    value.supportedExecutable = true;
    value.loadedRuntimeSignatures = true;
    value.wholeFrameBoundary = true;
    value.worldRenderBoundary = true;
    value.sceneGraphLayout = true;
    value.worldCamera = true;
    value.worldCuller = true;
    value.simulationOnce = true;
    value.conservativeVisibilityOnce = true;
    value.sameVisibilityForBothEyes = true;
    value.leftEyeRender = true;
    value.rightEyeRender = true;
    value.isolatedColorAndDepth = true;
    value.completeRenderGraph = true;
    value.authoritativeStateRestored = true;
    return value;
}
}

int main()
{
    using namespace fnvxr::engine;

    {
        const StereoCapabilityAssessment assessment =
            assessStereoCapability(completeEvidence());
        if (!assessment.productionViable
            || assessment.failure != StereoCapabilityFailure::None)
        {
            return fail("complete engine-level evidence must pass the viability gate");
        }
    }

    {
        StereoCapabilityEvidence evidence = completeEvidence();
        evidence.loadedRuntimeSignatures = false;
        const StereoCapabilityAssessment assessment = assessStereoCapability(evidence);
        if (assessment.productionViable
            || assessment.failure != StereoCapabilityFailure::RuntimeSignaturesUnverified)
        {
            return fail("packed on-disk bytes must not substitute for loaded runtime signatures");
        }
    }

    {
        StereoCapabilityEvidence evidence = completeEvidence();
        evidence.worldRenderBoundary = false;
        const StereoCapabilityAssessment assessment = assessStereoCapability(evidence);
        if (assessment.productionViable
            || assessment.failure != StereoCapabilityFailure::WorldRenderBoundaryUnverified)
        {
            return fail("the whole-frame hook alone must not prove engine stereo viability");
        }
    }

    {
        StereoCapabilityEvidence evidence = completeEvidence();
        evidence.conservativeVisibilityOnce = false;
        const StereoCapabilityAssessment assessment = assessStereoCapability(evidence);
        if (assessment.productionViable
            || assessment.failure != StereoCapabilityFailure::ConservativeVisibilityUnverified)
        {
            return fail("two eye renders without one conservative visible set must fail");
        }
    }

    {
        StereoCapabilityEvidence evidence = completeEvidence();
        evidence.sameVisibilityForBothEyes = false;
        const StereoCapabilityAssessment assessment = assessStereoCapability(evidence);
        if (assessment.productionViable
            || assessment.failure != StereoCapabilityFailure::VisibilityReuseUnverified)
        {
            return fail("both eyes must consume the exact same conservative visible set");
        }
    }

    {
        StereoCapabilityEvidence evidence = completeEvidence();
        evidence.isolatedColorAndDepth = false;
        const StereoCapabilityAssessment assessment = assessStereoCapability(evidence);
        if (assessment.productionViable
            || assessment.failure != StereoCapabilityFailure::IsolatedColorDepthUnverified)
        {
            return fail("color-only eye rendering must fail the engine viability gate");
        }
    }

    {
        StereoCapabilityEvidence evidence = completeEvidence();
        evidence.authoritativeStateRestored = false;
        const StereoCapabilityAssessment assessment = assessStereoCapability(evidence);
        if (assessment.productionViable
            || assessment.failure != StereoCapabilityFailure::StateRestorationUnverified)
        {
            return fail("unrestored retail state must fail even after both eyes render");
        }
    }

    std::cout << "fnvxr engine stereo capability gate PASS\n";
    return EXIT_SUCCESS;
}
