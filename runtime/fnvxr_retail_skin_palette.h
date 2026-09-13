#pragma once

#include <cstdint>
#include <cstring>
#include <array>

namespace fnvxr::engine
{
// The retail palette producer (E6FE30) subtracts the current renderer camera
// position from each bone, yet keys the result only by frame and row count.
// Different cameras in one game frame therefore require a new palette. A
// fixed table bounds storage; collisions cause a harmless extra rebuild.
class RetailSkinPaletteCameraCache
{
public:
    bool needsRebuild(std::uintptr_t skin, const std::array<float, 3>& camera) noexcept
    {
        auto& entry = mEntries[(skin >> 4u) % mEntries.size()];
        const bool changed = entry.skin != skin || entry.camera != camera;
        entry = { skin, camera };
        return changed;
    }
private:
    struct Entry { std::uintptr_t skin = 0; std::array<float, 3> camera {}; };
    std::array<Entry, 4096> mEntries {};
};

// Exact retail NiGeometry::skinInstance (+BC), NiSkinInstance::frameID (+18).
// NiSkinInstance's constructors at A8672D/A86878 initialize this cache key
// to FFFFFFFF. Reusing a palette after VR changes its bones in the SAME game
// frame mixes the previous pose's skinned pieces with current rigid pieces.
inline bool invalidateRetailSkinPalette(std::uintptr_t geometryAddress) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    if (geometryAddress < 0x10000u)
        return false;
    __try
    {
        std::uint32_t skinAddress = 0;
        std::memcpy(&skinAddress,
            reinterpret_cast<const void*>(geometryAddress + 0xBCu), 4);
        if (skinAddress < 0x10000u)
            return false;
        auto* skin = reinterpret_cast<std::uint8_t*>(skinAddress);
        std::uint32_t vtable = 0, data = 0, bones = 0;
        std::memcpy(&vtable, skin, 4);
        std::memcpy(&data, skin + 0x08u, 4);
        std::memcpy(&bones, skin + 0x14u, 4);
        if ((vtable != 0x01069B14u && vtable != 0x01069A84u)
            || !data || !bones)
            return false;
        constexpr std::uint32_t InvalidPaletteFrame = 0xFFFFFFFFu;
        std::memcpy(skin + 0x18u, &InvalidPaletteFrame, 4);
        return true;
    }
    __except (1) { return false; }
#else
    static_cast<void>(geometryAddress);
    return false;
#endif
}
}
