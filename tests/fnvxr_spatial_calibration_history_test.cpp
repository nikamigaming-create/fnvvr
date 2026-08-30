#include "../kernel/spatial_calibration_history.h"

#include <cstdlib>
#include <iostream>

namespace
{
struct Calibration final
{
    int value = 0;
};

int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}
}

int main()
{
    using namespace fnvxr::kernel;
    SpatialCalibrationHistory<Calibration, 4> history;
    Calibration found {};

    const SpatialCalibrationIdentity first { 7, 3, 1 };
    if (!history.record(first, { 11 })
        || !history.find(first, found) || found.value != 11)
        return fail("exact calibration was not recovered");
    if (history.find({ 8, 3, 1 }, found)
        || history.find({ 7, 4, 1 }, found)
        || history.find({ 7, 3, 2 }, found))
        return fail("non-exact calibration identity was accepted");

    if (!history.record({ 7, 3, 5 }, { 55 }))
        return fail("colliding calibration was not recorded");
    if (history.find(first, found)
        || !history.find({ 7, 3, 5 }, found) || found.value != 55)
        return fail("bounded slot collision did not fail closed");

    if (history.record({}, { 1 }) || history.find({}, found))
        return fail("invalid calibration identity was accepted");
    history.clear();
    if (history.find({ 7, 3, 5 }, found))
        return fail("history clear retained a calibration");
    return EXIT_SUCCESS;
}
