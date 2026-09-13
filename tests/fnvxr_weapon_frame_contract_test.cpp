#include "fnvxr_weapon_frame_contract.h"
#include "fnvxr_tracked_shot.h"
#include "fnvxr_throw_motion.h"
#include "../protocol/fnvxr_shared_state.h"

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
    for (const unsigned step : {11u, 22u})
    {
        for (const float speed : {0.4f, 4.0f})
        {
            fnvxr::throw_motion::History history;
            fnvxr::tracked_shot::Pose p {};
            p.valid = true; p.actor = 1; p.weapon = 2; p.cell = 3; p.epoch = 4; p.referenceSpace = 1;
            unsigned end = 0;
            for (unsigned t = 0; t <= 220; t += step)
            {
                end = t; p.position[0] = -66000; p.position[1] = -10000 + speed * 70 * t / 1000;
                p.position[2] = 7500 + speed * 35 * t / 1000;
                p.sampledAtMs = 1000 + t;
                history.sample(p, 1000 + t);
                history.sample(p, 1000 + t); // duplicate render of one pose
            }
            auto release = history.release(1000 + end);
            if (!release.pose.valid || std::fabs(release.velocity[0]) > .01f
                || std::fabs(release.velocity[1] - speed * 70) > .1f
                || std::fabs(release.velocity[2] - speed * 35) > .1f
                || history.release(1200 + end).pose.valid) return fail();
            p.referenceSpace++; p.sampledAtMs += step;
            history.sample(p, 1000 + end + step);
            if (history.release(p.sampledAtMs).pose.valid) return fail();
            p.position[0] += 700; p.sampledAtMs += step;
            history.sample(p, 1000 + end + step * 2);
            if (history.release(p.sampledAtMs).pose.valid) return fail();
        }
    }
    if (!fnvxr::weapon_frame::weaponBindingReady(true, true, false, 42, 42, false)
        || fnvxr::weapon_frame::weaponBindingReady(true, true, false, 42, 43, false)
        || fnvxr::weapon_frame::weaponBindingReady(true, false, false, 42, 42, false)
        || fnvxr::weapon_frame::weaponBindingReady(true, true, false, 42, 42))
        return fail();
    // The native shot uses +Y forward and positive-down pitch. Exercise
    // compass directions and tilted aim independently of the player camera.
    constexpr float halfPi = 1.57079632679f;
    const float directions[][5] {
        {0, 1, 0, 0, 0}, {1, 0, 0, 0, halfPi},
        {0, -1, 0, 0, 2 * halfPi}, {-1, 0, 0, 0, -halfPi},
        {0, 0.6f, 0.8f, -0.927295218f, 0},
        {0.6f, 0, -0.8f, 0.927295218f, halfPi},
    };
    for (const auto& direction : directions)
    {
        fnvxr::tracked_shot::Pose shot {};
        for (int i = 0; i < 3; ++i) shot.forward[i] = direction[i];
        float pitch = 0, yaw = 0;
        fnvxr::tracked_shot::angles(shot, pitch, yaw);
        if (std::fabs(pitch - direction[3]) > 0.00001f
            || std::fabs(yaw - direction[4]) > 0.00001f)
            return fail();
    }
    fnvxr::tracked_shot::Pose shot {};
    shot.valid = true;
    shot.actor = 0x1000u; shot.weapon = 0x2000u; shot.cell = 0x3000u;
    shot.epoch = 7u; shot.referenceSpace = 3u; shot.sequence = 24u;
    shot.frame = 100u; shot.sampledAtMs = 1000u; shot.forward[1] = 1.0f;
    const auto currentShot = [](const fnvxr::tracked_shot::Pose& candidate) {
        return fnvxr::tracked_shot::current(candidate,
            0x1000u, 0x2000u, 0x3000u, 7u, 3u, 106u, 1100u, true);
    };
    if (!currentShot(shot)) return fail();
    // Never fire from a prior equip/cell/recenter or disconnected/stale pose.
    for (int invalid = 0; invalid < 12; ++invalid)
    {
        auto candidate = shot;
        switch (invalid)
        {
        case 0: candidate.valid = false; break;
        case 1: candidate.actor++; break;
        case 2: candidate.weapon++; break;
        case 3: candidate.cell++; break;
        case 4: candidate.epoch++; break;
        case 5: candidate.referenceSpace++; break;
        case 6: candidate.sequence = 0; break;
        case 7: candidate.frame = 99; break;
        case 8: candidate.frame = 107; break;
        case 9: candidate.sampledAtMs = 999; break;
        case 10: candidate.sampledAtMs = 1101; break;
        case 11: candidate.position[0] = std::numeric_limits<float>::quiet_NaN(); break;
        }
        if (currentShot(candidate)) return fail();
    }
    if (fnvxr::tracked_shot::current(shot,
            0x1000u, 0x2000u, 0x3000u, 7u, 3u, 106u, 1100u, false))
        return fail();
    shot.forward[1] = 0.5f;
    if (currentShot(shot)) return fail();

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

    constexpr auto emptyFlags = fnvxr::shared::WeaponFrameFlagHandsOnly
        | fnvxr::shared::WeaponFrameFlagRightGripCurrent
        | fnvxr::shared::WeaponFrameFlagRightAimCurrent
        | fnvxr::shared::WeaponFrameFlagArmSolved;
    constexpr auto emptyRequired = fnvxr::shared::weaponFrameRequiredFlags(emptyFlags);
    if (fnvxr::weapon_frame::validateIdentity(
            committed, committed, emptyFlags, emptyRequired,
            42u, 900u, 42u, 900u, 0x1000u, 0u, false) != Failure::None)
        return fail();
    // Empty hands still need this exact solved pose; an old gun address cannot
    // masquerade as an unarmed commit, and equipping restores gun requirements.
    if (fnvxr::weapon_frame::validateIdentity(
            committed, committed, emptyFlags, emptyRequired,
            42u, 900u, 44u, 900u, 0x1000u, 0u, false) != Failure::PoseMismatch)
        return fail();
    if (fnvxr::weapon_frame::validateIdentity(
            committed, committed, emptyFlags, emptyRequired,
            42u, 900u, 42u, 900u, 0x1000u, 0x2000u, false) != Failure::MissingNodes)
        return fail();
    if (fnvxr::weapon_frame::validateIdentity(
            committed, committed, emptyFlags & ~fnvxr::shared::WeaponFrameFlagArmSolved,
            emptyRequired, 42u, 900u, 42u, 900u, 0x1000u, 0u, false) != Failure::Incomplete)
        return fail();
    if (fnvxr::shared::weaponFrameRequiredFlags(0u) != required
        || fnvxr::weapon_frame::validateIdentity(
            committed, committed, emptyFlags & ~fnvxr::shared::WeaponFrameFlagHandsOnly,
            required, 42u, 900u, 42u, 900u, 0x1000u, 0u) != Failure::Incomplete)
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
    if (!fnvxr::weapon_frame::committedPoseOwnsLiveWeaponTransform(
            stableWeaponPosition,
            committedTransform,
            stableWeaponRotation,
            committedWeaponRotation,
            0.02f,
            0.01f))
    {
        return fail();
    }
    // A host-spatial hand deliberately ignores stock hand animation while
    // retaining the exact committed weapon pose.
    if (!fnvxr::weapon_frame::committedPoseOwnsLiveWeaponTransform(
            stableWeaponPosition,
            committedTransform,
            stableWeaponRotation,
            committedWeaponRotation,
            0.02f,
            0.01f)
        || fnvxr::weapon_frame::committedPoseOwnsLiveWeaponTransform(
            overwrittenTransform,
            committedTransform,
            stableWeaponRotation,
            committedWeaponRotation,
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
