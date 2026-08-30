#pragma once

#include "config_validation.h"
#include "presentation/coordinator.h"

#include <optional>

namespace fnvxr::kernel
{
// The single application-policy entry point. Platform adapters translate
// OpenXR/D3D/Gamebryo state into kernel snapshots and execute its decisions.
class ProductKernel final
{
public:
    [[nodiscard]] static std::optional<ProductKernel> create(
        const RuntimeConfig& config,
        presentation::CoordinatorPolicy presentationPolicy = {});

    [[nodiscard]] const ValidatedRuntimeConfig& config() const noexcept;
    [[nodiscard]] presentation::PresentationDecision present(
        const presentation::PresentationInput& input);
    void resetPresentation() noexcept;

private:
    ProductKernel(
        const ValidatedRuntimeConfig& config,
        presentation::CoordinatorPolicy presentationPolicy);

    ValidatedRuntimeConfig config_;
    presentation::Coordinator presentation_;
};
}
