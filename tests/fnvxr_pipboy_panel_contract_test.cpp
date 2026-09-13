#include "../runtime/fnvxr_pipboy_panel_contract.h"

#include <cstdlib>
#include <iostream>

namespace
{
int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}
}

int main()
{
    // Observed retail tab center: source pixel (360, 487.5), native tile
    // (360, 650). Raw hit-testing used to select an inventory row above it.
    const auto widescreen = fnvxr::pipboy::nativeMenuPointFromTexture(
        360.0f, 487.5f, 1280.0f, 720.0f, 1024.0f, 768.0f);
    const auto classic = fnvxr::pipboy::nativeMenuPointFromTexture(
        360.0f, 650.0f, 1024.0f, 768.0f, 1024.0f, 768.0f);
    if (!widescreen.valid || !classic.valid
        || widescreen.x != classic.x || widescreen.y != classic.y
        || widescreen.y != 650.0f)
        return fail("rendered-menu aspect did not resolve the authored tab center");
    if (fnvxr::pipboy::nativeMenuPointFromTexture(
            360.0f, 487.5f, 1280.0f, 720.0f, 0.0f, 768.0f).valid)
        return fail("unavailable native menu dimensions admitted a guessed hit");

    const fnvxr::pipboy::ScreenPixelEvidence authored {
        1'024u, 0.82f, 0.01f, 34.0f,
        2.0f, 56.0f, 48.0f, 5u,
    };
    if (!fnvxr::pipboy::screenContentReady(authored))
        return fail("structured authored screen content was rejected");

    const fnvxr::pipboy::ScreenPixelEvidence uniformDark {
        1'024u, 1.0f, 0.0f, 34.0f,
        34.0f, 34.0f, 0.0f, 1u,
    };
    if (fnvxr::pipboy::screenContentReady(uniformDark))
        return fail("uniform dark placeholder passed content proof");

    auto sparse = authored;
    sparse.nonBlackFraction = 0.02f;
    if (fnvxr::pipboy::screenContentReady(
            sparse, 0.0f, 1.0f, 255.0f))
    {
        return fail("weakened tuning bypassed the non-black invariant");
    }

    auto blueFallback = authored;
    blueFallback.blueDominantFraction = 0.50f;
    if (fnvxr::pipboy::screenContentReady(
            blueFallback, 0.0f, 1.0f, 255.0f))
    {
        return fail("weakened tuning bypassed the blue-fallback invariant");
    }

    auto brightWorld = authored;
    brightWorld.meanLuma = 100.0f;
    if (fnvxr::pipboy::screenContentReady(
            brightWorld, 0.0f, 1.0f, 255.0f))
    {
        return fail("weakened tuning bypassed the world-brightness invariant");
    }

    auto uniformGreen = authored;
    uniformGreen.minimumLuma = 42.0f;
    uniformGreen.maximumLuma = 42.0f;
    uniformGreen.lumaVariance = 0.0f;
    uniformGreen.populatedLumaBins = 1u;
    if (fnvxr::pipboy::screenContentReady(uniformGreen))
        return fail("uniform green placeholder passed content proof");

    return EXIT_SUCCESS;
}
