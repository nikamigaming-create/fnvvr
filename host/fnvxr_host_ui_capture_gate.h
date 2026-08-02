#pragma once

#include "../protocol/fnvxr_product_contract.h"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace fnvxr::host::ui_capture
{
struct RuntimeSample
{
    std::uint64_t sample = 0u;
    std::uint32_t phase = shared::RuntimePhaseUnknown;
    std::uint32_t menuBits = 0u;
    std::uint32_t showroomActive = 0u;
    bool cameraActive = false;
    bool fresh = false;
};

struct WindowCapture
{
    std::uint64_t sourceFrame = 0u;
    std::uint32_t width = 0u;
    std::uint32_t height = 0u;
    std::size_t pixelCount = 0u;
    bool complete = false;
    bool visibleContent = false;
    bool retailWindowOwned = false;
};

struct Decision
{
    product::UiFrameProof proof {};

    enum class Failure : std::uint32_t
    {
        None = 0u,
        BeforeRuntimeUnavailable,
        BeforeNotConfirmedUi,
        AfterRuntimeUnavailable,
        AfterNotConfirmedUi,
        RuntimeSampleRegressed,
        RuntimeStateChanged,
        CaptureIncomplete,
        CaptureNotVisible,
        CaptureNotRetailOwned,
        CaptureIdentityMissing,
        CaptureDimensionsInvalid,
        CapturePayloadInvalid,
    } failure = Failure::None;

    bool accepted() const noexcept
    {
        return proof.completeForUiQuad();
    }
};

inline const char* failureName(Decision::Failure failure) noexcept
{
    using Failure = Decision::Failure;
    switch (failure)
    {
        case Failure::None: return "none";
        case Failure::BeforeRuntimeUnavailable: return "before_runtime_unavailable";
        case Failure::BeforeNotConfirmedUi: return "before_not_confirmed_ui";
        case Failure::AfterRuntimeUnavailable: return "after_runtime_unavailable";
        case Failure::AfterNotConfirmedUi: return "after_not_confirmed_ui";
        case Failure::RuntimeSampleRegressed: return "runtime_sample_regressed";
        case Failure::RuntimeStateChanged: return "runtime_state_changed";
        case Failure::CaptureIncomplete: return "capture_incomplete";
        case Failure::CaptureNotVisible: return "capture_not_visible";
        case Failure::CaptureNotRetailOwned: return "capture_not_retail_owned";
        case Failure::CaptureIdentityMissing: return "capture_identity_missing";
        case Failure::CaptureDimensionsInvalid: return "capture_dimensions_invalid";
        case Failure::CapturePayloadInvalid: return "capture_payload_invalid";
    }
    return "unknown";
}

inline product::RetailState classify(const RuntimeSample& sample) noexcept
{
    product::PresentationInput input {};
    input.runtimeStateSample = sample.sample;
    input.runtimePhase = sample.phase;
    input.menuBits = sample.menuBits;
    input.showroomActive = sample.showroomActive;
    input.runtimeFresh = sample.fresh;
    input.cameraActive = sample.cameraActive;
    return product::classifyRetailState(input);
}

inline bool confirmedUi(const RuntimeSample& sample) noexcept
{
    const product::RetailState state = classify(sample);
    return state == product::RetailState::InteractiveUi
        || state == product::RetailState::Loading;
}

constexpr bool sameRuntimeUiState(
    const RuntimeSample& before,
    const RuntimeSample& after) noexcept
{
    return before.fresh
        && after.fresh
        && before.sample != 0u
        && after.sample != 0u
        && before.sample <= after.sample
        && before.phase == after.phase
        && before.menuBits == after.menuBits
        && before.showroomActive == after.showroomActive
        && before.cameraActive == after.cameraActive;
}

constexpr bool completeWindowCapture(const WindowCapture& capture) noexcept
{
    if (!capture.complete
        || !capture.visibleContent
        || !capture.retailWindowOwned
        || capture.sourceFrame == 0u
        || capture.width == 0u
        || capture.height == 0u)
    {
        return false;
    }
    if (capture.width
        > (std::numeric_limits<std::size_t>::max)() / capture.height)
    {
        return false;
    }
    return capture.pixelCount
        == static_cast<std::size_t>(capture.width) * capture.height;
}

// A host window copy is an allowed UI source only when fresh, monotonically
// advancing retail observations bracket it with identical confirmed UI state.
// The accepted proof is committed to the post-copy sample. Gameplay, unknown
// state, state changes, regressions, and failed/black captures all fail closed.
inline Decision assess(
    const RuntimeSample& before,
    const WindowCapture& capture,
    const RuntimeSample& after) noexcept
{
    using Failure = Decision::Failure;
    if (!before.fresh || before.sample == 0u)
        return { {}, Failure::BeforeRuntimeUnavailable };
    if (!confirmedUi(before))
        return { {}, Failure::BeforeNotConfirmedUi };
    if (!after.fresh || after.sample == 0u)
        return { {}, Failure::AfterRuntimeUnavailable };
    if (!confirmedUi(after))
        return { {}, Failure::AfterNotConfirmedUi };
    if (after.sample < before.sample)
        return { {}, Failure::RuntimeSampleRegressed };
    if (!sameRuntimeUiState(before, after))
        return { {}, Failure::RuntimeStateChanged };
    if (!capture.complete)
        return { {}, Failure::CaptureIncomplete };
    if (!capture.visibleContent)
        return { {}, Failure::CaptureNotVisible };
    if (!capture.retailWindowOwned)
        return { {}, Failure::CaptureNotRetailOwned };
    if (capture.sourceFrame == 0u)
        return { {}, Failure::CaptureIdentityMissing };
    if (capture.width == 0u
        || capture.height == 0u
        || capture.width
            > (std::numeric_limits<std::size_t>::max)() / capture.height)
    {
        return { {}, Failure::CaptureDimensionsInvalid };
    }
    if (!completeWindowCapture(capture))
        return { {}, Failure::CapturePayloadInvalid };
    return {
        {
            capture.sourceFrame,
            after.sample,
            true,
            true,
            true,
        },
        Failure::None,
    };
}
}
