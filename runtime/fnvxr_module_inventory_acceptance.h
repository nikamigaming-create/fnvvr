#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace fnvxr::inventory
{
inline constexpr std::uint16_t PeMachineX86 = 0x014Cu;

enum class ModuleOrigin : std::uint8_t
{
    Unknown = 0,
    GameTree,
    SteamRuntime,
    WindowsRuntime,
    NvidiaDriverRuntime
};

template<std::size_t MaximumCharacters>
struct FixedAsciiSnapshot
{
    std::array<char, MaximumCharacters + 1u> bytes {};
    std::size_t length = 0;
    bool captureComplete = false;

    constexpr FixedAsciiSnapshot() noexcept = default;

    constexpr FixedAsciiSnapshot(std::string_view value) noexcept
    {
        capture(value);
    }

    constexpr FixedAsciiSnapshot& operator=(std::string_view value) noexcept
    {
        capture(value);
        return *this;
    }

    constexpr void reset() noexcept
    {
        for (char& value : bytes)
            value = '\0';
        length = 0u;
        captureComplete = false;
    }

    constexpr bool capture(std::string_view value) noexcept
    {
        reset();
        if (value.size() > MaximumCharacters)
            return false;
        for (std::size_t index = 0; index < value.size(); ++index)
        {
            const unsigned char character =
                static_cast<unsigned char>(value[index]);
            if (character < 0x20u || character > 0x7Eu)
                return false;
            bytes[index] = value[index];
        }
        length = value.size();
        bytes[length] = '\0';
        captureComplete = true;
        return true;
    }

    constexpr bool captureNullTerminated(
        const char* source,
        std::size_t availableBytes) noexcept
    {
        reset();
        if (!source || availableBytes == 0u)
            return false;
        for (std::size_t index = 0;
             index < availableBytes && index <= MaximumCharacters;
             ++index)
        {
            if (source[index] == '\0')
                return capture(std::string_view(source, index));
        }
        return false;
    }

    constexpr bool valid() const noexcept
    {
        if (!captureComplete
            || length > MaximumCharacters
            || bytes[length] != '\0')
        {
            return false;
        }
        for (std::size_t index = 0; index < length; ++index)
        {
            const unsigned char character =
                static_cast<unsigned char>(bytes[index]);
            if (character < 0x20u || character > 0x7Eu)
                return false;
        }
        for (std::size_t index = length + 1u; index < bytes.size(); ++index)
        {
            if (bytes[index] != '\0')
                return false;
        }
        return true;
    }

    constexpr std::string_view view() const noexcept
    {
        return valid() ? std::string_view(bytes.data(), length)
                       : std::string_view {};
    }

    constexpr operator std::string_view() const noexcept
    {
        return view();
    }
};

inline constexpr std::size_t ModulePathSnapshotCharacters = 260u;
inline constexpr std::size_t ModuleNameSnapshotCharacters = 260u;
inline constexpr std::size_t Sha256SnapshotCharacters = 64u;

using ModulePathSnapshot =
    FixedAsciiSnapshot<ModulePathSnapshotCharacters>;
using ModuleNameSnapshot =
    FixedAsciiSnapshot<ModuleNameSnapshotCharacters>;
using Sha256Snapshot = FixedAsciiSnapshot<Sha256SnapshotCharacters>;

struct PeIdentity
{
    std::uint16_t machine = 0;
    std::uint32_t timeDateStamp = 0;
    std::uint32_t sizeOfImage = 0;
    std::uint64_t preferredImageBase = 0;
    std::uint64_t fileSize = 0;
    std::string_view sha256 {};
};

struct ObservedPeIdentity
{
    std::uint16_t machine = 0;
    std::uint32_t timeDateStamp = 0;
    std::uint32_t sizeOfImage = 0;
    std::uint64_t preferredImageBase = 0;
    std::uint64_t fileSize = 0;
    Sha256Snapshot sha256 {};
};

constexpr ObservedPeIdentity snapshotPeIdentity(
    const PeIdentity& identity) noexcept
{
    ObservedPeIdentity snapshot {};
    snapshot.machine = identity.machine;
    snapshot.timeDateStamp = identity.timeDateStamp;
    snapshot.sizeOfImage = identity.sizeOfImage;
    snapshot.preferredImageBase = identity.preferredImageBase;
    snapshot.fileSize = identity.fileSize;
    snapshot.sha256.capture(identity.sha256);
    return snapshot;
}

struct ExpectedModule
{
    ModuleOrigin origin = ModuleOrigin::Unknown;
    std::string_view normalizedRelativePath {};
    bool exactIdentityRequired = false;
    PeIdentity exactIdentity {};
};

constexpr ExpectedModule exactGameModule(
    std::string_view path,
    std::uint64_t fileSize,
    std::uint32_t timeDateStamp,
    std::uint32_t sizeOfImage,
    std::uint64_t preferredImageBase,
    std::string_view sha256) noexcept
{
    return {
        ModuleOrigin::GameTree,
        path,
        true,
        { PeMachineX86, timeDateStamp, sizeOfImage, preferredImageBase, fileSize, sha256 }
    };
}

constexpr ExpectedModule observedGameProxy(std::string_view path) noexcept
{
    return { ModuleOrigin::GameTree, path, false, {} };
}

constexpr ExpectedModule signedRuntime(
    ModuleOrigin origin,
    std::string_view path) noexcept
{
    return { origin, path, false, {} };
}

// Exact census-derived allowlist. The two prohibited overlay modules observed
// during the audit (GameOverlayRenderer.dll and nvspcap.dll) are intentionally
// absent. Local D3D/DInput/XInput identities are authorized separately through
// the transparent-proxy evidence below because the audited live files predate
// their source fuses and must never be mistaken for approved transparent builds.
inline constexpr ExpectedModule AllowedModules[] {
    exactGameModule("falloutnv.exe", 16549704u, 0x4E0D50EDu, 0x107B000u, 0x00400000u,
        "518C87F58A6C4D9826E9EF8FBB7F4213882FA70822675610D45AEA2464502A57"),
    exactGameModule("binkw32.dll", 338944u, 0x41E09CAFu, 0x68000u, 0x18000000u,
        "C6C06E2D21F2179F0B7FBE829A4B17A7B371440D2C679253EC86B64D421185BD"),
    observedGameProxy("d3d9.dll"),
    observedGameProxy("dinput8.dll"),
    exactGameModule("libvorbis.dll", 1241528u, 0x481A29C9u, 0x133000u, 0x10000000u,
        "A34F3B4EE26CF093D096C574ABA26C6E9FE0CE3DBFA073F92378FE1DFFA01F0D"),
    exactGameModule("libvorbisfile.dll", 120248u, 0x481A29CAu, 0x1E000u, 0x10000000u,
        "6FC78D8ABF8683A91D25166732085D5F934003150087B3D435BBC19E532A7994"),
    exactGameModule("nvse_1_4.dll", 1385472u, 0x6A14A502u, 0x15D000u, 0x10000000u,
        "8070015BD8BDA83DB203D606BB06F7C86CF04F35EDC21CDF18E49CA6628DC66E"),
    exactGameModule("nvse_steam_loader.dll", 42496u, 0x6A14A4EFu, 0x10000u, 0x10000000u,
        "21C7E4CA55D3F03B378BCACC283452666B68801E09B6F0CFE29072F414833B31"),
    exactGameModule("steam_api.dll", 121984u, 0x4B60A94Bu, 0x1D000u, 0x3B400000u,
        "59ED854645EAA237463EB22F3C5A25F726D7CB2F29440F00A1EC0A4D73D0207A"),
    observedGameProxy("xinput1_3.dll"),
    exactGameModule("data/nvse/plugins/jip_nvse.dll", 502272u, 0x665225A8u, 0x80000u, 0x10000000u,
        "9D2779647ED0CE63043390F47FC978E3234AF8E558DC6CB6BCB231478A2D74D4"),
    exactGameModule("data/nvse/plugins/nvse_fnvxr.dll", 10752u, 0xE126C195u, 0x8000u, 0x10000000u,
        "E394CB96DC1F881E66DF5E11877BCCBA55033FE50A4D407FD70CCB359BB3650D"),
    exactGameModule("data/nvse/plugins/showoffnvse.dll", 1091584u, 0x69C84C9Bu, 0x110000u, 0x10000000u,
        "37CB22C5288FEDD0D57196C8C2F6BBABA5A1DAFD9CE58F14DAC9410DBEE7EF3E"),

    signedRuntime(ModuleOrigin::SteamRuntime, "cserhelper.dll"),
    signedRuntime(ModuleOrigin::SteamRuntime, "steam.dll"),
    signedRuntime(ModuleOrigin::SteamRuntime, "steamclient.dll"),
    signedRuntime(ModuleOrigin::SteamRuntime, "tier0_s.dll"),
    signedRuntime(ModuleOrigin::SteamRuntime, "vstdlib_s.dll"),

    signedRuntime(ModuleOrigin::NvidiaDriverRuntime, "nvdd.inf_amd64_55661cb7fab2ea8e/nvd3dum.dll"),
    signedRuntime(ModuleOrigin::NvidiaDriverRuntime, "nvdd.inf_amd64_55661cb7fab2ea8e/nvgpucomp32.dll"),
    signedRuntime(ModuleOrigin::NvidiaDriverRuntime, "nvdd.inf_amd64_55661cb7fab2ea8e/nvldumd.dll"),
    signedRuntime(ModuleOrigin::NvidiaDriverRuntime, "nvdd.inf_amd64_55661cb7fab2ea8e/nvmemmapstorage.dll"),
    signedRuntime(ModuleOrigin::NvidiaDriverRuntime, "nvdd.inf_amd64_55661cb7fab2ea8e/nvppe.dll"),

    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/advapi32.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/apphelp.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/audioses.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/avrt.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/bcrypt.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/bcryptprimitives.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/cfgmgr32.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/clbcatq.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/combase.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime,
        "winsxs/x86_microsoft.windows.common-controls_6595b64144ccf1df_5.82.26100.8521_none_cf9e08b68ea73d80/comctl32.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/coremessaging.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/coreuicomponents.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/crypt32.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/cryptbase.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/cryptnet.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/cryptsp.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/d3d9.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/d3dref9.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/d3dx9_38.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/dbgcore.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/dbghelp.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/devenum.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/devobj.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/dinput8.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/directxdatabasehelper.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/drvstore.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/dsound.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/dwmapi.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/dxcore.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/gdi32.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/gdi32full.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/gpapi.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/hid.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/imagehlp.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/imm32.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/inputhost.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/iphlpapi.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/kernel.appcore.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/kernel32.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/kernelbase.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/l3codeca.acm"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/mfperfhelper.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/mfplat.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/midimap.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/mmdevapi.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/mp3dmod.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/msacm32.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/msacm32.drv"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/msasn1.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/msctf.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/msdmo.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/msmpeg2adec.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/msvcp_win.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/msvcp140.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/msvcrt.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/mswsock.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/ntdll.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/ntmarta.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/nvapi.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/ole32.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/oleacc.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/oleaut32.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/powrprof.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/profapi.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/psapi.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/qasf.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/quartz.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/resampledmo.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/resourcepolicyclient.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/rpcrt4.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/rsaenh.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/rtworkq.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/sechost.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/secur32.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/setupapi.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/shcore.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/shell32.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/shlwapi.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/sspicli.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/textinputframework.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/ucrtbase.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/uiautomationcore.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/umpdc.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/user32.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/uxtheme.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/vcruntime140.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/version.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/wdmaud.drv"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/wdmaud2.drv"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/win32u.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/windows.storage.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/windows.ui.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/winmm.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/winmmbase.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/wintrust.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/wintypes.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/wldp.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/wmasf.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/wmvcore.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/ws2_32.dll"),
    signedRuntime(ModuleOrigin::WindowsRuntime, "system32/wsock32.dll")
};

