#include "fnvxr_retail_world_hook_lease.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <type_traits>
#include <utility>
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

constexpr std::uint32_t RuntimeWorldAddress = 0x00873200u;
constexpr std::uint32_t AdapterAddress = 0x60001000u;
constexpr std::uint32_t TrampolineAddress = 0x50002000u;
constexpr std::uintptr_t AllocationToken = 0xA110CA7Eu;
constexpr std::uintptr_t ProtectionToken = 0xC0DEF00Du;

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

struct FakeMemory
{
    std::vector<std::uint8_t> world;
    std::array<std::uint8_t, RetailWorldHookTrampolineByteCount> trampoline {};
    bool allocationOwned = false;
    bool worldWritable = false;
    std::size_t operationCount = 0u;
    std::size_t failOperation = 0u;
    std::size_t corruptReadOperation = 0u;
    bool mutateEntryOnProtection = false;
    bool malformedAllocation = false;
    bool malformedProtection = false;
    std::size_t allocationRequests = 0u;
    std::size_t releaseRequests = 0u;
    std::size_t protectionRequests = 0u;
    std::size_t protectionRestoreRequests = 0u;
    std::size_t writeRequests = 0u;
    std::size_t flushRequests = 0u;

    FakeMemory()
        : world(RetailWorldFunctionByteCount, 0xCCu)
    {
        std::copy(
            RetailWorldHookStolenBytes.begin(),
            RetailWorldHookStolenBytes.end(),
            world.begin());
        std::copy(
            RetailWorldReturnBytes.begin(),
            RetailWorldReturnBytes.end(),
            world.begin() + RetailWorldReturnInstructionOffset);
    }

    bool operationFails() noexcept
    {
        ++operationCount;
        return failOperation != 0u && operationCount == failOperation;
    }

    static FakeMemory& self(void* context) noexcept
    {
        return *static_cast<FakeMemory*>(context);
    }

    static bool read(
        void* context,
        std::uint32_t address,
        std::uint8_t* output,
        std::size_t byteCount) noexcept
    {
        FakeMemory& memory = self(context);
        const bool fail = memory.operationFails();
        if (fail || !output)
            return false;

        if (address == RuntimeWorldAddress
            && byteCount <= memory.world.size())
        {
            std::copy_n(memory.world.begin(), byteCount, output);
        }
        else if (address == TrampolineAddress
            && byteCount <= memory.trampoline.size()
            && memory.allocationOwned)
        {
            std::copy_n(memory.trampoline.begin(), byteCount, output);
        }
        else
        {
            return false;
        }

        if (memory.corruptReadOperation != 0u
            && memory.operationCount == memory.corruptReadOperation
            && byteCount != 0u)
        {
            output[0] ^= 0x01u;
        }
        return true;
    }

    static RetailWorldHookExecutableAllocation allocateExecutable(
        void* context,
        std::size_t byteCount) noexcept
    {
        FakeMemory& memory = self(context);
        ++memory.allocationRequests;
        if (memory.operationFails()
            || byteCount != RetailWorldHookTrampolineByteCount
            || memory.allocationOwned)
        {
            return {};
        }
        memory.allocationOwned = true;
        memory.trampoline = {};
        if (memory.malformedAllocation)
        {
            return {
                true,
                0u,
                AllocationToken,
            };
        }
        return {
            true,
            TrampolineAddress,
            AllocationToken,
        };
    }

    static bool releaseExecutable(
        void* context,
        const RetailWorldHookExecutableAllocation& allocation) noexcept
    {
        FakeMemory& memory = self(context);
        ++memory.releaseRequests;
        if (memory.operationFails())
            return false;
        if (!memory.allocationOwned
            || !allocation.ownershipTransferred
            || allocation.cleanupToken != AllocationToken)
        {
            return false;
        }
        memory.allocationOwned = false;
        memory.trampoline = {};
        return true;
    }

    static RetailWorldHookProtectionLease makeWritable(
        void* context,
        std::uint32_t address,
        std::size_t byteCount) noexcept
    {
        FakeMemory& memory = self(context);
        ++memory.protectionRequests;
        if (memory.operationFails()
            || address != RuntimeWorldAddress
            || byteCount != RetailWorldHookPatchByteCount
            || memory.worldWritable)
        {
            return {};
        }
        memory.worldWritable = true;
        if (memory.mutateEntryOnProtection)
            memory.world[0] ^= 0x01u;
        if (memory.malformedProtection)
            return { true, 0u };
        return { true, ProtectionToken };
    }

