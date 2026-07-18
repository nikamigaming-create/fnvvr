#include "fnvxr_engine_stereo_resources.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace fnvxr::engine
{
struct StereoResourceLifecycleTestAuthority final
{
public:
    static StereoResourceAuthorization issue() noexcept
    {
        return StereoResourceAuthorization(&Evidence, &validate);
    }

private:
    static bool validate(const void* evidence) noexcept
    {
        return evidence == &Evidence;
    }

    inline static constexpr std::uint64_t Evidence =
        0x53544552454F5445ull;
};
}

namespace
{
using namespace fnvxr::engine;

constexpr std::size_t CollectorCapacity = 16u;
using Binding = geometry::PrivateGeometryCollectorBinding<CollectorCapacity>;
using Operations = StereoResourceOperations<CollectorCapacity>;
using OwnedResources = OwnedStereoResources<CollectorCapacity>;
using Camera = abi::RetailNiCameraLayout;
using Culler = abi::RetailBSCullingProcessLayout;
using Accumulator = abi::RetailBSShaderAccumulatorLayout;
using ContextLeaseToken = StereoContextLeaseToken;
using CameraCleanupToken = StereoCameraCleanupToken;
using BindingCleanupToken = StereoBindingStorageCleanupToken;
using AccumulatorCleanupToken = StereoAccumulatorStorageCleanupToken;
using BindingStorageAcquisition = Operations::BindingStorageAcquisition;
using AccumulatorStorageAcquisition = Operations::AccumulatorStorageAcquisition;

static_assert(!std::is_copy_constructible_v<ContextLeaseToken>);
static_assert(std::is_nothrow_move_constructible_v<ContextLeaseToken>);
static_assert(!std::is_copy_constructible_v<CameraCleanupToken>);
static_assert(std::is_nothrow_move_constructible_v<CameraCleanupToken>);
static_assert(!std::is_copy_constructible_v<BindingCleanupToken>);
static_assert(!std::is_copy_constructible_v<AccumulatorCleanupToken>);
static_assert(!std::is_copy_constructible_v<BindingStorageAcquisition>);
static_assert(
    std::is_nothrow_move_constructible_v<BindingStorageAcquisition>);
static_assert(!std::is_copy_constructible_v<AccumulatorStorageAcquisition>);
static_assert(
    std::is_nothrow_move_constructible_v<AccumulatorStorageAcquisition>);

enum class Event : int
{
    RetainContext,
    AcquireCamera,
    ReleaseCamera,
    AllocateBinding,
    ConstructCuller,
    DestroyCuller,
    FreeBinding,
    AllocateLeft,
    ConstructLeft,
    DestroyLeft,
    FreeLeft,
    AllocateRight,
    ConstructRight,
    DestroyRight,
    FreeRight,
    ReleaseContext,
};

enum class FailureMode : int
{
    None,
    ContextLeaseRetainFailure,
    CameraFactoryNotInvokedWithCleanup,
    CameraNull,
    CameraCleanupTokenMissing,
    CameraMisaligned,
    BindingAllocation,
    BindingCleanupTokenMissing,
    BindingAliasesCamera,
    BindingOverlapsCameraTail,
    BindingMisaligned,
    CullerConstructorNotInvoked,
    CullerReturnedNull,
    CullerReturnedDifferentObject,
    CullerCorruptsBindingCanary,
    LeftAllocation,
    LeftCleanupTokenMissing,
    LeftAliasesCamera,
    LeftAliasesBinding,
    LeftMisaligned,
    LeftConstructorNotInvoked,
    LeftConstructorReturnedNull,
    LeftConstructorReturnedDifferentObject,
    RightAllocation,
    RightCleanupTokenMissing,
    RightDuplicatesLeftTokenDistinctStorage,
    RightDuplicatesLeftTokenAndAliasesLeft,
    RightAliasesLeft,
    RightAliasesCamera,
    RightAliasesBinding,
    RightOverlapsLeftTail,
    RightMisaligned,
    RightConstructorNotInvoked,
    RightConstructorReturnedNull,
    RightConstructorReturnedDifferentObject,
};

enum class LeaseState : std::uint8_t
{
    Unretained,
    Retained,
    Released,
};

enum class ResourceState : std::uint8_t
{
    Absent,
    Raw,
    Constructed,
    Destroyed,
    Freed,
};

int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}

bool sameEvents(
    const std::vector<Event>& actual,
    std::initializer_list<Event> expected)
{
    return actual == std::vector<Event>(expected);
}

struct FakeBackend
{
    FailureMode failureMode = FailureMode::None;
    bool metadataExact = true;
    bool constructorArgumentsExact = true;
    bool stateMachineValid = true;
    LeaseState leaseState = LeaseState::Unretained;
    ResourceState cameraState = ResourceState::Absent;
    ResourceState bindingState = ResourceState::Absent;
    ResourceState leftState = ResourceState::Absent;
    ResourceState rightState = ResourceState::Absent;
    std::vector<Event> events;

    Camera camera {};
    Culler foreignCuller {};
    Accumulator foreignAccumulator {};
    std::array<
        unsigned char,
        sizeof(Binding) + alignof(Binding)> bindingStorage {};
    std::array<
        unsigned char,
        sizeof(Accumulator) + alignof(Accumulator)> leftAccumulatorStorage {};
    std::array<
        unsigned char,
        sizeof(Accumulator) + alignof(Accumulator)> rightAccumulatorStorage {};

    static constexpr std::uint64_t LeaseGeneration = 0x101u;
    static constexpr std::uint64_t CameraCleanupGeneration = 0x202u;
    static constexpr std::uint64_t BindingCleanupGeneration = 0x303u;
    static constexpr std::uint64_t LeftCleanupGeneration = 0x404u;
    static constexpr std::uint64_t RightCleanupGeneration = 0x505u;

    StereoResourceConstructionParameters expectedParameters {
        7u,
        11u,
        13u,
        17u,
    };

    static FakeBackend& self(const ContextLeaseToken& lease) noexcept
    {
        return *reinterpret_cast<FakeBackend*>(lease.opaqueHandle());
    }

