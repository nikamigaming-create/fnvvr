#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <d3d9.h>
#include <d3d11_4.h>
#include <dxgi1_2.h>

#include "fnvxr_d3d9ex_color_transport_win32.h"

#include <cstdint>
#include <cstring>
#include <new>

namespace fnvxr::d3d9::color_transport
{
namespace
{
template <typename Interface>
void releaseInterface(Interface*& value) noexcept
{
    if (value)
    {
        value->Release();
        value = nullptr;
    }
}

template <typename Function>
Function resolveFunction(HMODULE module, const char* name) noexcept
{
    Function function = nullptr;
    const FARPROC address = module ? GetProcAddress(module, name) : nullptr;
    static_assert(sizeof(function) == sizeof(address));
    std::memcpy(&function, &address, sizeof(function));
    return function;
}

using CreateDxgiFactory1Function = HRESULT(WINAPI*)(REFIID, void**);
using D3D11CreateDeviceFunction = HRESULT(WINAPI*)(
    IDXGIAdapter*,
    D3D_DRIVER_TYPE,
    HMODULE,
    UINT,
    const D3D_FEATURE_LEVEL*,
    UINT,
    UINT,
    ID3D11Device**,
    D3D_FEATURE_LEVEL*,
    ID3D11DeviceContext**);

std::uint64_t packLuid(const LUID& luid) noexcept
{
    return (static_cast<std::uint64_t>(
                static_cast<std::uint32_t>(luid.HighPart))
            << 32u)
        | static_cast<std::uint64_t>(luid.LowPart);
}

bool sameLuid(const LUID& left, const LUID& right) noexcept
{
    return left.HighPart == right.HighPart && left.LowPart == right.LowPart;
}

bool sameComIdentity(IUnknown* left, IUnknown* right) noexcept
{
    if (!left || !right)
        return false;
    IUnknown* leftIdentity = nullptr;
    IUnknown* rightIdentity = nullptr;
    const HRESULT leftResult = left->QueryInterface(
        IID_IUnknown,
        reinterpret_cast<void**>(&leftIdentity));
    const HRESULT rightResult = right->QueryInterface(
        IID_IUnknown,
        reinterpret_cast<void**>(&rightIdentity));
    const bool same = SUCCEEDED(leftResult)
        && SUCCEEDED(rightResult)
        && leftIdentity
        && rightIdentity
        && leftIdentity == rightIdentity;
    releaseInterface(leftIdentity);
    releaseInterface(rightIdentity);
    return same;
}

DXGI_FORMAT dxgiFormatForD3D9(D3DFORMAT format) noexcept
{
    switch (format)
    {
    case D3DFMT_A8B8G8R8:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case D3DFMT_A2B10G10R10:
        return DXGI_FORMAT_R10G10B10A2_UNORM;
    case D3DFMT_A16B16G16R16F:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

gpu::GpuInteropFormat protocolFormat(DXGI_FORMAT format) noexcept
{
    switch (format)
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        return gpu::GpuInteropFormat::R8G8B8A8Unorm;
    case DXGI_FORMAT_R10G10B10A2_UNORM:
        return gpu::GpuInteropFormat::R10G10B10A2Unorm;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        return gpu::GpuInteropFormat::R16G16B16A16Float;
    default:
        return gpu::GpuInteropFormat::Unknown;
    }
}

bool d3d9TextureDescriptionValid(
    IDirect3DTexture9* texture,
    D3DSURFACE_DESC& description) noexcept
{
    description = {};
    return texture
        && texture->GetLevelCount() == 1u
        && SUCCEEDED(texture->GetLevelDesc(0u, &description))
        && description.Width != 0u
        && description.Height != 0u
        && description.Pool == D3DPOOL_DEFAULT
        && (description.Usage & D3DUSAGE_RENDERTARGET) != 0u
        && dxgiFormatForD3D9(description.Format) != DXGI_FORMAT_UNKNOWN;
}

bool d3d11SourceDescriptionValid(
    const D3D11_TEXTURE2D_DESC& description,
    const D3DSURFACE_DESC& expectedD3D9,
    DXGI_FORMAT expectedFormat) noexcept
{
    constexpr UINT requiredBindings =
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    return description.Width == expectedD3D9.Width
        && description.Height == expectedD3D9.Height
        && description.Width <= gpu::color_v5::MaximumTextureDimension
        && description.Height <= gpu::color_v5::MaximumTextureDimension
        && description.MipLevels == 1u
        && description.ArraySize == 1u
        && description.Format == expectedFormat
        && description.SampleDesc.Count == 1u
        && description.SampleDesc.Quality == 0u
        && description.Usage == D3D11_USAGE_DEFAULT
        && (description.BindFlags & requiredBindings) == requiredBindings
        && description.CPUAccessFlags == 0u;
}
}

struct Win32Producer::Implementation final
{
    HMODULE dxgiModule = nullptr;
    HMODULE d3d11Module = nullptr;
    IDirect3DDevice9Ex* d3d9DeviceEx = nullptr;
    IDirect3DTexture9* leftD3D9Texture = nullptr;
    IDirect3DTexture9* rightD3D9Texture = nullptr;
    IDirect3DQuery9* d3d9CompletionQuery = nullptr;
    IDXGIFactory1* dxgiFactory = nullptr;
    IDXGIAdapter1* dxgiAdapter = nullptr;
    ID3D11Device* d3d11Device = nullptr;
    ID3D11Device5* d3d11Device5 = nullptr;
    ID3D11DeviceContext* d3d11Context = nullptr;
    ID3D11DeviceContext4* d3d11Context4 = nullptr;
    ID3D11Texture2D* leftD3D11Source = nullptr;
    ID3D11Texture2D* rightD3D11Source = nullptr;
    ID3D11Texture2D* leftD3D11Destination = nullptr;
    ID3D11Texture2D* rightD3D11Destination = nullptr;
    ID3D11Fence* sharedFence = nullptr;
    HANDLE leftNtHandle = nullptr;
    HANDLE rightNtHandle = nullptr;
    HANDLE fenceNtHandle = nullptr;
    ProducerResources producerResources {};
    Producer producer {};
    Win32ProducerFailure failure = Win32ProducerFailure::None;

