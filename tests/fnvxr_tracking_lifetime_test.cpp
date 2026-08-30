#include "../host/fnvxr_tracking_lifetime.h"

#include <cstdlib>
#include <iostream>

int main()
{
    fnvxr::host::TrackingLifetime lifetime;
    if (lifetime.generation() != 1 || lifetime.advance(100).activated)
    {
        std::cerr << "initial tracking lifetime is invalid\n";
        return EXIT_FAILURE;
    }
    lifetime.scheduleReferenceSpaceChange(200);
    if (lifetime.advance(199).activated || !lifetime.pending())
    {
        std::cerr << "reference-space change activated too early\n";
        return EXIT_FAILURE;
    }
    const auto changed = lifetime.advance(200);
    if (!changed.activated || changed.generation != 2
        || changed.scheduledTime != 200 || lifetime.pending())
    {
        std::cerr << "reference-space change did not activate exactly\n";
        return EXIT_FAILURE;
    }
    lifetime.scheduleReferenceSpaceChange(500);
    const auto resumed = lifetime.sessionRegained();
    if (!resumed.activated || resumed.generation != 3 || lifetime.pending())
    {
        std::cerr << "session resume did not establish a fresh generation\n";
        return EXIT_FAILURE;
    }
    lifetime.scheduleReferenceSpaceChange(0);
    if (!lifetime.advance(1).activated || lifetime.generation() != 4)
    {
        std::cerr << "immediate reference-space change was not activated\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
