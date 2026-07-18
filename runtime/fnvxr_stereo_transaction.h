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
    std::uint64_t runtimeStateSample = 0;

    bool valid() const
    {
        return transactionId != 0
            && sourceFrame != 0
            && poseSequence != 0
            && runtimeStateSample != 0;
    }
};

// The backend may establish only publication evidence. Transaction identity is
// owned by renderStereoTransaction and is never exposed through a mutable API.
struct StereoPublicationEvidence
{
    bool colorPairComplete = false;
    bool depthPairComplete = false;
    bool sameSimulationTick = false;
    bool poseMatched = false;
    bool conservativeVisibilityComplete = false;
    bool resourceGraphComplete = false;
    bool exactShaderSemantics = false;
    bool gpuSynchronized = false;
    bool distinctBinocularViews = false;
    bool independentTranslational6Dof = false;
    bool independentRotational6Dof = false;
    bool authoritativeTrackedRetailWeapon = false;
    bool authoritativeMuzzleAlignment = false;
    bool gameplayHudExcluded = false;
    bool fresh = false;
};

enum class StereoTransactionFailure : std::uint32_t
{
    None = 0,
    InvalidIdentity = 1,
    Snapshot = 2,
    BeginIsolation = 3,
    ConservativeVisibility = 4,
    LeftEyeRender = 5,
    RightEyeRender = 6,
    RenderGraphValidation = 7,
    GpuPublicationPreparation = 8,
    IncompleteProof = 9,
    StateRestore = 10,
    Publish = 11,
};

struct StereoTransactionResult
{
    bool published = false;
    StereoTransactionFailure failure = StereoTransactionFailure::None;
    // A failed result may retain incomplete diagnostic evidence, but it must
    // never carry a proof accepted by completeForWorldStereo().
    product::StereoFrameProof proof {};
};

class StereoTransactionBackend
{
public:
    virtual ~StereoTransactionBackend() = default;

    // This is a pure snapshot: it must not mutate authoritative or externally
    // visible state. A false return therefore needs no restoration.
    virtual bool snapshotAuthoritativeState() noexcept = 0;
    // Activation may partially mutate state before returning false. Once the
    // snapshot succeeds, the transaction always restores, including on this
    // method's failure path.
    virtual bool beginIsolation(const StereoFrameIdentity& identity) noexcept = 0;
    virtual bool accumulateConservativeVisibility() noexcept = 0;
    virtual bool renderEye(StereoEye eye) noexcept = 0;
    virtual bool validateRenderGraph() noexcept = 0;
    // Preparation may create private textures, commands, and fence work, but
    // both true and false returns guarantee zero consumer-visible publication:
    // no publication record, shared fence/value, consumer image index, or
    // externally visible state may change. Only publish() may cross that
    // boundary after authoritative state has been restored.
    virtual bool prepareGpuPublication(StereoPublicationEvidence& evidence) noexcept = 0;
    // Restoration is mandatory before publication, on both success and failure.
    virtual bool restoreAuthoritativeState() noexcept = 0;
    // Atomic no-publication contract: true publishes exactly this proof once;
    // false guarantees that no frame, identity, fence, index, or consumer-
    // visible state was published. A partially publishing backend is invalid.
    virtual bool publish(const product::StereoFrameProof& proof) noexcept = 0;
    virtual void discardIsolatedOutputs() noexcept = 0;
};

inline StereoTransactionResult failAfterSnapshot(
    StereoTransactionBackend& backend,
    StereoTransactionFailure failure,
    const product::StereoFrameProof& proof = {}) noexcept
{
    StereoTransactionResult result {};
    result.failure = failure;
    result.proof = proof;
    if (result.proof.completeForWorldStereo())
        result.proof = {};
    if (!backend.restoreAuthoritativeState())
    {
        result.failure = StereoTransactionFailure::StateRestore;
        result.proof = {};
    }
    backend.discardIsolatedOutputs();
    return result;
}

