#pragma once

namespace fnvxr::pipboy
{
struct ScreenCrop
{
    float left {};
    float top {};
    float right {};
    float bottom {};
};

// Retail's live screen occupies this region of its own UI source. The host
// never renders that source as a Pip-Boy quad; it only maps wrist-device UVs
// into the ordinary retail screen/pointer coordinate space.
inline constexpr ScreenCrop RetailScreenCrop {
    0.255f,
    0.315f,
    0.745f,
    0.600f,
};

constexpr bool validScreenCrop(const ScreenCrop& crop) noexcept
{
    return crop.left >= 0.0f
        && crop.top >= 0.0f
        && crop.right <= 1.0f
        && crop.bottom <= 1.0f
        && crop.right > crop.left
        && crop.bottom > crop.top;
}

constexpr float screenAspect(
    float sourceAspect,
    const ScreenCrop& crop = RetailScreenCrop) noexcept
{
    return sourceAspect > 0.0f && validScreenCrop(crop)
        ? sourceAspect
            * (crop.right - crop.left)
            / (crop.bottom - crop.top)
        : 0.0f;
}

constexpr float sourceUFromPanel(
    float panelU,
    const ScreenCrop& crop = RetailScreenCrop) noexcept
{
    return crop.left + panelU * (crop.right - crop.left);
}

constexpr float sourceVFromPanel(
    float panelV,
    const ScreenCrop& crop = RetailScreenCrop) noexcept
{
    return crop.top + panelV * (crop.bottom - crop.top);
}
}
