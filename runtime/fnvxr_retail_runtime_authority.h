#pragma once

#include "fnvxr_center_renderer_backend.h"
#include "fnvxr_engine_stereo_resources.h"
#include "fnvxr_retail_abi_revalidator.h"
#include "fnvxr_retail_engine_calls.h"
#include "fnvxr_retail_world_hook_lease.h"

#include <cstdint>

namespace fnvxr::engine
{
enum class RetailRuntimeAuthorityFailure : std::uint8_t
{
    None = 0,
    UnsupportedHostArchitecture,
    CompatibilityProofRejected,
    AbiRevalidationRejected,
    ProcessBindingUnavailable,
    ProcessBindingChanged,
    GenerationUnavailable,
    WorldAddressRelocationFailed,
    CapabilityIssuanceRejected,
};

struct RetailRuntimeAuthorityMetadata
{
    std::uintptr_t runtimeImageBase = 0u;
    std::uint32_t processId = 0u;
    std::uint64_t processCreationTime100ns = 0u;
    std::uint64_t generation = 0u;
    std::uint32_t runtimeWorldAddress = 0u;
};

class RetailRuntimeAuthorityBundle final
{
public:
    constexpr RetailRuntimeAuthorityBundle() noexcept = default;

    bool current() const noexcept;

    constexpr const RetailRuntimeAuthorityMetadata& metadata() const noexcept
    {
        return mMetadata;
    }

    constexpr const RetailEngineCallAuthorization& engineCalls() const noexcept
    {
        return mEngineCalls;
    }

    constexpr const StereoResourceAuthorization& stereoResources() const noexcept
    {
        return mStereoResources;
    }

    constexpr const CenterRendererAuthorization& centerRenderer() const noexcept
    {
        return mCenterRenderer;
    }

    constexpr const RetailWorldHookAuthorization& worldHook() const noexcept
    {
        return mWorldHook;
    }

private:
    constexpr RetailRuntimeAuthorityBundle(
        const detail::RetailRuntimeBinding& binding,
        detail::RetailRuntimeBindingValidator bindingValidator,
        const RetailRuntimeAuthorityMetadata& metadata,
        const RetailEngineCallAuthorization& engineCalls,
        const StereoResourceAuthorization& stereoResources,
        const CenterRendererAuthorization& centerRenderer,
        const RetailWorldHookAuthorization& worldHook) noexcept
        : mBinding(binding),
          mBindingValidator(bindingValidator),
          mMetadata(metadata),
          mEngineCalls(engineCalls),
          mStereoResources(stereoResources),
          mCenterRenderer(centerRenderer),
          mWorldHook(worldHook)
    {
    }

    detail::RetailRuntimeBinding mBinding {};
    detail::RetailRuntimeBindingValidator mBindingValidator = nullptr;
    RetailRuntimeAuthorityMetadata mMetadata {};
    RetailEngineCallAuthorization mEngineCalls {};
    StereoResourceAuthorization mStereoResources {};
    CenterRendererAuthorization mCenterRenderer {};
    RetailWorldHookAuthorization mWorldHook {};

    friend struct detail::RetailRuntimeAuthorityIssuer;
};

struct RetailRuntimeAuthorityDecision
{
    RetailRuntimeAuthorityBundle authority {};
    abi::revalidation::RetailAbiRevalidationResult revalidation {};
    RetailRuntimeAuthorityFailure failure =
        RetailRuntimeAuthorityFailure::UnsupportedHostArchitecture;

    bool complete() const noexcept
    {
        return failure == RetailRuntimeAuthorityFailure::None
            && authority.current();
    }
};

// The sole production issuer. It accepts no caller evidence or configuration:
// all four capabilities are minted together only after the synchronous
// current-process compatibility and ABI revalidation succeeds.
RetailRuntimeAuthorityDecision authorizeCurrentRetailRuntimeAtDecisionPoint()
    noexcept;

#if defined(FNVXR_RETAIL_RUNTIME_AUTHORITY_TEST_AUTHORITY)
namespace testing
{
// Isolated positive-path seam for the authority unit test. It is absent from
// production builds and still binds the result to the actual test process.
RetailRuntimeAuthorityDecision
issueCurrentProcessBoundRetailRuntimeAuthorityForTest(
    std::uint64_t generation) noexcept;
}
#endif
}

