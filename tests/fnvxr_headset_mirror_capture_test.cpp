#include "../host/fnvxr_headset_mirror_capture.h"

#include <cstdlib>
#include <iostream>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
}

int main()
{
    using namespace fnvxr::host::headset_mirror;

    ScheduleState state {};
    require(
        !schedule(state, { false, 10u, 2u, 3u }).capture,
        "disabled headset mirror capture was scheduled");
    require(
        !schedule(state, { true, 0u, 2u, 3u }).capture,
        "zero OpenXR frame scheduled a headset mirror capture");
    require(
        !schedule(state, { true, 10u, 0u, 3u }).capture,
        "zero cadence scheduled a headset mirror capture");

    const ScheduleDecision first = schedule(state, { true, 10u, 3u, 2u });
    require(first.capture && first.ordinal == 1u, "first eligible stereo pair was not ordinal one");
    const ScheduleDecision repeated = schedule(state, { true, 10u, 3u, 2u });
    require(
        repeated.capture && repeated.ordinal == first.ordinal && state.scheduledPairs == 1u,
        "the second eye consumed a separate headset-mirror pair");
    require(
        !schedule(state, { true, 11u, 3u, 2u }).capture,
        "capture cadence admitted an in-between OpenXR frame");
    const ScheduleDecision second = schedule(state, { true, 13u, 3u, 2u });
    require(second.capture && second.ordinal == 2u, "second eligible stereo pair lost its ordinal");
    require(
        !schedule(state, { true, 16u, 3u, 2u }).capture,
        "bounded headset mirror capture exceeded its pair budget");

    std::cout << "headset mirror capture schedule passed\n";
    return 0;
}
