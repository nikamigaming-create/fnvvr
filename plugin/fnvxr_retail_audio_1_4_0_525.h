#pragma once

#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fnvxr::retail_1_4_0_525
{
// The native buffered and streamed sound constructors both finish their
// secondary DSBUFFERDESC with OR ECX, 0x2A0. Add DSBCAPS_GLOBALFOCUS there:
// native voices, music and effects retain their original playback/3D rules
// while an OpenXR session owns input independently of desktop focus.
// These are complete instruction windows from the supported retail build.
inline bool enableBackgroundAudio() noexcept
{
    struct Site { std::uintptr_t window; std::uintptr_t instruction; std::uint64_t hash; };
    constexpr Site sites[] {
        { 0x00AEB937u, 0x00AEB943u, 0x66d52953950ace74ull },
        { 0x00AEC4A7u, 0x00AEC4B0u, 0x5574fad488f597a6ull },
    };
    constexpr unsigned char expected[] { 0x81, 0xC9, 0xA0, 0x02, 0x00, 0x00 };
    DWORD protections[2] {};
    __try
    {
        for (const auto& site : sites)
        {
            const auto* bytes = reinterpret_cast<const unsigned char*>(site.window);
            std::uint64_t hash = 14695981039346656037ull;
            for (std::size_t i = 0; i < 96; ++i) hash = (hash ^ bytes[i]) * 1099511628211ull;
            if (hash != site.hash || std::memcmp(reinterpret_cast<const void*>(site.instruction),
                    expected, sizeof(expected)) != 0)
                return false;
        }
        for (std::size_t i = 0; i < 2; ++i)
        {
            if (!VirtualProtect(reinterpret_cast<void*>(sites[i].instruction), sizeof(expected),
                    PAGE_EXECUTE_READWRITE, &protections[i]))
            {
                if (i != 0) { DWORD unused = 0; VirtualProtect(reinterpret_cast<void*>(sites[0].instruction),
                    sizeof(expected), protections[0], &unused); }
                return false;
            }
        }
        for (std::size_t i = 0; i < 2; ++i)
        {
            // One atomic byte changes only the immediate's GLOBALFOCUS bit.
            *reinterpret_cast<volatile unsigned char*>(sites[i].instruction + 3u) = 0x82;
            DWORD unused = 0;
            VirtualProtect(reinterpret_cast<void*>(sites[i].instruction), sizeof(expected), protections[i], &unused);
            FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(sites[i].instruction), sizeof(expected));
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
}
