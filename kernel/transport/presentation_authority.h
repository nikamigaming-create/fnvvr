#pragma once

#include "../config_validation.h"
#include "../presentation/snapshots.h"

#include <cstdint>

namespace fnvxr::kernel::transport
{
enum class PresentationAuthority : std::uint8_t
{
    None,
    GpuProductKernel,
    CpuEngineCenter,
};

enum class RuntimePresentation : std::uint8_t
{
    None,
    World,
    WorldWithSpatialPipBoy,
    BlockingUi,
    Loading,
};

enum class AuthorityFailure : std::uint8_t
{
    None,
    RuntimeUnavailable,
    InvalidRuntimePhase,
    InvalidUiClassification,
    ContradictoryRuntimeEvidence,
    UnsupportedTransport,
};

struct AuthorityDecision final
{
    PresentationAuthority authority = PresentationAuthority::None;
    RuntimePresentation presentation = RuntimePresentation::None;
    AuthorityFailure failure = AuthorityFailure::RuntimeUnavailable;

    [[nodiscard]] explicit operator bool() const noexcept;
};

// Selects the only policy allowed to evaluate a runtime snapshot. This does
// not authorize pixels; the returned ProductKernel or engine-center policy
// must still validate its transport-specific frame evidence.
[[nodiscard]] AuthorityDecision resolvePresentationAuthority(
    const ValidatedRuntimeConfig& config,
    const presentation::RuntimeSnapshot& runtime) noexcept;
}
