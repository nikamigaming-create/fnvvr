#pragma once

#include <cstdint>

namespace fnvxr::safety
{
// xNVSE's packed runtime identifiers for the two 1.4.0.525 executables. The
// project has evidence only for the standard retail build, not the no-gore
// variant, so they are deliberately not treated as interchangeable.
inline constexpr std::uint32_t SupportedRuntimeVersion = 0x040020D0u;
inline constexpr std::uint32_t SupportedRuntimeVersionNoGore = 0x040020D1u;

// This is a source fuse, not a configuration default. It remains false until
// the engine stereo, GPU transport, and authoritative weapon gates all pass.
// An environment variable can request a feature but can never override this.
inline constexpr bool RetailMutationProofComplete = false;

inline bool compatibleNvseRuntime(
    bool isEditor,
    std::uint32_t nvseVersion,
    std::uint32_t runtimeVersion)
{
    return !isEditor
        && nvseVersion != 0
        && runtimeVersion == SupportedRuntimeVersion;
}

inline bool retailMutationAllowed(bool requested)
{
    return RetailMutationProofComplete && requested;
}
}
