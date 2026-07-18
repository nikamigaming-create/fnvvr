#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <bcrypt.h>
#include <d3d9.h>
#include <intrin.h>

#include "../protocol/fnvxr_shared_state.h"
#include "fnvxr_d3d9_activation.h"
#include "fnvxr_stereo_math.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace
{
using Direct3DCreate9Fn = IDirect3D9* (WINAPI*)(UINT);
using Direct3DCreate9ExFn = HRESULT(WINAPI*)(UINT, IDirect3D9Ex**);
using Direct3D9EnableMaximizedWindowedModeShimFn = void(WINAPI*)(BOOL);
using Direct3DShaderValidatorCreate9Fn = void* (WINAPI*)();
using Direct3DCreate9On12Fn = HRESULT(WINAPI*)(UINT, void*, UINT, IDirect3D9**);
using Direct3DCreate9On12ExFn = HRESULT(WINAPI*)(UINT, void*, UINT, IDirect3D9Ex**);
using D3DPERFBeginEventFn = int(WINAPI*)(D3DCOLOR, LPCWSTR);
using D3DPERFEndEventFn = int(WINAPI*)();
using D3DPERFGetStatusFn = DWORD(WINAPI*)();
using D3DPERFQueryRepeatFrameFn = BOOL(WINAPI*)();
using D3DPERFSetMarkerFn = void(WINAPI*)(D3DCOLOR, LPCWSTR);
using D3DPERFSetOptionsFn = void(WINAPI*)(DWORD);
using D3DPERFSetRegionFn = void(WINAPI*)(D3DCOLOR, LPCWSTR);
using DebugSetLevelFn = void(WINAPI*)(DWORD);
using DebugSetMuteFn = void(WINAPI*)();
using VoidNoArgsFn = void(WINAPI*)();

constexpr HRESULT D3DERR_NOTAVAILABLE_RESULT = static_cast<HRESULT>(0x8876086AL);

HMODULE gRealD3D9 = nullptr;
Direct3DCreate9Fn gRealDirect3DCreate9 = nullptr;
Direct3DCreate9ExFn gRealDirect3DCreate9Ex = nullptr;
using PresentFn = HRESULT(WINAPI*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
using EndSceneFn = HRESULT(WINAPI*)(IDirect3DDevice9*);
using ResetFn = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
using ClearFn = HRESULT(WINAPI*)(IDirect3DDevice9*, DWORD, const D3DRECT*, DWORD, D3DCOLOR, float, DWORD);
using SetTransformFn = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DTRANSFORMSTATETYPE, const D3DMATRIX*);
using SetVertexShaderFn = HRESULT(WINAPI*)(IDirect3DDevice9*, IDirect3DVertexShader9*);
using SetVertexShaderConstantFFn = HRESULT(WINAPI*)(IDirect3DDevice9*, UINT, const float*, UINT);
using SetPixelShaderFn = HRESULT(WINAPI*)(IDirect3DDevice9*, IDirect3DPixelShader9*);
using CreateTextureFn = HRESULT(WINAPI*)(
    IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*);
using CreateRenderTargetFn = HRESULT(WINAPI*)(
    IDirect3DDevice9*, UINT, UINT, D3DFORMAT, D3DMULTISAMPLE_TYPE, DWORD, BOOL, IDirect3DSurface9**, HANDLE*);
using CreateDepthStencilSurfaceFn = HRESULT(WINAPI*)(
    IDirect3DDevice9*, UINT, UINT, D3DFORMAT, D3DMULTISAMPLE_TYPE, DWORD, BOOL, IDirect3DSurface9**, HANDLE*);
using UpdateSurfaceFn = HRESULT(WINAPI*)(
    IDirect3DDevice9*, IDirect3DSurface9*, const RECT*, IDirect3DSurface9*, const POINT*);
using UpdateTextureFn = HRESULT(WINAPI*)(IDirect3DDevice9*, IDirect3DBaseTexture9*, IDirect3DBaseTexture9*);
using StretchRectFn = HRESULT(WINAPI*)(
    IDirect3DDevice9*, IDirect3DSurface9*, const RECT*, IDirect3DSurface9*, const RECT*, D3DTEXTUREFILTERTYPE);
using ColorFillFn = HRESULT(WINAPI*)(IDirect3DDevice9*, IDirect3DSurface9*, const RECT*, D3DCOLOR);
using SetRenderTargetFn = HRESULT(WINAPI*)(IDirect3DDevice9*, DWORD, IDirect3DSurface9*);
using SetDepthStencilSurfaceFn = HRESULT(WINAPI*)(IDirect3DDevice9*, IDirect3DSurface9*);
using SetTextureFn = HRESULT(WINAPI*)(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9*);
using SetTextureStageStateFn = HRESULT(WINAPI*)(IDirect3DDevice9*, DWORD, D3DTEXTURESTAGESTATETYPE, DWORD);
using DrawPrimitiveFn = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT);
using DrawIndexedPrimitiveFn = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT);
using DrawPrimitiveUPFn = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, const void*, UINT);
using DrawIndexedPrimitiveUPFn = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT, UINT, const void*,
    D3DFORMAT, const void*, UINT);
using MultiplyTransformFn = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DTRANSFORMSTATETYPE, const D3DMATRIX*);
using ProcessVerticesFn = HRESULT(WINAPI*)(
    IDirect3DDevice9*, UINT, UINT, UINT, IDirect3DVertexBuffer9*, IDirect3DVertexDeclaration9*, DWORD);
using DrawRectPatchFn = HRESULT(WINAPI*)(IDirect3DDevice9*, UINT, const float*, const D3DRECTPATCH_INFO*);
using DrawTriPatchFn = HRESULT(WINAPI*)(IDirect3DDevice9*, UINT, const float*, const D3DTRIPATCH_INFO*);
using BeginStateBlockFn = HRESULT(WINAPI*)(IDirect3DDevice9*);
using EndStateBlockFn = HRESULT(WINAPI*)(IDirect3DDevice9*, IDirect3DStateBlock9**);
using CreateQueryFn = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DQUERYTYPE, IDirect3DQuery9**);
PresentFn gRealPresent = nullptr;
EndSceneFn gRealEndScene = nullptr;
ResetFn gRealReset = nullptr;
ClearFn gRealClear = nullptr;
SetTransformFn gRealSetTransform = nullptr;
SetVertexShaderFn gRealSetVertexShader = nullptr;
SetVertexShaderConstantFFn gRealSetVertexShaderConstantF = nullptr;
SetPixelShaderFn gRealSetPixelShader = nullptr;
CreateTextureFn gRealCreateTexture = nullptr;
CreateRenderTargetFn gRealCreateRenderTarget = nullptr;
CreateDepthStencilSurfaceFn gRealCreateDepthStencilSurface = nullptr;
UpdateSurfaceFn gRealUpdateSurface = nullptr;
UpdateTextureFn gRealUpdateTexture = nullptr;
StretchRectFn gRealStretchRect = nullptr;
ColorFillFn gRealColorFill = nullptr;
SetRenderTargetFn gRealSetRenderTarget = nullptr;
SetDepthStencilSurfaceFn gRealSetDepthStencilSurface = nullptr;
SetTextureFn gRealSetTexture = nullptr;
SetTextureStageStateFn gRealSetTextureStageState = nullptr;
DrawPrimitiveFn gRealDrawPrimitive = nullptr;
DrawIndexedPrimitiveFn gRealDrawIndexedPrimitive = nullptr;
DrawPrimitiveUPFn gRealDrawPrimitiveUP = nullptr;
DrawIndexedPrimitiveUPFn gRealDrawIndexedPrimitiveUP = nullptr;
MultiplyTransformFn gRealMultiplyTransform = nullptr;
ProcessVerticesFn gRealProcessVertices = nullptr;
DrawRectPatchFn gRealDrawRectPatch = nullptr;
DrawTriPatchFn gRealDrawTriPatch = nullptr;
BeginStateBlockFn gRealBeginStateBlock = nullptr;
EndStateBlockFn gRealEndStateBlock = nullptr;
CreateQueryFn gRealCreateQuery = nullptr;
volatile LONG gPresentFrames = 0;
volatile LONG gEndSceneFrames = 0;
volatile LONG gSetTransformCalls = 0;
volatile LONG gSetVertexShaderCalls = 0;
volatile LONG gVertexShaderConstantFCalls = 0;
volatile LONG gSetPixelShaderCalls = 0;
volatile LONG gCreateTextureCalls = 0;
volatile LONG gCreateRenderTargetCalls = 0;
volatile LONG gCreateDepthStencilSurfaceCalls = 0;
volatile LONG gUpdateSurfaceCalls = 0;
volatile LONG gUpdateTextureCalls = 0;
volatile LONG gStretchRectCalls = 0;
volatile LONG gSetRenderTargetCalls = 0;
volatile LONG gSetDepthStencilSurfaceCalls = 0;
volatile LONG gSetTextureCalls = 0;
volatile LONG gSetTextureStageStateCalls = 0;
volatile LONG gDrawPrimitiveCalls = 0;
volatile LONG gDrawIndexedPrimitiveCalls = 0;
volatile LONG gDrawPrimitiveUPCalls = 0;
volatile LONG gDrawIndexedPrimitiveUPCalls = 0;
LARGE_INTEGER gPerfFrequency {};
LARGE_INTEGER gLastPresentSample {};
LONG gLastPresentFrameSample = 0;
LARGE_INTEGER gLastEndSceneSample {};
LONG gLastEndSceneFrameSample = 0;
LARGE_INTEGER gLastTransformSample {};
LONG gLastTransformCallSample = 0;
LARGE_INTEGER gLastVsConstSample {};
LONG gLastVsConstCallSample = 0;
LONG gLoggedViewTransforms = 0;
LONG gLoggedProjectionTransforms = 0;
LONG gLoggedVsConstRows = 0;
LONG gLoggedVertexShaderChanges = 0;
LONG gLoggedPixelShaderChanges = 0;
constexpr UINT MaxTrackedVsConstants = 256;
float gVsConstants[MaxTrackedVsConstants][4] {};
bool gHaveVsConstants[MaxTrackedVsConstants] {};
IDirect3DVertexShader9* gActiveVertexShader = nullptr;
IDirect3DPixelShader9* gActivePixelShader = nullptr;
std::uint32_t gActiveVertexShaderHash = 0;
std::uint32_t gActivePixelShaderHash = 0;
std::uint8_t gActiveVertexShaderSha256[32] {};
UINT gActiveVertexShaderByteCount = 0;
fnvxr::stereo::Matrix4 gBaseView {};
fnvxr::stereo::Matrix4 gBaseProjection {};
fnvxr::stereo::EyeMatrices gEyeMatrices {};
float gIpdMeters = fnvxr::stereo::DefaultIpdMeters;
float gIpdGameUnits = 2.5f;
float gGameUnitsPerMeter = fnvxr::stereo::DefaultGameUnitsPerMeter;
bool gHaveView = false;
bool gHaveProjection = false;
LONG gLoggedEyeMatrices = 0;
LONG gLoggedEyePoseScale = 0;
bool gNativeStereoEnabled = false;
bool gNativeSingleTraversalReplayEnabled = false;
bool gNativeStereoHookInstalled = false;
bool gNativeStereoRuntimeDisabled = false;
void** gHookedDeviceVtable = nullptr;
IUnknown* gHookedDeviceIdentity = nullptr;
std::uint64_t gCriticalDeviceHookMask = 0;
std::uint64_t gExpectedCriticalDeviceHookMask = 0;
bool gCriticalDeviceHooksReady = false;
volatile LONG gRenderOwnerThreadId = 0;
volatile LONG gRenderThreadViolation = 0;
volatile LONG gRenderShutdownRequested = 0;
SRWLOCK gRenderPublishCommitLock = SRWLOCK_INIT;
volatile LONG gStateBlockRecording = 0;
volatile LONG gD3DQueryObjectsObserved = 0;
DWORD gDeviceBehaviorFlags = 0;
bool gInNativeStereoHook = false;
bool gNativePostprocessFanoutActive = false;
bool gNativePipelineTraceThisPair = false;
bool gNativePipelineTracePostprocessWindow = false;
void* gNativeStereoTrampoline = nullptr;
void* gNativeCameraMatrixTrampoline = nullptr;
void* gNativeGeometrySubmitTrampoline = nullptr;
IDirect3DDevice9* gNativeStereoDevice = nullptr;
LONG gNativeStereoPairsThisPresent = 0;
LONG gNativeSingleTraversalFramesThisPresent = 0;
LONG gNativeStereoRenderPairSequence = 0;
volatile LONG gNativeStereoAuditPairCount = 0;
volatile LONG gNativeActiveEye = -1;
float gNativeSingleTraversalIpdUnits = 0.0f;
float gNativeSingleTraversalFov[2][4] {};
bool gNativeSingleTraversalViewProjectionValid = false;
fnvxr::stereo::Matrix4 gNativeSingleTraversalCenterViewProjection {};
fnvxr::stereo::Matrix4 gNativeSingleTraversalEyeViewProjection[2] {};
LONG gLoggedNativeStereoPair = 0;
LONG gLoggedNativeStereoFailure = 0;
LONG gLoggedNativeCenterCamera = 0;
LONG gLoggedPresentPerf = 0;
LONG gOriginalDoRenderCallsThisPresent = 0;
LONGLONG gOriginalDoRenderTicksThisPresent = 0;
LONG gNativeDoRenderHookEntriesThisPresent = 0;
LONG gNativeDoRenderStereoAttemptsThisPresent = 0;
LONG gNativeDoRenderGateMaskThisPresent = 0;
LONG gNativeLastDoRenderHookPresentFrame = -1;
LONG gNativeHookSilentGameplayFrames = 0;
LONG gLoggedNativeHookContinuity = 0;
LONG gLoggedNativePassEquivalence = 0;
LONG gNativePassRejectedThisPresent = 0;
LONG gNativePassMismatchThisPresent = 0;
LONG gNativeEquivalentPairsInSequence = 0;
LONG gNativeEquivalentLastPresentFrame = -2;
bool gNativePassEquivalenceWasBlocked = false;
LONG gNativeCameraMatrixOverrides = 0;
LONG gNativeCameraMatrixOverridesThisPair[2] {};
LONG gNativeGeometryRearmsThisPair[2] {};
LONG gLoggedNativeGeometryRearm = 0;
LONG gNativePostprocessFanoutDrawsThisPresent = 0;
LONG gNativePostprocessFinalWritesThisPresent = 0;
LONG gLoggedNativePostprocessFanout = 0;
LONG gNativePipelineTraceEvents[3] {};
LONG gNativePipelineTracePostDraws = 0;
LONG gNativePipelineTraceEyeDraws[2] {};
DWORD gNativePipelineStereoSamplerMask = 0;
LONG gNativeRenderTargetSetOrdinal[2] {};
bool gStereoReplayEnabled = false;
bool gStereoReadbackEnabled = false;
bool gInStereoReplay = false;
bool gWideWorldReplayEnabled = false;
bool gIsMain3DSceneActive = false;
bool gStereoFrameReadyToPublish = false;
IDirect3DSurface9* gMain3DSceneRenderTarget = nullptr;
LONG gLoggedShaderStereo = 0;
LONG gLoggedShaderStereoProof = 0;
LONG gLoggedShaderWvpReplay = 0;
LONG gLoggedShaderSanityOffset = 0;
IDirect3DTexture9* gLeftEyeTexture = nullptr;
IDirect3DTexture9* gRightEyeTexture = nullptr;
IDirect3DSurface9* gLeftEyeSurface = nullptr;
IDirect3DSurface9* gRightEyeSurface = nullptr;
IDirect3DSurface9* gLeftEyeDepth = nullptr;
IDirect3DSurface9* gRightEyeDepth = nullptr;
IDirect3DSurface9* gLeftEyeReadback = nullptr;
IDirect3DSurface9* gRightEyeReadback = nullptr;
IDirect3DSurface9* gBestLeftEyeReadback = nullptr;
IDirect3DSurface9* gBestRightEyeReadback = nullptr;
IDirect3DSurface9* gProbeLeftEyeReadback = nullptr;
IDirect3DSurface9* gProbeRightEyeReadback = nullptr;
IDirect3DTexture9* gWideWorldTexture = nullptr;
IDirect3DSurface9* gWideWorldSurface = nullptr;
IDirect3DSurface9* gWideWorldDepth = nullptr;
IDirect3DSurface9* gWideWorldReadback = nullptr;
bool gStereoTargetsAliasTwin = false;
UINT gStereoTargetWidth = 0;
UINT gStereoTargetHeight = 0;
D3DFORMAT gStereoTargetFormat = D3DFMT_UNKNOWN;
UINT gProbeStereoTargetWidth = 0;
UINT gProbeStereoTargetHeight = 0;
D3DFORMAT gProbeStereoTargetFormat = D3DFMT_UNKNOWN;
LONG gLoggedStereoReplay = 0;
LONG gLoggedD3d9EyeTarget = 0;
LONG gLoggedStereoPresentState = 0;
LONG gLoggedResourceGraphTelemetry = 0;
LONG gLoggedShaderStereoSkipped = 0;
LONG gLoggedStereoNoReplayDraws = 0;
LONG gLoggedStereoReplaySkipped = 0;
LONG gLoggedGameplayHudSuppressed = 0;
LONG gLoggedStereoReplayShaderState = 0;
LONG gLoggedStereoReplayImmediateDiff = 0;
LONG gLoggedStereoReplayStateBlock = 0;
LONG gLoggedStereoReplayDepthPrime = 0;
LONG gLoggedStereoReplayCullLimit = 0;
LONG gLoggedStereoReplayDrawResult = 0;
LONG gLoggedStereoClearMirror = 0;
LONG gLoggedD3d9TelemetryHammerConfig = 0;
LONG gLastReadbackFrame = 0;
LONG gStereoReplayDrawsThisPresent = 0;
LONG gStereoReplayBackbufferDrawsThisPresent = 0;
LONG gStereoReplayOffscreenDrawsThisPresent = 0;
LONG gWideWorldReplayDrawsThisPresent = 0;
LONG gLoggedWideWorldReplay = 0;
LONG gLoggedWideWorldNoReplayDraws = 0;
LONG gLoggedWideWorldReplaySkipped = 0;
LONG gLoggedWideWorldShader = 0;
LONG gLoggedWideWorldTargets = 0;
LONG gWideWorldLastPrimeFrame = 0;
LONG gConsecutiveIdenticalStereoWorldFrames = 0;
LONG gStereoIdenticalDisarmed = 0;
UINT gStereoCollapseAuditPrevDiff = 0;
UINT gStereoCollapseAuditPrevSamples = 0;
std::uint32_t gStereoCollapseAuditPrevLeftHash = 0;
std::uint32_t gStereoCollapseAuditPrevRightHash = 0;
LONG gStereoCollapseAuditPrevDraw = 0;
LONG gLoggedStereoCollapseAudit = 0;
struct StereoShaderPairHash
{
    std::uint32_t vertex = 0;
    std::uint32_t pixel = 0;
};
constexpr UINT MaxStereoCollapseSkipShaderPairs = 32;
StereoShaderPairHash gStereoCollapseSkipShaderPairs[MaxStereoCollapseSkipShaderPairs] {};
LONG gStereoCollapseSkipShaderPairCount = 0;
LONG gLoggedStereoCollapseSkipLearned = 0;
StereoShaderPairHash gConfiguredStereoSkipShaderPairs[MaxStereoCollapseSkipShaderPairs] {};
LONG gConfiguredStereoSkipShaderPairCount = 0;
LONG gConfiguredStereoSkipShaderPairsLoaded = 0;
constexpr UINT MaxShaderStereoAllowVertexHashes = 32;
std::uint32_t gShaderStereoAllowVertexHashes[MaxShaderStereoAllowVertexHashes] {};
LONG gShaderStereoAllowVertexHashCount = 0;
LONG gShaderStereoAllowVertexHashesLoaded = 0;
struct ShaderWvpContract
{
    std::uint32_t vertexHash = 0;
    std::uint8_t vertexSha256[32] {};
    UINT byteCount = 0;
    UINT startRegister = 0;
    bool columnVector = true;
};
constexpr UINT MaxShaderWvpContracts = 64;
ShaderWvpContract gShaderWvpContracts[MaxShaderWvpContracts] {};
LONG gShaderWvpContractCount = 0;
LONG gShaderWvpContractsLoaded = 0;
LONG gShaderWorldDrawCandidates = 0;
LONG gShaderWorldDraws = 0;
LONG gShaderContractCoveredWorldDraws = 0;
LONG gStrictEyeTargetDraws = 0;
LONG gStrictEyeTargetProvenBothEyeDraws = 0;
LONG gStrictEyeTargetClears = 0;
LONG gStrictEyeTargetProvenBothEyeClears = 0;
LONG gStrictEyeTargetCopies = 0;
LONG gStrictEyeTargetProvenBothEyeCopies = 0;
LONG gStrictEyeTargetFullInitializations = 0;
LONG gStrictEyeTargetUnprovenWrites = 0;
bool gStrictEquivalentClearSeenThisTraversal = false;
bool gStrictDrawSeenThisTraversal = false;
std::uint64_t gStereoReplayTransactionGeneration = 0;
bool gPrimaryBackBufferLockable = false;
LONG gShaderExcludedUserPrimitiveDraws = 0;
LONG gShaderExcludedAuxiliaryTargetDraws = 0;
struct ShaderWvpDiscoveryIdentity
{
    std::uint32_t vertexHash = 0;
    std::uint8_t vertexSha256[32] {};
    UINT byteCount = 0;
};
constexpr UINT MaxShaderWvpDiscoveryIdentities = 128;
ShaderWvpDiscoveryIdentity gShaderWvpDiscoveryIdentities[MaxShaderWvpDiscoveryIdentities] {};
LONG gShaderWvpDiscoveryIdentityCount = 0;
LONG gLoggedShaderStereoRejectedByHash = 0;
LONG gLoggedStereoVisualCoverageRejected = 0;
LONG gConsecutiveStereoVisualCoverageFrames = 0;
LONG gLoggedStereoVisualCoverageWarmup = 0;
LONG gLoggedRetainedInvalidStereoWorld = 0;
LONG gLoggedStrictStereoTargetGate = 0;
bool gHavePublishedValidStereoWorldFrame = false;
UINT gBestStereoDiffThisPresent = 0;
UINT gBestStereoSamplesThisPresent = 0;
std::uint32_t gBestStereoLeftHashThisPresent = 0;
std::uint32_t gBestStereoRightHashThisPresent = 0;
bool gHaveBestStereoSnapshotThisPresent = false;
LONG gLoggedBestStereoSnapshot = 0;
struct StereoSurfaceTwin
{
    IDirect3DSurface9* original = nullptr;
    IDirect3DSurface9* left = nullptr;
    IDirect3DSurface9* right = nullptr;
    IDirect3DTexture9* originalTexture = nullptr;
    IDirect3DTexture9* leftTexture = nullptr;
    IDirect3DTexture9* rightTexture = nullptr;
    D3DSURFACE_DESC desc {};
    bool depth = false;
    LONG lastUsedFrame = 0;
    LONG lastDepthPrimeFrame = -1;
    StereoSurfaceTwin* lastDepthPrimeTwin = nullptr;
    std::uint64_t equivalentInitializationGeneration = 0;
    std::uint64_t leftValidGeneration = 0;
    std::uint64_t rightValidGeneration = 0;
};
struct NativeFullSizeTargetCandidate
{
    IDirect3DSurface9* original = nullptr;
    StereoSurfaceTwin* twin = nullptr;
    D3DSURFACE_DESC desc {};
    bool seen[2] {};
    bool snapshotted[2] {};
    LONG lastSetOrdinal[2] {};
};
constexpr UINT MaxTrackedSamplers = 16;
enum class NativePostprocessDrawKind : std::uint8_t
{
    DrawPrimitive,
    DrawIndexedPrimitive,
};
struct NativePostprocessDrawRecord
{
    NativePostprocessDrawKind kind = NativePostprocessDrawKind::DrawPrimitive;
    IDirect3DStateBlock9* stateBlock = nullptr;
    IDirect3DSurface9* originalTarget = nullptr;
    IDirect3DSurface9* originalDepth = nullptr;
    StereoSurfaceTwin* targetTwin = nullptr;
    StereoSurfaceTwin* depthTwin = nullptr;
    IDirect3DBaseTexture9* textures[MaxTrackedSamplers] {};
    D3DVIEWPORT9 viewport {};
    RECT scissor {};
    bool haveViewport = false;
    bool haveScissor = false;
    D3DPRIMITIVETYPE primitiveType = D3DPT_TRIANGLELIST;
    UINT startVertex = 0;
    UINT primitiveCount = 0;
    INT baseVertexIndex = 0;
    UINT minVertexIndex = 0;
    UINT numVertices = 0;
    UINT startIndex = 0;
};
constexpr UINT MaxStereoSurfaceTwins = 512;
constexpr UINT MaxNativeFullSizeTargetCandidates = 32;
constexpr UINT MaxNativePostprocessDrawRecords = 64;
StereoSurfaceTwin gStereoSurfaceTwins[MaxStereoSurfaceTwins] {};
NativeFullSizeTargetCandidate gNativeFullSizeTargetCandidates[MaxNativeFullSizeTargetCandidates] {};
NativePostprocessDrawRecord gNativePostprocessDrawRecords[MaxNativePostprocessDrawRecords] {};
bool gNativePostprocessRecording = false;
bool gNativePostprocessRecordUnsupported = false;
LONG gNativePostprocessRecordedDrawCount = 0;
LONG gNativePostprocessReplayedDrawCount = 0;
StereoSurfaceTwin* gNativeResolvedTwinForEye[2] {};
IDirect3DSurface9* gNativeResolvedReadback[2] {};
D3DSURFACE_DESC gNativeResolvedDesc {};
LONG gNativeResolvedCopies[2] {};
LONG gLoggedNativeResolvedAudit = 0;
D3DMATRIX gNativeObservedView[2] {};
D3DMATRIX gNativeObservedProjection[2] {};
LONG gNativeObservedViewCalls[2] {};
LONG gNativeObservedProjectionCalls[2] {};
std::uint32_t gNativeVsConstantStreamHash[2] { 2166136261u, 2166136261u };
LONG gNativeVsConstantCalls[2] {};
LONG gNativeDrawCalls[2] {};

constexpr LONG NativeDoRenderGateDisabled = 1 << 0;
constexpr LONG NativeDoRenderGateRuntimeDisabled = 1 << 1;
constexpr LONG NativeDoRenderGateReentrant = 1 << 2;
constexpr LONG NativeDoRenderGateNoDevice = 1 << 3;
constexpr LONG NativeDoRenderGateRendererArgument = 1 << 4;
constexpr LONG NativeDoRenderGateUi = 1 << 5;
constexpr LONG NativeDoRenderGateAlreadyPaired = 1 << 6;
constexpr LONG NativeDoRenderGateFirstPerson = 1 << 7;
constexpr LONG NativeDoRenderGatePose = 1 << 8;
constexpr LONG NativeDoRenderGateCamera = 1 << 9;
constexpr LONG NativeDoRenderGateTargets = 1 << 10;
constexpr LONG NativeDoRenderGateCellTransition = 1 << 11;
constexpr LONG NativeDoRenderGateHookIntegrity = 1 << 12;
IDirect3DBaseTexture9* gActiveTextures[MaxTrackedSamplers] {};
LONG gLoggedStereoSurfaceTwinCreated = 0;
LONG gLoggedStereoSurfaceTwinFailed = 0;
LONG gLoggedStereoStretchMirrored = 0;
LONG gLoggedStereoTextureTwinBound = 0;
LONG gLoggedStereoProbeReadbackCreated = 0;
LONG gLoggedStereoProbeReadbackFailed = 0;
LONG gLoggedStereoTargetProbeDiff = 0;
constexpr DWORD SharedVideoMagic = fnvxr::shared::D3D9FrameSharedMagic;
constexpr DWORD SharedStereoMagic = fnvxr::shared::D3D9StereoFrameSharedMagic;
constexpr DWORD SharedVrPoseMagic = fnvxr::shared::VrPoseSharedMagic;
constexpr DWORD SharedVrPoseVersion = fnvxr::shared::VrPoseSharedVersion;
constexpr DWORD SharedCameraMagic = fnvxr::shared::CameraSharedMagic;
constexpr DWORD SharedCameraVersion = fnvxr::shared::CameraSharedVersion;
constexpr DWORD SharedRuntimeMagic = fnvxr::shared::RuntimeSharedMagic;
constexpr DWORD SharedRuntimeVersion = fnvxr::shared::RuntimeSharedVersion;
constexpr DWORD SharedPlayerMagic = fnvxr::shared::PlayerSharedMagic;
constexpr DWORD SharedPlayerVersion = fnvxr::shared::PlayerSharedVersion;
constexpr std::uintptr_t MenuVisibilityArrayAddress = 0x011F308F;
constexpr std::uintptr_t IsMenuModeAddress = 0x00702360;
constexpr UINT kMenuTypeInventory = 0x3EA;
constexpr UINT kMenuTypeStats = 0x3EB;
constexpr UINT kMenuTypeHUDMain = 0x3EC;
constexpr UINT kMenuTypeLoading = 0x3EF;
// xNVSE GameUI.h: 0x3F0 is ContainerMenu; DialogMenu is 0x3F1.
constexpr UINT kMenuTypeDialog = 0x3F1;
constexpr UINT kMenuTypeStart = 0x3F5;
constexpr UINT kMenuTypeMap = 0x3FF;
constexpr UINT kMenuTypeRaceSex = 0x40C;
constexpr UINT kMenuTypeVats = 0x420;
constexpr UINT SharedVideoMaxWidth = fnvxr::shared::D3D9SharedFrameMaxWidth;
constexpr UINT SharedVideoMaxHeight = fnvxr::shared::D3D9SharedFrameMaxHeight;

using SharedVideoHeader = fnvxr::shared::SharedD3D9FrameHeader;
using SharedStereoHeader = fnvxr::shared::SharedD3D9StereoFrameHeader;
using fnvxr::shared::SharedVrPoseState;
using fnvxr::shared::SharedVrOriginState;
using fnvxr::shared::SharedCameraState;
using fnvxr::shared::SharedRuntimeState;
using fnvxr::shared::SharedPlayerState;

struct NamedProducerLease
{
    HANDLE mutex = nullptr;
    HANDLE ownerThread = nullptr;
    DWORD ownerThreadId = 0;
    bool owned = false;
};

HANDLE gSharedVideoMapping = nullptr;
std::uint8_t* gSharedVideoView = nullptr;
NamedProducerLease gSharedVideoProducerLease {};
IDirect3DSurface9* gSharedVideoReadback = nullptr;
UINT gSharedVideoWidth = 0;
UINT gSharedVideoHeight = 0;
D3DFORMAT gSharedVideoFormat = D3DFMT_UNKNOWN;
LONG gSharedVideoCaptures = 0;
LONG gSharedVideoSkippedByThrottle = 0;
LONG gSharedVideoPerfLogs = 0;
LARGE_INTEGER gLastSharedVideoCaptureSample {};
HANDLE gSharedStereoMapping = nullptr;
std::uint8_t* gSharedStereoView = nullptr;
NamedProducerLease gSharedStereoProducerLease {};
std::uint64_t gSharedStereoRendererProducerEpoch = 0;
LONG gSharedStereoCaptures = 0;
LONG gSharedStereoPerfLogs = 0;
HANDLE gSharedWorldVideoMapping = nullptr;
std::uint8_t* gSharedWorldVideoView = nullptr;
NamedProducerLease gSharedWorldVideoProducerLease {};
LONG gSharedWorldVideoCaptures = 0;
HANDLE gSharedVrPoseMapping = nullptr;
SharedVrPoseState* gSharedVrPose = nullptr;
HANDLE gSharedVrOriginMapping = nullptr;
SharedVrOriginState* gSharedVrOrigin = nullptr;
NamedProducerLease gSharedVrOriginProducerLease {};
HANDLE gSharedCameraMapping = nullptr;
SharedCameraState* gSharedCamera = nullptr;
HANDLE gSharedRuntimeMapping = nullptr;
SharedRuntimeState* gSharedRuntime = nullptr;
HANDLE gSharedPlayerMapping = nullptr;
SharedPlayerState* gSharedPlayer = nullptr;
fnvxr::stereo::Matrix4 gSharedCameraView {};
bool gSharedCameraActive = false;
bool gSharedCameraRawActive = false;
bool gSharedCameraMatrixUsable = false;
bool gHaveVrPoseOrigin = false;
bool gNativeGameplayPoseOriginLatched = false;
bool gNativeViewOriginValid = false;
std::uint32_t gNativeLastRecenterRequestId = 0;
float gNativeViewOriginRot[4] { 0.0f, 0.0f, 0.0f, 1.0f };
float gVrPoseOrigin[3] {};
float gVrPoseOriginRot[4] { 0.0f, 0.0f, 0.0f, 1.0f };
LONG gVrPoseOriginPoseSequence = 0;
std::uint64_t gVrPoseOriginPoseFrame = 0;
std::uint32_t gVrPoseOriginGeneration = 0;
std::uint64_t gVrPoseOriginProducerEpoch = 0;
float gVrWorldYawCos = 1.0f;
float gVrWorldYawSin = 0.0f;
float gVrWorldYawHalfCos = 1.0f;
float gVrWorldYawHalfSin = 0.0f;
float gLatestHmdDelta[3] {};
float gLatestHmdRotDelta[4] { 0.0f, 0.0f, 0.0f, 1.0f };
float gLatestHeadLocalMeters[3] {};
float gLatestLeftEyeLocalMeters[3] { -0.032f, 0.0f, 0.0f };
float gLatestRightEyeLocalMeters[3] { 0.032f, 0.0f, 0.0f };
float gLatestRawIpdMeters = fnvxr::stereo::DefaultIpdMeters;
bool gHaveVrBodyFrame = false;
SharedVrPoseState gLatestVrPoseSnapshot {};
LONG gLatestVrPoseSnapshotSequence = 0;
std::int64_t gLatestVrPoseDisplayTime = 0;
bool gLatestVrPoseSnapshotValid = false;
LARGE_INTEGER gLatestVrPoseAcceptedAt {};
SharedVrPoseState gNativeStereoRenderedPoseSnapshot {};
LONG gNativeStereoRenderedPoseSequence = 0;
std::int64_t gNativeStereoRenderedDisplayTime = 0;
bool gNativeStereoRenderedPoseValid = false;
LONG gLastVrPoseSequence = 0;
std::uint64_t gLastVrPoseFrame = 0;
std::uint32_t gLastVrReferenceSpaceGeneration = 0;
std::uint64_t gLastVrProducerEpoch = 0;
std::uint32_t gPublishedVrOriginGeneration = 0;
bool gPublishedVrOriginActive = false;
bool gPublishedVrOriginCommitted = false;
std::uint64_t gLastHeadBodyInvariantPoseFrame = 0;
LONG gLastCameraSequence = 0;
LONG gLastRuntimeSequence = 0;
SharedCameraState gLatestSharedCameraSnapshot {};
LONG gLatestSharedCameraSnapshotSequence = 0;
bool gLatestSharedCameraSnapshotValid = false;
LARGE_INTEGER gLatestSharedCameraAcceptedAt {};
SharedPlayerState gLatestSharedPlayerSnapshot {};
LONG gLatestSharedPlayerSnapshotSequence = 0;
bool gLatestSharedPlayerSnapshotValid = false;
std::uint32_t gNativeStableCellFormId = 0;
std::uint64_t gNativeCellChangePlayerFrame = 0;
std::uint64_t gNativeDoubleTraversalHoldUntilPlayerFrame = 0;
bool gNativeCellTransitionReady = false;
LONG gNativeSharedPlayerReadMisses = 0;
LONG gLoggedNativeCellTransition = 0;
LONG gLoggedSharedCamera = 0;
LONG gLoggedSharedRuntime = 0;
LONG gLoggedSharedRuntimeMissing = 0;
LONG gLoggedSharedRuntimeInvalid = 0;
LONG gLoggedSharedVrPose = 0;
LONG gLoggedSharedVrPoseInvalid = 0;
LONG gLoggedNativeFirstPersonGate = 0;
LONG gLoggedMatrixAudit = 0;
LONG gLastMatrixAuditFrame = 0;
INIT_ONCE gD3D9ProxyInitializeOnce = INIT_ONCE_STATIC_INIT;

bool loadRealD3D9();
void logLine(const char* text);
void loadStereoConfig();
bool ensureD3D9ProxyInitialized();
float absFloat(float value);
bool currentProjectionLooksScreenSpace();
bool currentViewLooksScreenSpace();
bool currentDrawSamplesStereoTwin();
void logMatrixAuditFrame();
bool readEnvBool(const char* name, bool fallback);
bool telemetryHammerEnabled();
bool shouldTelemetryHammerLog(LONG count, const char* strideEnv, LONG fallbackStride);
void installNativeStereoEngineHook(IDirect3DDevice9* device);
bool buildLogPath(char* path, size_t pathSize, const char* leafName);
bool sameComIdentity(IUnknown* left, IUnknown* right);
bool criticalDeviceHooksIntact(IDirect3DDevice9* device);

bool stereoReplayTransactionAllowed()
{
    return InterlockedCompareExchange(&gStateBlockRecording, 0, 0) == 0
        && (!gNativeSingleTraversalReplayEnabled
            || (gInNativeStereoHook && gNativeActiveEye == 2));
}

double secondsBetween(const LARGE_INTEGER& start, const LARGE_INTEGER& end)
{
    if (gPerfFrequency.QuadPart == 0)
        QueryPerformanceFrequency(&gPerfFrequency);

    return static_cast<double>(end.QuadPart - start.QuadPart) / static_cast<double>(gPerfFrequency.QuadPart);
}

struct ScopedReplayStateBlock
{
    IDirect3DStateBlock9* block = nullptr;
    bool captured = false;
    bool restoreAttempted = false;
    bool restored = false;

    ScopedReplayStateBlock(IDirect3DDevice9* device, const char* context)
    {
        if (!device)
            return;

        const HRESULT createResult = device->CreateStateBlock(D3DSBT_ALL, &block);
        if (SUCCEEDED(createResult) && block)
            captured = SUCCEEDED(block->Capture());

        const LONG logCount = InterlockedIncrement(&gLoggedStereoReplayStateBlock);
        const bool hammerLog =
            shouldTelemetryHammerLog(logCount, "FNVXR_D3D9_STATEBLOCK_TELEMETRY_STRIDE", 1);
        if (logCount <= 12 || logCount % 300 == 0 || !captured || hammerLog)
        {
            char message[256] {};
            sprintf_s(
                message,
                "stereo replay stateblock context=%s create=0x%08lx captured=%d count=%ld",
                context ? context : "unknown",
                static_cast<unsigned long>(createResult),
                captured ? 1 : 0,
                logCount);
            logLine(message);
        }
        if (hammerLog)
        {
            char event[256] {};
            sprintf_s(
                event,
                "{\"event\":\"fnvxrD3d9StateBlock\",\"frame\":%ld,\"count\":%ld,\"context\":\"%s\","
                "\"create\":\"0x%08lx\",\"captured\":%d}",
                static_cast<LONG>(gPresentFrames),
                logCount,
                context ? context : "unknown",
                static_cast<unsigned long>(createResult),
                captured ? 1 : 0);
            logLine(event);
        }
    }

    bool restore()
    {
        restoreAttempted = true;
        if (!captured || !block)
            return false;
        const HRESULT result = block->Apply();
        if (SUCCEEDED(result))
        {
            restored = true;
            return true;
        }
        char message[160] {};
        sprintf_s(
            message,
            "stereo replay stateblock apply failed result=0x%08lx",
            static_cast<unsigned long>(result));
        logLine(message);
        return false;
    }

    ~ScopedReplayStateBlock()
    {
        if (captured && block && !restoreAttempted)
        {
            restoreAttempted = true;
            restored = SUCCEEDED(block->Apply());
            if (!restored)
            {
                InterlockedIncrement(&gStrictEyeTargetUnprovenWrites);
                logLine("stereo replay stateblock emergency restore failed");
            }
        }
        if (block)
            block->Release();
    }
};

void logFrameRate(const char* label, volatile LONG& counter, LARGE_INTEGER& lastSample, LONG& lastFrameSample)
{
    const LONG frame = InterlockedIncrement(&counter);
    LARGE_INTEGER now {};
    QueryPerformanceCounter(&now);

    if (lastSample.QuadPart == 0)
    {
        lastSample = now;
        lastFrameSample = frame;
        logLine(label);
        return;
    }

    const double elapsed = secondsBetween(lastSample, now);
    if (frame <= 5 || elapsed >= 5.0)
    {
        const LONG deltaFrames = frame - lastFrameSample;
        const double fps = elapsed > 0.0 ? static_cast<double>(deltaFrames) / elapsed : 0.0;
        char message[192] {};
        sprintf_s(message, "%s frame=%ld fps=%.2f", label, frame, fps);
        logLine(message);
        lastSample = now;
        lastFrameSample = frame;
    }
}

const char* transformName(D3DTRANSFORMSTATETYPE state)
{
    switch (state)
    {
    case D3DTS_VIEW:
        return "VIEW";
    case D3DTS_PROJECTION:
        return "PROJECTION";
    case D3DTS_TEXTURE0:
        return "TEXTURE0";
    case D3DTS_TEXTURE1:
        return "TEXTURE1";
    case D3DTS_TEXTURE2:
        return "TEXTURE2";
    case D3DTS_TEXTURE3:
        return "TEXTURE3";
    case D3DTS_TEXTURE4:
        return "TEXTURE4";
    case D3DTS_TEXTURE5:
        return "TEXTURE5";
    case D3DTS_TEXTURE6:
        return "TEXTURE6";
    case D3DTS_TEXTURE7:
        return "TEXTURE7";
    default:
        if (state >= D3DTS_WORLD && state < D3DTS_WORLD + 256)
            return "WORLD";
        return "UNKNOWN";
    }
}

void logTransformMatrix(const char* label, D3DTRANSFORMSTATETYPE state, const D3DMATRIX* matrix)
{
    if (!matrix)
        return;

    char message[512] {};
    sprintf_s(
        message,
        "%s state=%s(%u) frame=%ld rows=[%.4f %.4f %.4f %.4f] [%.4f %.4f %.4f %.4f] [%.4f %.4f %.4f %.4f] [%.4f %.4f %.4f %.4f]",
        label,
        transformName(state),
        static_cast<unsigned>(state),
        static_cast<LONG>(gPresentFrames),
        matrix->m[0][0],
        matrix->m[0][1],
        matrix->m[0][2],
        matrix->m[0][3],
        matrix->m[1][0],
        matrix->m[1][1],
        matrix->m[1][2],
        matrix->m[1][3],
        matrix->m[2][0],
        matrix->m[2][1],
        matrix->m[2][2],
        matrix->m[2][3],
        matrix->m[3][0],
        matrix->m[3][1],
        matrix->m[3][2],
        matrix->m[3][3]);
    logLine(message);
}

void logHookCadence(
    const char* label,
    LONG call,
    LARGE_INTEGER& lastSample,
    LONG& lastCallSample,
    double intervalSeconds)
{
    LARGE_INTEGER now {};
    QueryPerformanceCounter(&now);

    if (lastSample.QuadPart == 0)
    {
        lastSample = now;
        lastCallSample = call;
        return;
    }

    const double elapsed = secondsBetween(lastSample, now);
    if (elapsed >= intervalSeconds)
    {
        const LONG deltaCalls = call - lastCallSample;
        const double callsPerSecond = elapsed > 0.0 ? static_cast<double>(deltaCalls) / elapsed : 0.0;
        char message[192] {};
        sprintf_s(message, "%s calls=%ld cps=%.2f", label, call, callsPerSecond);
        logLine(message);
        lastSample = now;
        lastCallSample = call;
    }
}

float readEnvFloat(const char* name, float fallback)
{
    char value[64] {};
    if (GetEnvironmentVariableA(name, value, static_cast<DWORD>(sizeof(value))) == 0)
        return fallback;

    char* end = nullptr;
    const float parsed = strtof(value, &end);
    return end && *end == '\0' ? parsed : fallback;
}

bool readRawEnvBool(const char* name, bool fallback)
{
    char value[16] {};
    if (GetEnvironmentVariableA(name, value, static_cast<DWORD>(sizeof(value))) == 0)
        return fallback;

    return value[0] == '1' || value[0] == 't' || value[0] == 'T' || value[0] == 'y' || value[0] == 'Y';
}

bool telemetryHammerEnabled()
{
    return readRawEnvBool(
        "FNVXR_D3D9_TELEMETRY_HAMMER",
        readRawEnvBool("FNVXR_TELEMETRY_HAMMER", false));
}

LONG telemetryHammerStride(const char* name, LONG fallback)
{
    const LONG parsed = static_cast<LONG>(readEnvFloat(name, static_cast<float>(fallback)));
    return parsed <= 0 ? 1 : parsed;
}

bool shouldTelemetryHammerLog(LONG count, const char* strideEnv, LONG fallbackStride)
{
    if (!telemetryHammerEnabled())
        return false;

    const LONG warmup = static_cast<LONG>(readEnvFloat("FNVXR_D3D9_TELEMETRY_HAMMER_WARMUP", 240.0f));
    if (warmup > 0 && count <= warmup)
        return true;

    const LONG stride = telemetryHammerStride(strideEnv, fallbackStride);
    return stride > 0 && (count % stride) == 0;
}

constexpr bool StereoWorldProductionProofComplete = false;
// Independent integration fuse: the retained D3D replay code is not allowed
// to become WorldStereo merely because its historical proof fuse is edited.
// Production must enter through the engine transaction/product controller.
constexpr bool ProductWorldStereoIntegrationComplete = false;

bool stereoWorldRuntimeRequested()
{
    return !readRawEnvBool("FNVXR_DISABLE_STEREO_WORLD", false);
}

bool stereoWorldRuntimeEnabled()
{
    // This source-level gate is independent of launcher policy. Environment
    // variables alone cannot activate the unsafe traversal/replay path.
    return fnvxr::d3d9::ProductionRendererAuthorized
        && StereoWorldProductionProofComplete
        && ProductWorldStereoIntegrationComplete
        && stereoWorldRuntimeRequested();
}

bool stereoProofModeArmed()
{
    return fnvxr::d3d9::ProductionRendererAuthorized
        && StereoWorldProductionProofComplete
        && ProductWorldStereoIntegrationComplete
        && (readRawEnvBool("FNVXR_D3D9_SHARED_STEREO", false)
        || readRawEnvBool("FNVXR_D3D9_NATIVE_STEREO", false)
        || readRawEnvBool("FNVXR_D3D9_NATIVE_SINGLE_TRAVERSAL_REPLAY", false));
}

bool runProfileIs(const char* expected)
{
    if (!expected)
        return false;

    char value[32] {};
    return GetEnvironmentVariableA("FNVXR_RUN_PROFILE", value, static_cast<DWORD>(sizeof(value))) != 0
        && _stricmp(value, expected) == 0;
}

bool rockSolidProfile()
{
    return runProfileIs("rock-solid");
}

bool retailSidecarProfile()
{
    return runProfileIs("retail-sidecar") || runProfileIs("openxr-sidecar");
}

bool readEnvBool(const char* name, bool fallback)
{
    char value[16] {};
    if (GetEnvironmentVariableA(name, value, static_cast<DWORD>(sizeof(value))) == 0)
    {
        if (retailSidecarProfile())
        {
            if (_stricmp(name, "FNVXR_D3D9_USE_SHARED_CAMERA_VIEW") == 0)
                return true;
            if (_stricmp(name, "FNVXR_D3D9_CAPTURE_WORLD_DURING_UI") == 0)
                return false;
            if (_stricmp(name, "FNVXR_D3D9_ACCEPT_RAW_CAMERA_MATRIX") == 0)
                return false;
            if (_stricmp(name, "FNVXR_D3D9_REQUIRE_SHARED_CAMERA_FOR_WORLD") == 0)
                return false;
        }
        if (rockSolidProfile())
        {
            if (_stricmp(name, "FNVXR_D3D9_USE_SHARED_CAMERA_VIEW") == 0)
                return true;
            if (_stricmp(name, "FNVXR_D3D9_CAPTURE_WORLD_DURING_UI") == 0)
                return true;
            if (_stricmp(name, "FNVXR_D3D9_ACCEPT_RAW_CAMERA_MATRIX") == 0)
                return false;
            if (_stricmp(name, "FNVXR_D3D9_APPLY_HMD_POSE") == 0)
                return false;
        }
        return fallback;
    }

    return value[0] == '1' || value[0] == 't' || value[0] == 'T' || value[0] == 'y' || value[0] == 'Y';
}

std::uint32_t nextNativeStereoPairBits()
{
    std::uint32_t next = fnvxr::shared::sequencedValueBits(gNativeStereoRenderPairSequence) + 1u;
    return next == 0u ? 1u : next;
}

LONG nextNativeStereoPairSequence()
{
    const std::uint32_t bits = nextNativeStereoPairBits();
    LONG value = 0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

LONG publishNativeStereoPairSequence()
{
    const LONG published = fnvxr::shared::incrementNonzeroSharedCounter(
        gNativeStereoRenderPairSequence);
    for (;;)
    {
        const LONG before = gNativeStereoAuditPairCount;
        if (before >= 13)
            break;
        if (InterlockedCompareExchange(&gNativeStereoAuditPairCount, before + 1, before) == before)
            break;
    }
    return published;
}

bool nativeStereoPairNeedsWarmupAudit(std::uint32_t firstCount)
{
    return static_cast<std::uint32_t>(gNativeStereoAuditPairCount) < firstCount;
}

LONG checkedInterlockedCounterDelta(LONG after, LONG before)
{
    const std::uint32_t delta = fnvxr::shared::sequencedValueBits(after)
        - fnvxr::shared::sequencedValueBits(before);
    return delta <= static_cast<std::uint32_t>(LONG_MAX)
        ? static_cast<LONG>(delta)
        : LONG_MAX;
}

void forceImmediatePresentation(D3DPRESENT_PARAMETERS* params, const char* source)
{
    if (!params || !readEnvBool("FNVXR_D3D9_FORCE_PRESENT_IMMEDIATE", true))
        return;

    const UINT previousInterval = params->PresentationInterval;
    const UINT previousRefresh = params->FullScreen_RefreshRateInHz;
    params->PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    params->FullScreen_RefreshRateInHz = 0;

    char message[256] {};
    sprintf_s(
        message,
        "%s present immediate interval=%u->%u refresh=%u->%u windowed=%d backbuffer=%ux%u",
        source ? source : "D3D9",
        previousInterval,
        params->PresentationInterval,
        previousRefresh,
        params->FullScreen_RefreshRateInHz,
        params->Windowed,
        params->BackBufferWidth,
        params->BackBufferHeight);
    logLine(message);
}

bool resourceGraphTelemetryEnabled()
{
    return readEnvBool("FNVXR_D3D9_RESOURCE_GRAPH_TELEMETRY", false) || gStereoReplayEnabled || gWideWorldReplayEnabled;
}

bool shouldLogResourceGraphCall(LONG call)
{
    return resourceGraphTelemetryEnabled() && (call <= 96 || call % 1000 == 0);
}

void describeSurface(char* buffer, size_t bufferSize, IDirect3DSurface9* surface)
{
    if (!buffer || bufferSize == 0)
        return;

    if (!surface)
    {
        strcpy_s(buffer, bufferSize, "null");
        return;
    }

    D3DSURFACE_DESC desc {};
    if (FAILED(surface->GetDesc(&desc)))
    {
        sprintf_s(buffer, bufferSize, "ptr=%p desc=failed", reinterpret_cast<void*>(surface));
        return;
    }

    sprintf_s(
        buffer,
        bufferSize,
        "ptr=%p %ux%u fmt=%lu usage=0x%lx pool=%u ms=%u quality=%lu",
        reinterpret_cast<void*>(surface),
        desc.Width,
        desc.Height,
        static_cast<unsigned long>(desc.Format),
        static_cast<unsigned long>(desc.Usage),
        static_cast<unsigned>(desc.Pool),
        static_cast<unsigned>(desc.MultiSampleType),
        static_cast<unsigned long>(desc.MultiSampleQuality));
}

void describeBaseTexture(char* buffer, size_t bufferSize, IDirect3DBaseTexture9* texture)
{
    if (!buffer || bufferSize == 0)
        return;

    if (!texture)
    {
        strcpy_s(buffer, bufferSize, "null");
        return;
    }

    const D3DRESOURCETYPE type = texture->GetType();
    if (type != D3DRTYPE_TEXTURE)
    {
        sprintf_s(
            buffer,
            bufferSize,
            "ptr=%p type=%u levels=%lu lod=%lu",
            reinterpret_cast<void*>(texture),
            static_cast<unsigned>(type),
            static_cast<unsigned long>(texture->GetLevelCount()),
            static_cast<unsigned long>(texture->GetLOD()));
        return;
    }

    IDirect3DTexture9* texture2d = nullptr;
    if (FAILED(texture->QueryInterface(__uuidof(IDirect3DTexture9), reinterpret_cast<void**>(&texture2d))) || !texture2d)
    {
        sprintf_s(buffer, bufferSize, "ptr=%p type=texture qi=failed", reinterpret_cast<void*>(texture));
        return;
    }

    D3DSURFACE_DESC desc {};
    const HRESULT descResult = texture2d->GetLevelDesc(0, &desc);
    texture2d->Release();
    if (FAILED(descResult))
    {
        sprintf_s(buffer, bufferSize, "ptr=%p type=texture desc=failed", reinterpret_cast<void*>(texture));
        return;
    }

    sprintf_s(
        buffer,
        bufferSize,
        "ptr=%p type=texture %ux%u levels=%lu fmt=%lu usage=0x%lx pool=%u lod=%lu",
        reinterpret_cast<void*>(texture),
        desc.Width,
        desc.Height,
        static_cast<unsigned long>(texture->GetLevelCount()),
        static_cast<unsigned long>(desc.Format),
        static_cast<unsigned long>(desc.Usage),
        static_cast<unsigned>(desc.Pool),
        static_cast<unsigned long>(texture->GetLOD()));
}

std::uint32_t fnv1aHashBytes(const std::uint8_t* bytes, UINT byteCount)
{
    if (!bytes || byteCount == 0)
        return 0;

    std::uint32_t hash = 2166136261u;
    for (UINT i = 0; i < byteCount; ++i)
    {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash == 0 ? 1 : hash;
}

void fnv1aContinue(std::uint32_t& hash, const void* data, std::size_t byteCount)
{
    if (!data || byteCount == 0)
        return;
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    for (std::size_t index = 0; index < byteCount; ++index)
    {
        hash ^= bytes[index];
        hash *= 16777619u;
    }
    if (hash == 0)
        hash = 1;
}

void dumpD3DShaderBytecode(
    const char* stage,
    std::uint32_t hash,
    const std::uint8_t sha256[32],
    const std::uint8_t* bytes,
    UINT byteCount)
{
    if (!stage || hash == 0 || !sha256 || !bytes || byteCount == 0
        || !readEnvBool("FNVXR_D3D9_DUMP_SHADER_BYTECODE", false))
    {
        return;
    }

    char shaHex[65] {};
    for (UINT index = 0; index < 32; ++index)
        sprintf_s(shaHex + index * 2, sizeof(shaHex) - index * 2, "%02x", sha256[index]);
    char leafName[128] {};
    sprintf_s(leafName, "fnvxr_shader_%s_%s.bin", stage, shaHex);
    char path[MAX_PATH] {};
    if (!buildLogPath(path, sizeof(path), leafName))
        return;

    HANDLE file = CreateFileA(
        path,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;

    DWORD written = 0;
    const BOOL writeOk = WriteFile(file, bytes, byteCount, &written, nullptr);
    CloseHandle(file);
    char event[448] {};
    sprintf_s(
        event,
        "{\"event\":\"fnvxrShaderBytecodeDump\",\"stage\":\"%s\",\"fnv1a32\":\"%08x\",\"sha256\":\"%s\",\"bytes\":%u,\"written\":%lu,\"success\":%s,\"file\":\"%s\"}",
        stage,
        hash,
        shaHex,
        byteCount,
        static_cast<unsigned long>(written),
        writeOk && written == byteCount ? "true" : "false",
        leafName);
    logLine(event);
}

bool sha256Bytes(const std::uint8_t* bytes, UINT byteCount, std::uint8_t digest[32])
{
    if (!bytes || byteCount == 0 || !digest)
        return false;

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectBytes = 0;
    DWORD returned = 0;
    std::uint8_t* object = nullptr;
    bool ok = false;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0
        && BCryptGetProperty(
            algorithm,
            BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectBytes),
            sizeof(objectBytes),
            &returned,
            0) >= 0
        && objectBytes > 0)
    {
        object = new (std::nothrow) std::uint8_t[objectBytes];
        if (object
            && BCryptCreateHash(algorithm, &hash, object, objectBytes, nullptr, 0, 0) >= 0
            && BCryptHashData(hash, const_cast<PUCHAR>(bytes), byteCount, 0) >= 0
            && BCryptFinishHash(hash, digest, 32, 0) >= 0)
        {
            ok = true;
        }
    }
    if (hash)
        BCryptDestroyHash(hash);
    if (algorithm)
        BCryptCloseAlgorithmProvider(algorithm, 0);
    delete[] object;
    return ok;
}

struct ShaderFingerprint
{
    std::uint32_t fnv1a32 = 0;
    std::uint8_t sha256[32] {};
    UINT byteCount = 0;
};

struct ShaderFingerprintCacheEntry
{
    IUnknown* shader = nullptr;
    bool vertex = false;
    ShaderFingerprint fingerprint {};
};

constexpr UINT MaxShaderFingerprintCacheEntries = 1024;
ShaderFingerprintCacheEntry gShaderFingerprintCache[MaxShaderFingerprintCacheEntries] {};
UINT gShaderFingerprintCacheCount = 0;
SRWLOCK gShaderFingerprintCacheLock = SRWLOCK_INIT;

template <typename ShaderT>
ShaderFingerprint fingerprintD3DShaderBytecode(ShaderT* shader, const char* stage, bool vertex)
{
    ShaderFingerprint empty {};
    if (!shader)
        return empty;

    AcquireSRWLockShared(&gShaderFingerprintCacheLock);
    for (UINT index = 0; index < gShaderFingerprintCacheCount; ++index)
    {
        const auto& entry = gShaderFingerprintCache[index];
        if (entry.shader == shader && entry.vertex == vertex)
        {
            const ShaderFingerprint cached = entry.fingerprint;
            ReleaseSRWLockShared(&gShaderFingerprintCacheLock);
            return cached;
        }
    }
    ReleaseSRWLockShared(&gShaderFingerprintCacheLock);

    UINT byteCount = 0;
    if (FAILED(shader->GetFunction(nullptr, &byteCount)) || byteCount == 0 || byteCount > 1024 * 1024)
        return empty;

    auto* bytes = new (std::nothrow) std::uint8_t[byteCount];
    if (!bytes)
        return empty;

    const HRESULT result = shader->GetFunction(bytes, &byteCount);
    ShaderFingerprint fingerprint {};
    if (SUCCEEDED(result))
    {
        fingerprint.fnv1a32 = fnv1aHashBytes(bytes, byteCount);
        fingerprint.byteCount = byteCount;
        if (!sha256Bytes(bytes, byteCount, fingerprint.sha256))
            fingerprint = {};
    }
    if (fingerprint.fnv1a32 != 0)
        dumpD3DShaderBytecode(stage, fingerprint.fnv1a32, fingerprint.sha256, bytes, byteCount);
    delete[] bytes;

    if (fingerprint.fnv1a32 != 0)
    {
        AcquireSRWLockExclusive(&gShaderFingerprintCacheLock);
        if (gShaderFingerprintCacheCount < MaxShaderFingerprintCacheEntries)
        {
            shader->AddRef();
            gShaderFingerprintCache[gShaderFingerprintCacheCount++] = {
                shader,
                vertex,
                fingerprint
            };
        }
        ReleaseSRWLockExclusive(&gShaderFingerprintCacheLock);
    }
    return fingerprint;
}

bool shaderPairMatches(const StereoShaderPairHash& pair, std::uint32_t vertexHash, std::uint32_t pixelHash)
{
    return pair.vertex == vertexHash && pair.pixel == pixelHash;
}

bool shaderPairListContains(
    const StereoShaderPairHash* pairs,
    LONG count,
    std::uint32_t vertexHash,
    std::uint32_t pixelHash)
{
    if (!pairs || (vertexHash == 0 && pixelHash == 0))
        return false;

    for (LONG i = 0; i < count && i < static_cast<LONG>(MaxStereoCollapseSkipShaderPairs); ++i)
    {
        if (shaderPairMatches(pairs[i], vertexHash, pixelHash))
            return true;
    }
    return false;
}

void skipShaderPairListDelimiters(char*& cursor)
{
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n' || *cursor == ';' || *cursor == ',')
        ++cursor;
}

void loadConfiguredStereoSkipShaderPairs()
{
    if (InterlockedCompareExchange(&gConfiguredStereoSkipShaderPairsLoaded, 1, 0) != 0)
        return;

    char value[1024] {};
    if (GetEnvironmentVariableA(
            "FNVXR_D3D9_STEREO_SKIP_SHADER_HASH_PAIRS",
            value,
            static_cast<DWORD>(sizeof(value))) == 0)
        return;

    char* cursor = value;
    LONG count = 0;
    while (*cursor && count < static_cast<LONG>(MaxStereoCollapseSkipShaderPairs))
    {
        skipShaderPairListDelimiters(cursor);
        if (!*cursor)
            break;

        char* end = cursor;
        const unsigned long vertexHash = std::strtoul(cursor, &end, 16);
        if (end == cursor)
            break;
        cursor = end;
        while (*cursor == ' ' || *cursor == '\t')
            ++cursor;
        if (*cursor != '/' && *cursor != ':' && *cursor != '|')
        {
            while (*cursor && *cursor != ';' && *cursor != ',')
                ++cursor;
            continue;
        }
        ++cursor;
        while (*cursor == ' ' || *cursor == '\t')
            ++cursor;

        end = cursor;
        const unsigned long pixelHash = std::strtoul(cursor, &end, 16);
        if (end == cursor)
            break;
        cursor = end;

        const StereoShaderPairHash pair {
            static_cast<std::uint32_t>(vertexHash),
            static_cast<std::uint32_t>(pixelHash)
        };
        if (!shaderPairListContains(gConfiguredStereoSkipShaderPairs, count, pair.vertex, pair.pixel))
            gConfiguredStereoSkipShaderPairs[count++] = pair;

        while (*cursor && *cursor != ';' && *cursor != ',')
            ++cursor;
    }

    gConfiguredStereoSkipShaderPairCount = count;
    if (count > 0)
    {
        char message[128] {};
        sprintf_s(message, "stereo configured shader-pair skip count=%ld", count);
        logLine(message);
    }
}

bool currentShaderPairIsConfiguredStereoSkip()
{
    loadConfiguredStereoSkipShaderPairs();
    return shaderPairListContains(
        gConfiguredStereoSkipShaderPairs,
        gConfiguredStereoSkipShaderPairCount,
        gActiveVertexShaderHash,
        gActivePixelShaderHash);
}

void loadShaderWvpContracts()
{
    if (InterlockedCompareExchange(&gShaderWvpContractsLoaded, 1, 0) != 0)
        return;

    char value[4096] {};
    if (GetEnvironmentVariableA(
            "FNVXR_D3D9_SHADER_WVP_CONTRACTS",
            value,
            static_cast<DWORD>(sizeof(value))) == 0)
    {
        logLine("shader WVP contract count=0; programmable world draws fail closed");
        return;
    }

    auto hexNibble = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return 10 + ch - 'a';
        if (ch >= 'A' && ch <= 'F') return 10 + ch - 'A';
        return -1;
    };

    char* cursor = value;
    LONG count = 0;
    bool malformed = false;
    while (*cursor && count < static_cast<LONG>(MaxShaderWvpContracts))
    {
        skipShaderPairListDelimiters(cursor);
        if (!*cursor)
            break;

        char* entryEnd = cursor;
        while (*entryEnd && *entryEnd != ';' && *entryEnd != ',')
            ++entryEnd;
        while (entryEnd > cursor && (entryEnd[-1] == ' ' || entryEnd[-1] == '\t'))
            --entryEnd;

        char* part = cursor;
        std::uint32_t fnvHash = 0;
        for (int index = 0; index < 8; ++index)
        {
            const int nibble = part + index < entryEnd ? hexNibble(part[index]) : -1;
            if (nibble < 0)
            {
                malformed = true;
                break;
            }
            fnvHash = (fnvHash << 4) | static_cast<std::uint32_t>(nibble);
        }
        if (malformed || part + 8 >= entryEnd || part[8] != '/')
        {
            malformed = true;
            break;
        }
        part += 9;

        std::uint8_t sha256[32] {};
        for (int index = 0; index < 32; ++index)
        {
            const int high = part + index * 2 < entryEnd ? hexNibble(part[index * 2]) : -1;
            const int low = part + index * 2 + 1 < entryEnd ? hexNibble(part[index * 2 + 1]) : -1;
            if (high < 0 || low < 0)
            {
                malformed = true;
                break;
            }
            sha256[index] = static_cast<std::uint8_t>((high << 4) | low);
        }
        if (malformed || part + 64 >= entryEnd || part[64] != '/')
        {
            malformed = true;
            break;
        }
        part += 65;

        char* end = part;
        const unsigned long byteCount = std::strtoul(part, &end, 10);
        if (end == part || end >= entryEnd || *end != '@'
            || byteCount == 0 || byteCount > 1024 * 1024)
        {
            malformed = true;
            break;
        }
        part = end + 1;
        const unsigned long start = std::strtoul(part, &end, 10);
        if (end == part || end >= entryEnd || *end != '@'
            || start > MaxTrackedVsConstants - 4)
        {
            malformed = true;
            break;
        }
        part = end + 1;
        const size_t orderLength = static_cast<size_t>(entryEnd - part);
        const bool columnVector = orderLength == 6 && _strnicmp(part, "column", 6) == 0;
        const bool rowVector = orderLength == 3 && _strnicmp(part, "row", 3) == 0;
        if (!columnVector && !rowVector)
        {
            malformed = true;
            break;
        }

        bool duplicate = false;
        for (LONG index = 0; index < count; ++index)
        {
            if (gShaderWvpContracts[index].vertexHash == fnvHash
                || std::memcmp(gShaderWvpContracts[index].vertexSha256, sha256, sizeof(sha256)) == 0)
            {
                duplicate = true;
                break;
            }
        }
        if (duplicate)
        {
            malformed = true;
            break;
        }

        ShaderWvpContract& contract = gShaderWvpContracts[count++];
        contract.vertexHash = fnvHash;
        std::memcpy(contract.vertexSha256, sha256, sizeof(sha256));
        contract.byteCount = static_cast<UINT>(byteCount);
        contract.startRegister = static_cast<UINT>(start);
        contract.columnVector = columnVector;
        cursor = *entryEnd ? entryEnd + 1 : entryEnd;
    }

    if (malformed || (*cursor && count >= static_cast<LONG>(MaxShaderWvpContracts)))
    {
        std::memset(gShaderWvpContracts, 0, sizeof(gShaderWvpContracts));
        count = 0;
        logLine("shader WVP contract list malformed/ambiguous; all programmable world draws fail closed");
    }
    gShaderWvpContractCount = count;
    char message[192] {};
    sprintf_s(message, "shader WVP verified strong-fingerprint contract count=%ld", count);
    logLine(message);
}

const ShaderWvpContract* currentShaderWvpContract()
{
    loadShaderWvpContracts();
    for (LONG index = 0; index < gShaderWvpContractCount; ++index)
    {
        if (gShaderWvpContracts[index].vertexHash == gActiveVertexShaderHash
            && gShaderWvpContracts[index].byteCount == gActiveVertexShaderByteCount
            && std::memcmp(
                gShaderWvpContracts[index].vertexSha256,
                gActiveVertexShaderSha256,
                sizeof(gActiveVertexShaderSha256)) == 0)
            return &gShaderWvpContracts[index];
    }
    return nullptr;
}

void loadShaderStereoAllowVertexHashes()
{
    if (InterlockedCompareExchange(&gShaderStereoAllowVertexHashesLoaded, 1, 0) != 0)
        return;

    char value[1024] {};
    if (GetEnvironmentVariableA(
            "FNVXR_D3D9_SHADER_STEREO_ALLOW_VERTEX_HASHES",
            value,
            static_cast<DWORD>(sizeof(value))) == 0)
        return;

    char* cursor = value;
    LONG count = 0;
    while (*cursor && count < static_cast<LONG>(MaxShaderStereoAllowVertexHashes))
    {
        skipShaderPairListDelimiters(cursor);
        if (!*cursor)
            break;

        char* end = cursor;
        const unsigned long vertexHash = std::strtoul(cursor, &end, 16);
        if (end == cursor)
            break;

        const auto hash = static_cast<std::uint32_t>(vertexHash);
        bool duplicate = false;
        for (LONG i = 0; i < count; ++i)
        {
            if (gShaderStereoAllowVertexHashes[i] == hash)
            {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            gShaderStereoAllowVertexHashes[count++] = hash;

        cursor = end;
    }

    gShaderStereoAllowVertexHashCount = count;
    if (count > 0)
    {
        char message[128] {};
        sprintf_s(message, "shader stereo vertex allowlist count=%ld", count);
        logLine(message);
    }
}

bool currentVertexShaderAllowedForStereoPatch()
{
    if (currentShaderWvpContract())
        return true;

    loadShaderStereoAllowVertexHashes();
    const LONG count = gShaderStereoAllowVertexHashCount;
    if (count <= 0)
    {
        // c0-c3 is not a universal WVP contract in Fallout's D3D9 pipeline.
        // Treat an empty allowlist as "patch nothing" unless an explicit
        // diagnostic launch opts back into broad probing.  The old allow-all
        // behavior rewrote screen/post-process constants and could turn one
        // draw into a giant near-camera occluder.
        if (!readEnvBool("FNVXR_D3D9_SHADER_ALLOW_UNVERIFIED_PATCHES", false))
        {
            const LONG logCount = InterlockedIncrement(&gLoggedShaderStereoRejectedByHash);
            if (logCount <= 16 || logCount % 5000 == 0)
            {
                char message[224] {};
                sprintf_s(
                    message,
                    "shader stereo skipped count=%ld reason=no_verified_wvp_contract vsHash=0x%08x psHash=0x%08x",
                    logCount,
                    gActiveVertexShaderHash,
                    gActivePixelShaderHash);
                logLine(message);
            }
            return false;
        }
        return true;
    }

    for (LONG i = 0; i < count && i < static_cast<LONG>(MaxShaderStereoAllowVertexHashes); ++i)
    {
        if (gShaderStereoAllowVertexHashes[i] == gActiveVertexShaderHash)
            return true;
    }

    const LONG logCount = InterlockedIncrement(&gLoggedShaderStereoRejectedByHash);
    if (logCount <= 16 || logCount % 5000 == 0)
    {
        char message[192] {};
        sprintf_s(
            message,
            "shader stereo skipped count=%ld reason=vertex_hash_not_allowed vsHash=0x%08x psHash=0x%08x",
            logCount,
            gActiveVertexShaderHash,
            gActivePixelShaderHash);
        logLine(message);
    }
    return false;
}

bool currentShaderPairIsLearnedStereoCollapse()
{
    if (!readEnvBool("FNVXR_D3D9_STEREO_AUTO_SKIP_COLLAPSE_SHADER_PAIRS", false))
        return false;
    return shaderPairListContains(
        gStereoCollapseSkipShaderPairs,
        gStereoCollapseSkipShaderPairCount,
        gActiveVertexShaderHash,
        gActivePixelShaderHash);
}

void learnStereoCollapseShaderPair(LONG drawOrdinal)
{
    if (!readEnvBool("FNVXR_D3D9_STEREO_AUTO_SKIP_COLLAPSE_SHADER_PAIRS", false))
        return;
    if (gActiveVertexShaderHash == 0 && gActivePixelShaderHash == 0)
        return;

    const StereoShaderPairHash pair { gActiveVertexShaderHash, gActivePixelShaderHash };
    LONG count = gStereoCollapseSkipShaderPairCount;
    for (LONG i = 0; i < count && i < static_cast<LONG>(MaxStereoCollapseSkipShaderPairs); ++i)
    {
        if (shaderPairMatches(gStereoCollapseSkipShaderPairs[i], pair.vertex, pair.pixel))
            return;
    }
    if (count >= static_cast<LONG>(MaxStereoCollapseSkipShaderPairs))
    {
        if (InterlockedIncrement(&gLoggedStereoCollapseSkipLearned) <= 4)
            logLine("stereo collapse shader-pair skip table full");
        return;
    }

    gStereoCollapseSkipShaderPairs[count] = pair;
    MemoryBarrier();
    InterlockedExchange(&gStereoCollapseSkipShaderPairCount, count + 1);

    const LONG logCount = InterlockedIncrement(&gLoggedStereoCollapseSkipLearned);
    if (logCount <= 16)
    {
        char message[320] {};
        sprintf_s(
            message,
            "stereo collapse learned shader-pair skip count=%ld frame=%ld draw=%ld vs=%p ps=%p vsHash=0x%08x psHash=0x%08x",
            count + 1,
            static_cast<LONG>(gPresentFrames),
            drawOrdinal,
            reinterpret_cast<void*>(gActiveVertexShader),
            reinterpret_cast<void*>(gActivePixelShader),
            pair.vertex,
            pair.pixel);
        logLine(message);
    }
}

void normalizeQuat(float q[4])
{
    const float lengthSquared = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
    if (lengthSquared <= 0.000001f)
    {
        q[0] = 0.0f;
        q[1] = 0.0f;
        q[2] = 0.0f;
        q[3] = 1.0f;
        return;
    }

    const float invLength = 1.0f / std::sqrt(lengthSquared);
    q[0] *= invLength;
    q[1] *= invLength;
    q[2] *= invLength;
    q[3] *= invLength;
}

void copyNormalizedQuat(float out[4], const float in[4])
{
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
    out[3] = in[3];
    normalizeQuat(out);
}

void conjugateQuat(float out[4], const float in[4])
{
    out[0] = -in[0];
    out[1] = -in[1];
    out[2] = -in[2];
    out[3] = in[3];
}

void multiplyQuat(float out[4], const float a[4], const float b[4])
{
    const float ax = a[0];
    const float ay = a[1];
    const float az = a[2];
    const float aw = a[3];
    const float bx = b[0];
    const float by = b[1];
    const float bz = b[2];
    const float bw = b[3];

    out[0] = aw * bx + ax * bw + ay * bz - az * by;
    out[1] = aw * by - ax * bz + ay * bw + az * bx;
    out[2] = aw * bz + ax * by - ay * bx + az * bw;
    out[3] = aw * bw - ax * bx - ay * by - az * bz;
    normalizeQuat(out);
}

fnvxr::stereo::Matrix4 identityMatrix()
{
    fnvxr::stereo::Matrix4 matrix {};
    matrix.m[0][0] = 1.0f;
    matrix.m[1][1] = 1.0f;
    matrix.m[2][2] = 1.0f;
    matrix.m[3][3] = 1.0f;
    return matrix;
}

fnvxr::stereo::Matrix4 rotationMatrixFromQuat(const float qInput[4])
{
    float q[4] {};
    copyNormalizedQuat(q, qInput);

    const float x = q[0];
    const float y = q[1];
    const float z = q[2];
    const float w = q[3];
    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;
    const float xy = x * y;
    const float xz = x * z;
    const float yz = y * z;
    const float wx = w * x;
    const float wy = w * y;
    const float wz = w * z;

    fnvxr::stereo::Matrix4 matrix = identityMatrix();
    matrix.m[0][0] = 1.0f - 2.0f * (yy + zz);
    matrix.m[0][1] = 2.0f * (xy + wz);
    matrix.m[0][2] = 2.0f * (xz - wy);
    matrix.m[1][0] = 2.0f * (xy - wz);
    matrix.m[1][1] = 1.0f - 2.0f * (xx + zz);
    matrix.m[1][2] = 2.0f * (yz + wx);
    matrix.m[2][0] = 2.0f * (xz + wy);
    matrix.m[2][1] = 2.0f * (yz - wx);
    matrix.m[2][2] = 1.0f - 2.0f * (xx + yy);
    return matrix;
}

fnvxr::stereo::Matrix4 multiplyMatrix(const fnvxr::stereo::Matrix4& a, const fnvxr::stereo::Matrix4& b)
{
    fnvxr::stereo::Matrix4 result {};
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            for (int inner = 0; inner < 4; ++inner)
                result.m[row][column] += a.m[row][inner] * b.m[inner][column];
        }
    }
    return result;
}

void rotateByVrWorldYaw(const float in[3], float out[3])
{
    const float x = in[0];
    const float z = in[2];
    out[0] = gVrWorldYawCos * x - gVrWorldYawSin * z;
    out[1] = in[1];
    out[2] = gVrWorldYawSin * x + gVrWorldYawCos * z;
}

void correctedVrWorldQuat(float out[4], const float in[4])
{
    const fnvxr::stereo::Quaternion origin {
        gVrPoseOriginRot[0],
        gVrPoseOriginRot[1],
        gVrPoseOriginRot[2],
        gVrPoseOriginRot[3]
    };
    const fnvxr::stereo::Quaternion current { in[0], in[1], in[2], in[3] };
    const fnvxr::stereo::Quaternion relative = fnvxr::stereo::relativeOrientation(origin, current);
    out[0] = relative.x;
    out[1] = relative.y;
    out[2] = relative.z;
    out[3] = relative.w;
}

void readPoseAxisMode(char* buffer, size_t bufferSize)
{
    if (!buffer || bufferSize == 0)
        return;

    buffer[0] = '\0';
    if (GetEnvironmentVariableA("FNVXR_D3D9_POSE_AXIS_MODE", buffer, static_cast<DWORD>(bufferSize)) == 0)
        strcpy_s(buffer, bufferSize, "gamebryo");
}

bool poseAxisModeIsLegacySigns()
{
    char mode[32] {};
    readPoseAxisMode(mode, sizeof(mode));
    return _stricmp(mode, "legacy-signs") == 0 || _stricmp(mode, "legacy") == 0;
}

fnvxr::stereo::Matrix4 gamebryoRotationFromXrRotation(const fnvxr::stereo::Matrix4& xr)
{
    fnvxr::stereo::Matrix4 result = identityMatrix();
    // OpenXR: +X right, +Y up, -Z forward. Gamebryo/FNV: +X right, +Y forward, +Z up.
    const int xrAxisForGameAxis[3] { 0, 2, 1 };
    const float signForGameAxis[3] { 1.0f, -1.0f, 1.0f };
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

void xrMetersToGamebryoMeters(const float xrMeters[3], float gameMeters[3])
{
    if (poseAxisModeIsLegacySigns())
    {
        gameMeters[0] = xrMeters[0] * readEnvFloat("FNVXR_D3D9_POSE_X_SIGN", -1.0f);
        gameMeters[1] = xrMeters[1] * readEnvFloat("FNVXR_D3D9_POSE_Y_SIGN", -1.0f);
        gameMeters[2] = xrMeters[2] * readEnvFloat("FNVXR_D3D9_POSE_Z_SIGN", -1.0f);
        return;
    }

    gameMeters[0] = xrMeters[0] * readEnvFloat("FNVXR_D3D9_POSE_X_SIGN", 1.0f);
    gameMeters[1] = -xrMeters[2] * readEnvFloat("FNVXR_D3D9_POSE_Y_SIGN", 1.0f);
    gameMeters[2] = xrMeters[1] * readEnvFloat("FNVXR_D3D9_POSE_Z_SIGN", 1.0f);
}

void composeLocalXrMeters(const float eyeLocalMeters[3], float localMeters[3])
{
    const float headScale = readEnvFloat("FNVXR_D3D9_HEAD_POSITION_SCALE", 1.0f);
    const float stereoScale = readEnvFloat("FNVXR_D3D9_STEREO_SCALE", 1.0f);
    localMeters[0] = gLatestHeadLocalMeters[0] * headScale + eyeLocalMeters[0] * stereoScale;
    localMeters[1] = gLatestHeadLocalMeters[1] * headScale + eyeLocalMeters[1] * stereoScale;
    localMeters[2] = gLatestHeadLocalMeters[2] * headScale + eyeLocalMeters[2] * stereoScale;
}

void rotateVectorByQuat(const float qInput[4], const float in[3], float out[3])
{
    float q[4] {};
    copyNormalizedQuat(q, qInput);
    const float u[3] { q[0], q[1], q[2] };
    const float s = q[3];
    const float dotUv = u[0] * in[0] + u[1] * in[1] + u[2] * in[2];
    const float dotUu = u[0] * u[0] + u[1] * u[1] + u[2] * u[2];
    const float cross[3] {
        u[1] * in[2] - u[2] * in[1],
        u[2] * in[0] - u[0] * in[2],
        u[0] * in[1] - u[1] * in[0]
    };
    out[0] = 2.0f * dotUv * u[0] + (s * s - dotUu) * in[0] + 2.0f * s * cross[0];
    out[1] = 2.0f * dotUv * u[1] + (s * s - dotUu) * in[1] + 2.0f * s * cross[1];
    out[2] = 2.0f * dotUv * u[2] + (s * s - dotUu) * in[2] + 2.0f * s * cross[2];
}

void captureVrWorldYawCorrection(const float hmdRot[4])
{
    const float forwardLocal[3] { 0.0f, 0.0f, -1.0f };
    float forward[3] {};
    rotateVectorByQuat(hmdRot, forwardLocal, forward);
    const float capturedYaw = atan2f(-forward[0], -forward[2]);
    const float correctionYaw = -capturedYaw;
    gVrWorldYawCos = cosf(correctionYaw);
    gVrWorldYawSin = sinf(correctionYaw);
    gVrWorldYawHalfCos = cosf(correctionYaw * 0.5f);
    gVrWorldYawHalfSin = sinf(correctionYaw * 0.5f);
}

bool invertMatrix(const fnvxr::stereo::Matrix4& input, fnvxr::stereo::Matrix4& inverse)
{
    fnvxr::stereo::Matrix4 identity {};
    for (int axis = 0; axis < 4; ++axis)
        identity.m[axis][axis] = 1.0f;
    const fnvxr::stereo::ViewProjectionDelta certifiedInverse =
        fnvxr::stereo::makeViewProjectionDelta(input, identity, true);
    if (!certifiedInverse.valid)
        return false;
    inverse = certifiedInverse.matrix;
    return fnvxr::stereo::isFinite(inverse);
}

fnvxr::stereo::Matrix4 hmdViewRotationMatrix()
{
    float inverseDelta[4] {};
    conjugateQuat(inverseDelta, gLatestHmdRotDelta);
    const fnvxr::stereo::Matrix4 xrRotation = rotationMatrixFromQuat(inverseDelta);
    return poseAxisModeIsLegacySigns() ? xrRotation : gamebryoRotationFromXrRotation(xrRotation);
}

fnvxr::stereo::Matrix4 multiplyViewRotationOnly(
    const fnvxr::stereo::Matrix4& baseView,
    const fnvxr::stereo::Matrix4& localViewRotation)
{
    fnvxr::stereo::Matrix4 result = baseView;
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 3; ++column)
        {
            result.m[row][column] =
                baseView.m[row][0] * localViewRotation.m[0][column]
                + baseView.m[row][1] * localViewRotation.m[1][column]
                + baseView.m[row][2] * localViewRotation.m[2][column];
        }
    }
    return result;
}

fnvxr::stereo::Matrix4 makeLocalVrEyeView(
    const fnvxr::stereo::Matrix4& baseView,
    const float eyeLocalMeters[3],
    bool applyRotation)
{
    fnvxr::stereo::Matrix4 result = applyRotation
        ? multiplyViewRotationOnly(baseView, hmdViewRotationMatrix())
        : baseView;

    float localXrMeters[3] {};
    float localGameMeters[3] {};
    composeLocalXrMeters(eyeLocalMeters, localXrMeters);
    xrMetersToGamebryoMeters(localXrMeters, localGameMeters);
    result.m[3][0] -= localGameMeters[0] * gGameUnitsPerMeter;
    result.m[3][1] -= localGameMeters[1] * gGameUnitsPerMeter;
    result.m[3][2] -= localGameMeters[2] * gGameUnitsPerMeter;
    return result;
}

float viewTranslationDeltaMagnitude(
    const fnvxr::stereo::Matrix4& view,
    const fnvxr::stereo::Matrix4& baseView)
{
    return absFloat(view.m[3][0] - baseView.m[3][0])
        + absFloat(view.m[3][1] - baseView.m[3][1])
        + absFloat(view.m[3][2] - baseView.m[3][2]);
}

bool fnvMenuMode()
{
    __try
    {
        using IsMenuModeFn = bool (__cdecl*)();
        return reinterpret_cast<IsMenuModeFn>(IsMenuModeAddress)();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return true;
    }
}

bool fnvMenuVisible(UINT menuType)
{
    __try
    {
        auto* visibility = reinterpret_cast<std::uint8_t*>(MenuVisibilityArrayAddress);
        return visibility[menuType] != 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool fnvUiModeActive()
{
    {
        // Prefer the NVSE main-loop runtime state when it is available. The legacy menu probes are kept only as
        // startup/failure fallback because they are address-specific and can over-report UI during the handoff.
        __try
        {
            if (!gSharedRuntime)
            {
                gSharedRuntimeMapping = OpenFileMappingA(FILE_MAP_READ, FALSE, "Local\\FNVXR_Runtime_State");
                if (!gSharedRuntimeMapping && InterlockedIncrement(&gLoggedSharedRuntimeMissing) <= 4)
                    logLine("runtime state mapping not available yet");
                if (gSharedRuntimeMapping)
                {
                    gSharedRuntime = static_cast<SharedRuntimeState*>(
                        MapViewOfFile(gSharedRuntimeMapping, FILE_MAP_READ, 0, 0, sizeof(SharedRuntimeState)));
                    if (!gSharedRuntime)
                    {
                        CloseHandle(gSharedRuntimeMapping);
                        gSharedRuntimeMapping = nullptr;
                    }
                }
            }
            if (gSharedRuntime
                && (gSharedRuntime->magic != SharedRuntimeMagic
                    || gSharedRuntime->version != SharedRuntimeVersion))
            {
                if (InterlockedIncrement(&gLoggedSharedRuntimeInvalid) <= 4)
                {
                    char message[128] {};
                    sprintf_s(
                        message,
                        "runtime state invalid magic=0x%08lx version=%lu",
                        static_cast<unsigned long>(gSharedRuntime->magic),
                        static_cast<unsigned long>(gSharedRuntime->version));
                    logLine(message);
                }
            }
            if (gSharedRuntime
                && gSharedRuntime->magic == SharedRuntimeMagic
                && gSharedRuntime->version == SharedRuntimeVersion)
            {
                const LONG sequenceBefore = gSharedRuntime->sequence;
                const DWORD phase = gSharedRuntime->phase;
                const DWORD menuBits = gSharedRuntime->menuBits;
                const DWORD showroomActive = gSharedRuntime->showroomActive;
                const LONG sequenceAfter = gSharedRuntime->sequence;
                if ((sequenceBefore & 1) == 0 && sequenceBefore == sequenceAfter)
                {
                    if (sequenceBefore != gLastRuntimeSequence)
                    {
                        gLastRuntimeSequence = sequenceBefore;
                        const LONG logCount = InterlockedIncrement(&gLoggedSharedRuntime);
                        if (logCount <= 12 || logCount % 300 == 0)
                        {
                            char message[192] {};
                            sprintf_s(
                                message,
                                "runtime state observed seq=%ld phase=%lu menuBits=0x%lx camera=%lu showroom=%lu",
                                sequenceBefore,
                                static_cast<unsigned long>(phase),
                                static_cast<unsigned long>(menuBits),
                                static_cast<unsigned long>(gSharedRuntime->cameraActive),
                                static_cast<unsigned long>(showroomActive));
                            logLine(message);
                        }
                    }
                    return fnvxr::shared::runtimeUiActive(phase, menuBits, showroomActive);
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            if (gSharedRuntime)
            {
                UnmapViewOfFile(gSharedRuntime);
                gSharedRuntime = nullptr;
            }
            if (gSharedRuntimeMapping)
            {
                CloseHandle(gSharedRuntimeMapping);
                gSharedRuntimeMapping = nullptr;
            }
        }
    }

    return fnvMenuMode()
        || fnvMenuVisible(kMenuTypeStart)
        || fnvMenuVisible(kMenuTypeRaceSex)
        || fnvMenuVisible(kMenuTypeLoading)
        || fnvMenuVisible(kMenuTypeDialog)
        || fnvMenuVisible(kMenuTypeVats)
        || fnvMenuVisible(kMenuTypeInventory)
        || fnvMenuVisible(kMenuTypeStats)
        || fnvMenuVisible(kMenuTypeMap);
}

bool suppressStereoForUiMode()
{
    return fnvUiModeActive() && !readEnvBool("FNVXR_D3D9_CAPTURE_WORLD_DURING_UI", false);
}

bool suppressGameplayHudDraw()
{
    if (!readEnvBool("FNVXR_D3D9_HIDE_GAMEPLAY_HUD", false))
        return false;
    if (fnvUiModeActive())
        return false;
    if (readEnvBool("FNVXR_D3D9_HIDE_GAMEPLAY_HUD_REQUIRE_HUD_MENU", true)
        && !fnvMenuVisible(kMenuTypeHUDMain))
    {
        return false;
    }
    if (!currentProjectionLooksScreenSpace())
        return false;
    if (readEnvBool("FNVXR_D3D9_HIDE_GAMEPLAY_HUD_REQUIRE_SCREEN_VIEW", true)
        && !currentViewLooksScreenSpace())
    {
        return false;
    }

    const LONG count = InterlockedIncrement(&gLoggedGameplayHudSuppressed);
    if (count <= 12 || count % 1000 == 0)
    {
        char message[256] {};
        sprintf_s(
            message,
            "gameplay HUD draw suppressed count=%ld frame=%ld viewScreen=%d hudMenu=%d",
            count,
            static_cast<LONG>(gPresentFrames),
            currentViewLooksScreenSpace() ? 1 : 0,
            fnvMenuVisible(kMenuTypeHUDMain) ? 1 : 0);
        logLine(message);
    }
    return true;
}

SharedVrPoseState* sharedVrPoseState()
{
    if (gSharedVrPose)
        return gSharedVrPose;

    gSharedVrPoseMapping = OpenFileMappingA(
        FILE_MAP_READ,
        FALSE,
        fnvxr::shared::VrPoseSharedMappingName);
    if (!gSharedVrPoseMapping)
        return nullptr;

    gSharedVrPose = static_cast<SharedVrPoseState*>(
        MapViewOfFile(gSharedVrPoseMapping, FILE_MAP_READ, 0, 0, sizeof(SharedVrPoseState)));
    if (!gSharedVrPose)
    {
        CloseHandle(gSharedVrPoseMapping);
        gSharedVrPoseMapping = nullptr;
        return nullptr;
    }

    logLine("shared VR pose mapped");
    return gSharedVrPose;
}

bool namedProducerLeaseHeldByCurrentThread(const NamedProducerLease& lease)
{
    if (!lease.mutex
        || !lease.ownerThread
        || !lease.owned
        || lease.ownerThreadId != GetCurrentThreadId())
    {
        return false;
    }
    // A thread id may be reused after its original thread exits.  The retained
    // SYNCHRONIZE handle identifies the actual acquisition thread and must
    // still be unsignaled, not merely have a matching numeric id.
    return WaitForSingleObject(lease.ownerThread, 0) == WAIT_TIMEOUT;
}

bool acquireNamedProducerMutex(const char* name, NamedProducerLease& lease)
{
    if (lease.mutex)
    {
        if (namedProducerLeaseHeldByCurrentThread(lease))
            return true;
        if (!lease.ownerThread
            || WaitForSingleObject(lease.ownerThread, 0) != WAIT_OBJECT_0)
        {
            return false;
        }

        // The original acquisition thread exited. Recover the abandoned
        // mutex on this thread before allowing the retained mapping to be
        // touched. If another process won the lease, fail closed.
        const DWORD recovered = WaitForSingleObject(lease.mutex, 0);
        if (recovered != WAIT_OBJECT_0 && recovered != WAIT_ABANDONED)
            return false;
        const DWORD ownerThreadId = GetCurrentThreadId();
        HANDLE ownerThread = OpenThread(SYNCHRONIZE, FALSE, ownerThreadId);
        if (!ownerThread)
        {
            ReleaseMutex(lease.mutex);
            return false;
        }
        CloseHandle(lease.ownerThread);
        lease.ownerThread = ownerThread;
        lease.ownerThreadId = ownerThreadId;
        lease.owned = true;
        return true;
    }

    HANDLE mutex = CreateMutexA(nullptr, FALSE, name);
    if (!mutex)
        return false;
    const DWORD wait = WaitForSingleObject(mutex, 0);
    if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED)
    {
        CloseHandle(mutex);
        return false;
    }

    const DWORD ownerThreadId = GetCurrentThreadId();
    HANDLE ownerThread = OpenThread(SYNCHRONIZE, FALSE, ownerThreadId);
    if (!ownerThread)
    {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
        return false;
    }

    lease.mutex = mutex;
    lease.ownerThread = ownerThread;
    lease.ownerThreadId = ownerThreadId;
    lease.owned = true;
    return true;
}

void releaseNamedProducerMutex(NamedProducerLease& lease);

SharedVrOriginState* sharedVrOriginState()
{
    if (gSharedVrOrigin)
        return acquireNamedProducerMutex(
            "Local\\FNVXR_VR_Origin_Producer_v6",
            gSharedVrOriginProducerLease)
            ? gSharedVrOrigin
            : nullptr;

    if (!acquireNamedProducerMutex(
            "Local\\FNVXR_VR_Origin_Producer_v6",
            gSharedVrOriginProducerLease))
    {
        logLine("shared VR origin publisher lease unavailable; refusing second producer");
        return nullptr;
    }

    gSharedVrOriginMapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(SharedVrOriginState),
        fnvxr::shared::VrOriginSharedMappingName);
    if (!gSharedVrOriginMapping)
    {
        releaseNamedProducerMutex(gSharedVrOriginProducerLease);
        return nullptr;
    }

    gSharedVrOrigin = static_cast<SharedVrOriginState*>(MapViewOfFile(
        gSharedVrOriginMapping,
        FILE_MAP_READ | FILE_MAP_WRITE,
        0,
        0,
        sizeof(SharedVrOriginState)));
    if (!gSharedVrOrigin)
    {
        CloseHandle(gSharedVrOriginMapping);
        gSharedVrOriginMapping = nullptr;
        releaseNamedProducerMutex(gSharedVrOriginProducerLease);
        return nullptr;
    }

    // A consumer may retain the named mapping across a producer restart.
    // Recover an abandoned odd sequence under the exclusive producer lease,
    // then replace the record as one reader-visible invalid transaction while
    // preserving the live synchronization word.
    const bool recoveredAbandonedWrite =
        (fnvxr::shared::sequencedValueBits(gSharedVrOrigin->sequence) & 1u) != 0u;
    if (!recoveredAbandonedWrite
        && !fnvxr::shared::beginSequencedSharedWrite(gSharedVrOrigin->sequence))
    {
        UnmapViewOfFile(gSharedVrOrigin);
        gSharedVrOrigin = nullptr;
        CloseHandle(gSharedVrOriginMapping);
        gSharedVrOriginMapping = nullptr;
        releaseNamedProducerMutex(gSharedVrOriginProducerLease);
        return nullptr;
    }
    // If the prior owner died with an odd sequence, the exclusive producer
    // lease transfers that already-open write transaction to us. Keep it odd
    // while replacing every payload byte; never expose a temporary even view
    // of the abandoned partial record.
    MemoryBarrier();
    std::memset(gSharedVrOrigin, 0, offsetof(SharedVrOriginState, sequence));
    std::memset(
        reinterpret_cast<std::uint8_t*>(gSharedVrOrigin) + offsetof(SharedVrOriginState, active),
        0,
        sizeof(*gSharedVrOrigin) - offsetof(SharedVrOriginState, active));
    gSharedVrOrigin->magic = fnvxr::shared::VrOriginSharedMagic;
    gSharedVrOrigin->version = fnvxr::shared::VrOriginSharedVersion;
    gSharedVrOrigin->active = fnvxr::shared::VrOriginStateInvalid;
    gSharedVrOrigin->originRot[3] = 1.0f;
    gSharedVrOrigin->reserved = static_cast<std::uint32_t>(GetTickCount64());
    gSharedVrOrigin->reserved2 = GetCurrentProcessId();
    fnvxr::shared::endSequencedSharedWrite(gSharedVrOrigin->sequence);
    logLine("shared VR origin publisher mapped");
    return gSharedVrOrigin;
}

bool finiteArray(const float* values, int count);
bool readNativePlayerBodyRoot(
    std::uintptr_t& addressOut,
    float rotationOut[9],
    float positionOut[3],
    float& scaleOut);

void publishSharedVrOrigin(
    bool active,
    const SharedVrPoseState* pose = nullptr,
    LONG poseSequence = 0,
    std::uintptr_t renderCameraAddress = 0,
    const float* renderCameraWorldRot = nullptr,
    const float* renderCameraWorldPos = nullptr,
    std::uintptr_t bodyRootAddress = 0,
    const float* bodyRootWorldRot = nullptr,
    const float* bodyRootWorldPos = nullptr,
    float bodyRootWorldScale = 0.0f)
{
    SharedVrOriginState* origin = sharedVrOriginState();
    if (!origin)
        return;

    if (!fnvxr::shared::beginSequencedSharedWrite(origin->sequence))
        return;
    origin->magic = fnvxr::shared::VrOriginSharedMagic;
    origin->version = fnvxr::shared::VrOriginSharedVersion;
    origin->active = active
        ? fnvxr::shared::VrOriginStateRenderLease
        : fnvxr::shared::VrOriginStateInvalid;
    origin->generation = active ? gVrPoseOriginGeneration
                                : (pose ? pose->referenceSpaceGeneration : gLastVrReferenceSpaceGeneration);
    origin->poseSequence = active
        && fnvxr::shared::sequencedValueIsPublished(gVrPoseOriginPoseSequence)
        ? static_cast<std::uint32_t>(gVrPoseOriginPoseSequence)
        : 0u;
    origin->poseFrame = active ? gVrPoseOriginPoseFrame : 0u;
    for (int i = 0; i < 4; ++i)
        origin->originRot[i] = active ? gVrPoseOriginRot[i] : (i == 3 ? 1.0f : 0.0f);
    for (int i = 0; i < 3; ++i)
        origin->originPos[i] = active ? gVrPoseOrigin[i] : 0.0f;
    origin->producerEpoch = active ? gVrPoseOriginProducerEpoch
                                   : (pose ? pose->producerEpoch : gLastVrProducerEpoch);
    origin->renderPoseSequence = active
        && fnvxr::shared::sequencedValueIsPublished(poseSequence)
        ? static_cast<std::uint32_t>(poseSequence)
        : 0u;
    origin->reserved = 0;
    origin->renderPoseFrame = active && pose ? pose->frame : 0u;
    origin->renderedDisplayTime = active && pose ? pose->predictedDisplayTime : 0;
    const bool cameraWorldValid = active
        && renderCameraAddress != 0
        && renderCameraWorldRot
        && renderCameraWorldPos
        && finiteArray(renderCameraWorldRot, 9)
        && finiteArray(renderCameraWorldPos, 3);
    origin->renderCameraAddress = cameraWorldValid
        ? static_cast<std::uint32_t>(renderCameraAddress)
        : 0u;
    origin->renderCameraWorldValid = cameraWorldValid ? 1u : 0u;
    for (int i = 0; i < 9; ++i)
        origin->renderCameraWorldRot[i] = cameraWorldValid ? renderCameraWorldRot[i] : 0.0f;
    for (int i = 0; i < 3; ++i)
        origin->renderCameraWorldPos[i] = cameraWorldValid ? renderCameraWorldPos[i] : 0.0f;
    const bool bodyWorldValid = active
        && bodyRootAddress != 0
        && bodyRootWorldRot
        && bodyRootWorldPos
        && finiteArray(bodyRootWorldRot, 9)
        && finiteArray(bodyRootWorldPos, 3)
        && std::isfinite(bodyRootWorldScale)
        && std::fabs(bodyRootWorldScale) >= 0.0001f;
    origin->bodyRootAddress = bodyWorldValid
        ? static_cast<std::uint32_t>(bodyRootAddress)
        : 0u;
    origin->bodyRootWorldValid = bodyWorldValid ? 1u : 0u;
    for (int i = 0; i < 9; ++i)
        origin->bodyRootWorldRot[i] = bodyWorldValid ? bodyRootWorldRot[i] : 0.0f;
    for (int i = 0; i < 3; ++i)
        origin->bodyRootWorldPos[i] = bodyWorldValid ? bodyRootWorldPos[i] : 0.0f;
    origin->bodyRootWorldScale = bodyWorldValid ? bodyRootWorldScale : 0.0f;
    origin->reserved2 = active ? GetCurrentProcessId() : 0u;
    fnvxr::shared::endSequencedSharedWrite(origin->sequence);

    gPublishedVrOriginActive = active;
    gPublishedVrOriginCommitted = false;
    gPublishedVrOriginGeneration = active && pose ? pose->referenceSpaceGeneration : 0u;
}

bool commitNativeStereoOriginLease()
{
    if (!gPublishedVrOriginActive)
        return false;
    SharedVrOriginState* origin = sharedVrOriginState();
    if (!origin)
        return false;

    if (!fnvxr::shared::beginSequencedSharedWrite(origin->sequence))
        return false;
    origin->active = fnvxr::shared::VrOriginStateCommitted;
    origin->reserved = static_cast<std::uint32_t>(GetTickCount64());
    origin->reserved2 = GetCurrentProcessId();
    fnvxr::shared::endSequencedSharedWrite(origin->sequence);
    gPublishedVrOriginActive = false;
    gPublishedVrOriginCommitted = true;
    logLine("native stereo origin committed for animation handoff");
    return true;
}

void closeNativeStereoOriginLease(const char* reason)
{
    if (!gPublishedVrOriginActive && !gPublishedVrOriginCommitted)
        return;
    publishSharedVrOrigin(false);
    char message[224] {};
    sprintf_s(message, "native stereo origin lease closed reason=%s", reason ? reason : "unknown");
    logLine(message);
}

void openNativeStereoOriginLease(
    const SharedVrPoseState& pose,
    std::uintptr_t renderCameraAddress,
    const float* renderCameraWorldRot,
    const float* renderCameraWorldPos)
{
    std::uintptr_t bodyRootAddress = 0;
    float bodyRootWorldRot[9] {};
    float bodyRootWorldPos[3] {};
    float bodyRootWorldScale = 0.0f;
    const bool bodyRootValid = readNativePlayerBodyRoot(
        bodyRootAddress,
        bodyRootWorldRot,
        bodyRootWorldPos,
        bodyRootWorldScale);
    publishSharedVrOrigin(
        true,
        &pose,
        gLatestVrPoseSnapshotSequence,
        renderCameraAddress,
        renderCameraWorldRot,
        renderCameraWorldPos,
        bodyRootValid ? bodyRootAddress : 0,
        bodyRootValid ? bodyRootWorldRot : nullptr,
        bodyRootValid ? bodyRootWorldPos : nullptr,
        bodyRootValid ? bodyRootWorldScale : 0.0f);
    char message[512] {};
    sprintf_s(
        message,
        "native stereo origin lease opened epoch=%llu generation=%lu originPoseSeq=%ld originPoseFrame=%llu renderPoseSeq=%ld renderPoseFrame=%llu originPos=(%.5f %.5f %.5f) originYawQuat=(%.6f %.6f %.6f %.6f) renderCamera=0x%08lx renderCameraWorld=(%.4f %.4f %.4f) bodyRoot=0x%08lx bodyValid=%d",
        static_cast<unsigned long long>(gVrPoseOriginProducerEpoch),
        static_cast<unsigned long>(gVrPoseOriginGeneration),
        gVrPoseOriginPoseSequence,
        static_cast<unsigned long long>(gVrPoseOriginPoseFrame),
        gLatestVrPoseSnapshotSequence,
        static_cast<unsigned long long>(pose.frame),
        gVrPoseOrigin[0], gVrPoseOrigin[1], gVrPoseOrigin[2],
        gVrPoseOriginRot[0], gVrPoseOriginRot[1], gVrPoseOriginRot[2], gVrPoseOriginRot[3],
        static_cast<unsigned long>(renderCameraAddress),
        renderCameraWorldPos ? renderCameraWorldPos[0] : 0.0f,
        renderCameraWorldPos ? renderCameraWorldPos[1] : 0.0f,
        renderCameraWorldPos ? renderCameraWorldPos[2] : 0.0f,
        static_cast<unsigned long>(bodyRootAddress),
        bodyRootValid ? 1 : 0);
    logLine(message);
}

bool finiteArray(const float* values, int count)
{
    for (int i = 0; i < count; ++i)
    {
        if (!std::isfinite(values[i]))
            return false;
    }
    return true;
}

bool quatLooksUsable(const float q[4])
{
    if (!finiteArray(q, 4))
        return false;

    const float lengthSquared = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
    return lengthSquared >= 0.25f && lengthSquared <= 4.0f;
}

bool posePositionLooksUsable(const float p[3])
{
    if (!finiteArray(p, 3))
        return false;

    const float maxAbsMeters = readEnvFloat("FNVXR_D3D9_POSE_MAX_ABS_METERS", 100.0f);
    return absFloat(p[0]) <= maxAbsMeters
        && absFloat(p[1]) <= maxAbsMeters
        && absFloat(p[2]) <= maxAbsMeters;
}

bool fovLooksUsable(const float fov[4])
{
    return fnvxr::stereo::openXrFovAnglesUsable(fov);
}

bool d3d9VrPoseInputEnabled()
{
    return stereoWorldRuntimeEnabled()
        && (gNativeStereoEnabled
            || gNativeSingleTraversalReplayEnabled
            || readEnvBool("FNVXR_D3D9_APPLY_HMD_POSE", false)
            || readEnvBool("FNVXR_D3D9_STEREO_REPLAY", true)
            || readEnvBool("FNVXR_D3D9_SHARED_STEREO", true));
}

bool vrPoseSnapshotLooksUsable(const SharedVrPoseState& snapshot)
{
    return snapshot.magic == SharedVrPoseMagic
        && snapshot.version == SharedVrPoseVersion
        && snapshot.frame != 0
        && snapshot.referenceSpaceGeneration != 0
        && snapshot.producerEpoch != 0
        && (snapshot.trackingFlags & fnvxr::shared::VrPoseTrackingHmd) != 0
        && posePositionLooksUsable(snapshot.hmdPos)
        && posePositionLooksUsable(snapshot.leftPos)
        && posePositionLooksUsable(snapshot.rightPos)
        && quatLooksUsable(snapshot.hmdRot)
        && quatLooksUsable(snapshot.leftRot)
        && quatLooksUsable(snapshot.rightRot)
        && posePositionLooksUsable(snapshot.leftEyePos)
        && posePositionLooksUsable(snapshot.rightEyePos)
        && quatLooksUsable(snapshot.leftEyeRot)
        && quatLooksUsable(snapshot.rightEyeRot)
        && fovLooksUsable(snapshot.leftFov)
        && fovLooksUsable(snapshot.rightFov);
}

bool readStableSharedVrPose(SharedVrPoseState& snapshotOut, LONG& sequenceOut)
{
    SharedVrPoseState* pose = sharedVrPoseState();
    if (!pose || pose->magic != SharedVrPoseMagic || pose->version != SharedVrPoseVersion)
        return false;

    for (int attempt = 0; attempt < 50; ++attempt)
    {
        const LONG sequenceBefore = pose->sequence;
        if (sequenceBefore == 0 || (sequenceBefore & 1) != 0)
        {
            YieldProcessor();
            continue;
        }
        if (sequenceBefore == gLastVrPoseSequence)
        {
            if (!gLatestVrPoseSnapshotValid)
                return false;

            LARGE_INTEGER now {};
            QueryPerformanceCounter(&now);
            const double maxAgeSeconds = static_cast<double>(
                readEnvFloat("FNVXR_D3D9_POSE_MAX_SAME_SEQUENCE_MS", 250.0f)) / 1000.0;
            if (gLatestVrPoseAcceptedAt.QuadPart == 0
                || maxAgeSeconds <= 0.0
                || secondsBetween(gLatestVrPoseAcceptedAt, now) > maxAgeSeconds)
            {
                const LONG logCount = InterlockedIncrement(&gLoggedSharedVrPoseInvalid);
                if (logCount <= 16 || logCount % 300 == 0)
                {
                    char message[224] {};
                    sprintf_s(
                        message,
                        "shared VR pose rejected: stale sequence=%ld frame=%llu ageMs=%.1f limitMs=%.1f",
                        sequenceBefore,
                        static_cast<unsigned long long>(gLatestVrPoseSnapshot.frame),
                        gLatestVrPoseAcceptedAt.QuadPart == 0
                            ? -1.0
                            : secondsBetween(gLatestVrPoseAcceptedAt, now) * 1000.0,
                        maxAgeSeconds * 1000.0);
                    logLine(message);
                }
                return false;
            }

            snapshotOut = gLatestVrPoseSnapshot;
            sequenceOut = gLatestVrPoseSnapshotSequence;
            return true;
        }

        MemoryBarrier();
        const SharedVrPoseState snapshot = *pose;
        MemoryBarrier();
        const LONG sequenceAfter = pose->sequence;
        if (sequenceBefore != sequenceAfter || (sequenceAfter & 1) != 0)
        {
            YieldProcessor();
            continue;
        }
        if (!vrPoseSnapshotLooksUsable(snapshot))
            break;

        snapshotOut = snapshot;
        sequenceOut = sequenceAfter;
        return true;
    }

    const LONG logCount = InterlockedIncrement(&gLoggedSharedVrPoseInvalid);
    if (logCount <= 8 || logCount % 300 == 0)
        logLine("shared VR pose rejected: unstable or invalid snapshot");
    return false;
}

SharedCameraState* sharedCameraState()
{
    if (gSharedCamera)
        return gSharedCamera;

    gSharedCameraMapping = OpenFileMappingA(FILE_MAP_READ, FALSE, "Local\\FNVXR_Camera_State");
    if (!gSharedCameraMapping)
        return nullptr;

    gSharedCamera = static_cast<SharedCameraState*>(
        MapViewOfFile(gSharedCameraMapping, FILE_MAP_READ, 0, 0, sizeof(SharedCameraState)));
    if (!gSharedCamera)
    {
        CloseHandle(gSharedCameraMapping);
        gSharedCameraMapping = nullptr;
        return nullptr;
    }

    logLine("shared camera mapped");
    return gSharedCamera;
}

SharedPlayerState* sharedPlayerState()
{
    if (gSharedPlayer)
        return gSharedPlayer;

    gSharedPlayerMapping = OpenFileMappingA(FILE_MAP_READ, FALSE, "Local\\FNVXR_Player_State");
    if (!gSharedPlayerMapping)
        return nullptr;

    gSharedPlayer = static_cast<SharedPlayerState*>(
        MapViewOfFile(gSharedPlayerMapping, FILE_MAP_READ, 0, 0, sizeof(SharedPlayerState)));
    if (!gSharedPlayer)
    {
        CloseHandle(gSharedPlayerMapping);
        gSharedPlayerMapping = nullptr;
        return nullptr;
    }

    logLine("shared player state mapped for native stereo transition guard");
    return gSharedPlayer;
}

bool readStableSharedPlayerState(SharedPlayerState& snapshotOut, LONG& sequenceOut)
{
    SharedPlayerState* player = sharedPlayerState();
    if (!player)
        return false;

    for (int attempt = 0; attempt < 4; ++attempt)
    {
        const LONG sequenceBefore = player->sequence;
        if ((sequenceBefore & 1) != 0)
        {
            YieldProcessor();
            continue;
        }

        MemoryBarrier();
        const SharedPlayerState snapshot = *player;
        MemoryBarrier();
        const LONG sequenceAfter = player->sequence;
        if (sequenceBefore != sequenceAfter || (sequenceAfter & 1) != 0)
        {
            YieldProcessor();
            continue;
        }
        if (snapshot.magic != SharedPlayerMagic || snapshot.version != SharedPlayerVersion)
            return false;

        snapshotOut = snapshot;
        sequenceOut = sequenceAfter;
        return true;
    }
    return false;
}

bool nativeStereoCellTransitionReady()
{
    if (!readEnvBool("FNVXR_D3D9_NATIVE_REQUIRE_STABLE_CELL", true))
        return true;

    SharedPlayerState snapshot {};
    LONG sequence = 0;
    const bool snapshotReady = readStableSharedPlayerState(snapshot, sequence);
    if (!snapshotReady)
    {
        const LONG readMisses = InterlockedIncrement(&gNativeSharedPlayerReadMisses);
        const LONG graceHooks = static_cast<LONG>((std::max)(
            0.0f,
            readEnvFloat("FNVXR_D3D9_NATIVE_PLAYER_READ_GRACE_HOOKS", 4.0f)));
        if (gLatestSharedPlayerSnapshotValid && readMisses <= graceHooks)
        {
            gNativeCellTransitionReady = false;
            gNativeEquivalentPairsInSequence = 0;
            gNativeEquivalentLastPresentFrame = -2;
            gNativePassEquivalenceWasBlocked = true;
            if (readMisses == 1)
                logLine("native stereo cell guard saw transient sequence overlap; traversal suspended without resetting cell epoch");
            return false;
        }
    }
    else
    {
        InterlockedExchange(&gNativeSharedPlayerReadMisses, 0);
    }
    const bool gameplay = snapshotReady
        && (snapshot.flags & fnvxr::shared::PlayerSharedFlagGameplay) != 0;
    const bool cellKnown = snapshotReady
        && (snapshot.flags & fnvxr::shared::PlayerSharedFlagCellKnown) != 0
        && snapshot.currentCellFormId != 0;
    if (!snapshotReady || !gameplay || !cellKnown)
    {
        const bool wasReady = gNativeCellTransitionReady;
        gLatestSharedPlayerSnapshotValid = false;
        gNativeStableCellFormId = 0;
        gNativeCellChangePlayerFrame = 0;
        gNativeDoubleTraversalHoldUntilPlayerFrame = 0;
        gNativeCellTransitionReady = false;
        gNativeEquivalentPairsInSequence = 0;
        gNativeEquivalentLastPresentFrame = -2;
        gNativePassEquivalenceWasBlocked = true;
        const LONG logCount = InterlockedIncrement(&gLoggedNativeCellTransition);
        if (wasReady || logCount <= 8 || logCount % 240 == 0)
        {
            char message[320] {};
            sprintf_s(
                message,
                "native stereo cell guard waiting snapshot=%d gameplay=%d cellKnown=%d flags=0x%lx cell=0x%08lx playerFrame=%llu sequence=%ld",
                snapshotReady ? 1 : 0,
                gameplay ? 1 : 0,
                cellKnown ? 1 : 0,
                static_cast<unsigned long>(snapshot.flags),
                static_cast<unsigned long>(snapshot.currentCellFormId),
                static_cast<unsigned long long>(snapshot.frame),
                sequence);
            logLine(message);
        }
        return false;
    }

    gLatestSharedPlayerSnapshot = snapshot;
    gLatestSharedPlayerSnapshotSequence = sequence;
    gLatestSharedPlayerSnapshotValid = true;
    const bool cellChanged = gNativeStableCellFormId == 0
        || gNativeStableCellFormId != snapshot.currentCellFormId
        || snapshot.frame < gNativeCellChangePlayerFrame;
    if (cellChanged)
    {
        const std::uint32_t previousCell = gNativeStableCellFormId;
        gNativeStableCellFormId = snapshot.currentCellFormId;
        gNativeCellChangePlayerFrame = snapshot.frame;
        gNativeDoubleTraversalHoldUntilPlayerFrame = 0;
        gNativeCellTransitionReady = false;
        gNativeEquivalentPairsInSequence = 0;
        gNativeEquivalentLastPresentFrame = -2;
        gNativePassEquivalenceWasBlocked = true;
        char message[320] {};
        sprintf_s(
            message,
            "native stereo cell transition previous=0x%08lx current=0x%08lx playerFrame=%llu sequence=%ld; double traversal suspended",
            static_cast<unsigned long>(previousCell),
            static_cast<unsigned long>(snapshot.currentCellFormId),
            static_cast<unsigned long long>(snapshot.frame),
            sequence);
        logLine(message);
    }

    const std::uint64_t settleFrames = static_cast<std::uint64_t>((std::max)(
        0.0f,
        readEnvFloat("FNVXR_D3D9_NATIVE_CELL_SETTLE_PLAYER_FRAMES", 45.0f)));
    const std::uint64_t stableFrames = snapshot.frame >= gNativeCellChangePlayerFrame
        ? snapshot.frame - gNativeCellChangePlayerFrame
        : 0;
    const bool cellSettled = stableFrames >= settleFrames;
    const bool mismatchHoldActive = snapshot.frame < gNativeDoubleTraversalHoldUntilPlayerFrame;
    const bool ready = cellSettled && !mismatchHoldActive;
    if (ready && !gNativeCellTransitionReady)
    {
        char message[320] {};
        sprintf_s(
            message,
            "native stereo traversal guard settled cell=0x%08lx playerFrame=%llu stableFrames=%llu required=%llu mismatchHoldUntil=%llu sequence=%ld",
            static_cast<unsigned long>(snapshot.currentCellFormId),
            static_cast<unsigned long long>(snapshot.frame),
            static_cast<unsigned long long>(stableFrames),
            static_cast<unsigned long long>(settleFrames),
            static_cast<unsigned long long>(gNativeDoubleTraversalHoldUntilPlayerFrame),
            sequence);
        logLine(message);
    }
    gNativeCellTransitionReady = ready;
    return ready;
}

bool matrix33LooksUsable(const float* m)
{
    if (!m)
        return false;

    float maxAbs = 0.0f;
    for (int i = 0; i < 9; ++i)
    {
        if (!std::isfinite(m[i]))
            return false;
        const float a = absFloat(m[i]);
        if (a > maxAbs)
            maxAbs = a;
    }
    if (maxAbs <= 0.0001f || maxAbs >= 4.0f)
        return false;

    const auto rowLengthSq = [m](int row) {
        const int base = row * 3;
        return m[base + 0] * m[base + 0] + m[base + 1] * m[base + 1] + m[base + 2] * m[base + 2];
    };
    const auto rowDot = [m](int a, int b) {
        const int ai = a * 3;
        const int bi = b * 3;
        return m[ai + 0] * m[bi + 0] + m[ai + 1] * m[bi + 1] + m[ai + 2] * m[bi + 2];
    };

    const float minLengthSq = readEnvFloat("FNVXR_D3D9_CAMERA_MIN_AXIS_LEN_SQ", 0.25f);
    const float maxLengthSq = readEnvFloat("FNVXR_D3D9_CAMERA_MAX_AXIS_LEN_SQ", 4.0f);
    const float maxDot = readEnvFloat("FNVXR_D3D9_CAMERA_MAX_AXIS_DOT", 0.35f);
    for (int row = 0; row < 3; ++row)
    {
        const float lengthSq = rowLengthSq(row);
        if (lengthSq < minLengthSq || lengthSq > maxLengthSq)
            return false;
    }

    return absFloat(rowDot(0, 1)) <= maxDot
        && absFloat(rowDot(0, 2)) <= maxDot
        && absFloat(rowDot(1, 2)) <= maxDot;
}

fnvxr::stereo::Matrix4 viewFromSharedCamera(const SharedCameraState& camera)
{
    fnvxr::stereo::Matrix4 view = identityMatrix();
    const bool transposeRotation = readEnvBool("FNVXR_D3D9_CAMERA_TRANSPOSE_ROTATION", true);
    const float xSign = readEnvFloat("FNVXR_D3D9_CAMERA_X_SIGN", 1.0f);
    const float ySign = readEnvFloat("FNVXR_D3D9_CAMERA_Y_SIGN", 1.0f);
    const float zSign = readEnvFloat("FNVXR_D3D9_CAMERA_Z_SIGN", 1.0f);

    float r[3][3] {};
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 3; ++column)
        {
            const int sourceIndex = transposeRotation ? column * 3 + row : row * 3 + column;
            r[row][column] = camera.worldRot[sourceIndex];
        }
    }

    const float axisSigns[3] { xSign, ySign, zSign };
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 3; ++column)
            view.m[row][column] = r[row][column] * axisSigns[column];
    }

    const float tx = camera.worldPos[0];
    const float ty = camera.worldPos[1];
    const float tz = camera.worldPos[2];
    view.m[3][0] = -(tx * view.m[0][0] + ty * view.m[1][0] + tz * view.m[2][0]);
    view.m[3][1] = -(tx * view.m[0][1] + ty * view.m[1][1] + tz * view.m[2][1]);
    view.m[3][2] = -(tx * view.m[0][2] + ty * view.m[1][2] + tz * view.m[2][2]);
    return view;
}

void updateSharedCameraState()
{
    const auto invalidateCamera = []() {
        gSharedCameraActive = false;
        gSharedCameraRawActive = false;
        gSharedCameraMatrixUsable = false;
        gLatestSharedCameraSnapshotValid = false;
    };
    const auto expireCameraIfStale = [&invalidateCamera]() {
        if (!gLatestSharedCameraSnapshotValid || gLatestSharedCameraAcceptedAt.QuadPart == 0)
            return;
        LARGE_INTEGER now {};
        QueryPerformanceCounter(&now);
        const double maximumAgeSeconds = (std::min)(
            0.025,
            (std::max)(
                0.001,
                static_cast<double>(readEnvFloat(
                    "FNVXR_D3D9_SHARED_CAMERA_MAX_AGE_MS",
                    25.0f)) / 1000.0));
        if (secondsBetween(gLatestSharedCameraAcceptedAt, now) <= maximumAgeSeconds)
            return;
        invalidateCamera();
        const LONG count = InterlockedIncrement(&gLoggedSharedCamera);
        if (count <= 12 || count % 300 == 0)
            logLine("shared camera invalidated: publisher sequence exceeded hard freshness bound");
    };
    SharedCameraState* camera = sharedCameraState();
    if (!camera || camera->magic != SharedCameraMagic || camera->version != SharedCameraVersion)
    {
        invalidateCamera();
        return;
    }

    const LONG sequenceBefore = camera->sequence;
    if ((sequenceBefore & 1) != 0 || sequenceBefore == gLastCameraSequence)
    {
        expireCameraIfStale();
        return;
    }

    MemoryBarrier();
    const SharedCameraState snapshot = *camera;
    MemoryBarrier();
    const LONG sequenceAfter = camera->sequence;
    if (sequenceBefore != sequenceAfter || (sequenceAfter & 1) != 0)
    {
        expireCameraIfStale();
        return;
    }

    const bool rawCameraActive = snapshot.active != 0;
    const bool thirdPersonAllowed = readEnvBool("FNVXR_D3D9_ALLOW_THIRD_PERSON_STEREO", false);
    const bool firstPersonAccepted = snapshot.thirdPerson == 0 || thirdPersonAllowed;
    const bool strictUsableCameraMatrix = matrix33LooksUsable(snapshot.worldRot)
        && finiteArray(snapshot.worldPos, 3);
    const bool usableCameraMatrix = strictUsableCameraMatrix && firstPersonAccepted;
    gLastCameraSequence = sequenceAfter;
    gLatestSharedCameraSnapshot = snapshot;
    gLatestSharedCameraSnapshotSequence = sequenceAfter;
    gLatestSharedCameraSnapshotValid = snapshot.frame != 0 && strictUsableCameraMatrix;
    QueryPerformanceCounter(&gLatestSharedCameraAcceptedAt);
    gSharedCameraRawActive = rawCameraActive;
    gSharedCameraMatrixUsable = usableCameraMatrix;
    gSharedCameraActive = gLatestSharedCameraSnapshotValid && rawCameraActive && usableCameraMatrix;
    if (gSharedCameraActive)
        gSharedCameraView = viewFromSharedCamera(snapshot);

    const LONG logCount = InterlockedIncrement(&gLoggedSharedCamera);
    if (logCount <= 12 || logCount % 300 == 0)
    {
        char message[384] {};
        sprintf_s(
            message,
            "shared camera seq=%ld rawActive=%d matrixStrict=%d firstPerson=%d matrixAccepted=%d accepted=%d third=%lu pos=(%.3f %.3f %.3f) rot=[%.3f %.3f %.3f | %.3f %.3f %.3f | %.3f %.3f %.3f] viewT=(%.3f %.3f %.3f)",
            sequenceAfter,
            rawCameraActive ? 1 : 0,
            strictUsableCameraMatrix ? 1 : 0,
            firstPersonAccepted ? 1 : 0,
            usableCameraMatrix ? 1 : 0,
            gSharedCameraActive ? 1 : 0,
            static_cast<unsigned long>(snapshot.thirdPerson),
            snapshot.worldPos[0],
            snapshot.worldPos[1],
            snapshot.worldPos[2],
            snapshot.worldRot[0],
            snapshot.worldRot[1],
            snapshot.worldRot[2],
            snapshot.worldRot[3],
            snapshot.worldRot[4],
            snapshot.worldRot[5],
            snapshot.worldRot[6],
            snapshot.worldRot[7],
            snapshot.worldRot[8],
            gSharedCameraView.m[3][0],
            gSharedCameraView.m[3][1],
            gSharedCameraView.m[3][2]);
        logLine(message);
    }
}

bool nativeStereoFirstPersonReady()
{
    updateSharedCameraState();
    if (!readEnvBool("FNVXR_D3D9_NATIVE_REQUIRE_FIRST_PERSON", true))
        return true;

    const bool ready = gLatestSharedCameraSnapshotValid
        && gSharedCameraActive
        && gLatestSharedCameraSnapshot.active != 0
        && gLatestSharedCameraSnapshot.thirdPerson == 0;
    if (!ready)
    {
        const LONG count = InterlockedIncrement(&gLoggedNativeFirstPersonGate);
        if (count <= 24 || count % 120 == 0)
        {
            char message[320] {};
            sprintf_s(
                message,
                "native stereo first-person gate rejected count=%ld snapshotValid=%d sharedActive=%d rawActive=%d matrixUsable=%d snapshotActive=%lu third=%lu seq=%ld",
                count,
                gLatestSharedCameraSnapshotValid ? 1 : 0,
                gSharedCameraActive ? 1 : 0,
                gSharedCameraRawActive ? 1 : 0,
                gSharedCameraMatrixUsable ? 1 : 0,
                static_cast<unsigned long>(gLatestSharedCameraSnapshot.active),
                static_cast<unsigned long>(gLatestSharedCameraSnapshot.thirdPerson),
                gLatestSharedCameraSnapshotSequence);
            logLine(message);
        }
    }
    return ready;
}

void updateSharedVrPose()
{
    if (!d3d9VrPoseInputEnabled())
        return;

    SharedVrPoseState pose {};
    LONG sequence = 0;
    if (!readStableSharedVrPose(pose, sequence))
    {
        // Never keep applying the last good HMD transform after its shared
        // transaction has become unreadable or stale. The retained origin is
        // harmless; rendering stays closed until a fresh tracked pose arrives.
        gLatestVrPoseSnapshotValid = false;
        gNativeStereoRenderedPoseValid = false;
        gHaveVrBodyFrame = false;
        // Preserve the last authoritative body origin across a transient
        // seqlock overlap. Rendering and the public rig lease close now; the
        // next fresh pose resumes in the same frame instead of recentering.
        if (gPublishedVrOriginActive || gPublishedVrOriginCommitted)
            publishSharedVrOrigin(false);
        return;
    }

    const bool poseSequenceAdvanced = sequence != gLastVrPoseSequence;
    const bool producerEpochChanged = gLastVrProducerEpoch != 0
        && pose.producerEpoch != gLastVrProducerEpoch;
    const bool referenceGenerationChanged = gLastVrReferenceSpaceGeneration != 0
        && pose.referenceSpaceGeneration != gLastVrReferenceSpaceGeneration;
    if (referenceGenerationChanged || producerEpochChanged)
    {
        gHaveVrPoseOrigin = false;
        gHaveVrBodyFrame = false;
        gNativeGameplayPoseOriginLatched = false;
        gNativeViewOriginValid = false;
        // A new producer owns a fresh mailbox namespace. A generation change
        // from the same producer already relatches the reference space, so
        // acknowledge its current request instead of replaying that edge.
        gNativeLastRecenterRequestId = producerEpochChanged
            ? 0u
            : pose.recenterRequestId;
        publishSharedVrOrigin(false, &pose, sequence);
        char message[192] {};
        sprintf_s(
            message,
            "shared VR producer/reference changed epoch=%llu->%llu generation=%lu->%lu; invalidating camera and rig origin",
            static_cast<unsigned long long>(gLastVrProducerEpoch),
            static_cast<unsigned long long>(pose.producerEpoch),
            static_cast<unsigned long>(gLastVrReferenceSpaceGeneration),
            static_cast<unsigned long>(pose.referenceSpaceGeneration));
        logLine(message);
    }
    gLastVrReferenceSpaceGeneration = pose.referenceSpaceGeneration;
    gLastVrProducerEpoch = pose.producerEpoch;

    if (gLastVrPoseFrame != 0 && pose.frame < gLastVrPoseFrame)
    {
        gHaveVrPoseOrigin = false;
        gHaveVrBodyFrame = false;
        gNativeGameplayPoseOriginLatched = false;
        gNativeViewOriginValid = false;
        // Frame regression forces the same relatch; do not consume the same
        // monotonic recenter request twice after the reset.
        gNativeLastRecenterRequestId = pose.recenterRequestId;
        publishSharedVrOrigin(false, &pose, sequence);
        logLine("shared VR pose frame regressed; resetting origin");
    }

    gLastVrPoseSequence = sequence;
    gLastVrPoseFrame = pose.frame;
    gLatestVrPoseSnapshot = pose;
    gLatestVrPoseSnapshotSequence = sequence;
    gLatestVrPoseDisplayTime = pose.predictedDisplayTime;
    gLatestVrPoseSnapshotValid = true;
    if (poseSequenceAdvanced)
        QueryPerformanceCounter(&gLatestVrPoseAcceptedAt);
    float rawCenter[3] {
        (pose.leftEyePos[0] + pose.rightEyePos[0]) * 0.5f,
        (pose.leftEyePos[1] + pose.rightEyePos[1]) * 0.5f,
        (pose.leftEyePos[2] + pose.rightEyePos[2]) * 0.5f
    };
    const fnvxr::stereo::EyeBaselineValidation eyeBaseline =
        fnvxr::stereo::validateEyeBaseline(
            { pose.hmdRot[0], pose.hmdRot[1], pose.hmdRot[2], pose.hmdRot[3] },
            { pose.leftEyePos[0], pose.leftEyePos[1], pose.leftEyePos[2] },
            { pose.rightEyePos[0], pose.rightEyePos[1], pose.rightEyePos[2] },
            { pose.hmdPos[0], pose.hmdPos[1], pose.hmdPos[2] });
    if (!eyeBaseline.valid)
    {
        gLatestVrPoseSnapshotValid = false;
        gNativeStereoRenderedPoseValid = false;
        gHaveVrBodyFrame = false;
        if (gPublishedVrOriginActive || gPublishedVrOriginCommitted)
            publishSharedVrOrigin(false);
        const LONG invalidCount = InterlockedIncrement(&gLoggedSharedVrPoseInvalid);
        if (invalidCount <= 16 || invalidCount % 300 == 0)
        {
            char message[320] {};
            sprintf_s(
                message,
                "shared VR pose rejected: invalid signed eye baseline length=%.6f headLocal=(%.6f %.6f %.6f)",
                eyeBaseline.lengthMeters,
                eyeBaseline.headLocalMeters.x,
                eyeBaseline.headLocalMeters.y,
                eyeBaseline.headLocalMeters.z);
            logLine(message);
        }
        return;
    }
    gLatestRawIpdMeters = eyeBaseline.lengthMeters;
    if (!gHaveVrPoseOrigin)
    {
        gVrPoseOrigin[0] = rawCenter[0];
        gVrPoseOrigin[1] = rawCenter[1];
        gVrPoseOrigin[2] = rawCenter[2];
        float capturedHmdRot[4] {};
        copyNormalizedQuat(capturedHmdRot, pose.hmdRot);
        captureVrWorldYawCorrection(capturedHmdRot);
        const fnvxr::stereo::Quaternion yawOrigin =
            fnvxr::stereo::gravityAlignedYawOrientation(
                { capturedHmdRot[0], capturedHmdRot[1], capturedHmdRot[2], capturedHmdRot[3] },
                { gVrPoseOriginRot[0], gVrPoseOriginRot[1], gVrPoseOriginRot[2], gVrPoseOriginRot[3] });
        gVrPoseOriginRot[0] = yawOrigin.x;
        gVrPoseOriginRot[1] = yawOrigin.y;
        gVrPoseOriginRot[2] = yawOrigin.z;
        gVrPoseOriginRot[3] = yawOrigin.w;
        gHaveVrPoseOrigin = true;
        gVrPoseOriginPoseSequence = sequence;
        gVrPoseOriginPoseFrame = pose.frame;
        gVrPoseOriginGeneration = pose.referenceSpaceGeneration;
        gVrPoseOriginProducerEpoch = pose.producerEpoch;
        gHaveVrBodyFrame = false;
        logLine("shared VR body origin latched");
    }

    const fnvxr::stereo::Quaternion originOrientation {
        gVrPoseOriginRot[0],
        gVrPoseOriginRot[1],
        gVrPoseOriginRot[2],
        gVrPoseOriginRot[3]
    };
    const fnvxr::stereo::Vector3 originPosition {
        gVrPoseOrigin[0],
        gVrPoseOrigin[1],
        gVrPoseOrigin[2]
    };
    const fnvxr::stereo::Vector3 currentHeadPosition {
        rawCenter[0],
        rawCenter[1],
        rawCenter[2]
    };
    const fnvxr::stereo::Vector3 headInOrigin = fnvxr::stereo::positionInOriginFrame(
        originOrientation,
        originPosition,
        currentHeadPosition);
    gLatestHeadLocalMeters[0] = headInOrigin.x;
    gLatestHeadLocalMeters[1] = headInOrigin.y;
    gLatestHeadLocalMeters[2] = headInOrigin.z;
    gLatestHmdDelta[0] = gLatestHeadLocalMeters[0];
    gLatestHmdDelta[1] = gLatestHeadLocalMeters[1];
    gLatestHmdDelta[2] = gLatestHeadLocalMeters[2];

    const float rawLeftEyeLocal[3] {
        pose.leftEyePos[0] - rawCenter[0],
        pose.leftEyePos[1] - rawCenter[1],
        pose.leftEyePos[2] - rawCenter[2]
    };
    const float rawRightEyeLocal[3] {
        pose.rightEyePos[0] - rawCenter[0],
        pose.rightEyePos[1] - rawCenter[1],
        pose.rightEyePos[2] - rawCenter[2]
    };
    const fnvxr::stereo::Vector3 leftEyeInOrigin = fnvxr::stereo::vectorInOriginFrame(
        originOrientation,
        { rawLeftEyeLocal[0], rawLeftEyeLocal[1], rawLeftEyeLocal[2] });
    const fnvxr::stereo::Vector3 rightEyeInOrigin = fnvxr::stereo::vectorInOriginFrame(
        originOrientation,
        { rawRightEyeLocal[0], rawRightEyeLocal[1], rawRightEyeLocal[2] });
    gLatestLeftEyeLocalMeters[0] = leftEyeInOrigin.x;
    gLatestLeftEyeLocalMeters[1] = leftEyeInOrigin.y;
    gLatestLeftEyeLocalMeters[2] = leftEyeInOrigin.z;
    gLatestRightEyeLocalMeters[0] = rightEyeInOrigin.x;
    gLatestRightEyeLocalMeters[1] = rightEyeInOrigin.y;
    gLatestRightEyeLocalMeters[2] = rightEyeInOrigin.z;

    float currentRot[4] {};
    copyNormalizedQuat(currentRot, pose.hmdRot);
    correctedVrWorldQuat(gLatestHmdRotDelta, currentRot);
    gHaveVrBodyFrame = true;

    if ((pose.frame <= 10 || (pose.frame % 15) == 0)
        && pose.frame != gLastHeadBodyInvariantPoseFrame)
    {
        gLastHeadBodyInvariantPoseFrame = pose.frame;
        const float qx = gLatestHmdRotDelta[0];
        const float qy = gLatestHmdRotDelta[1];
        const float qz = gLatestHmdRotDelta[2];
        const float qw = gLatestHmdRotDelta[3];
        const float hmdYaw = fnvxr::stereo::xrHeadingYawRadians({ qx, qy, qz, qw });
        const float playerYaw = gLatestSharedPlayerSnapshotValid
            ? atan2f(
                gLatestSharedPlayerSnapshot.playerWorldRot[3],
                gLatestSharedPlayerSnapshot.playerWorldRot[0])
            : 0.0f;
        const float cameraYaw = gLatestSharedPlayerSnapshotValid
            ? atan2f(
                gLatestSharedPlayerSnapshot.cameraWorldRot[3],
                gLatestSharedPlayerSnapshot.cameraWorldRot[0])
            : 0.0f;
        const float eyeCenterError =
            absFloat(gLatestLeftEyeLocalMeters[0] + gLatestRightEyeLocalMeters[0])
            + absFloat(gLatestLeftEyeLocalMeters[1] + gLatestRightEyeLocalMeters[1])
            + absFloat(gLatestLeftEyeLocalMeters[2] + gLatestRightEyeLocalMeters[2]);
        char invariant[640] {};
        sprintf_s(
            invariant,
            "{\"event\":\"fnvxrHeadBodyInvariant\",\"poseFrame\":%llu,\"poseSeq\":%ld,\"playerValid\":%s,\"hmdYaw\":%.7f,\"playerYaw\":%.7f,\"cameraYaw\":%.7f,\"headLocal\":[%.6f,%.6f,%.6f],\"eyeCenterError\":%.8f,\"rawIpdMeters\":%.6f}",
            static_cast<unsigned long long>(pose.frame),
            sequence,
            gLatestSharedPlayerSnapshotValid ? "true" : "false",
            hmdYaw,
            playerYaw,
            cameraYaw,
            gLatestHeadLocalMeters[0],
            gLatestHeadLocalMeters[1],
            gLatestHeadLocalMeters[2],
            eyeCenterError,
            gLatestRawIpdMeters);
        logLine(invariant);
    }

    const LONG logCount = InterlockedIncrement(&gLoggedSharedVrPose);
    if (logCount <= 12 || logCount % 300 == 0)
    {
        char message[512] {};
        sprintf_s(
            message,
            "shared VR pose seq=%ld frame=%llu hmdPos=(%.3f %.3f %.3f) localHead=(%.4f %.4f %.4f) leftEyeLocal=(%.4f %.4f %.4f) rightEyeLocal=(%.4f %.4f %.4f) rawIpd=%.4f",
            sequence,
            static_cast<unsigned long long>(pose.frame),
            pose.hmdPos[0],
            pose.hmdPos[1],
            pose.hmdPos[2],
            gLatestHeadLocalMeters[0],
            gLatestHeadLocalMeters[1],
            gLatestHeadLocalMeters[2],
            gLatestLeftEyeLocalMeters[0],
            gLatestLeftEyeLocalMeters[1],
            gLatestLeftEyeLocalMeters[2],
            gLatestRightEyeLocalMeters[0],
            gLatestRightEyeLocalMeters[1],
            gLatestRightEyeLocalMeters[2],
            gLatestRawIpdMeters);
        logLine(message);
    }
}

const fnvxr::stereo::Matrix4& baseViewForStereo()
{
    if (readEnvBool("FNVXR_D3D9_USE_SHARED_CAMERA_VIEW", true) && gSharedCameraActive)
        return gSharedCameraView;
    return gBaseView;
}

fnvxr::stereo::Matrix4 toStereoMatrix(const D3DMATRIX& matrix)
{
    fnvxr::stereo::Matrix4 result {};
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
            result.m[row][column] = matrix.m[row][column];
    }
    return result;
}

D3DMATRIX toD3DMatrix(const fnvxr::stereo::Matrix4& matrix)
{
    D3DMATRIX result {};
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
            result.m[row][column] = matrix.m[row][column];
    }
    return result;
}

void updateEyeMatrices()
{
    if (!gHaveView || !gHaveProjection)
        return;

    updateSharedCameraState();
    const fnvxr::stereo::Matrix4& stereoBaseView = baseViewForStereo();
    const bool singleTraversalActive = gInNativeStereoHook && gNativeActiveEye == 2;
    const float effectiveIpdUnits = singleTraversalActive
            && std::isfinite(gNativeSingleTraversalIpdUnits)
            && gNativeSingleTraversalIpdUnits > 0.1f
            && gNativeSingleTraversalIpdUnits < 12.0f
        ? gNativeSingleTraversalIpdUnits
        : gIpdGameUnits;
    gEyeMatrices = fnvxr::stereo::makeEyeMatrices(
        stereoBaseView,
        gBaseProjection,
        effectiveIpdUnits);
    if (singleTraversalActive
        && readEnvBool("FNVXR_D3D9_NATIVE_ASYMMETRIC_FOV", true))
    {
        const float leftTangents[4] {
            std::tan(gNativeSingleTraversalFov[0][0]),
            std::tan(gNativeSingleTraversalFov[0][1]),
            std::tan(gNativeSingleTraversalFov[0][2]),
            std::tan(gNativeSingleTraversalFov[0][3])
        };
        const float rightTangents[4] {
            std::tan(gNativeSingleTraversalFov[1][0]),
            std::tan(gNativeSingleTraversalFov[1][1]),
            std::tan(gNativeSingleTraversalFov[1][2]),
            std::tan(gNativeSingleTraversalFov[1][3])
        };
        gEyeMatrices.leftProjection = fnvxr::stereo::projectionFromTangents(
            gBaseProjection,
            leftTangents[0],
            leftTangents[1],
            leftTangents[2],
            leftTangents[3]);
        gEyeMatrices.rightProjection = fnvxr::stereo::projectionFromTangents(
            gBaseProjection,
            rightTangents[0],
            rightTangents[1],
            rightTangents[2],
            rightTangents[3]);
    }
    const bool usingSharedCamera = readEnvBool("FNVXR_D3D9_USE_SHARED_CAMERA_VIEW", true) && gSharedCameraActive;
    const bool stereoSuppressedForUi = suppressStereoForUiMode();
    const bool screenSpaceProjection = currentProjectionLooksScreenSpace();
    const bool requestedHmdPose = readEnvBool("FNVXR_D3D9_APPLY_HMD_POSE", false);
    bool applyHmdPose =
        requestedHmdPose
        && !stereoSuppressedForUi
        && !screenSpaceProjection
        && gHaveVrBodyFrame;
    if (applyHmdPose)
    {
        const fnvxr::stereo::EyeMatrices fixedIpdMatrices = gEyeMatrices;
        gEyeMatrices.leftView = makeLocalVrEyeView(stereoBaseView, gLatestLeftEyeLocalMeters, true);
        gEyeMatrices.rightView = makeLocalVrEyeView(stereoBaseView, gLatestRightEyeLocalMeters, true);

        const float maxLocalViewOffsetUnits =
            readEnvFloat("FNVXR_D3D9_MAX_LOCAL_VIEW_OFFSET_UNITS", 400.0f);
        const float leftDelta = viewTranslationDeltaMagnitude(gEyeMatrices.leftView, stereoBaseView);
        const float rightDelta = viewTranslationDeltaMagnitude(gEyeMatrices.rightView, stereoBaseView);
        if ((maxLocalViewOffsetUnits > 0.0f)
            && (leftDelta > maxLocalViewOffsetUnits || rightDelta > maxLocalViewOffsetUnits))
        {
            gEyeMatrices = fixedIpdMatrices;
            applyHmdPose = false;
            const LONG rejectCount = InterlockedIncrement(&gLoggedEyePoseScale);
            if (rejectCount <= 12 || rejectCount % 300 == 0)
            {
                char reject[256] {};
                sprintf_s(
                    reject,
                    "local VR eye view rejected frame=%ld leftDelta=%.4f rightDelta=%.4f max=%.4f localHead=(%.4f %.4f %.4f)",
                    static_cast<LONG>(gPresentFrames),
                    leftDelta,
                    rightDelta,
                    maxLocalViewOffsetUnits,
                    gLatestHeadLocalMeters[0],
                    gLatestHeadLocalMeters[1],
                    gLatestHeadLocalMeters[2]);
                logLine(reject);
            }
        }
    }
    const LONG poseScaleLogCount = InterlockedIncrement(&gLoggedEyePoseScale);
    if (poseScaleLogCount <= 20 || poseScaleLogCount % 300 == 0)
    {
        const float headPositionScale = readEnvFloat("FNVXR_D3D9_HEAD_POSITION_SCALE", 1.0f);
        const float stereoScale = readEnvFloat("FNVXR_D3D9_STEREO_SCALE", 1.0f);
        const float effectiveIpdUnits = gHaveVrBodyFrame
            ? gLatestRawIpdMeters * gGameUnitsPerMeter * stereoScale
            : gIpdGameUnits;
        char poseAxisMode[32] {};
        readPoseAxisMode(poseAxisMode, sizeof(poseAxisMode));
        float leftLocalXrMeters[3] {};
        float rightLocalXrMeters[3] {};
        float leftLocalGameMeters[3] {};
        float rightLocalGameMeters[3] {};
        composeLocalXrMeters(gLatestLeftEyeLocalMeters, leftLocalXrMeters);
        composeLocalXrMeters(gLatestRightEyeLocalMeters, rightLocalXrMeters);
        xrMetersToGamebryoMeters(leftLocalXrMeters, leftLocalGameMeters);
        xrMetersToGamebryoMeters(rightLocalXrMeters, rightLocalGameMeters);
        char proof[1280] {};
        sprintf_s(
            proof,
            "{\"event\":\"fnvxrD3d9EyePoseScale\",\"frame\":%ld,\"headPositionScale\":%.4f,\"stereoScale\":%.4f,\"rawIpdMeters\":%.4f,\"effectiveIpdUnits\":%.4f,\"gameUnitsPerMeter\":%.4f,\"poseAxisMode\":\"%s\",\"sharedCamera\":%s,\"hmdPoseRequested\":%s,\"hmdPoseApplied\":%s,\"bodyFrameValid\":%s,\"uiSuppressed\":%s,\"screenSpaceProjection\":%s,\"headLocalMeters\":[%.4f,%.4f,%.4f],\"leftEyeLocalMeters\":[%.4f,%.4f,%.4f],\"rightEyeLocalMeters\":[%.4f,%.4f,%.4f],\"leftLocalGameMeters\":[%.4f,%.4f,%.4f],\"rightLocalGameMeters\":[%.4f,%.4f,%.4f],\"center\":[%.4f,%.4f,%.4f],\"leftOffset\":[%.4f,%.4f,%.4f],\"rightOffset\":[%.4f,%.4f,%.4f]}",
            static_cast<LONG>(gPresentFrames),
            headPositionScale,
            stereoScale,
            gHaveVrBodyFrame ? gLatestRawIpdMeters : gIpdMeters,
            effectiveIpdUnits,
            gGameUnitsPerMeter,
            poseAxisMode,
            usingSharedCamera ? "true" : "false",
            requestedHmdPose ? "true" : "false",
            applyHmdPose ? "true" : "false",
            gHaveVrBodyFrame ? "true" : "false",
            stereoSuppressedForUi ? "true" : "false",
            screenSpaceProjection ? "true" : "false",
            gLatestHeadLocalMeters[0],
            gLatestHeadLocalMeters[1],
            gLatestHeadLocalMeters[2],
            gLatestLeftEyeLocalMeters[0],
            gLatestLeftEyeLocalMeters[1],
            gLatestLeftEyeLocalMeters[2],
            gLatestRightEyeLocalMeters[0],
            gLatestRightEyeLocalMeters[1],
            gLatestRightEyeLocalMeters[2],
            leftLocalGameMeters[0],
            leftLocalGameMeters[1],
            leftLocalGameMeters[2],
            rightLocalGameMeters[0],
            rightLocalGameMeters[1],
            rightLocalGameMeters[2],
            stereoBaseView.m[3][0],
            stereoBaseView.m[3][1],
            stereoBaseView.m[3][2],
            gEyeMatrices.leftView.m[3][0] - stereoBaseView.m[3][0],
            gEyeMatrices.leftView.m[3][1] - stereoBaseView.m[3][1],
            gEyeMatrices.leftView.m[3][2] - stereoBaseView.m[3][2],
            gEyeMatrices.rightView.m[3][0] - stereoBaseView.m[3][0],
            gEyeMatrices.rightView.m[3][1] - stereoBaseView.m[3][1],
            gEyeMatrices.rightView.m[3][2] - stereoBaseView.m[3][2]);
        logLine(proof);
    }
    const LONG logCount = InterlockedIncrement(&gLoggedEyeMatrices);
    if (logCount > 12)
        return;

    char message[640] {};
    sprintf_s(
        message,
        "derived stereo eye matrices frame=%ld ipdMeters=%.4f ipdGame=%.4f gameUnitsPerMeter=%.4f sharedCamera=%d bodyFrame=%d hmdPoseApplied=%d hmdDelta=(%.4f %.4f %.4f) hmdRotDelta=(%.4f %.4f %.4f %.4f) baseViewT=(%.4f %.4f %.4f) leftViewT=(%.4f %.4f %.4f) rightViewT=(%.4f %.4f %.4f)",
        static_cast<LONG>(gPresentFrames),
        gIpdMeters,
        gIpdGameUnits,
        gGameUnitsPerMeter,
        static_cast<int>(usingSharedCamera),
        gHaveVrBodyFrame ? 1 : 0,
        applyHmdPose ? 1 : 0,
        gLatestHmdDelta[0],
        gLatestHmdDelta[1],
        gLatestHmdDelta[2],
        gLatestHmdRotDelta[0],
        gLatestHmdRotDelta[1],
        gLatestHmdRotDelta[2],
        gLatestHmdRotDelta[3],
        stereoBaseView.m[3][0],
        stereoBaseView.m[3][1],
        stereoBaseView.m[3][2],
        gEyeMatrices.leftView.m[3][0],
        gEyeMatrices.leftView.m[3][1],
        gEyeMatrices.leftView.m[3][2],
        gEyeMatrices.rightView.m[3][0],
        gEyeMatrices.rightView.m[3][1],
        gEyeMatrices.rightView.m[3][2]);
    logLine(message);
}

float matrixMaxAbs(const fnvxr::stereo::Matrix4& matrix)
{
    float maxValue = 0.0f;
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            const float value = absFloat(matrix.m[row][column]);
            if (value > maxValue)
                maxValue = value;
        }
    }
    return maxValue;
}

float matrixMaxAbsDifference(
    const fnvxr::stereo::Matrix4& a,
    const fnvxr::stereo::Matrix4& b)
{
    float maxDifference = 0.0f;
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            if (!std::isfinite(a.m[row][column]) || !std::isfinite(b.m[row][column]))
                return std::numeric_limits<float>::infinity();
            const float difference = absFloat(a.m[row][column] - b.m[row][column]);
            if (!std::isfinite(difference))
                return std::numeric_limits<float>::infinity();
            if (difference > maxDifference)
                maxDifference = difference;
        }
    }
    return maxDifference;
}

bool shaderMatrixLooksTransform(const fnvxr::stereo::Matrix4& matrix, const char*& reason)
{
    if (!fnvxr::stereo::isFinite(matrix))
    {
        reason = "nonfinite";
        return false;
    }

    const float maxAbs = matrixMaxAbs(matrix);
    const float maxAllowed = readEnvFloat("FNVXR_D3D9_SHADER_MATRIX_MAX_ABS", 1000000000.0f);
    if (maxAbs <= 0.000001f)
    {
        reason = "zero";
        return false;
    }
    if (maxAbs > maxAllowed)
    {
        reason = "too_large";
        return false;
    }

    int nonZeroRows = 0;
    for (int row = 0; row < 4; ++row)
    {
        const float rowMagnitude =
            absFloat(matrix.m[row][0])
            + absFloat(matrix.m[row][1])
            + absFloat(matrix.m[row][2])
            + absFloat(matrix.m[row][3]);
        if (rowMagnitude > 0.0001f)
            ++nonZeroRows;
    }
    if (nonZeroRows < 3)
    {
        reason = "sparse";
        return false;
    }

    reason = "ok";
    return true;
}

void logShaderStereoSkip(const char* reason)
{
    const LONG skipCount = InterlockedIncrement(&gLoggedShaderStereoSkipped);
    if (skipCount <= 12 || skipCount % 5000 == 0)
    {
        char message[160] {};
        sprintf_s(message, "shader stereo skipped count=%ld reason=%s", skipCount, reason ? reason : "unknown");
        logLine(message);
    }
}

bool haveTrackedShaderMatrixAt(UINT startRegister)
{
    return startRegister + 3 < MaxTrackedVsConstants
        && gHaveVsConstants[startRegister + 0]
        && gHaveVsConstants[startRegister + 1]
        && gHaveVsConstants[startRegister + 2]
        && gHaveVsConstants[startRegister + 3];
}

fnvxr::stereo::Matrix4 trackedShaderMatrixAt(UINT startRegister)
{
    fnvxr::stereo::Matrix4 matrix {};
    for (UINT row = 0; row < 4; ++row)
    {
        for (UINT column = 0; column < 4; ++column)
            matrix.m[row][column] = gVsConstants[startRegister + row][column];
    }
    return matrix;
}

void trackedShaderRowsAt(UINT startRegister, float rows[4][4])
{
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
            rows[row][column] = gVsConstants[startRegister + row][column];
    }
}

void readShaderMatrixOrder(char* order, size_t orderSize)
{
    if (!order || orderSize == 0)
        return;

    if (const ShaderWvpContract* contract = currentShaderWvpContract())
    {
        strcpy_s(order, orderSize, contract->columnVector ? "column" : "row");
        return;
    }

    if (GetEnvironmentVariableA("FNVXR_D3D9_SHADER_MATRIX_ORDER", order, static_cast<DWORD>(orderSize)) == 0)
        strcpy_s(order, orderSize, "column");
}

fnvxr::stereo::Matrix4 applyShaderMatrixDelta(
    const fnvxr::stereo::Matrix4& shaderMatrix,
    const fnvxr::stereo::Matrix4& inverseBaseViewProjection,
    const fnvxr::stereo::Matrix4& eyeViewProjection)
{
    char order[32] {};
    readShaderMatrixOrder(order, sizeof(order));

    if (_stricmp(order, "column") == 0 || _stricmp(order, "column-vector") == 0)
        return multiplyMatrix(multiplyMatrix(eyeViewProjection, inverseBaseViewProjection), shaderMatrix);

    return multiplyMatrix(multiplyMatrix(shaderMatrix, inverseBaseViewProjection), eyeViewProjection);
}

void matrixToRows(const fnvxr::stereo::Matrix4& matrix, float rows[4][4])
{
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
            rows[row][column] = matrix.m[row][column];
    }
}

bool setEyeShaderWvpConstants(
    IDirect3DDevice9* device,
    const D3DMATRIX& originalView,
    const D3DMATRIX& originalProjection,
    const D3DMATRIX& eyeViewD3d,
    const D3DMATRIX& eyeProjectionD3d,
    int eye,
    const char* eyeName)
{
    if (!device || !gRealSetVertexShaderConstantF)
        return false;
    if (!readEnvBool("FNVXR_D3D9_SHADER_WVP_REPLAY", true))
        return false;
    if (!currentVertexShaderAllowedForStereoPatch())
        return false;
    if (!gHaveView || !gHaveProjection)
    {
        logShaderStereoSkip("wvp_no_base_view_projection");
        return false;
    }

    const ShaderWvpContract* contract = currentShaderWvpContract();
    if (!contract && !readEnvBool("FNVXR_D3D9_SHADER_ALLOW_UNVERIFIED_PATCHES", false))
    {
        logShaderStereoSkip("wvp_missing_verified_contract");
        return false;
    }
    const UINT start = contract
        ? contract->startRegister
        : static_cast<UINT>(readEnvFloat("FNVXR_D3D9_SHADER_WVP_START_REGISTER", 0.0f));
    if (!haveTrackedShaderMatrixAt(start))
    {
        logShaderStereoSkip("wvp_c0_c3_not_cached");
        return false;
    }

    const fnvxr::stereo::Matrix4 originalWvp = trackedShaderMatrixAt(start);
    const char* rejectReason = nullptr;
    if (readEnvBool("FNVXR_D3D9_SHADER_WVP_VALIDATE", true)
        && !shaderMatrixLooksTransform(originalWvp, rejectReason))
    {
        logShaderStereoSkip(rejectReason ? rejectReason : "wvp_rejected");
        return false;
    }

    updateSharedCameraState();
    const fnvxr::stereo::Matrix4 originalViewMatrix = toStereoMatrix(originalView);
    const fnvxr::stereo::Matrix4 originalProjectionMatrix = toStereoMatrix(originalProjection);
    const fnvxr::stereo::Matrix4 eyeView = toStereoMatrix(eyeViewD3d);
    const fnvxr::stereo::Matrix4 eyeProjection = toStereoMatrix(eyeProjectionD3d);
    const bool nativeExactViewProjection =
        gInNativeStereoHook
        && gNativeActiveEye == 2
        && gNativeSingleTraversalViewProjectionValid
        && eye >= 0
        && eye <= 1;
    const fnvxr::stereo::Matrix4 baseViewProjection = nativeExactViewProjection
        ? gNativeSingleTraversalCenterViewProjection
        : multiplyMatrix(originalViewMatrix, originalProjectionMatrix);
    const fnvxr::stereo::Matrix4 eyeViewProjection = nativeExactViewProjection
        ? gNativeSingleTraversalEyeViewProjection[eye]
        : multiplyMatrix(eyeView, eyeProjection);
    const bool columnVector = contract ? contract->columnVector : true;
    const fnvxr::stereo::ViewProjectionDelta viewProjectionDelta =
        fnvxr::stereo::makeViewProjectionDelta(
            baseViewProjection,
            eyeViewProjection,
            columnVector);
    if (!viewProjectionDelta.valid)
    {
        logShaderStereoSkip("wvp_view_projection_delta_unstable");
        return false;
    }
    const fnvxr::stereo::Matrix4 patchedWvp =
        fnvxr::stereo::applyViewProjectionDelta(
            originalWvp,
            viewProjectionDelta.matrix,
            columnVector);
    if (!fnvxr::stereo::isFinite(patchedWvp))
    {
        logShaderStereoSkip("wvp_nonfinite");
        return false;
    }
    const double maximumPatchedWvpAbsoluteError = static_cast<double>(
        readEnvFloat("FNVXR_D3D9_WVP_MAX_PATCH_ABS_ERROR", 0.01f));
    const double maximumPatchedWvpNormalizedError = static_cast<double>(
        readEnvFloat("FNVXR_D3D9_WVP_MAX_PATCH_NORMALIZED_ERROR", 0.0000001f));
    const fnvxr::stereo::ViewProjectionPatchValidation patchValidation =
        fnvxr::stereo::validateAppliedViewProjectionDelta(
            originalWvp,
            patchedWvp,
            viewProjectionDelta,
            columnVector,
            maximumPatchedWvpAbsoluteError,
            maximumPatchedWvpNormalizedError);
    if (!patchValidation.valid)
    {
        const LONG failure = InterlockedIncrement(&gLoggedShaderStereoSkipped);
        if (failure <= 48 || failure % 5000 == 0)
        {
            char message[320] {};
            sprintf_s(
                message,
                "shader stereo skipped count=%ld reason=wvp_patched_matrix_precision maxAbsError=%.9g allowedAbs=%.9g normalizedError=%.9g allowedNormalized=%.9g",
                failure,
                patchValidation.maximumAbsoluteError,
                maximumPatchedWvpAbsoluteError,
                patchValidation.normalizedError,
                maximumPatchedWvpNormalizedError);
            logLine(message);
        }
        return false;
    }

    float rows[4][4] {};
    matrixToRows(patchedWvp, rows);
    const HRESULT result = gRealSetVertexShaderConstantF(device, start, &rows[0][0], 4);
    const LONG logCount = InterlockedIncrement(&gLoggedShaderWvpReplay);
    const bool hammerLog = shouldTelemetryHammerLog(logCount, "FNVXR_D3D9_WVP_TELEMETRY_STRIDE", 1);
    if (logCount <= 24 || logCount % 5000 == 0 || FAILED(result) || hammerLog)
    {
        char order[32] {};
        readShaderMatrixOrder(order, sizeof(order));
        char message[768] {};
        sprintf_s(
            message,
            "shader WVP replay count=%ld eye=%s start=%u result=0x%08lx order=%s exactNativeVp=%d vsHash=0x%08x psHash=0x%08x delta=%.6f baseT=(%.4f %.4f %.4f) eyeT=(%.4f %.4f %.4f) old03=%.5f old30=%.5f new03=%.5f new30=%.5f",
            logCount,
            eyeName ? eyeName : "unknown",
            start,
            static_cast<unsigned long>(result),
            order,
            nativeExactViewProjection ? 1 : 0,
            gActiveVertexShaderHash,
            gActivePixelShaderHash,
            matrixMaxAbsDifference(originalWvp, patchedWvp),
            originalViewMatrix.m[3][0],
            originalViewMatrix.m[3][1],
            originalViewMatrix.m[3][2],
            eyeView.m[3][0],
            eyeView.m[3][1],
            eyeView.m[3][2],
            originalWvp.m[0][3],
            originalWvp.m[3][0],
            patchedWvp.m[0][3],
            patchedWvp.m[3][0]);
        logLine(message);
    }
    if (hammerLog)
    {
        char order[32] {};
        readShaderMatrixOrder(order, sizeof(order));
        char event[1152] {};
        sprintf_s(
            event,
            "{\"event\":\"fnvxrD3d9ShaderWvpReplay\",\"frame\":%ld,\"count\":%ld,\"eye\":\"%s\","
            "\"start\":%u,\"result\":\"0x%08lx\",\"order\":\"%s\",\"vsHash\":\"0x%08x\","
            "\"psHash\":\"0x%08x\",\"formula\":\"origWvp_invOrigVp_eyeVp\","
            "\"source\":\"drawLocalTransforms\",\"delta\":%.8f,\"baseT\":[%.5f,%.5f,%.5f],"
            "\"eyeT\":[%.5f,%.5f,%.5f],\"old03\":%.8f,\"old30\":%.8f,"
            "\"new03\":%.8f,\"new30\":%.8f,\"wvpReplay\":1}",
            static_cast<LONG>(gPresentFrames),
            logCount,
            eyeName ? eyeName : "unknown",
            start,
            static_cast<unsigned long>(result),
            order,
            gActiveVertexShaderHash,
            gActivePixelShaderHash,
            matrixMaxAbsDifference(originalWvp, patchedWvp),
            originalViewMatrix.m[3][0],
            originalViewMatrix.m[3][1],
            originalViewMatrix.m[3][2],
            eyeView.m[3][0],
            eyeView.m[3][1],
            eyeView.m[3][2],
            originalWvp.m[0][3],
            originalWvp.m[3][0],
            patchedWvp.m[0][3],
            patchedWvp.m[3][0]);
        logLine(event);
    }

    return SUCCEEDED(result);
}

UINT shaderMatrixAlignment()
{
    const UINT alignment = static_cast<UINT>(readEnvFloat("FNVXR_D3D9_SHADER_MATRIX_ALIGNMENT", 4.0f));
    return alignment == 0 ? 1 : alignment;
}

UINT shaderMatrixPatchStartRegister()
{
    if (const ShaderWvpContract* contract = currentShaderWvpContract())
        return contract->startRegister;
    return static_cast<UINT>(readEnvFloat("FNVXR_D3D9_SHADER_PATCH_START_REGISTER", 0.0f));
}

bool shaderMatrixStartAllowedForStereo(UINT startRegister)
{
    if (!readEnvBool("FNVXR_D3D9_SHADER_LOW_REGISTER_ONLY", true))
        return true;
    return startRegister == shaderMatrixPatchStartRegister();
}

UINT shaderSanityStartRegister()
{
    return static_cast<UINT>(
        readEnvFloat("FNVXR_D3D9_SHADER_SANITY_START_REGISTER",
            static_cast<float>(shaderMatrixPatchStartRegister())));
}

bool shaderSanityOffsetAppliesToStart(UINT startRegister)
{
    return readEnvFloat("FNVXR_D3D9_SHADER_SANITY_OFFSET", 0.0f) != 0.0f
        && startRegister == shaderSanityStartRegister();
}

bool applyShaderSanityOffset(UINT startRegister, float rows[4][4], const char* path, float eyeSign)
{
    if (!rows)
        return false;

    const float offset = readEnvFloat("FNVXR_D3D9_SHADER_SANITY_OFFSET", 0.0f);
    if (offset == 0.0f || startRegister != shaderSanityStartRegister())
        return false;

    char slot[32] {};
    if (GetEnvironmentVariableA("FNVXR_D3D9_SHADER_SANITY_SLOT", slot, static_cast<DWORD>(sizeof(slot))) == 0)
        strcpy_s(slot, sizeof(slot), "c1w");

    struct PatchTarget
    {
        int row;
        int column;
        float before;
        float after;
    };

    PatchTarget targets[4] {};
    int targetCount = 0;
    auto addTarget = [&](int row, int column)
    {
        if (row < 0 || row >= 4 || column < 0 || column >= 4 || targetCount >= 4)
            return;

        targets[targetCount].row = row;
        targets[targetCount].column = column;
        targets[targetCount].before = rows[row][column];
        rows[row][column] += offset;
        targets[targetCount].after = rows[row][column];
        ++targetCount;
    };

    if (_stricmp(slot, "c0w") == 0 || _stricmp(slot, "03") == 0)
        addTarget(0, 3);
    else if (_stricmp(slot, "c1w") == 0 || _stricmp(slot, "13") == 0)
        addTarget(1, 3);
    else if (_stricmp(slot, "c2w") == 0 || _stricmp(slot, "23") == 0)
        addTarget(2, 3);
    else if (_stricmp(slot, "c3x") == 0 || _stricmp(slot, "30") == 0)
        addTarget(3, 0);
    else if (_stricmp(slot, "c3y") == 0 || _stricmp(slot, "31") == 0)
        addTarget(3, 1);
    else if (_stricmp(slot, "c3z") == 0 || _stricmp(slot, "32") == 0)
        addTarget(3, 2);
    else if (_stricmp(slot, "allw") == 0)
    {
        addTarget(0, 3);
        addTarget(1, 3);
        addTarget(2, 3);
    }
    else
        addTarget(1, 3);

    if (targetCount <= 0)
        return false;

    const LONG logCount = InterlockedIncrement(&gLoggedShaderSanityOffset);
    if (logCount <= 24 || logCount % 5000 == 0)
    {
        char message[768] {};
        int written = sprintf_s(
            message,
            "shader sanity offset count=%ld path=%s eyeSign=%.1f start=%u slot=%s offset=%.3f targets=%d",
            logCount,
            path ? path : "unknown",
            eyeSign,
            startRegister,
            slot,
            offset,
            targetCount);
        for (int i = 0; i < targetCount && written > 0 && written < static_cast<int>(sizeof(message)); ++i)
        {
            written += sprintf_s(
                message + written,
                sizeof(message) - static_cast<size_t>(written),
                " c%d.%c %.5f->%.5f",
                targets[i].row,
                "xyzw"[targets[i].column],
                targets[i].before,
                targets[i].after);
        }
        logLine(message);
    }
    return true;
}

void setEyeShaderConstants(IDirect3DDevice9* device, float eyeSign)
{
    if (!device)
        return;
    // The generic constant scanner has no bytecode-level semantic contract.
    // Production proof mode uses setEyeShaderWvpConstants exclusively.
    if (stereoProofModeArmed())
        return;
    if (!readEnvBool("FNVXR_D3D9_SHADER_STEREO", false))
        return;
    if (!currentVertexShaderAllowedForStereoPatch())
        return;

    const bool useMatrixDelta = readEnvBool("FNVXR_D3D9_SHADER_MATRIX_DELTA", true);
    if (!useMatrixDelta)
    {
        const float shaderIpd = readEnvFloat("FNVXR_D3D9_SHADER_IPD", gIpdGameUnits);
        const float shaderDepth = readEnvFloat("FNVXR_D3D9_SHADER_DEPTH", 1.0f);
        const float offset = eyeSign * shaderIpd * 0.5f * shaderDepth;
        const UINT maxCandidates = static_cast<UINT>(readEnvFloat("FNVXR_D3D9_SHADER_MATRIX_MAX_CANDIDATES", 12.0f));
        const UINT alignment = shaderMatrixAlignment();
        UINT patched = 0;
        UINT firstPatchedStart = MaxTrackedVsConstants;
        float firstBefore03 = 0.0f;
        float firstBefore30 = 0.0f;
        float firstAfter03 = 0.0f;
        float firstAfter30 = 0.0f;
        char nudgeSlot[32] {};
        if (GetEnvironmentVariableA("FNVXR_D3D9_SHADER_NUDGE_SLOT", nudgeSlot, static_cast<DWORD>(sizeof(nudgeSlot))) == 0)
            strcpy_s(nudgeSlot, sizeof(nudgeSlot), "03");
        const bool patch03 = _stricmp(nudgeSlot, "03") == 0 || _stricmp(nudgeSlot, "both") == 0;
        const bool patch30 = _stricmp(nudgeSlot, "30") == 0 || _stricmp(nudgeSlot, "both") == 0;

        for (UINT start = 0; start + 3 < MaxTrackedVsConstants && patched < maxCandidates; start += alignment)
        {
            if (!shaderMatrixStartAllowedForStereo(start))
                continue;
            if (!haveTrackedShaderMatrixAt(start))
                continue;

            const fnvxr::stereo::Matrix4 shaderMatrix = trackedShaderMatrixAt(start);
            const char* rejectReason = nullptr;
            if (readEnvBool("FNVXR_D3D9_SHADER_CAMERA_ONLY", true)
                && !shaderSanityOffsetAppliesToStart(start)
                && !shaderMatrixLooksTransform(shaderMatrix, rejectReason))
            {
                continue;
            }

            float rows[4][4] {};
            trackedShaderRowsAt(start, rows);
            const float before03 = rows[0][3];
            const float before30 = rows[3][0];
            if (patch03)
                rows[0][3] += offset;
            if (patch30)
                rows[3][0] += offset;
            if (!patch03 && !patch30)
                rows[0][3] += offset;
            applyShaderSanityOffset(start, rows, "baseline_nudge", eyeSign);
            if (firstPatchedStart == MaxTrackedVsConstants)
            {
                firstPatchedStart = start;
                firstBefore03 = before03;
                firstBefore30 = before30;
                firstAfter03 = rows[0][3];
                firstAfter30 = rows[3][0];
            }
            gRealSetVertexShaderConstantF(device, start, &rows[0][0], 4);
            ++patched;
        }

        if (patched == 0)
        {
            logShaderStereoSkip("baseline_no_matrix_candidates");
            return;
        }

        const LONG logCount = InterlockedIncrement(&gLoggedShaderStereo);
        if (logCount <= 12 || logCount % 5000 == 0)
        {
            char message[512] {};
            sprintf_s(
                message,
                "shader stereo constants count=%ld eyeSign=%.1f mode=baseline_nudge slot=%s patched=%u alignment=%u offset=%.5f firstStart=%u first03=%.5f->%.5f first30=%.5f->%.5f vs=%p ps=%p",
                logCount,
                eyeSign,
                nudgeSlot,
                patched,
                alignment,
                offset,
                firstPatchedStart,
                firstBefore03,
                firstAfter03,
                firstBefore30,
                firstAfter30,
                reinterpret_cast<void*>(gActiveVertexShader),
                reinterpret_cast<void*>(gActivePixelShader));
            logLine(message);
        }
        return;
    }

    if (!gHaveView || !gHaveProjection)
    {
        logShaderStereoSkip("no_base_view_projection");
        return;
    }

    updateSharedCameraState();
    if (readEnvBool("FNVXR_D3D9_SHADER_MATRIX_REQUIRE_SHARED_CAMERA", true)
        && !gSharedCameraActive)
    {
        logShaderStereoSkip("matrix_delta_no_shared_camera");
        return;
    }

    fnvxr::stereo::Matrix4 inverseBaseViewProjection {};
    const fnvxr::stereo::Matrix4 baseViewProjection = multiplyMatrix(baseViewForStereo(), gBaseProjection);
    if (!invertMatrix(baseViewProjection, inverseBaseViewProjection))
    {
        logShaderStereoSkip("base_view_projection_singular");
        return;
    }

    const fnvxr::stereo::Matrix4 eyeView =
        eyeSign > 0.0f ? gEyeMatrices.leftView : gEyeMatrices.rightView;
    const fnvxr::stereo::Matrix4 eyeViewProjection = multiplyMatrix(eyeView, gBaseProjection);
    const float eyeViewDelta = matrixMaxAbsDifference(eyeView, baseViewForStereo());
    const float eyeViewProjectionDelta = matrixMaxAbsDifference(eyeViewProjection, baseViewProjection);
    const UINT maxCandidates = static_cast<UINT>(readEnvFloat("FNVXR_D3D9_SHADER_MATRIX_MAX_CANDIDATES", 12.0f));
    const UINT alignment = shaderMatrixAlignment();
    UINT patched = 0;
    UINT firstPatchedStart = MaxTrackedVsConstants;
    float firstBefore03 = 0.0f;
    float firstBefore30 = 0.0f;
    float firstAfter03 = 0.0f;
    float firstAfter30 = 0.0f;
    float maxShaderPatchDelta = 0.0f;
    const char* lastRejectReason = nullptr;

    for (UINT start = 0; start + 3 < MaxTrackedVsConstants && patched < maxCandidates; start += alignment)
    {
        if (!shaderMatrixStartAllowedForStereo(start))
            continue;
        if (!haveTrackedShaderMatrixAt(start))
            continue;

        const fnvxr::stereo::Matrix4 originalShaderMatrix = trackedShaderMatrixAt(start);
        fnvxr::stereo::Matrix4 shaderMatrix = originalShaderMatrix;
        const char* rejectReason = nullptr;
        if (readEnvBool("FNVXR_D3D9_SHADER_CAMERA_ONLY", true)
            && !shaderSanityOffsetAppliesToStart(start)
            && !shaderMatrixLooksTransform(shaderMatrix, rejectReason))
        {
            lastRejectReason = rejectReason;
            continue;
        }

        shaderMatrix = applyShaderMatrixDelta(shaderMatrix, inverseBaseViewProjection, eyeViewProjection);
        if (!fnvxr::stereo::isFinite(shaderMatrix))
        {
            lastRejectReason = "matrix_delta_nonfinite";
            continue;
        }

        const float patchDelta = matrixMaxAbsDifference(originalShaderMatrix, shaderMatrix);
        if (patchDelta > maxShaderPatchDelta)
            maxShaderPatchDelta = patchDelta;
        if (firstPatchedStart == MaxTrackedVsConstants)
        {
            firstPatchedStart = start;
            firstBefore03 = originalShaderMatrix.m[0][3];
            firstBefore30 = originalShaderMatrix.m[3][0];
            firstAfter03 = shaderMatrix.m[0][3];
            firstAfter30 = shaderMatrix.m[3][0];
        }

        float rows[4][4] {};
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
                rows[row][column] = shaderMatrix.m[row][column];
        }
        applyShaderSanityOffset(start, rows, "matrix_delta", eyeSign);
        gRealSetVertexShaderConstantF(device, start, &rows[0][0], 4);
        ++patched;
    }

    if (patched == 0)
    {
        logShaderStereoSkip(lastRejectReason ? lastRejectReason : "no_matrix_candidates");
        return;
    }

    const LONG logCount = InterlockedIncrement(&gLoggedShaderStereo);
    if (logCount <= 12 || logCount % 5000 == 0)
    {
        char order[32] {};
        readShaderMatrixOrder(order, sizeof(order));
        char message[640] {};
        sprintf_s(
            message,
            "shader stereo constants count=%ld eyeSign=%.1f mode=matrix_delta order=%s patched=%u alignment=%u eyeViewDelta=%.6f eyeVpDelta=%.6f maxPatchDelta=%.6f firstStart=%u first03=%.5f->%.5f first30=%.5f->%.5f c0w=%.5f hmdDelta=(%.4f %.4f %.4f) hmdRotDelta=(%.4f %.4f %.4f %.4f)",
            logCount,
            eyeSign,
            order,
            patched,
            alignment,
            eyeViewDelta,
            eyeViewProjectionDelta,
            maxShaderPatchDelta,
            firstPatchedStart,
            firstBefore03,
            firstAfter03,
            firstBefore30,
            firstAfter30,
            gVsConstants[0][3],
            gLatestHmdDelta[0],
            gLatestHmdDelta[1],
            gLatestHmdDelta[2],
            gLatestHmdRotDelta[0],
            gLatestHmdRotDelta[1],
            gLatestHmdRotDelta[2],
            gLatestHmdRotDelta[3]);
        logLine(message);
    }
}

float currentProjectionHorizontalFovDegrees();
void logWideWorldReplaySkip(const char* reason, float fovX);
bool haveWorldCameraCandidate();

bool setWideWorldShaderConstants(
    IDirect3DDevice9* device,
    const fnvxr::stereo::Matrix4& wideWorldProjection)
{
    if (!device)
        return false;
    if (stereoProofModeArmed())
        return false;
    if (!readEnvBool("FNVXR_D3D9_WIDE_WORLD_SHADER_MATRIX_DELTA", true))
        return false;
    if (!gHaveView || !gHaveProjection)
        return false;

    updateSharedCameraState();
    fnvxr::stereo::Matrix4 inverseBaseViewProjection {};
    const fnvxr::stereo::Matrix4 baseViewProjection = multiplyMatrix(baseViewForStereo(), gBaseProjection);
    if (!invertMatrix(baseViewProjection, inverseBaseViewProjection))
        return false;

    const fnvxr::stereo::Matrix4 wideViewProjection = multiplyMatrix(baseViewForStereo(), wideWorldProjection);
    const UINT maxCandidates = static_cast<UINT>(
        readEnvFloat("FNVXR_D3D9_WIDE_WORLD_SHADER_MATRIX_MAX_CANDIDATES",
            readEnvFloat("FNVXR_D3D9_SHADER_MATRIX_MAX_CANDIDATES", 12.0f)));
    const UINT alignment = shaderMatrixAlignment();
    UINT patched = 0;
    UINT firstPatchedStart = MaxTrackedVsConstants;
    float maxShaderPatchDelta = 0.0f;
    const char* lastRejectReason = nullptr;

    for (UINT start = 0; start + 3 < MaxTrackedVsConstants && patched < maxCandidates; start += alignment)
    {
        if (!haveTrackedShaderMatrixAt(start))
            continue;

        const fnvxr::stereo::Matrix4 originalShaderMatrix = trackedShaderMatrixAt(start);
        const char* rejectReason = nullptr;
        if (readEnvBool("FNVXR_D3D9_WIDE_WORLD_SHADER_CAMERA_ONLY",
                readEnvBool("FNVXR_D3D9_SHADER_CAMERA_ONLY", true))
            && !shaderMatrixLooksTransform(originalShaderMatrix, rejectReason))
        {
            lastRejectReason = rejectReason;
            continue;
        }

        const fnvxr::stereo::Matrix4 shaderMatrix =
            applyShaderMatrixDelta(originalShaderMatrix, inverseBaseViewProjection, wideViewProjection);
        if (!fnvxr::stereo::isFinite(shaderMatrix))
        {
            lastRejectReason = "wide_matrix_delta_nonfinite";
            continue;
        }

        const float patchDelta = matrixMaxAbsDifference(originalShaderMatrix, shaderMatrix);
        if (patchDelta > maxShaderPatchDelta)
            maxShaderPatchDelta = patchDelta;
        if (firstPatchedStart == MaxTrackedVsConstants)
            firstPatchedStart = start;

        float rows[4][4] {};
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
                rows[row][column] = shaderMatrix.m[row][column];
        }
        gRealSetVertexShaderConstantF(device, start, &rows[0][0], 4);
        ++patched;
    }

    if (patched == 0)
    {
        logWideWorldReplaySkip(lastRejectReason ? lastRejectReason : "wide-no-shader-matrix-candidates", currentProjectionHorizontalFovDegrees());
        return false;
    }

    const LONG logCount = InterlockedIncrement(&gLoggedWideWorldShader);
    if (logCount <= 12 || logCount % 5000 == 0)
    {
        char order[32] {};
        readShaderMatrixOrder(order, sizeof(order));
        char message[384] {};
        sprintf_s(
            message,
            "wide world shader constants count=%ld order=%s patched=%u alignment=%u firstStart=%u maxPatchDelta=%.6f fovX=%.3f cropScale=(%.5f %.5f)",
            logCount,
            order,
            patched,
            alignment,
            firstPatchedStart,
            maxShaderPatchDelta,
            currentProjectionHorizontalFovDegrees(),
            wideWorldProjection.m[0][0] / (gBaseProjection.m[0][0] == 0.0f ? 1.0f : gBaseProjection.m[0][0]),
            wideWorldProjection.m[1][1] / (gBaseProjection.m[1][1] == 0.0f ? 1.0f : gBaseProjection.m[1][1]));
        logLine(message);
    }

    return true;
}

void restoreShaderConstants(IDirect3DDevice9* device)
{
    if (!device || !gRealSetVertexShaderConstantF)
        return;

    // A verified contract patches one exact four-register range. Restore that
    // exact range; scanning from c0 can restore a different matrix and leave
    // the contracted WVP contaminated by the previous eye.
    if (const ShaderWvpContract* contract = currentShaderWvpContract())
    {
        const UINT start = contract->startRegister;
        if (haveTrackedShaderMatrixAt(start))
            gRealSetVertexShaderConstantF(device, start, &gVsConstants[start][0], 4);
        return;
    }

    if (!readEnvBool("FNVXR_D3D9_SHADER_ALLOW_UNVERIFIED_PATCHES", false))
        return;

    const UINT maxCandidates = static_cast<UINT>(readEnvFloat("FNVXR_D3D9_SHADER_MATRIX_MAX_CANDIDATES", 12.0f));
    const UINT alignment = shaderMatrixAlignment();
    UINT restored = 0;
    for (UINT start = 0; start + 3 < MaxTrackedVsConstants && restored < maxCandidates; start += alignment)
    {
        if (!haveTrackedShaderMatrixAt(start))
            continue;

        gRealSetVertexShaderConstantF(device, start, &gVsConstants[start][0], 4);
        ++restored;
    }
}

float absFloat(float value)
{
    return value < 0.0f ? -value : value;
}

float baseViewTranslationMagnitude()
{
    const fnvxr::stereo::Matrix4& stereoBaseView = baseViewForStereo();
    return absFloat(stereoBaseView.m[3][0]) + absFloat(stereoBaseView.m[3][1]) + absFloat(stereoBaseView.m[3][2]);
}

float matrixTranslationMagnitude(const fnvxr::stereo::Matrix4& matrix)
{
    return absFloat(matrix.m[3][0]) + absFloat(matrix.m[3][1]) + absFloat(matrix.m[3][2]);
}

bool currentProjectionLooksScreenSpace()
{
    const float maxScale = readEnvFloat("FNVXR_D3D9_SCREEN_PROJECTION_MAX_SCALE", 0.02f);
    const float maxOffsetDelta = readEnvFloat("FNVXR_D3D9_SCREEN_PROJECTION_MAX_OFFSET_DELTA", 0.08f);
    const bool tinyScale =
        absFloat(gBaseProjection.m[0][0]) > 0.0f
        && absFloat(gBaseProjection.m[0][0]) <= maxScale
        && absFloat(gBaseProjection.m[1][1]) > 0.0f
        && absFloat(gBaseProjection.m[1][1]) <= maxScale;
    const bool xViewportOffset =
        absFloat(gBaseProjection.m[3][0] + 1.0f) <= maxOffsetDelta
        || absFloat(gBaseProjection.m[3][0] - 1.0f) <= maxOffsetDelta;
    const bool yViewportOffset =
        absFloat(gBaseProjection.m[3][1] + 1.0f) <= maxOffsetDelta
        || absFloat(gBaseProjection.m[3][1] - 1.0f) <= maxOffsetDelta;
    const bool viewportOffset = xViewportOffset && yViewportOffset;
    return tinyScale && viewportOffset;
}

bool currentViewLooksScreenSpace()
{
    const float maxTranslation = readEnvFloat("FNVXR_D3D9_SCREEN_VIEW_MAX_TRANSLATION", 10.0f);
    return matrixTranslationMagnitude(gBaseView) <= maxTranslation;
}

void logStereoReplaySkip(const char* reason)
{
    const LONG count = InterlockedIncrement(&gLoggedStereoReplaySkipped);
    if (count <= 12 || count % 5000 == 0)
    {
        char message[384] {};
        sprintf_s(
            message,
            "stereo replay skipped count=%ld reason=%s viewT=(%.4f %.4f %.4f) proj=[%.5f %.5f %.5f %.5f] vs=%p ps=%p vsHash=0x%08x psHash=0x%08x",
            count,
            reason ? reason : "unknown",
            gBaseView.m[3][0],
            gBaseView.m[3][1],
            gBaseView.m[3][2],
            gBaseProjection.m[0][0],
            gBaseProjection.m[1][1],
            gBaseProjection.m[3][0],
            gBaseProjection.m[3][1],
            reinterpret_cast<void*>(gActiveVertexShader),
            reinterpret_cast<void*>(gActivePixelShader),
            gActiveVertexShaderHash,
            gActivePixelShaderHash);
        logLine(message);
    }
}

bool shouldReplayCurrentDrawToStereo(bool userPrimitiveDraw)
{
    const bool textureCompositeDraw = currentDrawSamplesStereoTwin();
    const bool verifiedProgrammableWorldDraw = currentShaderWvpContract() != nullptr;

    if (userPrimitiveDraw && !readEnvBool("FNVXR_D3D9_STEREO_REPLAY_UP_DRAWS", false))
    {
        logStereoReplaySkip("user-primitive-draw");
        return false;
    }

    if (currentShaderPairIsConfiguredStereoSkip() && !textureCompositeDraw)
    {
        logStereoReplaySkip("configured-collapse-shader-pair");
        return false;
    }

    if (currentShaderPairIsLearnedStereoCollapse() && !textureCompositeDraw)
    {
        logStereoReplaySkip("collapse-shader-pair");
        return false;
    }

    if (readEnvBool("FNVXR_D3D9_SKIP_SCREENSPACE_BY_PROJECTION_ONLY", true)
        && currentProjectionLooksScreenSpace()
        && !verifiedProgrammableWorldDraw
        && !textureCompositeDraw)
    {
        logStereoReplaySkip("screen-space-projection");
        return false;
    }

    if (readEnvBool("FNVXR_D3D9_SKIP_SCREENSPACE_BY_VIEW_ONLY", false)
        && currentViewLooksScreenSpace()
        && !verifiedProgrammableWorldDraw
        && !textureCompositeDraw)
    {
        logStereoReplaySkip("screen-space-view");
        return false;
    }

    if (readEnvBool("FNVXR_D3D9_SKIP_SCREENSPACE_STEREO_DRAWS", true)
        && currentProjectionLooksScreenSpace()
        && currentViewLooksScreenSpace()
        && !verifiedProgrammableWorldDraw
        && !textureCompositeDraw)
    {
        logStereoReplaySkip("screen-space-transform");
        return false;
    }

    return true;
}

float currentProjectionHorizontalFovDegrees()
{
    if (!gHaveProjection)
        return 0.0f;

    const float xScale = absFloat(gBaseProjection.m[0][0]);
    if (!std::isfinite(xScale) || xScale <= 0.000001f)
        return 0.0f;

    return 2.0f * std::atan(1.0f / xScale) * 180.0f / 3.14159265358979323846f;
}

void logWideWorldReplaySkip(const char* reason, float fovX)
{
    const LONG count = InterlockedIncrement(&gLoggedWideWorldReplaySkipped);
    if (count <= 12 || count % 5000 == 0)
    {
        char message[256] {};
        sprintf_s(
            message,
            "wide world replay skipped count=%ld reason=%s fovX=%.3f proj=(%.5f %.5f) vsHash=0x%08x psHash=0x%08x",
            count,
            reason ? reason : "unknown",
            fovX,
            gBaseProjection.m[0][0],
            gBaseProjection.m[1][1],
            gActiveVertexShaderHash,
            gActivePixelShaderHash);
        logLine(message);
    }
}

bool shouldReplayCurrentDrawToWideWorld()
{
    if (readEnvBool("FNVXR_D3D9_WIDE_WORLD_REQUIRE_WORLD_CAMERA", true) && !haveWorldCameraCandidate())
    {
        logWideWorldReplaySkip("no-world-camera", currentProjectionHorizontalFovDegrees());
        return false;
    }

    if (readEnvBool("FNVXR_D3D9_WIDE_WORLD_SKIP_SCREENSPACE", true)
        && (currentProjectionLooksScreenSpace() || currentViewLooksScreenSpace()))
    {
        logWideWorldReplaySkip("screen-space", currentProjectionHorizontalFovDegrees());
        return false;
    }

    if (readEnvBool("FNVXR_D3D9_WIDE_WORLD_REJECT_NARROW_FOV", true))
    {
        const float fovX = currentProjectionHorizontalFovDegrees();
        const float centerFovX = readEnvFloat(
            "FNVXR_GAME_PLANE_CENTER_FOV_X_DEG",
            readEnvFloat("FNVXR_GAME_PLANE_CENTER_FOV_DEG", 95.0f));
        const float defaultMinWorldFovX = centerFovX
            - readEnvFloat("FNVXR_D3D9_WIDE_WORLD_NARROW_FOV_MARGIN_DEG", 12.0f);
        const float minWorldFovX = readEnvFloat("FNVXR_D3D9_WIDE_WORLD_MIN_WORLD_FOV_X_DEG", defaultMinWorldFovX);
        if (fovX > 0.0f && fovX < minWorldFovX)
        {
            logWideWorldReplaySkip("narrow-fov-viewmodel", fovX);
            return false;
        }
    }

    return true;
}

float fovDegreesToRadians(float degrees)
{
    return degrees * 3.14159265358979323846f / 180.0f;
}

float clampFovDegrees(float degrees)
{
    if (degrees < 1.0f)
        return 1.0f;
    if (degrees > 178.0f)
        return 178.0f;
    return degrees;
}

float horizontalToVerticalFovDegrees(float horizontalDegrees, float aspect)
{
    const float safeAspect = aspect > 0.1f ? aspect : 1.6f;
    const float halfHorizontal = fovDegreesToRadians(clampFovDegrees(horizontalDegrees)) * 0.5f;
    return 2.0f * std::atan(std::tan(halfHorizontal) / safeAspect) * 180.0f / 3.14159265358979323846f;
}

float fovCenterCropRatio(float centerDegrees, float wideDegrees)
{
    const float safeCenter = clampFovDegrees(centerDegrees);
    float safeWide = wideDegrees < safeCenter + 0.1f ? safeCenter + 0.1f : wideDegrees;
    if (safeWide > 179.0f)
        safeWide = 179.0f;
    const float center = fovDegreesToRadians(safeCenter) * 0.5f;
    const float wide = fovDegreesToRadians(safeWide) * 0.5f;
    const float ratio = std::tan(center) / std::tan(wide);
    if (ratio < 0.05f)
        return 0.05f;
    if (ratio > 1.0f)
        return 1.0f;
    return ratio;
}

fnvxr::stereo::Matrix4 makeWideWorldProjection()
{
    fnvxr::stereo::Matrix4 projection = gBaseProjection;
    if (readEnvBool("FNVXR_D3D9_WIDE_WORLD_MATCH_CENTER_FOV", false))
        return projection;

    const float aspect = static_cast<float>(gStereoTargetWidth == 0 ? SharedVideoMaxWidth : gStereoTargetWidth)
        / static_cast<float>(gStereoTargetHeight == 0 ? SharedVideoMaxHeight : gStereoTargetHeight);
    const float centerFovX =
        readEnvFloat("FNVXR_GAME_PLANE_CENTER_FOV_X_DEG", readEnvFloat("FNVXR_GAME_PLANE_CENTER_FOV_DEG", 95.0f));
    const float wideFovX =
        readEnvFloat("FNVXR_GAME_PLANE_SURROUND_WIDE_FOV_X_DEG", readEnvFloat("FNVXR_GAME_PLANE_SURROUND_WIDE_FOV_DEG", 130.0f));
    const float centerFovY =
        readEnvFloat("FNVXR_GAME_PLANE_CENTER_FOV_Y_DEG", horizontalToVerticalFovDegrees(centerFovX, aspect));
    const float wideFovY =
        readEnvFloat("FNVXR_GAME_PLANE_SURROUND_WIDE_FOV_Y_DEG", horizontalToVerticalFovDegrees(wideFovX, aspect));
    projection.m[0][0] *= fovCenterCropRatio(centerFovX, wideFovX);
    projection.m[1][1] *= fovCenterCropRatio(centerFovY, wideFovY);
    return projection;
}

bool haveWorldCameraCandidate()
{
    const bool captureWorldDuringUi = readEnvBool("FNVXR_D3D9_CAPTURE_WORLD_DURING_UI", false);
    const bool uiMode = fnvUiModeActive();
    if (uiMode && !captureWorldDuringUi)
        return false;

    updateSharedCameraState();
    if (readEnvBool("FNVXR_D3D9_USE_SHARED_CAMERA_VIEW", true) && gSharedCameraActive)
        return true;
    const bool requireSharedCameraForWorld =
        readEnvBool("FNVXR_D3D9_REQUIRE_SHARED_CAMERA_FOR_WORLD",
            readEnvBool("FNVXR_D3D9_SHARED_STEREO", false));
    if (requireSharedCameraForWorld)
        return false;
    if (baseViewTranslationMagnitude() > 0.001f)
        return true;
    return gHaveView && gHaveProjection
        && (!uiMode || captureWorldDuringUi);
}

bool matrixAuditEnabled()
{
    return readEnvBool("FNVXR_D3D9_MATRIX_AUDIT", stereoWorldRuntimeEnabled());
}

bool shouldLogMatrixAuditForFrame(LONG frame)
{
    if (!matrixAuditEnabled())
        return false;
    if (frame <= 0)
        return false;
    if (frame == gLastMatrixAuditFrame)
        return false;

    const LONG logCount = InterlockedCompareExchange(&gLoggedMatrixAudit, 0, 0);
    const LONG stride = static_cast<LONG>(readEnvFloat("FNVXR_D3D9_MATRIX_AUDIT_STRIDE", 30.0f));
    const LONG safeStride = stride <= 0 ? 1 : stride;
    return logCount < 12 || (frame % safeStride) == 0;
}

void appendMatrixJson(char* buffer, size_t bufferSize, int& offset, const char* name, const fnvxr::stereo::Matrix4& matrix)
{
    if (!buffer || offset < 0 || static_cast<size_t>(offset) >= bufferSize)
        return;

    offset += sprintf_s(
        buffer + offset,
        bufferSize - static_cast<size_t>(offset),
        ",\"%s\":[[%.6g,%.6g,%.6g,%.6g],[%.6g,%.6g,%.6g,%.6g],[%.6g,%.6g,%.6g,%.6g],[%.6g,%.6g,%.6g,%.6g]]",
        name,
        matrix.m[0][0],
        matrix.m[0][1],
        matrix.m[0][2],
        matrix.m[0][3],
        matrix.m[1][0],
        matrix.m[1][1],
        matrix.m[1][2],
        matrix.m[1][3],
        matrix.m[2][0],
        matrix.m[2][1],
        matrix.m[2][2],
        matrix.m[2][3],
        matrix.m[3][0],
        matrix.m[3][1],
        matrix.m[3][2],
        matrix.m[3][3]);
}

fnvxr::stereo::Matrix4 makeLocalVrEyeViewInverseOffset(
    const fnvxr::stereo::Matrix4& baseView,
    const float eyeLocalMeters[3],
    bool applyRotation)
{
    fnvxr::stereo::Matrix4 result = applyRotation
        ? multiplyViewRotationOnly(baseView, hmdViewRotationMatrix())
        : baseView;

    float localXrMeters[3] {};
    float localGameMeters[3] {};
    composeLocalXrMeters(eyeLocalMeters, localXrMeters);
    xrMetersToGamebryoMeters(localXrMeters, localGameMeters);
    result.m[3][0] -= localGameMeters[0] * gGameUnitsPerMeter;
    result.m[3][1] -= localGameMeters[1] * gGameUnitsPerMeter;
    result.m[3][2] -= localGameMeters[2] * gGameUnitsPerMeter;
    return result;
}

UINT countTrackedShaderMatrixCandidates(UINT& firstStart, fnvxr::stereo::Matrix4& firstMatrix)
{
    firstStart = MaxTrackedVsConstants;
    const UINT alignment = shaderMatrixAlignment();
    UINT count = 0;
    for (UINT start = 0; start + 3 < MaxTrackedVsConstants; start += alignment)
    {
        if (!haveTrackedShaderMatrixAt(start))
            continue;

        const fnvxr::stereo::Matrix4 matrix = trackedShaderMatrixAt(start);
        const char* reason = nullptr;
        if (!shaderMatrixLooksTransform(matrix, reason))
            continue;

        if (firstStart == MaxTrackedVsConstants)
        {
            firstStart = start;
            firstMatrix = matrix;
        }
        ++count;
    }
    return count;
}

void logShaderMatrixAuditCandidates(LONG frame)
{
    const UINT maxLogs = static_cast<UINT>(readEnvFloat("FNVXR_D3D9_MATRIX_AUDIT_SHADER_CANDIDATES", 3.0f));
    if (maxLogs == 0)
        return;

    const UINT alignment = shaderMatrixAlignment();
    UINT logged = 0;
    for (UINT start = 0; start + 3 < MaxTrackedVsConstants && logged < maxLogs; start += alignment)
    {
        if (!haveTrackedShaderMatrixAt(start))
            continue;

        const fnvxr::stereo::Matrix4 matrix = trackedShaderMatrixAt(start);
        const char* reason = nullptr;
        if (!shaderMatrixLooksTransform(matrix, reason))
            continue;

        char message[1536] {};
        int offset = sprintf_s(
            message,
            "{\"event\":\"fnvxrShaderMatrixAudit\",\"frame\":%ld,\"start\":%u,\"maxAbs\":%.6g,\"baseVpDelta\":",
            frame,
            start,
            matrixMaxAbs(matrix));
        if (gHaveView && gHaveProjection)
        {
            const fnvxr::stereo::Matrix4 baseViewProjection = multiplyMatrix(baseViewForStereo(), gBaseProjection);
            offset += sprintf_s(
                message + offset,
                sizeof(message) - static_cast<size_t>(offset),
                "%.6g",
                matrixMaxAbsDifference(matrix, baseViewProjection));
        }
        else
        {
            offset += sprintf_s(message + offset, sizeof(message) - static_cast<size_t>(offset), "null");
        }
        appendMatrixJson(message, sizeof(message), offset, "matrix", matrix);
        if (offset > 0 && static_cast<size_t>(offset) < sizeof(message) - 2)
            sprintf_s(message + offset, sizeof(message) - static_cast<size_t>(offset), "}");
        logLine(message);
        ++logged;
    }
}

void logWorldShaderWvpDiscovery()
{
    if (!readEnvBool("FNVXR_D3D9_DUMP_SHADER_BYTECODE", false)
        || !gActiveVertexShader
        || gActiveVertexShaderHash == 0
        || gActiveVertexShaderByteCount == 0)
    {
        return;
    }

    for (LONG index = 0; index < gShaderWvpDiscoveryIdentityCount; ++index)
    {
        const ShaderWvpDiscoveryIdentity& known = gShaderWvpDiscoveryIdentities[index];
        if (known.vertexHash == gActiveVertexShaderHash
            && known.byteCount == gActiveVertexShaderByteCount
            && std::memcmp(known.vertexSha256, gActiveVertexShaderSha256, 32) == 0)
        {
            return;
        }
    }

    if (gShaderWvpDiscoveryIdentityCount >= static_cast<LONG>(MaxShaderWvpDiscoveryIdentities))
    {
        logLine("shader WVP discovery capacity exhausted; diagnostic run is incomplete");
        return;
    }
    ShaderWvpDiscoveryIdentity& identity =
        gShaderWvpDiscoveryIdentities[gShaderWvpDiscoveryIdentityCount++];
    identity.vertexHash = gActiveVertexShaderHash;
    identity.byteCount = gActiveVertexShaderByteCount;
    std::memcpy(identity.vertexSha256, gActiveVertexShaderSha256, 32);

    char shaHex[65] {};
    for (UINT index = 0; index < 32; ++index)
        sprintf_s(shaHex + index * 2, sizeof(shaHex) - index * 2, "%02x", identity.vertexSha256[index]);

    char event[4096] {};
    int offset = sprintf_s(
        event,
        "{\"event\":\"fnvxrShaderWvpDiscovery\",\"fnv1a32\":\"%08x\",\"sha256\":\"%s\",\"bytes\":%u,\"candidateMatrices\":[",
        identity.vertexHash,
        shaHex,
        identity.byteCount);
    const UINT alignment = shaderMatrixAlignment();
    UINT candidateCount = 0;
    for (UINT start = 0; start + 3 < MaxTrackedVsConstants && candidateCount < 12; start += alignment)
    {
        if (!haveTrackedShaderMatrixAt(start))
            continue;
        const fnvxr::stereo::Matrix4 matrix = trackedShaderMatrixAt(start);
        const char* rejectReason = nullptr;
        if (!shaderMatrixLooksTransform(matrix, rejectReason))
            continue;
        offset += sprintf_s(
            event + offset,
            sizeof(event) - static_cast<size_t>(offset),
            "%s{\"start\":%u,\"maxAbs\":%.8g}",
            candidateCount == 0 ? "" : ",",
            start,
            matrixMaxAbs(matrix));
        ++candidateCount;
    }
    if (offset > 0 && static_cast<size_t>(offset) < sizeof(event) - 64)
    {
        sprintf_s(
            event + offset,
            sizeof(event) - static_cast<size_t>(offset),
            "],\"candidateCount\":%u,\"diagnosticOnly\":true}",
            candidateCount);
    }
    logLine(event);
}

void logMatrixAuditFrame()
{
    const LONG frame = static_cast<LONG>(gPresentFrames);
    if (!shouldLogMatrixAuditForFrame(frame))
        return;

    gLastMatrixAuditFrame = frame;
    InterlockedIncrement(&gLoggedMatrixAudit);

    updateSharedVrPose();
    updateSharedCameraState();
    if (gHaveView && gHaveProjection)
        updateEyeMatrices();

    bool runtimeUi = true;
    bool runtimeGameplay = false;
    std::uint32_t runtimePhase = 0;
    std::uint32_t runtimeMenuBits = 0;
    std::uint32_t runtimeCamera = 0;
    std::uint32_t runtimeShowroom = 0;
    if (gSharedRuntime && gSharedRuntime->magic == SharedRuntimeMagic && gSharedRuntime->version == SharedRuntimeVersion)
    {
        runtimePhase = gSharedRuntime->phase;
        runtimeMenuBits = gSharedRuntime->menuBits;
        runtimeCamera = gSharedRuntime->cameraActive;
        runtimeShowroom = gSharedRuntime->showroomActive;
        runtimeGameplay = fnvxr::shared::runtimeGameplayPhase(runtimePhase, runtimeMenuBits, runtimeShowroom);
        runtimeUi = fnvxr::shared::runtimeUiActive(runtimePhase, runtimeMenuBits, runtimeShowroom);
    }

    const fnvxr::stereo::Matrix4& stereoBaseView = baseViewForStereo();
    const bool requestedHmdPose = readEnvBool("FNVXR_D3D9_APPLY_HMD_POSE", false);
    const bool applyRotationCandidate = requestedHmdPose && gHaveVrBodyFrame;
    const fnvxr::stereo::Matrix4 inverseLeft =
        makeLocalVrEyeViewInverseOffset(stereoBaseView, gLatestLeftEyeLocalMeters, applyRotationCandidate);
    const fnvxr::stereo::Matrix4 inverseRight =
        makeLocalVrEyeViewInverseOffset(stereoBaseView, gLatestRightEyeLocalMeters, applyRotationCandidate);

    UINT firstShaderStart = MaxTrackedVsConstants;
    fnvxr::stereo::Matrix4 firstShaderMatrix {};
    const UINT shaderCandidates = countTrackedShaderMatrixCandidates(firstShaderStart, firstShaderMatrix);

    char message[4096] {};
    int offset = sprintf_s(
        message,
        "{\"event\":\"fnvxrMatrixAudit\",\"frame\":%ld,\"runtime\":{\"phase\":%lu,\"menuBits\":%lu,\"ui\":%s,\"gameplay\":%s,\"camera\":%lu,\"showroom\":%lu},\"sharedCamera\":{\"seq\":%ld,\"snapshot\":%s,\"rawActive\":%s,\"matrixUsable\":%s,\"accepted\":%s,\"thirdPerson\":%lu,\"pos\":[%.6g,%.6g,%.6g]},\"pose\":{\"seq\":%ld,\"valid\":%s,\"bodyFrame\":%s,\"rawIpdMeters\":%.6g,\"hmdDelta\":[%.6g,%.6g,%.6g],\"hmdRotDelta\":[%.6g,%.6g,%.6g,%.6g]},\"d3d\":{\"haveView\":%s,\"haveProjection\":%s,\"screenProjection\":%s,\"screenView\":%s,\"worldCandidate\":%s,\"shaderCandidates\":%u,\"firstShaderStart\":",
        frame,
        static_cast<unsigned long>(runtimePhase),
        static_cast<unsigned long>(runtimeMenuBits),
        runtimeUi ? "true" : "false",
        runtimeGameplay ? "true" : "false",
        static_cast<unsigned long>(runtimeCamera),
        static_cast<unsigned long>(runtimeShowroom),
        gLatestSharedCameraSnapshotSequence,
        gLatestSharedCameraSnapshotValid ? "true" : "false",
        gSharedCameraRawActive ? "true" : "false",
        gSharedCameraMatrixUsable ? "true" : "false",
        gSharedCameraActive ? "true" : "false",
        gLatestSharedCameraSnapshotValid ? static_cast<unsigned long>(gLatestSharedCameraSnapshot.thirdPerson) : 0ul,
        gLatestSharedCameraSnapshotValid ? gLatestSharedCameraSnapshot.worldPos[0] : 0.0f,
        gLatestSharedCameraSnapshotValid ? gLatestSharedCameraSnapshot.worldPos[1] : 0.0f,
        gLatestSharedCameraSnapshotValid ? gLatestSharedCameraSnapshot.worldPos[2] : 0.0f,
        gLatestVrPoseSnapshotSequence,
        gLatestVrPoseSnapshotValid ? "true" : "false",
        gHaveVrBodyFrame ? "true" : "false",
        gLatestRawIpdMeters,
        gLatestHmdDelta[0],
        gLatestHmdDelta[1],
        gLatestHmdDelta[2],
        gLatestHmdRotDelta[0],
        gLatestHmdRotDelta[1],
        gLatestHmdRotDelta[2],
        gLatestHmdRotDelta[3],
        gHaveView ? "true" : "false",
        gHaveProjection ? "true" : "false",
        currentProjectionLooksScreenSpace() ? "true" : "false",
        currentViewLooksScreenSpace() ? "true" : "false",
        haveWorldCameraCandidate() ? "true" : "false",
        shaderCandidates);

    if (firstShaderStart == MaxTrackedVsConstants)
        offset += sprintf_s(message + offset, sizeof(message) - static_cast<size_t>(offset), "null}");
    else
        offset += sprintf_s(message + offset, sizeof(message) - static_cast<size_t>(offset), "%u}", firstShaderStart);

    appendMatrixJson(message, sizeof(message), offset, "baseView", gBaseView);
    appendMatrixJson(message, sizeof(message), offset, "sharedOrBaseView", stereoBaseView);
    appendMatrixJson(message, sizeof(message), offset, "baseProjection", gBaseProjection);
    appendMatrixJson(message, sizeof(message), offset, "leftViewCurrent", gEyeMatrices.leftView);
    appendMatrixJson(message, sizeof(message), offset, "rightViewCurrent", gEyeMatrices.rightView);
    appendMatrixJson(message, sizeof(message), offset, "leftViewInverseOffsetCandidate", inverseLeft);
    appendMatrixJson(message, sizeof(message), offset, "rightViewInverseOffsetCandidate", inverseRight);
    if (firstShaderStart != MaxTrackedVsConstants)
        appendMatrixJson(message, sizeof(message), offset, "firstShaderMatrix", firstShaderMatrix);
    if (offset > 0 && static_cast<size_t>(offset) < sizeof(message) - 2)
        sprintf_s(message + offset, sizeof(message) - static_cast<size_t>(offset), "}");
    logLine(message);
    logShaderMatrixAuditCandidates(frame);
}

void releaseSurface(IDirect3DSurface9*& surface)
{
    if (surface)
    {
        surface->Release();
        surface = nullptr;
    }
}

void releaseSharedVideoReadback()
{
    releaseSurface(gSharedVideoReadback);
    gSharedVideoWidth = 0;
    gSharedVideoHeight = 0;
    gSharedVideoFormat = D3DFMT_UNKNOWN;
}

void releaseSharedVideo()
{
    releaseSharedVideoReadback();
    if (gSharedVideoView)
    {
        UnmapViewOfFile(gSharedVideoView);
        gSharedVideoView = nullptr;
    }
    if (gSharedVideoMapping)
    {
        CloseHandle(gSharedVideoMapping);
        gSharedVideoMapping = nullptr;
    }
    releaseNamedProducerMutex(gSharedVideoProducerLease);
}

void writeInvalidSharedStereoRecord(SharedStereoHeader* header, bool uiActive)
{
    if (!header)
        return;
    InterlockedExchange(&header->writing, 1);
    MemoryBarrier();
    header->magic = SharedStereoMagic;
    header->version = fnvxr::shared::D3D9StereoFrameSharedVersion;
    header->headerBytes = sizeof(SharedStereoHeader);
    header->width = 0;
    header->height = 0;
    header->pitchBytes = 0;
    header->format = 0;
    header->separated = 0;
    header->worldCandidate = 0;
    header->uiActive = uiActive ? 1 : 0;
    header->poseValid = 0;
    header->poseSequence = 0;
    header->renderedDisplayTime = 0;
    std::memset(header->leftEyeRot, 0, sizeof(header->leftEyeRot));
    std::memset(header->leftEyePos, 0, sizeof(header->leftEyePos));
    std::memset(header->rightEyeRot, 0, sizeof(header->rightEyeRot));
    std::memset(header->rightEyePos, 0, sizeof(header->rightEyePos));
    std::memset(header->leftFov, 0, sizeof(header->leftFov));
    std::memset(header->rightFov, 0, sizeof(header->rightFov));
    header->producerMode = fnvxr::shared::StereoProducerUnknown;
    header->renderPairSequence = 0;
    header->leftPayloadOffset = sizeof(SharedStereoHeader);
    header->rightPayloadOffset = sizeof(SharedStereoHeader);
    header->totalMappingBytes = sizeof(SharedStereoHeader)
        + SharedVideoMaxWidth * SharedVideoMaxHeight * 4 * 2
            * fnvxr::shared::D3D9StereoFrameSlotCount;
    header->referenceSpaceGeneration = 0;
    header->producerEpoch = 0;
    header->rendererProducerEpoch = gSharedStereoRendererProducerEpoch;
    header->producerProcessId = GetCurrentProcessId();
    MemoryBarrier();
    if (InterlockedIncrement64(&header->publicationGeneration) == 0)
        InterlockedIncrement64(&header->publicationGeneration);
    MemoryBarrier();
    fnvxr::shared::incrementNonzeroSharedCounter(header->sequence);
    MemoryBarrier();
    InterlockedExchange(&header->writing, 0);
}

void releaseNamedProducerMutex(NamedProducerLease& lease)
{
    if (lease.mutex)
    {
        if (namedProducerLeaseHeldByCurrentThread(lease))
            ReleaseMutex(lease.mutex);
        CloseHandle(lease.mutex);
    }
    if (lease.ownerThread)
        CloseHandle(lease.ownerThread);
    lease = {};
}

void releaseSharedStereo()
{
    if (gSharedStereoView)
    {
        writeInvalidSharedStereoRecord(
            reinterpret_cast<SharedStereoHeader*>(gSharedStereoView),
            false);
        UnmapViewOfFile(gSharedStereoView);
        gSharedStereoView = nullptr;
    }
    if (gSharedStereoMapping)
    {
        CloseHandle(gSharedStereoMapping);
        gSharedStereoMapping = nullptr;
    }
    releaseNamedProducerMutex(gSharedStereoProducerLease);
    gSharedStereoRendererProducerEpoch = 0;
}

void releaseSharedWorldVideo()
{
    if (gSharedWorldVideoView)
    {
        UnmapViewOfFile(gSharedWorldVideoView);
        gSharedWorldVideoView = nullptr;
    }
    if (gSharedWorldVideoMapping)
    {
        CloseHandle(gSharedWorldVideoMapping);
        gSharedWorldVideoMapping = nullptr;
    }
    releaseNamedProducerMutex(gSharedWorldVideoProducerLease);
}

void releaseSharedVrPose()
{
    if (gSharedVrPose)
    {
        UnmapViewOfFile(gSharedVrPose);
        gSharedVrPose = nullptr;
    }
    if (gSharedVrPoseMapping)
    {
        CloseHandle(gSharedVrPoseMapping);
        gSharedVrPoseMapping = nullptr;
    }
    gHaveVrPoseOrigin = false;
    gNativeGameplayPoseOriginLatched = false;
    gNativeViewOriginValid = false;
    gNativeLastRecenterRequestId = 0;
    gLatestVrPoseSnapshotValid = false;
    gNativeStereoRenderedPoseValid = false;
    gLatestVrPoseSnapshotSequence = 0;
    gNativeStereoRenderedPoseSequence = 0;
    gLatestVrPoseAcceptedAt.QuadPart = 0;
    gLastVrReferenceSpaceGeneration = 0;
    gLastVrProducerEpoch = 0;
    gVrPoseOrigin[0] = 0.0f;
    gVrPoseOrigin[1] = 0.0f;
    gVrPoseOrigin[2] = 0.0f;
    gVrPoseOriginRot[0] = 0.0f;
    gVrPoseOriginRot[1] = 0.0f;
    gVrPoseOriginRot[2] = 0.0f;
    gVrPoseOriginRot[3] = 1.0f;
    gVrPoseOriginPoseSequence = 0;
    gVrPoseOriginPoseFrame = 0;
    gVrPoseOriginGeneration = 0;
    gVrPoseOriginProducerEpoch = 0;
    gLatestHmdDelta[0] = 0.0f;
    gLatestHmdDelta[1] = 0.0f;
    gLatestHmdDelta[2] = 0.0f;
    gLatestHmdRotDelta[0] = 0.0f;
    gLatestHmdRotDelta[1] = 0.0f;
    gLatestHmdRotDelta[2] = 0.0f;
    gLatestHmdRotDelta[3] = 1.0f;
    gVrWorldYawCos = 1.0f;
    gVrWorldYawSin = 0.0f;
    gVrWorldYawHalfCos = 1.0f;
    gVrWorldYawHalfSin = 0.0f;
    gLatestHeadLocalMeters[0] = 0.0f;
    gLatestHeadLocalMeters[1] = 0.0f;
    gLatestHeadLocalMeters[2] = 0.0f;
    gLatestLeftEyeLocalMeters[0] = -0.032f;
    gLatestLeftEyeLocalMeters[1] = 0.0f;
    gLatestLeftEyeLocalMeters[2] = 0.0f;
    gLatestRightEyeLocalMeters[0] = 0.032f;
    gLatestRightEyeLocalMeters[1] = 0.0f;
    gLatestRightEyeLocalMeters[2] = 0.0f;
    gLatestRawIpdMeters = fnvxr::stereo::DefaultIpdMeters;
    gHaveVrBodyFrame = false;
    gLatestVrPoseSnapshotValid = false;
    gLatestVrPoseSnapshotSequence = 0;
    gLatestVrPoseDisplayTime = 0;
    gLastVrPoseSequence = 0;
    gLastVrPoseFrame = 0;
}

void releaseSharedCamera()
{
    if (gSharedCamera)
    {
        UnmapViewOfFile(gSharedCamera);
        gSharedCamera = nullptr;
    }
    if (gSharedCameraMapping)
    {
        CloseHandle(gSharedCameraMapping);
        gSharedCameraMapping = nullptr;
    }
    gSharedCameraActive = false;
    gSharedCameraRawActive = false;
    gSharedCameraMatrixUsable = false;
    gLatestSharedCameraSnapshotValid = false;
    gLatestSharedCameraAcceptedAt = {};
    gLastCameraSequence = 0;
}

void releaseSharedRuntime()
{
    if (gSharedRuntime)
    {
        UnmapViewOfFile(gSharedRuntime);
        gSharedRuntime = nullptr;
    }
    if (gSharedRuntimeMapping)
    {
        CloseHandle(gSharedRuntimeMapping);
        gSharedRuntimeMapping = nullptr;
    }
}

void releaseSharedVrOriginPublisher()
{
    if (gSharedVrOrigin)
    {
        publishSharedVrOrigin(false);
        UnmapViewOfFile(gSharedVrOrigin);
        gSharedVrOrigin = nullptr;
    }
    if (gSharedVrOriginMapping)
    {
        CloseHandle(gSharedVrOriginMapping);
        gSharedVrOriginMapping = nullptr;
    }
    gPublishedVrOriginActive = false;
    gPublishedVrOriginCommitted = false;
    gPublishedVrOriginGeneration = 0;
    releaseNamedProducerMutex(gSharedVrOriginProducerLease);
}

void releaseSharedPlayer()
{
    if (gSharedPlayer)
    {
        UnmapViewOfFile(gSharedPlayer);
        gSharedPlayer = nullptr;
    }
    if (gSharedPlayerMapping)
    {
        CloseHandle(gSharedPlayerMapping);
        gSharedPlayerMapping = nullptr;
    }
    gLatestSharedPlayerSnapshot = {};
    gLatestSharedPlayerSnapshotSequence = 0;
    gLatestSharedPlayerSnapshotValid = false;
    gNativeStableCellFormId = 0;
    gNativeCellChangePlayerFrame = 0;
    gNativeDoubleTraversalHoldUntilPlayerFrame = 0;
    gNativeCellTransitionReady = false;
    gNativeSharedPlayerReadMisses = 0;
}

bool sharedVideoEnabled()
{
    char buffer[8] {};
    size_t required = 0;
    if (getenv_s(&required, buffer, sizeof(buffer), "FNVXR_D3D9_SHARED_VIDEO") == 0 && required > 0)
        return buffer[0] != '0';
    return true;
}

bool sharedStereoEnabled()
{
    return stereoWorldRuntimeEnabled() && readEnvBool("FNVXR_D3D9_SHARED_STEREO", true);
}

bool ensureSharedVideo()
{
    if (gSharedVideoView)
        return acquireNamedProducerMutex(
            "Local\\FNVXR_D3D9_Frame_Producer_v1",
            gSharedVideoProducerLease);

    if (!acquireNamedProducerMutex(
            "Local\\FNVXR_D3D9_Frame_Producer_v1",
            gSharedVideoProducerLease))
    {
        logLine("shared video publisher lease unavailable; refusing second producer");
        return false;
    }

    constexpr DWORD mappingSize =
        sizeof(SharedVideoHeader) + SharedVideoMaxWidth * SharedVideoMaxHeight * 4;
    gSharedVideoMapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        mappingSize,
        "Local\\FNVXR_D3D9_Frame_v1");
    if (!gSharedVideoMapping)
    {
        logLine("shared video CreateFileMapping failed");
        releaseNamedProducerMutex(gSharedVideoProducerLease);
        return false;
    }
    const bool createdNewMapping = GetLastError() != ERROR_ALREADY_EXISTS;

    gSharedVideoView = static_cast<std::uint8_t*>(
        MapViewOfFile(gSharedVideoMapping, FILE_MAP_ALL_ACCESS, 0, 0, mappingSize));
    if (!gSharedVideoView)
    {
        logLine("shared video MapViewOfFile failed");
        CloseHandle(gSharedVideoMapping);
        gSharedVideoMapping = nullptr;
        releaseNamedProducerMutex(gSharedVideoProducerLease);
        return false;
    }

    auto* header = reinterpret_cast<SharedVideoHeader*>(gSharedVideoView);
    // A consumer may keep this page mapped across a producer restart. Raise
    // writing before relabeling retained storage, then publish a distinct
    // invalid record. Readers that were copying old pixels must reject when
    // they perform their final writing/sequence check.
    InterlockedExchange(&header->writing, 1);
    MemoryBarrier();
    const LONG previousSequence = !createdNewMapping && header->magic == SharedVideoMagic
        ? InterlockedCompareExchange(&header->sequence, 0, 0)
        : 0;
    header->magic = SharedVideoMagic;
    header->width = 0;
    header->height = 0;
    header->pitchBytes = 0;
    header->format = 0;
    InterlockedExchange(&header->sequence, previousSequence);
    fnvxr::shared::incrementNonzeroSharedCounter(header->sequence);
    MemoryBarrier();
    InterlockedExchange(&header->writing, 0);
    logLine("shared D3D9 video frame mapping ready");
    return true;
}

bool ensureSharedStereo()
{
    if (gSharedStereoView)
        return acquireNamedProducerMutex(
            "Local\\FNVXR_D3D9_Stereo_Producer_v7",
            gSharedStereoProducerLease);

    if (!acquireNamedProducerMutex(
            "Local\\FNVXR_D3D9_Stereo_Producer_v7",
            gSharedStereoProducerLease))
    {
        logLine("shared stereo publisher lease unavailable; refusing second producer");
        return false;
    }

    constexpr DWORD mappingSize =
        sizeof(SharedStereoHeader) + SharedVideoMaxWidth * SharedVideoMaxHeight * 4 * 2
            * fnvxr::shared::D3D9StereoFrameSlotCount;
    gSharedStereoMapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        mappingSize,
        fnvxr::shared::D3D9StereoFrameSharedMappingName);
    if (!gSharedStereoMapping)
    {
        logLine("shared stereo CreateFileMapping failed");
        releaseNamedProducerMutex(gSharedStereoProducerLease);
        gSharedStereoRendererProducerEpoch = 0;
        return false;
    }
    const bool createdNewMapping = GetLastError() != ERROR_ALREADY_EXISTS;

    gSharedStereoView = static_cast<std::uint8_t*>(
        MapViewOfFile(gSharedStereoMapping, FILE_MAP_ALL_ACCESS, 0, 0, mappingSize));
    if (!gSharedStereoView)
    {
        logLine("shared stereo MapViewOfFile failed");
        CloseHandle(gSharedStereoMapping);
        gSharedStereoMapping = nullptr;
        releaseNamedProducerMutex(gSharedStereoProducerLease);
        gSharedStereoRendererProducerEpoch = 0;
        return false;
    }

    auto* header = reinterpret_cast<SharedStereoHeader*>(gSharedStereoView);
    const bool existingValid = !createdNewMapping
        && header->magic == SharedStereoMagic
        && header->version == fnvxr::shared::D3D9StereoFrameSharedVersion
        && header->headerBytes == sizeof(SharedStereoHeader);
    const std::uint64_t previousRendererEpoch = existingValid
        ? static_cast<std::uint64_t>(InterlockedCompareExchange64(
            reinterpret_cast<volatile LONG64*>(&header->rendererProducerEpoch), 0, 0))
        : 0;
    if (existingValid
        && previousRendererEpoch == (std::numeric_limits<std::uint64_t>::max)())
    {
        logLine("shared stereo renderer epoch exhausted; refusing wrapped restart identity");
        releaseSharedStereo();
        return false;
    }
    gSharedStereoRendererProducerEpoch = existingValid
        ? previousRendererEpoch + 1u
        : 1u;
    // Close the retained-record relabeling window before changing lifetime
    // identity. Readers must see writing=1 until one complete invalid record
    // carrying the new epoch and a new generation/sequence is committed.
    InterlockedExchange(&header->writing, 1);
    MemoryBarrier();
    InterlockedExchange64(
        reinterpret_cast<volatile LONG64*>(&header->rendererProducerEpoch),
        static_cast<LONG64>(gSharedStereoRendererProducerEpoch));
    if (!existingValid)
    {
        InterlockedExchange(&header->publishedSlot, -1);
        for (std::uint32_t lane = 0;
             lane < fnvxr::shared::D3D9StereoFrameReaderLaneCount;
             ++lane)
        {
            InterlockedExchange(&header->readerSlots[lane], -1);
        }
        InterlockedExchange64(&header->publicationGeneration, 0);
    }
    writeInvalidSharedStereoRecord(header, false);
    logLine("shared D3D9 stereo frame mapping ready");
    return true;
}

bool ensureSharedWorldVideo()
{
    if (gSharedWorldVideoView)
        return acquireNamedProducerMutex(
            "Local\\FNVXR_D3D9_WorldFrame_Producer_v1",
            gSharedWorldVideoProducerLease);

    if (!acquireNamedProducerMutex(
            "Local\\FNVXR_D3D9_WorldFrame_Producer_v1",
            gSharedWorldVideoProducerLease))
    {
        logLine("shared world video publisher lease unavailable; refusing second producer");
        return false;
    }

    constexpr DWORD mappingSize =
        sizeof(SharedVideoHeader) + SharedVideoMaxWidth * SharedVideoMaxHeight * 4;
    char mappingName[96] {};
    if (GetEnvironmentVariableA("FNVXR_D3D9_WIDE_WORLD_FRAME_MAPPING", mappingName, sizeof(mappingName)) == 0)
        strcpy_s(mappingName, sizeof(mappingName), "Local\\FNVXR_D3D9_WorldFrame_v1");
    gSharedWorldVideoMapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        mappingSize,
        mappingName);
    if (!gSharedWorldVideoMapping)
    {
        logLine("shared world video CreateFileMapping failed");
        releaseNamedProducerMutex(gSharedWorldVideoProducerLease);
        return false;
    }
    const bool createdNewMapping = GetLastError() != ERROR_ALREADY_EXISTS;

    gSharedWorldVideoView = static_cast<std::uint8_t*>(
        MapViewOfFile(gSharedWorldVideoMapping, FILE_MAP_ALL_ACCESS, 0, 0, mappingSize));
    if (!gSharedWorldVideoView)
    {
        logLine("shared world video MapViewOfFile failed");
        CloseHandle(gSharedWorldVideoMapping);
        gSharedWorldVideoMapping = nullptr;
        releaseNamedProducerMutex(gSharedWorldVideoProducerLease);
        return false;
    }

    auto* header = reinterpret_cast<SharedVideoHeader*>(gSharedWorldVideoView);
    InterlockedExchange(&header->writing, 1);
    MemoryBarrier();
    const LONG previousSequence = !createdNewMapping && header->magic == SharedVideoMagic
        ? InterlockedCompareExchange(&header->sequence, 0, 0)
        : 0;
    header->magic = SharedVideoMagic;
    header->width = 0;
    header->height = 0;
    header->pitchBytes = 0;
    header->format = 0;
    InterlockedExchange(&header->sequence, previousSequence);
    fnvxr::shared::incrementNonzeroSharedCounter(header->sequence);
    MemoryBarrier();
    InterlockedExchange(&header->writing, 0);
    char message[160] {};
    sprintf_s(message, "shared D3D9 wide world frame mapping ready name=%s", mappingName);
    logLine(message);
    return true;
}

bool ensureSharedVideoReadback(IDirect3DDevice9* device, const D3DSURFACE_DESC& desc)
{
    if (gSharedVideoReadback
        && gSharedVideoWidth == desc.Width
        && gSharedVideoHeight == desc.Height
        && gSharedVideoFormat == desc.Format)
    {
        return true;
    }

    releaseSharedVideoReadback();
    if (desc.Width == 0
        || desc.Height == 0
        || desc.Width > SharedVideoMaxWidth
        || desc.Height > SharedVideoMaxHeight)
    {
        logLine("shared video backbuffer dimensions unsupported");
        return false;
    }

    const HRESULT result = device->CreateOffscreenPlainSurface(
        desc.Width,
        desc.Height,
        desc.Format,
        D3DPOOL_SYSTEMMEM,
        &gSharedVideoReadback,
        nullptr);
    if (FAILED(result))
    {
        logLine("shared video CreateOffscreenPlainSurface failed");
        return false;
    }

    gSharedVideoWidth = desc.Width;
    gSharedVideoHeight = desc.Height;
    gSharedVideoFormat = desc.Format;

    char message[160] {};
    sprintf_s(
        message,
        "shared video readback surface %ux%u format=%u",
        desc.Width,
        desc.Height,
        static_cast<unsigned>(desc.Format));
    logLine(message);
    return true;
}

void captureSharedVideoFrame(IDirect3DDevice9* device)
{
    if (!sharedVideoEnabled() || !ensureSharedVideo())
        return;

    const float captureHz = readEnvFloat("FNVXR_D3D9_SHARED_VIDEO_CAPTURE_HZ", 0.0f);
    LARGE_INTEGER captureStart {};
    QueryPerformanceCounter(&captureStart);
    if (captureHz > 0.0f && gLastSharedVideoCaptureSample.QuadPart != 0)
    {
        const double elapsed = secondsBetween(gLastSharedVideoCaptureSample, captureStart);
        if (elapsed < 1.0 / static_cast<double>(captureHz))
        {
            const LONG skipped = InterlockedIncrement(&gSharedVideoSkippedByThrottle);
            if (skipped <= 3 || skipped % 300 == 0)
            {
                char skipMessage[192] {};
                sprintf_s(
                    skipMessage,
                    "shared video capture throttled skipped=%ld frame=%ld hz=%.2f elapsedMs=%.3f",
                    skipped,
                    static_cast<LONG>(gPresentFrames),
                    captureHz,
                    elapsed * 1000.0);
                logLine(skipMessage);
            }
            return;
        }
    }
    gLastSharedVideoCaptureSample = captureStart;

    IDirect3DSurface9* backBuffer = nullptr;
    if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)) || !backBuffer)
        return;

    D3DSURFACE_DESC desc {};
    if (FAILED(backBuffer->GetDesc(&desc)) || !ensureSharedVideoReadback(device, desc))
    {
        releaseSurface(backBuffer);
        return;
    }

    LARGE_INTEGER readbackStart {};
    QueryPerformanceCounter(&readbackStart);
    if (FAILED(device->GetRenderTargetData(backBuffer, gSharedVideoReadback)))
    {
        releaseSurface(backBuffer);
        return;
    }
    LARGE_INTEGER readbackEnd {};
    QueryPerformanceCounter(&readbackEnd);

    D3DLOCKED_RECT locked {};
    LARGE_INTEGER lockStart {};
    QueryPerformanceCounter(&lockStart);
    if (FAILED(gSharedVideoReadback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
    {
        releaseSurface(backBuffer);
        return;
    }
    LARGE_INTEGER lockEnd {};
    QueryPerformanceCounter(&lockEnd);

    auto* header = reinterpret_cast<SharedVideoHeader*>(gSharedVideoView);
    auto* pixels = gSharedVideoView + sizeof(SharedVideoHeader);
    InterlockedExchange(&header->writing, 1);
    header->magic = SharedVideoMagic;
    header->width = static_cast<LONG>(desc.Width);
    header->height = static_cast<LONG>(desc.Height);
    header->pitchBytes = static_cast<LONG>(desc.Width * 4);
    header->format = static_cast<LONG>(desc.Format);

    const std::uint8_t* src = static_cast<const std::uint8_t*>(locked.pBits);
    LARGE_INTEGER copyStart {};
    QueryPerformanceCounter(&copyStart);
    for (UINT y = 0; y < desc.Height; ++y)
        std::memcpy(pixels + y * desc.Width * 4, src + y * locked.Pitch, desc.Width * 4);
    LARGE_INTEGER copyEnd {};
    QueryPerformanceCounter(&copyEnd);

    gSharedVideoReadback->UnlockRect();
    fnvxr::shared::incrementNonzeroSharedCounter(header->sequence);
    InterlockedExchange(&header->writing, 0);
    releaseSurface(backBuffer);
    LARGE_INTEGER captureEnd {};
    QueryPerformanceCounter(&captureEnd);

    const LONG captures = InterlockedIncrement(&gSharedVideoCaptures);
    const double totalMs = secondsBetween(captureStart, captureEnd) * 1000.0;
    const double readbackMs = secondsBetween(readbackStart, readbackEnd) * 1000.0;
    const double lockMs = secondsBetween(lockStart, lockEnd) * 1000.0;
    const double copyMs = secondsBetween(copyStart, copyEnd) * 1000.0;
    const float slowMs = readEnvFloat("FNVXR_D3D9_SHARED_VIDEO_SLOW_MS", 4.0f);
    const bool slow = totalMs >= static_cast<double>(slowMs);
    if (captures <= 3 || captures % 300 == 0)
    {
        char message[160] {};
        sprintf_s(
            message,
            "shared video captured frame=%ld size=%ux%u seq=%ld",
            static_cast<LONG>(gPresentFrames),
            desc.Width,
            desc.Height,
            static_cast<LONG>(header->sequence));
        logLine(message);
    }
    if (captures <= 5 || captures % 300 == 0 || slow)
    {
        const LONG perfLogs = InterlockedIncrement(&gSharedVideoPerfLogs);
        if (perfLogs <= 24 || perfLogs % 120 == 0 || slow)
        {
            char perfMessage[384] {};
            sprintf_s(
                perfMessage,
                "shared video perf frame=%ld seq=%ld totalMs=%.3f readbackMs=%.3f lockMs=%.3f copyMs=%.3f size=%ux%u pitch=%ld captureHz=%.2f skipped=%ld slow=%d",
                static_cast<LONG>(gPresentFrames),
                static_cast<LONG>(header->sequence),
                totalMs,
                readbackMs,
                lockMs,
                copyMs,
                desc.Width,
                desc.Height,
                static_cast<LONG>(locked.Pitch),
                captureHz,
                static_cast<LONG>(gSharedVideoSkippedByThrottle),
                slow ? 1 : 0);
            logLine(perfMessage);
        }
    }
}

void releaseTexture(IDirect3DTexture9*& texture)
{
    if (texture)
    {
        texture->Release();
        texture = nullptr;
    }
}

void releaseStereoSurfaceTwin(StereoSurfaceTwin& twin)
{
    for (UINT index = 0; index < MaxStereoSurfaceTwins; ++index)
    {
        if (gStereoSurfaceTwins[index].lastDepthPrimeTwin == &twin)
        {
            gStereoSurfaceTwins[index].lastDepthPrimeTwin = nullptr;
            gStereoSurfaceTwins[index].lastDepthPrimeFrame = -1;
        }
    }
    releaseSurface(twin.left);
    releaseSurface(twin.right);
    releaseTexture(twin.leftTexture);
    releaseTexture(twin.rightTexture);
    releaseTexture(twin.originalTexture);
    releaseSurface(twin.original);
    std::memset(&twin.desc, 0, sizeof(twin.desc));
    twin.depth = false;
    twin.lastUsedFrame = 0;
    twin.lastDepthPrimeFrame = -1;
    twin.lastDepthPrimeTwin = nullptr;
    twin.equivalentInitializationGeneration = 0;
    twin.leftValidGeneration = 0;
    twin.rightValidGeneration = 0;
}

void releaseStereoSurfaceTwins()
{
    for (UINT i = 0; i < MaxStereoSurfaceTwins; ++i)
        releaseStereoSurfaceTwin(gStereoSurfaceTwins[i]);
}

bool surfaceDescMatches(const D3DSURFACE_DESC& a, const D3DSURFACE_DESC& b)
{
    return a.Width == b.Width
        && a.Height == b.Height
        && a.Format == b.Format
        && a.Usage == b.Usage
        && a.Pool == b.Pool
        && a.MultiSampleType == b.MultiSampleType
        && a.MultiSampleQuality == b.MultiSampleQuality;
}

HRESULT createTwinRenderSurface(
    IDirect3DDevice9* device,
    const D3DSURFACE_DESC& desc,
    bool depth,
    IDirect3DSurface9* original,
    IDirect3DSurface9** surface,
    IDirect3DTexture9** texture)
{
    if (!device || !surface || !texture)
        return D3DERR_INVALIDCALL;

    *surface = nullptr;
    *texture = nullptr;
    if (depth || (desc.Usage & D3DUSAGE_DEPTHSTENCIL) != 0)
    {
        if (gRealCreateDepthStencilSurface)
        {
            return gRealCreateDepthStencilSurface(
                device,
                desc.Width,
                desc.Height,
                desc.Format,
                desc.MultiSampleType,
                desc.MultiSampleQuality,
                FALSE,
                surface,
                nullptr);
        }
        return device->CreateDepthStencilSurface(
            desc.Width,
            desc.Height,
            desc.Format,
            desc.MultiSampleType,
            desc.MultiSampleQuality,
            FALSE,
            surface,
            nullptr);
    }

    IDirect3DTexture9* originalTexture = nullptr;
    const bool textureBacked =
        original
        && desc.MultiSampleType == D3DMULTISAMPLE_NONE
        && SUCCEEDED(original->GetContainer(__uuidof(IDirect3DTexture9), reinterpret_cast<void**>(&originalTexture)))
        && originalTexture;
    if (textureBacked)
    {
        D3DSURFACE_DESC levelDesc {};
        if (FAILED(originalTexture->GetLevelDesc(0, &levelDesc))
            || levelDesc.Width != desc.Width
            || levelDesc.Height != desc.Height
            || levelDesc.Format != desc.Format)
        {
            releaseTexture(originalTexture);
        }
        else
        {
            IDirect3DTexture9* createdTexture = nullptr;
            const HRESULT textureResult = gRealCreateTexture
                ? gRealCreateTexture(
                    device,
                    desc.Width,
                    desc.Height,
                    1,
                    D3DUSAGE_RENDERTARGET,
                    desc.Format,
                    D3DPOOL_DEFAULT,
                    &createdTexture,
                    nullptr)
                : device->CreateTexture(
                    desc.Width,
                    desc.Height,
                    1,
                    D3DUSAGE_RENDERTARGET,
                    desc.Format,
                    D3DPOOL_DEFAULT,
                    &createdTexture,
                    nullptr);
            if (SUCCEEDED(textureResult) && createdTexture)
            {
                const HRESULT surfaceResult = createdTexture->GetSurfaceLevel(0, surface);
                if (SUCCEEDED(surfaceResult) && *surface)
                {
                    *texture = createdTexture;
                    releaseTexture(originalTexture);
                    return D3D_OK;
                }
                releaseTexture(createdTexture);
                releaseTexture(originalTexture);
                return FAILED(surfaceResult) ? surfaceResult : D3DERR_INVALIDCALL;
            }
            releaseTexture(originalTexture);
            if (FAILED(textureResult))
                return textureResult;
        }
    }

    if (gRealCreateRenderTarget)
    {
        return gRealCreateRenderTarget(
            device,
            desc.Width,
            desc.Height,
            desc.Format,
            desc.MultiSampleType,
            desc.MultiSampleQuality,
            FALSE,
            surface,
            nullptr);
    }
    return device->CreateRenderTarget(
        desc.Width,
        desc.Height,
        desc.Format,
        desc.MultiSampleType,
        desc.MultiSampleQuality,
        FALSE,
        surface,
        nullptr);
}

StereoSurfaceTwin* findSurfaceTwin(IDirect3DSurface9* original, bool depth)
{
    if (!original)
        return nullptr;
    for (UINT index = 0; index < MaxStereoSurfaceTwins; ++index)
    {
        StereoSurfaceTwin& twin = gStereoSurfaceTwins[index];
        if (twin.original
            && sameComIdentity(twin.original, original)
            && twin.depth == depth
            && twin.left
            && twin.right)
            return &twin;
    }
    return nullptr;
}

bool stereoSurfaceTwinPinned(const StereoSurfaceTwin* twin)
{
    if (!twin)
        return true;
    if (gNativeResolvedTwinForEye[0] == twin || gNativeResolvedTwinForEye[1] == twin)
        return true;
    for (UINT index = 0; index < MaxNativeFullSizeTargetCandidates; ++index)
    {
        if (gNativeFullSizeTargetCandidates[index].twin == twin)
            return true;
    }
    for (UINT index = 0; index < MaxNativePostprocessDrawRecords; ++index)
    {
        if (gNativePostprocessDrawRecords[index].targetTwin == twin
            || gNativePostprocessDrawRecords[index].depthTwin == twin)
        {
            return true;
        }
    }
    if (gStereoTargetsAliasTwin
        && (twin->left == gLeftEyeSurface || twin->right == gRightEyeSurface))
    {
        return true;
    }
    return false;
}

StereoSurfaceTwin* ensureSurfaceTwin(IDirect3DDevice9* device, IDirect3DSurface9* original, bool depth)
{
    if ((!gStereoReplayEnabled && !gNativeStereoEnabled) || !device || !original)
        return nullptr;

    D3DSURFACE_DESC desc {};
    if (FAILED(original->GetDesc(&desc)) || desc.Width == 0 || desc.Height == 0)
        return nullptr;

    for (UINT i = 0; i < MaxStereoSurfaceTwins; ++i)
    {
        StereoSurfaceTwin& twin = gStereoSurfaceTwins[i];
        if (twin.original && sameComIdentity(twin.original, original))
        {
            if (twin.left && twin.right && twin.depth == depth && surfaceDescMatches(twin.desc, desc))
            {
                twin.lastUsedFrame = static_cast<LONG>(gPresentFrames);
                return &twin;
            }
            releaseStereoSurfaceTwin(twin);
            break;
        }
    }

    StereoSurfaceTwin* slot = nullptr;
    for (UINT i = 0; i < MaxStereoSurfaceTwins; ++i)
    {
        if (!gStereoSurfaceTwins[i].original)
        {
            slot = &gStereoSurfaceTwins[i];
            break;
        }
    }

    if (!slot)
    {
        const std::uint32_t currentFrame =
            static_cast<std::uint32_t>(static_cast<LONG>(gPresentFrames));
        std::uint32_t greatestAge = 0;
        for (UINT i = 0; i < MaxStereoSurfaceTwins; ++i)
        {
            StereoSurfaceTwin& candidate = gStereoSurfaceTwins[i];
            const std::uint32_t usedFrame =
                static_cast<std::uint32_t>(candidate.lastUsedFrame);
            const std::uint32_t age = currentFrame - usedFrame;
            if (age != 0u
                && age < 0x80000000u
                && age >= greatestAge
                && !stereoSurfaceTwinPinned(&candidate))
            {
                greatestAge = age;
                slot = &candidate;
            }
        }
        if (slot)
        {
            releaseStereoSurfaceTwin(*slot);
        }
        else
        {
            const LONG failLog = InterlockedIncrement(&gLoggedStereoSurfaceTwinFailed);
            if (failLog <= 8 || failLog % 120 == 0)
                logLine("stereo surface twin table full with every entry live in the current transaction");
            return nullptr;
        }
    }

    IDirect3DSurface9* left = nullptr;
    IDirect3DSurface9* right = nullptr;
    IDirect3DTexture9* leftTexture = nullptr;
    IDirect3DTexture9* rightTexture = nullptr;
    const HRESULT leftResult = createTwinRenderSurface(device, desc, depth, original, &left, &leftTexture);
    const HRESULT rightResult = SUCCEEDED(leftResult)
        ? createTwinRenderSurface(device, desc, depth, original, &right, &rightTexture)
        : leftResult;
    if (FAILED(leftResult) || FAILED(rightResult) || !left || !right)
    {
        releaseSurface(left);
        releaseSurface(right);
        releaseTexture(leftTexture);
        releaseTexture(rightTexture);
        const LONG failLog = InterlockedIncrement(&gLoggedStereoSurfaceTwinFailed);
        if (failLog <= 16 || failLog % 120 == 0)
        {
            char message[256] {};
            sprintf_s(
                message,
                "failed to create stereo surface twin count=%ld depth=%d size=%ux%u fmt=%lu usage=0x%lx ms=%u result=0x%08lx/0x%08lx",
                failLog,
                depth ? 1 : 0,
                desc.Width,
                desc.Height,
                static_cast<unsigned long>(desc.Format),
                static_cast<unsigned long>(desc.Usage),
                static_cast<unsigned>(desc.MultiSampleType),
                static_cast<unsigned long>(leftResult),
                static_cast<unsigned long>(rightResult));
            logLine(message);
        }
        return nullptr;
    }

    original->AddRef();
    slot->original = original;
    slot->left = left;
    slot->right = right;
    slot->leftTexture = leftTexture;
    slot->rightTexture = rightTexture;
    if (leftTexture && rightTexture)
        original->GetContainer(__uuidof(IDirect3DTexture9), reinterpret_cast<void**>(&slot->originalTexture));
    slot->desc = desc;
    slot->depth = depth;
    slot->lastUsedFrame = static_cast<LONG>(gPresentFrames);

    const LONG createLog = InterlockedIncrement(&gLoggedStereoSurfaceTwinCreated);
    if (createLog <= 32 || createLog % 120 == 0)
    {
        char message[288] {};
        sprintf_s(
            message,
            "created stereo surface twin count=%ld original=%p depth=%d size=%ux%u fmt=%lu usage=0x%lx ms=%u quality=%lu",
            createLog,
            reinterpret_cast<void*>(original),
            depth ? 1 : 0,
            desc.Width,
            desc.Height,
            static_cast<unsigned long>(desc.Format),
            static_cast<unsigned long>(desc.Usage),
            static_cast<unsigned>(desc.MultiSampleType),
            static_cast<unsigned long>(desc.MultiSampleQuality));
        logLine(message);
    }
    return slot;
}

bool sameComObject(IUnknown* a, IUnknown* b)
{
    if (a == b)
        return true;
    if (!a || !b)
        return false;

    IUnknown* unknownA = nullptr;
    IUnknown* unknownB = nullptr;
    const HRESULT resultA = a->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&unknownA));
    const HRESULT resultB = b->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&unknownB));
    const bool same = SUCCEEDED(resultA) && SUCCEEDED(resultB) && unknownA == unknownB;
    if (unknownA)
        unknownA->Release();
    if (unknownB)
        unknownB->Release();
    return same;
}

IDirect3DBaseTexture9* stereoTextureFor(IDirect3DBaseTexture9* original, bool leftEye)
{
    if (!original)
        return nullptr;

    for (UINT i = 0; i < MaxStereoSurfaceTwins; ++i)
    {
        StereoSurfaceTwin& twin = gStereoSurfaceTwins[i];
        if (!twin.originalTexture || !twin.leftTexture || !twin.rightTexture)
            continue;
        if (sameComObject(original, twin.originalTexture))
        {
            twin.lastUsedFrame = static_cast<LONG>(gPresentFrames);
            return leftEye ? twin.leftTexture : twin.rightTexture;
        }
    }
    return nullptr;
}

int nativePipelineTracePhase()
{
    if (gNativePipelineTraceThisPair && gInNativeStereoHook)
    {
        const LONG eye = gNativeActiveEye;
        if (eye >= 0 && eye <= 1)
            return static_cast<int>(eye);
    }
    return gNativePipelineTracePostprocessWindow ? 2 : -1;
}

const char* nativePipelineTracePhaseName(int phase)
{
    switch (phase)
    {
    case 0:
        return "left";
    case 1:
        return "right";
    case 2:
        return "post";
    default:
        return "inactive";
    }
}

void replaceTrackedTexture(UINT sampler, IDirect3DBaseTexture9* texture, bool adoptReference)
{
    if (sampler >= MaxTrackedSamplers)
    {
        if (adoptReference && texture)
            texture->Release();
        return;
    }
    if (texture && !adoptReference)
        texture->AddRef();
    IDirect3DBaseTexture9* previous = gActiveTextures[sampler];
    gActiveTextures[sampler] = texture;
    if (previous)
        previous->Release();
}

void releaseTrackedTextures()
{
    for (UINT sampler = 0; sampler < MaxTrackedSamplers; ++sampler)
        replaceTrackedTexture(sampler, nullptr, false);
}

bool claimNativePipelineTraceEvent(int& phase, LONG& event)
{
    phase = nativePipelineTracePhase();
    if (phase < 0 || phase > 2)
        return false;
    const LONG limit = static_cast<LONG>(readEnvFloat(
        phase == 2
            ? "FNVXR_D3D9_NATIVE_POST_TRACE_LIMIT"
            : "FNVXR_D3D9_NATIVE_EYE_TRACE_LIMIT",
        phase == 2 ? 320.0f : 160.0f));
    event = InterlockedIncrement(&gNativePipelineTraceEvents[phase]);
    return limit > 0 && event <= limit;
}

void formatNativePipelineStack(char* buffer, size_t bufferSize, ULONG framesToSkip = 0)
{
    if (!buffer || bufferSize == 0)
        return;
    buffer[0] = '\0';
    void* frames[14] {};
    const USHORT frameCount = CaptureStackBackTrace(
        framesToSkip + 1,
        static_cast<DWORD>(sizeof(frames) / sizeof(frames[0])),
        frames,
        nullptr);
    size_t offset = 0;
    for (USHORT index = 0; index < frameCount && offset + 12 < bufferSize; ++index)
    {
        const int written = sprintf_s(
            buffer + offset,
            bufferSize - offset,
            index == 0 ? "%p" : ",%p",
            frames[index]);
        if (written <= 0)
            break;
        offset += static_cast<size_t>(written);
    }
}

void formatNativePipelineStackScan(char* buffer, size_t bufferSize)
{
    if (!buffer || bufferSize == 0)
        return;
    buffer[0] = '\0';
    auto** stack = reinterpret_cast<void**>(_AddressOfReturnAddress());
    size_t offset = 0;
    UINT found = 0;
    __try
    {
        for (UINT word = 0; word < 8192 && found < 28 && offset + 24 < bufferSize; ++word)
        {
            const std::uintptr_t value = reinterpret_cast<std::uintptr_t>(stack[word]);
            if (value < 0x00800000 || value >= 0x00FDE600)
                continue;
            bool callReturn = false;
            const auto* returnAddress = reinterpret_cast<const std::uint8_t*>(value);
            if (returnAddress[-5] == 0xE8)
            {
                callReturn = true;
            }
            else
            {
                for (UINT instructionLength = 2; instructionLength <= 8 && !callReturn; ++instructionLength)
                {
                    const auto* instruction = returnAddress - instructionLength;
                    if (instruction[0] != 0xFF || ((instruction[1] >> 3) & 7) != 2)
                        continue;
                    const std::uint8_t modrm = instruction[1];
                    const UINT mod = modrm >> 6;
                    const UINT rm = modrm & 7;
                    UINT decodedLength = 2;
                    if (mod != 3 && rm == 4)
                    {
                        const std::uint8_t sib = instruction[decodedLength++];
                        if (mod == 0 && (sib & 7) == 5)
                            decodedLength += 4;
                    }
                    if (mod == 0 && rm == 5)
                        decodedLength += 4;
                    else if (mod == 1)
                        decodedLength += 1;
                    else if (mod == 2)
                        decodedLength += 4;
                    callReturn = decodedLength == instructionLength;
                }
            }
            if (!callReturn)
                continue;
            const int written = sprintf_s(
                buffer + offset,
                bufferSize - offset,
                found == 0 ? "%u:%08lx" : ",%u:%08lx",
                word,
                static_cast<unsigned long>(value));
            if (written <= 0)
                break;
            offset += static_cast<size_t>(written);
            ++found;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

bool baseTextureLevelZeroDesc(IDirect3DBaseTexture9* texture, D3DSURFACE_DESC& desc)
{
    std::memset(&desc, 0, sizeof(desc));
    if (!texture || texture->GetType() != D3DRTYPE_TEXTURE)
        return false;
    auto* texture2d = static_cast<IDirect3DTexture9*>(texture);
    return SUCCEEDED(texture2d->GetLevelDesc(0, &desc));
}

void resetNativeFullSizeTargetCandidates()
{
    for (UINT index = 0; index < MaxNativeFullSizeTargetCandidates; ++index)
    {
        NativeFullSizeTargetCandidate& candidate = gNativeFullSizeTargetCandidates[index];
        releaseSurface(candidate.original);
        candidate = {};
    }
    InterlockedExchange(&gNativeRenderTargetSetOrdinal[0], 0);
    InterlockedExchange(&gNativeRenderTargetSetOrdinal[1], 0);
}

NativeFullSizeTargetCandidate* trackNativeFullSizeTargetCandidate(
    IDirect3DSurface9* surface,
    int eye)
{
    if (!surface || eye < 0 || eye > 1 || gStereoTargetWidth == 0 || gStereoTargetHeight == 0)
        return nullptr;

    D3DSURFACE_DESC desc {};
    if (FAILED(surface->GetDesc(&desc))
        || desc.Width != gStereoTargetWidth
        || desc.Height != gStereoTargetHeight
        || (desc.Usage & D3DUSAGE_DEPTHSTENCIL) != 0)
    {
        return nullptr;
    }

    NativeFullSizeTargetCandidate* freeSlot = nullptr;
    for (UINT index = 0; index < MaxNativeFullSizeTargetCandidates; ++index)
    {
        NativeFullSizeTargetCandidate& candidate = gNativeFullSizeTargetCandidates[index];
        if (!candidate.original)
        {
            if (!freeSlot)
                freeSlot = &candidate;
            continue;
        }
        if (candidate.original == surface || sameComObject(candidate.original, surface))
        {
            candidate.seen[eye] = true;
            candidate.lastSetOrdinal[eye] = InterlockedIncrement(&gNativeRenderTargetSetOrdinal[eye]);
            return &candidate;
        }
    }

    if (!freeSlot)
        return nullptr;
    surface->AddRef();
    freeSlot->original = surface;
    freeSlot->desc = desc;
    freeSlot->seen[eye] = true;
    freeSlot->lastSetOrdinal[eye] = InterlockedIncrement(&gNativeRenderTargetSetOrdinal[eye]);
    return freeSlot;
}

void snapshotNativeFullSizeTargets(IDirect3DDevice9* device, int eye)
{
    if (!device || !gRealStretchRect || eye < 0 || eye > 1)
        return;

    for (UINT index = 0; index < MaxNativeFullSizeTargetCandidates; ++index)
    {
        NativeFullSizeTargetCandidate& candidate = gNativeFullSizeTargetCandidates[index];
        const LONG pairSequence = nextNativeStereoPairSequence();
        const std::uint32_t pairBits = fnvxr::shared::sequencedValueBits(pairSequence);
        const bool auditSnapshot = gNativePipelineTraceThisPair
            || nativeStereoPairNeedsWarmupAudit(3)
            || pairBits % 120u == 0u;
        const bool requiredNonMsaaSource = candidate.desc.Format == D3DFMT_A16B16G16R16F
            && candidate.desc.MultiSampleType == D3DMULTISAMPLE_NONE;
        const bool shouldSnapshot = candidate.original
            && (auditSnapshot || requiredNonMsaaSource)
            && (candidate.seen[eye] || (eye == 1 && candidate.snapshotted[0]));
        if (!shouldSnapshot)
            continue;

        candidate.twin = ensureSurfaceTwin(device, candidate.original, false);
        IDirect3DSurface9* destination = candidate.twin
            ? (eye == 0 ? candidate.twin->left : candidate.twin->right)
            : nullptr;
        const HRESULT result = destination
            ? gRealStretchRect(device, candidate.original, nullptr, destination, nullptr, D3DTEXF_NONE)
            : D3DERR_INVALIDCALL;
        candidate.snapshotted[eye] = SUCCEEDED(result);
        if (SUCCEEDED(result)
            && candidate.twin
            && candidate.desc.Format == D3DFMT_A16B16G16R16F
            && candidate.desc.MultiSampleType == D3DMULTISAMPLE_NONE)
        {
            gNativeResolvedTwinForEye[eye] = candidate.twin;
            gNativeResolvedDesc = candidate.desc;
            if (gNativeResolvedCopies[eye] == 0)
                InterlockedIncrement(&gNativeResolvedCopies[eye]);
        }
        if (gNativePipelineTraceThisPair)
        {
            char message[448] {};
            sprintf_s(
                message,
                "native pipeline target snapshot pair=%ld eye=%s candidate=%u original=%p result=0x%08lx size=%ux%u format=%lu usage=0x%lx ms=%u lastSet=%ld textureBacked=%d",
                nextNativeStereoPairSequence(),
                eye == 0 ? "left" : "right",
                index,
                reinterpret_cast<void*>(candidate.original),
                static_cast<unsigned long>(result),
                candidate.desc.Width,
                candidate.desc.Height,
                static_cast<unsigned long>(candidate.desc.Format),
                static_cast<unsigned long>(candidate.desc.Usage),
                static_cast<unsigned>(candidate.desc.MultiSampleType),
                candidate.lastSetOrdinal[eye],
                candidate.twin && candidate.twin->originalTexture ? 1 : 0);
            logLine(message);
        }
    }
}

void releaseNativePostprocessDrawRecord(NativePostprocessDrawRecord& record)
{
    if (record.stateBlock)
        record.stateBlock->Release();
    releaseSurface(record.originalTarget);
    releaseSurface(record.originalDepth);
    for (UINT sampler = 0; sampler < MaxTrackedSamplers; ++sampler)
    {
        if (record.textures[sampler])
            record.textures[sampler]->Release();
    }
    record = {};
}

bool nativePostprocessCapturedLeftFinalTarget()
{
    const LONG recordCount = gNativePostprocessRecordedDrawCount;
    for (LONG index = 0;
         index >= 0 && index < recordCount && index < static_cast<LONG>(MaxNativePostprocessDrawRecords);
         ++index)
    {
        const NativePostprocessDrawRecord& record = gNativePostprocessDrawRecords[index];
        if (record.targetTwin && record.targetTwin->left == gLeftEyeSurface)
            return true;
    }
    return false;
}

void resetNativePostprocessCommandStream()
{
    for (UINT index = 0; index < MaxNativePostprocessDrawRecords; ++index)
        releaseNativePostprocessDrawRecord(gNativePostprocessDrawRecords[index]);
    gNativePostprocessRecording = false;
    gNativePostprocessRecordUnsupported = false;
    InterlockedExchange(&gNativePostprocessRecordedDrawCount, 0);
    InterlockedExchange(&gNativePostprocessReplayedDrawCount, 0);
}

bool captureNativePostprocessDraw(
    IDirect3DDevice9* device,
    NativePostprocessDrawKind kind,
    D3DPRIMITIVETYPE primitiveType,
    UINT startVertex,
    UINT primitiveCount,
    INT baseVertexIndex,
    UINT minVertexIndex,
    UINT numVertices,
    UINT startIndex)
{
    if (!device
        || !gNativePostprocessRecording
        || !gInNativeStereoHook
        || gNativeActiveEye != 0
        || gInStereoReplay)
    {
        return false;
    }

    const LONG ordinal = gNativePostprocessRecordedDrawCount;
    if (ordinal < 0 || ordinal >= static_cast<LONG>(MaxNativePostprocessDrawRecords))
    {
        gNativePostprocessRecordUnsupported = true;
        logLine("native postprocess command stream exceeded draw record capacity");
        return false;
    }

    NativePostprocessDrawRecord& record = gNativePostprocessDrawRecords[ordinal];
    releaseNativePostprocessDrawRecord(record);
    record.kind = kind;
    record.primitiveType = primitiveType;
    record.startVertex = startVertex;
    record.primitiveCount = primitiveCount;
    record.baseVertexIndex = baseVertexIndex;
    record.minVertexIndex = minVertexIndex;
    record.numVertices = numVertices;
    record.startIndex = startIndex;

    const HRESULT createStateResult = device->CreateStateBlock(D3DSBT_ALL, &record.stateBlock);
    const HRESULT captureStateResult = SUCCEEDED(createStateResult) && record.stateBlock
        ? record.stateBlock->Capture()
        : createStateResult;
    if (FAILED(createStateResult)
        || FAILED(captureStateResult)
        || FAILED(device->GetRenderTarget(0, &record.originalTarget))
        || !record.originalTarget)
    {
        releaseNativePostprocessDrawRecord(record);
        gNativePostprocessRecordUnsupported = true;
        return false;
    }
    device->GetDepthStencilSurface(&record.originalDepth);
    record.targetTwin = ensureSurfaceTwin(device, record.originalTarget, false);
    record.depthTwin = record.originalDepth
        ? ensureSurfaceTwin(device, record.originalDepth, true)
        : nullptr;
    if (!record.targetTwin || !record.targetTwin->left || !record.targetTwin->right)
    {
        releaseNativePostprocessDrawRecord(record);
        gNativePostprocessRecordUnsupported = true;
        return false;
    }

    record.haveViewport = SUCCEEDED(device->GetViewport(&record.viewport));
    record.haveScissor = SUCCEEDED(device->GetScissorRect(&record.scissor));
    for (UINT sampler = 0; sampler < MaxTrackedSamplers; ++sampler)
    {
        record.textures[sampler] = gActiveTextures[sampler];
        if (record.textures[sampler])
            record.textures[sampler]->AddRef();
    }

    const HRESULT leftSnapshot = gRealStretchRect
        ? gRealStretchRect(
            device,
            record.originalTarget,
            nullptr,
            record.targetTwin->left,
            nullptr,
            D3DTEXF_NONE)
        : D3DERR_INVALIDCALL;
    if (FAILED(leftSnapshot))
    {
        releaseNativePostprocessDrawRecord(record);
        gNativePostprocessRecordUnsupported = true;
        return false;
    }

    InterlockedIncrement(&gNativePostprocessRecordedDrawCount);
    const LONG pairSequence = nextNativeStereoPairSequence();
    if (nativeStereoPairNeedsWarmupAudit(3)
        || fnvxr::shared::sequencedValueBits(pairSequence) % 120u == 0u)
    {
        char message[576] {};
        sprintf_s(
            message,
            "native postprocess draw recorded pair=%ld ordinal=%ld kind=%s target=%p size=%ux%u format=%lu viewport=%lux%lu vsHash=0x%08x psHash=0x%08x",
            pairSequence,
            ordinal + 1,
            kind == NativePostprocessDrawKind::DrawIndexedPrimitive ? "indexed" : "primitive",
            record.originalTarget,
            record.targetTwin->desc.Width,
            record.targetTwin->desc.Height,
            static_cast<unsigned long>(record.targetTwin->desc.Format),
            record.haveViewport ? static_cast<unsigned long>(record.viewport.Width) : 0ul,
            record.haveViewport ? static_cast<unsigned long>(record.viewport.Height) : 0ul,
            gActiveVertexShaderHash,
            gActivePixelShaderHash);
        logLine(message);
    }
    return true;
}

bool replayNativePostprocessCommandStreamForRightEye(IDirect3DDevice9* device)
{
    const LONG recordCount = gNativePostprocessRecordedDrawCount;
    if (!device
        || recordCount <= 0
        || recordCount > static_cast<LONG>(MaxNativePostprocessDrawRecords)
        || gNativePostprocessRecordUnsupported
        || !gRealSetRenderTarget
        || !gRealSetDepthStencilSurface
        || !gRealSetTexture
        || !gRealDrawPrimitive
        || !gRealDrawIndexedPrimitive)
    {
        return false;
    }

    IDirect3DStateBlock9* restoreState = nullptr;
    const HRESULT createRestoreResult = device->CreateStateBlock(D3DSBT_ALL, &restoreState);
    const HRESULT captureRestoreResult = SUCCEEDED(createRestoreResult) && restoreState
        ? restoreState->Capture()
        : createRestoreResult;
    IDirect3DSurface9* restoreTarget = nullptr;
    IDirect3DSurface9* restoreDepth = nullptr;
    D3DVIEWPORT9 restoreViewport {};
    RECT restoreScissor {};
    const bool haveRestoreTarget = SUCCEEDED(device->GetRenderTarget(0, &restoreTarget)) && restoreTarget;
    const bool haveRestoreDepth = SUCCEEDED(device->GetDepthStencilSurface(&restoreDepth)) && restoreDepth;
    const bool haveRestoreViewport = SUCCEEDED(device->GetViewport(&restoreViewport));
    const bool haveRestoreScissor = SUCCEEDED(device->GetScissorRect(&restoreScissor));
    if (FAILED(createRestoreResult) || FAILED(captureRestoreResult) || !haveRestoreTarget)
    {
        if (restoreState)
            restoreState->Release();
        releaseSurface(restoreTarget);
        releaseSurface(restoreDepth);
        return false;
    }

    StereoSurfaceTwin* initializedTargets[MaxNativePostprocessDrawRecords] {};
    UINT initializedTargetCount = 0;
    LONG replayed = 0;
    LONG finalWrites = 0;
    bool allSucceeded = true;
    gInStereoReplay = true;
    for (LONG index = 0; index < recordCount; ++index)
    {
        NativePostprocessDrawRecord& record = gNativePostprocessDrawRecords[index];
        if (!record.stateBlock
            || !record.targetTwin
            || !record.targetTwin->right
            || FAILED(record.stateBlock->Apply()))
        {
            allSucceeded = false;
            break;
        }

        bool targetInitialized = false;
        for (UINT initialized = 0; initialized < initializedTargetCount; ++initialized)
        {
            if (initializedTargets[initialized] == record.targetTwin)
            {
                targetInitialized = true;
                break;
            }
        }
        if (!targetInitialized && initializedTargetCount < MaxNativePostprocessDrawRecords)
        {
            device->ColorFill(record.targetTwin->right, nullptr, 0);
            initializedTargets[initializedTargetCount++] = record.targetTwin;
        }

        const HRESULT targetResult = gRealSetRenderTarget(device, 0, record.targetTwin->right);
        IDirect3DSurface9* rightDepth = record.depthTwin ? record.depthTwin->right : nullptr;
        const HRESULT depthResult = gRealSetDepthStencilSurface(device, rightDepth);
        if (FAILED(targetResult) || FAILED(depthResult))
        {
            allSucceeded = false;
            break;
        }
        if (record.haveViewport)
            device->SetViewport(&record.viewport);
        if (record.haveScissor)
            device->SetScissorRect(&record.scissor);
        for (UINT sampler = 0; sampler < MaxTrackedSamplers; ++sampler)
        {
            IDirect3DBaseTexture9* texture = record.textures[sampler];
            IDirect3DBaseTexture9* rightTexture = stereoTextureFor(texture, false);
            if (rightTexture)
                texture = rightTexture;
            gRealSetTexture(device, sampler, texture);
        }

        HRESULT drawResult = D3DERR_INVALIDCALL;
        if (record.kind == NativePostprocessDrawKind::DrawIndexedPrimitive)
        {
            drawResult = gRealDrawIndexedPrimitive(
                device,
                record.primitiveType,
                record.baseVertexIndex,
                record.minVertexIndex,
                record.numVertices,
                record.startIndex,
                record.primitiveCount);
        }
        else
        {
            drawResult = gRealDrawPrimitive(
                device,
                record.primitiveType,
                record.startVertex,
                record.primitiveCount);
        }
        if (FAILED(drawResult))
        {
            allSucceeded = false;
            break;
        }
        ++replayed;
        if (record.targetTwin->right == gRightEyeSurface)
            ++finalWrites;
    }
    gInStereoReplay = false;

    restoreState->Apply();
    if (haveRestoreTarget)
        gRealSetRenderTarget(device, 0, restoreTarget);
    gRealSetDepthStencilSurface(device, haveRestoreDepth ? restoreDepth : nullptr);
    if (haveRestoreViewport)
        device->SetViewport(&restoreViewport);
    if (haveRestoreScissor)
        device->SetScissorRect(&restoreScissor);
    restoreState->Release();
    releaseSurface(restoreTarget);
    releaseSurface(restoreDepth);

    InterlockedExchange(&gNativePostprocessReplayedDrawCount, replayed);
    if (allSucceeded && replayed == recordCount && finalWrites > 0)
    {
        InterlockedExchange(&gNativePostprocessFanoutDrawsThisPresent, replayed);
        InterlockedExchange(&gNativePostprocessFinalWritesThisPresent, finalWrites);
    }

    const LONG pairSequence = nextNativeStereoPairSequence();
    const bool success = allSucceeded && replayed == recordCount && finalWrites > 0;
    if (!success
        || nativeStereoPairNeedsWarmupAudit(12)
        || fnvxr::shared::sequencedValueBits(pairSequence) % 120u == 0u)
    {
        char message[384] {};
        sprintf_s(
            message,
            "native postprocess command replay pair=%ld recorded=%ld replayed=%ld finalWrites=%ld success=%d",
            pairSequence,
            recordCount,
            replayed,
            finalWrites,
            success ? 1 : 0);
        logLine(message);
    }
    return success;
}

void auditNativeFullSizeTargetCandidates(IDirect3DDevice9* device);

bool currentDrawSamplesStereoTwin()
{
    if ((!gStereoReplayEnabled && !gNativePostprocessFanoutActive)
        || !readEnvBool("FNVXR_D3D9_STEREO_REPLAY_TEXTURE_COMPOSITES", true))
        return false;

    for (UINT sampler = 0; sampler < MaxTrackedSamplers; ++sampler)
    {
        if (!gActiveTextures[sampler])
            continue;
        if (stereoTextureFor(gActiveTextures[sampler], true)
            && stereoTextureFor(gActiveTextures[sampler], false))
        {
            return true;
        }
    }
    return false;
}

void bindStereoTextureTwins(IDirect3DDevice9* device, bool leftEye)
{
    if (!device || !gRealSetTexture)
        return;

    for (UINT sampler = 0; sampler < MaxTrackedSamplers; ++sampler)
    {
        IDirect3DBaseTexture9* stereoTexture = stereoTextureFor(gActiveTextures[sampler], leftEye);
        if (!stereoTexture)
            continue;
        gRealSetTexture(device, sampler, stereoTexture);
        const LONG logCount = InterlockedIncrement(&gLoggedStereoTextureTwinBound);
        if (logCount <= 32 || logCount % 500 == 0)
        {
            char message[224] {};
            sprintf_s(
                message,
                "stereo texture twin bound count=%ld frame=%ld sampler=%u eye=%s original=%p twin=%p",
                logCount,
                static_cast<LONG>(gPresentFrames),
                sampler,
                leftEye ? "left" : "right",
                reinterpret_cast<void*>(gActiveTextures[sampler]),
                reinterpret_cast<void*>(stereoTexture));
            logLine(message);
        }
    }
}

void restoreTrackedTextures(IDirect3DDevice9* device)
{
    if (!device || !gRealSetTexture)
        return;

    for (UINT sampler = 0; sampler < MaxTrackedSamplers; ++sampler)
        gRealSetTexture(device, sampler, gActiveTextures[sampler]);
}

void releaseWideWorldTargets()
{
    releaseSurface(gWideWorldReadback);
    releaseSurface(gWideWorldSurface);
    releaseSurface(gWideWorldDepth);
    releaseTexture(gWideWorldTexture);
    gWideWorldReplayDrawsThisPresent = 0;
    gWideWorldLastPrimeFrame = 0;
}

void releaseStereoTargets()
{
    releaseSurface(gNativeResolvedReadback[0]);
    releaseSurface(gNativeResolvedReadback[1]);
    gNativeResolvedTwinForEye[0] = nullptr;
    gNativeResolvedTwinForEye[1] = nullptr;
    gNativeResolvedDesc = {};
    InterlockedExchange(&gNativeResolvedCopies[0], 0);
    InterlockedExchange(&gNativeResolvedCopies[1], 0);
    releaseSurface(gLeftEyeReadback);
    releaseSurface(gRightEyeReadback);
    releaseSurface(gBestLeftEyeReadback);
    releaseSurface(gBestRightEyeReadback);
    releaseSurface(gProbeLeftEyeReadback);
    releaseSurface(gProbeRightEyeReadback);
    if (!gStereoTargetsAliasTwin)
    {
        releaseSurface(gLeftEyeSurface);
        releaseSurface(gRightEyeSurface);
    }
    else
    {
        gLeftEyeSurface = nullptr;
        gRightEyeSurface = nullptr;
    }
    releaseSurface(gLeftEyeDepth);
    releaseSurface(gRightEyeDepth);
    releaseTexture(gLeftEyeTexture);
    releaseTexture(gRightEyeTexture);
    releaseStereoSurfaceTwins();
    gStereoTargetsAliasTwin = false;
    gStereoTargetWidth = 0;
    gStereoTargetHeight = 0;
    gStereoTargetFormat = D3DFMT_UNKNOWN;
    gProbeStereoTargetWidth = 0;
    gProbeStereoTargetHeight = 0;
    gProbeStereoTargetFormat = D3DFMT_UNKNOWN;
    gBestStereoDiffThisPresent = 0;
    gBestStereoSamplesThisPresent = 0;
    gBestStereoLeftHashThisPresent = 0;
    gBestStereoRightHashThisPresent = 0;
    gHaveBestStereoSnapshotThisPresent = false;
    gIsMain3DSceneActive = false;
    gStereoFrameReadyToPublish = false;
    gMain3DSceneRenderTarget = nullptr;
    releaseWideWorldTargets();
}

bool createEyeTarget(
    IDirect3DDevice9* device,
    UINT width,
    UINT height,
    D3DFORMAT format,
    IDirect3DTexture9*& texture,
    IDirect3DSurface9*& surface,
    IDirect3DSurface9*& depth)
{
    if (FAILED(device->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, format, D3DPOOL_DEFAULT, &texture, nullptr)))
        return false;

    if (FAILED(texture->GetSurfaceLevel(0, &surface)))
        return false;

    if (FAILED(device->CreateDepthStencilSurface(
            width, height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, &depth, nullptr)))
    {
        return false;
    }

    return true;
}

bool formatHasStencil(D3DFORMAT format)
{
    return format == D3DFMT_D15S1
        || format == D3DFMT_D24S8
        || format == D3DFMT_D24X4S4
        || format == D3DFMT_D24FS8;
}

bool depthSurfaceIsProofNonLockable(const D3DSURFACE_DESC& desc)
{
    const bool standardNonLockableFormat = desc.Format == D3DFMT_D32
        || desc.Format == D3DFMT_D15S1
        || desc.Format == D3DFMT_D24S8
        || desc.Format == D3DFMT_D24X8
        || desc.Format == D3DFMT_D24X4S4
        || desc.Format == D3DFMT_D16
        || desc.Format == D3DFMT_D24FS8;
    return standardNonLockableFormat
        && desc.Type == D3DRTYPE_SURFACE
        && desc.Pool == D3DPOOL_DEFAULT
        && (desc.Usage & D3DUSAGE_DEPTHSTENCIL) != 0;
}

bool primeStereoReplayTarget(
    IDirect3DDevice9* device,
    StereoSurfaceTwin* targetTwin,
    StereoSurfaceTwin* depthTwin,
    const char* context)
{
    if (!device
        || !targetTwin
        || !targetTwin->left
        || !targetTwin->right
        || !gRealClear
        || !gRealSetRenderTarget
        || !gRealSetDepthStencilSurface)
        return false;

    (void)context;
    // Never invent a black/color/depth prestate immediately before replay.
    // The only accepted initialization is the exact full clear already
    // mirrored by hookedClear for this transaction, on both the actual color
    // and depth twins. This makes a late depth rebind fail closed.
    return depthTwin
        && depthTwin->left
        && depthTwin->right
        && targetTwin->equivalentInitializationGeneration
            == gStereoReplayTransactionGeneration
        && targetTwin->leftValidGeneration == gStereoReplayTransactionGeneration
        && targetTwin->rightValidGeneration == gStereoReplayTransactionGeneration
        && depthTwin->equivalentInitializationGeneration
            == gStereoReplayTransactionGeneration
        && depthTwin->leftValidGeneration == gStereoReplayTransactionGeneration
        && depthTwin->rightValidGeneration == gStereoReplayTransactionGeneration;

#if 0
    // Obsolete pre-proof implementation retained temporarily for forensic
    // comparison. It must never compile or execute: it invented a black
    // prestate instead of preserving the application's exact clear.
    const LONG frame = static_cast<LONG>(gPresentFrames);
    if (targetTwin->lastDepthPrimeFrame == frame
        && targetTwin->lastDepthPrimeTwin == depthTwin)
        return true;

    if (!depthTwin || !depthTwin->left || !depthTwin->right)
        return false;
    DWORD clearFlags = D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER;
    if (formatHasStencil(depthTwin->desc.Format))
        clearFlags |= D3DCLEAR_STENCIL;

    IDirect3DSurface9* originalTarget = nullptr;
    IDirect3DSurface9* originalDepth = nullptr;
    const HRESULT getTargetResult = device->GetRenderTarget(0, &originalTarget);
    const HRESULT getDepthResult = device->GetDepthStencilSurface(&originalDepth);
    const bool hadOriginalTarget = originalTarget != nullptr;
    const bool hadOriginalDepth = originalDepth != nullptr;
    D3DVIEWPORT9 fullViewport {};
    fullViewport.Width = targetTwin->desc.Width;
    fullViewport.Height = targetTwin->desc.Height;
    fullViewport.MinZ = 0.0f;
    fullViewport.MaxZ = 1.0f;

    const HRESULT leftTargetResult = gRealSetRenderTarget
        ? gRealSetRenderTarget(device, 0, targetTwin->left)
        : D3DERR_INVALIDCALL;
    const HRESULT leftDepthResult = gRealSetDepthStencilSurface
        ? gRealSetDepthStencilSurface(device, depthTwin->left)
        : D3DERR_INVALIDCALL;
    const HRESULT leftViewportResult = device->SetViewport(&fullViewport);
    const HRESULT leftScissorStateResult = device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    const HRESULT leftResult = SUCCEEDED(leftTargetResult)
        && SUCCEEDED(leftDepthResult)
        && SUCCEEDED(leftViewportResult)
        && SUCCEEDED(leftScissorStateResult)
        ? gRealClear(device, 0, nullptr, clearFlags, 0x00000000, 1.0f, 0)
        : D3DERR_INVALIDCALL;

    const HRESULT rightTargetResult = gRealSetRenderTarget
        ? gRealSetRenderTarget(device, 0, targetTwin->right)
        : D3DERR_INVALIDCALL;
    const HRESULT rightDepthResult = gRealSetDepthStencilSurface
        ? gRealSetDepthStencilSurface(device, depthTwin->right)
        : D3DERR_INVALIDCALL;
    const HRESULT rightViewportResult = device->SetViewport(&fullViewport);
    const HRESULT rightScissorStateResult = device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    const HRESULT rightResult = SUCCEEDED(rightTargetResult)
        && SUCCEEDED(rightDepthResult)
        && SUCCEEDED(rightViewportResult)
        && SUCCEEDED(rightScissorStateResult)
        ? gRealClear(device, 0, nullptr, clearFlags, 0x00000000, 1.0f, 0)
        : D3DERR_INVALIDCALL;

    const HRESULT restoreTargetResult = originalTarget && gRealSetRenderTarget
        ? gRealSetRenderTarget(device, 0, originalTarget)
        : D3DERR_INVALIDCALL;
    const HRESULT restoreDepthResult = gRealSetDepthStencilSurface
        ? gRealSetDepthStencilSurface(device, originalDepth)
        : D3DERR_INVALIDCALL;
    const bool initialized = SUCCEEDED(getTargetResult)
        && SUCCEEDED(getDepthResult)
        && hadOriginalTarget
        && hadOriginalDepth
        && SUCCEEDED(leftResult)
        && SUCCEEDED(rightResult)
        && SUCCEEDED(restoreTargetResult)
        && SUCCEEDED(restoreDepthResult);
    releaseSurface(originalTarget);
    releaseSurface(originalDepth);
    if (initialized)
    {
        targetTwin->lastDepthPrimeFrame = frame;
        targetTwin->lastDepthPrimeTwin = depthTwin;
    }
    const LONG logCount = InterlockedIncrement(&gLoggedStereoReplayDepthPrime);
    const bool hammerLog =
        shouldTelemetryHammerLog(logCount, "FNVXR_D3D9_REPLAY_TARGET_TELEMETRY_STRIDE", 1);
    if (logCount <= 24 || logCount % 300 == 0 || FAILED(leftResult) || FAILED(rightResult) || hammerLog)
    {
        char message[320] {};
        sprintf_s(
            message,
            "stereo replay target primed count=%ld frame=%ld context=%s flags=0x%lx depth=%d left=0x%08lx right=0x%08lx size=%ux%u",
            logCount,
            frame,
            context ? context : "unknown",
            static_cast<unsigned long>(clearFlags),
            depthTwin ? 1 : 0,
            static_cast<unsigned long>(leftResult),
            static_cast<unsigned long>(rightResult),
            targetTwin->desc.Width,
            targetTwin->desc.Height);
        logLine(message);
    }
    if (hammerLog)
    {
        char event[448] {};
        sprintf_s(
            event,
            "{\"event\":\"fnvxrD3d9ReplayPrime\",\"frame\":%ld,\"count\":%ld,\"context\":\"%s\","
            "\"kind\":\"stereo\",\"flags\":\"0x%lx\",\"target\":%d,\"zbuffer\":%d,"
            "\"stencil\":%d,\"depthTwin\":%d,\"left\":\"0x%08lx\",\"right\":\"0x%08lx\","
            "\"width\":%u,\"height\":%u,\"format\":%lu}",
            frame,
            logCount,
            context ? context : "unknown",
            static_cast<unsigned long>(clearFlags),
            (clearFlags & D3DCLEAR_TARGET) ? 1 : 0,
            (clearFlags & D3DCLEAR_ZBUFFER) ? 1 : 0,
            (clearFlags & D3DCLEAR_STENCIL) ? 1 : 0,
            depthTwin ? 1 : 0,
            static_cast<unsigned long>(leftResult),
            static_cast<unsigned long>(rightResult),
            targetTwin->desc.Width,
            targetTwin->desc.Height,
            static_cast<unsigned long>(targetTwin->desc.Format));
        logLine(event);
    }
    return initialized;
#endif
}

void primeWideWorldReplayTarget(IDirect3DDevice9* device)
{
    if (!device || !gWideWorldSurface || !gRealClear)
        return;
    if (!readEnvBool("FNVXR_D3D9_STEREO_PRIME_REPLAY_TARGETS", true))
        return;

    const LONG frame = static_cast<LONG>(gPresentFrames);
    if (gWideWorldLastPrimeFrame == frame)
        return;

    DWORD clearFlags = readEnvBool("FNVXR_D3D9_STEREO_PRIME_COLOR", true) ? D3DCLEAR_TARGET : 0;
    if (gWideWorldDepth && readEnvBool("FNVXR_D3D9_STEREO_PRIME_DEPTH", true))
        clearFlags |= D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL;
    if (clearFlags == 0)
        return;

    IDirect3DSurface9* originalTarget = nullptr;
    IDirect3DSurface9* originalDepth = nullptr;
    device->GetRenderTarget(0, &originalTarget);
    device->GetDepthStencilSurface(&originalDepth);
    device->SetRenderTarget(0, gWideWorldSurface);
    device->SetDepthStencilSurface(gWideWorldDepth);
    const HRESULT result = gRealClear(device, 0, nullptr, clearFlags, 0x00000000, 1.0f, 0);
    device->SetRenderTarget(0, originalTarget);
    device->SetDepthStencilSurface(originalDepth);
    releaseSurface(originalTarget);
    releaseSurface(originalDepth);

    gWideWorldLastPrimeFrame = frame;
    const LONG logCount = InterlockedIncrement(&gLoggedStereoReplayDepthPrime);
    const bool hammerLog =
        shouldTelemetryHammerLog(logCount, "FNVXR_D3D9_REPLAY_TARGET_TELEMETRY_STRIDE", 1);
    if (logCount <= 24 || logCount % 300 == 0 || FAILED(result) || hammerLog)
    {
        char message[256] {};
        sprintf_s(
            message,
            "wide world replay target primed count=%ld frame=%ld flags=0x%lx result=0x%08lx size=%ux%u",
            logCount,
            frame,
            static_cast<unsigned long>(clearFlags),
            static_cast<unsigned long>(result),
            gStereoTargetWidth,
            gStereoTargetHeight);
        logLine(message);
    }
    if (hammerLog)
    {
        char event[384] {};
        sprintf_s(
            event,
            "{\"event\":\"fnvxrD3d9ReplayPrime\",\"frame\":%ld,\"count\":%ld,"
            "\"kind\":\"wideWorld\",\"flags\":\"0x%lx\",\"target\":%d,\"zbuffer\":%d,"
            "\"stencil\":%d,\"result\":\"0x%08lx\",\"width\":%u,\"height\":%u}",
            frame,
            logCount,
            static_cast<unsigned long>(clearFlags),
            (clearFlags & D3DCLEAR_TARGET) ? 1 : 0,
            (clearFlags & D3DCLEAR_ZBUFFER) ? 1 : 0,
            (clearFlags & D3DCLEAR_STENCIL) ? 1 : 0,
            static_cast<unsigned long>(result),
            gStereoTargetWidth,
            gStereoTargetHeight);
        logLine(event);
    }
}

bool ensureStereoTargets(IDirect3DDevice9* device)
{
    if ((!gStereoReplayEnabled && !gNativeStereoEnabled) || !device)
        return false;

    IDirect3DSurface9* backBuffer = nullptr;
    if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)) || !backBuffer)
        return false;

    D3DSURFACE_DESC desc {};
    const HRESULT descResult = backBuffer->GetDesc(&desc);
    backBuffer->Release();
    if (FAILED(descResult))
        return false;

    if (gLeftEyeSurface
        && gRightEyeSurface
        && gStereoTargetsAliasTwin
        && gStereoTargetWidth == desc.Width
        && gStereoTargetHeight == desc.Height
        && gStereoTargetFormat == desc.Format)
    {
        return true;
    }

    releaseStereoTargets();

    if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)) || !backBuffer)
        return false;

    StereoSurfaceTwin* backbufferTwin = ensureSurfaceTwin(device, backBuffer, false);
    releaseSurface(backBuffer);
    if (!backbufferTwin || !backbufferTwin->left || !backbufferTwin->right)
    {
        releaseStereoTargets();
        logLine("failed to create stereo replay backbuffer twins");
        return false;
    }

    gLeftEyeSurface = backbufferTwin->left;
    gRightEyeSurface = backbufferTwin->right;
    gStereoTargetsAliasTwin = true;
    gStereoTargetWidth = desc.Width;
    gStereoTargetHeight = desc.Height;
    gStereoTargetFormat = desc.Format;

    char message[192] {};
    sprintf_s(
        message,
        "created stereo replay backbuffer twins %ux%u format=%u",
        gStereoTargetWidth,
        gStereoTargetHeight,
        static_cast<unsigned>(gStereoTargetFormat));
    logLine(message);
    return true;
}

// Fallout: New Vegas 1.4.0.525 retail render entry point used by the retained
// source-blocked D3D replay diagnostic. Per-draw mirroring cannot recover
// camera-dependent geometry omitted before D3D submission and is not the
// production VR architecture. See docs/architecture-v2.md.
constexpr std::uintptr_t FalloutDoRenderFrameAddress = 0x008706B0;
constexpr std::uintptr_t FalloutWorldSceneGraphAddress = 0x011DEB7C;
constexpr std::uintptr_t FalloutNiCameraSetViewFrustumAddress = 0x00A6FAF0;
constexpr std::uintptr_t FalloutNiCameraUpdateWorldToCameraAddress = 0x00A70BA0;
constexpr std::uintptr_t FalloutImageSpaceGeometrySubmitAddress = 0x00A74600;
constexpr std::uintptr_t FalloutPlayerCharacterAddress = 0x011DEA3C;
constexpr std::uintptr_t FalloutPlayerRetrieveRootNodeAddress = 0x00950BB0;
constexpr std::size_t NiAvObjectParentOffset = 0x18;
constexpr std::size_t NiAvObjectLocalTransformOffset = 0x34;
constexpr std::size_t NiAvObjectWorldTransformOffset = 0x68;
constexpr std::size_t NiCameraWorldToCameraOffset = 0x9C;
constexpr std::size_t NiCameraFrustumOffset = 0xDC;
constexpr std::size_t SceneGraphCameraOffset = 0xAC;

struct NativeVector3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct NativeMatrix3
{
    float m[3][3] {};
};

struct NativeNiTransform
{
    NativeMatrix3 rotation {};
    NativeVector3 translation {};
    float scale = 1.0f;
};
static_assert(sizeof(NativeNiTransform) == 0x34, "NiTransform layout mismatch");

struct NativeNiFrustum
{
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;
    float nearDistance = 0.0f;
    float farDistance = 0.0f;
    std::uint8_t orthographic = 0;
    std::uint8_t padding[3] {};
};
static_assert(sizeof(NativeNiFrustum) == 0x1C, "NiFrustum layout mismatch");

struct NativeCameraSnapshot
{
    void* camera = nullptr;
    NativeNiTransform local {};
    NativeNiTransform world {};
    float worldToCamera[4][4] {};
    NativeNiFrustum frustum {};
};

struct NativeStereoRig
{
    NativeMatrix3 headRotation {};
    NativeMatrix3 eyeRotation[2] {};
    NativeVector3 eyeOffsetUnits[2] {};
    float fov[2][4] {};
    SharedVrPoseState pose {};
    LONG poseSequence = 0;
    std::int64_t displayTime = 0;
};

NativeCameraSnapshot gNativeActiveCamera {};
NativeStereoRig gNativeActiveRig {};

using FalloutDoRenderFrameFn = void (__thiscall*)(void*, void*, std::uint8_t, std::uint8_t);
using NiCameraSetViewFrustumFn = void (__thiscall*)(void*, const NativeNiFrustum*);
using NiCameraUpdateWorldToCameraFn = void (__thiscall*)(void*);
using ImageSpaceGeometrySubmitFn = void (__thiscall*)(void*, void*);

NativeMatrix3 multiplyNativeMatrix3(const NativeMatrix3& a, const NativeMatrix3& b)
{
    NativeMatrix3 result {};
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

float determinantNativeMatrix3(const NativeMatrix3& matrix)
{
    return matrix.m[0][0] * (matrix.m[1][1] * matrix.m[2][2] - matrix.m[1][2] * matrix.m[2][1])
        - matrix.m[0][1] * (matrix.m[1][0] * matrix.m[2][2] - matrix.m[1][2] * matrix.m[2][0])
        + matrix.m[0][2] * (matrix.m[1][0] * matrix.m[2][1] - matrix.m[1][1] * matrix.m[2][0]);
}

float orthonormalErrorNativeMatrix3(const NativeMatrix3& matrix)
{
    float maxError = 0.0f;
    for (int columnA = 0; columnA < 3; ++columnA)
    {
        for (int columnB = 0; columnB < 3; ++columnB)
        {
            float dot = 0.0f;
            for (int row = 0; row < 3; ++row)
                dot += matrix.m[row][columnA] * matrix.m[row][columnB];
            const float expected = columnA == columnB ? 1.0f : 0.0f;
            maxError = (std::max)(maxError, absFloat(dot - expected));
        }
    }
    return maxError;
}

NativeMatrix3 transposeNativeMatrix3(const NativeMatrix3& matrix)
{
    NativeMatrix3 result {};
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 3; ++column)
            result.m[row][column] = matrix.m[column][row];
    }
    return result;
}

NativeMatrix3 gravityLevelNativeCameraRotation(const NativeMatrix3& matrix)
{
    fnvxr::stereo::Matrix3 cameraWorld {};
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            cameraWorld.m[row][column] = matrix.m[row][column];
    const fnvxr::stereo::Matrix3 leveled =
        fnvxr::stereo::gravityLevelCameraWorldRotation(cameraWorld);
    NativeMatrix3 result {};
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            result.m[row][column] = leveled.m[row][column];
    return result;
}

void captureNativeViewOrigin(const SharedVrPoseState& pose, const char* reason)
{
    // A VR recenter may choose heading, but it must not tilt the room basis.
    // Keeping pitch/roll here while translation uses the gravity-aligned pose
    // origin gives rotation and translation different frames: later world-up
    // yaw mixes into pitch/roll and horizontal motion acquires vertical error.
    const fnvxr::stereo::Quaternion yawOrigin =
        fnvxr::stereo::gravityAlignedYawOrientation(
            { pose.hmdRot[0], pose.hmdRot[1], pose.hmdRot[2], pose.hmdRot[3] },
            { gVrPoseOriginRot[0], gVrPoseOriginRot[1], gVrPoseOriginRot[2], gVrPoseOriginRot[3] });
    gNativeViewOriginRot[0] = yawOrigin.x;
    gNativeViewOriginRot[1] = yawOrigin.y;
    gNativeViewOriginRot[2] = yawOrigin.z;
    gNativeViewOriginRot[3] = yawOrigin.w;
    gNativeViewOriginValid = true;
    char message[256] {};
    sprintf_s(
        message,
        "native view recentered reason=%s gravityAligned=1 yawOriginRot=(%.6f %.6f %.6f %.6f)",
        reason ? reason : "unknown",
        gNativeViewOriginRot[0],
        gNativeViewOriginRot[1],
        gNativeViewOriginRot[2],
        gNativeViewOriginRot[3]);
    logLine(message);
}

NativeVector3 transformNativeVector(const NativeMatrix3& matrix, const NativeVector3& value)
{
    return {
        matrix.m[0][0] * value.x + matrix.m[0][1] * value.y + matrix.m[0][2] * value.z,
        matrix.m[1][0] * value.x + matrix.m[1][1] * value.y + matrix.m[1][2] * value.z,
        matrix.m[2][0] * value.x + matrix.m[2][1] * value.y + matrix.m[2][2] * value.z
    };
}

bool nativeTransformLooksUsable(const NativeNiTransform& transform)
{
    if (!std::isfinite(transform.translation.x)
        || !std::isfinite(transform.translation.y)
        || !std::isfinite(transform.translation.z)
        || !std::isfinite(transform.scale)
        || absFloat(transform.scale) < 0.0001f)
    {
        return false;
    }

    float maxAbs = 0.0f;
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 3; ++column)
        {
            if (!std::isfinite(transform.rotation.m[row][column]))
                return false;
            const float entryAbs = absFloat(transform.rotation.m[row][column]);
            if (entryAbs > maxAbs)
                maxAbs = entryAbs;
        }
    }
    return maxAbs > 0.01f && maxAbs < 4.0f;
}

bool readNativePlayerBodyRoot(
    std::uintptr_t& addressOut,
    float rotationOut[9],
    float positionOut[3],
    float& scaleOut)
{
    addressOut = 0;
    std::memset(rotationOut, 0, sizeof(float) * 9);
    std::memset(positionOut, 0, sizeof(float) * 3);
    scaleOut = 0.0f;
    __try
    {
        void* player = *reinterpret_cast<void**>(FalloutPlayerCharacterAddress);
        if (!player)
            return false;
        using RetrieveRootNodeFn = void* (__thiscall*)(void*, bool);
        void* bodyRoot = reinterpret_cast<RetrieveRootNodeFn>(
            FalloutPlayerRetrieveRootNodeAddress)(player, false);
        if (!bodyRoot)
            return false;
        NativeNiTransform world {};
        std::memcpy(
            &world,
            reinterpret_cast<void*>(
                reinterpret_cast<std::uintptr_t>(bodyRoot) + NiAvObjectWorldTransformOffset),
            sizeof(world));
        if (!nativeTransformLooksUsable(world))
            return false;
        addressOut = reinterpret_cast<std::uintptr_t>(bodyRoot);
        std::memcpy(rotationOut, &world.rotation.m[0][0], sizeof(float) * 9);
        positionOut[0] = world.translation.x;
        positionOut[1] = world.translation.y;
        positionOut[2] = world.translation.z;
        scaleOut = world.scale;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool readNativeWorldCamera(NativeCameraSnapshot& snapshot)
{
    snapshot = {};
    __try
    {
        void* sceneGraph = *reinterpret_cast<void**>(FalloutWorldSceneGraphAddress);
        if (!sceneGraph)
            return false;
        void* camera = *reinterpret_cast<void**>(
            reinterpret_cast<std::uintptr_t>(sceneGraph) + SceneGraphCameraOffset);
        if (!camera || !*reinterpret_cast<void***>(camera))
            return false;

        const auto address = reinterpret_cast<std::uintptr_t>(camera);
        snapshot.camera = camera;
        std::memcpy(&snapshot.local, reinterpret_cast<void*>(address + NiAvObjectLocalTransformOffset), sizeof(snapshot.local));
        std::memcpy(&snapshot.world, reinterpret_cast<void*>(address + NiAvObjectWorldTransformOffset), sizeof(snapshot.world));
        std::memcpy(
            snapshot.worldToCamera,
            reinterpret_cast<void*>(address + NiCameraWorldToCameraOffset),
            sizeof(snapshot.worldToCamera));
        std::memcpy(&snapshot.frustum, reinterpret_cast<void*>(address + NiCameraFrustumOffset), sizeof(snapshot.frustum));
        return nativeTransformLooksUsable(snapshot.local)
            && nativeTransformLooksUsable(snapshot.world)
            && finiteArray(&snapshot.worldToCamera[0][0], 16)
            && !snapshot.frustum.orthographic
            && std::isfinite(snapshot.frustum.left)
            && std::isfinite(snapshot.frustum.right)
            && std::isfinite(snapshot.frustum.top)
            && std::isfinite(snapshot.frustum.bottom)
            && std::isfinite(snapshot.frustum.nearDistance)
            && std::isfinite(snapshot.frustum.farDistance)
            && snapshot.frustum.left < snapshot.frustum.right
            && snapshot.frustum.bottom < snapshot.frustum.top
            && snapshot.frustum.nearDistance > 0.0f
            && snapshot.frustum.farDistance > snapshot.frustum.nearDistance;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        snapshot = {};
        return false;
    }
}

bool updateNativeCameraWorldToCamera(void* camera)
{
    if (!camera)
        return false;
    __try
    {
        // NiCamera's vtable slot at +0xB8 is not a parameterless
        // NiAVObject::UpdateTransform in FNV 1.4.0.525.  It takes a stack
        // argument and returns with `ret 4`; calling it as void(this) was the
        // reason every native eye failed before rendering.  We already derive
        // and write the exact local/world transforms for the eye, so invoke
        // NiCamera's retail world-to-camera rebuild directly.
        auto updateWorldToCamera = reinterpret_cast<NiCameraUpdateWorldToCameraFn>(
            gNativeCameraMatrixTrampoline
                ? gNativeCameraMatrixTrampoline
                : reinterpret_cast<void*>(FalloutNiCameraUpdateWorldToCameraAddress));
        updateWorldToCamera(camera);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool setNativeCameraFrustum(void* camera, const NativeNiFrustum& frustum)
{
    __try
    {
        auto setViewFrustum = reinterpret_cast<NiCameraSetViewFrustumFn>(FalloutNiCameraSetViewFrustumAddress);
        setViewFrustum(camera, &frustum);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

NativeNiTransform localTransformForDesiredWorld(void* camera, const NativeNiTransform& desiredWorld)
{
    NativeNiTransform local = desiredWorld;
    __try
    {
        void* parent = *reinterpret_cast<void**>(
            reinterpret_cast<std::uintptr_t>(camera) + NiAvObjectParentOffset);
        if (!parent)
            return local;

        NativeNiTransform parentWorld {};
        std::memcpy(
            &parentWorld,
            reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(parent) + NiAvObjectWorldTransformOffset),
            sizeof(parentWorld));
        if (!nativeTransformLooksUsable(parentWorld))
            return local;

        const NativeMatrix3 inverseParentRotation = transposeNativeMatrix3(parentWorld.rotation);
        local.rotation = multiplyNativeMatrix3(inverseParentRotation, desiredWorld.rotation);
        const NativeVector3 relative {
            desiredWorld.translation.x - parentWorld.translation.x,
            desiredWorld.translation.y - parentWorld.translation.y,
            desiredWorld.translation.z - parentWorld.translation.z
        };
        local.translation = transformNativeVector(inverseParentRotation, relative);
        local.translation.x /= parentWorld.scale;
        local.translation.y /= parentWorld.scale;
        local.translation.z /= parentWorld.scale;
        local.scale = desiredWorld.scale / parentWorld.scale;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return desiredWorld;
    }
    return local;
}

bool prepareNativeStereoRig(NativeStereoRig& rig)
{
    rig = {};
    updateSharedVrPose();
    if (readEnvBool("FNVXR_D3D9_NATIVE_RECENTER_ON_FIRST_GAMEPLAY", true)
        && !gNativeGameplayPoseOriginLatched
        && gLatestVrPoseSnapshotValid)
    {
        // The pose stream starts while Fallout is still in its menus.  Latch
        // again at the first accepted gameplay render so the retail camera and
        // the user's forward direction share the same body-space origin.
        gHaveVrPoseOrigin = false;
        gHaveVrBodyFrame = false;
        updateSharedVrPose();
        if (gHaveVrPoseOrigin && gHaveVrBodyFrame)
        {
            gNativeGameplayPoseOriginLatched = true;
            captureNativeViewOrigin(gLatestVrPoseSnapshot, "first-gameplay");
            logLine("native stereo gameplay pose origin latched");
        }
    }
    if ((!gNativeStereoEnabled && !gNativeSingleTraversalReplayEnabled)
        || !gHaveVrBodyFrame
        || !gLatestVrPoseSnapshotValid
        || !fnvxr::shared::sequencedValueIsPublished(gLatestVrPoseSnapshotSequence)
        || gLatestVrPoseDisplayTime <= 0)
    {
        return false;
    }

    rig.pose = gLatestVrPoseSnapshot;
    rig.poseSequence = gLatestVrPoseSnapshotSequence;
    rig.displayTime = gLatestVrPoseDisplayTime;

    const std::uint32_t recenterRequestId = rig.pose.recenterRequestId;
    if (recenterRequestId != 0
        && recenterRequestId != gNativeLastRecenterRequestId)
    {
        // Recenter is one rigid-frame transaction. Relatch position and the
        // gravity-aligned yaw from this same pose before deriving rotation;
        // changing only the rotation origin leaves translation expressed in
        // the previous heading frame and violates 6DoF composition.
        gHaveVrPoseOrigin = false;
        gHaveVrBodyFrame = false;
        gNativeViewOriginValid = false;
        updateSharedVrPose();
        if (!gHaveVrPoseOrigin || !gHaveVrBodyFrame || !gLatestVrPoseSnapshotValid)
            return false;
        rig.pose = gLatestVrPoseSnapshot;
        rig.poseSequence = gLatestVrPoseSnapshotSequence;
        rig.displayTime = gLatestVrPoseDisplayTime;
        captureNativeViewOrigin(rig.pose, "controller-chord-rigid-frame");
        gNativeLastRecenterRequestId = rig.pose.recenterRequestId;
    }

    const fnvxr::stereo::Quaternion currentHead {
        rig.pose.hmdRot[0], rig.pose.hmdRot[1], rig.pose.hmdRot[2], rig.pose.hmdRot[3]
    };
    const fnvxr::stereo::Quaternion relativeHead = gNativeViewOriginValid
        ? fnvxr::stereo::relativeOrientation(
            { gNativeViewOriginRot[0], gNativeViewOriginRot[1], gNativeViewOriginRot[2], gNativeViewOriginRot[3] },
            currentHead)
        : fnvxr::stereo::Quaternion {
            gLatestHmdRotDelta[0],
            gLatestHmdRotDelta[1],
            gLatestHmdRotDelta[2],
            gLatestHmdRotDelta[3]
        };
    const fnvxr::stereo::Matrix3 nativeHeadRotation =
        fnvxr::stereo::cameraLocalHeadRotation(relativeHead);
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 3; ++column)
        {
            rig.headRotation.m[row][column] = nativeHeadRotation.m[row][column];
        }
    }

    const float* localEyes[2] { gLatestLeftEyeLocalMeters, gLatestRightEyeLocalMeters };
    const float* sourceEyeRotations[2] { rig.pose.leftEyeRot, rig.pose.rightEyeRot };
    const float* sourceFov[2] { rig.pose.leftFov, rig.pose.rightFov };
    for (int eye = 0; eye < 2; ++eye)
    {
        const fnvxr::stereo::Quaternion relativeEye = fnvxr::stereo::relativeOrientation(
            gNativeViewOriginValid
                ? fnvxr::stereo::Quaternion {
                    gNativeViewOriginRot[0],
                    gNativeViewOriginRot[1],
                    gNativeViewOriginRot[2],
                    gNativeViewOriginRot[3]
                }
                : fnvxr::stereo::Quaternion {
                    gVrPoseOriginRot[0],
                    gVrPoseOriginRot[1],
                    gVrPoseOriginRot[2],
                    gVrPoseOriginRot[3]
                },
            {
                sourceEyeRotations[eye][0],
                sourceEyeRotations[eye][1],
                sourceEyeRotations[eye][2],
                sourceEyeRotations[eye][3]
            });
        const fnvxr::stereo::Matrix3 nativeEyeRotation =
            fnvxr::stereo::cameraLocalHeadRotation(relativeEye);
        for (int row = 0; row < 3; ++row)
        {
            for (int column = 0; column < 3; ++column)
                rig.eyeRotation[eye].m[row][column] = nativeEyeRotation.m[row][column];
        }
        if (absFloat(determinantNativeMatrix3(rig.eyeRotation[eye]) - 1.0f) > 0.002f
            || orthonormalErrorNativeMatrix3(rig.eyeRotation[eye]) > 0.002f)
        {
            return false;
        }

        float localXrMeters[3] {};
        composeLocalXrMeters(localEyes[eye], localXrMeters);
        const fnvxr::stereo::Vector3 localCameraMeters =
            fnvxr::stereo::xrVectorToNiCameraLocal({
                localXrMeters[0], localXrMeters[1], localXrMeters[2] });
        rig.eyeOffsetUnits[eye] = {
            localCameraMeters.x * gGameUnitsPerMeter,
            localCameraMeters.y * gGameUnitsPerMeter,
            localCameraMeters.z * gGameUnitsPerMeter
        };
        std::memcpy(rig.fov[eye], sourceFov[eye], sizeof(rig.fov[eye]));
    }
    return true;
}

bool writeNativeEyeCameraState(const NativeCameraSnapshot& base, const NativeStereoRig& rig, int eye)
{
    if (!base.camera || eye < 0 || eye > 1)
        return false;

    NativeNiTransform desiredWorld = base.world;
    const NativeMatrix3 bodyWorldRotation = gravityLevelNativeCameraRotation(base.world.rotation);
    if (readEnvBool("FNVXR_D3D9_NATIVE_APPLY_HEAD_ROTATION", true))
    {
        // The exact per-eye OpenXR pose is body-local and belongs to the same
        // immutable transaction as this eye's pixels. Preserve the retail
        // camera/player basis and apply that eye delta on its local side.
        desiredWorld.rotation = multiplyNativeMatrix3(bodyWorldRotation, rig.eyeRotation[eye]);
    }
    else
        desiredWorld.rotation = bodyWorldRotation;

    const NativeVector3 worldOffset = transformNativeVector(bodyWorldRotation, rig.eyeOffsetUnits[eye]);
    desiredWorld.translation.x += worldOffset.x;
    desiredWorld.translation.y += worldOffset.y;
    desiredWorld.translation.z += worldOffset.z;
    const NativeNiTransform desiredLocal = localTransformForDesiredWorld(base.camera, desiredWorld);

    NativeNiFrustum frustum = base.frustum;
    if (readEnvBool("FNVXR_D3D9_NATIVE_ASYMMETRIC_FOV", true))
    {
        frustum.left = std::tan(rig.fov[eye][0]);
        frustum.right = std::tan(rig.fov[eye][1]);
        frustum.top = std::tan(rig.fov[eye][2]);
        frustum.bottom = std::tan(rig.fov[eye][3]);
        frustum.orthographic = 0;
    }

    __try
    {
        const auto address = reinterpret_cast<std::uintptr_t>(base.camera);
        std::memcpy(
            reinterpret_cast<void*>(address + NiAvObjectLocalTransformOffset),
            &desiredLocal,
            sizeof(desiredLocal));
        std::memcpy(
            reinterpret_cast<void*>(address + NiAvObjectWorldTransformOffset),
            &desiredWorld,
            sizeof(desiredWorld));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }

    return setNativeCameraFrustum(base.camera, frustum);
}

NativeVector3 nativeCenterOffsetUnits(const NativeStereoRig& rig)
{
    return {
        (rig.eyeOffsetUnits[0].x + rig.eyeOffsetUnits[1].x) * 0.5f,
        (rig.eyeOffsetUnits[0].y + rig.eyeOffsetUnits[1].y) * 0.5f,
        (rig.eyeOffsetUnits[0].z + rig.eyeOffsetUnits[1].z) * 0.5f
    };
}

NativeNiTransform nativeCenterWorldTransform(
    const NativeCameraSnapshot& base,
    const NativeStereoRig& rig)
{
    NativeNiTransform desiredWorld = base.world;
    const NativeMatrix3 bodyWorldRotation = gravityLevelNativeCameraRotation(base.world.rotation);
    if (readEnvBool("FNVXR_D3D9_NATIVE_APPLY_HEAD_ROTATION", true))
        desiredWorld.rotation = multiplyNativeMatrix3(bodyWorldRotation, rig.headRotation);
    else
        desiredWorld.rotation = bodyWorldRotation;

    const NativeVector3 centerOffsetUnits = nativeCenterOffsetUnits(rig);
    const NativeVector3 worldOffset = transformNativeVector(bodyWorldRotation, centerOffsetUnits);
    desiredWorld.translation.x += worldOffset.x;
    desiredWorld.translation.y += worldOffset.y;
    desiredWorld.translation.z += worldOffset.z;
    return desiredWorld;
}

bool writeNativeCenterCameraState(const NativeCameraSnapshot& base, const NativeStereoRig& rig)
{
    if (!base.camera)
        return false;

    // Both eye offsets contain the recentered head translation plus their
    // individual eye displacement. Their midpoint is the physical head/eye
    // center used for the one conservative Gamebryo traversal.
    const NativeNiTransform desiredWorld = nativeCenterWorldTransform(base, rig);
    const NativeNiTransform desiredLocal = localTransformForDesiredWorld(base.camera, desiredWorld);

    NativeNiFrustum frustum = base.frustum;
    if (readEnvBool("FNVXR_D3D9_NATIVE_ASYMMETRIC_FOV", true))
    {
        const NativeVector3 centerOffsetUnits = nativeCenterOffsetUnits(rig);
        fnvxr::stereo::Matrix3 centerRotation {};
        fnvxr::stereo::Vector3 centerPosition {
            centerOffsetUnits.x, centerOffsetUnits.y, centerOffsetUnits.z
        };
        fnvxr::stereo::EyeCullFrustum eyeFrusta[2] {};
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 3; ++column)
                centerRotation.m[row][column] = rig.headRotation.m[row][column];
        for (int eye = 0; eye < 2; ++eye)
        {
            for (int row = 0; row < 3; ++row)
                for (int column = 0; column < 3; ++column)
                    eyeFrusta[eye].rotation.m[row][column] = rig.eyeRotation[eye].m[row][column];
            eyeFrusta[eye].position = {
                rig.eyeOffsetUnits[eye].x,
                rig.eyeOffsetUnits[eye].y,
                rig.eyeOffsetUnits[eye].z
            };
            eyeFrusta[eye].left = std::tan(rig.fov[eye][0]);
            eyeFrusta[eye].right = std::tan(rig.fov[eye][1]);
            eyeFrusta[eye].top = std::tan(rig.fov[eye][2]);
            eyeFrusta[eye].bottom = std::tan(rig.fov[eye][3]);
        }
        const fnvxr::stereo::PerspectiveCullFrustum unionFrustum =
            fnvxr::stereo::conservativeCenterCullFrustum(
                centerRotation,
                centerPosition,
                eyeFrusta,
                base.frustum.nearDistance,
                base.frustum.farDistance);
        if (!unionFrustum.valid)
            return false;
        frustum.left = unionFrustum.left;
        frustum.right = unionFrustum.right;
        frustum.top = unionFrustum.top;
        frustum.bottom = unionFrustum.bottom;
        frustum.nearDistance = unionFrustum.nearDistance;
        frustum.farDistance = unionFrustum.farDistance;
        frustum.orthographic = 0;
    }

    __try
    {
        const auto address = reinterpret_cast<std::uintptr_t>(base.camera);
        std::memcpy(
            reinterpret_cast<void*>(address + NiAvObjectLocalTransformOffset),
            &desiredLocal,
            sizeof(desiredLocal));
        std::memcpy(
            reinterpret_cast<void*>(address + NiAvObjectWorldTransformOffset),
            &desiredWorld,
            sizeof(desiredWorld));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    return setNativeCameraFrustum(base.camera, frustum);
}

bool applyNativeCenterCamera(const NativeCameraSnapshot& base, const NativeStereoRig& rig)
{
    return writeNativeCenterCameraState(base, rig)
        && updateNativeCameraWorldToCamera(base.camera);
}

bool applyNativeEyeCamera(const NativeCameraSnapshot& base, const NativeStereoRig& rig, int eye)
{
    // The camera matrix rebuild consumes both the world transform and the
    // frustum, so commit the complete eye state first and rebuild last.
    return writeNativeEyeCameraState(base, rig, eye)
        && updateNativeCameraWorldToCamera(base.camera);
}

fnvxr::stereo::Matrix4 nativeWorldToClipMatrix(const NativeCameraSnapshot& camera)
{
    fnvxr::stereo::Matrix4 result {};
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column)
            result.m[row][column] = camera.worldToCamera[row][column];
    return result;
}

// Build the exact center and per-eye NiCamera world-to-clip factors before
// the one Gamebryo traversal starts.  Fallout's programmable world shaders do
// not consume the stale fixed-function VIEW matrix seen by the D3D9 hook; they
// consume ModelViewProj constants derived from NiCamera::worldToCamera.
bool prepareNativeSingleTraversalViewProjections(
    const NativeCameraSnapshot& base,
    const NativeStereoRig& rig,
    NativeCameraSnapshot& centerCamera)
{
    gNativeSingleTraversalViewProjectionValid = false;
    gNativeSingleTraversalCenterViewProjection = {};
    gNativeSingleTraversalEyeViewProjection[0] = {};
    gNativeSingleTraversalEyeViewProjection[1] = {};

    NativeCameraSnapshot initialCenter {};
    if (!applyNativeCenterCamera(base, rig) || !readNativeWorldCamera(initialCenter))
        return false;

    NativeCameraSnapshot eyeCamera[2] {};
    for (int eye = 0; eye < 2; ++eye)
    {
        if (!applyNativeEyeCamera(base, rig, eye) || !readNativeWorldCamera(eyeCamera[eye]))
            return false;
    }

    // Leave the live camera centered for the only Gamebryo traversal and
    // capture that final factor, not an assumed reconstruction of it.
    if (!applyNativeCenterCamera(base, rig) || !readNativeWorldCamera(centerCamera))
        return false;

    const fnvxr::stereo::Matrix4 firstCenter = nativeWorldToClipMatrix(initialCenter);
    const fnvxr::stereo::Matrix4 finalCenter = nativeWorldToClipMatrix(centerCamera);
    if (matrixMaxAbsDifference(firstCenter, finalCenter) > 0.0001f)
        return false;

    gNativeSingleTraversalCenterViewProjection = finalCenter;
    gNativeSingleTraversalEyeViewProjection[0] = nativeWorldToClipMatrix(eyeCamera[0]);
    gNativeSingleTraversalEyeViewProjection[1] = nativeWorldToClipMatrix(eyeCamera[1]);
    gNativeSingleTraversalViewProjectionValid = true;
    return true;
}

void __fastcall hookedNativeCameraUpdateWorldToCamera(void* camera, void*)
{
    auto original = reinterpret_cast<NiCameraUpdateWorldToCameraFn>(gNativeCameraMatrixTrampoline);
    const LONG eye = gNativeActiveEye;
    if (gInNativeStereoHook
        && eye >= 0
        && eye <= 2
        && camera
        && camera == gNativeActiveCamera.camera)
    {
        const bool written = eye == 2
            ? writeNativeCenterCameraState(gNativeActiveCamera, gNativeActiveRig)
            : writeNativeEyeCameraState(gNativeActiveCamera, gNativeActiveRig, static_cast<int>(eye));
        if (written)
        {
            InterlockedIncrement(&gNativeCameraMatrixOverrides);
            if (eye <= 1)
                InterlockedIncrement(&gNativeCameraMatrixOverridesThisPair[eye]);
        }
        else
        {
            const LONG failure = InterlockedIncrement(&gLoggedNativeStereoFailure);
            if (failure <= 16 || failure % 120 == 0)
                logLine("native stereo failed to enforce eye at NiCamera matrix rebuild");
        }
    }
    if (original)
        original(camera);
}

void __fastcall hookedNativeImageSpaceGeometrySubmit(void* renderer, void*, void* geometry)
{
    auto original = reinterpret_cast<ImageSpaceGeometrySubmitFn>(gNativeGeometrySubmitTrampoline);
    bool rearmed = false;
    std::uint32_t submitMode = 0;
    std::uint8_t active = 0;
    const LONG eye = gNativeActiveEye;
    if (original
        && geometry
        && gInNativeStereoHook
        && eye == 1
        && gNativePipelineStereoSamplerMask != 0
        && readEnvBool("FNVXR_D3D9_NATIVE_REARM_IMAGE_SPACE", false))
    {
        __try
        {
            auto* bytes = static_cast<std::uint8_t*>(geometry);
            std::memcpy(&submitMode, bytes + 0x200, sizeof(submitMode));
            active = bytes[0x208];
            if ((submitMode == 1 || submitMode == 2) && active != 1)
            {
                bytes[0x208] = 1;
                rearmed = true;
                InterlockedIncrement(&gNativeGeometryRearmsThisPair[eye]);
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }

    if (original)
        original(renderer, geometry);

    if (rearmed)
    {
        const LONG logCount = InterlockedIncrement(&gLoggedNativeGeometryRearm);
        if (logCount <= 32 || logCount % 240 == 0)
        {
            std::uint8_t activeAfter = 0xff;
            __try
            {
                activeAfter = static_cast<std::uint8_t*>(geometry)[0x208];
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
            char message[384] {};
            sprintf_s(
                message,
                "native image-space geometry rearmed count=%ld pair=%ld eye=right geometry=%p mode=%lu active=%u->%u samplerMask=0x%04lx",
                logCount,
                nextNativeStereoPairSequence(),
                geometry,
                static_cast<unsigned long>(submitMode),
                static_cast<unsigned>(active),
                static_cast<unsigned>(activeAfter),
                static_cast<unsigned long>(gNativePipelineStereoSamplerMask));
            logLine(message);
        }
    }
}

void restoreNativeCamera(const NativeCameraSnapshot& snapshot)
{
    if (!snapshot.camera)
        return;
    __try
    {
        const auto address = reinterpret_cast<std::uintptr_t>(snapshot.camera);
        std::memcpy(
            reinterpret_cast<void*>(address + NiAvObjectLocalTransformOffset),
            &snapshot.local,
            sizeof(snapshot.local));
        std::memcpy(
            reinterpret_cast<void*>(address + NiAvObjectWorldTransformOffset),
            &snapshot.world,
            sizeof(snapshot.world));
        std::memcpy(
            reinterpret_cast<void*>(address + NiCameraWorldToCameraOffset),
            snapshot.worldToCamera,
            sizeof(snapshot.worldToCamera));
        std::memcpy(
            reinterpret_cast<void*>(address + NiCameraFrustumOffset),
            &snapshot.frustum,
            sizeof(snapshot.frustum));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        const LONG failure = InterlockedIncrement(&gLoggedNativeStereoFailure);
        if (failure <= 8 || failure % 120 == 0)
            logLine("native stereo camera restore raised an exception");
    }
}

bool captureNativeStereoEye(IDirect3DDevice9* device, int eye)
{
    if (!device || !gRealStretchRect || eye < 0 || eye > 1 || !ensureStereoTargets(device))
        return false;

    IDirect3DSurface9* backBuffer = nullptr;
    if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)) || !backBuffer)
        return false;
    IDirect3DSurface9* destination = eye == 0 ? gLeftEyeSurface : gRightEyeSurface;
    const HRESULT result = destination
        ? gRealStretchRect(device, backBuffer, nullptr, destination, nullptr, D3DTEXF_NONE)
        : D3DERR_INVALIDCALL;
    releaseSurface(backBuffer);
    if (FAILED(result))
    {
        const LONG failure = InterlockedIncrement(&gLoggedNativeStereoFailure);
        if (failure <= 16 || failure % 120 == 0)
        {
            char message[256] {};
            sprintf_s(
                message,
                "native stereo eye capture failed count=%ld eye=%s result=0x%08lx",
                failure,
                eye == 0 ? "left" : "right",
                static_cast<unsigned long>(result));
            logLine(message);
        }
        return false;
    }
    return true;
}

void callOriginalDoRenderFrame(
    void* osGlobals,
    void* renderer,
    std::uint8_t arg1,
    std::uint8_t arg2)
{
    auto original = reinterpret_cast<FalloutDoRenderFrameFn>(gNativeStereoTrampoline);
    if (original)
    {
        LARGE_INTEGER begin {};
        LARGE_INTEGER end {};
        QueryPerformanceCounter(&begin);
        original(osGlobals, renderer, arg1, arg2);
        QueryPerformanceCounter(&end);
        ++gOriginalDoRenderCallsThisPresent;
        gOriginalDoRenderTicksThisPresent += end.QuadPart - begin.QuadPart;
    }
}

float nativeFloatArrayMaxDelta(const float* a, const float* b, std::size_t count)
{
    if (!a || !b)
        return std::numeric_limits<float>::infinity();
    float maximum = 0.0f;
    for (std::size_t index = 0; index < count; ++index)
    {
        if (!std::isfinite(a[index]) || !std::isfinite(b[index]))
            return std::numeric_limits<float>::infinity();
        const float delta = absFloat(a[index] - b[index]);
        if (!std::isfinite(delta))
            return std::numeric_limits<float>::infinity();
        if (delta > maximum)
            maximum = delta;
    }
    return maximum;
}

float nativeMatrix3Determinant(const NativeMatrix3& matrix)
{
    return matrix.m[0][0] * (matrix.m[1][1] * matrix.m[2][2] - matrix.m[1][2] * matrix.m[2][1])
        - matrix.m[0][1] * (matrix.m[1][0] * matrix.m[2][2] - matrix.m[1][2] * matrix.m[2][0])
        + matrix.m[0][2] * (matrix.m[1][0] * matrix.m[2][1] - matrix.m[1][1] * matrix.m[2][0]);
}

void logNativeCameraBasisIfChanged(
    const NativeStereoRig& rig,
    const NativeCameraSnapshot& base,
    const NativeCameraSnapshot& afterLeft,
    bool afterLeftValid,
    const NativeCameraSnapshot& afterRight,
    bool afterRightValid)
{
    static bool haveLastHeadRotation = false;
    static NativeMatrix3 lastHeadRotation {};
    const float change = haveLastHeadRotation
        ? nativeFloatArrayMaxDelta(
            &lastHeadRotation.m[0][0],
            &rig.headRotation.m[0][0],
            9)
        : 1.0f;
    if (change < 0.0005f)
        return;

    lastHeadRotation = rig.headRotation;
    haveLastHeadRotation = true;
    NativeMatrix3 leftRelative {};
    NativeMatrix3 rightRelative {};
    const bool haveRelative = afterLeftValid && afterRightValid;
    if (haveRelative)
    {
        const NativeMatrix3 inverseBase = transposeNativeMatrix3(base.world.rotation);
        leftRelative = multiplyNativeMatrix3(inverseBase, afterLeft.world.rotation);
        rightRelative = multiplyNativeMatrix3(inverseBase, afterRight.world.rotation);
    }
    const float headLeftDelta = haveRelative
        ? nativeFloatArrayMaxDelta(&rig.eyeRotation[0].m[0][0], &leftRelative.m[0][0], 9)
        : 0.0f;
    const float headRightDelta = haveRelative
        ? nativeFloatArrayMaxDelta(&rig.eyeRotation[1].m[0][0], &rightRelative.m[0][0], 9)
        : 0.0f;
    const float leftRightDelta = haveRelative
        ? nativeFloatArrayMaxDelta(&leftRelative.m[0][0], &rightRelative.m[0][0], 9)
        : 0.0f;
    char message[1792] {};
    sprintf_s(
        message,
        "{\"event\":\"fnvxrNativeCameraBasis\",\"poseSeq\":%ld,\"cameraValid\":%s,\"headDet\":%.6f,\"leftDet\":%.6f,\"rightDet\":%.6f,\"headLeftDelta\":%.6f,\"headRightDelta\":%.6f,\"leftRightDelta\":%.6f,\"hmdRotDelta\":[%.6f,%.6f,%.6f,%.6f],\"head\":[[%.6f,%.6f,%.6f],[%.6f,%.6f,%.6f],[%.6f,%.6f,%.6f]],\"leftRelative\":[[%.6f,%.6f,%.6f],[%.6f,%.6f,%.6f],[%.6f,%.6f,%.6f]],\"rightRelative\":[[%.6f,%.6f,%.6f],[%.6f,%.6f,%.6f],[%.6f,%.6f,%.6f]]}",
        rig.poseSequence,
        haveRelative ? "true" : "false",
        nativeMatrix3Determinant(rig.headRotation),
        haveRelative ? nativeMatrix3Determinant(leftRelative) : 0.0f,
        haveRelative ? nativeMatrix3Determinant(rightRelative) : 0.0f,
        headLeftDelta,
        headRightDelta,
        leftRightDelta,
        gLatestHmdRotDelta[0],
        gLatestHmdRotDelta[1],
        gLatestHmdRotDelta[2],
        gLatestHmdRotDelta[3],
        rig.headRotation.m[0][0], rig.headRotation.m[0][1], rig.headRotation.m[0][2],
        rig.headRotation.m[1][0], rig.headRotation.m[1][1], rig.headRotation.m[1][2],
        rig.headRotation.m[2][0], rig.headRotation.m[2][1], rig.headRotation.m[2][2],
        leftRelative.m[0][0], leftRelative.m[0][1], leftRelative.m[0][2],
        leftRelative.m[1][0], leftRelative.m[1][1], leftRelative.m[1][2],
        leftRelative.m[2][0], leftRelative.m[2][1], leftRelative.m[2][2],
        rightRelative.m[0][0], rightRelative.m[0][1], rightRelative.m[0][2],
        rightRelative.m[1][0], rightRelative.m[1][1], rightRelative.m[1][2],
        rightRelative.m[2][0], rightRelative.m[2][1], rightRelative.m[2][2]);
    logLine(message);
}

struct NativePassEquivalence
{
    bool guardEnabled = true;
    bool equivalent = false;
    bool underfilled = false;
    bool resolvedTargetMismatch = false;
    bool drawMismatch = false;
    bool vsCallMismatch = false;
    LONG draws[2] {};
    LONG vsCalls[2] {};
    LONG resolvedCopies[2] {};
    LONG fullSizeCandidates = 0;
    LONG candidateSeenAsymmetry = 0;
    LONG candidateSnapshotAsymmetry = 0;
    LONG drawDelta = 0;
    LONG vsCallDelta = 0;
    LONG minEyeDraws = 0;
    LONG minEyeVsCalls = 0;
    LONG maxDrawDelta = 0;
    LONG maxVsCallDelta = 0;
    LONG recoveryPairs = 1;
    float drawDeltaRatio = 0.0f;
    float vsCallDeltaRatio = 0.0f;
    float maxDrawDeltaRatio = 0.0f;
    float maxVsCallDeltaRatio = 0.0f;
};

LONG nativeCountDelta(LONG left, LONG right)
{
    return left >= right ? left - right : right - left;
}

float nativeCountDeltaRatio(LONG left, LONG right)
{
    const LONG maximum = (std::max)(left, right);
    return maximum > 0
        ? static_cast<float>(nativeCountDelta(left, right)) / static_cast<float>(maximum)
        : 0.0f;
}

NativePassEquivalence evaluateNativePassEquivalence()
{
    NativePassEquivalence result {};
    result.guardEnabled = readEnvBool("FNVXR_D3D9_NATIVE_EQUIVALENCE_GUARD", true);
    result.draws[0] = gNativeDrawCalls[0];
    result.draws[1] = gNativeDrawCalls[1];
    result.vsCalls[0] = gNativeVsConstantCalls[0];
    result.vsCalls[1] = gNativeVsConstantCalls[1];
    result.resolvedCopies[0] = gNativeResolvedCopies[0];
    result.resolvedCopies[1] = gNativeResolvedCopies[1];
    for (UINT index = 0; index < MaxNativeFullSizeTargetCandidates; ++index)
    {
        const NativeFullSizeTargetCandidate& candidate = gNativeFullSizeTargetCandidates[index];
        if (!candidate.original)
            continue;
        ++result.fullSizeCandidates;
        if (candidate.seen[0] != candidate.seen[1])
            ++result.candidateSeenAsymmetry;
        if (candidate.snapshotted[0] != candidate.snapshotted[1])
            ++result.candidateSnapshotAsymmetry;
    }
    result.drawDelta = nativeCountDelta(result.draws[0], result.draws[1]);
    result.vsCallDelta = nativeCountDelta(result.vsCalls[0], result.vsCalls[1]);
    result.drawDeltaRatio = nativeCountDeltaRatio(result.draws[0], result.draws[1]);
    result.vsCallDeltaRatio = nativeCountDeltaRatio(result.vsCalls[0], result.vsCalls[1]);
    result.minEyeDraws = static_cast<LONG>((std::max)(
        0.0f,
        readEnvFloat("FNVXR_D3D9_NATIVE_EQUIVALENCE_MIN_EYE_DRAWS", 64.0f)));
    result.minEyeVsCalls = static_cast<LONG>((std::max)(
        0.0f,
        readEnvFloat("FNVXR_D3D9_NATIVE_EQUIVALENCE_MIN_EYE_VS_CALLS", 64.0f)));
    result.maxDrawDelta = static_cast<LONG>((std::max)(
        0.0f,
        readEnvFloat("FNVXR_D3D9_NATIVE_EQUIVALENCE_MAX_DRAW_DELTA", 96.0f)));
    result.maxVsCallDelta = static_cast<LONG>((std::max)(
        0.0f,
        readEnvFloat("FNVXR_D3D9_NATIVE_EQUIVALENCE_MAX_VS_CALL_DELTA", 192.0f)));
    result.maxDrawDeltaRatio = (std::max)(
        0.0f,
        readEnvFloat("FNVXR_D3D9_NATIVE_EQUIVALENCE_MAX_DRAW_RATIO", 0.20f));
    result.maxVsCallDeltaRatio = (std::max)(
        0.0f,
        readEnvFloat("FNVXR_D3D9_NATIVE_EQUIVALENCE_MAX_VS_CALL_RATIO", 0.20f));
    result.recoveryPairs = static_cast<LONG>((std::max)(
        1.0f,
        readEnvFloat("FNVXR_D3D9_NATIVE_EQUIVALENCE_RECOVERY_PAIRS", 3.0f)));

    result.underfilled = (std::min)(result.draws[0], result.draws[1]) < result.minEyeDraws
        || (std::min)(result.vsCalls[0], result.vsCalls[1]) < result.minEyeVsCalls;
    result.resolvedTargetMismatch = readEnvBool(
        "FNVXR_D3D9_NATIVE_EQUIVALENCE_REQUIRE_MATCHED_RESOLVED_TARGETS",
        true)
        && ((result.resolvedCopies[0] > 0) != (result.resolvedCopies[1] > 0));
    result.drawMismatch = result.drawDelta > result.maxDrawDelta
        && result.drawDeltaRatio > result.maxDrawDeltaRatio;
    result.vsCallMismatch = result.vsCallDelta > result.maxVsCallDelta
        && result.vsCallDeltaRatio > result.maxVsCallDeltaRatio;
    result.equivalent = !result.guardEnabled
        || (!result.underfilled
            && !result.resolvedTargetMismatch
            && !result.drawMismatch
            && !result.vsCallMismatch);
    return result;
}

bool updateNativePassEquivalenceContinuity(
    const NativePassEquivalence& equivalence,
    LONG presentFrame)
{
    if (!equivalence.guardEnabled)
    {
        gNativeEquivalentPairsInSequence = equivalence.recoveryPairs;
        gNativeEquivalentLastPresentFrame = presentFrame;
        return true;
    }

    if (!equivalence.equivalent)
    {
        gNativeEquivalentPairsInSequence = 0;
        gNativeEquivalentLastPresentFrame = -2;
        gNativePassEquivalenceWasBlocked = true;
        return false;
    }

    if (gNativeEquivalentLastPresentFrame != presentFrame - 1)
        gNativeEquivalentPairsInSequence = 0;
    gNativeEquivalentLastPresentFrame = presentFrame;
    ++gNativeEquivalentPairsInSequence;
    if (gNativeEquivalentPairsInSequence < equivalence.recoveryPairs)
    {
        gNativePassEquivalenceWasBlocked = true;
        return false;
    }
    return true;
}

bool restoreNativeLeftEyeAsMono(IDirect3DDevice9* device)
{
    if (!device || !gRealStretchRect || !gLeftEyeSurface)
        return false;

    IDirect3DSurface9* backBuffer = nullptr;
    if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)) || !backBuffer)
        return false;
    const HRESULT result = gRealStretchRect(
        device,
        gLeftEyeSurface,
        nullptr,
        backBuffer,
        nullptr,
        D3DTEXF_NONE);
    releaseSurface(backBuffer);
    return SUCCEEDED(result);
}

void rejectNativeStereoPair(
    IDirect3DDevice9* device,
    const NativePassEquivalence& equivalence,
    LONG pairSequence,
    LONG presentFrame)
{
    closeNativeStereoOriginLease("native-pair-rejected");
    const bool restoredMono = restoreNativeLeftEyeAsMono(device);
    if (equivalence.guardEnabled
        && !equivalence.equivalent
        && gLatestSharedPlayerSnapshotValid)
    {
        const std::uint64_t holdFrames = static_cast<std::uint64_t>((std::max)(
            0.0f,
            readEnvFloat("FNVXR_D3D9_NATIVE_MISMATCH_HOLD_PLAYER_FRAMES", 45.0f)));
        gNativeDoubleTraversalHoldUntilPlayerFrame = (std::max)(
            gNativeDoubleTraversalHoldUntilPlayerFrame,
            gLatestSharedPlayerSnapshot.frame + holdFrames);
        gNativeCellTransitionReady = false;
    }
    InterlockedExchange(&gNativePassRejectedThisPresent, 1);
    InterlockedExchange(
        &gNativePassMismatchThisPresent,
        equivalence.equivalent ? 0 : 1);
    InterlockedExchange(&gNativePostprocessFanoutDrawsThisPresent, 0);
    InterlockedExchange(&gNativePostprocessFinalWritesThisPresent, 0);
    gNativePostprocessFanoutActive = false;
    gNativePipelineTraceThisPair = false;
    gNativePipelineTracePostprocessWindow = false;
    gNativeStereoRenderedPoseValid = false;

    const LONG logCount = InterlockedIncrement(&gLoggedNativePassEquivalence);
    if (logCount <= 48 || logCount % 120 == 0)
    {
        const char* reason = equivalence.underfilled
            ? "underfilled"
            : (equivalence.resolvedTargetMismatch
                ? "resolved-target-asymmetry"
                : (equivalence.drawMismatch
                    ? "draw-divergence"
                    : (equivalence.vsCallMismatch ? "vs-call-divergence" : "recovery-warmup")));
        char message[768] {};
        sprintf_s(
            message,
            "native stereo pair rejected pair=%ld presentFrame=%ld reason=%s restoredMono=%d recovery=%ld/%ld mismatchHoldUntil=%llu resolvedCopies=%ld/%ld targetCandidates=%ld seenAsymmetry=%ld snapshotAsymmetry=%ld draws=%ld/%ld drawDelta=%ld drawRatio=%.4f drawLimits=%ld/%.4f vsCalls=%ld/%ld vsDelta=%ld vsRatio=%.4f vsLimits=%ld/%.4f",
            pairSequence,
            presentFrame,
            reason,
            restoredMono ? 1 : 0,
            static_cast<LONG>(gNativeEquivalentPairsInSequence),
            equivalence.recoveryPairs,
            static_cast<unsigned long long>(gNativeDoubleTraversalHoldUntilPlayerFrame),
            equivalence.resolvedCopies[0],
            equivalence.resolvedCopies[1],
            equivalence.fullSizeCandidates,
            equivalence.candidateSeenAsymmetry,
            equivalence.candidateSnapshotAsymmetry,
            equivalence.draws[0],
            equivalence.draws[1],
            equivalence.drawDelta,
            equivalence.drawDeltaRatio,
            equivalence.maxDrawDelta,
            equivalence.maxDrawDeltaRatio,
            equivalence.vsCalls[0],
            equivalence.vsCalls[1],
            equivalence.vsCallDelta,
            equivalence.vsCallDeltaRatio,
            equivalence.maxVsCallDelta,
            equivalence.maxVsCallDeltaRatio);
        logLine(message);
    }
}

void logNativePassEquivalenceRecovery(
    const NativePassEquivalence& equivalence,
    LONG pairSequence,
    LONG presentFrame)
{
    if (!gNativePassEquivalenceWasBlocked)
        return;
    gNativePassEquivalenceWasBlocked = false;
    char message[448] {};
    sprintf_s(
        message,
        "native stereo equivalence recovered pair=%ld presentFrame=%ld recovery=%ld/%ld draws=%ld/%ld drawRatio=%.4f vsCalls=%ld/%ld vsRatio=%.4f",
        pairSequence,
        presentFrame,
        static_cast<LONG>(gNativeEquivalentPairsInSequence),
        equivalence.recoveryPairs,
        equivalence.draws[0],
        equivalence.draws[1],
        equivalence.drawDeltaRatio,
        equivalence.vsCalls[0],
        equivalence.vsCalls[1],
        equivalence.vsCallDeltaRatio);
    logLine(message);
}

void publishSharedStereoInvalid(bool uiActive, const char* reason);

void requestRenderOwnerShutdown()
{
    AcquireSRWLockExclusive(&gRenderPublishCommitLock);
    InterlockedIncrement(&gRenderThreadViolation);
    InterlockedExchange(&gRenderShutdownRequested, 1);
    ReleaseSRWLockExclusive(&gRenderPublishCommitLock);
}

bool serviceRenderOwnerShutdown(const char* reason)
{
    if (InterlockedCompareExchange(&gRenderShutdownRequested, 0, 0) == 0)
        return false;

    // Only the render-owner thread may touch producer/origin state.  A second
    // thread merely raises the interlocked request above, avoiding concurrent
    // header commits and origin-lease mutation.
    gNativeStereoRuntimeDisabled = true;
    gHavePublishedValidStereoWorldFrame = false;
    closeNativeStereoOriginLease(reason);
    if (gSharedStereoView)
        writeInvalidSharedStereoRecord(
            reinterpret_cast<SharedStereoHeader*>(gSharedStereoView),
            false);
    return true;
}

void __fastcall hookedDoRenderFrame(
    void* osGlobals,
    void*,
    void* renderer,
    std::uint8_t arg1,
    std::uint8_t arg2)
{
    const LONG currentThread = static_cast<LONG>(GetCurrentThreadId());
    const LONG renderOwner = InterlockedCompareExchange(
        &gRenderOwnerThreadId, currentThread, 0);
    if (renderOwner != 0 && renderOwner != currentThread)
    {
        requestRenderOwnerShutdown();
        logLine("stereo shutdown requested: DoRenderFrame moved to a second thread");
        callOriginalDoRenderFrame(osGlobals, renderer, arg1, arg2);
        return;
    }
    serviceRenderOwnerShutdown("render-thread-mismatch");
    InterlockedIncrement(&gNativeDoRenderHookEntriesThisPresent);
    InterlockedExchange(&gNativeLastDoRenderHookPresentFrame, static_cast<LONG>(gPresentFrames));

    LONG gateMask = 0;
    if (!gNativeStereoEnabled && !gNativeSingleTraversalReplayEnabled)
        gateMask |= NativeDoRenderGateDisabled;
    if (gNativeStereoRuntimeDisabled)
        gateMask |= NativeDoRenderGateRuntimeDisabled;
    if (gInNativeStereoHook)
        gateMask |= NativeDoRenderGateReentrant;
    if (!gNativeStereoDevice)
        gateMask |= NativeDoRenderGateNoDevice;
    if (renderer != nullptr)
        gateMask |= NativeDoRenderGateRendererArgument;
    if (suppressStereoForUiMode())
        gateMask |= NativeDoRenderGateUi;
    if (gNativeStereoPairsThisPresent > 0 || gNativeSingleTraversalFramesThisPresent > 0)
        gateMask |= NativeDoRenderGateAlreadyPaired;
    if (!criticalDeviceHooksIntact(gNativeStereoDevice))
        gateMask |= NativeDoRenderGateHookIntegrity;
    if (gateMask != 0)
    {
        InterlockedOr(&gNativeDoRenderGateMaskThisPresent, gateMask);
        if ((gateMask & (NativeDoRenderGateDisabled
                | NativeDoRenderGateRuntimeDisabled
                | NativeDoRenderGateUi
                | NativeDoRenderGateNoDevice
                | NativeDoRenderGateRendererArgument)) != 0)
        {
            closeNativeStereoOriginLease("render-gate");
        }
        callOriginalDoRenderFrame(osGlobals, renderer, arg1, arg2);
        return;
    }

    if (!nativeStereoFirstPersonReady())
    {
        closeNativeStereoOriginLease("first-person-gate");
        InterlockedOr(&gNativeDoRenderGateMaskThisPresent, NativeDoRenderGateFirstPerson);
        callOriginalDoRenderFrame(osGlobals, renderer, arg1, arg2);
        return;
    }

    if (!nativeStereoCellTransitionReady())
    {
        closeNativeStereoOriginLease("cell-transition-gate");
        InterlockedOr(&gNativeDoRenderGateMaskThisPresent, NativeDoRenderGateCellTransition);
        callOriginalDoRenderFrame(osGlobals, renderer, arg1, arg2);
        return;
    }

    NativeStereoRig rig {};
    NativeCameraSnapshot camera {};
    const bool rigReady = prepareNativeStereoRig(rig);
    const bool cameraReady = rigReady && readNativeWorldCamera(camera);
    const bool targetsReady = cameraReady && ensureStereoTargets(gNativeStereoDevice);
    if (!rigReady || !cameraReady || !targetsReady)
    {
        closeNativeStereoOriginLease("pose-camera-target-unavailable");
        LONG unavailableMask = 0;
        if (!rigReady)
            unavailableMask |= NativeDoRenderGatePose;
        else if (!cameraReady)
            unavailableMask |= NativeDoRenderGateCamera;
        else if (!targetsReady)
            unavailableMask |= NativeDoRenderGateTargets;
        InterlockedOr(&gNativeDoRenderGateMaskThisPresent, unavailableMask);
        const LONG failure = InterlockedIncrement(&gLoggedNativeStereoFailure);
        if (failure <= 16 || failure % 120 == 0)
        {
            char message[256] {};
            sprintf_s(
                message,
                "native stereo pair skipped unavailableMask=0x%lx pose=%d camera=%d targets=%d",
                static_cast<unsigned long>(unavailableMask),
                rigReady ? 1 : 0,
                cameraReady ? 1 : 0,
                targetsReady ? 1 : 0);
            logLine(message);
        }
        callOriginalDoRenderFrame(osGlobals, renderer, arg1, arg2);
        return;
    }

    InterlockedIncrement(&gNativeDoRenderStereoAttemptsThisPresent);

    if (gNativeSingleTraversalReplayEnabled)
    {
        const LONG presentBefore = static_cast<LONG>(gPresentFrames);
        const LONG replayDrawsBefore = static_cast<LONG>(gStereoReplayDrawsThisPresent);
        const LONG shaderWorldCandidatesBefore = static_cast<LONG>(gShaderWorldDrawCandidates);
        const LONG shaderWorldDrawsBefore = static_cast<LONG>(gShaderWorldDraws);
        const LONG shaderCoveredDrawsBefore = static_cast<LONG>(gShaderContractCoveredWorldDraws);
        const LONG strictEyeTargetDrawsBefore = static_cast<LONG>(gStrictEyeTargetDraws);
        const LONG strictEyeTargetProvenBefore = static_cast<LONG>(gStrictEyeTargetProvenBothEyeDraws);
        const LONG strictEyeTargetClearsBefore = static_cast<LONG>(gStrictEyeTargetClears);
        const LONG strictEyeTargetProvenClearsBefore = static_cast<LONG>(gStrictEyeTargetProvenBothEyeClears);
        const LONG strictEyeTargetCopiesBefore = static_cast<LONG>(gStrictEyeTargetCopies);
        const LONG strictEyeTargetProvenCopiesBefore = static_cast<LONG>(gStrictEyeTargetProvenBothEyeCopies);
        const LONG strictEyeTargetInitializationsBefore =
            static_cast<LONG>(gStrictEyeTargetFullInitializations);
        const LONG strictEyeTargetUnprovenWritesBefore =
            static_cast<LONG>(gStrictEyeTargetUnprovenWrites);
        const LONG shaderExcludedUserPrimitiveDrawsBefore =
            static_cast<LONG>(gShaderExcludedUserPrimitiveDraws);
        const LONG shaderExcludedAuxiliaryTargetDrawsBefore =
            static_cast<LONG>(gShaderExcludedAuxiliaryTargetDraws);
        LARGE_INTEGER begin {};
        LARGE_INTEGER end {};
        QueryPerformanceCounter(&begin);

        const NativeVector3 eyeDelta {
            rig.eyeOffsetUnits[1].x - rig.eyeOffsetUnits[0].x,
            rig.eyeOffsetUnits[1].y - rig.eyeOffsetUnits[0].y,
            rig.eyeOffsetUnits[1].z - rig.eyeOffsetUnits[0].z
        };
        gNativeSingleTraversalIpdUnits = std::sqrt(
            eyeDelta.x * eyeDelta.x + eyeDelta.y * eyeDelta.y + eyeDelta.z * eyeDelta.z);
        std::memcpy(gNativeSingleTraversalFov, rig.fov, sizeof(gNativeSingleTraversalFov));
        gNativeActiveCamera = camera;
        gNativeActiveRig = rig;
        ++gStereoReplayTransactionGeneration;
        if (gStereoReplayTransactionGeneration == 0)
            gStereoReplayTransactionGeneration = 1;
        gStrictEquivalentClearSeenThisTraversal = false;
        gStrictDrawSeenThisTraversal = false;
        gInNativeStereoHook = true;
        InterlockedExchange(&gNativeActiveEye, 2);
        const NativeNiTransform expectedCenterWorld = nativeCenterWorldTransform(camera, rig);
        openNativeStereoOriginLease(
            rig.pose,
            reinterpret_cast<std::uintptr_t>(camera.camera),
            &camera.world.rotation.m[0][0],
            &camera.world.translation.x);
        NativeCameraSnapshot centerCamera {};
        const bool centerApplied = prepareNativeSingleTraversalViewProjections(camera, rig, centerCamera);
        const bool centerCameraCaptured = centerApplied;
        if (centerCameraCaptured)
        {
            const LONG centerLog = InterlockedIncrement(&gLoggedNativeCenterCamera);
            if (centerLog <= 12 || centerLog % 120 == 0)
            {
                fnvxr::stereo::Matrix4 worldToCamera {};
                for (int row = 0; row < 4; ++row)
                {
                    for (int column = 0; column < 4; ++column)
                        worldToCamera.m[row][column] = centerCamera.worldToCamera[row][column];
                }
                char event[2048] {};
                int offset = sprintf_s(
                    event,
                    "{\"event\":\"fnvxrNativeCenterCamera\",\"frame\":%ld,\"poseSeq\":%ld,"
                    "\"frustum\":{\"left\":%.9g,\"right\":%.9g,\"top\":%.9g,\"bottom\":%.9g,"
                    "\"near\":%.9g,\"far\":%.9g,\"orthographic\":%u}",
                    presentBefore,
                    rig.poseSequence,
                    centerCamera.frustum.left,
                    centerCamera.frustum.right,
                    centerCamera.frustum.top,
                    centerCamera.frustum.bottom,
                    centerCamera.frustum.nearDistance,
                    centerCamera.frustum.farDistance,
                    static_cast<unsigned>(centerCamera.frustum.orthographic));
                appendMatrixJson(event, sizeof(event), offset, "worldToCamera", worldToCamera);
                if (offset > 0 && static_cast<size_t>(offset) < sizeof(event) - 2)
                    sprintf_s(event + offset, sizeof(event) - static_cast<size_t>(offset), "}");
                logLine(event);
            }
        }
        if (centerApplied)
            callOriginalDoRenderFrame(osGlobals, renderer, arg1, arg2);
        if (!centerApplied)
            closeNativeStereoOriginLease("center-camera-apply-failed");

        NativeCameraSnapshot cameraAfter {};
        const bool cameraAfterValid = centerApplied && readNativeWorldCamera(cameraAfter);
        const float centerCameraDelta = cameraAfterValid
            ? nativeFloatArrayMaxDelta(
                reinterpret_cast<const float*>(&expectedCenterWorld),
                reinterpret_cast<const float*>(&cameraAfter.world),
                sizeof(NativeNiTransform) / sizeof(float))
            : FLT_MAX;
        const float centerWorldToCameraDelta = cameraAfterValid
            ? matrixMaxAbsDifference(
                nativeWorldToClipMatrix(centerCamera),
                nativeWorldToClipMatrix(cameraAfter))
            : FLT_MAX;
        const float centerFrustumDelta = cameraAfterValid
            ? nativeFloatArrayMaxDelta(
                reinterpret_cast<const float*>(&centerCamera.frustum),
                reinterpret_cast<const float*>(&cameraAfter.frustum),
                6)
            : FLT_MAX;
        const float maxCenterCameraDelta = readEnvFloat(
            "FNVXR_D3D9_NATIVE_CENTER_CAMERA_MAX_DELTA",
            0.05f);
        const float maxCenterWorldToCameraDelta = readEnvFloat(
            "FNVXR_D3D9_NATIVE_CENTER_W2C_MAX_DELTA",
            0.0001f);
        const float maxCenterFrustumDelta = readEnvFloat(
            "FNVXR_D3D9_NATIVE_CENTER_FRUSTUM_MAX_DELTA",
            0.000001f);
        const bool centerCameraStable = cameraAfterValid
            && std::isfinite(centerCameraDelta)
            && std::isfinite(centerWorldToCameraDelta)
            && std::isfinite(centerFrustumDelta)
            && centerCameraDelta <= maxCenterCameraDelta
            && centerWorldToCameraDelta <= maxCenterWorldToCameraDelta
            && centerFrustumDelta <= maxCenterFrustumDelta
            && cameraAfter.frustum.orthographic == centerCamera.frustum.orthographic;

        InterlockedExchange(&gNativeActiveEye, -1);
        restoreNativeCamera(camera);
        gInNativeStereoHook = false;
        gNativeActiveCamera = {};
        gNativeActiveRig = {};
        gNativeSingleTraversalIpdUnits = 0.0f;
        std::memset(gNativeSingleTraversalFov, 0, sizeof(gNativeSingleTraversalFov));
        gNativeSingleTraversalViewProjectionValid = false;
        gNativeSingleTraversalCenterViewProjection = {};
        gNativeSingleTraversalEyeViewProjection[0] = {};
        gNativeSingleTraversalEyeViewProjection[1] = {};
        QueryPerformanceCounter(&end);

        if (!centerApplied)
        {
            closeNativeStereoOriginLease("center-camera-apply-failed");
            const LONG failure = InterlockedIncrement(&gLoggedNativeStereoFailure);
            if (failure <= 16 || failure % 120 == 0)
                logLine("single-traversal stereo skipped: center camera apply failed");
            callOriginalDoRenderFrame(osGlobals, renderer, arg1, arg2);
            return;
        }

        if (static_cast<LONG>(gPresentFrames) != presentBefore)
        {
            closeNativeStereoOriginLease("present-inside-render");
            gNativeStereoRuntimeDisabled = true;
            logLine("single-traversal stereo disabled: Fallout presented from inside DoRenderFrame");
            return;
        }

        if (!centerCameraStable)
        {
            closeNativeStereoOriginLease("center-camera-drift");
            const LONG failure = InterlockedIncrement(&gLoggedNativeStereoFailure);
            if (failure <= 16 || failure % 120 == 0)
            {
                char rejected[320] {};
                sprintf_s(
                    rejected,
                    "single-traversal stereo rejected: center camera drifted cameraAfterValid=%d worldDelta=%.7g worldAllowed=%.7g w2cDelta=%.7g w2cAllowed=%.7g frustumDelta=%.7g frustumAllowed=%.7g orthoMatch=%d",
                    cameraAfterValid ? 1 : 0,
                    centerCameraDelta,
                    maxCenterCameraDelta,
                    centerWorldToCameraDelta,
                    maxCenterWorldToCameraDelta,
                    centerFrustumDelta,
                    maxCenterFrustumDelta,
                    cameraAfterValid && cameraAfter.frustum.orthographic == centerCamera.frustum.orthographic ? 1 : 0);
                logLine(rejected);
            }
            return;
        }

        const LONG replayDraws = checkedInterlockedCounterDelta(
            static_cast<LONG>(gStereoReplayDrawsThisPresent), replayDrawsBefore);
        const LONG shaderWorldCandidates =
            checkedInterlockedCounterDelta(gShaderWorldDrawCandidates, shaderWorldCandidatesBefore);
        const LONG shaderWorldDraws = checkedInterlockedCounterDelta(
            gShaderWorldDraws, shaderWorldDrawsBefore);
        const LONG shaderCoveredDraws =
            checkedInterlockedCounterDelta(
                gShaderContractCoveredWorldDraws, shaderCoveredDrawsBefore);
        const LONG strictEyeTargetDraws =
            checkedInterlockedCounterDelta(gStrictEyeTargetDraws, strictEyeTargetDrawsBefore);
        const LONG strictEyeTargetProvenBothEyeDraws =
            checkedInterlockedCounterDelta(
                gStrictEyeTargetProvenBothEyeDraws, strictEyeTargetProvenBefore);
        const LONG strictEyeTargetClears =
            checkedInterlockedCounterDelta(gStrictEyeTargetClears, strictEyeTargetClearsBefore);
        const LONG strictEyeTargetProvenBothEyeClears =
            checkedInterlockedCounterDelta(
                gStrictEyeTargetProvenBothEyeClears, strictEyeTargetProvenClearsBefore);
        const LONG strictEyeTargetCopies =
            checkedInterlockedCounterDelta(gStrictEyeTargetCopies, strictEyeTargetCopiesBefore);
        const LONG strictEyeTargetProvenBothEyeCopies =
            checkedInterlockedCounterDelta(
                gStrictEyeTargetProvenBothEyeCopies, strictEyeTargetProvenCopiesBefore);
        const LONG strictEyeTargetFullInitializations =
            checkedInterlockedCounterDelta(
                gStrictEyeTargetFullInitializations, strictEyeTargetInitializationsBefore);
        const LONG strictEyeTargetUnprovenWrites =
            checkedInterlockedCounterDelta(
                gStrictEyeTargetUnprovenWrites, strictEyeTargetUnprovenWritesBefore);
        const LONG shaderExcludedUserPrimitiveDraws =
            checkedInterlockedCounterDelta(
                gShaderExcludedUserPrimitiveDraws, shaderExcludedUserPrimitiveDrawsBefore);
        const LONG shaderExcludedAuxiliaryTargetDraws =
            checkedInterlockedCounterDelta(
                gShaderExcludedAuxiliaryTargetDraws, shaderExcludedAuxiliaryTargetDrawsBefore);
        const double shaderContractCoverage = shaderWorldCandidates > 0
            ? static_cast<double>(shaderCoveredDraws) / static_cast<double>(shaderWorldCandidates)
            : 0.0;
        const double requiredShaderCoverage = static_cast<double>(
            readEnvFloat("FNVXR_D3D9_SHADER_MIN_CONTRACT_COVERAGE", 1.0f));
        const bool strictEyeTargetDrawLedgerReady =
            fnvxr::stereo::strictEyeTargetDrawLedgerComplete(
                strictEyeTargetDraws,
                strictEyeTargetProvenBothEyeDraws);
        const bool strictEyeTargetClearLedgerReady =
            fnvxr::stereo::strictEyeTargetOptionalWriteLedgerComplete(
                strictEyeTargetClears,
                strictEyeTargetProvenBothEyeClears);
        const bool strictEyeTargetCopyLedgerReady =
            fnvxr::stereo::strictEyeTargetOptionalWriteLedgerComplete(
                strictEyeTargetCopies,
                strictEyeTargetProvenBothEyeCopies);
        const bool shaderCoverageReady = gCriticalDeviceHooksReady
            && gCriticalDeviceHookMask == gExpectedCriticalDeviceHookMask
            && InterlockedCompareExchange(&gRenderThreadViolation, 0, 0) == 0
            && InterlockedCompareExchange(&gStateBlockRecording, 0, 0) == 0
            && InterlockedCompareExchange(&gD3DQueryObjectsObserved, 0, 0) == 0
            && gShaderWvpContractCount > 0
            && shaderWorldCandidates > 0
            && shaderWorldDraws == shaderWorldCandidates
            && shaderCoveredDraws == shaderWorldCandidates
            && shaderContractCoverage >= requiredShaderCoverage
            && strictEyeTargetDrawLedgerReady
            && strictEyeTargetClearLedgerReady
            && strictEyeTargetCopyLedgerReady
            && strictEyeTargetFullInitializations > 0
            && gStrictEquivalentClearSeenThisTraversal
            && strictEyeTargetUnprovenWrites == 0
            && shaderExcludedUserPrimitiveDraws == 0
            && shaderExcludedAuxiliaryTargetDraws == 0;
        if (!shaderCoverageReady)
        {
            closeNativeStereoOriginLease("shader-contract-coverage");
            gNativeStereoRenderedPoseValid = false;
            const LONG failure = InterlockedIncrement(&gLoggedNativeStereoFailure);
            if (failure <= 48 || failure % 120 == 0)
            {
                char rejected[1024] {};
                sprintf_s(
                    rejected,
                    "single-traversal stereo rejected: shader contract coverage replayDraws=%ld strictDraws=%ld/%ld drawsReady=%d strictClears=%ld/%ld clearsReady=%d strictCopies=%ld/%ld copiesReady=%d fullInitializations=%ld unprovenWrites=%ld programmableCandidates=%ld replayedWorldDraws=%ld covered=%ld excludedImmediatePrimitives=%ld excludedAuxiliaryTargets=%ld fraction=%.8f required=%.8f contracts=%ld",
                    replayDraws,
                    strictEyeTargetProvenBothEyeDraws,
                    strictEyeTargetDraws,
                    strictEyeTargetDrawLedgerReady ? 1 : 0,
                    strictEyeTargetProvenBothEyeClears,
                    strictEyeTargetClears,
                    strictEyeTargetClearLedgerReady ? 1 : 0,
                    strictEyeTargetProvenBothEyeCopies,
                    strictEyeTargetCopies,
                    strictEyeTargetCopyLedgerReady ? 1 : 0,
                    strictEyeTargetFullInitializations,
                    strictEyeTargetUnprovenWrites,
                    shaderWorldCandidates,
                    shaderWorldDraws,
                    shaderCoveredDraws,
                    shaderExcludedUserPrimitiveDraws,
                    shaderExcludedAuxiliaryTargetDraws,
                    shaderContractCoverage,
                    requiredShaderCoverage,
                    gShaderWvpContractCount);
                logLine(rejected);
            }
            return;
        }

        // The animation consumer accepts Committed only. Keep the candidate
        // lease private through every camera/target/shader proof gate so a
        // subsequently rejected traversal can never mutate the retail rig.
        if (!commitNativeStereoOriginLease())
        {
            closeNativeStereoOriginLease("origin-commit-failed");
            gNativeStereoRenderedPoseValid = false;
            logLine("single-traversal stereo rejected: authoritative origin commit failed");
            return;
        }

        const LONG pairSequence = publishNativeStereoPairSequence();
        gNativeStereoRenderedPoseSnapshot = rig.pose;
        gNativeStereoRenderedPoseSequence = rig.poseSequence;
        gNativeStereoRenderedDisplayTime = rig.displayTime;
        gNativeStereoRenderedPoseValid = true;
        InterlockedIncrement(&gNativeSingleTraversalFramesThisPresent);
        const LONG pairLog = InterlockedIncrement(&gLoggedNativeStereoPair);
        if (pairLog <= 24 || pairLog % 120 == 0)
        {
            const float headDeterminant = determinantNativeMatrix3(rig.headRotation);
            const float headOrthonormalError = orthonormalErrorNativeMatrix3(rig.headRotation);
            char message[1536] {};
            sprintf_s(
                message,
                "{\"event\":\"fnvxrSingleTraversalStereo\",\"pair\":%ld,\"presentFrame\":%ld,\"poseSeq\":%ld,\"renderMs\":%.3f,\"originalTraversals\":1,\"replayDraws\":%ld,\"programmableWorldCandidates\":%ld,\"programmableWorldDraws\":%ld,\"contractCoveredWorldDraws\":%ld,\"excludedImmediatePrimitiveDraws\":%ld,\"excludedAuxiliaryTargetDraws\":%ld,\"shaderContractCoverage\":%.8f,\"verifiedShaderContracts\":%ld,\"centerCameraApplied\":true,\"centerCameraStable\":true,\"centerCameraDelta\":%.8g,\"headDeterminant\":%.8g,\"headOrthonormalError\":%.8g,\"headLocal\":[%.6f,%.6f,%.6f],\"headRotation\":[%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g],\"leftOffset\":[%.6f,%.6f,%.6f],\"rightOffset\":[%.6f,%.6f,%.6f]}",
                pairSequence,
                presentBefore,
                rig.poseSequence,
                secondsBetween(begin, end) * 1000.0,
                replayDraws,
                shaderWorldCandidates,
                shaderWorldDraws,
                shaderCoveredDraws,
                shaderExcludedUserPrimitiveDraws,
                shaderExcludedAuxiliaryTargetDraws,
                shaderContractCoverage,
                gShaderWvpContractCount,
                centerCameraDelta,
                headDeterminant,
                headOrthonormalError,
                gLatestHeadLocalMeters[0],
                gLatestHeadLocalMeters[1],
                gLatestHeadLocalMeters[2],
                rig.headRotation.m[0][0],
                rig.headRotation.m[0][1],
                rig.headRotation.m[0][2],
                rig.headRotation.m[1][0],
                rig.headRotation.m[1][1],
                rig.headRotation.m[1][2],
                rig.headRotation.m[2][0],
                rig.headRotation.m[2][1],
                rig.headRotation.m[2][2],
                rig.eyeOffsetUnits[0].x,
                rig.eyeOffsetUnits[0].y,
                rig.eyeOffsetUnits[0].z,
                rig.eyeOffsetUnits[1].x,
                rig.eyeOffsetUnits[1].y,
                rig.eyeOffsetUnits[1].z);
            logLine(message);
        }
        return;
    }

    const bool skipEyeCopiesForPerfProbe =
        readEnvBool("FNVXR_D3D9_NATIVE_PERF_SKIP_EYE_COPIES", false);
    const LONG nextPairSequence = nextNativeStereoPairSequence();
    const LONG traceStride = static_cast<LONG>(readEnvFloat("FNVXR_D3D9_NATIVE_PIPELINE_TRACE_STRIDE", 120.0f));
    gNativePipelineTraceThisPair = readEnvBool("FNVXR_D3D9_NATIVE_PIPELINE_TRACE", true)
        && (nativeStereoPairNeedsWarmupAudit(3)
            || (traceStride > 0
                && fnvxr::shared::sequencedValueBits(nextPairSequence)
                    % static_cast<std::uint32_t>(traceStride) == 0u));
    gNativePipelineTracePostprocessWindow = false;
    for (int phase = 0; phase < 3; ++phase)
        InterlockedExchange(&gNativePipelineTraceEvents[phase], 0);
    InterlockedExchange(&gNativePipelineTracePostDraws, 0);
    InterlockedExchange(&gNativePipelineTraceEyeDraws[0], 0);
    InterlockedExchange(&gNativePipelineTraceEyeDraws[1], 0);
    gNativePipelineStereoSamplerMask = 0;
    resetNativeFullSizeTargetCandidates();
    resetNativePostprocessCommandStream();

    LARGE_INTEGER begin {};
    LARGE_INTEGER end {};
    QueryPerformanceCounter(&begin);
    const LONG presentBefore = static_cast<LONG>(gPresentFrames);
    bool leftCaptured = false;
    bool rightCaptured = false;
    bool leftApplied = false;
    bool rightApplied = false;
    bool rightPostprocessReplayed = false;
    NativeCameraSnapshot cameraAfterLeft {};
    NativeCameraSnapshot cameraAfterRight {};
    bool cameraAfterLeftValid = false;
    bool cameraAfterRightValid = false;
    gNativePostprocessFanoutActive = false;
    InterlockedExchange(&gNativePostprocessFanoutDrawsThisPresent, 0);
    InterlockedExchange(&gNativePostprocessFinalWritesThisPresent, 0);
    for (int eye = 0; eye < 2; ++eye)
    {
        gNativeResolvedTwinForEye[eye] = nullptr;
        InterlockedExchange(&gNativeResolvedCopies[eye], 0);
        InterlockedExchange(&gNativeObservedViewCalls[eye], 0);
        InterlockedExchange(&gNativeObservedProjectionCalls[eye], 0);
        InterlockedExchange(&gNativeVsConstantCalls[eye], 0);
        InterlockedExchange(&gNativeDrawCalls[eye], 0);
        InterlockedExchange(&gNativeCameraMatrixOverridesThisPair[eye], 0);
        InterlockedExchange(&gNativeGeometryRearmsThisPair[eye], 0);
        gNativeVsConstantStreamHash[eye] = 2166136261u;
        std::memset(&gNativeObservedView[eye], 0, sizeof(gNativeObservedView[eye]));
        std::memset(&gNativeObservedProjection[eye], 0, sizeof(gNativeObservedProjection[eye]));
    }
    const LONG cameraOverridesBefore = gNativeCameraMatrixOverrides;
    gNativeActiveCamera = camera;
    gNativeActiveRig = rig;
    gInNativeStereoHook = true;

    gNativePipelineStereoSamplerMask = 0;
    InterlockedExchange(&gNativeActiveEye, 0);
    leftApplied = applyNativeEyeCamera(camera, rig, 0);
    if (leftApplied)
    {
        callOriginalDoRenderFrame(osGlobals, renderer, arg1, arg2);
        gNativePostprocessRecording = false;
        if (skipEyeCopiesForPerfProbe)
        {
            leftCaptured = true;
        }
        else
        {
            IDirect3DSurface9* currentTarget = nullptr;
            if (SUCCEEDED(gNativeStereoDevice->GetRenderTarget(0, &currentTarget)) && currentTarget)
            {
                trackNativeFullSizeTargetCandidate(currentTarget, 0);
                releaseSurface(currentTarget);
            }
            snapshotNativeFullSizeTargets(gNativeStereoDevice, 0);
            leftCaptured = nativePostprocessCapturedLeftFinalTarget()
                || captureNativeStereoEye(gNativeStereoDevice, 0);
        }
        cameraAfterLeftValid = readNativeWorldCamera(cameraAfterLeft);
    }

    InterlockedExchange(&gNativeActiveEye, -1);
    restoreNativeCamera(camera);
    if (static_cast<LONG>(gPresentFrames) == presentBefore)
    {
        gNativePipelineStereoSamplerMask = 0;
        InterlockedExchange(&gNativeActiveEye, 1);
        rightApplied = applyNativeEyeCamera(camera, rig, 1);
    }
    if (rightApplied)
    {
        callOriginalDoRenderFrame(osGlobals, renderer, arg1, arg2);
        if (skipEyeCopiesForPerfProbe)
        {
            rightCaptured = true;
        }
        else
        {
            IDirect3DSurface9* currentTarget = nullptr;
            if (SUCCEEDED(gNativeStereoDevice->GetRenderTarget(0, &currentTarget)) && currentTarget)
            {
                trackNativeFullSizeTargetCandidate(currentTarget, 1);
                releaseSurface(currentTarget);
            }
            // The census snapshots originals into their per-eye twins.  Run it
            // before delayed image-space replay so it cannot overwrite the final
            // right-eye backbuffer twin with Fallout's unchanged left backbuffer.
            snapshotNativeFullSizeTargets(gNativeStereoDevice, 1);
            rightPostprocessReplayed = replayNativePostprocessCommandStreamForRightEye(gNativeStereoDevice);
            rightCaptured = rightPostprocessReplayed
                || captureNativeStereoEye(gNativeStereoDevice, 1);
        }
        cameraAfterRightValid = readNativeWorldCamera(cameraAfterRight);
    }
    InterlockedExchange(&gNativeActiveEye, -1);
    restoreNativeCamera(camera);
    gInNativeStereoHook = false;
    gNativeActiveCamera = {};
    gNativeActiveRig = {};

    if (static_cast<LONG>(gPresentFrames) != presentBefore)
    {
        gNativeStereoRuntimeDisabled = true;
        logLine("native stereo disabled: Fallout presented from inside DoRenderFrame");
        return;
    }

    if (!leftCaptured || !rightCaptured)
    {
        gNativePipelineTraceThisPair = false;
        gNativePipelineTracePostprocessWindow = false;
        const LONG failure = InterlockedIncrement(&gLoggedNativeStereoFailure);
        if (failure <= 16 || failure % 120 == 0)
        {
            char message[256] {};
            sprintf_s(
                message,
                "native stereo pair incomplete leftApplied=%d leftCaptured=%d rightApplied=%d rightCaptured=%d presentChanged=%d; frame will not be published",
                leftApplied ? 1 : 0,
                leftCaptured ? 1 : 0,
                rightApplied ? 1 : 0,
                rightCaptured ? 1 : 0,
                static_cast<LONG>(gPresentFrames) != presentBefore ? 1 : 0);
            logLine(message);
        }
        if (gNativeStereoEnabled)
            publishSharedStereoInvalid(false, "incomplete-native-pair");
        return;
    }

    const LONG pairSequence = publishNativeStereoPairSequence();
    logNativeCameraBasisIfChanged(
        rig,
        camera,
        cameraAfterLeft,
        cameraAfterLeftValid,
        cameraAfterRight,
        cameraAfterRightValid);

    const NativePassEquivalence passEquivalence = evaluateNativePassEquivalence();
    if (!updateNativePassEquivalenceContinuity(passEquivalence, presentBefore))
    {
        rejectNativeStereoPair(
            gNativeStereoDevice,
            passEquivalence,
            pairSequence,
            presentBefore);
        return;
    }
    logNativePassEquivalenceRecovery(passEquivalence, pairSequence, presentBefore);

    gNativeStereoRenderedPoseSnapshot = rig.pose;
    gNativeStereoRenderedPoseSequence = rig.poseSequence;
    gNativeStereoRenderedDisplayTime = rig.displayTime;
    gNativeStereoRenderedPoseValid = true;
    gNativePostprocessFanoutActive = !rightPostprocessReplayed
        && gNativeResolvedTwinForEye[0]
        && gNativeResolvedTwinForEye[1]
        && gNativeResolvedCopies[0] > 0
        && gNativeResolvedCopies[1] > 0;
    gNativePipelineTracePostprocessWindow = gNativePipelineTraceThisPair;
    InterlockedIncrement(&gNativeStereoPairsThisPresent);
    QueryPerformanceCounter(&end);

    const LONG pairLog = InterlockedIncrement(&gLoggedNativeStereoPair);
    if (pairLog <= 24 || pairLog % 120 == 0)
    {
        const float cameraWorldDelta = cameraAfterLeftValid && cameraAfterRightValid
            ? nativeFloatArrayMaxDelta(
                reinterpret_cast<const float*>(&cameraAfterLeft.world),
                reinterpret_cast<const float*>(&cameraAfterRight.world),
                sizeof(NativeNiTransform) / sizeof(float))
            : 0.0f;
        const float cameraMatrixDelta = cameraAfterLeftValid && cameraAfterRightValid
            ? nativeFloatArrayMaxDelta(
                &cameraAfterLeft.worldToCamera[0][0],
                &cameraAfterRight.worldToCamera[0][0],
                16)
            : 0.0f;
        const float fixedViewDelta = nativeFloatArrayMaxDelta(
            reinterpret_cast<const float*>(&gNativeObservedView[0]),
            reinterpret_cast<const float*>(&gNativeObservedView[1]),
            16);
        const float fixedProjectionDelta = nativeFloatArrayMaxDelta(
            reinterpret_cast<const float*>(&gNativeObservedProjection[0]),
            reinterpret_cast<const float*>(&gNativeObservedProjection[1]),
            16);
        char message[1280] {};
        sprintf_s(
            message,
            "native same-frame stereo pair=%ld presentFrame=%ld poseSeq=%ld renderMs=%.3f camera=%p cameraMatrixOverrides=%ld/%ld/%ld cameraAfterValid=%d/%d cameraWorldDelta=%.7g cameraMatrixDelta=%.7g fixedViewCalls=%ld/%ld fixedViewDelta=%.7g fixedProjectionCalls=%ld/%ld fixedProjectionDelta=%.7g vsCalls=%ld/%ld vsDelta=%ld vsRatio=%.4f vsHash=0x%08x/0x%08x draws=%ld/%ld drawDelta=%ld drawRatio=%.4f equivalenceGuard=%d recovery=%ld/%ld imageSpaceRearms=%ld/%ld resolvedCopies=%ld/%ld targetCandidates=%ld seenAsymmetry=%ld snapshotAsymmetry=%ld leftOffset=(%.4f %.4f %.4f) rightOffset=(%.4f %.4f %.4f)",
            pairSequence,
            presentBefore,
            rig.poseSequence,
            secondsBetween(begin, end) * 1000.0,
            camera.camera,
            static_cast<LONG>(gNativeCameraMatrixOverrides) - cameraOverridesBefore,
            static_cast<LONG>(gNativeCameraMatrixOverridesThisPair[0]),
            static_cast<LONG>(gNativeCameraMatrixOverridesThisPair[1]),
            cameraAfterLeftValid ? 1 : 0,
            cameraAfterRightValid ? 1 : 0,
            cameraWorldDelta,
            cameraMatrixDelta,
            static_cast<LONG>(gNativeObservedViewCalls[0]),
            static_cast<LONG>(gNativeObservedViewCalls[1]),
            fixedViewDelta,
            static_cast<LONG>(gNativeObservedProjectionCalls[0]),
            static_cast<LONG>(gNativeObservedProjectionCalls[1]),
            fixedProjectionDelta,
            static_cast<LONG>(gNativeVsConstantCalls[0]),
            static_cast<LONG>(gNativeVsConstantCalls[1]),
            passEquivalence.vsCallDelta,
            passEquivalence.vsCallDeltaRatio,
            gNativeVsConstantStreamHash[0],
            gNativeVsConstantStreamHash[1],
            static_cast<LONG>(gNativeDrawCalls[0]),
            static_cast<LONG>(gNativeDrawCalls[1]),
            passEquivalence.drawDelta,
            passEquivalence.drawDeltaRatio,
            passEquivalence.guardEnabled ? 1 : 0,
            static_cast<LONG>(gNativeEquivalentPairsInSequence),
            passEquivalence.recoveryPairs,
            static_cast<LONG>(gNativeGeometryRearmsThisPair[0]),
            static_cast<LONG>(gNativeGeometryRearmsThisPair[1]),
            static_cast<LONG>(gNativeResolvedCopies[0]),
            static_cast<LONG>(gNativeResolvedCopies[1]),
            passEquivalence.fullSizeCandidates,
            passEquivalence.candidateSeenAsymmetry,
            passEquivalence.candidateSnapshotAsymmetry,
            rig.eyeOffsetUnits[0].x,
            rig.eyeOffsetUnits[0].y,
            rig.eyeOffsetUnits[0].z,
            rig.eyeOffsetUnits[1].x,
            rig.eyeOffsetUnits[1].y,
            rig.eyeOffsetUnits[1].z);
        logLine(message);
    }
}

bool writeNativeStereoJump(std::uintptr_t source, void* target)
{
    DWORD oldProtect = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(source), 6, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;
    auto* bytes = reinterpret_cast<std::uint8_t*>(source);
    bytes[0] = 0xE9;
    *reinterpret_cast<std::uint32_t*>(bytes + 1) = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(target) - source - 5);
    bytes[5] = 0x90;
    DWORD unused = 0;
    VirtualProtect(reinterpret_cast<void*>(source), 6, oldProtect, &unused);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(source), 6);
    return true;
}

void installNativeStereoEngineHook(IDirect3DDevice9* device)
{
    if ((!gNativeStereoEnabled && !gNativeSingleTraversalReplayEnabled)
        || gNativeStereoHookInstalled
        || gNativeStereoRuntimeDisabled
        || !device)
        return;
    gNativeStereoDevice = device;

    auto* target = reinterpret_cast<std::uint8_t*>(FalloutDoRenderFrameAddress);
    const bool enableExperimentalImageSpaceRearm =
        readEnvBool("FNVXR_D3D9_NATIVE_REARM_IMAGE_SPACE", false);
    const std::uint8_t expected[6] { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08 };
    const std::uint8_t expectedSetFrustum[7] { 0x8B, 0x54, 0x24, 0x04, 0xD9, 0x42, 0x10 };
    const std::uint8_t expectedUpdateWorldToCamera[6] { 0x83, 0xEC, 0x58, 0xD9, 0x41, 0x68 };
    const std::uint8_t expectedImageSpaceSubmit[6] { 0x56, 0x8B, 0x74, 0x24, 0x08, 0x57 };
    __try
    {
        if (std::memcmp(target, expected, sizeof(expected)) != 0)
        {
            char message[256] {};
            sprintf_s(
                message,
                "native stereo render hook prologue mismatch got=%02X %02X %02X %02X %02X %02X",
                target[0], target[1], target[2], target[3], target[4], target[5]);
            logLine(message);
            gNativeStereoRuntimeDisabled = true;
            return;
        }
        if (std::memcmp(
                reinterpret_cast<void*>(FalloutNiCameraSetViewFrustumAddress),
                expectedSetFrustum,
                sizeof(expectedSetFrustum)) != 0
            || std::memcmp(
                reinterpret_cast<void*>(FalloutNiCameraUpdateWorldToCameraAddress),
                expectedUpdateWorldToCamera,
                sizeof(expectedUpdateWorldToCamera)) != 0
            || (enableExperimentalImageSpaceRearm
                && std::memcmp(
                    reinterpret_cast<void*>(FalloutImageSpaceGeometrySubmitAddress),
                    expectedImageSpaceSubmit,
                    sizeof(expectedImageSpaceSubmit)) != 0))
        {
            logLine("native stereo engine helper signature mismatch");
            gNativeStereoRuntimeDisabled = true;
            return;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        logLine("native stereo render hook prologue read failed");
        gNativeStereoRuntimeDisabled = true;
        return;
    }

    if (enableExperimentalImageSpaceRearm)
    {
        auto* imageSpaceSubmitTarget = reinterpret_cast<std::uint8_t*>(
            FalloutImageSpaceGeometrySubmitAddress);
        auto* imageSpaceSubmitTrampoline = static_cast<std::uint8_t*>(
            VirtualAlloc(nullptr, 11, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        if (!imageSpaceSubmitTrampoline)
        {
            logLine("native stereo image-space submit trampoline allocation failed");
            gNativeStereoRuntimeDisabled = true;
            return;
        }
        std::memcpy(imageSpaceSubmitTrampoline, imageSpaceSubmitTarget, 6);
        imageSpaceSubmitTrampoline[6] = 0xE9;
        *reinterpret_cast<std::uint32_t*>(imageSpaceSubmitTrampoline + 7) = static_cast<std::uint32_t>(
            (FalloutImageSpaceGeometrySubmitAddress + 6)
            - (reinterpret_cast<std::uintptr_t>(imageSpaceSubmitTrampoline) + 11));
        FlushInstructionCache(GetCurrentProcess(), imageSpaceSubmitTrampoline, 11);
        gNativeGeometrySubmitTrampoline = imageSpaceSubmitTrampoline;
        if (!writeNativeStereoJump(
                FalloutImageSpaceGeometrySubmitAddress,
                reinterpret_cast<void*>(&hookedNativeImageSpaceGeometrySubmit)))
        {
            VirtualFree(imageSpaceSubmitTrampoline, 0, MEM_RELEASE);
            gNativeGeometrySubmitTrampoline = nullptr;
            gNativeStereoRuntimeDisabled = true;
            logLine("native stereo image-space submit hook installation failed");
            return;
        }
    }

    auto* cameraMatrixTarget = reinterpret_cast<std::uint8_t*>(
        FalloutNiCameraUpdateWorldToCameraAddress);
    auto* cameraMatrixTrampoline = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, 11, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!cameraMatrixTrampoline)
    {
        logLine("native stereo NiCamera matrix trampoline allocation failed");
        gNativeStereoRuntimeDisabled = true;
        return;
    }
    std::memcpy(cameraMatrixTrampoline, cameraMatrixTarget, 6);
    cameraMatrixTrampoline[6] = 0xE9;
    *reinterpret_cast<std::uint32_t*>(cameraMatrixTrampoline + 7) = static_cast<std::uint32_t>(
        (FalloutNiCameraUpdateWorldToCameraAddress + 6)
        - (reinterpret_cast<std::uintptr_t>(cameraMatrixTrampoline) + 11));
    FlushInstructionCache(GetCurrentProcess(), cameraMatrixTrampoline, 11);
    gNativeCameraMatrixTrampoline = cameraMatrixTrampoline;
    if (!writeNativeStereoJump(
            FalloutNiCameraUpdateWorldToCameraAddress,
            reinterpret_cast<void*>(&hookedNativeCameraUpdateWorldToCamera)))
    {
        VirtualFree(cameraMatrixTrampoline, 0, MEM_RELEASE);
        gNativeCameraMatrixTrampoline = nullptr;
        gNativeStereoRuntimeDisabled = true;
        logLine("native stereo NiCamera matrix hook installation failed");
        return;
    }

    auto* trampoline = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, 11, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!trampoline)
    {
        logLine("native stereo render hook trampoline allocation failed");
        gNativeStereoRuntimeDisabled = true;
        return;
    }
    std::memcpy(trampoline, target, 6);
    trampoline[6] = 0xE9;
    *reinterpret_cast<std::uint32_t*>(trampoline + 7) = static_cast<std::uint32_t>(
        (FalloutDoRenderFrameAddress + 6) - (reinterpret_cast<std::uintptr_t>(trampoline) + 11));
    FlushInstructionCache(GetCurrentProcess(), trampoline, 11);
    gNativeStereoTrampoline = trampoline;

    if (!writeNativeStereoJump(FalloutDoRenderFrameAddress, reinterpret_cast<void*>(&hookedDoRenderFrame)))
    {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        gNativeStereoTrampoline = nullptr;
        gNativeStereoRuntimeDisabled = true;
        logLine("native stereo render hook installation failed");
        return;
    }

    gNativeStereoHookInstalled = true;
    const char* producerName = gNativeSingleTraversalReplayEnabled
        ? "single-traversal stereo"
        : "native same-frame stereo";
    char installed[384] {};
    sprintf_s(
        installed,
        enableExperimentalImageSpaceRearm
            ? "%s hooked Fallout DoRenderFrame at 0x008706B0, NiCamera matrix rebuild at 0x00A70BA0, and experimental image-space submit at 0x00A74600"
            : "%s hooked Fallout DoRenderFrame at 0x008706B0 and NiCamera matrix rebuild at 0x00A70BA0; experimental image-space rearm is disabled",
        producerName);
    logLine(installed);
}

struct NativeRenderHookIntegrity
{
    bool readable = false;
    bool intact = false;
    std::uint8_t bytes[6] {};
    std::uintptr_t jumpTarget = 0;
};

NativeRenderHookIntegrity inspectNativeRenderHookIntegrity()
{
    NativeRenderHookIntegrity result {};
    __try
    {
        const auto* target = reinterpret_cast<const std::uint8_t*>(FalloutDoRenderFrameAddress);
        std::memcpy(result.bytes, target, sizeof(result.bytes));
        result.readable = true;
        if (result.bytes[0] == 0xE9)
        {
            std::int32_t displacement = 0;
            std::memcpy(&displacement, result.bytes + 1, sizeof(displacement));
            result.jumpTarget = static_cast<std::uintptr_t>(
                static_cast<std::intptr_t>(FalloutDoRenderFrameAddress + 5)
                + static_cast<std::intptr_t>(displacement));
            result.intact = result.jumpTarget
                == reinterpret_cast<std::uintptr_t>(&hookedDoRenderFrame);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        result = {};
    }
    return result;
}

void observeNativeRenderHookContinuity(
    LONG hookEntries,
    LONG stereoAttempts,
    LONG gateMask,
    bool stereoSuppressed)
{
    if ((!gNativeStereoEnabled && !gNativeSingleTraversalReplayEnabled) || !gNativeStereoHookInstalled)
        return;

    updateSharedCameraState();
    const bool firstPersonReady = gLatestSharedCameraSnapshotValid
        && gSharedCameraActive
        && gLatestSharedCameraSnapshot.active != 0
        && gLatestSharedCameraSnapshot.thirdPerson == 0;
    const bool shouldReachHook = !gNativeStereoRuntimeDisabled
        && !stereoSuppressed
        && firstPersonReady;
    if (!shouldReachHook)
    {
        gNativeHookSilentGameplayFrames = 0;
        return;
    }

    if (hookEntries > 0)
    {
        if (gNativeHookSilentGameplayFrames > 0)
        {
            char message[320] {};
            sprintf_s(
                message,
                "native stereo render hook resumed presentFrame=%ld silentFrames=%ld hookEntries=%ld attempts=%ld gateMask=0x%lx lastHookFrame=%ld",
                static_cast<LONG>(gPresentFrames),
                static_cast<LONG>(gNativeHookSilentGameplayFrames),
                hookEntries,
                stereoAttempts,
                static_cast<unsigned long>(gateMask),
                static_cast<LONG>(gNativeLastDoRenderHookPresentFrame));
            logLine(message);
        }
        gNativeHookSilentGameplayFrames = 0;
        return;
    }

    const LONG silentFrames = InterlockedIncrement(&gNativeHookSilentGameplayFrames);
    if (silentFrames != 1 && silentFrames != 30 && silentFrames % 120 != 0)
        return;

    const NativeRenderHookIntegrity integrity = inspectNativeRenderHookIntegrity();
    const LONG logCount = InterlockedIncrement(&gLoggedNativeHookContinuity);
    char message[640] {};
    sprintf_s(
        message,
        "native stereo render hook silent count=%ld presentFrame=%ld silentFrames=%ld hookReadable=%d hookIntact=%d jumpTarget=%p expectedTarget=%p bytes=%02X-%02X-%02X-%02X-%02X-%02X attempts=%ld gateMask=0x%lx lastHookFrame=%ld cameraSeq=%ld cell=0x%08lx cellReady=%d screenProjection=%d screenView=%d",
        logCount,
        static_cast<LONG>(gPresentFrames),
        silentFrames,
        integrity.readable ? 1 : 0,
        integrity.intact ? 1 : 0,
        reinterpret_cast<void*>(integrity.jumpTarget),
        reinterpret_cast<void*>(&hookedDoRenderFrame),
        static_cast<unsigned>(integrity.bytes[0]),
        static_cast<unsigned>(integrity.bytes[1]),
        static_cast<unsigned>(integrity.bytes[2]),
        static_cast<unsigned>(integrity.bytes[3]),
        static_cast<unsigned>(integrity.bytes[4]),
        static_cast<unsigned>(integrity.bytes[5]),
        stereoAttempts,
        static_cast<unsigned long>(gateMask),
        static_cast<LONG>(gNativeLastDoRenderHookPresentFrame),
        gLatestSharedCameraSnapshotSequence,
        static_cast<unsigned long>(gNativeStableCellFormId),
        gNativeCellTransitionReady ? 1 : 0,
        currentProjectionLooksScreenSpace() ? 1 : 0,
        currentViewLooksScreenSpace() ? 1 : 0);
    logLine(message);

    if (integrity.readable && !integrity.intact)
    {
        gNativeStereoRuntimeDisabled = true;
        gNativeEquivalentPairsInSequence = 0;
        gNativeEquivalentLastPresentFrame = -2;
        gNativePassEquivalenceWasBlocked = true;
        logLine("native stereo disabled fail-closed: Fallout DoRenderFrame hook was replaced");
    }
}

bool ensureWideWorldTargets(IDirect3DDevice9* device)
{
    if (!gWideWorldReplayEnabled || !device)
        return false;

    IDirect3DSurface9* backBuffer = nullptr;
    if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)) || !backBuffer)
        return false;

    D3DSURFACE_DESC desc {};
    const HRESULT descResult = backBuffer->GetDesc(&desc);
    backBuffer->Release();
    if (FAILED(descResult))
        return false;

    if (gWideWorldSurface
        && gWideWorldDepth
        && gStereoTargetWidth == desc.Width
        && gStereoTargetHeight == desc.Height
        && gStereoTargetFormat == desc.Format)
    {
        return true;
    }

    releaseWideWorldTargets();
    if (FAILED(createEyeTarget(
            device,
            desc.Width,
            desc.Height,
            desc.Format,
            gWideWorldTexture,
            gWideWorldSurface,
            gWideWorldDepth)))
    {
        releaseWideWorldTargets();
        logLine("failed to create wide world replay target");
        return false;
    }

    gStereoTargetWidth = desc.Width;
    gStereoTargetHeight = desc.Height;
    gStereoTargetFormat = desc.Format;
    const LONG logCount = InterlockedIncrement(&gLoggedWideWorldTargets);
    if (logCount <= 3 || logCount % 60 == 0)
    {
        char message[192] {};
        sprintf_s(
            message,
            "created wide world replay target %ux%u format=%u",
            gStereoTargetWidth,
            gStereoTargetHeight,
            static_cast<unsigned>(gStereoTargetFormat));
        logLine(message);
    }
    return true;
}

bool ensureReadbackTargets(IDirect3DDevice9* device)
{
    if (!gStereoReadbackEnabled || !gLeftEyeSurface || !gRightEyeSurface)
        return false;

    if (gLeftEyeReadback && gRightEyeReadback)
        return true;

    releaseSurface(gLeftEyeReadback);
    releaseSurface(gRightEyeReadback);
    if (FAILED(device->CreateOffscreenPlainSurface(
            gStereoTargetWidth,
            gStereoTargetHeight,
            gStereoTargetFormat,
            D3DPOOL_SYSTEMMEM,
            &gLeftEyeReadback,
            nullptr))
        || FAILED(device->CreateOffscreenPlainSurface(
            gStereoTargetWidth,
            gStereoTargetHeight,
            gStereoTargetFormat,
            D3DPOOL_SYSTEMMEM,
            &gRightEyeReadback,
            nullptr)))
    {
        releaseSurface(gLeftEyeReadback);
        releaseSurface(gRightEyeReadback);
        logLine("failed to create stereo readback surfaces");
        return false;
    }

    logLine("created stereo readback surfaces");
    return true;
}

bool ensureWideWorldReadback(IDirect3DDevice9* device)
{
    if (!gWideWorldReplayEnabled || !gWideWorldSurface)
        return false;

    if (gWideWorldReadback)
        return true;

    releaseSurface(gWideWorldReadback);
    if (FAILED(device->CreateOffscreenPlainSurface(
            gStereoTargetWidth,
            gStereoTargetHeight,
            gStereoTargetFormat,
            D3DPOOL_SYSTEMMEM,
            &gWideWorldReadback,
            nullptr)))
    {
        releaseSurface(gWideWorldReadback);
        logLine("failed to create wide world readback surface");
        return false;
    }

    logLine("created wide world readback surface");
    return true;
}

void resetBestStereoSnapshotThisPresent()
{
    gBestStereoDiffThisPresent = 0;
    gBestStereoSamplesThisPresent = 0;
    gBestStereoLeftHashThisPresent = 0;
    gBestStereoRightHashThisPresent = 0;
    gHaveBestStereoSnapshotThisPresent = false;
}

void resetStereoCollapseAuditThisPresent()
{
    gStereoCollapseAuditPrevDiff = 0;
    gStereoCollapseAuditPrevSamples = 0;
    gStereoCollapseAuditPrevLeftHash = 0;
    gStereoCollapseAuditPrevRightHash = 0;
    gStereoCollapseAuditPrevDraw = 0;
}

bool ensureBestStereoSnapshotTargets(IDirect3DDevice9* device)
{
    if (!device || !gStereoReadbackEnabled || gStereoTargetWidth == 0 || gStereoTargetHeight == 0)
        return false;

    if (gBestLeftEyeReadback && gBestRightEyeReadback)
        return true;

    releaseSurface(gBestLeftEyeReadback);
    releaseSurface(gBestRightEyeReadback);
    if (FAILED(device->CreateOffscreenPlainSurface(
            gStereoTargetWidth,
            gStereoTargetHeight,
            gStereoTargetFormat,
            D3DPOOL_SYSTEMMEM,
            &gBestLeftEyeReadback,
            nullptr))
        || FAILED(device->CreateOffscreenPlainSurface(
            gStereoTargetWidth,
            gStereoTargetHeight,
            gStereoTargetFormat,
            D3DPOOL_SYSTEMMEM,
            &gBestRightEyeReadback,
            nullptr)))
    {
        releaseSurface(gBestLeftEyeReadback);
        releaseSurface(gBestRightEyeReadback);
        logLine("failed to create best stereo snapshot surfaces");
        return false;
    }

    logLine("created best stereo snapshot surfaces");
    return true;
}

UINT bytesPerPixelForFormat(D3DFORMAT format)
{
    switch (format)
    {
    case D3DFMT_A8:
    case D3DFMT_L8:
    case D3DFMT_P8:
        return 1;
    case D3DFMT_R5G6B5:
    case D3DFMT_X1R5G5B5:
    case D3DFMT_A1R5G5B5:
    case D3DFMT_A4R4G4B4:
    case D3DFMT_V8U8:
    case D3DFMT_L6V5U5:
    case D3DFMT_L16:
    case D3DFMT_R16F:
        return 2;
    case D3DFMT_A8R8G8B8:
    case D3DFMT_X8R8G8B8:
    case D3DFMT_A8B8G8R8:
    case D3DFMT_X8B8G8R8:
    case D3DFMT_A2R10G10B10:
    case D3DFMT_A2B10G10R10:
    case D3DFMT_A2W10V10U10:
    case D3DFMT_G16R16:
    case D3DFMT_R8G8_B8G8:
    case D3DFMT_G8R8_G8B8:
    case D3DFMT_R32F:
        return 4;
    case D3DFMT_A16B16G16R16:
    case D3DFMT_Q16W16V16U16:
    case D3DFMT_A16B16G16R16F:
    case D3DFMT_G32R32F:
        return 8;
    case D3DFMT_A32B32G32R32F:
        return 16;
    default:
        return 4;
    }
}

bool ensureProbeReadbackTargets(IDirect3DDevice9* device, const D3DSURFACE_DESC& desc)
{
    if (!device || !gStereoReadbackEnabled)
        return false;
    if (desc.Width == 0
        || desc.Height == 0
        || desc.Width > SharedVideoMaxWidth
        || desc.Height > SharedVideoMaxHeight
        || desc.MultiSampleType != D3DMULTISAMPLE_NONE)
    {
        return false;
    }

    if (gProbeLeftEyeReadback
        && gProbeRightEyeReadback
        && gProbeStereoTargetWidth == desc.Width
        && gProbeStereoTargetHeight == desc.Height
        && gProbeStereoTargetFormat == desc.Format)
    {
        return true;
    }

    releaseSurface(gProbeLeftEyeReadback);
    releaseSurface(gProbeRightEyeReadback);
    gProbeStereoTargetWidth = 0;
    gProbeStereoTargetHeight = 0;
    gProbeStereoTargetFormat = D3DFMT_UNKNOWN;

    const HRESULT leftResult = device->CreateOffscreenPlainSurface(
        desc.Width,
        desc.Height,
        desc.Format,
        D3DPOOL_SYSTEMMEM,
        &gProbeLeftEyeReadback,
        nullptr);
    const HRESULT rightResult = SUCCEEDED(leftResult)
        ? device->CreateOffscreenPlainSurface(
            desc.Width,
            desc.Height,
            desc.Format,
            D3DPOOL_SYSTEMMEM,
            &gProbeRightEyeReadback,
            nullptr)
        : leftResult;
    if (FAILED(leftResult) || FAILED(rightResult))
    {
        releaseSurface(gProbeLeftEyeReadback);
        releaseSurface(gProbeRightEyeReadback);
        const LONG failLog = InterlockedIncrement(&gLoggedStereoProbeReadbackFailed);
        if (failLog <= 12 || failLog % 120 == 0)
        {
            char message[256] {};
            sprintf_s(
                message,
                "failed to create stereo probe readback count=%ld size=%ux%u fmt=%lu result=0x%08lx/0x%08lx",
                failLog,
                desc.Width,
                desc.Height,
                static_cast<unsigned long>(desc.Format),
                static_cast<unsigned long>(leftResult),
                static_cast<unsigned long>(rightResult));
            logLine(message);
        }
        return false;
    }

    gProbeStereoTargetWidth = desc.Width;
    gProbeStereoTargetHeight = desc.Height;
    gProbeStereoTargetFormat = desc.Format;
    const LONG createLog = InterlockedIncrement(&gLoggedStereoProbeReadbackCreated);
    if (createLog <= 8 || createLog % 120 == 0)
    {
        char message[192] {};
        sprintf_s(
            message,
            "created stereo probe readback count=%ld size=%ux%u fmt=%lu bpp=%u",
            createLog,
            desc.Width,
            desc.Height,
            static_cast<unsigned long>(desc.Format),
            bytesPerPixelForFormat(desc.Format));
        logLine(message);
    }
    return true;
}

bool copyReadbackSurfacePixels(IDirect3DSurface9* source, IDirect3DSurface9* destination)
{
    if (!source || !destination || gStereoTargetWidth == 0 || gStereoTargetHeight == 0)
        return false;

    D3DLOCKED_RECT srcLocked {};
    D3DLOCKED_RECT dstLocked {};
    if (FAILED(source->LockRect(&srcLocked, nullptr, D3DLOCK_READONLY)))
        return false;
    if (FAILED(destination->LockRect(&dstLocked, nullptr, 0)))
    {
        source->UnlockRect();
        return false;
    }

    const auto* srcRows = static_cast<const std::uint8_t*>(srcLocked.pBits);
    auto* dstRows = static_cast<std::uint8_t*>(dstLocked.pBits);
    const UINT rowBytes = gStereoTargetWidth * bytesPerPixelForFormat(gStereoTargetFormat);
    for (UINT y = 0; y < gStereoTargetHeight; ++y)
    {
        std::memcpy(
            dstRows + static_cast<size_t>(y) * dstLocked.Pitch,
            srcRows + static_cast<size_t>(y) * srcLocked.Pitch,
            rowBytes);
    }

    destination->UnlockRect();
    source->UnlockRect();
    return true;
}

std::uint32_t checksumSurface(IDirect3DSurface9* surface)
{
    if (!surface)
        return 0;

    D3DLOCKED_RECT locked {};
    if (FAILED(surface->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
        return 0;

    std::uint32_t hash = 2166136261u;
    const auto* row = static_cast<const std::uint8_t*>(locked.pBits);
    const UINT sampleStride = 4096;
    const UINT rowBytes = gStereoTargetWidth * bytesPerPixelForFormat(gStereoTargetFormat);
    for (UINT y = 0; y < gStereoTargetHeight; ++y)
    {
        const auto* pixel = row + y * locked.Pitch;
        for (UINT x = 0; x < rowBytes; x += sampleStride)
        {
            hash ^= pixel[x];
            hash *= 16777619u;
        }
    }

    surface->UnlockRect();
    return hash;
}

struct SurfaceDifference
{
    std::uint32_t leftHash = 2166136261u;
    std::uint32_t rightHash = 2166136261u;
    UINT samples = 0;
    UINT differentSamples = 0;
    UINT differentTileMask = 0;
    UINT differentTiles = 0;
};

UINT countSetBits16(UINT value)
{
    value &= 0xffffu;
    UINT count = 0;
    while (value)
    {
        value &= value - 1u;
        ++count;
    }
    return count;
}

SurfaceDifference compareReadbackSurfacesSized(
    IDirect3DSurface9* left,
    IDirect3DSurface9* right,
    UINT width,
    UINT height,
    UINT bytesPerPixel)
{
    SurfaceDifference difference {};
    if (!left || !right || width == 0 || height == 0)
        return difference;

    // Shared VR publication is canonical BGRA8. Other formats require an
    // explicit conversion path; byte-wise comparisons of packed/float/XRGB
    // surfaces are not semantic stereo evidence.
    if (bytesPerPixel != 4)
        return difference;
    const UINT safeBytesPerPixel = 4;
    const int meaningfulRgbDelta = static_cast<int>(
        readEnvFloat("FNVXR_D3D9_STEREO_MIN_RGB_DELTA", 4.0f));
    D3DLOCKED_RECT leftLocked {};
    D3DLOCKED_RECT rightLocked {};
    if (FAILED(left->LockRect(&leftLocked, nullptr, D3DLOCK_READONLY)))
        return difference;
    if (FAILED(right->LockRect(&rightLocked, nullptr, D3DLOCK_READONLY)))
    {
        left->UnlockRect();
        return difference;
    }

    const auto* leftRows = static_cast<const std::uint8_t*>(leftLocked.pBits);
    const auto* rightRows = static_cast<const std::uint8_t*>(rightLocked.pBits);
    const UINT xStride = static_cast<UINT>(readEnvFloat("FNVXR_D3D9_STEREO_DIFF_X_STRIDE", 8.0f));
    const UINT yStride = static_cast<UINT>(readEnvFloat("FNVXR_D3D9_STEREO_DIFF_Y_STRIDE", 8.0f));
    const UINT safeXStride = xStride == 0 ? 1 : xStride;
    const UINT safeYStride = yStride == 0 ? 1 : yStride;
    for (UINT y = 0; y < height; y += safeYStride)
    {
        const auto* leftRow = leftRows + static_cast<size_t>(y) * leftLocked.Pitch;
        const auto* rightRow = rightRows + static_cast<size_t>(y) * rightLocked.Pitch;
        for (UINT x = 0; x < width; x += safeXStride)
        {
            const auto* leftPixel = leftRow + static_cast<size_t>(x) * safeBytesPerPixel;
            const auto* rightPixel = rightRow + static_cast<size_t>(x) * safeBytesPerPixel;
            int maxRgbDelta = 0;
            for (UINT b = 0; b < 3; ++b)
            {
                const std::uint8_t leftValue = leftPixel[b];
                const std::uint8_t rightValue = rightPixel[b];
                difference.leftHash ^= leftValue;
                difference.leftHash *= 16777619u;
                difference.rightHash ^= rightValue;
                difference.rightHash *= 16777619u;
                maxRgbDelta = (std::max)(maxRgbDelta, std::abs(
                    static_cast<int>(leftValue) - static_cast<int>(rightValue)));
            }

            ++difference.samples;
            if (maxRgbDelta >= meaningfulRgbDelta)
            {
                ++difference.differentSamples;
                const UINT tileX = (std::min)(3u, x * 4u / width);
                const UINT tileY = (std::min)(3u, y * 4u / height);
                difference.differentTileMask |= 1u << (tileY * 4u + tileX);
            }
        }
    }

    right->UnlockRect();
    left->UnlockRect();
    difference.differentTiles = countSetBits16(difference.differentTileMask);
    return difference;
}

bool spatialStereoDifferenceAcceptable(
    const SurfaceDifference& difference,
    UINT requiredDifferentSamples)
{
    const UINT minDifferentTiles = static_cast<UINT>(readEnvFloat(
        "FNVXR_D3D9_STEREO_MIN_DIFFERENT_TILES", 8.0f));
    return difference.differentSamples >= requiredDifferentSamples
        && difference.differentTiles >= minDifferentTiles;
}

struct EyeVisualCoverage
{
    UINT samples = 0;
    UINT activeSamples = 0;
    UINT dominantSamples = 0;
    UINT dominantBucket = 0;
    UINT minX = 0;
    UINT maxX = 0;
    UINT minY = 0;
    UINT maxY = 0;
    UINT activeTileMask = 0;
    UINT activeTiles = 0;
};

struct StereoVisualCoverage
{
    EyeVisualCoverage left {};
    EyeVisualCoverage right {};
};

UINT quantizedColorBucket(std::uint32_t pixel)
{
    const UINT blue = (pixel >> 4) & 0x0f;
    const UINT green = (pixel >> 12) & 0x0f;
    const UINT red = (pixel >> 20) & 0x0f;
    return (red << 8) | (green << 4) | blue;
}

EyeVisualCoverage analyzeEyeVisualCoverage(IDirect3DSurface9* surface)
{
    EyeVisualCoverage coverage {};
    if (!surface || gStereoTargetWidth == 0 || gStereoTargetHeight == 0)
        return coverage;

    D3DLOCKED_RECT locked {};
    if (FAILED(surface->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
        return coverage;

    UINT histogram[4096] {};
    const UINT xStride = static_cast<UINT>(readEnvFloat("FNVXR_D3D9_STEREO_COVERAGE_X_STRIDE", 16.0f));
    const UINT yStride = static_cast<UINT>(readEnvFloat("FNVXR_D3D9_STEREO_COVERAGE_Y_STRIDE", 16.0f));
    const UINT safeXStride = xStride == 0 ? 1 : xStride;
    const UINT safeYStride = yStride == 0 ? 1 : yStride;
    const auto* rows = static_cast<const std::uint8_t*>(locked.pBits);
    for (UINT y = 0; y < gStereoTargetHeight; y += safeYStride)
    {
        const auto* row = rows + static_cast<size_t>(y) * locked.Pitch;
        for (UINT x = 0; x < gStereoTargetWidth; x += safeXStride)
        {
            std::uint32_t pixel = 0;
            std::memcpy(&pixel, row + static_cast<size_t>(x) * 4, sizeof(pixel));
            const UINT bucket = quantizedColorBucket(pixel);
            ++histogram[bucket];
            ++coverage.samples;
        }
    }

    UINT dominantBucket = 0;
    UINT dominantSamples = 0;
    for (UINT bucket = 0; bucket < 4096; ++bucket)
    {
        if (histogram[bucket] > dominantSamples)
        {
            dominantSamples = histogram[bucket];
            dominantBucket = bucket;
        }
    }
    coverage.dominantBucket = dominantBucket;
    coverage.dominantSamples = dominantSamples;
    coverage.minX = gStereoTargetWidth;
    coverage.minY = gStereoTargetHeight;

    for (UINT y = 0; y < gStereoTargetHeight; y += safeYStride)
    {
        const auto* row = rows + static_cast<size_t>(y) * locked.Pitch;
        for (UINT x = 0; x < gStereoTargetWidth; x += safeXStride)
        {
            std::uint32_t pixel = 0;
            std::memcpy(&pixel, row + static_cast<size_t>(x) * 4, sizeof(pixel));
            if (quantizedColorBucket(pixel) == dominantBucket)
                continue;

            ++coverage.activeSamples;
            const UINT tileX = (std::min)(3u, x * 4u / gStereoTargetWidth);
            const UINT tileY = (std::min)(3u, y * 4u / gStereoTargetHeight);
            coverage.activeTileMask |= 1u << (tileY * 4u + tileX);
            if (x < coverage.minX)
                coverage.minX = x;
            if (x > coverage.maxX)
                coverage.maxX = x;
            if (y < coverage.minY)
                coverage.minY = y;
            if (y > coverage.maxY)
                coverage.maxY = y;
        }
    }

    surface->UnlockRect();
    if (coverage.activeSamples == 0)
    {
        coverage.minX = 0;
        coverage.minY = 0;
        coverage.maxX = 0;
        coverage.maxY = 0;
    }
    coverage.activeTiles = countSetBits16(coverage.activeTileMask);
    return coverage;
}

StereoVisualCoverage analyzeStereoVisualCoverage(IDirect3DSurface9* left, IDirect3DSurface9* right)
{
    StereoVisualCoverage coverage {};
    coverage.left = analyzeEyeVisualCoverage(left);
    coverage.right = analyzeEyeVisualCoverage(right);
    return coverage;
}

bool eyeVisualCoverageAcceptable(
    const EyeVisualCoverage& coverage,
    float minActiveFraction,
    float minSpanX,
    float minSpanY,
    UINT minActiveTiles)
{
    if (coverage.samples == 0 || gStereoTargetWidth == 0 || gStereoTargetHeight == 0)
        return false;

    const float activeFraction =
        static_cast<float>(coverage.activeSamples) / static_cast<float>(coverage.samples);
    const UINT spanX = coverage.activeSamples == 0 ? 0 : (coverage.maxX - coverage.minX + 1);
    const UINT spanY = coverage.activeSamples == 0 ? 0 : (coverage.maxY - coverage.minY + 1);
    const float spanXF = static_cast<float>(spanX) / static_cast<float>(gStereoTargetWidth);
    const float spanYF = static_cast<float>(spanY) / static_cast<float>(gStereoTargetHeight);
    return activeFraction >= minActiveFraction
        && spanXF >= minSpanX
        && spanYF >= minSpanY
        && coverage.activeTiles >= minActiveTiles;
}

bool stereoVisualCoverageAcceptable(const StereoVisualCoverage& coverage)
{
    const float minActiveFraction = readEnvFloat("FNVXR_D3D9_STEREO_MIN_ACTIVE_FRACTION", 0.12f);
    const float minSpanX = readEnvFloat("FNVXR_D3D9_STEREO_MIN_ACTIVE_SPAN_X", 0.35f);
    const float minSpanY = readEnvFloat("FNVXR_D3D9_STEREO_MIN_ACTIVE_SPAN_Y", 0.35f);
    const UINT minActiveTiles = static_cast<UINT>(readEnvFloat(
        "FNVXR_D3D9_STEREO_MIN_ACTIVE_TILES", 12.0f));
    return eyeVisualCoverageAcceptable(
            coverage.left, minActiveFraction, minSpanX, minSpanY, minActiveTiles)
        && eyeVisualCoverageAcceptable(
            coverage.right, minActiveFraction, minSpanX, minSpanY, minActiveTiles);
}

void logStereoVisualCoverageRejected(const StereoVisualCoverage& coverage)
{
    const LONG count = InterlockedIncrement(&gLoggedStereoVisualCoverageRejected);
    if (count <= 16 || count % 120 == 0)
    {
        char message[512] {};
        sprintf_s(
            message,
            "shared stereo visual coverage rejected count=%ld frame=%ld leftActive=%u/%u leftTiles=%u/16 leftDominant=%u bucket=0x%03x leftBox=(%u,%u)-(%u,%u) rightActive=%u/%u rightTiles=%u/16 rightDominant=%u bucket=0x%03x rightBox=(%u,%u)-(%u,%u)",
            count,
            static_cast<LONG>(gPresentFrames),
            coverage.left.activeSamples,
            coverage.left.samples,
            coverage.left.activeTiles,
            coverage.left.dominantSamples,
            coverage.left.dominantBucket,
            coverage.left.minX,
            coverage.left.minY,
            coverage.left.maxX,
            coverage.left.maxY,
            coverage.right.activeSamples,
            coverage.right.samples,
            coverage.right.activeTiles,
            coverage.right.dominantSamples,
            coverage.right.dominantBucket,
            coverage.right.minX,
            coverage.right.minY,
            coverage.right.maxX,
            coverage.right.maxY);
        logLine(message);
    }
}

SurfaceDifference compareReadbackSurfaces(IDirect3DSurface9* left, IDirect3DSurface9* right)
{
    return compareReadbackSurfacesSized(
        left,
        right,
        gStereoTargetWidth,
        gStereoTargetHeight,
        bytesPerPixelForFormat(gStereoTargetFormat));
}

bool ensureNativeResolvedReadbacks(IDirect3DDevice9* device)
{
    StereoSurfaceTwin* leftTwin = gNativeResolvedTwinForEye[0];
    StereoSurfaceTwin* rightTwin = gNativeResolvedTwinForEye[1];
    IDirect3DSurface9* left = leftTwin ? leftTwin->left : nullptr;
    IDirect3DSurface9* right = rightTwin ? rightTwin->right : nullptr;
    if (!device || !left || !right)
        return false;

    D3DSURFACE_DESC leftDesc {};
    D3DSURFACE_DESC rightDesc {};
    if (FAILED(left->GetDesc(&leftDesc))
        || FAILED(right->GetDesc(&rightDesc))
        || leftDesc.Width != rightDesc.Width
        || leftDesc.Height != rightDesc.Height
        || leftDesc.Format != rightDesc.Format
        || leftDesc.MultiSampleType != D3DMULTISAMPLE_NONE
        || rightDesc.MultiSampleType != D3DMULTISAMPLE_NONE)
    {
        return false;
    }

    bool existingMatches = gNativeResolvedReadback[0] && gNativeResolvedReadback[1];
    if (existingMatches)
    {
        D3DSURFACE_DESC existingDesc {};
        existingMatches = SUCCEEDED(gNativeResolvedReadback[0]->GetDesc(&existingDesc))
            && existingDesc.Width == leftDesc.Width
            && existingDesc.Height == leftDesc.Height
            && existingDesc.Format == leftDesc.Format;
    }
    if (existingMatches)
    {
        gNativeResolvedDesc = leftDesc;
        return true;
    }

    releaseSurface(gNativeResolvedReadback[0]);
    releaseSurface(gNativeResolvedReadback[1]);
    if (FAILED(device->CreateOffscreenPlainSurface(
            leftDesc.Width,
            leftDesc.Height,
            leftDesc.Format,
            D3DPOOL_SYSTEMMEM,
            &gNativeResolvedReadback[0],
            nullptr))
        || FAILED(device->CreateOffscreenPlainSurface(
            leftDesc.Width,
            leftDesc.Height,
            leftDesc.Format,
            D3DPOOL_SYSTEMMEM,
            &gNativeResolvedReadback[1],
            nullptr)))
    {
        releaseSurface(gNativeResolvedReadback[0]);
        releaseSurface(gNativeResolvedReadback[1]);
        return false;
    }
    gNativeResolvedDesc = leftDesc;
    return true;
}

void auditNativeResolvedStereo(IDirect3DDevice9* device)
{
    if (!readEnvBool("FNVXR_D3D9_NATIVE_RESOLVED_AUDIT", true))
        return;
    const LONG pair = gNativeStereoRenderPairSequence;
    if (!nativeStereoPairNeedsWarmupAudit(4)
        && fnvxr::shared::sequencedValueBits(pair) % 120u != 0u)
        return;

    StereoSurfaceTwin* leftTwin = gNativeResolvedTwinForEye[0];
    StereoSurfaceTwin* rightTwin = gNativeResolvedTwinForEye[1];
    IDirect3DSurface9* left = leftTwin ? leftTwin->left : nullptr;
    IDirect3DSurface9* right = rightTwin ? rightTwin->right : nullptr;
    if (!ensureNativeResolvedReadbacks(device)
        || FAILED(device->GetRenderTargetData(left, gNativeResolvedReadback[0]))
        || FAILED(device->GetRenderTargetData(right, gNativeResolvedReadback[1])))
    {
        const LONG logCount = InterlockedIncrement(&gLoggedNativeResolvedAudit);
        if (logCount <= 12 || logCount % 120 == 0)
            logLine("native resolved-eye audit unavailable");
        return;
    }

    const SurfaceDifference difference = compareReadbackSurfacesSized(
        gNativeResolvedReadback[0],
        gNativeResolvedReadback[1],
        gNativeResolvedDesc.Width,
        gNativeResolvedDesc.Height,
        bytesPerPixelForFormat(gNativeResolvedDesc.Format));
    const LONG logCount = InterlockedIncrement(&gLoggedNativeResolvedAudit);
    char message[512] {};
    sprintf_s(
        message,
        "native resolved-eye audit count=%ld pair=%ld separated=%d diff=%u/%u size=%ux%u format=%lu bpp=%u leftHash=0x%08x rightHash=0x%08x",
        logCount,
        pair,
        difference.differentSamples > 0 ? 1 : 0,
        difference.differentSamples,
        difference.samples,
        gNativeResolvedDesc.Width,
        gNativeResolvedDesc.Height,
        static_cast<unsigned long>(gNativeResolvedDesc.Format),
        bytesPerPixelForFormat(gNativeResolvedDesc.Format),
        difference.leftHash,
        difference.rightHash);
    logLine(message);
}

void captureBestStereoSnapshot(
    IDirect3DDevice9* device,
    const SurfaceDifference& difference,
    LONG drawOrdinal)
{
    if (difference.differentSamples <= gBestStereoDiffThisPresent)
        return;
    if (!ensureBestStereoSnapshotTargets(device))
        return;
    if (!copyReadbackSurfacePixels(gLeftEyeReadback, gBestLeftEyeReadback)
        || !copyReadbackSurfacePixels(gRightEyeReadback, gBestRightEyeReadback))
    {
        logLine("best stereo snapshot copy failed");
        return;
    }

    gBestStereoDiffThisPresent = difference.differentSamples;
    gBestStereoSamplesThisPresent = difference.samples;
    gBestStereoLeftHashThisPresent = difference.leftHash;
    gBestStereoRightHashThisPresent = difference.rightHash;
    gHaveBestStereoSnapshotThisPresent = true;

    const LONG logCount = InterlockedIncrement(&gLoggedBestStereoSnapshot);
    if (logCount <= 20 || logCount % 300 == 0)
    {
        char message[384] {};
        sprintf_s(
            message,
            "best stereo snapshot frame=%ld draw=%ld diff=%u/%u leftHash=0x%08x rightHash=0x%08x vs=%p ps=%p vsHash=0x%08x psHash=0x%08x",
            static_cast<LONG>(gPresentFrames),
            drawOrdinal,
            difference.differentSamples,
            difference.samples,
            difference.leftHash,
            difference.rightHash,
            reinterpret_cast<void*>(gActiveVertexShader),
            reinterpret_cast<void*>(gActivePixelShader),
            gActiveVertexShaderHash,
            gActivePixelShaderHash);
        logLine(message);
    }
}

void captureBestStereoSnapshotFromProbe(
    IDirect3DDevice9* device,
    const SurfaceDifference& difference,
    LONG ordinal,
    const char* label)
{
    if (difference.differentSamples <= gBestStereoDiffThisPresent)
        return;
    if (gProbeStereoTargetWidth != gStereoTargetWidth
        || gProbeStereoTargetHeight != gStereoTargetHeight
        || gProbeStereoTargetFormat != gStereoTargetFormat)
    {
        return;
    }
    if (!ensureBestStereoSnapshotTargets(device))
        return;
    if (!copyReadbackSurfacePixels(gProbeLeftEyeReadback, gBestLeftEyeReadback)
        || !copyReadbackSurfacePixels(gProbeRightEyeReadback, gBestRightEyeReadback))
    {
        logLine("best stereo probe snapshot copy failed");
        return;
    }

    gBestStereoDiffThisPresent = difference.differentSamples;
    gBestStereoSamplesThisPresent = difference.samples;
    gBestStereoLeftHashThisPresent = difference.leftHash;
    gBestStereoRightHashThisPresent = difference.rightHash;
    gHaveBestStereoSnapshotThisPresent = true;

    const LONG logCount = InterlockedIncrement(&gLoggedBestStereoSnapshot);
    if (logCount <= 20 || logCount % 300 == 0)
    {
        char message[416] {};
        sprintf_s(
            message,
            "best stereo probe snapshot label=%s frame=%ld ordinal=%ld diff=%u/%u leftHash=0x%08x rightHash=0x%08x vsHash=0x%08x psHash=0x%08x",
            label ? label : "unknown",
            static_cast<LONG>(gPresentFrames),
            ordinal,
            difference.differentSamples,
            difference.samples,
            difference.leftHash,
            difference.rightHash,
            gActiveVertexShaderHash,
            gActivePixelShaderHash);
        logLine(message);
    }
}

void probeStereoSurfacePair(
    IDirect3DDevice9* device,
    StereoSurfaceTwin* twin,
    const char* label,
    LONG ordinal)
{
    if (!readEnvBool("FNVXR_D3D9_STEREO_TARGET_DIFF_PROBE", false))
        return;
    if (!device || !twin || twin->depth || !twin->left || !twin->right)
        return;
    const D3DSURFACE_DESC& desc = twin->desc;
    if (desc.MultiSampleType != D3DMULTISAMPLE_NONE)
        return;
    if (!ensureProbeReadbackTargets(device, desc))
        return;

    const HRESULT leftResult = device->GetRenderTargetData(twin->left, gProbeLeftEyeReadback);
    const HRESULT rightResult = SUCCEEDED(leftResult)
        ? device->GetRenderTargetData(twin->right, gProbeRightEyeReadback)
        : leftResult;
    if (FAILED(leftResult) || FAILED(rightResult))
    {
        const LONG failLog = InterlockedIncrement(&gLoggedStereoProbeReadbackFailed);
        if (failLog <= 12 || failLog % 120 == 0)
        {
            char message[320] {};
            sprintf_s(
                message,
                "stereo target probe readback failed count=%ld label=%s frame=%ld ordinal=%ld size=%ux%u fmt=%lu result=0x%08lx/0x%08lx",
                failLog,
                label ? label : "unknown",
                static_cast<LONG>(gPresentFrames),
                ordinal,
                desc.Width,
                desc.Height,
                static_cast<unsigned long>(desc.Format),
                static_cast<unsigned long>(leftResult),
                static_cast<unsigned long>(rightResult));
            logLine(message);
        }
        return;
    }

    const SurfaceDifference difference = compareReadbackSurfacesSized(
        gProbeLeftEyeReadback,
        gProbeRightEyeReadback,
        desc.Width,
        desc.Height,
        bytesPerPixelForFormat(desc.Format));
    if (difference.differentSamples > 0)
        captureBestStereoSnapshotFromProbe(device, difference, ordinal, label);

    const LONG logCount = InterlockedIncrement(&gLoggedStereoTargetProbeDiff);
    if (difference.differentSamples > 0 || logCount <= 80 || logCount % 240 == 0)
    {
        char message[512] {};
        sprintf_s(
            message,
            "stereo target probe diff count=%ld label=%s frame=%ld ordinal=%ld diff=%u/%u size=%ux%u fmt=%lu bpp=%u ms=%u leftHash=0x%08x rightHash=0x%08x vsHash=0x%08x psHash=0x%08x",
            logCount,
            label ? label : "unknown",
            static_cast<LONG>(gPresentFrames),
            ordinal,
            difference.differentSamples,
            difference.samples,
            desc.Width,
            desc.Height,
            static_cast<unsigned long>(desc.Format),
            bytesPerPixelForFormat(desc.Format),
            static_cast<unsigned>(desc.MultiSampleType),
            difference.leftHash,
            difference.rightHash,
            gActiveVertexShaderHash,
            gActivePixelShaderHash);
        logLine(message);
    }
}

void auditNativeFullSizeTargetCandidates(IDirect3DDevice9* device)
{
    if (!device || !gNativeStereoEnabled || gNativeStereoPairsThisPresent <= 0)
        return;
    const LONG pair = gNativeStereoRenderPairSequence;
    if (!gNativePipelineTraceThisPair
        && !nativeStereoPairNeedsWarmupAudit(4)
        && fnvxr::shared::sequencedValueBits(pair) % 120u != 0u)
        return;

    for (UINT index = 0; index < MaxNativeFullSizeTargetCandidates; ++index)
    {
        NativeFullSizeTargetCandidate& candidate = gNativeFullSizeTargetCandidates[index];
        const bool complete = candidate.original
            && candidate.twin
            && candidate.snapshotted[0]
            && candidate.snapshotted[1]
            && candidate.twin->left
            && candidate.twin->right;
        if (!complete)
            continue;

        HRESULT leftResult = D3DERR_INVALIDCALL;
        HRESULT rightResult = D3DERR_INVALIDCALL;
        SurfaceDifference difference {};
        const bool readbackEligible = candidate.desc.MultiSampleType == D3DMULTISAMPLE_NONE
            && ensureProbeReadbackTargets(device, candidate.desc);
        if (readbackEligible)
        {
            leftResult = device->GetRenderTargetData(candidate.twin->left, gProbeLeftEyeReadback);
            rightResult = SUCCEEDED(leftResult)
                ? device->GetRenderTargetData(candidate.twin->right, gProbeRightEyeReadback)
                : leftResult;
            if (SUCCEEDED(leftResult) && SUCCEEDED(rightResult))
            {
                difference = compareReadbackSurfacesSized(
                    gProbeLeftEyeReadback,
                    gProbeRightEyeReadback,
                    candidate.desc.Width,
                    candidate.desc.Height,
                    bytesPerPixelForFormat(candidate.desc.Format));
            }
        }

        const bool nativeResolved = candidate.twin == gNativeResolvedTwinForEye[0]
            || candidate.twin == gNativeResolvedTwinForEye[1];
        char message[768] {};
        sprintf_s(
            message,
            "native full-size target audit pair=%ld candidate=%u original=%p resolved=%d textureBacked=%d seen=%d/%d snapshot=%d/%d lastSet=%ld/%ld size=%ux%u format=%lu usage=0x%lx ms=%u readback=0x%08lx/0x%08lx separated=%d diff=%u/%u leftHash=0x%08x rightHash=0x%08x",
            pair,
            index,
            reinterpret_cast<void*>(candidate.original),
            nativeResolved ? 1 : 0,
            candidate.twin->originalTexture ? 1 : 0,
            candidate.seen[0] ? 1 : 0,
            candidate.seen[1] ? 1 : 0,
            candidate.snapshotted[0] ? 1 : 0,
            candidate.snapshotted[1] ? 1 : 0,
            candidate.lastSetOrdinal[0],
            candidate.lastSetOrdinal[1],
            candidate.desc.Width,
            candidate.desc.Height,
            static_cast<unsigned long>(candidate.desc.Format),
            static_cast<unsigned long>(candidate.desc.Usage),
            static_cast<unsigned>(candidate.desc.MultiSampleType),
            static_cast<unsigned long>(leftResult),
            static_cast<unsigned long>(rightResult),
            difference.differentSamples > 0 ? 1 : 0,
            difference.differentSamples,
            difference.samples,
            difference.leftHash,
            difference.rightHash);
        logLine(message);
    }
}

void auditStereoCollapseAfterDraw(
    IDirect3DDevice9* device,
    LONG drawOrdinal,
    bool userPrimitiveDraw)
{
    if (!readEnvBool("FNVXR_D3D9_STEREO_COLLAPSE_AUDIT", false))
        return;

    const LONG maxAuditDraws =
        static_cast<LONG>(readEnvFloat("FNVXR_D3D9_STEREO_COLLAPSE_AUDIT_DRAWS", 2048.0f));
    if (maxAuditDraws > 0 && drawOrdinal > maxAuditDraws)
        return;
    if (!ensureReadbackTargets(device))
        return;
    if (FAILED(device->GetRenderTargetData(gLeftEyeSurface, gLeftEyeReadback))
        || FAILED(device->GetRenderTargetData(gRightEyeSurface, gRightEyeReadback)))
    {
        logLine("stereo collapse audit readback failed");
        return;
    }

    const SurfaceDifference difference = compareReadbackSurfaces(gLeftEyeReadback, gRightEyeReadback);
    const UINT minDifferentSamples = static_cast<UINT>(readEnvFloat("FNVXR_D3D9_STEREO_MIN_DIFF_SAMPLES", 1.0f));
    const UINT requiredDifferentSamples = minDifferentSamples == 0 ? 1 : minDifferentSamples;
    const bool wasSeparated = gStereoCollapseAuditPrevDiff >= requiredDifferentSamples;
    const bool isSeparated = spatialStereoDifferenceAcceptable(difference, requiredDifferentSamples);

    const LONG sampleStride =
        static_cast<LONG>(readEnvFloat("FNVXR_D3D9_STEREO_COLLAPSE_AUDIT_LOG_STRIDE", 128.0f));
    const LONG safeSampleStride = sampleStride <= 0 ? 1 : sampleStride;
    if (drawOrdinal <= 12 || (drawOrdinal % safeSampleStride) == 0 || (wasSeparated && !isSeparated))
    {
        const LONG logCount = InterlockedIncrement(&gLoggedStereoCollapseAudit);
        if (drawOrdinal <= 12 || (wasSeparated && !isSeparated) || logCount <= 80 || logCount % 400 == 0)
        {
            char message[640] {};
            sprintf_s(
                message,
                "stereo collapse audit frame=%ld draw=%ld prevDraw=%ld collapsed=%d diff=%u/%u prevDiff=%u/%u leftHash=0x%08x rightHash=0x%08x prevHash=0x%08x/0x%08x userPrimitive=%d projScreen=%d viewScreen=%d worldCandidate=%d vs=%p ps=%p vsHash=0x%08x psHash=0x%08x",
                static_cast<LONG>(gPresentFrames),
                drawOrdinal,
                gStereoCollapseAuditPrevDraw,
                (wasSeparated && !isSeparated) ? 1 : 0,
                difference.differentSamples,
                difference.samples,
                gStereoCollapseAuditPrevDiff,
                gStereoCollapseAuditPrevSamples,
                difference.leftHash,
                difference.rightHash,
                gStereoCollapseAuditPrevLeftHash,
                gStereoCollapseAuditPrevRightHash,
                userPrimitiveDraw ? 1 : 0,
                currentProjectionLooksScreenSpace() ? 1 : 0,
                currentViewLooksScreenSpace() ? 1 : 0,
                haveWorldCameraCandidate() ? 1 : 0,
                reinterpret_cast<void*>(gActiveVertexShader),
                reinterpret_cast<void*>(gActivePixelShader),
                gActiveVertexShaderHash,
                gActivePixelShaderHash);
            logLine(message);
        }
    }
    if (wasSeparated && !isSeparated)
        learnStereoCollapseShaderPair(drawOrdinal);

    gStereoCollapseAuditPrevDiff = difference.differentSamples;
    gStereoCollapseAuditPrevSamples = difference.samples;
    gStereoCollapseAuditPrevLeftHash = difference.leftHash;
    gStereoCollapseAuditPrevRightHash = difference.rightHash;
    gStereoCollapseAuditPrevDraw = drawOrdinal;
}

void readbackStereoTargets(IDirect3DDevice9* device)
{
    if (!gStereoReadbackEnabled || !ensureStereoTargets(device) || !ensureReadbackTargets(device))
        return;

    const LONG frame = static_cast<LONG>(gPresentFrames);
    if (gLastReadbackFrame != 0 && frame - gLastReadbackFrame < 300)
        return;
    if (frame < 5)
        return;

    gLastReadbackFrame = frame;
    if (FAILED(device->GetRenderTargetData(gLeftEyeSurface, gLeftEyeReadback))
        || FAILED(device->GetRenderTargetData(gRightEyeSurface, gRightEyeReadback)))
    {
        logLine("stereo readback GetRenderTargetData failed");
        return;
    }

    const SurfaceDifference difference = compareReadbackSurfaces(gLeftEyeReadback, gRightEyeReadback);
    const UINT minDifferentSamples = static_cast<UINT>(readEnvFloat("FNVXR_D3D9_STEREO_MIN_DIFF_SAMPLES", 1.0f));
    const bool separated = spatialStereoDifferenceAcceptable(
        difference,
        minDifferentSamples == 0 ? 1 : minDifferentSamples);
    const bool worldCandidate = haveWorldCameraCandidate();
    const fnvxr::stereo::Matrix4& stereoBaseView = baseViewForStereo();
    char message[256] {};
    sprintf_s(
        message,
        "stereo readback frame=%ld worldCandidate=%d separated=%d diff=%u/%u baseViewT=(%.4f %.4f %.4f) leftHash=0x%08x rightHash=0x%08x",
        frame,
        worldCandidate ? 1 : 0,
        separated ? 1 : 0,
        difference.differentSamples,
        difference.samples,
        stereoBaseView.m[3][0],
        stereoBaseView.m[3][1],
        stereoBaseView.m[3][2],
        difference.leftHash,
        difference.rightHash);
    logLine(message);
}

bool copySurfaceToSharedPlane(IDirect3DSurface9* surface, std::uint8_t* dst, UINT width, UINT height, UINT pitchBytes)
{
    if (!surface || !dst || pitchBytes != width * 4)
        return false;
    D3DSURFACE_DESC desc {};
    if (FAILED(surface->GetDesc(&desc))
        || desc.Width != width
        || desc.Height != height
        || (desc.Format != D3DFMT_A8R8G8B8 && desc.Format != D3DFMT_X8R8G8B8))
    {
        return false;
    }
    D3DLOCKED_RECT locked {};
    if (FAILED(surface->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
        return false;
    if (locked.Pitch < static_cast<INT>(pitchBytes))
    {
        surface->UnlockRect();
        return false;
    }

    const auto* src = static_cast<const std::uint8_t*>(locked.pBits);
    for (UINT y = 0; y < height; ++y)
        std::memcpy(dst + static_cast<size_t>(y) * pitchBytes, src + static_cast<size_t>(y) * locked.Pitch, pitchBytes);

    surface->UnlockRect();
    return true;
}

void publishSharedStereoInvalid(bool uiActive, const char* reason)
{
    if (!sharedStereoEnabled() || !ensureSharedStereo())
        return;

    const bool clearOnUiInvalid = readEnvBool("FNVXR_D3D9_STEREO_CLEAR_ON_UI_INVALID", false);
    if (gHavePublishedValidStereoWorldFrame
        && readEnvBool("FNVXR_D3D9_STEREO_RETAIN_LAST_VALID_ON_INVALID", false)
        && (!uiActive || !clearOnUiInvalid))
    {
        const LONG retained = InterlockedIncrement(&gLoggedRetainedInvalidStereoWorld);
        if (retained <= 24 || retained % 300 == 0)
        {
            char message[320] {};
            sprintf_s(
                message,
                "shared stereo retained previous valid world frame=%ld reason=invalid-publish uiActive=%d invalidReason=%s",
                static_cast<LONG>(gPresentFrames),
                uiActive ? 1 : 0,
                reason ? reason : "unknown");
            logLine(message);
        }
        return;
    }

    if (uiActive && clearOnUiInvalid)
        gHavePublishedValidStereoWorldFrame = false;

    auto* header = reinterpret_cast<SharedStereoHeader*>(gSharedStereoView);
    writeInvalidSharedStereoRecord(header, uiActive);

    const LONG captures = InterlockedIncrement(&gSharedStereoCaptures);
    if (captures <= 3 || captures % 300 == 0)
    {
        char message[160] {};
        sprintf_s(
            message,
            "shared stereo invalidated frame=%ld uiActive=%d reason=%s seq=%ld",
            static_cast<LONG>(gPresentFrames),
            uiActive ? 1 : 0,
            reason ? reason : "unknown",
            header->sequence);
        logLine(message);
    }
}

void publishSharedWorldVideoFrame(IDirect3DDevice9* device)
{
    if (!gWideWorldReplayEnabled)
        return;

    if (suppressStereoForUiMode())
        return;

    if (gWideWorldReplayDrawsThisPresent <= 0)
    {
        const LONG logCount = InterlockedIncrement(&gLoggedWideWorldNoReplayDraws);
        if (logCount <= 5 || logCount % 300 == 0)
            logLine("shared wide world retained previous frame: no replay draws this present");
        return;
    }

    if (!ensureWideWorldTargets(device)
        || !ensureWideWorldReadback(device)
        || !ensureSharedWorldVideo())
    {
        return;
    }

    if (gStereoTargetWidth == 0
        || gStereoTargetHeight == 0
        || gStereoTargetWidth > SharedVideoMaxWidth
        || gStereoTargetHeight > SharedVideoMaxHeight)
    {
        return;
    }

    if (FAILED(device->GetRenderTargetData(gWideWorldSurface, gWideWorldReadback)))
    {
        logLine("shared wide world GetRenderTargetData failed");
        return;
    }

    const UINT pitchBytes = gStereoTargetWidth * 4;
    auto* header = reinterpret_cast<SharedVideoHeader*>(gSharedWorldVideoView);
    auto* pixels = gSharedWorldVideoView + sizeof(SharedVideoHeader);
    InterlockedExchange(&header->writing, 1);
    header->magic = SharedVideoMagic;
    header->width = static_cast<LONG>(gStereoTargetWidth);
    header->height = static_cast<LONG>(gStereoTargetHeight);
    header->pitchBytes = static_cast<LONG>(pitchBytes);
    header->format = static_cast<LONG>(gStereoTargetFormat);
    if (!copySurfaceToSharedPlane(gWideWorldReadback, pixels, gStereoTargetWidth, gStereoTargetHeight, pitchBytes))
    {
        header->width = 0;
        header->height = 0;
        header->pitchBytes = 0;
        header->format = 0;
        MemoryBarrier();
        fnvxr::shared::incrementNonzeroSharedCounter(header->sequence);
        MemoryBarrier();
        InterlockedExchange(&header->writing, 0);
        logLine("shared wide world copy failed; not publishing frame");
        return;
    }
    MemoryBarrier();
    fnvxr::shared::incrementNonzeroSharedCounter(header->sequence);
    MemoryBarrier();
    InterlockedExchange(&header->writing, 0);

    const LONG captures = InterlockedIncrement(&gSharedWorldVideoCaptures);
    if (captures <= 5 || captures % 300 == 0)
    {
        const float centerFovX =
            readEnvFloat("FNVXR_GAME_PLANE_CENTER_FOV_X_DEG", readEnvFloat("FNVXR_GAME_PLANE_CENTER_FOV_DEG", 95.0f));
        const float wideFovX =
            readEnvFloat("FNVXR_GAME_PLANE_SURROUND_WIDE_FOV_X_DEG", readEnvFloat("FNVXR_GAME_PLANE_SURROUND_WIDE_FOV_DEG", 130.0f));
        char message[320] {};
        sprintf_s(
            message,
            "shared wide world captured frame=%ld size=%ux%u seq=%ld replayDraws=%ld fovX=(%.3f->%.3f) cropX=%.5f",
            static_cast<LONG>(gPresentFrames),
            gStereoTargetWidth,
            gStereoTargetHeight,
            static_cast<LONG>(header->sequence),
            static_cast<LONG>(gWideWorldReplayDrawsThisPresent),
            centerFovX,
            wideFovX,
            fovCenterCropRatio(centerFovX, wideFovX));
        logLine(message);
    }
}

void publishSharedStereoFrame(IDirect3DDevice9* device)
{
    if (serviceRenderOwnerShutdown("render-thread-mismatch-before-publish"))
    {
        logLine("shared stereo publication aborted by render-thread violation");
        return;
    }
    if (!criticalDeviceHooksIntact(device))
    {
        gHavePublishedValidStereoWorldFrame = false;
        if (gSharedStereoView)
            writeInvalidSharedStereoRecord(
                reinterpret_cast<SharedStereoHeader*>(gSharedStereoView),
                false);
        logLine("shared stereo publication rejected: critical D3D9 hook integrity failed");
        return;
    }

    if (suppressStereoForUiMode())
    {
        InterlockedExchange(&gConsecutiveStereoVisualCoverageFrames, 0);
        publishSharedStereoInvalid(true, "ui");
        return;
    }

    const bool nativeRenderedPair = gNativeStereoEnabled
        && gNativeStereoPairsThisPresent > 0
        && gNativeStereoRenderedPoseValid;
    if (nativeRenderedPair)
        auditNativeResolvedStereo(device);
    const bool nativePair = nativeRenderedPair
        && gNativePostprocessFinalWritesThisPresent > 0;
    const bool singleTraversalPair = gNativeSingleTraversalReplayEnabled
        && gNativeSingleTraversalFramesThisPresent > 0
        && gNativeStereoRenderedPoseValid
        && gStereoReplayDrawsThisPresent > 0;
    const bool coherentPair = nativePair || singleTraversalPair;
    const bool producerEnabled = gNativeStereoEnabled
        || gNativeSingleTraversalReplayEnabled
        || gStereoReplayEnabled;
    if (!sharedStereoEnabled() || !producerEnabled || !gStereoReadbackEnabled)
    {
        const LONG logCount = InterlockedIncrement(&gLoggedStereoNoReplayDraws);
        if (logCount <= 3 || logCount % 300 == 0)
        {
            char message[256] {};
            sprintf_s(
                message,
                "shared stereo disabled; not publishing stereo world frame sharedStereo=%d nativeEnabled=%d replayEnabled=%d readbackEnabled=%d nativePairs=%ld nativeFanoutFinal=%ld replayDraws=%ld ui=%d",
                sharedStereoEnabled() ? 1 : 0,
                gNativeStereoEnabled ? 1 : 0,
                gStereoReplayEnabled ? 1 : 0,
                gStereoReadbackEnabled ? 1 : 0,
                static_cast<LONG>(gNativeStereoPairsThisPresent),
                static_cast<LONG>(gNativePostprocessFinalWritesThisPresent),
                static_cast<LONG>(gStereoReplayDrawsThisPresent),
                suppressStereoForUiMode() ? 1 : 0);
            logLine(message);
        }
        return;
    }

    if (!fnvxr::stereo::singleTraversalPublishAllowed(
            gNativeSingleTraversalReplayEnabled,
            singleTraversalPair))
    {
        // Draw replay may still have populated eye targets before the native
        // transaction failed its camera/pose/shader proof. Never downgrade
        // those pixels to the generic producer: that made the host alternate
        // between coherent world stereo and an unowned frame as view-dependent
        // shaders entered or left the cull set.
        publishSharedStereoInvalid(false, "incomplete-single-traversal");
        return;
    }

    if (!coherentPair && gStereoReplayDrawsThisPresent <= 0)
    {
        const LONG logCount = InterlockedIncrement(&gLoggedStereoNoReplayDraws);
        if (logCount <= 5 || logCount % 300 == 0)
            logLine("shared stereo has no complete native pair or replay draws this present");
        if (gNativeStereoEnabled)
            publishSharedStereoInvalid(false, "no-native-pair");
        return;
    }

    if (!ensureStereoTargets(device)
        || !ensureReadbackTargets(device)
        || !ensureSharedStereo())
    {
        publishSharedStereoInvalid(false, "target-setup");
        return;
    }

    if (gStereoTargetWidth == 0
        || gStereoTargetHeight == 0
        || gStereoTargetWidth > SharedVideoMaxWidth
        || gStereoTargetHeight > SharedVideoMaxHeight)
    {
        publishSharedStereoInvalid(false, "invalid-dimensions");
        return;
    }

    LARGE_INTEGER stereoPublishStart {};
    LARGE_INTEGER leftReadbackEnd {};
    LARGE_INTEGER rightReadbackEnd {};
    LARGE_INTEGER stereoCopyStart {};
    LARGE_INTEGER stereoCopyEnd {};
    QueryPerformanceCounter(&stereoPublishStart);
    const HRESULT leftReadbackResult = device->GetRenderTargetData(gLeftEyeSurface, gLeftEyeReadback);
    QueryPerformanceCounter(&leftReadbackEnd);
    const HRESULT rightReadbackResult = SUCCEEDED(leftReadbackResult)
        ? device->GetRenderTargetData(gRightEyeSurface, gRightEyeReadback)
        : leftReadbackResult;
    QueryPerformanceCounter(&rightReadbackEnd);
    if (FAILED(leftReadbackResult) || FAILED(rightReadbackResult))
    {
        logLine("shared stereo GetRenderTargetData failed");
        publishSharedStereoInvalid(false, "readback-failed");
        return;
    }

    const SurfaceDifference finalDifference = compareReadbackSurfaces(gLeftEyeReadback, gRightEyeReadback);
    const UINT minDifferentSamples = static_cast<UINT>(readEnvFloat("FNVXR_D3D9_STEREO_MIN_DIFF_SAMPLES", 1.0f));
    const UINT requiredDifferentSamples = minDifferentSamples == 0 ? 1 : minDifferentSamples;
    const bool finalSeparated = spatialStereoDifferenceAcceptable(
        finalDifference,
        requiredDifferentSamples);
    const bool bestSnapshotBeatsFinal =
        !finalSeparated || gBestStereoDiffThisPresent > finalDifference.differentSamples;
    const bool publishBestSnapshot =
        !nativePair
        && readEnvBool("FNVXR_D3D9_STEREO_PUBLISH_BEST_SNAPSHOT", true)
        && bestSnapshotBeatsFinal
        && gHaveBestStereoSnapshotThisPresent
        && gBestStereoDiffThisPresent >= requiredDifferentSamples
        && gBestLeftEyeReadback
        && gBestRightEyeReadback;
    IDirect3DSurface9* publishLeft = publishBestSnapshot ? gBestLeftEyeReadback : gLeftEyeReadback;
    IDirect3DSurface9* publishRight = publishBestSnapshot ? gBestRightEyeReadback : gRightEyeReadback;
    SurfaceDifference difference = finalDifference;
    if (publishBestSnapshot)
    {
        difference.leftHash = gBestStereoLeftHashThisPresent;
        difference.rightHash = gBestStereoRightHashThisPresent;
        difference.samples = gBestStereoSamplesThisPresent;
        difference.differentSamples = gBestStereoDiffThisPresent;
    }
    const bool separated = spatialStereoDifferenceAcceptable(difference, requiredDifferentSamples);
    const bool worldCandidate = coherentPair ? true : haveWorldCameraCandidate();
    const bool publishBestSnapshotAsWorld =
        publishBestSnapshot && readEnvBool("FNVXR_D3D9_STEREO_BEST_SNAPSHOT_AS_WORLD", false);
    const bool publishIsDiagnosticSnapshot = publishBestSnapshot && !publishBestSnapshotAsWorld;
    const bool visualCoverageGate = readEnvBool("FNVXR_D3D9_STEREO_VISUAL_COVERAGE_GATE", true);
    bool visualCoverageReady = !visualCoverageGate;
    LONG visualCoverageFrames = 0;
    StereoVisualCoverage visualCoverage {};
    bool visualCoverageMeasured = false;
    if (visualCoverageGate && separated && worldCandidate && !publishIsDiagnosticSnapshot)
    {
        visualCoverage = analyzeStereoVisualCoverage(publishLeft, publishRight);
        visualCoverageMeasured = true;
        if (!stereoVisualCoverageAcceptable(visualCoverage))
        {
            InterlockedExchange(&gConsecutiveStereoVisualCoverageFrames, 0);
            logStereoVisualCoverageRejected(visualCoverage);
            if (gHavePublishedValidStereoWorldFrame
                && readEnvBool("FNVXR_D3D9_STEREO_RETAIN_LAST_VALID_ON_INVALID", false))
            {
                const LONG retained = InterlockedIncrement(&gLoggedRetainedInvalidStereoWorld);
                if (retained <= 24 || retained % 300 == 0)
                {
                    char message[384] {};
                    sprintf_s(
                        message,
                        "shared stereo retained previous valid world frame=%ld reason=visual-coverage bestSnapshot=%d finalSeparated=%d finalDiff=%u/%u",
                        static_cast<LONG>(gPresentFrames),
                        publishBestSnapshot ? 1 : 0,
                        finalSeparated ? 1 : 0,
                        finalDifference.differentSamples,
                        finalDifference.samples);
                    logLine(message);
                }
                return;
            }
            publishSharedStereoInvalid(false, "visual-coverage");
            return;
        }

        visualCoverageFrames = InterlockedIncrement(&gConsecutiveStereoVisualCoverageFrames);
        const LONG requiredVisualCoverageFrames =
            static_cast<LONG>(readEnvFloat("FNVXR_D3D9_STEREO_VISUAL_STABLE_FRAMES", 60.0f));
        visualCoverageReady =
            requiredVisualCoverageFrames <= 1 || visualCoverageFrames >= requiredVisualCoverageFrames;
        if (!visualCoverageReady)
        {
            const LONG warmupLogs = InterlockedIncrement(&gLoggedStereoVisualCoverageWarmup);
            if (warmupLogs <= 12 || visualCoverageFrames % 30 == 0)
            {
                char warmup[240] {};
                sprintf_s(
                    warmup,
                    "shared stereo visual coverage warmup frame=%ld consecutive=%ld/%ld",
                    static_cast<LONG>(gPresentFrames),
                    visualCoverageFrames,
                    requiredVisualCoverageFrames);
                logLine(warmup);
            }
        }
    }
    else if (coherentPair)
    {
        // Count consecutive verified producer transactions, not consecutive
        // Present calls. Fallout legitimately has Present calls with no native
        // scene traversal between them; clearing on those gaps made the 12-pair
        // handoff threshold unreachable and left the headset black forever.
        InterlockedExchange(&gConsecutiveStereoVisualCoverageFrames, 0);
    }

    const bool publishReady =
        separated
        && worldCandidate
        && !publishIsDiagnosticSnapshot
        && visualCoverageReady
        && (coherentPair ? gNativeStereoRenderedPoseValid : gLatestVrPoseSnapshotValid);
    if (!publishReady
        && gHavePublishedValidStereoWorldFrame
        && readEnvBool("FNVXR_D3D9_STEREO_RETAIN_LAST_VALID_ON_INVALID", false))
    {
        const LONG retained = InterlockedIncrement(&gLoggedRetainedInvalidStereoWorld);
        if (retained <= 24 || retained % 300 == 0)
        {
            char message[512] {};
            sprintf_s(
                message,
                "shared stereo retained previous valid world frame=%ld reason=invalid-current separated=%d worldCandidate=%d diagnostic=%d visualReady=%d rawPoseValid=%d diff=%u/%u finalDiff=%u/%u bestSnapshot=%d",
                static_cast<LONG>(gPresentFrames),
                separated ? 1 : 0,
                worldCandidate ? 1 : 0,
                publishIsDiagnosticSnapshot ? 1 : 0,
                visualCoverageReady ? 1 : 0,
                gLatestVrPoseSnapshotValid ? 1 : 0,
                difference.differentSamples,
                difference.samples,
                finalDifference.differentSamples,
                finalDifference.samples,
                publishBestSnapshot ? 1 : 0);
            logLine(message);
        }
        return;
    }

    const UINT pitchBytes = gStereoTargetWidth * 4;
    auto* header = reinterpret_cast<SharedStereoHeader*>(gSharedStereoView);
    constexpr std::uint32_t slotBytes =
        SharedVideoMaxWidth * SharedVideoMaxHeight * 4 * 2;
    const LONG publishedSlot = InterlockedCompareExchange(
        &header->publishedSlot, 0, 0);
    const LONG hostReaderSlot =
        InterlockedCompareExchange(
            &header->readerSlots[fnvxr::shared::D3D9StereoHostReaderLane], 0, 0);
    const LONG captureReaderSlot =
        InterlockedCompareExchange(
            &header->readerSlots[fnvxr::shared::D3D9StereoCaptureReaderLane], 0, 0);
    const LONG writeSlot = fnvxr::shared::selectWritableStereoFrameSlot(
        publishedSlot, hostReaderSlot, captureReaderSlot);
    if (writeSlot < 0)
    {
        writeInvalidSharedStereoRecord(header, false);
        logLine("shared stereo ring has no writable slot");
        return;
    }
    const std::uint32_t leftPayloadOffset = sizeof(SharedStereoHeader)
        + static_cast<std::uint32_t>(writeSlot) * slotBytes;
    const std::uint32_t rightPayloadOffset = leftPayloadOffset + pitchBytes * gStereoTargetHeight;
    auto* leftPixels = gSharedStereoView + leftPayloadOffset;
    auto* rightPixels = gSharedStereoView + rightPayloadOffset;
    const SharedVrPoseState& renderedPose = coherentPair
        ? gNativeStereoRenderedPoseSnapshot
        : gLatestVrPoseSnapshot;
    const LONG renderedPoseSequence = coherentPair
        ? gNativeStereoRenderedPoseSequence
        : gLatestVrPoseSnapshotSequence;
    const std::int64_t renderedDisplayTime = coherentPair
        ? gNativeStereoRenderedDisplayTime
        : gLatestVrPoseDisplayTime;
    const bool renderedPoseValid = coherentPair
        ? gNativeStereoRenderedPoseValid
        : gLatestVrPoseSnapshotValid;
    QueryPerformanceCounter(&stereoCopyStart);
    const bool copiedLeft =
        copySurfaceToSharedPlane(publishLeft, leftPixels, gStereoTargetWidth, gStereoTargetHeight, pitchBytes);
    const bool copiedRight =
        copySurfaceToSharedPlane(publishRight, rightPixels, gStereoTargetWidth, gStereoTargetHeight, pitchBytes);
    QueryPerformanceCounter(&stereoCopyEnd);
    if (!copiedLeft || !copiedRight)
    {
        // Keep ownership of the same transaction while replacing the partial
        // payload metadata with a complete invalid record.  There is never a
        // reader-visible old sequence with writing==0 between these steps.
        writeInvalidSharedStereoRecord(header, false);
        logLine("shared stereo copy failed; not publishing frame");
        return;
    }

    if (serviceRenderOwnerShutdown("render-thread-mismatch-after-copy"))
    {
        logLine("shared stereo commit aborted after payload copy by render-thread violation");
        return;
    }
    if (!criticalDeviceHooksIntact(device))
    {
        gHavePublishedValidStereoWorldFrame = false;
        writeInvalidSharedStereoRecord(header, false);
        logLine("shared stereo commit rejected after copy: critical D3D9 hook integrity failed");
        return;
    }

    // The large payload is now immutable in a slot that was neither published
    // nor reader-owned. Hold the header transaction only for this small
    // metadata commit, then atomically select the completed slot.
    InterlockedExchange(&header->writing, 1);
    MemoryBarrier();
    header->magic = SharedStereoMagic;
    header->version = fnvxr::shared::D3D9StereoFrameSharedVersion;
    header->headerBytes = sizeof(SharedStereoHeader);
    header->leftPayloadOffset = leftPayloadOffset;
    header->rightPayloadOffset = rightPayloadOffset;
    header->totalMappingBytes = sizeof(SharedStereoHeader)
        + slotBytes * fnvxr::shared::D3D9StereoFrameSlotCount;
    header->rendererProducerEpoch = gSharedStereoRendererProducerEpoch;
    header->producerProcessId = GetCurrentProcessId();
    header->width = static_cast<LONG>(gStereoTargetWidth);
    header->height = static_cast<LONG>(gStereoTargetHeight);
    header->pitchBytes = static_cast<LONG>(pitchBytes);
    header->format = static_cast<LONG>(gStereoTargetFormat);
    header->uiActive = suppressStereoForUiMode() ? 1 : 0;
    header->poseValid = 0;
    header->poseSequence = 0;
    header->renderedDisplayTime = 0;
    header->referenceSpaceGeneration = 0;
    header->producerEpoch = 0;
    header->producerMode = nativePair
        ? fnvxr::shared::StereoProducerNativeSameFrame
        : (singleTraversalPair
            ? fnvxr::shared::StereoProducerSingleTraversal
            : fnvxr::shared::StereoProducerDrawReplay);
    header->renderPairSequence = coherentPair ? gNativeStereoRenderPairSequence : 0;
    if (renderedPoseValid && renderedDisplayTime != 0)
    {
        header->referenceSpaceGeneration = renderedPose.referenceSpaceGeneration;
        header->producerEpoch = renderedPose.producerEpoch;
        std::memcpy(header->leftEyeRot, renderedPose.leftEyeRot, sizeof(header->leftEyeRot));
        std::memcpy(header->leftEyePos, renderedPose.leftEyePos, sizeof(header->leftEyePos));
        std::memcpy(header->rightEyeRot, renderedPose.rightEyeRot, sizeof(header->rightEyeRot));
        std::memcpy(header->rightEyePos, renderedPose.rightEyePos, sizeof(header->rightEyePos));
        std::memcpy(header->leftFov, renderedPose.leftFov, sizeof(header->leftFov));
        std::memcpy(header->rightFov, renderedPose.rightFov, sizeof(header->rightFov));
        header->poseSequence = renderedPoseSequence;
        header->renderedDisplayTime = renderedDisplayTime;
        header->poseValid = 1;
    }
    header->separated = separated ? 1 : 0;
    const fnvxr::stereo::Matrix4& stereoBaseView = baseViewForStereo();
    header->worldCandidate = (worldCandidate && !publishIsDiagnosticSnapshot && visualCoverageReady) ? 1 : 0;
    if (publishIsDiagnosticSnapshot || !visualCoverageReady)
        header->poseValid = 0;
    if (header->separated && header->worldCandidate && header->poseValid)
        gHavePublishedValidStereoWorldFrame = true;
    if (publishBestSnapshot)
    {
        char snapshotMessage[512] {};
        sprintf_s(
            snapshotMessage,
            "shared stereo publishing best separated snapshot frame=%ld diagnosticOnly=%d asWorld=%d visualReady=%d bestDiff=%u/%u finalDiff=%u/%u bestHash=0x%08x/0x%08x finalHash=0x%08x/0x%08x",
            static_cast<LONG>(gPresentFrames),
            publishIsDiagnosticSnapshot ? 1 : 0,
            publishBestSnapshotAsWorld ? 1 : 0,
            visualCoverageReady ? 1 : 0,
            gBestStereoDiffThisPresent,
            gBestStereoSamplesThisPresent,
            finalDifference.differentSamples,
            finalDifference.samples,
            gBestStereoLeftHashThisPresent,
            gBestStereoRightHashThisPresent,
            finalDifference.leftHash,
            finalDifference.rightHash);
        logLine(snapshotMessage);
    }
    if (separated || header->uiActive || !worldCandidate || !header->poseValid)
    {
        InterlockedExchange(&gConsecutiveIdenticalStereoWorldFrames, 0);
        if (separated)
            InterlockedExchange(&gStereoIdenticalDisarmed, 0);
    }
    else
    {
        const LONG identicalFrames = InterlockedIncrement(&gConsecutiveIdenticalStereoWorldFrames);
        const LONG disarmThreshold =
            static_cast<LONG>(readEnvFloat("FNVXR_D3D9_STEREO_IDENTICAL_DISARM_FRAMES", 120.0f));
        if (disarmThreshold > 0
            && identicalFrames >= disarmThreshold
            && InterlockedCompareExchange(&gStereoIdenticalDisarmed, 1, 0) == 0)
        {
            char disabled[240] {};
            sprintf_s(
                disabled,
                "stereo world sample rejected after %ld identical gameplay frames; replay/readback stay enabled for recovery",
                identicalFrames);
            logLine(disabled);
        }
    }
    if (!separated && InterlockedCompareExchange(&gStereoIdenticalDisarmed, 0, 0) != 0)
    {
        header->worldCandidate = 0;
        header->poseValid = 0;
    }
    AcquireSRWLockExclusive(&gRenderPublishCommitLock);
    if (InterlockedCompareExchange(&gRenderShutdownRequested, 0, 0) != 0)
    {
        ReleaseSRWLockExclusive(&gRenderPublishCommitLock);
        serviceRenderOwnerShutdown("render-thread-mismatch-before-commit");
        logLine("shared stereo commit aborted at final publication boundary by render-thread violation");
        return;
    }
    if (!criticalDeviceHooksIntact(device))
    {
        gHavePublishedValidStereoWorldFrame = false;
        writeInvalidSharedStereoRecord(header, false);
        ReleaseSRWLockExclusive(&gRenderPublishCommitLock);
        logLine("shared stereo commit rejected at publication boundary: critical D3D9 hook integrity failed");
        return;
    }
    MemoryBarrier();
    InterlockedExchange(&header->publishedSlot, writeSlot);
    MemoryBarrier();
    if (InterlockedIncrement64(&header->publicationGeneration) == 0)
        InterlockedIncrement64(&header->publicationGeneration);
    MemoryBarrier();
    fnvxr::shared::incrementNonzeroSharedCounter(header->sequence);
    MemoryBarrier();
    InterlockedExchange(&header->writing, 0);
    ReleaseSRWLockExclusive(&gRenderPublishCommitLock);

    const LONG captures = InterlockedIncrement(&gSharedStereoCaptures);
    const double leftReadbackMs = secondsBetween(stereoPublishStart, leftReadbackEnd) * 1000.0;
    const double rightReadbackMs = secondsBetween(leftReadbackEnd, rightReadbackEnd) * 1000.0;
    const double analysisMs = secondsBetween(rightReadbackEnd, stereoCopyStart) * 1000.0;
    const double copyMs = secondsBetween(stereoCopyStart, stereoCopyEnd) * 1000.0;
    const double totalMs = secondsBetween(stereoPublishStart, stereoCopyEnd) * 1000.0;
    const float slowMs = readEnvFloat("FNVXR_D3D9_SHARED_STEREO_SLOW_MS", 8.0f);
    if (captures <= 5 || captures % 300 == 0 || totalMs >= static_cast<double>(slowMs))
    {
        const LONG perfLogs = InterlockedIncrement(&gSharedStereoPerfLogs);
        if (perfLogs <= 24 || perfLogs % 120 == 0)
        {
            char perfMessage[448] {};
            sprintf_s(
                perfMessage,
                "shared stereo perf frame=%ld capture=%ld totalMs=%.3f leftReadbackMs=%.3f rightReadbackMs=%.3f analysisMs=%.3f copyMs=%.3f size=%ux%u slow=%d",
                static_cast<LONG>(gPresentFrames),
                captures,
                totalMs,
                leftReadbackMs,
                rightReadbackMs,
                analysisMs,
                copyMs,
                gStereoTargetWidth,
                gStereoTargetHeight,
                totalMs >= static_cast<double>(slowMs) ? 1 : 0);
            logLine(perfMessage);
        }
    }
    if (captures <= 3 || captures % 300 == 0)
    {
        char message[512] {};
        sprintf_s(
            message,
            "shared stereo captured frame=%ld size=%ux%u seq=%ld producerMode=%ld renderPair=%ld worldCandidate=%d rawWorldCandidate=%d separated=%d uiActive=%d poseValid=%d rawPoseValid=%d visualGate=%d visualReady=%d visualFrames=%ld bestSnapshot=%d bestAsWorld=%d finalSeparated=%d poseSeq=%ld diff=%u/%u tiles=%u/16 finalDiff=%u/%u finalTiles=%u/16 baseViewT=(%.4f %.4f %.4f) leftHash=0x%08x rightHash=0x%08x",
            static_cast<LONG>(gPresentFrames),
            gStereoTargetWidth,
            gStereoTargetHeight,
            static_cast<LONG>(header->sequence),
            static_cast<LONG>(header->producerMode),
            static_cast<LONG>(header->renderPairSequence),
            header->worldCandidate ? 1 : 0,
            worldCandidate ? 1 : 0,
            separated ? 1 : 0,
            header->uiActive ? 1 : 0,
            header->poseValid ? 1 : 0,
            gLatestVrPoseSnapshotValid ? 1 : 0,
            visualCoverageGate ? 1 : 0,
            visualCoverageReady ? 1 : 0,
            visualCoverageFrames,
            publishBestSnapshot ? 1 : 0,
            publishBestSnapshotAsWorld ? 1 : 0,
            finalSeparated ? 1 : 0,
            static_cast<LONG>(header->poseSequence),
            difference.differentSamples,
            difference.samples,
            difference.differentTiles,
            finalDifference.differentSamples,
            finalDifference.samples,
            finalDifference.differentTiles,
            stereoBaseView.m[3][0],
            stereoBaseView.m[3][1],
            stereoBaseView.m[3][2],
            difference.leftHash,
            difference.rightHash);
        logLine(message);
    }
    const LONG eyeTargetLogCount = InterlockedIncrement(&gLoggedD3d9EyeTarget);
    if (telemetryHammerEnabled() || eyeTargetLogCount <= 20 || eyeTargetLogCount % 300 == 0)
    {
        const float leftActiveFraction = visualCoverage.left.samples == 0
            ? 0.0f
            : static_cast<float>(visualCoverage.left.activeSamples)
                / static_cast<float>(visualCoverage.left.samples);
        const float rightActiveFraction = visualCoverage.right.samples == 0
            ? 0.0f
            : static_cast<float>(visualCoverage.right.activeSamples)
                / static_cast<float>(visualCoverage.right.samples);
        const float leftDominantFraction = visualCoverage.left.samples == 0
            ? 0.0f
            : static_cast<float>(visualCoverage.left.dominantSamples)
                / static_cast<float>(visualCoverage.left.samples);
        const float rightDominantFraction = visualCoverage.right.samples == 0
            ? 0.0f
            : static_cast<float>(visualCoverage.right.dominantSamples)
                / static_cast<float>(visualCoverage.right.samples);
        char proof[1792] {};
        sprintf_s(
            proof,
            "{\"event\":\"fnvxrD3d9EyeTarget\",\"stage\":\"shared-stereo-publish\",\"frame\":%ld,\"ready\":%s,\"diagnosticSnapshot\":%s,\"bestSnapshot\":%s,\"bestSnapshotAsWorld\":%s,\"finalSeparated\":%s,\"leftRendered\":true,\"rightRendered\":true,\"width\":%u,\"height\":%u,\"format\":%lu,\"sequence\":%ld,\"producerMode\":%ld,\"renderPairSequence\":%ld,\"referenceSpaceGeneration\":%lu,\"poseProducerEpoch\":\"%llu\",\"rendererProducerEpoch\":\"%llu\",\"producerProcessId\":%lu,\"renderedDisplayTime\":%lld,\"worldCandidate\":%s,\"rawWorldCandidate\":%s,\"separated\":%s,\"uiActive\":%s,\"poseValid\":%s,\"rawPoseValid\":%s,\"visualCoverageGate\":%s,\"visualCoverageReady\":%s,\"visualCoverageMeasured\":%s,\"visualCoverageFrames\":%ld,\"leftActiveFraction\":%.6f,\"rightActiveFraction\":%.6f,\"leftActiveTiles\":%u,\"rightActiveTiles\":%u,\"leftDominantFraction\":%.6f,\"rightDominantFraction\":%.6f,\"leftDominantBucket\":%u,\"rightDominantBucket\":%u,\"poseSequence\":%ld,\"diffSamples\":%u,\"samples\":%u,\"differentTiles\":%u,\"finalDiffSamples\":%u,\"finalSamples\":%u,\"finalDifferentTiles\":%u,\"leftHash\":\"0x%08x\",\"rightHash\":\"0x%08x\",\"finalLeftHash\":\"0x%08x\",\"finalRightHash\":\"0x%08x\"}",
            static_cast<LONG>(gPresentFrames),
            (separated && header->worldCandidate && !header->uiActive && header->poseValid) ? "true" : "false",
            publishIsDiagnosticSnapshot ? "true" : "false",
            publishBestSnapshot ? "true" : "false",
            publishBestSnapshotAsWorld ? "true" : "false",
            finalSeparated ? "true" : "false",
            gStereoTargetWidth,
            gStereoTargetHeight,
            static_cast<unsigned long>(gStereoTargetFormat),
            static_cast<LONG>(header->sequence),
            static_cast<LONG>(header->producerMode),
            static_cast<LONG>(header->renderPairSequence),
            static_cast<unsigned long>(header->referenceSpaceGeneration),
            static_cast<unsigned long long>(header->producerEpoch),
            static_cast<unsigned long long>(header->rendererProducerEpoch),
            static_cast<unsigned long>(header->producerProcessId),
            static_cast<long long>(header->renderedDisplayTime),
            header->worldCandidate ? "true" : "false",
            worldCandidate ? "true" : "false",
            separated ? "true" : "false",
            header->uiActive ? "true" : "false",
            header->poseValid ? "true" : "false",
            gLatestVrPoseSnapshotValid ? "true" : "false",
            visualCoverageGate ? "true" : "false",
            visualCoverageReady ? "true" : "false",
            visualCoverageMeasured ? "true" : "false",
            visualCoverageFrames,
            leftActiveFraction,
            rightActiveFraction,
            visualCoverage.left.activeTiles,
            visualCoverage.right.activeTiles,
            leftDominantFraction,
            rightDominantFraction,
            visualCoverage.left.dominantBucket,
            visualCoverage.right.dominantBucket,
            static_cast<LONG>(header->poseSequence),
            difference.differentSamples,
            difference.samples,
            difference.differentTiles,
            finalDifference.differentSamples,
            finalDifference.samples,
            finalDifference.differentTiles,
            difference.leftHash,
            difference.rightHash,
            finalDifference.leftHash,
            finalDifference.rightHash);
        logLine(proof);
    }
}

bool strictStereoTargetGateEnabled()
{
    return readEnvBool("FNVXR_D3D9_STEREO_STRICT_TARGET_GATE", true);
}

bool configuredStereoDimensions(IDirect3DDevice9* device, UINT& width, UINT& height)
{
    if (gStereoTargetWidth != 0 && gStereoTargetHeight != 0)
    {
        width = gStereoTargetWidth;
        height = gStereoTargetHeight;
        return true;
    }

    if (!device)
        return false;

    IDirect3DSurface9* backBuffer = nullptr;
    if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)) || !backBuffer)
        return false;

    D3DSURFACE_DESC desc {};
    const HRESULT descResult = backBuffer->GetDesc(&desc);
    releaseSurface(backBuffer);
    if (FAILED(descResult) || desc.Width == 0 || desc.Height == 0)
        return false;

    width = desc.Width;
    height = desc.Height;
    return true;
}

bool sameComIdentity(IUnknown* left, IUnknown* right)
{
    if (!left || !right)
        return false;
    IUnknown* leftIdentity = nullptr;
    IUnknown* rightIdentity = nullptr;
    const HRESULT leftResult = left->QueryInterface(
        __uuidof(IUnknown),
        reinterpret_cast<void**>(&leftIdentity));
    const HRESULT rightResult = right->QueryInterface(
        __uuidof(IUnknown),
        reinterpret_cast<void**>(&rightIdentity));
    const bool same = SUCCEEDED(leftResult)
        && SUCCEEDED(rightResult)
        && leftIdentity == rightIdentity;
    if (leftIdentity)
        leftIdentity->Release();
    if (rightIdentity)
        rightIdentity->Release();
    return same;
}

void logStrictStereoTargetGate(
    const char* context,
    const char* reason,
    const D3DSURFACE_DESC* targetDesc,
    const D3DSURFACE_DESC* depthDesc,
    UINT configuredWidth,
    UINT configuredHeight)
{
    const LONG logCount = InterlockedIncrement(&gLoggedStrictStereoTargetGate);
    const bool hammerLog =
        shouldTelemetryHammerLog(logCount, "FNVXR_D3D9_STRICT_TARGET_GATE_TELEMETRY_STRIDE", 1);
    if (logCount > 48 && logCount % 300 != 0 && !hammerLog)
        return;

    char message[768] {};
    sprintf_s(
        message,
        "{\"event\":\"fnvxrD3d9StrictTargetGate\",\"frame\":%ld,\"count\":%ld,"
        "\"context\":\"%s\",\"reason\":\"%s\",\"configured\":[%u,%u],"
        "\"target\":[%u,%u,%lu],\"depth\":[%u,%u,%lu],"
        "\"active\":%d,\"ready\":%d,\"replayDraws\":%ld}",
        static_cast<LONG>(gPresentFrames),
        logCount,
        context ? context : "unknown",
        reason ? reason : "unknown",
        configuredWidth,
        configuredHeight,
        targetDesc ? targetDesc->Width : 0,
        targetDesc ? targetDesc->Height : 0,
        targetDesc ? static_cast<unsigned long>(targetDesc->Format) : 0,
        depthDesc ? depthDesc->Width : 0,
        depthDesc ? depthDesc->Height : 0,
        depthDesc ? static_cast<unsigned long>(depthDesc->Format) : 0,
        gIsMain3DSceneActive ? 1 : 0,
        gStereoFrameReadyToPublish ? 1 : 0,
        static_cast<LONG>(gStereoReplayDrawsThisPresent));
    logLine(message);
}

bool targetHasFullResolutionDepth(
    IDirect3DDevice9* device,
    IDirect3DSurface9* target,
    IDirect3DSurface9* depth,
    D3DSURFACE_DESC& targetDesc,
    D3DSURFACE_DESC& depthDesc,
    UINT& configuredWidth,
    UINT& configuredHeight,
    const char* context)
{
    std::memset(&targetDesc, 0, sizeof(targetDesc));
    std::memset(&depthDesc, 0, sizeof(depthDesc));
    configuredWidth = 0;
    configuredHeight = 0;

    if (!device || !target)
    {
        logStrictStereoTargetGate(context, "missing-target", nullptr, nullptr, 0, 0);
        return false;
    }

    if (FAILED(target->GetDesc(&targetDesc)))
    {
        logStrictStereoTargetGate(context, "target-desc-failed", nullptr, nullptr, 0, 0);
        return false;
    }

    if (!configuredStereoDimensions(device, configuredWidth, configuredHeight))
    {
        logStrictStereoTargetGate(context, "configured-size-unavailable", &targetDesc, nullptr, 0, 0);
        return false;
    }

    if (targetDesc.Width != configuredWidth || targetDesc.Height != configuredHeight)
    {
        logStrictStereoTargetGate(context, "target-not-full-resolution", &targetDesc, nullptr, configuredWidth, configuredHeight);
        return false;
    }

    IDirect3DSurface9* canonicalBackBuffer = nullptr;
    const bool canonicalTarget = !gPrimaryBackBufferLockable
        && SUCCEEDED(device->GetBackBuffer(
            0, 0, D3DBACKBUFFER_TYPE_MONO, &canonicalBackBuffer))
        && canonicalBackBuffer
        && sameComIdentity(target, canonicalBackBuffer);
    releaseSurface(canonicalBackBuffer);
    if (!canonicalTarget)
    {
        logStrictStereoTargetGate(
            context,
            gPrimaryBackBufferLockable ? "lockable-backbuffer-rejected" : "not-canonical-primary-backbuffer",
            &targetDesc,
            nullptr,
            configuredWidth,
            configuredHeight);
        return false;
    }

    if (!depth)
    {
        logStrictStereoTargetGate(context, "missing-depth", &targetDesc, nullptr, configuredWidth, configuredHeight);
        return false;
    }

    if (FAILED(depth->GetDesc(&depthDesc)))
    {
        logStrictStereoTargetGate(context, "depth-desc-failed", &targetDesc, nullptr, configuredWidth, configuredHeight);
        return false;
    }

    if (depthDesc.Width != configuredWidth || depthDesc.Height != configuredHeight)
    {
        logStrictStereoTargetGate(context, "depth-not-full-resolution", &targetDesc, &depthDesc, configuredWidth, configuredHeight);
        return false;
    }

    if (!depthSurfaceIsProofNonLockable(depthDesc))
    {
        logStrictStereoTargetGate(
            context,
            "depth-cpu-mutation-not-excluded",
            &targetDesc,
            &depthDesc,
            configuredWidth,
            configuredHeight);
        return false;
    }

    return true;
}

bool noAdditionalRenderTargetsBound(IDirect3DDevice9* device, const char* context)
{
    if (!device)
        return false;
    D3DCAPS9 caps {};
    if (FAILED(device->GetDeviceCaps(&caps))
        || caps.NumSimultaneousRTs == 0
        || caps.NumSimultaneousRTs > 4)
    {
        logStrictStereoTargetGate(
            context, "mrt-caps-unknown", nullptr, nullptr,
            gStereoTargetWidth, gStereoTargetHeight);
        return false;
    }
    for (DWORD index = 1; index < caps.NumSimultaneousRTs; ++index)
    {
        IDirect3DSurface9* auxiliaryTarget = nullptr;
        const HRESULT result = device->GetRenderTarget(index, &auxiliaryTarget);
        const bool clear = SUCCEEDED(result) && auxiliaryTarget == nullptr;
        releaseSurface(auxiliaryTarget);
        if (!clear)
        {
            logStrictStereoTargetGate(
                context, "additional-render-target-bound", nullptr, nullptr,
                gStereoTargetWidth, gStereoTargetHeight);
            return false;
        }
    }
    return true;
}

bool currentTargetPassesStrictStereoGate(
    IDirect3DDevice9* device,
    IDirect3DSurface9* target,
    IDirect3DSurface9* depth,
    const char* context)
{
    if (!strictStereoTargetGateEnabled())
        return true;
    if (gStereoFrameReadyToPublish)
    {
        logStrictStereoTargetGate(context, "frame-already-ready", nullptr, nullptr, gStereoTargetWidth, gStereoTargetHeight);
        return false;
    }
    if (!noAdditionalRenderTargetsBound(device, context))
        return false;

    D3DSURFACE_DESC targetDesc {};
    D3DSURFACE_DESC depthDesc {};
    UINT configuredWidth = 0;
    UINT configuredHeight = 0;
    if (!targetHasFullResolutionDepth(device, target, depth, targetDesc, depthDesc, configuredWidth, configuredHeight, context))
        return false;

    if (!gIsMain3DSceneActive && readEnvBool("FNVXR_D3D9_STEREO_AUTO_ACTIVATE_TARGET_ON_DRAW", true))
    {
        gMain3DSceneRenderTarget = target;
        gIsMain3DSceneActive = true;
        logStrictStereoTargetGate(context, "auto-activated-on-draw", &targetDesc, &depthDesc, configuredWidth, configuredHeight);
    }

    if (gIsMain3DSceneActive
        && gMain3DSceneRenderTarget
        && !sameComIdentity(target, gMain3DSceneRenderTarget))
    {
        logStrictStereoTargetGate(context, "not-active-main-target", &targetDesc, &depthDesc, configuredWidth, configuredHeight);
        return false;
    }

    return true;
}

void markStereoFrameReadyFromSwapAway(
    IDirect3DDevice9* device,
    const char* context,
    const char* reason,
    const D3DSURFACE_DESC* targetDesc,
    const D3DSURFACE_DESC* depthDesc,
    UINT configuredWidth,
    UINT configuredHeight)
{
    if (!gIsMain3DSceneActive)
        return;

    gStereoFrameReadyToPublish = true;
    gIsMain3DSceneActive = false;
    gMain3DSceneRenderTarget = nullptr;
    gInStereoReplay = false;

    logStrictStereoTargetGate(context, reason, targetDesc, depthDesc, configuredWidth, configuredHeight);

    if (readEnvBool("FNVXR_D3D9_STEREO_READBACK_ON_SWAP_AWAY", false))
        readbackStereoTargets(device);
}

void updateMain3DSceneTargetGate(IDirect3DDevice9* device, IDirect3DSurface9* renderTarget, const char* context)
{
    if (!strictStereoTargetGateEnabled() || gInStereoReplay || renderTarget == nullptr)
        return;
    if (gStereoFrameReadyToPublish)
    {
        gInStereoReplay = false;
        return;
    }

    IDirect3DSurface9* depth = nullptr;
    device->GetDepthStencilSurface(&depth);

    D3DSURFACE_DESC targetDesc {};
    D3DSURFACE_DESC depthDesc {};
    UINT configuredWidth = 0;
    UINT configuredHeight = 0;
    const bool eligible = targetHasFullResolutionDepth(
        device,
        renderTarget,
        depth,
        targetDesc,
        depthDesc,
        configuredWidth,
        configuredHeight,
        context);
    releaseSurface(depth);

    if (eligible)
    {
        if (!gIsMain3DSceneActive || gMain3DSceneRenderTarget != renderTarget)
        {
            gMain3DSceneRenderTarget = renderTarget;
            gIsMain3DSceneActive = true;
            logStrictStereoTargetGate(context, "main-3d-scene-active", &targetDesc, &depthDesc, configuredWidth, configuredHeight);
        }
        return;
    }

    markStereoFrameReadyFromSwapAway(
        device,
        context,
        "main-3d-scene-swap-away",
        &targetDesc,
        depthDesc.Width || depthDesc.Height ? &depthDesc : nullptr,
        configuredWidth,
        configuredHeight);
}

template <typename Fn>
Fn resolve(const char* name)
{
    if (!loadRealD3D9())
        return nullptr;
    return reinterpret_cast<Fn>(GetProcAddress(gRealD3D9, name));
}

bool patchVTableSlot(void** vtable, size_t index, void* replacement, void** original)
{
    if (!vtable || !replacement || !original)
        return false;

    DWORD oldProtect = 0;
    if (!VirtualProtect(&vtable[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    *original = vtable[index];
    vtable[index] = replacement;

    DWORD unused = 0;
    VirtualProtect(&vtable[index], sizeof(void*), oldProtect, &unused);
    return true;
}

bool sharedStereoPublishedCurrentNativePair()
{
    if (!gSharedStereoView)
        return false;

    __try
    {
        const auto* header = reinterpret_cast<const SharedStereoHeader*>(gSharedStereoView);
        const LONG sequenceBefore = header->sequence;
        if (header->writing != 0)
            return false;
        MemoryBarrier();
        const bool ready = header->magic == SharedStereoMagic
            && header->width > 0
            && header->height > 0
            && header->separated != 0
            && header->worldCandidate != 0
            && header->uiActive == 0
            && header->poseValid != 0
            && header->producerMode == static_cast<LONG>(fnvxr::shared::StereoProducerNativeSameFrame)
            && header->renderPairSequence == gNativeStereoRenderPairSequence;
        MemoryBarrier();
        return ready
            && header->writing == 0
            && header->sequence == sequenceBefore;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

HRESULT WINAPI hookedPresent(
    IDirect3DDevice9* device,
    const RECT* sourceRect,
    const RECT* destRect,
    HWND destWindowOverride,
    const RGNDATA* dirtyRegion)
{
    LARGE_INTEGER presentHookStart {};
    LARGE_INTEGER realPresentStart {};
    LARGE_INTEGER realPresentEnd {};
    QueryPerformanceCounter(&presentHookStart);
    const LONG currentThread = static_cast<LONG>(GetCurrentThreadId());
    const LONG renderOwner = InterlockedCompareExchange(
        &gRenderOwnerThreadId, currentThread, 0);
    if (renderOwner != 0 && renderOwner != currentThread)
    {
        requestRenderOwnerShutdown();
        logLine("stereo shutdown requested: Present moved to a second thread");
        return gRealPresent(device, sourceRect, destRect, destWindowOverride, dirtyRegion);
    }
    serviceRenderOwnerShutdown("present-thread-mismatch");
    const LONG originalRenderCalls = gOriginalDoRenderCallsThisPresent;
    const LONGLONG originalRenderTicks = gOriginalDoRenderTicksThisPresent;
    const LONG nativeHookEntries = InterlockedExchange(&gNativeDoRenderHookEntriesThisPresent, 0);
    const LONG nativeStereoAttempts = InterlockedExchange(&gNativeDoRenderStereoAttemptsThisPresent, 0);
    const LONG nativeGateMask = InterlockedExchange(&gNativeDoRenderGateMaskThisPresent, 0);
    const LONG nativePassRejected = InterlockedExchange(&gNativePassRejectedThisPresent, 0);
    const LONG nativePassMismatch = InterlockedExchange(&gNativePassMismatchThisPresent, 0);
    gOriginalDoRenderCallsThisPresent = 0;
    gOriginalDoRenderTicksThisPresent = 0;
    logFrameRate("IDirect3DDevice9::Present", gPresentFrames, gLastPresentSample, gLastPresentFrameSample);
    const bool stereoSuppressed = suppressStereoForUiMode();
    observeNativeRenderHookContinuity(
        nativeHookEntries,
        nativeStereoAttempts,
        nativeGateMask,
        stereoSuppressed);
    const bool haveNativePair = gNativeStereoEnabled
        && gNativeStereoPairsThisPresent > 0
        && gNativeStereoRenderedPoseValid;
    const bool havePublishableNativePair = haveNativePair
        && gNativePostprocessFinalWritesThisPresent > 0;
    const bool havePublishableSingleTraversalPair = gNativeSingleTraversalReplayEnabled
        && gNativeSingleTraversalFramesThisPresent > 0
        && gNativeStereoRenderedPoseValid
        && gStereoReplayDrawsThisPresent > 0;
    // A complete native pair supersedes the mono gameplay plane.  Avoid a
    // third synchronous GPU readback on those frames; menu/UI frames still
    // publish the mono surface normally.
    if (stereoSuppressed || (!havePublishableNativePair && !havePublishableSingleTraversalPair))
        captureSharedVideoFrame(device);
    if (!stereoSuppressed && gStereoReplayEnabled)
    {
        ensureStereoTargets(device);
        readbackStereoTargets(device);
    }
    publishSharedWorldVideoFrame(device);
    auditNativeFullSizeTargetCandidates(device);
    publishSharedStereoFrame(device);
    if (!stereoSuppressed
        && havePublishableNativePair
        && !sharedStereoPublishedCurrentNativePair())
    {
        restoreNativeLeftEyeAsMono(device);
        captureSharedVideoFrame(device);
    }
    logMatrixAuditFrame();
    const LONG presentStateLog = InterlockedIncrement(&gLoggedStereoPresentState);
    if ((gNativeStereoEnabled || gNativeSingleTraversalReplayEnabled || gStereoReplayEnabled || gWideWorldReplayEnabled)
        && (presentStateLog <= 8 || presentStateLog % 300 == 0))
    {
        char message[512] {};
        sprintf_s(
            message,
            "stereo present state frame=%ld suppressedByUi=%d nativePairs=%ld singleTraversal=%ld nativeHookEntries=%ld nativeAttempts=%ld nativeGateMask=0x%lx nativeRejected=%ld nativeMismatch=%ld cell=0x%08lx cellReady=%d equivalenceRecovery=%ld nativeFanoutDraws=%ld nativeFanoutFinal=%ld replayDraws=%ld wideDraws=%ld haveView=%d haveProjection=%d sharedStereo=%d wideWorld=%d strictGateActive=%d strictGateReady=%d",
            static_cast<LONG>(gPresentFrames),
            stereoSuppressed ? 1 : 0,
            static_cast<LONG>(gNativeStereoPairsThisPresent),
            static_cast<LONG>(gNativeSingleTraversalFramesThisPresent),
            nativeHookEntries,
            nativeStereoAttempts,
            static_cast<unsigned long>(nativeGateMask),
            nativePassRejected,
            nativePassMismatch,
            static_cast<unsigned long>(gNativeStableCellFormId),
            gNativeCellTransitionReady ? 1 : 0,
            static_cast<LONG>(gNativeEquivalentPairsInSequence),
            static_cast<LONG>(gNativePostprocessFanoutDrawsThisPresent),
            static_cast<LONG>(gNativePostprocessFinalWritesThisPresent),
            static_cast<LONG>(gStereoReplayDrawsThisPresent),
            static_cast<LONG>(gWideWorldReplayDrawsThisPresent),
            gHaveView ? 1 : 0,
            gHaveProjection ? 1 : 0,
            sharedStereoEnabled() ? 1 : 0,
            gWideWorldReplayEnabled ? 1 : 0,
            gIsMain3DSceneActive ? 1 : 0,
            gStereoFrameReadyToPublish ? 1 : 0);
        logLine(message);
    }
    InterlockedExchange(&gNativeStereoPairsThisPresent, 0);
    InterlockedExchange(&gNativeSingleTraversalFramesThisPresent, 0);
    InterlockedExchange(&gNativePostprocessFanoutDrawsThisPresent, 0);
    InterlockedExchange(&gNativePostprocessFinalWritesThisPresent, 0);
    gNativePostprocessFanoutActive = false;
    gNativePipelineTracePostprocessWindow = false;
    gNativePipelineTraceThisPair = false;
    gNativeStereoRenderedPoseValid = false;
    gNativeStereoRenderedPoseSequence = 0;
    gNativeStereoRenderedDisplayTime = 0;
    InterlockedExchange(&gStereoReplayDrawsThisPresent, 0);
    InterlockedExchange(&gWideWorldReplayDrawsThisPresent, 0);
    resetBestStereoSnapshotThisPresent();
    resetStereoCollapseAuditThisPresent();
    gIsMain3DSceneActive = false;
    gStereoFrameReadyToPublish = false;
    gMain3DSceneRenderTarget = nullptr;
    gInStereoReplay = false;
    resetNativePostprocessCommandStream();
    resetNativeFullSizeTargetCandidates();

    QueryPerformanceCounter(&realPresentStart);
    const HRESULT result = gRealPresent(device, sourceRect, destRect, destWindowOverride, dirtyRegion);
    QueryPerformanceCounter(&realPresentEnd);
    if (FAILED(result))
    {
        // A device-loss/failed Present is a discontinuity.  Do not let a
        // previously published eye pair survive as if the desktop render
        // transaction had completed.
        gHavePublishedValidStereoWorldFrame = false;
        closeNativeStereoOriginLease("present-failed");
        if (gSharedStereoView)
            writeInvalidSharedStereoRecord(
                reinterpret_cast<SharedStereoHeader*>(gSharedStereoView),
                false);
        logLine("shared stereo invalidated: IDirect3DDevice9::Present failed");
    }
    if (readEnvBool("FNVXR_D3D9_PRESENT_PERF", true))
    {
        const LONG logCount = InterlockedIncrement(&gLoggedPresentPerf);
        if (logCount <= 24 || logCount % 120 == 0)
        {
            char message[512] {};
            sprintf_s(
                message,
                "d3d9 present perf frame=%ld doRenderCalls=%ld hookEntries=%ld nativeAttempts=%ld nativeGateMask=0x%lx nativeRejected=%ld nativeMismatch=%ld doRenderMs=%.3f prePresentMs=%.3f realPresentMs=%.3f totalMs=%.3f result=0x%08lx",
                static_cast<LONG>(gPresentFrames),
                originalRenderCalls,
                nativeHookEntries,
                nativeStereoAttempts,
                static_cast<unsigned long>(nativeGateMask),
                nativePassRejected,
                nativePassMismatch,
                gPerfFrequency.QuadPart > 0
                    ? static_cast<double>(originalRenderTicks) * 1000.0 / static_cast<double>(gPerfFrequency.QuadPart)
                    : 0.0,
                secondsBetween(presentHookStart, realPresentStart) * 1000.0,
                secondsBetween(realPresentStart, realPresentEnd) * 1000.0,
                secondsBetween(presentHookStart, realPresentEnd) * 1000.0,
                static_cast<unsigned long>(result));
            logLine(message);
        }
    }
    return result;
}

HRESULT WINAPI hookedReset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* presentationParameters)
{
    closeNativeStereoOriginLease("device-reset");
    publishSharedStereoInvalid(false, "device-reset");
    gPrimaryBackBufferLockable = presentationParameters
        && (presentationParameters->Flags & D3DPRESENTFLAG_LOCKABLE_BACKBUFFER) != 0;
    gNativeEquivalentPairsInSequence = 0;
    gNativeEquivalentLastPresentFrame = -2;
    gNativePassEquivalenceWasBlocked = true;
    gNativeCellTransitionReady = false;
    gNativeStableCellFormId = 0;
    gNativeCellChangePlayerFrame = 0;
    gNativeDoubleTraversalHoldUntilPlayerFrame = 0;
    gNativeSharedPlayerReadMisses = 0;
    gNativeHookSilentGameplayFrames = 0;
    resetNativePostprocessCommandStream();
    resetNativeFullSizeTargetCandidates();
    releaseTrackedTextures();
    releaseStereoTargets();
    releaseSharedVideoReadback();
    forceImmediatePresentation(presentationParameters, "Reset");
    logLine("IDirect3DDevice9::Reset");
    const HRESULT result = gRealReset(device, presentationParameters);
    if (SUCCEEDED(result))
    {
        InterlockedExchange(&gStateBlockRecording, 0);
        InterlockedExchange(&gD3DQueryObjectsObserved, 0);
    }
    return result;
}

HRESULT WINAPI hookedEndScene(IDirect3DDevice9* device)
{
    logFrameRate("IDirect3DDevice9::EndScene", gEndSceneFrames, gLastEndSceneSample, gLastEndSceneFrameSample);

    return gRealEndScene(device);
}

HRESULT WINAPI hookedClear(
    IDirect3DDevice9* device,
    DWORD count,
    const D3DRECT* rects,
    DWORD flags,
    D3DCOLOR color,
    float z,
    DWORD stencil)
{
    const HRESULT result = gRealClear(device, count, rects, flags, color, z, stencil);
    if (gInStereoReplay || suppressStereoForUiMode())
        return result;
    IDirect3DSurface9* originalTarget = nullptr;
    IDirect3DSurface9* originalDepth = nullptr;
    const HRESULT getOriginalTargetResult = device->GetRenderTarget(0, &originalTarget);
    const HRESULT getOriginalDepthResult = device->GetDepthStencilSurface(&originalDepth);
    bool noAdditionalRenderTargets = true;
    for (DWORD targetIndex = 1; targetIndex < 4; ++targetIndex)
    {
        IDirect3DSurface9* extraTarget = nullptr;
        const HRESULT extraTargetResult = device->GetRenderTarget(targetIndex, &extraTarget);
        if (FAILED(extraTargetResult) || extraTarget)
            noAdditionalRenderTargets = false;
        if (extraTarget)
            extraTarget->Release();
    }
    D3DSURFACE_DESC originalTargetDesc {};
    D3DSURFACE_DESC originalDepthDesc {};
    D3DVIEWPORT9 clearViewport {};
    DWORD scissorEnabled = TRUE;
    RECT clearScissor {};
    const bool haveOriginalTargetDesc = originalTarget
        && SUCCEEDED(originalTarget->GetDesc(&originalTargetDesc));
    const bool haveOriginalDepthDesc = originalDepth
        && SUCCEEDED(originalDepth->GetDesc(&originalDepthDesc));
    const bool haveClearViewport = SUCCEEDED(device->GetViewport(&clearViewport));
    const bool haveScissorState = SUCCEEDED(device->GetRenderState(
        D3DRS_SCISSORTESTENABLE, &scissorEnabled));
    const bool haveClearScissor = SUCCEEDED(device->GetScissorRect(&clearScissor));
    const bool fullEquivalentClear = count == 0
        && (flags & (D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER))
            == (D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER)
        && haveOriginalTargetDesc
        && haveOriginalDepthDesc
        && haveClearViewport
        && clearViewport.X == 0
        && clearViewport.Y == 0
        && clearViewport.Width == originalTargetDesc.Width
        && clearViewport.Height == originalTargetDesc.Height
        && clearViewport.MinZ == 0.0f
        && clearViewport.MaxZ == 1.0f
        && haveScissorState
        && haveClearScissor
        && (scissorEnabled == FALSE
            || (clearScissor.left == 0
                && clearScissor.top == 0
                && clearScissor.right == static_cast<LONG>(originalTargetDesc.Width)
                && clearScissor.bottom == static_cast<LONG>(originalTargetDesc.Height)))
        && (!formatHasStencil(originalDepthDesc.Format)
            || (flags & D3DCLEAR_STENCIL) != 0);
    const bool clearStateKnown = SUCCEEDED(getOriginalTargetResult)
        && SUCCEEDED(getOriginalDepthResult)
        && haveClearViewport
        && haveScissorState
        && haveClearScissor
        && noAdditionalRenderTargets;
    const bool strictTargetAllowed =
        currentTargetPassesStrictStereoGate(device, originalTarget, originalDepth, "Clear");
    const bool strictEyeTargetLedgerClear = gInNativeStereoHook
        && gNativeActiveEye == 2
        && SUCCEEDED(result)
        && strictTargetAllowed
        && (flags & (D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL)) != 0;
    if (strictEyeTargetLedgerClear)
        InterlockedIncrement(&gStrictEyeTargetClears);
    const bool replayStereo = gStereoReplayEnabled
        && SUCCEEDED(result)
        && stereoReplayTransactionAllowed()
        && strictTargetAllowed
        && clearStateKnown
        && fullEquivalentClear
        && ensureStereoTargets(device);
    const bool replayWide = gWideWorldReplayEnabled
        && InterlockedCompareExchange(&gStateBlockRecording, 0, 0) == 0
        && strictTargetAllowed
        && clearStateKnown
        && fullEquivalentClear
        && shouldReplayCurrentDrawToWideWorld()
        && ensureWideWorldTargets(device);
    if (!replayStereo && !replayWide)
    {
        releaseSurface(originalTarget);
        releaseSurface(originalDepth);
        return result;
    }
    StereoSurfaceTwin* targetTwin = replayStereo ? ensureSurfaceTwin(device, originalTarget, false) : nullptr;
    StereoSurfaceTwin* depthTwin = (replayStereo && originalDepth) ? ensureSurfaceTwin(device, originalDepth, true) : nullptr;
    if (replayStereo && !targetTwin)
    {
        releaseSurface(originalTarget);
        releaseSurface(originalDepth);
        return result;
    }

    gInStereoReplay = true;
    HRESULT leftClearResult = D3DERR_INVALIDCALL;
    HRESULT rightClearResult = D3DERR_INVALIDCALL;
    HRESULT wideClearResult = D3DERR_INVALIDCALL;
    HRESULT leftTargetResult = D3DERR_INVALIDCALL;
    HRESULT leftDepthResult = D3DERR_INVALIDCALL;
    HRESULT rightTargetResult = D3DERR_INVALIDCALL;
    HRESULT rightDepthResult = D3DERR_INVALIDCALL;
    HRESULT leftViewportResult = D3DERR_INVALIDCALL;
    HRESULT leftScissorResult = D3DERR_INVALIDCALL;
    HRESULT leftScissorStateResult = D3DERR_INVALIDCALL;
    HRESULT rightViewportResult = D3DERR_INVALIDCALL;
    HRESULT rightScissorResult = D3DERR_INVALIDCALL;
    HRESULT rightScissorStateResult = D3DERR_INVALIDCALL;
    if (replayStereo)
    {
        leftTargetResult = device->SetRenderTarget(0, targetTwin->left);
        leftDepthResult = device->SetDepthStencilSurface(depthTwin ? depthTwin->left : nullptr);
        leftViewportResult = device->SetViewport(&clearViewport);
        leftScissorResult = device->SetScissorRect(&clearScissor);
        leftScissorStateResult = device->SetRenderState(D3DRS_SCISSORTESTENABLE, scissorEnabled);
        if (SUCCEEDED(leftTargetResult)
            && SUCCEEDED(leftDepthResult)
            && SUCCEEDED(leftViewportResult)
            && SUCCEEDED(leftScissorResult)
            && SUCCEEDED(leftScissorStateResult))
            leftClearResult = gRealClear(device, count, rects, flags, color, z, stencil);

        rightTargetResult = device->SetRenderTarget(0, targetTwin->right);
        rightDepthResult = device->SetDepthStencilSurface(depthTwin ? depthTwin->right : nullptr);
        rightViewportResult = device->SetViewport(&clearViewport);
        rightScissorResult = device->SetScissorRect(&clearScissor);
        rightScissorStateResult = device->SetRenderState(D3DRS_SCISSORTESTENABLE, scissorEnabled);
        if (SUCCEEDED(rightTargetResult)
            && SUCCEEDED(rightDepthResult)
            && SUCCEEDED(rightViewportResult)
            && SUCCEEDED(rightScissorResult)
            && SUCCEEDED(rightScissorStateResult))
            rightClearResult = gRealClear(device, count, rects, flags, color, z, stencil);
    }
    if (replayWide && gWideWorldSurface)
    {
        device->SetRenderTarget(0, gWideWorldSurface);
        device->SetDepthStencilSurface(gWideWorldDepth);
        wideClearResult = gRealClear(device, count, rects, flags, color, z, stencil);
    }

    const HRESULT restoreTargetResult = device->SetRenderTarget(0, originalTarget);
    const HRESULT restoreDepthResult = device->SetDepthStencilSurface(originalDepth);
    const HRESULT restoreViewportResult = haveClearViewport
        ? device->SetViewport(&clearViewport)
        : D3DERR_INVALIDCALL;
    const HRESULT restoreScissorResult = haveClearScissor
        ? device->SetScissorRect(&clearScissor)
        : D3DERR_INVALIDCALL;
    gInStereoReplay = false;
    if (FAILED(restoreTargetResult)
        || FAILED(restoreDepthResult)
        || FAILED(restoreViewportResult)
        || FAILED(restoreScissorResult))
        InterlockedIncrement(&gStrictEyeTargetUnprovenWrites);

    const LONG clearLog = InterlockedIncrement(&gLoggedStereoClearMirror);
    const bool hammerLog = shouldTelemetryHammerLog(clearLog, "FNVXR_D3D9_CLEAR_TELEMETRY_STRIDE", 1);
    const bool clearFailed =
        (replayStereo && (FAILED(leftClearResult) || FAILED(rightClearResult)))
        || (replayWide && FAILED(wideClearResult));
    if (strictEyeTargetLedgerClear
        && replayStereo
        && SUCCEEDED(leftTargetResult)
        && SUCCEEDED(leftDepthResult)
        && SUCCEEDED(leftViewportResult)
        && SUCCEEDED(leftScissorResult)
        && SUCCEEDED(leftScissorStateResult)
        && SUCCEEDED(leftClearResult)
        && SUCCEEDED(rightTargetResult)
        && SUCCEEDED(rightDepthResult)
        && SUCCEEDED(rightViewportResult)
        && SUCCEEDED(rightScissorResult)
        && SUCCEEDED(rightScissorStateResult)
        && SUCCEEDED(rightClearResult)
        && SUCCEEDED(restoreTargetResult)
        && SUCCEEDED(restoreDepthResult)
        && SUCCEEDED(restoreViewportResult)
        && SUCCEEDED(restoreScissorResult))
    {
        InterlockedIncrement(&gStrictEyeTargetProvenBothEyeClears);
        if (fullEquivalentClear && !gStrictDrawSeenThisTraversal)
        {
            gStrictEquivalentClearSeenThisTraversal = true;
            targetTwin->equivalentInitializationGeneration =
                gStereoReplayTransactionGeneration;
            targetTwin->leftValidGeneration = gStereoReplayTransactionGeneration;
            targetTwin->rightValidGeneration = gStereoReplayTransactionGeneration;
            depthTwin->equivalentInitializationGeneration =
                gStereoReplayTransactionGeneration;
            depthTwin->leftValidGeneration = gStereoReplayTransactionGeneration;
            depthTwin->rightValidGeneration = gStereoReplayTransactionGeneration;
            targetTwin->lastDepthPrimeFrame = static_cast<LONG>(gPresentFrames);
            targetTwin->lastDepthPrimeTwin = depthTwin;
            InterlockedIncrement(&gStrictEyeTargetFullInitializations);
        }
    }
    if (hammerLog || clearFailed)
    {
        char event[512] {};
        sprintf_s(
            event,
            "{\"event\":\"fnvxrD3d9ClearMirror\",\"frame\":%ld,\"count\":%ld,"
            "\"flags\":\"0x%lx\",\"target\":%d,\"zbuffer\":%d,\"stencilFlag\":%d,"
            "\"rectCount\":%lu,\"color\":\"0x%08lx\",\"z\":%.5f,\"stencil\":%lu,"
            "\"replayStereo\":%d,\"replayWide\":%d,\"depthTwin\":%d,"
            "\"left\":\"0x%08lx\",\"right\":\"0x%08lx\",\"wide\":\"0x%08lx\","
            "\"original\":\"0x%08lx\"}",
            static_cast<LONG>(gPresentFrames),
            clearLog,
            static_cast<unsigned long>(flags),
            (flags & D3DCLEAR_TARGET) ? 1 : 0,
            (flags & D3DCLEAR_ZBUFFER) ? 1 : 0,
            (flags & D3DCLEAR_STENCIL) ? 1 : 0,
            static_cast<unsigned long>(count),
            static_cast<unsigned long>(color),
            z,
            static_cast<unsigned long>(stencil),
            replayStereo ? 1 : 0,
            replayWide ? 1 : 0,
            depthTwin ? 1 : 0,
            static_cast<unsigned long>(leftClearResult),
            static_cast<unsigned long>(rightClearResult),
            static_cast<unsigned long>(wideClearResult),
            static_cast<unsigned long>(result));
        logLine(event);
    }

    releaseSurface(originalTarget);
    releaseSurface(originalDepth);
    return result;
}

HRESULT WINAPI hookedSetTransform(IDirect3DDevice9* device, D3DTRANSFORMSTATETYPE state, const D3DMATRIX* matrix)
{
    const LONG call = InterlockedIncrement(&gSetTransformCalls);
    logHookCadence("IDirect3DDevice9::SetTransform", call, gLastTransformSample, gLastTransformCallSample, 5.0);
    const HRESULT result = gRealSetTransform(device, state, matrix);
    if (FAILED(result))
        return result;

    const LONG nativeEye = gNativeActiveEye;
    if (gInNativeStereoHook && matrix && nativeEye >= 0 && nativeEye <= 1)
    {
        if (state == D3DTS_VIEW)
        {
            gNativeObservedView[nativeEye] = *matrix;
            InterlockedIncrement(&gNativeObservedViewCalls[nativeEye]);
        }
        else if (state == D3DTS_PROJECTION)
        {
            gNativeObservedProjection[nativeEye] = *matrix;
            InterlockedIncrement(&gNativeObservedProjectionCalls[nativeEye]);
        }
    }

    if (state == D3DTS_VIEW && InterlockedIncrement(&gLoggedViewTransforms) <= 12)
        logTransformMatrix("SetTransform", state, matrix);
    else if (state == D3DTS_PROJECTION && InterlockedIncrement(&gLoggedProjectionTransforms) <= 12)
        logTransformMatrix("SetTransform", state, matrix);

    if (matrix && state == D3DTS_VIEW)
    {
        updateSharedVrPose();
        gBaseView = toStereoMatrix(*matrix);
        gHaveView = true;
        updateEyeMatrices();
    }
    else if (matrix && state == D3DTS_PROJECTION)
    {
        updateSharedVrPose();
        gBaseProjection = toStereoMatrix(*matrix);
        gHaveProjection = true;
        updateEyeMatrices();
    }

    return result;
}

HRESULT WINAPI hookedSetVertexShader(IDirect3DDevice9* device, IDirect3DVertexShader9* shader)
{
    const LONG call = InterlockedIncrement(&gSetVertexShaderCalls);
    const HRESULT result = gRealSetVertexShader(device, shader);
    if (FAILED(result))
        return result;

    const ShaderFingerprint fingerprint = fingerprintD3DShaderBytecode(shader, "vs", true);
    gActiveVertexShader = shader;
    gActiveVertexShaderHash = fingerprint.fnv1a32;
    gActiveVertexShaderByteCount = fingerprint.byteCount;
    std::memcpy(
        gActiveVertexShaderSha256,
        fingerprint.sha256,
        sizeof(gActiveVertexShaderSha256));
    if (InterlockedIncrement(&gLoggedVertexShaderChanges) <= 24 || call % 5000 == 0)
    {
        char message[192] {};
        sprintf_s(
            message,
            "SetVertexShader call=%ld frame=%ld shader=%p hash=0x%08x",
            call,
            static_cast<LONG>(gPresentFrames),
            reinterpret_cast<void*>(shader),
            gActiveVertexShaderHash);
        logLine(message);
    }
    return result;
}

HRESULT WINAPI hookedSetPixelShader(IDirect3DDevice9* device, IDirect3DPixelShader9* shader)
{
    const LONG call = InterlockedIncrement(&gSetPixelShaderCalls);
    const HRESULT result = gRealSetPixelShader(device, shader);
    if (FAILED(result))
        return result;

    const ShaderFingerprint fingerprint = fingerprintD3DShaderBytecode(shader, "ps", false);
    gActivePixelShader = shader;
    gActivePixelShaderHash = fingerprint.fnv1a32;
    if (InterlockedIncrement(&gLoggedPixelShaderChanges) <= 24 || call % 5000 == 0)
    {
        char message[192] {};
        sprintf_s(
            message,
            "SetPixelShader call=%ld frame=%ld shader=%p hash=0x%08x",
            call,
            static_cast<LONG>(gPresentFrames),
            reinterpret_cast<void*>(shader),
            gActivePixelShaderHash);
        logLine(message);
    }
    return result;
}

bool refreshAuthoritativeDrawState(IDirect3DDevice9* device)
{
    if (!device)
        return false;

    IDirect3DVertexShader9* vertexShader = nullptr;
    if (FAILED(device->GetVertexShader(&vertexShader)))
        return false;
    const ShaderFingerprint vertexFingerprint =
        fingerprintD3DShaderBytecode(vertexShader, "vs-authoritative", false);
    gActiveVertexShader = vertexShader;
    gActiveVertexShaderHash = vertexFingerprint.fnv1a32;
    gActiveVertexShaderByteCount = vertexFingerprint.byteCount;
    std::memcpy(
        gActiveVertexShaderSha256,
        vertexFingerprint.sha256,
        sizeof(gActiveVertexShaderSha256));
    if (vertexShader)
        vertexShader->Release();

    IDirect3DPixelShader9* pixelShader = nullptr;
    if (FAILED(device->GetPixelShader(&pixelShader)))
        return false;
    const ShaderFingerprint pixelFingerprint =
        fingerprintD3DShaderBytecode(pixelShader, "ps-authoritative", false);
    gActivePixelShader = pixelShader;
    gActivePixelShaderHash = pixelFingerprint.fnv1a32;
    if (pixelShader)
        pixelShader->Release();

    D3DMATRIX view {};
    D3DMATRIX projection {};
    if (FAILED(device->GetTransform(D3DTS_VIEW, &view))
        || FAILED(device->GetTransform(D3DTS_PROJECTION, &projection)))
    {
        return false;
    }
    gBaseView = toStereoMatrix(view);
    gBaseProjection = toStereoMatrix(projection);
    gHaveView = fnvxr::stereo::isFinite(gBaseView);
    gHaveProjection = fnvxr::stereo::isFinite(gBaseProjection);
    if (!gHaveView || !gHaveProjection)
        return false;

    DWORD clipPlaneMask = 0;
    if (FAILED(device->GetRenderState(D3DRS_CLIPPLANEENABLE, &clipPlaneMask))
        || clipPlaneMask != 0)
    {
        return false;
    }

    for (UINT sampler = 0; sampler < MaxTrackedSamplers; ++sampler)
    {
        IDirect3DBaseTexture9* texture = nullptr;
        if (FAILED(device->GetTexture(sampler, &texture)))
        {
            if (texture)
                texture->Release();
            return false;
        }
        // GetTexture returns an owning reference. Retain it through twin binds
        // and restoration so application-side release cannot invalidate the
        // authoritative binding snapshot.
        replaceTrackedTexture(sampler, texture, true);
    }

    // Replay only swaps RT0. Any active MRT makes the original draw's pixel
    // effects different from the replayed eye transaction and therefore
    // cannot be admitted.
    for (DWORD targetIndex = 1; targetIndex < 4; ++targetIndex)
    {
        IDirect3DSurface9* extraTarget = nullptr;
        const HRESULT targetResult = device->GetRenderTarget(targetIndex, &extraTarget);
        if (FAILED(targetResult) || extraTarget)
        {
            if (extraTarget)
                extraTarget->Release();
            return false;
        }
    }

    if (const ShaderWvpContract* contract = currentShaderWvpContract())
    {
        float rows[16] {};
        if (contract->startRegister > MaxTrackedVsConstants - 4
            || FAILED(device->GetVertexShaderConstantF(contract->startRegister, rows, 4)))
        {
            return false;
        }
        for (UINT row = 0; row < 4; ++row)
        {
            std::memcpy(gVsConstants[contract->startRegister + row], rows + row * 4, sizeof(float) * 4);
            gHaveVsConstants[contract->startRegister + row] = true;
        }
    }
    return true;
}

template <typename DrawFn, typename... Args>
HRESULT replayDrawToStereoTargets(IDirect3DDevice9* device, bool userPrimitiveDraw, DrawFn draw, Args... args)
{
    const bool nativeCenterTraversal = gInNativeStereoHook
        && gNativeActiveEye == 2
        && !gInStereoReplay;
    if (nativeCenterTraversal && !refreshAuthoritativeDrawState(device))
    {
        InterlockedIncrement(&gStrictEyeTargetUnprovenWrites);
        return D3DERR_INVALIDCALL;
    }
    // Shader discovery is diagnostic and must observe the programmable draw
    // stream even when Fallout leaves stale fixed-function VIEW/PROJECTION
    // state behind. Those stale screen-space transforms are not evidence that
    // a vertex shader using its own ModelViewProj constants is UI.
    if (gInNativeStereoHook
        && gNativeActiveEye == 2
        && !gInStereoReplay
        && !suppressStereoForUiMode()
        && gActiveVertexShader
        && readEnvBool("FNVXR_D3D9_DUMP_SHADER_BYTECODE", false))
    {
        logWorldShaderWvpDiscovery();
    }

    // A native center traversal establishes world ownership even when Fallout
    // leaves a stale fixed-function screen-space VIEW transform behind.  The
    // programmable shaders consume their own ModelViewProj constants.  Count
    // every non-composite programmable draw before replay filters so an
    // unknown or skipped world shader makes the transaction fail closed.
    const bool uiSuppressed = suppressStereoForUiMode();
    const bool screenSpaceProjection = currentProjectionLooksScreenSpace();
    // A fixed-function projection left behind while a programmable shader is
    // bound is not a proof that the shader draw is screen-space. Until a
    // strong exact-bytecode screen/composite manifest exists, count it in the
    // world denominator. Unknown draws then fail closed instead of escaping
    // the proof ledger through stale D3DTS_PROJECTION state.
    const bool provenScreenSpaceDraw = false;
    const bool samplesStereoTwin = currentDrawSamplesStereoTwin();
    if (nativeCenterTraversal && !uiSuppressed && samplesStereoTwin)
    {
        // No bytecode/target-stage contract currently proves that a draw
        // sampling a twinned render texture is a screen-space composite. A
        // world reflection draw can otherwise escape every WVP obligation.
        InterlockedIncrement(&gStrictEyeTargetUnprovenWrites);
    }
    // Weak FNV shader-pair skip lists are diagnostic only. They cannot remove
    // a draw from the production proof denominator without an exact
    // SHA/byte-count/stage semantics contract.
    const bool configuredSkip = stereoProofModeArmed()
        ? false
        : currentShaderPairIsConfiguredStereoSkip();
    const bool replayUserPrimitiveDraws = readEnvBool(
        "FNVXR_D3D9_STEREO_REPLAY_UP_DRAWS",
        false);
    const bool programmableWorldDrawBasis = fnvxr::stereo::programmableWorldDrawBasis(
        nativeCenterTraversal,
        uiSuppressed,
        gActiveVertexShader != nullptr,
        gHaveProjection,
        provenScreenSpaceDraw,
        samplesStereoTwin,
        configuredSkip,
        userPrimitiveDraw,
        replayUserPrimitiveDraws);
    bool strictEyeTargetLedgerDraw = false;
    if (nativeCenterTraversal && !uiSuppressed && device && !samplesStereoTwin)
    {
        IDirect3DSurface9* ledgerTarget = nullptr;
        IDirect3DSurface9* ledgerDepth = nullptr;
        if (SUCCEEDED(device->GetRenderTarget(0, &ledgerTarget)) && ledgerTarget)
        {
            device->GetDepthStencilSurface(&ledgerDepth);
            strictEyeTargetLedgerDraw = currentTargetPassesStrictStereoGate(
                device,
                ledgerTarget,
                ledgerDepth,
                "DrawLedger");
        }
        releaseSurface(ledgerTarget);
        releaseSurface(ledgerDepth);
        if (strictEyeTargetLedgerDraw)
        {
            gStrictDrawSeenThisTraversal = true;
            InterlockedIncrement(&gStrictEyeTargetDraws);
        }
        else
        {
            // Every center-traversal target write needs an eye-equivalent
            // resource path. Unknown auxiliary/fixed-function targets cannot
            // disappear from the denominator.
            InterlockedIncrement(&gStrictEyeTargetUnprovenWrites);
        }
    }
    if (nativeCenterTraversal
        && !uiSuppressed
        && gActiveVertexShader
        && gHaveProjection
        && !samplesStereoTwin
        && !configuredSkip
        && userPrimitiveDraw
        && !replayUserPrimitiveDraws)
    {
        InterlockedIncrement(&gShaderExcludedUserPrimitiveDraws);
    }
    const LONG nativeTraceEye = gNativeActiveEye;
    if (gNativePipelineTraceThisPair
        && gInNativeStereoHook
        && !gInStereoReplay
        && device
        && nativeTraceEye >= 0
        && nativeTraceEye <= 1
        && (gNativePipelineStereoSamplerMask != 0
            || readEnvBool("FNVXR_D3D9_NATIVE_TRACE_ALL_DRAWS", false)))
    {
        const LONG eyeDrawTrace = InterlockedIncrement(&gNativePipelineTraceEyeDraws[nativeTraceEye]);
        const LONG eyeDrawTraceLimit = static_cast<LONG>(readEnvFloat(
            "FNVXR_D3D9_NATIVE_EYE_DRAW_TRACE_LIMIT",
            96.0f));
        if (eyeDrawTraceLimit > 0 && eyeDrawTrace <= eyeDrawTraceLimit)
        {
            IDirect3DSurface9* target = nullptr;
            device->GetRenderTarget(0, &target);
            char targetDesc[256] {};
            describeSurface(targetDesc, sizeof(targetDesc), target);
            char stack[320] {};
            formatNativePipelineStack(stack, sizeof(stack), 0);
            char stackScan[640] {};
            formatNativePipelineStackScan(stackScan, sizeof(stackScan));
            D3DVIEWPORT9 viewport {};
            const bool haveViewport = SUCCEEDED(device->GetViewport(&viewport));
            char message[1792] {};
            sprintf_s(
                message,
                "native pipeline trace phase=%s eyeDraw=%ld samplerMask=0x%04lx userPrimitive=%d vsHash=0x%08x psHash=0x%08x viewport=%lu,%lu,%lux%lu target=%s stack=%s stackScan=%s",
                nativeTraceEye == 0 ? "left" : "right",
                eyeDrawTrace,
                static_cast<unsigned long>(gNativePipelineStereoSamplerMask),
                userPrimitiveDraw ? 1 : 0,
                gActiveVertexShaderHash,
                gActivePixelShaderHash,
                haveViewport ? static_cast<unsigned long>(viewport.X) : 0ul,
                haveViewport ? static_cast<unsigned long>(viewport.Y) : 0ul,
                haveViewport ? static_cast<unsigned long>(viewport.Width) : 0ul,
                haveViewport ? static_cast<unsigned long>(viewport.Height) : 0ul,
                targetDesc,
                stack,
                stackScan);
            logLine(message);
            releaseSurface(target);
        }
    }

    if (gNativePipelineTracePostprocessWindow && !gInStereoReplay && device)
    {
        const LONG drawTrace = InterlockedIncrement(&gNativePipelineTracePostDraws);
        const LONG drawTraceLimit = static_cast<LONG>(readEnvFloat(
            "FNVXR_D3D9_NATIVE_POST_DRAW_TRACE_LIMIT",
            192.0f));
        if (drawTraceLimit > 0 && drawTrace <= drawTraceLimit)
        {
            DWORD stereoSamplerMask = 0;
            for (UINT sampler = 0; sampler < MaxTrackedSamplers; ++sampler)
            {
                if (gActiveTextures[sampler]
                    && stereoTextureFor(gActiveTextures[sampler], true)
                    && stereoTextureFor(gActiveTextures[sampler], false))
                {
                    stereoSamplerMask |= (1u << sampler);
                }
            }
            IDirect3DSurface9* target = nullptr;
            device->GetRenderTarget(0, &target);
            char targetDesc[256] {};
            describeSurface(targetDesc, sizeof(targetDesc), target);
            D3DVIEWPORT9 viewport {};
            const bool haveViewport = SUCCEEDED(device->GetViewport(&viewport));
            char message[768] {};
            sprintf_s(
                message,
                "native pipeline trace phase=post draw=%ld samplerMask=0x%04lx userPrimitive=%d vsHash=0x%08x psHash=0x%08x viewport=%lu,%lu,%lux%lu target=%s",
                drawTrace,
                static_cast<unsigned long>(stereoSamplerMask),
                userPrimitiveDraw ? 1 : 0,
                gActiveVertexShaderHash,
                gActivePixelShaderHash,
                haveViewport ? static_cast<unsigned long>(viewport.X) : 0ul,
                haveViewport ? static_cast<unsigned long>(viewport.Y) : 0ul,
                haveViewport ? static_cast<unsigned long>(viewport.Width) : 0ul,
                haveViewport ? static_cast<unsigned long>(viewport.Height) : 0ul,
                targetDesc);
            logLine(message);
            releaseSurface(target);
        }
    }

    const bool nativePostprocessComposite = gNativePostprocessFanoutActive
        && currentDrawSamplesStereoTwin();
    const bool stereoReplayRequested = stereoReplayTransactionAllowed()
        && (gStereoReplayEnabled || nativePostprocessComposite);
    const bool wideReplayRequested = gWideWorldReplayEnabled
        && InterlockedCompareExchange(&gStateBlockRecording, 0, 0) == 0;
    if ((!stereoReplayRequested && !wideReplayRequested)
        || gInStereoReplay
        || suppressStereoForUiMode()
        || !gHaveView
        || !gHaveProjection)
    {
        return D3DERR_INVALIDCALL;
    }

    const bool drawPolicyAllowsStereo = nativePostprocessComposite
        || shouldReplayCurrentDrawToStereo(userPrimitiveDraw);

    IDirect3DSurface9* originalTarget = nullptr;
    IDirect3DSurface9* originalDepth = nullptr;
    D3DVIEWPORT9 originalViewport {};
    RECT originalScissor {};
    D3DMATRIX originalView {};
    D3DMATRIX originalProjection {};
    if (FAILED(device->GetRenderTarget(0, &originalTarget)) || !originalTarget)
        return D3DERR_INVALIDCALL;
    device->GetDepthStencilSurface(&originalDepth);
    const bool strictTargetAllowed =
        currentTargetPassesStrictStereoGate(device, originalTarget, originalDepth, "DrawReplay");
    const bool programmableWorldDraw = fnvxr::stereo::programmableWorldDrawCandidate(
        programmableWorldDrawBasis,
        strictTargetAllowed);
    if (programmableWorldDrawBasis && !strictTargetAllowed)
        InterlockedIncrement(&gShaderExcludedAuxiliaryTargetDraws);
    if (programmableWorldDraw)
        InterlockedIncrement(&gShaderWorldDrawCandidates);
    if (!drawPolicyAllowsStereo)
    {
        releaseSurface(originalTarget);
        releaseSurface(originalDepth);
        return D3DERR_INVALIDCALL;
    }
    const bool replayStereo = stereoReplayRequested
        && strictTargetAllowed
        && ensureStereoTargets(device);
    const bool replayWide = wideReplayRequested
        && strictTargetAllowed
        && ensureWideWorldTargets(device);
    if (!replayStereo && !replayWide)
    {
        releaseSurface(originalTarget);
        releaseSurface(originalDepth);
        return D3DERR_INVALIDCALL;
    }
    StereoSurfaceTwin* targetTwin = replayStereo ? ensureSurfaceTwin(device, originalTarget, false) : nullptr;
    StereoSurfaceTwin* depthTwin = (replayStereo && originalDepth) ? ensureSurfaceTwin(device, originalDepth, true) : nullptr;
    if (replayStereo && (!targetTwin || !targetTwin->left || !targetTwin->right))
    {
        releaseSurface(originalTarget);
        releaseSurface(originalDepth);
        return D3DERR_INVALIDCALL;
    }
    const bool haveViewport = SUCCEEDED(device->GetViewport(&originalViewport));
    const bool haveScissor = SUCCEEDED(device->GetScissorRect(&originalScissor));
    const bool haveOriginalView = SUCCEEDED(device->GetTransform(D3DTS_VIEW, &originalView));
    const bool haveOriginalProjection = SUCCEEDED(device->GetTransform(D3DTS_PROJECTION, &originalProjection));
    if (!haveViewport || !haveScissor || !haveOriginalView || !haveOriginalProjection)
    {
        InterlockedIncrement(&gStrictEyeTargetUnprovenWrites);
        releaseSurface(originalTarget);
        releaseSurface(originalDepth);
        return D3DERR_INVALIDCALL;
    }
    ScopedReplayStateBlock replayState(device, "draw-replay");
    if (!replayState.captured)
    {
        InterlockedIncrement(&gStrictEyeTargetUnprovenWrites);
        releaseSurface(originalTarget);
        releaseSurface(originalDepth);
        return D3DERR_INVALIDCALL;
    }

    updateSharedVrPose();
    updateEyeMatrices();
    if (replayStereo
        && !primeStereoReplayTarget(device, targetTwin, depthTwin, "draw-replay"))
    {
        InterlockedIncrement(&gStrictEyeTargetUnprovenWrites);
        releaseSurface(originalTarget);
        releaseSurface(originalDepth);
        return D3DERR_INVALIDCALL;
    }
    if (replayWide)
        primeWideWorldReplayTarget(device);

    const LONG shaderStateLog = InterlockedIncrement(&gLoggedStereoReplayShaderState);
    const bool shaderStateHammer =
        shouldTelemetryHammerLog(shaderStateLog, "FNVXR_D3D9_REPLAY_DRAW_TELEMETRY_STRIDE", 1);
    if (shaderStateLog <= 24 || shaderStateLog % 5000 == 0 || shaderStateHammer)
    {
        char message[384] {};
        sprintf_s(
            message,
            "stereo replay shader state count=%ld frame=%ld vs=%p ps=%p vsHash=0x%08x psHash=0x%08x userPrimitive=%d projScreen=%d viewScreen=%d worldCamera=%d",
            shaderStateLog,
            static_cast<LONG>(gPresentFrames),
            reinterpret_cast<void*>(gActiveVertexShader),
            reinterpret_cast<void*>(gActivePixelShader),
            gActiveVertexShaderHash,
            gActivePixelShaderHash,
            userPrimitiveDraw ? 1 : 0,
            currentProjectionLooksScreenSpace() ? 1 : 0,
            currentViewLooksScreenSpace() ? 1 : 0,
            haveWorldCameraCandidate() ? 1 : 0);
        logLine(message);
    }
    const LONG cullLimitLog = InterlockedIncrement(&gLoggedStereoReplayCullLimit);
    if (cullLimitLog <= 3 || cullLimitLog % 3000 == 0
        || shouldTelemetryHammerLog(cullLimitLog, "FNVXR_D3D9_CULL_TELEMETRY_STRIDE", 120))
    {
        logLine(
            "stereo replay limit: D3D9 draw replay can patch shader WVP/depth for submitted draws, "
            "but cannot recover geometry culled before Gamebryo submitted the draw");
    }

    const D3DMATRIX leftView = toD3DMatrix(gEyeMatrices.leftView);
    const D3DMATRIX rightView = toD3DMatrix(gEyeMatrices.rightView);
    const D3DMATRIX leftProjection = toD3DMatrix(gEyeMatrices.leftProjection);
    const D3DMATRIX rightProjection = toD3DMatrix(gEyeMatrices.rightProjection);
    const D3DMATRIX wideWorldView = toD3DMatrix(baseViewForStereo());
    const fnvxr::stereo::Matrix4 wideWorldProjectionMatrix = makeWideWorldProjection();
    const D3DMATRIX wideWorldProjection = toD3DMatrix(wideWorldProjectionMatrix);
    const bool screenSpaceTextureComposite =
        currentDrawSamplesStereoTwin()
        && haveOriginalView
        && haveOriginalProjection;
    const D3DMATRIX& leftReplayView = screenSpaceTextureComposite ? originalView : leftView;
    const D3DMATRIX& rightReplayView = screenSpaceTextureComposite ? originalView : rightView;
    const D3DMATRIX& leftReplayProjection = screenSpaceTextureComposite ? originalProjection : leftProjection;
    const D3DMATRIX& rightReplayProjection = screenSpaceTextureComposite ? originalProjection : rightProjection;

    HRESULT result = D3DERR_INVALIDCALL;
    HRESULT wideResult = D3DERR_INVALIDCALL;
    HRESULT leftReplayResult = D3DERR_INVALIDCALL;
    bool leftWvpPatched = false;
    bool rightWvpPatched = false;
    const LONG replayOrdinal = static_cast<LONG>(gStereoReplayDrawsThisPresent) + 1;
    gInStereoReplay = true;
    if (replayWide
        && gWideWorldSurface
        && SUCCEEDED(device->SetRenderTarget(0, gWideWorldSurface))
        && SUCCEEDED(device->SetDepthStencilSurface(gWideWorldDepth))
        && (!haveViewport || SUCCEEDED(device->SetViewport(&originalViewport)))
        && (!haveScissor || SUCCEEDED(device->SetScissorRect(&originalScissor)))
        && SUCCEEDED(gRealSetTransform(device, D3DTS_VIEW, &wideWorldView))
        && SUCCEEDED(gRealSetTransform(device, D3DTS_PROJECTION, &wideWorldProjection)))
    {
        const bool wideShaderDeltaEnabled = readEnvBool("FNVXR_D3D9_WIDE_WORLD_SHADER_MATRIX_DELTA", true);
        const bool wideShaderReady =
            !gActiveVertexShader
            || !wideShaderDeltaEnabled
            || setWideWorldShaderConstants(device, wideWorldProjectionMatrix);
        if (wideShaderReady)
            wideResult = draw(device, args...);
        restoreShaderConstants(device);
    }

    if (replayStereo
        && SUCCEEDED(device->SetRenderTarget(0, targetTwin->left))
        && SUCCEEDED(device->SetDepthStencilSurface(depthTwin ? depthTwin->left : nullptr))
        && (!haveViewport || SUCCEEDED(device->SetViewport(&originalViewport)))
        && (!haveScissor || SUCCEEDED(device->SetScissorRect(&originalScissor)))
        && SUCCEEDED(gRealSetTransform(device, D3DTS_VIEW, &leftReplayView))
        && SUCCEEDED(gRealSetTransform(device, D3DTS_PROJECTION, &leftReplayProjection)))
    {
        if (!screenSpaceTextureComposite)
        {
            setEyeShaderConstants(device, 1.0f);
            leftWvpPatched =
                setEyeShaderWvpConstants(
                    device,
                    originalView,
                    originalProjection,
                    leftView,
                    leftProjection,
                    0,
                    "left");
        }
        bindStereoTextureTwins(device, true);
        leftReplayResult = draw(device, args...);
        if (SUCCEEDED(leftReplayResult)
            && SUCCEEDED(device->SetRenderTarget(0, targetTwin->right))
            && SUCCEEDED(device->SetDepthStencilSurface(depthTwin ? depthTwin->right : nullptr))
            && (!haveViewport || SUCCEEDED(device->SetViewport(&originalViewport)))
            && (!haveScissor || SUCCEEDED(device->SetScissorRect(&originalScissor)))
            && SUCCEEDED(gRealSetTransform(device, D3DTS_VIEW, &rightReplayView))
            && SUCCEEDED(gRealSetTransform(device, D3DTS_PROJECTION, &rightReplayProjection)))
        {
            restoreShaderConstants(device);
            if (!screenSpaceTextureComposite)
            {
                setEyeShaderConstants(device, -1.0f);
                rightWvpPatched =
                    setEyeShaderWvpConstants(
                        device,
                        originalView,
                        originalProjection,
                        rightView,
                        rightProjection,
                        1,
                        "right");
            }
            bindStereoTextureTwins(device, false);
            result = draw(device, args...);
        }
    }

    bool restorationComplete = replayState.restore();
    if (haveOriginalView)
        restorationComplete = SUCCEEDED(gRealSetTransform(device, D3DTS_VIEW, &originalView))
            && restorationComplete;
    if (haveOriginalProjection)
        restorationComplete = SUCCEEDED(gRealSetTransform(device, D3DTS_PROJECTION, &originalProjection))
            && restorationComplete;
    restorationComplete = SUCCEEDED(device->SetRenderTarget(0, originalTarget))
        && restorationComplete;
    restorationComplete = SUCCEEDED(device->SetDepthStencilSurface(originalDepth))
        && restorationComplete;
    if (haveViewport)
        restorationComplete = SUCCEEDED(device->SetViewport(&originalViewport))
            && restorationComplete;
    if (haveScissor)
        restorationComplete = SUCCEEDED(device->SetScissorRect(&originalScissor))
            && restorationComplete;
    gInStereoReplay = false;
    if (!restorationComplete)
        InterlockedIncrement(&gStrictEyeTargetUnprovenWrites);

    const bool targetPrestateProven = !replayStereo
        || (targetTwin
            && depthTwin
            && targetTwin->equivalentInitializationGeneration
                == gStereoReplayTransactionGeneration
            && targetTwin->leftValidGeneration == gStereoReplayTransactionGeneration
            && targetTwin->rightValidGeneration == gStereoReplayTransactionGeneration
            && depthTwin->equivalentInitializationGeneration
                == gStereoReplayTransactionGeneration
            && depthTwin->leftValidGeneration == gStereoReplayTransactionGeneration
            && depthTwin->rightValidGeneration == gStereoReplayTransactionGeneration);
    if (replayStereo && strictEyeTargetLedgerDraw && !targetPrestateProven)
        InterlockedIncrement(&gStrictEyeTargetUnprovenWrites);

    if (restorationComplete
        && targetPrestateProven
        && programmableWorldDraw
        && SUCCEEDED(leftReplayResult)
        && SUCCEEDED(result))
    {
        InterlockedIncrement(&gShaderWorldDraws);
        if (leftWvpPatched && rightWvpPatched)
        {
            InterlockedIncrement(&gShaderContractCoveredWorldDraws);
            if (strictEyeTargetLedgerDraw)
                InterlockedIncrement(&gStrictEyeTargetProvenBothEyeDraws);
        }
    }
    if (restorationComplete
        && targetPrestateProven
        && replayStereo
        && SUCCEEDED(leftReplayResult)
        && SUCCEEDED(result))
    {
        targetTwin->leftValidGeneration = gStereoReplayTransactionGeneration;
        targetTwin->rightValidGeneration = gStereoReplayTransactionGeneration;
    }

    const LONG replayResultLog = InterlockedIncrement(&gLoggedStereoReplayDrawResult);
    const bool replayResultHammer =
        shouldTelemetryHammerLog(replayResultLog, "FNVXR_D3D9_REPLAY_DRAW_TELEMETRY_STRIDE", 1);
    const bool replayResultFailed =
        (replayStereo && (FAILED(leftReplayResult) || FAILED(result)))
        || (replayWide && FAILED(wideResult));
    if (replayResultHammer || (replayResultFailed && (replayResultLog <= 240 || replayResultLog % 300 == 0)))
    {
        char event[1024] {};
        sprintf_s(
            event,
            "{\"event\":\"fnvxrD3d9ReplayDrawResult\",\"frame\":%ld,\"count\":%ld,"
            "\"draw\":%ld,\"replayStereo\":%d,\"replayWide\":%d,\"userPrimitive\":%d,"
            "\"left\":\"0x%08lx\",\"right\":\"0x%08lx\",\"wide\":\"0x%08lx\","
            "\"leftWvpPatched\":%d,\"rightWvpPatched\":%d,\"stateBlockCaptured\":%d,"
            "\"viewport\":%d,\"scissor\":%d,\"originalView\":%d,\"originalProjection\":%d,"
            "\"projScreen\":%d,\"viewScreen\":%d,\"worldCamera\":%d,\"textureComposite\":%d,"
            "\"targetTwin\":%d,\"depthTwin\":%d,\"width\":%u,\"height\":%u,"
            "\"vsHash\":\"0x%08x\",\"psHash\":\"0x%08x\"}",
            static_cast<LONG>(gPresentFrames),
            replayResultLog,
            replayOrdinal,
            replayStereo ? 1 : 0,
            replayWide ? 1 : 0,
            userPrimitiveDraw ? 1 : 0,
            static_cast<unsigned long>(leftReplayResult),
            static_cast<unsigned long>(result),
            static_cast<unsigned long>(wideResult),
            leftWvpPatched ? 1 : 0,
            rightWvpPatched ? 1 : 0,
            replayState.captured ? 1 : 0,
            haveViewport ? 1 : 0,
            haveScissor ? 1 : 0,
            haveOriginalView ? 1 : 0,
            haveOriginalProjection ? 1 : 0,
            currentProjectionLooksScreenSpace() ? 1 : 0,
            currentViewLooksScreenSpace() ? 1 : 0,
            haveWorldCameraCandidate() ? 1 : 0,
            screenSpaceTextureComposite ? 1 : 0,
            targetTwin ? 1 : 0,
            depthTwin ? 1 : 0,
            targetTwin ? targetTwin->desc.Width : gStereoTargetWidth,
            targetTwin ? targetTwin->desc.Height : gStereoTargetHeight,
            gActiveVertexShaderHash,
            gActivePixelShaderHash);
        logLine(event);
    }

    releaseSurface(originalTarget);
    releaseSurface(originalDepth);

    if (SUCCEEDED(wideResult))
    {
        const LONG wideDraws = InterlockedIncrement(&gWideWorldReplayDrawsThisPresent);
        const LONG logCount = InterlockedIncrement(&gLoggedWideWorldReplay);
        if (logCount <= 5 || logCount % 500 == 0)
        {
            char message[320] {};
            sprintf_s(
                message,
                "wide world replay draw count=%ld frame=%ld draw=%ld fovScale=(%.5f %.5f) vs=%p ps=%p vsHash=0x%08x psHash=0x%08x",
                logCount,
                static_cast<LONG>(gPresentFrames),
                wideDraws,
                wideWorldProjection.m[0][0] / (gBaseProjection.m[0][0] == 0.0f ? 1.0f : gBaseProjection.m[0][0]),
                wideWorldProjection.m[1][1] / (gBaseProjection.m[1][1] == 0.0f ? 1.0f : gBaseProjection.m[1][1]),
                reinterpret_cast<void*>(gActiveVertexShader),
                reinterpret_cast<void*>(gActivePixelShader),
                gActiveVertexShaderHash,
                gActivePixelShaderHash);
            logLine(message);
        }
    }

    if (!replayStereo)
        return wideResult;

    if (FAILED(result))
        return result;

    const LONG drawOrdinal = replayOrdinal;
    probeStereoSurfacePair(device, targetTwin, "draw", drawOrdinal);
    const bool bestSnapshotEnabled = readEnvBool("FNVXR_D3D9_STEREO_PUBLISH_BEST_SNAPSHOT", true);
    const LONG bestSnapshotLimit = static_cast<LONG>(
        readEnvFloat("FNVXR_D3D9_STEREO_SNAPSHOT_REPLAY_DRAWS", 512.0f));
    const LONG bestSnapshotFixedDraw = static_cast<LONG>(
        readEnvFloat("FNVXR_D3D9_STEREO_SNAPSHOT_FIXED_DRAW", 512.0f));
    const LONG bestSnapshotStride = static_cast<LONG>(
        readEnvFloat("FNVXR_D3D9_STEREO_SNAPSHOT_DRAW_STRIDE", 16.0f));
    const LONG safeBestSnapshotStride = bestSnapshotStride <= 0 ? 1 : bestSnapshotStride;
    const bool fixedSnapshotDrawActive = bestSnapshotFixedDraw > 0;
    const bool shouldCaptureBestSnapshot =
        bestSnapshotEnabled
        && bestSnapshotLimit > 0
        && drawOrdinal <= bestSnapshotLimit
        && (fixedSnapshotDrawActive
            ? drawOrdinal == bestSnapshotFixedDraw
            : (drawOrdinal == 1 || drawOrdinal == bestSnapshotLimit || (drawOrdinal % safeBestSnapshotStride) == 0));
    const LONG immediateDiffLimit =
        static_cast<LONG>(readEnvFloat("FNVXR_D3D9_DEBUG_COMPARE_REPLAY_DRAWS", 0.0f));
    LONG diffLog = 0;
    const bool shouldLogImmediateDiff =
        immediateDiffLimit > 0
        && (diffLog = InterlockedIncrement(&gLoggedStereoReplayImmediateDiff)) <= immediateDiffLimit;
    if (shouldCaptureBestSnapshot || shouldLogImmediateDiff)
    {
        const bool canReadTargetTwin =
            targetTwin
            && !targetTwin->depth
            && targetTwin->left
            && targetTwin->right
            && targetTwin->desc.MultiSampleType == D3DMULTISAMPLE_NONE
            && targetTwin->desc.Width == gStereoTargetWidth
            && targetTwin->desc.Height == gStereoTargetHeight
            && targetTwin->desc.Format == gStereoTargetFormat;
        if (canReadTargetTwin && ensureReadbackTargets(device))
        {
            if (SUCCEEDED(device->GetRenderTargetData(targetTwin->left, gLeftEyeReadback))
                && SUCCEEDED(device->GetRenderTargetData(targetTwin->right, gRightEyeReadback)))
            {
                const SurfaceDifference difference = compareReadbackSurfaces(gLeftEyeReadback, gRightEyeReadback);
                if (shouldCaptureBestSnapshot)
                    captureBestStereoSnapshot(device, difference, drawOrdinal);
                if (shouldLogImmediateDiff)
                {
                    char message[448] {};
                    sprintf_s(
                        message,
                        "stereo replay immediate diff count=%ld frame=%ld draw=%ld diff=%u/%u leftHash=0x%08x rightHash=0x%08x vs=%p ps=%p vsHash=0x%08x psHash=0x%08x",
                        diffLog,
                        static_cast<LONG>(gPresentFrames),
                        drawOrdinal,
                        difference.differentSamples,
                        difference.samples,
                        difference.leftHash,
                        difference.rightHash,
                        reinterpret_cast<void*>(gActiveVertexShader),
                        reinterpret_cast<void*>(gActivePixelShader),
                        gActiveVertexShaderHash,
                        gActivePixelShaderHash);
                    logLine(message);
                }
            }
            else
            {
                logLine("stereo replay immediate diff readback failed");
            }
        }
    }
    auditStereoCollapseAfterDraw(device, drawOrdinal, userPrimitiveDraw);

    const LONG logCount = InterlockedIncrement(&gLoggedStereoReplay);
    if (logCount <= 5 || logCount % 500 == 0
        || shouldTelemetryHammerLog(logCount, "FNVXR_D3D9_REPLAY_DRAW_TELEMETRY_STRIDE", 1))
    {
        char message[320] {};
        sprintf_s(
            message,
            "stereo replay draw count=%ld frame=%ld vs=%p ps=%p vsHash=0x%08x psHash=0x%08x",
            logCount,
            static_cast<LONG>(gPresentFrames),
            reinterpret_cast<void*>(gActiveVertexShader),
            reinterpret_cast<void*>(gActivePixelShader),
            gActiveVertexShaderHash,
            gActivePixelShaderHash);
        logLine(message);
    }

    if (nativePostprocessComposite)
    {
        const LONG fanoutDraws = InterlockedIncrement(&gNativePostprocessFanoutDrawsThisPresent);
        const bool finalTarget = targetTwin
            && targetTwin->left == gLeftEyeSurface
            && targetTwin->right == gRightEyeSurface;
        if (finalTarget)
            InterlockedIncrement(&gNativePostprocessFinalWritesThisPresent);
        const LONG logCount = InterlockedIncrement(&gLoggedNativePostprocessFanout);
        if (logCount <= 32 || logCount % 240 == 0 || finalTarget)
        {
            char message[384] {};
            sprintf_s(
                message,
                "native postprocess fanout count=%ld frame=%ld draw=%ld finalTarget=%d finalWrites=%ld targetFormat=%lu size=%ux%u",
                logCount,
                static_cast<LONG>(gPresentFrames),
                fanoutDraws,
                finalTarget ? 1 : 0,
                static_cast<LONG>(gNativePostprocessFinalWritesThisPresent),
                targetTwin ? static_cast<unsigned long>(targetTwin->desc.Format) : 0ul,
                targetTwin ? targetTwin->desc.Width : 0,
                targetTwin ? targetTwin->desc.Height : 0);
            logLine(message);
        }
    }
    else
    {
        InterlockedIncrement(&gStereoReplayDrawsThisPresent);
    }
    return result;
}

HRESULT WINAPI hookedDrawPrimitive(
    IDirect3DDevice9* device,
    D3DPRIMITIVETYPE primitiveType,
    UINT startVertex,
    UINT primitiveCount)
{
    InterlockedIncrement(&gDrawPrimitiveCalls);
    const LONG nativeEye = gNativeActiveEye;
    if (gInNativeStereoHook && nativeEye >= 0 && nativeEye <= 1)
        InterlockedIncrement(&gNativeDrawCalls[nativeEye]);
    if (suppressGameplayHudDraw())
        return D3D_OK;
    const HRESULT result = gRealDrawPrimitive(device, primitiveType, startVertex, primitiveCount);
    if (SUCCEEDED(result))
    {
        captureNativePostprocessDraw(
            device,
            NativePostprocessDrawKind::DrawPrimitive,
            primitiveType,
            startVertex,
            primitiveCount,
            0,
            0,
            0,
            0);
    }
    if (SUCCEEDED(result))
        replayDrawToStereoTargets(device, false, gRealDrawPrimitive, primitiveType, startVertex, primitiveCount);
    return result;
}

HRESULT WINAPI hookedDrawIndexedPrimitive(
    IDirect3DDevice9* device,
    D3DPRIMITIVETYPE primitiveType,
    INT baseVertexIndex,
    UINT minVertexIndex,
    UINT numVertices,
    UINT startIndex,
    UINT primitiveCount)
{
    InterlockedIncrement(&gDrawIndexedPrimitiveCalls);
    const LONG nativeEye = gNativeActiveEye;
    if (gInNativeStereoHook && nativeEye >= 0 && nativeEye <= 1)
        InterlockedIncrement(&gNativeDrawCalls[nativeEye]);
    if (suppressGameplayHudDraw())
        return D3D_OK;
    const HRESULT result = gRealDrawIndexedPrimitive(
        device, primitiveType, baseVertexIndex, minVertexIndex, numVertices, startIndex, primitiveCount);
    if (SUCCEEDED(result))
    {
        captureNativePostprocessDraw(
            device,
            NativePostprocessDrawKind::DrawIndexedPrimitive,
            primitiveType,
            0,
            primitiveCount,
            baseVertexIndex,
            minVertexIndex,
            numVertices,
            startIndex);
    }
    if (SUCCEEDED(result))
    {
        replayDrawToStereoTargets(
            device,
            false,
            gRealDrawIndexedPrimitive,
            primitiveType,
            baseVertexIndex,
            minVertexIndex,
            numVertices,
            startIndex,
            primitiveCount);
    }
    return result;
}

HRESULT WINAPI hookedDrawPrimitiveUP(
    IDirect3DDevice9* device,
    D3DPRIMITIVETYPE primitiveType,
    UINT primitiveCount,
    const void* vertexStreamZeroData,
    UINT vertexStreamZeroStride)
{
    InterlockedIncrement(&gDrawPrimitiveUPCalls);
    const LONG nativeEye = gNativeActiveEye;
    if (gInNativeStereoHook && nativeEye >= 0 && nativeEye <= 1)
        InterlockedIncrement(&gNativeDrawCalls[nativeEye]);
    if (suppressGameplayHudDraw())
        return D3D_OK;
    const HRESULT result =
        gRealDrawPrimitiveUP(device, primitiveType, primitiveCount, vertexStreamZeroData, vertexStreamZeroStride);
    if (SUCCEEDED(result)
        && gNativePostprocessRecording
        && gInNativeStereoHook
        && gNativeActiveEye == 0
        && !gInStereoReplay)
    {
        if (!gNativePostprocessRecordUnsupported)
            logLine("native delayed postprocess rejected: DrawPrimitiveUP appeared in the command stream");
        gNativePostprocessRecordUnsupported = true;
    }
    if (SUCCEEDED(result))
    {
        replayDrawToStereoTargets(
            device,
            true,
            gRealDrawPrimitiveUP,
            primitiveType,
            primitiveCount,
            vertexStreamZeroData,
            vertexStreamZeroStride);
    }
    return result;
}

HRESULT WINAPI hookedDrawIndexedPrimitiveUP(
    IDirect3DDevice9* device,
    D3DPRIMITIVETYPE primitiveType,
    UINT minVertexIndex,
    UINT numVertices,
    UINT primitiveCount,
    const void* indexData,
    D3DFORMAT indexDataFormat,
    const void* vertexStreamZeroData,
    UINT vertexStreamZeroStride)
{
    InterlockedIncrement(&gDrawIndexedPrimitiveUPCalls);
    const LONG nativeEye = gNativeActiveEye;
    if (gInNativeStereoHook && nativeEye >= 0 && nativeEye <= 1)
        InterlockedIncrement(&gNativeDrawCalls[nativeEye]);
    if (suppressGameplayHudDraw())
        return D3D_OK;
    const HRESULT result = gRealDrawIndexedPrimitiveUP(
        device,
        primitiveType,
        minVertexIndex,
        numVertices,
        primitiveCount,
        indexData,
        indexDataFormat,
        vertexStreamZeroData,
        vertexStreamZeroStride);
    if (SUCCEEDED(result)
        && gNativePostprocessRecording
        && gInNativeStereoHook
        && gNativeActiveEye == 0
        && !gInStereoReplay)
    {
        if (!gNativePostprocessRecordUnsupported)
            logLine("native delayed postprocess rejected: DrawIndexedPrimitiveUP appeared in the command stream");
        gNativePostprocessRecordUnsupported = true;
    }
    if (SUCCEEDED(result))
    {
        replayDrawToStereoTargets(
            device,
            true,
            gRealDrawIndexedPrimitiveUP,
            primitiveType,
            minVertexIndex,
            numVertices,
            primitiveCount,
            indexData,
            indexDataFormat,
            vertexStreamZeroData,
            vertexStreamZeroStride);
    }
    return result;
}

HRESULT WINAPI hookedSetVertexShaderConstantF(
    IDirect3DDevice9* device,
    UINT startRegister,
    const float* constantData,
    UINT vector4fCount)
{
    const LONG call = InterlockedIncrement(&gVertexShaderConstantFCalls);
    logHookCadence(
        "IDirect3DDevice9::SetVertexShaderConstantF", call, gLastVsConstSample, gLastVsConstCallSample, 5.0);
    const HRESULT result = gRealSetVertexShaderConstantF(
        device, startRegister, constantData, vector4fCount);
    if (FAILED(result))
        return result;

    const LONG nativeEye = gNativeActiveEye;
    if (gInNativeStereoHook
        && nativeEye >= 0
        && nativeEye <= 1
        && constantData
        && vector4fCount > 0)
    {
        fnv1aContinue(gNativeVsConstantStreamHash[nativeEye], &startRegister, sizeof(startRegister));
        fnv1aContinue(gNativeVsConstantStreamHash[nativeEye], &vector4fCount, sizeof(vector4fCount));
        fnv1aContinue(
            gNativeVsConstantStreamHash[nativeEye],
            constantData,
            static_cast<std::size_t>(vector4fCount) * 4 * sizeof(float));
        InterlockedIncrement(&gNativeVsConstantCalls[nativeEye]);
    }

    if (constantData && vector4fCount > 0 && startRegister < 32 && InterlockedIncrement(&gLoggedVsConstRows) <= 24)
    {
        const UINT rowsToLog = vector4fCount < 4 ? vector4fCount : 4;
        char message[512] {};
        int offset = sprintf_s(
            message,
            "SetVertexShaderConstantF start=%u rows=%u frame=%ld",
            startRegister,
            vector4fCount,
            static_cast<LONG>(gPresentFrames));
        for (UINT row = 0; row < rowsToLog && offset > 0 && offset < static_cast<int>(sizeof(message)); ++row)
        {
            const float* rowData = constantData + row * 4;
            offset += sprintf_s(
                message + offset,
                sizeof(message) - offset,
                " [%.4f %.4f %.4f %.4f]",
                rowData[0],
                rowData[1],
                rowData[2],
                rowData[3]);
        }
        logLine(message);
    }

    if (!gInStereoReplay && constantData && startRegister < MaxTrackedVsConstants)
    {
        const UINT maxRows = vector4fCount < (MaxTrackedVsConstants - startRegister)
            ? vector4fCount
            : (MaxTrackedVsConstants - startRegister);
        for (UINT row = 0; row < maxRows; ++row)
        {
            const UINT target = startRegister + row;
            gVsConstants[target][0] = constantData[row * 4 + 0];
            gVsConstants[target][1] = constantData[row * 4 + 1];
            gVsConstants[target][2] = constantData[row * 4 + 2];
            gVsConstants[target][3] = constantData[row * 4 + 3];
            gHaveVsConstants[target] = true;
        }
    }

    return result;
}

HRESULT WINAPI hookedCreateTexture(
    IDirect3DDevice9* device,
    UINT width,
    UINT height,
    UINT levels,
    DWORD usage,
    D3DFORMAT format,
    D3DPOOL pool,
    IDirect3DTexture9** texture,
    HANDLE* sharedHandle)
{
    const HRESULT result =
        gRealCreateTexture(device, width, height, levels, usage, format, pool, texture, sharedHandle);
    const LONG call = InterlockedIncrement(&gCreateTextureCalls);
    if (shouldLogResourceGraphCall(call))
    {
        IDirect3DBaseTexture9* baseTexture = (texture && *texture) ? *texture : nullptr;
        char textureDesc[256] {};
        describeBaseTexture(textureDesc, sizeof(textureDesc), baseTexture);
        char message[512] {};
        sprintf_s(
            message,
            "d3d9 resource graph CreateTexture call=%ld frame=%ld result=0x%08lx requested=%ux%u levels=%u usage=0x%lx fmt=%lu pool=%u out=%s replay=%d",
            call,
            static_cast<LONG>(gPresentFrames),
            static_cast<unsigned long>(result),
            width,
            height,
            levels,
            static_cast<unsigned long>(usage),
            static_cast<unsigned long>(format),
            static_cast<unsigned>(pool),
            textureDesc,
            gInStereoReplay ? 1 : 0);
        logLine(message);
    }
    return result;
}

HRESULT WINAPI hookedCreateRenderTarget(
    IDirect3DDevice9* device,
    UINT width,
    UINT height,
    D3DFORMAT format,
    D3DMULTISAMPLE_TYPE multiSample,
    DWORD multisampleQuality,
    BOOL lockable,
    IDirect3DSurface9** surface,
    HANDLE* sharedHandle)
{
    const HRESULT result = gRealCreateRenderTarget(
        device, width, height, format, multiSample, multisampleQuality, lockable, surface, sharedHandle);
    const LONG call = InterlockedIncrement(&gCreateRenderTargetCalls);
    if (shouldLogResourceGraphCall(call))
    {
        char surfaceDesc[256] {};
        describeSurface(surfaceDesc, sizeof(surfaceDesc), surface && *surface ? *surface : nullptr);
        char message[512] {};
        sprintf_s(
            message,
            "d3d9 resource graph CreateRenderTarget call=%ld frame=%ld result=0x%08lx requested=%ux%u fmt=%lu ms=%u quality=%lu lockable=%d out=%s replay=%d",
            call,
            static_cast<LONG>(gPresentFrames),
            static_cast<unsigned long>(result),
            width,
            height,
            static_cast<unsigned long>(format),
            static_cast<unsigned>(multiSample),
            static_cast<unsigned long>(multisampleQuality),
            lockable ? 1 : 0,
            surfaceDesc,
            gInStereoReplay ? 1 : 0);
        logLine(message);
    }
    return result;
}

HRESULT WINAPI hookedCreateDepthStencilSurface(
    IDirect3DDevice9* device,
    UINT width,
    UINT height,
    D3DFORMAT format,
    D3DMULTISAMPLE_TYPE multiSample,
    DWORD multisampleQuality,
    BOOL discard,
    IDirect3DSurface9** surface,
    HANDLE* sharedHandle)
{
    const HRESULT result = gRealCreateDepthStencilSurface(
        device, width, height, format, multiSample, multisampleQuality, discard, surface, sharedHandle);
    const LONG call = InterlockedIncrement(&gCreateDepthStencilSurfaceCalls);
    if (shouldLogResourceGraphCall(call))
    {
        char surfaceDesc[256] {};
        describeSurface(surfaceDesc, sizeof(surfaceDesc), surface && *surface ? *surface : nullptr);
        char message[512] {};
        sprintf_s(
            message,
            "d3d9 resource graph CreateDepthStencilSurface call=%ld frame=%ld result=0x%08lx requested=%ux%u fmt=%lu ms=%u quality=%lu discard=%d out=%s replay=%d",
            call,
            static_cast<LONG>(gPresentFrames),
            static_cast<unsigned long>(result),
            width,
            height,
            static_cast<unsigned long>(format),
            static_cast<unsigned>(multiSample),
            static_cast<unsigned long>(multisampleQuality),
            discard ? 1 : 0,
            surfaceDesc,
            gInStereoReplay ? 1 : 0);
        logLine(message);
    }
    return result;
}

HRESULT WINAPI hookedUpdateSurface(
    IDirect3DDevice9* device,
    IDirect3DSurface9* sourceSurface,
    const RECT* sourceRect,
    IDirect3DSurface9* destinationSurface,
    const POINT* destPoint)
{
    const HRESULT result = gRealUpdateSurface(device, sourceSurface, sourceRect, destinationSurface, destPoint);
    if (SUCCEEDED(result)
        && gInNativeStereoHook
        && gNativeActiveEye == 2
        && !gInStereoReplay)
    {
        InterlockedIncrement(&gStrictEyeTargetUnprovenWrites);
    }
    const LONG call = InterlockedIncrement(&gUpdateSurfaceCalls);
    if (shouldLogResourceGraphCall(call))
    {
        char sourceDesc[256] {};
        char destinationDesc[256] {};
        describeSurface(sourceDesc, sizeof(sourceDesc), sourceSurface);
        describeSurface(destinationDesc, sizeof(destinationDesc), destinationSurface);
        char message[704] {};
        sprintf_s(
            message,
            "d3d9 resource graph UpdateSurface call=%ld frame=%ld result=0x%08lx src=%s dst=%s srcRect=%d destPoint=%d replay=%d",
            call,
            static_cast<LONG>(gPresentFrames),
            static_cast<unsigned long>(result),
            sourceDesc,
            destinationDesc,
            sourceRect ? 1 : 0,
            destPoint ? 1 : 0,
            gInStereoReplay ? 1 : 0);
        logLine(message);
    }
    return result;
}

HRESULT WINAPI hookedUpdateTexture(
    IDirect3DDevice9* device,
    IDirect3DBaseTexture9* sourceTexture,
    IDirect3DBaseTexture9* destinationTexture)
{
    const HRESULT result = gRealUpdateTexture(device, sourceTexture, destinationTexture);
    if (SUCCEEDED(result)
        && gInNativeStereoHook
        && gNativeActiveEye == 2
        && !gInStereoReplay)
    {
        InterlockedIncrement(&gStrictEyeTargetUnprovenWrites);
    }
    const LONG call = InterlockedIncrement(&gUpdateTextureCalls);
    if (shouldLogResourceGraphCall(call))
    {
        char sourceDesc[256] {};
        char destinationDesc[256] {};
        describeBaseTexture(sourceDesc, sizeof(sourceDesc), sourceTexture);
        describeBaseTexture(destinationDesc, sizeof(destinationDesc), destinationTexture);
        char message[704] {};
        sprintf_s(
            message,
            "d3d9 resource graph UpdateTexture call=%ld frame=%ld result=0x%08lx src=%s dst=%s replay=%d",
            call,
            static_cast<LONG>(gPresentFrames),
            static_cast<unsigned long>(result),
            sourceDesc,
            destinationDesc,
            gInStereoReplay ? 1 : 0);
        logLine(message);
    }
    return result;
}

HRESULT WINAPI hookedStretchRect(
    IDirect3DDevice9* device,
    IDirect3DSurface9* sourceSurface,
    const RECT* sourceRect,
    IDirect3DSurface9* destinationSurface,
    const RECT* destRect,
    D3DTEXTUREFILTERTYPE filter)
{
    const HRESULT result = gRealStretchRect(device, sourceSurface, sourceRect, destinationSurface, destRect, filter);
    const LONG call = InterlockedIncrement(&gStretchRectCalls);
    const bool strictEyeTargetLedgerCopy = SUCCEEDED(result)
        && gInNativeStereoHook
        && gNativeActiveEye == 2
        && !gInStereoReplay
        && destinationSurface
        && gMain3DSceneRenderTarget
        && sameComIdentity(destinationSurface, gMain3DSceneRenderTarget);
    if (strictEyeTargetLedgerCopy)
        InterlockedIncrement(&gStrictEyeTargetCopies);

    if (!gInStereoReplay)
    {
        const int currentTracePhase = nativePipelineTracePhase();
        if (currentTracePhase >= 0)
        {
            D3DSURFACE_DESC traceSourceDesc {};
            D3DSURFACE_DESC traceDestinationDesc {};
            const bool haveTraceSource = sourceSurface && SUCCEEDED(sourceSurface->GetDesc(&traceSourceDesc));
            const bool haveTraceDestination = destinationSurface && SUCCEEDED(destinationSurface->GetDesc(&traceDestinationDesc));
            const bool fullSize = (haveTraceSource
                    && traceSourceDesc.Width == gStereoTargetWidth
                    && traceSourceDesc.Height == gStereoTargetHeight)
                || (haveTraceDestination
                    && traceDestinationDesc.Width == gStereoTargetWidth
                    && traceDestinationDesc.Height == gStereoTargetHeight);
            int tracePhase = -1;
            LONG traceEvent = 0;
            if ((currentTracePhase == 2 || fullSize)
                && claimNativePipelineTraceEvent(tracePhase, traceEvent))
            {
                char sourceDesc[256] {};
                char destinationDesc[256] {};
                describeSurface(sourceDesc, sizeof(sourceDesc), sourceSurface);
                describeSurface(destinationDesc, sizeof(destinationDesc), destinationSurface);
                char message[896] {};
                sprintf_s(
                    message,
                    "native pipeline trace phase=%s event=%ld op=StretchRect call=%ld result=0x%08lx fullSize=%d src=%s dst=%s srcRect=%d dstRect=%d filter=%u",
                    nativePipelineTracePhaseName(tracePhase),
                    traceEvent,
                    call,
                    static_cast<unsigned long>(result),
                    fullSize ? 1 : 0,
                    sourceDesc,
                    destinationDesc,
                    sourceRect ? 1 : 0,
                    destRect ? 1 : 0,
                    static_cast<unsigned>(filter));
                logLine(message);
            }
        }
    }

    const LONG nativeEye = gNativeActiveEye;
    if (SUCCEEDED(result)
        && gNativeStereoEnabled
        && gInNativeStereoHook
        && !readEnvBool("FNVXR_D3D9_NATIVE_PERF_SKIP_EYE_COPIES", false)
        && nativeEye >= 0
        && nativeEye <= 1
        && sourceSurface
        && destinationSurface)
    {
        D3DSURFACE_DESC sourceDesc {};
        D3DSURFACE_DESC destinationDesc {};
        if (SUCCEEDED(sourceSurface->GetDesc(&sourceDesc))
            && SUCCEEDED(destinationSurface->GetDesc(&destinationDesc))
            && sourceDesc.Width == gStereoTargetWidth
            && sourceDesc.Height == gStereoTargetHeight
            && destinationDesc.Width == gStereoTargetWidth
            && destinationDesc.Height == gStereoTargetHeight
            && sourceDesc.Format == destinationDesc.Format
            && sourceDesc.MultiSampleType != D3DMULTISAMPLE_NONE
            && destinationDesc.MultiSampleType == D3DMULTISAMPLE_NONE)
        {
            StereoSurfaceTwin* resolvedTwin = ensureSurfaceTwin(device, destinationSurface, false);
            IDirect3DSurface9* eyeTarget = resolvedTwin
                ? (nativeEye == 0 ? resolvedTwin->left : resolvedTwin->right)
                : nullptr;
            const HRESULT snapshotResult = eyeTarget
                ? gRealStretchRect(device, destinationSurface, nullptr, eyeTarget, nullptr, D3DTEXF_NONE)
                : D3DERR_INVALIDCALL;
            if (SUCCEEDED(snapshotResult))
            {
                gNativeResolvedTwinForEye[nativeEye] = resolvedTwin;
                gNativeResolvedDesc = destinationDesc;
                InterlockedIncrement(&gNativeResolvedCopies[nativeEye]);
                if (nativeEye == 0
                    && destinationDesc.Format == D3DFMT_A16B16G16R16F
                    && readEnvBool("FNVXR_D3D9_NATIVE_DELAYED_POSTPROCESS", true))
                {
                    gNativePostprocessRecording = true;
                }
            }
            else
            {
                const LONG failure = InterlockedIncrement(&gLoggedNativeStereoFailure);
                if (failure <= 16 || failure % 120 == 0)
                {
                    char message[320] {};
                    sprintf_s(
                        message,
                        "native resolved-eye snapshot failed eye=%s result=0x%08lx format=%lu size=%ux%u",
                        nativeEye == 0 ? "left" : "right",
                        static_cast<unsigned long>(snapshotResult),
                        static_cast<unsigned long>(destinationDesc.Format),
                        destinationDesc.Width,
                        destinationDesc.Height);
                    logLine(message);
                }
            }
        }
    }

    StereoSurfaceTwin* nativeFanoutSourceTwin = gNativePostprocessFanoutActive
        ? findSurfaceTwin(sourceSurface, false)
        : nullptr;
    const bool nativeFanoutMirror = nativeFanoutSourceTwin != nullptr;
    if (SUCCEEDED(result)
        && stereoReplayTransactionAllowed()
        && (gStereoReplayEnabled || nativeFanoutMirror)
        && !gInStereoReplay
        && !suppressStereoForUiMode()
        && sourceSurface
        && destinationSurface)
    {
        StereoSurfaceTwin* sourceTwin = nativeFanoutMirror
            ? nativeFanoutSourceTwin
            : findSurfaceTwin(sourceSurface, false);
        StereoSurfaceTwin* destinationTwin = ensureSurfaceTwin(device, destinationSurface, false);
        const bool sourceProven = nativeFanoutMirror
            || (sourceTwin
                && sourceTwin->leftValidGeneration == gStereoReplayTransactionGeneration
                && sourceTwin->rightValidGeneration == gStereoReplayTransactionGeneration);
        if (strictEyeTargetLedgerCopy && !sourceProven)
            InterlockedIncrement(&gStrictEyeTargetUnprovenWrites);
        if (sourceProven
            && sourceTwin
            && destinationTwin
            && sourceTwin->left
            && sourceTwin->right
            && destinationTwin->left
            && destinationTwin->right)
        {
            gInStereoReplay = true;
            const HRESULT leftCopy = gRealStretchRect(
                device,
                sourceTwin->left,
                sourceRect,
                destinationTwin->left,
                destRect,
                filter);
            const HRESULT rightCopy = gRealStretchRect(
                device,
                sourceTwin->right,
                sourceRect,
                destinationTwin->right,
                destRect,
                filter);
            gInStereoReplay = false;

            const LONG mirrorLog = InterlockedIncrement(&gLoggedStereoStretchMirrored);
            if (mirrorLog <= 32 || mirrorLog % 120 == 0 || FAILED(leftCopy) || FAILED(rightCopy))
            {
                char mirrorMessage[320] {};
                sprintf_s(
                    mirrorMessage,
                    "stereo StretchRect mirrored count=%ld frame=%ld left=0x%08lx right=0x%08lx src=%p dst=%p",
                    mirrorLog,
                    static_cast<LONG>(gPresentFrames),
                    static_cast<unsigned long>(leftCopy),
                    static_cast<unsigned long>(rightCopy),
                    reinterpret_cast<void*>(sourceSurface),
                    reinterpret_cast<void*>(destinationSurface));
                logLine(mirrorMessage);
            }
            if (nativeFanoutMirror && SUCCEEDED(leftCopy) && SUCCEEDED(rightCopy))
            {
                InterlockedIncrement(&gNativePostprocessFanoutDrawsThisPresent);
                const bool finalTarget = destinationTwin->left == gLeftEyeSurface
                    && destinationTwin->right == gRightEyeSurface;
                if (finalTarget)
                    InterlockedIncrement(&gNativePostprocessFinalWritesThisPresent);
            }
            else
            {
                probeStereoSurfacePair(device, destinationTwin, "stretch-dst", call);
            }
            if (strictEyeTargetLedgerCopy && SUCCEEDED(leftCopy) && SUCCEEDED(rightCopy))
            {
                destinationTwin->leftValidGeneration = gStereoReplayTransactionGeneration;
                destinationTwin->rightValidGeneration = gStereoReplayTransactionGeneration;
                InterlockedIncrement(&gStrictEyeTargetProvenBothEyeCopies);
            }
        }
    }

    if (shouldLogResourceGraphCall(call))
    {
        char sourceDesc[256] {};
        char destinationDesc[256] {};
        describeSurface(sourceDesc, sizeof(sourceDesc), sourceSurface);
        describeSurface(destinationDesc, sizeof(destinationDesc), destinationSurface);
        char message[704] {};
        sprintf_s(
            message,
            "d3d9 resource graph StretchRect call=%ld frame=%ld result=0x%08lx src=%s dst=%s srcRect=%d dstRect=%d filter=%u replay=%d",
            call,
            static_cast<LONG>(gPresentFrames),
            static_cast<unsigned long>(result),
            sourceDesc,
            destinationDesc,
            sourceRect ? 1 : 0,
            destRect ? 1 : 0,
            static_cast<unsigned>(filter),
            gInStereoReplay ? 1 : 0);
        logLine(message);
    }
    return result;
}

HRESULT WINAPI hookedSetRenderTarget(IDirect3DDevice9* device, DWORD renderTargetIndex, IDirect3DSurface9* renderTarget)
{
    IDirect3DSurface9* previousRenderTarget = nullptr;
    const LONG nativeEyeBeforeSet = gNativeActiveEye;
    const bool inspectNativeHdrTransition = renderTargetIndex == 0
        && !gInStereoReplay
        && gInNativeStereoHook
        && nativeEyeBeforeSet == 0
        && !gNativePostprocessRecording
        && !readEnvBool("FNVXR_D3D9_NATIVE_PERF_SKIP_EYE_COPIES", false)
        && readEnvBool("FNVXR_D3D9_NATIVE_DELAYED_POSTPROCESS", true);
    if (inspectNativeHdrTransition)
        device->GetRenderTarget(0, &previousRenderTarget);
    const HRESULT result = gRealSetRenderTarget(device, renderTargetIndex, renderTarget);
    if (SUCCEEDED(result)
        && renderTargetIndex > 0
        && renderTarget != nullptr
        && strictStereoTargetGateEnabled()
        && !gInStereoReplay
        && gInNativeStereoHook)
    {
        // RT1-RT3 are not twinned by the replay transaction.  Any binding is
        // therefore an auxiliary write path that invalidates the proof even if
        // it is later unbound before publication.
        InterlockedIncrement(&gStrictEyeTargetUnprovenWrites);
        logStrictStereoTargetGate(
            "SetRenderTarget", "unmodeled-mrt-binding", nullptr, nullptr,
            gStereoTargetWidth, gStereoTargetHeight);
    }
    NativeFullSizeTargetCandidate* nativeCandidate = nullptr;
    const LONG nativeEye = gNativeActiveEye;
    if (SUCCEEDED(result)
        && !gInStereoReplay
        && gInNativeStereoHook
        && nativeEye >= 0
        && nativeEye <= 1
        && renderTargetIndex == 0)
    {
        nativeCandidate = trackNativeFullSizeTargetCandidate(renderTarget, static_cast<int>(nativeEye));
    }
    if (SUCCEEDED(result) && inspectNativeHdrTransition && previousRenderTarget)
    {
        D3DSURFACE_DESC previousDesc {};
        D3DSURFACE_DESC nextDesc {};
        const bool previousIsFullHdr = SUCCEEDED(previousRenderTarget->GetDesc(&previousDesc))
            && previousDesc.Width == gStereoTargetWidth
            && previousDesc.Height == gStereoTargetHeight
            && previousDesc.Format == D3DFMT_A16B16G16R16F
            && previousDesc.MultiSampleType == D3DMULTISAMPLE_NONE;
        const bool nextIsNativePostprocessTarget = renderTarget
            && SUCCEEDED(renderTarget->GetDesc(&nextDesc))
            // Fallout's first HDR postprocess target is not a fixed quarter
            // scale at every backbuffer size.  It is 512x320 at 2048x1280,
            // but 640x256 at 1280x800.  The stable boundary is the first FP16
            // target no larger than half-size on both axes.  Requiring both
            // axes excludes the 1024x1024 shadow/world targets that previously
            // caused an early, over-broad transition match.
            && nextDesc.Width * 2 <= gStereoTargetWidth
            && nextDesc.Height * 2 <= gStereoTargetHeight
            && nextDesc.Format == D3DFMT_A16B16G16R16F
            && nextDesc.MultiSampleType == D3DMULTISAMPLE_NONE;
        if (previousIsFullHdr && nextIsNativePostprocessTarget)
        {
            gNativePostprocessRecording = true;
            const LONG pairSequence = nextNativeStereoPairSequence();
            if (nativeStereoPairNeedsWarmupAudit(3)
                || fnvxr::shared::sequencedValueBits(pairSequence) % 120u == 0u)
            {
                char message[320] {};
                sprintf_s(
                    message,
                    "native delayed postprocess started at non-MSAA HDR transition pair=%ld previous=%p next=%p nextSize=%ux%u nextFormat=%lu",
                    pairSequence,
                    previousRenderTarget,
                    renderTarget,
                    nextDesc.Width,
                    nextDesc.Height,
                    static_cast<unsigned long>(nextDesc.Format));
                logLine(message);
            }
        }
    }
    releaseSurface(previousRenderTarget);
    if (SUCCEEDED(result)
        && renderTargetIndex == 0
        && strictStereoTargetGateEnabled()
        && !gInStereoReplay)
    {
        if (renderTarget)
            updateMain3DSceneTargetGate(device, renderTarget, "SetRenderTarget");
        else
            markStereoFrameReadyFromSwapAway(device, "SetRenderTarget", "main-3d-scene-unbound", nullptr, nullptr, gStereoTargetWidth, gStereoTargetHeight);
    }
    const LONG call = InterlockedIncrement(&gSetRenderTargetCalls);
    if (!gInStereoReplay)
    {
        int tracePhase = -1;
        LONG traceEvent = 0;
        if (claimNativePipelineTraceEvent(tracePhase, traceEvent))
        {
            char surfaceDesc[256] {};
            describeSurface(surfaceDesc, sizeof(surfaceDesc), renderTarget);
            char stack[320] {};
            if (nativeCandidate)
                formatNativePipelineStack(stack, sizeof(stack), 0);
            char message[992] {};
            sprintf_s(
                message,
                "native pipeline trace phase=%s event=%ld op=SetRenderTarget call=%ld index=%lu result=0x%08lx candidate=%d target=%s stack=%s",
                nativePipelineTracePhaseName(tracePhase),
                traceEvent,
                call,
                static_cast<unsigned long>(renderTargetIndex),
                static_cast<unsigned long>(result),
                nativeCandidate ? 1 : 0,
                surfaceDesc,
                stack);
            logLine(message);
        }
    }
    if (shouldLogResourceGraphCall(call))
    {
        char surfaceDesc[256] {};
        describeSurface(surfaceDesc, sizeof(surfaceDesc), renderTarget);
        char message[448] {};
        sprintf_s(
            message,
            "d3d9 resource graph SetRenderTarget call=%ld frame=%ld result=0x%08lx index=%lu target=%s replay=%d strictActive=%d strictReady=%d",
            call,
            static_cast<LONG>(gPresentFrames),
            static_cast<unsigned long>(result),
            static_cast<unsigned long>(renderTargetIndex),
            surfaceDesc,
            gInStereoReplay ? 1 : 0,
            gIsMain3DSceneActive ? 1 : 0,
            gStereoFrameReadyToPublish ? 1 : 0);
        logLine(message);
    }
    return result;
}

HRESULT WINAPI hookedColorFill(
    IDirect3DDevice9* device,
    IDirect3DSurface9* surface,
    const RECT* rect,
    D3DCOLOR color)
{
    const HRESULT result = gRealColorFill(device, surface, rect, color);
    if (SUCCEEDED(result)
        && gInNativeStereoHook
        && gNativeActiveEye == 2
        && !gInStereoReplay)
    {
        InterlockedIncrement(&gStrictEyeTargetUnprovenWrites);
    }
    return result;
}

HRESULT WINAPI hookedMultiplyTransform(
    IDirect3DDevice9* device,
    D3DTRANSFORMSTATETYPE state,
    const D3DMATRIX* matrix)
{
    const HRESULT result = gRealMultiplyTransform(device, state, matrix);
    if (SUCCEEDED(result)
        && gInNativeStereoHook
        && gNativeActiveEye == 2
        && !gInStereoReplay)
    {
        InterlockedIncrement(&gStrictEyeTargetUnprovenWrites);
    }
    return result;
}

HRESULT WINAPI hookedProcessVertices(
    IDirect3DDevice9* device,
    UINT sourceStartIndex,
    UINT destinationIndex,
    UINT vertexCount,
    IDirect3DVertexBuffer9* destinationBuffer,
    IDirect3DVertexDeclaration9* declaration,
    DWORD flags)
{
    const HRESULT result = gRealProcessVertices(
        device,
        sourceStartIndex,
        destinationIndex,
        vertexCount,
        destinationBuffer,
        declaration,
        flags);
    if (SUCCEEDED(result)
        && gInNativeStereoHook
        && gNativeActiveEye == 2
        && !gInStereoReplay)
    {
        InterlockedIncrement(&gStrictEyeTargetUnprovenWrites);
    }
    return result;
}

HRESULT WINAPI hookedDrawRectPatch(
    IDirect3DDevice9* device,
    UINT handle,
    const float* segments,
    const D3DRECTPATCH_INFO* info)
{
    const HRESULT result = gRealDrawRectPatch(device, handle, segments, info);
    if (SUCCEEDED(result)
        && gInNativeStereoHook
        && gNativeActiveEye == 2
        && !gInStereoReplay)
    {
        InterlockedIncrement(&gStrictEyeTargetUnprovenWrites);
    }
    return result;
}

HRESULT WINAPI hookedDrawTriPatch(
    IDirect3DDevice9* device,
    UINT handle,
    const float* segments,
    const D3DTRIPATCH_INFO* info)
{
    const HRESULT result = gRealDrawTriPatch(device, handle, segments, info);
    if (SUCCEEDED(result)
        && gInNativeStereoHook
        && gNativeActiveEye == 2
        && !gInStereoReplay)
    {
        InterlockedIncrement(&gStrictEyeTargetUnprovenWrites);
    }
    return result;
}

HRESULT WINAPI hookedBeginStateBlock(IDirect3DDevice9* device)
{
    const HRESULT result = gRealBeginStateBlock(device);
    if (SUCCEEDED(result))
    {
        InterlockedExchange(&gStateBlockRecording, 1);
        if (gInNativeStereoHook && gNativeActiveEye == 2)
            InterlockedIncrement(&gStrictEyeTargetUnprovenWrites);
        if (stereoProofModeArmed())
            logLine("BeginStateBlock observed: stereo replay is suspended until a successful EndStateBlock");
    }
    return result;
}

HRESULT WINAPI hookedEndStateBlock(
    IDirect3DDevice9* device,
    IDirect3DStateBlock9** stateBlock)
{
    const HRESULT result = gRealEndStateBlock(device, stateBlock);
    if (SUCCEEDED(result))
        InterlockedExchange(&gStateBlockRecording, 0);
    return result;
}

HRESULT WINAPI hookedCreateQuery(
    IDirect3DDevice9* device,
    D3DQUERYTYPE type,
    IDirect3DQuery9** query)
{
    const HRESULT result = gRealCreateQuery(device, type, query);
    if (SUCCEEDED(result) && query && *query && stereoProofModeArmed())
    {
        InterlockedExchange(&gD3DQueryObjectsObserved, 1);
        if (gInNativeStereoHook && gNativeActiveEye == 2)
            InterlockedIncrement(&gStrictEyeTargetUnprovenWrites);
        logLine("CreateQuery preserved for retail semantics; stereo proof remains fail-closed while query objects exist");
    }
    return result;
}

HRESULT WINAPI hookedSetDepthStencilSurface(IDirect3DDevice9* device, IDirect3DSurface9* depthStencilSurface)
{
    const HRESULT result = gRealSetDepthStencilSurface(device, depthStencilSurface);
    if (SUCCEEDED(result)
        && strictStereoTargetGateEnabled()
        && !gInStereoReplay)
    {
        IDirect3DSurface9* currentTarget = nullptr;
        if (SUCCEEDED(device->GetRenderTarget(0, &currentTarget)) && currentTarget)
        {
            updateMain3DSceneTargetGate(device, currentTarget, "SetDepthStencilSurface");
            releaseSurface(currentTarget);
        }
    }
    const LONG call = InterlockedIncrement(&gSetDepthStencilSurfaceCalls);
    if (shouldLogResourceGraphCall(call))
    {
        char surfaceDesc[256] {};
        describeSurface(surfaceDesc, sizeof(surfaceDesc), depthStencilSurface);
        char message[448] {};
        sprintf_s(
            message,
            "d3d9 resource graph SetDepthStencilSurface call=%ld frame=%ld result=0x%08lx surface=%s replay=%d strictActive=%d strictReady=%d",
            call,
            static_cast<LONG>(gPresentFrames),
            static_cast<unsigned long>(result),
            surfaceDesc,
            gInStereoReplay ? 1 : 0,
            gIsMain3DSceneActive ? 1 : 0,
            gStereoFrameReadyToPublish ? 1 : 0);
        logLine(message);
    }
    return result;
}

HRESULT WINAPI hookedSetTexture(IDirect3DDevice9* device, DWORD sampler, IDirect3DBaseTexture9* texture)
{
    const HRESULT result = gRealSetTexture(device, sampler, texture);
    if (SUCCEEDED(result) && !gInStereoReplay && sampler < MaxTrackedSamplers)
        replaceTrackedTexture(static_cast<UINT>(sampler), texture, false);
    const LONG call = InterlockedIncrement(&gSetTextureCalls);
    const int currentTracePhase = !gInStereoReplay ? nativePipelineTracePhase() : -1;
    IDirect3DBaseTexture9* leftTwinTexture = nullptr;
    IDirect3DBaseTexture9* rightTwinTexture = nullptr;
    if (SUCCEEDED(result)
        && !gInStereoReplay
        && sampler < MaxTrackedSamplers
        && (gInNativeStereoHook || gNativePostprocessFanoutActive || currentTracePhase >= 0))
    {
        leftTwinTexture = stereoTextureFor(texture, true);
        rightTwinTexture = stereoTextureFor(texture, false);
        const DWORD samplerBit = 1u << sampler;
        if (leftTwinTexture && rightTwinTexture)
            gNativePipelineStereoSamplerMask |= samplerBit;
        else
            gNativePipelineStereoSamplerMask &= ~samplerBit;
    }
    if (SUCCEEDED(result) && !gInStereoReplay)
    {
        if (currentTracePhase >= 0)
        {
            D3DSURFACE_DESC levelDesc {};
            const bool haveLevelDesc = baseTextureLevelZeroDesc(texture, levelDesc);
            const bool fullSize = haveLevelDesc
                && levelDesc.Width == gStereoTargetWidth
                && levelDesc.Height == gStereoTargetHeight;
            const bool relevant = currentTracePhase == 2
                || fullSize
                || leftTwinTexture
                || rightTwinTexture;
            int tracePhase = -1;
            LONG traceEvent = 0;
            if (relevant && claimNativePipelineTraceEvent(tracePhase, traceEvent))
            {
                char textureDesc[256] {};
                describeBaseTexture(textureDesc, sizeof(textureDesc), texture);
                char stack[320] {};
                char stackScan[640] {};
                if (fullSize || leftTwinTexture || rightTwinTexture)
                {
                    formatNativePipelineStack(stack, sizeof(stack), 0);
                    formatNativePipelineStackScan(stackScan, sizeof(stackScan));
                }
                char message[1728] {};
                sprintf_s(
                    message,
                    "native pipeline trace phase=%s event=%ld op=SetTexture call=%ld sampler=%lu result=0x%08lx fullSize=%d stereoTwin=%d/%d texture=%s stack=%s stackScan=%s",
                    nativePipelineTracePhaseName(tracePhase),
                    traceEvent,
                    call,
                    static_cast<unsigned long>(sampler),
                    static_cast<unsigned long>(result),
                    fullSize ? 1 : 0,
                    leftTwinTexture ? 1 : 0,
                    rightTwinTexture ? 1 : 0,
                    textureDesc,
                    stack,
                    stackScan);
                logLine(message);
            }
        }
    }
    if (shouldLogResourceGraphCall(call))
    {
        char textureDesc[256] {};
        describeBaseTexture(textureDesc, sizeof(textureDesc), texture);
        char message[448] {};
        sprintf_s(
            message,
            "d3d9 resource graph SetTexture call=%ld frame=%ld result=0x%08lx sampler=%lu texture=%s replay=%d",
            call,
            static_cast<LONG>(gPresentFrames),
            static_cast<unsigned long>(result),
            static_cast<unsigned long>(sampler),
            textureDesc,
            gInStereoReplay ? 1 : 0);
        logLine(message);
    }
    return result;
}

HRESULT WINAPI hookedSetTextureStageState(
    IDirect3DDevice9* device,
    DWORD stage,
    D3DTEXTURESTAGESTATETYPE type,
    DWORD value)
{
    const HRESULT result = gRealSetTextureStageState(device, stage, type, value);
    const LONG call = InterlockedIncrement(&gSetTextureStageStateCalls);
    if (shouldLogResourceGraphCall(call))
    {
        char message[256] {};
        sprintf_s(
            message,
            "d3d9 resource graph SetTextureStageState call=%ld frame=%ld result=0x%08lx stage=%lu type=%u value=0x%lx replay=%d",
            call,
            static_cast<LONG>(gPresentFrames),
            static_cast<unsigned long>(result),
            static_cast<unsigned long>(stage),
            static_cast<unsigned>(type),
            static_cast<unsigned long>(value),
            gInStereoReplay ? 1 : 0);
        logLine(message);
    }
    return result;
}

bool criticalDeviceHooksIntact(IDirect3DDevice9* device)
{
    if (!device || !gHookedDeviceIdentity || !gHookedDeviceVtable)
        return false;

    IUnknown* identity = nullptr;
    if (FAILED(device->QueryInterface(__uuidof(IUnknown), reinterpret_cast<void**>(&identity)))
        || !identity)
    {
        gCriticalDeviceHooksReady = false;
        return false;
    }
    const bool identityMatches = identity == gHookedDeviceIdentity;
    identity->Release();
    void** vtable = *reinterpret_cast<void***>(device);
    if (!identityMatches || vtable != gHookedDeviceVtable)
    {
        gCriticalDeviceHooksReady = false;
        return false;
    }

    struct RequiredHook
    {
        UINT slot;
        void* function;
    };
    const RequiredHook required[] {
        { 16, reinterpret_cast<void*>(&hookedReset) },
        { 17, reinterpret_cast<void*>(&hookedPresent) },
        { 23, reinterpret_cast<void*>(&hookedCreateTexture) },
        { 28, reinterpret_cast<void*>(&hookedCreateRenderTarget) },
        { 29, reinterpret_cast<void*>(&hookedCreateDepthStencilSurface) },
        { 30, reinterpret_cast<void*>(&hookedUpdateSurface) },
        { 31, reinterpret_cast<void*>(&hookedUpdateTexture) },
        { 34, reinterpret_cast<void*>(&hookedStretchRect) },
        { 35, reinterpret_cast<void*>(&hookedColorFill) },
        { 37, reinterpret_cast<void*>(&hookedSetRenderTarget) },
        { 39, reinterpret_cast<void*>(&hookedSetDepthStencilSurface) },
        { 60, reinterpret_cast<void*>(&hookedBeginStateBlock) },
        { 61, reinterpret_cast<void*>(&hookedEndStateBlock) },
        { 42, reinterpret_cast<void*>(&hookedEndScene) },
        { 43, reinterpret_cast<void*>(&hookedClear) },
        { 44, reinterpret_cast<void*>(&hookedSetTransform) },
        { 46, reinterpret_cast<void*>(&hookedMultiplyTransform) },
        { 65, reinterpret_cast<void*>(&hookedSetTexture) },
        { 67, reinterpret_cast<void*>(&hookedSetTextureStageState) },
        { 81, reinterpret_cast<void*>(&hookedDrawPrimitive) },
        { 82, reinterpret_cast<void*>(&hookedDrawIndexedPrimitive) },
        { 83, reinterpret_cast<void*>(&hookedDrawPrimitiveUP) },
        { 84, reinterpret_cast<void*>(&hookedDrawIndexedPrimitiveUP) },
        { 85, reinterpret_cast<void*>(&hookedProcessVertices) },
        { 92, reinterpret_cast<void*>(&hookedSetVertexShader) },
        { 94, reinterpret_cast<void*>(&hookedSetVertexShaderConstantF) },
        { 107, reinterpret_cast<void*>(&hookedSetPixelShader) },
        { 115, reinterpret_cast<void*>(&hookedDrawRectPatch) },
        { 116, reinterpret_cast<void*>(&hookedDrawTriPatch) },
        { 118, reinterpret_cast<void*>(&hookedCreateQuery) }
    };
    static_assert(sizeof(required) / sizeof(required[0]) < 64, "critical hook mask overflow");
    std::uint64_t currentMask = 0;
    __try
    {
        for (UINT index = 0; index < sizeof(required) / sizeof(required[0]); ++index)
        {
            if (vtable[required[index].slot] == required[index].function)
                currentMask |= (std::uint64_t { 1 } << index);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        currentMask = 0;
    }
    const std::uint64_t expectedMask =
        (std::uint64_t { 1 } << (sizeof(required) / sizeof(required[0]))) - 1u;
    gCriticalDeviceHookMask = currentMask;
    gExpectedCriticalDeviceHookMask = expectedMask;
    gCriticalDeviceHooksReady = currentMask == expectedMask;
    return gCriticalDeviceHooksReady;
}

bool installDeviceHooks(IDirect3DDevice9* device)
{
    if constexpr (!fnvxr::d3d9::ProductionRendererAuthorized)
        return false;

    if (!device)
        return false;

    void** vtable = *reinterpret_cast<void***>(device);
    IUnknown* deviceIdentity = nullptr;
    if (FAILED(device->QueryInterface(
            __uuidof(IUnknown), reinterpret_cast<void**>(&deviceIdentity)))
        || !deviceIdentity)
    {
        return false;
    }
    if (gHookedDeviceIdentity && gHookedDeviceIdentity != deviceIdentity)
    {
        deviceIdentity->Release();
        gCriticalDeviceHooksReady = false;
        logLine("critical D3D9 hook install rejected: a second device identity appeared");
        return false;
    }
    if (!gHookedDeviceIdentity)
        gHookedDeviceIdentity = deviceIdentity;
    else
        deviceIdentity->Release();
    if (gHookedDeviceVtable && gHookedDeviceVtable != vtable)
    {
        gCriticalDeviceHooksReady = false;
        logLine("critical D3D9 hook install rejected: a second device vtable appeared");
        return false;
    }
    if (!gRealReset
        && patchVTableSlot(vtable, 16, reinterpret_cast<void*>(&hookedReset), reinterpret_cast<void**>(&gRealReset)))
    {
        logLine("hooked IDirect3DDevice9::Reset");
    }

    if (!gRealPresent
        && patchVTableSlot(vtable, 17, reinterpret_cast<void*>(&hookedPresent), reinterpret_cast<void**>(&gRealPresent)))
    {
        logLine("hooked IDirect3DDevice9::Present");
    }

    if (!gRealCreateTexture
        && patchVTableSlot(
            vtable, 23, reinterpret_cast<void*>(&hookedCreateTexture), reinterpret_cast<void**>(&gRealCreateTexture)))
    {
        logLine("hooked IDirect3DDevice9::CreateTexture");
    }

    if (!gRealCreateRenderTarget
        && patchVTableSlot(
            vtable,
            28,
            reinterpret_cast<void*>(&hookedCreateRenderTarget),
            reinterpret_cast<void**>(&gRealCreateRenderTarget)))
    {
        logLine("hooked IDirect3DDevice9::CreateRenderTarget");
    }

    if (!gRealCreateDepthStencilSurface
        && patchVTableSlot(
            vtable,
            29,
            reinterpret_cast<void*>(&hookedCreateDepthStencilSurface),
            reinterpret_cast<void**>(&gRealCreateDepthStencilSurface)))
    {
        logLine("hooked IDirect3DDevice9::CreateDepthStencilSurface");
    }

    if (!gRealUpdateSurface
        && patchVTableSlot(
            vtable, 30, reinterpret_cast<void*>(&hookedUpdateSurface), reinterpret_cast<void**>(&gRealUpdateSurface)))
    {
        logLine("hooked IDirect3DDevice9::UpdateSurface");
    }

    if (!gRealUpdateTexture
        && patchVTableSlot(
            vtable, 31, reinterpret_cast<void*>(&hookedUpdateTexture), reinterpret_cast<void**>(&gRealUpdateTexture)))
    {
        logLine("hooked IDirect3DDevice9::UpdateTexture");
    }

    if (!gRealStretchRect
        && patchVTableSlot(
            vtable, 34, reinterpret_cast<void*>(&hookedStretchRect), reinterpret_cast<void**>(&gRealStretchRect)))
    {
        logLine("hooked IDirect3DDevice9::StretchRect");
    }

    if (!gRealColorFill
        && patchVTableSlot(
            vtable, 35, reinterpret_cast<void*>(&hookedColorFill), reinterpret_cast<void**>(&gRealColorFill)))
    {
        logLine("hooked IDirect3DDevice9::ColorFill");
    }

    if (!gRealSetRenderTarget
        && patchVTableSlot(
            vtable, 37, reinterpret_cast<void*>(&hookedSetRenderTarget), reinterpret_cast<void**>(&gRealSetRenderTarget)))
    {
        logLine("hooked IDirect3DDevice9::SetRenderTarget");
    }

    if (!gRealSetDepthStencilSurface
        && patchVTableSlot(
            vtable,
            39,
            reinterpret_cast<void*>(&hookedSetDepthStencilSurface),
            reinterpret_cast<void**>(&gRealSetDepthStencilSurface)))
    {
        logLine("hooked IDirect3DDevice9::SetDepthStencilSurface");
    }

    if (!gRealBeginStateBlock
        && patchVTableSlot(
            vtable,
            60,
            reinterpret_cast<void*>(&hookedBeginStateBlock),
            reinterpret_cast<void**>(&gRealBeginStateBlock)))
    {
        logLine("hooked IDirect3DDevice9::BeginStateBlock");
    }

    if (!gRealEndStateBlock
        && patchVTableSlot(
            vtable,
            61,
            reinterpret_cast<void*>(&hookedEndStateBlock),
            reinterpret_cast<void**>(&gRealEndStateBlock)))
    {
        logLine("hooked IDirect3DDevice9::EndStateBlock");
    }

    if (!gRealEndScene
        && patchVTableSlot(vtable, 42, reinterpret_cast<void*>(&hookedEndScene), reinterpret_cast<void**>(&gRealEndScene)))
    {
        logLine("hooked IDirect3DDevice9::EndScene");
    }

    if (!gRealClear
        && patchVTableSlot(vtable, 43, reinterpret_cast<void*>(&hookedClear), reinterpret_cast<void**>(&gRealClear)))
    {
        logLine("hooked IDirect3DDevice9::Clear");
    }

    if (!gRealSetTransform
        && patchVTableSlot(
            vtable, 44, reinterpret_cast<void*>(&hookedSetTransform), reinterpret_cast<void**>(&gRealSetTransform)))
    {
        logLine("hooked IDirect3DDevice9::SetTransform");
    }

    if (!gRealMultiplyTransform
        && patchVTableSlot(
            vtable,
            46,
            reinterpret_cast<void*>(&hookedMultiplyTransform),
            reinterpret_cast<void**>(&gRealMultiplyTransform)))
    {
        logLine("hooked IDirect3DDevice9::MultiplyTransform");
    }

    if (!gRealSetVertexShader
        && patchVTableSlot(
            vtable, 92, reinterpret_cast<void*>(&hookedSetVertexShader), reinterpret_cast<void**>(&gRealSetVertexShader)))
    {
        logLine("hooked IDirect3DDevice9::SetVertexShader");
    }

    if (!gRealSetVertexShaderConstantF
        && patchVTableSlot(
            vtable,
            94,
            reinterpret_cast<void*>(&hookedSetVertexShaderConstantF),
            reinterpret_cast<void**>(&gRealSetVertexShaderConstantF)))
    {
        logLine("hooked IDirect3DDevice9::SetVertexShaderConstantF");
    }

    if (!gRealSetPixelShader
        && patchVTableSlot(
            vtable, 107, reinterpret_cast<void*>(&hookedSetPixelShader), reinterpret_cast<void**>(&gRealSetPixelShader)))
    {
        logLine("hooked IDirect3DDevice9::SetPixelShader");
    }

    if (!gRealSetTexture
        && patchVTableSlot(vtable, 65, reinterpret_cast<void*>(&hookedSetTexture), reinterpret_cast<void**>(&gRealSetTexture)))
    {
        logLine("hooked IDirect3DDevice9::SetTexture");
    }

    if (!gRealSetTextureStageState
        && patchVTableSlot(
            vtable,
            67,
            reinterpret_cast<void*>(&hookedSetTextureStageState),
            reinterpret_cast<void**>(&gRealSetTextureStageState)))
    {
        logLine("hooked IDirect3DDevice9::SetTextureStageState");
    }

    if (!gRealDrawPrimitive
        && patchVTableSlot(
            vtable, 81, reinterpret_cast<void*>(&hookedDrawPrimitive), reinterpret_cast<void**>(&gRealDrawPrimitive)))
    {
        logLine("hooked IDirect3DDevice9::DrawPrimitive");
    }

    if (!gRealDrawIndexedPrimitive
        && patchVTableSlot(
            vtable,
            82,
            reinterpret_cast<void*>(&hookedDrawIndexedPrimitive),
            reinterpret_cast<void**>(&gRealDrawIndexedPrimitive)))
    {
        logLine("hooked IDirect3DDevice9::DrawIndexedPrimitive");
    }

    if (!gRealDrawPrimitiveUP
        && patchVTableSlot(
            vtable, 83, reinterpret_cast<void*>(&hookedDrawPrimitiveUP), reinterpret_cast<void**>(&gRealDrawPrimitiveUP)))
    {
        logLine("hooked IDirect3DDevice9::DrawPrimitiveUP");
    }

    if (!gRealDrawIndexedPrimitiveUP
        && patchVTableSlot(
            vtable,
            84,
            reinterpret_cast<void*>(&hookedDrawIndexedPrimitiveUP),
            reinterpret_cast<void**>(&gRealDrawIndexedPrimitiveUP)))
    {
        logLine("hooked IDirect3DDevice9::DrawIndexedPrimitiveUP");
    }


    if (!gRealProcessVertices
        && patchVTableSlot(
            vtable,
            85,
            reinterpret_cast<void*>(&hookedProcessVertices),
            reinterpret_cast<void**>(&gRealProcessVertices)))
    {
        logLine("hooked IDirect3DDevice9::ProcessVertices");
    }

    if (!gRealDrawRectPatch
        && patchVTableSlot(
            vtable,
            115,
            reinterpret_cast<void*>(&hookedDrawRectPatch),
            reinterpret_cast<void**>(&gRealDrawRectPatch)))
    {
        logLine("hooked IDirect3DDevice9::DrawRectPatch");
    }

    if (!gRealDrawTriPatch
        && patchVTableSlot(
            vtable,
            116,
            reinterpret_cast<void*>(&hookedDrawTriPatch),
            reinterpret_cast<void**>(&gRealDrawTriPatch)))
    {
        logLine("hooked IDirect3DDevice9::DrawTriPatch");
    }

    if (!gRealCreateQuery
        && patchVTableSlot(
            vtable,
            118,
            reinterpret_cast<void*>(&hookedCreateQuery),
            reinterpret_cast<void**>(&gRealCreateQuery)))
    {
        logLine("hooked IDirect3DDevice9::CreateQuery");
    }

    installNativeStereoEngineHook(device);

    struct RequiredHook
    {
        UINT slot;
        void* function;
    };
    const RequiredHook required[] {
        { 16, reinterpret_cast<void*>(&hookedReset) },
        { 17, reinterpret_cast<void*>(&hookedPresent) },
        { 23, reinterpret_cast<void*>(&hookedCreateTexture) },
        { 28, reinterpret_cast<void*>(&hookedCreateRenderTarget) },
        { 29, reinterpret_cast<void*>(&hookedCreateDepthStencilSurface) },
        { 30, reinterpret_cast<void*>(&hookedUpdateSurface) },
        { 31, reinterpret_cast<void*>(&hookedUpdateTexture) },
        { 34, reinterpret_cast<void*>(&hookedStretchRect) },
        { 35, reinterpret_cast<void*>(&hookedColorFill) },
        { 37, reinterpret_cast<void*>(&hookedSetRenderTarget) },
        { 39, reinterpret_cast<void*>(&hookedSetDepthStencilSurface) },
        { 60, reinterpret_cast<void*>(&hookedBeginStateBlock) },
        { 61, reinterpret_cast<void*>(&hookedEndStateBlock) },
        { 42, reinterpret_cast<void*>(&hookedEndScene) },
        { 43, reinterpret_cast<void*>(&hookedClear) },
        { 44, reinterpret_cast<void*>(&hookedSetTransform) },
        { 46, reinterpret_cast<void*>(&hookedMultiplyTransform) },
        { 65, reinterpret_cast<void*>(&hookedSetTexture) },
        { 67, reinterpret_cast<void*>(&hookedSetTextureStageState) },
        { 81, reinterpret_cast<void*>(&hookedDrawPrimitive) },
        { 82, reinterpret_cast<void*>(&hookedDrawIndexedPrimitive) },
        { 83, reinterpret_cast<void*>(&hookedDrawPrimitiveUP) },
        { 84, reinterpret_cast<void*>(&hookedDrawIndexedPrimitiveUP) },
        { 85, reinterpret_cast<void*>(&hookedProcessVertices) },
        { 92, reinterpret_cast<void*>(&hookedSetVertexShader) },
        { 94, reinterpret_cast<void*>(&hookedSetVertexShaderConstantF) },
        { 107, reinterpret_cast<void*>(&hookedSetPixelShader) },
        { 115, reinterpret_cast<void*>(&hookedDrawRectPatch) },
        { 116, reinterpret_cast<void*>(&hookedDrawTriPatch) },
        { 118, reinterpret_cast<void*>(&hookedCreateQuery) }
    };
    static_assert(sizeof(required) / sizeof(required[0]) < 64, "critical hook mask overflow");
    std::uint64_t installedMask = 0;
    for (UINT index = 0; index < sizeof(required) / sizeof(required[0]); ++index)
    {
        if (vtable[required[index].slot] == required[index].function)
            installedMask |= (std::uint64_t { 1 } << index);
    }
    const std::uint64_t expectedMask =
        (std::uint64_t { 1 } << (sizeof(required) / sizeof(required[0]))) - 1u;
    gHookedDeviceVtable = vtable;
    gCriticalDeviceHookMask = installedMask;
    gExpectedCriticalDeviceHookMask = expectedMask;
    gCriticalDeviceHooksReady = installedMask == expectedMask;
    char integrity[256] {};
    sprintf_s(
        integrity,
        "critical D3D9 hook mask installed=0x%016llx expected=0x%016llx ready=%d",
        static_cast<unsigned long long>(installedMask),
        static_cast<unsigned long long>(expectedMask),
        gCriticalDeviceHooksReady ? 1 : 0);
    logLine(integrity);
    return gCriticalDeviceHooksReady;
}

bool buildLogPath(char* path, size_t pathSize, const char* leafName)
{
    if (!path || pathSize == 0 || !leafName)
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
        return strcat_s(path, pathSize, leafName) == 0;
    }

    DWORD length = GetModuleFileNameA(nullptr, path, static_cast<DWORD>(pathSize));
    if (length == 0 || length >= pathSize)
        return false;

    char* slash = strrchr(path, '\\');
    if (slash)
        slash[1] = '\0';

    return strcat_s(path, pathSize, leafName) == 0;
}

void logLine(const char* text)
{
    if constexpr (!fnvxr::d3d9::ProductionRendererAuthorized)
        return;

    char path[MAX_PATH] {};
    if (!buildLogPath(path, sizeof(path), "fnvxr_d3d9_proxy.log"))
        return;

    FILE* file = nullptr;
    if (fopen_s(&file, path, "ab") != 0 || !file)
        return;

    SYSTEMTIME time {};
    GetLocalTime(&time);
    fprintf(
        file,
        "%04u-%02u-%02u %02u:%02u:%02u.%03u %s\r\n",
        time.wYear,
        time.wMonth,
        time.wDay,
        time.wHour,
        time.wMinute,
        time.wSecond,
        time.wMilliseconds,
        text);
    fclose(file);
}

void loadStereoConfig()
{
    gIpdMeters = readEnvFloat("FNVXR_D3D9_IPD_METERS", fnvxr::stereo::DefaultIpdMeters);
    if (gIpdMeters <= 0.0f || gIpdMeters > 0.12f)
        gIpdMeters = fnvxr::stereo::DefaultIpdMeters;
    gGameUnitsPerMeter =
        readEnvFloat("FNVXR_D3D9_GAME_UNITS_PER_METER", fnvxr::stereo::DefaultGameUnitsPerMeter);
    if (gGameUnitsPerMeter <= 0.0f || gGameUnitsPerMeter > 10000.0f)
        gGameUnitsPerMeter = fnvxr::stereo::DefaultGameUnitsPerMeter;
    gIpdGameUnits = readEnvFloat("FNVXR_D3D9_IPD_GAME_UNITS", gIpdMeters * gGameUnitsPerMeter);
    if (gIpdGameUnits <= 0.0f || gIpdGameUnits > 12.0f)
        gIpdGameUnits = gIpdMeters * gGameUnitsPerMeter;
    const bool stereoWorldEnabled = stereoWorldRuntimeEnabled();
    if (stereoWorldRuntimeRequested() && !StereoWorldProductionProofComplete)
    {
        logLine("stereo world runtime hard-blocked in D3D9: structural production proof incomplete");
    }
    gNativeSingleTraversalReplayEnabled =
        stereoWorldEnabled
        && readEnvBool("FNVXR_D3D9_NATIVE_SINGLE_TRAVERSAL_REPLAY", true);
    gNativeStereoEnabled =
        stereoWorldEnabled
        && !gNativeSingleTraversalReplayEnabled
        && readEnvBool("FNVXR_D3D9_NATIVE_MULTIPASS", false);
    gStereoReplayEnabled =
        stereoWorldEnabled
        && (gNativeSingleTraversalReplayEnabled
            || (!gNativeStereoEnabled && readEnvBool("FNVXR_D3D9_STEREO_REPLAY", true)));
    gStereoReadbackEnabled =
        stereoWorldEnabled
        && (readEnvBool("FNVXR_D3D9_STEREO_READBACK", true) || sharedStereoEnabled());
    gWideWorldReplayEnabled = readEnvBool("FNVXR_D3D9_WIDE_WORLD_REPLAY", false);

    const bool shaderStereo = stereoWorldEnabled && readEnvBool("FNVXR_D3D9_SHADER_STEREO", false);
    const bool shaderWvpReplay = readEnvBool("FNVXR_D3D9_SHADER_WVP_REPLAY", true);
    const bool replayStateBlock = readEnvBool("FNVXR_D3D9_STEREO_STATEBLOCK_RESTORE", true);
    const bool replayPrimeTargets = readEnvBool("FNVXR_D3D9_STEREO_PRIME_REPLAY_TARGETS", true);
    const bool hammer = telemetryHammerEnabled();
    char message[768] {};
    sprintf_s(
        message,
        "stereo config runtime=%d singleTraversal=%d nativeMultipass=%d ipdMeters=%.4f ipdGame=%.4f gameUnitsPerMeter=%.4f replay=%d readback=%d sharedStereo=%d shaderStereo=%d shaderWvpReplay=%d stateBlock=%d primeTargets=%d wideWorldReplay=%d telemetryHammer=%d hammerWarmup=%ld replayStride=%ld wvpStride=%ld clearStride=%ld",
        stereoWorldEnabled ? 1 : 0,
        gNativeSingleTraversalReplayEnabled ? 1 : 0,
        gNativeStereoEnabled ? 1 : 0,
        gIpdMeters,
        gIpdGameUnits,
        gGameUnitsPerMeter,
        gStereoReplayEnabled ? 1 : 0,
        gStereoReadbackEnabled ? 1 : 0,
        sharedStereoEnabled() ? 1 : 0,
        shaderStereo ? 1 : 0,
        shaderWvpReplay ? 1 : 0,
        replayStateBlock ? 1 : 0,
        replayPrimeTargets ? 1 : 0,
        gWideWorldReplayEnabled ? 1 : 0,
        hammer ? 1 : 0,
        static_cast<LONG>(readEnvFloat("FNVXR_D3D9_TELEMETRY_HAMMER_WARMUP", 240.0f)),
        telemetryHammerStride("FNVXR_D3D9_REPLAY_DRAW_TELEMETRY_STRIDE", 1),
        telemetryHammerStride("FNVXR_D3D9_WVP_TELEMETRY_STRIDE", 1),
        telemetryHammerStride("FNVXR_D3D9_CLEAR_TELEMETRY_STRIDE", 1));
    logLine(message);
    if (InterlockedIncrement(&gLoggedD3d9TelemetryHammerConfig) <= 1)
    {
        char event[512] {};
        sprintf_s(
            event,
            "{\"event\":\"fnvxrD3d9TelemetryHammerConfig\",\"enabled\":%d,"
            "\"warmup\":%ld,\"replayDrawStride\":%ld,\"wvpStride\":%ld,"
            "\"clearStride\":%ld,\"targetStride\":%ld,\"stateBlockStride\":%ld}",
            hammer ? 1 : 0,
            static_cast<LONG>(readEnvFloat("FNVXR_D3D9_TELEMETRY_HAMMER_WARMUP", 240.0f)),
            telemetryHammerStride("FNVXR_D3D9_REPLAY_DRAW_TELEMETRY_STRIDE", 1),
            telemetryHammerStride("FNVXR_D3D9_WVP_TELEMETRY_STRIDE", 1),
            telemetryHammerStride("FNVXR_D3D9_CLEAR_TELEMETRY_STRIDE", 1),
            telemetryHammerStride("FNVXR_D3D9_REPLAY_TARGET_TELEMETRY_STRIDE", 1),
            telemetryHammerStride("FNVXR_D3D9_STATEBLOCK_TELEMETRY_STRIDE", 1));
        logLine(event);
    }
}

BOOL CALLBACK initializeD3D9ProxyOnce(PINIT_ONCE, PVOID, PVOID*)
{
    loadStereoConfig();
    logLine("fnvxr d3d9 proxy initialized outside loader lock");
    return TRUE;
}

bool ensureD3D9ProxyInitialized()
{
    return InitOnceExecuteOnce(
        &gD3D9ProxyInitializeOnce,
        initializeD3D9ProxyOnce,
        nullptr,
        nullptr) != FALSE;
}

bool loadRealD3D9()
{
    if (gRealD3D9)
        return true;

    char systemPath[MAX_PATH] {};
    const UINT length = GetSystemDirectoryA(systemPath, static_cast<UINT>(sizeof(systemPath)));
    if (length == 0 || length >= sizeof(systemPath))
        return false;

    strcat_s(systemPath, "\\d3d9.dll");
    gRealD3D9 = LoadLibraryA(systemPath);
    if (!gRealD3D9)
        return false;

    gRealDirect3DCreate9 = reinterpret_cast<Direct3DCreate9Fn>(GetProcAddress(gRealD3D9, "Direct3DCreate9"));
    gRealDirect3DCreate9Ex = reinterpret_cast<Direct3DCreate9ExFn>(GetProcAddress(gRealD3D9, "Direct3DCreate9Ex"));

    if (!gRealDirect3DCreate9)
        return false;

    return true;
}

class Direct3D9Proxy final : public IDirect3D9
{
public:
    explicit Direct3D9Proxy(IDirect3D9* real)
        : mReal(real)
    {
    }

    ~Direct3D9Proxy()
    {
        if (mReal)
            mReal->Release();
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override
    {
        if (!out)
            return E_POINTER;
        *out = nullptr;

        if (riid == IID_IUnknown || riid == IID_IDirect3D9)
        {
            *out = static_cast<IDirect3D9*>(this);
            AddRef();
            return S_OK;
        }

        if (stereoProofModeArmed())
        {
            logLine("Direct3D9 QueryInterface rejected: interface is outside the stereo proof");
            return E_NOINTERFACE;
        }
        return mReal->QueryInterface(riid, out);
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return InterlockedIncrement(&mRefs);
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG refs = InterlockedDecrement(&mRefs);
        if (refs == 0)
            delete this;
        return refs;
    }

    HRESULT STDMETHODCALLTYPE RegisterSoftwareDevice(void* initializeFunction) override
    {
        return mReal->RegisterSoftwareDevice(initializeFunction);
    }

    UINT STDMETHODCALLTYPE GetAdapterCount() override
    {
        return mReal->GetAdapterCount();
    }

    HRESULT STDMETHODCALLTYPE GetAdapterIdentifier(UINT adapter, DWORD flags, D3DADAPTER_IDENTIFIER9* identifier) override
    {
        return mReal->GetAdapterIdentifier(adapter, flags, identifier);
    }

    UINT STDMETHODCALLTYPE GetAdapterModeCount(UINT adapter, D3DFORMAT format) override
    {
        return mReal->GetAdapterModeCount(adapter, format);
    }

    HRESULT STDMETHODCALLTYPE EnumAdapterModes(UINT adapter, D3DFORMAT format, UINT mode, D3DDISPLAYMODE* displayMode) override
    {
        return mReal->EnumAdapterModes(adapter, format, mode, displayMode);
    }

    HRESULT STDMETHODCALLTYPE GetAdapterDisplayMode(UINT adapter, D3DDISPLAYMODE* displayMode) override
    {
        return mReal->GetAdapterDisplayMode(adapter, displayMode);
    }

    HRESULT STDMETHODCALLTYPE CheckDeviceType(
        UINT adapter,
        D3DDEVTYPE deviceType,
        D3DFORMAT adapterFormat,
        D3DFORMAT backBufferFormat,
        BOOL windowed) override
    {
        return mReal->CheckDeviceType(adapter, deviceType, adapterFormat, backBufferFormat, windowed);
    }

    HRESULT STDMETHODCALLTYPE CheckDeviceFormat(
        UINT adapter,
        D3DDEVTYPE deviceType,
        D3DFORMAT adapterFormat,
        DWORD usage,
        D3DRESOURCETYPE resourceType,
        D3DFORMAT checkFormat) override
    {
        return mReal->CheckDeviceFormat(adapter, deviceType, adapterFormat, usage, resourceType, checkFormat);
    }

    HRESULT STDMETHODCALLTYPE CheckDeviceMultiSampleType(
        UINT adapter,
        D3DDEVTYPE deviceType,
        D3DFORMAT surfaceFormat,
        BOOL windowed,
        D3DMULTISAMPLE_TYPE multiSampleType,
        DWORD* qualityLevels) override
    {
        return mReal->CheckDeviceMultiSampleType(
            adapter, deviceType, surfaceFormat, windowed, multiSampleType, qualityLevels);
    }

    HRESULT STDMETHODCALLTYPE CheckDepthStencilMatch(
        UINT adapter,
        D3DDEVTYPE deviceType,
        D3DFORMAT adapterFormat,
        D3DFORMAT renderTargetFormat,
        D3DFORMAT depthStencilFormat) override
    {
        return mReal->CheckDepthStencilMatch(
            adapter, deviceType, adapterFormat, renderTargetFormat, depthStencilFormat);
    }

    HRESULT STDMETHODCALLTYPE CheckDeviceFormatConversion(
        UINT adapter,
        D3DDEVTYPE deviceType,
        D3DFORMAT sourceFormat,
        D3DFORMAT targetFormat) override
    {
        return mReal->CheckDeviceFormatConversion(adapter, deviceType, sourceFormat, targetFormat);
    }

    HRESULT STDMETHODCALLTYPE GetDeviceCaps(UINT adapter, D3DDEVTYPE deviceType, D3DCAPS9* caps) override
    {
        return mReal->GetDeviceCaps(adapter, deviceType, caps);
    }

    HMONITOR STDMETHODCALLTYPE GetAdapterMonitor(UINT adapter) override
    {
        return mReal->GetAdapterMonitor(adapter);
    }

    HRESULT STDMETHODCALLTYPE CreateDevice(
        UINT adapter,
        D3DDEVTYPE deviceType,
        HWND focusWindow,
        DWORD behaviorFlags,
        D3DPRESENT_PARAMETERS* presentationParameters,
        IDirect3DDevice9** returnedDevice) override
    {
        forceImmediatePresentation(presentationParameters, "CreateDevice");
        gDeviceBehaviorFlags = behaviorFlags;
        if (stereoProofModeArmed() && (behaviorFlags & D3DCREATE_MULTITHREADED) != 0)
        {
            if (returnedDevice)
                *returnedDevice = nullptr;
            logLine("CreateDevice rejected: D3DCREATE_MULTITHREADED is outside the single-owner stereo proof");
            return D3DERR_NOTAVAILABLE_RESULT;
        }
        gPrimaryBackBufferLockable = presentationParameters
            && (presentationParameters->Flags & D3DPRESENTFLAG_LOCKABLE_BACKBUFFER) != 0;
        char message[256] {};
        if (presentationParameters)
        {
            sprintf_s(
                message,
                "CreateDevice adapter=%u windowed=%d backbuffer=%ux%u format=%u interval=%u refresh=%u",
                adapter,
                presentationParameters->Windowed,
                presentationParameters->BackBufferWidth,
                presentationParameters->BackBufferHeight,
                static_cast<unsigned>(presentationParameters->BackBufferFormat),
                presentationParameters->PresentationInterval,
                presentationParameters->FullScreen_RefreshRateInHz);
        }
        else
        {
            sprintf_s(message, "CreateDevice adapter=%u with null presentation parameters", adapter);
        }
        logLine(message);

        const HRESULT result = mReal->CreateDevice(
            adapter, deviceType, focusWindow, behaviorFlags, presentationParameters, returnedDevice);
        logLine(SUCCEEDED(result) ? "CreateDevice succeeded" : "CreateDevice failed");
        if (SUCCEEDED(result) && returnedDevice && *returnedDevice
            && !installDeviceHooks(*returnedDevice)
            && stereoProofModeArmed())
        {
            (*returnedDevice)->Release();
            *returnedDevice = nullptr;
            logLine("CreateDevice rejected: critical stereo hook mask incomplete");
            return D3DERR_NOTAVAILABLE_RESULT;
        }
        return result;
    }

private:
    IDirect3D9* mReal = nullptr;
    volatile LONG mRefs = 1;
};
}

extern "C" IDirect3D9* WINAPI FNVXR_Direct3DCreate9(UINT sdkVersion)
{
    if (!loadRealD3D9())
        return nullptr;

    IDirect3D9* real = gRealDirect3DCreate9(sdkVersion);
    if constexpr (!fnvxr::d3d9::ProductionRendererAuthorized)
        return real;

    if (!ensureD3D9ProxyInitialized())
    {
        if (real)
            real->Release();
        return nullptr;
    }
    logLine("Direct3DCreate9 called");
    if (!real)
    {
        logLine("Direct3DCreate9 returned null");
        return nullptr;
    }

    Direct3D9Proxy* proxy = new (std::nothrow) Direct3D9Proxy(real);
    if (!proxy)
    {
        real->Release();
        logLine("Direct3DCreate9 proxy allocation failed");
        return nullptr;
    }

    logLine("Direct3DCreate9 wrapped successfully");
    return proxy;
}

extern "C" HRESULT WINAPI FNVXR_Direct3DCreate9Ex(UINT sdkVersion, IDirect3D9Ex** out)
{
    if (!loadRealD3D9() || !gRealDirect3DCreate9Ex)
    {
        if (out)
            *out = nullptr;
        return D3DERR_NOTAVAILABLE_RESULT;
    }

    if constexpr (!fnvxr::d3d9::ProductionRendererAuthorized)
        return gRealDirect3DCreate9Ex(sdkVersion, out);

    if (!out)
        return E_POINTER;
    *out = nullptr;

    if (!ensureD3D9ProxyInitialized())
        return D3DERR_NOTAVAILABLE_RESULT;
    logLine("Direct3DCreate9Ex called");

    if (stereoProofModeArmed())
    {
        logLine("Direct3DCreate9Ex rejected: Ex vtable is not covered by the stereo proof");
        return D3DERR_NOTAVAILABLE_RESULT;
    }

    const HRESULT result = gRealDirect3DCreate9Ex(sdkVersion, out);
    logLine(SUCCEEDED(result) ? "Direct3DCreate9Ex forwarded successfully" : "Direct3DCreate9Ex failed");
    return result;
}

extern "C" int WINAPI FNVXR_D3DPERF_BeginEvent(D3DCOLOR color, LPCWSTR name)
{
    if (auto fn = resolve<D3DPERFBeginEventFn>("D3DPERF_BeginEvent"))
        return fn(color, name);
    return -1;
}

extern "C" int WINAPI FNVXR_D3DPERF_EndEvent()
{
    if (auto fn = resolve<D3DPERFEndEventFn>("D3DPERF_EndEvent"))
        return fn();
    return -1;
}

extern "C" DWORD WINAPI FNVXR_D3DPERF_GetStatus()
{
    if (auto fn = resolve<D3DPERFGetStatusFn>("D3DPERF_GetStatus"))
        return fn();
    return 0;
}

extern "C" BOOL WINAPI FNVXR_D3DPERF_QueryRepeatFrame()
{
    if (auto fn = resolve<D3DPERFQueryRepeatFrameFn>("D3DPERF_QueryRepeatFrame"))
        return fn();
    return FALSE;
}

extern "C" void WINAPI FNVXR_D3DPERF_SetMarker(D3DCOLOR color, LPCWSTR name)
{
    if (auto fn = resolve<D3DPERFSetMarkerFn>("D3DPERF_SetMarker"))
        fn(color, name);
}

extern "C" void WINAPI FNVXR_D3DPERF_SetOptions(DWORD options)
{
    if (auto fn = resolve<D3DPERFSetOptionsFn>("D3DPERF_SetOptions"))
        fn(options);
}

extern "C" void WINAPI FNVXR_D3DPERF_SetRegion(D3DCOLOR color, LPCWSTR name)
{
    if (auto fn = resolve<D3DPERFSetRegionFn>("D3DPERF_SetRegion"))
        fn(color, name);
}

extern "C" void WINAPI FNVXR_DebugSetLevel(DWORD level)
{
    if (auto fn = resolve<DebugSetLevelFn>("DebugSetLevel"))
        fn(level);
}

extern "C" void WINAPI FNVXR_DebugSetMute()
{
    if (auto fn = resolve<DebugSetMuteFn>("DebugSetMute"))
        fn();
}

extern "C" void WINAPI FNVXR_Direct3D9EnableMaximizedWindowedModeShim(BOOL enable)
{
    if (auto fn = resolve<Direct3D9EnableMaximizedWindowedModeShimFn>("Direct3D9EnableMaximizedWindowedModeShim"))
        fn(enable);
}

extern "C" HRESULT WINAPI FNVXR_Direct3DCreate9On12(UINT sdkVersion, void* overrideList, UINT overrideCount, IDirect3D9** out)
{
    if (stereoProofModeArmed())
    {
        if (out)
            *out = nullptr;
        logLine("Direct3DCreate9On12 rejected: On12 is not covered by the stereo proof");
        return D3DERR_NOTAVAILABLE_RESULT;
    }
    if (auto fn = resolve<Direct3DCreate9On12Fn>("Direct3DCreate9On12"))
        return fn(sdkVersion, overrideList, overrideCount, out);
    if (out)
        *out = nullptr;
    return D3DERR_NOTAVAILABLE_RESULT;
}

extern "C" HRESULT WINAPI FNVXR_Direct3DCreate9On12Ex(UINT sdkVersion, void* overrideList, UINT overrideCount, IDirect3D9Ex** out)
{
    if (stereoProofModeArmed())
    {
        if (out)
            *out = nullptr;
        logLine("Direct3DCreate9On12Ex rejected: On12Ex is not covered by the stereo proof");
        return D3DERR_NOTAVAILABLE_RESULT;
    }
    if (auto fn = resolve<Direct3DCreate9On12ExFn>("Direct3DCreate9On12Ex"))
        return fn(sdkVersion, overrideList, overrideCount, out);
    if (out)
        *out = nullptr;
    return D3DERR_NOTAVAILABLE_RESULT;
}

extern "C" void* WINAPI FNVXR_Direct3DShaderValidatorCreate9()
{
    if (auto fn = resolve<Direct3DShaderValidatorCreate9Fn>("Direct3DShaderValidatorCreate9"))
        return fn();
    return nullptr;
}

extern "C" void WINAPI FNVXR_PSGPError()
{
    if (auto fn = resolve<VoidNoArgsFn>("PSGPError"))
        fn();
}

extern "C" void WINAPI FNVXR_PSGPSampleTexture()
{
    if (auto fn = resolve<VoidNoArgsFn>("PSGPSampleTexture"))
        fn();
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module);
        QueryPerformanceFrequency(&gPerfFrequency);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        // DllMain runs under the loader lock and may be called on a thread
        // other than the render/producer owner. COM release, logging, mapping
        // publication, and ReleaseMutex are all forbidden here. This proxy is
        // process-lifetime; Windows reclaims handles/address space on process
        // termination, and retained mappings are invalidated transactionally
        // by the next named-lease owner.
    }
    return TRUE;
}
