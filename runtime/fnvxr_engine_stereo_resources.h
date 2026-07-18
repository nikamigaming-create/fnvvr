#pragma once

#include "fnvxr_private_geometry_collector.h"
#include "fnvxr_retail_engine_abi.h"
#include "fnvxr_retail_renderer_contract.h"
#include "fnvxr_retail_runtime_binding.h"

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace fnvxr::engine
{
enum class StereoResourceKind : std::uint8_t
{
    Camera,
    CollectorBinding,
    LeftAccumulator,
    RightAccumulator,
};

struct StereoResourceLayoutMetadata
{
    StereoResourceKind kind = StereoResourceKind::Camera;
    std::size_t allocationByteCount = 0u;
    std::size_t allocationAlignment = 0u;
    std::size_t engineObjectByteOffset = 0u;
    std::size_t engineObjectByteCount = 0u;
};

constexpr bool operator==(
    const StereoResourceLayoutMetadata& left,
    const StereoResourceLayoutMetadata& right) noexcept
{
    return left.kind == right.kind
        && left.allocationByteCount == right.allocationByteCount
        && left.allocationAlignment == right.allocationAlignment
        && left.engineObjectByteOffset == right.engineObjectByteOffset
        && left.engineObjectByteCount == right.engineObjectByteCount;
}

constexpr bool operator!=(
    const StereoResourceLayoutMetadata& left,
    const StereoResourceLayoutMetadata& right) noexcept
{
    return !(left == right);
}

inline constexpr StereoResourceLayoutMetadata StereoCameraLayoutMetadata {
    StereoResourceKind::Camera,
    sizeof(abi::RetailNiCameraLayout),
    alignof(abi::RetailNiCameraLayout),
    0u,
    sizeof(abi::RetailNiCameraLayout),
};

inline constexpr std::size_t StereoAccumulatorLayoutByteCount =
    sizeof(abi::RetailBSShaderAccumulatorLayout);

constexpr StereoResourceLayoutMetadata stereoAccumulatorLayoutMetadata(
    StereoResourceKind kind) noexcept
{
    return {
        kind,
        sizeof(abi::RetailBSShaderAccumulatorLayout),
        alignof(abi::RetailBSShaderAccumulatorLayout),
        0u,
        sizeof(abi::RetailBSShaderAccumulatorLayout),
    };
}

template <std::size_t CollectorCapacity>
constexpr StereoResourceLayoutMetadata
stereoCollectorBindingLayoutMetadata() noexcept
{
    using Binding =
        geometry::PrivateGeometryCollectorBinding<CollectorCapacity>;
    static_assert(
        Binding::CullingProcessByteOffset + Binding::CullingProcessByteCount
        <= sizeof(Binding));
    return {
        StereoResourceKind::CollectorBinding,
        sizeof(Binding),
        alignof(Binding),
        Binding::CullingProcessByteOffset,
        Binding::CullingProcessByteCount,
    };
}

static_assert(sizeof(abi::RetailNiCameraLayout) == 0x114u);
static_assert(sizeof(abi::RetailBSCullingProcessLayout) == 0xC8u);
static_assert(sizeof(abi::RetailBSShaderAccumulatorLayout) == 0x280u);
static_assert(
    StereoCameraLayoutMetadata.allocationByteCount
    == StereoCameraLayoutMetadata.engineObjectByteCount);
static_assert(
    StereoAccumulatorLayoutByteCount
    == stereoAccumulatorLayoutMetadata(
        StereoResourceKind::LeftAccumulator).engineObjectByteCount);

struct StereoResourceConstructionParameters
{
    // Callers are still validated transactionally, but the production values
    // are no longer unknown: they are pinned by the captured retail call sites
    // in fnvxr_retail_renderer_contract.h.
    std::uint32_t cullingBaseConstructorArgument = 0u;
    std::uint32_t accumulatorConstructorMode = 0u;
    std::uint32_t accumulatorBatchRendererCount = 0u;
    std::uint32_t accumulatorMaximumPassCount = 0u;
};

inline constexpr StereoResourceConstructionParameters
    RetailWorldStereoResourceConstructionParameters {
        RetailWorldCullerConstructorArgument,
        RetailWorldAccumulatorConstructorMode,
        RetailWorldAccumulatorBatchRendererCount,
        RetailWorldAccumulatorMaximumPassCount,
    };

constexpr bool operator==(
    const StereoResourceConstructionParameters& left,
    const StereoResourceConstructionParameters& right) noexcept
{
    return left.cullingBaseConstructorArgument
            == right.cullingBaseConstructorArgument
        && left.accumulatorConstructorMode
            == right.accumulatorConstructorMode
        && left.accumulatorBatchRendererCount
            == right.accumulatorBatchRendererCount
        && left.accumulatorMaximumPassCount
            == right.accumulatorMaximumPassCount;
}

constexpr bool operator!=(
    const StereoResourceConstructionParameters& left,
    const StereoResourceConstructionParameters& right) noexcept
{
    return !(left == right);
}

template <typename Tag>
class StereoOpaqueLifecycleToken final
{
public:
    constexpr StereoOpaqueLifecycleToken() noexcept = default;

    static constexpr StereoOpaqueLifecycleToken fromOpaque(
        std::uintptr_t handle,
        std::uint64_t generation) noexcept
    {
        return StereoOpaqueLifecycleToken(handle, generation);
    }

    StereoOpaqueLifecycleToken(const StereoOpaqueLifecycleToken&) = delete;
    StereoOpaqueLifecycleToken& operator=(
        const StereoOpaqueLifecycleToken&) = delete;

    constexpr StereoOpaqueLifecycleToken(
        StereoOpaqueLifecycleToken&& other) noexcept
        : mHandle(other.mHandle),
          mGeneration(other.mGeneration)
    {
        other.clear();
    }

    constexpr StereoOpaqueLifecycleToken& operator=(
        StereoOpaqueLifecycleToken&& other) noexcept
    {
        if (this != &other)
        {
            mHandle = other.mHandle;
            mGeneration = other.mGeneration;
            other.clear();
        }
        return *this;
    }

    constexpr bool valid() const noexcept
    {
        return mHandle != 0u && mGeneration != 0u;
    }

    constexpr std::uintptr_t opaqueHandle() const noexcept
    {
        return mHandle;
    }

    constexpr std::uint64_t generation() const noexcept
    {
        return mGeneration;
    }

    constexpr bool identifiesSameCapabilityAs(
        const StereoOpaqueLifecycleToken& other) const noexcept
    {
        return valid()
            && other.valid()
            && mHandle == other.mHandle
            && mGeneration == other.mGeneration;
    }

private:
    constexpr StereoOpaqueLifecycleToken(
        std::uintptr_t handle,
        std::uint64_t generation) noexcept
        : mHandle(handle),
          mGeneration(generation)
    {
    }

    constexpr void clear() noexcept
    {
        mHandle = 0u;
        mGeneration = 0u;
    }

    std::uintptr_t mHandle = 0u;
    std::uint64_t mGeneration = 0u;
};

struct StereoContextLeaseTokenTag;
struct StereoCameraCleanupTokenTag;
struct StereoBindingStorageCleanupTokenTag;
struct StereoAccumulatorStorageCleanupTokenTag;
using StereoContextLeaseToken =
    StereoOpaqueLifecycleToken<StereoContextLeaseTokenTag>;
using StereoCameraCleanupToken =
    StereoOpaqueLifecycleToken<StereoCameraCleanupTokenTag>;
using StereoBindingStorageCleanupToken =
    StereoOpaqueLifecycleToken<StereoBindingStorageCleanupTokenTag>;
using StereoAccumulatorStorageCleanupToken =
    StereoOpaqueLifecycleToken<StereoAccumulatorStorageCleanupTokenTag>;

template <typename Storage, typename CleanupToken>
struct StereoRawStorageAcquisition
{
    // A false result means no usable storage result was produced. A valid
    // cleanup token is nevertheless an ownership transfer and must be
    // released; it can represent cleanup for a partially completed backend
    // operation. The candidate storage address is never ownership evidence.
    bool invocationReturned = false;
    Storage* storage = nullptr;
    CleanupToken cleanupToken {};
};

template <typename Resource>
struct StereoConstructorInvocationResult
{
    // False guarantees the retail constructor was not invoked and no object
    // lifetime began. True means the invocation returned and the intended
    // storage must be treated as constructed, even when returnedObject is not
    // the storage pointer. There is no recoverable "invoked but did not
    // return" state in this noexcept transaction.
    bool invocationReturned = false;
    Resource* returnedObject = nullptr;
};

struct StereoCameraAcquisition
{
    // A valid cleanup token is the sole ownership-transfer signal. It remains
    // usable even when the candidate camera pointer is null or malformed.
    bool factoryInvocationReturned = false;
    abi::RetailNiCameraLayout* camera = nullptr;
    StereoCameraCleanupToken cleanupToken {};
};

// This is a capability, not a configuration boolean. The only production
// issuer is the single same-process retail runtime authority bundle. The
// lifecycle test retains its isolated friend issuer.
class StereoResourceAuthorization;

namespace detail
{
struct StereoResourceAuthorizationAccess;
template <std::size_t CollectorCapacity>
struct StereoResourceBuilder;
}

struct StereoResourceLifecycleTestAuthority;

class StereoResourceAuthorization final
{
public:
    constexpr StereoResourceAuthorization() noexcept = default;

private:
    using Validator = bool (*)(const void*) noexcept;

    explicit constexpr StereoResourceAuthorization(
        const void* evidence,
        Validator validator) noexcept
        : mEvidence(evidence),
          mValidator(validator)
    {
    }

    explicit constexpr StereoResourceAuthorization(
        const detail::RetailRuntimeBinding& binding,
        detail::RetailRuntimeBindingValidator validator) noexcept
        : mBinding(binding),
          mBindingValidator(validator)
    {
    }

    const void* mEvidence = nullptr;
    Validator mValidator = nullptr;
    detail::RetailRuntimeBinding mBinding {};
    detail::RetailRuntimeBindingValidator mBindingValidator = nullptr;

    friend struct detail::StereoResourceAuthorizationAccess;
    friend struct detail::RetailRuntimeAuthorityIssuer;
    friend struct StereoResourceLifecycleTestAuthority;
};

inline constexpr bool StereoResourceProductionAuthorizationAvailable =
    RetailRuntimeProductionAuthorizationAvailable;

namespace detail
{
struct StereoResourceAuthorizationAccess
{
    static bool authorized(
        const StereoResourceAuthorization& authorization) noexcept
    {
        if (authorization.mBindingValidator)
        {
            return authorization.mBindingValidator(
                authorization.mBinding);
        }
        return authorization.mEvidence && authorization.mValidator
            && authorization.mValidator(authorization.mEvidence);
    }
};
}

enum class StereoResourceFailure : std::uint8_t
{
    None,
    OperationTableIncomplete,
    Unauthorized,
    ContextLeaseRetainFailed,
    CameraFactoryNotInvoked,
    CameraWasNull,
    CameraCleanupTokenMissing,
    CollectorBindingAllocationFailed,
    CollectorBindingCleanupTokenMissing,
    AccumulatorCleanupCapabilityAlias,
    ResourceAlias,
    ResourceAlignmentInvalid,
    CullingConstructorNotInvoked,
    CullingConstructorDidNotReturnThis,
    CollectorBindingIntegrityFailed,
    LeftAccumulatorAllocationFailed,
    LeftAccumulatorCleanupTokenMissing,
    LeftAccumulatorConstructorNotInvoked,
    LeftAccumulatorConstructorDidNotReturnThis,
    RightAccumulatorAllocationFailed,
    RightAccumulatorCleanupTokenMissing,
    RightAccumulatorConstructorNotInvoked,
    RightAccumulatorConstructorDidNotReturnThis,
};

enum class StereoResourceView : std::uint8_t
{
    Center,
};

struct StereoResourceViewPlan
{
    StereoResourceView left = StereoResourceView::Center;
    StereoResourceView right = StereoResourceView::Center;
};

template <std::size_t CollectorCapacity>
struct StereoResourceOperations
{
    using Binding =
        geometry::PrivateGeometryCollectorBinding<CollectorCapacity>;
    using Camera = abi::RetailNiCameraLayout;
    using Culler = abi::RetailBSCullingProcessLayout;
    using Accumulator = abi::RetailBSShaderAccumulatorLayout;
    using BindingStorageAcquisition = StereoRawStorageAcquisition<
        Binding,
        StereoBindingStorageCleanupToken>;
    using AccumulatorStorageAcquisition = StereoRawStorageAcquisition<
        Accumulator,
        StereoAccumulatorStorageCleanupToken>;

    // bootstrapContext is borrowed only for the synchronous retain call. Every
    // later operation, including teardown, receives the retained lease. A
    // valid lease is released last and exactly once.
    void* bootstrapContext = nullptr;
    StereoContextLeaseToken (*retainContextLease)(void*) noexcept = nullptr;
    void (*releaseContextLease)(StereoContextLeaseToken) noexcept = nullptr;

    StereoCameraAcquisition (*acquireCamera)(
        const StereoContextLeaseToken&,
        const StereoResourceLayoutMetadata&) noexcept = nullptr;
    void (*releaseCameraOwnership)(
        const StereoContextLeaseToken&,
        StereoCameraCleanupToken) noexcept = nullptr;

    // The culler destructor is explicitly in-place/non-deleting. The binding
    // wrapper and its raw allocation remain owned until the separate token
    // release. Every valid token names one independent cleanup obligation and
    // remains safe to consume when the candidate pointer is null, malformed,
    // aliased, or overlapping another candidate.
    BindingStorageAcquisition (*allocateCollectorBindingRawStorage)(
        const StereoContextLeaseToken&,
        const StereoResourceLayoutMetadata&) noexcept = nullptr;
    void (*freeCollectorBindingRawStorage)(
        const StereoContextLeaseToken&,
        StereoBindingStorageCleanupToken,
        const StereoResourceLayoutMetadata&) noexcept = nullptr;
    StereoConstructorInvocationResult<Culler> (*invokeCullingConstructor)(
        const StereoContextLeaseToken&,
        Culler*,
        std::uint32_t) noexcept = nullptr;
    void (*destroyCullingProcessInPlace)(
        const StereoContextLeaseToken&,
        Culler*) noexcept = nullptr;

    // Unconstructed raw accumulator storage is freed by the raw-storage path.
    // Once invocationReturned is true, destroyAndReleaseAccumulator is the
    // only legal teardown operation and atomically destroys and frees it.
    AccumulatorStorageAcquisition (*allocateAccumulatorRawStorage)(
        const StereoContextLeaseToken&,
        StereoResourceKind,
        const StereoResourceLayoutMetadata&) noexcept = nullptr;
    void (*freeAccumulatorRawStorage)(
        const StereoContextLeaseToken&,
        StereoResourceKind,
        StereoAccumulatorStorageCleanupToken,
        const StereoResourceLayoutMetadata&) noexcept = nullptr;
    StereoConstructorInvocationResult<Accumulator> (*invokeAccumulatorConstructor)(
        const StereoContextLeaseToken&,
        StereoResourceKind,
        Accumulator*,
        const StereoResourceConstructionParameters&) noexcept = nullptr;
    void (*destroyAndReleaseAccumulator)(
        const StereoContextLeaseToken&,
        StereoResourceKind,
        Accumulator*,
        StereoAccumulatorStorageCleanupToken,
        const StereoResourceLayoutMetadata&) noexcept = nullptr;
};

template <std::size_t CollectorCapacity>
constexpr bool stereoResourceOperationsComplete(
    const StereoResourceOperations<CollectorCapacity>& operations) noexcept
{
    return operations.retainContextLease
        && operations.releaseContextLease
        && operations.acquireCamera
        && operations.releaseCameraOwnership
        && operations.allocateCollectorBindingRawStorage
        && operations.freeCollectorBindingRawStorage
        && operations.invokeCullingConstructor
        && operations.destroyCullingProcessInPlace
        && operations.allocateAccumulatorRawStorage
        && operations.freeAccumulatorRawStorage
        && operations.invokeAccumulatorConstructor
        && operations.destroyAndReleaseAccumulator;
}

template <std::size_t CollectorCapacity>
class OwnedStereoResources final
{
public:
    using Binding =
        geometry::PrivateGeometryCollectorBinding<CollectorCapacity>;
    using Camera = abi::RetailNiCameraLayout;
    using Culler = abi::RetailBSCullingProcessLayout;
    using Accumulator = abi::RetailBSShaderAccumulatorLayout;
    using Operations = StereoResourceOperations<CollectorCapacity>;

    OwnedStereoResources() noexcept = default;

    OwnedStereoResources(const OwnedStereoResources&) = delete;
    OwnedStereoResources& operator=(const OwnedStereoResources&) = delete;

    OwnedStereoResources(OwnedStereoResources&& other) noexcept
    {
        take(std::move(other));
    }

    OwnedStereoResources& operator=(OwnedStereoResources&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            take(std::move(other));
        }
        return *this;
    }

    ~OwnedStereoResources() noexcept
    {
        reset();
    }

    bool valid() const noexcept
    {
        return mContextLease.valid()
            && mCamera
            && mCameraCleanupToken.valid()
            && mBindingCleanupToken.valid()
            && mBindingObjectConstructed
            && mBinding
            && mCullerConstructed
            && mCuller
            && mLeftCleanupToken.valid()
            && mLeftConstructed
            && mLeftAccumulator
            && mRightCleanupToken.valid()
            && mRightConstructed
            && mRightAccumulator;
    }

    constexpr StereoResourceViewPlan viewPlan() const noexcept
    {
        return {};
    }

    Camera* camera() const noexcept
    {
        return mCamera;
    }

    Binding* collectorBinding() const noexcept
    {
        return mBinding;
    }

    Culler* cullingProcess() const noexcept
    {
        return mCuller;
    }

    Accumulator* leftAccumulator() const noexcept
    {
        return mLeftAccumulator;
    }

    Accumulator* rightAccumulator() const noexcept
    {
        return mRightAccumulator;
    }

private:
    void reset() noexcept
    {
        if (mRightCleanupToken.valid())
        {
            if (mRightConstructed
                && mOperations.destroyAndReleaseAccumulator)
            {
                mOperations.destroyAndReleaseAccumulator(
                    mContextLease,
                    StereoResourceKind::RightAccumulator,
                    mRightAccumulator,
                    std::move(mRightCleanupToken),
                    stereoAccumulatorLayoutMetadata(
                        StereoResourceKind::RightAccumulator));
            }
            else if (mOperations.freeAccumulatorRawStorage)
            {
                mOperations.freeAccumulatorRawStorage(
                    mContextLease,
                    StereoResourceKind::RightAccumulator,
                    std::move(mRightCleanupToken),
                    stereoAccumulatorLayoutMetadata(
                        StereoResourceKind::RightAccumulator));
            }
        }

        if (mLeftCleanupToken.valid())
        {
            if (mLeftConstructed
                && mOperations.destroyAndReleaseAccumulator)
            {
                mOperations.destroyAndReleaseAccumulator(
                    mContextLease,
                    StereoResourceKind::LeftAccumulator,
                    mLeftAccumulator,
                    std::move(mLeftCleanupToken),
                    stereoAccumulatorLayoutMetadata(
                        StereoResourceKind::LeftAccumulator));
            }
            else if (mOperations.freeAccumulatorRawStorage)
            {
                mOperations.freeAccumulatorRawStorage(
                    mContextLease,
                    StereoResourceKind::LeftAccumulator,
                    std::move(mLeftCleanupToken),
                    stereoAccumulatorLayoutMetadata(
                        StereoResourceKind::LeftAccumulator));
            }
        }

        if (mBindingCleanupToken.valid())
        {
            if (mCullerConstructed && mCuller
                && mOperations.destroyCullingProcessInPlace)
            {
                mOperations.destroyCullingProcessInPlace(
                    mContextLease,
                    mCuller);
            }
            if (mBindingObjectConstructed)
                mBinding->~Binding();
            if (mOperations.freeCollectorBindingRawStorage)
            {
                mOperations.freeCollectorBindingRawStorage(
                    mContextLease,
                    std::move(mBindingCleanupToken),
                    stereoCollectorBindingLayoutMetadata<CollectorCapacity>());
            }
        }

        if (mCameraCleanupToken.valid()
            && mOperations.releaseCameraOwnership)
        {
            mOperations.releaseCameraOwnership(
                mContextLease,
                std::move(mCameraCleanupToken));
        }

        if (mContextLease.valid() && mOperations.releaseContextLease)
        {
            mOperations.releaseContextLease(std::move(mContextLease));
        }

        clearWithoutCleanup();
    }

    void clearWithoutCleanup() noexcept
    {
        mOperations = {};
        mCamera = nullptr;
        mBinding = nullptr;
        mCuller = nullptr;
        mLeftAccumulator = nullptr;
        mRightAccumulator = nullptr;
        mContextLease = {};
        mCameraCleanupToken = {};
        mBindingCleanupToken = {};
        mLeftCleanupToken = {};
        mRightCleanupToken = {};
        mBindingObjectConstructed = false;
        mCullerConstructed = false;
        mLeftConstructed = false;
        mRightConstructed = false;
    }

    void take(OwnedStereoResources&& other) noexcept
    {
        mOperations = other.mOperations;
        mCamera = other.mCamera;
        mBinding = other.mBinding;
        mCuller = other.mCuller;
        mLeftAccumulator = other.mLeftAccumulator;
        mRightAccumulator = other.mRightAccumulator;
        mContextLease = std::move(other.mContextLease);
        mCameraCleanupToken = std::move(other.mCameraCleanupToken);
        mBindingCleanupToken = std::move(other.mBindingCleanupToken);
        mLeftCleanupToken = std::move(other.mLeftCleanupToken);
        mRightCleanupToken = std::move(other.mRightCleanupToken);
        mBindingObjectConstructed = other.mBindingObjectConstructed;
        mCullerConstructed = other.mCullerConstructed;
        mLeftConstructed = other.mLeftConstructed;
        mRightConstructed = other.mRightConstructed;
        other.clearWithoutCleanup();
    }

    Operations mOperations {};
    StereoContextLeaseToken mContextLease {};
    StereoCameraCleanupToken mCameraCleanupToken {};
    StereoBindingStorageCleanupToken mBindingCleanupToken {};
    StereoAccumulatorStorageCleanupToken mLeftCleanupToken {};
    StereoAccumulatorStorageCleanupToken mRightCleanupToken {};
    Camera* mCamera = nullptr;
    Binding* mBinding = nullptr;
    Culler* mCuller = nullptr;
    Accumulator* mLeftAccumulator = nullptr;
    Accumulator* mRightAccumulator = nullptr;
    bool mBindingObjectConstructed = false;
    bool mCullerConstructed = false;
    bool mLeftConstructed = false;
    bool mRightConstructed = false;

    friend struct detail::StereoResourceBuilder<CollectorCapacity>;
};

