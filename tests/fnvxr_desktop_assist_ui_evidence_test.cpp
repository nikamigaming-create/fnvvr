#include "../runtime/fnvxr_desktop_assist_ui_evidence.h"

#include <array>
#include <cstdint>
#include <iostream>

namespace
{
fnvxr::shared::SharedDesktopAssistUiQuadHeader completeHeader()
{
    fnvxr::shared::SharedDesktopAssistUiQuadHeader header {};
    header.magic = fnvxr::shared::DesktopAssistUiQuadSharedMagic;
    header.version = fnvxr::shared::DesktopAssistUiQuadSharedVersion;
    header.headerBytes = sizeof(header);
    header.sequence = 2;
    header.flags = fnvxr::engine::DesktopAssistUiQuadRequiredFlags;
    header.width = 2;
    header.height = 1;
    header.pitchBytes = 8;
    header.format = fnvxr::engine::DesktopAssistUiQuadPixelFormatX8R8G8B8;
    header.runtimeStateSample = 11u;
    header.poseFrame = 12u;
    header.poseSequence = 4;
    header.runtimePhase = fnvxr::shared::RuntimePhaseMenu;
    header.runtimeMenuBits = fnvxr::shared::RuntimeGenericMenuBit;
    header.captureFailure = 0u;
    header.nonBlackSampleCount = 1u;
    header.poseProducerEpoch = 13u;
    header.captureOrdinal = 14u;
    return header;
}

bool expect(bool condition, const char* message)
{
    if (condition)
        return true;
    std::cerr << "FAIL: " << message << "\n";
    return false;
}
}

int main()
{
    using fnvxr::engine::desktopAssistUiQuadHeaderIsComplete;
    using fnvxr::engine::desktopAssistUiQuadPayloadLayoutIsValid;
    using fnvxr::engine::desktopAssistUiQuadPixelHash;

    auto header = completeHeader();
    bool passed = expect(desktopAssistUiQuadHeaderIsComplete(header), "complete header rejected");

    std::size_t payloadBytes = 0u;
    passed = expect(
        desktopAssistUiQuadPayloadLayoutIsValid(header, &payloadBytes) && payloadBytes == 8u,
        "payload layout rejected") && passed;

    const std::array<std::uint8_t, 8> pixels {
        0u, 0u, 0u, 255u,
        1u, 2u, 3u, 255u,
    };
    std::uint32_t nonBlack = 0u;
    header.pixelHash = desktopAssistUiQuadPixelHash(
        pixels.data(), pixels.size(), &nonBlack);
    passed = expect(nonBlack == 1u, "non-black count mismatch") && passed;
    passed = expect(
        header.pixelHash == 0x48B0A255u,
        "pixel hash changed unexpectedly") && passed;

    header.flags &= ~fnvxr::shared::DesktopAssistUiQuadFlagPoseEpochCurrent;
    passed = expect(!desktopAssistUiQuadHeaderIsComplete(header), "missing epoch flag accepted") && passed;
    header = completeHeader();
    header.poseProducerEpoch = 0u;
    passed = expect(!desktopAssistUiQuadHeaderIsComplete(header), "missing epoch accepted") && passed;
    header = completeHeader();
    header.runtimePhase = fnvxr::shared::RuntimePhaseGameplay;
    header.runtimeMenuBits = 0u;
    passed = expect(!desktopAssistUiQuadHeaderIsComplete(header), "gameplay header accepted") && passed;
    header = completeHeader();
    header.pitchBytes = 12;
    passed = expect(!desktopAssistUiQuadHeaderIsComplete(header), "non-tight pitch accepted") && passed;
    header = completeHeader();
    header.format = 0;
    passed = expect(!desktopAssistUiQuadHeaderIsComplete(header), "unknown format accepted") && passed;
    header = completeHeader();
    header.writing = 1;
    passed = expect(!desktopAssistUiQuadHeaderIsComplete(header), "active writer accepted") && passed;

    std::cout << "{\"test\":\"fnvxr-desktop-assist-ui-evidence\",\"pass\":"
              << (passed ? "true" : "false") << "}\n";
    return passed ? 0 : 1;
}
