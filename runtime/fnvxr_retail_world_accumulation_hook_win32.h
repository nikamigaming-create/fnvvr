#pragma once

#include "fnvxr_retail_first_person_hook_lease.h"
#include "fnvxr_retail_world_accumulation_hook_lease.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace fnvxr::engine
{
#if defined(_WIN32) && defined(_M_IX86)
inline constexpr bool RetailWorldAccumulationHookWin32MemoryAvailable = true;
#else
inline constexpr bool RetailWorldAccumulationHookWin32MemoryAvailable = false;
#endif

// Current-process memory backend for the accumulation lifecycle callsite
// lease. It cannot allocate executable memory and accepts a write only when
// the exact address is one of the audited five-byte E8 instructions.
class RetailWorldAccumulationHookWin32Memory final
{
public:
    RetailWorldAccumulationHookWin32Memory() noexcept = default;
    ~RetailWorldAccumulationHookWin32Memory() noexcept;

    RetailWorldAccumulationHookWin32Memory(
        const RetailWorldAccumulationHookWin32Memory&) = delete;
    RetailWorldAccumulationHookWin32Memory& operator=(
        const RetailWorldAccumulationHookWin32Memory&) = delete;
    RetailWorldAccumulationHookWin32Memory(
        RetailWorldAccumulationHookWin32Memory&&) = delete;
    RetailWorldAccumulationHookWin32Memory& operator=(
        RetailWorldAccumulationHookWin32Memory&&) = delete;

    bool initialize(std::uint32_t authorizedWorldAddress) noexcept;
    bool ready() const noexcept;
    RetailWorldAccumulationHookMemoryOperations operations() noexcept;

private:
    static bool read(
        void*,
        std::uint32_t,
        std::uint8_t*,
        std::size_t) noexcept;
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

    bool rangeIsCallSite(
        std::uint32_t address,
        std::size_t byteCount) const noexcept;
    void abandonOwnedState() noexcept;

    std::uint32_t mWorldAddress = 0u;
    std::array<
        std::uint32_t,
        RetailWorldAccumulationCallSiteContractInventory.size()>
        mCallSiteAddresses {};
    std::uint32_t mWritableAddress = 0u;
    std::uintptr_t mProtectionToken = 0u;
    std::uint32_t mOriginalProtection = 0u;
    std::uint64_t mNextToken = 1u;
    bool mInitialized = false;
};

// A separate current-process writer for the exact RenderFirstPerson E8
// callsites.  Sharing the low-level Win32 mechanics with the world lease is
// not permission to share its address allow-list: this object accepts only
// the three independently sampled first-person call instructions.
class RetailFirstPersonHookWin32Memory final
{
public:
    RetailFirstPersonHookWin32Memory() noexcept = default;
    ~RetailFirstPersonHookWin32Memory() noexcept;

    RetailFirstPersonHookWin32Memory(
        const RetailFirstPersonHookWin32Memory&) = delete;
    RetailFirstPersonHookWin32Memory& operator=(
        const RetailFirstPersonHookWin32Memory&) = delete;
    RetailFirstPersonHookWin32Memory(
        RetailFirstPersonHookWin32Memory&&) = delete;
    RetailFirstPersonHookWin32Memory& operator=(
        RetailFirstPersonHookWin32Memory&&) = delete;

    bool initialize(std::uint32_t authorizedImageBase) noexcept;
    bool ready() const noexcept;
    RetailWorldAccumulationHookMemoryOperations operations() noexcept;

private:
    static bool read(
        void*,
        std::uint32_t,
        std::uint8_t*,
        std::size_t) noexcept;
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

    bool rangeIsCallSite(
        std::uint32_t address,
        std::size_t byteCount) const noexcept;
    void abandonOwnedState() noexcept;

    std::uint32_t mImageBase = 0u;
    std::array<
        std::uint32_t,
        RetailFirstPersonCallSiteContractInventory.size()>
        mCallSiteAddresses {};
    std::uint32_t mWritableAddress = 0u;
    std::uintptr_t mProtectionToken = 0u;
    std::uint32_t mOriginalProtection = 0u;
    std::uint64_t mNextToken = 1u;
    bool mInitialized = false;
};
}
