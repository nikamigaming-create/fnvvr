#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "fnvxr_gpu_eye_transport_win32.h"
#include <windows.h>
#include <d3d9on12.h>
#include <wrl/client.h>

#include <array>
#include <new>

namespace fnvxr::d3d9::color_transport
{
using Microsoft::WRL::ComPtr;

struct GpuEyeTransport::State
{
    ComPtr<IDirect3DDevice9On12> interop;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commands;
    ComPtr<ID3D12Fence> fence;
    std::array<ComPtr<IDirect3DTexture9>, 2> sources;
    std::array<ComPtr<ID3D12Resource>, 2> unwrapped;
    std::array<ComPtr<ID3D12Resource>, 2> destinations;
    std::array<HANDLE, 3> handles {};
    ProducerResources exported {};
    Producer producer;
    GpuEyeFailure failure = GpuEyeFailure::None;
    HRESULT lastHresult = S_OK;
    std::uint64_t lastSubmitted = 0;
    bool recording = false;

    ~State()
    {
        returnSources(lastSubmitted);
        // Never tear down a command allocator with our GPU work outstanding.
        // This is teardown only; the frame path never waits on a CPU event.
        if (fence && lastSubmitted != 0
            && fence->GetCompletedValue() < lastSubmitted)
        {
            const HANDLE completed = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (completed)
            {
                if (SUCCEEDED(fence->SetEventOnCompletion(lastSubmitted, completed)))
                    (void)WaitForSingleObject(completed, 5000);
                CloseHandle(completed);
            }
        }
        for (const HANDLE handle : handles)
            if (handle) CloseHandle(handle);
    }

    bool reject(GpuEyeFailure reason, HRESULT result = E_FAIL) noexcept
    {
        failure = reason;
        lastHresult = result;
        return false;
    }

    bool healthy() noexcept
    {
        if (!device || failure != GpuEyeFailure::None) return false;
        const HRESULT result = device->GetDeviceRemovedReason();
        return SUCCEEDED(result) || reject(GpuEyeFailure::DeviceRemoved, result);
    }

    bool returnSources(std::uint64_t sequence) noexcept
    {
        bool returned = true;
        for (std::size_t eye = 0; eye != unwrapped.size(); ++eye)
        {
            if (!unwrapped[eye]) continue;
            UINT64 value = sequence;
            ID3D12Fence* completion = fence.Get();
            const HRESULT result = interop->ReturnUnderlyingResource(
                sources[eye].Get(), sequence == 0 ? 0u : 1u,
                sequence == 0 ? nullptr : &value,
                sequence == 0 ? nullptr : &completion);
            unwrapped[eye].Reset();
            if (FAILED(result))
            {
                returned = false;
                reject(GpuEyeFailure::Return, result);
            }
        }
        return returned;
    }

    static D3D12_RESOURCE_BARRIER barrier(ID3D12Resource* resource,
        D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) noexcept
    {
        D3D12_RESOURCE_BARRIER value {};
        value.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        value.Transition = { resource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            before, after };
        return value;
    }

