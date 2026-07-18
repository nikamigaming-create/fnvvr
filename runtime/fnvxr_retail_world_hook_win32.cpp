#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "fnvxr_retail_world_hook_win32.h"

#include <limits>

namespace fnvxr::engine
{
namespace
{
bool checkedRangeEnd(
    std::uint32_t address,
    std::size_t byteCount,
    std::uint64_t& end) noexcept
{
    if (address == 0u || byteCount == 0u)
        return false;
    end = static_cast<std::uint64_t>(address)
        + static_cast<std::uint64_t>(byteCount);
    return end <= 0x100000000ull;
}

bool readableExecutableRange(
    std::uint32_t address,
    std::size_t byteCount,
    bool permitWritable) noexcept
{
    std::uint64_t requestedEnd = 0u;
    if (!checkedRangeEnd(address, byteCount, requestedEnd))
        return false;

    std::uintptr_t cursor = address;
    while (cursor < requestedEnd)
    {
        MEMORY_BASIC_INFORMATION information {};
        if (VirtualQuery(
                reinterpret_cast<const void*>(
                    static_cast<std::uintptr_t>(cursor)),
                &information,
                sizeof(information))
                != sizeof(information)
            || information.State != MEM_COMMIT
            || (information.Protect & PAGE_GUARD) != 0u
            || (information.Protect & PAGE_NOACCESS) != 0u)
        {
            return false;
        }
        const DWORD access = information.Protect & 0xFFu;
        const bool executable = access == PAGE_EXECUTE
            || access == PAGE_EXECUTE_READ
            || access == PAGE_EXECUTE_READWRITE
            || access == PAGE_EXECUTE_WRITECOPY;
        const bool writable = access == PAGE_READWRITE
            || access == PAGE_WRITECOPY
            || access == PAGE_EXECUTE_READWRITE
            || access == PAGE_EXECUTE_WRITECOPY;
        if (!executable || (!permitWritable && writable))
            return false;

        const std::uintptr_t regionBase = reinterpret_cast<std::uintptr_t>(
            information.BaseAddress);
        if (regionBase > cursor
            || information.RegionSize
                > (std::numeric_limits<std::uintptr_t>::max)() - regionBase)
        {
            return false;
        }
        const std::uintptr_t regionEnd = regionBase + information.RegionSize;
        if (regionEnd <= cursor)
            return false;
        cursor = regionEnd < requestedEnd
            ? regionEnd
            : static_cast<std::uintptr_t>(requestedEnd);
    }
    return true;
}
}

RetailWorldHookWin32Memory::~RetailWorldHookWin32Memory() noexcept
{
    // Correct ownership destroys the hook lease before this adapter. Restore
    // an abandoned writable page, but never free an outstanding trampoline
    // here: an installed entry could still reach it, and a small leak is safer
    // than turning incorrect destruction order into executable use-after-free.
    if (mWorldWritable && mWorldAddress != 0u && mOriginalProtection != 0u)
    {
        DWORD ignored = 0u;
        (void)VirtualProtect(
            reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(mWorldAddress)),
            RetailWorldHookPatchByteCount,
            mOriginalProtection,
            &ignored);
    }
    abandonOwnedState();
}

bool RetailWorldHookWin32Memory::initialize(
    std::uint32_t authorizedWorldAddress) noexcept
{
    if constexpr (!RetailWorldHookWin32MemoryAvailable)
    {
        (void)authorizedWorldAddress;
        return false;
    }
    else
    {
        if (mInitialized
            || mTrampoline
            || mWorldWritable
            || authorizedWorldAddress == 0u
            || !readableExecutableRange(
                authorizedWorldAddress,
                RetailWorldFunctionByteCount,
                false))
        {
            return false;
        }
        mWorldAddress = authorizedWorldAddress;
        mNextToken = 1u;
        mInitialized = true;
        return true;
    }
}

bool RetailWorldHookWin32Memory::ready() const noexcept
{
    return RetailWorldHookWin32MemoryAvailable
        && mInitialized
        && mWorldAddress != 0u;
}

RetailWorldHookMemoryOperations
RetailWorldHookWin32Memory::operations() noexcept
{
    if (!ready())
        return {};
    return {
        this,
        &read,
        &allocateExecutable,
        &releaseExecutable,
        &makeWritable,
        &restoreProtection,
        &write,
        &flushInstructionCache,
    };
}

bool RetailWorldHookWin32Memory::rangeIsWorldRead(
    std::uint32_t address,
    std::size_t byteCount) const noexcept
{
    std::uint64_t end = 0u;
    return ready()
        && checkedRangeEnd(address, byteCount, end)
        && address >= mWorldAddress
        && end <= static_cast<std::uint64_t>(mWorldAddress)
            + RetailWorldFunctionByteCount;
}

bool RetailWorldHookWin32Memory::rangeIsWorldEntry(
    std::uint32_t address,
    std::size_t byteCount) const noexcept
{
    return ready()
        && address == mWorldAddress
        && byteCount == RetailWorldHookPatchByteCount;
}

bool RetailWorldHookWin32Memory::rangeIsTrampoline(
    std::uint32_t address,
    std::size_t byteCount) const noexcept
{
    return mTrampoline
        && mTrampolineToken != 0u
        && address == static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(mTrampoline))
        && byteCount == RetailWorldHookTrampolineByteCount;
}

bool RetailWorldHookWin32Memory::read(
    void* opaque,
    std::uint32_t address,
    std::uint8_t* destination,
    std::size_t byteCount) noexcept
{
    auto* state = static_cast<RetailWorldHookWin32Memory*>(opaque);
    if (!state || !destination
        || (!state->rangeIsWorldRead(address, byteCount)
            && !state->rangeIsTrampoline(address, byteCount)))
    {
        return false;
    }
    SIZE_T transferred = 0u;
    return ReadProcessMemory(
            GetCurrentProcess(),
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(address)),
            destination,
            byteCount,
            &transferred)
        && transferred == byteCount;
}

