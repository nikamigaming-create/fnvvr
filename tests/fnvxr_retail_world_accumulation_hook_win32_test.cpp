#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "fnvxr_retail_world_accumulation_hook_win32.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace
{
using namespace fnvxr::engine;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
}

int main()
{
    try
    {
#if !defined(_WIN32) || !defined(_M_IX86)
        static_assert(!RetailWorldAccumulationHookWin32MemoryAvailable);
        RetailWorldAccumulationHookWin32Memory unavailable;
        require(
            !unavailable.initialize(static_cast<std::uint32_t>(
                WorldRenderAddress)),
            "non-x86 backend unexpectedly initialized");
#else
        static_assert(RetailWorldAccumulationHookWin32MemoryAvailable);
        // Do not initialize against a synthetic address: the production
        // backend intentionally demands the exact readable/executable retail
        // function. The x86 branch is covered by the real process bridge.
        RetailWorldAccumulationHookWin32Memory unavailable;
        require(
            !unavailable.initialize(0u),
            "zero world address unexpectedly initialized");
#endif
        std::cout << "retail AccumulateScene Win32 backend fuse passed\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