    static bool restoreProtection(
        void* context,
        const RetailWorldHookProtectionLease& protection) noexcept
    {
        FakeMemory& memory = self(context);
        ++memory.protectionRestoreRequests;
        if (memory.operationFails())
            return false;
        if (!memory.worldWritable
            || !protection.ownershipTransferred)
        {
            return false;
        }
        memory.worldWritable = false;
        return true;
    }

    static bool write(
        void* context,
        std::uint32_t address,
        const std::uint8_t* input,
        std::size_t byteCount) noexcept
    {
        FakeMemory& memory = self(context);
        ++memory.writeRequests;
        const bool fail = memory.operationFails();
        if (!input)
            return false;

        bool destinationValid = false;
        if (address == RuntimeWorldAddress
            && byteCount <= memory.world.size()
            && memory.worldWritable)
        {
            std::copy_n(input, byteCount, memory.world.begin());
            destinationValid = true;
        }
        else if (address == TrampolineAddress
            && byteCount <= memory.trampoline.size()
            && memory.allocationOwned)
        {
            std::copy_n(input, byteCount, memory.trampoline.begin());
            destinationValid = true;
        }
        // A failing write is allowed to have partially/completely changed the
        // destination; this fake deliberately mutates first to exercise the
        // installer's mandatory rollback path.
        return destinationValid && !fail;
    }

    static bool flushInstructionCache(
        void* context,
        std::uint32_t address,
        std::size_t byteCount) noexcept
    {
        FakeMemory& memory = self(context);
        ++memory.flushRequests;
        const bool fail = memory.operationFails();
        const bool rangeValid =
            (address == RuntimeWorldAddress
                && byteCount == RetailWorldHookPatchByteCount)
            || (address == TrampolineAddress
                && byteCount == RetailWorldHookTrampolineByteCount
                && memory.allocationOwned);
        return rangeValid && !fail;
    }

    RetailWorldHookMemoryOperations operations() noexcept
    {
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

    bool entryIsOriginal() const noexcept
    {
        return std::equal(
            RetailWorldHookStolenBytes.begin(),
            RetailWorldHookStolenBytes.end(),
            world.begin());
    }

    void resetFaults() noexcept
    {
        operationCount = 0u;
        failOperation = 0u;
        corruptReadOperation = 0u;
        mutateEntryOnProtection = false;
        malformedAllocation = false;
        malformedProtection = false;
    }
};

struct DispatchCapture
{
    std::size_t count = 0u;
    RetailWorldHookDispatchFrame frame {};

    static void dispatch(
        void* context,
        const RetailWorldHookDispatchFrame& frame) noexcept
    {
        auto& capture = *static_cast<DispatchCapture*>(context);
        ++capture.count;
        capture.frame = frame;
    }

    RetailWorldHookDispatchOperations operations() noexcept
    {
        return { this, &dispatch };
    }
};

RetailWorldHookInstallResult install(
    FakeMemory& memory,
    DispatchCapture& dispatch,
    const RetailWorldHookAuthorization& authorization)
{
    return installRetailWorldHook(
        authorization,
        memory.operations(),
        dispatch.operations(),
        RuntimeWorldAddress,
        AdapterAddress);
}

const std::array<RetailWorldHookInstallFailure, 11> InstallOperationFailures {{
    RetailWorldHookInstallFailure::WorldBodyReadFailed,
    RetailWorldHookInstallFailure::TrampolineAllocationFailed,
    RetailWorldHookInstallFailure::TrampolineWriteFailed,
    RetailWorldHookInstallFailure::TrampolineFlushFailed,
    RetailWorldHookInstallFailure::TrampolineVerificationReadFailed,
    RetailWorldHookInstallFailure::EntryProtectionFailed,
    RetailWorldHookInstallFailure::EntryRevalidationReadFailed,
    RetailWorldHookInstallFailure::EntryWriteFailed,
    RetailWorldHookInstallFailure::EntryFlushFailed,
    RetailWorldHookInstallFailure::EntryVerificationReadFailed,
    RetailWorldHookInstallFailure::EntryProtectionRestoreFailed,
}};

const std::array<RetailWorldHookUninstallFailure, 7>
    UninstallOperationFailures {{
        RetailWorldHookUninstallFailure::EntryProtectionFailed,
        RetailWorldHookUninstallFailure::EntryReadFailed,
        RetailWorldHookUninstallFailure::OriginalWriteFailed,
        RetailWorldHookUninstallFailure::OriginalFlushFailed,
        RetailWorldHookUninstallFailure::OriginalVerificationReadFailed,
        RetailWorldHookUninstallFailure::EntryProtectionRestoreFailed,
        RetailWorldHookUninstallFailure::TrampolineReleaseFailed,
    }};
}

