#include "fnvxr_retail_runtime_authority.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <atomic>
#include <limits>

namespace fnvxr::engine
{
namespace
{
constexpr std::uint64_t BindingSealSalt = 0x63D51A97E2B40C8Full;

std::uint64_t mixBindingWord(
    std::uint64_t accumulator,
    std::uint64_t value) noexcept
{
    accumulator ^= value + 0x9E3779B97F4A7C15ull
        + (accumulator << 6u) + (accumulator >> 2u);
    accumulator ^= accumulator >> 29u;
    accumulator *= 0xBF58476D1CE4E5B9ull;
    accumulator ^= accumulator >> 31u;
    return accumulator;
}

std::uint64_t sealBinding(
    const detail::RetailRuntimeBinding& binding) noexcept
{
    std::uint64_t seal = BindingSealSalt;
    seal = mixBindingWord(
        seal,
        static_cast<std::uint64_t>(binding.runtimeImageBase));
    seal = mixBindingWord(seal, binding.processId);
    seal = mixBindingWord(seal, binding.processCreationTime100ns);
    seal = mixBindingWord(seal, binding.generation);
    return seal ? seal : BindingSealSalt;
}

bool currentProcessIdentity(
    std::uint32_t& processId,
    std::uint64_t& creationTime100ns) noexcept
{
    processId = 0u;
    creationTime100ns = 0u;
    FILETIME creation {};
    FILETIME exit {};
    FILETIME kernel {};
    FILETIME user {};
    if (!GetProcessTimes(
            GetCurrentProcess(),
            &creation,
            &exit,
            &kernel,
            &user))
    {
        return false;
    }
    ULARGE_INTEGER value {};
    value.LowPart = creation.dwLowDateTime;
    value.HighPart = creation.dwHighDateTime;
    processId = GetCurrentProcessId();
    creationTime100ns = value.QuadPart;
    return processId != 0u && creationTime100ns != 0u;
}

bool validateCurrentBinding(
    const detail::RetailRuntimeBinding& binding) noexcept
{
    if (!binding.runtimeImageBase
        || !binding.processId
        || !binding.processCreationTime100ns
        || !binding.generation
        || !binding.seal
        || binding.seal != sealBinding(binding))
    {
        return false;
    }

    std::uint32_t processId = 0u;
    std::uint64_t creationTime100ns = 0u;
    const HMODULE mainModule = GetModuleHandleW(nullptr);
    return currentProcessIdentity(processId, creationTime100ns)
        && mainModule
        && processId == binding.processId
        && creationTime100ns == binding.processCreationTime100ns
        && reinterpret_cast<std::uintptr_t>(mainModule)
            == binding.runtimeImageBase;
}

bool relocateWorldAddress(
    std::uintptr_t runtimeImageBase,
    std::uint32_t& runtimeWorldAddress) noexcept
{
    runtimeWorldAddress = 0u;
    const RetailEngineAddressRelocation relocated =
        relocateRetailEngineAddress(runtimeImageBase, WorldRenderAddress);
    if (!relocated.complete()
        || relocated.address
            > (std::numeric_limits<std::uint32_t>::max)())
    {
        return false;
    }
    runtimeWorldAddress = static_cast<std::uint32_t>(relocated.address);
    return runtimeWorldAddress != 0u;
}

std::uint64_t nextGeneration() noexcept
{
    static std::atomic<std::uint64_t> generation { 1u };
    for (;;)
    {
        const std::uint64_t value = generation.fetch_add(
            1u,
            std::memory_order_relaxed);
        if (value != 0u)
            return value;
    }
}

RetailRuntimeAuthorityFailure mapRevalidationFailure(
    const abi::revalidation::RetailAbiRevalidationResult& result) noexcept
{
    using RevalidationFailure =
        abi::revalidation::RetailAbiRevalidationFailure;
    if (result.failure == RevalidationFailure::UnsupportedHostArchitecture)
        return RetailRuntimeAuthorityFailure::UnsupportedHostArchitecture;
    if (result.failure == RevalidationFailure::CompatibilityProofRejected)
    {
        return RetailRuntimeAuthorityFailure::CompatibilityProofRejected;
    }
    return RetailRuntimeAuthorityFailure::AbiRevalidationRejected;
}
}

namespace detail
{
struct RetailRuntimeAuthorityIssuer
{
    static RetailRuntimeAuthorityBundle issue(
        const RetailRuntimeBinding& binding,
        const RetailRuntimeAuthorityMetadata& metadata,
        const abi::RetailEngineAbiAssessment& assessment) noexcept
    {
        const RetailEngineCallAuthorization calls(
            assessment,
            binding,
            &validateCurrentBinding);
        const StereoResourceAuthorization resources(
            binding,
            &validateCurrentBinding);
        const CenterRendererAuthorization renderer(
            binding,
            &validateCurrentBinding);
        const RetailWorldHookAuthorization hook(
            metadata.runtimeWorldAddress,
            binding,
            &validateCurrentBinding);
        return RetailRuntimeAuthorityBundle(
            binding,
            &validateCurrentBinding,
            metadata,
            calls,
            resources,
            renderer,
            hook);
    }
};
}

bool RetailRuntimeAuthorityBundle::current() const noexcept
{
    return mBindingValidator
        && mMetadata.runtimeImageBase == mBinding.runtimeImageBase
        && mMetadata.processId == mBinding.processId
        && mMetadata.processCreationTime100ns
            == mBinding.processCreationTime100ns
        && mMetadata.generation == mBinding.generation
        && mMetadata.runtimeWorldAddress != 0u
        && mBindingValidator(mBinding);
}

RetailRuntimeAuthorityDecision authorizeCurrentRetailRuntimeAtDecisionPoint()
    noexcept
{
    RetailRuntimeAuthorityDecision decision {};
    decision.revalidation =
        abi::revalidation::revalidateCurrentRetailEngineAbiAtDecisionPoint();
    if (!decision.revalidation.assessment.engineCallsAuthorized
        || decision.revalidation.assessment.failure
            != abi::RetailEngineAbiFailure::None
        || decision.revalidation.failure
            != abi::revalidation::RetailAbiRevalidationFailure::None)
    {
        decision.failure = mapRevalidationFailure(decision.revalidation);
        return decision;
    }

    if constexpr (!RetailRuntimeProductionAuthorizationAvailable)
    {
        decision.failure =
            RetailRuntimeAuthorityFailure::UnsupportedHostArchitecture;
        return decision;
    }
    else
    {
        const auto& diagnostics = decision.revalidation.diagnostics;
        std::uint32_t processId = 0u;
        std::uint64_t creationTime100ns = 0u;
        const HMODULE mainModule = GetModuleHandleW(nullptr);
        if (!mainModule
            || !currentProcessIdentity(processId, creationTime100ns)
            || !diagnostics.runtimeImageBase
            || !diagnostics.processId
            || !diagnostics.processCreationTime100ns)
        {
            decision.failure =
                RetailRuntimeAuthorityFailure::ProcessBindingUnavailable;
            return decision;
        }
        if (reinterpret_cast<std::uintptr_t>(mainModule)
                != diagnostics.runtimeImageBase
            || processId != diagnostics.processId
            || creationTime100ns
                != diagnostics.processCreationTime100ns)
        {
            decision.failure =
                RetailRuntimeAuthorityFailure::ProcessBindingChanged;
            return decision;
        }

        const std::uint64_t generation = nextGeneration();
        if (!generation)
        {
            decision.failure =
                RetailRuntimeAuthorityFailure::GenerationUnavailable;
            return decision;
        }
        std::uint32_t worldAddress = 0u;
        if (!relocateWorldAddress(
                diagnostics.runtimeImageBase,
                worldAddress))
        {
            decision.failure =
                RetailRuntimeAuthorityFailure::WorldAddressRelocationFailed;
            return decision;
        }

        detail::RetailRuntimeBinding binding {
            diagnostics.runtimeImageBase,
            processId,
            creationTime100ns,
            generation,
            0u,
        };
        binding.seal = sealBinding(binding);
        const RetailRuntimeAuthorityMetadata metadata {
            diagnostics.runtimeImageBase,
            processId,
            creationTime100ns,
            generation,
            worldAddress,
        };
        decision.authority = detail::RetailRuntimeAuthorityIssuer::issue(
            binding,
            metadata,
            decision.revalidation.assessment);
        if (!decision.authority.current())
        {
            decision.authority = {};
            decision.failure =
                RetailRuntimeAuthorityFailure::CapabilityIssuanceRejected;
            return decision;
        }
        decision.failure = RetailRuntimeAuthorityFailure::None;
        return decision;
    }
}

#if defined(FNVXR_RETAIL_RUNTIME_AUTHORITY_TEST_AUTHORITY)
namespace testing
{
RetailRuntimeAuthorityDecision
issueCurrentProcessBoundRetailRuntimeAuthorityForTest(
    std::uint64_t generation) noexcept
{
    RetailRuntimeAuthorityDecision decision {};
    if constexpr (!RetailRuntimeProductionAuthorizationAvailable)
    {
        (void)generation;
        decision.failure =
            RetailRuntimeAuthorityFailure::UnsupportedHostArchitecture;
        return decision;
    }
    else
    {
        if (!generation)
        {
            decision.failure =
                RetailRuntimeAuthorityFailure::GenerationUnavailable;
            return decision;
        }
        std::uint32_t processId = 0u;
        std::uint64_t creationTime100ns = 0u;
        const HMODULE mainModule = GetModuleHandleW(nullptr);
        if (!mainModule
            || !currentProcessIdentity(processId, creationTime100ns))
        {
            decision.failure =
                RetailRuntimeAuthorityFailure::ProcessBindingUnavailable;
            return decision;
        }
        const std::uintptr_t imageBase =
            reinterpret_cast<std::uintptr_t>(mainModule);
        std::uint32_t worldAddress = 0u;
        if (!relocateWorldAddress(imageBase, worldAddress))
        {
            decision.failure =
                RetailRuntimeAuthorityFailure::WorldAddressRelocationFailed;
            return decision;
        }

        decision.revalidation.evidence = {
            true, true, true, true, true, true, true, true, true, true, true,
        };
        decision.revalidation.assessment =
            abi::assessRetailEngineAbi(decision.revalidation.evidence);
        decision.revalidation.failure =
            abi::revalidation::RetailAbiRevalidationFailure::None;
        decision.revalidation.diagnostics.runtimeImageBase = imageBase;
        decision.revalidation.diagnostics.processId = processId;
        decision.revalidation.diagnostics.processCreationTime100ns =
            creationTime100ns;
        auto& compatibility = decision.revalidation.compatibilityProof;
        compatibility.evidence = {
            true, true, true, true, true, true, true, true, true, true, true,
        };
        compatibility.diagnostics.runtimeImageBase = imageBase;
        compatibility.diagnostics.processId = processId;
        compatibility.diagnostics.processCreationTime100ns =
            creationTime100ns;
        compatibility.failure = compatibility::RetailCompatibilityFailure::None;
        compatibility.compatible = true;

        detail::RetailRuntimeBinding binding {
            imageBase,
            processId,
            creationTime100ns,
            generation,
            0u,
        };
        binding.seal = sealBinding(binding);
        const RetailRuntimeAuthorityMetadata metadata {
            imageBase,
            processId,
            creationTime100ns,
            generation,
            worldAddress,
        };
        decision.authority = detail::RetailRuntimeAuthorityIssuer::issue(
            binding,
            metadata,
            decision.revalidation.assessment);
        decision.failure = decision.authority.current()
            ? RetailRuntimeAuthorityFailure::None
            : RetailRuntimeAuthorityFailure::CapabilityIssuanceRejected;
        return decision;
    }
}
}
#endif
}
