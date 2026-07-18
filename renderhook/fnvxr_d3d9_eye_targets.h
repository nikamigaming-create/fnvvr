#pragma once

#include "../runtime/fnvxr_retail_center_renderer_operations.h"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace fnvxr::d3d9
{
enum class EyeTargetSurfaceIdentity : std::uint8_t
{
    Failure,
    Same,
    Distinct,
};

struct EyeTargetSurfaceDescription
{
    std::uint32_t width = 0u;
    std::uint32_t height = 0u;
    std::uint32_t format = 0u;
    std::uint32_t multisampleType = 0u;
    std::uint32_t multisampleQuality = 0u;
    bool renderTarget = false;
    bool depthStencil = false;
};

struct EyeTargetViewport
{
    std::uint32_t x = 0u;
    std::uint32_t y = 0u;
    std::uint32_t width = 0u;
    std::uint32_t height = 0u;
    float minimumDepth = 0.0f;
    float maximumDepth = 1.0f;
};

struct EyeTargetRect
{
    std::int32_t left = 0;
    std::int32_t top = 0;
    std::int32_t right = 0;
    std::int32_t bottom = 0;
};

constexpr bool operator==(
    const EyeTargetViewport& left,
    const EyeTargetViewport& right) noexcept
{
    return left.x == right.x
        && left.y == right.y
        && left.width == right.width
        && left.height == right.height
        && left.minimumDepth == right.minimumDepth
        && left.maximumDepth == right.maximumDepth;
}

constexpr bool operator==(
    const EyeTargetRect& left,
    const EyeTargetRect& right) noexcept
{
    return left.left == right.left
        && left.top == right.top
        && left.right == right.right
        && left.bottom == right.bottom;
}

// This narrow table keeps the ownership and state machine independently
// testable without asking tests to emulate the 119-method IDirect3DDevice9
// interface. GetRenderTarget/GetDepthStencilSurface return retained surfaces;
// releaseSurface balances each successful getter.
struct EyeTargetDeviceApi
{
    void* context = nullptr;
    bool (*describeSurface)(
        void*,
        void*,
        EyeTargetSurfaceDescription&) noexcept = nullptr;
    EyeTargetSurfaceIdentity (*compareSurfaceIdentity)(
        void*,
        void*,
        void*) noexcept = nullptr;
    bool (*getRenderTarget)(void*, void*, void*&) noexcept = nullptr;
    bool (*getDepthStencil)(void*, void*, void*&) noexcept = nullptr;
    bool (*getViewport)(void*, void*, EyeTargetViewport&) noexcept = nullptr;
    bool (*getScissorRect)(void*, void*, EyeTargetRect&) noexcept = nullptr;
    bool (*getScissorEnabled)(void*, void*, bool&) noexcept = nullptr;
    bool (*setRenderTarget)(void*, void*, void*) noexcept = nullptr;
    bool (*setDepthStencil)(void*, void*, void*) noexcept = nullptr;
    bool (*setViewport)(void*, void*, const EyeTargetViewport&) noexcept = nullptr;
    bool (*setScissorRect)(void*, void*, const EyeTargetRect&) noexcept = nullptr;
    bool (*setScissorEnabled)(void*, void*, bool) noexcept = nullptr;
    void (*releaseSurface)(void*, void*) noexcept = nullptr;
};

constexpr bool eyeTargetDeviceApiComplete(
    const EyeTargetDeviceApi& api) noexcept
{
    return api.describeSurface
        && api.compareSurfaceIdentity
        && api.getRenderTarget
        && api.getDepthStencil
        && api.getViewport
        && api.getScissorRect
        && api.getScissorEnabled
        && api.setRenderTarget
        && api.setDepthStencil
        && api.setViewport
        && api.setScissorRect
        && api.setScissorEnabled
        && api.releaseSurface;
}

struct EyeTargetResources
{
    void* device = nullptr;
    void* leftColor = nullptr;
    void* leftDepth = nullptr;
    void* rightColor = nullptr;
    void* rightDepth = nullptr;
};

class RetailEyeTargetContext;

namespace detail
{
struct RetailEyeTargetAdapter;
}

class RetailEyeTargetContext final
{
public:
    RetailEyeTargetContext() noexcept = default;
    RetailEyeTargetContext(const RetailEyeTargetContext&) = delete;
    RetailEyeTargetContext& operator=(const RetailEyeTargetContext&) = delete;

    bool initialize(
        const EyeTargetDeviceApi& api,
        const EyeTargetResources& resources) noexcept
    {
        if (mSnapshotActive || mRollbackRestoreSatisfied)
            return false;
        clearConfiguration();
        if (!eyeTargetDeviceApiComplete(api)
            || !resources.device
            || !resources.leftColor
            || !resources.leftDepth
            || !resources.rightColor
            || !resources.rightDepth)
        {
            return false;
        }

        void* surfaces[] {
            resources.leftColor,
            resources.leftDepth,
            resources.rightColor,
            resources.rightDepth,
        };
        for (std::size_t left = 0u; left < 4u; ++left)
        {
            for (std::size_t right = left + 1u; right < 4u; ++right)
            {
                if (api.compareSurfaceIdentity(
                        api.context,
                        surfaces[left],
                        surfaces[right])
                    != EyeTargetSurfaceIdentity::Distinct)
                {
                    return false;
                }
            }
        }

        EyeTargetSurfaceDescription leftColor {};
        EyeTargetSurfaceDescription leftDepth {};
        EyeTargetSurfaceDescription rightColor {};
        EyeTargetSurfaceDescription rightDepth {};
        if (!api.describeSurface(api.context, resources.leftColor, leftColor)
            || !api.describeSurface(api.context, resources.leftDepth, leftDepth)
            || !api.describeSurface(api.context, resources.rightColor, rightColor)
            || !api.describeSurface(api.context, resources.rightDepth, rightDepth)
            || !descriptionsFormStereoPair(
                leftColor,
                leftDepth,
                rightColor,
                rightDepth))
        {
            return false;
        }

        mApi = api;
        mResources = resources;
        mWidth = leftColor.width;
        mHeight = leftColor.height;
        mInitialized = true;
        return true;
    }

    bool ready() const noexcept
    {
        return mInitialized
            && eyeTargetDeviceApiComplete(mApi)
            && mResources.device
            && mResources.leftColor
            && mResources.leftDepth
            && mResources.rightColor
            && mResources.rightDepth
            && mWidth != 0u
            && mHeight != 0u;
    }

    std::uint32_t width() const noexcept { return mWidth; }
    std::uint32_t height() const noexcept { return mHeight; }

    // Device reset/loss cannot restore default-pool state. This drops any
    // retained snapshot references without issuing D3D calls and makes the
    // operation table inert until initialize succeeds with new resources.
    void abandonForDeviceLoss() noexcept
    {
        releaseSnapshotSurfaces();
        clearConfiguration();
    }

private:
    struct AuthoritativeSnapshot
    {
        void* color = nullptr;
        void* depth = nullptr;
        EyeTargetViewport viewport {};
        EyeTargetRect scissor {};
        bool scissorEnabled = false;
    };

    static bool colorDescriptionValid(
        const EyeTargetSurfaceDescription& description) noexcept
    {
        return description.width != 0u
            && description.height != 0u
            && description.width
                <= static_cast<std::uint32_t>(
                    (std::numeric_limits<std::int32_t>::max)())
            && description.height
                <= static_cast<std::uint32_t>(
                    (std::numeric_limits<std::int32_t>::max)())
            && description.renderTarget
            && !description.depthStencil;
    }

    static bool depthDescriptionValid(
        const EyeTargetSurfaceDescription& description) noexcept
    {
        return description.width != 0u
            && description.height != 0u
            && !description.renderTarget
            && description.depthStencil;
    }

    static bool dimensionsAndSamplingMatch(
        const EyeTargetSurfaceDescription& left,
        const EyeTargetSurfaceDescription& right) noexcept
    {
        return left.width == right.width
            && left.height == right.height
            && left.multisampleType == right.multisampleType
            && left.multisampleQuality == right.multisampleQuality;
    }

    static bool descriptionsFormStereoPair(
        const EyeTargetSurfaceDescription& leftColor,
        const EyeTargetSurfaceDescription& leftDepth,
        const EyeTargetSurfaceDescription& rightColor,
        const EyeTargetSurfaceDescription& rightDepth) noexcept
    {
        return colorDescriptionValid(leftColor)
            && colorDescriptionValid(rightColor)
            && depthDescriptionValid(leftDepth)
            && depthDescriptionValid(rightDepth)
            && leftColor.format == rightColor.format
            && leftDepth.format == rightDepth.format
            && dimensionsAndSamplingMatch(leftColor, rightColor)
            && dimensionsAndSamplingMatch(leftDepth, rightDepth)
            && dimensionsAndSamplingMatch(leftColor, leftDepth)
            && dimensionsAndSamplingMatch(rightColor, rightDepth);
    }

    void clearConfiguration() noexcept
    {
        mInitialized = false;
        mApi = {};
        mResources = {};
        mSnapshot = {};
        mSnapshotActive = false;
        mEyeActive = false;
        mActiveEye = engine::CenterRendererEye::Left;
        mActiveToken = 0u;
        mCompletedEyes = 0u;
        mRollbackRestoreSatisfied = false;
        mWidth = 0u;
        mHeight = 0u;
    }

    void releaseSnapshotSurfaces() noexcept
    {
        if (mApi.releaseSurface)
        {
            if (mSnapshot.color)
                mApi.releaseSurface(mApi.context, mSnapshot.color);
            if (mSnapshot.depth)
                mApi.releaseSurface(mApi.context, mSnapshot.depth);
        }
        mSnapshot.color = nullptr;
        mSnapshot.depth = nullptr;
    }

    bool surfaceIdentityIs(
        void* left,
        void* right,
        EyeTargetSurfaceIdentity expected) const noexcept
    {
        return left
            && right
            && mApi.compareSurfaceIdentity(mApi.context, left, right)
                == expected;
    }

    EyeTargetViewport fullViewport() const noexcept
    {
        return { 0u, 0u, mWidth, mHeight, 0.0f, 1.0f };
    }

    EyeTargetRect fullScissor() const noexcept
    {
        return {
            0,
            0,
            static_cast<std::int32_t>(mWidth),
            static_cast<std::int32_t>(mHeight),
        };
    }

    void* colorFor(engine::CenterRendererEye eye) const noexcept
    {
        return eye == engine::CenterRendererEye::Left
            ? mResources.leftColor
            : mResources.rightColor;
    }

    void* depthFor(engine::CenterRendererEye eye) const noexcept
    {
        return eye == engine::CenterRendererEye::Left
            ? mResources.leftDepth
            : mResources.rightDepth;
    }

    bool eyeOrderValid(engine::CenterRendererEye eye) const noexcept
    {
        return (eye == engine::CenterRendererEye::Left && mCompletedEyes == 0u)
            || (eye == engine::CenterRendererEye::Right
                && mCompletedEyes == 1u);
    }

    std::uintptr_t nextIsolationToken() noexcept
    {
        ++mNextToken;
        if (mNextToken == 0u)
            ++mNextToken;
        return mNextToken;
    }

    bool snapshot() noexcept
    {
        if (!ready()
            || mSnapshotActive
            || mEyeActive
            || mRollbackRestoreSatisfied)
        {
            return false;
        }

        AuthoritativeSnapshot candidate {};
        if (!mApi.getRenderTarget(
                mApi.context,
                mResources.device,
                candidate.color)
            || !candidate.color)
        {
            if (candidate.color)
                mApi.releaseSurface(mApi.context, candidate.color);
            return false;
        }
        if (!mApi.getDepthStencil(
                mApi.context,
                mResources.device,
                candidate.depth)
            || !candidate.depth)
        {
            mApi.releaseSurface(mApi.context, candidate.color);
            if (candidate.depth)
                mApi.releaseSurface(mApi.context, candidate.depth);
            return false;
        }

        const bool stateCaptured = mApi.getViewport(
                mApi.context,
                mResources.device,
                candidate.viewport)
            && mApi.getScissorRect(
                mApi.context,
                mResources.device,
                candidate.scissor)
            && mApi.getScissorEnabled(
                mApi.context,
                mResources.device,
                candidate.scissorEnabled);
        const bool startsOutsideEyeTargets =
            surfaceIdentityIs(
                candidate.color,
                mResources.leftColor,
                EyeTargetSurfaceIdentity::Distinct)
            && surfaceIdentityIs(
                candidate.color,
                mResources.rightColor,
                EyeTargetSurfaceIdentity::Distinct)
            && surfaceIdentityIs(
                candidate.depth,
                mResources.leftDepth,
                EyeTargetSurfaceIdentity::Distinct)
            && surfaceIdentityIs(
                candidate.depth,
                mResources.rightDepth,
                EyeTargetSurfaceIdentity::Distinct);
        if (!stateCaptured || !startsOutsideEyeTargets)
        {
            mApi.releaseSurface(mApi.context, candidate.color);
            mApi.releaseSurface(mApi.context, candidate.depth);
            return false;
        }

        mSnapshot = candidate;
        mSnapshotActive = true;
        mCompletedEyes = 0u;
        return true;
    }

    bool bind(
        engine::CenterRendererEye eye,
        engine::CenterRendererEyeIsolation& isolation) noexcept
    {
        if (!ready()
            || !mSnapshotActive
            || mEyeActive
            || isolation.active()
            || !eyeOrderValid(eye))
        {
            return false;
        }

        const bool targetSet = mApi.setRenderTarget(
            mApi.context,
            mResources.device,
            colorFor(eye));
        const bool depthSet = targetSet && mApi.setDepthStencil(
            mApi.context,
            mResources.device,
            depthFor(eye));
        const EyeTargetViewport viewport = fullViewport();
        const EyeTargetRect scissor = fullScissor();
        const bool viewportSet = depthSet && mApi.setViewport(
            mApi.context,
            mResources.device,
            viewport);
        const bool scissorSet = viewportSet && mApi.setScissorRect(
            mApi.context,
            mResources.device,
            scissor);
        const bool scissorDisabled = scissorSet && mApi.setScissorEnabled(
            mApi.context,
            mResources.device,
            false);
        if (!scissorDisabled)
            return false;

        mEyeActive = true;
        mActiveEye = eye;
        mActiveToken = nextIsolationToken();
        isolation.token = mActiveToken;
        return true;
    }

    bool end(
        engine::CenterRendererEye eye,
        engine::CenterRendererEyeIsolation& isolation) noexcept
    {
        if (!ready()
            || !mSnapshotActive
            || !mEyeActive
            || eye != mActiveEye
            || !isolation.active()
            || isolation.token != mActiveToken)
        {
            return false;
        }

        void* currentColor = nullptr;
        void* currentDepth = nullptr;
        EyeTargetViewport viewport {};
        EyeTargetRect scissor {};
        bool scissorEnabled = true;
        const bool colorCaptured = mApi.getRenderTarget(
            mApi.context,
            mResources.device,
            currentColor);
        const bool depthCaptured = mApi.getDepthStencil(
            mApi.context,
            mResources.device,
            currentDepth);
        const bool stateCaptured = colorCaptured
            && depthCaptured
            && mApi.getViewport(mApi.context, mResources.device, viewport)
            && mApi.getScissorRect(
                mApi.context,
                mResources.device,
                scissor)
            && mApi.getScissorEnabled(
                mApi.context,
                mResources.device,
                scissorEnabled);
        const bool exactEyeState = stateCaptured
            && surfaceIdentityIs(
                currentColor,
                colorFor(eye),
                EyeTargetSurfaceIdentity::Same)
            && surfaceIdentityIs(
                currentDepth,
                depthFor(eye),
                EyeTargetSurfaceIdentity::Same)
            && viewport == fullViewport()
            && scissor == fullScissor()
            && !scissorEnabled;
        if (currentColor)
            mApi.releaseSurface(mApi.context, currentColor);
        if (currentDepth)
            mApi.releaseSurface(mApi.context, currentDepth);
        if (!exactEyeState)
            return false;

        mCompletedEyes |= eye == engine::CenterRendererEye::Left ? 1u : 2u;
        mEyeActive = false;
        mActiveToken = 0u;
        isolation = {};
        return true;
    }

    bool restoreCapturedState() noexcept
    {
        if (!mSnapshotActive || !mSnapshot.color || !mSnapshot.depth)
            return false;

        bool restored = true;
        if (!mApi.setRenderTarget(
                mApi.context,
                mResources.device,
                mSnapshot.color))
        {
            restored = false;
        }
        if (!mApi.setDepthStencil(
                mApi.context,
                mResources.device,
                mSnapshot.depth))
        {
            restored = false;
        }
        if (!mApi.setViewport(
                mApi.context,
                mResources.device,
                mSnapshot.viewport))
        {
            restored = false;
        }
        if (!mApi.setScissorRect(
                mApi.context,
                mResources.device,
                mSnapshot.scissor))
        {
            restored = false;
        }
        if (!mApi.setScissorEnabled(
                mApi.context,
                mResources.device,
                mSnapshot.scissorEnabled))
        {
            restored = false;
        }
        if (!restored)
            return false;

        releaseSnapshotSurfaces();
        mSnapshot = {};
        mSnapshotActive = false;
        mEyeActive = false;
        mActiveToken = 0u;
        mCompletedEyes = 0u;
        return true;
    }

    void rollback(
        engine::CenterRendererEye,
        engine::CenterRendererEyeIsolation& isolation) noexcept
    {
        isolation = {};
        mEyeActive = false;
        mActiveToken = 0u;
        if (restoreCapturedState())
            mRollbackRestoreSatisfied = true;
    }

    bool restore() noexcept
    {
        if (mRollbackRestoreSatisfied)
        {
            mRollbackRestoreSatisfied = false;
            return true;
        }
        if (mEyeActive)
            return false;
        return restoreCapturedState();
    }

    bool mInitialized = false;
    EyeTargetDeviceApi mApi {};
    EyeTargetResources mResources {};
    AuthoritativeSnapshot mSnapshot {};
    bool mSnapshotActive = false;
    bool mEyeActive = false;
    engine::CenterRendererEye mActiveEye = engine::CenterRendererEye::Left;
    std::uintptr_t mActiveToken = 0u;
    std::uintptr_t mNextToken = 0u;
    std::uint8_t mCompletedEyes = 0u;
    bool mRollbackRestoreSatisfied = false;
    std::uint32_t mWidth = 0u;
    std::uint32_t mHeight = 0u;

    friend struct detail::RetailEyeTargetAdapter;
};

namespace detail
{
struct RetailEyeTargetAdapter
{
    static RetailEyeTargetContext* checked(void* opaque) noexcept
    {
        auto* context = static_cast<RetailEyeTargetContext*>(opaque);
        return context && context->ready() ? context : nullptr;
    }

    static bool snapshot(void* opaque) noexcept
    {
        RetailEyeTargetContext* context = checked(opaque);
        return context && context->snapshot();
    }

    static bool bind(
        void* opaque,
        engine::CenterRendererEye eye,
        engine::CenterRendererEyeIsolation& isolation) noexcept
    {
        RetailEyeTargetContext* context = checked(opaque);
        return context && context->bind(eye, isolation);
    }

    static bool end(
        void* opaque,
        engine::CenterRendererEye eye,
        engine::CenterRendererEyeIsolation& isolation) noexcept
    {
        RetailEyeTargetContext* context = checked(opaque);
        return context && context->end(eye, isolation);
    }

    static void rollback(
        void* opaque,
        engine::CenterRendererEye eye,
        engine::CenterRendererEyeIsolation& isolation) noexcept
    {
        RetailEyeTargetContext* context = checked(opaque);
        if (context)
            context->rollback(eye, isolation);
        else
            isolation = {};
    }

    static bool restore(void* opaque) noexcept
    {
        RetailEyeTargetContext* context = checked(opaque);
        return context && context->restore();
    }
};
}

inline engine::RetailEyeTargetOperations makeRetailEyeTargetOperations(
    RetailEyeTargetContext& context) noexcept
{
    return {
        &context,
        &detail::RetailEyeTargetAdapter::snapshot,
        &detail::RetailEyeTargetAdapter::bind,
        &detail::RetailEyeTargetAdapter::end,
        &detail::RetailEyeTargetAdapter::rollback,
        &detail::RetailEyeTargetAdapter::restore,
    };
}
}
