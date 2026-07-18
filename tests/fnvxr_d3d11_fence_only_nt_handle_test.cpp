#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>

#include <cstdlib>
#include <iostream>

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

bool verifyDriver(
    D3D_DRIVER_TYPE driverType,
    UINT miscFlags,
    const char* label) noexcept
{
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_9_1;
    const HRESULT deviceResult = D3D11CreateDevice(
        nullptr,
        driverType,
        nullptr,
        0u,
        nullptr,
        0u,
        D3D11_SDK_VERSION,
        &device,
        &featureLevel,
        &context);
    if (FAILED(deviceResult) || !device || !context)
    {
        releaseInterface(context);
        releaseInterface(device);
        return false;
    }

    D3D11_TEXTURE2D_DESC description {};
    description.Width = 16u;
    description.Height = 16u;
    description.MipLevels = 1u;
    description.ArraySize = 1u;
    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.SampleDesc.Count = 1u;
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags =
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    description.MiscFlags = miscFlags;

    ID3D11Texture2D* texture = nullptr;
    IDXGIResource1* resource = nullptr;
    IDXGIKeyedMutex* keyedMutex = nullptr;
    HANDLE sharedHandle = nullptr;
    const HRESULT textureResult = device->CreateTexture2D(
        &description,
        nullptr,
        &texture);
    const bool textureCreated = SUCCEEDED(textureResult)
        && texture;
    const bool resourceAvailable = textureCreated
        && SUCCEEDED(texture->QueryInterface(
            __uuidof(IDXGIResource1),
            reinterpret_cast<void**>(&resource)))
        && resource;
    const bool keyedMutexAbsent = textureCreated
        && FAILED(texture->QueryInterface(
            __uuidof(IDXGIKeyedMutex),
            reinterpret_cast<void**>(&keyedMutex)))
        && !keyedMutex;
    const HRESULT handleResult = resourceAvailable
        ? resource->CreateSharedHandle(
            nullptr,
            DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
            nullptr,
            &sharedHandle)
        : E_NOINTERFACE;
    const bool handleCreated = resourceAvailable
        && SUCCEEDED(handleResult)
        && sharedHandle;

    ID3D11Device* openingDevice = nullptr;
    ID3D11DeviceContext* openingContext = nullptr;
    D3D_FEATURE_LEVEL openingFeatureLevel = D3D_FEATURE_LEVEL_9_1;
    const HRESULT openingDeviceResult = D3D11CreateDevice(
        nullptr,
        driverType,
        nullptr,
        0u,
        nullptr,
        0u,
        D3D11_SDK_VERSION,
        &openingDevice,
        &openingFeatureLevel,
        &openingContext);
    ID3D11Device1* openingDevice1 = nullptr;
    ID3D11Texture2D* reopenedTexture = nullptr;
    const bool reopened = handleCreated
        && SUCCEEDED(openingDeviceResult)
        && openingDevice
        && openingContext
        && openingFeatureLevel >= D3D_FEATURE_LEVEL_10_0
        && SUCCEEDED(openingDevice->QueryInterface(
            __uuidof(ID3D11Device1),
            reinterpret_cast<void**>(&openingDevice1)))
        && openingDevice1
        && SUCCEEDED(openingDevice1->OpenSharedResource1(
            sharedHandle,
            __uuidof(ID3D11Texture2D),
            reinterpret_cast<void**>(&reopenedTexture)))
        && reopenedTexture;

    if (sharedHandle)
        CloseHandle(sharedHandle);
    releaseInterface(reopenedTexture);
    releaseInterface(openingDevice1);
    releaseInterface(openingContext);
    releaseInterface(openingDevice);
    releaseInterface(keyedMutex);
    releaseInterface(resource);
    releaseInterface(texture);
    releaseInterface(context);
    releaseInterface(device);
    const bool passed = featureLevel >= D3D_FEATURE_LEVEL_10_0
        && textureCreated
        && resourceAvailable
        && keyedMutexAbsent
        && handleCreated
        && reopened;
    std::cout << label
              << " driver=" << static_cast<unsigned>(driverType)
              << " flags=0x" << std::hex << miscFlags
              << " texture=0x" << static_cast<unsigned long>(textureResult)
              << " handle=0x" << static_cast<unsigned long>(handleResult)
              << std::dec
              << " keyedAbsent=" << (keyedMutexAbsent ? 1 : 0)
              << " reopened=" << (reopened ? 1 : 0)
              << '\n';
    return passed;
}
}

int main()
{
    constexpr UINT ntAndShared = D3D11_RESOURCE_MISC_SHARED_NTHANDLE
        | D3D11_RESOURCE_MISC_SHARED;
    if (verifyDriver(
            D3D_DRIVER_TYPE_HARDWARE,
            ntAndShared,
            "nt-shared")
        || verifyDriver(D3D_DRIVER_TYPE_WARP, ntAndShared, "nt-shared"))
    {
        std::cout << "fence-only NT-handle texture route passed\n";
        return EXIT_SUCCESS;
    }
    std::cerr << "fence-only NT-handle texture route unavailable\n";
    return EXIT_FAILURE;
}
