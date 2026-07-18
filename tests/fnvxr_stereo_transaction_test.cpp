#include "fnvxr_stereo_transaction.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}

class FakeBackend final : public fnvxr::runtime::StereoTransactionBackend
{
public:
    enum Call : int
    {
        Snapshot = 1,
        BeginIsolation = 2,
        Accumulate = 3,
        RenderLeft = 4,
        RenderRight = 5,
        Validate = 6,
        Prepare = 7,
        Restore = 8,
        Publish = 9,
        Discard = 10,
    };

    bool failSnapshot = false;
    bool failBeginIsolation = false;
    bool failAccumulate = false;
    bool failLeft = false;
    bool failRight = false;
    bool failValidation = false;
    bool failPrepare = false;
    bool failRestore = false;
    bool failPublish = false;
    bool omitDepth = false;
    bool rewriteCallerIdentityInBegin = false;
    fnvxr::runtime::StereoFrameIdentity* callerIdentityAlias = nullptr;
    int authoritativeState = 17;
    int fallbackColor = 0x11223344;
    int fallbackDepth = 0x55667788;
    int savedAuthoritativeState = 0;
    bool gpuPublicationStaged = false;
    // True means any consumer publication record, fence/value, image index,
    // or equivalent externally visible state has become observable.
    bool externalVisibility = false;
    bool published = false;
    fnvxr::product::StereoFrameProof publishedProof {};
    std::vector<int> calls;

    bool snapshotAuthoritativeState() noexcept override
    {
        calls.push_back(Snapshot);
        if (failSnapshot)
            return false;
        savedAuthoritativeState = authoritativeState;
        return true;
    }

    bool beginIsolation(const fnvxr::runtime::StereoFrameIdentity& identity) noexcept override
    {
        calls.push_back(BeginIsolation);
        authoritativeState = 99;
        if (rewriteCallerIdentityInBegin && callerIdentityAlias)
        {
            callerIdentityAlias->transactionId += 9000;
            callerIdentityAlias->sourceFrame += 9000;
            callerIdentityAlias->poseSequence += 9000;
            callerIdentityAlias->runtimeStateSample += 9000;
        }
        return !failBeginIsolation && identity.transactionId != 0;
    }

    bool accumulateConservativeVisibility() noexcept override
    {
        calls.push_back(Accumulate);
        authoritativeState = 100;
        return !failAccumulate;
    }

    bool renderEye(fnvxr::runtime::StereoEye eye) noexcept override
    {
        calls.push_back(eye == fnvxr::runtime::StereoEye::Left ? RenderLeft : RenderRight);
        authoritativeState += eye == fnvxr::runtime::StereoEye::Left ? 1 : 2;
        return eye == fnvxr::runtime::StereoEye::Left ? !failLeft : !failRight;
    }

    bool validateRenderGraph() noexcept override
    {
        calls.push_back(Validate);
        return !failValidation;
    }

    bool prepareGpuPublication(
        fnvxr::runtime::StereoPublicationEvidence& evidence) noexcept override
    {
        calls.push_back(Prepare);
        gpuPublicationStaged = true;
        // Staging is private. externalVisibility is latched only by a
        // successful publish and is intentionally not clearable by discard.
        if (failPrepare)
            return false;
        evidence.colorPairComplete = true;
        evidence.depthPairComplete = !omitDepth;
        evidence.sameSimulationTick = true;
        evidence.poseMatched = true;
        evidence.conservativeVisibilityComplete = true;
        evidence.resourceGraphComplete = true;
        evidence.exactShaderSemantics = true;
        evidence.gpuSynchronized = true;
        evidence.distinctBinocularViews = true;
        evidence.independentTranslational6Dof = true;
        evidence.independentRotational6Dof = true;
        evidence.authoritativeTrackedRetailWeapon = true;
        evidence.authoritativeMuzzleAlignment = true;
        evidence.gameplayHudExcluded = true;
        evidence.fresh = true;
        return true;
    }

    bool restoreAuthoritativeState() noexcept override
    {
        calls.push_back(Restore);
        if (failRestore)
            return false;
        authoritativeState = savedAuthoritativeState;
        return true;
    }

    bool publish(const fnvxr::product::StereoFrameProof& proof) noexcept override
    {
        calls.push_back(Publish);
        if (failPublish || authoritativeState != savedAuthoritativeState)
            return false;
        gpuPublicationStaged = false;
        externalVisibility = true;
        published = true;
        publishedProof = proof;
        return true;
    }

