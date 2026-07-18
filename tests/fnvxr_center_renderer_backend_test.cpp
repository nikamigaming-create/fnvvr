#include "fnvxr_center_renderer_backend.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace fnvxr::engine
{
struct CenterRendererLifecycleTestAuthority
{
    static CenterRendererAuthorization issue(const bool& evidence) noexcept
    {
        return CenterRendererAuthorization(&evidence, validate);
    }

private:
    static bool validate(const void* evidence) noexcept
    {
        return evidence && *static_cast<const bool*>(evidence);
    }
};
}

namespace
{
using namespace fnvxr::engine;

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

struct FakeContext
{
    std::vector<std::string> calls;
    const abi::RetailNiVisibleArrayLayout* visiblePointers[2] {};
    abi::RetailBSShaderAccumulatorLayout* accumulators[2] {};
    abi::RetailNiCameraLayout* collectedCamera = nullptr;
    abi::RetailNiCameraLayout* cameras[2] {};
    bool failRightRender = false;
    bool restoreSucceeds = true;
};

const char* eyeName(CenterRendererEye eye)
{
    return eye == CenterRendererEye::Left ? "left" : "right";
}

bool snapshot(void* raw) noexcept
{
    static_cast<FakeContext*>(raw)->calls.emplace_back("snapshot");
    return true;
}

bool collect(
    void* raw,
    abi::RetailNiCameraLayout* camera,
    void*,
    abi::RetailBSCullingProcessLayout*,
    std::uint64_t generation,
    CenterRendererVisibleSet& result) noexcept
{
    auto& context = *static_cast<FakeContext*>(raw);
    context.calls.emplace_back("collect");
    context.collectedCamera = camera;
    result.array.geometryPointers = 0x1000u;
    result.array.itemCount = 3u;
    result.array.capacity = 3u;
    result.array.growBy = 0u;
    result.generation = generation;
    return true;
}

bool bind(
    void* raw,
    CenterRendererEye eye,
    CenterRendererEyeIsolation& isolation) noexcept
{
    auto& context = *static_cast<FakeContext*>(raw);
    context.calls.emplace_back(std::string("bind-") + eyeName(eye));
    isolation.token = eye == CenterRendererEye::Left ? 1u : 2u;
    return true;
}

bool setCamera(
    void* raw,
    abi::RetailBSShaderAccumulatorLayout* accumulator,
    abi::RetailNiCameraLayout* camera) noexcept
{
    auto& context = *static_cast<FakeContext*>(raw);
    const int eye = context.accumulators[0] ? 1 : 0;
    context.accumulators[eye] = accumulator;
    context.cameras[eye] = camera;
    context.calls.emplace_back(eye == 0 ? "camera-left" : "camera-right");
    return true;
}

bool addVisible(
    void* raw,
    abi::RetailBSShaderAccumulatorLayout* accumulator,
    const abi::RetailNiVisibleArrayLayout* visible) noexcept
{
    auto& context = *static_cast<FakeContext*>(raw);
    const int eye = accumulator == context.accumulators[0] ? 0 : 1;
    context.visiblePointers[eye] = visible;
    context.calls.emplace_back(eye == 0 ? "add-left" : "add-right");
    return true;
}

bool render(
    void* raw,
    abi::RetailNiCameraLayout* camera,
    abi::RetailBSShaderAccumulatorLayout* accumulator,
    std::uint32_t renderContext) noexcept
{
    auto& context = *static_cast<FakeContext*>(raw);
    const bool right = accumulator == context.accumulators[1];
    if (camera != context.cameras[right ? 1 : 0])
        return false;
    context.calls.emplace_back(right ? "render-right" : "render-left");
    return renderContext == RetailWorldRenderContext
        && !(right && context.failRightRender);
}

bool finalize(
    void* raw,
    abi::RetailNiCameraLayout* camera,
    abi::RetailBSShaderAccumulatorLayout* accumulator,
    std::uint32_t renderContext) noexcept
{
    auto& context = *static_cast<FakeContext*>(raw);
    const bool right = accumulator == context.accumulators[1];
    if (camera != context.cameras[right ? 1 : 0])
        return false;
    context.calls.emplace_back(
        right
            ? "finalize-right"
            : "finalize-left");
    return renderContext == RetailWorldRenderContext;
}

bool end(
    void* raw,
    CenterRendererEye eye,
    CenterRendererEyeIsolation& isolation) noexcept
{
    auto& context = *static_cast<FakeContext*>(raw);
    context.calls.emplace_back(std::string("end-") + eyeName(eye));
    isolation.token = 0u;
    return true;
}

void rollback(
    void* raw,
    CenterRendererEye eye,
    CenterRendererEyeIsolation& isolation) noexcept
{
    auto& context = *static_cast<FakeContext*>(raw);
    context.calls.emplace_back(std::string("rollback-") + eyeName(eye));
    isolation.token = 0u;
}

bool restore(void* raw) noexcept
{
    auto& context = *static_cast<FakeContext*>(raw);
    context.calls.emplace_back("restore");
    return context.restoreSucceeds;
}

void discard(void* raw, CenterRendererVisibleSet& visible) noexcept
{
    static_cast<FakeContext*>(raw)->calls.emplace_back("discard");
    visible = {};
}

CenterRendererOperations operations(FakeContext& context)
{
    return {
        &context,
        snapshot,
        collect,
        bind,
        setCamera,
        addVisible,
        render,
        finalize,
        end,
        rollback,
        restore,
        discard,
    };
}

CenterRendererFrameInput input()
{
    static abi::RetailNiCameraLayout centerCamera {};
    static abi::RetailNiCameraLayout leftCamera {};
    static abi::RetailNiCameraLayout rightCamera {};
    static abi::RetailBSCullingProcessLayout culler {};
    static abi::RetailBSShaderAccumulatorLayout left {};
    static abi::RetailBSShaderAccumulatorLayout right {};
    static std::uint8_t scene = 0u;
    return {
        &scene,
        &centerCamera,
        &leftCamera,
        &rightCamera,
        &culler,
        &left,
        &right,
        77u,
    };
}
}

