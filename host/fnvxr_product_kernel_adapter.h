#pragma once

#include "../kernel/product_kernel.h"
#include "../protocol/fnvxr_product_contract.h"

#include <cstdint>
namespace fnvxr::host
{
struct PresentationTransportProof final
{
    std::uint64_t producerEpoch = 0;
    std::uint64_t transaction = 0;
    bool producerOwned = false;
    bool completionObserved = false;
    bool consumerAcquired = false;
    bool exclusiveInterval = false;
    bool synchronized = false;
};

// Temporary protocol adapter: legacy host evidence enters here, all policy is
// decided by ProductKernel, and the legacy decision shape survives only for
// existing renderer bindings and telemetry.
class ProductKernelAdapter final
{
public:
    explicit ProductKernelAdapter(
        const kernel::ValidatedRuntimeConfig& config);

    [[nodiscard]] static kernel::presentation::RuntimeSnapshot runtimeSnapshot(
        const product::PresentationInput& input) noexcept;

    [[nodiscard]] product::PresentationDecision advance(
        const product::PresentationInput& input,
        const PresentationTransportProof& transport,
        bool trackedRigReady,
        bool wristContentReady,
        const PresentationTransportProof& uiTransport = {});
    void reset() noexcept;

private:
    [[nodiscard]] static kernel::presentation::PresentationInput translate(
        const product::PresentationInput& input,
        const PresentationTransportProof& transport,
        bool trackedRigReady,
        bool wristContentReady,
        const PresentationTransportProof& uiTransport) noexcept;
    [[nodiscard]] static product::PresentationDecision translate(
        const kernel::presentation::PresentationDecision& decision,
        const product::PresentationInput& input) noexcept;

    kernel::ProductKernel kernel_;
};
}