inline product::StereoFrameProof makeStereoFrameProof(
    const StereoFrameIdentity& identity,
    const StereoPublicationEvidence& evidence) noexcept
{
    product::StereoFrameProof proof {};
    proof.transactionId = identity.transactionId;
    proof.sourceFrame = identity.sourceFrame;
    proof.poseSequence = identity.poseSequence;
    proof.runtimeStateSample = identity.runtimeStateSample;
    proof.colorPairComplete = evidence.colorPairComplete;
    proof.depthPairComplete = evidence.depthPairComplete;
    proof.sameSimulationTick = evidence.sameSimulationTick;
    proof.poseMatched = evidence.poseMatched;
    proof.conservativeVisibilityComplete = evidence.conservativeVisibilityComplete;
    proof.resourceGraphComplete = evidence.resourceGraphComplete;
    proof.exactShaderSemantics = evidence.exactShaderSemantics;
    proof.gpuSynchronized = evidence.gpuSynchronized;
    proof.distinctBinocularViews = evidence.distinctBinocularViews;
    proof.independentTranslational6Dof = evidence.independentTranslational6Dof;
    proof.independentRotational6Dof = evidence.independentRotational6Dof;
    proof.authoritativeTrackedRetailWeapon = evidence.authoritativeTrackedRetailWeapon;
    proof.authoritativeMuzzleAlignment = evidence.authoritativeMuzzleAlignment;
    proof.gameplayHudExcluded = evidence.gameplayHudExcluded;
    proof.fresh = evidence.fresh;
    return proof;
}

inline StereoTransactionResult renderStereoTransaction(
    StereoTransactionBackend& backend,
    const StereoFrameIdentity& identity) noexcept
{
    // Freeze caller-owned identity before invoking any backend method. The
    // backend receives only this private immutable copy, so an external alias
    // to the caller's object cannot rewrite the identity later published.
    const StereoFrameIdentity frozenIdentity = identity;
    if (!frozenIdentity.valid())
    {
        backend.discardIsolatedOutputs();
        return { false, StereoTransactionFailure::InvalidIdentity, {} };
    }

    if (!backend.snapshotAuthoritativeState())
    {
        backend.discardIsolatedOutputs();
        return { false, StereoTransactionFailure::Snapshot, {} };
    }

    if (!backend.beginIsolation(frozenIdentity))
        return failAfterSnapshot(backend, StereoTransactionFailure::BeginIsolation);

    if (!backend.accumulateConservativeVisibility())
        return failAfterSnapshot(backend, StereoTransactionFailure::ConservativeVisibility);

    if (!backend.renderEye(StereoEye::Left))
        return failAfterSnapshot(backend, StereoTransactionFailure::LeftEyeRender);

    if (!backend.renderEye(StereoEye::Right))
        return failAfterSnapshot(backend, StereoTransactionFailure::RightEyeRender);

    if (!backend.validateRenderGraph())
        return failAfterSnapshot(backend, StereoTransactionFailure::RenderGraphValidation);

    StereoPublicationEvidence evidence {};
    product::StereoFrameProof proof = makeStereoFrameProof(frozenIdentity, evidence);
    if (!backend.prepareGpuPublication(evidence))
        return failAfterSnapshot(
            backend,
            StereoTransactionFailure::GpuPublicationPreparation,
            proof);

    proof = makeStereoFrameProof(frozenIdentity, evidence);

    if (!proof.completeForWorldStereo())
        return failAfterSnapshot(backend, StereoTransactionFailure::IncompleteProof, proof);

    if (!backend.restoreAuthoritativeState())
    {
        backend.discardIsolatedOutputs();
        return { false, StereoTransactionFailure::StateRestore, {} };
    }

    if (!backend.publish(proof))
    {
        backend.discardIsolatedOutputs();
        return { false, StereoTransactionFailure::Publish, {} };
    }

    return { true, StereoTransactionFailure::None, proof };
}
}
