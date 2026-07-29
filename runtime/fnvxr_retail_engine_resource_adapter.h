#pragma once

#include "fnvxr_engine_stereo_resources.h"
#include "fnvxr_retail_engine_calls.h"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace fnvxr::engine
{
enum class RetailEngineResourceAdapterFailure : std::uint8_t
{
    None,
    InvalidInitialization,
    ContextBusy,
    InvalidLease,
    InvalidMetadata,
    TokenExhausted,
    AllocationBusy,
    AllocationFailed,
    CleanupTokenMismatch,
    ConstructorInputMismatch,
    AccumulatorReferenceOwnershipMismatch,
    CullerVtableMismatch,
};

template <std::size_t CollectorCapacity>
class RetailEngineResourceContext;

namespace detail
{
template <std::size_t CollectorCapacity>
struct RetailEngineResourceAdapter;
}

// This context never grants resource or engine-call authority. It can only
// adapt an already resolved call table after the separate resource capability
// admits the transaction. Keeping those gates independent makes a forged
// operations table insufficient to invoke retail code.
template <std::size_t CollectorCapacity>
class RetailEngineResourceContext final
{
public:
    using Binding =
        geometry::PrivateGeometryCollectorBinding<CollectorCapacity>;

    RetailEngineResourceContext() noexcept = default;
    RetailEngineResourceContext(const RetailEngineResourceContext&) = delete;
    RetailEngineResourceContext& operator=(
        const RetailEngineResourceContext&) = delete;
    RetailEngineResourceContext(RetailEngineResourceContext&&) = delete;
    RetailEngineResourceContext& operator=(
        RetailEngineResourceContext&&) = delete;

    bool initialize(
        const RetailEngineCallResolution& resolution,
        std::uintptr_t loadedImageBase,
        const abi::RetailPointer32* verifiedCullerVtable,
        std::uint64_t contextGeneration) noexcept
    {
        if (mLeaseActive || anyResourceActive())
        {
            fail(RetailEngineResourceAdapterFailure::ContextBusy);
            return false;
        }
        if (!resolution.complete()
            || loadedImageBase != SupportedImageBase
            || !verifiedCullerVtable
            || contextGeneration == 0u)
        {
            fail(RetailEngineResourceAdapterFailure::InvalidInitialization);
            return false;
        }
        for (std::size_t index = 0u;
             index < Binding::OwnedVtableEntryCount;
             ++index)
        {
            if (verifiedCullerVtable[index] == 0u)
            {
                fail(RetailEngineResourceAdapterFailure::InvalidInitialization);
                return false;
            }
        }
        if (verifiedCullerVtable[Binding::AppendVtableEntryIndex]
            != geometry::PrivateGeometryCollectorX86Abi
                ::preferredTargetAddress)
        {
            fail(RetailEngineResourceAdapterFailure::InvalidInitialization);
            return false;
        }

        mCalls = resolution.calls;
        mVerifiedCullerVtable = verifiedCullerVtable;
        mContextGeneration = contextGeneration;
        mNextTokenGeneration = contextGeneration;
        mFailure = RetailEngineResourceAdapterFailure::None;
        mInitialized = true;
        return true;
    }

    bool ready() const noexcept
    {
        return mInitialized
            && mCalls.complete()
            && mVerifiedCullerVtable
            && mContextGeneration != 0u
            && mFailure == RetailEngineResourceAdapterFailure::None;
    }

    RetailEngineResourceAdapterFailure failure() const noexcept
    {
        return mFailure;
    }

private:
    struct AllocationRecord
    {
        void* address = nullptr;
        void* allocationBase = nullptr;
        std::uint32_t allocationByteCount = 0u;
        std::uint64_t tokenGeneration = 0u;
        bool constructed = false;

        bool active() const noexcept
        {
            return address && tokenGeneration != 0u;
        }

        void clear() noexcept
        {
            address = nullptr;
            allocationBase = nullptr;
            allocationByteCount = 0u;
            tokenGeneration = 0u;
            constructed = false;
        }
    };

    bool anyResourceActive() const noexcept
    {
        return mCamera.active()
            || mBinding.active()
            || mLeftAccumulator.active()
            || mRightAccumulator.active();
    }

    void fail(RetailEngineResourceAdapterFailure failure) noexcept
    {
        if (mFailure == RetailEngineResourceAdapterFailure::None)
            mFailure = failure;
    }

    std::uint64_t issueTokenGeneration() noexcept
    {
        if (mNextTokenGeneration == 0u
            || mNextTokenGeneration
                == (std::numeric_limits<std::uint64_t>::max)())
        {
            fail(RetailEngineResourceAdapterFailure::TokenExhausted);
            return 0u;
        }
        return mNextTokenGeneration++;
    }

    AllocationRecord& accumulatorRecord(StereoResourceKind kind) noexcept
    {
        return kind == StereoResourceKind::LeftAccumulator
            ? mLeftAccumulator
            : mRightAccumulator;
    }

    const AllocationRecord& accumulatorRecord(
        StereoResourceKind kind) const noexcept
    {
        return kind == StereoResourceKind::LeftAccumulator
            ? mLeftAccumulator
            : mRightAccumulator;
    }

    bool mInitialized = false;
    bool mLeaseActive = false;
    RetailEngineCalls mCalls {};
    const abi::RetailPointer32* mVerifiedCullerVtable = nullptr;
    std::uint64_t mContextGeneration = 0u;
    std::uint64_t mNextTokenGeneration = 0u;
    RetailEngineResourceAdapterFailure mFailure =
        RetailEngineResourceAdapterFailure::None;
    AllocationRecord mCamera {};
    AllocationRecord mBinding {};
    AllocationRecord mLeftAccumulator {};
    AllocationRecord mRightAccumulator {};

    friend struct detail::RetailEngineResourceAdapter<CollectorCapacity>;
};

namespace detail
{
template <std::size_t CollectorCapacity>
struct RetailEngineResourceAdapter
{
    using Context = RetailEngineResourceContext<CollectorCapacity>;
    using Binding = typename Context::Binding;
    using Operations = StereoResourceOperations<CollectorCapacity>;
    using AllocationRecord = typename Context::AllocationRecord;

    static Context* fromLease(
        const StereoContextLeaseToken& lease) noexcept
    {
        if (!lease.valid())
            return nullptr;
        auto* context = reinterpret_cast<Context*>(lease.opaqueHandle());
        if (!context
            || !context->mLeaseActive
            || !context->mInitialized
            || lease.generation() != context->mContextGeneration)
        {
            return nullptr;
        }
        return context;
    }

    static bool metadataMatches(
        const StereoResourceLayoutMetadata& actual,
        const StereoResourceLayoutMetadata& expected) noexcept
    {
        return actual == expected
            && actual.allocationByteCount
                <= (std::numeric_limits<std::uint32_t>::max)();
    }

    static StereoContextLeaseToken retain(void* bootstrap) noexcept
    {
        auto* context = static_cast<Context*>(bootstrap);
        if (!context || !context->ready() || context->mLeaseActive)
        {
            if (context)
                context->fail(RetailEngineResourceAdapterFailure::ContextBusy);
            return {};
        }
        context->mLeaseActive = true;
        return StereoContextLeaseToken::fromOpaque(
            reinterpret_cast<std::uintptr_t>(context),
            context->mContextGeneration);
    }

    static void release(StereoContextLeaseToken lease) noexcept
    {
        Context* context = fromLease(lease);
        if (!context)
            return;
        context->mLeaseActive = false;
    }

    static StereoCameraAcquisition acquireCamera(
        const StereoContextLeaseToken& lease,
        const StereoResourceLayoutMetadata& metadata) noexcept
    {
        Context* context = fromLease(lease);
        if (!context)
            return {};
        if (!metadataMatches(metadata, StereoCameraLayoutMetadata))
        {
            context->fail(RetailEngineResourceAdapterFailure::InvalidMetadata);
            return {};
        }
        if (context->mCamera.active())
        {
            context->fail(RetailEngineResourceAdapterFailure::AllocationBusy);
            return {};
        }
        const std::uint64_t tokenGeneration = context->issueTokenGeneration();
        if (tokenGeneration == 0u)
            return {};

        abi::RetailNiCameraLayout* camera = context->mCalls.niCameraCreate();
        if (!camera)
            return { true, nullptr, {} };
        context->mCamera = {
            camera,
            camera,
            static_cast<std::uint32_t>(
                StereoCameraLayoutMetadata.allocationByteCount),
            tokenGeneration,
            true,
        };
        return {
            true,
            camera,
            StereoCameraCleanupToken::fromOpaque(
                reinterpret_cast<std::uintptr_t>(camera),
                tokenGeneration),
        };
    }

    static void releaseCamera(
        const StereoContextLeaseToken& lease,
        StereoCameraCleanupToken token) noexcept
    {
        Context* context = fromLease(lease);
        if (!context)
            return;
        if (!context->mCamera.active()
            || token.opaqueHandle()
                != reinterpret_cast<std::uintptr_t>(context->mCamera.address)
            || token.generation() != context->mCamera.tokenGeneration)
        {
            context->fail(
                RetailEngineResourceAdapterFailure::CleanupTokenMismatch);
            return;
        }
        context->mCalls.niRefObjectFree(
            reinterpret_cast<abi::RetailNiAccumulatorLayout*>(
                context->mCamera.address));
        context->mCamera.clear();
    }

    static typename Operations::BindingStorageAcquisition allocateBinding(
        const StereoContextLeaseToken& lease,
        const StereoResourceLayoutMetadata& metadata) noexcept
    {
        Context* context = fromLease(lease);
        if (!context)
            return {};
        if (!metadataMatches(
                metadata,
                stereoCollectorBindingLayoutMetadata<CollectorCapacity>()))
        {
            context->fail(RetailEngineResourceAdapterFailure::InvalidMetadata);
            return {};
        }
        if (context->mBinding.active())
        {
            context->fail(RetailEngineResourceAdapterFailure::AllocationBusy);
            return {};
        }
        const std::uint64_t tokenGeneration = context->issueTokenGeneration();
        if (tokenGeneration == 0u)
            return {};
        if (metadata.allocationAlignment == 0u
            || (metadata.allocationAlignment
                    & (metadata.allocationAlignment - 1u)) != 0u)
        {
            context->fail(RetailEngineResourceAdapterFailure::InvalidMetadata);
            return {};
        }
        const std::size_t alignmentPadding =
            metadata.allocationAlignment - 1u;
        if (metadata.allocationByteCount
            > (std::numeric_limits<std::uint32_t>::max)()
                - alignmentPadding)
        {
            context->fail(RetailEngineResourceAdapterFailure::InvalidMetadata);
            return {};
        }
        const auto allocationByteCount = static_cast<std::uint32_t>(
            metadata.allocationByteCount + alignmentPadding);
        void* allocationBase = context->mCalls.niAllocate(
            allocationByteCount);
        if (!allocationBase)
            return { true, nullptr, {} };
        const std::uintptr_t allocationAddress =
            reinterpret_cast<std::uintptr_t>(allocationBase);
        if (allocationAddress
            > (std::numeric_limits<std::uintptr_t>::max)()
                - alignmentPadding)
        {
            context->mCalls.niFree(allocationBase, allocationByteCount);
            context->fail(RetailEngineResourceAdapterFailure::AllocationFailed);
            return {};
        }
        const std::uintptr_t alignedAddress =
            (allocationAddress + alignmentPadding)
            & ~static_cast<std::uintptr_t>(alignmentPadding);
        void* alignedStorage = reinterpret_cast<void*>(alignedAddress);
        context->mBinding = {
            alignedStorage,
            allocationBase,
            allocationByteCount,
            tokenGeneration,
            false,
        };
        return {
            true,
            static_cast<Binding*>(alignedStorage),
            StereoBindingStorageCleanupToken::fromOpaque(
                alignedAddress,
                tokenGeneration),
        };
    }

    static void freeBinding(
        const StereoContextLeaseToken& lease,
        StereoBindingStorageCleanupToken token,
        const StereoResourceLayoutMetadata& metadata) noexcept
    {
        Context* context = fromLease(lease);
        if (!context)
            return;
        if (!metadataMatches(
                metadata,
                stereoCollectorBindingLayoutMetadata<CollectorCapacity>()))
        {
            context->fail(RetailEngineResourceAdapterFailure::InvalidMetadata);
            return;
        }
        if (!context->mBinding.active()
            || token.opaqueHandle()
                != reinterpret_cast<std::uintptr_t>(context->mBinding.address)
            || token.generation() != context->mBinding.tokenGeneration)
        {
            context->fail(
                RetailEngineResourceAdapterFailure::CleanupTokenMismatch);
            return;
        }
        context->mCalls.niFree(
            context->mBinding.allocationBase,
            context->mBinding.allocationByteCount);
        context->mBinding.clear();
    }

    static StereoConstructorInvocationResult<abi::RetailBSCullingProcessLayout>
    constructCuller(
        const StereoContextLeaseToken& lease,
        abi::RetailBSCullingProcessLayout* storage,
        std::uint32_t baseConstructorArgument) noexcept
    {
        Context* context = fromLease(lease);
        if (!context)
            return {};
        auto* binding = static_cast<Binding*>(context->mBinding.address);
        if (!context->mBinding.active()
            || !binding
            || storage != binding->cullingProcess()
            || baseConstructorArgument
                != RetailWorldCullerConstructorArgument)
        {
            context->fail(
                RetailEngineResourceAdapterFailure::ConstructorInputMismatch);
            return {};
        }

        abi::RetailBSCullingProcessLayout* returned =
            context->mCalls.cullingProcessConstruct(
                storage,
                baseConstructorArgument);
        context->mBinding.constructed = true;
        if (returned != storage)
            return { true, returned };

        // The constructor must retain the exact runtime-verified BSCulling
        // table. A later scoped collector clones this table privately; it
        // never accepts an arbitrary constructor dispatch target.
        const std::uintptr_t verifiedVtableAddress =
            reinterpret_cast<std::uintptr_t>(context->mVerifiedCullerVtable);
#if defined(_MSC_VER) && defined(_M_IX86)
        const bool cullerVtableMatches = verifiedVtableAddress
                <= (std::numeric_limits<abi::RetailPointer32>::max)()
            && storage->base.vtable
                == static_cast<abi::RetailPointer32>(verifiedVtableAddress);
#else
        static_cast<void>(verifiedVtableAddress);
        const bool cullerVtableMatches = storage->base.vtable
            == static_cast<abi::RetailPointer32>(
                abi::BSCullingProcessVtableAddress);
#endif
        if (!cullerVtableMatches)
        {
            context->fail(
                RetailEngineResourceAdapterFailure::CullerVtableMismatch);
            return { true, nullptr };
        }
        return { true, returned };
    }

    static void destroyCuller(
        const StereoContextLeaseToken& lease,
        abi::RetailBSCullingProcessLayout* culler) noexcept
    {
        Context* context = fromLease(lease);
        if (!context)
            return;
        auto* binding = static_cast<Binding*>(context->mBinding.address);
        if (!context->mBinding.active()
            || !context->mBinding.constructed
            || !binding
            || culler != binding->cullingProcess())
        {
            context->fail(
                RetailEngineResourceAdapterFailure::ConstructorInputMismatch);
            return;
        }
        (void)binding->reset();
        context->mCalls.cullingProcessDestroy(culler);
        context->mBinding.constructed = false;
    }

    static typename Operations::AccumulatorStorageAcquisition
    allocateAccumulator(
        const StereoContextLeaseToken& lease,
        StereoResourceKind kind,
        const StereoResourceLayoutMetadata& metadata) noexcept
    {
        Context* context = fromLease(lease);
        if (!context)
            return {};
        if ((kind != StereoResourceKind::LeftAccumulator
                && kind != StereoResourceKind::RightAccumulator)
            || !metadataMatches(
                metadata,
                stereoAccumulatorLayoutMetadata(kind)))
        {
            context->fail(RetailEngineResourceAdapterFailure::InvalidMetadata);
            return {};
        }
        AllocationRecord& record = context->accumulatorRecord(kind);
        if (record.active())
        {
            context->fail(RetailEngineResourceAdapterFailure::AllocationBusy);
            return {};
        }
        const std::uint64_t tokenGeneration = context->issueTokenGeneration();
        if (tokenGeneration == 0u)
            return {};
        void* raw = context->mCalls.niAllocate(
            static_cast<std::uint32_t>(metadata.allocationByteCount));
        if (!raw)
            return { true, nullptr, {} };
        record = {
            raw,
            raw,
            static_cast<std::uint32_t>(metadata.allocationByteCount),
            tokenGeneration,
            false,
        };
        return {
            true,
            static_cast<abi::RetailBSShaderAccumulatorLayout*>(raw),
            StereoAccumulatorStorageCleanupToken::fromOpaque(
                reinterpret_cast<std::uintptr_t>(raw),
                tokenGeneration),
        };
    }

    static void freeAccumulator(
        const StereoContextLeaseToken& lease,
        StereoResourceKind kind,
        StereoAccumulatorStorageCleanupToken token,
        const StereoResourceLayoutMetadata& metadata) noexcept
    {
        Context* context = fromLease(lease);
        if (!context)
            return;
        if ((kind != StereoResourceKind::LeftAccumulator
                && kind != StereoResourceKind::RightAccumulator)
            || !metadataMatches(
                metadata,
                stereoAccumulatorLayoutMetadata(kind)))
        {
            context->fail(RetailEngineResourceAdapterFailure::InvalidMetadata);
            return;
        }
        AllocationRecord& record = context->accumulatorRecord(kind);
        if (!record.active()
            || token.opaqueHandle()
                != reinterpret_cast<std::uintptr_t>(record.address)
            || token.generation() != record.tokenGeneration)
        {
            context->fail(
                RetailEngineResourceAdapterFailure::CleanupTokenMismatch);
            return;
        }
        context->mCalls.niFree(
            record.address,
            static_cast<std::uint32_t>(metadata.allocationByteCount));
        record.clear();
    }

    static StereoConstructorInvocationResult<
        abi::RetailBSShaderAccumulatorLayout>
    constructAccumulator(
        const StereoContextLeaseToken& lease,
        StereoResourceKind kind,
        abi::RetailBSShaderAccumulatorLayout* storage,
        const StereoResourceConstructionParameters& parameters) noexcept
    {
        Context* context = fromLease(lease);
        if (!context)
            return {};
        if ((kind != StereoResourceKind::LeftAccumulator
                && kind != StereoResourceKind::RightAccumulator)
            || parameters
                != RetailWorldStereoResourceConstructionParameters)
        {
            context->fail(
                RetailEngineResourceAdapterFailure::ConstructorInputMismatch);
            return {};
        }
        AllocationRecord& record = context->accumulatorRecord(kind);
        if (!record.active() || record.address != storage)
        {
            context->fail(
                RetailEngineResourceAdapterFailure::ConstructorInputMismatch);
            return {};
        }
        abi::RetailBSShaderAccumulatorLayout* returned =
            context->mCalls.shaderAccumulatorConstruct(
                storage,
                parameters.accumulatorConstructorMode,
                parameters.accumulatorBatchRendererCount,
                parameters.accumulatorMaximumPassCount);
        record.constructed = true;
        if (storage->referenceCount != 0u)
        {
            context->fail(
                RetailEngineResourceAdapterFailure
                    ::AccumulatorReferenceOwnershipMismatch);
            return { true, nullptr };
        }
        // NiRefObject constructors begin at reference count zero.  This raw
        // resource owner holds one explicit reference so the culler's scoped
        // NiPointer binding can increment/decrement without deleting the
        // accumulator before its eye render.
        storage->referenceCount = 1u;
        return { true, returned };
    }

    static void destroyAccumulator(
        const StereoContextLeaseToken& lease,
        StereoResourceKind kind,
        abi::RetailBSShaderAccumulatorLayout* accumulator,
        StereoAccumulatorStorageCleanupToken token,
        const StereoResourceLayoutMetadata& metadata) noexcept
    {
        Context* context = fromLease(lease);
        if (!context)
            return;
        if ((kind != StereoResourceKind::LeftAccumulator
                && kind != StereoResourceKind::RightAccumulator)
            || !metadataMatches(
                metadata,
                stereoAccumulatorLayoutMetadata(kind)))
        {
            context->fail(RetailEngineResourceAdapterFailure::InvalidMetadata);
            return;
        }
        AllocationRecord& record = context->accumulatorRecord(kind);
        if (!record.active()
            || !record.constructed
            || record.address != accumulator
            || token.opaqueHandle()
                != reinterpret_cast<std::uintptr_t>(record.address)
            || token.generation() != record.tokenGeneration)
        {
            context->fail(
                RetailEngineResourceAdapterFailure::CleanupTokenMismatch);
            return;
        }

        // A failed frame must not leave the private culler's NiPointer holding
        // an accumulator past its owner teardown. Release that scoped engine
        // reference while both objects are still alive, then require the sole
        // explicit owner reference before invoking the deleting destructor.
        if (context->mBinding.active()
            && context->mBinding.constructed)
        {
            auto* binding = static_cast<Binding*>(
                context->mBinding.address);
            auto* culler = binding ? binding->cullingProcess() : nullptr;
            const std::uintptr_t accumulatorAddress =
                reinterpret_cast<std::uintptr_t>(accumulator);
            if (culler
                && accumulatorAddress
                    <= (std::numeric_limits<abi::RetailPointer32>::max)()
                && culler->shaderAccumulator
                    == static_cast<abi::RetailPointer32>(
                        accumulatorAddress))
            {
                context->mCalls.cullingProcessSetAccumulator(culler, nullptr);
                if (culler->shaderAccumulator != 0u)
                {
                    context->fail(
                        RetailEngineResourceAdapterFailure
                            ::AccumulatorReferenceOwnershipMismatch);
                    return;
                }
            }
        }
        if (accumulator->referenceCount != 1u)
        {
            context->fail(
                RetailEngineResourceAdapterFailure
                    ::AccumulatorReferenceOwnershipMismatch);
            return;
        }
        accumulator->referenceCount = 0u;
        (void)context->mCalls.shaderAccumulatorDestroy(accumulator, 1u);
        record.clear();
    }

    static Operations operations(Context& context) noexcept
    {
        Operations result {};
        result.bootstrapContext = &context;
        result.retainContextLease = &retain;
        result.releaseContextLease = &release;
        result.acquireCamera = &acquireCamera;
        result.releaseCameraOwnership = &releaseCamera;
        result.allocateCollectorBindingRawStorage = &allocateBinding;
        result.freeCollectorBindingRawStorage = &freeBinding;
        result.invokeCullingConstructor = &constructCuller;
        result.destroyCullingProcessInPlace = &destroyCuller;
        result.allocateAccumulatorRawStorage = &allocateAccumulator;
        result.freeAccumulatorRawStorage = &freeAccumulator;
        result.invokeAccumulatorConstructor = &constructAccumulator;
        result.destroyAndReleaseAccumulator = &destroyAccumulator;
        return result;
    }
};
}

template <std::size_t CollectorCapacity>
StereoResourceOperations<CollectorCapacity>
makeRetailEngineStereoResourceOperations(
    RetailEngineResourceContext<CollectorCapacity>& context) noexcept
{
    return detail::RetailEngineResourceAdapter<CollectorCapacity>::operations(
        context);
}
}