    bool initialize(const GpuEyeSource& source) noexcept
    {
        if (!source.device || !source.left || !source.right
            || source.left == source.right || source.resourceSetId == 0)
            return reject(GpuEyeFailure::InvalidSource);
        HRESULT result = source.device->QueryInterface(IID_PPV_ARGS(&interop));
        if (FAILED(result)) return reject(GpuEyeFailure::InteropUnavailable, result);
        result = interop->GetD3D12Device(IID_PPV_ARGS(&device));
        if (FAILED(result)) return reject(GpuEyeFailure::DeviceUnavailable, result);
        const LUID luid = device->GetAdapterLuid();
        exported.adapterLuid = (std::uint64_t(static_cast<std::uint32_t>(luid.HighPart)) << 32)
            | luid.LowPart;
        if (exported.adapterLuid == 0) return reject(GpuEyeFailure::AdapterUnavailable);
        exported.resourceSetId = source.resourceSetId;
        sources = { source.left, source.right };
        D3DSURFACE_DESC descriptions[2] {};
        for (std::size_t eye = 0; eye != sources.size(); ++eye)
        {
            ComPtr<IDirect3DDevice9> owner;
            if (FAILED(sources[eye]->GetDevice(&owner)) || owner.Get() != source.device
                || FAILED(sources[eye]->GetLevelDesc(0, &descriptions[eye]))
                || sources[eye]->GetLevelCount() != 1
                || descriptions[eye].Type != D3DRTYPE_SURFACE
                || descriptions[eye].Pool != D3DPOOL_DEFAULT
                || (descriptions[eye].Usage & D3DUSAGE_RENDERTARGET) == 0
                || descriptions[eye].MultiSampleType != D3DMULTISAMPLE_NONE
                || descriptions[eye].Width == 0 || descriptions[eye].Height == 0
                || descriptions[eye].Format != D3DFMT_A8B8G8R8)
                return reject(GpuEyeFailure::TextureDescription);
        }
        if (descriptions[0].Width != descriptions[1].Width
            || descriptions[0].Height != descriptions[1].Height)
            return reject(GpuEyeFailure::TextureDescription);
        exported.width = descriptions[0].Width;
        exported.height = descriptions[0].Height;
        exported.format = gpu::GpuInteropFormat::R8G8B8A8Unorm;

        D3D12_COMMAND_QUEUE_DESC queueDesc {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        result = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue));
        if (FAILED(result)) return reject(GpuEyeFailure::QueueCreation, result);
        result = device->CreateCommandAllocator(queueDesc.Type, IID_PPV_ARGS(&allocator));
        if (FAILED(result)) return reject(GpuEyeFailure::AllocatorCreation, result);
        result = device->CreateCommandList(0, queueDesc.Type, allocator.Get(), nullptr,
            IID_PPV_ARGS(&commands));
        if (FAILED(result)) return reject(GpuEyeFailure::CommandsCreation, result);
        result = commands->Close();
        if (FAILED(result)) return reject(GpuEyeFailure::CommandClose, result);
        result = device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&fence));
        if (FAILED(result)) return reject(GpuEyeFailure::FenceCreation, result);
        result = device->CreateSharedHandle(fence.Get(), nullptr, GENERIC_ALL, nullptr, &handles[2]);
        if (FAILED(result)) return reject(GpuEyeFailure::HandleCreation, result);

        D3D12_HEAP_PROPERTIES heap {};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC description {};
        description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        description.Width = exported.width;
        description.Height = exported.height;
        description.DepthOrArraySize = 1;
        description.MipLevels = 1;
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.SampleDesc.Count = 1;
        description.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
            | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
        for (std::size_t eye = 0; eye != destinations.size(); ++eye)
        {
            result = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_SHARED,
                &description, D3D12_RESOURCE_STATE_COMMON, nullptr,
                IID_PPV_ARGS(&destinations[eye]));
            if (FAILED(result)) return reject(GpuEyeFailure::SharedTextureCreation, result);
            result = device->CreateSharedHandle(destinations[eye].Get(), nullptr,
                GENERIC_ALL, nullptr, &handles[eye]);
            if (FAILED(result)) return reject(GpuEyeFailure::HandleCreation, result);
        }
        exported.leftColorNtHandle = reinterpret_cast<std::uintptr_t>(handles[0]);
        exported.rightColorNtHandle = reinterpret_cast<std::uintptr_t>(handles[1]);
        exported.sharedFenceNtHandle = reinterpret_cast<std::uintptr_t>(handles[2]);
        const ProducerOperations operations { this, &releaseReached, &beginCopy,
            &copyLeft, &copyRight, &submit };
        // The NT handles/fence are compatible with D3D11 OpenSharedResource1
        // and OpenSharedFence; synchronization stays entirely on GPU queues.
        const ProducerCapabilities capabilities { true, true, true, true, true, true, true };
        if (!producer.initialize(operations, capabilities, exported))
            return reject(GpuEyeFailure::ProducerInitialization);
        return true;
    }

    static bool releaseReached(void* opaque, std::uint64_t sequence) noexcept
    {
        auto& state = *static_cast<State*>(opaque);
        return state.healthy() && state.fence->GetCompletedValue() >= sequence;
    }

    static bool beginCopy(void* opaque) noexcept
    {
        auto& state = *static_cast<State*>(opaque);
        if (!state.healthy()) return false;
        HRESULT result = state.allocator->Reset();
        if (SUCCEEDED(result)) result = state.commands->Reset(state.allocator.Get(), nullptr);
        if (FAILED(result)) return state.reject(GpuEyeFailure::CommandReset, result);
        state.recording = true;
        return true;
    }

    bool copyEye(std::size_t eye) noexcept
    {
        if (!recording || !healthy()) return false;
        const HRESULT result = interop->UnwrapUnderlyingResource(sources[eye].Get(),
            queue.Get(), IID_PPV_ARGS(&unwrapped[eye]));
        if (FAILED(result))
        {
            returnSources(0);
            return reject(GpuEyeFailure::Unwrap, result);
        }
        const auto desc = unwrapped[eye]->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D
            || desc.Width != exported.width || desc.Height != exported.height
            || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
            || desc.MipLevels != 1 || desc.DepthOrArraySize != 1 || desc.SampleDesc.Count != 1)
        {
            returnSources(0);
            return reject(GpuEyeFailure::TextureDescription);
        }
        D3D12_RESOURCE_BARRIER before[] = {
            barrier(unwrapped[eye].Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE),
            barrier(destinations[eye].Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST) };
        commands->ResourceBarrier(2, before);
        commands->CopyResource(destinations[eye].Get(), unwrapped[eye].Get());
        D3D12_RESOURCE_BARRIER after[] = {
            barrier(unwrapped[eye].Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON),
            barrier(destinations[eye].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON) };
        commands->ResourceBarrier(2, after);
        return true;
    }

    static bool copyLeft(void* opaque) noexcept { return static_cast<State*>(opaque)->copyEye(0); }
    static bool copyRight(void* opaque) noexcept { return static_cast<State*>(opaque)->copyEye(1); }

    static bool submit(void* opaque, std::uint64_t sequence) noexcept
    {
        auto& state = *static_cast<State*>(opaque);
        if (!state.recording || !state.healthy()) return false;
        state.recording = false;
        HRESULT result = state.commands->Close();
        if (FAILED(result))
        {
            state.returnSources(0);
            return state.reject(GpuEyeFailure::CommandClose, result);
        }
        ID3D12CommandList* lists[] = { state.commands.Get() };
        state.queue->ExecuteCommandLists(1, lists);
        result = state.queue->Signal(state.fence.Get(), sequence);
        if (FAILED(result))
        {
            state.returnSources(0);
            return state.reject(GpuEyeFailure::QueueSignal, result);
        }
        state.lastSubmitted = sequence;
        return state.returnSources(sequence) && state.healthy();
    }
};

