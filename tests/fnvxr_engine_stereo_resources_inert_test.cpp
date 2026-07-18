#if defined(FNVXR_STEREO_RESOURCE_LIFECYCLE_TEST_AUTHORITY)
#error The production-inert test must not compile with the synthetic authority.
#endif

#include "fnvxr_engine_stereo_resources.h"

#include <cstdlib>
#include <type_traits>

namespace
{
using namespace fnvxr::engine;

constexpr std::size_t CollectorCapacity = 1u;
using Binding = geometry::PrivateGeometryCollectorBinding<CollectorCapacity>;
using Camera = abi::RetailNiCameraLayout;
using Culler = abi::RetailBSCullingProcessLayout;
using Accumulator = abi::RetailBSShaderAccumulatorLayout;
using ContextLeaseToken = StereoContextLeaseToken;
using CameraCleanupToken = StereoCameraCleanupToken;
using BindingCleanupToken = StereoBindingStorageCleanupToken;
using AccumulatorCleanupToken = StereoAccumulatorStorageCleanupToken;
using Operations = StereoResourceOperations<CollectorCapacity>;
using BindingStorageAcquisition = Operations::BindingStorageAcquisition;
using AccumulatorStorageAcquisition = Operations::AccumulatorStorageAcquisition;

struct MustRemainUncalled
{
    int callCount = 0;

    static MustRemainUncalled& self(void* context) noexcept
    {
        return *static_cast<MustRemainUncalled*>(context);
    }

    static StereoContextLeaseToken retainContextLease(void* context) noexcept
    {
        ++self(context).callCount;
        return {};
    }

    static void releaseContextLease(StereoContextLeaseToken) noexcept
    {
    }

    static StereoCameraAcquisition acquireCamera(
        const ContextLeaseToken&,
        const StereoResourceLayoutMetadata&) noexcept
    {
        return {};
    }

    static void releaseCameraOwnership(
        const ContextLeaseToken&,
        CameraCleanupToken) noexcept
    {
    }

    static BindingStorageAcquisition allocateBindingRawStorage(
        const ContextLeaseToken&,
        const StereoResourceLayoutMetadata&) noexcept
    {
        return {};
    }

    static void freeBindingRawStorage(
        const ContextLeaseToken&,
        BindingCleanupToken,
        const StereoResourceLayoutMetadata&) noexcept
    {
    }

    static StereoConstructorInvocationResult<Culler> invokeCullerConstructor(
        const ContextLeaseToken&,
        Culler*,
        std::uint32_t) noexcept
    {
        return {};
    }

    static void destroyCullerInPlace(
        const ContextLeaseToken&,
        Culler*) noexcept
    {
    }

    static AccumulatorStorageAcquisition allocateAccumulatorRawStorage(
        const ContextLeaseToken&,
        StereoResourceKind,
        const StereoResourceLayoutMetadata&) noexcept
    {
        return {};
    }

    static void freeAccumulatorRawStorage(
        const ContextLeaseToken&,
        StereoResourceKind,
        AccumulatorCleanupToken,
        const StereoResourceLayoutMetadata&) noexcept
    {
    }

    static StereoConstructorInvocationResult<Accumulator>
    invokeAccumulatorConstructor(
        const ContextLeaseToken&,
        StereoResourceKind,
        Accumulator*,
        const StereoResourceConstructionParameters&) noexcept
    {
        return {};
    }

    static void destroyAndReleaseAccumulator(
        const ContextLeaseToken&,
        StereoResourceKind,
        Accumulator*,
        AccumulatorCleanupToken,
        const StereoResourceLayoutMetadata&) noexcept
    {
    }
};
}

int main()
{
    using namespace fnvxr::engine;

    static_assert(!StereoResourceProductionAuthorizationAvailable);
    static_assert(std::is_default_constructible_v<StereoResourceAuthorization>);
    static_assert(!std::is_constructible_v<StereoResourceAuthorization, bool>);
    static_assert(!std::is_copy_constructible_v<ContextLeaseToken>);
    static_assert(!std::is_copy_constructible_v<CameraCleanupToken>);
    static_assert(!std::is_copy_constructible_v<BindingCleanupToken>);
    static_assert(!std::is_copy_constructible_v<AccumulatorCleanupToken>);

    MustRemainUncalled backend;
    const Operations operations {
        &backend,
        &MustRemainUncalled::retainContextLease,
        &MustRemainUncalled::releaseContextLease,
        &MustRemainUncalled::acquireCamera,
        &MustRemainUncalled::releaseCameraOwnership,
        &MustRemainUncalled::allocateBindingRawStorage,
        &MustRemainUncalled::freeBindingRawStorage,
        &MustRemainUncalled::invokeCullerConstructor,
        &MustRemainUncalled::destroyCullerInPlace,
        &MustRemainUncalled::allocateAccumulatorRawStorage,
        &MustRemainUncalled::freeAccumulatorRawStorage,
        &MustRemainUncalled::invokeAccumulatorConstructor,
        &MustRemainUncalled::destroyAndReleaseAccumulator,
    };
    const auto result = acquireCenterStereoResources<CollectorCapacity>(
        operations,
        StereoResourceAuthorization {},
        {});

    return stereoResourceOperationsComplete(operations)
            && result.failure == StereoResourceFailure::Unauthorized
            && !result.succeeded()
            && !result.resources.valid()
            && backend.callCount == 0
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}
