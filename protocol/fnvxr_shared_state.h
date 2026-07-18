#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fnvxr::shared
{
constexpr std::uint32_t XInputSharedMagic = 0x58564e46; // FNVX
constexpr std::uint32_t XInputSharedVersion = 2;
inline constexpr char XInputSharedMappingName[] = "Local\\FNVXR_XInput_State_v2";
constexpr std::uint32_t DInputSharedMagic = 0x49444e46; // FNDI
constexpr std::uint32_t DInputSharedVersion = 9;
inline constexpr char DInputSharedMappingName[] = "Local\\FNVXR_DInput_State_v9";
inline constexpr char InputCoreProducerMutexName[] = "Local\\FNVXR_Input_Core_Producer_v2_v9";
constexpr std::uint32_t VrPoseSharedMagic = 0x52505646; // FVPR
constexpr std::uint32_t VrPoseSharedVersion = 8;
inline constexpr char VrPoseSharedMappingName[] = "Local\\FNVXR_VR_Pose_State_v8";
constexpr std::uint32_t VrOriginSharedMagic = 0x4f565846; // FXVO
constexpr std::uint32_t VrOriginSharedVersion = 6;
inline constexpr char VrOriginSharedMappingName[] = "Local\\FNVXR_VR_Origin_State_v6";
constexpr std::uint32_t VrOriginStateInvalid = 0;
constexpr std::uint32_t VrOriginStateRenderLease = 1;
constexpr std::uint32_t VrOriginStateCommitted = 2;
constexpr std::uint32_t CameraSharedMagic = 0x43585646; // FNXC
constexpr std::uint32_t CameraSharedVersion = 1;
constexpr std::uint32_t RuntimeSharedMagic = 0x53585646; // FNVS
constexpr std::uint32_t RuntimeSharedVersion = 1;
constexpr std::uint32_t PlayerSharedMagic = 0x50564e46; // FNVP
constexpr std::uint32_t PlayerSharedVersion = 1;
constexpr std::uint32_t CommandSharedMagic = 0x43564e46; // FNVC
constexpr std::uint32_t CommandSharedVersion = 2;
inline constexpr char CommandSharedMappingName[] = "Local\\FNVXR_Command_State_v2";
inline constexpr char CommandWriterMutexName[] = "Local\\FNVXR_Command_Writer_v2";
constexpr std::uint32_t InputEventSharedMagic = 0x49564e46; // FNVI
constexpr std::uint32_t InputEventSharedVersion = 1;
constexpr std::uint32_t InputEventQueueLength = 64;
inline constexpr char InputEventWriterMutexName[] = "Local\\FNVXR_Input_Event_Writer_v1";
constexpr std::uint32_t D3D9FrameSharedMagic = 0x46585646; // FNVF
constexpr std::uint32_t D3D9StereoFrameSharedMagic = 0x53585646; // FNXS
constexpr std::uint32_t D3D9StereoFrameSharedVersion = 7;
inline constexpr char D3D9StereoFrameSharedMappingName[] = "Local\\FNVXR_D3D9_StereoFrame_v7";
constexpr std::uint32_t D3D9SharedFrameMaxWidth = 4096;
constexpr std::uint32_t D3D9SharedFrameMaxHeight = 2560;
// Two independent consumers (the OpenXR host and the evidence capturer) each
// own one claim lane.  Four payload slots guarantee that even if both readers
// terminate while holding different slots, one slot remains writable after
// excluding the current publication.  Stale claims therefore reduce capacity
// but can never stop the producer.
constexpr std::uint32_t D3D9StereoFrameReaderLaneCount = 2;
constexpr std::uint32_t D3D9StereoHostReaderLane = 0;
constexpr std::uint32_t D3D9StereoCaptureReaderLane = 1;
constexpr std::uint32_t D3D9StereoFrameSlotCount = 4;

inline LONG selectWritableStereoFrameSlot(
    LONG publishedSlot,
    LONG hostReaderSlot,
    LONG captureReaderSlot)
{
    for (LONG candidate = 0;
         candidate < static_cast<LONG>(D3D9StereoFrameSlotCount);
         ++candidate)
    {
        if (candidate != publishedSlot
            && candidate != hostReaderSlot
            && candidate != captureReaderSlot)
            return candidate;
    }
    return -1;
}

// Describes how the two eye images in SharedD3D9StereoFrameHeader were made.
// NativeSameFrame is the legacy two-engine-traversal producer. SingleTraversal
// applies the HMD pose to the retail camera once, builds one Gamebryo scene,
// and replays that exact D3D draw stream into both eyes from the same tick.
constexpr std::uint32_t StereoProducerUnknown = 0;
constexpr std::uint32_t StereoProducerDrawReplay = 1;
constexpr std::uint32_t StereoProducerNativeSameFrame = 2;
constexpr std::uint32_t StereoProducerSingleTraversal = 3;

constexpr std::uint32_t RuntimePhaseUnknown = 0;
constexpr std::uint32_t RuntimePhaseMenu = 1;
constexpr std::uint32_t RuntimePhaseLoading = 2;
constexpr std::uint32_t RuntimePhaseGameplay = 3;

constexpr std::uint32_t RuntimeMenuModeBit = 1u << 0;
constexpr std::uint32_t RuntimeStartMenuBit = 1u << 1;
constexpr std::uint32_t RuntimeRaceSexMenuBit = 1u << 2;
constexpr std::uint32_t RuntimeDialogMenuBit = 1u << 3;
constexpr std::uint32_t RuntimeVatsMenuBit = 1u << 4;
constexpr std::uint32_t RuntimeLoadingMenuBit = 1u << 5;
constexpr std::uint32_t RuntimePipBoyMenuBit = 1u << 6;
constexpr std::uint32_t RuntimeGenericMenuBit = 1u << 7;
constexpr std::uint32_t RuntimeInteractiveMenuBits =
    RuntimeStartMenuBit
    | RuntimeRaceSexMenuBit
    | RuntimeDialogMenuBit
    | RuntimeVatsMenuBit
    | RuntimePipBoyMenuBit
    | RuntimeGenericMenuBit;
constexpr std::uint32_t RuntimeBlockingMenuBits =
    RuntimeStartMenuBit
    | RuntimeRaceSexMenuBit
    | RuntimeDialogMenuBit
    | RuntimeVatsMenuBit
    | RuntimeLoadingMenuBit
    | RuntimePipBoyMenuBit
    | RuntimeGenericMenuBit;

inline bool runtimeLoadingMenuBlocksInput(bool rawLoadingVisible, bool actionableMenuVisible)
{
    return rawLoadingVisible && !actionableMenuVisible;
}

inline bool runtimeUiInputAllowed(std::uint32_t menuBits)
{
    return (menuBits & RuntimeInteractiveMenuBits) != 0u
        && (menuBits & RuntimeLoadingMenuBit) == 0u;
}

inline bool runtimeGameplayPhase(std::uint32_t phase, std::uint32_t menuBits, std::uint32_t showroomActive)
{
    return phase == RuntimePhaseGameplay
        && showroomActive == 0u
        && (menuBits & RuntimeBlockingMenuBits) == 0u;
}

inline bool runtimeUiActive(std::uint32_t phase, std::uint32_t menuBits, std::uint32_t showroomActive)
{
    return !runtimeGameplayPhase(phase, menuBits, showroomActive);
}

constexpr std::uint32_t PlayerSharedFlagPlayerNodeValid = 1u << 0;
constexpr std::uint32_t PlayerSharedFlagCameraValid = 1u << 1;
constexpr std::uint32_t PlayerSharedFlagCellKnown = 1u << 2;
constexpr std::uint32_t PlayerSharedFlagThirdPerson = 1u << 3;
constexpr std::uint32_t PlayerSharedFlagGameplay = 1u << 4;
constexpr std::uint32_t PlayerSharedFlagWeaponOut = 1u << 5;
constexpr std::uint32_t PlayerSharedFlagWeaponClassKnown = 1u << 6;
constexpr std::uint32_t PlayerSharedWeaponClassReservedIndex = 0;
constexpr std::uint32_t PlayerSharedEquippedWeaponFormIdReservedIndex = 1;
constexpr std::uint32_t PlayerSharedEquippedFavoriteSlotReservedIndex = 2;
constexpr std::uint32_t PlayerWeaponClassUnknown = 0;
constexpr std::uint32_t PlayerWeaponClassNone = 1;
constexpr std::uint32_t PlayerWeaponClassUnarmed = 2;
constexpr std::uint32_t PlayerWeaponClassMelee = 3;
constexpr std::uint32_t PlayerWeaponClassRanged = 4;
constexpr std::uint32_t PlayerWeaponClassThrown = 5;
constexpr std::uint32_t DInputGameplayFlagAimHeld = 1u << 0;
constexpr std::uint32_t DInputGameplayFlagThirdPersonZoomHeld = 1u << 1;
constexpr std::uint32_t DInputGameplayFlagWeaponOut = 1u << 2;
constexpr std::uint32_t DInputGameplayFlagMeleeOrUnarmed = 1u << 3;
constexpr std::uint8_t XInputReservedRetailConsumed = 0;
constexpr std::uint8_t XInputReservedAutoRun = 1;
constexpr std::uint8_t XInputReservedMovementMode = 2; // 0 normal, 1 walk, 2 run
constexpr std::uint32_t CommandTypeNone = 0;
constexpr std::uint32_t CommandTypeSave = 1;
constexpr std::uint32_t CommandTypeQuit = 2;
constexpr std::uint32_t CommandTypeConsole = 3;
constexpr std::uint32_t CommandStatusIdle = 0;
constexpr std::uint32_t CommandStatusPending = 1;
constexpr std::uint32_t CommandStatusRunning = 2;
constexpr std::uint32_t CommandStatusSucceeded = 3;
constexpr std::uint32_t CommandStatusFailed = 4;
constexpr std::uint32_t InputEventTypeNone = 0;
constexpr std::uint32_t InputEventTypeKeyDown = 1;
constexpr std::uint32_t InputEventTypeKeyUp = 2;
constexpr std::uint32_t InputEventTypeKeyTap = 3;
constexpr std::uint32_t InputEventTypeMouseButtonDown = 4;
constexpr std::uint32_t InputEventTypeMouseButtonUp = 5;
constexpr std::uint32_t InputEventTypeMouseButtonTap = 6;
constexpr std::uint32_t InputEventTypeMouseMove = 7;
constexpr std::uint32_t InputEventTypeMouseWheel = 8;
constexpr std::uint32_t VrPoseTrackingHmd = 1u << 0;
constexpr std::uint32_t VrPoseTrackingLeftGripActive = 1u << 1;
constexpr std::uint32_t VrPoseTrackingRightGripActive = 1u << 2;
constexpr std::uint32_t VrPoseTrackingLeftGripCurrent = 1u << 3;
constexpr std::uint32_t VrPoseTrackingRightGripCurrent = 1u << 4;
constexpr std::uint32_t VrPoseTrackingLeftAimActive = 1u << 5;
constexpr std::uint32_t VrPoseTrackingRightAimActive = 1u << 6;
constexpr std::uint32_t VrPoseTrackingLeftAimCurrent = 1u << 7;
constexpr std::uint32_t VrPoseTrackingRightAimCurrent = 1u << 8;
constexpr std::uint32_t VrPoseTrackingRecenterRequested = 1u << 9;

struct SharedXInputState
{
    std::uint32_t magic;
    std::uint32_t version;
    volatile LONG sequence;
    std::uint32_t packet;
    std::uint16_t buttons;
    std::uint8_t leftTrigger;
    std::uint8_t rightTrigger;
    std::int16_t leftThumbX;
    std::int16_t leftThumbY;
    std::int16_t rightThumbX;
    std::int16_t rightThumbY;
    std::uint8_t connected;
    std::uint8_t reserved[3];
};

struct SharedDInputState
{
    std::uint32_t magic;
    std::uint32_t version;
    volatile LONG sequence;
    std::uint32_t frame;
    LONG clientX;
    LONG clientY;
    std::uint32_t pointerActive;
    std::uint32_t mouseClickPacket;
    // Plugin-owned atomic fallback mailbox; host payload writes preserve it.
    std::uint32_t keyboardAcceptPacket;
    std::uint32_t menuInputActive;
    std::uint32_t gameplayControlsActive;
    std::int32_t leftStickX;
    std::int32_t leftStickY;
    std::int32_t rightStickX;
    std::int32_t rightStickY;
    std::uint32_t headLookActive;
    // Cumulative signed microradian counters. Consumers difference consecutive
    // samples with wrappedInt32Delta so a frame can be polled at most once
    // without losing motion when producer frames are skipped.
    std::int32_t headLookX;
    std::int32_t headLookY;
    std::uint32_t gyroLookActive;
    std::int32_t gyroLookX;
    std::int32_t gyroLookY;
    std::int32_t leftGrip;
    std::int32_t rightGrip;
    std::uint32_t gameplayFlags;
    std::uint32_t aimTrigger;
};

// The input bridges are mapped into several independently scheduled processes.
// Writers mark the sequence odd while mutating producer-owned frame fields and
// even only after that frame is complete. Readers either copy one matching even
// sequence or reject the sample; they must never act on a torn
// button/trigger/stick combination. SharedXInputState::reserved is an explicit
// exception: its three bytes are independent atomic status mailboxes owned by
// the retail proxy/plugin, not part of the host payload transaction.
inline bool beginSequencedSharedWrite(volatile LONG& sequence)
{
    for (int attempt = 0; attempt < 1024; ++attempt)
    {
        const LONG before = sequence;
        if ((before & 1) != 0)
        {
            YieldProcessor();
            continue;
        }
        const std::uint32_t beforeBits = static_cast<std::uint32_t>(before);
        const std::uint32_t desiredBits = beforeBits + 1u;
        LONG desired = 0;
        std::memcpy(&desired, &desiredBits, sizeof(desired));
        if (InterlockedCompareExchange(&sequence, desired, before) == before)
        {
            MemoryBarrier();
            return true;
        }
        YieldProcessor();
    }
    return false;
}

// LONG is the Win32 interlocked storage type, not an ordered identity.  A
// valid even sequence becomes negative after bit 31 is set; consumers must
// therefore test the modulo-2^32 bit pattern rather than signed positivity.
inline std::uint32_t sequencedValueBits(LONG sequence)
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &sequence, sizeof(bits));
    return bits;
}

