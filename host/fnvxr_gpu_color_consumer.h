#pragma once

#include "../protocol/fnvxr_gpu_color_transport.h"

#include <cstdint>

struct ID3D11Device;
struct ID3D11ShaderResourceView;
struct ID3D11Texture2D;

namespace fnvxr::host::gpu_color
{
enum class ConsumerFailure : std::uint8_t
{
    None,
    NotInitialized,
    UnstablePublication,
    InvalidMetadata,
    AdapterMismatch,
    ProducerIsConsumer,
    ResourceOpen,
    PublicationChangedDuringOpen,
    ProducerExited,
    FenceSequenceInvalid,
    FrameIdentityRegression,
    FenceWait,
    LeftGpuCopy,
    RightGpuCopy,
    FenceReleaseSignal,
};

enum class ConsumeDisposition : std::uint8_t
{
    Rejected,
    NoNewFrame,
    FrameReady,
};

struct ConsumerFrame
{
    gpu::color_v5::PresentationMode presentationMode =
        gpu::color_v5::PresentationMode::Unknown;
    gpu::GpuInteropFormat format = gpu::GpuInteropFormat::Unknown;
    std::uint32_t width = 0u;
    std::uint32_t height = 0u;
    std::uint64_t resourceSetId = 0u;
    std::uint64_t producerEpoch = 0u;
    std::uint32_t producerProcessId = 0u;
    std::uint64_t adapterLuid = 0u;
    std::uint64_t transactionId = 0u;
    std::uint64_t sourceFrame = 0u;
    std::uint64_t poseSequence = 0u;
    std::uint64_t runtimeStateSample = 0u;
    std::int64_t renderedDisplayTime = 0;
    std::uint64_t readySequence = 0u;
    std::uint64_t releaseSequence = 0u;
};

struct ConsumeResult
{
    ConsumeDisposition disposition = ConsumeDisposition::Rejected;
    ConsumerFailure failure = ConsumerFailure::NotInitialized;
    ConsumerFrame frame {};
};

// These operations are deliberately GPU-only. There is no CPU image buffer,
// map/readback callback, or pixel-classification callback in this interface.
// openResourceSet must open both producer textures and the producer fence, and
// prepare two consumer-private destination textures.
struct ConsumerOperations
{
    void* context = nullptr;
    std::uint64_t currentAdapterLuid = 0u;
    std::uint32_t currentProcessId = 0u;
    bool (*openResourceSet)(
        void*,
        const gpu::color_v5::SharedStereoColorDescriptor&) noexcept = nullptr;
    void (*closeResourceSet)(void*) noexcept = nullptr;
    bool (*producerStillActive)(void*, std::uint32_t) noexcept = nullptr;
    bool (*waitForReadyOnGpu)(void*, std::uint64_t) noexcept = nullptr;
    bool (*copyLeftOnGpu)(void*) noexcept = nullptr;
    bool (*copyRightOnGpu)(void*) noexcept = nullptr;
    bool (*signalReleaseOnGpu)(void*, std::uint64_t) noexcept = nullptr;
};

constexpr bool operationsComplete(
    const ConsumerOperations& operations) noexcept
{
    return operations.currentAdapterLuid != 0u
        && operations.currentProcessId != 0u
        && operations.openResourceSet
        && operations.closeResourceSet
        && operations.producerStillActive
        && operations.waitForReadyOnGpu
        && operations.copyLeftOnGpu
        && operations.copyRightOnGpu
        && operations.signalReleaseOnGpu;
}

class Consumer final
{
public:
    Consumer() noexcept = default;
    ~Consumer() noexcept;

    Consumer(const Consumer&) = delete;
    Consumer& operator=(const Consumer&) = delete;

    bool initialize(const ConsumerOperations& operations) noexcept;
    void reset() noexcept;
    bool ready() const noexcept;
    ConsumeResult consume(
        const gpu::color_v5::SharedStereoColorDescriptor* shared) noexcept;
    const ConsumerFrame& lastFrame() const noexcept;

private:
    struct ResourceIdentity
    {
        std::uint64_t fenceHandle = 0u;
        std::uint64_t leftHandle = 0u;
        std::uint64_t rightHandle = 0u;
        std::uint64_t resourceSetId = 0u;
        std::uint64_t producerEpoch = 0u;
        std::uint32_t producerProcessId = 0u;
        std::uint64_t adapterLuid = 0u;
        std::uint32_t width = 0u;
        std::uint32_t height = 0u;
        gpu::GpuInteropFormat format = gpu::GpuInteropFormat::Unknown;
    };

    ConsumeResult reject(ConsumerFailure failure, bool close) noexcept;

    ConsumerOperations mOperations {};
    ResourceIdentity mResourceIdentity {};
    ConsumerFrame mLastFrame {};
    bool mInitialized = false;
    bool mResourceSetOpen = false;
};

enum class Win32ConsumerFailure : std::uint8_t
{
    None,
    InvalidDevice,
    FenceInterfacesUnavailable,
    AdapterIdentityUnavailable,
    MappingUnavailable,
    ProducerProcessUnavailable,
    SourceHandleOutOfRange,
    HandleDuplication,
    SharedTextureOpen,
    SharedFenceOpen,
    TextureDescriptionMismatch,
    PrivateTextureCreation,
    PrivateViewCreation,
    DeviceRemoved,
};

// Concrete OpenXR-host backend. The returned texture/view pointers are
// borrowed and remain valid only while ready() is true and until the next
// rejected consume, reset, or producer resource identity change.
class Win32Consumer final
{
public:
    Win32Consumer() noexcept;
    ~Win32Consumer() noexcept;

    Win32Consumer(const Win32Consumer&) = delete;
    Win32Consumer& operator=(const Win32Consumer&) = delete;

    bool initialize(
        ID3D11Device* device,
        const wchar_t* mappingName = nullptr) noexcept;
    void reset() noexcept;
    bool ready() const noexcept;
    ConsumeResult consume() noexcept;
    Win32ConsumerFailure backendFailure() const noexcept;
    ID3D11Texture2D* eyeTexture(std::uint32_t eyeIndex) const noexcept;
    ID3D11ShaderResourceView* eyeView(
        std::uint32_t eyeIndex) const noexcept;
    const ConsumerFrame& lastFrame() const noexcept;

private:
    struct Implementation;
    Implementation* mImplementation = nullptr;
    Win32ConsumerFailure mFailure = Win32ConsumerFailure::None;
};
}