int main()
{
    using namespace fnvxr::engine;

    static_assert(
        RetailWorldHookProductionAuthorizationAvailable
        == RetailRuntimeProductionAuthorizationAvailable);
    static_assert(RetailWorldHookInstallerEmitsX86CodeOnly);
    static_assert(std::is_default_constructible_v<RetailWorldHookAuthorization>);
    static_assert(
        !std::is_constructible_v<RetailWorldHookAuthorization, bool>);
    static_assert(
        !std::is_constructible_v<
            RetailWorldHookAuthorization,
            std::uint32_t>);
    static_assert(!std::is_copy_constructible_v<RetailWorldHookLease>);
    static_assert(std::is_nothrow_move_constructible_v<RetailWorldHookLease>);
#if defined(_MSC_VER) && defined(_M_IX86)
    static_assert(RetailWorldRenderCallableAbiAvailable);
#else
    static_assert(!RetailWorldRenderCallableAbiAvailable);
#endif

    const RetailWorldHookAuthorization authorization =
        RetailWorldHookLeaseTestAuthority::issue(RuntimeWorldAddress);

    {
        FakeMemory memory;
        DispatchCapture dispatch;
        const RetailWorldHookInstallResult unauthorized =
            install(memory, dispatch, {});
        require(
            unauthorized.failure == RetailWorldHookInstallFailure::Unauthorized
                && memory.operationCount == 0u,
            "a default capability must fail before touching memory");

        const RetailWorldHookAuthorization wrongAddress =
            RetailWorldHookLeaseTestAuthority::issue(
                RuntimeWorldAddress + 1u);
        const RetailWorldHookInstallResult wrongAuthorization =
            install(memory, dispatch, wrongAddress);
        require(
            wrongAuthorization.failure
                    == RetailWorldHookInstallFailure::Unauthorized
                && memory.operationCount == 0u,
            "authorization must be bound to the exact loaded world address");
    }

    {
        FakeMemory memory;
        DispatchCapture dispatch;
        RetailWorldHookMemoryOperations incomplete = memory.operations();
        incomplete.write = nullptr;
        const RetailWorldHookInstallResult result = installRetailWorldHook(
            authorization,
            incomplete,
            dispatch.operations(),
            RuntimeWorldAddress,
            AdapterAddress);
        require(
            result.failure
                    == RetailWorldHookInstallFailure::MemoryOperationsIncomplete
                && memory.operationCount == 0u,
            "an incomplete opaque memory API must fail before acquisition");

        const RetailWorldHookInstallResult missingDispatch =
            installRetailWorldHook(
                authorization,
                memory.operations(),
                {},
                RuntimeWorldAddress,
                AdapterAddress);
        require(
            missingDispatch.failure
                    == RetailWorldHookInstallFailure::DispatchOperationsIncomplete
                && memory.operationCount == 0u,
            "a missing engine-world dispatch seam must fail before acquisition");

        const RetailWorldHookInstallResult zeroAdapter =
            installRetailWorldHook(
                authorization,
                memory.operations(),
                dispatch.operations(),
                RuntimeWorldAddress,
                0u);
        require(
            zeroAdapter.failure
                    == RetailWorldHookInstallFailure::InvalidAdapterAddress
                && memory.operationCount == 0u,
            "a null fastcall adapter must fail before reading engine memory");
    }

    for (std::size_t operation = 1u;
         operation <= InstallOperationFailures.size();
         ++operation)
    {
        FakeMemory memory;
        DispatchCapture dispatch;
        memory.failOperation = operation;
        const RetailWorldHookInstallResult result =
            install(memory, dispatch, authorization);
        require(!result.complete(), "an injected install failure must reject");
        require(
            result.failure == InstallOperationFailures[operation - 1u],
            "the installer reported the wrong failing operation");
        require(
            result.rollbackComplete,
            "every transient install failure must roll back completely");
        require(
            memory.entryIsOriginal(),
            "install failure rollback must restore the exact five stolen bytes");
        require(
            !memory.allocationOwned && !memory.worldWritable,
            "install failure rollback must release trampoline and protection");
    }

    {
        FakeMemory memory;
        DispatchCapture dispatch;
        memory.corruptReadOperation = 5u;
        const RetailWorldHookInstallResult result =
            install(memory, dispatch, authorization);
        require(
            result.failure
                    == RetailWorldHookInstallFailure::TrampolineVerificationMismatch
                && result.rollbackComplete
                && memory.entryIsOriginal()
                && !memory.allocationOwned,
            "a corrupted trampoline readback must release without patching entry");
    }

    {
        FakeMemory memory;
        DispatchCapture dispatch;
        memory.corruptReadOperation = 10u;
        const RetailWorldHookInstallResult result =
            install(memory, dispatch, authorization);
        require(
            result.failure
                    == RetailWorldHookInstallFailure::EntryVerificationMismatch
                && result.rollbackComplete
                && memory.entryIsOriginal()
                && !memory.allocationOwned
                && !memory.worldWritable,
            "a corrupted patch readback must restore entry, protection, and allocation");
    }

    {
        FakeMemory memory;
        DispatchCapture dispatch;
        memory.mutateEntryOnProtection = true;
        const RetailWorldHookInstallResult result =
            install(memory, dispatch, authorization);
        require(
            result.failure
                    == RetailWorldHookInstallFailure::EntryChangedBeforePatch
                && result.rollbackComplete
                && !memory.allocationOwned
                && !memory.worldWritable,
            "TOCTOU mutation must be detected after protection and before write");
        require(
            !memory.entryIsOriginal(),
            "the installer must not overwrite a third-party pre-patch mutation");
    }

    {
        FakeMemory memory;
        DispatchCapture dispatch;
        memory.malformedAllocation = true;
        const RetailWorldHookInstallResult result =
            install(memory, dispatch, authorization);
        require(
            result.failure
                    == RetailWorldHookInstallFailure::TrampolineAllocationMalformed
                && result.rollbackComplete
                && !memory.allocationOwned,
            "owned malformed allocation results must still be released");
    }

    {
        FakeMemory memory;
        DispatchCapture dispatch;
        memory.malformedProtection = true;
        const RetailWorldHookInstallResult result =
            install(memory, dispatch, authorization);
        require(
            result.failure
                    == RetailWorldHookInstallFailure::EntryProtectionLeaseMalformed
                && result.rollbackComplete
                && !memory.worldWritable
                && !memory.allocationOwned,
            "owned malformed protection leases must be restored and released");
    }

    {
        FakeMemory memory;
        DispatchCapture dispatch;
        RetailWorldHookInstallResult result =
            install(memory, dispatch, authorization);
        require(result.complete(), "the exact synthetic x86 image must install");
        require(
            memory.operationCount == 11u
                && memory.allocationOwned
                && !memory.worldWritable
                && memory.world[0] == 0xE9u,
            "install must complete the exact read/allocate/write/verify/protect sequence");
        require(
            std::equal(
                RetailWorldHookStolenBytes.begin(),
                RetailWorldHookStolenBytes.end(),
                memory.trampoline.begin()),
            "the executable trampoline must contain exactly five stolen bytes");
        require(
            memory.world[RetailWorldHookPatchByteCount] == 0xCCu,
            "the entry patch must not touch byte six of the retail prologue");

        require(
            result.lease.dispatchFromFastcallAdapter(
                reinterpret_cast<void*>(0x11110000u),
                reinterpret_cast<void*>(0x22220000u),
                1u,
                0u,
                1u),
            "an installed lease must expose the fastcall adapter dispatch seam");
        require(
            dispatch.count == 1u
                && dispatch.frame.retailThis
                    == reinterpret_cast<void*>(0x11110000u)
                && dispatch.frame.arguments.sharedRenderObjectAddress
                    == 0x22220000u
                && dispatch.frame.arguments.callerModePredicate == 1u
                && dispatch.frame.arguments.stockWorldPathSelector == 0u
                && dispatch.frame.arguments.postWorldOption == 1u
                && dispatch.frame.originalTrampolineAddress
                    == TrampolineAddress,
            "dispatch must preserve every thiscall slot and expose only the retail trampoline");

        memory.resetFaults();
        const std::size_t allocationRequestsBeforeSecondInstall =
            memory.allocationRequests;
        const RetailWorldHookInstallResult second =
            install(memory, dispatch, authorization);
        require(
            second.failure == RetailWorldHookInstallFailure::WorldSeamMismatch
                && memory.allocationRequests
                    == allocationRequestsBeforeSecondInstall,
            "the one-shot patched entry must reject a second installation before allocation");

        RetailWorldHookLease lease(std::move(result.lease));
        require(
            lease.installed() && !result.lease.ownsResources(),
            "moving a lease must transfer its sole cleanup responsibility");
        memory.resetFaults();
        const RetailWorldHookUninstallResult removed = lease.uninstall();
        require(
            removed.complete
                && removed.failure == RetailWorldHookUninstallFailure::None
                && memory.operationCount == 7u
                && memory.entryIsOriginal()
                && !memory.allocationOwned
                && !memory.worldWritable
                && !lease.ownsResources(),
            "uninstall must restore, verify, reprotect, and release exactly once");
        require(
            !lease.dispatchFromFastcallAdapter(nullptr, nullptr, 0u, 0u, 0u),
            "an uninstalled lease must reject adapter dispatch");
        require(
            lease.uninstall().failure
                == RetailWorldHookUninstallFailure::NotInstalled,
            "a consumed one-shot lease must not uninstall twice");
    }

    for (std::size_t operation = 1u;
         operation <= UninstallOperationFailures.size();
         ++operation)
    {
        FakeMemory memory;
        DispatchCapture dispatch;
        RetailWorldHookInstallResult installed =
            install(memory, dispatch, authorization);
        require(installed.complete(), "uninstall fixture installation failed");
        RetailWorldHookLease lease(std::move(installed.lease));
        const std::array<std::uint8_t, RetailWorldHookPatchByteCount> patch {{
            memory.world[0], memory.world[1], memory.world[2],
            memory.world[3], memory.world[4],
        }};

        memory.resetFaults();
        memory.failOperation = operation;
        const RetailWorldHookUninstallResult first = lease.uninstall();
        require(!first.complete, "an injected uninstall failure must reject");
        require(
            first.failure == UninstallOperationFailures[operation - 1u],
            "the lease reported the wrong uninstall failure");

        if (operation < UninstallOperationFailures.size())
        {
            require(
                std::equal(patch.begin(), patch.end(), memory.world.begin())
                    && memory.allocationOwned
                    && !memory.worldWritable
                    && lease.ownsResources(),
                "failed uninstall must roll back to the installed patch");
            require(
                first.rollbackComplete,
                "transient uninstall failure must report complete rollback");
        }
        else
        {
            require(
                memory.entryIsOriginal()
                    && memory.allocationOwned
                    && lease.cleanupPending(),
                "release failure must leave an unhooked cleanup-only lease");
        }

        memory.resetFaults();
        const RetailWorldHookUninstallResult retried = lease.uninstall();
        require(
            retried.complete
                && memory.entryIsOriginal()
                && !memory.allocationOwned
                && !memory.worldWritable
                && !lease.ownsResources(),
            "a transient uninstall failure must be safely retryable");
    }

    {
        FakeMemory memory;
        DispatchCapture dispatch;
        RetailWorldHookInstallResult installed =
            install(memory, dispatch, authorization);
        require(installed.complete(), "entry-replacement fixture installation failed");
        RetailWorldHookLease lease(std::move(installed.lease));
        const std::array<std::uint8_t, RetailWorldHookPatchByteCount> patch {{
            memory.world[0], memory.world[1], memory.world[2],
            memory.world[3], memory.world[4],
        }};
        memory.world[1] ^= 0x01u;
        memory.resetFaults();
        const RetailWorldHookUninstallResult replaced = lease.uninstall();
        require(
            replaced.failure == RetailWorldHookUninstallFailure::EntryReplaced
                && memory.world[1] != patch[1]
                && memory.allocationOwned
                && !memory.worldWritable
                && lease.ownsResources(),
            "uninstall must never overwrite an entry replaced by another owner");
        std::copy(patch.begin(), patch.end(), memory.world.begin());
        memory.resetFaults();
        require(
            lease.uninstall().complete,
            "restoring the owned patch must permit deterministic cleanup");
    }

    {
        FakeMemory memory;
        DispatchCapture dispatch;
        {
            RetailWorldHookInstallResult installed =
                install(memory, dispatch, authorization);
            require(installed.complete(), "destructor fixture installation failed");
        }
        require(
            memory.entryIsOriginal()
                && !memory.allocationOwned
                && !memory.worldWritable,
            "lease destruction must perform best-effort one-shot uninstall");
    }

    std::cout
        << "retail world hook lease tests passed; install/uninstall rollback "
           "matrix green\n";
    return EXIT_SUCCESS;
}
