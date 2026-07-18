#include "fnvxr_retail_abi_map.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

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

void writeRel32Call(
    std::vector<std::uint8_t>& bytes,
    std::size_t offset,
    std::uintptr_t functionAddress,
    std::uintptr_t targetAddress)
{
    require(offset + 5u <= bytes.size(), "test call must fit in the fixture");
    const std::int64_t displacement = static_cast<std::int64_t>(targetAddress)
        - static_cast<std::int64_t>(functionAddress + offset + 5u);
    require(
        displacement >= (std::numeric_limits<std::int32_t>::min)()
            && displacement <= (std::numeric_limits<std::int32_t>::max)(),
        "test call displacement must fit in rel32");
    const std::uint32_t encoded = static_cast<std::uint32_t>(
        static_cast<std::int32_t>(displacement));
    bytes[offset] = 0xE8u;
    bytes[offset + 1u] = static_cast<std::uint8_t>(encoded);
    bytes[offset + 2u] = static_cast<std::uint8_t>(encoded >> 8u);
    bytes[offset + 3u] = static_cast<std::uint8_t>(encoded >> 16u);
    bytes[offset + 4u] = static_cast<std::uint8_t>(encoded >> 24u);
}

void writeNearConditionalBranch(
    std::vector<std::uint8_t>& bytes,
    std::size_t offset,
    std::uint8_t conditionOpcode,
    std::size_t targetOffset)
{
    require(offset + 6u <= bytes.size(), "test near branch must fit in the fixture");
    const std::int64_t displacement = static_cast<std::int64_t>(targetOffset)
        - static_cast<std::int64_t>(offset + 6u);
    require(
        displacement >= (std::numeric_limits<std::int32_t>::min)()
            && displacement <= (std::numeric_limits<std::int32_t>::max)(),
        "test near branch displacement must fit in rel32");
    const std::uint32_t encoded = static_cast<std::uint32_t>(
        static_cast<std::int32_t>(displacement));
    bytes[offset] = 0x0Fu;
    bytes[offset + 1u] = conditionOpcode;
    bytes[offset + 2u] = static_cast<std::uint8_t>(encoded);
    bytes[offset + 3u] = static_cast<std::uint8_t>(encoded >> 8u);
    bytes[offset + 4u] = static_cast<std::uint8_t>(encoded >> 16u);
    bytes[offset + 5u] = static_cast<std::uint8_t>(encoded >> 24u);
}

void writeShortBranch(
    std::vector<std::uint8_t>& bytes,
    std::size_t offset,
    std::uint8_t opcode,
    std::size_t targetOffset)
{
    require(offset + 2u <= bytes.size(), "test short branch must fit in the fixture");
    const std::int64_t displacement = static_cast<std::int64_t>(targetOffset)
        - static_cast<std::int64_t>(offset + 2u);
    require(
        displacement >= (std::numeric_limits<std::int8_t>::min)()
            && displacement <= (std::numeric_limits<std::int8_t>::max)(),
        "test short branch displacement must fit in rel8");
    bytes[offset] = opcode;
    bytes[offset + 1u] = static_cast<std::uint8_t>(
        static_cast<std::int8_t>(displacement));
}
}

