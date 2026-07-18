#pragma once

#include "fnvxr_retail_engine_abi.h"
#include "fnvxr_retail_renderer_contract.h"
#include "fnvxr_retail_runtime_binding.h"

#include <cstddef>
#include <cstdint>

namespace fnvxr::engine
{
enum class CenterRendererEye : std::uint8_t
{
    Left,
    Right,
};

enum class CenterRendererFailure : std::uint8_t
{
    None,
    Unauthorized,
    InvalidInput,
    OperationTableIncomplete,
    Snapshot,
    Visibility,
    VisibilityInvalid,
    LeftBind,
    LeftSetCamera,
    LeftPopulate,
    LeftRender,
    LeftFinalize,
    LeftEnd,
    RightBind,
    RightSetCamera,
    RightPopulate,
    RightRender,
    RightFinalize,
    RightEnd,
    Restore,
};

struct CenterRendererVisibleSet
{
    abi::RetailNiVisibleArrayLayout array {};
    std::uint64_t generation = 0u;

    bool valid() const noexcept
    {
        return array.geometryPointers != 0u
            && array.itemCount != 0u
            && array.itemCount <= array.capacity
            && generation != 0u;
    }
};

struct CenterRendererEyeIsolation
{
    std::uintptr_t token = 0u;

    bool active() const noexcept
    {
        return token != 0u;
    }
};

struct CenterRendererFrameInput
{
    void* sceneObject = nullptr;
    abi::RetailNiCameraLayout* centerCamera = nullptr;
    abi::RetailNiCameraLayout* leftCamera = nullptr;
    abi::RetailNiCameraLayout* rightCamera = nullptr;
    abi::RetailBSCullingProcessLayout* privateCuller = nullptr;
    abi::RetailBSShaderAccumulatorLayout* leftAccumulator = nullptr;
    abi::RetailBSShaderAccumulatorLayout* rightAccumulator = nullptr;
    std::uint64_t generation = 0u;
};

struct CenterRendererOperations
{
    void* context = nullptr;

    bool (*snapshotAuthoritativeState)(void*) noexcept = nullptr;
    bool (*collectConservativeVisibleSet)(
        void*,
        abi::RetailNiCameraLayout*,
        void*,
        abi::RetailBSCullingProcessLayout*,
        std::uint64_t,
        CenterRendererVisibleSet&) noexcept = nullptr;
    bool (*bindEyeTargets)(
        void*,
        CenterRendererEye,
        CenterRendererEyeIsolation&) noexcept = nullptr;
    bool (*setAccumulatorCamera)(
        void*,
        abi::RetailBSShaderAccumulatorLayout*,
        abi::RetailNiCameraLayout*) noexcept = nullptr;
    bool (*addVisibleArray)(
        void*,
        abi::RetailBSShaderAccumulatorLayout*,
        const abi::RetailNiVisibleArrayLayout*) noexcept = nullptr;
    bool (*renderAccumulatorWithoutFinalize)(
        void*,
        abi::RetailNiCameraLayout*,
        abi::RetailBSShaderAccumulatorLayout*,
        std::uint32_t) noexcept = nullptr;
    bool (*finalizeAccumulator)(
        void*,
        abi::RetailNiCameraLayout*,
        abi::RetailBSShaderAccumulatorLayout*,
        std::uint32_t) noexcept = nullptr;
    bool (*endEyeTargets)(
        void*,
        CenterRendererEye,
        CenterRendererEyeIsolation&) noexcept = nullptr;
    void (*rollbackEyeTargets)(
        void*,
        CenterRendererEye,
        CenterRendererEyeIsolation&) noexcept = nullptr;
    bool (*restoreAuthoritativeState)(void*) noexcept = nullptr;
    void (*discardVisibleSet)(void*, CenterRendererVisibleSet&) noexcept = nullptr;
};

constexpr bool centerRendererOperationsComplete(
    const CenterRendererOperations& operations) noexcept
{
    return operations.snapshotAuthoritativeState
        && operations.collectConservativeVisibleSet
        && operations.bindEyeTargets
        && operations.setAccumulatorCamera
        && operations.addVisibleArray
        && operations.renderAccumulatorWithoutFinalize
        && operations.finalizeAccumulator
        && operations.endEyeTargets
        && operations.rollbackEyeTargets
        && operations.restoreAuthoritativeState
        && operations.discardVisibleSet;
}

class CenterRendererAuthorization;

namespace detail
{
struct CenterRendererAuthorizationAccess;
}

struct CenterRendererLifecycleTestAuthority;

class CenterRendererAuthorization final
{
public:
    constexpr CenterRendererAuthorization() noexcept = default;

private:
    using Validator = bool (*)(const void*) noexcept;

