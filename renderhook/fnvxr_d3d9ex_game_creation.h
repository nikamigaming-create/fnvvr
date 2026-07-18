#pragma once

#include <cstdint>

namespace fnvxr::d3d9
{
enum class GameD3D9CreationBackend : std::uint8_t
{
    Unavailable,
    LegacyD3D9,
};

enum class GameD3D9BootstrapFailure : std::uint8_t
{
    None,
    ExecutableLeafMismatch,
    LoadedExecutableIdentityMismatch,
    NotWin32Process,
    LegacyCreationUnavailable,
    RuntimeMutationNotDeferred,
    UiCaptureNotDeferred,
};

// D3D9 is requested while NVSE is still loading plugins. Requiring the final
// JIP/ShowOff/function inventory here creates a startup deadlock: Fallout
// cannot create the device that lets loading reach a stable Present loop.
// This narrower evidence authorizes only an ordinary D3D9 bootstrap. Engine
// mutation and UI publication must still wait for full current-process retail
// authority inside the bridge retry path.
struct GameD3D9BootstrapEvidence
{
    bool executableLeafMatched = false;
    bool loadedExecutableIdentityMatched = false;
    bool win32Process = false;
    bool legacyCreationAvailable = false;
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
    if (!evidence.legacyCreationAvailable)
        return { GameD3D9BootstrapFailure::LegacyCreationUnavailable };
    if (!evidence.runtimeMutationDeferredToFullAuthority)
        return { GameD3D9BootstrapFailure::RuntimeMutationNotDeferred };
    if (!evidence.uiCaptureDeferredToAuthorizedBridge)
        return { GameD3D9BootstrapFailure::UiCaptureNotDeferred };
    return { GameD3D9BootstrapFailure::None };
}

// Every route preserves the application's ordinary D3D9 object. D3D9Ex may
// still be used by an out-of-band transport device, but Fallout's enumerator
// and game device must never change as a consequence of enabling VR.
constexpr GameD3D9CreationBackend selectGameD3D9CreationBackend(
    bool legacyCreateAvailable) noexcept
{
    return legacyCreateAvailable
        ? GameD3D9CreationBackend::LegacyD3D9
        : GameD3D9CreationBackend::Unavailable;
}

}
