#pragma once

#include "../frame_math.h"

namespace fnvxr::kernel::wrist
{
[[nodiscard]] bool finitePose(const Pose& pose) noexcept;
[[nodiscard]] Pose compose(const Pose& parent, const Pose& local) noexcept;
[[nodiscard]] Vec3 rotate(const Quaternion& rotation, const Vec3& value) noexcept;
[[nodiscard]] float distance(const Vec3& left, const Vec3& right) noexcept;
[[nodiscard]] float angleDegrees(const Vec3& left, const Vec3& right) noexcept;
}
