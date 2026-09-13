#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fnvxr::host::mirror
{
struct Image
{
    std::wstring path;
    std::uint32_t width = 0, height = 0, rowPitch = 0, format = 0;
    bool rgba = false;
    bool jpeg = false;
    std::vector<std::uint8_t> pixels;
};
struct Pair
{
    std::uint64_t frame = 0;
    std::uint32_t ordinal = 0;
    std::array<Image, 2> eyes;
    std::array<std::uint32_t, 2> hashes {};
};
struct Result
{
    std::uint64_t frame = 0;
    std::uint32_t ordinal = 0, width = 0, height = 0, format = 0;
    std::array<std::int32_t, 2> status {};
    std::array<std::uint32_t, 2> hashes {};
};

// No graphics/context pointers cross this boundary. The render thread copies
// mapped bytes, then continues. Image encoding and disk writes own a bounded
// queue of whole pairs, with completion reported back on the host thread.
class Writer final
{
public:
    Writer();
    ~Writer();
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;
    bool submit(Pair&& pair);
    std::vector<Result> takeCompleted();
    void finish();
private:
    struct State;
    std::unique_ptr<State> state_;
};
}
