#pragma once

#include "fnvxr_center_renderer_backend.h"
#include "fnvxr_private_geometry_collector.h"
#include "fnvxr_retail_engine_calls.h"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace fnvxr::engine
{
struct RetailEyeTargetOperations
{
    void* context = nullptr;
    bool (*snapshot)(void*) noexcept = nullptr;
    bool (*bind)(
        void*,
        CenterRendererEye,
        CenterRendererEyeIsolation&) noexcept = nullptr;
    bool (*end)(
        void*,
        CenterRendererEye,
        CenterRendererEyeIsolation&) noexcept = nullptr;
    void (*rollback)(
        void*,
        CenterRendererEye,
        CenterRendererEyeIsolation&) noexcept = nullptr;
    bool (*restore)(void*) noexcept = nullptr;
};

constexpr bool retailEyeTargetOperationsComplete(
    const RetailEyeTargetOperations& operations) noexcept
{
    return operations.snapshot
        && operations.bind
        && operations.end
        && operations.rollback
        && operations.restore;
}

template <std::size_t CollectorCapacity>
class RetailCenterRendererOperationsContext;

namespace detail
{
template <std::size_t CollectorCapacity>
struct RetailCenterRendererOperationsAdapter;
}

template <std::size_t CollectorCapacity>
class RetailCenterRendererOperationsContext final
{
public:
    using Binding =
        geometry::PrivateGeometryCollectorBinding<CollectorCapacity>;

    RetailCenterRendererOperationsContext() noexcept = default;
    RetailCenterRendererOperationsContext(
        const RetailCenterRendererOperationsContext&) = delete;
    RetailCenterRendererOperationsContext& operator=(
        const RetailCenterRendererOperationsContext&) = delete;

    bool initialize(
        const RetailEngineCalls& calls,
        Binding& binding,
        abi::RetailBSShaderAccumulatorLayout& collectionAccumulator,
        const RetailEyeTargetOperations& targets) noexcept
    {
        if (!calls.privateStereoComplete()
            || !binding.ownedVtableCloneInstalled()
            || !binding.ownedVtableIntegrityValid()
            || !retailEyeTargetOperationsComplete(targets))
        {
            return false;
        }
        mCalls = calls;
        mBinding = &binding;
        mCollectionAccumulator = &collectionAccumulator;
        mTargets = targets;
        mInitialized = true;
        return true;
    }

    bool ready() const noexcept
    {
        return mInitialized
            && mCalls.privateStereoComplete()
            && mBinding
            && mCollectionAccumulator
            && mBinding->ownedVtableCloneInstalled()
            && mBinding->ownedVtableIntegrityValid()
            && retailEyeTargetOperationsComplete(mTargets);
    }

private:
    struct EngineAccumulatorSnapshot
    {
        abi::RetailRendererAccumulatorOwnerLayout* renderer = nullptr;
        abi::RetailPointer32 rendererAccumulator = 0u;
        abi::RetailPointer32 accumulatingAccumulator = 0u;
        abi::RetailPointer32 renderingAccumulator = 0u;
        bool active = false;
    };

    bool mInitialized = false;
    RetailEngineCalls mCalls {};
    Binding* mBinding = nullptr;
    abi::RetailBSShaderAccumulatorLayout* mCollectionAccumulator = nullptr;
    RetailEyeTargetOperations mTargets {};
    EngineAccumulatorSnapshot mEngineSnapshot {};

    friend struct detail::RetailCenterRendererOperationsAdapter<
        CollectorCapacity>;
};

namespace detail
{
template <std::size_t CollectorCapacity>
struct RetailCenterRendererOperationsAdapter
{
    using Context = RetailCenterRendererOperationsContext<CollectorCapacity>;

    static Context* checked(void* opaque) noexcept
    {
        auto* context = static_cast<Context*>(opaque);
        return context && context->ready() ? context : nullptr;
    }

    static abi::RetailBSShaderAccumulatorLayout* accumulatorFromAddress(
        abi::RetailPointer32 address) noexcept
    {
        return reinterpret_cast<abi::RetailBSShaderAccumulatorLayout*>(
            static_cast<std::uintptr_t>(address));
    }

    static bool snapshotEngineAccumulatorState(Context& context) noexcept
    {
        if (context.mEngineSnapshot.active
            || !context.mCalls.privateStereoRegistrationComplete())
        {
            return false;
        }
        const abi::RetailPointer32 rendererAddress =
            *context.mCalls.rendererSingleton;
        if (rendererAddress == 0u)
            return false;
        auto* renderer =
            reinterpret_cast<abi::RetailRendererAccumulatorOwnerLayout*>(
                static_cast<std::uintptr_t>(rendererAddress));
        const abi::RetailPointer32 rendererAccumulator =
            renderer->accumulator;
        auto* ownedAccumulator = accumulatorFromAddress(rendererAccumulator);
        const abi::RetailPointer32 accumulatingAccumulator =
            *context.mCalls.accumulatingAccumulator;
        const abi::RetailPointer32 renderingAccumulator =
            *context.mCalls.renderingAccumulator;
        // At the stock world boundary these are three owners of the same
        // coordinator-owned accumulator. Requiring that exact state keeps the
        // saved pointer alive while the private eyes are temporarily installed.
        if (!ownedAccumulator
            || ownedAccumulator->referenceCount < 4u
            || accumulatingAccumulator != rendererAccumulator
            || renderingAccumulator != rendererAccumulator)
        {
            return false;
        }

        context.mEngineSnapshot = {
            renderer,
            rendererAccumulator,
            accumulatingAccumulator,
            renderingAccumulator,
            true,
        };
        return true;
    }

    static bool restoreEngineAccumulatorState(Context& context) noexcept
    {
        if (!context.mEngineSnapshot.active)
            return true;
        const typename Context::EngineAccumulatorSnapshot snapshot =
            context.mEngineSnapshot;
        auto* rendererAccumulator = accumulatorFromAddress(
            snapshot.rendererAccumulator);
        auto* accumulatingAccumulator = accumulatorFromAddress(
            snapshot.accumulatingAccumulator);
        auto* renderingAccumulator = accumulatorFromAddress(
            snapshot.renderingAccumulator);
        context.mCalls.setRenderingAccumulator(renderingAccumulator);
        context.mCalls.setAccumulatingAccumulator(accumulatingAccumulator);
        context.mCalls.rendererSetAccumulator(
            snapshot.renderer,
            rendererAccumulator);
        const bool restored = snapshot.renderer->accumulator
                == snapshot.rendererAccumulator
            && *context.mCalls.accumulatingAccumulator
                == snapshot.accumulatingAccumulator
            && *context.mCalls.renderingAccumulator
                == snapshot.renderingAccumulator;
        if (restored)
            context.mEngineSnapshot = {};
        return restored;
    }

    static bool registerEyeAccumulator(
        Context& context,
        abi::RetailBSShaderAccumulatorLayout* accumulator) noexcept
    {
        if (!context.mEngineSnapshot.active || !accumulator)
            return false;
        const std::uintptr_t accumulatorAddress =
            reinterpret_cast<std::uintptr_t>(accumulator);
        if (accumulatorAddress
            > (std::numeric_limits<abi::RetailPointer32>::max)())
        {
            return false;
        }
        const abi::RetailPointer32 address =
            static_cast<abi::RetailPointer32>(accumulatorAddress);
        context.mCalls.rendererSetAccumulator(
            context.mEngineSnapshot.renderer,
            accumulator);
        context.mCalls.setAccumulatingAccumulator(accumulator);
        // AddVisibleArray is permitted to draw immediately, before the
        // explicit render call. Its rendering-global must therefore already
        // name this eye's private accumulator; leaving the stock accumulator
        // installed sends those draws through the wrong camera/state lane and
        // produced the all-black private-eye payload seen in the live runs.
        context.mCalls.setRenderingAccumulator(accumulator);
        return context.mEngineSnapshot.renderer->accumulator == address
            && *context.mCalls.accumulatingAccumulator == address
            && *context.mCalls.renderingAccumulator == address;
    }

    static bool snapshot(void* opaque) noexcept
    {
        Context* context = checked(opaque);
        if (!context || !snapshotEngineAccumulatorState(*context))
            return false;
        if (!context->mTargets.snapshot(context->mTargets.context))
        {
            context->mEngineSnapshot = {};
            return false;
        }
        return true;
    }

    static bool collect(
        void* opaque,
        abi::RetailNiCameraLayout* camera,
        void* sceneObject,
        abi::RetailBSCullingProcessLayout* culler,
        std::uint64_t generation,
        CenterRendererVisibleSet& visibleSet) noexcept
    {
        visibleSet = {};
        Context* context = checked(opaque);
        if (!context
            || !camera
            || !sceneObject
            || !culler
            || !context->mCollectionAccumulator
            || context->mCollectionAccumulator->referenceCount != 1u
            || generation == 0u
            || culler != context->mBinding->cullingProcess())
        {
            return false;
        }

#if defined(_MSC_VER) && defined(_M_IX86)
        const std::uintptr_t collectionAccumulatorAddress =
            reinterpret_cast<std::uintptr_t>(
                context->mCollectionAccumulator);
        if (collectionAccumulatorAddress
                > (std::numeric_limits<abi::RetailPointer32>::max)()
            || culler->shaderAccumulator != 0u)
        {
            return false;
        }
        if (!context->mBinding->beginCollection(generation))
            return false;
        context->mCalls.accumulatorSetCamera(
            reinterpret_cast<abi::RetailNiAccumulatorLayout*>(
                context->mCollectionAccumulator),
            camera);
        context->mCalls.cullingProcessSetAccumulator(
            culler,
            context->mCollectionAccumulator);
        if (culler->shaderAccumulator
                != static_cast<abi::RetailPointer32>(
                    collectionAccumulatorAddress)
            || context->mCollectionAccumulator->referenceCount != 2u)
        {
            if (culler->shaderAccumulator
                == static_cast<abi::RetailPointer32>(
                    collectionAccumulatorAddress))
            {
                context->mCalls.cullingProcessSetAccumulator(culler, nullptr);
            }
            return false;
        }
        context->mCalls.cullingProcessAlt(
            culler,
            camera,
            sceneObject,
            nullptr);
        context->mCalls.cullingProcessSetAccumulator(culler, nullptr);
        if (culler->shaderAccumulator != 0u
            || context->mCollectionAccumulator->referenceCount != 1u)
            return false;
        if (context->mBinding->sealCollection()
            != geometry::GeometrySealResult::Sealed)
        {
            return false;
        }

        geometry::PrivateGeometrySealedView sealed {};
        if (!context->mBinding->tryGetSealedView(sealed)
            || !sealed.geometryPointers
            || sealed.itemCount == 0u
            || sealed.itemCount > CollectorCapacity
            || sealed.generation != generation)
        {
            return false;
        }
        const std::uintptr_t geometryAddress =
            reinterpret_cast<std::uintptr_t>(sealed.geometryPointers);
        if (geometryAddress
            > (std::numeric_limits<abi::RetailPointer32>::max)())
        {
            return false;
        }

        visibleSet.array.geometryPointers =
            static_cast<abi::RetailPointer32>(geometryAddress);
        visibleSet.array.itemCount = sealed.itemCount;
        visibleSet.array.capacity = static_cast<std::uint32_t>(
            CollectorCapacity);
        visibleSet.array.growBy = 0u;
        visibleSet.generation = sealed.generation;
        return true;
#else
        (void)generation;
        return false;
#endif
    }

    static bool bind(
        void* opaque,
        CenterRendererEye eye,
        CenterRendererEyeIsolation& isolation) noexcept
    {
        Context* context = checked(opaque);
        return context
            && context->mTargets.bind(
                context->mTargets.context,
                eye,
                isolation);
    }

    static bool setCamera(
        void* opaque,
        abi::RetailBSShaderAccumulatorLayout* accumulator,
        abi::RetailNiCameraLayout* camera) noexcept
    {
        Context* context = checked(opaque);
        if (!context || !accumulator || !camera)
            return false;
        if (!registerEyeAccumulator(*context, accumulator))
            return false;
        context->mCalls.accumulatorSetCamera(
            reinterpret_cast<abi::RetailNiAccumulatorLayout*>(accumulator),
            camera);
        return true;
    }

    static bool addVisible(
        void* opaque,
        abi::RetailBSShaderAccumulatorLayout* accumulator,
        const abi::RetailNiVisibleArrayLayout* visibleArray) noexcept
    {
        Context* context = checked(opaque);
        if (!context || !accumulator || !visibleArray)
            return false;
        context->mCalls.accumulatorAddVisibleArray(
            reinterpret_cast<abi::RetailNiAccumulatorLayout*>(accumulator),
            const_cast<abi::RetailNiVisibleArrayLayout*>(visibleArray));
        return true;
    }

    static bool render(
        void* opaque,
        abi::RetailNiCameraLayout* camera,
        abi::RetailBSShaderAccumulatorLayout* accumulator,
        std::uint32_t renderContext) noexcept
    {
        Context* context = checked(opaque);
        if (!context
            || !camera
            || !accumulator
            || renderContext != RetailWorldRenderContext)
        {
            return false;
        }
        context->mCalls.renderAccumulatorWithoutFinalize(
            camera,
            accumulator,
            renderContext);
        const std::uintptr_t accumulatorAddress =
            reinterpret_cast<std::uintptr_t>(accumulator);
        return accumulatorAddress
                <= (std::numeric_limits<abi::RetailPointer32>::max)()
            && *context->mCalls.renderingAccumulator
                == static_cast<abi::RetailPointer32>(accumulatorAddress);
    }

    static bool finalize(
        void* opaque,
        abi::RetailNiCameraLayout* camera,
        abi::RetailBSShaderAccumulatorLayout* accumulator,
        std::uint32_t renderContext) noexcept
    {
        Context* context = checked(opaque);
        if (!context
            || !camera
            || !accumulator
            || renderContext != RetailWorldRenderContext)
        {
            return false;
        }
        context->mCalls.finalizeAccumulator(
            camera,
            accumulator,
            renderContext);
        return true;
    }

    static bool end(
        void* opaque,
        CenterRendererEye eye,
        CenterRendererEyeIsolation& isolation) noexcept
    {
        Context* context = checked(opaque);
        return context
            && context->mTargets.end(
                context->mTargets.context,
                eye,
                isolation);
    }

    static void rollback(
        void* opaque,
        CenterRendererEye eye,
        CenterRendererEyeIsolation& isolation) noexcept
    {
        Context* context = checked(opaque);
        if (context)
        {
            context->mTargets.rollback(
                context->mTargets.context,
                eye,
                isolation);
            restoreEngineAccumulatorState(*context);
        }
    }

    static bool restore(void* opaque) noexcept
    {
        Context* context = checked(opaque);
        if (!context)
            return false;
        const bool targetsRestored =
            context->mTargets.restore(context->mTargets.context);
        const bool engineRestored = restoreEngineAccumulatorState(*context);
        return targetsRestored && engineRestored;
    }

    static void discard(
        void* opaque,
        CenterRendererVisibleSet& visibleSet) noexcept
    {
        Context* context = checked(opaque);
        if (context)
        {
            (void)context->mBinding
                ->resetCollectedGeometryPreservingOwnedVtable();
        }
        visibleSet = {};
    }

    static CenterRendererOperations operations(Context& context) noexcept
    {
        CenterRendererOperations result {};
        result.context = &context;
        result.snapshotAuthoritativeState = &snapshot;
        result.collectConservativeVisibleSet = &collect;
        result.bindEyeTargets = &bind;
        result.setAccumulatorCamera = &setCamera;
        result.addVisibleArray = &addVisible;
        result.renderAccumulatorWithoutFinalize = &render;
        result.finalizeAccumulator = &finalize;
        result.endEyeTargets = &end;
        result.rollbackEyeTargets = &rollback;
        result.restoreAuthoritativeState = &restore;
        result.discardVisibleSet = &discard;
        return result;
    }
};
}

template <std::size_t CollectorCapacity>
CenterRendererOperations makeRetailCenterRendererOperations(
    RetailCenterRendererOperationsContext<CollectorCapacity>& context) noexcept
{
    return detail::RetailCenterRendererOperationsAdapter<CollectorCapacity>
        ::operations(context);
}
}
