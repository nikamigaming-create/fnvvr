#include "fnvxr_retail_abi_revalidator.h"
#include "fnvxr_windows_module_census.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string_view>
#include <vector>

namespace
{
using namespace fnvxr::engine;
using namespace fnvxr::engine::abi;
using namespace fnvxr::engine::abi::revalidation;
using namespace fnvxr::engine::abi::revalidation::testing;
using namespace fnvxr::inventory;

[[noreturn]] void fail(const char* message)
{
    std::cerr << message << '\n';
    std::exit(1);
}

void require(bool condition, const char* message)
{
    if (!condition)
        fail(message);
}

constexpr std::uintptr_t PreferredImageBase = 0x00400000u;
constexpr std::uintptr_t RuntimeImageBase = 0x00500000u;
constexpr std::uint32_t ImageBytes = 0x00020000u;
constexpr std::uint32_t HeaderBytes = 0x00001000u;
constexpr std::uint32_t PeHeaderBytes = 0x00000400u;
constexpr std::uint32_t TextRva = 0x00001000u;
constexpr std::uint32_t TextVirtualBytes = 0x0000A100u;
constexpr std::uint32_t TextRawBytes = 0x0000A200u;
constexpr std::uint32_t TextMappedBytes = TextRawBytes;
constexpr std::uint32_t TextProtectionBytes = 0x0000B000u;
constexpr std::uint32_t RdataRva = 0x0000C000u;
constexpr std::uint32_t RdataBytes = 0x00003000u;
constexpr std::uint32_t TimeDateStamp = 0x13572468u;
constexpr std::uint32_t Checksum = 0x24681357u;
constexpr std::uint32_t SceneSingletonRva = RdataRva + 0x300u;
constexpr std::uint32_t CullerVtableRva = RdataRva;
constexpr std::uintptr_t SceneAddress = 0x00600000u;
constexpr std::uintptr_t CameraAddress = 0x00601000u;
constexpr std::uintptr_t VisibleAddress = 0x00602000u;
constexpr std::uintptr_t CullerAddress = 0x00603000u;
constexpr std::uint32_t CoreFirstRva = 0x00002000u;
constexpr std::uint32_t FunctionFirstRva = 0x00004000u;
constexpr std::uint32_t FunctionStride = 0x40u;
constexpr std::size_t SyntheticFunctionBytes = 24u;
constexpr ProcessCreationIdentity SyntheticProcessIdentity {
    29072u,
    133812345678901234u
};
constexpr std::uint64_t SyntheticGeneration = 41u;
constexpr std::string_view SyntheticSha =
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

void writeU16(std::vector<std::uint8_t>& image, std::size_t offset,
    std::uint16_t value)
{
    require(offset <= image.size() && sizeof(value) <= image.size() - offset,
        "test PE16 write escaped the synthetic image");
    image[offset] = static_cast<std::uint8_t>(value & 0xFFu);
    image[offset + 1u] = static_cast<std::uint8_t>(value >> 8u);
}

void writeU32(std::vector<std::uint8_t>& image, std::size_t offset,
    std::uint32_t value)
{
    require(offset <= image.size() && sizeof(value) <= image.size() - offset,
        "test PE32 write escaped the synthetic image");
    for (std::size_t byte = 0; byte < sizeof(value); ++byte)
    {
        image[offset + byte] = static_cast<std::uint8_t>(
            (value >> (byte * 8u)) & 0xFFu);
    }
}

void writeSectionName(std::vector<std::uint8_t>& image, std::size_t offset,
    const char* name)
{
    for (std::size_t index = 0; index < 8u && name[index] != '\0'; ++index)
        image[offset + index] = static_cast<std::uint8_t>(name[index]);
}

void initializePe32(std::vector<std::uint8_t>& image)
{
    constexpr std::size_t PeOffset = 0x80u;
    constexpr std::size_t CoffOffset = PeOffset + 4u;
    constexpr std::size_t OptionalOffset = CoffOffset + 20u;
    constexpr std::size_t SectionTableOffset = OptionalOffset + 0xE0u;

    image[0] = 'M';
    image[1] = 'Z';
    writeU32(image, 0x3Cu, static_cast<std::uint32_t>(PeOffset));
    image[PeOffset] = 'P';
    image[PeOffset + 1u] = 'E';
    writeU16(image, CoffOffset, 0x014Cu);
    writeU16(image, CoffOffset + 2u, 2u);
    writeU32(image, CoffOffset + 4u, TimeDateStamp);
    writeU16(image, CoffOffset + 16u, 0xE0u);
    writeU16(image, CoffOffset + 18u, 0x010Fu);

    writeU16(image, OptionalOffset, 0x010Bu);
    writeU32(image, OptionalOffset + 4u, TextRawBytes);
    writeU32(image, OptionalOffset + 8u, RdataBytes);
    writeU32(image, OptionalOffset + 16u, CoreFirstRva);
    writeU32(image, OptionalOffset + 20u, TextRva);
    writeU32(image, OptionalOffset + 24u, RdataRva);
    writeU32(image, OptionalOffset + 28u,
        static_cast<std::uint32_t>(PreferredImageBase));
    writeU32(image, OptionalOffset + 32u, 0x1000u);
    writeU32(image, OptionalOffset + 36u, 0x200u);
    writeU32(image, OptionalOffset + 56u, ImageBytes);
    writeU32(image, OptionalOffset + 60u, PeHeaderBytes);
    writeU32(image, OptionalOffset + 64u, Checksum);
    writeU16(image, OptionalOffset + 68u, 2u);
    writeU32(image, OptionalOffset + 92u, 16u);

    writeSectionName(image, SectionTableOffset, ".text");
    writeU32(image, SectionTableOffset + 8u, TextVirtualBytes);
    writeU32(image, SectionTableOffset + 12u, TextRva);
    writeU32(image, SectionTableOffset + 16u, TextRawBytes);
    writeU32(image, SectionTableOffset + 20u, 0x400u);
    writeU32(image, SectionTableOffset + 36u, 0x60000020u);

    constexpr std::size_t RdataSection = SectionTableOffset + 40u;
    writeSectionName(image, RdataSection, ".rdata");
    writeU32(image, RdataSection + 8u, RdataBytes);
    writeU32(image, RdataSection + 12u, RdataRva);
    writeU32(image, RdataSection + 16u, RdataBytes);
    writeU32(image, RdataSection + 20u, 0xA600u);
    writeU32(image, RdataSection + 36u, 0x40000040u);
}

ObservedPeIdentity syntheticModuleIdentity(std::size_t index)
{
    const PeIdentity identity {
        PeMachineX86,
        static_cast<std::uint32_t>(0x10000000u + index),
        static_cast<std::uint32_t>(0x1000u + index * 0x100u),
        0x10000000u,
        static_cast<std::uint64_t>(0x2000u + index),
        SyntheticSha
    };
    return snapshotPeIdentity(identity);
}

std::vector<ModuleObservation> completeModuleSet()
{
    std::vector<ModuleObservation> modules;
    modules.reserve(AllowedModuleCount);
    for (std::size_t index = 0; index < AllowedModuleCount; ++index)
    {
        const ExpectedModule& expected = AllowedModules[index];
        ModuleObservation module {};
        module.origin = expected.origin;
        module.normalizedRelativePath = expected.normalizedRelativePath;
        module.normalizedModuleName = baseName(expected.normalizedRelativePath);
        module.identity = expected.exactIdentityRequired
            ? snapshotPeIdentity(expected.exactIdentity)
            : syntheticModuleIdentity(index);
        module.runtimeModuleBase = baseName(expected.normalizedRelativePath)
                == "falloutnv.exe"
            ? static_cast<std::uint32_t>(RuntimeImageBase)
            : static_cast<std::uint32_t>(
                0x01000000ull + index * 0x02000000ull);
        module.runtimeModuleBytes = module.identity.sizeOfImage;
        module.canonicalPathObserved = true;
        module.authenticodeValid = expected.origin != ModuleOrigin::GameTree;
        module.loadedExecutableSectionsMatched = true;
        modules.push_back(module);
    }
    return modules;
}

TransparentProxyPolicyEvidence completeProxyPolicy()
{
    TransparentProxyPolicyEvidence policy {};
    policy.processIdentity = SyntheticProcessIdentity;
    policy.evidenceGeneration = SyntheticGeneration;
    policy.d3dExactTransparentBuildIdentityMatched = true;
    policy.d3dMappedExecutableSectionsMatched = true;
    policy.d3dActivationSourceFuseFalse = true;
    policy.d3dProductIntegrationSourceFuseFalse = true;
    policy.d3dTransparentForwardingVerified = true;
    policy.dinputExactTransparentBuildIdentityMatched = true;
    policy.dinputMappedExecutableSectionsMatched = true;
    policy.dinputProductionSourceFuseFalse = true;
    policy.dinputProductIntegrationSourceFuseFalse = true;
    policy.dinputTransparentForwardingVerified = true;
    policy.xinputExactTransparentBuildIdentityMatched = true;
    policy.xinputMappedExecutableSectionsMatched = true;
    policy.xinputProductionSourceFuseFalse = true;
    policy.xinputProductIntegrationSourceFuseFalse = true;
    policy.xinputTransparentForwardingVerified = true;
    policy.noConfigurationOrSharedMemoryBypass = true;
    return policy;
}

JipCompatibilityProof completeJipProof()
{
    JipCompatibilityProof proof {};
    proof.processIdentity = SyntheticProcessIdentity;
    proof.evidenceGeneration = SyntheticGeneration;
    proof.disposition = ProofDisposition::Passed;
    proof.loadedExecutableSectionsMatched = true;
    proof.exactOwnedPatchInventoryMatched = true;
    proof.exactHookTargetMatched = true;
    proof.normalizedRetailManifestMatched = true;
    proof.noUnexpectedExecutableWrites = true;
    return proof;
}

ShowOffCompatibilityProof completeShowOffProof()
{
    ShowOffCompatibilityProof proof {};
    proof.processIdentity = SyntheticProcessIdentity;
    proof.evidenceGeneration = SyntheticGeneration;
    proof.disposition = ProofDisposition::Passed;
    proof.loadedExecutableSectionsMatched = true;
    proof.exactOwnedPatchInventoryMatched = true;
    proof.protectedRendererBytesMatched = true;
    proof.noUnexpectedExecutableWrites = true;
    return proof;
}

struct SyntheticFixture
{
    std::vector<std::uint8_t> image = std::vector<std::uint8_t>(ImageBytes);
    RetailSceneGraphLayout scene {};
    RetailNiCameraLayout camera {};
    RetailNiVisibleArrayLayout visible {};
    RetailBSCullingProcessLayout culler {};

