#pragma once

#include <cstdint>

namespace fnvxr::engine
{
// Supported retail target. FalloutNV.exe is Steam-bound, so these virtual
// addresses cannot be validated from its encrypted on-disk .text bytes. Every
// address used by production must be signature-checked after the loader has
// produced executable code in the running process.
inline constexpr std::uintptr_t SupportedImageBase = 0x00400000u;
inline constexpr std::uintptr_t WholeFrameRenderAddress = 0x008706B0u;
inline constexpr std::uintptr_t WorldRenderAddress = 0x00873200u;
inline constexpr std::uintptr_t FirstPersonRenderAddress = 0x00875110u;

enum class StereoCapabilityFailure : std::uint32_t
{
    None = 0,
    UnsupportedExecutable = 1,
    RuntimeSignaturesUnverified = 2,
    WholeFrameBoundaryUnverified = 3,
    WorldRenderBoundaryUnverified = 4,
    SceneGraphLayoutUnverified = 5,
    WorldCameraUnverified = 6,
    WorldCullerUnverified = 7,
    SimulationScheduleUnverified = 8,
    ConservativeVisibilityUnverified = 9,
    VisibilityReuseUnverified = 10,
    LeftEyeRenderUnverified = 11,
    RightEyeRenderUnverified = 12,
    IsolatedColorDepthUnverified = 13,
    CompleteRenderGraphUnverified = 14,
    StateRestorationUnverified = 15,
};

// This is evidence, not configuration. A field may become true only from a
// deterministic probe or test that directly establishes the named property.
// No environment variable or optimistic fallback is allowed to set it.
struct StereoCapabilityEvidence
{
    bool supportedExecutable = false;
    bool loadedRuntimeSignatures = false;
    bool wholeFrameBoundary = false;
    bool worldRenderBoundary = false;
    bool sceneGraphLayout = false;
    bool worldCamera = false;
    bool worldCuller = false;
    bool simulationOnce = false;
    bool conservativeVisibilityOnce = false;
    bool sameVisibilityForBothEyes = false;
    bool leftEyeRender = false;
    bool rightEyeRender = false;
    bool isolatedColorAndDepth = false;
    bool completeRenderGraph = false;
    bool authoritativeStateRestored = false;
};

struct StereoCapabilityAssessment
{
    bool productionViable = false;
    StereoCapabilityFailure failure = StereoCapabilityFailure::UnsupportedExecutable;
};

inline StereoCapabilityAssessment fail(StereoCapabilityFailure failure)
{
    return { false, failure };
}

inline StereoCapabilityAssessment assessStereoCapability(
    const StereoCapabilityEvidence& evidence)
{
    if (!evidence.supportedExecutable)
        return fail(StereoCapabilityFailure::UnsupportedExecutable);
    if (!evidence.loadedRuntimeSignatures)
        return fail(StereoCapabilityFailure::RuntimeSignaturesUnverified);
    if (!evidence.wholeFrameBoundary)
        return fail(StereoCapabilityFailure::WholeFrameBoundaryUnverified);
    if (!evidence.worldRenderBoundary)
        return fail(StereoCapabilityFailure::WorldRenderBoundaryUnverified);
    if (!evidence.sceneGraphLayout)
        return fail(StereoCapabilityFailure::SceneGraphLayoutUnverified);
    if (!evidence.worldCamera)
        return fail(StereoCapabilityFailure::WorldCameraUnverified);
    if (!evidence.worldCuller)
        return fail(StereoCapabilityFailure::WorldCullerUnverified);
    if (!evidence.simulationOnce)
        return fail(StereoCapabilityFailure::SimulationScheduleUnverified);
    if (!evidence.conservativeVisibilityOnce)
        return fail(StereoCapabilityFailure::ConservativeVisibilityUnverified);
    if (!evidence.sameVisibilityForBothEyes)
        return fail(StereoCapabilityFailure::VisibilityReuseUnverified);
    if (!evidence.leftEyeRender)
        return fail(StereoCapabilityFailure::LeftEyeRenderUnverified);
    if (!evidence.rightEyeRender)
        return fail(StereoCapabilityFailure::RightEyeRenderUnverified);
    if (!evidence.isolatedColorAndDepth)
        return fail(StereoCapabilityFailure::IsolatedColorDepthUnverified);
    if (!evidence.completeRenderGraph)
        return fail(StereoCapabilityFailure::CompleteRenderGraphUnverified);
    if (!evidence.authoritativeStateRestored)
        return fail(StereoCapabilityFailure::StateRestorationUnverified);
    return { true, StereoCapabilityFailure::None };
}
}
