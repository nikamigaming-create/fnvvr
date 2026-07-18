#include "fnvxr_retail_engine_manifest.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main()
{
    using namespace fnvxr::engine;

    require(
        matchesLoadedExecutableIdentity({
            SupportedPeTimeDateStamp,
            SupportedPeChecksum,
            SupportedSizeOfImage,
        }),
        "the exact supported loaded executable identity must pass");
    require(
        !matchesLoadedExecutableIdentity({
            SupportedPeTimeDateStamp + 1u,
            SupportedPeChecksum,
            SupportedSizeOfImage,
        }),
        "a timestamp mismatch must fail closed");
    require(
        !matchesLoadedExecutableIdentity({
            SupportedPeTimeDateStamp,
            SupportedPeChecksum ^ 1u,
            SupportedSizeOfImage,
        }),
        "a checksum mismatch must fail closed");
    require(
        !matchesLoadedExecutableIdentity({
            SupportedPeTimeDateStamp,
            SupportedPeChecksum,
            SupportedSizeOfImage - 0x1000u,
        }),
        "an image-size mismatch must fail closed");

    require(!RetailEngineManifest.empty(), "the loaded-memory manifest must not be empty");
    require(
        std::all_of(
            RetailEngineManifest.begin(),
            RetailEngineManifest.end(),
            [](const LoadedFunctionManifestEntry& entry) {
                return entry.name != nullptr
                    && entry.name[0] != '\0'
                    && entry.sha256.valid
                    && entry.preferredAddress >= SupportedImageBase
                    && entry.byteCount != 0
                    && entry.preferredAddress + entry.byteCount
                        <= SupportedImageBase + SupportedSizeOfImage;
            }),
        "every manifest range must be named, nonempty, and inside the image");

    for (std::size_t left = 0; left < RetailEngineManifest.size(); ++left)
    {
        const LoadedFunctionManifestEntry& entry = RetailEngineManifest[left];
        require(
            digestMatches(entry.sha256, entry.sha256.bytes.data(), entry.sha256.bytes.size()),
            "a manifest digest must match its exact bytes");

        auto changed = entry.sha256.bytes;
        changed.front() ^= 0x01u;
        require(
            !digestMatches(entry.sha256, changed.data(), changed.size()),
            "a one-bit digest mismatch must fail closed");
        require(
            !digestMatches(entry.sha256, entry.sha256.bytes.data(), entry.sha256.bytes.size() - 1u),
            "a truncated digest must fail closed");

        const std::uintptr_t leftEnd = entry.preferredAddress + entry.byteCount;
        for (std::size_t right = left + 1; right < RetailEngineManifest.size(); ++right)
        {
            const LoadedFunctionManifestEntry& other = RetailEngineManifest[right];
            const std::uintptr_t rightEnd = other.preferredAddress + other.byteCount;
            require(
                leftEnd <= other.preferredAddress || rightEnd <= entry.preferredAddress,
                "manifest function ranges must not overlap");
        }
    }

    require(
        RetailEngineManifest[0].preferredAddress == WholeFrameRenderAddress,
        "the manifest must anchor the verified whole-frame boundary");
    require(
        RetailEngineManifest[1].preferredAddress == WorldRenderAddress,
        "the manifest must anchor the verified world-render boundary");

    const auto addVisibleArray = std::find_if(
        RetailEngineManifest.begin(),
        RetailEngineManifest.end(),
        [](const LoadedFunctionManifestEntry& entry) {
            return std::string_view(entry.name) == "NiAccumulator::AddVisibleArray";
        });
    require(
        addVisibleArray != RetailEngineManifest.end(),
        "the accumulator population ABI must be protected");
    require(
        addVisibleArray->preferredAddress == 0x00A9B790u
            && addVisibleArray->byteCount == 189u
            && addVisibleArray->preferredAddress + addVisibleArray->byteCount
                == 0x00A9B84Du,
        "AddVisibleArray must cover only its callable body, not the adjacent cluster");

    constexpr Sha256Digest malformed = sha256FromHex(
        "G8CC17DF032791EAF2C6D898898827F72A1EB63926B1423E7DDB395395C3D0D5");
    require(!malformed.valid, "invalid SHA-256 hex must be rejected, not decoded as zero");
    require(
        !digestMatches(malformed, malformed.bytes.data(), malformed.bytes.size()),
        "a malformed expected digest must never match runtime bytes");

    std::cout << "retail engine manifest contract passed\n";
    return EXIT_SUCCESS;
}
