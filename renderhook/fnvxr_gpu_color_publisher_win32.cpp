#include "fnvxr_gpu_color_publisher_win32.h"

#include <cstring>
#include <new>

namespace fnvxr::d3d9::color_transport
{
namespace
{
constexpr char DefaultProducerMutexName[] =
    "Local\\FNVXR_GPU_StereoColor_v5_Producer";
volatile LONG ProcessPublisherClaimed = 0;
}

Win32Publisher::~Win32Publisher() noexcept
{
    reset();
}

bool Win32Publisher::initialize(
    const char* mappingName,
    const char* producerMutexName) noexcept
{
    reset();
    const char* selectedMapping = mappingName && mappingName[0] != '\0'
        ? mappingName
        : gpu::color_v5::SharedStereoColorMappingName;
    const char* selectedMutex = producerMutexName
            && producerMutexName[0] != '\0'
        ? producerMutexName
        : DefaultProducerMutexName;
    if (!selectedMapping[0] || !selectedMutex[0])
    {
        mFailure = Win32PublisherFailure::InvalidName;
        return false;
    }

    // A Win32 mutex is recursive for its owning thread. The process-local
    // claim prevents a second publisher object on that same thread from
    // silently acquiring the named lifetime lease again.
    if (InterlockedCompareExchange(&ProcessPublisherClaimed, 1, 0) != 0)
    {
        mFailure = Win32PublisherFailure::ProducerLeaseUnavailable;
        return false;
    }
    mOwnsProcessClaim = true;

    mProducerMutex = CreateMutexA(nullptr, FALSE, selectedMutex);
    if (!mProducerMutex)
    {
        const Win32PublisherFailure failure =
            Win32PublisherFailure::ProducerLeaseUnavailable;
        reset();
        mFailure = failure;
        return false;
    }
    const DWORD wait = WaitForSingleObject(mProducerMutex, 0u);
    if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED)
    {
        const Win32PublisherFailure failure =
            Win32PublisherFailure::ProducerLeaseUnavailable;
        reset();
        mFailure = failure;
        return false;
    }
    mOwnsProducerLease = true;

    mMapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0u,
        sizeof(gpu::color_v5::SharedStereoColorDescriptor),
        selectedMapping);
    if (!mMapping)
    {
        const Win32PublisherFailure failure =
            Win32PublisherFailure::MappingUnavailable;
        reset();
        mFailure = failure;
        return false;
    }
    mDescriptor = static_cast<gpu::color_v5::SharedStereoColorDescriptor*>(
        MapViewOfFile(
            mMapping,
            FILE_MAP_ALL_ACCESS,
            0u,
            0u,
            sizeof(gpu::color_v5::SharedStereoColorDescriptor)));
    if (!mDescriptor)
    {
        const Win32PublisherFailure failure =
            Win32PublisherFailure::MappingViewUnavailable;
        reset();
        mFailure = failure;
        return false;
    }

    // The named mutex is the sole-writer lifetime lease. Reinitializing the
    // descriptor after acquiring it invalidates any stale process lifetime
    // before new NT handles can be advertised.
    std::memset(
        static_cast<void*>(mDescriptor),
        0,
        sizeof(*mDescriptor));
    new (mDescriptor) gpu::color_v5::SharedStereoColorDescriptor {};
    mFailure = Win32PublisherFailure::None;
    return true;
}

void Win32Publisher::reset() noexcept
{
    if (mDescriptor)
        UnmapViewOfFile(mDescriptor);
    if (mMapping)
        CloseHandle(mMapping);
    if (mOwnsProducerLease && mProducerMutex)
        ReleaseMutex(mProducerMutex);
    if (mProducerMutex)
        CloseHandle(mProducerMutex);
    if (mOwnsProcessClaim)
        InterlockedExchange(&ProcessPublisherClaimed, 0);
    mProducerMutex = nullptr;
    mMapping = nullptr;
    mDescriptor = nullptr;
    mOwnsProducerLease = false;
    mOwnsProcessClaim = false;
    mFailure = Win32PublisherFailure::NotInitialized;
}

bool Win32Publisher::ready() const noexcept
{
    return mOwnsProducerLease
        && mProducerMutex
        && mMapping
        && mDescriptor
        && mFailure == Win32PublisherFailure::None;
}

bool Win32Publisher::publish(
    const ProducerPublication& publication) noexcept
{
    if (!ready())
    {
        mFailure = Win32PublisherFailure::NotInitialized;
        return false;
    }
    if (!publication.complete
        || publication.failure != ProducerFailure::None
        || gpu::color_v5::validatePayloadMetadata(publication.payload)
            != gpu::color_v5::Validation::Valid)
    {
        mFailure = Win32PublisherFailure::InvalidPublication;
        return false;
    }
    const bool published = gpu::color_v5::publish(
        mDescriptor,
        [&publication](gpu::color_v5::SharedStereoColorPayload& payload) {
            payload = publication.payload;
            return true;
        });
    mFailure = published
        ? Win32PublisherFailure::None
        : Win32PublisherFailure::SharedPublicationRejected;
    return published;
}

Win32PublisherFailure Win32Publisher::failure() const noexcept
{
    return mFailure;
}

const gpu::color_v5::SharedStereoColorDescriptor*
Win32Publisher::descriptor() const noexcept
{
    return mDescriptor;
}
}