inline bool sequencedValueIsPublished(LONG sequence)
{
    const std::uint32_t bits = sequencedValueBits(sequence);
    return bits != 0u && (bits & 1u) == 0u;
}

inline void endSequencedSharedWrite(volatile LONG& sequence)
{
    MemoryBarrier();
    const std::uint32_t beforeBits = sequencedValueBits(sequence);
    std::uint32_t publishedBits = beforeBits + 1u;
    // Zero is the cold/uninitialized sentinel in every consumer.  The writer
    // owns the odd value, so it can skip the single modulo-2^32 zero value
    // without exposing an intermediate even record.
    if (publishedBits == 0u)
        publishedBits = 2u;
    LONG published = 0;
    std::memcpy(&published, &publishedBits, sizeof(published));
    InterlockedExchange(&sequence, published);
}

// Advance a single-writer publication counter while preserving zero as the
// uninitialized sentinel. This is used by non-seqlock records whose separate
// `writing` flag already owns the transaction.
inline LONG incrementNonzeroSharedCounter(volatile LONG& sequence)
{
    for (;;)
    {
        const LONG before = sequence;
        std::uint32_t nextBits = sequencedValueBits(before) + 1u;
        if (nextBits == 0u)
            nextBits = 1u;
        LONG next = 0;
        std::memcpy(&next, &nextBits, sizeof(next));
        if (InterlockedCompareExchange(&sequence, next, before) == before)
            return next;
        YieldProcessor();
    }
}

