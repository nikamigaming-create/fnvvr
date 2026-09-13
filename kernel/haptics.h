#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace fnvxr::kernel::haptics
{
struct Feedback
{
    std::uint64_t issuedMilliseconds = 0;
    std::uint32_t producerProcessId = 0;
    std::uint32_t durationMilliseconds = 0;
    float left = 0;
    float right = 0;
};

inline float amplitude(const Feedback& feedback, std::uint64_t now,
    std::uint32_t expectedProducer, bool right, bool tracked, bool inputActive) noexcept
{
    if (!tracked || !inputActive || expectedProducer == 0
        || feedback.producerProcessId != expectedProducer
        || feedback.issuedMilliseconds == 0 || now < feedback.issuedMilliseconds
        || now - feedback.issuedMilliseconds >= (std::min)(feedback.durationMilliseconds, 250u))
        return 0;
    const float value = right ? feedback.right : feedback.left;
    return std::isfinite(value) ? (std::clamp)(value, 0.0f, 0.75f) : 0.0f;
}
}
