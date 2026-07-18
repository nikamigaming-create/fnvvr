#include "fnvxr_d3d9_eye_targets.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
using namespace fnvxr::d3d9;
using fnvxr::engine::CenterRendererEye;
using fnvxr::engine::CenterRendererEyeIsolation;

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

enum class SetCall : std::uint8_t
{
    Color,
    Depth,
    Viewport,
    Scissor,
    ScissorEnable,
};

struct FakeState
{
    int device = 0;
    int originalColor = 0;
    int originalDepth = 0;
    int leftColor = 0;
    int leftDepth = 0;
    int rightColor = 0;
    int rightDepth = 0;
    int unrelatedColor = 0;

    void* currentColor = &originalColor;
    void* currentDepth = &originalDepth;
    EyeTargetViewport viewport { 4u, 8u, 800u, 600u, 0.2f, 0.9f };
    EyeTargetRect scissor { 11, 12, 711, 512 };
    bool scissorEnabled = true;

    std::vector<SetCall> sets;
    int retainedReferences = 0;
    bool failGetDepth = false;
    bool failNextDepthSet = false;
    bool failIdentity = false;
    bool incompatibleRightDepth = false;
};

bool describe(
    void* opaque,
    void* surface,
    EyeTargetSurfaceDescription& description) noexcept
{
    auto* state = static_cast<FakeState*>(opaque);
    description = {};
    const bool color = surface == &state->leftColor
        || surface == &state->rightColor;
    const bool depth = surface == &state->leftDepth
        || surface == &state->rightDepth;
    if (!color && !depth)
        return false;
    description.width = 1024u;
    description.height = 768u;
    description.format = color ? 21u : 75u;
    description.multisampleType = 0u;
    description.multisampleQuality = 0u;
    description.renderTarget = color;
    description.depthStencil = depth;
    if (state->incompatibleRightDepth && surface == &state->rightDepth)
        description.width = 512u;
    return true;
}

EyeTargetSurfaceIdentity compare(
    void* opaque,
    void* left,
    void* right) noexcept
{
    const auto* state = static_cast<const FakeState*>(opaque);
    if (state->failIdentity || !left || !right)
        return EyeTargetSurfaceIdentity::Failure;
    return left == right
        ? EyeTargetSurfaceIdentity::Same
        : EyeTargetSurfaceIdentity::Distinct;
}

bool getColor(void* opaque, void*, void*& surface) noexcept
{
    auto* state = static_cast<FakeState*>(opaque);
    surface = state->currentColor;
    if (!surface)
        return false;
    ++state->retainedReferences;
    return true;
}

bool getDepth(void* opaque, void*, void*& surface) noexcept
{
    auto* state = static_cast<FakeState*>(opaque);
    if (state->failGetDepth)
    {
        surface = nullptr;
        return false;
    }
    surface = state->currentDepth;
    if (!surface)
        return false;
    ++state->retainedReferences;
    return true;
}

bool getViewport(
    void* opaque,
    void*,
    EyeTargetViewport& viewport) noexcept
{
    viewport = static_cast<FakeState*>(opaque)->viewport;
    return true;
}

bool getScissor(
    void* opaque,
    void*,
    EyeTargetRect& scissor) noexcept
{
    scissor = static_cast<FakeState*>(opaque)->scissor;
    return true;
}

bool getScissorEnable(void* opaque, void*, bool& enabled) noexcept
{
    enabled = static_cast<FakeState*>(opaque)->scissorEnabled;
    return true;
}

bool setColor(void* opaque, void*, void* surface) noexcept
{
    auto* state = static_cast<FakeState*>(opaque);
    state->sets.push_back(SetCall::Color);
    state->currentColor = surface;
    return surface != nullptr;
}

bool setDepth(void* opaque, void*, void* surface) noexcept
{
    auto* state = static_cast<FakeState*>(opaque);
    state->sets.push_back(SetCall::Depth);
    if (state->failNextDepthSet)
    {
        state->failNextDepthSet = false;
        return false;
    }
    state->currentDepth = surface;
    return surface != nullptr;
}