inline bool nonzeroSharedCounterAdvanced(LONG current, LONG previous)
{
    const std::uint32_t currentBits = sequencedValueBits(current);
    const std::uint32_t previousBits = sequencedValueBits(previous);
    const std::uint32_t delta = currentBits - previousBits;
    return currentBits != 0u && delta != 0u && delta < 0x80000000u;
}

// Forward distance in the publication domain 1..UINT32_MAX. Zero is skipped,
// so ordinary modulo-2^32 subtraction is one too large across the wrap.
inline std::uint64_t nonzeroSharedCounterDistance(
    std::uint32_t currentBits,
    std::uint32_t previousBits)
{
    if (currentBits == 0u || previousBits == 0u || currentBits == previousBits)
        return 0;
    if (currentBits > previousBits)
        return static_cast<std::uint64_t>(currentBits - previousBits);
    return static_cast<std::uint64_t>(0xffffffffu - previousBits) + currentBits;
}

inline std::int32_t addWrappedInt32(std::int32_t value, std::int32_t delta)
{
    std::uint32_t valueBits = 0;
    std::uint32_t deltaBits = 0;
    std::memcpy(&valueBits, &value, sizeof(valueBits));
    std::memcpy(&deltaBits, &delta, sizeof(deltaBits));
    const std::uint32_t resultBits = valueBits + deltaBits;
    std::int32_t result = 0;
    std::memcpy(&result, &resultBits, sizeof(result));
    return result;
}