    ~Implementation() noexcept
    {
        releaseAll();
    }

    void releaseAll() noexcept
    {
        producer.reset();
        if (leftNtHandle)
        {
            CloseHandle(leftNtHandle);
            leftNtHandle = nullptr;
        }
        if (rightNtHandle)
        {
            CloseHandle(rightNtHandle);
            rightNtHandle = nullptr;
        }
        if (fenceNtHandle)
        {
            CloseHandle(fenceNtHandle);
            fenceNtHandle = nullptr;
        }
        releaseInterface(sharedFence);
        releaseInterface(leftD3D11Destination);
        releaseInterface(rightD3D11Destination);
        releaseInterface(leftD3D11Source);
        releaseInterface(rightD3D11Source);
        releaseInterface(d3d11Context4);
        releaseInterface(d3d11Context);
        releaseInterface(d3d11Device5);
        releaseInterface(d3d11Device);
        releaseInterface(dxgiAdapter);
        releaseInterface(dxgiFactory);
        releaseInterface(d3d9CompletionQuery);
        releaseInterface(leftD3D9Texture);
        releaseInterface(rightD3D9Texture);
        releaseInterface(d3d9DeviceEx);
        if (d3d11Module)
        {
            FreeLibrary(d3d11Module);
            d3d11Module = nullptr;
        }
        if (dxgiModule)
        {
            FreeLibrary(dxgiModule);
            dxgiModule = nullptr;
        }
        producerResources = {};
    }

