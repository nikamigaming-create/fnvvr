#include "../host/fnvxr_openxr_spatial_adapter.h"
#include "../kernel/config_validation.h"

#include <cmath>
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

bool close(float left, float right) noexcept
{
    return std::fabs(left - right) < 0.0001F;
}

XrPosef pose(float x, float y, float z) noexcept
{
    XrPosef value {};
    value.orientation.w = 1.0F;
    value.position = { x, y, z };
    return value;
}
}

int main()
{
    using namespace fnvxr;

    const kernel::ConfigValidationResult validated =
        kernel::validateRuntimeConfig(kernel::RuntimeConfig {});
    if (!validated || !validated.config)
        return fail("default spatial configuration did not validate");
    const kernel::RuntimeConfig& config = validated.config->get();

    const host::spatial::BodyRig rig = host::spatial::solveBodyRig(
        pose(0.0F, 1.7F, 0.0F),
        true,
        pose(-0.35F, 1.25F, -0.25F),
        true,
        pose(0.35F, 1.25F, -0.25F),
        true,
        config.bodyRig);
    if (!rig.valid || !rig.left.valid || !rig.right.valid)
        return fail("tracked OpenXR poses did not produce a complete rig");
    if (!close(rig.left.shoulder.position.x, -0.19F)
        || !close(rig.right.shoulder.position.x, 0.19F)
        || !close(rig.left.shoulder.position.y, 1.46F)
        || !close(rig.right.shoulder.position.z, 0.08F))
        return fail("typed shoulder anchor configuration was not applied");
    if (!std::isfinite(rig.left.forearmLength)
        || rig.left.forearmLength <= 0.0F)
        return fail("adapter did not produce a usable authored-mesh forearm");

    // A 90-degree wrist rotation rotates the attachment offset too. Both
    // authored hands and the forearm endpoint consume this exact socket.
    XrPosef grip = pose(0.35F, 1.25F, -0.25F);
    grip.orientation = { 0.0F, 0.0F, 0.70710678F, 0.70710678F };
    const XrPosef socket = host::spatial::handAttachmentPose(
        grip, { -0.040F, 0.026F, 0.0F });
    if (!close(socket.position.x, 0.324F) || !close(socket.position.y, 1.21F)
        || !close(socket.position.z, -0.25F))
        return fail("hand socket did not rotate its local offset with the grip");
    const auto attached = host::spatial::solveBodyRig(
        pose(0.0F, 1.7F, 0.0F), true,
        pose(-0.35F, 1.25F, -0.25F), true, socket, true, config.bodyRig);
    if (!attached.right.valid
        || !close(attached.right.wrist.position.x, socket.position.x)
        || !close(attached.right.wrist.position.y, socket.position.y)
        || !close(attached.right.wrist.position.z, socket.position.z))
        return fail("forearm endpoint detached from the rotated hand socket");

    XrPosef pitchedHead = pose(0.0F, 1.7F, 0.0F);
    pitchedHead.orientation = { 0.258819F, 0.0F, 0.0F, 0.965926F };
    const host::spatial::BodyRig pitchedRig = host::spatial::solveBodyRig(
        pitchedHead,
        true,
        pose(-0.35F, 1.25F, -0.25F),
        true,
        pose(0.35F, 1.25F, -0.25F),
        true,
        config.bodyRig);
    if (!pitchedRig.valid
        || !close(pitchedRig.left.shoulder.position.y,
            rig.left.shoulder.position.y)
        || !close(pitchedRig.left.elbow.position.x, rig.left.elbow.position.x)
        || !close(pitchedRig.left.elbow.position.y, rig.left.elbow.position.y)
        || !close(pitchedRig.left.elbow.position.z, rig.left.elbow.position.z))
        return fail("head pitch must not twist the gravity-aligned torso/elbow plane");

    const host::spatial::BodyRig untracked = host::spatial::solveBodyRig(
        pose(0.0F, 1.7F, 0.0F),
        false,
        pose(-0.35F, 1.25F, -0.25F),
        true,
        pose(0.35F, 1.25F, -0.25F),
        true,
        config.bodyRig);
    if (untracked.valid || untracked.left.valid || untracked.right.valid)
        return fail("an untracked head must fail the complete rig closed");

    XrPosef calibration = pose(0.01F, 0.02F, -0.03F);
    const auto wrist = host::spatial::placeWristPlane(
        config.wristUi,
        pose(1.0F, 2.0F, 3.0F),
        true,
        1.25F,
        &calibration);
    if (!wrist || !wrist->calibrated
        || !close(wrist->pose.position.x, 1.01F)
        || !close(wrist->pose.position.y, 2.02F)
        || !close(wrist->pose.position.z, 2.97F)
        || !close(wrist->widthMeters, config.wristUi.widthMeters * 1.25F)
        || !close(wrist->heightMeters, config.wristUi.heightMeters * 1.25F))
        return fail("grip-local wrist calibration was not composed exactly");

    calibration.position.x = std::numeric_limits<float>::quiet_NaN();
    if (host::spatial::placeWristPlane(
            config.wristUi,
            pose(1.0F, 2.0F, 3.0F),
            true,
            1.0F,
            &calibration))
        return fail("invalid wrist calibration must fail closed");
    if (host::spatial::placeWristPlane(
            config.wristUi,
            pose(1.0F, 2.0F, 3.0F),
            false,
            1.0F))
        return fail("untracked wrist placement must fail closed");

    return EXIT_SUCCESS;
}
