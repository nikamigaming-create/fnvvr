#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11

#include "fnvxr_protocol.h"
#include "fnvxr_shared_state.h"
#include "fnvxr_stereo_math.h"

#include <windows.h>
#include <tlhelp32.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <DirectXMath.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace
{
struct OpenXr
{
    HMODULE loader = nullptr;
    PFN_xrGetInstanceProcAddr getInstanceProcAddr = nullptr;
    PFN_xrCreateInstance createInstance = nullptr;
    PFN_xrEnumerateInstanceExtensionProperties enumerateExtensions = nullptr;

    PFN_xrDestroyInstance destroyInstance = nullptr;
    PFN_xrGetSystem getSystem = nullptr;
    PFN_xrGetD3D11GraphicsRequirementsKHR getD3D11GraphicsRequirementsKHR = nullptr;
    PFN_xrCreateSession createSession = nullptr;
    PFN_xrDestroySession destroySession = nullptr;
    PFN_xrPollEvent pollEvent = nullptr;
    PFN_xrEnumerateViewConfigurationViews enumerateViewConfigurationViews = nullptr;
    PFN_xrLocateViews locateViews = nullptr;
    PFN_xrBeginSession beginSession = nullptr;
    PFN_xrEndSession endSession = nullptr;
    PFN_xrWaitFrame waitFrame = nullptr;
    PFN_xrBeginFrame beginFrame = nullptr;
    PFN_xrEndFrame endFrame = nullptr;
    PFN_xrEnumerateSwapchainFormats enumerateSwapchainFormats = nullptr;
    PFN_xrCreateSwapchain createSwapchain = nullptr;
    PFN_xrDestroySwapchain destroySwapchain = nullptr;
    PFN_xrEnumerateSwapchainImages enumerateSwapchainImages = nullptr;
    PFN_xrAcquireSwapchainImage acquireSwapchainImage = nullptr;
    PFN_xrWaitSwapchainImage waitSwapchainImage = nullptr;
    PFN_xrReleaseSwapchainImage releaseSwapchainImage = nullptr;
    PFN_xrCreateReferenceSpace createReferenceSpace = nullptr;
    PFN_xrDestroySpace destroySpace = nullptr;
    PFN_xrLocateSpace locateSpace = nullptr;
    PFN_xrStringToPath stringToPath = nullptr;
    PFN_xrPathToString pathToString = nullptr;
    PFN_xrCreateActionSet createActionSet = nullptr;
    PFN_xrDestroyActionSet destroyActionSet = nullptr;
    PFN_xrCreateAction createAction = nullptr;
    PFN_xrDestroyAction destroyAction = nullptr;
    PFN_xrCreateActionSpace createActionSpace = nullptr;
    PFN_xrSuggestInteractionProfileBindings suggestInteractionProfileBindings = nullptr;
    PFN_xrGetCurrentInteractionProfile getCurrentInteractionProfile = nullptr;
    PFN_xrAttachSessionActionSets attachSessionActionSets = nullptr;
    PFN_xrSyncActions syncActions = nullptr;
    PFN_xrGetActionStateFloat getActionStateFloat = nullptr;
    PFN_xrGetActionStateBoolean getActionStateBoolean = nullptr;
    PFN_xrGetActionStateVector2f getActionStateVector2f = nullptr;
    PFN_xrGetActionStatePose getActionStatePose = nullptr;
};

struct XrInputActions
{
    XrAction handPose = XR_NULL_HANDLE;
    XrAction aimPose = XR_NULL_HANDLE;
    XrAction leftTrigger = XR_NULL_HANDLE;
    XrAction rightTrigger = XR_NULL_HANDLE;
    XrAction leftSqueeze = XR_NULL_HANDLE;
    XrAction rightSqueeze = XR_NULL_HANDLE;
    XrAction buttonA = XR_NULL_HANDLE;
    XrAction buttonB = XR_NULL_HANDLE;
    XrAction buttonX = XR_NULL_HANDLE;
    XrAction buttonY = XR_NULL_HANDLE;
    XrAction leftMenu = XR_NULL_HANDLE;
    XrAction rightMenu = XR_NULL_HANDLE;
    XrAction leftThumbstick = XR_NULL_HANDLE;
    XrAction rightThumbstick = XR_NULL_HANDLE;
    XrAction leftThumbstickAxis = XR_NULL_HANDLE;
    XrAction rightThumbstickAxis = XR_NULL_HANDLE;
};

struct FloatActionSample
{
    float value = 0.0f;
    bool active = false;
};

struct BooleanActionSample
{
    bool value = false;
    bool active = false;
};

struct Vector2ActionSample
{
    float x = 0.0f;
    float y = 0.0f;
    bool active = false;
};

struct QuadSwapchain
{
    XrSwapchain handle = XR_NULL_HANDLE;
    int32_t width = 512;
    int32_t height = 512;
    DXGI_FORMAT dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    std::vector<XrSwapchainImageD3D11KHR> images;
    std::vector<ComPtr<ID3D11RenderTargetView>> renderTargetViews;
    ComPtr<ID3D11Texture2D> depthTexture;
    ComPtr<ID3D11DepthStencilView> depthStencilView;
};

struct MenuPointer
{
    bool active = false;
    float x = -1.0f;
    float y = -1.0f;
};

struct HeadspaceLook
{
    bool active = false;
    float yawRadians = 0.0f;
    float pitchRadians = 0.0f;
    std::int32_t yawMicroradians = 0;
    std::int32_t pitchMicroradians = 0;
};

struct HeadspaceLookTracker
{
    bool hasLast = false;
    float yawRadians = 0.0f;
    float pitchRadians = 0.0f;
};

struct HandspaceLookTracker
{
    bool hasLast = false;
    float x = 0.5f;
    float y = 0.5f;
};

struct SolvedArm
{
    XrPosef shoulder {};
    XrPosef elbow {};
    XrPosef wrist {};
    XrPosef forearm {};
    float forearmLength = 0.0f;
};

struct BodyRig
{
    SolvedArm left {};
    SolvedArm right {};
};

struct GamePlane
{
    XrPosef pose {};
    float width = 2.4f;
    float height = 1.35f;
};

struct Vertex
{
    float x;
    float y;
    float z;
};

struct TexturedVertex
{
    float x;
    float y;
    float z;
    float u;
    float v;
};

struct Constants
{
    XMFLOAT4X4 mvp;
    XMFLOAT4 color;
};

struct Renderer
{
    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader> pixelShader;
    ComPtr<ID3D11InputLayout> inputLayout;
    ComPtr<ID3D11Buffer> vertexBuffer;
    ComPtr<ID3D11Buffer> constantBuffer;
    ComPtr<ID3D11VertexShader> texturedVertexShader;
    ComPtr<ID3D11PixelShader> texturedPixelShader;
    ComPtr<ID3D11PixelShader> centerNoHudPixelShader;
    ComPtr<ID3D11PixelShader> hudOverlayPixelShader;
    ComPtr<ID3D11PixelShader> edgeSmearPixelShader;
    ComPtr<ID3D11InputLayout> texturedInputLayout;
    ComPtr<ID3D11Buffer> texturedVertexBuffer;
    ComPtr<ID3D11Buffer> curvedTexturedVertexBuffer;
    UINT curvedTexturedVertexCount = 0;
    ComPtr<ID3D11Buffer> shieldCenterTexturedVertexBuffer;
    UINT shieldCenterTexturedVertexCount = 0;
    ComPtr<ID3D11Buffer> surroundTexturedVertexBuffer;
    UINT surroundTexturedVertexCount = 0;
    ComPtr<ID3D11Buffer> edgeSmearTexturedVertexBuffer;
    UINT edgeSmearTexturedVertexCount = 0;
    ComPtr<ID3D11Texture2D> gameTexture;
    ComPtr<ID3D11ShaderResourceView> gameTextureView;
    ComPtr<ID3D11Texture2D> worldTexture;
    ComPtr<ID3D11ShaderResourceView> worldTextureView;
    ComPtr<ID3D11ShaderResourceView> pauseSceneTextureView;
    std::array<ComPtr<ID3D11ShaderResourceView>, 5> pauseSceneTextureViews;
    std::array<ComPtr<ID3D11Texture2D>, 2> stereoGameTextures;
    std::array<ComPtr<ID3D11ShaderResourceView>, 2> stereoGameTextureViews;
    ComPtr<ID3D11SamplerState> gameSampler;
    ComPtr<ID3D11BlendState> alphaBlendState;
    ComPtr<ID3D11RasterizerState> rasterizerState;
    ComPtr<ID3D11DepthStencilState> depthState;
    ComPtr<ID3D11DepthStencilState> gamePlaneDepthState;
    int gameTextureWidth = 2048;
    int gameTextureHeight = 1280;
    float gameTextureAspect = 16.0f / 9.0f;
    int pauseSceneTextureWidth = 0;
    int pauseSceneTextureHeight = 0;
    float pauseSceneTextureAspect = 16.0f / 9.0f;
    std::array<float, 5> pauseSceneTextureAspects {};
    size_t pauseSceneTextureCount = 0;
    bool hasPauseSceneTexture = false;
    HANDLE sharedVideoMapping = nullptr;
    std::uint8_t* sharedVideoView = nullptr;
    LONG lastSharedVideoSequence = 0;
    HANDLE sharedWorldVideoMapping = nullptr;
    std::uint8_t* sharedWorldVideoView = nullptr;
    LONG lastSharedWorldVideoSequence = 0;
    HANDLE sharedStereoMapping = nullptr;
    std::uint8_t* sharedStereoView = nullptr;
    LONG lastSharedStereoSequence = 0;
    HANDLE sharedRuntimeMapping = nullptr;
    fnvxr::shared::SharedRuntimeState* sharedRuntimeState = nullptr;
    bool hasGameTextureFrame = false;
    uint64_t gameTextureUpdates = 0;
    uint64_t gameTextureMisses = 0;
    bool hasWorldTextureFrame = false;
    uint64_t worldTextureUpdates = 0;
    uint64_t worldTextureMisses = 0;
    bool hasStereoGameFrame = false;
    bool stereoGameFrameSeparated = false;
    bool stereoGameFrameWorldCandidate = false;
    LONG stereoGameFrameSequence = 0;
    LONG stereoGamePoseSequence = 0;
    XrTime stereoGameRenderedDisplayTime = 0;
    std::uint32_t stereoReferenceSpaceGeneration = 0;
    std::uint32_t stereoProducerEpoch = 0;
    std::array<XrView, 2> stereoSourceViews {
        XrView { XR_TYPE_VIEW },
        XrView { XR_TYPE_VIEW }
    };
    uint64_t stereoStableFrameCount = 0;
    bool hasCachedSceneFrame = false;
    uint64_t lastStereoFrameUpdated = 0;
    uint64_t stereoFrameMisses = 0;
    uint64_t stereoTransientReadMisses = 0;
    std::chrono::steady_clock::time_point stereoAcceptedAt {};
    size_t stereoPixelSamples = 0;
    size_t stereoNonBlackSamples = 0;
    size_t stereoMeaningfulDifferentSamples = 0;
    size_t stereoLeftActiveTiles = 0;
    size_t stereoRightActiveTiles = 0;
    size_t stereoDifferentTiles = 0;
    std::uint32_t stereoLeftHash = 0;
    std::uint32_t stereoRightHash = 0;
};

constexpr DWORD SharedVideoMagic = fnvxr::shared::D3D9FrameSharedMagic;
constexpr DWORD SharedStereoMagic = fnvxr::shared::D3D9StereoFrameSharedMagic;
constexpr UINT SharedVideoMaxWidth = fnvxr::shared::D3D9SharedFrameMaxWidth;
constexpr UINT SharedVideoMaxHeight = fnvxr::shared::D3D9SharedFrameMaxHeight;

using SharedVideoHeader = fnvxr::shared::SharedD3D9FrameHeader;
using SharedStereoHeader = fnvxr::shared::SharedD3D9StereoFrameHeader;

struct StereoPixelStats
{
    size_t samples = 0;
    size_t nonBlackSamples = 0;
    size_t differentSamples = 0;
    std::uint32_t leftActiveTileMask = 0;
    std::uint32_t rightActiveTileMask = 0;
    std::uint32_t differentTileMask = 0;
    size_t leftActiveTiles = 0;
    size_t rightActiveTiles = 0;
    size_t differentTiles = 0;
    std::uint32_t leftHash = 2166136261u;
    std::uint32_t rightHash = 2166136261u;
};

struct HostSharedBridge
{
    HANDLE inputProducerMutex = nullptr;
    bool ownsInputProducerMutex = false;
    HANDLE xinputMapping = nullptr;
    fnvxr::shared::SharedXInputState* xinputState = nullptr;
    HANDLE dinputMapping = nullptr;
    fnvxr::shared::SharedDInputState* dinputState = nullptr;
    HANDLE vrPoseMapping = nullptr;
    fnvxr::shared::SharedVrPoseState* vrPoseState = nullptr;
    HANDLE playerMapping = nullptr;
    fnvxr::shared::SharedPlayerState* playerState = nullptr;
    std::uint32_t xinputPacket = 0;
    std::uint32_t dinputMouseClickPacket = 0;
    std::int32_t dinputHeadLookX = 0;
    std::int32_t dinputHeadLookY = 0;
    std::int32_t dinputGyroLookX = 0;
    std::int32_t dinputGyroLookY = 0;
    std::uint32_t producerEpoch = 0;
};

constexpr LONG SharedPointerWidth = 1280;
constexpr LONG SharedPointerHeight = 720;
constexpr std::uint16_t XInputDpadUp = 0x0001;
constexpr std::uint16_t XInputDpadDown = 0x0002;
constexpr std::uint16_t XInputDpadLeft = 0x0004;
constexpr std::uint16_t XInputDpadRight = 0x0008;
constexpr std::uint16_t XInputStart = 0x0010;
constexpr std::uint16_t XInputBack = 0x0020;
constexpr std::uint16_t XInputLeftThumb = 0x0040;
constexpr std::uint16_t XInputRightThumb = 0x0080;
constexpr std::uint16_t XInputA = 0x1000;
constexpr std::uint16_t XInputB = 0x2000;
constexpr std::uint16_t XInputX = 0x4000;
constexpr std::uint16_t XInputY = 0x8000;

#pragma pack(push, 1)
struct FnvxrSceneCacheHeader
{
    char magic[8];
    std::uint32_t version;
    std::uint32_t headerBytes;
    std::uint32_t flags;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t format;
    std::uint32_t rowPitch;
    std::uint32_t eyeCount;
    std::uint32_t reserved0;
    std::uint32_t reserved1;
    std::uint64_t sequence;
    std::uint64_t leftOffset;
    std::uint64_t leftBytes;
    std::uint64_t rightOffset;
    std::uint64_t rightBytes;
    std::uint8_t leftSha256[32];
    std::uint8_t rightSha256[32];
    std::uint8_t reserved2[8];
};
#pragma pack(pop)

static_assert(sizeof(FnvxrSceneCacheHeader) == 160, "Unexpected scene cache header layout");

struct WindowSearch
{
    HWND hwnd = nullptr;
    DWORD processId = 0;
    LONG bestArea = 0;
};

HMODULE loadOpenXrLoader()
{
    HMODULE module = LoadLibraryA("openxr_loader.dll");
    if (module)
        return module;

#ifdef FNVXR_OPENXR_LOADER_HINT
    std::string hint = FNVXR_OPENXR_LOADER_HINT;
    const std::string::size_type slash = hint.find_last_of("/\\");
    if (slash != std::string::npos)
        SetDllDirectoryA(hint.substr(0, slash).c_str());

    return LoadLibraryExA(FNVXR_OPENXR_LOADER_HINT, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
#else
    return nullptr;
#endif
}

const char* resultName(XrResult result)
{
    switch (result)
    {
        case XR_SUCCESS: return "XR_SUCCESS";
        case XR_TIMEOUT_EXPIRED: return "XR_TIMEOUT_EXPIRED";
        case XR_SESSION_LOSS_PENDING: return "XR_SESSION_LOSS_PENDING";
        case XR_EVENT_UNAVAILABLE: return "XR_EVENT_UNAVAILABLE";
        case XR_ERROR_RUNTIME_UNAVAILABLE: return "XR_ERROR_RUNTIME_UNAVAILABLE";
        case XR_ERROR_FORM_FACTOR_UNAVAILABLE: return "XR_ERROR_FORM_FACTOR_UNAVAILABLE";
        case XR_ERROR_GRAPHICS_DEVICE_INVALID: return "XR_ERROR_GRAPHICS_DEVICE_INVALID";
        case XR_ERROR_SESSION_NOT_RUNNING: return "XR_ERROR_SESSION_NOT_RUNNING";
        default: return "XR_RESULT_OTHER";
    }
}

int envInt(const char* name, int fallback);

bool waitForHmdSystem(OpenXr& xr, XrInstance instance, const XrSystemGetInfo& systemInfo, XrSystemId& systemId)
{
    const int retryCount = std::max(1, envInt("FNVXR_OPENXR_SYSTEM_RETRIES", 120));
    const int retryDelayMs = std::max(50, envInt("FNVXR_OPENXR_SYSTEM_RETRY_MS", 250));

    for (int attempt = 1; attempt <= retryCount; ++attempt)
    {
        const XrResult result = xr.getSystem(instance, &systemInfo, &systemId);
        if (result == XR_SUCCESS)
        {
            std::cout << "xrGetSystem: " << resultName(result)
                      << " systemId=" << systemId
                      << " attempts=" << attempt
                      << "\n";
            return true;
        }

        if (result != XR_ERROR_FORM_FACTOR_UNAVAILABLE && result != XR_ERROR_RUNTIME_UNAVAILABLE)
        {
            std::cout << "xrGetSystem: " << resultName(result)
                      << " systemId=" << systemId
                      << "\n";
            return false;
        }

        if (attempt == 1 || (attempt % 10) == 0)
        {
            std::cout << "xrGetSystem waiting attempt=" << attempt << "/" << retryCount
                      << " result=" << resultName(result)
                      << "\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
    }

    std::cout << "xrGetSystem: " << resultName(XR_ERROR_FORM_FACTOR_UNAVAILABLE)
              << " systemId=" << systemId
              << " retries_exhausted=1"
              << "\n";
    return false;
}

float envFloat(const char* name, float fallback)
{
    const char* value = std::getenv(name);
    if (!value || !*value)
        return fallback;

    char* end = nullptr;
    const float parsed = std::strtof(value, &end);
    return end != value ? parsed : fallback;
}

int envInt(const char* name, int fallback)
{
    const char* value = std::getenv(name);
    if (!value || !*value)
        return fallback;

    char* end = nullptr;
    const long parsed = std::strtol(value, &end, 10);
    return end != value && parsed > 0 ? static_cast<int>(parsed) : fallback;
}

unsigned short envPort(const char* name, unsigned short fallback)
{
    const int parsed = envInt(name, fallback);
    if (parsed <= 0 || parsed > 65535)
        return fallback;
    return static_cast<unsigned short>(parsed);
}

bool envEnabled(const char* name, bool fallback)
{
    const char* value = std::getenv(name);
    if (!value || !*value)
    {
        const char* profile = std::getenv("FNVXR_RUN_PROFILE");
        const bool sidecarProfile =
            profile && (_stricmp(profile, "openxr-sidecar") == 0 || _stricmp(profile, "retail-sidecar") == 0);
        if (sidecarProfile)
        {
            if (std::strcmp(name, "FNVXR_SHOW_GAME_PLANE") == 0
                || std::strcmp(name, "FNVXR_USE_STEREO_GAME_TEXTURES") == 0
                || std::strcmp(name, "FNVXR_GAME_FULLSCREEN_IN_XR") == 0
                || std::strcmp(name, "FNVXR_REQUIRE_WORLD_STEREO") == 0
                || std::strcmp(name, "FNVXR_RENDER_WHEN_SHOULD_RENDER_FALSE") == 0)
            {
                return true;
            }
            if (std::strcmp(name, "FNVXR_HOST_CURSOR_CLICK_ENABLED") == 0
                || std::strcmp(name, "FNVXR_HOST_CURSOR_SET_POS") == 0
                || std::strcmp(name, "FNVXR_HOST_CURSOR_ABSOLUTE_MOVE") == 0
                || std::strcmp(name, "FNVXR_HOST_CURSOR_TRACK_POINTER") == 0
                || std::strcmp(name, "FNVXR_HOST_CURSOR_FOCUS") == 0
                || std::strcmp(name, "FNVXR_HOST_SENDINPUT_CLICK") == 0
                || std::strcmp(name, "FNVXR_CURSOR_TRACK_POINTER") == 0
                || std::strcmp(name, "FNVXR_GAME_WORLD_BEHIND_MENU") == 0
                || std::strcmp(name, "FNVXR_RUNTIME_UI_STUCK_FORCE_WORLD") == 0
                || std::strcmp(name, "FNVXR_FORCE_GAMEPLAY") == 0
                || std::strcmp(name, "FNVXR_SHOW_GAME_PLANE_IN_GAME") == 0
                || std::strcmp(name, "FNVXR_STEREO_FALLBACK_MONO_FULLSCREEN") == 0
                || std::strcmp(name, "FNVXR_SHOW_GAME_PLANE_ON_STEREO_LOSS") == 0
                || std::strcmp(name, "FNVXR_CACHE_ONLY") == 0
                || std::strcmp(name, "FNVXR_ALLOW_CACHE_FULLSCREEN") == 0
                || std::strcmp(name, "FNVXR_MENU_SHOWROOM") == 0
                || std::strcmp(name, "FNVXR_SHOW_WORLD_PROPS") == 0
                || std::strcmp(name, "FNVXR_SHOW_BODY_RIG") == 0
                || std::strcmp(name, "FNVXR_SHOW_HAND_FINGERS") == 0
                || std::strcmp(name, "FNVXR_SHOW_LEFT_AIM_RAY") == 0
                || std::strcmp(name, "FNVXR_SHOW_RIGHT_AIM_RAY") == 0)
            {
                return false;
            }
        }
        if (profile && _stricmp(profile, "rock-solid") == 0)
        {
            if (std::strcmp(name, "FNVXR_SHOW_GAME_PLANE") == 0
                || std::strcmp(name, "FNVXR_SHOW_GAME_PLANE_IN_GAME") == 0
                || std::strcmp(name, "FNVXR_SHOW_GAME_PLANE_ON_STEREO_LOSS") == 0
                || std::strcmp(name, "FNVXR_STEREO_FALLBACK_MONO_FULLSCREEN") == 0
                || std::strcmp(name, "FNVXR_SHOW_WORLD_PROPS") == 0
                || std::strcmp(name, "FNVXR_SHOW_BODY_RIG") == 0
                || std::strcmp(name, "FNVXR_SHOW_PIPBOY_RIG") == 0
                || std::strcmp(name, "FNVXR_SHOW_LEFT_AIM_RAY") == 0
                || std::strcmp(name, "FNVXR_SHOW_RIGHT_AIM_RAY") == 0
                || std::strcmp(name, "FNVXR_HOST_CURSOR_CLICK_ENABLED") == 0
                || std::strcmp(name, "FNVXR_CLICK_CLEAR_CLIP") == 0
                || std::strcmp(name, "FNVXR_GAME_WORLD_BEHIND_MENU") == 0
                || std::strcmp(name, "FNVXR_GAME_FULLSCREEN_IN_XR") == 0
                || std::strcmp(name, "FNVXR_USE_STEREO_GAME_TEXTURES") == 0)
            {
                return true;
            }
            if (std::strcmp(name, "FNVXR_HOST_CURSOR_SET_POS") == 0
                || std::strcmp(name, "FNVXR_HOST_CURSOR_ABSOLUTE_MOVE") == 0
                || std::strcmp(name, "FNVXR_HOST_CURSOR_TRACK_POINTER") == 0
                || std::strcmp(name, "FNVXR_CURSOR_TRACK_POINTER") == 0
                || std::strcmp(name, "FNVXR_SHOW_PAUSE_SCENE") == 0
                || std::strcmp(name, "FNVXR_SHOW_HAND_FINGERS") == 0)
            {
                return false;
            }
        }
        return fallback;
    }

    return value[0] != '0';
}

enum class GamePlaneMode
{
    Center2D,
    Shield2D,
    Stereo3D,
};

GamePlaneMode gamePlaneMode()
{
    const char* mode = std::getenv("FNVXR_GAME_PLANE_MODE");
    if (!mode || !*mode)
        return GamePlaneMode::Center2D;
    if (_stricmp(mode, "shield2d") == 0)
        return GamePlaneMode::Shield2D;
    if (_stricmp(mode, "stereo3d") == 0)
        return GamePlaneMode::Stereo3D;
    return GamePlaneMode::Center2D;
}

const char* gamePlaneModeName(GamePlaneMode mode)
{
    switch (mode)
    {
    case GamePlaneMode::Shield2D:
        return "shield2d";
    case GamePlaneMode::Stereo3D:
        return "stereo3d";
    case GamePlaneMode::Center2D:
    default:
        return "center2d";
    }
}

float fovCenterCropRatio(float centerDegrees, float wideDegrees);

struct GamePlaneModeSettings
{
    bool sourceSurround = false;
    bool surroundInGameplay = false;
    bool compositeCenter = false;
    bool useWideSource = false;
    bool requireWideSource = false;
    bool atlasStitch = false;
    bool drawBottom = false;
    bool drawCenter = false;
    bool edgeStitch = false;
    bool hudOverlay = false;
    bool weaponSafe = false;
    bool singleSourceDuplicate = false;
    bool singleSourceClamp = false;
    bool useInwardBands = false;
    bool useSafeBands = false;
    bool edgeSmear = false;
    int segmentsX = 0;
    int segmentsY = 0;
    float gameplayWidth = 5.60f;
    float gameplayHeight = 3.50f;
    float gameplayOffsetZ = -3.75f;
    float centerDepthX = 0.22f;
    float centerDepthY = 0.08f;
    float centerCornerDepth = 0.03f;
    float surroundDepthX = 0.0f;
    float surroundDepthY = 0.0f;
    float surroundCornerDepth = 0.0f;
    float surroundScaleX = 1.0f;
    float surroundScaleY = 1.0f;
    float surroundBrightness = 1.0f;
    float edgeSmearWidthX = 0.15f;
    float edgeSmearWidthY = 0.13f;
    float edgeSmearOpacity = 0.58f;
    float centerU0 = 0.0f;
    float centerV0 = 0.0f;
    float centerU1 = 1.0f;
    float centerV1 = 1.0f;
    float safeU0 = 0.22f;
    float safeU1 = 0.78f;
    float topV0 = 0.04f;
    float topV1 = 0.22f;
    float sideV0 = 0.18f;
    float sideV1 = 0.58f;
    float safeBottomV0 = 0.42f;
    float safeBottomV1 = 0.62f;
    float bottomV0 = 0.42f;
    float bottomV1 = 0.62f;
    float transitionPower = 0.45f;
    float sign = 1.0f;
};

GamePlaneModeSettings gamePlaneModeSettings(GamePlaneMode mode)
{
    GamePlaneModeSettings settings {};
    if (mode == GamePlaneMode::Shield2D)
    {
        settings.sourceSurround = true;
        settings.surroundInGameplay = true;
        settings.compositeCenter = true;
        settings.atlasStitch = true;
        settings.drawBottom = true;
        settings.drawCenter = false;
        settings.edgeStitch = true;
        settings.hudOverlay = false;
        settings.weaponSafe = true;
        settings.singleSourceDuplicate = false;
        settings.singleSourceClamp = false;
        settings.useInwardBands = false;
        settings.useSafeBands = false;
        settings.edgeSmear = true;
        settings.segmentsX = 64;
        settings.segmentsY = 34;
        settings.surroundDepthX = 0.42f;
        settings.surroundDepthY = 0.16f;
        settings.surroundCornerDepth = 0.08f;
        settings.surroundScaleX = 1.72f;
        settings.surroundScaleY = 1.55f;
        settings.surroundBrightness = 1.0f;
        settings.edgeSmearWidthX = 0.16f;
        settings.edgeSmearWidthY = 0.14f;
        settings.edgeSmearOpacity = 0.52f;
        const float sourceCrop = fovCenterCropRatio(95.0f, 115.0f);
        settings.centerU0 = 0.5f - sourceCrop * 0.5f;
        settings.centerV0 = 0.5f - sourceCrop * 0.5f;
        settings.centerU1 = 0.5f + sourceCrop * 0.5f;
        settings.centerV1 = 0.5f + sourceCrop * 0.5f;
    }
    return settings;
}

float fovDegreesToRadians(float degrees)
{
    return degrees * 3.14159265358979323846f / 180.0f;
}

float horizontalToVerticalFovDegrees(float horizontalDegrees, float aspect)
{
    const float safeAspect = std::max(0.1f, aspect);
    const float halfHorizontal = fovDegreesToRadians(std::clamp(horizontalDegrees, 1.0f, 178.0f)) * 0.5f;
    return 2.0f * std::atan(std::tan(halfHorizontal) / safeAspect) * 180.0f / 3.14159265358979323846f;
}

float fovCenterCropRatio(float centerDegrees, float wideDegrees)
{
    const float safeCenter = std::clamp(centerDegrees, 1.0f, 178.0f);
    const float safeWide = std::clamp(wideDegrees, safeCenter + 0.1f, 179.0f);
    const float center = fovDegreesToRadians(safeCenter) * 0.5f;
    const float wide = fovDegreesToRadians(safeWide) * 0.5f;
    return std::clamp(std::tan(center) / std::tan(wide), 0.05f, 1.0f);
}

bool stereoWorldRuntimeEnabled()
{
    return !envEnabled("FNVXR_DISABLE_STEREO_WORLD", false);
}

bool envEquals(const char* name, const char* expected)
{
    const char* value = std::getenv(name);
    return value && std::strcmp(value, expected) == 0;
}

bool envEqualsIgnoreCase(const char* name, const char* expected)
{
    const char* value = std::getenv(name);
    return value && _stricmp(value, expected) == 0;
}

std::wstring widenPath(const char* text)
{
    if (!text || !*text)
        return {};

    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
    UINT codePage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    if (count <= 0)
    {
        codePage = CP_ACP;
        flags = 0;
        count = MultiByteToWideChar(codePage, flags, text, -1, nullptr, 0);
    }
    if (count <= 0)
        return {};

    std::wstring wide(static_cast<size_t>(count - 1), L'\0');
    MultiByteToWideChar(codePage, flags, text, -1, wide.data(), count);
    return wide;
}

std::wstring defaultPauseSceneImagePath()
{
    return {};
}

std::vector<std::wstring> defaultPauseSceneImagePaths()
{
    return {};
}

std::string readTextFile(const std::wstring& path)
{
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return {};

    LARGE_INTEGER size {};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 1024 * 1024)
    {
        CloseHandle(file);
        return {};
    }

    std::string text(static_cast<size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    const BOOL ok = ReadFile(file, text.data(), static_cast<DWORD>(text.size()), &read, nullptr);
    CloseHandle(file);
    if (!ok)
        return {};
    text.resize(read);
    return text;
}

std::vector<std::uint8_t> readBinaryFile(const std::wstring& path, size_t maxBytes)
{
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return {};

    LARGE_INTEGER size {};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0
        || static_cast<unsigned long long>(size.QuadPart) > static_cast<unsigned long long>(maxBytes))
    {
        CloseHandle(file);
        return {};
    }

    std::vector<std::uint8_t> bytes(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    const BOOL ok = ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
    CloseHandle(file);
    if (!ok)
        return {};
    bytes.resize(read);
    return bytes;
}

std::string jsonStringField(const std::string& text, const char* field)
{
    if (!field || !*field)
        return {};

    const std::string needle = "\"" + std::string(field) + "\"";
    size_t position = text.find(needle);
    if (position == std::string::npos)
        return {};
    position = text.find(':', position + needle.size());
    if (position == std::string::npos)
        return {};
    position = text.find('"', position + 1);
    if (position == std::string::npos)
        return {};
    const size_t end = text.find('"', position + 1);
    if (end == std::string::npos)
        return {};
    return text.substr(position + 1, end - position - 1);
}

std::wstring pathDirectory(const std::wstring& path)
{
    const size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
        return {};
    return path.substr(0, slash);
}

std::wstring joinPath(const std::wstring& left, const std::wstring& right)
{
    if (left.empty())
        return right;
    if (right.empty())
        return left;
    if (left.back() == L'\\' || left.back() == L'/')
        return left + right;
    return left + L"\\" + right;
}

std::vector<std::wstring> sceneCacheManifestPaths()
{
    const char* cacheRootText = std::getenv("FNVXR_SCENE_CACHE_DIR");
    if (!cacheRootText || !*cacheRootText)
        return {};

    const std::wstring cacheRoot = widenPath(cacheRootText);
    std::vector<std::wstring> manifestPaths;
    const std::wstring rootManifest = joinPath(cacheRoot, L"scene-cache.json");
    if (GetFileAttributesW(rootManifest.c_str()) != INVALID_FILE_ATTRIBUTES)
        manifestPaths.push_back(rootManifest);

    WIN32_FIND_DATAW findData {};
    const std::wstring search = joinPath(cacheRoot, L"*");
    HANDLE find = FindFirstFileW(search.c_str(), &findData);
    if (find != INVALID_HANDLE_VALUE)
    {
        do
        {
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
                continue;
            if (std::wcscmp(findData.cFileName, L".") == 0 || std::wcscmp(findData.cFileName, L"..") == 0)
                continue;
            const std::wstring candidate = joinPath(joinPath(cacheRoot, findData.cFileName), L"scene-cache.json");
            if (GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES)
                manifestPaths.push_back(candidate);
        } while (FindNextFileW(find, &findData));
        FindClose(find);
    }
    return manifestPaths;
}

std::vector<std::wstring> sceneCacheImagePaths()
{
    const std::vector<std::wstring> manifestPaths = sceneCacheManifestPaths();
    if (manifestPaths.empty())
    {
        const char* cacheRootText = std::getenv("FNVXR_SCENE_CACHE_DIR");
        if (cacheRootText && *cacheRootText)
            std::wcerr << L"sceneCache root=" << widenPath(cacheRootText) << L" manifest=none\n";
        return {};
    }

    for (const std::wstring& manifestPath : manifestPaths)
    {
        const std::string manifest = readTextFile(manifestPath);
        const std::string left = jsonStringField(manifest, "left");
        const std::string right = jsonStringField(manifest, "right");
        if (left.empty())
            continue;

        const std::wstring dir = pathDirectory(manifestPath);
        std::vector<std::wstring> paths;
        paths.push_back(joinPath(dir, widenPath(left.c_str())));
        if (!right.empty())
            paths.push_back(joinPath(dir, widenPath(right.c_str())));
        std::wcerr << L"sceneCache manifest=" << manifestPath << L" images=" << paths.size() << L"\n";
        return paths;
    }

    std::wcerr << L"sceneCache manifest images=none\n";
    return {};
}

std::wstring sceneCacheRawArtifactPath()
{
    for (const std::wstring& manifestPath : sceneCacheManifestPaths())
    {
        const std::wstring dir = pathDirectory(manifestPath);
        const std::wstring completePath = joinPath(dir, L".complete");
        if (GetFileAttributesW(completePath.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            std::wcerr << L"sceneCache manifest=" << manifestPath << L" complete=0 skipped=1\n";
            continue;
        }

        const std::string manifest = readTextFile(manifestPath);
        if (manifest.find("\"uiActive\": false") == std::string::npos)
        {
            std::wcerr << L"sceneCache manifest=" << manifestPath << L" uiClean=0 skipped=1\n";
            continue;
        }

        const std::string artifact = jsonStringField(manifest, "artifact");
        if (artifact.empty())
        {
            std::wcerr << L"sceneCache manifest=" << manifestPath << L" artifact=none\n";
            continue;
        }

        const std::wstring artifactPath = joinPath(dir, widenPath(artifact.c_str()));
        if (GetFileAttributesW(artifactPath.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            std::wcerr << L"sceneCache artifact=" << artifactPath << L" exists=0\n";
            continue;
        }

        std::wcerr << L"sceneCache artifact=" << artifactPath << L" selected=1\n";
        return artifactPath;
    }

    return {};
}

bool loadTextureFromFile(
    ID3D11Device* device,
    const std::wstring& path,
    ComPtr<ID3D11ShaderResourceView>& textureView,
    int& widthOut,
    int& heightOut)
{
    if (!device || path.empty())
        return false;

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (FAILED(hr))
    {
        std::cerr << "pauseScenePortal WIC factory failed hr=0x" << std::hex << hr << std::dec << "\n";
        return false;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(
        path.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        &decoder);
    if (FAILED(hr))
    {
        std::wcerr << L"pauseScenePortal decode failed path=" << path << L" hr=0x"
                   << std::hex << hr << std::dec << L"\n";
        return false;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr))
        return false;

    UINT width = 0;
    UINT height = 0;
    hr = frame->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0)
        return false;

    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr))
        return false;

    hr = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0f,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr))
        return false;

    const UINT stride = width * 4;
    std::vector<std::uint8_t> pixels(static_cast<size_t>(stride) * static_cast<size_t>(height));
    hr = converter->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data());
    if (FAILED(hr))
        return false;

    D3D11_TEXTURE2D_DESC textureDesc {};
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_B8G8R8A8_TYPELESS;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA textureData {};
    textureData.pSysMem = pixels.data();
    textureData.SysMemPitch = stride;

    ComPtr<ID3D11Texture2D> texture;
    hr = device->CreateTexture2D(&textureDesc, &textureData, &texture);
    if (FAILED(hr))
        return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc {};
    viewDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    viewDesc.Texture2D.MipLevels = 1;
    hr = device->CreateShaderResourceView(texture.Get(), &viewDesc, &textureView);
    if (FAILED(hr))
        return false;

    widthOut = static_cast<int>(width);
    heightOut = static_cast<int>(height);
    return true;
}

enum class HostMode
{
    Control,
    Flat,
    Vr,
};

HostMode hostMode()
{
    const char* value = std::getenv("FNVXR_HOST_MODE");
    if (!value || !*value)
    {
        const char* profile = std::getenv("FNVXR_RUN_PROFILE");
        if (profile && (_stricmp(profile, "openxr-sidecar") == 0 || _stricmp(profile, "retail-sidecar") == 0))
            return HostMode::Vr;
        return HostMode::Control;
    }
    if (_stricmp(value, "control") == 0)
        return HostMode::Control;
    if (_stricmp(value, "flat") == 0 || _stricmp(value, "quad") == 0)
        return HostMode::Flat;
    if (_stricmp(value, "vr") == 0 || _stricmp(value, "experimental-vr") == 0)
        return HostMode::Vr;
    return HostMode::Control;
}

const char* hostModeName(HostMode mode)
{
    switch (mode)
    {
        case HostMode::Control: return "control";
        case HostMode::Flat: return "flat";
        case HostMode::Vr: return "vr";
    }
    return "control";
}

float clamp01(float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

float degreesToRadians(float degrees)
{
    return degrees * 0.017453292519943295769f;
}

float wrapRadians(float value)
{
    constexpr float pi = 3.14159265358979323846f;
    while (value > pi)
        value -= 2.0f * pi;
    while (value < -pi)
        value += 2.0f * pi;
    return value;
}

XMVECTOR vec3(const XrVector3f& value)
{
    return XMVectorSet(value.x, value.y, value.z, 0.0f);
}

XMVECTOR quat(const XrQuaternionf& value)
{
    return XMVectorSet(value.x, value.y, value.z, value.w);
}

XrVector3f xrVector(XMVECTOR value)
{
    XMFLOAT3 out {};
    XMStoreFloat3(&out, value);
    return { out.x, out.y, out.z };
}

XrQuaternionf xrQuaternion(XMVECTOR value)
{
    XMFLOAT4 out {};
    XMStoreFloat4(&out, XMQuaternionNormalize(value));
    return { out.x, out.y, out.z, out.w };
}

XMVECTOR safeNormalize3(XMVECTOR value, XMVECTOR fallback)
{
    const float length = XMVectorGetX(XMVector3Length(value));
    if (length < 0.0001f)
        return XMVector3Normalize(fallback);
    return XMVectorScale(value, 1.0f / length);
}

void hmdYawPitch(const XrPosef& pose, float& yawRadians, float& pitchRadians)
{
    const XMVECTOR orientation = quat(pose.orientation);
    const XMVECTOR forward = XMVector3Rotate(XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), orientation);
    XMFLOAT3 f {};
    XMStoreFloat3(&f, safeNormalize3(forward, XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f)));

    yawRadians = std::atan2(f.x, -f.z);
    pitchRadians = std::asin(std::clamp(f.y, -1.0f, 1.0f));
}

HeadspaceLook sampleHeadspaceLook(HeadspaceLookTracker& tracker, const XrPosef& hmdPose, bool trackingActive, bool aimHeld)
{
    HeadspaceLook look {};
    if (!trackingActive || !envEnabled("FNVXR_HEADSPACE_LOOK_ENABLE", true))
    {
        tracker.hasLast = false;
        return look;
    }

    float yaw = 0.0f;
    float pitch = 0.0f;
    hmdYawPitch(hmdPose, yaw, pitch);
    if (!tracker.hasLast)
    {
        tracker.hasLast = true;
        tracker.yawRadians = yaw;
        tracker.pitchRadians = pitch;
        look.active = true;
        return look;
    }

    const float fallbackDeadzone = envFloat("FNVXR_HEADSPACE_LOOK_DEADZONE_DEGREES", 0.02f);
    const float deadzone = degreesToRadians(aimHeld
        ? envFloat("FNVXR_HEADSPACE_LOOK_AIM_DEADZONE_DEGREES", fallbackDeadzone)
        : envFloat("FNVXR_HEADSPACE_LOOK_NORMAL_DEADZONE_DEGREES", fallbackDeadzone));
    const float fallbackMaxDelta = envFloat("FNVXR_HEADSPACE_LOOK_MAX_DELTA_DEGREES", 8.0f);
    const float maxDelta = degreesToRadians(aimHeld
        ? envFloat("FNVXR_HEADSPACE_LOOK_AIM_MAX_DELTA_DEGREES", fallbackMaxDelta)
        : envFloat("FNVXR_HEADSPACE_LOOK_NORMAL_MAX_DELTA_DEGREES", fallbackMaxDelta));
    float yawDelta = wrapRadians(yaw - tracker.yawRadians);
    float pitchDelta = wrapRadians(pitch - tracker.pitchRadians);
    tracker.yawRadians = yaw;
    tracker.pitchRadians = pitch;

    if (std::fabs(yawDelta) < deadzone)
        yawDelta = 0.0f;
    if (std::fabs(pitchDelta) < deadzone)
        pitchDelta = 0.0f;
    yawDelta = std::clamp(yawDelta, -maxDelta, maxDelta);
    pitchDelta = std::clamp(pitchDelta, -maxDelta, maxDelta);

    look.active = true;
    look.yawRadians = yawDelta;
    look.pitchRadians = pitchDelta;
    look.yawMicroradians = static_cast<std::int32_t>(std::lround(yawDelta * 1000000.0f));
    look.pitchMicroradians = static_cast<std::int32_t>(std::lround(pitchDelta * 1000000.0f));
    return look;
}

HeadspaceLook sampleRightHandGyroLook(HeadspaceLookTracker& tracker, const XrPosef& aimPose, bool trackingActive)
{
    HeadspaceLook look {};
    if (!trackingActive || !envEnabled("FNVXR_GYRO_AIM_ENABLE", false))
    {
        tracker.hasLast = false;
        return look;
    }

    float yaw = 0.0f;
    float pitch = 0.0f;
    hmdYawPitch(aimPose, yaw, pitch);
    if (!tracker.hasLast)
    {
        tracker.hasLast = true;
        tracker.yawRadians = yaw;
        tracker.pitchRadians = pitch;
        look.active = true;
        return look;
    }

    const float deadzone =
        degreesToRadians(envFloat("FNVXR_GYRO_AIM_DEADZONE_DEGREES", 0.04f));
    const float maxDelta =
        degreesToRadians(envFloat("FNVXR_GYRO_AIM_MAX_DELTA_DEGREES", 2.0f));
    float yawDelta = wrapRadians(yaw - tracker.yawRadians);
    float pitchDelta = wrapRadians(pitch - tracker.pitchRadians);
    tracker.yawRadians = yaw;
    tracker.pitchRadians = pitch;

    if (std::fabs(yawDelta) < deadzone)
        yawDelta = 0.0f;
    if (std::fabs(pitchDelta) < deadzone)
        pitchDelta = 0.0f;
    yawDelta = std::clamp(yawDelta, -maxDelta, maxDelta);
    pitchDelta = std::clamp(pitchDelta, -maxDelta, maxDelta);

    look.active = true;
    look.yawRadians = yawDelta;
    look.pitchRadians = pitchDelta;
    look.yawMicroradians = static_cast<std::int32_t>(std::lround(yawDelta * 1000000.0f));
    look.pitchMicroradians = static_cast<std::int32_t>(std::lround(pitchDelta * 1000000.0f));
    return look;
}

template <typename Fn>
bool loadInstanceFunction(OpenXr& xr, XrInstance instance, const char* name, Fn& fn)
{
    PFN_xrVoidFunction raw = nullptr;
    XrResult result = xr.getInstanceProcAddr(instance, name, &raw);
    fn = reinterpret_cast<Fn>(raw);
    if (result != XR_SUCCESS || !fn)
    {
        std::cerr << "missing OpenXR function " << name << " result=" << resultName(result) << "\n";
        return false;
    }
    return true;
}

bool loadGlobalFunctions(OpenXr& xr)
{
    xr.loader = loadOpenXrLoader();
    if (!xr.loader)
    {
        std::cerr << "OpenXR loader not found\n";
        return false;
    }

    xr.getInstanceProcAddr = reinterpret_cast<PFN_xrGetInstanceProcAddr>(
        GetProcAddress(xr.loader, "xrGetInstanceProcAddr"));
    xr.createInstance = reinterpret_cast<PFN_xrCreateInstance>(
        GetProcAddress(xr.loader, "xrCreateInstance"));
    xr.enumerateExtensions = reinterpret_cast<PFN_xrEnumerateInstanceExtensionProperties>(
        GetProcAddress(xr.loader, "xrEnumerateInstanceExtensionProperties"));

    return xr.getInstanceProcAddr && xr.createInstance && xr.enumerateExtensions;
}

void enableDpiAwareness()
{
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32)
    {
        using SetProcessDpiAwarenessContextFn = BOOL (WINAPI*)(HANDLE);
        auto* setProcessDpiAwarenessContext = reinterpret_cast<SetProcessDpiAwarenessContextFn>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (setProcessDpiAwarenessContext && setProcessDpiAwarenessContext(reinterpret_cast<HANDLE>(-4)))
            return;
    }

    SetProcessDPIAware();
}

bool loadInstanceFunctions(OpenXr& xr, XrInstance instance)
{
    return loadInstanceFunction(xr, instance, "xrDestroyInstance", xr.destroyInstance)
        && loadInstanceFunction(xr, instance, "xrGetSystem", xr.getSystem)
        && loadInstanceFunction(xr, instance, "xrGetD3D11GraphicsRequirementsKHR", xr.getD3D11GraphicsRequirementsKHR)
        && loadInstanceFunction(xr, instance, "xrCreateSession", xr.createSession)
        && loadInstanceFunction(xr, instance, "xrDestroySession", xr.destroySession)
        && loadInstanceFunction(xr, instance, "xrPollEvent", xr.pollEvent)
        && loadInstanceFunction(xr, instance, "xrEnumerateViewConfigurationViews", xr.enumerateViewConfigurationViews)
        && loadInstanceFunction(xr, instance, "xrLocateViews", xr.locateViews)
        && loadInstanceFunction(xr, instance, "xrBeginSession", xr.beginSession)
        && loadInstanceFunction(xr, instance, "xrEndSession", xr.endSession)
        && loadInstanceFunction(xr, instance, "xrWaitFrame", xr.waitFrame)
        && loadInstanceFunction(xr, instance, "xrBeginFrame", xr.beginFrame)
        && loadInstanceFunction(xr, instance, "xrEndFrame", xr.endFrame)
        && loadInstanceFunction(xr, instance, "xrEnumerateSwapchainFormats", xr.enumerateSwapchainFormats)
        && loadInstanceFunction(xr, instance, "xrCreateSwapchain", xr.createSwapchain)
        && loadInstanceFunction(xr, instance, "xrDestroySwapchain", xr.destroySwapchain)
        && loadInstanceFunction(xr, instance, "xrEnumerateSwapchainImages", xr.enumerateSwapchainImages)
        && loadInstanceFunction(xr, instance, "xrAcquireSwapchainImage", xr.acquireSwapchainImage)
        && loadInstanceFunction(xr, instance, "xrWaitSwapchainImage", xr.waitSwapchainImage)
        && loadInstanceFunction(xr, instance, "xrReleaseSwapchainImage", xr.releaseSwapchainImage)
        && loadInstanceFunction(xr, instance, "xrCreateReferenceSpace", xr.createReferenceSpace)
        && loadInstanceFunction(xr, instance, "xrDestroySpace", xr.destroySpace)
        && loadInstanceFunction(xr, instance, "xrLocateSpace", xr.locateSpace)
        && loadInstanceFunction(xr, instance, "xrStringToPath", xr.stringToPath)
        && loadInstanceFunction(xr, instance, "xrPathToString", xr.pathToString)
        && loadInstanceFunction(xr, instance, "xrCreateActionSet", xr.createActionSet)
        && loadInstanceFunction(xr, instance, "xrDestroyActionSet", xr.destroyActionSet)
        && loadInstanceFunction(xr, instance, "xrCreateAction", xr.createAction)
        && loadInstanceFunction(xr, instance, "xrDestroyAction", xr.destroyAction)
        && loadInstanceFunction(xr, instance, "xrCreateActionSpace", xr.createActionSpace)
        && loadInstanceFunction(xr, instance, "xrSuggestInteractionProfileBindings", xr.suggestInteractionProfileBindings)
        && loadInstanceFunction(xr, instance, "xrGetCurrentInteractionProfile", xr.getCurrentInteractionProfile)
        && loadInstanceFunction(xr, instance, "xrAttachSessionActionSets", xr.attachSessionActionSets)
        && loadInstanceFunction(xr, instance, "xrSyncActions", xr.syncActions)
        && loadInstanceFunction(xr, instance, "xrGetActionStateFloat", xr.getActionStateFloat)
        && loadInstanceFunction(xr, instance, "xrGetActionStateBoolean", xr.getActionStateBoolean)
        && loadInstanceFunction(xr, instance, "xrGetActionStateVector2f", xr.getActionStateVector2f)
        && loadInstanceFunction(xr, instance, "xrGetActionStatePose", xr.getActionStatePose);
}

int64_t chooseSwapchainFormat(OpenXr& xr, XrSession session)
{
    uint32_t formatCount = 0;
    if (xr.enumerateSwapchainFormats(session, 0, &formatCount, nullptr) != XR_SUCCESS || formatCount == 0)
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    std::vector<int64_t> formats(formatCount);
    xr.enumerateSwapchainFormats(session, formatCount, &formatCount, formats.data());

    for (int64_t format : formats)
    {
        if (format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
            return format;
    }

    for (int64_t format : formats)
    {
        if (format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
            return format;
    }

    for (int64_t format : formats)
    {
        if (format == DXGI_FORMAT_R8G8B8A8_UNORM)
            return format;
    }

    return formats.front();
}

bool createQuadSwapchain(OpenXr& xr, XrSession session, int64_t format, int32_t width, int32_t height, QuadSwapchain& out)
{
    out.width = width;
    out.height = height;
    out.dxgiFormat = static_cast<DXGI_FORMAT>(format);

    XrSwapchainCreateInfo createInfo { XR_TYPE_SWAPCHAIN_CREATE_INFO };
    createInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.format = format;
    createInfo.sampleCount = 1;
    createInfo.width = static_cast<uint32_t>(width);
    createInfo.height = static_cast<uint32_t>(height);
    createInfo.faceCount = 1;
    createInfo.arraySize = 1;
    createInfo.mipCount = 1;

    XrResult result = xr.createSwapchain(session, &createInfo, &out.handle);
    if (result != XR_SUCCESS)
    {
        std::cerr << "xrCreateSwapchain failed: " << resultName(result) << "\n";
        return false;
    }

    uint32_t imageCount = 0;
    xr.enumerateSwapchainImages(out.handle, 0, &imageCount, nullptr);
    out.images.resize(imageCount);
    for (auto& image : out.images)
        image.type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;

    result = xr.enumerateSwapchainImages(
        out.handle,
        imageCount,
        &imageCount,
        reinterpret_cast<XrSwapchainImageBaseHeader*>(out.images.data()));
    if (result != XR_SUCCESS)
    {
        std::cerr << "xrEnumerateSwapchainImages failed: " << resultName(result) << "\n";
        return false;
    }

    return true;
}

bool createSwapchainRenderTargets(ID3D11Device* device, QuadSwapchain& swapchain)
{
    swapchain.renderTargetViews.clear();
    swapchain.renderTargetViews.resize(swapchain.images.size());

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc {};
    rtvDesc.Format = swapchain.dxgiFormat;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    for (size_t i = 0; i < swapchain.images.size(); ++i)
    {
        if (FAILED(device->CreateRenderTargetView(swapchain.images[i].texture, &rtvDesc, &swapchain.renderTargetViews[i])))
        {
            std::cerr << "CreateRenderTargetView failed for XR swapchain image=" << i << "\n";
            return false;
        }
    }

    D3D11_TEXTURE2D_DESC depthTextureDesc {};
    depthTextureDesc.Width = static_cast<UINT>(swapchain.width);
    depthTextureDesc.Height = static_cast<UINT>(swapchain.height);
    depthTextureDesc.MipLevels = 1;
    depthTextureDesc.ArraySize = 1;
    depthTextureDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthTextureDesc.SampleDesc.Count = 1;
    depthTextureDesc.Usage = D3D11_USAGE_DEFAULT;
    depthTextureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(device->CreateTexture2D(&depthTextureDesc, nullptr, &swapchain.depthTexture)))
    {
        std::cerr << "CreateTexture2D failed for XR depth buffer\n";
        return false;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc {};
    dsvDesc.Format = depthTextureDesc.Format;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    if (FAILED(device->CreateDepthStencilView(swapchain.depthTexture.Get(), &dsvDesc, &swapchain.depthStencilView)))
    {
        std::cerr << "CreateDepthStencilView failed for XR depth buffer\n";
        return false;
    }

    return true;
}

bool releaseSwapchainImage(OpenXr& xr, XrSwapchain swapchain)
{
    XrSwapchainImageReleaseInfo releaseInfo { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
    return xr.releaseSwapchainImage(swapchain, &releaseInfo) == XR_SUCCESS;
}

bool clearSwapchain(OpenXr& xr, ID3D11Device* device, QuadSwapchain& swapchain, const float color[4])
{
    uint32_t imageIndex = 0;
    XrSwapchainImageAcquireInfo acquireInfo { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
    XrResult result = xr.acquireSwapchainImage(swapchain.handle, &acquireInfo, &imageIndex);
    if (result != XR_SUCCESS)
        return false;
    if (imageIndex >= swapchain.renderTargetViews.size()
        || !swapchain.renderTargetViews[imageIndex]
        || !swapchain.depthStencilView)
    {
        releaseSwapchainImage(xr, swapchain.handle);
        return false;
    }

    XrSwapchainImageWaitInfo waitInfo { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
    waitInfo.timeout = XR_INFINITE_DURATION;
    result = xr.waitSwapchainImage(swapchain.handle, &waitInfo);
    if (result != XR_SUCCESS)
    {
        releaseSwapchainImage(xr, swapchain.handle);
        return false;
    }

    ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(&context);

    ComPtr<ID3D11RenderTargetView> rtv;
    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc {};
    rtvDesc.Format = swapchain.dxgiFormat;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    const HRESULT rtvResult = device->CreateRenderTargetView(swapchain.images[imageIndex].texture, &rtvDesc, &rtv);
    bool cleared = false;
    if (SUCCEEDED(rtvResult))
    {
        context->ClearRenderTargetView(rtv.Get(), color);
        cleared = true;
    }
    else
    {
        static bool printed = false;
        if (!printed)
        {
            std::cerr << "CreateRenderTargetView failed hr=0x" << std::hex << rtvResult << std::dec << "\n";
            printed = true;
        }
    }

    return releaseSwapchainImage(xr, swapchain.handle) && cleared;
}

BOOL CALLBACK enumFalloutWindow(HWND hwnd, LPARAM param)
{
    if (!IsWindowVisible(hwnd))
        return TRUE;

    auto* search = reinterpret_cast<WindowSearch*>(param);
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (search->processId != 0 && processId != search->processId)
        return TRUE;

    RECT rect {};
    if (!GetClientRect(hwnd, &rect) || rect.right <= rect.left || rect.bottom <= rect.top)
        return TRUE;

    const LONG width = rect.right - rect.left;
    const LONG height = rect.bottom - rect.top;
    const LONG area = width * height;
    if (area > search->bestArea)
    {
        search->hwnd = hwnd;
        search->bestArea = area;
    }
    return TRUE;
}

DWORD findFalloutProcessId()
{
    PROCESSENTRY32W entry {};
    entry.dwSize = sizeof(entry);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;

    DWORD processId = 0;
    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (_wcsicmp(entry.szExeFile, L"FalloutNV.exe") == 0)
            {
                processId = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return processId;
}

HWND findFalloutWindow()
{
    WindowSearch search {};
    search.processId = findFalloutProcessId();
    EnumWindows(enumFalloutWindow, reinterpret_cast<LPARAM>(&search));
    return search.hwnd;
}

bool captureFalloutWindowBgra(int width, int height, std::vector<std::uint32_t>& pixels, float& sourceAspect)
{
    pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height));

    HWND hwnd = findFalloutWindow();
    if (!hwnd)
        return false;

    RECT client {};
    if (!GetClientRect(hwnd, &client))
        return false;

    const int sourceWidth = client.right - client.left;
    const int sourceHeight = client.bottom - client.top;
    if (sourceWidth <= 0 || sourceHeight <= 0)
        return false;
    sourceAspect = static_cast<float>(sourceWidth) / static_cast<float>(sourceHeight);

    HDC sourceDc = GetDC(hwnd);
    if (!sourceDc)
        return false;

    HDC memoryDc = CreateCompatibleDC(sourceDc);
    HDC sourceMemoryDc = CreateCompatibleDC(sourceDc);
    if (!memoryDc || !sourceMemoryDc)
    {
        if (sourceMemoryDc)
            DeleteDC(sourceMemoryDc);
        if (memoryDc)
            DeleteDC(memoryDc);
        ReleaseDC(hwnd, sourceDc);
        return false;
    }

    BITMAPINFO bitmapInfo {};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(memoryDc, &bitmapInfo, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib || !bits)
    {
        if (dib)
            DeleteObject(dib);
        DeleteDC(sourceMemoryDc);
        DeleteDC(memoryDc);
        ReleaseDC(hwnd, sourceDc);
        return false;
    }

    BITMAPINFO sourceBitmapInfo {};
    sourceBitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    sourceBitmapInfo.bmiHeader.biWidth = sourceWidth;
    sourceBitmapInfo.bmiHeader.biHeight = -sourceHeight;
    sourceBitmapInfo.bmiHeader.biPlanes = 1;
    sourceBitmapInfo.bmiHeader.biBitCount = 32;
    sourceBitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* sourceBits = nullptr;
    HBITMAP sourceDib =
        CreateDIBSection(sourceMemoryDc, &sourceBitmapInfo, DIB_RGB_COLORS, &sourceBits, nullptr, 0);
    if (!sourceDib || !sourceBits)
    {
        if (sourceDib)
            DeleteObject(sourceDib);
        DeleteObject(dib);
        DeleteDC(sourceMemoryDc);
        DeleteDC(memoryDc);
        ReleaseDC(hwnd, sourceDc);
        return false;
    }

    HGDIOBJ previous = SelectObject(memoryDc, dib);
    HGDIOBJ previousSource = SelectObject(sourceMemoryDc, sourceDib);
    SetStretchBltMode(memoryDc, HALFTONE);
    BOOL copied = BitBlt(sourceMemoryDc, 0, 0, sourceWidth, sourceHeight, sourceDc, 0, 0, SRCCOPY);
    if (copied)
    {
        copied = StretchBlt(
            memoryDc,
            0,
            0,
            width,
            height,
            sourceMemoryDc,
            0,
            0,
            sourceWidth,
            sourceHeight,
            SRCCOPY);
    }
    if (copied)
        std::memcpy(pixels.data(), bits, pixels.size() * sizeof(std::uint32_t));

    SelectObject(sourceMemoryDc, previousSource);
    SelectObject(memoryDc, previous);
    DeleteObject(sourceDib);
    DeleteObject(dib);
    DeleteDC(sourceMemoryDc);
    DeleteDC(memoryDc);
    ReleaseDC(hwnd, sourceDc);
    return copied == TRUE;
}

void fillGamePlaneTestPattern(int width, int height, std::vector<std::uint32_t>& pixels)
{
    pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const bool border = x < 16 || y < 16 || x >= width - 16 || y >= height - 16;
            const bool centerX = std::abs(x - width / 2) < 5;
            const bool centerY = std::abs(y - height / 2) < 5;
            const bool grid = (x % 160) < 3 || (y % 90) < 3;
            std::uint32_t color = 0xff181818u;
            if (grid)
                color = 0xff304030u;
            if (centerX)
                color = 0xff3030f0u;
            if (centerY)
                color = 0xff30f030u;
            if (border)
                color = 0xfff0f0f0u;
            pixels[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] = color;
        }
    }
}

bool readSharedD3D9VideoFrameMapping(
    HANDLE& mapping,
    std::uint8_t*& view,
    LONG& lastSequence,
    const char* mappingName,
    int textureWidth,
    int textureHeight,
    std::vector<std::uint32_t>& pixels,
    float& sourceAspect)
{
    if (!view)
    {
        mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, mappingName);
        if (!mapping)
            return false;

        constexpr SIZE_T mappingSize =
            sizeof(SharedVideoHeader) + SharedVideoMaxWidth * SharedVideoMaxHeight * 4;
        view = static_cast<std::uint8_t*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, mappingSize));
        if (!view)
        {
            CloseHandle(mapping);
            mapping = nullptr;
            return false;
        }
    }

    const auto* header = reinterpret_cast<const SharedVideoHeader*>(view);
    if (header->magic != SharedVideoMagic || header->writing)
        return false;

    const LONG sequenceBefore = header->sequence;
    if (sequenceBefore == lastSequence)
        return false;

    const int sourceWidth = header->width;
    const int sourceHeight = header->height;
    const int sourcePitch = header->pitchBytes;
    if (sourceWidth <= 0
        || sourceHeight <= 0
        || sourceWidth > static_cast<int>(SharedVideoMaxWidth)
        || sourceHeight > static_cast<int>(SharedVideoMaxHeight)
        || sourcePitch < sourceWidth * 4)
    {
        return false;
    }

    const auto* srcPixels = reinterpret_cast<const std::uint32_t*>(view + sizeof(SharedVideoHeader));
    pixels.resize(static_cast<size_t>(textureWidth) * static_cast<size_t>(textureHeight));
    if (sourceWidth == textureWidth && sourceHeight == textureHeight)
    {
        for (int y = 0; y < sourceHeight; ++y)
        {
            std::memcpy(
                pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(sourceWidth),
                reinterpret_cast<const std::uint8_t*>(srcPixels) + static_cast<size_t>(y) * sourcePitch,
                static_cast<size_t>(sourceWidth) * sizeof(std::uint32_t));
        }
    }
    else
    {
        for (int y = 0; y < textureHeight; ++y)
        {
            const int srcY = y * sourceHeight / textureHeight;
            const auto* srcRow = reinterpret_cast<const std::uint32_t*>(
                reinterpret_cast<const std::uint8_t*>(srcPixels) + static_cast<size_t>(srcY) * sourcePitch);
            auto* dstRow = pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(textureWidth);
            for (int x = 0; x < textureWidth; ++x)
            {
                const int srcX = x * sourceWidth / textureWidth;
                dstRow[x] = srcRow[srcX];
            }
        }
    }

    if (header->writing || header->sequence != sequenceBefore)
        return false;

    lastSequence = sequenceBefore;
    sourceAspect = static_cast<float>(sourceWidth) / static_cast<float>(sourceHeight);
    return true;
}

bool readSharedD3D9VideoFrame(Renderer& renderer, std::vector<std::uint32_t>& pixels, float& sourceAspect)
{
    return readSharedD3D9VideoFrameMapping(
        renderer.sharedVideoMapping,
        renderer.sharedVideoView,
        renderer.lastSharedVideoSequence,
        "Local\\FNVXR_D3D9_Frame_v1",
        renderer.gameTextureWidth,
        renderer.gameTextureHeight,
        pixels,
        sourceAspect);
}

bool readSharedD3D9WorldVideoFrame(Renderer& renderer, std::vector<std::uint32_t>& pixels, float& sourceAspect)
{
    const char* mappingName = std::getenv("FNVXR_D3D9_WIDE_WORLD_FRAME_MAPPING");
    if (!mappingName || !*mappingName)
        mappingName = "Local\\FNVXR_D3D9_WorldFrame_v1";
    return readSharedD3D9VideoFrameMapping(
        renderer.sharedWorldVideoMapping,
        renderer.sharedWorldVideoView,
        renderer.lastSharedWorldVideoSequence,
        mappingName,
        renderer.gameTextureWidth,
        renderer.gameTextureHeight,
        pixels,
        sourceAspect);
}

void copySharedPlaneToTexturePixels(
    const std::uint8_t* srcPixels,
    int sourceWidth,
    int sourceHeight,
    int sourcePitch,
    int textureWidth,
    int textureHeight,
    std::vector<std::uint32_t>& pixels)
{
    pixels.resize(static_cast<size_t>(textureWidth) * static_cast<size_t>(textureHeight));
    if (sourceWidth == textureWidth && sourceHeight == textureHeight)
    {
        for (int y = 0; y < sourceHeight; ++y)
        {
            std::memcpy(
                pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(sourceWidth),
                srcPixels + static_cast<size_t>(y) * sourcePitch,
                static_cast<size_t>(sourceWidth) * sizeof(std::uint32_t));
        }
        return;
    }

    for (int y = 0; y < textureHeight; ++y)
    {
        const int srcY = y * sourceHeight / textureHeight;
        const auto* srcRow = reinterpret_cast<const std::uint32_t*>(srcPixels + static_cast<size_t>(srcY) * sourcePitch);
        auto* dstRow = pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(textureWidth);
        for (int x = 0; x < textureWidth; ++x)
        {
            const int srcX = x * sourceWidth / textureWidth;
            dstRow[x] = srcRow[srcX];
        }
    }
}

size_t countSetBits16(std::uint32_t value)
{
    value &= 0xffffu;
    size_t count = 0;
    while (value)
    {
        value &= value - 1u;
        ++count;
    }
    return count;
}

StereoPixelStats analyzeStereoPixels(
    const std::array<std::vector<std::uint32_t>, 2>& eyePixels,
    int width,
    int height)
{
    StereoPixelStats stats {};
    const size_t pixelCount = std::min(eyePixels[0].size(), eyePixels[1].size());
    if (pixelCount == 0 || width <= 0 || height <= 0)
        return stats;

    const size_t stride = static_cast<size_t>(std::max(1, envInt("FNVXR_STEREO_CONTENT_SAMPLE_STRIDE", 97)));
    const std::uint32_t blackThreshold =
        static_cast<std::uint32_t>(std::max(0, envInt("FNVXR_STEREO_CONTENT_BLACK_THRESHOLD", 8)));
    const int meaningfulRgbDelta = std::max(1, envInt("FNVXR_STEREO_MIN_RGB_DELTA", 4));
    for (size_t i = 0; i < pixelCount; i += stride)
    {
        const std::uint32_t left = eyePixels[0][i] & 0x00ffffffu;
        const std::uint32_t right = eyePixels[1][i] & 0x00ffffffu;
        stats.leftHash ^= left;
        stats.leftHash *= 16777619u;
        stats.rightHash ^= right;
        stats.rightHash *= 16777619u;
        ++stats.samples;
        const int leftB = static_cast<int>(left & 0xffu);
        const int leftG = static_cast<int>((left >> 8) & 0xffu);
        const int leftR = static_cast<int>((left >> 16) & 0xffu);
        const int rightB = static_cast<int>(right & 0xffu);
        const int rightG = static_cast<int>((right >> 8) & 0xffu);
        const int rightR = static_cast<int>((right >> 16) & 0xffu);
        const bool leftActive = std::max({ leftR, leftG, leftB }) > static_cast<int>(blackThreshold);
        const bool rightActive = std::max({ rightR, rightG, rightB }) > static_cast<int>(blackThreshold);
        const size_t x = i % static_cast<size_t>(width);
        const size_t y = i / static_cast<size_t>(width);
        const std::uint32_t tileX = static_cast<std::uint32_t>((std::min)(
            static_cast<size_t>(3), x * 4u / static_cast<size_t>(width)));
        const std::uint32_t tileY = static_cast<std::uint32_t>((std::min)(
            static_cast<size_t>(3), y * 4u / static_cast<size_t>(height)));
        const std::uint32_t tileBit = 1u << (tileY * 4u + tileX);
        if (leftActive)
            stats.leftActiveTileMask |= tileBit;
        if (rightActive)
            stats.rightActiveTileMask |= tileBit;
        if (leftActive || rightActive)
        {
            ++stats.nonBlackSamples;
        }
        if (std::max({
                std::abs(leftR - rightR),
                std::abs(leftG - rightG),
                std::abs(leftB - rightB) }) >= meaningfulRgbDelta)
        {
            ++stats.differentSamples;
            stats.differentTileMask |= tileBit;
        }
    }
    stats.leftActiveTiles = countSetBits16(stats.leftActiveTileMask);
    stats.rightActiveTiles = countSetBits16(stats.rightActiveTileMask);
    stats.differentTiles = countSetBits16(stats.differentTileMask);
    return stats;
}

bool isMostlyBlackFrame(const std::vector<std::uint32_t>& pixels)
{
    if (pixels.empty())
        return true;

    const size_t stride = static_cast<size_t>(std::max(1, envInt("FNVXR_GAME_FRAME_SAMPLE_STRIDE", 97)));
    const std::uint32_t blackThreshold =
        static_cast<std::uint32_t>(std::max(0, envInt("FNVXR_GAME_FRAME_BLACK_THRESHOLD", 8)));
    const size_t minNonBlackSamples =
        static_cast<size_t>(std::max(1, envInt("FNVXR_GAME_FRAME_MIN_NONBLACK_SAMPLES", 16)));

    size_t nonBlackSamples = 0;
    for (size_t i = 0; i < pixels.size(); i += stride)
    {
        const std::uint32_t pixel = pixels[i] & 0x00ffffffu;
        const std::uint32_t maxChannel = std::max({
            pixel & 0xffu,
            (pixel >> 8) & 0xffu,
            (pixel >> 16) & 0xffu });
        if (maxChannel > blackThreshold)
            ++nonBlackSamples;
    }
    return nonBlackSamples < minNonBlackSamples;
}

bool readSharedD3D9StereoFrame(
    Renderer& renderer,
    std::array<std::vector<std::uint32_t>, 2>& eyePixels,
    float& sourceAspect,
    bool& separated,
    bool& worldCandidate,
    LONG& poseSequenceOut,
    XrTime& renderedDisplayTimeOut,
    LONG& sequenceOut)
{
    poseSequenceOut = 0;
    renderedDisplayTimeOut = 0;
    sequenceOut = 0;
    if (!renderer.sharedStereoView)
    {
        renderer.sharedStereoMapping = OpenFileMappingA(
            FILE_MAP_READ,
            FALSE,
            fnvxr::shared::D3D9StereoFrameSharedMappingName);
        if (!renderer.sharedStereoMapping)
            return false;

        constexpr SIZE_T mappingSize =
            sizeof(SharedStereoHeader) + SharedVideoMaxWidth * SharedVideoMaxHeight * 4 * 2;
        renderer.sharedStereoView =
            static_cast<std::uint8_t*>(MapViewOfFile(renderer.sharedStereoMapping, FILE_MAP_READ, 0, 0, mappingSize));
        if (!renderer.sharedStereoView)
        {
            CloseHandle(renderer.sharedStereoMapping);
            renderer.sharedStereoMapping = nullptr;
            return false;
        }
    }

    const auto* header = reinterpret_cast<const SharedStereoHeader*>(renderer.sharedStereoView);
    constexpr std::uint32_t mappingSize =
        sizeof(SharedStereoHeader) + SharedVideoMaxWidth * SharedVideoMaxHeight * 4 * 2;
    if (header->magic != SharedStereoMagic
        || header->version != fnvxr::shared::D3D9StereoFrameSharedVersion
        || header->headerBytes != sizeof(SharedStereoHeader)
        || header->leftPayloadOffset != sizeof(SharedStereoHeader)
        || header->totalMappingBytes != mappingSize
        || header->writing)
        return false;

    const LONG sequenceBefore = header->sequence;
    if (sequenceBefore == renderer.lastSharedStereoSequence)
        return false;
    const bool requireNativeStereo = envEnabled("FNVXR_REQUIRE_NATIVE_STEREO", true);
    const bool coherentSameTickProducer =
        (header->producerMode == static_cast<LONG>(fnvxr::shared::StereoProducerNativeSameFrame)
            || header->producerMode == static_cast<LONG>(fnvxr::shared::StereoProducerSingleTraversal))
        && header->renderPairSequence > 0;
    if (requireNativeStereo && !coherentSameTickProducer)
        return false;
    separated = header->separated != 0;
    worldCandidate = header->worldCandidate != 0;
    if (header->uiActive)
        return false;
    if (!header->poseValid)
        return false;
    const LONG poseSequence = header->poseSequence;
    if (poseSequence <= 0)
        return false;
    const XrTime renderedDisplayTime = static_cast<XrTime>(header->renderedDisplayTime);
    if (renderedDisplayTime <= 0)
        return false;
    const std::uint32_t sourceReferenceGeneration = header->referenceSpaceGeneration;
    const std::uint32_t sourceProducerEpoch = header->producerEpoch;
    if (sourceReferenceGeneration == 0 || sourceProducerEpoch == 0)
        return false;

    std::array<XrView, 2> sourceViews {
        XrView { XR_TYPE_VIEW },
        XrView { XR_TYPE_VIEW }
    };
    const float* sourceRot[2] { header->leftEyeRot, header->rightEyeRot };
    const float* sourcePos[2] { header->leftEyePos, header->rightEyePos };
    const float* sourceFov[2] { header->leftFov, header->rightFov };
    for (int eye = 0; eye < 2; ++eye)
    {
        const float quatLengthSquared = sourceRot[eye][0] * sourceRot[eye][0]
            + sourceRot[eye][1] * sourceRot[eye][1]
            + sourceRot[eye][2] * sourceRot[eye][2]
            + sourceRot[eye][3] * sourceRot[eye][3];
        if (!std::isfinite(quatLengthSquared)
            || quatLengthSquared < 0.25f
            || quatLengthSquared > 4.0f)
        {
            return false;
        }
        for (int component = 0; component < 3; ++component)
        {
            if (!std::isfinite(sourcePos[eye][component]))
                return false;
        }
        for (int component = 0; component < 4; ++component)
        {
            if (!std::isfinite(sourceFov[eye][component]))
                return false;
        }
        sourceViews[eye].pose.orientation = {
            sourceRot[eye][0], sourceRot[eye][1], sourceRot[eye][2], sourceRot[eye][3] };
        sourceViews[eye].pose.position = {
            sourcePos[eye][0], sourcePos[eye][1], sourcePos[eye][2] };
        sourceViews[eye].fov = {
            sourceFov[eye][0], sourceFov[eye][1], sourceFov[eye][2], sourceFov[eye][3] };
    }

    const int sourceWidth = header->width;
    const int sourceHeight = header->height;
    const int sourcePitch = header->pitchBytes;
    if (sourceWidth <= 0
        || sourceHeight <= 0
        || sourceWidth > static_cast<int>(SharedVideoMaxWidth)
        || sourceHeight > static_cast<int>(SharedVideoMaxHeight)
        || sourcePitch < sourceWidth * 4)
    {
        return false;
    }

    const std::uint32_t requiredRightOffset =
        static_cast<std::uint32_t>(sizeof(SharedStereoHeader)
            + static_cast<std::size_t>(sourcePitch) * sourceHeight);
    const std::uint64_t payloadEnd = static_cast<std::uint64_t>(header->rightPayloadOffset)
        + static_cast<std::uint64_t>(sourcePitch) * sourceHeight;
    if (header->rightPayloadOffset != requiredRightOffset || payloadEnd > header->totalMappingBytes)
        return false;
    const auto* leftPixels = renderer.sharedStereoView + header->leftPayloadOffset;
    const auto* rightPixels = renderer.sharedStereoView + header->rightPayloadOffset;
    copySharedPlaneToTexturePixels(
        leftPixels,
        sourceWidth,
        sourceHeight,
        sourcePitch,
        renderer.gameTextureWidth,
        renderer.gameTextureHeight,
        eyePixels[0]);
    copySharedPlaneToTexturePixels(
        rightPixels,
        sourceWidth,
        sourceHeight,
        sourcePitch,
        renderer.gameTextureWidth,
        renderer.gameTextureHeight,
        eyePixels[1]);

    if (header->writing || header->sequence != sequenceBefore)
        return false;

    renderer.lastSharedStereoSequence = sequenceBefore;
    renderer.stereoReferenceSpaceGeneration = sourceReferenceGeneration;
    renderer.stereoProducerEpoch = sourceProducerEpoch;
    renderer.stereoSourceViews = sourceViews;
    poseSequenceOut = poseSequence;
    renderedDisplayTimeOut = renderedDisplayTime;
    sequenceOut = sequenceBefore;
    sourceAspect = static_cast<float>(sourceWidth) / static_cast<float>(sourceHeight);
    return true;
}

bool readSharedStereoState(
    Renderer& renderer,
    bool& uiActive,
    bool& worldReady,
    LONG* sequenceOut = nullptr)
{
    if (sequenceOut)
        *sequenceOut = 0;

    if (!renderer.sharedStereoView)
    {
        renderer.sharedStereoMapping = OpenFileMappingA(
            FILE_MAP_READ,
            FALSE,
            fnvxr::shared::D3D9StereoFrameSharedMappingName);
        if (!renderer.sharedStereoMapping)
            return false;

        constexpr SIZE_T mappingSize =
            sizeof(SharedStereoHeader) + SharedVideoMaxWidth * SharedVideoMaxHeight * 4 * 2;
        renderer.sharedStereoView =
            static_cast<std::uint8_t*>(MapViewOfFile(renderer.sharedStereoMapping, FILE_MAP_READ, 0, 0, mappingSize));
        if (!renderer.sharedStereoView)
        {
            CloseHandle(renderer.sharedStereoMapping);
            renderer.sharedStereoMapping = nullptr;
            return false;
        }
    }

    const auto* header = reinterpret_cast<const SharedStereoHeader*>(renderer.sharedStereoView);
    constexpr std::uint32_t mappingSize =
        sizeof(SharedStereoHeader) + SharedVideoMaxWidth * SharedVideoMaxHeight * 4 * 2;
    if (header->magic != SharedStereoMagic
        || header->version != fnvxr::shared::D3D9StereoFrameSharedVersion
        || header->headerBytes != sizeof(SharedStereoHeader)
        || header->leftPayloadOffset != sizeof(SharedStereoHeader)
        || header->totalMappingBytes != mappingSize
        || header->writing)
        return false;

    const LONG sequenceBefore = header->sequence;
    MemoryBarrier();
    const LONG width = header->width;
    const LONG height = header->height;
    const LONG pitchBytes = header->pitchBytes;
    const bool separated = header->separated != 0;
    const bool worldCandidate = header->worldCandidate != 0;
    const bool stereoUiActive = header->uiActive != 0;
    const bool poseValid = header->poseValid != 0;
    const bool poseTimed = header->renderedDisplayTime > 0;
    const bool coherentSameTickProducer =
        (header->producerMode == static_cast<LONG>(fnvxr::shared::StereoProducerNativeSameFrame)
            || header->producerMode == static_cast<LONG>(fnvxr::shared::StereoProducerSingleTraversal))
        && header->renderPairSequence > 0;
    const bool acceptedProducer =
        !envEnabled("FNVXR_REQUIRE_NATIVE_STEREO", true) || coherentSameTickProducer;
    MemoryBarrier();
    const LONG sequenceAfter = header->sequence;
    if (sequenceBefore != sequenceAfter || header->writing)
        return false;

    if (sequenceOut)
        *sequenceOut = sequenceAfter;

    uiActive = stereoUiActive;
    worldReady =
        !stereoUiActive
        && separated
        && worldCandidate
        && poseValid
        && poseTimed
        && acceptedProducer
        && width > 0
        && height > 0
        && width <= static_cast<LONG>(SharedVideoMaxWidth)
        && height <= static_cast<LONG>(SharedVideoMaxHeight)
        && pitchBytes >= width * 4;
    return true;
}

bool readSharedRuntimeUiActive(
    Renderer& renderer,
    bool& uiActive,
    bool& gameplayActive,
    bool& cameraActive,
    std::uint32_t& menuBits)
{
    if (!renderer.sharedRuntimeState)
    {
        renderer.sharedRuntimeMapping = OpenFileMappingA(FILE_MAP_READ, FALSE, "Local\\FNVXR_Runtime_State");
        if (!renderer.sharedRuntimeMapping)
            return false;

        renderer.sharedRuntimeState = static_cast<fnvxr::shared::SharedRuntimeState*>(
            MapViewOfFile(renderer.sharedRuntimeMapping, FILE_MAP_READ, 0, 0, sizeof(fnvxr::shared::SharedRuntimeState)));
        if (!renderer.sharedRuntimeState)
        {
            CloseHandle(renderer.sharedRuntimeMapping);
            renderer.sharedRuntimeMapping = nullptr;
            return false;
        }
    }

    const auto* state = renderer.sharedRuntimeState;
    const LONG sequenceBefore = state->sequence;
    if (sequenceBefore & 1)
        return false;

    MemoryBarrier();
    fnvxr::shared::SharedRuntimeState snapshot {};
    std::memcpy(&snapshot, state, sizeof(snapshot));
    MemoryBarrier();

    const LONG sequenceAfter = state->sequence;
    if (sequenceBefore != sequenceAfter || (sequenceAfter & 1))
        return false;

    if (snapshot.magic != fnvxr::shared::RuntimeSharedMagic
        || snapshot.version != fnvxr::shared::RuntimeSharedVersion)
    {
        return false;
    }

    cameraActive = snapshot.cameraActive != 0u;
    menuBits = snapshot.menuBits;
    gameplayActive = fnvxr::shared::runtimeGameplayPhase(snapshot.phase, snapshot.menuBits, snapshot.showroomActive);
    // Runtime opens the world probe; a presentable stereo header decides when the quad can disappear.
    uiActive = !gameplayActive;
    return true;
}

template <typename T>
T* mapOrCreateSharedState(HANDLE& mapping, const char* mappingName, bool& existed)
{
    mapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        static_cast<DWORD>(sizeof(T)),
        mappingName);
    if (!mapping)
        return nullptr;

    existed = GetLastError() == ERROR_ALREADY_EXISTS;
    return static_cast<T*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(T)));
}

std::uint8_t triggerByte(float value)
{
    return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}

std::int16_t thumbValue(float value)
{
    const float clamped = std::clamp(value, -1.0f, 1.0f);
    const long scaled = lroundf(clamped * 32767.0f);
    return static_cast<std::int16_t>(std::clamp<long>(scaled, -32767, 32767));
}

bool initHostSharedBridge(HostSharedBridge& bridge)
{
    bridge.producerEpoch = static_cast<std::uint32_t>(
        GetTickCount64() ^ (static_cast<std::uint64_t>(GetCurrentProcessId()) << 16));
    if (bridge.producerEpoch == 0)
        bridge.producerEpoch = 1;
    bridge.inputProducerMutex = CreateMutexA(
        nullptr,
        TRUE,
        fnvxr::shared::InputCoreProducerMutexName);
    if (!bridge.inputProducerMutex)
        return false;
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        const DWORD waitResult = WaitForSingleObject(bridge.inputProducerMutex, 0);
        if (waitResult != WAIT_OBJECT_0 && waitResult != WAIT_ABANDONED)
        {
            CloseHandle(bridge.inputProducerMutex);
            bridge.inputProducerMutex = nullptr;
            return false;
        }
    }
    bridge.ownsInputProducerMutex = true;

    bool xinputExisted = false;
    bool dinputExisted = false;
    bool vrPoseExisted = false;
    bool playerExisted = false;
    bridge.xinputState = mapOrCreateSharedState<fnvxr::shared::SharedXInputState>(
        bridge.xinputMapping,
        fnvxr::shared::XInputSharedMappingName,
        xinputExisted);
    bridge.dinputState = mapOrCreateSharedState<fnvxr::shared::SharedDInputState>(
        bridge.dinputMapping,
        fnvxr::shared::DInputSharedMappingName,
        dinputExisted);
    bridge.vrPoseState = mapOrCreateSharedState<fnvxr::shared::SharedVrPoseState>(
        bridge.vrPoseMapping,
        fnvxr::shared::VrPoseSharedMappingName,
        vrPoseExisted);
    bridge.playerState = mapOrCreateSharedState<fnvxr::shared::SharedPlayerState>(
        bridge.playerMapping,
        "Local\\FNVXR_Player_State",
        playerExisted);

    if (!bridge.xinputState || !bridge.dinputState || !bridge.vrPoseState || !bridge.playerState)
        return false;

    const bool xinputValid = xinputExisted
        && bridge.xinputState->magic == fnvxr::shared::XInputSharedMagic
        && bridge.xinputState->version == fnvxr::shared::XInputSharedVersion;
    if (!xinputValid)
    {
        std::memset(bridge.xinputState, 0, sizeof(*bridge.xinputState));
        bridge.xinputState->magic = fnvxr::shared::XInputSharedMagic;
        bridge.xinputState->version = fnvxr::shared::XInputSharedVersion;
    }
    else if ((bridge.xinputState->sequence & 1) != 0)
    {
        // The named producer lease proves no live host owns this abandoned
        // write. Keep it odd while replacing every producer field with a
        // neutral payload, then publish the repaired frame once.
        bridge.xinputState->magic = fnvxr::shared::XInputSharedMagic;
        bridge.xinputState->version = fnvxr::shared::XInputSharedVersion;
        bridge.xinputState->packet = 0;
        bridge.xinputState->buttons = 0;
        bridge.xinputState->leftTrigger = 0;
        bridge.xinputState->rightTrigger = 0;
        bridge.xinputState->leftThumbX = 0;
        bridge.xinputState->leftThumbY = 0;
        bridge.xinputState->rightThumbX = 0;
        bridge.xinputState->rightThumbY = 0;
        bridge.xinputState->connected = 0;
        fnvxr::shared::endSequencedSharedWrite(bridge.xinputState->sequence);
    }
    fnvxr::shared::SharedXInputState xinputSnapshot {};
    if (!fnvxr::shared::readSequencedSharedSnapshot(bridge.xinputState, xinputSnapshot, 1024))
        return false;
    bridge.xinputPacket = xinputSnapshot.packet;

    const bool dinputValid = dinputExisted
        && bridge.dinputState->magic == fnvxr::shared::DInputSharedMagic
        && bridge.dinputState->version == fnvxr::shared::DInputSharedVersion;
    if (!dinputValid)
    {
        std::memset(bridge.dinputState, 0, sizeof(*bridge.dinputState));
        bridge.dinputState->magic = fnvxr::shared::DInputSharedMagic;
        bridge.dinputState->version = fnvxr::shared::DInputSharedVersion;
    }
    else if ((bridge.dinputState->sequence & 1) != 0)
    {
        bridge.dinputState->magic = fnvxr::shared::DInputSharedMagic;
        bridge.dinputState->version = fnvxr::shared::DInputSharedVersion;
        bridge.dinputState->frame = 0;
        bridge.dinputState->clientX = 0;
        bridge.dinputState->clientY = 0;
        bridge.dinputState->pointerActive = 0;
        bridge.dinputState->mouseClickPacket = 0;
        // keyboardAcceptPacket is an independent plugin-owned atomic mailbox.
        bridge.dinputState->menuInputActive = 0;
        bridge.dinputState->gameplayControlsActive = 0;
        bridge.dinputState->leftStickX = 0;
        bridge.dinputState->leftStickY = 0;
        bridge.dinputState->rightStickX = 0;
        bridge.dinputState->rightStickY = 0;
        bridge.dinputState->headLookActive = 0;
        bridge.dinputState->headLookX = 0;
        bridge.dinputState->headLookY = 0;
        bridge.dinputState->gyroLookActive = 0;
        bridge.dinputState->gyroLookX = 0;
        bridge.dinputState->gyroLookY = 0;
        bridge.dinputState->leftGrip = 0;
        bridge.dinputState->rightGrip = 0;
        bridge.dinputState->gameplayFlags = 0;
        bridge.dinputState->aimTrigger = 0;
        fnvxr::shared::endSequencedSharedWrite(bridge.dinputState->sequence);
    }
    fnvxr::shared::SharedDInputState dinputSnapshot {};
    if (!fnvxr::shared::readSequencedSharedSnapshot(bridge.dinputState, dinputSnapshot, 1024))
        return false;
    bridge.dinputMouseClickPacket = dinputSnapshot.mouseClickPacket;
    bridge.dinputHeadLookX = dinputSnapshot.headLookX;
    bridge.dinputHeadLookY = dinputSnapshot.headLookY;
    bridge.dinputGyroLookX = dinputSnapshot.gyroLookX;
    bridge.dinputGyroLookY = dinputSnapshot.gyroLookY;
    if (fnvxr::shared::beginSequencedSharedWrite(bridge.dinputState->sequence))
    {
        bridge.dinputState->headLookActive = 0;
        bridge.dinputState->gyroLookActive = 0;
        bridge.dinputState->pointerActive = 0;
        bridge.dinputState->menuInputActive = 0;
        bridge.dinputState->gameplayControlsActive = 0;
        bridge.dinputState->gameplayFlags = 0;
        bridge.dinputState->aimTrigger = 0;
        fnvxr::shared::endSequencedSharedWrite(bridge.dinputState->sequence);
    }

    const bool vrPoseValid = vrPoseExisted
        && bridge.vrPoseState->magic == fnvxr::shared::VrPoseSharedMagic
        && bridge.vrPoseState->version == fnvxr::shared::VrPoseSharedVersion;
    if (!vrPoseValid)
    {
        std::memset(bridge.vrPoseState, 0, sizeof(*bridge.vrPoseState));
        bridge.vrPoseState->magic = fnvxr::shared::VrPoseSharedMagic;
        bridge.vrPoseState->version = fnvxr::shared::VrPoseSharedVersion;
        bridge.vrPoseState->hmdRot[3] = 1.0f;
        bridge.vrPoseState->leftRot[3] = 1.0f;
        bridge.vrPoseState->rightRot[3] = 1.0f;
        bridge.vrPoseState->leftAimRot[3] = 1.0f;
        bridge.vrPoseState->rightAimRot[3] = 1.0f;
        bridge.vrPoseState->leftEyeRot[3] = 1.0f;
        bridge.vrPoseState->rightEyeRot[3] = 1.0f;
    }
    else if ((bridge.vrPoseState->sequence & 1) != 0)
    {
        // The producer mutex proves this odd sequence was abandoned by an old
        // host. Finish that transaction with a completely neutral, untracked
        // pose so consumers can recover instead of spinning on it forever.
        bridge.vrPoseState->magic = fnvxr::shared::VrPoseSharedMagic;
        bridge.vrPoseState->version = fnvxr::shared::VrPoseSharedVersion;
        bridge.vrPoseState->frame = 0;
        bridge.vrPoseState->predictedDisplayTime = 0;
        bridge.vrPoseState->trackingFlags = 0;
        bridge.vrPoseState->referenceSpaceGeneration = 0;
        bridge.vrPoseState->producerEpoch = bridge.producerEpoch;
        fnvxr::shared::endSequencedSharedWrite(bridge.vrPoseState->sequence);
    }
    // The producer lease belongs to this process now. Always replace any
    // abandoned even payload with a neutral record carrying the new epoch
    // before bridge-ready is announced; otherwise a consumer can briefly act
    // on the previous host's still-tracked pose.
    if (!fnvxr::shared::beginSequencedSharedWrite(bridge.vrPoseState->sequence))
        return false;
    bridge.vrPoseState->magic = fnvxr::shared::VrPoseSharedMagic;
    bridge.vrPoseState->version = fnvxr::shared::VrPoseSharedVersion;
    bridge.vrPoseState->frame = 0;
    bridge.vrPoseState->predictedDisplayTime = 0;
    bridge.vrPoseState->trackingFlags = 0;
    bridge.vrPoseState->referenceSpaceGeneration = 0;
    bridge.vrPoseState->producerEpoch = bridge.producerEpoch;
    fnvxr::shared::endSequencedSharedWrite(bridge.vrPoseState->sequence);

    const bool playerValid = playerExisted
        && bridge.playerState->magic == fnvxr::shared::PlayerSharedMagic
        && bridge.playerState->version == fnvxr::shared::PlayerSharedVersion;
    if (!playerValid)
    {
        std::memset(bridge.playerState, 0, sizeof(*bridge.playerState));
        bridge.playerState->magic = fnvxr::shared::PlayerSharedMagic;
        bridge.playerState->version = fnvxr::shared::PlayerSharedVersion;
    }
    return true;
}

bool readSharedPlayerState(
    const HostSharedBridge& bridge,
    fnvxr::shared::SharedPlayerState& snapshot)
{
    const auto* state = bridge.playerState;
    if (!state)
        return false;

    for (int attempt = 0; attempt < 4; ++attempt)
    {
        const LONG sequenceBefore = state->sequence;
        if (sequenceBefore & 1)
        {
            YieldProcessor();
            continue;
        }

        MemoryBarrier();
        fnvxr::shared::SharedPlayerState candidate {};
        std::memcpy(&candidate, state, sizeof(candidate));
        MemoryBarrier();

        const LONG sequenceAfter = state->sequence;
        if (sequenceBefore != sequenceAfter || (sequenceAfter & 1))
        {
            YieldProcessor();
            continue;
        }
        if (candidate.magic != fnvxr::shared::PlayerSharedMagic
            || candidate.version != fnvxr::shared::PlayerSharedVersion)
        {
            return false;
        }

        snapshot = candidate;
        return true;
    }
    return false;
}

void writePoseToShared(const XrPosef& pose, float rot[4], float pos[3])
{
    rot[0] = pose.orientation.x;
    rot[1] = pose.orientation.y;
    rot[2] = pose.orientation.z;
    rot[3] = pose.orientation.w;
    pos[0] = pose.position.x;
    pos[1] = pose.position.y;
    pos[2] = pose.position.z;
}

void writeFovToShared(const XrFovf& fov, float out[4])
{
    out[0] = fov.angleLeft;
    out[1] = fov.angleRight;
    out[2] = fov.angleUp;
    out[3] = fov.angleDown;
}

void invalidateHostSharedBridgeTracking(HostSharedBridge& bridge, const char* reason)
{
    if (bridge.xinputState
        && fnvxr::shared::beginSequencedSharedWrite(bridge.xinputState->sequence))
    {
        bridge.xinputState->packet = ++bridge.xinputPacket;
        bridge.xinputState->buttons = 0;
        bridge.xinputState->leftTrigger = 0;
        bridge.xinputState->rightTrigger = 0;
        bridge.xinputState->leftThumbX = 0;
        bridge.xinputState->leftThumbY = 0;
        bridge.xinputState->rightThumbX = 0;
        bridge.xinputState->rightThumbY = 0;
        bridge.xinputState->connected = 0;
        fnvxr::shared::endSequencedSharedWrite(bridge.xinputState->sequence);
    }
    if (bridge.dinputState
        && fnvxr::shared::beginSequencedSharedWrite(bridge.dinputState->sequence))
    {
        ++bridge.dinputState->frame;
        bridge.dinputState->pointerActive = 0;
        bridge.dinputState->menuInputActive = 0;
        bridge.dinputState->gameplayControlsActive = 0;
        bridge.dinputState->leftStickX = 0;
        bridge.dinputState->leftStickY = 0;
        bridge.dinputState->rightStickX = 0;
        bridge.dinputState->rightStickY = 0;
        bridge.dinputState->headLookActive = 0;
        bridge.dinputState->gyroLookActive = 0;
        bridge.dinputState->leftGrip = 0;
        bridge.dinputState->rightGrip = 0;
        bridge.dinputState->gameplayFlags = 0;
        bridge.dinputState->aimTrigger = 0;
        fnvxr::shared::endSequencedSharedWrite(bridge.dinputState->sequence);
    }
    if (bridge.vrPoseState
        && fnvxr::shared::beginSequencedSharedWrite(bridge.vrPoseState->sequence))
    {
        ++bridge.vrPoseState->frame;
        bridge.vrPoseState->trackingFlags = 0;
        bridge.vrPoseState->predictedDisplayTime = 0;
        fnvxr::shared::endSequencedSharedWrite(bridge.vrPoseState->sequence);
    }
    std::cout << "sharedTrackingInvalidated reason=" << (reason ? reason : "unknown")
              << " producerEpoch=" << bridge.producerEpoch << "\n";
}

void closeHostSharedBridge(HostSharedBridge& bridge)
{
    if (bridge.xinputState
        && fnvxr::shared::beginSequencedSharedWrite(bridge.xinputState->sequence))
    {
        bridge.xinputState->magic = fnvxr::shared::XInputSharedMagic;
        bridge.xinputState->version = fnvxr::shared::XInputSharedVersion;
        bridge.xinputState->packet = ++bridge.xinputPacket;
        bridge.xinputState->buttons = 0;
        bridge.xinputState->leftTrigger = 0;
        bridge.xinputState->rightTrigger = 0;
        bridge.xinputState->leftThumbX = 0;
        bridge.xinputState->leftThumbY = 0;
        bridge.xinputState->rightThumbX = 0;
        bridge.xinputState->rightThumbY = 0;
        bridge.xinputState->connected = 0;
        fnvxr::shared::endSequencedSharedWrite(bridge.xinputState->sequence);
    }
    if (bridge.dinputState
        && fnvxr::shared::beginSequencedSharedWrite(bridge.dinputState->sequence))
    {
        ++bridge.dinputState->frame;
        bridge.dinputState->clientX = 0;
        bridge.dinputState->clientY = 0;
        bridge.dinputState->pointerActive = 0;
        bridge.dinputState->menuInputActive = 0;
        bridge.dinputState->gameplayControlsActive = 0;
        bridge.dinputState->leftStickX = 0;
        bridge.dinputState->leftStickY = 0;
        bridge.dinputState->rightStickX = 0;
        bridge.dinputState->rightStickY = 0;
        bridge.dinputState->headLookActive = 0;
        bridge.dinputState->gyroLookActive = 0;
        bridge.dinputState->leftGrip = 0;
        bridge.dinputState->rightGrip = 0;
        bridge.dinputState->gameplayFlags = 0;
        bridge.dinputState->aimTrigger = 0;
        fnvxr::shared::endSequencedSharedWrite(bridge.dinputState->sequence);
    }
    if (bridge.vrPoseState
        && fnvxr::shared::beginSequencedSharedWrite(bridge.vrPoseState->sequence))
    {
        ++bridge.vrPoseState->frame;
        bridge.vrPoseState->trackingFlags = 0;
        bridge.vrPoseState->predictedDisplayTime = 0;
        fnvxr::shared::endSequencedSharedWrite(bridge.vrPoseState->sequence);
    }
    if (bridge.xinputState)
    {
        UnmapViewOfFile(bridge.xinputState);
        bridge.xinputState = nullptr;
    }
    if (bridge.xinputMapping)
    {
        CloseHandle(bridge.xinputMapping);
        bridge.xinputMapping = nullptr;
    }
    if (bridge.dinputState)
    {
        UnmapViewOfFile(bridge.dinputState);
        bridge.dinputState = nullptr;
    }
    if (bridge.dinputMapping)
    {
        CloseHandle(bridge.dinputMapping);
        bridge.dinputMapping = nullptr;
    }
    if (bridge.vrPoseState)
    {
        UnmapViewOfFile(bridge.vrPoseState);
        bridge.vrPoseState = nullptr;
    }
    if (bridge.vrPoseMapping)
    {
        CloseHandle(bridge.vrPoseMapping);
        bridge.vrPoseMapping = nullptr;
    }
    if (bridge.playerState)
    {
        UnmapViewOfFile(bridge.playerState);
        bridge.playerState = nullptr;
    }
    if (bridge.playerMapping)
    {
        CloseHandle(bridge.playerMapping);
        bridge.playerMapping = nullptr;
    }
    if (bridge.inputProducerMutex)
    {
        if (bridge.ownsInputProducerMutex)
            ReleaseMutex(bridge.inputProducerMutex);
        CloseHandle(bridge.inputProducerMutex);
        bridge.inputProducerMutex = nullptr;
        bridge.ownsInputProducerMutex = false;
    }
}

void publishHostSharedBridge(
    HostSharedBridge& bridge,
    const fnvxr::PoseFrame& frame,
    XrTime predictedDisplayTime,
    const MenuPointer& menuPointer,
    const HeadspaceLook& headspaceLook,
    const HeadspaceLook& gyroAimLook,
    bool hostClick,
    bool menuInputActive,
    bool gameplayControlsActive,
    bool weaponOut,
    std::uint32_t runtimeMenuBits,
    std::uint32_t poseTrackingFlags,
    std::uint32_t referenceSpaceGeneration,
    const XrPosef& leftAimPose,
    const XrPosef& rightAimPose,
    const XrView* views,
    std::uint32_t viewCount)
{
    if (!bridge.xinputState || !bridge.dinputState || !bridge.vrPoseState)
        return;

    const bool suppressMenuAnalog =
        menuInputActive && envEnabled("FNVXR_XINPUT_MENU_SUPPRESS_ANALOG", true);
    const bool pipBoyRightStickNav =
        (runtimeMenuBits & fnvxr::shared::RuntimePipBoyMenuBit) != 0
        && envEnabled("FNVXR_PIPBOY_RIGHT_STICK_NAV", true);
    const bool pipBoySplitStickNav =
        (runtimeMenuBits & fnvxr::shared::RuntimePipBoyMenuBit) != 0
        && envEnabled("FNVXR_PIPBOY_SPLIT_STICK_NAV", true);
    const bool suppressLeftMenuAnalog = suppressMenuAnalog && !pipBoySplitStickNav;
    const bool suppressRightMenuAnalog = suppressMenuAnalog && !(pipBoyRightStickNav || pipBoySplitStickNav);
    const std::int16_t leftThumbX = suppressLeftMenuAnalog ? 0 : thumbValue(frame.leftThumbstickX);
    const std::int16_t leftThumbY = suppressLeftMenuAnalog ? 0 : thumbValue(frame.leftThumbstickY);
    const std::int16_t rightThumbX = suppressRightMenuAnalog ? 0 : thumbValue(frame.rightThumbstickX);
    const std::int16_t rightThumbY = suppressRightMenuAnalog ? 0 : thumbValue(frame.rightThumbstickY);
    std::uint16_t buttons = 0;
    if (menuInputActive && envEnabled("FNVXR_XINPUT_MENU_STICK_TO_DPAD", false))
    {
        const float dpadDeadzone = std::clamp(envFloat("FNVXR_XINPUT_MENU_DPAD_DEADZONE", 0.45f), 0.05f, 0.95f);
        if (frame.leftThumbstickY > dpadDeadzone)
            buttons |= XInputDpadUp;
        if (frame.leftThumbstickY < -dpadDeadzone)
            buttons |= XInputDpadDown;
        if (frame.leftThumbstickX < -dpadDeadzone)
            buttons |= XInputDpadLeft;
        if (frame.leftThumbstickX > dpadDeadzone)
            buttons |= XInputDpadRight;
    }
    if (frame.buttons & fnvxr::ButtonA)
        buttons |= XInputA;
    if (frame.buttons & fnvxr::ButtonB)
        buttons |= XInputB;
    if (frame.buttons & fnvxr::ButtonX)
        buttons |= XInputX;
    if (frame.buttons & fnvxr::ButtonY)
        buttons |= XInputY;
    if (envEnabled("FNVXR_XINPUT_PHYSICAL_MENU_BUTTONS_ENABLE", false))
    {
        if (frame.buttons & fnvxr::LeftMenu)
            buttons |= XInputBack;
        if (frame.buttons & fnvxr::RightMenu)
            buttons |= XInputStart;
    }
    if (frame.buttons & fnvxr::LeftThumbstick)
        buttons |= XInputLeftThumb;
    if (frame.buttons & fnvxr::RightThumbstick)
        buttons |= XInputRightThumb;
    static bool previousLeftGripPipBoyHeld = false;
    static bool previousRightGripMenuHeld = false;
    static int leftGripPipBoyPulseFrames = 0;
    static int rightGripMenuPulseFrames = 0;
    const float gripThreshold = std::clamp(envFloat("FNVXR_XINPUT_GRIP_MENU_THRESHOLD", 0.55f), 0.0f, 1.0f);
    const bool leftGripPipBoyHeld =
        envEnabled("FNVXR_XINPUT_LEFT_GRIP_PIPBOY_ENABLE", false)
        && frame.leftGrip > gripThreshold;
    const bool rightGripMenuHeld =
        envEnabled("FNVXR_XINPUT_RIGHT_GRIP_MENU_ENABLE", false)
        && frame.rightGrip > gripThreshold;
    const int gripPulseFrames = std::max(1, envInt("FNVXR_XINPUT_GRIP_PULSE_FRAMES", 6));
    if (leftGripPipBoyHeld && !previousLeftGripPipBoyHeld)
        leftGripPipBoyPulseFrames = gripPulseFrames;
    if (rightGripMenuHeld && !previousRightGripMenuHeld)
        rightGripMenuPulseFrames = gripPulseFrames;
    if (leftGripPipBoyPulseFrames > 0)
    {
        buttons |= XInputBack;
        --leftGripPipBoyPulseFrames;
    }
    if (rightGripMenuPulseFrames > 0)
    {
        buttons |= XInputStart;
        --rightGripMenuPulseFrames;
    }
    previousLeftGripPipBoyHeld = leftGripPipBoyHeld;
    previousRightGripMenuHeld = rightGripMenuHeld;
    if (fnvxr::shared::beginSequencedSharedWrite(bridge.xinputState->sequence))
    {
        bridge.xinputState->magic = fnvxr::shared::XInputSharedMagic;
        bridge.xinputState->version = fnvxr::shared::XInputSharedVersion;
        bridge.xinputState->connected = 1;
        bridge.xinputState->packet = ++bridge.xinputPacket;
        bridge.xinputState->buttons = buttons;
        bridge.xinputState->leftTrigger = triggerByte(frame.leftTrigger);
        bridge.xinputState->rightTrigger = triggerByte(frame.rightTrigger);
        bridge.xinputState->leftThumbX = leftThumbX;
        bridge.xinputState->leftThumbY = leftThumbY;
        bridge.xinputState->rightThumbX = rightThumbX;
        bridge.xinputState->rightThumbY = rightThumbY;
        fnvxr::shared::endSequencedSharedWrite(bridge.xinputState->sequence);
    }

    const float aimThreshold = std::clamp(envFloat("FNVXR_HEADSPACE_LOOK_AIM_TRIGGER", 0.35f), 0.0f, 1.0f);
    std::uint32_t gameplayFlags = 0;
    if (gameplayControlsActive && frame.leftTrigger >= aimThreshold)
        gameplayFlags |= fnvxr::shared::DInputGameplayFlagAimHeld;
    if (gameplayControlsActive && weaponOut)
        gameplayFlags |= fnvxr::shared::DInputGameplayFlagWeaponOut;
    if (gameplayControlsActive && (frame.buttons & fnvxr::LeftThumbstick) != 0)
        gameplayFlags |= fnvxr::shared::DInputGameplayFlagThirdPersonZoomHeld;
    const float normalizedX = std::clamp(menuPointer.x, 0.0f, 1.0f);
    const float normalizedY = std::clamp(menuPointer.y, 0.0f, 1.0f);
    const LONG sharedPointerWidth = static_cast<LONG>(std::max(2, envInt("FNVXR_UI_SHARED_WIDTH", SharedPointerWidth)));
    const LONG sharedPointerHeight = static_cast<LONG>(std::max(2, envInt("FNVXR_UI_SHARED_HEIGHT", SharedPointerHeight)));
    const LONG clientX = static_cast<LONG>(lroundf(normalizedX * static_cast<float>(sharedPointerWidth - 1)));
    const LONG clientY = static_cast<LONG>(lroundf(normalizedY * static_cast<float>(sharedPointerHeight - 1)));
    if (hostClick)
        ++bridge.dinputMouseClickPacket;
    bridge.dinputHeadLookX = fnvxr::shared::addWrappedInt32(
        bridge.dinputHeadLookX,
        headspaceLook.active ? headspaceLook.yawMicroradians : 0);
    bridge.dinputHeadLookY = fnvxr::shared::addWrappedInt32(
        bridge.dinputHeadLookY,
        headspaceLook.active ? headspaceLook.pitchMicroradians : 0);
    bridge.dinputGyroLookX = fnvxr::shared::addWrappedInt32(
        bridge.dinputGyroLookX,
        gyroAimLook.active ? gyroAimLook.yawMicroradians : 0);
    bridge.dinputGyroLookY = fnvxr::shared::addWrappedInt32(
        bridge.dinputGyroLookY,
        gyroAimLook.active ? gyroAimLook.pitchMicroradians : 0);
    if (fnvxr::shared::beginSequencedSharedWrite(bridge.dinputState->sequence))
    {
        bridge.dinputState->magic = fnvxr::shared::DInputSharedMagic;
        bridge.dinputState->version = fnvxr::shared::DInputSharedVersion;
        bridge.dinputState->frame = static_cast<std::uint32_t>(frame.frame);
        bridge.dinputState->clientX = clientX;
        bridge.dinputState->clientY = clientY;
        bridge.dinputState->pointerActive = menuPointer.active ? 1u : 0u;
        bridge.dinputState->mouseClickPacket = bridge.dinputMouseClickPacket;
        bridge.dinputState->menuInputActive = menuInputActive ? 1u : 0u;
        bridge.dinputState->gameplayControlsActive = gameplayControlsActive ? 1u : 0u;
        bridge.dinputState->leftStickX = static_cast<std::int32_t>(std::lround(std::clamp(frame.leftThumbstickX, -1.0f, 1.0f) * 32767.0f));
        bridge.dinputState->leftStickY = static_cast<std::int32_t>(std::lround(std::clamp(frame.leftThumbstickY, -1.0f, 1.0f) * 32767.0f));
        bridge.dinputState->rightStickX = static_cast<std::int32_t>(std::lround(std::clamp(frame.rightThumbstickX, -1.0f, 1.0f) * 32767.0f));
        bridge.dinputState->rightStickY = static_cast<std::int32_t>(std::lround(std::clamp(frame.rightThumbstickY, -1.0f, 1.0f) * 32767.0f));
        bridge.dinputState->headLookActive = headspaceLook.active ? 1u : 0u;
        bridge.dinputState->headLookX = bridge.dinputHeadLookX;
        bridge.dinputState->headLookY = bridge.dinputHeadLookY;
        bridge.dinputState->gyroLookActive = gyroAimLook.active ? 1u : 0u;
        bridge.dinputState->gyroLookX = bridge.dinputGyroLookX;
        bridge.dinputState->gyroLookY = bridge.dinputGyroLookY;
        bridge.dinputState->leftGrip = static_cast<std::int32_t>(std::lround(std::clamp(frame.leftGrip, 0.0f, 1.0f) * 32767.0f));
        bridge.dinputState->rightGrip = static_cast<std::int32_t>(std::lround(std::clamp(frame.rightGrip, 0.0f, 1.0f) * 32767.0f));
        bridge.dinputState->gameplayFlags = gameplayFlags;
        bridge.dinputState->aimTrigger = triggerByte(frame.leftTrigger);
        fnvxr::shared::endSequencedSharedWrite(bridge.dinputState->sequence);
    }

    if (!fnvxr::shared::beginSequencedSharedWrite(bridge.vrPoseState->sequence))
        return;
    bridge.vrPoseState->magic = fnvxr::shared::VrPoseSharedMagic;
    bridge.vrPoseState->version = fnvxr::shared::VrPoseSharedVersion;
    bridge.vrPoseState->trackingFlags = poseTrackingFlags;
    bridge.vrPoseState->referenceSpaceGeneration = referenceSpaceGeneration;
    bridge.vrPoseState->producerEpoch = bridge.producerEpoch;
    bridge.vrPoseState->frame = frame.frame;
    bridge.vrPoseState->predictedDisplayTime = static_cast<std::int64_t>(predictedDisplayTime);
    bridge.vrPoseState->hmdRot[0] = frame.hmdRot.x;
    bridge.vrPoseState->hmdRot[1] = frame.hmdRot.y;
    bridge.vrPoseState->hmdRot[2] = frame.hmdRot.z;
    bridge.vrPoseState->hmdRot[3] = frame.hmdRot.w;
    bridge.vrPoseState->hmdPos[0] = frame.hmdPos.x;
    bridge.vrPoseState->hmdPos[1] = frame.hmdPos.y;
    bridge.vrPoseState->hmdPos[2] = frame.hmdPos.z;
    bridge.vrPoseState->leftRot[0] = frame.leftRot.x;
    bridge.vrPoseState->leftRot[1] = frame.leftRot.y;
    bridge.vrPoseState->leftRot[2] = frame.leftRot.z;
    bridge.vrPoseState->leftRot[3] = frame.leftRot.w;
    bridge.vrPoseState->leftPos[0] = frame.leftPos.x;
    bridge.vrPoseState->leftPos[1] = frame.leftPos.y;
    bridge.vrPoseState->leftPos[2] = frame.leftPos.z;
    bridge.vrPoseState->rightRot[0] = frame.rightRot.x;
    bridge.vrPoseState->rightRot[1] = frame.rightRot.y;
    bridge.vrPoseState->rightRot[2] = frame.rightRot.z;
    bridge.vrPoseState->rightRot[3] = frame.rightRot.w;
    bridge.vrPoseState->rightPos[0] = frame.rightPos.x;
    bridge.vrPoseState->rightPos[1] = frame.rightPos.y;
    bridge.vrPoseState->rightPos[2] = frame.rightPos.z;
    writePoseToShared(
        leftAimPose,
        bridge.vrPoseState->leftAimRot,
        bridge.vrPoseState->leftAimPos);
    writePoseToShared(
        rightAimPose,
        bridge.vrPoseState->rightAimRot,
        bridge.vrPoseState->rightAimPos);
    if (views && viewCount >= 2)
    {
        writePoseToShared(views[0].pose, bridge.vrPoseState->leftEyeRot, bridge.vrPoseState->leftEyePos);
        writePoseToShared(views[1].pose, bridge.vrPoseState->rightEyeRot, bridge.vrPoseState->rightEyePos);
        writeFovToShared(views[0].fov, bridge.vrPoseState->leftFov);
        writeFovToShared(views[1].fov, bridge.vrPoseState->rightFov);
    }
    else
    {
        bridge.vrPoseState->leftEyeRot[0] = frame.hmdRot.x;
        bridge.vrPoseState->leftEyeRot[1] = frame.hmdRot.y;
        bridge.vrPoseState->leftEyeRot[2] = frame.hmdRot.z;
        bridge.vrPoseState->leftEyeRot[3] = frame.hmdRot.w;
        bridge.vrPoseState->leftEyePos[0] = frame.hmdPos.x - 0.032f;
        bridge.vrPoseState->leftEyePos[1] = frame.hmdPos.y;
        bridge.vrPoseState->leftEyePos[2] = frame.hmdPos.z;
        bridge.vrPoseState->rightEyeRot[0] = frame.hmdRot.x;
        bridge.vrPoseState->rightEyeRot[1] = frame.hmdRot.y;
        bridge.vrPoseState->rightEyeRot[2] = frame.hmdRot.z;
        bridge.vrPoseState->rightEyeRot[3] = frame.hmdRot.w;
        bridge.vrPoseState->rightEyePos[0] = frame.hmdPos.x + 0.032f;
        bridge.vrPoseState->rightEyePos[1] = frame.hmdPos.y;
        bridge.vrPoseState->rightEyePos[2] = frame.hmdPos.z;
        bridge.vrPoseState->leftFov[0] = 0.0f;
        bridge.vrPoseState->leftFov[1] = 0.0f;
        bridge.vrPoseState->leftFov[2] = 0.0f;
        bridge.vrPoseState->leftFov[3] = 0.0f;
        std::memcpy(bridge.vrPoseState->rightFov, bridge.vrPoseState->leftFov, sizeof(bridge.vrPoseState->rightFov));
    }
    fnvxr::shared::endSequencedSharedWrite(bridge.vrPoseState->sequence);
}

void closeSharedD3D9VideoFrame(Renderer& renderer)
{
    if (renderer.sharedVideoView)
    {
        UnmapViewOfFile(renderer.sharedVideoView);
        renderer.sharedVideoView = nullptr;
    }
    if (renderer.sharedVideoMapping)
    {
        CloseHandle(renderer.sharedVideoMapping);
        renderer.sharedVideoMapping = nullptr;
    }
    if (renderer.sharedWorldVideoView)
    {
        UnmapViewOfFile(renderer.sharedWorldVideoView);
        renderer.sharedWorldVideoView = nullptr;
    }
    if (renderer.sharedWorldVideoMapping)
    {
        CloseHandle(renderer.sharedWorldVideoMapping);
        renderer.sharedWorldVideoMapping = nullptr;
    }
    if (renderer.sharedStereoView)
    {
        UnmapViewOfFile(renderer.sharedStereoView);
        renderer.sharedStereoView = nullptr;
    }
    if (renderer.sharedStereoMapping)
    {
        CloseHandle(renderer.sharedStereoMapping);
        renderer.sharedStereoMapping = nullptr;
    }
    if (renderer.sharedRuntimeState)
    {
        UnmapViewOfFile(renderer.sharedRuntimeState);
        renderer.sharedRuntimeState = nullptr;
    }
    if (renderer.sharedRuntimeMapping)
    {
        CloseHandle(renderer.sharedRuntimeMapping);
        renderer.sharedRuntimeMapping = nullptr;
    }
}

bool updateGameTexture(ID3D11Device* device, Renderer& renderer)
{
    const auto updateStart = std::chrono::steady_clock::now();
    const int textureWidth = renderer.gameTextureWidth;
    const int textureHeight = renderer.gameTextureHeight;
    static std::vector<std::uint32_t> pixels;
    bool captured = true;
    float sourceAspect = static_cast<float>(textureWidth) / static_cast<float>(textureHeight);
    if (envEquals("FNVXR_GAME_PLANE_SOURCE", "test"))
    {
        fillGamePlaneTestPattern(textureWidth, textureHeight, pixels);
    }
    else
    {
        const bool preferWindowSource = envEquals("FNVXR_GAME_PLANE_SOURCE", "window");
        bool fromShared = false;
        if (!preferWindowSource)
        {
            captured = readSharedD3D9VideoFrame(renderer, pixels, sourceAspect);
            fromShared = captured;
        }
        if (!captured && preferWindowSource)
            captured = captureFalloutWindowBgra(textureWidth, textureHeight, pixels, sourceAspect);
        if (captured
            && fromShared
            && envEnabled("FNVXR_GAME_PLANE_WINDOW_FALLBACK_ON_BLACK", true)
            && isMostlyBlackFrame(pixels))
        {
            float windowAspect = sourceAspect;
            if (captureFalloutWindowBgra(textureWidth, textureHeight, pixels, windowAspect))
            {
                sourceAspect = windowAspect;
                static uint64_t blackFallbacks = 0;
                ++blackFallbacks;
                if (blackFallbacks <= 5 || blackFallbacks % 120 == 0)
                {
                    std::cout << "gameTexture blackFallback=" << blackFallbacks
                              << " seq=" << renderer.lastSharedVideoSequence
                              << " aspect=" << sourceAspect
                              << "\n";
                }
            }
        }
        if (!captured && envEquals("FNVXR_GAME_PLANE_SOURCE", "window"))
            captured = captureFalloutWindowBgra(textureWidth, textureHeight, pixels, sourceAspect);
        if (!captured && renderer.hasGameTextureFrame)
        {
            ++renderer.gameTextureMisses;
            return true;
        }
        if (!captured)
        {
            ++renderer.gameTextureMisses;
            fillGamePlaneTestPattern(textureWidth, textureHeight, pixels);
        }
    }
    const auto sourceReady = std::chrono::steady_clock::now();

    ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(&context);
    const auto uploadStart = std::chrono::steady_clock::now();
    context->UpdateSubresource(
        renderer.gameTexture.Get(),
        0,
        nullptr,
        pixels.data(),
        static_cast<UINT>(textureWidth * sizeof(std::uint32_t)),
        0);
    const auto uploadEnd = std::chrono::steady_clock::now();
    renderer.gameTextureAspect = sourceAspect;
    renderer.hasGameTextureFrame = captured;
    ++renderer.gameTextureUpdates;
    const double sourceMs = std::chrono::duration<double, std::milli>(sourceReady - updateStart).count();
    const double uploadMs = std::chrono::duration<double, std::milli>(uploadEnd - uploadStart).count();
    const double totalMs = std::chrono::duration<double, std::milli>(uploadEnd - updateStart).count();
    const double slowMs = static_cast<double>(envFloat("FNVXR_HOST_GAME_TEXTURE_SLOW_MS", 2.0f));
    const bool slow = totalMs >= slowMs;
    if (renderer.gameTextureUpdates <= 5 || renderer.gameTextureUpdates % 120 == 0 || !captured || slow)
    {
        std::cout << "gameTexture update=" << renderer.gameTextureUpdates
                  << " captured=" << static_cast<int>(captured)
                  << " misses=" << renderer.gameTextureMisses
                  << " sharedMapped=" << static_cast<int>(renderer.sharedVideoView != nullptr)
                  << " seq=" << renderer.lastSharedVideoSequence
                  << " aspect=" << sourceAspect
                  << " sourceMs=" << sourceMs
                  << " uploadMs=" << uploadMs
                  << " totalMs=" << totalMs
                  << " slow=" << static_cast<int>(slow)
                  << "\n";
    }

    return captured;
}

bool updateWorldTexture(ID3D11Device* device, Renderer& renderer)
{
    if (!gamePlaneModeSettings(gamePlaneMode()).useWideSource)
    {
        renderer.hasWorldTextureFrame = false;
        return false;
    }

    const auto updateStart = std::chrono::steady_clock::now();
    static std::vector<std::uint32_t> pixels;
    float sourceAspect = static_cast<float>(renderer.gameTextureWidth) / static_cast<float>(renderer.gameTextureHeight);
    const bool captured = readSharedD3D9WorldVideoFrame(renderer, pixels, sourceAspect);
    if (!captured)
    {
        ++renderer.worldTextureMisses;
        const bool holdingPrevious = renderer.hasWorldTextureFrame && renderer.worldTextureView;
        if ((!holdingPrevious && renderer.worldTextureMisses <= 8) || renderer.worldTextureMisses % 120 == 0)
        {
            const char* mappingName = std::getenv("FNVXR_D3D9_WIDE_WORLD_FRAME_MAPPING");
            if (!mappingName || !*mappingName)
                mappingName = "Local\\FNVXR_D3D9_WorldFrame_v1";
            std::cout << "worldTexture miss=" << renderer.worldTextureMisses
                      << " mapped=" << static_cast<int>(renderer.sharedWorldVideoView != nullptr)
                      << " holdingPrevious=" << static_cast<int>(holdingPrevious)
                      << " seq=" << renderer.lastSharedWorldVideoSequence
                      << " mapping=" << mappingName
                      << "\n";
        }
        return false;
    }

    ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(&context);
    const auto uploadStart = std::chrono::steady_clock::now();
    context->UpdateSubresource(
        renderer.worldTexture.Get(),
        0,
        nullptr,
        pixels.data(),
        static_cast<UINT>(renderer.gameTextureWidth * sizeof(std::uint32_t)),
        0);
    const auto uploadEnd = std::chrono::steady_clock::now();

    renderer.hasWorldTextureFrame = true;
    ++renderer.worldTextureUpdates;
    renderer.worldTextureMisses = 0;
    const double sourceMs = std::chrono::duration<double, std::milli>(uploadStart - updateStart).count();
    const double uploadMs = std::chrono::duration<double, std::milli>(uploadEnd - uploadStart).count();
    const double totalMs = std::chrono::duration<double, std::milli>(uploadEnd - updateStart).count();
    const double slowMs = static_cast<double>(envFloat("FNVXR_HOST_GAME_TEXTURE_SLOW_MS", 2.0f));
    const bool slow = totalMs >= slowMs;
    if (renderer.worldTextureUpdates <= 5 || renderer.worldTextureUpdates % 120 == 0 || slow)
    {
        std::cout << "worldTexture update=" << renderer.worldTextureUpdates
                  << " seq=" << renderer.lastSharedWorldVideoSequence
                  << " aspect=" << sourceAspect
                  << " sourceMs=" << sourceMs
                  << " uploadMs=" << uploadMs
                  << " totalMs=" << totalMs
                  << " slow=" << static_cast<int>(slow)
                  << "\n";
    }

    return true;
}

bool updateStereoGameTextures(ID3D11Device* device, Renderer& renderer)
{
    if (renderer.stereoGameFrameSequence == 0)
    {
        renderer.stereoTransientReadMisses = 0;
        renderer.stereoAcceptedAt = {};
    }

    if (renderer.hasCachedSceneFrame && envEnabled("FNVXR_CACHE_ONLY", false))
        return true;

    if (!stereoWorldRuntimeEnabled())
    {
        renderer.hasStereoGameFrame = false;
        renderer.stereoGameFrameSeparated = false;
        renderer.stereoGameFrameWorldCandidate = false;
        renderer.stereoGameFrameSequence = 0;
        renderer.stereoGamePoseSequence = 0;
        renderer.stereoGameRenderedDisplayTime = 0;
        renderer.stereoStableFrameCount = 0;
        renderer.stereoFrameMisses = 0;
        renderer.stereoTransientReadMisses = 0;
        return false;
    }

    static std::array<std::vector<std::uint32_t>, 2> eyePixels;
    float sourceAspect = static_cast<float>(renderer.gameTextureWidth) / static_cast<float>(renderer.gameTextureHeight);
    bool separated = false;
    bool worldCandidate = false;
    LONG sourceSequence = 0;
    LONG poseSequence = 0;
    XrTime renderedDisplayTime = 0;
    if (!readSharedD3D9StereoFrame(
            renderer,
            eyePixels,
            sourceAspect,
            separated,
            worldCandidate,
            poseSequence,
            renderedDisplayTime,
            sourceSequence))
    {
        // The OpenXR consumer commonly polls faster than the retail D3D9
        // producer. Reusing the same still-valid sequence is not a stereo
        // failure, but retaining an explicitly invalidated sequence can show
        // stale actors or stale cell geometry. Keep only a coherent source
        // sequence that the producer still advertises as world-ready.
        bool sourceUiActive = false;
        bool sourceWorldReady = false;
        LONG advertisedSequence = 0;
        const bool haveSourceState = readSharedStereoState(
            renderer,
            sourceUiActive,
            sourceWorldReady,
            &advertisedSequence);
        if (haveSourceState
            && !sourceUiActive
            && sourceWorldReady
            && advertisedSequence == renderer.stereoGameFrameSequence
            && renderer.stereoGameFrameSequence > 0
            && renderer.stereoStableFrameCount > 0)
        {
            const auto now = std::chrono::steady_clock::now();
            const uint64_t maxSameSequenceMs = static_cast<uint64_t>(
                std::max(1, envInt("FNVXR_STEREO_MAX_SAME_SEQUENCE_MS", 250)));
            const bool producerFresh = renderer.stereoAcceptedAt.time_since_epoch().count() != 0
                && now >= renderer.stereoAcceptedAt
                && static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - renderer.stereoAcceptedAt).count()) <= maxSameSequenceMs;
            if (producerFresh)
            {
                renderer.stereoFrameMisses = 0;
                renderer.stereoTransientReadMisses = 0;
                return true;
            }

            std::cout << "stereoGameFrame same sequence expired"
                      << " seq=" << renderer.stereoGameFrameSequence
                      << " maxAgeMs=" << maxSameSequenceMs
                      << "; falling back to live retail plane\n";
        }

        const uint64_t transientReadGracePolls = static_cast<uint64_t>(
            std::max(1, envInt("FNVXR_STEREO_TRANSIENT_READ_GRACE_POLLS", 4)));
        if (!haveSourceState)
            ++renderer.stereoTransientReadMisses;
        else
            renderer.stereoTransientReadMisses = 0;
        if (!haveSourceState
            && renderer.stereoGameFrameSequence > 0
            && renderer.stereoStableFrameCount > 0
            && renderer.stereoTransientReadMisses <= transientReadGracePolls)
        {
            if (renderer.stereoTransientReadMisses == 1)
            {
                std::cout << "stereoGameFrame transient producer read overlap"
                          << " seq=" << renderer.stereoGameFrameSequence
                          << " stable=" << renderer.stereoStableFrameCount
                          << " grace=" << renderer.stereoTransientReadMisses << "/" << transientReadGracePolls
                          << "\n";
            }
            return true;
        }

        ++renderer.stereoFrameMisses;
        const uint64_t staleLimit =
            static_cast<uint64_t>(std::max(1, envInt("FNVXR_STEREO_STALE_FRAME_LIMIT", 2)));
        if (renderer.hasStereoGameFrame
            && envEnabled("FNVXR_STEREO_RETAIN_LAST_VALID_ON_REJECT", false)
            && renderer.stereoFrameMisses <= staleLimit)
        {
            return true;
        }
        renderer.hasStereoGameFrame = false;
        renderer.stereoGameFrameSeparated = false;
        renderer.stereoGameFrameWorldCandidate = false;
        renderer.stereoStableFrameCount = 0;
        return false;
    }

    const bool requireWorldStereo = envEnabled("FNVXR_REQUIRE_WORLD_STEREO", true);
    const StereoPixelStats pixelStats = analyzeStereoPixels(
        eyePixels,
        renderer.gameTextureWidth,
        renderer.gameTextureHeight);
    const size_t minNonBlackSamples =
        static_cast<size_t>(std::max(1, envInt("FNVXR_STEREO_MIN_NONBLACK_SAMPLES", 8)));
    const size_t minDifferentSamples =
        static_cast<size_t>(std::max(1, envInt("FNVXR_STEREO_HOST_MIN_DIFF_SAMPLES", 1)));
    const size_t minActiveTiles =
        static_cast<size_t>(std::max(1, envInt("FNVXR_STEREO_HOST_MIN_ACTIVE_TILES", 12)));
    const size_t minDifferentTiles =
        static_cast<size_t>(std::max(1, envInt("FNVXR_STEREO_HOST_MIN_DIFFERENT_TILES", 8)));
    const bool contentCandidate =
        pixelStats.nonBlackSamples >= minNonBlackSamples
        && pixelStats.differentSamples >= minDifferentSamples
        && pixelStats.leftActiveTiles >= minActiveTiles
        && pixelStats.rightActiveTiles >= minActiveTiles
        && pixelStats.differentTiles >= minDifferentTiles;
    if (!separated || !contentCandidate || (requireWorldStereo && !worldCandidate))
    {
        ++renderer.stereoFrameMisses;
        const uint64_t staleLimit =
            static_cast<uint64_t>(std::max(1, envInt("FNVXR_STEREO_STALE_FRAME_LIMIT", 2)));
        if (renderer.hasStereoGameFrame
            && envEnabled("FNVXR_STEREO_RETAIN_LAST_VALID_ON_REJECT", false)
            && renderer.stereoFrameMisses <= staleLimit)
        {
            if (renderer.stereoFrameMisses <= 5 || renderer.stereoFrameMisses % 120 == 0)
            {
                std::cout << "stereoGameFrame retained previous valid rejected separated=" << static_cast<int>(separated)
                          << " seq=" << sourceSequence
                          << " poseSeq=" << poseSequence
                          << " worldCandidate=" << static_cast<int>(worldCandidate)
                          << " content=" << static_cast<int>(contentCandidate)
                          << " nonBlack=" << pixelStats.nonBlackSamples << "/" << pixelStats.samples
                          << " diff=" << pixelStats.differentSamples
                          << " activeTiles=" << pixelStats.leftActiveTiles << "/" << pixelStats.rightActiveTiles
                          << " diffTiles=" << pixelStats.differentTiles
                          << " leftHash=0x" << std::hex << pixelStats.leftHash
                          << " rightHash=0x" << pixelStats.rightHash << std::dec
                          << " requireWorld=" << static_cast<int>(requireWorldStereo)
                          << " misses=" << renderer.stereoFrameMisses
                          << "\n";
            }
            return true;
        }
        renderer.hasStereoGameFrame = false;
        renderer.stereoGameFrameSeparated = separated;
        renderer.stereoGameFrameWorldCandidate = worldCandidate;
        renderer.stereoGameFrameSequence = sourceSequence;
        renderer.stereoGamePoseSequence = poseSequence;
        renderer.stereoGameRenderedDisplayTime = renderedDisplayTime;
        renderer.stereoStableFrameCount = 0;
        if (renderer.stereoFrameMisses <= 5 || renderer.stereoFrameMisses % 120 == 0)
        {
            std::cout << "stereoGameFrame rejected separated=" << static_cast<int>(separated)
                      << " seq=" << sourceSequence
                      << " poseSeq=" << poseSequence
                      << " worldCandidate=" << static_cast<int>(worldCandidate)
                      << " content=" << static_cast<int>(contentCandidate)
                      << " nonBlack=" << pixelStats.nonBlackSamples << "/" << pixelStats.samples
                      << " diff=" << pixelStats.differentSamples
                      << " activeTiles=" << pixelStats.leftActiveTiles << "/" << pixelStats.rightActiveTiles
                      << " diffTiles=" << pixelStats.differentTiles
                      << " leftHash=0x" << std::hex << pixelStats.leftHash
                      << " rightHash=0x" << pixelStats.rightHash << std::dec
                      << " requireWorld=" << static_cast<int>(requireWorldStereo)
                      << " misses=" << renderer.stereoFrameMisses
                      << "\n";
        }
        return false;
    }

    ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(&context);
    for (size_t eye = 0; eye < eyePixels.size(); ++eye)
    {
        context->UpdateSubresource(
            renderer.stereoGameTextures[eye].Get(),
            0,
            nullptr,
            eyePixels[eye].data(),
            static_cast<UINT>(renderer.gameTextureWidth * sizeof(std::uint32_t)),
            0);
    }

    renderer.gameTextureAspect = sourceAspect;
    const uint64_t minStableFrames =
        static_cast<uint64_t>(std::max(1, envInt("FNVXR_STEREO_STABLE_HANDOFF_FRAMES", 6)));
    if (renderer.stereoGameFrameSequence != 0
        && sourceSequence > renderer.stereoGameFrameSequence)
    {
        ++renderer.stereoStableFrameCount;
    }
    else if (renderer.stereoGameFrameSequence == 0)
    {
        renderer.stereoStableFrameCount = 1;
    }
    renderer.stereoGameFrameSequence = sourceSequence;
    renderer.stereoGamePoseSequence = poseSequence;
    renderer.stereoGameRenderedDisplayTime = renderedDisplayTime;
    renderer.hasStereoGameFrame = renderer.stereoStableFrameCount >= minStableFrames;
    renderer.stereoGameFrameSeparated = true;
    renderer.stereoGameFrameWorldCandidate = worldCandidate;
    renderer.stereoFrameMisses = 0;
    renderer.stereoTransientReadMisses = 0;
    renderer.stereoAcceptedAt = std::chrono::steady_clock::now();
    renderer.stereoPixelSamples = pixelStats.samples;
    renderer.stereoNonBlackSamples = pixelStats.nonBlackSamples;
    renderer.stereoMeaningfulDifferentSamples = pixelStats.differentSamples;
    renderer.stereoLeftActiveTiles = pixelStats.leftActiveTiles;
    renderer.stereoRightActiveTiles = pixelStats.rightActiveTiles;
    renderer.stereoDifferentTiles = pixelStats.differentTiles;
    renderer.stereoLeftHash = pixelStats.leftHash;
    renderer.stereoRightHash = pixelStats.rightHash;
    ++renderer.lastStereoFrameUpdated;
    if (renderer.lastStereoFrameUpdated <= 8
        || renderer.lastStereoFrameUpdated % 60 == 0
        || renderer.stereoStableFrameCount <= minStableFrames)
    {
        std::cout << "stereoGameFrame updated="
                  << renderer.lastStereoFrameUpdated
                  << " seq=" << sourceSequence
                  << " poseSeq=" << poseSequence
                  << " stable=" << renderer.stereoStableFrameCount << "/" << minStableFrames
                  << " ready=" << static_cast<int>(renderer.hasStereoGameFrame)
                  << " aspect=" << sourceAspect
                  << " separated=" << static_cast<int>(separated)
                  << " worldCandidate=" << static_cast<int>(worldCandidate)
                  << " nonBlack=" << pixelStats.nonBlackSamples << "/" << pixelStats.samples
                  << " diff=" << pixelStats.differentSamples
                  << " activeTiles=" << pixelStats.leftActiveTiles << "/" << pixelStats.rightActiveTiles
                  << " diffTiles=" << pixelStats.differentTiles
                  << " leftHash=0x" << std::hex << pixelStats.leftHash
                  << " rightHash=0x" << pixelStats.rightHash << std::dec
                  << "\n";
    }
    return true;
}

bool loadSceneCacheIntoStereoTextures(ID3D11Device* device, Renderer& renderer)
{
    if (!device)
        return false;

    const std::wstring artifactPath = sceneCacheRawArtifactPath();
    if (artifactPath.empty())
    {
        std::cerr << "sceneCache raw loaded=0 reason=no_complete_artifact\n";
        return false;
    }

    constexpr size_t maxSceneBytes = sizeof(FnvxrSceneCacheHeader)
        + static_cast<size_t>(SharedVideoMaxWidth) * static_cast<size_t>(SharedVideoMaxHeight) * 4u * 2u;
    const std::vector<std::uint8_t> bytes = readBinaryFile(artifactPath, maxSceneBytes);
    if (bytes.size() < sizeof(FnvxrSceneCacheHeader))
    {
        std::wcerr << L"sceneCache raw loaded=0 path=" << artifactPath << L" reason=short_file\n";
        return false;
    }

    FnvxrSceneCacheHeader header {};
    std::memcpy(&header, bytes.data(), sizeof(header));
    const char expectedMagic[8] = { 'F', 'N', 'V', 'X', 'S', 'C', 'N', '\0' };
    const bool separated = (header.flags & 1u) != 0;
    const bool worldCandidate = (header.flags & 2u) != 0;
    const size_t rowBytes = static_cast<size_t>(header.width) * 4u;
    const size_t eyeBytes = rowBytes * static_cast<size_t>(header.height);
    const size_t fileBytes = bytes.size();
    const bool sane =
        std::memcmp(header.magic, expectedMagic, sizeof(expectedMagic)) == 0
        && header.version == 1
        && header.headerBytes >= sizeof(FnvxrSceneCacheHeader)
        && header.width == static_cast<std::uint32_t>(renderer.gameTextureWidth)
        && header.height == static_cast<std::uint32_t>(renderer.gameTextureHeight)
        && header.rowPitch == rowBytes
        && header.eyeCount == 2
        && header.leftBytes == eyeBytes
        && header.rightBytes == eyeBytes
        && header.leftOffset >= header.headerBytes
        && header.rightOffset >= header.headerBytes
        && header.leftOffset + header.leftBytes <= fileBytes
        && header.rightOffset + header.rightBytes <= fileBytes
        && separated
        && worldCandidate;
    if (!sane)
    {
        std::wcerr << L"sceneCache raw loaded=0 path=" << artifactPath
                   << L" version=" << header.version
                   << L" width=" << header.width
                   << L" height=" << header.height
                   << L" rowPitch=" << header.rowPitch
                   << L" separated=" << static_cast<int>(separated)
                   << L" worldCandidate=" << static_cast<int>(worldCandidate)
                   << L" reason=validation_failed\n";
        return false;
    }

    ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(&context);
    const std::uint8_t* left = bytes.data() + header.leftOffset;
    const std::uint8_t* right = bytes.data() + header.rightOffset;
    context->UpdateSubresource(
        renderer.stereoGameTextures[0].Get(),
        0,
        nullptr,
        left,
        static_cast<UINT>(header.rowPitch),
        0);
    context->UpdateSubresource(
        renderer.stereoGameTextures[1].Get(),
        0,
        nullptr,
        right,
        static_cast<UINT>(header.rowPitch),
        0);

    renderer.hasCachedSceneFrame = true;
    renderer.hasStereoGameFrame = true;
    renderer.stereoGameFrameSeparated = true;
    renderer.stereoGameFrameWorldCandidate = true;
    renderer.stereoGameFrameSequence = static_cast<LONG>(header.sequence);
    renderer.stereoGamePoseSequence = static_cast<LONG>(header.sequence);
    renderer.stereoGameRenderedDisplayTime = 0;
    renderer.stereoStableFrameCount =
        static_cast<uint64_t>(std::max(1, envInt("FNVXR_STEREO_STABLE_HANDOFF_FRAMES", 6)));
    renderer.gameTextureAspect =
        static_cast<float>(header.width) / static_cast<float>(header.height);
    renderer.stereoFrameMisses = 0;
    ++renderer.lastStereoFrameUpdated;
    std::wcerr << L"sceneCache raw loaded=1 path=" << artifactPath
               << L" width=" << header.width
               << L" height=" << header.height
               << L" sequence=" << header.sequence << L"\n";
    return true;
}

void sendHostMouseClick(HWND hwnd, POINT clientPoint)
{
    const LPARAM lparam = MAKELPARAM(clientPoint.x, clientPoint.y);
    PostMessageW(hwnd, WM_MOUSEMOVE, 0, lparam);
    PostMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lparam);
    PostMessageW(hwnd, WM_LBUTTONUP, 0, lparam);

    if (envEnabled("FNVXR_HOST_SENDINPUT_CLICK", false) && GetForegroundWindow() == hwnd)
    {
        INPUT inputs[2] {};
        inputs[0].type = INPUT_MOUSE;
        inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        inputs[1].type = INPUT_MOUSE;
        inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(2, inputs, sizeof(INPUT));
    }
}

bool sendAbsoluteCursorMove(const POINT& screenTarget)
{
    const int virtualX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int virtualY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int virtualWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int virtualHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (virtualWidth <= 1 || virtualHeight <= 1)
        return false;

    const LONG absoluteX = static_cast<LONG>(
        std::lround((static_cast<double>(screenTarget.x - virtualX) * 65535.0) / static_cast<double>(virtualWidth - 1)));
    const LONG absoluteY = static_cast<LONG>(
        std::lround((static_cast<double>(screenTarget.y - virtualY) * 65535.0) / static_cast<double>(virtualHeight - 1)));

    INPUT input {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    input.mi.dx = absoluteX;
    input.mi.dy = absoluteY;
    return SendInput(1, &input, sizeof(INPUT)) == 1;
}

void updateHostCursorPointer(const MenuPointer& pointer, bool click)
{
    const bool trackPointer = envEnabled("FNVXR_HOST_CURSOR_TRACK_POINTER", false);
    const bool clickFallback = envEnabled("FNVXR_HOST_CURSOR_CLICK_FALLBACK", false);
    if (!envEnabled("FNVXR_HOST_CURSOR_CLICK_ENABLED", false))
        click = false;
    if (!trackPointer && !(clickFallback && click))
        return;

    HWND hwnd = findFalloutWindow();
    if (!hwnd)
        return;

    RECT client {};
    if (!GetClientRect(hwnd, &client))
        return;

    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0)
        return;
    const int inputWidth = envInt("FNVXR_UI_INPUT_WIDTH", width);
    const int inputHeight = envInt("FNVXR_UI_INPUT_HEIGHT", height);
    const int mapWidth = inputWidth > 1 ? inputWidth : width;
    const int mapHeight = inputHeight > 1 ? inputHeight : height;

    static bool hasLastClientPoint = false;
    static POINT lastClientPoint {};
    static bool hasSmoothedClientPoint = false;
    static float smoothedClientX = 0.0f;
    static float smoothedClientY = 0.0f;
    static uint64_t cursorUpdateCount = 0;
    if (envEnabled("FNVXR_HOST_CURSOR_FOCUS", false))
        SetForegroundWindow(hwnd);

    if (!pointer.active && !click)
        return;

    POINT target {
        hasLastClientPoint ? lastClientPoint.x : mapWidth / 2,
        hasLastClientPoint ? lastClientPoint.y : mapHeight / 2
    };

    if (pointer.active)
    {
        const float rawX = clamp01(pointer.x) * static_cast<float>(mapWidth - 1);
        const float rawY = clamp01(pointer.y) * static_cast<float>(mapHeight - 1);
        const float follow = clamp01(envFloat("FNVXR_HOST_CURSOR_FOLLOW", 0.85f));
        if (!hasSmoothedClientPoint || follow >= 0.999f)
        {
            smoothedClientX = rawX;
            smoothedClientY = rawY;
            hasSmoothedClientPoint = true;
        }
        else
        {
            smoothedClientX += (rawX - smoothedClientX) * follow;
            smoothedClientY += (rawY - smoothedClientY) * follow;
        }
        target.x = static_cast<LONG>(std::lround(smoothedClientX));
        target.y = static_cast<LONG>(std::lround(smoothedClientY));
        lastClientPoint = target;
        hasLastClientPoint = true;
    }

    if (trackPointer || click)
        PostMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(target.x, target.y));

    POINT screenTarget = target;
    if (!ClientToScreen(hwnd, &screenTarget))
        return;

    POINT beforeCursor {};
    GetCursorPos(&beforeCursor);

    bool setCursorOk = false;
    bool absoluteMoveOk = false;
    if ((trackPointer || click) && envEnabled("FNVXR_HOST_CURSOR_SET_POS", false))
    {
        if (envEnabled("FNVXR_HOST_CURSOR_CLEAR_CLIP", true))
            ClipCursor(nullptr);
        setCursorOk = SetCursorPos(screenTarget.x, screenTarget.y) != FALSE;
    }
    if ((trackPointer || click) && envEnabled("FNVXR_HOST_CURSOR_ABSOLUTE_MOVE", false))
        absoluteMoveOk = sendAbsoluteCursorMove(screenTarget);

    POINT afterCursor {};
    GetCursorPos(&afterCursor);

    DWORD foregroundPid = 0;
    HWND foreground = GetForegroundWindow();
    if (foreground)
        GetWindowThreadProcessId(foreground, &foregroundPid);

    ++cursorUpdateCount;
    const int logEvery = envInt("FNVXR_HOST_CURSOR_LOG_EVERY", 15);
    if (click || (envEnabled("FNVXR_HOST_CURSOR_LOG", true) && logEvery > 0 && cursorUpdateCount % static_cast<uint64_t>(logEvery) == 0))
    {
        std::cout << "hostCursor pointer=" << static_cast<int>(pointer.active)
                  << " click=" << static_cast<int>(click)
                  << " norm=(" << pointer.x << "," << pointer.y << ")"
                  << " client=(" << target.x << "," << target.y << ")"
                  << " screen=(" << screenTarget.x << "," << screenTarget.y << ")"
                  << " before=(" << beforeCursor.x << "," << beforeCursor.y << ")"
                  << " after=(" << afterCursor.x << "," << afterCursor.y << ")"
                  << " delta=(" << (afterCursor.x - screenTarget.x) << "," << (afterCursor.y - screenTarget.y) << ")"
                  << " set=" << static_cast<int>(setCursorOk)
                  << " abs=" << static_cast<int>(absoluteMoveOk)
                  << " foregroundPid=" << foregroundPid
                  << " size=" << width << "x" << height
                  << " inputSize=" << mapWidth << "x" << mapHeight
                  << "\n" << std::flush;
    }

    if (click)
        sendHostMouseClick(hwnd, target);
}

XMMATRIX projectionFromFov(const XrFovf& fov, float nearZ, float farZ)
{
    const float left = std::tan(fov.angleLeft) * nearZ;
    const float right = std::tan(fov.angleRight) * nearZ;
    const float down = std::tan(fov.angleDown) * nearZ;
    const float up = std::tan(fov.angleUp) * nearZ;
    return XMMatrixPerspectiveOffCenterRH(left, right, down, up, nearZ, farZ);
}

XMMATRIX viewFromPose(const XrPosef& pose)
{
    const XMVECTOR orientation = XMVectorSet(pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w);
    const XMVECTOR inverseOrientation = XMQuaternionConjugate(orientation);
    return XMMatrixTranslation(-pose.position.x, -pose.position.y, -pose.position.z)
        * XMMatrixRotationQuaternion(inverseOrientation);
}

XMMATRIX modelFromPose(const XrPosef& pose, const XMFLOAT3& scale)
{
    const XMVECTOR orientation = XMVectorSet(pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w);
    return XMMatrixScaling(scale.x, scale.y, scale.z)
        * XMMatrixRotationQuaternion(orientation)
        * XMMatrixTranslation(pose.position.x, pose.position.y, pose.position.z);
}

XMMATRIX modelFromParentLocal(
    const XrPosef& parent,
    const XMFLOAT3& localOffset,
    const XMFLOAT3& localEulerRadians,
    const XMFLOAT3& scale)
{
    const XMVECTOR orientation = XMVectorSet(parent.orientation.x, parent.orientation.y, parent.orientation.z, parent.orientation.w);
    const XMVECTOR offset = XMVector3Rotate(XMLoadFloat3(&localOffset), orientation);

    XMFLOAT3 rotatedOffset {};
    XMStoreFloat3(&rotatedOffset, offset);

    return XMMatrixScaling(scale.x, scale.y, scale.z)
        * XMMatrixRotationRollPitchYaw(localEulerRadians.x, localEulerRadians.y, localEulerRadians.z)
        * XMMatrixRotationQuaternion(orientation)
        * XMMatrixTranslation(
            parent.position.x + rotatedOffset.x,
            parent.position.y + rotatedOffset.y,
            parent.position.z + rotatedOffset.z);
}

XrPosef poseFromBasis(XMVECTOR position, XMVECTOR xAxis, XMVECTOR yAxis, XMVECTOR zAxis)
{
    xAxis = safeNormalize3(xAxis, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
    yAxis = safeNormalize3(yAxis, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    zAxis = safeNormalize3(zAxis, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));

    const XMMATRIX basis {
        XMVectorGetX(xAxis), XMVectorGetY(xAxis), XMVectorGetZ(xAxis), 0.0f,
        XMVectorGetX(yAxis), XMVectorGetY(yAxis), XMVectorGetZ(yAxis), 0.0f,
        XMVectorGetX(zAxis), XMVectorGetY(zAxis), XMVectorGetZ(zAxis), 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    XrPosef pose {};
    pose.position = xrVector(position);
    pose.orientation = xrQuaternion(XMQuaternionRotationMatrix(basis));
    return pose;
}

XrPosef poseAlongSegment(XMVECTOR start, XMVECTOR end, XMVECTOR preferredUp)
{
    const XMVECTOR zAxis = safeNormalize3(end - start, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
    XMVECTOR yAxis = preferredUp - XMVectorScale(zAxis, XMVectorGetX(XMVector3Dot(preferredUp, zAxis)));
    yAxis = safeNormalize3(yAxis, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    const XMVECTOR xAxis = safeNormalize3(XMVector3Cross(yAxis, zAxis), XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
    yAxis = safeNormalize3(XMVector3Cross(zAxis, xAxis), yAxis);
    return poseFromBasis((start + end) * 0.5f, xAxis, yAxis, zAxis);
}

SolvedArm solveArm(const XrPosef& hmdPose, const XrPosef& wristPose, bool left)
{
    const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const XMVECTOR hmdOrientation = quat(hmdPose.orientation);
    XMVECTOR hmdForward = XMVector3Rotate(XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), hmdOrientation);
    hmdForward = safeNormalize3(XMVectorSetY(hmdForward, 0.0f), XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f));
    const XMVECTOR hmdRight = safeNormalize3(XMVector3Cross(hmdForward, worldUp), XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));

    const float shoulderWidth = envFloat("FNVXR_BODY_SHOULDER_WIDTH", 0.38f);
    const float shoulderDrop = envFloat("FNVXR_BODY_SHOULDER_DROP", 0.24f);
    const float shoulderBack = envFloat("FNVXR_BODY_SHOULDER_BACK", 0.08f);
    const float upperLength = envFloat("FNVXR_ARM_UPPER_LENGTH", 0.31f);
    const float lowerLength = envFloat("FNVXR_ARM_LOWER_LENGTH", 0.27f);

    const XMVECTOR hmdPosition = vec3(hmdPose.position);
    const XMVECTOR shoulderCenter = hmdPosition - worldUp * shoulderDrop - hmdForward * shoulderBack;
    const XMVECTOR side = left ? -hmdRight : hmdRight;
    const XMVECTOR shoulder = shoulderCenter + side * (shoulderWidth * 0.5f);
    const XMVECTOR wrist = vec3(wristPose.position);
    const XMVECTOR shoulderToWrist = wrist - shoulder;
    float distance = XMVectorGetX(XMVector3Length(shoulderToWrist));
    const float maxReach = upperLength + lowerLength - 0.002f;
    const float minReach = std::fabs(upperLength - lowerLength) + 0.002f;
    const XMVECTOR dir = safeNormalize3(shoulderToWrist, hmdForward);
    distance = std::max(minReach, std::min(distance, maxReach));

    const float along = (upperLength * upperLength - lowerLength * lowerLength + distance * distance) / (2.0f * distance);
    const float bend = std::sqrt(std::max(0.0f, upperLength * upperLength - along * along));
    XMVECTOR elbowHint = -worldUp * envFloat("FNVXR_ARM_ELBOW_DOWN", 1.0f)
        + side * envFloat("FNVXR_ARM_ELBOW_OUT", 0.35f)
        - hmdForward * envFloat("FNVXR_ARM_ELBOW_BACK", 0.20f);
    elbowHint -= dir * XMVectorGetX(XMVector3Dot(elbowHint, dir));
    elbowHint = safeNormalize3(elbowHint, -worldUp);
    const XMVECTOR elbow = shoulder + dir * along + elbowHint * bend;

    const XMVECTOR wristOrientation = quat(wristPose.orientation);
    XMVECTOR handUp = XMVector3Rotate(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), wristOrientation);
    handUp = safeNormalize3(handUp, worldUp);

    SolvedArm arm {};
    arm.shoulder.position = xrVector(shoulder);
    arm.shoulder.orientation = xrQuaternion(hmdOrientation);
    arm.elbow.position = xrVector(elbow);
    arm.elbow.orientation = xrQuaternion(hmdOrientation);
    arm.wrist = wristPose;
    arm.forearm = poseAlongSegment(elbow, wrist, handUp);
    arm.forearmLength = XMVectorGetX(XMVector3Length(wrist - elbow));
    return arm;
}

BodyRig solveBodyRig(const XrPosef& hmdPose, const XrPosef& leftPose, const XrPosef& rightPose)
{
    BodyRig rig {};
    rig.left = solveArm(hmdPose, leftPose, true);
    rig.right = solveArm(hmdPose, rightPose, false);
    return rig;
}

GamePlane gamePlaneFromHmd(const XrPosef& hmdPose)
{
    GamePlane plane {};
    plane.width = envFloat("FNVXR_GAME_PLANE_WIDTH", 2.4f);
    plane.height = envFloat("FNVXR_GAME_PLANE_HEIGHT", 1.35f);

    XMVECTOR planeOrientation =
        XMVectorSet(hmdPose.orientation.x, hmdPose.orientation.y, hmdPose.orientation.z, hmdPose.orientation.w);
    if (envFloat("FNVXR_GAME_PLANE_REMOVE_ROLL", 1.0f) != 0.0f
        || envFloat("FNVXR_GAME_PLANE_REMOVE_PITCH", 1.0f) != 0.0f)
    {
        XMVECTOR hmdForward =
            XMVector3Normalize(XMVector3Rotate(XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), planeOrientation));
        const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        if (envFloat("FNVXR_GAME_PLANE_REMOVE_PITCH", 1.0f) != 0.0f)
        {
            // A fallback/menu surface is not the native 3D camera.  Recenter
            // it at eye height using headset yaw only so looking down while
            // pressing the chord cannot throw the surface underneath the
            // user and masquerade as a broken 3D-camera recenter.
            hmdForward = safeNormalize3(
                XMVectorSetY(hmdForward, 0.0f),
                XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f));
        }
        XMMATRIX viewNoRoll = XMMatrixLookToRH(XMVectorZero(), hmdForward, worldUp);
        XMMATRIX planeWorld = XMMatrixInverse(nullptr, viewNoRoll);
        planeOrientation = XMQuaternionRotationMatrix(planeWorld);
    }

    XMFLOAT4 orientation {};
    XMStoreFloat4(&orientation, planeOrientation);
    plane.pose.orientation = { orientation.x, orientation.y, orientation.z, orientation.w };

    const XMFLOAT3 localOffset {
        envFloat("FNVXR_GAME_PLANE_OFFSET_X", 0.0f),
        envFloat("FNVXR_GAME_PLANE_OFFSET_Y", 0.0f),
        envFloat("FNVXR_GAME_PLANE_OFFSET_Z", -2.0f)
    };
    const XMVECTOR offset = XMVector3Rotate(XMLoadFloat3(&localOffset), planeOrientation);

    XMFLOAT3 rotatedOffset {};
    XMStoreFloat3(&rotatedOffset, offset);
    plane.pose.position = {
        hmdPose.position.x + rotatedOffset.x,
        hmdPose.position.y + rotatedOffset.y,
        hmdPose.position.z + rotatedOffset.z
    };
    return plane;
}

XrPosef pauseSceneAnchorFromHmd(const XrPosef& hmdPose)
{
    XrPosef anchor {};
    XMVECTOR orientation =
        XMVectorSet(hmdPose.orientation.x, hmdPose.orientation.y, hmdPose.orientation.z, hmdPose.orientation.w);
    XMVECTOR hmdForward =
        XMVector3Normalize(XMVector3Rotate(XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), orientation));
    hmdForward = safeNormalize3(
        XMVectorSetY(hmdForward, 0.0f),
        XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f));
    const XMMATRIX viewNoRoll = XMMatrixLookToRH(XMVectorZero(), hmdForward, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    const XMMATRIX anchorWorld = XMMatrixInverse(nullptr, viewNoRoll);
    const XMVECTOR anchorOrientation = XMQuaternionRotationMatrix(anchorWorld);

    XMFLOAT4 storedOrientation {};
    XMStoreFloat4(&storedOrientation, anchorOrientation);
    anchor.orientation = {
        storedOrientation.x,
        storedOrientation.y,
        storedOrientation.z,
        storedOrientation.w
    };
    anchor.position = hmdPose.position;
    return anchor;
}

XrPosef smoothPose(const XrPosef& current, const XrPosef& target, float alpha)
{
    XMVECTOR currentPosition = XMVectorSet(current.position.x, current.position.y, current.position.z, 0.0f);
    XMVECTOR targetPosition = XMVectorSet(target.position.x, target.position.y, target.position.z, 0.0f);
    XMVECTOR blendedPosition = XMVectorLerp(currentPosition, targetPosition, alpha);

    XMVECTOR currentOrientation = XMVectorSet(current.orientation.x, current.orientation.y, current.orientation.z, current.orientation.w);
    XMVECTOR targetOrientation = XMVectorSet(target.orientation.x, target.orientation.y, target.orientation.z, target.orientation.w);
    XMVECTOR blendedOrientation = XMQuaternionNormalize(XMQuaternionSlerp(currentOrientation, targetOrientation, alpha));

    XrPosef out {};
    XMFLOAT3 position {};
    XMFLOAT4 orientation {};
    XMStoreFloat3(&position, blendedPosition);
    XMStoreFloat4(&orientation, blendedOrientation);
    out.position = { position.x, position.y, position.z };
    out.orientation = { orientation.x, orientation.y, orientation.z, orientation.w };
    return out;
}

void drawCube(ID3D11DeviceContext* context, Renderer& renderer, const XMMATRIX& viewProjection, const XrPosef& pose, const XMFLOAT3& scale, const XMFLOAT4& color)
{
    Constants constants {};
    XMStoreFloat4x4(&constants.mvp, modelFromPose(pose, scale) * viewProjection);
    constants.color = color;
    context->UpdateSubresource(renderer.constantBuffer.Get(), 0, nullptr, &constants, 0, 0);
    context->Draw(36, 0);
}

void bindSolidPipeline(ID3D11DeviceContext* context, Renderer& renderer)
{
    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    context->IASetInputLayout(renderer.inputLayout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetVertexBuffers(0, 1, renderer.vertexBuffer.GetAddressOf(), &stride, &offset);
    context->VSSetShader(renderer.vertexShader.Get(), nullptr, 0);
    context->PSSetShader(renderer.pixelShader.Get(), nullptr, 0);
    context->VSSetConstantBuffers(0, 1, renderer.constantBuffer.GetAddressOf());
    context->PSSetConstantBuffers(0, 1, renderer.constantBuffer.GetAddressOf());
}

XrPosef poseAt(float x, float y, float z)
{
    XrPosef pose {};
    pose.orientation = { 0.0f, 0.0f, 0.0f, 1.0f };
    pose.position = { x, y, z };
    return pose;
}

void drawLocalCube(
    ID3D11DeviceContext* context,
    Renderer& renderer,
    const XMMATRIX& viewProjection,
    const XrPosef& parent,
    const XMFLOAT3& localOffset,
    const XMFLOAT3& localEulerRadians,
    const XMFLOAT3& scale,
    const XMFLOAT4& color)
{
    Constants constants {};
    XMStoreFloat4x4(&constants.mvp, modelFromParentLocal(parent, localOffset, localEulerRadians, scale) * viewProjection);
    constants.color = color;
    context->UpdateSubresource(renderer.constantBuffer.Get(), 0, nullptr, &constants, 0, 0);
    context->Draw(36, 0);
}

void drawTexturedPlane(
    ID3D11DeviceContext* context,
    Renderer& renderer,
    const XMMATRIX& viewProjection,
    const XMMATRIX& model,
    const XMFLOAT4& color,
    ID3D11ShaderResourceView* gameTextureView)
{
    if (!gameTextureView)
        return;

    Constants constants {};
    XMStoreFloat4x4(&constants.mvp, model * viewProjection);
    constants.color = color;
    context->UpdateSubresource(renderer.constantBuffer.Get(), 0, nullptr, &constants, 0, 0);

    const UINT stride = sizeof(TexturedVertex);
    const UINT offset = 0;
    context->IASetInputLayout(renderer.texturedInputLayout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetVertexBuffers(0, 1, renderer.texturedVertexBuffer.GetAddressOf(), &stride, &offset);
    context->VSSetShader(renderer.texturedVertexShader.Get(), nullptr, 0);
    context->PSSetShader(renderer.texturedPixelShader.Get(), nullptr, 0);
    context->VSSetConstantBuffers(0, 1, renderer.constantBuffer.GetAddressOf());
    context->PSSetShaderResources(0, 1, &gameTextureView);
    context->PSSetSamplers(0, 1, renderer.gameSampler.GetAddressOf());
    context->Draw(6, 0);

    ID3D11ShaderResourceView* emptyView = nullptr;
    context->PSSetShaderResources(0, 1, &emptyView);
}

void drawTexturedVertexBuffer(
    ID3D11DeviceContext* context,
    Renderer& renderer,
    const XMMATRIX& viewProjection,
    const XMMATRIX& model,
    const XMFLOAT4& color,
    ID3D11ShaderResourceView* gameTextureView,
    ID3D11Buffer* vertexBuffer,
    UINT vertexCount,
    ID3D11PixelShader* pixelShader = nullptr,
    ID3D11BlendState* blendState = nullptr)
{
    if (!gameTextureView || !vertexBuffer || vertexCount == 0)
        return;

    Constants constants {};
    XMStoreFloat4x4(&constants.mvp, model * viewProjection);
    constants.color = color;
    context->UpdateSubresource(renderer.constantBuffer.Get(), 0, nullptr, &constants, 0, 0);

    const UINT stride = sizeof(TexturedVertex);
    const UINT offset = 0;
    context->IASetInputLayout(renderer.texturedInputLayout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    context->VSSetShader(renderer.texturedVertexShader.Get(), nullptr, 0);
    context->PSSetShader(pixelShader ? pixelShader : renderer.texturedPixelShader.Get(), nullptr, 0);
    context->VSSetConstantBuffers(0, 1, renderer.constantBuffer.GetAddressOf());
    context->PSSetShaderResources(0, 1, &gameTextureView);
    context->PSSetSamplers(0, 1, renderer.gameSampler.GetAddressOf());

    ComPtr<ID3D11BlendState> previousBlendState;
    FLOAT previousBlendFactor[4] {};
    UINT previousSampleMask = 0xffffffff;
    if (blendState)
    {
        context->OMGetBlendState(previousBlendState.GetAddressOf(), previousBlendFactor, &previousSampleMask);
        const FLOAT blendFactor[4] { 0.0f, 0.0f, 0.0f, 0.0f };
        context->OMSetBlendState(blendState, blendFactor, 0xffffffff);
    }

    context->Draw(vertexCount, 0);

    if (blendState)
        context->OMSetBlendState(previousBlendState.Get(), previousBlendFactor, previousSampleMask);

    ID3D11ShaderResourceView* emptyView = nullptr;
    context->PSSetShaderResources(0, 1, &emptyView);
}

void drawGamePlane(
    ID3D11DeviceContext* context,
    Renderer& renderer,
    const XMMATRIX& viewProjection,
    const GamePlane& gamePlane,
    ID3D11ShaderResourceView* gameTextureView,
    bool allowSourceSurround)
{
    const GamePlaneMode mode = gamePlaneMode();
    const GamePlaneModeSettings modeSettings = gamePlaneModeSettings(mode);
    bool drewSourceSurround = false;
    if (allowSourceSurround
        && modeSettings.sourceSurround
        && renderer.surroundTexturedVertexBuffer
        && renderer.surroundTexturedVertexCount > 0)
    {
        const bool compositeCenter = modeSettings.compositeCenter;
        const bool useWideSource = modeSettings.useWideSource;
        const bool requireWideSource = modeSettings.requireWideSource;
        ID3D11ShaderResourceView* surroundTextureView = gameTextureView;
        if (useWideSource && renderer.hasWorldTextureFrame && renderer.worldTextureView)
            surroundTextureView = renderer.worldTextureView.Get();
        else if (useWideSource && requireWideSource)
            surroundTextureView = nullptr;
        const float brightness = std::clamp(modeSettings.surroundBrightness, 0.05f, 1.0f);
        if (surroundTextureView)
        {
            drawTexturedVertexBuffer(
                context,
                renderer,
                viewProjection,
                modelFromPose(gamePlane.pose, { gamePlane.width, gamePlane.height, 1.0f }),
                { brightness, brightness, brightness, 1.0f },
                surroundTextureView,
                renderer.surroundTexturedVertexBuffer.Get(),
                renderer.surroundTexturedVertexCount);
            drewSourceSurround = true;
            if (modeSettings.edgeSmear
                && renderer.edgeSmearPixelShader
                && renderer.alphaBlendState
                && renderer.edgeSmearTexturedVertexBuffer
                && renderer.edgeSmearTexturedVertexCount > 0)
            {
                drawTexturedVertexBuffer(
                    context,
                    renderer,
                    viewProjection,
                    modelFromPose(gamePlane.pose, { gamePlane.width, gamePlane.height, 1.0f }),
                    { brightness, brightness, brightness, modeSettings.edgeSmearOpacity },
                    surroundTextureView,
                    renderer.edgeSmearTexturedVertexBuffer.Get(),
                    renderer.edgeSmearTexturedVertexCount,
                    renderer.edgeSmearPixelShader.Get(),
                    renderer.alphaBlendState.Get());
            }
            if (!compositeCenter)
                return;
        }
    }

    if (drewSourceSurround
        && modeSettings.compositeCenter
        && renderer.shieldCenterTexturedVertexBuffer
        && renderer.shieldCenterTexturedVertexCount > 0)
    {
        drawTexturedVertexBuffer(
            context,
            renderer,
            viewProjection,
            modelFromPose(gamePlane.pose, { gamePlane.width, gamePlane.height, 1.0f }),
            { 1.0f, 1.0f, 1.0f, 1.0f },
                gameTextureView,
                renderer.shieldCenterTexturedVertexBuffer.Get(),
                renderer.shieldCenterTexturedVertexCount,
                modeSettings.hudOverlay ? renderer.centerNoHudPixelShader.Get() : nullptr);
        if (modeSettings.hudOverlay
            && renderer.hudOverlayPixelShader
            && renderer.alphaBlendState
            && renderer.curvedTexturedVertexBuffer
            && renderer.curvedTexturedVertexCount > 0)
        {
            drawTexturedVertexBuffer(
                context,
                renderer,
                viewProjection,
                modelFromPose(gamePlane.pose, { gamePlane.width, gamePlane.height, 1.0f }),
                { 1.0f, 1.0f, 1.0f, 1.0f },
                gameTextureView,
                renderer.curvedTexturedVertexBuffer.Get(),
                renderer.curvedTexturedVertexCount,
                renderer.hudOverlayPixelShader.Get(),
                renderer.alphaBlendState.Get());
        }
        return;
    }

    if (envEnabled("FNVXR_GAME_PLANE_CURVE_ENABLE", true)
        && renderer.curvedTexturedVertexBuffer
        && renderer.curvedTexturedVertexCount > 0)
    {
        drawTexturedVertexBuffer(
            context,
            renderer,
            viewProjection,
            modelFromPose(gamePlane.pose, { gamePlane.width, gamePlane.height, 1.0f }),
            { 1.0f, 1.0f, 1.0f, 1.0f },
            gameTextureView,
            renderer.curvedTexturedVertexBuffer.Get(),
            renderer.curvedTexturedVertexCount);
        return;
    }

    drawTexturedPlane(
        context,
        renderer,
        viewProjection,
        modelFromPose(gamePlane.pose, { gamePlane.width, gamePlane.height, 1.0f }),
        { 1.0f, 1.0f, 1.0f, 1.0f },
        gameTextureView);
}

void drawLocalTexturedPlane(
    ID3D11DeviceContext* context,
    Renderer& renderer,
    const XMMATRIX& viewProjection,
    const XrPosef& parent,
    const XMFLOAT3& localOffset,
    const XMFLOAT3& localEulerRadians,
    const XMFLOAT3& scale,
    const XMFLOAT4& color,
    ID3D11ShaderResourceView* gameTextureView)
{
    drawTexturedPlane(
        context,
        renderer,
        viewProjection,
        modelFromParentLocal(parent, localOffset, localEulerRadians, scale),
        color,
        gameTextureView);
}

void drawMenuShowroom(
    ID3D11DeviceContext* context,
    Renderer& renderer,
    const XMMATRIX& viewProjection,
    const GamePlane& gamePlane,
    ID3D11ShaderResourceView* gameTextureView)
{
    if (!envEnabled("FNVXR_MENU_SHOWROOM", false))
        return;

    const float depth = envFloat("FNVXR_MENU_SHOWROOM_DEPTH", 1.25f);
    const float sideYaw = degreesToRadians(envFloat("FNVXR_MENU_SHOWROOM_SIDE_YAW", 34.0f));
    drawLocalTexturedPlane(
        context,
        renderer,
        viewProjection,
        gamePlane.pose,
        { 0.0f, 0.12f, -depth },
        { 0.0f, 0.0f, 0.0f },
        { gamePlane.width * 1.65f, gamePlane.height * 1.65f, 1.0f },
        { 0.54f, 0.60f, 0.58f, 1.0f },
        gameTextureView);
    drawLocalTexturedPlane(
        context,
        renderer,
        viewProjection,
        gamePlane.pose,
        { -gamePlane.width * 0.83f, 0.02f, -depth * 0.58f },
        { 0.0f, -sideYaw, 0.0f },
        { gamePlane.width * 0.86f, gamePlane.height * 1.25f, 1.0f },
        { 0.42f, 0.52f, 0.50f, 1.0f },
        gameTextureView);
    drawLocalTexturedPlane(
        context,
        renderer,
        viewProjection,
        gamePlane.pose,
        { gamePlane.width * 0.83f, 0.02f, -depth * 0.58f },
        { 0.0f, sideYaw, 0.0f },
        { gamePlane.width * 0.86f, gamePlane.height * 1.25f, 1.0f },
        { 0.48f, 0.47f, 0.42f, 1.0f },
        gameTextureView);
    drawLocalTexturedPlane(
        context,
        renderer,
        viewProjection,
        gamePlane.pose,
        { 0.0f, -gamePlane.height * 0.82f, -depth * 0.45f },
        { degreesToRadians(72.0f), 0.0f, 0.0f },
        { gamePlane.width * 1.60f, gamePlane.height * 1.05f, 1.0f },
        { 0.36f, 0.42f, 0.38f, 1.0f },
        gameTextureView);
}

void drawGameFullscreen(
    ID3D11DeviceContext* context,
    Renderer& renderer,
    ID3D11ShaderResourceView* gameTextureView)
{
    if (!gameTextureView)
        return;

    Constants constants {};
    XMStoreFloat4x4(&constants.mvp, XMMatrixScaling(2.0f, 2.0f, 1.0f));
    constants.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    context->UpdateSubresource(renderer.constantBuffer.Get(), 0, nullptr, &constants, 0, 0);

    const UINT stride = sizeof(TexturedVertex);
    const UINT offset = 0;
    context->IASetInputLayout(renderer.texturedInputLayout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetVertexBuffers(0, 1, renderer.texturedVertexBuffer.GetAddressOf(), &stride, &offset);
    context->VSSetShader(renderer.texturedVertexShader.Get(), nullptr, 0);
    context->PSSetShader(renderer.texturedPixelShader.Get(), nullptr, 0);
    context->VSSetConstantBuffers(0, 1, renderer.constantBuffer.GetAddressOf());
    context->PSSetShaderResources(0, 1, &gameTextureView);
    context->PSSetSamplers(0, 1, renderer.gameSampler.GetAddressOf());
    context->Draw(6, 0);

    ID3D11ShaderResourceView* emptyView = nullptr;
    context->PSSetShaderResources(0, 1, &emptyView);
}

MenuPointer menuPointerFromAimPose(const XrPosef& aimPose, const GamePlane& gamePlane)
{
    const XMVECTOR origin = XMVectorSet(aimPose.position.x, aimPose.position.y, aimPose.position.z, 0.0f);
    const XMVECTOR aimOrientation =
        XMVectorSet(aimPose.orientation.x, aimPose.orientation.y, aimPose.orientation.z, aimPose.orientation.w);
    const XMVECTOR direction = XMVector3Rotate(XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), aimOrientation);
    const XMVECTOR planePosition =
        XMVectorSet(gamePlane.pose.position.x, gamePlane.pose.position.y, gamePlane.pose.position.z, 0.0f);
    const XMVECTOR planeOrientation = XMVectorSet(
        gamePlane.pose.orientation.x,
        gamePlane.pose.orientation.y,
        gamePlane.pose.orientation.z,
        gamePlane.pose.orientation.w);
    const XMVECTOR inversePlaneOrientation = XMQuaternionConjugate(planeOrientation);
    const XMVECTOR localOrigin = XMVector3Rotate(origin - planePosition, inversePlaneOrientation);
    const XMVECTOR localDirection = XMVector3Rotate(direction, inversePlaneOrientation);

    XMFLOAT3 originFloat {};
    XMFLOAT3 directionFloat {};
    XMStoreFloat3(&originFloat, localOrigin);
    XMStoreFloat3(&directionFloat, localDirection);
    if (std::fabs(directionFloat.z) < 0.0001f)
        return {};

    const float t = -originFloat.z / directionFloat.z;
    if (t <= 0.0f)
        return {};

    const float hitX = originFloat.x + directionFloat.x * t;
    const float hitY = originFloat.y + directionFloat.y * t;
    const float pointerX = (hitX + gamePlane.width * 0.5f) / gamePlane.width;
    const float pointerY = (gamePlane.height * 0.5f - hitY) / gamePlane.height;
    const float margin = envFloat("FNVXR_MENU_POINTER_MARGIN", 0.05f);
    if (pointerX < -margin || pointerX > 1.0f + margin || pointerY < -margin || pointerY > 1.0f + margin)
        return {};

    return { true, clamp01(pointerX), clamp01(pointerY) };
}

HeadspaceLook sampleRightHandspaceLook(
    HandspaceLookTracker& tracker,
    const XrPosef& aimPose,
    const GamePlane& gamePlane,
    bool trackingActive)
{
    HeadspaceLook look {};
    if (!trackingActive || !envEnabled("FNVXR_HANDSPACE_LOOK_ENABLE", true))
    {
        tracker.hasLast = false;
        return look;
    }

    const MenuPointer pointer = menuPointerFromAimPose(aimPose, gamePlane);
    if (!pointer.active)
    {
        tracker.hasLast = false;
        return look;
    }

    if (!tracker.hasLast)
    {
        tracker.hasLast = true;
        tracker.x = pointer.x;
        tracker.y = pointer.y;
        look.active = true;
        return look;
    }

    const float deadzone = std::clamp(envFloat("FNVXR_HANDSPACE_LOOK_DEADZONE", 0.0005f), 0.0f, 0.10f);
    const float maxDelta = std::clamp(envFloat("FNVXR_HANDSPACE_LOOK_MAX_DELTA", 0.025f), 0.001f, 0.25f);
    float xDelta = pointer.x - tracker.x;
    float yDelta = pointer.y - tracker.y;
    tracker.x = pointer.x;
    tracker.y = pointer.y;

    if (std::fabs(xDelta) < deadzone)
        xDelta = 0.0f;
    if (std::fabs(yDelta) < deadzone)
        yDelta = 0.0f;
    xDelta = std::clamp(xDelta, -maxDelta, maxDelta);
    yDelta = std::clamp(yDelta, -maxDelta, maxDelta);

    const float horizontalRadians =
        degreesToRadians(std::clamp(envFloat("FNVXR_HANDSPACE_LOOK_HORIZONTAL_DEGREES", 75.0f), 1.0f, 170.0f));
    const float verticalRadians =
        degreesToRadians(std::clamp(envFloat("FNVXR_HANDSPACE_LOOK_VERTICAL_DEGREES", 50.0f), 1.0f, 170.0f));
    const float yawDelta = xDelta * horizontalRadians;
    const float pitchDelta = -yDelta * verticalRadians;

    look.active = true;
    look.yawRadians = yawDelta;
    look.pitchRadians = pitchDelta;
    look.yawMicroradians = static_cast<std::int32_t>(std::lround(yawDelta * 1000000.0f));
    look.pitchMicroradians = static_cast<std::int32_t>(std::lround(pitchDelta * 1000000.0f));
    return look;
}

void drawMenuPointer(
    ID3D11DeviceContext* context,
    Renderer& renderer,
    const XMMATRIX& viewProjection,
    const MenuPointer& pointer,
    const GamePlane& gamePlane)
{
    if (!pointer.active)
        return;

    drawLocalCube(
        context,
        renderer,
        viewProjection,
        gamePlane.pose,
        {
            pointer.x * gamePlane.width - gamePlane.width * 0.5f,
            gamePlane.height * 0.5f - pointer.y * gamePlane.height,
            0.015f
        },
        { 0.0f, 0.0f, 0.0f },
        { 0.025f, 0.025f, 0.004f },
        { 1.0f, 0.95f, 0.05f, 1.0f });
}

void drawPoseAxes(
    ID3D11DeviceContext* context,
    Renderer& renderer,
    const XMMATRIX& viewProjection,
    const XrPosef& pose)
{
    drawLocalCube(
        context,
        renderer,
        viewProjection,
        pose,
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.022f, 0.022f, 0.022f },
        { 1.0f, 1.0f, 1.0f, 1.0f });
    drawLocalCube(
        context,
        renderer,
        viewProjection,
        pose,
        { 0.05f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.10f, 0.006f, 0.006f },
        { 1.0f, 0.05f, 0.05f, 1.0f });
    drawLocalCube(
        context,
        renderer,
        viewProjection,
        pose,
        { 0.0f, 0.05f, 0.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.006f, 0.10f, 0.006f },
        { 0.05f, 1.0f, 0.05f, 1.0f });
    drawLocalCube(
        context,
        renderer,
        viewProjection,
        pose,
        { 0.0f, 0.0f, 0.05f },
        { 0.0f, 0.0f, 0.0f },
        { 0.006f, 0.006f, 0.10f },
        { 0.05f, 0.3f, 1.0f, 1.0f });
}

void drawSegment(
    ID3D11DeviceContext* context,
    Renderer& renderer,
    const XMMATRIX& viewProjection,
    XMVECTOR start,
    XMVECTOR end,
    const XMFLOAT4& color,
    float thickness)
{
    const float length = XMVectorGetX(XMVector3Length(end - start));
    if (length < 0.001f)
        return;

    const XrPosef pose = poseAlongSegment(start, end, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    drawCube(context, renderer, viewProjection, pose, { thickness, thickness, length }, color);
}

void drawSolvedArm(
    ID3D11DeviceContext* context,
    Renderer& renderer,
    const XMMATRIX& viewProjection,
    const SolvedArm& arm,
    const XMFLOAT4& upperColor,
    const XMFLOAT4& lowerColor)
{
    const XMVECTOR shoulder = vec3(arm.shoulder.position);
    const XMVECTOR elbow = vec3(arm.elbow.position);
    const XMVECTOR wrist = vec3(arm.wrist.position);
    drawSegment(context, renderer, viewProjection, shoulder, elbow, upperColor, 0.035f);
    drawSegment(context, renderer, viewProjection, elbow, wrist, lowerColor, 0.032f);
    drawCube(context, renderer, viewProjection, arm.shoulder, { 0.045f, 0.045f, 0.045f }, upperColor);
    drawCube(context, renderer, viewProjection, arm.elbow, { 0.040f, 0.040f, 0.040f }, { 1.0f, 0.10f, 0.95f, 1.0f });
    drawCube(context, renderer, viewProjection, arm.wrist, { 0.070f, 0.070f, 0.070f }, lowerColor);
    if (envFloat("FNVXR_DEBUG_AXES", envFloat("FNVXR_DEBUG_LEFT_AXES", 1.0f)) != 0.0f)
    {
        drawPoseAxes(context, renderer, viewProjection, arm.forearm);
        drawPoseAxes(context, renderer, viewProjection, arm.wrist);
    }
}

void drawPipboyRig(ID3D11DeviceContext* context, Renderer& renderer, const XMMATRIX& viewProjection, const SolvedArm& leftArm)
{
    const XMFLOAT3 forearmOffset {
        envFloat("FNVXR_PIPBOY_OFFSET_X", 0.0f),
        envFloat("FNVXR_PIPBOY_OFFSET_Y", 0.04f),
        envFloat("FNVXR_PIPBOY_OFFSET_Z", 0.02f)
    };
    const XMFLOAT3 faceDown {
        degreesToRadians(envFloat("FNVXR_PIPBOY_ROT_X", 0.0f)),
        degreesToRadians(envFloat("FNVXR_PIPBOY_ROT_Y", 0.0f)),
        degreesToRadians(envFloat("FNVXR_PIPBOY_ROT_Z", 0.0f))
    };
    const float scale = envFloat("FNVXR_PIPBOY_SCALE", 1.0f);
    const XrPosef& forearmPose = leftArm.forearm;

    if (envFloat("FNVXR_DEBUG_AXES", envFloat("FNVXR_DEBUG_LEFT_AXES", 1.0f)) != 0.0f)
    {
        drawPoseAxes(context, renderer, viewProjection, forearmPose);
        drawLocalCube(
            context,
            renderer,
            viewProjection,
            forearmPose,
            forearmOffset,
            { 0.0f, 0.0f, 0.0f },
            { 0.026f, 0.026f, 0.026f },
            { 1.0f, 0.05f, 1.0f, 1.0f });
    }

    drawLocalCube(
        context,
        renderer,
        viewProjection,
        forearmPose,
        { forearmOffset.x, forearmOffset.y, forearmOffset.z - 0.06f },
        faceDown,
        { 0.155f * scale, 0.018f * scale, 0.022f * scale },
        { 0.0f, 0.35f, 0.05f, 1.0f });
    drawLocalCube(
        context,
        renderer,
        viewProjection,
        forearmPose,
        { forearmOffset.x, forearmOffset.y, forearmOffset.z + 0.06f },
        faceDown,
        { 0.155f * scale, 0.018f * scale, 0.022f * scale },
        { 0.0f, 0.55f, 0.08f, 1.0f });
    drawLocalCube(
        context,
        renderer,
        viewProjection,
        forearmPose,
        forearmOffset,
        faceDown,
        { 0.14f * scale, 0.026f * scale, 0.09f * scale },
        { 0.02f, 0.7f, 0.18f, 1.0f });
    drawLocalCube(
        context,
        renderer,
        viewProjection,
        forearmPose,
        { forearmOffset.x, forearmOffset.y + 0.016f, forearmOffset.z },
        faceDown,
        { 0.105f * scale, 0.004f * scale, 0.065f * scale },
        { 0.0f, 0.95f, 1.0f, 1.0f });
}

void drawHandFingers(
    ID3D11DeviceContext* context,
    Renderer& renderer,
    const XMMATRIX& viewProjection,
    const XrPosef& wristPose,
    bool left)
{
    if (!envEnabled("FNVXR_SHOW_HAND_FINGERS", true))
        return;

    const float side = left ? -1.0f : 1.0f;
    const XMFLOAT4 palmColor = left
        ? XMFLOAT4 { 0.05f, 0.38f, 0.85f, 1.0f }
        : XMFLOAT4 { 0.92f, 0.48f, 0.08f, 1.0f };
    const XMFLOAT4 fingerColor = left
        ? XMFLOAT4 { 0.18f, 0.62f, 1.0f, 1.0f }
        : XMFLOAT4 { 1.0f, 0.78f, 0.18f, 1.0f };

    drawLocalCube(
        context,
        renderer,
        viewProjection,
        wristPose,
        { 0.0f, 0.0f, -0.035f },
        { 0.0f, 0.0f, 0.0f },
        { 0.070f, 0.030f, 0.095f },
        palmColor);

    for (int finger = 0; finger < 4; ++finger)
    {
        const float x = (-0.033f + static_cast<float>(finger) * 0.022f) * side;
        drawLocalCube(
            context,
            renderer,
            viewProjection,
            wristPose,
            { x, 0.002f, -0.105f },
            { 0.0f, 0.0f, 0.0f },
            { 0.010f, 0.012f, 0.085f },
            fingerColor);
        drawLocalCube(
            context,
            renderer,
            viewProjection,
            wristPose,
            { x, 0.000f, -0.158f },
            { degreesToRadians(8.0f), 0.0f, 0.0f },
            { 0.009f, 0.010f, 0.048f },
            fingerColor);
    }

    drawLocalCube(
        context,
        renderer,
        viewProjection,
        wristPose,
        { 0.055f * side, -0.006f, -0.045f },
        { 0.0f, degreesToRadians(32.0f) * side, degreesToRadians(20.0f) * side },
        { 0.014f, 0.014f, 0.065f },
        fingerColor);
}

void drawPauseModeScene(
    ID3D11DeviceContext* context,
    Renderer& renderer,
    const XMMATRIX& viewProjection,
    const XrPosef& sceneAnchor)
{
    if (!envEnabled("FNVXR_SHOW_PAUSE_SCENE", true))
        return;

    if (renderer.pauseSceneTextureCount == 0)
        return;

    const float radius = envFloat("FNVXR_MENU_SCENE_SHELL_RADIUS", 4.0f);
    const float wallHeight = envFloat("FNVXR_MENU_SCENE_SHELL_HEIGHT", 3.0f);
    const float floorSize = envFloat("FNVXR_MENU_SCENE_SHELL_FLOOR_SIZE", 7.0f);
    const float yOffset = envFloat("FNVXR_MENU_SCENE_SHELL_Y", 0.0f);
    const float brightness = envFloat("FNVXR_MENU_SCENE_SHELL_BRIGHTNESS", 1.0f);

    auto sceneView = [&](size_t index) -> ID3D11ShaderResourceView*
    {
        const size_t count = std::max<size_t>(1, renderer.pauseSceneTextureCount);
        return renderer.pauseSceneTextureViews[index % count].Get();
    };

    const XMFLOAT4 color { brightness, brightness, brightness, 1.0f };

    drawLocalTexturedPlane(
        context,
        renderer,
        viewProjection,
        sceneAnchor,
        { 0.0f, yOffset, -radius },
        { 0.0f, 0.0f, 0.0f },
        { radius * 2.0f, wallHeight, 1.0f },
        color,
        sceneView(0));
    drawLocalTexturedPlane(
        context,
        renderer,
        viewProjection,
        sceneAnchor,
        { -radius, yOffset, 0.0f },
        { 0.0f, -degreesToRadians(90.0f), 0.0f },
        { radius * 2.0f, wallHeight, 1.0f },
        color,
        sceneView(1));
    drawLocalTexturedPlane(
        context,
        renderer,
        viewProjection,
        sceneAnchor,
        { radius, yOffset, 0.0f },
        { 0.0f, degreesToRadians(90.0f), 0.0f },
        { radius * 2.0f, wallHeight, 1.0f },
        color,
        sceneView(2));
    drawLocalTexturedPlane(
        context,
        renderer,
        viewProjection,
        sceneAnchor,
        { 0.0f, yOffset, radius },
        { 0.0f, degreesToRadians(180.0f), 0.0f },
        { radius * 2.0f, wallHeight, 1.0f },
        color,
        sceneView(3));
    drawLocalTexturedPlane(
        context,
        renderer,
        viewProjection,
        sceneAnchor,
        { 0.0f, yOffset - wallHeight * 0.5f, 0.0f },
        { degreesToRadians(90.0f), 0.0f, 0.0f },
        { floorSize, floorSize, 1.0f },
        { brightness * 0.75f, brightness * 0.75f, brightness * 0.75f, 1.0f },
        sceneView(4));
}

void drawAimRay(ID3D11DeviceContext* context, Renderer& renderer, const XMMATRIX& viewProjection, const XrPosef& aimPose)
{
    drawLocalCube(
        context,
        renderer,
        viewProjection,
        aimPose,
        { 0.0f, 0.0f, -0.35f },
        { 0.0f, 0.0f, 0.0f },
        { 0.008f, 0.008f, 0.70f },
        { 1.0f, 0.95f, 0.05f, 1.0f });
}

bool renderEye(
    OpenXr& xr,
    ID3D11Device* device,
    Renderer& renderer,
    QuadSwapchain& swapchain,
    size_t eyeIndex,
    const XrView& view,
    const XrPosef& leftPose,
    const XrPosef& rightPose,
    const XrPosef& leftAimPose,
    const XrPosef& rightAimPose,
    const BodyRig& bodyRig,
    const MenuPointer& menuPointer,
    const GamePlane& gamePlane,
    const XrPosef& pauseSceneAnchor,
    bool gameUiMode,
    bool showGamePlane,
    bool useStereoFullscreen,
    const float clearColor[4])
{
    uint32_t imageIndex = 0;
    XrSwapchainImageAcquireInfo acquireInfo { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
    XrResult result = xr.acquireSwapchainImage(swapchain.handle, &acquireInfo, &imageIndex);
    if (result != XR_SUCCESS)
        return false;

    XrSwapchainImageWaitInfo waitInfo { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
    waitInfo.timeout = XR_INFINITE_DURATION;
    result = xr.waitSwapchainImage(swapchain.handle, &waitInfo);
    if (result != XR_SUCCESS)
    {
        releaseSwapchainImage(xr, swapchain.handle);
        return false;
    }

    ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(&context);
    ID3D11RenderTargetView* rtv = swapchain.renderTargetViews[imageIndex].Get();
    ID3D11DepthStencilView* dsv = swapchain.depthStencilView.Get();

    D3D11_VIEWPORT viewport {};
    viewport.Width = static_cast<float>(swapchain.width);
    viewport.Height = static_cast<float>(swapchain.height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);
    context->RSSetState(renderer.rasterizerState.Get());
    context->OMSetDepthStencilState(renderer.depthState.Get(), 0);
    context->OMSetRenderTargets(1, &rtv, dsv);
    context->ClearRenderTargetView(rtv, clearColor);
    context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    if (envEnabled("FNVXR_HOST_CLEAR_ONLY", false))
        return releaseSwapchainImage(xr, swapchain.handle);

    const XMMATRIX viewProjection = viewFromPose(view.pose) * projectionFromFov(view.fov, 0.05f, 50.0f);
    ID3D11ShaderResourceView* uiTextureView = renderer.gameTextureView.Get();
    ID3D11ShaderResourceView* worldTextureView = uiTextureView;
    if (useStereoFullscreen
        && renderer.hasStereoGameFrame
        && renderer.stereoGameFrameSeparated
        && eyeIndex < renderer.stereoGameTextureViews.size())
    {
        worldTextureView = renderer.stereoGameTextureViews[eyeIndex].Get();
    }

    if (useStereoFullscreen)
    {
        context->OMSetDepthStencilState(renderer.gamePlaneDepthState.Get(), 0);
        drawGameFullscreen(context.Get(), renderer, worldTextureView);
        context->OMSetDepthStencilState(renderer.depthState.Get(), 0);
    }
    else
    {
        context->OMSetDepthStencilState(renderer.depthState.Get(), 0);
        if (gameUiMode)
        {
            bindSolidPipeline(context.Get(), renderer);
            drawPauseModeScene(context.Get(), renderer, viewProjection, pauseSceneAnchor);
        }
    }

    if (showGamePlane)
    {
        context->OMSetDepthStencilState(renderer.depthState.Get(), 0);
        const GamePlaneModeSettings modeSettings = gamePlaneModeSettings(gamePlaneMode());
        const bool allowSourceSurround =
            !gameUiMode
            && modeSettings.surroundInGameplay;
        drawGamePlane(context.Get(), renderer, viewProjection, gamePlane, uiTextureView, allowSourceSurround);
        context->OMSetDepthStencilState(renderer.depthState.Get(), 0);
    }

    bindSolidPipeline(context.Get(), renderer);

    if (showGamePlane)
    {
        context->OMSetDepthStencilState(renderer.gamePlaneDepthState.Get(), 0);
        drawMenuPointer(context.Get(), renderer, viewProjection, menuPointer, gamePlane);
        context->OMSetDepthStencilState(renderer.depthState.Get(), 0);
    }
    const bool drawWorldProps = envEnabled("FNVXR_SHOW_WORLD_PROPS", false)
        && (!useStereoFullscreen || envEnabled("FNVXR_OVERLAY_PROPS_ON_FULLSCREEN", false));
    if (drawWorldProps && envEnabled("FNVXR_SHOW_BODY_RIG", true))
    {
        drawSolvedArm(
            context.Get(),
            renderer,
            viewProjection,
            bodyRig.left,
            { 0.05f, 0.35f, 1.0f, 1.0f },
            { 0.02f, 0.85f, 0.20f, 1.0f });
        drawHandFingers(context.Get(), renderer, viewProjection, bodyRig.left.wrist, true);
        drawSolvedArm(
            context.Get(),
            renderer,
            viewProjection,
            bodyRig.right,
            { 1.0f, 0.42f, 0.08f, 1.0f },
            { 1.0f, 0.76f, 0.05f, 1.0f });
        drawHandFingers(context.Get(), renderer, viewProjection, bodyRig.right.wrist, false);
    }
    else if (drawWorldProps)
    {
        drawCube(context.Get(), renderer, viewProjection, leftPose, { 0.08f, 0.08f, 0.08f }, { 0.05f, 0.35f, 1.0f, 1.0f });
        drawCube(context.Get(), renderer, viewProjection, rightPose, { 0.08f, 0.08f, 0.08f }, { 1.0f, 0.42f, 0.08f, 1.0f });
        drawHandFingers(context.Get(), renderer, viewProjection, leftPose, true);
        drawHandFingers(context.Get(), renderer, viewProjection, rightPose, false);
    }
    if (drawWorldProps && envEnabled("FNVXR_SHOW_PIPBOY_RIG", true))
        drawPipboyRig(context.Get(), renderer, viewProjection, bodyRig.left);
    if (drawWorldProps && envEnabled("FNVXR_SHOW_LEFT_AIM_RAY", false))
        drawAimRay(context.Get(), renderer, viewProjection, leftAimPose);
    if (drawWorldProps && envEnabled("FNVXR_SHOW_RIGHT_AIM_RAY", true))
        drawAimRay(context.Get(), renderer, viewProjection, rightAimPose);

    return releaseSwapchainImage(xr, swapchain.handle);
}

bool extensionAvailable(OpenXr& xr, const char* extensionName)
{
    uint32_t count = 0;
    if (xr.enumerateExtensions(nullptr, 0, &count, nullptr) != XR_SUCCESS)
        return false;

    std::vector<XrExtensionProperties> extensions(count);
    for (auto& extension : extensions)
        extension.type = XR_TYPE_EXTENSION_PROPERTIES;

    if (xr.enumerateExtensions(nullptr, count, &count, extensions.data()) != XR_SUCCESS)
        return false;

    for (const auto& extension : extensions)
    {
        if (std::strcmp(extension.extensionName, extensionName) == 0)
            return true;
    }

    return false;
}

bool sameLuid(const LUID& left, const LUID& right)
{
    return left.HighPart == right.HighPart && left.LowPart == right.LowPart;
}

bool createD3D11DeviceForRuntime(const XrGraphicsRequirementsD3D11KHR& requirements, ID3D11Device** device)
{
    ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
        return false;

    ComPtr<IDXGIAdapter1> selectedAdapter;
    for (UINT adapterIndex = 0;; ++adapterIndex)
    {
        ComPtr<IDXGIAdapter1> adapter;
        if (factory->EnumAdapters1(adapterIndex, &adapter) == DXGI_ERROR_NOT_FOUND)
            break;

        DXGI_ADAPTER_DESC1 desc {};
        adapter->GetDesc1(&desc);
        if (sameLuid(desc.AdapterLuid, requirements.adapterLuid))
        {
            selectedAdapter = adapter;
            break;
        }
    }

    if (!selectedAdapter)
    {
        std::cerr << "could not find D3D11 adapter requested by OpenXR runtime\n";
        return false;
    }

    const D3D_FEATURE_LEVEL requestedLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    D3D_FEATURE_LEVEL createdLevel {};
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    HRESULT hr = D3D11CreateDevice(
        selectedAdapter.Get(),
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        flags,
        requestedLevels,
        static_cast<UINT>(std::size(requestedLevels)),
        D3D11_SDK_VERSION,
        device,
        &createdLevel,
        nullptr);

    if (FAILED(hr))
    {
        std::cerr << "D3D11CreateDevice failed hr=0x" << std::hex << hr << std::dec << "\n";
        return false;
    }

    return true;
}

bool compileShader(const char* source, const char* entryPoint, const char* target, ID3DBlob** blob)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG;
#endif

    ComPtr<ID3DBlob> errors;
    HRESULT hr = D3DCompile(
        source,
        std::strlen(source),
        nullptr,
        nullptr,
        nullptr,
        entryPoint,
        target,
        flags,
        0,
        blob,
        &errors);

    if (FAILED(hr))
    {
        if (errors)
            std::cerr << static_cast<const char*>(errors->GetBufferPointer()) << "\n";
        return false;
    }

    return true;
}

std::vector<TexturedVertex> makeCurvedGamePlaneVertices(float centerU0, float centerV0, float centerU1, float centerV1)
{
    const int segmentsX = std::clamp(envInt("FNVXR_GAME_PLANE_CURVE_SEGMENTS_X", 32), 1, 96);
    const int segmentsY = std::clamp(envInt("FNVXR_GAME_PLANE_CURVE_SEGMENTS_Y", 18), 1, 64);
    const float depthX = envFloat("FNVXR_GAME_PLANE_CURVE_DEPTH_X", 0.22f);
    const float depthY = envFloat("FNVXR_GAME_PLANE_CURVE_DEPTH_Y", 0.08f);
    const float cornerDepth = envFloat("FNVXR_GAME_PLANE_CURVE_CORNER_DEPTH", 0.03f);
    const float sign = envFloat("FNVXR_GAME_PLANE_CURVE_SIGN", 1.0f) < 0.0f ? -1.0f : 1.0f;
    centerU0 = std::clamp(centerU0, 0.0f, 0.99f);
    centerV0 = std::clamp(centerV0, 0.0f, 0.99f);
    centerU1 = std::clamp(centerU1, centerU0 + 0.01f, 1.0f);
    centerV1 = std::clamp(centerV1, centerV0 + 0.01f, 1.0f);

    auto vertex = [&](float u, float v) -> TexturedVertex
    {
        const float x = u - 0.5f;
        const float y = 0.5f - v;
        const float nx = x * 2.0f;
        const float ny = y * 2.0f;
        const float nx2 = nx * nx;
        const float ny2 = ny * ny;
        const float z = sign * (depthX * nx2 + depthY * ny2 + cornerDepth * nx2 * ny2);
        const float sampleU = centerU0 + u * (centerU1 - centerU0);
        const float sampleV = centerV0 + v * (centerV1 - centerV0);
        return { x, y, z, sampleU, sampleV };
    };

    std::vector<TexturedVertex> vertices;
    vertices.reserve(static_cast<size_t>(segmentsX) * static_cast<size_t>(segmentsY) * 6u);
    for (int y = 0; y < segmentsY; ++y)
    {
        const float v0 = static_cast<float>(y) / static_cast<float>(segmentsY);
        const float v1 = static_cast<float>(y + 1) / static_cast<float>(segmentsY);
        for (int x = 0; x < segmentsX; ++x)
        {
            const float u0 = static_cast<float>(x) / static_cast<float>(segmentsX);
            const float u1 = static_cast<float>(x + 1) / static_cast<float>(segmentsX);
            const TexturedVertex topLeft = vertex(u0, v0);
            const TexturedVertex topRight = vertex(u1, v0);
            const TexturedVertex bottomLeft = vertex(u0, v1);
            const TexturedVertex bottomRight = vertex(u1, v1);
            vertices.push_back(bottomLeft);
            vertices.push_back(topLeft);
            vertices.push_back(topRight);
            vertices.push_back(bottomLeft);
            vertices.push_back(topRight);
            vertices.push_back(bottomRight);
        }
    }
    return vertices;
}

std::vector<TexturedVertex> makeCurvedGamePlaneVertices()
{
    const float centerU0 = std::clamp(envFloat("FNVXR_GAME_PLANE_CENTER_UV_LEFT", 0.0f), 0.0f, 0.99f);
    const float centerV0 = std::clamp(envFloat("FNVXR_GAME_PLANE_CENTER_UV_TOP", 0.0f), 0.0f, 0.99f);
    const float centerU1 = std::clamp(envFloat("FNVXR_GAME_PLANE_CENTER_UV_RIGHT", 1.0f), centerU0 + 0.01f, 1.0f);
    const float centerV1 = std::clamp(envFloat("FNVXR_GAME_PLANE_CENTER_UV_BOTTOM", 1.0f), centerV0 + 0.01f, 1.0f);
    return makeCurvedGamePlaneVertices(centerU0, centerV0, centerU1, centerV1);
}

std::vector<TexturedVertex> makeSurroundGamePlaneVertices()
{
    const GamePlaneMode mode = gamePlaneMode();
    const GamePlaneModeSettings modeSettings = gamePlaneModeSettings(mode);
    if (!modeSettings.sourceSurround)
        return {};

    const int segmentsX = std::clamp(modeSettings.segmentsX, 1, 128);
    const int segmentsY = std::clamp(modeSettings.segmentsY, 1, 96);
    const float centerDepthX = modeSettings.centerDepthX;
    const float centerDepthY = modeSettings.centerDepthY;
    const float centerCornerDepth = modeSettings.centerCornerDepth;
    const float depthX = modeSettings.surroundDepthX;
    const float depthY = modeSettings.surroundDepthY;
    const float cornerDepth = modeSettings.surroundCornerDepth;
    const float sign = modeSettings.sign;
    const float scaleX = std::max(1.0f, modeSettings.surroundScaleX);
    const float scaleY = std::max(1.0f, modeSettings.surroundScaleY);
    const bool atlasStitch = modeSettings.atlasStitch;
    float sourceCenterU0 = modeSettings.centerU0;
    float sourceCenterV0 = modeSettings.centerV0;
    float sourceCenterU1 = modeSettings.centerU1;
    float sourceCenterV1 = modeSettings.centerV1;
    if (atlasStitch && envEnabled("FNVXR_GAME_PLANE_SURROUND_WIDE_SOURCE_UV", false))
    {
        const float textureAspect = envFloat(
            "FNVXR_GAME_PLANE_FOV_ASPECT",
            static_cast<float>(envInt("FNVXR_GAME_TEXTURE_WIDTH", 2048))
                / static_cast<float>(std::max(1, envInt("FNVXR_GAME_TEXTURE_HEIGHT", 1280))));
        const float centerFovX = envFloat("FNVXR_GAME_PLANE_CENTER_FOV_X_DEG", envFloat("FNVXR_GAME_PLANE_CENTER_FOV_DEG", 95.0f));
        const float wideFovX =
            envFloat("FNVXR_GAME_PLANE_SURROUND_WIDE_FOV_X_DEG", envFloat("FNVXR_GAME_PLANE_SURROUND_WIDE_FOV_DEG", 115.0f));
        const float derivedCenterFovY = horizontalToVerticalFovDegrees(centerFovX, textureAspect);
        const float derivedWideFovY = horizontalToVerticalFovDegrees(wideFovX, textureAspect);
        const float centerFovY = envFloat("FNVXR_GAME_PLANE_CENTER_FOV_Y_DEG", derivedCenterFovY);
        const float wideFovY = envFloat("FNVXR_GAME_PLANE_SURROUND_WIDE_FOV_Y_DEG", derivedWideFovY);
        const float cropX = fovCenterCropRatio(centerFovX, wideFovX);
        const float cropY = fovCenterCropRatio(centerFovY, wideFovY);
        sourceCenterU0 = 0.5f - cropX * 0.5f;
        sourceCenterU1 = 0.5f + cropX * 0.5f;
        sourceCenterV0 = 0.5f - cropY * 0.5f;
        sourceCenterV1 = 0.5f + cropY * 0.5f;
    }
    const float atlasExpandX = 1.0f;
    const float atlasExpandY = 1.0f;
    const bool drawBottom = modeSettings.drawBottom;
    const bool drawCenter = modeSettings.drawCenter;
    const bool edgeStitch = modeSettings.edgeStitch;
    const float centerU0 = modeSettings.centerU0;
    const float centerV0 = modeSettings.centerV0;
    const float centerU1 = modeSettings.centerU1;
    const float centerV1 = modeSettings.centerV1;
    const float sourceCenterWidth = std::max(0.05f, sourceCenterU1 - sourceCenterU0);
    const float sourceCenterHeight = std::max(0.05f, sourceCenterV1 - sourceCenterV0);
    const float outerLeftX = atlasStitch ? -0.5f - atlasExpandX * sourceCenterU0 / sourceCenterWidth : -scaleX * 0.5f;
    const float outerRightX = atlasStitch ? 0.5f + atlasExpandX * (1.0f - sourceCenterU1) / sourceCenterWidth : scaleX * 0.5f;
    const float outerTopY = atlasStitch ? 0.5f + atlasExpandY * sourceCenterV0 / sourceCenterHeight : scaleY * 0.5f;
    const float outerBottomY = atlasStitch ? -0.5f - atlasExpandY * (1.0f - sourceCenterV1) / sourceCenterHeight : -scaleY * 0.5f;
    const float outerX = std::max(std::fabs(outerLeftX), std::fabs(outerRightX));
    const float outerY = std::max(std::fabs(outerTopY), std::fabs(outerBottomY));

    auto vertex = [&](float x, float y, float u, float v) -> TexturedVertex
    {
        const float centerX = std::clamp(x, -0.5f, 0.5f);
        const float centerY = std::clamp(y, -0.5f, 0.5f);
        const float centerNx = centerX * 2.0f;
        const float centerNy = centerY * 2.0f;
        const float centerNx2 = centerNx * centerNx;
        const float centerNy2 = centerNy * centerNy;
        const float centerZ =
            centerDepthX * centerNx2 + centerDepthY * centerNy2 + centerCornerDepth * centerNx2 * centerNy2;

        const float atlasOuterX =
            atlasStitch
                    ? std::max(std::fabs(outerLeftX), std::fabs(outerRightX))
                    : scaleX * 0.5f;
        const float atlasOuterY =
            atlasStitch
                    ? std::max(std::fabs(outerTopY), std::fabs(outerBottomY))
                    : scaleY * 0.5f;
        const float maxOuterX = std::max(0.001f, atlasOuterX - 0.5f);
        const float maxOuterY = std::max(0.001f, atlasOuterY - 0.5f);
        const float outerBlendX = std::clamp((std::fabs(x) - 0.5f) / maxOuterX, 0.0f, 1.0f);
        const float outerBlendY = std::clamp((std::fabs(y) - 0.5f) / maxOuterY, 0.0f, 1.0f);
        const float outerBlendX2 = outerBlendX * outerBlendX;
        const float outerBlendY2 = outerBlendY * outerBlendY;
        const float extraZ = depthX * outerBlendX2 + depthY * outerBlendY2 + cornerDepth * outerBlendX2 * outerBlendY2;
        const float z = sign * (centerZ + extraZ);
        return { x, y, z, u, v };
    };

    std::vector<TexturedVertex> vertices;
    vertices.reserve(static_cast<size_t>(segmentsX) * static_cast<size_t>(segmentsY) * 12u);

    auto addPatch = [&](float x0, float x1, float y0, float y1, float u0, float u1, float v0, float v1)
    {
        if (x1 <= x0 || y1 <= y0 || u1 < u0 || v1 < v0)
            return;
        const int patchSegmentsX = std::max(1, static_cast<int>(std::ceil((x1 - x0) * static_cast<float>(segmentsX))));
        const int patchSegmentsY = std::max(1, static_cast<int>(std::ceil((y1 - y0) * static_cast<float>(segmentsY))));
        for (int y = 0; y < patchSegmentsY; ++y)
        {
            const float ty0 = static_cast<float>(y) / static_cast<float>(patchSegmentsY);
            const float ty1 = static_cast<float>(y + 1) / static_cast<float>(patchSegmentsY);
            const float py0 = y0 + ty0 * (y1 - y0);
            const float py1 = y0 + ty1 * (y1 - y0);
            const float pv0 = v1 + ty0 * (v0 - v1);
            const float pv1 = v1 + ty1 * (v0 - v1);
            for (int x = 0; x < patchSegmentsX; ++x)
            {
                const float tx0 = static_cast<float>(x) / static_cast<float>(patchSegmentsX);
                const float tx1 = static_cast<float>(x + 1) / static_cast<float>(patchSegmentsX);
                const float px0 = x0 + tx0 * (x1 - x0);
                const float px1 = x0 + tx1 * (x1 - x0);
                const float pu0 = u0 + tx0 * (u1 - u0);
                const float pu1 = u0 + tx1 * (u1 - u0);
                const TexturedVertex bottomLeft = vertex(px0, py0, pu0, pv0);
                const TexturedVertex topLeft = vertex(px0, py1, pu0, pv1);
                const TexturedVertex topRight = vertex(px1, py1, pu1, pv1);
                const TexturedVertex bottomRight = vertex(px1, py0, pu1, pv0);
                vertices.push_back(bottomLeft);
                vertices.push_back(topLeft);
                vertices.push_back(topRight);
                vertices.push_back(bottomLeft);
                vertices.push_back(topRight);
                vertices.push_back(bottomRight);
            }
        }
    };

    auto addMappedPatch = [&](float x0, float x1, float y0, float y1, auto&& uvAt)
    {
        if (x1 <= x0 || y1 <= y0)
            return;
        const int patchSegmentsX = std::max(1, static_cast<int>(std::ceil((x1 - x0) * static_cast<float>(segmentsX))));
        const int patchSegmentsY = std::max(1, static_cast<int>(std::ceil((y1 - y0) * static_cast<float>(segmentsY))));
        for (int y = 0; y < patchSegmentsY; ++y)
        {
            const float ty0 = static_cast<float>(y) / static_cast<float>(patchSegmentsY);
            const float ty1 = static_cast<float>(y + 1) / static_cast<float>(patchSegmentsY);
            const float py0 = y0 + ty0 * (y1 - y0);
            const float py1 = y0 + ty1 * (y1 - y0);
            for (int x = 0; x < patchSegmentsX; ++x)
            {
                const float tx0 = static_cast<float>(x) / static_cast<float>(patchSegmentsX);
                const float tx1 = static_cast<float>(x + 1) / static_cast<float>(patchSegmentsX);
                const float px0 = x0 + tx0 * (x1 - x0);
                const float px1 = x0 + tx1 * (x1 - x0);
                const std::array<float, 2> bottomLeftUv = uvAt(px0, py0);
                const std::array<float, 2> topLeftUv = uvAt(px0, py1);
                const std::array<float, 2> topRightUv = uvAt(px1, py1);
                const std::array<float, 2> bottomRightUv = uvAt(px1, py0);
                const TexturedVertex bottomLeft = vertex(px0, py0, bottomLeftUv[0], bottomLeftUv[1]);
                const TexturedVertex topLeft = vertex(px0, py1, topLeftUv[0], topLeftUv[1]);
                const TexturedVertex topRight = vertex(px1, py1, topRightUv[0], topRightUv[1]);
                const TexturedVertex bottomRight = vertex(px1, py0, bottomRightUv[0], bottomRightUv[1]);
                vertices.push_back(bottomLeft);
                vertices.push_back(topLeft);
                vertices.push_back(topRight);
                vertices.push_back(bottomLeft);
                vertices.push_back(topRight);
                vertices.push_back(bottomRight);
            }
        }
    };

    if (atlasStitch)
    {
        if (drawCenter)
            addPatch(-0.5f, 0.5f, -0.5f, 0.5f, sourceCenterU0, sourceCenterU1, sourceCenterV0, sourceCenterV1);
        addPatch(outerLeftX, -0.5f, 0.5f, outerTopY, 0.0f, sourceCenterU0, 0.0f, sourceCenterV0);
        addPatch(-0.5f, 0.5f, 0.5f, outerTopY, sourceCenterU0, sourceCenterU1, 0.0f, sourceCenterV0);
        addPatch(0.5f, outerRightX, 0.5f, outerTopY, sourceCenterU1, 1.0f, 0.0f, sourceCenterV0);
        addPatch(outerLeftX, -0.5f, -0.5f, 0.5f, 0.0f, sourceCenterU0, sourceCenterV0, sourceCenterV1);
        addPatch(0.5f, outerRightX, -0.5f, 0.5f, sourceCenterU1, 1.0f, sourceCenterV0, sourceCenterV1);
    }
    else
    {
        const bool weaponSafe = modeSettings.weaponSafe;
        const float surroundU0 = 0.10f;
        const float surroundV0 = 0.07f;
        const float surroundU1 = 0.90f;
        const float surroundV1 = 0.80f;
        if (edgeStitch)
        {
            const bool singleSourceDuplicate = modeSettings.singleSourceDuplicate;
            const bool singleSourceClamp = modeSettings.singleSourceClamp;
            const float safeU0 = modeSettings.safeU0;
            const float safeU1 = modeSettings.safeU1;
            const float topV0 = modeSettings.topV0;
            const float topV1 = modeSettings.topV1;
            const float sideV0 = modeSettings.sideV0;
            const float sideV1 = modeSettings.sideV1;
            const float safeBottomV0 = modeSettings.safeBottomV0;
            const float safeBottomV1 = modeSettings.safeBottomV1;
            const float bottomV0 = modeSettings.bottomV0;
            const float bottomV1 = modeSettings.bottomV1;
            const float transitionPower = std::clamp(modeSettings.transitionPower, 0.20f, 2.0f);
            auto edgeT = [&](float distance, float span) -> float
            {
                const float t = std::clamp(distance / std::max(0.001f, span), 0.0f, 1.0f);
                return std::pow(t, transitionPower);
            };
            auto lerp = [](float a, float b, float t) -> float
            {
                return a + (b - a) * t;
            };
            auto uvAt = [&](float x, float y) -> std::array<float, 2>
            {
                const float clampedCenterX = std::clamp(x, -0.5f, 0.5f);
                const float clampedCenterY = std::clamp(y, -0.5f, 0.5f);
                const float localU = clampedCenterX + 0.5f;
                const float localV = 0.5f - clampedCenterY;
                float u = lerp(centerU0, centerU1, localU);
                float v = lerp(centerV0, centerV1, localV);

                const bool left = x < -0.5f;
                const bool right = x > 0.5f;
                const bool top = y > 0.5f;
                const bool bottom = y < -0.5f;
                if (singleSourceDuplicate)
                {
                    const float duplicateU = std::clamp((x + outerX) / std::max(0.001f, 2.0f * outerX), 0.0f, 1.0f);
                    const float duplicateV = std::clamp((outerY - y) / std::max(0.001f, 2.0f * outerY), 0.0f, 1.0f);
                    return {
                        lerp(centerU0, centerU1, duplicateU),
                        lerp(centerV0, centerV1, duplicateV)
                    };
                }
                if (singleSourceClamp)
                {
                    const float sourceWidth = std::max(0.001f, centerU1 - centerU0);
                    const float sourceHeight = std::max(0.001f, centerV1 - centerV0);
                    const bool useInwardBands = modeSettings.useInwardBands;
                    const bool useSafeBands = weaponSafe && modeSettings.useSafeBands;
                    const float edgeBandX = 0.16f;
                    const float edgeBandY = 0.14f;
                    const float bottomBandY = 0.10f;
                    const float tx = left
                        ? edgeT(-0.5f - x, outerX - 0.5f)
                        : (right ? edgeT(x - 0.5f, outerX - 0.5f) : 0.0f);
                    const float ty = top
                        ? edgeT(y - 0.5f, outerY - 0.5f)
                        : (bottom ? edgeT(-0.5f - y, outerY - 0.5f) : 0.0f);

                    if (useInwardBands)
                    {
                        if (left)
                            u = lerp(centerU0, centerU0 + sourceWidth * edgeBandX, tx);
                        else if (right)
                            u = lerp(centerU1, centerU1 - sourceWidth * edgeBandX, tx);
                        if (top)
                            v = lerp(centerV0, centerV0 + sourceHeight * edgeBandY, ty);
                        else if (bottom)
                            v = lerp(centerV1, centerV1 - sourceHeight * bottomBandY, ty);
                        return { std::clamp(u, 0.0f, 1.0f), std::clamp(v, 0.0f, 1.0f) };
                    }

                    if (useSafeBands)
                    {
                        const float shieldU =
                            std::clamp((x + outerX) / std::max(0.001f, 2.0f * outerX), 0.0f, 1.0f);
                        if (top)
                        {
                            u = lerp(safeU0, safeU1, shieldU);
                            v = lerp(topV1, topV0, ty);
                        }
                        else if (bottom)
                        {
                            u = lerp(safeU0, safeU1, shieldU);
                            v = lerp(safeBottomV0, safeBottomV1, ty);
                        }
                        else if (left || right)
                        {
                            const float sourceSpan = safeU1 - safeU0;
                            const float sideInnerU = left
                                ? safeU0 + sourceSpan * 0.35f
                                : safeU1 - sourceSpan * 0.35f;
                            const float sideOuterU = left ? safeU0 : safeU1;
                            u = lerp(sideInnerU, sideOuterU, tx);
                            v = lerp(sideV0, sideV1, localV);
                        }
                        return { std::clamp(u, 0.0f, 1.0f), std::clamp(v, 0.0f, 1.0f) };
                    }

                    if (left)
                        u = centerU0;
                    else if (right)
                        u = centerU1;

                    if (top)
                        v = centerV0;
                    else if (bottom)
                        v = centerV1;
                    return { std::clamp(u, 0.0f, 1.0f), std::clamp(v, 0.0f, 1.0f) };
                }

                const float tx = left
                    ? edgeT(-0.5f - x, outerX - 0.5f)
                    : (right ? edgeT(x - 0.5f, outerX - 0.5f) : 0.0f);

                if (left)
                    u = lerp(centerU0, weaponSafe ? safeU0 : surroundU0, tx);
                else if (right)
                    u = lerp(centerU1, weaponSafe ? safeU1 : surroundU1, tx);

                if (top)
                {
                    const float ty = edgeT(y - 0.5f, outerY - 0.5f);
                    v = lerp(centerV0, weaponSafe ? topV1 : surroundV0, ty);
                }
                else if (bottom)
                {
                    const float ty = edgeT(-0.5f - y, outerY - 0.5f);
                    v = lerp(centerV1, weaponSafe ? safeBottomV1 : bottomV1, ty);
                }
                else if (weaponSafe && (left || right))
                {
                    const float sideTargetV = lerp(sideV0, sideV1, localV);
                    v = lerp(v, sideTargetV, tx);
                }

                return { std::clamp(u, 0.0f, 1.0f), std::clamp(v, 0.0f, 1.0f) };
            };

            if (drawCenter)
                addMappedPatch(-0.5f, 0.5f, -0.5f, 0.5f, uvAt);
            addMappedPatch(-outerX, -0.5f,  0.5f,  outerY, uvAt);
            addMappedPatch(-0.5f,   0.5f,  0.5f,  outerY, uvAt);
            addMappedPatch( 0.5f,   outerX, 0.5f,  outerY, uvAt);
            addMappedPatch(-outerX, -0.5f, -0.5f,  0.5f,  uvAt);
            addMappedPatch( 0.5f,   outerX, -0.5f, 0.5f,  uvAt);
            if (drawBottom)
            {
                addMappedPatch(-outerX, -0.5f, -outerY, -0.5f, uvAt);
                addMappedPatch(-0.5f,    0.5f, -outerY, -0.5f, uvAt);
                addMappedPatch( 0.5f,   outerX, -outerY, -0.5f, uvAt);
            }
        }
        else
        {
            if (drawCenter)
                addPatch(-0.5f, 0.5f, -0.5f, 0.5f, centerU0, centerU1, centerV0, centerV1);
            if (weaponSafe)
            {
                const float safeU0 = envFloat("FNVXR_GAME_PLANE_SURROUND_SAFE_SOURCE_LEFT", 0.18f);
                const float safeU1 = envFloat("FNVXR_GAME_PLANE_SURROUND_SAFE_SOURCE_RIGHT", 0.62f);
                const float topV0 = envFloat("FNVXR_GAME_PLANE_SURROUND_SAFE_TOP_SOURCE_TOP", 0.04f);
                const float topV1 = envFloat("FNVXR_GAME_PLANE_SURROUND_SAFE_TOP_SOURCE_BOTTOM", 0.24f);
                const float sideV0 = envFloat("FNVXR_GAME_PLANE_SURROUND_SAFE_SIDE_SOURCE_TOP", 0.16f);
                const float sideV1 = envFloat("FNVXR_GAME_PLANE_SURROUND_SAFE_SIDE_SOURCE_BOTTOM", 0.42f);
                const float safeMid = safeU0 + (safeU1 - safeU0) * 0.5f;
                addPatch(-outerX, -0.5f,  0.5f,  outerY, safeU0,  safeMid, topV0, topV1);
                addPatch(-0.5f,   0.5f,  0.5f,  outerY, safeU0,  safeU1,  topV0, topV1);
                addPatch( 0.5f,   outerX, 0.5f,  outerY, safeMid, safeU1,  topV0, topV1);
                addPatch(-outerX, -0.5f, -0.5f,  0.5f,  safeU0,  safeMid, sideV0, sideV1);
                addPatch( 0.5f,   outerX, -0.5f, 0.5f,  safeMid, safeU1,  sideV0, sideV1);
            }
            else
            {
                addPatch(-outerX, -0.5f,  0.5f,  outerY, 0.0f,       surroundU0, 0.0f,      surroundV0);
                addPatch(-0.5f,   0.5f,  0.5f,  outerY, surroundU0, surroundU1, 0.0f,      surroundV0);
                addPatch( 0.5f,   outerX, 0.5f,  outerY, surroundU1, 1.0f,      0.0f,      surroundV0);
                addPatch(-outerX, -0.5f, -0.5f,  0.5f,  0.0f,       surroundU0, surroundV0, surroundV1);
                addPatch( 0.5f,   outerX, -0.5f, 0.5f,  surroundU1, 1.0f,      surroundV0, surroundV1);
            }
        }
    }
    if (drawBottom && (atlasStitch || !edgeStitch))
    {
        if (atlasStitch)
        {
            addPatch(outerLeftX, -0.5f, outerBottomY, -0.5f, 0.0f, sourceCenterU0, sourceCenterV1, 1.0f);
            addPatch(-0.5f, 0.5f, outerBottomY, -0.5f, sourceCenterU0, sourceCenterU1, sourceCenterV1, 1.0f);
            addPatch(0.5f, outerRightX, outerBottomY, -0.5f, sourceCenterU1, 1.0f, sourceCenterV1, 1.0f);
        }
        else
        {
            const bool weaponSafe = modeSettings.weaponSafe;
            const float surroundU0 = 0.10f;
            const float surroundU1 = 0.90f;
            const float bottomV0 = modeSettings.bottomV0;
            const float bottomV1 = modeSettings.bottomV1;
            if (weaponSafe)
            {
                const float safeU0 = modeSettings.safeU0;
                const float safeU1 = modeSettings.safeU1;
                const float safeMid = safeU0 + (safeU1 - safeU0) * 0.5f;
                const float safeBottomV0 = modeSettings.safeBottomV0;
                const float safeBottomV1 = modeSettings.safeBottomV1;
                addPatch(-outerX, -0.5f, -outerY, -0.5f, safeU0,  safeMid, safeBottomV0, safeBottomV1);
                addPatch(-0.5f,    0.5f, -outerY, -0.5f, safeU0,  safeU1,  safeBottomV0, safeBottomV1);
                addPatch( 0.5f,   outerX, -outerY, -0.5f, safeMid, safeU1,  safeBottomV0, safeBottomV1);
            }
            else
            {
                addPatch(-outerX, -0.5f, -outerY, -0.5f, 0.0f,       surroundU0, bottomV0, bottomV1);
                addPatch(-0.5f,    0.5f, -outerY, -0.5f, surroundU0, surroundU1, bottomV0, bottomV1);
                addPatch( 0.5f,   outerX, -outerY, -0.5f, surroundU1, 1.0f,      bottomV0, bottomV1);
            }
        }
    }
    return vertices;
}

std::vector<TexturedVertex> makeEdgeSmearGamePlaneVertices()
{
    const GamePlaneMode mode = gamePlaneMode();
    const GamePlaneModeSettings modeSettings = gamePlaneModeSettings(mode);
    if (!modeSettings.sourceSurround || !modeSettings.edgeSmear)
        return {};

    float sourceCenterU0 = modeSettings.centerU0;
    float sourceCenterV0 = modeSettings.centerV0;
    float sourceCenterU1 = modeSettings.centerU1;
    float sourceCenterV1 = modeSettings.centerV1;
    if (modeSettings.atlasStitch && envEnabled("FNVXR_GAME_PLANE_SURROUND_WIDE_SOURCE_UV", false))
    {
        const float textureAspect = envFloat(
            "FNVXR_GAME_PLANE_FOV_ASPECT",
            static_cast<float>(envInt("FNVXR_GAME_TEXTURE_WIDTH", 2048))
                / static_cast<float>(std::max(1, envInt("FNVXR_GAME_TEXTURE_HEIGHT", 1280))));
        const float centerFovX = envFloat("FNVXR_GAME_PLANE_CENTER_FOV_X_DEG", envFloat("FNVXR_GAME_PLANE_CENTER_FOV_DEG", 95.0f));
        const float wideFovX =
            envFloat("FNVXR_GAME_PLANE_SURROUND_WIDE_FOV_X_DEG", envFloat("FNVXR_GAME_PLANE_SURROUND_WIDE_FOV_DEG", 115.0f));
        const float derivedCenterFovY = horizontalToVerticalFovDegrees(centerFovX, textureAspect);
        const float derivedWideFovY = horizontalToVerticalFovDegrees(wideFovX, textureAspect);
        const float centerFovY = envFloat("FNVXR_GAME_PLANE_CENTER_FOV_Y_DEG", derivedCenterFovY);
        const float wideFovY = envFloat("FNVXR_GAME_PLANE_SURROUND_WIDE_FOV_Y_DEG", derivedWideFovY);
        const float cropX = fovCenterCropRatio(centerFovX, wideFovX);
        const float cropY = fovCenterCropRatio(centerFovY, wideFovY);
        sourceCenterU0 = 0.5f - cropX * 0.5f;
        sourceCenterU1 = 0.5f + cropX * 0.5f;
        sourceCenterV0 = 0.5f - cropY * 0.5f;
        sourceCenterV1 = 0.5f + cropY * 0.5f;
    }

    const float sourceCenterWidth = std::max(0.05f, sourceCenterU1 - sourceCenterU0);
    const float sourceCenterHeight = std::max(0.05f, sourceCenterV1 - sourceCenterV0);
    const float scaleX = std::max(1.0f, modeSettings.surroundScaleX);
    const float scaleY = std::max(1.0f, modeSettings.surroundScaleY);
    const float outerLeftX = modeSettings.atlasStitch ? -0.5f - sourceCenterU0 / sourceCenterWidth : -scaleX * 0.5f;
    const float outerRightX = modeSettings.atlasStitch ? 0.5f + (1.0f - sourceCenterU1) / sourceCenterWidth : scaleX * 0.5f;
    const float outerTopY = modeSettings.atlasStitch ? 0.5f + sourceCenterV0 / sourceCenterHeight : scaleY * 0.5f;
    const float outerBottomY = modeSettings.atlasStitch ? -0.5f - (1.0f - sourceCenterV1) / sourceCenterHeight : -scaleY * 0.5f;
    const float smearX = std::clamp(modeSettings.edgeSmearWidthX, 0.02f, 0.40f);
    const float smearY = std::clamp(modeSettings.edgeSmearWidthY, 0.02f, 0.34f);
    const int segmentsX = std::clamp(modeSettings.segmentsX / 2, 8, 96);
    const int segmentsY = std::clamp(modeSettings.segmentsY / 2, 6, 72);
    const float sign = modeSettings.sign;

    auto uvAt = [&](float x, float y) -> std::array<float, 2>
    {
        const float leftSpan = std::max(0.001f, -0.5f - outerLeftX);
        const float rightSpan = std::max(0.001f, outerRightX - 0.5f);
        const float topSpan = std::max(0.001f, outerTopY - 0.5f);
        const float bottomSpan = std::max(0.001f, -0.5f - outerBottomY);

        float u = sourceCenterU0 + (std::clamp(x, -0.5f, 0.5f) + 0.5f) * sourceCenterWidth;
        if (x < -0.5f)
            u = sourceCenterU0 + ((x + 0.5f) / leftSpan) * sourceCenterU0;
        else if (x > 0.5f)
            u = sourceCenterU1 + ((x - 0.5f) / rightSpan) * (1.0f - sourceCenterU1);

        float v = sourceCenterV0 + (0.5f - std::clamp(y, -0.5f, 0.5f)) * sourceCenterHeight;
        if (y > 0.5f)
            v = sourceCenterV0 - ((y - 0.5f) / topSpan) * sourceCenterV0;
        else if (y < -0.5f)
            v = sourceCenterV1 + ((-0.5f - y) / bottomSpan) * (1.0f - sourceCenterV1);

        return { u, v };
    };

    auto vertex = [&](float x, float y, float u, float v) -> TexturedVertex
    {
        const float centerX = std::clamp(x, -0.5f, 0.5f);
        const float centerY = std::clamp(y, -0.5f, 0.5f);
        const float centerNx = centerX * 2.0f;
        const float centerNy = centerY * 2.0f;
        const float centerNx2 = centerNx * centerNx;
        const float centerNy2 = centerNy * centerNy;
        const float centerZ =
            modeSettings.centerDepthX * centerNx2
            + modeSettings.centerDepthY * centerNy2
            + modeSettings.centerCornerDepth * centerNx2 * centerNy2;

        const float maxOuterX = std::max(
            0.001f,
            std::max(std::fabs(outerLeftX - smearX), std::fabs(outerRightX + smearX)) - 0.5f);
        const float maxOuterY = std::max(
            0.001f,
            std::max(std::fabs(outerTopY + smearY), std::fabs(outerBottomY - smearY)) - 0.5f);
        const float outerBlendX = std::clamp((std::fabs(x) - 0.5f) / maxOuterX, 0.0f, 1.0f);
        const float outerBlendY = std::clamp((std::fabs(y) - 0.5f) / maxOuterY, 0.0f, 1.0f);
        const float outerBlendX2 = outerBlendX * outerBlendX;
        const float outerBlendY2 = outerBlendY * outerBlendY;
        const float extraZ =
            modeSettings.surroundDepthX * outerBlendX2
            + modeSettings.surroundDepthY * outerBlendY2
            + modeSettings.surroundCornerDepth * outerBlendX2 * outerBlendY2;
        return { x, y, sign * (centerZ + extraZ), u, v };
    };

    std::vector<TexturedVertex> vertices;
    vertices.reserve(static_cast<size_t>(segmentsX) * static_cast<size_t>(segmentsY) * 24u);

    auto addMappedPatch = [&](float x0, float x1, float y0, float y1)
    {
        if (x1 <= x0 || y1 <= y0)
            return;
        const int patchSegmentsX = std::max(1, static_cast<int>(std::ceil((x1 - x0) * static_cast<float>(segmentsX))));
        const int patchSegmentsY = std::max(1, static_cast<int>(std::ceil((y1 - y0) * static_cast<float>(segmentsY))));
        for (int y = 0; y < patchSegmentsY; ++y)
        {
            const float ty0 = static_cast<float>(y) / static_cast<float>(patchSegmentsY);
            const float ty1 = static_cast<float>(y + 1) / static_cast<float>(patchSegmentsY);
            const float py0 = y0 + ty0 * (y1 - y0);
            const float py1 = y0 + ty1 * (y1 - y0);
            for (int x = 0; x < patchSegmentsX; ++x)
            {
                const float tx0 = static_cast<float>(x) / static_cast<float>(patchSegmentsX);
                const float tx1 = static_cast<float>(x + 1) / static_cast<float>(patchSegmentsX);
                const float px0 = x0 + tx0 * (x1 - x0);
                const float px1 = x0 + tx1 * (x1 - x0);
                const std::array<float, 2> bottomLeftUv = uvAt(px0, py0);
                const std::array<float, 2> topLeftUv = uvAt(px0, py1);
                const std::array<float, 2> topRightUv = uvAt(px1, py1);
                const std::array<float, 2> bottomRightUv = uvAt(px1, py0);
                const TexturedVertex bottomLeft = vertex(px0, py0, bottomLeftUv[0], bottomLeftUv[1]);
                const TexturedVertex topLeft = vertex(px0, py1, topLeftUv[0], topLeftUv[1]);
                const TexturedVertex topRight = vertex(px1, py1, topRightUv[0], topRightUv[1]);
                const TexturedVertex bottomRight = vertex(px1, py0, bottomRightUv[0], bottomRightUv[1]);
                vertices.push_back(bottomLeft);
                vertices.push_back(topLeft);
                vertices.push_back(topRight);
                vertices.push_back(bottomLeft);
                vertices.push_back(topRight);
                vertices.push_back(bottomRight);
            }
        }
    };

    addMappedPatch(outerLeftX - smearX, outerLeftX, outerBottomY, outerTopY);
    addMappedPatch(outerRightX, outerRightX + smearX, outerBottomY, outerTopY);
    addMappedPatch(outerLeftX - smearX, outerRightX + smearX, outerTopY, outerTopY + smearY);
    if (modeSettings.drawBottom)
        addMappedPatch(outerLeftX - smearX, outerRightX + smearX, outerBottomY - smearY, outerBottomY);

    return vertices;
}

bool createRenderer(ID3D11Device* device, Renderer& renderer)
{
    const char* shaderSource = R"(
cbuffer ConstantsBuffer : register(b0)
{
    row_major float4x4 mvp;
    float4 color;
};

struct VSInput
{
    float3 position : POSITION;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.position = mul(float4(input.position, 1.0), mvp);
    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    return color;
}
)";

    ComPtr<ID3DBlob> vertexBlob;
    ComPtr<ID3DBlob> pixelBlob;
    if (!compileShader(shaderSource, "VSMain", "vs_5_0", &vertexBlob)
        || !compileShader(shaderSource, "PSMain", "ps_5_0", &pixelBlob))
        return false;

    if (FAILED(device->CreateVertexShader(vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(), nullptr, &renderer.vertexShader)))
        return false;
    if (FAILED(device->CreatePixelShader(pixelBlob->GetBufferPointer(), pixelBlob->GetBufferSize(), nullptr, &renderer.pixelShader)))
        return false;

    D3D11_INPUT_ELEMENT_DESC inputElement {
        "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0
    };
    if (FAILED(device->CreateInputLayout(&inputElement, 1, vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(), &renderer.inputLayout)))
        return false;

    const char* texturedShaderSource = R"(
cbuffer ConstantsBuffer : register(b0)
{
    row_major float4x4 mvp;
    float4 color;
};

Texture2D gameTexture : register(t0);
SamplerState gameSampler : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.position = mul(float4(input.position, 1.0), mvp);
    output.uv = input.uv;
    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    return gameTexture.Sample(gameSampler, input.uv) * color;
}

float hudKeyMask(float3 sampleColor)
{
    float redSignal = saturate((sampleColor.r - 0.32) * 4.0);
    float greenSignal = saturate((sampleColor.g - 0.18) * 4.0);
    float blueReject = saturate(1.0 - sampleColor.b * 3.25);
    float amberDominance = saturate((sampleColor.r - sampleColor.b * 1.5) * 3.5);
    return saturate(redSignal * greenSignal * blueReject * amberDominance);
}

float hudSpatialMask(float2 uv)
{
    float bottomBand = smoothstep(0.66, 0.74, uv.y);
    return saturate(bottomBand);
}

float3 hudSuppressedColor(float2 uv, float3 sampleColor)
{
    float hudMask = hudKeyMask(sampleColor) * hudSpatialMask(uv);

    uint width;
    uint height;
    gameTexture.GetDimensions(width, height);
    float2 texel = 1.0 / float2(max((float)width, 1.0), max((float)height, 1.0));
    float3 fill =
        gameTexture.Sample(gameSampler, uv + texel * float2(-18.0, -18.0)).rgb +
        gameTexture.Sample(gameSampler, uv + texel * float2( 18.0, -18.0)).rgb +
        gameTexture.Sample(gameSampler, uv + texel * float2(-26.0, -34.0)).rgb +
        gameTexture.Sample(gameSampler, uv + texel * float2( 26.0, -34.0)).rgb;
    fill *= 0.25;

    float suppress = smoothstep(0.12, 0.34, hudMask);
    return lerp(sampleColor, fill, suppress);
}

float edgeDither(float2 pixel)
{
    return frac(52.9829189 * frac(pixel.x * 0.06711056 + pixel.y * 0.00583715));
}

float4 PSCenterNoHud(VSOutput input) : SV_TARGET
{
    float4 sampleColor = gameTexture.Sample(gameSampler, input.uv);
    return float4(hudSuppressedColor(input.uv, sampleColor.rgb) * color.rgb, color.a);
}

float4 PSEdgeSmear(VSOutput input) : SV_TARGET
{
    uint width;
    uint height;
    gameTexture.GetDimensions(width, height);
    float2 texel = 1.0 / float2(max((float)width, 1.0), max((float)height, 1.0));

    float2 rawUv = input.uv;
    float2 outside = max(float2(0.0, 0.0), max(-rawUv, rawUv - 1.0));
    float edgeDistance = max(outside.x, outside.y);
    float2 inward = float2(
        rawUv.x < 0.0 ? 1.0 : (rawUv.x > 1.0 ? -1.0 : 0.0),
        rawUv.y < 0.0 ? 1.0 : (rawUv.y > 1.0 ? -1.0 : 0.0));
    float2 clampedUv = clamp(rawUv, float2(0.001, 0.001), float2(0.999, 0.999));
    float smearSteps = lerp(2.0, 24.0, saturate(edgeDistance / 0.14));
    float2 sampleUv = clamp(clampedUv + inward * texel * smearSteps, float2(0.001, 0.001), float2(0.999, 0.999));
    float3 sampleColor = gameTexture.Sample(gameSampler, sampleUv).rgb;
    sampleColor = hudSuppressedColor(sampleUv, sampleColor);

    float fade = 1.0 - smoothstep(0.0, 0.14, edgeDistance);
    float grain = edgeDither(input.position.xy);
    float ditheredFade = saturate(fade - (grain - 0.5) * 0.18);
    float brightness = lerp(0.78, 0.42, saturate(edgeDistance / 0.14));
    return float4(sampleColor * color.rgb * brightness, color.a * ditheredFade);
}

float4 PSHudOverlay(VSOutput input) : SV_TARGET
{
    float4 sampleColor = gameTexture.Sample(gameSampler, input.uv);
    float mask = hudKeyMask(sampleColor.rgb) * hudSpatialMask(input.uv);
    float intensity = saturate(max(sampleColor.r, sampleColor.g) * 1.10);
    float alpha = mask * color.a;
    return float4(float3(1.0, 0.72, 0.04) * intensity * color.rgb, alpha);
}
)";

    ComPtr<ID3DBlob> texturedVertexBlob;
    ComPtr<ID3DBlob> texturedPixelBlob;
    ComPtr<ID3DBlob> centerNoHudPixelBlob;
    ComPtr<ID3DBlob> hudOverlayPixelBlob;
    ComPtr<ID3DBlob> edgeSmearPixelBlob;
    if (!compileShader(texturedShaderSource, "VSMain", "vs_5_0", &texturedVertexBlob)
        || !compileShader(texturedShaderSource, "PSMain", "ps_5_0", &texturedPixelBlob)
        || !compileShader(texturedShaderSource, "PSCenterNoHud", "ps_5_0", &centerNoHudPixelBlob)
        || !compileShader(texturedShaderSource, "PSHudOverlay", "ps_5_0", &hudOverlayPixelBlob)
        || !compileShader(texturedShaderSource, "PSEdgeSmear", "ps_5_0", &edgeSmearPixelBlob))
        return false;

    if (FAILED(device->CreateVertexShader(texturedVertexBlob->GetBufferPointer(), texturedVertexBlob->GetBufferSize(), nullptr, &renderer.texturedVertexShader)))
        return false;
    if (FAILED(device->CreatePixelShader(texturedPixelBlob->GetBufferPointer(), texturedPixelBlob->GetBufferSize(), nullptr, &renderer.texturedPixelShader)))
        return false;
    if (FAILED(device->CreatePixelShader(
            centerNoHudPixelBlob->GetBufferPointer(),
            centerNoHudPixelBlob->GetBufferSize(),
            nullptr,
            &renderer.centerNoHudPixelShader)))
    {
        return false;
    }
    if (FAILED(device->CreatePixelShader(
            hudOverlayPixelBlob->GetBufferPointer(),
            hudOverlayPixelBlob->GetBufferSize(),
            nullptr,
            &renderer.hudOverlayPixelShader)))
    {
        return false;
    }
    if (FAILED(device->CreatePixelShader(
            edgeSmearPixelBlob->GetBufferPointer(),
            edgeSmearPixelBlob->GetBufferSize(),
            nullptr,
            &renderer.edgeSmearPixelShader)))
    {
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC texturedInputElements[] {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, sizeof(float) * 3, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    if (FAILED(device->CreateInputLayout(
            texturedInputElements,
            static_cast<UINT>(std::size(texturedInputElements)),
            texturedVertexBlob->GetBufferPointer(),
            texturedVertexBlob->GetBufferSize(),
            &renderer.texturedInputLayout)))
        return false;

    const Vertex cubeVertices[] = {
        {-0.5f,-0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f},
        {-0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f},
        {-0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
        {-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f},
        {-0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f},
        {-0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f,-0.5f},
        {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f, 0.5f}, {-0.5f,-0.5f, 0.5f},
        {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f, 0.5f},
        {-0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
        {-0.5f,-0.5f,-0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f,-0.5f},
        { 0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f},
        { 0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f, 0.5f},
    };

    D3D11_BUFFER_DESC vertexDesc {};
    vertexDesc.ByteWidth = sizeof(cubeVertices);
    vertexDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vertexData {};
    vertexData.pSysMem = cubeVertices;
    if (FAILED(device->CreateBuffer(&vertexDesc, &vertexData, &renderer.vertexBuffer)))
        return false;

    const TexturedVertex gamePlaneVertices[] = {
        { -0.5f, -0.5f, 0.0f, 0.0f, 1.0f },
        { -0.5f,  0.5f, 0.0f, 0.0f, 0.0f },
        {  0.5f,  0.5f, 0.0f, 1.0f, 0.0f },
        { -0.5f, -0.5f, 0.0f, 0.0f, 1.0f },
        {  0.5f,  0.5f, 0.0f, 1.0f, 0.0f },
        {  0.5f, -0.5f, 0.0f, 1.0f, 1.0f },
    };

    D3D11_BUFFER_DESC gamePlaneDesc {};
    gamePlaneDesc.ByteWidth = sizeof(gamePlaneVertices);
    gamePlaneDesc.Usage = D3D11_USAGE_DEFAULT;
    gamePlaneDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA gamePlaneData {};
    gamePlaneData.pSysMem = gamePlaneVertices;
    if (FAILED(device->CreateBuffer(&gamePlaneDesc, &gamePlaneData, &renderer.texturedVertexBuffer)))
        return false;

    const std::vector<TexturedVertex> curvedGamePlaneVertices = makeCurvedGamePlaneVertices();
    if (!curvedGamePlaneVertices.empty())
    {
        D3D11_BUFFER_DESC curvedGamePlaneDesc {};
        curvedGamePlaneDesc.ByteWidth = static_cast<UINT>(curvedGamePlaneVertices.size() * sizeof(TexturedVertex));
        curvedGamePlaneDesc.Usage = D3D11_USAGE_DEFAULT;
        curvedGamePlaneDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA curvedGamePlaneData {};
        curvedGamePlaneData.pSysMem = curvedGamePlaneVertices.data();
        if (FAILED(device->CreateBuffer(
                &curvedGamePlaneDesc,
                &curvedGamePlaneData,
                &renderer.curvedTexturedVertexBuffer)))
        {
            return false;
        }
        renderer.curvedTexturedVertexCount = static_cast<UINT>(curvedGamePlaneVertices.size());
        std::cout << "gamePlaneCurve vertices=" << renderer.curvedTexturedVertexCount
                  << " segX=" << std::clamp(envInt("FNVXR_GAME_PLANE_CURVE_SEGMENTS_X", 32), 1, 96)
                  << " segY=" << std::clamp(envInt("FNVXR_GAME_PLANE_CURVE_SEGMENTS_Y", 18), 1, 64)
                  << " depthX=" << envFloat("FNVXR_GAME_PLANE_CURVE_DEPTH_X", 0.22f)
                  << " depthY=" << envFloat("FNVXR_GAME_PLANE_CURVE_DEPTH_Y", 0.08f)
                  << " cornerDepth=" << envFloat("FNVXR_GAME_PLANE_CURVE_CORNER_DEPTH", 0.03f)
                  << " centerUv=(" << std::clamp(envFloat("FNVXR_GAME_PLANE_CENTER_UV_LEFT", 0.0f), 0.0f, 0.49f)
                  << "," << std::clamp(envFloat("FNVXR_GAME_PLANE_CENTER_UV_TOP", 0.0f), 0.0f, 0.49f)
                  << "," << std::clamp(envFloat("FNVXR_GAME_PLANE_CENTER_UV_RIGHT", 1.0f), 0.01f, 1.0f)
                  << "," << std::clamp(envFloat("FNVXR_GAME_PLANE_CENTER_UV_BOTTOM", 1.0f), 0.01f, 1.0f)
                  << ")"
                  << " sign=" << (envFloat("FNVXR_GAME_PLANE_CURVE_SIGN", 1.0f) < 0.0f ? -1 : 1)
                  << "\n";
    }

    const GamePlaneMode mode = gamePlaneMode();
    const GamePlaneModeSettings modeSettings = gamePlaneModeSettings(mode);
    if (modeSettings.sourceSurround && modeSettings.compositeCenter)
    {
        const std::vector<TexturedVertex> shieldCenterGamePlaneVertices = makeCurvedGamePlaneVertices(
            modeSettings.centerU0,
            modeSettings.centerV0,
            modeSettings.centerU1,
            modeSettings.centerV1);
        if (!shieldCenterGamePlaneVertices.empty())
        {
            D3D11_BUFFER_DESC shieldCenterGamePlaneDesc {};
            shieldCenterGamePlaneDesc.ByteWidth =
                static_cast<UINT>(shieldCenterGamePlaneVertices.size() * sizeof(TexturedVertex));
            shieldCenterGamePlaneDesc.Usage = D3D11_USAGE_DEFAULT;
            shieldCenterGamePlaneDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            D3D11_SUBRESOURCE_DATA shieldCenterGamePlaneData {};
            shieldCenterGamePlaneData.pSysMem = shieldCenterGamePlaneVertices.data();
            if (FAILED(device->CreateBuffer(
                    &shieldCenterGamePlaneDesc,
                    &shieldCenterGamePlaneData,
                    &renderer.shieldCenterTexturedVertexBuffer)))
            {
                return false;
            }
            renderer.shieldCenterTexturedVertexCount = static_cast<UINT>(shieldCenterGamePlaneVertices.size());
            std::cout << "gamePlaneShieldCenterCrop vertices=" << renderer.shieldCenterTexturedVertexCount
                      << " mode=" << gamePlaneModeName(mode)
                      << " centerUv=(" << modeSettings.centerU0
                      << "," << modeSettings.centerV0
                      << "," << modeSettings.centerU1
                      << "," << modeSettings.centerV1
                      << ")"
                      << "\n";
        }
    }
    if (modeSettings.hudOverlay)
    {
        std::cout << "gamePlaneHudOverlay enabled=1 mode=" << gamePlaneModeName(mode)
                  << " sourceUv=(0,0,1,1)"
                  << " key=amber-bottom-band"
                  << " output=amber"
                  << " pass=after-cropped-center"
                  << "\n";
    }

    const std::vector<TexturedVertex> surroundGamePlaneVertices = makeSurroundGamePlaneVertices();
    if (!surroundGamePlaneVertices.empty())
    {
        D3D11_BUFFER_DESC surroundGamePlaneDesc {};
        surroundGamePlaneDesc.ByteWidth = static_cast<UINT>(surroundGamePlaneVertices.size() * sizeof(TexturedVertex));
        surroundGamePlaneDesc.Usage = D3D11_USAGE_DEFAULT;
        surroundGamePlaneDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA surroundGamePlaneData {};
        surroundGamePlaneData.pSysMem = surroundGamePlaneVertices.data();
        if (FAILED(device->CreateBuffer(
                &surroundGamePlaneDesc,
                &surroundGamePlaneData,
                &renderer.surroundTexturedVertexBuffer)))
        {
            return false;
        }
        renderer.surroundTexturedVertexCount = static_cast<UINT>(surroundGamePlaneVertices.size());
        const float surroundLogAspect = envFloat(
            "FNVXR_GAME_PLANE_FOV_ASPECT",
            static_cast<float>(envInt("FNVXR_GAME_TEXTURE_WIDTH", 2048))
                / static_cast<float>(std::max(1, envInt("FNVXR_GAME_TEXTURE_HEIGHT", 1280))));
        const float surroundCenterFovX =
            envFloat("FNVXR_GAME_PLANE_CENTER_FOV_X_DEG", envFloat("FNVXR_GAME_PLANE_CENTER_FOV_DEG", 95.0f));
        const float surroundWideFovX = envFloat(
            "FNVXR_GAME_PLANE_SURROUND_WIDE_FOV_X_DEG",
            envFloat("FNVXR_GAME_PLANE_SURROUND_WIDE_FOV_DEG", 115.0f));
        const float surroundCenterFovY = envFloat(
            "FNVXR_GAME_PLANE_CENTER_FOV_Y_DEG",
            horizontalToVerticalFovDegrees(surroundCenterFovX, surroundLogAspect));
        const float surroundWideFovY = envFloat(
            "FNVXR_GAME_PLANE_SURROUND_WIDE_FOV_Y_DEG",
            horizontalToVerticalFovDegrees(surroundWideFovX, surroundLogAspect));
        const float surroundCropX = fovCenterCropRatio(surroundCenterFovX, surroundWideFovX);
        const float surroundCropY = fovCenterCropRatio(surroundCenterFovY, surroundWideFovY);
        std::cout << "gamePlaneSourceSurround vertices=" << renderer.surroundTexturedVertexCount
                  << " mode=" << gamePlaneModeName(mode)
                  << " segX=" << std::clamp(modeSettings.segmentsX, 1, 128)
                  << " segY=" << std::clamp(modeSettings.segmentsY, 1, 96)
                  << " depthX=" << modeSettings.surroundDepthX
                  << " depthY=" << modeSettings.surroundDepthY
                  << " cornerDepth=" << modeSettings.surroundCornerDepth
                  << " scale=(" << std::max(1.0f, modeSettings.surroundScaleX)
                  << "," << std::max(1.0f, modeSettings.surroundScaleY)
                  << ")"
                  << " atlasStitch=" << static_cast<int>(modeSettings.atlasStitch)
                  << " atlasCenter=(" << modeSettings.centerU0
                  << "," << modeSettings.centerV0
                  << "," << modeSettings.centerU1
                  << "," << modeSettings.centerV1
                  << ")"
                  << " atlasExpand=(1,1)"
                  << " center=" << static_cast<int>(modeSettings.drawCenter)
                  << " composite=" << static_cast<int>(modeSettings.compositeCenter)
                  << " weaponSafe=" << static_cast<int>(modeSettings.weaponSafe)
                  << " edgeStitch=" << static_cast<int>(modeSettings.edgeStitch)
                  << " wideSource=" << static_cast<int>(modeSettings.useWideSource)
                  << " requireWide=" << static_cast<int>(modeSettings.requireWideSource)
                  << " fovCrop=(" << surroundCropX
                  << "," << surroundCropY
                  << ")"
                  << " fovCenter=(" << surroundCenterFovX
                  << "," << surroundCenterFovY
                  << ")"
                  << " fovWide=(" << surroundWideFovX
                  << "," << surroundWideFovY
                  << ")"
                  << " duplicate=" << static_cast<int>(modeSettings.singleSourceDuplicate)
                  << " edgeClamp=" << static_cast<int>(modeSettings.singleSourceClamp)
                  << " inwardBands=" << static_cast<int>(modeSettings.useInwardBands)
                  << " safeBands=" << static_cast<int>(modeSettings.useSafeBands)
                  << " edgeBands=(0.16,0.14,0.10)"
                  << " transitionPower=" << modeSettings.transitionPower
                  << " safeSource=(" << modeSettings.safeU0
                  << "," << modeSettings.safeU1
                  << ")"
                  << " safeBottom=(" << modeSettings.safeBottomV0
                  << "," << modeSettings.safeBottomV1
                  << ")"
                  << " bottom=" << static_cast<int>(modeSettings.drawBottom)
                  << " bottomSource=(" << modeSettings.bottomV0
                  << "," << modeSettings.bottomV1
                  << ")"
                  << " centerUv=(" << modeSettings.centerU0
                  << "," << modeSettings.centerV0
                  << "," << modeSettings.centerU1
                  << "," << modeSettings.centerV1
                  << ")"
                  << "\n";
    }
    const std::vector<TexturedVertex> edgeSmearGamePlaneVertices = makeEdgeSmearGamePlaneVertices();
    if (!edgeSmearGamePlaneVertices.empty())
    {
        D3D11_BUFFER_DESC edgeSmearGamePlaneDesc {};
        edgeSmearGamePlaneDesc.ByteWidth =
            static_cast<UINT>(edgeSmearGamePlaneVertices.size() * sizeof(TexturedVertex));
        edgeSmearGamePlaneDesc.Usage = D3D11_USAGE_DEFAULT;
        edgeSmearGamePlaneDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA edgeSmearGamePlaneData {};
        edgeSmearGamePlaneData.pSysMem = edgeSmearGamePlaneVertices.data();
        if (FAILED(device->CreateBuffer(
                &edgeSmearGamePlaneDesc,
                &edgeSmearGamePlaneData,
                &renderer.edgeSmearTexturedVertexBuffer)))
        {
            return false;
        }
        renderer.edgeSmearTexturedVertexCount = static_cast<UINT>(edgeSmearGamePlaneVertices.size());
        std::cout << "gamePlaneEdgeSmear vertices=" << renderer.edgeSmearTexturedVertexCount
                  << " mode=" << gamePlaneModeName(mode)
                  << " width=(" << modeSettings.edgeSmearWidthX
                  << "," << modeSettings.edgeSmearWidthY
                  << ")"
                  << " opacity=" << modeSettings.edgeSmearOpacity
                  << " shader=PSEdgeSmear"
                  << "\n";
    }

    D3D11_BUFFER_DESC constantDesc {};
    constantDesc.ByteWidth = sizeof(Constants);
    constantDesc.Usage = D3D11_USAGE_DEFAULT;
    constantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(device->CreateBuffer(&constantDesc, nullptr, &renderer.constantBuffer)))
        return false;

    D3D11_TEXTURE2D_DESC gameTextureDesc {};
    renderer.gameTextureWidth = envInt("FNVXR_GAME_TEXTURE_WIDTH", 2048);
    renderer.gameTextureHeight = envInt("FNVXR_GAME_TEXTURE_HEIGHT", 1280);
    renderer.gameTextureAspect =
        static_cast<float>(renderer.gameTextureWidth) / static_cast<float>(renderer.gameTextureHeight);
    gameTextureDesc.Width = static_cast<UINT>(renderer.gameTextureWidth);
    gameTextureDesc.Height = static_cast<UINT>(renderer.gameTextureHeight);
    gameTextureDesc.MipLevels = 1;
    gameTextureDesc.ArraySize = 1;
    gameTextureDesc.Format = DXGI_FORMAT_B8G8R8A8_TYPELESS;
    gameTextureDesc.SampleDesc.Count = 1;
    gameTextureDesc.Usage = D3D11_USAGE_DEFAULT;
    gameTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device->CreateTexture2D(&gameTextureDesc, nullptr, &renderer.gameTexture)))
        return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC gameTextureViewDesc {};
    gameTextureViewDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    gameTextureViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    gameTextureViewDesc.Texture2D.MipLevels = 1;
    if (FAILED(device->CreateShaderResourceView(renderer.gameTexture.Get(), &gameTextureViewDesc, &renderer.gameTextureView)))
        return false;

    if (FAILED(device->CreateTexture2D(&gameTextureDesc, nullptr, &renderer.worldTexture)))
        return false;
    if (FAILED(device->CreateShaderResourceView(renderer.worldTexture.Get(), &gameTextureViewDesc, &renderer.worldTextureView)))
        return false;

    for (size_t eye = 0; eye < renderer.stereoGameTextures.size(); ++eye)
    {
        if (FAILED(device->CreateTexture2D(&gameTextureDesc, nullptr, &renderer.stereoGameTextures[eye])))
            return false;
        if (FAILED(device->CreateShaderResourceView(
                renderer.stereoGameTextures[eye].Get(),
                &gameTextureViewDesc,
                &renderer.stereoGameTextureViews[eye])))
        {
            return false;
        }
    }

    D3D11_SAMPLER_DESC samplerDesc {};
    samplerDesc.Filter = envEnabled("FNVXR_GAME_PLANE_SHARP_FILTER", true)
        ? D3D11_FILTER_MIN_MAG_MIP_POINT
        : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(device->CreateSamplerState(&samplerDesc, &renderer.gameSampler)))
        return false;

    D3D11_BLEND_DESC alphaBlendDesc {};
    alphaBlendDesc.RenderTarget[0].BlendEnable = TRUE;
    alphaBlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    alphaBlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    alphaBlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    alphaBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    alphaBlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    alphaBlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    alphaBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(device->CreateBlendState(&alphaBlendDesc, &renderer.alphaBlendState)))
        return false;

    D3D11_RASTERIZER_DESC rasterizerDesc {};
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_NONE;
    rasterizerDesc.DepthClipEnable = TRUE;
    if (FAILED(device->CreateRasterizerState(&rasterizerDesc, &renderer.rasterizerState)))
        return false;

    D3D11_DEPTH_STENCIL_DESC depthDesc {};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    if (FAILED(device->CreateDepthStencilState(&depthDesc, &renderer.depthState)))
        return false;

    D3D11_DEPTH_STENCIL_DESC gamePlaneDepthDesc {};
    gamePlaneDepthDesc.DepthEnable = FALSE;
    gamePlaneDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    gamePlaneDepthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    if (FAILED(device->CreateDepthStencilState(&gamePlaneDepthDesc, &renderer.gamePlaneDepthState)))
        return false;

    return true;
}

bool initPauseSceneTexture(ID3D11Device* device, Renderer& renderer)
{
    std::vector<std::wstring> imagePaths;
    const char* envPath = std::getenv("FNVXR_MENU_SCENE_PORTAL_IMAGE");
    if (envPath && *envPath)
        imagePaths.push_back(widenPath(envPath));
    else if (std::vector<std::wstring> cachePaths = sceneCacheImagePaths(); !cachePaths.empty())
        imagePaths = cachePaths;
    else if (envEnabled("FNVXR_ALLOW_DEBUG_SCENE_SHELL", false))
        imagePaths = defaultPauseSceneImagePaths();

    if (imagePaths.empty())
    {
        std::cerr << "pauseScenePortal image=none loaded=0 debugShell="
                  << static_cast<int>(envEnabled("FNVXR_ALLOW_DEBUG_SCENE_SHELL", false)) << "\n";
        return false;
    }

    for (const std::wstring& imagePath : imagePaths)
    {
        if (renderer.pauseSceneTextureCount >= renderer.pauseSceneTextureViews.size())
            break;

        ComPtr<ID3D11ShaderResourceView> textureView;
        int width = 0;
        int height = 0;
        if (!loadTextureFromFile(device, imagePath, textureView, width, height))
        {
            std::wcerr << L"pauseSceneShell image=" << imagePath << L" loaded=0\n";
            continue;
        }

        const size_t index = renderer.pauseSceneTextureCount++;
        renderer.pauseSceneTextureViews[index] = textureView;
        renderer.pauseSceneTextureAspects[index] =
            height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 16.0f / 9.0f;
        if (!renderer.pauseSceneTextureView)
        {
            renderer.pauseSceneTextureView = textureView;
            renderer.pauseSceneTextureWidth = width;
            renderer.pauseSceneTextureHeight = height;
            renderer.pauseSceneTextureAspect = renderer.pauseSceneTextureAspects[index];
        }
        std::wcerr << L"pauseSceneShell image=" << imagePath << L" loaded=1 index="
                   << index << L" width=" << width << L" height=" << height << L"\n";
    }

    renderer.hasPauseSceneTexture = renderer.pauseSceneTextureCount > 0;
    std::cerr << "pauseSceneShell loaded=" << static_cast<int>(renderer.hasPauseSceneTexture)
              << " count=" << renderer.pauseSceneTextureCount << "\n";
    return renderer.hasPauseSceneTexture;
}

XrPosef identityPose()
{
    XrPosef pose {};
    pose.orientation.w = 1.0f;
    return pose;
}

fnvxr::Quat toQuat(const XrQuaternionf& orientation)
{
    return { orientation.x, orientation.y, orientation.z, orientation.w };
}

fnvxr::Vec3 toVec3(const XrVector3f& position)
{
    return { position.x, position.y, position.z };
}

bool locate(OpenXr& xr, XrSpace space, XrSpace baseSpace, XrTime time, fnvxr::Quat& rotation, fnvxr::Vec3& position)
{
    XrSpaceLocation location { XR_TYPE_SPACE_LOCATION };
    if (xr.locateSpace(space, baseSpace, time, &location) != XR_SUCCESS)
        return false;

    const XrSpaceLocationFlags required =
        XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
        | XR_SPACE_LOCATION_POSITION_VALID_BIT
        | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT
        | XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
    if ((location.locationFlags & required) != required)
        return false;

    rotation = toQuat(location.pose.orientation);
    position = toVec3(location.pose.position);
    return true;
}

bool locatePose(OpenXr& xr, XrSpace space, XrSpace baseSpace, XrTime time, XrPosef& pose)
{
    XrSpaceLocation location { XR_TYPE_SPACE_LOCATION };
    if (xr.locateSpace(space, baseSpace, time, &location) != XR_SUCCESS)
        return false;

    const XrSpaceLocationFlags required =
        XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
        | XR_SPACE_LOCATION_POSITION_VALID_BIT
        | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT
        | XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
    if ((location.locationFlags & required) != required)
        return false;

    pose = location.pose;
    return true;
}

bool pollUntilSessionReady(OpenXr& xr, XrInstance instance, XrSession session)
{
    const int timeoutSeconds = envInt("FNVXR_SESSION_READY_TIMEOUT_SECONDS", 0);
    const auto started = std::chrono::steady_clock::now();
    auto nextHeartbeat = std::chrono::steady_clock::now() + std::chrono::seconds(5);

    while (timeoutSeconds <= 0
        || std::chrono::steady_clock::now() < started + std::chrono::seconds(timeoutSeconds))
    {
        XrEventDataBuffer event { XR_TYPE_EVENT_DATA_BUFFER };
        while (xr.pollEvent(instance, &event) == XR_SUCCESS)
        {
            if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)
            {
                auto* changed = reinterpret_cast<XrEventDataSessionStateChanged*>(&event);
                std::cout << "session state=" << changed->state << "\n";
                if (changed->state == XR_SESSION_STATE_READY)
                {
                    XrSessionBeginInfo beginInfo { XR_TYPE_SESSION_BEGIN_INFO };
                    beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    XrResult result = xr.beginSession(session, &beginInfo);
                    std::cout << "xrBeginSession: " << resultName(result) << "\n";
                    return result == XR_SUCCESS;
                }
                if (changed->state == XR_SESSION_STATE_EXITING || changed->state == XR_SESSION_STATE_LOSS_PENDING)
                    return false;
            }

            event = { XR_TYPE_EVENT_DATA_BUFFER };
        }

        if (std::chrono::steady_clock::now() >= nextHeartbeat)
        {
            std::cout << "waiting for XR_SESSION_STATE_READY\n";
            nextHeartbeat = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    std::cerr << "timed out waiting for XR_SESSION_STATE_READY after " << timeoutSeconds << " seconds\n";
    return false;
}

void drainOpenXrEvents(
    OpenXr& xr,
    XrInstance instance,
    XrSession session,
    bool& sessionBegun,
    bool& shouldSyncFrames,
    bool& shouldReadInput,
    bool& exitRequested,
    bool& lossPending,
    bool& referenceSpaceChangePending,
    XrTime& referenceSpaceChangeTime)
{
    XrEventDataBuffer event { XR_TYPE_EVENT_DATA_BUFFER };
    while (xr.pollEvent(instance, &event) == XR_SUCCESS)
    {
        if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)
        {
            auto* changed = reinterpret_cast<XrEventDataSessionStateChanged*>(&event);
            std::cout << "session state=" << changed->state << "\n";
            if (changed->state == XR_SESSION_STATE_READY && !sessionBegun)
            {
                XrSessionBeginInfo beginInfo { XR_TYPE_SESSION_BEGIN_INFO };
                beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                XrResult result = xr.beginSession(session, &beginInfo);
                std::cout << "xrBeginSession: " << resultName(result) << "\n";
                sessionBegun = result == XR_SUCCESS;
                shouldSyncFrames = sessionBegun;
                shouldReadInput = false;
            }
            else if (changed->state == XR_SESSION_STATE_SYNCHRONIZED
                || changed->state == XR_SESSION_STATE_VISIBLE)
            {
                shouldSyncFrames = sessionBegun;
                shouldReadInput = false;
            }
            else if (changed->state == XR_SESSION_STATE_FOCUSED)
            {
                shouldSyncFrames = sessionBegun;
                shouldReadInput = sessionBegun;
            }
            else if (changed->state == XR_SESSION_STATE_STOPPING)
            {
                shouldSyncFrames = false;
                shouldReadInput = false;
                if (sessionBegun)
                {
                    XrResult result = xr.endSession(session);
                    std::cout << "xrEndSession: " << resultName(result) << "\n";
                    sessionBegun = false;
                }
            }
            else if (changed->state == XR_SESSION_STATE_IDLE)
            {
                shouldSyncFrames = false;
                shouldReadInput = false;
            }
            else if (changed->state == XR_SESSION_STATE_EXITING || changed->state == XR_SESSION_STATE_LOSS_PENDING)
            {
                exitRequested = true;
                lossPending = changed->state == XR_SESSION_STATE_LOSS_PENDING;
                shouldSyncFrames = false;
                shouldReadInput = false;
            }
        }
        else if (event.type == XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING)
        {
            const auto* changed = reinterpret_cast<const XrEventDataReferenceSpaceChangePending*>(&event);
            const bool affectsPublishedBase = changed->referenceSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL;
            if (affectsPublishedBase)
            {
                referenceSpaceChangePending = true;
                referenceSpaceChangeTime = changed->changeTime;
            }
            std::cout << "referenceSpaceChangePending affectsPublishedBase="
                      << static_cast<int>(affectsPublishedBase)
                      << " type=" << static_cast<int>(changed->referenceSpaceType)
                      << " changeTime=" << changed->changeTime
                      << " poseValid=" << static_cast<int>(changed->poseValid)
                      << " previousSpacePos=("
                      << changed->poseInPreviousSpace.position.x << ","
                      << changed->poseInPreviousSpace.position.y << ","
                      << changed->poseInPreviousSpace.position.z << ")\n";
        }

        event = { XR_TYPE_EVENT_DATA_BUFFER };
    }
}

XrPath path(OpenXr& xr, XrInstance instance, const char* text)
{
    XrPath out = XR_NULL_PATH;
    xr.stringToPath(instance, text, &out);
    return out;
}

std::string pathText(OpenXr& xr, XrInstance instance, XrPath xrPath)
{
    if (xrPath == XR_NULL_PATH)
        return "<none>";

    uint32_t written = 0;
    char buffer[XR_MAX_PATH_LENGTH] {};
    if (xr.pathToString(instance, xrPath, static_cast<uint32_t>(std::size(buffer)), &written, buffer) != XR_SUCCESS)
        return "<path-to-string-failed>";

    return buffer;
}

XrPath currentInteractionProfile(OpenXr& xr, XrSession session, XrPath topLevelUserPath)
{
    XrInteractionProfileState state { XR_TYPE_INTERACTION_PROFILE_STATE };
    if (xr.getCurrentInteractionProfile(session, topLevelUserPath, &state) != XR_SUCCESS)
        return XR_NULL_PATH;

    return state.interactionProfile;
}

bool createAction(OpenXr& xr, XrActionSet actionSet, XrActionType type, const char* name, const char* localizedName, XrAction& action)
{
    XrActionCreateInfo info { XR_TYPE_ACTION_CREATE_INFO };
    info.actionType = type;
    strcpy_s(info.actionName, name);
    strcpy_s(info.localizedActionName, localizedName);

    const XrResult result = xr.createAction(actionSet, &info, &action);
    std::cout << "xrCreateAction(" << name << "): " << resultName(result) << "\n";
    return result == XR_SUCCESS;
}

FloatActionSample getFloatAction(OpenXr& xr, XrSession session, XrAction action)
{
    if (action == XR_NULL_HANDLE)
        return {};

    XrActionStateGetInfo getInfo { XR_TYPE_ACTION_STATE_GET_INFO };
    getInfo.action = action;

    XrActionStateFloat state { XR_TYPE_ACTION_STATE_FLOAT };
    if (xr.getActionStateFloat(session, &getInfo, &state) != XR_SUCCESS || !state.isActive)
        return {};

    FloatActionSample sample {};
    sample.active = true;
    if (state.currentState < 0.0f)
        sample.value = 0.0f;
    else if (state.currentState > 1.0f)
        sample.value = 1.0f;
    else
        sample.value = state.currentState;
    return sample;
}

BooleanActionSample getBooleanAction(OpenXr& xr, XrSession session, XrAction action)
{
    if (action == XR_NULL_HANDLE)
        return {};

    XrActionStateGetInfo getInfo { XR_TYPE_ACTION_STATE_GET_INFO };
    getInfo.action = action;

    XrActionStateBoolean state { XR_TYPE_ACTION_STATE_BOOLEAN };
    if (xr.getActionStateBoolean(session, &getInfo, &state) != XR_SUCCESS || !state.isActive)
        return {};

    return { state.currentState == XR_TRUE, true };
}

Vector2ActionSample getVector2Action(OpenXr& xr, XrSession session, XrAction action)
{
    if (action == XR_NULL_HANDLE)
        return {};

    XrActionStateGetInfo getInfo { XR_TYPE_ACTION_STATE_GET_INFO };
    getInfo.action = action;

    XrActionStateVector2f state { XR_TYPE_ACTION_STATE_VECTOR2F };
    if (xr.getActionStateVector2f(session, &getInfo, &state) != XR_SUCCESS || !state.isActive)
        return {};

    Vector2ActionSample sample {};
    sample.x = state.currentState.x;
    sample.y = state.currentState.y;
    sample.active = true;
    return sample;
}

bool getPoseActionActive(OpenXr& xr, XrSession session, XrAction action, XrPath subactionPath)
{
    if (action == XR_NULL_HANDLE || subactionPath == XR_NULL_PATH)
        return false;

    XrActionStateGetInfo getInfo { XR_TYPE_ACTION_STATE_GET_INFO };
    getInfo.action = action;
    getInfo.subactionPath = subactionPath;

    XrActionStatePose state { XR_TYPE_ACTION_STATE_POSE };
    return xr.getActionStatePose(session, &getInfo, &state) == XR_SUCCESS && state.isActive;
}

struct ComApartment
{
    HRESULT result = E_FAIL;
    bool uninitialize = false;

    explicit ComApartment(DWORD model)
    {
        result = CoInitializeEx(nullptr, model);
        uninitialize = SUCCEEDED(result);
    }

    ~ComApartment()
    {
        if (uninitialize)
            CoUninitialize();
    }
};
}

int main(int argc, char** argv)
{
    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);
    enableDpiAwareness();
    ComApartment comApartment(COINIT_MULTITHREADED);
    if (FAILED(comApartment.result) && comApartment.result != RPC_E_CHANGED_MODE)
    {
        std::cerr << "CoInitializeEx failed hr=0x" << std::hex << comApartment.result << std::dec << "\n";
    }

    uint64_t targetFrames = 0;
    if (argc >= 2)
    {
        const unsigned long long parsed = std::strtoull(argv[1], nullptr, 10);
        targetFrames = static_cast<uint64_t>(parsed);
    }

    OpenXr xr {};
    if (!loadGlobalFunctions(xr))
        return 1;

    if (!extensionAvailable(xr, XR_KHR_D3D11_ENABLE_EXTENSION_NAME))
    {
        std::cerr << "runtime does not expose " << XR_KHR_D3D11_ENABLE_EXTENSION_NAME << "\n";
        return 2;
    }

    std::vector<const char*> enabledExtensions;
    enabledExtensions.push_back(XR_KHR_D3D11_ENABLE_EXTENSION_NAME);

    for (const char* optionalExtension : { "XR_META_touch_controller_plus", "XR_FB_touch_controller_pro" })
    {
        if (extensionAvailable(xr, optionalExtension))
        {
            enabledExtensions.push_back(optionalExtension);
            std::cout << "enabling OpenXR extension: " << optionalExtension << "\n";
        }
    }

    XrInstanceCreateInfo instanceInfo { XR_TYPE_INSTANCE_CREATE_INFO };
    strcpy_s(instanceInfo.applicationInfo.applicationName, "FNVXR Pose Host");
    instanceInfo.applicationInfo.applicationVersion = 1;
    strcpy_s(instanceInfo.applicationInfo.engineName, "fnvxr-bridge-experiment");
    instanceInfo.applicationInfo.engineVersion = 1;
    instanceInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    instanceInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
    instanceInfo.enabledExtensionNames = enabledExtensions.data();

    XrInstance instance = XR_NULL_HANDLE;
    XrResult result = xr.createInstance(&instanceInfo, &instance);
    std::cout << "xrCreateInstance: " << resultName(result) << "\n";
    if (result != XR_SUCCESS)
        return 3;

    if (!loadInstanceFunctions(xr, instance))
        return 4;

    XrSystemGetInfo systemInfo { XR_TYPE_SYSTEM_GET_INFO };
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId systemId = XR_NULL_SYSTEM_ID;
    if (!waitForHmdSystem(xr, instance, systemInfo, systemId))
        return 5;

    XrGraphicsRequirementsD3D11KHR graphicsRequirements { XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR };
    result = xr.getD3D11GraphicsRequirementsKHR(instance, systemId, &graphicsRequirements);
    std::cout << "xrGetD3D11GraphicsRequirementsKHR: " << resultName(result) << "\n";
    if (result != XR_SUCCESS)
        return 6;

    ComPtr<ID3D11Device> device;
    if (!createD3D11DeviceForRuntime(graphicsRequirements, &device))
        return 7;

    Renderer renderer {};
    if (!createRenderer(device.Get(), renderer))
    {
        std::cerr << "failed to create stereo prop renderer\n";
        return 8;
    }
    initPauseSceneTexture(device.Get(), renderer);
    loadSceneCacheIntoStereoTextures(device.Get(), renderer);

    XrGraphicsBindingD3D11KHR graphicsBinding { XR_TYPE_GRAPHICS_BINDING_D3D11_KHR };
    graphicsBinding.device = device.Get();

    XrSessionCreateInfo sessionInfo { XR_TYPE_SESSION_CREATE_INFO };
    sessionInfo.next = &graphicsBinding;
    sessionInfo.systemId = systemId;

    XrSession session = XR_NULL_HANDLE;
    result = xr.createSession(instance, &sessionInfo, &session);
    std::cout << "xrCreateSession: " << resultName(result) << "\n";
    if (result != XR_SUCCESS)
        return 9;

    const int64_t swapchainFormat = chooseSwapchainFormat(xr, session);
    uint32_t viewConfigCount = 0;
    result = xr.enumerateViewConfigurationViews(
        instance,
        systemId,
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
        0,
        &viewConfigCount,
        nullptr);
    if (result != XR_SUCCESS || viewConfigCount < 2)
    {
        std::cerr << "xrEnumerateViewConfigurationViews failed or returned too few views\n";
        return 10;
    }

    std::vector<XrViewConfigurationView> viewConfigs(viewConfigCount);
    for (auto& viewConfig : viewConfigs)
        viewConfig.type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
    xr.enumerateViewConfigurationViews(
        instance,
        systemId,
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
        viewConfigCount,
        &viewConfigCount,
        viewConfigs.data());

    std::cout << "OpenXR recommended per-eye render sizes"
              << " left=" << viewConfigs[0].recommendedImageRectWidth
              << "x" << viewConfigs[0].recommendedImageRectHeight
              << " samples=" << viewConfigs[0].recommendedSwapchainSampleCount
              << " right=" << viewConfigs[1].recommendedImageRectWidth
              << "x" << viewConfigs[1].recommendedImageRectHeight
              << " samples=" << viewConfigs[1].recommendedSwapchainSampleCount
              << "\n";

    QuadSwapchain leftEye {};
    QuadSwapchain rightEye {};
    if (!createQuadSwapchain(
            xr,
            session,
            swapchainFormat,
            static_cast<int32_t>(viewConfigs[0].recommendedImageRectWidth),
            static_cast<int32_t>(viewConfigs[0].recommendedImageRectHeight),
            leftEye)
        || !createQuadSwapchain(
            xr,
            session,
            swapchainFormat,
            static_cast<int32_t>(viewConfigs[1].recommendedImageRectWidth),
            static_cast<int32_t>(viewConfigs[1].recommendedImageRectHeight),
            rightEye))
    {
        return 11;
    }
    if (!createSwapchainRenderTargets(device.Get(), leftEye)
        || !createSwapchainRenderTargets(device.Get(), rightEye))
    {
        return 12;
    }

    XrReferenceSpaceCreateInfo localSpaceInfo { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    localSpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    localSpaceInfo.poseInReferenceSpace = identityPose();

    XrSpace localSpace = XR_NULL_HANDLE;
    result = xr.createReferenceSpace(session, &localSpaceInfo, &localSpace);
    std::cout << "xrCreateReferenceSpace(LOCAL): " << resultName(result) << "\n";
    if (result != XR_SUCCESS)
        return 13;

    XrReferenceSpaceCreateInfo viewSpaceInfo { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    viewSpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    viewSpaceInfo.poseInReferenceSpace = identityPose();

    XrSpace viewSpace = XR_NULL_HANDLE;
    result = xr.createReferenceSpace(session, &viewSpaceInfo, &viewSpace);
    std::cout << "xrCreateReferenceSpace(VIEW): " << resultName(result) << "\n";
    if (result != XR_SUCCESS)
        return 14;

    XrPath hands[] = {
        path(xr, instance, "/user/hand/left"),
        path(xr, instance, "/user/hand/right")
    };

    XrActionSetCreateInfo actionSetInfo { XR_TYPE_ACTION_SET_CREATE_INFO };
    strcpy_s(actionSetInfo.actionSetName, "fnvxr");
    strcpy_s(actionSetInfo.localizedActionSetName, "FNVXR");
    actionSetInfo.priority = 0;

    XrActionSet actionSet = XR_NULL_HANDLE;
    result = xr.createActionSet(instance, &actionSetInfo, &actionSet);
    std::cout << "xrCreateActionSet: " << resultName(result) << "\n";
    if (result != XR_SUCCESS)
        return 15;

    XrInputActions actions {};

    XrActionCreateInfo poseActionInfo { XR_TYPE_ACTION_CREATE_INFO };
    poseActionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
    strcpy_s(poseActionInfo.actionName, "hand_pose");
    strcpy_s(poseActionInfo.localizedActionName, "Hand Pose");
    poseActionInfo.countSubactionPaths = 2;
    poseActionInfo.subactionPaths = hands;

    result = xr.createAction(actionSet, &poseActionInfo, &actions.handPose);
    std::cout << "xrCreateAction(hand_pose): " << resultName(result) << "\n";
    if (result != XR_SUCCESS)
        return 16;

    XrActionCreateInfo aimPoseActionInfo = poseActionInfo;
    strcpy_s(aimPoseActionInfo.actionName, "aim_pose");
    strcpy_s(aimPoseActionInfo.localizedActionName, "Aim Pose");
    result = xr.createAction(actionSet, &aimPoseActionInfo, &actions.aimPose);
    std::cout << "xrCreateAction(aim_pose): " << resultName(result) << "\n";
    if (result != XR_SUCCESS)
        return 16;

    if (!createAction(xr, actionSet, XR_ACTION_TYPE_FLOAT_INPUT, "left_trigger", "Left Trigger", actions.leftTrigger)
        || !createAction(xr, actionSet, XR_ACTION_TYPE_FLOAT_INPUT, "right_trigger", "Right Trigger", actions.rightTrigger)
        || !createAction(xr, actionSet, XR_ACTION_TYPE_FLOAT_INPUT, "left_squeeze", "Left Squeeze", actions.leftSqueeze)
        || !createAction(xr, actionSet, XR_ACTION_TYPE_FLOAT_INPUT, "right_squeeze", "Right Squeeze", actions.rightSqueeze)
        || !createAction(xr, actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_a", "Button A", actions.buttonA)
        || !createAction(xr, actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_b", "Button B", actions.buttonB)
        || !createAction(xr, actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_x", "Button X", actions.buttonX)
        || !createAction(xr, actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_y", "Button Y", actions.buttonY)
        || !createAction(xr, actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "left_menu", "Left Menu", actions.leftMenu)
        || !createAction(xr, actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "right_menu", "Right Menu", actions.rightMenu)
        || !createAction(xr, actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "left_thumbstick", "Left Thumbstick", actions.leftThumbstick)
        || !createAction(xr, actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "right_thumbstick", "Right Thumbstick", actions.rightThumbstick)
        || !createAction(xr, actionSet, XR_ACTION_TYPE_VECTOR2F_INPUT, "left_thumbstick_axis", "Left Thumbstick Axis", actions.leftThumbstickAxis)
        || !createAction(xr, actionSet, XR_ACTION_TYPE_VECTOR2F_INPUT, "right_thumbstick_axis", "Right Thumbstick Axis", actions.rightThumbstickAxis))
    {
        return 16;
    }

    XrActionSuggestedBinding bindings[] = {
        { actions.handPose, path(xr, instance, "/user/hand/left/input/grip/pose") },
        { actions.handPose, path(xr, instance, "/user/hand/right/input/grip/pose") },
        { actions.aimPose, path(xr, instance, "/user/hand/left/input/aim/pose") },
        { actions.aimPose, path(xr, instance, "/user/hand/right/input/aim/pose") },
        { actions.leftTrigger, path(xr, instance, "/user/hand/left/input/trigger/value") },
        { actions.rightTrigger, path(xr, instance, "/user/hand/right/input/trigger/value") },
        { actions.leftSqueeze, path(xr, instance, "/user/hand/left/input/squeeze/value") },
        { actions.rightSqueeze, path(xr, instance, "/user/hand/right/input/squeeze/value") },
        { actions.buttonA, path(xr, instance, "/user/hand/right/input/a/click") },
        { actions.buttonB, path(xr, instance, "/user/hand/right/input/b/click") },
        { actions.buttonX, path(xr, instance, "/user/hand/left/input/x/click") },
        { actions.buttonY, path(xr, instance, "/user/hand/left/input/y/click") },
        { actions.leftMenu, path(xr, instance, "/user/hand/left/input/menu/click") },
        { actions.leftThumbstick, path(xr, instance, "/user/hand/left/input/thumbstick/click") },
        { actions.rightThumbstick, path(xr, instance, "/user/hand/right/input/thumbstick/click") },
        { actions.leftThumbstickAxis, path(xr, instance, "/user/hand/left/input/thumbstick") },
        { actions.rightThumbstickAxis, path(xr, instance, "/user/hand/right/input/thumbstick") },
    };
    XrInteractionProfileSuggestedBinding suggested { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
    suggested.countSuggestedBindings = static_cast<uint32_t>(std::size(bindings));
    suggested.suggestedBindings = bindings;

    auto suggestBindings = [&](const char* label, const char* profilePath)
    {
        suggested.interactionProfile = path(xr, instance, profilePath);
        result = xr.suggestInteractionProfileBindings(instance, &suggested);
        std::cout << "xrSuggestInteractionProfileBindings(" << label << "): " << resultName(result) << "\n";
    };

    suggestBindings("oculus touch", "/interaction_profiles/oculus/touch_controller");
    suggestBindings("meta touch plus promoted", "/interaction_profiles/meta/touch_plus_controller");
    suggestBindings("meta touch pro promoted", "/interaction_profiles/meta/touch_pro_controller");

    if (extensionAvailable(xr, "XR_META_touch_controller_plus"))
    {
        suggestBindings("meta touch plus extension", "/interaction_profiles/meta/touch_controller_plus");
    }

    if (extensionAvailable(xr, "XR_FB_touch_controller_pro"))
    {
        suggestBindings("facebook touch pro extension", "/interaction_profiles/facebook/touch_controller_pro");
    }

    XrActionSpaceCreateInfo leftSpaceInfo { XR_TYPE_ACTION_SPACE_CREATE_INFO };
    leftSpaceInfo.action = actions.handPose;
    leftSpaceInfo.subactionPath = hands[0];
    leftSpaceInfo.poseInActionSpace = identityPose();

    XrActionSpaceCreateInfo rightSpaceInfo = leftSpaceInfo;
    rightSpaceInfo.subactionPath = hands[1];

    XrSpace leftSpace = XR_NULL_HANDLE;
    XrSpace rightSpace = XR_NULL_HANDLE;
    XrSpace leftAimSpace = XR_NULL_HANDLE;
    XrSpace rightAimSpace = XR_NULL_HANDLE;
    result = xr.createActionSpace(session, &leftSpaceInfo, &leftSpace);
    std::cout << "xrCreateActionSpace(left): " << resultName(result) << "\n";
    if (result != XR_SUCCESS)
        return 17;
    result = xr.createActionSpace(session, &rightSpaceInfo, &rightSpace);
    std::cout << "xrCreateActionSpace(right): " << resultName(result) << "\n";
    if (result != XR_SUCCESS)
        return 18;

    leftSpaceInfo.action = actions.aimPose;
    rightSpaceInfo.action = actions.aimPose;
    result = xr.createActionSpace(session, &leftSpaceInfo, &leftAimSpace);
    std::cout << "xrCreateActionSpace(left aim): " << resultName(result) << "\n";
    if (result != XR_SUCCESS)
        return 17;
    result = xr.createActionSpace(session, &rightSpaceInfo, &rightAimSpace);
    std::cout << "xrCreateActionSpace(right aim): " << resultName(result) << "\n";
    if (result != XR_SUCCESS)
        return 18;

    XrSessionActionSetsAttachInfo attachInfo { XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &actionSet;
    result = xr.attachSessionActionSets(session, &attachInfo);
    std::cout << "xrAttachSessionActionSets: " << resultName(result) << "\n";
    if (result != XR_SUCCESS)
        return 19;

    HostSharedBridge sharedBridge {};
    if (!initHostSharedBridge(sharedBridge))
    {
        std::cerr << "host shared bridge init failed\n";
        closeHostSharedBridge(sharedBridge);
        closeSharedD3D9VideoFrame(renderer);
        return 21;
    }
    // This is the launcher handshake. The OpenXR instance/session/actions and
    // all cross-bitness mappings now exist, so retail may start safely even
    // when the runtime keeps an unworn headset in IDLE. Input remains neutral
    // until READY/FOCUSED and the first pose frame is published.
    std::cout << "fnvxrHostBridgeReady xrSessionCreated=1 sharedMappingsReady=1\n" << std::flush;

    if (!pollUntilSessionReady(xr, instance, session))
    {
        closeHostSharedBridge(sharedBridge);
        return 20;
    }

    const HostMode mode = hostMode();
    std::cout << "hostMode=" << hostModeName(mode) << "\n";
    const float leftEyeColor[] = { 0.005f, 0.005f, 0.007f, 1.0f };
    const float rightEyeColor[] = { 0.005f, 0.005f, 0.007f, 1.0f };
    XrPosef lastLeftPose = identityPose();
    XrPosef lastRightPose = identityPose();
    XrPosef lastLeftAimPose = identityPose();
    XrPosef lastRightAimPose = identityPose();
    XrPosef lastHmdPose = identityPose();
    lastLeftPose.position = { -0.25f, -0.25f, -0.55f };
    lastRightPose.position = { 0.25f, -0.25f, -0.55f };
    lastLeftAimPose.position = lastLeftPose.position;
    lastRightAimPose.position = lastRightPose.position;
    bool previousStereoFullscreenActive = false;
    bool previousShowGamePlane = false;
    bool previousAllowStereoFullscreen = false;
    bool previousMonoFullscreenFallback = false;
    bool previousRawStereoReady = false;
    uint64_t previousStereoStableFrameCount = UINT64_MAX;
    LONG previousStereoGameFrameSequence = 0;
    uint64_t missingStereoFrames = 0;
    auto nextGamePlaneCapture = std::chrono::steady_clock::time_point {};
    GamePlane anchoredGamePlane = gamePlaneFromHmd(lastHmdPose);
    GamePlane gamePlane = anchoredGamePlane;
    bool gamePlaneAnchored = false;
    bool gamePlaneFocusAnchorCaptured = false;
    bool previousGripRecenter = false;
    bool previousHostLeftTriggerDown = false;
    bool previousHostRightTriggerDown = false;
    bool previousHostButtonADown = false;
    bool previousHostButtonXDown = false;
    MenuPointer lastActiveMenuPointer {};
    bool hasLastActiveMenuPointer = false;
    HeadspaceLookTracker headspaceLookTracker {};
    HandspaceLookTracker handspaceLookTracker {};
    HeadspaceLookTracker gyroAimLookTracker {};
    bool previousGameUiMode = false;
    uint64_t runtimeUiStuckFrames = 0;
    uint64_t gameUiModeFrames = 0;
    uint64_t gameUiRenderFrames = 0;
    uint64_t lastSharedStereoWorldReadyFrame = 0;
    bool cachedSharedStereoWorldReady = false;
    std::uint32_t stereoCellFormId = 0;
    uint64_t stereoCellStableFrames = 0;
    uint64_t stereoCellEpochPlayerFrame = 0;
    uint64_t stereoCellLastPlayerFrame = 0;
    bool stereoCellKnown = false;
    uint64_t sharedPlayerReadMisses = 0;
    bool lastSharedPlayerWeaponOut = false;
    bool previousLoggedGameplayPlaneSize = false;
    bool previousLoggedPipBoyGripMode = false;
    bool previousLoggedPipBoyMenuMode = false;
    float previousLoggedGamePlaneWidth = -1.0f;
    float previousLoggedGamePlaneHeight = -1.0f;
    float previousLoggedGamePlaneOffsetZ = 1000.0f;
    bool previousGameplayControlsActive = false;
    uint64_t gameplayControlsWarmupUntilFrame = 0;
    XrPosef pauseSceneAnchor = pauseSceneAnchorFromHmd(lastHmdPose);
    bool hasPauseSceneAnchor = false;
    XrPath lastLeftInteractionProfile = XR_NULL_PATH;
    XrPath lastRightInteractionProfile = XR_NULL_PATH;
    bool sessionBegun = true;
    bool shouldSyncFrames = true;
    bool previousShouldSyncFrames = true;
    bool shouldReadInput = false;
    bool previousShouldReadInput = false;
    bool exitRequested = false;
    bool lossPending = false;
    std::uint32_t referenceSpaceGeneration = 1;
    bool referenceSpaceChangePending = false;
    XrTime referenceSpaceChangeTime = 0;

    for (uint64_t frameIndex = 1; targetFrames == 0 || frameIndex <= targetFrames; ++frameIndex)
    {
        drainOpenXrEvents(
            xr,
            instance,
            session,
            sessionBegun,
            shouldSyncFrames,
            shouldReadInput,
            exitRequested,
            lossPending,
            referenceSpaceChangePending,
            referenceSpaceChangeTime);
        const bool sessionSyncLost = previousShouldSyncFrames && !shouldSyncFrames;
        const bool sessionSyncRegained = !previousShouldSyncFrames && shouldSyncFrames;
        if (sessionSyncLost)
            invalidateHostSharedBridgeTracking(sharedBridge, "session-not-running");
        if (sessionSyncRegained)
        {
            ++referenceSpaceGeneration;
            if (referenceSpaceGeneration == 0)
                referenceSpaceGeneration = 1;
            referenceSpaceChangePending = false;
            referenceSpaceChangeTime = 0;
            std::cout << "sessionTrackingGenerationChanged generation="
                      << referenceSpaceGeneration << " reason=session-rebegun\n";
        }
        previousShouldSyncFrames = shouldSyncFrames;
        const bool inputFocusLost = previousShouldReadInput && !shouldReadInput;
        const bool inputFocusRegained = shouldReadInput && !previousShouldReadInput;
        if (inputFocusLost || inputFocusRegained)
        {
            hasLastActiveMenuPointer = false;
            headspaceLookTracker.hasLast = false;
            gyroAimLookTracker.hasLast = false;
            previousHostLeftTriggerDown = false;
            previousHostRightTriggerDown = false;
            previousHostButtonADown = false;
            previousHostButtonXDown = false;
            std::cout << "inputFocusTransition frame=" << frameIndex
                      << " shouldReadInput=" << static_cast<int>(shouldReadInput)
                      << " lost=" << static_cast<int>(inputFocusLost)
                      << " regained=" << static_cast<int>(inputFocusRegained)
                      << "\n";
        }
        previousShouldReadInput = shouldReadInput;
        if (exitRequested)
            break;
        if (!shouldSyncFrames)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        XrFrameWaitInfo waitInfo { XR_TYPE_FRAME_WAIT_INFO };
        XrFrameState frameState { XR_TYPE_FRAME_STATE };
        result = xr.waitFrame(session, &waitInfo, &frameState);
        if (result != XR_SUCCESS)
        {
            if (result == XR_SESSION_LOSS_PENDING)
            {
                lossPending = true;
                invalidateHostSharedBridgeTracking(sharedBridge, "xrWaitFrame-session-loss-pending");
                break;
            }
            if (result == XR_ERROR_SESSION_NOT_RUNNING)
            {
                shouldSyncFrames = false;
                shouldReadInput = false;
                previousShouldSyncFrames = false;
                invalidateHostSharedBridgeTracking(sharedBridge, "xrWaitFrame-session-not-running");
                continue;
            }
            std::cerr << "xrWaitFrame failed: " << resultName(result) << "\n";
            break;
        }

        // The pending event can arrive before the runtime's advertised
        // discontinuity. Change generations on the first pose sample at or
        // after changeTime so camera and controller consumers invalidate the
        // exact frame whose LOCAL-space coordinates changed.
        if (referenceSpaceChangePending
            && (referenceSpaceChangeTime <= 0
                || frameState.predictedDisplayTime >= referenceSpaceChangeTime))
        {
            ++referenceSpaceGeneration;
            if (referenceSpaceGeneration == 0)
                referenceSpaceGeneration = 1;
            std::cout << "referenceSpaceChangeActivated generation=" << referenceSpaceGeneration
                      << " changeTime=" << referenceSpaceChangeTime
                      << " displayTime=" << frameState.predictedDisplayTime << "\n";
            referenceSpaceChangePending = false;
            referenceSpaceChangeTime = 0;
        }

        XrFrameBeginInfo beginInfo { XR_TYPE_FRAME_BEGIN_INFO };
        result = xr.beginFrame(session, &beginInfo);
        if (result != XR_SUCCESS)
        {
            if (result == XR_SESSION_LOSS_PENDING)
            {
                lossPending = true;
                invalidateHostSharedBridgeTracking(sharedBridge, "xrBeginFrame-session-loss-pending");
                break;
            }
            if (result == XR_ERROR_SESSION_NOT_RUNNING)
            {
                shouldSyncFrames = false;
                shouldReadInput = false;
                previousShouldSyncFrames = false;
                invalidateHostSharedBridgeTracking(sharedBridge, "xrBeginFrame-session-not-running");
                continue;
            }
            std::cerr << "xrBeginFrame failed: " << resultName(result) << "\n";
            break;
        }
        const bool runtimeShouldRender = frameState.shouldRender == XR_TRUE;
        static uint64_t notRequestedFrameCount = 0;
        const bool renderWhenNotRequested = envEnabled("FNVXR_RENDER_WHEN_SHOULD_RENDER_FALSE", true);
        if (!runtimeShouldRender)
        {
            ++notRequestedFrameCount;
            if (notRequestedFrameCount <= 5 || notRequestedFrameCount % 300 == 0)
            {
                std::cout << "xrFrame shouldRender=0 count=" << notRequestedFrameCount
                          << " continuingSubmit=" << static_cast<int>(renderWhenNotRequested)
                          << "\n" << std::flush;
            }
        }
        const bool shouldRender = runtimeShouldRender || renderWhenNotRequested;

        if (shouldReadInput)
        {
            XrActiveActionSet activeActionSet { actionSet, XR_NULL_PATH };
            XrActionsSyncInfo syncInfo { XR_TYPE_ACTIONS_SYNC_INFO };
            syncInfo.countActiveActionSets = 1;
            syncInfo.activeActionSets = &activeActionSet;
            const XrResult syncResult = xr.syncActions(session, &syncInfo);
            if (syncResult == XR_SESSION_LOSS_PENDING)
            {
                lossPending = true;
                break;
            }
            if (syncResult == XR_ERROR_SESSION_NOT_RUNNING)
            {
                shouldReadInput = false;
                shouldSyncFrames = false;
            }
        }

        if (shouldReadInput)
        {
            const XrPath leftInteractionProfile = currentInteractionProfile(xr, session, hands[0]);
            const XrPath rightInteractionProfile = currentInteractionProfile(xr, session, hands[1]);
            if (leftInteractionProfile != lastLeftInteractionProfile || rightInteractionProfile != lastRightInteractionProfile)
            {
                lastLeftInteractionProfile = leftInteractionProfile;
                lastRightInteractionProfile = rightInteractionProfile;
                std::cout << "interactionProfile left=" << pathText(xr, instance, leftInteractionProfile)
                          << " right=" << pathText(xr, instance, rightInteractionProfile)
                          << "\n";
            }
        }

        fnvxr::PoseFrame frame {};
        frame.frame = frameIndex;
        frame.predictedDisplayTime = static_cast<double>(frameState.predictedDisplayTime) * 1e-9;
        locate(xr, viewSpace, localSpace, frameState.predictedDisplayTime, frame.hmdRot, frame.hmdPos);
        locate(xr, leftSpace, localSpace, frameState.predictedDisplayTime, frame.leftRot, frame.leftPos);
        locate(xr, rightSpace, localSpace, frameState.predictedDisplayTime, frame.rightRot, frame.rightPos);
        FloatActionSample leftTrigger {};
        FloatActionSample rightTrigger {};
        FloatActionSample leftGrip {};
        FloatActionSample rightGrip {};
        BooleanActionSample buttonA {};
        BooleanActionSample buttonB {};
        BooleanActionSample buttonX {};
        BooleanActionSample buttonY {};
        BooleanActionSample leftMenu {};
        BooleanActionSample rightMenu {};
        BooleanActionSample leftThumbstick {};
        BooleanActionSample rightThumbstick {};
        Vector2ActionSample leftThumbstickAxis {};
        Vector2ActionSample rightThumbstickAxis {};
        bool leftHandPoseActive = false;
        bool rightHandPoseActive = false;
        bool leftAimPoseActive = false;
        bool rightAimPoseActive = false;
        if (shouldReadInput)
        {
            leftHandPoseActive = getPoseActionActive(xr, session, actions.handPose, hands[0]);
            rightHandPoseActive = getPoseActionActive(xr, session, actions.handPose, hands[1]);
            leftAimPoseActive = getPoseActionActive(xr, session, actions.aimPose, hands[0]);
            rightAimPoseActive = getPoseActionActive(xr, session, actions.aimPose, hands[1]);
            leftTrigger = getFloatAction(xr, session, actions.leftTrigger);
            rightTrigger = getFloatAction(xr, session, actions.rightTrigger);
            leftGrip = getFloatAction(xr, session, actions.leftSqueeze);
            rightGrip = getFloatAction(xr, session, actions.rightSqueeze);
            buttonA = getBooleanAction(xr, session, actions.buttonA);
            buttonB = getBooleanAction(xr, session, actions.buttonB);
            buttonX = getBooleanAction(xr, session, actions.buttonX);
            buttonY = getBooleanAction(xr, session, actions.buttonY);
            leftMenu = getBooleanAction(xr, session, actions.leftMenu);
            rightMenu = getBooleanAction(xr, session, actions.rightMenu);
            leftThumbstick = getBooleanAction(xr, session, actions.leftThumbstick);
            rightThumbstick = getBooleanAction(xr, session, actions.rightThumbstick);
            leftThumbstickAxis = getVector2Action(xr, session, actions.leftThumbstickAxis);
            rightThumbstickAxis = getVector2Action(xr, session, actions.rightThumbstickAxis);
        }

        frame.leftTrigger = leftTrigger.value;
        frame.rightTrigger = rightTrigger.value;
        frame.leftGrip = leftGrip.value;
        frame.rightGrip = rightGrip.value;
        frame.leftThumbstickX = leftThumbstickAxis.x;
        frame.leftThumbstickY = leftThumbstickAxis.y;
        frame.rightThumbstickX = rightThumbstickAxis.x;
        frame.rightThumbstickY = rightThumbstickAxis.y;
        frame.buttons = 0;
        if (buttonA.value)
            frame.buttons |= fnvxr::ButtonA;
        if (buttonB.value)
            frame.buttons |= fnvxr::ButtonB;
        if (buttonX.value)
            frame.buttons |= fnvxr::ButtonX;
        if (buttonY.value)
            frame.buttons |= fnvxr::ButtonY;
        if (leftMenu.value)
            frame.buttons |= fnvxr::LeftMenu;
        if (rightMenu.value)
            frame.buttons |= fnvxr::RightMenu;
        if (leftThumbstick.value)
            frame.buttons |= fnvxr::LeftThumbstick;
        if (rightThumbstick.value)
            frame.buttons |= fnvxr::RightThumbstick;

        {
            static std::uint64_t lastButtonMask = UINT64_MAX;
            static unsigned loggedButtonTransitions = 0;
            if (frame.buttons != lastButtonMask || frameIndex <= 5)
            {
                lastButtonMask = frame.buttons;
                if (loggedButtonTransitions < 160 || frame.buttons != 0)
                {
                    ++loggedButtonTransitions;
                    std::cout << "buttonState frame=" << frame.frame
                              << " buttons=0x" << std::hex << frame.buttons << std::dec
                              << " A=" << static_cast<int>(buttonA.value) << "/" << static_cast<int>(buttonA.active)
                              << " B=" << static_cast<int>(buttonB.value) << "/" << static_cast<int>(buttonB.active)
                              << " X=" << static_cast<int>(buttonX.value) << "/" << static_cast<int>(buttonX.active)
                              << " Y=" << static_cast<int>(buttonY.value) << "/" << static_cast<int>(buttonY.active)
                              << " LM=" << static_cast<int>(leftMenu.value) << "/" << static_cast<int>(leftMenu.active)
                              << " RM=" << static_cast<int>(rightMenu.value) << "/" << static_cast<int>(rightMenu.active)
                              << " L3=" << static_cast<int>(leftThumbstick.value) << "/" << static_cast<int>(leftThumbstick.active)
                              << " R3=" << static_cast<int>(rightThumbstick.value) << "/" << static_cast<int>(rightThumbstick.active)
                              << "\n";
                }
            }
        }

        if (frameIndex <= 5 || frameIndex % 300 == 0)
        {
            const unsigned activeMask =
                (leftTrigger.active ? 1u << 0 : 0)
                | (rightTrigger.active ? 1u << 1 : 0)
                | (leftGrip.active ? 1u << 2 : 0)
                | (rightGrip.active ? 1u << 3 : 0)
                | (buttonA.active ? 1u << 4 : 0)
                | (buttonB.active ? 1u << 5 : 0)
                | (buttonX.active ? 1u << 6 : 0)
                | (buttonY.active ? 1u << 7 : 0)
                | (leftMenu.active ? 1u << 8 : 0)
                | (rightMenu.active ? 1u << 17 : 0)
                | (leftThumbstick.active ? 1u << 9 : 0)
                | (rightThumbstick.active ? 1u << 10 : 0)
                | (leftThumbstickAxis.active ? 1u << 11 : 0)
                | (rightThumbstickAxis.active ? 1u << 12 : 0)
                | (leftHandPoseActive ? 1u << 13 : 0)
                | (rightHandPoseActive ? 1u << 14 : 0)
                | (leftAimPoseActive ? 1u << 15 : 0)
                | (rightAimPoseActive ? 1u << 16 : 0);
            std::cout << "poseFrame=" << frame.frame
                      << " lt=" << frame.leftTrigger
                      << " rt=" << frame.rightTrigger
                      << " lg=" << frame.leftGrip
                      << " rg=" << frame.rightGrip
                      << " ls=(" << frame.leftThumbstickX << "," << frame.leftThumbstickY << ")"
                      << " rs=(" << frame.rightThumbstickX << "," << frame.rightThumbstickY << ")"
                      << " buttons=0x" << std::hex << frame.buttons << std::dec
                      << " btn(A/B/X/Y/LM/RM/L3/R3)="
                      << static_cast<int>(buttonA.value)
                      << "/" << static_cast<int>(buttonB.value)
                      << "/" << static_cast<int>(buttonX.value)
                      << "/" << static_cast<int>(buttonY.value)
                      << "/" << static_cast<int>(leftMenu.value)
                      << "/" << static_cast<int>(rightMenu.value)
                      << "/" << static_cast<int>(leftThumbstick.value)
                      << "/" << static_cast<int>(rightThumbstick.value)
                      << " activeMask=0x" << std::hex << activeMask << std::dec
                      << "\n";
        }

        XrPosef leftPose = lastLeftPose;
        XrPosef rightPose = lastRightPose;
        XrPosef leftAimPose = lastLeftAimPose;
        XrPosef rightAimPose = lastRightAimPose;
        XrPosef hmdPose = lastHmdPose;
        XrPosef rawHmdPose = lastHmdPose;
        XrPosef rawLeftPose = lastLeftPose;
        XrPosef rawRightPose = lastRightPose;
        XrPosef rawLeftAimPose = lastLeftAimPose;
        XrPosef rawRightAimPose = lastRightAimPose;
        XrPosef trackedPose {};
        const bool hmdTracked = locatePose(xr, viewSpace, localSpace, frameState.predictedDisplayTime, trackedPose);
        if (hmdTracked)
        {
            rawHmdPose = trackedPose;
            lastHmdPose = smoothPose(lastHmdPose, trackedPose, 0.45f);
        }
        const bool leftHandTracked = leftHandPoseActive
            && locatePose(xr, leftSpace, localSpace, frameState.predictedDisplayTime, trackedPose);
        if (leftHandTracked)
        {
            rawLeftPose = trackedPose;
            lastLeftPose = smoothPose(lastLeftPose, trackedPose, 0.35f);
        }
        const bool rightHandTracked = rightHandPoseActive
            && locatePose(xr, rightSpace, localSpace, frameState.predictedDisplayTime, trackedPose);
        if (rightHandTracked)
        {
            rawRightPose = trackedPose;
            lastRightPose = smoothPose(lastRightPose, trackedPose, 0.35f);
        }
        const bool leftAimTracked = leftAimPoseActive
            && locatePose(xr, leftAimSpace, localSpace, frameState.predictedDisplayTime, trackedPose);
        if (leftAimTracked)
        {
            rawLeftAimPose = trackedPose;
            lastLeftAimPose = smoothPose(lastLeftAimPose, trackedPose, 0.35f);
        }
        const bool rightAimTracked = rightAimPoseActive
            && locatePose(xr, rightAimSpace, localSpace, frameState.predictedDisplayTime, trackedPose);
        if (rightAimTracked)
        {
            rawRightAimPose = trackedPose;
            lastRightAimPose = smoothPose(lastRightAimPose, trackedPose, 0.35f);
        }
        leftPose = lastLeftPose;
        rightPose = lastRightPose;
        leftAimPose = lastLeftAimPose;
        rightAimPose = lastRightAimPose;
        hmdPose = lastHmdPose;
        HeadspaceLook headspaceLook {};
        HeadspaceLook handspaceLook {};
        HeadspaceLook gyroAimLook {};
        const BodyRig bodyRig = solveBodyRig(hmdPose, leftPose, rightPose);

        const bool recenterChord =
            (rightThumbstick.value && rightGrip.value > 0.50f)
            || (leftThumbstick.value && leftGrip.value > 0.50f);
        const bool gripRecenter = recenterChord;
        const int autoCenterFrames = envInt("FNVXR_GAME_PLANE_AUTO_CENTER_FRAMES", 0);
        const bool lockPlaneToHead = envEnabled("FNVXR_GAME_PLANE_LOCK_TO_HEAD", false);
        const bool autoCenter =
            hmdTracked
            && autoCenterFrames > 0
            && frameIndex <= static_cast<uint64_t>(autoCenterFrames);
        const bool oneShotFocusAnchor = inputFocusRegained
            && !gamePlaneFocusAnchorCaptured
            && envEnabled("FNVXR_GAME_PLANE_RECENTER_ON_FOCUS", true);
        const bool shouldCaptureGamePlane =
            lockPlaneToHead
            || autoCenter
            || oneShotFocusAnchor
            || (hmdTracked && !gamePlaneAnchored)
            || (gripRecenter && !previousGripRecenter);
        if (shouldCaptureGamePlane)
        {
            anchoredGamePlane = gamePlaneFromHmd(hmdTracked ? rawHmdPose : hmdPose);
            if (!gamePlaneAnchored || (gripRecenter && !previousGripRecenter))
            {
                std::cout << "gamePlaneAnchor captured frame=" << frameIndex
                          << " lockToHead=" << static_cast<int>(lockPlaneToHead)
                          << " autoCenter=" << static_cast<int>(autoCenter)
                          << " focusRegain=" << static_cast<int>(oneShotFocusAnchor)
                          << " recenter=" << static_cast<int>(gripRecenter && !previousGripRecenter)
                          << " x=" << anchoredGamePlane.pose.position.x
                          << " y=" << anchoredGamePlane.pose.position.y
                          << " z=" << anchoredGamePlane.pose.position.z
                          << "\n";
            }
            gamePlaneAnchored = true;
            if (oneShotFocusAnchor)
                gamePlaneFocusAnchorCaptured = true;
        }
        previousGripRecenter = gripRecenter;
        gamePlane = anchoredGamePlane;

        bool sharedStereoUiActive = true;
        bool sharedStereoWorldReady = false;
        bool runtimeUiActive = true;
        bool runtimeGameplayActive = false;
        bool runtimeCameraActive = false;
        std::uint32_t runtimeMenuBits = 0;
        const bool stereoWorldIntent = stereoWorldRuntimeEnabled();
        bool haveSharedStereoUiState = false;
        if (stereoWorldIntent)
            haveSharedStereoUiState = readSharedStereoState(renderer, sharedStereoUiActive, sharedStereoWorldReady);
        else
        {
            cachedSharedStereoWorldReady = false;
            lastSharedStereoWorldReadyFrame = 0;
        }
        if (haveSharedStereoUiState)
        {
            if (sharedStereoWorldReady)
            {
                cachedSharedStereoWorldReady = true;
                lastSharedStereoWorldReadyFrame = frame.frame;
            }
            else if (sharedStereoUiActive)
            {
                cachedSharedStereoWorldReady = false;
                lastSharedStereoWorldReadyFrame = 0;
            }
        }
        const uint64_t stereoWorldProofHoldFrames =
            static_cast<uint64_t>(std::max(1, envInt("FNVXR_STEREO_WORLD_PROOF_HOLD_FRAMES", 90)));
        const bool heldSharedStereoWorldReady =
            cachedSharedStereoWorldReady
            && lastSharedStereoWorldReadyFrame != 0
            && frame.frame >= lastSharedStereoWorldReadyFrame
            && frame.frame - lastSharedStereoWorldReadyFrame <= stereoWorldProofHoldFrames;
        const bool effectiveSharedStereoWorldReady = sharedStereoWorldReady || heldSharedStereoWorldReady;
        const bool effectiveSharedStereoUiActive =
            heldSharedStereoWorldReady ? false : sharedStereoUiActive;
        const bool haveRuntimeUiState =
            readSharedRuntimeUiActive(renderer, runtimeUiActive, runtimeGameplayActive, runtimeCameraActive, runtimeMenuBits);
        fnvxr::shared::SharedPlayerState playerSnapshot {};
        const bool havePlayerSnapshot = readSharedPlayerState(sharedBridge, playerSnapshot);
        if (havePlayerSnapshot)
            sharedPlayerReadMisses = 0;
        else
            ++sharedPlayerReadMisses;
        const uint64_t sharedPlayerReadGraceFrames = static_cast<uint64_t>(
            std::max(1, envInt("FNVXR_PLAYER_STATE_READ_GRACE_FRAMES", 4)));
        const bool transientPlayerReadMiss = !havePlayerSnapshot
            && stereoCellKnown
            && sharedPlayerReadMisses <= sharedPlayerReadGraceFrames;
        if (havePlayerSnapshot)
        {
            lastSharedPlayerWeaponOut =
                (playerSnapshot.flags & fnvxr::shared::PlayerSharedFlagWeaponOut) != 0;
        }
        else if (!transientPlayerReadMiss)
        {
            lastSharedPlayerWeaponOut = false;
        }
        const bool currentCellGameplay = havePlayerSnapshot
            && (playerSnapshot.flags & fnvxr::shared::PlayerSharedFlagGameplay) != 0;
        const bool currentCellKnown = currentCellGameplay
            && (playerSnapshot.flags & fnvxr::shared::PlayerSharedFlagCellKnown) != 0
            && playerSnapshot.currentCellFormId != 0
            && playerSnapshot.frame != 0;
        const std::uint32_t currentCellFormId = currentCellKnown
            ? playerSnapshot.currentCellFormId
            : 0;
        const bool cellChanged = currentCellKnown
            && stereoCellKnown
            && (currentCellFormId != stereoCellFormId
                || playerSnapshot.frame < stereoCellLastPlayerFrame);
        const bool cellAcquired = currentCellKnown && !stereoCellKnown;
        if (transientPlayerReadMiss)
        {
            // A sequence-guarded writer can be briefly odd while xNVSE is
            // publishing. Preserve the last proven cell without advancing its
            // stability counter; sustained loss still fails closed below.
        }
        else if (!currentCellKnown)
        {
            stereoCellKnown = false;
            stereoCellFormId = 0;
            stereoCellStableFrames = 0;
            stereoCellEpochPlayerFrame = 0;
            stereoCellLastPlayerFrame = 0;
        }
        else if (cellAcquired || cellChanged)
        {
            const std::uint32_t previousCellFormId = stereoCellFormId;
            stereoCellKnown = true;
            stereoCellFormId = currentCellFormId;
            stereoCellStableFrames = 1;
            stereoCellEpochPlayerFrame = playerSnapshot.frame;
            stereoCellLastPlayerFrame = playerSnapshot.frame;
            renderer.hasStereoGameFrame = false;
            renderer.stereoGameFrameSeparated = false;
            renderer.stereoGameFrameWorldCandidate = false;
            renderer.stereoGameFrameSequence = 0;
            renderer.stereoGamePoseSequence = 0;
            renderer.stereoGameRenderedDisplayTime = 0;
            renderer.stereoStableFrameCount = 0;
            renderer.stereoFrameMisses = 0;
            renderer.stereoTransientReadMisses = 0;
            renderer.stereoAcceptedAt = {};
            missingStereoFrames = 0;
            cachedSharedStereoWorldReady = false;
            lastSharedStereoWorldReadyFrame = 0;
            std::cout << "stereoCellEpoch frame=" << frame.frame
                      << " previous=0x" << std::hex << previousCellFormId
                      << " current=0x" << currentCellFormId << std::dec
                      << " acquired=" << static_cast<int>(cellAcquired)
                      << " changed=" << static_cast<int>(cellChanged)
                      << "; stereo handoff reset\n";
        }
        else
        {
            stereoCellLastPlayerFrame = playerSnapshot.frame;
            stereoCellStableFrames = playerSnapshot.frame >= stereoCellEpochPlayerFrame
                ? playerSnapshot.frame - stereoCellEpochPlayerFrame + 1
                : 0;
        }
        const uint64_t stereoCellStableRequired = static_cast<uint64_t>(
            std::max(1, envInt("FNVXR_STEREO_CELL_STABLE_FRAMES", 60)));
        const bool stereoCellStable = stereoCellKnown
            && stereoCellStableFrames >= stereoCellStableRequired;
        const bool requireWorldStereoBeforeGameplay =
            envEnabled("FNVXR_REQUIRE_WORLD_STEREO_BEFORE_GAMEPLAY_UI", false);
        bool gameUiMode = haveRuntimeUiState
            ? !(runtimeGameplayActive
                && (!requireWorldStereoBeforeGameplay
                    || !stereoWorldIntent
                    || effectiveSharedStereoWorldReady))
            : (!haveSharedStereoUiState || effectiveSharedStereoUiActive);
        if (haveRuntimeUiState && runtimeUiActive && !runtimeGameplayActive && runtimeCameraActive)
            ++runtimeUiStuckFrames;
        else
            runtimeUiStuckFrames = 0;
        if (envEnabled("FNVXR_RUNTIME_UI_STUCK_FORCE_WORLD", false)
            && runtimeUiStuckFrames >= static_cast<uint64_t>(std::max(1, envInt("FNVXR_RUNTIME_UI_STUCK_FRAMES", 240))))
        {
            gameUiMode = false;
        }
        if (envEnabled("FNVXR_FORCE_GAMEPLAY", false))
            gameUiMode = false;
        const float pipBoyGripThreshold = envFloat("FNVXR_PIPBOY_GRIP_THRESHOLD", 0.55f);
        const bool pipBoyGripMode = envEnabled("FNVXR_LEFT_GRIP_PIPBOY_MODE", false)
            && frame.leftGrip > pipBoyGripThreshold;
        const bool pipBoyMenuMode =
            haveRuntimeUiState
            && (runtimeMenuBits & fnvxr::shared::RuntimePipBoyMenuBit) != 0;
        const bool pipBoyPlaneMode = pipBoyGripMode || pipBoyMenuMode;
        const bool dialoguePlaneSize =
            haveRuntimeUiState
            && (runtimeMenuBits & fnvxr::shared::RuntimeDialogMenuBit) != 0
            && envEnabled("FNVXR_DIALOG_USES_GAMEPLAY_PLANE", true);
        const bool gameplayPlaneSize =
            haveRuntimeUiState
            && (runtimeGameplayActive || dialoguePlaneSize)
            && !pipBoyPlaneMode;
        const GamePlaneMode currentGamePlaneMode = gamePlaneMode();
        const GamePlaneModeSettings currentGamePlaneModeSettings = gamePlaneModeSettings(currentGamePlaneMode);
        const float menuPlaneWidth = envFloat("FNVXR_GAME_PLANE_WIDTH", 2.4f);
        gamePlane.width = gameplayPlaneSize
            ? currentGamePlaneModeSettings.gameplayWidth
            : menuPlaneWidth;
        gamePlane.height = gameplayPlaneSize
            ? currentGamePlaneModeSettings.gameplayHeight
            : envFloat("FNVXR_GAME_PLANE_HEIGHT", gamePlane.width / renderer.gameTextureAspect);
        const float menuPlaneOffsetZ = envFloat("FNVXR_GAME_PLANE_OFFSET_Z", -2.0f);
        const float activePlaneOffsetZ = gameplayPlaneSize
            ? currentGamePlaneModeSettings.gameplayOffsetZ
            : menuPlaneOffsetZ;
        const float planeOffsetDeltaZ = activePlaneOffsetZ - menuPlaneOffsetZ;
        if (std::fabs(planeOffsetDeltaZ) > 0.0001f)
        {
            const XMVECTOR planeOrientation = quat(gamePlane.pose.orientation);
            const XMVECTOR delta =
                XMVector3Rotate(XMVectorSet(0.0f, 0.0f, planeOffsetDeltaZ, 0.0f), planeOrientation);
            XMFLOAT3 deltaFloat {};
            XMStoreFloat3(&deltaFloat, delta);
            gamePlane.pose.position.x += deltaFloat.x;
            gamePlane.pose.position.y += deltaFloat.y;
            gamePlane.pose.position.z += deltaFloat.z;
        }
        if (gameplayPlaneSize != previousLoggedGameplayPlaneSize
            || pipBoyGripMode != previousLoggedPipBoyGripMode
            || pipBoyMenuMode != previousLoggedPipBoyMenuMode
            || std::fabs(gamePlane.width - previousLoggedGamePlaneWidth) > 0.001f
            || std::fabs(gamePlane.height - previousLoggedGamePlaneHeight) > 0.001f
            || std::fabs(activePlaneOffsetZ - previousLoggedGamePlaneOffsetZ) > 0.001f)
        {
            previousLoggedGameplayPlaneSize = gameplayPlaneSize;
            previousLoggedPipBoyGripMode = pipBoyGripMode;
            previousLoggedPipBoyMenuMode = pipBoyMenuMode;
            previousLoggedGamePlaneWidth = gamePlane.width;
            previousLoggedGamePlaneHeight = gamePlane.height;
            previousLoggedGamePlaneOffsetZ = activePlaneOffsetZ;
            std::cout << "gamePlaneSize frame=" << frame.frame
                      << " mode=" << gamePlaneModeName(currentGamePlaneMode)
                      << " gameplaySize=" << static_cast<int>(gameplayPlaneSize)
                      << " pipBoyGripMode=" << static_cast<int>(pipBoyGripMode)
                      << " leftGrip=" << frame.leftGrip
                      << " width=" << gamePlane.width
                      << " height=" << gamePlane.height
                      << " offsetZ=" << activePlaneOffsetZ
                      << " curve=" << static_cast<int>(envEnabled("FNVXR_GAME_PLANE_CURVE_ENABLE", true))
                      << " curveDepth=(" << envFloat("FNVXR_GAME_PLANE_CURVE_DEPTH_X", 0.22f)
                      << "," << envFloat("FNVXR_GAME_PLANE_CURVE_DEPTH_Y", 0.08f)
                      << "," << envFloat("FNVXR_GAME_PLANE_CURVE_CORNER_DEPTH", 0.03f)
                      << ")"
                      << " sourceSurround=" << static_cast<int>(currentGamePlaneModeSettings.sourceSurround)
                      << " centerUv=(" << envFloat("FNVXR_GAME_PLANE_CENTER_UV_LEFT", 0.0f)
                      << "," << envFloat("FNVXR_GAME_PLANE_CENTER_UV_TOP", 0.0f)
                      << "," << envFloat("FNVXR_GAME_PLANE_CENTER_UV_RIGHT", 1.0f)
                      << "," << envFloat("FNVXR_GAME_PLANE_CENTER_UV_BOTTOM", 1.0f)
                      << ")"
                      << " surroundScale=(" << currentGamePlaneModeSettings.surroundScaleX
                      << "," << currentGamePlaneModeSettings.surroundScaleY
                      << ")"
                      << " surroundDepth=(" << currentGamePlaneModeSettings.surroundDepthX
                      << "," << currentGamePlaneModeSettings.surroundDepthY
                      << "," << currentGamePlaneModeSettings.surroundCornerDepth
                      << ")"
                      << " edgeStitch=" << static_cast<int>(currentGamePlaneModeSettings.edgeStitch)
                      << " transitionPower=" << currentGamePlaneModeSettings.transitionPower
                      << " runtimeGameplay=" << static_cast<int>(runtimeGameplayActive)
                      << " runtimeUi=" << static_cast<int>(runtimeUiActive)
                      << " runtimeCamera=" << static_cast<int>(runtimeCameraActive)
                      << " menuBits=0x" << std::hex << runtimeMenuBits << std::dec
                      << " pipBoyMenuMode=" << static_cast<int>(pipBoyMenuMode)
                      << " dialogueSize=" << static_cast<int>(dialoguePlaneSize)
                      << "\n";
        }
        const bool gameUiModeChanged = gameUiMode != previousGameUiMode;
        if (gameUiMode)
        {
            gameUiModeFrames = previousGameUiMode ? gameUiModeFrames + 1 : 1;
            if (!previousGameUiMode || !hasPauseSceneAnchor)
            {
                pauseSceneAnchor = pauseSceneAnchorFromHmd(hmdPose);
                hasPauseSceneAnchor = true;
                std::cout << "pauseSceneAnchor captured x=" << pauseSceneAnchor.position.x
                          << " y=" << pauseSceneAnchor.position.y
                          << " z=" << pauseSceneAnchor.position.z
                          << "\n";
            }
        }
        else
        {
            gameUiModeFrames = 0;
            hasPauseSceneAnchor = false;
        }
        if (gameUiModeChanged)
        {
            renderer.hasStereoGameFrame = false;
            renderer.stereoGameFrameSeparated = false;
            renderer.stereoGameFrameWorldCandidate = false;
            renderer.stereoGameFrameSequence = 0;
            renderer.stereoGamePoseSequence = 0;
            renderer.stereoGameRenderedDisplayTime = 0;
            renderer.stereoStableFrameCount = 0;
            renderer.stereoFrameMisses = 0;
            missingStereoFrames = 0;
        }
        previousGameUiMode = gameUiMode;
        const bool controlMode = mode == HostMode::Control;
        const bool flatMode = mode == HostMode::Flat;
        const bool vrMode = mode == HostMode::Vr;
        const bool cacheOnlyMode =
            renderer.hasCachedSceneFrame
            && envEnabled("FNVXR_CACHE_ONLY", false);
        const bool worldBehindMenu =
            gameUiMode
            && envEnabled("FNVXR_GAME_WORLD_BEHIND_MENU", true);
        const bool allowCacheFullscreen =
            cacheOnlyMode
            && envEnabled("FNVXR_ALLOW_CACHE_FULLSCREEN", false);
        const bool allowStereoFullscreen =
            allowCacheFullscreen
            || (envEnabled("FNVXR_GAME_FULLSCREEN_IN_XR", true)
                && stereoWorldIntent
                && stereoCellStable
                && (!gameUiMode || worldBehindMenu)
                && (vrMode || worldBehindMenu));
        const bool showGameplayPlane =
            !gameUiMode
            && !cacheOnlyMode
            && !allowStereoFullscreen
            && (flatMode
                || controlMode
                || (vrMode && envEnabled("FNVXR_SHOW_GAME_PLANE_IN_GAME", false)))
            && envEnabled("FNVXR_SHOW_GAME_PLANE_IN_GAME", true);
        const bool showGamePlane =
            !cacheOnlyMode
            &&
            (gameUiMode || showGameplayPlane)
            && envEnabled("FNVXR_SHOW_GAME_PLANE", true);
        const uint64_t stereoStaleFrameLimit = static_cast<uint64_t>(std::max(1, envInt("FNVXR_STEREO_STALE_FRAME_LIMIT", 2)));
        const uint64_t stereoStableHandoffFrames =
            static_cast<uint64_t>(std::max(1, envInt("FNVXR_STEREO_STABLE_HANDOFF_FRAMES", 6)));
        const bool allowStereoWorld2dFallback =
            !stereoWorldIntent || envEnabled("FNVXR_ALLOW_STEREO_WORLD_2D_FALLBACK", false);
        const bool stereoLossGamePlaneAllowed =
            envEnabled("FNVXR_SHOW_GAME_PLANE_ON_STEREO_LOSS", false)
            && allowStereoWorld2dFallback;
        const bool monoFullscreenFallbackAllowed =
            envEnabled("FNVXR_STEREO_FALLBACK_MONO_FULLSCREEN", false)
            && allowStereoWorld2dFallback;
        const bool menuPointerInputActive =
            shouldReadInput
            && showGamePlane
            && (haveRuntimeUiState ? runtimeUiActive : gameUiMode)
            && !runtimeGameplayActive;
        const bool gameplayControlsActive =
            shouldReadInput
            && (haveRuntimeUiState ? (!runtimeUiActive || runtimeGameplayActive) : !gameUiMode);
        const float headspaceAimTrigger = std::clamp(envFloat("FNVXR_HEADSPACE_LOOK_AIM_TRIGGER", 0.35f), 0.0f, 1.0f);
        const bool headspaceAimHeld = gameplayControlsActive && frame.leftTrigger >= headspaceAimTrigger;
        const bool weaponOut = gameplayControlsActive && lastSharedPlayerWeaponOut;
        const bool precisionAimActive = weaponOut;
        const bool handspaceLookRequiresWeaponOut = envEnabled("FNVXR_HANDSPACE_LOOK_REQUIRES_WEAPON_OUT", true);
        if (gameplayControlsActive && !previousGameplayControlsActive)
        {
            headspaceLookTracker.hasLast = false;
            handspaceLookTracker.hasLast = false;
            gyroAimLookTracker.hasLast = false;
            const uint64_t warmupFrames =
                static_cast<uint64_t>(std::max(0, envInt("FNVXR_HEADSPACE_LOOK_GAMEPLAY_WARMUP_FRAMES", 45)));
            gameplayControlsWarmupUntilFrame = frame.frame + warmupFrames;
            std::cout << "headspaceGameplayReset frame=" << frame.frame
                      << " warmupFrames=" << warmupFrames
                      << " warmupUntil=" << gameplayControlsWarmupUntilFrame
                      << " runtimeGameplay=" << static_cast<int>(runtimeGameplayActive)
                      << " runtimeUi=" << static_cast<int>(runtimeUiActive)
                      << " menuBits=0x" << std::hex << runtimeMenuBits << std::dec
                      << "\n";
        }
        else if (!gameplayControlsActive && previousGameplayControlsActive)
        {
            headspaceLookTracker.hasLast = false;
            handspaceLookTracker.hasLast = false;
            gyroAimLookTracker.hasLast = false;
            gameplayControlsWarmupUntilFrame = 0;
        }
        const bool headspaceGameplayWarmup =
            gameplayControlsActive
            && gameplayControlsWarmupUntilFrame != 0
            && frame.frame < gameplayControlsWarmupUntilFrame;
        const bool headspaceTrackingActive =
            shouldReadInput
            && hmdTracked
            && gameplayControlsActive
            && !headspaceGameplayWarmup;
        headspaceLook = sampleHeadspaceLook(
            headspaceLookTracker,
            hmdTracked ? rawHmdPose : hmdPose,
            headspaceTrackingActive,
            headspaceAimHeld);
        const bool gyroAimTrackingActive =
            shouldReadInput
            && gameplayControlsActive
            && !headspaceGameplayWarmup
            && precisionAimActive
            && rightAimTracked;
        gyroAimLook = sampleRightHandGyroLook(
            gyroAimLookTracker,
            rightAimPose,
            gyroAimTrackingActive);
        const bool handspaceLookTrackingActive =
            shouldReadInput
            && gameplayControlsActive
            && !headspaceGameplayWarmup
            && !precisionAimActive
            && (!handspaceLookRequiresWeaponOut || weaponOut)
            && rightAimTracked;
        handspaceLook = sampleRightHandspaceLook(
            handspaceLookTracker,
            rightAimPose,
            gamePlane,
            handspaceLookTrackingActive);
        HeadspaceLook noHandLook {};
        const bool publishPrecisionHandLook = precisionAimActive && gyroAimLook.active;
        const bool publishHandspaceLook = !precisionAimActive && handspaceLook.active;
        const HeadspaceLook& publishedHandLook =
            publishPrecisionHandLook ? gyroAimLook : (publishHandspaceLook ? handspaceLook : noHandLook);
        const char* publishedHandLookName =
            publishPrecisionHandLook ? "precisionGyro" : (publishHandspaceLook ? "handspace" : "none");
        previousGameplayControlsActive = gameplayControlsActive;

        const bool pointerHandLeft = envEqualsIgnoreCase("FNVXR_MENU_POINTER_HAND", "left");
        const bool pointerHandRight = envEqualsIgnoreCase("FNVXR_MENU_POINTER_HAND", "right");
        const bool pointerHandBoth = envEqualsIgnoreCase("FNVXR_MENU_POINTER_HAND", "both");
        const bool pointerHeadOnly =
            envEqualsIgnoreCase("FNVXR_MENU_POINTER_HAND", "head")
            || envEqualsIgnoreCase("FNVXR_MENU_POINTER_SOURCE", "head");
        const bool pointerHeadFallback = envEnabled("FNVXR_MENU_POINTER_HEAD_FALLBACK", false);
        const MenuPointer leftMenuPointer = leftAimTracked
            ? menuPointerFromAimPose(leftAimPose, gamePlane)
            : MenuPointer {};
        const MenuPointer rightMenuPointer = rightAimTracked
            ? menuPointerFromAimPose(rightAimPose, gamePlane)
            : MenuPointer {};
        const MenuPointer headMenuPointer = menuPointerFromAimPose(hmdTracked ? rawHmdPose : hmdPose, gamePlane);
        MenuPointer menuPointer = rightMenuPointer;
        const char* menuPointerSource = rightMenuPointer.active ? "right" : "none";
        if (pointerHeadOnly)
        {
            menuPointer = headMenuPointer;
            menuPointerSource = headMenuPointer.active ? "head" : "none";
        }
        else if (pointerHandLeft)
        {
            menuPointer = leftMenuPointer;
            menuPointerSource = leftMenuPointer.active ? "left" : "none";
        }
        else if (pointerHandRight)
        {
            menuPointer = rightMenuPointer;
            menuPointerSource = rightMenuPointer.active ? "right" : "none";
        }
        else if (pointerHandBoth)
        {
            menuPointer = rightMenuPointer.active ? rightMenuPointer : leftMenuPointer;
            menuPointerSource = rightMenuPointer.active ? "right" : (leftMenuPointer.active ? "left" : "none");
        }
        if (!pointerHeadOnly && !menuPointer.active && pointerHeadFallback && headMenuPointer.active)
        {
            menuPointer = headMenuPointer;
            menuPointerSource = "headFallback";
        }
        if (menuPointer.active && hasLastActiveMenuPointer)
        {
            const float smoothing = clamp01(envFloat("FNVXR_MENU_POINTER_SMOOTHING", 0.35f));
            menuPointer.x = lastActiveMenuPointer.x + (menuPointer.x - lastActiveMenuPointer.x) * (1.0f - smoothing);
            menuPointer.y = lastActiveMenuPointer.y + (menuPointer.y - lastActiveMenuPointer.y) * (1.0f - smoothing);
        }
        if (menuPointer.active)
        {
            lastActiveMenuPointer = menuPointer;
            hasLastActiveMenuPointer = true;
        }
        const bool hostRightTriggerDown = rightTrigger.value > 0.65f;
        const bool hostLeftTriggerDown = leftTrigger.value > 0.65f;
        bool hostClick =
            (hostLeftTriggerDown && !previousHostLeftTriggerDown)
            || (hostRightTriggerDown && !previousHostRightTriggerDown);
        if (pointerHandLeft)
        {
            hostClick = hostLeftTriggerDown && !previousHostLeftTriggerDown;
        }
        else if (pointerHandRight)
        {
            hostClick = hostRightTriggerDown && !previousHostRightTriggerDown;
        }
        previousHostLeftTriggerDown = hostLeftTriggerDown;
        previousHostRightTriggerDown = hostRightTriggerDown;
        previousHostButtonADown = buttonA.value;
        previousHostButtonXDown = buttonX.value;
        if (!menuPointerInputActive)
        {
            menuPointer = {};
            menuPointerSource = "none";
            hostClick = false;
            hasLastActiveMenuPointer = false;
        }
        else if (!menuPointer.active && hasLastActiveMenuPointer)
        {
            menuPointer = lastActiveMenuPointer;
            menuPointerSource = "latched";
        }
        updateHostCursorPointer(menuPointer, hostClick);
        frame.menuPointerActive = menuPointer.active ? 1 : 0;
        frame.menuPointerX = menuPointer.x;
        frame.menuPointerY = menuPointer.y;
        // Shared retail tracking must preserve the same predicted-time sample
        // for head and hands. Independent smoothing creates artificial
        // head/hand lag and makes a stationary physical pose swim in view.
        frame.hmdRot = toQuat(rawHmdPose.orientation);
        frame.hmdPos = toVec3(rawHmdPose.position);
        frame.leftRot = toQuat(rawLeftPose.orientation);
        frame.leftPos = toVec3(rawLeftPose.position);
        frame.rightRot = toQuat(rawRightPose.orientation);
        frame.rightPos = toVec3(rawRightPose.position);
        std::uint32_t poseTrackingFlags = 0;
        if (hmdTracked)
            poseTrackingFlags |= fnvxr::shared::VrPoseTrackingHmd;
        if (leftHandPoseActive)
            poseTrackingFlags |= fnvxr::shared::VrPoseTrackingLeftGripActive;
        if (rightHandPoseActive)
            poseTrackingFlags |= fnvxr::shared::VrPoseTrackingRightGripActive;
        if (leftHandTracked)
            poseTrackingFlags |= fnvxr::shared::VrPoseTrackingLeftGripCurrent;
        if (rightHandTracked)
            poseTrackingFlags |= fnvxr::shared::VrPoseTrackingRightGripCurrent;
        if (leftAimPoseActive)
            poseTrackingFlags |= fnvxr::shared::VrPoseTrackingLeftAimActive;
        if (rightAimPoseActive)
            poseTrackingFlags |= fnvxr::shared::VrPoseTrackingRightAimActive;
        if (leftAimTracked)
            poseTrackingFlags |= fnvxr::shared::VrPoseTrackingLeftAimCurrent;
        if (rightAimTracked)
            poseTrackingFlags |= fnvxr::shared::VrPoseTrackingRightAimCurrent;
        if (recenterChord)
            poseTrackingFlags |= fnvxr::shared::VrPoseTrackingRecenterRequested;

        XrFrameEndInfo endInfo { XR_TYPE_FRAME_END_INFO };
        std::vector<XrView> views(viewConfigCount);
        for (auto& view : views)
            view.type = XR_TYPE_VIEW;

        XrCompositionLayerProjectionView projectionViews[2] {
            { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW },
            { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW }
        };
        bool submitProjectionLayer = false;
        bool submittedStereoFullscreen = false;
        int64_t sourcePoseAgeNanoseconds = 0;
        bool sourcePoseAgeValid = false;
        const int64_t maximumSourcePoseAgeNanoseconds =
            static_cast<int64_t>(std::max(1, envInt("FNVXR_STEREO_MAX_SOURCE_POSE_AGE_MS", 25)))
            * 1000000LL;

        XrViewLocateInfo viewLocateInfo { XR_TYPE_VIEW_LOCATE_INFO };
        viewLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        viewLocateInfo.displayTime = frameState.predictedDisplayTime;
        viewLocateInfo.space = localSpace;

        XrViewState viewState { XR_TYPE_VIEW_STATE };
        uint32_t viewCountOutput = 0;
        result = xr.locateViews(
            session,
            &viewLocateInfo,
            &viewState,
            static_cast<uint32_t>(views.size()),
            &viewCountOutput,
            views.data());
        const XrViewStateFlags requiredViewState =
            XR_VIEW_STATE_ORIENTATION_VALID_BIT | XR_VIEW_STATE_POSITION_VALID_BIT;
        const bool viewsValid = (viewState.viewStateFlags & requiredViewState) == requiredViewState;
        publishHostSharedBridge(
            sharedBridge,
            frame,
            frameState.predictedDisplayTime,
            menuPointer,
            headspaceLook,
            publishedHandLook,
            hostClick,
            menuPointerInputActive,
            gameplayControlsActive,
            weaponOut,
            runtimeMenuBits,
            poseTrackingFlags,
            referenceSpaceGeneration,
            rawLeftAimPose,
            rawRightAimPose,
            (result == XR_SUCCESS && viewCountOutput >= 2 && viewsValid) ? views.data() : nullptr,
            (result == XR_SUCCESS && viewCountOutput >= 2 && viewsValid) ? viewCountOutput : 0);
        if (frameIndex <= 5
            || frameIndex % 300 == 0
            || std::fabs(frame.leftThumbstickX) > 0.10f
            || std::fabs(frame.leftThumbstickY) > 0.10f
            || std::fabs(frame.rightThumbstickX) > 0.10f
            || std::fabs(frame.rightThumbstickY) > 0.10f
            || headspaceLook.yawMicroradians != 0
            || headspaceLook.pitchMicroradians != 0
            || handspaceLook.yawMicroradians != 0
            || handspaceLook.pitchMicroradians != 0
            || gyroAimLook.yawMicroradians != 0
            || gyroAimLook.pitchMicroradians != 0
            || frame.buttons != 0)
        {
            static uint64_t loggedControlFrames = 0;
            if (loggedControlFrames < 120 || frameIndex % 300 == 0)
            {
                ++loggedControlFrames;
                std::cout << "controlBridge frame=" << frame.frame
                          << " shouldReadInput=" << static_cast<int>(shouldReadInput)
                          << " menuInput=" << static_cast<int>(menuPointerInputActive)
                          << " gameplayControls=" << static_cast<int>(gameplayControlsActive)
                          << " ls=(" << frame.leftThumbstickX << "," << frame.leftThumbstickY << ")"
                          << " rs=(" << frame.rightThumbstickX << "," << frame.rightThumbstickY << ")"
                          << " lt=" << frame.leftTrigger
                          << " aim=" << static_cast<int>(headspaceAimHeld)
                          << " weaponOut=" << static_cast<int>(weaponOut)
                          << " precision=" << static_cast<int>(precisionAimActive)
                          << " head=(" << headspaceLook.yawMicroradians << "," << headspaceLook.pitchMicroradians << ")"
                          << " headActive=" << static_cast<int>(headspaceLook.active)
                          << " handspace=(" << handspaceLook.yawMicroradians << "," << handspaceLook.pitchMicroradians << ")"
                          << " handspaceActive=" << static_cast<int>(handspaceLook.active)
                          << " gyro=(" << gyroAimLook.yawMicroradians << "," << gyroAimLook.pitchMicroradians << ")"
                          << " gyroActive=" << static_cast<int>(gyroAimLook.active)
                          << " handspaceRequiresWeaponOut=" << static_cast<int>(handspaceLookRequiresWeaponOut)
                          << " publishedHandLook=" << publishedHandLookName
                          << " buttons=0x" << std::hex << frame.buttons << std::dec
                          << " btn(A/B/X/Y/LM/RM/L3/R3)="
                          << static_cast<int>(buttonA.value)
                          << "/" << static_cast<int>(buttonB.value)
                          << "/" << static_cast<int>(buttonX.value)
                          << "/" << static_cast<int>(buttonY.value)
                          << "/" << static_cast<int>(leftMenu.value)
                          << "/" << static_cast<int>(rightMenu.value)
                          << "/" << static_cast<int>(leftThumbstick.value)
                          << "/" << static_cast<int>(rightThumbstick.value)
                          << " pointer=" << static_cast<int>(menuPointer.active)
                          << " pointerSource=" << menuPointerSource
                          << " click=" << static_cast<int>(hostClick)
                          << "\n";
            }
        }
        if (shouldRender && result == XR_SUCCESS && viewCountOutput >= 2 && viewsValid)
        {
            if (gameUiMode)
                ++gameUiRenderFrames;
            else
                gameUiRenderFrames = 0;

            const float captureHz = envFloat("FNVXR_GAME_PLANE_CAPTURE_HZ", 45.0f);
            const auto now = std::chrono::steady_clock::now();
            if (captureHz <= 0.0f || now >= nextGamePlaneCapture)
            {
                if (!cacheOnlyMode
                    && (showGamePlane
                    || (allowStereoFullscreen && stereoLossGamePlaneAllowed)
                    || (allowStereoFullscreen && monoFullscreenFallbackAllowed)))
                {
                        updateGameTexture(device.Get(), renderer);
                        updateWorldTexture(device.Get(), renderer);
                }
                if (captureHz > 0.0f)
                {
                    const auto interval = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                        std::chrono::duration<double>(1.0 / static_cast<double>(captureHz)));
                    nextGamePlaneCapture = now + interval;
                }
            }
            if (allowStereoFullscreen)
            {
                    updateStereoGameTextures(device.Get(), renderer);
            }
            else
            {
                renderer.hasStereoGameFrame = false;
                renderer.stereoGameFrameSeparated = false;
                renderer.stereoGameFrameWorldCandidate = false;
                renderer.stereoGameFrameSequence = 0;
                renderer.stereoGamePoseSequence = 0;
                renderer.stereoGameRenderedDisplayTime = 0;
                renderer.stereoStableFrameCount = 0;
                renderer.stereoFrameMisses = 0;
            }
            if (allowStereoFullscreen
                && renderer.stereoFrameMisses > stereoStaleFrameLimit)
            {
                renderer.hasStereoGameFrame = false;
                renderer.stereoGameFrameSeparated = false;
                renderer.stereoGameFrameWorldCandidate = false;
                renderer.stereoGameFrameSequence = 0;
                renderer.stereoGamePoseSequence = 0;
                renderer.stereoGameRenderedDisplayTime = 0;
                renderer.stereoStableFrameCount = 0;
            }
            const int64_t sourcePoseFutureToleranceNanoseconds =
                static_cast<int64_t>(std::max(0, envInt("FNVXR_STEREO_SOURCE_POSE_FUTURE_TOLERANCE_MS", 5)))
                * 1000000LL;
            sourcePoseAgeValid = fnvxr::stereo::sourcePoseAgeWithinBudget(
                frameState.predictedDisplayTime,
                renderer.stereoGameRenderedDisplayTime,
                maximumSourcePoseAgeNanoseconds,
                sourcePoseFutureToleranceNanoseconds,
                &sourcePoseAgeNanoseconds);
            const bool stereoFullscreenActive =
                allowStereoFullscreen
                && renderer.hasStereoGameFrame
                && renderer.stereoGameFrameSeparated
                && renderer.stereoStableFrameCount >= stereoStableHandoffFrames
                && renderer.stereoReferenceSpaceGeneration == referenceSpaceGeneration
                && renderer.stereoProducerEpoch == sharedBridge.producerEpoch
                && sourcePoseAgeValid;
            submittedStereoFullscreen = stereoFullscreenActive;
            std::vector<XrView> submitViews = views;
            bool usingSourceStereoViews = false;
            if (stereoFullscreenActive && renderer.stereoGameRenderedDisplayTime > 0)
            {
                // These are the exact per-eye poses/FOV copied atomically with
                // the D3D9 pixels. Never label historical pixels with current
                // xrLocateViews output; the compositor can reproject from the
                // source pose to this frame's display time.
                submitViews[0] = renderer.stereoSourceViews[0];
                submitViews[1] = renderer.stereoSourceViews[1];
                usingSourceStereoViews = true;
            }
            const bool monoFullscreenFallback =
                allowStereoFullscreen
                && !worldBehindMenu
                && !stereoFullscreenActive
                && renderer.hasGameTextureFrame
                && monoFullscreenFallbackAllowed;
            const bool rawStereoReady =
                renderer.stereoGameFrameSeparated
                && renderer.stereoGameFrameWorldCandidate
                && renderer.stereoStableFrameCount > 0;
            const bool stereoLossGamePlane =
                allowStereoFullscreen
                && !stereoFullscreenActive
                && !monoFullscreenFallback
                && renderer.hasGameTextureFrame
                && stereoLossGamePlaneAllowed;
            const bool sceneBeforeQuad = false;
            const bool displayGamePlane = showGamePlane || stereoLossGamePlane;
            if (stereoFullscreenActive)
                missingStereoFrames = 0;
            else if (allowStereoFullscreen)
                ++missingStereoFrames;
            if (stereoFullscreenActive != previousStereoFullscreenActive
                || displayGamePlane != previousShowGamePlane
                || allowStereoFullscreen != previousAllowStereoFullscreen
                || monoFullscreenFallback != previousMonoFullscreenFallback
                || rawStereoReady != previousRawStereoReady
                || (renderer.stereoStableFrameCount != previousStereoStableFrameCount
                    && renderer.stereoStableFrameCount <= stereoStableHandoffFrames)
                || (renderer.stereoGameFrameSequence != previousStereoGameFrameSequence
                    && renderer.stereoStableFrameCount <= stereoStableHandoffFrames)
                || (allowStereoFullscreen && missingStereoFrames > 0 && missingStereoFrames % 120 == 0))
            {
                previousStereoFullscreenActive = stereoFullscreenActive;
                previousShowGamePlane = displayGamePlane;
                previousAllowStereoFullscreen = allowStereoFullscreen;
                previousMonoFullscreenFallback = monoFullscreenFallback;
                previousRawStereoReady = rawStereoReady;
                previousStereoStableFrameCount = renderer.stereoStableFrameCount;
                previousStereoGameFrameSequence = renderer.stereoGameFrameSequence;
                std::cout << "renderMode mode=" << hostModeName(mode)
                          << " stereoFullscreen=" << static_cast<int>(stereoFullscreenActive)
                          << " allowStereo=" << static_cast<int>(allowStereoFullscreen)
                          << " showGamePlane=" << static_cast<int>(displayGamePlane)
                          << " sceneBeforeQuad=" << static_cast<int>(sceneBeforeQuad)
                          << " gameUiFrames=" << gameUiModeFrames
                          << " gameUiRenderFrames=" << gameUiRenderFrames
                          << " stereoLossPlane=" << static_cast<int>(stereoLossGamePlane)
                          << " showGameplayPlane=" << static_cast<int>(showGameplayPlane)
                          << " monoFallback=" << static_cast<int>(monoFullscreenFallback)
                          << " monoFallbackAllowed=" << static_cast<int>(monoFullscreenFallbackAllowed)
                          << " stereoLossPlaneAllowed=" << static_cast<int>(stereoLossGamePlaneAllowed)
                          << " gameUi=" << static_cast<int>(gameUiMode)
                          << " plane=(" << gamePlane.width << "x" << gamePlane.height << ")"
                          << " worldBehindMenu=" << static_cast<int>(worldBehindMenu)
                          << " sharedStereoUi=" << static_cast<int>(sharedStereoUiActive)
                          << " sharedStereoWorldReady=" << static_cast<int>(sharedStereoWorldReady)
                          << " sharedStereoWorldHeld=" << static_cast<int>(heldSharedStereoWorldReady)
                          << " haveSharedStereoUiState=" << static_cast<int>(haveSharedStereoUiState)
                          << " runtimeUi=" << static_cast<int>(runtimeUiActive)
                          << " runtimeGameplay=" << static_cast<int>(runtimeGameplayActive)
                          << " runtimeCamera=" << static_cast<int>(runtimeCameraActive)
                          << " menuPointerInput=" << static_cast<int>(menuPointerInputActive)
                          << " gameplayControls=" << static_cast<int>(gameplayControlsActive)
                          << " headspaceLook=" << static_cast<int>(headspaceLook.active)
                          << " pointerSource=" << menuPointerSource
                          << " runtimeUiStuckFrames=" << runtimeUiStuckFrames
                          << " haveRuntimeUiState=" << static_cast<int>(haveRuntimeUiState)
                          << " cellKnown=" << static_cast<int>(stereoCellKnown)
                          << " cell=0x" << std::hex << stereoCellFormId << std::dec
                          << " cellStable=" << static_cast<int>(stereoCellStable)
                          << " cellStableFrames=" << stereoCellStableFrames << "/" << stereoCellStableRequired
                          << " hasGameTexture=" << static_cast<int>(renderer.hasGameTextureFrame)
                          << " gameTextureUpdates=" << renderer.gameTextureUpdates
                          << " hasWorldTexture=" << static_cast<int>(renderer.hasWorldTextureFrame)
                          << " worldTextureUpdates=" << renderer.worldTextureUpdates
                          << " worldTextureMisses=" << renderer.worldTextureMisses
                          << " hasStereo=" << static_cast<int>(renderer.hasStereoGameFrame)
                          << " rawStereoReady=" << static_cast<int>(rawStereoReady)
                          << " separated=" << static_cast<int>(renderer.stereoGameFrameSeparated)
                          << " worldCandidate=" << static_cast<int>(renderer.stereoGameFrameWorldCandidate)
                          << " stereoSeq=" << renderer.stereoGameFrameSequence
                          << " stereoPoseSeq=" << renderer.stereoGamePoseSequence
                          << " stereoRenderedTime=" << renderer.stereoGameRenderedDisplayTime
                          << " sourcePoseAgeNs=" << sourcePoseAgeNanoseconds
                          << " sourcePoseAgeValid=" << static_cast<int>(sourcePoseAgeValid)
                          << " sourcePoseAgeLimitNs=" << maximumSourcePoseAgeNanoseconds
                          << " exactSourceStereoViews=" << static_cast<int>(usingSourceStereoViews)
                          << " stereoStable=" << renderer.stereoStableFrameCount << "/" << stereoStableHandoffFrames
                          << " stereoUpdates=" << renderer.lastStereoFrameUpdated
                          << " stereoMisses=" << renderer.stereoFrameMisses
                          << " stereoTransientReads=" << renderer.stereoTransientReadMisses
                          << " missingStereoFrames=" << missingStereoFrames
                          << "\n";
            }
            bool leftRendered = false;
            bool rightRendered = false;
            {
                leftRendered = renderEye(
                    xr,
                    device.Get(),
                    renderer,
                    leftEye,
                    0,
                    views[0],
                    leftPose,
                    rightPose,
                    leftAimPose,
                    rightAimPose,
                    bodyRig,
                    menuPointer,
                    gamePlane,
                    pauseSceneAnchor,
                    gameUiMode,
                    displayGamePlane,
                    stereoFullscreenActive || monoFullscreenFallback,
                    leftEyeColor);
                rightRendered = renderEye(
                    xr,
                    device.Get(),
                    renderer,
                    rightEye,
                    1,
                    views[1],
                    leftPose,
                    rightPose,
                    leftAimPose,
                    rightAimPose,
                    bodyRig,
                    menuPointer,
                    gamePlane,
                    pauseSceneAnchor,
                    gameUiMode,
                    displayGamePlane,
                    stereoFullscreenActive || monoFullscreenFallback,
                    rightEyeColor);
            }

            submitProjectionLayer = leftRendered && rightRendered;
            if (submitProjectionLayer)
            {
                projectionViews[0].pose = submitViews[0].pose;
                projectionViews[0].fov = submitViews[0].fov;
                projectionViews[0].subImage.swapchain = leftEye.handle;
                projectionViews[0].subImage.imageRect.offset = { 0, 0 };
                projectionViews[0].subImage.imageRect.extent = { leftEye.width, leftEye.height };

                projectionViews[1].pose = submitViews[1].pose;
                projectionViews[1].fov = submitViews[1].fov;
                projectionViews[1].subImage.swapchain = rightEye.handle;
                projectionViews[1].subImage.imageRect.offset = { 0, 0 };
                projectionViews[1].subImage.imageRect.extent = { rightEye.width, rightEye.height };
            }
            else
            {
                std::cerr << "renderEye failed left=" << static_cast<int>(leftRendered)
                          << " right=" << static_cast<int>(rightRendered)
                          << "; submitting zero layers\n";
            }
        }
        else if (shouldRender)
        {
            std::cerr << "xrLocateViews failed or returned fewer than two views result="
                      << resultName(result)
                      << " viewCount=" << viewCountOutput
                      << " viewStateFlags=0x" << std::hex << viewState.viewStateFlags << std::dec
                      << "; submitting zero layers\n";
        }

        XrCompositionLayerProjection projectionLayer { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
        projectionLayer.space = localSpace;
        projectionLayer.viewCount = 2;
        projectionLayer.views = projectionViews;

        const XrCompositionLayerBaseHeader* layers[] = {
            reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projectionLayer),
        };

        endInfo.displayTime = frameState.predictedDisplayTime;
        endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        if (submitProjectionLayer)
        {
            endInfo.layerCount = static_cast<uint32_t>(std::size(layers));
            endInfo.layers = layers;
        }
        else
        {
            endInfo.layerCount = 0;
            endInfo.layers = nullptr;
        }
        const XrResult endResult = xr.endFrame(session, &endInfo);
        if (frameIndex <= 12
            || frameIndex % 60 == 0
            || !submitProjectionLayer
            || endResult != XR_SUCCESS)
        {
            std::cout
                << "{\"event\":\"fnvxrOpenXrSubmit\",\"frame\":" << frameIndex
                << ",\"predictedDisplayTime\":" << frameState.predictedDisplayTime
                << ",\"sourceStereoSequence\":" << renderer.stereoGameFrameSequence
                << ",\"sourcePoseSequence\":" << renderer.stereoGamePoseSequence
                << ",\"sourceRenderedDisplayTime\":" << renderer.stereoGameRenderedDisplayTime
                << ",\"sourcePoseAgeNanoseconds\":" << sourcePoseAgeNanoseconds
                << ",\"sourcePoseAgeValid\":" << (sourcePoseAgeValid ? "true" : "false")
                << ",\"sourcePoseAgeLimitNanoseconds\":" << maximumSourcePoseAgeNanoseconds
                << ",\"leftHash\":\"0x" << std::hex << renderer.stereoLeftHash
                << "\",\"rightHash\":\"0x" << renderer.stereoRightHash << std::dec
                << "\",\"pixelSamples\":" << renderer.stereoPixelSamples
                << ",\"nonBlackSamples\":" << renderer.stereoNonBlackSamples
                << ",\"meaningfulDifferentSamples\":" << renderer.stereoMeaningfulDifferentSamples
                << ",\"leftActiveTiles\":" << renderer.stereoLeftActiveTiles
                << ",\"rightActiveTiles\":" << renderer.stereoRightActiveTiles
                << ",\"differentTiles\":" << renderer.stereoDifferentTiles
                << ",\"stereoFullscreen\":" << (submittedStereoFullscreen ? "true" : "false")
                << ",\"runtimeGameplay\":" << (runtimeGameplayActive ? "true" : "false")
                << ",\"projectionLayerSubmitted\":" << (submitProjectionLayer ? "true" : "false")
                << ",\"layerCount\":" << endInfo.layerCount
                << ",\"xrEndFrame\":\"" << resultName(endResult) << "\"}"
                << "\n";
        }
        if (endResult == XR_SESSION_LOSS_PENDING)
        {
            lossPending = true;
            invalidateHostSharedBridgeTracking(sharedBridge, "xrEndFrame-session-loss-pending");
            break;
        }
        if (endResult == XR_ERROR_SESSION_NOT_RUNNING)
        {
            shouldSyncFrames = false;
            shouldReadInput = false;
            previousShouldSyncFrames = false;
            invalidateHostSharedBridgeTracking(sharedBridge, "xrEndFrame-session-not-running");
            continue;
        }
        if (endResult != XR_SUCCESS)
        {
            std::cerr << "xrEndFrame failed: " << resultName(endResult) << "\n";
            break;
        }
    }

    closeHostSharedBridge(sharedBridge);
    closeSharedD3D9VideoFrame(renderer);
    if (sessionBegun && !lossPending)
        xr.endSession(session);
    xr.destroySwapchain(leftEye.handle);
    xr.destroySwapchain(rightEye.handle);
    xr.destroySpace(leftAimSpace);
    xr.destroySpace(rightAimSpace);
    xr.destroySpace(leftSpace);
    xr.destroySpace(rightSpace);
    xr.destroySpace(viewSpace);
    xr.destroySpace(localSpace);
    xr.destroyAction(actions.rightThumbstickAxis);
    xr.destroyAction(actions.leftThumbstickAxis);
    xr.destroyAction(actions.rightThumbstick);
    xr.destroyAction(actions.leftThumbstick);
    xr.destroyAction(actions.rightMenu);
    xr.destroyAction(actions.leftMenu);
    xr.destroyAction(actions.buttonY);
    xr.destroyAction(actions.buttonX);
    xr.destroyAction(actions.buttonB);
    xr.destroyAction(actions.buttonA);
    xr.destroyAction(actions.rightSqueeze);
    xr.destroyAction(actions.leftSqueeze);
    xr.destroyAction(actions.rightTrigger);
    xr.destroyAction(actions.leftTrigger);
    xr.destroyAction(actions.aimPose);
    xr.destroyAction(actions.handPose);
    xr.destroyActionSet(actionSet);
    xr.destroySession(session);
    xr.destroyInstance(instance);
    FreeLibrary(xr.loader);

    return 0;
}
