#pragma once

#include "fnvxr_retail_world_hook_lease.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace fnvxr::engine
{
// A deliberately narrow writer contract for the seven audited accumulation and
// render-phase E8 calls in RenderWorldSceneGraph. It has no allocation or
// arbitrary-memory operation: the Win32 implementation accepts only one of
// those five-byte instructions.
struct RetailWorldAccumulationHookMemoryOperations
{
    void* context = nullptr;
    bool (*read)(
        void*,
        std::uint32_t,
        std::uint8_t*,
        std::size_t) noexcept = nullptr;
    RetailWorldHookProtectionLease (*makeWritable)(
        void*,
        std::uint32_t,
        std::size_t) noexcept = nullptr;
    bool (*restoreProtection)(
        void*,
        const RetailWorldHookProtectionLease&) noexcept = nullptr;
    bool (*write)(
        void*,
        std::uint32_t,
        const std::uint8_t*,
        std::size_t) noexcept = nullptr;
    bool (*flushInstructionCache)(
        void*,
        std::uint32_t,
        std::size_t) noexcept = nullptr;
};

constexpr bool retailWorldAccumulationHookMemoryOperationsComplete(
    const RetailWorldAccumulationHookMemoryOperations& operations) noexcept
{
    return operations.context
        && operations.read
        && operations.makeWritable
        && operations.restoreProtection
        && operations.write
        && operations.flushInstructionCache;
}

enum class RetailWorldAccumulationHookInstallFailure : std::uint8_t
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

enum class RetailWorldAccumulationHookUninstallFailure : std::uint8_t
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

struct RetailWorldAccumulationHookPatch
{
    std::uint32_t address = 0u;
    std::array<std::uint8_t, RetailWorldAccumulationCallPatchByteCount>
        original {};
    std::array<std::uint8_t, RetailWorldAccumulationCallPatchByteCount>
        replacement {};
};

struct RetailWorldAccumulationHookUninstallResult
{
    bool complete = false;
    RetailWorldAccumulationHookUninstallFailure failure =
        RetailWorldAccumulationHookUninstallFailure::NotInstalled;
    bool rollbackAttempted = false;
    bool rollbackComplete = true;
    std::size_t failedCallSiteIndex =
        RetailWorldAccumulationCallSiteContractInventory.size();
};

class RetailWorldAccumulationHookLease;
struct RetailWorldAccumulationHookInstallResult;

RetailWorldAccumulationHookInstallResult installRetailWorldAccumulationHook(
    const RetailWorldHookAuthorization&,
    const RetailWorldAccumulationHookMemoryOperations&,
    std::uint32_t,
    std::uintptr_t,
    std::uintptr_t,
    std::uintptr_t,
    std::uintptr_t) noexcept;

namespace detail
{
inline bool retailWorldAccumulationBytesMatch(
    const std::uint8_t* actual,
    const std::uint8_t* expected,
    std::size_t byteCount) noexcept
{
    return retailWorldHookBytesMatch(actual, expected, byteCount);
}

inline bool restoreRetailWorldAccumulationPatchWhileWritable(
    const RetailWorldAccumulationHookMemoryOperations& memory,
    const RetailWorldHookProtectionLease& protection,
    const RetailWorldAccumulationHookPatch& patch) noexcept
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
    std::array<std::uint8_t, RetailWorldAccumulationCallPatchByteCount>
        verification {};
    const bool read = memory.read(
        memory.context,
        patch.address,
        verification.data(),
        verification.size());
    complete = read
        && retailWorldAccumulationBytesMatch(
            verification.data(),
            patch.original.data(),
            patch.original.size())
        && complete;
    return memory.restoreProtection(memory.context, protection) && complete;
}

struct RetailWorldAccumulationPatchApplyResult
{
    RetailWorldAccumulationHookInstallFailure failure =
        RetailWorldAccumulationHookInstallFailure::None;
    bool patchInstalled = false;
    bool cleanupComplete = true;
};

inline RetailWorldAccumulationPatchApplyResult
applyRetailWorldAccumulationPatch(
    const RetailWorldAccumulationHookMemoryOperations& memory,
    const RetailWorldAccumulationHookPatch& patch) noexcept
{
    using Failure = RetailWorldAccumulationHookInstallFailure;
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

    std::array<std::uint8_t, RetailWorldAccumulationCallPatchByteCount>
        current {};
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
    if (!retailWorldAccumulationBytesMatch(
            current.data(),
            patch.original.data(),
            patch.original.size()))
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
            restoreRetailWorldAccumulationPatchWhileWritable(
                memory,
                protection,
                patch),
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
            restoreRetailWorldAccumulationPatchWhileWritable(
                memory,
                protection,
                patch),
        };
    }
    std::array<std::uint8_t, RetailWorldAccumulationCallPatchByteCount>
        verification {};
    if (!memory.read(
            memory.context,
            patch.address,
            verification.data(),
            verification.size()))
    {
        return {
            Failure::CallSiteVerificationReadFailed,
            false,
            restoreRetailWorldAccumulationPatchWhileWritable(
                memory,
                protection,
                patch),
        };
    }
    if (!retailWorldAccumulationBytesMatch(
            verification.data(),
            patch.replacement.data(),
            patch.replacement.size()))
    {
        return {
            Failure::CallSiteVerificationMismatch,
            false,
            restoreRetailWorldAccumulationPatchWhileWritable(
                memory,
                protection,
                patch),
        };
    }
    if (!memory.restoreProtection(memory.context, protection))
    {
        return {
            Failure::CallSiteProtectionRestoreFailed,
            false,
            restoreRetailWorldAccumulationPatchWhileWritable(
                memory,
                protection,
                patch),
        };
    }
    return { Failure::None, true, true };
}