int main()
{
    using namespace fnvxr::engine;

    static_assert(RetailWorldCullerConstructorArgument == 0u);
    static_assert(RetailWorldAccumulatorConstructorMode == 0x63u);
    static_assert(RetailWorldAccumulatorBatchRendererCount == 1u);
    static_assert(RetailWorldAccumulatorMaximumPassCount == 0x2F7u);
    static_assert(
        CenterRendererProductionAuthorizationAvailable
        == RetailRuntimeProductionAuthorizationAvailable);

    FakeContext unauthorizedContext;
    const CenterRendererResult unauthorized = executeCenterRendererFrame(
        operations(unauthorizedContext),
        {},
        input());
    require(
        unauthorized.failure == CenterRendererFailure::Unauthorized
            && unauthorizedContext.calls.empty(),
        "production default authorization must reject before operations");

    const bool evidence = true;
    const CenterRendererAuthorization authorization =
        CenterRendererLifecycleTestAuthority::issue(evidence);
    FakeContext successContext;
    const CenterRendererResult success = executeCenterRendererFrame(
        operations(successContext),
        authorization,
        input());
    require(success.complete, "center-cull/distinct-eye frame must complete");
    require(
        success.visibleSetGeneration == 77u
            && success.visibleGeometryCount == 3u,
        "complete result must report the sealed visibility identity");
    require(
        successContext.visiblePointers[0]
            && successContext.visiblePointers[0]
                == successContext.visiblePointers[1],
        "both eyes must consume the exact same visible-array object");
    const CenterRendererFrameInput successfulInput = input();
    require(
        successContext.collectedCamera == successfulInput.centerCamera
            && successContext.cameras[0] == successfulInput.leftCamera
            && successContext.cameras[1] == successfulInput.rightCamera,
        "center cull and eye renders must receive their exact camera objects");
    const std::vector<std::string> expected {
        "snapshot",
        "collect",
        "bind-left",
        "camera-left",
        "add-left",
        "render-left",
        "finalize-left",
        "end-left",
        "bind-right",
        "camera-right",
        "add-right",
        "render-right",
        "finalize-right",
        "end-right",
        "restore",
        "discard",
    };
    require(
        successContext.calls == expected,
        "target binding must precede AddVisibleArray for both ordered eyes");

    FakeContext failureContext;
    failureContext.failRightRender = true;
    const CenterRendererResult failure = executeCenterRendererFrame(
        operations(failureContext),
        authorization,
        input());
    require(
        failure.failure == CenterRendererFailure::RightRender,
        "right render failure must be exact");
    require(
        failureContext.calls.size() >= 3u
            && failureContext.calls[failureContext.calls.size() - 3u]
                == "rollback-right"
            && failureContext.calls[failureContext.calls.size() - 2u]
                == "restore"
            && failureContext.calls.back() == "discard",
        "an active right-eye failure must roll back, restore, and discard");

    CenterRendererFrameInput aliased = input();
    aliased.rightAccumulator = aliased.leftAccumulator;
    FakeContext aliasContext;
    const CenterRendererResult aliasResult = executeCenterRendererFrame(
        operations(aliasContext),
        authorization,
        aliased);
    require(
        aliasResult.failure == CenterRendererFailure::InvalidInput
            && aliasContext.calls.empty(),
        "aliased eye accumulators must reject before snapshot");

    CenterRendererFrameInput aliasedCameras = input();
    aliasedCameras.rightCamera = aliasedCameras.leftCamera;
    FakeContext cameraAliasContext;
    const CenterRendererResult cameraAliasResult = executeCenterRendererFrame(
        operations(cameraAliasContext),
        authorization,
        aliasedCameras);
    require(
        cameraAliasResult.failure == CenterRendererFailure::InvalidInput
            && cameraAliasContext.calls.empty(),
        "aliased eye cameras must reject before snapshot");

    std::cout << "fnvxr center renderer backend tests passed\n";
    return EXIT_SUCCESS;
}