inline std::int32_t wrappedInt32Delta(std::int32_t current, std::int32_t previous)
{
    std::uint32_t currentBits = 0;
    std::uint32_t previousBits = 0;
    std::memcpy(&currentBits, &current, sizeof(currentBits));
    std::memcpy(&previousBits, &previous, sizeof(previousBits));
    const std::uint32_t deltaBits = currentBits - previousBits;
    std::int32_t delta = 0;
    std::memcpy(&delta, &deltaBits, sizeof(delta));
    return delta;
}

template <typename T>
inline bool readSequencedSharedSnapshot(const T* state, T& snapshot, int attempts = 4)
{
    if (!state)
        return false;

    for (int attempt = 0; attempt < attempts; ++attempt)
    {
        const LONG sequenceBefore = state->sequence;
        if ((sequenceBefore & 1) != 0)
        {
            YieldProcessor();
            continue;
        }

        MemoryBarrier();
        std::memcpy(&snapshot, state, sizeof(snapshot));
        MemoryBarrier();

        const LONG sequenceAfter = state->sequence;
        if (sequenceBefore == sequenceAfter && (sequenceAfter & 1) == 0)
            return true;
        YieldProcessor();
    }
    return false;
}

struct SharedVrPoseState
{
    std::uint32_t magic;
    std::uint32_t version;
    volatile LONG sequence;
    std::uint64_t frame;
    std::int64_t predictedDisplayTime;
    float hmdRot[4];
    float hmdPos[3];
    float leftRot[4];
    float leftPos[3];
    float rightRot[4];
    float rightPos[3];
    float leftEyeRot[4];
    float leftEyePos[3];
    float rightEyeRot[4];
    float rightEyePos[3];
    float leftFov[4];
    float rightFov[4];
    // Grip poses remain the wrist/arm anchors. Aim poses are kept separately
    // so retail weapon visuals can use the runtime-defined pointing direction
    // without moving the wrist away from the physical controller.
    float leftAimRot[4];
    float leftAimPos[3];
    float rightAimRot[4];
    float rightAimPos[3];
    std::uint32_t trackingFlags;
    // Incremented by the OpenXR host when LOCAL space changes. Consumers must
    // invalidate every recenter/body anchor derived from an older generation.
    std::uint32_t referenceSpaceGeneration;
    // Nonzero host-lifetime identity. The producer owns a named lifetime mutex
    // and atomically increments the retained mapping value on every restart;
    // consumers reset all origins when this changes. Uniqueness assumes fewer
    // than 2^64 producer lifetimes while the mapping lineage remains live.
    std::uint64_t producerEpoch;
    // Monotonic edge mailbox. The host increments this only on a recenter
    // chord rising edge, so a short press cannot disappear between polls.
    std::uint32_t recenterRequestId;
    std::uint32_t reserved;
};

