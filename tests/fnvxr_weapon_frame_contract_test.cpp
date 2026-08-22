#include "fnvxr_weapon_frame_contract.h"

#include <cstdint>
#include <limits>

namespace
{
int fail()
{
    return 1;
}
}

int main()
{
    using fnvxr::weapon_frame::Failure;
    constexpr std::uint32_t committed = 1u;
    constexpr std::uint32_t required = 0x0fu;
    if (fnvxr::weapon_frame::validateIdentity(
            committed, committed, required, required,
            42u, 900u, 42u, 900u, 0x1000u, 0x2000u) != Failure::None)
        return fail();
    if (fnvxr::weapon_frame::validateIdentity(
            committed, committed, required, required,
            42u, 900u, 44u, 900u, 0x1000u, 0x2000u) != Failure::PoseMismatch)
        return fail();
    if (fnvxr::weapon_frame::validateIdentity(
            committed, committed, required & ~4u, required,
            42u, 900u, 42u, 900u, 0x1000u, 0x2000u) != Failure::Incomplete)
        return fail();
    if (fnvxr::weapon_frame::validateIdentity(
            committed, committed, required, required,
            42u, 900u, 42u, 900u, 0u, 0x2000u) != Failure::MissingNodes)
        return fail();

    const float committedTransform[3] { 1.0f, 2.0f, 3.0f };
    const float stableTransform[3] { 1.01f, 1.99f, 3.0f };
    const float overwrittenTransform[3] { 1.0f, 2.0f, 3.03f };
    const float invalidTransform[3] {
        1.0f, 2.0f, std::numeric_limits<float>::quiet_NaN() };
    if (!fnvxr::weapon_frame::transformMatches(
            stableTransform, committedTransform, 3, 0.02f))
        return fail();
    if (fnvxr::weapon_frame::transformMatches(
            overwrittenTransform, committedTransform, 3, 0.02f))
        return fail();
    if (fnvxr::weapon_frame::transformMatches(
            invalidTransform, committedTransform, 3, 0.02f))
        return fail();

    const float committedWeaponRotation[9] {
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
    };
    const float stableWeaponPosition[3] { 1.01f, 1.99f, 3.0f };
    const float stableWeaponRotation[9] {
        1.0f, 0.001f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
    };
    if (!fnvxr::weapon_frame::committedPoseOwnsLiveRigTransforms(
            stableTransform,
            committedTransform,
            stableWeaponPosition,
            committedTransform,
            stableWeaponRotation,
            committedWeaponRotation,
            0.02f,
            0.02f,
            0.01f))
    {
        return fail();
    }
    if (fnvxr::weapon_frame::committedPoseOwnsLiveRigTransforms(
            overwrittenTransform,
            committedTransform,
            stableWeaponPosition,
            committedTransform,
            stableWeaponRotation,
            committedWeaponRotation,
            0.02f,
            0.02f,
            0.01f))
    {
        return fail();
    }
    if (fnvxr::weapon_frame::committedPoseOwnsLiveRigTransforms(
            stableTransform,
            committedTransform,
            overwrittenTransform,
            committedTransform,
            stableWeaponRotation,
            committedWeaponRotation,
            0.02f,
            0.02f,
            0.01f))
    {
        return fail();
    }
    if (!fnvxr::weapon_frame::duplicatePoseSolveCanBeSkipped(42, 42, false))
        return fail();
    if (fnvxr::weapon_frame::duplicatePoseSolveCanBeSkipped(42, 42, true))
        return fail();
    if (fnvxr::weapon_frame::duplicatePoseSolveCanBeSkipped(43, 42, false))
        return fail();
    if (fnvxr::weapon_frame::weaponBindingMustBeRecalibrated(
            false,
            0x000e3778u,
            0x000e3778u,
            0x1000u,
            0x1000u,
            0x2000u,
            0x2000u,
            0x3000u,
            0x3000u))
    {
        return fail();
    }
    if (!fnvxr::weapon_frame::weaponBindingMustBeRecalibrated(
            false,
            0x000e3778u,
            0x001735d4u,
            0x1000u,
            0x1000u,
            0x2000u,
            0x4000u,
            0x3000u,
            0x5000u))
    {
        return fail();
    }
    if (!fnvxr::weapon_frame::weaponBindingMustBeRecalibrated(
            true,
            0x000e3778u,
            0x000e3778u,
            0x1000u,
            0x1000u,
            0x2000u,
            0x2000u,
            0x3000u,
            0x3000u))
    {
        return fail();
    }
    if (!fnvxr::weapon_frame::weaponBindingReady(
            true, true, true, 0x001735d4u, 0x001735d4u))
    {
        return fail();
    }
    if (fnvxr::weapon_frame::weaponBindingReady(
            true, true, true, 0x001735d4u, 0x000e3778u))
    {
        return fail();
    }
    if (fnvxr::weapon_frame::weaponBindingReady(
            true, true, false, 0x001735d4u, 0x001735d4u))
    {
        return fail();
    }
    if (!fnvxr::weapon_frame::preserveCommittedPose(
            committed, committed, 42u, 900u, 42u, 900u, false))
        return fail();
    if (fnvxr::weapon_frame::preserveCommittedPose(
            committed, committed, 42u, 900u, 44u, 901u, false))
        return fail();
    if (fnvxr::weapon_frame::preserveCommittedPose(
            committed, committed, 42u, 900u, 42u, 900u, true))
        return fail();
    return 0;
}
