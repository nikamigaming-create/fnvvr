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

    mDevice = device;
    mOriginalVtable = originalVtable;
    mPresentEntry = &originalVtable[RetailD3D9PresentVtableSlot];
    mOriginalPresent = originalPresent;
    if (!mCapture.initialize(operations))
    {
        mDevice = nullptr;
        mOriginalVtable = nullptr;
        mPresentEntry = nullptr;
        mOriginalPresent = nullptr;
        InterlockedCompareExchangePointer(&sActiveHook, nullptr, this);
        mFailure = RetailUiQuadPresentHookFailure::OperationsIncomplete;
        return false;
    }

    // Do not replace the concrete D3D9 device's vptr. Windows' CD3DHal uses
    // implementation-private entries beyond the public IDirect3DDevice9Ex
    // interface. A public-length clone truncates that private tail and turns
    // internal dispatch into an out-of-bounds call. Lease only the documented
    // Present entry in the native table and leave the concrete vptr intact.
    DWORD oldProtection = 0u;
    if (!VirtualProtect(
            mPresentEntry,
            sizeof(*mPresentEntry),
            PAGE_EXECUTE_READWRITE,
            &oldProtection))
    {
        mDevice = nullptr;
        mOriginalVtable = nullptr;
        mPresentEntry = nullptr;
        mOriginalPresent = nullptr;
        InterlockedCompareExchangePointer(&sActiveHook, nullptr, this);
        mFailure = RetailUiQuadPresentHookFailure::VtableProtectionFailed;
        return false;
    }
    void* const adapterEntry = vtableEntryFromFunction(&presentAdapter);
    void* const originalEntry = vtableEntryFromFunction(originalPresent);
    void* observed = InterlockedCompareExchangePointer(
        reinterpret_cast<void* volatile*>(mPresentEntry),
        adapterEntry,
        originalEntry);
    DWORD ignoredProtection = 0u;
    const bool protectionRestored = VirtualProtect(
        mPresentEntry,
        sizeof(*mPresentEntry),
        oldProtection,
        &ignoredProtection) != FALSE;
    if (observed != originalEntry || !protectionRestored)
    {
        if (observed == originalEntry)
        {
            DWORD rollbackProtection = 0u;
            if (VirtualProtect(
                    mPresentEntry,
                    sizeof(*mPresentEntry),
                    PAGE_EXECUTE_READWRITE,
                    &rollbackProtection))
            {
                InterlockedCompareExchangePointer(
                    reinterpret_cast<void* volatile*>(mPresentEntry),
                    originalEntry,
                    adapterEntry);
                DWORD rollbackIgnored = 0u;
                VirtualProtect(
                    mPresentEntry,
                    sizeof(*mPresentEntry),
                    oldProtection,
                    &rollbackIgnored);
            }
        }
        mDevice = nullptr;
        mOriginalVtable = nullptr;
        mPresentEntry = nullptr;
        mOriginalPresent = nullptr;
        InterlockedCompareExchangePointer(&sActiveHook, nullptr, this);
        mFailure = protectionRestored
            ? RetailUiQuadPresentHookFailure::PresentSlotLeaseFailed
            : RetailUiQuadPresentHookFailure::VtableProtectionFailed;
        return false;
    }

    mInstalled = true;
    mFailure = RetailUiQuadPresentHookFailure::None;
    return true;
}

bool RetailUiQuadPresentHookWin32::detachWhileDeviceAlive() noexcept
{
    if (!mInstalled || !mDevice || !mOriginalVtable || !mPresentEntry)
        return false;
    auto*** deviceVtableAddress = reinterpret_cast<void***>(mDevice);
    if (!deviceVtableAddress || *deviceVtableAddress != mOriginalVtable)
        return false;
    DWORD oldProtection = 0u;
    if (!VirtualProtect(
            mPresentEntry,
            sizeof(*mPresentEntry),
            PAGE_EXECUTE_READWRITE,
            &oldProtection))
    {
        mFailure = RetailUiQuadPresentHookFailure::VtableProtectionFailed;
        return false;
    }
    void* const adapterEntry = vtableEntryFromFunction(&presentAdapter);
    void* const originalEntry = vtableEntryFromFunction(mOriginalPresent);
    void* observed = InterlockedCompareExchangePointer(
        reinterpret_cast<void* volatile*>(mPresentEntry),
        originalEntry,
        adapterEntry);
    DWORD ignoredProtection = 0u;
    const bool protectionRestored = VirtualProtect(
        mPresentEntry,
        sizeof(*mPresentEntry),
        oldProtection,
        &ignoredProtection) != FALSE;
    if (observed != adapterEntry || !protectionRestored)
    {
        mFailure = protectionRestored
            ? RetailUiQuadPresentHookFailure::PresentSlotLeaseFailed
            : RetailUiQuadPresentHookFailure::VtableProtectionFailed;
        return false;
    }
    InterlockedCompareExchangePointer(&sActiveHook, nullptr, this);
    mDevice = nullptr;
    mOriginalVtable = nullptr;
    mPresentEntry = nullptr;
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
        && mPresentEntry
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
