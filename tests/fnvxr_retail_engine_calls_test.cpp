#include "fnvxr_retail_engine_calls.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <type_traits>

namespace fnvxr::engine
{
struct RetailEngineCallResolverTestAuthority
{
    static RetailEngineCallAuthorization issue(
        const abi::RetailEngineAbiAssessment& assessment) noexcept
    {
        return RetailEngineCallAuthorization(assessment);
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

abi::RetailEngineAbiEvidence completeEvidence()
{
    abi::RetailEngineAbiEvidence evidence {};
    evidence.loadedExecutableIdentityMatched = true;
    evidence.loadedExecutableSectionLayoutAndProtectionsVerified = true;
    evidence.coreManifestMatched = true;
    evidence.fullFunctionInventoryMatched = true;
    evidence.vtableSlotsMatched = true;
    evidence.vtableBlocksMatched = true;
    evidence.liveObjectLayoutsVerified = true;
    evidence.constructorOwnershipVerified = true;
    evidence.bothWorldBranchesVerified = true;
    evidence.compatibilityModulesVerified = true;
    evidence.synchronousRuntimeRevalidation = true;
    return evidence;
}

template <typename Function>
std::uintptr_t functionAddress(Function function) noexcept
{
    return reinterpret_cast<std::uintptr_t>(function);
}

void requireEmpty(const RetailEngineCalls& calls, const char* message)
{
    require(calls.empty() && !calls.complete(), message);
}

void requireAddresses(
    const RetailEngineCalls& calls,
    std::uintptr_t loadedImageBase)
{
    const std::uintptr_t delta = loadedImageBase - SupportedImageBase;
    require(
        functionAddress(calls.niAllocate)
                == 0x00AA13E0u + delta
            && functionAddress(calls.niFree)
                == 0x00AA1460u + delta
            && functionAddress(calls.niCameraCreate)
                == 0x00A71430u + delta
            && functionAddress(calls.cullingProcessConstruct)
                == 0x004A0EB0u + delta
            && functionAddress(calls.cullingProcessDestroy)
                == 0x004A0F60u + delta
            && functionAddress(calls.shaderAccumulatorConstruct)
                == 0x00B660D0u + delta
            && functionAddress(calls.shaderAccumulatorDestroy)
                == 0x00B664F0u + delta
            && functionAddress(calls.niRefObjectFree)
                == 0x00CFCC20u + delta
            && functionAddress(calls.accumulatorSetCamera)
                == 0x00D47A40u + delta
            && functionAddress(calls.cullingProcessAlt)
                == 0x00C4F070u + delta
            && functionAddress(calls.accumulatorAddVisibleArray)
                == 0x00A9B790u + delta
            && functionAddress(calls.renderAccumulatorWithoutFinalize)
                == 0x00B6BA20u + delta
            && functionAddress(calls.finalizeAccumulator)
                == 0x00B6B930u + delta,
        "every resolved engine call must use its exact ASLR-relocated address");
}
}

int main()
{
    using namespace fnvxr::engine;

    static_assert(!RetailEngineCallProductionAuthorizationAvailable);
    static_assert(std::is_default_constructible_v<RetailEngineCallAuthorization>);
    static_assert(
        !std::is_constructible_v<
            RetailEngineCallAuthorization,
            bool>);
    static_assert(
        !std::is_constructible_v<
            RetailEngineCallAuthorization,
            abi::RetailEngineAbiAssessment>);

    static_assert(RetailEngineCallPreferredAddresses.niAllocate == 0x00AA13E0u);
    static_assert(RetailEngineCallPreferredAddresses.niFree == 0x00AA1460u);
    static_assert(RetailEngineCallPreferredAddresses.niCameraCreate == 0x00A71430u);
    static_assert(
        RetailEngineCallPreferredAddresses.cullingProcessConstruct
        == 0x004A0EB0u);
    static_assert(
        RetailEngineCallPreferredAddresses.cullingProcessDestroy
        == 0x004A0F60u);
    static_assert(
        RetailEngineCallPreferredAddresses.shaderAccumulatorConstruct
        == 0x00B660D0u);
    static_assert(
        RetailEngineCallPreferredAddresses.shaderAccumulatorDestroy
        == 0x00B664F0u);
    static_assert(
        RetailEngineCallPreferredAddresses.niRefObjectFree
        == 0x00CFCC20u);
    static_assert(
        RetailEngineCallPreferredAddresses.accumulatorSetCamera
        == 0x00D47A40u);
    static_assert(
        RetailEngineCallPreferredAddresses.cullingProcessAlt
        == 0x00C4F070u);
    static_assert(
        RetailEngineCallPreferredAddresses.accumulatorAddVisibleArray
        == 0x00A9B790u);
    static_assert(
        RetailEngineCallPreferredAddresses.renderAccumulatorWithoutFinalize
        == 0x00B6BA20u);
    static_assert(
        RetailEngineCallPreferredAddresses.finalizeAccumulator
        == 0x00B6B930u);
    static_assert(retailEngineCallInventoryComplete());

    const RetailEngineCallResolution defaultAuthorization =
        resolveRetailEngineCalls({}, SupportedImageBase);
    require(
        defaultAuthorization.failure
            == RetailEngineCallResolutionFailure::Unauthorized,
        "default authorization must fail before address resolution");
    requireEmpty(
        defaultAuthorization.calls,
        "an unauthorized result must contain no callable pointers");

    const abi::RetailEngineAbiAssessment incompleteAssessment =
        abi::assessRetailEngineAbi({});
    const RetailEngineCallAuthorization incompleteAuthorization =
        RetailEngineCallResolverTestAuthority::issue(incompleteAssessment);
    const RetailEngineCallResolution incompleteResult =
        resolveRetailEngineCalls(incompleteAuthorization, SupportedImageBase);
    require(
        incompleteResult.failure
            == RetailEngineCallResolutionFailure::Unauthorized,
        "incomplete ABI evidence must not authorize the resolver");
    requireEmpty(
        incompleteResult.calls,
        "incomplete ABI evidence must not expose partial call pointers");

    const abi::RetailEngineAbiAssessment inconsistentAssessment {
        true,
        abi::RetailEngineAbiFailure::RuntimeFunctionInventoryUnverified,
    };
    const RetailEngineCallResolution inconsistentResult =
        resolveRetailEngineCalls(
            RetailEngineCallResolverTestAuthority::issue(
                inconsistentAssessment),
            SupportedImageBase);
    require(
        inconsistentResult.failure
            == RetailEngineCallResolutionFailure::Unauthorized,
        "an authorized bit with a non-success ABI failure must fail closed");
    requireEmpty(
        inconsistentResult.calls,
        "inconsistent ABI evidence must expose no callable pointers");

    const abi::RetailEngineAbiAssessment falseSuccessAssessment {
        false,
        abi::RetailEngineAbiFailure::None,
    };
    const RetailEngineCallResolution falseSuccessResult =
        resolveRetailEngineCalls(
            RetailEngineCallResolverTestAuthority::issue(
                falseSuccessAssessment),
            SupportedImageBase);
    require(
        falseSuccessResult.failure
            == RetailEngineCallResolutionFailure::Unauthorized,
        "a success failure-code without authorization must fail closed");
    requireEmpty(
        falseSuccessResult.calls,
        "a false authorization bit must expose no callable pointers");

    const abi::RetailEngineAbiAssessment completeAssessment =
        abi::assessRetailEngineAbi(completeEvidence());
    require(
        completeAssessment.engineCallsAuthorized
            && completeAssessment.failure == abi::RetailEngineAbiFailure::None,
        "the test prerequisite must be a fully authorized ABI assessment");
    const RetailEngineCallAuthorization authorization =
        RetailEngineCallResolverTestAuthority::issue(completeAssessment);

    constexpr std::uintptr_t RelocatedBase = 0x10000000u;
    const RetailEngineAddressRelocation preferredRelocation =
        relocateRetailEngineAddress(
            SupportedImageBase,
            RetailEngineCallPreferredAddresses.niAllocate);
    require(
        preferredRelocation.complete()
            && preferredRelocation.address == 0x00AA13E0u,
        "the preferred image base must preserve the preferred call address");

    const RetailEngineAddressRelocation relocated =
        relocateRetailEngineAddress(
            RelocatedBase,
            RetailEngineCallPreferredAddresses.finalizeAccumulator);
    require(
        relocated.complete()
            && relocated.address
                == RelocatedBase + (0x00B6B930u - SupportedImageBase),
        "ASLR relocation must preserve the preferred-image offset");

    const std::array<std::uintptr_t, 13> everyPreferredAddress {{
        RetailEngineCallPreferredAddresses.niAllocate,
        RetailEngineCallPreferredAddresses.niFree,
        RetailEngineCallPreferredAddresses.niCameraCreate,
        RetailEngineCallPreferredAddresses.cullingProcessConstruct,
        RetailEngineCallPreferredAddresses.cullingProcessDestroy,
        RetailEngineCallPreferredAddresses.shaderAccumulatorConstruct,
        RetailEngineCallPreferredAddresses.shaderAccumulatorDestroy,
        RetailEngineCallPreferredAddresses.niRefObjectFree,
        RetailEngineCallPreferredAddresses.accumulatorSetCamera,
        RetailEngineCallPreferredAddresses.cullingProcessAlt,
        RetailEngineCallPreferredAddresses.accumulatorAddVisibleArray,
        RetailEngineCallPreferredAddresses.renderAccumulatorWithoutFinalize,
        RetailEngineCallPreferredAddresses.finalizeAccumulator,
    }};
    for (const std::uintptr_t preferredAddress : everyPreferredAddress)
    {
        const RetailEngineAddressRelocation each =
            relocateRetailEngineAddress(RelocatedBase, preferredAddress);
        require(
            each.complete()
                && each.address
                    == RelocatedBase
                        + (preferredAddress - SupportedImageBase),
            "every required call must preserve its image-relative offset");
    }

    require(
        relocateRetailEngineAddress(0u, 0x00AA13E0u).failure
            == RetailEngineAddressRelocationFailure::InvalidModuleBase,
        "a null module base must fail closed");
    require(
        relocateRetailEngineAddress(
            SupportedImageBase + 1u,
            0x00AA13E0u).failure
            == RetailEngineAddressRelocationFailure::InvalidModuleBase,
        "a non-allocation-aligned image base must fail closed");
    require(
        relocateRetailEngineAddress(
            SupportedImageBase,
            SupportedImageBase - 1u).failure
            == RetailEngineAddressRelocationFailure::PreferredAddressOutsideImage,
        "a preferred address below the supported image must fail closed");
    require(
        relocateRetailEngineAddress(
            SupportedImageBase,
            SupportedImageBase + SupportedSizeOfImage).failure
            == RetailEngineAddressRelocationFailure::PreferredAddressOutsideImage,
        "a preferred address at the image end must fail closed");

    constexpr std::uintptr_t OverflowBase =
        std::numeric_limits<std::uintptr_t>::max()
        & ~std::uintptr_t { 0xFFFFu };
    require(
        relocateRetailEngineAddress(
            OverflowBase,
            RetailEngineCallPreferredAddresses.finalizeAccumulator).failure
            == RetailEngineAddressRelocationFailure::AddressOverflow,
        "relocation arithmetic overflow must fail closed");

    if constexpr (!RetailEngineCallArchitectureSupported)
    {
        const RetailEngineCallResolution result =
            resolveRetailEngineCalls(authorization, SupportedImageBase);
        require(
            result.failure
                == RetailEngineCallResolutionFailure::UnsupportedArchitecture,
            "the live resolver must reject every non-x86 process");
        requireEmpty(
            result.calls,
            "x64 architecture rejection must expose no callable pointers");
    }
    else
    {
        const RetailEngineCallResolution preferred =
            resolveRetailEngineCalls(authorization, SupportedImageBase);
        require(
            preferred.complete(),
            "a fully authorized x86 preferred-base resolution must complete");
        requireAddresses(preferred.calls, SupportedImageBase);

        const RetailEngineCallResolution aslr =
            resolveRetailEngineCalls(authorization, RelocatedBase);
        require(
            aslr.complete(),
            "a fully authorized x86 ASLR resolution must complete");
        requireAddresses(aslr.calls, RelocatedBase);

        const RetailEngineCallResolution invalidBase =
            resolveRetailEngineCalls(
                authorization,
                SupportedImageBase + 1u);
        require(
            invalidBase.failure
                == RetailEngineCallResolutionFailure::InvalidModuleBase,
            "the x86 resolver must reject an invalid loaded module base");
        requireEmpty(
            invalidBase.calls,
            "invalid-base rejection must expose no callable pointers");

        const RetailEngineCallResolution overflow =
            resolveRetailEngineCalls(authorization, OverflowBase);
        require(
            overflow.failure
                == RetailEngineCallResolutionFailure::AddressOverflow,
            "the x86 resolver must reject overflowing module relocation");
        requireEmpty(
            overflow.calls,
            "overflow rejection must expose no callable pointers");
    }

    std::cout << "retail x86 engine call resolver tests passed\n";
    return EXIT_SUCCESS;
}