// The D3D9 native camera transaction is the authority that chooses the
// gameplay recenter sample. The 32-bit retail rig reads this record so camera
// and controller transforms cannot latch two subtly different origins.
struct SharedVrOriginState
{
    std::uint32_t magic;
    std::uint32_t version;
    volatile LONG sequence;
    // VrOriginStateRenderLease while the native camera traversal is active,
    // VrOriginStateCommitted after a verified traversal completed, otherwise
    // VrOriginStateInvalid. The committed state lets animation consume the
    // exact prior render origin even though animation and rendering are not
    // nested in Fallout's frame schedule.
    std::uint32_t active;
    std::uint32_t generation;
    std::uint32_t poseSequence;
    std::uint64_t poseFrame;
    float originRot[4];
    float originPos[3];
    std::uint64_t producerEpoch;
    std::uint32_t renderPoseSequence;
    std::uint32_t reserved;
    std::uint64_t renderPoseFrame;
    std::int64_t renderedDisplayTime;
    // Exact SceneGraph NiCamera and engine-authored world transform captured
    // before the HMD overlay for this render transaction. The retail arm
    // solver anchors against this record instead of a different camera hook.
    std::uint32_t renderCameraAddress;
    std::uint32_t renderCameraWorldValid;
    float renderCameraWorldRot[9];
    float renderCameraWorldPos[3];
    // Body-root transform captured in the same render transaction as the
    // camera above. The animation plugin must never combine that historical
    // camera with a body transform sampled in a later callback.
    std::uint32_t bodyRootAddress;
    std::uint32_t bodyRootWorldValid;
    float bodyRootWorldRot[9];
    float bodyRootWorldPos[3];
    float bodyRootWorldScale;
    std::uint32_t reserved2;
};

