#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <d3d11_4.h>
#include <dxgi1_2.h>

#include "fnvxr_gpu_color_consumer.h"

#include <cstdint>
#include <cstring>
#include <cwchar>
#include <limits>
#include <new>

namespace fnvxr::host::gpu_color
{
namespace
{
using Descriptor = gpu::color_v5::SharedStereoColorDescriptor;

bool sameDescriptorPayload(
    const Descriptor& left,
    const Descriptor& right) noexcept
{
    return left.publicationSequence.load(std::memory_order_relaxed)
            == right.publicationSequence.load(std::memory_order_relaxed)
        && left.magic == right.magic
        && left.version == right.version
        && left.descriptorBytes == right.descriptorBytes
        && left.gpuCompletionHandle == right.gpuCompletionHandle
        && left.gpuReadySequence == right.gpuReadySequence
        && left.gpuConsumerReleaseSequence
            == right.gpuConsumerReleaseSequence
        && left.gpuCompletionPrimitive == right.gpuCompletionPrimitive
        && left.presentationMode == right.presentationMode
        && left.resourceSetId == right.resourceSetId
        && left.producerEpoch == right.producerEpoch
        && left.producerProcessId == right.producerProcessId
        && left.reserved == right.reserved
        && left.adapterLuid == right.adapterLuid
        && left.transactionId == right.transactionId
        && left.sourceFrame == right.sourceFrame
        && left.poseSequence == right.poseSequence
        && left.runtimeStateSample == right.runtimeStateSample
        && left.renderedDisplayTime == right.renderedDisplayTime
        && left.leftColor.sharedHandle == right.leftColor.sharedHandle
        && left.leftColor.width == right.leftColor.width
        && left.leftColor.height == right.leftColor.height
        && left.leftColor.format == right.leftColor.format
        && left.leftColor.handleType == right.leftColor.handleType
        && left.rightColor.sharedHandle == right.rightColor.sharedHandle
        && left.rightColor.width == right.rightColor.width
        && left.rightColor.height == right.rightColor.height
        && left.rightColor.format == right.rightColor.format
        && left.rightColor.handleType == right.rightColor.handleType;
}

ConsumerFrame makeFrame(const Descriptor& value) noexcept
{
    ConsumerFrame frame {};
    frame.presentationMode = value.presentationMode;
    frame.format = value.leftColor.format;
    frame.width = value.leftColor.width;
    frame.height = value.leftColor.height;
    frame.resourceSetId = value.resourceSetId;
    frame.producerEpoch = value.producerEpoch;
    frame.producerProcessId = value.producerProcessId;
    frame.adapterLuid = value.adapterLuid;
    frame.transactionId = value.transactionId;
    frame.sourceFrame = value.sourceFrame;
    frame.poseSequence = value.poseSequence;
    frame.runtimeStateSample = value.runtimeStateSample;
    frame.renderedDisplayTime = value.renderedDisplayTime;
    frame.readySequence = value.gpuReadySequence;
    frame.releaseSequence = value.gpuConsumerReleaseSequence;
    return frame;
}

bool samePublishedFrame(
    const Descriptor& value,
    const ConsumerFrame& frame) noexcept
{
    return value.presentationMode == frame.presentationMode
        && value.resourceSetId == frame.resourceSetId
        && value.producerEpoch == frame.producerEpoch
        && value.producerProcessId == frame.producerProcessId
        && value.adapterLuid == frame.adapterLuid
        && value.transactionId == frame.transactionId
        && value.sourceFrame == frame.sourceFrame
        && value.poseSequence == frame.poseSequence
        && value.runtimeStateSample == frame.runtimeStateSample
        && value.renderedDisplayTime == frame.renderedDisplayTime
        && value.gpuReadySequence == frame.readySequence
        && value.gpuConsumerReleaseSequence == frame.releaseSequence;
}

template <typename Interface>
void releaseInterface(Interface*& value) noexcept
{
    if (value)
    {
        value->Release();
        value = nullptr;
    }
}

std::uint64_t packLuid(const LUID& value) noexcept
{
    return (static_cast<std::uint64_t>(
                static_cast<std::uint32_t>(value.HighPart))
            << 32u)
        | static_cast<std::uint64_t>(value.LowPart);
}

DXGI_FORMAT dxgiFormat(gpu::GpuInteropFormat value) noexcept
{
    switch (value)
    {
    case gpu::GpuInteropFormat::R8G8B8A8Unorm:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case gpu::GpuInteropFormat::R10G10B10A2Unorm:
        return DXGI_FORMAT_R10G10B10A2_UNORM;
    case gpu::GpuInteropFormat::R16G16B16A16Float:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    default:
        return DXGI_FORMAT_UNKNOWN;
    }
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
}

Consumer::~Consumer() noexcept
{
    reset();
}

bool Consumer::initialize(const ConsumerOperations& operations) noexcept
{
    reset();
    if (!operationsComplete(operations))
        return false;
    mOperations = operations;
    mInitialized = true;
    return true;
}

void Consumer::reset() noexcept
{
    if (mResourceSetOpen && mOperations.closeResourceSet)
        mOperations.closeResourceSet(mOperations.context);
    mOperations = {};
    mResourceIdentity = {};
    mLastFrame = {};
    mInitialized = false;
    mResourceSetOpen = false;
}

bool Consumer::ready() const noexcept
{
    return mInitialized && operationsComplete(mOperations);
}

const ConsumerFrame& Consumer::lastFrame() const noexcept
{
    return mLastFrame;
}

ConsumeResult Consumer::reject(
    ConsumerFailure failure,
    bool close) noexcept
{
    if (close && mOperations.closeResourceSet)
        mOperations.closeResourceSet(mOperations.context);
    if (close)
    {
        mResourceIdentity = {};
        mLastFrame = {};
        mResourceSetOpen = false;
    }
    return { ConsumeDisposition::Rejected, failure, {} };
}

ConsumeResult Consumer::consume(const Descriptor* shared) noexcept
{
    if (!ready())
        return reject(ConsumerFailure::NotInitialized, false);

    Descriptor first {};
    if (!gpu::color_v5::readStableSnapshot(shared, first))
        return reject(ConsumerFailure::UnstablePublication, false);
    if (gpu::color_v5::validatePayloadMetadata(first)
        != gpu::color_v5::Validation::Valid)
    {
        return reject(ConsumerFailure::InvalidMetadata, true);
    }
    if (first.adapterLuid != mOperations.currentAdapterLuid)
        return reject(ConsumerFailure::AdapterMismatch, true);
    if (first.producerProcessId == mOperations.currentProcessId)
        return reject(ConsumerFailure::ProducerIsConsumer, true);

    ResourceIdentity candidate {};
    candidate.fenceHandle = first.gpuCompletionHandle;
    candidate.leftHandle = first.leftColor.sharedHandle;
    candidate.rightHandle = first.rightColor.sharedHandle;
    candidate.resourceSetId = first.resourceSetId;
    candidate.producerEpoch = first.producerEpoch;
    candidate.producerProcessId = first.producerProcessId;
    candidate.adapterLuid = first.adapterLuid;
    candidate.width = first.leftColor.width;
    candidate.height = first.leftColor.height;
    candidate.format = first.leftColor.format;

    const bool sameResourceIdentity = mResourceSetOpen
        && candidate.fenceHandle == mResourceIdentity.fenceHandle
        && candidate.leftHandle == mResourceIdentity.leftHandle
        && candidate.rightHandle == mResourceIdentity.rightHandle
        && candidate.resourceSetId == mResourceIdentity.resourceSetId
        && candidate.producerEpoch == mResourceIdentity.producerEpoch
        && candidate.producerProcessId
            == mResourceIdentity.producerProcessId
        && candidate.adapterLuid == mResourceIdentity.adapterLuid
        && candidate.width == mResourceIdentity.width
        && candidate.height == mResourceIdentity.height
        && candidate.format == mResourceIdentity.format;

    if (!sameResourceIdentity)
    {
        if (mResourceSetOpen)
            mOperations.closeResourceSet(mOperations.context);
        mResourceIdentity = {};
        mLastFrame = {};
        mResourceSetOpen = true;
        if (!mOperations.openResourceSet(mOperations.context, first))
            return reject(ConsumerFailure::ResourceOpen, true);

        // Handle duplication and COM opens are not allowed to bridge two
        // publications. Re-read the seqlock after opening and require the
        // complete descriptor to be byte-for-byte equivalent by field.
        Descriptor afterOpen {};
        if (!gpu::color_v5::readStableSnapshot(shared, afterOpen)
            || !sameDescriptorPayload(first, afterOpen))
        {
            return reject(
                ConsumerFailure::PublicationChangedDuringOpen,
                true);
        }
        mResourceIdentity = candidate;
    }

    if (!mOperations.producerStillActive(
            mOperations.context,
            first.producerProcessId))
    {
        return reject(ConsumerFailure::ProducerExited, true);
    }

    if ((first.gpuReadySequence & 1u) == 0u
        || first.gpuConsumerReleaseSequence
            != first.gpuReadySequence + 1u)
    {
        return reject(ConsumerFailure::FenceSequenceInvalid, true);
    }

    if (mLastFrame.readySequence != 0u)
    {
        if (first.gpuReadySequence == mLastFrame.readySequence)
        {
            if (!samePublishedFrame(first, mLastFrame))
                return reject(ConsumerFailure::FenceSequenceInvalid, true);
            return {
                ConsumeDisposition::NoNewFrame,
                ConsumerFailure::None,
                mLastFrame,
            };
        }
        if (first.gpuReadySequence != mLastFrame.releaseSequence + 1u)
            return reject(ConsumerFailure::FenceSequenceInvalid, true);
        if (first.transactionId <= mLastFrame.transactionId
            // Fallout may render more than once from one published OpenXR
            // pose (including a world-to-menu transition before the next
            // host sample). Transaction and fence identities must advance;
            // the source pose frame may remain equal but can never regress.
            || first.sourceFrame < mLastFrame.sourceFrame
            || first.poseSequence < mLastFrame.poseSequence
            || first.runtimeStateSample < mLastFrame.runtimeStateSample
            || first.renderedDisplayTime < mLastFrame.renderedDisplayTime)
        {
            return reject(ConsumerFailure::FrameIdentityRegression, true);
        }
    }

    if (!mOperations.waitForReadyOnGpu(
            mOperations.context,
            first.gpuReadySequence))
    {
        return reject(ConsumerFailure::FenceWait, true);
    }
    if (!mOperations.copyLeftOnGpu(mOperations.context))
        return reject(ConsumerFailure::LeftGpuCopy, true);
    if (!mOperations.copyRightOnGpu(mOperations.context))
        return reject(ConsumerFailure::RightGpuCopy, true);
    if (!mOperations.signalReleaseOnGpu(
            mOperations.context,
            first.gpuConsumerReleaseSequence))
    {
        return reject(ConsumerFailure::FenceReleaseSignal, true);
    }

    mLastFrame = makeFrame(first);
    return {
        ConsumeDisposition::FrameReady,
        ConsumerFailure::None,
        mLastFrame,
    };
}

struct Win32Consumer::Implementation final
{
    static constexpr wchar_t DefaultMappingName[] =
        L"Local\\FNVXR_GPU_StereoColor_v5";
    static constexpr std::size_t MappingNameCapacity = 128u;

