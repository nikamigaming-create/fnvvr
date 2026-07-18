#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "fnvxr_retail_world_hook_win32.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace fnvxr::engine
{
struct RetailWorldHookLeaseTestAuthority
{
    static RetailWorldHookAuthorization issue(
        std::uint32_t worldAddress) noexcept
    {
        return RetailWorldHookAuthorization(worldAddress);
    }
};
}

namespace
{
using namespace fnvxr::engine;

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

struct DispatchCapture
{
    std::size_t count = 0u;
    RetailWorldHookDispatchFrame frame {};

    static void capture(
        void* opaque,
        const RetailWorldHookDispatchFrame& frame) noexcept
    {
        auto& self = *static_cast<DispatchCapture*>(opaque);
        ++self.count;
        self.frame = frame;
    }
};

void WINAPI inertAdapterForAddressOnly()
{
}
}

int main()
{
#if !defined(_M_IX86)
    static_assert(!RetailWorldHookWin32MemoryAvailable);
    RetailWorldHookWin32Memory unavailable;
    require(!unavailable.initialize(WorldRenderAddress),
        "non-x86 builds must not expose the x86 writer");
    require(!retailWorldHookMemoryOperationsComplete(
            unavailable.operations()),
        "non-x86 builds returned live memory operations");
    std::cout << "Win32 world-hook memory adapter is inert off x86\n";
    return EXIT_SUCCESS;
#else
    static_assert(RetailWorldHookWin32MemoryAvailable);

    void* body = VirtualAlloc(
        nullptr,
        RetailWorldFunctionByteCount,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE);
    require(body != nullptr, "could not allocate the synthetic world body");
    const std::uintptr_t bodyAddress = reinterpret_cast<std::uintptr_t>(body);
    require(bodyAddress != 0u
            && bodyAddress
                <= (std::numeric_limits<std::uint32_t>::max)(),
        "the synthetic x86 body is not addressable by the retail ABI");

    auto* bytes = static_cast<std::uint8_t*>(body);
    std::fill_n(
        bytes,
        RetailWorldFunctionByteCount,
        static_cast<std::uint8_t>(0xCCu));
    std::copy(
        RetailWorldHookStolenBytes.begin(),
        RetailWorldHookStolenBytes.end(),
        bytes);
    std::copy(
        RetailWorldReturnBytes.begin(),
        RetailWorldReturnBytes.end(),
        bytes + RetailWorldReturnInstructionOffset);
    DWORD previous = 0u;
    require(VirtualProtect(
            body,
            RetailWorldFunctionByteCount,
            PAGE_EXECUTE_READ,
            &previous) != FALSE,
        "could not seal the synthetic world body read/execute");

    const std::uint32_t worldAddress = static_cast<std::uint32_t>(bodyAddress);
    RetailWorldHookWin32Memory memory;
    require(memory.initialize(worldAddress),
        "the exact current-process executable body was rejected");
    const RetailWorldHookMemoryOperations operations = memory.operations();
    require(retailWorldHookMemoryOperationsComplete(operations),
        "the initialized current-process operation table is incomplete");

    std::array<std::uint8_t, RetailWorldHookPatchByteCount> outside {};
    require(!operations.read(
            operations.context,
            worldAddress - 1u,
            outside.data(),
            outside.size()),
        "the adapter read outside its authorized world body");
    require(!operations.write(
            operations.context,
            worldAddress,
            RetailWorldHookStolenBytes.data(),
            RetailWorldHookStolenBytes.size()),
        "the adapter wrote the world entry without a protection lease");

    DispatchCapture dispatch;
    const RetailWorldHookAuthorization authorization =
        RetailWorldHookLeaseTestAuthority::issue(worldAddress);
    const std::uintptr_t adapterAddress = reinterpret_cast<std::uintptr_t>(
        &inertAdapterForAddressOnly);
    require(adapterAddress != 0u
            && adapterAddress
                <= (std::numeric_limits<std::uint32_t>::max)(),
        "the synthetic x86 adapter address is invalid");

    RetailWorldHookInstallResult installed = installRetailWorldHook(
        authorization,
        operations,
        { &dispatch, &DispatchCapture::capture },
        worldAddress,
        adapterAddress);
    require(installed.complete(),
        "the real current-process memory transaction did not install");
    require(!std::equal(
            RetailWorldHookStolenBytes.begin(),
            RetailWorldHookStolenBytes.end(),
            bytes),
        "the entry patch was not committed");
    require(installed.lease.dispatchFromFastcallAdapter(
            reinterpret_cast<void*>(0x1010u),
            reinterpret_cast<void*>(0x2020u),
            0x3030u,
            1u,
            0x4040u),
        "the installed lease rejected its dispatch seam");
    require(dispatch.count == 1u
            && dispatch.frame.retailThis
                == reinterpret_cast<void*>(0x1010u)
            && dispatch.frame.arguments.sharedRenderObjectAddress == 0x2020u
            && dispatch.frame.originalTrampolineAddress
                == installed.lease.trampolineAddress(),
        "the dispatch seam changed retail arguments or trampoline identity");

    const RetailWorldHookUninstallResult removed = installed.lease.uninstall();
    require(removed.complete
            && removed.failure == RetailWorldHookUninstallFailure::None,
        "the real current-process transaction did not uninstall");
    require(std::equal(
            RetailWorldHookStolenBytes.begin(),
            RetailWorldHookStolenBytes.end(),
            bytes),
        "uninstall did not restore the exact original entry");

    MEMORY_BASIC_INFORMATION information {};
    require(VirtualQuery(body, &information, sizeof(information))
                == sizeof(information)
            && (information.Protect & 0xFFu) == PAGE_EXECUTE_READ,
        "uninstall did not restore the original page protection");
    require(VirtualFree(body, 0u, MEM_RELEASE) != FALSE,
        "could not release the synthetic world body");

    std::cout << "Win32 world-hook memory adapter transaction passed\n";
    return EXIT_SUCCESS;
#endif
}