bool setViewport(
    void* opaque,
    void*,
    const EyeTargetViewport& viewport) noexcept
{
    auto* state = static_cast<FakeState*>(opaque);
    state->sets.push_back(SetCall::Viewport);
    state->viewport = viewport;
    return true;
}

bool setScissor(
    void* opaque,
    void*,
    const EyeTargetRect& scissor) noexcept
{
    auto* state = static_cast<FakeState*>(opaque);
    state->sets.push_back(SetCall::Scissor);
    state->scissor = scissor;
    return true;
}

bool setScissorEnable(void* opaque, void*, bool enabled) noexcept
{
    auto* state = static_cast<FakeState*>(opaque);
    state->sets.push_back(SetCall::ScissorEnable);
    state->scissorEnabled = enabled;
    return true;
}

void release(void* opaque, void* surface) noexcept
{
    auto* state = static_cast<FakeState*>(opaque);
    if (surface)
        --state->retainedReferences;
}

EyeTargetDeviceApi api(FakeState& state)
{
    return {
        &state,
        &describe,
        &compare,
        &getColor,
        &getDepth,
        &getViewport,
        &getScissor,
        &getScissorEnable,
        &setColor,
        &setDepth,
        &setViewport,
        &setScissor,
        &setScissorEnable,
        &release,
    };
}

EyeTargetResources resources(FakeState& state)
{
    return {
        &state.device,
        &state.leftColor,
        &state.leftDepth,
        &state.rightColor,
        &state.rightDepth,
    };
}

void requireOriginalState(const FakeState& state)
{
    require(state.currentColor == &state.originalColor,
        "the authoritative color target was not restored");
    require(state.currentDepth == &state.originalDepth,
        "the authoritative depth target was not restored");
    require(state.viewport
            == EyeTargetViewport { 4u, 8u, 800u, 600u, 0.2f, 0.9f },
        "the authoritative viewport was not restored");
    require(state.scissor == EyeTargetRect { 11, 12, 711, 512 },
        "the authoritative scissor was not restored");
    require(state.scissorEnabled,
        "the authoritative scissor-enable state was not restored");
}
}

