#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "fnvxr_gpu_color_producer.h"

#include <cstdint>

namespace fnvxr::d3d9::color_transport
{
enum class Win32PublisherFailure : std::uint8_t
{
    None = 0u,
    InvalidName,
    ProducerLeaseUnavailable,
    MappingUnavailable,
    MappingViewUnavailable,
    NotInitialized,
    InvalidPublication,
    SharedPublicationRejected,
};

class Win32Publisher final
{
public:
    Win32Publisher() noexcept = default;
    ~Win32Publisher() noexcept;

    Win32Publisher(const Win32Publisher&) = delete;
    Win32Publisher& operator=(const Win32Publisher&) = delete;

    bool initialize(
        const char* mappingName = nullptr,
        const char* producerMutexName = nullptr) noexcept;
    void reset() noexcept;
    bool ready() const noexcept;
    bool publish(const ProducerPublication& publication) noexcept;
    Win32PublisherFailure failure() const noexcept;
    const gpu::color_v5::SharedStereoColorDescriptor* descriptor() const
        noexcept;

private:
    HANDLE mProducerMutex = nullptr;
    HANDLE mMapping = nullptr;
    gpu::color_v5::SharedStereoColorDescriptor* mDescriptor = nullptr;
    Win32PublisherFailure mFailure = Win32PublisherFailure::NotInitialized;
    bool mOwnsProducerLease = false;
    bool mOwnsProcessClaim = false;
};
}