    ID3D11Device* device = nullptr;
    ID3D11Device5* device5 = nullptr;
    ID3D11DeviceContext* context = nullptr;
    ID3D11DeviceContext4* context4 = nullptr;
    HANDLE mappingHandle = nullptr;
    const Descriptor* shared = nullptr;
    HANDLE producerProcess = nullptr;
    ID3D11Texture2D* sharedTextures[2] { nullptr, nullptr };
    ID3D11Fence* sharedFence = nullptr;
    ID3D11Texture2D* privateTextures[2] { nullptr, nullptr };
    ID3D11ShaderResourceView* privateViews[2] { nullptr, nullptr };
    Consumer consumer {};
    wchar_t mappingName[MappingNameCapacity] {};
    std::uint64_t adapterLuid = 0u;
    Win32ConsumerFailure failure = Win32ConsumerFailure::None;

    ~Implementation() noexcept
    {
        releaseAll();
    }

    void releaseResourceSet() noexcept
    {
        releaseInterface(privateViews[0]);
        releaseInterface(privateViews[1]);
        releaseInterface(privateTextures[0]);
        releaseInterface(privateTextures[1]);
        releaseInterface(sharedFence);
        releaseInterface(sharedTextures[0]);
        releaseInterface(sharedTextures[1]);
        if (producerProcess)
        {
            CloseHandle(producerProcess);
            producerProcess = nullptr;
        }
    }

