#pragma once

#include "fnvxr_retail_engine_abi.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace fnvxr::engine
{
// This table is intentionally typed all the way to the retail x86 ABI.  It is
// populated only after synchronous ABI evidence has produced an authorized
// assessment and every address has been relocated without truncation.
struct RetailEngineCalls
{
    abi::NiAllocateFunction niAllocate = nullptr;
    abi::NiFreeFunction niFree = nullptr;
    abi::NiCameraCreateFunction niCameraCreate = nullptr;
    abi::BSCullingProcessConstructorFunction cullingProcessConstruct = nullptr;
    abi::BSCullingProcessDestructorBodyFunction cullingProcessDestroy = nullptr;
    abi::BSShaderAccumulatorConstructorFunction shaderAccumulatorConstruct = nullptr;
    abi::BSShaderAccumulatorScalarDestructorFunction shaderAccumulatorDestroy = nullptr;
    abi::NiRefObjectFreeFunction niRefObjectFree = nullptr;
    abi::AccumulatorSetCameraFunction accumulatorSetCamera = nullptr;
    abi::CullingProcessAltFunction cullingProcessAlt = nullptr;
    abi::AccumulatorAddVisibleArrayFunction accumulatorAddVisibleArray = nullptr;
    abi::AccumulatorRenderFunction renderAccumulatorWithoutFinalize = nullptr;
    abi::AccumulatorRenderFunction finalizeAccumulator = nullptr;

    bool empty() const noexcept
    {
        return !niAllocate
            && !niFree
            && !niCameraCreate
            && !cullingProcessConstruct
            && !cullingProcessDestroy
            && !shaderAccumulatorConstruct
            && !shaderAccumulatorDestroy
            && !niRefObjectFree
            && !accumulatorSetCamera
            && !cullingProcessAlt
            && !accumulatorAddVisibleArray
            && !renderAccumulatorWithoutFinalize
            && !finalizeAccumulator;
    }

    bool complete() const noexcept
    {
        return niAllocate
            && niFree
            && niCameraCreate
            && cullingProcessConstruct
            && cullingProcessDestroy
            && shaderAccumulatorConstruct
            && shaderAccumulatorDestroy
            && niRefObjectFree
            && accumulatorSetCamera
            && cullingProcessAlt
            && accumulatorAddVisibleArray
            && renderAccumulatorWithoutFinalize
            && finalizeAccumulator;
    }
};

struct RetailEngineCallAddressTable
{
    std::uintptr_t niAllocate = 0u;
    std::uintptr_t niFree = 0u;
    std::uintptr_t niCameraCreate = 0u;
    std::uintptr_t cullingProcessConstruct = 0u;
    std::uintptr_t cullingProcessDestroy = 0u;
    std::uintptr_t shaderAccumulatorConstruct = 0u;
    std::uintptr_t shaderAccumulatorDestroy = 0u;
    std::uintptr_t niRefObjectFree = 0u;
    std::uintptr_t accumulatorSetCamera = 0u;
    std::uintptr_t cullingProcessAlt = 0u;
    std::uintptr_t accumulatorAddVisibleArray = 0u;
    std::uintptr_t renderAccumulatorWithoutFinalize = 0u;
    std::uintptr_t finalizeAccumulator = 0u;
};

namespace detail
{
constexpr bool retailEngineFunctionNameEqual(
    const char* left,
    const char* right) noexcept
{
    if (!left || !right)
        return false;
    while (*left && *right)
    {
        if (*left != *right)
            return false;
        ++left;
        ++right;
    }
    return *left == *right;
}

constexpr std::uintptr_t uniqueRetailFunctionPreferredAddress(
    const char* name) noexcept
{
    std::uintptr_t result = 0u;
    for (const abi::RetailFunctionAbiDescriptor& descriptor
         : abi::RetailFunctionAbiInventory)
    {
        if (!retailEngineFunctionNameEqual(descriptor.name, name))
            continue;
        if (result != 0u)
            return 0u;
        result = descriptor.preferredAddress;
    }
    return result;
}

constexpr bool uniqueProductionFunctionMatches(
    const char* name,
    std::uintptr_t preferredAddress) noexcept
{
    std::size_t matches = 0u;
    for (const abi::RetailFunctionAbiDescriptor& descriptor
         : abi::RetailFunctionAbiInventory)
    {
        if (!retailEngineFunctionNameEqual(descriptor.name, name))
            continue;
        ++matches;
        if (descriptor.preferredAddress != preferredAddress
            || !abi::productionProven(descriptor))
        {
            return false;
        }
    }
    return matches == 1u;
}
}

// The values are selected by canonical ABI-inventory name.  Exact-address
// assertions below prevent an inventory reorder or silent address drift from
// changing the resolver's call surface.
inline constexpr RetailEngineCallAddressTable RetailEngineCallPreferredAddresses {
    detail::uniqueRetailFunctionPreferredAddress("Ni_Alloc"),
    detail::uniqueRetailFunctionPreferredAddress("Ni_Free"),
    detail::uniqueRetailFunctionPreferredAddress("NiCamera::Create"),
    detail::uniqueRetailFunctionPreferredAddress(
        "BSCullingProcess::BSCullingProcess"),
    detail::uniqueRetailFunctionPreferredAddress(
        "BSCullingProcess::~BSCullingProcess body"),
    detail::uniqueRetailFunctionPreferredAddress(
        "BSShaderAccumulator::BSShaderAccumulator"),
    detail::uniqueRetailFunctionPreferredAddress(
        "BSShaderAccumulator scalar deleting destructor"),
    detail::uniqueRetailFunctionPreferredAddress("NiRefObject::Free thunk"),
    detail::uniqueRetailFunctionPreferredAddress("NiAccumulator::SetCamera"),
    detail::uniqueRetailFunctionPreferredAddress("BSCullingProcess::ProcessAlt"),
    detail::uniqueRetailFunctionPreferredAddress(
        "NiAccumulator::AddVisibleArray"),
    detail::uniqueRetailFunctionPreferredAddress(
        "RenderAccumulatorWithoutFinalize"),
    detail::uniqueRetailFunctionPreferredAddress("FinalizeAccumulator"),
};

constexpr bool retailEngineCallInventoryComplete() noexcept
{
    return detail::uniqueProductionFunctionMatches(
               "Ni_Alloc",
               RetailEngineCallPreferredAddresses.niAllocate)
        && detail::uniqueProductionFunctionMatches(
               "Ni_Free",
               RetailEngineCallPreferredAddresses.niFree)
        && detail::uniqueProductionFunctionMatches(
               "NiCamera::Create",
               RetailEngineCallPreferredAddresses.niCameraCreate)
        && detail::uniqueProductionFunctionMatches(
               "BSCullingProcess::BSCullingProcess",
               RetailEngineCallPreferredAddresses.cullingProcessConstruct)
        && detail::uniqueProductionFunctionMatches(
               "BSCullingProcess::~BSCullingProcess body",
               RetailEngineCallPreferredAddresses.cullingProcessDestroy)
        && detail::uniqueProductionFunctionMatches(
               "BSShaderAccumulator::BSShaderAccumulator",
               RetailEngineCallPreferredAddresses.shaderAccumulatorConstruct)
        && detail::uniqueProductionFunctionMatches(
               "BSShaderAccumulator scalar deleting destructor",
               RetailEngineCallPreferredAddresses.shaderAccumulatorDestroy)
        && detail::uniqueProductionFunctionMatches(
               "NiRefObject::Free thunk",
               RetailEngineCallPreferredAddresses.niRefObjectFree)
        && detail::uniqueProductionFunctionMatches(
               "NiAccumulator::SetCamera",
               RetailEngineCallPreferredAddresses.accumulatorSetCamera)
        && detail::uniqueProductionFunctionMatches(
               "BSCullingProcess::ProcessAlt",
               RetailEngineCallPreferredAddresses.cullingProcessAlt)
        && detail::uniqueProductionFunctionMatches(
               "NiAccumulator::AddVisibleArray",
               RetailEngineCallPreferredAddresses.accumulatorAddVisibleArray)
        && detail::uniqueProductionFunctionMatches(
               "RenderAccumulatorWithoutFinalize",
               RetailEngineCallPreferredAddresses
                   .renderAccumulatorWithoutFinalize)
        && detail::uniqueProductionFunctionMatches(
               "FinalizeAccumulator",
               RetailEngineCallPreferredAddresses.finalizeAccumulator);
}

static_assert(RetailEngineCallPreferredAddresses.niAllocate == 0x00AA13E0u);
static_assert(RetailEngineCallPreferredAddresses.niFree == 0x00AA1460u);
static_assert(RetailEngineCallPreferredAddresses.niCameraCreate == 0x00A71430u);
static_assert(
    RetailEngineCallPreferredAddresses.cullingProcessConstruct == 0x004A0EB0u);
static_assert(
    RetailEngineCallPreferredAddresses.cullingProcessDestroy == 0x004A0F60u);
static_assert(
    RetailEngineCallPreferredAddresses.shaderAccumulatorConstruct
    == 0x00B660D0u);
static_assert(
    RetailEngineCallPreferredAddresses.shaderAccumulatorDestroy
    == 0x00B664F0u);
static_assert(RetailEngineCallPreferredAddresses.niRefObjectFree == 0x00CFCC20u);
static_assert(
    RetailEngineCallPreferredAddresses.accumulatorSetCamera == 0x00D47A40u);
static_assert(
    RetailEngineCallPreferredAddresses.cullingProcessAlt == 0x00C4F070u);
static_assert(
    RetailEngineCallPreferredAddresses.accumulatorAddVisibleArray
    == 0x00A9B790u);
static_assert(
    RetailEngineCallPreferredAddresses.renderAccumulatorWithoutFinalize
    == 0x00B6BA20u);
static_assert(
    RetailEngineCallPreferredAddresses.finalizeAccumulator == 0x00B6B930u);
static_assert(retailEngineCallInventoryComplete());

enum class RetailEngineAddressRelocationFailure : std::uint8_t
{
    None,
    InvalidModuleBase,
    PreferredAddressOutsideImage,
    AddressOverflow,
};

struct RetailEngineAddressRelocation
{
    std::uintptr_t address = 0u;
    RetailEngineAddressRelocationFailure failure =
        RetailEngineAddressRelocationFailure::InvalidModuleBase;

    constexpr bool complete() const noexcept
    {
        return failure == RetailEngineAddressRelocationFailure::None
            && address != 0u;
    }
};

// PE images are allocated on 64-KiB boundaries on the supported Windows x86
// process.  The full image and every relocated address must also remain inside
// the 32-bit target address space; a host-size cast is never allowed to wrap.
inline constexpr std::uintptr_t RetailPeAllocationAlignment = 0x10000u;
inline constexpr std::uintptr_t RetailX86MaximumAddress =
    static_cast<std::uintptr_t>(
        std::numeric_limits<std::uint32_t>::max());

constexpr RetailEngineAddressRelocation relocateRetailEngineAddress(
    std::uintptr_t loadedImageBase,
    std::uintptr_t preferredAddress) noexcept
{
    if (loadedImageBase == 0u
        || loadedImageBase % RetailPeAllocationAlignment != 0u)
    {
        return {
            0u,
            RetailEngineAddressRelocationFailure::InvalidModuleBase,
        };
    }
    if (!abi::imageContains(preferredAddress, 1u))
    {
        return {
            0u,
            RetailEngineAddressRelocationFailure::PreferredAddressOutsideImage,
        };
    }
    if (loadedImageBase > RetailX86MaximumAddress
        || static_cast<std::uintptr_t>(SupportedSizeOfImage - 1u)
            > RetailX86MaximumAddress - loadedImageBase)
    {
        return {
            0u,
            RetailEngineAddressRelocationFailure::AddressOverflow,
        };
    }

    const std::uintptr_t imageOffset = preferredAddress - SupportedImageBase;
    if (imageOffset > RetailX86MaximumAddress - loadedImageBase)
    {
        return {
            0u,
            RetailEngineAddressRelocationFailure::AddressOverflow,
        };
    }
    return {
        loadedImageBase + imageOffset,
        RetailEngineAddressRelocationFailure::None,
    };
}

class RetailEngineCallAuthorization;

namespace detail
{
struct RetailEngineCallAuthorizationAccess;
}

// There is deliberately no production issuer in this header.  A future
// same-process probe must own issuance after producing the complete assessment.
// The named friend exists only in the isolated unit-test translation unit.
struct RetailEngineCallResolverTestAuthority;

class RetailEngineCallAuthorization final
{
public:
    constexpr RetailEngineCallAuthorization() noexcept = default;

private:
    explicit constexpr RetailEngineCallAuthorization(
        const abi::RetailEngineAbiAssessment& assessment) noexcept
        : mAssessment(assessment)
    {
    }

    abi::RetailEngineAbiAssessment mAssessment {};

    friend struct detail::RetailEngineCallAuthorizationAccess;
    friend struct RetailEngineCallResolverTestAuthority;
};

inline constexpr bool RetailEngineCallProductionAuthorizationAvailable = false;
#if defined(_WIN32) && (defined(_M_IX86) || defined(__i386__))
inline constexpr bool RetailEngineCallArchitectureSupported = true;
#else
inline constexpr bool RetailEngineCallArchitectureSupported = false;
#endif

namespace detail
{
struct RetailEngineCallAuthorizationAccess
{
    static constexpr bool authorized(
        const RetailEngineCallAuthorization& authorization) noexcept
    {
        return authorization.mAssessment.engineCallsAuthorized
            && authorization.mAssessment.failure
                == abi::RetailEngineAbiFailure::None;
    }
};

inline constexpr std::array<std::uintptr_t, 13>
retailEngineCallAddressesAsArray(
    const RetailEngineCallAddressTable& addresses) noexcept
{
    return {{
        addresses.niAllocate,
        addresses.niFree,
        addresses.niCameraCreate,
        addresses.cullingProcessConstruct,
        addresses.cullingProcessDestroy,
        addresses.shaderAccumulatorConstruct,
        addresses.shaderAccumulatorDestroy,
        addresses.niRefObjectFree,
        addresses.accumulatorSetCamera,
        addresses.cullingProcessAlt,
        addresses.accumulatorAddVisibleArray,
        addresses.renderAccumulatorWithoutFinalize,
        addresses.finalizeAccumulator,
    }};
}

inline constexpr RetailEngineCallAddressTable retailEngineCallAddressesFromArray(
    const std::array<std::uintptr_t, 13>& addresses) noexcept
{
    return {
        addresses[0],
        addresses[1],
        addresses[2],
        addresses[3],
        addresses[4],
        addresses[5],
        addresses[6],
        addresses[7],
        addresses[8],
        addresses[9],
        addresses[10],
        addresses[11],
        addresses[12],
    };
}

inline RetailEngineAddressRelocationFailure relocateRetailEngineCallTable(
    std::uintptr_t loadedImageBase,
    RetailEngineCallAddressTable& relocated) noexcept
{
    const std::array<std::uintptr_t, 13> preferred =
        retailEngineCallAddressesAsArray(RetailEngineCallPreferredAddresses);
    std::array<std::uintptr_t, 13> candidate {};
    for (std::size_t index = 0u; index < preferred.size(); ++index)
    {
        const RetailEngineAddressRelocation result =
            relocateRetailEngineAddress(loadedImageBase, preferred[index]);
        if (!result.complete())
            return result.failure;
        candidate[index] = result.address;
    }
    relocated = retailEngineCallAddressesFromArray(candidate);
    return RetailEngineAddressRelocationFailure::None;
}
}

enum class RetailEngineCallResolutionFailure : std::uint8_t
{
    None,
    Unauthorized,
    UnsupportedArchitecture,
    StaticInventoryIncomplete,
    InvalidModuleBase,
    PreferredAddressOutsideImage,
    AddressOverflow,
};

struct RetailEngineCallResolution
{
    RetailEngineCalls calls {};
    RetailEngineCallResolutionFailure failure =
        RetailEngineCallResolutionFailure::Unauthorized;

    bool complete() const noexcept
    {
        return failure == RetailEngineCallResolutionFailure::None
            && calls.complete();
    }
};

constexpr RetailEngineCallResolutionFailure retailEngineCallResolutionFailure(
    RetailEngineAddressRelocationFailure failure) noexcept
{
    switch (failure)
    {
    case RetailEngineAddressRelocationFailure::None:
        return RetailEngineCallResolutionFailure::None;
    case RetailEngineAddressRelocationFailure::InvalidModuleBase:
        return RetailEngineCallResolutionFailure::InvalidModuleBase;
    case RetailEngineAddressRelocationFailure::PreferredAddressOutsideImage:
        return RetailEngineCallResolutionFailure::PreferredAddressOutsideImage;
    case RetailEngineAddressRelocationFailure::AddressOverflow:
        return RetailEngineCallResolutionFailure::AddressOverflow;
    }
    return RetailEngineCallResolutionFailure::AddressOverflow;
}

inline RetailEngineCallResolution resolveRetailEngineCalls(
    const RetailEngineCallAuthorization& authorization,
    std::uintptr_t loadedImageBase) noexcept
{
    if (!detail::RetailEngineCallAuthorizationAccess::authorized(authorization))
        return { {}, RetailEngineCallResolutionFailure::Unauthorized };
    if constexpr (!RetailEngineCallArchitectureSupported)
    {
        return {
            {},
            RetailEngineCallResolutionFailure::UnsupportedArchitecture,
        };
    }
    else
    {
        if (!retailEngineCallInventoryComplete())
        {
            return {
                {},
                RetailEngineCallResolutionFailure::StaticInventoryIncomplete,
            };
        }

        RetailEngineCallAddressTable addresses {};
        const RetailEngineAddressRelocationFailure relocationFailure =
            detail::relocateRetailEngineCallTable(
                loadedImageBase,
                addresses);
        if (relocationFailure != RetailEngineAddressRelocationFailure::None)
        {
            return {
                {},
                retailEngineCallResolutionFailure(relocationFailure),
            };
        }

        RetailEngineCalls calls {};
        calls.niAllocate =
            reinterpret_cast<abi::NiAllocateFunction>(addresses.niAllocate);
        calls.niFree =
            reinterpret_cast<abi::NiFreeFunction>(addresses.niFree);
        calls.niCameraCreate =
            reinterpret_cast<abi::NiCameraCreateFunction>(addresses.niCameraCreate);
        calls.cullingProcessConstruct =
            reinterpret_cast<abi::BSCullingProcessConstructorFunction>(
                addresses.cullingProcessConstruct);
        calls.cullingProcessDestroy =
            reinterpret_cast<abi::BSCullingProcessDestructorBodyFunction>(
                addresses.cullingProcessDestroy);
        calls.shaderAccumulatorConstruct =
            reinterpret_cast<abi::BSShaderAccumulatorConstructorFunction>(
                addresses.shaderAccumulatorConstruct);
        calls.shaderAccumulatorDestroy =
            reinterpret_cast<abi::BSShaderAccumulatorScalarDestructorFunction>(
                addresses.shaderAccumulatorDestroy);
        calls.niRefObjectFree =
            reinterpret_cast<abi::NiRefObjectFreeFunction>(
                addresses.niRefObjectFree);
        calls.accumulatorSetCamera =
            reinterpret_cast<abi::AccumulatorSetCameraFunction>(
                addresses.accumulatorSetCamera);
        calls.cullingProcessAlt =
            reinterpret_cast<abi::CullingProcessAltFunction>(
                addresses.cullingProcessAlt);
        calls.accumulatorAddVisibleArray =
            reinterpret_cast<abi::AccumulatorAddVisibleArrayFunction>(
                addresses.accumulatorAddVisibleArray);
        calls.renderAccumulatorWithoutFinalize =
            reinterpret_cast<abi::AccumulatorRenderFunction>(
                addresses.renderAccumulatorWithoutFinalize);
        calls.finalizeAccumulator =
            reinterpret_cast<abi::AccumulatorRenderFunction>(
                addresses.finalizeAccumulator);

        if (!calls.complete())
            return { {}, RetailEngineCallResolutionFailure::AddressOverflow };
        return { calls, RetailEngineCallResolutionFailure::None };
    }
}

static_assert(!RetailEngineCallProductionAuthorizationAvailable);
}