    void discardIsolatedOutputs() noexcept override
    {
        calls.push_back(Discard);
        gpuPublicationStaged = false;
        published = false;
    }
};

fnvxr::runtime::StereoFrameIdentity identity()
{
    return { 1234, 88, 5678, 0x4000 };
}
}

int main()
{
    using namespace fnvxr::runtime;

    {
        FakeBackend backend;
        const int fallbackColor = backend.fallbackColor;
        const int fallbackDepth = backend.fallbackDepth;
        const StereoTransactionResult result = renderStereoTransaction(backend, identity());
        const std::vector<int> expected {
            FakeBackend::Snapshot,
            FakeBackend::BeginIsolation,
            FakeBackend::Accumulate,
            FakeBackend::RenderLeft,
            FakeBackend::RenderRight,
            FakeBackend::Validate,
            FakeBackend::Prepare,
            FakeBackend::Restore,
            FakeBackend::Publish,
        };
        if (!result.published || result.failure != StereoTransactionFailure::None)
            return fail("known-good stereo fixture must publish");
        if (backend.calls != expected)
            return fail("known-good transaction call order changed");
        if (!backend.published
            || !backend.externalVisibility
            || backend.gpuPublicationStaged
            || backend.authoritativeState != 17)
            return fail("publication must occur only after authoritative state restoration");
        if (backend.fallbackColor != fallbackColor || backend.fallbackDepth != fallbackDepth)
            return fail("successful proof rendering must not alter fallback color/depth");
        if (!result.proof.completeForWorldStereo())
            return fail("known-good transaction must produce complete product proof");
        if (result.proof.transactionId != identity().transactionId
            || result.proof.sourceFrame != identity().sourceFrame
            || result.proof.poseSequence != identity().poseSequence
            || result.proof.runtimeStateSample != identity().runtimeStateSample
            || backend.publishedProof.transactionId != identity().transactionId
            || backend.publishedProof.sourceFrame != identity().sourceFrame
            || backend.publishedProof.poseSequence != identity().poseSequence
            || backend.publishedProof.runtimeStateSample
                != identity().runtimeStateSample)
        {
            return fail("publication preparation must not author or rewrite transaction identity");
        }
    }

    {
        FakeBackend backend;
        StereoFrameIdentity missingRuntimeSample = identity();
        missingRuntimeSample.runtimeStateSample = 0;
        const StereoTransactionResult result = renderStereoTransaction(
            backend,
            missingRuntimeSample);
        const std::vector<int> expected { FakeBackend::Discard };
        if (result.published
            || result.failure != StereoTransactionFailure::InvalidIdentity
            || result.proof.completeForWorldStereo()
            || backend.calls != expected
            || backend.externalVisibility)
        {
            return fail("stereo identity with runtime-state sample zero must fail before snapshot");
        }
    }

    {
        FakeBackend backend;
        backend.failLeft = true;
        const int fallbackColor = backend.fallbackColor;
        const int fallbackDepth = backend.fallbackDepth;
        const StereoTransactionResult result = renderStereoTransaction(backend, identity());
        const std::vector<int> expected {
            FakeBackend::Snapshot,
            FakeBackend::BeginIsolation,
            FakeBackend::Accumulate,
            FakeBackend::RenderLeft,
            FakeBackend::Restore,
            FakeBackend::Discard,
        };
        if (result.published || result.failure != StereoTransactionFailure::LeftEyeRender)
            return fail("left-eye failure must abort the transaction");
        if (backend.calls != expected || backend.published)
            return fail("left-eye failure must restore and discard without rendering/publishing right eye");
        if (backend.authoritativeState != 17
            || backend.fallbackColor != fallbackColor
            || backend.fallbackDepth != fallbackDepth)
        {
            return fail("left-eye failure must restore engine and fallback state bit-exactly");
        }
    }

    {
        FakeBackend backend;
        backend.failPrepare = true;
        const StereoTransactionResult result = renderStereoTransaction(backend, identity());
        if (result.published
            || result.failure != StereoTransactionFailure::GpuPublicationPreparation)
        {
            return fail("GPU preparation failure after staging must reject the transaction");
        }
        if (backend.externalVisibility || backend.gpuPublicationStaged)
        {
            return fail(
                "failed GPU preparation must expose no publication, fence, index, or state");
        }
        if (result.proof.completeForWorldStereo())
            return fail("GPU preparation failure must return a non-accepting proof");
    }

    {
        FakeBackend backend;
        backend.omitDepth = true;
        const StereoTransactionResult result = renderStereoTransaction(backend, identity());
        if (result.published || result.failure != StereoTransactionFailure::IncompleteProof)
            return fail("missing per-eye depth must reject a fully rendered color pair");
        if (backend.calls.back() != FakeBackend::Discard || backend.published)
            return fail("incomplete proof must discard isolated outputs");
        if (backend.externalVisibility || backend.gpuPublicationStaged)
            return fail("incomplete proof after GPU preparation must remain consumer-invisible");
        if (result.proof.completeForWorldStereo())
            return fail("incomplete-proof failure result must remain non-accepting");
    }

    {
        FakeBackend backend;
        backend.failRestore = true;
        const StereoTransactionResult result = renderStereoTransaction(backend, identity());
        if (result.published || result.failure != StereoTransactionFailure::StateRestore)
            return fail("state-restore failure must prevent publication");
        if (result.proof.completeForWorldStereo()
            || result.proof.transactionId != 0
            || result.proof.sourceFrame != 0
            || result.proof.poseSequence != 0
            || result.proof.runtimeStateSample != 0)
        {
            return fail("state-restore failure must not leak an acceptance-complete proof");
        }
        if (backend.externalVisibility || backend.gpuPublicationStaged)
            return fail("restore failure after GPU preparation must remain consumer-invisible");
        for (const int call : backend.calls)
        {
            if (call == FakeBackend::Publish)
                return fail("publication must never run after failed state restoration");
        }
    }

    {
        FakeBackend backend;
        backend.failSnapshot = true;
        const StereoTransactionResult result = renderStereoTransaction(backend, identity());
        if (result.published || result.failure != StereoTransactionFailure::Snapshot)
            return fail("pure authoritative-state snapshot failure must fail closed");
        const std::vector<int> expected { FakeBackend::Snapshot, FakeBackend::Discard };
        if (backend.calls != expected)
            return fail("snapshot failure must not restore state that was never captured");
    }

    {
        FakeBackend backend;
        backend.failBeginIsolation = true;
        const StereoTransactionResult result = renderStereoTransaction(backend, identity());
        if (result.published
            || result.failure != StereoTransactionFailure::BeginIsolation)
        {
            return fail("partial isolation activation failure must fail closed");
        }
        const std::vector<int> expected {
            FakeBackend::Snapshot,
            FakeBackend::BeginIsolation,
            FakeBackend::Restore,
            FakeBackend::Discard,
        };
        if (backend.calls != expected || backend.authoritativeState != 17)
            return fail("partial isolation activation must always be structurally restorable");
    }

    {
        FakeBackend backend;
        backend.failPublish = true;
        const StereoTransactionResult result = renderStereoTransaction(backend, identity());
        if (result.published
            || backend.published
            || result.failure != StereoTransactionFailure::Publish)
        {
            return fail("publish false must mean atomically that nothing became externally visible");
        }
        if (result.proof.completeForWorldStereo()
            || result.proof.transactionId != 0
            || result.proof.sourceFrame != 0
            || result.proof.poseSequence != 0
            || result.proof.runtimeStateSample != 0)
        {
            return fail("publication failure must not return a proof another layer could accept");
        }
        if (backend.externalVisibility || backend.gpuPublicationStaged)
            return fail("publish false must leave no consumer-visible publication state");
        if (backend.calls.size() < 2
            || backend.calls[backend.calls.size() - 2] != FakeBackend::Publish
            || backend.calls.back() != FakeBackend::Discard)
        {
            return fail("failed atomic publication must discard all isolated outputs");
        }
    }

    {
        FakeBackend backend;
        StereoFrameIdentity callerIdentity = identity();
        const StereoFrameIdentity originalIdentity = callerIdentity;
        backend.rewriteCallerIdentityInBegin = true;
        backend.callerIdentityAlias = &callerIdentity;

        const StereoTransactionResult result = renderStereoTransaction(
            backend,
            callerIdentity);
        if (!result.published || !result.proof.completeForWorldStereo())
            return fail("identity-freeze fixture must otherwise publish successfully");
        if (callerIdentity.transactionId == originalIdentity.transactionId
            || result.proof.transactionId != originalIdentity.transactionId
            || result.proof.sourceFrame != originalIdentity.sourceFrame
            || result.proof.poseSequence != originalIdentity.poseSequence
            || result.proof.runtimeStateSample
                != originalIdentity.runtimeStateSample)
        {
            return fail("transaction must freeze caller identity before invoking an aliased backend");
        }
    }

    std::cout << "fnvxr isolated stereo transaction PASS\n";
    return EXIT_SUCCESS;
}
