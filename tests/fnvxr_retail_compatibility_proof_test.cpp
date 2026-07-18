#include "fnvxr_retail_compatibility_proof.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
using namespace fnvxr::engine;
using namespace fnvxr::engine::abi;
using namespace fnvxr::engine::compatibility;
using namespace fnvxr::engine::compatibility::testing;

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

constexpr std::uintptr_t PreferredMainBase = 0x00400000u;
constexpr std::uintptr_t RuntimeMainBase = 0x00500000u;
constexpr std::uint32_t MainImageBytes = 0x0107B000u;
constexpr std::uint32_t MainTimestamp = 0x12345678u;
constexpr std::uint32_t MainChecksum = 0x87654321u;
constexpr std::uint32_t MainTextRva = 0x00001000u;
constexpr std::uint32_t MainRdataRva = 0x00E00000u;
constexpr std::uint32_t CoreFirstRva = 0x00002000u;
constexpr std::uint32_t FunctionFirstRva = 0x00010000u;
constexpr std::uint32_t FunctionStride = 0x40u;
constexpr std::size_t SmallFunctionBytes = 24u;
constexpr std::uint32_t VtableRva = MainRdataRva + 0x100u;
constexpr std::uintptr_t JipBase = 0x20000000u;
constexpr std::uint32_t JipImageBytes = 0x00080000u;
constexpr std::uint32_t JipTimestamp = 0x11111111u;
constexpr std::uint32_t JipPreferredBase = 0x10000000u;
constexpr std::uint32_t JipStubRva = 0x00010880u;
constexpr std::uint32_t JipGuardRva = 0x0006A188u;
constexpr std::uintptr_t ShowOffBase = 0x30000000u;
constexpr std::uint32_t ShowOffImageBytes = 0x00110000u;
constexpr std::uint32_t ShowOffTimestamp = 0x22222222u;
constexpr std::uint32_t ShowOffPreferredBase = 0x10000000u;
constexpr std::uintptr_t RenderFirstPersonPreferred = 0x00875110u;
constexpr std::size_t RenderFirstPersonBytes = 3361u;
constexpr std::size_t JipCallOffset = 0x00B6u;
constexpr std::uintptr_t StockTargetPreferred = 0x007148C0u;
constexpr std::uint32_t UnknownTextRva = 0x00D00000u;

void writeU16(
    std::vector<std::uint8_t>& bytes,
    std::size_t offset,
    std::uint16_t value)
{
    require(offset <= bytes.size() && 2u <= bytes.size() - offset,
        "synthetic PE16 write escaped its image");
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1u] = static_cast<std::uint8_t>(value >> 8u);
}

void writeU32(
    std::vector<std::uint8_t>& bytes,
    std::size_t offset,
    std::uint32_t value)
{
    require(offset <= bytes.size() && 4u <= bytes.size() - offset,
        "synthetic PE32 write escaped its image");
    for (std::size_t index = 0u; index < 4u; ++index)
    {
        bytes[offset + index] = static_cast<std::uint8_t>(
            value >> (index * 8u));
    }
}

void initializeLoadedPe32(
    std::vector<std::uint8_t>& image,
    std::uint32_t timestamp,
    std::uint32_t checksum,
    std::uint32_t sizeOfImage,
    std::uint32_t preferredBase)
{
    constexpr std::size_t PeOffset = 0x80u;
    constexpr std::size_t CoffOffset = PeOffset + 4u;
    constexpr std::size_t OptionalOffset = CoffOffset + 20u;
    image[0] = 'M';
    image[1] = 'Z';
    writeU32(image, 0x3Cu, static_cast<std::uint32_t>(PeOffset));
    image[PeOffset] = 'P';
    image[PeOffset + 1u] = 'E';
    writeU16(image, CoffOffset, 0x014Cu);
    writeU16(image, CoffOffset + 2u, 1u);
    writeU32(image, CoffOffset + 4u, timestamp);
    writeU16(image, CoffOffset + 16u, 0xE0u);
    writeU16(image, OptionalOffset, 0x010Bu);
    writeU32(image, OptionalOffset + 28u, preferredBase);
    writeU32(image, OptionalOffset + 32u, 0x1000u);
    writeU32(image, OptionalOffset + 36u, 0x200u);
    writeU32(image, OptionalOffset + 56u, sizeOfImage);
    writeU32(image, OptionalOffset + 60u, 0x400u);
    writeU32(image, OptionalOffset + 64u, checksum);
}

