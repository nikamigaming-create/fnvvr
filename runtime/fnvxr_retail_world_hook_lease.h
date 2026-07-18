#pragma once

#include "fnvxr_retail_world_hook_contract.h"
#include "fnvxr_retail_runtime_binding.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace fnvxr::engine
{
struct RetailWorldHookExecutableAllocation
{
    // ownershipTransferred is authoritative even when the returned address or
    // token is malformed.  An installer must still ask the backend to release
    // such a partial acquisition.
    bool ownershipTransferred = false;
    std::uint32_t address = 0u;
    std::uintptr_t cleanupToken = 0u;

    constexpr bool usable() const noexcept
    {
        return ownershipTransferred && address != 0u && cleanupToken != 0u;
    }
};

struct RetailWorldHookProtectionLease
{
    // On a failed restoreProtection call, the backend contract guarantees the
    // lease remains owned and the range remains writable so rollback can
    // restore bytes and retry the protection restoration.
    bool ownershipTransferred = false;
    std::uintptr_t restoreToken = 0u;

    constexpr bool usable() const noexcept
    {
        return ownershipTransferred && restoreToken != 0u;
    }
};

struct RetailWorldHookMemoryOperations
{
    void* context = nullptr;
    bool (*read)(
        void*,
        std::uint32_t,
        std::uint8_t*,
        std::size_t) noexcept = nullptr;
    RetailWorldHookExecutableAllocation (*allocateExecutable)(
        void*,
        std::size_t) noexcept = nullptr;
    bool (*releaseExecutable)(
        void*,
        const RetailWorldHookExecutableAllocation&) noexcept = nullptr;
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

constexpr bool retailWorldHookMemoryOperationsComplete(
    const RetailWorldHookMemoryOperations& operations) noexcept
{
    return operations.context
        && operations.read
        && operations.allocateExecutable
        && operations.releaseExecutable
        && operations.makeWritable
        && operations.restoreProtection
        && operations.write
        && operations.flushInstructionCache;
}

struct RetailWorldHookDispatchFrame
{
    void* retailThis = nullptr;
    RetailWorldRenderStackArguments arguments {};
    std::uint32_t originalTrampolineAddress = 0u;
};

struct RetailWorldHookDispatchOperations
{
    void* context = nullptr;
    void (*dispatch)(
        void*,
        const RetailWorldHookDispatchFrame&) noexcept = nullptr;
};

constexpr bool retailWorldHookDispatchOperationsComplete(
    const RetailWorldHookDispatchOperations& operations) noexcept
{
    return operations.context && operations.dispatch;
}

// Future x86 glue uses RetailWorldRenderHookFunction from the contract as its
// __fastcall entry and forwards the six values to dispatchFromFastcallAdapter.
// This seam invokes only the supplied engine-world dispatcher.  It contains no
// reference to, and cannot invoke, the retired D3D replay implementation.

class RetailWorldHookAuthorization;

namespace detail
{
struct RetailWorldHookAuthorizationAccess;
}

struct RetailWorldHookLeaseTestAuthority;

class RetailWorldHookAuthorization final
{
public:
    constexpr RetailWorldHookAuthorization() noexcept = default;

private:
    explicit constexpr RetailWorldHookAuthorization(
        std::uint32_t authorizedWorldAddress) noexcept
        : mAuthorizedWorldAddress(authorizedWorldAddress),
          mIssued(authorizedWorldAddress != 0u)
    {
    }

    explicit constexpr RetailWorldHookAuthorization(
        std::uint32_t authorizedWorldAddress,
        const detail::RetailRuntimeBinding& binding,
        detail::RetailRuntimeBindingValidator validator) noexcept
        : mAuthorizedWorldAddress(authorizedWorldAddress),
          mIssued(authorizedWorldAddress != 0u),
          mBinding(binding),
          mBindingValidator(validator)
    {
    }

    std::uint32_t mAuthorizedWorldAddress = 0u;
    bool mIssued = false;
    detail::RetailRuntimeBinding mBinding {};
    detail::RetailRuntimeBindingValidator mBindingValidator = nullptr;

    friend struct detail::RetailWorldHookAuthorizationAccess;
    friend struct detail::RetailRuntimeAuthorityIssuer;
    friend struct RetailWorldHookLeaseTestAuthority;
};

inline constexpr bool RetailWorldHookProductionAuthorizationAvailable =
    RetailRuntimeProductionAuthorizationAvailable;
inline constexpr bool RetailWorldHookInstallerEmitsX86CodeOnly = true;

namespace detail
{
struct RetailWorldHookAuthorizationAccess
{
    static constexpr bool authorized(
        const RetailWorldHookAuthorization& authorization,
        std::uint32_t requestedWorldAddress) noexcept
    {
        return authorization.mIssued
            && authorization.mAuthorizedWorldAddress != 0u
            && authorization.mAuthorizedWorldAddress == requestedWorldAddress
            && (!authorization.mBindingValidator
                || authorization.mBindingValidator(authorization.mBinding));
    }
};

inline bool retailWorldHookBytesMatch(
    const std::uint8_t* actual,
    const std::uint8_t* expected,
    std::size_t byteCount) noexcept
{
    if (!actual || !expected)
        return false;
    std::uint8_t difference = 0u;
    for (std::size_t index = 0u; index < byteCount; ++index)
        difference |= static_cast<std::uint8_t>(actual[index] ^ expected[index]);
    return difference == 0u;
}
}

enum class RetailWorldHookInstallFailure : std::uint8_t
{
    None,
    Unauthorized,
    MemoryOperationsIncomplete,
    DispatchOperationsIncomplete,
    InvalidAdapterAddress,
    WorldBodyReadFailed,
    WorldSeamMismatch,
    TrampolineAllocationFailed,
    TrampolineAllocationMalformed,
    HookPlanFailed,
    TrampolineWriteFailed,
    TrampolineFlushFailed,
    TrampolineVerificationReadFailed,
    TrampolineVerificationMismatch,
    EntryProtectionFailed,
    EntryProtectionLeaseMalformed,
    EntryRevalidationReadFailed,
    EntryChangedBeforePatch,
    EntryWriteFailed,
    EntryFlushFailed,
    EntryVerificationReadFailed,
    EntryVerificationMismatch,
    EntryProtectionRestoreFailed,
};

enum class RetailWorldHookUninstallFailure : std::uint8_t
{
    None,
    NotInstalled,
    EntryProtectionFailed,
    EntryProtectionLeaseMalformed,
    EntryReadFailed,
    EntryReplaced,
    OriginalWriteFailed,
    OriginalFlushFailed,
    OriginalVerificationReadFailed,
    OriginalVerificationMismatch,
    EntryProtectionRestoreFailed,
    TrampolineReleaseFailed,
};

struct RetailWorldHookUninstallResult
{
    bool complete = false;
    RetailWorldHookUninstallFailure failure =
        RetailWorldHookUninstallFailure::NotInstalled;
    bool rollbackAttempted = false;
    bool rollbackComplete = true;
};

class RetailWorldHookLease final
{
public:
    constexpr RetailWorldHookLease() noexcept = default;

    RetailWorldHookLease(const RetailWorldHookLease&) = delete;
    RetailWorldHookLease& operator=(const RetailWorldHookLease&) = delete;

    RetailWorldHookLease(RetailWorldHookLease&& other) noexcept
    {
        moveFrom(other);
    }

    RetailWorldHookLease& operator=(RetailWorldHookLease&&) = delete;

    ~RetailWorldHookLease() noexcept
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

    bool cleanupPending() const noexcept
    {
        return mState == State::CleanupPending;
    }

    std::uint32_t worldAddress() const noexcept
    {
        return ownsResources() ? mWorldAddress : 0u;
    }

    std::uint32_t trampolineAddress() const noexcept
    {
        return ownsResources() ? mAllocation.address : 0u;
    }

    bool dispatchFromFastcallAdapter(
        void* retailThis,
        void* sharedRenderObject,
        std::uint32_t callerModePredicate,
        std::uint32_t stockWorldPathSelector,
        std::uint32_t postWorldOption) const noexcept
    {
        if (!installed()
            || !retailWorldHookDispatchOperationsComplete(mDispatch))
        {
            return false;
        }

        const std::uintptr_t sharedAddress =
            reinterpret_cast<std::uintptr_t>(sharedRenderObject);
        if (sharedAddress > 0xFFFFFFFFu)
            return false;

        RetailWorldHookDispatchFrame frame {};
        frame.retailThis = retailThis;
        frame.arguments.sharedRenderObjectAddress =
            static_cast<std::uint32_t>(sharedAddress);
        frame.arguments.callerModePredicate = callerModePredicate;
        frame.arguments.stockWorldPathSelector = stockWorldPathSelector;
        frame.arguments.postWorldOption = postWorldOption;
        frame.originalTrampolineAddress = mAllocation.address;
        mDispatch.dispatch(mDispatch.context, frame);
        return true;
    }

    RetailWorldHookUninstallResult uninstall() noexcept
    {
        if (mState == State::Empty)
            return {};

        if (mState == State::CleanupPending)
        {
            if (!mMemory.releaseExecutable(mMemory.context, mAllocation))
            {
                return {
                    false,
                    RetailWorldHookUninstallFailure::TrampolineReleaseFailed,
                    true,
                    false,
                };
            }
            clear();
            return {
                true,
                RetailWorldHookUninstallFailure::None,
                true,
                true,
            };
        }

        const RetailWorldHookProtectionLease protection =
            mMemory.makeWritable(
                mMemory.context,
                mWorldAddress,
                RetailWorldHookPatchByteCount);
        if (!protection.ownershipTransferred)
        {
            return {
                false,
                RetailWorldHookUninstallFailure::EntryProtectionFailed,
                false,
                true,
            };
        }
        if (!protection.usable())
        {
            const bool restored = mMemory.restoreProtection(
                mMemory.context,
                protection);
            mIntegrityKnown = false;
            return {
                false,
                RetailWorldHookUninstallFailure::EntryProtectionLeaseMalformed,
                true,
                restored,
            };
        }

        std::array<std::uint8_t, RetailWorldHookPatchByteCount> current {};
        if (!mMemory.read(
                mMemory.context,
                mWorldAddress,
                current.data(),
                current.size()))
        {
            const bool restored = mMemory.restoreProtection(
                mMemory.context,
                protection);
            mIntegrityKnown = false;
            return {
                false,
                RetailWorldHookUninstallFailure::EntryReadFailed,
                true,
                restored,
            };
        }

        const bool entryIsPatch = detail::retailWorldHookBytesMatch(
            current.data(),
            mPatch.data(),
            mPatch.size());
        const bool entryIsOriginal = detail::retailWorldHookBytesMatch(
            current.data(),
            mOriginal.data(),
            mOriginal.size());
        if (!entryIsPatch && !entryIsOriginal)
        {
            const bool restored = mMemory.restoreProtection(
                mMemory.context,
                protection);
            mIntegrityKnown = false;
            return {
                false,
                RetailWorldHookUninstallFailure::EntryReplaced,
                true,
                restored,
            };
        }

        if (entryIsOriginal)
        {
            const bool restored = mMemory.restoreProtection(
                mMemory.context,
                protection);
            if (!restored)
            {
                mIntegrityKnown = false;
                return {
                    false,
                    RetailWorldHookUninstallFailure::EntryProtectionRestoreFailed,
                    true,
                    false,
                };
            }
            return finishUninstallAfterOriginalRestored();
        }

        mIntegrityKnown = true;
        if (!mMemory.write(
                mMemory.context,
                mWorldAddress,
                mOriginal.data(),
                mOriginal.size()))
        {
            return rollbackUninstall(
                protection,
                RetailWorldHookUninstallFailure::OriginalWriteFailed);
        }
        if (!mMemory.flushInstructionCache(
                mMemory.context,
                mWorldAddress,
                mOriginal.size()))
        {
            return rollbackUninstall(
                protection,
                RetailWorldHookUninstallFailure::OriginalFlushFailed);
        }

        std::array<std::uint8_t, RetailWorldHookPatchByteCount> verification {};
        if (!mMemory.read(
                mMemory.context,
                mWorldAddress,
                verification.data(),
                verification.size()))
        {
            return rollbackUninstall(
                protection,
                RetailWorldHookUninstallFailure::OriginalVerificationReadFailed);
        }
        if (!detail::retailWorldHookBytesMatch(
                verification.data(),
                mOriginal.data(),
                mOriginal.size()))
        {
            return rollbackUninstall(
                protection,
                RetailWorldHookUninstallFailure::OriginalVerificationMismatch);
        }

        if (!mMemory.restoreProtection(mMemory.context, protection))
        {
            return rollbackUninstall(
                protection,
                RetailWorldHookUninstallFailure::EntryProtectionRestoreFailed);
        }
        return finishUninstallAfterOriginalRestored();
    }

private:
    enum class State : std::uint8_t
    {
        Empty,
        Installed,
        CleanupPending,
    };

    RetailWorldHookLease(
        const RetailWorldHookMemoryOperations& memory,
        const RetailWorldHookDispatchOperations& dispatch,
        const RetailWorldHookExecutableAllocation& allocation,
        std::uint32_t worldAddress,
        const std::array<std::uint8_t, RetailWorldHookPatchByteCount>& original,
        const std::array<std::uint8_t, RetailWorldHookPatchByteCount>& patch) noexcept
        : mMemory(memory),
          mDispatch(dispatch),
          mAllocation(allocation),
          mWorldAddress(worldAddress),
          mOriginal(original),
          mPatch(patch),
          mState(State::Installed),
          mIntegrityKnown(true)
    {
    }

    RetailWorldHookUninstallResult rollbackUninstall(
        const RetailWorldHookProtectionLease& protection,
        RetailWorldHookUninstallFailure failure) noexcept
    {
        bool complete = mMemory.write(
            mMemory.context,
            mWorldAddress,
            mPatch.data(),
            mPatch.size());
        if (!mMemory.flushInstructionCache(
                mMemory.context,
                mWorldAddress,
                mPatch.size()))
        {
            complete = false;
        }
        std::array<std::uint8_t, RetailWorldHookPatchByteCount> verification {};
        const bool read = mMemory.read(
            mMemory.context,
            mWorldAddress,
            verification.data(),
            verification.size());
        complete = read
            && detail::retailWorldHookBytesMatch(
                verification.data(),
                mPatch.data(),
                mPatch.size())
            && complete;
        const bool protectionRestored = mMemory.restoreProtection(
            mMemory.context,
            protection);
        complete = protectionRestored && complete;
        mIntegrityKnown = complete;
        return { false, failure, true, complete };
    }

    RetailWorldHookUninstallResult finishUninstallAfterOriginalRestored() noexcept
    {
        mState = State::CleanupPending;
        mIntegrityKnown = false;
        if (!mMemory.releaseExecutable(mMemory.context, mAllocation))
        {
            return {
                false,
                RetailWorldHookUninstallFailure::TrampolineReleaseFailed,
                true,
                false,
            };
        }
        clear();
        return {
            true,
            RetailWorldHookUninstallFailure::None,
            true,
            true,
        };
    }

    void moveFrom(RetailWorldHookLease& other) noexcept
    {
        mMemory = other.mMemory;
        mDispatch = other.mDispatch;
        mAllocation = other.mAllocation;
        mWorldAddress = other.mWorldAddress;
        mOriginal = other.mOriginal;
        mPatch = other.mPatch;
        mState = other.mState;
        mIntegrityKnown = other.mIntegrityKnown;
        other.clear();
    }

    void clear() noexcept
    {
        mMemory = {};
        mDispatch = {};
        mAllocation = {};
        mWorldAddress = 0u;
        mOriginal = {};
        mPatch = {};
        mState = State::Empty;
        mIntegrityKnown = false;
    }

    RetailWorldHookMemoryOperations mMemory {};
    RetailWorldHookDispatchOperations mDispatch {};
    RetailWorldHookExecutableAllocation mAllocation {};
    std::uint32_t mWorldAddress = 0u;
    std::array<std::uint8_t, RetailWorldHookPatchByteCount> mOriginal {};
    std::array<std::uint8_t, RetailWorldHookPatchByteCount> mPatch {};
    State mState = State::Empty;
    bool mIntegrityKnown = false;

    friend struct RetailWorldHookInstallResult;
    friend RetailWorldHookInstallResult installRetailWorldHook(
        const RetailWorldHookAuthorization&,
        const RetailWorldHookMemoryOperations&,
        const RetailWorldHookDispatchOperations&,
        std::uint32_t,
        std::uintptr_t) noexcept;
};

struct RetailWorldHookInstallResult
{
    RetailWorldHookLease lease {};
    RetailWorldHookInstallFailure failure =
        RetailWorldHookInstallFailure::Unauthorized;
    bool rollbackAttempted = false;
    bool rollbackComplete = true;

    RetailWorldHookInstallResult() noexcept = default;
    RetailWorldHookInstallResult(const RetailWorldHookInstallResult&) = delete;
    RetailWorldHookInstallResult& operator=(
        const RetailWorldHookInstallResult&) = delete;
    RetailWorldHookInstallResult(
        RetailWorldHookInstallResult&&) noexcept = default;
    RetailWorldHookInstallResult& operator=(
        RetailWorldHookInstallResult&&) = delete;

    bool complete() const noexcept
    {
        return failure == RetailWorldHookInstallFailure::None
            && lease.installed();
    }

private:
    explicit RetailWorldHookInstallResult(
        RetailWorldHookLease&& installedLease) noexcept
        : lease(std::move(installedLease)),
          failure(RetailWorldHookInstallFailure::None),
          rollbackAttempted(false),
          rollbackComplete(true)
    {
    }

    friend RetailWorldHookInstallResult installRetailWorldHook(
        const RetailWorldHookAuthorization&,
        const RetailWorldHookMemoryOperations&,
        const RetailWorldHookDispatchOperations&,
        std::uint32_t,
        std::uintptr_t) noexcept;
};

namespace detail
{
inline RetailWorldHookInstallResult retailWorldHookInstallFailure(
    RetailWorldHookInstallFailure failure,
    bool rollbackAttempted = false,
    bool rollbackComplete = true) noexcept
{
    RetailWorldHookInstallResult result {};
    result.failure = failure;
    result.rollbackAttempted = rollbackAttempted;
    result.rollbackComplete = rollbackComplete;
    return result;
}

inline RetailWorldHookInstallResult cleanupRetailWorldHookBeforeEntryWrite(
    RetailWorldHookInstallFailure failure,
    const RetailWorldHookMemoryOperations& memory,
    const RetailWorldHookExecutableAllocation& allocation,
    const RetailWorldHookProtectionLease* protection = nullptr) noexcept
{
    bool complete = true;
    bool attempted = allocation.ownershipTransferred;
    if (protection && protection->ownershipTransferred)
    {
        attempted = true;
        complete = memory.restoreProtection(memory.context, *protection)
            && complete;
    }
    if (allocation.ownershipTransferred)
    {
        complete = memory.releaseExecutable(memory.context, allocation)
            && complete;
    }
    return retailWorldHookInstallFailure(failure, attempted, complete);
}

inline RetailWorldHookInstallResult rollbackRetailWorldHookEntryWrite(
    RetailWorldHookInstallFailure failure,
    const RetailWorldHookMemoryOperations& memory,
    const RetailWorldHookExecutableAllocation& allocation,
    const RetailWorldHookProtectionLease& protection,
    std::uint32_t worldAddress) noexcept
{
    bool originalRestored = memory.write(
        memory.context,
        worldAddress,
        RetailWorldHookStolenBytes.data(),
        RetailWorldHookStolenBytes.size());
    if (!memory.flushInstructionCache(
            memory.context,
            worldAddress,
            RetailWorldHookStolenBytes.size()))
    {
        originalRestored = false;
    }
    std::array<std::uint8_t, RetailWorldHookPatchByteCount> verification {};
    const bool read = memory.read(
        memory.context,
        worldAddress,
        verification.data(),
        verification.size());
    originalRestored = read
        && retailWorldHookBytesMatch(
            verification.data(),
            RetailWorldHookStolenBytes.data(),
            RetailWorldHookStolenBytes.size())
        && originalRestored;
    const bool protectionRestored = memory.restoreProtection(
        memory.context,
        protection);
    bool allocationReleased = false;
    if (originalRestored)
    {
        allocationReleased = memory.releaseExecutable(
            memory.context,
            allocation);
    }
    return retailWorldHookInstallFailure(
        failure,
        true,
        originalRestored && protectionRestored && allocationReleased);
}
}

inline RetailWorldHookInstallResult installRetailWorldHook(
    const RetailWorldHookAuthorization& authorization,
    const RetailWorldHookMemoryOperations& memory,
    const RetailWorldHookDispatchOperations& dispatch,
    std::uint32_t runtimeWorldAddress,
    std::uintptr_t fastcallAdapterAddress) noexcept
{
    using Failure = RetailWorldHookInstallFailure;

    if (!detail::RetailWorldHookAuthorizationAccess::authorized(
            authorization,
            runtimeWorldAddress))
    {
        return detail::retailWorldHookInstallFailure(Failure::Unauthorized);
    }
    if (!retailWorldHookMemoryOperationsComplete(memory))
    {
        return detail::retailWorldHookInstallFailure(
            Failure::MemoryOperationsIncomplete);
    }
    if (!retailWorldHookDispatchOperationsComplete(dispatch))
    {
        return detail::retailWorldHookInstallFailure(
            Failure::DispatchOperationsIncomplete);
    }
    if (fastcallAdapterAddress == 0u || fastcallAdapterAddress > 0xFFFFFFFFu)
    {
        return detail::retailWorldHookInstallFailure(
            Failure::InvalidAdapterAddress);
    }

    std::array<std::uint8_t, RetailWorldFunctionByteCount> functionBytes {};
    if (!memory.read(
            memory.context,
            runtimeWorldAddress,
            functionBytes.data(),
            functionBytes.size()))
    {
        return detail::retailWorldHookInstallFailure(
            Failure::WorldBodyReadFailed);
    }
    if (!observeRetailWorldHookSeam(
            functionBytes.data(),
            functionBytes.size())
             .complete())
    {
        return detail::retailWorldHookInstallFailure(
            Failure::WorldSeamMismatch);
    }

    const RetailWorldHookExecutableAllocation allocation =
        memory.allocateExecutable(
            memory.context,
            RetailWorldHookTrampolineByteCount);
    if (!allocation.ownershipTransferred)
    {
        return detail::retailWorldHookInstallFailure(
            Failure::TrampolineAllocationFailed);
    }
    if (!allocation.usable())
    {
        return detail::cleanupRetailWorldHookBeforeEntryWrite(
            Failure::TrampolineAllocationMalformed,
            memory,
            allocation);
    }

    const RetailWorldHookPlan plan = buildRetailWorldHookPlan(
        functionBytes.data(),
        functionBytes.size(),
        runtimeWorldAddress,
        fastcallAdapterAddress,
        allocation.address);
    if (!plan.valid)
    {
        return detail::cleanupRetailWorldHookBeforeEntryWrite(
            Failure::HookPlanFailed,
            memory,
            allocation);
    }

    if (!memory.write(
            memory.context,
            allocation.address,
            plan.trampoline.data(),
            plan.trampoline.size()))
    {
        return detail::cleanupRetailWorldHookBeforeEntryWrite(
            Failure::TrampolineWriteFailed,
            memory,
            allocation);
    }
    if (!memory.flushInstructionCache(
            memory.context,
            allocation.address,
            plan.trampoline.size()))
    {
        return detail::cleanupRetailWorldHookBeforeEntryWrite(
            Failure::TrampolineFlushFailed,
            memory,
            allocation);
    }
    std::array<std::uint8_t, RetailWorldHookTrampolineByteCount>
        trampolineVerification {};
    if (!memory.read(
            memory.context,
            allocation.address,
            trampolineVerification.data(),
            trampolineVerification.size()))
    {
        return detail::cleanupRetailWorldHookBeforeEntryWrite(
            Failure::TrampolineVerificationReadFailed,
            memory,
            allocation);
    }
    if (!detail::retailWorldHookBytesMatch(
            trampolineVerification.data(),
            plan.trampoline.data(),
            plan.trampoline.size()))
    {
        return detail::cleanupRetailWorldHookBeforeEntryWrite(
            Failure::TrampolineVerificationMismatch,
            memory,
            allocation);
    }

    const RetailWorldHookProtectionLease protection = memory.makeWritable(
        memory.context,
        runtimeWorldAddress,
        RetailWorldHookPatchByteCount);
    if (!protection.ownershipTransferred)
    {
        return detail::cleanupRetailWorldHookBeforeEntryWrite(
            Failure::EntryProtectionFailed,
            memory,
            allocation);
    }
    if (!protection.usable())
    {
        return detail::cleanupRetailWorldHookBeforeEntryWrite(
            Failure::EntryProtectionLeaseMalformed,
            memory,
            allocation,
            &protection);
    }

    std::array<std::uint8_t, RetailWorldHookPatchByteCount> entry {};
    if (!memory.read(
            memory.context,
            runtimeWorldAddress,
            entry.data(),
            entry.size()))
    {
        return detail::cleanupRetailWorldHookBeforeEntryWrite(
            Failure::EntryRevalidationReadFailed,
            memory,
            allocation,
            &protection);
    }
    if (!detail::retailWorldHookBytesMatch(
            entry.data(),
            RetailWorldHookStolenBytes.data(),
            entry.size()))
    {
        return detail::cleanupRetailWorldHookBeforeEntryWrite(
            Failure::EntryChangedBeforePatch,
            memory,
            allocation,
            &protection);
    }

    if (!memory.write(
            memory.context,
            runtimeWorldAddress,
            plan.entryPatch.data(),
            plan.entryPatch.size()))
    {
        return detail::rollbackRetailWorldHookEntryWrite(
            Failure::EntryWriteFailed,
            memory,
            allocation,
            protection,
            runtimeWorldAddress);
    }
    if (!memory.flushInstructionCache(
            memory.context,
            runtimeWorldAddress,
            plan.entryPatch.size()))
    {
        return detail::rollbackRetailWorldHookEntryWrite(
            Failure::EntryFlushFailed,
            memory,
            allocation,
            protection,
            runtimeWorldAddress);
    }
    std::array<std::uint8_t, RetailWorldHookPatchByteCount> entryVerification {};
    if (!memory.read(
            memory.context,
            runtimeWorldAddress,
            entryVerification.data(),
            entryVerification.size()))
    {
        return detail::rollbackRetailWorldHookEntryWrite(
            Failure::EntryVerificationReadFailed,
            memory,
            allocation,
            protection,
            runtimeWorldAddress);
    }
    if (!detail::retailWorldHookBytesMatch(
            entryVerification.data(),
            plan.entryPatch.data(),
            plan.entryPatch.size()))
    {
        return detail::rollbackRetailWorldHookEntryWrite(
            Failure::EntryVerificationMismatch,
            memory,
            allocation,
            protection,
            runtimeWorldAddress);
    }
    if (!memory.restoreProtection(memory.context, protection))
    {
        return detail::rollbackRetailWorldHookEntryWrite(
            Failure::EntryProtectionRestoreFailed,
            memory,
            allocation,
            protection,
            runtimeWorldAddress);
    }

    return RetailWorldHookInstallResult(RetailWorldHookLease(
        memory,
        dispatch,
        allocation,
        runtimeWorldAddress,
        RetailWorldHookStolenBytes,
        plan.entryPatch));
}

static_assert(
    RetailWorldHookProductionAuthorizationAvailable
    == RetailRuntimeProductionAuthorizationAvailable);
static_assert(RetailWorldHookInstallerEmitsX86CodeOnly);
static_assert(std::is_default_constructible_v<RetailWorldHookAuthorization>);
static_assert(!std::is_constructible_v<RetailWorldHookAuthorization, bool>);
static_assert(
    !std::is_constructible_v<RetailWorldHookAuthorization, std::uint32_t>);
static_assert(!std::is_copy_constructible_v<RetailWorldHookLease>);
static_assert(std::is_nothrow_move_constructible_v<RetailWorldHookLease>);
}
