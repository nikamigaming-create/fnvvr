#pragma once

#include "fnvxr_retail_engine_abi.h"

#include <cstddef>
#include <cstdint>

namespace fnvxr::engine::compatibility
{
enum class RetailCompatibilityFailure : std::uint8_t
{
    None = 0,
    UnsupportedHostArchitecture,
    ProcessIdentityUnavailable,
    ModuleEnumerationFailed,
    ModuleSnapshotChanged,
    MainModuleUnavailable,
    MainModuleIsNotFallout,
    RetailExecutableIdentityMismatch,
    ProhibitedOverlayOrCaptureLoaded,
    DuplicateJipModule,
    Jip5730IdentityMismatch,
    JipRenderFirstPersonRewriteMismatch,
    DuplicateShowOffModule,
    ShowOff184IdentityMismatch,
    ProtectedCoreBodyMismatch,
    ProtectedFunctionMismatch,
    ProtectedVtableSlotMismatch,
    ProtectedVtableBlockMismatch,
    ProtectedMemoryUnstable,
};

struct RetailCompatibilityEvidence
{
    bool retailExecutableIdentityMatched = false;
    bool moduleSnapshotStable = false;
    bool prohibitedModulesAbsent = false;
    bool jip5730ExactOrAbsent = false;
    bool showOff184ExactOrAbsent = false;
    bool renderFirstPersonStockOrJipNormalized = false;
    bool protectedCoreBodiesMatched = false;
    bool protectedFunctionInventoryMatched = false;
    bool protectedVtableSlotsMatched = false;
    bool protectedVtableBlocksMatched = false;
    bool synchronousSameProcess = false;
};

struct RetailCompatibilityDiagnostics
{
    std::uintptr_t runtimeImageBase = 0;
    std::uint32_t processId = 0;
    std::uint64_t processCreationTime100ns = 0;
    std::size_t moduleCount = 0;
    std::size_t protectedCoreBodiesHashed = 0;
    std::size_t protectedFunctionsHashed = 0;
    std::size_t protectedVtableSlotsRead = 0;
    std::size_t protectedVtableBytesHashed = 0;
    bool jipPresent = false;
    bool jipNormalizationApplied = false;
    bool showOffPresent = false;
};

struct RetailCompatibilityProof
{
    RetailCompatibilityEvidence evidence {};
    RetailCompatibilityDiagnostics diagnostics {};
    RetailCompatibilityFailure failure =
        RetailCompatibilityFailure::UnsupportedHostArchitecture;
    bool compatible = false;
};

// Enumerates and re-enumerates only the current process. Unknown modules are
// tolerated unless prohibited by name or they alter a protected engine range.
// There is no full-image or exact-module-count claim in this proof.
RetailCompatibilityProof proveCurrentRetailCompatibilityAtDecisionPoint()
    noexcept;

#if defined(FNVXR_RETAIL_COMPATIBILITY_PROOF_TEST_AUTHORITY)
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

struct SyntheticModule
{
    const wchar_t* baseName = nullptr;
    const wchar_t* path = nullptr;
    std::uintptr_t runtimeBase = 0;
    std::size_t runtimeBytes = 0;
    const std::uint8_t* exactFileBytes = nullptr;
    std::size_t exactFileByteCount = 0;
};

struct SyntheticProcessIdentity
{
    std::uint32_t processId = 0;
    std::uint64_t creationTime100ns = 0;
};

struct SyntheticExactModuleSeal
{
    const wchar_t* baseName = nullptr;
    std::uint64_t fileBytes = 0;
    Sha256Digest fileSha256 {};
    std::uint32_t loadedPeTimeDateStamp = 0;
    std::uint32_t loadedPeSizeOfImage = 0;
    std::uint32_t loadedPePreferredImageBase = 0;
};

struct SyntheticJipRewriteSeal
{
    std::uintptr_t functionPreferredAddress = 0;
    std::size_t functionBytes = 0;
    std::size_t callOffset = 0;
    std::uintptr_t stockTargetPreferredAddress = 0;
    std::uint32_t stubRva = 0;
    std::uint32_t guardVariableRva = 0;
    std::size_t stubBytes = 0;
};

struct SyntheticRetailCompatibilityContract
{
    std::uintptr_t preferredImageBase = 0;
    LoadedExecutableIdentity loadedIdentity {};
    std::uint32_t sizeOfImage = 0;
    SyntheticExactModuleSeal jip {};
    SyntheticExactModuleSeal showOff {};
    SyntheticJipRewriteSeal jipRewrite {};
    const LoadedFunctionManifestEntry* coreManifest = nullptr;
    std::size_t coreManifestCount = 0;
    const abi::RetailFunctionAbiDescriptor* functionInventory = nullptr;
    std::size_t functionInventoryCount = 0;
    const abi::RetailVtableSlotDescriptor* vtableSlots = nullptr;
    std::size_t vtableSlotCount = 0;
    const abi::RetailVtableBlockDescriptor* vtableBlocks = nullptr;
    std::size_t vtableBlockCount = 0;
};

struct SyntheticRetailCompatibilitySnapshot
{
    const SyntheticModule* modulesBefore = nullptr;
    std::size_t moduleCountBefore = 0;
    const SyntheticModule* modulesAfter = nullptr;
    std::size_t moduleCountAfter = 0;
    const SyntheticByteRange* byteRanges = nullptr;
    std::size_t byteRangeCount = 0;
    const SyntheticProtectionRange* protectionRanges = nullptr;
    std::size_t protectionRangeCount = 0;
    SyntheticProcessIdentity processBefore {};
    SyntheticProcessIdentity processAfter {};
};

RetailCompatibilityProof proveSyntheticRetailCompatibilityAtDecisionPoint(
    const SyntheticRetailCompatibilitySnapshot& snapshot,
    const SyntheticRetailCompatibilityContract& contract) noexcept;

Sha256Digest sha256ForSyntheticCompatibilityAuthority(
    const std::uint8_t* bytes,
    std::size_t byteCount) noexcept;
}
#endif
}
