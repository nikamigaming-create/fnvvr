#include "../kernel/haptics.h"
#include <iostream>
#include <limits>

int main()
{
    using namespace fnvxr::kernel::haptics;
    int failures = 0;
    auto expect = [&](bool ok, const char* message) { if (!ok) { ++failures; std::cerr << message << '\n'; } };
    Feedback event{1000, 42, 200, 0.4f, 0.9f};
    expect(amplitude(event, 1050, 42, false, true, true) == 0.4f, "left native motor lost");
    expect(amplitude(event, 1050, 42, true, true, true) == 0.75f, "right intensity cap lost");
    expect(amplitude(event, 1200, 42, false, true, true) == 0, "expired motor held");
    expect(amplitude(event, 999, 42, false, true, true) == 0, "future event accepted");
    expect(amplitude(event, 1050, 43, false, true, true) == 0, "another process supplied feedback");
    expect(amplitude(event, 1050, 42, false, false, true) == 0, "untracked hand vibrated");
    expect(amplitude(event, 1050, 42, false, true, false) == 0, "unfocused session vibrated");
    event.durationMilliseconds = 10000;
    expect(amplitude(event, 1250, 42, false, true, true) == 0, "native event escaped bounded duration");
    event.left = std::numeric_limits<float>::quiet_NaN();
    expect(amplitude(event, 1050, 42, false, true, true) == 0, "invalid intensity accepted");
    return failures ? 1 : 0;
}