struct SharedCameraState
{
    std::uint32_t magic;
    std::uint32_t version;
    volatile LONG sequence;
    std::uint64_t frame;
    std::uint32_t active;
    std::uint32_t thirdPerson;
    float worldRot[9];
    float worldPos[3];
};

struct SharedRuntimeState
{
    std::uint32_t magic;
    std::uint32_t version;
    volatile LONG sequence;
    std::uint64_t frame;
    std::uint32_t menuBits;
    std::uint32_t phase;
    std::uint32_t uiInputAllowed;
    std::uint32_t cameraActive;
    std::uint32_t showroomActive;
    std::uint32_t showroomPhase;
    std::uint32_t showroomSceneIndex;
    std::uint32_t showroomCellFormId;
    std::uint32_t reserved[8];
};

struct SharedD3D9FrameHeader
{
    std::uint32_t magic;
    volatile LONG writing;
    volatile LONG sequence;
    LONG width;
    LONG height;
    LONG pitchBytes;
    LONG format;
};

struct SharedD3D9StereoFrameHeader
{
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t headerBytes;
    volatile LONG writing;
    volatile LONG sequence;
    LONG width;
    LONG height;
    LONG pitchBytes;
    LONG format;
    LONG separated;
    LONG worldCandidate;
    LONG uiActive;
    LONG poseValid;
    LONG poseSequence;
    std::int64_t renderedDisplayTime;
    float leftEyeRot[4];
    float leftEyePos[3];
    float rightEyeRot[4];
    float rightEyePos[3];
    float leftFov[4];
    float rightFov[4];
    LONG producerMode;
    LONG renderPairSequence;
    std::uint32_t leftPayloadOffset;
    std::uint32_t rightPayloadOffset;
    std::uint32_t totalMappingBytes;
    std::uint32_t referenceSpaceGeneration;
    // Identity of the OpenXR pose publisher whose eye transforms are stored.
    std::uint64_t producerEpoch;
    // Independent identity of the D3D9 process that rendered/copied pixels.
    // Both epochs are required: a renderer restart may retain the same live
    // pose publisher and a PID can be reused.
    std::uint64_t rendererProducerEpoch;
    std::uint32_t producerProcessId;
    volatile LONG publishedSlot;
    volatile LONG readerSlots[D3D9StereoFrameReaderLaneCount];
    // 64-bit ABA guard for the metadata snapshot/claim handshake. Combined
    // with rendererProducerEpoch, a reader cannot confuse a wrapped 32-bit
    // sequence with the publication it originally inspected.
    volatile LONG64 publicationGeneration;
};

struct SharedPlayerState
{
    std::uint32_t magic;
    std::uint32_t version;
    volatile LONG sequence;
    std::uint32_t flags;
    std::uint64_t frame;
    std::uint32_t currentCellFormId;
    std::uint32_t playerAddress;
    std::uint32_t playerNodeAddress;
    std::uint32_t cameraNodeAddress;
    float playerWorldRot[9];
    float playerWorldPos[3];
    float cameraWorldRot[9];
    float cameraWorldPos[3];
    std::uint32_t reserved[6];
};

struct SharedCommandState
{
    std::uint32_t magic;
    std::uint32_t version;
    volatile LONG sequence;
    std::uint32_t requestId;
    std::uint32_t command;
    std::uint32_t status;
    std::uint32_t flags;
    std::uint64_t requestedFrame;
    std::uint64_t completedFrame;
    std::uint32_t resultCode;
    char saveName[64];
    char lastCommand[64];
    std::uint32_t reserved[8];
};

struct SharedInputEvent
{
    volatile LONG sequence;
    std::uint32_t type;
    std::uint32_t code;
    std::int32_t value0;
    std::int32_t value1;
    std::uint32_t flags;
    std::uint64_t frame;
};

struct SharedInputEventQueue
{
    std::uint32_t magic;
    std::uint32_t version;
    volatile LONG writeLock;
    volatile LONG writeSequence;
    volatile LONG droppedEvents;
    std::uint32_t reserved[5];
    SharedInputEvent events[InputEventQueueLength];
};

