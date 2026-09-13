#pragma once
#include <array>
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace fnvxr::input
{
// Game-thread action pulses use retail control IDs, independently of keyboard
// acquisition and user key bindings. Readers may query a state many times in
// one update; a query must not consume another reader's edge.
template<std::size_t Count>
class NativeInputPulses
{
public:
    void beginFrame() noexcept { ++frame_; authorized_ = false; }
    void authorize(bool value) noexcept
    {
        authorized_ = value;
        if (!value) { pulses_ = {}; holds_ = {}; }
    }
    bool hold(std::uint32_t control, bool held, std::uint64_t nowMs) noexcept
    {
        if (!authorized_ || control >= holds_.size()) return false;
        auto& state = holds_[control];
        if (state.held != held) state.changedFrame = frame_;
        state.frame = frame_;
        state.timeMs = nowMs;
        state.held = held;
        state.valid = true;
        return true;
    }
    bool publish(std::uint32_t control, std::uint64_t nowMs) noexcept
    {
        if (!authorized_ || control >= pulses_.size()) return false;
        pulses_[control] = { frame_, nowMs, true };
        return true;
    }
    bool sample(std::uint32_t control, std::uint32_t state,
        std::uint64_t nowMs) const noexcept
    {
        if (!authorized_ || control >= pulses_.size() || state > 3) return false;
        const auto& held = holds_[control];
        if (held.valid && held.frame == frame_ && nowMs >= held.timeMs
            && nowMs - held.timeMs <= 125)
        {
            const bool changed = held.changedFrame == frame_;
            if (state == 0 ? held.held : state == 1 ? changed && held.held
                : state == 2 ? changed && !held.held : changed)
                return true;
        }
        const auto& pulse = pulses_[control];
        if (!pulse.valid || nowMs < pulse.timeMs || nowMs - pulse.timeMs > 125
            || frame_ < pulse.frame || frame_ - pulse.frame > 1) return false;
        const bool pressed = frame_ == pulse.frame;
        // Retail states: held, pressed, released, changed.
        return state == 3 || (state == 2 ? !pressed : pressed);
    }
private:
    struct Pulse { std::uint64_t frame{}, timeMs{}; bool valid{}; };
    struct Hold { std::uint64_t frame{}, timeMs{}, changedFrame{}; bool held{}, valid{}; };
    std::array<Pulse, Count> pulses_ {};
    std::array<Hold, Count> holds_ {};
    std::uint64_t frame_ {};
    bool authorized_ {};
};
using NativeControlPulses = NativeInputPulses<28>;
using NativeMenuKeyPulses = NativeInputPulses<256>;

// Some native menus ignore Back during their own transition. Retain one edge
// for that same menu, then consume it once at the native input reader.
class DeferredMenuBack
{
public:
    bool queue(std::uintptr_t owner, std::uint64_t nowMs) noexcept
    {
        if (!owner) return false;
        owner_ = owner; timeMs_ = nowMs;
        return true;
    }
    void reset() noexcept { owner_ = 0; }
    bool consume(std::uintptr_t owner, bool ready, std::uint64_t nowMs) noexcept
    {
        if (!owner_ || owner != owner_ || nowMs < timeMs_
            || nowMs - timeMs_ > 2000u)
        { reset(); return false; }
        if (!ready) return false;
        reset();
        return true;
    }
private:
    std::uintptr_t owner_ {};
    std::uint64_t timeMs_ {};
};

// Convert a held VR stick to relative native-menu motion without making
// minigame speed depend on the engine's render rate.
class NativeMenuAnalogMotion
{
public:
    void reset() noexcept { lastMs_ = 0; remainder_ = 0.0f; initialized_ = false; }
    int step(float axis, std::uint64_t nowMs) noexcept
    {
        if (!std::isfinite(axis)) { reset(); return 0; }
        if (!initialized_) { lastMs_ = nowMs; initialized_ = true; return 0; }
        if (nowMs <= lastMs_) return 0;
        const auto elapsed = nowMs - lastMs_;
        lastMs_ = nowMs;
        if (elapsed > 125u) { remainder_ = 0.0f; return 0; }
        const float magnitude = (std::min)(1.0f, std::fabs(axis));
        if (magnitude <= 0.18f) { remainder_ = 0.0f; return 0; }
        const float value = std::copysign((magnitude - 0.18f) / 0.82f, axis);
        remainder_ += value * 600.0f * static_cast<float>(elapsed) / 1000.0f;
        const int delta = static_cast<int>(remainder_);
        remainder_ -= static_cast<float>(delta);
        return delta;
    }
private:
    std::uint64_t lastMs_ {};
    float remainder_ {};
    bool initialized_ {};
};

// A grip used as part of a trigger chord cannot become a standalone menu
// press when the trigger is released first. Rearm only after grip release.
class GripChordLatch
{
public:
    bool standalone(bool grip, bool modifier) noexcept
    {
        if (!grip) consumed_ = false;
        else if (modifier) consumed_ = true;
        return grip && !consumed_;
    }
private:
    bool consumed_ {};
};
}
