#pragma once

#include "fnvxr_retail_engine_manifest.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace fnvxr::engine::abi
{
// FalloutNV.exe 1.4.0.525 is a 32-bit process.  Keep every embedded pointer
// four bytes wide so these evidence layouts stay exact even in the x64 audit
// build.  These are inspection layouts, not owning C++ object types.
using RetailPointer32 = std::uint32_t;

struct RetailNiFrustumLayout
{
    float left;
    float right;
    float top;
    float bottom;
    float nearDistance;
    float farDistance;
    std::uint8_t orthographic;
    std::uint8_t reserved19[3];
};

struct RetailNiViewportLayout
{
    float left;
    float right;
    float top;
    float bottom;
};

struct RetailNiVisibleArrayLayout
{
    RetailPointer32 geometryPointers;
    std::uint32_t itemCount;
    std::uint32_t capacity;
    std::uint32_t growBy;
};

struct RetailNiAccumulatorLayout
{
    RetailPointer32 vtable;
    std::uint32_t referenceCount;
    RetailPointer32 camera;
};

struct RetailNiCameraLayout
{
    std::uint8_t opaqueBase[0x9C];
    float worldToCamera[16];
    RetailNiFrustumLayout frustum;
    float minimumNearPlane;
    float maximumFarNearRatio;
    RetailNiViewportLayout viewport;
    float lodAdjust;
};

struct RetailSceneGraphLayout
{
    std::uint8_t opaqueNode[0xAC];
    RetailPointer32 camera;
    RetailPointer32 visibleArray;
    RetailPointer32 cullingProcess;
    std::uint8_t isMenuSceneGraph;
    std::uint8_t reservedB9[3];
    float cameraFov;
};

struct RetailNiCullingProcessLayout
{
    RetailPointer32 vtable;
    std::uint8_t useAppendFunction;
    std::uint8_t reserved05[3];
    RetailPointer32 visibleArray;
    RetailPointer32 camera;
    RetailNiFrustumLayout frustum;
    std::uint8_t opaquePlanes[0x64];
};

struct RetailBSCullingProcessLayout
{
    RetailNiCullingProcessLayout base;
    std::uint32_t topCullMode;
    std::uint32_t cullModeStack[10];
    std::uint32_t cullModeStackSize;
    RetailPointer32 compoundFrustum;
    RetailPointer32 shaderAccumulator;
};

struct RetailBSShaderAccumulatorLayout
{
    RetailPointer32 vtable;
    std::uint32_t referenceCount;
    RetailPointer32 camera;
    std::uint8_t opaque0C[0x180];
    std::uint32_t batchRendererCount;
    std::uint32_t maximumPassCount;
    RetailPointer32 shadowScene;
    std::uint32_t unknown198;
    std::uint32_t renderMode;
    RetailPointer32 depthLight;
    RetailPointer32 tileRenderPass;
    std::uint16_t renderPassType;
    std::uint16_t reserved1AA;
    std::uint8_t opaque1AC[0xD4];
};

static_assert(sizeof(RetailPointer32) == 4u);
static_assert(sizeof(RetailNiFrustumLayout) == 0x1Cu);
static_assert(sizeof(RetailNiViewportLayout) == 0x10u);
static_assert(sizeof(RetailNiVisibleArrayLayout) == 0x10u);
static_assert(sizeof(RetailNiAccumulatorLayout) == 0x0Cu);
static_assert(sizeof(RetailNiCameraLayout) == 0x114u);
static_assert(sizeof(RetailSceneGraphLayout) == 0xC0u);
static_assert(sizeof(RetailNiCullingProcessLayout) == 0x90u);
static_assert(sizeof(RetailBSCullingProcessLayout) == 0xC8u);
static_assert(sizeof(RetailBSShaderAccumulatorLayout) == 0x280u);

static_assert(offsetof(RetailNiVisibleArrayLayout, geometryPointers) == 0x00u);
static_assert(offsetof(RetailNiVisibleArrayLayout, itemCount) == 0x04u);
static_assert(offsetof(RetailNiVisibleArrayLayout, capacity) == 0x08u);
static_assert(offsetof(RetailNiVisibleArrayLayout, growBy) == 0x0Cu);

static_assert(offsetof(RetailNiAccumulatorLayout, vtable) == 0x00u);
static_assert(offsetof(RetailNiAccumulatorLayout, referenceCount) == 0x04u);
static_assert(offsetof(RetailNiAccumulatorLayout, camera) == 0x08u);

static_assert(offsetof(RetailNiCameraLayout, worldToCamera) == 0x9Cu);
static_assert(offsetof(RetailNiCameraLayout, frustum) == 0xDCu);
static_assert(offsetof(RetailNiCameraLayout, minimumNearPlane) == 0xF8u);
static_assert(offsetof(RetailNiCameraLayout, maximumFarNearRatio) == 0xFCu);
static_assert(offsetof(RetailNiCameraLayout, viewport) == 0x100u);
static_assert(offsetof(RetailNiCameraLayout, lodAdjust) == 0x110u);

static_assert(offsetof(RetailSceneGraphLayout, camera) == 0xACu);
static_assert(offsetof(RetailSceneGraphLayout, visibleArray) == 0xB0u);
static_assert(offsetof(RetailSceneGraphLayout, cullingProcess) == 0xB4u);
static_assert(offsetof(RetailSceneGraphLayout, isMenuSceneGraph) == 0xB8u);
static_assert(offsetof(RetailSceneGraphLayout, cameraFov) == 0xBCu);

static_assert(offsetof(RetailNiCullingProcessLayout, useAppendFunction) == 0x04u);
static_assert(offsetof(RetailNiCullingProcessLayout, visibleArray) == 0x08u);
static_assert(offsetof(RetailNiCullingProcessLayout, camera) == 0x0Cu);
static_assert(offsetof(RetailNiCullingProcessLayout, frustum) == 0x10u);

static_assert(offsetof(RetailBSCullingProcessLayout, topCullMode) == 0x90u);
static_assert(offsetof(RetailBSCullingProcessLayout, cullModeStack) == 0x94u);
static_assert(offsetof(RetailBSCullingProcessLayout, cullModeStackSize) == 0xBCu);
static_assert(offsetof(RetailBSCullingProcessLayout, compoundFrustum) == 0xC0u);
static_assert(offsetof(RetailBSCullingProcessLayout, shaderAccumulator) == 0xC4u);

static_assert(offsetof(RetailBSShaderAccumulatorLayout, batchRendererCount) == 0x18Cu);
static_assert(offsetof(RetailBSShaderAccumulatorLayout, maximumPassCount) == 0x190u);
static_assert(offsetof(RetailBSShaderAccumulatorLayout, shadowScene) == 0x194u);
static_assert(offsetof(RetailBSShaderAccumulatorLayout, unknown198) == 0x198u);
static_assert(offsetof(RetailBSShaderAccumulatorLayout, renderMode) == 0x19Cu);
static_assert(offsetof(RetailBSShaderAccumulatorLayout, depthLight) == 0x1A0u);
static_assert(offsetof(RetailBSShaderAccumulatorLayout, tileRenderPass) == 0x1A4u);
static_assert(offsetof(RetailBSShaderAccumulatorLayout, renderPassType) == 0x1A8u);

static_assert(std::is_standard_layout_v<RetailNiCameraLayout>);
static_assert(std::is_standard_layout_v<RetailSceneGraphLayout>);
static_assert(std::is_standard_layout_v<RetailBSCullingProcessLayout>);
static_assert(std::is_standard_layout_v<RetailBSShaderAccumulatorLayout>);
static_assert(std::is_trivially_copyable_v<RetailNiCameraLayout>);
static_assert(std::is_trivially_copyable_v<RetailSceneGraphLayout>);
static_assert(std::is_trivially_copyable_v<RetailBSCullingProcessLayout>);
static_assert(std::is_trivially_copyable_v<RetailBSShaderAccumulatorLayout>);

inline constexpr std::uintptr_t SceneGraphSingletonPointerAddress = 0x011DEB7Cu;
inline constexpr std::uintptr_t BSCullingProcessVtableAddress = 0x0101E2ECu;
inline constexpr std::uintptr_t BSShaderAccumulatorVtableAddress = 0x010ADFF8u;

enum class RetailX86CallingConvention : std::uint8_t
{
    Cdecl,
    Thiscall,
};

// These aliases document the exact x86 call frames.  This header intentionally
// provides no address cast, resolver, constructor helper, hook, or invocation.
#if defined(_MSC_VER)
#define FNVXR_RETAIL_CDECL __cdecl
#define FNVXR_RETAIL_THISCALL __thiscall
#elif defined(__i386__) && (defined(__GNUC__) || defined(__clang__))
#define FNVXR_RETAIL_CDECL __attribute__((cdecl))
#define FNVXR_RETAIL_THISCALL __attribute__((thiscall))
#else
#define FNVXR_RETAIL_CDECL
#define FNVXR_RETAIL_THISCALL
#endif

using NiAllocateFunction = void* (FNVXR_RETAIL_CDECL*)(std::uint32_t byteCount);
using NiFreeFunction = void (FNVXR_RETAIL_CDECL*)(
    void* allocation,
    std::uint32_t byteCount);
using NiCameraCreateFunction = RetailNiCameraLayout* (FNVXR_RETAIL_CDECL*)();
using BSCullingProcessConstructorFunction = RetailBSCullingProcessLayout*
    (FNVXR_RETAIL_THISCALL*)(
        RetailBSCullingProcessLayout* storage,
        std::uint32_t baseConstructorArgument);
using BSCullingProcessDestructorBodyFunction = void
    (FNVXR_RETAIL_THISCALL*)(RetailBSCullingProcessLayout* instance);
using BSCullingProcessScalarDestructorFunction = RetailBSCullingProcessLayout*
    (FNVXR_RETAIL_THISCALL*)(
        RetailBSCullingProcessLayout* instance,
        std::uint32_t deletingFlags);
using BSShaderAccumulatorConstructorFunction = RetailBSShaderAccumulatorLayout*
    (FNVXR_RETAIL_THISCALL*)(
        RetailBSShaderAccumulatorLayout* storage,
        std::uint32_t constructorMode,
        std::uint32_t batchRendererCount,
        std::uint32_t maximumPassCount);
using BSShaderAccumulatorScalarDestructorFunction = RetailBSShaderAccumulatorLayout*
    (FNVXR_RETAIL_THISCALL*)(
        RetailBSShaderAccumulatorLayout* instance,
        std::uint32_t deletingFlags);
using NiRefObjectFreeFunction = void
    (FNVXR_RETAIL_THISCALL*)(RetailNiAccumulatorLayout* instance);
using AccumulatorSetCameraFunction = void
    (FNVXR_RETAIL_THISCALL*)(
        RetailNiAccumulatorLayout* accumulator,
        RetailNiCameraLayout* camera);
using AccumulatorAddVisibleArrayFunction = void
    (FNVXR_RETAIL_THISCALL*)(
        RetailNiAccumulatorLayout* accumulator,
        RetailNiVisibleArrayLayout* visibleArray);
using GeometryOnVisibleFunction = void
    (FNVXR_RETAIL_THISCALL*)(
        void* geometry,
        RetailNiCullingProcessLayout* cullingProcess);
using CullingProcessAltFunction = void
    (FNVXR_RETAIL_THISCALL*)(
        RetailBSCullingProcessLayout* cullingProcess,
        RetailNiCameraLayout* camera,
        void* sceneObject,
        RetailNiVisibleArrayLayout* visibleArray);
using CullingProcessFunction = void
    (FNVXR_RETAIL_THISCALL*)(
        RetailBSCullingProcessLayout* cullingProcess,
        void* sceneObject);
using CullingAppendFunction = void
    (FNVXR_RETAIL_THISCALL*)(
        RetailBSCullingProcessLayout* cullingProcess,
        void* geometry);
using ShaderAccumulatorRenderFunction = void
    (FNVXR_RETAIL_THISCALL*)(RetailBSShaderAccumulatorLayout* accumulator);
using ShaderAccumulatorFinalizeFunction = void
    (FNVXR_RETAIL_THISCALL*)(RetailBSShaderAccumulatorLayout* accumulator);
using AccumulateSceneFunction = void (FNVXR_RETAIL_CDECL*)(
    RetailNiCameraLayout* camera,
    void* sceneObject,
    RetailBSCullingProcessLayout* cullingProcess);
using AccumulateSecondWorldBranchFunction = void (FNVXR_RETAIL_CDECL*)(
    RetailNiCameraLayout* camera,
    void* retailSceneList,
    RetailBSCullingProcessLayout* cullingProcess);
using AccumulatorRenderFunction = void (FNVXR_RETAIL_CDECL*)(
    RetailNiCameraLayout* camera,
    RetailBSShaderAccumulatorLayout* accumulator,
    std::uint32_t branchSelectorOrContext);

#undef FNVXR_RETAIL_CDECL
#undef FNVXR_RETAIL_THISCALL

struct RetailFunctionAbiDescriptor
{
    const char* name = nullptr;
    std::uintptr_t preferredAddress = 0;
    std::size_t byteCount = 0;
    Sha256Digest sha256 {};
    RetailX86CallingConvention callingConvention = RetailX86CallingConvention::Cdecl;
    std::uint8_t stackArgumentCount = 0;
    std::uint8_t calleePopBytes = 0;
    bool exactInstructionBoundary = false;
    bool stackContractProven = false;
    bool argumentSemanticsProven = false;
    std::uint8_t independentLoadedSamples = 0;
};

// Every callable range below matched in two independent loaded retail process
// samples.  The second sample also captured the stock constructor call sites
// and both world-branch/wrapper call frames, establishing the argument
// semantics recorded here.  Runtime use still requires a synchronous match in
// the exact target process; this static inventory is never sufficient alone.
inline constexpr std::array<RetailFunctionAbiDescriptor, 22>
    RetailFunctionAbiInventory {{
        {
            "Ni_Alloc",
            0x00AA13E0u,
            62u,
            sha256FromHex("A6D466261F7EFE14EF6D039472EB18E391BFCAFF7B0B517D226F77DB279D522F"),
            RetailX86CallingConvention::Cdecl,
            1u,
            0u,
            true,
            true,
            true,
            2u,
        },
        {
            "Ni_Free",
            0x00AA1460u,
            34u,
            sha256FromHex("913981C3922AEA3327F9D2FAD205D91F3BF6E10DCF155CFABA4EA9E9B65758DA"),
            RetailX86CallingConvention::Cdecl,
            2u,
            0u,
            true,
            true,
            true,
            2u,
        },
        {
            "NiCamera::Create",
            0x00A71430u,
            27u,
            sha256FromHex("7E44D91953FF04F3EA524EB9CA217725098AF144155F3B58DA1EC7D8CD1C2427"),
            RetailX86CallingConvention::Cdecl,
            0u,
            0u,
            true,
            true,
            true,
            2u,
        },
        {
            "BSCullingProcess::BSCullingProcess",
            0x004A0EB0u,
            148u,
            sha256FromHex("14D034A770A7A6CCD4352F89AB8729FD82CC7E723C3AA6E7EE3CA021B07DDB01"),
            RetailX86CallingConvention::Thiscall,
            1u,
            4u,
            true,
            true,
            true,
            2u,
        },
        {
            "BSCullingProcess::~BSCullingProcess body",
            0x004A0F60u,
            98u,
            sha256FromHex("74BDB507CC332B6431CAE48430555F179871E1992AEADEA658B21D23A8B57B44"),
            RetailX86CallingConvention::Thiscall,
            0u,
            0u,
            true,
            true,
            true,
            2u,
        },
        {
            "BSCullingProcess scalar deleting destructor",
            0x004A0FF0u,
            44u,
            sha256FromHex("6A3EA7214D4547EAB025BF945CADB5D8988304987105D228FD7CC55DD0E40774"),
            RetailX86CallingConvention::Thiscall,
            1u,
            4u,
            true,
            true,
            true,
            2u,
        },
        {
            "BSShaderAccumulator::BSShaderAccumulator",
            0x00B660D0u,
            1055u,
            sha256FromHex("93575B329AF7E5F508FB6B2C30A0BC37950C45F5694601800D2C2BDBF513C0A7"),
            RetailX86CallingConvention::Thiscall,
            3u,
            12u,
            true,
            true,
            true,
            2u,
        },
        {
            "BSShaderAccumulator scalar deleting destructor",
            0x00B664F0u,
            35u,
            sha256FromHex("C7A7B304AD972950F4C530AFDF5DB0102F5D09E0E1519872023BE87E744E3196"),
            RetailX86CallingConvention::Thiscall,
            1u,
            4u,
            true,
            true,
            true,
            2u,
        },
        {
            "NiRefObject::Free thunk",
            0x00CFCC20u,
            13u,
            sha256FromHex("E05407482FB2CFA446EF461E18C81B95321488F14CAEE466BE9A43F4AA6B9B7B"),
            RetailX86CallingConvention::Thiscall,
            0u,
            0u,
            true,
            true,
            true,
            2u,
        },
        {
            "NiAccumulator::SetCamera",
            0x00D47A40u,
            10u,
            sha256FromHex("B35CAAC991EB0D06657C3FBBA49C85DE5F83E8CF4DE0B07B70FA4B90CB5770FD"),
            RetailX86CallingConvention::Thiscall,
            1u,
            4u,
            true,
            true,
            true,
            2u,
        },
        {
            "NiAccumulator::AddVisibleArray",
            0x00A9B790u,
            189u,
            sha256FromHex("A929F2C8289B45EC15F5A16E88A5052D5E1C3F1348880E07C66E223DCB592843"),
            RetailX86CallingConvention::Thiscall,
            1u,
            4u,
            true,
            true,
            true,
            2u,
        },
        {
            "NiGeometry::OnVisible",
            0x00A7FD90u,
            17u,
            sha256FromHex("C3A270DCBE479A05ED865C7AEB395E19CBADCEEB575486F12A5D6F94B7F8452C"),
            RetailX86CallingConvention::Thiscall,
            1u,
            4u,
            true,
            true,
            true,
            2u,
        },
        {
            "BSCullingProcess::ProcessAlt",
            0x00C4F070u,
            343u,
            sha256FromHex("213054747E94294B95DABD125B34D430DD0D12137396C97EAE269FCA7A0E301F"),
            RetailX86CallingConvention::Thiscall,
            3u,
            12u,
            true,
            true,
            true,
            2u,
        },
        {
            "BSCullingProcess::Process",
            0x00C4EE90u,
            479u,
            sha256FromHex("7680B9EFC6E7DA6FB1E954A784096803515889F58E2720C38075E50AE3AF2AEF"),
            RetailX86CallingConvention::Thiscall,
            1u,
            4u,
            true,
            true,
            true,
            2u,
        },
        {
            "BSCullingProcess::Append",
            0x00C4F1D0u,
            150u,
            sha256FromHex("7E198CD639B8CA514E2427DD3524300DF7D9CAF2D5181B915D917210FACFC9BE"),
            RetailX86CallingConvention::Thiscall,
            1u,
            4u,
            true,
            true,
            true,
            2u,
        },
        {
            "BSShaderAccumulator::Render",
            0x00B66520u,
            68u,
            sha256FromHex("0B5DBE36A8C5BB61BDD0DB3D9844AC925664B16BA17ACA41252F4DCBB1F3350F"),
            RetailX86CallingConvention::Thiscall,
            0u,
            0u,
            true,
            true,
            true,
            2u,
        },
        {
            "BSShaderAccumulator::Finalize",
            0x00B66570u,
            41u,
            sha256FromHex("7F43A986F29D39CEDFC8C9648C6FD1EB620141475EFEEA923B61A8ACD0829C06"),
            RetailX86CallingConvention::Thiscall,
            0u,
            0u,
            true,
            true,
            true,
            2u,
        },
        {
            "AccumulateScene",
            0x00B6BEE0u,
            209u,
            sha256FromHex("45BF64E849FF26829D43BB52866BF51C412DF95F12F3177159A2BEC2D5838A5A"),
            RetailX86CallingConvention::Cdecl,
            3u,
            0u,
            true,
            true,
            true,
            2u,
        },
        {
            "AccumulateSecondWorldBranch",
            0x00B6BFC0u,
            264u,
            sha256FromHex("3B3E2F913D36BD7DF7B5299A7C026A44B44D82643C6714B60EAD45F60F172B5B"),
            RetailX86CallingConvention::Cdecl,
            3u,
            0u,
            true,
            true,
            true,
            2u,
        },
        {
            "RenderAccumulatorWithoutFinalize",
            0x00B6BA20u,
            74u,
            sha256FromHex("D493A62CA76EBEF84C25CDFA7B1BA4C10966D1EEED2D0FA0EA4E2D583880C9E4"),
            RetailX86CallingConvention::Cdecl,
            3u,
            0u,
            true,
            true,
            true,
            2u,
        },
        {
            "FinalizeAccumulator",
            0x00B6B930u,
            14u,
            sha256FromHex("C3CA665ECDDCAC09D42561F945B34450340B1354CC2054BF45599426581BB792"),
            RetailX86CallingConvention::Cdecl,
            3u,
            0u,
            true,
            true,
            true,
            2u,
        },
        {
            "RenderAndFinalizeAccumulator",
            0x00B6C0D0u,
            84u,
            sha256FromHex("1BB66CF7419B4FBF086CA3D2F36BE79F17EEA3D04516C7E4A0A3E4EEAA99119B"),
            RetailX86CallingConvention::Cdecl,
            3u,
            0u,
            true,
            true,
            true,
            2u,
        },
    }};

struct RetailVtableSlotDescriptor
{
    const char* owner = nullptr;
    std::uintptr_t vtableAddress = 0;
    std::size_t slotByteOffset = 0;
    std::uintptr_t preferredTargetAddress = 0;
};

inline constexpr std::array<RetailVtableSlotDescriptor, 16> RetailVtableSlots {{
    { "BSCullingProcess", BSCullingProcessVtableAddress, 0x40u, 0x004A0FF0u },
    { "BSCullingProcess", BSCullingProcessVtableAddress, 0x44u, 0x00C4EE90u },
    { "BSCullingProcess", BSCullingProcessVtableAddress, 0x48u, 0x00C4F070u },
    { "BSCullingProcess", BSCullingProcessVtableAddress, 0x4Cu, 0x00C4F1D0u },
    { "BSShaderAccumulator", BSShaderAccumulatorVtableAddress, 0x00u, 0x00B664F0u },
    { "BSShaderAccumulator", BSShaderAccumulatorVtableAddress, 0x04u, 0x00CFCC20u },
    { "BSShaderAccumulator", BSShaderAccumulatorVtableAddress, 0x8Cu, 0x00D47A40u },
    { "BSShaderAccumulator", BSShaderAccumulatorVtableAddress, 0x90u, 0x00B63070u },
    { "BSShaderAccumulator", BSShaderAccumulatorVtableAddress, 0x94u, 0x00A9B790u },
    { "BSShaderAccumulator", BSShaderAccumulatorVtableAddress, 0x98u, 0x00B63F10u },
    { "BSShaderAccumulator", BSShaderAccumulatorVtableAddress, 0x9Cu, 0x00B63B10u },
    { "BSShaderAccumulator", BSShaderAccumulatorVtableAddress, 0xA0u, 0x00B63B60u },
    { "BSShaderAccumulator", BSShaderAccumulatorVtableAddress, 0xA4u, 0x00A9B570u },
    { "BSShaderAccumulator", BSShaderAccumulatorVtableAddress, 0xA8u, 0x00B66520u },
    { "BSShaderAccumulator", BSShaderAccumulatorVtableAddress, 0xACu, 0x00B66570u },
    { "BSShaderAccumulator", BSShaderAccumulatorVtableAddress, 0xB0u, 0x00D070A0u },
}};

struct RetailVtableBlockDescriptor
{
    const char* owner = nullptr;
    std::uintptr_t preferredAddress = 0;
    std::size_t byteCount = 0;
    Sha256Digest sha256 {};
    std::uint8_t independentLoadedSamples = 0;
};

// Pin the complete inherited block that a future private culler would have to
// clone.  Named slot checks remain useful diagnostics, but they cannot prove
// that any unnamed inherited slot stayed stock.
inline constexpr std::array<RetailVtableBlockDescriptor, 1> RetailVtableBlocks {{
    {
        "BSCullingProcess",
        BSCullingProcessVtableAddress,
        0x50u,
        sha256FromHex("7F1CEFA617A2A3A0F07D944C0233436C9FEED2A2B3FA5AF379730D079D3EF838"),
        2u,
    },
}};

inline constexpr std::uint8_t MinimumIndependentLoadedSamples = 2u;

constexpr bool imageContains(std::uintptr_t address, std::size_t byteCount)
{
    if (address < SupportedImageBase || byteCount == 0u
        || byteCount > SupportedSizeOfImage)
    {
        return false;
    }
    const std::uintptr_t imageOffset = address - SupportedImageBase;
    return imageOffset <= SupportedSizeOfImage - byteCount;
}

constexpr bool structurallyValid(const RetailFunctionAbiDescriptor& descriptor)
{
    if (!descriptor.name || descriptor.name[0] == '\0'
        || !imageContains(descriptor.preferredAddress, descriptor.byteCount)
        || !descriptor.sha256.valid || !descriptor.exactInstructionBoundary
        || !descriptor.stackContractProven)
    {
        return false;
    }

    if (descriptor.callingConvention == RetailX86CallingConvention::Cdecl)
        return descriptor.calleePopBytes == 0u;

    return descriptor.calleePopBytes
        == static_cast<std::uint8_t>(descriptor.stackArgumentCount * 4u);
}

constexpr bool productionProven(const RetailFunctionAbiDescriptor& descriptor)
{
    return structurallyValid(descriptor)
        && descriptor.argumentSemanticsProven
        && descriptor.independentLoadedSamples >= MinimumIndependentLoadedSamples;
}

constexpr bool structurallyValid(const RetailVtableBlockDescriptor& descriptor)
{
    return descriptor.owner
        && descriptor.owner[0] != '\0'
        && descriptor.byteCount % sizeof(RetailPointer32) == 0u
        && imageContains(descriptor.preferredAddress, descriptor.byteCount)
        && descriptor.sha256.valid;
}

constexpr bool productionProven(const RetailVtableBlockDescriptor& descriptor)
{
    return structurallyValid(descriptor)
        && descriptor.independentLoadedSamples >= MinimumIndependentLoadedSamples;
}

constexpr bool retailFunctionInventoryStructurallyValid()
{
    for (const RetailFunctionAbiDescriptor& descriptor : RetailFunctionAbiInventory)
    {
        if (!structurallyValid(descriptor))
            return false;
    }
    return true;
}

constexpr bool retailFunctionInventoryProductionProven()
{
    for (const RetailFunctionAbiDescriptor& descriptor : RetailFunctionAbiInventory)
    {
        if (!productionProven(descriptor))
            return false;
    }
    return true;
}

constexpr bool retailVtableBlockInventoryStructurallyValid()
{
    for (const RetailVtableBlockDescriptor& descriptor : RetailVtableBlocks)
    {
        if (!structurallyValid(descriptor))
            return false;
    }
    return true;
}

constexpr bool retailVtableBlockInventoryProductionProven()
{
    for (const RetailVtableBlockDescriptor& descriptor : RetailVtableBlocks)
    {
        if (!productionProven(descriptor))
            return false;
    }
    return true;
}

static_assert(retailFunctionInventoryStructurallyValid());
static_assert(retailFunctionInventoryProductionProven());
static_assert(retailVtableBlockInventoryStructurallyValid());
static_assert(retailVtableBlockInventoryProductionProven());

enum class RetailEngineAbiFailure : std::uint32_t
{
    None = 0,
    UnsupportedExecutable = 1,
    LoadedExecutableSectionLayoutUnverified = 2,
    CoreManifestUnverified = 3,
    StaticFunctionInventoryIncomplete = 4,
    StaticVtableBlockInventoryIncomplete = 5,
    RuntimeFunctionInventoryUnverified = 6,
    RuntimeVtableSlotsUnverified = 7,
    RuntimeVtableBlocksUnverified = 8,
    LiveObjectLayoutsUnverified = 9,
    ConstructorOwnershipUnverified = 10,
    BothWorldBranchesUnverified = 11,
    CompatibilityModulesUnverified = 12,
    SynchronousRuntimeRevalidationMissing = 13,
};

// Evidence fields may be set only by direct probes of the same loaded process
// immediately before a future transaction.  They are not configuration knobs.
struct RetailEngineAbiEvidence
{
    bool loadedExecutableIdentityMatched = false;
    // Exact executable-section geometry and page protections from the loaded
    // PE. Content authority is deliberately narrower: every engine entry,
    // core body, dispatch slot, and complete cloned vtable block is hashed
    // below. A modded retail process legitimately rewrites unrelated .text
    // bytes, so those bytes must not be misrepresented as an immutable seal.
    bool loadedExecutableSectionLayoutAndProtectionsVerified = false;
    bool coreManifestMatched = false;
    bool fullFunctionInventoryMatched = false;
    bool vtableSlotsMatched = false;
    bool vtableBlocksMatched = false;
    bool liveObjectLayoutsVerified = false;
    bool constructorOwnershipVerified = false;
    bool bothWorldBranchesVerified = false;
    bool compatibilityModulesVerified = false;
    bool synchronousRuntimeRevalidation = false;
};

struct RetailEngineAbiAssessment
{
    bool engineCallsAuthorized = false;
    RetailEngineAbiFailure failure = RetailEngineAbiFailure::UnsupportedExecutable;
};

constexpr RetailEngineAbiAssessment assessRetailEngineAbiWithStaticInventory(
    const RetailEngineAbiEvidence& evidence,
    bool staticFunctionInventoryProven,
    bool staticVtableBlockInventoryProven)
{
    if (!evidence.loadedExecutableIdentityMatched)
        return { false, RetailEngineAbiFailure::UnsupportedExecutable };
    if (!evidence.loadedExecutableSectionLayoutAndProtectionsVerified)
        return { false, RetailEngineAbiFailure::LoadedExecutableSectionLayoutUnverified };
    if (!evidence.coreManifestMatched)
        return { false, RetailEngineAbiFailure::CoreManifestUnverified };
    if (!staticFunctionInventoryProven)
        return { false, RetailEngineAbiFailure::StaticFunctionInventoryIncomplete };
    if (!staticVtableBlockInventoryProven)
        return { false, RetailEngineAbiFailure::StaticVtableBlockInventoryIncomplete };
    if (!evidence.fullFunctionInventoryMatched)
        return { false, RetailEngineAbiFailure::RuntimeFunctionInventoryUnverified };
    if (!evidence.vtableSlotsMatched)
        return { false, RetailEngineAbiFailure::RuntimeVtableSlotsUnverified };
    if (!evidence.vtableBlocksMatched)
        return { false, RetailEngineAbiFailure::RuntimeVtableBlocksUnverified };
    if (!evidence.liveObjectLayoutsVerified)
        return { false, RetailEngineAbiFailure::LiveObjectLayoutsUnverified };
    if (!evidence.constructorOwnershipVerified)
        return { false, RetailEngineAbiFailure::ConstructorOwnershipUnverified };
    if (!evidence.bothWorldBranchesVerified)
        return { false, RetailEngineAbiFailure::BothWorldBranchesUnverified };
    if (!evidence.compatibilityModulesVerified)
        return { false, RetailEngineAbiFailure::CompatibilityModulesUnverified };
    if (!evidence.synchronousRuntimeRevalidation)
        return { false, RetailEngineAbiFailure::SynchronousRuntimeRevalidationMissing };
    return { true, RetailEngineAbiFailure::None };
}

constexpr RetailEngineAbiAssessment assessRetailEngineAbi(
    const RetailEngineAbiEvidence& evidence)
{
    return assessRetailEngineAbiWithStaticInventory(
        evidence,
        retailFunctionInventoryProductionProven(),
        retailVtableBlockInventoryProductionProven());
}
}
