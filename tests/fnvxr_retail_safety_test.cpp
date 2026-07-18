#include "fnvxr_retail_safety.h"

#include <cstdlib>
#include <iostream>

namespace
{
int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}

fnvxr::safety::RetailMutationEvidenceToken completeEvidence(std::uint32_t processId)
{
    fnvxr::safety::RetailMutationEvidenceToken evidence {};
    evidence.validatingProcessId = processId;
    evidence.nvseVersion = fnvxr::safety::SupportedNvseVersion;
    evidence.runtimeVersion = fnvxr::safety::SupportedRuntimeVersion;
    evidence.loadedImageBase = fnvxr::engine::SupportedImageBase;
    evidence.loadedExecutable = {
        fnvxr::engine::SupportedPeTimeDateStamp,
        fnvxr::engine::SupportedPeChecksum,
        fnvxr::engine::SupportedSizeOfImage,
    };
    evidence.loadedFunctionCount = fnvxr::engine::RetailEngineManifest.size();
    for (std::size_t index = 0; index < fnvxr::engine::RetailEngineManifest.size(); ++index)
    {
        evidence.loadedFunctionDigests[index] =
            fnvxr::engine::RetailEngineManifest[index].sha256;
    }
    evidence.engineStereoComplete = true;
    evidence.gpuColorTransportComplete = true;
    evidence.gpuDepthTransportComplete = true;
    evidence.authoritativeWeaponComplete = true;
    evidence.uiModeTransitionsComplete = true;
    return evidence;
}

struct IntegrityValidatorState
{
    const fnvxr::safety::RetailMutationEvidenceToken* expectedEvidence = nullptr;
    std::uint32_t invocationCount = 0;
    bool result = false;
};

bool validateCurrentProcessIntegrity(
    const fnvxr::safety::RetailMutationEvidenceToken& evidence,
    void* context) noexcept
{
    auto* state = static_cast<IntegrityValidatorState*>(context);
    if (!state)
        return false;

    ++state->invocationCount;
    return state->expectedEvidence == &evidence && state->result;
}
}

