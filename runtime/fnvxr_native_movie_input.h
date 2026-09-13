#pragma once
#include <cstdint>

namespace fnvxr::input
{
// Movie polling has no MainGameLoop ticks. Require a neutral controller in
// this polling lifetime, then one press, without reusing the opening click.
class NativeMovieSkipInput final
{
public:
    bool sample(std::uint64_t now, bool connected, bool down) noexcept
    {
        if (!connected || !mLastPoll || now < mLastPoll || now - mLastPoll > 500u)
            mArmed = false;
        mLastPoll = now;
        if (!connected) return false;
        if (!down) { mArmed = true; return false; }
        const bool pressed = mArmed;
        mArmed = false;
        return pressed;
    }
private:
    std::uint64_t mLastPoll = 0;
    bool mArmed = false;
};
}
