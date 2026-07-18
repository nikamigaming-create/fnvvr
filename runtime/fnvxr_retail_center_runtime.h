#pragma once

#include "fnvxr_retail_eye_camera_transaction.h"
#include "fnvxr_retail_center_renderer_operations.h"
#include "fnvxr_retail_engine_resource_adapter.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace fnvxr::engine
{
enum class RetailCenterRuntimeFailure : std::uint8_t
{
    None = 0u,
    AlreadyInitialized,
    ResourceContextRejected,
    ResourceAcquisitionRejected,
    EyeCameraAcquisitionRejected,
    RendererOperationsRejected,
    RuntimeNotReady,
    InvalidFrame,
    StaleFrameGeneration,
    CameraPointerMismatch,
    TrackedFrameRejected,
    StalePoseFrame,
    EyeCameraDerivationRejected,
    EyeCameraTransactionRejected,
    StereoRenderRejected,
};

enum class RetailWorldHookDisposition : std::uint8_t
{
    RejectGameplayFrame = 0u,
    CallOriginalForUi,
    StereoWorldComplete,
};

struct RetailCenterRuntimeFrame
{
    void* sceneObject = nullptr;
    const abi::RetailSceneGraphLayout* sceneGraph = nullptr;
    abi::RetailNiCameraLayout* stockCenterCamera = nullptr;
    RetailTrackedFrame tracked {};
    float gameUnitsPerMeter = 0.0f;
    std::uint64_t generation = 0u;
};

struct RetailCenterRuntimeFrameResult
{
    RetailWorldHookDisposition disposition =
        RetailWorldHookDisposition::RejectGameplayFrame;
    RetailCenterRuntimeFailure failure =
        RetailCenterRuntimeFailure::RuntimeNotReady;
    CenterRendererResult renderer {};
};

// Owns the private camera/culler/accumulators and turns a proven retail world
// hook invocation into one conservative cull plus two engine renders. Gameplay
// failures never fall back to a flat world. The original function is admitted
// only for the explicitly allowed menu/dialogue/Pip-Boy scene-graph mode.
template <std::size_t CollectorCapacity>
class RetailCenterStereoRuntime final
{
public:
    using Resources = OwnedStereoResources<CollectorCapacity>;
    using ResourceContext = RetailEngineResourceContext<CollectorCapacity>;
    using RendererContext =
        RetailCenterRendererOperationsContext<CollectorCapacity>;

    RetailCenterStereoRuntime() noexcept = default;
    RetailCenterStereoRuntime(const RetailCenterStereoRuntime&) = delete;
    RetailCenterStereoRuntime& operator=(
        const RetailCenterStereoRuntime&) = delete;

    bool initialize(
        const RetailEngineCallResolution& resolution,
        std::uintptr_t loadedImageBase,
        const abi::RetailPointer32* verifiedCullerVtable,
        std::uint64_t runtimeGeneration,
        const StereoResourceAuthorization& resourceAuthorization,
        const RetailEyeTargetOperations& targets) noexcept
    {
        if (mInitialized || mResources.valid())
            return fail(RetailCenterRuntimeFailure::AlreadyInitialized);
        if (!mResourceContext.initialize(
                resolution,
                loadedImageBase,
                verifiedCullerVtable,
                runtimeGeneration))
        {
            return fail(RetailCenterRuntimeFailure::ResourceContextRejected);
        }

        auto acquisition = acquireCenterStereoResources<CollectorCapacity>(
            makeRetailEngineStereoResourceOperations(mResourceContext),
            resourceAuthorization,
            RetailWorldStereoResourceConstructionParameters);
        if (!acquisition.succeeded())
        {
            mResourceFailure = acquisition.failure;
            return fail(
                RetailCenterRuntimeFailure::ResourceAcquisitionRejected);
        }
        mResources = std::move(acquisition.resources);
        if (!mRendererContext.initialize(
                resolution.calls,
                *mResources.collectorBinding(),
                targets))
        {
            mResources = {};
            return fail(RetailCenterRuntimeFailure::RendererOperationsRejected);
        }
        if (!mEyeCameras.initialize(
                resolution.calls,
                mResources.camera()))
        {
            mEyeCameraFailure = mEyeCameras.failure();
            mResources = {};
            return fail(
                RetailCenterRuntimeFailure::EyeCameraAcquisitionRejected);
        }

        mRendererOperations = makeRetailCenterRendererOperations(
            mRendererContext);
        mRuntimeGeneration = runtimeGeneration;
        mLastFrameGeneration = 0u;
        mLastPoseFrame = 0u;
        mLastPoseSequence = 0;
        mOrigin = {};
        mFailure = RetailCenterRuntimeFailure::None;
        mInitialized = true;
        return true;
    }

    bool rebindEyeTargets(
        const RetailEngineCalls& calls,
        const RetailEyeTargetOperations& targets) noexcept
    {
        if (!mInitialized || !mResources.valid())
            return fail(RetailCenterRuntimeFailure::RuntimeNotReady);
        if (!mRendererContext.initialize(
                calls,
                *mResources.collectorBinding(),
                targets))
        {
            return fail(RetailCenterRuntimeFailure::RendererOperationsRejected);
        }
        mRendererOperations = makeRetailCenterRendererOperations(
            mRendererContext);
        mFailure = RetailCenterRuntimeFailure::None;
        return true;
    }

    bool ready() const noexcept
    {
        return mInitialized
            && mResources.valid()
            && mEyeCameras.valid()
            && mRuntimeGeneration != 0u
            && centerRendererOperationsComplete(mRendererOperations);
    }

    RetailCenterRuntimeFailure failure() const noexcept
    {
        return mFailure;
    }

    StereoResourceFailure resourceFailure() const noexcept
    {
        return mResourceFailure;
    }

    RetailEyeCameraFailure eyeCameraFailure() const noexcept
    {
        return mEyeCameraFailure;
    }

    const Resources& resources() const noexcept
    {
        return mResources;
    }

    RetailCenterRuntimeFrameResult renderWorld(
        const CenterRendererAuthorization& authorization,
        const RetailCenterRuntimeFrame& frame) noexcept
    {
        if (!ready())
        {
            return reject(RetailCenterRuntimeFailure::RuntimeNotReady);
        }
        if (!frame.sceneObject
            || !frame.sceneGraph
            || !frame.stockCenterCamera
            || frame.generation == 0u)
        {
            return reject(RetailCenterRuntimeFailure::InvalidFrame);
        }
        if (frame.sceneGraph->isMenuSceneGraph != 0u)
        {
            mFailure = RetailCenterRuntimeFailure::None;
            return {
                RetailWorldHookDisposition::CallOriginalForUi,
                RetailCenterRuntimeFailure::None,
                {},
            };
        }
        if (frame.generation <= mLastFrameGeneration)
        {
            return reject(RetailCenterRuntimeFailure::StaleFrameGeneration);
        }

        const std::uintptr_t cameraAddress =
            reinterpret_cast<std::uintptr_t>(frame.stockCenterCamera);
        if (cameraAddress
                > (std::numeric_limits<abi::RetailPointer32>::max)()
            || frame.sceneGraph->camera
                != static_cast<abi::RetailPointer32>(cameraAddress))
        {
            return reject(RetailCenterRuntimeFailure::CameraPointerMismatch);
        }

        const RetailTrackedFrameValidation trackedValidation =
            validateRetailTrackedGameplayFrame(frame.tracked);
        if (!trackedValidation.complete())
            return reject(RetailCenterRuntimeFailure::TrackedFrameRejected);
        if ((mLastPoseFrame != 0u
                && frame.tracked.pose.frame <= mLastPoseFrame)
            || (mLastPoseSequence != 0
                && frame.tracked.poseSequence == mLastPoseSequence))
        {
            return reject(RetailCenterRuntimeFailure::StalePoseFrame);
        }

        const RetailVrOriginCandidate originCandidate =
            prepareRetailVrOriginCandidate(mOrigin, frame.tracked);
        if (!originCandidate.complete())
        {
            mEyeCameraFailure = originCandidate.failure;
            return reject(
                RetailCenterRuntimeFailure::EyeCameraDerivationRejected);
        }
        const RetailDerivedEyeCameraRig cameraRig = deriveRetailEyeCameraRig(
            frame.stockCenterCamera,
            frame.tracked,
            originCandidate.origin,
            frame.gameUnitsPerMeter);
        if (!cameraRig.complete())
        {
            mEyeCameraFailure = cameraRig.failure;
            return reject(
                RetailCenterRuntimeFailure::EyeCameraDerivationRejected);
        }

        const RetailPrivateCameraSet cameras {
            mResources.camera(),
            mEyeCameras.left(),
            mEyeCameras.right(),
        };
        RetailScopedEyeCameraTransaction cameraTransaction;
        if (!cameraTransaction.begin(
                frame.stockCenterCamera,
                cameras,
                cameraRig))
        {
            mEyeCameraFailure = cameraTransaction.failure();
            return reject(
                RetailCenterRuntimeFailure::EyeCameraTransactionRejected);
        }

        const CenterRendererFrameInput input {
            frame.sceneObject,
            cameras.center,
            cameras.left,
            cameras.right,
            mResources.cullingProcess(),
            mResources.leftAccumulator(),
            mResources.rightAccumulator(),
            frame.generation,
        };
        CenterRendererResult renderer = executeCenterRendererFrame(
            mRendererOperations,
            authorization,
            input);
        if (!renderer.complete
            || renderer.failure != CenterRendererFailure::None)
        {
            mFailure = RetailCenterRuntimeFailure::StereoRenderRejected;
            return {
                RetailWorldHookDisposition::RejectGameplayFrame,
                mFailure,
                renderer,
            };
        }
        mLastFrameGeneration = frame.generation;
        mLastPoseFrame = frame.tracked.pose.frame;
        mLastPoseSequence = frame.tracked.poseSequence;
        mOrigin = originCandidate.origin;
        mEyeCameraFailure = RetailEyeCameraFailure::None;
        mFailure = RetailCenterRuntimeFailure::None;
        return {
            RetailWorldHookDisposition::StereoWorldComplete,
            RetailCenterRuntimeFailure::None,
            renderer,
        };
    }

private:
    bool fail(RetailCenterRuntimeFailure failure) noexcept
    {
        mFailure = failure;
        return false;
    }

    RetailCenterRuntimeFrameResult reject(
        RetailCenterRuntimeFailure failure) noexcept
    {
        mFailure = failure;
        return {
            RetailWorldHookDisposition::RejectGameplayFrame,
            failure,
            {},
        };
    }

    bool mInitialized = false;
    std::uint64_t mRuntimeGeneration = 0u;
    std::uint64_t mLastFrameGeneration = 0u;
    std::uint64_t mLastPoseFrame = 0u;
    LONG mLastPoseSequence = 0;
    RetailCenterRuntimeFailure mFailure =
        RetailCenterRuntimeFailure::RuntimeNotReady;
    StereoResourceFailure mResourceFailure =
        StereoResourceFailure::Unauthorized;
    RetailEyeCameraFailure mEyeCameraFailure =
        RetailEyeCameraFailure::FactoryUnavailable;
    RetailVrOrigin mOrigin {};
    ResourceContext mResourceContext {};
    Resources mResources {};
    RetailPrivateEyeCameraPair mEyeCameras {};
    RendererContext mRendererContext {};
    CenterRendererOperations mRendererOperations {};
};
}
