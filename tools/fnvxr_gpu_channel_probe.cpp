// Explicit hardware integration probe. CPU reads are confined to this tool.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9on12.h>
#include <d3d11_4.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include "../renderhook/fnvxr_gpu_eye_transport_win32.h"
#include "../renderhook/fnvxr_gpu_color_publisher_win32.h"
#include "../host/fnvxr_gpu_color_consumer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
namespace color = fnvxr::gpu::color_v5;
namespace producer = fnvxr::d3d9::color_transport;
namespace consumer = fnvxr::host::gpu_color;
namespace
{
constexpr UINT Width = 1872, Height = 2016, Iterations = 120;
void require(bool value, const char* step)
{
    if (!value) throw std::runtime_error(step);
}
void check(HRESULT result, const char* step)
{
    if (FAILED(result))
    {
        std::fprintf(stderr, "%s HRESULT=0x%08lx\n", step, result);
        throw std::runtime_error(step);
    }
}
struct Handle
{
    HANDLE value = nullptr;
    ~Handle() { if (value) CloseHandle(value); }
};
struct DeviceWindow
{
    HWND value = CreateWindowExW(0, L"STATIC", L"FNVXR private GPU probe",
        WS_POPUP, 0, 0, 16, 16, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ~DeviceWindow() { if (value) DestroyWindow(value); }
};
struct Events
{
    Handle published, consumed;
    explicit Events(const std::string& id)
    {
        published.value = CreateEventA(nullptr, FALSE, FALSE,
            ("Local\\FNVXR_Probe_Published_" + id).c_str());
        consumed.value = CreateEventA(nullptr, FALSE, FALSE,
            ("Local\\FNVXR_Probe_Consumed_" + id).c_str());
        require(published.value && consumed.value, "create probe events");
    }
    void wait(HANDLE value) { require(WaitForSingleObject(value, 10000) == WAIT_OBJECT_0, "probe peer timeout"); }
};
std::array<unsigned char, 4> pixel(UINT iteration, UINT channel, UINT eye)
{
    return { static_cast<unsigned char>(17 + iteration),
        static_cast<unsigned char>(channel ? 193 : 37),
        static_cast<unsigned char>(eye ? 211 : 53), 255 };
}
void fill(IDirect3DDevice9* device, IDirect3DTexture9* texture,
    UINT iteration, UINT channel, UINT eye)
{
    ComPtr<IDirect3DSurface9> surface;
    check(texture->GetSurfaceLevel(0, &surface), "source surface");
    const auto rgba = pixel(iteration, channel, eye);
    check(device->ColorFill(surface.Get(), nullptr,
        D3DCOLOR_XRGB(rgba[0], rgba[1], rgba[2])), "source pattern");
}

int produce(const std::string& id)
{
    Events events(id);
    const auto module = LoadLibraryExW(L"d3d9.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    require(module != nullptr, "system D3D9");
    const auto create = reinterpret_cast<PFN_Direct3DCreate9On12>(
        GetProcAddress(module, "Direct3DCreate9On12"));
    require(create != nullptr, "9on12 factory export");
    D3D9ON12_ARGS args {};
    args.Enable9On12 = TRUE;
    ComPtr<IDirect3D9> factory;
    factory.Attach(create(D3D_SDK_VERSION, &args, 1));
    require(factory != nullptr, "9on12 factory");
    DeviceWindow window;
    require(window.value != nullptr, "private device window");
    D3DPRESENT_PARAMETERS present {};
    present.BackBufferWidth = present.BackBufferHeight = 16;
    present.BackBufferFormat = D3DFMT_X8R8G8B8;
    present.BackBufferCount = 1;
    present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present.hDeviceWindow = window.value;
    present.Windowed = TRUE;
    present.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    ComPtr<IDirect3DDevice9> device;
    check(factory->CreateDevice(0, D3DDEVTYPE_HAL, window.value,
        D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
        &present, &device), "ordinary D3D9 device");
    std::array<ComPtr<IDirect3DTexture9>, 2> sources;
    for (auto& source : sources)
            check(device->CreateTexture(Width, Height, 1, D3DUSAGE_RENDERTARGET,
                D3DFMT_A8B8G8R8, D3DPOOL_DEFAULT, &source, nullptr), "source texture");
    std::array<producer::GpuEyeTransport, 2> channels;
    const auto initializeChannels = [&](std::uint64_t generation)
    {
        for (std::size_t channel = 0; channel != channels.size(); ++channel)
            require(channels[channel].initialize({ device.Get(),
                sources[0].Get(), sources[1].Get(), generation }), "channel initialization");
    };
    initializeChannels(1);
    producer::Win32Publisher publisher;
    require(publisher.initialize(("Local\\FNVXR_Probe_Frames_" + id).c_str(),
        ("Local\\FNVXR_Probe_Producer_" + id).c_str()), "mapping publisher");
    std::uint64_t transaction = 0;
    std::vector<double> timings;
    const auto publish = [&](UINT iteration, UINT channel)
    {
        for (UINT eye = 0; eye != 2; ++eye)
            fill(device.Get(), sources[eye].Get(), iteration, channel, eye);
        ++transaction;
        producer::ProducerFrameIdentity identity {
            channel ? color::PresentationMode::MonoUiQuad : color::PresentationMode::BinocularWorld,
            iteration < 90 ? 1u : 2u, GetCurrentProcessId(), transaction,
            transaction, transaction, transaction, static_cast<std::int64_t>(transaction),
            channel ? color::RetailMenuCaptured : color::RetailWorldTransactionComplete };
        const auto start = std::chrono::steady_clock::now();
        const auto publication = channels[channel].produce(identity);
        timings.push_back(std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count());
        require(publication.complete && publisher.publish(publication), "GPU publication");
        return identity;
    };
    for (UINT iteration = 0; iteration != Iterations; ++iteration)
    {
        if (iteration == 60) initializeChannels(2);
        const auto world = publish(iteration, 0);
        publish(iteration, 1);
        // No consumer has been notified yet. Reusing an owned destination
        // must skip, including after a resource-set or producer-epoch change.
        require(channels[0].produce(world).failure
            == producer::ProducerFailure::ConsumerReleasePending, "world ownership overwrite");
        SetEvent(events.published.value);
        events.wait(events.consumed.value);
        if (iteration == 0)
        {
            // The consumer deliberately released UI only. Updating that
            // channel must work while world remains owned by the consumer.
            require(channels[0].produce(world).failure
                == producer::ProducerFailure::ConsumerReleasePending, "UI released world ownership");
            publish(iteration, 1);
            SetEvent(events.published.value);
            events.wait(events.consumed.value);
        }
    }
    std::sort(timings.begin(), timings.end());
    std::printf("{\"passed\":true,\"role\":\"producer\",\"pointerBits\":%zu,"
        "\"frames\":%u,\"width\":%u,\"height\":%u,\"submitMedianMs\":%.3f,\"submitP95Ms\":%.3f}\n",
        sizeof(void*) * 8, Iterations, Width, Height, timings[timings.size()/2],
        timings[timings.size()*95/100]);
    return 0;
}

int consume(const std::string& id)
{
    Events events(id);
    events.wait(events.published.value);
    const std::string mappingName = "Local\\FNVXR_Probe_Frames_" + id;
    Handle mapping;
    mapping.value = OpenFileMappingA(FILE_MAP_READ, FALSE, mappingName.c_str());
    require(mapping.value != nullptr, "open mapping");
    const auto* descriptors = static_cast<const color::SharedStereoColorDescriptor*>(
        MapViewOfFile(mapping.value, FILE_MAP_READ, 0, 0,
            sizeof(color::SharedStereoColorDescriptor) * color::FrameChannelCount));
    require(descriptors != nullptr, "map channel descriptors");
    color::SharedStereoColorDescriptor initial {};
    require(color::readStableSnapshot(descriptors, initial), "read adapter identity");
    UnmapViewOfFile(descriptors);
    const LUID luid { static_cast<DWORD>(initial.adapterLuid),
        static_cast<LONG>(initial.adapterLuid >> 32) };
    ComPtr<IDXGIFactory4> factory;
    ComPtr<IDXGIAdapter> adapter;
    check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "DXGI factory");
    check(factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&adapter)), "producer adapter");
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    check(D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
        &device, nullptr, &context), "D3D11 consumer device");
    const std::wstring wideName(mappingName.begin(), mappingName.end());
    std::array<consumer::Win32Consumer, 2> channels;
    for (UINT channel = 0; channel != 2; ++channel)
        require(channels[channel].initialize(device.Get(), wideName.c_str(),
            static_cast<color::FrameChannel>(channel)), "consumer initialization");
    std::array<ComPtr<ID3D11Texture2D>, 2> readback;
    for (auto& texture : readback)
    {
        D3D11_TEXTURE2D_DESC desc {};
        desc.Width = Width;
        desc.Height = Height;
        desc.MipLevels = desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        check(device->CreateTexture2D(&desc, nullptr, &texture), "probe-only staging texture");
    }
    const auto verify = [&](UINT iteration, UINT channel)
    {
        const auto frame = channels[channel].consume();
        if (frame.disposition != consumer::ConsumeDisposition::FrameReady)
            std::fprintf(stderr, "consumer failure=%u backend=%u iteration=%u channel=%u\n",
                unsigned(frame.failure), unsigned(channels[channel].backendFailure()), iteration, channel);
        require(frame.disposition == consumer::ConsumeDisposition::FrameReady, "consume published channel");
        require(frame.frame.presentationMode == (channel ? color::PresentationMode::MonoUiQuad
            : color::PresentationMode::BinocularWorld), "crossed world/UI channels");
        require(frame.frame.resourceSetId == (iteration < 60 ? 1u : 2u)
            && frame.frame.producerEpoch == (iteration < 90 ? 1u : 2u), "stale resource or producer epoch");
        require(frame.frame.renderFlags == (channel ? color::RetailMenuCaptured
            : color::RetailWorldTransactionComplete), "capture provenance lost");
        for (UINT eye = 0; eye != 2; ++eye)
        {
            context->CopyResource(readback[eye].Get(), channels[channel].eyeTexture(eye));
            D3D11_MAPPED_SUBRESOURCE mapped {};
            check(context->Map(readback[eye].Get(), 0, D3D11_MAP_READ, 0, &mapped), "probe-only pixel verification");
            const auto expected = pixel(iteration, channel, eye);
            bool matches = true;
            for (UINT y = 0; y < Height; y += 101)
                for (UINT x = 0; x < Width; x += 97)
                    matches = matches && std::memcmp(static_cast<const unsigned char*>(mapped.pData)
                        + std::size_t(y)*mapped.RowPitch + x*4, expected.data(), 4) == 0;
            context->Unmap(readback[eye].Get(), 0);
            require(matches, "wrong, torn or stale eye pixels");
        }
        require(channels[channel].consume().disposition == consumer::ConsumeDisposition::NoNewFrame,
            "retained frame identity changed");
    };
    for (UINT iteration = 0; iteration != Iterations; ++iteration)
    {
        if (iteration != 0) events.wait(events.published.value);
        if (iteration == 0)
        {
            verify(iteration, 1);
            SetEvent(events.consumed.value);
            events.wait(events.published.value);
        }
        verify(iteration, 0);
        verify(iteration, 1);
        SetEvent(events.consumed.value);
    }
    std::printf("{\"passed\":true,\"role\":\"consumer\",\"pointerBits\":%zu,\"frames\":%u,"
        "\"independentChannels\":true,\"resourceReset\":true,\"producerEpochReset\":true,\"pixelsVerified\":true}\n",
        sizeof(void*) * 8, Iterations);
    return 0;
}
}
int main(int argc, char** argv)
{
    try
    {
        if (argc != 3) throw std::runtime_error("Usage: fnvxr_gpu_channel_probe --produce|--consume unique-id");
        if (std::strcmp(argv[1], "--produce") == 0) return produce(argv[2]);
        if (std::strcmp(argv[1], "--consume") == 0) return consume(argv[2]);
        throw std::runtime_error("invalid probe role");
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "GPU channel probe failed: %s\n", error.what());
        return 1;
    }
}
