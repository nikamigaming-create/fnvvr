#pragma once

#include <cstdint>

namespace fnvxr::probe
{
enum class ModuleLookupResult : std::uint8_t
{
    Found,
    NotFound,
    EnumerationFailed,
};

// Compatibility is proven only when a completed module enumeration establishes
// absence. A present unverified module and an unreadable module inventory both
// fail closed.
constexpr bool compatibilityModuleProofComplete(ModuleLookupResult result) noexcept
{
    return result == ModuleLookupResult::NotFound;
}
}
