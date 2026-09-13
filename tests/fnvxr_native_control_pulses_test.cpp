#include "fnvxr_native_control_pulses.h"
#include "fnvxr_native_movie_input.h"
#include <iostream>

int main()
{
    fnvxr::input::NativeControlPulses pulses;
    int failures = 0;
    const auto expect = [&](bool value, const char* name) {
        if (!value) { std::cerr << name << '\n'; ++failures; }
    };
    fnvxr::input::NativeMovieSkipInput movie;
    expect(!movie.sample(1000, true, true), "Opening confirm cannot skip a movie");
    expect(!movie.sample(1010, true, false), "Release arms native movie input");
    expect(movie.sample(1020, true, true), "A new controller press skips");
    expect(!movie.sample(1030, true, true), "Held skip cannot repeat");
    expect(!movie.sample(1040, true, false), "Release rearms");
    expect(!movie.sample(2000, true, true), "A new movie requires a fresh release");
    expect(!movie.sample(2010, false, false), "Disconnected state cannot arm");
    expect(!movie.sample(2020, true, true), "Reconnect with button held cannot skip");
    expect(!movie.sample(2030, true, false), "Connected neutral rearms");
    expect(!movie.sample(1500, true, true), "Clock rollback cannot reuse a press");
    pulses.beginFrame();
    expect(!pulses.publish(5, 1000), "An unauthorized action cannot be queued");
    pulses.authorize(true);
    expect(pulses.publish(5, 1000), "Activate is queued on the game thread");
    expect(pulses.sample(5, 1, 1010) && pulses.sample(5, 1, 1011),
        "Repeated native readers see the same press edge");
    expect(pulses.sample(5, 0, 1010) && !pulses.sample(5, 2, 1010),
        "The press frame is held and has no release edge");
    expect(!pulses.sample(12, 1, 1010), "An action cannot press another control");
    pulses.beginFrame();
    expect(!pulses.sample(5, 2, 1020), "Each update renews input authority");
    pulses.authorize(true);
    expect(pulses.sample(5, 2, 1020) && pulses.sample(5, 3, 1020)
        && !pulses.sample(5, 0, 1020) && !pulses.sample(5, 1, 1020),
        "The next update releases once without repeating activation");
    pulses.beginFrame(); pulses.authorize(true);
    expect(!pulses.sample(5, 2, 1030), "A release does not repeat indefinitely");
    expect(pulses.publish(12, 1040), "Jump uses the same delivery semantics");
    expect(!pulses.sample(12, 1, 1039) && !pulses.sample(12, 1, 1166),
        "Clock reversal and a stalled game reject an old press");
    pulses.authorize(false); pulses.authorize(true);
    expect(!pulses.sample(12, 1, 1041), "A menu or disconnect clears pending input");
    expect(!pulses.publish(28, 1042) && !pulses.sample(1000, 1, 1042),
        "Invalid control IDs stay outside the action array");
    pulses.beginFrame(); pulses.authorize(true);
    expect(pulses.hold(4, true, 2000) && pulses.sample(4, 0, 2001)
        && pulses.sample(4, 1, 2001) && pulses.sample(4, 3, 2001),
        "Trigger down publishes a held control and one native press edge");
    pulses.hold(4, true, 2002);
    expect(pulses.sample(4, 1, 2003), "Repeated publication preserves this frame's edge");
    pulses.beginFrame(); pulses.authorize(true); pulses.hold(4, true, 2010);
    expect(pulses.sample(4, 0, 2011) && !pulses.sample(4, 1, 2011)
        && !pulses.sample(4, 2, 2011) && !pulses.sample(4, 3, 2011),
        "Holding a trigger does not synthesize repeated semi-automatic presses");
    pulses.beginFrame(); pulses.authorize(true); pulses.hold(4, false, 2020);
    expect(!pulses.sample(4, 0, 2021) && pulses.sample(4, 2, 2021)
        && pulses.sample(4, 2, 2022), "Every native reader sees the release frame");
    pulses.beginFrame(); pulses.authorize(true); pulses.hold(4, false, 2030);
    expect(!pulses.sample(4, 2, 2031), "Release expires on the next game update");
    pulses.hold(7, true, 2040);
    expect(!pulses.sample(7, 0, 2039) && !pulses.sample(7, 0, 2166),
        "Held controls reject reversed clocks and stalled input");
    pulses.authorize(false); pulses.authorize(true);
    expect(!pulses.sample(7, 0, 2041), "Opening a menu clears reload/holster holds");
    fnvxr::input::NativeMenuKeyPulses menuKeys;
    menuKeys.beginFrame(); menuKeys.authorize(true);
    expect(menuKeys.publish(0x12, 3000) && menuKeys.sample(0x12, 1, 3001),
        "VATS confirm reaches a native polled key without gameplay authority");
    expect(!pulses.sample(5, 1, 3001), "Menu accept cannot leak into gameplay Activate");
    expect(menuKeys.publish(0xcd, 3000) && !menuKeys.publish(256, 3000),
        "Extended arrow keys remain valid but invalid scancodes are rejected");
    menuKeys.authorize(false); menuKeys.authorize(true);
    expect(!menuKeys.sample(0x12, 1, 3001), "Changing the top menu discards pending confirmation");
    fnvxr::input::DeferredMenuBack back;
    expect(back.queue(0x1000, 4000) && !back.consume(0x1000, false, 4100)
        && !back.consume(0x1000, false, 4500) && back.consume(0x1000, true, 4600)
        && !back.consume(0x1000, true, 4601),
        "One Back press survives target zoom and is consumed once when native input is ready");
    back.queue(0x1000, 5000);
    expect(!back.consume(0x2000, true, 5010) && !back.consume(0x1000, true, 5011),
        "A pending Back cannot dismiss another menu or a later reuse of the old address");
    back.queue(0x1000, 6000);
    expect(!back.consume(0x1000, true, 8001), "A stalled transition expires Back");
    back.queue(0x1000, 9000);
    expect(!back.consume(0x1000, true, 8999), "Clock reversal cannot replay Back");
    back.queue(0x1000, 10000); back.reset();
    expect(!back.consume(0x1000, true, 10001) && !back.queue(0, 10002),
        "Disconnect and absent menu discard deferred input");
    fnvxr::input::GripChordLatch grip;
    expect(!grip.standalone(true, true) && !grip.standalone(true, false),
        "Releasing the VATS trigger before its grip cannot open Pause");
    expect(!grip.standalone(false, false) && grip.standalone(true, false),
        "A new standalone grip rearms after release");
    expect(!grip.standalone(true, true) && !grip.standalone(false, true)
        && !grip.standalone(false, false) && grip.standalone(true, false),
        "Releasing the grip before the trigger also rearms correctly");
    fnvxr::input::NativeMenuAnalogMotion motion45, motion90;
    motion45.step(1.0f, 0); motion90.step(1.0f, 0);
    int total45 = 0, total90 = 0;
    for (unsigned frame = 1; frame <= 45; ++frame)
        total45 += motion45.step(1.0f, frame * 1000u / 45u);
    for (unsigned frame = 1; frame <= 90; ++frame)
        total90 += motion90.step(1.0f, frame * 1000u / 90u);
    expect(std::abs(total45 - 600) <= 1 && std::abs(total90 - total45) <= 1,
        "Lockpick motion travels the same distance at 45 and 90 updates per second");
    expect(motion45.step(1.0f, 2000) == 0 && motion45.step(1.0f, 1990) == 0,
        "A pause or historical timestamp cannot spin the pick on resume");
    expect(motion45.step(0.1f, 2010) == 0 && motion45.step(-1.0f, 2030) < 0,
        "Stick drift stays still and left motion reverses the pick");
    motion45.reset();
    expect(motion45.step(-1.0f, 2040) == 0,
        "Changing menus resets relative motion without carrying an old delta");
    return failures ? 1 : 0;
}