std::array<std::uint8_t, 4> encodeRel32(
    std::uintptr_t instructionEnd,
    std::uintptr_t target)
{
    const auto displacement = static_cast<std::int32_t>(
        static_cast<std::int64_t>(target)
        - static_cast<std::int64_t>(instructionEnd));
    const auto encoded = static_cast<std::uint32_t>(displacement);
    return {
        static_cast<std::uint8_t>(encoded),
        static_cast<std::uint8_t>(encoded >> 8u),
        static_cast<std::uint8_t>(encoded >> 16u),
        static_cast<std::uint8_t>(encoded >> 24u),
    };
}

std::array<std::uint8_t, 19> expectedJipStub()
{
    std::array<std::uint8_t, 19> bytes {
        0x83u, 0x3Du, 0u, 0u, 0u, 0u, 0x00u, 0x74u, 0x07u,
        0xB8u, 0xC0u, 0x48u, 0x71u, 0x00u, 0xFFu, 0xE0u,
        0xC2u, 0x08u, 0x00u,
    };
    const auto guard = static_cast<std::uint32_t>(JipBase + JipGuardRva);
    bytes[2] = static_cast<std::uint8_t>(guard);
    bytes[3] = static_cast<std::uint8_t>(guard >> 8u);
    bytes[4] = static_cast<std::uint8_t>(guard >> 16u);
    bytes[5] = static_cast<std::uint8_t>(guard >> 24u);
    return bytes;
}

struct Fixture
{
    std::vector<std::uint8_t> mainImage =
        std::vector<std::uint8_t>(MainImageBytes);
    std::vector<std::uint8_t> jipImage =
        std::vector<std::uint8_t>(JipImageBytes);
    std::vector<std::uint8_t> showOffImage =
        std::vector<std::uint8_t>(ShowOffImageBytes);
    std::vector<std::uint8_t> jipFile =
        std::vector<std::uint8_t>(257u, 0x4Au);
    std::vector<std::uint8_t> showOffFile =
        std::vector<std::uint8_t>(389u, 0x53u);
    std::vector<std::uint8_t> stockFirstPerson =
        std::vector<std::uint8_t>(RenderFirstPersonBytes, 0x90u);

    std::array<LoadedFunctionManifestEntry, 13> coreManifest {};
    std::array<RetailFunctionAbiDescriptor, 22> functions {};
    std::array<RetailVtableSlotDescriptor, 16> slots {};
    std::array<RetailVtableBlockDescriptor, 1> blocks {};
    std::vector<SyntheticModule> modulesBefore {};
    std::vector<SyntheticModule> modulesAfter {};
    std::array<SyntheticByteRange, 3> byteRanges {};
    std::array<SyntheticProtectionRange, 6> protections {};
    SyntheticRetailCompatibilityContract contract {};
    SyntheticRetailCompatibilitySnapshot snapshot {};

    Fixture()
    {
        initializeLoadedPe32(
            mainImage,
            MainTimestamp,
            MainChecksum,
            MainImageBytes,
            static_cast<std::uint32_t>(PreferredMainBase));
        initializeLoadedPe32(
            jipImage,
            JipTimestamp,
            0u,
            JipImageBytes,
            JipPreferredBase);
        initializeLoadedPe32(
            showOffImage,
            ShowOffTimestamp,
            0u,
            ShowOffImageBytes,
            ShowOffPreferredBase);
        initializeProtectedFunctions();
        initializeVtables();
        initializeJipPatch();
        initializeModules();
        initializeContract();
        refreshViews();
    }

