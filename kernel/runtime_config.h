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
    // Game and compositor cadence are independent. Keep the original source
    // pose across the observed 33-56 ms producer/consumer and capture delay.
    float maximumPoseAgeMilliseconds = 75.0F;
    float maximumCpuPoseAgeMilliseconds = 75.0F;
    EyeTransport eyeTransport = EyeTransport::GpuColorV5;
};

struct PresentationConfig
{
    float renderScale = 1.0F;
    float nearClipMeters = 0.05F;
    float farClipMeters = 1000.0F;
    float menuWidthMeters = 1.35F;
    // Used until an accepted UI texture supplies its authoritative aspect.
    float menuHeightMeters = 0.759375F;
    float menuDistanceMeters = 0.90F;
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
