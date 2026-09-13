#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>

namespace fnvxr::retail_1_4_0_525::melee
{
using Select = bool (__cdecl*)(void*, bool, void*&);
inline Select select = nullptr;
inline bool installed = false;

// TES::RayCast (thiscall, two caller arguments, callee pops eight bytes).
// The native query owns Havok filtering and collision; no actor list or
// camera-centered cone is substituted for physical contact.
inline void* cast(void* player, const float* from, const float* to) noexcept
{
#if defined(_M_IX86)
    __try
    {
        alignas(16) unsigned char ray[0xB0] {};
        constexpr std::uint32_t conversionBits = 0x3E124DD2u;
        float conversion = 0;
        std::memcpy(&conversion, &conversionBits, sizeof(conversion));
        for (unsigned i = 0; i < 3; ++i)
        {
            reinterpret_cast<float*>(ray)[i] = from[i] * conversion;
            reinterpret_cast<float*>(ray + 0x10)[i] = to[i] * conversion;
        }
        *reinterpret_cast<float*>(ray + 0x40) = 1.0f;
        *reinterpret_cast<std::uint32_t*>(ray + 0x44) = 0xFFFFFFFFu;
        *reinterpret_cast<std::uint32_t*>(ray + 0x50) = 0xFFFFFFFFu;
        const auto process = *reinterpret_cast<std::uintptr_t*>(static_cast<unsigned char*>(player) + 0x68);
        const auto controller = *reinterpret_cast<std::uintptr_t*>(process + 0x138);
        const auto proxy = *reinterpret_cast<std::uintptr_t*>(controller + 0x594);
        const auto body = *reinterpret_cast<std::uintptr_t*>(proxy + 8);
        const auto filter = *reinterpret_cast<std::uint32_t*>(body + 0x2C);
        *reinterpret_cast<std::uint32_t*>(ray + 0x24) = (filter & 0xFFFF0000u) | 6u;
        using RayCast = void* (__thiscall*)(void*, void*, bool);
        void* node = reinterpret_cast<RayCast>(0x00458440u)(
            *reinterpret_cast<void**>(0x011DEA10u), ray, true);
        for (unsigned depth = 0; node && depth < 128; ++depth)
        {
            const auto address = reinterpret_cast<std::uintptr_t>(node);
            if (*reinterpret_cast<std::uintptr_t*>(address) == 0x010A8F90u)
                if (void* ref = *reinterpret_cast<void**>(address + 0xCC)) return ref;
            node = *reinterpret_cast<void**>(address + 0x18);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
#else
    static_cast<void>(player); static_cast<void>(from); static_cast<void>(to);
#endif
    return nullptr;
}

#if defined(_M_IX86)
inline void* __fastcall choose(void* actor, std::uintptr_t callerFrame)
{
    void* target = nullptr;
    __try
    {
        if (select && select(actor, (*reinterpret_cast<unsigned char*>(callerFrame + 8) != 0), target))
            return target;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
    using Native = void* (__thiscall*)(void*);
    return reinterpret_cast<Native>(0x008990F0u)(actor);
}
__declspec(naked) inline void relay()
{
    __asm { mov edx, ebp }
    __asm { jmp choose }
}
#endif
inline bool install(Select callback) noexcept
{
    if (installed) return true;
#if !defined(_M_IX86)
    static_cast<void>(callback); return false;
#else
    __try
    {
        constexpr std::uintptr_t site = 0x008997BCu;
        constexpr unsigned char expected[] {0xE8,0x2F,0xF9,0xFF,0xFF};
        const auto hash = [](std::uintptr_t start) {
            std::uint64_t value = 14695981039346656037ull;
            for (unsigned i = 0; i < 96; ++i)
                value = (value ^ reinterpret_cast<const unsigned char*>(start)[i]) * 1099511628211ull;
            return value;
        };
        if (!callback || std::memcmp(reinterpret_cast<void*>(site), expected, 5) != 0
            || hash(0x00899790u) != 0xb98c4dafa1bfc940ull
            || hash(0x00458440u) != 0xf60bb300fb5d7b3full) return false;
        DWORD protection = 0;
        if (!VirtualProtect(reinterpret_cast<void*>(site), 5, PAGE_EXECUTE_READWRITE, &protection)) return false;
        unsigned char replacement[5] {0xE8,0,0,0,0};
        const auto relative = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&relay) - site - 5);
        std::memcpy(replacement + 1, &relative, 4);
        select = callback;
        std::memcpy(reinterpret_cast<void*>(site), replacement, 5);
        DWORD unused = 0;
        VirtualProtect(reinterpret_cast<void*>(site), 5, protection, &unused);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(site), 5);
        installed = true;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
#endif
}
}
