#include "fnvxr_retail_first_person_hook_lease.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace fnvxr::engine
{
struct RetailEngineCallResolverTestAuthority
{
    static RetailEngineCallAuthorization issue(
        const abi::RetailEngineAbiAssessment& assessment) noexcept
    {
        return RetailEngineCallAuthorization(assessment);
    }
};
}

namespace
{
using namespace fnvxr::engine;

constexpr std::uintptr_t RuntimeImageBase = SupportedImageBase;
constexpr std::uintptr_t RelayAddress = 0x60001000u;
constexpr std::uintptr_t PreparedRenderRelayAddress = 0x60002000u;
constexpr std::uintptr_t PrimaryRelayAddress = 0x60003000u;
constexpr std::uintptr_t AlternateRelayAddress = 0x60004000u;
constexpr std::uintptr_t ThirdRelayAddress = 0x60005000u;
constexpr RetailFirstPersonRelayAddresses RelayAddresses {
    RelayAddress,
    PreparedRenderRelayAddress,
    PrimaryRelayAddress,
    AlternateRelayAddress,
    ThirdRelayAddress,
};
constexpr std::uintptr_t ProtectionToken = 0xF1A5700Du;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

abi::RetailEngineAbiEvidence completeEvidence()
{
    abi::RetailEngineAbiEvidence evidence {};
    evidence.loadedExecutableIdentityMatched = true;
    evidence.loadedExecutableSectionLayoutAndProtectionsVerified = true;
    evidence.coreManifestMatched = true;
    evidence.fullFunctionInventoryMatched = true;
    evidence.vtableSlotsMatched = true;
    evidence.vtableBlocksMatched = true;
    evidence.liveObjectLayoutsVerified = true;
    evidence.constructorOwnershipVerified = true;
    evidence.bothWorldBranchesVerified = true;
    evidence.compatibilityModulesVerified = true;
    evidence.synchronousRuntimeRevalidation = true;
    return evidence;
}

struct FakeMemory
{
    std::vector<std::uint8_t> image;
    std::uint32_t writable = 0u;

    FakeMemory()
        : image(0x00480000u, 0xCCu)
    {
        for (const RetailFirstPersonCallSiteContract& contract
             : RetailFirstPersonCallSiteContractInventory)
        {
            const std::size_t offset = static_cast<std::size_t>(
                contract.preferredCallAddress - RuntimeImageBase);
            std::memcpy(
                image.data() + offset,
                contract.bytes.data(),
                contract.bytes.size());
        }
    }

    bool callSite(std::uint32_t address, std::size_t byteCount) const noexcept
    {
        if (byteCount != RetailFirstPersonCallPatchByteCount)
            return false;
        for (const RetailFirstPersonCallSiteContract& contract
             : RetailFirstPersonCallSiteContractInventory)
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
            address - RuntimeImageBase);
        std::memcpy(destination, self.image.data() + offset, byteCount);
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
            address - RuntimeImageBase);
        std::memcpy(self.image.data() + offset, source, byteCount);
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
        static_assert(RetailFirstPersonCallSiteContractInventory.size() == 4u);
        static_assert(retailFirstPersonCallSiteContractInventoryProductionProven());
        const abi::RetailEngineAbiAssessment assessment =
            abi::assessRetailEngineAbi(completeEvidence());
        require(
            assessment.engineCallsAuthorized,
            "test prerequisite did not mint the engine-call authority");
        const RetailEngineCallAuthorization authorization =
            RetailEngineCallResolverTestAuthority::issue(assessment);

        FakeMemory memory;
        RetailFirstPersonHookInstallResult installed =
            installRetailFirstPersonHook(
                authorization,
                memory.operations(),
                RuntimeImageBase,
                RelayAddresses);
        require(installed.complete(), "first-person E8 lease did not install");
        require(memory.writable == 0u, "install leaked a writable callsite");
        for (std::size_t index = 0u;
             index < installed.lease.patches().size();
             ++index)
        {
            const RetailFirstPersonHookPatch& patch =
            installed.lease.patches()[index];
            std::array<std::uint8_t, RetailFirstPersonCallPatchByteCount> actual {};
            std::uintptr_t replacementTarget = 0u;
            const std::uintptr_t expectedRelay =
                RelayAddresses.relayForCallSite(index);
            require(
                FakeMemory::read(
                    &memory,
                    patch.address,
                    actual.data(),
                    actual.size())
                    && actual == patch.replacement
                    && actual[0] == 0xE8u
                    && decodeRetailFirstPersonRel32Target(
                        actual.data(),
                        actual.size(),
                        patch.address,
                        replacementTarget)
                    && replacementTarget == expectedRelay,
                "lease did not replace an exact first-person E8 call");
        }

        RetailFirstPersonHookLease lease = std::move(installed.lease);
        const RetailFirstPersonHookUninstallResult removed = lease.uninstall();
        require(
            removed.complete && !lease.ownsResources() && memory.writable == 0u,
            "first-person E8 lease did not restore its original callsites");
        for (const RetailFirstPersonCallSiteContract& contract
             : RetailFirstPersonCallSiteContractInventory)
        {
            std::array<std::uint8_t, RetailFirstPersonCallPatchByteCount> actual {};
            require(
                FakeMemory::read(
                    &memory,
                    static_cast<std::uint32_t>(contract.preferredCallAddress),
                    actual.data(),
                    actual.size())
                    && actual == contract.bytes,
                "lease uninstall did not restore the stock first-person bytes");
        }

        FakeMemory changed;
        changed.image[RetailFirstPersonCallSiteContractInventory[1]
                          .preferredCallAddress
                      - RuntimeImageBase]
            ^= 0x01u;
        const RetailFirstPersonHookInstallResult rejected =
            installRetailFirstPersonHook(
                authorization,
                changed.operations(),
                RuntimeImageBase,
                RelayAddresses);
        require(
            rejected.failure
                    == RetailFirstPersonHookInstallFailure::CallSiteContractMismatch
                && rejected.failedCallSiteIndex == 1u
                && changed.writable == 0u,
            "a changed first-person caller did not fail closed before mutation");

        const RetailFirstPersonHookInstallResult unauthorized =
            installRetailFirstPersonHook(
                {},
                memory.operations(),
                RuntimeImageBase,
                RelayAddresses);
        require(
            unauthorized.failure == RetailFirstPersonHookInstallFailure::Unauthorized,
            "first-person callsite installer accepted an unissued authority");

        std::cout << "retail first-person callsite lease tests passed\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