template <std::size_t CollectorCapacity>
struct StereoResourceTransactionResult
{
    StereoResourceFailure failure = StereoResourceFailure::Unauthorized;
    OwnedStereoResources<CollectorCapacity> resources {};

    bool succeeded() const noexcept
    {
        return failure == StereoResourceFailure::None && resources.valid();
    }
};

namespace detail
{
inline bool stereoResourceAddressAligned(
    const void* resource,
    std::size_t alignment) noexcept
{
    if (!resource || alignment == 0u)
        return false;
    return reinterpret_cast<std::uintptr_t>(resource) % alignment == 0u;
}

inline bool stereoResourceRangesOverlap(
    const void* left,
    std::size_t leftByteCount,
    const void* right,
    std::size_t rightByteCount) noexcept
{
    if (!left || !right || leftByteCount == 0u || rightByteCount == 0u)
        return false;

    const std::uintptr_t leftAddress =
        reinterpret_cast<std::uintptr_t>(left);
    const std::uintptr_t rightAddress =
        reinterpret_cast<std::uintptr_t>(right);
    if (leftAddress <= rightAddress)
        return rightAddress - leftAddress < leftByteCount;
    return leftAddress - rightAddress < rightByteCount;
}

template <std::size_t CollectorCapacity>
struct StereoResourceBuilder
{
    using Operations = StereoResourceOperations<CollectorCapacity>;
    using Owned = OwnedStereoResources<CollectorCapacity>;
    using Result = StereoResourceTransactionResult<CollectorCapacity>;
    using Binding = typename Owned::Binding;
    using Accumulator = typename Owned::Accumulator;