inline constexpr std::size_t AllowedModuleCount =
    sizeof(AllowedModules) / sizeof(AllowedModules[0]);
static_assert(AllowedModuleCount == 124u, "census-derived allowlist count changed");

// Acceptance still requires exactly AllowedModuleCount observations. This
// fixed census-builder envelope permits pure acceptance to diagnose known
// prohibited modules in a realistically dirty process without opening an
// unbounded attacker-controlled scan.
inline constexpr std::size_t MaximumDiagnosticModuleCount = 512u;
static_assert(MaximumDiagnosticModuleCount > AllowedModuleCount);

struct ModuleObservation
{
    ModuleOrigin origin = ModuleOrigin::Unknown;
    ModulePathSnapshot normalizedRelativePath {};
    ModuleNameSnapshot normalizedModuleName {};
    ObservedPeIdentity identity {};
    std::uint32_t runtimeModuleBase = 0;
    std::uint32_t runtimeModuleBytes = 0;
    bool canonicalPathObserved = false;
    bool authenticodeValid = false;
    bool loadedExecutableSectionsMatched = false;
};

// A PID is recyclable. Every observation in one authorization transaction is
// therefore bound to both the PID and the immutable process creation time.
struct ProcessCreationIdentity
{
    std::uint32_t processId = 0;
    std::uint64_t creationTime100ns = 0;
};

