#pragma once

#include "decision.h"
#include "snapshots.h"

#include <cstdint>

namespace fnvxr::kernel::presentation
{
struct CoordinatorPolicy
{
    // Counts coordinator advances, not wall-clock or compositor frames.
    std::uint32_t maxUiHoldAdvances = 2;
    bool holdLastUiWhileBlocking = true;
};

class Coordinator
{
public:
    explicit Coordinator(CoordinatorPolicy policy = {});

    PresentationDecision advance(const PresentationInput& input);
    void reset();

private:
    PresentationDecision decideBlockingUi(const PresentationInput& input);
    PresentationDecision decideWorld(const PresentationInput& input);
    DecisionReason uiFailure(const PresentationInput& input) const;
    DecisionReason worldFailure(const PresentationInput& input) const;
    bool exactPoseExistsFor(const PresentationInput& input, const SourceKey& source) const;
    PresentationDecision heldUi(DecisionReason reason);
    void clearRetainedUi();

    CoordinatorPolicy policy_ {};
    SourceKey retainedUiSource_ {};
    SourceKey latestAcceptedUiSource_ {};
    SourceKey lastAcceptedWorldSource_ {};
    std::uint64_t lastAcceptedWorldRuntimeSample_ = 0;
    std::uint64_t retainedUiRuntimeSample_ = 0;
    std::uint32_t holdAdvances_ = 0;
};
}