    std::array<ExecutableSectionLayout, 1> executableSections {};
    std::array<LoadedFunctionManifestEntry, 13> coreManifest {};
    std::array<RetailFunctionAbiDescriptor, 22> functions {};
    std::array<RetailVtableSlotDescriptor, 16> vtableSlots {};
    std::array<RetailVtableBlockDescriptor, 1> vtableBlocks {};
    std::vector<ModuleObservation> modules = completeModuleSet();
    std::vector<ModuleObservation> postModules = completeModuleSet();
    ModuleInventoryEvidence census {};
    std::array<SyntheticByteRange, 5> byteRanges {};
    std::array<SyntheticProtectionRange, 7> protections {};
    SyntheticRetailAbiContract contract {};
    SyntheticRetailAbiSnapshot snapshot {};

    SyntheticFixture()
    {
        initializePe32(image);
        initializeFunctionBytes();
        initializeVtables();
        initializeLayouts();
        initializeCensus();
        initializeSeals();
        refreshViews();
    }

    std::uintptr_t runtimeAddress(std::uint32_t rva) const
    {
        return RuntimeImageBase + rva;
    }

    std::uintptr_t preferredAddress(std::uint32_t rva) const
    {
        return PreferredImageBase + rva;
    }

    void initializeFunctionBytes()
    {
        for (std::size_t index = 0; index < coreManifest.size(); ++index)
        {
            const std::uint32_t rva = CoreFirstRva
                + static_cast<std::uint32_t>(index * FunctionStride);
            for (std::size_t byte = 0; byte < SyntheticFunctionBytes; ++byte)
            {
                image[rva + byte] = static_cast<std::uint8_t>(
                    0x31u + index * 7u + byte * 3u);
            }
        }
        for (std::size_t index = 0; index < functions.size(); ++index)
        {
            const std::uint32_t rva = FunctionFirstRva
                + static_cast<std::uint32_t>(index * FunctionStride);
            for (std::size_t byte = 0; byte < SyntheticFunctionBytes; ++byte)
            {
                image[rva + byte] = static_cast<std::uint8_t>(
                    0xA7u + index * 11u + byte * 5u);
            }
        }
    }