    void checkLease(const ContextLeaseToken& lease) noexcept
    {
        if (!lease.valid()
            || lease.opaqueHandle()
                != reinterpret_cast<std::uintptr_t>(this)
            || lease.generation() != LeaseGeneration
            || leaseState != LeaseState::Retained)
        {
            stateMachineValid = false;
        }
    }

    static ContextLeaseToken retainContextLease(void* bootstrap) noexcept
    {
        FakeBackend& backend = *static_cast<FakeBackend*>(bootstrap);
        backend.events.push_back(Event::RetainContext);
        if (backend.leaseState != LeaseState::Unretained)
            backend.stateMachineValid = false;
        if (backend.failureMode == FailureMode::ContextLeaseRetainFailure)
            return {};
        backend.leaseState = LeaseState::Retained;
        return ContextLeaseToken::fromOpaque(
            reinterpret_cast<std::uintptr_t>(&backend),
            LeaseGeneration);
    }

    static void releaseContextLease(ContextLeaseToken lease) noexcept
    {
        FakeBackend& backend = self(lease);
        backend.checkLease(lease);
        if (backend.cameraState == ResourceState::Raw
            || backend.cameraState == ResourceState::Constructed
            || backend.bindingState == ResourceState::Raw
            || backend.bindingState == ResourceState::Constructed
            || backend.bindingState == ResourceState::Destroyed
            || backend.leftState == ResourceState::Raw
            || backend.leftState == ResourceState::Constructed
            || backend.leftState == ResourceState::Destroyed
            || backend.rightState == ResourceState::Raw
            || backend.rightState == ResourceState::Constructed
            || backend.rightState == ResourceState::Destroyed)
        {
            backend.stateMachineValid = false;
        }
        backend.events.push_back(Event::ReleaseContext);
        backend.leaseState = LeaseState::Released;
    }

    Camera* misalignedCamera() noexcept
    {
        return reinterpret_cast<Camera*>(
            reinterpret_cast<unsigned char*>(&camera) + 1u);
    }

    Binding* bindingAllocation() noexcept
    {
        unsigned char* bytes = bindingStorage.data();
        const std::uintptr_t address =
            reinterpret_cast<std::uintptr_t>(bytes);
        const std::size_t padding =
            (alignof(Binding) - address % alignof(Binding))
            % alignof(Binding);
        bytes += padding;
        if (failureMode == FailureMode::BindingMisaligned)
            ++bytes;
        return reinterpret_cast<Binding*>(bytes);
    }

    Accumulator* misalignedAccumulator(bool left) noexcept
    {
        Accumulator* storage = accumulatorStorage(left);
        return reinterpret_cast<Accumulator*>(
            reinterpret_cast<unsigned char*>(storage) + 1u);
    }

    Accumulator* accumulatorStorage(bool left) noexcept
    {
        auto& bytes = left
            ? leftAccumulatorStorage
            : rightAccumulatorStorage;
        unsigned char* storage = bytes.data();
        const std::uintptr_t address =
            reinterpret_cast<std::uintptr_t>(storage);
        storage += (alignof(Accumulator) - address % alignof(Accumulator))
            % alignof(Accumulator);
        return reinterpret_cast<Accumulator*>(storage);
    }

    CameraCleanupToken cameraCleanupToken() noexcept
    {
        return CameraCleanupToken::fromOpaque(
            reinterpret_cast<std::uintptr_t>(this),
            CameraCleanupGeneration);
    }

    BindingCleanupToken bindingCleanupToken() noexcept
    {
        return BindingCleanupToken::fromOpaque(
            reinterpret_cast<std::uintptr_t>(this),
            BindingCleanupGeneration);
    }

    AccumulatorCleanupToken accumulatorCleanupToken(bool left) noexcept
    {
        return AccumulatorCleanupToken::fromOpaque(
            reinterpret_cast<std::uintptr_t>(this),
            left ? LeftCleanupGeneration : RightCleanupGeneration);
    }

    bool fullyUnwound() const noexcept
    {
        const auto finalOrAbsent = [](ResourceState state) {
            return state == ResourceState::Absent
                || state == ResourceState::Freed;
        };
        return stateMachineValid
            && leaseState == LeaseState::Released
            && finalOrAbsent(cameraState)
            && finalOrAbsent(bindingState)
            && finalOrAbsent(leftState)
            && finalOrAbsent(rightState);
    }

    void checkMetadata(
        const StereoResourceLayoutMetadata& actual,
        const StereoResourceLayoutMetadata& expected) noexcept
    {
        if (actual != expected)
            metadataExact = false;
    }

    static StereoCameraAcquisition acquireCamera(
        const ContextLeaseToken& lease,
        const StereoResourceLayoutMetadata& metadata) noexcept
    {
        FakeBackend& backend = self(lease);
        backend.checkLease(lease);
        backend.events.push_back(Event::AcquireCamera);
        backend.checkMetadata(metadata, StereoCameraLayoutMetadata);
        switch (backend.failureMode)
        {
        case FailureMode::CameraFactoryNotInvokedWithCleanup:
            backend.cameraState = ResourceState::Raw;
            return { false, &backend.camera, backend.cameraCleanupToken() };
        case FailureMode::CameraNull:
            backend.cameraState = ResourceState::Constructed;
            return { true, nullptr, backend.cameraCleanupToken() };
        case FailureMode::CameraCleanupTokenMissing:
            return { true, &backend.camera, {} };
        case FailureMode::CameraMisaligned:
            backend.cameraState = ResourceState::Constructed;
            return {
                true,
                backend.misalignedCamera(),
                backend.cameraCleanupToken()
            };
        default:
            backend.cameraState = ResourceState::Constructed;
            return { true, &backend.camera, backend.cameraCleanupToken() };
        }
    }

    static void releaseCameraOwnership(
        const ContextLeaseToken& lease,
        CameraCleanupToken cleanupToken) noexcept
    {
        FakeBackend& backend = self(lease);
        backend.checkLease(lease);
        backend.events.push_back(Event::ReleaseCamera);
        if (!cleanupToken.valid()
            || cleanupToken.opaqueHandle()
                != reinterpret_cast<std::uintptr_t>(&backend)
            || cleanupToken.generation() != CameraCleanupGeneration
            || (backend.cameraState != ResourceState::Raw
                && backend.cameraState != ResourceState::Constructed))
        {
            backend.stateMachineValid = false;
        }
        backend.cameraState = ResourceState::Freed;
    }