struct TransparentProxyPolicyEvidence
{
    ProcessCreationIdentity processIdentity {};
    std::uint64_t evidenceGeneration = 0;
    bool d3dExactTransparentBuildIdentityMatched = false;
    bool d3dMappedExecutableSectionsMatched = false;
    bool d3dActivationSourceFuseFalse = false;
    bool d3dProductIntegrationSourceFuseFalse = false;
    bool d3dTransparentForwardingVerified = false;
    bool dinputExactTransparentBuildIdentityMatched = false;
    bool dinputMappedExecutableSectionsMatched = false;
    bool dinputProductionSourceFuseFalse = false;
    bool dinputProductIntegrationSourceFuseFalse = false;
    bool dinputTransparentForwardingVerified = false;
    bool xinputExactTransparentBuildIdentityMatched = false;
    bool xinputMappedExecutableSectionsMatched = false;
    bool xinputProductionSourceFuseFalse = false;
    bool xinputProductIntegrationSourceFuseFalse = false;
    bool xinputTransparentForwardingVerified = false;
    bool noConfigurationOrSharedMemoryBypass = false;
};

enum class ProofDisposition : std::uint8_t
{
    NotRun = 0,
    Passed,
    Failed
};

struct JipCompatibilityProof
{
    ProcessCreationIdentity processIdentity {};
    std::uint64_t evidenceGeneration = 0;
    ProofDisposition disposition = ProofDisposition::NotRun;
    bool loadedExecutableSectionsMatched = false;
    bool exactOwnedPatchInventoryMatched = false;
    bool exactHookTargetMatched = false;
    bool normalizedRetailManifestMatched = false;
    bool noUnexpectedExecutableWrites = false;
};

struct ShowOffCompatibilityProof
{
    ProcessCreationIdentity processIdentity {};
    std::uint64_t evidenceGeneration = 0;
    ProofDisposition disposition = ProofDisposition::NotRun;
    bool loadedExecutableSectionsMatched = false;
    bool exactOwnedPatchInventoryMatched = false;
    bool protectedRendererBytesMatched = false;
    bool noUnexpectedExecutableWrites = false;
};

