#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "fnvxr_retail_world_accumulation_hook_win32.h"

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
    std::size_t byteCount) noexcept
{
    std::uint64_t requestedEnd = 0u;
    if (!checkedRangeEnd(address, byteCount, requestedEnd))
        return false;

    std::uintptr_t cursor = address;
    while (cursor < requestedEnd)
    {
        MEMORY_BASIC_INFORMATION information {};
        if (VirtualQuery(
                reinterpret_cast<const void*>(cursor),
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
        if (!executable || writable)
            return false;

        const std::uintptr_t regionBase =
            reinterpret_cast<std::uintptr_t>(information.BaseAddress);
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

RetailWorldAccumulationHookWin32Memory::~RetailWorldAccumulationHookWin32Memory()
    noexcept
{
    // A lease restores its calls before this backend is destroyed. If a
    // failed cleanup left one page writable, restore that page but never
    // manufacture a broad recovery write from the destructor.
    if (mWritableAddress != 0u && mOriginalProtection != 0u)
    {
        DWORD ignored = 0u;
        static_cast<void>(VirtualProtect(
            reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(mWritableAddress)),
            RetailWorldAccumulationCallPatchByteCount,
            mOriginalProtection,
            &ignored));
    }
    abandonOwnedState();
}

bool RetailWorldAccumulationHookWin32Memory::initialize(
    std::uint32_t authorizedWorldAddress) noexcept
{
    if constexpr (!RetailWorldAccumulationHookWin32MemoryAvailable)
    {
        (void)authorizedWorldAddress;
        return false;
    }
    else
    {
        if (mInitialized
            || mWritableAddress != 0u
            || authorizedWorldAddress == 0u
            || !readableExecutableRange(
                authorizedWorldAddress,
                RetailWorldFunctionByteCount))
        {
            return false;
        }
        std::array<
            std::uint32_t,
            RetailWorldAccumulationCallSiteContractInventory.size()>
            addresses {};
        for (std::size_t index = 0u; index < addresses.size(); ++index)
        {
            std::uintptr_t address = 0u;
            if (!relocateRetailWorldPreferredAddress(
                    authorizedWorldAddress,
                    RetailWorldAccumulationCallSiteContractInventory[index]
                        .preferredCallAddress,
                    address)
                || address == 0u
                || address > 0xFFFFFFFFu)
            {
                return false;
            }
            addresses[index] = static_cast<std::uint32_t>(address);
        }
        mWorldAddress = authorizedWorldAddress;
        mCallSiteAddresses = addresses;
        mNextToken = 1u;
        mInitialized = true;
        return true;
    }
}

bool RetailWorldAccumulationHookWin32Memory::ready() const noexcept
{
    return RetailWorldAccumulationHookWin32MemoryAvailable
        && mInitialized
        && mWorldAddress != 0u;
}

RetailWorldAccumulationHookMemoryOperations
RetailWorldAccumulationHookWin32Memory::operations() noexcept
{
    if (!ready())
        return {};
    return {
        this,
        &read,
        &makeWritable,
        &restoreProtection,
        &write,
        &flushInstructionCache,
    };
}

bool RetailWorldAccumulationHookWin32Memory::rangeIsCallSite(
    std::uint32_t address,
    std::size_t byteCount) const noexcept
{
    if (!ready()
        || byteCount != RetailWorldAccumulationCallPatchByteCount)
    {
        return false;
    }
    for (const std::uint32_t callSite : mCallSiteAddresses)
    {
        if (address == callSite)
            return true;
    }
    return false;
}

bool RetailWorldAccumulationHookWin32Memory::read(
    void* opaque,
    std::uint32_t address,
    std::uint8_t* destination,
    std::size_t byteCount) noexcept
{
    auto* state = static_cast<RetailWorldAccumulationHookWin32Memory*>(opaque);
    if (!state || !destination || !state->rangeIsCallSite(address, byteCount))
        return false;
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

RetailWorldHookProtectionLease
RetailWorldAccumulationHookWin32Memory::makeWritable(
    void* opaque,
    std::uint32_t address,
    std::size_t byteCount) noexcept
{
    auto* state = static_cast<RetailWorldAccumulationHookWin32Memory*>(opaque);
    if (!state
        || !state->rangeIsCallSite(address, byteCount)
        || state->mWritableAddress != 0u
        || state->mProtectionToken != 0u)
    {
        return {};
    }
    DWORD oldProtection = 0u;
    if (!VirtualProtect(
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(address)),
            byteCount,
            PAGE_EXECUTE_READWRITE,
            &oldProtection)
        || oldProtection == 0u)
    {
        return {};
    }
    const std::uintptr_t token = static_cast<std::uintptr_t>(state->mNextToken++);
    if (token == 0u || state->mNextToken == 0u)
    {
        DWORD ignored = 0u;
        static_cast<void>(VirtualProtect(
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(address)),
            byteCount,
            oldProtection,
            &ignored));
        return {};
    }
    state->mWritableAddress = address;
    state->mOriginalProtection = oldProtection;
    state->mProtectionToken = token;
    return { true, token };
}

bool RetailWorldAccumulationHookWin32Memory::restoreProtection(
    void* opaque,
    const RetailWorldHookProtectionLease& lease) noexcept
{
    auto* state = static_cast<RetailWorldAccumulationHookWin32Memory*>(opaque);
    if (!state
        || state->mWritableAddress == 0u
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
                static_cast<std::uintptr_t>(state->mWritableAddress)),
            RetailWorldAccumulationCallPatchByteCount,
            state->mOriginalProtection,
            &ignored))
    {
        return false;
    }
    state->mWritableAddress = 0u;
    state->mProtectionToken = 0u;
    state->mOriginalProtection = 0u;
    return true;
}

