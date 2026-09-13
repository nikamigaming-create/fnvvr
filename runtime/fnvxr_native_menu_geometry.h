#pragma once
#include <algorithm>
#include <cmath>

namespace fnvxr::ui
{
struct MenuRect
{
    float x{}, y{}, width{}, height{};
    bool valid() const noexcept
    {
        return std::isfinite(x) && std::isfinite(y)
            && std::isfinite(width) && std::isfinite(height)
            && width > 0.0f && height > 0.0f;
    }
    bool contains(float px, float py) const noexcept
    { return valid() && px >= x && py >= y && px < x + width && py < y + height; }
};

inline MenuRect clipMenuRect(MenuRect rect, const MenuRect& clip) noexcept
{
    if (!rect.valid() || !clip.valid()) return {};
    const float left = (std::max)(rect.x, clip.x), top = (std::max)(rect.y, clip.y);
    const float right = (std::min)(rect.x + rect.width, clip.x + clip.width);
    const float bottom = (std::min)(rect.y + rect.height, clip.y + clip.height);
    return {left, top, (std::max)(0.0f, right - left), (std::max)(0.0f, bottom - top)};
}

struct MenuViewport
{
    float width{}, height{}, centerX{}, centerZ{}, scale{};
    bool valid() const noexcept
    {
        return std::isfinite(width) && std::isfinite(height)
            && width >= 2.0f && height >= 2.0f
            && std::isfinite(centerX) && std::isfinite(centerZ)
            && std::isfinite(scale) && scale > 0.00001f;
    }
};

// Native screen tiles lie on X/Z. Their NiNode already includes the offsets
// of every locus ancestor; a tile without a locus applies its own offset in
// geometry instead. Read the live viewport rather than guessing 640x480.
inline MenuRect renderedMenuRect(const MenuViewport& viewport,
    float worldX, float worldZ, float worldScale, bool locus,
    float localX, float localY, float width, float height) noexcept
{
    if (!viewport.valid() || !std::isfinite(worldScale) || worldScale <= 0.0f) return {};
    const float scale = worldScale / viewport.scale;
    MenuRect rect{viewport.width * 0.5f + (worldX - viewport.centerX) / viewport.scale,
        viewport.height * 0.5f - (worldZ - viewport.centerZ) / viewport.scale,
        width * scale, height * scale};
    if (!locus) { rect.x += localX * scale; rect.y += localY * scale; }
    return rect.valid() ? rect : MenuRect{};
}

inline bool nativeMenuPointer(const MenuViewport& viewport, float sourceWidth,
    float sourceHeight, float& x, float& y) noexcept
{
    if (!viewport.valid() || !std::isfinite(sourceWidth) || !std::isfinite(sourceHeight)
        || sourceWidth < 2.0f || sourceHeight < 2.0f || !std::isfinite(x) || !std::isfinite(y)
        || x < 0.0f || y < 0.0f || x >= sourceWidth || y >= sourceHeight) return false;
    x *= viewport.width / sourceWidth;
    y *= viewport.height / sourceHeight;
    return true;
}
}