static_assert(sizeof(SharedXInputState) == 32, "SharedXInputState layout changed");
static_assert(sizeof(SharedDInputState) == 100, "SharedDInputState layout changed unexpectedly");
static_assert(sizeof(SharedVrPoseState) == 288, "SharedVrPoseState layout changed unexpectedly");
static_assert(sizeof(SharedVrOriginState) == 216, "SharedVrOriginState layout changed unexpectedly");
static_assert(sizeof(SharedCameraState) == 80, "SharedCameraState layout changed");
static_assert(sizeof(SharedRuntimeState) == 88, "SharedRuntimeState layout changed");
static_assert(sizeof(SharedD3D9FrameHeader) == 28, "SharedD3D9FrameHeader layout changed");
static_assert(sizeof(SharedD3D9StereoFrameHeader) == 216, "SharedD3D9StereoFrameHeader layout changed");
static_assert(sizeof(SharedPlayerState) == 160, "SharedPlayerState layout changed");
static_assert(sizeof(SharedCommandState) == 216, "SharedCommandState layout changed");
static_assert(sizeof(SharedInputEvent) == 32, "SharedInputEvent layout changed");
static_assert(sizeof(SharedInputEventQueue) == 2088, "SharedInputEventQueue layout changed");
static_assert(offsetof(SharedXInputState, sequence) == 8, "SharedXInputState sequence offset changed");
static_assert(offsetof(SharedXInputState, packet) == 12, "SharedXInputState packet offset changed");
static_assert(offsetof(SharedXInputState, buttons) == 16, "SharedXInputState buttons offset changed");
static_assert(offsetof(SharedXInputState, connected) == 28, "SharedXInputState connected offset changed");
static_assert(offsetof(SharedXInputState, reserved) == 29, "SharedXInputState mailbox offset changed");
static_assert(offsetof(SharedDInputState, sequence) == 8, "SharedDInputState sequence offset changed");
static_assert(offsetof(SharedDInputState, frame) == 12, "SharedDInputState frame offset changed");
static_assert(offsetof(SharedDInputState, mouseClickPacket) == 28, "SharedDInputState click offset changed");
static_assert(offsetof(SharedDInputState, keyboardAcceptPacket) == 32, "SharedDInputState accept offset changed");
static_assert(offsetof(SharedDInputState, gameplayFlags) == 92, "SharedDInputState gameplay flags offset changed");
static_assert(offsetof(SharedDInputState, aimTrigger) == 96, "SharedDInputState aim trigger offset changed");
static_assert(offsetof(SharedPlayerState, sequence) == 8, "SharedPlayerState sequence offset changed");
static_assert(offsetof(SharedPlayerState, frame) == 16, "SharedPlayerState frame offset changed");
static_assert(
    offsetof(SharedPlayerState, currentCellFormId) == 24, "SharedPlayerState currentCellFormId offset changed");
