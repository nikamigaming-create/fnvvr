#pragma once
#include <windows.h>
#include <cstdint>

namespace fnvxr::retail_1_4_0_525::target_retry
{
// The native failure path calls BeginUsingRenderTargetGroup on the default
// group through the same virtual slot. If that bind also fails (notably after
// a failed Reset), retail recursively retries until its stack overflows.
// Retain the first fallback, but return failure from any recursive retry.
inline thread_local unsigned depth = 0;
inline void (*report)() noexcept = nullptr;
inline bool installed = false;
#if defined(_M_IX86)
inline bool __fastcall begin(void* renderer, void*, void* group, std::uint32_t clearFlags)
{
    if (depth >= 2)
    {
        if (report) report();
        return false;
    }
    struct Scope { Scope() { ++depth; } ~Scope() { --depth; } } scope;
    using Native = bool (__thiscall*)(void*, void*, std::uint32_t);
    return reinterpret_cast<Native>(0x00E6EEA0u)(renderer, group, clearFlags);
}
#endif
inline bool install(void (*onRejected)() noexcept) noexcept
{
    if (installed) return true;
#if !defined(_M_IX86)
    static_cast<void>(onRejected); return false;
#else
    __try
    {
        // Exact 1.4.0.525 NiDX9Renderer virtual slot 0x194. This does not
        // alter any caller arguments, successful bind, or device HRESULT.
        auto* slot = reinterpret_cast<std::uintptr_t*>(0x010EE650u);
        if (*slot != 0x00E6EEA0u) return false;
        std::uint64_t hash = 14695981039346656037ull;
        const auto* code = reinterpret_cast<const unsigned char*>(0x00E6EEA0u);
        for (unsigned i = 0; i < 96; ++i) hash = (hash ^ code[i]) * 1099511628211ull;
        if (hash != 0x4fe1ae363d1d7166ull) return false;
        DWORD previous = 0;
        if (!VirtualProtect(slot, 4, PAGE_EXECUTE_READWRITE, &previous)) return false;
        report = onRejected;
        *slot = reinterpret_cast<std::uintptr_t>(&begin);
        DWORD unused = 0;
        VirtualProtect(slot, 4, previous, &unused);
        installed = true;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
#endif
}
}