struct ModuleInventoryEvidence
{
    bool enumerationComplete = false;
    bool usedToolhelp32ModuleAndModule32 = false;
    bool targetProcessIs32Bit = false;
    ProcessCreationIdentity transactionProcessIdentity {};
    std::uint64_t transactionGeneration = 0;
    ProcessCreationIdentity primaryProcessIdentity {};
    std::uint64_t primaryEvidenceGeneration = 0;
    const ModuleObservation* modules = nullptr;
    std::size_t moduleCount = 0;
    TransparentProxyPolicyEvidence proxyPolicy {};
    JipCompatibilityProof jipProof {};
    ShowOffCompatibilityProof showOffProof {};
    bool postProofEnumerationComplete = false;
    bool postProofUsedToolhelp32ModuleAndModule32 = false;
    ProcessCreationIdentity postProofProcessIdentity {};
    std::uint64_t postProofEvidenceGeneration = 0;
    const ModuleObservation* postProofModules = nullptr;
    std::size_t postProofModuleCount = 0;
    std::uint64_t proofCompletionOrdinal = 0;
    std::uint64_t postProofEnumerationOrdinal = 0;
    std::uint64_t transactionBeginOrdinal = 0;
};

enum class InventoryFailure : std::uint8_t
{
    None = 0,
    EnumerationIncomplete,
    WrongEnumerationMode,
    TargetNot32Bit,
    MissingTransactionProcessIdentity,
    MissingPrimaryProcessIdentity,
    MissingPostProofProcessIdentity,
    MissingEvidenceProcessIdentity,
    EvidenceProcessIdentityMismatch,
    MissingTransactionGeneration,
    MissingEvidenceGeneration,
    EvidenceGenerationMismatch,
    PrimaryModuleCountMismatch,
    PostProofModuleCountMismatch,
    ObservationStorageMissing,
    ObservationStorageInvalid,
    PostProofObservationStorageMissing,
    PostProofObservationStorageInvalid,
    PostProofObservationStorageAliased,
    PostProofEnumerationIncomplete,
    WrongPostProofEnumerationMode,
    RevalidationNotTransactionAdjacent,
    ProhibitedSteamOverlay,
    ProhibitedNvidiaCapture,
    InvalidObservationTextSnapshot,
    NonCanonicalPath,
    InvalidPeIdentity,
    UnsignedRuntime,
    NamePathMismatch,
    UnexpectedModule,
    DuplicateModulePath,
    ExactIdentityMismatch,
    InvalidRuntimeMapping,
    OverlappingRuntimeMappings,
    LoadedExecutableSectionsMismatch,
    MissingModule,
    PostProofObservationMismatch,
    ProxyPolicyUnverified,
    JipProofUnverified,
    ShowOffProofUnverified
};

struct ModuleInventoryAssessment
{
    bool moduleSetAccepted = false;
    bool overlaysAbsent = false;
    bool proxyPoliciesAccepted = false;
    bool jipProofAccepted = false;
    bool showOffProofAccepted = false;
    bool overallAccepted = false;
    InventoryFailure failure = InventoryFailure::EnumerationIncomplete;
    std::size_t observationIndex = 0;
};

constexpr bool isHex(char value) noexcept
{
    return (value >= '0' && value <= '9')
        || (value >= 'a' && value <= 'f')
        || (value >= 'A' && value <= 'F');
}

constexpr bool validSha256(std::string_view digest) noexcept
{
    if (digest.size() != 64u)
        return false;
    for (char value : digest)
    {
        if (!isHex(value))
            return false;
    }
    return true;
}

constexpr std::string_view baseName(std::string_view path) noexcept
{
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string_view::npos ? path : path.substr(slash + 1u);
}

constexpr char asciiLower(char value) noexcept
{
    return value >= 'A' && value <= 'Z'
        ? static_cast<char>(value + ('a' - 'A'))
        : value;
}

constexpr bool asciiCaseInsensitiveEqual(
    std::string_view left,
    std::string_view right) noexcept
{
    if (left.size() != right.size())
        return false;
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        if (asciiLower(left[index]) != asciiLower(right[index]))
            return false;
    }
    return true;
}

constexpr bool normalizedRelativePath(std::string_view path) noexcept
{
    if (path.empty() || path.front() == '/' || path.back() == '/')
        return false;
    if (path.find('\\') != std::string_view::npos
        || path.find(':') != std::string_view::npos
        || path.find("..") != std::string_view::npos
        || path.find("//") != std::string_view::npos)
    {
        return false;
    }
    for (char value : path)
    {
        if (value >= 'A' && value <= 'Z')
            return false;
    }
    return true;
}

constexpr bool peIdentityObserved(const PeIdentity& identity) noexcept
{
    return identity.machine == PeMachineX86
        && identity.timeDateStamp != 0u
        && identity.sizeOfImage != 0u
        && identity.preferredImageBase != 0u
        && identity.fileSize != 0u
        && validSha256(identity.sha256);
}

constexpr bool peIdentityObserved(
    const ObservedPeIdentity& identity) noexcept
{
    return identity.machine == PeMachineX86
        && identity.timeDateStamp != 0u
        && identity.sizeOfImage != 0u
        && identity.preferredImageBase != 0u
        && identity.fileSize != 0u
        && identity.sha256.valid()
        && validSha256(identity.sha256.view());
}

