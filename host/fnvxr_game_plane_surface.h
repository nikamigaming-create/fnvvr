#pragma once

namespace fnvxr::host::game_plane_surface
{
// The game plane is used for two deliberately different presentations:
// gameplay may opt into a curved/surround surface, while a retail menu must
// remain a literal flat quad so it agrees with menu-pointer hit testing.
enum class Kind
{
    FlatFallback,
    CurvedGameplay,
    FlatUiQuad,
};

constexpr Kind select(
    bool legacyUiScene,
    bool verifiedUiQuad,
    bool gameplayCurvatureEnabled,
    bool curvedMeshAvailable) noexcept
{
    if (legacyUiScene || verifiedUiQuad)
        return Kind::FlatUiQuad;
    if (gameplayCurvatureEnabled && curvedMeshAvailable)
        return Kind::CurvedGameplay;
    return Kind::FlatFallback;
}

constexpr bool permitsSourceSurround(
    Kind kind,
    bool gameplayModePermitsSurround) noexcept
{
    return kind != Kind::FlatUiQuad && gameplayModePermitsSurround;
}
}
