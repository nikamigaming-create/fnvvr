#pragma once

#include <cstdint>
#include <cstring>

namespace fnvxr::engine
{
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