int main()
{
    {
        FakeState state;
        RetailEyeTargetContext context;
        require(context.initialize(api(state), resources(state)),
            "four distinct compatible eye surfaces must initialize");
        require(context.ready() && context.width() == 1024u
                && context.height() == 768u,
            "the concrete target context did not retain exact dimensions");

        auto operations = makeRetailEyeTargetOperations(context);
        require(fnvxr::engine::retailEyeTargetOperationsComplete(operations),
            "the concrete D3D9 target operation table is incomplete");
        require(operations.snapshot(operations.context),
            "the authoritative D3D9 state did not snapshot");
        require(state.retainedReferences == 2,
            "snapshot must retain exactly color and depth");

        CenterRendererEyeIsolation isolation {};
        require(operations.bind(
                operations.context,
                CenterRendererEye::Left,
                isolation)
                && isolation.active(),
            "left eye color/depth did not bind atomically");
        const std::uintptr_t leftToken = isolation.token;
        require(state.currentColor == &state.leftColor
                && state.currentDepth == &state.leftDepth
                && state.viewport
                    == EyeTargetViewport { 0u, 0u, 1024u, 768u, 0.0f, 1.0f }
                && state.scissor == EyeTargetRect { 0, 0, 1024, 768 }
                && !state.scissorEnabled,
            "left eye did not receive full-size isolated color/depth state");
        require(operations.end(
                operations.context,
                CenterRendererEye::Left,
                isolation)
                && !isolation.active(),
            "left eye end did not prove the target remained bound");

        require(operations.bind(
                operations.context,
                CenterRendererEye::Right,
                isolation)
                && isolation.active()
                && isolation.token != leftToken,
            "right eye did not bind with a distinct isolation token");
        require(state.currentColor == &state.rightColor
                && state.currentDepth == &state.rightDepth,
            "right eye aliased the left eye target");
        require(operations.end(
                operations.context,
                CenterRendererEye::Right,
                isolation),
            "right eye end did not prove the target remained bound");
        require(operations.restore(operations.context),
            "the authoritative D3D9 state did not restore");
        requireOriginalState(state);
        require(state.retainedReferences == 0,
            "successful restore leaked retained D3D9 surfaces");

        const std::vector<SetCall> perBinding {
            SetCall::Color,
            SetCall::Depth,
            SetCall::Viewport,
            SetCall::Scissor,
            SetCall::ScissorEnable,
        };
        std::vector<SetCall> expected;
        expected.insert(expected.end(), perBinding.begin(), perBinding.end());
        expected.insert(expected.end(), perBinding.begin(), perBinding.end());
        expected.insert(expected.end(), perBinding.begin(), perBinding.end());
        require(state.sets == expected,
            "left, right, and restore did not use the exact target-state order");
    }

    {
        FakeState state;
        RetailEyeTargetContext context;
        EyeTargetResources aliased = resources(state);
        aliased.rightColor = aliased.leftColor;
        require(!context.initialize(api(state), aliased),
            "left/right color alias must be rejected");
        state.incompatibleRightDepth = true;
        require(!context.initialize(api(state), resources(state)),
            "incompatible right depth dimensions must be rejected");
    }

    {
        FakeState state;
        RetailEyeTargetContext context;
        require(context.initialize(api(state), resources(state)),
            "failure-path context did not initialize");
        auto operations = makeRetailEyeTargetOperations(context);
        state.failGetDepth = true;
        require(!operations.snapshot(operations.context),
            "partial snapshot must fail");
        require(state.retainedReferences == 0,
            "partial snapshot leaked its retained color target");
    }

    {
        FakeState state;
        RetailEyeTargetContext context;
        require(context.initialize(api(state), resources(state)),
            "partial-bind context did not initialize");
        auto operations = makeRetailEyeTargetOperations(context);
        require(operations.snapshot(operations.context),
            "partial-bind snapshot failed");
        state.failNextDepthSet = true;
        CenterRendererEyeIsolation isolation {};
        require(!operations.bind(
                operations.context,
                CenterRendererEye::Left,
                isolation)
                && !isolation.active(),
            "partial bind must fail without issuing an isolation token");
        require(operations.restore(operations.context),
            "partial bind did not restore the authoritative state");
        requireOriginalState(state);
        require(state.retainedReferences == 0,
            "partial bind restoration leaked snapshot references");
    }

    {
        FakeState state;
        RetailEyeTargetContext context;
        require(context.initialize(api(state), resources(state)),
            "rollback context did not initialize");
        auto operations = makeRetailEyeTargetOperations(context);
        require(operations.snapshot(operations.context),
            "rollback snapshot failed");
        CenterRendererEyeIsolation isolation {};
        require(operations.bind(
                operations.context,
                CenterRendererEye::Left,
                isolation),
            "rollback left-eye bind failed");
        state.currentColor = &state.unrelatedColor;
        require(!operations.end(
                operations.context,
                CenterRendererEye::Left,
                isolation)
                && isolation.active(),
            "end must reject a target redirected during engine rendering");
        operations.rollback(
            operations.context,
            CenterRendererEye::Left,
            isolation);
        require(!isolation.active(),
            "rollback did not revoke the active eye isolation token");
        requireOriginalState(state);
        require(operations.restore(operations.context),
            "restore must acknowledge a completed rollback exactly once");
        require(!operations.restore(operations.context),
            "a consumed rollback restoration must not be replayed");
        require(state.retainedReferences == 0,
            "rollback leaked authoritative surface references");

        context.abandonForDeviceLoss();
        require(!context.ready()
                && !operations.snapshot(operations.context),
            "device-loss invalidation left stale eye operations callable");
    }

    std::cout << "D3D9 retail eye target operations passed\n";
    return EXIT_SUCCESS;
}
