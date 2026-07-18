#pragma once

#include "fnvxr_engine_capability.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace fnvxr::engine
{
// Exact loaded PE identity for the supported FalloutNV.exe 1.4.0.525 target.
// Steam encrypts parts of the executable on disk, so function hashes below
// must be checked against mapped executable memory after the retail loader has
// decoded it. These constants are evidence, never configuration defaults.
inline constexpr std::uint32_t SupportedPeTimeDateStamp = 0x4E0D50EDu;
inline constexpr std::uint32_t SupportedPeChecksum = 0x00FCF93Eu;
inline constexpr std::uint32_t SupportedSizeOfImage = 0x0107B000u;
inline constexpr std::uint64_t SupportedFileBytes = 16549704u;
inline constexpr char SupportedFileSha256[] =
    "518C87F58A6C4D9826E9EF8FBB7F4213882FA70822675610D45AEA2464502A57";

struct LoadedExecutableIdentity
{
    std::uint32_t timeDateStamp = 0;
    std::uint32_t checksum = 0;
    std::uint32_t sizeOfImage = 0;
};

inline bool matchesLoadedExecutableIdentity(const LoadedExecutableIdentity& value)
{
    return value.timeDateStamp == SupportedPeTimeDateStamp
        && value.checksum == SupportedPeChecksum
        && value.sizeOfImage == SupportedSizeOfImage;
}

struct Sha256Digest
{
    std::array<std::uint8_t, 32> bytes {};
    bool valid = false;
};

constexpr int hexNibble(char value)
{
    return value >= '0' && value <= '9'
        ? static_cast<std::uint8_t>(value - '0')
        : value >= 'A' && value <= 'F'
            ? static_cast<std::uint8_t>(value - 'A' + 10)
            : value >= 'a' && value <= 'f'
                ? static_cast<std::uint8_t>(value - 'a' + 10)
                : -1;
}

template <std::size_t Size>
constexpr Sha256Digest sha256FromHex(const char (&text)[Size])
{
    static_assert(Size == 65, "SHA-256 text must contain exactly 64 hex digits");
    Sha256Digest result {};
    result.valid = text[64] == '\0';
    for (std::size_t index = 0; index < result.bytes.size(); ++index)
    {
        const int high = hexNibble(text[index * 2]);
        const int low = hexNibble(text[index * 2 + 1]);
        if (high < 0 || low < 0)
        {
            result.valid = false;
            result.bytes[index] = 0;
        }
        else
        {
            result.bytes[index] = static_cast<std::uint8_t>((high << 4) | low);
        }
    }
    return result;
}

inline bool digestMatches(
    const Sha256Digest& expected,
    const std::uint8_t* actual,
    std::size_t actualBytes)
{
    if (!expected.valid || !actual || actualBytes != expected.bytes.size())
        return false;

    std::uint8_t difference = 0;
    for (std::size_t index = 0; index < expected.bytes.size(); ++index)
        difference |= static_cast<std::uint8_t>(expected.bytes[index] ^ actual[index]);
    return difference == 0;
}

struct LoadedFunctionManifestEntry
{
    const char* name = nullptr;
    std::uintptr_t preferredAddress = 0;
    std::size_t byteCount = 0;
    Sha256Digest sha256 {};
};

// Every range is one complete instruction-aligned function body extracted
// from two independent loaded-memory dumps. Both dumps produced identical
// bytes. Do not widen a callable entry to cover adjacent alignment bytes or
// functions: a matching cluster is not ABI evidence for the named function.
inline constexpr std::array<LoadedFunctionManifestEntry, 13> RetailEngineManifest {{
    {
        "DoRenderFrame",
        WholeFrameRenderAddress,
        185u,
        sha256FromHex("A8CC17DF032791EAF2C6D898898827F72A1EB63926B1423E7DDB395395C3D0D5"),
    },
    {
        "RenderWorldSceneGraph",
        WorldRenderAddress,
        4698u,
        sha256FromHex("D2355FF1593FD9D843C0C61FE95205C1B2C4F1FB6D560499B6FA4EE9C312AEAE"),
    },
    {
        "RenderFirstPerson",
        FirstPersonRenderAddress,
        3361u,
        sha256FromHex("7F734D69C1C74C2099BE684FB4FE682BF84B3F75A108F109CCF1DF74EF9D55F2"),
    },
    {
        "PlayerCharacter::UpdateCamera",
        0x0094AE40u,
        5433u,
        sha256FromHex("6BB45EDC72162B703610CBF425DB949BE060C516F2187A84D8420B2224FB35B5"),
    },
    {
        "NiCamera::SetViewFrustum",
        0x00A6FAF0u,
        148u,
        sha256FromHex("84AB9FC5D235706FD6B24BB203F89F0C63B8D02EA1F3F1AFA906FEE6FBE8489E"),
    },
    {
        "NiCamera::UpdateWorldToCamera",
        0x00A70BA0u,
        947u,
        sha256FromHex("F41A0EAAC2E0573BC25B8A7A5E3799601B29E5D298D68B1C9E7CA002124D9B2D"),
    },
    {
        "NiAccumulator::AddVisibleArray",
        0x00A9B790u,
        189u,
        sha256FromHex("A929F2C8289B45EC15F5A16E88A5052D5E1C3F1348880E07C66E223DCB592843"),
    },
    {
        "FinalizeAccumulator",
        0x00B6B930u,
        14u,
        sha256FromHex("C3CA665ECDDCAC09D42561F945B34450340B1354CC2054BF45599426581BB792"),
    },
    {
        "RenderAccumulatorWithoutFinalize",
        0x00B6BA20u,
        74u,
        sha256FromHex("D493A62CA76EBEF84C25CDFA7B1BA4C10966D1EEED2D0FA0EA4E2D583880C9E4"),
    },
    {
        "AccumulateScene",
        0x00B6BEE0u,
        209u,
        sha256FromHex("45BF64E849FF26829D43BB52866BF51C412DF95F12F3177159A2BEC2D5838A5A"),
    },
    {
        "RenderAndFinalizeAccumulator",
        0x00B6C0D0u,
        84u,
        sha256FromHex("1BB66CF7419B4FBF086CA3D2F36BE79F17EEA3D04516C7E4A0A3E4EEAA99119B"),
    },
    {
        "BSCullingProcess::ProcessAlt",
        0x00C4F070u,
        343u,
        sha256FromHex("213054747E94294B95DABD125B34D430DD0D12137396C97EAE269FCA7A0E301F"),
    },
    {
        "NiAccumulator::SetCamera",
        0x00D47A40u,
        10u,
        sha256FromHex("B35CAAC991EB0D06657C3FBBA49C85DE5F83E8CF4DE0B07B70FA4B90CB5770FD"),
    },
}};
}
