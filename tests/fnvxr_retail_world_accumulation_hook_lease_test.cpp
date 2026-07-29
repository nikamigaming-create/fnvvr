#include "fnvxr_retail_world_accumulation_hook_lease.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace fnvxr::engine
{
struct RetailWorldHookLeaseTestAuthority
{
    static RetailWorldHookAuthorization issue(
        std::uint32_t worldAddress) noexcept
    {
        return RetailWorldHookAuthorization(worldAddress);
    }
};
}

namespace
{
using namespace fnvxr::engine;

constexpr std::uint32_t RuntimeWorldAddress = WorldRenderAddress;
constexpr std::uint32_t AccumulationAdapterAddress = 0x60001000u;
constexpr std::uint32_t RenderAdapterAddress = 0x60002000u;
constexpr std::uint32_t RenderAndFinalizeAdapterAddress = 0x60003000u;
constexpr std::uintptr_t ProtectionToken = 0x7AA10C0Du;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

struct FakeMemory
{
    std::vector<std::uint8_t> world;
    std::uint32_t writable = 0u;

    FakeMemory()
        : world(RetailWorldFunctionByteCount, 0xCCu)
    {
        for (const RetailWorldAccumulationCallSiteContract& contract
             : RetailWorldAccumulationCallSiteContractInventory)
        {
            const std::size_t offset = static_cast<std::size_t>(
                contract.preferredCallAddress - RuntimeWorldAddress);
            std::memcpy(
                world.data() + offset,
                contract.bytes.data(),
                contract.bytes.size());
        }
    }

    bool callSite(std::uint32_t address, std::size_t byteCount) const noexcept
    {
        if (byteCount != RetailWorldAccumulationCallPatchByteCount)
            return false;
        for (const RetailWorldAccumulationCallSiteContract& contract
             : RetailWorldAccumulationCallSiteContractInventory)
        {
            if (address == contract.preferredCallAddress)
                return true;
        }
        return false;
    }

    static bool read(
        void* raw,
        std::uint32_t address,
        std::uint8_t* destination,
        std::size_t byteCount) noexcept
    {
        auto& self = *static_cast<FakeMemory*>(raw);
        if (!destination || !self.callSite(address, byteCount))
            return false;
        const std::size_t offset = static_cast<std::size_t>(
            address - RuntimeWorldAddress);
        std::memcpy(destination, self.world.data() + offset, byteCount);
        return true;
    }

    static RetailWorldHookProtectionLease writableLease(
        void* raw,
        std::uint32_t address,
        std::size_t byteCount) noexcept
    {
        auto& self = *static_cast<FakeMemory*>(raw);
        if (!self.callSite(address, byteCount) || self.writable != 0u)
            return {};
        self.writable = address;
        return { true, ProtectionToken };
    }

    static bool restore(
        void* raw,
        const RetailWorldHookProtectionLease& lease) noexcept
    {
        auto& self = *static_cast<FakeMemory*>(raw);
        if (self.writable == 0u
            || !lease.ownershipTransferred
            || lease.restoreToken != ProtectionToken)
        {
            return false;
        }
        self.writable = 0u;
        return true;
    }

    static bool write(
        void* raw,
        std::uint32_t address,
        const std::uint8_t* source,
        std::size_t byteCount) noexcept
    {
        auto& self = *static_cast<FakeMemory*>(raw);
        if (!source || self.writable != address || !self.callSite(address, byteCount))
            return false;
        const std::size_t offset = static_cast<std::size_t>(
            address - RuntimeWorldAddress);
        std::memcpy(self.world.data() + offset, source, byteCount);
        return true;
    }

    static bool flush(
        void* raw,
        std::uint32_t address,
        std::size_t byteCount) noexcept
    {
        return static_cast<FakeMemory*>(raw)->callSite(address, byteCount);
    }

    RetailWorldAccumulationHookMemoryOperations operations() noexcept
    {
        return {
            this,
            &read,
            &writableLease,
            &restore,
            &write,
            &flush,
        };
    }
};
}

int main()
{
    try
    {
        static_assert(
            RetailWorldAccumulationCallSiteContractInventory.size() == 6u);

        FakeMemory memory;
        const RetailWorldHookAuthorization authorization =
            RetailWorldHookLeaseTestAuthority::issue(RuntimeWorldAddress);
        RetailWorldAccumulationHookInstallResult installed =
            installRetailWorldAccumulationHook(
                authorization,
                memory.operations(),
                RuntimeWorldAddress,
                AccumulationAdapterAddress,
                RenderAdapterAddress,
                RenderAndFinalizeAdapterAddress);
        require(installed.complete(), "audited callsite lease did not install");
        require(memory.writable == 0u, "install leaked a writable code page");

        for (const RetailWorldAccumulationHookPatch& patch :
             installed.lease.patches())
        {
            std::array<std::uint8_t, RetailWorldAccumulationCallPatchByteCount>
                actual {};
            require(
                FakeMemory::read(
                    &memory,
                    patch.address,
                    actual.data(),
                    actual.size())
                    && actual == patch.replacement
                    && actual[0] == 0xE8u,
                "install did not replace an exact stock E8 call");
        }

        const RetailWorldAccumulationHookUninstallResult removed =
            installed.lease.uninstall();
        require(
            removed.complete && !installed.lease.ownsResources()
                && memory.writable == 0u,
            "callsite lease did not restore its original instructions");
        for (const RetailWorldAccumulationCallSiteContract& contract
             : RetailWorldAccumulationCallSiteContractInventory)
        {
            std::array<std::uint8_t, RetailWorldAccumulationCallPatchByteCount>
                actual {};
            require(
                FakeMemory::read(
                    &memory,
                    static_cast<std::uint32_t>(contract.preferredCallAddress),
                    actual.data(),
                    actual.size())
                    && actual == contract.bytes,
                "uninstall did not restore the audited retail call bytes");
        }

        FakeMemory wrongBytes;
        wrongBytes.world[RetailWorldAccumulationCallSiteContractInventory[1]
                             .preferredCallAddress
                         - RuntimeWorldAddress]
            ^= 0x01u;
        const RetailWorldAccumulationHookInstallResult rejected =
            installRetailWorldAccumulationHook(
                authorization,
                wrongBytes.operations(),
                RuntimeWorldAddress,
                AccumulationAdapterAddress,
                RenderAdapterAddress,
                RenderAndFinalizeAdapterAddress);
        require(
            rejected.failure
                    == RetailWorldAccumulationHookInstallFailure::
                        CallSiteContractMismatch
                && rejected.failedCallSiteIndex == 1u
                && wrongBytes.writable == 0u,
            "a changed stock callsite did not fail closed before mutation");

        const RetailWorldHookAuthorization wrongAuthorization =
            RetailWorldHookLeaseTestAuthority::issue(RuntimeWorldAddress + 1u);
        const RetailWorldAccumulationHookInstallResult unauthorized =
            installRetailWorldAccumulationHook(
                wrongAuthorization,
                memory.operations(),
                RuntimeWorldAddress,
                AccumulationAdapterAddress,
                RenderAdapterAddress,
                RenderAndFinalizeAdapterAddress);
        require(
            unauthorized.failure
                == RetailWorldAccumulationHookInstallFailure::Unauthorized,
            "callsite installer accepted the wrong current-process authority");

        std::cout
            << "retail accumulation lifecycle callsite lease tests passed\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