constexpr bool exactIdentityMatches(
    const PeIdentity& expected,
    const ObservedPeIdentity& observed) noexcept
{
    return expected.machine == observed.machine
        && expected.timeDateStamp == observed.timeDateStamp
        && expected.sizeOfImage == observed.sizeOfImage
        && expected.preferredImageBase == observed.preferredImageBase
        && expected.fileSize == observed.fileSize
        && expected.sha256 == observed.sha256.view();
}

constexpr bool processIdentityObserved(
    const ProcessCreationIdentity& identity) noexcept
{
    return identity.processId != 0u && identity.creationTime100ns != 0u;
}

constexpr bool sameProcessCreation(
    const ProcessCreationIdentity& left,
    const ProcessCreationIdentity& right) noexcept
{
    return left.processId == right.processId
        && left.creationTime100ns == right.creationTime100ns;
}

inline bool observationStorageRangesOverlap(
    const ModuleObservation* left,
    std::size_t leftCount,
    const ModuleObservation* right,
    std::size_t rightCount) noexcept
{
    constexpr std::uintptr_t MaximumAddress =
        (std::numeric_limits<std::uintptr_t>::max)();
    if (leftCount > MaximumAddress / sizeof(ModuleObservation)
        || rightCount > MaximumAddress / sizeof(ModuleObservation))
    {
        return true;
    }

    const std::uintptr_t leftBytes =
        static_cast<std::uintptr_t>(leftCount * sizeof(ModuleObservation));
    const std::uintptr_t rightBytes =
        static_cast<std::uintptr_t>(rightCount * sizeof(ModuleObservation));
    const std::uintptr_t leftBegin = reinterpret_cast<std::uintptr_t>(left);
    const std::uintptr_t rightBegin = reinterpret_cast<std::uintptr_t>(right);
    if (leftBegin > MaximumAddress - leftBytes
        || rightBegin > MaximumAddress - rightBytes)
    {
        return true;
    }

    const std::uintptr_t leftEnd = leftBegin + leftBytes;
    const std::uintptr_t rightEnd = rightBegin + rightBytes;
    return leftBegin < rightEnd && rightBegin < leftEnd;
}

inline bool observationStorageRangeValid(
    const ModuleObservation* observations,
    std::size_t count) noexcept
{
    constexpr std::uintptr_t MaximumAddress =
        (std::numeric_limits<std::uintptr_t>::max)();
    if (!observations
        || count == 0u
        || count > MaximumAddress / sizeof(ModuleObservation))
    {
        return false;
    }

    const std::uintptr_t begin =
        reinterpret_cast<std::uintptr_t>(observations);
    // MSVC's x86 ABI may place otherwise valid stack aggregates containing
    // 64-bit fields at four-byte boundaries. Require the target ABI pointer
    // alignment here rather than the stricter reported aggregate alignment.
    if (begin % alignof(std::uintptr_t) != 0u)
        return false;

    const std::uintptr_t bytes =
        static_cast<std::uintptr_t>(count * sizeof(ModuleObservation));
    return begin <= MaximumAddress - bytes;
}

constexpr bool runtimeMappingObserved(
    const ModuleObservation& module) noexcept
{
    constexpr std::uint64_t Pe32AddressSpaceBytes = 1ull << 32u;
    const std::uint64_t mappingEnd =
        static_cast<std::uint64_t>(module.runtimeModuleBase)
        + static_cast<std::uint64_t>(module.runtimeModuleBytes);
    return module.runtimeModuleBase != 0u
        && module.runtimeModuleBytes != 0u
        && module.runtimeModuleBytes == module.identity.sizeOfImage
        && mappingEnd <= Pe32AddressSpaceBytes;
}

constexpr bool runtimeMappingsOverlap(
    const ModuleObservation& left,
    const ModuleObservation& right) noexcept
{
    const std::uint64_t leftBegin = left.runtimeModuleBase;
    const std::uint64_t leftEnd =
        leftBegin + static_cast<std::uint64_t>(left.runtimeModuleBytes);
    const std::uint64_t rightBegin = right.runtimeModuleBase;
    const std::uint64_t rightEnd =
        rightBegin + static_cast<std::uint64_t>(right.runtimeModuleBytes);
    return leftBegin < rightEnd && rightBegin < leftEnd;
}

constexpr bool samePeIdentity(
    const ObservedPeIdentity& left,
    const ObservedPeIdentity& right) noexcept
{
    return left.machine == right.machine
        && left.timeDateStamp == right.timeDateStamp
        && left.sizeOfImage == right.sizeOfImage
        && left.preferredImageBase == right.preferredImageBase
        && left.fileSize == right.fileSize
        && left.sha256.valid()
        && right.sha256.valid()
        && left.sha256.view() == right.sha256.view();
}

constexpr bool sameModuleObservation(
    const ModuleObservation& left,
    const ModuleObservation& right) noexcept
{
    return left.origin == right.origin
        && left.normalizedRelativePath.valid()
        && right.normalizedRelativePath.valid()
        && left.normalizedRelativePath.view()
            == right.normalizedRelativePath.view()
        && left.normalizedModuleName.valid()
        && right.normalizedModuleName.valid()
        && left.normalizedModuleName.view() == right.normalizedModuleName.view()
        && samePeIdentity(left.identity, right.identity)
        && left.runtimeModuleBase == right.runtimeModuleBase
        && left.runtimeModuleBytes == right.runtimeModuleBytes
        && left.canonicalPathObserved == right.canonicalPathObserved
        && left.authenticodeValid == right.authenticodeValid
        && left.loadedExecutableSectionsMatched
            == right.loadedExecutableSectionsMatched;
}

