#pragma once

#include <cstdint>

namespace fnvxr::engine
{
// Exact arguments captured from the stock Fallout: New Vegas 1.4.0.525
// RenderWorldSceneGraph call sites in two independently loaded processes.
// They are immutable ABI evidence, not configuration values.
inline constexpr std::uint32_t RetailWorldCullerConstructorArgument = 0u;
inline constexpr std::uint32_t RetailWorldAccumulatorConstructorMode = 0x63u;
inline constexpr std::uint32_t RetailWorldAccumulatorBatchRendererCount = 1u;
inline constexpr std::uint32_t RetailWorldAccumulatorMaximumPassCount = 0x2F7u;
inline constexpr std::uint32_t RetailWorldRenderContext = 1u;
}