    explicit constexpr CenterRendererAuthorization(
        const void* evidence,
        Validator validator) noexcept
        : mEvidence(evidence),
          mValidator(validator)
    {
    }

    explicit constexpr CenterRendererAuthorization(
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

    friend struct detail::CenterRendererAuthorizationAccess;
    friend struct detail::RetailRuntimeAuthorityIssuer;
    friend struct CenterRendererLifecycleTestAuthority;
};

inline constexpr bool CenterRendererProductionAuthorizationAvailable =
    RetailRuntimeProductionAuthorizationAvailable;

namespace detail
{
struct CenterRendererAuthorizationAccess
{
    static bool authorized(
        const CenterRendererAuthorization& authorization) noexcept
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

struct CenterRendererResult
{
    bool complete = false;
    CenterRendererFailure failure = CenterRendererFailure::Unauthorized;
    std::uint64_t visibleSetGeneration = 0u;
    std::uint32_t visibleGeometryCount = 0u;
};

namespace detail
{
inline bool centerRendererInputValid(
    const CenterRendererFrameInput& input) noexcept
{
    return input.sceneObject
        && input.centerCamera
        && input.leftCamera
        && input.rightCamera
        && input.privateCuller
        && input.leftAccumulator
        && input.rightAccumulator
        && input.centerCamera != input.leftCamera
        && input.centerCamera != input.rightCamera
        && input.leftCamera != input.rightCamera
        && input.leftAccumulator != input.rightAccumulator
        && input.generation != 0u;
}

inline CenterRendererFailure eyeFailure(
    CenterRendererEye eye,
    CenterRendererFailure left,
    CenterRendererFailure right) noexcept
{
    return eye == CenterRendererEye::Left ? left : right;
}

inline CenterRendererFailure renderCenterEye(
    const CenterRendererOperations& operations,
    CenterRendererEye eye,
    abi::RetailNiCameraLayout* camera,
    abi::RetailBSShaderAccumulatorLayout* accumulator,
    const CenterRendererVisibleSet& visibleSet,
    CenterRendererEyeIsolation& isolation) noexcept
{
    if (!operations.bindEyeTargets(operations.context, eye, isolation)
        || !isolation.active())
    {
        return eyeFailure(
            eye,
            CenterRendererFailure::LeftBind,
            CenterRendererFailure::RightBind);
    }
    if (!operations.setAccumulatorCamera(
            operations.context,
            accumulator,
            camera))
    {
        return eyeFailure(
            eye,
            CenterRendererFailure::LeftSetCamera,
            CenterRendererFailure::RightSetCamera);
    }
    // AddVisibleArray can render immediately.  The eye target is deliberately
    // bound above before this call.
    if (!operations.addVisibleArray(
            operations.context,
            accumulator,
            &visibleSet.array))
    {
        return eyeFailure(
            eye,
            CenterRendererFailure::LeftPopulate,
            CenterRendererFailure::RightPopulate);
    }
    if (!operations.renderAccumulatorWithoutFinalize(
            operations.context,
            camera,
            accumulator,
            RetailWorldRenderContext))
    {
        return eyeFailure(
            eye,
            CenterRendererFailure::LeftRender,
            CenterRendererFailure::RightRender);
    }
    if (!operations.finalizeAccumulator(
            operations.context,
            camera,
            accumulator,
            RetailWorldRenderContext))
    {
        return eyeFailure(
            eye,
            CenterRendererFailure::LeftFinalize,
            CenterRendererFailure::RightFinalize);
    }
    if (!operations.endEyeTargets(operations.context, eye, isolation)
        || isolation.active())
    {
        return eyeFailure(
            eye,
            CenterRendererFailure::LeftEnd,
            CenterRendererFailure::RightEnd);
    }
    return CenterRendererFailure::None;
}

inline CenterRendererResult failCenterRenderer(
    const CenterRendererOperations& operations,
    CenterRendererFailure failure,
    CenterRendererVisibleSet& visibleSet,
    CenterRendererEyeIsolation* isolation = nullptr,
    CenterRendererEye eye = CenterRendererEye::Left) noexcept
{
    if (isolation && isolation->active())
        operations.rollbackEyeTargets(operations.context, eye, *isolation);
    if (!operations.restoreAuthoritativeState(operations.context))
        failure = CenterRendererFailure::Restore;
    operations.discardVisibleSet(operations.context, visibleSet);
    return { false, failure, 0u, 0u };
}
}

inline CenterRendererResult executeCenterRendererFrame(
    const CenterRendererOperations& operations,
    const CenterRendererAuthorization& authorization,
    const CenterRendererFrameInput& input) noexcept
{
    if (!detail::CenterRendererAuthorizationAccess::authorized(authorization))
        return { false, CenterRendererFailure::Unauthorized, 0u, 0u };
    if (!detail::centerRendererInputValid(input))
        return { false, CenterRendererFailure::InvalidInput, 0u, 0u };
    if (!centerRendererOperationsComplete(operations))
    {
        return {
            false,
            CenterRendererFailure::OperationTableIncomplete,
            0u,
            0u,
        };
    }
    if (!operations.snapshotAuthoritativeState(operations.context))
        return { false, CenterRendererFailure::Snapshot, 0u, 0u };

    CenterRendererVisibleSet visibleSet {};
    if (!operations.collectConservativeVisibleSet(
            operations.context,
            input.centerCamera,
            input.sceneObject,
            input.privateCuller,
            input.generation,
            visibleSet))
    {
        return detail::failCenterRenderer(
            operations,
            CenterRendererFailure::Visibility,
            visibleSet);
    }
    if (!visibleSet.valid() || visibleSet.generation != input.generation)
    {
        return detail::failCenterRenderer(
            operations,
            CenterRendererFailure::VisibilityInvalid,
            visibleSet);
    }

    CenterRendererEyeIsolation isolation {};
    CenterRendererFailure failure = detail::renderCenterEye(
        operations,
        CenterRendererEye::Left,
        input.leftCamera,
        input.leftAccumulator,
        visibleSet,
        isolation);
    if (failure != CenterRendererFailure::None)
    {
        return detail::failCenterRenderer(
            operations,
            failure,
            visibleSet,
            &isolation,
            CenterRendererEye::Left);
    }

    failure = detail::renderCenterEye(
        operations,
        CenterRendererEye::Right,
        input.rightCamera,
        input.rightAccumulator,
        visibleSet,
        isolation);
    if (failure != CenterRendererFailure::None)
    {
        return detail::failCenterRenderer(
            operations,
            failure,
            visibleSet,
            &isolation,
            CenterRendererEye::Right);
    }

    if (!operations.restoreAuthoritativeState(operations.context))
    {
        operations.discardVisibleSet(operations.context, visibleSet);
        return { false, CenterRendererFailure::Restore, 0u, 0u };
    }

    const CenterRendererResult result {
        true,
        CenterRendererFailure::None,
        visibleSet.generation,
        visibleSet.array.itemCount,
    };
    operations.discardVisibleSet(operations.context, visibleSet);
    return result;
}
}