    bool reject(Win32ProducerFailure value) noexcept
    {
        failure = value;
        releaseAll();
        return false;
    }

    bool acquireD3D9ExAndAdapterIdentity(
        const Win32ProducerSource& source,
        LUID& sourceAdapterLuid) noexcept
    {
        sourceAdapterLuid = {};
        if (FAILED(source.device->QueryInterface(
                __uuidof(IDirect3DDevice9Ex),
                reinterpret_cast<void**>(&d3d9DeviceEx)))
            || !d3d9DeviceEx)
        {
            return reject(Win32ProducerFailure::SourceNotD3D9Ex);
        }

        D3DDEVICE_CREATION_PARAMETERS creation {};
        IDirect3D9* direct3D = nullptr;
        IDirect3D9Ex* direct3DEx = nullptr;
        const bool identityAvailable = SUCCEEDED(
                source.device->GetCreationParameters(&creation))
            && creation.DeviceType == D3DDEVTYPE_HAL
            && SUCCEEDED(source.device->GetDirect3D(&direct3D))
            && direct3D
            && SUCCEEDED(direct3D->QueryInterface(
                __uuidof(IDirect3D9Ex),
                reinterpret_cast<void**>(&direct3DEx)))
            && direct3DEx
            && SUCCEEDED(direct3DEx->GetAdapterLUID(
                creation.AdapterOrdinal,
                &sourceAdapterLuid))
            && packLuid(sourceAdapterLuid) != 0u;
        releaseInterface(direct3DEx);
        releaseInterface(direct3D);
        if (!identityAvailable)
            return reject(Win32ProducerFailure::AdapterIdentityUnavailable);
        return true;
    }

    bool loadRuntimes() noexcept
    {
        dxgiModule = LoadLibraryW(L"dxgi.dll");
        d3d11Module = LoadLibraryW(L"d3d11.dll");
        if (!dxgiModule || !d3d11Module)
            return reject(Win32ProducerFailure::GraphicsRuntimeUnavailable);
        return true;
    }

