#pragma once

#include "../frame_math.h"

namespace fnvxr::kernel::wrist
{
// Coordinate contract: right-handed meters, +X right, +Y up, -Z forward.
// Quaternions are Hamilton xyzw rotations from local space to parent space.
// A UI quad lies in local XY and its visible front normal is local +Z.
class PoseSnapshot final
{
public:
    explicit PoseSnapshot(const Pose& pose = {}, bool tracked = false) noexcept
        : pose_(pose), tracked_(tracked)
    {
    }

    PoseSnapshot(const PoseSnapshot&) noexcept = default;
    PoseSnapshot(PoseSnapshot&&) noexcept = default;
    PoseSnapshot& operator=(const PoseSnapshot&) = delete;
    PoseSnapshot& operator=(PoseSnapshot&&) = delete;

    [[nodiscard]] const Pose& pose() const noexcept { return pose_; }
    [[nodiscard]] bool tracked() const noexcept { return tracked_; }

private:
    Pose pose_ {};
    bool tracked_ = false;
};

class LocalScreenTransform final
{
public:
    explicit LocalScreenTransform(const Pose& gripToScreen,
        const Vec3& gripLocalScalePivot = {}) noexcept
        : gripToScreen_(gripToScreen), gripLocalScalePivot_(gripLocalScalePivot)
    {
    }

    LocalScreenTransform(const LocalScreenTransform&) noexcept = default;
    LocalScreenTransform(LocalScreenTransform&&) noexcept = default;
    LocalScreenTransform& operator=(const LocalScreenTransform&) = delete;
    LocalScreenTransform& operator=(LocalScreenTransform&&) = delete;

    [[nodiscard]] const Pose& gripToScreen() const noexcept
    {
        return gripToScreen_;
    }

    [[nodiscard]] const Vec3& gripLocalScalePivot() const noexcept
    {
        return gripLocalScalePivot_;
    }

private:
    Pose gripToScreen_ {};
    Vec3 gripLocalScalePivot_ {};
};
}