    void initializeVtables()
    {
        for (std::size_t index = 0; index < vtableSlots.size(); ++index)
        {
            const std::uint32_t targetRva = FunctionFirstRva
                + static_cast<std::uint32_t>(index * FunctionStride);
            writeU32(image, CullerVtableRva + index * sizeof(RetailPointer32),
                static_cast<std::uint32_t>(runtimeAddress(targetRva)));
        }
        for (std::size_t offset = vtableSlots.size() * sizeof(RetailPointer32);
             offset < 0x50u; ++offset)
        {
            image[CullerVtableRva + offset] = static_cast<std::uint8_t>(
                0xD0u + offset);
        }
        writeU32(image, SceneSingletonRva,
            static_cast<std::uint32_t>(SceneAddress));
    }

    static RetailNiFrustumLayout validFrustum()
    {
        RetailNiFrustumLayout frustum {};
        frustum.left = -1.0f;
        frustum.right = 1.0f;
        frustum.top = 1.0f;
        frustum.bottom = -1.0f;
        frustum.nearDistance = 0.1f;
        frustum.farDistance = 10000.0f;
        frustum.orthographic = 0u;
        return frustum;
    }

    void initializeLayouts()
    {
        for (std::size_t row = 0; row < 4u; ++row)
            camera.worldToCamera[row * 4u + row] = 1.0f;
        camera.frustum = validFrustum();
        camera.minimumNearPlane = 0.01f;
        camera.maximumFarNearRatio = 100000.0f;
        camera.viewport.left = 0.0f;
        camera.viewport.right = 1.0f;
        camera.viewport.top = 0.0f;
        camera.viewport.bottom = 1.0f;
        camera.lodAdjust = 1.0f;

        visible.geometryPointers = 0u;
        visible.itemCount = 0u;
        visible.capacity = 0u;
        visible.growBy = 0u;

        scene.camera = static_cast<RetailPointer32>(CameraAddress);
        scene.visibleArray = static_cast<RetailPointer32>(VisibleAddress);
        scene.cullingProcess = static_cast<RetailPointer32>(CullerAddress);
        scene.isMenuSceneGraph = 0u;
        scene.cameraFov = 75.0f;

        culler.base.vtable = static_cast<RetailPointer32>(
            runtimeAddress(CullerVtableRva));
        culler.base.useAppendFunction = 1u;
        culler.base.visibleArray = static_cast<RetailPointer32>(VisibleAddress);
        culler.base.camera = static_cast<RetailPointer32>(CameraAddress);
        culler.base.frustum = validFrustum();
        culler.topCullMode = 0u;
        culler.cullModeStackSize = 0u;
        culler.compoundFrustum = 0u;
        culler.shaderAccumulator = 0u;
    }