int main()
{
    using namespace fnvxr::safety;

    if (SupportedNvseVersion != 100925568u || SupportedNvseVersion != 0x06040080u)
        return fail("the supported xNVSE 6.4.8 packed version is incorrect");
    if (!compatibleNvseRuntime(false, SupportedNvseVersion, SupportedRuntimeVersion))
        return fail("the exact supported xNVSE and 1.4.0.525 runtime must be accepted");
    if (compatibleNvseRuntime(true, SupportedNvseVersion, SupportedRuntimeVersion))
        return fail("the editor must never be accepted as the retail runtime");
    if (compatibleNvseRuntime(false, 0, SupportedRuntimeVersion))
        return fail("an invalid xNVSE interface version must be rejected");
    if (compatibleNvseRuntime(false, SupportedNvseVersion - 1u, SupportedRuntimeVersion)
        || compatibleNvseRuntime(false, SupportedNvseVersion + 1u, SupportedRuntimeVersion))
    {
        return fail("a merely nonzero or nearby xNVSE version must be rejected");
    }
    if (compatibleNvseRuntime(false, SupportedNvseVersion, SupportedRuntimeVersionNoGore))
        return fail("the unproven no-gore executable must be rejected");
    if (compatibleNvseRuntime(false, SupportedNvseVersion, 1))
        return fail("a merely nonzero runtime version must not be accepted");

    constexpr std::uint32_t ProcessId = 0x1234u;
    const RetailMutationEvidenceToken complete = completeEvidence(ProcessId);
    if (!retailMutationEvidenceComplete(complete, ProcessId))
        return fail("a complete exact in-process evidence token must validate");

    RetailMutationEvidenceToken changed {};
    if (retailMutationEvidenceComplete(changed, ProcessId))
        return fail("an empty evidence token must fail closed");

    changed = complete;
    changed.validatingProcessId = ProcessId + 1u;
    if (retailMutationEvidenceComplete(changed, ProcessId)
        || retailMutationEvidenceComplete(complete, 0))
    {
        return fail("evidence from a different or invalid process must fail closed");
    }

    changed = complete;
    changed.loadedImageBase += 0x1000u;
    if (retailMutationEvidenceComplete(changed, ProcessId))
        return fail("a relocated or unexpected loaded image must fail closed");

    changed = complete;
    --changed.nvseVersion;
    if (retailMutationEvidenceComplete(changed, ProcessId))
        return fail("an evidence token for a different xNVSE build must fail closed");

    changed = complete;
    changed.runtimeVersion = SupportedRuntimeVersionNoGore;
    if (retailMutationEvidenceComplete(changed, ProcessId))
        return fail("an evidence token for the no-gore runtime must fail closed");

    changed = complete;
    changed.loadedExecutable.checksum ^= 1u;
    if (retailMutationEvidenceComplete(changed, ProcessId))
        return fail("a loaded PE identity mismatch must fail closed");

    changed = complete;
    --changed.loadedFunctionCount;
    if (retailMutationEvidenceComplete(changed, ProcessId))
        return fail("a partial loaded-function manifest must fail closed");

    changed = complete;
    changed.loadedFunctionDigests.front().valid = false;
    if (loadedFunctionManifestMatches(changed)
        || retailMutationEvidenceComplete(changed, ProcessId))
    {
        return fail(
            "an invalid actual loaded-function digest must fail even when its bytes match");
    }

    changed = complete;
    changed.loadedFunctionDigests.back().bytes.back() ^= 1u;
    if (retailMutationEvidenceComplete(changed, ProcessId))
        return fail("one mismatched loaded-function hash must fail closed");

    changed = complete;
    changed.engineStereoComplete = false;
    if (retailMutationEvidenceComplete(changed, ProcessId))
        return fail("engine stereo proof is mandatory");
    changed = complete;
    changed.gpuColorTransportComplete = false;
    if (retailMutationEvidenceComplete(changed, ProcessId))
        return fail("GPU color transport proof is mandatory");
    changed = complete;
    changed.gpuDepthTransportComplete = false;
    if (retailMutationEvidenceComplete(changed, ProcessId))
        return fail("GPU depth transport proof is mandatory");
    changed = complete;
    changed.authoritativeWeaponComplete = false;
    if (retailMutationEvidenceComplete(changed, ProcessId))
        return fail("authoritative weapon proof is mandatory");
    changed = complete;
    changed.uiModeTransitionsComplete = false;
    if (retailMutationEvidenceComplete(changed, ProcessId))
        return fail("UI mode-transition proof is mandatory");

    if (retailMutationEvidenceCurrent(
            complete,
            ProcessId,
            nullptr,
            nullptr))
    {
        return fail("stored evidence without an immediate integrity validator must fail closed");
    }

    IntegrityValidatorState rejectingValidator { &complete, 0, false };
    if (retailMutationEvidenceCurrent(
            complete,
            ProcessId,
            validateCurrentProcessIntegrity,
            &rejectingValidator))
    {
        return fail("a rejecting immediate integrity validator must fail closed");
    }
    if (rejectingValidator.invocationCount != 1)
        return fail("the immediate integrity validator must execute synchronously exactly once");

    IntegrityValidatorState acceptingValidator { &complete, 0, true };
    if (!retailMutationEvidenceCurrent(
            complete,
            ProcessId,
            validateCurrentProcessIntegrity,
            &acceptingValidator))
    {
        return fail("complete evidence with a successful immediate validator must be current");
    }
    if (acceptingValidator.invocationCount != 1)
        return fail("a successful immediate validator must execute synchronously exactly once");

    IntegrityValidatorState invalidEvidenceValidator { &changed, 0, true };
    if (retailMutationEvidenceCurrent(
            changed,
            ProcessId,
            validateCurrentProcessIntegrity,
            &invalidEvidenceValidator)
        || invalidEvidenceValidator.invocationCount != 0)
    {
        return fail("invalid stored evidence must fail before invoking the live validator");
    }

    if (retailMutationAllowed(
            true,
            complete,
            ProcessId,
            validateCurrentProcessIntegrity,
            &acceptingValidator)
        || retailMutationAllowed(true, complete, ProcessId, nullptr, nullptr)
        || retailMutationAllowed(
            false,
            complete,
            ProcessId,
            validateCurrentProcessIntegrity,
            &acceptingValidator)
        || retailMutationAllowed(
            true,
            RetailMutationEvidenceToken {},
            ProcessId,
            validateCurrentProcessIntegrity,
            &acceptingValidator))
    {
        return fail(
            "configuration, stored evidence, and a validator cannot bypass the source-level mutation fuse");
    }

    std::cout << "fnvxr retail mutation safety gate PASS\n";
    return EXIT_SUCCESS;
}