inline RetailWorldAccumulationHookUninstallResult
restoreRetailWorldAccumulationPatch(
    const RetailWorldAccumulationHookMemoryOperations& memory,
    const RetailWorldAccumulationHookPatch& patch,
    std::size_t callSiteIndex) noexcept
{
    using Failure = RetailWorldAccumulationHookUninstallFailure;
    const RetailWorldHookProtectionLease protection = memory.makeWritable(
        memory.context,
        patch.address,
        patch.original.size());
    if (!protection.ownershipTransferred)
    {
        return { false, Failure::CallSiteProtectionFailed, true, true,
            callSiteIndex };
    }
    if (!protection.usable())
    {
        const bool restored = memory.restoreProtection(memory.context, protection);
        return { false, Failure::CallSiteProtectionLeaseMalformed, true,
            restored, callSiteIndex };
    }
    std::array<std::uint8_t, RetailWorldAccumulationCallPatchByteCount>
        current {};
    if (!memory.read(
            memory.context,
            patch.address,
            current.data(),
            current.size()))
    {
        const bool restored = memory.restoreProtection(memory.context, protection);
        return { false, Failure::CallSiteReadFailed, true, restored,
            callSiteIndex };
    }
    const bool originalPresent = retailWorldAccumulationBytesMatch(
        current.data(), patch.original.data(), patch.original.size());
    const bool replacementPresent = retailWorldAccumulationBytesMatch(
        current.data(), patch.replacement.data(), patch.replacement.size());
    if (!originalPresent && !replacementPresent)
    {
        const bool restored = memory.restoreProtection(memory.context, protection);
        return { false, Failure::CallSiteReplaced, true, restored,
            callSiteIndex };
    }
    if (originalPresent)
    {
        const bool restored = memory.restoreProtection(memory.context, protection);
        return restored
            ? RetailWorldAccumulationHookUninstallResult {
                true, Failure::None, true, true, callSiteIndex }
            : RetailWorldAccumulationHookUninstallResult {
                false, Failure::CallSiteProtectionRestoreFailed, true, false,
                callSiteIndex };
    }
    if (!memory.write(
            memory.context,
            patch.address,
            patch.original.data(),
            patch.original.size()))
    {
        return { false, Failure::OriginalWriteFailed, true,
            restoreRetailWorldAccumulationPatchWhileWritable(
                memory, protection, patch), callSiteIndex };
    }
    if (!memory.flushInstructionCache(
            memory.context,
            patch.address,
            patch.original.size()))
    {
        return { false, Failure::OriginalFlushFailed, true,
            restoreRetailWorldAccumulationPatchWhileWritable(
                memory, protection, patch), callSiteIndex };
    }
    std::array<std::uint8_t, RetailWorldAccumulationCallPatchByteCount>
        verification {};
    if (!memory.read(
            memory.context,
            patch.address,
            verification.data(),
            verification.size()))
    {
        return { false, Failure::OriginalVerificationReadFailed, true,
            restoreRetailWorldAccumulationPatchWhileWritable(
                memory, protection, patch), callSiteIndex };
    }
    if (!retailWorldAccumulationBytesMatch(
            verification.data(),
            patch.original.data(),
            patch.original.size()))
    {
        return { false, Failure::OriginalVerificationMismatch, true,
            restoreRetailWorldAccumulationPatchWhileWritable(
                memory, protection, patch), callSiteIndex };
    }
    if (!memory.restoreProtection(memory.context, protection))
    {
        return { false, Failure::CallSiteProtectionRestoreFailed, true,
            restoreRetailWorldAccumulationPatchWhileWritable(
                memory, protection, patch), callSiteIndex };
    }
    return { true, Failure::None, true, true, callSiteIndex };
}
}

