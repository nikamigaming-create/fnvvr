#pragma once

#include "fnvxr_retail_engine_calls.h"
#include "fnvxr_retail_first_person_hook_contract.h"
#include "fnvxr_retail_world_accumulation_hook_lease.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace fnvxr::engine
{
// The first-person lease deliberately shares only the narrow read/protect/
// write backend shape with the world callsite lease.  Its backend is a
// different concrete object that admits exactly the four contracts above.
// It therefore cannot turn a valid world-hook authority into arbitrary code
// mutation.
struct RetailFirstPersonHookPatch
{
    std::uint32_t address = 0u;
    std::array<std::uint8_t, RetailFirstPersonCallPatchByteCount> original {};
    std::array<std::uint8_t, RetailFirstPersonCallPatchByteCount> replacement {};
};

struct RetailFirstPersonRelayAddresses
{
    std::uintptr_t renderFirstPerson = 0u;
    std::uintptr_t renderPreparedAccumulator = 0u;
    std::uintptr_t renderFirstPersonPrimary = 0u;
    std::uintptr_t renderFirstPersonAlternate = 0u;
    std::uintptr_t renderFirstPersonThird = 0u;

    constexpr bool complete() const noexcept
    {
        return renderFirstPerson != 0u
            && renderFirstPerson <= 0xFFFFFFFFu
            && renderPreparedAccumulator != 0u
            && renderPreparedAccumulator <= 0xFFFFFFFFu
            && renderFirstPersonPrimary != 0u
            && renderFirstPersonPrimary <= 0xFFFFFFFFu
            && renderFirstPersonAlternate != 0u
            && renderFirstPersonAlternate <= 0xFFFFFFFFu
            && renderFirstPersonThird != 0u
            && renderFirstPersonThird <= 0xFFFFFFFFu;
    }

    constexpr std::uintptr_t relayForCallSite(std::size_t index) const noexcept
    {
        switch (index)
        {
        case 1u:
            return renderFirstPersonPrimary;
        case 2u:
            return renderFirstPersonAlternate;
        case 3u:
            return renderFirstPersonThird;
        default:
            return index == 0u
                ? renderPreparedAccumulator
                : renderFirstPerson;
        }
    }
};

enum class RetailFirstPersonHookInstallFailure : std::uint8_t
{
    None = 0u,
    Unauthorized,
    MemoryOperationsIncomplete,
    InvalidAdapterAddress,
    CallSiteAddressRelocationFailed,
    CallSiteReadFailed,
    CallSiteContractMismatch,
    PatchPlanFailed,
    CallSiteProtectionFailed,
    CallSiteProtectionLeaseMalformed,
    CallSiteRevalidationReadFailed,
    CallSiteChangedBeforePatch,
    CallSiteWriteFailed,
    CallSiteFlushFailed,
    CallSiteVerificationReadFailed,
    CallSiteVerificationMismatch,
    CallSiteProtectionRestoreFailed,
};

enum class RetailFirstPersonHookUninstallFailure : std::uint8_t
{
    None = 0u,
    NotInstalled,
    CallSiteProtectionFailed,
    CallSiteProtectionLeaseMalformed,
    CallSiteReadFailed,
    CallSiteReplaced,
    OriginalWriteFailed,
    OriginalFlushFailed,
    OriginalVerificationReadFailed,
    OriginalVerificationMismatch,
    CallSiteProtectionRestoreFailed,
};

struct RetailFirstPersonHookUninstallResult
{
    bool complete = false;
    RetailFirstPersonHookUninstallFailure failure =
        RetailFirstPersonHookUninstallFailure::NotInstalled;
    bool rollbackAttempted = false;
    bool rollbackComplete = true;
    std::size_t failedCallSiteIndex =
        RetailFirstPersonCallSiteContractInventory.size();
};

namespace detail
{
inline bool restoreRetailFirstPersonPatchWhileWritable(
    const RetailWorldAccumulationHookMemoryOperations& memory,
    const RetailWorldHookProtectionLease& protection,
    const RetailFirstPersonHookPatch& patch) noexcept
{
    bool complete = memory.write(
        memory.context,
        patch.address,
        patch.original.data(),
        patch.original.size());
    if (!memory.flushInstructionCache(
            memory.context,
            patch.address,
            patch.original.size()))
    {
        complete = false;
    }
    std::array<std::uint8_t, RetailFirstPersonCallPatchByteCount> verify {};
    const bool read = memory.read(
        memory.context,
        patch.address,
        verify.data(),
        verify.size());
    complete = read
        && retailFirstPersonBytesMatch(
            verify.data(), patch.original.data(), verify.size())
        && complete;
    return memory.restoreProtection(memory.context, protection) && complete;
}

struct RetailFirstPersonPatchApplyResult
{
    RetailFirstPersonHookInstallFailure failure =
        RetailFirstPersonHookInstallFailure::None;
    bool installed = false;
    bool cleanupComplete = true;
};

inline RetailFirstPersonPatchApplyResult applyRetailFirstPersonPatch(
    const RetailWorldAccumulationHookMemoryOperations& memory,
    const RetailFirstPersonHookPatch& patch) noexcept
{
    using Failure = RetailFirstPersonHookInstallFailure;
    const RetailWorldHookProtectionLease protection = memory.makeWritable(
        memory.context,
        patch.address,
        patch.original.size());
    if (!protection.ownershipTransferred)
        return { Failure::CallSiteProtectionFailed, false, true };
    if (!protection.usable())
    {
        return {
            Failure::CallSiteProtectionLeaseMalformed,
            false,
            memory.restoreProtection(memory.context, protection),
        };
    }

    std::array<std::uint8_t, RetailFirstPersonCallPatchByteCount> current {};
    if (!memory.read(
            memory.context,
            patch.address,
            current.data(),
            current.size()))
    {
        return {
            Failure::CallSiteRevalidationReadFailed,
            false,
            memory.restoreProtection(memory.context, protection),
        };
    }
    if (!retailFirstPersonBytesMatch(
            current.data(), patch.original.data(), current.size()))
    {
        return {
            Failure::CallSiteChangedBeforePatch,
            false,
            memory.restoreProtection(memory.context, protection),
        };
    }
    if (!memory.write(
            memory.context,
            patch.address,
            patch.replacement.data(),
            patch.replacement.size()))
    {
        return {
            Failure::CallSiteWriteFailed,
            false,
            restoreRetailFirstPersonPatchWhileWritable(memory, protection, patch),
        };
    }
    if (!memory.flushInstructionCache(
            memory.context,
            patch.address,
            patch.replacement.size()))
    {
        return {
            Failure::CallSiteFlushFailed,
            false,
            restoreRetailFirstPersonPatchWhileWritable(memory, protection, patch),
        };
    }
    std::array<std::uint8_t, RetailFirstPersonCallPatchByteCount> verify {};
    if (!memory.read(
            memory.context,
            patch.address,
            verify.data(),
            verify.size()))
    {
        return {
            Failure::CallSiteVerificationReadFailed,
            false,
            restoreRetailFirstPersonPatchWhileWritable(memory, protection, patch),
        };
    }
    if (!retailFirstPersonBytesMatch(
            verify.data(), patch.replacement.data(), verify.size()))
    {
        return {
            Failure::CallSiteVerificationMismatch,
            false,
            restoreRetailFirstPersonPatchWhileWritable(memory, protection, patch),
        };
    }
    if (!memory.restoreProtection(memory.context, protection))
    {
        return {
            Failure::CallSiteProtectionRestoreFailed,
            false,
            restoreRetailFirstPersonPatchWhileWritable(memory, protection, patch),
        };
    }
    return { Failure::None, true, true };
}

inline RetailFirstPersonHookUninstallResult restoreRetailFirstPersonPatch(
    const RetailWorldAccumulationHookMemoryOperations& memory,
    const RetailFirstPersonHookPatch& patch,
    std::size_t index) noexcept
{
    using Failure = RetailFirstPersonHookUninstallFailure;
    const RetailWorldHookProtectionLease protection = memory.makeWritable(
        memory.context,
        patch.address,
        patch.original.size());
    if (!protection.ownershipTransferred)
        return { false, Failure::CallSiteProtectionFailed, false, true, index };
    if (!protection.usable())
    {
        return {
            false,
            Failure::CallSiteProtectionLeaseMalformed,
            false,
            memory.restoreProtection(memory.context, protection),
            index,
        };
    }
    std::array<std::uint8_t, RetailFirstPersonCallPatchByteCount> current {};
    if (!memory.read(
            memory.context,
            patch.address,
            current.data(),
            current.size()))
    {
        return {
            false,
            Failure::CallSiteReadFailed,
            false,
            memory.restoreProtection(memory.context, protection),
            index,
        };
    }
    if (!retailFirstPersonBytesMatch(
            current.data(), patch.replacement.data(), current.size()))
    {
        return {
            false,
            Failure::CallSiteReplaced,
            false,
            memory.restoreProtection(memory.context, protection),
            index,
        };
    }
    if (!memory.write(
            memory.context,
            patch.address,
            patch.original.data(),
            patch.original.size()))
    {
        return {
            false,
            Failure::OriginalWriteFailed,
            true,
            restoreRetailFirstPersonPatchWhileWritable(memory, protection, patch),
            index,
        };
    }
    if (!memory.flushInstructionCache(
            memory.context,
            patch.address,
            patch.original.size()))
    {
        return {
            false,
            Failure::OriginalFlushFailed,
            true,
            restoreRetailFirstPersonPatchWhileWritable(memory, protection, patch),
            index,
        };
    }
    std::array<std::uint8_t, RetailFirstPersonCallPatchByteCount> verify {};
    if (!memory.read(
            memory.context,
            patch.address,
            verify.data(),
            verify.size()))
    {
        return {
            false,
            Failure::OriginalVerificationReadFailed,
            true,
            restoreRetailFirstPersonPatchWhileWritable(memory, protection, patch),
            index,
        };
    }
    if (!retailFirstPersonBytesMatch(
            verify.data(), patch.original.data(), verify.size()))
    {
        return {
            false,
            Failure::OriginalVerificationMismatch,
            true,
            restoreRetailFirstPersonPatchWhileWritable(memory, protection, patch),
            index,
        };
    }
    if (!memory.restoreProtection(memory.context, protection))
    {
        return {
            false,
            Failure::CallSiteProtectionRestoreFailed,
            true,
            false,
            index,
        };
    }
    return { true, Failure::None, true, true, index };
}
}

class RetailFirstPersonHookLease;
struct RetailFirstPersonHookInstallResult;

class RetailFirstPersonHookLease final
{
public:
    constexpr RetailFirstPersonHookLease() noexcept = default;
    RetailFirstPersonHookLease(const RetailFirstPersonHookLease&) = delete;
    RetailFirstPersonHookLease& operator=(const RetailFirstPersonHookLease&) =
        delete;

    RetailFirstPersonHookLease(RetailFirstPersonHookLease&& other) noexcept
    {
        moveFrom(other);
    }
    RetailFirstPersonHookLease& operator=(RetailFirstPersonHookLease&&) =
        delete;

    ~RetailFirstPersonHookLease() noexcept
    {
        if (ownsResources())
            static_cast<void>(uninstall());
    }

    bool installed() const noexcept
    {
        return mState == State::Installed && mIntegrityKnown;
    }

    bool ownsResources() const noexcept
    {
        return mState != State::Empty;
    }

    const std::array<
        RetailFirstPersonHookPatch,
        RetailFirstPersonCallSiteContractInventory.size()>& patches() const
        noexcept
    {
        return mPatches;
    }

    RetailFirstPersonHookUninstallResult uninstall() noexcept
    {
        if (mState == State::Empty)
            return {};
        RetailFirstPersonHookUninstallResult firstFailure {};
        bool complete = true;
        for (std::size_t reverse = mPatches.size(); reverse != 0u; --reverse)
        {
            const std::size_t index = reverse - 1u;
            const RetailFirstPersonHookUninstallResult result =
                detail::restoreRetailFirstPersonPatch(
                    mMemory, mPatches[index], index);
            if (!result.complete && complete)
                firstFailure = result;
            complete = result.complete && complete;
        }
        if (complete)
        {
            clear();
            return {
                true,
                RetailFirstPersonHookUninstallFailure::None,
                true,
                true,
                RetailFirstPersonCallSiteContractInventory.size(),
            };
        }
        mIntegrityKnown = false;
        return firstFailure;
    }

private:
    enum class State : std::uint8_t
    {
        Empty,
        Installed,
    };

    RetailFirstPersonHookLease(
        const RetailWorldAccumulationHookMemoryOperations& memory,
        const std::array<
            RetailFirstPersonHookPatch,
            RetailFirstPersonCallSiteContractInventory.size()>& patches) noexcept
        : mMemory(memory),
          mPatches(patches),
          mState(State::Installed),
          mIntegrityKnown(true)
    {
    }

    void moveFrom(RetailFirstPersonHookLease& other) noexcept
    {
        mMemory = other.mMemory;
        mPatches = other.mPatches;
        mState = other.mState;
        mIntegrityKnown = other.mIntegrityKnown;
        other.clear();
    }

    void clear() noexcept
    {
        mMemory = {};
        mPatches = {};
        mState = State::Empty;
        mIntegrityKnown = false;
    }

    RetailWorldAccumulationHookMemoryOperations mMemory {};
    std::array<
        RetailFirstPersonHookPatch,
        RetailFirstPersonCallSiteContractInventory.size()> mPatches {};
    State mState = State::Empty;
    bool mIntegrityKnown = false;

    friend struct RetailFirstPersonHookInstallResult;
    friend RetailFirstPersonHookInstallResult installRetailFirstPersonHook(
        const RetailEngineCallAuthorization&,
        const RetailWorldAccumulationHookMemoryOperations&,
        std::uintptr_t,
        const RetailFirstPersonRelayAddresses&) noexcept;
};

struct RetailFirstPersonHookInstallResult
{
    RetailFirstPersonHookLease lease {};
    RetailFirstPersonHookInstallFailure failure =
        RetailFirstPersonHookInstallFailure::Unauthorized;
    bool rollbackAttempted = false;
    bool rollbackComplete = true;
    std::size_t failedCallSiteIndex =
        RetailFirstPersonCallSiteContractInventory.size();

    RetailFirstPersonHookInstallResult() noexcept = default;
    RetailFirstPersonHookInstallResult(const RetailFirstPersonHookInstallResult&) =
        delete;
    RetailFirstPersonHookInstallResult& operator=(
        const RetailFirstPersonHookInstallResult&) = delete;
    RetailFirstPersonHookInstallResult(RetailFirstPersonHookInstallResult&&) =
        default;
    RetailFirstPersonHookInstallResult& operator=(
        RetailFirstPersonHookInstallResult&&) = delete;

    bool complete() const noexcept
    {
        return failure == RetailFirstPersonHookInstallFailure::None
            && lease.installed();
    }

private:
    explicit RetailFirstPersonHookInstallResult(
        RetailFirstPersonHookLease&& installedLease) noexcept
        : lease(std::move(installedLease)),
          failure(RetailFirstPersonHookInstallFailure::None)
    {
    }

    friend RetailFirstPersonHookInstallResult installRetailFirstPersonHook(
        const RetailEngineCallAuthorization&,
        const RetailWorldAccumulationHookMemoryOperations&,
        std::uintptr_t,
        const RetailFirstPersonRelayAddresses&) noexcept;
};

namespace detail
{
inline RetailFirstPersonHookInstallResult retailFirstPersonHookInstallFailure(
    RetailFirstPersonHookInstallFailure failure,
    std::size_t failedCallSiteIndex,
    bool rollbackAttempted = false,
    bool rollbackComplete = true) noexcept
{
    RetailFirstPersonHookInstallResult result {};
    result.failure = failure;
    result.failedCallSiteIndex = failedCallSiteIndex;
    result.rollbackAttempted = rollbackAttempted;
    result.rollbackComplete = rollbackComplete;
    return result;
}

inline bool rollbackRetailFirstPersonPatches(
    const RetailWorldAccumulationHookMemoryOperations& memory,
    const std::array<
        RetailFirstPersonHookPatch,
        RetailFirstPersonCallSiteContractInventory.size()>& patches,
    std::size_t appliedCount) noexcept
{
    bool complete = true;
    while (appliedCount != 0u)
    {
        --appliedCount;
        const RetailFirstPersonHookUninstallResult restored =
            restoreRetailFirstPersonPatch(memory, patches[appliedCount], appliedCount);
        complete = restored.complete && complete;
    }
    return complete;
}
}

inline RetailFirstPersonHookInstallResult installRetailFirstPersonHook(
    const RetailEngineCallAuthorization& authorization,
    const RetailWorldAccumulationHookMemoryOperations& memory,
    std::uintptr_t runtimeImageBase,
    const RetailFirstPersonRelayAddresses& relayAddresses) noexcept
{
    using Failure = RetailFirstPersonHookInstallFailure;
    constexpr std::size_t CallSiteCount =
        RetailFirstPersonCallSiteContractInventory.size();
    if (!detail::RetailEngineCallAuthorizationAccess::authorized(
            authorization, runtimeImageBase))
    {
        return detail::retailFirstPersonHookInstallFailure(
            Failure::Unauthorized, CallSiteCount);
    }
    if (!retailFirstPersonCallSiteContractInventoryProductionProven()
        || !retailWorldAccumulationHookMemoryOperationsComplete(memory))
    {
        return detail::retailFirstPersonHookInstallFailure(
            Failure::MemoryOperationsIncomplete, CallSiteCount);
    }
    if (!relayAddresses.complete())
    {
        return detail::retailFirstPersonHookInstallFailure(
            Failure::InvalidAdapterAddress, CallSiteCount);
    }

    std::array<RetailFirstPersonHookPatch, CallSiteCount> patches {};
    for (std::size_t index = 0u; index < CallSiteCount; ++index)
    {
        const RetailFirstPersonCallSiteContract& contract =
            RetailFirstPersonCallSiteContractInventory[index];
        const std::uintptr_t relayAddress =
            relayAddresses.relayForCallSite(index);
        if (relayAddress == 0u || relayAddress > 0xFFFFFFFFu)
        {
            return detail::retailFirstPersonHookInstallFailure(
                Failure::InvalidAdapterAddress, index);
        }
        std::uintptr_t runtimeCallAddress = 0u;
        if (!relocateRetailFirstPersonPreferredAddress(
                runtimeImageBase,
                contract.preferredCallAddress,
                runtimeCallAddress)
            || runtimeCallAddress == 0u
            || runtimeCallAddress > 0xFFFFFFFFu)
        {
            return detail::retailFirstPersonHookInstallFailure(
                Failure::CallSiteAddressRelocationFailed, index);
        }
        RetailFirstPersonHookPatch& patch = patches[index];
        patch.address = static_cast<std::uint32_t>(runtimeCallAddress);
        if (!memory.read(
                memory.context,
                patch.address,
                patch.original.data(),
                patch.original.size()))
        {
            return detail::retailFirstPersonHookInstallFailure(
                Failure::CallSiteReadFailed, index);
        }
        if (!observeRetailFirstPersonCallSite(
                patch.original.data(),
                patch.original.size(),
                patch.address,
                runtimeImageBase,
                contract)
                 .complete())
        {
            return detail::retailFirstPersonHookInstallFailure(
                Failure::CallSiteContractMismatch, index);
        }
        const RetailFirstPersonRelativeCall replacement =
            encodeRetailFirstPersonX86Call(patch.address, relayAddress);
        if (!replacement.valid)
        {
            return detail::retailFirstPersonHookInstallFailure(
                Failure::PatchPlanFailed, index);
        }
        patch.replacement = replacement.bytes;
    }

    std::size_t appliedCount = 0u;
    for (; appliedCount < patches.size(); ++appliedCount)
    {
        const detail::RetailFirstPersonPatchApplyResult applied =
            detail::applyRetailFirstPersonPatch(memory, patches[appliedCount]);
        if (applied.failure != Failure::None)
        {
            const bool previousRestored = detail::rollbackRetailFirstPersonPatches(
                memory, patches, appliedCount);
            return detail::retailFirstPersonHookInstallFailure(
                applied.failure,
                appliedCount,
                true,
                applied.cleanupComplete && previousRestored);
        }
    }
    return RetailFirstPersonHookInstallResult(
        RetailFirstPersonHookLease(memory, patches));
}
}
