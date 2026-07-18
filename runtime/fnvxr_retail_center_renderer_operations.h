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
        const RetailEyeTargetOperations& targets) noexcept
    {
        if (!calls.complete()
            || !binding.ownedVtableCloneInstalled()
            || !binding.ownedVtableIntegrityValid()
            || !retailEyeTargetOperationsComplete(targets))
        {
            return false;
        }
        mCalls = calls;
        mBinding = &binding;
        mTargets = targets;
        mInitialized = true;
        return true;
    }

    bool ready() const noexcept
    {
        return mInitialized
            && mCalls.complete()
            && mBinding
            && mBinding->ownedVtableCloneInstalled()
            && mBinding->ownedVtableIntegrityValid()
            && retailEyeTargetOperationsComplete(mTargets);
    }

private:
    bool mInitialized = false;
    RetailEngineCalls mCalls {};
    Binding* mBinding = nullptr;
    RetailEyeTargetOperations mTargets {};

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

    static bool snapshot(void* opaque) noexcept
    {
        Context* context = checked(opaque);
        return context
            && context->mTargets.snapshot(context->mTargets.context);
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
            || generation == 0u
            || culler != context->mBinding->cullingProcess())
        {
            return false;
        }

#if defined(_MSC_VER) && defined(_M_IX86)
        if (!context->mBinding->beginCollection(generation))
            return false;
        context->mCalls.cullingProcessAlt(
            culler,
            camera,
            sceneObject,
            nullptr);
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
        return true;
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
        }
    }

    static bool restore(void* opaque) noexcept
    {
        Context* context = checked(opaque);
        return context
            && context->mTargets.restore(context->mTargets.context);
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
