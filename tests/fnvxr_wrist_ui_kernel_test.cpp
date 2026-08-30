#include "../kernel/wrist/activation.h"
#include "../kernel/wrist/placement.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>

namespace w = fnvxr::kernel::wrist;
using fnvxr::kernel::Pose;
using fnvxr::kernel::WristUiConfig;

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

bool close(float left, float right)
{
    return std::fabs(left - right) < 0.00001F;
}

bool samePose(const Pose& left, const Pose& right)
{
    return close(left.position.x, right.position.x)
        && close(left.position.y, right.position.y)
        && close(left.position.z, right.position.z)
        && close(left.orientation.x, right.orientation.x)
        && close(left.orientation.y, right.orientation.y)
        && close(left.orientation.z, right.orientation.z)
        && close(left.orientation.w, right.orientation.w);
}

w::PoseSnapshot trackedAt(float x, float y, float z)
{
    return w::PoseSnapshot(Pose { { x, y, z }, {} }, true);
}
}

int main()
{
    const WristUiConfig config {};

    {
        constexpr float HalfSqrtTwo = 0.70710678118F;
        const Pose gripPose {
            { 1.0F, 2.0F, 3.0F },
            { 0.0F, 0.0F, HalfSqrtTwo, HalfSqrtTwo },
        };
        const Pose calibratedLocal {
            { 0.1F, 0.0F, 0.0F },
            { 0.0F, 0.0F, 0.0F, 1.0F },
        };
        const auto placed = w::placeWristSurface(
            config,
            w::PoseSnapshot(gripPose, true),
            std::optional<w::LocalScreenTransform> {
                w::LocalScreenTransform(calibratedLocal) });
        expect(placed.has_value() && placed->calibrated,
            "valid calibration was not selected");
        expect(placed && close(placed->pose.position.x, 1.0F)
                && close(placed->pose.position.y, 2.1F)
                && close(placed->pose.position.z, 3.0F),
            "grip-local calibration was not composed into world space");
        expect(placed && close(placed->pose.orientation.z, HalfSqrtTwo)
                && close(placed->pose.orientation.w, HalfSqrtTwo),
            "screen orientation did not inherit the tracked grip rotation");
    }

    {
        const auto placed = w::placeWristSurface(
            config, trackedAt(0.5F, 1.0F, -0.25F));
        const Pose fallback = w::fallbackGripToScreenPose();
        expect(placed.has_value() && !placed->calibrated,
            "missing calibration did not use the documented fallback");
        expect(placed && close(placed->pose.position.x, 0.5F)
                && close(placed->pose.position.y,
                    1.0F + fallback.position.y)
                && close(placed->pose.position.z, -0.25F),
            "fallback grip-local transform was placed incorrectly");
    }

    {
        const auto placed = w::placeWristSurface(
            config, trackedAt(0.0F, 0.0F, 0.0F), std::nullopt, 1.5F);
        expect(placed && close(placed->widthMeters, config.widthMeters * 1.5F)
                && close(placed->heightMeters, config.heightMeters * 1.5F),
            "surface scale did not apply uniformly to configured dimensions");
        expect(placed && close(
                placed->widthMeters / placed->heightMeters,
                config.widthMeters / config.heightMeters),
            "surface scaling changed the configured aspect ratio");
    }

    {
        w::WristActivation activation;
        const w::PoseSnapshot head = trackedAt(0.0F, 0.0F, 0.0F);
        const w::PoseSnapshot aim = trackedAt(0.0F, 0.0F, 0.0F);

        auto decision = activation.advance(
            config, head, aim, trackedAt(0.0F, 0.0F, -0.30F));
        expect(decision.active
                && decision.reason == w::ActivationReason::Activated,
            "near aimed wrist did not activate");

        decision = activation.advance(
            config, head, aim, trackedAt(0.0F, 0.0F, -0.40F));
        expect(decision.active
                && decision.reason == w::ActivationReason::RemainedActive,
            "active wrist did not remain active inside the hysteresis band");

        activation.reset();
        decision = activation.advance(
            config, head, aim, trackedAt(0.0F, 0.0F, -0.40F));
        expect(!decision.active
                && decision.reason == w::ActivationReason::OutsideDistance,
            "inactive wrist activated inside only the deactivation threshold");

        const auto reactivated = activation.advance(config, head, aim,
            trackedAt(0.0F, 0.0F, -0.30F));
        expect(reactivated.active,
            "deactivation fixture did not reactivate the wrist");
        decision = activation.advance(
            config, head, aim, trackedAt(0.0F, 0.0F, -0.46F));
        expect(!decision.active,
            "active wrist remained active beyond the deactivation threshold");
    }

    {
        w::WristActivation activation;
        const auto decision = activation.advance(
            config,
            trackedAt(0.0F, 0.0F, 0.0F),
            trackedAt(0.0F, 0.0F, 0.0F),
            trackedAt(0.30F, 0.0F, -0.10F));
        expect(!decision.active
                && decision.reason == w::ActivationReason::AimOutsideAngle,
            "right aim outside the activation cone was accepted");
    }

    {
        const w::PoseSnapshot untracked(Pose {}, false);
        expect(!w::placeWristSurface(config, untracked),
            "untracked left grip produced a wrist surface");

        w::WristActivation activation;
        auto decision = activation.advance(
            config, untracked, trackedAt(0.0F, 0.0F, 0.0F),
            trackedAt(0.0F, 0.0F, -0.2F));
        expect(!decision.active
                && decision.reason == w::ActivationReason::TrackingUnavailable,
            "missing head tracking did not fail activation closed");

        Pose invalid {};
        invalid.position.x = std::numeric_limits<float>::quiet_NaN();
        expect(!w::placeWristSurface(
                config, w::PoseSnapshot(invalid, true)),
            "non-finite grip pose produced a wrist surface");

        Pose invalidCalibration {};
        invalidCalibration.orientation.w =
            std::numeric_limits<float>::quiet_NaN();
        expect(!w::placeWristSurface(
                config,
                trackedAt(0.0F, 0.0F, 0.0F),
                std::optional<w::LocalScreenTransform> {
                    w::LocalScreenTransform(invalidCalibration) }),
            "non-finite calibrated transform produced a wrist surface");

        WristUiConfig invalidConfig = config;
        invalidConfig.widthMeters = std::numeric_limits<float>::infinity();
        expect(!w::placeWristSurface(
                invalidConfig, trackedAt(0.0F, 0.0F, 0.0F)),
            "non-finite dimensions produced a wrist surface");
        expect(!w::placeWristSurface(config,
                trackedAt(0.0F, 0.0F, 0.0F), std::nullopt, 0.0F),
            "non-positive scale produced a wrist surface");

        decision = activation.advance(
            config,
            w::PoseSnapshot(invalid, true),
            trackedAt(0.0F, 0.0F, 0.0F),
            trackedAt(0.0F, 0.0F, -0.2F));
        expect(!decision.active
                && decision.reason == w::ActivationReason::InvalidInput,
            "non-finite activation pose did not fail closed");
    }

    {
        const auto baseline = w::placeWristSurface(
            config, trackedAt(0.25F, 1.1F, -0.4F), std::nullopt, 1.25F);
        expect(baseline.has_value(),
            "determinism fixture did not produce its baseline");
        for (int iteration = 0; iteration < 100; ++iteration)
        {
            const auto repeated = w::placeWristSurface(
                config, trackedAt(0.25F, 1.1F, -0.4F),
                std::nullopt, 1.25F);
            expect(repeated && baseline
                    && samePose(repeated->pose, baseline->pose)
                    && close(repeated->widthMeters, baseline->widthMeters)
                    && close(repeated->heightMeters, baseline->heightMeters),
                "identical placement inputs were not deterministic");
        }
    }

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
