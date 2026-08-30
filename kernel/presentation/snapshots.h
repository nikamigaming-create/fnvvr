#pragma once

#include "evidence.h"

#include <cstdint>

namespace fnvxr::kernel::presentation
{
enum class RuntimePhase : std::uint8_t
{
    Unknown,
    Running,
    Loading,
};

enum class UiClassification : std::uint8_t
{
    None,
    Blocking,
    PipBoySpatial,
    BlockingWithPipBoy,
};

struct RuntimeSnapshot
{
    std::uint64_t sample = 0;
    RuntimePhase phase = RuntimePhase::Unknown;
    UiClassification ui = UiClassification::None;
    bool fresh = false;
};

struct WorldFrameProof
{
    SourceKey source {};
    std::uint64_t runtimeSample = 0;
    StereoIdentityProof stereoIdentity {};
    GpuFrameProof gpu {};
    RetailWorldProof retail {};
    bool completeStereoPair = false;
    bool distinctEyeViews = false;
    bool runtimeLineageVerified = false;
    bool fresh = false;
};

struct UiFrameProof
{
    SourceKey source {};
    std::uint64_t runtimeSample = 0;
    GpuFrameProof gpu {};
    bool completeRetailColor = false;
    bool monoRetailView = false;
    bool runtimeLineageVerified = false;
    bool fresh = false;
};

struct PoseHistorySnapshot
{
    SourceKey source {};
    bool exact = false;
};

struct PresentationInput
{
    RuntimeSnapshot runtime {};
    WorldFrameProof world {};
    UiFrameProof ui {};
    PoseHistorySnapshot poseHistory {};
    bool trackedRigReady = false;
    bool wristContentReady = false;
};
}
