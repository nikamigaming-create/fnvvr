#pragma once

#include "fnvxr_retail_engine_calls.h"
#include "fnvxr_retail_tracked_frame.h"
#include "../renderhook/fnvxr_stereo_math.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace fnvxr::engine
{
// NiAVObject and NiCamera offsets are independently corroborated by the
// retail xNVSE/JIP layouts and the retained loaded-process camera captures.
// The production transaction accesses the two transforms through memcpy so
// no unrelated C++ overlay type is used to alias a live retail object.
inline constexpr std::size_t RetailNiAvObjectParentOffset = 0x18u;
inline constexpr std::size_t RetailNiAvObjectLocalTransformOffset = 0x34u;
inline constexpr std::size_t RetailNiAvObjectWorldTransformOffset = 0x68u;

struct RetailNiTransformLayout
{
    float rotation[9] {};
    float translation[3] {};
    float scale = 0.0f;
};

struct RetailNiCameraSpatialEvidenceLayout
{
    abi::RetailPointer32 vtable = 0u;
    std::uint32_t referenceCount = 0u;
    std::uint8_t opaque08[0x10] {};
    abi::RetailPointer32 parent = 0u;
    std::uint8_t opaque1C[0x18] {};
    RetailNiTransformLayout local {};
    RetailNiTransformLayout world {};
    float worldToCamera[16] {};
    abi::RetailNiFrustumLayout frustum {};
    float minimumNearPlane = 0.0f;
    float maximumFarNearRatio = 0.0f;
    abi::RetailNiViewportLayout viewport {};
    float lodAdjust = 0.0f;
};

static_assert(sizeof(RetailNiTransformLayout) == 0x34u);
static_assert(sizeof(RetailNiCameraSpatialEvidenceLayout) == 0x114u);
static_assert(offsetof(RetailNiCameraSpatialEvidenceLayout, parent) == 0x18u);
static_assert(offsetof(RetailNiCameraSpatialEvidenceLayout, local) == 0x34u);
static_assert(offsetof(RetailNiCameraSpatialEvidenceLayout, world) == 0x68u);
static_assert(
    offsetof(RetailNiCameraSpatialEvidenceLayout, worldToCamera) == 0x9Cu);
static_assert(offsetof(RetailNiCameraSpatialEvidenceLayout, frustum) == 0xDCu);
static_assert(
    sizeof(RetailNiCameraSpatialEvidenceLayout)
    == sizeof(abi::RetailNiCameraLayout));

enum class RetailEyeCameraFailure : std::uint8_t
{
    None = 0u,
    FactoryUnavailable,
    LeftFactoryReturnedNull,
    RightFactoryReturnedNull,
    CameraAlias,
    PoseRejected,
    OriginRejected,
    ScaleRejected,
    StockCameraRejected,
    PrivateCameraRejected,
    EyeBaselineRejected,
    EyeFovRejected,
    CenterCullRejected,
    MatrixRejected,
};

struct RetailVrOrigin
{
    stereo::Quaternion orientation {};
    stereo::Vector3 position {};
    std::uint64_t producerEpoch = 0u;
    std::uint32_t referenceSpaceGeneration = 0u;
    std::uint32_t recenterRequestId = 0u;
    std::uint64_t lastPoseFrame = 0u;
    LONG lastPoseSequence = 0;
    bool valid = false;
};

struct RetailVrOriginCandidate
{
    RetailVrOrigin origin {};
    bool relatched = false;
    RetailEyeCameraFailure failure = RetailEyeCameraFailure::OriginRejected;

    constexpr bool complete() const noexcept
    {
        return failure == RetailEyeCameraFailure::None && origin.valid;
    }
};

struct RetailCameraMutableState
{
    RetailNiTransformLayout local {};
    RetailNiTransformLayout world {};
    float worldToCamera[16] {};
    abi::RetailNiFrustumLayout frustum {};
    float minimumNearPlane = 0.0f;
    float maximumFarNearRatio = 0.0f;
    abi::RetailNiViewportLayout viewport {};
    float lodAdjust = 0.0f;
};

struct RetailDerivedEyeCameraRig
{
    RetailCameraMutableState center {};
    RetailCameraMutableState left {};
    RetailCameraMutableState right {};
    RetailEyeCameraFailure failure = RetailEyeCameraFailure::PoseRejected;

    constexpr bool complete() const noexcept
    {
        return failure == RetailEyeCameraFailure::None;
    }
};

struct RetailPrivateCameraSet
{
    abi::RetailNiCameraLayout* center = nullptr;
    abi::RetailNiCameraLayout* left = nullptr;
    abi::RetailNiCameraLayout* right = nullptr;
};

namespace detail
{
inline bool retailCameraFinite(const float* values, std::size_t count) noexcept
{
    if (!values)
        return false;
    for (std::size_t index = 0u; index < count; ++index)
    {
        if (!std::isfinite(values[index]))
            return false;
    }
    return true;
}

inline abi::RetailPointer32 retailCameraReadPointer32(
    const abi::RetailNiCameraLayout* camera,
    std::size_t offset) noexcept
{
    abi::RetailPointer32 value = 0u;
    if (camera)
    {
        std::memcpy(
            &value,
            reinterpret_cast<const std::uint8_t*>(camera) + offset,
            sizeof(value));
    }
    return value;
}

inline RetailNiTransformLayout retailCameraReadTransform(
    const abi::RetailNiCameraLayout* camera,
    std::size_t offset) noexcept
{
    RetailNiTransformLayout result {};
    if (camera)
    {
        std::memcpy(
            &result,
            reinterpret_cast<const std::uint8_t*>(camera) + offset,
            sizeof(result));
    }
    return result;
}

inline void retailCameraWriteTransform(
    abi::RetailNiCameraLayout* camera,
    std::size_t offset,
    const RetailNiTransformLayout& transform) noexcept
{
    std::memcpy(
        reinterpret_cast<std::uint8_t*>(camera) + offset,
        &transform,
        sizeof(transform));
}

inline bool retailRotationProper(const float rotation[9]) noexcept
{
    if (!retailCameraFinite(rotation, 9u))
        return false;
    constexpr double Tolerance = 0.003;
    for (std::size_t column = 0u; column < 3u; ++column)
    {
        for (std::size_t other = 0u; other < 3u; ++other)
        {
            double dot = 0.0;
            for (std::size_t row = 0u; row < 3u; ++row)
            {
                dot += static_cast<double>(rotation[row * 3u + column])
                    * static_cast<double>(rotation[row * 3u + other]);
            }
            const double expected = column == other ? 1.0 : 0.0;
            if (std::fabs(dot - expected) > Tolerance)
                return false;
        }
    }
    const double determinant =
        static_cast<double>(rotation[0])
            * (static_cast<double>(rotation[4]) * rotation[8]
                - static_cast<double>(rotation[5]) * rotation[7])
        - static_cast<double>(rotation[1])
            * (static_cast<double>(rotation[3]) * rotation[8]
                - static_cast<double>(rotation[5]) * rotation[6])
        + static_cast<double>(rotation[2])
            * (static_cast<double>(rotation[3]) * rotation[7]
                - static_cast<double>(rotation[4]) * rotation[6]);
    return std::fabs(determinant - 1.0) <= Tolerance;
}

inline bool retailTransformUsable(
    const RetailNiTransformLayout& transform) noexcept
{
    return retailRotationProper(transform.rotation)
        && retailCameraFinite(transform.translation, 3u)
        && std::isfinite(transform.scale)
        && transform.scale >= 0.0001f
        && transform.scale <= 10000.0f;
}

inline bool retailFrustumUsable(
    const abi::RetailNiFrustumLayout& frustum) noexcept
{
    constexpr float MaximumDistance = 1000000.0f;
    const float values[6] {
        frustum.left,
        frustum.right,
        frustum.top,
        frustum.bottom,
        frustum.nearDistance,
        frustum.farDistance,
    };
    return retailCameraFinite(values, 6u)
        && frustum.orthographic == 0u
        && frustum.left < frustum.right
        && frustum.bottom < frustum.top
        && frustum.nearDistance >= 0.01f
        && frustum.farDistance > frustum.nearDistance
        && frustum.farDistance <= MaximumDistance;
}

inline stereo::Matrix3 retailMatrix3(
    const float rotation[9]) noexcept
{
    stereo::Matrix3 result {};
    for (std::size_t row = 0u; row < 3u; ++row)
    {
        for (std::size_t column = 0u; column < 3u; ++column)
            result.m[row][column] = rotation[row * 3u + column];
    }
    return result;
}

inline RetailNiTransformLayout retailTransform(
    const stereo::Matrix3& rotation,
    const stereo::Vector3& translation,
    float scale) noexcept
{
    RetailNiTransformLayout result {};
    for (std::size_t row = 0u; row < 3u; ++row)
    {
        for (std::size_t column = 0u; column < 3u; ++column)
            result.rotation[row * 3u + column] = rotation.m[row][column];
    }
    result.translation[0] = translation.x;
    result.translation[1] = translation.y;
    result.translation[2] = translation.z;
    result.scale = scale;
    return result;
}

inline bool buildRetailWorldToCamera(
    const RetailNiTransformLayout& world,
    const abi::RetailNiFrustumLayout& frustum,
    float output[16]) noexcept
{
    if (!output
        || !retailTransformUsable(world)
        || !retailFrustumUsable(frustum))
    {
        return false;
    }

    const double inverseScale = 1.0 / static_cast<double>(world.scale);
    double view[3][4] {};
    // Camera columns are right, up, back. D3D clip depth is positive in the
    // forward direction, hence the negated third column.
    for (std::size_t axis = 0u; axis < 3u; ++axis)
    {
        const double sign = axis == 2u ? -1.0 : 1.0;
        for (std::size_t component = 0u; component < 3u; ++component)
        {
            view[axis][component] = sign
                * static_cast<double>(
                    world.rotation[component * 3u + axis])
                * inverseScale;
        }
        view[axis][3] = -(
            view[axis][0] * static_cast<double>(world.translation[0])
            + view[axis][1] * static_cast<double>(world.translation[1])
            + view[axis][2] * static_cast<double>(world.translation[2]));
    }

    const double left = frustum.left;
    const double right = frustum.right;
    const double top = frustum.top;
    const double bottom = frustum.bottom;
    const double nearDistance = frustum.nearDistance;
    const double farDistance = frustum.farDistance;
    const double horizontalScale = 2.0 / (right - left);
    const double horizontalOffset = -(right + left) / (right - left);
    const double verticalScale = 2.0 / (top - bottom);
    const double verticalOffset = -(top + bottom) / (top - bottom);
    const double depthScale = farDistance / (farDistance - nearDistance);
    const double depthOffset =
        -(farDistance * nearDistance) / (farDistance - nearDistance);

    double matrix[4][4] {};
    for (std::size_t component = 0u; component < 4u; ++component)
    {
        matrix[0][component] = horizontalScale * view[0][component]
            + horizontalOffset * view[2][component];
        matrix[1][component] = verticalScale * view[1][component]
            + verticalOffset * view[2][component];
        matrix[2][component] = depthScale * view[2][component];
        matrix[3][component] = view[2][component];
    }
    matrix[2][3] += depthOffset;

    for (std::size_t row = 0u; row < 4u; ++row)
    {
        for (std::size_t column = 0u; column < 4u; ++column)
        {
            const double value = matrix[row][column];
            if (!std::isfinite(value)
                || value > (std::numeric_limits<float>::max)()
                || value < -(std::numeric_limits<float>::max)())
            {
                return false;
            }
            output[row * 4u + column] = static_cast<float>(value);
        }
    }
    return retailCameraFinite(output, 16u);
}

inline RetailCameraMutableState retailCameraMutableState(
    const abi::RetailNiCameraLayout* camera) noexcept
{
    RetailCameraMutableState result {};
    if (!camera)
        return result;
    result.local = retailCameraReadTransform(
        camera,
        RetailNiAvObjectLocalTransformOffset);
    result.world = retailCameraReadTransform(
        camera,
        RetailNiAvObjectWorldTransformOffset);
    std::memcpy(
        result.worldToCamera,
        camera->worldToCamera,
        sizeof(result.worldToCamera));
    result.frustum = camera->frustum;
    result.minimumNearPlane = camera->minimumNearPlane;
    result.maximumFarNearRatio = camera->maximumFarNearRatio;
    result.viewport = camera->viewport;
    result.lodAdjust = camera->lodAdjust;
    return result;
}

inline void writeRetailCameraMutableState(
    abi::RetailNiCameraLayout* camera,
    const RetailCameraMutableState& state) noexcept
{
    retailCameraWriteTransform(
        camera,
        RetailNiAvObjectLocalTransformOffset,
        state.local);
    retailCameraWriteTransform(
        camera,
        RetailNiAvObjectWorldTransformOffset,
        state.world);
    std::memcpy(
        camera->worldToCamera,
        state.worldToCamera,
        sizeof(state.worldToCamera));
    camera->frustum = state.frustum;
    camera->minimumNearPlane = state.minimumNearPlane;
    camera->maximumFarNearRatio = state.maximumFarNearRatio;
    camera->viewport = state.viewport;
    camera->lodAdjust = state.lodAdjust;
}

inline bool privateCameraSetUsable(
    const RetailPrivateCameraSet& cameras,
    const abi::RetailNiCameraLayout* stock) noexcept
{
    const auto aligned = [](const void* value) noexcept {
        return reinterpret_cast<std::uintptr_t>(value)
            % alignof(abi::RetailNiCameraLayout) == 0u;
    };
    if (!stock
        || !cameras.center
        || !cameras.left
        || !cameras.right
        || cameras.center == cameras.left
        || cameras.center == cameras.right
        || cameras.left == cameras.right
        || cameras.center == stock
        || cameras.left == stock
        || cameras.right == stock
        || !aligned(cameras.center)
        || !aligned(cameras.left)
        || !aligned(cameras.right))
    {
        return false;
    }
    const abi::RetailNiCameraLayout* values[3] {
        cameras.center,
        cameras.left,
        cameras.right,
    };
    for (const abi::RetailNiCameraLayout* camera : values)
    {
        if (retailCameraReadPointer32(camera, 0u) == 0u
            || retailCameraReadPointer32(
                    camera,
                    RetailNiAvObjectParentOffset) != 0u)
        {
            return false;
        }
    }
    return true;
}

inline stereo::Quaternion poseQuaternion(const float value[4]) noexcept
{
    return { value[0], value[1], value[2], value[3] };
}

inline stereo::Vector3 posePosition(const float value[3]) noexcept
{
    return { value[0], value[1], value[2] };
}

inline stereo::Vector3 eyeMidpoint(
    const shared::SharedVrPoseState& pose) noexcept
{
    return {
        (pose.leftEyePos[0] + pose.rightEyePos[0]) * 0.5f,
        (pose.leftEyePos[1] + pose.rightEyePos[1]) * 0.5f,
        (pose.leftEyePos[2] + pose.rightEyePos[2]) * 0.5f,
    };
}

inline abi::RetailNiFrustumLayout eyeFrustum(
    const abi::RetailNiFrustumLayout& stock,
    const float fov[4]) noexcept
{
    abi::RetailNiFrustumLayout result = stock;
    result.left = std::tan(fov[0]);
    result.right = std::tan(fov[1]);
    result.top = std::tan(fov[2]);
    result.bottom = std::tan(fov[3]);
    result.orthographic = 0u;
    result.reserved19[0] = 0u;
    result.reserved19[1] = 0u;
    result.reserved19[2] = 0u;
    return result;
}

inline bool configureDerivedCamera(
    RetailCameraMutableState& destination,
    const RetailCameraMutableState& stock,
    const stereo::Matrix3& bodyWorldRotation,
    const stereo::Matrix3& localRotation,
    const stereo::Vector3& localPositionMeters,
    float gameUnitsPerMeter,
    const abi::RetailNiFrustumLayout& frustum) noexcept
{
    const stereo::Matrix3 worldRotation = stereo::multiply(
        bodyWorldRotation,
        localRotation);
    const stereo::Vector3 localUnits {
        localPositionMeters.x * gameUnitsPerMeter,
        localPositionMeters.y * gameUnitsPerMeter,
        localPositionMeters.z * gameUnitsPerMeter,
    };
    const stereo::Vector3 worldOffset = stereo::transform(
        bodyWorldRotation,
        localUnits);
    const stereo::Vector3 worldPosition {
        stock.world.translation[0] + worldOffset.x,
        stock.world.translation[1] + worldOffset.y,
        stock.world.translation[2] + worldOffset.z,
    };
    destination = stock;
    destination.world = retailTransform(
        worldRotation,
        worldPosition,
        stock.world.scale);
    // NiCamera::Create returns a detached camera. Its local and world
    // transforms are therefore identical; parented private cameras reject.
    destination.local = destination.world;
    destination.frustum = frustum;
    return retailTransformUsable(destination.world)
        && retailFrustumUsable(destination.frustum)
        && buildRetailWorldToCamera(
            destination.world,
            destination.frustum,
            destination.worldToCamera);
}
}

inline RetailVrOriginCandidate prepareRetailVrOriginCandidate(
    const RetailVrOrigin& current,
    const RetailTrackedFrame& frame) noexcept
{
    RetailVrOriginCandidate result {};
    const RetailTrackedFrameValidation validation =
        validateRetailTrackedGameplayFrame(frame);
    if (!validation.complete())
    {
        result.failure = RetailEyeCameraFailure::PoseRejected;
        return result;
    }

    const bool namespaceChanged = !current.valid
        || current.producerEpoch != frame.pose.producerEpoch
        || current.referenceSpaceGeneration
            != frame.pose.referenceSpaceGeneration;
    const bool recenterChanged = current.valid
        && !namespaceChanged
        && current.recenterRequestId != frame.pose.recenterRequestId;
    const bool poseRegressed = current.valid
        && !namespaceChanged
        && frame.pose.frame < current.lastPoseFrame;
    const bool relatch = namespaceChanged || recenterChanged || poseRegressed;

    result.origin = current;
    if (relatch)
    {
        const stereo::Quaternion fallback = current.valid
            ? current.orientation
            : stereo::Quaternion {};
        result.origin.orientation = stereo::gravityAlignedYawOrientation(
            detail::poseQuaternion(frame.pose.hmdRot),
            fallback);
        result.origin.position = detail::eyeMidpoint(frame.pose);
    }
    result.origin.producerEpoch = frame.pose.producerEpoch;
    result.origin.referenceSpaceGeneration =
        frame.pose.referenceSpaceGeneration;
    result.origin.recenterRequestId = frame.pose.recenterRequestId;
    result.origin.lastPoseFrame = frame.pose.frame;
    result.origin.lastPoseSequence = frame.poseSequence;
    result.origin.valid = true;
    result.relatched = relatch;
    result.failure = RetailEyeCameraFailure::None;
    return result;
}

inline RetailDerivedEyeCameraRig deriveRetailEyeCameraRig(
    const abi::RetailNiCameraLayout* stockCamera,
    const RetailTrackedFrame& frame,
    const RetailVrOrigin& origin,
    float gameUnitsPerMeter) noexcept
{
    RetailDerivedEyeCameraRig result {};
    const RetailTrackedFrameValidation validation =
        validateRetailTrackedGameplayFrame(frame);
    if (!validation.complete())
        return result;
    if (!origin.valid
        || origin.producerEpoch != frame.pose.producerEpoch
        || origin.referenceSpaceGeneration
            != frame.pose.referenceSpaceGeneration)
    {
        result.failure = RetailEyeCameraFailure::OriginRejected;
        return result;
    }
    if (!std::isfinite(gameUnitsPerMeter)
        || gameUnitsPerMeter <= 0.0f
        || gameUnitsPerMeter > 10000.0f)
    {
        result.failure = RetailEyeCameraFailure::ScaleRejected;
        return result;
    }
    if (!stockCamera
        || detail::retailCameraReadPointer32(stockCamera, 0u) == 0u)
    {
        result.failure = RetailEyeCameraFailure::StockCameraRejected;
        return result;
    }

    const RetailCameraMutableState stock =
        detail::retailCameraMutableState(stockCamera);
    if (!detail::retailTransformUsable(stock.world)
        || !detail::retailFrustumUsable(stock.frustum))
    {
        result.failure = RetailEyeCameraFailure::StockCameraRejected;
        return result;
    }

    const stereo::Quaternion hmd = detail::poseQuaternion(frame.pose.hmdRot);
    const stereo::Vector3 hmdPosition = detail::posePosition(frame.pose.hmdPos);
    const stereo::Vector3 leftPosition =
        detail::posePosition(frame.pose.leftEyePos);
    const stereo::Vector3 rightPosition =
        detail::posePosition(frame.pose.rightEyePos);
    const stereo::EyeBaselineValidation baseline =
        stereo::validateEyeBaseline(
            hmd,
            leftPosition,
            rightPosition,
            hmdPosition);
    if (!baseline.valid)
    {
        result.failure = RetailEyeCameraFailure::EyeBaselineRejected;
        return result;
    }
    if (!stereo::openXrFovAnglesUsable(frame.pose.leftFov)
        || !stereo::openXrFovAnglesUsable(frame.pose.rightFov))
    {
        result.failure = RetailEyeCameraFailure::EyeFovRejected;
        return result;
    }

    const stereo::Vector3 midpoint = detail::eyeMidpoint(frame.pose);
    const stereo::Vector3 centerLocal = stereo::positionInOriginFrame(
        origin.orientation,
        origin.position,
        midpoint);
    const stereo::Vector3 leftLocal = stereo::positionInOriginFrame(
        origin.orientation,
        origin.position,
        leftPosition);
    const stereo::Vector3 rightLocal = stereo::positionInOriginFrame(
        origin.orientation,
        origin.position,
        rightPosition);
    const stereo::Matrix3 centerRotation = stereo::cameraLocalHeadRotation(
        stereo::relativeOrientation(origin.orientation, hmd));
    const stereo::Matrix3 leftRotation = stereo::cameraLocalHeadRotation(
        stereo::relativeOrientation(
            origin.orientation,
            detail::poseQuaternion(frame.pose.leftEyeRot)));
    const stereo::Matrix3 rightRotation = stereo::cameraLocalHeadRotation(
        stereo::relativeOrientation(
            origin.orientation,
            detail::poseQuaternion(frame.pose.rightEyeRot)));
    const stereo::Matrix3 bodyWorldRotation =
        stereo::gravityLevelCameraWorldRotation(
            detail::retailMatrix3(stock.world.rotation));

    const abi::RetailNiFrustumLayout leftFrustum = detail::eyeFrustum(
        stock.frustum,
        frame.pose.leftFov);
    const abi::RetailNiFrustumLayout rightFrustum = detail::eyeFrustum(
        stock.frustum,
        frame.pose.rightFov);
    stereo::EyeCullFrustum cullEyes[2] {};
    cullEyes[0].rotation = leftRotation;
    cullEyes[0].position = {
        leftLocal.x * gameUnitsPerMeter,
        leftLocal.y * gameUnitsPerMeter,
        leftLocal.z * gameUnitsPerMeter,
    };
    cullEyes[0].left = leftFrustum.left;
    cullEyes[0].right = leftFrustum.right;
    cullEyes[0].top = leftFrustum.top;
    cullEyes[0].bottom = leftFrustum.bottom;
    cullEyes[1].rotation = rightRotation;
    cullEyes[1].position = {
        rightLocal.x * gameUnitsPerMeter,
        rightLocal.y * gameUnitsPerMeter,
        rightLocal.z * gameUnitsPerMeter,
    };
    cullEyes[1].left = rightFrustum.left;
    cullEyes[1].right = rightFrustum.right;
    cullEyes[1].top = rightFrustum.top;
    cullEyes[1].bottom = rightFrustum.bottom;
    const stereo::Vector3 centerCullPosition {
        centerLocal.x * gameUnitsPerMeter,
        centerLocal.y * gameUnitsPerMeter,
        centerLocal.z * gameUnitsPerMeter,
    };
    const stereo::PerspectiveCullFrustum centerCull =
        stereo::conservativeCenterCullFrustum(
            centerRotation,
            centerCullPosition,
            cullEyes,
            stock.frustum.nearDistance,
            stock.frustum.farDistance);
    if (!centerCull.valid)
    {
        result.failure = RetailEyeCameraFailure::CenterCullRejected;
        return result;
    }
    abi::RetailNiFrustumLayout centerFrustum = stock.frustum;
    centerFrustum.left = centerCull.left;
    centerFrustum.right = centerCull.right;
    centerFrustum.top = centerCull.top;
    centerFrustum.bottom = centerCull.bottom;
    centerFrustum.nearDistance = centerCull.nearDistance;
    centerFrustum.farDistance = centerCull.farDistance;
    centerFrustum.orthographic = 0u;

    if (!detail::configureDerivedCamera(
            result.center,
            stock,
            bodyWorldRotation,
            centerRotation,
            centerLocal,
            gameUnitsPerMeter,
            centerFrustum)
        || !detail::configureDerivedCamera(
            result.left,
            stock,
            bodyWorldRotation,
            leftRotation,
            leftLocal,
            gameUnitsPerMeter,
            leftFrustum)
        || !detail::configureDerivedCamera(
            result.right,
            stock,
            bodyWorldRotation,
            rightRotation,
            rightLocal,
            gameUnitsPerMeter,
            rightFrustum))
    {
        result.failure = RetailEyeCameraFailure::MatrixRejected;
        return result;
    }
    result.failure = RetailEyeCameraFailure::None;
    return result;
}

class RetailPrivateEyeCameraPair final
{
public:
    RetailPrivateEyeCameraPair() noexcept = default;
    RetailPrivateEyeCameraPair(const RetailPrivateEyeCameraPair&) = delete;
    RetailPrivateEyeCameraPair& operator=(
        const RetailPrivateEyeCameraPair&) = delete;

    ~RetailPrivateEyeCameraPair() noexcept
    {
        reset();
    }

    bool initialize(
        const RetailEngineCalls& calls,
        abi::RetailNiCameraLayout* centerCamera) noexcept
    {
        if (mLeft || mRight || mRelease || !centerCamera
            || !calls.niCameraCreate || !calls.niRefObjectFree)
        {
            mFailure = RetailEyeCameraFailure::FactoryUnavailable;
            return false;
        }
        mRelease = calls.niRefObjectFree;
        mBorrowedCenter = centerCamera;
        mLeft = calls.niCameraCreate();
        if (!mLeft)
        {
            mFailure = RetailEyeCameraFailure::LeftFactoryReturnedNull;
            reset();
            return false;
        }
        mRight = calls.niCameraCreate();
        if (!mRight)
        {
            mFailure = RetailEyeCameraFailure::RightFactoryReturnedNull;
            reset();
            return false;
        }
        if (mLeft == mRight
            || mLeft == centerCamera
            || mRight == centerCamera)
        {
            mFailure = RetailEyeCameraFailure::CameraAlias;
            reset();
            return false;
        }
        mFailure = RetailEyeCameraFailure::None;
        return true;
    }

    bool valid() const noexcept
    {
        return mLeft && mRight && mLeft != mRight && mRelease
            && mFailure == RetailEyeCameraFailure::None;
    }

    abi::RetailNiCameraLayout* left() const noexcept
    {
        return mLeft;
    }

    abi::RetailNiCameraLayout* right() const noexcept
    {
        return mRight;
    }

    RetailEyeCameraFailure failure() const noexcept
    {
        return mFailure;
    }

private:
    void reset() noexcept
    {
        if (mRight && mRelease && mRight != mBorrowedCenter)
        {
            mRelease(reinterpret_cast<abi::RetailNiAccumulatorLayout*>(mRight));
        }
        if (mLeft
            && mRelease
            && mLeft != mBorrowedCenter
            && mLeft != mRight)
        {
            mRelease(reinterpret_cast<abi::RetailNiAccumulatorLayout*>(mLeft));
        }
        mRight = nullptr;
        mLeft = nullptr;
        mRelease = nullptr;
        mBorrowedCenter = nullptr;
    }

    abi::RetailNiCameraLayout* mLeft = nullptr;
    abi::RetailNiCameraLayout* mRight = nullptr;
    abi::RetailNiCameraLayout* mBorrowedCenter = nullptr;
    abi::NiRefObjectFreeFunction mRelease = nullptr;
    RetailEyeCameraFailure mFailure =
        RetailEyeCameraFailure::FactoryUnavailable;
};

class RetailScopedEyeCameraTransaction final
{
public:
    RetailScopedEyeCameraTransaction() noexcept = default;
    RetailScopedEyeCameraTransaction(
        const RetailScopedEyeCameraTransaction&) = delete;
    RetailScopedEyeCameraTransaction& operator=(
        const RetailScopedEyeCameraTransaction&) = delete;

    ~RetailScopedEyeCameraTransaction() noexcept
    {
        restore();
    }

    bool begin(
        const abi::RetailNiCameraLayout* stockCamera,
        const RetailPrivateCameraSet& cameras,
        const RetailDerivedEyeCameraRig& rig) noexcept
    {
        if (mActive || !rig.complete()
            || !detail::privateCameraSetUsable(cameras, stockCamera))
        {
            mFailure = RetailEyeCameraFailure::PrivateCameraRejected;
            return false;
        }
        mCameras = cameras;
        mSaved[0] = detail::retailCameraMutableState(cameras.center);
        mSaved[1] = detail::retailCameraMutableState(cameras.left);
        mSaved[2] = detail::retailCameraMutableState(cameras.right);
        detail::writeRetailCameraMutableState(cameras.center, rig.center);
        detail::writeRetailCameraMutableState(cameras.left, rig.left);
        detail::writeRetailCameraMutableState(cameras.right, rig.right);
        mActive = true;
        mFailure = RetailEyeCameraFailure::None;
        return true;
    }

    bool active() const noexcept
    {
        return mActive;
    }

    RetailEyeCameraFailure failure() const noexcept
    {
        return mFailure;
    }

    void restore() noexcept
    {
        if (!mActive)
            return;
        detail::writeRetailCameraMutableState(mCameras.right, mSaved[2]);
        detail::writeRetailCameraMutableState(mCameras.left, mSaved[1]);
        detail::writeRetailCameraMutableState(mCameras.center, mSaved[0]);
        mActive = false;
        mCameras = {};
    }

private:
    bool mActive = false;
    RetailPrivateCameraSet mCameras {};
    RetailCameraMutableState mSaved[3] {};
    RetailEyeCameraFailure mFailure =
        RetailEyeCameraFailure::PrivateCameraRejected;
};
}