    std::uintptr_t preferred(std::uint32_t rva) const
    {
        return PreferredMainBase + rva;
    }

    std::uintptr_t runtime(std::uint32_t rva) const
    {
        return RuntimeMainBase + rva;
    }

    void initializeProtectedFunctions()
    {
        stockFirstPerson[JipCallOffset] = 0xE8u;
        const std::uintptr_t functionRuntime = RuntimeMainBase
            + (RenderFirstPersonPreferred - PreferredMainBase);
        const std::uintptr_t stockRuntime = RuntimeMainBase
            + (StockTargetPreferred - PreferredMainBase);
        const auto stockDisplacement = encodeRel32(
            functionRuntime + JipCallOffset + 5u,
            stockRuntime);
        for (std::size_t index = 0u; index < stockDisplacement.size(); ++index)
        {
            stockFirstPerson[JipCallOffset + 1u + index] =
                stockDisplacement[index];
        }

        for (std::size_t index = 0u; index < coreManifest.size(); ++index)
        {
            if (index == 2u)
            {
                coreManifest[index] = {
                    "RenderFirstPerson",
                    RenderFirstPersonPreferred,
                    stockFirstPerson.size(),
                    sha256ForSyntheticCompatibilityAuthority(
                        stockFirstPerson.data(),
                        stockFirstPerson.size()),
                };
                continue;
            }
            const std::uint32_t rva = CoreFirstRva
                + static_cast<std::uint32_t>(index * 0x80u);
            for (std::size_t byte = 0u; byte < SmallFunctionBytes; ++byte)
            {
                mainImage[rva + byte] = static_cast<std::uint8_t>(
                    index * 13u + byte + 1u);
            }
            coreManifest[index] = {
                "SyntheticCore",
                preferred(rva),
                SmallFunctionBytes,
                sha256ForSyntheticCompatibilityAuthority(
                    mainImage.data() + rva,
                    SmallFunctionBytes),
            };
        }

        for (std::size_t index = 0u; index < functions.size(); ++index)
        {
            const std::uint32_t rva = FunctionFirstRva
                + static_cast<std::uint32_t>(index * FunctionStride);
            for (std::size_t byte = 0u; byte < SmallFunctionBytes; ++byte)
            {
                mainImage[rva + byte] = static_cast<std::uint8_t>(
                    0x40u + index * 7u + byte);
            }
            functions[index] = {
                "SyntheticAbiFunction",
                preferred(rva),
                SmallFunctionBytes,
                sha256ForSyntheticCompatibilityAuthority(
                    mainImage.data() + rva,
                    SmallFunctionBytes),
                RetailX86CallingConvention::Cdecl,
                0u,
                0u,
                true,
                true,
                true,
                2u,
            };
        }
    }

    void initializeVtables()
    {
        for (std::size_t index = 0u; index < slots.size(); ++index)
        {
            const std::uint32_t targetRva = FunctionFirstRva
                + static_cast<std::uint32_t>(index * FunctionStride);
            slots[index] = {
                "SyntheticVtable",
                preferred(VtableRva),
                index * sizeof(RetailPointer32),
                preferred(targetRva),
            };
            writeU32(
                mainImage,
                VtableRva + index * sizeof(RetailPointer32),
                static_cast<std::uint32_t>(runtime(targetRva)));
        }
        for (std::size_t index = slots.size() * sizeof(RetailPointer32);
             index < 0x50u;
             ++index)
        {
            mainImage[VtableRva + index] = static_cast<std::uint8_t>(
                0xA0u + index);
        }
        blocks[0] = {
            "SyntheticVtable",
            preferred(VtableRva),
            0x50u,
            sha256ForSyntheticCompatibilityAuthority(
                mainImage.data() + VtableRva,
                0x50u),
            2u,
        };
    }

