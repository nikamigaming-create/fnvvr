#pragma once

#include "fnvxr_gpu_color_producer.h"

struct IDirect3DDevice9;
struct IDirect3DTexture9;

namespace fnvxr::d3d9::color_transport
{
// Process-local, bounded diagnostics. Does not enable Watson/GPU heap dumps.
bool enableDeviceRemovalDiagnostics() noexcept;
// D3D9On12 owns engine rendering; a private D3D12 queue copies complete eye
// pairs to NT-shared textures. The D3D11 consumer releases the shared fence
// before another pair can overwrite them. No pixels pass through CPU memory.
struct GpuEyeSource
{
    IDirect3DDevice9* device = nullptr;
    IDirect3DTexture9* left = nullptr;
    IDirect3DTexture9* right = nullptr;
    std::uint64_t resourceSetId = 0;
};

enum class GpuEyeFailure : std::uint8_t
{
    None, InvalidSource, InteropUnavailable, DeviceUnavailable,
    AdapterUnavailable, QueueCreation, AllocatorCreation, CommandsCreation,
    FenceCreation, TextureDescription, SharedTextureCreation, HandleCreation,
    Unwrap, Return, CommandReset, CommandClose, QueueSignal, DeviceRemoved,
    ProducerInitialization,
};

class GpuEyeTransport final
{
public:
    GpuEyeTransport() noexcept = default;
    ~GpuEyeTransport() noexcept;
    GpuEyeTransport(const GpuEyeTransport&) = delete;
    GpuEyeTransport& operator=(const GpuEyeTransport&) = delete;
    bool initialize(const GpuEyeSource& source) noexcept;
    void reset() noexcept;
    bool ready() const noexcept;
    bool available() const noexcept;
    GpuEyeFailure failure() const noexcept;
    std::int32_t lastHresult() const noexcept;
    const char* failureDetails() const noexcept;
    ProducerResources resources() const noexcept;
    ProducerPublication produce(const ProducerFrameIdentity& identity) noexcept;
private:
    struct State;
    State* state_ = nullptr;
    GpuEyeFailure failure_ = GpuEyeFailure::None;
    std::int32_t lastHresult_ = 0;
};
}
