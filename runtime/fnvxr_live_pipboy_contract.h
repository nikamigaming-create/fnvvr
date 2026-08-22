#pragma once

#include "../protocol/fnvxr_shared_state.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace fnvxr::engine::live_pipboy
{
// The Pip-Boy is a persistent first-person prop. Opening its live screen
// changes retail input focus, not presentation ownership. Only a second,
// conflicting menu moves presentation to the ordinary front-facing UI quad.
inline constexpr std::uint32_t ConflictingMenuBits =
    shared::RuntimeStartMenuBit
    | shared::RuntimeRaceSexMenuBit
    | shared::RuntimeDialogMenuBit
    | shared::RuntimeVatsMenuBit
    | shared::RuntimeLoadingMenuBit
    | shared::RuntimeGenericMenuBit;

constexpr bool hasFocusedScreen(std::uint32_t menuBits) noexcept
{
    return (menuBits & shared::RuntimePipBoyMenuBit) != 0u;
}

constexpr bool hasConflictingMenu(std::uint32_t menuBits) noexcept
{
    return (menuBits & ConflictingMenuBits) != 0u;
}

constexpr bool worldPresentationContinues(
    std::uint32_t phase,
    std::uint32_t menuBits,
    std::uint32_t showroomActive,
    bool cameraActive) noexcept
{
    return shared::runtimeLivePipBoyFocus(
        phase,
        menuBits,
        showroomActive,
        cameraActive);
}

struct FocusInput
{
    bool rayIntersectsDevice = false;
    bool activationGripHeld = false;
    bool retailScreenOpen = false;
    std::uint32_t hoverFrames = 0u;
    std::uint32_t focusFrames = 0u;
};

struct FocusDecision
{
    bool hovered = false;
    bool focused = false;
    bool requestOpen = false;
    float scale = 1.0f;
};

inline FocusDecision assessFocus(const FocusInput& input) noexcept
{
    const bool activationHover = input.rayIntersectsDevice
        && input.activationGripHeld;
    const bool focusFromPointing = activationHover
        && input.hoverFrames >= input.focusFrames;
    const bool focused = input.retailScreenOpen || focusFromPointing;
    const float progress = !activationHover
        ? 0.0f
        : input.focusFrames == 0u
        ? 1.0f
        : std::clamp(
            static_cast<float>(input.hoverFrames)
                / static_cast<float>(input.focusFrames),
            0.0f,
            1.0f);
    return {
        activationHover,
        focused,
        focusFromPointing && !input.retailScreenOpen,
        1.0f + 0.35f * (focused ? 1.0f : progress),
    };
}

inline int weaponOrbitSlot(float x, float y, float deadzone = 0.35f) noexcept
{
    if (!std::isfinite(x) || !std::isfinite(y)
        || x * x + y * y < deadzone * deadzone)
    {
        return -1;
    }
    constexpr float TanHalfSector = 0.41421356237f;
    const float ax = std::fabs(x);
    const float ay = std::fabs(y);
    if (ay <= ax * TanHalfSector)
        return x >= 0.0f ? 2 : 6;
    if (ax <= ay * TanHalfSector)
        return y >= 0.0f ? 0 : 4;
    if (x > 0.0f)
        return y > 0.0f ? 1 : 3;
    return y > 0.0f ? 7 : 5;
}

enum class PhysicalControl : std::uint8_t
{
    Screen,
    StatsDial,
    ItemsDial,
    DataDial,
    ScrollUp,
    ScrollDown,
};

inline float screenUFromDevice(float u) noexcept
{
    return std::clamp((u - 0.16f) / 0.68f, 0.0f, 1.0f);
}

inline float screenVFromDevice(float v) noexcept
{
    return std::clamp(v, 0.0f, 1.0f);
}

inline PhysicalControl physicalControl(float u, float v) noexcept
{
    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);
    if (u < 0.16f)
    {
        if (v < 1.0f / 3.0f)
            return PhysicalControl::StatsDial;
        if (v < 2.0f / 3.0f)
            return PhysicalControl::ItemsDial;
        return PhysicalControl::DataDial;
    }
    if (u > 0.84f)
        return v < 0.5f
            ? PhysicalControl::ScrollUp
            : PhysicalControl::ScrollDown;
    return PhysicalControl::Screen;
}
}
