#pragma once

#include <cstdint>

namespace fnvxr::d3d9
{
enum class GameD3D9CreationBackend : std::uint8_t
{
    Unavailable,
    LegacyD3D9,
    ExBackedD3D9,
};

enum class GameD3D9BootstrapFailure : std::uint8_t
{
    None,
    ExecutableLeafMismatch,
    LoadedExecutableIdentityMismatch,
    NotWin32Process,
    ExCreationUnavailable,
    RuntimeMutationNotDeferred,
    UiCaptureNotDeferred,
};

// D3D9 is requested while NVSE is still loading plugins. Requiring the final
// JIP/ShowOff/function inventory here creates a startup deadlock: Fallout
// cannot create the device that lets loading reach a stable Present loop.
// This narrower evidence authorizes only an Ex-backed D3D bootstrap. Engine
// mutation and UI publication must still wait for full current-process retail
// authority inside the bridge retry path.
struct GameD3D9BootstrapEvidence
{
    bool executableLeafMatched = false;
    bool loadedExecutableIdentityMatched = false;
    bool win32Process = false;
    bool exCreationAvailable = false;
    bool runtimeMutationDeferredToFullAuthority = false;
    bool uiCaptureDeferredToAuthorizedBridge = false;
};

struct GameD3D9BootstrapDecision
{
    GameD3D9BootstrapFailure failure =
        GameD3D9BootstrapFailure::ExecutableLeafMismatch;

    constexpr bool authorized() const noexcept
    {
        return failure == GameD3D9BootstrapFailure::None;
    }
};

constexpr GameD3D9BootstrapDecision assessGameD3D9Bootstrap(
    const GameD3D9BootstrapEvidence& evidence) noexcept
{
    if (!evidence.executableLeafMatched)
        return { GameD3D9BootstrapFailure::ExecutableLeafMismatch };
    if (!evidence.loadedExecutableIdentityMatched)
    {
        return {
            GameD3D9BootstrapFailure::LoadedExecutableIdentityMismatch,
        };
    }
    if (!evidence.win32Process)
        return { GameD3D9BootstrapFailure::NotWin32Process };
    if (!evidence.exCreationAvailable)
        return { GameD3D9BootstrapFailure::ExCreationUnavailable };
    if (!evidence.runtimeMutationDeferredToFullAuthority)
        return { GameD3D9BootstrapFailure::RuntimeMutationNotDeferred };
    if (!evidence.uiCaptureDeferredToAuthorizedBridge)
        return { GameD3D9BootstrapFailure::UiCaptureNotDeferred };
    return { GameD3D9BootstrapFailure::None };
}

// A forwarding-only artifact preserves the application's ordinary D3D9
// object. An authorized production artifact instead requires an Ex enumerator
// so that its base CreateDevice call can be fulfilled with CreateDeviceEx.
// There is deliberately no production fallback to a non-Ex device because
// that device cannot satisfy the native cross-API color transport contract.
constexpr GameD3D9CreationBackend selectGameD3D9CreationBackend(
    bool productionAuthorized,
    bool legacyCreateAvailable,
    bool exCreateAvailable) noexcept
{
    if (productionAuthorized)
    {
        return exCreateAvailable
            ? GameD3D9CreationBackend::ExBackedD3D9
            : GameD3D9CreationBackend::Unavailable;
    }
    return legacyCreateAvailable
        ? GameD3D9CreationBackend::LegacyD3D9
        : GameD3D9CreationBackend::Unavailable;
}

constexpr bool creationBackendRequiresDeviceEx(
    GameD3D9CreationBackend backend) noexcept
{
    return backend == GameD3D9CreationBackend::ExBackedD3D9;
}

struct GameD3D9ExDisplayModeFields
{
    bool required = false;
    std::uint32_t structureBytes = 0u;
    std::uint32_t width = 0u;
    std::uint32_t height = 0u;
    std::uint32_t refreshRate = 0u;
    std::uint32_t format = 0u;
    std::uint32_t scanLineOrdering = 0u;
};

constexpr GameD3D9ExDisplayModeFields makeGameD3D9ExDisplayModeFields(
    bool windowed,
    std::uint32_t structureBytes,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t refreshRate,
    std::uint32_t format,
    std::uint32_t unknownScanLineOrdering) noexcept
{
    if (windowed)
        return {};
    return {
        true,
        structureBytes,
        width,
        height,
        refreshRate,
        format,
        unknownScanLineOrdering,
    };
}
}