    void initializeCensus()
    {
        census.enumerationComplete = true;
        census.usedToolhelp32ModuleAndModule32 = true;
        census.targetProcessIs32Bit = true;
        census.transactionProcessIdentity = SyntheticProcessIdentity;
        census.transactionGeneration = SyntheticGeneration;
        census.primaryProcessIdentity = SyntheticProcessIdentity;
        census.primaryEvidenceGeneration = SyntheticGeneration;
        census.proxyPolicy = completeProxyPolicy();
        census.jipProof = completeJipProof();
        census.showOffProof = completeShowOffProof();
        census.postProofEnumerationComplete = true;
        census.postProofUsedToolhelp32ModuleAndModule32 = true;
        census.postProofProcessIdentity = SyntheticProcessIdentity;
        census.postProofEvidenceGeneration = SyntheticGeneration;
        census.proofCompletionOrdinal = 900u;
        census.postProofEnumerationOrdinal = 901u;
        census.transactionBeginOrdinal = 902u;
    }

    void initializeSeals()
    {
        executableSections[0].name = {
            '.', 't', 'e', 'x', 't', 0u, 0u, 0u
        };
        executableSections[0].rva = TextRva;
        executableSections[0].virtualBytes = TextVirtualBytes;
        executableSections[0].mappedBytes = TextMappedBytes;
        executableSections[0].protectionBytes = TextProtectionBytes;
        executableSections[0].rawBytes = TextRawBytes;
        executableSections[0].characteristics = 0x60000020u;
        executableSections[0].independentLayoutSamples = 2u;

        for (std::size_t index = 0; index < coreManifest.size(); ++index)
        {
            const std::uint32_t rva = CoreFirstRva
                + static_cast<std::uint32_t>(index * FunctionStride);
            coreManifest[index] = {
                RetailEngineManifest[index].name,
                preferredAddress(rva),
                SyntheticFunctionBytes,
                sha256ForSyntheticAuthority(
                    image.data() + rva, SyntheticFunctionBytes)
            };
        }

        for (std::size_t index = 0; index < functions.size(); ++index)
        {
            const std::uint32_t rva = FunctionFirstRva
                + static_cast<std::uint32_t>(index * FunctionStride);
            functions[index] = RetailFunctionAbiInventory[index];
            functions[index].preferredAddress = preferredAddress(rva);
            functions[index].byteCount = SyntheticFunctionBytes;
            functions[index].sha256 = sha256ForSyntheticAuthority(
                image.data() + rva, SyntheticFunctionBytes);
        }

        for (std::size_t index = 0; index < vtableSlots.size(); ++index)
        {
            const std::uint32_t targetRva = FunctionFirstRva
                + static_cast<std::uint32_t>(index * FunctionStride);
            vtableSlots[index] = {
                "SyntheticBSCullingProcess",
                preferredAddress(CullerVtableRva),
                index * sizeof(RetailPointer32),
                preferredAddress(targetRva)
            };
        }

        vtableBlocks[0] = {
            "SyntheticBSCullingProcess",
            preferredAddress(CullerVtableRva),
            0x50u,
            sha256ForSyntheticAuthority(image.data() + CullerVtableRva, 0x50u),
            2u
        };

        contract.preferredImageBase = PreferredImageBase;
        contract.loadedIdentity = { TimeDateStamp, Checksum, ImageBytes };
        contract.sizeOfImage = ImageBytes;
        contract.executableSections = executableSections.data();
        contract.executableSectionCount = executableSections.size();
        contract.coreManifest = coreManifest.data();
        contract.coreManifestCount = coreManifest.size();
        contract.functionInventory = functions.data();
        contract.functionInventoryCount = functions.size();
        contract.vtableSlots = vtableSlots.data();
        contract.vtableSlotCount = vtableSlots.size();
        contract.vtableBlocks = vtableBlocks.data();
        contract.vtableBlockCount = vtableBlocks.size();
        contract.sceneGraphSingletonPointerAddress = preferredAddress(
            SceneSingletonRva);
        contract.bSCullingProcessVtableAddress = preferredAddress(
            CullerVtableRva);
    }

