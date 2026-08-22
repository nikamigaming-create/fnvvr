#pragma once

#include <cmath>
#include <cstdint>

namespace fnvxr::weapon_frame
{
enum class Failure : std::uint32_t
{
    None = 0,
    Unavailable = 1,
    NotCommitted = 2,
    PoseMismatch = 3,
    Incomplete = 4,
    MissingNodes = 5,
    TransformOverwritten = 6,
};

constexpr Failure validateIdentity(
    std::uint32_t status,
    std::uint32_t committedStatus,
    std::uint32_t flags,
    std::uint32_t requiredFlags,
    std::uint32_t committedPoseSequence,
    std::uint64_t committedPoseFrame,
    std::uint32_t renderedPoseSequence,
    std::uint64_t renderedPoseFrame,
    std::uint32_t rightHandAddress,
    std::uint32_t weaponAddress) noexcept
{
    if (status != committedStatus)
        return Failure::NotCommitted;
    if (committedPoseSequence != renderedPoseSequence
        || committedPoseFrame != renderedPoseFrame)
        return Failure::PoseMismatch;
    if ((flags & requiredFlags) != requiredFlags)
        return Failure::Incomplete;
    if (!rightHandAddress || !weaponAddress)
        return Failure::MissingNodes;
    return Failure::None;
}

inline bool transformMatches(
    const float* live,
    const float* committed,
    int count,
    float tolerance) noexcept
{
    if (!live || !committed || count <= 0 || tolerance < 0.0f)
        return false;
    for (int i = 0; i < count; ++i)
    {
        if (!std::isfinite(live[i])
            || !std::isfinite(committed[i])
            || std::fabs(live[i] - committed[i]) > tolerance)
            return false;
    }
    return true;
}

// A render transaction may visit the controller-rig seam more than once for
// one OpenXR pose. Skip the second write only when the live hand and weapon
// still agree with the committed controller pose. Gamebryo can overwrite the
// hand while leaving the child's cached world transform untouched, so checking
// only the weapon produces a thresholded "moves once in a while" result.
inline bool committedPoseOwnsLiveRigTransforms(
    const float* liveHandPosition,
    const float* committedHandPosition,
    const float* liveWeaponPosition,
    const float* committedWeaponPosition,
    const float* liveWeaponRotation,
    const float* committedWeaponRotation,
    float handPositionTolerance,
    float weaponPositionTolerance,
    float rotationTolerance) noexcept
{
    return transformMatches(
            liveHandPosition, committedHandPosition, 3, handPositionTolerance)
        && transformMatches(
            liveWeaponPosition,
            committedWeaponPosition,
            3,
            weaponPositionTolerance)
        && transformMatches(
            liveWeaponRotation, committedWeaponRotation, 9, rotationTolerance);
}

// When the OpenXR host replaces the unstable stock hand category in the final
// eye, the retail mutation seam owns only the stock weapon. Stock animation is
// free to move or cull the unused engine hand; that must not invalidate an
// otherwise exact weapon commit or cause the hook to rewrite the hidden arm.
inline bool committedPoseOwnsLiveWeaponTransform(
    const float* liveWeaponPosition,
    const float* committedWeaponPosition,
    const float* liveWeaponRotation,
    const float* committedWeaponRotation,
    float weaponPositionTolerance,
    float rotationTolerance) noexcept
{
    return transformMatches(
            liveWeaponPosition,
            committedWeaponPosition,
            3,
            weaponPositionTolerance)
        && transformMatches(
            liveWeaponRotation, committedWeaponRotation, 9, rotationTolerance);
}

// Normal animation callbacks solve one controller pose once. The renderer is
// the exception: if stock animation overwrote that same pose before the eye
// traversal, it must be allowed to restore it at the render boundary.
constexpr bool duplicatePoseSolveCanBeSkipped(
    std::int32_t poseSequence,
    std::int32_t lastSolvedPoseSequence,
    bool rendererRestoreActive) noexcept
{
    return poseSequence == lastSolvedPoseSequence && !rendererRestoreActive;
}

// The first-person "Weapon" node is a stable hand attachment. Fallout swaps
// the model beneath it when an inventory equip completes, so wrapper identity
// alone cannot prove that the calibration still belongs to the equipped gun.
constexpr bool weaponBindingMustBeRecalibrated(
    bool explicitRefresh,
    std::uint32_t previousEquippedFormId,
    std::uint32_t currentEquippedFormId,
    std::uintptr_t previousWeaponNode,
    std::uintptr_t currentWeaponNode,
    std::uintptr_t previousModelNode,
    std::uintptr_t currentModelNode,
    std::uintptr_t previousEndpointNode,
    std::uintptr_t currentEndpointNode) noexcept
{
    return explicitRefresh
        || previousEquippedFormId != currentEquippedFormId
        || previousWeaponNode != currentWeaponNode
        || previousModelNode != currentModelNode
        || previousEndpointNode != currentEndpointNode;
}

constexpr bool weaponBindingReady(
    bool weaponNodePresent,
    bool modelNodePresent,
    bool endpointInCurrentModel,
    std::uint32_t equippedFormId,
    std::uint32_t modelFormId) noexcept
{
    return weaponNodePresent
        && modelNodePresent
        && endpointInCurrentModel
        && (equippedFormId == 0u
            || modelFormId == 0u
            || equippedFormId == modelFormId);
}

constexpr bool preserveCommittedPose(
    std::uint32_t status,
    std::uint32_t committedStatus,
    std::uint32_t committedPoseSequence,
    std::uint64_t committedPoseFrame,
    std::uint32_t candidatePoseSequence,
    std::uint64_t candidatePoseFrame,
    bool candidateComplete) noexcept
{
    return !candidateComplete
        && status == committedStatus
        && committedPoseSequence == candidatePoseSequence
        && committedPoseFrame == candidatePoseFrame;
}
}
