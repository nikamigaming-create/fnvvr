#pragma once

#include "fnvxr_retail_engine_abi.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace fnvxr::engine::compatibility
{
// Exact loaded-image RVAs from the pinned JohnnyGuitar 5.28 module. Keep the
// production proof and its synthetic rewrite-group test on one source of
// truth; these are virtual addresses, not raw-file offsets.
inline constexpr std::uint32_t JohnnyGuitar528RenderFirstPersonHookRva =
    0x00029C20u;
inline constexpr std::uint32_t JohnnyGuitar528UpdateCameraOverwriteHookRva =
    0x00022D80u;
inline constexpr std::uint32_t JohnnyGuitar528UpdateCameraFirstCallHookRva =
    0x00022590u;
inline constexpr std::uint32_t JohnnyGuitar528UpdateCameraSecondCallHookRva =
    0x000225E0u;

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
    DuplicateJipModule = 9,
    Jip5730IdentityMismatch,
    JipRenderFirstPersonRewriteMismatch,
    DuplicateShowOffModule,
    ShowOff184IdentityMismatch,
    ProtectedCoreBodyMismatch,
    ProtectedFunctionMismatch,
    ProtectedVtableSlotMismatch,
    ProtectedVtableBlockMismatch,
    ProtectedMemoryUnstable,
    DuplicateJohnnyGuitarModule,
    JohnnyGuitar528IdentityMismatch,
    JohnnyGuitarProtectedRewriteMismatch,
};

enum class RetailRenderFirstPersonProofStage : std::uint8_t
{
    NotEvaluated = 0,
    ContractRejected,
    FunctionAccessRejected,
    FunctionReadRejected,
    RawStockMatched,
    JipTargetRejected,
    JipRewriteRejected,
    JipNormalized,
    JipOnlyMatched,
    JohnnyGuitarUnavailable,
    JohnnyGuitarTargetRejected,
    JohnnyGuitarRewriteRejected,
    JohnnyGuitarDigestRejected,
    FullyNormalized,
};

struct RetailCompatibilityEvidence
{
    bool retailExecutableIdentityMatched = false;
    bool moduleSnapshotStable = false;
    bool jip5730ExactOrAbsent = false;
    bool johnnyGuitar528ExactOrAbsent = false;
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
    bool johnnyGuitarPresent = false;
    bool johnnyGuitarNormalizationApplied = false;
    bool showOffPresent = false;
    RetailRenderFirstPersonProofStage renderFirstPersonProofStage =
        RetailRenderFirstPersonProofStage::NotEvaluated;
    std::uintptr_t observedJipRenderFirstPersonTarget = 0;
    std::uintptr_t expectedJipRenderFirstPersonTarget = 0;
    std::uintptr_t observedJohnnyGuitarRenderFirstPersonTarget = 0;
    std::uintptr_t expectedJohnnyGuitarRenderFirstPersonTarget = 0;
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
// tolerated unless they alter a protected engine range. Canonical path,
// signature, and mapped-code validation for the complete module census is a
// separate mandatory module-inventory proof; this proof independently demands
// stable modules and exact protected retail/JIP/ShowOff bytes. There is no
// full-image or exact-module-count claim in this proof.
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

// Exact JohnnyGuitar 5.28 rewrites observed in the protected game bodies.
// Every listed target is module-relative so the verifier can normalize only
// the proven ASLR-dependent layout back to its stock engine bytes.
struct SyntheticJohnnyGuitarRewriteSeal
{
    std::uintptr_t renderFirstPersonPreferredAddress = 0;
    std::size_t renderFirstPersonBytes = 0;
    std::size_t renderFirstPersonCallOffset = 0;
    std::uintptr_t renderFirstPersonStockTargetPreferredAddress = 0;
    std::uintptr_t updateCameraPreferredAddress = 0;
    std::size_t updateCameraBytes = 0;
    std::size_t updateCameraOverwriteOffset = 0;
    std::array<std::uint8_t, 5> updateCameraOriginalBytes {};
    std::size_t updateCameraFirstCallOffset = 0;
    std::uintptr_t updateCameraFirstStockTargetPreferredAddress = 0;
    std::size_t updateCameraSecondCallOffset = 0;
    std::uintptr_t updateCameraSecondStockTargetPreferredAddress = 0;
    std::uint32_t renderFirstPersonHookRva = 0;
    std::uint32_t updateCameraOverwriteHookRva = 0;
    std::uint32_t updateCameraFirstCallHookRva = 0;
    std::uint32_t updateCameraSecondCallHookRva = 0;
};

struct SyntheticRetailCompatibilityContract
{
    std::uintptr_t preferredImageBase = 0;
    LoadedExecutableIdentity loadedIdentity {};
    std::uint32_t sizeOfImage = 0;
    SyntheticExactModuleSeal jip {};
    SyntheticExactModuleSeal johnnyGuitar {};
    SyntheticExactModuleSeal showOff {};
    SyntheticJipRewriteSeal jipRewrite {};
    SyntheticJohnnyGuitarRewriteSeal johnnyGuitarRewrite {};
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