    bool createSameAdapterD3D11Device(const LUID& sourceAdapterLuid) noexcept
    {
        const auto createFactory = resolveFunction<CreateDxgiFactory1Function>(
            dxgiModule,
            "CreateDXGIFactory1");
        const auto createDevice = resolveFunction<D3D11CreateDeviceFunction>(
            d3d11Module,
            "D3D11CreateDevice");
        if (!createFactory || !createDevice)
            return reject(Win32ProducerFailure::GraphicsRuntimeUnavailable);
        if (FAILED(createFactory(
                __uuidof(IDXGIFactory1),
                reinterpret_cast<void**>(&dxgiFactory)))
            || !dxgiFactory)
        {
            return reject(Win32ProducerFailure::DxgiFactoryCreation);
        }

        for (UINT index = 0u; ; ++index)
        {
            IDXGIAdapter1* candidate = nullptr;
            const HRESULT result = dxgiFactory->EnumAdapters1(index, &candidate);
            if (result == DXGI_ERROR_NOT_FOUND)
                break;
            if (FAILED(result) || !candidate)
            {
                releaseInterface(candidate);
                return reject(
                    Win32ProducerFailure::MatchingDxgiAdapterUnavailable);
            }
            DXGI_ADAPTER_DESC1 description {};
            if (SUCCEEDED(candidate->GetDesc1(&description))
                && sameLuid(description.AdapterLuid, sourceAdapterLuid)
                && (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0u)
            {
                dxgiAdapter = candidate;
                break;
            }
            releaseInterface(candidate);
        }
        if (!dxgiAdapter)
        {
            return reject(
                Win32ProducerFailure::MatchingDxgiAdapterUnavailable);
        }

        D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_9_1;
        if (FAILED(createDevice(
                dxgiAdapter,
                D3D_DRIVER_TYPE_UNKNOWN,
                nullptr,
                0u,
                nullptr,
                0u,
                D3D11_SDK_VERSION,
                &d3d11Device,
                &featureLevel,
                &d3d11Context))
            || !d3d11Device
            || !d3d11Context)
        {
            return reject(Win32ProducerFailure::D3D11DeviceCreation);
        }
        if (featureLevel < D3D_FEATURE_LEVEL_10_0)
        {
            return reject(
                Win32ProducerFailure::D3D11FeatureLevelUnsupported);
        }
        if (FAILED(d3d11Device->QueryInterface(
                __uuidof(ID3D11Device5),
                reinterpret_cast<void**>(&d3d11Device5)))
            || !d3d11Device5
            || FAILED(d3d11Context->QueryInterface(
                __uuidof(ID3D11DeviceContext4),
                reinterpret_cast<void**>(&d3d11Context4)))
            || !d3d11Context4)
        {
            return reject(
                Win32ProducerFailure::D3D11FenceInterfacesUnavailable);
        }

        IDXGIDevice* dxgiDevice = nullptr;
        IDXGIAdapter* actualAdapter = nullptr;
        DXGI_ADAPTER_DESC actualDescription {};
        const bool exactAdapter = SUCCEEDED(d3d11Device->QueryInterface(
                __uuidof(IDXGIDevice),
                reinterpret_cast<void**>(&dxgiDevice)))
            && dxgiDevice
            && SUCCEEDED(dxgiDevice->GetAdapter(&actualAdapter))
            && actualAdapter
            && SUCCEEDED(actualAdapter->GetDesc(&actualDescription))
            && sameLuid(actualDescription.AdapterLuid, sourceAdapterLuid);
        releaseInterface(actualAdapter);
        releaseInterface(dxgiDevice);
        if (!exactAdapter)
        {
            return reject(
                Win32ProducerFailure::MatchingDxgiAdapterUnavailable);
        }
        releaseInterface(dxgiAdapter);
        releaseInterface(dxgiFactory);
        return true;
    }

    bool openD3D9Source(
        std::uintptr_t sharedHandle,
        ID3D11Texture2D*& texture) noexcept
    {
        ID3D11Resource* resource = nullptr;
        const HRESULT result = d3d11Device->OpenSharedResource(
            reinterpret_cast<HANDLE>(sharedHandle),
            __uuidof(ID3D11Resource),
            reinterpret_cast<void**>(&resource));
        const bool opened = SUCCEEDED(result)
            && resource
            && SUCCEEDED(resource->QueryInterface(
                __uuidof(ID3D11Texture2D),
                reinterpret_cast<void**>(&texture)))
            && texture;
        releaseInterface(resource);
        return opened;
    }

    bool validateFormatSupport(DXGI_FORMAT format) noexcept
    {
        UINT support = 0u;
        constexpr UINT required = D3D11_FORMAT_SUPPORT_TEXTURE2D
            | D3D11_FORMAT_SUPPORT_RENDER_TARGET
            | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE;
        if (FAILED(d3d11Device->CheckFormatSupport(format, &support))
            || (support & required) != required)
        {
            return false;
        }
        D3D11_FEATURE_DATA_FORMAT_SUPPORT2 support2 {};
        support2.InFormat = format;
        return SUCCEEDED(d3d11Device->CheckFeatureSupport(
                D3D11_FEATURE_FORMAT_SUPPORT2,
                &support2,
                sizeof(support2)))
            && (support2.OutFormatSupport2
                & D3D11_FORMAT_SUPPORT2_SHAREABLE) != 0u;
    }

    bool createNtDestination(
        const D3D11_TEXTURE2D_DESC& sourceDescription,
        ID3D11Texture2D*& destination,
        HANDLE& sharedHandle) noexcept
    {
        D3D11_TEXTURE2D_DESC destinationDescription = sourceDescription;
        destinationDescription.MipLevels = 1u;
        destinationDescription.ArraySize = 1u;
        destinationDescription.SampleDesc.Count = 1u;
        destinationDescription.SampleDesc.Quality = 0u;
        destinationDescription.Usage = D3D11_USAGE_DEFAULT;
        destinationDescription.BindFlags =
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        destinationDescription.CPUAccessFlags = 0u;
        // This transport's sole ownership primitive is the shared D3D11
        // fence. Advertising SHARED_KEYEDMUTEX would require every opener to
        // AcquireSync/ReleaseSync in addition to obeying that fence. Request
        // the ordinary shared-resource type plus NT handle semantics, and
        // fail closed below if this runtime/driver rejects that fence-only
        // combination.
        destinationDescription.MiscFlags =
            D3D11_RESOURCE_MISC_SHARED_NTHANDLE
            | D3D11_RESOURCE_MISC_SHARED;
        if (FAILED(d3d11Device->CreateTexture2D(
                &destinationDescription,
                nullptr,
                &destination))
            || !destination)
        {
            return false;
        }

        IDXGIResource1* shareable = nullptr;
        IDXGIKeyedMutex* keyedMutex = nullptr;
        const bool resourceInterfaceAvailable = SUCCEEDED(
                destination->QueryInterface(
                    __uuidof(IDXGIResource1),
                    reinterpret_cast<void**>(&shareable)))
            && shareable;
        const bool incorrectlyKeyed = SUCCEEDED(destination->QueryInterface(
                __uuidof(IDXGIKeyedMutex),
                reinterpret_cast<void**>(&keyedMutex)))
            && keyedMutex;
        const HRESULT handleResult = resourceInterfaceAvailable
                && !incorrectlyKeyed
            ? shareable->CreateSharedHandle(
                nullptr,
                DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
                nullptr,
                &sharedHandle)
            : E_NOINTERFACE;
        releaseInterface(keyedMutex);
        releaseInterface(shareable);
        return SUCCEEDED(handleResult) && sharedHandle;
    }

    bool createFence() noexcept
    {
        if (FAILED(d3d11Device5->CreateFence(
                0u,
                D3D11_FENCE_FLAG_SHARED,
                __uuidof(ID3D11Fence),
                reinterpret_cast<void**>(&sharedFence)))
            || !sharedFence)
        {
            return false;
        }
        return SUCCEEDED(sharedFence->CreateSharedHandle(
                nullptr,
                GENERIC_ALL,
                nullptr,
                &fenceNtHandle))
            && fenceNtHandle;
    }

    static Implementation* checked(void* opaque) noexcept
    {
        return static_cast<Implementation*>(opaque);
    }

    static bool consumerReleaseReached(
        void* opaque,
        std::uint64_t sequence) noexcept
    {
        Implementation* state = checked(opaque);
        return state
            && state->sharedFence
            && state->d3d11Device
            && state->d3d11Device->GetDeviceRemovedReason() == S_OK
            && state->sharedFence->GetCompletedValue() >= sequence;
    }

    static bool synchronizeD3D9ColorWrites(void* opaque) noexcept
    {
        Implementation* state = checked(opaque);
        if (!state || !state->d3d9CompletionQuery)
            return false;
        if (FAILED(state->d3d9CompletionQuery->Issue(D3DISSUE_END)))
            return false;

        const ULONGLONG started = GetTickCount64();
        for (;;)
        {
            const HRESULT result = state->d3d9CompletionQuery->GetData(
                nullptr,
                0u,
                D3DGETDATA_FLUSH);
            if (result == S_OK)
                return true;
            if (result != S_FALSE || GetTickCount64() - started >= 1000u)
                return false;
            (void)SwitchToThread();
        }
    }

    bool copyColor(
        ID3D11Texture2D* destination,
        ID3D11Texture2D* source) noexcept
    {
        if (!d3d11Context || !d3d11Device || !destination || !source)
            return false;
        d3d11Context->CopyResource(destination, source);
        return d3d11Device->GetDeviceRemovedReason() == S_OK;
    }

    static bool copyLeftColor(void* opaque) noexcept
    {
        Implementation* state = checked(opaque);
        return state && state->copyColor(
            state->leftD3D11Destination,
            state->leftD3D11Source);
    }

    static bool copyRightColor(void* opaque) noexcept
    {
        Implementation* state = checked(opaque);
        return state && state->copyColor(
            state->rightD3D11Destination,
            state->rightD3D11Source);
    }

    static bool signalFence(void* opaque, std::uint64_t sequence) noexcept
    {
        Implementation* state = checked(opaque);
        if (!state
            || !state->d3d11Context
            || !state->d3d11Context4
            || !state->sharedFence
            || !state->d3d11Device)
        {
            return false;
        }
        if (FAILED(state->d3d11Context4->Signal(
                state->sharedFence,
                sequence)))
        {
            return false;
        }
        // Microsoft requires a device that updates a shared texture to flush
        // its command stream. The fence signal remains ordered after both
        // CopyResource commands on this immediate context.
        state->d3d11Context->Flush();
        return state->d3d11Device->GetDeviceRemovedReason() == S_OK;
    }

    bool initialize(const Win32ProducerSource& source) noexcept
    {
        if (!source.device
            || !source.leftTexture
            || !source.rightTexture
            || source.leftD3D9SharedHandle == 0u
            || source.rightD3D9SharedHandle == 0u
            || source.leftD3D9SharedHandle == source.rightD3D9SharedHandle
            || source.resourceSetId == 0u
            || sameComIdentity(source.leftTexture, source.rightTexture))
        {
            return reject(Win32ProducerFailure::InvalidInput);
        }

        D3DSURFACE_DESC leftD3D9Description {};
        D3DSURFACE_DESC rightD3D9Description {};
        if (!d3d9TextureDescriptionValid(
                source.leftTexture,
                leftD3D9Description)
            || !d3d9TextureDescriptionValid(
                source.rightTexture,
                rightD3D9Description)
            || leftD3D9Description.Width != rightD3D9Description.Width
            || leftD3D9Description.Height != rightD3D9Description.Height
            || leftD3D9Description.Format != rightD3D9Description.Format)
        {
            return reject(Win32ProducerFailure::SourceTextureInvalid);
        }
        const DXGI_FORMAT expectedFormat = dxgiFormatForD3D9(
            leftD3D9Description.Format);
        if (expectedFormat == DXGI_FORMAT_UNKNOWN)
            return reject(Win32ProducerFailure::SourceTextureInvalid);

        LUID sourceAdapterLuid {};
        if (!acquireD3D9ExAndAdapterIdentity(source, sourceAdapterLuid))
            return false;
        source.leftTexture->AddRef();
        leftD3D9Texture = source.leftTexture;
        source.rightTexture->AddRef();
        rightD3D9Texture = source.rightTexture;

        if (!loadRuntimes()
            || !createSameAdapterD3D11Device(sourceAdapterLuid))
        {
            return false;
        }
        if (!openD3D9Source(
                source.leftD3D9SharedHandle,
                leftD3D11Source)
            || !openD3D9Source(
                source.rightD3D9SharedHandle,
                rightD3D11Source)
            || sameComIdentity(leftD3D11Source, rightD3D11Source))
        {
            return reject(Win32ProducerFailure::D3D9SourceOpen);
        }

        D3D11_TEXTURE2D_DESC leftD3D11Description {};
        D3D11_TEXTURE2D_DESC rightD3D11Description {};
        leftD3D11Source->GetDesc(&leftD3D11Description);
        rightD3D11Source->GetDesc(&rightD3D11Description);
        if (!d3d11SourceDescriptionValid(
                leftD3D11Description,
                leftD3D9Description,
                expectedFormat)
            || !d3d11SourceDescriptionValid(
                rightD3D11Description,
                rightD3D9Description,
                expectedFormat)
            || std::memcmp(
                &leftD3D11Description,
                &rightD3D11Description,
                sizeof(leftD3D11Description)) != 0)
        {
            return reject(
                Win32ProducerFailure::D3D11SourceDescriptionInvalid);
        }
        if (!validateFormatSupport(expectedFormat))
            return reject(Win32ProducerFailure::D3D11FormatUnsupported);

        if (!createNtDestination(
                leftD3D11Description,
                leftD3D11Destination,
                leftNtHandle)
            || !createNtDestination(
                rightD3D11Description,
                rightD3D11Destination,
                rightNtHandle))
        {
            return reject(Win32ProducerFailure::D3D11NtTextureCreation);
        }
        if (!leftNtHandle
            || !rightNtHandle
            || leftNtHandle == rightNtHandle)
        {
            return reject(Win32ProducerFailure::D3D11NtHandleCreation);
        }
        if (!createFence())
        {
            return reject(Win32ProducerFailure::D3D11SharedFenceCreation);
        }
        if (FAILED(d3d9DeviceEx->CreateQuery(
                D3DQUERYTYPE_EVENT,
                &d3d9CompletionQuery))
            || !d3d9CompletionQuery)
        {
            return reject(Win32ProducerFailure::D3D9EventQueryCreation);
        }

        producerResources = {
            static_cast<std::uint64_t>(
                reinterpret_cast<std::uintptr_t>(leftNtHandle)),
            static_cast<std::uint64_t>(
                reinterpret_cast<std::uintptr_t>(rightNtHandle)),
            static_cast<std::uint64_t>(
                reinterpret_cast<std::uintptr_t>(fenceNtHandle)),
            source.resourceSetId,
            packLuid(sourceAdapterLuid),
            leftD3D11Description.Width,
            leftD3D11Description.Height,
            protocolFormat(expectedFormat),
        };
        const ProducerCapabilities capabilities {
            true,
            true,
            true,
            true,
            true,
            true,
            true,
        };
        const ProducerOperations operations {
            this,
            &consumerReleaseReached,
            &synchronizeD3D9ColorWrites,
            &copyLeftColor,
            &copyRightColor,
            &signalFence,
        };
        if (!producer.initialize(operations, capabilities, producerResources))
        {
            return reject(
                Win32ProducerFailure::ProducerContractInitialization);
        }
        failure = Win32ProducerFailure::None;
        return true;
    }
};

Win32Producer::Win32Producer() noexcept = default;

Win32Producer::~Win32Producer() noexcept
{
    reset();
}

bool Win32Producer::initialize(const Win32ProducerSource& source) noexcept
{
    reset();
    mImplementation = new (std::nothrow) Implementation;
    if (!mImplementation)
    {
        mFailure = Win32ProducerFailure::GraphicsRuntimeUnavailable;
        return false;
    }
    if (!mImplementation->initialize(source))
    {
        mFailure = mImplementation->failure;
        delete mImplementation;
        mImplementation = nullptr;
        return false;
    }
    mFailure = Win32ProducerFailure::None;
    return true;
}

void Win32Producer::reset() noexcept
{
    delete mImplementation;
    mImplementation = nullptr;
    mFailure = Win32ProducerFailure::None;
}

bool Win32Producer::ready() const noexcept
{
    return mImplementation
        && mImplementation->producer.ready()
        && resourcesComplete(mImplementation->producerResources);
}

Win32ProducerFailure Win32Producer::failure() const noexcept
{
    return mFailure;
}

ProducerResources Win32Producer::resources() const noexcept
{
    return ready() ? mImplementation->producerResources : ProducerResources {};
}

ProducerPublication Win32Producer::produce(
    const ProducerFrameIdentity& identity) noexcept
{
    if (!ready())
        return { false, ProducerFailure::NotInitialized, {} };
    return mImplementation->producer.produce(identity);
}
}
