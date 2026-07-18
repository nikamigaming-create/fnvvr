#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d9.h>

#include "fnvxr_retail_ui_quad_capture.h"

#include <cstddef>
#include <cstdint>

namespace fnvxr::d3d9
{
inline constexpr std::size_t RetailD3D9PresentVtableSlot = 17u;
inline constexpr std::size_t RetailD3D9ExDeviceMethodCount = 134u;

enum class RetailUiQuadPresentHookFailure : std::uint8_t
{
    None = 0u,
    InvalidDevice,
    OperationsIncomplete,
    AnotherHookActive,
    InvalidVtable,
    VtableAllocationFailed,
    VtableProtectionFailed,
    VtableSwapFailed,
};

using RetailD3D9PresentFunction = HRESULT(WINAPI*)(
    IDirect3DDevice9*,
    const RECT*,
    const RECT*,
    HWND,
    const RGNDATA*);

class RetailUiQuadPresentHookWin32 final
{
public:
    RetailUiQuadPresentHookWin32() noexcept = default;
    RetailUiQuadPresentHookWin32(
        const RetailUiQuadPresentHookWin32&) = delete;
    RetailUiQuadPresentHookWin32& operator=(
        const RetailUiQuadPresentHookWin32&) = delete;

    bool initializeAuthorizedDevice(
        IDirect3DDevice9* device,
        const RetailUiQuadCaptureOperations& operations) noexcept;

    // The production object intentionally has process lifetime. Tests may
    // detach only while their fake/real device object is still alive.
    bool detachWhileDeviceAlive() noexcept;

    bool ready() const noexcept;
    IDirect3DDevice9* device() const noexcept;
    RetailUiQuadPresentHookFailure failure() const noexcept;
    RetailUiQuadCaptureFailure captureFailure() const noexcept;

private:
    static HRESULT WINAPI presentAdapter(
        IDirect3DDevice9* device,
        const RECT* sourceRect,
        const RECT* destinationRect,
        HWND destinationWindowOverride,
        const RGNDATA* dirtyRegion) noexcept;

    HRESULT forwardPresent(
        IDirect3DDevice9* device,
        const RECT* sourceRect,
        const RECT* destinationRect,
        HWND destinationWindowOverride,
        const RGNDATA* dirtyRegion) noexcept;

    static void* volatile sActiveHook;

    RetailUiQuadCaptureController mCapture {};
    IDirect3DDevice9* mDevice = nullptr;
    void** mOriginalVtable = nullptr;
    void** mPrivateVtable = nullptr;
    RetailD3D9PresentFunction mOriginalPresent = nullptr;
    RetailUiQuadPresentHookFailure mFailure =
        RetailUiQuadPresentHookFailure::InvalidDevice;
    bool mInstalled = false;
};
}
