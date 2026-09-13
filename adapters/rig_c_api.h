#pragma once

#include <stdint.h>

#if defined(_WIN32)
# if defined(FNVXR_RIG_BUILD)
#  define FNVXR_RIG_API __declspec(dllexport)
# else
#  define FNVXR_RIG_API __declspec(dllimport)
# endif
#else
# define FNVXR_RIG_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ABI v1: meters, right handed, +Y up, -Z forward, quaternion XYZW.
// Engine world origins, asset units and anatomical bone axes belong to the
// adapter. No engine pointer, graphics handle, STL object or allocator crosses.
typedef struct FnvxrRigPose {
    float x, y, z;
    float qx, qy, qz, qw;
} FnvxrRigPose;

typedef struct FnvxrRigArm {
    FnvxrRigPose shoulder, elbow, wrist;
    uint32_t reachClamped;
} FnvxrRigArm;

FNVXR_RIG_API uint32_t fnvxr_rig_abi_version(void);
// Return 1 on success, 0 for missing/invalid input. Outputs are cleared on
// failure. Parent and local may alias the output for in-place composition.
FNVXR_RIG_API int32_t fnvxr_rig_compose_v1(
    const FnvxrRigPose* parent, const FnvxrRigPose* local, FnvxrRigPose* output);
// Source limb lengths are physical measurements, not universal rig constants.
// This returns geometric targets. Each engine applies its own rest-bone axes,
// skin weights and twist helpers after its native animation publication.
// Reach clamping stabilizes the elbow; the wrist retains the physical endpoint.
// Engines with collision/reach constraints pass their resolved wrist target.
FNVXR_RIG_API int32_t fnvxr_rig_solve_arm_v1(uint32_t left,
    float upperArmMeters, float forearmMeters,
    const FnvxrRigPose* shoulder, const FnvxrRigPose* wrist, FnvxrRigArm* output);

#ifdef __cplusplus
}
#endif
