#pragma once

#include <cstdint>

namespace fnvxr::engine
{
// The renderer and the tracked rig share this body anchor within one native
// frame. It contains no HMD delta and no cinematic camera transform.
struct FirstPersonView
{
    float rotation[9] {}; // NiCamera columns: forward, up, right.
    float position[3] {};
    std::uint32_t excludedBodyRoot = 0u;
};
static_assert(sizeof(FirstPersonView) == 52u);
}
