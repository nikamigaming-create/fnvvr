#include "product_kernel.h"

#include <utility>

namespace fnvxr::kernel
{
std::optional<ProductKernel> ProductKernel::create(
    const RuntimeConfig& config,
    presentation::CoordinatorPolicy presentationPolicy)
{
    const ConfigValidationResult validation = validateRuntimeConfig(config);
    if (!validation)
        return std::nullopt;
    return ProductKernel::fromValidated(
        std::move(*validation.config), presentationPolicy);
}

ProductKernel ProductKernel::fromValidated(
    ValidatedRuntimeConfig config,
    presentation::CoordinatorPolicy presentationPolicy)
{
    return ProductKernel(std::move(config), presentationPolicy);
}

ProductKernel::ProductKernel(
    ValidatedRuntimeConfig config,
    presentation::CoordinatorPolicy presentationPolicy)
    : config_(std::move(config)), presentation_(presentationPolicy)
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
