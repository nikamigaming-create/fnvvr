#include "../../../adapters/openmw/rig_adapter.hpp"
#include <cmath>
#include <iostream>
int main()
{
    using namespace fnvxr::adapters::openmw;
    const Stereo::Pose source { Stereo::Position::fromMeters(.2f, .4f, 1.5f),
        osg::Quat(0, 0, std::sqrt(.5), std::sqrt(.5)) };
    auto grip = toRig(source);
    const auto roundTrip = fromRig(grip);
    if ((roundTrip.position.asMeters() - source.position.asMeters()).length() > 1e-5
        || std::fabs(grip.y - 1.5f) > 1e-5 || std::fabs(grip.z + .4f) > 1e-5
        || std::fabs(grip.qy - std::sqrt(.5f)) > 1e-5) return 1;
    const FnvxrRigPose mount { 0, 0, -.2f, 0, 0, 0, 1 };
    FnvxrRigPose hand {};
    if (!fnvxr_rig_compose_v1(&grip, &mount, &hand)) return 2;
    const auto position = fromRig(hand).position.asMeters();
    if (std::fabs(position.x()) > 1e-5 || std::fabs(position.y() - .4f) > 1e-5
        || std::fabs(position.z() - 1.5f) > 1e-5) return 3;
    std::cout << "OpenMW: actual Stereo::Pose types, axes, meter units and shared attachment passed.\n";
    return 0;
}
