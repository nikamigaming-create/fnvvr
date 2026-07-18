#pragma once

#include "fnvxr_retail_engine_abi.h"

#if defined(FNVXR_RETAIL_ABI_REVALIDATOR_TEST_AUTHORITY)
#include "fnvxr_module_inventory_acceptance.h"
#endif

#include <array>
#include <cstddef>
#include <cstdint>

namespace fnvxr::module_census
{
class OwnedWindowsModuleCensus;
}

namespace fnvxr::inventory
{
struct ModuleInventoryEvidence;
}

namespace fnvxr::engine::abi::revalidation
{
enum class RetailAbiRevalidationFailure : std::uint8_t
{
    None = 0,
    UnsupportedHostArchitecture,
    CensusNotSealed,
    ProcessIdentityUnavailable,
    CensusProcessMismatch,
    MainModuleUnavailable,
    MainModuleIsNotFallout,
    InvalidLoadedPe,
    LoadedImageReadFailed,
    RuntimeEvidenceRejected,
};

struct RetailAbiRevalidationDiagnostics
{
    std::uintptr_t runtimeImageBase = 0;
    std::uint32_t processId = 0;
    std::uint64_t processCreationTime100ns = 0;
    std::size_t loadedExecutableSectionCount = 0;
    std::size_t executableSectionBytesInspected = 0;
    std::size_t functionBodiesHashed = 0;
    std::size_t vtableSlotsRead = 0;
    std::size_t vtableBlockBytesHashed = 0;
};

struct RetailAbiRevalidationResult
{
    RetailEngineAbiEvidence evidence {};
    RetailEngineAbiAssessment assessment {};
    RetailAbiRevalidationFailure failure =
        RetailAbiRevalidationFailure::UnsupportedHostArchitecture;
    RetailAbiRevalidationDiagnostics diagnostics {};
};

// This is the only production entry point. It has no evidence switches and no
// caller-selected success path: every bit is derived from the current process
// and the sealed, owned Windows module census during this call.
RetailAbiRevalidationResult revalidateCurrentRetailEngineAbiAtDecisionPoint(
    const module_census::OwnedWindowsModuleCensus& census) noexcept;

#if defined(FNVXR_RETAIL_ABI_REVALIDATOR_TEST_AUTHORITY)
namespace testing
{
enum class SyntheticMemoryAccess : std::uint8_t
{
    NoAccess = 0,
    ReadOnly,
    ReadWrite,
    ExecuteRead,
    ExecuteReadWrite,
};

struct SyntheticByteRange
{
    std::uintptr_t address = 0;
    const std::uint8_t* bytes = nullptr;
    std::size_t byteCount = 0;
};

struct SyntheticProtectionRange
{
    std::uintptr_t address = 0;
    std::size_t byteCount = 0;
    SyntheticMemoryAccess access = SyntheticMemoryAccess::NoAccess;
    bool guard = false;
};

struct ExecutableSectionLayout
{
    std::array<std::uint8_t, 8> name {};
    std::uint32_t rva = 0;
    std::uint32_t virtualBytes = 0;
    std::uint32_t mappedBytes = 0;
    std::uint32_t protectionBytes = 0;
    std::uint32_t rawBytes = 0;
    std::uint32_t characteristics = 0;
    std::uint8_t independentLayoutSamples = 0;
};

struct SyntheticRetailAbiContract
{
    std::uintptr_t preferredImageBase = 0;
    LoadedExecutableIdentity loadedIdentity {};
    std::uint32_t sizeOfImage = 0;

    const ExecutableSectionLayout* executableSections = nullptr;
    std::size_t executableSectionCount = 0;
    const LoadedFunctionManifestEntry* coreManifest = nullptr;
    std::size_t coreManifestCount = 0;
    const RetailFunctionAbiDescriptor* functionInventory = nullptr;
    std::size_t functionInventoryCount = 0;
    const RetailVtableSlotDescriptor* vtableSlots = nullptr;
    std::size_t vtableSlotCount = 0;
    const RetailVtableBlockDescriptor* vtableBlocks = nullptr;
    std::size_t vtableBlockCount = 0;

    std::uintptr_t sceneGraphSingletonPointerAddress = 0;
    std::uintptr_t bSCullingProcessVtableAddress = 0;
};

struct SyntheticRetailAbiSnapshot
{
    std::uintptr_t runtimeImageBase = 0;
    const SyntheticByteRange* byteRanges = nullptr;
    std::size_t byteRangeCount = 0;
    const SyntheticProtectionRange* protectionRanges = nullptr;
    std::size_t protectionRangeCount = 0;
    const inventory::ModuleInventoryEvidence* census = nullptr;
    inventory::ProcessCreationIdentity processIdentity;
    std::uint64_t evidenceGeneration = 0;
};

RetailAbiRevalidationResult revalidateSyntheticRetailAbiAtDecisionPoint(
    const SyntheticRetailAbiSnapshot& snapshot,
    const SyntheticRetailAbiContract& contract) noexcept;

Sha256Digest sha256ForSyntheticAuthority(
    const std::uint8_t* bytes,
    std::size_t byteCount) noexcept;
}
#endif
}
