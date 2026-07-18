#pragma once

#include <cstdint>

namespace fnvxr::engine
{
// A production issuer exists only in the 32-bit Windows runtime that can be
// loaded into FalloutNV.exe.  Other builds retain the same fail-closed API so
// they can compile and test rejection paths without ever issuing authority.
#if defined(_WIN32) && (defined(_M_IX86) || defined(__i386__))
inline constexpr bool RetailRuntimeProductionAuthorizationAvailable = true;
#else
inline constexpr bool RetailRuntimeProductionAuthorizationAvailable = false;
#endif

namespace detail
{
struct RetailRuntimeBinding
{
    std::uintptr_t runtimeImageBase = 0u;
    std::uint32_t processId = 0u;
    std::uint64_t processCreationTime100ns = 0u;
    std::uint64_t generation = 0u;
    std::uint64_t seal = 0u;
};

using RetailRuntimeBindingValidator = bool (*)(
    const RetailRuntimeBinding&) noexcept;

// Defined only by fnvxr_retail_runtime_authority.cpp.  Component headers can
// consume capabilities, but cannot mint a production-bound one.
struct RetailRuntimeAuthorityIssuer;
}
}
