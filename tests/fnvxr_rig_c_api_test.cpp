#include "../adapters/rig_c_api.h"
#include <cmath>
#include <limits>

int main()
{
    static_assert(sizeof(FnvxrRigPose) == 28);
    static_assert(sizeof(FnvxrRigArm) == 88);
    if (fnvxr_rig_abi_version() != 1) return 1;
    const float q = std::sqrt(0.5f);
    FnvxrRigPose parent { 1, 2, 3, 0, q, 0, q };
    FnvxrRigPose local { 0, 0, -0.2f, 0, 0, 0, 1 };
    if (!fnvxr_rig_compose_v1(&parent, &local, &parent)
        || std::fabs(parent.x - 0.8f) > 1e-5f
        || std::fabs(parent.y - 2) > 1e-5f || std::fabs(parent.z - 3) > 1e-5f) return 2;
    local.x = std::numeric_limits<float>::quiet_NaN();
    if (fnvxr_rig_compose_v1(&parent, &local, &parent) || parent.qw != 0) return 3;
    FnvxrRigPose shoulder { 0, 1.5f, 0, 0, 0, 0, 1 };
    FnvxrRigPose wrist { 0, 1.5f, -2, 0, 0, 0, 1 };
    FnvxrRigArm arm {};
    if (!fnvxr_rig_solve_arm_v1(0, .31f, .27f, &shoulder, &wrist, &arm)
        || !arm.reachClamped || std::fabs(arm.wrist.z + 2.0f) > 1e-5f
        || arm.elbow.z < -.32f || arm.elbow.z > -.30f) return 4;
    if (fnvxr_rig_solve_arm_v1(2, .31f, .27f, &shoulder, &wrist, &arm)
        || arm.reachClamped || arm.wrist.qw != 0) return 5;
    return 0;
}