GpuEyeTransport::~GpuEyeTransport() noexcept { reset(); }
void GpuEyeTransport::reset() noexcept
{
    delete state_;
    state_ = nullptr;
    failure_ = GpuEyeFailure::None;
    lastHresult_ = S_OK;
}
bool GpuEyeTransport::initialize(const GpuEyeSource& source) noexcept
{
    reset();
    state_ = new (std::nothrow) State;
    if (!state_) { failure_ = GpuEyeFailure::DeviceUnavailable; return false; }
    if (state_->initialize(source)) return true;
    failure_ = state_->failure;
    lastHresult_ = state_->lastHresult;
    delete state_;
    state_ = nullptr;
    return false;
}
bool GpuEyeTransport::ready() const noexcept
{
    return state_ && state_->failure == GpuEyeFailure::None && state_->producer.ready();
}
bool GpuEyeTransport::available() const noexcept { return ready() && state_->producer.available(); }
GpuEyeFailure GpuEyeTransport::failure() const noexcept { return state_ ? state_->failure : failure_; }
std::int32_t GpuEyeTransport::lastHresult() const noexcept { return state_ ? state_->lastHresult : lastHresult_; }
ProducerResources GpuEyeTransport::resources() const noexcept { return state_ ? state_->exported : ProducerResources {}; }
ProducerPublication GpuEyeTransport::produce(const ProducerFrameIdentity& identity) noexcept
{
    return ready() ? state_->producer.produce(identity) : ProducerPublication {};
}
}
