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
        Begin = 1,
        Accumulate = 2,
        RenderLeft = 3,
        RenderRight = 4,
        Validate = 5,
        Prepare = 6,
        Restore = 7,
        Publish = 8,
        Discard = 9,
    };

    bool failBegin = false;
    bool failAccumulate = false;
    bool failLeft = false;
    bool failRight = false;
    bool failValidation = false;
    bool failPrepare = false;
    bool failRestore = false;
    bool failPublish = false;
    bool omitDepth = false;
    int authoritativeState = 17;
    int fallbackColor = 0x11223344;
    int fallbackDepth = 0x55667788;
    int savedAuthoritativeState = 0;
    bool published = false;
    std::vector<int> calls;

    bool begin(const fnvxr::runtime::StereoFrameIdentity& identity) noexcept override
    {
        calls.push_back(Begin);
        if (failBegin || identity.transactionId == 0)
            return false;
        savedAuthoritativeState = authoritativeState;
        authoritativeState = 99;
        return true;
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

    bool prepareGpuPublication(fnvxr::product::StereoFrameProof& proof) noexcept override
    {
        calls.push_back(Prepare);
        if (failPrepare)
            return false;
        proof.colorPairComplete = true;
        proof.depthPairComplete = !omitDepth;
        proof.sameSimulationTick = true;
        proof.poseMatched = true;
        proof.conservativeVisibilityComplete = true;
        proof.resourceGraphComplete = true;
        proof.exactShaderSemantics = true;
        proof.gpuSynchronized = true;
        proof.fresh = true;
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

    bool publish(const fnvxr::product::StereoFrameProof&) noexcept override
    {
        calls.push_back(Publish);
        if (failPublish || authoritativeState != savedAuthoritativeState)
            return false;
        published = true;
        return true;
    }

    void discardIsolatedOutputs() noexcept override
    {
        calls.push_back(Discard);
        published = false;
    }
};

fnvxr::runtime::StereoFrameIdentity identity()
{
    return { 1234, 88, 5678 };
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
            FakeBackend::Begin,
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
        if (!backend.published || backend.authoritativeState != 17)
            return fail("publication must occur only after authoritative state restoration");
        if (backend.fallbackColor != fallbackColor || backend.fallbackDepth != fallbackDepth)
            return fail("successful proof rendering must not alter fallback color/depth");
        if (!result.proof.completeForWorldStereo())
            return fail("known-good transaction must produce complete product proof");
    }

    {
        FakeBackend backend;
        backend.failLeft = true;
        const int fallbackColor = backend.fallbackColor;
        const int fallbackDepth = backend.fallbackDepth;
        const StereoTransactionResult result = renderStereoTransaction(backend, identity());
        const std::vector<int> expected {
            FakeBackend::Begin,
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
        backend.omitDepth = true;
        const StereoTransactionResult result = renderStereoTransaction(backend, identity());
        if (result.published || result.failure != StereoTransactionFailure::IncompleteProof)
            return fail("missing per-eye depth must reject a fully rendered color pair");
        if (backend.calls.back() != FakeBackend::Discard || backend.published)
            return fail("incomplete proof must discard isolated outputs");
    }

    {
        FakeBackend backend;
        backend.failRestore = true;
        const StereoTransactionResult result = renderStereoTransaction(backend, identity());
        if (result.published || result.failure != StereoTransactionFailure::StateRestore)
            return fail("state-restore failure must prevent publication");
        for (const int call : backend.calls)
        {
            if (call == FakeBackend::Publish)
                return fail("publication must never run after failed state restoration");
        }
    }

    {
        FakeBackend backend;
        backend.failBegin = true;
        const StereoTransactionResult result = renderStereoTransaction(backend, identity());
        if (result.published || result.failure != StereoTransactionFailure::Begin)
            return fail("begin failure must fail closed");
        const std::vector<int> expected { FakeBackend::Begin, FakeBackend::Discard };
        if (backend.calls != expected)
            return fail("begin failure must not restore unsnapshotted state");
    }

    std::cout << "fnvxr isolated stereo transaction PASS\n";
    return EXIT_SUCCESS;
}