    void initializeJipPatch()
    {
        const std::uint32_t functionRva = static_cast<std::uint32_t>(
            RenderFirstPersonPreferred - PreferredMainBase);
        std::copy(
            stockFirstPerson.begin(),
            stockFirstPerson.end(),
            mainImage.begin() + functionRva);
        const std::uintptr_t functionRuntime = RuntimeMainBase + functionRva;
        const auto jipDisplacement = encodeRel32(
            functionRuntime + JipCallOffset + 5u,
            JipBase + JipStubRva);
        for (std::size_t index = 0u; index < jipDisplacement.size(); ++index)
        {
            mainImage[functionRva + JipCallOffset + 1u + index] =
                jipDisplacement[index];
        }
        const auto stub = expectedJipStub();
        std::copy(
            stub.begin(),
            stub.end(),
            jipImage.begin() + JipStubRva);
    }

    void initializeModules()
    {
        modulesBefore = {
            {
                L"FalloutNV.exe",
                L"C:\\Games\\FalloutNV.exe",
                RuntimeMainBase,
                mainImage.size(),
                nullptr,
                0u,
            },
            {
                L"jip_nvse.dll",
                L"C:\\Games\\Data\\NVSE\\Plugins\\jip_nvse.dll",
                JipBase,
                jipImage.size(),
                jipFile.data(),
                jipFile.size(),
            },
            {
                L"ShowOffNVSE.dll",
                L"C:\\Games\\Data\\NVSE\\Plugins\\ShowOffNVSE.dll",
                ShowOffBase,
                showOffImage.size(),
                showOffFile.data(),
                showOffFile.size(),
            },
            {
                L"unrelated_mod.dll",
                L"C:\\Games\\Data\\NVSE\\Plugins\\unrelated_mod.dll",
                0x35000000u,
                0x2000u,
                nullptr,
                0u,
            },
        };
        modulesAfter = modulesBefore;
    }

    void initializeContract()
    {
        contract.preferredImageBase = PreferredMainBase;
        contract.loadedIdentity = {
            MainTimestamp,
            MainChecksum,
            MainImageBytes,
        };
        contract.sizeOfImage = MainImageBytes;
        contract.jip = {
            L"jip_nvse.dll",
            jipFile.size(),
            sha256ForSyntheticCompatibilityAuthority(
                jipFile.data(),
                jipFile.size()),
            JipTimestamp,
            JipImageBytes,
            JipPreferredBase,
        };
        contract.showOff = {
            L"showoffnvse.dll",
            showOffFile.size(),
            sha256ForSyntheticCompatibilityAuthority(
                showOffFile.data(),
                showOffFile.size()),
            ShowOffTimestamp,
            ShowOffImageBytes,
            ShowOffPreferredBase,
        };
        contract.jipRewrite = {
            RenderFirstPersonPreferred,
            RenderFirstPersonBytes,
            JipCallOffset,
            StockTargetPreferred,
            JipStubRva,
            JipGuardRva,
            19u,
        };
        contract.coreManifest = coreManifest.data();
        contract.coreManifestCount = coreManifest.size();
        contract.functionInventory = functions.data();
        contract.functionInventoryCount = functions.size();
        contract.vtableSlots = slots.data();
        contract.vtableSlotCount = slots.size();
        contract.vtableBlocks = blocks.data();
        contract.vtableBlockCount = blocks.size();
    }