static_assert(offsetof(SharedPlayerState, playerWorldRot) == 40, "SharedPlayerState playerWorldRot offset changed");
static_assert(offsetof(SharedPlayerState, playerWorldPos) == 76, "SharedPlayerState playerWorldPos offset changed");
static_assert(offsetof(SharedPlayerState, cameraWorldRot) == 88, "SharedPlayerState cameraWorldRot offset changed");
static_assert(offsetof(SharedPlayerState, cameraWorldPos) == 124, "SharedPlayerState cameraWorldPos offset changed");
static_assert(offsetof(SharedCommandState, sequence) == 8, "SharedCommandState sequence offset changed");
static_assert(offsetof(SharedCommandState, requestId) == 12, "SharedCommandState requestId offset changed");
static_assert(offsetof(SharedCommandState, requestedFrame) == 32, "SharedCommandState requestedFrame offset changed");
static_assert(offsetof(SharedCommandState, saveName) == 52, "SharedCommandState saveName offset changed");
static_assert(offsetof(SharedCommandState, lastCommand) == 116, "SharedCommandState lastCommand offset changed");
static_assert(offsetof(SharedInputEventQueue, writeSequence) == 12, "SharedInputEventQueue writeSequence offset changed");
static_assert(offsetof(SharedInputEventQueue, events) == 40, "SharedInputEventQueue events offset changed");
static_assert(offsetof(SharedVrPoseState, predictedDisplayTime) == 24, "SharedVrPoseState predictedDisplayTime offset changed");
static_assert(offsetof(SharedVrPoseState, hmdRot) == 32, "SharedVrPoseState hmdRot offset changed");
static_assert(offsetof(SharedVrPoseState, leftAimRot) == 204, "SharedVrPoseState leftAimRot offset changed");
static_assert(offsetof(SharedVrPoseState, rightAimRot) == 232, "SharedVrPoseState rightAimRot offset changed");
static_assert(offsetof(SharedVrPoseState, trackingFlags) == 260, "SharedVrPoseState trackingFlags offset changed");
static_assert(offsetof(SharedVrPoseState, referenceSpaceGeneration) == 264, "SharedVrPoseState reference generation offset changed");
static_assert(offsetof(SharedVrPoseState, producerEpoch) == 272, "SharedVrPoseState producer epoch offset changed");
static_assert(offsetof(SharedVrPoseState, recenterRequestId) == 280, "SharedVrPoseState recenter request offset changed");
static_assert(offsetof(SharedVrOriginState, poseFrame) == 24, "SharedVrOriginState pose frame offset changed");
static_assert(offsetof(SharedVrOriginState, originRot) == 32, "SharedVrOriginState rotation offset changed");
static_assert(offsetof(SharedVrOriginState, producerEpoch) == 64, "SharedVrOriginState producer epoch offset changed");
static_assert(offsetof(SharedVrOriginState, renderPoseSequence) == 72, "SharedVrOriginState render pose sequence offset changed");
static_assert(offsetof(SharedVrOriginState, renderPoseFrame) == 80, "SharedVrOriginState render pose frame offset changed");
static_assert(offsetof(SharedVrOriginState, renderCameraAddress) == 96, "SharedVrOriginState render camera offset changed");
static_assert(offsetof(SharedVrOriginState, renderCameraWorldRot) == 104, "SharedVrOriginState render camera rotation offset changed");
static_assert(offsetof(SharedVrOriginState, renderCameraWorldPos) == 140, "SharedVrOriginState render camera position offset changed");
static_assert(offsetof(SharedVrOriginState, bodyRootAddress) == 152, "SharedVrOriginState body root offset changed");
static_assert(offsetof(SharedVrOriginState, bodyRootWorldRot) == 160, "SharedVrOriginState body rotation offset changed");
static_assert(offsetof(SharedVrOriginState, bodyRootWorldPos) == 196, "SharedVrOriginState body position offset changed");
static_assert(offsetof(SharedVrOriginState, bodyRootWorldScale) == 208, "SharedVrOriginState body scale offset changed");
static_assert(offsetof(SharedD3D9StereoFrameHeader, writing) == 12, "SharedD3D9StereoFrameHeader writing offset changed");
static_assert(offsetof(SharedD3D9StereoFrameHeader, sequence) == 16, "SharedD3D9StereoFrameHeader sequence offset changed");
static_assert(offsetof(SharedD3D9StereoFrameHeader, renderedDisplayTime) == 56, "SharedD3D9StereoFrameHeader renderedDisplayTime offset changed");
static_assert(offsetof(SharedD3D9StereoFrameHeader, leftEyeRot) == 64, "SharedD3D9StereoFrameHeader leftEyeRot offset changed");
static_assert(offsetof(SharedD3D9StereoFrameHeader, producerMode) == 152, "SharedD3D9StereoFrameHeader producerMode offset changed");
static_assert(offsetof(SharedD3D9StereoFrameHeader, renderPairSequence) == 156, "SharedD3D9StereoFrameHeader renderPairSequence offset changed");
static_assert(offsetof(SharedD3D9StereoFrameHeader, leftPayloadOffset) == 160, "SharedD3D9StereoFrameHeader payload offset changed");
static_assert(offsetof(SharedD3D9StereoFrameHeader, producerEpoch) == 176, "SharedD3D9StereoFrameHeader producer epoch offset changed");
static_assert(offsetof(SharedD3D9StereoFrameHeader, rendererProducerEpoch) == 184, "SharedD3D9StereoFrameHeader renderer epoch offset changed");
static_assert(offsetof(SharedD3D9StereoFrameHeader, producerProcessId) == 192, "SharedD3D9StereoFrameHeader producer PID offset changed");
static_assert(offsetof(SharedD3D9StereoFrameHeader, publishedSlot) == 196, "SharedD3D9StereoFrameHeader published slot offset changed");
static_assert(offsetof(SharedD3D9StereoFrameHeader, readerSlots) == 200, "SharedD3D9StereoFrameHeader reader slots offset changed");
static_assert(offsetof(SharedD3D9StereoFrameHeader, publicationGeneration) == 208, "SharedD3D9StereoFrameHeader publication generation offset changed");
static_assert(sizeof(SharedD3D9StereoFrameHeader) == 216, "SharedD3D9StereoFrameHeader size changed");
}