    void releaseMapping() noexcept
    {
        if (shared)
        {
            UnmapViewOfFile(shared);
            shared = nullptr;
        }
        if (mappingHandle)
        {
            CloseHandle(mappingHandle);
            mappingHandle = nullptr;
        }
    }

    void releaseAll() noexcept
    {
        consumer.reset();
        releaseResourceSet();
        releaseMapping();
        releaseInterface(context4);
        releaseInterface(context);
        releaseInterface(device5);
        releaseInterface(device);
        adapterLuid = 0u;
    }

    bool reject(Win32ConsumerFailure value) noexcept
    {
        failure = value;
        return false;
    }

    bool copyMappingName(const wchar_t* requested) noexcept
    {
        const wchar_t* source = requested && requested[0] != L'\0'
            ? requested
            : DefaultMappingName;
        const std::size_t length = std::wcslen(source);
        if (length == 0u || length >= MappingNameCapacity)
            return false;
        std::wmemcpy(mappingName, source, length + 1u);
        return true;
    }

    bool acquireAdapterIdentity() noexcept
    {
        IDXGIDevice* dxgiDevice = nullptr;
        IDXGIAdapter* adapter = nullptr;
        DXGI_ADAPTER_DESC description {};
        const bool available = SUCCEEDED(device->QueryInterface(
                __uuidof(IDXGIDevice),
                reinterpret_cast<void**>(&dxgiDevice)))
            && dxgiDevice
            && SUCCEEDED(dxgiDevice->GetAdapter(&adapter))
            && adapter
            && SUCCEEDED(adapter->GetDesc(&description));
        releaseInterface(adapter);
        releaseInterface(dxgiDevice);
        if (!available || packLuid(description.AdapterLuid) == 0u)
            return reject(Win32ConsumerFailure::AdapterIdentityUnavailable);
        adapterLuid = packLuid(description.AdapterLuid);
        return true;
    }