    void refreshViews()
    {
        byteRanges = {{
            { RuntimeMainBase, mainImage.data(), mainImage.size() },
            { JipBase, jipImage.data(), jipImage.size() },
            { ShowOffBase, showOffImage.data(), showOffImage.size() },
        }};
        protections = {{
            { RuntimeMainBase, 0x1000u, SyntheticMemoryAccess::ReadOnly, false },
            { RuntimeMainBase + MainTextRva, MainRdataRva - MainTextRva,
                SyntheticMemoryAccess::ExecuteRead, false },
            { RuntimeMainBase + MainRdataRva,
                MainImageBytes - MainRdataRva,
                SyntheticMemoryAccess::ReadOnly, false },
            { JipBase, 0x1000u, SyntheticMemoryAccess::ReadOnly, false },
            { JipBase + 0x1000u, JipImageBytes - 0x1000u,
                SyntheticMemoryAccess::ExecuteRead, false },
            { ShowOffBase, ShowOffImageBytes,
                SyntheticMemoryAccess::ReadOnly, false },
        }};
        snapshot.modulesBefore = modulesBefore.data();
        snapshot.moduleCountBefore = modulesBefore.size();
        snapshot.modulesAfter = modulesAfter.data();
        snapshot.moduleCountAfter = modulesAfter.size();
        snapshot.byteRanges = byteRanges.data();
        snapshot.byteRangeCount = byteRanges.size();
        snapshot.protectionRanges = protections.data();
        snapshot.protectionRangeCount = protections.size();
        snapshot.processBefore = { 41076u, 133812345678901234u };
        snapshot.processAfter = snapshot.processBefore;
    }

    RetailCompatibilityProof evaluate()
    {
        refreshViews();
        return proveSyntheticRetailCompatibilityAtDecisionPoint(
            snapshot,
            contract);
    }

    void restoreStockFirstPerson()
    {
        const std::uint32_t rva = static_cast<std::uint32_t>(
            RenderFirstPersonPreferred - PreferredMainBase);
        std::copy(
            stockFirstPerson.begin(),
            stockFirstPerson.end(),
            mainImage.begin() + rva);
    }
};

void requireRejected(
    const RetailCompatibilityProof& proof,
    const char* message)
{
    require(!proof.compatible, message);
    require(proof.failure != RetailCompatibilityFailure::None, message);
}

void testExactOptionalModulesAndJipNormalizationAuthorize()
{
    Fixture fixture;
    const RetailCompatibilityProof proof = fixture.evaluate();
    require(proof.compatible
            && proof.failure == RetailCompatibilityFailure::None,
        "the exact narrow compatibility proof did not authorize");
    require(proof.evidence.retailExecutableIdentityMatched
            && proof.evidence.moduleSnapshotStable
            && proof.evidence.prohibitedModulesAbsent
            && proof.evidence.jip5730ExactOrAbsent
            && proof.evidence.showOff184ExactOrAbsent
            && proof.evidence.renderFirstPersonStockOrJipNormalized
            && proof.evidence.protectedCoreBodiesMatched
            && proof.evidence.protectedFunctionInventoryMatched
            && proof.evidence.protectedVtableSlotsMatched
            && proof.evidence.protectedVtableBlocksMatched
            && proof.evidence.synchronousSameProcess,
        "the complete proof did not derive every evidence field");
    require(proof.diagnostics.jipPresent
            && proof.diagnostics.jipNormalizationApplied
            && proof.diagnostics.showOffPresent,
        "the exact optional-module path was not diagnosed");
    require(proof.diagnostics.protectedCoreBodiesHashed == 13u
            && proof.diagnostics.protectedFunctionsHashed == 22u
            && proof.diagnostics.protectedVtableSlotsRead == 16u
            && proof.diagnostics.protectedVtableBytesHashed == 0x50u,
        "not every protected engine range was inspected");
}

void testOptionalAbsenceAndUnrelatedModificationsAreAllowed()
{
    Fixture fixture;
    fixture.restoreStockFirstPerson();
    fixture.modulesBefore.erase(fixture.modulesBefore.begin() + 1u,
        fixture.modulesBefore.begin() + 3u);
    fixture.modulesAfter = fixture.modulesBefore;
    fixture.mainImage[UnknownTextRva] ^= 0xA5u;
    const RetailCompatibilityProof proof = fixture.evaluate();
    require(proof.compatible
            && proof.evidence.jip5730ExactOrAbsent
            && proof.evidence.showOff184ExactOrAbsent
            && !proof.diagnostics.jipPresent
            && !proof.diagnostics.showOffPresent
            && !proof.diagnostics.jipNormalizationApplied,
        "optional module absence or unrelated modded text was rejected");
}