constexpr bool proxyPolicyComplete(
    const TransparentProxyPolicyEvidence& policy) noexcept
{
    return policy.d3dExactTransparentBuildIdentityMatched
        && policy.d3dMappedExecutableSectionsMatched
        && policy.d3dActivationSourceFuseFalse
        && policy.d3dProductIntegrationSourceFuseFalse
        && policy.d3dTransparentForwardingVerified
        && policy.dinputExactTransparentBuildIdentityMatched
        && policy.dinputMappedExecutableSectionsMatched
        && policy.dinputProductionSourceFuseFalse
        && policy.dinputProductIntegrationSourceFuseFalse
        && policy.dinputTransparentForwardingVerified
        && policy.xinputExactTransparentBuildIdentityMatched
        && policy.xinputMappedExecutableSectionsMatched
        && policy.xinputProductionSourceFuseFalse
        && policy.xinputProductIntegrationSourceFuseFalse
        && policy.xinputTransparentForwardingVerified
        && policy.noConfigurationOrSharedMemoryBypass;
}

constexpr bool jipProofComplete(const JipCompatibilityProof& proof) noexcept
{
    return proof.disposition == ProofDisposition::Passed
        && proof.loadedExecutableSectionsMatched
        && proof.exactOwnedPatchInventoryMatched
        && proof.exactHookTargetMatched
        && proof.normalizedRetailManifestMatched
        && proof.noUnexpectedExecutableWrites;
}

constexpr bool showOffProofComplete(
    const ShowOffCompatibilityProof& proof) noexcept
{
    return proof.disposition == ProofDisposition::Passed
        && proof.loadedExecutableSectionsMatched
        && proof.exactOwnedPatchInventoryMatched
        && proof.protectedRendererBytesMatched
        && proof.noUnexpectedExecutableWrites;
}

constexpr std::size_t findExpectedModule(
    ModuleOrigin origin,
    std::string_view path) noexcept
{
    for (std::size_t index = 0; index < AllowedModuleCount; ++index)
    {
        if (AllowedModules[index].origin == origin
            && AllowedModules[index].normalizedRelativePath == path)
        {
            return index;
        }
    }
    return AllowedModuleCount;
}

inline bool moduleNameSnapshotsValid(
    const ModuleObservation* observations,
    std::size_t count,
    std::size_t& invalidIndex) noexcept
{
    for (std::size_t index = 0; index < count; ++index)
    {
        if (!observations[index].normalizedModuleName.valid())
        {
            invalidIndex = index;
            return false;
        }
    }
    return true;
}

inline InventoryFailure prohibitedOverlayFailure(
    const ModuleObservation* observations,
    std::size_t count,
    std::size_t& prohibitedIndex) noexcept
{
    for (std::size_t index = 0; index < count; ++index)
    {
        const std::string_view moduleName =
            observations[index].normalizedModuleName.view();
        if (asciiCaseInsensitiveEqual(
                moduleName,
                "gameoverlayrenderer.dll"))
        {
            prohibitedIndex = index;
            return InventoryFailure::ProhibitedSteamOverlay;
        }
        if (asciiCaseInsensitiveEqual(moduleName, "nvspcap.dll"))
        {
            prohibitedIndex = index;
            return InventoryFailure::ProhibitedNvidiaCapture;
        }
    }
    return InventoryFailure::None;
}

