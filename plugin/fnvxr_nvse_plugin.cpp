#define WIN32_LEAN_AND_MEAN

#include "fnvxr_protocol.h"
#include "fnvxr_product_capabilities.h"
#include "fnvxr_shared_state.h"
#include "fnvxr_fabrik.h"
#include "fnvxr_desktop_assist_authority.h"
#include "fnvxr_desktop_assist_automation_authority.h"
#include "fnvxr_tracked_prop_assist_authority.h"
#include "fnvxr_retail_observation_authority.h"
#include "fnvxr_retail_runtime_publication.h"
#include "fnvxr_retail_runtime_authority.h"
#include "fnvxr_retail_safety.h"
#include "fnvxr_retail_fixture_automation_authority.h"
#include "fnvxr_headset_demo_authority.h"
#include "fnvxr_physical_input_authority.h"
#include "fnvxr_live_pipboy_contract.h"
#include "fnvxr_stereo_visual_trial_automation_authority.h"
#include "fnvxr_weapon_frame_contract.h"

#include <windows.h>
#include <intrin.h>

#include <atomic>
#include <cctype>
#include <cstdarg>
#include <chrono>
#include <cstddef>
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
static_assert(
    fnvxr::product::SelectedProductInputOwner
        == fnvxr::product::InputOwner::NvseMainGameLoop,
    "FNVVR product input is consumed only on the xNVSE main game loop");

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
constexpr const char* GamePluginProducerMutexName = "Local\\FNVXR_GamePlugin_Producer_v1_g2";
constexpr const char* DesktopAssistRecoveryLoadCommand = "load FNVXR_HostExitRecovery";
// The OpenXR host is the sole producer for XInput, DInput, and VR pose in the
// supported architecture.  Retained in-plugin writers stay source-disabled
// until they acquire the same lifetime/epoch protocol as the host.
constexpr bool LegacyNvseInputProducerEnabled = false;
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
constexpr UInt32 DIK_F1 = 0x3B;
constexpr UInt32 DIK_F2 = 0x3C;
constexpr UInt32 DIK_F3 = 0x3D;
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
constexpr UInt32 InterfaceManagerOpenPipBoyAddress = 0x0070F4E0;
constexpr UInt32 InterfaceManagerClosePipBoyAddress = 0x0070F690;
constexpr UInt32 InterfaceManagerPipBoyModeOffset = 0x4BC;
constexpr UInt32 InventoryMenuType = 1002;
constexpr UInt32 InventorySelectionAddress = 0x011D9EA8;
constexpr UInt32 ActorEquipItemAddress = 0x0088C650;
// xNVSE GameUI.h identifies these as InterfaceManager::activeTile and
// InterfaceManager::activeMenu.  The exact official-pack path below sets them
// only for the single verified native MessageMenu response; it never routes
// OS, controller, or simulator input through the interface manager.
constexpr UInt32 InterfaceManagerActiveTileOffset = 0x0CC;
constexpr UInt32 InterfaceManagerActiveMenuOffset = 0x0D0;
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
// xNVSE TESObjectREFR layout: Euler rotation is stored in radians at 0x24,
// 0x28, 0x2C; Z is the actor heading used by SetAngle.
constexpr std::uintptr_t TESObjectRefrRotationZOffset = 0x2C;
constexpr std::uintptr_t TESObjectRefrParentCellOffset = 0x40;
constexpr std::uintptr_t MobileObjectBaseProcessOffset = 0x68;
constexpr std::uintptr_t ActorActorMoverOffset = 0x190;
constexpr std::uintptr_t PlayerMoverMovementFlagsOffset = 0x94;
constexpr UInt32 ActorMoverSetMovementFlagsAddress = 0x009EA3E0;
constexpr UInt32 MovementFlagForward = 0x01;
constexpr UInt32 MovementFlagBackward = 0x02;
constexpr UInt32 MovementFlagLeft = 0x04;
constexpr UInt32 MovementFlagRight = 0x08;
constexpr UInt32 MovementFlagDirectionalMask = 0x0F;
constexpr UInt32 MovementFlagIsKeyboard = 0x40;
constexpr UInt32 MovementFlagWalking = 0x100;
constexpr UInt32 MovementFlagRunning = 0x200;
constexpr std::uintptr_t MiddleHighProcessWeaponOutOffset = 0x135;
constexpr std::uintptr_t MiddleHighProcessProjectileNodeOffset = 0x130;
// xNVSE GameProcess.h: HighProcess::forceFireWeapon is the engine-owned
// one-frame request consumed by the normal weapon pipeline.  The two virtual
// slots below are BaseProcess::GetAmmoInfo and its PlayerCharacter reload
// vtable entry.  These are used only by the leased headless combat fixture so
// the simulator can exercise real gameplay without foreground-window input.
constexpr std::uintptr_t HighProcessForceFireWeaponOffset = 0x2F4;
constexpr UInt32 BaseProcessGetWeaponInfoVtableSlot = 0x52;
constexpr UInt32 BaseProcessGetAmmoInfoVtableSlot = 0x53;
constexpr UInt32 PlayerCharacterReloadVtableAddress = 0x0108AE28;
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
constexpr UInt32 DesktopAssistSharedMagic = fnvxr::shared::DesktopAssistSharedMagic;
constexpr UInt32 DesktopAssistSharedVersion = fnvxr::shared::DesktopAssistSharedVersion;
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
// xNVSE GameUI.h: the first menu slot is the modal MessageMenu.  It is kept
// explicit because a visible MessageMenu after a load is useful diagnostic
// evidence.  It remains non-mutating by default; only the separately opted-in
// one-time exact Tribal Pack acknowledgement below may advance it.
constexpr UInt32 kMenuTypeMessage = kMenuTypeMin;
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
using fnvxr::shared::SharedWeaponFrameState;
using fnvxr::shared::SharedCameraState;
using fnvxr::shared::SharedDesktopAssistState;
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
    // The retail xNVSE DIHookControl inherits ISingleton, whose virtual
    // destructor gives this object a vtable.  This is confirmed by the PDB
    // shipped with the installed nvse_1_4.dll.  The public data interface
    // returns the complete object, so m_keys begins after this pointer.
    void* vtable;
    DirectInputKeyInfo keys[MaxDirectInputMacros];
    std::queue<DirectInputDeviceObjectData> bufferedPresses;
};

static_assert(sizeof(DirectInputKeyInfo) == 7, "xNVSE DirectInput key ABI drift");

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

constexpr float DirectMenuViewportWidth = 1280.0f;
constexpr float DirectMenuViewportHeight = 720.0f;

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
    UInt64 producerEpoch {};
    UInt32 recenterRequestId {};
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
    // Fallout keeps the hand's "Weapon" attachment node stable while
    // replacing this model root after an inventory equip.
    void* weaponModel {};
    void* upperBodyMesh {};
    void* armsGeometry0 {};
    void* armsGeometry1 {};
    void* leftHandMesh {};
    void* rightHandMesh {};
    void* pipBoy {};
    void* pipBoyScreen {};
    void* pipBoyScreenSurface {};
    void* projectileNode {};
    void* muzzleFlash {};
};

struct RetailHandCalibration
{
    bool valid {};
    bool usesAimOrientation {};
    // Controller-owned rigs place the wrist from the controller's absolute
    // pose in the recentered body frame.  Never retain the stock wrist's
    // initial gap from the controller: doing so cancels the absolute pose and
    // reduces tracking to a small delta sphere around the stock animation.
    bool usesStageLocalBodyPositionAnchor {};
    Matrix33 controllerToHandRotation {};
    Vec3 controllerToWristLocal {};
    Vec3 controllerToWristBodyLocal {};
    // Capture the stock skeleton's anatomical lengths before the first VR
    // write. Re-measuring these from a controller-owned wrist on the next
    // frame makes the prior extension look like a new, longer arm and causes
    // the render-bound reapply to oscillate between solved and rejected.
    float upperArmLength {};
    float forearmLength {};
};

struct RetailWeaponCalibration
{
    bool valid {};
    bool usesStageLocalBodyPositionAnchor {};
    bool usesTrackedWristSocketPositionAnchor {};
    bool handMeshRotationValid {};
    Matrix33 controllerToWeaponRotation {};
    Vec3 controllerToWeaponPosition {};
    Vec3 controllerToWeaponBodyLocal {};
    // Stock Fallout already authors the exact relationship between the
    // anatomical wrist and the stable Weapon attachment.  Preserve the
    // wrist point in weapon-local space so aim rotation pivots the gun about
    // its grip instead of orbiting the whole model around a stale controller
    // delta captured in one pose.
    Vec3 weaponToWristLocal {};
    Quat rightHandGripLocalRotation { 0.0f, 0.0f, 0.0f, 1.0f };
};

struct RetailPipBoyCalibration
{
    Matrix33 controllerToRootRotation {};
    Vec3 rootToScreenLocal {};
    Vec3 screenGripLocalPositionMeters {};
    Quat screenGripLocalRotation { 0.0f, 0.0f, 0.0f, 1.0f };
    bool hostSpatialValid {};
    bool valid {};
};

struct RetailRigContinuityPose
{
    bool valid {};
    void* root {};
    void* upperArm {};
    void* forearm {};
    void* hand {};
    void* weapon {};
    UInt32 referenceSpaceGeneration {};
    UInt32 consecutiveReplays {};
    Matrix33 upperArmLocalRotation {};
    Matrix33 forearmLocalRotation {};
    Matrix33 handLocalRotation {};
    Vec3 handLocalTranslation {};
    Matrix33 weaponLocalRotation {};
    Vec3 weaponLocalTranslation {};
    float weaponLocalScale {};
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

struct RuntimeObservation
{
    UInt64 frame = 0u;
    UInt32 menuBits = 0u;
    RuntimePhase phase = RuntimePhase::Unknown;
    bool uiInputAllowed = false;
    bool cameraActive = false;
    bool showroomActive = false;
};

enum class CameraHookAuthorization : UInt8
{
    None = 0,
    DesktopAssist = 1,
    TrackedPropAssist = 2,
    FullRetail = 3,
};

enum class RetailRigOriginSource : UInt8
{
    None = 0,
    NativeStereo = 1,
    TrackedPropAssist = 2,
    HeadlessStereoRigVisualTrial = 3,
};

PluginHandle g_pluginHandle = InvalidPluginHandle;
const NVSEInterface* g_nvse = nullptr;
// Intentionally empty until a production in-process validator reads and
// verifies the loaded PE/function ranges and supplies every acceptance gate.
// No environment variable or loader argument populates this record.
fnvxr::safety::RetailMutationEvidenceToken g_retailMutationEvidence {};
fnvxr::engine::RetailRuntimeAuthorityDecision g_retailRuntimeAuthority {};
bool g_authorizedSharedBridgeStarted = false;
UInt32 g_retailRuntimeAuthorityAttempts = 0;
bool g_desktopAssistBridgeStarted = false;
UInt32 g_desktopAssistAuthorityAttempts = 0;
bool g_trackedPropAssistBridgeStarted = false;
UInt32 g_trackedPropAssistAuthorityAttempts = 0;
bool g_headlessStereoRigVisualTrialBridgeStarted = false;
UInt32 g_headlessStereoRigVisualTrialAuthorityAttempts = 0;
CameraHookAuthorization g_cameraHookAuthorization = CameraHookAuthorization::None;
bool g_authorizedRuntimeObservationStarted = false;
bool g_headsetDemoFixtureReady = false;
UInt32 g_retailObservationAuthorityAttempts = 0;
UInt64 g_runtimeObservationFrame = 0;
NVSEConsoleInterface* g_console = nullptr;
NVSEMessagingInterface* g_messaging = nullptr;
NVSEPlayerControlsInterface* g_playerControls = nullptr;
DirectInputHookControl* g_directInputHook = nullptr;
bool g_publishedDirectInputHoldKnown[MaxDirectInputMacros] {};
bool g_publishedDirectInputHoldState[MaxDirectInputMacros] {};
bool g_publishedDirectInputHoldViaHook[MaxDirectInputMacros] {};
UInt64 g_publishedDirectInputHoldHeartbeatMs[MaxDirectInputMacros] {};
bool g_physicalLocomotionDirectInputApplied[MaxDirectInputMacros] {};
bool g_physicalLocomotionDirectInputUnavailableLogged[MaxDirectInputMacros] {};
std::atomic<UInt32> g_physicalPlayerMoverDirections { 0u };
std::atomic<bool> g_physicalPlayerMoverAllowed { false };
std::atomic<bool> g_physicalPlayerMoverRun { false };
fnvxr::physical_input::SnapTurnLatch g_physicalSnapTurnLatch {};
void** g_physicalPlayerMoverVtable = nullptr;
void* g_physicalPlayerMoverOriginalSetMovementFlags = nullptr;
bool g_physicalPlayerMoverHookInstalled = false;
constexpr UInt64 DirectInputHoldHeartbeatMilliseconds = 200;
constexpr bool LegacyInProcessDirectInputHoldFallbackEnabled = false;
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
UInt64 g_lastPipBoyMenuChordMs = 0;
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
void* g_directMenuLastAcceptTile = nullptr;
UInt64 g_directMenuLastAcceptMs = 0;
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
HANDLE g_weaponFrameMapping = nullptr;
SharedWeaponFrameState* g_weaponFrameState = nullptr;
UInt64 g_weaponFrameCommitId = 0;
HANDLE g_cameraMapping = nullptr;
SharedCameraState* g_cameraState = nullptr;
HANDLE g_desktopAssistMapping = nullptr;
SharedDesktopAssistState* g_desktopAssistState = nullptr;
HANDLE g_runtimeMapping = nullptr;
SharedRuntimeState* g_runtimeState = nullptr;
HANDLE g_playerMapping = nullptr;
SharedPlayerState* g_playerState = nullptr;
HANDLE g_gamePluginProducerMutex = nullptr;
bool g_gamePluginProducerMutexOwned = false;
DWORD g_gamePluginProducerThreadId = 0;
HANDLE g_gamePluginProducerThread = nullptr;
HANDLE g_commandMapping = nullptr;
SharedCommandState* g_commandState = nullptr;
HANDLE g_commandWriterMutex = nullptr;
UInt32 g_lastCommandRequestId = 0;
HANDLE g_inputEventMapping = nullptr;
SharedInputEventQueue* g_inputEvents = nullptr;
HANDLE g_inputEventWriterMutex = nullptr;
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
RetailRigOriginSource g_retailRigOriginSource = RetailRigOriginSource::None;
RetailRigNodes g_retailRigNodes {};
RetailHandCalibration g_retailLeftCalibration {};
RetailHandCalibration g_retailRightCalibration {};
RetailWeaponCalibration g_retailWeaponCalibration {};
RetailPipBoyCalibration g_retailPipBoyCalibration {};
RetailRigContinuityPose g_retailRigContinuityPose {};
void* g_livePipBoyScaleNode = nullptr;
float g_livePipBoyBaseScale = 1.0f;
float g_livePipBoyAppliedScale = 1.0f;
bool g_weaponOrbitWasActive = false;
int g_weaponOrbitSelectedSlot = -1;
Vec3 g_latestTrackedLeftHandWorld {};
Vec3 g_latestTrackedPipBoyScreenWorld {};
bool g_latestTrackedLeftHandValid = false;
bool g_latestTrackedPipBoyScreenValid = false;
LONG g_latestCompleteTrackedPropsPoseSequence = 0;
UInt64 g_latestCompleteTrackedPropsPoseFrame = 0;
bool g_latestCompleteTrackedPropsApplied = false;
void* g_retailEquippedWeaponForm = nullptr;
UInt32 g_retailEquippedWeaponFormId = 0;
UInt32 g_retailWeaponModelFormId = 0;
bool g_retailWeaponRefreshRequested = true;
// Inventory/Pip-Boy transitions can rebuild the first-person biped in place:
// the root address may survive while its child transforms and model bindings
// do not. Force one complete discovery/calibration pass when gameplay resumes.
bool g_retailRigRediscoveryRequested = true;
bool g_haveRetailRigOrigin = false;
Quat g_retailRigOriginHmdRot { 0.0f, 0.0f, 0.0f, 1.0f };
Vec3 g_retailRigOriginHmdPos {};
void* g_retailRigOriginBodyRoot = nullptr;
Vec3 g_retailRigBodyAnchorLocal {};
UInt32 g_retailRigReferenceSpaceGeneration = 0;
UInt64 g_retailRigProducerEpoch = 0;
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
bool g_renderRigPoseOverrideActive = false;
VrRigPoseSnapshot g_renderRigPoseOverride {};
LONG g_retailRigPoseOriginUnavailableCount = 0;
LONG g_retailRigPoseOriginSkewCount = 0;
LONG g_retailRigNoHmdCount = 0;
LONG g_retailRigNoCurrentControllerCount = 0;
LONG g_retailRigNoRootCount = 0;
LONG g_retailRigDiscoveryFailureCount = 0;
LONG g_retailRigIncompleteCount = 0;
LONG g_retailRigNoBodyRootCount = 0;
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
UInt64 g_lastCameraPoseProducerEpoch = 0;
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
void logReadOnlyMessageMenuDiagnostic(UInt64 generation);
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

const char* retailRigOriginSourceName()
{
    switch (g_retailRigOriginSource)
    {
    case RetailRigOriginSource::NativeStereo:
        return "d3d9-native-camera";
    case RetailRigOriginSource::TrackedPropAssist:
        return "tracked-prop-assist-body";
    case RetailRigOriginSource::HeadlessStereoRigVisualTrial:
        return "headless-stereo-rig-body";
    default:
        return "unlatched";
    }
}

const char* retailRigAnchorSourceName()
{
    switch (g_retailRigOriginSource)
    {
    case RetailRigOriginSource::NativeStereo:
        return "exact-d3d9-render-camera";
    case RetailRigOriginSource::TrackedPropAssist:
    case RetailRigOriginSource::HeadlessStereoRigVisualTrial:
        return "first-person-rig-root-at-latch";
    default:
        return "unlatched";
    }
}

bool isCompatibleRuntime(const NVSEInterface* nvse)
{
    if (!nvse)
        return false;
    return fnvxr::safety::compatibleNvseRuntime(
        nvse->isEditor != 0,
        nvse->nvseVersion,
        nvse->runtimeVersion);
}

bool rejectUntilCurrentProcessIntegrityValidatorImplemented(
    const fnvxr::safety::RetailMutationEvidenceToken&,
    void*) noexcept
{
    // Deliberately fail closed until the plugin can synchronously re-read the
    // loaded PE, protected function bytes, and compatibility-module inventory
    // at each mutation decision. Flipping the source fuse alone is insufficient.
    return false;
}

bool retailMutationAllowedForCurrentProcess(bool requested)
{
    return fnvxr::safety::retailMutationAllowed(
        requested,
        g_retailMutationEvidence,
        GetCurrentProcessId(),
        rejectUntilCurrentProcessIntegrityValidatorImplemented,
        nullptr);
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

bool envEnabled(const char* name, bool fallback);

bool windowsForegroundInputForbidden()
{
    return envEnabled("FNVXR_WINDOWS_FOREGROUND_INPUT_FORBIDDEN", false);
}

bool desktopAssistProfileSelected()
{
    return runProfileIs("desktop-assist");
}

bool desktopAssistProfileRequested()
{
    return desktopAssistProfileSelected()
        && envEnabled("FNVXR_DESKTOP_ASSIST_CAMERA_ONLY", false);
}

bool trackedPropAssistProfileSelected()
{
    return runProfileIs("tracked-prop-assist");
}

bool trackedPropAssistProfileRequested()
{
    return trackedPropAssistProfileSelected()
        && envEnabled("FNVXR_TRACKED_PROP_ASSIST_VISUAL_ONLY", false);
}

bool stereoVisualTrialProfileSelected()
{
    return runProfileIs("stereo-visual-trial-v5");
}

bool physicalHeadsetPlayProfileSelected()
{
    return runProfileIs("retail-vr-play-v1");
}

bool physicalHeadsetPlayRequested()
{
    return physicalHeadsetPlayProfileSelected()
        && envEnabled("FNVXR_PHYSICAL_HEADSET_PLAY", false);
}

bool physicalHeadsetEngineCenterRigRequested()
{
    return physicalHeadsetPlayRequested()
        && envEnabled("FNVXR_ENABLE_ENGINE_CENTER_STEREO", false);
}

bool hostSpatialPropReplacementRequested()
{
    // The final OpenXR eye owns these replacement categories directly. The
    // retail scene seam may still move the stock weapon, but must not also
    // mutate hidden arm/hand/Pip-Boy nodes and create two transform owners.
    return envEnabled("FNVXR_SPATIAL_HANDS_OVERLAY", false)
        && (stereoVisualTrialProfileSelected()
            || physicalHeadsetPlayProfileSelected());
}

bool retailFixtureProfileSelected()
{
    // These profiles exist solely for command-line-owned save fixtures. They
    // deliberately does not initialize OpenXR, a simulator, a renderer bridge,
    // camera/rig hooks, controller paths, or desktop input.
    return runProfileIs("retail-fixture-v1") || runProfileIs("ttw-fixture-v1");
}

bool ttwBaselineProfileSelected()
{
    // A TTW baseline check is deliberately less authoritative than the owned
    // retail fixture.  It observes only the real Start Menu after the exact
    // TTW core profile has loaded.  It has no mailbox, console, save, input,
    // camera, renderer, bridge, OpenXR, or simulator behavior.
    return runProfileIs("ttw-baseline-v1");
}

bool headsetDemoFixtureProfileSelected()
{
    // This is the sole opt-in that lets an already-owned fixture participate
    // in a bounded headless OpenXR visual run. The separate world-only key
    // below can close the demo UI gate while retaining this exact fixture
    // route. Neither mode enables the normal input, camera, rig, weapon, or
    // simulator-control bridge. A separate world-only key can request one
    // verified stock-weapon draw for the named owned fixture only.
    return stereoVisualTrialProfileSelected()
        && envEnabled("FNVXR_HEADSET_DEMO_FIXTURE", false);
}

bool headsetWorldOnlyCaptureProfileSelected()
{
    return headsetDemoFixtureProfileSelected()
        && envEnabled("FNVXR_HEADSET_WORLD_ONLY_CAPTURE", false);
}

bool headsetWorldOnlyFixtureWeaponDrawRequested()
{
    return headsetWorldOnlyCaptureProfileSelected()
        && envEnabled("FNVXR_HEADSET_FIXTURE_DRAW_WEAPON", false);
}

bool readOnlyFirstPersonSemanticsRequested()
{
    // Read-only semantic publication for the guide-mandated intact fallback.
    // This mode gives the renderer player/weapon/root identity so it can lease
    // the complete stock RenderFirstPerson call; it never authorizes the
    // post-animation rig hook or any transform write.
    const bool stockPrivateBaseline =
        headsetWorldOnlyFixtureWeaponDrawRequested()
        && envEnabled("FNVXR_STOCK_FIRST_PERSON_BASELINE", false);
    const bool stockCenterCollection =
        headsetDemoFixtureProfileSelected()
        && envEnabled("FNVXR_RETAIL_CENTER_INTEGRATED_FIRST_PERSON", false);
    return (stockPrivateBaseline || stockCenterCollection)
        && envEnabled("OPENXR_SIMULATOR_HEADLESS", false)
        && !envEnabled("FNVXR_HEADSET_CONTROLLER_RIG_VISUAL_TRIAL", false)
        && !envEnabled("FNVXR_PHYSICAL_HEADSET_PLAY", false);
}

bool headsetControllerRigVisualTrialRequested()
{
    // This is intentionally headless-only and fixture-only. It adds the
    // controller-driven visual first-person rig to the bounded stereo trial;
    // it does not grant a physical headset, input, firing, or projectile path.
    return headsetWorldOnlyFixtureWeaponDrawRequested()
        && envEnabled("FNVXR_HEADSET_CONTROLLER_RIG_VISUAL_TRIAL", false)
        && envEnabled("OPENXR_SIMULATOR_HEADLESS", false)
        && !envEnabled("FNVXR_PHYSICAL_HEADSET_PLAY", false);
}

bool retailRigVisualOnlyTrialRequested()
{
    return trackedPropAssistProfileRequested()
        || headsetControllerRigVisualTrialRequested();
}

bool headsetDemoUiProfileSelected()
{
    return headsetDemoFixtureProfileSelected()
        && !headsetWorldOnlyCaptureProfileSelected();
}

fnvxr::engine::RetailPluginMainLoopDisposition
stereoVisualTrialMainLoopDisposition()
{
    return fnvxr::engine::retailPluginMainLoopDisposition({
        stereoVisualTrialProfileSelected(),
        envEnabled("FNVXR_ENABLE_ENGINE_CENTER_STEREO", false),
    });
}

bool stereoVisualTrialAutomationRequested()
{
    // Publication-only visual trials remain command-inert by default.  This
    // opt-in maps only the command mailbox consumed by the fixed automation
    // authority gate; it does not start the plugin bridge or any input,
    // camera, rig, renderer, or weapon authority.  The two workflows are
    // deliberately mutually exclusive so no caller can combine a load with a
    // fresh-character creation sequence.
    const bool recoveryLoadRequested = envEnabled(
        "FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_RECOVERY_LOAD",
        false);
    const bool freshCharacterRequested = envEnabled(
        "FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_FRESH_CHARACTER",
        false);
    return stereoVisualTrialMainLoopDisposition()
            == fnvxr::engine::RetailPluginMainLoopDisposition::
                PublishRuntimeOnly
        && recoveryLoadRequested != freshCharacterRequested;
}

bool stereoVisualTrialRecoveryLoadRequested()
{
    return stereoVisualTrialAutomationRequested()
        && envEnabled(
            "FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_RECOVERY_LOAD",
            false);
}

bool stereoVisualTrialFreshCharacterRequested()
{
    return stereoVisualTrialAutomationRequested()
        && envEnabled(
            "FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_FRESH_CHARACTER",
            false);
}

bool retailFixtureAutomationRequested()
{
    // Fixture ownership is deliberately distinct from the legacy single-save
    // visual trial. The dedicated profile never starts an input bridge, camera
    // hook, rig hook, renderer mutation, OpenXR host, or simulator control.
    return (retailFixtureProfileSelected()
            || headsetDemoFixtureProfileSelected())
        && envEnabled("FNVXR_RETAIL_FIXTURE_AUTOMATION", false);
}

bool physicalHeadsetFixtureMessageAcknowledgementRequested()
{
    // Physical play uses the full game-loop bridge to consume its owned
    // fixture-load command.  It deliberately does not enter the fixture
    // lifecycle state machine, but it may need to acknowledge one of the
    // exact, versioned retail fixture notices that otherwise blocks the
    // loaded owned save.  Keep this independently and explicitly gated so it
    // grants no general physical-play menu authority.
    return physicalHeadsetPlayProfileSelected()
        && envEnabled("FNVXR_PHYSICAL_HEADSET_PLAY", false)
        && envEnabled("FNVXR_RETAIL_FIXTURE_AUTOMATION", false);
}

bool ownedRetailFixtureMessageAcknowledgementRequested()
{
    return retailFixtureAutomationRequested()
        || physicalHeadsetFixtureMessageAcknowledgementRequested();
}

struct RetailFixtureAutomationPlan
{
    fnvxr::engine::retail_fixture_automation::Plan plan {};
    char saveName[64] {};
};

bool readRetailFixtureAutomationPlan(RetailFixtureAutomationPlan& output)
{
    namespace fixture = fnvxr::engine::retail_fixture_automation;
    constexpr char actionEnvironmentName[] = "FNVXR_RETAIL_FIXTURE_ACTION";
    constexpr char saveEnvironmentName[] = "FNVXR_RETAIL_FIXTURE_SAVE_NAME";
    constexpr char firstTraitEnvironmentName[] = "FNVXR_RETAIL_FIXTURE_TRAIT_ONE";
    constexpr char secondTraitEnvironmentName[] = "FNVXR_RETAIL_FIXTURE_TRAIT_TWO";
    constexpr char weaponEnvironmentName[] = "FNVXR_RETAIL_FIXTURE_WEAPON";
    char action[16] {};
    char firstTrait[32] {};
    char secondTrait[32] {};
    char weapon[32] {};
    size_t actionRequired = 0u;
    size_t saveRequired = 0u;
    size_t firstTraitRequired = 0u;
    size_t secondTraitRequired = 0u;
    size_t weaponRequired = 0u;
    if (getenv_s(&actionRequired, action, sizeof(action), actionEnvironmentName) != 0
        || actionRequired == 0u || actionRequired > sizeof(action)
        || getenv_s(&saveRequired, output.saveName, sizeof(output.saveName), saveEnvironmentName) != 0
        || saveRequired == 0u || saveRequired > sizeof(output.saveName)
        || getenv_s(&firstTraitRequired, firstTrait, sizeof(firstTrait), firstTraitEnvironmentName) != 0
        || firstTraitRequired == 0u || firstTraitRequired > sizeof(firstTrait)
        || getenv_s(&secondTraitRequired, secondTrait, sizeof(secondTrait), secondTraitEnvironmentName) != 0
        || secondTraitRequired == 0u || secondTraitRequired > sizeof(secondTrait)
        || getenv_s(&weaponRequired, weapon, sizeof(weapon), weaponEnvironmentName) != 0
        || weaponRequired == 0u || weaponRequired > sizeof(weapon))
    {
        return false;
    }

    const fixture::TraitCommand* const first = fixture::findTrait(
        std::string_view { firstTrait, firstTraitRequired - 1u });
    const fixture::TraitCommand* const second = fixture::findTrait(
        std::string_view { secondTrait, secondTraitRequired - 1u });
    const fixture::WeaponCommand* const selectedWeapon = fixture::findWeapon(
        std::string_view { weapon, weaponRequired - 1u });
    fixture::Action requestedAction = fixture::Action::None;
    if (std::strcmp(action, "create") == 0)
        requestedAction = fixture::Action::Create;
    else if (std::strcmp(action, "load") == 0)
        requestedAction = fixture::Action::Load;
    if (first == nullptr || second == nullptr || selectedWeapon == nullptr)
        return false;

    output.plan = {
        requestedAction,
        first->trait,
        second->trait,
        selectedWeapon->weapon,
        std::string_view { output.saveName, saveRequired - 1u },
    };
    return fixture::authorized(output.plan);
}

bool stereoVisualTrialTribalPackAcknowledgementRequested()
{
    // This is deliberately narrower than visual-trial automation itself:
    // only the fixed recovery-save workflow can request the four known
    // official-pack MessageMenu acknowledgements.  It does not enable an
    // input bridge.
    return stereoVisualTrialRecoveryLoadRequested()
        && envEnabled(
            "FNVXR_STEREO_VISUAL_TRIAL_ACK_TRIBAL_PACK_POPUP",
            false);
}

bool retailFixtureOfficialPackAcknowledgementRequested()
{
    // Retail keeps the four pre-order packs available even with an
    // FalloutNV.esm-only plugins.txt profile. A fixture can therefore opt in
    // to the same exact native acknowledgement only for the known title/body
    // pairs and their unique stock first-button OK tile. This is not desktop,
    // keyboard, mouse, controller, or simulator input.
    return ownedRetailFixtureMessageAcknowledgementRequested()
        && envEnabled(
            "FNVXR_RETAIL_FIXTURE_ACK_OFFICIAL_PACK_POPUP",
            false);
}

bool exactOfficialPackAcknowledgementRequested()
{
    return stereoVisualTrialTribalPackAcknowledgementRequested()
        || retailFixtureOfficialPackAcknowledgementRequested();
}

bool retailFixtureTtwStewieDependencyAcknowledgementRequested()
{
    // This opt-in is emitted only for the owned TTW fixture family. The
    // decision point still requires the complete, versioned title/body pair
    // and one native first-button OK tile; it grants no general menu input.
    return ownedRetailFixtureMessageAcknowledgementRequested()
        && envEnabled(
            "FNVXR_RETAIL_FIXTURE_ACK_TTW_STEWIE_DEPENDENCY_WARNING",
            false);
}

const fnvxr::engine::stereo_visual_trial_automation::ApprovedRetailSave*
stereoVisualTrialSelectedRetailSave()
{
    namespace automation =
        fnvxr::engine::stereo_visual_trial_automation;
    constexpr char selectionEnvironmentName[] =
        "FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_RECOVERY_SAVE_NAME";
    char saveName[64] {};
    size_t required = 0u;
    if (getenv_s(
            &required,
            saveName,
            sizeof(saveName),
            selectionEnvironmentName) != 0
        || required == 0u
        || required > sizeof(saveName))
    {
        return nullptr;
    }

    return automation::findApprovedRetailSave(
        std::string_view { saveName, required - 1u });
}

bool desktopAssistAutomationRequested()
{
    // This does not broaden desktop assist into a general command or input
    // bridge.  It enables only the fixed recovery-load action checked again
    // at the point where the command is consumed.
    return desktopAssistProfileRequested()
        && envEnabled("FNVXR_DESKTOP_ASSIST_AUTOMATION", false);
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

Quat gravityAlignedYawQuat(Quat input, Quat fallback = { 0.0f, 0.0f, 0.0f, 1.0f })
{
    const Quat q = normalizeQuat(input);
    const float forwardX = -2.0f * (q.x * q.z + q.w * q.y);
    const float forwardZ = -(1.0f - 2.0f * (q.x * q.x + q.y * q.y));
    const float horizontalForwardSquared = forwardX * forwardX + forwardZ * forwardZ;
    const Quat headingSource = (!std::isfinite(horizontalForwardSquared)
            || horizontalForwardSquared < 0.000001f)
        ? normalizeQuat(fallback)
        : q;
    const float yaw = std::atan2(
        2.0f * (headingSource.x * headingSource.z + headingSource.w * headingSource.y),
        1.0f - 2.0f * (headingSource.x * headingSource.x + headingSource.y * headingSource.y));
    const float halfYaw = yaw * 0.5f;
    return { 0.0f, std::sin(halfYaw), 0.0f, std::cos(halfYaw) };
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

Quat quatFromMatrix(const Matrix33& matrix)
{
    const float trace = matrix.m[0][0] + matrix.m[1][1] + matrix.m[2][2];
    Quat result {};
    if (trace > 0.0f)
    {
        const float scale = 2.0f * std::sqrt((std::max)(0.0f, trace + 1.0f));
        if (scale < 0.000001f)
            return { 0.0f, 0.0f, 0.0f, 1.0f };
        result.w = 0.25f * scale;
        result.x = (matrix.m[2][1] - matrix.m[1][2]) / scale;
        result.y = (matrix.m[0][2] - matrix.m[2][0]) / scale;
        result.z = (matrix.m[1][0] - matrix.m[0][1]) / scale;
    }
    else if (matrix.m[0][0] > matrix.m[1][1]
        && matrix.m[0][0] > matrix.m[2][2])
    {
        const float scale = 2.0f * std::sqrt((std::max)(
            0.0f,
            1.0f + matrix.m[0][0] - matrix.m[1][1] - matrix.m[2][2]));
        if (scale < 0.000001f)
            return { 0.0f, 0.0f, 0.0f, 1.0f };
        result.w = (matrix.m[2][1] - matrix.m[1][2]) / scale;
        result.x = 0.25f * scale;
        result.y = (matrix.m[0][1] + matrix.m[1][0]) / scale;
        result.z = (matrix.m[0][2] + matrix.m[2][0]) / scale;
    }
    else if (matrix.m[1][1] > matrix.m[2][2])
    {
        const float scale = 2.0f * std::sqrt((std::max)(
            0.0f,
            1.0f + matrix.m[1][1] - matrix.m[0][0] - matrix.m[2][2]));
        if (scale < 0.000001f)
            return { 0.0f, 0.0f, 0.0f, 1.0f };
        result.w = (matrix.m[0][2] - matrix.m[2][0]) / scale;
        result.x = (matrix.m[0][1] + matrix.m[1][0]) / scale;
        result.y = 0.25f * scale;
        result.z = (matrix.m[1][2] + matrix.m[2][1]) / scale;
    }
    else
    {
        const float scale = 2.0f * std::sqrt((std::max)(
            0.0f,
            1.0f + matrix.m[2][2] - matrix.m[0][0] - matrix.m[1][1]));
        if (scale < 0.000001f)
            return { 0.0f, 0.0f, 0.0f, 1.0f };
        result.w = (matrix.m[1][0] - matrix.m[0][1]) / scale;
        result.x = (matrix.m[0][2] + matrix.m[2][0]) / scale;
        result.y = (matrix.m[1][2] + matrix.m[2][1]) / scale;
        result.z = 0.25f * scale;
    }
    return normalizeQuat(result);
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

Matrix33 xrDeltaToNiCameraMatrix(Quat xrDelta)
{
    // OpenXR local axes are right/up/back. The audited retail NiCamera
    // transform stores forward/up/right, so convert the rotation with the
    // corresponding proper basis change before composing it with a camera.
    const Quat xr = normalizeQuat(xrDelta);
    return matrixFromQuat({ -xr.z, xr.y, xr.x, xr.w });
}

Vec3 xrDeltaToNiCameraVector(Vec3 xrDelta)
{
    // Vector form of the same right/up/back -> forward/up/right conversion.
    return { -xrDelta.z, xrDelta.y, xrDelta.x };
}

Matrix33 yawOnlyNiCameraMatrix(Matrix33 matrix)
{
    // NiCamera's local Y axis is up. Its X/Z axes are forward/right, unlike
    // the actor matrix handled by yawOnlyMatrix above.
    const float yaw = std::atan2(matrix.m[0][2], matrix.m[0][0])
        * getFloatFromEnv("FNVXR_CAMERA_YAW_SCALE", 1.0f);
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);
    Matrix33 result {};
    result.m[0][0] = c;
    result.m[0][2] = s;
    result.m[1][1] = 1.0f;
    result.m[2][0] = -s;
    result.m[2][2] = c;
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

// Return an OpenXR position in the gravity-aligned gameplay-origin frame.
// g_vrOriginRot is latched as yaw-only so recenter pitch/roll can never tilt
// room translation into the gravity axis.
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
    UInt64 producerEpoch = 0;
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
            producerEpoch = g_vrPoseState->producerEpoch;
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

    if (!haveStableSnapshot || producerEpoch == 0)
        return false;

    if (g_lastCameraPoseProducerEpoch != 0
        && g_lastCameraPoseProducerEpoch != producerEpoch)
    {
        // A new pose producer has its own local origin. Never carry a prior
        // headset or synthetic-fixture origin into this trace.
        g_haveVrOrigin = false;
        g_lastAppliedCameraPoseSequence = 0;
        logTelemetry(
            "camera pose producer changed oldEpoch=%llu newEpoch=%llu; origin reset\n",
            static_cast<unsigned long long>(g_lastCameraPoseProducerEpoch),
            static_cast<unsigned long long>(producerEpoch));
    }

    if (!g_haveVrOrigin || envEnabled("FNVXR_CAMERA_RESET_ORIGIN", false))
    {
        g_vrOriginRot = gravityAlignedYawQuat(currentRot, g_vrOriginRot);
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
    g_lastCameraPoseProducerEpoch = producerEpoch;
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
        "inputConfig windowsForegroundInputForbidden=%d cursorTrack=%d cursorFocus=%d sendInputMouse=%d postKeys=%d immediate=%d queue=%d acceptRepeat=%d cooldown=%.1f pointerScale=(%.3f,%.3f) pointerOffset=(%.3f,%.3f) uiShared=%dx%d uiInput=%dx%d\n",
        static_cast<int>(windowsForegroundInputForbidden()),
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

bool currentProcessHasActiveWindow()
{
    HWND active = GetActiveWindow();
    if (!active)
        return false;

    DWORD processId = 0;
    GetWindowThreadProcessId(active, &processId);
    return processId == GetCurrentProcessId();
}

bool focusProcessWindow(HWND hwnd)
{
    if (windowsForegroundInputForbidden() || !hwnd)
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
    // This plugin is a reader plus owner of explicitly documented atomic
    // mailbox bytes. It must not initialize host-owned producer fields.
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
    // Leave a new/invalid map zeroed until the InputCore lease owner publishes.
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

    g_vrPoseMapping = OpenFileMappingA(
        FILE_MAP_READ,
        FALSE,
        fnvxr::shared::VrPoseSharedMappingName);
    if (!g_vrPoseMapping)
    {
        logTelemetry("vr pose shared OpenFileMapping failed err=%lu\n", GetLastError());
        return;
    }

    g_vrPoseState = static_cast<SharedVrPoseState*>(
        MapViewOfFile(g_vrPoseMapping, FILE_MAP_READ, 0, 0, sizeof(SharedVrPoseState)));
    if (!g_vrPoseState)
    {
        logTelemetry("vr pose shared MapViewOfFile failed err=%lu\n", GetLastError());
        CloseHandle(g_vrPoseMapping);
        g_vrPoseMapping = nullptr;
        return;
    }

    const bool existingValid = g_vrPoseState->magic == VrPoseSharedMagic
        && g_vrPoseState->version == VrPoseSharedVersion;
    // VR_Pose is host-owned. An invalid header remains unreadable until the
    // InputCore producer publishes; reader startup never mutates its seqlock.
    logTelemetry("vr pose shared ready state=%p mapping=%p existing=%d seq=%ld frame=%llu\n",
        g_vrPoseState,
        g_vrPoseMapping,
        static_cast<int>(existingValid),
        g_vrPoseState->sequence,
        static_cast<unsigned long long>(g_vrPoseState->frame));
}

bool acquireGamePluginProducerLease()
{
    if (g_gamePluginProducerMutexOwned)
        return true;

    g_gamePluginProducerMutex = CreateMutexA(
        nullptr,
        FALSE,
        GamePluginProducerMutexName);
    if (!g_gamePluginProducerMutex)
    {
        logTelemetry("game plugin producer lease CreateMutex failed err=%lu\n", GetLastError());
        return false;
    }

    const DWORD wait = WaitForSingleObject(g_gamePluginProducerMutex, 0);
    if (wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED)
    {
        const DWORD ownerThreadId = GetCurrentThreadId();
        HANDLE ownerThread = OpenThread(SYNCHRONIZE, FALSE, ownerThreadId);
        if (!ownerThread)
        {
            ReleaseMutex(g_gamePluginProducerMutex);
            CloseHandle(g_gamePluginProducerMutex);
            g_gamePluginProducerMutex = nullptr;
            logTelemetry(
                "game plugin producer lease owner-thread handle failed err=%lu\n",
                GetLastError());
            return false;
        }
        g_gamePluginProducerMutexOwned = true;
        g_gamePluginProducerThreadId = ownerThreadId;
        g_gamePluginProducerThread = ownerThread;
        logTelemetry(
            "game plugin producer lease acquired name=%s abandoned=%d pid=%lu\n",
            GamePluginProducerMutexName,
            static_cast<int>(wait == WAIT_ABANDONED),
            GetCurrentProcessId());
        return true;
    }

    logTelemetry(
        "game plugin producer lease refused name=%s wait=%lu err=%lu; another retail process owns camera/runtime/player publication\n",
        GamePluginProducerMutexName,
        wait,
        GetLastError());
    CloseHandle(g_gamePluginProducerMutex);
    g_gamePluginProducerMutex = nullptr;
    return false;
}

bool gamePluginProducerLeaseHeldByCurrentThread()
{
    return g_gamePluginProducerMutexOwned
        && g_gamePluginProducerThreadId != 0
        && g_gamePluginProducerThreadId == GetCurrentThreadId()
        && g_gamePluginProducerThread
        && WaitForSingleObject(g_gamePluginProducerThread, 0) == WAIT_TIMEOUT;
}

template <typename T>
bool publishNeutralPluginOwnedState(
    T* state,
    std::uint32_t magic,
    std::uint32_t version)
{
    if (!state || !gamePluginProducerLeaseHeldByCurrentThread())
        return false;

    const LONG sequence = InterlockedCompareExchange(&state->sequence, 0, 0);
    const bool ownsInheritedOddWrite = (sequence & 1) != 0;
    if (!ownsInheritedOddWrite
        && !fnvxr::shared::beginSequencedSharedWrite(state->sequence))
    {
        return false;
    }

    // Preserve the odd transaction marker while replacing every other byte.
    // Resetting the whole struct would briefly publish an even zero sequence
    // to readers that keep this mapping alive across a retail restart.
    auto* bytes = reinterpret_cast<std::uint8_t*>(state);
    constexpr std::size_t sequenceOffset = offsetof(T, sequence);
    std::memset(bytes, 0, sequenceOffset);
    std::memset(
        bytes + sequenceOffset + sizeof(state->sequence),
        0,
        sizeof(T) - sequenceOffset - sizeof(state->sequence));
    state->magic = magic;
    state->version = version;
    fnvxr::shared::endSequencedSharedWrite(state->sequence);
    return true;
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
    const DWORD createError = GetLastError();

    g_cameraState = static_cast<SharedCameraState*>(
        MapViewOfFile(g_cameraMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedCameraState)));
    if (!g_cameraState)
    {
        logTelemetry("camera shared MapViewOfFile failed err=%lu\n", GetLastError());
        CloseHandle(g_cameraMapping);
        g_cameraMapping = nullptr;
        return;
    }

    if (!publishNeutralPluginOwnedState(g_cameraState, CameraSharedMagic, CameraSharedVersion))
    {
        logTelemetry("camera shared neutral publication failed; disabling publisher\n");
        UnmapViewOfFile(g_cameraState);
        g_cameraState = nullptr;
        CloseHandle(g_cameraMapping);
        g_cameraMapping = nullptr;
        return;
    }
    logTelemetry("camera shared ready state=%p mapping=%p retained=%d\n",
        g_cameraState,
        g_cameraMapping,
        static_cast<int>(createError == ERROR_ALREADY_EXISTS));
}

void initSharedDesktopAssist()
{
    if (g_desktopAssistState)
        return;

    g_desktopAssistMapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(SharedDesktopAssistState),
        fnvxr::shared::DesktopAssistSharedMappingName);
    if (!g_desktopAssistMapping)
    {
        logTelemetry("desktopAssist shared CreateFileMapping failed err=%lu\n", GetLastError());
        return;
    }
    const DWORD createError = GetLastError();

    g_desktopAssistState = static_cast<SharedDesktopAssistState*>(
        MapViewOfFile(
            g_desktopAssistMapping,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            sizeof(SharedDesktopAssistState)));
    if (!g_desktopAssistState)
    {
        logTelemetry("desktopAssist shared MapViewOfFile failed err=%lu\n", GetLastError());
        CloseHandle(g_desktopAssistMapping);
        g_desktopAssistMapping = nullptr;
        return;
    }

    if (!publishNeutralPluginOwnedState(
            g_desktopAssistState,
            DesktopAssistSharedMagic,
            DesktopAssistSharedVersion))
    {
        logTelemetry("desktopAssist shared neutral publication failed; disabling publisher\n");
        UnmapViewOfFile(g_desktopAssistState);
        g_desktopAssistState = nullptr;
        CloseHandle(g_desktopAssistMapping);
        g_desktopAssistMapping = nullptr;
        return;
    }
    logTelemetry("desktopAssist shared ready state=%p mapping=%p bytes=%zu retained=%d\n",
        g_desktopAssistState,
        g_desktopAssistMapping,
        sizeof(SharedDesktopAssistState),
        static_cast<int>(createError == ERROR_ALREADY_EXISTS));
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
    const DWORD createError = GetLastError();

    g_runtimeState = static_cast<SharedRuntimeState*>(
        MapViewOfFile(g_runtimeMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedRuntimeState)));
    if (!g_runtimeState)
    {
        logTelemetry("runtime shared MapViewOfFile failed err=%lu\n", GetLastError());
        CloseHandle(g_runtimeMapping);
        g_runtimeMapping = nullptr;
        return;
    }

    if (!publishNeutralPluginOwnedState(g_runtimeState, RuntimeSharedMagic, RuntimeSharedVersion))
    {
        logTelemetry("runtime shared neutral publication failed; disabling publisher\n");
        UnmapViewOfFile(g_runtimeState);
        g_runtimeState = nullptr;
        CloseHandle(g_runtimeMapping);
        g_runtimeMapping = nullptr;
        return;
    }
    logTelemetry("runtime shared ready state=%p mapping=%p bytes=%zu retained=%d\n",
        g_runtimeState,
        g_runtimeMapping,
        sizeof(SharedRuntimeState),
        static_cast<int>(createError == ERROR_ALREADY_EXISTS));
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
    const DWORD createError = GetLastError();

    g_playerState = static_cast<SharedPlayerState*>(
        MapViewOfFile(g_playerMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedPlayerState)));
    if (!g_playerState)
    {
        logTelemetry("player shared MapViewOfFile failed err=%lu\n", GetLastError());
        CloseHandle(g_playerMapping);
        g_playerMapping = nullptr;
        return;
    }

    if (!publishNeutralPluginOwnedState(g_playerState, PlayerSharedMagic, PlayerSharedVersion))
    {
        logTelemetry("player shared neutral publication failed; disabling publisher\n");
        UnmapViewOfFile(g_playerState);
        g_playerState = nullptr;
        CloseHandle(g_playerMapping);
        g_playerMapping = nullptr;
        return;
    }
    logTelemetry("player shared ready state=%p mapping=%p bytes=%zu retained=%d\n",
        g_playerState,
        g_playerMapping,
        sizeof(SharedPlayerState),
        static_cast<int>(createError == ERROR_ALREADY_EXISTS));
}

bool acquireSharedCommandWriter(DWORD timeoutMilliseconds = 2000)
{
    if (!g_commandWriterMutex)
        return false;
    const DWORD wait = WaitForSingleObject(g_commandWriterMutex, timeoutMilliseconds);
    return wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED;
}

void releaseSharedCommandWriter()
{
    if (g_commandWriterMutex)
        ReleaseMutex(g_commandWriterMutex);
}

void repairSharedCommandState(bool recoverOrphanedRunning = false)
{
    if (!g_commandState)
        return;
    if (g_commandState->magic != CommandSharedMagic
        || g_commandState->version != CommandSharedVersion)
    {
        std::memset(g_commandState, 0, sizeof(*g_commandState));
        g_commandState->magic = CommandSharedMagic;
        g_commandState->version = CommandSharedVersion;
        g_commandState->status = fnvxr::shared::CommandStatusIdle;
        return;
    }
    if ((g_commandState->sequence & 1) != 0)
    {
        g_commandState->status = fnvxr::shared::CommandStatusFailed;
        g_commandState->resultCode = ERROR_OPERATION_ABORTED;
        fnvxr::shared::endSequencedSharedWrite(g_commandState->sequence);
    }
    if (recoverOrphanedRunning
        && g_commandState->status == fnvxr::shared::CommandStatusRunning
        && fnvxr::shared::beginSequencedSharedWrite(g_commandState->sequence))
    {
        g_commandState->status = fnvxr::shared::CommandStatusFailed;
        g_commandState->resultCode = ERROR_OPERATION_ABORTED;
        fnvxr::shared::endSequencedSharedWrite(g_commandState->sequence);
    }
}

void initSharedCommand()
{
    if (g_commandState)
        return;

    g_commandWriterMutex = CreateMutexA(
        nullptr, FALSE, fnvxr::shared::CommandWriterMutexName);
    if (!g_commandWriterMutex || !acquireSharedCommandWriter())
    {
        logTelemetry("command shared writer mutex unavailable err=%lu\n", GetLastError());
        if (g_commandWriterMutex)
            CloseHandle(g_commandWriterMutex);
        g_commandWriterMutex = nullptr;
        return;
    }

    g_commandMapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(SharedCommandState),
        fnvxr::shared::CommandSharedMappingName);
    if (!g_commandMapping)
    {
        logTelemetry("command shared CreateFileMapping failed err=%lu\n", GetLastError());
        releaseSharedCommandWriter();
        CloseHandle(g_commandWriterMutex);
        g_commandWriterMutex = nullptr;
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
        releaseSharedCommandWriter();
        CloseHandle(g_commandWriterMutex);
        g_commandWriterMutex = nullptr;
        return;
    }

    const bool existingValid = createError == ERROR_ALREADY_EXISTS
        && g_commandState->magic == CommandSharedMagic
        && g_commandState->version == CommandSharedVersion;
    repairSharedCommandState(true);
    g_lastCommandRequestId = g_commandState->status == fnvxr::shared::CommandStatusPending
        ? 0
        : g_commandState->requestId;
    logTelemetry("command shared ready state=%p mapping=%p existing=%d request=%lu status=%lu bytes=%zu\n",
        g_commandState,
        g_commandMapping,
        static_cast<int>(existingValid),
        static_cast<unsigned long>(g_commandState->requestId),
        static_cast<unsigned long>(g_commandState->status),
        sizeof(SharedCommandState));
    releaseSharedCommandWriter();
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

    g_inputEventWriterMutex = CreateMutexA(
        nullptr,
        FALSE,
        fnvxr::shared::InputEventWriterMutexName);
    if (!g_inputEventWriterMutex)
    {
        logTelemetry("input events writer mutex failed err=%lu\n", GetLastError());
        UnmapViewOfFile(g_inputEvents);
        g_inputEvents = nullptr;
        CloseHandle(g_inputEventMapping);
        g_inputEventMapping = nullptr;
        return;
    }
    const DWORD writerWait = WaitForSingleObject(g_inputEventWriterMutex, 2000);
    if (writerWait != WAIT_OBJECT_0 && writerWait != WAIT_ABANDONED)
    {
        logTelemetry("input events writer mutex wait failed result=%lu\n", writerWait);
        CloseHandle(g_inputEventWriterMutex);
        g_inputEventWriterMutex = nullptr;
        UnmapViewOfFile(g_inputEvents);
        g_inputEvents = nullptr;
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
    // The role mutex transfers an abandoned writer transaction safely.
    InterlockedExchange(&g_inputEvents->writeLock, 0);
    ReleaseMutex(g_inputEventWriterMutex);
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
        || !g_inputEventWriterMutex
        || g_inputEvents->magic != InputEventSharedMagic
        || g_inputEvents->version != InputEventSharedVersion)
        return false;

    const DWORD writerWait = WaitForSingleObject(g_inputEventWriterMutex, 50);
    if (writerWait != WAIT_OBJECT_0 && writerWait != WAIT_ABANDONED)
    {
        InterlockedIncrement(&g_inputEvents->droppedEvents);
        return false;
    }
    InterlockedExchange(&g_inputEvents->writeLock, 1);

    UInt32 sequence = fnvxr::shared::sequencedValueBits(g_inputEvents->writeSequence) + 1u;
    if (sequence == 0u)
        sequence = 1u;
    const UInt32 index = (sequence - 1u) & (InputEventQueueLength - 1u);
    auto& event = g_inputEvents->events[index];
    InterlockedExchange(&event.sequence, 0);
    event.type = type;
    event.code = code;
    event.value0 = value0;
    event.value1 = value1;
    event.flags = flags;
    event.frame = frame;
    MemoryBarrier();
    LONG publishedEventSequence = 0;
    std::memcpy(&publishedEventSequence, &sequence, sizeof(publishedEventSequence));
    InterlockedExchange(&event.sequence, publishedEventSequence);
    MemoryBarrier();
    LONG publishedSequence = 0;
    std::memcpy(&publishedSequence, &sequence, sizeof(publishedSequence));
    InterlockedExchange(&g_inputEvents->writeSequence, publishedSequence);
    InterlockedExchange(&g_inputEvents->writeLock, 0);
    ReleaseMutex(g_inputEventWriterMutex);

    static UInt32 logged = 0;
    if (logged < 64)
    {
        ++logged;
        logTelemetry(
            "inputEvent publish seq=%lu type=%lu code=%lu value=%ld,%ld flags=0x%lx frame=%llu\n",
            static_cast<unsigned long>(sequence),
            static_cast<unsigned long>(type),
            static_cast<unsigned long>(code),
            static_cast<LONG>(value0),
            static_cast<LONG>(value1),
            static_cast<unsigned long>(flags),
            static_cast<unsigned long long>(frame));
    }
    return true;
}

void publishRuntimeState(
    UInt64 frame,
    UInt32 menuBits,
    RuntimePhase phase,
    bool uiInputAllowed,
    bool cameraActive)
{
    if (!g_runtimeState || !gamePluginProducerLeaseHeldByCurrentThread())
        return;

    if (!fnvxr::shared::beginSequencedSharedWrite(g_runtimeState->sequence))
        return;
    g_runtimeState->magic = RuntimeSharedMagic;
    g_runtimeState->version = RuntimeSharedVersion;
    g_runtimeState->frame = frame;
    g_runtimeState->menuBits = menuBits;
    g_runtimeState->phase = static_cast<UInt32>(phase);
    g_runtimeState->uiInputAllowed = uiInputAllowed ? 1u : 0u;
    g_runtimeState->cameraActive = cameraActive ? 1u : 0u;
    g_runtimeState->showroomActive = g_showroomActive ? 1u : 0u;
    g_runtimeState->showroomPhase = static_cast<UInt32>(g_showroomPhase);
    g_runtimeState->showroomSceneIndex = g_showroomSceneIndex;
    g_runtimeState->showroomCellFormId = g_showroomCellFormId;
    fnvxr::shared::endSequencedSharedWrite(g_runtimeState->sequence);
}

void updateSharedVrPose(const fnvxr::PoseFrame& pose)
{
    if (!g_vrPoseState)
        return;
    if (!LegacyNvseInputProducerEnabled
        || !envEnabled("FNVXR_NVSE_WRITES_VR_POSE", false))
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
    g_vrPoseState->recenterRequestId = 0;
    g_vrPoseState->reserved = static_cast<UInt32>(GetTickCount64());
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
    if (!LegacyNvseInputProducerEnabled)
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
    if (!LegacyNvseInputProducerEnabled)
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
    if (!LegacyNvseInputProducerEnabled)
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

bool tapDirectInputKey(UInt32 keycode)
{
    if (keycode >= MaxDirectInputMacros)
        return false;

    bool published = false;
    // The proxy-owned shared queue is the durable VR input lane: unlike the
    // xNVSE in-process hook, it can synthesize DirectInput while the physical
    // desktop keyboard is unacquired.  Keep the hook as an explicit legacy
    // fallback, but do not make headset inventory input depend on foreground
    // window ownership.
    const bool inventoryUsesNvseInputOwner =
        envEnabled("FNVXR_HEADSET_INVENTORY_VISUAL_TRIAL", false)
        && g_directInputHook
        && !envEnabled("FNVXR_INVENTORY_SHARED_INPUT_QUEUE", true);
    if (!inventoryUsesNvseInputOwner
        && keycode >= MouseButtonOffset && keycode < MouseButtonOffset + 8)
    {
        published = publishInputEvent(
            fnvxr::shared::InputEventTypeMouseButtonTap,
            keycode - MouseButtonOffset);
    }
    else if (!inventoryUsesNvseInputOwner)
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

bool openEnginePipBoyInventory(const char* source, UInt64 frame)
{
    __try
    {
        void* manager = *pointerFromAddress32<void**>(InterfaceManagerAddress);
        if (!manager)
            return false;
        const UInt32 mode = readUInt32(
            reinterpret_cast<std::uintptr_t>(manager)
            + InterfaceManagerPipBoyModeOffset);
        if (mode != 0u)
            return isPipboyVisible();
        using OpenPipBoyFn = void (__thiscall*)(void*, void (__cdecl*)(), UInt32);
        pointerFromAddress32<OpenPipBoyFn>(InterfaceManagerOpenPipBoyAddress)(
            manager,
            nullptr,
            InventoryMenuType);
        logTelemetry(
            "enginePipBoy open frame=%llu source=%s menuType=%lu manager=%p modeBefore=%lu\n",
            static_cast<unsigned long long>(frame),
            source ? source : "unknown",
            static_cast<unsigned long>(InventoryMenuType),
            manager,
            static_cast<unsigned long>(mode));
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        logTelemetry("enginePipBoy open exception frame=%llu source=%s\n",
            static_cast<unsigned long long>(frame), source ? source : "unknown");
        return false;
    }
}

bool closeEnginePipBoy(const char* source, UInt64 frame)
{
    __try
    {
        void* manager = *pointerFromAddress32<void**>(InterfaceManagerAddress);
        if (!manager || !isPipboyVisible())
            return false;
        using ClosePipBoyFn = void (__thiscall*)(void*, void (__cdecl*)());
        pointerFromAddress32<ClosePipBoyFn>(InterfaceManagerClosePipBoyAddress)(
            manager,
            nullptr);
        logTelemetry(
            "enginePipBoy close frame=%llu source=%s manager=%p\n",
            static_cast<unsigned long long>(frame),
            source ? source : "unknown",
            manager);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        logTelemetry("enginePipBoy close exception frame=%llu source=%s\n",
            static_cast<unsigned long long>(frame), source ? source : "unknown");
        return false;
    }
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
        if (genericMenuType == kMenuTypeMessage)
            logReadOnlyMessageMenuDiagnostic(menuLifecycleGeneration);
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
UInt32 externalDInputFrame();

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

bool physicalLeftMenuPipBoyEnabled()
{
    return physicalHeadsetPlayRequested()
        && envEnabled("FNVXR_PHYSICAL_LEFT_MENU_PIPBOY_ENABLE", true);
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
    if (!g_cameraState || !gamePluginProducerLeaseHeldByCurrentThread())
        return;

    if (!fnvxr::shared::beginSequencedSharedWrite(g_cameraState->sequence))
        return;
    g_cameraState->magic = CameraSharedMagic;
    g_cameraState->version = CameraSharedVersion;
    g_cameraState->frame = frame;

    if (!cameraAllowedForMenuBits(menuBits))
    {
        g_cameraState->active = 0;
        fnvxr::shared::endSequencedSharedWrite(g_cameraState->sequence);
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
        fnvxr::shared::endSequencedSharedWrite(g_cameraState->sequence);
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
    fnvxr::shared::endSequencedSharedWrite(g_cameraState->sequence);
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

bool discoverRetailRigNodes(void* root);

void updateSharedPlayer(UInt64 frame, RuntimePhase phase)
{
    if (!g_playerState || !gamePluginProducerLeaseHeldByCurrentThread())
        return;

    if (!fnvxr::shared::beginSequencedSharedWrite(g_playerState->sequence))
        return;
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
        ? readPointer(
            reinterpret_cast<std::uintptr_t>(player)
                + PlayerCharacterFirstPersonNodeOffset)
        : nullptr;
    if (readOnlyFirstPersonSemanticsRequested()
        && looksLikeNiObject(playerNode)
        && (g_retailRigNodes.root != playerNode
            || !looksLikeNiObject(g_retailRigNodes.weapon)
            || !looksLikeNiObject(g_retailRigNodes.leftHandMesh)
            || !looksLikeNiObject(g_retailRigNodes.rightHandMesh)
            || !looksLikeNiObject(g_retailRigNodes.pipBoy)))
    {
        // Read-only discovery is the semantic recorder for the intact stock
        // fallback. It traverses the live first-person tree but installs no
        // post-animation hook and performs no transform/culling write.
        static_cast<void>(discoverRetailRigNodes(playerNode));
    }
    void* firstPersonSceneRoot = g_retailRigNodes.weapon;
    if (looksLikeNiObject(firstPersonSceneRoot))
    {
        g_playerState->reserved[
            fnvxr::shared::PlayerSharedFirstPersonWeaponNodeReservedIndex] =
                sharedPointerAddress(firstPersonSceneRoot);
    }
    // Both first-person clavicles share the stock view-model skeleton parent.
    // Publish that narrow subtree separately from PlayerCharacter's full
    // first-person root: it contains both arms, hands, and the child weapon
    // without pulling unrelated/cull-only actor branches into the private
    // eye traversal.
    void* firstPersonArmsRoot = g_retailRigNodes.left.clavicle
        ? readPointer(
            reinterpret_cast<std::uintptr_t>(g_retailRigNodes.left.clavicle)
                + NiAvObjectParentOffset)
        : nullptr;
    void* rightClavicleParent = g_retailRigNodes.right.clavicle
        ? readPointer(
            reinterpret_cast<std::uintptr_t>(g_retailRigNodes.right.clavicle)
                + NiAvObjectParentOffset)
        : nullptr;
    if (firstPersonArmsRoot == rightClavicleParent
        && looksLikeNiObject(firstPersonArmsRoot))
    {
        g_playerState->reserved[
            fnvxr::shared::PlayerSharedFirstPersonArmsNodeReservedIndex] =
                sharedPointerAddress(firstPersonArmsRoot);
    }
    if (looksLikeNiObject(g_retailRigNodes.upperBodyMesh))
    {
        g_playerState->reserved[
            fnvxr::shared::PlayerSharedFirstPersonUpperBodyNodeReservedIndex] =
                sharedPointerAddress(g_retailRigNodes.upperBodyMesh);
    }
    const struct PublishedFirstPersonNode
    {
        void* object;
        std::uint32_t reservedIndex;
    } publishedFirstPersonNodes[] = {
        { g_retailRigNodes.armsGeometry0,
          fnvxr::shared::PlayerSharedFirstPersonArmsGeometry0ReservedIndex },
        { g_retailRigNodes.armsGeometry1,
          fnvxr::shared::PlayerSharedFirstPersonArmsGeometry1ReservedIndex },
        { g_retailRigNodes.leftHandMesh,
          fnvxr::shared::PlayerSharedFirstPersonLeftHandNodeReservedIndex },
        { g_retailRigNodes.rightHandMesh,
          fnvxr::shared::PlayerSharedFirstPersonRightHandNodeReservedIndex },
        { g_retailRigNodes.pipBoy,
          fnvxr::shared::PlayerSharedFirstPersonPipBoyNodeReservedIndex },
    };
    for (const PublishedFirstPersonNode& published : publishedFirstPersonNodes)
    {
        if (looksLikeNiObject(published.object))
            g_playerState->reserved[published.reservedIndex] =
                sharedPointerAddress(published.object);
    }

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
    fnvxr::shared::endSequencedSharedWrite(g_playerState->sequence);

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

bool desktopAssistCameraLeaseCurrent();
bool finiteVec3(Vec3 value);
bool finiteMatrix33(const Matrix33& matrix);
void* retrievePlayerRootNode(bool firstPerson);

void updateSharedDesktopAssist(UInt64 frame)
{
    if (!g_desktopAssistState || !gamePluginProducerLeaseHeldByCurrentThread())
        return;
    if (!fnvxr::shared::beginSequencedSharedWrite(g_desktopAssistState->sequence))
        return;

    g_desktopAssistState->magic = DesktopAssistSharedMagic;
    g_desktopAssistState->version = DesktopAssistSharedVersion;
    g_desktopAssistState->flags = 0u;
    g_desktopAssistState->frame = frame;
    g_desktopAssistState->cameraNodeAddress = 0u;
    g_desktopAssistState->poseSequence = 0u;
    g_desktopAssistState->poseProducerEpoch = 0u;
    std::memset(g_desktopAssistState->playerWorldRot, 0, sizeof(g_desktopAssistState->playerWorldRot));
    std::memset(g_desktopAssistState->playerWorldPos, 0, sizeof(g_desktopAssistState->playerWorldPos));
    std::memset(g_desktopAssistState->cameraLocalRot, 0, sizeof(g_desktopAssistState->cameraLocalRot));
    std::memset(g_desktopAssistState->cameraLocalPos, 0, sizeof(g_desktopAssistState->cameraLocalPos));
    std::memset(g_desktopAssistState->cameraWorldRot, 0, sizeof(g_desktopAssistState->cameraWorldRot));
    std::memset(g_desktopAssistState->cameraWorldPos, 0, sizeof(g_desktopAssistState->cameraWorldPos));
    g_desktopAssistState->bodyRootAddress = 0u;
    g_desktopAssistState->bodyRootReserved = 0u;
    std::memset(g_desktopAssistState->bodyRootWorldRot, 0, sizeof(g_desktopAssistState->bodyRootWorldRot));
    std::memset(g_desktopAssistState->bodyRootWorldPos, 0, sizeof(g_desktopAssistState->bodyRootWorldPos));

    const bool leaseCurrent = g_cameraHookAuthorization
            == CameraHookAuthorization::DesktopAssist
        && desktopAssistCameraLeaseCurrent();
    if (leaseCurrent)
        g_desktopAssistState->flags |= fnvxr::shared::DesktopAssistFlagLeaseCurrent;
    if (g_cameraHookInstalled
        && g_cameraHookAuthorization == CameraHookAuthorization::DesktopAssist)
    {
        g_desktopAssistState->flags |= fnvxr::shared::DesktopAssistFlagCameraHookInstalled;
    }

    void* player = readPointer(PlayerCharacterAddress);
    const bool thirdPerson = playerThirdPersonActive();
    if (!thirdPerson)
        g_desktopAssistState->flags |= fnvxr::shared::DesktopAssistFlagFirstPerson;
    void* playerNode = player
        ? readPointer(reinterpret_cast<std::uintptr_t>(player) + PlayerCharacterFirstPersonNodeOffset)
        : nullptr;
    if (looksLikeNiObject(playerNode))
    {
        const auto playerBase = reinterpret_cast<std::uintptr_t>(playerNode);
        const Matrix33 playerWorldRotation = readMatrix33(
            playerBase + NiAvObjectWorldRotationOffset);
        const Vec3 playerWorldPosition = readVec3(
            playerBase + NiAvObjectWorldTranslationOffset);
        if (finiteMatrix33(playerWorldRotation) && finiteVec3(playerWorldPosition))
        {
            std::memcpy(
                g_desktopAssistState->playerWorldRot,
                playerWorldRotation.m,
                sizeof(g_desktopAssistState->playerWorldRot));
            g_desktopAssistState->playerWorldPos[0] = playerWorldPosition.x;
            g_desktopAssistState->playerWorldPos[1] = playerWorldPosition.y;
            g_desktopAssistState->playerWorldPos[2] = playerWorldPosition.z;
            g_desktopAssistState->flags |= fnvxr::shared::DesktopAssistFlagPlayerTransformValid;
        }
    }

    void* camera = activeGameCameraObject();
    if (looksLikeNiObject(camera))
    {
        const auto base = reinterpret_cast<std::uintptr_t>(camera);
        const Matrix33 localRotation = readMatrix33(base + NiAvObjectLocalRotationOffset);
        const Vec3 localPosition = readVec3(base + NiAvObjectLocalTranslationOffset);
        g_desktopAssistState->cameraNodeAddress = sharedPointerAddress(camera);
        if (finiteMatrix33(localRotation) && finiteVec3(localPosition))
        {
            std::memcpy(
                g_desktopAssistState->cameraLocalRot,
                localRotation.m,
                sizeof(g_desktopAssistState->cameraLocalRot));
            g_desktopAssistState->cameraLocalPos[0] = localPosition.x;
            g_desktopAssistState->cameraLocalPos[1] = localPosition.y;
            g_desktopAssistState->cameraLocalPos[2] = localPosition.z;
            g_desktopAssistState->flags |= fnvxr::shared::DesktopAssistFlagCameraLocalTransformValid;
        }
        const Matrix33 worldRotation = readMatrix33(base + NiAvObjectWorldRotationOffset);
        const Vec3 worldPosition = readVec3(base + NiAvObjectWorldTranslationOffset);
        if (finiteMatrix33(worldRotation) && finiteVec3(worldPosition))
        {
            std::memcpy(
                g_desktopAssistState->cameraWorldRot,
                worldRotation.m,
                sizeof(g_desktopAssistState->cameraWorldRot));
            g_desktopAssistState->cameraWorldPos[0] = worldPosition.x;
            g_desktopAssistState->cameraWorldPos[1] = worldPosition.y;
            g_desktopAssistState->cameraWorldPos[2] = worldPosition.z;
            g_desktopAssistState->flags |= fnvxr::shared::DesktopAssistFlagCameraWorldTransformValid;
        }
        if (leaseCurrent
            && !thirdPerson
            && inCameraGameplay()
            && g_lastAppliedCamera == camera
            && g_lastAppliedCameraPoseSequence != 0
            && g_lastCameraPoseProducerEpoch != 0u)
        {
            g_desktopAssistState->poseSequence = static_cast<UInt32>(
                g_lastAppliedCameraPoseSequence);
            g_desktopAssistState->poseProducerEpoch = g_lastCameraPoseProducerEpoch;
            g_desktopAssistState->flags |= fnvxr::shared::DesktopAssistFlagCameraPoseApplied;
        }
    }

    // The first-person node above is useful diagnostic context, but it is not
    // the body proof. Read the engine's explicit non-first-person root only
    // after the already-authorized camera-local transaction is active. This
    // is a read-only getter plus transform copy; it does not touch any actor,
    // camera, animation, or world state.
    if (leaseCurrent
        && g_cameraHookInstalled
        && !thirdPerson
        && inCameraGameplay())
    {
        void* bodyRoot = retrievePlayerRootNode(false);
        if (bodyRoot && bodyRoot != camera && looksLikeNiObject(bodyRoot))
        {
            const auto bodyBase = reinterpret_cast<std::uintptr_t>(bodyRoot);
            const Matrix33 bodyWorldRotation = readMatrix33(
                bodyBase + NiAvObjectWorldRotationOffset);
            const Vec3 bodyWorldPosition = readVec3(
                bodyBase + NiAvObjectWorldTranslationOffset);
            if (finiteMatrix33(bodyWorldRotation) && finiteVec3(bodyWorldPosition))
            {
                g_desktopAssistState->bodyRootAddress = sharedPointerAddress(bodyRoot);
                std::memcpy(
                    g_desktopAssistState->bodyRootWorldRot,
                    bodyWorldRotation.m,
                    sizeof(g_desktopAssistState->bodyRootWorldRot));
                g_desktopAssistState->bodyRootWorldPos[0] = bodyWorldPosition.x;
                g_desktopAssistState->bodyRootWorldPos[1] = bodyWorldPosition.y;
                g_desktopAssistState->bodyRootWorldPos[2] = bodyWorldPosition.z;
                g_desktopAssistState->flags |= fnvxr::shared::DesktopAssistFlagBodyRootTransformValid;
            }
        }
    }

    // This is an observation record only. It never invokes a transform update,
    // writes a transform, or creates a render/OpenXR transaction.
    fnvxr::shared::endSequencedSharedWrite(g_desktopAssistState->sequence);
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

fnvxr::engine::DesktopAssistCameraRequest desktopAssistCameraRequest()
{
    fnvxr::engine::DesktopAssistCameraRequest request {};
    request.desktopAssistProfile = desktopAssistProfileRequested();
    request.cameraOnlyRequested = request.desktopAssistProfile;
    request.cameraPoseApplicationRequested =
        envEnabled("FNVXR_CAMERA_HOOK", false)
        && envEnabled("FNVXR_CAMERA_APPLY", false);
    request.appliesLocalRotation = envEnabled("FNVXR_CAMERA_APPLY_ROTATION", true);
    request.yawPitchRollEnabled = !envEnabled("FNVXR_CAMERA_YAW_ONLY", false);
    request.writesWorldTransform = envEnabled("FNVXR_CAMERA_WRITE_WORLD", false);
    request.callsUnverifiedTransformUpdate =
        envEnabled("FNVXR_CAMERA_UPDATE_TRANSFORM", false);
    request.appliesLocalTranslation =
        envEnabled("FNVXR_CAMERA_APPLY_TRANSLATION", false);
    return request;
}

bool desktopAssistCameraMutationAllowedAtDecision()
{
    const fnvxr::engine::DesktopAssistCameraRequest request =
        desktopAssistCameraRequest();
    const fnvxr::engine::compatibility::RetailCompatibilityProof proof =
        fnvxr::engine::compatibility::proveCurrentRetailCompatibilityAtDecisionPoint();
    const bool authorized = fnvxr::engine::desktopAssistCameraAuthorized(
        proof,
        request);
    if (!authorized)
    {
        static UInt32 rejected = 0;
        ++rejected;
        if (rejected <= 12u || (rejected % 300u) == 0u)
        {
            logTelemetry(
                "desktopAssist camera authority rejected count=%lu profile=%d requested=%d apply=%d rotation=%d yawPitchRoll=%d world=%d updateTransform=%d translation=%d compatibility=%d failure=%u evidence=%d%d%d%d%d%d%d%d%d%d%d\n",
                static_cast<unsigned long>(rejected),
                static_cast<int>(request.desktopAssistProfile),
                static_cast<int>(request.cameraOnlyRequested),
                static_cast<int>(request.cameraPoseApplicationRequested),
                static_cast<int>(request.appliesLocalRotation),
                static_cast<int>(request.yawPitchRollEnabled),
                static_cast<int>(request.writesWorldTransform),
                static_cast<int>(request.callsUnverifiedTransformUpdate),
                static_cast<int>(request.appliesLocalTranslation),
                static_cast<int>(proof.compatible),
                static_cast<unsigned>(proof.failure),
                static_cast<int>(proof.evidence.retailExecutableIdentityMatched),
                static_cast<int>(proof.evidence.moduleSnapshotStable),
                static_cast<int>(proof.evidence.jip5730ExactOrAbsent),
                static_cast<int>(proof.evidence.johnnyGuitar528ExactOrAbsent),
                static_cast<int>(proof.evidence.showOff184ExactOrAbsent),
                static_cast<int>(proof.evidence.renderFirstPersonStockOrJipNormalized),
                static_cast<int>(proof.evidence.protectedCoreBodiesMatched),
                static_cast<int>(proof.evidence.protectedFunctionInventoryMatched),
                static_cast<int>(proof.evidence.protectedVtableSlotsMatched),
                static_cast<int>(proof.evidence.protectedVtableBlocksMatched),
                static_cast<int>(proof.evidence.synchronousSameProcess));
        }
    }
    return authorized;
}

bool desktopAssistCameraLeaseCurrent()
{
    return g_cameraHookAuthorization == CameraHookAuthorization::DesktopAssist
        && fnvxr::engine::desktopAssistCameraRequestIsNarrow(
            desktopAssistCameraRequest());
}

fnvxr::engine::TrackedPropAssistRequest trackedPropAssistRequest()
{
    fnvxr::engine::TrackedPropAssistRequest request {};
    request.trackedPropAssistProfile = trackedPropAssistProfileRequested();
    request.visualOnlyRequested = request.trackedPropAssistProfile;
    request.cameraPoseApplicationRequested =
        envEnabled("FNVXR_CAMERA_HOOK", false)
        && envEnabled("FNVXR_CAMERA_APPLY", false);
    request.appliesLocalCameraRotation =
        envEnabled("FNVXR_CAMERA_APPLY_ROTATION", true);
    request.yawPitchRollEnabled = !envEnabled("FNVXR_CAMERA_YAW_ONLY", false);
    request.appliesLocalCameraTranslation =
        envEnabled("FNVXR_CAMERA_APPLY_TRANSLATION", false);
    request.writesWorldTransform = envEnabled("FNVXR_CAMERA_WRITE_WORLD", false);
    request.callsUnverifiedTransformUpdate =
        envEnabled("FNVXR_CAMERA_UPDATE_TRANSFORM", false);
    request.rigHookRequested = envEnabled("FNVXR_RETAIL_RIG_ENABLE", false);
    request.rigTransformWritesRequested =
        envEnabled("FNVXR_RETAIL_RIG_APPLY", false);
    request.weaponTransformWritesRequested =
        envEnabled("FNVXR_RETAIL_WEAPON_APPLY", false);
    // The trial rejects a pose that lacks either the grip or the aim source;
    // it must not silently fall back to a head-locked weapon orientation.
    request.rightGripAndAimRequired = true;
    request.projectileNodeHookRequested =
        envEnabled("FNVXR_RETAIL_PROJECTILE_NODE_HOOK", false);
    request.projectileOrHitMutationRequested =
        envEnabled("FNVXR_TRACKED_PROP_ASSIST_PROJECTILE_OR_HIT_MUTATION", false);
    request.inputInjectionRequested =
        envEnabled("FNVXR_NVSE_WRITES_VR_POSE", false)
        || envEnabled("FNVXR_CLICK_SENDINPUT_MOUSE", false)
        || envEnabled("FNVXR_PLUGIN_SENDINPUT_CLICK", false)
        || envEnabled("FNVXR_EXTERNAL_XINPUT_WRITER", false)
        || envEnabled("FNVXR_EXTERNAL_DINPUT_WRITER", false)
        || envEnabled("FNVXR_DESKTOP_ASSIST_AUTOMATION", false);
    request.worldStereoRequested =
        envEnabled("FNVXR_ENABLE_ENGINE_CENTER_STEREO", false)
        || !envEnabled("FNVXR_DISABLE_STEREO_WORLD", true);
    request.legacyReplayRequested =
        envEnabled("FNVXR_D3D9_STEREO_REPLAY", false)
        || envEnabled("FNVXR_D3D9_NATIVE_SINGLE_TRAVERSAL_REPLAY", false)
        || envEnabled("FNVXR_D3D9_WIDE_WORLD_REPLAY", false);
    request.openXrPresentationRequested =
        envEnabled("FNVXR_TRACKED_PROP_ASSIST_OPENXR_PRESENTATION", false);
    request.uiCaptureRequested =
        envEnabled("FNVXR_DESKTOP_ASSIST_UI_CAPTURE", false);
    return request;
}

bool trackedPropAssistMutationAllowedAtDecision()
{
    const fnvxr::engine::TrackedPropAssistRequest request =
        trackedPropAssistRequest();
    const fnvxr::engine::compatibility::RetailCompatibilityProof proof =
        fnvxr::engine::compatibility::proveCurrentRetailCompatibilityAtDecisionPoint();
    const bool authorized = fnvxr::engine::trackedPropAssistAuthorized(
        proof,
        request);
    if (!authorized)
    {
        static UInt32 rejected = 0;
        ++rejected;
        if (rejected <= 12u || (rejected % 300u) == 0u)
        {
            logTelemetry(
                "trackedPropAssist authority rejected count=%lu profile=%d visualOnly=%d camera=%d rotation=%d yawPitchRoll=%d rig=%d rigApply=%d weaponApply=%d projectile=%d input=%d stereo=%d replay=%d openxr=%d ui=%d compatibility=%d failure=%u\n",
                static_cast<unsigned long>(rejected),
                static_cast<int>(request.trackedPropAssistProfile),
                static_cast<int>(request.visualOnlyRequested),
                static_cast<int>(request.cameraPoseApplicationRequested),
                static_cast<int>(request.appliesLocalCameraRotation),
                static_cast<int>(request.yawPitchRollEnabled),
                static_cast<int>(request.rigHookRequested),
                static_cast<int>(request.rigTransformWritesRequested),
                static_cast<int>(request.weaponTransformWritesRequested),
                static_cast<int>(request.projectileNodeHookRequested
                    || request.projectileOrHitMutationRequested),
                static_cast<int>(request.inputInjectionRequested),
                static_cast<int>(request.worldStereoRequested),
                static_cast<int>(request.legacyReplayRequested),
                static_cast<int>(request.openXrPresentationRequested),
                static_cast<int>(request.uiCaptureRequested),
                static_cast<int>(proof.compatible),
                static_cast<unsigned>(proof.failure));
        }
    }
    return authorized;
}

bool trackedPropAssistLeaseCurrent()
{
    return g_cameraHookAuthorization == CameraHookAuthorization::TrackedPropAssist
        && g_retailRigHookInstalled
        && fnvxr::engine::trackedPropAssistRequestIsNarrow(
            trackedPropAssistRequest());
}

fnvxr::engine::HeadlessStereoRigVisualTrialRequest
headlessStereoRigVisualTrialRequest()
{
    fnvxr::engine::HeadlessStereoRigVisualTrialRequest request {};
    request.stereoVisualTrialProfile = headsetControllerRigVisualTrialRequested();
    request.headlessSimulator = envEnabled("OPENXR_SIMULATOR_HEADLESS", false);
    request.ownedHeadsetFixture = headsetDemoFixtureProfileSelected()
        && retailFixtureAutomationRequested();
    request.worldOnlyCapture = headsetWorldOnlyCaptureProfileSelected();
    request.stockWeaponDrawRequested =
        headsetWorldOnlyFixtureWeaponDrawRequested();
    request.finalStockFrameCaptureRequested =
        envEnabled("FNVXR_HEADSET_FINAL_STOCK_FRAME_CAPTURE", false);
    request.cameraHookRequested =
        envEnabled("FNVXR_INSTALL_CAMERA_HOOK", false)
        || envEnabled("FNVXR_CAMERA_HOOK", false)
        || envEnabled("FNVXR_CAMERA_APPLY", false);
    request.rigHookRequested = envEnabled("FNVXR_RETAIL_RIG_ENABLE", false);
    request.rigTransformWritesRequested =
        envEnabled("FNVXR_RETAIL_RIG_APPLY", false);
    request.weaponTransformWritesRequested =
        envEnabled("FNVXR_RETAIL_WEAPON_APPLY", false);
    request.rightGripAndAimRequired = true;
    request.engineCenterStereoRequested =
        envEnabled("FNVXR_ENABLE_ENGINE_CENTER_STEREO", false)
        && !envEnabled("FNVXR_DISABLE_STEREO_WORLD", false);
    request.projectileNodeHookRequested =
        envEnabled("FNVXR_RETAIL_PROJECTILE_NODE_HOOK", false);
    request.projectileOrHitMutationRequested =
        envEnabled("FNVXR_TRACKED_PROP_ASSIST_PROJECTILE_OR_HIT_MUTATION", false);
    request.inputInjectionRequested =
        envEnabled("FNVXR_NVSE_WRITES_VR_POSE", false)
        || envEnabled("FNVXR_CLICK_SENDINPUT_MOUSE", false)
        || envEnabled("FNVXR_PLUGIN_SENDINPUT_CLICK", false)
        || envEnabled("FNVXR_EXTERNAL_XINPUT_WRITER", false)
        || envEnabled("FNVXR_EXTERNAL_DINPUT_WRITER", false)
        || envEnabled("FNVXR_DESKTOP_ASSIST_AUTOMATION", false);
    request.legacyReplayRequested =
        envEnabled("FNVXR_D3D9_STEREO_REPLAY", false)
        || envEnabled("FNVXR_D3D9_NATIVE_SINGLE_TRAVERSAL_REPLAY", false)
        || envEnabled("FNVXR_D3D9_WIDE_WORLD_REPLAY", false);
    request.uiCaptureRequested =
        envEnabled("FNVXR_DESKTOP_ASSIST_UI_CAPTURE", false);
    request.physicalHeadsetRequested =
        envEnabled("FNVXR_PHYSICAL_HEADSET_PLAY", false);
    return request;
}

bool headlessStereoRigVisualTrialMutationAllowedAtDecision()
{
    const fnvxr::engine::HeadlessStereoRigVisualTrialRequest request =
        headlessStereoRigVisualTrialRequest();
    const fnvxr::engine::compatibility::RetailCompatibilityProof proof =
        fnvxr::engine::compatibility::proveCurrentRetailCompatibilityAtDecisionPoint();
    const bool authorized = fnvxr::engine::headlessStereoRigVisualTrialAuthorized(
        proof,
        request);
    if (!authorized)
    {
        static UInt32 rejected = 0;
        ++rejected;
        if (rejected <= 12u || (rejected % 300u) == 0u)
        {
            logTelemetry(
                "headlessStereoRig authority rejected count=%lu profile=%d headless=%d fixture=%d worldOnly=%d weapon=%d stockCapture=%d camera=%d rig=%d rigApply=%d weaponApply=%d gripAim=%d centerStereo=%d projectile=%d input=%d replay=%d ui=%d physical=%d compatibility=%d failure=%u\n",
                static_cast<unsigned long>(rejected),
                static_cast<int>(request.stereoVisualTrialProfile),
                static_cast<int>(request.headlessSimulator),
                static_cast<int>(request.ownedHeadsetFixture),
                static_cast<int>(request.worldOnlyCapture),
                static_cast<int>(request.stockWeaponDrawRequested),
                static_cast<int>(request.finalStockFrameCaptureRequested),
                static_cast<int>(request.cameraHookRequested),
                static_cast<int>(request.rigHookRequested),
                static_cast<int>(request.rigTransformWritesRequested),
                static_cast<int>(request.weaponTransformWritesRequested),
                static_cast<int>(request.rightGripAndAimRequired),
                static_cast<int>(request.engineCenterStereoRequested),
                static_cast<int>(
                    request.projectileNodeHookRequested
                    || request.projectileOrHitMutationRequested),
                static_cast<int>(request.inputInjectionRequested),
                static_cast<int>(request.legacyReplayRequested),
                static_cast<int>(request.uiCaptureRequested),
                static_cast<int>(request.physicalHeadsetRequested),
                static_cast<int>(proof.compatible),
                static_cast<unsigned>(proof.failure));
        }
    }
    return authorized;
}

bool headlessStereoRigVisualTrialLeaseCurrent()
{
    return g_retailRigHookInstalled
        && fnvxr::engine::headlessStereoRigVisualTrialRequestIsNarrow(
            headlessStereoRigVisualTrialRequest());
}

bool retailRigVisualOnlyTrialLeaseCurrent()
{
    return trackedPropAssistProfileRequested()
        ? trackedPropAssistLeaseCurrent()
        : (headsetControllerRigVisualTrialRequested()
            && headlessStereoRigVisualTrialLeaseCurrent());
}

bool cameraHookUsesAssistAuthority()
{
    return g_cameraHookAuthorization == CameraHookAuthorization::DesktopAssist
        || g_cameraHookAuthorization == CameraHookAuthorization::TrackedPropAssist;
}

void restoreVrPoseFromGameCamera()
{
    if (!g_haveCameraBase || !g_cameraBaseObject || g_lastAppliedCamera != g_cameraBaseObject)
        return;

    const auto base = reinterpret_cast<std::uintptr_t>(g_cameraBaseObject);
    writeMatrix33(base + NiAvObjectLocalRotationOffset, g_cameraBaseLocalRotation);
    writeVec3(base + NiAvObjectLocalTranslationOffset, g_cameraBaseLocalTranslation);
    if (!cameraHookUsesAssistAuthority()
        && envEnabled("FNVXR_CAMERA_WRITE_WORLD", false))
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
    if (g_cameraHookAuthorization == CameraHookAuthorization::DesktopAssist)
    {
        if (!desktopAssistCameraLeaseCurrent())
            return;
        if (playerThirdPersonActive())
        {
            static UInt32 thirdPersonRejected = 0;
            ++thirdPersonRejected;
            if (thirdPersonRejected <= 12u || (thirdPersonRejected % 300u) == 0u)
            {
                logTelemetry(
                    "desktopAssist camera apply skipped count=%lu reason=third-person\n",
                    static_cast<unsigned long>(thirdPersonRejected));
            }
            return;
        }
    }
    else if (g_cameraHookAuthorization == CameraHookAuthorization::TrackedPropAssist)
    {
        if (!trackedPropAssistLeaseCurrent())
            return;
        if (playerThirdPersonActive())
        {
            static UInt32 thirdPersonRejected = 0;
            ++thirdPersonRejected;
            if (thirdPersonRejected <= 12u || (thirdPersonRejected % 300u) == 0u)
            {
                logTelemetry(
                    "trackedPropAssist camera apply skipped count=%lu reason=third-person\n",
                    static_cast<unsigned long>(thirdPersonRejected));
            }
            return;
        }
    }
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
    Matrix33 cameraRotationDelta = xrDeltaToNiCameraMatrix(xrRotationDelta);
    const bool yawOnly = envEnabled("FNVXR_CAMERA_YAW_ONLY", false);
    if (yawOnly)
        cameraRotationDelta = yawOnlyNiCameraMatrix(cameraRotationDelta);
    const bool preMultiply = envEnabled("FNVXR_CAMERA_PREMULTIPLY", false);
    const bool applyRotation = envEnabled("FNVXR_CAMERA_APPLY_ROTATION", true);
    const bool applyTranslation = envEnabled("FNVXR_CAMERA_APPLY_TRANSLATION", false);
    if (applyRotation)
    {
        const Matrix33 localRotated = preMultiply
            ? multiplyMatrix33(cameraRotationDelta, g_cameraBaseLocalRotation)
            : multiplyMatrix33(g_cameraBaseLocalRotation, cameraRotationDelta);
        writeMatrix33(base + NiAvObjectLocalRotationOffset, localRotated);
    }

    const float positionScale = getFloatFromEnv("FNVXR_CAMERA_POSITION_SCALE", 0.0f);
    if (applyTranslation && positionScale != 0.0f)
    {
        // NiCamera is forward/up/right, unlike OpenXR right/up/back. Actor
        // right/forward/up is a third, separate basis used only for gameplay.
        const Vec3 cameraDelta = xrDeltaToNiCameraVector(xrPositionDelta);
        Vec3 localTranslate = g_cameraBaseLocalTranslation;
        localTranslate.x += cameraDelta.x * positionScale;
        localTranslate.y += cameraDelta.y * positionScale;
        localTranslate.z += cameraDelta.z * positionScale;
        writeVec3(base + NiAvObjectLocalTranslationOffset, localTranslate);
    }

    if (envEnabled("FNVXR_CAMERA_WRITE_WORLD", false))
    {
        const Matrix33 worldRotated = preMultiply
            ? multiplyMatrix33(cameraRotationDelta, g_cameraBaseWorldRotation)
            : multiplyMatrix33(g_cameraBaseWorldRotation, cameraRotationDelta);
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
    if (stereoVisualTrialProfileSelected())
    {
        logTelemetry(
            "cameraHook hard-blocked: stereo visual trial reserves world-render authority for the D3D bridge\n");
        return false;
    }
    if (g_cameraHookInstalled)
        return true;
    const bool requested = envEnabled("FNVXR_INSTALL_CAMERA_HOOK", true);
    if (!requested)
    {
        logTelemetry("cameraHook install disabled\n");
        return true;
    }
    const bool desktopAssist = desktopAssistProfileRequested();
    const bool trackedPropAssist = trackedPropAssistProfileRequested();
    if (desktopAssist)
    {
        if (!desktopAssistCameraMutationAllowedAtDecision())
        {
            logTelemetry(
                "cameraHook hard-blocked: desktop assist compatibility/configuration proof incomplete\n");
            return true;
        }
    }
    else if (trackedPropAssist)
    {
        if (!trackedPropAssistMutationAllowedAtDecision())
        {
            logTelemetry(
                "cameraHook hard-blocked: tracked-prop assist compatibility/configuration proof incomplete\n");
            return true;
        }
    }
    else if (!retailMutationAllowedForCurrentProcess(requested))
    {
        logTelemetry("cameraHook hard-blocked: retail mutation source/evidence proof incomplete\n");
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
    g_cameraHookAuthorization = desktopAssist
        ? CameraHookAuthorization::DesktopAssist
        : trackedPropAssist
            ? CameraHookAuthorization::TrackedPropAssist
            : CameraHookAuthorization::FullRetail;
    logTelemetry(
        "cameraHook installed target=%p hook=%p trampoline=%p authority=%s\n",
        pointerFromAddress32<void*>(PlayerCharacterUpdateCameraAddress),
        reinterpret_cast<void*>(hookedUpdateCamera),
        g_updateCameraTrampoline,
        desktopAssist
            ? "desktop-assist"
            : trackedPropAssist
                ? "tracked-prop-assist"
                : "full-retail");
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

bool niObjectNameStartsWith(void* object, const char* prefix)
{
    if (!prefix)
        return false;
    char name[128] {};
    if (!copyNiObjectName(object, name, sizeof(name)))
        return false;
    const size_t length = std::strlen(prefix);
    return _strnicmp(name, prefix, length) == 0;
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

void collectPrefixedNiObjectsRecursive(
    void* object,
    const char* prefix,
    int depth,
    UInt32& visits,
    void*& onlyMatch,
    UInt32& matchCount)
{
    if (!object || !prefix || depth > 64 || ++visits > 4096 || matchCount > 1)
        return;
    if (niObjectNameStartsWith(object, prefix))
    {
        onlyMatch = object;
        ++matchCount;
        if (matchCount > 1)
            return;
    }
    void** children = nullptr;
    UInt16 count = 0;
    if (!readNiNodeChildren(object, &children, count))
        return;
    for (UInt16 index = 0; index < count && matchCount <= 1; ++index)
    {
        void* child = nullptr;
        __try { child = children[index]; }
        __except (EXCEPTION_EXECUTE_HANDLER) { child = nullptr; }
        collectPrefixedNiObjectsRecursive(
            child, prefix, depth + 1, visits, onlyMatch, matchCount);
    }
}

void* findUniqueNiObjectByPrefix(
    void* root,
    const char* prefix,
    UInt32* matchCountOut = nullptr)
{
    UInt32 visits = 0;
    UInt32 matches = 0;
    void* match = nullptr;
    collectPrefixedNiObjectsRecursive(
        root, prefix, 0, visits, match, matches);
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

void makeRetailVrSurfaceVisible(void* node, int depth, UInt32& visits)
{
    if (!node || depth > 64 || ++visits > 4096 || niObjectKind(node) == 0)
        return;
    const auto base = reinterpret_cast<std::uintptr_t>(node);
    __try
    {
        auto* flags = reinterpret_cast<UInt32*>(base + NiAvObjectFlagsOffset);
        *flags &= ~1u;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return;
    }
    void** children = nullptr;
    UInt16 count = 0;
    if (!readNiNodeChildren(node, &children, count))
        return;
    for (UInt16 index = 0; index < count; ++index)
    {
        void* child = nullptr;
        __try { child = children[index]; }
        __except (EXCEPTION_EXECUTE_HANDLER) { child = nullptr; }
        makeRetailVrSurfaceVisible(child, depth + 1, visits);
    }
}

void makeRetailVrSurfaceVisible(void* node)
{
    UInt32 visits = 0;
    makeRetailVrSurfaceVisible(node, 0, visits);
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
            pose.recenterRequestId = g_vrPoseState->recenterRequestId;
            pose.hmdRot = {
                g_vrPoseState->hmdRot[0], g_vrPoseState->hmdRot[1],
                g_vrPoseState->hmdRot[2], g_vrPoseState->hmdRot[3] };
            pose.hmdPos = {
                g_vrPoseState->hmdPos[0], g_vrPoseState->hmdPos[1], g_vrPoseState->hmdPos[2] };
            pose.leftRot = {
                g_vrPoseState->leftRot[0], g_vrPoseState->leftRot[1],
                g_vrPoseState->leftRot[2], g_vrPoseState->leftRot[3] };
            pose.leftPos = {
                g_vrPoseState->leftPos[0], g_vrPoseState->leftPos[1], g_vrPoseState->leftPos[2] };
            pose.rightRot = {
                g_vrPoseState->rightRot[0], g_vrPoseState->rightRot[1],
                g_vrPoseState->rightRot[2], g_vrPoseState->rightRot[3] };
            pose.rightPos = {
                g_vrPoseState->rightPos[0], g_vrPoseState->rightPos[1], g_vrPoseState->rightPos[2] };
            pose.leftAimRot = {
                g_vrPoseState->leftAimRot[0], g_vrPoseState->leftAimRot[1],
                g_vrPoseState->leftAimRot[2], g_vrPoseState->leftAimRot[3] };
            pose.leftAimPos = {
                g_vrPoseState->leftAimPos[0], g_vrPoseState->leftAimPos[1], g_vrPoseState->leftAimPos[2] };
            pose.rightAimRot = {
                g_vrPoseState->rightAimRot[0], g_vrPoseState->rightAimRot[1],
                g_vrPoseState->rightAimRot[2], g_vrPoseState->rightAimRot[3] };
            pose.rightAimPos = {
                g_vrPoseState->rightAimPos[0], g_vrPoseState->rightAimPos[1], g_vrPoseState->rightAimPos[2] };
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            continue;
        }
        MemoryBarrier();
        const UInt32 poseAgeMs = static_cast<UInt32>(GetTickCount64())
            - g_vrPoseState->reserved;
        const UInt32 maximumPoseAgeMs = static_cast<UInt32>((std::min)(
            25,
            (std::max)(1, getIntFromEnv("FNVXR_RETAIL_RIG_MAX_SHARED_POSE_AGE_MS", 25))));
        if (sequenceBefore == g_vrPoseState->sequence
            && (g_vrPoseState->sequence & 1) == 0
            && pose.referenceSpaceGeneration != 0
            && pose.producerEpoch != 0
            && pose.predictedDisplayTime > 0
            && poseAgeMs <= maximumPoseAgeMs
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
            // Validation must precede normalization: normalizeQuat deliberately
            // maps zero/NaN input to identity for internal math, which is not
            // an acceptable policy for an external shared-memory payload.
            pose.hmdRot = normalizeQuat(pose.hmdRot);
            pose.leftRot = normalizeQuat(pose.leftRot);
            pose.rightRot = normalizeQuat(pose.rightRot);
            pose.leftAimRot = normalizeQuat(pose.leftAimRot);
            pose.rightAimRot = normalizeQuat(pose.rightAimRot);
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
        Matrix33 bodyRootWorldRotation {};
        for (int row = 0; row < 3; ++row)
        {
            for (int column = 0; column < 3; ++column)
            {
                renderCameraWorldRotation.m[row][column] =
                    origin.renderCameraWorldRot[row * 3 + column];
            }
        }
        for (int row = 0; row < 3; ++row)
        {
            for (int column = 0; column < 3; ++column)
            {
                bodyRootWorldRotation.m[row][column] =
                    origin.bodyRootWorldRot[row * 3 + column];
            }
        }
        const Vec3 renderCameraWorldPosition {
            origin.renderCameraWorldPos[0],
            origin.renderCameraWorldPos[1],
            origin.renderCameraWorldPos[2]
        };
        const Vec3 bodyRootWorldPosition {
            origin.bodyRootWorldPos[0],
            origin.bodyRootWorldPos[1],
            origin.bodyRootWorldPos[2]
        };
        const UInt32 originAgeMs = static_cast<UInt32>(GetTickCount64()) - origin.reserved;
        const UInt32 maximumOriginAgeMs = static_cast<UInt32>((std::min)(
            25,
            (std::max)(1, getIntFromEnv("FNVXR_RETAIL_RIG_MAX_COMMITTED_ORIGIN_AGE_MS", 25))));
        if (sequenceBefore == g_vrOriginState->sequence
            && (g_vrOriginState->sequence & 1) == 0
            && origin.magic == fnvxr::shared::VrOriginSharedMagic
            && origin.version == fnvxr::shared::VrOriginSharedVersion
            && origin.active == fnvxr::shared::VrOriginStateCommitted
            && origin.generation != 0
            && origin.poseSequence != 0
            && origin.producerEpoch != 0
            && origin.renderPoseSequence != 0
            && origin.renderPoseFrame != 0
            && origin.renderedDisplayTime > 0
            && originAgeMs <= maximumOriginAgeMs
            && origin.reserved2 == GetCurrentProcessId()
            && origin.renderCameraWorldValid != 0
            && origin.renderCameraAddress >= 0x01000000u
            && looksLikeNiObject(reinterpret_cast<void*>(origin.renderCameraAddress))
            && finiteMatrix33(renderCameraWorldRotation)
            && finiteVec3(renderCameraWorldPosition)
            && origin.bodyRootWorldValid != 0
            && origin.bodyRootAddress >= 0x01000000u
            && looksLikeNiObject(reinterpret_cast<void*>(origin.bodyRootAddress))
            && finiteMatrix33(bodyRootWorldRotation)
            && finiteVec3(bodyRootWorldPosition)
            && std::isfinite(origin.bodyRootWorldScale)
            && std::fabs(origin.bodyRootWorldScale) >= 0.0001f
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
    const auto orderedTime = [](std::int64_t value) {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits ^ (std::uint64_t { 1 } << 63);
    };
    const std::uint64_t poseTimeOrdered = orderedTime(pose.predictedDisplayTime);
    const std::uint64_t renderTimeOrdered = orderedTime(originBefore.renderedDisplayTime);
    const std::uint64_t absoluteDisplayTimeDelta = poseTimeOrdered >= renderTimeOrdered
        ? poseTimeOrdered - renderTimeOrdered
        : renderTimeOrdered - poseTimeOrdered;
    const double configuredPoseSkewMilliseconds = static_cast<double>(getFloatFromEnv(
        "FNVXR_RETAIL_RIG_MAX_RENDER_POSE_SKEW_MS",
        250.0f));
    if (!std::isfinite(configuredPoseSkewMilliseconds))
        return false;
    const double maximumPoseSkewMilliseconds = std::clamp(
        configuredPoseSkewMilliseconds,
        1.0,
        60000.0);
    const std::uint64_t maximumPoseSkewNanoseconds = static_cast<std::uint64_t>(
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
                "retailRig pose handoff count=%ld latestSeq=%ld renderSeq=%lu latestFrame=%llu renderFrame=%llu skewMs=%.3f generation=%lu epoch=%llu\n",
                count,
                pose.sequence,
                static_cast<unsigned long>(originBefore.renderPoseSequence),
                static_cast<unsigned long long>(pose.frame),
                static_cast<unsigned long long>(originBefore.renderPoseFrame),
                pose.predictedDisplayTime >= originBefore.renderedDisplayTime
                    ? static_cast<double>(absoluteDisplayTimeDelta) / 1000000.0
                    : -static_cast<double>(absoluteDisplayTimeDelta) / 1000000.0,
                static_cast<unsigned long>(pose.referenceSpaceGeneration),
                static_cast<unsigned long long>(pose.producerEpoch));
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
    void* bodyRoot)
{
    Matrix33 bodyWorldRotation {};
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 3; ++column)
            bodyWorldRotation.m[row][column] = authoritativeOrigin.bodyRootWorldRot[row * 3 + column];
    }
    const Vec3 bodyWorldPosition {
        authoritativeOrigin.bodyRootWorldPos[0],
        authoritativeOrigin.bodyRootWorldPos[1],
        authoritativeOrigin.bodyRootWorldPos[2]
    };
    const Vec3 stableCameraWorld {
        authoritativeOrigin.renderCameraWorldPos[0],
        authoritativeOrigin.renderCameraWorldPos[1],
        authoritativeOrigin.renderCameraWorldPos[2]
    };
    const float bodyWorldScale = authoritativeOrigin.bodyRootWorldScale;
    if (!bodyRoot
        || reinterpret_cast<std::uintptr_t>(bodyRoot) != authoritativeOrigin.bodyRootAddress
        || !finiteMatrix33(bodyWorldRotation)
        || !finiteVec3(bodyWorldPosition)
        || !finiteVec3(stableCameraWorld)
        || !std::isfinite(bodyWorldScale)
        || std::fabs(bodyWorldScale) < 0.0001f)
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
    g_retailRigOriginSource = RetailRigOriginSource::NativeStereo;
    g_retailRigBodyAnchorLocal = transformVec3(
        transposeMatrix33(bodyWorldRotation),
        subtractVec3(stableCameraWorld, bodyWorldPosition));
    g_retailRigBodyAnchorLocal = scaleVec3(g_retailRigBodyAnchorLocal, 1.0f / bodyWorldScale);
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

bool captureTrackedPropAssistRigOrigin(
    const VrRigPoseSnapshot& pose,
    void* bodyRoot,
    void* firstPersonRigRoot,
    bool headlessStereoRigVisualTrial)
{
    if (!bodyRoot || !firstPersonRigRoot
        || !looksLikeNiObject(bodyRoot)
        || !looksLikeNiObject(firstPersonRigRoot)
        || !finiteUsableQuat(pose.hmdRot)
        || !finiteVec3(pose.hmdPos)
        || pose.referenceSpaceGeneration == 0u
        || pose.producerEpoch == 0u)
    {
        return false;
    }

    const auto bodyBase = reinterpret_cast<std::uintptr_t>(bodyRoot);
    const auto rigBase = reinterpret_cast<std::uintptr_t>(firstPersonRigRoot);
    const Matrix33 bodyWorldRotation = readMatrix33(
        bodyBase + NiAvObjectWorldRotationOffset);
    const Vec3 bodyWorldPosition = readVec3(
        bodyBase + NiAvObjectWorldTranslationOffset);
    const float bodyWorldScale = readFloat(
        bodyBase + NiAvObjectWorldScaleOffset,
        0.0f);
    void* firstPersonCamera = readPointer(Camera1stNodeAddress);
    const bool firstPersonCameraUsable = firstPersonCamera
        && looksLikeNiObject(firstPersonCamera);
    const Vec3 controllerAnchorWorld = firstPersonCameraUsable
        ? readVec3(
            reinterpret_cast<std::uintptr_t>(firstPersonCamera)
                + NiAvObjectWorldTranslationOffset)
        : readVec3(rigBase + NiAvObjectWorldTranslationOffset);
    if (!finiteMatrix33(bodyWorldRotation)
        || !finiteVec3(bodyWorldPosition)
        || !finiteVec3(controllerAnchorWorld)
        || !std::isfinite(bodyWorldScale)
        || std::fabs(bodyWorldScale) < 0.0001f)
    {
        return false;
    }

    // Latch Fallout's engine-authored first-person camera height in the body
    // frame once. Controller poses are recentered around the HMD, so anchoring
    // them at the rig root placed an absolute tracked hand at the player's
    // feet. The one-time Camera1st anchor keeps seated and standing hands in
    // view without allowing later HMD motion to drag the weapon.
    g_retailRigOriginHmdRot = normalizeQuat(pose.hmdRot);
    g_retailRigOriginHmdPos = pose.hmdPos;
    g_retailRigReferenceSpaceGeneration = pose.referenceSpaceGeneration;
    g_retailRigProducerEpoch = pose.producerEpoch;
    g_retailRigOriginPoseSequence = static_cast<UInt32>(pose.sequence);
    g_retailRigOriginAuthoritySequence = pose.sequence;
    g_retailRigOriginBodyRoot = bodyRoot;
    g_retailRigOriginSource = headlessStereoRigVisualTrial
        ? RetailRigOriginSource::HeadlessStereoRigVisualTrial
        : RetailRigOriginSource::TrackedPropAssist;
    g_retailRigBodyAnchorLocal = transformVec3(
        transposeMatrix33(bodyWorldRotation),
        subtractVec3(controllerAnchorWorld, bodyWorldPosition));
    g_retailRigBodyAnchorLocal = scaleVec3(
        g_retailRigBodyAnchorLocal,
        1.0f / bodyWorldScale);
    if (!finiteVec3(g_retailRigBodyAnchorLocal))
        return false;

    g_haveRetailRigOrigin = true;
    logTelemetry(
        "retailRigVisualTrial origin latched seq=%ld frame=%llu referenceGeneration=%lu hmdPos=(%.4f %.4f %.4f) bodyRoot=%p firstPersonRigRoot=%p bodyAnchorLocal=(%.3f %.3f %.3f) anchorSource=%s originSource=%s projectile=0 input=0 renderer=0 headlessStereo=%d\n",
        pose.sequence,
        static_cast<unsigned long long>(pose.frame),
        static_cast<unsigned long>(pose.referenceSpaceGeneration),
        g_retailRigOriginHmdPos.x,
        g_retailRigOriginHmdPos.y,
        g_retailRigOriginHmdPos.z,
        bodyRoot,
        firstPersonRigRoot,
        g_retailRigBodyAnchorLocal.x,
        g_retailRigBodyAnchorLocal.y,
        g_retailRigBodyAnchorLocal.z,
        firstPersonCameraUsable
            ? "first-person-camera-at-latch"
            : "first-person-rig-root-fallback",
        headlessStereoRigVisualTrial
            ? "headless-stereo-rig-body"
            : "tracked-prop-assist-body",
        static_cast<int>(headlessStereoRigVisualTrial));
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
    discovered.weaponModel = discovered.weapon
        ? findUniqueNiObjectByPrefix(discovered.weapon, "Weapon ")
        : nullptr;
    discovered.upperBodyMesh = findUniqueNiObjectByPrefix(root, "UpperBody ");
    discovered.armsGeometry0 = findUniqueNiNode(root, "Arms:0");
    discovered.armsGeometry1 = findUniqueNiNode(root, "Arms:1");
    discovered.leftHandMesh = findUniqueNiObjectByPrefix(root, "LeftHand ");
    discovered.rightHandMesh = findUniqueNiObjectByPrefix(root, "RightHand ");
    discovered.pipBoy = findUniqueNiObjectByPrefix(root, "PipBoy ");
    discovered.pipBoyScreen = discovered.pipBoy
        ? findUniqueNiNode(discovered.pipBoy, "ScreenLit")
        : nullptr;
    discovered.pipBoyScreenSurface = discovered.pipBoyScreen
        ? findUniqueNiObjectByPrefix(
            discovered.pipBoyScreen,
            "pipboyscreen:0")
        : nullptr;
    discovered.projectileNode = findUniqueNiNode(root, "ProjectileNode", &projectileMatches);
    discovered.muzzleFlash = findUniqueNiNode(root, "MuzzleFlash", &muzzleMatches);
    g_retailRigNodes = discovered;
    g_retailLeftCalibration = {};
    g_retailRightCalibration = {};
    g_retailWeaponCalibration = {};
    g_retailPipBoyCalibration = {};
    g_livePipBoyScaleNode = nullptr;
    g_livePipBoyBaseScale = 1.0f;
    g_livePipBoyAppliedScale = 1.0f;
    g_latestTrackedLeftHandValid = false;
    g_latestTrackedPipBoyScreenValid = false;
    g_latestCompleteTrackedPropsPoseSequence = 0;
    g_latestCompleteTrackedPropsPoseFrame = 0;
    g_latestCompleteTrackedPropsApplied = false;
    g_retailRigContinuityPose = {};
    g_retailEquippedWeaponForm = nullptr;
    g_retailEquippedWeaponFormId = 0;
    g_retailWeaponModelFormId = 0;
    g_retailWeaponRefreshRequested = true;
    ++g_retailRigDiscoveryCount;
    const bool complete = retailRigNodesComplete(discovered);
    g_retailRigRediscoveryRequested = !complete;

    logTelemetry(
        "retailRig discovery count=%llu root=%p left=(clav=%p upper=%p fore=%p hand=%p) right=(clav=%p upper=%p fore=%p hand=%p) weapon=%p weaponModel=%p weaponMatches=%lu upperBodyMesh=%p leftHandMesh=%p rightHandMesh=%p pipBoy=%p pipBoyScreen=%p pipBoyScreenSurface=%p projectile=%p projectileMatches=%lu muzzleFlash=%p muzzleMatches=%lu complete=%d ancestry=validated\n",
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
        discovered.weaponModel,
        static_cast<unsigned long>(weaponMatches),
        discovered.upperBodyMesh,
        discovered.leftHandMesh,
        discovered.rightHandMesh,
        discovered.pipBoy,
        discovered.pipBoyScreen,
        discovered.pipBoyScreenSurface,
        discovered.projectileNode,
        static_cast<unsigned long>(projectileMatches),
        discovered.muzzleFlash,
        static_cast<unsigned long>(muzzleMatches),
        complete ? 1 : 0);

    if (envEnabled("FNVXR_RETAIL_RIG_DUMP_NODES", true)
        && (g_retailRigDiscoveryCount == 1 || !complete))
    {
        UInt32 visits = 0;
        dumpNiNodesRecursive(root, 0, visits);
        logTelemetry("retailRig node dump complete visits=%lu\n", static_cast<unsigned long>(visits));
    }
    return complete;
}

void* currentEquippedWeaponForm()
{
    __try
    {
        void* player = readPointer(PlayerCharacterAddress);
        void* process = player
            ? readPointer(
                reinterpret_cast<std::uintptr_t>(player)
                    + MobileObjectBaseProcessOffset)
            : nullptr;
        if (!process)
            return nullptr;
        void** processVtable = *reinterpret_cast<void***>(process);
        if (!processVtable || !processVtable[BaseProcessGetWeaponInfoVtableSlot])
            return nullptr;
        using GetWeaponInfoFn = void* (__thiscall*)(void*);
        void* weaponInfo = reinterpret_cast<GetWeaponInfoFn>(
            processVtable[BaseProcessGetWeaponInfoVtableSlot])(process);
        // xNVSE Actor::GetEquippedWeapon reads EntryData::type at +0x08.
        return weaponInfo
            ? readPointer(reinterpret_cast<std::uintptr_t>(weaponInfo) + 0x08)
            : nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

UInt32 retailWeaponModelFormId(void* model)
{
    char name[128] {};
    if (!copyNiObjectName(model, name, sizeof(name)))
        return 0u;
    const char* open = std::strrchr(name, '(');
    if (!open || !open[1])
        return 0u;
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(open + 1, &end, 16);
    return end && end != open + 1 && *end == ')'
        ? static_cast<UInt32>(parsed)
        : 0u;
}

bool currentRetailWeaponBindingReady()
{
    const bool endpointInCurrentModel = g_retailRigNodes.projectileNode
        && g_retailRigNodes.weaponModel
        && niObjectDescendsFrom(
            g_retailRigNodes.projectileNode,
            g_retailRigNodes.weaponModel);
    return fnvxr::weapon_frame::weaponBindingReady(
        g_retailRigNodes.weapon != nullptr,
        g_retailRigNodes.weaponModel != nullptr,
        endpointInCurrentModel,
        g_retailEquippedWeaponFormId,
        g_retailWeaponModelFormId);
}

bool refreshRetailWeaponNodes()
{
    if (!g_retailRigNodes.root)
        return false;

    void* const previousWeapon = g_retailRigNodes.weapon;
    void* const previousModel = g_retailRigNodes.weaponModel;
    void* const previousEndpoint = g_retailRigNodes.projectileNode;
    const UInt32 previousEquippedFormId = g_retailEquippedWeaponFormId;
    const bool explicitRefresh = g_retailWeaponRefreshRequested;

    void* const weapon = findUniqueNiNode(g_retailRigNodes.root, "Weapon");
    void* const model = weapon
        ? findUniqueNiObjectByPrefix(weapon, "Weapon ")
        : nullptr;
    void* const equippedForm = currentEquippedWeaponForm();
    const UInt32 equippedFormId = equippedForm
        ? readUInt32(
            reinterpret_cast<std::uintptr_t>(equippedForm)
                + TESFormRefIdOffset)
        : 0u;
    const UInt32 modelFormId = retailWeaponModelFormId(model);

    void* player = readPointer(PlayerCharacterAddress);
    void* process = player
        ? readPointer(
            reinterpret_cast<std::uintptr_t>(player)
                + MobileObjectBaseProcessOffset)
        : nullptr;
    void* endpoint = process
        ? readPointer(
            reinterpret_cast<std::uintptr_t>(process)
                + MiddleHighProcessProjectileNodeOffset)
        : nullptr;
    if (!endpoint || !model || !niObjectDescendsFrom(endpoint, model))
        endpoint = model ? findUniqueNiNode(model, "ProjectileNode") : nullptr;
    void* const muzzleFlash = model
        ? findUniqueNiNode(model, "MuzzleFlash")
        : nullptr;

    const bool bindingChanged =
        fnvxr::weapon_frame::weaponBindingMustBeRecalibrated(
            explicitRefresh,
            previousEquippedFormId,
            equippedFormId,
            reinterpret_cast<std::uintptr_t>(previousWeapon),
            reinterpret_cast<std::uintptr_t>(weapon),
            reinterpret_cast<std::uintptr_t>(previousModel),
            reinterpret_cast<std::uintptr_t>(model),
            reinterpret_cast<std::uintptr_t>(previousEndpoint),
            reinterpret_cast<std::uintptr_t>(endpoint));

    g_retailRigNodes.weapon = weapon;
    g_retailRigNodes.weaponModel = model;
    g_retailRigNodes.projectileNode = endpoint;
    g_retailRigNodes.muzzleFlash = muzzleFlash;
    g_retailEquippedWeaponForm = equippedForm;
    g_retailEquippedWeaponFormId = equippedFormId;
    g_retailWeaponModelFormId = modelFormId;
    if (equippedFormId != 0u)
        g_lastKnownWeaponFormId = equippedFormId;

    if (bindingChanged)
    {
        // The replacement view-model can alter the stock right-hand terminal
        // even when the stable Weapon wrapper survives. Rebind the hand and
        // weapon from the same current controller sample.
        g_retailRightCalibration = {};
        g_retailWeaponCalibration = {};
        g_retailRigContinuityPose = {};
        // A model swap can complete without a new OpenXR sample. Permit that
        // exact pose to bind and render the replacement weapon immediately.
        g_lastRetailRigPoseSequence = 0;
    }

    const bool ready = currentRetailWeaponBindingReady();
    g_retailWeaponRefreshRequested = !ready;
    if (bindingChanged || !ready)
    {
        logTelemetry(
            "retailWeapon binding refresh explicit=%d ready=%d previousForm=0x%08lx currentForm=0x%08lx modelForm=0x%08lx previousWeapon=%p weapon=%p previousModel=%p model=%p previousEndpoint=%p endpoint=%p calibrationReset=%d\n",
            explicitRefresh ? 1 : 0,
            ready ? 1 : 0,
            static_cast<unsigned long>(previousEquippedFormId),
            static_cast<unsigned long>(equippedFormId),
            static_cast<unsigned long>(modelFormId),
            previousWeapon,
            weapon,
            previousModel,
            model,
            previousEndpoint,
            endpoint,
            bindingChanged ? 1 : 0);
    }
    return ready;
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

bool livePipBoyInteractionAuthorized()
{
    return physicalHeadsetPlayRequested()
        || (stereoVisualTrialProfileSelected()
            && envEnabled("OPENXR_SIMULATOR_HEADLESS", false)
            && envEnabled("FNVXR_HEADSET_DEMO_FIXTURE", false));
}

void updateLivePipBoyInteraction(
    const fnvxr::PoseFrame& pose,
    UInt32 menuBits,
    bool rightTriggerPressed)
{
    if (!livePipBoyInteractionAuthorized())
        return;

    const std::uint8_t flags = pose.poseReserved[
        fnvxr::PoseReservedInteractionFlags];
    const bool focused =
        (flags & fnvxr::PoseInteractionLivePipBoyFocused) != 0u;
    const bool openRequested =
        (flags & fnvxr::PoseInteractionLivePipBoyOpenRequest) != 0u;
    void* const pipBoy = g_retailRigNodes.pipBoy;
    if (!hostSpatialPropReplacementRequested()
        && pipBoy && niObjectKind(pipBoy) != 0)
    {
        const auto base = reinterpret_cast<std::uintptr_t>(pipBoy);
        if (g_livePipBoyScaleNode != pipBoy)
        {
            g_livePipBoyScaleNode = pipBoy;
            g_livePipBoyBaseScale = readFloat(
                base + NiAvObjectLocalScaleOffset,
                1.0f);
            if (!std::isfinite(g_livePipBoyBaseScale)
                || std::fabs(g_livePipBoyBaseScale) < 0.0001f)
            {
                g_livePipBoyBaseScale = 1.0f;
            }
            g_livePipBoyAppliedScale = g_livePipBoyBaseScale;
        }
        const float focusAmount = static_cast<float>(pose.poseReserved[
            fnvxr::PoseReservedPipBoyScale]) / 510.0f;
        const float target = g_livePipBoyBaseScale
            * (1.0f + std::clamp(focusAmount, 0.0f, 0.5f));
        g_livePipBoyAppliedScale +=
            (target - g_livePipBoyAppliedScale) * 0.25f;
        if (writeFloat(
                base + NiAvObjectLocalScaleOffset,
                g_livePipBoyAppliedScale))
        {
            forwardKinematics(pipBoy);
        }
    }

    if (openRequested && !pipBoyVisibleFromMenuBits(menuBits))
    {
        static UInt64 lastOpenFrame = 0u;
        if (lastOpenFrame == 0u || pose.frame >= lastOpenFrame + 30u)
        {
            lastOpenFrame = pose.frame;
            static_cast<void>(openEnginePipBoyInventory(
                "live-pipboy-focus",
                pose.frame));
        }
    }

    const bool liveDevicePointerActive =
        (flags & fnvxr::PoseInteractionLivePipBoyPointerActive) != 0u;
    if (focused && pipBoyVisibleFromMenuBits(menuBits)
        && liveDevicePointerActive && rightTriggerPressed)
    {
        const float u = static_cast<float>(pose.poseReserved[
            fnvxr::PoseReservedPipBoyDeviceU]) / 255.0f;
        const float v = static_cast<float>(pose.poseReserved[
            fnvxr::PoseReservedPipBoyDeviceV]) / 255.0f;
        using Control = fnvxr::engine::live_pipboy::PhysicalControl;
        const Control control =
            fnvxr::engine::live_pipboy::physicalControl(u, v);
        bool handled = false;
        switch (control)
        {
            case Control::StatsDial:
                handled = tapDirectInputKey(DIK_F1);
                break;
            case Control::ItemsDial:
                handled = tapDirectInputKey(DIK_F2);
                break;
            case Control::DataDial:
                handled = tapDirectInputKey(DIK_F3);
                break;
            case Control::ScrollUp:
                handled = publishMouseWheelInput(
                    120,
                    pose.frame,
                    "live-pipboy-scroll-up");
                break;
            case Control::ScrollDown:
                handled = publishMouseWheelInput(
                    -120,
                    pose.frame,
                    "live-pipboy-scroll-down");
                break;
            case Control::Screen:
                // The ordinary pointer/click path owns the live screen.
                break;
        }
        if (control != Control::Screen)
        {
            logTelemetry(
                "livePipBoy physical control frame=%llu control=%u uv=(%.4f,%.4f) handled=%d\n",
                static_cast<unsigned long long>(pose.frame),
                static_cast<unsigned>(control),
                u,
                v,
                handled ? 1 : 0);
        }
    }

    const bool orbitActive =
        (flags & fnvxr::PoseInteractionWeaponOrbitActive) != 0u;
    const std::uint8_t encodedSlot = pose.poseReserved[
        fnvxr::PoseReservedWeaponOrbitSlot];
    if (orbitActive && encodedSlot >= 1u && encodedSlot <= 8u)
        g_weaponOrbitSelectedSlot = static_cast<int>(encodedSlot - 1u);
    if (!orbitActive && g_weaponOrbitWasActive
        && g_weaponOrbitSelectedSlot >= 0
        && runtimePhaseFromMenuBits(menuBits) == RuntimePhase::Gameplay)
    {
        constexpr UInt32 FavoriteKeys[8] {
            DIK_1, DIK_2, DIK_3, DIK_4,
            DIK_5, DIK_6, DIK_7, DIK_8,
        };
        const int selected = g_weaponOrbitSelectedSlot;
        const bool selectedOk = tapDirectInputKey(FavoriteKeys[selected]);
        logTelemetry(
            "weaponOrbit select frame=%llu slot=%d key=0x%02lx handled=%d\n",
            static_cast<unsigned long long>(pose.frame),
            selected + 1,
            static_cast<unsigned long>(FavoriteKeys[selected]),
            selectedOk ? 1 : 0);
        g_weaponOrbitSelectedSlot = -1;
    }
    g_weaponOrbitWasActive = orbitActive;
}

void updateLivePipBoyInteraction(
    const SharedXInputState& state,
    UInt32 menuBits,
    bool rightTriggerPressed)
{
    fnvxr::PoseFrame pose {};
    pose.frame = externalDInputSharedReady()
        ? externalDInputFrame()
        : state.packet;
    pose.poseReserved[fnvxr::PoseReservedInteractionFlags] =
        state.reserved[fnvxr::shared::XInputReservedInteractionFlags];
    pose.poseReserved[fnvxr::PoseReservedPipBoyScale] =
        state.reserved[fnvxr::shared::XInputReservedPipBoyScale];
    pose.poseReserved[fnvxr::PoseReservedWeaponOrbitSlot] =
        state.reserved[fnvxr::shared::XInputReservedWeaponOrbitSlot];
    pose.poseReserved[fnvxr::PoseReservedPipBoyDeviceU] =
        state.reserved[fnvxr::shared::XInputReservedPipBoyDeviceU];
    pose.poseReserved[fnvxr::PoseReservedPipBoyDeviceV] =
        state.reserved[fnvxr::shared::XInputReservedPipBoyDeviceV];
    pose.menuPointerActive =
        (pose.poseReserved[fnvxr::PoseReservedInteractionFlags]
            & fnvxr::PoseInteractionLivePipBoyHovered) != 0u
        ? 1u
        : 0u;
    updateLivePipBoyInteraction(
        pose,
        menuBits,
        rightTriggerPressed);
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
    const float unitsPerMeter = getFloatFromEnv(
        "FNVXR_D3D9_GAME_UNITS_PER_METER",
        70.0f);
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

Vec3 hostSpatialHandWristWorld(
    const RetailControllerWorldPose& controller,
    bool left)
{
    const char* xName = left
        ? "FNVXR_RETAIL_LEFT_HAND_OFFSET_X"
        : "FNVXR_RETAIL_RIGHT_HAND_OFFSET_X";
    const char* yName = left
        ? "FNVXR_RETAIL_LEFT_HAND_OFFSET_Y"
        : "FNVXR_RETAIL_RIGHT_HAND_OFFSET_Y";
    const char* zName = left
        ? "FNVXR_RETAIL_LEFT_HAND_OFFSET_Z"
        : "FNVXR_RETAIL_RIGHT_HAND_OFFSET_Z";
    const Vec3 offsetMeters {
        getFloatFromEnv(xName, left ? 0.0f : -0.040f),
        getFloatFromEnv(yName, left ? 0.0f : 0.026f),
        getFloatFromEnv(zName, 0.0f),
    };
    Vec3 offsetGame = xrDeltaToGamebryoVector(offsetMeters);
    offsetGame.x *= getFloatFromEnv("FNVXR_D3D9_POSE_X_SIGN", 1.0f);
    offsetGame.y *= getFloatFromEnv("FNVXR_D3D9_POSE_Y_SIGN", 1.0f);
    offsetGame.z *= getFloatFromEnv("FNVXR_D3D9_POSE_Z_SIGN", 1.0f);
    offsetGame = scaleVec3(
        offsetGame,
        getFloatFromEnv("FNVXR_D3D9_GAME_UNITS_PER_METER", 70.0f)
            * getFloatFromEnv("FNVXR_RETAIL_RIG_POSITION_SCALE", 1.0f));
    return addVec3(
        controller.wristPosition,
        transformVec3(controller.wristRotation, offsetGame));
}

Matrix33 nifHandToXrMeshMatrix()
{
    // Exact basis used by convert_nif_hand_mesh.py:
    // (x, y, z) NIF hand-local -> (y, -z, -x) OpenXR grip-local.
    return {
        { { 0.0f, 1.0f, 0.0f },
          { 0.0f, 0.0f, -1.0f },
          { -1.0f, 0.0f, 0.0f } }
    };
}

bool measureHostSpatialPipBoyCalibration()
{
    if (g_retailPipBoyCalibration.hostSpatialValid)
        return true;
    void* const hand = g_retailRigNodes.left.hand;
    void* const screen = g_retailRigNodes.pipBoyScreenSurface
        ? g_retailRigNodes.pipBoyScreenSurface
        : g_retailRigNodes.pipBoyScreen;
    if (!hand || !screen || niObjectKind(hand) == 0
        || niObjectKind(screen) == 0)
    {
        return false;
    }

    const auto handBase = reinterpret_cast<std::uintptr_t>(hand);
    const auto screenBase = reinterpret_cast<std::uintptr_t>(screen);
    const Matrix33 handWorldRotation = readMatrix33(
        handBase + NiAvObjectWorldRotationOffset);
    const Vec3 handWorldPosition = readVec3(
        handBase + NiAvObjectWorldTranslationOffset);
    const float handWorldScale = readFloat(
        handBase + NiAvObjectWorldScaleOffset,
        0.0f);
    const Matrix33 screenWorldRotation = readMatrix33(
        screenBase + NiAvObjectWorldRotationOffset);
    const Vec3 screenWorldPosition = readVec3(
        screenBase + NiAvObjectWorldTranslationOffset);
    if (!finiteMatrix33(handWorldRotation)
        || !finiteVec3(handWorldPosition)
        || !std::isfinite(handWorldScale)
        || std::fabs(handWorldScale) < 0.0001f
        || !finiteMatrix33(screenWorldRotation)
        || !finiteVec3(screenWorldPosition))
    {
        return false;
    }

    const Matrix33 handInverse = transposeMatrix33(handWorldRotation);
    const Vec3 screenHandLocalGameUnits = scaleVec3(
        transformVec3(
            handInverse,
            subtractVec3(screenWorldPosition, handWorldPosition)),
        1.0f / handWorldScale);
    const Matrix33 screenHandLocalRotation = multiplyMatrix33(
        handInverse,
        screenWorldRotation);
    const Matrix33 handToHost = nifHandToXrMeshMatrix();
    const float unitsPerMeter = getFloatFromEnv(
        "FNVXR_D3D9_GAME_UNITS_PER_METER",
        70.0f);
    if (!std::isfinite(unitsPerMeter) || unitsPerMeter < 1.0f)
        return false;

    g_retailPipBoyCalibration.screenGripLocalPositionMeters = scaleVec3(
        transformVec3(handToHost, screenHandLocalGameUnits),
        1.0f / unitsPerMeter);
    g_retailPipBoyCalibration.screenGripLocalRotation = quatFromMatrix(
        multiplyMatrix33(handToHost, screenHandLocalRotation));
    const float offsetMeters = lengthVec3(
        g_retailPipBoyCalibration.screenGripLocalPositionMeters);
    g_retailPipBoyCalibration.hostSpatialValid = finiteVec3(
            g_retailPipBoyCalibration.screenGripLocalPositionMeters)
        && std::isfinite(offsetMeters)
        && offsetMeters >= 0.02f
        && offsetMeters <= 0.40f
        && finiteUsableQuat(
            g_retailPipBoyCalibration.screenGripLocalRotation);
    logTelemetry(
        "retailPipBoy host calibration valid=%d hand=%p screen=%p source=stock-left-hand-to-screen gripLocalPositionMeters=(%.6f %.6f %.6f) gripLocalQuat=(%.7f %.7f %.7f %.7f) offsetMeters=%.6f\n",
        g_retailPipBoyCalibration.hostSpatialValid ? 1 : 0,
        hand,
        screen,
        g_retailPipBoyCalibration.screenGripLocalPositionMeters.x,
        g_retailPipBoyCalibration.screenGripLocalPositionMeters.y,
        g_retailPipBoyCalibration.screenGripLocalPositionMeters.z,
        g_retailPipBoyCalibration.screenGripLocalRotation.x,
        g_retailPipBoyCalibration.screenGripLocalRotation.y,
        g_retailPipBoyCalibration.screenGripLocalRotation.z,
        g_retailPipBoyCalibration.screenGripLocalRotation.w,
        offsetMeters);
    return g_retailPipBoyCalibration.hostSpatialValid;
}

Quat retailRightHandMeshGripLocalRotation(
    const RetailControllerWorldPose& controller,
    const Matrix33& stockHandWorldRotation,
    const Matrix33& stockWeaponWorldRotation,
    const Matrix33& desiredWeaponWorldRotation)
{
    // Preserve Fallout's authored hand-to-weapon orientation, then express
    // it in the OpenXR grip-local basis used by the host mesh.  P maps an
    // OpenXR local vector to Gamebryo (x,-z,y); Q is the converter's exact
    // NIF-hand -> OpenXR-mesh basis (y,-z,-x).
    const Matrix33 desiredHandWorldRotation = multiplyMatrix33(
        desiredWeaponWorldRotation,
        multiplyMatrix33(
            transposeMatrix33(stockWeaponWorldRotation),
            stockHandWorldRotation));
    const Matrix33 gripToNifHand = multiplyMatrix33(
        transposeMatrix33(controller.wristRotation),
        desiredHandWorldRotation);
    const Matrix33 xrToGame {
        { { 1.0f, 0.0f, 0.0f },
          { 0.0f, 0.0f, -1.0f },
          { 0.0f, 1.0f, 0.0f } }
    };
    const Matrix33 nifHandToXrMesh = nifHandToXrMeshMatrix();
    const Matrix33 gripToHostMesh = multiplyMatrix33(
        transposeMatrix33(xrToGame),
        multiplyMatrix33(
            gripToNifHand,
            transposeMatrix33(nifHandToXrMesh)));
    return quatFromMatrix(gripToHostMesh);
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
    const Matrix33& bodyWorldRotation,
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
    const Vec3 upperArmWorldPosition = readVec3(
        reinterpret_cast<std::uintptr_t>(arm.upperArm)
            + NiAvObjectWorldTranslationOffset);
    const Vec3 forearmWorldPosition = readVec3(
        reinterpret_cast<std::uintptr_t>(arm.forearm)
            + NiAvObjectWorldTranslationOffset);
    calibration.controllerToHandRotation = multiplyMatrix33(
        transposeMatrix33(controller.wristRotation),
        handWorldRotation);
    calibration.usesAimOrientation = false;
    calibration.controllerToWristLocal = configuredControllerToWristOffset(left);
    calibration.upperArmLength = lengthVec3(subtractVec3(
        forearmWorldPosition,
        upperArmWorldPosition));
    calibration.forearmLength = lengthVec3(subtractVec3(
        handWorldPosition,
        forearmWorldPosition));
    const bool bodyAnchoredControllerRig =
        (headsetControllerRigVisualTrialRequested()
            && headlessStereoRigVisualTrialLeaseCurrent())
        || physicalHeadsetEngineCenterRigRequested();
    if (bodyAnchoredControllerRig)
    {
        // The controller position is already an absolute, meter-scaled pose
        // in the stable body frame.  Only an explicit ergonomic offset may be
        // added here.  Calibrating from the animated hand would preserve a
        // seated/standing-dependent gap and leave the hand orbiting the stock
        // first-person pose instead of attaching it to the tracked wrist.
        calibration.controllerToWristBodyLocal = configuredControllerToWristOffset(left);
        calibration.usesStageLocalBodyPositionAnchor =
            finiteVec3(calibration.controllerToWristBodyLocal);
    }
    else if (envEnabled("FNVXR_RETAIL_RIG_AUTO_CALIBRATE_POSITION", false))
    {
        const Vec3 measured = transformVec3(
            transposeMatrix33(controller.wristRotation),
            subtractVec3(handWorldPosition, controller.wristPosition));
        const float maxOffset = getFloatFromEnv("FNVXR_RETAIL_RIG_MAX_AUTO_CALIBRATION_UNITS", 12.0f);
        if (lengthVec3(measured) <= maxOffset)
            calibration.controllerToWristLocal = measured;
    }
    calibration.valid = finiteMatrix33(calibration.controllerToHandRotation)
        && std::isfinite(calibration.upperArmLength)
        && std::isfinite(calibration.forearmLength)
        && calibration.upperArmLength >= 0.01f
        && calibration.forearmLength >= 0.01f
        && (calibration.usesStageLocalBodyPositionAnchor
            ? finiteVec3(calibration.controllerToWristBodyLocal)
            : finiteVec3(calibration.controllerToWristLocal));
    logTelemetry(
        "retailRig calibration side=%s valid=%d orientationSource=%s wristLocal=(%.3f %.3f %.3f) wristBodyLocal=(%.3f %.3f %.3f) anatomicalLengths=(%.4f %.4f) positionAnchor=%s autoPosition=%d\n",
        left ? "left" : "right",
        calibration.valid ? 1 : 0,
        calibration.usesAimOrientation ? "aim" : "grip",
        calibration.controllerToWristLocal.x,
        calibration.controllerToWristLocal.y,
        calibration.controllerToWristLocal.z,
        calibration.controllerToWristBodyLocal.x,
        calibration.controllerToWristBodyLocal.y,
        calibration.controllerToWristBodyLocal.z,
        calibration.upperArmLength,
        calibration.forearmLength,
        calibration.usesStageLocalBodyPositionAnchor
            ? "absolute-controller-body"
            : "controller-local",
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
    ensureRetailHandCalibration(
        calibration,
        arm,
        controller,
        bodyWorldRotation,
        left);
    if (!calibration.valid)
        return false;

    const Vec3 target = calibration.usesStageLocalBodyPositionAnchor
        ? addVec3(
            controller.wristPosition,
            transformVec3(
                bodyWorldRotation,
                calibration.controllerToWristBodyLocal))
        : addVec3(
            controller.wristPosition,
            transformVec3(
                controller.wristRotation,
                calibration.controllerToWristLocal));
    const bool controllerOwnedHand =
        physicalHeadsetEngineCenterRigRequested()
            || (headsetControllerRigVisualTrialRequested()
                && headlessStereoRigVisualTrialLeaseCurrent());

    const Vec3 shoulder = readVec3(
        reinterpret_cast<std::uintptr_t>(arm.upperArm) + NiAvObjectWorldTranslationOffset);
    const Vec3 elbow = readVec3(
        reinterpret_cast<std::uintptr_t>(arm.forearm) + NiAvObjectWorldTranslationOffset);
    const Vec3 wrist = readVec3(
        reinterpret_cast<std::uintptr_t>(arm.hand) + NiAvObjectWorldTranslationOffset);
    const float lengths[2] {
        calibration.upperArmLength,
        calibration.forearmLength
    };
    const float maxSegment = getFloatFromEnv("FNVXR_RETAIL_RIG_MAX_SEGMENT_UNITS", 80.0f);
    if (lengths[0] < 0.01f || lengths[1] < 0.01f
        || lengths[0] > maxSegment || lengths[1] > maxSegment)
    {
        return false;
    }

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
    const bool targetOutsideArmReach =
        shoulderTargetDistance > maximumReach + reachTolerance
        || shoulderTargetDistance < minimumReach - reachTolerance;
    if (targetOutsideArmReach && !controllerOwnedHand)
    {
        return false;
    }

    // A room-scale hand can naturally move beyond the stock first-person
    // skeleton's short arm. Solve the bones toward the reachable point, then
    // place the terminal hand at the tracked wrist below. Otherwise the old
    // reach rejection leaves the rendered weapon attached to a static hand.
    Vec3 solveTarget = target;
    if (targetOutsideArmReach && shoulderTargetDistance > 0.0001f)
    {
        const float reachableDistance = std::clamp(
            shoulderTargetDistance,
            minimumReach + reachTolerance,
            maximumReach - reachTolerance);
        solveTarget = addVec3(
            shoulder,
            scaleVec3(
                normalizeVec3(subtractVec3(target, shoulder)),
                reachableDistance));
    }

    // Solve in shoulder-relative coordinates. Fallout world coordinates can
    // be tens of thousands of units from zero, while the convergence budget
    // is a fraction of one unit; translating the chain avoids needless float
    // precision loss without changing any solved direction.
    const Vec3 elbowFromShoulder = subtractVec3(elbow, shoulder);
    const Vec3 wristFromShoulder = subtractVec3(wrist, shoulder);
    const Vec3 solveTargetFromShoulder = subtractVec3(solveTarget, shoulder);
    const Vec3 poleFromShoulder = subtractVec3(pole, shoulder);
    fnvxr::ik::Vec3 joints[3] {
        { 0.0f, 0.0f, 0.0f },
        { elbowFromShoulder.x, elbowFromShoulder.y, elbowFromShoulder.z },
        { wristFromShoulder.x, wristFromShoulder.y, wristFromShoulder.z }
    };
    fnvxr::ik::SolveOptions options {};
    options.maxIterations = static_cast<int>(getFloatFromEnv("FNVXR_RETAIL_RIG_FABRIK_ITERATIONS", 48.0f));
    options.tolerance = getFloatFromEnv("FNVXR_RETAIL_RIG_FABRIK_TOLERANCE", 0.05f);
    options.poleWeight = getFloatFromEnv("FNVXR_RETAIL_RIG_ELBOW_POLE_WEIGHT", 1.0f);
    const auto result = fnvxr::ik::solveFabrik(
        joints,
        3,
        lengths,
        { solveTargetFromShoulder.x, solveTargetFromShoulder.y, solveTargetFromShoulder.z },
        { poleFromShoulder.x, poleFromShoulder.y, poleFromShoulder.z },
        options);
    const float maximumFinalError = getFloatFromEnv(
        "FNVXR_RETAIL_RIG_MAX_FINAL_ERROR_UNITS",
        0.25f);
    const bool solverResultUsable =
        result.iterations > 0
        && fnvxr::ik::finite(joints[0])
        && fnvxr::ik::finite(joints[1])
        && fnvxr::ik::finite(joints[2])
        && std::isfinite(result.error)
        && result.error <= maximumFinalError;
    if (!solverResultUsable)
    {
        static LONG solverFailureCount = 0;
        const LONG failure = InterlockedIncrement(&solverFailureCount);
        if (failure <= 12 || failure % 120 == 0)
        {
            logTelemetry(
                "retailRig FABRIK rejected count=%ld side=%s solved=%d reachable=%d iterations=%d error=%.4f tolerance=%.4f shoulderTarget=%.4f maximumReach=%.4f targetClamped=%d\n",
                failure,
                left ? "left" : "right",
                result.solved ? 1 : 0,
                result.reachable ? 1 : 0,
                result.iterations,
                result.error,
                options.tolerance,
                shoulderTargetDistance,
                maximumReach,
                targetOutsideArmReach ? 1 : 0);
        }
        return false;
    }

    finalError = solverResultUsable ? result.error : shoulderTargetDistance;
    if (!applyWrites)
        return solverResultUsable;

    const Vec3 solvedShoulder { joints[0].x, joints[0].y, joints[0].z };
    const Vec3 solvedElbow { joints[1].x, joints[1].y, joints[1].z };
    const Vec3 solvedWrist { joints[2].x, joints[2].y, joints[2].z };
    const bool upperAligned = solverResultUsable
        && alignBoneToDirection(
            arm.upperArm,
            arm.forearm,
            subtractVec3(solvedElbow, solvedShoulder));
    const bool forearmAligned = upperAligned
        && alignBoneToDirection(
            arm.forearm,
            arm.hand,
            subtractVec3(solvedWrist, solvedElbow));
    if (!upperAligned || !forearmAligned)
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
    if (controllerOwnedHand && handParent)
    {
        const auto parentBase = reinterpret_cast<std::uintptr_t>(handParent);
        const Vec3 parentWorldPosition = readVec3(
            parentBase + NiAvObjectWorldTranslationOffset);
        const float parentWorldScale = readFloat(
            parentBase + NiAvObjectWorldScaleOffset,
            1.0f);
        Vec3 desiredHandLocalPosition = transformVec3(
            transposeMatrix33(parentWorldRotation),
            subtractVec3(target, parentWorldPosition));
        if (!std::isfinite(parentWorldScale)
            || std::fabs(parentWorldScale) < 0.0001f)
        {
            return false;
        }
        desiredHandLocalPosition = scaleVec3(
            desiredHandLocalPosition,
            1.0f / parentWorldScale);
        if (!finiteVec3(desiredHandLocalPosition)
            || !writeVec3(
                reinterpret_cast<std::uintptr_t>(arm.hand)
                    + NiAvObjectLocalTranslationOffset,
                desiredHandLocalPosition))
        {
            return false;
        }
    }
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

bool applyRetailTrackedPipBoy(
    const RetailControllerWorldPose& controller,
    bool applyWrites)
{
    void* const pipBoy = g_retailRigNodes.pipBoy;
    void* const screen = g_retailRigNodes.pipBoyScreen;
    if (!pipBoy || !screen || niObjectKind(pipBoy) != 2
        || niObjectKind(screen) == 0)
    {
        return false;
    }
    const auto rootBase = reinterpret_cast<std::uintptr_t>(pipBoy);
    const auto screenBase = reinterpret_cast<std::uintptr_t>(screen);
    const Matrix33 rootWorldRotation = readMatrix33(
        rootBase + NiAvObjectWorldRotationOffset);
    const Vec3 rootWorldPosition = readVec3(
        rootBase + NiAvObjectWorldTranslationOffset);
    const Vec3 screenWorldPosition = readVec3(
        screenBase + NiAvObjectWorldTranslationOffset);
    const float rootWorldScale = readFloat(
        rootBase + NiAvObjectWorldScaleOffset,
        0.0f);
    if (!finiteMatrix33(rootWorldRotation)
        || !finiteVec3(rootWorldPosition)
        || !finiteVec3(screenWorldPosition)
        || !finiteMatrix33(controller.wristRotation)
        || !finiteVec3(controller.wristPosition)
        || !std::isfinite(rootWorldScale)
        || std::fabs(rootWorldScale) < 0.0001f)
    {
        return false;
    }
    if (!g_retailPipBoyCalibration.valid)
    {
        g_retailPipBoyCalibration.controllerToRootRotation =
            multiplyMatrix33(
                transposeMatrix33(controller.wristRotation),
                rootWorldRotation);
        g_retailPipBoyCalibration.rootToScreenLocal = scaleVec3(
            transformVec3(
                transposeMatrix33(rootWorldRotation),
                subtractVec3(screenWorldPosition, rootWorldPosition)),
            1.0f / rootWorldScale);
        g_retailPipBoyCalibration.valid = finiteMatrix33(
                g_retailPipBoyCalibration.controllerToRootRotation)
            && finiteVec3(g_retailPipBoyCalibration.rootToScreenLocal);
        logTelemetry(
            "retailPipBoy calibration valid=%d root=%p screen=%p rootToScreenLocal=(%.4f %.4f %.4f) anchor=tracked-left-wrist\n",
            g_retailPipBoyCalibration.valid ? 1 : 0,
            pipBoy,
            screen,
            g_retailPipBoyCalibration.rootToScreenLocal.x,
            g_retailPipBoyCalibration.rootToScreenLocal.y,
            g_retailPipBoyCalibration.rootToScreenLocal.z);
    }
    if (!g_retailPipBoyCalibration.valid)
        return false;

    const float unitsPerMeter = getFloatFromEnv(
        "FNVXR_D3D9_GAME_UNITS_PER_METER",
        70.0f);
    const float positionScale = getFloatFromEnv(
        "FNVXR_RETAIL_RIG_POSITION_SCALE",
        1.0f);
    const Vec3 deviceScreenOffsetMeters {
        getFloatFromEnv("FNVXR_PIPBOY_WRIST_UI_OFFSET_X", 0.0f),
        getFloatFromEnv("FNVXR_PIPBOY_WRIST_UI_OFFSET_Y", 0.075f),
        getFloatFromEnv("FNVXR_PIPBOY_WRIST_UI_OFFSET_Z", -0.035f),
    };
    const Vec3 deviceScreenOffsetGame = scaleVec3(
        xrDeltaToGamebryoVector(deviceScreenOffsetMeters),
        unitsPerMeter * positionScale);
    const Matrix33 desiredRootWorldRotation = multiplyMatrix33(
        controller.wristRotation,
        g_retailPipBoyCalibration.controllerToRootRotation);
    const Vec3 desiredScreenWorldPosition = addVec3(
        controller.wristPosition,
        transformVec3(
            controller.wristRotation,
            deviceScreenOffsetGame));
    const Vec3 desiredRootWorldPosition = subtractVec3(
        desiredScreenWorldPosition,
        scaleVec3(
            transformVec3(
                desiredRootWorldRotation,
                g_retailPipBoyCalibration.rootToScreenLocal),
            rootWorldScale));
    void* const parent = readPointer(rootBase + NiAvObjectParentOffset);
    if (!parent || !finiteMatrix33(desiredRootWorldRotation)
        || !finiteVec3(desiredRootWorldPosition))
    {
        return false;
    }
    const auto parentBase = reinterpret_cast<std::uintptr_t>(parent);
    const Matrix33 parentWorldRotation = readMatrix33(
        parentBase + NiAvObjectWorldRotationOffset);
    const Vec3 parentWorldPosition = readVec3(
        parentBase + NiAvObjectWorldTranslationOffset);
    const float parentWorldScale = readFloat(
        parentBase + NiAvObjectWorldScaleOffset,
        0.0f);
    if (!finiteMatrix33(parentWorldRotation)
        || !finiteVec3(parentWorldPosition)
        || !std::isfinite(parentWorldScale)
        || std::fabs(parentWorldScale) < 0.0001f)
    {
        return false;
    }
    const Matrix33 desiredLocalRotation = multiplyMatrix33(
        transposeMatrix33(parentWorldRotation),
        desiredRootWorldRotation);
    Vec3 desiredLocalPosition = transformVec3(
        transposeMatrix33(parentWorldRotation),
        subtractVec3(desiredRootWorldPosition, parentWorldPosition));
    desiredLocalPosition = scaleVec3(
        desiredLocalPosition,
        1.0f / parentWorldScale);
    if (!finiteMatrix33(desiredLocalRotation)
        || !finiteVec3(desiredLocalPosition))
    {
        return false;
    }
    if (applyWrites
        && (!writeMatrix33(
                rootBase + NiAvObjectLocalRotationOffset,
                desiredLocalRotation)
            || !writeVec3(
                rootBase + NiAvObjectLocalTranslationOffset,
                desiredLocalPosition)))
    {
        return false;
    }
    if (applyWrites)
    {
        makeRetailVrSurfaceVisible(pipBoy);
        forwardKinematics(pipBoy);
    }
    const Vec3 appliedScreenWorld = readVec3(
        screenBase + NiAvObjectWorldTranslationOffset);
    const bool verified = finiteVec3(appliedScreenWorld)
        && lengthVec3(subtractVec3(
               appliedScreenWorld,
               desiredScreenWorldPosition))
            <= getFloatFromEnv(
                "FNVXR_RETAIL_PIPBOY_MAX_WRITE_RESIDUAL_UNITS",
                0.5f);
    if (verified)
    {
        g_latestTrackedPipBoyScreenWorld = appliedScreenWorld;
        g_latestTrackedPipBoyScreenValid = true;
    }
    return verified;
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
    bool wristSocketMeasured {};
    float positionResidualUnits {};
    float angularResidualRadians {};
    float endpointAimResidualRadians {};
    float wristSocketResidualUnits {};
    Vec3 desiredWorldPosition {};
    Vec3 actualWorldPosition {};
    Vec3 endpointWorldPosition {};
    Vec3 wristSocketWorldPosition {};
    Vec3 wristSocketTargetWorldPosition {};
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
        const bool bodyAnchoredControllerRig =
            (headsetControllerRigVisualTrialRequested()
                && headlessStereoRigVisualTrialLeaseCurrent())
            || physicalHeadsetEngineCenterRigRequested();
        const bool hostSpatialProps = hostSpatialPropReplacementRequested();
        void* endpoint = g_retailRigNodes.projectileNode
            ? g_retailRigNodes.projectileNode
            : g_retailRigNodes.muzzleFlash;
        const bool endpointUsable = endpoint
            && g_retailRigNodes.weaponModel
            && niObjectDescendsFrom(
                endpoint,
                g_retailRigNodes.weaponModel)
            && niObjectKind(endpoint) != 0;
        // During an equip, Fallout replaces the model below the stable Weapon
        // attachment asynchronously. Never calibrate a replacement gun from
        // the departing model's muzzle axis.
        if (bodyAnchoredControllerRig && !endpointUsable)
            return result;
        const Matrix33 endpointWorldRotation = endpointUsable
            ? readMatrix33(
                reinterpret_cast<std::uintptr_t>(endpoint)
                    + NiAvObjectWorldRotationOffset)
            : Matrix33 {};
        // In the headless stage-local trial the controller aim transform is
        // the muzzle aim contract. The stock Weapon node's +Y is not the
        // projectile node's +Y (the rejected run measured a 0.51-rad offset),
        // so calibrate the root through the real endpoint relationship rather
        // than preserving that stock-axis mismatch.
        const bool endpointAxisCalibration = bodyAnchoredControllerRig
            && endpointUsable
            && finiteMatrix33(endpointWorldRotation);
        g_retailWeaponCalibration.controllerToWeaponRotation =
            endpointAxisCalibration
            ? transposeMatrix33(multiplyMatrix33(
                transposeMatrix33(weaponWorldRotation),
                endpointWorldRotation))
            : multiplyMatrix33(
                transposeMatrix33(controller.rotation),
                weaponWorldRotation);
        g_retailWeaponCalibration.controllerToWeaponPosition = transformVec3(
            transposeMatrix33(controller.rotation),
            subtractVec3(weaponWorldPosition, controller.position));
        if (bodyAnchoredControllerRig && hostSpatialProps
            && g_retailRigNodes.right.hand)
        {
            const Vec3 stockWristWorld = readVec3(
                reinterpret_cast<std::uintptr_t>(
                    g_retailRigNodes.right.hand)
                    + NiAvObjectWorldTranslationOffset);
            const Matrix33 stockHandWorldRotation = readMatrix33(
                reinterpret_cast<std::uintptr_t>(
                    g_retailRigNodes.right.hand)
                    + NiAvObjectWorldRotationOffset);
            if (finiteVec3(stockWristWorld)
                && finiteMatrix33(stockHandWorldRotation))
            {
                g_retailWeaponCalibration.weaponToWristLocal = transformVec3(
                    transposeMatrix33(weaponWorldRotation),
                    subtractVec3(stockWristWorld, weaponWorldPosition));
                g_retailWeaponCalibration
                    .usesTrackedWristSocketPositionAnchor =
                        finiteVec3(
                            g_retailWeaponCalibration.weaponToWristLocal)
                        && lengthVec3(
                            g_retailWeaponCalibration.weaponToWristLocal)
                            <= getFloatFromEnv(
                                "FNVXR_RETAIL_WEAPON_MAX_SOCKET_UNITS",
                                48.0f);
                const Matrix33 desiredWeaponWorldRotation = multiplyMatrix33(
                    controller.rotation,
                    g_retailWeaponCalibration.controllerToWeaponRotation);
                g_retailWeaponCalibration.rightHandGripLocalRotation =
                    retailRightHandMeshGripLocalRotation(
                        controller,
                        stockHandWorldRotation,
                        weaponWorldRotation,
                        desiredWeaponWorldRotation);
                g_retailWeaponCalibration.handMeshRotationValid =
                    finiteUsableQuat(
                        g_retailWeaponCalibration
                            .rightHandGripLocalRotation);
            }
        }
        else if (bodyAnchoredControllerRig)
        {
            // The weapon is a child of the tracked hand.  Its translation is
            // therefore inherited from the newly attached hand, while this
            // lane owns only the aim rotation.  Preserving the initial
            // weapon/controller gap was the same delta-only calibration bug
            // as the wrist path (149 units in the first physical recording).
            g_retailWeaponCalibration.controllerToWeaponBodyLocal = {};
            g_retailWeaponCalibration.usesStageLocalBodyPositionAnchor = true;
        }
        g_retailWeaponCalibration.valid = finiteMatrix33(
                g_retailWeaponCalibration.controllerToWeaponRotation)
            && (bodyAnchoredControllerRig && hostSpatialProps
                ? g_retailWeaponCalibration
                        .usesTrackedWristSocketPositionAnchor
                    && finiteVec3(
                        g_retailWeaponCalibration.weaponToWristLocal)
                    && g_retailWeaponCalibration.handMeshRotationValid
                : g_retailWeaponCalibration.usesStageLocalBodyPositionAnchor
                ? finiteVec3(
                    g_retailWeaponCalibration.controllerToWeaponBodyLocal)
                : g_retailWeaponCalibration
                        .usesTrackedWristSocketPositionAnchor
                ? finiteVec3(
                    g_retailWeaponCalibration.weaponToWristLocal)
                : (finiteVec3(g_retailWeaponCalibration.controllerToWeaponPosition)
                    && lengthVec3(
                        g_retailWeaponCalibration.controllerToWeaponPosition)
                        <= getFloatFromEnv(
                            "FNVXR_RETAIL_WEAPON_MAX_CALIBRATION_UNITS",
                            48.0f)));
        logTelemetry(
            "retailWeapon calibration valid=%d source=right-aim fullSE3=1 weapon=%p localPosition=(%.4f %.4f %.4f) bodyPosition=(%.4f %.4f %.4f) weaponToWrist=(%.4f %.4f %.4f) handGripLocalQuat=(%.6f %.6f %.6f %.6f) handMeshRotationValid=%d positionAnchor=%s endpointAxisCalibration=%d endpoint=%p\n",
            g_retailWeaponCalibration.valid ? 1 : 0,
            weapon,
            g_retailWeaponCalibration.controllerToWeaponPosition.x,
            g_retailWeaponCalibration.controllerToWeaponPosition.y,
            g_retailWeaponCalibration.controllerToWeaponPosition.z,
            g_retailWeaponCalibration.controllerToWeaponBodyLocal.x,
            g_retailWeaponCalibration.controllerToWeaponBodyLocal.y,
            g_retailWeaponCalibration.controllerToWeaponBodyLocal.z,
            g_retailWeaponCalibration.weaponToWristLocal.x,
            g_retailWeaponCalibration.weaponToWristLocal.y,
            g_retailWeaponCalibration.weaponToWristLocal.z,
            g_retailWeaponCalibration.rightHandGripLocalRotation.x,
            g_retailWeaponCalibration.rightHandGripLocalRotation.y,
            g_retailWeaponCalibration.rightHandGripLocalRotation.z,
            g_retailWeaponCalibration.rightHandGripLocalRotation.w,
            g_retailWeaponCalibration.handMeshRotationValid ? 1 : 0,
            g_retailWeaponCalibration.usesStageLocalBodyPositionAnchor
                ? "tracked-hand-child"
                : g_retailWeaponCalibration
                        .usesTrackedWristSocketPositionAnchor
                ? "measured-stock-wrist-socket"
                : "controller-local",
            endpointAxisCalibration ? 1 : 0,
            endpoint);
    }
    if (!g_retailWeaponCalibration.valid)
        return result;

    const Matrix33 desiredWorldRotation = multiplyMatrix33(
        controller.rotation,
        g_retailWeaponCalibration.controllerToWeaponRotation);
    const Vec3 desiredWorldPosition =
        g_retailWeaponCalibration.usesStageLocalBodyPositionAnchor
        // applyRetailArmFabrik has already updated the hand and propagated
        // its child transforms. Retain that attached weapon translation.
        ? weaponWorldPosition
        : g_retailWeaponCalibration.usesTrackedWristSocketPositionAnchor
        ? subtractVec3(
            hostSpatialHandWristWorld(controller, false),
            transformVec3(
                desiredWorldRotation,
                g_retailWeaponCalibration.weaponToWristLocal))
        : addVec3(
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
    result.writeRequested = applyWrites
        && (physicalHeadsetPlayRequested()
            || envEnabled("FNVXR_RETAIL_WEAPON_APPLY", false));
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
    if (g_retailWeaponCalibration.usesTrackedWristSocketPositionAnchor
        && finiteMatrix33(result.actualWorldRotation)
        && finiteVec3(result.actualWorldPosition))
    {
        result.wristSocketTargetWorldPosition =
            hostSpatialHandWristWorld(controller, false);
        result.wristSocketWorldPosition = addVec3(
            result.actualWorldPosition,
            transformVec3(
                result.actualWorldRotation,
                g_retailWeaponCalibration.weaponToWristLocal));
        result.wristSocketResidualUnits = lengthVec3(subtractVec3(
            result.wristSocketWorldPosition,
            result.wristSocketTargetWorldPosition));
        result.wristSocketMeasured =
            finiteVec3(result.wristSocketWorldPosition)
            && finiteVec3(result.wristSocketTargetWorldPosition)
            && std::isfinite(result.wristSocketResidualUnits);
    }
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
    if (!process || !envEnabled("FNVXR_RETAIL_PROJECTILE_NODE_HOOK", false))
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
    const bool gameplay =
        runtimePhaseFromMenuBits(menuBits) == RuntimePhase::Gameplay
        && (menuBits & fnvxr::shared::RuntimeBlockingMenuBits) == 0;
    const bool livePipBoy = fnvxr::engine::live_pipboy::hasFocusedScreen(
            menuBits)
        && !fnvxr::engine::live_pipboy::hasConflictingMenu(menuBits);
    return gameplay || livePipBoy;
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
    g_retailRigOriginSource = RetailRigOriginSource::None;
    g_lastRetailRigPoseSequence = 0;
    g_retailRigContinuityPose = {};
    g_haveRetailRigMotionSample = false;
    g_retailRigHeadOnlySamples = 0;
    g_retailRigControllerOnlySamples = 0;
}

void captureRetailRigContinuityPose()
{
    const RetailArmNodes& arm = g_retailRigNodes.right;
    void* weapon = g_retailRigNodes.weapon;
    if (!g_retailRigNodes.root || !arm.upperArm || !arm.forearm || !arm.hand
        || !weapon || niObjectKind(weapon) == 0)
    {
        g_retailRigContinuityPose = {};
        return;
    }

    RetailRigContinuityPose pose {};
    pose.root = g_retailRigNodes.root;
    pose.upperArm = arm.upperArm;
    pose.forearm = arm.forearm;
    pose.hand = arm.hand;
    pose.weapon = weapon;
    pose.referenceSpaceGeneration = g_retailRigReferenceSpaceGeneration;
    pose.upperArmLocalRotation = readMatrix33(
        reinterpret_cast<std::uintptr_t>(arm.upperArm) + NiAvObjectLocalRotationOffset);
    pose.forearmLocalRotation = readMatrix33(
        reinterpret_cast<std::uintptr_t>(arm.forearm) + NiAvObjectLocalRotationOffset);
    pose.handLocalRotation = readMatrix33(
        reinterpret_cast<std::uintptr_t>(arm.hand) + NiAvObjectLocalRotationOffset);
    pose.handLocalTranslation = readVec3(
        reinterpret_cast<std::uintptr_t>(arm.hand) + NiAvObjectLocalTranslationOffset);
    pose.weaponLocalRotation = readMatrix33(
        reinterpret_cast<std::uintptr_t>(weapon) + NiAvObjectLocalRotationOffset);
    pose.weaponLocalTranslation = readVec3(
        reinterpret_cast<std::uintptr_t>(weapon) + NiAvObjectLocalTranslationOffset);
    pose.weaponLocalScale = readFloat(
        reinterpret_cast<std::uintptr_t>(weapon) + NiAvObjectLocalScaleOffset,
        0.0f);
    pose.valid = finiteMatrix33(pose.upperArmLocalRotation)
        && finiteMatrix33(pose.forearmLocalRotation)
        && finiteMatrix33(pose.handLocalRotation)
        && finiteVec3(pose.handLocalTranslation)
        && finiteMatrix33(pose.weaponLocalRotation)
        && finiteVec3(pose.weaponLocalTranslation)
        && std::isfinite(pose.weaponLocalScale)
        && std::fabs(pose.weaponLocalScale) >= 0.0001f;
    g_retailRigContinuityPose = pose;
}

bool replayRetailRigContinuityPose()
{
    constexpr UInt32 MaximumConsecutiveReplays = 3u;
    RetailRigContinuityPose& pose = g_retailRigContinuityPose;
    const RetailArmNodes& arm = g_retailRigNodes.right;
    if (!pose.valid || pose.consecutiveReplays >= MaximumConsecutiveReplays
        || pose.referenceSpaceGeneration != g_retailRigReferenceSpaceGeneration
        || pose.root != g_retailRigNodes.root
        || pose.upperArm != arm.upperArm || pose.forearm != arm.forearm
        || pose.hand != arm.hand || pose.weapon != g_retailRigNodes.weapon)
    {
        pose = {};
        return false;
    }

    const bool wrote = writeMatrix33(
            reinterpret_cast<std::uintptr_t>(arm.upperArm) + NiAvObjectLocalRotationOffset,
            pose.upperArmLocalRotation)
        && writeMatrix33(
            reinterpret_cast<std::uintptr_t>(arm.forearm) + NiAvObjectLocalRotationOffset,
            pose.forearmLocalRotation)
        && writeMatrix33(
            reinterpret_cast<std::uintptr_t>(arm.hand) + NiAvObjectLocalRotationOffset,
            pose.handLocalRotation)
        && writeVec3(
            reinterpret_cast<std::uintptr_t>(arm.hand) + NiAvObjectLocalTranslationOffset,
            pose.handLocalTranslation)
        && writeMatrix33(
            reinterpret_cast<std::uintptr_t>(pose.weapon) + NiAvObjectLocalRotationOffset,
            pose.weaponLocalRotation)
        && writeVec3(
            reinterpret_cast<std::uintptr_t>(pose.weapon) + NiAvObjectLocalTranslationOffset,
            pose.weaponLocalTranslation)
        && writeFloat(
            reinterpret_cast<std::uintptr_t>(pose.weapon) + NiAvObjectLocalScaleOffset,
            pose.weaponLocalScale);
    if (!wrote)
    {
        pose = {};
        return false;
    }

    forwardKinematics(arm.upperArm);
    forwardKinematics(pose.weapon);
    ++pose.consecutiveReplays;
    logTelemetry(
        "retailRig continuity replay count=%lu max=%lu reason=transient-right-solve-failure\n",
        static_cast<unsigned long>(pose.consecutiveReplays),
        static_cast<unsigned long>(MaximumConsecutiveReplays));
    return true;
}

void logRetailRigGateSkip(
    LONG& reasonCounter,
    const char* reason,
    const VrRigPoseSnapshot& pose,
    void* root = nullptr)
{
    const LONG count = InterlockedIncrement(&reasonCounter);
    if (count <= 12 || count % 300 == 0)
    {
        logTelemetry(
            "retailRig skipped count=%ld reason=%s poseSeq=%ld poseFrame=%llu tracking=0x%03lX root=%p\n",
            count,
            reason ? reason : "unknown",
            pose.sequence,
            static_cast<unsigned long long>(pose.frame),
            static_cast<unsigned long>(pose.trackingFlags),
            root);
    }
}

SharedWeaponFrameState* sharedWeaponFrameState()
{
    if (g_weaponFrameState)
        return g_weaponFrameState;
    g_weaponFrameMapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(SharedWeaponFrameState),
        fnvxr::shared::WeaponFrameSharedMappingName);
    if (!g_weaponFrameMapping)
        return nullptr;
    const bool created = GetLastError() != ERROR_ALREADY_EXISTS;
    g_weaponFrameState = static_cast<SharedWeaponFrameState*>(MapViewOfFile(
        g_weaponFrameMapping,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        sizeof(SharedWeaponFrameState)));
    if (!g_weaponFrameState)
    {
        CloseHandle(g_weaponFrameMapping);
        g_weaponFrameMapping = nullptr;
        return nullptr;
    }
    if (created
        || g_weaponFrameState->magic != fnvxr::shared::WeaponFrameSharedMagic
        || g_weaponFrameState->version != fnvxr::shared::WeaponFrameSharedVersion)
    {
        std::memset(g_weaponFrameState, 0, sizeof(*g_weaponFrameState));
        g_weaponFrameState->magic = fnvxr::shared::WeaponFrameSharedMagic;
        g_weaponFrameState->version = fnvxr::shared::WeaponFrameSharedVersion;
    }
    return g_weaponFrameState;
}

void publishWeaponFrameCommit(
    const VrRigPoseSnapshot& pose,
    bool rightControllerUsable,
    bool rightAimUsable,
    bool rightSolved,
    bool weaponWritten,
    bool weaponAligned,
    bool handMeshRotationValid,
    const Quat& rightHandGripLocalRotation,
    bool pipBoyScreenPoseValid,
    const Vec3& pipBoyScreenGripLocalPosition,
    const Quat& pipBoyScreenGripLocalRotation,
    const Vec3& rightHandWorld,
    const Vec3& weaponWorld,
    const Matrix33& weaponWorldRotation)
{
    SharedWeaponFrameState* state = sharedWeaponFrameState();
    if (!state)
        return;
    UInt32 flags = 0;
    if (rightControllerUsable)
        flags |= fnvxr::shared::WeaponFrameFlagRightGripCurrent;
    if (rightAimUsable)
        flags |= fnvxr::shared::WeaponFrameFlagRightAimCurrent;
    if (rightSolved)
        flags |= fnvxr::shared::WeaponFrameFlagArmSolved;
    if (weaponWritten)
        flags |= fnvxr::shared::WeaponFrameFlagWeaponWritten;
    if (weaponAligned)
        flags |= fnvxr::shared::WeaponFrameFlagWeaponAligned;
    if (handMeshRotationValid)
        flags |= fnvxr::shared::WeaponFrameFlagHandMeshRotationValid;
    if (pipBoyScreenPoseValid)
        flags |= fnvxr::shared::WeaponFrameFlagPipBoyScreenPoseValid;
    const bool complete =
        (flags & fnvxr::shared::WeaponFrameRequiredFlags)
            == fnvxr::shared::WeaponFrameRequiredFlags
        && g_retailRigNodes.root
        && g_retailRigNodes.right.hand
        && g_retailRigNodes.weapon
        && finiteVec3(rightHandWorld)
        && finiteVec3(weaponWorld)
        && finiteMatrix33(weaponWorldRotation)
        && (!hostSpatialPropReplacementRequested()
            || (handMeshRotationValid
                && finiteUsableQuat(rightHandGripLocalRotation)
                && pipBoyScreenPoseValid
                && finiteVec3(pipBoyScreenGripLocalPosition)
                && finiteUsableQuat(pipBoyScreenGripLocalRotation)));
    // Fallout invokes the animation seam more than once for one OpenXR pose.
    // A later cull/stock callback can temporarily hide the arm branch after an
    // earlier callback already committed the exact current controller pose.
    // Do not let that transient callback erase the valid same-pose handoff
    // before the first-person renderer consumes it. A genuinely newer pose
    // still has to produce a new complete commit.
    const LONG publishedSequence = InterlockedCompareExchange(
        &state->producerSequence, 0, 0);
    if (fnvxr::shared::sequencedValueIsPublished(publishedSequence))
    {
        MemoryBarrier();
        const UInt32 publishedStatus = state->status;
        const UInt32 publishedPoseSequence = state->poseSequence;
        const UInt64 publishedPoseFrame = state->poseFrame;
        MemoryBarrier();
        const LONG confirmedSequence = InterlockedCompareExchange(
            &state->producerSequence, 0, 0);
        if (publishedSequence == confirmedSequence
            && fnvxr::weapon_frame::preserveCommittedPose(
                publishedStatus,
                fnvxr::shared::WeaponFramePoseCommitted,
                publishedPoseSequence,
                publishedPoseFrame,
                static_cast<UInt32>(pose.sequence),
                pose.frame,
                complete))
        {
            return;
        }
    }
    if (!fnvxr::shared::beginSequencedSharedWrite(state->producerSequence))
        return;
    state->magic = fnvxr::shared::WeaponFrameSharedMagic;
    state->version = fnvxr::shared::WeaponFrameSharedVersion;
    state->status = complete
        ? fnvxr::shared::WeaponFramePoseCommitted
        : fnvxr::shared::WeaponFrameInvalid;
    state->commitId = complete ? ++g_weaponFrameCommitId : 0u;
    state->poseSequence = complete ? static_cast<UInt32>(pose.sequence) : 0u;
    state->referenceSpaceGeneration = complete
        ? pose.referenceSpaceGeneration : 0u;
    state->poseFrame = complete ? pose.frame : 0u;
    state->producerEpoch = complete ? pose.producerEpoch : 0u;
    state->flags = flags;
    state->rootAddress = complete
        ? static_cast<UInt32>(reinterpret_cast<std::uintptr_t>(g_retailRigNodes.root)) : 0u;
    state->rightHandAddress = complete
        ? static_cast<UInt32>(reinterpret_cast<std::uintptr_t>(g_retailRigNodes.right.hand)) : 0u;
    state->weaponAddress = complete
        ? static_cast<UInt32>(reinterpret_cast<std::uintptr_t>(g_retailRigNodes.weapon)) : 0u;
    const float hand[3] { rightHandWorld.x, rightHandWorld.y, rightHandWorld.z };
    const float weapon[3] { weaponWorld.x, weaponWorld.y, weaponWorld.z };
    const float rotation[9] {
        weaponWorldRotation.m[0][0], weaponWorldRotation.m[0][1], weaponWorldRotation.m[0][2],
        weaponWorldRotation.m[1][0], weaponWorldRotation.m[1][1], weaponWorldRotation.m[1][2],
        weaponWorldRotation.m[2][0], weaponWorldRotation.m[2][1], weaponWorldRotation.m[2][2],
    };
    for (int i = 0; i < 3; ++i)
    {
        state->rightHandWorldPos[i] = complete ? hand[i] : 0.0f;
        state->weaponWorldPos[i] = complete ? weapon[i] : 0.0f;
    }
    for (int i = 0; i < 9; ++i)
        state->weaponWorldRot[i] = complete ? rotation[i] : 0.0f;
    const float handRotation[4] {
        rightHandGripLocalRotation.x,
        rightHandGripLocalRotation.y,
        rightHandGripLocalRotation.z,
        rightHandGripLocalRotation.w,
    };
    for (int i = 0; i < 4; ++i)
    {
        state->rightHandGripLocalRot[i] =
            complete && handMeshRotationValid
                ? handRotation[i]
                : (i == 3 ? 1.0f : 0.0f);
    }
    const float pipBoyPosition[3] {
        pipBoyScreenGripLocalPosition.x,
        pipBoyScreenGripLocalPosition.y,
        pipBoyScreenGripLocalPosition.z,
    };
    const float pipBoyRotation[4] {
        pipBoyScreenGripLocalRotation.x,
        pipBoyScreenGripLocalRotation.y,
        pipBoyScreenGripLocalRotation.z,
        pipBoyScreenGripLocalRotation.w,
    };
    for (int i = 0; i < 3; ++i)
    {
        state->leftPipBoyScreenGripLocalPos[i] =
            complete && pipBoyScreenPoseValid
                ? pipBoyPosition[i] : 0.0f;
    }
    for (int i = 0; i < 4; ++i)
    {
        state->leftPipBoyScreenGripLocalRot[i] =
            complete && pipBoyScreenPoseValid
                ? pipBoyRotation[i]
                : (i == 3 ? 1.0f : 0.0f);
    }
    fnvxr::shared::endSequencedSharedWrite(state->producerSequence);
}

void onRetailPostAnimation(void* animData)
{
    if (!physicalHeadsetPlayRequested()
        && !envEnabled("FNVXR_RETAIL_RIG_ENABLE", false))
        return;
    const bool trackedPropAssist = trackedPropAssistProfileRequested();
    const bool headlessStereoRigVisualTrial =
        headsetControllerRigVisualTrialRequested();
    const bool visualOnlyRigTrial = trackedPropAssist
        || headlessStereoRigVisualTrial;
    // Engine-center physical play does not use the retired native
    // single-traversal camera path that publishes SharedVrOriginState.  Its
    // HMD camera transaction is owned by the retail D3D bridge, so anchor the
    // controller rig once to the engine-authored body/first-person roots just
    // like the proven headless engine-center route.  Subsequent hand targets
    // remain body-relative and cannot inherit the moving HMD camera.
    const bool bodyAnchoredEngineCenterRig = visualOnlyRigTrial
        || physicalHeadsetEngineCenterRigRequested();
    if (visualOnlyRigTrial && !retailRigVisualOnlyTrialLeaseCurrent())
    {
        static LONG rejectedLease = 0;
        const LONG count = InterlockedIncrement(&rejectedLease);
        if (count <= 12 || count % 300 == 0)
        {
            logTelemetry(
                "retailRigVisualTrial rig skipped count=%ld reason=lease-not-current; no visual rig write performed\n",
                count);
        }
        return;
    }
    if (!retailRigGameplayAllowed())
    {
        // Pip-Boy/inventory work may reconstruct children below the same
        // first-person root. A pointer comparison cannot detect that rebuild.
        g_retailRigRediscoveryRequested = true;
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
    // Keep the tracked-prop exclusion explicit: that lease must never reach
    // projectile-node mutation. The separate headless stereo lease is also
    // visual-only, so it remains inside the same outer exclusion.
    if (!trackedPropAssist)
    {
        if (!headlessStereoRigVisualTrial)
        {
            void* playerProcess = player
                ? readPointer(reinterpret_cast<std::uintptr_t>(player) + MobileObjectBaseProcessOffset)
                : nullptr;
            installProjectileNodeConsumeHook(playerProcess);
        }
    }

    VrRigPoseSnapshot pose {};
    SharedVrOriginState authoritativeOrigin {};
    if (bodyAnchoredEngineCenterRig)
    {
        if (g_renderRigPoseOverrideActive)
            pose = g_renderRigPoseOverride;
        else if (!readLatestRetailRigPose(pose))
        {
            const LONG count = InterlockedIncrement(&g_retailRigPoseOriginUnavailableCount);
            if (count <= 12 || count % 300 == 0)
            {
                logTelemetry(
                    "retailRigVisualTrial rig skipped count=%ld reason=pose-unavailable\n",
                    count);
            }
            return;
        }
    }
    else if (!readCoherentRetailRigPoseAndOrigin(pose, authoritativeOrigin))
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
    if (!bodyAnchoredEngineCenterRig
        && g_haveRetailRigOrigin
        && (authoritativeOrigin.generation != g_retailRigReferenceSpaceGeneration
            || authoritativeOrigin.producerEpoch != g_retailRigProducerEpoch
            || authoritativeOrigin.poseSequence != g_retailRigOriginPoseSequence))
    {
        resetRetailRigOrigin("authoritative-origin-changed");
    }
    // The animation call site runs several times for one OpenXR frame. The
    // first-person rig normally has one solve per stable pose. A renderer
    // restore is deliberately allowed to repeat that exact pose after stock
    // animation overwrote it and immediately before the binocular traversal.
    if (fnvxr::weapon_frame::duplicatePoseSolveCanBeSkipped(
            pose.sequence,
            g_lastRetailRigPoseSequence,
            g_renderRigPoseOverrideActive))
        return;
    if ((pose.trackingFlags & fnvxr::shared::VrPoseTrackingHmd) == 0)
    {
        logRetailRigGateSkip(g_retailRigNoHmdCount, "hmd-not-tracked", pose);
        return;
    }
    const bool leftControllerUsable =
        (pose.trackingFlags & fnvxr::shared::VrPoseTrackingLeftGripActive) != 0
        && (pose.trackingFlags & fnvxr::shared::VrPoseTrackingLeftGripCurrent) != 0;
    const bool rightControllerUsable =
        (pose.trackingFlags & fnvxr::shared::VrPoseTrackingRightGripActive) != 0
        && (pose.trackingFlags & fnvxr::shared::VrPoseTrackingRightGripCurrent) != 0;
    const bool rightAimUsable =
        (pose.trackingFlags & fnvxr::shared::VrPoseTrackingRightAimActive) != 0
        && (pose.trackingFlags & fnvxr::shared::VrPoseTrackingRightAimCurrent) != 0;
    if (bodyAnchoredEngineCenterRig
        && (!rightControllerUsable || !rightAimUsable))
    {
        logRetailRigGateSkip(
            g_retailRigNoCurrentControllerCount,
            "right-grip-or-aim-not-current",
            pose);
        return;
    }
    if (!leftControllerUsable && !rightControllerUsable)
    {
        logRetailRigGateSkip(
            g_retailRigNoCurrentControllerCount,
            "no-current-controller-grip",
            pose);
        return;
    }

    void* root = retrievePlayerRootNode(true);
    if (!root)
        root = readPointer(Camera1stBipedNodeAddress);
    if (!root)
    {
        logRetailRigGateSkip(g_retailRigNoRootCount, "first-person-root-unavailable", pose);
        return;
    }
    if (bodyAnchoredEngineCenterRig
        && g_haveRetailRigOrigin
        && g_retailRigNodes.root
        && root != g_retailRigNodes.root)
    {
        resetRetailRigOrigin("first-person-rig-root-changed");
    }
    if (g_retailRigRediscoveryRequested
        || root != g_retailRigNodes.root
        || !retailRigNodesComplete(g_retailRigNodes))
    {
        if (!discoverRetailRigNodes(root))
        {
            logRetailRigGateSkip(
                g_retailRigDiscoveryFailureCount,
                "first-person-rig-discovery-failed",
                pose,
                root);
            return;
        }
    }
    const UInt64 weaponRefreshStride = static_cast<UInt64>((std::max)(
        1,
        getIntFromEnv("FNVXR_RETAIL_WEAPON_REFRESH_SOLVES", 15)));
    bool weaponBindingReady = currentRetailWeaponBindingReady();
    if (g_retailWeaponRefreshRequested
        || g_retailRigSolveCount == 0
        || (g_retailRigSolveCount % weaponRefreshStride) == 0)
    {
        weaponBindingReady = refreshRetailWeaponNodes();
    }
    if (!retailRigNodesComplete(g_retailRigNodes) || !weaponBindingReady)
    {
        logRetailRigGateSkip(
            g_retailRigIncompleteCount,
            weaponBindingReady
                ? "first-person-rig-incomplete"
                : "equipped-weapon-model-transition",
            pose,
            root);
        return;
    }

    void* bodyRoot = retrievePlayerRootNode(false);
    if (!bodyRoot)
    {
        logRetailRigGateSkip(g_retailRigNoBodyRootCount, "body-root-unavailable", pose);
        return;
    }
    const Matrix33 bodyWorldRotation = readMatrix33(
        reinterpret_cast<std::uintptr_t>(bodyRoot) + NiAvObjectWorldRotationOffset);
    const Vec3 bodyWorldPosition = readVec3(
        reinterpret_cast<std::uintptr_t>(bodyRoot) + NiAvObjectWorldTranslationOffset);
    const float bodyWorldScale = readFloat(
        reinterpret_cast<std::uintptr_t>(bodyRoot) + NiAvObjectWorldScaleOffset,
        0.0f);
    const Vec3 stableCameraWorld = bodyAnchoredEngineCenterRig
        ? readVec3(
            reinterpret_cast<std::uintptr_t>(root)
                + NiAvObjectWorldTranslationOffset)
        : Vec3 {
            authoritativeOrigin.renderCameraWorldPos[0],
            authoritativeOrigin.renderCameraWorldPos[1],
            authoritativeOrigin.renderCameraWorldPos[2]
        };
    if (!finiteMatrix33(bodyWorldRotation)
        || !finiteVec3(bodyWorldPosition)
        || !finiteVec3(stableCameraWorld)
        || !std::isfinite(bodyWorldScale)
        || std::fabs(bodyWorldScale) < 0.0001f
        || (!bodyAnchoredEngineCenterRig
            && reinterpret_cast<std::uintptr_t>(bodyRoot)
                != authoritativeOrigin.bodyRootAddress))
    {
        static LONG loggedAnchorUnavailable = 0;
        const LONG count = InterlockedIncrement(&loggedAnchorUnavailable);
        if (count <= 12 || count % 300 == 0)
        {
            logTelemetry(
                "retailRig skipped count=%ld reason=stable-body-anchor-unavailable bodyRoot=%p renderCamera=0x%08lx renderCameraWorldValid=%lu\n",
                count,
                bodyRoot,
                bodyAnchoredEngineCenterRig
                    ? static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(root))
                    : static_cast<unsigned long>(authoritativeOrigin.renderCameraAddress),
                bodyAnchoredEngineCenterRig
                    ? 1ul
                    : static_cast<unsigned long>(authoritativeOrigin.renderCameraWorldValid));
        }
        return;
    }

    if (g_haveRetailRigOrigin && g_retailRigOriginBodyRoot != bodyRoot)
        resetRetailRigOrigin("body-root-changed");
    if (!g_haveRetailRigOrigin)
    {
        const bool originCaptured = bodyAnchoredEngineCenterRig
            ? captureTrackedPropAssistRigOrigin(
                pose,
                bodyRoot,
                root,
                headlessStereoRigVisualTrial)
            : captureRetailRigOrigin(pose, authoritativeOrigin, bodyRoot);
        if (!originCaptured)
            return;
    }

    // The hand/controller origin follows only the engine-authored body frame.
    // It must never use the current render camera because that camera can
    // already contain the HMD overlay during the Gamebryo render traversal.
    const Vec3 bodyAnchorWorld = addVec3(
        bodyWorldPosition,
        transformVec3(
            bodyWorldRotation,
            scaleVec3(g_retailRigBodyAnchorLocal, bodyWorldScale)));
    if (!finiteVec3(bodyAnchorWorld))
        return;

    const RetailControllerWorldPose leftController = retailControllerWorldPose(
        pose, true, bodyAnchorWorld, bodyWorldRotation);
    const RetailControllerWorldPose rightController = retailControllerWorldPose(
        pose, false, bodyAnchorWorld, bodyWorldRotation);
    const bool applyWrites = physicalHeadsetPlayRequested()
        || envEnabled("FNVXR_RETAIL_RIG_APPLY", false);
    const bool hostSpatialProps = hostSpatialPropReplacementRequested();
    float leftError = 0.0f;
    float rightError = 0.0f;
    const bool rightSolved = hostSpatialProps
        ? rightControllerUsable
        : rightControllerUsable && applyRetailArmFabrik(
            g_retailRigNodes.right,
            g_retailRightCalibration,
            rightController,
            bodyWorldRotation,
            false,
            applyWrites,
            rightError);
    const bool leftSolved = hostSpatialProps
        ? leftControllerUsable
        : leftControllerUsable && applyRetailArmFabrik(
            g_retailRigNodes.left,
            g_retailLeftCalibration,
            leftController,
            bodyWorldRotation,
            true,
            applyWrites,
            leftError);
    const bool pipBoyTracked = hostSpatialProps
        ? leftControllerUsable
            && measureHostSpatialPipBoyCalibration()
        : leftControllerUsable
            && applyRetailTrackedPipBoy(leftController, applyWrites);
    if (hostSpatialProps && leftControllerUsable)
    {
        g_latestTrackedLeftHandWorld = leftController.wristPosition;
        g_latestTrackedLeftHandValid = finiteVec3(
            g_latestTrackedLeftHandWorld);
    }
    else if (leftSolved && g_retailRigNodes.left.hand)
    {
        g_latestTrackedLeftHandWorld = readVec3(
            reinterpret_cast<std::uintptr_t>(g_retailRigNodes.left.hand)
                + NiAvObjectWorldTranslationOffset);
        g_latestTrackedLeftHandValid = finiteVec3(
            g_latestTrackedLeftHandWorld);
    }
    g_latestCompleteTrackedPropsApplied = leftSolved && pipBoyTracked;
    g_latestCompleteTrackedPropsPoseSequence =
        g_latestCompleteTrackedPropsApplied ? pose.sequence : 0;
    g_latestCompleteTrackedPropsPoseFrame =
        g_latestCompleteTrackedPropsApplied ? pose.frame : 0u;
    if (applyWrites && !hostSpatialProps)
    {
        makeRetailVrSurfaceVisible(g_retailRigNodes.upperBodyMesh);
        makeRetailVrSurfaceVisible(g_retailRigNodes.leftHandMesh);
        makeRetailVrSurfaceVisible(g_retailRigNodes.rightHandMesh);
        makeRetailVrSurfaceVisible(g_retailRigNodes.pipBoy);
    }
    // Weapon aim is controller-owned and remains valid even when the visual
    // arm chain is momentarily outside its FABRIK reach envelope. Coupling
    // these writes made the stock animation flash through during wide firing
    // motions despite a current, usable aim pose.
    RetailWeaponApplyResult weaponResult = rightControllerUsable
        ? applyRetailWeaponAim(
            rightController,
            applyWrites)
        : RetailWeaponApplyResult {};
    bool continuityReplayed = false;
    if (!hostSpatialProps
        && (headlessStereoRigVisualTrial || physicalHeadsetEngineCenterRigRequested())
        && applyWrites)
    {
        if (rightSolved && weaponResult.writeVerified)
            captureRetailRigContinuityPose();
        else if (!weaponResult.writeVerified)
            continuityReplayed = replayRetailRigContinuityPose();
    }
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
        && (!hostSpatialProps
            || (weaponResult.wristSocketMeasured
                && weaponResult.wristSocketResidualUnits
                    <= getFloatFromEnv(
                        "FNVXR_RETAIL_WEAPON_MAX_SOCKET_RESIDUAL_UNITS",
                        0.25f)))
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

    const Vec3 committedRightHandWorld = hostSpatialProps
        ? hostSpatialHandWristWorld(rightController, false)
        : g_retailRigNodes.right.hand
        ? readVec3(
            reinterpret_cast<std::uintptr_t>(g_retailRigNodes.right.hand)
                + NiAvObjectWorldTranslationOffset)
        : Vec3 {};
    const Vec3 committedWeaponWorld = g_retailRigNodes.weapon
        ? readVec3(
            reinterpret_cast<std::uintptr_t>(g_retailRigNodes.weapon)
                + NiAvObjectWorldTranslationOffset)
        : Vec3 {};
    const Matrix33 committedWeaponWorldRotation = g_retailRigNodes.weapon
        ? readMatrix33(
            reinterpret_cast<std::uintptr_t>(g_retailRigNodes.weapon)
                + NiAvObjectWorldRotationOffset)
        : Matrix33 {};
    if (physicalHeadsetEngineCenterRigRequested()
        || headlessStereoRigVisualTrial)
    {
        publishWeaponFrameCommit(
            pose,
            rightControllerUsable,
            rightAimUsable,
            rightSolved,
            weaponResult.writeVerified,
            weaponAligned,
            g_retailWeaponCalibration.handMeshRotationValid,
            g_retailWeaponCalibration.rightHandGripLocalRotation,
            g_retailPipBoyCalibration.hostSpatialValid,
            g_retailPipBoyCalibration.screenGripLocalPositionMeters,
            g_retailPipBoyCalibration.screenGripLocalRotation,
            committedRightHandWorld,
            committedWeaponWorld,
            committedWeaponWorldRotation);
    }

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
        const Vec3 rightHandWorld = committedRightHandWorld;
        const Vec3 rightHandTargetWorld = hostSpatialProps
            ? hostSpatialHandWristWorld(rightController, false)
            :
            g_retailRightCalibration.usesStageLocalBodyPositionAnchor
            ? addVec3(
                rightController.wristPosition,
                transformVec3(
                    bodyWorldRotation,
                    g_retailRightCalibration.controllerToWristBodyLocal))
            : addVec3(
                rightController.wristPosition,
                transformVec3(
                    rightController.wristRotation,
                    g_retailRightCalibration.controllerToWristLocal));
        const Vec3 rightHandLocalUnits = transformVec3(
            inverseBodyRotation,
            subtractVec3(rightHandWorld, bodyAnchorWorld));
        const Vec3 weaponWorld = committedWeaponWorld;
        const Matrix33 weaponWorldRotation = committedWeaponWorldRotation;
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
                "\"originSource\":\"%s\","
                "\"anchorSource\":\"%s\","
                "\"cameraInput\":\"hmd-only\",\"rigInput\":\"controller-only\","
                "\"apply\":%s,\"leftSolved\":%s,\"rightSolved\":%s,\"pipBoyTracked\":%s,\"continuityReplayed\":%s,\"weaponAligned\":%s,\"handMeshRotationValid\":%s,"
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
                "\"weaponAngularResidualRadians\":%.7f,\"wristSocketMeasured\":%s,"
                "\"wristSocketResidualUnits\":%.6f,"
                "\"wristSocketWorld\":[%.5f,%.5f,%.5f],"
                "\"wristSocketTargetWorld\":[%.5f,%.5f,%.5f],\"muzzleMeasured\":%s,"
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
                visualOnlyRigTrial
                    ? static_cast<unsigned long>(pose.sequence)
                    : static_cast<unsigned long>(authoritativeOrigin.renderPoseSequence),
                1.0f - 2.0f * (
                    g_retailRigOriginHmdRot.x * g_retailRigOriginHmdRot.x
                    + g_retailRigOriginHmdRot.z * g_retailRigOriginHmdRot.z),
                retailRigOriginSourceName(),
                retailRigAnchorSourceName(),
                applyWrites ? "true" : "false",
                leftSolved ? "true" : "false",
                rightSolved ? "true" : "false",
                pipBoyTracked ? "true" : "false",
                continuityReplayed ? "true" : "false",
                weaponAligned ? "true" : "false",
                g_retailWeaponCalibration.handMeshRotationValid
                    ? "true" : "false",
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
                weaponResult.wristSocketMeasured ? "true" : "false",
                weaponResult.wristSocketResidualUnits,
                weaponResult.wristSocketWorldPosition.x,
                weaponResult.wristSocketWorldPosition.y,
                weaponResult.wristSocketWorldPosition.z,
                weaponResult.wristSocketTargetWorldPosition.x,
                weaponResult.wristSocketTargetWorldPosition.y,
                weaponResult.wristSocketTargetWorldPosition.z,
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
            (physicalHeadsetPlayRequested()
                || envEnabled("FNVXR_RETAIL_WEAPON_APPLY", false)) ? 1 : 0,
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
    const bool headlessStereoRigVisualTrial =
        headsetControllerRigVisualTrialRequested();
    if (stereoVisualTrialProfileSelected())
    {
        if (!headlessStereoRigVisualTrial)
        {
            logTelemetry(
                "retailRig hook hard-blocked: stereo visual trial is publication-only in the plugin\n");
            return false;
        }
    }
    if (g_retailRigHookInstalled)
        return true;
    const bool requested = physicalHeadsetPlayRequested()
        || envEnabled("FNVXR_RETAIL_RIG_ENABLE", false);
    if (!requested)
    {
        logTelemetry("retailRig hook install disabled\n");
        return true;
    }
    const bool trackedPropAssist = trackedPropAssistProfileRequested();
    if (trackedPropAssist)
    {
        if (!trackedPropAssistMutationAllowedAtDecision())
        {
            logTelemetry(
                "retailRig hook hard-blocked: tracked-prop visual-only compatibility/configuration proof incomplete\n");
            return true;
        }
    }
    else if (headlessStereoRigVisualTrial)
    {
        if (!headlessStereoRigVisualTrialMutationAllowedAtDecision())
        {
            logTelemetry(
                "retailRig hook hard-blocked: headless stereo visual-rig compatibility/configuration proof incomplete\n");
            return true;
        }
    }
    else if (physicalHeadsetPlayProfileSelected()
        && envEnabled("FNVXR_PHYSICAL_HEADSET_PLAY", false))
    {
        // Physical play reaches this decision only after the full bridge has
        // synchronously acquired the process-bound retail runtime authority.
        // That authority revalidates the loaded executable, complete protected
        // function/vtable inventory, live ABI objects, compatibility modules,
        // process identity, and generation at the current decision point.
        // Use it for this exact retail post-animation hook instead of the
        // retired RetailMutationEvidenceToken, which is intentionally never
        // populated and otherwise makes the physical rig impossible to arm.
        if (!g_retailRuntimeAuthority.complete())
        {
            logTelemetry(
                "retailRig hook hard-blocked: physical retail runtime authority is not current\n");
            return true;
        }
    }
    else if (!retailMutationAllowedForCurrentProcess(requested))
    {
        logTelemetry("retailRig hook hard-blocked: retail mutation source/evidence proof incomplete\n");
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
        "retailRig hook installed site=%p hook=%p original=%p apply=%d authority=%s projectile=%s\n",
        pointerFromAddress32<void*>(PlayerAnimationApplyCallSiteAddress),
        reinterpret_cast<void*>(hookedRetailAnimationApply),
        pointerFromAddress32<void*>(ApplyActorAnimDataAddress),
        (physicalHeadsetPlayRequested()
            || envEnabled("FNVXR_RETAIL_RIG_APPLY", false)) ? 1 : 0,
        trackedPropAssist
            ? "tracked-prop-assist"
            : (headlessStereoRigVisualTrial
                ? "headless-stereo-rig-visual-trial"
                : "full-retail"),
        (trackedPropAssist || headlessStereoRigVisualTrial)
            ? "0-for-visual-rig-trial"
            : "managed-by-full-retail");
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

// This path is deliberately observational.  It runs only for a visible
// MessageMenu, copies a small printable snapshot of existing TileValue text,
// and never invokes a menu handler or writes a tile trait.  In particular, it
// cannot acknowledge, dismiss, or otherwise progress a load-time popup.
bool copyPrintableTileValueStringReadOnly(
    const TileValue* value,
    char* output,
    size_t outputCapacity)
{
    if (!value || !output || outputCapacity < 2)
        return false;

    output[0] = '\0';
    size_t written = 0;
    __try
    {
        char* source = value->str;
        if (!source)
            return false;
        for (size_t index = 0; index + 1 < outputCapacity; ++index)
        {
            const unsigned char character =
                static_cast<unsigned char>(source[index]);
            if (character == 0)
                break;

            if (character == '\r' || character == '\n' || character == '\t')
            {
                output[written++] = ' ';
                continue;
            }
            if (character < 0x20 || character > 0x7e)
            {
                output[0] = '\0';
                return false;
            }
            output[written++] = static_cast<char>(character);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        output[0] = '\0';
        return false;
    }

    while (written != 0 && output[written - 1] == ' ')
        --written;
    output[written] = '\0';
    return written != 0;
}

struct ReadOnlyMessageMenuTrace
{
    UInt32 tileCount = 0;
    UInt32 textCount = 0;
    UInt32 faultCount = 0;
    std::vector<void*> visitedTiles;
};

void collectReadOnlyMessageMenuText(
    void* tile,
    UInt32 depth,
    ReadOnlyMessageMenuTrace& trace)
{
    if (!tile || depth > 20 || trace.tileCount >= 256 || trace.textCount >= 64)
        return;
    if (std::find(trace.visitedTiles.begin(), trace.visitedTiles.end(), tile)
        != trace.visitedTiles.end())
    {
        return;
    }

    trace.visitedTiles.push_back(tile);
    ++trace.tileCount;
    __try
    {
        auto* values = *reinterpret_cast<TileValue***>(
            reinterpret_cast<std::uintptr_t>(tile) + 0x14);
        const UInt32 valueCount = *reinterpret_cast<UInt32*>(
            reinterpret_cast<std::uintptr_t>(tile) + 0x18);
        const UInt32 buttonId = getTileButtonId(tile);
        if (values && valueCount <= 512)
        {
            for (UInt32 index = 0; index < valueCount && trace.textCount < 64; ++index)
            {
                TileValue* value = values[index];
                char text[384] {};
                if (!copyPrintableTileValueStringReadOnly(value, text, sizeof(text)))
                    continue;

                ++trace.textCount;
                logTelemetry(
                    "messageMenu readOnlyText[%lu] depth=%lu tile=%p buttonId=%lu trait=0x%04lx text=\"%s\"\n",
                    static_cast<unsigned long>(trace.textCount),
                    static_cast<unsigned long>(depth),
                    tile,
                    static_cast<unsigned long>(buttonId),
                    static_cast<unsigned long>(value->id),
                    text);
            }
        }

        auto* node = reinterpret_cast<TileListNode*>(
            reinterpret_cast<std::uintptr_t>(tile) + 0x04);
        for (UInt32 count = 0; node && count < 512 && trace.tileCount < 256; ++count)
        {
            auto* childNode = static_cast<TileChildNode*>(node->data);
            void* child = childNode ? childNode->child : nullptr;
            if (child)
                collectReadOnlyMessageMenuText(child, depth + 1, trace);
            node = node->next;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ++trace.faultCount;
    }
}

void logReadOnlyMessageMenuDiagnostic(UInt64 generation)
{
    void* menu = nullptr;
    void* tileMenu = nullptr;
    if (!validatedVisibleMenu(kMenuTypeMessage, nullptr, &menu, &tileMenu))
    {
        logTelemetry(
            "messageMenu readOnlyDiagnostic generation=%llu unavailable\n",
            static_cast<unsigned long long>(generation));
        return;
    }

    void* root = tileRootFromMenu(menu, tileMenu);
    ReadOnlyMessageMenuTrace trace {};
    logTelemetry(
        "messageMenu readOnlyDiagnostic generation=%llu menu=%p tileMenu=%p root=%p action=inspect-only\n",
        static_cast<unsigned long long>(generation),
        menu,
        tileMenu,
        root);
    collectReadOnlyMessageMenuText(root, 0, trace);
    logTelemetry(
        "messageMenu readOnlyDiagnostic complete generation=%llu tiles=%lu readableText=%lu faults=%lu action=none\n",
        static_cast<unsigned long long>(generation),
        static_cast<unsigned long>(trace.tileCount),
        static_cast<unsigned long>(trace.textCount),
        static_cast<unsigned long>(trace.faultCount));
}

struct ExactOfficialPackMessageMenuMatch
{
    UInt32 observedTitleMask = 0u;
    UInt32 observedBodyMask = 0u;
    bool observedTtwStewieDependencyTitle = false;
    bool observedTtwStewieDependencyBody = false;
    void* firstButtonOkTile = nullptr;
    UInt32 firstButtonOkTileCount = 0u;
    UInt32 tileCount = 0u;
    UInt32 faultCount = 0u;
    std::vector<void*> visitedTiles;
};

void collectExactOfficialPackMessageMenuMatch(
    void* tile,
    UInt32 depth,
    ExactOfficialPackMessageMenuMatch& match)
{
    if (!tile || depth > 20u || match.tileCount >= 256u)
        return;
    if (std::find(match.visitedTiles.begin(), match.visitedTiles.end(), tile)
        != match.visitedTiles.end())
    {
        return;
    }

    match.visitedTiles.push_back(tile);
    ++match.tileCount;
    __try
    {
        auto* values = *reinterpret_cast<TileValue***>(
            reinterpret_cast<std::uintptr_t>(tile) + 0x14);
        const UInt32 valueCount = *reinterpret_cast<UInt32*>(
            reinterpret_cast<std::uintptr_t>(tile) + 0x18);
        bool exactOkOnVisibleTile = false;
        if (values && valueCount <= 512u)
        {
            for (UInt32 index = 0u; index < valueCount; ++index)
            {
                char text[384] {};
                if (!copyPrintableTileValueStringReadOnly(
                        values[index], text, sizeof(text)))
                {
                    continue;
                }

                const std::string_view valueText { text };
                constexpr UInt32 notificationCount = static_cast<UInt32>(
                    sizeof(fnvxr::engine::stereo_visual_trial_automation::
                        OfficialPackNotifications)
                    / sizeof(fnvxr::engine::stereo_visual_trial_automation::
                        OfficialPackNotifications[0]));
                static_assert(notificationCount <= 32u,
                    "official-pack acknowledgement mask no longer fits");
                for (UInt32 notificationIndex = 0u;
                     notificationIndex < notificationCount;
                     ++notificationIndex)
                {
                    const auto& notification =
                        fnvxr::engine::stereo_visual_trial_automation::
                            OfficialPackNotifications[notificationIndex];
                    if (valueText == notification.title)
                        match.observedTitleMask |= 1u << notificationIndex;
                    if (valueText == notification.body)
                        match.observedBodyMask |= 1u << notificationIndex;
                }
                if (valueText
                    == fnvxr::engine::stereo_visual_trial_automation::
                        MessageMenuOkText)
                {
                    exactOkOnVisibleTile = true;
                }
                if (valueText
                    == fnvxr::engine::retail_fixture_automation::
                        TtwStewieDependencyWarningTitle)
                {
                    match.observedTtwStewieDependencyTitle = true;
                }
                if (valueText
                    == fnvxr::engine::retail_fixture_automation::
                        TtwStewieDependencyWarningBody)
                {
                    match.observedTtwStewieDependencyBody = true;
                }
            }
        }

        if (exactOkOnVisibleTile
            && getTileFloatByName(
                    tile,
                    "visible",
                    TileValueVisible,
                    1.0f) != 0.0f
            // The MessageMenu contains a duplicate text descendant labeled
            // OK.  The native actionable tile is uniquely the first button
            // (button index zero), as observed in the retail trace.
            && getTileButtonId(tile) == 0u)
        {
            ++match.firstButtonOkTileCount;
            match.firstButtonOkTile = match.firstButtonOkTileCount == 1u
                ? tile
                : nullptr;
        }

        auto* node = reinterpret_cast<TileListNode*>(
            reinterpret_cast<std::uintptr_t>(tile) + 0x04);
        for (UInt32 count = 0u;
             node && count < 512u && match.tileCount < 256u;
             ++count)
        {
            auto* childNode = static_cast<TileChildNode*>(node->data);
            void* child = childNode ? childNode->child : nullptr;
            if (child)
                collectExactOfficialPackMessageMenuMatch(
                    child, depth + 1u, match);
            node = node->next;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ++match.faultCount;
    }
}

bool findExactOfficialPackMessageMenuTarget(
    void** outMenu,
    void** outOkTile,
    UInt32* outOfficialPackNotificationMask = nullptr,
    bool* outExactlyOneFirstButtonOk = nullptr)
{
    if (outMenu)
        *outMenu = nullptr;
    if (outOkTile)
        *outOkTile = nullptr;
    if (outOfficialPackNotificationMask)
        *outOfficialPackNotificationMask = 0u;
    if (outExactlyOneFirstButtonOk)
        *outExactlyOneFirstButtonOk = false;

    void* menu = nullptr;
    void* tileMenu = nullptr;
    if (!validatedVisibleMenu(kMenuTypeMessage, nullptr, &menu, &tileMenu))
        return false;

    ExactOfficialPackMessageMenuMatch match {};
    collectExactOfficialPackMessageMenuMatch(
        tileRootFromMenu(menu, tileMenu), 0u, match);
    const UInt32 officialPackNotificationMask =
        match.observedTitleMask & match.observedBodyMask;
    const bool exactlyOneOfficialPackNotification =
        officialPackNotificationMask != 0u
        && (officialPackNotificationMask
            & (officialPackNotificationMask - 1u)) == 0u;
    const bool exactlyOneFirstButtonOk = match.firstButtonOkTileCount == 1u
        && match.firstButtonOkTile != nullptr;
    if (outOfficialPackNotificationMask)
        *outOfficialPackNotificationMask = officialPackNotificationMask;
    if (outExactlyOneFirstButtonOk)
        *outExactlyOneFirstButtonOk = exactlyOneFirstButtonOk;
    const bool exact = exactlyOneOfficialPackNotification
        && exactlyOneFirstButtonOk;
    if (!exact)
        return false;

    if (outMenu)
        *outMenu = menu;
    if (outOkTile)
        *outOkTile = match.firstButtonOkTile;
    return true;
}

bool findExactTtwStewieDependencyMessageMenuTarget(
    void** outMenu,
    void** outOkTile,
    bool* outExactTitle = nullptr,
    bool* outExactBody = nullptr,
    bool* outExactlyOneFirstButtonOk = nullptr)
{
    if (outMenu)
        *outMenu = nullptr;
    if (outOkTile)
        *outOkTile = nullptr;
    if (outExactTitle)
        *outExactTitle = false;
    if (outExactBody)
        *outExactBody = false;
    if (outExactlyOneFirstButtonOk)
        *outExactlyOneFirstButtonOk = false;

    void* menu = nullptr;
    void* tileMenu = nullptr;
    if (!validatedVisibleMenu(kMenuTypeMessage, nullptr, &menu, &tileMenu))
        return false;

    ExactOfficialPackMessageMenuMatch match {};
    collectExactOfficialPackMessageMenuMatch(
        tileRootFromMenu(menu, tileMenu), 0u, match);
    const bool exactlyOneFirstButtonOk = match.firstButtonOkTileCount == 1u
        && match.firstButtonOkTile != nullptr;
    if (outExactTitle)
        *outExactTitle = match.observedTtwStewieDependencyTitle;
    if (outExactBody)
        *outExactBody = match.observedTtwStewieDependencyBody;
    if (outExactlyOneFirstButtonOk)
        *outExactlyOneFirstButtonOk = exactlyOneFirstButtonOk;
    const bool exact = match.observedTtwStewieDependencyTitle
        && match.observedTtwStewieDependencyBody
        && exactlyOneFirstButtonOk;
    if (!exact)
        return false;

    if (outMenu)
        *outMenu = menu;
    if (outOkTile)
        *outOkTile = match.firstButtonOkTile;
    return true;
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
    const bool overlapsViewport = x < DirectMenuViewportWidth
        && y < DirectMenuViewportHeight
        && x + width > 0.0f
        && y + height > 0.0f;
    if (visible != 0.0f && buttonId != 0 && plausibleButtonRect && overlapsViewport)
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

bool copyFirstPrintableTileTextReadOnly(
    void* tile,
    char* output,
    size_t outputCapacity,
    UInt32 depth = 0u)
{
    if (!tile || !output || outputCapacity < 2u || depth > 3u)
        return false;

    __try
    {
        auto* values = *reinterpret_cast<TileValue***>(
            reinterpret_cast<std::uintptr_t>(tile) + 0x14);
        const UInt32 valueCount = *reinterpret_cast<UInt32*>(
            reinterpret_cast<std::uintptr_t>(tile) + 0x18);
        if (values && valueCount <= 512u)
        {
            for (UInt32 index = 0u; index < valueCount; ++index)
            {
                if (copyPrintableTileValueStringReadOnly(
                        values[index], output, outputCapacity)
                    && std::strlen(output) >= 2u)
                {
                    return true;
                }
            }
        }

        auto* node = reinterpret_cast<TileListNode*>(
            reinterpret_cast<std::uintptr_t>(tile) + 0x04);
        for (UInt32 count = 0u; node && count < 128u; ++count)
        {
            auto* childNode = static_cast<TileChildNode*>(node->data);
            void* child = childNode ? childNode->child : nullptr;
            if (child && copyFirstPrintableTileTextReadOnly(
                    child, output, outputCapacity, depth + 1u))
            {
                return true;
            }
            node = node->next;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        output[0] = '\0';
    }
    return false;
}

bool tileSubtreeContainsExactTextReadOnly(
    void* tile,
    const char* expected,
    UInt32 depth = 0u)
{
    if (!tile || !expected || !*expected || depth > 4u)
        return false;
    __try
    {
        auto* values = *reinterpret_cast<TileValue***>(
            reinterpret_cast<std::uintptr_t>(tile) + 0x14);
        const UInt32 valueCount = *reinterpret_cast<UInt32*>(
            reinterpret_cast<std::uintptr_t>(tile) + 0x18);
        if (values && valueCount <= 512u)
        {
            for (UInt32 index = 0u; index < valueCount; ++index)
            {
                char text[160] {};
                if (copyPrintableTileValueStringReadOnly(
                        values[index], text, sizeof(text))
                    && _stricmp(text, expected) == 0)
                {
                    return true;
                }
            }
        }
        auto* node = reinterpret_cast<TileListNode*>(
            reinterpret_cast<std::uintptr_t>(tile) + 0x04);
        for (UInt32 count = 0u; node && count < 128u; ++count)
        {
            auto* childNode = static_cast<TileChildNode*>(node->data);
            void* child = childNode ? childNode->child : nullptr;
            if (child && tileSubtreeContainsExactTextReadOnly(
                    child, expected, depth + 1u))
            {
                return true;
            }
            node = node->next;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    return false;
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

    auto buttons = visibleMenuButtons(menu, tileMenu);
    if (menuType == kMenuTypeInventory)
    {
        std::vector<MenuButtonCandidate> inventoryControls;
        inventoryControls.reserve(buttons.size());
        for (const auto& item : buttons)
        {
            char itemLabel[160] {};
            const bool haveItemLabel = copyFirstPrintableTileTextReadOnly(
                item.tile, itemLabel, sizeof(itemLabel));
            const bool inventoryRow = item.buttonId == 255
                && item.x >= 80.0f && item.x <= 400.0f
                && item.y >= 100.0f && item.width >= 100.0f;
            const bool actionButton = item.buttonId == 128;
            const bool textureAsset = haveItemLabel
                && (std::strstr(itemLabel, ".dds") != nullptr
                    || std::strstr(itemLabel, "interface\\") != nullptr
                    || std::strstr(itemLabel, "Interface\\") != nullptr);
            if (haveItemLabel && !textureAsset && (inventoryRow || actionButton))
                inventoryControls.push_back(item);
        }
        if (!inventoryControls.empty())
            buttons = std::move(inventoryControls);
    }
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

bool directMenuClickExactText(const char* expected)
{
    void* tileMenu = nullptr;
    UInt32 menuType = 0;
    void* menu = visibleMenuForInput(&tileMenu, &menuType);
    if (!menu || !expected || !*expected)
        return false;

    const auto buttons = visibleMenuButtons(menu, tileMenu);
    for (const auto& candidate : buttons)
    {
        if (!tileSubtreeContainsExactTextReadOnly(candidate.tile, expected))
            continue;
        logTelemetry(
            "directMenu exactText menu=%p type=0x%lx tile=%p id=%u text=\"%s\"\n",
            menu,
            static_cast<unsigned long>(menuType),
            candidate.tile,
            candidate.buttonId,
            expected);
        return dispatchMenuClick(
            menu,
            candidate.tile,
            candidate.buttonId,
            "exactText");
    }
    logTelemetry(
        "directMenu exactText miss menu=%p type=0x%lx buttons=%zu text=\"%s\"\n",
        menu,
        static_cast<unsigned long>(menuType),
        buttons.size(),
        expected);
    return false;
}

bool equipNativeInventorySelection(const char* source, void* inventoryMenu, void* selectedTile)
{
    __try
    {
        void* selection = nullptr;
        if (inventoryMenu && selectedTile)
        {
            // InventoryMenu::itemList is a ListBox<ItemChange> at +0xB8.
            // The ListBox head stores its ListBoxItem pointer at +4 and the
            // next BSSimpleList node at +8; later nodes store item/next at
            // +0/+4. Resolve the controller-highlighted retail tile back to
            // the exact ItemChange owned by the menu.
            const std::uintptr_t listBox =
                reinterpret_cast<std::uintptr_t>(inventoryMenu) + 0xB8;
            void* listBoxItem = readPointer(listBox + 0x04);
            void* node = readPointer(listBox + 0x08);
            for (UInt32 guard = 0; guard < 512; ++guard)
            {
                if (listBoxItem)
                {
                    void* itemTile = readPointer(
                        reinterpret_cast<std::uintptr_t>(listBoxItem) + 0x00);
                    bool tileMatches = itemTile == selectedTile;
                    void* ancestor = selectedTile;
                    for (UInt32 depth = 0; !tileMatches && ancestor && depth < 16; ++depth)
                    {
                        ancestor = readPointer(
                            reinterpret_cast<std::uintptr_t>(ancestor) + 0x28);
                        tileMatches = ancestor == itemTile;
                    }
                    ancestor = itemTile;
                    for (UInt32 depth = 0; !tileMatches && ancestor && depth < 16; ++depth)
                    {
                        ancestor = readPointer(
                            reinterpret_cast<std::uintptr_t>(ancestor) + 0x28);
                        tileMatches = ancestor == selectedTile;
                    }
                    if (tileMatches)
                    {
                        selection = readPointer(
                            reinterpret_cast<std::uintptr_t>(listBoxItem) + 0x04);
                        break;
                    }
                }
                if (!node)
                    break;
                listBoxItem = readPointer(reinterpret_cast<std::uintptr_t>(node) + 0x00);
                node = readPointer(reinterpret_cast<std::uintptr_t>(node) + 0x04);
            }
        }
        // The global selection can lag one pointer frame behind the tile that
        // received this controller click. Use it only when the exact list row
        // could not be resolved.
        if (!selection)
            selection = readPointer(InventorySelectionAddress);
        void* player = readPointer(PlayerCharacterAddress);
        void* object = selection
            ? readPointer(reinterpret_cast<std::uintptr_t>(selection) + 0x08)
            : nullptr;
        if (!selection || !player || !object)
        {
            logTelemetry(
                "engineInventory equip unavailable source=%s selection=%p player=%p object=%p\n",
                source ? source : "unknown", selection, player, object);
            return false;
        }

        using EquipItemFn = void (__thiscall*)(
            void*, void*, UInt32, void*, UInt32, bool, UInt32);
        pointerFromAddress32<EquipItemFn>(ActorEquipItemAddress)(
            player, object, 1, nullptr, 1, false, 1);
        const UInt32 formId = readUInt32(reinterpret_cast<std::uintptr_t>(object) + 0x0C);
        const UInt8 formType = readUInt8(
            reinterpret_cast<std::uintptr_t>(object) + TESFormTypeIdOffset);
        g_retailRigRediscoveryRequested = true;
        g_retailWeaponRefreshRequested = true;
        logTelemetry(
            "engineInventory equip source=%s selection=%p player=%p object=%p formId=0x%08lx formType=0x%02x fullRigRebind=1 weaponReacquire=1\n",
            source ? source : "unknown",
            selection,
            player,
            object,
            static_cast<unsigned long>(formId),
            static_cast<unsigned int>(formType));
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        logTelemetry("engineInventory equip exception source=%s\n", source ? source : "unknown");
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

    auto buttons = visibleMenuButtons(menu, tileMenu);
    if (menuType == kMenuTypeInventory)
    {
        std::vector<MenuButtonCandidate> inventoryControls;
        inventoryControls.reserve(buttons.size());
        for (const auto& item : buttons)
        {
            char itemLabel[160] {};
            const bool haveItemLabel = copyFirstPrintableTileTextReadOnly(
                item.tile, itemLabel, sizeof(itemLabel));
            const bool inventoryRow = item.buttonId == 255
                && item.x >= 80.0f && item.x <= 400.0f
                && item.y >= 100.0f && item.width >= 100.0f;
            const bool actionButton = item.buttonId == 128;
            const bool textureAsset = haveItemLabel
                && (std::strstr(itemLabel, ".dds") != nullptr
                    || std::strstr(itemLabel, "interface\\") != nullptr
                    || std::strstr(itemLabel, "Interface\\") != nullptr);
            if (haveItemLabel && !textureAsset && (inventoryRow || actionButton))
                inventoryControls.push_back(item);
        }
        if (!inventoryControls.empty())
            buttons = std::move(inventoryControls);
    }
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

    auto buttons = visibleMenuButtons(menu, tileMenu);
    if (menuType == kMenuTypeInventory)
    {
        std::vector<MenuButtonCandidate> inventoryControls;
        inventoryControls.reserve(buttons.size());
        for (const auto& item : buttons)
        {
            char itemLabel[160] {};
            const bool haveItemLabel = copyFirstPrintableTileTextReadOnly(
                item.tile, itemLabel, sizeof(itemLabel));
            const bool inventoryRow = item.buttonId == 255
                && item.x >= 80.0f && item.x <= 400.0f
                && item.y >= 100.0f && item.width >= 100.0f;
            const bool actionButton = item.buttonId == 128;
            const bool textureAsset = haveItemLabel
                && (std::strstr(itemLabel, ".dds") != nullptr
                    || std::strstr(itemLabel, "interface\\") != nullptr
                    || std::strstr(itemLabel, "Interface\\") != nullptr);
            if (haveItemLabel && !textureAsset && (inventoryRow || actionButton))
                inventoryControls.push_back(item);
        }
        if (!inventoryControls.empty())
            buttons = std::move(inventoryControls);
    }
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
        char label[160] {};
        const bool haveLabel = copyFirstPrintableTileTextReadOnly(
            buttons[index].tile, label, sizeof(label));
        logTelemetry(
            "directMenu nav menu=%p type=0x%lx buttons=%zu index=%lu tile=%p id=%u delta=%d rect=(%.1f %.1f %.1f %.1f) label=\"%s\"\n",
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
            buttons[index].height,
            haveLabel ? label : "");
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
    if (g_hasMenuPointer && !updateDirectMenuPointerHover())
    {
        logTelemetry(
            "directMenu accept pointer-miss menu=%p type=0x%lx shared=(%ld,%ld)\n",
            menu,
            static_cast<unsigned long>(menuType),
            g_lastMenuPointerClient.x,
            g_lastMenuPointerClient.y);
        return false;
    }

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

    const UInt64 acceptNowMs = ::GetTickCount64();
    if (g_directMenuLastAcceptTile == candidate->tile
        && acceptNowMs >= g_directMenuLastAcceptMs
        && (acceptNowMs - g_directMenuLastAcceptMs) < 250)
    {
        logTelemetry(
            "directMenu accept deduplicated menu=%p type=0x%lx tile=%p ageMs=%llu\n",
            menu,
            static_cast<unsigned long>(menuType),
            candidate->tile,
            static_cast<unsigned long long>(acceptNowMs - g_directMenuLastAcceptMs));
        return true;
    }
    g_directMenuLastAcceptTile = candidate->tile;
    g_directMenuLastAcceptMs = acceptNowMs;

    char label[160] {};
    const bool haveLabel = copyFirstPrintableTileTextReadOnly(
        candidate->tile, label, sizeof(label));
    logTelemetry(
        "directMenu accept menu=%p type=0x%lx tile=%p id=%u label=\"%s\"\n",
        menu,
        static_cast<unsigned long>(menuType),
        candidate->tile,
        candidate->buttonId,
        haveLabel ? label : "");
    const bool selected = dispatchMenuClick(menu, candidate->tile, candidate->buttonId, "xinput");
    if (!selected)
        return false;

    // A VR controller A press is one complete inventory accept. Resolve the
    // exact clicked ListBox row and invoke the engine equip once. Previously
    // we clicked the menu's Equip control and then invoked Actor::EquipItem a
    // second time, while a stale global selection could point at another row.
    // That produced the observed holster-without-replacement transition.
    if (menuType == kMenuTypeInventory && candidate->buttonId == 255)
    {
        const bool engineEquipped = equipNativeInventorySelection(
            "directMenuAcceptSelection", menu, candidate->tile);
        const bool menuEquipped = !engineEquipped
            && directMenuClickExactText("Equip");
        logTelemetry(
            "directMenu inventory accept label=\"%s\" selected=1 menuEquipped=%u engineEquipped=%u\n",
            haveLabel ? label : "",
            menuEquipped ? 1u : 0u,
            engineEquipped ? 1u : 0u);
        return engineEquipped || menuEquipped;
    }
    return true;
}

bool directMenuCancel(const char* source)
{
    void* tileMenu = nullptr;
    UInt32 menuType = 0;
    void* menu = visibleMenuForInput(&tileMenu, &menuType);
    if (!menu)
        return false;

    if (menuType == kMenuTypeStart && g_console)
    {
        const bool closed = g_console->RunScriptLine2("CloseAllMenus", nullptr, true);
        logTelemetry(
            "directMenu cancel source=%s menu=%p type=0x%lx engineCloseAll=%d\n",
            source ? source : "unknown",
            menu,
            static_cast<unsigned long>(menuType),
            static_cast<int>(closed));
        return closed;
    }

    __try
    {
        auto** vtable = *reinterpret_cast<void***>(menu);
        if (!vtable || !vtable[12])
            return false;
        using HandleKeyboardInputFn = bool (__thiscall*)(void*, UInt32);
        reinterpret_cast<HandleKeyboardInputFn>(vtable[12])(menu, DIK_ESCAPE);
        logTelemetry(
            "directMenu cancel source=%s menu=%p type=0x%lx key=0x%02lx\n",
            source ? source : "unknown",
            menu,
            static_cast<unsigned long>(menuType),
            static_cast<unsigned long>(DIK_ESCAPE));
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        logTelemetry(
            "directMenu cancel exception source=%s menu=%p type=0x%lx\n",
            source ? source : "unknown",
            menu,
            static_cast<unsigned long>(menuType));
        return false;
    }
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

    if (retailSidecarProfile() && g_hasMenuPointer)
    {
        return pointerFallback
            ? dispatchPointerMenuClick()
            : directMenuAcceptSelection();
    }
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
    if (windowsForegroundInputForbidden()
        || !hwnd
        || !envEnabled("FNVXR_POST_MENU_KEYS", false))
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
    if (windowsForegroundInputForbidden()
        || !currentProcessHasForegroundWindow())
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
    if (windowsForegroundInputForbidden() || !hwnd)
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
    if (windowsForegroundInputForbidden())
        return false;
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
    if (windowsForegroundInputForbidden() || !hwnd)
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

bool holdDirectInputKey(
    UInt32 keycode,
    bool held,
    fnvxr::physical_input::LocomotionDelivery delivery =
        fnvxr::physical_input::LocomotionDelivery::SharedInputQueue)
{
    if (keycode >= MaxDirectInputMacros)
        return false;

    if (delivery
        == fnvxr::physical_input::LocomotionDelivery::InProcessNvseDirectInput)
    {
        const bool directDeliveryAlreadyApplied =
            g_physicalLocomotionDirectInputApplied[keycode]
            && g_publishedDirectInputHoldKnown[keycode]
            && g_publishedDirectInputHoldState[keycode] == held
            && (!held || g_publishedDirectInputHoldViaHook[keycode]);
        if (directDeliveryAlreadyApplied)
            return true;

        if (!g_directInputHook)
        {
            if (!g_physicalLocomotionDirectInputUnavailableLogged[keycode])
            {
                g_physicalLocomotionDirectInputUnavailableLogged[keycode] = true;
                logTelemetry(
                    "physicalLocomotionDirectInput key=0x%02lx held=%d finalConsumer=nvse-directinput-hold applied=0 reason=directinput-hook-unavailable\n",
                    static_cast<unsigned long>(keycode),
                    static_cast<int>(held));
            }
            return false;
        }

        g_directInputHook->keys[keycode].hold = held;
        g_publishedDirectInputHoldKnown[keycode] = true;
        g_publishedDirectInputHoldState[keycode] = held;
        g_publishedDirectInputHoldViaHook[keycode] = held;
        g_publishedDirectInputHoldHeartbeatMs[keycode] = 0;
        g_physicalLocomotionDirectInputApplied[keycode] = true;
        g_physicalLocomotionDirectInputUnavailableLogged[keycode] = false;
        logTelemetry(
            "physicalLocomotionDirectInput key=0x%02lx held=%d finalConsumer=nvse-directinput-hold applied=1\n",
            static_cast<unsigned long>(keycode),
            static_cast<int>(held));
        return true;
    }

    if (g_publishedDirectInputHoldKnown[keycode]
        && g_publishedDirectInputHoldState[keycode] == held)
    {
        if (held && !g_publishedDirectInputHoldViaHook[keycode])
        {
            const UInt64 nowMs = GetTickCount64();
            const UInt64 lastHeartbeatMs = g_publishedDirectInputHoldHeartbeatMs[keycode];
            if (lastHeartbeatMs == 0
                || nowMs - lastHeartbeatMs >= DirectInputHoldHeartbeatMilliseconds)
            {
                const bool refreshed = keycode >= MouseButtonOffset
                    && keycode < MouseButtonOffset + 8
                    ? publishInputEvent(
                        fnvxr::shared::InputEventTypeMouseButtonDown,
                        keycode - MouseButtonOffset)
                    : publishInputEvent(fnvxr::shared::InputEventTypeKeyDown, keycode);
                if (!refreshed)
                    return false;
                g_publishedDirectInputHoldHeartbeatMs[keycode] = nowMs;
            }
        }
        return true;
    }
    if (!g_publishedDirectInputHoldKnown[keycode] && !held)
    {
        g_publishedDirectInputHoldKnown[keycode] = true;
        g_publishedDirectInputHoldState[keycode] = false;
        g_publishedDirectInputHoldHeartbeatMs[keycode] = 0;
        g_physicalLocomotionDirectInputApplied[keycode] = false;
        return true;
    }
    if (g_publishedDirectInputHoldViaHook[keycode])
    {
        if (!g_directInputHook)
            return false;
        g_directInputHook->keys[keycode].hold = held;
        g_publishedDirectInputHoldState[keycode] = held;
        g_publishedDirectInputHoldViaHook[keycode] = held;
        g_publishedDirectInputHoldHeartbeatMs[keycode] = 0;
        g_physicalLocomotionDirectInputApplied[keycode] = false;
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
        g_publishedDirectInputHoldHeartbeatMs[keycode] = held ? GetTickCount64() : 0;
        g_physicalLocomotionDirectInputApplied[keycode] = false;
        return true;
    }
    if (releasingQueuedHold)
        return false;
    if (!LegacyInProcessDirectInputHoldFallbackEnabled || !g_directInputHook)
        return false;

    g_directInputHook->keys[keycode].hold = held;
    g_publishedDirectInputHoldKnown[keycode] = true;
    g_publishedDirectInputHoldState[keycode] = held;
    g_publishedDirectInputHoldViaHook[keycode] = held;
    g_publishedDirectInputHoldHeartbeatMs[keycode] = 0;
    g_physicalLocomotionDirectInputApplied[keycode] = false;
    return true;
}

bool holdGameplayMovementKey(UInt32 keycode, bool held)
{
    return holdDirectInputKey(
        keycode,
        held,
        fnvxr::physical_input::selectLocomotionDelivery(
            physicalHeadsetPlayRequested()));
}

using SetPlayerMovementFlagsFn = void (__thiscall*)(void*, UInt32);

void __fastcall hookedPhysicalPlayerMovementFlags(
    void* mover,
    void*,
    UInt32 movementFlags)
{
    const bool allowed =
        g_physicalPlayerMoverAllowed.load(std::memory_order_acquire);
    const UInt32 directions = allowed
        ? g_physicalPlayerMoverDirections.load(std::memory_order_acquire)
        : 0u;
    movementFlags = (movementFlags & ~MovementFlagDirectionalMask)
        | directions;
    if (directions != 0u)
    {
        movementFlags |= MovementFlagIsKeyboard;
        movementFlags &= ~(MovementFlagWalking | MovementFlagRunning);
        movementFlags |= g_physicalPlayerMoverRun.load(
            std::memory_order_acquire)
            ? MovementFlagRunning
            : MovementFlagWalking;
    }

    auto original = reinterpret_cast<SetPlayerMovementFlagsFn>(
        g_physicalPlayerMoverOriginalSetMovementFlags);
    if (original)
        original(mover, movementFlags);
}

bool installPhysicalPlayerMoverHook(void* mover)
{
    if (!mover)
        return false;
    if (g_physicalPlayerMoverHookInstalled)
        return true;

    void** vtable = nullptr;
    void* original = nullptr;
    __try
    {
        vtable = *reinterpret_cast<void***>(mover);
        original = vtable ? vtable[3] : nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        vtable = nullptr;
        original = nullptr;
    }
    if (!vtable
        || original != pointerFromAddress32<void*>(
            ActorMoverSetMovementFlagsAddress))
    {
        logTelemetry(
            "physicalPlayerMoverHook rejected mover=%p vtable=%p slot=%p expected=%p\n",
            mover,
            vtable,
            original,
            pointerFromAddress32<void*>(ActorMoverSetMovementFlagsAddress));
        return false;
    }

    DWORD oldProtect = 0;
    if (!VirtualProtect(
            &vtable[3],
            sizeof(void*),
            PAGE_READWRITE,
            &oldProtect))
    {
        logTelemetry(
            "physicalPlayerMoverHook protect failed err=%lu\n",
            static_cast<unsigned long>(GetLastError()));
        return false;
    }
    g_physicalPlayerMoverOriginalSetMovementFlags = original;
    vtable[3] = reinterpret_cast<void*>(
        hookedPhysicalPlayerMovementFlags);
    DWORD ignored = 0;
    VirtualProtect(&vtable[3], sizeof(void*), oldProtect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), &vtable[3], sizeof(void*));
    g_physicalPlayerMoverVtable = vtable;
    g_physicalPlayerMoverHookInstalled = true;
    logTelemetry(
        "physicalPlayerMoverHook installed mover=%p vtable=%p slot=3 original=%p hook=%p finalConsumer=PlayerMover::SetMovementFlags\n",
        mover,
        vtable,
        original,
        reinterpret_cast<void*>(hookedPhysicalPlayerMovementFlags));
    return true;
}

bool drivePhysicalPlayerMovement(
    const fnvxr::physical_input::LocomotionIntent& intent,
    bool allowed,
    bool runHeld,
    UInt64 frame)
{
    UInt32 requestedDirections = 0u;
    if (allowed)
    {
        requestedDirections =
            (intent.forward ? MovementFlagForward : 0u)
            | (intent.backward ? MovementFlagBackward : 0u)
            | (intent.left ? MovementFlagLeft : 0u)
            | (intent.right ? MovementFlagRight : 0u);
    }
    g_physicalPlayerMoverDirections.store(
        requestedDirections,
        std::memory_order_release);
    g_physicalPlayerMoverRun.store(runHeld, std::memory_order_release);
    g_physicalPlayerMoverAllowed.store(allowed, std::memory_order_release);

    bool applied = false;
    UInt32 before = 0u;
    UInt32 after = 0u;
    void* mover = nullptr;
    __try
    {
        void* player = readPointer(PlayerCharacterAddress);
        mover = player
            ? readPointer(
                reinterpret_cast<std::uintptr_t>(player)
                    + ActorActorMoverOffset)
            : nullptr;
        if (mover)
        {
            const bool hookInstalled = installPhysicalPlayerMoverHook(mover);
            before = *reinterpret_cast<UInt32*>(
                reinterpret_cast<std::uintptr_t>(mover)
                    + PlayerMoverMovementFlagsOffset);
            after = (before & ~MovementFlagDirectionalMask)
                | requestedDirections;
            if (requestedDirections != 0u)
            {
                after |= MovementFlagIsKeyboard;
                after &= ~(MovementFlagWalking | MovementFlagRunning);
                after |= runHeld
                    ? MovementFlagRunning
                    : MovementFlagWalking;
            }
            auto setMovementFlags = hookInstalled && g_physicalPlayerMoverVtable
                ? reinterpret_cast<SetPlayerMovementFlagsFn>(
                    g_physicalPlayerMoverVtable[3])
                : reinterpret_cast<SetPlayerMovementFlagsFn>(
                    ActorMoverSetMovementFlagsAddress);
            setMovementFlags(mover, after);
            const UInt32 verified = *reinterpret_cast<UInt32*>(
                reinterpret_cast<std::uintptr_t>(mover)
                    + PlayerMoverMovementFlagsOffset);
            applied = (verified & MovementFlagDirectionalMask)
                == requestedDirections;
            after = verified;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        applied = false;
    }

    static bool initialized = false;
    static UInt32 previousDirections = 0u;
    static bool previousApplied = false;
    if (!initialized || requestedDirections != previousDirections
        || applied != previousApplied)
    {
        initialized = true;
        previousDirections = requestedDirections;
        previousApplied = applied;
        logTelemetry(
            "physicalPlayerMover frame=%llu requested=0x%02lx before=0x%04lx after=0x%04lx run=%d allowed=%d mover=%p applied=%d finalConsumer=PlayerMover::SetMovementFlags\n",
            static_cast<unsigned long long>(frame),
            static_cast<unsigned long>(requestedDirections),
            static_cast<unsigned long>(before),
            static_cast<unsigned long>(after),
            static_cast<int>(runHeld),
            static_cast<int>(allowed),
            mover,
            static_cast<int>(applied));
    }
    return applied;
}

bool drivePhysicalGameplayPrimaryAttack(
    bool held,
    bool previousHeld,
    UInt64 frame)
{
    bool applied = false;
    void* process = nullptr;
    __try
    {
        void* player = readPointer(PlayerCharacterAddress);
        process = player
            ? readPointer(reinterpret_cast<std::uintptr_t>(player)
                + MobileObjectBaseProcessOffset)
            : nullptr;
        if (process)
        {
            // HighProcess::forceFireWeapon is consumed by Fallout's normal
            // weapon pipeline. Reassert while squeezed because the engine
            // clears the request after each gameplay update.
            *reinterpret_cast<UInt8*>(
                reinterpret_cast<std::uintptr_t>(process)
                + HighProcessForceFireWeaponOffset) = held ? 1u : 0u;
            applied = true;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        applied = false;
    }
    if (held != previousHeld)
    {
        logTelemetry(
            "primaryAttack physical frame=%llu held=%d applied=%d finalConsumer=HighProcess::forceFireWeapon process=%p\n",
            static_cast<unsigned long long>(frame),
            static_cast<int>(held),
            static_cast<int>(applied),
            process);
    }
    return applied;
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
    if (physicalHeadsetPlayRequested()
        || headsetControllerRigVisualTrialRequested())
        return 0.68f;
    return std::clamp(getFloatFromEnv("FNVXR_GAMEPLAY_ANALOG_RUN_THRESHOLD", 0.92f), 0.0f, 1.0f);
}

bool gameplayAnalogRunHeld(float leftThumbX, float leftThumbY)
{
    const float magnitude = (std::max)(std::fabs(leftThumbX), std::fabs(leftThumbY));
    return (physicalHeadsetPlayRequested()
            || headsetControllerRigVisualTrialRequested()
            || envEnabled("FNVXR_GAMEPLAY_ANALOG_RUN_ENABLE", false))
        && magnitude >= gameplayAnalogRunThreshold();
}

bool gameplayAnalogRunHeld(std::int16_t leftThumbX, std::int16_t leftThumbY)
{
    const std::int32_t threshold = static_cast<std::int32_t>(
        gameplayAnalogRunThreshold() * 32767.0f);
    return (physicalHeadsetPlayRequested()
            || headsetControllerRigVisualTrialRequested()
            || envEnabled("FNVXR_GAMEPLAY_ANALOG_RUN_ENABLE", false))
        && fnvxr::physical_input::radialAnalogRunHeld(
            leftThumbX,
            leftThumbY,
            threshold);
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
    holdGameplayMovementKey(DIK_W, false);
    holdGameplayMovementKey(DIK_A, false);
    holdGameplayMovementKey(DIK_S, false);
    holdGameplayMovementKey(DIK_D, false);
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
    const bool analogRun = gameplayAnalogRunHeld(
        pose.leftThumbstickX,
        pose.leftThumbstickY);
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
    // Product input is normalized OpenXR/shared-state input. Do not even
    // enumerate or inspect HWND state when the foreground-input fuse is set;
    // the native menu path below does not require a desktop window.
    if (windowsForegroundInputForbidden())
        return nullptr;
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

    const int sharedWidth = getIntFromEnv("FNVXR_UI_SHARED_WIDTH", SharedVideoPointerWidth);
    const int sharedHeight = getIntFromEnv("FNVXR_UI_SHARED_HEIGHT", SharedVideoPointerHeight);
    const int inputWidth = getIntFromEnv(
        "FNVXR_UI_INPUT_WIDTH", SharedVideoPointerWidth);
    const int inputHeight = getIntFromEnv(
        "FNVXR_UI_INPUT_HEIGHT", SharedVideoPointerHeight);
    HWND hwnd = gameWindow();
    int width = inputWidth;
    int height = inputHeight;
    if (hwnd)
    {
        RECT client {};
        if (!GetClientRect(hwnd, &client))
        {
            g_latestPointerValid.store(false);
            return;
        }
        width = client.right - client.left;
        height = client.bottom - client.top;
    }
    if (width <= 0 || height <= 0
        || sharedWidth <= 0 || sharedHeight <= 0
        || inputWidth <= 0 || inputHeight <= 0)
    {
        g_latestPointerValid.store(false);
        return;
    }

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
    if (hwnd
        && !windowsForegroundInputForbidden()
        && envEnabled("FNVXR_CURSOR_TRACK_POINTER", false))
    {
        const POINT windowPointer = mapSharedPointerToWindow(
            hwnd,
            g_lastMenuPointerClient);
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

    const bool windowInputFused = windowsForegroundInputForbidden();
    HWND hwnd = g_hasMenuPointer ? gameWindow() : nullptr;
    const bool focusedForClick = windowInputFused
        ? false
        : hwnd
        ? ensureClickForeground(hwnd)
        : currentProcessHasForegroundWindow();
    const bool foregroundObserved = windowInputFused
        ? false : currentProcessHasForegroundWindow();
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
            static_cast<int>(foregroundObserved));
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
        static_cast<int>(foregroundObserved));

    if (g_hasMenuPointer && allowLegacyFallback)
    {
        HWND hwnd = gameWindow();
        if (!hwnd)
            return;

        const bool diMouseTap = tapDirectInputKey(MouseButtonOffset);

        const POINT windowPointer = mapSharedPointerToWindow(hwnd, g_lastMenuPointerClient);
        BOOL postDown = FALSE;
        BOOL postUp = FALSE;
        if (!windowsForegroundInputForbidden()
            && envEnabled("FNVXR_POST_WINDOW_MOUSE_FALLBACK", false))
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
        if (!windowsForegroundInputForbidden()
            && envEnabled("FNVXR_CURSOR_TRACK_POINTER", false)
            && ClientToScreen(hwnd, &screenPoint))
        {
            if (envEnabled("FNVXR_CURSOR_FOCUS", false))
                SetForegroundWindow(hwnd);
            SetCursorPos(screenPoint.x, screenPoint.y);
        }

        if (!windowsForegroundInputForbidden()
            && envEnabled("FNVXR_PLUGIN_SENDINPUT_CLICK", false)
            && currentProcessHasForegroundWindow())
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

    if (windowsForegroundInputForbidden()
        || !currentProcessHasForegroundWindow())
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
    if (windowsForegroundInputForbidden()
        || !envEnabled("FNVXR_IMMEDIATE_OS_CLICK", false))
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

void recoverFocusLossPause(UInt64 frame, UInt32 menuBits, RuntimePhase phase)
{
    static bool initialized = false;
    static bool previousInteractive = false;
    static RuntimePhase previousPhase = RuntimePhase::Unknown;
    static UInt32 previousMenuBits = fnvxr::shared::RuntimeBlockingMenuBits;
    static bool focusLossArmed = false;
    static UInt64 lastCloseAttemptFrame = 0;
    static UInt32 ownedFixtureCloseAttempts = 0u;

    // The general visual trial is publication-only. The one exception is an
    // already-completed, owned headset-demo fixture: retail itself creates a
    // StartMenu pause as soon as its hidden process loses foreground after a
    // verified load. This path never repairs focus or sends input; it can
    // issue one fixed in-engine CloseAllMenus command only after the exact
    // gameplay-to-StartMenu transition has been observed.
    const bool ownedHeadsetFixtureRecovery =
        headsetDemoFixtureProfileSelected()
        && retailFixtureAutomationRequested()
        && g_headsetDemoFixtureReady;
    if ((stereoVisualTrialProfileSelected() && !ownedHeadsetFixtureRecovery)
        || (!retailSidecarProfile() && !ownedHeadsetFixtureRecovery)
        || !envEnabled("FNVXR_CLOSE_FOCUS_LOSS_PAUSE", true))
        return;

    const bool foreground = currentProcessHasForegroundWindow();
    const bool active = currentProcessHasActiveWindow();
    const bool interactive = foreground || active;
    if (!initialized)
    {
        initialized = true;
        previousInteractive = interactive;
        previousPhase = phase;
        previousMenuBits = menuBits;
        return;
    }

    const bool cleanPreviousGameplay =
        previousPhase == RuntimePhase::Gameplay
        && (previousMenuBits & fnvxr::shared::RuntimeBlockingMenuBits) == 0u;
    constexpr UInt32 nonStartBlockingBits =
        fnvxr::shared::RuntimeBlockingMenuBits
        & ~fnvxr::shared::RuntimeStartMenuBit;
    const bool focusPauseVisible =
        (menuBits & fnvxr::shared::RuntimeStartMenuBit) != 0u
        && (menuBits & nonStartBlockingBits) == 0u;
    const bool observedForegroundLoss =
        previousInteractive && !interactive && cleanPreviousGameplay;
    const bool observedOwnedFixturePause =
        ownedHeadsetFixtureRecovery
        && cleanPreviousGameplay
        && focusPauseVisible;
    if (observedForegroundLoss || observedOwnedFixturePause)
    {
        focusLossArmed = true;
        lastCloseAttemptFrame = 0;
        logTelemetry(
            "focusLossPause armed frame=%llu previousBits=0x%02lx ownedFixture=%d\n",
            static_cast<unsigned long long>(frame),
            static_cast<unsigned long>(previousMenuBits),
            static_cast<int>(ownedHeadsetFixtureRecovery));
    }

    if (focusLossArmed
        && focusPauseVisible
        && (!ownedHeadsetFixtureRecovery || ownedFixtureCloseAttempts == 0u)
        && (lastCloseAttemptFrame == 0 || frame >= lastCloseAttemptFrame + 30))
    {
        lastCloseAttemptFrame = frame;
        if (ownedHeadsetFixtureRecovery)
            ++ownedFixtureCloseAttempts;
        const bool closed = runPluginConsoleCommand(
            "fnvxrFocusLossPauseRecovery",
            "CloseAllMenus");
        logTelemetry(
            "focusLossPause close frame=%llu bits=0x%02lx foreground=%d active=%d ownedFixture=%d ok=%d\n",
            static_cast<unsigned long long>(frame),
            static_cast<unsigned long>(menuBits),
            static_cast<int>(foreground),
            static_cast<int>(active),
            static_cast<int>(ownedHeadsetFixtureRecovery),
            static_cast<int>(closed));
        if (closed)
            focusLossArmed = false;
    }

    // A normal foreground gameplay frame closes the recovery window. Menus
    // opened deliberately while Fallout already has focus never arm it.
    if (interactive && phase == RuntimePhase::Gameplay)
        focusLossArmed = false;

    previousInteractive = interactive;
    previousPhase = phase;
    previousMenuBits = menuBits;
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

bool publishSharedCommandStatus(
    UInt32 requestId,
    UInt32 status,
    UInt64 frame,
    UInt32 resultCode,
    const char* command)
{
    if (!g_commandState)
        return false;

    if (!acquireSharedCommandWriter())
        return false;
    repairSharedCommandState();
    const UInt32 currentStatus = g_commandState->status;
    const bool validTransition = g_commandState->requestId == requestId
        && ((status == fnvxr::shared::CommandStatusRunning
                && currentStatus == fnvxr::shared::CommandStatusPending)
            || ((status == fnvxr::shared::CommandStatusSucceeded
                    || status == fnvxr::shared::CommandStatusFailed)
                && currentStatus == fnvxr::shared::CommandStatusRunning));
    if (!validTransition
        || !fnvxr::shared::beginSequencedSharedWrite(g_commandState->sequence))
    {
        releaseSharedCommandWriter();
        return false;
    }
    g_commandState->magic = CommandSharedMagic;
    g_commandState->version = CommandSharedVersion;
    g_commandState->status = status;
    g_commandState->completedFrame = frame;
    g_commandState->resultCode = resultCode;
    if (command && *command)
        strcpy_s(g_commandState->lastCommand, command);
    fnvxr::shared::endSequencedSharedWrite(g_commandState->sequence);
    releaseSharedCommandWriter();
    return true;
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
    if (!publishSharedCommandStatus(
        request.requestId,
        fnvxr::shared::CommandStatusRunning,
        frame,
        built ? 0u : 1u,
        built ? command : ""))
    {
        logTelemetry(
            "shared command request lost ownership before running request=%lu\n",
            static_cast<unsigned long>(request.requestId));
        return;
    }

    const bool ok = built && runPluginConsoleCommand("fnvxrSharedCommand", command);
    g_lastCommandRequestId = request.requestId;
    const bool completionPublished = publishSharedCommandStatus(
        request.requestId,
        ok ? fnvxr::shared::CommandStatusSucceeded : fnvxr::shared::CommandStatusFailed,
        frame,
        ok ? 0u : 2u,
        built ? command : "");
    if (!completionPublished)
        logTelemetry("shared command completion ownership lost request=%lu\n",
            static_cast<unsigned long>(request.requestId));
    logTelemetry(
        "{\"event\":\"fnvxrSharedCommandComplete\",\"requestId\":%lu,\"command\":\"%s\",\"ok\":%s,\"resultCode\":%lu,\"line\":\"%s\",\"frame\":%llu}\n",
        static_cast<unsigned long>(request.requestId),
        sharedCommandName(request.command),
        ok ? "true" : "false",
        static_cast<unsigned long>(ok ? 0u : 2u),
        built ? command : "",
        static_cast<unsigned long long>(frame));
}

bool desktopAssistRecoveryLoadCommandIsExact(const SharedCommandState& request)
{
    return request.command == fnvxr::shared::CommandTypeConsole
        // The mailbox is cross-process storage.  Bound this comparison by its
        // fixed field size so a malformed, unterminated request cannot make
        // the in-game consumer read beyond the snapshot while deciding
        // whether it is the sole permitted automation action.
        && std::strncmp(
            request.saveName,
            DesktopAssistRecoveryLoadCommand,
            sizeof(request.saveName)) == 0;
}

void consumeDesktopAssistRecoveryLoad(
    UInt64 frame,
    RuntimePhase phase,
    UInt32 menuBits,
    bool uiInputAllowed)
{
    if (!desktopAssistAutomationRequested())
        return;

    // One game process gets one recovery-load attempt.  A later mailbox
    // request, even if it repeats the exact text, cannot reload the save or
    // become a general desktop-assist command channel.
    static bool recoveryLoadAlreadySubmitted = false;

    SharedCommandState request {};
    if (!readSharedCommandSnapshot(request))
        return;
    if (request.requestId == 0 || request.requestId == g_lastCommandRequestId)
        return;
    if (request.status != fnvxr::shared::CommandStatusPending)
    {
        logTelemetry(
            "{\"event\":\"fnvxrDesktopAssistAutomationSkip\",\"requestId\":%lu,\"status\":\"%s\",\"frame\":%llu}\n",
            static_cast<unsigned long>(request.requestId),
            sharedCommandStatusName(request.status),
            static_cast<unsigned long long>(frame));
        g_lastCommandRequestId = request.requestId;
        return;
    }

    const bool exactRecoveryLoad = desktopAssistRecoveryLoadCommandIsExact(request);
    const bool startMenu = phase == RuntimePhase::Menu
        && (menuBits & fnvxr::shared::RuntimeStartMenuBit) != 0u
        && uiInputAllowed;
    const bool authorized = exactRecoveryLoad
        && startMenu
        && !recoveryLoadAlreadySubmitted
        && fnvxr::engine::desktopAssistAutomationAuthorized(
            desktopAssistCameraRequest(),
            desktopAssistAutomationRequested(),
            fnvxr::engine::DesktopAssistAutomationAction::LoadFixedRecoverySave);
    logTelemetry(
        "{\"event\":\"fnvxrDesktopAssistAutomationRequest\",\"requestId\":%lu,\"exactRecoveryLoad\":%s,\"startMenu\":%s,\"authorized\":%s,\"frame\":%llu}\n",
        static_cast<unsigned long>(request.requestId),
        exactRecoveryLoad ? "true" : "false",
        startMenu ? "true" : "false",
        authorized ? "true" : "false",
        static_cast<unsigned long long>(frame));
    if (!publishSharedCommandStatus(
            request.requestId,
            fnvxr::shared::CommandStatusRunning,
            frame,
            authorized ? 0u : ERROR_ACCESS_DENIED,
            authorized ? DesktopAssistRecoveryLoadCommand : ""))
    {
        logTelemetry(
            "desktopAssist recovery load lost ownership before running request=%lu\n",
            static_cast<unsigned long>(request.requestId));
        return;
    }

    const bool ok = authorized && runPluginConsoleCommand(
        "fnvxrDesktopAssistRecoveryLoad",
        DesktopAssistRecoveryLoadCommand);
    if (authorized)
        recoveryLoadAlreadySubmitted = true;
    g_lastCommandRequestId = request.requestId;
    const bool completionPublished = publishSharedCommandStatus(
        request.requestId,
        ok ? fnvxr::shared::CommandStatusSucceeded : fnvxr::shared::CommandStatusFailed,
        frame,
        ok ? 0u : ERROR_ACCESS_DENIED,
        authorized ? DesktopAssistRecoveryLoadCommand : "");
    if (!completionPublished)
        logTelemetry(
            "desktopAssist recovery load completion ownership lost request=%lu\n",
            static_cast<unsigned long>(request.requestId));
    logTelemetry(
        "{\"event\":\"fnvxrDesktopAssistAutomationComplete\",\"requestId\":%lu,\"ok\":%s,\"resultCode\":%lu,\"frame\":%llu}\n",
        static_cast<unsigned long>(request.requestId),
        ok ? "true" : "false",
        static_cast<unsigned long>(ok ? 0u : ERROR_ACCESS_DENIED),
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

void consumeExternalDInputBridge(
    const RuntimeObservation& observation)
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

    const fnvxr::shared::RuntimeControllerMode controllerMode =
        fnvxr::shared::runtimeControllerMode(
            static_cast<UInt32>(observation.phase),
            observation.menuBits,
            observation.showroomActive ? 1u : 0u,
            observation.cameraActive,
            observation.frame != 0u);
    const bool active =
        controllerMode == fnvxr::shared::RuntimeControllerMode::Ui
        && state.pointerActive != 0;
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

void syncExternalDInputPointer(
    const RuntimeObservation& observation)
{
    SharedDInputState state {};
    if (!readSharedDInputSnapshot(state))
        return;

    const fnvxr::shared::RuntimeControllerMode controllerMode =
        fnvxr::shared::runtimeControllerMode(
            static_cast<UInt32>(observation.phase),
            observation.menuBits,
            observation.showroomActive ? 1u : 0u,
            observation.cameraActive,
            observation.frame != 0u);
    const bool active =
        controllerMode == fnvxr::shared::RuntimeControllerMode::Ui
        && state.pointerActive != 0;
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
        if (!windowsForegroundInputForbidden())
        {
            PostMessageA(
                hwnd,
                WM_MOUSEMOVE,
                0,
                MAKELPARAM(static_cast<WORD>(windowPointer.x), static_cast<WORD>(windowPointer.y)));
        }
        updateGameCursorTile(hwnd);
    }
    if (directUiClickEnabled() || pointerTileFallbackEnabled())
        updateDirectMenuPointerHover();

    if (g_loggedExternalDInputPointers < 48 || (frame % 120) == 0)
    {
        ++g_loggedExternalDInputPointers;
        logTelemetry(
            "externalDInput pointer active=1 raw=(%ld,%ld) mapped=(%ld,%ld) frame=%lu hwnd=%p controllerMode=%s menuBits=0x%02lx pipboy=%d\n",
            clientX,
            clientY,
            windowPointer.x,
            windowPointer.y,
            static_cast<unsigned long>(frame),
            hwnd,
            fnvxr::shared::runtimeControllerModeName(controllerMode),
            static_cast<unsigned long>(observation.menuBits),
            static_cast<int>(
                (observation.menuBits
                    & fnvxr::shared::RuntimePipBoyMenuBit) != 0u));
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

void tapExternalXInputNav(UInt32 navMask, UInt32 menuBits)
{
    // Navigate the live menu tiles directly. This is the durable headless VR
    // path as well as the normal windowed path; it does not require a desktop
    // keyboard device or foreground-window acquisition.
    const bool directUiClick = directUiClickEnabled();
    const bool pipBoyVisible =
        (menuBits & fnvxr::shared::RuntimePipBoyMenuBit) != 0u;
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
    g_physicalSnapTurnLatch.reset();
    setGameplayRunMode(false, "externalXInput:release", 0);
    setGameplayAutoRun(false, "externalXInput:release", 0);
    holdDirectInputKey(MouseButtonOffset, false);
    if (physicalHeadsetPlayRequested())
        drivePhysicalGameplayPrimaryAttack(false, false, 0);
    holdDirectInputKey(MouseButtonOffset + 1, false);
    holdDirectInputKey(DIK_R, false);
    holdGameplayGrab(false);
    holdGameplayRunModifier(false);
    holdGameplayMovementKey(DIK_W, false);
    holdGameplayMovementKey(DIK_A, false);
    holdGameplayMovementKey(DIK_S, false);
    holdGameplayMovementKey(DIK_D, false);
    holdDirectInputKey(DIK_LEFT, false);
    holdDirectInputKey(DIK_RIGHT, false);
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

bool driveGameplayPrimaryAttack(
    bool rightTriggerHeld,
    PrimaryAttackState& state,
    UInt64 frame,
    const char* source,
    fnvxr::physical_input::LocomotionDelivery delivery =
        fnvxr::physical_input::LocomotionDelivery::SharedInputQueue)
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
        const bool applied = holdDirectInputKey(
            MouseButtonOffset,
            rightTriggerHeld,
            delivery);
        if (changed)
        {
            logTelemetry(
                "primaryAttack hold frame=%llu source=%s held=%d applied=%d finalConsumer=%s class=%s formId=0x%08lx slot=%lu\n",
                static_cast<unsigned long long>(frame),
                source ? source : "unknown",
                static_cast<int>(rightTriggerHeld),
                static_cast<int>(applied),
                delivery == fnvxr::physical_input::LocomotionDelivery::InProcessNvseDirectInput
                    ? "nvse-directinput-hold"
                    : "shared-input-queue",
                weaponClassName(weaponClass),
                static_cast<unsigned long>(g_lastKnownWeaponFormId),
                static_cast<unsigned long>(g_lastKnownWeaponFavoriteSlot));
        }
        return changed && applied;
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

bool externalLeftGripHeld()
{
    if (!externalDInputSharedReady())
        return false;

    const float threshold = std::clamp(
        getFloatFromEnv("FNVXR_PIPBOY_GRIP_THRESHOLD", 0.55f),
        0.0f,
        1.0f);
    return externalDInputLeftGrip() > sharedStickValue(threshold);
}

bool externalLeftGripPipBoyHeld()
{
    if (!envEnabled("FNVXR_LEFT_GRIP_PIPBOY_MODE", false) || !externalDInputSharedReady())
        return false;

    return externalLeftGripHeld();
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

bool applyControllerSnapTurn(
    std::int16_t rightThumbX,
    UInt64 frame,
    const char* source)
{
    constexpr std::int32_t pressThreshold = 22000;
    constexpr std::int32_t releaseThreshold = 9000;
    const int snapDirection = g_physicalSnapTurnLatch.update(
        rightThumbX,
        pressThreshold,
        releaseThreshold);
    if (snapDirection == 0)
        return false;

    void* player = readPointer(PlayerCharacterAddress);
    const float currentYawRadians = player
        ? readFloat(
            reinterpret_cast<std::uintptr_t>(player)
                + TESObjectRefrRotationZOffset,
            std::numeric_limits<float>::quiet_NaN())
        : std::numeric_limits<float>::quiet_NaN();
    constexpr float degrees = 30.0f;
    bool applied = false;
    float targetDegrees = 0.0f;
    if (std::isfinite(currentYawRadians))
    {
        constexpr float RadiansToDegrees = 57.29577951308232f;
        targetDegrees = std::fmod(
            currentYawRadians * RadiansToDegrees
                + static_cast<float>(snapDirection) * degrees
                + 360.0f,
            360.0f);
        char command[96] {};
        sprintf_s(command, "player.setangle z %.6f", targetDegrees);
        applied = runPluginConsoleCommand("fnvxrControllerSnapTurn", command);
    }
    logTelemetry(
        "controllerSnapTurn frame=%llu source=%s direction=%d degrees=%.3f targetDegrees=%.3f stickX=%d applied=%d consumer=player.setangle-z\n",
        static_cast<unsigned long long>(frame),
        source ? source : "unknown",
        snapDirection,
        degrees,
        targetDegrees,
        static_cast<int>(rightThumbX),
        static_cast<int>(applied));
    return applied;
}

void applyHeadRelativeLocomotion(SharedXInputState& state, UInt64 frame)
{
    if (!g_haveRetailRigOrigin
        || (state.leftThumbX == 0 && state.leftThumbY == 0))
    {
        return;
    }
    VrRigPoseSnapshot pose {};
    if (!readLatestRetailRigPose(pose)
        || (pose.trackingFlags & fnvxr::shared::VrPoseTrackingHmd) == 0)
    {
        return;
    }
    const Quat relativeHead = multiplyQuat(
        conjugateQuat(g_retailRigOriginHmdRot),
        pose.hmdRot);
    const Quat headYaw = gravityAlignedYawQuat(relativeHead);
    const float yaw = 2.0f * std::atan2(headYaw.y, headYaw.w);
    const float cosine = std::cos(yaw);
    const float sine = std::sin(yaw);
    const float inputX = static_cast<float>(state.leftThumbX);
    const float inputY = static_cast<float>(state.leftThumbY);
    const auto clampAxis = [](float value) -> std::int16_t {
        return static_cast<std::int16_t>(std::lround(std::clamp(
            value,
            -32767.0f,
            32767.0f)));
    };
    const std::int16_t rotatedX = clampAxis(
        inputX * cosine + inputY * sine);
    const std::int16_t rotatedY = clampAxis(
        inputY * cosine - inputX * sine);
    static std::int16_t previousRawX = 0;
    static std::int16_t previousRawY = 0;
    static std::int16_t previousRotatedX = 0;
    static std::int16_t previousRotatedY = 0;
    if (state.leftThumbX != previousRawX
        || state.leftThumbY != previousRawY
        || rotatedX != previousRotatedX
        || rotatedY != previousRotatedY)
    {
        logTelemetry(
            "headRelativeLocomotion frame=%llu yawDegrees=%.3f raw=(%d,%d) rotated=(%d,%d)\n",
            static_cast<unsigned long long>(frame),
            yaw * 57.29577951308232f,
            static_cast<int>(state.leftThumbX),
            static_cast<int>(state.leftThumbY),
            static_cast<int>(rotatedX),
            static_cast<int>(rotatedY));
        previousRawX = state.leftThumbX;
        previousRawY = state.leftThumbY;
        previousRotatedX = rotatedX;
        previousRotatedY = rotatedY;
    }
    state.leftThumbX = rotatedX;
    state.leftThumbY = rotatedY;
}

void consumeExternalXInputGameplayControls(
    const SharedXInputState& state,
    const SharedDInputState& hostInput,
    bool physicalLocomotionAllowed,
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
    const UInt64 frame = hostInput.frame != 0u
        ? hostInput.frame
        : externalDInputFrame();
    const bool rightTriggerHeld = state.rightTrigger > triggerThreshold;
    const bool leftTriggerHeld = state.leftTrigger > triggerThreshold;
    const bool analogRun = gameplayAnalogRunHeld(
        state.leftThumbX,
        state.leftThumbY);
    const bool keyboardMovement = pluginKeyboardMovementEnabled();
    const bool keyboardGameplayFallback = pluginGameplayKeyboardFallbackEnabled();
    const bool physicalLocomotionRoute = physicalHeadsetPlayRequested();
    const int movementDeadzone = std::clamp(
        getIntFromEnv("FNVXR_PLUGIN_MOVEMENT_DEADZONE", 9000),
        1000,
        30000);
    const auto requestedLocomotion =
        fnvxr::physical_input::classifyLocomotion(
            state.leftThumbX,
            state.leftThumbY,
            movementDeadzone);
    const bool locomotionKeyboardMovement =
        keyboardMovement
        && (!physicalLocomotionRoute || physicalLocomotionAllowed);
    const bool moveLeft = locomotionKeyboardMovement && requestedLocomotion.left;
    const bool moveRight = locomotionKeyboardMovement && requestedLocomotion.right;
    const bool moveBackward = locomotionKeyboardMovement && requestedLocomotion.backward;
    const bool moveForward = locomotionKeyboardMovement && requestedLocomotion.forward;
    const bool snapTurnEnabled = physicalLocomotionRoute;
    const bool keyTurnEnabled =
        keyboardMovement
        && !snapTurnEnabled
        && envEnabled("FNVXR_RIGHT_STICK_KEY_TURN", true);
    const bool turnLeft =
        keyTurnEnabled && state.rightThumbX < -movementDeadzone;
    const bool turnRight =
        keyTurnEnabled && state.rightThumbX > movementDeadzone;
    if (snapTurnEnabled)
        applyControllerSnapTurn(state.rightThumbX, frame, "physical:right-stick");
    const bool thirdPersonChordHeld =
        thirdPersonL3ControlsEnabled()
        && (state.buttons & XInputLeftThumb) != 0;
    const bool weaponOrbitHeld =
        (state.reserved[fnvxr::shared::XInputReservedInteractionFlags]
            & fnvxr::PoseInteractionWeaponOrbitActive) != 0u;
    const bool autoRunPressed = !weaponOrbitHeld
        && (pressed & XInputRightThumb) != 0;
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
    const bool pipBoyMenuChordPressed =
        keyboardGameplayFallback
        && !leftTriggerHeld
        && externalLeftGripHeld()
        && (pressed & XInputBack) != 0
        && (g_lastPipBoyMenuChordMs == 0
            || GetTickCount64() >= g_lastPipBoyMenuChordMs + 750)
        && envEnabled("FNVXR_PIPBOY_MENU_CHORD_ENABLE", true);
    const bool physicalPipBoyMenuPressed =
        keyboardGameplayFallback
        && !leftTriggerHeld
        && physicalLeftMenuPipBoyEnabled()
        && (pressed & XInputBack) != 0;
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
        && !leftTriggerHeld
        && !weaponOrbitHeld;
    const bool thirdPersonZoomStep =
        updateThirdPersonL3Control(thirdPersonChordHeld, state.rightThumbY, frame, "externalXInput:L3");
    if (autoRunPressed)
        toggleGameplayAutoRun("externalXInput:R3", frame);
    const bool runHeld =
        locomotionKeyboardMovement
        &&
        envEnabled("FNVXR_GAMEPLAY_RUN_BUTTON_ENABLE", true)
        && (g_gameplayRunModeEnabled || analogRun);
    const bool playerMoverApplied = physicalLocomotionRoute
        ? drivePhysicalPlayerMovement(
            requestedLocomotion,
            physicalLocomotionAllowed,
            runHeld,
            frame)
        : false;

    const bool primaryAttackStep = physicalLocomotionRoute
        ? drivePhysicalGameplayPrimaryAttack(
            rightTriggerHeld,
            previousRightTriggerHeld,
            frame)
        : driveGameplayPrimaryAttack(
            rightTriggerHeld,
            primaryAttackState,
            frame,
            "externalXInput:RT");
    holdDirectInputKey(MouseButtonOffset + 1, leftTriggerHeld && !suppressAimMouseForCombatChord && !vatsChordHeld);
    holdDirectInputKey(DIK_R, reloadHeld);
    holdGameplayGrab(rightGripGrabHeld);
    holdGameplayRunModifier(runHeld);
    const bool finalForwardHeld =
        locomotionKeyboardMovement
        && (moveForward || g_gameplayAutoRunEnabled);
    const bool forwardApplied =
        holdGameplayMovementKey(DIK_W, finalForwardHeld);
    const bool leftApplied = holdGameplayMovementKey(DIK_A, moveLeft);
    const bool backwardApplied = holdGameplayMovementKey(DIK_S, moveBackward);
    const bool rightApplied = holdGameplayMovementKey(DIK_D, moveRight);
    holdDirectInputKey(DIK_LEFT, turnLeft);
    holdDirectInputKey(DIK_RIGHT, turnRight);

    if (physicalLocomotionRoute)
    {
        const UInt8 requestedMask = static_cast<UInt8>(
            (requestedLocomotion.forward ? 0x1u : 0u)
            | (requestedLocomotion.backward ? 0x2u : 0u)
            | (requestedLocomotion.left ? 0x4u : 0u)
            | (requestedLocomotion.right ? 0x8u : 0u));
        const UInt8 generatedMask = static_cast<UInt8>(
            (finalForwardHeld ? 0x1u : 0u)
            | (moveBackward ? 0x2u : 0u)
            | (moveLeft ? 0x4u : 0u)
            | (moveRight ? 0x8u : 0u));
        const bool finalApplied = playerMoverApplied;
        static bool physicalLocomotionFinalTelemetryInitialized = false;
        static UInt8 previousPhysicalLocomotionRequestedMask = 0u;
        static UInt8 previousPhysicalLocomotionGeneratedMask = 0u;
        static bool previousPhysicalLocomotionApplied = false;
        if (!physicalLocomotionFinalTelemetryInitialized
            || requestedMask != previousPhysicalLocomotionRequestedMask
            || generatedMask != previousPhysicalLocomotionGeneratedMask
            || finalApplied != previousPhysicalLocomotionApplied)
        {
            physicalLocomotionFinalTelemetryInitialized = true;
            previousPhysicalLocomotionRequestedMask = requestedMask;
            previousPhysicalLocomotionGeneratedMask = generatedMask;
            previousPhysicalLocomotionApplied = finalApplied;
            logTelemetry(
                "physicalLocomotionFinal runtimeFrame=%llu hostFrame=%lu sourcePacket=%lu effectivePacket=%lu sharedLs=(%d,%d) hostLs=(%ld,%ld) requested=0x%02x generated=0x%02x authority=%d finalConsumer=%s finalApplied=%d\n",
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long>(hostInput.frame),
                static_cast<unsigned long>(g_lastExternalXInputSourcePacket),
                static_cast<unsigned long>(state.packet),
                static_cast<int>(state.leftThumbX),
                static_cast<int>(state.leftThumbY),
                static_cast<long>(hostInput.leftStickX),
                static_cast<long>(hostInput.leftStickY),
                static_cast<unsigned int>(requestedMask),
                static_cast<unsigned int>(generatedMask),
                static_cast<int>(physicalLocomotionAllowed),
                "PlayerMover::SetMovementFlags",
                static_cast<int>(finalApplied));
        }
    }

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
        const bool pipBoyItemsChord =
            !leftTriggerHeld
            && externalLeftGripHeld()
            && envEnabled("FNVXR_PIPBOY_MENU_CHORD_ENABLE", true);
        if (pipBoyItemsChord)
        {
            g_lastPipBoyMenuChordMs = GetTickCount64();
            const bool opened = openEnginePipBoyInventory(
                "externalXInput:LG+RightMenu",
                frame);
            logTelemetry(
                "pipboyItems fire frame=%llu source=externalXInput:LG+RightMenu key=0x%02lx gameplay=1 engineOpened=%d\n",
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long>(DIK_F2),
                static_cast<int>(opened));
        }
        else
        {
            tapDirectInputKey(DIK_ESCAPE);
            logTelemetry("menuStart fire frame=%llu source=externalXInput:Start gameplay=1\n", static_cast<unsigned long long>(frame));
        }
    }
    if (waitChordPressed)
    {
        tapWaitKey("externalXInput:LT+Back", frame);
    }
    else if (physicalPipBoyMenuPressed)
    {
        const bool opened = openEnginePipBoyInventory(
            "externalXInput:LeftMenu",
            frame);
        logTelemetry(
            "pipboyOpen fire frame=%llu source=externalXInput:LeftMenu gameplay=1 engineOpened=%d\n",
            static_cast<unsigned long long>(frame),
            static_cast<int>(opened));
    }
    else if (pipBoyMenuChordPressed)
    {
        g_lastPipBoyMenuChordMs = GetTickCount64();
        tapDirectInputKey(DIK_TAB);
        logTelemetry(
            "pipboyToggle fire frame=%llu source=externalXInput:LG+LeftMenu key=0x%02lx gameplay=1\n",
            static_cast<unsigned long long>(frame),
            static_cast<unsigned long>(DIK_TAB));
    }
    else if (keyboardGameplayFallback
        && (pressed & XInputBack)
        && (g_lastPipBoyMenuChordMs == 0
            || GetTickCount64() >= g_lastPipBoyMenuChordMs + 2000))
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
            "externalXInput gameplay buttons=0x%04x pressed=0x%04x rt=%u lt=%u rtMouse=%d ltMouse=%d aimMouseSuppressed=%d vatsChord=%d vatsPressed=%d waitPressed=%d reloadHeld=%d grabHeld=%d grabKey=0x%02lx runHeld=%d runKey=0x%02lx moveMode=%u walk=%d runMode=%d thirdPersonChord=%d combatChord=%d protectedCombatChord=%d combatPressed=0x%04x weaponClass=%s weaponFormId=0x%08lx weaponSlot=%lu analogRun=%d autoRun=%d autoToggle=%d keyboardMovement=%d keyboardFallback=%d move=%d%d%d%d turn=%d%d bJump=%d ySneak=%d rs=%d\n",
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
            static_cast<int>(moveForward),
            static_cast<int>(moveLeft),
            static_cast<int>(moveBackward),
            static_cast<int>(moveRight),
            static_cast<int>(turnLeft),
            static_cast<int>(turnRight),
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

void consumeExternalXInputBridge(
    const RuntimeObservation& observation)
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
    static bool physicalLocomotionRearmPending = true;
    static bool physicalLocomotionAuthorityTelemetryInitialized = false;
    static bool previousPhysicalLocomotionAuthority = false;
    static bool previousPhysicalLocomotionRearmPending = true;
    static UInt8 previousPhysicalLocomotionIntentMask = 0u;
    static fnvxr::shared::RuntimeControllerMode previousControllerMode =
        fnvxr::shared::RuntimeControllerMode::Unknown;

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
        physicalLocomotionRearmPending = true;
        physicalLocomotionAuthorityTelemetryInitialized = false;
        previousControllerMode =
            fnvxr::shared::RuntimeControllerMode::Unknown;
        return;
    }

    if (physicalHeadsetPlayRequested())
        applyHeadRelativeLocomotion(state, observation.frame);

    const UInt32 menuBits = observation.menuBits;
    const fnvxr::shared::RuntimeControllerMode controllerMode =
        fnvxr::shared::runtimeControllerMode(
            static_cast<UInt32>(observation.phase),
            observation.menuBits,
            observation.showroomActive ? 1u : 0u,
            observation.cameraActive,
            observation.frame != 0u);
    SharedDInputState hostInput {};
    const bool hostInputAvailable = readSharedDInputSnapshot(hostInput);
    const bool physicalLocomotionRoute = physicalHeadsetPlayRequested();
    const bool hostGameplayControlsActive =
        hostInputAvailable && hostInput.gameplayControlsActive != 0u;
    const bool hostMenuInputActive =
        hostInputAvailable && hostInput.menuInputActive != 0u;
    const int physicalMovementDeadzone = std::clamp(
        getIntFromEnv("FNVXR_PLUGIN_MOVEMENT_DEADZONE", 9000),
        1000,
        30000);
    const auto physicalLocomotionIntent =
        fnvxr::physical_input::classifyLocomotion(
            state.leftThumbX,
            state.leftThumbY,
            physicalMovementDeadzone);
    const UInt8 physicalLocomotionIntentMask = static_cast<UInt8>(
        (physicalLocomotionIntent.forward ? 0x1u : 0u)
        | (physicalLocomotionIntent.backward ? 0x2u : 0u)
        | (physicalLocomotionIntent.left ? 0x4u : 0u)
        | (physicalLocomotionIntent.right ? 0x8u : 0u));
    const bool physicalLocomotionAuthority =
        physicalLocomotionRoute
        && controllerMode == fnvxr::shared::RuntimeControllerMode::Gameplay
        && hostGameplayControlsActive;
    if (physicalLocomotionRoute)
    {
        if (!physicalLocomotionAuthority)
        {
            physicalLocomotionRearmPending = true;
        }
        else if (physicalLocomotionRearmPending
            && !physicalLocomotionIntent.any())
        {
            // A held stick must never restart movement after a menu, focus,
            // or runtime-authority transition. Require neutral, then a new
            // press before the physical W/A/S/D holds can return.
            physicalLocomotionRearmPending = false;
        }
    }
    else
    {
        physicalLocomotionRearmPending = false;
        physicalLocomotionAuthorityTelemetryInitialized = false;
    }
    const bool physicalLocomotionAllowed =
        !physicalLocomotionRoute
        || (physicalLocomotionAuthority && !physicalLocomotionRearmPending);
    const bool uiInputAllowed =
        controllerMode == fnvxr::shared::RuntimeControllerMode::Ui;
    const bool gameplayInputAllowed =
        controllerMode == fnvxr::shared::RuntimeControllerMode::Gameplay;
    const bool gripXboxMode = state.leftTrigger > 180;
    const bool leftTriggerModifierHeld = state.leftTrigger > 64;
    const bool inputAllowed = uiInputAllowed || gameplayInputAllowed;
    const bool pipBoyMenuVisible = pipBoyVisibleFromMenuBits(menuBits);
    const bool startMenuVisible = (menuBits & (1u << 1)) != 0;
    const bool menuKeyboardFallback = pluginMenuKeyboardFallbackEnabled();
    if (physicalLocomotionRoute
        && (!physicalLocomotionAuthorityTelemetryInitialized
            || physicalLocomotionAuthority
                != previousPhysicalLocomotionAuthority
            || physicalLocomotionRearmPending
                != previousPhysicalLocomotionRearmPending
            || physicalLocomotionIntentMask
                != previousPhysicalLocomotionIntentMask))
    {
        physicalLocomotionAuthorityTelemetryInitialized = true;
        previousPhysicalLocomotionAuthority = physicalLocomotionAuthority;
        previousPhysicalLocomotionRearmPending =
            physicalLocomotionRearmPending;
        previousPhysicalLocomotionIntentMask = physicalLocomotionIntentMask;
        logTelemetry(
            "physicalLocomotionConsumerAuthority runtimeFrame=%llu hostFrame=%lu sourcePacket=%lu effectivePacket=%lu sharedLs=(%d,%d) hostLs=(%ld,%ld) hostInput=%d hostGameplay=%d hostMenu=%d localMode=%s authorized=%d rearmPending=%d releaseMovement=%d\n",
            static_cast<unsigned long long>(observation.frame),
            static_cast<unsigned long>(hostInput.frame),
            static_cast<unsigned long>(g_lastExternalXInputSourcePacket),
            static_cast<unsigned long>(state.packet),
            static_cast<int>(state.leftThumbX),
            static_cast<int>(state.leftThumbY),
            static_cast<long>(hostInput.leftStickX),
            static_cast<long>(hostInput.leftStickY),
            static_cast<int>(hostInputAvailable),
            static_cast<int>(hostGameplayControlsActive),
            static_cast<int>(hostMenuInputActive),
            fnvxr::shared::runtimeControllerModeName(controllerMode),
            static_cast<int>(physicalLocomotionAuthority),
            static_cast<int>(physicalLocomotionRearmPending),
            static_cast<int>(!physicalLocomotionAllowed));
    }
    if (controllerMode != previousControllerMode)
    {
        updateExternalPipBoyGripMode(false, previousPipBoyGripHeld);
        updateExternalRightGripMenuMode(
            false,
            previousRightGripMenuHeld);
        g_lastExternalXInputButtons = state.buttons;
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
        logTelemetry(
            "controller mode transition frame=%llu from=%s to=%s phase=%lu menuBits=0x%02lx camera=%d showroom=%d releaseBeforePress=1\n",
            static_cast<unsigned long long>(observation.frame),
            fnvxr::shared::runtimeControllerModeName(
                previousControllerMode),
            fnvxr::shared::runtimeControllerModeName(controllerMode),
            static_cast<unsigned long>(observation.phase),
            static_cast<unsigned long>(observation.menuBits),
            static_cast<int>(observation.cameraActive),
            static_cast<int>(observation.showroomActive));
        previousControllerMode = controllerMode;
    }
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
    const bool pipBoyMenuChordPressed =
        externalLeftGripHeld()
        && (pressed & XInputBack) != 0
        && (g_lastPipBoyMenuChordMs == 0
            || GetTickCount64() >= g_lastPipBoyMenuChordMs + 750)
        && envEnabled("FNVXR_PIPBOY_MENU_CHORD_ENABLE", true);
    const bool physicalPipBoyMenuPressed =
        physicalLeftMenuPipBoyEnabled()
        && (pressed & XInputBack) != 0;
    const UInt32 navMask = uiInputAllowed ? externalXInputNavMask(state, menuBits) : 0;
    const UInt64 nowMs = GetTickCount64();
    const bool navChanged = navMask != 0 && navMask != g_lastExternalXInputNavMask;
    const bool navRepeat = navMask != 0 && nowMs >= g_lastExternalXInputNavMs + 140;

    if (menuKeyboardFallback && (navChanged || navRepeat))
    {
        tapExternalXInputNav(navMask, menuBits);
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

    const bool livePipBoyRightTriggerPressed =
        state.rightTrigger > 180 && !previousRightTriggerHeld;
    updateLivePipBoyInteraction(
        state,
        menuBits,
        livePipBoyRightTriggerPressed);

    if (gameplayInputAllowed && !uiInputAllowed)
    {
        consumeExternalXInputGameplayControls(
            state,
            hostInput,
            physicalLocomotionAllowed,
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
            const bool handled = directUiClickEnabled() && directMenuAcceptSelection();
            logTelemetry(
                "uiButton fire source=A action=accept pointerOnly=1 pipBoy=%d menuBits=0x%02lx handled=%d\n",
                static_cast<int>(pipBoyMenuVisible),
                static_cast<unsigned long>(menuBits),
                static_cast<int>(handled));
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
        UInt32 topMenuType = 0;
        visibleMenuForInput(nullptr, &topMenuType);
        const bool handled = (topMenuType == kMenuTypeStart
                && directMenuCancel("externalXInput:B:start"))
            || (pipBoyMenuVisible
                && closeEnginePipBoy("externalXInput:B", externalDInputFrame()))
            || directMenuCancel("externalXInput:B:fallback");
        if (!handled)
        {
            tapDirectInputKey(backKey);
            if (HWND hwnd = gameWindow())
                postMenuKey(hwnd, uiBackVirtualKeyForMenu(menuBits));
        }
        logTelemetry(
            "uiButton fire source=B action=back key=0x%02lx pipBoy=%d menuBits=0x%02lx handled=%d\n",
            static_cast<unsigned long>(backKey),
            static_cast<int>(pipBoyMenuVisible),
            static_cast<unsigned long>(menuBits),
            static_cast<int>(handled));
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
        const bool handled = uiInputAllowed
            && !pipBoyMenuVisible
            && directUiClickEnabled()
            && directMenuAcceptSelection();
        if (!handled)
            tapDirectInputKey(uiInputAllowed ? DIK_RETURN : uiBackKeyForMenu(menuBits));
        if (!handled)
        {
            if (HWND hwnd = gameWindow())
                postMenuKey(hwnd, uiInputAllowed ? VK_RETURN : uiBackVirtualKeyForMenu(menuBits));
        }
    }
    if (menuKeyboardFallback && uiInputAllowed && physicalPipBoyMenuPressed)
    {
        const bool handled = pipBoyMenuVisible
            && closeEnginePipBoy("externalXInput:LeftMenu", externalDInputFrame());
        if (!handled)
            tapDirectInputKey(DIK_ESCAPE);
        logTelemetry(
            "pipboyClose fire frame=%llu source=externalXInput:LeftMenu ui=1 pipBoy=%d handled=%d\n",
            static_cast<unsigned long long>(externalDInputFrame()),
            static_cast<int>(pipBoyMenuVisible),
            static_cast<int>(handled));
    }
    else if (menuKeyboardFallback && uiInputAllowed && pipBoyMenuChordPressed)
    {
        g_lastPipBoyMenuChordMs = GetTickCount64();
        tapDirectInputKey(DIK_TAB);
        logTelemetry(
            "pipboyToggle fire frame=%llu source=externalXInput:LG+LeftMenu key=0x%02lx ui=1 menuBits=0x%02lx\n",
            static_cast<unsigned long long>(externalDInputFrame()),
            static_cast<unsigned long>(DIK_TAB),
            static_cast<unsigned long>(menuBits));
    }
    else if (menuKeyboardFallback
        && uiInputAllowed
        && (pressed & XInputBack)
        && (g_lastPipBoyMenuChordMs == 0
            || GetTickCount64() >= g_lastPipBoyMenuChordMs + 2000))
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

bool startBridge();

bool ensureAuthorizedRuntimeObservationStarted()
{
    const bool fixedCommandAutomationRequested =
        desktopAssistAutomationRequested()
        || stereoVisualTrialAutomationRequested()
        || retailFixtureAutomationRequested();
    if (g_authorizedRuntimeObservationStarted)
    {
        return gamePluginProducerLeaseHeldByCurrentThread()
            && g_runtimeState
            && (!readOnlyFirstPersonSemanticsRequested() || g_playerState)
            && (!fixedCommandAutomationRequested || g_commandState);
    }

    ++g_retailObservationAuthorityAttempts;
    const auto proof = fnvxr::engine::compatibility::
        proveCurrentRetailCompatibilityAtDecisionPoint();
    if (!fnvxr::engine::retailObservationAuthorized(proof))
    {
        if (g_retailObservationAuthorityAttempts <= 12u
            || (g_retailObservationAuthorityAttempts % 300u) == 0u)
        {
            logTelemetry(
                "runtime observation authority deferred attempt=%lu failure=%u compatible=%d evidence=%d%d%d%d%d%d%d%d%d%d%d renderFirstPersonStage=%u jipTarget=0x%08lX/0x%08lX johnnyTarget=0x%08lX/0x%08lX; no game state read performed\n",
                static_cast<unsigned long>(
                    g_retailObservationAuthorityAttempts),
                static_cast<unsigned>(proof.failure),
                static_cast<int>(proof.compatible),
                static_cast<int>(proof.evidence.retailExecutableIdentityMatched),
                static_cast<int>(proof.evidence.moduleSnapshotStable),
                static_cast<int>(proof.evidence.jip5730ExactOrAbsent),
                static_cast<int>(proof.evidence.johnnyGuitar528ExactOrAbsent),
                static_cast<int>(proof.evidence.showOff184ExactOrAbsent),
                static_cast<int>(proof.evidence.renderFirstPersonStockOrJipNormalized),
                static_cast<int>(proof.evidence.protectedCoreBodiesMatched),
                static_cast<int>(proof.evidence.protectedFunctionInventoryMatched),
                static_cast<int>(proof.evidence.protectedVtableSlotsMatched),
                static_cast<int>(proof.evidence.protectedVtableBlocksMatched),
                static_cast<int>(proof.evidence.synchronousSameProcess),
                static_cast<unsigned>(
                    proof.diagnostics.renderFirstPersonProofStage),
                static_cast<unsigned long>(
                    proof.diagnostics.observedJipRenderFirstPersonTarget),
                static_cast<unsigned long>(
                    proof.diagnostics.expectedJipRenderFirstPersonTarget),
                static_cast<unsigned long>(
                    proof.diagnostics
                        .observedJohnnyGuitarRenderFirstPersonTarget),
                static_cast<unsigned long>(
                    proof.diagnostics
                        .expectedJohnnyGuitarRenderFirstPersonTarget));
        }
        return false;
    }
    if (!acquireGamePluginProducerLease())
        return false;
    initSharedRuntime();
    if (readOnlyFirstPersonSemanticsRequested())
        initSharedPlayer();
    if (headsetDemoUiProfileSelected())
        initSharedInputEvents();
    // Mapping this mailbox is not general command authority.  Its only
    // consumers are the separately opted-in, fixed-command automation gates.
    // The publication-only visual-trial consumer is reached later with the
    // just-published runtime observation and never starts the full bridge.
    if (fixedCommandAutomationRequested)
        initSharedCommand();
    g_authorizedRuntimeObservationStarted = g_runtimeState != nullptr;
    if (readOnlyFirstPersonSemanticsRequested())
    {
        g_authorizedRuntimeObservationStarted =
            g_authorizedRuntimeObservationStarted && g_playerState != nullptr;
    }
    if (headsetDemoUiProfileSelected())
    {
        g_authorizedRuntimeObservationStarted =
            g_authorizedRuntimeObservationStarted
            && g_inputEvents != nullptr
            && g_inputEventWriterMutex != nullptr;
    }
    if (fixedCommandAutomationRequested)
        g_authorizedRuntimeObservationStarted = g_authorizedRuntimeObservationStarted
            && g_commandState != nullptr;
    if (g_authorizedRuntimeObservationStarted)
    {
        logTelemetry(
            "runtime observation ready under exact same-process compatibility authority attempt=%lu\n",
            static_cast<unsigned long>(g_retailObservationAuthorityAttempts));
    }
    return g_authorizedRuntimeObservationStarted;
}

RuntimeObservation observeAndPublishRuntime()
{
    RuntimeObservation observation {};
    observation.frame = ++g_runtimeObservationFrame;
    observation.menuBits = currentMenuBits();
    observation.phase = runtimePhaseFromMenuBits(observation.menuBits);
    observation.uiInputAllowed =
        uiInputAllowedFromMenuBits(observation.menuBits);
    observation.showroomActive = g_showroomActive;
    const bool visualTrialPublicationOnly =
        stereoVisualTrialMainLoopDisposition()
        == fnvxr::engine::RetailPluginMainLoopDisposition::
            PublishRuntimeOnly;
    // The owned fixture has the same read-only runtime-publication boundary as
    // the visual trial, but is independently enabled and never starts a
    // bridge. Its lifecycle needs a real current camera observation to prove
    // it reached a scene before mutating its owned save.
    const bool readOnlyCameraPublicationAuthorized =
        visualTrialPublicationOnly || retailFixtureAutomationRequested();
    const bool currentCameraObjectObserved =
        readOnlyCameraPublicationAuthorized
        && cameraAllowedForMenuBits(observation.menuBits)
        && activeGameCameraObject() != nullptr;
    observation.cameraActive =
        fnvxr::engine::retailRuntimeCameraActive(
            g_cameraState && g_cameraState->active != 0u,
            readOnlyCameraPublicationAuthorized,
            currentCameraObjectObserved);
    publishRuntimeState(
        observation.frame,
        observation.menuBits,
        observation.phase,
        observation.uiInputAllowed,
        observation.cameraActive);

    static UInt32 previousMenuBits = 0xffffffffu;
    if (observation.menuBits != previousMenuBits
        || observation.frame <= 10u
        || (observation.frame % 300u) == 0u)
    {
        previousMenuBits = observation.menuBits;
        logTelemetry(
            "runtime observation frame=%llu bits=0x%02X phase=%lu ui=%d camera=%d source=compatibility-authorized-mainloop\n",
            static_cast<unsigned long long>(observation.frame),
            observation.menuBits,
            static_cast<unsigned long>(observation.phase),
            static_cast<int>(observation.uiInputAllowed),
            static_cast<int>(observation.cameraActive));
    }
    return observation;
}

bool stereoVisualTrialRecoveryLoadCommandIsExact(
    const SharedCommandState& request,
    const fnvxr::engine::stereo_visual_trial_automation::
        ApprovedRetailSave& selectedRetailSave)
{
    namespace automation =
        fnvxr::engine::stereo_visual_trial_automation;
    static_assert(
        automation::FreshCharacterLoadCommand.size() + 1u
            <= sizeof(request.saveName),
        "verified Goodsprings visual-trial load command no longer fits the mailbox");
    const std::string_view fixedCommand = selectedRetailSave.loadCommand;
    return request.command == fnvxr::shared::CommandTypeConsole
        && std::memcmp(
            request.saveName,
            fixedCommand.data(),
            fixedCommand.size()) == 0
        && request.saveName[fixedCommand.size()] == '\0';
}

bool stereoVisualTrialFreshCharacterCommandIsExact(
    const SharedCommandState& request)
{
    namespace automation =
        fnvxr::engine::stereo_visual_trial_automation;
    static_assert(
        automation::FreshCharacterStartCommand.size() + 1u
            <= sizeof(request.saveName),
        "fresh-character visual-trial COC command no longer fits the mailbox");
    return request.command == fnvxr::shared::CommandTypeConsole
        && std::memcmp(
            request.saveName,
            automation::FreshCharacterStartCommand.data(),
            automation::FreshCharacterStartCommand.size()) == 0
        && request.saveName[
            automation::FreshCharacterStartCommand.size()] == '\0';
}

bool realStereoVisualTrialStartMenuState(
    const RuntimeObservation& observation)
{
    return observation.frame != 0u
        && observation.phase == RuntimePhase::Menu
        && observation.uiInputAllowed
        && !g_showroomActive
        && (observation.menuBits
                & fnvxr::shared::RuntimeBlockingMenuBits)
            == fnvxr::shared::RuntimeStartMenuBit;
}

bool realStereoVisualTrialFreshGameplayState(
    const RuntimeObservation& observation)
{
    // Retail can retain MenuMode alone for a frame after a load. It is not an
    // actionable or blocking menu (and uiInputAllowed remains false), so it
    // must not invalidate an otherwise verified gameplay camera observation.
    return observation.frame != 0u
        && observation.phase == RuntimePhase::Gameplay
        && !observation.uiInputAllowed
        && observation.cameraActive
        && !g_showroomActive
        && (observation.menuBits
                & fnvxr::shared::RuntimeBlockingMenuBits)
            == 0u;
}

// The fixture uses the same concrete proof as the isolated runtime
// publication path: no UI, gameplay phase, and a currently observed retail
// camera. It does not activate or steer that camera.
bool realRetailFixtureFreshGameplayState(
    const RuntimeObservation& observation)
{
    return observation.frame != 0u
        && observation.phase == RuntimePhase::Gameplay
        && !observation.uiInputAllowed
        && observation.cameraActive
        && !g_showroomActive
        && (observation.menuBits
                & fnvxr::shared::RuntimeBlockingMenuBits)
            == 0u;
}

// The world-only fixture finisher lives before the general fixture lifecycle
// implementation below, so keep this exact owned-name builder visible here.
bool buildRetailFixtureNamedCommand(
    std::string_view prefix,
    const fnvxr::engine::retail_fixture_automation::Plan& plan,
    char* output,
    std::size_t outputSize);

// Set only after the exact stock MessageMenu handler has returned. Both the
// fixed-close fallback and the one-time fixture finalizer use this to avoid
// immediately competing with the native acknowledgement in the same frame.
UInt64 g_lastExactOfficialPackNativeAcknowledgementFrame = 0u;
// Updated for every fully re-proven stock DLC notice, including a notice
// whose native acknowledgement happens to fail closed. The final owned save
// must occur after this notice drain rather than before it.
UInt64 g_lastExactOfficialPackObservationFrame = 0u;

enum class HeadsetWorldOnlyFixtureWeaponDrawStage : UInt8
{
    AwaitingFixture = 0u,
    AwaitingDrawResult,
    AwaitingFinalSaveSettle,
    Complete,
};

// This intentionally sits outside the fixture creation lifecycle below.  A
// loaded owned fixture may still have its stock DLC notice state pending, so
// this one-time finisher drains known stock notices, makes the already-equipped
// named weapon visible through JIP's fixed SetWeaponOut command, verifies it,
// then saves that clean weapon-out state back to the exact owned fixture name.
// It never sends desktop, keyboard, mouse, controller, or simulator input, and
// it never retries either command.
void processHeadsetWorldOnlyFixtureWeaponDraw(
    const RuntimeObservation& observation)
{
    namespace fixture = fnvxr::engine::retail_fixture_automation;
    if (!headsetWorldOnlyFixtureWeaponDrawRequested())
        return;

    static HeadsetWorldOnlyFixtureWeaponDrawStage stage =
        HeadsetWorldOnlyFixtureWeaponDrawStage::AwaitingFixture;
    static UInt64 firstGameplayFrame = 0u;
    static UInt64 quietGameplaySinceFrame = 0u;
    static UInt64 saveFrame = 0u;
    static UInt64 drawFrame = 0u;
    static UInt64 weaponOutStableSinceFrame = 0u;
    static bool weaponWasAlreadyOut = false;
    static bool loggedInvalidPlan = false;
    if (stage == HeadsetWorldOnlyFixtureWeaponDrawStage::Complete)
        return;

    RetailFixtureAutomationPlan fixturePlan {};
    if (!readRetailFixtureAutomationPlan(fixturePlan)
        || !fixture::authorized(fixturePlan.plan))
    {
        if (!loggedInvalidPlan)
        {
            loggedInvalidPlan = true;
            logTelemetry(
                "{\"event\":\"fnvxrHeadsetFixtureWeaponDrawBlocked\",\"reason\":\"invalid-owned-fixture-plan\",\"frame\":%llu}\n",
                static_cast<unsigned long long>(observation.frame));
        }
        stage = HeadsetWorldOnlyFixtureWeaponDrawStage::Complete;
        return;
    }

    void* player = readPointer(PlayerCharacterAddress);
    void* playerProcess = player
        ? readPointer(
            reinterpret_cast<std::uintptr_t>(player)
            + MobileObjectBaseProcessOffset)
        : nullptr;
    const bool playerProcessAvailable = player != nullptr && playerProcess != nullptr;
    const bool gameplay = realRetailFixtureFreshGameplayState(observation);
    const bool weaponOut = playerProcessAvailable && playerWeaponOut();
    const fixture::WeaponCommand* const selectedWeapon =
        fixture::findWeapon(fixturePlan.plan.weapon);
    const char* const weaponToken = selectedWeapon
        ? selectedWeapon->token.data()
        : "invalid";

    if (!gameplay)
    {
        quietGameplaySinceFrame = 0u;
        return;
    }
    if (firstGameplayFrame == 0u)
    {
        firstGameplayFrame = observation.frame;
    }
    if (quietGameplaySinceFrame == 0u)
    {
        quietGameplaySinceFrame = observation.frame;
    }

    if (stage == HeadsetWorldOnlyFixtureWeaponDrawStage::AwaitingFixture)
    {
        // Gun Runners' Arsenal and the pre-order notices may be scheduled
        // after the first apparently clean gameplay frame. Leave a bounded
        // initial drain window, then require both a menu-free gameplay window
        // and a grace interval after the last exact known notice before we
        // change or save the owned fixture.
        // A freshly created fixture receives the full stock-notice drain.
        // A previously finalized, weapon-out owned fixture has already saved
        // past those notices, so it only needs a short menu-free re-entry
        // check before the demo can start again.
        constexpr UInt64 FixtureInitialOfficialNoticeDrainFrames = 900u;
        constexpr UInt64 FixturePersistedCleanSaveDrainFrames = 120u;
        constexpr UInt64 FixturePopupSettlingFrames = 180u;
        constexpr UInt64 FixtureQuietGameplayFrames = 180u;
        constexpr UInt64 FixturePersistedCleanSaveQuietGameplayFrames = 60u;
        const UInt64 latestKnownNoticeFrame =
            g_lastExactOfficialPackObservationFrame
                > g_lastExactOfficialPackNativeAcknowledgementFrame
            ? g_lastExactOfficialPackObservationFrame
            : g_lastExactOfficialPackNativeAcknowledgementFrame;
        const bool knownNoticeStillSettling =
            latestKnownNoticeFrame != 0u
            && observation.frame
                >= latestKnownNoticeFrame
            && observation.frame
                - latestKnownNoticeFrame
                < FixturePopupSettlingFrames;
        const bool persistedCleanWeaponOut = weaponOut
            && latestKnownNoticeFrame == 0u;
        const UInt64 requiredInitialNoticeDrainFrames =
            persistedCleanWeaponOut
                ? FixturePersistedCleanSaveDrainFrames
                : FixtureInitialOfficialNoticeDrainFrames;
        const UInt64 requiredQuietGameplayFrames =
            persistedCleanWeaponOut
                ? FixturePersistedCleanSaveQuietGameplayFrames
                : FixtureQuietGameplayFrames;
        const bool initialNoticeDrainIncomplete =
            observation.frame < firstGameplayFrame
            || observation.frame - firstGameplayFrame
                < requiredInitialNoticeDrainFrames;
        if (initialNoticeDrainIncomplete
            || knownNoticeStillSettling
            || observation.frame < quietGameplaySinceFrame
            || observation.frame - quietGameplaySinceFrame
                < requiredQuietGameplayFrames)
        {
            return;
        }

        // A previous owned run may already have persisted this exact weapon
        // state. Preserve it rather than issuing SetWeaponOut again, because
        // that command would holster the weapon.
        if (weaponOut)
        {
            weaponWasAlreadyOut = true;
            drawFrame = observation.frame;
            stage = HeadsetWorldOnlyFixtureWeaponDrawStage::AwaitingDrawResult;
            return;
        }

        if (!fixture::headsetWorldOnlyFixtureWeaponDrawAuthorized(
                fixturePlan.plan,
                true,
                g_headsetDemoFixtureReady,
                gameplay,
                playerProcessAvailable,
                false))
        {
            return;
        }

        weaponWasAlreadyOut = false;
        const bool submitted = runPluginConsoleCommand(
            "fnvxrHeadsetFixtureWeaponDraw",
            fixture::SetFixtureWeaponOutCommand.data());
        logTelemetry(
            "{\"event\":\"fnvxrHeadsetFixtureWeaponDraw\",\"action\":\"set-weapon-out\",\"source\":\"JIP\",\"weapon\":\"%s\",\"submitted\":%s,\"weaponOutBefore\":false,\"frame\":%llu}\n",
            weaponToken,
            submitted ? "true" : "false",
            static_cast<unsigned long long>(observation.frame));
        if (!submitted)
        {
            logTelemetry(
                "{\"event\":\"fnvxrHeadsetFixtureWeaponDrawBlocked\",\"reason\":\"jip-set-weapon-out-failed\",\"weapon\":\"%s\",\"frame\":%llu}\n",
                weaponToken,
                static_cast<unsigned long long>(observation.frame));
            stage = HeadsetWorldOnlyFixtureWeaponDrawStage::Complete;
            return;
        }

        drawFrame = observation.frame;
        stage = HeadsetWorldOnlyFixtureWeaponDrawStage::AwaitingDrawResult;
        return;
    }

    if (stage == HeadsetWorldOnlyFixtureWeaponDrawStage::AwaitingDrawResult)
    {
        constexpr UInt64 DrawResultSettlingFrames = 60u;
        constexpr UInt64 DrawResultTimeoutFrames = 600u;
        constexpr UInt64 WeaponOutStableFrames = 30u;
        if (observation.frame <= drawFrame
            || observation.frame - drawFrame < DrawResultSettlingFrames)
        {
            return;
        }
        const bool drawSucceeded = playerProcessAvailable && playerWeaponOut();
        if (!drawSucceeded)
        {
            weaponOutStableSinceFrame = 0u;
            if (observation.frame >= drawFrame
                && observation.frame - drawFrame < DrawResultTimeoutFrames)
            {
                return;
            }
            logTelemetry(
                "{\"event\":\"fnvxrHeadsetFixtureWeaponDrawResult\",\"weapon\":\"%s\",\"success\":false,\"alreadyReady\":%s,\"weaponOut\":false,\"saveFrame\":0,\"issuedFrame\":%llu,\"frame\":%llu}\n",
                weaponToken,
                weaponWasAlreadyOut ? "true" : "false",
                static_cast<unsigned long long>(drawFrame),
                static_cast<unsigned long long>(observation.frame));
            stage = HeadsetWorldOnlyFixtureWeaponDrawStage::Complete;
            return;
        }

        // SetWeaponOut changes animation state asynchronously.  Do not persist
        // the first true sample: require a continuous weapon-out window so a
        // one-frame process flag cannot produce a save that reloads holstered.
        if (weaponOutStableSinceFrame == 0u)
            weaponOutStableSinceFrame = observation.frame;
        if (observation.frame < weaponOutStableSinceFrame
            || observation.frame - weaponOutStableSinceFrame
                < WeaponOutStableFrames)
        {
            return;
        }

        if (!fixture::headsetWorldOnlyFixturePreparationSaveAuthorized(
                fixturePlan.plan,
                true,
                g_headsetDemoFixtureReady,
                gameplay,
                playerProcessAvailable,
                false))
        {
            return;
        }

        char saveCommand[64] {};
        const bool commandBuilt = buildRetailFixtureNamedCommand(
            fixture::SaveCommandPrefix,
            fixturePlan.plan,
            saveCommand,
            sizeof(saveCommand));
        const bool submitted = commandBuilt && runPluginConsoleCommand(
            "fnvxrHeadsetFixtureFinalizeSave",
            saveCommand);
        logTelemetry(
            "{\"event\":\"fnvxrHeadsetFixtureFinalizeSave\",\"saveName\":\"%s\",\"weapon\":\"%s\",\"weaponOut\":true,\"commandBuilt\":%s,\"submitted\":%s,\"frame\":%llu}\n",
            fixturePlan.saveName,
            weaponToken,
            commandBuilt ? "true" : "false",
            submitted ? "true" : "false",
            static_cast<unsigned long long>(observation.frame));
        if (!submitted)
        {
            logTelemetry(
                "{\"event\":\"fnvxrHeadsetFixtureWeaponDrawBlocked\",\"reason\":\"owned-fixture-finalize-save-failed\",\"weapon\":\"%s\",\"frame\":%llu}\n",
                weaponToken,
                static_cast<unsigned long long>(observation.frame));
            stage = HeadsetWorldOnlyFixtureWeaponDrawStage::Complete;
            return;
        }

        saveFrame = observation.frame;
        stage = HeadsetWorldOnlyFixtureWeaponDrawStage::AwaitingFinalSaveSettle;
        return;
    }

    constexpr UInt64 FixtureSaveSettlingFrames = 60u;
    if (!playerProcessAvailable
        || observation.frame <= saveFrame
        || observation.frame - saveFrame < FixtureSaveSettlingFrames)
    {
        return;
    }
    const bool drawSucceeded = playerWeaponOut();
    logTelemetry(
        "{\"event\":\"fnvxrHeadsetFixtureWeaponDrawResult\",\"weapon\":\"%s\",\"success\":%s,\"alreadyReady\":%s,\"weaponOut\":%s,\"saveFrame\":%llu,\"issuedFrame\":%llu,\"frame\":%llu}\n",
        weaponToken,
        drawSucceeded ? "true" : "false",
        weaponWasAlreadyOut ? "true" : "false",
        drawSucceeded ? "true" : "false",
        static_cast<unsigned long long>(saveFrame),
        static_cast<unsigned long long>(drawFrame),
        static_cast<unsigned long long>(observation.frame));
    stage = HeadsetWorldOnlyFixtureWeaponDrawStage::Complete;
}

// The retail GUI normally assigns these two fields before dispatching a menu
// click. In the headless visual-trial run there is deliberately no desktop
// cursor, keyboard, controller, or simulator input to make that assignment.
// Arm only the exact, already-verified first button of one official DLC notice
// so the stock MessageMenu handler receives the same in-game selection state.
bool armExactOfficialPackMessageMenuSelection(void* menu, void* okTile)
{
    if (!menu || !okTile)
        return false;

    void* interfaceManager = readPointer(InterfaceManagerAddress);
    if (!interfaceManager)
        return false;

    bool armed = false;
    __try
    {
        auto** activeTile = reinterpret_cast<void**>(
            reinterpret_cast<std::uintptr_t>(interfaceManager)
            + InterfaceManagerActiveTileOffset);
        auto** activeMenu = reinterpret_cast<void**>(
            reinterpret_cast<std::uintptr_t>(interfaceManager)
            + InterfaceManagerActiveMenuOffset);
        if (!activeTile || !activeMenu)
            return false;

        // Do not synthesize a device event. These are the stock in-game
        // selection fields that identify the exact native tile and its menu.
        *activeTile = okTile;
        *activeMenu = menu;
        armed = true;
        logTelemetry(
            "fnvxrStereoVisualTrialOfficialPackNativeSelection menu=%p tile=%p\n",
            menu,
            okTile);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        armed = false;
    }
    return armed;
}

void acknowledgeExactOfficialPackMessageMenu(
    const RuntimeObservation& observation)
{
    namespace automation = fnvxr::engine::stereo_visual_trial_automation;
    static UInt32 acknowledgedOfficialPackNotificationMask = 0u;
    const bool explicitlyOptedIn =
        exactOfficialPackAcknowledgementRequested();
    if (!explicitlyOptedIn)
        return;

    void* menu = nullptr;
    void* okTile = nullptr;
    UInt32 officialPackNotificationMask = 0u;
    bool exactlyOneFirstButtonOk = false;
    const bool expectedMessageMenuState = observation.frame != 0u
        && observation.phase == RuntimePhase::Menu
        && observation.uiInputAllowed
        && !g_showroomActive
        && (observation.menuBits
                & fnvxr::shared::RuntimeGenericMenuBit) != 0u;
    if (expectedMessageMenuState)
    {
        findExactOfficialPackMessageMenuTarget(
            &menu,
            &okTile,
            &officialPackNotificationMask,
            &exactlyOneFirstButtonOk);
    }
    const bool visibleMessageMenu = expectedMessageMenuState && menu != nullptr;
    if (visibleMessageMenu
        && officialPackNotificationMask != 0u
        && exactlyOneFirstButtonOk)
    {
        g_lastExactOfficialPackObservationFrame = observation.frame;
    }
    const bool alreadyAttempted = officialPackNotificationMask != 0u
        && (acknowledgedOfficialPackNotificationMask
            & officialPackNotificationMask) != 0u;
    const bool authorized = automation::exactOfficialPackAcknowledgementAuthorized(
        explicitlyOptedIn,
        visibleMessageMenu,
        officialPackNotificationMask != 0u,
        exactlyOneFirstButtonOk,
        alreadyAttempted);
    if (!authorized)
        return;

    // Mark before the native game call: a bad or changed retail vtable must
    // fail closed rather than repeatedly attempting a menu action.
    acknowledgedOfficialPackNotificationMask |= officialPackNotificationMask;
    bool invoked = false;
    bool nativeSelectionArmed = false;
    __try
    {
        void** vtable = menu ? *reinterpret_cast<void***>(menu) : nullptr;
        if (vtable && vtable[3] && vtable[4] && okTile)
        {
            // This exactly mirrors the game-native tile preparation used by
            // its regular menu click path, but only after the exact prompt
            // and unique first-button OK tile have been proven above.  It is
            // not a desktop, keyboard, mouse, controller, or simulator event.
            setTileFloatByName(
                okTile, "mouseover", TileValueMouseover, 1.0f);
            setTileFloatByName(okTile, "clicked", TileValueClicked, 1.0f);

            nativeSelectionArmed = armExactOfficialPackMessageMenuSelection(
                menu,
                okTile);
            if (nativeSelectionArmed)
            {
                // Invoke the same native mouseover-then-click dispatch slots
                // used by a normal GUI click, but only for the one verified
                // first-button tile. This is an in-game method call, not a
                // desktop key press, SendInput, controller event, or simulator
                // event.
                using HandleMouseoverFn = void (__thiscall*)(void*, UInt32, void*);
                reinterpret_cast<HandleMouseoverFn>(vtable[4])(
                    menu,
                    0u,
                    okTile);
                using HandleClickFn = void (__thiscall*)(void*, UInt32, void*);
                reinterpret_cast<HandleClickFn>(vtable[3])(
                    menu,
                    0u,
                    okTile);
                logTelemetry(
                    "fnvxrStereoVisualTrialOfficialPackNativeMouseoverAndClick menu=%p buttonId=0 tile=%p\n",
                    menu,
                    okTile);
                invoked = true;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        invoked = false;
    }
    if (invoked)
    {
        g_lastExactOfficialPackNativeAcknowledgementFrame = observation.frame;
    }
    logTelemetry(
        "{\"event\":\"fnvxrStereoVisualTrialOfficialPackAcknowledgement\",\"attempted\":true,\"officialPackMask\":%lu,\"exactFirstButtonOk\":true,\"nativeTileArmed\":%s,\"buttonIndex\":0,\"invoked\":%s,\"frame\":%llu}\n",
        static_cast<unsigned long>(officialPackNotificationMask),
        nativeSelectionArmed ? "true" : "false",
        invoked ? "true" : "false",
        static_cast<unsigned long long>(observation.frame));
}

void acknowledgeExactTtwStewieDependencyMessageMenu(
    const RuntimeObservation& observation)
{
    namespace fixture = fnvxr::engine::retail_fixture_automation;
    static bool acknowledged = false;
    const bool explicitlyOptedIn =
        retailFixtureTtwStewieDependencyAcknowledgementRequested();
    if (!explicitlyOptedIn)
        return;

    void* menu = nullptr;
    void* okTile = nullptr;
    bool exactTitle = false;
    bool exactBody = false;
    bool exactlyOneFirstButtonOk = false;
    const bool expectedMessageMenuState = observation.frame != 0u
        && observation.phase == RuntimePhase::Menu
        && observation.uiInputAllowed
        && !g_showroomActive
        && (observation.menuBits
                & fnvxr::shared::RuntimeGenericMenuBit) != 0u;
    if (expectedMessageMenuState)
    {
        findExactTtwStewieDependencyMessageMenuTarget(
            &menu,
            &okTile,
            &exactTitle,
            &exactBody,
            &exactlyOneFirstButtonOk);
    }
    const bool visibleMessageMenu = expectedMessageMenuState && menu != nullptr;
    const bool authorized =
        fixture::exactTtwStewieDependencyAcknowledgementAuthorized(
            explicitlyOptedIn,
            visibleMessageMenu,
            exactTitle,
            exactBody,
            exactlyOneFirstButtonOk,
            acknowledged);
    if (!authorized)
        return;

    // Mark before entering the retail vtable so a changed handler fails
    // closed instead of turning this one exact warning into a retry loop.
    acknowledged = true;
    bool invoked = false;
    bool nativeSelectionArmed = false;
    __try
    {
        void** vtable = menu ? *reinterpret_cast<void***>(menu) : nullptr;
        if (vtable && vtable[3] && vtable[4] && okTile)
        {
            setTileFloatByName(
                okTile, "mouseover", TileValueMouseover, 1.0f);
            setTileFloatByName(okTile, "clicked", TileValueClicked, 1.0f);
            nativeSelectionArmed = armExactOfficialPackMessageMenuSelection(
                menu,
                okTile);
            if (nativeSelectionArmed)
            {
                using HandleMouseoverFn =
                    void (__thiscall*)(void*, UInt32, void*);
                reinterpret_cast<HandleMouseoverFn>(vtable[4])(
                    menu,
                    0u,
                    okTile);
                using HandleClickFn =
                    void (__thiscall*)(void*, UInt32, void*);
                reinterpret_cast<HandleClickFn>(vtable[3])(
                    menu,
                    0u,
                    okTile);
                invoked = true;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        invoked = false;
    }
    logTelemetry(
        "{\"event\":\"fnvxrRetailFixtureTtwStewieDependencyAcknowledgement\",\"attempted\":true,\"exactTitle\":true,\"exactBody\":true,\"exactFirstButtonOk\":true,\"nativeTileArmed\":%s,\"buttonIndex\":0,\"invoked\":%s,\"frame\":%llu}\n",
        nativeSelectionArmed ? "true" : "false",
        invoked ? "true" : "false",
        static_cast<unsigned long long>(observation.frame));
}

void completeStereoVisualTrialFreshCharacterRequest(
    UInt32 requestId,
    UInt64 frame,
    bool ok,
    const char* command,
    const char* stage)
{
    g_lastCommandRequestId = requestId;
    const bool completionPublished = publishSharedCommandStatus(
        requestId,
        ok
            ? fnvxr::shared::CommandStatusSucceeded
            : fnvxr::shared::CommandStatusFailed,
        frame,
        ok ? 0u : ERROR_ACCESS_DENIED,
        command);
    if (!completionPublished)
    {
        logTelemetry(
            "stereo visual-trial fresh-character completion ownership lost request=%lu stage=%s\n",
            static_cast<unsigned long>(requestId),
            stage ? stage : "unknown");
    }
    logTelemetry(
        "{\"event\":\"fnvxrStereoVisualTrialFreshCharacterComplete\",\"requestId\":%lu,\"stage\":\"%s\",\"ok\":%s,\"resultCode\":%lu,\"frame\":%llu}\n",
        static_cast<unsigned long>(requestId),
        stage ? stage : "unknown",
        ok ? "true" : "false",
        static_cast<unsigned long>(ok ? 0u : ERROR_ACCESS_DENIED),
        static_cast<unsigned long long>(frame));
}

// The stock MessageMenu handler is invoked above for the exact prompt, but
// some retail pack notifications retain their tile even after that handler
// returns. The fixture owns a second, stricter fallback: re-prove the exact
// notice and submit only FNV's fixed stock menu-close console command. No menu
// traversal or device event is involved, and no launcher supplied text reaches
// the console.
void closeExactRetailFixtureOfficialPackMessageMenu(
    const RuntimeObservation& observation)
{
    namespace automation = fnvxr::engine::stereo_visual_trial_automation;
    namespace fixture = fnvxr::engine::retail_fixture_automation;
    static UInt32 closeAttemptCount = 0u;
    static UInt32 visibleOfficialPackNotificationMask = 0u;
    static bool exactOfficialPackVisibleLastFrame = false;
    const bool explicitlyOptedIn =
        retailFixtureOfficialPackAcknowledgementRequested();
    if (!explicitlyOptedIn)
        return;

    // The native exact-OK dispatch above and this stock CloseAllMenus fallback
    // must never run back-to-back in one frame.  Give the game time to commit
    // its own acknowledgement first; the fallback remains available only for
    // a still-visible, independently re-proven stock prompt after that grace.
    constexpr UInt64 NativeAcknowledgementSettlingFrames = 90u;
    if (g_lastExactOfficialPackNativeAcknowledgementFrame != 0u
        && observation.frame >= g_lastExactOfficialPackNativeAcknowledgementFrame
        && observation.frame
            - g_lastExactOfficialPackNativeAcknowledgementFrame
            < NativeAcknowledgementSettlingFrames)
    {
        return;
    }

    void* menu = nullptr;
    void* okTile = nullptr;
    UInt32 officialPackNotificationMask = 0u;
    bool exactlyOneFirstButtonOk = false;
    const bool expectedMessageMenuState = observation.frame != 0u
        && observation.phase == RuntimePhase::Menu
        && observation.uiInputAllowed
        && !g_showroomActive
        && (observation.menuBits
                & fnvxr::shared::RuntimeGenericMenuBit) != 0u;
    if (expectedMessageMenuState)
    {
        findExactOfficialPackMessageMenuTarget(
            &menu,
            &okTile,
            &officialPackNotificationMask,
            &exactlyOneFirstButtonOk);
    }
    const bool visibleMessageMenu = expectedMessageMenuState && menu != nullptr;
    const bool exactVisible = visibleMessageMenu
        && officialPackNotificationMask != 0u
        && exactlyOneFirstButtonOk;
    if (!exactVisible)
    {
        exactOfficialPackVisibleLastFrame = false;
        visibleOfficialPackNotificationMask = 0u;
        return;
    }
    g_lastExactOfficialPackObservationFrame = observation.frame;

    // The retail message queue can show the same exact stock pack notice more
    // than once after a successful close. Permit one fixed command per new
    // visible episode, while keeping a hard process-local cap so a changed
    // retail UI cannot turn this into general menu authority.
    const bool newVisibleEpisode = !exactOfficialPackVisibleLastFrame
        || visibleOfficialPackNotificationMask != officialPackNotificationMask;
    exactOfficialPackVisibleLastFrame = true;
    visibleOfficialPackNotificationMask = officialPackNotificationMask;
    const bool alreadyAttempted = !newVisibleEpisode
        || closeAttemptCount
            >= fixture::MaxExactOfficialPackCloseAttemptsPerRun;
    const bool authorized = fixture::exactOfficialPackCloseAuthorized(
        explicitlyOptedIn,
        visibleMessageMenu,
        officialPackNotificationMask != 0u,
        exactlyOneFirstButtonOk,
        alreadyAttempted);
    if (!authorized)
        return;

    ++closeAttemptCount;
    const bool submitted = runPluginConsoleCommand(
        "fnvxrRetailFixtureOfficialPackClose",
        fixture::CloseExactOfficialPackMessageCommand.data());
    logTelemetry(
        "{\"event\":\"fnvxrRetailFixtureOfficialPackClose\",\"submitted\":%s,\"officialPackMask\":%lu,\"exactFirstButtonOk\":true,\"attempt\":%lu,\"frame\":%llu}\n",
        submitted ? "true" : "false",
        static_cast<unsigned long>(officialPackNotificationMask),
        static_cast<unsigned long>(closeAttemptCount),
        static_cast<unsigned long long>(observation.frame));
}

void closeExactRetailFixtureTtwStewieDependencyMessageMenu(
    const RuntimeObservation& observation)
{
    namespace fixture = fnvxr::engine::retail_fixture_automation;
    static UInt32 closeAttemptCount = 0u;
    static bool exactWarningVisibleLastFrame = false;
    const bool explicitlyOptedIn =
        retailFixtureTtwStewieDependencyAcknowledgementRequested();
    if (!explicitlyOptedIn)
        return;

    void* menu = nullptr;
    void* okTile = nullptr;
    bool exactTitle = false;
    bool exactBody = false;
    bool exactlyOneFirstButtonOk = false;
    const bool expectedMessageMenuState = observation.frame != 0u
        && observation.phase == RuntimePhase::Menu
        && observation.uiInputAllowed
        && !g_showroomActive
        && (observation.menuBits
                & fnvxr::shared::RuntimeGenericMenuBit) != 0u;
    if (expectedMessageMenuState)
    {
        findExactTtwStewieDependencyMessageMenuTarget(
            &menu,
            &okTile,
            &exactTitle,
            &exactBody,
            &exactlyOneFirstButtonOk);
    }
    const bool visibleMessageMenu = expectedMessageMenuState && menu != nullptr;
    const bool exactVisible = visibleMessageMenu
        && exactTitle
        && exactBody
        && exactlyOneFirstButtonOk;
    if (!exactVisible)
    {
        exactWarningVisibleLastFrame = false;
        return;
    }

    const bool newVisibleEpisode = !exactWarningVisibleLastFrame;
    exactWarningVisibleLastFrame = true;
    const bool alreadyAttempted = !newVisibleEpisode
        || closeAttemptCount
            >= fixture::MaxExactTtwStewieDependencyCloseAttemptsPerRun;
    const bool authorized =
        fixture::exactTtwStewieDependencyAcknowledgementAuthorized(
            explicitlyOptedIn,
            visibleMessageMenu,
            exactTitle,
            exactBody,
            exactlyOneFirstButtonOk,
            alreadyAttempted);
    if (!authorized)
        return;

    ++closeAttemptCount;
    const bool submitted = runPluginConsoleCommand(
        "fnvxrRetailFixtureTtwStewieDependencyClose",
        fixture::CloseExactOfficialPackMessageCommand.data());
    logTelemetry(
        "{\"event\":\"fnvxrRetailFixtureTtwStewieDependencyClose\",\"submitted\":%s,\"exactTitle\":true,\"exactBody\":true,\"exactFirstButtonOk\":true,\"attempt\":%lu,\"frame\":%llu}\n",
        submitted ? "true" : "false",
        static_cast<unsigned long>(closeAttemptCount),
        static_cast<unsigned long long>(observation.frame));
}

void processOwnedRetailFixtureMessageMenuAcknowledgements(
    const RuntimeObservation& observation)
{
    // Each helper independently matches one complete, versioned MessageMenu
    // and its unique native first-button OK tile.  This deliberately does not
    // dispatch controller, pointer, keyboard, mouse, or arbitrary menu input.
    acknowledgeExactOfficialPackMessageMenu(observation);
    closeExactRetailFixtureOfficialPackMessageMenu(observation);
    acknowledgeExactTtwStewieDependencyMessageMenu(observation);
    closeExactRetailFixtureTtwStewieDependencyMessageMenu(observation);
}

enum class RetailFixtureAutomationStage : UInt8
{
    AwaitingStartMenu = 0u,
    AwaitingLoadGameplay,
    AwaitingGameplayName,
    AwaitingFirstTrait,
    AwaitingSecondTrait,
    AwaitingWeaponAdd,
    AwaitingWeaponAmmo,
    AwaitingWeaponEquip,
    AwaitingSave,
    Complete,
};

bool buildRetailFixtureNamedCommand(
    std::string_view prefix,
    const fnvxr::engine::retail_fixture_automation::Plan& plan,
    char* output,
    std::size_t outputSize)
{
    namespace fixture = fnvxr::engine::retail_fixture_automation;
    if (!output || outputSize == 0u || !fixture::authorized(plan)
        || prefix.size() + plan.saveName.size() + 1u > outputSize)
    {
        return false;
    }
    std::memcpy(output, prefix.data(), prefix.size());
    std::memcpy(output + prefix.size(), plan.saveName.data(), plan.saveName.size());
    output[prefix.size() + plan.saveName.size()] = '\0';
    return true;
}

bool copyRetailFixtureStaticCommand(
    std::string_view command,
    char* output,
    std::size_t outputSize)
{
    if (!output || outputSize == 0u || command.size() + 1u > outputSize)
        return false;
    std::memcpy(output, command.data(), command.size());
    output[command.size()] = '\0';
    return true;
}

bool retailFixtureCommandIsExact(
    const SharedCommandState& request,
    const char* expected)
{
    if (!expected || request.command != fnvxr::shared::CommandTypeConsole)
        return false;
    const std::size_t expectedSize = std::strlen(expected);
    return expectedSize + 1u <= sizeof(request.saveName)
        && std::memcmp(request.saveName, expected, expectedSize) == 0
        && request.saveName[expectedSize] == '\0';
}

bool completeRetailFixtureAutomationRequest(
    UInt32 requestId,
    UInt64 frame,
    bool ok,
    const char* command,
    const char* stage)
{
    const bool completionPublished = publishSharedCommandStatus(
        requestId,
        ok
            ? fnvxr::shared::CommandStatusSucceeded
            : fnvxr::shared::CommandStatusFailed,
        frame,
        ok ? 0u : ERROR_ACCESS_DENIED,
        command);
    if (completionPublished)
        g_lastCommandRequestId = requestId;
    if (!completionPublished)
    {
        logTelemetry(
            "retail fixture completion ownership lost request=%lu stage=%s\n",
            static_cast<unsigned long>(requestId),
            stage ? stage : "unknown");
    }
    logTelemetry(
        "{\"event\":\"fnvxrRetailFixtureComplete\",\"requestId\":%lu,\"stage\":\"%s\",\"ok\":%s,\"published\":%s,\"resultCode\":%lu,\"frame\":%llu}\n",
        static_cast<unsigned long>(requestId),
        stage ? stage : "unknown",
        ok ? "true" : "false",
        completionPublished ? "true" : "false",
        static_cast<unsigned long>(ok ? 0u : ERROR_ACCESS_DENIED),
        static_cast<unsigned long long>(frame));
    return completionPublished;
}

void processRetailFixtureAutomation(const RuntimeObservation& observation)
{
    namespace fixture = fnvxr::engine::retail_fixture_automation;
    if (!retailFixtureAutomationRequested())
        return;

    // Retail can present a known official pre-order-pack inventory notice
    // after COC even when the temporary profile contains only FalloutNV.esm.
    // This invokes the separately gated exact native click-handler path and,
    // only if the stock modal remains, one exact stock-console fallback. It
    // never emits desktop, controller, keyboard, mouse, or simulator input.
    // Retail can enqueue a duplicate known official-pack notice after the
    // owned fixture has reached gameplay. Keep this exact matcher alive for
    // the bounded demo so that one of those modals cannot cover the Pip-Boy.
    // Neither helper accepts arbitrary UI: each revalidates the known title,
    // body, unique first-button OK tile, and per-pack/attempt limits.
    processOwnedRetailFixtureMessageMenuAcknowledgements(observation);

    // This owned fixture path intentionally contains no menu traversal or
    // device input. It accepts a single start-menu command, then uses only the
    // static Goodsprings/name/trait/loadout commands and a validated owned
    // fixture save name. A player's historical saves cannot pass the gate.
    static RetailFixtureAutomationStage stage =
        RetailFixtureAutomationStage::AwaitingStartMenu;
    static RetailFixtureAutomationPlan fixturePlan {};
    static UInt32 requestId = 0u;
    static UInt64 lastMutationFrame = 0u;
    // A load is only acknowledged from a later main-loop observation, after
    // the native console call has unwound. That observation already proves a
    // real, menu-free gameplay camera, so additional frame delay is neither a
    // safety condition nor a useful popup-handling window.
    constexpr UInt64 FixtureLoadGameplaySettlingFrames = 1u;
    constexpr UInt64 FixtureMutationSettlingFrames = 20u;
    // Exact official-pack handling runs before this state-machine's terminal
    // check, so it remains live after a fixture request has completed. Do not
    // keep the command pending just to extend that independently bounded
    // handler: acknowledge once a later gameplay observation proves that the
    // owned load has settled.

    if (stage == RetailFixtureAutomationStage::Complete)
        return;

    if (stage == RetailFixtureAutomationStage::AwaitingStartMenu)
    {
        SharedCommandState request {};
        if (!readSharedCommandSnapshot(request))
            return;
        if (request.requestId == 0u || request.requestId == g_lastCommandRequestId)
            return;
        if (request.status != fnvxr::shared::CommandStatusPending)
        {
            g_lastCommandRequestId = request.requestId;
            logTelemetry(
                "{\"event\":\"fnvxrRetailFixtureSkip\",\"requestId\":%lu,\"status\":\"%s\",\"frame\":%llu}\n",
                static_cast<unsigned long>(request.requestId),
                sharedCommandStatusName(request.status),
                static_cast<unsigned long long>(observation.frame));
            return;
        }

        const bool planValid = readRetailFixtureAutomationPlan(fixturePlan);
        char expectedCommand[64] {};
        bool commandBuilt = false;
        if (planValid)
        {
            commandBuilt = fixturePlan.plan.action == fixture::Action::Create
                ? copyRetailFixtureStaticCommand(
                    fixture::CreateStartCommand,
                    expectedCommand,
                    sizeof(expectedCommand))
                : buildRetailFixtureNamedCommand(
                    fixture::LoadCommandPrefix,
                    fixturePlan.plan,
                    expectedCommand,
                    sizeof(expectedCommand));
        }
        const bool startMenu = realStereoVisualTrialStartMenuState(observation);
        const bool exactCommand = commandBuilt
            && retailFixtureCommandIsExact(request, expectedCommand);
        const bool authorized = planValid && startMenu && exactCommand;
        logTelemetry(
            "{\"event\":\"fnvxrRetailFixtureStartRequest\",\"requestId\":%lu,\"planValid\":%s,\"startMenu\":%s,\"exactCommand\":%s,\"authorized\":%s,\"action\":\"%s\",\"saveName\":\"%s\",\"frame\":%llu}\n",
            static_cast<unsigned long>(request.requestId),
            planValid ? "true" : "false",
            startMenu ? "true" : "false",
            exactCommand ? "true" : "false",
            authorized ? "true" : "false",
            planValid && fixturePlan.plan.action == fixture::Action::Create
                ? "create"
                : (planValid && fixturePlan.plan.action == fixture::Action::Load
                    ? "load" : "invalid"),
            planValid ? fixturePlan.saveName : "",
            static_cast<unsigned long long>(observation.frame));
        if (!publishSharedCommandStatus(
                request.requestId,
                fnvxr::shared::CommandStatusRunning,
                observation.frame,
                authorized ? 0u : ERROR_ACCESS_DENIED,
                authorized ? expectedCommand : ""))
        {
            return;
        }
        if (!authorized)
        {
            stage = RetailFixtureAutomationStage::Complete;
            completeRetailFixtureAutomationRequest(
                request.requestId,
                observation.frame,
                false,
                "",
                "start-gate");
            return;
        }

        requestId = request.requestId;
        const bool ok = runPluginConsoleCommand(
            fixturePlan.plan.action == fixture::Action::Create
                ? "fnvxrRetailFixtureCreateStart"
                : "fnvxrRetailFixtureLoad",
            expectedCommand);
        if (fixturePlan.plan.action == fixture::Action::Load)
        {
            if (!ok)
            {
                stage = RetailFixtureAutomationStage::Complete;
                completeRetailFixtureAutomationRequest(
                    requestId,
                    observation.frame,
                    false,
                    expectedCommand,
                    "load-dispatch");
                return;
            }
            // A retail load can re-enter its engine work while RunScriptLine2
            // is still on this stack. Wait for a subsequent, settled real
            // gameplay observation before acknowledging the exact request.
            lastMutationFrame = observation.frame;
            stage = RetailFixtureAutomationStage::AwaitingLoadGameplay;
            logTelemetry(
                "{\"event\":\"fnvxrRetailFixtureLoadDispatched\",\"requestId\":%lu,\"frame\":%llu}\n",
                static_cast<unsigned long>(requestId),
                static_cast<unsigned long long>(observation.frame));
            return;
        }
        if (!ok)
        {
            stage = RetailFixtureAutomationStage::Complete;
            completeRetailFixtureAutomationRequest(
                requestId,
                observation.frame,
                false,
                expectedCommand,
                "coc");
            return;
        }
        lastMutationFrame = observation.frame;
        stage = RetailFixtureAutomationStage::AwaitingGameplayName;
        return;
    }

    if (!realRetailFixtureFreshGameplayState(observation) || requestId == 0u)
        return;

    if (stage == RetailFixtureAutomationStage::AwaitingLoadGameplay)
    {
        if (observation.frame
            < lastMutationFrame + FixtureLoadGameplaySettlingFrames)
            return;
        char loadCommand[64] {};
        const bool commandBuilt = buildRetailFixtureNamedCommand(
            fixture::LoadCommandPrefix,
            fixturePlan.plan,
            loadCommand,
            sizeof(loadCommand));
        const bool published = completeRetailFixtureAutomationRequest(
            requestId,
            observation.frame,
            commandBuilt,
            commandBuilt ? loadCommand : "",
            "load-gameplay");
        logTelemetry(
            "{\"event\":\"fnvxrRetailFixtureLoadGameplay\",\"requestId\":%lu,\"commandBuilt\":%s,\"published\":%s,\"frame\":%llu}\n",
            static_cast<unsigned long>(requestId),
            commandBuilt ? "true" : "false",
            published ? "true" : "false",
            static_cast<unsigned long long>(observation.frame));
        if (published)
        {
            stage = RetailFixtureAutomationStage::Complete;
            g_headsetDemoFixtureReady = headsetDemoFixtureProfileSelected()
                && commandBuilt;
        }
        return;
    }

    if (stage == RetailFixtureAutomationStage::AwaitingGameplayName)
    {
        const bool ok = runPluginConsoleCommand(
            "fnvxrRetailFixtureSetName",
            fixture::SetFixturePlayerNameCommand.data());
        logTelemetry(
            "{\"event\":\"fnvxrRetailFixtureSetName\",\"requestId\":%lu,\"ok\":%s,\"frame\":%llu}\n",
            static_cast<unsigned long>(requestId),
            ok ? "true" : "false",
            static_cast<unsigned long long>(observation.frame));
        if (!ok)
        {
            stage = RetailFixtureAutomationStage::Complete;
            completeRetailFixtureAutomationRequest(
                requestId,
                observation.frame,
                false,
                fixture::SetFixturePlayerNameCommand.data(),
                "name");
            return;
        }
        lastMutationFrame = observation.frame;
        stage = RetailFixtureAutomationStage::AwaitingFirstTrait;
        return;
    }

    if (observation.frame
        < lastMutationFrame + FixtureMutationSettlingFrames)
        return;

    fixture::Trait trait = fixture::Trait::None;
    const char* stageName = nullptr;
    RetailFixtureAutomationStage nextStage =
        RetailFixtureAutomationStage::Complete;
    if (stage == RetailFixtureAutomationStage::AwaitingFirstTrait)
    {
        trait = fixturePlan.plan.firstTrait;
        stageName = "trait-one";
        nextStage = RetailFixtureAutomationStage::AwaitingSecondTrait;
    }
    else if (stage == RetailFixtureAutomationStage::AwaitingSecondTrait)
    {
        trait = fixturePlan.plan.secondTrait;
        stageName = "trait-two";
        nextStage = fixturePlan.plan.weapon == fixture::Weapon::None
            ? RetailFixtureAutomationStage::AwaitingSave
            : RetailFixtureAutomationStage::AwaitingWeaponAdd;
    }
    else if (stage == RetailFixtureAutomationStage::AwaitingWeaponAdd
        || stage == RetailFixtureAutomationStage::AwaitingWeaponAmmo
        || stage == RetailFixtureAutomationStage::AwaitingWeaponEquip)
    {
        std::string_view weaponCommand {};
        if (stage == RetailFixtureAutomationStage::AwaitingWeaponAdd)
        {
            weaponCommand = fixture::addWeaponCommand(fixturePlan.plan.weapon);
            stageName = "weapon-add";
            nextStage = RetailFixtureAutomationStage::AwaitingWeaponAmmo;
        }
        else if (stage == RetailFixtureAutomationStage::AwaitingWeaponAmmo)
        {
            weaponCommand = fixture::addWeaponAmmoCommand(fixturePlan.plan.weapon);
            stageName = "weapon-ammo";
            nextStage = RetailFixtureAutomationStage::AwaitingWeaponEquip;
        }
        else
        {
            weaponCommand = fixture::equipWeaponCommand(fixturePlan.plan.weapon);
            stageName = "weapon-equip";
            nextStage = RetailFixtureAutomationStage::AwaitingSave;
        }

        char command[96] {};
        const bool commandBuilt = weaponCommand.empty()
            || copyRetailFixtureStaticCommand(
                weaponCommand,
                command,
                sizeof(command));
        const bool ok = commandBuilt && (weaponCommand.empty()
            || runPluginConsoleCommand(
                "fnvxrRetailFixtureWeaponLoadout",
                command));
        const fixture::WeaponCommand* const weapon =
            fixture::findWeapon(fixturePlan.plan.weapon);
        logTelemetry(
            "{\"event\":\"fnvxrRetailFixtureWeapon\",\"requestId\":%lu,\"stage\":\"%s\",\"weapon\":\"%s\",\"hasStaticCommand\":%s,\"ok\":%s,\"frame\":%llu}\n",
            static_cast<unsigned long>(requestId),
            stageName,
            weapon ? weapon->token.data() : "invalid",
            weaponCommand.empty() ? "false" : "true",
            ok ? "true" : "false",
            static_cast<unsigned long long>(observation.frame));
        if (!ok)
        {
            stage = RetailFixtureAutomationStage::Complete;
            completeRetailFixtureAutomationRequest(
                requestId,
                observation.frame,
                false,
                commandBuilt ? command : "",
                stageName);
            return;
        }
        lastMutationFrame = observation.frame;
        stage = nextStage;
        return;
    }
    else if (stage == RetailFixtureAutomationStage::AwaitingSave)
    {
        char saveCommand[64] {};
        const bool commandBuilt = buildRetailFixtureNamedCommand(
            fixture::SaveCommandPrefix,
            fixturePlan.plan,
            saveCommand,
            sizeof(saveCommand));
        const bool ok = commandBuilt && runPluginConsoleCommand(
            "fnvxrRetailFixtureSave",
            saveCommand);
        stage = RetailFixtureAutomationStage::Complete;
        const bool completed = completeRetailFixtureAutomationRequest(
            requestId,
            observation.frame,
            ok,
            commandBuilt ? saveCommand : "",
            "save");
        g_headsetDemoFixtureReady = headsetDemoFixtureProfileSelected()
            && ok
            && completed;
        return;
    }
    else
    {
        return;
    }

    const std::string_view traitCommand = fixture::addPerkCommand(trait);
    const bool ok = trait == fixture::Trait::None || (!traitCommand.empty()
        && runPluginConsoleCommand("fnvxrRetailFixtureAddTrait", traitCommand.data()));
    logTelemetry(
        "{\"event\":\"fnvxrRetailFixtureTrait\",\"requestId\":%lu,\"stage\":\"%s\",\"trait\":\"%s\",\"ok\":%s,\"frame\":%llu}\n",
        static_cast<unsigned long>(requestId),
        stageName,
        fixture::findTrait(trait) ? fixture::findTrait(trait)->token.data() : "invalid",
        ok ? "true" : "false",
        static_cast<unsigned long long>(observation.frame));
    if (!ok)
    {
        stage = RetailFixtureAutomationStage::Complete;
        completeRetailFixtureAutomationRequest(
            requestId,
            observation.frame,
            false,
            traitCommand.empty() ? "" : traitCommand.data(),
            stageName);
        return;
    }
    lastMutationFrame = observation.frame;
    stage = nextStage;
}

void processHeadsetDemoFixtureUi(const RuntimeObservation& observation)
{
    if (!headsetDemoUiProfileSelected())
        return;

    // Apart from the separately bounded exact official-pack acknowledgement
    // above, the headset demo has exactly two native engine actions: open the
    // inventory Pip-Boy after stable gameplay, then close it after the bounded
    // hold. This invokes the retail InterfaceManager directly on the game
    // thread; it is never an OS key, desktop/window operation, input-device
    // event, or simulator command.
    namespace demo = fnvxr::engine::headset_demo;
    static demo::State state {};
    static UInt64 pipBoyOpenedAtMilliseconds = 0u;

    const std::uint64_t gameplayWarmupFrames = static_cast<std::uint64_t>(
        std::clamp(getIntFromEnv("FNVXR_HEADSET_DEMO_GAMEPLAY_WARMUP_FRAMES", 90), 1, 1200));
    const std::uint64_t pipBoyHoldFrames = static_cast<std::uint64_t>(
        std::clamp(getIntFromEnv("FNVXR_HEADSET_DEMO_PIPBOY_HOLD_FRAMES", 240), 30, 3600));
    const bool pipBoyVisible = pipBoyVisibleFromMenuBits(observation.menuBits);
    const demo::Input input {
        true,
        g_headsetDemoFixtureReady,
        realRetailFixtureFreshGameplayState(observation),
        pipBoyVisible,
        g_headsetDemoFixtureReady,
        observation.frame,
        gameplayWarmupFrames,
        pipBoyHoldFrames,
    };
    const demo::Decision decision = demo::advance(state, input);
    if (decision.action != demo::Action::None)
    {
        const UInt64 now = GetTickCount64();
        const UInt64 minimumOpenMilliseconds = static_cast<UInt64>(
            std::clamp(
                getIntFromEnv(
                    "FNVXR_HEADSET_DEMO_PIPBOY_MIN_HOLD_MILLISECONDS",
                    5000),
                1000,
                15000));
        if (decision.action == demo::Action::ClosePipBoy
            && pipBoyOpenedAtMilliseconds != 0u
            && now >= pipBoyOpenedAtMilliseconds
            && now - pipBoyOpenedAtMilliseconds < minimumOpenMilliseconds)
        {
            // Engine frame rate is uncapped in the owned headless fixture.
            // A frame-only hold closed the menu after 0.7 seconds despite a
            // nominal 240-frame dwell. Keep the state at Open until real
            // wall time proves sustained visible menu content.
            return;
        }
        const bool handled = decision.action == demo::Action::OpenPipBoy
            ? openEnginePipBoyInventory("headset-demo-fixed", observation.frame)
            : closeEnginePipBoy("headset-demo-fixed", observation.frame);
        if (handled && decision.action == demo::Action::OpenPipBoy)
            pipBoyOpenedAtMilliseconds = now;
        else if (handled && decision.action == demo::Action::ClosePipBoy)
            pipBoyOpenedAtMilliseconds = 0u;
        logTelemetry(
            "{\"event\":\"fnvxrHeadsetDemoPipBoyAction\",\"action\":\"%s\",\"handled\":%s,\"fixtureReady\":%s,\"gameplay\":%s,\"pipBoyVisible\":%s,\"frame\":%llu}\n",
            decision.action == demo::Action::OpenPipBoy ? "open" : "close",
            handled ? "true" : "false",
            g_headsetDemoFixtureReady ? "true" : "false",
            input.gameplay ? "true" : "false",
            pipBoyVisible ? "true" : "false",
            static_cast<unsigned long long>(observation.frame));
        if (!handled)
            return;
    }
    if (state.stage != decision.next.stage
        || state.stageFrame != decision.next.stageFrame)
    {
        logTelemetry(
            "{\"event\":\"fnvxrHeadsetDemoPipBoyStage\",\"stage\":%u,\"fixtureReady\":%s,\"gameplay\":%s,\"pipBoyVisible\":%s,\"frame\":%llu}\n",
            static_cast<unsigned>(decision.next.stage),
            g_headsetDemoFixtureReady ? "true" : "false",
            input.gameplay ? "true" : "false",
            pipBoyVisible ? "true" : "false",
            static_cast<unsigned long long>(observation.frame));
    }
    state = decision.next;
}

void processStereoVisualTrialFixedSaveAutomation(
    const RuntimeObservation& observation)
{
    namespace automation =
        fnvxr::engine::stereo_visual_trial_automation;
    if (!stereoVisualTrialAutomationRequested())
        return;

    // The state is process-local. Every transition is fixed in the authority
    // gate. A recovery load is one exact command. A fresh character can only
    // use the fixed no-save COC, fixed name, and fixed save sequence.  The
    // only menu exception is the separately opted-in, once-per-known-pack
    // native acknowledgement of an exact official-pack notification; neither
    // route gains input, camera, rig, or weapon control.
    acknowledgeExactOfficialPackMessageMenu(observation);
    static automation::State authorityState {};
    static const automation::ApprovedRetailSave* const selectedRetailSave =
        stereoVisualTrialRecoveryLoadRequested()
            ? stereoVisualTrialSelectedRetailSave()
            : nullptr;
    static UInt32 freshCharacterRequestId = 0u;
    static UInt64 freshCharacterNameFrame = 0u;

    if (authorityState.stage
        == automation::Stage::AwaitingStartMenuCommand)
    {
        SharedCommandState request {};
        if (!readSharedCommandSnapshot(request))
            return;
        if (request.requestId == 0u
            || request.requestId == g_lastCommandRequestId)
        {
            return;
        }
        if (request.status != fnvxr::shared::CommandStatusPending)
        {
            logTelemetry(
                "{\"event\":\"fnvxrStereoVisualTrialAutomationSkip\",\"requestId\":%lu,\"status\":\"%s\",\"frame\":%llu}\n",
                static_cast<unsigned long>(request.requestId),
                sharedCommandStatusName(request.status),
                static_cast<unsigned long long>(observation.frame));
            g_lastCommandRequestId = request.requestId;
            return;
        }

        const bool recoveryWorkflow =
            stereoVisualTrialRecoveryLoadRequested();
        const bool freshCharacterWorkflow =
            stereoVisualTrialFreshCharacterRequested();
        const bool exactRecoveryLoad = recoveryWorkflow
            && selectedRetailSave != nullptr
            && stereoVisualTrialRecoveryLoadCommandIsExact(
                request,
                *selectedRetailSave);
        const bool exactFreshCharacterStart = freshCharacterWorkflow
            && stereoVisualTrialFreshCharacterCommandIsExact(request);
        const bool realStartMenu =
            realStereoVisualTrialStartMenuState(observation);
        automation::Request authorityRequest {};
        authorityRequest.explicitlyOptedIn =
            stereoVisualTrialAutomationRequested();
        authorityRequest.action = exactRecoveryLoad
            ? automation::Action::LoadFixedRecoverySave
            : (exactFreshCharacterStart
                ? automation::Action::StartFreshCharacter
                : automation::Action::None);
        authorityRequest.argument = exactRecoveryLoad
            ? selectedRetailSave->name
            : (exactFreshCharacterStart
                ? automation::FreshCharacterStartCommand
                : std::string_view {});
        const automation::Decision decision =
            automation::decide(authorityState, authorityRequest);
        const bool authorized = (exactRecoveryLoad || exactFreshCharacterStart)
            && realStartMenu
            && decision.authorized
            && (exactRecoveryLoad
                ? decision.command == selectedRetailSave->loadCommand
                : decision.command == automation::FreshCharacterStartCommand);
        logTelemetry(
            "{\"event\":\"fnvxrStereoVisualTrialAutomationStartRequest\",\"requestId\":%lu,\"exactRecoveryLoad\":%s,\"exactFreshCharacterStart\":%s,\"realStartMenu\":%s,\"authorized\":%s,\"gateFailure\":%u,\"frame\":%llu}\n",
            static_cast<unsigned long>(request.requestId),
            exactRecoveryLoad ? "true" : "false",
            exactFreshCharacterStart ? "true" : "false",
            realStartMenu ? "true" : "false",
            authorized ? "true" : "false",
            static_cast<unsigned>(decision.failure),
            static_cast<unsigned long long>(observation.frame));
        if (!publishSharedCommandStatus(
                request.requestId,
                fnvxr::shared::CommandStatusRunning,
                observation.frame,
                authorized ? 0u : ERROR_ACCESS_DENIED,
                authorized ? decision.command.data() : ""))
        {
            logTelemetry(
                "stereo visual-trial automation lost ownership before running request=%lu\n",
                static_cast<unsigned long>(request.requestId));
            return;
        }

        if (!authorized)
        {
            g_lastCommandRequestId = request.requestId;
            const bool completionPublished = publishSharedCommandStatus(
                request.requestId,
                fnvxr::shared::CommandStatusFailed,
                observation.frame,
                ERROR_ACCESS_DENIED,
                "");
            if (!completionPublished)
            {
                logTelemetry(
                    "stereo visual-trial automation rejection completion ownership lost request=%lu\n",
                    static_cast<unsigned long>(request.requestId));
            }
            return;
        }

        authorityState = decision.nextState;
        if (exactRecoveryLoad)
        {
            const bool ok = runPluginConsoleCommand(
                "fnvxrStereoVisualTrialRecoveryLoad",
                decision.command.data());
            g_lastCommandRequestId = request.requestId;
            const bool completionPublished = publishSharedCommandStatus(
                request.requestId,
                ok
                    ? fnvxr::shared::CommandStatusSucceeded
                    : fnvxr::shared::CommandStatusFailed,
                observation.frame,
                ok ? 0u : ERROR_ACCESS_DENIED,
                decision.command.data());
            if (!completionPublished)
            {
                logTelemetry(
                    "stereo visual-trial recovery load completion ownership lost request=%lu\n",
                    static_cast<unsigned long>(request.requestId));
            }
            logTelemetry(
                "{\"event\":\"fnvxrStereoVisualTrialAutomationLoadComplete\",\"requestId\":%lu,\"ok\":%s,\"resultCode\":%lu,\"frame\":%llu}\n",
                static_cast<unsigned long>(request.requestId),
                ok ? "true" : "false",
                static_cast<unsigned long>(
                    ok ? 0u : ERROR_ACCESS_DENIED),
                static_cast<unsigned long long>(observation.frame));
            return;
        }

        freshCharacterRequestId = request.requestId;
        const bool ok = runPluginConsoleCommand(
            "fnvxrStereoVisualTrialFreshCharacterStart",
            decision.command.data());
        if (!ok)
        {
            authorityState.stage = automation::Stage::Complete;
            completeStereoVisualTrialFreshCharacterRequest(
                freshCharacterRequestId,
                observation.frame,
                false,
                decision.command.data(),
                "coc");
        }
        else
        {
            logTelemetry(
                "{\"event\":\"fnvxrStereoVisualTrialFreshCharacterStart\",\"requestId\":%lu,\"ok\":true,\"frame\":%llu}\n",
                static_cast<unsigned long>(freshCharacterRequestId),
                static_cast<unsigned long long>(observation.frame));
        }
        return;
    }

    if (authorityState.stage
        == automation::Stage::AwaitingFreshGameplayName)
    {
        if (!realStereoVisualTrialFreshGameplayState(observation)
            || freshCharacterRequestId == 0u)
        {
            return;
        }

        const automation::Decision decision = automation::decide(
            authorityState,
            { true, automation::Action::NameFreshCharacter, {} });
        const bool authorized = decision.authorized
            && decision.command == automation::FreshCharacterSetNameCommand;
        logTelemetry(
            "{\"event\":\"fnvxrStereoVisualTrialFreshCharacterName\",\"requestId\":%lu,\"authorized\":%s,\"gateFailure\":%u,\"frame\":%llu}\n",
            static_cast<unsigned long>(freshCharacterRequestId),
            authorized ? "true" : "false",
            static_cast<unsigned>(decision.failure),
            static_cast<unsigned long long>(observation.frame));
        authorityState = authorized
            ? decision.nextState
            : automation::State { automation::Stage::Complete };
        const bool ok = authorized && runPluginConsoleCommand(
            "fnvxrStereoVisualTrialFreshCharacterName",
            decision.command.data());
        if (!ok)
        {
            completeStereoVisualTrialFreshCharacterRequest(
                freshCharacterRequestId,
                observation.frame,
                false,
                authorized ? decision.command.data() : "",
                "name");
            freshCharacterRequestId = 0u;
            return;
        }
        freshCharacterNameFrame = observation.frame;
        return;
    }

    if (authorityState.stage
        == automation::Stage::AwaitingFreshGameplaySave)
    {
        constexpr UInt64 FreshCharacterNameSettlingFrames = 30u;
        if (!realStereoVisualTrialFreshGameplayState(observation)
            || freshCharacterRequestId == 0u
            || observation.frame
                < freshCharacterNameFrame + FreshCharacterNameSettlingFrames)
        {
            return;
        }

        const automation::Decision decision = automation::decide(
            authorityState,
            { true, automation::Action::SaveFreshCharacter, {} });
        const bool authorized = decision.authorized
            && decision.command == automation::FreshCharacterSaveCommand;
        logTelemetry(
            "{\"event\":\"fnvxrStereoVisualTrialFreshCharacterSave\",\"requestId\":%lu,\"authorized\":%s,\"gateFailure\":%u,\"frame\":%llu}\n",
            static_cast<unsigned long>(freshCharacterRequestId),
            authorized ? "true" : "false",
            static_cast<unsigned>(decision.failure),
            static_cast<unsigned long long>(observation.frame));
        authorityState = authorized
            ? decision.nextState
            : automation::State { automation::Stage::Complete };
        const bool ok = authorized && runPluginConsoleCommand(
            "fnvxrStereoVisualTrialFreshCharacterSave",
            decision.command.data());
        completeStereoVisualTrialFreshCharacterRequest(
            freshCharacterRequestId,
            observation.frame,
            ok,
            authorized ? decision.command.data() : "",
            "save");
        freshCharacterRequestId = 0u;
        return;
    }
}

bool ensureAuthorizedDesktopAssistBridgeStarted()
{
    if (!desktopAssistProfileRequested())
        return false;

    if (g_desktopAssistBridgeStarted)
    {
        return gamePluginProducerLeaseHeldByCurrentThread()
            && g_vrPoseState
            && g_cameraState
            && g_runtimeState
            && g_playerState
            && g_desktopAssistState
            && g_cameraHookInstalled
            && g_cameraHookAuthorization == CameraHookAuthorization::DesktopAssist
            && (!desktopAssistAutomationRequested() || g_commandState)
            && desktopAssistCameraLeaseCurrent();
    }

    ++g_desktopAssistAuthorityAttempts;
    const fnvxr::engine::compatibility::RetailCompatibilityProof proof =
        fnvxr::engine::compatibility::proveCurrentRetailCompatibilityAtDecisionPoint();
    const fnvxr::engine::DesktopAssistCameraRequest request =
        desktopAssistCameraRequest();
    if (!fnvxr::engine::desktopAssistCameraAuthorized(proof, request))
    {
        if (g_desktopAssistAuthorityAttempts <= 12u
            || (g_desktopAssistAuthorityAttempts % 300u) == 0u)
        {
            logTelemetry(
                "desktopAssist bridge deferred attempt=%lu compatible=%d failure=%u; no input, renderer, weapon, or body mutation performed\n",
                static_cast<unsigned long>(g_desktopAssistAuthorityAttempts),
                static_cast<int>(proof.compatible),
                static_cast<unsigned>(proof.failure));
        }
        return false;
    }
    if (!acquireGamePluginProducerLease())
        return false;

    // Desktop assist normally maps only the external HMD pose and the
    // read-only evidence required by fnvxr_assist.  Its separately requested
    // unattended recovery path may map the command mailbox, but accepts only
    // one fixed load command below; it never enables a general command or
    // input bridge.
    initSharedVrPose();
    initSharedCamera();
    initSharedPlayer();
    initSharedDesktopAssist();
    if (!g_vrPoseState || !g_cameraState || !g_runtimeState || !g_playerState
        || !g_desktopAssistState
        || (desktopAssistAutomationRequested() && !g_commandState))
    {
        logTelemetry("desktopAssist bridge initialization deferred: required mapping unavailable\n");
        return false;
    }

    if (!installCameraHook()
        || !g_cameraHookInstalled
        || g_cameraHookAuthorization != CameraHookAuthorization::DesktopAssist)
    {
        logTelemetry("desktopAssist bridge deferred: camera hook was not authorized/installed\n");
        return false;
    }

    g_desktopAssistBridgeStarted = true;
    logTelemetry(
        "desktopAssist bridge ready attempt=%lu mode=rotation-only input=0 commandRecovery=%d renderer=0 weapon=0 rig=0 openxr=0\n",
        static_cast<unsigned long>(g_desktopAssistAuthorityAttempts),
        static_cast<int>(desktopAssistAutomationRequested()));
    return true;
}

bool ensureAuthorizedTrackedPropAssistBridgeStarted()
{
    if (!trackedPropAssistProfileRequested())
        return false;

    if (g_trackedPropAssistBridgeStarted)
    {
        return gamePluginProducerLeaseHeldByCurrentThread()
            && g_vrPoseState
            && g_cameraState
            && g_runtimeState
            && g_playerState
            && g_cameraHookInstalled
            && g_cameraHookAuthorization == CameraHookAuthorization::TrackedPropAssist
            && g_retailRigHookInstalled
            && trackedPropAssistLeaseCurrent();
    }

    ++g_trackedPropAssistAuthorityAttempts;
    const fnvxr::engine::compatibility::RetailCompatibilityProof proof =
        fnvxr::engine::compatibility::proveCurrentRetailCompatibilityAtDecisionPoint();
    const fnvxr::engine::TrackedPropAssistRequest request =
        trackedPropAssistRequest();
    if (!fnvxr::engine::trackedPropAssistAuthorized(proof, request))
    {
        if (g_trackedPropAssistAuthorityAttempts <= 12u
            || (g_trackedPropAssistAuthorityAttempts % 300u) == 0u)
        {
            logTelemetry(
                "trackedPropAssist bridge deferred attempt=%lu compatible=%d failure=%u; no input, projectile, hit, renderer, replay, or OpenXR transaction performed\n",
                static_cast<unsigned long>(g_trackedPropAssistAuthorityAttempts),
                static_cast<int>(proof.compatible),
                static_cast<unsigned>(proof.failure));
        }
        return false;
    }
    if (!acquireGamePluginProducerLease())
        return false;

    // This profile maps only the host/fixture pose and read-only game
    // observations required to place the first-person visual rig.  It does
    // not map controller input, commands, input events, renderer state, or a
    // D3D/OpenXR origin transaction.
    initSharedVrPose();
    initSharedCamera();
    initSharedPlayer();
    if (!g_vrPoseState || !g_cameraState || !g_runtimeState || !g_playerState)
    {
        logTelemetry("trackedPropAssist bridge initialization deferred: required mapping unavailable\n");
        return false;
    }

    if (!installCameraHook()
        || !g_cameraHookInstalled
        || g_cameraHookAuthorization != CameraHookAuthorization::TrackedPropAssist
        || !installRetailRigHook()
        || !g_retailRigHookInstalled
        || !trackedPropAssistLeaseCurrent())
    {
        logTelemetry("trackedPropAssist bridge deferred: camera/rig hook was not authorized or installed\n");
        return false;
    }

    g_trackedPropAssistBridgeStarted = true;
    logTelemetry(
        "trackedPropAssist bridge ready attempt=%lu mode=body-anchored-visual-rig input=0 projectile=0 hit=0 renderer=0 replay=0 openxr=0\n",
        static_cast<unsigned long>(g_trackedPropAssistAuthorityAttempts));
    return true;
}

bool ensureAuthorizedHeadlessStereoRigVisualTrialBridgeStarted()
{
    if (!headsetControllerRigVisualTrialRequested())
        return false;

    if (g_headlessStereoRigVisualTrialBridgeStarted)
    {
        return gamePluginProducerLeaseHeldByCurrentThread()
            && g_vrPoseState
            && g_cameraState
            && g_runtimeState
            && g_playerState
            && g_retailRigHookInstalled
            && headlessStereoRigVisualTrialLeaseCurrent();
    }

    ++g_headlessStereoRigVisualTrialAuthorityAttempts;
    const fnvxr::engine::compatibility::RetailCompatibilityProof proof =
        fnvxr::engine::compatibility::proveCurrentRetailCompatibilityAtDecisionPoint();
    const fnvxr::engine::HeadlessStereoRigVisualTrialRequest request =
        headlessStereoRigVisualTrialRequest();
    if (!fnvxr::engine::headlessStereoRigVisualTrialAuthorized(proof, request))
    {
        if (g_headlessStereoRigVisualTrialAuthorityAttempts <= 12u
            || (g_headlessStereoRigVisualTrialAuthorityAttempts % 300u) == 0u)
        {
            logTelemetry(
                "headlessStereoRig bridge deferred attempt=%lu compatible=%d failure=%u; no input, projectile, hit, camera-hook, replay, UI, or physical-headset authority performed\n",
                static_cast<unsigned long>(
                    g_headlessStereoRigVisualTrialAuthorityAttempts),
                static_cast<int>(proof.compatible),
                static_cast<unsigned>(proof.failure));
        }
        return false;
    }
    if (!acquireGamePluginProducerLease())
        return false;

    // The host owns the OpenXR pose mapping and the D3D bridge owns camera and
    // same-tick stereo publication. The plugin normally maps only the pose
    // plus read-only runtime records needed by the post-animation visual rig.
    // The separately opted-in owned-fixture combat trial additionally maps
    // the host-owned controller records so its game-thread consumer can drive
    // gameplay, Pip-Boy, and native menu controls from this one bounded run.
    initSharedVrPose();
    initSharedCamera();
    initSharedPlayer();
    const bool combatVisualTrial =
        envEnabled("FNVXR_HEADSET_COMBAT_VISUAL_TRIAL", false);
    const bool inventoryVisualTrial =
        envEnabled("FNVXR_HEADSET_INVENTORY_VISUAL_TRIAL", false);
    const bool controllerInputTrial =
        combatVisualTrial || inventoryVisualTrial;
    if (controllerInputTrial)
    {
        initSharedXInput();
        initSharedDInput();
        if (inventoryVisualTrial)
            initSharedInputEvents();
    }
    if (!g_vrPoseState || !g_cameraState || !g_runtimeState || !g_playerState)
    {
        logTelemetry(
            "headlessStereoRig bridge initialization deferred: required mapping unavailable\n");
        return false;
    }

    if (!installRetailRigHook()
        || !g_retailRigHookInstalled
        || !headlessStereoRigVisualTrialLeaseCurrent())
    {
        logTelemetry(
            "headlessStereoRig bridge deferred: visual-rig hook was not authorized or installed\n");
        return false;
    }

    g_headlessStereoRigVisualTrialBridgeStarted = true;
    if (controllerInputTrial)
    {
        if (!g_xinputState || !g_dinputState
            || (inventoryVisualTrial && !g_inputEvents))
        {
            logTelemetry(
                "headlessStereoRig combat bridge deferred: controller mappings unavailable\n");
            g_headlessStereoRigVisualTrialBridgeStarted = false;
            return false;
        }
        // This is the same exact-current-process consumer acknowledgement
        // used by the full bridge. The visual-trial loop consumes the normal
        // mode-aware gameplay/Pip-Boy/menu controller route.
        InterlockedExchange8(
            reinterpret_cast<volatile char*>(
                &g_xinputState->reserved[
                    fnvxr::shared::XInputReservedRetailConsumed]),
            1);
    }
    logTelemetry(
        "headlessStereoRig bridge ready attempt=%lu mode=body-anchored-controller-weapon-visual-rig combatInput=%d inventoryInput=%d inputScope=%s projectile=0 hit=0 cameraHook=0 replay=0 physical=0 engineCenterStereo=external\n",
        static_cast<unsigned long>(g_headlessStereoRigVisualTrialAuthorityAttempts),
        static_cast<int>(combatVisualTrial),
        static_cast<int>(inventoryVisualTrial),
        controllerInputTrial ? "gameplay-pipboy-native-menu" : "none");
    return true;
}

bool ensureAuthorizedSharedBridgeStarted()
{
    // Assist profiles have deliberately smaller authority paths. Do not let a
    // missing opt-in fall through to mapping input, command, rig, or render
    // state via the full bridge.
    if (desktopAssistProfileSelected()
        || trackedPropAssistProfileSelected()
        || stereoVisualTrialProfileSelected())
        return false;

    if (g_authorizedSharedBridgeStarted)
    {
        return g_retailRuntimeAuthority.complete()
            && gamePluginProducerLeaseHeldByCurrentThread()
            && g_xinputState
            && g_dinputState
            && g_vrPoseState
            && g_cameraState
            && g_runtimeState
            && g_playerState
            && g_commandState
            && g_inputEvents;
    }

    if (!g_retailRuntimeAuthority.complete())
    {
        ++g_retailRuntimeAuthorityAttempts;
        const fnvxr::engine::RetailRuntimeAuthorityDecision decision =
            fnvxr::engine::authorizeCurrentRetailRuntimeAtDecisionPoint();
        if (!decision.complete())
        {
            if (g_retailRuntimeAuthorityAttempts <= 12
                || (g_retailRuntimeAuthorityAttempts % 300) == 0)
            {
                logTelemetry(
                    "shared bridge authority deferred attempt=%lu failure=%u revalidation=%u abi=%u compatibility=%u compatible=%d evidence=%d%d%d%d%d%d%d%d%d%d diagnostics=%zu,%zu,%zu,%zu; no game state read or mutation performed\n",
                    static_cast<unsigned long>(g_retailRuntimeAuthorityAttempts),
                    static_cast<unsigned>(decision.failure),
                    static_cast<unsigned>(decision.revalidation.failure),
                    static_cast<unsigned>(decision.revalidation.assessment.failure),
                    static_cast<unsigned>(
                        decision.revalidation.compatibilityProof.failure),
                    static_cast<int>(
                        decision.revalidation.compatibilityProof.compatible),
                    static_cast<int>(decision.revalidation.evidence.loadedExecutableIdentityMatched),
                    static_cast<int>(decision.revalidation.evidence.loadedExecutableSectionLayoutAndProtectionsVerified),
                    static_cast<int>(decision.revalidation.evidence.coreManifestMatched),
                    static_cast<int>(decision.revalidation.evidence.fullFunctionInventoryMatched),
                    static_cast<int>(decision.revalidation.evidence.vtableSlotsMatched),
                    static_cast<int>(decision.revalidation.evidence.vtableBlocksMatched),
                    static_cast<int>(decision.revalidation.evidence.liveObjectLayoutsVerified),
                    static_cast<int>(decision.revalidation.evidence.constructorOwnershipVerified),
                    static_cast<int>(decision.revalidation.evidence.bothWorldBranchesVerified),
                    static_cast<int>(decision.revalidation.evidence.compatibilityModulesVerified),
                    decision.revalidation.diagnostics.functionBodiesHashed,
                    decision.revalidation.diagnostics.vtableSlotsRead,
                    decision.revalidation.diagnostics.vtableBlockBytesHashed,
                    decision.revalidation.diagnostics.executableSectionBytesInspected);
            }
            return false;
        }
        g_retailRuntimeAuthority = decision;
        logTelemetry(
            "shared bridge exact retail authority acquired attempt=%lu generation=%llu\n",
            static_cast<unsigned long>(g_retailRuntimeAuthorityAttempts),
            static_cast<unsigned long long>(
                g_retailRuntimeAuthority.authority.metadata().generation));
    }

    if (!acquireGamePluginProducerLease())
        return false;

    // The OpenXR host owns these three mappings. The plugin maps them only as
    // controller/pose inputs and never initializes or publishes their bytes.
    initSharedXInput();
    initSharedDInput();
    initSharedVrPose();

    // These mappings are the plugin's authenticated observations of the
    // exact retail process. They are required by the renderer before any
    // world or UI transaction can be published.
    initSharedCamera();
    initSharedRuntime();
    initSharedPlayer();
    initSharedCommand();
    initSharedInputEvents();

    const bool mappingsReady = g_xinputState
        && g_dinputState
        && g_vrPoseState
        && g_cameraState
        && g_runtimeState
        && g_playerState
        && g_commandState
        && g_inputEvents;
    if (!mappingsReady)
    {
        logTelemetry("shared bridge initialization deferred: one or more mappings are unavailable\n");
        return false;
    }

    // Camera/rig mutation retain their independent, stricter source fuses.
    // A shared-state publisher becoming ready cannot authorize either hook.
    if (!installCameraHook() || !installRetailRigHook())
        return false;

    g_authorizedSharedBridgeStarted = startBridge();
    if (g_authorizedSharedBridgeStarted)
    {
        // This consumer-owned byte is reset by each newly leased OpenXR host
        // before retail starts. Publish it only after exact current-process
        // authority, all controller mappings, and the mode-aware game-thread
        // consumer are live.
        InterlockedExchange8(
            reinterpret_cast<volatile char*>(
                &g_xinputState->reserved[
                    fnvxr::shared::XInputReservedRetailConsumed]),
            1);
        logTelemetry(
            "shared bridge ready under exact current-process authority controllerConsumerAcknowledged=1 profile=%s\n",
            physicalHeadsetPlayProfileSelected()
                ? "retail-vr-play-v1"
                : "standard");
    }
    return g_authorizedSharedBridgeStarted;
}

void processMainGameLoop(const RuntimeObservation& observation)
{
    processShowroomCarousel();
    syncExternalDInputPointer(observation);
    consumeExternalDInputBridge(observation);
    consumeExternalXInputBridge(observation);

    const UInt64 frame = observation.frame;
    const UInt32 menuBits = observation.menuBits;
    const RuntimePhase phase = observation.phase;
    const bool uiInputAllowed = observation.uiInputAllowed;
    recoverFocusLossPause(frame, menuBits, phase);
    if (phase == RuntimePhase::Gameplay)
    {
        ensureAutoVanityCameraDisabled(frame);
        forceFirstPersonCameraMode("mainloop", frame);
    }
    else
        cancelThirdPersonL3Control("mainloop:ui", frame);
    updateSharedCamera(frame, menuBits);
    updateSharedPlayer(frame, phase);
    logCameraTelemetry(frame, menuBits);
    tickUiFavoriteAssignment(frame, menuBits);
    consumeSharedCommand(frame);
    UInt32 pending = g_pendingAcceptClicks.exchange(0);
    if (pending > 5)
        pending = 5;

    for (UInt32 index = 0; index < pending; ++index)
        executeAcceptClickOnGameThread();
}

void processDesktopAssistMainLoop(const RuntimeObservation& observation)
{
    // Keep the game entirely responsible for simulation, movement, menus,
    // and body heading.  This path observes only the camera/player results of
    // the camera-local pose transaction; it never consumes external input.
    updateSharedCamera(observation.frame, observation.menuBits);
    updateSharedPlayer(observation.frame, observation.phase);
    updateSharedDesktopAssist(observation.frame);
    logCameraTelemetry(observation.frame, observation.menuBits);
    // This function is intentionally the only desktop-assist command path.
    // It accepts the fixed recovery save only after the real Start Menu is
    // observed; all UI navigation during acceptance remains external and is
    // limited by the supervisor to two verified Escape taps.
    if (desktopAssistAutomationRequested())
        consumeDesktopAssistRecoveryLoad(
            observation.frame,
            observation.phase,
            observation.menuBits,
            observation.uiInputAllowed);
}

void processTrackedPropAssistMainLoop(const RuntimeObservation& observation)
{
    // Camera/player records are observational.  The only visual writes happen
    // later at the already-authorized post-animation rig hook; this main-loop
    // path does not consume external input, commands, or renderer state.
    updateSharedCamera(observation.frame, observation.menuBits);
    updateSharedPlayer(observation.frame, observation.phase);
    logCameraTelemetry(observation.frame, observation.menuBits);
}

void consumeHeadlessCombatVisualTrialInput(
    const RuntimeObservation& observation)
{
    static bool previousTriggerHeld = false;
    static bool previousReloadHeld = false;
    static UInt8 previousLocomotionMask = 0xffu;

    const bool authorized =
        envEnabled("FNVXR_HEADSET_COMBAT_VISUAL_TRIAL", false)
        && headlessStereoRigVisualTrialLeaseCurrent()
        && observation.phase == RuntimePhase::Gameplay
        && observation.menuBits == 0u;
    SharedXInputState state {};
    const bool haveInput = authorized
        && readSharedXInputFrameSnapshot(state)
        && state.connected != 0u;
    if (haveInput)
        applyHeadRelativeLocomotion(state, observation.frame);
    const bool rightTriggerHeld =
        haveInput && state.rightTrigger > 64u;
    const bool reloadHeld =
        haveInput && (state.buttons & XInputX) != 0u;
    const int movementDeadzone = std::clamp(
        getIntFromEnv("FNVXR_PLUGIN_MOVEMENT_DEADZONE", 9000),
        1000,
        30000);
    const auto requestedLocomotion =
        fnvxr::physical_input::classifyLocomotion(
            state.leftThumbX,
            state.leftThumbY,
            movementDeadzone);
    const UInt8 locomotionMask = static_cast<UInt8>(
        (requestedLocomotion.forward ? 0x1u : 0u)
        | (requestedLocomotion.backward ? 0x2u : 0u)
        | (requestedLocomotion.left ? 0x4u : 0u)
        | (requestedLocomotion.right ? 0x8u : 0u));

    // The headless acceptance harness must exercise the same final engine
    // consumer as physical play. Simulator acknowledgement or generated key
    // flags are not locomotion proof: PlayerMover receives the stick intent,
    // and the supervisor separately requires the player's world coordinates
    // to change before the run can pass.
    const bool playerMoverApplied = drivePhysicalPlayerMovement(
        requestedLocomotion,
        haveInput,
        gameplayAnalogRunHeld(state.leftThumbX, state.leftThumbY),
        observation.frame);
    if (haveInput)
        applyControllerSnapTurn(
            state.rightThumbX,
            observation.frame,
            "headless-simulator:right-stick");
    else
        g_physicalSnapTurnLatch.reset();
    if (locomotionMask != previousLocomotionMask)
    {
        previousLocomotionMask = locomotionMask;
        logTelemetry(
            "headlessLocomotion frame=%llu stick=(%d,%d) requested=0x%02x authorized=%d finalConsumer=PlayerMover::SetMovementFlags applied=%d\n",
            static_cast<unsigned long long>(observation.frame),
            static_cast<int>(state.leftThumbX),
            static_cast<int>(state.leftThumbY),
            static_cast<unsigned int>(locomotionMask),
            static_cast<int>(haveInput),
            static_cast<int>(playerMoverApplied));
    }

    struct CombatAmmoSnapshot
    {
        void* process {};
        void* ammoInfo {};
        void* weapon {};
        UInt32 loadedRounds {};
        bool valid {};
    };
    const auto readCombatAmmo = []() -> CombatAmmoSnapshot
    {
        CombatAmmoSnapshot result {};
        __try
        {
            void* player = readPointer(PlayerCharacterAddress);
            result.process = player
                ? readPointer(reinterpret_cast<std::uintptr_t>(player)
                    + MobileObjectBaseProcessOffset)
                : nullptr;
            if (!result.process)
                return result;
            void** processVtable = *reinterpret_cast<void***>(result.process);
            if (!processVtable
                || !processVtable[BaseProcessGetWeaponInfoVtableSlot]
                || !processVtable[BaseProcessGetAmmoInfoVtableSlot])
            {
                return result;
            }
            using GetAmmoInfoFn = void* (__thiscall*)(void*);
            result.ammoInfo = reinterpret_cast<GetAmmoInfoFn>(
                processVtable[BaseProcessGetAmmoInfoVtableSlot])(
                    result.process);
            if (!result.ammoInfo)
                return result;
            const auto ammoBase = reinterpret_cast<std::uintptr_t>(
                result.ammoInfo);
            result.loadedRounds = readUInt32(ammoBase + 0x04);
            // xNVSE Actor::GetEquippedWeapon reads EntryData::type at +0x08
            // from BaseProcess::GetWeaponInfo. AmmoInfo::weapon is not
            // populated for every player weapon and cannot be the authority.
            using GetWeaponInfoFn = void* (__thiscall*)(void*);
            void* weaponInfo = reinterpret_cast<GetWeaponInfoFn>(
                processVtable[BaseProcessGetWeaponInfoVtableSlot])(
                    result.process);
            result.weapon = weaponInfo
                ? readPointer(reinterpret_cast<std::uintptr_t>(weaponInfo)
                    + 0x08)
                : nullptr;
            result.valid = result.weapon != nullptr;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            result = {};
        }
        return result;
    };

    // Drive the real engine action state, not an OS/DInput queue. Fallout
    // consumes forceFireWeapon on its gameplay update even while its window is
    // deliberately kept in the background by the headless simulator run.
    if (rightTriggerHeld && !previousTriggerHeld)
    {
        const CombatAmmoSnapshot before = readCombatAmmo();
        bool applied = false;
        if (before.process && before.valid)
        {
            __try
            {
                *reinterpret_cast<UInt8*>(
                    reinterpret_cast<std::uintptr_t>(before.process)
                    + HighProcessForceFireWeaponOffset) = 1u;
                applied = true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                applied = false;
            }
        }
        logTelemetry(
            "primaryAttack edge frame=%llu source=headlessCombat:RT held=true applied=%s finalConsumer=HighProcess::forceFireWeapon loadedBefore=%lu process=%p weapon=%p\n",
            static_cast<unsigned long long>(observation.frame),
            applied ? "true" : "false",
            static_cast<unsigned long>(before.loadedRounds),
            before.process,
            before.weapon);
    }
    else if (!rightTriggerHeld && previousTriggerHeld)
    {
        // forceFireWeapon is a level-triggered engine request. Clear it on
        // the controller's release edge so the next squeeze is a distinct
        // semi-automatic shot instead of a permanently asserted request.
        const CombatAmmoSnapshot released = readCombatAmmo();
        if (released.process)
        {
            __try
            {
                *reinterpret_cast<UInt8*>(
                    reinterpret_cast<std::uintptr_t>(released.process)
                    + HighProcessForceFireWeaponOffset) = 0u;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }
    }
    previousTriggerHeld = rightTriggerHeld;
    if (reloadHeld && !previousReloadHeld)
    {
        const CombatAmmoSnapshot before = readCombatAmmo();
        bool reloadApplied = false;
        __try
        {
            void* player = readPointer(PlayerCharacterAddress);
            using ReloadFn = bool (__thiscall*)(
                void*, void*, int, UInt8, UInt8);
            ReloadFn reload = player
                ? *pointerFromAddress32<ReloadFn*>(
                    PlayerCharacterReloadVtableAddress)
                : nullptr;
            reloadApplied = reload && before.weapon
                && reload(player, before.weapon, 1, 0u, 0u);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            reloadApplied = false;
        }
        logTelemetry(
            "buttonX reloadEdge frame=%llu held=true applied=%s finalConsumer=PlayerCharacter::Reload loadedBefore=%lu weapon=%p source=headlessCombat:X\n",
            static_cast<unsigned long long>(observation.frame),
            reloadApplied ? "true" : "false",
            static_cast<unsigned long>(before.loadedRounds),
            before.weapon);
    }
    previousReloadHeld = reloadHeld;
}

void processHeadlessStereoRigVisualTrialMainLoop(
    const RuntimeObservation& observation)
{
    static bool inventoryMeleeSeedAttempted = false;
    if (!inventoryMeleeSeedAttempted
        && envEnabled("FNVXR_HEADSET_INVENTORY_VISUAL_TRIAL", false)
        && g_headsetDemoFixtureReady
        && realRetailFixtureFreshGameplayState(observation))
    {
        inventoryMeleeSeedAttempted = true;
        runPluginConsoleCommand(
            "fnvxrInventoryTrialSeedMelee",
            "player.additem 00004347 1");
    }
    // The D3D bridge retains camera and same-tick eye authority. This
    // fixture-only path publishes observations solely for the separately
    // leased post-animation controller/weapon visual rig.
    updateSharedCamera(observation.frame, observation.menuBits);
    updateSharedPlayer(observation.frame, observation.phase);
    logCameraTelemetry(observation.frame, observation.menuBits);
    if (envEnabled("FNVXR_HEADSET_COMBAT_VISUAL_TRIAL", false))
        consumeHeadlessCombatVisualTrialInput(observation);
    else if (envEnabled("FNVXR_HEADSET_INVENTORY_VISUAL_TRIAL", false))
        consumeExternalXInputBridge(observation);
}

void handleNvseMessage(NVSEMessagingInterface::Message* message)
{
    if (!message)
        return;

    if (message->type == MessageMainGameLoop)
    {
        const fnvxr::engine::RetailPluginMainLoopDisposition
            visualTrialDisposition =
                stereoVisualTrialMainLoopDisposition();
        if (visualTrialDisposition
            == fnvxr::engine::RetailPluginMainLoopDisposition::
                RejectVisualTrial)
        {
            static bool loggedVisualTrialOptInMissing = false;
            if (!loggedVisualTrialOptInMissing)
            {
                loggedVisualTrialOptInMissing = true;
                logTelemetry(
                    "stereo visual-trial profile selected but FNVXR_ENABLE_ENGINE_CENTER_STEREO is not enabled; no bridge or hook will start\n");
            }
            return;
        }
        if (retailFixtureProfileSelected()
            && !retailFixtureAutomationRequested())
        {
            static bool loggedRetailFixtureOptInMissing = false;
            if (!loggedRetailFixtureOptInMissing)
            {
                loggedRetailFixtureOptInMissing = true;
                logTelemetry(
                    "retail fixture profile selected without FNVXR_RETAIL_FIXTURE_AUTOMATION; no fixture, OpenXR, input, bridge, camera, or rig authority will start\n");
            }
            return;
        }
        if (headsetDemoFixtureProfileSelected()
            && !retailFixtureAutomationRequested())
        {
            static bool loggedHeadsetDemoFixtureOptInMissing = false;
            if (!loggedHeadsetDemoFixtureOptInMissing)
            {
                loggedHeadsetDemoFixtureOptInMissing = true;
                logTelemetry(
                    "headset demo fixture profile selected without FNVXR_RETAIL_FIXTURE_AUTOMATION; no fixture, OpenXR, input, bridge, camera, or rig authority will start\n");
            }
            return;
        }
        if (desktopAssistProfileSelected() && !desktopAssistProfileRequested())
        {
            static bool loggedDesktopAssistOptInMissing = false;
            if (!loggedDesktopAssistOptInMissing)
            {
                loggedDesktopAssistOptInMissing = true;
                logTelemetry(
                    "desktopAssist profile selected but FNVXR_DESKTOP_ASSIST_CAMERA_ONLY is not enabled; no bridge or hook will start\n");
            }
            return;
        }
        if (trackedPropAssistProfileSelected() && !trackedPropAssistProfileRequested())
        {
            static bool loggedTrackedPropAssistOptInMissing = false;
            if (!loggedTrackedPropAssistOptInMissing)
            {
                loggedTrackedPropAssistOptInMissing = true;
                logTelemetry(
                    "trackedPropAssist profile selected but FNVXR_TRACKED_PROP_ASSIST_VISUAL_ONLY is not enabled; no bridge or hook will start\n");
            }
            return;
        }
        if (physicalHeadsetPlayProfileSelected()
            && !envEnabled("FNVXR_PHYSICAL_HEADSET_PLAY", false))
        {
            static bool loggedPhysicalPlayOptInMissing = false;
            if (!loggedPhysicalPlayOptInMissing)
            {
                loggedPhysicalPlayOptInMissing = true;
                logTelemetry(
                    "physical headset play profile selected without FNVXR_PHYSICAL_HEADSET_PLAY; no bridge, controller, camera, rig, or renderer authority will start\n");
            }
            return;
        }
        if (!ensureAuthorizedRuntimeObservationStarted())
            return;
        // The D3D bridge is intentionally gated on the first published
        // runtime frame. Acquire the headless controller-rig lease before
        // publishing that frame so both independent compatibility proofs see
        // pristine retail code. Publishing first creates a race in which the
        // D3D bridge can install its audited callsite hook just before this
        // plugin hashes the same protected function.
        if (headsetDemoFixtureProfileSelected()
            && headsetControllerRigVisualTrialRequested()
            && !g_headlessStereoRigVisualTrialBridgeStarted
            && !ensureAuthorizedHeadlessStereoRigVisualTrialBridgeStarted())
        {
            static bool loggedPrePublicationHeadlessRig = false;
            if (!loggedPrePublicationHeadlessRig)
            {
                loggedPrePublicationHeadlessRig = true;
                logTelemetry(
                    "headlessStereoRig pre-publication lease deferred; runtime frame remains unpublished so D3D cannot race the compatibility proof\n");
            }
            return;
        }
        const RuntimeObservation observation = observeAndPublishRuntime();
        if (ttwBaselineProfileSelected())
        {
            static bool loggedTtwBaselinePublicationOnly = false;
            if (!loggedTtwBaselinePublicationOnly)
            {
                loggedTtwBaselinePublicationOnly = true;
                logTelemetry(
                    "TTW baseline runtime publication ready; menu observation only and no save, console, bridge, OpenXR, simulator, input, camera, rig, renderer, or weapon authority is active\n");
            }
            return;
        }
        if (retailFixtureProfileSelected())
        {
            processRetailFixtureAutomation(observation);
            static bool loggedRetailFixturePublicationOnly = false;
            if (!loggedRetailFixturePublicationOnly)
            {
                loggedRetailFixturePublicationOnly = true;
                logTelemetry(
                    "retail fixture runtime publication ready; no OpenXR, simulator, bridge, input, camera, rig, renderer, or weapon authority is active\n");
            }
            return;
        }
        if (headsetDemoFixtureProfileSelected())
        {
            processRetailFixtureAutomation(observation);
            processHeadsetWorldOnlyFixtureWeaponDraw(observation);
            if (readOnlyFirstPersonSemanticsRequested())
                updateSharedPlayer(observation.frame, observation.phase);
            recoverFocusLossPause(
                observation.frame,
                observation.menuBits,
                observation.phase);
            processHeadsetDemoFixtureUi(observation);
            if (headsetControllerRigVisualTrialRequested())
            {
                if (!ensureAuthorizedHeadlessStereoRigVisualTrialBridgeStarted())
                {
                    static bool loggedHeadlessStereoRig = false;
                    if (!loggedHeadlessStereoRig)
                    {
                        loggedHeadlessStereoRig = true;
                        logTelemetry(
                            "headlessStereoRig bridge deferred until exact compatibility proof and visual-rig lease are ready\n");
                    }
                    return;
                }
                processHeadlessStereoRigVisualTrialMainLoop(observation);
            }
            static bool loggedHeadsetFixturePublicationOnly = false;
            if (!loggedHeadsetFixturePublicationOnly)
            {
                loggedHeadsetFixturePublicationOnly = true;
                if (headsetWorldOnlyCaptureProfileSelected())
                {
                    if (headsetWorldOnlyFixtureWeaponDrawRequested())
                    {
                        if (headsetControllerRigVisualTrialRequested())
                        {
                            logTelemetry(envEnabled("FNVXR_HEADSET_COMBAT_VISUAL_TRIAL", false)
                                ? "headset world-only fixture controller proof ready; engine-center stereo and host display remain separately owned; bounded native gameplay, Pip-Boy, and menu controller input enabled; no desktop, window, mouse, simulator-GUI, camera-hook, replay, or physical-headset path is enabled\n"
                                : "headset world-only fixture runtime publication ready; engine-center stereo and host display remain separately owned, while the fixture-only lease may apply controller-driven hand/weapon visual transforms; no input, firing, projectile, hit, camera-hook, replay, UI, or physical-headset path is enabled\n");
                        }
                        else
                        {
                            logTelemetry(readOnlyFirstPersonSemanticsRequested()
                                ? "stock first-person baseline publication ready; read-only player/weapon/root semantics feed the complete stock RenderFirstPerson lease; manual roots, controller rig, IK, Pip-Boy transforms, input, and window control remain disabled\n"
                                : "headset world-only fixture runtime publication ready; OpenXR display remains host-owned and the fixture finisher is limited to one save of the same owned loaded fixture after exact stock-notice settlement plus one fixed JIP SetWeaponOut command for its named weapon; no Pip-Boy, desktop, keyboard, controller, mouse, simulator, camera, or rig input is enabled\n");
                        }
                    }
                    else
                    {
                        logTelemetry(
                            "headset world-only fixture runtime publication ready; OpenXR display remains host-owned and no Pip-Boy, controller, keyboard, mouse, weapon, or simulator input is enabled\n");
                    }
                }
                else
                {
                    logTelemetry(
                        "headset demo fixture runtime publication ready; OpenXR display remains host-owned and the only in-game events are the fixed Pip-Boy open/close taps\n");
                }
            }
            return;
        }
        if (visualTrialDisposition
            == fnvxr::engine::RetailPluginMainLoopDisposition::
                PublishRuntimeOnly)
        {
            if (stereoVisualTrialAutomationRequested())
                processStereoVisualTrialFixedSaveAutomation(observation);
            static bool loggedVisualTrialPublicationOnly = false;
            if (!loggedVisualTrialPublicationOnly)
            {
                loggedVisualTrialPublicationOnly = true;
                logTelemetry(
                    "stereo visual trial runtime publication ready; plugin full bridge, input, camera, and rig hooks remain disabled for D3D world authority\n");
            }
            return;
        }
        if (desktopAssistProfileRequested())
        {
            if (!ensureAuthorizedDesktopAssistBridgeStarted())
            {
                static bool loggedDesktopAssist = false;
                if (!loggedDesktopAssist)
                {
                    loggedDesktopAssist = true;
                    logTelemetry("desktopAssist bridge deferred until exact compatibility proof and rotation-only camera lease are ready\n");
                }
                return;
            }
            processDesktopAssistMainLoop(observation);
            return;
        }
        if (trackedPropAssistProfileRequested())
        {
            if (!ensureAuthorizedTrackedPropAssistBridgeStarted())
            {
                static bool loggedTrackedPropAssist = false;
                if (!loggedTrackedPropAssist)
                {
                    loggedTrackedPropAssist = true;
                    logTelemetry("trackedPropAssist bridge deferred until exact compatibility proof and visual-rig lease are ready\n");
                }
                return;
            }
            processTrackedPropAssistMainLoop(observation);
            return;
        }
        if (physicalHeadsetPlayProfileSelected()
            && physicalHeadsetFixtureMessageAcknowledgementRequested())
        {
            // The physical profile retains its game-loop bridge and ordinary
            // controller ownership.  Only the independently bounded exact
            // fixture-message handler runs before that bridge starts.
            processOwnedRetailFixtureMessageMenuAcknowledgements(observation);
        }
        if (!ensureAuthorizedSharedBridgeStarted())
        {
            static bool logged = false;
            if (!logged)
            {
                logged = true;
                logTelemetry("mainloop bridge deferred until exact retail authority and shared mappings are ready\n");
            }
            return;
        }
        processMainGameLoop(observation);
    }
}

void tapKey(WORD virtualKey)
{
    if (windowsForegroundInputForbidden()
        || !currentProcessHasForegroundWindow())
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
    updateLivePipBoyInteraction(
        pose,
        menuBits,
        rightTriggerPressed);
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
    const float liveDeviceU = static_cast<float>(pose.poseReserved[
        fnvxr::PoseReservedPipBoyDeviceU]) / 255.0f;
    const float liveDeviceV = static_cast<float>(pose.poseReserved[
        fnvxr::PoseReservedPipBoyDeviceV]) / 255.0f;
    const bool pointerTargetsLiveScreen = !pipBoyVisible
        || fnvxr::engine::live_pipboy::physicalControl(
                liveDeviceU,
                liveDeviceV)
            == fnvxr::engine::live_pipboy::PhysicalControl::Screen;
    const bool pointerClickPressed = uiInputAllowed
        && rightTriggerPressed
        && pose.menuPointerActive
        && pointerTargetsLiveScreen;
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
    const bool weaponOrbitHeld =
        (pose.poseReserved[fnvxr::PoseReservedInteractionFlags]
            & fnvxr::PoseInteractionWeaponOrbitActive) != 0u;
    if (!uiInputAllowed && !weaponOrbitHeld
        && (pressed & fnvxr::RightThumbstick))
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
        UInt32 topMenuType = 0;
        visibleMenuForInput(nullptr, &topMenuType);
        const bool handled = (topMenuType == kMenuTypeStart
                && directMenuCancel("pose:B:start"))
            || (pipBoyVisible && closeEnginePipBoy("pose:B", pose.frame))
            || directMenuCancel("pose:B:fallback");
        logTelemetry(
            "menuBack fire frame=%llu source=B key=0x%02lx pipBoy=%d handled=%d\n",
            static_cast<unsigned long long>(pose.frame),
            static_cast<unsigned long>(backKey),
            static_cast<int>(pipBoyVisible),
            static_cast<int>(handled));
        if (!handled)
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
        else if (!uiInputAllowed
            && gameplayKeyboardFallback
            && leftGripModifierDown
            && envEnabled("FNVXR_PIPBOY_MENU_CHORD_ENABLE", true))
        {
            const bool tapped = tapDirectInputKey(DIK_TAB);
            logTelemetry(
                "pipboyToggle fire frame=%llu source=pose:LG+LeftMenu key=0x%02lx gameplay=1 tapped=%d\n",
                static_cast<unsigned long long>(pose.frame),
                static_cast<unsigned long>(DIK_TAB),
                static_cast<int>(tapped));
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

extern "C" __declspec(dllexport) bool __cdecl
FNVXR_ApplyWeaponFrameForRender(
    const fnvxr::shared::SharedVrPoseState* source,
    LONG poseSequence)
{
    if (!source
        || poseSequence == 0
        || (!physicalHeadsetEngineCenterRigRequested()
            && !headsetControllerRigVisualTrialRequested())
        || !g_lastRetailRigAnimData)
    {
        return false;
    }
    VrRigPoseSnapshot pose {};
    __try
    {
        pose.sequence = poseSequence;
        pose.frame = source->frame;
        pose.predictedDisplayTime = source->predictedDisplayTime;
        pose.trackingFlags = source->trackingFlags;
        pose.referenceSpaceGeneration = source->referenceSpaceGeneration;
        pose.producerEpoch = source->producerEpoch;
        pose.recenterRequestId = source->recenterRequestId;
        pose.hmdRot = { source->hmdRot[0], source->hmdRot[1], source->hmdRot[2], source->hmdRot[3] };
        pose.hmdPos = { source->hmdPos[0], source->hmdPos[1], source->hmdPos[2] };
        pose.leftRot = { source->leftRot[0], source->leftRot[1], source->leftRot[2], source->leftRot[3] };
        pose.leftPos = { source->leftPos[0], source->leftPos[1], source->leftPos[2] };
        pose.rightRot = { source->rightRot[0], source->rightRot[1], source->rightRot[2], source->rightRot[3] };
        pose.rightPos = { source->rightPos[0], source->rightPos[1], source->rightPos[2] };
        pose.leftAimRot = { source->leftAimRot[0], source->leftAimRot[1], source->leftAimRot[2], source->leftAimRot[3] };
        pose.leftAimPos = { source->leftAimPos[0], source->leftAimPos[1], source->leftAimPos[2] };
        pose.rightAimRot = { source->rightAimRot[0], source->rightAimRot[1], source->rightAimRot[2], source->rightAimRot[3] };
        pose.rightAimPos = { source->rightAimPos[0], source->rightAimPos[1], source->rightAimPos[2] };
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    if (pose.referenceSpaceGeneration == 0
        || pose.producerEpoch == 0
        || pose.predictedDisplayTime <= 0
        || !finiteVec3(pose.hmdPos)
        || !finiteVec3(pose.rightPos)
        || !finiteVec3(pose.rightAimPos)
        || !finiteUsableQuat(pose.hmdRot)
        || !finiteUsableQuat(pose.rightRot)
        || !finiteUsableQuat(pose.rightAimRot))
    {
        return false;
    }
    pose.hmdRot = normalizeQuat(pose.hmdRot);
    pose.leftRot = normalizeQuat(pose.leftRot);
    pose.rightRot = normalizeQuat(pose.rightRot);
    pose.leftAimRot = normalizeQuat(pose.leftAimRot);
    pose.rightAimRot = normalizeQuat(pose.rightAimRot);
    // Both the center render and the later first-person seam can arrive with
    // this exact OpenXR pose. Keep one controller-owned weapon state for that
    // pose; only solve again when Fallout has actually put the weapon back
    // into its stock animation before the draw.
    const auto committedPoseStillOwnsLiveTransforms = [&]() noexcept
    {
        SharedWeaponFrameState* state = sharedWeaponFrameState();
        if (!state || !g_retailRigNodes.right.hand || !g_retailRigNodes.weapon)
            return false;
        const LONG before = InterlockedCompareExchange(
            &state->producerSequence, 0, 0);
        if (!fnvxr::shared::sequencedValueIsPublished(before))
            return false;
        MemoryBarrier();
        const bool identityMatches =
            state->status == fnvxr::shared::WeaponFramePoseCommitted
            && state->poseSequence == static_cast<UInt32>(poseSequence)
            && state->poseFrame == pose.frame
            && state->rightHandAddress == static_cast<UInt32>(
                reinterpret_cast<std::uintptr_t>(g_retailRigNodes.right.hand))
            && state->weaponAddress == static_cast<UInt32>(
                reinterpret_cast<std::uintptr_t>(g_retailRigNodes.weapon));
        const Vec3 hand = readVec3(
            reinterpret_cast<std::uintptr_t>(g_retailRigNodes.right.hand)
                + NiAvObjectWorldTranslationOffset);
        const Vec3 weapon = readVec3(
            reinterpret_cast<std::uintptr_t>(g_retailRigNodes.weapon)
                + NiAvObjectWorldTranslationOffset);
        const Matrix33 weaponRotation = readMatrix33(
            reinterpret_cast<std::uintptr_t>(g_retailRigNodes.weapon)
                + NiAvObjectWorldRotationOffset);
        const float liveHand[3] { hand.x, hand.y, hand.z };
        const float liveWeapon[3] { weapon.x, weapon.y, weapon.z };
        const float liveRotation[9] {
            weaponRotation.m[0][0], weaponRotation.m[0][1], weaponRotation.m[0][2],
            weaponRotation.m[1][0], weaponRotation.m[1][1], weaponRotation.m[1][2],
            weaponRotation.m[2][0], weaponRotation.m[2][1], weaponRotation.m[2][2],
        };
        MemoryBarrier();
        const LONG after = InterlockedCompareExchange(
            &state->producerSequence, 0, 0);
        const bool transformStillOwned = hostSpatialPropReplacementRequested()
            ? fnvxr::weapon_frame::committedPoseOwnsLiveWeaponTransform(
                liveWeapon,
                state->weaponWorldPos,
                liveRotation,
                state->weaponWorldRot,
                0.02f,
                0.001f)
            : fnvxr::weapon_frame::committedPoseOwnsLiveRigTransforms(
                liveHand,
                state->rightHandWorldPos,
                liveWeapon,
                state->weaponWorldPos,
                liveRotation,
                state->weaponWorldRot,
                0.02f,
                0.02f,
                0.001f);
        return before == after && identityMatches && transformStillOwned;
    };
    // Reapply the complete prop set at this exact render boundary every time.
    // A right-hand/weapon commit alone cannot prove that stock animation did
    // not subsequently hide or restore the opposite hand and wrist device.
    g_renderRigPoseOverride = pose;
    g_renderRigPoseOverrideActive = true;
    onRetailPostAnimation(g_lastRetailRigAnimData);
    g_renderRigPoseOverrideActive = false;
    const bool completeTrackedPropsRestored =
        hostSpatialPropReplacementRequested()
        || (g_latestCompleteTrackedPropsApplied
            && g_latestCompleteTrackedPropsPoseSequence == poseSequence
            && g_latestCompleteTrackedPropsPoseFrame == pose.frame);
    const bool restored = committedPoseStillOwnsLiveTransforms()
        && completeTrackedPropsRestored;
    static LONG restoreAttempts = 0;
    static LONG restoreFailures = 0;
    const LONG attempt = InterlockedIncrement(&restoreAttempts);
    LONG failure = 0;
    if (!restored)
        failure = InterlockedIncrement(&restoreFailures);
    if (attempt <= 12 || attempt % 120 == 0 || (!restored && failure <= 12))
    {
        logTelemetry(
            "retailRig render-bound restore attempt=%ld failure=%ld restored=%d poseSeq=%ld poseFrame=%llu\n",
            attempt,
            failure,
            restored ? 1 : 0,
            poseSequence,
            static_cast<unsigned long long>(pose.frame));
    }
    return restored;
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

extern "C" __declspec(dllexport) bool FNVXR_RetailMutationProofComplete()
{
    return retailMutationAllowedForCurrentProcess(true);
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
    // Never acquire full retail authority from NVSEPlugin_Load. The D3D
    // Present bootstrap may already exist, so the first authenticated
    // main-loop observation must run and advance the plugin-owned runtime
    // mapping from neutral before either side can install a world hook.
    if (desktopAssistProfileSelected())
    {
        logTelemetry(
            "desktopAssist profile selected: deferring to camera-only main-loop authority; full bridge is disabled\n");
    }
    else if (trackedPropAssistProfileSelected())
    {
        logTelemetry(
            "trackedPropAssist profile selected: deferring to visual-rig main-loop authority; full bridge, input, projectile, renderer, replay, and OpenXR are disabled\n");
    }
    else if (retailFixtureProfileSelected())
    {
        logTelemetry(
            "retail fixture profile selected: xNVSE fixture lifecycle only; OpenXR, simulator, D3D bridge, input, camera, rig, renderer, and weapon authority are disabled\n");
    }
    else if (ttwBaselineProfileSelected())
    {
        logTelemetry(
            "TTW baseline profile selected: Start Menu observation only; save, console, OpenXR, simulator, D3D bridge, input, camera, rig, renderer, and weapon authority are disabled\n");
    }
    else if (stereoVisualTrialProfileSelected())
    {
        logTelemetry(
            "stereo visual-trial profile selected: deferring to publication-only main-loop authority; plugin full bridge, input, camera, and rig hooks are disabled\n");
    }
    else
    {
        logTelemetry(
            "full bridge authority deferred until a plugin-owned runtime publication advances from neutral on the main loop\n");
    }
    return g_pluginHandle != InvalidPluginHandle;
}

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_DETACH)
    {
        // NVSE loads this plugin for the lifetime of FalloutNV.exe. DllMain is
        // under the loader lock and can run on a non-owner thread, so hook
        // restoration, input publication, logging, unmapping, and mutex
        // release are forbidden here. Process teardown reclaims all handles;
        // dynamic unload without an explicit owner-thread shutdown protocol is
        // intentionally unsupported.
        return TRUE;
#if 0
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
        if (g_gamePluginProducerMutex)
        {
            // Win32 mutex ownership is thread-affine. Release only from the
            // NVSE load/main thread that acquired the lifetime lease; process
            // termination otherwise abandons it safely after all writers stop.
            if (g_gamePluginProducerMutexOwned
                && g_gamePluginProducerThreadId == GetCurrentThreadId())
            {
                ReleaseMutex(g_gamePluginProducerMutex);
            }
            g_gamePluginProducerMutexOwned = false;
            g_gamePluginProducerThreadId = 0;
            CloseHandle(g_gamePluginProducerMutex);
            g_gamePluginProducerMutex = nullptr;
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
        if (g_commandWriterMutex)
        {
            CloseHandle(g_commandWriterMutex);
            g_commandWriterMutex = nullptr;
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
        if (g_inputEventWriterMutex)
        {
            CloseHandle(g_inputEventWriterMutex);
            g_inputEventWriterMutex = nullptr;
        }
        g_nvse = nullptr;
        g_console = nullptr;
        g_messaging = nullptr;
        g_playerControls = nullptr;
        g_pluginHandle = InvalidPluginHandle;
#endif
    }

    return TRUE;
}
