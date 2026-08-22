#pragma once

#include <cstdint>

namespace fnvxr::pipboy
{
struct ScreenCrop
{
    float left {};
    float top {};
    float right {};
    float bottom {};
};

struct ScreenPixelEvidence
{
    std::uint32_t sampleCount {};
    float nonBlackFraction {};
    float blueDominantFraction {};
    float meanLuma {};
};

// Retail's live screen occupies this region of its own UI source. The host
// maps it onto the locally derived pipboyscreen:0 geometry and uses this same
// crop for pointer coordinates, keeping pixels and interaction in one space.
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

// A runtime menu bit is structural evidence, not proof that the copied
// surface has finished changing from the last world frame into the authored
// Pip-Boy UI.  The exact screen crop must be populated, dark enough to reject
// the outdoor world, and free of the historical blue fallback before it can
// become visible on the wrist.
constexpr bool screenPixelsReady(
    const ScreenPixelEvidence& evidence,
    float minimumNonBlackFraction = 0.30f,
    float maximumBlueDominantFraction = 0.20f,
    float maximumMeanLuma = 60.0f) noexcept
{
    return evidence.sampleCount != 0u
        && evidence.nonBlackFraction >= minimumNonBlackFraction
        && evidence.blueDominantFraction <= maximumBlueDominantFraction
        && evidence.meanLuma <= maximumMeanLuma;
}
}
