#include "fnvxr_private_geometry_collector.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <type_traits>

namespace
{
std::size_t gAllocations = 0u;

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

template <class Object>
void xorObjectByte(Object& object, std::size_t byteOffset)
{
    auto* bytes = reinterpret_cast<unsigned char*>(&object);
    bytes[byteOffset] ^= 0x5Au;
}

template <class Object>
void overwriteObjectUint32(
    Object& object,
    std::size_t byteOffset,
    std::uint32_t value)
{
    std::memcpy(
        reinterpret_cast<unsigned char*>(&object) + byteOffset,
        &value,
        sizeof(value));
}
}

void* operator new(std::size_t byteCount)
{
    ++gAllocations;
    if (void* allocation = std::malloc(byteCount == 0u ? 1u : byteCount))
        return allocation;
    throw std::bad_alloc {};
}

void* operator new[](std::size_t byteCount)
{
    return ::operator new(byteCount);
}

void operator delete(void* allocation) noexcept
{
    std::free(allocation);
}

void operator delete[](void* allocation) noexcept
{
    std::free(allocation);
}

void operator delete(void* allocation, std::size_t) noexcept
{
    std::free(allocation);
}

void operator delete[](void* allocation, std::size_t) noexcept
{
    std::free(allocation);
}

int main()
{
    using namespace fnvxr::engine::geometry;

    using State = PrivateGeometryCollectorState<4u>;
    using Binding = PrivateGeometryCollectorBinding<4u>;

    static_assert(std::is_standard_layout_v<State>);
    static_assert(std::is_trivially_copyable_v<State>);
    static_assert(std::is_standard_layout_v<Binding>);
    static_assert(std::is_trivially_copyable_v<Binding>);
    static_assert(sizeof(State) == sizeof(PrivateGeometryCollectorStorageLayout<4u>));
    static_assert(sizeof(Binding) == sizeof(PrivateGeometryCollectorBindingLayout<4u>));
    static_assert(State::HeadCanaryByteOffset == 0u);
    static_assert(State::ItemCountByteOffset == 28u);
    static_assert(Binding::CullingProcessByteOffset == 8u);
    static_assert(Binding::CullingProcessByteCount == 0xC8u);
    static_assert(Binding::CullingTailCanaryByteOffset == 0xD0u);
    static_assert(Binding::OwnedVtableCloneByteCount == 0x50u);
    static_assert(Binding::OwnedVtableEntryCount == 20u);
    static_assert(Binding::AppendVtableEntryIndex == 19u);

    require(
        PrivateGeometryCollectorX86Abi::ownerVtableAddress
                == fnvxr::engine::abi::BSCullingProcessVtableAddress
            && PrivateGeometryCollectorX86Abi::vtableSlotByteOffset == 0x4Cu
            && PrivateGeometryCollectorX86Abi::preferredTargetAddress == 0x00C4F1D0u
            && PrivateGeometryCollectorX86Abi::appendByteCount == 150u
            && PrivateGeometryCollectorX86Abi::callingConvention
                == fnvxr::engine::abi::RetailX86CallingConvention::Thiscall
            && PrivateGeometryCollectorX86Abi::stackArgumentCount == 1u
            && PrivateGeometryCollectorX86Abi::calleePopBytes == 4u
            && PrivateGeometryCollectorX86Abi::geometryPointerBytes == 4u,
        "append callback metadata must match the complete retail +0x4C function");
    require(
        PrivateGeometryCollectorX86Abi::onVisibleThunkAddress == 0x00A7FD90u
            && PrivateGeometryCollectorX86Abi::onVisibleThunkByteCount == 17u
            && privateGeometryDigestEqual(
                PrivateGeometryCollectorX86Abi::onVisibleThunkSha256,
                fnvxr::engine::sha256FromHex(
                    "C3A270DCBE479A05ED865C7AEB395E19CBADCEEB575486F12A5D6F94B7F8452C")),
        "the exact OnVisible argument-marshalling thunk must remain explicit");
    require(
        PrivateGeometryCollectorX86Abi::ownerVtableByteCount == 0x50u
            && privateGeometryDigestEqual(
                PrivateGeometryCollectorX86Abi::ownerVtableSha256,
                fnvxr::engine::sha256FromHex(
                    "7F1CEFA617A2A3A0F07D944C0233436C9FEED2A2B3FA5AF379730D079D3EF838")),
        "the collector must pin the complete inherited BSCullingProcess vtable block");
    require(
        privateGeometryDigestEqual(
            PrivateGeometryCollectorX86Abi::appendSha256,
            fnvxr::engine::sha256FromHex(
                "7E198CD639B8CA514E2427DD3524300DF7D9CAF2D5181B915D917210FACFC9BE"))
            && privateGeometryCollectorStaticAbiMatchesRetailInventory(),
        "collector static ABI metadata must cross-check the complete append inventory");
    require(
        privateGeometryCollectorOnVisibleProductionEvidencePresent()
            && privateGeometryCollectorVtableBlockProductionEvidencePresent()
            && !PrivateGeometryCollectorX86Abi::onVisibleRuntimeVerificationImplemented
            && !PrivateGeometryCollectorX86Abi::fullClonedVtableProven
            && !PrivateGeometryCollectorX86Abi::runtimeActivationAuthorized
            && !PrivateGeometryCollectorX86Abi::activatesHook
            && !PrivateGeometryCollectorX86Abi::clonesVtable
            && !PrivateGeometryCollectorX86Abi::callsEngine
            && !PrivateGeometryCollectorX86Abi::writesProcessMemory
            && !privateGeometryCollectorProductionReady(),
        "runtime activation flags must remain false after static ABI promotion");

    constexpr std::uint64_t generation = 0x1122334455667788ull;
    constexpr RetailGeometryPointer32 owner = 0x10002000u;
    constexpr PrivateGeometryThreadToken thread = 0x8877665544332211ull;

    constexpr RetailGeometryPointer32 sourceVtableAddress = 0x101E2ECu;
    constexpr RetailGeometryPointer32 bindingX86Address = 0x20004000u;
    constexpr RetailGeometryPointer32 cloneVtableAddress =
        bindingX86Address
        + static_cast<RetailGeometryPointer32>(
            Binding::OwnedVtableCloneByteOffset);
    constexpr RetailGeometryPointer32 callbackAddress = 0x20005000u;
    std::array<RetailGeometryPointer32, Binding::OwnedVtableEntryCount>
        sourceVtable {};
    for (std::size_t index = 0u; index < sourceVtable.size(); ++index)
    {
        sourceVtable[index] = static_cast<RetailGeometryPointer32>(
            0x00500000u + index * 0x100u);
    }
    sourceVtable[Binding::AppendVtableEntryIndex] =
        PrivateGeometryCollectorX86Abi::preferredTargetAddress;

    Binding clonedBinding {};
    clonedBinding.cullingProcess()->base.vtable = sourceVtableAddress;
    const std::size_t allocationsBeforeClone = gAllocations;
    require(
        clonedBinding.installOwnedVtableClone(
            sourceVtable.data(),
            sourceVtable.size(),
            sourceVtableAddress,
            bindingX86Address,
            callbackAddress)
            == GeometryVtableInstallResult::Installed,
        "an exact stock culler vtable did not install its owned clone");
    require(
        gAllocations == allocationsBeforeClone
            && clonedBinding.ownedVtableCloneInstalled()
            && clonedBinding.ownedVtableIntegrityValid()
            && clonedBinding.cullingProcess()->base.vtable == cloneVtableAddress,
        "vtable installation did not remain allocation-free and owned");
    for (std::size_t index = 0u; index < sourceVtable.size(); ++index)
    {
        const RetailGeometryPointer32 expected =
            index == Binding::AppendVtableEntryIndex
            ? callbackAddress
            : sourceVtable[index];
        require(
            clonedBinding.ownedVtableCloneForAudit()[index] == expected,
            "the owned clone changed a non-Append vtable entry");
    }
    require(
        clonedBinding.sourceVtableSnapshotForAudit()
                [Binding::AppendVtableEntryIndex]
            == PrivateGeometryCollectorX86Abi::preferredTargetAddress,
        "the exact stock Append target was not retained for integrity checks");
    require(
        clonedBinding.resetCollectedGeometryPreservingOwnedVtable()
            && clonedBinding.ownedVtableCloneInstalled()
            && clonedBinding.ownedVtableIntegrityValid()
            && clonedBinding.cullingProcess()->base.vtable
                == cloneVtableAddress,
        "per-frame geometry reset must preserve the owned culler vtable clone");
    sourceVtable[0] ^= 0x1000u;
    require(
        clonedBinding.ownedVtableIntegrityValid()
            && clonedBinding.ownedVtableCloneForAudit()[0] != sourceVtable[0],
        "the binding retained borrowed source-vtable storage");
    require(
        clonedBinding.reset()
            && !clonedBinding.ownedVtableCloneInstalled()
            && clonedBinding.ownedVtableIntegrityValid()
            && clonedBinding.cullingProcess()->base.vtable == sourceVtableAddress,
        "reset did not restore the source vtable and clear clone ownership");
    for (RetailGeometryPointer32 entry
         : clonedBinding.ownedVtableCloneForAudit())
    {
        require(entry == 0u, "reset retained an owned clone entry");
    }

    sourceVtable[0] ^= 0x1000u;
    Binding nullSource {};
    nullSource.cullingProcess()->base.vtable = sourceVtableAddress;
    require(
        nullSource.installOwnedVtableClone(
            nullptr,
            sourceVtable.size(),
            sourceVtableAddress,
            bindingX86Address,
            callbackAddress)
            == GeometryVtableInstallResult::NullSource,
        "a null source vtable was accepted");

    Binding wrongCount {};
    wrongCount.cullingProcess()->base.vtable = sourceVtableAddress;
    require(
        wrongCount.installOwnedVtableClone(
            sourceVtable.data(),
            sourceVtable.size() - 1u,
            sourceVtableAddress,
            bindingX86Address,
            callbackAddress)
            == GeometryVtableInstallResult::WrongEntryCount,
        "an incomplete inherited vtable was accepted");

    auto invalidSource = sourceVtable;
    invalidSource[3] = 0u;
    Binding nullEntry {};
    nullEntry.cullingProcess()->base.vtable = sourceVtableAddress;
    require(
        nullEntry.installOwnedVtableClone(
            invalidSource.data(),
            invalidSource.size(),
            sourceVtableAddress,
            bindingX86Address,
            callbackAddress)
            == GeometryVtableInstallResult::NullSourceEntry,
        "a source vtable containing a null inherited entry was accepted");

    invalidSource = sourceVtable;
    invalidSource[Binding::AppendVtableEntryIndex] ^= 4u;
    Binding wrongAppend {};
    wrongAppend.cullingProcess()->base.vtable = sourceVtableAddress;
    require(
        wrongAppend.installOwnedVtableClone(
            invalidSource.data(),
            invalidSource.size(),
            sourceVtableAddress,
            bindingX86Address,
            callbackAddress)
            == GeometryVtableInstallResult::StockAppendTargetMismatch,
        "a changed stock Append vtable entry was accepted");

    Binding wrongOwnerVtable {};
    wrongOwnerVtable.cullingProcess()->base.vtable = sourceVtableAddress + 4u;
    require(
        wrongOwnerVtable.installOwnedVtableClone(
            sourceVtable.data(),
            sourceVtable.size(),
            sourceVtableAddress,
            bindingX86Address,
            callbackAddress)
            == GeometryVtableInstallResult::CullerSourceMismatch,
        "a source vtable not owned by the culler was accepted");

    Binding zeroCallback {};
    zeroCallback.cullingProcess()->base.vtable = sourceVtableAddress;
    require(
        zeroCallback.installOwnedVtableClone(
            sourceVtable.data(),
            sourceVtable.size(),
            sourceVtableAddress,
            bindingX86Address,
            0u)
            == GeometryVtableInstallResult::InvalidCallback,
        "a zero callback address was accepted");

    Binding stockCallback {};
    stockCallback.cullingProcess()->base.vtable = sourceVtableAddress;
    require(
        stockCallback.installOwnedVtableClone(
            sourceVtable.data(),
            sourceVtable.size(),
            sourceVtableAddress,
            bindingX86Address,
            PrivateGeometryCollectorX86Abi::preferredTargetAddress)
            == GeometryVtableInstallResult::InvalidCallback,
        "the unchanged stock Append target was accepted as a replacement");

    Binding corruptClone {};
    corruptClone.cullingProcess()->base.vtable = sourceVtableAddress;
    require(
        corruptClone.installOwnedVtableClone(
            sourceVtable.data(),
            sourceVtable.size(),
            sourceVtableAddress,
            bindingX86Address,
            callbackAddress)
            == GeometryVtableInstallResult::Installed,
        "clone corruption setup did not install");
    xorObjectByte(
        corruptClone,
        Binding::OwnedVtableCloneByteOffset + sizeof(RetailGeometryPointer32));
    require(
        !corruptClone.ownedVtableIntegrityValid()
            && corruptClone.failure() == GeometryCollectorFailure::BindingCorrupt
            && !corruptClone.reset(),
        "an altered owned vtable clone was not detected fail-closed");

    State ordered {};
    PrivateGeometrySealedView orderedView {};
    require(
        ordered.phase() == GeometryCollectorPhase::Inactive
            && ordered.failure() == GeometryCollectorFailure::None
            && ordered.collectedCountForAudit() == 0u
            && !ordered.tryGetSealedView(orderedView),
        "a new collector must be inactive and unable to expose storage");
    require(
        ordered.begin(generation, owner, thread),
        "an exact nonzero pass identity must enter Collecting");

    const std::size_t allocationsBeforeOrderedAppends = gAllocations;
    require(
        ordered.append(owner, generation, thread, 0xA000u)
                == GeometryAppendResult::Inserted
            && ordered.append(owner, generation, thread, 0xA000u)
                == GeometryAppendResult::Inserted
            && ordered.append(owner, generation, thread, 0xB000u)
                == GeometryAppendResult::Inserted,
        "every callback, including duplicates, must be retained in exact order");
    require(
        gAllocations == allocationsBeforeOrderedAppends,
        "the fixed collector append path must perform no heap allocation");
    require(
        !ordered.tryGetSealedView(orderedView),
        "collecting storage must not yield a premature view");
    require(
        ordered.seal(owner, generation, thread) == GeometrySealResult::Sealed
            && ordered.tryGetSealedView(orderedView)
            && orderedView.itemCount == 3u
            && orderedView.generation == generation
            && orderedView.geometryPointers != nullptr
            && orderedView.geometryPointers[0] == 0xA000u
            && orderedView.geometryPointers[1] == 0xA000u
            && orderedView.geometryPointers[2] == 0xB000u,
        "a sealed view must preserve exact callback multiplicity and order");

    require(
        ordered.append(owner, generation, thread, 0xC000u)
            == GeometryAppendResult::NotCollectingInvalidated,
        "a callback after sealing must invalidate the pass");
    require(
        ordered.failure() == GeometryCollectorFailure::InvalidTransition
            && !ordered.tryGetSealedView(orderedView)
            && orderedView.geometryPointers == nullptr
            && orderedView.itemCount == 0u,
        "a stale callback must revoke future view acquisition");
    require(ordered.reset(), "a logically failed but intact collector must reset");

    State empty {};
    PrivateGeometrySealedView emptyView {};
    require(
        empty.begin(generation + 1u, owner, thread)
            && empty.seal(owner, generation + 1u, thread) == GeometrySealResult::Sealed
            && empty.tryGetSealedView(emptyView)
            && emptyView.geometryPointers != nullptr
            && emptyView.itemCount == 0u
            && emptyView.generation == generation + 1u,
        "a completed empty traversal must produce a valid sealed empty view");

    PrivateGeometryCollectorState<2u> overflow {};
    PrivateGeometrySealedView rejectedView {
        reinterpret_cast<const RetailGeometryPointer32*>(std::uintptr_t { 1u }),
        99u,
        99u,
    };
    require(
        overflow.begin(generation, owner, thread)
            && overflow.append(owner, generation, thread, 0x1000u)
                == GeometryAppendResult::Inserted
            && overflow.append(owner, generation, thread, 0x2000u)
                == GeometryAppendResult::Inserted
            && overflow.append(owner, generation, thread, 0x3000u)
                == GeometryAppendResult::OverflowInvalidated,
        "capacity exhaustion must invalidate rather than truncate");
    require(
        overflow.phase() == GeometryCollectorPhase::Failed
            && overflow.failure() == GeometryCollectorFailure::CapacityExceeded
            && overflow.collectedCountForAudit() == 2u
            && !overflow.tryGetSealedView(rejectedView)
            && rejectedView.geometryPointers == nullptr
            && rejectedView.itemCount == 0u
            && rejectedView.generation == 0u,
        "an overflowed partial collection must never yield a view");

    State nullAfterPartial {};
    require(
        nullAfterPartial.begin(generation, owner, thread)
            && nullAfterPartial.append(owner, generation, thread, 0x1000u)
                == GeometryAppendResult::Inserted
            && nullAfterPartial.append(owner, generation, thread, 0u)
                == GeometryAppendResult::NullInvalidated
            && nullAfterPartial.failure() == GeometryCollectorFailure::NullGeometry
            && !nullAfterPartial.tryGetSealedView(rejectedView),
        "a null callback after valid callbacks must hide the complete partial collection");

    State wrongOwner {};
    require(
        wrongOwner.begin(generation, owner, thread)
            && wrongOwner.append(owner + 4u, generation, thread, 0x1000u)
                == GeometryAppendResult::OwnerMismatchInvalidated
            && wrongOwner.failure() == GeometryCollectorFailure::OwnerMismatch,
        "the callback owner must match the private culler exactly");

    State wrongGeneration {};
    require(
        wrongGeneration.begin(generation, owner, thread)
            && wrongGeneration.append(owner, generation + 1u, thread, 0x1000u)
                == GeometryAppendResult::GenerationMismatchInvalidated
            && wrongGeneration.failure() == GeometryCollectorFailure::GenerationMismatch,
        "a callback from another pass generation must fail closed");

    State wrongThread {};
    require(
        wrongThread.begin(generation, owner, thread)
            && wrongThread.append(owner, generation, thread + 1u, 0x1000u)
                == GeometryAppendResult::ThreadMismatchInvalidated
            && wrongThread.failure() == GeometryCollectorFailure::ThreadMismatch,
        "a callback from another thread token must fail closed");

    State corruptHead {};
    require(corruptHead.begin(generation, owner, thread), "head corruption setup must begin");
    xorObjectByte(corruptHead, State::HeadCanaryByteOffset);
    require(
        corruptHead.failure() == GeometryCollectorFailure::StateCorrupt
            && corruptHead.phase() == GeometryCollectorPhase::Failed
            && !corruptHead.tryGetSealedView(rejectedView)
            && !corruptHead.reset(),
        "a head-canary mismatch must remain failed until storage is reconstructed");

    State corruptTail {};
    require(corruptTail.begin(generation, owner, thread), "tail corruption setup must begin");
    xorObjectByte(corruptTail, State::TailCanaryByteOffset);
    require(
        corruptTail.append(owner, generation, thread, 0x1000u)
                == GeometryAppendResult::StateCorruptInvalidated
            && corruptTail.failure() == GeometryCollectorFailure::StateCorrupt
            && !corruptTail.tryGetSealedView(rejectedView),
        "a tail-canary mismatch must fail before touching item storage");

    State corruptCount {};
    require(corruptCount.begin(generation, owner, thread), "count corruption setup must begin");
    overwriteObjectUint32(
        corruptCount,
        State::ItemCountByteOffset,
        static_cast<std::uint32_t>(State::capacity() + 1u));
    require(
        corruptCount.append(owner, generation, thread, 0x1000u)
                == GeometryAppendResult::StateCorruptInvalidated
            && corruptCount.failure() == GeometryCollectorFailure::StateCorrupt
            && !corruptCount.tryGetSealedView(rejectedView),
        "count greater than capacity must fail before scanning or writing storage");

#if defined(_MSC_VER) && defined(_M_IX86)
    static_assert(PrivateGeometryCollectorX86Abi::actualCallbackCompiled);
    static_assert(std::is_same_v<
        decltype(&privateGeometryCollectorVslotCallback<4u>),
        PrivateGeometryCollectorVslotCallbackFunction<4u>>);

    Binding binding {};
    require(
        binding.beginCollection(generation),
        "the guarded private culler binding must begin on its owning thread");

    auto callback = &privateGeometryCollectorVslotCallback<4u>;
    auto* cullingProcess = binding.cullingProcess();
    void* geometry = reinterpret_cast<void*>(std::uintptr_t { 0x10203040u });
    std::uintptr_t stackBefore = 0u;
    std::uintptr_t stackAfter = 0u;
    const std::size_t allocationsBeforeCallback = gAllocations;

    __asm
    {
        mov stackBefore, esp
        push geometry
        mov edx, 055667788h
        mov ecx, cullingProcess
        mov eax, callback
        call eax
        mov stackAfter, esp
    }

    callback(cullingProcess, reinterpret_cast<void*>(std::uintptr_t { 0xDEADBEEFu }), geometry);
    require(
        stackAfter == stackBefore,
        "the x86 fastcall shim must consume the one geometry stack argument with ret 4");
    require(
        gAllocations == allocationsBeforeCallback,
        "the actual x86 vslot callback must perform no heap allocation");

    PrivateGeometrySealedView callbackView {};
    require(
        binding.sealCollection() == GeometrySealResult::Sealed
            && binding.tryGetSealedView(callbackView)
            && callbackView.itemCount == 2u
            && callbackView.geometryPointers[0] == 0x10203040u
            && callbackView.geometryPointers[1] == 0x10203040u,
        "the emulated vslot call must marshal ECX/stack arguments and preserve duplicates");

    Binding corruptBinding {};
    xorObjectByte(corruptBinding, Binding::CullingTailCanaryByteOffset);
    require(
        !corruptBinding.beginCollection(generation)
            && corruptBinding.failure() == GeometryCollectorFailure::BindingCorrupt
            && !corruptBinding.tryGetSealedView(rejectedView),
        "a culler-storage overrun must trip the adjacent binding canary");
#else
    static_assert(!PrivateGeometryCollectorX86Abi::actualCallbackCompiled);
#endif

    std::cout << "private geometry collector hardening passed\n";
    return EXIT_SUCCESS;
}
