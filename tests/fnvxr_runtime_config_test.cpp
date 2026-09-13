#include "../kernel/config_validation.h"

#include <cmath>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}

bool close(float left, float right)
{
    return std::fabs(left - right) < 0.0001F;
}
}

int main()
{
    using namespace fnvxr::kernel;

    const ConfigValidationResult defaults = validateRuntimeConfig(RuntimeConfig {});
    if (!defaults)
        return fail("production defaults must validate");
    if (!defaults.config
        || defaults.config->get().performance.eyeTransport
            != EyeTransport::GpuColorV5)
        return fail("production defaults must select the GPU-v5 eye transport");
    if (!close(defaults.config->frameBudgetMilliseconds(), 1000.0F / 90.0F))
        return fail("validated config must normalize refresh rate to a frame budget once");
    if (defaults.config->get().performance.maximumPoseAgeMilliseconds
            < defaults.config->frameBudgetMilliseconds() * 3.0F)
        return fail("rounding must not reject the third compositor cycle");
    if (defaults.config->maximumPoseAgeNanoseconds() < 75000000LL
        || defaults.config->maximumPoseAgeNanoseconds() >= 75000020LL)
        return fail("integer pose age must retain the complete 75 ms transport window");
    if (!close(defaults.config->wristHysteresisMeters(), 0.10F))
        return fail("validated config must expose normalized wrist hysteresis");

    RuntimeConfig invalid = {};
    invalid.presentation.renderScale = std::numeric_limits<float>::quiet_NaN();
    if (validateRuntimeConfig(invalid).error != ConfigError::InvalidRenderScale)
        return fail("non-finite scalar values must be rejected explicitly");

    invalid = {};
    invalid.performance.targetRefreshHz = 144;
    if (validateRuntimeConfig(invalid).error != ConfigError::UnsupportedRefreshRate)
        return fail("unsupported refresh rates must be rejected explicitly");

    invalid = {};
    invalid.performance.poseHistoryCapacity = 64;
    if (validateRuntimeConfig(invalid).error !=
        ConfigError::InvalidPoseHistoryCapacity)
        return fail("unsupported pose history capacity must fail instead of becoming a fake knob");

    invalid = {};
    invalid.performance.maximumPoseAgeMilliseconds = 101.0F;
    if (validateRuntimeConfig(invalid).error != ConfigError::InvalidPoseAge)
        return fail("transport retention must remain bounded to 100 ms");
    invalid.performance.maximumPoseAgeMilliseconds = 22.0F;
    if (!validateRuntimeConfig(invalid))
        return fail("a frame arriving on the next host cycle must remain displayable");

    for (const auto refresh : { 72u, 80u, 90u, 120u })
    {
        RuntimeConfig cadence = {};
        cadence.performance.targetRefreshHz = refresh;
        const auto validated = validateRuntimeConfig(cadence);
        if (!validated || validated.config->maximumPoseAgeNanoseconds()
                != defaults.config->maximumPoseAgeNanoseconds())
            return fail("changing headset cadence must preserve elapsed-time transport retention");
    }

    invalid = {};
    invalid.performance.maximumCpuPoseAgeMilliseconds = 251.0F;
    if (validateRuntimeConfig(invalid).error != ConfigError::InvalidCpuPoseAge)
        return fail("CPU transport pose age must retain its bounded compatibility policy");

    invalid = {};
    invalid.presentation.menuWidthMeters = 1.0F;
    invalid.presentation.menuDistanceMeters = 0.4F;
    if (validateRuntimeConfig(invalid).error != ConfigError::InvalidMenuSurface)
        return fail("menu geometry must satisfy its cross-field visibility invariant");

    invalid = {};
    invalid.presentation.menuWidthMeters = 0.5F;
    invalid.presentation.menuHeightMeters = 1.0F;
    invalid.presentation.menuDistanceMeters = 0.4F;
    if (validateRuntimeConfig(invalid).error != ConfigError::InvalidMenuSurface)
        return fail("vertical menu geometry must satisfy the visibility invariant");

    invalid = {};
    invalid.bodyRig.shoulderDropMeters = 0.0F;
    if (validateRuntimeConfig(invalid).error != ConfigError::InvalidBodyDimensions)
        return fail("shoulder anchors must be bounded configuration, not per-frame environment reads");

    invalid = {};
    invalid.bodyRig.standingHeightMeters = 1.2F;
    invalid.bodyRig.upperArmLengthMeters = 0.5F;
    invalid.bodyRig.forearmLengthMeters = 0.5F;
    invalid.bodyRig.handLengthMeters = 0.3F;
    if (validateRuntimeConfig(invalid).error != ConfigError::InvalidArmReach)
        return fail("body dimensions must satisfy their cross-field reach invariant");

    invalid = {};
    invalid.wristUi.maximumContentAgeMilliseconds = 49u;
    if (validateRuntimeConfig(invalid).error !=
        ConfigError::InvalidWristContentAge)
        return fail("wrist content freshness must have a finite policy bound");

    invalid = {};
    invalid.wristUi.activationDistanceMeters = 0.42F;
    invalid.wristUi.deactivationDistanceMeters = 0.45F;
    if (validateRuntimeConfig(invalid).error != ConfigError::InvalidWristHysteresis)
        return fail("wrist activation must retain a stable hysteresis band");

    invalid = {};
    invalid.performance.eyeTransport = static_cast<EyeTransport>(255);
    if (validateRuntimeConfig(invalid).error !=
        ConfigError::InvalidEyeTransport)
        return fail("unknown eye transports must fail validation");
    if (std::strcmp(configErrorName(ConfigError::InvalidPoseAge),
            "invalid-pose-age") != 0)
        return fail("configuration failures must expose stable diagnostic names");

    return EXIT_SUCCESS;
}
