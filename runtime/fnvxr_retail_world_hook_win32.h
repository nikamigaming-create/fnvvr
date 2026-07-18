#pragma once

#include "fnvxr_retail_world_hook_lease.h"

#include <cstddef>
#include <cstdint>

namespace fnvxr::engine
{
#if defined(_WIN32) && defined(_M_IX86)
inline constexpr bool RetailWorldHookWin32MemoryAvailable = true;
#else
inline constexpr bool RetailWorldHookWin32MemoryAvailable = false;
#endif

// Current-process implementation of the deliberately opaque hook-memory API.
// The object is bound to one exact world-function address. It will read/write
// only that function's five-byte entry or the single trampoline allocation it
// owns; callers cannot use it as a general process-memory primitive.
class RetailWorldHookWin32Memory final
{
public:
    RetailWorldHookWin32Memory() noexcept = default;
    ~RetailWorldHookWin32Memory() noexcept;

    RetailWorldHookWin32Memory(const RetailWorldHookWin32Memory&) = delete;
    RetailWorldHookWin32Memory& operator=(
        const RetailWorldHookWin32Memory&) = delete;
    RetailWorldHookWin32Memory(RetailWorldHookWin32Memory&&) = delete;
    RetailWorldHookWin32Memory& operator=(
        RetailWorldHookWin32Memory&&) = delete;

    bool initialize(std::uint32_t authorizedWorldAddress) noexcept;
    bool ready() const noexcept;
    RetailWorldHookMemoryOperations operations() noexcept;

private:
    static bool read(
        void*,
        std::uint32_t,
        std::uint8_t*,
        std::size_t) noexcept;
    static RetailWorldHookExecutableAllocation allocateExecutable(
        void*,
        std::size_t) noexcept;
    static bool releaseExecutable(
        void*,
        const RetailWorldHookExecutableAllocation&) noexcept;
    static RetailWorldHookProtectionLease makeWritable(
        void*,
        std::uint32_t,
        std::size_t) noexcept;
    static bool restoreProtection(
        void*,
        const RetailWorldHookProtectionLease&) noexcept;
    static bool write(
        void*,
        std::uint32_t,
        const std::uint8_t*,
        std::size_t) noexcept;
    static bool flushInstructionCache(
        void*,
        std::uint32_t,
        std::size_t) noexcept;

    bool rangeIsWorldRead(
        std::uint32_t address,
        std::size_t byteCount) const noexcept;
    bool rangeIsWorldEntry(
        std::uint32_t address,
        std::size_t byteCount) const noexcept;
    bool rangeIsTrampoline(
        std::uint32_t address,
        std::size_t byteCount) const noexcept;
    void abandonOwnedState() noexcept;

    std::uint32_t mWorldAddress = 0u;
    void* mTrampoline = nullptr;
    std::uintptr_t mTrampolineToken = 0u;
    std::uintptr_t mProtectionToken = 0u;
    std::uint32_t mOriginalProtection = 0u;
    std::uint64_t mNextToken = 1u;
    bool mWorldWritable = false;
    bool mInitialized = false;
};

static_assert(!RetailWorldHookWin32MemoryAvailable
    || RetailWorldHookInstallerEmitsX86CodeOnly);
}
