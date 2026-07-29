#pragma once

#include <array>
#include <cstddef>

namespace fnvxr::assist
{
// A named, deterministic head/body test trace.  Every pose is expressed in
// OpenXR LOCAL-space meters.  The baseline comes first so a retail camera hook
// can establish its body/recenter origin before any deliberate head motion.
struct PoseStep
{
    const char* name = "";
    float yawDegrees = 0.0f;
    float pitchDegrees = 0.0f;
    float rollDegrees = 0.0f;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    bool expectsRotationResponse = false;
    bool expectsTranslationResponse = false;
    float controllerPositionX = 0.0f;
    float controllerPositionY = 0.0f;
    float controllerPositionZ = 0.0f;
    float controllerYawDegrees = 0.0f;
    float controllerPitchDegrees = 0.0f;
    float controllerRollDegrees = 0.0f;
    bool expectsControllerResponse = false;
};

inline constexpr float HeadHeightMeters = 1.65f;
inline constexpr float ExpectedYawDegrees = 20.0f;
inline constexpr float ExpectedTranslationMeters = 0.16f;
inline constexpr float ExpectedControllerTranslationMeters = 0.12f;
inline constexpr float ExpectedControllerYawDegrees = 20.0f;

inline constexpr std::array<PoseStep, 11> HeadBodyScenario {{
    { "baseline", 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false, false },
    { "yaw-right", ExpectedYawDegrees, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, true, false },
    { "yaw-left", -ExpectedYawDegrees, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, true, false },
    { "pitch-down", 0.0f, 12.0f, 0.0f, 0.0f, 0.0f, 0.0f, true, false },
    { "roll-right", 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, 0.0f, true, false },
    { "lean-right", 0.0f, 0.0f, 0.0f, ExpectedTranslationMeters, 0.0f, 0.0f, false, true },
    { "lean-left", 0.0f, 0.0f, 0.0f, -ExpectedTranslationMeters, 0.0f, 0.0f, false, true },
    { "lean-forward", 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -ExpectedTranslationMeters, false, true },
    { "controller-right", 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false, false,
        ExpectedControllerTranslationMeters, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, true },
    { "controller-aim-right", 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false, false,
        0.0f, 0.0f, 0.0f, ExpectedControllerYawDegrees, 0.0f, 0.0f, true },
    { "return-baseline", 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false, false },
}};

inline bool headBodyScenarioIsValid() noexcept
{
    bool hasBaseline = false;
    bool hasPositiveYaw = false;
    bool hasNegativeYaw = false;
    bool hasTranslation = false;
    bool hasControllerTranslation = false;
    bool hasControllerRotation = false;

    for (const PoseStep& step : HeadBodyScenario)
    {
        if (!step.name || step.name[0] == '\0')
            return false;
        if (!step.expectsRotationResponse
            && !step.expectsTranslationResponse
            && step.yawDegrees == 0.0f
            && step.pitchDegrees == 0.0f
             && step.rollDegrees == 0.0f
             && step.positionX == 0.0f
             && step.positionY == 0.0f
            && step.positionZ == 0.0f
            && step.controllerPositionX == 0.0f
            && step.controllerPositionY == 0.0f
            && step.controllerPositionZ == 0.0f
            && step.controllerYawDegrees == 0.0f
            && step.controllerPitchDegrees == 0.0f
            && step.controllerRollDegrees == 0.0f
            && !step.expectsControllerResponse)
        {
            hasBaseline = true;
        }
        hasPositiveYaw = hasPositiveYaw || step.yawDegrees >= ExpectedYawDegrees;
        hasNegativeYaw = hasNegativeYaw || step.yawDegrees <= -ExpectedYawDegrees;
        hasTranslation = hasTranslation || step.expectsTranslationResponse;
        hasControllerTranslation = hasControllerTranslation
            || step.controllerPositionX >= ExpectedControllerTranslationMeters;
        hasControllerRotation = hasControllerRotation
            || step.controllerYawDegrees >= ExpectedControllerYawDegrees;

        // Rotation and translation probes must be isolated.  That makes a
        // failed camera/body invariant attributable to one kind of motion.
        if (step.expectsRotationResponse
            && (step.positionX != 0.0f || step.positionY != 0.0f || step.positionZ != 0.0f))
        {
            return false;
        }
        if (step.expectsTranslationResponse
            && (step.yawDegrees != 0.0f || step.pitchDegrees != 0.0f || step.rollDegrees != 0.0f))
        {
            return false;
        }
        // Controller probes must have a neutral head pose.  They prove that
        // the right grip/aim source can move the visual rig independently of
        // the camera source, instead of merely repeating a head-yaw test.
        if (step.expectsControllerResponse
            && (step.expectsRotationResponse
                || step.expectsTranslationResponse
                || step.yawDegrees != 0.0f
                || step.pitchDegrees != 0.0f
                || step.rollDegrees != 0.0f
                || step.positionX != 0.0f
                || step.positionY != 0.0f
                || step.positionZ != 0.0f))
        {
            return false;
        }
    }

    return hasBaseline
        && hasPositiveYaw
        && hasNegativeYaw
        && hasTranslation
        && hasControllerTranslation
        && hasControllerRotation;
}

inline bool trackedPropScenarioIsValid() noexcept
{
    bool hasControllerResponse = false;
    for (const PoseStep& step : HeadBodyScenario)
    {
        hasControllerResponse = hasControllerResponse || step.expectsControllerResponse;
        if (step.expectsControllerResponse
            && (step.controllerPositionX == 0.0f
                && step.controllerPositionY == 0.0f
                && step.controllerPositionZ == 0.0f
                && step.controllerYawDegrees == 0.0f
                && step.controllerPitchDegrees == 0.0f
                && step.controllerRollDegrees == 0.0f))
        {
            return false;
        }
    }
    return headBodyScenarioIsValid() && hasControllerResponse;
}

inline constexpr std::size_t HeadBodyScenarioStepCount = HeadBodyScenario.size();
}
