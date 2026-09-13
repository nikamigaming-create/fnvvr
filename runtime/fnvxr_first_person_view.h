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
    std::uint32_t excludedWeaponRoot = 0u;
    // Current native geometry after animation/equipment changes, joined to
    // this same pose instead of the earlier main-loop player publication.
    std::uint32_t firstPersonRoots[8] {};
    std::uint32_t firstPersonRootCount = 0u;
};
static_assert(sizeof(FirstPersonView) == 92u);
}