    void refreshViews()
    {
        census.modules = modules.data();
        census.moduleCount = modules.size();
        census.postProofModules = postModules.data();
        census.postProofModuleCount = postModules.size();

        byteRanges = {{
            { RuntimeImageBase, image.data(), image.size() },
            { SceneAddress, reinterpret_cast<const std::uint8_t*>(&scene),
                sizeof(scene) },
            { CameraAddress, reinterpret_cast<const std::uint8_t*>(&camera),
                sizeof(camera) },
            { VisibleAddress, reinterpret_cast<const std::uint8_t*>(&visible),
                sizeof(visible) },
            { CullerAddress, reinterpret_cast<const std::uint8_t*>(&culler),
                sizeof(culler) },
        }};
        protections = {{
            { RuntimeImageBase, HeaderBytes, SyntheticMemoryAccess::ReadOnly,
                false },
            { RuntimeImageBase + TextRva, TextProtectionBytes,
                SyntheticMemoryAccess::ExecuteRead, false },
            { RuntimeImageBase + RdataRva, RdataBytes,
                SyntheticMemoryAccess::ReadOnly, false },
            { SceneAddress, sizeof(scene), SyntheticMemoryAccess::ReadWrite,
                false },
            { CameraAddress, sizeof(camera), SyntheticMemoryAccess::ReadWrite,
                false },
            { VisibleAddress, sizeof(visible), SyntheticMemoryAccess::ReadWrite,
                false },
            { CullerAddress, sizeof(culler), SyntheticMemoryAccess::ReadWrite,
                false },
        }};

        snapshot.runtimeImageBase = RuntimeImageBase;
        snapshot.byteRanges = byteRanges.data();
        snapshot.byteRangeCount = byteRanges.size();
        snapshot.protectionRanges = protections.data();
        snapshot.protectionRangeCount = protections.size();
        snapshot.census = &census;
        snapshot.processIdentity = SyntheticProcessIdentity;
        snapshot.evidenceGeneration = SyntheticGeneration;
    }

