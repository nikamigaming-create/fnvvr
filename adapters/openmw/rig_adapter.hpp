#pragma once
#include "../rig_c_api.h"
#include <components/stereo/types.hpp>

namespace fnvxr::adapters::openmw
{
// Match XR::toXR/fromXR in OpenMW's components/xr/typeconversion.cpp.
// Stereo::Position owns MW-unit conversion; never apply UnitsPerMeter twice.
inline FnvxrRigPose toRig(const Stereo::Pose& pose)
{
    const auto p = pose.position.asMeters();
    const auto& q = pose.orientation;
    return { p.x(), p.z(), -p.y(), static_cast<float>(q.x()),
        static_cast<float>(q.z()), static_cast<float>(-q.y()), static_cast<float>(q.w()) };
}
inline Stereo::Pose fromRig(const FnvxrRigPose& pose)
{
    return { Stereo::Position::fromMeters(pose.x, -pose.z, pose.y),
        osg::Quat(pose.qx, -pose.qz, pose.qy, pose.qw) };
}
}
