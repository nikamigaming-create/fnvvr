#pragma once

#include <windows.h>
#include <cstdint>
#include <cstring>
#include "fnvxr_tracked_shot.h"

namespace fnvxr::retail_1_4_0_525::shot
{
using Prepare = tracked_shot::Decision (__cdecl*)(void*, void*, tracked_shot::Pose&);
using Observe = void (__cdecl*)(void*);
inline Prepare prepare = nullptr;
inline Observe observe = nullptr;
inline bool installed = false;

#if defined(_M_IX86)
// TESObjectWEAP::Fire at 00523150. The native pellet loop copies its base
// pitch/yaw immediately here, then adds weapon spread at 005244D0/005244E2.
// The existing native/JIP projectile creator and hit path remain untouched.
// Frame slots: shooter -2C, weapon -238, origin -44/-40/-3C, pitch -60, yaw -20.
inline constexpr std::uintptr_t Site = 0x0052442Du;
inline constexpr std::uintptr_t Resume = 0x00524436u;
inline constexpr std::uintptr_t SkipPellets = 0x0052467Fu;
inline constexpr std::uintptr_t CreatedSite = 0x005245C2u;
inline constexpr std::uintptr_t CreatedResume = 0x005245CBu;
inline constexpr std::uintptr_t CameraAimSite = 0x009BD9E2u;
inline constexpr std::uintptr_t VelocitySite = 0x009BEBF4u;
inline constexpr std::uintptr_t NativeSetVelocity = 0x0062B8D0u;
struct PendingShot
{
    tracked_shot::Decision decision;
    void* actor;
    void* weapon;
    ULONGLONG timeMs;
    bool velocityOverride;
    float velocity[3];
    bool velocityApplied;
};
inline thread_local PendingShot pending[16] {};
inline thread_local unsigned pendingDepth = 0;

// Only the native creation call nested in this pellet owns this exemption.
// Nested script/NPC fire gets its own entry and cannot inherit player aim.
inline void __fastcall cameraAim(void* player, void*, void* projectile,
    void* node, float* yaw, float* pitch, void* position, float first, float second)
{
    bool tracked = false;
    __try
    {
        if (projectile && pendingDepth && pendingDepth <= 16)
        {
            const auto& shot = pending[pendingDepth - 1];
            const auto address = reinterpret_cast<std::uintptr_t>(projectile);
            tracked = shot.decision == tracked_shot::Decision::Tracked
                && GetTickCount64() - shot.timeMs <= 100u
                && shot.actor == player
                && *reinterpret_cast<void**>(address + 0xFCu) == shot.actor
                && *reinterpret_cast<void**>(address + 0xF8u) == shot.weapon;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { tracked = false; }
    if (!tracked)
    {
        using Native = void (__thiscall*)(void*, void*, void*, float*, float*, void*, float, float);
        reinterpret_cast<Native>(0x00965620u)(player, projectile, node, yaw, pitch, position, first, second);
    }
}

inline void __cdecl created(void* projectile) noexcept
{
    __try { if (observe) observe(projectile); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    if (pendingDepth) --pendingDepth;
}

inline tracked_shot::Decision __cdecl apply(std::uintptr_t frame) noexcept
{
    __try
    {
        tracked_shot::Pose pose {};
        const auto decision = prepare ? prepare(
            *reinterpret_cast<void**>(frame - 0x2Cu),
            *reinterpret_cast<void**>(frame - 0x238u), pose)
            : tracked_shot::Decision::Stock;
        if (decision == tracked_shot::Decision::Tracked)
        {
            float pitch = 0.0f, yaw = 0.0f;
            tracked_shot::angles(pose, pitch, yaw);
            std::memcpy(reinterpret_cast<void*>(frame - 0x44u), pose.position, sizeof(pose.position));
            *reinterpret_cast<float*>(frame - 0x60u) = pitch;
            *reinterpret_cast<float*>(frame - 0x20u) = yaw;
        }
        if (decision != tracked_shot::Decision::Unavailable)
        {
            if (pendingDepth < 16)
                pending[pendingDepth] = { decision,
                    *reinterpret_cast<void**>(frame - 0x2Cu),
                    *reinterpret_cast<void**>(frame - 0x238u), GetTickCount64(),
                    pose.velocityOverride,
                    {pose.linearVelocity[0], pose.linearVelocity[1], pose.linearVelocity[2]} };
            ++pendingDepth;
        }
        return decision;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return tracked_shot::Decision::Unavailable; }
}

// Projectile::Initialize3D supplies its native world velocity to 62B8D0.
// Replace only that vector for the matched tracked throw being constructed.
// The native function still sets every rigid body, units, gravity and bounce.
inline void __cdecl applyVelocity(std::uintptr_t frame) noexcept
{
    __try
    {
        if (!pendingDepth || pendingDepth > 16) return;
        auto& shot = pending[pendingDepth - 1];
        const auto projectile = *reinterpret_cast<std::uintptr_t*>(frame - 0x20u);
        if (shot.decision == tracked_shot::Decision::Tracked && shot.velocityOverride
            && GetTickCount64() - shot.timeMs <= 100u && projectile
            && *reinterpret_cast<void**>(projectile + 0xFCu) == shot.actor
            && *reinterpret_cast<void**>(projectile + 0xF8u) == shot.weapon)
        {
            std::memcpy(reinterpret_cast<void*>(frame - 0x1Cu), shot.velocity, sizeof(shot.velocity));
            shot.velocityApplied = true;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

__declspec(naked) inline void velocityRelay()
{
    __asm
    {
        pushfd
        pushad
        sub esp, 0x220
        lea esi, [esp + 15]
        and esi, 0xfffffff0
        fxsave [esi]
        push ebp
        call applyVelocity
        add esp, 4
        fxrstor [esi]
        add esp, 0x220
        popad
        popfd
        jmp NativeSetVelocity
    }
}

__declspec(naked) inline void relay()
{
    __asm
    {
        pushfd
        pushad
        sub esp, 0x220
        lea esi, [esp + 15]
        and esi, 0xfffffff0
        fxsave [esi]
        push ebp
        call apply
        add esp, 4
        mov [esp + 0x210], eax
        fxrstor [esi]
        cmp dword ptr [esp + 0x210], 2
        je unavailable
        add esp, 0x220
        popad
        popfd
        fld dword ptr [ebp - 0x60]
        fstp dword ptr [ebp - 0x1b4]
        jmp Resume
    unavailable:
        add esp, 0x220
        popad
        popfd
        jmp SkipPellets
    }
}

__declspec(naked) inline void createdRelay()
{
    __asm
    {
        add esp, 0x40
        mov [ebp - 0x1a8], eax
        pushfd
        pushad
        sub esp, 0x220
        lea esi, [esp + 15]
        and esi, 0xfffffff0
        fxsave [esi]
        push eax
        call created
        add esp, 4
        fxrstor [esi]
        add esp, 0x220
        popad
        popfd
        jmp CreatedResume
    }
}
#endif

inline bool creationUsedHandVelocity() noexcept
{
#if defined(_M_IX86)
    return pendingDepth && pendingDepth <= 16 && pending[pendingDepth - 1].velocityApplied;
#else
    return false;
#endif
}

inline bool install(Prepare callback, Observe observer) noexcept
{
    if (installed) return true;
#if !defined(_M_IX86)
    static_cast<void>(callback);
    static_cast<void>(observer);
    return false;
#else
    __try
    {
        constexpr unsigned char expected[] { 0xD9,0x45,0xA0,0xD9,0x9D,0x4C,0xFE,0xFF,0xFF };
        constexpr unsigned char createdExpected[] { 0x83,0xC4,0x40,0x89,0x85,0x58,0xFE,0xFF,0xFF };
        constexpr unsigned char aimExpected[] { 0xE8,0x39,0x7C,0xFA,0xFF };
        constexpr unsigned char velocityExpected[] {0xE8,0xD7,0xCC,0xC6,0xFF};
        const auto* window = reinterpret_cast<const unsigned char*>(0x0052441Eu);
        std::uint64_t hash = 14695981039346656037ull;
        for (std::size_t i = 0; i < 96; ++i) hash = (hash ^ window[i]) * 1099511628211ull;
        const auto* aimWindow = reinterpret_cast<const unsigned char*>(0x009BD999u);
        std::uint64_t aimHash = 14695981039346656037ull;
        for (std::size_t i = 0; i < 96; ++i) aimHash = (aimHash ^ aimWindow[i]) * 1099511628211ull;
        std::uint64_t velocityHash = 14695981039346656037ull;
        for (unsigned i = 0; i < 96; ++i)
            velocityHash = (velocityHash ^ reinterpret_cast<const unsigned char*>(0x009BEBBAu)[i]) * 1099511628211ull;
        if (!callback || !observer || hash != 0x6d6438a42e80e4f6ull
            || aimHash != 0x3f795c2a5cc053a7ull
            || velocityHash != 0x55bf1e5978dc5f0bull
            || std::memcmp(reinterpret_cast<void*>(VelocitySite), velocityExpected, 5) != 0
            || std::memcmp(reinterpret_cast<void*>(Site), expected, sizeof(expected)) != 0
            || std::memcmp(reinterpret_cast<void*>(CreatedSite), createdExpected, sizeof(createdExpected)) != 0
            || std::memcmp(reinterpret_cast<void*>(CameraAimSite), aimExpected, sizeof(aimExpected)) != 0)
            return false;
        unsigned char replacement[sizeof(expected)] { 0xE9,0,0,0,0,0x90,0x90,0x90,0x90 };
        const auto relative = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&relay) - (Site + 5u));
        std::memcpy(replacement + 1, &relative, sizeof(relative));
        DWORD oldProtect = 0;
        if (!VirtualProtect(reinterpret_cast<void*>(Site), sizeof(expected), PAGE_EXECUTE_READWRITE, &oldProtect))
            return false;
        DWORD aimOldProtect = 0;
        if (!VirtualProtect(reinterpret_cast<void*>(CameraAimSite), sizeof(aimExpected), PAGE_EXECUTE_READWRITE, &aimOldProtect))
        {
            DWORD unused = 0;
            VirtualProtect(reinterpret_cast<void*>(Site), sizeof(expected), oldProtect, &unused);
            return false;
        }
        DWORD velocityOldProtect = 0;
        if (!VirtualProtect(reinterpret_cast<void*>(VelocitySite), 5, PAGE_EXECUTE_READWRITE, &velocityOldProtect))
        {
            DWORD unused = 0;
            VirtualProtect(reinterpret_cast<void*>(Site), sizeof(expected), oldProtect, &unused);
            VirtualProtect(reinterpret_cast<void*>(CameraAimSite), 5, aimOldProtect, &unused);
            return false;
        }
        prepare = callback;
        observe = observer;
        std::memcpy(reinterpret_cast<void*>(Site), replacement, sizeof(replacement));
        const auto createdRelative = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&createdRelay) - (CreatedSite + 5u));
        std::memcpy(replacement + 1, &createdRelative, sizeof(createdRelative));
        // Both sites lie in this same page; the original permission is restored below.
        std::memcpy(reinterpret_cast<void*>(CreatedSite), replacement, sizeof(replacement));
        unsigned char aimReplacement[5] { 0xE8,0,0,0,0 };
        const auto aimRelative = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&cameraAim) - (CameraAimSite + 5u));
        std::memcpy(aimReplacement + 1, &aimRelative, sizeof(aimRelative));
        std::memcpy(reinterpret_cast<void*>(CameraAimSite), aimReplacement, sizeof(aimReplacement));
        const auto velocityRelative = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&velocityRelay) - (VelocitySite + 5u));
        std::memcpy(aimReplacement + 1, &velocityRelative, 4);
        std::memcpy(reinterpret_cast<void*>(VelocitySite), aimReplacement, 5);
        DWORD unused = 0;
        VirtualProtect(reinterpret_cast<void*>(Site), sizeof(expected), oldProtect, &unused);
        VirtualProtect(reinterpret_cast<void*>(CameraAimSite), sizeof(aimExpected), aimOldProtect, &unused);
        VirtualProtect(reinterpret_cast<void*>(VelocitySite), 5, velocityOldProtect, &unused);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(Site), sizeof(expected));
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(CreatedSite), sizeof(createdExpected));
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(CameraAimSite), sizeof(aimExpected));
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(VelocitySite), 5);
        installed = true;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
#endif
}
}