class RetailWorldAccumulationHookLease final
{
public:
    constexpr RetailWorldAccumulationHookLease() noexcept = default;

    RetailWorldAccumulationHookLease(const RetailWorldAccumulationHookLease&) =
        delete;
    RetailWorldAccumulationHookLease& operator=(
        const RetailWorldAccumulationHookLease&) = delete;

    RetailWorldAccumulationHookLease(
        RetailWorldAccumulationHookLease&& other) noexcept
    {
        moveFrom(other);
    }

    RetailWorldAccumulationHookLease& operator=(
        RetailWorldAccumulationHookLease&&) = delete;

    ~RetailWorldAccumulationHookLease() noexcept
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
        RetailWorldAccumulationHookPatch,
        RetailWorldAccumulationCallSiteContractInventory.size()>& patches()
        const noexcept
    {
        return mPatches;
    }

    RetailWorldAccumulationHookUninstallResult uninstall() noexcept
    {
        using Failure = RetailWorldAccumulationHookUninstallFailure;
        if (mState == State::Empty)
            return {};

        RetailWorldAccumulationHookUninstallResult firstFailure {};
        bool complete = true;
        for (std::size_t reverse = mPatches.size(); reverse != 0u; --reverse)
        {
            const std::size_t index = reverse - 1u;
            const RetailWorldAccumulationHookUninstallResult result =
                detail::restoreRetailWorldAccumulationPatch(
                    mMemory,
                    mPatches[index],
                    index);
            if (!result.complete && complete)
                firstFailure = result;
            complete = result.complete && complete;
        }
        if (complete)
        {
            clear();
            return { true, Failure::None, true, true,
                RetailWorldAccumulationCallSiteContractInventory.size() };
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

    RetailWorldAccumulationHookLease(
        const RetailWorldAccumulationHookMemoryOperations& memory,
        const std::array<
            RetailWorldAccumulationHookPatch,
            RetailWorldAccumulationCallSiteContractInventory.size()>& patches)
        noexcept
        : mMemory(memory),
          mPatches(patches),
          mState(State::Installed),
          mIntegrityKnown(true)
    {
    }

    void moveFrom(RetailWorldAccumulationHookLease& other) noexcept
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
        RetailWorldAccumulationHookPatch,
        RetailWorldAccumulationCallSiteContractInventory.size()> mPatches {};
    State mState = State::Empty;
    bool mIntegrityKnown = false;

    friend struct RetailWorldAccumulationHookInstallResult;
    friend RetailWorldAccumulationHookInstallResult
    installRetailWorldAccumulationHook(
        const RetailWorldHookAuthorization&,
        const RetailWorldAccumulationHookMemoryOperations&,
        std::uint32_t,
        std::uintptr_t,
        std::uintptr_t,
        std::uintptr_t,
        std::uintptr_t) noexcept;
};

struct RetailWorldAccumulationHookInstallResult
{
    RetailWorldAccumulationHookLease lease {};
    RetailWorldAccumulationHookInstallFailure failure =
        RetailWorldAccumulationHookInstallFailure::Unauthorized;
    bool rollbackAttempted = false;
    bool rollbackComplete = true;
    std::size_t failedCallSiteIndex =
        RetailWorldAccumulationCallSiteContractInventory.size();

    RetailWorldAccumulationHookInstallResult() noexcept = default;
    RetailWorldAccumulationHookInstallResult(
        const RetailWorldAccumulationHookInstallResult&) = delete;
    RetailWorldAccumulationHookInstallResult& operator=(
        const RetailWorldAccumulationHookInstallResult&) = delete;
    RetailWorldAccumulationHookInstallResult(
        RetailWorldAccumulationHookInstallResult&&) noexcept = default;
    RetailWorldAccumulationHookInstallResult& operator=(
        RetailWorldAccumulationHookInstallResult&&) = delete;

    bool complete() const noexcept
    {
        return failure == RetailWorldAccumulationHookInstallFailure::None
            && lease.installed();
    }

private:
    explicit RetailWorldAccumulationHookInstallResult(
        RetailWorldAccumulationHookLease&& installedLease) noexcept
        : lease(std::move(installedLease)),
          failure(RetailWorldAccumulationHookInstallFailure::None)
    {
    }

    friend RetailWorldAccumulationHookInstallResult
    installRetailWorldAccumulationHook(
        const RetailWorldHookAuthorization&,
        const RetailWorldAccumulationHookMemoryOperations&,
        std::uint32_t,
        std::uintptr_t,
        std::uintptr_t,
        std::uintptr_t,
        std::uintptr_t) noexcept;
};

namespace detail
{
inline RetailWorldAccumulationHookInstallResult
retailWorldAccumulationHookInstallFailure(
    RetailWorldAccumulationHookInstallFailure failure,
    std::size_t failedCallSiteIndex,
    bool rollbackAttempted = false,
    bool rollbackComplete = true) noexcept
{
    RetailWorldAccumulationHookInstallResult result {};
    result.failure = failure;
    result.failedCallSiteIndex = failedCallSiteIndex;
    result.rollbackAttempted = rollbackAttempted;
    result.rollbackComplete = rollbackComplete;
    return result;
}

inline bool rollbackRetailWorldAccumulationPatches(
    const RetailWorldAccumulationHookMemoryOperations& memory,
    const std::array<
        RetailWorldAccumulationHookPatch,
        RetailWorldAccumulationCallSiteContractInventory.size()>& patches,
    std::size_t appliedCount) noexcept
{
    bool complete = true;
    while (appliedCount != 0u)
    {
        --appliedCount;
        const RetailWorldAccumulationHookUninstallResult result =
            restoreRetailWorldAccumulationPatch(
                memory,
                patches[appliedCount],
                appliedCount);
        complete = result.complete && complete;
    }
    return complete;
}
}

inline RetailWorldAccumulationHookInstallResult
installRetailWorldAccumulationHook(
    const RetailWorldHookAuthorization& authorization,
    const RetailWorldAccumulationHookMemoryOperations& memory,
    std::uint32_t runtimeWorldAddress,
    std::uintptr_t accumulationAdapterAddress,
    std::uintptr_t renderWithoutFinalizeAdapterAddress,
    std::uintptr_t finalizeAdapterAddress,
    std::uintptr_t renderAndFinalizeAdapterAddress) noexcept
{
    using Failure = RetailWorldAccumulationHookInstallFailure;
    constexpr std::size_t CallSiteCount =
        RetailWorldAccumulationCallSiteContractInventory.size();
    if (!detail::RetailWorldHookAuthorizationAccess::authorized(
            authorization,
            runtimeWorldAddress))
    {
        return detail::retailWorldAccumulationHookInstallFailure(
            Failure::Unauthorized,
            CallSiteCount);
    }
    if (!retailWorldAccumulationHookMemoryOperationsComplete(memory))
    {
        return detail::retailWorldAccumulationHookInstallFailure(
            Failure::MemoryOperationsIncomplete,
            CallSiteCount);
    }
    if (accumulationAdapterAddress == 0u
        || accumulationAdapterAddress > 0xFFFFFFFFu
        || renderWithoutFinalizeAdapterAddress == 0u
        || renderWithoutFinalizeAdapterAddress > 0xFFFFFFFFu
        || finalizeAdapterAddress == 0u
        || finalizeAdapterAddress > 0xFFFFFFFFu
        || renderAndFinalizeAdapterAddress == 0u
        || renderAndFinalizeAdapterAddress > 0xFFFFFFFFu)
    {
        return detail::retailWorldAccumulationHookInstallFailure(
            Failure::InvalidAdapterAddress,
            CallSiteCount);
    }

    std::array<RetailWorldAccumulationHookPatch, CallSiteCount> patches {};
    for (std::size_t index = 0u; index < CallSiteCount; ++index)
    {
        const RetailWorldAccumulationCallSiteContract& contract =
            RetailWorldAccumulationCallSiteContractInventory[index];
        std::uintptr_t runtimeCallAddress = 0u;
        if (!relocateRetailWorldPreferredAddress(
                runtimeWorldAddress,
                contract.preferredCallAddress,
                runtimeCallAddress)
            || runtimeCallAddress == 0u
            || runtimeCallAddress > 0xFFFFFFFFu)
        {
            return detail::retailWorldAccumulationHookInstallFailure(
                Failure::CallSiteAddressRelocationFailed,
                index);
        }
        RetailWorldAccumulationHookPatch& patch = patches[index];
        patch.address = static_cast<std::uint32_t>(runtimeCallAddress);
        if (!memory.read(
                memory.context,
                patch.address,
                patch.original.data(),
                patch.original.size()))
        {
            return detail::retailWorldAccumulationHookInstallFailure(
                Failure::CallSiteReadFailed,
                index);
        }
        if (!observeRetailWorldAccumulationCallSite(
                patch.original.data(),
                patch.original.size(),
                patch.address,
                runtimeWorldAddress,
                contract)
                 .complete())
        {
            return detail::retailWorldAccumulationHookInstallFailure(
                Failure::CallSiteContractMismatch,
                index);
        }
        std::uintptr_t adapterAddress = accumulationAdapterAddress;
        switch (contract.relayKind)
        {
        case RetailWorldAccumulationCallSiteContract::RelayKind::
            AccumulateScene:
            adapterAddress = accumulationAdapterAddress;
            break;
        case RetailWorldAccumulationCallSiteContract::RelayKind::
            RenderWithoutFinalize:
            adapterAddress = renderWithoutFinalizeAdapterAddress;
            break;
        case RetailWorldAccumulationCallSiteContract::RelayKind::FinalizeOnly:
            adapterAddress = finalizeAdapterAddress;
            break;
        case RetailWorldAccumulationCallSiteContract::RelayKind::
            RenderAndFinalize:
            adapterAddress = renderAndFinalizeAdapterAddress;
            break;
        }
        const RetailWorldRelativeCall replacement = encodeRetailWorldX86Call(
            patch.address,
            adapterAddress);
        if (!replacement.valid)
        {
            return detail::retailWorldAccumulationHookInstallFailure(
                Failure::PatchPlanFailed,
                index);
        }
        patch.replacement = replacement.bytes;
    }

    std::size_t appliedCount = 0u;
    for (; appliedCount < patches.size(); ++appliedCount)
    {
        const detail::RetailWorldAccumulationPatchApplyResult applied =
            detail::applyRetailWorldAccumulationPatch(
                memory,
                patches[appliedCount]);
        if (applied.failure != Failure::None)
        {
            const bool previousRestored =
                detail::rollbackRetailWorldAccumulationPatches(
                    memory,
                    patches,
                    appliedCount);
            return detail::retailWorldAccumulationHookInstallFailure(
                applied.failure,
                appliedCount,
                true,
                applied.cleanupComplete && previousRestored);
        }
    }
    return RetailWorldAccumulationHookInstallResult(
        RetailWorldAccumulationHookLease(memory, patches));
}
}
