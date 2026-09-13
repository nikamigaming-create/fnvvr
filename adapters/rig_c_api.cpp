#include "rig_c_api.h"
#include "../kernel/rig/first_person_rig.h"
#include "../kernel/wrist/geometry.h"

#include <cmath>

namespace
{
fnvxr::kernel::Pose toKernel(const FnvxrRigPose& value) noexcept
{
    return { { value.x, value.y, value.z },
        { value.qx, value.qy, value.qz, value.qw } };
}
FnvxrRigPose fromKernel(const fnvxr::kernel::Pose& value) noexcept
{
    return { value.position.x, value.position.y, value.position.z,
        value.orientation.x, value.orientation.y, value.orientation.z, value.orientation.w };
}
}

uint32_t fnvxr_rig_abi_version(void) { return 1; }

int32_t fnvxr_rig_compose_v1(const FnvxrRigPose* parent,
    const FnvxrRigPose* local, FnvxrRigPose* output)
{
    if (!output) return 0;
    const auto p = parent ? toKernel(*parent) : fnvxr::kernel::Pose{};
    const auto l = local ? toKernel(*local) : fnvxr::kernel::Pose{};
    *output = {};
    if (!parent || !local || !fnvxr::kernel::wrist::finitePose(p)
        || !fnvxr::kernel::wrist::finitePose(l)) return 0;
    const auto result = fnvxr::kernel::wrist::compose(p, l);
    if (!fnvxr::kernel::wrist::finitePose(result)) return 0;
    *output = fromKernel(result);
    return 1;
}

int32_t fnvxr_rig_solve_arm_v1(uint32_t left, float upperArmMeters,
    float forearmMeters, const FnvxrRigPose* shoulder,
    const FnvxrRigPose* wrist, FnvxrRigArm* output)
{
    if (!output) return 0;
    *output = {};
    if (!shoulder || !wrist || left > 1 || !std::isfinite(upperArmMeters)
        || !std::isfinite(forearmMeters) || upperArmMeters <= 0 || forearmMeters <= 0)
        return 0;
    fnvxr::kernel::BodyRigConfig config;
    config.upperArmLengthMeters = upperArmMeters;
    config.forearmLengthMeters = forearmMeters;
    const auto result = fnvxr::kernel::rig::solveArm(config,
        left ? fnvxr::kernel::rig::Handedness::Left : fnvxr::kernel::rig::Handedness::Right,
        toKernel(*shoulder), toKernel(*wrist));
    if (!result) return 0;
    *output = { fromKernel(result.shoulder), fromKernel(result.elbow),
        fromKernel(result.wrist), result.reachClamped ? 1u : 0u };
    return 1;
}