RetailWorldHookExecutableAllocation
RetailWorldHookWin32Memory::allocateExecutable(
    void* opaque,
    std::size_t byteCount) noexcept
{
    auto* state = static_cast<RetailWorldHookWin32Memory*>(opaque);
    if (!state
        || !state->ready()
        || state->mTrampoline
        || byteCount != RetailWorldHookTrampolineByteCount)
    {
        return {};
    }
    void* allocation = VirtualAlloc(
        nullptr,
        byteCount,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE);
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(allocation);
    if (!allocation
        || address == 0u
        || address > (std::numeric_limits<std::uint32_t>::max)())
    {
        if (allocation)
            (void)VirtualFree(allocation, 0u, MEM_RELEASE);
        return {};
    }
    const std::uintptr_t token = static_cast<std::uintptr_t>(
        state->mNextToken++);
    if (token == 0u || state->mNextToken == 0u)
    {
        (void)VirtualFree(allocation, 0u, MEM_RELEASE);
        return {};
    }
    state->mTrampoline = allocation;
    state->mTrampolineToken = token;
    return {
        true,
        static_cast<std::uint32_t>(address),
        token,
    };
}

bool RetailWorldHookWin32Memory::releaseExecutable(
    void* opaque,
    const RetailWorldHookExecutableAllocation& allocation) noexcept
{
    auto* state = static_cast<RetailWorldHookWin32Memory*>(opaque);
    if (!state
        || !state->mTrampoline
        || !allocation.ownershipTransferred
        || allocation.address != static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(state->mTrampoline))
        || allocation.cleanupToken != state->mTrampolineToken)
    {
        return false;
    }
    if (!VirtualFree(state->mTrampoline, 0u, MEM_RELEASE))
        return false;
    state->mTrampoline = nullptr;
    state->mTrampolineToken = 0u;
    return true;
}

RetailWorldHookProtectionLease RetailWorldHookWin32Memory::makeWritable(
    void* opaque,
    std::uint32_t address,
    std::size_t byteCount) noexcept
{
    auto* state = static_cast<RetailWorldHookWin32Memory*>(opaque);
    if (!state
        || !state->rangeIsWorldEntry(address, byteCount)
        || state->mWorldWritable
        || state->mProtectionToken != 0u)
    {
        return {};
    }
    DWORD oldProtection = 0u;
    if (!VirtualProtect(
            reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(address)),
            byteCount,
            PAGE_EXECUTE_READWRITE,
            &oldProtection)
        || oldProtection == 0u)
    {
        return {};
    }
    const std::uintptr_t token = static_cast<std::uintptr_t>(
        state->mNextToken++);
    if (token == 0u || state->mNextToken == 0u)
    {
        DWORD ignored = 0u;
        (void)VirtualProtect(
            reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(address)),
            byteCount,
            oldProtection,
            &ignored);
        return {};
    }
    state->mOriginalProtection = oldProtection;
    state->mProtectionToken = token;
    state->mWorldWritable = true;
    return { true, token };
}

bool RetailWorldHookWin32Memory::restoreProtection(
    void* opaque,
    const RetailWorldHookProtectionLease& lease) noexcept
{
    auto* state = static_cast<RetailWorldHookWin32Memory*>(opaque);
    if (!state
        || !state->mWorldWritable
        || !lease.ownershipTransferred
        || lease.restoreToken == 0u
        || lease.restoreToken != state->mProtectionToken
        || state->mOriginalProtection == 0u)
    {
        return false;
    }
    DWORD ignored = 0u;
    if (!VirtualProtect(
            reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(state->mWorldAddress)),
            RetailWorldHookPatchByteCount,
            state->mOriginalProtection,
            &ignored))
    {
        return false;
    }
    state->mWorldWritable = false;
    state->mProtectionToken = 0u;
    state->mOriginalProtection = 0u;
    return true;
}

bool RetailWorldHookWin32Memory::write(
    void* opaque,
    std::uint32_t address,
    const std::uint8_t* source,
    std::size_t byteCount) noexcept
{
    auto* state = static_cast<RetailWorldHookWin32Memory*>(opaque);
    const bool worldWrite = state
        && state->mWorldWritable
        && state->rangeIsWorldEntry(address, byteCount);
    const bool trampolineWrite = state
        && state->rangeIsTrampoline(address, byteCount);
    if (!source || (!worldWrite && !trampolineWrite))
        return false;
    SIZE_T transferred = 0u;
    return WriteProcessMemory(
            GetCurrentProcess(),
            reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(address)),
            source,
            byteCount,
            &transferred)
        && transferred == byteCount;
}

bool RetailWorldHookWin32Memory::flushInstructionCache(
    void* opaque,
    std::uint32_t address,
    std::size_t byteCount) noexcept
{
    auto* state = static_cast<RetailWorldHookWin32Memory*>(opaque);
    if (!state
        || (!state->rangeIsWorldEntry(address, byteCount)
            && !state->rangeIsTrampoline(address, byteCount)))
    {
        return false;
    }
    return FlushInstructionCache(
        GetCurrentProcess(),
        reinterpret_cast<const void*>(
            static_cast<std::uintptr_t>(address)),
        byteCount) != FALSE;
}

void RetailWorldHookWin32Memory::abandonOwnedState() noexcept
{
    mWorldAddress = 0u;
    mTrampoline = nullptr;
    mTrampolineToken = 0u;
    mProtectionToken = 0u;
    mOriginalProtection = 0u;
    mNextToken = 1u;
    mWorldWritable = false;
    mInitialized = false;
}
}
