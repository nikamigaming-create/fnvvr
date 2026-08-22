#include "../host/fnvxr_game_plane_surface.h"
#include "../runtime/fnvxr_pipboy_panel_contract.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
using fnvxr::host::game_plane_surface::Kind;
using fnvxr::host::game_plane_surface::permitsSourceSurround;
using fnvxr::host::game_plane_surface::select;

bool require(bool condition, const char* message)
{
    if (condition)
        return true;
    std::cerr << message << "\n";
    return false;
}
}

int main()
{
    bool passed = true;
    // Menus take precedence over every gameplay display preference.
    passed = require(
        select(true, false, true, true) == Kind::FlatUiQuad,
        "legacy menu scene was allowed to curve") && passed;
    passed = require(
        select(false, true, true, true) == Kind::FlatUiQuad,
        "verified menu quad was allowed to curve") && passed;
    passed = require(
        select(true, true, true, true) == Kind::FlatUiQuad,
        "mixed menu evidence was allowed to curve") && passed;

    // World presentation retains the configurable gameplay behavior.
    passed = require(
        select(false, false, true, true) == Kind::CurvedGameplay,
        "gameplay lost its configured curved surface") && passed;
    passed = require(
        select(false, false, false, true) == Kind::FlatFallback,
        "disabled gameplay curvature did not fall back to a flat plane") && passed;
    passed = require(
        select(false, false, true, false) == Kind::FlatFallback,
        "missing curved mesh did not fall back to a flat plane") && passed;

    passed = require(
        !permitsSourceSurround(Kind::FlatUiQuad, true),
        "menu quad was allowed to use gameplay surround geometry") && passed;
    passed = require(
        permitsSourceSurround(Kind::CurvedGameplay, true),
        "curved gameplay surface lost source surround") && passed;
    passed = require(
        permitsSourceSurround(Kind::FlatFallback, true),
        "flat gameplay fallback lost source surround") && passed;
    passed = require(
        !permitsSourceSurround(Kind::CurvedGameplay, false),
        "disabled gameplay surround was enabled") && passed;

    constexpr auto crop = fnvxr::pipboy::RetailScreenCrop;
    passed = require(
        fnvxr::pipboy::validScreenCrop(crop)
            && crop.left > 0.20f && crop.right < 0.80f
            && crop.top > 0.30f && crop.bottom < 0.62f,
        "Pip-Boy crop includes the surrounding arms/world") && passed;
    passed = require(
        std::fabs(fnvxr::pipboy::screenAspect(1872.0f / 2016.0f) - 1.60f)
            < 0.02f,
        "Pip-Boy screen crop is distorted") && passed;
    passed = require(
        std::fabs(fnvxr::pipboy::sourceUFromPanel(0.0f) - crop.left) < 0.00001f
            && std::fabs(fnvxr::pipboy::sourceUFromPanel(1.0f) - crop.right) < 0.00001f
            && std::fabs(fnvxr::pipboy::sourceVFromPanel(0.0f) - crop.top) < 0.00001f
            && std::fabs(fnvxr::pipboy::sourceVFromPanel(1.0f) - crop.bottom) < 0.00001f,
        "Pip-Boy pointer mapping diverged from its visible crop") && passed;
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
