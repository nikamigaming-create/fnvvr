#pragma once

#include "fnvxr_retail_engine_abi.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace fnvxr::engine::geometry
{
using RetailGeometryPointer32 = fnvxr::engine::abi::RetailPointer32;
using PrivateGeometryThreadToken = std::uint64_t;

inline constexpr std::uint64_t PrivateGeometryStateHeadCanary =
    0xBADC0FFEE0DDF00Dull;
inline constexpr std::uint64_t PrivateGeometryStateTailCanary =
    0xC001D00D5AFECAFEull;
inline constexpr std::uint64_t PrivateGeometryBindingHeadCanary =
    0x46564E585243554Cull;
inline constexpr std::uint64_t PrivateGeometryCullingTailCanary =
    0x43554C4C45524F4Bull;
inline constexpr std::uint64_t PrivateGeometryBindingTailCanary =
    0x564945574641494Cull;
inline constexpr std::uint32_t PrivateGeometryOwnedVtableInstalledTag =
    0x5654424Cu;

enum class GeometryCollectorPhase : std::uint8_t
{
    Inactive,
    Collecting,
    Sealed,
    Failed,
};

enum class GeometryCollectorFailure : std::uint8_t
{
    None,
    NullGeometry,
    CapacityExceeded,
    OwnerMismatch,
    GenerationMismatch,
    ThreadMismatch,
    InvalidTransition,
    StateCorrupt,
    BindingCorrupt,
};

enum class GeometryAppendResult : std::uint8_t
{
    Inserted,
    NullInvalidated,
    OverflowInvalidated,
    OwnerMismatchInvalidated,
    GenerationMismatchInvalidated,
    ThreadMismatchInvalidated,
    NotCollectingInvalidated,
    StateCorruptInvalidated,
    PassAlreadyFailed,
};

enum class GeometrySealResult : std::uint8_t
{
    Sealed,
    OwnerMismatchInvalidated,
    GenerationMismatchInvalidated,
    ThreadMismatchInvalidated,
    NotCollectingInvalidated,
    StateCorruptInvalidated,
    PassAlreadyFailed,
};

enum class GeometryVtableInstallResult : std::uint8_t
{
    Installed,
    BindingCorrupt,
    BindingBusy,
    AlreadyInstalled,
    NullSource,
    WrongEntryCount,
    InvalidVtableAddress,
    InvalidCallback,
    CullerSourceMismatch,
    NullSourceEntry,
    StockAppendTargetMismatch,
};

struct PrivateGeometrySealedView
{
    const RetailGeometryPointer32* geometryPointers = nullptr;
    std::uint32_t itemCount = 0u;
    std::uint64_t generation = 0u;
};

constexpr bool privateGeometryDigestEqual(
    const fnvxr::engine::Sha256Digest& left,
    const fnvxr::engine::Sha256Digest& right) noexcept
{
    if (!left.valid || !right.valid)
        return false;
    for (std::size_t index = 0u; index < left.bytes.size(); ++index)
    {
        if (left.bytes[index] != right.bytes[index])
            return false;
    }
    return true;
}

// Exact static metadata only. Runtime authorization additionally requires the
// loaded OnVisible thunk, every inherited cloned-vtable slot, and the full
// engine ABI gate to be verified synchronously in the target process. None of
// those activation authorities exist in this header.
struct PrivateGeometryCollectorX86Abi
{
    static constexpr std::uintptr_t ownerVtableAddress =
        fnvxr::engine::abi::BSCullingProcessVtableAddress;
    static constexpr std::size_t vtableSlotByteOffset = 0x4Cu;
    static constexpr std::uintptr_t preferredTargetAddress = 0x00C4F1D0u;
    static constexpr std::size_t appendByteCount = 150u;
    static constexpr fnvxr::engine::Sha256Digest appendSha256 =
        fnvxr::engine::sha256FromHex(
            "7E198CD639B8CA514E2427DD3524300DF7D9CAF2D5181B915D917210FACFC9BE");

    static constexpr std::uintptr_t onVisibleThunkAddress = 0x00A7FD90u;
    static constexpr std::size_t onVisibleThunkByteCount = 17u;
    static constexpr fnvxr::engine::Sha256Digest onVisibleThunkSha256 =
        fnvxr::engine::sha256FromHex(
            "C3A270DCBE479A05ED865C7AEB395E19CBADCEEB575486F12A5D6F94B7F8452C");

    static constexpr std::size_t ownerVtableByteCount = 0x50u;
    static constexpr std::size_t ownerVtableEntryCount =
        ownerVtableByteCount / sizeof(RetailGeometryPointer32);
    static constexpr std::size_t appendVtableEntryIndex =
        vtableSlotByteOffset / sizeof(RetailGeometryPointer32);
    static constexpr fnvxr::engine::Sha256Digest ownerVtableSha256 =
        fnvxr::engine::sha256FromHex(
            "7F1CEFA617A2A3A0F07D944C0233436C9FEED2A2B3FA5AF379730D079D3EF838");

    static constexpr fnvxr::engine::abi::RetailX86CallingConvention callingConvention =
        fnvxr::engine::abi::RetailX86CallingConvention::Thiscall;
    static constexpr std::uint8_t stackArgumentCount = 1u;
    static constexpr std::uint8_t calleePopBytes = 4u;
    static constexpr std::size_t geometryPointerBytes =
        sizeof(RetailGeometryPointer32);
    static constexpr bool callbackNoexcept = true;
    static constexpr bool callbackUsesHeap = false;

#if defined(_MSC_VER) && defined(_M_IX86)
    static constexpr bool actualCallbackCompiled = true;
#else
    static constexpr bool actualCallbackCompiled = false;
#endif

    // The revalidator now hashes the live OnVisible thunk and the complete
    // 0x50-byte source vtable, while this binding copies all 20 entries and
    // replaces only Append. Runtime activation remains a separate capability.
    static constexpr bool onVisibleRuntimeVerificationImplemented = true;
    static constexpr bool fullClonedVtableProven = true;
    static constexpr bool runtimeActivationAuthorized = false;
    static constexpr bool activatesHook = false;
    static constexpr bool clonesVtable = true;
    static constexpr bool callsEngine = false;
    static constexpr bool writesProcessMemory = false;
};

constexpr bool privateGeometryCollectorStaticAbiMatchesRetailInventory() noexcept
{
    bool appendMatches = false;
    bool onVisibleMatches = false;
    for (const fnvxr::engine::abi::RetailFunctionAbiDescriptor& function
         : fnvxr::engine::abi::RetailFunctionAbiInventory)
    {
        if (function.preferredAddress
                == PrivateGeometryCollectorX86Abi::preferredTargetAddress
            && function.byteCount == PrivateGeometryCollectorX86Abi::appendByteCount
            && privateGeometryDigestEqual(
                function.sha256,
                PrivateGeometryCollectorX86Abi::appendSha256)
            && function.callingConvention
                == PrivateGeometryCollectorX86Abi::callingConvention
            && function.stackArgumentCount
                == PrivateGeometryCollectorX86Abi::stackArgumentCount
            && function.calleePopBytes
                == PrivateGeometryCollectorX86Abi::calleePopBytes
            && function.exactInstructionBoundary
            && function.stackContractProven
            && function.argumentSemanticsProven)
        {
            appendMatches = true;
        }

        if (function.preferredAddress
                == PrivateGeometryCollectorX86Abi::onVisibleThunkAddress
            && function.byteCount
                == PrivateGeometryCollectorX86Abi::onVisibleThunkByteCount
            && privateGeometryDigestEqual(
                function.sha256,
                PrivateGeometryCollectorX86Abi::onVisibleThunkSha256)
            && function.callingConvention
                == PrivateGeometryCollectorX86Abi::callingConvention
            && function.stackArgumentCount
                == PrivateGeometryCollectorX86Abi::stackArgumentCount
            && function.calleePopBytes
                == PrivateGeometryCollectorX86Abi::calleePopBytes
            && function.exactInstructionBoundary
            && function.stackContractProven
            && function.argumentSemanticsProven)
        {
            onVisibleMatches = true;
        }
    }

    bool slotMatches = false;
    for (const fnvxr::engine::abi::RetailVtableSlotDescriptor& slot
         : fnvxr::engine::abi::RetailVtableSlots)
    {
        if (slot.vtableAddress
                == PrivateGeometryCollectorX86Abi::ownerVtableAddress
            && slot.slotByteOffset
                == PrivateGeometryCollectorX86Abi::vtableSlotByteOffset
            && slot.preferredTargetAddress
                == PrivateGeometryCollectorX86Abi::preferredTargetAddress)
        {
            slotMatches = true;
            break;
        }
    }

    bool blockMatches = false;
    for (const fnvxr::engine::abi::RetailVtableBlockDescriptor& block
         : fnvxr::engine::abi::RetailVtableBlocks)
    {
        if (block.preferredAddress
                == PrivateGeometryCollectorX86Abi::ownerVtableAddress
            && block.byteCount
                == PrivateGeometryCollectorX86Abi::ownerVtableByteCount
            && privateGeometryDigestEqual(
                block.sha256,
                PrivateGeometryCollectorX86Abi::ownerVtableSha256))
        {
            blockMatches = true;
            break;
        }
    }
    return appendMatches && onVisibleMatches && slotMatches && blockMatches;
}

constexpr bool privateGeometryCollectorAppendProductionEvidencePresent() noexcept
{
    for (const fnvxr::engine::abi::RetailFunctionAbiDescriptor& function
         : fnvxr::engine::abi::RetailFunctionAbiInventory)
    {
        if (function.preferredAddress
            == PrivateGeometryCollectorX86Abi::preferredTargetAddress)
        {
            return fnvxr::engine::abi::productionProven(function);
        }
    }
    return false;
}

constexpr bool privateGeometryCollectorOnVisibleProductionEvidencePresent() noexcept
{
    for (const fnvxr::engine::abi::RetailFunctionAbiDescriptor& function
         : fnvxr::engine::abi::RetailFunctionAbiInventory)
    {
        if (function.preferredAddress
            == PrivateGeometryCollectorX86Abi::onVisibleThunkAddress)
        {
            return fnvxr::engine::abi::productionProven(function);
        }
    }
    return false;
}

constexpr bool privateGeometryCollectorVtableBlockProductionEvidencePresent() noexcept
{
    for (const fnvxr::engine::abi::RetailVtableBlockDescriptor& block
         : fnvxr::engine::abi::RetailVtableBlocks)
    {
        if (block.preferredAddress
            == PrivateGeometryCollectorX86Abi::ownerVtableAddress)
        {
            return fnvxr::engine::abi::productionProven(block);
        }
    }
    return false;
}

constexpr bool privateGeometryCollectorProductionReady() noexcept
{
    return privateGeometryCollectorStaticAbiMatchesRetailInventory()
        && privateGeometryCollectorAppendProductionEvidencePresent()
        && privateGeometryCollectorOnVisibleProductionEvidencePresent()
        && privateGeometryCollectorVtableBlockProductionEvidencePresent()
        && PrivateGeometryCollectorX86Abi::actualCallbackCompiled
        && PrivateGeometryCollectorX86Abi::onVisibleRuntimeVerificationImplemented
        && PrivateGeometryCollectorX86Abi::fullClonedVtableProven
        && PrivateGeometryCollectorX86Abi::runtimeActivationAuthorized;
}

static_assert(sizeof(RetailGeometryPointer32) == 4u);
static_assert(PrivateGeometryCollectorX86Abi::ownerVtableEntryCount == 20u);
static_assert(PrivateGeometryCollectorX86Abi::appendVtableEntryIndex == 19u);
static_assert(PrivateGeometryCollectorX86Abi::appendSha256.valid);
static_assert(PrivateGeometryCollectorX86Abi::onVisibleThunkSha256.valid);
static_assert(PrivateGeometryCollectorX86Abi::ownerVtableSha256.valid);
static_assert(privateGeometryCollectorStaticAbiMatchesRetailInventory());
static_assert(privateGeometryCollectorOnVisibleProductionEvidencePresent());
static_assert(privateGeometryCollectorVtableBlockProductionEvidencePresent());
static_assert(!privateGeometryCollectorProductionReady());

inline PrivateGeometryThreadToken currentPrivateGeometryThreadToken() noexcept
{
    static thread_local const std::uint8_t threadMarker = 0u;
    return static_cast<PrivateGeometryThreadToken>(
        reinterpret_cast<std::uintptr_t>(&threadMarker));
}

template <std::size_t Capacity>
struct alignas(8) PrivateGeometryCollectorStorageLayout
{
    std::uint64_t headCanary = PrivateGeometryStateHeadCanary;
    std::uint64_t generation = 0u;
    PrivateGeometryThreadToken expectedThreadToken = 0u;
    RetailGeometryPointer32 expectedOwner = 0u;
    std::uint32_t itemCount = 0u;
    std::array<RetailGeometryPointer32, Capacity> items {};
    GeometryCollectorPhase phase = GeometryCollectorPhase::Inactive;
    GeometryCollectorFailure failure = GeometryCollectorFailure::None;
    std::uint16_t reserved = 0u;
    std::uint64_t tailCanary = PrivateGeometryStateTailCanary;
};

template <std::size_t Capacity>
class PrivateGeometryCollectorBinding;

template <std::size_t Capacity>
class PrivateGeometryCollectorState
{
    static_assert(Capacity > 0u, "a private geometry collector needs positive capacity");
    static_assert(
        Capacity <= (std::numeric_limits<std::uint32_t>::max)(),
        "collector count must remain representable in retail x86 state");

    using Storage = PrivateGeometryCollectorStorageLayout<Capacity>;

public:
    static constexpr std::size_t HeadCanaryByteOffset =
        offsetof(Storage, headCanary);
    static constexpr std::size_t ItemCountByteOffset =
        offsetof(Storage, itemCount);
    static constexpr std::size_t ItemsByteOffset = offsetof(Storage, items);
    static constexpr std::size_t TailCanaryByteOffset =
        offsetof(Storage, tailCanary);

    static constexpr std::size_t capacity() noexcept
    {
        return Capacity;
    }

    GeometryCollectorPhase phase() const noexcept
    {
        return storageIntegrityValid() ? mStorage.phase : GeometryCollectorPhase::Failed;
    }

    GeometryCollectorFailure failure() const noexcept
    {
        return storageIntegrityValid()
            ? mStorage.failure
            : GeometryCollectorFailure::StateCorrupt;
    }

    std::uint32_t collectedCountForAudit() const noexcept
    {
        return mStorage.itemCount;
    }

    std::uint64_t generationForAudit() const noexcept
    {
        return mStorage.generation;
    }

    bool begin(
        std::uint64_t generation,
        RetailGeometryPointer32 expectedOwner,
        PrivateGeometryThreadToken expectedThreadToken) noexcept
    {
        if (!storageIntegrityValid())
        {
            markStateCorrupt();
            return false;
        }
        if (mStorage.phase != GeometryCollectorPhase::Inactive)
        {
            invalidate(GeometryCollectorFailure::InvalidTransition);
            return false;
        }
        if (generation == 0u || expectedOwner == 0u || expectedThreadToken == 0u)
        {
            invalidate(GeometryCollectorFailure::InvalidTransition);
            return false;
        }

        mStorage.generation = generation;
        mStorage.expectedOwner = expectedOwner;
        mStorage.expectedThreadToken = expectedThreadToken;
        mStorage.itemCount = 0u;
        mStorage.failure = GeometryCollectorFailure::None;
        mStorage.phase = GeometryCollectorPhase::Collecting;
        return true;
    }

    GeometryAppendResult append(
        RetailGeometryPointer32 owner,
        std::uint64_t generation,
        PrivateGeometryThreadToken threadToken,
        RetailGeometryPointer32 geometry) noexcept
    {
        if (!storageIntegrityValid())
        {
            markStateCorrupt();
            return GeometryAppendResult::StateCorruptInvalidated;
        }
        if (mStorage.phase == GeometryCollectorPhase::Failed)
            return GeometryAppendResult::PassAlreadyFailed;
        if (mStorage.phase != GeometryCollectorPhase::Collecting)
        {
            invalidate(GeometryCollectorFailure::InvalidTransition);
            return GeometryAppendResult::NotCollectingInvalidated;
        }
        if (owner != mStorage.expectedOwner)
        {
            invalidate(GeometryCollectorFailure::OwnerMismatch);
            return GeometryAppendResult::OwnerMismatchInvalidated;
        }
        if (generation != mStorage.generation)
        {
            invalidate(GeometryCollectorFailure::GenerationMismatch);
            return GeometryAppendResult::GenerationMismatchInvalidated;
        }
        if (threadToken != mStorage.expectedThreadToken)
        {
            invalidate(GeometryCollectorFailure::ThreadMismatch);
            return GeometryAppendResult::ThreadMismatchInvalidated;
        }
        if (geometry == 0u)
        {
            invalidate(GeometryCollectorFailure::NullGeometry);
            return GeometryAppendResult::NullInvalidated;
        }
        if (mStorage.itemCount >= Capacity)
        {
            invalidate(GeometryCollectorFailure::CapacityExceeded);
            return GeometryAppendResult::OverflowInvalidated;
        }

        mStorage.items[mStorage.itemCount] = geometry;
        ++mStorage.itemCount;
        return GeometryAppendResult::Inserted;
    }

    GeometrySealResult seal(
        RetailGeometryPointer32 owner,
        std::uint64_t generation,
        PrivateGeometryThreadToken threadToken) noexcept
    {
        if (!storageIntegrityValid())
        {
            markStateCorrupt();
            return GeometrySealResult::StateCorruptInvalidated;
        }
        if (mStorage.phase == GeometryCollectorPhase::Failed)
            return GeometrySealResult::PassAlreadyFailed;
        if (mStorage.phase != GeometryCollectorPhase::Collecting)
        {
            invalidate(GeometryCollectorFailure::InvalidTransition);
            return GeometrySealResult::NotCollectingInvalidated;
        }
        if (owner != mStorage.expectedOwner)
        {
            invalidate(GeometryCollectorFailure::OwnerMismatch);
            return GeometrySealResult::OwnerMismatchInvalidated;
        }
        if (generation != mStorage.generation)
        {
            invalidate(GeometryCollectorFailure::GenerationMismatch);
            return GeometrySealResult::GenerationMismatchInvalidated;
        }
        if (threadToken != mStorage.expectedThreadToken)
        {
            invalidate(GeometryCollectorFailure::ThreadMismatch);
            return GeometrySealResult::ThreadMismatchInvalidated;
        }

        mStorage.phase = GeometryCollectorPhase::Sealed;
        return GeometrySealResult::Sealed;
    }

    bool tryGetSealedView(PrivateGeometrySealedView& view) noexcept
    {
        view = {};
        if (!storageIntegrityValid())
        {
            markStateCorrupt();
            return false;
        }
        if (mStorage.phase != GeometryCollectorPhase::Sealed
            || mStorage.failure != GeometryCollectorFailure::None)
        {
            return false;
        }

        view.geometryPointers = mStorage.items.data();
        view.itemCount = mStorage.itemCount;
        view.generation = mStorage.generation;
        return true;
    }

    bool reset() noexcept
    {
        if (!storageIntegrityValid())
        {
            markStateCorrupt();
            return false;
        }

        for (RetailGeometryPointer32& item : mStorage.items)
            item = 0u;
        mStorage.generation = 0u;
        mStorage.expectedThreadToken = 0u;
        mStorage.expectedOwner = 0u;
        mStorage.itemCount = 0u;
        mStorage.phase = GeometryCollectorPhase::Inactive;
        mStorage.failure = GeometryCollectorFailure::None;
        mStorage.reserved = 0u;
        return true;
    }

private:
    friend class PrivateGeometryCollectorBinding<Capacity>;

    static constexpr bool knownPhase(GeometryCollectorPhase value) noexcept
    {
        return value == GeometryCollectorPhase::Inactive
            || value == GeometryCollectorPhase::Collecting
            || value == GeometryCollectorPhase::Sealed
            || value == GeometryCollectorPhase::Failed;
    }

    static constexpr bool knownFailure(GeometryCollectorFailure value) noexcept
    {
        return value == GeometryCollectorFailure::None
            || value == GeometryCollectorFailure::NullGeometry
            || value == GeometryCollectorFailure::CapacityExceeded
            || value == GeometryCollectorFailure::OwnerMismatch
            || value == GeometryCollectorFailure::GenerationMismatch
            || value == GeometryCollectorFailure::ThreadMismatch
            || value == GeometryCollectorFailure::InvalidTransition
            || value == GeometryCollectorFailure::StateCorrupt
            || value == GeometryCollectorFailure::BindingCorrupt;
    }

    bool storageIntegrityValid() const noexcept
    {
        if (mStorage.headCanary != PrivateGeometryStateHeadCanary
            || mStorage.tailCanary != PrivateGeometryStateTailCanary
            || mStorage.itemCount > Capacity || mStorage.reserved != 0u
            || !knownPhase(mStorage.phase) || !knownFailure(mStorage.failure))
        {
            return false;
        }

        if (mStorage.phase == GeometryCollectorPhase::Inactive)
        {
            return mStorage.failure == GeometryCollectorFailure::None
                && mStorage.generation == 0u && mStorage.expectedThreadToken == 0u
                && mStorage.expectedOwner == 0u && mStorage.itemCount == 0u;
        }
        if (mStorage.phase == GeometryCollectorPhase::Collecting
            || mStorage.phase == GeometryCollectorPhase::Sealed)
        {
            return mStorage.failure == GeometryCollectorFailure::None
                && mStorage.generation != 0u && mStorage.expectedThreadToken != 0u
                && mStorage.expectedOwner != 0u;
        }
        return mStorage.phase == GeometryCollectorPhase::Failed
            && mStorage.failure != GeometryCollectorFailure::None;
    }

    void invalidate(GeometryCollectorFailure failure) noexcept
    {
        mStorage.failure = failure;
        mStorage.phase = GeometryCollectorPhase::Failed;
    }

    void markStateCorrupt() noexcept
    {
        mStorage.failure = GeometryCollectorFailure::StateCorrupt;
        mStorage.phase = GeometryCollectorPhase::Failed;
    }

    void invalidateFromBinding() noexcept
    {
        if (storageIntegrityValid())
            invalidate(GeometryCollectorFailure::BindingCorrupt);
        else
            markStateCorrupt();
    }

    Storage mStorage {};
};

template <std::size_t Capacity>
struct alignas(8) PrivateGeometryCollectorBindingLayout
{
    std::uint64_t headCanary = PrivateGeometryBindingHeadCanary;
    fnvxr::engine::abi::RetailBSCullingProcessLayout cullingProcess {};
    std::uint64_t cullingTailCanary = PrivateGeometryCullingTailCanary;
    std::array<
        RetailGeometryPointer32,
        PrivateGeometryCollectorX86Abi::ownerVtableEntryCount>
        ownedVtableClone {};
    std::array<
        RetailGeometryPointer32,
        PrivateGeometryCollectorX86Abi::ownerVtableEntryCount>
        sourceVtableSnapshot {};
    RetailGeometryPointer32 sourceVtableAddress = 0u;
    RetailGeometryPointer32 ownedVtableAddress = 0u;
    RetailGeometryPointer32 callbackAddress = 0u;
    std::uint32_t ownedVtableInstalledTag = 0u;
    std::uint64_t dispatchGeneration = 0u;
    PrivateGeometryCollectorState<Capacity> collector {};
    std::uint64_t tailCanary = PrivateGeometryBindingTailCanary;
};

template <std::size_t Capacity>
class PrivateGeometryCollectorBinding
{
    using Storage = PrivateGeometryCollectorBindingLayout<Capacity>;

public:
    static constexpr std::size_t OwnedVtableEntryCount =
        PrivateGeometryCollectorX86Abi::ownerVtableEntryCount;
    static constexpr std::size_t AppendVtableEntryIndex =
        PrivateGeometryCollectorX86Abi::appendVtableEntryIndex;
    static constexpr std::size_t HeadCanaryByteOffset =
        offsetof(Storage, headCanary);
    static constexpr std::size_t CullingProcessByteOffset =
        offsetof(Storage, cullingProcess);
    static constexpr std::size_t CullingProcessByteCount =
        sizeof(fnvxr::engine::abi::RetailBSCullingProcessLayout);
    static constexpr std::size_t CullingTailCanaryByteOffset =
        offsetof(Storage, cullingTailCanary);
    static constexpr std::size_t OwnedVtableCloneByteOffset =
        offsetof(Storage, ownedVtableClone);
    static constexpr std::size_t OwnedVtableCloneByteCount =
        sizeof(Storage::ownedVtableClone);
    static constexpr std::size_t SourceVtableSnapshotByteOffset =
        offsetof(Storage, sourceVtableSnapshot);
    static constexpr std::size_t DispatchGenerationByteOffset =
        offsetof(Storage, dispatchGeneration);
    static constexpr std::size_t CollectorByteOffset =
        offsetof(Storage, collector);
    static constexpr std::size_t TailCanaryByteOffset =
        offsetof(Storage, tailCanary);

    fnvxr::engine::abi::RetailBSCullingProcessLayout* cullingProcess() noexcept
    {
        return &mStorage.cullingProcess;
    }

    const fnvxr::engine::abi::RetailBSCullingProcessLayout* cullingProcess() const noexcept
    {
        return &mStorage.cullingProcess;
    }

    GeometryVtableInstallResult installOwnedVtableClone(
        const RetailGeometryPointer32* verifiedSourceVtable,
        std::size_t sourceEntryCount,
        RetailGeometryPointer32 verifiedSourceVtableAddress,
        RetailGeometryPointer32 ownedBindingAddress,
        RetailGeometryPointer32 replacementCallbackAddress) noexcept
    {
        if (!bindingIntegrityValid())
        {
            mStorage.collector.invalidateFromBinding();
            return GeometryVtableInstallResult::BindingCorrupt;
        }
        if (mStorage.ownedVtableInstalledTag
            == PrivateGeometryOwnedVtableInstalledTag)
        {
            return GeometryVtableInstallResult::AlreadyInstalled;
        }
        if (mStorage.collector.phase() != GeometryCollectorPhase::Inactive
            || mStorage.collector.failure() != GeometryCollectorFailure::None
            || mStorage.dispatchGeneration != 0u)
        {
            return GeometryVtableInstallResult::BindingBusy;
        }
        if (!verifiedSourceVtable)
            return GeometryVtableInstallResult::NullSource;
        if (sourceEntryCount != OwnedVtableEntryCount)
            return GeometryVtableInstallResult::WrongEntryCount;
        const std::uint64_t ownedCloneAddressWide =
            static_cast<std::uint64_t>(ownedBindingAddress)
            + OwnedVtableCloneByteOffset;
        if (verifiedSourceVtableAddress == 0u
            || ownedBindingAddress == 0u
            || ownedBindingAddress % alignof(Storage) != 0u
            || ownedCloneAddressWide
                > (std::numeric_limits<RetailGeometryPointer32>::max)())
        {
            return GeometryVtableInstallResult::InvalidVtableAddress;
        }
        const RetailGeometryPointer32 ownedCloneVtableAddress =
            static_cast<RetailGeometryPointer32>(ownedCloneAddressWide);
        if (verifiedSourceVtableAddress == ownedCloneVtableAddress)
            return GeometryVtableInstallResult::InvalidVtableAddress;
        if (replacementCallbackAddress == 0u
            || replacementCallbackAddress
                == PrivateGeometryCollectorX86Abi::preferredTargetAddress)
        {
            return GeometryVtableInstallResult::InvalidCallback;
        }
        if (mStorage.cullingProcess.base.vtable
            != verifiedSourceVtableAddress)
        {
            return GeometryVtableInstallResult::CullerSourceMismatch;
        }
        for (std::size_t index = 0u; index < OwnedVtableEntryCount; ++index)
        {
            if (verifiedSourceVtable[index] == 0u)
                return GeometryVtableInstallResult::NullSourceEntry;
        }
        if (verifiedSourceVtable[AppendVtableEntryIndex]
            != PrivateGeometryCollectorX86Abi::preferredTargetAddress)
        {
            return GeometryVtableInstallResult::StockAppendTargetMismatch;
        }

        for (std::size_t index = 0u; index < OwnedVtableEntryCount; ++index)
        {
            mStorage.sourceVtableSnapshot[index] = verifiedSourceVtable[index];
            mStorage.ownedVtableClone[index] = verifiedSourceVtable[index];
        }
        mStorage.ownedVtableClone[AppendVtableEntryIndex] =
            replacementCallbackAddress;
        mStorage.sourceVtableAddress = verifiedSourceVtableAddress;
        mStorage.ownedVtableAddress = ownedCloneVtableAddress;
        mStorage.callbackAddress = replacementCallbackAddress;
        mStorage.cullingProcess.base.vtable = ownedCloneVtableAddress;
        mStorage.ownedVtableInstalledTag =
            PrivateGeometryOwnedVtableInstalledTag;
        return GeometryVtableInstallResult::Installed;
    }

    bool ownedVtableCloneInstalled() const noexcept
    {
        return bindingIntegrityValid()
            && mStorage.ownedVtableInstalledTag
                == PrivateGeometryOwnedVtableInstalledTag;
    }

    bool ownedVtableIntegrityValid() const noexcept
    {
        return bindingIntegrityValid();
    }

    const std::array<RetailGeometryPointer32, OwnedVtableEntryCount>&
    ownedVtableCloneForAudit() const noexcept
    {
        return mStorage.ownedVtableClone;
    }

    const std::array<RetailGeometryPointer32, OwnedVtableEntryCount>&
    sourceVtableSnapshotForAudit() const noexcept
    {
        return mStorage.sourceVtableSnapshot;
    }

    GeometryCollectorFailure failure() const noexcept
    {
        return bindingIntegrityValid()
            ? mStorage.collector.failure()
            : GeometryCollectorFailure::BindingCorrupt;
    }

    GeometryCollectorPhase phase() const noexcept
    {
        return bindingIntegrityValid()
            ? mStorage.collector.phase()
            : GeometryCollectorPhase::Failed;
    }

    bool tryGetSealedView(PrivateGeometrySealedView& view) noexcept
    {
        if (!bindingIntegrityValid())
        {
            view = {};
            mStorage.collector.invalidateFromBinding();
            return false;
        }
        return mStorage.collector.tryGetSealedView(view);
    }

    bool reset() noexcept
    {
        if (!bindingIntegrityValid())
        {
            mStorage.collector.invalidateFromBinding();
            return false;
        }
        if (!mStorage.collector.reset())
            return false;
        if (mStorage.ownedVtableInstalledTag
            == PrivateGeometryOwnedVtableInstalledTag)
        {
            mStorage.cullingProcess.base.vtable =
                mStorage.sourceVtableAddress;
        }
        for (RetailGeometryPointer32& entry : mStorage.ownedVtableClone)
            entry = 0u;
        for (RetailGeometryPointer32& entry : mStorage.sourceVtableSnapshot)
            entry = 0u;
        mStorage.sourceVtableAddress = 0u;
        mStorage.ownedVtableAddress = 0u;
        mStorage.callbackAddress = 0u;
        mStorage.ownedVtableInstalledTag = 0u;
        mStorage.dispatchGeneration = 0u;
        return true;
    }

    // End one visibility pass without surrendering the owned dispatch table.
    // Full reset() is teardown: it restores the stock vtable and clears the
    // clone. A persistent renderer must keep the clone installed across frames
    // while discarding only the sealed geometry and generation.
    bool resetCollectedGeometryPreservingOwnedVtable() noexcept
    {
        if (!bindingIntegrityValid()
            || mStorage.ownedVtableInstalledTag
                != PrivateGeometryOwnedVtableInstalledTag)
        {
            mStorage.collector.invalidateFromBinding();
            return false;
        }
        if (!mStorage.collector.reset())
            return false;
        mStorage.dispatchGeneration = 0u;
        return bindingIntegrityValid();
    }

#if defined(_MSC_VER) && defined(_M_IX86)
    bool beginCollection(std::uint64_t generation) noexcept
    {
        if (!bindingIntegrityValid())
        {
            mStorage.collector.invalidateFromBinding();
            return false;
        }
        mStorage.dispatchGeneration = generation;
        return mStorage.collector.begin(
            generation,
            cullingProcessToken(),
            currentPrivateGeometryThreadToken());
    }

    GeometrySealResult sealCollection() noexcept
    {
        if (!bindingIntegrityValid())
        {
            mStorage.collector.invalidateFromBinding();
            return GeometrySealResult::StateCorruptInvalidated;
        }
        return mStorage.collector.seal(
            cullingProcessToken(),
            mStorage.dispatchGeneration,
            currentPrivateGeometryThreadToken());
    }

    void appendFromVslot(
        fnvxr::engine::abi::RetailBSCullingProcessLayout* callbackOwner,
        void* geometry) noexcept
    {
        if (!bindingIntegrityValid())
        {
            mStorage.collector.invalidateFromBinding();
            return;
        }

        const auto ownerToken = static_cast<RetailGeometryPointer32>(
            reinterpret_cast<std::uintptr_t>(callbackOwner));
        const auto geometryToken = static_cast<RetailGeometryPointer32>(
            reinterpret_cast<std::uintptr_t>(geometry));
        (void)mStorage.collector.append(
            ownerToken,
            mStorage.dispatchGeneration,
            currentPrivateGeometryThreadToken(),
            geometryToken);
    }
#endif

private:
    bool ownedVtableStateValid() const noexcept
    {
        if (mStorage.ownedVtableInstalledTag == 0u)
        {
            if (mStorage.sourceVtableAddress != 0u
                || mStorage.ownedVtableAddress != 0u
                || mStorage.callbackAddress != 0u)
            {
                return false;
            }
            for (std::size_t index = 0u; index < OwnedVtableEntryCount; ++index)
            {
                if (mStorage.ownedVtableClone[index] != 0u
                    || mStorage.sourceVtableSnapshot[index] != 0u)
                {
                    return false;
                }
            }
            return true;
        }
        if (mStorage.ownedVtableInstalledTag
            != PrivateGeometryOwnedVtableInstalledTag
            || mStorage.sourceVtableAddress == 0u
            || mStorage.ownedVtableAddress == 0u
            || mStorage.sourceVtableAddress == mStorage.ownedVtableAddress
            || mStorage.callbackAddress == 0u
            || mStorage.callbackAddress
                == PrivateGeometryCollectorX86Abi::preferredTargetAddress
            || mStorage.cullingProcess.base.vtable
                != mStorage.ownedVtableAddress
            || mStorage.sourceVtableSnapshot[AppendVtableEntryIndex]
                != PrivateGeometryCollectorX86Abi::preferredTargetAddress)
        {
            return false;
        }

        for (std::size_t index = 0u; index < OwnedVtableEntryCount; ++index)
        {
            if (mStorage.sourceVtableSnapshot[index] == 0u)
                return false;
            const RetailGeometryPointer32 expected =
                index == AppendVtableEntryIndex
                ? mStorage.callbackAddress
                : mStorage.sourceVtableSnapshot[index];
            if (mStorage.ownedVtableClone[index] != expected)
                return false;
        }
        return true;
    }

    bool bindingIntegrityValid() const noexcept
    {
        return mStorage.headCanary == PrivateGeometryBindingHeadCanary
            && mStorage.cullingTailCanary == PrivateGeometryCullingTailCanary
            && mStorage.tailCanary == PrivateGeometryBindingTailCanary
            && ownedVtableStateValid();
    }

#if defined(_MSC_VER) && defined(_M_IX86)
    RetailGeometryPointer32 cullingProcessToken() const noexcept
    {
        return static_cast<RetailGeometryPointer32>(
            reinterpret_cast<std::uintptr_t>(&mStorage.cullingProcess));
    }
#endif

    Storage mStorage {};
};

#if defined(_MSC_VER) && defined(_M_IX86)
template <std::size_t Capacity>
using PrivateGeometryCollectorVslotCallbackFunction = void(__fastcall*)(
    fnvxr::engine::abi::RetailBSCullingProcessLayout*,
    void*,
    void*) noexcept;

template <std::size_t Capacity>
void __fastcall privateGeometryCollectorVslotCallback(
    fnvxr::engine::abi::RetailBSCullingProcessLayout* cullingProcess,
    void* ignoredEdx,
    void* geometry) noexcept
{
    (void)ignoredEdx;
    if (!cullingProcess)
        return;

    auto* cullingBytes = reinterpret_cast<unsigned char*>(cullingProcess);
    auto* binding = reinterpret_cast<PrivateGeometryCollectorBinding<Capacity>*>(
        cullingBytes
        - PrivateGeometryCollectorBinding<Capacity>::CullingProcessByteOffset);
    binding->appendFromVslot(cullingProcess, geometry);
}
#endif

static_assert(std::is_standard_layout_v<PrivateGeometrySealedView>);
static_assert(std::is_trivially_copyable_v<PrivateGeometrySealedView>);
}
