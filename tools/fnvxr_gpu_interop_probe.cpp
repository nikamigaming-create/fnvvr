#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "fnvxr_gpu_eye_transport_win32.h"
#include <d3d9on12.h>
#include <d3d11_4.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

namespace
{
void check(HRESULT result, const char* step)
{
    if (FAILED(result))
    {
        std::fprintf(stderr, "{\"step\":\"%s\",\"hresult\":\"0x%08lx\"}\n",
            step, static_cast<unsigned long>(result));
        throw std::runtime_error(step);
    }
}

struct Handle
{
    HANDLE value = nullptr;
    ~Handle() { if (value) CloseHandle(value); }
};

// A private, never-shown device window. No foreground, cursor, or input APIs.
struct DeviceWindow
{
    HWND value = CreateWindowExW(0, L"STATIC", L"FNVXR GPU interop probe",
        WS_POPUP, 0, 0, 16, 16, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ~DeviceWindow() { if (value) DestroyWindow(value); }
};
}

int main(int argc, char** argv)
{
    if (argc != 2 || std::strcmp(argv[1], "--run") != 0)
    {
        std::puts("Usage: fnvxr_gpu_interop_probe --run (private GPU devices; no game or OpenXR)");
        return 0;
    }
    try
    {
        const HMODULE runtime = LoadLibraryExW(L"d3d9.dll", nullptr,
            LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!runtime) throw std::runtime_error("system d3d9 unavailable");
        const auto create = reinterpret_cast<PFN_Direct3DCreate9On12>(
            GetProcAddress(runtime, "Direct3DCreate9On12"));
        if (!create) throw std::runtime_error("Direct3DCreate9On12 unavailable");
        D3D9ON12_ARGS overrideArgs {};
        overrideArgs.Enable9On12 = TRUE;
        ComPtr<IDirect3D9> d3d9;
        d3d9.Attach(create(D3D_SDK_VERSION, &overrideArgs, 1));
        if (!d3d9) throw std::runtime_error("9on12 factory failed");
        DeviceWindow window;
        if (!window.value) throw std::runtime_error("private device window failed");
        D3DPRESENT_PARAMETERS present {};
        present.BackBufferWidth = 16;
        present.BackBufferHeight = 16;
        present.BackBufferFormat = D3DFMT_X8R8G8B8;
        present.BackBufferCount = 1;
        present.SwapEffect = D3DSWAPEFFECT_DISCARD;
        present.hDeviceWindow = window.value;
        present.Windowed = TRUE;
        present.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        ComPtr<IDirect3DDevice9> device9;
        check(d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window.value,
            D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
            &present, &device9), "create-ordinary-d3d9-on12-device");
        ComPtr<IDirect3DDevice9On12> interop;
        check(device9.As(&interop), "query-9on12");
        ComPtr<ID3D12Device> device12;
        check(interop->GetD3D12Device(IID_PPV_ARGS(&device12)), "get-d3d12-device");
        const LUID luid = device12->GetAdapterLuid();
        ComPtr<IDXGIFactory4> factory;
        ComPtr<IDXGIAdapter> adapter;
        check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "create-dxgi-factory");
        check(factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&adapter)), "same-adapter");
        ComPtr<ID3D11Device> device11;
        ComPtr<ID3D11DeviceContext> context11;
        D3D_FEATURE_LEVEL featureLevel {};
        check(D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
            &device11, &featureLevel, &context11), "create-d3d11");
        ComPtr<ID3D11Device5> device115;
        ComPtr<ID3D11DeviceContext4> context114;
        check(device11.As(&device115), "d3d11-fence-device");
        check(context11.As(&context114), "d3d11-fence-context");

        std::array<ComPtr<IDirect3DTexture9>, 2> source9;
        constexpr UINT width = 1872;
        constexpr UINT height = 2016;
        for (UINT eye = 0; eye < 2; ++eye)
        {
            check(device9->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET,
                D3DFMT_A8B8G8R8, D3DPOOL_DEFAULT, &source9[eye], nullptr), "eye-texture");
            ComPtr<IDirect3DSurface9> surface;
            check(source9[eye]->GetSurfaceLevel(0, &surface), "eye-surface");
            check(device9->ColorFill(surface.Get(), nullptr,
                eye == 0 ? D3DCOLOR_XRGB(211, 31, 47) : D3DCOLOR_XRGB(19, 89, 227)), "eye-color");
        }
        using namespace fnvxr::d3d9::color_transport;
        GpuEyeTransport transport;
        if (!transport.initialize({ device9.Get(), source9[0].Get(), source9[1].Get(), 1 }))
        {
            std::fprintf(stderr, "transport initialization failed reason=%u hr=0x%08x\n",
                unsigned(transport.failure()), unsigned(transport.lastHresult()));
            throw std::runtime_error("transport initialization");
        }
        const ProducerResources resources = transport.resources();
        ComPtr<ID3D11Fence> fence11;
        check(device115->OpenSharedFence(reinterpret_cast<HANDLE>(
            static_cast<std::uintptr_t>(resources.sharedFenceNtHandle)),
            IID_PPV_ARGS(&fence11)), "import-fence");
        std::array<ComPtr<ID3D11Texture2D>, 2> shared11;
        std::array<ComPtr<ID3D11Texture2D>, 2> readback11;
        const std::uint64_t handles[] = { resources.leftColorNtHandle, resources.rightColorNtHandle };
        for (UINT eye = 0; eye < 2; ++eye)
        {
            check(device115->OpenSharedResource1(reinterpret_cast<HANDLE>(
                static_cast<std::uintptr_t>(handles[eye])), IID_PPV_ARGS(&shared11[eye])), "import-eye");
            D3D11_TEXTURE2D_DESC desc11 {};
            shared11[eye]->GetDesc(&desc11);
            std::printf("{\"eye\":%u,\"format\":%u,\"bindFlags\":%u,\"miscFlags\":%u}\n",
                eye, static_cast<unsigned>(desc11.Format), desc11.BindFlags, desc11.MiscFlags);
            desc11.Usage = D3D11_USAGE_STAGING;
            desc11.BindFlags = 0;
            desc11.MiscFlags = 0;
            desc11.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            check(device11->CreateTexture2D(&desc11, nullptr, &readback11[eye]), "probe-only-readback");
        }
        ProducerFrameIdentity identity { fnvxr::gpu::color_v5::PresentationMode::BinocularWorld,
            1, GetCurrentProcessId(), 1, 1, 1, 1, 1 };
        auto publication = transport.produce(identity);
        if (!publication.complete)
        {
            std::fprintf(stderr, "transport publication failed reason=%u hr=0x%08x\n",
                unsigned(transport.failure()), unsigned(transport.lastHresult()));
            throw std::runtime_error("transport publication");
        }
        // Before consumer release, another publication must skip without
        // overwriting the shared pair or waiting for a CPU event.
        ++identity.transactionId;
        ++identity.sourceFrame;
        if (transport.produce(identity).failure != ProducerFailure::ConsumerReleasePending)
            throw std::runtime_error("producer overwrote consumer-owned eyes");
        check(context114->Wait(fence11.Get(), publication.payload.gpuReadySequence), "gpu-consumer-wait");
        for (UINT eye = 0; eye < 2; ++eye)
            context11->CopyResource(readback11[eye].Get(), shared11[eye].Get());
        check(context114->Signal(fence11.Get(), publication.payload.gpuConsumerReleaseSequence), "gpu-consumer-release");
        context11->Flush();
        for (UINT eye = 0; eye < 2; ++eye)
        {
            D3D11_MAPPED_SUBRESOURCE mapped {};
            check(context11->Map(readback11[eye].Get(), 0, D3D11_MAP_READ, 0, &mapped), "verify-pixels");
            const std::array<std::uint8_t, 4> expected = eye == 0
                ? std::array<std::uint8_t, 4> { 211, 31, 47, 255 }
                : std::array<std::uint8_t, 4> { 19, 89, 227, 255 };
            bool matches = true;
            for (UINT y = 0; y < height; y += 101)
                for (UINT x = 0; x < width; x += 97)
                    matches = matches && std::memcmp(static_cast<const std::uint8_t*>(mapped.pData)
                        + static_cast<std::size_t>(y) * mapped.RowPitch + x * 4,
                        expected.data(), expected.size()) == 0;
            context11->Unmap(readback11[eye].Get(), 0);
            if (!matches) throw std::runtime_error("eye pixel mismatch");
        }
        Handle completion;
        completion.value = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!completion.value) throw std::runtime_error("completion event unavailable");
        check(fence11->SetEventOnCompletion(2, completion.value), "release-completion-event");
        if (WaitForSingleObject(completion.value, 5000) != WAIT_OBJECT_0
            || fence11->GetCompletedValue() < 2)
            throw std::runtime_error("release not observed");
        std::printf("{\"passed\":true,\"pointerBits\":%zu,\"width\":%u,\"height\":%u,"
            "\"transport\":\"D3D9On12-to-D3D12-shared-to-D3D11\",\"gpuReleaseObserved\":true}\n",
            sizeof(void*) * 8, width, height);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "GPU interop probe failed: %s\n", error.what());
        return 1;
    }
}
