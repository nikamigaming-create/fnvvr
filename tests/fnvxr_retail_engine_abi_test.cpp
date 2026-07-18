#include "fnvxr_retail_engine_abi.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <type_traits>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main()
{
    using namespace fnvxr::engine;
    using namespace fnvxr::engine::abi;

    require(sizeof(RetailNiFrustumLayout) == 0x1Cu, "NiFrustum must be 0x1C bytes");
    require(sizeof(RetailNiVisibleArrayLayout) == 0x10u, "NiVisibleArray must be 0x10 bytes");
    require(sizeof(RetailNiAccumulatorLayout) == 0x0Cu, "NiAccumulator must be 0x0C bytes");
    require(sizeof(RetailNiCameraLayout) == 0x114u, "NiCamera must be 0x114 bytes");
    require(sizeof(RetailSceneGraphLayout) == 0xC0u, "SceneGraph must be 0xC0 bytes");
    require(sizeof(RetailNiCullingProcessLayout) == 0x90u, "NiCullingProcess must be 0x90 bytes");
    require(sizeof(RetailBSCullingProcessLayout) == 0xC8u, "BSCullingProcess must be 0xC8 bytes");
    require(
        sizeof(RetailBSShaderAccumulatorLayout) == 0x280u,
        "BSShaderAccumulator must be 0x280 bytes");

    require(
        offsetof(RetailNiCameraLayout, worldToCamera) == 0x9Cu
            && offsetof(RetailNiCameraLayout, frustum) == 0xDCu
            && offsetof(RetailNiCameraLayout, viewport) == 0x100u,
        "NiCamera world transform, frustum, and viewport offsets must stay exact");
    require(
        offsetof(RetailSceneGraphLayout, camera) == 0xACu
            && offsetof(RetailSceneGraphLayout, visibleArray) == 0xB0u
            && offsetof(RetailSceneGraphLayout, cullingProcess) == 0xB4u,
        "SceneGraph camera, visibility, and culler offsets must stay exact");
    require(
        offsetof(RetailBSCullingProcessLayout, topCullMode) == 0x90u
            && offsetof(RetailBSCullingProcessLayout, shaderAccumulator) == 0xC4u,
        "BSCullingProcess derived fields must follow the 0x90-byte retail base");
    require(
        offsetof(RetailBSShaderAccumulatorLayout, batchRendererCount) == 0x18Cu
            && offsetof(RetailBSShaderAccumulatorLayout, renderMode) == 0x19Cu
            && offsetof(RetailBSShaderAccumulatorLayout, renderPassType) == 0x1A8u,
        "BSShaderAccumulator critical field offsets must stay exact");

    static_assert(std::is_pointer_v<NiAllocateFunction>);
    static_assert(std::is_pointer_v<NiFreeFunction>);
    static_assert(std::is_pointer_v<NiCameraCreateFunction>);
    static_assert(std::is_pointer_v<BSCullingProcessConstructorFunction>);
    static_assert(std::is_pointer_v<BSShaderAccumulatorConstructorFunction>);
    static_assert(std::is_pointer_v<AccumulatorAddVisibleArrayFunction>);
    static_assert(std::is_pointer_v<GeometryOnVisibleFunction>);
    static_assert(std::is_pointer_v<CullingProcessAltFunction>);
    static_assert(std::is_pointer_v<AccumulatorRenderFunction>);

    require(
        SceneGraphSingletonPointerAddress == 0x011DEB7Cu,
        "the SceneGraph singleton pointer address must stay exact");
    require(
        BSCullingProcessVtableAddress == 0x0101E2ECu
            && BSShaderAccumulatorVtableAddress == 0x010ADFF8u,
        "the retail vtable addresses must stay exact");

    require(
        retailFunctionInventoryStructurallyValid(),
        "every ABI range and x86 stack contract must be structurally valid");
    require(
        retailFunctionInventoryProductionProven(),
        "the independently captured ABI inventory must be production-proven");
    require(
        RetailFunctionAbiInventory.size() == 22u,
        "the complete ABI inventory must include the OnVisible dispatch thunk");

    for (std::size_t left = 0; left < RetailFunctionAbiInventory.size(); ++left)
    {
        const RetailFunctionAbiDescriptor& descriptor = RetailFunctionAbiInventory[left];
        require(structurallyValid(descriptor), "each ABI descriptor must be structurally valid");
        require(
            digestMatches(
                descriptor.sha256,
                descriptor.sha256.bytes.data(),
                descriptor.sha256.bytes.size()),
            "each exact ABI digest must decode and compare");

        const std::uintptr_t leftEnd = descriptor.preferredAddress + descriptor.byteCount;
        for (std::size_t right = left + 1u; right < RetailFunctionAbiInventory.size(); ++right)
        {
            const RetailFunctionAbiDescriptor& other = RetailFunctionAbiInventory[right];
            const std::uintptr_t rightEnd = other.preferredAddress + other.byteCount;
            require(
                leftEnd <= other.preferredAddress || rightEnd <= descriptor.preferredAddress,
                "exact ABI function bodies must not overlap");
        }
    }

    const auto findFunction = [](std::string_view name) {
        return std::find_if(
            RetailFunctionAbiInventory.begin(),
            RetailFunctionAbiInventory.end(),
            [name](const RetailFunctionAbiDescriptor& descriptor) {
                return descriptor.name && std::string_view(descriptor.name) == name;
            });
    };

    std::size_t coreFunctionsCrossChecked = 0u;
    for (const LoadedFunctionManifestEntry& core : RetailEngineManifest)
    {
        const auto abi = findFunction(core.name);
        if (abi == RetailFunctionAbiInventory.end())
            continue;

        ++coreFunctionsCrossChecked;
        require(
            abi->preferredAddress == core.preferredAddress
                && abi->byteCount == core.byteCount
                && abi->sha256.valid == core.sha256.valid
                && std::equal(
                    abi->sha256.bytes.begin(),
                    abi->sha256.bytes.end(),
                    core.sha256.bytes.begin()),
            "shared ABI and core-manifest identities must not drift apart");
    }
    require(
        coreFunctionsCrossChecked == 7u,
        "all seven core accumulator/culling ABI bodies must cross-check the manifest");

    const auto allocate = findFunction("Ni_Alloc");
    const auto free = findFunction("Ni_Free");
    const auto cameraCreate = findFunction("NiCamera::Create");
    require(
        allocate != RetailFunctionAbiInventory.end()
            && allocate->preferredAddress == 0x00AA13E0u
            && allocate->byteCount == 62u
            && allocate->callingConvention == RetailX86CallingConvention::Cdecl
            && allocate->stackArgumentCount == 1u
            && allocate->calleePopBytes == 0u,
        "Ni_Alloc must retain its exact cdecl address, body, and call frame");
    require(
        free != RetailFunctionAbiInventory.end()
            && free->preferredAddress == 0x00AA1460u
            && free->byteCount == 34u
            && free->stackArgumentCount == 2u
            && free->independentLoadedSamples == 2u,
        "Ni_Free must retain both independent loaded samples");
    require(
        cameraCreate != RetailFunctionAbiInventory.end()
            && cameraCreate->preferredAddress == 0x00A71430u
            && cameraCreate->byteCount == 27u
            && cameraCreate->independentLoadedSamples == 2u,
        "NiCamera::Create must retain both independent loaded-memory samples");

    const auto cullingConstructor = findFunction("BSCullingProcess::BSCullingProcess");
    const auto cullingDestructor = findFunction("BSCullingProcess::~BSCullingProcess body");
    require(
        cullingConstructor != RetailFunctionAbiInventory.end()
            && cullingConstructor->callingConvention == RetailX86CallingConvention::Thiscall
            && cullingConstructor->stackArgumentCount == 1u
            && cullingConstructor->calleePopBytes == 4u
            && cullingConstructor->argumentSemanticsProven,
        "the culler constructor stack ABI and zero argument semantics must stay exact");
    require(
        cullingDestructor != RetailFunctionAbiInventory.end()
            && cullingDestructor->preferredAddress == 0x004A0F60u
            && cullingDestructor->byteCount == 98u,
        "the culler destructor body must not be confused with its deleting thunk");

    const auto shaderConstructor = findFunction("BSShaderAccumulator::BSShaderAccumulator");
    const auto shaderDestructor = findFunction("BSShaderAccumulator scalar deleting destructor");
    require(
        shaderConstructor != RetailFunctionAbiInventory.end()
            && shaderConstructor->stackArgumentCount == 3u
            && shaderConstructor->calleePopBytes == 12u
            && shaderConstructor->byteCount == 1055u
            && shaderConstructor->argumentSemanticsProven,
        "the shader accumulator constructor must retain its exact stock call frame");
    require(
        shaderDestructor != RetailFunctionAbiInventory.end()
            && shaderDestructor->preferredAddress == 0x00B664F0u
            && shaderDestructor->byteCount == 35u
            && shaderDestructor->independentLoadedSamples == 2u,
        "the shader deleting destructor must stay independently hash-gated");

    const auto addVisibleArray = findFunction("NiAccumulator::AddVisibleArray");
    require(
        addVisibleArray != RetailFunctionAbiInventory.end()
            && addVisibleArray->preferredAddress == 0x00A9B790u
            && addVisibleArray->byteCount == 189u
            && addVisibleArray->preferredAddress + addVisibleArray->byteCount == 0x00A9B84Du,
        "AddVisibleArray must protect only its function body, not a neighboring cluster");

    const auto geometryOnVisible = findFunction("NiGeometry::OnVisible");
    require(
        geometryOnVisible != RetailFunctionAbiInventory.end()
            && geometryOnVisible->preferredAddress == 0x00A7FD90u
            && geometryOnVisible->byteCount == 17u
            && geometryOnVisible->sha256.bytes
                == sha256FromHex(
                       "C3A270DCBE479A05ED865C7AEB395E19CBADCEEB575486F12A5D6F94B7F8452C")
                       .bytes
            && geometryOnVisible->callingConvention
                == RetailX86CallingConvention::Thiscall
            && geometryOnVisible->stackArgumentCount == 1u
            && geometryOnVisible->calleePopBytes == 4u
            && geometryOnVisible->argumentSemanticsProven
            && geometryOnVisible->independentLoadedSamples == 2u
            && productionProven(*geometryOnVisible),
        "OnVisible must remain exact and independently loaded-sample proven");

    const auto secondBranch = findFunction("AccumulateSecondWorldBranch");
    require(
        secondBranch != RetailFunctionAbiInventory.end()
            && secondBranch->preferredAddress == 0x00B6BFC0u
            && secondBranch->byteCount == 264u
            && secondBranch->callingConvention == RetailX86CallingConvention::Cdecl
            && secondBranch->stackArgumentCount == 3u
            && secondBranch->argumentSemanticsProven
            && secondBranch->independentLoadedSamples == 2u,
        "the second stock branch must retain its captured scene-list call frame");

    const auto cullingProcess = findFunction("BSCullingProcess::Process");
    const auto cullingAppend = findFunction("BSCullingProcess::Append");
    require(
        cullingProcess != RetailFunctionAbiInventory.end()
            && cullingProcess->preferredAddress == 0x00C4EE90u
            && cullingProcess->byteCount == 479u
            && cullingProcess->sha256.bytes
                == sha256FromHex(
                       "7680B9EFC6E7DA6FB1E954A784096803515889F58E2720C38075E50AE3AF2AEF")
                       .bytes
            && cullingProcess->callingConvention == RetailX86CallingConvention::Thiscall
            && cullingProcess->stackArgumentCount == 1u
            && cullingProcess->calleePopBytes == 4u,
        "the culler Process body used by ProcessAlt must be an exact ABI entry");
    require(
        cullingAppend != RetailFunctionAbiInventory.end()
            && cullingAppend->preferredAddress == 0x00C4F1D0u
            && cullingAppend->byteCount == 150u
            && cullingAppend->sha256.bytes
                == sha256FromHex(
                       "7E198CD639B8CA514E2427DD3524300DF7D9CAF2D5181B915D917210FACFC9BE")
                       .bytes
            && cullingAppend->callingConvention == RetailX86CallingConvention::Thiscall
            && cullingAppend->stackArgumentCount == 1u
            && cullingAppend->calleePopBytes == 4u,
        "the culler Append body that controls immediate rendering must be an exact ABI entry");

    for (const std::string_view name : {
             std::string_view("RenderAccumulatorWithoutFinalize"),
             std::string_view("FinalizeAccumulator"),
             std::string_view("RenderAndFinalizeAccumulator"),
         })
    {
        const auto render = findFunction(name);
        require(
            render != RetailFunctionAbiInventory.end()
                && render->callingConvention == RetailX86CallingConvention::Cdecl
                && render->stackArgumentCount == 3u
                && render->calleePopBytes == 0u,
            "stock accumulator render callers must retain all three cdecl stack arguments");
    }

    const auto renderVirtual = findFunction("BSShaderAccumulator::Render");
    const auto finalizeVirtual = findFunction("BSShaderAccumulator::Finalize");
    require(
        renderVirtual != RetailFunctionAbiInventory.end()
            && renderVirtual->preferredAddress == 0x00B66520u
            && renderVirtual->byteCount == 68u
            && renderVirtual->callingConvention == RetailX86CallingConvention::Thiscall
            && renderVirtual->stackArgumentCount == 0u,
        "the render wrapper's +0xA8 virtual target must be hash-protected");
    require(
        finalizeVirtual != RetailFunctionAbiInventory.end()
            && finalizeVirtual->preferredAddress == 0x00B66570u
            && finalizeVirtual->byteCount == 41u
            && finalizeVirtual->callingConvention == RetailX86CallingConvention::Thiscall
            && finalizeVirtual->stackArgumentCount == 0u,
        "the finalize wrapper's +0xAC virtual target must be hash-protected");

    RetailFunctionAbiDescriptor raisedSampleCount = *free;
    raisedSampleCount.independentLoadedSamples = MinimumIndependentLoadedSamples - 1u;
    require(
        !productionProven(raisedSampleCount),
        "a complete descriptor must fail below the independent sample threshold");
    raisedSampleCount.independentLoadedSamples = MinimumIndependentLoadedSamples;
    require(
        productionProven(raisedSampleCount),
        "a complete descriptor becomes proven at the independent sample threshold");
    raisedSampleCount.argumentSemanticsProven = false;
    require(
        !productionProven(raisedSampleCount),
        "sample count must not substitute for proven argument semantics");

    RetailFunctionAbiDescriptor badStack = *cullingConstructor;
    badStack.calleePopBytes = 0u;
    require(
        !structurallyValid(badStack),
        "a thiscall descriptor with the wrong callee cleanup must fail closed");

    require(RetailVtableSlots.size() == 16u, "every known critical vtable slot must be explicit");
    for (std::size_t left = 0u; left < RetailVtableSlots.size(); ++left)
    {
        const RetailVtableSlotDescriptor& slot = RetailVtableSlots[left];
        require(
            slot.owner && slot.owner[0] != '\0'
                && slot.slotByteOffset % sizeof(RetailPointer32) == 0u
                && imageContains(slot.vtableAddress + slot.slotByteOffset, sizeof(RetailPointer32))
                && imageContains(slot.preferredTargetAddress, 1u),
            "every vtable slot and target must be aligned and contained in the image");
        for (std::size_t right = left + 1u; right < RetailVtableSlots.size(); ++right)
        {
            const RetailVtableSlotDescriptor& other = RetailVtableSlots[right];
            require(
                slot.vtableAddress != other.vtableAddress
                    || slot.slotByteOffset != other.slotByteOffset,
                "a retail vtable slot must have exactly one recorded target");
        }
    }
    const auto addVisibleSlot = std::find_if(
        RetailVtableSlots.begin(),
        RetailVtableSlots.end(),
        [](const RetailVtableSlotDescriptor& slot) {
            return slot.vtableAddress == BSShaderAccumulatorVtableAddress
                && slot.slotByteOffset == 0x94u;
        });
    require(
        addVisibleSlot != RetailVtableSlots.end()
            && addVisibleSlot->preferredTargetAddress == 0x00A9B790u,
        "BSShaderAccumulator slot +0x94 must resolve to AddVisibleArray");
    const auto processAltSlot = std::find_if(
        RetailVtableSlots.begin(),
        RetailVtableSlots.end(),
        [](const RetailVtableSlotDescriptor& slot) {
            return slot.vtableAddress == BSCullingProcessVtableAddress
                && slot.slotByteOffset == 0x48u;
        });
    require(
        processAltSlot != RetailVtableSlots.end()
            && processAltSlot->preferredTargetAddress == 0x00C4F070u,
        "BSCullingProcess slot +0x48 must resolve to ProcessAlt");
    const auto processSlot = std::find_if(
        RetailVtableSlots.begin(),
        RetailVtableSlots.end(),
        [](const RetailVtableSlotDescriptor& slot) {
            return slot.vtableAddress == BSCullingProcessVtableAddress
                && slot.slotByteOffset == 0x44u;
        });
    const auto appendSlot = std::find_if(
        RetailVtableSlots.begin(),
        RetailVtableSlots.end(),
        [](const RetailVtableSlotDescriptor& slot) {
            return slot.vtableAddress == BSCullingProcessVtableAddress
                && slot.slotByteOffset == 0x4Cu;
        });
    require(
        processSlot != RetailVtableSlots.end()
            && processSlot->preferredTargetAddress == 0x00C4EE90u,
        "BSCullingProcess slot +0x44 must resolve to Process");
    require(
        appendSlot != RetailVtableSlots.end()
            && appendSlot->preferredTargetAddress == 0x00C4F1D0u,
        "BSCullingProcess slot +0x4C must resolve to Append");

    require(
        RetailVtableBlocks.size() == 1u,
        "the complete BSCullingProcess vtable block must have one canonical descriptor");
    const RetailVtableBlockDescriptor& cullingVtableBlock = RetailVtableBlocks.front();
    require(
        structurallyValid(cullingVtableBlock)
            && cullingVtableBlock.owner
            && std::string_view(cullingVtableBlock.owner) == "BSCullingProcess"
            && cullingVtableBlock.preferredAddress == BSCullingProcessVtableAddress
            && cullingVtableBlock.byteCount == 0x50u
            && cullingVtableBlock.sha256.bytes
                == sha256FromHex(
                       "7F1CEFA617A2A3A0F07D944C0233436C9FEED2A2B3FA5AF379730D079D3EF838")
                       .bytes
            && cullingVtableBlock.independentLoadedSamples == 2u,
        "the inherited culler vtable bytes must remain one exact retail block");
    require(
        retailVtableBlockInventoryProductionProven(),
        "the cloned dispatch table requires both independent loaded vtable samples");

    const RetailEngineAbiAssessment emptyAssessment = assessRetailEngineAbi({});
    require(
        !emptyAssessment.engineCallsAuthorized
            && emptyAssessment.failure == RetailEngineAbiFailure::UnsupportedExecutable,
        "empty ABI evidence must fail at the executable identity gate");

    RetailEngineAbiEvidence completeDynamicEvidence {};
    completeDynamicEvidence.loadedExecutableIdentityMatched = true;
    completeDynamicEvidence.loadedExecutableSectionLayoutAndProtectionsVerified = true;
    completeDynamicEvidence.coreManifestMatched = true;
    completeDynamicEvidence.fullFunctionInventoryMatched = true;
    completeDynamicEvidence.vtableSlotsMatched = true;
    completeDynamicEvidence.vtableBlocksMatched = true;
    completeDynamicEvidence.liveObjectLayoutsVerified = true;
    completeDynamicEvidence.constructorOwnershipVerified = true;
    completeDynamicEvidence.bothWorldBranchesVerified = true;
    completeDynamicEvidence.compatibilityModulesVerified = true;
    completeDynamicEvidence.synchronousRuntimeRevalidation = true;
    const RetailEngineAbiAssessment completeAssessment =
        assessRetailEngineAbi(completeDynamicEvidence);
    require(
        completeAssessment.engineCallsAuthorized
            && completeAssessment.failure == RetailEngineAbiFailure::None,
        "complete static and synchronous dynamic evidence must authorize engine calls");

    RetailEngineAbiEvidence missingLoadedSectionLayout = completeDynamicEvidence;
    missingLoadedSectionLayout.loadedExecutableSectionLayoutAndProtectionsVerified = false;
    const RetailEngineAbiAssessment missingLoadedSectionLayoutAssessment =
        assessRetailEngineAbiWithStaticInventory(
            missingLoadedSectionLayout,
            true,
            true);
    require(
        !missingLoadedSectionLayoutAssessment.engineCallsAuthorized
            && missingLoadedSectionLayoutAssessment.failure
                == RetailEngineAbiFailure::LoadedExecutableSectionLayoutUnverified,
        "disk identity must not substitute for exact loaded executable-section geometry and protections");

    RetailEngineAbiEvidence missingRuntimeInventory = completeDynamicEvidence;
    missingRuntimeInventory.fullFunctionInventoryMatched = false;
    const RetailEngineAbiAssessment missingRuntimeInventoryAssessment =
        assessRetailEngineAbiWithStaticInventory(missingRuntimeInventory, true, true);
    require(
        !missingRuntimeInventoryAssessment.engineCallsAuthorized
            && missingRuntimeInventoryAssessment.failure
                == RetailEngineAbiFailure::RuntimeFunctionInventoryUnverified,
        "static ABI descriptors must never substitute for live function hashes");

    RetailEngineAbiEvidence missingVtableSlots = completeDynamicEvidence;
    missingVtableSlots.vtableSlotsMatched = false;
    const RetailEngineAbiAssessment missingVtableAssessment =
        assessRetailEngineAbiWithStaticInventory(missingVtableSlots, true, true);
    require(
        !missingVtableAssessment.engineCallsAuthorized
            && missingVtableAssessment.failure
                == RetailEngineAbiFailure::RuntimeVtableSlotsUnverified,
        "matching wrapper bytes must not authorize rewritten virtual dispatch slots");

    RetailEngineAbiEvidence missingVtableBlocks = completeDynamicEvidence;
    missingVtableBlocks.vtableBlocksMatched = false;
    const RetailEngineAbiAssessment missingVtableBlockAssessment =
        assessRetailEngineAbiWithStaticInventory(missingVtableBlocks, true, true);
    require(
        !missingVtableBlockAssessment.engineCallsAuthorized
            && missingVtableBlockAssessment.failure
                == RetailEngineAbiFailure::RuntimeVtableBlocksUnverified,
        "matching named slots must not substitute for the complete inherited vtable block");

    const RetailEngineAbiAssessment missingStaticVtableBlockAssessment =
        assessRetailEngineAbiWithStaticInventory(
            completeDynamicEvidence,
            true,
            false);
    require(
        !missingStaticVtableBlockAssessment.engineCallsAuthorized
            && missingStaticVtableBlockAssessment.failure
                == RetailEngineAbiFailure::StaticVtableBlockInventoryIncomplete,
        "runtime block hashes must not substitute for independent static vtable samples");

    const RetailEngineAbiAssessment syntheticCompleteAssessment =
        assessRetailEngineAbiWithStaticInventory(completeDynamicEvidence, true, true);
    require(
        syntheticCompleteAssessment.engineCallsAuthorized
            && syntheticCompleteAssessment.failure == RetailEngineAbiFailure::None,
        "the injectable assessment must expose every dynamic gate to deterministic tests");

    std::cout << "retail engine ABI evidence contract passed\n";
    return EXIT_SUCCESS;
}