inline ModuleInventoryAssessment assessModuleInventory(
    const ModuleInventoryEvidence& evidence) noexcept
{
    ModuleInventoryAssessment result {};
    const bool proxyPolicyContentAccepted =
        proxyPolicyComplete(evidence.proxyPolicy);
    const bool jipProofContentAccepted = jipProofComplete(evidence.jipProof);
    const bool showOffProofContentAccepted =
        showOffProofComplete(evidence.showOffProof);

    if (!evidence.enumerationComplete)
        return result;
    if (!evidence.usedToolhelp32ModuleAndModule32)
    {
        result.failure = InventoryFailure::WrongEnumerationMode;
        return result;
    }
    if (!evidence.targetProcessIs32Bit)
    {
        result.failure = InventoryFailure::TargetNot32Bit;
        return result;
    }

    // Counts are attacker-controlled live-process observations. Bound both
    // before evaluating either pointer or dereferencing a single element.
    // Exact allowlist cardinality remains a later, independent gate.
    if (evidence.moduleCount == 0u
        || evidence.moduleCount > MaximumDiagnosticModuleCount)
    {
        result.failure = InventoryFailure::PrimaryModuleCountMismatch;
        return result;
    }
    if (evidence.postProofModuleCount == 0u
        || evidence.postProofModuleCount > MaximumDiagnosticModuleCount)
    {
        result.failure = InventoryFailure::PostProofModuleCountMismatch;
        return result;
    }
    if (!evidence.modules)
    {
        result.failure = InventoryFailure::ObservationStorageMissing;
        return result;
    }
    if (!observationStorageRangeValid(
            evidence.modules,
            evidence.moduleCount))
    {
        result.failure = InventoryFailure::ObservationStorageInvalid;
        return result;
    }
    if (!evidence.postProofModules)
    {
        result.failure = InventoryFailure::PostProofObservationStorageMissing;
        return result;
    }
    if (!observationStorageRangeValid(
            evidence.postProofModules,
            evidence.postProofModuleCount))
    {
        result.failure = InventoryFailure::PostProofObservationStorageInvalid;
        return result;
    }
    if (observationStorageRangesOverlap(
            evidence.modules,
            evidence.moduleCount,
            evidence.postProofModules,
            evidence.postProofModuleCount))
    {
        result.failure = InventoryFailure::PostProofObservationStorageAliased;
        return result;
    }

    // The normalized module names are owned, bounded snapshots. Validate all
    // snapshots before viewing any of them, then diagnose known prohibited
    // overlays in both independently captured censuses. This scan does not
    // relax the exact-count acceptance gate below.
    std::size_t diagnosticIndex = 0u;
    if (!moduleNameSnapshotsValid(
            evidence.modules,
            evidence.moduleCount,
            diagnosticIndex)
        || !moduleNameSnapshotsValid(
            evidence.postProofModules,
            evidence.postProofModuleCount,
            diagnosticIndex))
    {
        result.failure = InventoryFailure::InvalidObservationTextSnapshot;
        result.observationIndex = diagnosticIndex;
        return result;
    }
    InventoryFailure overlayFailure = prohibitedOverlayFailure(
        evidence.modules,
        evidence.moduleCount,
        diagnosticIndex);
    if (overlayFailure == InventoryFailure::None)
    {
        overlayFailure = prohibitedOverlayFailure(
            evidence.postProofModules,
            evidence.postProofModuleCount,
            diagnosticIndex);
    }
    if (overlayFailure != InventoryFailure::None)
    {
        result.failure = overlayFailure;
        result.observationIndex = diagnosticIndex;
        result.overlaysAbsent = false;
        return result;
    }

    if (evidence.moduleCount != AllowedModuleCount)
    {
        result.failure = InventoryFailure::PrimaryModuleCountMismatch;
        return result;
    }
    if (evidence.postProofModuleCount != AllowedModuleCount)
    {
        result.failure = InventoryFailure::PostProofModuleCountMismatch;
        return result;
    }
    if (!evidence.postProofEnumerationComplete)
    {
        result.failure = InventoryFailure::PostProofEnumerationIncomplete;
        return result;
    }
    if (!evidence.postProofUsedToolhelp32ModuleAndModule32)
    {
        result.failure = InventoryFailure::WrongPostProofEnumerationMode;
        return result;
    }

    if (!processIdentityObserved(evidence.transactionProcessIdentity))
    {
        result.failure = InventoryFailure::MissingTransactionProcessIdentity;
        return result;
    }
    if (!processIdentityObserved(evidence.primaryProcessIdentity))
    {
        result.failure = InventoryFailure::MissingPrimaryProcessIdentity;
        return result;
    }
    if (!processIdentityObserved(evidence.postProofProcessIdentity))
    {
        result.failure = InventoryFailure::MissingPostProofProcessIdentity;
        return result;
    }
    if (!processIdentityObserved(evidence.proxyPolicy.processIdentity)
        || !processIdentityObserved(evidence.jipProof.processIdentity)
        || !processIdentityObserved(evidence.showOffProof.processIdentity))
    {
        result.failure = InventoryFailure::MissingEvidenceProcessIdentity;
        return result;
    }
    if (!sameProcessCreation(
            evidence.transactionProcessIdentity,
            evidence.primaryProcessIdentity)
        || !sameProcessCreation(
            evidence.transactionProcessIdentity,
            evidence.postProofProcessIdentity)
        || !sameProcessCreation(
            evidence.transactionProcessIdentity,
            evidence.proxyPolicy.processIdentity)
        || !sameProcessCreation(
            evidence.transactionProcessIdentity,
            evidence.jipProof.processIdentity)
        || !sameProcessCreation(
            evidence.transactionProcessIdentity,
            evidence.showOffProof.processIdentity))
    {
        result.failure = InventoryFailure::EvidenceProcessIdentityMismatch;
        return result;
    }

    if (evidence.transactionGeneration == 0u)
    {
        result.failure = InventoryFailure::MissingTransactionGeneration;
        return result;
    }
    if (evidence.primaryEvidenceGeneration == 0u
        || evidence.postProofEvidenceGeneration == 0u
        || evidence.proxyPolicy.evidenceGeneration == 0u
        || evidence.jipProof.evidenceGeneration == 0u
        || evidence.showOffProof.evidenceGeneration == 0u)
    {
        result.failure = InventoryFailure::MissingEvidenceGeneration;
        return result;
    }
    if (evidence.primaryEvidenceGeneration != evidence.transactionGeneration
        || evidence.postProofEvidenceGeneration != evidence.transactionGeneration
        || evidence.proxyPolicy.evidenceGeneration != evidence.transactionGeneration
        || evidence.jipProof.evidenceGeneration != evidence.transactionGeneration
        || evidence.showOffProof.evidenceGeneration != evidence.transactionGeneration)
    {
        result.failure = InventoryFailure::EvidenceGenerationMismatch;
        return result;
    }
    if (evidence.proofCompletionOrdinal == 0u
        || evidence.postProofEnumerationOrdinal == 0u
        || evidence.transactionBeginOrdinal == 0u
        || evidence.postProofEnumerationOrdinal <= evidence.proofCompletionOrdinal
        || evidence.transactionBeginOrdinal
            <= evidence.postProofEnumerationOrdinal
        || evidence.transactionBeginOrdinal
            - evidence.postProofEnumerationOrdinal != 1u)
    {
        result.failure = InventoryFailure::RevalidationNotTransactionAdjacent;
        return result;
    }

    // Content is never represented as accepted until its process and
    // generation bindings have been validated as one transaction.
    result.proxyPoliciesAccepted = proxyPolicyContentAccepted;
    result.jipProofAccepted = jipProofContentAccepted;
    result.showOffProofAccepted = showOffProofContentAccepted;

    std::array<bool, AllowedModuleCount> seen {};
    result.overlaysAbsent = true;
    for (std::size_t index = 0; index < evidence.moduleCount; ++index)
    {
        result.observationIndex = index;
        const ModuleObservation& module = evidence.modules[index];
        if (!module.normalizedRelativePath.valid()
            || !module.normalizedModuleName.valid()
            || !module.identity.sha256.valid())
        {
            result.failure = InventoryFailure::InvalidObservationTextSnapshot;
            return result;
        }
        if (asciiCaseInsensitiveEqual(
                module.normalizedModuleName.view(),
                "gameoverlayrenderer.dll"))
        {
            result.overlaysAbsent = false;
            result.failure = InventoryFailure::ProhibitedSteamOverlay;
            return result;
        }
        if (asciiCaseInsensitiveEqual(
                module.normalizedModuleName.view(),
                "nvspcap.dll"))
        {
            result.overlaysAbsent = false;
            result.failure = InventoryFailure::ProhibitedNvidiaCapture;
            return result;
        }
        if (!module.canonicalPathObserved
            || !normalizedRelativePath(module.normalizedRelativePath.view()))
        {
            result.failure = InventoryFailure::NonCanonicalPath;
            return result;
        }
        if (!peIdentityObserved(module.identity))
        {
            result.failure = InventoryFailure::InvalidPeIdentity;
            return result;
        }
        if (!runtimeMappingObserved(module))
        {
            result.failure = InventoryFailure::InvalidRuntimeMapping;
            return result;
        }
        for (std::size_t priorIndex = 0; priorIndex < index; ++priorIndex)
        {
            if (runtimeMappingsOverlap(module, evidence.modules[priorIndex]))
            {
                result.failure = InventoryFailure::OverlappingRuntimeMappings;
                return result;
            }
        }
        if (module.origin != ModuleOrigin::GameTree
            && !module.authenticodeValid)
        {
            result.failure = InventoryFailure::UnsignedRuntime;
            return result;
        }
        if (module.normalizedModuleName.view()
            != baseName(module.normalizedRelativePath.view()))
        {
            result.failure = InventoryFailure::NamePathMismatch;
            return result;
        }

        const std::size_t expectedIndex = findExpectedModule(
            module.origin,
            module.normalizedRelativePath.view());
        if (expectedIndex == AllowedModuleCount)
        {
            result.failure = InventoryFailure::UnexpectedModule;
            return result;
        }
        if (seen[expectedIndex])
        {
            result.failure = InventoryFailure::DuplicateModulePath;
            return result;
        }
        seen[expectedIndex] = true;

        const ExpectedModule& expected = AllowedModules[expectedIndex];
        if (expected.exactIdentityRequired
            && !exactIdentityMatches(expected.exactIdentity, module.identity))
        {
            result.failure = InventoryFailure::ExactIdentityMismatch;
            return result;
        }
        if (expected.origin == ModuleOrigin::GameTree
            && expected.exactIdentityRequired
            && !module.loadedExecutableSectionsMatched)
        {
            result.failure = InventoryFailure::LoadedExecutableSectionsMismatch;
            return result;
        }
    }

    for (std::size_t index = 0; index < AllowedModuleCount; ++index)
    {
        if (!seen[index])
        {
            result.failure = InventoryFailure::MissingModule;
            result.observationIndex = index;
            return result;
        }
    }
    for (std::size_t index = 0; index < AllowedModuleCount; ++index)
    {
        if (!sameModuleObservation(
                evidence.modules[index],
                evidence.postProofModules[index]))
        {
            result.failure = InventoryFailure::PostProofObservationMismatch;
            result.observationIndex = index;
            return result;
        }
    }

    result.moduleSetAccepted = true;
    if (!result.proxyPoliciesAccepted)
    {
        result.failure = InventoryFailure::ProxyPolicyUnverified;
        return result;
    }
    if (!result.jipProofAccepted)
    {
        result.failure = InventoryFailure::JipProofUnverified;
        return result;
    }
    if (!result.showOffProofAccepted)
    {
        result.failure = InventoryFailure::ShowOffProofUnverified;
        return result;
    }

    result.overallAccepted = true;
    result.failure = InventoryFailure::None;
    result.observationIndex = 0u;
    return result;
}
}
