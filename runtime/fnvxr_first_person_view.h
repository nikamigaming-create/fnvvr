#pragma once

#include <cstdint>

namespace fnvxr::engine
{
// Scripted first-person movement (beds, chairs, character creation) owns the
// player's authored view. VATS playback is a third-person camera and must not
// take that authority. Head tracking is applied after this common rig anchor.
inline bool useAuthoredFirstPersonCamera(bool movementLocked, bool lookLocked,
    bool characterPreview, bool vatsActive) noexcept
{
    return !vatsActive && (movementLocked || lookLocked || characterPreview);
}

// The renderer and the tracked rig share this anchor within one native frame.
// It contains the authored first-person frame, with no HMD delta applied yet.
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