int main()
{
    using namespace fnvxr::probe::abi;

    require(
        RenderWorldDirectCallContract.size() == 10u,
        "the ABI map must cover setup, both selector strategies, common render, and auxiliary passes");
    require(
        RenderWorldDirectCallContract[0].functionOffset == 0x0E6Du,
        "the stack culler constructor callsite must stay exact");
    require(
        RenderWorldDirectCallContract[1].functionOffset == 0x0ECEu
            && RenderWorldDirectCallContract[2].functionOffset == 0x0EF2u,
        "both alternate-strategy accumulation callsites must stay exact");
    require(
        RenderWorldDirectCallContract[3].functionOffset == 0x0F5Bu,
        "the normal-strategy accumulation callsite must stay exact");
    require(
        RenderWorldBranchContract.size() == 2u,
        "the selector entry and alternate-strategy join must both be explicit");

    std::vector<std::uint8_t> fixture(RenderWorldFunctionBytes, 0x90u);
    for (const RenderWorldDirectCall& call : RenderWorldDirectCallContract)
    {
        writeRel32Call(
            fixture,
            call.functionOffset,
            RenderWorldPreferredAddress,
            call.preferredTargetAddress);
    }
    writeNearConditionalBranch(
        fixture,
        RenderWorldBranchContract[0].functionOffset,
        0x84u,
        RenderWorldBranchContract[0].targetFunctionOffset);
    writeShortBranch(
        fixture,
        RenderWorldBranchContract[1].functionOffset,
        0xEBu,
        RenderWorldBranchContract[1].targetFunctionOffset);

    for (const RenderWorldDirectCall& call : RenderWorldDirectCallContract)
    {
        const DirectCallObservation observation = observeDirectCall(
            fixture.data(),
            fixture.size(),
            RenderWorldPreferredAddress,
            RenderWorldPreferredAddress,
            call);
        require(observation.instructionReadable, "a complete direct call must be readable");
        require(observation.opcodeMatches, "a direct call must require opcode E8");
        require(observation.targetDecoded, "a direct call target must decode without overflow");
        require(observation.targetMatches, "a direct call target must match the ABI contract");
        require(observation.complete(), "a matching direct call observation must be complete");

        const std::uintptr_t relocatedFunction = RenderWorldPreferredAddress + 0x200000u;
        std::vector<std::uint8_t> relocatedFixture(RenderWorldFunctionBytes, 0x90u);
        writeRel32Call(
            relocatedFixture,
            call.functionOffset,
            relocatedFunction,
            call.preferredTargetAddress + 0x200000u);
        require(
            observeDirectCall(
                relocatedFixture.data(),
                relocatedFixture.size(),
                relocatedFunction,
                RenderWorldPreferredAddress,
                call)
                .complete(),
            "the ABI map must relocate same-module call targets with the function");
    }

    for (const RenderWorldBranch& branch : RenderWorldBranchContract)
    {
        const BranchObservation observation = observeBranch(
            fixture.data(),
            fixture.size(),
            RenderWorldPreferredAddress,
            branch);
        require(observation.instructionReadable, "a complete branch must be readable");
        require(observation.opcodeMatches, "a branch opcode must match its exact encoding");
        require(observation.targetDecoded, "a branch target must decode without overflow");
        require(observation.targetMatches, "a branch target must match the ABI contract");
        require(observation.complete(), "a matching branch observation must be complete");
    }

    std::vector<std::uint8_t> wrongOpcode = fixture;
    wrongOpcode[RenderWorldDirectCallContract[3].functionOffset] = 0x90u;
    require(
        !observeDirectCall(
             wrongOpcode.data(),
             wrongOpcode.size(),
             RenderWorldPreferredAddress,
             RenderWorldPreferredAddress,
             RenderWorldDirectCallContract[3])
             .complete(),
        "a target-shaped displacement without an E8 opcode must fail closed");

    std::vector<std::uint8_t> redirected = fixture;
    writeRel32Call(
        redirected,
        RenderWorldDirectCallContract[3].functionOffset,
        RenderWorldPreferredAddress,
        RenderWorldDirectCallContract[3].preferredTargetAddress + 1u);
    const DirectCallObservation redirectedObservation = observeDirectCall(
        redirected.data(),
        redirected.size(),
        RenderWorldPreferredAddress,
        RenderWorldPreferredAddress,
        RenderWorldDirectCallContract[3]);
    require(
        redirectedObservation.targetDecoded && !redirectedObservation.targetMatches,
        "a one-byte call redirection must be observable and fail the contract");

    std::vector<std::uint8_t> redirectedBranch = fixture;
    redirectedBranch[RenderWorldBranchContract[1].functionOffset + 1u] ^= 0x01u;
    const BranchObservation redirectedBranchObservation = observeBranch(
        redirectedBranch.data(),
        redirectedBranch.size(),
        RenderWorldPreferredAddress,
        RenderWorldBranchContract[1]);
    require(
        redirectedBranchObservation.targetDecoded
            && !redirectedBranchObservation.targetMatches
            && !redirectedBranchObservation.complete(),
        "a one-byte branch redirection must be observable and fail the contract");

    std::vector<std::uint8_t> wrongBranchOpcode = fixture;
    wrongBranchOpcode[RenderWorldBranchContract[0].functionOffset + 1u] = 0x85u;
    require(
        !observeBranch(
             wrongBranchOpcode.data(),
             wrongBranchOpcode.size(),
             RenderWorldPreferredAddress,
             RenderWorldBranchContract[0])
             .complete(),
        "a different conditional branch opcode must fail closed");

    require(
        !observeDirectCall(
             fixture.data(),
             RenderWorldDirectCallContract.back().functionOffset + 4u,
             RenderWorldPreferredAddress,
             RenderWorldPreferredAddress,
             RenderWorldDirectCallContract.back())
             .complete(),
        "a truncated rel32 call must fail closed");

    const ByteWindow middleWindow = contextWindow(100u, 40u, 5u, 16u, 16u);
    require(
        middleWindow.offset == 24u && middleWindow.byteCount == 37u,
        "callsite context must include the exact instruction and bounded neighbors");
    const ByteWindow startWindow = contextWindow(8u, 0u, 5u, 16u, 16u);
    require(
        startWindow.offset == 0u && startWindow.byteCount == 8u,
        "callsite context must clamp at the function start and end");

    require(
        rangeContained(0x00400000u, 0x0107B000u, 0x00400000u, 1u),
        "the first byte of an image must be contained");
    require(
        rangeContained(0x00400000u, 0x0107B000u, 0x0147AFFFu, 1u),
        "the last byte of an image must be contained");
    require(
        !rangeContained(0x00400000u, 0x0107B000u, 0x0147B000u, 1u),
        "the first byte after an image must be rejected");
    require(
        !rangeContained(
            (std::numeric_limits<std::uintptr_t>::max)() - 4u,
            8u,
            (std::numeric_limits<std::uintptr_t>::max)() - 2u,
            4u),
        "overflowing image and range arithmetic must fail closed");

    require(
        RetailAbiEvidenceInventory.size()
            == fnvxr::engine::abi::RetailFunctionAbiInventory.size()
            && &RetailAbiEvidenceInventory.front()
                == &fnvxr::engine::abi::RetailFunctionAbiInventory.front(),
        "the probe must source the complete runtime ABI inventory without a private subset");
    for (const fnvxr::engine::abi::RetailFunctionAbiDescriptor& entry
         : RetailAbiEvidenceInventory)
    {
        require(entry.name && entry.name[0], "every ABI evidence range must be named");
        require(entry.byteCount != 0u, "every ABI evidence range must be nonempty");
        require(entry.sha256.valid, "every ABI evidence range must have a valid digest");
        require(
            fnvxr::engine::abi::structurallyValid(entry),
            "every probed ABI descriptor must retain its exact stack and body contract");
        require(
            rangeContained(
                fnvxr::engine::SupportedImageBase,
                fnvxr::engine::SupportedSizeOfImage,
                entry.preferredAddress,
                entry.byteCount),
            "every ABI evidence range must stay inside the supported image");
    }
    for (std::size_t left = 0; left < RetailAbiEvidenceInventory.size(); ++left)
    {
        const auto& first = RetailAbiEvidenceInventory[left];
        const std::uintptr_t firstEnd = first.preferredAddress + first.byteCount;
        for (std::size_t right = left + 1u;
             right < RetailAbiEvidenceInventory.size();
             ++right)
        {
            const auto& second = RetailAbiEvidenceInventory[right];
            const std::uintptr_t secondEnd = second.preferredAddress + second.byteCount;
            require(
                firstEnd <= second.preferredAddress
                    || secondEnd <= first.preferredAddress,
                "ABI evidence function ranges must never overlap");
        }
    }

    for (const std::uintptr_t requiredAddress : {
             std::uintptr_t { 0x00A71430u },
             std::uintptr_t { 0x004A0F60u },
             std::uintptr_t { 0x004A0FF0u },
             std::uintptr_t { 0x00B664F0u },
             std::uintptr_t { 0x00CFCC20u },
             std::uintptr_t { 0x00A7FD90u },
             std::uintptr_t { 0x00C4EE90u },
             std::uintptr_t { 0x00C4F1D0u },
             std::uintptr_t { 0x00B66520u },
             std::uintptr_t { 0x00B66570u },
         })
    {
        require(
            std::find_if(
                RetailAbiEvidenceInventory.begin(),
                RetailAbiEvidenceInventory.end(),
                [requiredAddress](const auto& entry) {
                    return entry.preferredAddress == requiredAddress;
                }) != RetailAbiEvidenceInventory.end(),
            "the probe inventory must include every lifecycle and virtual dispatch body");
    }

    require(
        !fnvxr::engine::abi::RetailVtableSlots.empty(),
        "critical virtual dispatch slots must be explicit");
    for (const fnvxr::engine::abi::RetailVtableSlotDescriptor& slot
         : fnvxr::engine::abi::RetailVtableSlots)
    {
        const VtableSlotObservation observation = observeVtableSlot(
            fnvxr::engine::SupportedImageBase,
            fnvxr::engine::SupportedSizeOfImage,
            slot,
            true,
            static_cast<std::uint32_t>(slot.preferredTargetAddress));
        require(observation.complete(), "an exact loaded vtable slot must pass");

        constexpr std::uintptr_t RelocationDelta = 0x00010000u;
        const VtableSlotObservation relocated = observeVtableSlot(
            fnvxr::engine::SupportedImageBase + RelocationDelta,
            fnvxr::engine::SupportedSizeOfImage,
            slot,
            true,
            static_cast<std::uint32_t>(
                slot.preferredTargetAddress + RelocationDelta));
        require(relocated.complete(), "vtable slots and targets must relocate together");
    }

    const auto& firstSlot = fnvxr::engine::abi::RetailVtableSlots.front();
    require(
        !observeVtableSlot(
             fnvxr::engine::SupportedImageBase,
             fnvxr::engine::SupportedSizeOfImage,
             firstSlot,
             true,
             static_cast<std::uint32_t>(firstSlot.preferredTargetAddress + 1u))
             .complete(),
        "a one-byte vtable target rewrite must fail closed");
    require(
        !observeVtableSlot(
             fnvxr::engine::SupportedImageBase,
             fnvxr::engine::SupportedSizeOfImage,
             firstSlot,
             false,
             0u)
             .complete(),
        "an unreadable vtable slot must fail closed");
    require(
        !observeVtableSlot(
             fnvxr::engine::SupportedImageBase,
             0x1000u,
             firstSlot,
             true,
             static_cast<std::uint32_t>(firstSlot.preferredTargetAddress))
             .complete(),
        "a vtable slot outside the claimed module bounds must fail closed");

    require(
        RetailVtableBlockEvidenceInventory.size()
                == fnvxr::engine::abi::RetailVtableBlocks.size()
            && &RetailVtableBlockEvidenceInventory.front()
                == &fnvxr::engine::abi::RetailVtableBlocks.front(),
        "the probe must source the canonical complete-vtable block inventory");
    const auto& cullingBlock = RetailVtableBlockEvidenceInventory.front();
    fnvxr::engine::Sha256Digest computedBlockHash = cullingBlock.sha256;
    computedBlockHash.valid = false;
    require(
        finalizeComputedSha256Digest(true, computedBlockHash)
            && computedBlockHash.valid,
        "a successful Windows hash result must become a valid comparable digest");
    const VtableBlockObservation exactBlock = observeVtableBlock(
        fnvxr::engine::SupportedImageBase,
        fnvxr::engine::SupportedSizeOfImage,
        cullingBlock,
        true,
        computedBlockHash);
    require(
        exactBlock.complete(),
        "an exact readable hash of the complete culler vtable block must pass");

    fnvxr::engine::Sha256Digest rewrittenBlock = cullingBlock.sha256;
    rewrittenBlock.bytes[0] ^= 0x01u;
    require(
        !observeVtableBlock(
             fnvxr::engine::SupportedImageBase,
             fnvxr::engine::SupportedSizeOfImage,
             cullingBlock,
             true,
             rewrittenBlock)
             .complete(),
        "one changed byte anywhere in the complete vtable block must fail closed");
    require(
        !observeVtableBlock(
             fnvxr::engine::SupportedImageBase,
             fnvxr::engine::SupportedSizeOfImage,
             cullingBlock,
             false,
             {})
             .complete(),
        "an unreadable complete vtable block must fail closed");
    fnvxr::engine::Sha256Digest failedBlockHash = cullingBlock.sha256;
    require(
        !finalizeComputedSha256Digest(false, failedBlockHash)
            && !failedBlockHash.valid,
        "a failed Windows hash must clear its digest instead of leaving stale bytes valid");
    require(
        !observeVtableBlock(
             fnvxr::engine::SupportedImageBase,
             0x1000u,
             cullingBlock,
             true,
             cullingBlock.sha256)
             .complete(),
        "a complete vtable block outside the loaded module must fail closed");

    std::cout << "retail ABI map contract passed\n";
    return EXIT_SUCCESS;
}
