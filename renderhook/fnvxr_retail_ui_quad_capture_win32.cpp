#define WIN32_LEAN_AND_MEAN

#include "fnvxr_retail_ui_quad_capture_win32.h"

#include <cstring>

namespace fnvxr::d3d9
{
void* volatile RetailUiQuadPresentHookWin32::sActiveHook = nullptr;

namespace
{
template <typename Function>
Function functionFromVtableEntry(void* entry) noexcept
{
    Function function = nullptr;
    static_assert(sizeof(function) == sizeof(entry));
    std::memcpy(&function, &entry, sizeof(function));
    return function;
}

void* vtableEntryFromFunction(RetailD3D9PresentFunction function) noexcept
{
    void* entry = nullptr;
    static_assert(sizeof(function) == sizeof(entry));
    std::memcpy(&entry, &function, sizeof(entry));
    return entry;
}
}

bool RetailUiQuadPresentHookWin32::initializeAuthorizedDevice(
    IDirect3DDevice9* device,
    const RetailUiQuadCaptureOperations& operations) noexcept
{
    if (mInstalled)
        return device == mDevice && ready();
    if (!device)
    {
        mFailure = RetailUiQuadPresentHookFailure::InvalidDevice;
        return false;
    }
    if (!retailUiQuadCaptureOperationsComplete(operations))
    {
        mFailure = RetailUiQuadPresentHookFailure::OperationsIncomplete;
        return false;
    }
    if (InterlockedCompareExchangePointer(
            &sActiveHook,
            this,
            nullptr)
        != nullptr)
    {
        mFailure = RetailUiQuadPresentHookFailure::AnotherHookActive;
        return false;
    }

    auto*** deviceVtableAddress = reinterpret_cast<void***>(device);
    void** originalVtable = deviceVtableAddress
        ? *deviceVtableAddress
        : nullptr;
    if (!originalVtable)
    {
        InterlockedCompareExchangePointer(&sActiveHook, nullptr, this);
        mFailure = RetailUiQuadPresentHookFailure::InvalidVtable;
        return false;
    }
    RetailD3D9PresentFunction originalPresent =
        functionFromVtableEntry<RetailD3D9PresentFunction>(
            originalVtable[RetailD3D9PresentVtableSlot]);
    if (!originalPresent || originalPresent == &presentAdapter)
    {
        InterlockedCompareExchangePointer(&sActiveHook, nullptr, this);
        mFailure = RetailUiQuadPresentHookFailure::InvalidVtable;
        return false;
    }

    constexpr SIZE_T vtableBytes =
        RetailD3D9ExDeviceMethodCount * sizeof(void*);
    void** privateVtable = static_cast<void**>(VirtualAlloc(
        nullptr,
        vtableBytes,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE));
    if (!privateVtable)
    {
        InterlockedCompareExchangePointer(&sActiveHook, nullptr, this);
        mFailure = RetailUiQuadPresentHookFailure::VtableAllocationFailed;
        return false;
    }
    std::memcpy(privateVtable, originalVtable, vtableBytes);
    privateVtable[RetailD3D9PresentVtableSlot] =
        vtableEntryFromFunction(&presentAdapter);
    DWORD oldProtection = 0u;
    if (!VirtualProtect(
            privateVtable,
            vtableBytes,
            PAGE_READONLY,
            &oldProtection))
    {
        VirtualFree(privateVtable, 0u, MEM_RELEASE);
        InterlockedCompareExchangePointer(&sActiveHook, nullptr, this);
        mFailure = RetailUiQuadPresentHookFailure::VtableProtectionFailed;
        return false;
    }

    mDevice = device;
    mOriginalVtable = originalVtable;
    mPrivateVtable = privateVtable;
    mOriginalPresent = originalPresent;
    if (!mCapture.initialize(operations))
    {
        VirtualFree(privateVtable, 0u, MEM_RELEASE);
        mDevice = nullptr;
        mOriginalVtable = nullptr;
        mPrivateVtable = nullptr;
        mOriginalPresent = nullptr;
        InterlockedCompareExchangePointer(&sActiveHook, nullptr, this);
        mFailure = RetailUiQuadPresentHookFailure::OperationsIncomplete;
        return false;
    }

    void* observed = InterlockedCompareExchangePointer(
        reinterpret_cast<void* volatile*>(deviceVtableAddress),
        privateVtable,
        originalVtable);
    if (observed != originalVtable)
    {
        VirtualFree(privateVtable, 0u, MEM_RELEASE);
        mDevice = nullptr;
        mOriginalVtable = nullptr;
        mPrivateVtable = nullptr;
        mOriginalPresent = nullptr;
        InterlockedCompareExchangePointer(&sActiveHook, nullptr, this);
        mFailure = RetailUiQuadPresentHookFailure::VtableSwapFailed;
        return false;
    }

    mInstalled = true;
    mFailure = RetailUiQuadPresentHookFailure::None;
    return true;
}

bool RetailUiQuadPresentHookWin32::detachWhileDeviceAlive() noexcept
{
    if (!mInstalled || !mDevice || !mOriginalVtable || !mPrivateVtable)
        return false;
    auto*** deviceVtableAddress = reinterpret_cast<void***>(mDevice);
    void* observed = InterlockedCompareExchangePointer(
        reinterpret_cast<void* volatile*>(deviceVtableAddress),
        mOriginalVtable,
        mPrivateVtable);
    if (observed != mPrivateVtable)
        return false;
    InterlockedCompareExchangePointer(&sActiveHook, nullptr, this);
    VirtualFree(mPrivateVtable, 0u, MEM_RELEASE);
    mDevice = nullptr;
    mOriginalVtable = nullptr;
    mPrivateVtable = nullptr;
    mOriginalPresent = nullptr;
    mInstalled = false;
    mFailure = RetailUiQuadPresentHookFailure::InvalidDevice;
    return true;
}

bool RetailUiQuadPresentHookWin32::ready() const noexcept
{
    return mInstalled
        && mFailure == RetailUiQuadPresentHookFailure::None
        && mDevice
        && mOriginalVtable
        && mPrivateVtable
        && mOriginalPresent
        && mCapture.ready()
        && InterlockedCompareExchangePointer(
                &sActiveHook,
                nullptr,
                nullptr)
            == this;
}

IDirect3DDevice9* RetailUiQuadPresentHookWin32::device() const noexcept
{
    return mDevice;
}

RetailUiQuadPresentHookFailure RetailUiQuadPresentHookWin32::failure() const
    noexcept
{
    return mFailure;
}

RetailUiQuadCaptureFailure RetailUiQuadPresentHookWin32::captureFailure()
    const noexcept
{
    return mCapture.failure();
}

HRESULT WINAPI RetailUiQuadPresentHookWin32::presentAdapter(
    IDirect3DDevice9* device,
    const RECT* sourceRect,
    const RECT* destinationRect,
    HWND destinationWindowOverride,
    const RGNDATA* dirtyRegion) noexcept
{
    auto* active = static_cast<RetailUiQuadPresentHookWin32*>(
        InterlockedCompareExchangePointer(
            &sActiveHook,
            nullptr,
            nullptr));
    if (!active || !active->mOriginalPresent)
        return D3DERR_INVALIDCALL;
    return active->forwardPresent(
        device,
        sourceRect,
        destinationRect,
        destinationWindowOverride,
        dirtyRegion);
}

HRESULT RetailUiQuadPresentHookWin32::forwardPresent(
    IDirect3DDevice9* device,
    const RECT* sourceRect,
    const RECT* destinationRect,
    HWND destinationWindowOverride,
    const RGNDATA* dirtyRegion) noexcept
{
    RetailD3D9PresentFunction original = mOriginalPresent;
    if (!original)
        return D3DERR_INVALIDCALL;
    if (device == mDevice)
        static_cast<void>(mCapture.beforePresent(device));
    return original(
        device,
        sourceRect,
        destinationRect,
        destinationWindowOverride,
        dirtyRegion);
}
}