void testExactModuleIdentitiesAndOverlaysFailClosed()
{
    Fixture fixture;
    fixture.jipFile[0] ^= 1u;
    RetailCompatibilityProof rejected = fixture.evaluate();
    require(rejected.failure == RetailCompatibilityFailure::Jip5730IdentityMismatch,
        "a non-exact JIP file was not identified");
    requireRejected(rejected, "a non-exact JIP file authorized");
    fixture.jipFile[0] ^= 1u;

    fixture.showOffFile[0] ^= 1u;
    rejected = fixture.evaluate();
    require(rejected.failure == RetailCompatibilityFailure::ShowOff184IdentityMismatch,
        "a non-exact ShowOff file was not identified");
    requireRejected(rejected, "a non-exact ShowOff file authorized");
    fixture.showOffFile[0] ^= 1u;

    constexpr std::size_t LoadedTimestampOffset = 0x80u + 4u + 4u;
    fixture.jipImage[LoadedTimestampOffset] ^= 1u;
    rejected = fixture.evaluate();
    require(rejected.failure == RetailCompatibilityFailure::Jip5730IdentityMismatch,
        "a non-exact loaded JIP PE was not identified");
    fixture.jipImage[LoadedTimestampOffset] ^= 1u;

    SyntheticModule duplicateJip = fixture.modulesBefore[1];
    duplicateJip.runtimeBase += 0x01000000u;
    fixture.modulesBefore.push_back(duplicateJip);
    fixture.modulesAfter.push_back(duplicateJip);
    rejected = fixture.evaluate();
    require(rejected.failure == RetailCompatibilityFailure::DuplicateJipModule,
        "duplicate JIP modules were not rejected");
    fixture.modulesBefore.pop_back();
    fixture.modulesAfter.pop_back();

    const SyntheticModule overlay {
        L"GameOverlayRenderer.DLL",
        L"C:\\Steam\\GameOverlayRenderer.DLL",
        0x36000000u,
        0x1000u,
        nullptr,
        0u,
    };
    fixture.modulesBefore.push_back(overlay);
    fixture.modulesAfter.push_back(overlay);
    rejected = fixture.evaluate();
    require(rejected.failure
            == RetailCompatibilityFailure::ProhibitedOverlayOrCaptureLoaded,
        "the known Steam overlay was not rejected by basename");
    requireRejected(rejected, "a known overlay authorized");
}

void testOnlyTheProvenJipRewriteCanNormalize()
{
    Fixture fixture;
    const std::uint32_t functionRva = static_cast<std::uint32_t>(
        RenderFirstPersonPreferred - PreferredMainBase);
    fixture.mainImage[functionRva + JipCallOffset + 17u] ^= 0x44u;
    RetailCompatibilityProof rejected = fixture.evaluate();
    require(rejected.failure
            == RetailCompatibilityFailure::JipRenderFirstPersonRewriteMismatch,
        "an extra first-person rewrite was not distinguished from JIP");
    requireRejected(rejected, "an unknown protected rewrite authorized");
    fixture.mainImage[functionRva + JipCallOffset + 17u] ^= 0x44u;

    fixture.jipImage[JipStubRva + 6u] ^= 0x01u;
    rejected = fixture.evaluate();
    require(rejected.failure
            == RetailCompatibilityFailure::JipRenderFirstPersonRewriteMismatch,
        "a changed JIP stub was accepted as the proven rewrite");
    requireRejected(rejected, "a changed JIP stub authorized");

    Fixture noJip;
    noJip.modulesBefore.erase(noJip.modulesBefore.begin() + 1u);
    noJip.modulesAfter = noJip.modulesBefore;
    rejected = noJip.evaluate();
    require(rejected.failure
            == RetailCompatibilityFailure::JipRenderFirstPersonRewriteMismatch,
        "a JIP-shaped rewrite was accepted without the exact JIP module");
}