    bool initialize(ID3D11Device* source, const wchar_t* name) noexcept
    {
        if (!source || !copyMappingName(name))
            return reject(Win32ConsumerFailure::InvalidDevice);
        source->AddRef();
        device = source;
        if (FAILED(device->QueryInterface(
                __uuidof(ID3D11Device5),
                reinterpret_cast<void**>(&device5)))
            || !device5)
        {
            return reject(
                Win32ConsumerFailure::FenceInterfacesUnavailable);
        }
        device->GetImmediateContext(&context);
        if (!context
            || FAILED(context->QueryInterface(
                __uuidof(ID3D11DeviceContext4),
                reinterpret_cast<void**>(&context4)))
            || !context4)
        {
            return reject(
                Win32ConsumerFailure::FenceInterfacesUnavailable);
        }
        if (!acquireAdapterIdentity())
            return false;

        const ConsumerOperations operations {
            this,
            adapterLuid,
            GetCurrentProcessId(),
            &openResourceSetCallback,
            &closeResourceSetCallback,
            &producerStillActiveCallback,
            &waitForReadyCallback,
            &copyLeftCallback,
            &copyRightCallback,
            &signalReleaseCallback,
        };
        if (!consumer.initialize(operations))
            return reject(Win32ConsumerFailure::InvalidDevice);
        failure = Win32ConsumerFailure::None;
        return true;
    }

    bool ensureMapping() noexcept
    {
        if (shared && mappingHandle)
            return true;
        releaseMapping();
        mappingHandle = OpenFileMappingW(FILE_MAP_READ, FALSE, mappingName);
        if (!mappingHandle)
            return reject(Win32ConsumerFailure::MappingUnavailable);
        shared = static_cast<const Descriptor*>(MapViewOfFile(
            mappingHandle,
            FILE_MAP_READ,
            0u,
            0u,
            sizeof(Descriptor)));
        if (!shared)
        {
            releaseMapping();
            return reject(Win32ConsumerFailure::MappingUnavailable);
        }
        return true;
    }