    static BindingStorageAcquisition allocateBindingRawStorage(
        const ContextLeaseToken& lease,
        const StereoResourceLayoutMetadata& metadata) noexcept
    {
        FakeBackend& backend = self(lease);
        backend.checkLease(lease);
        backend.events.push_back(Event::AllocateBinding);
        backend.checkMetadata(
            metadata,
            stereoCollectorBindingLayoutMetadata<CollectorCapacity>());
        if (backend.failureMode == FailureMode::BindingCleanupTokenMissing)
            return { true, backend.bindingAllocation(), {} };
        if (backend.bindingState != ResourceState::Absent)
            backend.stateMachineValid = false;
        backend.bindingState = ResourceState::Raw;
        if (backend.failureMode == FailureMode::BindingAllocation)
            return { false, nullptr, backend.bindingCleanupToken() };
        if (backend.failureMode == FailureMode::BindingAliasesCamera)
        {
            return {
                true,
                reinterpret_cast<Binding*>(&backend.camera),
                backend.bindingCleanupToken()
            };
        }
        if (backend.failureMode == FailureMode::BindingOverlapsCameraTail)
        {
            return {
                true,
                reinterpret_cast<Binding*>(
                    reinterpret_cast<unsigned char*>(&backend.camera)
                    + sizeof(backend.camera) - 1u),
                backend.bindingCleanupToken()
            };
        }
        return {
            true,
            backend.bindingAllocation(),
            backend.bindingCleanupToken()
        };
    }

    static void freeBindingRawStorage(
        const ContextLeaseToken& lease,
        BindingCleanupToken cleanupToken,
        const StereoResourceLayoutMetadata& metadata) noexcept
    {
        FakeBackend& backend = self(lease);
        backend.checkLease(lease);
        backend.events.push_back(Event::FreeBinding);
        backend.checkMetadata(
            metadata,
            stereoCollectorBindingLayoutMetadata<CollectorCapacity>());
        if (!cleanupToken.valid()
            || cleanupToken.opaqueHandle()
                != reinterpret_cast<std::uintptr_t>(&backend)
            || cleanupToken.generation() != BindingCleanupGeneration
            || (backend.bindingState != ResourceState::Raw
            && backend.bindingState != ResourceState::Destroyed)
        )
        {
            backend.stateMachineValid = false;
        }
        backend.bindingState = ResourceState::Freed;
    }

    static StereoConstructorInvocationResult<Culler> invokeCullerConstructor(
        const ContextLeaseToken& lease,
        Culler* storage,
        std::uint32_t baseConstructorArgument) noexcept
    {
        FakeBackend& backend = self(lease);
        backend.checkLease(lease);
        backend.events.push_back(Event::ConstructCuller);
        if (baseConstructorArgument
            != backend.expectedParameters.cullingBaseConstructorArgument)
        {
            backend.constructorArgumentsExact = false;
        }
        if (storage != backend.bindingAllocation()->cullingProcess()
            || backend.bindingState != ResourceState::Raw)
        {
            backend.stateMachineValid = false;
        }
        if (backend.failureMode == FailureMode::CullerConstructorNotInvoked)
            return { false, &backend.foreignCuller };

        backend.bindingState = ResourceState::Constructed;
        switch (backend.failureMode)
        {
        case FailureMode::CullerReturnedNull:
            return { true, nullptr };
        case FailureMode::CullerReturnedDifferentObject:
            return { true, &backend.foreignCuller };
        case FailureMode::CullerCorruptsBindingCanary:
        {
            auto* bindingBytes = reinterpret_cast<unsigned char*>(
                backend.bindingAllocation());
            bindingBytes[Binding::CullingTailCanaryByteOffset] ^= 0x01u;
            return { true, storage };
        }
        default:
            return { true, storage };
        }
    }

    static void destroyCullerInPlace(
        const ContextLeaseToken& lease,
        Culler* culler) noexcept
    {
        FakeBackend& backend = self(lease);
        backend.checkLease(lease);
        backend.events.push_back(Event::DestroyCuller);
        if (culler != backend.bindingAllocation()->cullingProcess()
            || backend.bindingState != ResourceState::Constructed)
        {
            backend.stateMachineValid = false;
        }
        backend.bindingState = ResourceState::Destroyed;
    }

