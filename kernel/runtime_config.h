#pragma once

#include <cstddef>
#include <cstdint>

namespace fnvxr::kernel
{
enum class EyeTransport : std::uint8_t
{
    GpuColorV5,
    CpuEngineCenter,
};

struct PerformanceConfig
{
    std::uint32_t targetRefreshHz = 90;
    std::uint32_t maximumFramesInFlight = 1;
    std::size_t poseHistoryCapacity = 128;
    // A completed game pair normally arrives on a later compositor cycle.
    // Keep its historical pose and bound retention to three refresh periods.
    float maximumPoseAgeMilliseconds = 33.0F;
    float maximumCpuPoseAgeMilliseconds = 75.0F;
    EyeTransport eyeTransport = EyeTransport::GpuColorV5;
};

struct PresentationConfig
{
    float renderScale = 1.0F;
    float nearClipMeters = 0.05F;
    float farClipMeters = 1000.0F;
    float menuWidthMeters = 0.90F;
    // Used until an accepted UI texture supplies its authoritative aspect.
    float menuHeightMeters = 0.50625F;
    float menuDistanceMeters = 1.20F;
};

struct BodyRigConfig
{
    float standingHeightMeters = 1.75F;
    float shoulderWidthMeters = 0.38F;
    float shoulderDropMeters = 0.24F;
    float shoulderBackMeters = 0.08F;
    float upperArmLengthMeters = 0.31F;
    float forearmLengthMeters = 0.27F;
    float handLengthMeters = 0.18F;
};

struct WristUiConfig
{
    float widthMeters = 0.0838993714F;
    float heightMeters = 0.0638814571F;
    float activationDistanceMeters = 0.35F;
    float deactivationDistanceMeters = 0.45F;
    float activationAngleDegrees = 35.0F;
    std::uint32_t maximumContentAgeMilliseconds = 250;
};

struct RuntimeConfig
{
    PerformanceConfig performance {};
    PresentationConfig presentation {};
    BodyRigConfig bodyRig {};
    WristUiConfig wristUi {};
};
}