    RetailAbiRevalidationResult evaluate()
    {
        refreshViews();
        return revalidateSyntheticRetailAbiAtDecisionPoint(snapshot, contract);
    }
};

void requireRejected(const RetailAbiRevalidationResult& result,
    const char* message)
{
    require(!result.assessment.engineCallsAuthorized, message);
    require(result.failure != RetailAbiRevalidationFailure::None, message);
}

void testCompleteDecisionPointDerivesEveryEvidenceField()
{
    SyntheticFixture fixture;
    const RetailAbiRevalidationResult result = fixture.evaluate();
    const RetailEngineAbiEvidence& evidence = result.evidence;

    require(evidence.loadedExecutableIdentityMatched,
        "the exact loaded PE identity was not derived");
    require(evidence.loadedExecutableSectionLayoutAndProtectionsVerified,
        "the exact executable-section layout and protections were not derived");
    require(evidence.coreManifestMatched,
        "the 13-entry core manifest was not derived");
    require(evidence.fullFunctionInventoryMatched,
        "the 22-entry function inventory was not derived");
    require(evidence.vtableSlotsMatched,
        "the 16 exact vtable slots were not derived");
    require(evidence.vtableBlocksMatched,
        "the complete 0x50 vtable block was not derived");
    require(evidence.liveObjectLayoutsVerified,
        "the live scene/camera/culler layouts were not derived");
    require(evidence.constructorOwnershipVerified,
        "sealed constructor ownership was not derived");
    require(evidence.bothWorldBranchesVerified,
        "sealed two-world-branch evidence was not derived");
    require(evidence.compatibilityModulesVerified,
        "the complete compatibility census was not derived");
    require(evidence.synchronousRuntimeRevalidation,
        "the same decision-point transaction was not derived");
    require(result.assessment.engineCallsAuthorized
            && result.assessment.failure == RetailEngineAbiFailure::None
            && result.failure == RetailAbiRevalidationFailure::None,
        "complete derived evidence did not authorize");
    require(result.diagnostics.loadedExecutableSectionCount == 1u,
        "the executable section census was not exact");
    require(result.diagnostics.executableSectionBytesInspected
            == TextProtectionBytes,
        "the complete executable protection range was not inspected");
    require(result.diagnostics.functionBodiesHashed
            == fixture.coreManifest.size() + fixture.functions.size(),
        "not every function body was hashed");
    require(result.diagnostics.vtableSlotsRead == 16u,
        "not every vtable slot was read");
    require(result.diagnostics.vtableBlockBytesHashed == 0x50u,
        "the complete vtable block was not hashed");
}

void testEveryFunctionAndVtableSlotIsLiveEvidence()
{
    SyntheticFixture fixture;
    for (std::size_t index = 0; index < fixture.functions.size(); ++index)
    {
        const std::uint32_t rva = FunctionFirstRva
            + static_cast<std::uint32_t>(index * FunctionStride);
        fixture.image[rva] ^= 0x5Au;
        const RetailAbiRevalidationResult rejected = fixture.evaluate();
        require(!rejected.evidence.fullFunctionInventoryMatched,
            "a mutated member of the 22-function inventory was accepted");
        requireRejected(rejected,
            "a mutated function inventory member authorized engine calls");
        fixture.image[rva] ^= 0x5Au;
    }

    for (std::size_t index = 0; index < fixture.vtableSlots.size(); ++index)
    {
        const std::size_t offset = CullerVtableRva
            + index * sizeof(RetailPointer32);
        fixture.image[offset] ^= 0x01u;
        const RetailAbiRevalidationResult rejected = fixture.evaluate();
        require(!rejected.evidence.vtableSlotsMatched,
            "a mutated member of the 16-slot inventory was accepted");
        requireRejected(rejected,
            "a mutated vtable slot authorized engine calls");
        fixture.image[offset] ^= 0x01u;
    }
}

void testIndependentSealsAndProtectionFailClosed()
{
    SyntheticFixture fixture;

    fixture.image[CoreFirstRva] ^= 0x80u;
    RetailAbiRevalidationResult rejected = fixture.evaluate();
    require(!rejected.evidence.coreManifestMatched,
        "a mutated core manifest body was accepted");
    requireRejected(rejected, "a mutated core manifest authorized calls");
    fixture.image[CoreFirstRva] ^= 0x80u;

    constexpr std::size_t VtablePaddingOffset = CullerVtableRva + 0x44u;
    fixture.image[VtablePaddingOffset] ^= 0x10u;
    rejected = fixture.evaluate();
    require(rejected.evidence.vtableSlotsMatched
            && !rejected.evidence.vtableBlocksMatched,
        "unnamed vtable-block bytes were not independently sealed");
    requireRejected(rejected, "a mutated complete vtable block authorized calls");
    fixture.image[VtablePaddingOffset] ^= 0x10u;

    constexpr std::size_t TextPagePadding = TextRva + TextMappedBytes + 0x40u;
    static_assert(TextPagePadding < TextRva + TextProtectionBytes);
    fixture.image[TextPagePadding] ^= 0x20u;
    rejected = fixture.evaluate();
    require(
        rejected.evidence.loadedExecutableSectionLayoutAndProtectionsVerified
            && rejected.assessment.engineCallsAuthorized,
        "unrelated mod-patched .text bytes were mistaken for protected engine-call content");
    fixture.image[TextPagePadding] ^= 0x20u;

    constexpr std::size_t TextSectionRawBytesOffset = 0x178u + 16u;
    writeU32(
        fixture.image,
        TextSectionRawBytesOffset,
        TextRawBytes + 0x200u);
    rejected = fixture.evaluate();
    require(
        !rejected.evidence.loadedExecutableSectionLayoutAndProtectionsVerified,
        "changed executable-section geometry was accepted");
    requireRejected(rejected, "changed executable-section geometry authorized calls");
    writeU32(fixture.image, TextSectionRawBytesOffset, TextRawBytes);

    fixture.protections[1].access = SyntheticMemoryAccess::ExecuteReadWrite;
    fixture.snapshot.protectionRanges = fixture.protections.data();
    rejected = revalidateSyntheticRetailAbiAtDecisionPoint(
        fixture.snapshot, fixture.contract);
    require(!rejected.evidence.loadedExecutableSectionLayoutAndProtectionsVerified,
        "a writable executable retail section was accepted");
    requireRejected(rejected, "writable executable memory authorized calls");
}

void testEveryLiveLayoutAndCensusRemainSynchronous()
{
    SyntheticFixture fixture;

    const RetailPointer32 originalCamera = fixture.scene.camera;
    fixture.scene.camera = 0u;
    RetailAbiRevalidationResult rejected = fixture.evaluate();
    require(!rejected.evidence.liveObjectLayoutsVerified,
        "a scene graph with no camera was accepted");
    requireRejected(rejected, "an invalid scene graph authorized calls");
    fixture.scene.camera = originalCamera;

    const float originalNear = fixture.camera.frustum.nearDistance;
    fixture.camera.frustum.nearDistance = -1.0f;
    rejected = fixture.evaluate();
    require(!rejected.evidence.liveObjectLayoutsVerified,
        "an invalid camera frustum was accepted");
    requireRejected(rejected, "an invalid camera layout authorized calls");
    fixture.camera.frustum.nearDistance = originalNear;

    const RetailPointer32 originalCullerCamera = fixture.culler.base.camera;
    fixture.culler.base.camera = static_cast<RetailPointer32>(SceneAddress);
    rejected = fixture.evaluate();
    require(!rejected.evidence.liveObjectLayoutsVerified,
        "a culler bound to the wrong camera was accepted");
    requireRejected(rejected, "an invalid culler layout authorized calls");
    fixture.culler.base.camera = originalCullerCamera;

    fixture.visible.itemCount = 1u;
    rejected = fixture.evaluate();
    require(!rejected.evidence.liveObjectLayoutsVerified,
        "a visible array with count above capacity was accepted");
    requireRejected(rejected, "an invalid visible array authorized calls");
    fixture.visible.itemCount = 0u;

    fixture.census.enumerationComplete = false;
    rejected = fixture.evaluate();
    require(!rejected.evidence.compatibilityModulesVerified,
        "an incomplete compatibility census was accepted");
    require(!rejected.evidence.synchronousRuntimeRevalidation,
        "an incomplete census was described as synchronous");
    requireRejected(rejected, "an incomplete compatibility census authorized calls");
    fixture.census.enumerationComplete = true;

    fixture.snapshot.processIdentity.creationTime100ns += 1u;
    rejected = revalidateSyntheticRetailAbiAtDecisionPoint(
        fixture.snapshot, fixture.contract);
    require(!rejected.evidence.synchronousRuntimeRevalidation,
        "a recycled-PID process identity was accepted");
    requireRejected(rejected, "a process identity mismatch authorized calls");
}

void testLoadedIdentityAndExactInventoryShapesFailClosed()
{
    SyntheticFixture fixture;
    writeU32(fixture.image, 0x80u + 4u + 4u, TimeDateStamp ^ 1u);
    RetailAbiRevalidationResult rejected = fixture.evaluate();
    require(!rejected.evidence.loadedExecutableIdentityMatched,
        "a changed loaded PE timestamp was accepted");
    requireRejected(rejected, "a changed loaded identity authorized calls");
    writeU32(fixture.image, 0x80u + 4u + 4u, TimeDateStamp);

    const std::size_t completeCount = fixture.contract.functionInventoryCount;
    fixture.contract.functionInventoryCount = completeCount - 1u;
    rejected = fixture.evaluate();
    require(!rejected.evidence.fullFunctionInventoryMatched,
        "a 21-entry function inventory was accepted as complete");
    requireRejected(rejected, "a truncated function inventory authorized calls");
    fixture.contract.functionInventoryCount = completeCount;

    const std::size_t completeSlotCount = fixture.contract.vtableSlotCount;
    fixture.contract.vtableSlotCount = completeSlotCount - 1u;
    rejected = fixture.evaluate();
    require(!rejected.evidence.vtableSlotsMatched,
        "a 15-entry vtable inventory was accepted as complete");
    requireRejected(rejected, "a truncated vtable inventory authorized calls");
}

void testProductionEntryCannotAuthorizeThisTestProcess()
{
    const RetailAbiRevalidationResult result =
        revalidateCurrentRetailEngineAbiAtDecisionPoint();
    requireRejected(result,
        "the production revalidator authorized a non-Fallout test process");
    require(!result.evidence.loadedExecutableIdentityMatched
            && !result.evidence.synchronousRuntimeRevalidation,
        "the production path synthesized retail evidence in the test process");
}
}

int main()
{
    static_assert(RetailEngineManifest.size() == 13u);
    static_assert(RetailFunctionAbiInventory.size() == 22u);
    static_assert(RetailVtableSlots.size() == 16u);
    static_assert(RetailVtableBlocks.size() == 1u);
    static_assert(sizeof(RetailBSCullingProcessLayout) == 0xC8u);
    static_assert(AllowedModuleCount == 124u);

    testCompleteDecisionPointDerivesEveryEvidenceField();
    testEveryFunctionAndVtableSlotIsLiveEvidence();
    testIndependentSealsAndProtectionFailClosed();
    testEveryLiveLayoutAndCensusRemainSynchronous();
    testLoadedIdentityAndExactInventoryShapesFailClosed();
    testProductionEntryCannotAuthorizeThisTestProcess();
    std::cout << "same-process retail ABI revalidator contract passed\n";
    return 0;
}