    static AccumulatorStorageAcquisition allocateAccumulatorRawStorage(
        const ContextLeaseToken& lease,
        StereoResourceKind kind,
        const StereoResourceLayoutMetadata& metadata) noexcept
    {
        FakeBackend& backend = self(lease);
        backend.checkLease(lease);
        const bool left = kind == StereoResourceKind::LeftAccumulator;
        backend.events.push_back(left ? Event::AllocateLeft : Event::AllocateRight);
        backend.checkMetadata(
            metadata,
            stereoAccumulatorLayoutMetadata(kind));

        ResourceState& state = left
            ? backend.leftState
            : backend.rightState;
        if ((left
                && backend.failureMode
                    == FailureMode::LeftCleanupTokenMissing)
            || (!left
                && backend.failureMode
                    == FailureMode::RightCleanupTokenMissing))
        {
            return { true, backend.accumulatorStorage(left), {} };
        }
        if (!left
            && (backend.failureMode
                    == FailureMode::RightDuplicatesLeftTokenDistinctStorage
                || backend.failureMode
                    == FailureMode::RightDuplicatesLeftTokenAndAliasesLeft))
        {
            const bool aliasesLeft = backend.failureMode
                == FailureMode::RightDuplicatesLeftTokenAndAliasesLeft;
            return {
                true,
                backend.accumulatorStorage(aliasesLeft),
                AccumulatorCleanupToken::fromOpaque(
                    reinterpret_cast<std::uintptr_t>(&backend),
                    LeftCleanupGeneration)
            };
        }
        if (state != ResourceState::Absent)
            backend.stateMachineValid = false;
        state = ResourceState::Raw;
        if (left && backend.failureMode == FailureMode::LeftAllocation)
        {
            return {
                false,
                nullptr,
                backend.accumulatorCleanupToken(left)
            };
        }
        if (!left && backend.failureMode == FailureMode::RightAllocation)
        {
            return {
                false,
                nullptr,
                backend.accumulatorCleanupToken(left)
            };
        }
        if (left && backend.failureMode == FailureMode::LeftAliasesCamera)
        {
            return {
                true,
                reinterpret_cast<Accumulator*>(&backend.camera),
                backend.accumulatorCleanupToken(left)
            };
        }
        if (left && backend.failureMode == FailureMode::LeftAliasesBinding)
        {
            return {
                true,
                reinterpret_cast<Accumulator*>(backend.bindingAllocation()),
                backend.accumulatorCleanupToken(left)
            };
        }
        if (!left && backend.failureMode == FailureMode::RightAliasesLeft)
        {
            return {
                true,
                backend.accumulatorStorage(true),
                backend.accumulatorCleanupToken(left)
            };
        }
        if (!left && backend.failureMode == FailureMode::RightAliasesCamera)
        {
            return {
                true,
                reinterpret_cast<Accumulator*>(&backend.camera),
                backend.accumulatorCleanupToken(left)
            };
        }
        if (!left && backend.failureMode == FailureMode::RightAliasesBinding)
        {
            return {
                true,
                reinterpret_cast<Accumulator*>(backend.bindingAllocation()),
                backend.accumulatorCleanupToken(left)
            };
        }
        if (!left && backend.failureMode == FailureMode::RightOverlapsLeftTail)
        {
            return {
                true,
                reinterpret_cast<Accumulator*>(
                    reinterpret_cast<unsigned char*>(
                        backend.accumulatorStorage(true))
                    + sizeof(Accumulator) - 1u),
                backend.accumulatorCleanupToken(left)
            };
        }
        if (left && backend.failureMode == FailureMode::LeftMisaligned)
        {
            return {
                true,
                backend.misalignedAccumulator(true),
                backend.accumulatorCleanupToken(left)
            };
        }
        if (!left && backend.failureMode == FailureMode::RightMisaligned)
        {
            return {
                true,
                backend.misalignedAccumulator(false),
                backend.accumulatorCleanupToken(left)
            };
        }
        return {
            true,
            backend.accumulatorStorage(left),
            backend.accumulatorCleanupToken(left)
        };
    }

    static void freeAccumulatorRawStorage(
        const ContextLeaseToken& lease,
        StereoResourceKind kind,
        AccumulatorCleanupToken cleanupToken,
        const StereoResourceLayoutMetadata& metadata) noexcept
    {
        FakeBackend& backend = self(lease);
        backend.checkLease(lease);
        const bool left = kind == StereoResourceKind::LeftAccumulator;
        backend.events.push_back(left ? Event::FreeLeft : Event::FreeRight);
        backend.checkMetadata(
            metadata,
            stereoAccumulatorLayoutMetadata(kind));
        ResourceState& state = left
            ? backend.leftState
            : backend.rightState;
        if (!cleanupToken.valid()
            || cleanupToken.opaqueHandle()
                != reinterpret_cast<std::uintptr_t>(&backend)
            || cleanupToken.generation()
                != (left ? LeftCleanupGeneration : RightCleanupGeneration)
            || state != ResourceState::Raw)
        {
            backend.stateMachineValid = false;
        }
        state = ResourceState::Freed;
    }

    static StereoConstructorInvocationResult<Accumulator>
    invokeAccumulatorConstructor(
        const ContextLeaseToken& lease,
        StereoResourceKind kind,
        Accumulator* storage,
        const StereoResourceConstructionParameters& parameters) noexcept
    {
        FakeBackend& backend = self(lease);
        backend.checkLease(lease);
        const bool left = kind == StereoResourceKind::LeftAccumulator;
        backend.events.push_back(left ? Event::ConstructLeft : Event::ConstructRight);
        if (parameters != backend.expectedParameters)
            backend.constructorArgumentsExact = false;

        ResourceState& state = left
            ? backend.leftState
            : backend.rightState;
        if (storage != backend.accumulatorStorage(left)
            || state != ResourceState::Raw)
        {
            backend.stateMachineValid = false;
        }
        if ((left
                && backend.failureMode
                    == FailureMode::LeftConstructorNotInvoked)
            || (!left
                && backend.failureMode
                    == FailureMode::RightConstructorNotInvoked))
        {
            return { false, &backend.foreignAccumulator };
        }

        ::new (static_cast<void*>(storage)) Accumulator {};
        state = ResourceState::Constructed;

        if (left)
        {
            switch (backend.failureMode)
            {
            case FailureMode::LeftConstructorReturnedNull:
                return { true, nullptr };
            case FailureMode::LeftConstructorReturnedDifferentObject:
                return { true, &backend.foreignAccumulator };
            default:
                return { true, storage };
            }
        }

        switch (backend.failureMode)
        {
        case FailureMode::RightConstructorReturnedNull:
            return { true, nullptr };
        case FailureMode::RightConstructorReturnedDifferentObject:
            return { true, &backend.foreignAccumulator };
        default:
            return { true, storage };
        }
    }

    static void destroyAndReleaseAccumulator(
        const ContextLeaseToken& lease,
        StereoResourceKind kind,
        Accumulator* accumulator,
        AccumulatorCleanupToken cleanupToken,
        const StereoResourceLayoutMetadata& metadata) noexcept
    {
        FakeBackend& backend = self(lease);
        backend.checkLease(lease);
        backend.checkMetadata(
            metadata,
            stereoAccumulatorLayoutMetadata(kind));
        const bool left = kind == StereoResourceKind::LeftAccumulator;
        backend.events.push_back(
            left
                ? Event::DestroyLeft
                : Event::DestroyRight);
        ResourceState& state = left
            ? backend.leftState
            : backend.rightState;
        if (!cleanupToken.valid()
            || cleanupToken.opaqueHandle()
                != reinterpret_cast<std::uintptr_t>(&backend)
            || cleanupToken.generation()
                != (left ? LeftCleanupGeneration : RightCleanupGeneration)
            || accumulator != backend.accumulatorStorage(left)
            || state != ResourceState::Constructed)
        {
            backend.stateMachineValid = false;
        }
        accumulator->~Accumulator();
        state = ResourceState::Destroyed;
        // This callback models one indivisible retail operation: after it
        // returns there is no separately owned allocation to free.
        state = ResourceState::Freed;
    }