    static_assert(
        std::is_nothrow_default_constructible_v<Binding>,
        "private collector binding construction must not introduce an "
        "unwind path outside the injected lifecycle table");

    static Result fail(
        StereoResourceFailure failure,
        Owned& owned) noexcept
    {
        owned.reset();
        Result result;
        result.failure = failure;
        return result;
    }

    static Result acquire(
        const Operations& operations,
        const StereoResourceAuthorization& authorization,
        const StereoResourceConstructionParameters& parameters) noexcept
    {
        if (!StereoResourceAuthorizationAccess::authorized(authorization))
        {
            Result result;
            result.failure = StereoResourceFailure::Unauthorized;
            return result;
        }
        if (!stereoResourceOperationsComplete(operations))
        {
            Result result;
            result.failure = StereoResourceFailure::OperationTableIncomplete;
            return result;
        }

        StereoContextLeaseToken lease =
            operations.retainContextLease(operations.bootstrapContext);
        if (!lease.valid())
        {
            Result result;
            result.failure = StereoResourceFailure::ContextLeaseRetainFailed;
            return result;
        }

        Owned owned;
        owned.mOperations = operations;
        // The owner retains only the lease-backed operations. It never stores
        // or reuses the borrowed bootstrap context.
        owned.mOperations.bootstrapContext = nullptr;
        owned.mOperations.retainContextLease = nullptr;
        owned.mContextLease = std::move(lease);

        StereoCameraAcquisition camera = operations.acquireCamera(
            owned.mContextLease,
            StereoCameraLayoutMetadata);
        owned.mCamera = camera.camera;
        owned.mCameraCleanupToken = std::move(camera.cleanupToken);
        if (!camera.factoryInvocationReturned)
            return fail(StereoResourceFailure::CameraFactoryNotInvoked, owned);
        if (!camera.camera)
            return fail(StereoResourceFailure::CameraWasNull, owned);
        if (!owned.mCameraCleanupToken.valid())
        {
            return fail(
                StereoResourceFailure::CameraCleanupTokenMissing,
                owned);
        }
        if (!stereoResourceAddressAligned(
                owned.mCamera,
                StereoCameraLayoutMetadata.allocationAlignment))
        {
            return fail(
                StereoResourceFailure::ResourceAlignmentInvalid,
                owned);
        }

        typename Operations::BindingStorageAcquisition binding =
            operations.allocateCollectorBindingRawStorage(
            owned.mContextLease,
            stereoCollectorBindingLayoutMetadata<CollectorCapacity>());
        // Adopt the cleanup capability before inspecting any untrusted
        // candidate address or status. Cleanup never depends on that address.
        owned.mBinding = binding.storage;
        owned.mBindingCleanupToken = std::move(binding.cleanupToken);
        if (!binding.invocationReturned || !owned.mBinding)
        {
            return fail(
                StereoResourceFailure::CollectorBindingAllocationFailed,
                owned);
        }
        if (!owned.mBindingCleanupToken.valid())
        {
            return fail(
                StereoResourceFailure::CollectorBindingCleanupTokenMissing,
                owned);
        }
        constexpr StereoResourceLayoutMetadata BindingMetadata =
            stereoCollectorBindingLayoutMetadata<CollectorCapacity>();
        if (stereoResourceRangesOverlap(
                owned.mBinding,
                BindingMetadata.allocationByteCount,
                owned.mCamera,
                StereoCameraLayoutMetadata.allocationByteCount))
        {
            return fail(StereoResourceFailure::ResourceAlias, owned);
        }

        if (!stereoResourceAddressAligned(
                owned.mBinding,
                BindingMetadata.allocationAlignment))
        {
            return fail(
                StereoResourceFailure::ResourceAlignmentInvalid,
                owned);
        }
        owned.mBinding = ::new (static_cast<void*>(owned.mBinding)) Binding {};
        owned.mBindingObjectConstructed = true;
        owned.mCuller = owned.mBinding->cullingProcess();

        const StereoConstructorInvocationResult<typename Owned::Culler> culler =
            operations.invokeCullingConstructor(
                owned.mContextLease,
                owned.mCuller,
                parameters.cullingBaseConstructorArgument);
        if (!culler.invocationReturned)
        {
            return fail(
                StereoResourceFailure::CullingConstructorNotInvoked,
                owned);
        }
        // A returned constructor invocation starts the intended lifetime even
        // if its ABI return register is corrupt. Never adopt the foreign
        // returned pointer.
        owned.mCullerConstructed = true;
        if (culler.returnedObject != owned.mCuller)
        {
            return fail(
                StereoResourceFailure::CullingConstructorDidNotReturnThis,
                owned);
        }
        if (owned.mBinding->failure()
                != geometry::GeometryCollectorFailure::None
            || owned.mBinding->phase()
                != geometry::GeometryCollectorPhase::Inactive)
        {
            return fail(
                StereoResourceFailure::CollectorBindingIntegrityFailed,
                owned);
        }

        typename Operations::AccumulatorStorageAcquisition leftStorage =
            operations.allocateAccumulatorRawStorage(
                owned.mContextLease,
                StereoResourceKind::LeftAccumulator,
                stereoAccumulatorLayoutMetadata(
                    StereoResourceKind::LeftAccumulator));
        owned.mLeftAccumulator = leftStorage.storage;
        owned.mLeftCleanupToken = std::move(leftStorage.cleanupToken);
        if (!leftStorage.invocationReturned || !owned.mLeftAccumulator)
        {
            return fail(
                StereoResourceFailure::LeftAccumulatorAllocationFailed,
                owned);
        }
        if (!owned.mLeftCleanupToken.valid())
        {
            return fail(
                StereoResourceFailure::LeftAccumulatorCleanupTokenMissing,
                owned);
        }
        constexpr StereoResourceLayoutMetadata LeftMetadata =
            stereoAccumulatorLayoutMetadata(
                StereoResourceKind::LeftAccumulator);
        if (stereoResourceRangesOverlap(
                owned.mLeftAccumulator,
                LeftMetadata.allocationByteCount,
                owned.mCamera,
                StereoCameraLayoutMetadata.allocationByteCount)
            || stereoResourceRangesOverlap(
                owned.mLeftAccumulator,
                LeftMetadata.allocationByteCount,
                owned.mBinding,
                BindingMetadata.allocationByteCount))
        {
            return fail(StereoResourceFailure::ResourceAlias, owned);
        }
        if (!stereoResourceAddressAligned(
                owned.mLeftAccumulator,
                LeftMetadata.allocationAlignment))
        {
            return fail(
                StereoResourceFailure::ResourceAlignmentInvalid,
                owned);
        }

        const StereoConstructorInvocationResult<Accumulator> left =
            operations.invokeAccumulatorConstructor(
                owned.mContextLease,
                StereoResourceKind::LeftAccumulator,
                owned.mLeftAccumulator,
                parameters);
        if (!left.invocationReturned)
        {
            return fail(
                StereoResourceFailure::LeftAccumulatorConstructorNotInvoked,
                owned);
        }
        owned.mLeftConstructed = true;
        if (left.returnedObject != owned.mLeftAccumulator)
        {
            return fail(
                StereoResourceFailure::LeftAccumulatorConstructorDidNotReturnThis,
                owned);
        }

        typename Operations::AccumulatorStorageAcquisition rightStorage =
            operations.allocateAccumulatorRawStorage(
                owned.mContextLease,
                StereoResourceKind::RightAccumulator,
                stereoAccumulatorLayoutMetadata(
                    StereoResourceKind::RightAccumulator));
        // An exact duplicate capability is not a second ownership transfer.
        // Reject it before adoption so teardown consumes the first-issued
        // capability exactly once, regardless of whether the new candidate
        // address aliases that allocation or points somewhere distinct.
        if (rightStorage.cleanupToken.identifiesSameCapabilityAs(
                owned.mLeftCleanupToken))
        {
            return fail(
                StereoResourceFailure::AccumulatorCleanupCapabilityAlias,
                owned);
        }
        owned.mRightAccumulator = rightStorage.storage;
        owned.mRightCleanupToken = std::move(rightStorage.cleanupToken);
        if (!rightStorage.invocationReturned || !owned.mRightAccumulator)
        {
            return fail(
                StereoResourceFailure::RightAccumulatorAllocationFailed,
                owned);
        }
        if (!owned.mRightCleanupToken.valid())
        {
            return fail(
                StereoResourceFailure::RightAccumulatorCleanupTokenMissing,
                owned);
        }
        constexpr StereoResourceLayoutMetadata RightMetadata =
            stereoAccumulatorLayoutMetadata(
                StereoResourceKind::RightAccumulator);
        if (stereoResourceRangesOverlap(
                owned.mRightAccumulator,
                RightMetadata.allocationByteCount,
                owned.mCamera,
                StereoCameraLayoutMetadata.allocationByteCount)
            || stereoResourceRangesOverlap(
                owned.mRightAccumulator,
                RightMetadata.allocationByteCount,
                owned.mBinding,
                BindingMetadata.allocationByteCount)
            || stereoResourceRangesOverlap(
                owned.mRightAccumulator,
                RightMetadata.allocationByteCount,
                owned.mLeftAccumulator,
                LeftMetadata.allocationByteCount))
        {
            return fail(StereoResourceFailure::ResourceAlias, owned);
        }
        if (!stereoResourceAddressAligned(
                owned.mRightAccumulator,
                RightMetadata.allocationAlignment))
        {
            return fail(
                StereoResourceFailure::ResourceAlignmentInvalid,
                owned);
        }

        const StereoConstructorInvocationResult<Accumulator> right =
            operations.invokeAccumulatorConstructor(
                owned.mContextLease,
                StereoResourceKind::RightAccumulator,
                owned.mRightAccumulator,
                parameters);
        if (!right.invocationReturned)
        {
            return fail(
                StereoResourceFailure::RightAccumulatorConstructorNotInvoked,
                owned);
        }
        owned.mRightConstructed = true;
        if (right.returnedObject != owned.mRightAccumulator)
        {
            return fail(
                StereoResourceFailure::RightAccumulatorConstructorDidNotReturnThis,
                owned);
        }

        Result result;
        result.failure = StereoResourceFailure::None;
        result.resources = std::move(owned);
        return result;
    }
};
}

template <std::size_t CollectorCapacity>
StereoResourceTransactionResult<CollectorCapacity> acquireCenterStereoResources(
    const StereoResourceOperations<CollectorCapacity>& operations,
    const StereoResourceAuthorization& authorization,
    const StereoResourceConstructionParameters& parameters) noexcept
{
    return detail::StereoResourceBuilder<CollectorCapacity>::acquire(
        operations,
        authorization,
        parameters);
}

static_assert(
    StereoResourceProductionAuthorizationAvailable
    == RetailRuntimeProductionAuthorizationAvailable);
static_assert(std::is_standard_layout_v<StereoResourceLayoutMetadata>);
static_assert(std::is_trivially_copyable_v<StereoResourceLayoutMetadata>);
static_assert(std::is_standard_layout_v<StereoResourceConstructionParameters>);
static_assert(
    std::is_trivially_copyable_v<StereoResourceConstructionParameters>);
static_assert(!std::is_copy_constructible_v<StereoContextLeaseToken>);
static_assert(std::is_nothrow_move_constructible_v<StereoContextLeaseToken>);
static_assert(!std::is_copy_constructible_v<StereoCameraCleanupToken>);
static_assert(std::is_nothrow_move_constructible_v<StereoCameraCleanupToken>);
}
