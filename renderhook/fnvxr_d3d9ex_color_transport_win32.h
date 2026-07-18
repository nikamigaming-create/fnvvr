#pragma once

#include "fnvxr_gpu_color_producer.h"

#include <cstdint>

struct IDirect3DDevice9;
struct IDirect3DTexture9;

namespace fnvxr::d3d9::color_transport
{
enum class Win32ProducerFailure : std::uint8_t
{
    None,
    InvalidInput,
    SourceNotD3D9Ex,
    SourceTextureInvalid,
    SourceSharedHandleInvalid,
    AdapterIdentityUnavailable,
    GraphicsRuntimeUnavailable,
    DxgiFactoryCreation,
    MatchingDxgiAdapterUnavailable,
    D3D11DeviceCreation,
    D3D11FeatureLevelUnsupported,
    D3D11FenceInterfacesUnavailable,
    D3D9SourceOpen,
    D3D11SourceDescriptionInvalid,
    D3D11FormatUnsupported,
    D3D11NtTextureCreation,
    D3D11NtHandleCreation,
    D3D11SharedFenceCreation,
    D3D9EventQueryCreation,
    ProducerContractInitialization,
};

struct Win32ProducerSource
{
    IDirect3DDevice9* device = nullptr;
    IDirect3DTexture9* leftTexture = nullptr;
    IDirect3DTexture9* rightTexture = nullptr;
    std::uintptr_t leftD3D9SharedHandle = 0u;
    std::uintptr_t rightD3D9SharedHandle = 0u;
    std::uint64_t resourceSetId = 0u;
};

class Win32Producer final
{
public:
    Win32Producer() noexcept;
    ~Win32Producer() noexcept;

    Win32Producer(const Win32Producer&) = delete;
    Win32Producer& operator=(const Win32Producer&) = delete;

    bool initialize(const Win32ProducerSource& source) noexcept;
    void reset() noexcept;
    bool ready() const noexcept;
    Win32ProducerFailure failure() const noexcept;
    ProducerResources resources() const noexcept;
    ProducerPublication produce(
        const ProducerFrameIdentity& identity) noexcept;

private:
    struct Implementation;
    Implementation* mImplementation = nullptr;
    Win32ProducerFailure mFailure = Win32ProducerFailure::None;
};
}