    Operations operations() noexcept
    {
        return {
            this,
            &retainContextLease,
            &releaseContextLease,
            &acquireCamera,
            &releaseCameraOwnership,
            &allocateBindingRawStorage,
            &freeBindingRawStorage,
            &invokeCullerConstructor,
            &destroyCullerInPlace,
            &allocateAccumulatorRawStorage,
            &freeAccumulatorRawStorage,
            &invokeAccumulatorConstructor,
            &destroyAndReleaseAccumulator,
        };
    }
};

StereoResourceAuthorization authorizedForTest() noexcept
{
    return StereoResourceLifecycleTestAuthority::issue();
}

bool verifyFailureCase(
    FailureMode mode,
    StereoResourceFailure expectedFailure,
    std::initializer_list<Event> expectedEvents)
{
    FakeBackend backend;
    backend.failureMode = mode;
    const auto result = acquireCenterStereoResources<CollectorCapacity>(
        backend.operations(),
        authorizedForTest(),
        backend.expectedParameters);
    std::vector<Event> fullExpected { Event::RetainContext };
    fullExpected.insert(
        fullExpected.end(),
        expectedEvents.begin(),
        expectedEvents.end());
    fullExpected.push_back(Event::ReleaseContext);
    const bool valid = result.failure == expectedFailure
        && !result.succeeded()
        && !result.resources.valid()
        && result.resources.camera() == nullptr
        && result.resources.collectorBinding() == nullptr
        && result.resources.leftAccumulator() == nullptr
        && result.resources.rightAccumulator() == nullptr
        && backend.metadataExact
        && backend.constructorArgumentsExact
        && backend.fullyUnwound()
        && backend.events == fullExpected;
    if (!valid)
    {
        std::cerr << "failure mode " << static_cast<int>(mode)
                  << ": expected failure " << static_cast<int>(expectedFailure)
                  << ", actual " << static_cast<int>(result.failure)
                  << ", metadata " << backend.metadataExact
                  << ", arguments " << backend.constructorArgumentsExact
                  << ", state " << backend.stateMachineValid
                  << ", events";
        for (const Event event : backend.events)
            std::cerr << ' ' << static_cast<int>(event);
        std::cerr << '\n';
    }
    return valid;
}
}

