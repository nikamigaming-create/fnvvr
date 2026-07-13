#define WIN32_LEAN_AND_MEAN

#include "fnvxr_protocol.h"
#include "fnvxr_shared_state.h"
#include "fnvxr_fabrik.h"

#include <windows.h>
#include <dbghelp.h>
#include <intrin.h>

#include <atomic>
#include <cctype>
#include <cstdarg>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <memory>
#include <queue>
#include <thread>
#include <vector>

namespace
{
using UInt32 = std::uint32_t;
using UInt16 = std::uint16_t;
using UInt64 = std::uint64_t;
using UInt8 = std::uint8_t;
using PluginHandle = UInt32;

constexpr UInt32 PluginVersion = 1;
constexpr UInt32 PluginInfoVersion = 1;
constexpr UInt32 InvalidPluginHandle = 0xffffffff;
constexpr UInt32 InterfaceConsole = 1;
constexpr UInt32 InterfaceMessaging = 2;
constexpr UInt32 InterfaceData = 7;
constexpr UInt32 InterfacePlayerControls = 10;
constexpr UInt32 NvseDataDiHookControl = 1;
constexpr UInt32 MouseButtonOffset = 256;
constexpr UInt32 MaxDirectInputMacros = MouseButtonOffset + 8 + 2;
constexpr UInt32 DIK_ESCAPE = 0x01;
constexpr UInt32 DIK_1 = 0x02;
constexpr UInt32 DIK_2 = 0x03;
constexpr UInt32 DIK_3 = 0x04;
constexpr UInt32 DIK_4 = 0x05;
constexpr UInt32 DIK_5 = 0x06;
constexpr UInt32 DIK_6 = 0x07;
constexpr UInt32 DIK_7 = 0x08;
constexpr UInt32 DIK_8 = 0x09;
constexpr UInt32 DIK_TAB = 0x0F;
constexpr UInt32 DIK_W = 0x11;
constexpr UInt32 DIK_E = 0x12;
constexpr UInt32 DIK_R = 0x13;
constexpr UInt32 DIK_T = 0x14;
constexpr UInt32 DIK_A = 0x1E;
constexpr UInt32 DIK_S = 0x1F;
constexpr UInt32 DIK_D = 0x20;
constexpr UInt32 DIK_LCONTROL = 0x1D;
constexpr UInt32 DIK_F = 0x21;
constexpr UInt32 DIK_LSHIFT = 0x2A;
constexpr UInt32 DIK_Z = 0x2C;
constexpr UInt32 DIK_X = 0x2D;
constexpr UInt32 DIK_SPACE = 0x39;
constexpr UInt32 DIK_RETURN = 0x1C;
constexpr UInt32 DIK_UP = 0xC8;
constexpr UInt32 DIK_LEFT = 0xCB;
constexpr UInt32 DIK_RIGHT = 0xCD;
constexpr UInt32 DIK_DOWN = 0xD0;
constexpr UInt32 TileValueId = 0x0FA9;
constexpr UInt32 TileValueClicked = 0x0FC7;
constexpr UInt32 TileValueX = 0x0FA1;
constexpr UInt32 TileValueY = 0x0FA2;
constexpr UInt32 TileValueVisible = 0x0FA3;
constexpr UInt32 TileValueMouseover = 0x0FC6;
constexpr UInt32 TileValueHeight = 0x0FAF;
constexpr UInt32 TileValueWidth = 0x0FB0;
constexpr UInt32 InterfaceManagerAddress = 0x011D8A80;
constexpr UInt32 PlayerCharacterAddress = 0x011DEA3C;
constexpr UInt32 Camera1stNodeAddress = 0x011E07D0;
constexpr UInt32 Camera3rdNodeAddress = 0x011E07D4;
// xNVSE GameObjects.cpp calls this g_1stPersonCameraBipedNode.  It is a
// distinct scene root from Camera1st and is the first safe point to inspect
// for separating the retail view-model from headset rotation.
constexpr UInt32 Camera1stBipedNodeAddress = 0x011E07D8;
constexpr UInt32 PlayerCharacterToggleFirstPersonAddress = 0x00950110;
constexpr UInt32 DisableAutoVanityModeSettingAddress = 0x011E09E0;
constexpr UInt32 MenuVisibilityArrayAddress = 0x011F308F;
constexpr UInt32 TileMenuArrayAddress = 0x011F3508;
constexpr UInt32 IsMenuModeAddress = 0x00702360;
constexpr UInt32 TileSetFloatValueAddress = 0x00A012D0;
constexpr UInt32 TraitNameToIDAddress = 0x00A01860;
constexpr UInt32 PlayerCharacterUpdateCameraAddress = 0x0094AE40;
constexpr UInt32 PlayerCharacterRetrieveRootNodeAddress = 0x00950BB0;
constexpr UInt32 PlayerCharacterGetActorAnimDataAddress = 0x00950A60;
constexpr UInt32 ApplyActorAnimDataAddress = 0x00493BD0;
// New Vegas Enhanced Camera 1.4c identified this retail call site as the
// boundary immediately after ActorAnimData is applied.  Its public source is
// credited here because that boundary and the NiAVObject layout below are the
// foundation for applying VR IK after (rather than before) retail animation.
constexpr UInt32 PlayerAnimationApplyCallSiteAddress = 0x0088882F;
constexpr UInt32 PlayerAnimationApplyReturnAddress = 0x00888834;
constexpr UInt32 NiRttiNiAvObjectAddress = 0x011F4280;
constexpr UInt32 NiRttiNiNodeAddress = 0x011F4428;
constexpr UInt32 NiRttiBsFadeNodeAddress = 0x011F9140;
constexpr std::uintptr_t TESFormRefIdOffset = 0x0C;
constexpr std::uintptr_t TESFormTypeIdOffset = 0x04;
constexpr std::uintptr_t InterfaceManagerCrosshairRefOffset = 0x0FC;
constexpr std::uintptr_t TESObjectRefrBaseFormOffset = 0x20;
constexpr std::uintptr_t TESObjectRefrParentCellOffset = 0x40;
constexpr std::uintptr_t MobileObjectBaseProcessOffset = 0x68;
constexpr std::uintptr_t MiddleHighProcessWeaponOutOffset = 0x135;
constexpr std::uintptr_t MiddleHighProcessProjectileNodeOffset = 0x130;
// Verified against the live retail body of PlayerCharacter::ToggleFirstPerson
// at 0x00950110.  Offset 0x64A is a separate first-person-node transition
// flag; treating it as the camera mode causes false third-person transitions.
constexpr std::uintptr_t PlayerCharacterIsThirdPersonOffset = 0x64C;
constexpr std::uintptr_t PlayerCharacterFirstPersonNodeOffset = 0x694;
constexpr std::uintptr_t TESObjectRefrRenderStateOffset = 0x64;
constexpr std::uintptr_t TESObjectRefrRenderStateRootNodeOffset = 0x14;
constexpr std::uintptr_t NiObjectNetNameOffset = 0x08;
constexpr std::uintptr_t NiAvObjectParentOffset = 0x18;
constexpr std::uintptr_t NiAvObjectFlagsOffset = 0x30;
constexpr std::uintptr_t NiAvObjectLocalRotationOffset = 0x34;
constexpr std::uintptr_t NiAvObjectLocalTranslationOffset = 0x58;
constexpr std::uintptr_t NiAvObjectLocalScaleOffset = 0x64;
constexpr std::uintptr_t NiAvObjectWorldRotationOffset = 0x68;
constexpr std::uintptr_t NiAvObjectWorldTranslationOffset = 0x8C;
constexpr std::uintptr_t NiAvObjectWorldScaleOffset = 0x98;
constexpr std::uintptr_t NiNodeChildrenOffset = 0x9C;
constexpr UInt32 MessageMainGameLoop = 20;
constexpr UInt32 XInputSharedMagic = fnvxr::shared::XInputSharedMagic;
constexpr UInt32 XInputSharedVersion = fnvxr::shared::XInputSharedVersion;
constexpr UInt32 DInputSharedMagic = fnvxr::shared::DInputSharedMagic;
constexpr UInt32 DInputSharedVersion = fnvxr::shared::DInputSharedVersion;
constexpr UInt32 VrPoseSharedMagic = fnvxr::shared::VrPoseSharedMagic;
constexpr UInt32 VrPoseSharedVersion = fnvxr::shared::VrPoseSharedVersion;
constexpr UInt32 CameraSharedMagic = fnvxr::shared::CameraSharedMagic;
constexpr UInt32 CameraSharedVersion = fnvxr::shared::CameraSharedVersion;
constexpr UInt32 RuntimeSharedMagic = fnvxr::shared::RuntimeSharedMagic;
constexpr UInt32 RuntimeSharedVersion = fnvxr::shared::RuntimeSharedVersion;
constexpr UInt32 PlayerSharedMagic = fnvxr::shared::PlayerSharedMagic;
constexpr UInt32 PlayerSharedVersion = fnvxr::shared::PlayerSharedVersion;
constexpr UInt32 CommandSharedMagic = fnvxr::shared::CommandSharedMagic;
constexpr UInt32 CommandSharedVersion = fnvxr::shared::CommandSharedVersion;
constexpr UInt32 InputEventSharedMagic = fnvxr::shared::InputEventSharedMagic;
constexpr UInt32 InputEventSharedVersion = fnvxr::shared::InputEventSharedVersion;
constexpr UInt32 InputEventQueueLength = fnvxr::shared::InputEventQueueLength;
constexpr LONG SharedVideoPointerWidth = 1280;
constexpr LONG SharedVideoPointerHeight = 720;

constexpr UInt32 kMenuTypeInventory = 0x3EA;
constexpr UInt32 kMenuTypeMin = 0x3E9;
constexpr UInt32 kMenuTypeStats = 0x3EB;
constexpr UInt32 kMenuTypeHUDMain = 0x3EC;
constexpr UInt32 kMenuTypeLoading = 0x3EF;
// xNVSE GameUI.h: 0x3F0 is ContainerMenu; DialogMenu is 0x3F1.
constexpr UInt32 kMenuTypeDialog = 0x3F1;
constexpr UInt32 kMenuTypeStart = 0x3F5;
constexpr UInt32 kMenuTypeMap = 0x3FF;
constexpr UInt8 kFormTypeTESNPC = 0x2A;
constexpr UInt8 kFormTypeTESCreature = 0x2B;
constexpr UInt8 kFormTypeCharacter = 0x3B;
constexpr UInt8 kFormTypeCreature = 0x3C;
constexpr UInt32 kCheyenneRefId = 0x0010588E;
constexpr UInt32 kCheyenneBaseId = 0x0010588D;
constexpr UInt16 XInputDpadUp = 0x0001;
constexpr UInt16 XInputDpadDown = 0x0002;
constexpr UInt16 XInputDpadLeft = 0x0004;
constexpr UInt16 XInputDpadRight = 0x0008;
constexpr UInt16 XInputStart = 0x0010;
constexpr UInt16 XInputBack = 0x0020;
constexpr UInt16 XInputLeftThumb = 0x0040;
constexpr UInt16 XInputRightThumb = 0x0080;
constexpr UInt16 XInputA = 0x1000;
constexpr UInt16 XInputB = 0x2000;
constexpr UInt16 XInputX = 0x4000;
constexpr UInt16 XInputY = 0x8000;
constexpr UInt32 kMenuTypeRaceSex = 0x40C;
constexpr UInt32 kMenuTypeVats = 0x420;
constexpr UInt32 kMenuTypeMax = 0x43C;

using fnvxr::shared::SharedXInputState;
using fnvxr::shared::SharedDInputState;
using fnvxr::shared::SharedVrPoseState;
using fnvxr::shared::SharedVrOriginState;
using fnvxr::shared::SharedCameraState;
using fnvxr::shared::SharedRuntimeState;
using fnvxr::shared::SharedPlayerState;
using fnvxr::shared::SharedCommandState;
using fnvxr::shared::SharedInputEventQueue;

struct NVSEInterface
{
    UInt32 nvseVersion;
    UInt32 runtimeVersion;
    UInt32 editorVersion;
    UInt32 isEditor;
    bool (*RegisterCommand)(void* info);
    void (*SetOpcodeBase)(UInt32 opcode);
    void* (*QueryInterface)(UInt32 id);
    PluginHandle (*GetPluginHandle)();
    bool (*RegisterTypedCommand)(void* info, UInt32 returnType);
    const char* (*GetRuntimeDirectory)();
};

struct PluginInfo
{
    UInt32 infoVersion;
    const char* name;
    UInt32 version;
};

struct NVSEDataInterface
{
    UInt32 version;
    void* (*GetSingleton)(UInt32 singletonID);
};

struct NVSEConsoleInterface
{
    UInt32 version;
    bool (*RunScriptLine)(const char* text, void* callingRefr);
    bool (*RunScriptLine2)(const char* text, void* callingRefr, bool suppressConsoleOutput);
};

struct NVSEPlayerControlsInterface
{
    void (__fastcall* DisablePlayerControlsAlt)(UInt32 flagsToAdd, const char* modName);
    void (__fastcall* EnablePlayerControlsAlt)(UInt32 flagsToRemove, const char* modName);
    bool (__cdecl* GetPlayerControlsDisabledAlt)(UInt32 disabledHow, UInt32 flagsToCheck, const char* modName);
    UInt32 (__fastcall* GetDisabledPlayerControls)(UInt32 disabledHow, const char* modName);
};

struct NVSEMessagingInterface
{
    struct Message
    {
        const char* sender;
        UInt32 type;
        UInt32 dataLen;
        void* data;
    };

    using EventCallback = void (*)(Message* msg);

    UInt32 version;
    bool (*RegisterListener)(PluginHandle listener, const char* sender, EventCallback handler);
    bool (*Dispatch)(PluginHandle sender, UInt32 messageType, void* data, UInt32 dataLen, const char* receiver);
};

struct DirectInputKeyInfo
{
    bool rawState;
    bool gameState;
    bool insertedState;
    bool hold;
    bool tap;
    bool userDisable;
    bool scriptDisable;
};

struct DirectInputDeviceObjectData
{
    DWORD dwOfs;
    DWORD dwData;
    DWORD dwTimeStamp;
    DWORD dwSequence;
    ULONG_PTR uAppData;
};

struct DirectInputHookControl
{
    void* vtable;
    DirectInputKeyInfo keys[MaxDirectInputMacros];
    std::queue<DirectInputDeviceObjectData> bufferedPresses;
};

struct TileValue
{
    UInt32 id;
    void* parent;
    float num;
    char* str;
    void* action;
};

struct TileListNode
{
    void* data;
    TileListNode* next;
};

struct TileChildNode
{
    TileChildNode* next;
    TileChildNode* prev;
    void* child;
};

struct NiTArrayRaw
{
    void* vtable;
    void** data;
    UInt16 capacity;
    UInt16 firstFreeEntry;
    UInt16 numObjs;
    UInt16 growSize;
};

struct MenuButtonCandidate
{
    void* tile;
    UInt32 buttonId;
    float x;
    float y;
    float width;
    float height;
};

struct PointerMenuPoint
{
    float x;
    float y;
    const char* space;
};

struct Vec3
{
    float x;
    float y;
    float z;
};

struct Quat
{
    float x;
    float y;
    float z;
    float w;
};

struct Matrix33
{
    float m[3][3];
};

struct NiRttiRaw
{
    const char* name;
    NiRttiRaw* parent;
};

struct VrRigPoseSnapshot
{
    LONG sequence {};
    UInt64 frame {};
    std::int64_t predictedDisplayTime {};
    UInt32 trackingFlags {};
    UInt32 referenceSpaceGeneration {};
    UInt32 producerEpoch {};
    Quat hmdRot { 0.0f, 0.0f, 0.0f, 1.0f };
    Vec3 hmdPos {};
    Quat leftRot { 0.0f, 0.0f, 0.0f, 1.0f };
    Vec3 leftPos {};
    Quat rightRot { 0.0f, 0.0f, 0.0f, 1.0f };
    Vec3 rightPos {};
    Quat leftAimRot { 0.0f, 0.0f, 0.0f, 1.0f };
    Vec3 leftAimPos {};
    Quat rightAimRot { 0.0f, 0.0f, 0.0f, 1.0f };
    Vec3 rightAimPos {};
};

struct RetailArmNodes
{
    void* clavicle {};
    void* upperArm {};
    void* forearm {};
    void* hand {};
};

struct RetailRigNodes
{
    void* root {};
    RetailArmNodes left {};
    RetailArmNodes right {};
    void* weapon {};
    void* projectileNode {};
    void* muzzleFlash {};
};

struct RetailHandCalibration
{
    bool valid {};
    bool usesAimOrientation {};
    Matrix33 controllerToHandRotation {};
    Vec3 controllerToWristLocal {};
};

struct RetailWeaponCalibration
{
    bool valid {};
    Matrix33 controllerToWeaponRotation {};
    Vec3 controllerToWeaponPosition {};
};

struct ShowroomScene
{
    const char* name;
    const char* loadCommand;
    const char* postCommands[8];
    UInt32 expectedCellFormId;
};

enum class ShowroomPhase : UInt32
{
    Idle,
    Loading,
    PostLoad,
    Settled,
};

enum class RuntimePhase : UInt32
{
    Unknown = 0,
    Menu = 1,
    Loading = 2,
    Gameplay = 3,
};

PluginHandle g_pluginHandle = InvalidPluginHandle;
const NVSEInterface* g_nvse = nullptr;
NVSEConsoleInterface* g_console = nullptr;
NVSEMessagingInterface* g_messaging = nullptr;
NVSEPlayerControlsInterface* g_playerControls = nullptr;
DirectInputHookControl* g_directInputHook = nullptr;
bool g_publishedDirectInputHoldKnown[MaxDirectInputMacros] {};
bool g_publishedDirectInputHoldState[MaxDirectInputMacros] {};
bool g_publishedDirectInputHoldViaHook[MaxDirectInputMacros] {};
using BufferedKeyTapFn = void (__thiscall*)(void*, UInt32);
BufferedKeyTapFn g_bufferedKeyTap = nullptr;
bool g_triedResolveBufferedKeyTap = false;
bool g_loggedBufferedSymbolEnum = false;
std::atomic<UInt32> g_pendingAcceptClicks { 0 };
std::atomic<LONG> g_latestPointerX { 0 };
std::atomic<LONG> g_latestPointerY { 0 };
std::atomic<bool> g_latestPointerValid { false };
std::atomic<UInt64> g_latestPointerFrame { 0 };
UInt32 g_lastConsumedDInputMouseClickPacket = 0;
UInt32 g_lastPublishedDInputMouseClickPacket = 0;
UInt32 g_loggedExternalDInputClicks = 0;
UInt32 g_lastExternalDInputPointerFrame = 0;
LONG g_lastExternalDInputPointerX = LONG_MIN;
LONG g_lastExternalDInputPointerY = LONG_MIN;
bool g_lastExternalDInputPointerActive = false;
UInt32 g_loggedExternalDInputPointers = 0;
SharedDInputState g_lastStableExternalDInput {};
bool g_haveLastStableExternalDInput = false;
UInt32 g_lastExternalDInputSourceFrame = 0;
UInt64 g_lastExternalDInputAdvanceMs = 0;
UInt32 g_lastExternalXInputPacket = 0;
UInt16 g_lastExternalXInputButtons = 0;
UInt32 g_lastExternalXInputNavMask = 0;
UInt64 g_lastExternalXInputNavMs = 0;
UInt32 g_loggedExternalXInput = 0;
SharedXInputState g_lastStableExternalXInput {};
bool g_haveLastStableExternalXInput = false;
bool g_externalXInputNeutral = false;
UInt32 g_lastExternalXInputSourcePacket = 0;
UInt32 g_lastReturnedExternalXInputSourcePacket = 0;
UInt32 g_effectiveExternalXInputPacket = 0;
UInt64 g_lastExternalXInputAdvanceMs = 0;
bool g_gameplayAutoRunEnabled = false;
UInt64 g_lastGameplayAutoRunToggleMs = 0;
UInt8 g_gameplayMovementMode = 0;
UInt64 g_lastGameplayMovementModeToggleMs = 0;
bool g_gameplayWalkModeEnabled = false;
bool g_gameplayRunModeEnabled = false;
UInt64 g_lastGameplayRunModeToggleMs = 0;
bool g_thirdPersonL3Held = false;
bool g_thirdPersonL3ChordUsed = false;
UInt64 g_thirdPersonL3DownMs = 0;
void* g_directMenuSelectionMenu = nullptr;
void* g_directMenuSelectionTile = nullptr;
UInt32 g_directMenuSelectionIndex = 0;
UInt32 g_directMenuSelectionLogCount = 0;
void* g_directMenuPointerHoverMenu = nullptr;
void* g_directMenuPointerHoverTile = nullptr;
UInt32 g_directMenuPointerHoverLogCount = 0;
UInt32 g_directMenuPointerMissDetailLogCount = 0;
HANDLE g_xinputMapping = nullptr;
SharedXInputState* g_xinputState = nullptr;
HANDLE g_dinputMapping = nullptr;
SharedDInputState* g_dinputState = nullptr;
HANDLE g_vrPoseMapping = nullptr;
SharedVrPoseState* g_vrPoseState = nullptr;
HANDLE g_vrOriginMapping = nullptr;
SharedVrOriginState* g_vrOriginState = nullptr;
HANDLE g_cameraMapping = nullptr;
SharedCameraState* g_cameraState = nullptr;
HANDLE g_runtimeMapping = nullptr;
SharedRuntimeState* g_runtimeState = nullptr;
HANDLE g_playerMapping = nullptr;
SharedPlayerState* g_playerState = nullptr;
HANDLE g_commandMapping = nullptr;
SharedCommandState* g_commandState = nullptr;
UInt32 g_lastCommandRequestId = 0;
HANDLE g_inputEventMapping = nullptr;
SharedInputEventQueue* g_inputEvents = nullptr;
POINT g_lastMenuPointerClient {};
bool g_hasMenuPointer = false;
std::uint64_t g_loggedPointerFrames = 0;
std::uint64_t g_lastCameraTelemetryFrame = 0;
UInt32 g_lastCameraTelemetryBits = 0xffffffff;
UInt32 g_lastSharedCameraActive = 0xffffffff;
UInt32 g_lastSharedCameraReason = 0xffffffff;
UInt32 g_lastSharedPlayerFlags = 0xffffffff;
UInt32 g_lastSharedPlayerCell = 0xffffffff;
UInt32 g_lastSharedPlayerWeaponClass = 0xffffffff;
UInt32 g_lastKnownWeaponClass = fnvxr::shared::PlayerWeaponClassUnknown;
UInt32 g_lastKnownWeaponFormId = 0;
UInt32 g_lastKnownWeaponFavoriteSlot = 0;
void* g_updateCameraTrampoline = nullptr;
bool g_cameraHookInstalled = false;
bool g_retailRigHookInstalled = false;
RetailRigNodes g_retailRigNodes {};
RetailHandCalibration g_retailLeftCalibration {};
RetailHandCalibration g_retailRightCalibration {};
RetailWeaponCalibration g_retailWeaponCalibration {};
bool g_haveRetailRigOrigin = false;
Quat g_retailRigOriginHmdRot { 0.0f, 0.0f, 0.0f, 1.0f };
Vec3 g_retailRigOriginHmdPos {};
void* g_retailRigOriginBodyRoot = nullptr;
Vec3 g_retailRigBodyAnchorLocal {};
UInt32 g_retailRigReferenceSpaceGeneration = 0;
UInt32 g_retailRigProducerEpoch = 0;
UInt32 g_retailRigOriginPoseSequence = 0;
LONG g_retailRigOriginAuthoritySequence = 0;
LONG g_lastRetailRigPoseSequence = 0;
UInt64 g_retailRigSolveCount = 0;
UInt64 g_retailRigDiscoveryCount = 0;
void* g_lastRetailRigAnimData = nullptr;
bool g_haveRetailRigMotionSample = false;
Vec3 g_previousRetailRigHeadLocalMeters {};
Quat g_previousRetailRigHeadLocalRotation { 0.0f, 0.0f, 0.0f, 1.0f };
Vec3 g_previousRetailRigRightLocalMeters {};
Quat g_previousRetailRigRightLocalRotation { 0.0f, 0.0f, 0.0f, 1.0f };
Vec3 g_previousRetailRigRightTargetLocalUnits {};
Vec3 g_previousRetailRigRightHandLocalUnits {};
Vec3 g_previousRetailRigWeaponWorld {};
Matrix33 g_previousRetailRigWeaponWorldRotation {};
Vec3 g_previousRetailRigBodyWorld {};
Vec3 g_previousRetailRigBodyAnchorWorld {};
Vec3 g_previousRetailRigCameraWorld {};
UInt64 g_retailRigHeadOnlySamples = 0;
UInt64 g_retailRigControllerOnlySamples = 0;
LONG g_retailRigPoseOriginUnavailableCount = 0;
LONG g_retailRigPoseOriginSkewCount = 0;
using GetProjectileNodeFn = void* (__thiscall*)(void*);
GetProjectileNodeFn g_originalGetProjectileNode = nullptr;
void** g_projectileNodeVtable = nullptr;
bool g_projectileNodeHookInstalled = false;
LONG g_projectileNodeConsumeCalls = 0;
LONG g_latestMuzzleProofPoseSequence = 0;
void* g_latestMuzzleProofNode = nullptr;
Vec3 g_latestMuzzleAimForward {};
bool g_haveVrOrigin = false;
Quat g_vrOriginRot { 0.0f, 0.0f, 0.0f, 1.0f };
Vec3 g_vrOriginPos {};
LONG g_lastCameraPoseSequence = 0;
LONG g_lastAppliedCameraPoseSequence = 0;
void* g_lastAppliedCamera = nullptr;
bool g_haveCameraBase = false;
void* g_cameraBaseObject = nullptr;
Matrix33 g_cameraBaseLocalRotation {};
Vec3 g_cameraBaseLocalTranslation {};
Matrix33 g_cameraBaseWorldRotation {};
bool g_showroomActive = false;
ShowroomPhase g_showroomPhase = ShowroomPhase::Idle;
UInt32 g_showroomSceneIndex = 0;
UInt32 g_showroomCommandSerial = 0;
UInt64 g_showroomNextActionMs = 0;
UInt64 g_showroomSceneSettledMs = 0;
bool g_showroomExecutorLogged = false;
bool g_showroomControlsLocked = false;
UInt32 g_showroomCellFormId = 0;
UInt32 g_uiFavoriteWeaponAssignIndex = 0;
UInt32 g_uiFavoriteUtilityAssignIndex = 0;
UInt32 g_uiFavoriteAssignHeldKey = 0;
UInt64 g_uiFavoriteAssignReleaseMs = 0;
UInt64 g_uiFavoriteAssignClickMs = 0;
bool g_uiFavoriteAssignClickPending = false;
bool g_previousUiFavoritePipBoyVisible = false;

bool allowUiInput();
UInt32 currentMenuBits();
bool playerWeaponOut();
UInt32 currentWeaponClass();
bool weaponClassKnown(UInt32 weaponClass);
bool currentWeaponClassKnown();
bool currentWeaponClassMeleeOrUnarmed();
bool playerCombatWeaponReady();
const char* weaponClassName(UInt32 weaponClass);

constexpr ShowroomScene kShowroomScenes[] = {
    {
        "GSDocMitchellHouse",
        "coc GSDocMitchellHouse",
        {
            "player.setpos x 2243",
            "player.setpos y 2276",
            "player.setpos z 7360",
            "player.setangle z 0",
            nullptr,
        },
        0,
    },
    {
        "GSProspectorSaloonInterior",
        "coc GSProspectorSaloonInterior",
        {
            "player.setpos x -250",
            "player.setpos y -350",
            "player.setpos z 3456",
            "player.setangle z 92",
            nullptr,
        },
        0,
    },
    {
        "GoodspringsExteriorEasyPete",
        "cow WastelandNV -17 0",
        {
            "player.setpos x -67845",
            "player.setpos y 3334",
            "player.setpos z 8392",
            "player.setangle z 86",
            nullptr,
        },
        0x000daeb9,
    },
};

bool buildTelemetryPath(char* path, size_t pathSize)
{
    if (!path || pathSize == 0)
        return false;

    path[0] = '\0';
    const DWORD runDirLength = GetEnvironmentVariableA("FNVXR_RUN_LOG_DIR", path, static_cast<DWORD>(pathSize));
    if (runDirLength > 0)
    {
        if (runDirLength >= pathSize)
            return false;
        const size_t length = std::strlen(path);
        if (length > 0 && path[length - 1] != '\\' && path[length - 1] != '/')
        {
            if (strcat_s(path, pathSize, "\\") != 0)
                return false;
        }
        return strcat_s(path, pathSize, "fnvxr_input_telemetry.log") == 0;
    }

    return strcpy_s(path, pathSize, "Data\\NVSE\\Plugins\\fnvxr_input_telemetry.log") == 0;
}

void appendTelemetry(const char* text)
{
    char path[MAX_PATH] {};
    if (!buildTelemetryPath(path, sizeof(path)))
        return;

    HANDLE file = CreateFileA(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;

    DWORD written = 0;
    WriteFile(file, text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
    CloseHandle(file);
}

void logTelemetry(const char* format, ...)
{
    // Structured camera/rig proofs contain the full source and applied
    // transforms.  A 512-byte buffer silently truncated those records into
    // invalid JSON, making the telemetry look present while discarding the
    // evidence needed to verify independence.
    char line[16384] {};
    va_list args;
    va_start(args, format);
    const int required = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (required < 0 || required >= static_cast<int>(sizeof(line)))
    {
        static volatile LONG truncations = 0;
        const LONG count = InterlockedIncrement(&truncations);
        char failure[256] {};
        sprintf_s(
            failure,
            "{\"event\":\"fnvxrTelemetryTruncated\",\"count\":%ld,\"bufferBytes\":%zu,\"requiredBytes\":%d}\n",
            count,
            sizeof(line),
            required);
        appendTelemetry(failure);
        return;
    }
    appendTelemetry(line);
}

const char* sharedCameraReasonName(UInt32 reason)
{
    switch (reason)
    {
        case 0: return "active";
        case 1: return "not-gameplay";
        case 2: return "missing-camera";
        default: return "unknown";
    }
}

bool isCompatibleRuntime(const NVSEInterface* nvse)
{
    if (!nvse)
        return false;

    if (nvse->isEditor)
        return false;

    return nvse->nvseVersion != 0;
}

bool bridgeDisabledByEnv()
{
    char buffer[8] {};
    size_t required = 0;
    return getenv_s(&required, buffer, sizeof(buffer), "FNVXR_DISABLE_BRIDGE") == 0
        && required > 0
        && buffer[0] == '1';
}

bool runProfileIs(const char* expected)
{
    if (!expected)
        return false;

    char buffer[32] {};
    size_t required = 0;
    return getenv_s(&required, buffer, sizeof(buffer), "FNVXR_RUN_PROFILE") == 0
        && required > 0
        && _stricmp(buffer, expected) == 0;
}

bool rockSolidProfile()
{
    return runProfileIs("rock-solid");
}

bool retailSidecarProfile()
{
    return runProfileIs("retail-sidecar") || runProfileIs("openxr-sidecar");
}

bool scenePipelineModeIs(const char* expected)
{
    char buffer[32] {};
    size_t required = 0;
    return expected
        && getenv_s(&required, buffer, sizeof(buffer), "FNVXR_SCENE_PIPELINE_MODE") == 0
        && required > 0
        && _stricmp(buffer, expected) == 0;
}

bool envEnabled(const char* name, bool fallback)
{
    char buffer[8] {};
    size_t required = 0;
    if (getenv_s(&required, buffer, sizeof(buffer), name) != 0 || required == 0)
    {
        if (retailSidecarProfile())
        {
            if (_stricmp(name, "FNVXR_DIRECT_UI_CLICK") == 0)
                return false;
            if (_stricmp(name, "FNVXR_POINTER_TILE_FALLBACK") == 0)
                return false;
            if (_stricmp(name, "FNVXR_QUEUE_ACCEPT_CLICK") == 0)
                return true;
            if (_stricmp(name, "FNVXR_CLICK_CLEAR_CLIP") == 0)
                return true;
            if (_stricmp(name, "FNVXR_D3D9_USE_SHARED_CAMERA_VIEW") == 0)
                return true;
            if (_stricmp(name, "FNVXR_CURSOR_TRACK_POINTER") == 0)
                return false;
            if (_stricmp(name, "FNVXR_CURSOR_FOCUS") == 0)
                return false;
            if (_stricmp(name, "FNVXR_CLICK_FOCUS_ON_CLICK") == 0)
                return false;
            if (_stricmp(name, "FNVXR_CLICK_SENDINPUT_MOUSE") == 0)
                return false;
            if (_stricmp(name, "FNVXR_PLUGIN_SENDINPUT_CLICK") == 0)
                return false;
            if (_stricmp(name, "FNVXR_IMMEDIATE_OS_CLICK") == 0)
                return false;
            if (_stricmp(name, "FNVXR_CLICK_LEGACY_FALLBACK_AFTER_DIRECT") == 0)
                return false;
            if (_stricmp(name, "FNVXR_ACCEPT_REPEAT") == 0)
                return false;
            if (_stricmp(name, "FNVXR_CAMERA_APPLY") == 0)
                return false;
            if (_stricmp(name, "FNVXR_NVSE_WRITES_VR_POSE") == 0)
                return false;
        }
        if (rockSolidProfile())
        {
            if (_stricmp(name, "FNVXR_CAMERA_HOOK") == 0)
                return true;
            if (_stricmp(name, "FNVXR_MENU_SCENE_CAROUSEL") == 0)
                return scenePipelineModeIs("producer") || scenePipelineModeIs("live");
            if (_stricmp(name, "FNVXR_EXPERIMENTAL_MUTATE_GAME_SCENE") == 0)
                return scenePipelineModeIs("producer") || scenePipelineModeIs("live");
            if (_stricmp(name, "FNVXR_SHOWROOM_LOCK_CONTROLS") == 0)
                return scenePipelineModeIs("producer") || scenePipelineModeIs("live");
            if (_stricmp(name, "FNVXR_SHOWROOM_RESTRAIN_PLAYER") == 0)
                return scenePipelineModeIs("producer") || scenePipelineModeIs("live");
            if (_stricmp(name, "FNVXR_CAMERA_APPLY") == 0)
                return false;
            if (_stricmp(name, "FNVXR_CURSOR_TRACK_POINTER") == 0)
                return false;
            if (_stricmp(name, "FNVXR_CURSOR_FOCUS") == 0)
                return false;
            if (_stricmp(name, "FNVXR_CLICK_FOCUS_ON_CLICK") == 0)
                return false;
            if (_stricmp(name, "FNVXR_CLICK_SENDINPUT_MOUSE") == 0)
                return false;
            if (_stricmp(name, "FNVXR_PLUGIN_SENDINPUT_CLICK") == 0)
                return false;
            if (_stricmp(name, "FNVXR_IMMEDIATE_OS_CLICK") == 0)
                return false;
            if (_stricmp(name, "FNVXR_QUEUE_ACCEPT_CLICK") == 0)
                return true;
            if (_stricmp(name, "FNVXR_DIRECT_UI_CLICK") == 0)
                return false;
            if (_stricmp(name, "FNVXR_POINTER_TILE_FALLBACK") == 0)
                return false;
            if (_stricmp(name, "FNVXR_CLICK_CLEAR_CLIP") == 0)
                return true;
        }
        return fallback;
    }

    return buffer[0] != '0';
}

bool envEqualsIgnoreCase(const char* name, const char* expected)
{
    char buffer[32] {};
    size_t required = 0;
    if (getenv_s(&required, buffer, sizeof(buffer), name) != 0 || required == 0)
        return false;

    return _stricmp(buffer, expected) == 0;
}

float getFloatFromEnv(const char* name, float fallback)
{
    char buffer[32] {};
    size_t required = 0;
    if (getenv_s(&required, buffer, sizeof(buffer), name) != 0 || required == 0)
    {
        if (rockSolidProfile())
        {
            if (_stricmp(name, "FNVXR_CAMERA_TELEMETRY_FRAMES") == 0)
                return 60.0f;
            if (_stricmp(name, "FNVXR_ACCEPT_COOLDOWN_FRAMES") == 0)
                return 18.0f;
        }
        return fallback;
    }

    char* end = nullptr;
    const float value = std::strtof(buffer, &end);
    return end != buffer ? value : fallback;
}

int getIntFromEnv(const char* name, int fallback)
{
    char buffer[32] {};
    size_t required = 0;
    if (getenv_s(&required, buffer, sizeof(buffer), name) != 0 || required == 0)
        return fallback;

    char* end = nullptr;
    const long value = std::strtol(buffer, &end, 10);
    return end != buffer && value > 1 ? static_cast<int>(value) : fallback;
}

void* readPointer(std::uintptr_t address)
{
    __try
    {
        return *reinterpret_cast<void**>(address);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

float readFloat(std::uintptr_t address, float fallback = 0.0f)
{
    __try
    {
        const float value = *reinterpret_cast<float*>(address);
        return std::isfinite(value) ? value : fallback;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return fallback;
    }
}

UInt8 readUInt8(std::uintptr_t address, UInt8 fallback = 0)
{
    __try
    {
        return *reinterpret_cast<UInt8*>(address);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return fallback;
    }
}

UInt32 readUInt32(std::uintptr_t address, UInt32 fallback = 0)
{
    __try
    {
        return *reinterpret_cast<UInt32*>(address);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return fallback;
    }
}

template <typename T>
T pointerFromAddress32(UInt32 address)
{
    return reinterpret_cast<T>(static_cast<std::uintptr_t>(address));
}

template <typename T>
UInt32 address32FromPointer(T pointer)
{
    return static_cast<UInt32>(reinterpret_cast<std::uintptr_t>(pointer));
}

Quat normalizeQuat(Quat value)
{
    const float length = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z + value.w * value.w);
    if (length < 0.000001f || !std::isfinite(length))
        return { 0.0f, 0.0f, 0.0f, 1.0f };

    const float inv = 1.0f / length;
    return { value.x * inv, value.y * inv, value.z * inv, value.w * inv };
}

Quat conjugateQuat(Quat value)
{
    return { -value.x, -value.y, -value.z, value.w };
}

Quat multiplyQuat(Quat a, Quat b)
{
    return normalizeQuat({
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
    });
}

Matrix33 matrixFromQuat(Quat q)
{
    q = normalizeQuat(q);
    const float xx = q.x * q.x;
    const float yy = q.y * q.y;
    const float zz = q.z * q.z;
    const float xy = q.x * q.y;
    const float xz = q.x * q.z;
    const float yz = q.y * q.z;
    const float wx = q.w * q.x;
    const float wy = q.w * q.y;
    const float wz = q.w * q.z;

    Matrix33 result {};
    result.m[0][0] = 1.0f - 2.0f * (yy + zz);
    result.m[0][1] = 2.0f * (xy - wz);
    result.m[0][2] = 2.0f * (xz + wy);
    result.m[1][0] = 2.0f * (xy + wz);
    result.m[1][1] = 1.0f - 2.0f * (xx + zz);
    result.m[1][2] = 2.0f * (yz - wx);
    result.m[2][0] = 2.0f * (xz - wy);
    result.m[2][1] = 2.0f * (yz + wx);
    result.m[2][2] = 1.0f - 2.0f * (xx + yy);
    return result;
}

Matrix33 multiplyMatrix33(const Matrix33& a, const Matrix33& b)
{
    Matrix33 result {};
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 3; ++column)
        {
            for (int inner = 0; inner < 3; ++inner)
                result.m[row][column] += a.m[row][inner] * b.m[inner][column];
        }
    }
    return result;
}

Matrix33 yawOnlyMatrix(Matrix33 matrix)
{
    const float yaw = std::atan2(matrix.m[1][0], matrix.m[0][0])
        * getFloatFromEnv("FNVXR_CAMERA_YAW_SCALE", 1.0f);
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);
    Matrix33 result {};
    result.m[0][0] = c;
    result.m[0][1] = -s;
    result.m[1][0] = s;
    result.m[1][1] = c;
    result.m[2][2] = 1.0f;
    return result;
}

Matrix33 readMatrix33(std::uintptr_t address)
{
    Matrix33 result {};
    __try
    {
        for (int row = 0; row < 3; ++row)
        {
            for (int column = 0; column < 3; ++column)
                result.m[row][column] = *reinterpret_cast<float*>(address + static_cast<std::uintptr_t>((row * 3 + column) * 4));
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        for (auto& row : result.m)
            for (float& value : row)
                value = NAN;
    }
    return result;
}

bool writeMatrix33(std::uintptr_t address, const Matrix33& matrix)
{
    __try
    {
        for (int row = 0; row < 3; ++row)
        {
            for (int column = 0; column < 3; ++column)
                *reinterpret_cast<float*>(address + static_cast<std::uintptr_t>((row * 3 + column) * 4)) = matrix.m[row][column];
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        logTelemetry("camera matrix write exception address=%p\n", reinterpret_cast<void*>(address));
        return false;
    }
}

Vec3 readVec3(std::uintptr_t address)
{
    Vec3 result {};
    __try
    {
        result.x = *reinterpret_cast<float*>(address + 0);
        result.y = *reinterpret_cast<float*>(address + 4);
        result.z = *reinterpret_cast<float*>(address + 8);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        result = { NAN, NAN, NAN };
    }
    return result;
}

bool writeVec3(std::uintptr_t address, Vec3 value)
{
    __try
    {
        *reinterpret_cast<float*>(address + 0) = value.x;
        *reinterpret_cast<float*>(address + 4) = value.y;
        *reinterpret_cast<float*>(address + 8) = value.z;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        logTelemetry("camera vector write exception address=%p\n", reinterpret_cast<void*>(address));
        return false;
    }
}

Matrix33 xrDeltaToGamebryoMatrix(Quat xrDelta)
{
    const Matrix33 xr = matrixFromQuat(xrDelta);
    Matrix33 result {};
    // OpenXR: +X right, +Y up, -Z forward. Gamebryo/FNV: +X right, +Y forward, +Z up.
    const int xrAxisForGameAxis[3] = { 0, 2, 1 };
    const float signForGameAxis[3] = { 1.0f, -1.0f, 1.0f };
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 3; ++column)
        {
            result.m[row][column] =
                signForGameAxis[row]
                * signForGameAxis[column]
                * xr.m[xrAxisForGameAxis[row]][xrAxisForGameAxis[column]];
        }
    }
    return result;
}

Vec3 xrDeltaToGamebryoVector(Vec3 xrDelta)
{
    return {
        xrDelta.x,
        -xrDelta.z,
        xrDelta.y
    };
}

// Return an OpenXR position in the full gameplay-origin frame.  Applying only
// the origin yaw leaves pitch/roll in the basis and makes horizontal physical
// motion trace an ellipse.  Camera and controller consumers must use this
// exact same origin-space operation.
Vec3 xrPositionInOriginFrame(Quat originRotation, Vec3 originPosition, Vec3 currentPosition)
{
    const Matrix33 inverseOrigin = matrixFromQuat(conjugateQuat(normalizeQuat(originRotation)));
    const Vec3 delta {
        currentPosition.x - originPosition.x,
        currentPosition.y - originPosition.y,
        currentPosition.z - originPosition.z
    };
    return {
        inverseOrigin.m[0][0] * delta.x + inverseOrigin.m[0][1] * delta.y + inverseOrigin.m[0][2] * delta.z,
        inverseOrigin.m[1][0] * delta.x + inverseOrigin.m[1][1] * delta.y + inverseOrigin.m[1][2] * delta.z,
        inverseOrigin.m[2][0] * delta.x + inverseOrigin.m[2][1] * delta.y + inverseOrigin.m[2][2] * delta.z
    };
}

bool readLatestCameraPose(Quat& rotationDelta, Vec3& positionDelta)
{
    if (!g_vrPoseState || g_vrPoseState->magic != VrPoseSharedMagic || g_vrPoseState->version != VrPoseSharedVersion)
        return false;

    LONG sequence = 0;
    Quat currentRot {};
    Vec3 currentPos {};
    bool haveStableSnapshot = false;
    for (int attempt = 0; attempt < 4 && !haveStableSnapshot; ++attempt)
    {
        const LONG sequenceBefore = g_vrPoseState->sequence;
        if (sequenceBefore == 0 || (sequenceBefore & 1) != 0)
            continue;

        MemoryBarrier();
        __try
        {
            currentRot = normalizeQuat({
                g_vrPoseState->hmdRot[0],
                g_vrPoseState->hmdRot[1],
                g_vrPoseState->hmdRot[2],
                g_vrPoseState->hmdRot[3]
            });
            currentPos = {
                g_vrPoseState->hmdPos[0],
                g_vrPoseState->hmdPos[1],
                g_vrPoseState->hmdPos[2]
            };
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
        MemoryBarrier();

        const LONG sequenceAfter = g_vrPoseState->sequence;
        if (sequenceBefore == sequenceAfter)
        {
            sequence = sequenceAfter;
            haveStableSnapshot = true;
        }
    }

    if (!haveStableSnapshot)
        return false;

    if (!g_haveVrOrigin || envEnabled("FNVXR_CAMERA_RESET_ORIGIN", false))
    {
        g_vrOriginRot = currentRot;
        g_vrOriginPos = currentPos;
        g_haveVrOrigin = true;
        logTelemetry(
            "camera origin latched seq=%ld rot=(%.4f %.4f %.4f %.4f) pos=(%.4f %.4f %.4f)\n",
            sequence,
            currentRot.x,
            currentRot.y,
            currentRot.z,
            currentRot.w,
            currentPos.x,
            currentPos.y,
            currentPos.z);
    }

    rotationDelta = multiplyQuat(conjugateQuat(g_vrOriginRot), currentRot);
    positionDelta = xrPositionInOriginFrame(g_vrOriginRot, g_vrOriginPos, currentPos);
    g_lastCameraPoseSequence = sequence;
    return true;
}

void logNiAvObjectTransform(const char* label, void* object)
{
    if (!object)
    {
        logTelemetry("cameraNode %s=null\n", label);
        return;
    }

    const auto base = reinterpret_cast<std::uintptr_t>(object);
    const float localTx = readFloat(base + NiAvObjectLocalTranslationOffset + 0x0);
    const float localTy = readFloat(base + NiAvObjectLocalTranslationOffset + 0x4);
    const float localTz = readFloat(base + NiAvObjectLocalTranslationOffset + 0x8);
    const float localScale = readFloat(base + NiAvObjectLocalScaleOffset, 1.0f);
    const float worldTx = readFloat(base + NiAvObjectWorldTranslationOffset + 0x0);
    const float worldTy = readFloat(base + NiAvObjectWorldTranslationOffset + 0x4);
    const float worldTz = readFloat(base + NiAvObjectWorldTranslationOffset + 0x8);
    const float worldR00 = readFloat(base + NiAvObjectWorldRotationOffset + 0x00);
    const float worldR01 = readFloat(base + NiAvObjectWorldRotationOffset + 0x04);
    const float worldR02 = readFloat(base + NiAvObjectWorldRotationOffset + 0x08);
    const float worldR10 = readFloat(base + NiAvObjectWorldRotationOffset + 0x0C);
    const float worldR11 = readFloat(base + NiAvObjectWorldRotationOffset + 0x10);
    const float worldR12 = readFloat(base + NiAvObjectWorldRotationOffset + 0x14);
    const float worldR20 = readFloat(base + NiAvObjectWorldRotationOffset + 0x18);
    const float worldR21 = readFloat(base + NiAvObjectWorldRotationOffset + 0x1C);
    const float worldR22 = readFloat(base + NiAvObjectWorldRotationOffset + 0x20);
    logTelemetry(
        "cameraNode %s=%p localT=(%.3f %.3f %.3f) localScale=%.3f worldT=(%.3f %.3f %.3f) worldR=[%.3f %.3f %.3f | %.3f %.3f %.3f | %.3f %.3f %.3f]\n",
        label,
        object,
        localTx,
        localTy,
        localTz,
        localScale,
        worldTx,
        worldTy,
        worldTz,
        worldR00,
        worldR01,
        worldR02,
        worldR10,
        worldR11,
        worldR12,
        worldR20,
        worldR21,
        worldR22);
}

void logCameraTelemetry(std::uint64_t frame, UInt32 menuBits, bool force = false)
{
    if (!envEnabled("FNVXR_CAMERA_TELEMETRY", true))
        return;

    const std::uint64_t cadence = static_cast<std::uint64_t>(getFloatFromEnv("FNVXR_CAMERA_TELEMETRY_FRAMES", 120.0f));
    if (!force
        && g_lastCameraTelemetryBits == menuBits
        && cadence > 0
        && frame < g_lastCameraTelemetryFrame + cadence)
    {
        return;
    }

    g_lastCameraTelemetryFrame = frame;
    g_lastCameraTelemetryBits = menuBits;

    void* interfaceManager = readPointer(InterfaceManagerAddress);
    void* sceneGraph004 = interfaceManager
        ? readPointer(reinterpret_cast<std::uintptr_t>(interfaceManager) + 0x004)
        : nullptr;
    void* sceneGraph008 = interfaceManager
        ? readPointer(reinterpret_cast<std::uintptr_t>(interfaceManager) + 0x008)
        : nullptr;
    void* sceneCamera004 = sceneGraph004
        ? readPointer(reinterpret_cast<std::uintptr_t>(sceneGraph004) + 0x0DC)
        : nullptr;
    void* sceneCamera008 = sceneGraph008
        ? readPointer(reinterpret_cast<std::uintptr_t>(sceneGraph008) + 0x0DC)
        : nullptr;
    void* firstPersonCameraNode = readPointer(Camera1stNodeAddress);
    void* thirdPersonCameraNode = readPointer(Camera3rdNodeAddress);
    void* firstPersonBipedNode = readPointer(Camera1stBipedNodeAddress);
    void* firstPersonBipedParent = firstPersonBipedNode
        ? readPointer(reinterpret_cast<std::uintptr_t>(firstPersonBipedNode) + NiAvObjectParentOffset)
        : nullptr;
    void* player = readPointer(PlayerCharacterAddress);
    const UInt8 thirdPerson = player
        ? readUInt8(reinterpret_cast<std::uintptr_t>(player) + PlayerCharacterIsThirdPersonOffset)
        : 0;
    void* firstPersonNode = player
        ? readPointer(reinterpret_cast<std::uintptr_t>(player) + PlayerCharacterFirstPersonNodeOffset)
        : nullptr;
    void* playerRenderState = player
        ? readPointer(reinterpret_cast<std::uintptr_t>(player) + TESObjectRefrRenderStateOffset)
        : nullptr;
    void* thirdPersonRoot = playerRenderState
        ? readPointer(reinterpret_cast<std::uintptr_t>(playerRenderState) + TESObjectRefrRenderStateRootNodeOffset)
        : nullptr;
    const UInt32 firstPersonFlags = firstPersonNode
        ? readUInt32(reinterpret_cast<std::uintptr_t>(firstPersonNode) + NiAvObjectFlagsOffset)
        : 0;
    const UInt32 thirdPersonFlags = thirdPersonRoot
        ? readUInt32(reinterpret_cast<std::uintptr_t>(thirdPersonRoot) + NiAvObjectFlagsOffset)
        : 0;
    const float fov004 = sceneGraph004 ? readFloat(reinterpret_cast<std::uintptr_t>(sceneGraph004) + 0x0EC) : 0.0f;
    const float fov008 = sceneGraph008 ? readFloat(reinterpret_cast<std::uintptr_t>(sceneGraph008) + 0x0EC) : 0.0f;

    logTelemetry(
        "camera frame=%llu bits=0x%02X interface=%p sg004=%p sg008=%p sceneCam004=%p sceneCam008=%p cam1st=%p cam3rd=%p firstBiped=%p firstBipedParent=%p player=%p third=%u firstNode=%p firstFlags=0x%08lX thirdRoot=%p thirdFlags=0x%08lX fov=(%.3f %.3f)\n",
        static_cast<unsigned long long>(frame),
        menuBits,
        interfaceManager,
        sceneGraph004,
        sceneGraph008,
        sceneCamera004,
        sceneCamera008,
        firstPersonCameraNode,
        thirdPersonCameraNode,
        firstPersonBipedNode,
        firstPersonBipedParent,
        player,
        thirdPerson,
        firstPersonNode,
        static_cast<unsigned long>(firstPersonFlags),
        thirdPersonRoot,
        static_cast<unsigned long>(thirdPersonFlags),
        fov004,
        fov008);
    logNiAvObjectTransform("scene004.camera", sceneCamera004);
    logNiAvObjectTransform("scene008.camera", sceneCamera008);
    logNiAvObjectTransform("camera1stNode", firstPersonCameraNode);
    logNiAvObjectTransform("camera3rdNode", thirdPersonCameraNode);
    logNiAvObjectTransform("camera1stBipedNode", firstPersonBipedNode);
    logNiAvObjectTransform("camera1stBipedParent", firstPersonBipedParent);
    logNiAvObjectTransform("player.firstPersonNode", firstPersonNode);
    logNiAvObjectTransform("player.thirdPersonRoot", thirdPersonRoot);
}

void logInputConfig()
{
    logTelemetry(
        "inputConfig cursorTrack=%d cursorFocus=%d sendInputMouse=%d postKeys=%d immediate=%d queue=%d acceptRepeat=%d cooldown=%.1f pointerScale=(%.3f,%.3f) pointerOffset=(%.3f,%.3f) uiShared=%dx%d uiInput=%dx%d\n",
        static_cast<int>(envEnabled("FNVXR_CURSOR_TRACK_POINTER", false)),
        static_cast<int>(envEnabled("FNVXR_CURSOR_FOCUS", false)),
        static_cast<int>(envEnabled("FNVXR_CLICK_SENDINPUT_MOUSE", false)),
        static_cast<int>(envEnabled("FNVXR_POST_MENU_KEYS", false)),
        static_cast<int>(envEnabled("FNVXR_IMMEDIATE_OS_CLICK", false)),
        static_cast<int>(envEnabled("FNVXR_QUEUE_ACCEPT_CLICK", false)),
        static_cast<int>(envEnabled("FNVXR_ACCEPT_REPEAT", false)),
        getFloatFromEnv("FNVXR_ACCEPT_COOLDOWN_FRAMES", 30.0f),
        getFloatFromEnv("FNVXR_POINTER_SCALE_X", 1.0f),
        getFloatFromEnv("FNVXR_POINTER_SCALE_Y", 1.0f),
        getFloatFromEnv("FNVXR_POINTER_OFFSET_X", 0.0f),
        getFloatFromEnv("FNVXR_POINTER_OFFSET_Y", 0.0f),
        getIntFromEnv("FNVXR_UI_SHARED_WIDTH", SharedVideoPointerWidth),
        getIntFromEnv("FNVXR_UI_SHARED_HEIGHT", SharedVideoPointerHeight),
        getIntFromEnv("FNVXR_UI_INPUT_WIDTH", 0),
        getIntFromEnv("FNVXR_UI_INPUT_HEIGHT", 0));
}

bool currentProcessHasForegroundWindow()
{
    HWND foreground = GetForegroundWindow();
    if (!foreground)
        return false;

    DWORD processId = 0;
    GetWindowThreadProcessId(foreground, &processId);
    return processId == GetCurrentProcessId();
}

bool focusProcessWindow(HWND hwnd)
{
    if (!hwnd)
        return false;
    if (currentProcessHasForegroundWindow())
        return true;

    const DWORD currentThreadId = GetCurrentThreadId();
    const DWORD windowThreadId = GetWindowThreadProcessId(hwnd, nullptr);
    HWND foreground = GetForegroundWindow();
    DWORD foregroundThreadId = 0;
    if (foreground)
        foregroundThreadId = GetWindowThreadProcessId(foreground, nullptr);

    const bool attachForeground = foregroundThreadId != 0 && foregroundThreadId != currentThreadId;
    const bool attachWindow = windowThreadId != 0
        && windowThreadId != currentThreadId
        && windowThreadId != foregroundThreadId;
    if (attachForeground)
        AttachThreadInput(currentThreadId, foregroundThreadId, TRUE);
    if (attachWindow)
        AttachThreadInput(currentThreadId, windowThreadId, TRUE);

    ShowWindow(hwnd, SW_RESTORE);
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);
    Sleep(20);
    const bool focused = currentProcessHasForegroundWindow();

    if (attachWindow)
        AttachThreadInput(currentThreadId, windowThreadId, FALSE);
    if (attachForeground)
        AttachThreadInput(currentThreadId, foregroundThreadId, FALSE);
    return focused;
}

std::int16_t thumbValue(float value)
{
    if (value > 1.0f)
        value = 1.0f;
    if (value < -1.0f)
        value = -1.0f;
    return static_cast<std::int16_t>(value * 32767.0f);
}

std::int32_t sharedStickValue(float value)
{
    return static_cast<std::int32_t>(thumbValue(value));
}

UInt8 triggerValue(float value)
{
    if (value > 1.0f)
        value = 1.0f;
    if (value < 0.0f)
        value = 0.0f;
    return static_cast<UInt8>(value * 255.0f);
}

void initSharedXInput()
{
    if (g_xinputState)
        return;

    g_xinputMapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(SharedXInputState),
        fnvxr::shared::XInputSharedMappingName);
    if (!g_xinputMapping)
    {
        logTelemetry("xinput shared CreateFileMapping failed err=%lu\n", GetLastError());
        return;
    }
    const DWORD createError = GetLastError();

    g_xinputState = static_cast<SharedXInputState*>(
        MapViewOfFile(g_xinputMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedXInputState)));
    if (!g_xinputState)
    {
        logTelemetry("xinput shared MapViewOfFile failed err=%lu\n", GetLastError());
        CloseHandle(g_xinputMapping);
        g_xinputMapping = nullptr;
        return;
    }

    SharedXInputState existingSnapshot {};
    const bool existingHeaderValid = createError == ERROR_ALREADY_EXISTS
        && g_xinputState->magic == XInputSharedMagic
        && g_xinputState->version == XInputSharedVersion;
    const bool existingSnapshotValid = existingHeaderValid
        && fnvxr::shared::readSequencedSharedSnapshot(g_xinputState, existingSnapshot, 16);
    const bool existingValid = existingHeaderValid;
    if (!existingValid)
    {
        std::memset(g_xinputState, 0, sizeof(*g_xinputState));
        g_xinputState->magic = XInputSharedMagic;
        g_xinputState->version = XInputSharedVersion;
        // Mapping existence is not producer liveness. The OpenXR host sets
        // connected only when it publishes its first complete input frame.
        g_xinputState->connected = 0;
        g_xinputState->reserved[fnvxr::shared::XInputReservedRetailConsumed] = 0;
        g_xinputState->reserved[fnvxr::shared::XInputReservedAutoRun] = 0;
        g_xinputState->reserved[fnvxr::shared::XInputReservedMovementMode] = 0;
    }
    logTelemetry("xinput shared ready state=%p mapping=%p existing=%d stable=%d\n",
        g_xinputState,
        g_xinputMapping,
        static_cast<int>(existingValid),
        static_cast<int>(existingSnapshotValid));
}

void initSharedDInput()
{
    if (g_dinputState)
        return;

    g_dinputMapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(SharedDInputState),
        fnvxr::shared::DInputSharedMappingName);
    if (!g_dinputMapping)
    {
        logTelemetry("dinput shared CreateFileMapping failed err=%lu\n", GetLastError());
        return;
    }
    const DWORD createError = GetLastError();

    g_dinputState = static_cast<SharedDInputState*>(
        MapViewOfFile(g_dinputMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedDInputState)));
    if (!g_dinputState)
    {
        logTelemetry("dinput shared MapViewOfFile failed err=%lu\n", GetLastError());
        CloseHandle(g_dinputMapping);
        g_dinputMapping = nullptr;
        return;
    }

    SharedDInputState existingSnapshot {};
    const bool existingHeaderValid = createError == ERROR_ALREADY_EXISTS
        && g_dinputState->magic == DInputSharedMagic
        && g_dinputState->version == DInputSharedVersion;
    const bool existingSnapshotValid = existingHeaderValid
        && fnvxr::shared::readSequencedSharedSnapshot(g_dinputState, existingSnapshot, 16);
    const bool existingValid = existingHeaderValid;
    if (!existingValid)
    {
        std::memset(g_dinputState, 0, sizeof(*g_dinputState));
        g_dinputState->magic = DInputSharedMagic;
        g_dinputState->version = DInputSharedVersion;
    }
    const UInt32 initialMouseClickPacket = existingSnapshotValid ? existingSnapshot.mouseClickPacket : 0u;
    const UInt32 initialFrame = existingSnapshotValid ? existingSnapshot.frame : 0u;
    g_lastConsumedDInputMouseClickPacket = initialMouseClickPacket;
    g_lastPublishedDInputMouseClickPacket = initialMouseClickPacket;
    logTelemetry("dinput shared ready state=%p mapping=%p existing=%d stable=%d packet=%lu frame=%lu\n",
        g_dinputState,
        g_dinputMapping,
        static_cast<int>(existingValid),
        static_cast<int>(existingSnapshotValid),
        static_cast<unsigned long>(initialMouseClickPacket),
        static_cast<unsigned long>(initialFrame));
}

bool readSharedXInputFrameSnapshot(SharedXInputState& snapshot)
{
    return fnvxr::shared::readSequencedSharedSnapshot(g_xinputState, snapshot)
        && snapshot.magic == XInputSharedMagic
        && snapshot.version == XInputSharedVersion;
}

bool readEffectiveExternalXInputSnapshot(SharedXInputState& snapshot)
{
    if (!g_xinputState)
        return false;

    SharedXInputState current {};
    const bool currentValid = readSharedXInputFrameSnapshot(current);
    const UInt64 nowMs = GetTickCount64();
    if (currentValid)
    {
        if (!g_haveLastStableExternalXInput || current.packet != g_lastExternalXInputSourcePacket)
        {
            g_lastExternalXInputSourcePacket = current.packet;
            g_lastExternalXInputAdvanceMs = nowMs;
        }
        g_lastStableExternalXInput = current;
        g_haveLastStableExternalXInput = true;
    }

    const UInt64 staleMs = static_cast<UInt64>((std::max)(
        50,
        getIntFromEnv("FNVXR_XINPUT_STALE_PACKET_MS", 250)));
    const bool fresh = g_haveLastStableExternalXInput
        && g_lastExternalXInputAdvanceMs != 0
        && nowMs - g_lastExternalXInputAdvanceMs <= staleMs
        && g_lastStableExternalXInput.connected != 0;
    if (fresh)
    {
        snapshot = g_lastStableExternalXInput;
        if (g_externalXInputNeutral
            || g_effectiveExternalXInputPacket == 0
            || snapshot.packet != g_lastReturnedExternalXInputSourcePacket)
        {
            ++g_effectiveExternalXInputPacket;
            g_lastReturnedExternalXInputSourcePacket = snapshot.packet;
        }
        snapshot.packet = g_effectiveExternalXInputPacket;
        g_externalXInputNeutral = false;
        return true;
    }

    if (!g_externalXInputNeutral)
    {
        ++g_effectiveExternalXInputPacket;
        g_externalXInputNeutral = true;
    }
    snapshot = {};
    snapshot.magic = XInputSharedMagic;
    snapshot.version = XInputSharedVersion;
    snapshot.packet = g_effectiveExternalXInputPacket;
    snapshot.connected = 1;
    return true;
}

bool readSharedDInputSnapshot(SharedDInputState& snapshot)
{
    if (!g_dinputState)
        return false;

    SharedDInputState current {};
    const bool currentValid = fnvxr::shared::readSequencedSharedSnapshot(g_dinputState, current)
        && current.magic == DInputSharedMagic
        && current.version == DInputSharedVersion;
    const UInt64 nowMs = GetTickCount64();
    if (currentValid)
    {
        if (!g_haveLastStableExternalDInput || current.frame != g_lastExternalDInputSourceFrame)
        {
            g_lastExternalDInputSourceFrame = current.frame;
            g_lastExternalDInputAdvanceMs = nowMs;
        }
        g_lastStableExternalDInput = current;
        g_haveLastStableExternalDInput = true;
    }

    const UInt64 staleMs = static_cast<UInt64>((std::max)(
        50,
        getIntFromEnv("FNVXR_DINPUT_STALE_FRAME_MS", 250)));
    if (g_haveLastStableExternalDInput
        && g_lastExternalDInputAdvanceMs != 0
        && nowMs - g_lastExternalDInputAdvanceMs <= staleMs)
    {
        snapshot = g_lastStableExternalDInput;
        return true;
    }

    snapshot = {};
    snapshot.magic = DInputSharedMagic;
    snapshot.version = DInputSharedVersion;
    if (g_haveLastStableExternalDInput)
    {
        snapshot.frame = g_lastStableExternalDInput.frame;
        snapshot.mouseClickPacket = g_lastStableExternalDInput.mouseClickPacket;
        snapshot.keyboardAcceptPacket = g_lastStableExternalDInput.keyboardAcceptPacket;
        snapshot.headLookX = g_lastStableExternalDInput.headLookX;
        snapshot.headLookY = g_lastStableExternalDInput.headLookY;
        snapshot.gyroLookX = g_lastStableExternalDInput.gyroLookX;
        snapshot.gyroLookY = g_lastStableExternalDInput.gyroLookY;
    }
    return true;
}

void initSharedVrPose()
{
    if (g_vrPoseState)
        return;

    g_vrPoseMapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(SharedVrPoseState),
        fnvxr::shared::VrPoseSharedMappingName);
    if (!g_vrPoseMapping)
    {
        logTelemetry("vr pose shared CreateFileMapping failed err=%lu\n", GetLastError());
        return;
    }
    const DWORD createError = GetLastError();

    g_vrPoseState = static_cast<SharedVrPoseState*>(
        MapViewOfFile(g_vrPoseMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedVrPoseState)));
    if (!g_vrPoseState)
    {
        logTelemetry("vr pose shared MapViewOfFile failed err=%lu\n", GetLastError());
        CloseHandle(g_vrPoseMapping);
        g_vrPoseMapping = nullptr;
        return;
    }

    const bool existingValid = createError == ERROR_ALREADY_EXISTS
        && g_vrPoseState->magic == VrPoseSharedMagic
        && g_vrPoseState->version == VrPoseSharedVersion;
    if (!existingValid)
    {
        std::memset(g_vrPoseState, 0, sizeof(*g_vrPoseState));
        g_vrPoseState->magic = VrPoseSharedMagic;
        g_vrPoseState->version = VrPoseSharedVersion;
        g_vrPoseState->hmdRot[3] = 1.0f;
        g_vrPoseState->leftRot[3] = 1.0f;
        g_vrPoseState->rightRot[3] = 1.0f;
        g_vrPoseState->leftAimRot[3] = 1.0f;
        g_vrPoseState->rightAimRot[3] = 1.0f;
        g_vrPoseState->leftEyeRot[3] = 1.0f;
        g_vrPoseState->rightEyeRot[3] = 1.0f;
    }
    logTelemetry("vr pose shared ready state=%p mapping=%p existing=%d seq=%ld frame=%llu\n",
        g_vrPoseState,
        g_vrPoseMapping,
        static_cast<int>(existingValid),
        g_vrPoseState->sequence,
        static_cast<unsigned long long>(g_vrPoseState->frame));
}

void initSharedCamera()
{
    if (g_cameraState)
        return;

    g_cameraMapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(SharedCameraState),
        "Local\\FNVXR_Camera_State");
    if (!g_cameraMapping)
    {
        logTelemetry("camera shared CreateFileMapping failed err=%lu\n", GetLastError());
        return;
    }

    g_cameraState = static_cast<SharedCameraState*>(
        MapViewOfFile(g_cameraMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedCameraState)));
    if (!g_cameraState)
    {
        logTelemetry("camera shared MapViewOfFile failed err=%lu\n", GetLastError());
        CloseHandle(g_cameraMapping);
        g_cameraMapping = nullptr;
        return;
    }

    std::memset(g_cameraState, 0, sizeof(*g_cameraState));
    g_cameraState->magic = CameraSharedMagic;
    g_cameraState->version = CameraSharedVersion;
    logTelemetry("camera shared ready state=%p mapping=%p\n", g_cameraState, g_cameraMapping);
}

void initSharedRuntime()
{
    if (g_runtimeState)
        return;

    g_runtimeMapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(SharedRuntimeState),
        "Local\\FNVXR_Runtime_State");
    if (!g_runtimeMapping)
    {
        logTelemetry("runtime shared CreateFileMapping failed err=%lu\n", GetLastError());
        return;
    }

    g_runtimeState = static_cast<SharedRuntimeState*>(
        MapViewOfFile(g_runtimeMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedRuntimeState)));
    if (!g_runtimeState)
    {
        logTelemetry("runtime shared MapViewOfFile failed err=%lu\n", GetLastError());
        CloseHandle(g_runtimeMapping);
        g_runtimeMapping = nullptr;
        return;
    }

    std::memset(g_runtimeState, 0, sizeof(*g_runtimeState));
    g_runtimeState->magic = RuntimeSharedMagic;
    g_runtimeState->version = RuntimeSharedVersion;
    logTelemetry("runtime shared ready state=%p mapping=%p bytes=%zu\n",
        g_runtimeState,
        g_runtimeMapping,
        sizeof(SharedRuntimeState));
}

void initSharedPlayer()
{
    if (g_playerState)
        return;

    g_playerMapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(SharedPlayerState),
        "Local\\FNVXR_Player_State");
    if (!g_playerMapping)
    {
        logTelemetry("player shared CreateFileMapping failed err=%lu\n", GetLastError());
        return;
    }

    g_playerState = static_cast<SharedPlayerState*>(
        MapViewOfFile(g_playerMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedPlayerState)));
    if (!g_playerState)
    {
        logTelemetry("player shared MapViewOfFile failed err=%lu\n", GetLastError());
        CloseHandle(g_playerMapping);
        g_playerMapping = nullptr;
        return;
    }

    std::memset(g_playerState, 0, sizeof(*g_playerState));
    g_playerState->magic = PlayerSharedMagic;
    g_playerState->version = PlayerSharedVersion;
    logTelemetry("player shared ready state=%p mapping=%p bytes=%zu\n",
        g_playerState,
        g_playerMapping,
        sizeof(SharedPlayerState));
}

void initSharedCommand()
{
    if (g_commandState)
        return;

    g_commandMapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(SharedCommandState),
        "Local\\FNVXR_Command_State");
    if (!g_commandMapping)
    {
        logTelemetry("command shared CreateFileMapping failed err=%lu\n", GetLastError());
        return;
    }
    const DWORD createError = GetLastError();

    g_commandState = static_cast<SharedCommandState*>(
        MapViewOfFile(g_commandMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedCommandState)));
    if (!g_commandState)
    {
        logTelemetry("command shared MapViewOfFile failed err=%lu\n", GetLastError());
        CloseHandle(g_commandMapping);
        g_commandMapping = nullptr;
        return;
    }

    const bool existingValid = createError == ERROR_ALREADY_EXISTS
        && g_commandState->magic == CommandSharedMagic
        && g_commandState->version == CommandSharedVersion;
    if (!existingValid)
    {
        std::memset(g_commandState, 0, sizeof(*g_commandState));
        g_commandState->magic = CommandSharedMagic;
        g_commandState->version = CommandSharedVersion;
        g_commandState->status = fnvxr::shared::CommandStatusIdle;
    }
    g_lastCommandRequestId = g_commandState->requestId;
    logTelemetry("command shared ready state=%p mapping=%p existing=%d request=%lu status=%lu bytes=%zu\n",
        g_commandState,
        g_commandMapping,
        static_cast<int>(existingValid),
        static_cast<unsigned long>(g_commandState->requestId),
        static_cast<unsigned long>(g_commandState->status),
        sizeof(SharedCommandState));
}

void initSharedInputEvents()
{
    if (g_inputEvents)
        return;

    g_inputEventMapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(SharedInputEventQueue),
        "Local\\FNVXR_Input_Events");
    if (!g_inputEventMapping)
    {
        logTelemetry("input events CreateFileMapping failed err=%lu\n", GetLastError());
        return;
    }
    const DWORD createError = GetLastError();

    g_inputEvents = static_cast<SharedInputEventQueue*>(
        MapViewOfFile(g_inputEventMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedInputEventQueue)));
    if (!g_inputEvents)
    {
        logTelemetry("input events MapViewOfFile failed err=%lu\n", GetLastError());
        CloseHandle(g_inputEventMapping);
        g_inputEventMapping = nullptr;
        return;
    }

    const bool existingValid = createError == ERROR_ALREADY_EXISTS
        && g_inputEvents->magic == InputEventSharedMagic
        && g_inputEvents->version == InputEventSharedVersion;
    if (!existingValid)
    {
        std::memset(g_inputEvents, 0, sizeof(*g_inputEvents));
        g_inputEvents->magic = InputEventSharedMagic;
        g_inputEvents->version = InputEventSharedVersion;
    }
    logTelemetry("input events ready queue=%p mapping=%p existing=%d writeSeq=%ld bytes=%zu\n",
        g_inputEvents,
        g_inputEventMapping,
        static_cast<int>(existingValid),
        g_inputEvents->writeSequence,
        sizeof(SharedInputEventQueue));
}

bool publishInputEvent(UInt32 type, UInt32 code, std::int32_t value0 = 0, std::int32_t value1 = 0, UInt32 flags = 0, UInt64 frame = 0)
{
    if (!g_inputEvents
        || g_inputEvents->magic != InputEventSharedMagic
        || g_inputEvents->version != InputEventSharedVersion)
        return false;

    UInt32 spins = 0;
    while (InterlockedCompareExchange(&g_inputEvents->writeLock, 1, 0) != 0)
    {
        if (++spins > 1024)
        {
            InterlockedIncrement(&g_inputEvents->droppedEvents);
            return false;
        }
        Sleep(0);
    }

    const LONG sequence = g_inputEvents->writeSequence + 1;
    const UInt32 index = static_cast<UInt32>(sequence - 1) & (InputEventQueueLength - 1u);
    auto& event = g_inputEvents->events[index];
    event.sequence = 0;
    event.type = type;
    event.code = code;
    event.value0 = value0;
    event.value1 = value1;
    event.flags = flags;
    event.frame = frame;
    MemoryBarrier();
    event.sequence = static_cast<UInt32>(sequence);
    MemoryBarrier();
    g_inputEvents->writeSequence = sequence;
    InterlockedExchange(&g_inputEvents->writeLock, 0);

    static UInt32 logged = 0;
    if (logged < 64)
    {
        ++logged;
        logTelemetry(
            "inputEvent publish seq=%ld type=%lu code=%lu value=%ld,%ld flags=0x%lx frame=%llu\n",
            sequence,
            static_cast<unsigned long>(type),
            static_cast<unsigned long>(code),
            static_cast<LONG>(value0),
            static_cast<LONG>(value1),
            static_cast<unsigned long>(flags),
            static_cast<unsigned long long>(frame));
    }
    return true;
}

void publishRuntimeState(UInt64 frame, UInt32 menuBits, RuntimePhase phase, bool uiInputAllowed)
{
    if (!g_runtimeState)
        return;

    InterlockedIncrement(&g_runtimeState->sequence);
    MemoryBarrier();
    g_runtimeState->magic = RuntimeSharedMagic;
    g_runtimeState->version = RuntimeSharedVersion;
    g_runtimeState->frame = frame;
    g_runtimeState->menuBits = menuBits;
    g_runtimeState->phase = static_cast<UInt32>(phase);
    g_runtimeState->uiInputAllowed = uiInputAllowed ? 1u : 0u;
    g_runtimeState->cameraActive = (g_cameraState && g_cameraState->active) ? 1u : 0u;
    g_runtimeState->showroomActive = g_showroomActive ? 1u : 0u;
    g_runtimeState->showroomPhase = static_cast<UInt32>(g_showroomPhase);
    g_runtimeState->showroomSceneIndex = g_showroomSceneIndex;
    g_runtimeState->showroomCellFormId = g_showroomCellFormId;
    MemoryBarrier();
    InterlockedIncrement(&g_runtimeState->sequence);
}

void updateSharedVrPose(const fnvxr::PoseFrame& pose)
{
    if (!g_vrPoseState)
        return;
    if (!envEnabled("FNVXR_NVSE_WRITES_VR_POSE", false))
        return;

    if (!fnvxr::shared::beginSequencedSharedWrite(g_vrPoseState->sequence))
    {
        logTelemetry("legacy VR pose writer skipped: shared sequence busy\n");
        return;
    }
    g_vrPoseState->magic = VrPoseSharedMagic;
    g_vrPoseState->version = VrPoseSharedVersion;
    g_vrPoseState->referenceSpaceGeneration = 1;
    g_vrPoseState->producerEpoch = 1;
    g_vrPoseState->trackingFlags =
        fnvxr::shared::VrPoseTrackingHmd
        | fnvxr::shared::VrPoseTrackingLeftGripActive
        | fnvxr::shared::VrPoseTrackingRightGripActive
        | fnvxr::shared::VrPoseTrackingLeftGripCurrent
        | fnvxr::shared::VrPoseTrackingRightGripCurrent;
    g_vrPoseState->frame = pose.frame;
    g_vrPoseState->predictedDisplayTime = 0;
    g_vrPoseState->hmdRot[0] = pose.hmdRot.x;
    g_vrPoseState->hmdRot[1] = pose.hmdRot.y;
    g_vrPoseState->hmdRot[2] = pose.hmdRot.z;
    g_vrPoseState->hmdRot[3] = pose.hmdRot.w;
    g_vrPoseState->hmdPos[0] = pose.hmdPos.x;
    g_vrPoseState->hmdPos[1] = pose.hmdPos.y;
    g_vrPoseState->hmdPos[2] = pose.hmdPos.z;
    g_vrPoseState->leftRot[0] = pose.leftRot.x;
    g_vrPoseState->leftRot[1] = pose.leftRot.y;
    g_vrPoseState->leftRot[2] = pose.leftRot.z;
    g_vrPoseState->leftRot[3] = pose.leftRot.w;
    g_vrPoseState->leftPos[0] = pose.leftPos.x;
    g_vrPoseState->leftPos[1] = pose.leftPos.y;
    g_vrPoseState->leftPos[2] = pose.leftPos.z;
    g_vrPoseState->rightRot[0] = pose.rightRot.x;
    g_vrPoseState->rightRot[1] = pose.rightRot.y;
    g_vrPoseState->rightRot[2] = pose.rightRot.z;
    g_vrPoseState->rightRot[3] = pose.rightRot.w;
    g_vrPoseState->rightPos[0] = pose.rightPos.x;
    g_vrPoseState->rightPos[1] = pose.rightPos.y;
    g_vrPoseState->rightPos[2] = pose.rightPos.z;
    // Legacy PoseFrame input has only one pose per hand. Seed the aim fields
    // from that grip pose, but leave the aim tracking bits clear so retail IK
    // deliberately takes its safe grip-orientation fallback.
    std::memcpy(g_vrPoseState->leftAimRot, g_vrPoseState->leftRot, sizeof(g_vrPoseState->leftAimRot));
    std::memcpy(g_vrPoseState->leftAimPos, g_vrPoseState->leftPos, sizeof(g_vrPoseState->leftAimPos));
    std::memcpy(g_vrPoseState->rightAimRot, g_vrPoseState->rightRot, sizeof(g_vrPoseState->rightAimRot));
    std::memcpy(g_vrPoseState->rightAimPos, g_vrPoseState->rightPos, sizeof(g_vrPoseState->rightAimPos));
    g_vrPoseState->leftEyeRot[0] = 0.0f;
    g_vrPoseState->leftEyeRot[1] = 0.0f;
    g_vrPoseState->leftEyeRot[2] = 0.0f;
    g_vrPoseState->leftEyeRot[3] = 1.0f;
    g_vrPoseState->rightEyeRot[0] = 0.0f;
    g_vrPoseState->rightEyeRot[1] = 0.0f;
    g_vrPoseState->rightEyeRot[2] = 0.0f;
    g_vrPoseState->rightEyeRot[3] = 1.0f;
    fnvxr::shared::endSequencedSharedWrite(g_vrPoseState->sequence);
}

void updateSharedXInput(const fnvxr::PoseFrame& pose)
{
    if (!g_xinputState)
        return;
    if (retailSidecarProfile() && envEnabled("FNVXR_EXTERNAL_XINPUT_WRITER", true))
        return;

    constexpr float dpadDeadzone = 0.45f;
    const bool uiInputAllowed = allowUiInput();
    const bool dpadMode = uiInputAllowed && envEnabled("FNVXR_XINPUT_MENU_STICK_TO_DPAD", false);
    const bool suppressMenuAnalog =
        uiInputAllowed && envEnabled("FNVXR_XINPUT_MENU_SUPPRESS_ANALOG", true);
    float navX = 0.0f;
    float navY = 0.0f;
    if (envEqualsIgnoreCase("FNVXR_UI_NAV_STICK", "right"))
    {
        navX = pose.rightThumbstickX;
        navY = pose.rightThumbstickY;
    }
    else if (envEqualsIgnoreCase("FNVXR_UI_NAV_STICK", "both"))
    {
        navX = std::fabs(pose.rightThumbstickX) > std::fabs(pose.leftThumbstickX)
            ? pose.rightThumbstickX
            : pose.leftThumbstickX;
        navY = std::fabs(pose.rightThumbstickY) > std::fabs(pose.leftThumbstickY)
            ? pose.rightThumbstickY
            : pose.leftThumbstickY;
    }
    else if (!envEqualsIgnoreCase("FNVXR_UI_NAV_STICK", "dpad")
        && !envEqualsIgnoreCase("FNVXR_UI_NAV_STICK", "none"))
    {
        navX = pose.leftThumbstickX;
        navY = pose.leftThumbstickY;
    }
    UInt16 buttons = 0;
    if (dpadMode)
    {
        if (navY > dpadDeadzone)
            buttons |= XInputDpadUp;
        if (navY < -dpadDeadzone)
            buttons |= XInputDpadDown;
        if (navX < -dpadDeadzone)
            buttons |= XInputDpadLeft;
        if (navX > dpadDeadzone)
            buttons |= XInputDpadRight;
    }
    if (pose.buttons & fnvxr::ButtonA)
        buttons |= XInputA;
    if (pose.buttons & fnvxr::ButtonB)
        buttons |= XInputB;
    if (pose.buttons & fnvxr::ButtonX)
        buttons |= XInputX;
    if (pose.buttons & fnvxr::ButtonY)
        buttons |= XInputY;
    if (envEnabled("FNVXR_XINPUT_PHYSICAL_MENU_BUTTONS_ENABLE", false))
    {
        if (pose.buttons & fnvxr::LeftMenu)
            buttons |= XInputBack;
        if (pose.buttons & fnvxr::RightMenu)
            buttons |= XInputStart;
    }
    if (pose.buttons & fnvxr::LeftThumbstick)
        buttons |= XInputLeftThumb;
    if (pose.buttons & fnvxr::RightThumbstick)
        buttons |= XInputRightThumb;

    static UInt16 lastLoggedButtons = 0xffff;
    static bool lastLoggedDpadMode = false;
    static bool lastLoggedSuppressMenuAnalog = false;
    if (buttons != lastLoggedButtons
        || dpadMode != lastLoggedDpadMode
        || suppressMenuAnalog != lastLoggedSuppressMenuAnalog
        || (pose.frame % 240) == 0)
    {
        lastLoggedButtons = buttons;
        lastLoggedDpadMode = dpadMode;
        lastLoggedSuppressMenuAnalog = suppressMenuAnalog;
        logTelemetry(
            "{\"event\":\"fnvxrVirtualXboxState\",\"frame\":%llu,\"buttons\":%u,\"dpadMode\":%s,\"ui\":%s,\"leftGrip\":%.3f,\"navStick\":[%.3f,%.3f],\"navSource\":\"%s\",\"analogSuppressed\":%s}\n",
            static_cast<unsigned long long>(pose.frame),
            static_cast<unsigned int>(buttons),
            dpadMode ? "true" : "false",
            uiInputAllowed ? "true" : "false",
            pose.leftGrip,
            navX,
            navY,
            envEqualsIgnoreCase("FNVXR_UI_NAV_STICK", "right") ? "right" :
                (envEqualsIgnoreCase("FNVXR_UI_NAV_STICK", "both") ? "both" :
                    (envEqualsIgnoreCase("FNVXR_UI_NAV_STICK", "dpad") || envEqualsIgnoreCase("FNVXR_UI_NAV_STICK", "none") ? "dpad" : "left")),
            suppressMenuAnalog ? "true" : "false");
    }

    if (!fnvxr::shared::beginSequencedSharedWrite(g_xinputState->sequence))
        return;
    g_xinputState->magic = XInputSharedMagic;
    g_xinputState->version = XInputSharedVersion;
    g_xinputState->connected = 1;
    g_xinputState->buttons = buttons;
    g_xinputState->leftTrigger = triggerValue(pose.leftTrigger);
    g_xinputState->rightTrigger = triggerValue(pose.rightTrigger);
    g_xinputState->leftThumbX = (dpadMode || suppressMenuAnalog) ? 0 : thumbValue(pose.leftThumbstickX);
    g_xinputState->leftThumbY = (dpadMode || suppressMenuAnalog) ? 0 : thumbValue(pose.leftThumbstickY);
    g_xinputState->rightThumbX = (dpadMode || suppressMenuAnalog) ? 0 : thumbValue(pose.rightThumbstickX);
    g_xinputState->rightThumbY = (dpadMode || suppressMenuAnalog) ? 0 : thumbValue(pose.rightThumbstickY);
    g_xinputState->packet++;
    fnvxr::shared::endSequencedSharedWrite(g_xinputState->sequence);
}

void updateSharedDInput(const fnvxr::PoseFrame& pose)
{
    if (!g_dinputState)
        return;
    if (retailSidecarProfile() && envEnabled("FNVXR_EXTERNAL_DINPUT_WRITER", true))
        return;

    const bool uiInputAllowed = allowUiInput();
    const bool gameplayControlsActive = !uiInputAllowed;
    const float aimThreshold = std::clamp(getFloatFromEnv("FNVXR_HEADSPACE_LOOK_AIM_TRIGGER", 0.35f), 0.0f, 1.0f);
    UInt32 gameplayFlags = 0;
    if (gameplayControlsActive && pose.leftTrigger >= aimThreshold)
        gameplayFlags |= fnvxr::shared::DInputGameplayFlagAimHeld;
    if (gameplayControlsActive && playerCombatWeaponReady())
        gameplayFlags |= fnvxr::shared::DInputGameplayFlagWeaponOut;
    if (gameplayControlsActive && currentWeaponClassMeleeOrUnarmed())
        gameplayFlags |= fnvxr::shared::DInputGameplayFlagMeleeOrUnarmed;
    if (gameplayControlsActive && (pose.buttons & fnvxr::LeftThumbstick) != 0)
        gameplayFlags |= fnvxr::shared::DInputGameplayFlagThirdPersonZoomHeld;
    if (!fnvxr::shared::beginSequencedSharedWrite(g_dinputState->sequence))
        return;
    g_dinputState->magic = DInputSharedMagic;
    g_dinputState->version = DInputSharedVersion;
    g_dinputState->frame = static_cast<UInt32>(pose.frame);
    g_dinputState->pointerActive = g_hasMenuPointer ? 1u : 0u;
    g_dinputState->menuInputActive = uiInputAllowed ? 1u : 0u;
    g_dinputState->gameplayControlsActive = uiInputAllowed ? 0u : 1u;
    g_dinputState->leftStickX = sharedStickValue(pose.leftThumbstickX);
    g_dinputState->leftStickY = sharedStickValue(pose.leftThumbstickY);
    g_dinputState->rightStickX = sharedStickValue(pose.rightThumbstickX);
    g_dinputState->rightStickY = sharedStickValue(pose.rightThumbstickY);
    g_dinputState->headLookActive = 0u;
    g_dinputState->headLookX = 0;
    g_dinputState->headLookY = 0;
    g_dinputState->gyroLookActive = 0u;
    g_dinputState->gyroLookX = 0;
    g_dinputState->gyroLookY = 0;
    g_dinputState->leftGrip = sharedStickValue(std::clamp(pose.leftGrip, 0.0f, 1.0f));
    g_dinputState->rightGrip = sharedStickValue(std::clamp(pose.rightGrip, 0.0f, 1.0f));
    g_dinputState->aimTrigger = triggerValue(pose.leftTrigger);
    g_dinputState->gameplayFlags = gameplayFlags;
    g_dinputState->clientX = g_lastMenuPointerClient.x;
    g_dinputState->clientY = g_lastMenuPointerClient.y;
    fnvxr::shared::endSequencedSharedWrite(g_dinputState->sequence);
}

void publishDInputMouseClick()
{
    if (!g_dinputState)
        return;
    if (retailSidecarProfile() && envEnabled("FNVXR_EXTERNAL_DINPUT_WRITER", true))
        return;

    if (!fnvxr::shared::beginSequencedSharedWrite(g_dinputState->sequence))
        return;
    g_dinputState->mouseClickPacket++;
    g_lastPublishedDInputMouseClickPacket = g_dinputState->mouseClickPacket;
    g_lastConsumedDInputMouseClickPacket = g_dinputState->mouseClickPacket;
    fnvxr::shared::endSequencedSharedWrite(g_dinputState->sequence);
}

BufferedKeyTapFn resolveBufferedKeyTap()
{
    if (g_triedResolveBufferedKeyTap)
        return g_bufferedKeyTap;

    g_triedResolveBufferedKeyTap = true;

    HMODULE nvseModule = GetModuleHandleA("nvse_1_4.dll");
    if (!nvseModule)
    {
        logTelemetry("bufferedTap resolve failed: nvse module missing\n");
        return nullptr;
    }

    char modulePath[MAX_PATH] {};
    GetModuleFileNameA(nvseModule, modulePath, sizeof(modulePath));

    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    if (!SymInitialize(process, nullptr, FALSE) && GetLastError() != ERROR_INVALID_PARAMETER)
    {
        logTelemetry("bufferedTap SymInitialize failed err=%lu\n", GetLastError());
        return nullptr;
    }

    DWORD64 moduleBase = SymLoadModuleEx(
        process,
        nullptr,
        modulePath,
        nullptr,
        reinterpret_cast<DWORD64>(nvseModule),
        0,
        nullptr,
        0);
    if (moduleBase == 0)
        moduleBase = reinterpret_cast<DWORD64>(nvseModule);

    SYMBOL_INFO_PACKAGE symbol {};
    symbol.si.SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol.si.MaxNameLen = MAX_SYM_NAME;
    const char* names[] = {
        "?BufferedKeyTap@DIHookControl@@QAEXI@Z",
        "DIHookControl::BufferedKeyTap"
    };
    for (const char* name : names)
    {
        if (SymFromName(process, name, &symbol.si))
        {
            g_bufferedKeyTap = reinterpret_cast<BufferedKeyTapFn>(symbol.si.Address);
            logTelemetry("bufferedTap resolved name=%s addr=%p module=%p\n", name, g_bufferedKeyTap, nvseModule);
            return g_bufferedKeyTap;
        }
    }

    if (!g_loggedBufferedSymbolEnum)
    {
        g_loggedBufferedSymbolEnum = true;
        auto enumCallback = [](PSYMBOL_INFO info, ULONG, PVOID) -> BOOL
        {
            if (info && info->Name)
                logTelemetry("bufferedTap candidate name=%s addr=%p\n", info->Name, reinterpret_cast<void*>(info->Address));
            return TRUE;
        };
        SymEnumSymbols(process, moduleBase, "*BufferedKey*", enumCallback, nullptr);
        SymEnumSymbols(process, moduleBase, "*DIHookControl*", enumCallback, nullptr);
    }

    logTelemetry("bufferedTap resolve failed err=%lu module=%p path='%s'\n", GetLastError(), nvseModule, modulePath);
    return nullptr;
}

bool tapDirectInputKey(UInt32 keycode)
{
    if (keycode >= MaxDirectInputMacros)
        return false;

    bool published = false;
    if (keycode >= MouseButtonOffset && keycode < MouseButtonOffset + 8)
    {
        published = publishInputEvent(
            fnvxr::shared::InputEventTypeMouseButtonTap,
            keycode - MouseButtonOffset);
    }
    else
    {
        published = publishInputEvent(fnvxr::shared::InputEventTypeKeyTap, keycode);
    }

    // The shared input queue and the in-process DI hook are alternative
    // delivery lanes. Publishing to both turns one buffered tap into two.
    if (published)
        return true;

    // Return has a dedicated atomic fallback mailbox consumed by the retail
    // DirectInput proxy. Once that lane is selected, do not also tap the
    // in-process hook or the same accept action can arrive twice.
    if (g_dinputState && keycode == DIK_RETURN)
    {
        InterlockedIncrement(reinterpret_cast<volatile LONG*>(&g_dinputState->keyboardAcceptPacket));
        return true;
    }
    if (!g_directInputHook)
        return false;

    g_directInputHook->keys[keycode].tap = true;
    if (envEnabled("FNVXR_BUFFERED_DIRECTINPUT", true))
    {
        DirectInputDeviceObjectData data {};
        data.dwOfs = keycode;
        data.dwData = 0x80;
        data.dwTimeStamp = GetTickCount();
        data.dwSequence = 0;
        data.uAppData = static_cast<ULONG_PTR>(-1);
        g_directInputHook->bufferedPresses.push(data);
        data.dwData = 0x00;
        g_directInputHook->bufferedPresses.push(data);
        logTelemetry("bufferedTap key=%u queue=%zu\n", keycode, g_directInputHook->bufferedPresses.size());
    }
    if (envEnabled("FNVXR_BUFFERED_DIRECTINPUT_CALL", false))
    {
        if (BufferedKeyTapFn bufferedKeyTap = resolveBufferedKeyTap())
        {
            bufferedKeyTap(g_directInputHook, keycode);
            logTelemetry("bufferedTap key=%u\n", keycode);
        }
    }
    return true;
}

bool isMenuMode()
{
    __try
    {
        using IsMenuModeFn = bool (__cdecl*)();
        return pointerFromAddress32<IsMenuModeFn>(IsMenuModeAddress)();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        logTelemetry("menuMode exception\n");
        return true;
    }
}

bool isMenuVisible(UInt32 menuType)
{
    __try
    {
        auto* visibility = pointerFromAddress32<UInt8*>(MenuVisibilityArrayAddress);
        return visibility[menuType] != 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        logTelemetry("menuVisible exception type=0x%x\n", menuType);
        return false;
    }
}

void* tileMenuByType(UInt32 menuType);
void* menuFromTileMenu(void* tileMenu);

bool validatedVisibleMenu(
    UInt32 menuType,
    void* expectedMenu = nullptr,
    void** outMenu = nullptr,
    void** outTileMenu = nullptr)
{
    if (menuType < kMenuTypeMin || menuType > kMenuTypeMax || !isMenuVisible(menuType))
        return false;

    __try
    {
        void* tileMenu = tileMenuByType(menuType);
        void* menu = menuFromTileMenu(tileMenu);
        if (!tileMenu || !menu || (expectedMenu && menu != expectedMenu))
            return false;
        if (readUInt32(reinterpret_cast<std::uintptr_t>(menu) + 0x20) != menuType)
            return false;
        if (readPointer(reinterpret_cast<std::uintptr_t>(menu) + 0x04) != tileMenu)
            return false;
        if (menuFromTileMenu(tileMenu) != menu)
            return false;
        if (outMenu)
            *outMenu = menu;
        if (outTileMenu)
            *outTileMenu = tileMenu;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool menuVisibleWithTile(UInt32 menuType)
{
    return validatedVisibleMenu(menuType);
}

bool isPipboyVisible()
{
    return isMenuVisible(kMenuTypeInventory)
        || menuVisibleWithTile(kMenuTypeStats)
        || menuVisibleWithTile(kMenuTypeMap);
}

UInt32 visibleGenericBlockingMenuType()
{
    for (UInt32 menuType = kMenuTypeMin; menuType <= kMenuTypeMax; ++menuType)
    {
        switch (menuType)
        {
            case kMenuTypeInventory:
            case kMenuTypeStats:
            case kMenuTypeHUDMain:
            case kMenuTypeLoading:
            case kMenuTypeDialog:
            case kMenuTypeStart:
            case kMenuTypeMap:
            case kMenuTypeRaceSex:
            case kMenuTypeVats:
                continue;
            default:
                break;
        }
        if (menuVisibleWithTile(menuType))
            return menuType;
    }
    return 0;
}

UInt32 activeInterfaceMenuType()
{
    void* interfaceManager = readPointer(InterfaceManagerAddress);
    void* activeMenu = interfaceManager
        ? readPointer(reinterpret_cast<std::uintptr_t>(interfaceManager) + 0x0D0)
        : nullptr;
    if (!activeMenu)
        return 0;

    const UInt32 menuType = readUInt32(reinterpret_cast<std::uintptr_t>(activeMenu) + 0x20);
    if (!validatedVisibleMenu(menuType, activeMenu))
    {
        static UInt32 invalidLogCount = 0;
        if (invalidLogCount++ < 24)
        {
            logTelemetry(
                "activeMenu rejected ptr=%p type=0x%03lx reason=identity-visibility-lifecycle\n",
                activeMenu,
                static_cast<unsigned long>(menuType));
        }
        return 0;
    }
    // The HUD may remain the active interface object during ordinary gameplay.
    return menuType == kMenuTypeHUDMain ? 0u : menuType;
}

bool explicitlyClassifiedMenuType(UInt32 menuType)
{
    switch (menuType)
    {
        case kMenuTypeInventory:
        case kMenuTypeStats:
        case kMenuTypeLoading:
        case kMenuTypeDialog:
        case kMenuTypeStart:
        case kMenuTypeMap:
        case kMenuTypeRaceSex:
        case kMenuTypeVats:
            return true;
        default:
            return false;
    }
}

bool allowUiInput()
{
    return fnvxr::shared::runtimeUiInputAllowed(currentMenuBits());
}

UInt32 currentMenuBits()
{
    constexpr UInt32 menuModeBit = 1u << 0;
    constexpr UInt32 startBit = 1u << 1;
    constexpr UInt32 raceSexBit = 1u << 2;
    constexpr UInt32 dialogBit = 1u << 3;
    constexpr UInt32 vatsBit = 1u << 4;
    constexpr UInt32 loadingBit = 1u << 5;
    const bool menuMode = isMenuMode();
    const UInt32 activeMenuType = activeInterfaceMenuType();
    const bool startVisible = menuVisibleWithTile(kMenuTypeStart) || activeMenuType == kMenuTypeStart;
    const bool raceSexVisible = menuVisibleWithTile(kMenuTypeRaceSex) || activeMenuType == kMenuTypeRaceSex;
    // The visibility byte is authoritative during the dialogue camera
    // transition; the TileMenu pointer may lag it by a frame.
    const bool dialogVisible = isMenuVisible(kMenuTypeDialog) || activeMenuType == kMenuTypeDialog;
    const bool vatsVisible = isMenuVisible(kMenuTypeVats) || activeMenuType == kMenuTypeVats;
    const bool rawLoadingVisible = menuVisibleWithTile(kMenuTypeLoading) || activeMenuType == kMenuTypeLoading;
    const bool pipboyVisible = isPipboyVisible()
        || activeMenuType == kMenuTypeInventory
        || activeMenuType == kMenuTypeStats
        || activeMenuType == kMenuTypeMap;
    const UInt32 visibleGenericMenuType = visibleGenericBlockingMenuType();
    const UInt32 genericMenuType = visibleGenericMenuType != 0
        ? visibleGenericMenuType
        : (activeMenuType != 0 && !explicitlyClassifiedMenuType(activeMenuType) ? activeMenuType : 0u);
    // StartMenu and other actionable UIs can coexist with a retained
    // LoadingMenu visibility byte/TileMenu. Letting that stale loading object
    // win classifies the whole front end as non-interactive and drops every
    // controller action. Loading blocks input only when it is the sole
    // validated UI lifecycle state.
    const bool actionableMenuVisible = startVisible
        || raceSexVisible
        || dialogVisible
        || vatsVisible
        || pipboyVisible
        || genericMenuType != 0;
    const bool loadingVisible = fnvxr::shared::runtimeLoadingMenuBlocksInput(
        rawLoadingVisible,
        actionableMenuVisible);
    static bool lastRawLoadingVisible = false;
    static bool lastLoadingVisible = false;
    if (rawLoadingVisible != lastRawLoadingVisible || loadingVisible != lastLoadingVisible)
    {
        logTelemetry(
            "{\"event\":\"fnvxrMenuLoadingPrecedence\",\"rawLoading\":%s,\"loadingBlocksInput\":%s,\"actionableMenu\":%s,\"start\":%s,\"activeType\":%lu}\n",
            rawLoadingVisible ? "true" : "false",
            loadingVisible ? "true" : "false",
            actionableMenuVisible ? "true" : "false",
            startVisible ? "true" : "false",
            static_cast<unsigned long>(activeMenuType));
        lastRawLoadingVisible = rawLoadingVisible;
        lastLoadingVisible = loadingVisible;
    }
    static UInt32 lastGenericMenuType = 0xffffffffu;
    static UInt32 lastActiveMenuType = 0xffffffffu;
    static UInt64 menuLifecycleGeneration = 0;
    if (genericMenuType != lastGenericMenuType || activeMenuType != lastActiveMenuType)
    {
        ++menuLifecycleGeneration;
        lastGenericMenuType = genericMenuType;
        lastActiveMenuType = activeMenuType;
        logTelemetry(
            "genericMenu generation=%llu type=0x%03lx activeType=0x%03lx visible=%d menuMode=%d validated=1\n",
            static_cast<unsigned long long>(menuLifecycleGeneration),
            static_cast<unsigned long>(genericMenuType),
            static_cast<unsigned long>(activeMenuType),
            genericMenuType != 0 ? 1 : 0,
            menuMode ? 1 : 0);
    }
    return (menuMode ? menuModeBit : 0)
        | (startVisible ? startBit : 0)
        | (raceSexVisible ? raceSexBit : 0)
        | (dialogVisible ? dialogBit : 0)
        | (vatsVisible ? vatsBit : 0)
        | (loadingVisible ? loadingBit : 0)
        | (pipboyVisible ? fnvxr::shared::RuntimePipBoyMenuBit : 0)
        | (genericMenuType != 0 ? fnvxr::shared::RuntimeGenericMenuBit : 0);
}

RuntimePhase runtimePhaseFromMenuBits(UInt32 menuBits)
{
    if ((menuBits & fnvxr::shared::RuntimeLoadingMenuBit) != 0)
        return RuntimePhase::Loading;
    if ((menuBits & fnvxr::shared::RuntimeBlockingMenuBits) != 0)
        return RuntimePhase::Menu;
    return RuntimePhase::Gameplay;
}

bool uiInputAllowedFromMenuBits(UInt32 menuBits)
{
    return fnvxr::shared::runtimeUiInputAllowed(menuBits);
}

bool pipBoyVisibleFromMenuBits(UInt32 menuBits)
{
    return (menuBits & fnvxr::shared::RuntimePipBoyMenuBit) != 0 || isPipboyVisible();
}

bool pipBoyRightStickNav(UInt32 menuBits)
{
    return pipBoyVisibleFromMenuBits(menuBits)
        && envEnabled("FNVXR_PIPBOY_RIGHT_STICK_NAV", true);
}

bool pipBoySplitStickNav(UInt32 menuBits)
{
    return pipBoyVisibleFromMenuBits(menuBits)
        && envEnabled("FNVXR_PIPBOY_SPLIT_STICK_NAV", true);
}

bool pipBoyPointerOnly(UInt32 menuBits)
{
    return pipBoyVisibleFromMenuBits(menuBits)
        && envEnabled("FNVXR_PIPBOY_POINTER_ONLY", true);
}

const char* uiNavStickSourceName(UInt32 menuBits)
{
    if (pipBoyPointerOnly(menuBits))
        return "pipboy-pointer";
    if (pipBoySplitStickNav(menuBits))
        return "pipboy-split";
    if (pipBoyRightStickNav(menuBits))
        return "pipboy-right";
    if (envEqualsIgnoreCase("FNVXR_UI_NAV_STICK", "right"))
        return "right";
    if (envEqualsIgnoreCase("FNVXR_UI_NAV_STICK", "both"))
        return "both";
    if (envEqualsIgnoreCase("FNVXR_UI_NAV_STICK", "dpad")
        || envEqualsIgnoreCase("FNVXR_UI_NAV_STICK", "none"))
    {
        return "dpad";
    }
    return "left";
}

void selectUiNavAxes(UInt32 menuBits, const SharedXInputState& state, std::int16_t& navX, std::int16_t& navY)
{
    navX = 0;
    navY = 0;
    if (pipBoyRightStickNav(menuBits) || envEqualsIgnoreCase("FNVXR_UI_NAV_STICK", "right"))
    {
        navX = state.rightThumbX;
        navY = state.rightThumbY;
    }
    else if (envEqualsIgnoreCase("FNVXR_UI_NAV_STICK", "both"))
    {
        navX = std::abs(static_cast<int>(state.rightThumbX)) > std::abs(static_cast<int>(state.leftThumbX))
            ? state.rightThumbX
            : state.leftThumbX;
        navY = std::abs(static_cast<int>(state.rightThumbY)) > std::abs(static_cast<int>(state.leftThumbY))
            ? state.rightThumbY
            : state.leftThumbY;
    }
    else if (!envEqualsIgnoreCase("FNVXR_UI_NAV_STICK", "dpad")
        && !envEqualsIgnoreCase("FNVXR_UI_NAV_STICK", "none"))
    {
        navX = state.leftThumbX;
        navY = state.leftThumbY;
    }
}

void selectPoseUiNavAxes(UInt32 menuBits, const fnvxr::PoseFrame& pose, float& navX, float& navY)
{
    navX = 0.0f;
    navY = 0.0f;
    if (pipBoyRightStickNav(menuBits) || envEqualsIgnoreCase("FNVXR_UI_NAV_STICK", "right"))
    {
        navX = pose.rightThumbstickX;
        navY = pose.rightThumbstickY;
    }
    else if (envEqualsIgnoreCase("FNVXR_UI_NAV_STICK", "both"))
    {
        navX = std::fabs(pose.rightThumbstickX) > std::fabs(pose.leftThumbstickX)
            ? pose.rightThumbstickX
            : pose.leftThumbstickX;
        navY = std::fabs(pose.rightThumbstickY) > std::fabs(pose.leftThumbstickY)
            ? pose.rightThumbstickY
            : pose.leftThumbstickY;
    }
    else if (!envEqualsIgnoreCase("FNVXR_UI_NAV_STICK", "dpad")
        && !envEqualsIgnoreCase("FNVXR_UI_NAV_STICK", "none"))
    {
        navX = pose.leftThumbstickX;
        navY = pose.leftThumbstickY;
    }
}

UInt32 poseUiNavMask(UInt32 menuBits, const fnvxr::PoseFrame& pose, float deadzone, float& navX, float& navY)
{
    if (pipBoySplitStickNav(menuBits))
    {
        navX = pose.leftThumbstickX;
        navY = pose.rightThumbstickY;
        UInt32 mask = 0;
        if (pose.rightThumbstickY > deadzone)
            mask |= 1u;
        if (pose.rightThumbstickY < -deadzone)
            mask |= 2u;
        if (pose.leftThumbstickX < -deadzone)
            mask |= 4u;
        if (pose.leftThumbstickX > deadzone)
            mask |= 8u;
        return mask;
    }

    selectPoseUiNavAxes(menuBits, pose, navX, navY);
    UInt32 mask = 0;
    if (navY > deadzone)
        mask |= 1u;
    if (navY < -deadzone)
        mask |= 2u;
    if (navX < -deadzone)
        mask |= 4u;
    if (navX > deadzone)
        mask |= 8u;
    return mask;
}

UInt32 uiBackKeyForMenu(UInt32 menuBits)
{
    constexpr UInt32 menuModeBit = 1u << 0;
    constexpr UInt32 startBit = 1u << 1;
    constexpr UInt32 raceSexBit = 1u << 2;
    constexpr UInt32 dialogBit = 1u << 3;
    constexpr UInt32 vatsBit = 1u << 4;
    constexpr UInt32 loadingBit = 1u << 5;

    if (pipBoyVisibleFromMenuBits(menuBits)
        && envEnabled("FNVXR_PIPBOY_B_USES_TAB", true))
    {
        return DIK_TAB;
    }

    const bool genericPickerBack =
        (menuBits & (menuModeBit | raceSexBit)) != 0
        && (menuBits & (startBit | dialogBit | vatsBit | loadingBit)) == 0
        && envEnabled("FNVXR_UI_GENERIC_BACK_USES_TAB", true);
    return genericPickerBack ? DIK_TAB : DIK_ESCAPE;
}

WPARAM uiBackVirtualKeyForMenu(UInt32 menuBits)
{
    return uiBackKeyForMenu(menuBits) == DIK_TAB ? VK_TAB : VK_ESCAPE;
}

UInt32 uiSortKey()
{
    return DIK_X;
}

bool assignUiFavoriteSlot(const char* source, UInt64 frame, bool utilitySlot);
void tickUiFavoriteAssignment(UInt64 frame, UInt32 menuBits);
void releaseUiFavoriteAssignment(const char* source, UInt64 frame);
bool externalDInputSharedReady();

bool cameraAllowedForMenuBits(UInt32 menuBits)
{
    constexpr UInt32 raceSexBit = 1u << 2;
    constexpr UInt32 dialogBit = 1u << 3;
    constexpr UInt32 vatsBit = 1u << 4;
    constexpr UInt32 loadingBit = 1u << 5;

    if (g_showroomActive && envEnabled("FNVXR_SHOWROOM_CAMERA_DURING_UI", true))
        return (menuBits & (raceSexBit | dialogBit | vatsBit | loadingBit)) == 0;

    return (menuBits & fnvxr::shared::RuntimeBlockingMenuBits) == 0;
}

bool directUiClickEnabled()
{
    return envEnabled("FNVXR_DIRECT_UI_CLICK", true);
}

bool pointerTileFallbackEnabled()
{
    return envEnabled("FNVXR_POINTER_TILE_FALLBACK", false);
}

bool inCameraGameplay()
{
    if (g_showroomActive && envEnabled("FNVXR_SHOWROOM_CAMERA_DURING_UI", true))
    {
        return !menuVisibleWithTile(kMenuTypeLoading)
            && !menuVisibleWithTile(kMenuTypeRaceSex)
            && !menuVisibleWithTile(kMenuTypeDialog)
            && !isMenuVisible(kMenuTypeVats);
    }

    return visibleGenericBlockingMenuType() == 0
        && !menuVisibleWithTile(kMenuTypeStart)
        && !menuVisibleWithTile(kMenuTypeRaceSex)
        && !menuVisibleWithTile(kMenuTypeLoading)
        && !menuVisibleWithTile(kMenuTypeDialog)
        && !isMenuVisible(kMenuTypeVats)
        && !isPipboyVisible();
}

bool looksLikeNiObject(void* object)
{
    const auto address = reinterpret_cast<std::uintptr_t>(object);
    if (address < 0x01000000)
        return false;

    void* vtable = readPointer(address);
    const auto vtableAddress = reinterpret_cast<std::uintptr_t>(vtable);
    return vtableAddress >= 0x00400000 && vtableAddress < 0x02000000;
}

bool looksLikeGameForm(void* object)
{
    const auto address = reinterpret_cast<std::uintptr_t>(object);
    if (address < 0x01000000)
        return false;

    void* vtable = readPointer(address);
    const auto vtableAddress = reinterpret_cast<std::uintptr_t>(vtable);
    return vtableAddress >= 0x00400000 && vtableAddress < 0x02000000;
}

const char* formTypeName(UInt8 type)
{
    switch (type)
    {
        case kFormTypeTESNPC: return "TESNPC";
        case kFormTypeTESCreature: return "TESCreature";
        case kFormTypeCharacter: return "Character";
        case kFormTypeCreature: return "Creature";
        default: return "Other";
    }
}

struct FriendlyMobTarget
{
    void* ref = nullptr;
    void* base = nullptr;
    UInt32 refId = 0;
    UInt32 baseId = 0;
    UInt8 refType = 0;
    UInt8 baseType = 0;
    bool actorish = false;
    bool creatureish = false;
    bool knownFriendly = false;
    bool safe = false;
    const char* reason = "unset";
};

bool knownFriendlyPetForm(UInt32 formId)
{
    return formId == kCheyenneRefId || formId == kCheyenneBaseId;
}

bool readFriendlyMobTarget(FriendlyMobTarget& target)
{
    target = {};
    void* interfaceManager = readPointer(InterfaceManagerAddress);
    if (!interfaceManager)
    {
        target.reason = "interface-null";
        return false;
    }

    target.ref = readPointer(reinterpret_cast<std::uintptr_t>(interfaceManager) + InterfaceManagerCrosshairRefOffset);
    if (!looksLikeGameForm(target.ref))
    {
        target.reason = target.ref ? "crosshair-not-form" : "crosshair-null";
        return false;
    }

    const auto refAddress = reinterpret_cast<std::uintptr_t>(target.ref);
    target.refId = readUInt32(refAddress + TESFormRefIdOffset);
    target.refType = readUInt8(refAddress + TESFormTypeIdOffset);
    target.base = readPointer(refAddress + TESObjectRefrBaseFormOffset);
    if (looksLikeGameForm(target.base))
    {
        const auto baseAddress = reinterpret_cast<std::uintptr_t>(target.base);
        target.baseId = readUInt32(baseAddress + TESFormRefIdOffset);
        target.baseType = readUInt8(baseAddress + TESFormTypeIdOffset);
    }

    target.creatureish =
        target.refType == kFormTypeCreature
        || target.baseType == kFormTypeTESCreature;
    target.actorish =
        target.creatureish
        || target.refType == kFormTypeCharacter
        || target.baseType == kFormTypeTESNPC;
    target.knownFriendly =
        knownFriendlyPetForm(target.refId)
        || knownFriendlyPetForm(target.baseId);
    target.safe = target.actorish && (target.creatureish || target.knownFriendly);
    target.reason = target.safe ? "safe-crosshair-mob" : (target.actorish ? "actor-not-pet-target" : "not-actor");
    return target.safe;
}

bool tryPetFriendlyMobActivation(UInt64 frame)
{
    if (!envEnabled("FNVXR_FRIENDLY_MOB_PET_ENABLE", true))
        return false;

    FriendlyMobTarget target {};
    const bool safe = readFriendlyMobTarget(target);
    const bool consume = safe && envEnabled("FNVXR_FRIENDLY_MOB_PET_CONSUME", true);
    const bool logMisses = envEnabled("FNVXR_FRIENDLY_MOB_PET_LOG_MISSES", false);
    if (safe || logMisses)
    {
        logTelemetry(
            "{\"event\":\"fnvxrFriendlyMobPet\",\"frame\":%llu,\"safe\":%s,\"consumed\":%s,\"reason\":\"%s\",\"ref\":\"%p\",\"base\":\"%p\",\"refId\":\"0x%08lx\",\"baseId\":\"0x%08lx\",\"refType\":\"%s\",\"baseType\":\"%s\",\"creature\":%s,\"knownFriendly\":%s}\n",
            static_cast<unsigned long long>(frame),
            safe ? "true" : "false",
            consume ? "true" : "false",
            target.reason ? target.reason : "unset",
            target.ref,
            target.base,
            static_cast<unsigned long>(target.refId),
            static_cast<unsigned long>(target.baseId),
            formTypeName(target.refType),
            formTypeName(target.baseType),
            target.creatureish ? "true" : "false",
            target.knownFriendly ? "true" : "false");
    }
    return consume;
}

UInt32 currentPlayerCellFormId(void* player, void** cellOut)
{
    if (cellOut)
        *cellOut = nullptr;
    if (!player)
        return 0;

    void* parentCell = readPointer(reinterpret_cast<std::uintptr_t>(player) + TESObjectRefrParentCellOffset);
    if (!looksLikeGameForm(parentCell))
        return 0;

    const UInt32 cellFormId = readUInt32(reinterpret_cast<std::uintptr_t>(parentCell) + TESFormRefIdOffset);
    if (cellOut)
        *cellOut = parentCell;
    return cellFormId;
}

void readNiAvObjectWorldTransform(void* object, float rot[9], float pos[3])
{
    const auto base = reinterpret_cast<std::uintptr_t>(object);
    rot[0] = readFloat(base + NiAvObjectWorldRotationOffset + 0x00);
    rot[1] = readFloat(base + NiAvObjectWorldRotationOffset + 0x04);
    rot[2] = readFloat(base + NiAvObjectWorldRotationOffset + 0x08);
    rot[3] = readFloat(base + NiAvObjectWorldRotationOffset + 0x0C);
    rot[4] = readFloat(base + NiAvObjectWorldRotationOffset + 0x10);
    rot[5] = readFloat(base + NiAvObjectWorldRotationOffset + 0x14);
    rot[6] = readFloat(base + NiAvObjectWorldRotationOffset + 0x18);
    rot[7] = readFloat(base + NiAvObjectWorldRotationOffset + 0x1C);
    rot[8] = readFloat(base + NiAvObjectWorldRotationOffset + 0x20);
    pos[0] = readFloat(base + NiAvObjectWorldTranslationOffset + 0x0);
    pos[1] = readFloat(base + NiAvObjectWorldTranslationOffset + 0x4);
    pos[2] = readFloat(base + NiAvObjectWorldTranslationOffset + 0x8);
}

UInt32 sharedPointerAddress(void* pointer)
{
    return static_cast<UInt32>(reinterpret_cast<std::uintptr_t>(pointer) & 0xffffffffu);
}

bool playerThirdPersonActive()
{
    void* player = readPointer(PlayerCharacterAddress);
    return player
        && readUInt8(reinterpret_cast<std::uintptr_t>(player) + PlayerCharacterIsThirdPersonOffset) != 0;
}

bool strictFirstPersonEnabled()
{
    return envEnabled("FNVXR_FORCE_FIRST_PERSON", true);
}

bool forceFirstPersonCameraMode(const char* source, UInt64 frame)
{
    if (!strictFirstPersonEnabled())
        return true;

    void* player = readPointer(PlayerCharacterAddress);
    if (!player)
        return false;
    if (!playerThirdPersonActive())
        return true;

    static UInt64 lastAttemptMs = 0;
    const UInt64 nowMs = GetTickCount64();
    const UInt64 retryMs = static_cast<UInt64>(
        (std::max)(50, getIntFromEnv("FNVXR_FORCE_FIRST_PERSON_RETRY_MS", 250)));
    if (lastAttemptMs != 0 && nowMs < lastAttemptMs + retryMs)
        return false;
    lastAttemptMs = nowMs;

    bool nativeResult = false;
    bool callCompleted = false;
    __try
    {
        using ToggleFirstPersonFn = bool (__thiscall*)(void*, bool);
        auto toggleFirstPerson = pointerFromAddress32<ToggleFirstPersonFn>(
            PlayerCharacterToggleFirstPersonAddress);
        nativeResult = toggleFirstPerson(player, true);
        callCompleted = true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        callCompleted = false;
    }

    const bool stillThirdPerson = playerThirdPersonActive();
    logTelemetry(
        "strictFirstPerson frame=%llu source=%s callCompleted=%d nativeResult=%d thirdAfter=%d player=%p\n",
        static_cast<unsigned long long>(frame),
        source ? source : "unknown",
        static_cast<int>(callCompleted),
        static_cast<int>(nativeResult),
        static_cast<int>(stillThirdPerson),
        player);
    return callCompleted && !stillThirdPerson;
}

bool playerWeaponOut()
{
    void* player = readPointer(PlayerCharacterAddress);
    if (!player)
        return false;

    void* baseProcess = readPointer(reinterpret_cast<std::uintptr_t>(player) + MobileObjectBaseProcessOffset);
    return baseProcess
        && readUInt8(reinterpret_cast<std::uintptr_t>(baseProcess) + MiddleHighProcessWeaponOutOffset) != 0;
}

bool playerCombatWeaponReady()
{
    return playerWeaponOut() || currentWeaponClassMeleeOrUnarmed();
}

void* activeGameCameraObject()
{
    if (!playerThirdPersonActive())
    {
        if (void* firstPersonCameraNode = readPointer(Camera1stNodeAddress))
        {
            if (looksLikeNiObject(firstPersonCameraNode))
                return firstPersonCameraNode;
        }
    }

    void* thirdPersonCameraNode = readPointer(Camera3rdNodeAddress);
    return looksLikeNiObject(thirdPersonCameraNode) ? thirdPersonCameraNode : nullptr;
}

bool thirdPersonL3ControlsEnabled()
{
    return !strictFirstPersonEnabled()
        && envEnabled("FNVXR_THIRD_PERSON_L3_ENABLE", true);
}

bool ensureThirdPersonCameraMode(const char* source, UInt64 frame)
{
    if (!thirdPersonL3ControlsEnabled())
        return false;

    const bool alreadyThirdPerson = playerThirdPersonActive();
    if (!alreadyThirdPerson)
    {
        static UInt64 lastEnsureTapMs = 0;
        const UInt64 nowMs = GetTickCount64();
        const UInt64 duplicateGuardMs =
            static_cast<UInt64>((std::max)(100, getIntFromEnv("FNVXR_THIRD_PERSON_TOGGLE_GUARD_MS", 350)));
        if (lastEnsureTapMs != 0 && nowMs < lastEnsureTapMs + duplicateGuardMs)
        {
            logTelemetry(
                "thirdPersonToggle guard frame=%llu source=%s alreadyThird=0 key=F\n",
                static_cast<unsigned long long>(frame),
                source ? source : "unknown");
            return false;
        }

        const bool tapped = tapDirectInputKey(DIK_F);
        if (tapped)
            lastEnsureTapMs = nowMs;
        logTelemetry(
            "thirdPersonToggle fire frame=%llu source=%s alreadyThird=0 key=F tapped=%d\n",
            static_cast<unsigned long long>(frame),
            source ? source : "unknown",
            static_cast<int>(tapped));
        return tapped;
    }

    logTelemetry(
        "thirdPersonToggle skip frame=%llu source=%s alreadyThird=1 key=F\n",
        static_cast<unsigned long long>(frame),
        source ? source : "unknown");
    return false;
}

bool toggleThirdPersonCameraMode(const char* source, UInt64 frame)
{
    if (!thirdPersonL3ControlsEnabled())
        return false;

    const bool beforeThirdPerson = playerThirdPersonActive();
    const bool tapped = tapDirectInputKey(DIK_F);
    logTelemetry(
        "thirdPersonToggle tap frame=%llu source=%s beforeThird=%d key=F tapped=%d\n",
        static_cast<unsigned long long>(frame),
        source ? source : "unknown",
        static_cast<int>(beforeThirdPerson),
        static_cast<int>(tapped));
    return tapped;
}

bool publishMouseWheelInput(std::int32_t delta, UInt64 frame, const char* source)
{
    if (delta == 0)
        return false;

    const bool published = publishInputEvent(
        fnvxr::shared::InputEventTypeMouseWheel,
        0,
        delta,
        0,
        0,
        frame);
    logTelemetry(
        "mouseWheel wheel=%ld frame=%llu source=%s published=%d\n",
        static_cast<long>(delta),
        static_cast<unsigned long long>(frame),
        source ? source : "unknown",
        static_cast<int>(published));
    return published;
}

bool uiMapZoomVisible(UInt32 menuBits)
{
    return uiInputAllowedFromMenuBits(menuBits)
        && menuVisibleWithTile(kMenuTypeMap)
        && envEnabled("FNVXR_UI_MAP_ZOOM_ENABLE", true);
}

int uiMapZoomWheelDelta()
{
    return (std::max)(1, getIntFromEnv("FNVXR_UI_MAP_ZOOM_WHEEL_DELTA", 120));
}

UInt64 uiMapZoomRepeatMs()
{
    return static_cast<UInt64>((std::max)(40, getIntFromEnv("FNVXR_UI_MAP_ZOOM_REPEAT_MS", 120)));
}

bool publishUiMapZoom(int direction, UInt64 frame, const char* source)
{
    if (direction == 0)
        return false;

    const int clampedDirection = direction > 0 ? 1 : -1;
    const bool published = publishMouseWheelInput(clampedDirection * uiMapZoomWheelDelta(), frame, source);
    logTelemetry(
        "uiMapZoom fire frame=%llu source=%s direction=%d wheel=%d published=%d\n",
        static_cast<unsigned long long>(frame),
        source ? source : "unknown",
        clampedDirection,
        clampedDirection * uiMapZoomWheelDelta(),
        static_cast<int>(published));
    return published;
}

int externalUiMapZoomDirection(const SharedXInputState& state)
{
    const int triggerThreshold = std::clamp(getIntFromEnv("FNVXR_UI_MAP_ZOOM_TRIGGER_THRESHOLD", 64), 1, 255);
    if (envEnabled("FNVXR_UI_MAP_ZOOM_TRIGGERS_ENABLE", false))
    {
        if (state.rightTrigger >= triggerThreshold)
            return 1;
        if (state.leftTrigger >= triggerThreshold)
            return -1;
    }

    if (!envEnabled("FNVXR_UI_MAP_ZOOM_RIGHT_STICK_ENABLE", true))
        return 0;

    const int deadzone = (std::max)(1000, getIntFromEnv("FNVXR_UI_MAP_ZOOM_STICK_DEADZONE", 16000));
    int rightStickY = state.rightThumbY;
    SharedDInputState dinput {};
    if (readSharedDInputSnapshot(dinput))
        rightStickY = dinput.rightStickY;
    if (rightStickY > deadzone)
        return 1;
    if (rightStickY < -deadzone)
        return -1;
    return 0;
}

int poseUiMapZoomDirection(const fnvxr::PoseFrame& pose)
{
    const float triggerThreshold = std::clamp(
        getFloatFromEnv("FNVXR_UI_MAP_ZOOM_POSE_TRIGGER_THRESHOLD", 0.25f),
        0.01f,
        1.0f);
    if (envEnabled("FNVXR_UI_MAP_ZOOM_TRIGGERS_ENABLE", false))
    {
        if (pose.rightTrigger >= triggerThreshold)
            return 1;
        if (pose.leftTrigger >= triggerThreshold)
            return -1;
    }

    if (!envEnabled("FNVXR_UI_MAP_ZOOM_RIGHT_STICK_ENABLE", true))
        return 0;

    const float deadzone = std::clamp(
        getFloatFromEnv("FNVXR_UI_MAP_ZOOM_POSE_STICK_DEADZONE", 0.45f),
        0.05f,
        0.95f);
    if (pose.rightThumbstickY > deadzone)
        return 1;
    if (pose.rightThumbstickY < -deadzone)
        return -1;
    return 0;
}

int thirdPersonZoomDeadzone()
{
    return (std::max)(1000, getIntFromEnv("FNVXR_THIRD_PERSON_ZOOM_DEADZONE", 9000));
}

bool thirdPersonZoomStickActive(int rightStickY)
{
    return std::abs(rightStickY) > thirdPersonZoomDeadzone();
}

bool driveThirdPersonZoomFromRightStick(int rightStickY, UInt64 frame, const char* source)
{
    if (!thirdPersonL3ControlsEnabled() || !thirdPersonZoomStickActive(rightStickY))
        return false;

    static UInt64 lastZoomMs = 0;
    const UInt64 nowMs = GetTickCount64();
    const UInt64 repeatMs = static_cast<UInt64>((std::max)(20, getIntFromEnv("FNVXR_THIRD_PERSON_ZOOM_REPEAT_MS", 80)));
    if (lastZoomMs != 0 && nowMs < lastZoomMs + repeatMs)
        return false;

    int direction = rightStickY > 0 ? 1 : -1;
    if (envEnabled("FNVXR_THIRD_PERSON_ZOOM_INVERT", false))
        direction = -direction;
    const int wheelDelta = (std::max)(1, getIntFromEnv("FNVXR_THIRD_PERSON_ZOOM_WHEEL_DELTA", 120));
    lastZoomMs = nowMs;
    return publishMouseWheelInput(direction * wheelDelta, frame, source);
}

bool updateThirdPersonL3Control(bool held, int rightStickY, UInt64 frame, const char* source)
{
    if (!thirdPersonL3ControlsEnabled())
    {
        g_thirdPersonL3Held = false;
        g_thirdPersonL3ChordUsed = false;
        g_thirdPersonL3DownMs = 0;
        return false;
    }

    const UInt64 nowMs = GetTickCount64();
    if (held && !g_thirdPersonL3Held)
    {
        g_thirdPersonL3Held = true;
        g_thirdPersonL3ChordUsed = false;
        g_thirdPersonL3DownMs = nowMs;
        logTelemetry(
            "thirdPersonL3 down frame=%llu source=%s third=%d\n",
            static_cast<unsigned long long>(frame),
            source ? source : "unknown",
            static_cast<int>(playerThirdPersonActive()));
    }

    if (held)
    {
        if (!thirdPersonZoomStickActive(rightStickY))
            return false;

        g_thirdPersonL3ChordUsed = true;
        if (!playerThirdPersonActive())
            ensureThirdPersonCameraMode(source, frame);
        return driveThirdPersonZoomFromRightStick(rightStickY, frame, source);
    }

    if (!g_thirdPersonL3Held)
        return false;

    const UInt64 heldMs = g_thirdPersonL3DownMs != 0 ? nowMs - g_thirdPersonL3DownMs : 0;
    const UInt64 tapMaxMs =
        static_cast<UInt64>((std::max)(100, getIntFromEnv("FNVXR_THIRD_PERSON_TAP_MAX_MS", 650)));
    const bool tapToggle = !g_thirdPersonL3ChordUsed && heldMs <= tapMaxMs;
    logTelemetry(
        "thirdPersonL3 up frame=%llu source=%s heldMs=%llu chordUsed=%d tapToggle=%d third=%d\n",
        static_cast<unsigned long long>(frame),
        source ? source : "unknown",
        static_cast<unsigned long long>(heldMs),
        static_cast<int>(g_thirdPersonL3ChordUsed),
        static_cast<int>(tapToggle),
        static_cast<int>(playerThirdPersonActive()));

    g_thirdPersonL3Held = false;
    g_thirdPersonL3ChordUsed = false;
    g_thirdPersonL3DownMs = 0;
    if (tapToggle)
        return toggleThirdPersonCameraMode(source, frame);
    return false;
}

void cancelThirdPersonL3Control(const char* source, UInt64 frame)
{
    if (!g_thirdPersonL3Held)
        return;

    logTelemetry(
        "thirdPersonL3 cancel frame=%llu source=%s chordUsed=%d third=%d\n",
        static_cast<unsigned long long>(frame),
        source ? source : "unknown",
        static_cast<int>(g_thirdPersonL3ChordUsed),
        static_cast<int>(playerThirdPersonActive()));
    g_thirdPersonL3Held = false;
    g_thirdPersonL3ChordUsed = false;
    g_thirdPersonL3DownMs = 0;
}

void updateSharedCamera(UInt64 frame, UInt32 menuBits)
{
    if (!g_cameraState)
        return;

    InterlockedIncrement(&g_cameraState->sequence);
    MemoryBarrier();
    g_cameraState->magic = CameraSharedMagic;
    g_cameraState->version = CameraSharedVersion;
    g_cameraState->frame = frame;

    if (!cameraAllowedForMenuBits(menuBits))
    {
        g_cameraState->active = 0;
        MemoryBarrier();
        InterlockedIncrement(&g_cameraState->sequence);
        if (g_lastSharedCameraActive != 0 || g_lastSharedCameraReason != 1 || (frame % 300) == 0)
        {
            g_lastSharedCameraActive = 0;
            g_lastSharedCameraReason = 1;
            logTelemetry(
                "{\"event\":\"fnvxrWorldCameraState\",\"frame\":%llu,\"active\":false,\"reason\":\"%s\",\"sequence\":%ld}\n",
                static_cast<unsigned long long>(frame),
                sharedCameraReasonName(1),
                static_cast<LONG>(g_cameraState->sequence));
        }
        return;
    }

    void* camera = activeGameCameraObject();
    if (!camera)
    {
        g_cameraState->active = 0;
        MemoryBarrier();
        InterlockedIncrement(&g_cameraState->sequence);
        if (g_lastSharedCameraActive != 0 || g_lastSharedCameraReason != 2 || (frame % 120) == 0)
        {
            g_lastSharedCameraActive = 0;
            g_lastSharedCameraReason = 2;
            logTelemetry(
                "{\"event\":\"fnvxrWorldCameraState\",\"frame\":%llu,\"active\":false,\"reason\":\"%s\",\"sequence\":%ld}\n",
                static_cast<unsigned long long>(frame),
                sharedCameraReasonName(2),
                static_cast<LONG>(g_cameraState->sequence));
        }
        return;
    }

    void* player = readPointer(PlayerCharacterAddress);
    g_cameraState->active = 1;
    g_cameraState->thirdPerson = player
        ? readUInt8(reinterpret_cast<std::uintptr_t>(player) + PlayerCharacterIsThirdPersonOffset)
        : 0;
    readNiAvObjectWorldTransform(camera, g_cameraState->worldRot, g_cameraState->worldPos);
    MemoryBarrier();
    InterlockedIncrement(&g_cameraState->sequence);
    if (g_lastSharedCameraActive != 1 || g_lastSharedCameraReason != 0 || (frame % 120) == 0)
    {
        g_lastSharedCameraActive = 1;
        g_lastSharedCameraReason = 0;
        logTelemetry(
            "{\"event\":\"fnvxrWorldCameraState\",\"frame\":%llu,\"active\":true,\"reason\":\"%s\",\"sequence\":%ld,\"camera\":\"%p\",\"thirdPerson\":%lu,\"position\":[%.4f,%.4f,%.4f],\"rotation\":[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f]}\n",
            static_cast<unsigned long long>(frame),
            sharedCameraReasonName(0),
            static_cast<LONG>(g_cameraState->sequence),
            camera,
            static_cast<unsigned long>(g_cameraState->thirdPerson),
            g_cameraState->worldPos[0],
            g_cameraState->worldPos[1],
            g_cameraState->worldPos[2],
            g_cameraState->worldRot[0],
            g_cameraState->worldRot[1],
            g_cameraState->worldRot[2],
            g_cameraState->worldRot[3],
            g_cameraState->worldRot[4],
            g_cameraState->worldRot[5],
            g_cameraState->worldRot[6],
            g_cameraState->worldRot[7],
            g_cameraState->worldRot[8]);
    }
}

void updateSharedPlayer(UInt64 frame, RuntimePhase phase)
{
    if (!g_playerState)
        return;

    InterlockedIncrement(&g_playerState->sequence);
    MemoryBarrier();
    g_playerState->magic = PlayerSharedMagic;
    g_playerState->version = PlayerSharedVersion;
    g_playerState->frame = frame;
    g_playerState->flags = 0;
    g_playerState->currentCellFormId = 0;
    g_playerState->playerAddress = 0;
    g_playerState->playerNodeAddress = 0;
    g_playerState->cameraNodeAddress = 0;
    std::memset(g_playerState->playerWorldRot, 0, sizeof(g_playerState->playerWorldRot));
    std::memset(g_playerState->playerWorldPos, 0, sizeof(g_playerState->playerWorldPos));
    std::memset(g_playerState->cameraWorldRot, 0, sizeof(g_playerState->cameraWorldRot));
    std::memset(g_playerState->cameraWorldPos, 0, sizeof(g_playerState->cameraWorldPos));
    std::memset(g_playerState->reserved, 0, sizeof(g_playerState->reserved));

    UInt32 flags = 0;
    void* player = readPointer(PlayerCharacterAddress);
    g_playerState->playerAddress = sharedPointerAddress(player);

    void* parentCell = nullptr;
    const UInt32 currentCellFormId = currentPlayerCellFormId(player, &parentCell);
    if (currentCellFormId != 0)
    {
        g_playerState->currentCellFormId = currentCellFormId;
        flags |= fnvxr::shared::PlayerSharedFlagCellKnown;
    }

    const UInt8 thirdPerson = player
        ? readUInt8(reinterpret_cast<std::uintptr_t>(player) + PlayerCharacterIsThirdPersonOffset)
        : 0;
    if (thirdPerson)
        flags |= fnvxr::shared::PlayerSharedFlagThirdPerson;
    const bool engineWeaponOut = playerWeaponOut();
    const bool combatWeaponReady = playerCombatWeaponReady();
    const UInt32 weaponClass = currentWeaponClass();
    if (combatWeaponReady)
        flags |= fnvxr::shared::PlayerSharedFlagWeaponOut;
    if (weaponClassKnown(weaponClass))
        flags |= fnvxr::shared::PlayerSharedFlagWeaponClassKnown;
    if (phase == RuntimePhase::Gameplay)
        flags |= fnvxr::shared::PlayerSharedFlagGameplay;
    g_playerState->reserved[fnvxr::shared::PlayerSharedWeaponClassReservedIndex] = weaponClass;
    g_playerState->reserved[fnvxr::shared::PlayerSharedEquippedWeaponFormIdReservedIndex] = g_lastKnownWeaponFormId;
    g_playerState->reserved[fnvxr::shared::PlayerSharedEquippedFavoriteSlotReservedIndex] = g_lastKnownWeaponFavoriteSlot;

    void* playerNode = player
        ? readPointer(reinterpret_cast<std::uintptr_t>(player) + PlayerCharacterFirstPersonNodeOffset)
        : nullptr;
    if (looksLikeNiObject(playerNode))
    {
        g_playerState->playerNodeAddress = sharedPointerAddress(playerNode);
        readNiAvObjectWorldTransform(playerNode, g_playerState->playerWorldRot, g_playerState->playerWorldPos);
        flags |= fnvxr::shared::PlayerSharedFlagPlayerNodeValid;
    }

    void* camera = (g_cameraState && g_cameraState->active) ? activeGameCameraObject() : nullptr;
    if (looksLikeNiObject(camera))
    {
        g_playerState->cameraNodeAddress = sharedPointerAddress(camera);
        readNiAvObjectWorldTransform(camera, g_playerState->cameraWorldRot, g_playerState->cameraWorldPos);
        flags |= fnvxr::shared::PlayerSharedFlagCameraValid;
    }

    g_playerState->flags = flags;
    MemoryBarrier();
    InterlockedIncrement(&g_playerState->sequence);

    if (g_lastSharedPlayerFlags != flags
        || g_lastSharedPlayerCell != g_playerState->currentCellFormId
        || g_lastSharedPlayerWeaponClass != weaponClass
        || (frame % 120) == 0)
    {
        g_lastSharedPlayerFlags = flags;
        g_lastSharedPlayerCell = g_playerState->currentCellFormId;
        g_lastSharedPlayerWeaponClass = weaponClass;
        logTelemetry(
            "{\"event\":\"fnvxrPlayerState\",\"frame\":%llu,\"sequence\":%ld,\"flags\":%lu,\"weaponOut\":%s,\"engineWeaponOut\":%s,\"weaponClass\":\"%s\",\"weaponClassId\":%lu,\"weaponClassKnown\":%s,\"weaponFormId\":\"0x%08lx\",\"weaponFavoriteSlot\":%lu,\"player\":\"%p\",\"playerNode\":\"%p\",\"cameraNode\":\"%p\",\"parentCell\":\"%p\",\"cellKnown\":%s,\"cellFormId\":%lu,\"playerPosition\":[%.4f,%.4f,%.4f],\"cameraPosition\":[%.4f,%.4f,%.4f]}\n",
            static_cast<unsigned long long>(frame),
            static_cast<LONG>(g_playerState->sequence),
            static_cast<unsigned long>(flags),
            combatWeaponReady ? "true" : "false",
            engineWeaponOut ? "true" : "false",
            weaponClassName(weaponClass),
            static_cast<unsigned long>(weaponClass),
            weaponClassKnown(weaponClass) ? "true" : "false",
            static_cast<unsigned long>(g_lastKnownWeaponFormId),
            static_cast<unsigned long>(g_lastKnownWeaponFavoriteSlot),
            player,
            playerNode,
            camera,
            parentCell,
            (flags & fnvxr::shared::PlayerSharedFlagCellKnown) ? "true" : "false",
            static_cast<unsigned long>(g_playerState->currentCellFormId),
            g_playerState->playerWorldPos[0],
            g_playerState->playerWorldPos[1],
            g_playerState->playerWorldPos[2],
            g_playerState->cameraWorldPos[0],
            g_playerState->cameraWorldPos[1],
            g_playerState->cameraWorldPos[2]);
    }
}

void updateNiAvObjectTransform(void* object)
{
    // Runtime evidence from the retail NiCamera path shows this vtable slot
    // requires an additional stack argument. Calling it as void(this) corrupts
    // the call frame, so this old diagnostic switch is permanently fail-closed
    // until an exact signature/call-site contract is proven.
    static LONG logged = 0;
    if (object && InterlockedIncrement(&logged) == 1)
        logTelemetry("camera UpdateTransform refused object=%p reason=unverified-vtable-signature\n", object);
}

void restoreVrPoseFromGameCamera()
{
    if (!g_haveCameraBase || !g_cameraBaseObject || g_lastAppliedCamera != g_cameraBaseObject)
        return;

    const auto base = reinterpret_cast<std::uintptr_t>(g_cameraBaseObject);
    writeMatrix33(base + NiAvObjectLocalRotationOffset, g_cameraBaseLocalRotation);
    writeVec3(base + NiAvObjectLocalTranslationOffset, g_cameraBaseLocalTranslation);
    if (envEnabled("FNVXR_CAMERA_WRITE_WORLD", false))
        writeMatrix33(base + NiAvObjectWorldRotationOffset, g_cameraBaseWorldRotation);

    // The engine must always begin UpdateCamera from its own unmodified result.
    // Otherwise the previous HMD delta becomes input to the next update and can
    // leak head motion into the player/body frame or accumulate drift.
    g_lastAppliedCamera = nullptr;
}

void applyVrPoseToGameCamera()
{
    if (!envEnabled("FNVXR_CAMERA_HOOK", false))
        return;
    if (!inCameraGameplay())
    {
        g_haveCameraBase = false;
        g_cameraBaseObject = nullptr;
        return;
    }

    Quat xrRotationDelta {};
    Vec3 xrPositionDelta {};
    if (!readLatestCameraPose(xrRotationDelta, xrPositionDelta))
        return;

    void* camera = activeGameCameraObject();
    if (!camera)
        return;

    const auto base = reinterpret_cast<std::uintptr_t>(camera);
    const bool baseChanged = !g_haveCameraBase || g_cameraBaseObject != camera;
    g_cameraBaseObject = camera;
    // Capture the fresh engine-authored camera after UpdateCamera on every
    // invocation. hookedUpdateCamera restores the previous unmodified base
    // before calling the engine, keeping HMD motion camera-local.
    g_cameraBaseLocalRotation = readMatrix33(base + NiAvObjectLocalRotationOffset);
    g_cameraBaseLocalTranslation = readVec3(base + NiAvObjectLocalTranslationOffset);
    g_cameraBaseWorldRotation = readMatrix33(base + NiAvObjectWorldRotationOffset);
    g_haveCameraBase = true;
    if (baseChanged || envEnabled("FNVXR_CAMERA_RESET_BASE", false))
    {
        g_lastAppliedCameraPoseSequence = 0;
        logTelemetry(
            "cameraHook base latched seq=%ld camera=%p localT=(%.4f %.4f %.4f)\n",
            g_lastCameraPoseSequence,
            camera,
            g_cameraBaseLocalTranslation.x,
            g_cameraBaseLocalTranslation.y,
            g_cameraBaseLocalTranslation.z);
    }

    if (!envEnabled("FNVXR_CAMERA_APPLY", false))
    {
        static LONG loggedCandidate = 0;
        const LONG count = InterlockedIncrement(&loggedCandidate);
        if (count <= 20 || count % 120 == 0)
        {
            logTelemetry(
                "cameraHook candidate count=%ld seq=%ld camera=%p apply=0 xrRot=(%.4f %.4f %.4f %.4f) xrPos=(%.4f %.4f %.4f)\n",
                count,
                g_lastCameraPoseSequence,
                camera,
                xrRotationDelta.x,
                xrRotationDelta.y,
                xrRotationDelta.z,
                xrRotationDelta.w,
                xrPositionDelta.x,
                xrPositionDelta.y,
                xrPositionDelta.z);
            logTelemetry(
                "{\"event\":\"fnvxrCameraApply\",\"ready\":true,\"applied\":false,\"reason\":\"apply-disabled\",\"count\":%ld,\"sequence\":%ld,\"camera\":\"%p\",\"xrRot\":[%.4f,%.4f,%.4f,%.4f],\"xrPos\":[%.4f,%.4f,%.4f]}\n",
                count,
                g_lastCameraPoseSequence,
                camera,
                xrRotationDelta.x,
                xrRotationDelta.y,
                xrRotationDelta.z,
                xrRotationDelta.w,
                xrPositionDelta.x,
                xrPositionDelta.y,
                xrPositionDelta.z);
        }
        return;
    }
    Matrix33 gameRotationDelta = xrDeltaToGamebryoMatrix(xrRotationDelta);
    const bool yawOnly = envEnabled("FNVXR_CAMERA_YAW_ONLY", false);
    if (yawOnly)
        gameRotationDelta = yawOnlyMatrix(gameRotationDelta);
    const bool preMultiply = envEnabled("FNVXR_CAMERA_PREMULTIPLY", false);
    const bool applyRotation = envEnabled("FNVXR_CAMERA_APPLY_ROTATION", true);
    const bool applyTranslation = envEnabled("FNVXR_CAMERA_APPLY_TRANSLATION", false);
    if (applyRotation)
    {
        const Matrix33 localRotated = preMultiply
            ? multiplyMatrix33(gameRotationDelta, g_cameraBaseLocalRotation)
            : multiplyMatrix33(g_cameraBaseLocalRotation, gameRotationDelta);
        writeMatrix33(base + NiAvObjectLocalRotationOffset, localRotated);
    }

    const float positionScale = getFloatFromEnv("FNVXR_CAMERA_POSITION_SCALE", 0.0f);
    if (applyTranslation && positionScale != 0.0f)
    {
        const Vec3 gameDelta = xrDeltaToGamebryoVector(xrPositionDelta);
        Vec3 localTranslate = g_cameraBaseLocalTranslation;
        localTranslate.x += gameDelta.x * positionScale;
        localTranslate.y += gameDelta.y * positionScale;
        localTranslate.z += gameDelta.z * positionScale;
        writeVec3(base + NiAvObjectLocalTranslationOffset, localTranslate);
    }

    if (envEnabled("FNVXR_CAMERA_WRITE_WORLD", false))
    {
        const Matrix33 worldRotated = preMultiply
            ? multiplyMatrix33(gameRotationDelta, g_cameraBaseWorldRotation)
            : multiplyMatrix33(g_cameraBaseWorldRotation, gameRotationDelta);
        writeMatrix33(base + NiAvObjectWorldRotationOffset, worldRotated);
    }

    if (envEnabled("FNVXR_CAMERA_UPDATE_TRANSFORM", false))
        updateNiAvObjectTransform(camera);
    g_lastAppliedCamera = camera;
    g_lastAppliedCameraPoseSequence = g_lastCameraPoseSequence;

    static LONG logged = 0;
    const LONG count = InterlockedIncrement(&logged);
    if (count <= 12 || count % 120 == 0)
    {
        logTelemetry(
            "cameraHook applied count=%ld seq=%ld camera=%p localOnly=%d preMultiply=%d yawOnly=%d xrRot=(%.4f %.4f %.4f %.4f) xrPos=(%.4f %.4f %.4f) scale=%.2f\n",
            count,
            g_lastCameraPoseSequence,
            camera,
            !envEnabled("FNVXR_CAMERA_WRITE_WORLD", false) ? 1 : 0,
            preMultiply ? 1 : 0,
            yawOnly ? 1 : 0,
            xrRotationDelta.x,
            xrRotationDelta.y,
            xrRotationDelta.z,
            xrRotationDelta.w,
            xrPositionDelta.x,
            xrPositionDelta.y,
            xrPositionDelta.z,
            positionScale);
        logTelemetry(
            "{\"event\":\"fnvxrCameraApply\",\"ready\":true,\"applied\":true,\"count\":%ld,\"sequence\":%ld,\"camera\":\"%p\",\"writeWorld\":%s,\"preMultiply\":%s,\"yawOnly\":%s,\"applyRotation\":%s,\"applyTranslation\":%s,\"positionScale\":%.4f,\"xrRot\":[%.4f,%.4f,%.4f,%.4f],\"xrPos\":[%.4f,%.4f,%.4f]}\n",
            count,
            g_lastCameraPoseSequence,
            camera,
            envEnabled("FNVXR_CAMERA_WRITE_WORLD", false) ? "true" : "false",
            preMultiply ? "true" : "false",
            yawOnly ? "true" : "false",
            applyRotation ? "true" : "false",
            applyTranslation ? "true" : "false",
            positionScale,
            xrRotationDelta.x,
            xrRotationDelta.y,
            xrRotationDelta.z,
            xrRotationDelta.w,
            xrPositionDelta.x,
            xrPositionDelta.y,
            xrPositionDelta.z);
    }
}

using UpdateCameraFn = void (__thiscall*)(void*, UInt8, UInt8);

void __fastcall hookedUpdateCamera(void* player, void*, UInt8 isCalledFromFunc21, UInt8 zeroSkipUpdateLod)
{
    restoreVrPoseFromGameCamera();

    auto original = reinterpret_cast<UpdateCameraFn>(g_updateCameraTrampoline);
    if (original)
        original(player, isCalledFromFunc21, zeroSkipUpdateLod);

    applyVrPoseToGameCamera();
}

bool writeJump(UInt32 source, void* target)
{
    DWORD oldProtect = 0;
    if (!VirtualProtect(pointerFromAddress32<void*>(source), 5, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    auto* bytes = pointerFromAddress32<UInt8*>(source);
    bytes[0] = 0xE9;
    const std::intptr_t relTarget =
        static_cast<std::intptr_t>(reinterpret_cast<std::uintptr_t>(target))
        - static_cast<std::intptr_t>(source)
        - 5;
    *pointerFromAddress32<UInt32*>(source + 1) = static_cast<UInt32>(relTarget);
    VirtualProtect(pointerFromAddress32<void*>(source), 5, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), pointerFromAddress32<void*>(source), 5);
    return true;
}

bool installCameraHook()
{
    if (g_cameraHookInstalled)
        return true;
    if (!envEnabled("FNVXR_INSTALL_CAMERA_HOOK", true))
    {
        logTelemetry("cameraHook install disabled\n");
        return true;
    }

    auto* target = pointerFromAddress32<UInt8*>(PlayerCharacterUpdateCameraAddress);
    __try
    {
        const UInt8 expected[] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF };
        if (std::memcmp(target, expected, sizeof(expected)) != 0)
        {
            logTelemetry(
                "cameraHook prologue mismatch got=%02X %02X %02X %02X %02X\n",
                target[0],
                target[1],
                target[2],
                target[3],
                target[4]);
            return false;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        logTelemetry("cameraHook prologue read exception\n");
        return false;
    }

    auto* trampoline = static_cast<UInt8*>(VirtualAlloc(nullptr, 10, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!trampoline)
    {
        logTelemetry("cameraHook trampoline alloc failed err=%lu\n", GetLastError());
        return false;
    }

    std::memcpy(trampoline, target, 5);
    trampoline[5] = 0xE9;
    *reinterpret_cast<UInt32*>(trampoline + 6) =
        (PlayerCharacterUpdateCameraAddress + 5) - (address32FromPointer(trampoline) + 10);
    FlushInstructionCache(GetCurrentProcess(), trampoline, 10);
    g_updateCameraTrampoline = trampoline;

    if (!writeJump(PlayerCharacterUpdateCameraAddress, reinterpret_cast<void*>(hookedUpdateCamera)))
    {
        logTelemetry("cameraHook write jump failed err=%lu\n", GetLastError());
        VirtualFree(trampoline, 0, MEM_RELEASE);
        g_updateCameraTrampoline = nullptr;
        return false;
    }

    g_cameraHookInstalled = true;
    logTelemetry(
        "cameraHook installed target=%p hook=%p trampoline=%p\n",
        pointerFromAddress32<void*>(PlayerCharacterUpdateCameraAddress),
        reinterpret_cast<void*>(hookedUpdateCamera),
        g_updateCameraTrampoline);
    return true;
}

Matrix33 transposeMatrix33(const Matrix33& matrix)
{
    Matrix33 result {};
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 3; ++column)
            result.m[row][column] = matrix.m[column][row];
    }
    return result;
}

Vec3 addVec3(Vec3 lhs, Vec3 rhs)
{
    return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

Vec3 subtractVec3(Vec3 lhs, Vec3 rhs)
{
    return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
}

Vec3 scaleVec3(Vec3 value, float scale)
{
    return { value.x * scale, value.y * scale, value.z * scale };
}

float dotVec3(Vec3 lhs, Vec3 rhs)
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vec3 crossVec3(Vec3 lhs, Vec3 rhs)
{
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x
    };
}

float lengthVec3(Vec3 value)
{
    return std::sqrt(dotVec3(value, value));
}

float quaternionAngularDistance(Quat left, Quat right)
{
    left = normalizeQuat(left);
    right = normalizeQuat(right);
    const float dot = std::clamp(
        std::fabs(left.x * right.x + left.y * right.y + left.z * right.z + left.w * right.w),
        0.0f,
        1.0f);
    return 2.0f * std::acos(dot);
}

float matrixAngularDistance(const Matrix33& left, const Matrix33& right)
{
    const Matrix33 delta = multiplyMatrix33(transposeMatrix33(left), right);
    const float cosine = std::clamp(
        (delta.m[0][0] + delta.m[1][1] + delta.m[2][2] - 1.0f) * 0.5f,
        -1.0f,
        1.0f);
    return std::acos(cosine);
}

bool finiteVec3(Vec3 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool finiteUsableQuat(Quat value)
{
    if (!std::isfinite(value.x) || !std::isfinite(value.y)
        || !std::isfinite(value.z) || !std::isfinite(value.w))
    {
        return false;
    }
    const float lengthSquared = value.x * value.x + value.y * value.y
        + value.z * value.z + value.w * value.w;
    return lengthSquared >= 0.25f && lengthSquared <= 4.0f;
}

bool finiteMatrix33(const Matrix33& matrix)
{
    for (const auto& row : matrix.m)
    {
        for (float value : row)
        {
            if (!std::isfinite(value))
                return false;
        }
    }
    const float determinant =
        matrix.m[0][0] * (matrix.m[1][1] * matrix.m[2][2] - matrix.m[1][2] * matrix.m[2][1])
        - matrix.m[0][1] * (matrix.m[1][0] * matrix.m[2][2] - matrix.m[1][2] * matrix.m[2][0])
        + matrix.m[0][2] * (matrix.m[1][0] * matrix.m[2][1] - matrix.m[1][1] * matrix.m[2][0]);
    if (std::fabs(determinant - 1.0f) > 0.05f)
        return false;
    for (int columnA = 0; columnA < 3; ++columnA)
    {
        for (int columnB = 0; columnB < 3; ++columnB)
        {
            float dot = 0.0f;
            for (int row = 0; row < 3; ++row)
                dot += matrix.m[row][columnA] * matrix.m[row][columnB];
            const float expected = columnA == columnB ? 1.0f : 0.0f;
            if (std::fabs(dot - expected) > 0.05f)
                return false;
        }
    }
    return true;
}

Vec3 normalizeVec3(Vec3 value, Vec3 fallback = { 1.0f, 0.0f, 0.0f })
{
    const float magnitude = lengthVec3(value);
    if (!std::isfinite(magnitude) || magnitude < 0.000001f)
        return fallback;
    return scaleVec3(value, 1.0f / magnitude);
}

Vec3 transformVec3(const Matrix33& matrix, Vec3 vector)
{
    return {
        matrix.m[0][0] * vector.x + matrix.m[0][1] * vector.y + matrix.m[0][2] * vector.z,
        matrix.m[1][0] * vector.x + matrix.m[1][1] * vector.y + matrix.m[1][2] * vector.z,
        matrix.m[2][0] * vector.x + matrix.m[2][1] * vector.y + matrix.m[2][2] * vector.z
    };
}

Matrix33 rotationFromTo(Vec3 from, Vec3 to)
{
    from = normalizeVec3(from);
    to = normalizeVec3(to, from);
    const float cosine = std::clamp(dotVec3(from, to), -1.0f, 1.0f);
    Quat rotation {};
    if (cosine < -0.9999f)
    {
        Vec3 axis = crossVec3(from, { 1.0f, 0.0f, 0.0f });
        if (lengthVec3(axis) < 0.0001f)
            axis = crossVec3(from, { 0.0f, 1.0f, 0.0f });
        axis = normalizeVec3(axis, { 0.0f, 0.0f, 1.0f });
        rotation = { axis.x, axis.y, axis.z, 0.0f };
    }
    else
    {
        const Vec3 axis = crossVec3(from, to);
        rotation = normalizeQuat({ axis.x, axis.y, axis.z, 1.0f + cosine });
    }
    return matrixFromQuat(rotation);
}

bool writeFloat(std::uintptr_t address, float value)
{
    if (!std::isfinite(value))
        return false;
    __try
    {
        *reinterpret_cast<float*>(address) = value;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        logTelemetry("retailRig float write exception address=%p\n", reinterpret_cast<void*>(address));
        return false;
    }
}

NiRttiRaw* niObjectRtti(void* object)
{
    if (!object)
        return nullptr;
    __try
    {
        void** vtable = *reinterpret_cast<void***>(object);
        if (!vtable || !vtable[2])
            return nullptr;
        using GetTypeFn = NiRttiRaw* (__thiscall*)(void*);
        return reinterpret_cast<GetTypeFn>(vtable[2])(object);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

// 0 = not an NiAVObject, 1 = leaf NiAVObject, 2 = NiNode-compatible container.
int niObjectKind(void* object)
{
    NiRttiRaw* rtti = niObjectRtti(object);
    for (int depth = 0; rtti && depth < 32; ++depth)
    {
        const UInt32 address = address32FromPointer(rtti);
        if (address == NiRttiNiNodeAddress)
            return 2;
        if (address == NiRttiNiAvObjectAddress)
            return 1;
        __try
        {
            rtti = rtti->parent;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return 0;
        }
    }
    return 0;
}

bool copyNiObjectName(void* object, char* buffer, size_t bufferSize)
{
    if (!buffer || bufferSize == 0)
        return false;
    buffer[0] = '\0';
    if (!object)
        return false;

    __try
    {
        const char* name = *reinterpret_cast<const char**>(
            reinterpret_cast<std::uintptr_t>(object) + NiObjectNetNameOffset);
        if (!name)
            return false;
        size_t index = 0;
        for (; index + 1 < bufferSize; ++index)
        {
            const char value = name[index];
            buffer[index] = value;
            if (value == '\0')
                return index != 0;
        }
        buffer[bufferSize - 1] = '\0';
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        buffer[0] = '\0';
        return false;
    }
}

bool niObjectNameEquals(void* object, const char* expected)
{
    if (!expected)
        return false;
    char name[128] {};
    return copyNiObjectName(object, name, sizeof(name)) && _stricmp(name, expected) == 0;
}

bool readNiNodeChildren(void* node, void*** children, UInt16& count)
{
    if (children)
        *children = nullptr;
    count = 0;
    if (!node || niObjectKind(node) != 2)
        return false;

    __try
    {
        auto* array = reinterpret_cast<NiTArrayRaw*>(
            reinterpret_cast<std::uintptr_t>(node) + NiNodeChildrenOffset);
        if (!array->data || array->firstFreeEntry > 1024 || array->firstFreeEntry > array->capacity)
            return false;
        if (children)
            *children = array->data;
        count = array->firstFreeEntry;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

void* findNiNodeRecursive(void* node, const char* name, int depth, UInt32& visits)
{
    if (!node || !name || depth > 64 || ++visits > 4096)
        return nullptr;
    if (niObjectNameEquals(node, name))
        return node;

    void** children = nullptr;
    UInt16 count = 0;
    if (!readNiNodeChildren(node, &children, count))
        return nullptr;

    for (UInt16 index = 0; index < count; ++index)
    {
        void* child = nullptr;
        __try
        {
            child = children[index];
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            child = nullptr;
        }
        if (void* match = findNiNodeRecursive(child, name, depth + 1, visits))
            return match;
    }
    return nullptr;
}

void* findNiNode(void* root, const char* name)
{
    UInt32 visits = 0;
    return findNiNodeRecursive(root, name, 0, visits);
}

void collectNamedNiNodesRecursive(
    void* node,
    const char* name,
    int depth,
    UInt32& visits,
    void*& onlyMatch,
    UInt32& matchCount)
{
    if (!node || !name || depth > 64 || ++visits > 4096 || matchCount > 1)
        return;
    if (niObjectNameEquals(node, name))
    {
        onlyMatch = node;
        ++matchCount;
        if (matchCount > 1)
            return;
    }
    void** children = nullptr;
    UInt16 count = 0;
    if (!readNiNodeChildren(node, &children, count))
        return;
    for (UInt16 index = 0; index < count && matchCount <= 1; ++index)
    {
        void* child = nullptr;
        __try { child = children[index]; }
        __except (EXCEPTION_EXECUTE_HANDLER) { child = nullptr; }
        collectNamedNiNodesRecursive(child, name, depth + 1, visits, onlyMatch, matchCount);
    }
}

void* findUniqueNiNode(void* root, const char* name, UInt32* matchCountOut = nullptr)
{
    UInt32 visits = 0;
    UInt32 matches = 0;
    void* match = nullptr;
    collectNamedNiNodesRecursive(root, name, 0, visits, match, matches);
    if (matchCountOut)
        *matchCountOut = matches;
    return matches == 1 ? match : nullptr;
}

bool niObjectDescendsFrom(void* object, void* ancestor)
{
    if (!object || !ancestor)
        return false;
    for (int depth = 0; object && depth < 64; ++depth)
    {
        if (object == ancestor)
            return true;
        object = readPointer(
            reinterpret_cast<std::uintptr_t>(object) + NiAvObjectParentOffset);
    }
    return false;
}

bool niObjectAncestorChainVisible(void* object, void* root)
{
    if (!object || !root)
        return false;
    for (int depth = 0; object && depth < 64; ++depth)
    {
        const UInt32 flags = readUInt32(
            reinterpret_cast<std::uintptr_t>(object) + NiAvObjectFlagsOffset,
            0xffffffffu);
        if ((flags & 1u) != 0)
            return false;
        if (object == root)
            return true;
        object = readPointer(
            reinterpret_cast<std::uintptr_t>(object) + NiAvObjectParentOffset);
    }
    return false;
}

UInt32 countVisibleNiLeaves(void* node, void* root, int depth, UInt32& visits)
{
    if (!node || !root || depth > 64 || ++visits > 4096
        || !niObjectAncestorChainVisible(node, root))
    {
        return 0;
    }
    void** children = nullptr;
    UInt16 count = 0;
    if (!readNiNodeChildren(node, &children, count))
        return niObjectKind(node) == 1 ? 1u : 0u;
    UInt32 leaves = 0;
    for (UInt16 index = 0; index < count; ++index)
    {
        void* child = nullptr;
        __try { child = children[index]; }
        __except (EXCEPTION_EXECUTE_HANDLER) { child = nullptr; }
        leaves += countVisibleNiLeaves(child, root, depth + 1, visits);
    }
    return leaves;
}

UInt32 countVisibleNiLeaves(void* node, void* root)
{
    UInt32 visits = 0;
    return countVisibleNiLeaves(node, root, 0, visits);
}

void dumpNiNodesRecursive(void* node, int depth, UInt32& visits)
{
    if (!node || depth > 48 || ++visits > 512)
        return;

    char name[128] {};
    copyNiObjectName(node, name, sizeof(name));
    const Vec3 world = readVec3(reinterpret_cast<std::uintptr_t>(node) + NiAvObjectWorldTranslationOffset);
    void* parent = readPointer(reinterpret_cast<std::uintptr_t>(node) + NiAvObjectParentOffset);
    logTelemetry(
        "retailRig node depth=%d object=%p parent=%p kind=%d name=\"%s\" world=(%.3f %.3f %.3f)\n",
        depth,
        node,
        parent,
        niObjectKind(node),
        name[0] ? name : "<unnamed>",
        world.x,
        world.y,
        world.z);

    void** children = nullptr;
    UInt16 count = 0;
    if (!readNiNodeChildren(node, &children, count))
        return;
    for (UInt16 index = 0; index < count && visits <= 512; ++index)
    {
        void* child = nullptr;
        __try
        {
            child = children[index];
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            child = nullptr;
        }
        dumpNiNodesRecursive(child, depth + 1, visits);
    }
}

void* retrievePlayerRootNode(bool firstPerson)
{
#if defined(_M_IX86)
    void* player = readPointer(PlayerCharacterAddress);
    if (!player)
        return nullptr;
    __try
    {
        using RetrieveRootNodeFn = void* (__thiscall*)(void*, bool);
        return reinterpret_cast<RetrieveRootNodeFn>(PlayerCharacterRetrieveRootNodeAddress)(player, firstPerson);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
#else
    (void)firstPerson;
    return nullptr;
#endif
}

void* currentPlayerThirdPersonAnimData()
{
#if defined(_M_IX86)
    void* player = readPointer(PlayerCharacterAddress);
    if (!player)
        return nullptr;
    __try
    {
        using GetActorAnimDataFn = void* (__thiscall*)(void*);
        return reinterpret_cast<GetActorAnimDataFn>(PlayerCharacterGetActorAnimDataAddress)(player);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
#else
    return nullptr;
#endif
}

bool readLatestRetailRigPose(VrRigPoseSnapshot& pose)
{
    if (!g_vrPoseState || g_vrPoseState->magic != VrPoseSharedMagic || g_vrPoseState->version != VrPoseSharedVersion)
        return false;

    for (int attempt = 0; attempt < 4; ++attempt)
    {
        const LONG sequenceBefore = g_vrPoseState->sequence;
        if (sequenceBefore == 0 || (sequenceBefore & 1) != 0)
            continue;
        MemoryBarrier();
        __try
        {
            pose.sequence = sequenceBefore;
            pose.frame = g_vrPoseState->frame;
            pose.predictedDisplayTime = g_vrPoseState->predictedDisplayTime;
            pose.trackingFlags = g_vrPoseState->trackingFlags;
            pose.referenceSpaceGeneration = g_vrPoseState->referenceSpaceGeneration;
            pose.producerEpoch = g_vrPoseState->producerEpoch;
            pose.hmdRot = normalizeQuat({
                g_vrPoseState->hmdRot[0], g_vrPoseState->hmdRot[1],
                g_vrPoseState->hmdRot[2], g_vrPoseState->hmdRot[3] });
            pose.hmdPos = {
                g_vrPoseState->hmdPos[0], g_vrPoseState->hmdPos[1], g_vrPoseState->hmdPos[2] };
            pose.leftRot = normalizeQuat({
                g_vrPoseState->leftRot[0], g_vrPoseState->leftRot[1],
                g_vrPoseState->leftRot[2], g_vrPoseState->leftRot[3] });
            pose.leftPos = {
                g_vrPoseState->leftPos[0], g_vrPoseState->leftPos[1], g_vrPoseState->leftPos[2] };
            pose.rightRot = normalizeQuat({
                g_vrPoseState->rightRot[0], g_vrPoseState->rightRot[1],
                g_vrPoseState->rightRot[2], g_vrPoseState->rightRot[3] });
            pose.rightPos = {
                g_vrPoseState->rightPos[0], g_vrPoseState->rightPos[1], g_vrPoseState->rightPos[2] };
            pose.leftAimRot = normalizeQuat({
                g_vrPoseState->leftAimRot[0], g_vrPoseState->leftAimRot[1],
                g_vrPoseState->leftAimRot[2], g_vrPoseState->leftAimRot[3] });
            pose.leftAimPos = {
                g_vrPoseState->leftAimPos[0], g_vrPoseState->leftAimPos[1], g_vrPoseState->leftAimPos[2] };
            pose.rightAimRot = normalizeQuat({
                g_vrPoseState->rightAimRot[0], g_vrPoseState->rightAimRot[1],
                g_vrPoseState->rightAimRot[2], g_vrPoseState->rightAimRot[3] });
            pose.rightAimPos = {
                g_vrPoseState->rightAimPos[0], g_vrPoseState->rightAimPos[1], g_vrPoseState->rightAimPos[2] };
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            continue;
        }
        MemoryBarrier();
        if (sequenceBefore == g_vrPoseState->sequence
            && (g_vrPoseState->sequence & 1) == 0
            && pose.referenceSpaceGeneration != 0
            && pose.producerEpoch != 0
            && finiteVec3(pose.hmdPos)
            && finiteVec3(pose.leftPos)
            && finiteVec3(pose.rightPos)
            && finiteVec3(pose.leftAimPos)
            && finiteVec3(pose.rightAimPos)
            && finiteUsableQuat(pose.hmdRot)
            && finiteUsableQuat(pose.leftRot)
            && finiteUsableQuat(pose.rightRot)
            && finiteUsableQuat(pose.leftAimRot)
            && finiteUsableQuat(pose.rightAimRot))
        {
            return true;
        }
    }
    return false;
}

bool readAuthoritativeRetailVrOrigin(SharedVrOriginState& origin)
{
    if (!g_vrOriginState)
    {
        g_vrOriginMapping = OpenFileMappingA(
            FILE_MAP_READ,
            FALSE,
            fnvxr::shared::VrOriginSharedMappingName);
        if (!g_vrOriginMapping)
            return false;
        g_vrOriginState = static_cast<SharedVrOriginState*>(MapViewOfFile(
            g_vrOriginMapping,
            FILE_MAP_READ,
            0,
            0,
            sizeof(SharedVrOriginState)));
        if (!g_vrOriginState)
        {
            CloseHandle(g_vrOriginMapping);
            g_vrOriginMapping = nullptr;
            return false;
        }
        logTelemetry("retailRig authoritative VR origin mapped state=%p\n", g_vrOriginState);
    }

    for (int attempt = 0; attempt < 4; ++attempt)
    {
        const LONG sequenceBefore = g_vrOriginState->sequence;
        if ((sequenceBefore & 1) != 0)
            continue;
        MemoryBarrier();
        __try
        {
            std::memcpy(&origin, g_vrOriginState, sizeof(origin));
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            continue;
        }
        MemoryBarrier();
        Matrix33 renderCameraWorldRotation {};
        std::memcpy(
            &renderCameraWorldRotation.m[0][0],
            origin.renderCameraWorldRot,
            sizeof(origin.renderCameraWorldRot));
        const Vec3 renderCameraWorldPosition {
            origin.renderCameraWorldPos[0],
            origin.renderCameraWorldPos[1],
            origin.renderCameraWorldPos[2]
        };
        if (sequenceBefore == g_vrOriginState->sequence
            && (g_vrOriginState->sequence & 1) == 0
            && origin.magic == fnvxr::shared::VrOriginSharedMagic
            && origin.version == fnvxr::shared::VrOriginSharedVersion
            && (origin.active == fnvxr::shared::VrOriginStateRenderLease
                || origin.active == fnvxr::shared::VrOriginStateCommitted)
            && origin.generation != 0
            && origin.poseSequence != 0
            && origin.producerEpoch != 0
            && origin.renderPoseSequence != 0
            && origin.renderPoseFrame != 0
            && origin.renderedDisplayTime != 0
            && origin.renderCameraWorldValid != 0
            && origin.renderCameraAddress >= 0x01000000u
            && looksLikeNiObject(reinterpret_cast<void*>(origin.renderCameraAddress))
            && finiteMatrix33(renderCameraWorldRotation)
            && finiteVec3(renderCameraWorldPosition)
            && finiteUsableQuat({
                origin.originRot[0],
                origin.originRot[1],
                origin.originRot[2],
                origin.originRot[3] })
            && finiteVec3({
                origin.originPos[0],
                origin.originPos[1],
                origin.originPos[2] }))
        {
            return true;
        }
    }
    return false;
}

bool readCoherentRetailRigPoseAndOrigin(
    VrRigPoseSnapshot& pose,
    SharedVrOriginState& origin)
{
    SharedVrOriginState originBefore {};
    SharedVrOriginState originAfter {};
    if (!readAuthoritativeRetailVrOrigin(originBefore)
        || !readLatestRetailRigPose(pose)
        || !readAuthoritativeRetailVrOrigin(originAfter))
    {
        return false;
    }

    if (originBefore.sequence != originAfter.sequence
        || originBefore.generation != originAfter.generation
        || originBefore.poseSequence != originAfter.poseSequence
        || originBefore.poseFrame != originAfter.poseFrame
        || originBefore.renderPoseSequence != originAfter.renderPoseSequence
        || originBefore.renderPoseFrame != originAfter.renderPoseFrame
        || originBefore.renderedDisplayTime != originAfter.renderedDisplayTime
        || pose.referenceSpaceGeneration != originBefore.generation
        || pose.producerEpoch != originBefore.producerEpoch)
    {
        return false;
    }

    // The host can publish a newer controller pose while Fallout is still in
    // the same (comparatively long) render traversal. Requiring an identical
    // shared-memory sequence/frame/time here made the arm hook miss every
    // transaction. The render camera and recenter origin remain locked by the
    // stable originBefore/originAfter transaction above; hands may consume the
    // newest pose from that same OpenXR epoch and reference-space generation.
    const std::int64_t displayTimeDelta = pose.predictedDisplayTime
        - originBefore.renderedDisplayTime;
    const std::int64_t absoluteDisplayTimeDelta = displayTimeDelta >= 0
        ? displayTimeDelta
        : -displayTimeDelta;
    const double maximumPoseSkewMilliseconds = (std::max)(
        1.0,
        static_cast<double>(getFloatFromEnv(
            "FNVXR_RETAIL_RIG_MAX_RENDER_POSE_SKEW_MS",
            250.0f)));
    const std::int64_t maximumPoseSkewNanoseconds = static_cast<std::int64_t>(
        maximumPoseSkewMilliseconds * 1000000.0);
    if (absoluteDisplayTimeDelta > maximumPoseSkewNanoseconds)
        return false;

    if (static_cast<std::uint32_t>(pose.sequence) != originBefore.renderPoseSequence
        || pose.frame != originBefore.renderPoseFrame)
    {
        const LONG count = InterlockedIncrement(&g_retailRigPoseOriginSkewCount);
        if (count <= 12 || count % 300 == 0)
        {
            logTelemetry(
                "retailRig pose handoff count=%ld latestSeq=%ld renderSeq=%lu latestFrame=%llu renderFrame=%llu skewMs=%.3f generation=%lu epoch=%lu\n",
                count,
                pose.sequence,
                static_cast<unsigned long>(originBefore.renderPoseSequence),
                static_cast<unsigned long long>(pose.frame),
                static_cast<unsigned long long>(originBefore.renderPoseFrame),
                static_cast<double>(displayTimeDelta) / 1000000.0,
                static_cast<unsigned long>(pose.referenceSpaceGeneration),
                static_cast<unsigned long>(pose.producerEpoch));
        }
    }

    origin = originBefore;
    return true;
}

bool readPublishedRetailCameraWorld(Vec3& cameraWorld)
{
    if (!g_cameraState
        || g_cameraState->magic != CameraSharedMagic
        || g_cameraState->version != CameraSharedVersion)
    {
        return false;
    }

    for (int attempt = 0; attempt < 4; ++attempt)
    {
        const LONG sequenceBefore = g_cameraState->sequence;
        if ((sequenceBefore & 1) != 0)
            continue;
        MemoryBarrier();
        UInt32 active = 0;
        __try
        {
            active = g_cameraState->active;
            cameraWorld = {
                g_cameraState->worldPos[0],
                g_cameraState->worldPos[1],
                g_cameraState->worldPos[2]
            };
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            continue;
        }
        MemoryBarrier();
        if (sequenceBefore == g_cameraState->sequence
            && (g_cameraState->sequence & 1) == 0
            && active != 0
            && finiteVec3(cameraWorld))
        {
            return true;
        }
    }
    return false;
}

bool captureRetailRigOrigin(
    const VrRigPoseSnapshot& pose,
    const SharedVrOriginState& authoritativeOrigin,
    void* bodyRoot,
    const Matrix33& bodyWorldRotation,
    Vec3 bodyWorldPosition,
    Vec3 stableCameraWorld)
{
    if (!bodyRoot
        || !finiteMatrix33(bodyWorldRotation)
        || !finiteVec3(bodyWorldPosition)
        || !finiteVec3(stableCameraWorld))
    {
        return false;
    }

    if (authoritativeOrigin.generation != pose.referenceSpaceGeneration
        || authoritativeOrigin.producerEpoch != pose.producerEpoch)
    {
        return false;
    }
    g_retailRigOriginHmdRot = normalizeQuat({
        authoritativeOrigin.originRot[0],
        authoritativeOrigin.originRot[1],
        authoritativeOrigin.originRot[2],
        authoritativeOrigin.originRot[3]
    });
    g_retailRigOriginHmdPos = {
        authoritativeOrigin.originPos[0],
        authoritativeOrigin.originPos[1],
        authoritativeOrigin.originPos[2]
    };
    g_retailRigReferenceSpaceGeneration = authoritativeOrigin.generation;
    g_retailRigProducerEpoch = authoritativeOrigin.producerEpoch;
    g_retailRigOriginPoseSequence = authoritativeOrigin.poseSequence;
    g_retailRigOriginAuthoritySequence = authoritativeOrigin.sequence;
    g_retailRigOriginBodyRoot = bodyRoot;
    g_retailRigBodyAnchorLocal = transformVec3(
        transposeMatrix33(bodyWorldRotation),
        subtractVec3(stableCameraWorld, bodyWorldPosition));
    if (!finiteVec3(g_retailRigBodyAnchorLocal))
        return false;
    g_haveRetailRigOrigin = true;
    logTelemetry(
        "retailRig origin latched seq=%ld frame=%llu authorityPoseSeq=%lu authorityPoseFrame=%llu referenceGeneration=%lu hmdPos=(%.4f %.4f %.4f) yawOriginRot=(%.5f %.5f %.5f %.5f) renderCamera=0x%08lx bodyRoot=%p bodyAnchorLocal=(%.3f %.3f %.3f) anchorSource=exact-d3d9-render-camera originSource=d3d9-native-camera gravityAlignedOrigin=1\n",
        pose.sequence,
        static_cast<unsigned long long>(pose.frame),
        static_cast<unsigned long>(authoritativeOrigin.poseSequence),
        static_cast<unsigned long long>(authoritativeOrigin.poseFrame),
        static_cast<unsigned long>(authoritativeOrigin.generation),
        g_retailRigOriginHmdPos.x,
        g_retailRigOriginHmdPos.y,
        g_retailRigOriginHmdPos.z,
        g_retailRigOriginHmdRot.x,
        g_retailRigOriginHmdRot.y,
        g_retailRigOriginHmdRot.z,
        g_retailRigOriginHmdRot.w,
        static_cast<unsigned long>(authoritativeOrigin.renderCameraAddress),
        bodyRoot,
        g_retailRigBodyAnchorLocal.x,
        g_retailRigBodyAnchorLocal.y,
        g_retailRigBodyAnchorLocal.z);
    return true;
}

bool retailRigNodesComplete(const RetailRigNodes& rig)
{
    return rig.root
        && rig.left.clavicle && rig.left.upperArm && rig.left.forearm && rig.left.hand
        && rig.right.clavicle && rig.right.upperArm && rig.right.forearm && rig.right.hand
        && rig.weapon
        && niObjectDescendsFrom(rig.left.upperArm, rig.left.clavicle)
        && niObjectDescendsFrom(rig.left.forearm, rig.left.upperArm)
        && niObjectDescendsFrom(rig.left.hand, rig.left.forearm)
        && niObjectDescendsFrom(rig.right.upperArm, rig.right.clavicle)
        && niObjectDescendsFrom(rig.right.forearm, rig.right.upperArm)
        && niObjectDescendsFrom(rig.right.hand, rig.right.forearm)
        && niObjectDescendsFrom(rig.weapon, rig.root);
}

RetailArmNodes discoverRetailArm(void* root, bool left)
{
    RetailArmNodes arm {};
    const char* side = left ? "L" : "R";
    char name[64] {};
    sprintf_s(name, "Bip01 %s Clavicle", side);
    arm.clavicle = findUniqueNiNode(root, name);
    sprintf_s(name, "Bip01 %s UpperArm", side);
    arm.upperArm = findUniqueNiNode(root, name);
    sprintf_s(name, "Bip01 %s Forearm", side);
    arm.forearm = findUniqueNiNode(root, name);
    sprintf_s(name, "Bip01 %s Hand", side);
    arm.hand = findUniqueNiNode(root, name);
    return arm;
}

bool discoverRetailRigNodes(void* root)
{
    if (!root)
        return false;
    RetailRigNodes discovered {};
    discovered.root = root;
    discovered.left = discoverRetailArm(root, true);
    discovered.right = discoverRetailArm(root, false);
    UInt32 weaponMatches = 0;
    UInt32 projectileMatches = 0;
    UInt32 muzzleMatches = 0;
    discovered.weapon = findUniqueNiNode(root, "Weapon", &weaponMatches);
    discovered.projectileNode = findUniqueNiNode(root, "ProjectileNode", &projectileMatches);
    discovered.muzzleFlash = findUniqueNiNode(root, "MuzzleFlash", &muzzleMatches);
    g_retailRigNodes = discovered;
    g_retailLeftCalibration = {};
    g_retailRightCalibration = {};
    g_retailWeaponCalibration = {};
    ++g_retailRigDiscoveryCount;

    logTelemetry(
        "retailRig discovery count=%llu root=%p left=(clav=%p upper=%p fore=%p hand=%p) right=(clav=%p upper=%p fore=%p hand=%p) weapon=%p weaponMatches=%lu projectile=%p projectileMatches=%lu muzzleFlash=%p muzzleMatches=%lu complete=%d ancestry=validated\n",
        static_cast<unsigned long long>(g_retailRigDiscoveryCount),
        root,
        discovered.left.clavicle,
        discovered.left.upperArm,
        discovered.left.forearm,
        discovered.left.hand,
        discovered.right.clavicle,
        discovered.right.upperArm,
        discovered.right.forearm,
        discovered.right.hand,
        discovered.weapon,
        static_cast<unsigned long>(weaponMatches),
        discovered.projectileNode,
        static_cast<unsigned long>(projectileMatches),
        discovered.muzzleFlash,
        static_cast<unsigned long>(muzzleMatches),
        retailRigNodesComplete(discovered) ? 1 : 0);

    if (envEnabled("FNVXR_RETAIL_RIG_DUMP_NODES", true)
        && (g_retailRigDiscoveryCount == 1 || !retailRigNodesComplete(discovered)))
    {
        UInt32 visits = 0;
        dumpNiNodesRecursive(root, 0, visits);
        logTelemetry("retailRig node dump complete visits=%lu\n", static_cast<unsigned long>(visits));
    }
    return retailRigNodesComplete(discovered);
}

void refreshRetailWeaponNodes()
{
    if (!g_retailRigNodes.root)
        return;
    void* previousWeapon = g_retailRigNodes.weapon;
    g_retailRigNodes.weapon = findUniqueNiNode(g_retailRigNodes.root, "Weapon");
    if (g_retailRigNodes.weapon != previousWeapon)
    {
        g_retailWeaponCalibration = {};
        logTelemetry(
            "retailWeapon node changed previous=%p current=%p calibrationReset=1\n",
            previousWeapon,
            g_retailRigNodes.weapon);
    }
    void* player = readPointer(PlayerCharacterAddress);
    void* process = player
        ? readPointer(reinterpret_cast<std::uintptr_t>(player) + MobileObjectBaseProcessOffset)
        : nullptr;
    g_retailRigNodes.projectileNode = process
        ? readPointer(reinterpret_cast<std::uintptr_t>(process) + MiddleHighProcessProjectileNodeOffset)
        : nullptr;
    if (!g_retailRigNodes.projectileNode)
        g_retailRigNodes.projectileNode = findUniqueNiNode(g_retailRigNodes.root, "ProjectileNode");
    g_retailRigNodes.muzzleFlash = findUniqueNiNode(g_retailRigNodes.root, "MuzzleFlash");
}

void forwardKinematics(void* node, int depth, UInt32& visits)
{
    if (!node || depth > 64 || ++visits > 4096)
        return;
    const int kind = niObjectKind(node);
    if (kind == 0)
        return;

    const auto base = reinterpret_cast<std::uintptr_t>(node);
    void* parent = readPointer(base + NiAvObjectParentOffset);
    const Matrix33 localRotation = readMatrix33(base + NiAvObjectLocalRotationOffset);
    const Vec3 localTranslation = readVec3(base + NiAvObjectLocalTranslationOffset);
    const float localScale = readFloat(base + NiAvObjectLocalScaleOffset, 1.0f);
    if (parent)
    {
        const auto parentBase = reinterpret_cast<std::uintptr_t>(parent);
        const Matrix33 parentRotation = readMatrix33(parentBase + NiAvObjectWorldRotationOffset);
        const Vec3 parentTranslation = readVec3(parentBase + NiAvObjectWorldTranslationOffset);
        const float parentScale = readFloat(parentBase + NiAvObjectWorldScaleOffset, 1.0f);
        const Vec3 translated = transformVec3(parentRotation, localTranslation);
        writeVec3(
            base + NiAvObjectWorldTranslationOffset,
            addVec3(parentTranslation, scaleVec3(translated, parentScale)));
        writeMatrix33(
            base + NiAvObjectWorldRotationOffset,
            multiplyMatrix33(parentRotation, localRotation));
        writeFloat(base + NiAvObjectWorldScaleOffset, parentScale * localScale);
    }
    else
    {
        writeVec3(base + NiAvObjectWorldTranslationOffset, localTranslation);
        writeMatrix33(base + NiAvObjectWorldRotationOffset, localRotation);
        writeFloat(base + NiAvObjectWorldScaleOffset, localScale);
    }

    if (kind != 2)
        return;
    void** children = nullptr;
    UInt16 count = 0;
    if (!readNiNodeChildren(node, &children, count))
        return;
    for (UInt16 index = 0; index < count; ++index)
    {
        void* child = nullptr;
        __try
        {
            child = children[index];
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            child = nullptr;
        }
        forwardKinematics(child, depth + 1, visits);
    }
}

void forwardKinematics(void* node)
{
    UInt32 visits = 0;
    forwardKinematics(node, 0, visits);
}

bool alignBoneToDirection(void* bone, void* child, Vec3 desiredDirection)
{
    if (!bone || !child || lengthVec3(desiredDirection) < 0.0001f)
        return false;
    const auto boneBase = reinterpret_cast<std::uintptr_t>(bone);
    const Vec3 boneWorld = readVec3(boneBase + NiAvObjectWorldTranslationOffset);
    const Vec3 childWorld = readVec3(
        reinterpret_cast<std::uintptr_t>(child) + NiAvObjectWorldTranslationOffset);
    const Vec3 currentDirection = subtractVec3(childWorld, boneWorld);
    if (lengthVec3(currentDirection) < 0.0001f)
        return false;

    const Matrix33 currentWorldRotation = readMatrix33(boneBase + NiAvObjectWorldRotationOffset);
    const Matrix33 directionDelta = rotationFromTo(currentDirection, desiredDirection);
    const Matrix33 desiredWorldRotation = multiplyMatrix33(directionDelta, currentWorldRotation);
    void* parent = readPointer(boneBase + NiAvObjectParentOffset);
    const Matrix33 parentWorldRotation = parent
        ? readMatrix33(reinterpret_cast<std::uintptr_t>(parent) + NiAvObjectWorldRotationOffset)
        : Matrix33 { { { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } } };
    const Matrix33 desiredLocalRotation = multiplyMatrix33(
        transposeMatrix33(parentWorldRotation),
        desiredWorldRotation);
    if (!finiteMatrix33(desiredLocalRotation))
        return false;
    if (!writeMatrix33(boneBase + NiAvObjectLocalRotationOffset, desiredLocalRotation))
        return false;
    forwardKinematics(bone);
    return true;
}

struct RetailControllerWorldPose
{
    Vec3 position {};
    Vec3 wristPosition {};
    Vec3 originLocalMeters {};
    Vec3 originLocalWristMeters {};
    Vec3 bodyLocalGameUnits {};
    Vec3 wristBodyLocalGameUnits {};
    Quat originLocalWristRotation { 0.0f, 0.0f, 0.0f, 1.0f };
    Quat originLocalRotation { 0.0f, 0.0f, 0.0f, 1.0f };
    Matrix33 wristRotation {};
    Matrix33 rotation {};
    bool usesAimOrientation {};
};

RetailControllerWorldPose retailControllerWorldPose(
    const VrRigPoseSnapshot& pose,
    bool left,
    Vec3 bodyAnchorWorld,
    const Matrix33& bodyWorldRotation)
{
    const bool rightAimTracked = !left
        && (pose.trackingFlags & fnvxr::shared::VrPoseTrackingRightAimActive) != 0
        && (pose.trackingFlags & fnvxr::shared::VrPoseTrackingRightAimCurrent) != 0;
    const Vec3 wristPosition = left ? pose.leftPos : pose.rightPos;
    const Vec3 controllerPosition = rightAimTracked ? pose.rightAimPos : wristPosition;
    const Quat gripRotation = left ? pose.leftRot : pose.rightRot;
    const Quat controllerRotation = left
        ? pose.leftRot
        : (rightAimTracked ? pose.rightAimRot : pose.rightRot);
    const Vec3 localMeters = xrPositionInOriginFrame(
        g_retailRigOriginHmdRot,
        g_retailRigOriginHmdPos,
        controllerPosition);
    const Vec3 wristLocalMeters = xrPositionInOriginFrame(
        g_retailRigOriginHmdRot,
        g_retailRigOriginHmdPos,
        wristPosition);
    Vec3 localGame = xrDeltaToGamebryoVector(localMeters);
    Vec3 wristLocalGame = xrDeltaToGamebryoVector(wristLocalMeters);
    localGame.x *= getFloatFromEnv("FNVXR_D3D9_POSE_X_SIGN", 1.0f);
    localGame.y *= getFloatFromEnv("FNVXR_D3D9_POSE_Y_SIGN", 1.0f);
    localGame.z *= getFloatFromEnv("FNVXR_D3D9_POSE_Z_SIGN", 1.0f);
    wristLocalGame.x *= getFloatFromEnv("FNVXR_D3D9_POSE_X_SIGN", 1.0f);
    wristLocalGame.y *= getFloatFromEnv("FNVXR_D3D9_POSE_Y_SIGN", 1.0f);
    wristLocalGame.z *= getFloatFromEnv("FNVXR_D3D9_POSE_Z_SIGN", 1.0f);
    const float unitsPerMeter = getFloatFromEnv("FNVXR_D3D9_GAME_UNITS_PER_METER", 39.3701f);
    const float positionScale = getFloatFromEnv("FNVXR_RETAIL_RIG_POSITION_SCALE", 1.0f);
    const Quat recenteredGripRotation = multiplyQuat(
        conjugateQuat(g_retailRigOriginHmdRot),
        gripRotation);
    const Quat recenteredControllerRotation = multiplyQuat(
        conjugateQuat(g_retailRigOriginHmdRot),
        controllerRotation);

    RetailControllerWorldPose result {};
    result.originLocalMeters = localMeters;
    result.originLocalWristMeters = wristLocalMeters;
    result.bodyLocalGameUnits = scaleVec3(localGame, unitsPerMeter * positionScale);
    result.wristBodyLocalGameUnits = scaleVec3(wristLocalGame, unitsPerMeter * positionScale);
    result.originLocalWristRotation = recenteredGripRotation;
    result.originLocalRotation = recenteredControllerRotation;
    result.position = addVec3(
        bodyAnchorWorld,
        transformVec3(bodyWorldRotation, result.bodyLocalGameUnits));
    result.wristPosition = addVec3(
        bodyAnchorWorld,
        transformVec3(bodyWorldRotation, result.wristBodyLocalGameUnits));
    result.wristRotation = multiplyMatrix33(
        bodyWorldRotation,
        xrDeltaToGamebryoMatrix(recenteredGripRotation));
    result.rotation = multiplyMatrix33(
        bodyWorldRotation,
        xrDeltaToGamebryoMatrix(recenteredControllerRotation));
    result.usesAimOrientation = rightAimTracked;
    return result;
}

Vec3 configuredControllerToWristOffset(bool left)
{
    const char* prefix = left ? "FNVXR_RETAIL_RIG_LEFT_WRIST_OFFSET_" : "FNVXR_RETAIL_RIG_RIGHT_WRIST_OFFSET_";
    char name[96] {};
    sprintf_s(name, "%sX", prefix);
    const float x = getFloatFromEnv(name, 0.0f);
    sprintf_s(name, "%sY", prefix);
    const float y = getFloatFromEnv(name, 0.0f);
    sprintf_s(name, "%sZ", prefix);
    const float z = getFloatFromEnv(name, 0.0f);
    return { x, y, z };
}

void ensureRetailHandCalibration(
    RetailHandCalibration& calibration,
    const RetailArmNodes& arm,
    const RetailControllerWorldPose& controller,
    bool left)
{
    if (calibration.valid)
        return;
    if (!arm.hand)
        return;
    calibration = {};
    const auto handBase = reinterpret_cast<std::uintptr_t>(arm.hand);
    const Matrix33 handWorldRotation = readMatrix33(handBase + NiAvObjectWorldRotationOffset);
    const Vec3 handWorldPosition = readVec3(handBase + NiAvObjectWorldTranslationOffset);
    calibration.controllerToHandRotation = multiplyMatrix33(
        transposeMatrix33(controller.wristRotation),
        handWorldRotation);
    calibration.usesAimOrientation = false;
    calibration.controllerToWristLocal = configuredControllerToWristOffset(left);
    if (envEnabled("FNVXR_RETAIL_RIG_AUTO_CALIBRATE_POSITION", false))
    {
        const Vec3 measured = transformVec3(
            transposeMatrix33(controller.wristRotation),
            subtractVec3(handWorldPosition, controller.wristPosition));
        const float maxOffset = getFloatFromEnv("FNVXR_RETAIL_RIG_MAX_AUTO_CALIBRATION_UNITS", 12.0f);
        if (lengthVec3(measured) <= maxOffset)
            calibration.controllerToWristLocal = measured;
    }
    calibration.valid = finiteMatrix33(calibration.controllerToHandRotation)
        && finiteVec3(calibration.controllerToWristLocal);
    logTelemetry(
        "retailRig calibration side=%s valid=%d orientationSource=%s wristLocal=(%.3f %.3f %.3f) autoPosition=%d\n",
        left ? "left" : "right",
        calibration.valid ? 1 : 0,
        calibration.usesAimOrientation ? "aim" : "grip",
        calibration.controllerToWristLocal.x,
        calibration.controllerToWristLocal.y,
        calibration.controllerToWristLocal.z,
        envEnabled("FNVXR_RETAIL_RIG_AUTO_CALIBRATE_POSITION", false) ? 1 : 0);
}

bool applyRetailArmFabrik(
    const RetailArmNodes& arm,
    RetailHandCalibration& calibration,
    const RetailControllerWorldPose& controller,
    const Matrix33& bodyWorldRotation,
    bool left,
    bool applyWrites,
    float& finalError)
{
    finalError = 0.0f;
    if (!arm.upperArm || !arm.forearm || !arm.hand)
        return false;
    ensureRetailHandCalibration(calibration, arm, controller, left);
    if (!calibration.valid)
        return false;

    const Vec3 shoulder = readVec3(
        reinterpret_cast<std::uintptr_t>(arm.upperArm) + NiAvObjectWorldTranslationOffset);
    const Vec3 elbow = readVec3(
        reinterpret_cast<std::uintptr_t>(arm.forearm) + NiAvObjectWorldTranslationOffset);
    const Vec3 wrist = readVec3(
        reinterpret_cast<std::uintptr_t>(arm.hand) + NiAvObjectWorldTranslationOffset);
    const float lengths[2] {
        lengthVec3(subtractVec3(elbow, shoulder)),
        lengthVec3(subtractVec3(wrist, elbow))
    };
    const float maxSegment = getFloatFromEnv("FNVXR_RETAIL_RIG_MAX_SEGMENT_UNITS", 80.0f);
    if (lengths[0] < 0.01f || lengths[1] < 0.01f
        || lengths[0] > maxSegment || lengths[1] > maxSegment)
    {
        return false;
    }

    const Vec3 target = addVec3(
        controller.wristPosition,
        transformVec3(controller.wristRotation, calibration.controllerToWristLocal));
    const float poleOut = getFloatFromEnv("FNVXR_RETAIL_RIG_ELBOW_POLE_OUT", 20.0f) * (left ? -1.0f : 1.0f);
    const Vec3 poleLocal {
        poleOut,
        getFloatFromEnv("FNVXR_RETAIL_RIG_ELBOW_POLE_FORWARD", -15.0f),
        getFloatFromEnv("FNVXR_RETAIL_RIG_ELBOW_POLE_UP", -25.0f)
    };
    const Vec3 pole = addVec3(shoulder, transformVec3(bodyWorldRotation, poleLocal));
    const float shoulderTargetDistance = lengthVec3(subtractVec3(target, shoulder));
    const float maximumReach = lengths[0] + lengths[1];
    const float minimumReach = std::fabs(lengths[0] - lengths[1]);
    const float reachTolerance = getFloatFromEnv("FNVXR_RETAIL_RIG_REACH_TOLERANCE", 0.10f);
    if (shoulderTargetDistance > maximumReach + reachTolerance
        || shoulderTargetDistance < minimumReach - reachTolerance)
    {
        return false;
    }

    fnvxr::ik::Vec3 joints[3] {
        { shoulder.x, shoulder.y, shoulder.z },
        { elbow.x, elbow.y, elbow.z },
        { wrist.x, wrist.y, wrist.z }
    };
    fnvxr::ik::SolveOptions options {};
    options.maxIterations = static_cast<int>(getFloatFromEnv("FNVXR_RETAIL_RIG_FABRIK_ITERATIONS", 12.0f));
    options.tolerance = getFloatFromEnv("FNVXR_RETAIL_RIG_FABRIK_TOLERANCE", 0.05f);
    options.poleWeight = getFloatFromEnv("FNVXR_RETAIL_RIG_ELBOW_POLE_WEIGHT", 1.0f);
    const auto result = fnvxr::ik::solveFabrik(
        joints,
        3,
        lengths,
        { target.x, target.y, target.z },
        { pole.x, pole.y, pole.z },
        options);
    const float maximumFinalError = getFloatFromEnv(
        "FNVXR_RETAIL_RIG_MAX_FINAL_ERROR_UNITS",
        0.25f);
    if (!result.solved || !std::isfinite(result.error) || result.error > maximumFinalError)
        return false;

    finalError = result.error;
    if (!applyWrites)
        return true;

    const Vec3 solvedShoulder { joints[0].x, joints[0].y, joints[0].z };
    const Vec3 solvedElbow { joints[1].x, joints[1].y, joints[1].z };
    const Vec3 solvedWrist { joints[2].x, joints[2].y, joints[2].z };
    if (!alignBoneToDirection(arm.upperArm, arm.forearm, subtractVec3(solvedElbow, solvedShoulder)))
        return false;
    if (!alignBoneToDirection(arm.forearm, arm.hand, subtractVec3(solvedWrist, solvedElbow)))
        return false;

    void* handParent = readPointer(
        reinterpret_cast<std::uintptr_t>(arm.hand) + NiAvObjectParentOffset);
    const Matrix33 parentWorldRotation = handParent
        ? readMatrix33(reinterpret_cast<std::uintptr_t>(handParent) + NiAvObjectWorldRotationOffset)
        : Matrix33 { { { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } } };
    const Matrix33 desiredHandWorldRotation = multiplyMatrix33(
        controller.wristRotation,
        calibration.controllerToHandRotation);
    const Matrix33 desiredHandLocalRotation = multiplyMatrix33(
        transposeMatrix33(parentWorldRotation),
        desiredHandWorldRotation);
    if (!finiteMatrix33(desiredHandLocalRotation))
        return false;
    if (!writeMatrix33(
            reinterpret_cast<std::uintptr_t>(arm.hand) + NiAvObjectLocalRotationOffset,
            desiredHandLocalRotation))
    {
        return false;
    }
    forwardKinematics(arm.hand);

    const Vec3 appliedWrist = readVec3(
        reinterpret_cast<std::uintptr_t>(arm.hand) + NiAvObjectWorldTranslationOffset);
    finalError = lengthVec3(subtractVec3(appliedWrist, target));
    return std::isfinite(finalError) && finalError <= maximumFinalError;
}

struct RetailWeaponApplyResult
{
    bool targetValid {};
    bool writeRequested {};
    bool writeAttempted {};
    bool writeCommitted {};
    bool writeVerified {};
    bool endpointMeasured {};
    bool endpointInWeaponBranch {};
    float positionResidualUnits {};
    float angularResidualRadians {};
    float endpointAimResidualRadians {};
    Vec3 desiredWorldPosition {};
    Vec3 actualWorldPosition {};
    Vec3 endpointWorldPosition {};
    Vec3 aimForward {};
    Vec3 endpointForward {};
    Matrix33 desiredWorldRotation {};
    Matrix33 actualWorldRotation {};
};

RetailWeaponApplyResult applyRetailWeaponAim(
    const RetailControllerWorldPose& controller,
    bool applyWrites)
{
    RetailWeaponApplyResult result {};
    void* weapon = g_retailRigNodes.weapon;
    if (!weapon || !controller.usesAimOrientation || niObjectKind(weapon) == 0)
        return result;

    const auto weaponBase = reinterpret_cast<std::uintptr_t>(weapon);
    const Matrix33 weaponWorldRotation = readMatrix33(weaponBase + NiAvObjectWorldRotationOffset);
    const Vec3 weaponWorldPosition = readVec3(weaponBase + NiAvObjectWorldTranslationOffset);
    if (!finiteMatrix33(weaponWorldRotation) || !finiteVec3(weaponWorldPosition)
        || !finiteMatrix33(controller.rotation) || !finiteVec3(controller.position))
        return result;

    if (!g_retailWeaponCalibration.valid)
    {
        g_retailWeaponCalibration.controllerToWeaponRotation = multiplyMatrix33(
            transposeMatrix33(controller.rotation),
            weaponWorldRotation);
        g_retailWeaponCalibration.controllerToWeaponPosition = transformVec3(
            transposeMatrix33(controller.rotation),
            subtractVec3(weaponWorldPosition, controller.position));
        g_retailWeaponCalibration.valid = finiteMatrix33(
                g_retailWeaponCalibration.controllerToWeaponRotation)
            && finiteVec3(g_retailWeaponCalibration.controllerToWeaponPosition)
            && lengthVec3(g_retailWeaponCalibration.controllerToWeaponPosition)
                <= getFloatFromEnv("FNVXR_RETAIL_WEAPON_MAX_CALIBRATION_UNITS", 48.0f);
        logTelemetry(
            "retailWeapon calibration valid=%d source=right-aim fullSE3=1 weapon=%p localPosition=(%.4f %.4f %.4f)\n",
            g_retailWeaponCalibration.valid ? 1 : 0,
            weapon,
            g_retailWeaponCalibration.controllerToWeaponPosition.x,
            g_retailWeaponCalibration.controllerToWeaponPosition.y,
            g_retailWeaponCalibration.controllerToWeaponPosition.z);
    }
    if (!g_retailWeaponCalibration.valid)
        return result;

    const Matrix33 desiredWorldRotation = multiplyMatrix33(
        controller.rotation,
        g_retailWeaponCalibration.controllerToWeaponRotation);
    const Vec3 desiredWorldPosition = addVec3(
        controller.position,
        transformVec3(
            controller.rotation,
            g_retailWeaponCalibration.controllerToWeaponPosition));
    void* parent = readPointer(weaponBase + NiAvObjectParentOffset);
    if (!parent || !finiteMatrix33(desiredWorldRotation) || !finiteVec3(desiredWorldPosition))
        return result;
    const Matrix33 parentWorldRotation = readMatrix33(
        reinterpret_cast<std::uintptr_t>(parent) + NiAvObjectWorldRotationOffset);
    const Vec3 parentWorldPosition = readVec3(
        reinterpret_cast<std::uintptr_t>(parent) + NiAvObjectWorldTranslationOffset);
    const float parentWorldScale = readFloat(
        reinterpret_cast<std::uintptr_t>(parent) + NiAvObjectWorldScaleOffset,
        1.0f);
    if (!finiteMatrix33(parentWorldRotation) || !finiteVec3(parentWorldPosition)
        || !std::isfinite(parentWorldScale) || std::fabs(parentWorldScale) < 0.0001f)
        return result;
    const Matrix33 desiredLocalRotation = multiplyMatrix33(
        transposeMatrix33(parentWorldRotation),
        desiredWorldRotation);
    if (!finiteMatrix33(desiredLocalRotation))
        return result;
    Vec3 desiredLocalPosition = transformVec3(
        transposeMatrix33(parentWorldRotation),
        subtractVec3(desiredWorldPosition, parentWorldPosition));
    desiredLocalPosition = scaleVec3(desiredLocalPosition, 1.0f / parentWorldScale);
    if (!finiteVec3(desiredLocalPosition))
        return result;

    result.targetValid = true;
    result.desiredWorldPosition = desiredWorldPosition;
    result.desiredWorldRotation = desiredWorldRotation;
    result.writeRequested = applyWrites && envEnabled("FNVXR_RETAIL_WEAPON_APPLY", false);
    if (result.writeRequested)
    {
        result.writeAttempted = true;
        const bool positionWritten = writeVec3(
            weaponBase + NiAvObjectLocalTranslationOffset,
            desiredLocalPosition);
        const bool rotationWritten = writeMatrix33(
            weaponBase + NiAvObjectLocalRotationOffset,
            desiredLocalRotation);
        result.writeCommitted = positionWritten && rotationWritten;
        if (result.writeCommitted)
            forwardKinematics(weapon);
    }
    result.actualWorldRotation = readMatrix33(weaponBase + NiAvObjectWorldRotationOffset);
    result.actualWorldPosition = readVec3(weaponBase + NiAvObjectWorldTranslationOffset);
    result.positionResidualUnits = finiteVec3(result.actualWorldPosition)
        ? lengthVec3(subtractVec3(result.desiredWorldPosition, result.actualWorldPosition))
        : 1000000.0f;
    result.angularResidualRadians = finiteMatrix33(result.actualWorldRotation)
        ? matrixAngularDistance(result.desiredWorldRotation, result.actualWorldRotation)
        : 3.14159265f;
    result.writeVerified = result.writeRequested
        && result.writeCommitted
        && std::isfinite(result.positionResidualUnits)
        && result.positionResidualUnits <= getFloatFromEnv(
            "FNVXR_RETAIL_WEAPON_MAX_WRITE_RESIDUAL_UNITS",
            0.25f)
        && std::isfinite(result.angularResidualRadians)
        && result.angularResidualRadians <= getFloatFromEnv(
            "FNVXR_RETAIL_WEAPON_MAX_WRITE_RESIDUAL_RADIANS",
            0.01f);

    void* endpoint = g_retailRigNodes.projectileNode
        ? g_retailRigNodes.projectileNode
        : g_retailRigNodes.muzzleFlash;
    if (endpoint && niObjectKind(endpoint) != 0)
    {
        result.endpointInWeaponBranch = niObjectDescendsFrom(endpoint, weapon);
        const auto endpointBase = reinterpret_cast<std::uintptr_t>(endpoint);
        const Matrix33 endpointRotation = readMatrix33(
            endpointBase + NiAvObjectWorldRotationOffset);
        result.endpointWorldPosition = readVec3(
            endpointBase + NiAvObjectWorldTranslationOffset);
        if (finiteMatrix33(endpointRotation) && finiteVec3(result.endpointWorldPosition))
        {
            result.aimForward = normalizeVec3(transformVec3(
                controller.rotation,
                { 0.0f, 1.0f, 0.0f }));
            result.endpointForward = normalizeVec3(transformVec3(
                endpointRotation,
                { 0.0f, 1.0f, 0.0f }));
            result.endpointAimResidualRadians = std::acos(std::clamp(
                dotVec3(result.aimForward, result.endpointForward),
                -1.0f,
                1.0f));
            result.endpointMeasured = std::isfinite(result.endpointAimResidualRadians);
        }
    }
    return result;
}

void* __fastcall hookedGetProjectileNode(void* process, void*)
{
    void* node = g_originalGetProjectileNode
        ? g_originalGetProjectileNode(process)
        : nullptr;
    const LONG call = InterlockedIncrement(&g_projectileNodeConsumeCalls);
    void* player = readPointer(PlayerCharacterAddress);
    void* playerProcess = player
        ? readPointer(reinterpret_cast<std::uintptr_t>(player) + MobileObjectBaseProcessOffset)
        : nullptr;
    const bool playerCall = process && process == playerProcess;
    const bool endpointMatches = node && node == g_latestMuzzleProofNode;
    const UInt8 rightTrigger = g_xinputState ? g_xinputState->rightTrigger : 0;
    if (playerCall && (rightTrigger > 64 || call <= 24 || call % 120 == 0))
    {
        Vec3 nodePosition { NAN, NAN, NAN };
        Vec3 nodeForward { NAN, NAN, NAN };
        float residual = 3.14159265f;
        if (node && niObjectKind(node) != 0)
        {
            const auto base = reinterpret_cast<std::uintptr_t>(node);
            const Matrix33 rotation = readMatrix33(base + NiAvObjectWorldRotationOffset);
            nodePosition = readVec3(base + NiAvObjectWorldTranslationOffset);
            if (finiteMatrix33(rotation))
            {
                nodeForward = normalizeVec3(transformVec3(rotation, { 0.0f, 1.0f, 0.0f }));
                residual = std::acos(std::clamp(
                    dotVec3(normalizeVec3(g_latestMuzzleAimForward), nodeForward),
                    -1.0f,
                    1.0f));
            }
        }
        logTelemetry(
            "{\"event\":\"fnvxrProjectileNodeConsume\",\"call\":%ld,\"method\":\"BaseProcess::GetProjectileNode\",\"vtableSlot\":97,\"returnAddress\":\"%p\",\"playerProcess\":%s,\"endpointMatches\":%s,\"poseSeq\":%ld,\"rightTrigger\":%u,\"node\":\"%p\",\"nodeWorld\":[%.6f,%.6f,%.6f],\"nodeForward\":[%.7f,%.7f,%.7f],\"aimResidualRadians\":%.7f}\n",
            call,
            _ReturnAddress(),
            playerCall ? "true" : "false",
            endpointMatches ? "true" : "false",
            g_latestMuzzleProofPoseSequence,
            static_cast<unsigned>(rightTrigger),
            node,
            nodePosition.x,
            nodePosition.y,
            nodePosition.z,
            nodeForward.x,
            nodeForward.y,
            nodeForward.z,
            residual);
    }
    return node;
}

bool installProjectileNodeConsumeHook(void* process)
{
    if (g_projectileNodeHookInstalled)
        return true;
    if (!process || !envEnabled("FNVXR_RETAIL_PROJECTILE_NODE_HOOK", true))
        return false;
    __try
    {
        void** vtable = *reinterpret_cast<void***>(process);
        if (!vtable || !vtable[0x61])
            return false;
        MEMORY_BASIC_INFORMATION memory {};
        if (!VirtualQuery(vtable[0x61], &memory, sizeof(memory))
            || (memory.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ
                | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) == 0)
        {
            return false;
        }
        DWORD oldProtect = 0;
        if (!VirtualProtect(&vtable[0x61], sizeof(void*), PAGE_READWRITE, &oldProtect))
            return false;
        g_originalGetProjectileNode = reinterpret_cast<GetProjectileNodeFn>(vtable[0x61]);
        g_projectileNodeVtable = vtable;
        vtable[0x61] = reinterpret_cast<void*>(hookedGetProjectileNode);
        DWORD ignored = 0;
        VirtualProtect(&vtable[0x61], sizeof(void*), oldProtect, &ignored);
        FlushInstructionCache(GetCurrentProcess(), &vtable[0x61], sizeof(void*));
        g_projectileNodeHookInstalled = true;
        logTelemetry(
            "retailProjectile hook installed process=%p vtable=%p slot=0x61 original=%p hook=%p source=xNVSE-GameProcess\n",
            process,
            vtable,
            reinterpret_cast<void*>(g_originalGetProjectileNode),
            reinterpret_cast<void*>(hookedGetProjectileNode));
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

void restoreProjectileNodeConsumeHook()
{
    if (!g_projectileNodeHookInstalled || !g_projectileNodeVtable || !g_originalGetProjectileNode)
        return;
    __try
    {
        DWORD oldProtect = 0;
        if (VirtualProtect(&g_projectileNodeVtable[0x61], sizeof(void*), PAGE_READWRITE, &oldProtect))
        {
            if (g_projectileNodeVtable[0x61] == reinterpret_cast<void*>(hookedGetProjectileNode))
                g_projectileNodeVtable[0x61] = reinterpret_cast<void*>(g_originalGetProjectileNode);
            DWORD ignored = 0;
            VirtualProtect(&g_projectileNodeVtable[0x61], sizeof(void*), oldProtect, &ignored);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    g_projectileNodeHookInstalled = false;
    g_projectileNodeVtable = nullptr;
    g_originalGetProjectileNode = nullptr;
}

bool retailRigGameplayAllowed()
{
    void* player = readPointer(PlayerCharacterAddress);
    if (!player || readUInt8(
        reinterpret_cast<std::uintptr_t>(player) + PlayerCharacterIsThirdPersonOffset) != 0)
    {
        return false;
    }
    const UInt32 menuBits = currentMenuBits();
    return runtimePhaseFromMenuBits(menuBits) == RuntimePhase::Gameplay
        && (menuBits & fnvxr::shared::RuntimeBlockingMenuBits) == 0;
}

void resetRetailRigOrigin(const char* reason)
{
    if (g_haveRetailRigOrigin)
        logTelemetry("retailRig origin reset reason=%s\n", reason ? reason : "unknown");
    g_haveRetailRigOrigin = false;
    g_retailRigOriginHmdRot = { 0.0f, 0.0f, 0.0f, 1.0f };
    g_retailRigOriginHmdPos = {};
    g_retailRigOriginBodyRoot = nullptr;
    g_retailRigBodyAnchorLocal = {};
    g_retailRigReferenceSpaceGeneration = 0;
    g_retailRigProducerEpoch = 0;
    g_retailRigOriginPoseSequence = 0;
    g_retailRigOriginAuthoritySequence = 0;
    g_lastRetailRigPoseSequence = 0;
    g_haveRetailRigMotionSample = false;
    g_retailRigHeadOnlySamples = 0;
    g_retailRigControllerOnlySamples = 0;
}

void onRetailPostAnimation(void* animData)
{
    if (!envEnabled("FNVXR_RETAIL_RIG_ENABLE", false))
        return;
    if (!retailRigGameplayAllowed())
    {
        resetRetailRigOrigin("not-gameplay");
        return;
    }

    void* player = readPointer(PlayerCharacterAddress);
    void* actor = animData
        ? readPointer(reinterpret_cast<std::uintptr_t>(animData) + 0x04)
        : nullptr;
    if (!animData || actor != player)
        return;
    void* thirdPersonAnimData = currentPlayerThirdPersonAnimData();
    // This call site runs for both player animation sets. Solving the
    // first-person arm a second time from the third-person AnimData produces
    // a small but visible target shift every frame, so this is deliberately
    // not configurable: the retail view-model pass is the sole IK owner.
    if (animData == thirdPersonAnimData)
        return;
    void* playerProcess = player
        ? readPointer(reinterpret_cast<std::uintptr_t>(player) + MobileObjectBaseProcessOffset)
        : nullptr;
    installProjectileNodeConsumeHook(playerProcess);

    VrRigPoseSnapshot pose {};
    SharedVrOriginState authoritativeOrigin {};
    if (!readCoherentRetailRigPoseAndOrigin(pose, authoritativeOrigin))
    {
        const LONG count = InterlockedIncrement(&g_retailRigPoseOriginUnavailableCount);
        if (count <= 12 || count % 300 == 0)
        {
            logTelemetry(
                "retailRig skipped count=%ld reason=coherent-pose-origin-unavailable originMapped=%d\n",
                count,
                g_vrOriginState ? 1 : 0);
        }
        return;
    }
    if (pose.referenceSpaceGeneration == 0)
        return;
    if (g_haveRetailRigOrigin
        && (pose.referenceSpaceGeneration != g_retailRigReferenceSpaceGeneration
            || pose.producerEpoch != g_retailRigProducerEpoch))
    {
        resetRetailRigOrigin("reference-space-generation-changed");
    }
    if (g_haveRetailRigOrigin
        && (authoritativeOrigin.generation != g_retailRigReferenceSpaceGeneration
            || authoritativeOrigin.producerEpoch != g_retailRigProducerEpoch
            || authoritativeOrigin.poseSequence != g_retailRigOriginPoseSequence))
    {
        resetRetailRigOrigin("authoritative-origin-changed");
    }
    // The animation call site runs several times for one OpenXR frame. The
    // first-person rig has one owner and is solved once per stable pose.
    if (pose.sequence == g_lastRetailRigPoseSequence)
        return;
    if ((pose.trackingFlags & fnvxr::shared::VrPoseTrackingHmd) == 0)
        return;
    const bool leftControllerUsable =
        (pose.trackingFlags & fnvxr::shared::VrPoseTrackingLeftGripActive) != 0
        && (pose.trackingFlags & fnvxr::shared::VrPoseTrackingLeftGripCurrent) != 0;
    const bool rightControllerUsable =
        (pose.trackingFlags & fnvxr::shared::VrPoseTrackingRightGripActive) != 0
        && (pose.trackingFlags & fnvxr::shared::VrPoseTrackingRightGripCurrent) != 0;
    if (!leftControllerUsable && !rightControllerUsable)
        return;

    void* root = retrievePlayerRootNode(true);
    if (!root)
        root = readPointer(Camera1stBipedNodeAddress);
    if (root != g_retailRigNodes.root || !retailRigNodesComplete(g_retailRigNodes))
    {
        if (!discoverRetailRigNodes(root))
            return;
    }
    const UInt64 weaponRefreshStride = static_cast<UInt64>((std::max)(
        1,
        getIntFromEnv("FNVXR_RETAIL_WEAPON_REFRESH_SOLVES", 15)));
    if (g_retailRigSolveCount == 0 || (g_retailRigSolveCount % weaponRefreshStride) == 0)
        refreshRetailWeaponNodes();
    if (!retailRigNodesComplete(g_retailRigNodes))
        return;

    void* bodyRoot = retrievePlayerRootNode(false);
    if (!bodyRoot)
        return;
    const Matrix33 bodyWorldRotation = readMatrix33(
        reinterpret_cast<std::uintptr_t>(bodyRoot) + NiAvObjectWorldRotationOffset);
    const Vec3 bodyWorldPosition = readVec3(
        reinterpret_cast<std::uintptr_t>(bodyRoot) + NiAvObjectWorldTranslationOffset);
    const Vec3 stableCameraWorld {
        authoritativeOrigin.renderCameraWorldPos[0],
        authoritativeOrigin.renderCameraWorldPos[1],
        authoritativeOrigin.renderCameraWorldPos[2]
    };
    if (!finiteMatrix33(bodyWorldRotation)
        || !finiteVec3(bodyWorldPosition)
        || !finiteVec3(stableCameraWorld))
    {
        static LONG loggedAnchorUnavailable = 0;
        const LONG count = InterlockedIncrement(&loggedAnchorUnavailable);
        if (count <= 12 || count % 300 == 0)
        {
            logTelemetry(
                "retailRig skipped count=%ld reason=stable-body-anchor-unavailable bodyRoot=%p renderCamera=0x%08lx renderCameraWorldValid=%lu\n",
                count,
                bodyRoot,
                static_cast<unsigned long>(authoritativeOrigin.renderCameraAddress),
                static_cast<unsigned long>(authoritativeOrigin.renderCameraWorldValid));
        }
        return;
    }

    if (g_haveRetailRigOrigin && g_retailRigOriginBodyRoot != bodyRoot)
        resetRetailRigOrigin("body-root-changed");
    if (!g_haveRetailRigOrigin
        && !captureRetailRigOrigin(
            pose,
            authoritativeOrigin,
            bodyRoot,
            bodyWorldRotation,
            bodyWorldPosition,
            stableCameraWorld))
    {
        return;
    }

    // The hand/controller origin follows only the engine-authored body frame.
    // It must never use the current render camera because that camera can
    // already contain the HMD overlay during the Gamebryo render traversal.
    const Vec3 bodyAnchorWorld = addVec3(
        bodyWorldPosition,
        transformVec3(bodyWorldRotation, g_retailRigBodyAnchorLocal));
    if (!finiteVec3(bodyAnchorWorld))
        return;

    const RetailControllerWorldPose leftController = retailControllerWorldPose(
        pose, true, bodyAnchorWorld, bodyWorldRotation);
    const RetailControllerWorldPose rightController = retailControllerWorldPose(
        pose, false, bodyAnchorWorld, bodyWorldRotation);
    const bool applyWrites = envEnabled("FNVXR_RETAIL_RIG_APPLY", false);
    float leftError = 0.0f;
    float rightError = 0.0f;
    const bool rightSolved = rightControllerUsable && applyRetailArmFabrik(
        g_retailRigNodes.right,
        g_retailRightCalibration,
        rightController,
        bodyWorldRotation,
        false,
        applyWrites,
        rightError);
    const bool leftSolved = leftControllerUsable && applyRetailArmFabrik(
        g_retailRigNodes.left,
        g_retailLeftCalibration,
        leftController,
        bodyWorldRotation,
        true,
        applyWrites,
        leftError);
    const RetailWeaponApplyResult weaponResult = rightSolved
        ? applyRetailWeaponAim(rightController, applyWrites)
        : RetailWeaponApplyResult {};
    if (weaponResult.endpointMeasured)
    {
        g_latestMuzzleProofPoseSequence = pose.sequence;
        g_latestMuzzleProofNode = g_retailRigNodes.projectileNode
            ? g_retailRigNodes.projectileNode
            : g_retailRigNodes.muzzleFlash;
        g_latestMuzzleAimForward = weaponResult.aimForward;
    }
    const bool weaponAligned = weaponResult.writeVerified
        && weaponResult.endpointMeasured
        && weaponResult.endpointInWeaponBranch
        && niObjectAncestorChainVisible(g_retailRigNodes.weapon, g_retailRigNodes.root)
        && niObjectAncestorChainVisible(
            g_retailRigNodes.projectileNode
                ? g_retailRigNodes.projectileNode
                : g_retailRigNodes.muzzleFlash,
            g_retailRigNodes.root)
        && weaponResult.endpointAimResidualRadians <= getFloatFromEnv(
            "FNVXR_RETAIL_MUZZLE_MAX_AIM_RESIDUAL_RADIANS",
            0.08f);

    g_lastRetailRigPoseSequence = pose.sequence;
    g_lastRetailRigAnimData = animData;
    ++g_retailRigSolveCount;

    if (rightControllerUsable && g_retailRigNodes.right.hand)
    {
        const Vec3 headLocalMeters = xrPositionInOriginFrame(
            g_retailRigOriginHmdRot,
            g_retailRigOriginHmdPos,
            pose.hmdPos);
        const Quat headLocalRotation = multiplyQuat(
            conjugateQuat(g_retailRigOriginHmdRot),
            pose.hmdRot);
        const Matrix33 inverseBodyRotation = transposeMatrix33(bodyWorldRotation);
        const Vec3 rightHandWorld = readVec3(
            reinterpret_cast<std::uintptr_t>(g_retailRigNodes.right.hand)
                + NiAvObjectWorldTranslationOffset);
        const Vec3 rightHandTargetWorld = addVec3(
            rightController.wristPosition,
            transformVec3(
                rightController.wristRotation,
                g_retailRightCalibration.controllerToWristLocal));
        const Vec3 rightHandLocalUnits = transformVec3(
            inverseBodyRotation,
            subtractVec3(rightHandWorld, bodyAnchorWorld));
        const Vec3 weaponWorld = g_retailRigNodes.weapon
            ? readVec3(
                reinterpret_cast<std::uintptr_t>(g_retailRigNodes.weapon)
                    + NiAvObjectWorldTranslationOffset)
            : Vec3 {};
        const Matrix33 weaponWorldRotation = g_retailRigNodes.weapon
            ? readMatrix33(
                reinterpret_cast<std::uintptr_t>(g_retailRigNodes.weapon)
                    + NiAvObjectWorldRotationOffset)
            : Matrix33 {};
        const UInt32 firstPersonRootFlags = readUInt32(
            reinterpret_cast<std::uintptr_t>(g_retailRigNodes.root) + NiAvObjectFlagsOffset);
        const UInt32 rightHandFlags = readUInt32(
            reinterpret_cast<std::uintptr_t>(g_retailRigNodes.right.hand) + NiAvObjectFlagsOffset);
        const UInt32 weaponFlags = g_retailRigNodes.weapon
            ? readUInt32(
                reinterpret_cast<std::uintptr_t>(g_retailRigNodes.weapon) + NiAvObjectFlagsOffset)
            : 0xffffffffu;
        const UInt32 rightHandVisibleLeaves = countVisibleNiLeaves(
            g_retailRigNodes.right.hand,
            g_retailRigNodes.root);
        const UInt32 weaponVisibleLeaves = countVisibleNiLeaves(
            g_retailRigNodes.weapon,
            g_retailRigNodes.root);

        Vec3 headPositionDeltaVector {};
        Vec3 controllerPositionDeltaVector {};
        Vec3 targetLocalDeltaVector {};
        Vec3 handLocalDeltaVector {};
        Vec3 weaponWorldDeltaVector {};
        Vec3 bodyWorldDeltaVector {};
        Vec3 bodyAnchorDeltaVector {};
        Vec3 cameraWorldDeltaVector {};
        float headPositionDeltaMeters = 0.0f;
        float headAngularDeltaRadians = 0.0f;
        float controllerPositionDeltaMeters = 0.0f;
        float controllerAngularDeltaRadians = 0.0f;
        float targetLocalDeltaUnits = 0.0f;
        float handLocalDeltaUnits = 0.0f;
        float weaponPositionDeltaUnits = 0.0f;
        float weaponAngularDeltaRadians = 0.0f;
        float bodyPositionDeltaUnits = 0.0f;
        float bodyAnchorDeltaUnits = 0.0f;
        float cameraPositionDeltaUnits = 0.0f;
        if (g_haveRetailRigMotionSample)
        {
            headPositionDeltaVector = subtractVec3(
                headLocalMeters,
                g_previousRetailRigHeadLocalMeters);
            headPositionDeltaMeters = lengthVec3(headPositionDeltaVector);
            headAngularDeltaRadians = quaternionAngularDistance(
                headLocalRotation,
                g_previousRetailRigHeadLocalRotation);
            controllerPositionDeltaVector = subtractVec3(
                rightController.originLocalMeters,
                g_previousRetailRigRightLocalMeters);
            controllerPositionDeltaMeters = lengthVec3(controllerPositionDeltaVector);
            controllerAngularDeltaRadians = quaternionAngularDistance(
                rightController.originLocalRotation,
                g_previousRetailRigRightLocalRotation);
            targetLocalDeltaVector = subtractVec3(
                rightController.bodyLocalGameUnits,
                g_previousRetailRigRightTargetLocalUnits);
            targetLocalDeltaUnits = lengthVec3(targetLocalDeltaVector);
            handLocalDeltaVector = subtractVec3(
                rightHandLocalUnits,
                g_previousRetailRigRightHandLocalUnits);
            handLocalDeltaUnits = lengthVec3(handLocalDeltaVector);
            if (g_retailRigNodes.weapon)
            {
                weaponWorldDeltaVector = subtractVec3(
                    weaponWorld,
                    g_previousRetailRigWeaponWorld);
                weaponPositionDeltaUnits = lengthVec3(weaponWorldDeltaVector);
                weaponAngularDeltaRadians = matrixAngularDistance(
                    g_previousRetailRigWeaponWorldRotation,
                    weaponWorldRotation);
            }
            bodyWorldDeltaVector = subtractVec3(bodyWorldPosition, g_previousRetailRigBodyWorld);
            bodyPositionDeltaUnits = lengthVec3(bodyWorldDeltaVector);
            bodyAnchorDeltaVector = subtractVec3(bodyAnchorWorld, g_previousRetailRigBodyAnchorWorld);
            bodyAnchorDeltaUnits = lengthVec3(bodyAnchorDeltaVector);
            cameraWorldDeltaVector = subtractVec3(stableCameraWorld, g_previousRetailRigCameraWorld);
            cameraPositionDeltaUnits = lengthVec3(cameraWorldDeltaVector);
        }

        const float positionMotionMeters = 0.005f;
        const float angularMotionRadians = 0.03f;
        const bool headMoved = headPositionDeltaMeters >= positionMotionMeters
            || headAngularDeltaRadians >= angularMotionRadians;
        const bool controllerMoved = controllerPositionDeltaMeters >= positionMotionMeters
            || controllerAngularDeltaRadians >= angularMotionRadians;
        const bool headOnly = g_haveRetailRigMotionSample && headMoved && !controllerMoved;
        const bool controllerOnly = g_haveRetailRigMotionSample && controllerMoved && !headMoved;
        if (headOnly)
            ++g_retailRigHeadOnlySamples;
        if (controllerOnly)
            ++g_retailRigControllerOnlySamples;

        if (g_retailRigSolveCount <= 24
            || (g_retailRigSolveCount % 15) == 0
            || headOnly
            || controllerOnly)
        {
            logTelemetry(
                "{\"event\":\"fnvxrRigIndependence\",\"solve\":%llu,\"poseFrame\":%llu,\"poseSeq\":%ld,"
                "\"referenceGeneration\":%lu,\"originPoseSeq\":%lu,\"originAuthoritySeq\":%ld,"
                "\"renderPoseSeq\":%lu,\"gravityAlignedOrigin\":true,\"originUpDotWorldUp\":%.8f,"
                "\"originSource\":\"d3d9-native-camera\","
                "\"anchorSource\":\"exact-d3d9-render-camera\","
                "\"cameraInput\":\"hmd-only\",\"rigInput\":\"controller-only\","
                "\"apply\":%s,\"rightSolved\":%s,\"weaponAligned\":%s,"
                "\"weaponWriteRequested\":%s,\"weaponWriteAttempted\":%s,\"weaponWriteApplied\":%s,"
                "\"headLocalMeters\":[%.6f,%.6f,%.6f],"
                "\"controllerLocalMeters\":[%.6f,%.6f,%.6f],"
                "\"targetLocalUnits\":[%.5f,%.5f,%.5f],"
                "\"handLocalUnits\":[%.5f,%.5f,%.5f],"
                "\"bodyAnchorWorld\":[%.4f,%.4f,%.4f],"
                "\"weaponWorld\":[%.4f,%.4f,%.4f],"
                "\"delta\":{\"headMeters\":%.7f,\"headRadians\":%.7f,"
                "\"controllerMeters\":%.7f,\"controllerRadians\":%.7f,"
                "\"targetUnits\":%.6f,\"handUnits\":%.6f,\"weaponUnits\":%.6f,\"weaponRadians\":%.7f,"
                "\"bodyUnits\":%.6f,\"bodyAnchorUnits\":%.6f,\"cameraUnits\":%.6f},"
                "\"deltaVectors\":{\"headMeters\":[%.7f,%.7f,%.7f],"
                "\"controllerMeters\":[%.7f,%.7f,%.7f],\"targetUnits\":[%.6f,%.6f,%.6f],"
                "\"handUnits\":[%.6f,%.6f,%.6f],\"weaponUnits\":[%.6f,%.6f,%.6f],"
                "\"bodyUnits\":[%.6f,%.6f,%.6f],\"cameraUnits\":[%.6f,%.6f,%.6f]},"
                "\"classification\":{\"headMoved\":%s,\"controllerMoved\":%s,"
                "\"headOnly\":%s,\"controllerOnly\":%s},"
                "\"samples\":{\"headOnly\":%llu,\"controllerOnly\":%llu},"
                "\"handTargetErrorUnits\":%.6f,\"weaponPositionResidualUnits\":%.6f,"
                "\"weaponAngularResidualRadians\":%.7f,\"muzzleMeasured\":%s,"
                "\"muzzleInWeaponBranch\":%s,\"muzzleAimResidualRadians\":%.7f,"
                "\"muzzleWorld\":[%.5f,%.5f,%.5f],\"aimForward\":[%.7f,%.7f,%.7f],"
                "\"muzzleForward\":[%.7f,%.7f,%.7f],\"projectileNodeConsumeHookInstalled\":%s,"
                "\"culling\":{\"rootFlags\":%lu,\"rightHandFlags\":%lu,\"weaponFlags\":%lu,"
                "\"rootAppCulled\":%s,\"rightHandAppCulled\":%s,\"weaponAppCulled\":%s,"
                "\"rightHandVisibleLeaves\":%lu,\"weaponVisibleLeaves\":%lu},"
                "\"headTermInRigTransform\":0}\n",
                static_cast<unsigned long long>(g_retailRigSolveCount),
                static_cast<unsigned long long>(pose.frame),
                pose.sequence,
                static_cast<unsigned long>(g_retailRigReferenceSpaceGeneration),
                static_cast<unsigned long>(g_retailRigOriginPoseSequence),
                g_retailRigOriginAuthoritySequence,
                static_cast<unsigned long>(authoritativeOrigin.renderPoseSequence),
                1.0f - 2.0f * (
                    g_retailRigOriginHmdRot.x * g_retailRigOriginHmdRot.x
                    + g_retailRigOriginHmdRot.z * g_retailRigOriginHmdRot.z),
                applyWrites ? "true" : "false",
                rightSolved ? "true" : "false",
                weaponAligned ? "true" : "false",
                weaponResult.writeRequested ? "true" : "false",
                weaponResult.writeAttempted ? "true" : "false",
                weaponResult.writeVerified ? "true" : "false",
                headLocalMeters.x,
                headLocalMeters.y,
                headLocalMeters.z,
                rightController.originLocalMeters.x,
                rightController.originLocalMeters.y,
                rightController.originLocalMeters.z,
                rightController.bodyLocalGameUnits.x,
                rightController.bodyLocalGameUnits.y,
                rightController.bodyLocalGameUnits.z,
                rightHandLocalUnits.x,
                rightHandLocalUnits.y,
                rightHandLocalUnits.z,
                bodyAnchorWorld.x,
                bodyAnchorWorld.y,
                bodyAnchorWorld.z,
                weaponWorld.x,
                weaponWorld.y,
                weaponWorld.z,
                headPositionDeltaMeters,
                headAngularDeltaRadians,
                controllerPositionDeltaMeters,
                controllerAngularDeltaRadians,
                targetLocalDeltaUnits,
                handLocalDeltaUnits,
                weaponPositionDeltaUnits,
                weaponAngularDeltaRadians,
                bodyPositionDeltaUnits,
                bodyAnchorDeltaUnits,
                cameraPositionDeltaUnits,
                headPositionDeltaVector.x,
                headPositionDeltaVector.y,
                headPositionDeltaVector.z,
                controllerPositionDeltaVector.x,
                controllerPositionDeltaVector.y,
                controllerPositionDeltaVector.z,
                targetLocalDeltaVector.x,
                targetLocalDeltaVector.y,
                targetLocalDeltaVector.z,
                handLocalDeltaVector.x,
                handLocalDeltaVector.y,
                handLocalDeltaVector.z,
                weaponWorldDeltaVector.x,
                weaponWorldDeltaVector.y,
                weaponWorldDeltaVector.z,
                bodyWorldDeltaVector.x,
                bodyWorldDeltaVector.y,
                bodyWorldDeltaVector.z,
                cameraWorldDeltaVector.x,
                cameraWorldDeltaVector.y,
                cameraWorldDeltaVector.z,
                headMoved ? "true" : "false",
                controllerMoved ? "true" : "false",
                headOnly ? "true" : "false",
                controllerOnly ? "true" : "false",
                static_cast<unsigned long long>(g_retailRigHeadOnlySamples),
                static_cast<unsigned long long>(g_retailRigControllerOnlySamples),
                lengthVec3(subtractVec3(rightHandWorld, rightHandTargetWorld)),
                weaponResult.positionResidualUnits,
                weaponResult.angularResidualRadians,
                weaponResult.endpointMeasured ? "true" : "false",
                weaponResult.endpointInWeaponBranch ? "true" : "false",
                weaponResult.endpointAimResidualRadians,
                weaponResult.endpointWorldPosition.x,
                weaponResult.endpointWorldPosition.y,
                weaponResult.endpointWorldPosition.z,
                weaponResult.aimForward.x,
                weaponResult.aimForward.y,
                weaponResult.aimForward.z,
                weaponResult.endpointForward.x,
                weaponResult.endpointForward.y,
                weaponResult.endpointForward.z,
                g_projectileNodeHookInstalled ? "true" : "false",
                static_cast<unsigned long>(firstPersonRootFlags),
                static_cast<unsigned long>(rightHandFlags),
                static_cast<unsigned long>(weaponFlags),
                (firstPersonRootFlags & 1u) != 0 ? "true" : "false",
                (rightHandFlags & 1u) != 0 ? "true" : "false",
                g_retailRigNodes.weapon && (weaponFlags & 1u) != 0 ? "true" : "false",
                static_cast<unsigned long>(rightHandVisibleLeaves),
                static_cast<unsigned long>(weaponVisibleLeaves));
        }

        g_previousRetailRigHeadLocalMeters = headLocalMeters;
        g_previousRetailRigHeadLocalRotation = headLocalRotation;
        g_previousRetailRigRightLocalMeters = rightController.originLocalMeters;
        g_previousRetailRigRightLocalRotation = rightController.originLocalRotation;
        g_previousRetailRigRightTargetLocalUnits = rightController.bodyLocalGameUnits;
        g_previousRetailRigRightHandLocalUnits = rightHandLocalUnits;
        g_previousRetailRigWeaponWorld = weaponWorld;
        g_previousRetailRigWeaponWorldRotation = weaponWorldRotation;
        g_previousRetailRigBodyWorld = bodyWorldPosition;
        g_previousRetailRigBodyAnchorWorld = bodyAnchorWorld;
        g_previousRetailRigCameraWorld = stableCameraWorld;
        g_haveRetailRigMotionSample = true;
    }

    if (g_retailRigSolveCount <= 24 || (g_retailRigSolveCount % 120) == 0)
    {
        logTelemetry(
            "retailRig solve count=%llu seq=%ld poseFrame=%llu tracking=0x%03lX rightAimCurrent=%d orientationSource=%s anim=%p thirdAnim=%p root=%p apply=%d leftSolved=%d rightSolved=%d weaponAligned=%d weaponApply=%d error=(%.3f %.3f) leftTarget=(%.3f %.3f %.3f) rightTarget=(%.3f %.3f %.3f) rightAimWorldR=[%.4f %.4f %.4f | %.4f %.4f %.4f | %.4f %.4f %.4f]\n",
            static_cast<unsigned long long>(g_retailRigSolveCount),
            pose.sequence,
            static_cast<unsigned long long>(pose.frame),
            static_cast<unsigned long>(pose.trackingFlags),
            static_cast<int>(
                (pose.trackingFlags & fnvxr::shared::VrPoseTrackingRightAimActive) != 0
                && (pose.trackingFlags & fnvxr::shared::VrPoseTrackingRightAimCurrent) != 0),
            rightController.usesAimOrientation ? "aim" : "grip",
            animData,
            thirdPersonAnimData,
            root,
            applyWrites ? 1 : 0,
            leftSolved ? 1 : 0,
            rightSolved ? 1 : 0,
            weaponAligned ? 1 : 0,
            envEnabled("FNVXR_RETAIL_WEAPON_APPLY", false) ? 1 : 0,
            leftError,
            rightError,
            leftController.position.x,
            leftController.position.y,
            leftController.position.z,
            rightController.position.x,
            rightController.position.y,
            rightController.position.z,
            rightController.rotation.m[0][0],
            rightController.rotation.m[0][1],
            rightController.rotation.m[0][2],
            rightController.rotation.m[1][0],
            rightController.rotation.m[1][1],
            rightController.rotation.m[1][2],
            rightController.rotation.m[2][0],
            rightController.rotation.m[2][1],
            rightController.rotation.m[2][2]);
        logNiAvObjectTransform("retailRig.weapon", g_retailRigNodes.weapon);
        logNiAvObjectTransform("retailRig.projectile", g_retailRigNodes.projectileNode);
        logNiAvObjectTransform("retailRig.muzzleFlash", g_retailRigNodes.muzzleFlash);
    }
}

#if defined(_M_IX86)
__declspec(naked) void hookedRetailAnimationApply()
{
    __asm
    {
        call ApplyActorAnimDataAddress
        pushfd
        pushad
        mov eax, [ebp + 0x08]
        push eax
        call onRetailPostAnimation
        add esp, 4
        popad
        popfd
        jmp PlayerAnimationApplyReturnAddress
    }
}
#endif

bool installRetailRigHook()
{
    if (g_retailRigHookInstalled)
        return true;
    if (!envEnabled("FNVXR_RETAIL_RIG_ENABLE", false))
    {
        logTelemetry("retailRig hook install disabled\n");
        return true;
    }
#if !defined(_M_IX86)
    logTelemetry("retailRig hook requires the 32-bit retail process\n");
    return false;
#else
    auto* target = pointerFromAddress32<UInt8*>(PlayerAnimationApplyCallSiteAddress);
    UInt32 originalTarget = 0;
    __try
    {
        if (target[0] == 0xE8)
        {
            const std::int32_t relative = *reinterpret_cast<std::int32_t*>(target + 1);
            originalTarget = static_cast<UInt32>(
                static_cast<std::int64_t>(PlayerAnimationApplyCallSiteAddress + 5) + relative);
        }
        if (target[0] != 0xE8 || originalTarget != ApplyActorAnimDataAddress)
        {
            logTelemetry(
                "retailRig hook mismatch opcode=%02X decodedTarget=0x%08lX expected=0x%08lX (another animation hook may be installed)\n",
                target[0],
                static_cast<unsigned long>(originalTarget),
                static_cast<unsigned long>(ApplyActorAnimDataAddress));
            return false;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        logTelemetry("retailRig hook source read exception\n");
        return false;
    }

    if (!writeJump(PlayerAnimationApplyCallSiteAddress, reinterpret_cast<void*>(hookedRetailAnimationApply)))
    {
        logTelemetry("retailRig hook write failed err=%lu\n", GetLastError());
        return false;
    }
    g_retailRigHookInstalled = true;
    logTelemetry(
        "retailRig hook installed site=%p hook=%p original=%p apply=%d\n",
        pointerFromAddress32<void*>(PlayerAnimationApplyCallSiteAddress),
        reinterpret_cast<void*>(hookedRetailAnimationApply),
        pointerFromAddress32<void*>(ApplyActorAnimDataAddress),
        envEnabled("FNVXR_RETAIL_RIG_APPLY", false) ? 1 : 0);
    return true;
#endif
}

TileValue* getTileValue(void* tile, UInt32 id)
{
    if (!tile)
        return nullptr;

    auto* values = *reinterpret_cast<TileValue***>(reinterpret_cast<std::uintptr_t>(tile) + 0x14);
    const UInt32 size = *reinterpret_cast<UInt32*>(reinterpret_cast<std::uintptr_t>(tile) + 0x18);
    if (!values || size > 512)
        return nullptr;

    for (UInt32 index = 0; index < size; ++index)
    {
        TileValue* value = values[index];
        if (value && value->id == id)
            return value;
    }
    return nullptr;
}

UInt32 getTileButtonId(void* tile)
{
    TileValue* id = getTileValue(tile, TileValueId);
    if (!id)
        id = getTileValue(tile, 0x0FAA);
    if (!id)
        return 0;

    return static_cast<UInt32>(id->num);
}

UInt32 tileTraitId(const char* name, UInt32 fallback)
{
    using TraitNameToIDFn = UInt32 (__cdecl*)(const char*);
    __try
    {
        const UInt32 id = pointerFromAddress32<TraitNameToIDFn>(TraitNameToIDAddress)(name);
        return id ? id : fallback;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return fallback;
    }
}

TileValue* getTileValueByName(void* tile, const char* name, UInt32 fallback)
{
    return getTileValue(tile, tileTraitId(name, fallback));
}

float getTileFloatByName(void* tile, const char* name, UInt32 fallback, float defaultValue, bool* found = nullptr)
{
    TileValue* value = getTileValueByName(tile, name, fallback);
    if (found)
        *found = value != nullptr;
    return value ? value->num : defaultValue;
}

void setTileFloat(void* tile, UInt32 valueId, float value)
{
    if (!tile)
        return;

    using SetFloatValueFn = void (__thiscall*)(void*, UInt32, float, bool);
    pointerFromAddress32<SetFloatValueFn>(TileSetFloatValueAddress)(tile, valueId, value, true);
}

void setTileFloatByName(void* tile, const char* name, UInt32 fallback, float value)
{
    setTileFloat(tile, tileTraitId(name, fallback), value);
}

void* tileMenuByType(UInt32 menuType)
{
    if (menuType < kMenuTypeMin || menuType > kMenuTypeMax)
        return nullptr;

    auto* array = pointerFromAddress32<NiTArrayRaw*>(TileMenuArrayAddress);
    const UInt32 index = menuType - kMenuTypeMin;
    if (!array->data || index >= array->firstFreeEntry || index >= array->capacity)
        return nullptr;
    return array->data[index];
}

void* menuFromTileMenu(void* tileMenu)
{
    return tileMenu ? readPointer(reinterpret_cast<std::uintptr_t>(tileMenu) + 0x3C) : nullptr;
}

void* tileRootFromMenu(void* menu, void* tileMenu)
{
    void* root = menu ? readPointer(reinterpret_cast<std::uintptr_t>(menu) + 0x04) : nullptr;
    return root ? root : tileMenu;
}

void* visibleMenuForInput(void** outTileMenu = nullptr, UInt32* outMenuType = nullptr)
{
    void* interfaceManager = readPointer(InterfaceManagerAddress);
    void* activeMenu = interfaceManager
        ? readPointer(reinterpret_cast<std::uintptr_t>(interfaceManager) + 0x0D0)
        : nullptr;
    if (activeMenu)
    {
        const UInt32 activeType = readUInt32(reinterpret_cast<std::uintptr_t>(activeMenu) + 0x20);
        void* validatedMenu = nullptr;
        void* validatedTileMenu = nullptr;
        if (validatedVisibleMenu(activeType, activeMenu, &validatedMenu, &validatedTileMenu))
        {
            if (outTileMenu)
                *outTileMenu = validatedTileMenu;
            if (outMenuType)
                *outMenuType = activeType;
            return validatedMenu;
        }
    }

    const UInt32 priorityMenus[] = {
        kMenuTypeStart,
        kMenuTypeRaceSex,
        kMenuTypeInventory,
        kMenuTypeStats,
        kMenuTypeMap,
        kMenuTypeDialog,
        kMenuTypeVats,
        kMenuTypeLoading,
    };
    for (UInt32 menuType : priorityMenus)
    {
        void* tileMenu = nullptr;
        void* menu = nullptr;
        if (validatedVisibleMenu(menuType, nullptr, &menu, &tileMenu))
        {
            if (outTileMenu)
                *outTileMenu = tileMenu;
            if (outMenuType)
                *outMenuType = menuType;
            return menu;
        }
    }

    for (UInt32 menuType = kMenuTypeMin; menuType <= kMenuTypeMax; ++menuType)
    {
        void* tileMenu = nullptr;
        void* menu = nullptr;
        if (validatedVisibleMenu(menuType, nullptr, &menu, &tileMenu))
        {
            if (outTileMenu)
                *outTileMenu = tileMenu;
            if (outMenuType)
                *outMenuType = menuType;
            return menu;
        }
    }

    if (outTileMenu)
        *outTileMenu = nullptr;
    if (outMenuType)
        *outMenuType = 0;
    return nullptr;
}

void collectMenuButtons(
    void* tile,
    float parentX,
    float parentY,
    UInt32 depth,
    std::vector<MenuButtonCandidate>& buttons)
{
    if (!tile || depth > 18 || buttons.size() >= 384)
        return;

    const float x = parentX + getTileFloatByName(tile, "x", TileValueX, 0.0f);
    const float y = parentY + getTileFloatByName(tile, "y", TileValueY, 0.0f);
    const float visible = getTileFloatByName(tile, "visible", TileValueVisible, 1.0f);
    const float width = getTileFloatByName(tile, "width", TileValueWidth, 0.0f);
    const float height = getTileFloatByName(tile, "height", TileValueHeight, 0.0f);
    const UInt32 buttonId = getTileButtonId(tile);
    const bool plausibleButtonRect = width >= 4.0f && height >= 4.0f && width <= 900.0f && height <= 220.0f;
    if (visible != 0.0f && buttonId != 0 && plausibleButtonRect)
    {
        buttons.push_back({ tile, buttonId, x, y, width, height });
    }

    auto* node = reinterpret_cast<TileListNode*>(reinterpret_cast<std::uintptr_t>(tile) + 0x04);
    for (UInt32 count = 0; node && count < 512; ++count)
    {
        auto* childNode = static_cast<TileChildNode*>(node->data);
        void* child = childNode ? childNode->child : nullptr;
        if (child)
            collectMenuButtons(child, x, y, depth + 1, buttons);
        node = node->next;
    }
}

std::vector<MenuButtonCandidate> visibleMenuButtons(void* menu, void* tileMenu)
{
    std::vector<MenuButtonCandidate> buttons;
    void* root = tileRootFromMenu(menu, tileMenu);
    if (!root)
        return buttons;

    collectMenuButtons(root, 0.0f, 0.0f, 0, buttons);

    std::sort(buttons.begin(), buttons.end(), [](const MenuButtonCandidate& lhs, const MenuButtonCandidate& rhs) {
        if (std::fabs(lhs.y - rhs.y) > 8.0f)
            return lhs.y < rhs.y;
        return lhs.x < rhs.x;
    });

    return buttons;
}

bool pointerInsideButton(const MenuButtonCandidate& candidate, float px, float py)
{
    return px >= candidate.x && px <= candidate.x + candidate.width
        && py >= candidate.y && py <= candidate.y + candidate.height;
}

const MenuButtonCandidate* buttonUnderPointer(
    const std::vector<MenuButtonCandidate>& buttons, float px, float py)
{
    for (const auto& candidate : buttons)
    {
        if (pointerInsideButton(candidate, px, py))
            return &candidate;
    }
    return nullptr;
}

PointerMenuPoint rawPointerMenuPoint()
{
    return {
        static_cast<float>(g_lastMenuPointerClient.x),
        static_cast<float>(g_lastMenuPointerClient.y),
        "raw"
    };
}

PointerMenuPoint scaledPointerMenuPoint()
{
    const auto atLeast = [](float value, float minimum) {
        return value < minimum ? minimum : value;
    };
    const float sourceWidth = atLeast(getFloatFromEnv("FNVXR_UI_SHARED_WIDTH", static_cast<float>(SharedVideoPointerWidth)), 2.0f);
    const float sourceHeight = atLeast(getFloatFromEnv("FNVXR_UI_SHARED_HEIGHT", static_cast<float>(SharedVideoPointerHeight)), 2.0f);
    const float tileWidth = atLeast(getFloatFromEnv("FNVXR_MENU_TILE_WIDTH", 640.0f), 2.0f);
    const float tileHeight = atLeast(getFloatFromEnv("FNVXR_MENU_TILE_HEIGHT", 480.0f), 2.0f);
    const float sharedX = std::clamp(static_cast<float>(g_lastMenuPointerClient.x), 0.0f, sourceWidth - 1.0f);
    const float sharedY = std::clamp(static_cast<float>(g_lastMenuPointerClient.y), 0.0f, sourceHeight - 1.0f);

    return {
        (sharedX * (tileWidth - 1.0f)) / (sourceWidth - 1.0f),
        (sharedY * (tileHeight - 1.0f)) / (sourceHeight - 1.0f),
        "scaled"
    };
}

const MenuButtonCandidate* buttonUnderPointerInAnySpace(
    const std::vector<MenuButtonCandidate>& buttons,
    PointerMenuPoint& resolved)
{
    resolved = rawPointerMenuPoint();
    if (const MenuButtonCandidate* hit = buttonUnderPointer(buttons, resolved.x, resolved.y))
        return hit;

    resolved = scaledPointerMenuPoint();
    if (const MenuButtonCandidate* hit = buttonUnderPointer(buttons, resolved.x, resolved.y))
        return hit;

    return nullptr;
}

void logPointerMissDetails(
    void* menu,
    UInt32 menuType,
    const std::vector<MenuButtonCandidate>& buttons,
    const PointerMenuPoint& rawPoint,
    const PointerMenuPoint& scaledPoint)
{
    if (g_directMenuPointerMissDetailLogCount >= 8)
        return;

    ++g_directMenuPointerMissDetailLogCount;
    logTelemetry(
        "directMenu candidates missDetail=%u menu=%p type=0x%lx buttons=%zu shared=(%ld,%ld) raw=(%.1f,%.1f) scaled=(%.1f,%.1f)\n",
        g_directMenuPointerMissDetailLogCount,
        menu,
        static_cast<unsigned long>(menuType),
        buttons.size(),
        g_lastMenuPointerClient.x,
        g_lastMenuPointerClient.y,
        rawPoint.x,
        rawPoint.y,
        scaledPoint.x,
        scaledPoint.y);
    const size_t count = std::min<size_t>(buttons.size(), 32);
    for (size_t index = 0; index < count; ++index)
    {
        const auto& candidate = buttons[index];
        logTelemetry(
            "directMenu candidate[%zu] tile=%p id=%u rect=(%.1f %.1f %.1f %.1f)\n",
            index,
            candidate.tile,
            candidate.buttonId,
            candidate.x,
            candidate.y,
            candidate.width,
            candidate.height);
    }
}

void clearPointerHover()
{
    if (g_directMenuPointerHoverTile)
    {
        setTileFloatByName(g_directMenuPointerHoverTile, "mouseover", TileValueMouseover, 0.0f);
        g_directMenuPointerHoverTile = nullptr;
        g_directMenuPointerHoverMenu = nullptr;
    }
}

bool updateDirectMenuPointerHover()
{
    if (!g_hasMenuPointer)
    {
        clearPointerHover();
        return false;
    }

    void* tileMenu = nullptr;
    UInt32 menuType = 0;
    void* menu = visibleMenuForInput(&tileMenu, &menuType);
    if (!menu)
    {
        clearPointerHover();
        return false;
    }

    const auto buttons = visibleMenuButtons(menu, tileMenu);
    PointerMenuPoint point {};
    const MenuButtonCandidate* hit = buttonUnderPointerInAnySpace(buttons, point);
    if (!hit)
    {
        clearPointerHover();
        return false;
    }

    if (g_directMenuPointerHoverTile && g_directMenuPointerHoverTile != hit->tile)
        setTileFloatByName(g_directMenuPointerHoverTile, "mouseover", TileValueMouseover, 0.0f);

    g_directMenuPointerHoverMenu = menu;
    g_directMenuPointerHoverTile = hit->tile;
    g_directMenuSelectionMenu = menu;
    g_directMenuSelectionTile = hit->tile;
    setTileFloatByName(hit->tile, "mouseover", TileValueMouseover, 1.0f);

    if (g_directMenuPointerHoverLogCount < 64)
    {
        ++g_directMenuPointerHoverLogCount;
        logTelemetry(
            "directMenu hover menu=%p type=0x%lx shared=(%ld,%ld) point=(%.1f,%.1f) space=%s tile=%p id=%u rect=(%.1f %.1f %.1f %.1f)\n",
            menu,
            static_cast<unsigned long>(menuType),
            g_lastMenuPointerClient.x,
            g_lastMenuPointerClient.y,
            point.x,
            point.y,
            point.space,
            hit->tile,
            hit->buttonId,
            hit->x,
            hit->y,
            hit->width,
            hit->height);
    }

    return true;
}

bool dispatchMenuClick(void* menu, void* tile, UInt32 buttonId, const char* source)
{
    if (!menu || !tile || buttonId == 0)
        return false;

    __try
    {
        setTileFloatByName(tile, "mouseover", TileValueMouseover, 1.0f);
        setTileFloatByName(tile, "clicked", TileValueClicked, 1.0f);

        void** vtable = *reinterpret_cast<void***>(menu);
        if (!vtable || !vtable[3])
            return false;

        using HandleMouseoverFn = void (__thiscall*)(void*, UInt32, void*);
        if (vtable[4])
            reinterpret_cast<HandleMouseoverFn>(vtable[4])(menu, buttonId, tile);

        using HandleClickFn = void (__thiscall*)(void*, UInt32, void*);
        reinterpret_cast<HandleClickFn>(vtable[3])(menu, buttonId, tile);
        logTelemetry("directMenu click source=%s menu=%p tile=%p buttonId=%u\n", source, menu, tile, buttonId);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        logTelemetry("directMenu click exception source=%s menu=%p tile=%p buttonId=%u\n", source, menu, tile, buttonId);
        return false;
    }
}

bool dispatchPointerMenuClick()
{
    void* tileMenu = nullptr;
    UInt32 menuType = 0;
    void* menu = visibleMenuForInput(&tileMenu, &menuType);
    if (!menu)
        return false;

    const auto buttons = visibleMenuButtons(menu, tileMenu);
    if (buttons.empty())
    {
        logTelemetry("directMenu pointer no-buttons menu=%p type=0x%lx tileMenu=%p\n", menu, static_cast<unsigned long>(menuType), tileMenu);
        return false;
    }

    PointerMenuPoint point {};
    const PointerMenuPoint rawPoint = rawPointerMenuPoint();
    const PointerMenuPoint scaledPoint = scaledPointerMenuPoint();
    const MenuButtonCandidate* best = buttonUnderPointerInAnySpace(buttons, point);
    float bestDistance = 0.0f;
    if (!best && envEnabled("FNVXR_POINTER_CLICK_NEAREST", false))
    {
        point = rawPoint;
        bestDistance = 1.0e20f;
        for (const auto& candidate : buttons)
        {
            const float cx = candidate.x + candidate.width * 0.5f;
            const float cy = candidate.y + candidate.height * 0.5f;
            const float dx = point.x - cx;
            const float dy = point.y - cy;
            const float distance = dx * dx + dy * dy;
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = &candidate;
            }
        }
    }

    if (!best)
    {
        clearPointerHover();
        logTelemetry(
            "directMenu pointer miss menu=%p type=0x%lx buttons=%zu shared=(%ld,%ld) raw=(%.1f,%.1f) scaled=(%.1f,%.1f)\n",
            menu,
            static_cast<unsigned long>(menuType),
            buttons.size(),
            g_lastMenuPointerClient.x,
            g_lastMenuPointerClient.y,
            rawPoint.x,
            rawPoint.y,
            scaledPoint.x,
            scaledPoint.y);
        logPointerMissDetails(menu, menuType, buttons, rawPoint, scaledPoint);
        return false;
    }

    g_directMenuSelectionMenu = menu;
    g_directMenuSelectionTile = best->tile;
    setTileFloatByName(best->tile, "mouseover", TileValueMouseover, 1.0f);
    logTelemetry(
        "directMenu pointer menu=%p type=0x%lx buttons=%zu shared=(%ld,%ld) point=(%.1f,%.1f) space=%s chosen=%p id=%u rect=(%.1f %.1f %.1f %.1f) dist=%.1f\n",
        menu,
        static_cast<unsigned long>(menuType),
        buttons.size(),
        g_lastMenuPointerClient.x,
        g_lastMenuPointerClient.y,
        point.x,
        point.y,
        point.space,
        best->tile,
        best->buttonId,
        best->x,
        best->y,
        best->width,
        best->height,
        bestDistance);
    return dispatchMenuClick(menu, best->tile, best->buttonId, "pointer");
}

bool directMenuNavigate(int delta)
{
    void* tileMenu = nullptr;
    UInt32 menuType = 0;
    void* menu = visibleMenuForInput(&tileMenu, &menuType);
    if (!menu)
        return false;

    const auto buttons = visibleMenuButtons(menu, tileMenu);
    if (buttons.empty())
    {
        logTelemetry("directMenu nav no-buttons menu=%p type=0x%lx tileMenu=%p\n", menu, static_cast<unsigned long>(menuType), tileMenu);
        return false;
    }

    UInt32 index = 0;
    if (g_directMenuSelectionMenu == menu && g_directMenuSelectionTile)
    {
        for (UInt32 i = 0; i < buttons.size(); ++i)
        {
            if (buttons[i].tile == g_directMenuSelectionTile)
            {
                index = i;
                break;
            }
        }
    }
    else
    {
        g_directMenuSelectionMenu = menu;
        index = 0;
    }

    const int next = static_cast<int>(index) + delta;
    if (next < 0)
        index = static_cast<UInt32>(buttons.size() - 1);
    else if (next >= static_cast<int>(buttons.size()))
        index = 0;
    else
        index = static_cast<UInt32>(next);

    g_directMenuSelectionIndex = index;
    g_directMenuSelectionTile = buttons[index].tile;
    setTileFloatByName(g_directMenuSelectionTile, "mouseover", TileValueMouseover, 1.0f);
    if (g_directMenuSelectionLogCount < 96)
    {
        ++g_directMenuSelectionLogCount;
        logTelemetry(
            "directMenu nav menu=%p type=0x%lx buttons=%zu index=%lu tile=%p id=%u delta=%d rect=(%.1f %.1f %.1f %.1f)\n",
            menu,
            static_cast<unsigned long>(menuType),
            buttons.size(),
            static_cast<unsigned long>(index),
            buttons[index].tile,
            buttons[index].buttonId,
            delta,
            buttons[index].x,
            buttons[index].y,
            buttons[index].width,
            buttons[index].height);
    }
    return true;
}

bool directMenuAcceptSelection()
{
    void* tileMenu = nullptr;
    UInt32 menuType = 0;
    void* menu = visibleMenuForInput(&tileMenu, &menuType);
    if (!menu)
        return false;

    const auto buttons = visibleMenuButtons(menu, tileMenu);
    if (buttons.empty())
        return false;

    const MenuButtonCandidate* candidate = nullptr;
    if (g_directMenuSelectionMenu == menu && g_directMenuSelectionTile)
    {
        for (const auto& item : buttons)
        {
            if (item.tile == g_directMenuSelectionTile)
            {
                candidate = &item;
                break;
            }
        }
    }
    if (!candidate)
    {
        const UInt32 index = std::min<UInt32>(g_directMenuSelectionIndex, static_cast<UInt32>(buttons.size() - 1));
        candidate = &buttons[index];
        g_directMenuSelectionTile = candidate->tile;
    }

    logTelemetry(
        "directMenu accept menu=%p type=0x%lx tile=%p id=%u\n",
        menu,
        static_cast<unsigned long>(menuType),
        candidate->tile,
        candidate->buttonId);
    return dispatchMenuClick(menu, candidate->tile, candidate->buttonId, "xinput");
}

POINT mapSharedPointerToWindow(HWND hwnd, POINT point);

void updateGameCursorTile(HWND hwnd = nullptr)
{
    void* interfaceManager = *pointerFromAddress32<void**>(InterfaceManagerAddress);
    if (!interfaceManager || !g_hasMenuPointer)
        return;

    void* cursorTile = *reinterpret_cast<void**>(reinterpret_cast<std::uintptr_t>(interfaceManager) + 0x028);
    if (!cursorTile)
        return;

    __try
    {
        const POINT windowPointer = hwnd ? mapSharedPointerToWindow(hwnd, g_lastMenuPointerClient) : g_lastMenuPointerClient;
        setTileFloat(cursorTile, TileValueX, static_cast<float>(windowPointer.x));
        setTileFloat(cursorTile, TileValueY, static_cast<float>(windowPointer.y));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        logTelemetry("cursorTile exception cursor=%p client=(%ld,%ld)\n", cursorTile, g_lastMenuPointerClient.x, g_lastMenuPointerClient.y);
    }
}

bool dispatchActiveMenuClick()
{
    const bool directUi = directUiClickEnabled();
    const bool pointerFallback = pointerTileFallbackEnabled();
    if (!directUi && !pointerFallback)
        return false;
    if (!allowUiInput())
        return false;

    if (retailSidecarProfile() && g_hasMenuPointer && pointerFallback)
        return dispatchPointerMenuClick();
    if (!directUi)
        return false;

    void* interfaceManager = *pointerFromAddress32<void**>(InterfaceManagerAddress);
    if (!interfaceManager)
    {
        logTelemetry("uiClick interface=null\n");
        return false;
    }

    void* activeTileAlt = *reinterpret_cast<void**>(reinterpret_cast<std::uintptr_t>(interfaceManager) + 0x0BC);
    void* activeTile = *reinterpret_cast<void**>(reinterpret_cast<std::uintptr_t>(interfaceManager) + 0x0CC);
    void* activeMenu = *reinterpret_cast<void**>(reinterpret_cast<std::uintptr_t>(interfaceManager) + 0x0D0);
    void* tile = activeTile ? activeTile : activeTileAlt;
    const UInt32 buttonId = getTileButtonId(tile);
    logTelemetry(
        "uiClick interface=%p menu=%p active=%p alt=%p chosen=%p buttonId=%u\n",
        interfaceManager,
        activeMenu,
        activeTile,
        activeTileAlt,
        tile,
        buttonId);

    if (!activeMenu || !tile || buttonId == 0)
    {
        if (g_hasMenuPointer && pointerFallback)
            return dispatchPointerMenuClick();
        return directMenuAcceptSelection();
    }

    __try
    {
        setTileFloat(tile, TileValueClicked, 1.0f);

        void** vtable = *reinterpret_cast<void***>(activeMenu);
        if (!vtable || !vtable[3])
            return false;

        using HandleClickFn = void (__thiscall*)(void*, UInt32, void*);
        reinterpret_cast<HandleClickFn>(vtable[3])(activeMenu, buttonId, tile);
        logTelemetry("uiClick dispatched buttonId=%u tile=%p menu=%p\n", buttonId, tile, activeMenu);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        logTelemetry("uiClick exception menu=%p tile=%p buttonId=%u\n", activeMenu, tile, buttonId);
        return false;
    }
}

void postMenuKey(HWND hwnd, WPARAM virtualKey)
{
    if (!hwnd || !envEnabled("FNVXR_POST_MENU_KEYS", false))
        return;

    PostMessageA(hwnd, WM_KEYDOWN, virtualKey, 1);
    PostMessageA(hwnd, WM_KEYUP, virtualKey, 0xC0000001);
}

POINT mapSharedPointerToWindow(HWND hwnd, POINT point)
{
    RECT client {};
    if (!hwnd || !GetClientRect(hwnd, &client))
        return point;

    const LONG width = client.right - client.left;
    const LONG height = client.bottom - client.top;
    if (width <= 0 || height <= 0)
        return point;
    const LONG sourceWidth = static_cast<LONG>(getIntFromEnv("FNVXR_UI_SHARED_WIDTH", SharedVideoPointerWidth));
    const LONG sourceHeight = static_cast<LONG>(getIntFromEnv("FNVXR_UI_SHARED_HEIGHT", SharedVideoPointerHeight));
    const LONG outputWidth = static_cast<LONG>(getIntFromEnv("FNVXR_UI_INPUT_WIDTH", width));
    const LONG outputHeight = static_cast<LONG>(getIntFromEnv("FNVXR_UI_INPUT_HEIGHT", height));
    if (sourceWidth <= 1 || sourceHeight <= 1 || outputWidth <= 1 || outputHeight <= 1)
        return point;

    if (pipBoyVisibleFromMenuBits(currentMenuBits())
        && envEnabled("FNVXR_PIPBOY_POINTER_CANONICAL_GRID", false))
    {
        POINT pipBoyPoint {};
        pipBoyPoint.x = std::clamp(point.x, 0L, sourceWidth - 1);
        pipBoyPoint.y = std::clamp(point.y, 0L, sourceHeight - 1);
        return pipBoyPoint;
    }

    POINT mapped {};
    mapped.x = std::clamp(static_cast<LONG>(
                              (static_cast<double>(point.x) * static_cast<double>(outputWidth))
                              / static_cast<double>(sourceWidth)),
        0L,
        outputWidth - 1);
    mapped.y = std::clamp(static_cast<LONG>(
                              (static_cast<double>(point.y) * static_cast<double>(outputHeight))
                              / static_cast<double>(sourceHeight)),
        0L,
        outputHeight - 1);
    return mapped;
}

bool sendForegroundKey(WORD virtualKey)
{
    if (!currentProcessHasForegroundWindow())
        return false;

    INPUT inputs[2] {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = virtualKey;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = virtualKey;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    return SendInput(2, inputs, sizeof(INPUT)) == 2;
}

bool ensureClickForeground(HWND hwnd)
{
    if (!hwnd)
        return false;

    if (currentProcessHasForegroundWindow())
        return true;

    if (!envEnabled("FNVXR_CLICK_FOCUS_ON_CLICK", false))
        return false;

    static UInt32 focusRepairLogCount = 0;
    const bool focused = focusProcessWindow(hwnd);
    if (focusRepairLogCount < 12)
    {
        ++focusRepairLogCount;
        logTelemetry(
            "click focus repair hwnd=%p focused=%d foreground=%d\n",
            hwnd,
            static_cast<int>(focused),
            static_cast<int>(currentProcessHasForegroundWindow()));
    }
    return focused;
}

void logClickWindow(HWND hwnd, POINT clientPoint)
{
    if (!hwnd)
    {
        logTelemetry("clickWindow hwnd=null\n");
        return;
    }

    RECT client {};
    RECT window {};
    POINT cursor {};
    char title[128] {};
    char className[64] {};
    GetClientRect(hwnd, &client);
    GetWindowRect(hwnd, &window);
    GetCursorPos(&cursor);
    GetWindowTextA(hwnd, title, sizeof(title));
    GetClassNameA(hwnd, className, sizeof(className));
    logTelemetry(
        "clickWindow hwnd=%p title='%s' class='%s' client=(%ld,%ld %ldx%ld) window=(%ld,%ld %ldx%ld) point=(%ld,%ld) cursorScreen=(%ld,%ld) foreground=%d\n",
        hwnd,
        title,
        className,
        client.left,
        client.top,
        client.right - client.left,
        client.bottom - client.top,
        window.left,
        window.top,
        window.right - window.left,
        window.bottom - window.top,
        clientPoint.x,
        clientPoint.y,
        cursor.x,
        cursor.y,
        static_cast<int>(currentProcessHasForegroundWindow()));
}

bool sendForegroundMouseClickAt(HWND hwnd, POINT clientPoint)
{
    clientPoint = mapSharedPointerToWindow(hwnd, clientPoint);
    logClickWindow(hwnd, clientPoint);
    if (!ensureClickForeground(hwnd))
        return false;

    POINT screenPoint = clientPoint;
    if (!ClientToScreen(hwnd, &screenPoint))
        return false;

    if (envEnabled("FNVXR_CLICK_CLEAR_CLIP", true))
        ClipCursor(nullptr);

    const bool moved = SetCursorPos(screenPoint.x, screenPoint.y) != FALSE;

    INPUT inputs[2] {};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    const bool clicked = SendInput(2, inputs, sizeof(INPUT)) == 2;
    logTelemetry(
        "sendInputMouse client=(%ld,%ld) screen=(%ld,%ld) moved=%d clicked=%d foreground=%d\n",
        clientPoint.x,
        clientPoint.y,
        screenPoint.x,
        screenPoint.y,
        static_cast<int>(moved),
        static_cast<int>(clicked),
        static_cast<int>(currentProcessHasForegroundWindow()));
    return moved && clicked;
}

bool postWindowMouseClick(HWND hwnd, POINT clientPoint)
{
    if (!hwnd)
        return false;

    clientPoint = mapSharedPointerToWindow(hwnd, clientPoint);
    const LPARAM point =
        MAKELPARAM(static_cast<WORD>(clientPoint.x), static_cast<WORD>(clientPoint.y));
    const BOOL moved = PostMessageA(hwnd, WM_MOUSEMOVE, 0, point);
    const BOOL down = PostMessageA(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, point);
    const BOOL up = PostMessageA(hwnd, WM_LBUTTONUP, 0, point);
    logTelemetry(
        "postMouse client=(%ld,%ld) moved=%d down=%d up=%d foreground=%d\n",
        clientPoint.x,
        clientPoint.y,
        static_cast<int>(moved != FALSE),
        static_cast<int>(down != FALSE),
        static_cast<int>(up != FALSE),
        static_cast<int>(currentProcessHasForegroundWindow()));
    return moved && down && up;
}

bool holdDirectInputKey(UInt32 keycode, bool held)
{
    if (keycode >= MaxDirectInputMacros)
        return false;

    if (g_publishedDirectInputHoldKnown[keycode]
        && g_publishedDirectInputHoldState[keycode] == held)
    {
        return true;
    }
    if (!g_publishedDirectInputHoldKnown[keycode] && !held)
    {
        g_publishedDirectInputHoldKnown[keycode] = true;
        g_publishedDirectInputHoldState[keycode] = false;
        return true;
    }
    if (g_publishedDirectInputHoldViaHook[keycode])
    {
        if (!g_directInputHook)
            return false;
        g_directInputHook->keys[keycode].hold = held;
        g_publishedDirectInputHoldState[keycode] = held;
        g_publishedDirectInputHoldViaHook[keycode] = held;
        return true;
    }
    const bool releasingQueuedHold =
        g_publishedDirectInputHoldKnown[keycode]
        && g_publishedDirectInputHoldState[keycode]
        && !held;

    bool published = false;
    if (keycode >= MouseButtonOffset && keycode < MouseButtonOffset + 8)
    {
        published = publishInputEvent(
            held ? fnvxr::shared::InputEventTypeMouseButtonDown : fnvxr::shared::InputEventTypeMouseButtonUp,
            keycode - MouseButtonOffset);
    }
    else
    {
        published = publishInputEvent(
            held ? fnvxr::shared::InputEventTypeKeyDown : fnvxr::shared::InputEventTypeKeyUp,
            keycode);
    }

    if (published)
    {
        g_publishedDirectInputHoldKnown[keycode] = true;
        g_publishedDirectInputHoldState[keycode] = held;
        g_publishedDirectInputHoldViaHook[keycode] = false;
        return true;
    }
    if (releasingQueuedHold)
        return false;
    if (!g_directInputHook)
        return false;

    g_directInputHook->keys[keycode].hold = held;
    g_publishedDirectInputHoldKnown[keycode] = true;
    g_publishedDirectInputHoldState[keycode] = held;
    g_publishedDirectInputHoldViaHook[keycode] = held;
    return true;
}

UInt32 directInputMacroKeyFromEnv(const char* name, UInt32 fallback)
{
    const int value = getIntFromEnv(name, static_cast<int>(fallback));
    if (value <= 0 || value >= static_cast<int>(MaxDirectInputMacros))
        return 0;
    return static_cast<UInt32>(value);
}

UInt32 favoriteSlotForKey(UInt32 key)
{
    switch (key)
    {
        case DIK_1: return 1;
        case DIK_2: return 2;
        case DIK_3: return 3;
        case DIK_4: return 4;
        case DIK_5: return 5;
        case DIK_6: return 6;
        case DIK_7: return 7;
        case DIK_8: return 8;
        default: return 0;
    }
}

UInt32 nextUiFavoriteAssignKey(bool utilitySlot)
{
    constexpr UInt32 utilityKeys[] = { DIK_1, DIK_3, DIK_4 };
    constexpr UInt32 weaponKeys[] = { DIK_5, DIK_6, DIK_7, DIK_8 };
    if (utilitySlot)
    {
        const UInt32 index = g_uiFavoriteUtilityAssignIndex
            % static_cast<UInt32>(sizeof(utilityKeys) / sizeof(utilityKeys[0]));
        g_uiFavoriteUtilityAssignIndex =
            (index + 1u) % static_cast<UInt32>(sizeof(utilityKeys) / sizeof(utilityKeys[0]));
        return utilityKeys[index];
    }

    const UInt32 index = g_uiFavoriteWeaponAssignIndex
        % static_cast<UInt32>(sizeof(weaponKeys) / sizeof(weaponKeys[0]));
    g_uiFavoriteWeaponAssignIndex =
        (index + 1u) % static_cast<UInt32>(sizeof(weaponKeys) / sizeof(weaponKeys[0]));
    return weaponKeys[index];
}

void releaseUiFavoriteAssignment(const char* source, UInt64 frame)
{
    if (!g_uiFavoriteAssignHeldKey)
        return;

    const UInt32 key = g_uiFavoriteAssignHeldKey;
    holdDirectInputKey(key, false);
    g_uiFavoriteAssignHeldKey = 0;
    g_uiFavoriteAssignReleaseMs = 0;
    g_uiFavoriteAssignClickMs = 0;
    g_uiFavoriteAssignClickPending = false;
    logTelemetry(
        "uiFavoriteAssign release frame=%llu source=%s slot=%lu key=0x%02lx\n",
        static_cast<unsigned long long>(frame),
        source ? source : "unknown",
        static_cast<unsigned long>(favoriteSlotForKey(key)),
        static_cast<unsigned long>(key));
}

void resetUiFavoriteAssignmentCycles(const char* source, UInt64 frame)
{
    g_uiFavoriteWeaponAssignIndex = 0;
    g_uiFavoriteUtilityAssignIndex = 0;
    logTelemetry(
        "uiFavoriteAssign reset frame=%llu source=%s weaponSlots=5,6,7,8 utilitySlots=1,3,4 reservedSlot=2\n",
        static_cast<unsigned long long>(frame),
        source ? source : "unknown");
}

void tickUiFavoriteAssignment(UInt64 frame, UInt32 menuBits)
{
    const bool pipBoyVisible = pipBoyVisibleFromMenuBits(menuBits);
    if (pipBoyVisible && !g_previousUiFavoritePipBoyVisible)
        resetUiFavoriteAssignmentCycles("pipboy-open", frame);

    if (!pipBoyVisible && g_previousUiFavoritePipBoyVisible)
        releaseUiFavoriteAssignment("pipboy-close", frame);

    g_previousUiFavoritePipBoyVisible = pipBoyVisible;
    if (!g_uiFavoriteAssignHeldKey)
        return;

    const UInt64 nowMs = GetTickCount64();
    if (g_uiFavoriteAssignClickPending && pipBoyVisible && nowMs >= g_uiFavoriteAssignClickMs)
    {
        const bool mouseTapped = tapDirectInputKey(MouseButtonOffset);
        g_uiFavoriteAssignClickPending = false;
        logTelemetry(
            "uiFavoriteAssign click frame=%llu slot=%lu key=0x%02lx mouseTap=%d pointer=%d client=(%ld,%ld)\n",
            static_cast<unsigned long long>(frame),
            static_cast<unsigned long>(favoriteSlotForKey(g_uiFavoriteAssignHeldKey)),
            static_cast<unsigned long>(g_uiFavoriteAssignHeldKey),
            static_cast<int>(mouseTapped),
            static_cast<int>(g_latestPointerValid.load()),
            static_cast<LONG>(g_latestPointerX.load()),
            static_cast<LONG>(g_latestPointerY.load()));
    }
    if (!pipBoyVisible || nowMs >= g_uiFavoriteAssignReleaseMs)
        releaseUiFavoriteAssignment("timer", frame);
}

const char* favoriteRoleForSlot(UInt32 slot)
{
    switch (slot)
    {
        case 1: return "stimpak";
        case 2: return "ammoSwapReserved";
        case 3: return "grenade";
        case 4: return "backup";
        case 5: return "combatA";
        case 6: return "combatB";
        case 7: return "combatX";
        case 8: return "combatY";
        default: return "unknown";
    }
}

const char* weaponClassName(UInt32 weaponClass)
{
    switch (weaponClass)
    {
        case fnvxr::shared::PlayerWeaponClassNone: return "none";
        case fnvxr::shared::PlayerWeaponClassUnarmed: return "unarmed";
        case fnvxr::shared::PlayerWeaponClassMelee: return "melee";
        case fnvxr::shared::PlayerWeaponClassRanged: return "ranged";
        case fnvxr::shared::PlayerWeaponClassThrown: return "thrown";
        case fnvxr::shared::PlayerWeaponClassUnknown:
        default:
            return "unknown";
    }
}

UInt32 weaponClassFromString(const char* value, UInt32 fallback)
{
    if (!value || !*value)
        return fallback;
    if (_stricmp(value, "none") == 0)
        return fnvxr::shared::PlayerWeaponClassNone;
    if (_stricmp(value, "unarmed") == 0 || _stricmp(value, "fist") == 0 || _stricmp(value, "fists") == 0)
        return fnvxr::shared::PlayerWeaponClassUnarmed;
    if (_stricmp(value, "melee") == 0)
        return fnvxr::shared::PlayerWeaponClassMelee;
    if (_stricmp(value, "ranged") == 0 || _stricmp(value, "gun") == 0 || _stricmp(value, "guns") == 0)
        return fnvxr::shared::PlayerWeaponClassRanged;
    if (_stricmp(value, "thrown") == 0 || _stricmp(value, "grenade") == 0 || _stricmp(value, "explosive") == 0)
        return fnvxr::shared::PlayerWeaponClassThrown;
    return fallback;
}

UInt32 favoriteSlotDefaultWeaponClass(UInt32 slot)
{
    switch (slot)
    {
        case 3: return fnvxr::shared::PlayerWeaponClassThrown;
        case 4: return fnvxr::shared::PlayerWeaponClassUnarmed;
        case 5:
        case 6:
        case 7:
        case 8:
            return fnvxr::shared::PlayerWeaponClassRanged;
        default:
            return fnvxr::shared::PlayerWeaponClassUnknown;
    }
}

UInt32 favoriteSlotDefaultFormId(UInt32 slot)
{
    switch (slot)
    {
        case 3: return 0x00004330; // Frag Grenade in our deterministic test loadout.
        case 4: return 0x00004347; // Power Fist, an unarmed-skill weapon.
        case 5: return 0x000E3778; // 9mm Pistol.
        case 6: return 0x000E9C3B; // Service Rifle.
        case 7: return 0x000CD53A; // Caravan Shotgun.
        case 8: return 0x0008F21A; // Cowboy Repeater.
        default:
            return 0;
    }
}

UInt32 favoriteSlotWeaponClass(UInt32 slot)
{
    char envName[64] {};
    std::snprintf(envName, sizeof(envName), "FNVXR_FAVORITE_SLOT_%lu_WEAPON_CLASS", static_cast<unsigned long>(slot));
    char value[32] {};
    const DWORD actualLength = GetEnvironmentVariableA(envName, value, static_cast<DWORD>(sizeof(value)));
    const UInt32 fallback = favoriteSlotDefaultWeaponClass(slot);
    if (actualLength == 0 || actualLength >= sizeof(value))
        return fallback;
    return weaponClassFromString(value, fallback);
}

UInt32 favoriteSlotWeaponFormId(UInt32 slot)
{
    char envName[64] {};
    std::snprintf(envName, sizeof(envName), "FNVXR_FAVORITE_SLOT_%lu_FORM_ID", static_cast<unsigned long>(slot));
    char value[32] {};
    const DWORD length = GetEnvironmentVariableA(envName, value, static_cast<DWORD>(sizeof(value)));
    if (length == 0 || length >= sizeof(value))
        return favoriteSlotDefaultFormId(slot);

    char* end = nullptr;
    const unsigned long parsed = std::strtoul(value, &end, 0);
    return end && *end == '\0' ? static_cast<UInt32>(parsed) : favoriteSlotDefaultFormId(slot);
}

bool weaponClassKnown(UInt32 weaponClass)
{
    return weaponClass != fnvxr::shared::PlayerWeaponClassUnknown;
}

bool weaponClassMeleeOrUnarmed(UInt32 weaponClass)
{
    return weaponClass == fnvxr::shared::PlayerWeaponClassUnarmed
        || weaponClass == fnvxr::shared::PlayerWeaponClassMelee;
}

UInt32 currentWeaponClass()
{
    return g_lastKnownWeaponClass;
}

bool currentWeaponClassKnown()
{
    return weaponClassKnown(currentWeaponClass());
}

bool currentWeaponClassMeleeOrUnarmed()
{
    return weaponClassMeleeOrUnarmed(currentWeaponClass());
}

void noteFavoriteWeaponSelection(UInt32 key, const char* source, UInt64 frame)
{
    const UInt32 slot = favoriteSlotForKey(key);
    const UInt32 weaponClass = favoriteSlotWeaponClass(slot);
    if (!weaponClassKnown(weaponClass))
        return;

    const UInt32 formId = favoriteSlotWeaponFormId(slot);
    if (weaponClass == g_lastKnownWeaponClass
        && slot == g_lastKnownWeaponFavoriteSlot
        && formId == g_lastKnownWeaponFormId)
    {
        return;
    }

    g_lastKnownWeaponClass = weaponClass;
    g_lastKnownWeaponFavoriteSlot = slot;
    g_lastKnownWeaponFormId = formId;
    logTelemetry(
        "weaponClass selected frame=%llu source=%s slot=%lu key=0x%02lx class=%s formId=0x%08lx\n",
        static_cast<unsigned long long>(frame),
        source ? source : "unknown",
        static_cast<unsigned long>(slot),
        static_cast<unsigned long>(key),
        weaponClassName(weaponClass),
        static_cast<unsigned long>(formId));
}

bool assignUiFavoriteSlotKey(const char* source, UInt64 frame, UInt32 key, const char* role)
{
    if (!envEnabled("FNVXR_PIPBOY_Y_ASSIGN_FAVORITE", true))
    {
        logTelemetry(
            "uiFavoriteAssign ignore frame=%llu source=%s disabled=1\n",
            static_cast<unsigned long long>(frame),
            source ? source : "unknown");
        return false;
    }

    releaseUiFavoriteAssignment("replace", frame);
    const UInt32 slot = favoriteSlotForKey(key);
    if (!slot)
    {
        logTelemetry(
            "uiFavoriteAssign ignore frame=%llu source=%s invalidKey=0x%02lx role=%s\n",
            static_cast<unsigned long long>(frame),
            source ? source : "unknown",
            static_cast<unsigned long>(key),
            role ? role : "unknown");
        return false;
    }
    if (slot == 2)
    {
        logTelemetry(
            "uiFavoriteAssign reserved frame=%llu source=%s slot=2 key=0x%02lx role=%s reason=ammoSwap\n",
            static_cast<unsigned long long>(frame),
            source ? source : "unknown",
            static_cast<unsigned long>(key),
            role ? role : favoriteRoleForSlot(slot));
        return false;
    }

    const int holdMs = std::clamp(getIntFromEnv("FNVXR_UI_FAVORITE_ASSIGN_HOLD_MS", 900), 80, 1500);
    const int clickDelayMs = std::clamp(getIntFromEnv("FNVXR_UI_FAVORITE_ASSIGN_CLICK_DELAY_MS", 75), 0, 300);
    const UInt64 nowMs = GetTickCount64();
    g_uiFavoriteAssignHeldKey = key;
    g_uiFavoriteAssignReleaseMs = nowMs + static_cast<UInt64>(holdMs);
    g_uiFavoriteAssignClickMs = nowMs + static_cast<UInt64>(clickDelayMs);
    g_uiFavoriteAssignClickPending = true;
    const bool keyHeld = holdDirectInputKey(key, true);
    logTelemetry(
        "uiFavoriteAssign fire frame=%llu source=%s slot=%lu key=0x%02lx role=%s holdMs=%d clickDelayMs=%d keyHeld=%d pendingClick=1 pointer=%d client=(%ld,%ld) weaponNextSlot=%lu utilityNextSlot=%lu\n",
        static_cast<unsigned long long>(frame),
        source ? source : "unknown",
        static_cast<unsigned long>(slot),
        static_cast<unsigned long>(key),
        role ? role : favoriteRoleForSlot(slot),
        holdMs,
        clickDelayMs,
        static_cast<int>(keyHeld),
        static_cast<int>(g_latestPointerValid.load()),
        static_cast<LONG>(g_latestPointerX.load()),
        static_cast<LONG>(g_latestPointerY.load()),
        static_cast<unsigned long>(g_uiFavoriteWeaponAssignIndex + 5u),
        static_cast<unsigned long>(g_uiFavoriteUtilityAssignIndex == 0 ? 1u : (g_uiFavoriteUtilityAssignIndex == 1 ? 3u : 4u)));
    return keyHeld;
}

bool assignUiFavoriteSlot(const char* source, UInt64 frame, bool utilitySlot)
{
    const UInt32 key = nextUiFavoriteAssignKey(utilitySlot);
    const UInt32 slot = favoriteSlotForKey(key);
    return assignUiFavoriteSlotKey(
        source,
        frame,
        key,
        favoriteRoleForSlot(slot));
}

UInt32 gameplayRunModifierKey()
{
    return directInputMacroKeyFromEnv("FNVXR_GAMEPLAY_RUN_MODIFIER_DIK", DIK_LSHIFT);
}

UInt32 gameplayGrabKey()
{
    return directInputMacroKeyFromEnv("FNVXR_GAMEPLAY_GRAB_DIK", DIK_Z);
}

void holdGameplayGrab(bool held)
{
    const UInt32 key = gameplayGrabKey();
    if (key)
        holdDirectInputKey(key, held);
}

void holdGameplayRunModifier(bool held)
{
    const UInt32 key = gameplayRunModifierKey();
    if (key)
        holdDirectInputKey(key, held);
}

bool pluginKeyboardMovementEnabled()
{
    return envEnabled("FNVXR_PLUGIN_KEYBOARD_MOVEMENT_ENABLE", false);
}

bool pluginMenuKeyboardFallbackEnabled()
{
    return envEnabled("FNVXR_PLUGIN_MENU_KEYBOARD_FALLBACK", false);
}

bool pluginGameplayKeyboardFallbackEnabled()
{
    return envEnabled("FNVXR_PLUGIN_GAMEPLAY_KEYBOARD_FALLBACK", false);
}

void publishGameplayMovementFlags()
{
    if (g_xinputState
        && g_xinputState->magic == XInputSharedMagic
        && g_xinputState->version == XInputSharedVersion)
    {
        // These bytes are plugin-owned status metadata; the input producer and
        // its sequence guard intentionally do not mutate them after startup.
        InterlockedExchange8(
            reinterpret_cast<volatile char*>(&g_xinputState->reserved[fnvxr::shared::XInputReservedAutoRun]),
            g_gameplayAutoRunEnabled ? 1 : 0);
        InterlockedExchange8(
            reinterpret_cast<volatile char*>(&g_xinputState->reserved[fnvxr::shared::XInputReservedMovementMode]),
            static_cast<char>(g_gameplayMovementMode));
    }
}

float gameplayAnalogRunThreshold()
{
    return std::clamp(getFloatFromEnv("FNVXR_GAMEPLAY_ANALOG_RUN_THRESHOLD", 0.92f), 0.0f, 1.0f);
}

bool gameplayAnalogRunHeld(float leftThumbY)
{
    return envEnabled("FNVXR_GAMEPLAY_ANALOG_RUN_ENABLE", false)
        && leftThumbY >= gameplayAnalogRunThreshold();
}

bool gameplayAnalogRunHeld(std::int16_t leftThumbY)
{
    return gameplayAnalogRunHeld(static_cast<float>(leftThumbY) / 32767.0f);
}

void setGameplayAutoRun(bool enabled, const char* source, UInt64 frame)
{
    if (g_gameplayAutoRunEnabled == enabled)
    {
        publishGameplayMovementFlags();
        return;
    }

    g_gameplayAutoRunEnabled = enabled;
    publishGameplayMovementFlags();
    logTelemetry(
        "gameplayAutoRun state=%d frame=%llu source=%s\n",
        static_cast<int>(g_gameplayAutoRunEnabled),
        static_cast<unsigned long long>(frame),
        source ? source : "unknown");
}

void setGameplayMovementMode(UInt8 mode, const char* source, UInt64 frame)
{
    mode = static_cast<UInt8>(std::min<int>(mode, 2));
    const bool changed =
        g_gameplayMovementMode != mode
        || g_gameplayWalkModeEnabled != (mode == 1)
        || g_gameplayRunModeEnabled != (mode == 2);

    g_gameplayMovementMode = mode;
    g_gameplayWalkModeEnabled = mode == 1;
    g_gameplayRunModeEnabled = mode == 2;
    if (mode != 0)
        setGameplayAutoRun(false, source, frame);
    publishGameplayMovementFlags();

    holdGameplayRunModifier(
        pluginKeyboardMovementEnabled()
        && envEnabled("FNVXR_GAMEPLAY_RUN_BUTTON_ENABLE", true)
        && g_gameplayRunModeEnabled);

    if (!changed)
        return;

    logTelemetry(
        "gameplayMovementMode mode=%u walk=%d run=%d autoRun=%d frame=%llu source=%s\n",
        static_cast<unsigned int>(g_gameplayMovementMode),
        static_cast<int>(g_gameplayWalkModeEnabled),
        static_cast<int>(g_gameplayRunModeEnabled),
        static_cast<int>(g_gameplayAutoRunEnabled),
        static_cast<unsigned long long>(frame),
        source ? source : "unknown");
}

void cycleGameplayMovementMode(const char* source, UInt64 frame)
{
    const UInt64 nowMs = GetTickCount64();
    if (nowMs < g_lastGameplayMovementModeToggleMs + 150)
        return;

    g_lastGameplayMovementModeToggleMs = nowMs;
    setGameplayMovementMode(static_cast<UInt8>((g_gameplayMovementMode + 1u) % 3u), source, frame);
}

void toggleGameplayAutoRun(const char* source, UInt64 frame)
{
    const UInt64 nowMs = GetTickCount64();
    if (nowMs < g_lastGameplayAutoRunToggleMs + 150)
        return;

    g_lastGameplayAutoRunToggleMs = nowMs;
    if (!g_gameplayAutoRunEnabled)
        setGameplayMovementMode(0, source, frame);
    setGameplayAutoRun(!g_gameplayAutoRunEnabled, source, frame);
}

void setGameplayRunMode(bool enabled, const char* source, UInt64 frame)
{
    if (g_gameplayRunModeEnabled == enabled && g_gameplayMovementMode == (enabled ? 2 : 0))
        return;

    setGameplayMovementMode(enabled ? 2u : 0u, source, frame);
    logTelemetry(
        "gameplayRunMode state=%d frame=%llu source=%s runKey=0x%02lx\n",
        static_cast<int>(g_gameplayRunModeEnabled),
        static_cast<unsigned long long>(frame),
        source ? source : "unknown",
        static_cast<unsigned long>(gameplayRunModifierKey()));
}

void toggleGameplayRunMode(const char* source, UInt64 frame)
{
    const UInt64 nowMs = GetTickCount64();
    if (nowMs < g_lastGameplayRunModeToggleMs + 150)
        return;

    g_lastGameplayRunModeToggleMs = nowMs;
    setGameplayRunMode(!g_gameplayRunModeEnabled, source, frame);
}

void tapAccept()
{
    tapDirectInputKey(DIK_RETURN);
    tapDirectInputKey(DIK_E);
    tapDirectInputKey(MouseButtonOffset);
}

void releaseControllerHolds()
{
    setGameplayAutoRun(false, "releaseControllerHolds", 0);
    setGameplayMovementMode(0, "releaseControllerHolds", 0);
    releaseUiFavoriteAssignment("releaseControllerHolds", 0);
    holdDirectInputKey(DIK_R, false);
    holdGameplayGrab(false);
    holdDirectInputKey(DIK_W, false);
    holdDirectInputKey(DIK_A, false);
    holdDirectInputKey(DIK_S, false);
    holdDirectInputKey(DIK_D, false);
    holdDirectInputKey(DIK_UP, false);
    holdDirectInputKey(DIK_LEFT, false);
    holdDirectInputKey(DIK_DOWN, false);
    holdDirectInputKey(DIK_RIGHT, false);
    holdGameplayRunModifier(false);
}

void updateControllerAxes(const fnvxr::PoseFrame& pose)
{
    constexpr float deadzone = 0.35f;
    const bool leftMove = pose.leftThumbstickX < -deadzone;
    const bool rightMove = pose.leftThumbstickX > deadzone;
    const bool downMove = pose.leftThumbstickY < -deadzone;
    const bool upMove = pose.leftThumbstickY > deadzone;
    const bool leftTurn = pose.rightThumbstickX < -deadzone;
    const bool rightTurn = pose.rightThumbstickX > deadzone;
    const bool analogRun = gameplayAnalogRunHeld(pose.leftThumbstickY);
    const bool uiInputAllowed = allowUiInput();
    const bool keyboardMovement = pluginKeyboardMovementEnabled();

    if (!keyboardMovement)
    {
        holdDirectInputKey(DIK_W, false);
        holdDirectInputKey(DIK_A, false);
        holdDirectInputKey(DIK_S, false);
        holdDirectInputKey(DIK_D, false);
        holdDirectInputKey(DIK_UP, false);
        holdDirectInputKey(DIK_LEFT, false);
        holdDirectInputKey(DIK_DOWN, false);
        holdDirectInputKey(DIK_RIGHT, false);
        holdGameplayRunModifier(false);
        static bool loggedAnalogPrimary = false;
        if (!loggedAnalogPrimary || (pose.frame % 600) == 0)
        {
            loggedAnalogPrimary = true;
            logTelemetry(
                "gameplayMovement analogPrimary=1 pluginKeyboardMovement=0 mode=%u walk=%d run=%d autoRun=%d ls=(%.3f,%.3f)\n",
                static_cast<unsigned int>(g_gameplayMovementMode),
                static_cast<int>(g_gameplayWalkModeEnabled),
                static_cast<int>(g_gameplayRunModeEnabled),
                static_cast<int>(g_gameplayAutoRunEnabled),
                pose.leftThumbstickX,
                pose.leftThumbstickY);
        }
        return;
    }

    if (uiInputAllowed)
    {
        holdDirectInputKey(DIK_W, false);
        holdDirectInputKey(DIK_A, false);
        holdDirectInputKey(DIK_S, false);
        holdDirectInputKey(DIK_D, false);
        holdDirectInputKey(DIK_UP, upMove);
        holdDirectInputKey(DIK_LEFT, leftMove);
        holdDirectInputKey(DIK_DOWN, downMove);
        holdDirectInputKey(DIK_RIGHT, rightMove);
        holdGameplayRunModifier(false);
        holdDirectInputKey(DIK_W, false);
        return;
    }

    const bool autoRunForward = g_gameplayAutoRunEnabled && !downMove;
    holdDirectInputKey(DIK_W, upMove || autoRunForward);
    holdDirectInputKey(DIK_A, leftMove);
    holdDirectInputKey(DIK_S, downMove);
    holdDirectInputKey(DIK_D, rightMove);
    holdDirectInputKey(DIK_UP, false);
    holdDirectInputKey(DIK_DOWN, false);
    holdDirectInputKey(DIK_LEFT, envEnabled("FNVXR_RIGHT_STICK_KEY_TURN", true) && leftTurn);
    holdDirectInputKey(DIK_RIGHT, envEnabled("FNVXR_RIGHT_STICK_KEY_TURN", true) && rightTurn);
    const bool runModifierHeld =
        envEnabled("FNVXR_GAMEPLAY_RUN_BUTTON_ENABLE", true)
        && (g_gameplayRunModeEnabled || analogRun);
    holdGameplayRunModifier(runModifierHeld);

    static bool wasRunModifierHeld = false;
    static bool wasAutoRunForward = false;
    if (runModifierHeld != wasRunModifierHeld || autoRunForward != wasAutoRunForward)
    {
        wasRunModifierHeld = runModifierHeld;
        wasAutoRunForward = autoRunForward;
        logTelemetry(
            "gameplayRun state=%d autoForward=%d analogRun=%d runMode=%d autoRun=%d leftY=%.3f threshold=%.3f runKey=0x%02lx\n",
            static_cast<int>(runModifierHeld),
            static_cast<int>(autoRunForward),
            static_cast<int>(analogRun),
            static_cast<int>(g_gameplayRunModeEnabled),
            static_cast<int>(g_gameplayAutoRunEnabled),
            pose.leftThumbstickY,
            gameplayAnalogRunThreshold(),
            static_cast<unsigned long>(gameplayRunModifierKey()));
    }

    static bool wasTurning = false;
    const bool turning = leftTurn || rightTurn;
    if (turning != wasTurning)
    {
        wasTurning = turning;
        logTelemetry(
            "rightStickTurn active=%d rs=(%.3f,%.3f) leftKey=%d rightKey=%d\n",
            static_cast<int>(turning),
            pose.rightThumbstickX,
            pose.rightThumbstickY,
            static_cast<int>(leftTurn),
            static_cast<int>(rightTurn));
    }
}

HWND currentProcessWindow()
{
    HWND foreground = GetForegroundWindow();
    if (!foreground)
        return nullptr;

    DWORD processId = 0;
    GetWindowThreadProcessId(foreground, &processId);
    if (processId != GetCurrentProcessId())
        return nullptr;

    return foreground;
}

BOOL CALLBACK findCurrentProcessWindow(HWND hwnd, LPARAM outWindow)
{
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId() || !IsWindowVisible(hwnd))
        return TRUE;

    *reinterpret_cast<HWND*>(outWindow) = hwnd;
    return FALSE;
}

HWND gameWindow()
{
    if (HWND foreground = currentProcessWindow())
        return foreground;

    HWND found = nullptr;
    EnumWindows(findCurrentProcessWindow, reinterpret_cast<LPARAM>(&found));
    return found;
}

float clamp01(float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

float calibratedPointerAxis(float value, const char* scaleName, const char* offsetName)
{
    const float scale = getFloatFromEnv(scaleName, 1.0f);
    const float offset = getFloatFromEnv(offsetName, 0.0f);
    return clamp01((value - 0.5f) * scale + 0.5f + offset);
}

void updateMenuPointer(const fnvxr::PoseFrame& pose)
{
    g_hasMenuPointer = false;
    if (!pose.menuPointerActive)
    {
        g_latestPointerValid.store(false);
        return;
    }

    HWND hwnd = gameWindow();
    if (!hwnd)
    {
        g_latestPointerValid.store(false);
        return;
    }

    RECT client {};
    if (!GetClientRect(hwnd, &client))
    {
        g_latestPointerValid.store(false);
        return;
    }

    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0)
    {
        g_latestPointerValid.store(false);
        return;
    }
    const int sharedWidth = getIntFromEnv("FNVXR_UI_SHARED_WIDTH", SharedVideoPointerWidth);
    const int sharedHeight = getIntFromEnv("FNVXR_UI_SHARED_HEIGHT", SharedVideoPointerHeight);
    const int inputWidth = getIntFromEnv("FNVXR_UI_INPUT_WIDTH", width);
    const int inputHeight = getIntFromEnv("FNVXR_UI_INPUT_HEIGHT", height);

    const float pointerX = calibratedPointerAxis(
        pose.menuPointerX,
        "FNVXR_POINTER_SCALE_X",
        "FNVXR_POINTER_OFFSET_X");
    const float pointerY = calibratedPointerAxis(
        pose.menuPointerY,
        "FNVXR_POINTER_SCALE_Y",
        "FNVXR_POINTER_OFFSET_Y");
    g_lastMenuPointerClient = {
        static_cast<LONG>(pointerX * static_cast<float>(sharedWidth - 1)),
        static_cast<LONG>(pointerY * static_cast<float>(sharedHeight - 1))
    };
    g_hasMenuPointer = true;
    g_latestPointerX.store(g_lastMenuPointerClient.x);
    g_latestPointerY.store(g_lastMenuPointerClient.y);
    g_latestPointerFrame.store(pose.frame);
    g_latestPointerValid.store(true);
    ++g_loggedPointerFrames;
    if (g_loggedPointerFrames <= 20 || g_loggedPointerFrames % 60 == 0)
    {
        logTelemetry(
            "pointer frame=%llu norm=(%.4f,%.4f) mapped=(%.4f,%.4f) client=(%ld,%ld) size=%dx%d sharedSize=%dx%d inputSize=%dx%d hwnd=%p di=%p\n",
            static_cast<unsigned long long>(pose.frame),
            pose.menuPointerX,
            pose.menuPointerY,
            pointerX,
            pointerY,
            g_lastMenuPointerClient.x,
            g_lastMenuPointerClient.y,
            width,
            height,
            sharedWidth,
            sharedHeight,
            inputWidth,
            inputHeight,
            hwnd,
            g_directInputHook);
    }

    // The DInput proxy is the sole production pointer owner. The plugin only
    // publishes the requested canonical coordinate; duplicating it through
    // WM_MOUSEMOVE or cursor-tile writes lets the visible cursor diverge from
    // the native DirectInput hit-test position.
    const POINT windowPointer = mapSharedPointerToWindow(hwnd, g_lastMenuPointerClient);

    if (envEnabled("FNVXR_CURSOR_TRACK_POINTER", false))
    {
        POINT screenPoint = windowPointer;
        if (ClientToScreen(hwnd, &screenPoint))
        {
            if (envEnabled("FNVXR_CURSOR_FOCUS", false))
                SetForegroundWindow(hwnd);
            SetCursorPos(screenPoint.x, screenPoint.y);
        }
    }
}

void executeAcceptClickOnGameThread()
{
    const UInt32 menuBits = currentMenuBits();
    const bool pipBoyPointerPath = (menuBits & (1u << 6)) != 0;
    if (!uiInputAllowedFromMenuBits(menuBits))
    {
        logTelemetry("click ignored outside ui/loading-safe state\n");
        return;
    }

    g_hasMenuPointer = g_latestPointerValid.load();
    if (g_hasMenuPointer)
    {
        g_lastMenuPointerClient.x = g_latestPointerX.load();
        g_lastMenuPointerClient.y = g_latestPointerY.load();
    }

    HWND hwnd = g_hasMenuPointer ? gameWindow() : nullptr;
    const bool focusedForClick = hwnd ? ensureClickForeground(hwnd) : currentProcessHasForegroundWindow();
    const bool uiClick = dispatchActiveMenuClick();
    if (retailSidecarProfile() && g_hasMenuPointer && !uiClick)
    {
        if (!hwnd)
            return;

        const bool diMouseTap = tapDirectInputKey(MouseButtonOffset);
        const bool postedMouse = envEnabled("FNVXR_POST_WINDOW_MOUSE_FALLBACK", false)
            && postWindowMouseClick(hwnd, g_lastMenuPointerClient);
        logTelemetry(
            "sidecar shared pointer click raw=(%ld,%ld) menuBits=0x%02lx pipboy=%d diMouseTap=%d postedMouse=%d focused=%d foreground=%d\n",
            g_lastMenuPointerClient.x,
            g_lastMenuPointerClient.y,
            static_cast<unsigned long>(menuBits),
            static_cast<int>(pipBoyPointerPath),
            static_cast<int>(diMouseTap),
            static_cast<int>(postedMouse),
            static_cast<int>(focusedForClick),
            static_cast<int>(currentProcessHasForegroundWindow()));
        return;
    }

    const bool allowLegacyFallback =
        !uiClick || envEnabled("FNVXR_CLICK_LEGACY_FALLBACK_AFTER_DIRECT", false);
    const bool sendInputMouse = allowLegacyFallback
        && envEnabled("FNVXR_CLICK_SENDINPUT_MOUSE", false)
        && sendForegroundMouseClickAt(hwnd, g_lastMenuPointerClient);
    logTelemetry(
        "click hasPointer=%d client=(%ld,%ld) menuBits=0x%02lx pipboy=%d focused=%d uiClick=%d sendInputMouse=%d di=%p foreground=%d\n",
        static_cast<int>(g_hasMenuPointer),
        g_lastMenuPointerClient.x,
        g_lastMenuPointerClient.y,
        static_cast<unsigned long>(menuBits),
        static_cast<int>(pipBoyPointerPath),
        static_cast<int>(focusedForClick),
        static_cast<int>(uiClick),
        static_cast<int>(sendInputMouse),
        g_directInputHook,
        static_cast<int>(currentProcessHasForegroundWindow()));

    if (g_hasMenuPointer && allowLegacyFallback)
    {
        HWND hwnd = gameWindow();
        if (!hwnd)
            return;

        const bool diMouseTap = tapDirectInputKey(MouseButtonOffset);

        const POINT windowPointer = mapSharedPointerToWindow(hwnd, g_lastMenuPointerClient);
        BOOL postDown = FALSE;
        BOOL postUp = FALSE;
        if (envEnabled("FNVXR_POST_WINDOW_MOUSE_FALLBACK", false))
        {
            const LPARAM point =
                MAKELPARAM(static_cast<WORD>(windowPointer.x), static_cast<WORD>(windowPointer.y));
            postDown = PostMessageA(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, point);
            postUp = PostMessageA(hwnd, WM_LBUTTONUP, 0, point);
        }
        logTelemetry(
            "click pointer dispatch raw=(%ld,%ld) mapped=(%ld,%ld) diMouseTap=%d postDown=%d postUp=%d foreground=%d\n",
            g_lastMenuPointerClient.x,
            g_lastMenuPointerClient.y,
            windowPointer.x,
            windowPointer.y,
            static_cast<int>(diMouseTap),
            static_cast<int>(postDown != FALSE),
            static_cast<int>(postUp != FALSE),
            static_cast<int>(currentProcessHasForegroundWindow()));

        POINT screenPoint = windowPointer;
        if (envEnabled("FNVXR_CURSOR_TRACK_POINTER", false) && ClientToScreen(hwnd, &screenPoint))
        {
            if (envEnabled("FNVXR_CURSOR_FOCUS", false))
                SetForegroundWindow(hwnd);
            SetCursorPos(screenPoint.x, screenPoint.y);
        }

        if (envEnabled("FNVXR_PLUGIN_SENDINPUT_CLICK", false) && currentProcessHasForegroundWindow())
        {
            INPUT inputs[2] {};
            inputs[0].type = INPUT_MOUSE;
            inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            inputs[1].type = INPUT_MOUSE;
            inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput(2, inputs, sizeof(INPUT));
        }
        return;
    }

    if (retailSidecarProfile())
    {
        logTelemetry("sidecar click skipped: no active shared pointer uiClick=%d\n", static_cast<int>(uiClick));
        return;
    }

    if (!currentProcessHasForegroundWindow())
        return;

    INPUT inputs[2] {};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(2, inputs, sizeof(INPUT));
}

void requestAcceptClick()
{
    const UInt32 previous = g_pendingAcceptClicks.fetch_add(1);
    const UInt32 menuBits = currentMenuBits();
    logTelemetry(
        "click request pending=%u hasPointer=%d client=(%ld,%ld) menuBits=0x%02lx pipboy=%d\n",
        previous + 1,
        static_cast<int>(g_latestPointerValid.load()),
        g_latestPointerX.load(),
        g_latestPointerY.load(),
        static_cast<unsigned long>(menuBits),
        static_cast<int>((menuBits & (1u << 6)) != 0));
}

void executeImmediateInputClick()
{
    if (!envEnabled("FNVXR_IMMEDIATE_OS_CLICK", false))
        return;
    if (!g_hasMenuPointer)
        return;

    HWND hwnd = gameWindow();
    if (!hwnd)
        return;

    const bool postMouse = postWindowMouseClick(hwnd, g_lastMenuPointerClient);
    const bool sendInputMouse =
        envEnabled("FNVXR_CLICK_SENDINPUT_MOUSE", false) && sendForegroundMouseClickAt(hwnd, g_lastMenuPointerClient);
    logTelemetry(
        "immediateClick client=(%ld,%ld) postMouse=%d sendInputMouse=%d foreground=%d\n",
        g_lastMenuPointerClient.x,
        g_lastMenuPointerClient.y,
        static_cast<int>(postMouse),
        static_cast<int>(sendInputMouse),
        static_cast<int>(currentProcessHasForegroundWindow()));
}

UInt64 nowMilliseconds()
{
    return GetTickCount64();
}

bool showroomEnabled()
{
    if (!envEnabled("FNVXR_MENU_SCENE_CAROUSEL", false))
        return false;

    if (!envEnabled("FNVXR_EXPERIMENTAL_MUTATE_GAME_SCENE", false))
    {
        static bool loggedBlocked = false;
        if (!loggedBlocked)
        {
            loggedBlocked = true;
            logTelemetry(
                "{\"event\":\"fnvxrShowroomState\",\"active\":false,\"blocked\":true,\"reason\":\"menu-scene-mutation-disabled\"}\n");
        }
        return false;
    }

    return true;
}

bool runPluginConsoleCommand(const char* eventName, const char* command)
{
    if (!command || !*command)
        return true;

    if (!g_console || !g_console->RunScriptLine2)
    {
        logTelemetry(
            "{\"event\":\"%s\",\"ready\":false,\"reason\":\"missing-xnvse-console-interface\"}\n",
            eventName ? eventName : "fnvxrConsoleCommand");
        return false;
    }

    const bool ok = g_console->RunScriptLine2(command, nullptr, true);
    logTelemetry(
        "{\"event\":\"%s\",\"ok\":%s,\"command\":\"%s\"}\n",
        eventName ? eventName : "fnvxrConsoleCommand",
        ok ? "true" : "false",
        command);
    return ok;
}

bool ensureAutoVanityCameraDisabled(UInt64 frame)
{
    if (!strictFirstPersonEnabled()
        || !envEnabled("FNVXR_DISABLE_AUTO_VANITY_CAMERA", true))
    {
        return true;
    }

    static bool applied = false;
    static UInt64 lastAttemptMs = 0;
    if (applied)
        return true;
    const UInt64 nowMs = GetTickCount64();
    if (lastAttemptMs != 0 && nowMs < lastAttemptMs + 1000)
        return false;
    lastAttemptMs = nowMs;

    UInt32 previous = 0;
    UInt32 current = 0;
    bool writeCompleted = false;
    __try
    {
        // 0x011E09E0 is the verified retail Setting object for
        // bDisableAutoVanityMode:General; numeric Setting data is at +0x04.
        auto* value = pointerFromAddress32<volatile UInt32*>(
            DisableAutoVanityModeSettingAddress + 0x04);
        previous = *value;
        *value = 1;
        current = *value;
        writeCompleted = true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        writeCompleted = false;
    }
    applied = writeCompleted && current != 0;
    logTelemetry(
        "strictFirstPerson vanityDisabled frame=%llu applied=%d previous=%lu current=%lu\n",
        static_cast<unsigned long long>(frame),
        static_cast<int>(applied),
        static_cast<unsigned long>(previous),
        static_cast<unsigned long>(current));
    return applied;
}

void sanitizeCommandSaveName(const char* input, char* output, size_t outputSize)
{
    if (!output || outputSize == 0)
        return;

    output[0] = '\0';
    const char* source = (input && *input) ? input : "FNVXR_QuickSave";
    size_t written = 0;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(source); *p && written + 1 < outputSize; ++p)
    {
        const unsigned char ch = *p;
        if (std::isalnum(ch) || ch == '_' || ch == '-')
            output[written++] = static_cast<char>(ch);
        else if (ch == ' ' || ch == '.')
            output[written++] = '_';
    }
    output[written] = '\0';
    if (written == 0)
        strcpy_s(output, outputSize, "FNVXR_QuickSave");
}

bool buildSharedCommandLine(const SharedCommandState& request, char* command, size_t commandSize)
{
    if (!command || commandSize == 0)
        return false;

    command[0] = '\0';
    if (request.command == fnvxr::shared::CommandTypeSave)
    {
        char saveName[64] {};
        sanitizeCommandSaveName(request.saveName, saveName, sizeof(saveName));
        return sprintf_s(command, commandSize, "save %s", saveName) > 0;
    }
    if (request.command == fnvxr::shared::CommandTypeQuit)
        return strcpy_s(command, commandSize, "qqq") == 0;
    if (request.command == fnvxr::shared::CommandTypeConsole)
        return strcpy_s(command, commandSize, request.saveName) == 0;

    return false;
}

const char* sharedCommandName(UInt32 command)
{
    switch (command)
    {
        case fnvxr::shared::CommandTypeSave: return "save";
        case fnvxr::shared::CommandTypeQuit: return "quit";
        case fnvxr::shared::CommandTypeConsole: return "console";
        default: return "none";
    }
}

const char* sharedCommandStatusName(UInt32 status)
{
    switch (status)
    {
        case fnvxr::shared::CommandStatusIdle: return "idle";
        case fnvxr::shared::CommandStatusPending: return "pending";
        case fnvxr::shared::CommandStatusRunning: return "running";
        case fnvxr::shared::CommandStatusSucceeded: return "succeeded";
        case fnvxr::shared::CommandStatusFailed: return "failed";
        default: return "unknown";
    }
}

bool readSharedCommandSnapshot(SharedCommandState& snapshot)
{
    if (!g_commandState
        || g_commandState->magic != CommandSharedMagic
        || g_commandState->version != CommandSharedVersion)
        return false;

    for (int attempt = 0; attempt < 4; ++attempt)
    {
        const LONG begin = g_commandState->sequence;
        MemoryBarrier();
        std::memcpy(&snapshot, g_commandState, sizeof(snapshot));
        MemoryBarrier();
        const LONG end = g_commandState->sequence;
        if (begin == end && (end % 2) == 0)
            return snapshot.magic == CommandSharedMagic && snapshot.version == CommandSharedVersion;
        Sleep(0);
    }
    return false;
}

void publishSharedCommandStatus(
    UInt32 requestId,
    UInt32 status,
    UInt64 frame,
    UInt32 resultCode,
    const char* command)
{
    if (!g_commandState)
        return;

    InterlockedIncrement(&g_commandState->sequence);
    MemoryBarrier();
    g_commandState->magic = CommandSharedMagic;
    g_commandState->version = CommandSharedVersion;
    g_commandState->requestId = requestId;
    g_commandState->status = status;
    g_commandState->completedFrame = frame;
    g_commandState->resultCode = resultCode;
    if (command && *command)
        strcpy_s(g_commandState->lastCommand, command);
    MemoryBarrier();
    InterlockedIncrement(&g_commandState->sequence);
}

void consumeSharedCommand(UInt64 frame)
{
    SharedCommandState request {};
    if (!readSharedCommandSnapshot(request))
        return;

    if (request.requestId == 0 || request.requestId == g_lastCommandRequestId)
        return;

    if (request.status != fnvxr::shared::CommandStatusPending)
    {
        logTelemetry(
            "{\"event\":\"fnvxrSharedCommandSkip\",\"requestId\":%lu,\"command\":\"%s\",\"status\":\"%s\",\"frame\":%llu}\n",
            static_cast<unsigned long>(request.requestId),
            sharedCommandName(request.command),
            sharedCommandStatusName(request.status),
            static_cast<unsigned long long>(frame));
        g_lastCommandRequestId = request.requestId;
        return;
    }

    char command[96] {};
    const bool built = buildSharedCommandLine(request, command, sizeof(command));
    logTelemetry(
        "{\"event\":\"fnvxrSharedCommandRequest\",\"requestId\":%lu,\"command\":\"%s\",\"saveName\":\"%s\",\"built\":%s,\"line\":\"%s\",\"frame\":%llu}\n",
        static_cast<unsigned long>(request.requestId),
        sharedCommandName(request.command),
        request.saveName,
        built ? "true" : "false",
        built ? command : "",
        static_cast<unsigned long long>(frame));
    publishSharedCommandStatus(
        request.requestId,
        fnvxr::shared::CommandStatusRunning,
        frame,
        built ? 0u : 1u,
        built ? command : "");

    const bool ok = built && runPluginConsoleCommand("fnvxrSharedCommand", command);
    g_lastCommandRequestId = request.requestId;
    publishSharedCommandStatus(
        request.requestId,
        ok ? fnvxr::shared::CommandStatusSucceeded : fnvxr::shared::CommandStatusFailed,
        frame,
        ok ? 0u : 2u,
        built ? command : "");
    logTelemetry(
        "{\"event\":\"fnvxrSharedCommandComplete\",\"requestId\":%lu,\"command\":\"%s\",\"ok\":%s,\"resultCode\":%lu,\"line\":\"%s\",\"frame\":%llu}\n",
        static_cast<unsigned long>(request.requestId),
        sharedCommandName(request.command),
        ok ? "true" : "false",
        static_cast<unsigned long>(ok ? 0u : 2u),
        built ? command : "",
        static_cast<unsigned long long>(frame));
}

bool runShowroomCommand(const char* command)
{
    if (!command || !*command)
        return true;

    if (!g_console || !g_console->RunScriptLine2)
        g_showroomExecutorLogged = true;

    const bool ok = runPluginConsoleCommand("fnvxrShowroomCommand", command);
    ++g_showroomCommandSerial;
    return ok;
}

void lockShowroomControls()
{
    if (g_showroomControlsLocked)
        return;

    if (!envEnabled("FNVXR_SHOWROOM_LOCK_CONTROLS", false))
    {
        logTelemetry("{\"event\":\"fnvxrShowroomControls\",\"locked\":false,\"path\":\"menu-safe-default\"}\n");
        return;
    }

    if (g_playerControls && g_playerControls->DisablePlayerControlsAlt)
    {
        constexpr UInt32 disableMovementFightingActivationCameraSneak = 0x1u | 0x2u | 0x4u | 0x10u | 0x40u;
        g_playerControls->DisablePlayerControlsAlt(disableMovementFightingActivationCameraSneak, "FNVXR");
        logTelemetry("{\"event\":\"fnvxrShowroomControls\",\"locked\":true,\"path\":\"xnvse-player-controls\"}\n");
    }
    else
    {
        runShowroomCommand("DisablePlayerControls 1 1 1 0 1 0 1");
        logTelemetry("{\"event\":\"fnvxrShowroomControls\",\"locked\":true,\"path\":\"script-command\"}\n");
    }
    if (envEnabled("FNVXR_SHOWROOM_RESTRAIN_PLAYER", false))
        runShowroomCommand("player.SetRestrained 1");
    g_showroomControlsLocked = true;
}

void unlockShowroomControls()
{
    if (!g_showroomControlsLocked)
        return;

    if (g_playerControls && g_playerControls->EnablePlayerControlsAlt)
    {
        constexpr UInt32 disableMovementFightingActivationCameraSneak = 0x1u | 0x2u | 0x4u | 0x10u | 0x40u;
        g_playerControls->EnablePlayerControlsAlt(disableMovementFightingActivationCameraSneak, "FNVXR");
        logTelemetry("{\"event\":\"fnvxrShowroomControls\",\"locked\":false,\"path\":\"xnvse-player-controls\"}\n");
    }
    else
    {
        runShowroomCommand("EnablePlayerControls");
        logTelemetry("{\"event\":\"fnvxrShowroomControls\",\"locked\":false,\"path\":\"script-command\"}\n");
    }
    if (envEnabled("FNVXR_SHOWROOM_RESTRAIN_PLAYER", false))
        runShowroomCommand("player.SetRestrained 0");
    g_showroomControlsLocked = false;
}

void requestShowroomScene(UInt32 sceneIndex)
{
    if (sceneIndex >= static_cast<UInt32>(sizeof(kShowroomScenes) / sizeof(kShowroomScenes[0])))
        sceneIndex = 0;

    const ShowroomScene& scene = kShowroomScenes[sceneIndex];
    g_showroomSceneIndex = sceneIndex;
    g_showroomCellFormId = scene.expectedCellFormId;
    g_showroomPhase = ShowroomPhase::Loading;
    g_showroomNextActionMs = nowMilliseconds() + 350;
    logTelemetry(
        "{\"event\":\"fnvxrShowroomScene\",\"phase\":\"request\",\"index\":%lu,\"name\":\"%s\",\"load\":\"%s\",\"expectedCellFormId\":%lu}\n",
        static_cast<unsigned long>(sceneIndex),
        scene.name,
        scene.loadCommand,
        static_cast<unsigned long>(scene.expectedCellFormId));
}

void processShowroomCarousel()
{
    if (!showroomEnabled())
    {
        if (g_showroomActive)
        {
            unlockShowroomControls();
            logTelemetry("{\"event\":\"fnvxrShowroomState\",\"active\":false,\"reason\":\"disabled\"}\n");
        }
        g_showroomActive = false;
        g_showroomPhase = ShowroomPhase::Idle;
        return;
    }

    const UInt64 nowMs = nowMilliseconds();
    if (!g_showroomActive)
    {
        g_showroomActive = true;
        g_showroomPhase = ShowroomPhase::Idle;
        g_showroomSceneIndex = 0;
        g_showroomNextActionMs = nowMs + 1000;
        logTelemetry("{\"event\":\"fnvxrShowroomState\",\"active\":true,\"sceneCount\":3}\n");
    }

    if (nowMs < g_showroomNextActionMs)
        return;

    if (menuVisibleWithTile(kMenuTypeLoading)
        && g_showroomPhase != ShowroomPhase::Idle
        && g_showroomPhase != ShowroomPhase::Loading)
    {
        return;
    }

    const ShowroomScene& scene = kShowroomScenes[g_showroomSceneIndex];
    switch (g_showroomPhase)
    {
        case ShowroomPhase::Idle:
            requestShowroomScene(g_showroomSceneIndex);
            return;

        case ShowroomPhase::Loading:
            if (runShowroomCommand(scene.loadCommand))
            {
                g_showroomPhase = ShowroomPhase::PostLoad;
                g_showroomNextActionMs = nowMs + 2500;
                logTelemetry(
                    "{\"event\":\"fnvxrShowroomScene\",\"phase\":\"loading\",\"index\":%lu,\"name\":\"%s\"}\n",
                    static_cast<unsigned long>(g_showroomSceneIndex),
                    scene.name);
            }
            else
            {
                g_showroomNextActionMs = nowMs + 3000;
            }
            return;

        case ShowroomPhase::PostLoad:
            for (const char* command : scene.postCommands)
            {
                if (!command)
                    break;
                runShowroomCommand(command);
            }
            lockShowroomControls();
            if (!allowUiInput())
                tapDirectInputKey(DIK_ESCAPE);
            g_showroomPhase = ShowroomPhase::Settled;
            g_showroomSceneSettledMs = nowMs;
            if (envEnabled("FNVXR_MENU_SCENE_AUTO_ADVANCE", false))
                g_showroomNextActionMs =
                    nowMs + static_cast<UInt64>(getFloatFromEnv("FNVXR_MENU_SCENE_SECONDS", 18.0f) * 1000.0f);
            else
                g_showroomNextActionMs = nowMs + 24ull * 60ull * 60ull * 1000ull;
            logTelemetry(
                "{\"event\":\"fnvxrShowroomScene\",\"phase\":\"settled\",\"index\":%lu,\"name\":\"%s\",\"menuOpen\":%s,\"locked\":%s}\n",
                static_cast<unsigned long>(g_showroomSceneIndex),
                scene.name,
                allowUiInput() ? "true" : "false",
                g_showroomControlsLocked ? "true" : "false");
            return;

        case ShowroomPhase::Settled:
            g_showroomSceneIndex =
                (g_showroomSceneIndex + 1) % static_cast<UInt32>(sizeof(kShowroomScenes) / sizeof(kShowroomScenes[0]));
            g_showroomPhase = ShowroomPhase::Idle;
            g_showroomNextActionMs = nowMs + 250;
            logTelemetry(
                "{\"event\":\"fnvxrShowroomScene\",\"phase\":\"advance\",\"nextIndex\":%lu}\n",
                static_cast<unsigned long>(g_showroomSceneIndex));
            return;
    }
}

void consumeExternalDInputBridge()
{
    SharedDInputState state {};
    if (!readSharedDInputSnapshot(state))
        return;

    const UInt32 packet = state.mouseClickPacket;
    if (packet == g_lastConsumedDInputMouseClickPacket)
        return;

    g_lastConsumedDInputMouseClickPacket = packet;
    if (packet == g_lastPublishedDInputMouseClickPacket)
        return;

    const bool active = state.pointerActive != 0;
    g_hasMenuPointer = active;
    g_lastMenuPointerClient.x = state.clientX;
    g_lastMenuPointerClient.y = state.clientY;
    g_latestPointerX.store(g_lastMenuPointerClient.x);
    g_latestPointerY.store(g_lastMenuPointerClient.y);
    g_latestPointerValid.store(active);
    g_latestPointerFrame.store(state.frame);

    if (g_loggedExternalDInputClicks < 24)
    {
        ++g_loggedExternalDInputClicks;
        logTelemetry(
            "externalDInput click packet=%lu active=%d client=(%ld,%ld) frame=%lu pluginAccept=%d\n",
            static_cast<unsigned long>(packet),
            static_cast<int>(active),
            g_lastMenuPointerClient.x,
            g_lastMenuPointerClient.y,
            static_cast<unsigned long>(state.frame),
            static_cast<int>(envEnabled("FNVXR_PLUGIN_ACCEPT_ON_EXTERNAL_DINPUT_CLICK", false)));
    }

    if (active && envEnabled("FNVXR_PLUGIN_ACCEPT_ON_EXTERNAL_DINPUT_CLICK", false))
        requestAcceptClick();
}

void syncExternalDInputPointer()
{
    SharedDInputState state {};
    if (!readSharedDInputSnapshot(state))
        return;

    const bool active = state.pointerActive != 0;
    const UInt32 frame = state.frame;
    const LONG clientX = state.clientX;
    const LONG clientY = state.clientY;
    if (active == g_lastExternalDInputPointerActive
        && frame == g_lastExternalDInputPointerFrame
        && clientX == g_lastExternalDInputPointerX
        && clientY == g_lastExternalDInputPointerY)
    {
        return;
    }

    g_lastExternalDInputPointerActive = active;
    g_lastExternalDInputPointerFrame = frame;
    g_lastExternalDInputPointerX = clientX;
    g_lastExternalDInputPointerY = clientY;

    g_hasMenuPointer = active;
    g_latestPointerValid.store(active);
    g_latestPointerFrame.store(frame);
    if (!active)
    {
        clearPointerHover();
        return;
    }

    g_lastMenuPointerClient.x = clientX;
    g_lastMenuPointerClient.y = clientY;
    g_latestPointerX.store(clientX);
    g_latestPointerY.store(clientY);

    HWND hwnd = gameWindow();
    POINT windowPointer { clientX, clientY };
    if (hwnd)
    {
        windowPointer = mapSharedPointerToWindow(hwnd, windowPointer);
        // Fallout's menus do not derive their visible hover state solely from
        // the DirectInput relative deltas. Keep the established native menu
        // cursor path synchronized with the same controller pointer sample so
        // the rendered cursor, Gamebryo hover state, and injected click agree.
        PostMessageA(
            hwnd,
            WM_MOUSEMOVE,
            0,
            MAKELPARAM(static_cast<WORD>(windowPointer.x), static_cast<WORD>(windowPointer.y)));
        updateGameCursorTile(hwnd);
        if (pointerTileFallbackEnabled())
            updateDirectMenuPointerHover();
    }

    if (g_loggedExternalDInputPointers < 48 || (frame % 120) == 0)
    {
        ++g_loggedExternalDInputPointers;
        const UInt32 menuBits = currentMenuBits();
        logTelemetry(
            "externalDInput pointer active=1 raw=(%ld,%ld) mapped=(%ld,%ld) frame=%lu hwnd=%p ui=%d menuBits=0x%02lx pipboy=%d\n",
            clientX,
            clientY,
            windowPointer.x,
            windowPointer.y,
            static_cast<unsigned long>(frame),
            hwnd,
            static_cast<int>(allowUiInput()),
            static_cast<unsigned long>(menuBits),
            static_cast<int>((menuBits & (1u << 6)) != 0));
    }
}

UInt32 externalXInputNavMask(const SharedXInputState& state, UInt32 menuBits)
{
    SharedXInputState navState = state;
    SharedDInputState dinput {};
    if (readSharedDInputSnapshot(dinput))
    {
        navState.leftThumbX = static_cast<std::int16_t>(
            std::clamp(dinput.leftStickX, static_cast<std::int32_t>(-32767), static_cast<std::int32_t>(32767)));
        navState.leftThumbY = static_cast<std::int16_t>(
            std::clamp(dinput.leftStickY, static_cast<std::int32_t>(-32767), static_cast<std::int32_t>(32767)));
        navState.rightThumbX = static_cast<std::int16_t>(
            std::clamp(dinput.rightStickX, static_cast<std::int32_t>(-32767), static_cast<std::int32_t>(32767)));
        navState.rightThumbY = static_cast<std::int16_t>(
            std::clamp(dinput.rightStickY, static_cast<std::int32_t>(-32767), static_cast<std::int32_t>(32767)));
    }

    UInt32 mask = 0;
    if (navState.buttons & XInputDpadUp)
        mask |= 1u;
    if (navState.buttons & XInputDpadDown)
        mask |= 2u;
    if (navState.buttons & XInputDpadLeft)
        mask |= 4u;
    if (navState.buttons & XInputDpadRight)
        mask |= 8u;

    const int axisDeadzone = getIntFromEnv("FNVXR_UI_NAV_DEADZONE", 16000);
    std::int16_t navX = 0;
    std::int16_t navY = 0;
    if (pipBoyPointerOnly(menuBits)
        && envEnabled("FNVXR_PIPBOY_POINTER_ONLY_STICK_NAV_SUPPRESS", false))
    {
        return mask;
    }
    if (pipBoySplitStickNav(menuBits))
    {
        if (navState.rightThumbY > axisDeadzone)
            mask |= 1u;
        if (navState.rightThumbY < -axisDeadzone)
            mask |= 2u;
        if (navState.leftThumbX < -axisDeadzone)
            mask |= 4u;
        if (navState.leftThumbX > axisDeadzone)
            mask |= 8u;
        return mask;
    }

    selectUiNavAxes(menuBits, navState, navX, navY);

    if (navY > axisDeadzone)
        mask |= 1u;
    if (navY < -axisDeadzone)
        mask |= 2u;
    if (navX < -axisDeadzone)
        mask |= 4u;
    if (navX > axisDeadzone)
        mask |= 8u;
    return mask;
}

void tapExternalXInputNav(UInt32 navMask)
{
    const bool directUiClick = directUiClickEnabled();
    HWND hwnd = gameWindow();
    if (navMask & 1u)
    {
        const bool handled = directUiClick && directMenuNavigate(-1);
        if (!handled)
            tapDirectInputKey(DIK_UP);
        if (hwnd && !handled)
            postMenuKey(hwnd, VK_UP);
    }
    if (navMask & 2u)
    {
        const bool handled = directUiClick && directMenuNavigate(1);
        if (!handled)
            tapDirectInputKey(DIK_DOWN);
        if (hwnd && !handled)
            postMenuKey(hwnd, VK_DOWN);
    }
    if (navMask & 4u)
    {
        const bool handled = directUiClick && directMenuNavigate(-1);
        if (!handled)
            tapDirectInputKey(DIK_LEFT);
        if (hwnd && !handled)
            postMenuKey(hwnd, VK_LEFT);
    }
    if (navMask & 8u)
    {
        const bool handled = directUiClick && directMenuNavigate(1);
        if (!handled)
            tapDirectInputKey(DIK_RIGHT);
        if (hwnd && !handled)
            postMenuKey(hwnd, VK_RIGHT);
    }
}

void releaseExternalXInputGameplayHolds()
{
    setGameplayRunMode(false, "externalXInput:release", 0);
    setGameplayAutoRun(false, "externalXInput:release", 0);
    holdDirectInputKey(MouseButtonOffset, false);
    holdDirectInputKey(MouseButtonOffset + 1, false);
    holdDirectInputKey(DIK_R, false);
    holdGameplayGrab(false);
    holdGameplayRunModifier(false);
    holdDirectInputKey(DIK_W, false);
    cancelThirdPersonL3Control("externalXInput:release", 0);
}

UInt32 gameplayStimpakKey()
{
    return directInputMacroKeyFromEnv("FNVXR_GAMEPLAY_STIMPAK_DIK", DIK_1);
}

UInt32 gameplayAmmoSwapKey()
{
    return directInputMacroKeyFromEnv("FNVXR_GAMEPLAY_AMMO_SWAP_DIK", DIK_2);
}

UInt32 gameplayVatsKey()
{
    return directInputMacroKeyFromEnv("FNVXR_GAMEPLAY_VATS_DIK", 0x2F);
}

UInt32 gameplayWaitKey()
{
    return directInputMacroKeyFromEnv("FNVXR_GAMEPLAY_WAIT_DIK", DIK_T);
}

UInt32 gameplayGrenadeKey()
{
    return directInputMacroKeyFromEnv("FNVXR_GAMEPLAY_GRENADE_DIK", DIK_3);
}

UInt32 gameplayBackupKey()
{
    return directInputMacroKeyFromEnv("FNVXR_GAMEPLAY_BACKUP_DIK", DIK_4);
}

UInt32 gameplayCombatAKey()
{
    return directInputMacroKeyFromEnv("FNVXR_GAMEPLAY_COMBAT_A_DIK", DIK_5);
}

UInt32 gameplayCombatBKey()
{
    return directInputMacroKeyFromEnv("FNVXR_GAMEPLAY_COMBAT_B_DIK", DIK_6);
}

UInt32 gameplayCombatXKey()
{
    return directInputMacroKeyFromEnv("FNVXR_GAMEPLAY_COMBAT_X_DIK", DIK_7);
}

UInt32 gameplayCombatYKey()
{
    return directInputMacroKeyFromEnv("FNVXR_GAMEPLAY_COMBAT_Y_DIK", DIK_8);
}

bool tapCombatKey(const char* action, UInt32 key, const char* source, UInt64 frame)
{
    const bool releaseAimMouse = envEnabled("FNVXR_GAMEPLAY_HOTKEY_RELEASE_AIM_MOUSE", true);
    if (releaseAimMouse)
        holdDirectInputKey(MouseButtonOffset + 1, false);
    const bool weaponOut = playerWeaponOut();
    const bool tapped = key != 0 && tapDirectInputKey(key);
    if (tapped)
        noteFavoriteWeaponSelection(key, source, frame);
    logTelemetry(
        "combatChord fire frame=%llu source=%s action=%s slot=%lu key=0x%02lx tapped=%d weaponOut=%d weaponClass=%s weaponFormId=0x%08lx releaseAimMouse=%d hotkeyIgnoresWeaponOut=1\n",
        static_cast<unsigned long long>(frame),
        source ? source : "unknown",
        action ? action : "unknown",
        static_cast<unsigned long>(favoriteSlotForKey(key)),
        static_cast<unsigned long>(key),
        static_cast<int>(tapped),
        static_cast<int>(weaponOut),
        weaponClassName(currentWeaponClass()),
        static_cast<unsigned long>(g_lastKnownWeaponFormId),
        static_cast<int>(releaseAimMouse));
    return tapped;
}

bool tapStimpakKey(const char* source, UInt64 frame)
{
    return tapCombatKey("stimpak", gameplayStimpakKey(), source, frame);
}

bool tapAmmoSwapKey(const char* source, UInt64 frame)
{
    return tapCombatKey("ammoSwap", gameplayAmmoSwapKey(), source, frame);
}

bool tapVatsKey(const char* source, UInt64 frame)
{
    return tapCombatKey("vats", gameplayVatsKey(), source, frame);
}

bool tapWaitKey(const char* source, UInt64 frame)
{
    const UInt32 key = gameplayWaitKey();
    const bool tapped = key != 0 && tapDirectInputKey(key);
    logTelemetry(
        "gameplayWait fire frame=%llu source=%s key=0x%02lx tapped=%d\n",
        static_cast<unsigned long long>(frame),
        source ? source : "unknown",
        static_cast<unsigned long>(key),
        static_cast<int>(tapped));
    return tapped;
}

struct PrimaryAttackState
{
    bool held = false;
    UInt64 lastTapMs = 0;
    UInt32 lastWeaponClass = fnvxr::shared::PlayerWeaponClassUnknown;
};

UInt64 meleeTriggerRepeatMs()
{
    return static_cast<UInt64>((std::max)(80, getIntFromEnv("FNVXR_GAMEPLAY_MELEE_TRIGGER_REPEAT_MS", 420)));
}

bool driveGameplayPrimaryAttack(bool rightTriggerHeld, PrimaryAttackState& state, UInt64 frame, const char* source)
{
    const UInt32 weaponClass = currentWeaponClass();
    const bool classTapMode =
        envEnabled("FNVXR_GAMEPLAY_WEAPON_CLASS_TRIGGER_ENABLE", true)
        && weaponClassMeleeOrUnarmed(weaponClass);

    if (!classTapMode)
    {
        state.lastTapMs = 0;
        state.lastWeaponClass = weaponClass;
        const bool changed = state.held != rightTriggerHeld;
        state.held = rightTriggerHeld;
        holdDirectInputKey(MouseButtonOffset, rightTriggerHeld);
        if (changed)
        {
            logTelemetry(
                "primaryAttack hold frame=%llu source=%s held=%d class=%s formId=0x%08lx slot=%lu\n",
                static_cast<unsigned long long>(frame),
                source ? source : "unknown",
                static_cast<int>(rightTriggerHeld),
                weaponClassName(weaponClass),
                static_cast<unsigned long>(g_lastKnownWeaponFormId),
                static_cast<unsigned long>(g_lastKnownWeaponFavoriteSlot));
        }
        return changed;
    }

    holdDirectInputKey(MouseButtonOffset, false);
    const UInt64 nowMs = GetTickCount64();
    const bool triggerPressed = rightTriggerHeld && !state.held;
    const bool repeatDue =
        rightTriggerHeld
        && state.lastTapMs != 0
        && nowMs >= state.lastTapMs + meleeTriggerRepeatMs();
    const bool classChanged = state.lastWeaponClass != weaponClass;

    if (triggerPressed || repeatDue || classChanged)
    {
        const bool tapped = rightTriggerHeld && tapDirectInputKey(MouseButtonOffset);
        if (tapped)
            state.lastTapMs = nowMs;
        logTelemetry(
            "primaryAttack meleeTap frame=%llu source=%s held=%d pressed=%d repeat=%d class=%s formId=0x%08lx slot=%lu tapped=%d repeatMs=%llu\n",
            static_cast<unsigned long long>(frame),
            source ? source : "unknown",
            static_cast<int>(rightTriggerHeld),
            static_cast<int>(triggerPressed),
            static_cast<int>(repeatDue),
            weaponClassName(weaponClass),
            static_cast<unsigned long>(g_lastKnownWeaponFormId),
            static_cast<unsigned long>(g_lastKnownWeaponFavoriteSlot),
            static_cast<int>(tapped),
            static_cast<unsigned long long>(meleeTriggerRepeatMs()));
    }

    if (!rightTriggerHeld)
        state.lastTapMs = 0;
    state.held = rightTriggerHeld;
    state.lastWeaponClass = weaponClass;
    return triggerPressed || repeatDue || classChanged;
}

bool tapGrenadeKey(const char* source, UInt64 frame)
{
    return tapCombatKey("grenade", gameplayGrenadeKey(), source, frame);
}

bool tapBackupKey(const char* source, UInt64 frame)
{
    return tapCombatKey("backup", gameplayBackupKey(), source, frame);
}

bool tapCombatAKey(const char* source, UInt64 frame)
{
    return tapCombatKey("combatA", gameplayCombatAKey(), source, frame);
}

bool tapCombatBKey(const char* source, UInt64 frame)
{
    return tapCombatKey("combatB", gameplayCombatBKey(), source, frame);
}

bool tapCombatXKey(const char* source, UInt64 frame)
{
    return tapCombatKey("combatX", gameplayCombatXKey(), source, frame);
}

bool tapCombatYKey(const char* source, UInt64 frame)
{
    return tapCombatKey("combatY", gameplayCombatYKey(), source, frame);
}

bool externalDInputSharedReady()
{
    SharedDInputState snapshot {};
    return readSharedDInputSnapshot(snapshot);
}

UInt32 externalDInputFrame()
{
    SharedDInputState snapshot {};
    return readSharedDInputSnapshot(snapshot) ? snapshot.frame : 0u;
}

LONG externalDInputLeftGrip()
{
    SharedDInputState snapshot {};
    return readSharedDInputSnapshot(snapshot) ? static_cast<LONG>(snapshot.leftGrip) : 0;
}

LONG externalDInputRightGrip()
{
    SharedDInputState snapshot {};
    return readSharedDInputSnapshot(snapshot) ? static_cast<LONG>(snapshot.rightGrip) : 0;
}

bool externalLeftGripPipBoyHeld()
{
    if (!envEnabled("FNVXR_LEFT_GRIP_PIPBOY_MODE", true) || !externalDInputSharedReady())
        return false;

    const float threshold = std::clamp(getFloatFromEnv("FNVXR_PIPBOY_GRIP_THRESHOLD", 0.55f), 0.0f, 1.0f);
    return externalDInputLeftGrip() > sharedStickValue(threshold);
}

float rightGripMenuThreshold()
{
    const float fallbackThreshold = getFloatFromEnv("FNVXR_PIPBOY_GRIP_THRESHOLD", 0.55f);
    return std::clamp(getFloatFromEnv("FNVXR_MENU_GRIP_THRESHOLD", fallbackThreshold), 0.0f, 1.0f);
}

bool externalRightGripHeld()
{
    if (!externalDInputSharedReady())
        return false;

    return externalDInputRightGrip() > sharedStickValue(rightGripMenuThreshold());
}

bool externalRightGripMenuHeld()
{
    if (!envEnabled("FNVXR_RIGHT_GRIP_MENU_MODE", true))
        return false;

    return externalRightGripHeld();
}

void updateExternalPipBoyGripMode(bool held, bool& previousHeld)
{
    if (held == previousHeld)
        return;

    const UInt32 menuBits = currentMenuBits();
    const bool pipBoyVisible = (menuBits & (1u << 6)) != 0 || isPipboyVisible();
    bool tappedTab = false;
    const bool keyboardFallback = pluginMenuKeyboardFallbackEnabled();
    if (held)
    {
        if (keyboardFallback)
        {
            tapDirectInputKey(DIK_TAB);
            tappedTab = true;
        }
        logTelemetry(
            "pipboyGrip mode=toggle held=1 menuBits=0x%02lx pipboy=%d tab=%d leftGrip=%ld threshold=%.3f\n",
            static_cast<unsigned long>(menuBits),
            static_cast<int>(pipBoyVisible),
            static_cast<int>(tappedTab),
            externalDInputLeftGrip(),
            getFloatFromEnv("FNVXR_PIPBOY_GRIP_THRESHOLD", 0.55f));
    }
    else
    {
        logTelemetry(
            "pipboyGrip mode=release held=0 menuBits=0x%02lx pipboy=%d tab=%d leftGrip=%ld threshold=%.3f\n",
            static_cast<unsigned long>(menuBits),
            static_cast<int>(pipBoyVisible),
            static_cast<int>(tappedTab),
            externalDInputLeftGrip(),
            getFloatFromEnv("FNVXR_PIPBOY_GRIP_THRESHOLD", 0.55f));
    }

    previousHeld = held;
}

void updateExternalRightGripMenuMode(bool held, bool& previousHeld)
{
    if (held == previousHeld)
        return;

    constexpr UInt32 startMenuBit = 1u << 1;
    constexpr UInt32 nonStartUiBits = (1u << 2) | (1u << 3) | (1u << 4) | (1u << 5) | (1u << 6);
    const UInt32 menuBits = currentMenuBits();
    const bool startMenuVisible = (menuBits & startMenuBit) != 0;
    const bool otherUiVisible = (menuBits & nonStartUiBits) != 0;
    bool tappedEscape = false;
    const bool keyboardFallback = pluginMenuKeyboardFallbackEnabled();

    if (held)
    {
        if (keyboardFallback && !otherUiVisible)
        {
            tapDirectInputKey(DIK_ESCAPE);
            tappedEscape = true;
        }
        logTelemetry(
            "rightGripMenu mode=toggle held=1 menuBits=0x%02lx startMenu=%d otherUi=%d escape=%d rightGrip=%ld threshold=%.3f\n",
            static_cast<unsigned long>(menuBits),
            static_cast<int>(startMenuVisible),
            static_cast<int>(otherUiVisible),
            static_cast<int>(tappedEscape),
            externalDInputRightGrip(),
            rightGripMenuThreshold());
    }
    else
    {
        logTelemetry(
            "rightGripMenu mode=release held=0 menuBits=0x%02lx startMenu=%d otherUi=%d escape=%d rightGrip=%ld threshold=%.3f\n",
            static_cast<unsigned long>(menuBits),
            static_cast<int>(startMenuVisible),
            static_cast<int>(otherUiVisible),
            static_cast<int>(tappedEscape),
            externalDInputRightGrip(),
            rightGripMenuThreshold());
    }

    previousHeld = held;
}

void consumeExternalXInputGameplayControls(
    const SharedXInputState& state,
    UInt16 pressed,
    bool& previousRightTriggerHeld,
    bool& previousLeftTriggerHeld,
    bool& previousRunHeld,
    bool& previousReloadHeld,
    bool& previousGrabHeld)
{
    constexpr UInt8 triggerThreshold = 64;
    static bool previousThirdPersonChordHeld = false;
    static bool previousVatsChordHeld = false;
    static PrimaryAttackState primaryAttackState {};
    const UInt64 frame = externalDInputFrame();
    const bool rightTriggerHeld = state.rightTrigger > triggerThreshold;
    const bool leftTriggerHeld = state.leftTrigger > triggerThreshold;
    const bool analogRun = gameplayAnalogRunHeld(state.leftThumbY);
    const bool keyboardMovement = pluginKeyboardMovementEnabled();
    const bool keyboardGameplayFallback = pluginGameplayKeyboardFallbackEnabled();
    const bool thirdPersonChordHeld =
        thirdPersonL3ControlsEnabled()
        && (state.buttons & XInputLeftThumb) != 0;
    const bool autoRunPressed = (pressed & XInputRightThumb) != 0;
    const bool combatChordHeld =
        keyboardGameplayFallback
        && leftTriggerHeld
        && envEnabled("FNVXR_GAMEPLAY_COMBAT_CHORDS_ENABLE", true);
    const bool protectedCombatChordHeld =
        combatChordHeld
        && externalLeftGripPipBoyHeld();
    const bool vatsChordHeld =
        keyboardGameplayFallback
        && leftTriggerHeld
        && externalRightGripHeld()
        && envEnabled("FNVXR_GAMEPLAY_VATS_CHORD_ENABLE", true);
    const bool vatsChordPressed = vatsChordHeld && !previousVatsChordHeld;
    const bool waitChordPressed =
        keyboardGameplayFallback
        && leftTriggerHeld
        && (pressed & XInputBack) != 0
        && envEnabled("FNVXR_GAMEPLAY_WAIT_CHORD_ENABLE", true);
    const UInt16 combatChordPressed = combatChordHeld ? pressed : 0;
    const bool combatChordFaceHeld =
        combatChordHeld
        && (state.buttons & (XInputA | XInputB | XInputX | XInputY)) != 0;
    const bool suppressAimMouseForCombatChord =
        combatChordFaceHeld
        && envEnabled("FNVXR_GAMEPLAY_COMBAT_CHORD_SUPPRESS_AIM_MOUSE", true);
    const bool reloadHeld = keyboardGameplayFallback && !combatChordHeld && (state.buttons & XInputX) != 0;
    const bool rawRightGripHeld = externalRightGripHeld();
    const bool rightGripGrabHeld =
        keyboardGameplayFallback
        && envEnabled("FNVXR_GAMEPLAY_RIGHT_GRIP_GRAB_ENABLE", true)
        && rawRightGripHeld
        && !leftTriggerHeld;
    const bool thirdPersonZoomStep =
        updateThirdPersonL3Control(thirdPersonChordHeld, state.rightThumbY, frame, "externalXInput:L3");
    if (autoRunPressed)
        toggleGameplayAutoRun("externalXInput:R3", frame);
    const bool runHeld =
        keyboardMovement
        &&
        envEnabled("FNVXR_GAMEPLAY_RUN_BUTTON_ENABLE", true)
        && (g_gameplayRunModeEnabled || analogRun);

    const bool primaryAttackStep =
        driveGameplayPrimaryAttack(rightTriggerHeld, primaryAttackState, frame, "externalXInput:RT");
    holdDirectInputKey(MouseButtonOffset + 1, leftTriggerHeld && !suppressAimMouseForCombatChord && !vatsChordHeld);
    holdDirectInputKey(DIK_R, reloadHeld);
    holdGameplayGrab(rightGripGrabHeld);
    holdGameplayRunModifier(runHeld);
    holdDirectInputKey(DIK_W, keyboardMovement && g_gameplayAutoRunEnabled);

    if (vatsChordPressed)
        tapVatsKey("externalXInput:LT+RG", frame);
    if (keyboardGameplayFallback && (combatChordPressed & XInputA))
    {
        if (protectedCombatChordHeld)
            tapStimpakKey("externalXInput:LT+LG+A", frame);
        else
            tapCombatAKey("externalXInput:LT+A", frame);
    }
    else if (keyboardGameplayFallback && (pressed & XInputA))
    {
        if (!tryPetFriendlyMobActivation(frame))
            tapDirectInputKey(DIK_E);
    }
    if (keyboardGameplayFallback && (combatChordPressed & XInputB))
    {
        if (protectedCombatChordHeld)
            tapGrenadeKey("externalXInput:LT+LG+B", frame);
        else
            tapCombatBKey("externalXInput:LT+B", frame);
    }
    else if (keyboardGameplayFallback && (pressed & XInputB))
    {
        tapDirectInputKey(DIK_SPACE);
        logTelemetry("gameplayJump fire frame=%llu source=externalXInput:B\n", static_cast<unsigned long long>(frame));
    }
    if (keyboardGameplayFallback && (combatChordPressed & XInputX))
    {
        if (protectedCombatChordHeld)
            tapBackupKey("externalXInput:LT+LG+X", frame);
        else
            tapCombatXKey("externalXInput:LT+X", frame);
    }
    if (keyboardGameplayFallback && (combatChordPressed & XInputY))
    {
        if (protectedCombatChordHeld)
            tapAmmoSwapKey("externalXInput:LT+LG+Y", frame);
        else
            tapCombatYKey("externalXInput:LT+Y", frame);
    }
    else if (keyboardGameplayFallback && (pressed & XInputY))
    {
        tapDirectInputKey(DIK_LCONTROL);
        logTelemetry("gameplaySneak fire frame=%llu source=externalXInput:Y\n", static_cast<unsigned long long>(frame));
    }
    if (keyboardGameplayFallback && (pressed & XInputStart))
    {
        tapDirectInputKey(DIK_ESCAPE);
        logTelemetry("menuStart fire frame=%llu source=externalXInput:Start gameplay=1\n", static_cast<unsigned long long>(frame));
    }
    if (waitChordPressed)
    {
        tapWaitKey("externalXInput:LT+Back", frame);
    }
    else if (keyboardGameplayFallback && (pressed & XInputBack))
    {
        tapDirectInputKey(DIK_ESCAPE);
        logTelemetry("menuStart fire frame=%llu source=externalXInput:Back gameplay=1\n", static_cast<unsigned long long>(frame));
    }
    if (keyboardGameplayFallback
        && !thirdPersonL3ControlsEnabled()
        && envEnabled("FNVXR_L3_MENU_FALLBACK", true)
        && (pressed & XInputLeftThumb))
    {
        tapDirectInputKey(DIK_ESCAPE);
        logTelemetry(
            "menuToggle fire frame=%llu source=L3Fallback key=0x%02lx gameplay=1\n",
            static_cast<unsigned long long>(frame),
            static_cast<unsigned long>(DIK_ESCAPE));
    }

    if (pressed
        || rightTriggerHeld != previousRightTriggerHeld
        || primaryAttackStep
        || leftTriggerHeld != previousLeftTriggerHeld
        || reloadHeld != previousReloadHeld
        || rightGripGrabHeld != previousGrabHeld
        || runHeld != previousRunHeld
        || autoRunPressed
        || thirdPersonZoomStep
        || thirdPersonChordHeld != previousThirdPersonChordHeld
        || vatsChordPressed
        || waitChordPressed)
    {
        logTelemetry(
            "externalXInput gameplay buttons=0x%04x pressed=0x%04x rt=%u lt=%u rtMouse=%d ltMouse=%d aimMouseSuppressed=%d vatsChord=%d vatsPressed=%d waitPressed=%d reloadHeld=%d grabHeld=%d grabKey=0x%02lx runHeld=%d runKey=0x%02lx moveMode=%u walk=%d runMode=%d thirdPersonChord=%d combatChord=%d protectedCombatChord=%d combatPressed=0x%04x weaponClass=%s weaponFormId=0x%08lx weaponSlot=%lu analogRun=%d autoRun=%d autoToggle=%d keyboardMovement=%d keyboardFallback=%d bJump=%d ySneak=%d rs=%d\n",
            static_cast<unsigned int>(state.buttons),
            static_cast<unsigned int>(pressed),
            static_cast<unsigned int>(state.rightTrigger),
            static_cast<unsigned int>(state.leftTrigger),
            static_cast<int>(rightTriggerHeld),
            static_cast<int>(leftTriggerHeld && !suppressAimMouseForCombatChord && !vatsChordHeld),
            static_cast<int>(suppressAimMouseForCombatChord || vatsChordHeld),
            static_cast<int>(vatsChordHeld),
            static_cast<int>(vatsChordPressed),
            static_cast<int>(waitChordPressed),
            static_cast<int>(reloadHeld),
            static_cast<int>(rightGripGrabHeld),
            static_cast<unsigned long>(gameplayGrabKey()),
            static_cast<int>(runHeld),
            static_cast<unsigned long>(gameplayRunModifierKey()),
            static_cast<unsigned int>(g_gameplayMovementMode),
            static_cast<int>(g_gameplayWalkModeEnabled),
            static_cast<int>(g_gameplayRunModeEnabled),
            static_cast<int>(thirdPersonChordHeld),
            static_cast<int>(combatChordHeld),
            static_cast<int>(protectedCombatChordHeld),
            static_cast<unsigned int>(combatChordPressed),
            weaponClassName(currentWeaponClass()),
            static_cast<unsigned long>(g_lastKnownWeaponFormId),
            static_cast<unsigned long>(g_lastKnownWeaponFavoriteSlot),
            static_cast<int>(analogRun),
            static_cast<int>(g_gameplayAutoRunEnabled),
            static_cast<int>(autoRunPressed),
            static_cast<int>(keyboardMovement),
            static_cast<int>(keyboardGameplayFallback),
            static_cast<int>((pressed & XInputB) != 0),
            static_cast<int>((pressed & XInputY) != 0),
            static_cast<int>(state.rightThumbY));
    }

    previousRightTriggerHeld = rightTriggerHeld;
    previousLeftTriggerHeld = leftTriggerHeld;
    previousRunHeld = runHeld;
    previousReloadHeld = reloadHeld;
    previousGrabHeld = rightGripGrabHeld;
    previousThirdPersonChordHeld = thirdPersonChordHeld;
    previousVatsChordHeld = vatsChordHeld;
}

void consumeExternalXInputBridge()
{
    static bool previousRightTriggerHeld = false;
    static bool previousLeftTriggerHeld = false;
    static bool previousRunHeld = false;
    static bool previousReloadHeld = false;
    static bool previousGrabHeld = false;
    static bool previousPipBoyGripHeld = false;
    static bool previousRightGripMenuHeld = false;
    static UInt32 previousUiFavoriteAssignChordState = 0;
    static UInt64 lastUiMapZoomMs = 0;
    static bool wasInputAllowed = false;
    static bool releaseBeforePressPending = true;

    SharedXInputState state {};
    if (!readEffectiveExternalXInputSnapshot(state))
    {
        updateExternalPipBoyGripMode(false, previousPipBoyGripHeld);
        updateExternalRightGripMenuMode(false, previousRightGripMenuHeld);
        g_lastExternalXInputButtons = 0;
        g_lastExternalXInputNavMask = 0;
        releaseExternalXInputGameplayHolds();
        previousRightTriggerHeld = false;
        previousLeftTriggerHeld = false;
        previousRunHeld = false;
        previousReloadHeld = false;
        previousGrabHeld = false;
        previousUiFavoriteAssignChordState = 0;
        wasInputAllowed = false;
        releaseBeforePressPending = true;
        return;
    }

    const UInt32 menuBits = currentMenuBits();
    const RuntimePhase phase = runtimePhaseFromMenuBits(menuBits);
    const bool uiInputAllowed = uiInputAllowedFromMenuBits(menuBits);
    const bool gameplayInputAllowed =
        phase == RuntimePhase::Gameplay
        && cameraAllowedForMenuBits(menuBits);
    const bool gripXboxMode = state.leftTrigger > 180;
    const bool leftTriggerModifierHeld = state.leftTrigger > 64;
    const bool inputAllowed = uiInputAllowed || gameplayInputAllowed;
    const bool pipBoyMenuVisible = pipBoyVisibleFromMenuBits(menuBits);
    const bool startMenuVisible = (menuBits & (1u << 1)) != 0;
    const bool menuKeyboardFallback = pluginMenuKeyboardFallbackEnabled();
    if (inputAllowed && !wasInputAllowed)
        releaseBeforePressPending = true;
    if (inputAllowed && releaseBeforePressPending)
    {
        const bool dangerousInputHeld = state.buttons != 0
            || state.leftTrigger > 16
            || state.rightTrigger > 16
            || externalLeftGripPipBoyHeld()
            || externalRightGripHeld();
        g_lastExternalXInputButtons = state.buttons;
        g_lastExternalXInputNavMask = 0;
        releaseExternalXInputGameplayHolds();
        previousRightTriggerHeld = false;
        previousLeftTriggerHeld = false;
        previousRunHeld = false;
        previousReloadHeld = false;
        previousGrabHeld = false;
        previousUiFavoriteAssignChordState = 0;
        if (dangerousInputHeld)
        {
            wasInputAllowed = true;
            return;
        }
        releaseBeforePressPending = false;
        logTelemetry("input rebaseline complete release-before-press=1 packet=%lu\n", static_cast<unsigned long>(state.packet));
    }
    wasInputAllowed = inputAllowed;
    const bool pipBoyGripEligible = gameplayInputAllowed || previousPipBoyGripHeld || pipBoyMenuVisible;
    const bool rawPipBoyGripHeld = pipBoyGripEligible && externalLeftGripPipBoyHeld();
    const bool pipBoyGripSuppressedForChord =
        rawPipBoyGripHeld
        && leftTriggerModifierHeld
        && envEnabled("FNVXR_LEFT_GRIP_COMBAT_CHORD_SUPPRESSES_PIPBOY", true);
    const bool pipBoyGripHeld = rawPipBoyGripHeld && !pipBoyGripSuppressedForChord;
    updateExternalPipBoyGripMode(pipBoyGripHeld, previousPipBoyGripHeld);
    const bool rightGripMenuEligible = gameplayInputAllowed || previousRightGripMenuHeld || startMenuVisible;
    const bool rawRightGripMenuHeld = rightGripMenuEligible && externalRightGripMenuHeld();
    const bool rightGripMenuSuppressedForChord =
        rawRightGripMenuHeld
        && leftTriggerModifierHeld
        && envEnabled("FNVXR_RIGHT_GRIP_COMBAT_CHORD_SUPPRESSES_MENU", true);
    const bool rightGripMenuHeld = rawRightGripMenuHeld && !rightGripMenuSuppressedForChord;
    updateExternalRightGripMenuMode(rightGripMenuHeld, previousRightGripMenuHeld);
    if (!inputAllowed)
    {
        wasInputAllowed = false;
        releaseBeforePressPending = true;
        g_lastExternalXInputButtons = state.buttons;
        g_lastExternalXInputNavMask = 0;
        releaseExternalXInputGameplayHolds();
        previousRightTriggerHeld = false;
        previousLeftTriggerHeld = false;
        previousRunHeld = false;
        previousReloadHeld = false;
        previousGrabHeld = false;
        previousUiFavoriteAssignChordState = 0;
        return;
    }

    const UInt16 pressed = state.buttons & ~g_lastExternalXInputButtons;
    const UInt32 navMask = uiInputAllowed ? externalXInputNavMask(state, menuBits) : 0;
    const UInt64 nowMs = GetTickCount64();
    const bool navChanged = navMask != 0 && navMask != g_lastExternalXInputNavMask;
    const bool navRepeat = navMask != 0 && nowMs >= g_lastExternalXInputNavMs + 140;

    if (menuKeyboardFallback && (navChanged || navRepeat))
    {
        tapExternalXInputNav(navMask);
    }
    if (navChanged || navRepeat)
    {
        g_lastExternalXInputNavMask = navMask;
        g_lastExternalXInputNavMs = nowMs;
    }
    else if (navMask == 0)
    {
        g_lastExternalXInputNavMask = 0;
    }

    if (menuKeyboardFallback && uiMapZoomVisible(menuBits))
    {
        const int mapZoomDirection = externalUiMapZoomDirection(state);
        if (mapZoomDirection != 0 && nowMs >= lastUiMapZoomMs + uiMapZoomRepeatMs())
        {
            lastUiMapZoomMs = nowMs;
            publishUiMapZoom(
                mapZoomDirection,
                externalDInputSharedReady() ? externalDInputFrame() : state.packet,
                "externalXInput:mapZoom");
        }
    }

    UInt16 uiFavoriteAssignPressed = 0;
    UInt32 uiFavoriteAssignChordState = 0;
    if (menuKeyboardFallback
        && uiInputAllowed
        && pipBoyMenuVisible
        && leftTriggerModifierHeld)
    {
        const bool protectedAssign = rawPipBoyGripHeld;
        constexpr UInt32 protectedAssignBit = 1u << 16;
        const UInt32 faceButtons = state.buttons & (XInputA | XInputB | XInputX | XInputY);
        uiFavoriteAssignChordState = faceButtons | (protectedAssign ? protectedAssignBit : 0u);
        const bool sameProtectedMode =
            (previousUiFavoriteAssignChordState & protectedAssignBit)
            == (uiFavoriteAssignChordState & protectedAssignBit);
        const UInt32 previousFaceButtons = sameProtectedMode
            ? (previousUiFavoriteAssignChordState & (XInputA | XInputB | XInputX | XInputY))
            : 0u;
        uiFavoriteAssignPressed = static_cast<UInt16>(faceButtons & ~previousFaceButtons);
        const UInt64 frame = externalDInputFrame();
        if (uiFavoriteAssignPressed & XInputA)
        {
            assignUiFavoriteSlotKey(
                protectedAssign ? "externalXInput:pipboy:LT+LG+A" : "externalXInput:pipboy:LT+A",
                frame,
                protectedAssign ? DIK_1 : DIK_5,
                protectedAssign ? "stimpak" : "combatA");
        }
        if (uiFavoriteAssignPressed & XInputB)
        {
            assignUiFavoriteSlotKey(
                protectedAssign ? "externalXInput:pipboy:LT+LG+B" : "externalXInput:pipboy:LT+B",
                frame,
                protectedAssign ? DIK_3 : DIK_6,
                protectedAssign ? "grenade" : "combatB");
        }
        if (uiFavoriteAssignPressed & XInputX)
        {
            assignUiFavoriteSlotKey(
                protectedAssign ? "externalXInput:pipboy:LT+LG+X" : "externalXInput:pipboy:LT+X",
                frame,
                protectedAssign ? DIK_4 : DIK_7,
                protectedAssign ? "backup" : "combatX");
        }
        if (uiFavoriteAssignPressed & XInputY)
        {
            assignUiFavoriteSlotKey(
                protectedAssign ? "externalXInput:pipboy:LT+LG+Y" : "externalXInput:pipboy:LT+Y",
                frame,
                protectedAssign ? DIK_2 : DIK_8,
                protectedAssign ? "ammoSwapReserved" : "combatY");
        }
    }
    previousUiFavoriteAssignChordState = uiFavoriteAssignChordState;

    if (gameplayInputAllowed && !uiInputAllowed)
    {
        consumeExternalXInputGameplayControls(
            state,
            pressed,
            previousRightTriggerHeld,
            previousLeftTriggerHeld,
            previousRunHeld,
            previousReloadHeld,
            previousGrabHeld);
    }
    else
    {
        releaseExternalXInputGameplayHolds();
        previousRightTriggerHeld = false;
        previousLeftTriggerHeld = false;
        previousRunHeld = false;
        previousReloadHeld = false;
    }

    if (menuKeyboardFallback && uiInputAllowed && (pressed & XInputA) && !(uiFavoriteAssignPressed & XInputA))
    {
        if (pipBoyPointerOnly(menuBits))
        {
            logTelemetry(
                "uiButton ignore source=A action=accept pointerOnly=1 pipBoy=%d menuBits=0x%02lx\n",
                static_cast<int>(pipBoyMenuVisible),
                static_cast<unsigned long>(menuBits));
        }
        else
        {
            const bool handled = directUiClickEnabled() && directMenuAcceptSelection();
            if (!handled)
                tapDirectInputKey(DIK_RETURN);
            if (!handled)
            {
                if (HWND hwnd = gameWindow())
                    postMenuKey(hwnd, VK_RETURN);
            }
            logTelemetry(
                "uiButton fire source=A action=accept key=0x%02lx pointerClick=0 pipBoy=%d menuBits=0x%02lx handled=%d\n",
                static_cast<unsigned long>(DIK_RETURN),
                static_cast<int>(pipBoyMenuVisible),
                static_cast<unsigned long>(menuBits),
                static_cast<int>(handled));
        }
    }
    if (menuKeyboardFallback && uiInputAllowed && (pressed & XInputB) && !(uiFavoriteAssignPressed & XInputB))
    {
        const UInt32 backKey = uiBackKeyForMenu(menuBits);
        tapDirectInputKey(backKey);
        if (HWND hwnd = gameWindow())
            postMenuKey(hwnd, uiBackVirtualKeyForMenu(menuBits));
        logTelemetry(
            "uiButton fire source=B action=back key=0x%02lx pipBoy=%d menuBits=0x%02lx\n",
            static_cast<unsigned long>(backKey),
            static_cast<int>(pipBoyMenuVisible),
            static_cast<unsigned long>(menuBits));
    }
    if (menuKeyboardFallback && uiInputAllowed && (pressed & XInputX) && !(uiFavoriteAssignPressed & XInputX))
    {
        if (pipBoyMenuVisible)
        {
            const UInt32 sortKey = uiSortKey();
            tapDirectInputKey(sortKey);
            logTelemetry(
                "uiButton fire source=X action=sort key=0x%02lx pipBoy=%d menuBits=0x%02lx\n",
                static_cast<unsigned long>(sortKey),
                static_cast<int>(pipBoyMenuVisible),
                static_cast<unsigned long>(menuBits));
        }
        else
        {
            logTelemetry(
                "uiButton ignore source=X action=sort pipBoy=0 menuBits=0x%02lx\n",
                static_cast<unsigned long>(menuBits));
        }
    }
    if (menuKeyboardFallback && uiInputAllowed && (pressed & XInputY) && !(uiFavoriteAssignPressed & XInputY))
    {
        if (pipBoyMenuVisible)
        {
            const bool utilityFavorite = state.leftTrigger > 64;
            assignUiFavoriteSlot(
                utilityFavorite ? "externalXInput:pipboy:LT+Y" : "externalXInput:pipboy:Y",
                externalDInputFrame(),
                utilityFavorite);
            logTelemetry(
                "uiButton fire source=Y action=favoriteAssign utility=%d pipBoy=%d menuBits=0x%02lx\n",
                static_cast<int>(utilityFavorite),
                static_cast<int>(pipBoyMenuVisible),
                static_cast<unsigned long>(menuBits));
        }
        else
        {
            logTelemetry(
                "uiButton ignore source=Y action=favoriteAssign pipBoy=0 menuBits=0x%02lx\n",
                static_cast<unsigned long>(menuBits));
        }
    }
    if (menuKeyboardFallback && uiInputAllowed && (pressed & XInputStart))
    {
        const bool handled = uiInputAllowed && directUiClickEnabled() && directMenuAcceptSelection();
        if (!handled)
            tapDirectInputKey(uiInputAllowed ? DIK_RETURN : uiBackKeyForMenu(menuBits));
        if (!handled)
        {
            if (HWND hwnd = gameWindow())
                postMenuKey(hwnd, uiInputAllowed ? VK_RETURN : uiBackVirtualKeyForMenu(menuBits));
        }
    }
    if (menuKeyboardFallback && uiInputAllowed && (pressed & XInputBack))
    {
        // The physical left Quest/Oculus menu button is the one reliable
        // Fallout pause-menu escape hatch. Keep it independent from the
        // context-sensitive B/Tab path and inject exactly one Escape edge.
        tapDirectInputKey(DIK_ESCAPE);
        logTelemetry(
            "menuToggle fire frame=%llu source=physicalLeftMenu key=0x%02lx ui=1 menuBits=0x%02lx\n",
            static_cast<unsigned long long>(externalDInputFrame()),
            static_cast<unsigned long>(DIK_ESCAPE),
            static_cast<unsigned long>(menuBits));
    }
    if (menuKeyboardFallback
        && uiInputAllowed
        && envEnabled("FNVXR_L3_MENU_FALLBACK", true)
        && (pressed & XInputLeftThumb))
    {
        tapDirectInputKey(DIK_ESCAPE);
        logTelemetry(
            "menuToggle fire frame=%llu source=L3Fallback key=0x%02lx ui=1 menuBits=0x%02lx\n",
            static_cast<unsigned long long>(externalDInputFrame()),
            static_cast<unsigned long>(DIK_ESCAPE),
            static_cast<unsigned long>(menuBits));
    }

    if ((state.packet != g_lastExternalXInputPacket || pressed || navChanged || navRepeat)
        && (g_loggedExternalXInput < 96 || (state.packet % 240) == 0))
    {
        ++g_loggedExternalXInput;
        const char* navStick = uiNavStickSourceName(menuBits);
        logTelemetry(
            "externalXInput packet=%lu buttons=0x%04x pressed=0x%04x nav=0x%lx ui=%d gripMode=%d pipBoyGrip=%d rawPipBoyGrip=%d pipBoyGripSuppressed=%d rightGripMenu=%d rawRightGripMenu=%d rightGripMenuSuppressed=%d leftTrigger=%u directUi=%d menuKeyboardFallback=%d ls=%d,%d rs=%d,%d navStick=%s\n",
            static_cast<unsigned long>(state.packet),
            static_cast<unsigned int>(state.buttons),
            static_cast<unsigned int>(pressed),
            static_cast<unsigned long>(navMask),
            static_cast<int>(uiInputAllowed),
            static_cast<int>(gripXboxMode),
            static_cast<int>(pipBoyGripHeld),
            static_cast<int>(rawPipBoyGripHeld),
            static_cast<int>(pipBoyGripSuppressedForChord),
            static_cast<int>(rightGripMenuHeld),
            static_cast<int>(rawRightGripMenuHeld),
            static_cast<int>(rightGripMenuSuppressedForChord),
            static_cast<unsigned int>(state.leftTrigger),
            static_cast<int>(directUiClickEnabled()),
            static_cast<int>(menuKeyboardFallback),
            static_cast<int>(state.leftThumbX),
            static_cast<int>(state.leftThumbY),
            static_cast<int>(state.rightThumbX),
            static_cast<int>(state.rightThumbY),
            navStick);
    }

    g_lastExternalXInputPacket = state.packet;
    g_lastExternalXInputButtons = state.buttons;
}

void processMainGameLoop()
{
    processShowroomCarousel();
    syncExternalDInputPointer();
    consumeExternalDInputBridge();
    consumeExternalXInputBridge();

    static UInt64 runtimeFrame = 0;
    static UInt32 previousMenuBits = 0xffffffff;
    const UInt64 frame = ++runtimeFrame;
    const UInt32 menuBits = currentMenuBits();
    const RuntimePhase phase = runtimePhaseFromMenuBits(menuBits);
    const bool uiInputAllowed = uiInputAllowedFromMenuBits(menuBits);
    if (phase == RuntimePhase::Gameplay)
    {
        ensureAutoVanityCameraDisabled(frame);
        forceFirstPersonCameraMode("mainloop", frame);
    }
    else
        cancelThirdPersonL3Control("mainloop:ui", frame);
    updateSharedCamera(frame, menuBits);
    updateSharedPlayer(frame, phase);
    publishRuntimeState(frame, menuBits, phase, uiInputAllowed);
    logCameraTelemetry(frame, menuBits);
    tickUiFavoriteAssignment(frame, menuBits);
    consumeSharedCommand(frame);
    if (menuBits != previousMenuBits || frame <= 10 || (frame % 300) == 0)
    {
        previousMenuBits = menuBits;
        logTelemetry(
            "runtime state frame=%llu bits=0x%02X phase=%lu ui=%d camera=%u source=mainloop\n",
            static_cast<unsigned long long>(frame),
            menuBits,
            static_cast<unsigned long>(phase),
            static_cast<int>(uiInputAllowed),
            g_cameraState ? g_cameraState->active : 0u);
    }

    UInt32 pending = g_pendingAcceptClicks.exchange(0);
    if (pending > 5)
        pending = 5;

    for (UInt32 index = 0; index < pending; ++index)
        executeAcceptClickOnGameThread();
}

void handleNvseMessage(NVSEMessagingInterface::Message* message)
{
    if (!message)
        return;

    if (message->type == MessageMainGameLoop)
        processMainGameLoop();
}

void tapKey(WORD virtualKey)
{
    if (!currentProcessHasForegroundWindow())
        return;

    INPUT inputs[2] {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = virtualKey;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = virtualKey;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

void injectRisingEdgeInput(
    const fnvxr::PoseFrame& pose,
    std::uint64_t& previousButtons,
    bool& previousLeftTriggerDown,
    bool& previousRightTriggerDown)
{
    static std::uint64_t lastAcceptFrame = 0;
    static UInt32 lastMenuDpadMask = 0;
    static std::uint64_t lastMenuDpadFrame = 0;
    static UInt64 lastPoseUiMapZoomMs = 0;
    static bool previousPoseReloadHeld = false;
    static bool previousPoseGrabHeld = false;
    static std::uint64_t previousPoseUiFavoriteAssignChordState = 0;
    static bool previousPoseVatsChordHeld = false;
    const std::uint64_t pressed = pose.buttons & ~previousButtons;
    previousButtons = pose.buttons;
    updateControllerAxes(pose);

    const bool leftTriggerDown = pose.leftTrigger > 0.65f;
    const bool leftTriggerPressed = leftTriggerDown && !previousLeftTriggerDown;
    previousLeftTriggerDown = leftTriggerDown;
    const bool rightTriggerDown = pose.rightTrigger > 0.65f;
    const bool rightTriggerPressed = rightTriggerDown && !previousRightTriggerDown;
    previousRightTriggerDown = rightTriggerDown;
    updateMenuPointer(pose);
    updateSharedXInput(pose);
    updateSharedDInput(pose);
    updateSharedVrPose(pose);

    const bool uiInputAllowed = allowUiInput();
    const UInt32 menuBits = currentMenuBits();
    const bool pipBoyVisible = pipBoyVisibleFromMenuBits(menuBits);
    const bool menuKeyboardFallback = pluginMenuKeyboardFallbackEnabled();
    const bool gameplayKeyboardFallback = pluginGameplayKeyboardFallbackEnabled();
    if (uiInputAllowed)
        cancelThirdPersonL3Control("pose:ui", pose.frame);
    const float leftGripModifierThreshold =
        std::clamp(getFloatFromEnv("FNVXR_PIPBOY_GRIP_THRESHOLD", 0.55f), 0.0f, 1.0f);
    const bool leftGripModifierDown = pose.leftGrip > leftGripModifierThreshold;
    const bool uiFavoriteAssignLayer =
        uiInputAllowed
        && menuKeyboardFallback
        && pipBoyVisible
        && leftTriggerDown;
    const bool uiFavoriteProtectedAssign = uiFavoriteAssignLayer && leftGripModifierDown;
    constexpr std::uint64_t uiFavoriteProtectedAssignBit = 1ull << 60;
    constexpr std::uint64_t uiFavoriteFaceMask =
        fnvxr::ButtonA | fnvxr::ButtonB | fnvxr::ButtonX | fnvxr::ButtonY;
    const std::uint64_t uiFavoriteFaceButtons = pose.buttons & uiFavoriteFaceMask;
    const std::uint64_t uiFavoriteAssignChordState =
        uiFavoriteAssignLayer
            ? (uiFavoriteFaceButtons | (uiFavoriteProtectedAssign ? uiFavoriteProtectedAssignBit : 0ull))
            : 0ull;
    const bool sameUiFavoriteProtectedMode =
        (previousPoseUiFavoriteAssignChordState & uiFavoriteProtectedAssignBit)
        == (uiFavoriteAssignChordState & uiFavoriteProtectedAssignBit);
    const std::uint64_t previousUiFavoriteFaceButtons = sameUiFavoriteProtectedMode
        ? (previousPoseUiFavoriteAssignChordState & uiFavoriteFaceMask)
        : 0ull;
    const std::uint64_t uiFavoriteAssignPressed =
        uiFavoriteAssignLayer ? (uiFavoriteFaceButtons & ~previousUiFavoriteFaceButtons) : 0;
    previousPoseUiFavoriteAssignChordState = uiFavoriteAssignChordState;
    if (uiFavoriteAssignPressed & fnvxr::ButtonA)
    {
        assignUiFavoriteSlotKey(
            uiFavoriteProtectedAssign ? "pose:pipboy:LT+LG+A" : "pose:pipboy:LT+A",
            pose.frame,
            uiFavoriteProtectedAssign ? DIK_1 : DIK_5,
            uiFavoriteProtectedAssign ? "stimpak" : "combatA");
    }
    if (uiFavoriteAssignPressed & fnvxr::ButtonB)
    {
        assignUiFavoriteSlotKey(
            uiFavoriteProtectedAssign ? "pose:pipboy:LT+LG+B" : "pose:pipboy:LT+B",
            pose.frame,
            uiFavoriteProtectedAssign ? DIK_3 : DIK_6,
            uiFavoriteProtectedAssign ? "grenade" : "combatB");
    }
    if (uiFavoriteAssignPressed & fnvxr::ButtonX)
    {
        assignUiFavoriteSlotKey(
            uiFavoriteProtectedAssign ? "pose:pipboy:LT+LG+X" : "pose:pipboy:LT+X",
            pose.frame,
            uiFavoriteProtectedAssign ? DIK_4 : DIK_7,
            uiFavoriteProtectedAssign ? "backup" : "combatX");
    }
    if (uiFavoriteAssignPressed & fnvxr::ButtonY)
    {
        assignUiFavoriteSlotKey(
            uiFavoriteProtectedAssign ? "pose:pipboy:LT+LG+Y" : "pose:pipboy:LT+Y",
            pose.frame,
            uiFavoriteProtectedAssign ? DIK_2 : DIK_8,
            uiFavoriteProtectedAssign ? "ammoSwapReserved" : "combatY");
    }

    const bool buttonAcceptHeld =
        uiInputAllowed
        && !(uiFavoriteAssignLayer && (pose.buttons & fnvxr::ButtonA))
        && (pose.buttons & fnvxr::ButtonA);
    const bool buttonAcceptPressed =
        uiInputAllowed
        && !(uiFavoriteAssignPressed & fnvxr::ButtonA)
        && (pressed & fnvxr::ButtonA);
    const bool pointerClickPressed = uiInputAllowed && rightTriggerPressed && pose.menuPointerActive;
    const bool acceptRepeat =
        envEnabled("FNVXR_ACCEPT_REPEAT", false) && buttonAcceptHeld && pose.frame >= lastAcceptFrame + 18;
    const bool fallbackAcceptPressed = menuKeyboardFallback && buttonAcceptPressed;
    const bool fallbackAcceptRepeat = menuKeyboardFallback && acceptRepeat;
    const std::uint64_t cooldownFrames =
        static_cast<std::uint64_t>(getFloatFromEnv("FNVXR_ACCEPT_COOLDOWN_FRAMES", 30.0f));
    const bool acceptCooldownClear = lastAcceptFrame == 0 || pose.frame >= lastAcceptFrame + cooldownFrames;
    if ((fallbackAcceptPressed || fallbackAcceptRepeat || pointerClickPressed) && acceptCooldownClear)
    {
        lastAcceptFrame = pose.frame;
        logTelemetry(
            "accept fire frame=%llu pressed=0x%llx buttonAccept=%d pointerClick=%d repeat=%d menuKeyboardFallback=%d lt=%.3f rt=%.3f lg=%.3f rg=%.3f pointer=%d norm=(%.4f,%.4f)\n",
            static_cast<unsigned long long>(pose.frame),
            static_cast<unsigned long long>(pressed),
            static_cast<int>(buttonAcceptPressed),
            static_cast<int>(pointerClickPressed),
            static_cast<int>(acceptRepeat),
            static_cast<int>(menuKeyboardFallback),
            pose.leftTrigger,
            pose.rightTrigger,
            pose.leftGrip,
            pose.rightGrip,
            static_cast<int>(pose.menuPointerActive),
            pose.menuPointerX,
            pose.menuPointerY);
        if (pointerClickPressed && envEnabled("FNVXR_IMMEDIATE_OS_CLICK", false))
            executeImmediateInputClick();
        if (pointerClickPressed && envEnabled("FNVXR_QUEUE_ACCEPT_CLICK", true))
        {
            publishDInputMouseClick();
            requestAcceptClick();
        }
        if (fallbackAcceptPressed || fallbackAcceptRepeat)
        {
            logTelemetry("menuAccept fire frame=%llu source=A fallback=return\n", static_cast<unsigned long long>(pose.frame));
            tapDirectInputKey(DIK_RETURN);
        }
    }

    if (uiInputAllowed && menuKeyboardFallback)
    {
        constexpr float dpadDeadzone = 0.45f;
        float navX = 0.0f;
        float navY = 0.0f;
        selectPoseUiNavAxes(menuBits, pose, navX, navY);
        UInt32 dpadMask = 0;
        if (navY > dpadDeadzone)
            dpadMask |= 1u;
        if (navY < -dpadDeadzone)
            dpadMask |= 2u;
        if (navX < -dpadDeadzone)
            dpadMask |= 4u;
        if (navX > dpadDeadzone)
            dpadMask |= 8u;

        const bool dpadRepeat = dpadMask != 0 && pose.frame >= lastMenuDpadFrame + 12;
        if (dpadMask != 0 && (dpadMask != lastMenuDpadMask || dpadRepeat))
        {
            lastMenuDpadFrame = pose.frame;
            if (dpadMask & 1u)
                tapDirectInputKey(DIK_UP);
            if (dpadMask & 2u)
                tapDirectInputKey(DIK_DOWN);
            if (dpadMask & 4u)
                tapDirectInputKey(DIK_LEFT);
            if (dpadMask & 8u)
                tapDirectInputKey(DIK_RIGHT);
            logTelemetry(
                "menuDpad fire frame=%llu mask=0x%x stick=(%.3f,%.3f) navSource=%s\n",
                static_cast<unsigned long long>(pose.frame),
                dpadMask,
                navX,
                navY,
                uiNavStickSourceName(menuBits));
        }
        lastMenuDpadMask = dpadMask;
    }
    else
    {
        lastMenuDpadMask = 0;
    }

    if (uiInputAllowed && menuKeyboardFallback && uiMapZoomVisible(menuBits))
    {
        const UInt64 nowMs = GetTickCount64();
        const int mapZoomDirection = poseUiMapZoomDirection(pose);
        if (mapZoomDirection != 0 && nowMs >= lastPoseUiMapZoomMs + uiMapZoomRepeatMs())
        {
            lastPoseUiMapZoomMs = nowMs;
            publishUiMapZoom(mapZoomDirection, pose.frame, "pose:mapZoom");
        }
    }

    const bool thirdPersonChordHeld =
        !uiInputAllowed
        && thirdPersonL3ControlsEnabled()
        && (pose.buttons & fnvxr::LeftThumbstick) != 0;
    const int thirdPersonRightStickY = static_cast<int>(
        std::lround(std::clamp(pose.rightThumbstickY, -1.0f, 1.0f) * 32767.0f));
    updateThirdPersonL3Control(thirdPersonChordHeld, thirdPersonRightStickY, pose.frame, "pose:L3");
    if (!uiInputAllowed && (pressed & fnvxr::RightThumbstick))
        toggleGameplayAutoRun("pose:R3", pose.frame);
    const bool poseReloadHeld =
        !uiInputAllowed
        && gameplayKeyboardFallback
        && !(leftTriggerDown && envEnabled("FNVXR_GAMEPLAY_COMBAT_CHORDS_ENABLE", true))
        && (pose.buttons & fnvxr::ButtonX) != 0;
    holdDirectInputKey(DIK_R, poseReloadHeld);
    if (poseReloadHeld != previousPoseReloadHeld)
    {
        logTelemetry(
            "buttonX reloadHold frame=%llu held=%d source=pose:X\n",
            static_cast<unsigned long long>(pose.frame),
            static_cast<int>(poseReloadHeld));
    }
    previousPoseReloadHeld = poseReloadHeld;

    const bool combatChordHeld =
        !uiInputAllowed
        && gameplayKeyboardFallback
        && leftTriggerDown
        && envEnabled("FNVXR_GAMEPLAY_COMBAT_CHORDS_ENABLE", true);
    const bool protectedCombatChordHeld = combatChordHeld && leftGripModifierDown;
    const std::uint64_t combatChordPressed = combatChordHeld ? pressed : 0;
    const bool rightGripVatsHeld = pose.rightGrip > rightGripMenuThreshold();
    const bool vatsChordHeld =
        !uiInputAllowed
        && gameplayKeyboardFallback
        && leftTriggerDown
        && rightGripVatsHeld
        && envEnabled("FNVXR_GAMEPLAY_VATS_CHORD_ENABLE", true);
    const bool vatsChordPressed = vatsChordHeld && !previousPoseVatsChordHeld;
    previousPoseVatsChordHeld = vatsChordHeld;
    const bool poseRightGripGrabHeld =
        !uiInputAllowed
        && gameplayKeyboardFallback
        && envEnabled("FNVXR_GAMEPLAY_RIGHT_GRIP_GRAB_ENABLE", true)
        && rightGripVatsHeld
        && !leftTriggerDown;
    holdGameplayGrab(poseRightGripGrabHeld);
    if (poseRightGripGrabHeld != previousPoseGrabHeld)
    {
        logTelemetry(
            "rightGripGrab frame=%llu held=%d source=pose:RG key=0x%02lx lt=%.3f rg=%.3f\n",
            static_cast<unsigned long long>(pose.frame),
            static_cast<int>(poseRightGripGrabHeld),
            static_cast<unsigned long>(gameplayGrabKey()),
            pose.leftTrigger,
            pose.rightGrip);
    }
    previousPoseGrabHeld = poseRightGripGrabHeld;
    const bool combatChordFaceHeld =
        combatChordHeld
        && (pose.buttons & (fnvxr::ButtonA | fnvxr::ButtonB | fnvxr::ButtonX | fnvxr::ButtonY)) != 0;
    const bool suppressAimMouseForCombatChord =
        combatChordFaceHeld
        && envEnabled("FNVXR_GAMEPLAY_COMBAT_CHORD_SUPPRESS_AIM_MOUSE", true);
    if (leftTriggerPressed && !suppressAimMouseForCombatChord && !vatsChordHeld)
        tapDirectInputKey(MouseButtonOffset + 1);
    if (vatsChordPressed)
        tapVatsKey("pose:LT+RG", pose.frame);
    if (combatChordPressed & fnvxr::ButtonA)
    {
        if (protectedCombatChordHeld)
            tapStimpakKey("pose:LT+LG+A", pose.frame);
        else
            tapCombatAKey("pose:LT+A", pose.frame);
    }
    if (uiInputAllowed && menuKeyboardFallback && (pressed & fnvxr::ButtonB) && !(uiFavoriteAssignPressed & fnvxr::ButtonB))
    {
        const UInt32 backKey = uiBackKeyForMenu(menuBits);
        logTelemetry(
            "menuBack fire frame=%llu source=B key=0x%02lx pipBoy=%d\n",
            static_cast<unsigned long long>(pose.frame),
            static_cast<unsigned long>(backKey),
            static_cast<int>(pipBoyVisible));
        tapDirectInputKey(backKey);
    }
    else if (combatChordPressed & fnvxr::ButtonB)
    {
        if (protectedCombatChordHeld)
            tapGrenadeKey("pose:LT+LG+B", pose.frame);
        else
            tapCombatBKey("pose:LT+B", pose.frame);
    }
    else if (!uiInputAllowed && gameplayKeyboardFallback && (pressed & fnvxr::ButtonB))
    {
        logTelemetry("gameplayJump fire frame=%llu source=B\n", static_cast<unsigned long long>(pose.frame));
        tapDirectInputKey(DIK_SPACE);
    }
    if (uiInputAllowed
        && menuKeyboardFallback
        && (pressed & fnvxr::ButtonX)
        && !(uiFavoriteAssignPressed & fnvxr::ButtonX))
    {
        if (pipBoyVisible)
        {
            const UInt32 sortKey = uiSortKey();
            logTelemetry(
                "buttonX fire frame=%llu action=sort key=0x%02lx ui=%d pipBoy=%d\n",
                static_cast<unsigned long long>(pose.frame),
                static_cast<unsigned long>(sortKey),
                static_cast<int>(uiInputAllowed),
                static_cast<int>(pipBoyVisible));
            tapDirectInputKey(sortKey);
        }
        else
        {
            logTelemetry(
                "buttonX ignore frame=%llu action=sort ui=%d pipBoy=%d\n",
                static_cast<unsigned long long>(pose.frame),
                static_cast<int>(uiInputAllowed),
                static_cast<int>(pipBoyVisible));
        }
    }
    else if (combatChordPressed & fnvxr::ButtonX)
    {
        if (protectedCombatChordHeld)
            tapBackupKey("pose:LT+LG+X", pose.frame);
        else
            tapCombatXKey("pose:LT+X", pose.frame);
    }
    if (((uiInputAllowed && menuKeyboardFallback) || (!uiInputAllowed && gameplayKeyboardFallback))
        && (pressed & fnvxr::ButtonY)
        && !(uiFavoriteAssignPressed & fnvxr::ButtonY))
    {
        if (uiInputAllowed && pipBoyVisible)
        {
            const bool utilityFavorite = leftTriggerDown;
            assignUiFavoriteSlot(
                utilityFavorite ? "pose:pipboy:LT+Y" : "pose:pipboy:Y",
                pose.frame,
                utilityFavorite);
            logTelemetry(
                "buttonY fire frame=%llu action=%s utility=%d ui=%d pipBoy=%d\n",
                static_cast<unsigned long long>(pose.frame),
                "favoriteAssign",
                static_cast<int>(utilityFavorite),
                static_cast<int>(uiInputAllowed),
                static_cast<int>(pipBoyVisible));
        }
        else if (combatChordPressed & fnvxr::ButtonY)
        {
            if (protectedCombatChordHeld)
                tapAmmoSwapKey("pose:LT+LG+Y", pose.frame);
            else
                tapCombatYKey("pose:LT+Y", pose.frame);
        }
        else if (!uiInputAllowed)
        {
            tapDirectInputKey(DIK_LCONTROL);
            logTelemetry(
                "buttonY fire frame=%llu action=%s ui=%d pipBoy=%d\n",
                static_cast<unsigned long long>(pose.frame),
                "sneak",
                static_cast<int>(uiInputAllowed),
                static_cast<int>(pipBoyVisible));
        }
        else
        {
            logTelemetry(
                "buttonY ignore frame=%llu action=favoriteAssign ui=%d pipBoy=%d\n",
                static_cast<unsigned long long>(pose.frame),
                static_cast<int>(uiInputAllowed),
                static_cast<int>(pipBoyVisible));
        }
    }
    if (pressed & fnvxr::LeftMenu)
    {
        if (!uiInputAllowed
            && gameplayKeyboardFallback
            && leftTriggerDown
            && envEnabled("FNVXR_GAMEPLAY_WAIT_CHORD_ENABLE", true))
        {
            tapWaitKey("pose:LT+LeftMenu", pose.frame);
        }
        else if (menuKeyboardFallback)
        {
            logTelemetry("menuStart fire frame=%llu source=LeftMenu\n", static_cast<unsigned long long>(pose.frame));
            tapDirectInputKey(uiBackKeyForMenu(menuBits));
        }
    }
    if (menuKeyboardFallback && (pressed & fnvxr::RightMenu))
    {
        logTelemetry("menuSelect fire frame=%llu source=RightMenu\n", static_cast<unsigned long long>(pose.frame));
        tapDirectInputKey(DIK_ESCAPE);
    }
}

bool startBridge()
{
    if (!bridgeDisabledByEnv())
        logTelemetry("live bridge uses shared memory maps only\n");
    return true;
}

void stopBridge()
{
    releaseControllerHolds();
}
}

extern "C" __declspec(dllexport) bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info)
{
    if (!info)
        return false;

    info->infoVersion = PluginInfoVersion;
    info->name = "FNVXR";
    info->version = PluginVersion;

    return isCompatibleRuntime(nvse);
}

extern "C" __declspec(dllexport) bool NVSEPlugin_Load(const NVSEInterface* nvse)
{
    if (!isCompatibleRuntime(nvse))
        return false;

    g_nvse = nvse;
    if (nvse->GetPluginHandle)
        g_pluginHandle = nvse->GetPluginHandle();
    if (nvse->QueryInterface)
    {
        g_console = static_cast<NVSEConsoleInterface*>(nvse->QueryInterface(InterfaceConsole));
        g_messaging = static_cast<NVSEMessagingInterface*>(nvse->QueryInterface(InterfaceMessaging));
        if (g_messaging && g_messaging->RegisterListener)
        {
            const bool listenerRegistered = g_messaging->RegisterListener(g_pluginHandle, "NVSE", handleNvseMessage);
            logTelemetry("messaging listener registered=%d\n", static_cast<int>(listenerRegistered));
        }
        else
            logTelemetry("messaging listener unavailable; runtime state will not mainloop-publish\n");

        auto* data = static_cast<NVSEDataInterface*>(nvse->QueryInterface(InterfaceData));
        if (data && data->GetSingleton)
            g_directInputHook = static_cast<DirectInputHookControl*>(data->GetSingleton(NvseDataDiHookControl));
        g_playerControls = static_cast<NVSEPlayerControlsInterface*>(nvse->QueryInterface(InterfacePlayerControls));
    }
    logTelemetry(
        "load pluginHandle=0x%x nvse=%u runtime=%u di=%p messaging=%p console=%p playerControls=%p\n",
        g_pluginHandle,
        nvse->nvseVersion,
        nvse->runtimeVersion,
        g_directInputHook,
        g_messaging,
        g_console,
        g_playerControls);
    logInputConfig();
    initSharedXInput();
    initSharedDInput();
    initSharedVrPose();
    initSharedCamera();
    initSharedRuntime();
    initSharedPlayer();
    initSharedCommand();
    initSharedInputEvents();
    installCameraHook();
    installRetailRigHook();

    return g_pluginHandle != InvalidPluginHandle && startBridge();
}

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_DETACH)
    {
        restoreProjectileNodeConsumeHook();
        stopBridge();
        if (g_xinputState)
        {
            UnmapViewOfFile(g_xinputState);
            g_xinputState = nullptr;
        }
        if (g_xinputMapping)
        {
            CloseHandle(g_xinputMapping);
            g_xinputMapping = nullptr;
        }
        if (g_dinputState)
        {
            UnmapViewOfFile(g_dinputState);
            g_dinputState = nullptr;
        }
        if (g_dinputMapping)
        {
            CloseHandle(g_dinputMapping);
            g_dinputMapping = nullptr;
        }
        if (g_vrPoseState)
        {
            UnmapViewOfFile(g_vrPoseState);
            g_vrPoseState = nullptr;
        }
        if (g_vrPoseMapping)
        {
            CloseHandle(g_vrPoseMapping);
            g_vrPoseMapping = nullptr;
        }
        if (g_vrOriginState)
        {
            UnmapViewOfFile(g_vrOriginState);
            g_vrOriginState = nullptr;
        }
        if (g_vrOriginMapping)
        {
            CloseHandle(g_vrOriginMapping);
            g_vrOriginMapping = nullptr;
        }
        if (g_cameraState)
        {
            UnmapViewOfFile(g_cameraState);
            g_cameraState = nullptr;
        }
        if (g_cameraMapping)
        {
            CloseHandle(g_cameraMapping);
            g_cameraMapping = nullptr;
        }
        if (g_runtimeState)
        {
            UnmapViewOfFile(g_runtimeState);
            g_runtimeState = nullptr;
        }
        if (g_runtimeMapping)
        {
            CloseHandle(g_runtimeMapping);
            g_runtimeMapping = nullptr;
        }
        if (g_playerState)
        {
            UnmapViewOfFile(g_playerState);
            g_playerState = nullptr;
        }
        if (g_playerMapping)
        {
            CloseHandle(g_playerMapping);
            g_playerMapping = nullptr;
        }
        if (g_commandState)
        {
            UnmapViewOfFile(g_commandState);
            g_commandState = nullptr;
        }
        if (g_commandMapping)
        {
            CloseHandle(g_commandMapping);
            g_commandMapping = nullptr;
        }
        if (g_inputEvents)
        {
            UnmapViewOfFile(g_inputEvents);
            g_inputEvents = nullptr;
        }
        if (g_inputEventMapping)
        {
            CloseHandle(g_inputEventMapping);
            g_inputEventMapping = nullptr;
        }
        g_nvse = nullptr;
        g_console = nullptr;
        g_messaging = nullptr;
        g_playerControls = nullptr;
        g_pluginHandle = InvalidPluginHandle;
    }

    return TRUE;
}