void testEveryProtectedFunctionSlotAndBlockFailsClosed()
{
    Fixture fixture;

    fixture.mainImage[CoreFirstRva] ^= 0x10u;
    RetailCompatibilityProof rejected = fixture.evaluate();
    require(rejected.failure
            == RetailCompatibilityFailure::ProtectedCoreBodyMismatch,
        "a protected non-JIP core body rewrite was not rejected");
    fixture.mainImage[CoreFirstRva] ^= 0x10u;

    for (std::size_t index = 0u; index < fixture.functions.size(); ++index)
    {
        const std::uint32_t rva = FunctionFirstRva
            + static_cast<std::uint32_t>(index * FunctionStride);
        fixture.mainImage[rva] ^= 0x20u;
        const RetailCompatibilityProof functionRejected = fixture.evaluate();
        require(functionRejected.failure
                == RetailCompatibilityFailure::ProtectedFunctionMismatch,
            "a protected function rewrite was not rejected");
        fixture.mainImage[rva] ^= 0x20u;
    }

    for (std::size_t index = 0u; index < fixture.slots.size(); ++index)
    {
        fixture.mainImage[VtableRva + index * sizeof(RetailPointer32)] ^= 1u;
        const RetailCompatibilityProof slotRejected = fixture.evaluate();
        require(slotRejected.failure
                == RetailCompatibilityFailure::ProtectedVtableSlotMismatch,
            "a protected vtable slot rewrite was not rejected");
        fixture.mainImage[VtableRva + index * sizeof(RetailPointer32)] ^= 1u;
    }

    fixture.mainImage[VtableRva + 0x48u] ^= 0x80u;
    rejected = fixture.evaluate();
    require(rejected.evidence.protectedVtableSlotsMatched
            && rejected.failure
                == RetailCompatibilityFailure::ProtectedVtableBlockMismatch,
        "an unnamed protected vtable byte was not sealed");
}

void testSnapshotsAndProtectionMustRemainSynchronous()
{
    Fixture fixture;
    fixture.modulesAfter.back().runtimeBase += 0x1000u;
    RetailCompatibilityProof rejected = fixture.evaluate();
    require(rejected.failure == RetailCompatibilityFailure::ModuleSnapshotChanged
            && !rejected.evidence.synchronousSameProcess,
        "a changing module census was described as synchronous");
    requireRejected(rejected, "a changing module census authorized");

    fixture.modulesAfter = fixture.modulesBefore;
    fixture.refreshViews();
    fixture.snapshot.processAfter.creationTime100ns += 1u;
    rejected = proveSyntheticRetailCompatibilityAtDecisionPoint(
        fixture.snapshot,
        fixture.contract);
    require(rejected.failure
            == RetailCompatibilityFailure::ProcessIdentityUnavailable
            && !rejected.evidence.synchronousSameProcess,
        "a recycled process identity was accepted");

    fixture.refreshViews();
    fixture.protections[1].access = SyntheticMemoryAccess::ExecuteReadWrite;
    fixture.snapshot.protectionRanges = fixture.protections.data();
    rejected = proveSyntheticRetailCompatibilityAtDecisionPoint(
        fixture.snapshot,
        fixture.contract);
    requireRejected(rejected, "writable protected engine code authorized");
}

void testProductionEntryRejectsThisProcess()
{
    const RetailCompatibilityProof proof =
        proveCurrentRetailCompatibilityAtDecisionPoint();
    requireRejected(proof,
        "the production compatibility proof authorized a non-Fallout test process");
}
}

int main()
{
    testExactOptionalModulesAndJipNormalizationAuthorize();
    testOptionalAbsenceAndUnrelatedModificationsAreAllowed();
    testExactModuleIdentitiesAndOverlaysFailClosed();
    testOnlyTheProvenJipRewriteCanNormalize();
    testEveryProtectedFunctionSlotAndBlockFailsClosed();
    testSnapshotsAndProtectionMustRemainSynchronous();
    testProductionEntryRejectsThisProcess();
    return 0;
}