    static bool rawHandleValid(std::uint64_t value) noexcept
    {
        return value != 0u
            && value <= static_cast<std::uint64_t>(
                (std::numeric_limits<std::uintptr_t>::max)())
            && static_cast<std::uintptr_t>(value)
                != reinterpret_cast<std::uintptr_t>(INVALID_HANDLE_VALUE);
    }

    bool duplicateProducerHandle(
        std::uint64_t sourceValue,
        HANDLE& destination) noexcept
    {
        destination = nullptr;
        if (!rawHandleValid(sourceValue))
            return reject(Win32ConsumerFailure::SourceHandleOutOfRange);
        const HANDLE sourceHandle = reinterpret_cast<HANDLE>(
            static_cast<std::uintptr_t>(sourceValue));
        if (!DuplicateHandle(
                producerProcess,
                sourceHandle,
                GetCurrentProcess(),
                &destination,
                0u,
                FALSE,
                DUPLICATE_SAME_ACCESS)
            || !destination)
        {
            return reject(Win32ConsumerFailure::HandleDuplication);
        }
        return true;
    }

    bool sourceDescriptionValid(
        ID3D11Texture2D* texture,
        const gpu::GpuTextureDescriptor& expected) noexcept
    {
        if (!texture)
            return false;
        D3D11_TEXTURE2D_DESC actual {};
        texture->GetDesc(&actual);
        return actual.Width == expected.width
            && actual.Height == expected.height
            && actual.MipLevels == 1u
            && actual.ArraySize == 1u
            && actual.Format == dxgiFormat(expected.format)
            && actual.SampleDesc.Count == 1u
            && actual.SampleDesc.Quality == 0u
            && actual.Usage == D3D11_USAGE_DEFAULT
            && (actual.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0u
            && actual.CPUAccessFlags == 0u
            && (actual.MiscFlags
                & (D3D11_RESOURCE_MISC_SHARED_NTHANDLE
                    | D3D11_RESOURCE_MISC_SHARED))
                == (D3D11_RESOURCE_MISC_SHARED_NTHANDLE
                    | D3D11_RESOURCE_MISC_SHARED)
            && (actual.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX)
                == 0u;
    }

    bool createPrivateEye(
        const gpu::GpuTextureDescriptor& expected,
        std::uint32_t index) noexcept
    {
        D3D11_TEXTURE2D_DESC description {};
        description.Width = expected.width;
        description.Height = expected.height;
        description.MipLevels = 1u;
        description.ArraySize = 1u;
        description.Format = dxgiFormat(expected.format);
        description.SampleDesc.Count = 1u;
        description.SampleDesc.Quality = 0u;
        description.Usage = D3D11_USAGE_DEFAULT;
        description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        description.CPUAccessFlags = 0u;
        description.MiscFlags = 0u;
        if (FAILED(device->CreateTexture2D(
                &description,
                nullptr,
                &privateTextures[index]))
            || !privateTextures[index])
        {
            return reject(Win32ConsumerFailure::PrivateTextureCreation);
        }
        if (FAILED(device->CreateShaderResourceView(
                privateTextures[index],
                nullptr,
                &privateViews[index]))
            || !privateViews[index])
        {
            return reject(Win32ConsumerFailure::PrivateViewCreation);
        }
        return true;
    }

    bool openResourceSet(const Descriptor& value) noexcept
    {
        releaseResourceSet();
        failure = Win32ConsumerFailure::None;
        if (!rawHandleValid(value.leftColor.sharedHandle)
            || !rawHandleValid(value.rightColor.sharedHandle)
            || !rawHandleValid(value.gpuCompletionHandle))
        {
            return reject(Win32ConsumerFailure::SourceHandleOutOfRange);
        }
        if (value.leftColor.sharedHandle == value.rightColor.sharedHandle
            || value.leftColor.sharedHandle == value.gpuCompletionHandle
            || value.rightColor.sharedHandle == value.gpuCompletionHandle)
        {
            return reject(Win32ConsumerFailure::HandleDuplication);
        }

        constexpr DWORD access = PROCESS_DUP_HANDLE
            | PROCESS_QUERY_LIMITED_INFORMATION
            | SYNCHRONIZE;
        producerProcess = OpenProcess(
            access,
            FALSE,
            value.producerProcessId);
        if (!producerProcess
            || GetProcessId(producerProcess) != value.producerProcessId
            || WaitForSingleObject(producerProcess, 0u) != WAIT_TIMEOUT)
        {
            releaseResourceSet();
            return reject(
                Win32ConsumerFailure::ProducerProcessUnavailable);
        }

        HANDLE duplicatedLeft = nullptr;
        HANDLE duplicatedRight = nullptr;
        HANDLE duplicatedFence = nullptr;
        const bool duplicated = duplicateProducerHandle(
                value.leftColor.sharedHandle,
                duplicatedLeft)
            && duplicateProducerHandle(
                value.rightColor.sharedHandle,
                duplicatedRight)
            && duplicateProducerHandle(
                value.gpuCompletionHandle,
                duplicatedFence);
        if (duplicated)
        {
            if (FAILED(device5->OpenSharedResource1(
                    duplicatedLeft,
                    __uuidof(ID3D11Texture2D),
                    reinterpret_cast<void**>(&sharedTextures[0])))
                || !sharedTextures[0]
                || FAILED(device5->OpenSharedResource1(
                    duplicatedRight,
                    __uuidof(ID3D11Texture2D),
                    reinterpret_cast<void**>(&sharedTextures[1])))
                || !sharedTextures[1])
            {
                reject(Win32ConsumerFailure::SharedTextureOpen);
            }
            else if (FAILED(device5->OpenSharedFence(
                    duplicatedFence,
                    __uuidof(ID3D11Fence),
                    reinterpret_cast<void**>(&sharedFence)))
                || !sharedFence)
            {
                reject(Win32ConsumerFailure::SharedFenceOpen);
            }
        }
        if (duplicatedLeft)
            CloseHandle(duplicatedLeft);
        if (duplicatedRight)
            CloseHandle(duplicatedRight);
        if (duplicatedFence)
            CloseHandle(duplicatedFence);
        if (!duplicated
            || !sharedTextures[0]
            || !sharedTextures[1]
            || !sharedFence)
        {
            releaseResourceSet();
            return false;
        }

        if (sameComIdentity(sharedTextures[0], sharedTextures[1])
            || !sourceDescriptionValid(
                sharedTextures[0],
                value.leftColor)
            || !sourceDescriptionValid(
                sharedTextures[1],
                value.rightColor))
        {
            releaseResourceSet();
            return reject(
                Win32ConsumerFailure::TextureDescriptionMismatch);
        }
        if (!createPrivateEye(value.leftColor, 0u)
            || !createPrivateEye(value.rightColor, 1u))
        {
            releaseResourceSet();
            return false;
        }
        return true;
    }

    bool producerStillActive(std::uint32_t expectedPid) noexcept
    {
        DWORD exitCode = 0u;
        if (!producerProcess
            || GetProcessId(producerProcess) != expectedPid
            || WaitForSingleObject(producerProcess, 0u) != WAIT_TIMEOUT
            || !GetExitCodeProcess(producerProcess, &exitCode)
            || exitCode != STILL_ACTIVE)
        {
            return reject(
                Win32ConsumerFailure::ProducerProcessUnavailable);
        }
        return true;
    }

    bool deviceAvailable() noexcept
    {
        if (!device || device->GetDeviceRemovedReason() != S_OK)
            return reject(Win32ConsumerFailure::DeviceRemoved);
        return true;
    }

    bool waitForReady(std::uint64_t sequence) noexcept
    {
        return deviceAvailable()
            && context4
            && sharedFence
            && SUCCEEDED(context4->Wait(sharedFence, sequence))
            && deviceAvailable();
    }

    bool copyEye(std::uint32_t index) noexcept
    {
        if (!deviceAvailable()
            || !context
            || !sharedTextures[index]
            || !privateTextures[index])
        {
            return false;
        }
        context->CopyResource(privateTextures[index], sharedTextures[index]);
        return deviceAvailable();
    }

    bool signalRelease(std::uint64_t sequence) noexcept
    {
        if (!deviceAvailable()
            || !context
            || !context4
            || !sharedFence
            || FAILED(context4->Signal(sharedFence, sequence)))
        {
            return false;
        }
        // Signal is ordered after both CopyResource calls. Flush makes the
        // release visible promptly to the producer without any CPU readback.
        context->Flush();
        return deviceAvailable();
    }

    static Implementation* checked(void* opaque) noexcept
    {
        return static_cast<Implementation*>(opaque);
    }

    static bool openResourceSetCallback(
        void* opaque,
        const Descriptor& value) noexcept
    {
        Implementation* state = checked(opaque);
        return state && state->openResourceSet(value);
    }

    static void closeResourceSetCallback(void* opaque) noexcept
    {
        Implementation* state = checked(opaque);
        if (state)
            state->releaseResourceSet();
    }

    static bool producerStillActiveCallback(
        void* opaque,
        std::uint32_t expectedPid) noexcept
    {
        Implementation* state = checked(opaque);
        return state && state->producerStillActive(expectedPid);
    }

    static bool waitForReadyCallback(
        void* opaque,
        std::uint64_t sequence) noexcept
    {
        Implementation* state = checked(opaque);
        return state && state->waitForReady(sequence);
    }

    static bool copyLeftCallback(void* opaque) noexcept
    {
        Implementation* state = checked(opaque);
        return state && state->copyEye(0u);
    }

    static bool copyRightCallback(void* opaque) noexcept
    {
        Implementation* state = checked(opaque);
        return state && state->copyEye(1u);
    }

    static bool signalReleaseCallback(
        void* opaque,
        std::uint64_t sequence) noexcept
    {
        Implementation* state = checked(opaque);
        return state && state->signalRelease(sequence);
    }
};

constexpr wchar_t Win32Consumer::Implementation::DefaultMappingName[];

Win32Consumer::Win32Consumer() noexcept = default;

Win32Consumer::~Win32Consumer() noexcept
{
    reset();
}

bool Win32Consumer::initialize(
    ID3D11Device* device,
    const wchar_t* mappingName) noexcept
{
    reset();
    mImplementation = new (std::nothrow) Implementation;
    if (!mImplementation)
    {
        mFailure = Win32ConsumerFailure::InvalidDevice;
        return false;
    }
    if (!mImplementation->initialize(device, mappingName))
    {
        mFailure = mImplementation->failure;
        delete mImplementation;
        mImplementation = nullptr;
        return false;
    }
    mFailure = Win32ConsumerFailure::None;
    return true;
}

void Win32Consumer::reset() noexcept
{
    delete mImplementation;
    mImplementation = nullptr;
    mFailure = Win32ConsumerFailure::None;
}

bool Win32Consumer::ready() const noexcept
{
    return mImplementation && mImplementation->consumer.ready();
}

ConsumeResult Win32Consumer::consume() noexcept
{
    if (!mImplementation)
    {
        mFailure = Win32ConsumerFailure::InvalidDevice;
        return {
            ConsumeDisposition::Rejected,
            ConsumerFailure::NotInitialized,
            {},
        };
    }
    if (!mImplementation->ensureMapping())
    {
        mFailure = mImplementation->failure;
        return {
            ConsumeDisposition::Rejected,
            ConsumerFailure::NotInitialized,
            {},
        };
    }
    mImplementation->failure = Win32ConsumerFailure::None;
    const ConsumeResult result = mImplementation->consumer.consume(
        mImplementation->shared);
    mFailure = mImplementation->failure;
    if (result.failure == ConsumerFailure::ProducerExited)
        mImplementation->releaseMapping();
    return result;
}

Win32ConsumerFailure Win32Consumer::backendFailure() const noexcept
{
    return mImplementation ? mImplementation->failure : mFailure;
}

ID3D11Texture2D* Win32Consumer::eyeTexture(
    std::uint32_t eyeIndex) const noexcept
{
    return mImplementation && eyeIndex < 2u
        ? mImplementation->privateTextures[eyeIndex]
        : nullptr;
}

ID3D11ShaderResourceView* Win32Consumer::eyeView(
    std::uint32_t eyeIndex) const noexcept
{
    return mImplementation && eyeIndex < 2u
        ? mImplementation->privateViews[eyeIndex]
        : nullptr;
}

const ConsumerFrame& Win32Consumer::lastFrame() const noexcept
{
    static const ConsumerFrame Empty {};
    return mImplementation
        ? mImplementation->consumer.lastFrame()
        : Empty;
}
}
