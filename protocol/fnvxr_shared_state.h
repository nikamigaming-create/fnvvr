#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>

namespace fnvxr::shared
{
constexpr std::uint32_t XInputSharedMagic = 0x58564e46; // FNVX
constexpr std::uint32_t XInputSharedVersion = 1;
constexpr std::uint32_t DInputSharedMagic = 0x49444e46; // FNDI
constexpr std::uint32_t DInputSharedVersion = 7;
constexpr std::uint32_t VrPoseSharedMagic = 0x52505646; // FVPR
constexpr std::uint32_t VrPoseSharedVersion = 4;
constexpr std::uint32_t CameraSharedMagic = 0x43585646; // FNXC
constexpr std::uint32_t CameraSharedVersion = 1;
constexpr std::uint32_t RuntimeSharedMagic = 0x53585646; // FNVS
constexpr std::uint32_t RuntimeSharedVersion = 1;
constexpr std::uint32_t PlayerSharedMagic = 0x50564e46; // FNVP
constexpr std::uint32_t PlayerSharedVersion = 1;
constexpr std::uint32_t CommandSharedMagic = 0x43564e46; // FNVC
constexpr std::uint32_t CommandSharedVersion = 1;
constexpr std::uint32_t InputEventSharedMagic = 0x49564e46; // FNVI
constexpr std::uint32_t InputEventSharedVersion = 1;
constexpr std::uint32_t InputEventQueueLength = 64;
constexpr std::uint32_t OpenMwPlayerSharedMagic = 0x4f4d4e46; // FNMO
constexpr std::uint32_t OpenMwPlayerSharedVersion = 1;
constexpr std::uint32_t D3D9FrameSharedMagic = 0x46585646; // FNVF
constexpr std::uint32_t D3D9StereoFrameSharedMagic = 0x53585646; // FNXS
constexpr std::uint32_t D3D9SharedFrameMaxWidth = 4096;
constexpr std::uint32_t D3D9SharedFrameMaxHeight = 2560;

// Describes how the two eye images in SharedD3D9StereoFrameHeader were made.
// NativeSameFrame is the only production VR mode: both complete eye renders
// come from one simulation tick and are published as a single pair.
constexpr std::uint32_t StereoProducerUnknown = 0;
constexpr std::uint32_t StereoProducerDrawReplay = 1;
constexpr std::uint32_t StereoProducerNativeSameFrame = 2;

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
constexpr std::uint32_t RuntimeInteractiveMenuBits =
    RuntimeMenuModeBit
    | RuntimeStartMenuBit
    | RuntimeRaceSexMenuBit
    | RuntimeDialogMenuBit
    | RuntimeVatsMenuBit
    | RuntimePipBoyMenuBit;
constexpr std::uint32_t RuntimeBlockingMenuBits =
    RuntimeStartMenuBit
    | RuntimeRaceSexMenuBit
    | RuntimeDialogMenuBit
    | RuntimeVatsMenuBit
    | RuntimeLoadingMenuBit
    | RuntimePipBoyMenuBit;

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

struct SharedXInputState
{
    std::uint32_t magic;
    std::uint32_t version;
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
    std::uint32_t frame;
    LONG clientX;
    LONG clientY;
    std::uint32_t pointerActive;
    std::uint32_t mouseClickPacket;
    std::uint32_t keyboardAcceptPacket;
    std::uint32_t menuInputActive;
    std::uint32_t gameplayControlsActive;
    std::int32_t leftStickX;
    std::int32_t leftStickY;
    std::int32_t rightStickX;
    std::int32_t rightStickY;
    std::uint32_t headLookActive;
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
    // Occupies the former trailing alignment padding, keeping this mapping at
    // 208 bytes while allowing retail IK to reject inactive controllers.
    std::uint32_t trackingFlags;
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
    std::uint32_t sequence;
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

static_assert(sizeof(SharedXInputState) == 28, "SharedXInputState layout changed");
static_assert(sizeof(SharedDInputState) == 96, "SharedDInputState layout changed unexpectedly");
static_assert(sizeof(SharedVrPoseState) == 208, "SharedVrPoseState layout changed unexpectedly");
static_assert(sizeof(SharedCameraState) == 80, "SharedCameraState layout changed");
static_assert(sizeof(SharedRuntimeState) == 88, "SharedRuntimeState layout changed");
static_assert(sizeof(SharedD3D9FrameHeader) == 28, "SharedD3D9FrameHeader layout changed");
static_assert(sizeof(SharedD3D9StereoFrameHeader) == 152, "SharedD3D9StereoFrameHeader layout changed");
static_assert(sizeof(SharedPlayerState) == 160, "SharedPlayerState layout changed");
static_assert(sizeof(SharedCommandState) == 216, "SharedCommandState layout changed");
static_assert(sizeof(SharedInputEvent) == 32, "SharedInputEvent layout changed");
static_assert(sizeof(SharedInputEventQueue) == 2088, "SharedInputEventQueue layout changed");
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
static_assert(offsetof(SharedD3D9StereoFrameHeader, renderedDisplayTime) == 48, "SharedD3D9StereoFrameHeader renderedDisplayTime offset changed");
static_assert(offsetof(SharedD3D9StereoFrameHeader, leftEyeRot) == 56, "SharedD3D9StereoFrameHeader leftEyeRot offset changed");
static_assert(offsetof(SharedD3D9StereoFrameHeader, producerMode) == 144, "SharedD3D9StereoFrameHeader producerMode offset changed");
static_assert(offsetof(SharedD3D9StereoFrameHeader, renderPairSequence) == 148, "SharedD3D9StereoFrameHeader renderPairSequence offset changed");
}