int main()
{
    using namespace fnvxr::engine;

    static_assert(
        RetailWorldStereoResourceConstructionParameters
            .cullingBaseConstructorArgument == 0u);
    static_assert(
        RetailWorldStereoResourceConstructionParameters
            .accumulatorConstructorMode == 0x63u);
    static_assert(
        RetailWorldStereoResourceConstructionParameters
            .accumulatorBatchRendererCount == 1u);
    static_assert(
        RetailWorldStereoResourceConstructionParameters
            .accumulatorMaximumPassCount == 0x2F7u);

    static_assert(
        StereoResourceProductionAuthorizationAvailable
        == RetailRuntimeProductionAuthorizationAvailable);
    static_assert(std::is_default_constructible_v<StereoResourceAuthorization>);
    static_assert(!std::is_constructible_v<StereoResourceAuthorization, bool>);
    static_assert(!std::is_copy_constructible_v<OwnedResources>);
    static_assert(!std::is_copy_assignable_v<OwnedResources>);
    static_assert(std::is_nothrow_move_constructible_v<OwnedResources>);
    static_assert(std::is_nothrow_move_assignable_v<OwnedResources>);

    static_assert(StereoCameraLayoutMetadata.kind == StereoResourceKind::Camera);
    static_assert(StereoCameraLayoutMetadata.allocationByteCount == 0x114u);
    static_assert(
        StereoCameraLayoutMetadata.allocationByteCount == sizeof(Camera));
    static_assert(
        StereoCameraLayoutMetadata.engineObjectByteCount == sizeof(Camera));
    constexpr auto bindingMetadata =
        stereoCollectorBindingLayoutMetadata<CollectorCapacity>();
    static_assert(
        bindingMetadata.kind == StereoResourceKind::CollectorBinding);
    static_assert(bindingMetadata.allocationByteCount == sizeof(Binding));
    static_assert(bindingMetadata.allocationAlignment == alignof(Binding));
    static_assert(
        bindingMetadata.engineObjectByteOffset
        == Binding::CullingProcessByteOffset);
    static_assert(bindingMetadata.engineObjectByteCount == 0xC8u);
    static_assert(
        bindingMetadata.engineObjectByteCount == sizeof(Culler));
    static_assert(
        StereoAccumulatorLayoutByteCount == sizeof(Accumulator));
    static_assert(StereoAccumulatorLayoutByteCount == 0x280u);

    {
        alignas(16) std::array<unsigned char, 256u> ranges {};
        constexpr std::array<std::size_t, 4u> byteCounts {
            17u,
            23u,
            31u,
            47u,
        };
        for (std::size_t left = 0u; left < byteCounts.size(); ++left)
        {
            for (std::size_t right = left + 1u;
                 right < byteCounts.size();
                 ++right)
            {
                unsigned char* const base = ranges.data() + 64u;
                if (!detail::stereoResourceRangesOverlap(
                        base,
                        byteCounts[left],
                        base,
                        byteCounts[right])
                    || !detail::stereoResourceRangesOverlap(
                        base,
                        byteCounts[left],
                        base + byteCounts[left] - 1u,
                        byteCounts[right])
                    || detail::stereoResourceRangesOverlap(
                        base,
                        byteCounts[left],
                        base + byteCounts[left],
                        byteCounts[right])
                    || detail::stereoResourceRangesOverlap(
                        base + byteCounts[right],
                        byteCounts[left],
                        base,
                        byteCounts[right]))
                {
                    return fail("pairwise overlap or adjacency arithmetic regressed");
                }
            }
        }
    }

    {
        FakeBackend backend;
        const auto result = acquireCenterStereoResources<CollectorCapacity>(
            backend.operations(),
            StereoResourceAuthorization {},
            backend.expectedParameters);
        if (result.failure != StereoResourceFailure::Unauthorized
            || result.succeeded() || !backend.events.empty())
        {
            return fail("default authorization did not fail closed before acquisition");
        }
    }

    {
        FakeBackend backend;
        const auto rejected = [&](const Operations& operations) {
            const auto result = acquireCenterStereoResources<CollectorCapacity>(
                operations,
                authorizedForTest(),
                backend.expectedParameters);
            return result.failure
                    == StereoResourceFailure::OperationTableIncomplete
                && !result.succeeded()
                && !result.resources.valid()
                && backend.events.empty();
        };

        Operations operations = backend.operations();
        operations.retainContextLease = nullptr;
        if (!rejected(operations))
            return fail("missing context retain operation was accepted");
        operations = backend.operations();
        operations.releaseContextLease = nullptr;
        if (!rejected(operations))
            return fail("missing context release operation was accepted");
        operations = backend.operations();
        operations.acquireCamera = nullptr;
        if (!rejected(operations))
            return fail("missing camera acquisition operation was accepted");
        operations = backend.operations();
        operations.releaseCameraOwnership = nullptr;
        if (!rejected(operations))
            return fail("missing camera release operation was accepted");
        operations = backend.operations();
        operations.allocateCollectorBindingRawStorage = nullptr;
        if (!rejected(operations))
            return fail("missing binding allocation operation was accepted");
        operations = backend.operations();
        operations.freeCollectorBindingRawStorage = nullptr;
        if (!rejected(operations))
            return fail("missing binding free operation was accepted");
        operations = backend.operations();
        operations.invokeCullingConstructor = nullptr;
        if (!rejected(operations))
            return fail("missing culler constructor operation was accepted");
        operations = backend.operations();
        operations.destroyCullingProcessInPlace = nullptr;
        if (!rejected(operations))
            return fail("missing culler destructor operation was accepted");
        operations = backend.operations();
        operations.allocateAccumulatorRawStorage = nullptr;
        if (!rejected(operations))
            return fail("missing accumulator allocation operation was accepted");
        operations = backend.operations();
        operations.freeAccumulatorRawStorage = nullptr;
        if (!rejected(operations))
            return fail("missing accumulator free operation was accepted");
        operations = backend.operations();
        operations.invokeAccumulatorConstructor = nullptr;
        if (!rejected(operations))
            return fail("missing accumulator constructor operation was accepted");
        operations = backend.operations();
        operations.destroyAndReleaseAccumulator = nullptr;
        if (!rejected(operations))
        {
            return fail("missing accumulator destructor operation was accepted");
        }
    }

    {
        FakeBackend backend;
        backend.failureMode = FailureMode::ContextLeaseRetainFailure;
        const auto result = acquireCenterStereoResources<CollectorCapacity>(
            backend.operations(),
            authorizedForTest(),
            backend.expectedParameters);
        if (result.failure != StereoResourceFailure::ContextLeaseRetainFailed
            || result.succeeded()
            || result.resources.valid()
            || backend.leaseState != LeaseState::Unretained
            || !backend.stateMachineValid
            || !sameEvents(backend.events, { Event::RetainContext }))
        {
            return fail("failed context retain did not stop before acquisition");
        }
    }

    if (!verifyFailureCase(
            FailureMode::CameraFactoryNotInvokedWithCleanup,
            StereoResourceFailure::CameraFactoryNotInvoked,
            { Event::AcquireCamera, Event::ReleaseCamera }))
    {
        return fail("owned camera was not released after reported acquisition failure");
    }
    if (!verifyFailureCase(
            FailureMode::CameraNull,
            StereoResourceFailure::CameraWasNull,
            { Event::AcquireCamera, Event::ReleaseCamera }))
    {
        return fail("null camera failure was not fail closed");
    }
    if (!verifyFailureCase(
            FailureMode::CameraCleanupTokenMissing,
            StereoResourceFailure::CameraCleanupTokenMissing,
            { Event::AcquireCamera }))
    {
        return fail("borrowed camera was accepted or released as owned");
    }
    if (!verifyFailureCase(
            FailureMode::CameraMisaligned,
            StereoResourceFailure::ResourceAlignmentInvalid,
            { Event::AcquireCamera, Event::ReleaseCamera }))
    {
        return fail("misaligned owned camera was accepted or not released");
    }
    if (!verifyFailureCase(
            FailureMode::BindingAllocation,
            StereoResourceFailure::CollectorBindingAllocationFailed,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("binding allocation failure did not release its cleanup token");
    }
    if (!verifyFailureCase(
            FailureMode::BindingCleanupTokenMissing,
            StereoResourceFailure::CollectorBindingCleanupTokenMissing,
            { Event::AcquireCamera, Event::AllocateBinding, Event::ReleaseCamera }))
    {
        return fail("binding pointer without a cleanup token was accepted or freed");
    }
    if (!verifyFailureCase(
            FailureMode::BindingAliasesCamera,
            StereoResourceFailure::ResourceAlias,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("binding/camera alias was not rejected without double release");
    }
    if (!verifyFailureCase(
            FailureMode::BindingOverlapsCameraTail,
            StereoResourceFailure::ResourceAlias,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("partial binding/camera overlap was not rejected as an alias");
    }
    if (!verifyFailureCase(
            FailureMode::BindingMisaligned,
            StereoResourceFailure::ResourceAlignmentInvalid,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("misaligned binding was constructed or not freed");
    }
    if (!verifyFailureCase(
            FailureMode::CullerConstructorNotInvoked,
            StereoResourceFailure::CullingConstructorNotInvoked,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::ConstructCuller,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("reported culler construction failure did not unwind raw storage");
    }
    if (!verifyFailureCase(
            FailureMode::CullerReturnedNull,
            StereoResourceFailure::CullingConstructorDidNotReturnThis,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::ConstructCuller,
                Event::DestroyCuller,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("null culler constructor return was accepted");
    }
    if (!verifyFailureCase(
            FailureMode::CullerReturnedDifferentObject,
            StereoResourceFailure::CullingConstructorDidNotReturnThis,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::ConstructCuller,
                Event::DestroyCuller,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("foreign culler constructor return was accepted or destroyed");
    }
    if (!verifyFailureCase(
            FailureMode::CullerCorruptsBindingCanary,
            StereoResourceFailure::CollectorBindingIntegrityFailed,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::ConstructCuller,
                Event::DestroyCuller,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("culler constructor overrun did not invalidate the transaction");
    }
    if (!verifyFailureCase(
            FailureMode::LeftAllocation,
            StereoResourceFailure::LeftAccumulatorAllocationFailed,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::ConstructCuller,
                Event::AllocateLeft,
                Event::FreeLeft,
                Event::DestroyCuller,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("left allocation failure did not unwind in reverse order");
    }
    if (!verifyFailureCase(
            FailureMode::LeftCleanupTokenMissing,
            StereoResourceFailure::LeftAccumulatorCleanupTokenMissing,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::ConstructCuller,
                Event::AllocateLeft,
                Event::DestroyCuller,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("left pointer without a cleanup token was accepted or freed");
    }
    if (!verifyFailureCase(
            FailureMode::LeftAliasesCamera,
            StereoResourceFailure::ResourceAlias,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::ConstructCuller,
                Event::AllocateLeft,
                Event::FreeLeft,
                Event::DestroyCuller,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("left/camera alias was not rejected without double release");
    }
    if (!verifyFailureCase(
            FailureMode::LeftAliasesBinding,
            StereoResourceFailure::ResourceAlias,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::ConstructCuller,
                Event::AllocateLeft,
                Event::FreeLeft,
                Event::DestroyCuller,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("left/binding alias was not rejected without double release");
    }
    if (!verifyFailureCase(
            FailureMode::LeftMisaligned,
            StereoResourceFailure::ResourceAlignmentInvalid,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::ConstructCuller,
                Event::AllocateLeft,
                Event::FreeLeft,
                Event::DestroyCuller,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("misaligned left accumulator was constructed or not unwound");
    }
    if (!verifyFailureCase(
            FailureMode::LeftConstructorNotInvoked,
            StereoResourceFailure::LeftAccumulatorConstructorNotInvoked,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::ConstructCuller,
                Event::AllocateLeft,
                Event::ConstructLeft,
                Event::FreeLeft,
                Event::DestroyCuller,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("reported left constructor failure did not free raw storage");
    }
    if (!verifyFailureCase(
            FailureMode::LeftConstructorReturnedNull,
            StereoResourceFailure::LeftAccumulatorConstructorDidNotReturnThis,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::ConstructCuller,
                Event::AllocateLeft,
                Event::ConstructLeft,
                Event::DestroyLeft,
                Event::DestroyCuller,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("null left constructor return was accepted");
    }
    if (!verifyFailureCase(
            FailureMode::LeftConstructorReturnedDifferentObject,
            StereoResourceFailure::LeftAccumulatorConstructorDidNotReturnThis,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::ConstructCuller,
                Event::AllocateLeft,
                Event::ConstructLeft,
                Event::DestroyLeft,
                Event::DestroyCuller,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("foreign left constructor return was accepted or destroyed");
    }
    if (!verifyFailureCase(
            FailureMode::RightAllocation,
            StereoResourceFailure::RightAccumulatorAllocationFailed,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::ConstructCuller,
                Event::AllocateLeft,
                Event::ConstructLeft,
                Event::AllocateRight,
                Event::FreeRight,
                Event::DestroyLeft,
                Event::DestroyCuller,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("right allocation failure did not unwind in reverse order");
    }
    if (!verifyFailureCase(
            FailureMode::RightCleanupTokenMissing,
            StereoResourceFailure::RightAccumulatorCleanupTokenMissing,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::ConstructCuller,
                Event::AllocateLeft,
                Event::ConstructLeft,
                Event::AllocateRight,
                Event::DestroyLeft,
                Event::DestroyCuller,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("right pointer without a cleanup token was accepted or freed");
    }
    if (!verifyFailureCase(
            FailureMode::RightDuplicatesLeftTokenDistinctStorage,
            StereoResourceFailure::AccumulatorCleanupCapabilityAlias,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::ConstructCuller,
                Event::AllocateLeft,
                Event::ConstructLeft,
                Event::AllocateRight,
                Event::DestroyLeft,
                Event::DestroyCuller,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("duplicate cleanup token with distinct storage was adopted twice");
    }
    if (!verifyFailureCase(
            FailureMode::RightDuplicatesLeftTokenAndAliasesLeft,
            StereoResourceFailure::AccumulatorCleanupCapabilityAlias,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::ConstructCuller,
                Event::AllocateLeft,
                Event::ConstructLeft,
                Event::AllocateRight,
                Event::DestroyLeft,
                Event::DestroyCuller,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("duplicate cleanup token with aliased storage was adopted twice");
    }
    if (!verifyFailureCase(
            FailureMode::RightAliasesLeft,
            StereoResourceFailure::ResourceAlias,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::ConstructCuller,
                Event::AllocateLeft,
                Event::ConstructLeft,
                Event::AllocateRight,
                Event::FreeRight,
                Event::DestroyLeft,
                Event::DestroyCuller,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("left/right accumulator alias was not rejected without double free");
    }
    if (!verifyFailureCase(
            FailureMode::RightAliasesCamera,
            StereoResourceFailure::ResourceAlias,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::ConstructCuller,
                Event::AllocateLeft,
                Event::ConstructLeft,
                Event::AllocateRight,
                Event::FreeRight,
                Event::DestroyLeft,
                Event::DestroyCuller,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("right/camera alias was not rejected without double release");
    }
    if (!verifyFailureCase(
            FailureMode::RightAliasesBinding,
            StereoResourceFailure::ResourceAlias,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::ConstructCuller,
                Event::AllocateLeft,
                Event::ConstructLeft,
                Event::AllocateRight,
                Event::FreeRight,
                Event::DestroyLeft,
                Event::DestroyCuller,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("right/binding alias was not rejected without double release");
    }
    if (!verifyFailureCase(
            FailureMode::RightOverlapsLeftTail,
            StereoResourceFailure::ResourceAlias,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::ConstructCuller,
                Event::AllocateLeft,
                Event::ConstructLeft,
                Event::AllocateRight,
                Event::FreeRight,
                Event::DestroyLeft,
                Event::DestroyCuller,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("partial right/left overlap was not rejected as an alias");
    }
    if (!verifyFailureCase(
            FailureMode::RightMisaligned,
            StereoResourceFailure::ResourceAlignmentInvalid,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::ConstructCuller,
                Event::AllocateLeft,
                Event::ConstructLeft,
                Event::AllocateRight,
                Event::FreeRight,
                Event::DestroyLeft,
                Event::DestroyCuller,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("misaligned right accumulator was constructed or not unwound");
    }
    if (!verifyFailureCase(
            FailureMode::RightConstructorNotInvoked,
            StereoResourceFailure::RightAccumulatorConstructorNotInvoked,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::ConstructCuller,
                Event::AllocateLeft,
                Event::ConstructLeft,
                Event::AllocateRight,
                Event::ConstructRight,
                Event::FreeRight,
                Event::DestroyLeft,
                Event::DestroyCuller,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("reported right constructor failure did not unwind raw storage");
    }
    if (!verifyFailureCase(
            FailureMode::RightConstructorReturnedNull,
            StereoResourceFailure::RightAccumulatorConstructorDidNotReturnThis,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::ConstructCuller,
                Event::AllocateLeft,
                Event::ConstructLeft,
                Event::AllocateRight,
                Event::ConstructRight,
                Event::DestroyRight,
                Event::DestroyLeft,
                Event::DestroyCuller,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("null right constructor return was accepted");
    }
    if (!verifyFailureCase(
            FailureMode::RightConstructorReturnedDifferentObject,
            StereoResourceFailure::RightAccumulatorConstructorDidNotReturnThis,
            {
                Event::AcquireCamera,
                Event::AllocateBinding,
                Event::ConstructCuller,
                Event::AllocateLeft,
                Event::ConstructLeft,
                Event::AllocateRight,
                Event::ConstructRight,
                Event::DestroyRight,
                Event::DestroyLeft,
                Event::DestroyCuller,
                Event::FreeBinding,
                Event::ReleaseCamera,
            }))
    {
        return fail("foreign right constructor return was accepted or destroyed");
    }

    {
        FakeBackend backend;
        {
            auto result = acquireCenterStereoResources<CollectorCapacity>(
                backend.operations(),
                authorizedForTest(),
                backend.expectedParameters);
            if (!result.succeeded()
                || result.failure != StereoResourceFailure::None
                || !result.resources.valid()
                || result.resources.viewPlan().left != StereoResourceView::Center
                || result.resources.viewPlan().right != StereoResourceView::Center
                || result.resources.camera() != &backend.camera
                || result.resources.collectorBinding() == nullptr
                || result.resources.cullingProcess()
                    != result.resources.collectorBinding()->cullingProcess()
                || result.resources.collectorBinding()->phase()
                    != geometry::GeometryCollectorPhase::Inactive
                || result.resources.leftAccumulator()
                    != backend.accumulatorStorage(true)
                || result.resources.rightAccumulator()
                    != backend.accumulatorStorage(false)
                || result.resources.leftAccumulator()
                    == result.resources.rightAccumulator()
                || !backend.metadataExact
                || !backend.constructorArgumentsExact
                || !backend.stateMachineValid
                || backend.leaseState != LeaseState::Retained
                || backend.cameraState != ResourceState::Constructed
                || backend.bindingState != ResourceState::Constructed
                || backend.leftState != ResourceState::Constructed
                || backend.rightState != ResourceState::Constructed
                || !sameEvents(
                    backend.events,
                    {
                        Event::RetainContext,
                        Event::AcquireCamera,
                        Event::AllocateBinding,
                        Event::ConstructCuller,
                        Event::AllocateLeft,
                        Event::ConstructLeft,
                        Event::AllocateRight,
                        Event::ConstructRight,
                    }))
            {
                return fail("successful center/center acquisition contract was violated");
            }

            OwnedResources moved = std::move(result.resources);
            if (result.resources.valid() || !moved.valid())
                return fail("move did not transfer exclusive resource ownership");
            result.resources = std::move(moved);
            if (!result.resources.valid() || moved.valid())
                return fail("move assignment did not transfer ownership exactly once");
        }

        if (!sameEvents(
                backend.events,
                {
                    Event::RetainContext,
                    Event::AcquireCamera,
                    Event::AllocateBinding,
                    Event::ConstructCuller,
                    Event::AllocateLeft,
                    Event::ConstructLeft,
                    Event::AllocateRight,
                    Event::ConstructRight,
                    Event::DestroyRight,
                    Event::DestroyLeft,
                    Event::DestroyCuller,
                    Event::FreeBinding,
                    Event::ReleaseCamera,
                    Event::ReleaseContext,
                }))
        {
            return fail("successful transaction did not release in exact reverse order");
        }
        if (!backend.fullyUnwound())
            return fail("successful transaction left a lifecycle state retained");
    }

    {
        FakeBackend firstBackend;
        FakeBackend secondBackend;
        {
            auto first = acquireCenterStereoResources<CollectorCapacity>(
                firstBackend.operations(),
                authorizedForTest(),
                firstBackend.expectedParameters);
            auto second = acquireCenterStereoResources<CollectorCapacity>(
                secondBackend.operations(),
                authorizedForTest(),
                secondBackend.expectedParameters);
            if (!first.succeeded() || !second.succeeded())
                return fail("move-assignment setup did not acquire both owners");

            first.resources = std::move(second.resources);
            if (!first.resources.valid()
                || second.resources.valid()
                || !firstBackend.fullyUnwound()
                || secondBackend.leaseState != LeaseState::Retained
                || secondBackend.cameraState != ResourceState::Constructed)
            {
                return fail("move assignment did not transfer the retained lease");
            }
        }
        if (!secondBackend.fullyUnwound())
            return fail("moved retained lease did not release from its new owner");
    }

    return EXIT_SUCCESS;
}
