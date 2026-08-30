#pragma once

namespace fnvxr::host
{
// Product rendering must not introduce a per-eye GPU/CPU synchronization.
// Readback exists only for an explicitly requested diagnostic consumer.
[[nodiscard]] constexpr bool eyeReadbackRequired(
    bool mirrorCaptureRequested,
    bool pixelVerificationRequested) noexcept
{
    return mirrorCaptureRequested || pixelVerificationRequested;
}
}
