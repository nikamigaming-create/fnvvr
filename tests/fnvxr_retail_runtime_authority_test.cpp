#include "fnvxr_retail_runtime_authority.h"

#include <cstdlib>
#include <iostream>
#include <type_traits>

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

void testDefaultObjectsCannotAuthorizeAnything()
{
    const RetailRuntimeAuthorityBundle bundle {};
    require(!bundle.current(), "a default authority bundle was current");
    require(bundle.metadata().generation == 0u,
        "a default authority bundle had a generation");

    const RetailEngineCallResolution calls = resolveRetailEngineCalls(
        bundle.engineCalls(),
        SupportedImageBase);
    require(calls.failure == RetailEngineCallResolutionFailure::Unauthorized,
        "default engine-call authority was accepted");

    const StereoResourceOperations<4u> resourceOperations {};
    const StereoResourceConstructionParameters resourceParameters {};
    const auto resources = acquireCenterStereoResources<4u>(
        resourceOperations,
        bundle.stereoResources(),
        resourceParameters);
    require(resources.failure == StereoResourceFailure::Unauthorized,
        "default resource authority was accepted");

    const CenterRendererResult renderer = executeCenterRendererFrame(
        {},
        bundle.centerRenderer(),
        {});
    require(renderer.failure == CenterRendererFailure::Unauthorized,
        "default renderer authority was accepted");

    const RetailWorldHookInstallResult hook = installRetailWorldHook(
        bundle.worldHook(),
        {},
        {},
        static_cast<std::uint32_t>(WorldRenderAddress),
        1u);
    require(hook.failure == RetailWorldHookInstallFailure::Unauthorized,
        "default world-hook authority was accepted");
}

void testProductionDecisionRejectsThisTestExecutable()
{
    const RetailRuntimeAuthorityDecision decision =
        authorizeCurrentRetailRuntimeAtDecisionPoint();
    require(!decision.complete(),
        "the production authority admitted the unit-test executable");
    require(decision.failure != RetailRuntimeAuthorityFailure::None,
        "a rejected production decision reported success");
    require(!decision.authority.current(),
        "a rejected production decision leaked a current capability");

    if constexpr (!RetailRuntimeProductionAuthorizationAvailable)
    {
        require(
            decision.failure
                == RetailRuntimeAuthorityFailure::UnsupportedHostArchitecture,
            "the x64 production decision did not fail as unsupported");
        require(
            decision.revalidation.failure
                == abi::revalidation::RetailAbiRevalidationFailure::
                    UnsupportedHostArchitecture,
            "the x64 revalidator did not fail before issuance");
    }
}

#if defined(_M_IX86)
void testIsolatedCurrentProcessBundleIsAtomicAndAddressBound()
{
    constexpr std::uint64_t Generation = 0x1020304050607080ull;
    const RetailRuntimeAuthorityDecision decision =
        testing::issueCurrentProcessBoundRetailRuntimeAuthorityForTest(
            Generation);
    require(decision.complete(),
        "the isolated current-process authority seam did not issue");
    require(decision.authority.metadata().generation == Generation,
        "the authority generation changed during issuance");
    require(decision.authority.metadata().runtimeImageBase != 0u,
        "the authority omitted its runtime image base");
    require(decision.authority.metadata().processId != 0u,
        "the authority omitted its process identity");

    const std::uintptr_t expectedWorld =
        decision.authority.metadata().runtimeImageBase
        + (WorldRenderAddress - SupportedImageBase);
    require(
        decision.authority.metadata().runtimeWorldAddress
            == expectedWorld,
        "world-hook authority was not relocated from the admitted image");

    const RetailEngineCallResolution calls = resolveRetailEngineCalls(
        decision.authority.engineCalls(),
        decision.authority.metadata().runtimeImageBase);
    require(calls.complete(),
        "the jointly issued engine-call capability was rejected");
    const RetailEngineCallResolution wrongImageCalls = resolveRetailEngineCalls(
        decision.authority.engineCalls(),
        decision.authority.metadata().runtimeImageBase + 0x1000u);
    require(
        wrongImageCalls.failure
            == RetailEngineCallResolutionFailure::Unauthorized,
        "engine-call authority accepted a neighboring image base");

    const auto resources = acquireCenterStereoResources<4u>(
        {},
        decision.authority.stereoResources(),
        {});
    require(
        resources.failure == StereoResourceFailure::OperationTableIncomplete,
        "the jointly issued resource capability did not pass authorization");

    const CenterRendererResult renderer = executeCenterRendererFrame(
        {},
        decision.authority.centerRenderer(),
        {});
    require(renderer.failure == CenterRendererFailure::InvalidInput,
        "the jointly issued renderer capability did not pass authorization");

    const RetailWorldHookInstallResult correctAddress = installRetailWorldHook(
        decision.authority.worldHook(),
        {},
        {},
        decision.authority.metadata().runtimeWorldAddress,
        1u);
    require(
        correctAddress.failure
            == RetailWorldHookInstallFailure::MemoryOperationsIncomplete,
        "the exact relocated world address did not pass authorization");

    const RetailWorldHookInstallResult wrongAddress = installRetailWorldHook(
        decision.authority.worldHook(),
        {},
        {},
        decision.authority.metadata().runtimeWorldAddress + 1u,
        1u);
    require(wrongAddress.failure == RetailWorldHookInstallFailure::Unauthorized,
        "world-hook authority accepted a neighboring address");

    const RetailRuntimeAuthorityDecision zeroGeneration =
        testing::issueCurrentProcessBoundRetailRuntimeAuthorityForTest(0u);
    require(
        !zeroGeneration.complete()
            && zeroGeneration.failure
                == RetailRuntimeAuthorityFailure::GenerationUnavailable,
        "a zero-generation authority was issued");
}
#endif
}

int main()
{
    static_assert(
        RetailEngineCallProductionAuthorizationAvailable
        == RetailRuntimeProductionAuthorizationAvailable);
    static_assert(
        StereoResourceProductionAuthorizationAvailable
        == RetailRuntimeProductionAuthorizationAvailable);
    static_assert(
        CenterRendererProductionAuthorizationAvailable
        == RetailRuntimeProductionAuthorizationAvailable);
    static_assert(
        RetailWorldHookProductionAuthorizationAvailable
        == RetailRuntimeProductionAuthorizationAvailable);
    static_assert(
        !std::is_constructible_v<
            RetailEngineCallAuthorization,
            detail::RetailRuntimeBinding,
            detail::RetailRuntimeBindingValidator>);
    static_assert(
        !std::is_constructible_v<
            StereoResourceAuthorization,
            detail::RetailRuntimeBinding,
            detail::RetailRuntimeBindingValidator>);
    static_assert(
        !std::is_constructible_v<
            CenterRendererAuthorization,
            detail::RetailRuntimeBinding,
            detail::RetailRuntimeBindingValidator>);
    static_assert(
        !std::is_constructible_v<
            RetailWorldHookAuthorization,
            std::uint32_t,
            detail::RetailRuntimeBinding,
            detail::RetailRuntimeBindingValidator>);

    testDefaultObjectsCannotAuthorizeAnything();
    testProductionDecisionRejectsThisTestExecutable();
#if defined(_M_IX86)
    testIsolatedCurrentProcessBundleIsAtomicAndAddressBound();
#endif
    std::cout << "single retail runtime authority contract passed\n";
    return 0;
}
