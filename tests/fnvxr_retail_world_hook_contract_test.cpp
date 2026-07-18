#include "fnvxr_retail_world_hook_contract.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
using namespace fnvxr::engine;

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

std::uint32_t decodeJumpTarget(
    const std::uint8_t* jump,
    std::uint32_t instructionAddress)
{
    require(jump && jump[0] == 0xE9u, "test fixture must contain an x86 jump");
    const std::uint32_t displacement = static_cast<std::uint32_t>(jump[1])
        | (static_cast<std::uint32_t>(jump[2]) << 8u)
        | (static_cast<std::uint32_t>(jump[3]) << 16u)
        | (static_cast<std::uint32_t>(jump[4]) << 24u);
    return instructionAddress + 5u + displacement;
}
}

int main()
{
    using namespace fnvxr::engine;

    static_assert(WorldRenderAddress == 0x00873200u);
    static_assert(RetailWorldFunctionByteCount == 4698u);
    static_assert(RetailWorldHookStolenByteCount == 5u);
    static_assert(RetailWorldHookPatchByteCount == 5u);
    static_assert(RetailWorldHookTrampolineByteCount == 10u);
    static_assert(RetailWorldStackArgumentBytes == 0x10u);
    static_assert(RetailWorldHookIndependentProcessSamples == 2u);
    static_assert(!RetailWorldHookContractHeaderContainsIssuer);
#if defined(_MSC_VER) && defined(_M_IX86)
    static_assert(RetailWorldRenderCallableAbiAvailable);
    static_assert(sizeof(RetailWorldRenderFunction) == sizeof(void*));
    static_assert(sizeof(RetailWorldRenderHookFunction) == sizeof(void*));
#else
    static_assert(!RetailWorldRenderCallableAbiAvailable);
#endif

    require(
        RetailWorldHookStolenBytes
            == std::array<std::uint8_t, 5> {{
                0x55u, 0x8Bu, 0xECu, 0x6Au, 0xFFu }},
        "the entry contract must steal exactly three whole instructions");
    require(
        RetailWorldReturnBytes
            == std::array<std::uint8_t, 3> {{ 0xC2u, 0x10u, 0x00u }},
        "the retail function must callee-pop exactly four dword arguments");
    require(
        RetailWorldStackArgumentUseContract[0].stackOffset == 0x08u
            && RetailWorldStackArgumentUseContract[0].readByRetailBody
            && RetailWorldStackArgumentUseContract[1].stackOffset == 0x0Cu
            && !RetailWorldStackArgumentUseContract[1].readByRetailBody
            && RetailWorldStackArgumentUseContract[2].stackOffset == 0x10u
            && RetailWorldStackArgumentUseContract[2].readByRetailBody
            && RetailWorldStackArgumentUseContract[3].stackOffset == 0x14u
            && RetailWorldStackArgumentUseContract[3].readByRetailBody,
        "the body-use contract must preserve the unused-but-real predicate slot");

    std::vector<std::uint8_t> function(RetailWorldFunctionByteCount, 0xCCu);
    std::copy(
        RetailWorldHookStolenBytes.begin(),
        RetailWorldHookStolenBytes.end(),
        function.begin());
    std::copy(
        RetailWorldReturnBytes.begin(),
        RetailWorldReturnBytes.end(),
        function.begin() + RetailWorldReturnInstructionOffset);

    require(
        observeRetailWorldHookSeam(function.data(), function.size()).complete(),
        "the exact entry and ret-0x10 boundary must form a complete seam");
    require(
        !observeRetailWorldHookSeam(
             function.data(),
             RetailWorldFunctionByteCount - 1u)
             .complete(),
        "a truncated world function must fail closed");

    std::vector<std::uint8_t> wrongEntry = function;
    wrongEntry[4] ^= 0x01u;
    require(
        !observeRetailWorldHookSeam(wrongEntry.data(), wrongEntry.size()).complete(),
        "a changed stolen instruction must reject the seam");
    std::vector<std::uint8_t> wrongReturn = function;
    wrongReturn[RetailWorldReturnInstructionOffset + 1u] = 0x0Cu;
    require(
        !observeRetailWorldHookSeam(wrongReturn.data(), wrongReturn.size()).complete(),
        "a changed callee-pop width must reject the seam");

    constexpr std::uintptr_t HookAddress = 0x60001000u;
    constexpr std::uintptr_t TrampolineAddress = 0x50002000u;
    const RetailWorldHookPlan plan = buildRetailWorldHookPlan(
        function.data(),
        function.size(),
        WorldRenderAddress,
        HookAddress,
        TrampolineAddress);
    require(plan.valid, "an exact x86 seam must produce an inert hook plan");
    require(
        plan.continuationAddress
            == WorldRenderAddress + RetailWorldHookStolenByteCount,
        "the trampoline must continue after all stolen instructions");
    require(
        decodeJumpTarget(plan.entryPatch.data(), WorldRenderAddress)
            == HookAddress,
        "the entry jump must target the requested adapter");
    require(
        std::equal(
            RetailWorldHookStolenBytes.begin(),
            RetailWorldHookStolenBytes.end(),
            plan.trampoline.begin()),
        "the trampoline must begin with the exact stolen instructions");
    require(
        decodeJumpTarget(
            plan.trampoline.data() + RetailWorldHookStolenByteCount,
            static_cast<std::uint32_t>(
                TrampolineAddress + RetailWorldHookStolenByteCount))
            == WorldRenderAddress + RetailWorldHookStolenByteCount,
        "the trampoline jump must return to the first untouched instruction");
    require(
        !buildRetailWorldHookPlan(
             wrongEntry.data(),
             wrongEntry.size(),
             WorldRenderAddress,
             HookAddress,
             TrampolineAddress)
             .valid,
        "planning must fail when the loaded prologue changes");

    if constexpr (sizeof(std::uintptr_t) > sizeof(std::uint32_t))
    {
        const std::uintptr_t nonX86Address =
            static_cast<std::uintptr_t>(0x100000000ull);
        require(
            !encodeRetailWorldX86Jump(nonX86Address, HookAddress).valid,
            "a non-x86 source address must fail closed");
        require(
            !buildRetailWorldHookPlan(
                 function.data(),
                 function.size(),
                 WorldRenderAddress,
                 nonX86Address,
                 TrampolineAddress)
                 .valid,
            "a non-x86 hook address must not produce a truncated plan");
    }

    require(
        RetailWorldCallSiteContractInventory.size() == 2u,
        "both stock direct callers must be represented");
    require(
        RetailWorldCallSiteContractInventory[0].preferredCallAddress
                == 0x00870AE8u
            && RetailWorldCallSiteContractInventory[1].preferredCallAddress
                == 0x00870E18u,
        "the caller inventory must pin both loaded direct-call addresses");
    require(
        RetailWorldCallSiteContractInventory[0].stockWorldPathSelector == 1u
            && RetailWorldCallSiteContractInventory[1].stockWorldPathSelector
                == 0u,
        "the two stock callers must preserve their distinct selector literals");

    for (const RetailWorldCallSiteContract& callSite
         : RetailWorldCallSiteContractInventory)
    {
        require(
            callSite.independentLoadedProcessSamples == 2u,
            "each exact callsite window must have two independent samples");
        require(
            callSite.sharedRenderObjectProducerAddress == 0x00872B00u
                && callSite.postWorldOptionProducerAddress == 0x008749B0u,
            "argument provenance must remain address-based and non-speculative");
        require(
            callSite.preferredWindowAddress
                    + RetailWorldCallInstructionWindowOffset
                == callSite.preferredCallAddress,
            "the call offset must identify the exact E8 instruction");

        const RetailWorldCallSiteObservation preferred =
            observeRetailWorldCallSite(
                callSite.bytes.data(),
                callSite.bytes.size(),
                callSite.preferredWindowAddress,
                WorldRenderAddress,
                callSite);
        require(
            preferred.complete(),
            "each preferred-base stock caller must target RenderWorldSceneGraph");

        constexpr std::uintptr_t RelocationDelta = 0x00100000u;
        const RetailWorldCallSiteObservation relocated =
            observeRetailWorldCallSite(
                callSite.bytes.data(),
                callSite.bytes.size(),
                callSite.preferredWindowAddress + RelocationDelta,
                WorldRenderAddress + RelocationDelta,
                callSite);
        require(
            relocated.complete(),
            "a same-module relocation must preserve the exact relative call");
        require(
            !observeRetailWorldCallSite(
                 callSite.bytes.data(),
                 callSite.bytes.size(),
                 callSite.preferredWindowAddress + 1u,
                 WorldRenderAddress,
                 callSite)
                 .complete(),
            "the same bytes at the wrong loaded address must fail closed");

        require(
            !observeRetailWorldCallSite(
                 callSite.bytes.data(),
                 callSite.bytes.size() - 1u,
                 callSite.preferredWindowAddress,
                 WorldRenderAddress,
                 callSite)
                 .complete(),
            "a truncated callsite window must fail closed");
        require(
            !observeRetailWorldCallSite(
                 callSite.bytes.data(),
                 callSite.bytes.size(),
                 callSite.preferredWindowAddress,
                 WorldRenderAddress + 1u,
                 callSite)
                 .complete(),
            "a redirected call target must fail closed");

        std::array<std::uint8_t, 24> changed = callSite.bytes;
        changed[6] ^= 0x01u;
        require(
            !observeRetailWorldCallSite(
                 changed.data(),
                 changed.size(),
                 callSite.preferredWindowAddress,
                 WorldRenderAddress,
                 callSite)
                 .complete(),
            "a changed selector literal must fail the exact callsite contract");
    }

    std::cout
        << "retail world hook contract tests passed; inert entry/trampoline "
           "plan covers both stock callers\n";
    return EXIT_SUCCESS;
}