bool RetailWorldAccumulationHookWin32Memory::write(
    void* opaque,
    std::uint32_t address,
    const std::uint8_t* source,
    std::size_t byteCount) noexcept
{
    auto* state = static_cast<RetailWorldAccumulationHookWin32Memory*>(opaque);
    if (!state
        || !source
        || state->mWritableAddress != address
        || !state->rangeIsCallSite(address, byteCount))
    {
        return false;
    }
    SIZE_T transferred = 0u;
    return WriteProcessMemory(
            GetCurrentProcess(),
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(address)),
            source,
            byteCount,
            &transferred)
        && transferred == byteCount;
}

bool RetailWorldAccumulationHookWin32Memory::flushInstructionCache(
    void* opaque,
    std::uint32_t address,
    std::size_t byteCount) noexcept
{
    auto* state = static_cast<RetailWorldAccumulationHookWin32Memory*>(opaque);
    if (!state || !state->rangeIsCallSite(address, byteCount))
        return false;
    return FlushInstructionCache(
        GetCurrentProcess(),
        reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)),
        byteCount) != FALSE;
}

void RetailWorldAccumulationHookWin32Memory::abandonOwnedState() noexcept
{
    mWorldAddress = 0u;
    mCallSiteAddresses = {};
    mWritableAddress = 0u;
    mProtectionToken = 0u;
    mOriginalProtection = 0u;
    mNextToken = 1u;
    mInitialized = false;
}

RetailFirstPersonHookWin32Memory::~RetailFirstPersonHookWin32Memory() noexcept
{
    if (mWritableAddress != 0u && mOriginalProtection != 0u)
    {
        DWORD ignored = 0u;
        static_cast<void>(VirtualProtect(
            reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(mWritableAddress)),
            RetailFirstPersonCallPatchByteCount,
            mOriginalProtection,
            &ignored));
    }
    abandonOwnedState();
}

bool RetailFirstPersonHookWin32Memory::initialize(
    std::uint32_t authorizedImageBase) noexcept
{
    if constexpr (!RetailWorldAccumulationHookWin32MemoryAvailable)
    {
        (void)authorizedImageBase;
        return false;
    }
    else
    {
        if (mInitialized
            || mWritableAddress != 0u
            || authorizedImageBase == 0u
            || !retailFirstPersonCallSiteContractInventoryProductionProven())
        {
            return false;
        }
        std::array<
            std::uint32_t,
            RetailFirstPersonCallSiteContractInventory.size()> addresses {};
        for (std::size_t index = 0u; index < addresses.size(); ++index)
        {
            std::uintptr_t address = 0u;
            if (!relocateRetailFirstPersonPreferredAddress(
                    authorizedImageBase,
                    RetailFirstPersonCallSiteContractInventory[index]
                        .preferredCallAddress,
                    address)
                || address == 0u
                || address > 0xFFFFFFFFu
                || !readableExecutableRange(
                    static_cast<std::uint32_t>(address),
                    RetailFirstPersonCallPatchByteCount))
            {
                return false;
            }
            addresses[index] = static_cast<std::uint32_t>(address);
        }
        mImageBase = authorizedImageBase;
        mCallSiteAddresses = addresses;
        mNextToken = 1u;
        mInitialized = true;
        return true;
    }
}

