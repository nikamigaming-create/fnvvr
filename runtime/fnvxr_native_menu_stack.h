#pragma once

#include <array>
#include <cstdint>

namespace fnvxr::ui
{
// Retail InterfaceManager::menuStack: zero terminates the stack; 1 denotes
// the Pip-Boy. Numeric menu IDs describe types, never their modal ordering.
inline std::uint32_t nativeMenuStackTop(
    const std::array<std::uint32_t, 10>& stack) noexcept
{
    std::uint32_t top = 0;
    for (const auto entry : stack)
    {
        if (entry == 0)
            return top;
        if (entry != 1 && (entry < 1001 || entry > 1084))
            return 0;
        top = entry;
    }
    return top;
}
}
