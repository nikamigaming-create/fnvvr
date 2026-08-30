#include "config_validation.h"

#include <cmath>

namespace fnvxr::kernel
{
namespace
{
bool finiteInRange(float value, float minimum, float maximum) noexcept
{
    return std::isfinite(value) && value >= minimum && value <= maximum;
}

bool supportedRefreshRate(std::uint32_t refreshHz) noexcept
{
    switch (refreshHz)
    {
    case 72:
    case 80:
    case 90:
    case 120:
        return true;
    default:
        return false;
    }
}
}

ValidatedRuntimeConfig::ValidatedRuntimeConfig(const RuntimeConfig& value) noexcept
    : value_(value),
      frameBudgetMilliseconds_(1000.0F / static_cast<float>(value.performance.targetRefreshHz)),
      wristHysteresisMeters_(
          value.wristUi.deactivationDistanceMeters - value.wristUi.activationDistanceMeters)
{
}

const RuntimeConfig& ValidatedRuntimeConfig::get() const noexcept
{
    return value_;
}

float ValidatedRuntimeConfig::frameBudgetMilliseconds() const noexcept
{
    return frameBudgetMilliseconds_;
}

float ValidatedRuntimeConfig::wristHysteresisMeters() const noexcept
{
    return wristHysteresisMeters_;
}

ConfigValidationResult::ConfigValidationResult(
    ConfigError failure,
    const RuntimeConfig& value) noexcept
    : error(failure),
      config(failure == ConfigError::None
          ? std::optional<ValidatedRuntimeConfig> { ValidatedRuntimeConfig(value) }
          : std::nullopt)
{
}

ConfigValidationResult::operator bool() const noexcept
{
    return error == ConfigError::None;
}

ConfigValidationResult validateRuntimeConfig(const RuntimeConfig& candidate) noexcept
{
    const PerformanceConfig& performance = candidate.performance;
    if (!supportedRefreshRate(performance.targetRefreshHz))
        return { ConfigError::UnsupportedRefreshRate, candidate };
    if (performance.maximumFramesInFlight != 1)
        return { ConfigError::InvalidFramesInFlight, candidate };
    // The live host currently owns one compile-time 128-slot O(1) joiner.
    // Reject pretend configurability until a second capacity is actually
    // implemented and tested end-to-end.
    if (performance.poseHistoryCapacity != 128)
        return { ConfigError::InvalidPoseHistoryCapacity, candidate };
    const float frameBudget = 1000.0F / static_cast<float>(performance.targetRefreshHz);
    if (!finiteInRange(performance.maximumPoseAgeMilliseconds, 0.1F, frameBudget))
        return { ConfigError::InvalidPoseAge, candidate };
    if (!performance.requireGpuEyeTransport)
        return { ConfigError::GpuEyeTransportRequired, candidate };

    const PresentationConfig& presentation = candidate.presentation;
    if (!finiteInRange(presentation.renderScale, 0.5F, 2.0F))
        return { ConfigError::InvalidRenderScale, candidate };
    if (!finiteInRange(presentation.nearClipMeters, 0.01F, 1.0F) ||
        !finiteInRange(presentation.farClipMeters, 10.0F, 10000.0F) ||
        presentation.nearClipMeters >= presentation.farClipMeters)
        return { ConfigError::InvalidClipPlanes, candidate };
    if (!finiteInRange(presentation.menuWidthMeters, 0.25F, 2.0F) ||
        !finiteInRange(presentation.menuDistanceMeters, 0.3F, 3.0F) ||
        presentation.menuWidthMeters >= 2.0F * presentation.menuDistanceMeters)
        return { ConfigError::InvalidMenuSurface, candidate };

    const BodyRigConfig& body = candidate.bodyRig;
    if (!finiteInRange(body.standingHeightMeters, 1.2F, 2.3F) ||
        !finiteInRange(body.shoulderWidthMeters, 0.25F, 0.7F) ||
        !finiteInRange(body.upperArmLengthMeters, 0.15F, 0.5F) ||
        !finiteInRange(body.forearmLengthMeters, 0.15F, 0.5F) ||
        !finiteInRange(body.handLengthMeters, 0.1F, 0.3F))
        return { ConfigError::InvalidBodyDimensions, candidate };
    if (body.upperArmLengthMeters + body.forearmLengthMeters + body.handLengthMeters >=
        body.standingHeightMeters * 0.65F)
        return { ConfigError::InvalidArmReach, candidate };

    const WristUiConfig& wrist = candidate.wristUi;
    if (!finiteInRange(wrist.widthMeters, 0.05F, 0.25F) ||
        !finiteInRange(wrist.heightMeters, 0.04F, 0.2F) ||
        wrist.heightMeters > wrist.widthMeters)
        return { ConfigError::InvalidWristSurface, candidate };
    if (!finiteInRange(wrist.activationAngleDegrees, 5.0F, 80.0F))
        return { ConfigError::InvalidWristActivationAngle, candidate };
    if (!finiteInRange(wrist.activationDistanceMeters, 0.1F, 0.8F) ||
        !finiteInRange(wrist.deactivationDistanceMeters, 0.15F, 1.0F) ||
        wrist.deactivationDistanceMeters - wrist.activationDistanceMeters < 0.05F)
        return { ConfigError::InvalidWristHysteresis, candidate };

    return { ConfigError::None, candidate };
}

const char* configErrorName(ConfigError error) noexcept
{
    switch (error)
    {
    case ConfigError::None: return "none";
    case ConfigError::UnsupportedRefreshRate: return "unsupported-refresh-rate";
    case ConfigError::InvalidFramesInFlight: return "invalid-frames-in-flight";
    case ConfigError::InvalidPoseHistoryCapacity: return "invalid-pose-history-capacity";
    case ConfigError::InvalidPoseAge: return "invalid-pose-age";
    case ConfigError::GpuEyeTransportRequired: return "gpu-eye-transport-required";
    case ConfigError::InvalidRenderScale: return "invalid-render-scale";
    case ConfigError::InvalidClipPlanes: return "invalid-clip-planes";
    case ConfigError::InvalidMenuSurface: return "invalid-menu-surface";
    case ConfigError::InvalidBodyDimensions: return "invalid-body-dimensions";
    case ConfigError::InvalidArmReach: return "invalid-arm-reach";
    case ConfigError::InvalidWristSurface: return "invalid-wrist-surface";
    case ConfigError::InvalidWristActivationAngle: return "invalid-wrist-activation-angle";
    case ConfigError::InvalidWristHysteresis: return "invalid-wrist-hysteresis";
    }
    return "unknown";
}
}
