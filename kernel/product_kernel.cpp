#include "product_kernel.h"

namespace fnvxr::kernel
{
std::optional<ProductKernel> ProductKernel::create(
    const RuntimeConfig& config,
    presentation::CoordinatorPolicy presentationPolicy)
{
    const ConfigValidationResult validation = validateRuntimeConfig(config);
    if (!validation)
        return std::nullopt;
    return ProductKernel(*validation.config, presentationPolicy);
}

ProductKernel::ProductKernel(
    const ValidatedRuntimeConfig& config,
    presentation::CoordinatorPolicy presentationPolicy)
    : config_(config), presentation_(presentationPolicy)
{
}

const ValidatedRuntimeConfig& ProductKernel::config() const noexcept
{
    return config_;
}

presentation::PresentationDecision ProductKernel::present(
    const presentation::PresentationInput& input)
{
    return presentation_.advance(input);
}

void ProductKernel::resetPresentation() noexcept
{
    presentation_.reset();
}
}
