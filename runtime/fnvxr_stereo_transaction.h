#pragma once

#include "fnvxr_product_contract.h"

#include <cstdint>

namespace fnvxr::runtime
{
enum class StereoEye : std::uint32_t
{
    Left = 0,
    Right = 1,
};

struct StereoFrameIdentity
{
    std::uint64_t transactionId = 0;
    std::uint64_t sourceFrame = 0;
    std::uint64_t poseSequence = 0;

    bool valid() const
    {
        return transactionId != 0 && sourceFrame != 0 && poseSequence != 0;
    }
};

enum class StereoTransactionFailure : std::uint32_t
{
    None = 0,
    InvalidIdentity = 1,
    Begin = 2,
    ConservativeVisibility = 3,
    LeftEyeRender = 4,
    RightEyeRender = 5,
    RenderGraphValidation = 6,
    GpuPublicationPreparation = 7,
    IncompleteProof = 8,
    StateRestore = 9,
    Publish = 10,
};

struct StereoTransactionResult
{
    bool published = false;
    StereoTransactionFailure failure = StereoTransactionFailure::None;
    product::StereoFrameProof proof {};
};

class StereoTransactionBackend
{
public:
    virtual ~StereoTransactionBackend() = default;

    // begin snapshots every authoritative camera/render state and redirects all
    // proof work to isolated color/depth resources.
    virtual bool begin(const StereoFrameIdentity& identity) noexcept = 0;
    virtual bool accumulateConservativeVisibility() noexcept = 0;
    virtual bool renderEye(StereoEye eye) noexcept = 0;
    virtual bool validateRenderGraph() noexcept = 0;
    virtual bool prepareGpuPublication(product::StereoFrameProof& proof) noexcept = 0;
    // Restoration is mandatory before publication, on both success and failure.
    virtual bool restoreAuthoritativeState() noexcept = 0;
    virtual bool publish(const product::StereoFrameProof& proof) noexcept = 0;
    virtual void discardIsolatedOutputs() noexcept = 0;
};

inline StereoTransactionResult failAfterBegin(
    StereoTransactionBackend& backend,
    StereoTransactionFailure failure,
    const product::StereoFrameProof& proof = {}) noexcept
{
    StereoTransactionResult result {};
    result.failure = failure;
    result.proof = proof;
    if (!backend.restoreAuthoritativeState())
        result.failure = StereoTransactionFailure::StateRestore;
    backend.discardIsolatedOutputs();
    return result;
}

inline StereoTransactionResult renderStereoTransaction(
    StereoTransactionBackend& backend,
    const StereoFrameIdentity& identity) noexcept
{
    if (!identity.valid())
    {
        backend.discardIsolatedOutputs();
        return { false, StereoTransactionFailure::InvalidIdentity, {} };
    }

    if (!backend.begin(identity))
    {
        backend.discardIsolatedOutputs();
        return { false, StereoTransactionFailure::Begin, {} };
    }

    if (!backend.accumulateConservativeVisibility())
        return failAfterBegin(backend, StereoTransactionFailure::ConservativeVisibility);

    if (!backend.renderEye(StereoEye::Left))
        return failAfterBegin(backend, StereoTransactionFailure::LeftEyeRender);

    if (!backend.renderEye(StereoEye::Right))
        return failAfterBegin(backend, StereoTransactionFailure::RightEyeRender);

    if (!backend.validateRenderGraph())
        return failAfterBegin(backend, StereoTransactionFailure::RenderGraphValidation);

    product::StereoFrameProof proof {};
    proof.transactionId = identity.transactionId;
    proof.sourceFrame = identity.sourceFrame;
    proof.poseSequence = identity.poseSequence;
    if (!backend.prepareGpuPublication(proof))
        return failAfterBegin(
            backend,
            StereoTransactionFailure::GpuPublicationPreparation,
            proof);

    if (!proof.completeForWorldStereo())
        return failAfterBegin(backend, StereoTransactionFailure::IncompleteProof, proof);

    if (!backend.restoreAuthoritativeState())
    {
        backend.discardIsolatedOutputs();
        return { false, StereoTransactionFailure::StateRestore, proof };
    }

    if (!backend.publish(proof))
    {
        backend.discardIsolatedOutputs();
        return { false, StereoTransactionFailure::Publish, proof };
    }

    return { true, StereoTransactionFailure::None, proof };
}
}