bool RetailFirstPersonHookWin32Memory::ready() const noexcept
{
    return RetailWorldAccumulationHookWin32MemoryAvailable
        && mInitialized
        && mImageBase != 0u;
}

RetailWorldAccumulationHookMemoryOperations
RetailFirstPersonHookWin32Memory::operations() noexcept
{
    if (!ready())
        return {};
    return {
        this,
        &read,
        &makeWritable,
        &restoreProtection,
        &write,
        &flushInstructionCache,
    };
}

bool RetailFirstPersonHookWin32Memory::rangeIsCallSite(
    std::uint32_t address,
    std::size_t byteCount) const noexcept
{
    if (!ready() || byteCount != RetailFirstPersonCallPatchByteCount)
        return false;
    for (const std::uint32_t callSite : mCallSiteAddresses)
    {
        if (address == callSite)
            return true;
    }
    return false;
}

bool RetailFirstPersonHookWin32Memory::read(
    void* opaque,
    std::uint32_t address,
    std::uint8_t* destination,
    std::size_t byteCount) noexcept
{
    auto* state = static_cast<RetailFirstPersonHookWin32Memory*>(opaque);
    if (!state || !destination || !state->rangeIsCallSite(address, byteCount))
        return false;
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

RetailWorldHookProtectionLease
RetailFirstPersonHookWin32Memory::makeWritable(
    void* opaque,
    std::uint32_t address,
    std::size_t byteCount) noexcept
{
    auto* state = static_cast<RetailFirstPersonHookWin32Memory*>(opaque);
    if (!state
        || !state->rangeIsCallSite(address, byteCount)
        || state->mWritableAddress != 0u
        || state->mProtectionToken != 0u)
    {
        return {};
    }
    DWORD oldProtection = 0u;
    if (!VirtualProtect(
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(address)),
            byteCount,
            PAGE_EXECUTE_READWRITE,
            &oldProtection)
        || oldProtection == 0u)
    {
        return {};
    }
    const std::uintptr_t token = static_cast<std::uintptr_t>(state->mNextToken++);
    if (token == 0u || state->mNextToken == 0u)
    {
        DWORD ignored = 0u;
        static_cast<void>(VirtualProtect(
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(address)),
            byteCount,
            oldProtection,
            &ignored));
        return {};
    }
    state->mWritableAddress = address;
    state->mOriginalProtection = oldProtection;
    state->mProtectionToken = token;
    return { true, token };
}

bool RetailFirstPersonHookWin32Memory::restoreProtection(
    void* opaque,
    const RetailWorldHookProtectionLease& lease) noexcept
{
    auto* state = static_cast<RetailFirstPersonHookWin32Memory*>(opaque);
    if (!state
        || state->mWritableAddress == 0u
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
                static_cast<std::uintptr_t>(state->mWritableAddress)),
            RetailFirstPersonCallPatchByteCount,
            state->mOriginalProtection,
            &ignored))
    {
        return false;
    }
    state->mWritableAddress = 0u;
    state->mProtectionToken = 0u;
    state->mOriginalProtection = 0u;
    return true;
}

bool RetailFirstPersonHookWin32Memory::write(
    void* opaque,
    std::uint32_t address,
    const std::uint8_t* source,
    std::size_t byteCount) noexcept
{
    auto* state = static_cast<RetailFirstPersonHookWin32Memory*>(opaque);
    if (!state
        || !source
        || state->mWritableAddress != address
        || !state->rangeIsCallSite(address, byteCount))
    {
        return false;
    }
    SIZE_T transferred = 0u;
    return WriteProcessMemory(
            GetCurrentProcess(),
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(address)),
            source,
            byteCount,
            &transferred)
        && transferred == byteCount;
}

bool RetailFirstPersonHookWin32Memory::flushInstructionCache(
    void* opaque,
    std::uint32_t address,
    std::size_t byteCount) noexcept
{
    auto* state = static_cast<RetailFirstPersonHookWin32Memory*>(opaque);
    if (!state || !state->rangeIsCallSite(address, byteCount))
        return false;
    return FlushInstructionCache(
        GetCurrentProcess(),
        reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)),
        byteCount) != FALSE;
}

void RetailFirstPersonHookWin32Memory::abandonOwnedState() noexcept
{
    mImageBase = 0u;
    mCallSiteAddresses = {};
    mWritableAddress = 0u;
    mProtectionToken = 0u;
    mOriginalProtection = 0u;
    mNextToken = 1u;
    mInitialized = false;
}
}
