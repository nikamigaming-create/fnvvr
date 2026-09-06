#pragma once

#include "source_key.h"

namespace fnvxr::kernel::presentation
{
struct StereoIdentityProof
{
    SourceKey leftEye {};
    SourceKey rightEye {};
    bool sameSimulationTick = false;

    constexpr bool coherentWith(const SourceKey& source) const
    {
        return isValid(source)
            && leftEye == source
            && rightEye == source
            && sameSimulationTick;
    }
};

struct GpuFrameProof
{
    SourceKey source {};
    bool retailProducerOwned = false;
    bool producerCompletionObserved = false;
    bool consumerOwnershipAcquired = false;
    bool exclusiveOwnershipInterval = false;
    bool synchronized = false;

    constexpr bool completeFor(const SourceKey& expected) const
    {
        return source == expected
            && retailProducerOwned
            && producerCompletionObserved
            && consumerOwnershipAcquired
            && exclusiveOwnershipInterval
            && synchronized;
    }
};

// Claims established inside the retail render transaction. Receiving two GPU
// textures cannot establish any of these prerequisites by itself.
struct RetailWorldProof
{
    bool renderLocalDepthPairComplete = false;
    bool conservativeVisibilityComplete = false;
    bool resourceGraphComplete = false;
    bool exactShaderSemantics = false;
    bool independentTranslational6Dof = false;
    bool independentRotational6Dof = false;
    bool authoritativeTrackedRetailWeapon = false;
    bool authoritativeMuzzleAlignment = false;
    bool gameplayHudExcluded = false;
    bool renderTransactionComplete = false;

    constexpr bool renderComplete() const
    {
        return renderTransactionComplete || complete();
    }

    constexpr bool complete() const
    {
        return renderLocalDepthPairComplete
            && conservativeVisibilityComplete
            && resourceGraphComplete
            && exactShaderSemantics
            && independentTranslational6Dof
            && independentRotational6Dof
            && authoritativeTrackedRetailWeapon
            && authoritativeMuzzleAlignment
            && gameplayHudExcluded;
    }
};
}
