#pragma once
#include <windows.h>
#include <cstring>
#include "../kernel/haptics.h"

namespace fnvxr::shared
{
// Independent lanes prevent native motor updates from cancelling the brief
// acknowledgement of a successfully dispatched native menu click.
enum class HapticChannel : unsigned { NativeMotors, MenuClick };
#pragma pack(push, 4)
struct HapticLane { volatile LONG sequence; kernel::haptics::Feedback feedback; };
struct SharedHapticFeedback
{
    std::uint32_t magic;
    std::uint32_t version;
    std::uint64_t hostEpoch;
    HapticLane channels[2];
};
#pragma pack(pop)
static_assert(sizeof(kernel::haptics::Feedback) == 24);
static_assert(sizeof(SharedHapticFeedback) == 72);

class HapticBridge
{
    HANDLE mapping_ = nullptr;
    SharedHapticFeedback* state_ = nullptr;
    bool valid() const noexcept
    { return state_ && state_->magic == 0x484E5646u && state_->version == 1 && state_->hostEpoch != 0; }
public:
    HapticBridge() = default;
    HapticBridge(const HapticBridge&) = delete;
    HapticBridge& operator=(const HapticBridge&) = delete;
    ~HapticBridge()
    {
        if (state_) UnmapViewOfFile(state_);
        if (mapping_) CloseHandle(mapping_);
    }
    bool create(std::uint64_t epoch) noexcept
    {
        mapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
            0, sizeof(SharedHapticFeedback), L"Local\\FNVXR_Haptic_State");
        if (!mapping_) return false;
        state_ = static_cast<SharedHapticFeedback*>(MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS,
            0, 0, sizeof(SharedHapticFeedback)));
        if (!state_) return false;
        std::memset(state_, 0, sizeof(*state_));
        state_->version = 1;
        state_->hostEpoch = epoch;
        MemoryBarrier();
        state_->magic = 0x484E5646u;
        return valid();
    }
    bool publish(HapticChannel channel, float left, float right, std::uint32_t duration) noexcept
    {
        if (!state_)
        {
            if (!mapping_) mapping_ = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"Local\\FNVXR_Haptic_State");
            if (!mapping_) return false;
            state_ = static_cast<SharedHapticFeedback*>(MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS,
                0, 0, sizeof(SharedHapticFeedback)));
        }
        if (!valid()) return false;
        auto& lane = state_->channels[static_cast<unsigned>(channel)];
        const LONG sequence = lane.sequence;
        if ((sequence & 1) || InterlockedCompareExchange(&lane.sequence,
            static_cast<LONG>(static_cast<unsigned long>(sequence) + 1u), sequence) != sequence) return false;
        lane.feedback = { GetTickCount64(), GetCurrentProcessId(), duration, left, right };
        MemoryBarrier();
        InterlockedExchange(&lane.sequence, static_cast<LONG>(static_cast<unsigned long>(sequence) + 2u));
        return true;
    }
    kernel::haptics::Feedback read(HapticChannel channel) const noexcept
    {
        if (!valid()) return {};
        const auto& lane = state_->channels[static_cast<unsigned>(channel)];
        const LONG sequence = lane.sequence;
        if (sequence == 0 || (sequence & 1)) return {};
        MemoryBarrier();
        const auto value = lane.feedback;
        MemoryBarrier();
        return sequence == lane.sequence ? value : kernel::haptics::Feedback{};
    }
};

inline bool publishHapticFeedback(HapticChannel channel, float left, float right, std::uint32_t duration) noexcept
{
    static HapticBridge bridge;
    return bridge.publish(channel, left, right, duration);
}
}
