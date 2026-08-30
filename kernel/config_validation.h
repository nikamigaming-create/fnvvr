#pragma once

#include "runtime_config.h"

#include <optional>

namespace fnvxr::kernel
{
enum class ConfigError
{
    None,
    UnsupportedRefreshRate,
    InvalidFramesInFlight,
    InvalidPoseHistoryCapacity,
    InvalidPoseAge,
    GpuEyeTransportRequired,
    InvalidRenderScale,
    InvalidClipPlanes,
    InvalidMenuSurface,
    InvalidBodyDimensions,
    InvalidArmReach,
    InvalidWristSurface,
    InvalidWristActivationAngle,
    InvalidWristHysteresis,
};

class ValidatedRuntimeConfig final
{
public:
    const RuntimeConfig& get() const noexcept;
    float frameBudgetMilliseconds() const noexcept;
    float wristHysteresisMeters() const noexcept;

private:
    friend struct ConfigValidationResult;
    explicit ValidatedRuntimeConfig(const RuntimeConfig& value) noexcept;

    RuntimeConfig value_;
    float frameBudgetMilliseconds_ = 0.0F;
    float wristHysteresisMeters_ = 0.0F;
};

struct ConfigValidationResult
{
    ConfigError error = ConfigError::None;
    std::optional<ValidatedRuntimeConfig> config;

    explicit operator bool() const noexcept;

private:
    friend ConfigValidationResult validateRuntimeConfig(const RuntimeConfig&) noexcept;
    ConfigValidationResult(ConfigError failure, const RuntimeConfig& value) noexcept;
};

ConfigValidationResult validateRuntimeConfig(const RuntimeConfig& candidate) noexcept;
const char* configErrorName(ConfigError error) noexcept;
}
