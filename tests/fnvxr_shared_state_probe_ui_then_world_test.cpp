#include "../protocol/fnvxr_shared_state.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{
constexpr std::size_t SharedStereoMappingBytes =
    sizeof(fnvxr::shared::SharedD3D9StereoFrameHeader)
    + static_cast<std::size_t>(fnvxr::shared::D3D9SharedFrameMaxWidth)
        * static_cast<std::size_t>(fnvxr::shared::D3D9SharedFrameMaxHeight)
        * 4u * 2u * fnvxr::shared::D3D9StereoFrameSlotCount;
constexpr std::size_t SharedStereoSlotBytes =
    static_cast<std::size_t>(fnvxr::shared::D3D9SharedFrameMaxWidth)
    * static_cast<std::size_t>(fnvxr::shared::D3D9SharedFrameMaxHeight)
    * 4u * 2u;

static_assert(SharedStereoMappingBytes <= MAXDWORD);

class Mapping final
{
public:
    ~Mapping()
    {
        if (mView)
            UnmapViewOfFile(mView);
        if (mHandle)
            CloseHandle(mHandle);
    }

    bool create()
    {
        mHandle = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            0u,
            static_cast<DWORD>(SharedStereoMappingBytes),
            fnvxr::shared::D3D9StereoFrameSharedMappingName);
        if (!mHandle || GetLastError() == ERROR_ALREADY_EXISTS)
            return false;
        mView = static_cast<std::uint8_t*>(MapViewOfFile(
            mHandle,
            FILE_MAP_ALL_ACCESS,
            0u,
            0u,
            SharedStereoMappingBytes));
        return mView != nullptr;
    }

    std::uint8_t* view() const noexcept
    {
        return mView;
    }

private:
    HANDLE mHandle = nullptr;
    std::uint8_t* mView = nullptr;
};

LONG longFromBits(std::uint32_t bits)
{
    LONG result = 0;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

LONG64 long64FromBits(std::uint64_t bits)
{
    LONG64 result = 0;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

void writePresentation(
    std::uint8_t* view,
    std::uint32_t producerMode,
    bool uiActive,
    bool separated,
    bool worldCandidate,
    bool monoPixels,
    std::uint64_t transactionId,
    std::uint64_t sourceFrame,
    std::uint64_t runtimeStateSample,
    std::uint64_t publicationGeneration,
    LONG sequence,
    std::uint64_t rendererProducerEpoch = 5u)
{
    if (!view)
        return;

    auto* header = reinterpret_cast<
        fnvxr::shared::SharedD3D9StereoFrameHeader*>(view);
    constexpr LONG slot = 0;
    constexpr LONG width = 2;
    constexpr LONG height = 1;
    constexpr LONG pitchBytes = width * 4;
    const std::size_t leftOffset = sizeof(*header)
        + static_cast<std::size_t>(slot) * SharedStereoSlotBytes;
    const std::size_t rightOffset = leftOffset + pitchBytes * height;
    std::uint32_t transactionBits = static_cast<std::uint32_t>(transactionId);
    if (transactionBits == 0u)
        transactionBits = 1u;

    InterlockedExchange(&header->writing, 1);
    MemoryBarrier();
    header->magic = fnvxr::shared::D3D9StereoFrameSharedMagic;
    header->version = fnvxr::shared::D3D9StereoFrameSharedVersion;
    header->headerBytes = sizeof(*header);
    header->sequence = sequence;
    header->width = width;
    header->height = height;
    header->pitchBytes = pitchBytes;
    header->format = 22;
    header->separated = separated ? 1 : 0;
    header->worldCandidate = worldCandidate ? 1 : 0;
    header->uiActive = uiActive ? 1 : 0;
    header->poseValid = 1;
    header->poseSequence = 2;
    header->renderedDisplayTime = 123;
    header->producerMode = static_cast<LONG>(producerMode);
    header->renderPairSequence = longFromBits(transactionBits);
    header->leftPayloadOffset = static_cast<std::uint32_t>(leftOffset);
    header->rightPayloadOffset = static_cast<std::uint32_t>(rightOffset);
    header->totalMappingBytes = static_cast<std::uint32_t>(
        SharedStereoMappingBytes);
    header->referenceSpaceGeneration = 3u;
    header->producerEpoch = 4u;
    header->rendererProducerEpoch = rendererProducerEpoch;
    header->producerProcessId = GetCurrentProcessId();
    header->publishedSlot = slot;
    for (std::uint32_t lane = 0u;
         lane < fnvxr::shared::D3D9StereoFrameReaderLaneCount;
         ++lane)
    {
        header->readerSlots[lane] = -1;
    }
    header->transactionId = transactionId;
    header->sourceFrame = sourceFrame;
    header->runtimeStateSample = runtimeStateSample;

    const std::uint8_t leftPixels[] {
        1u, 2u, 3u, 255u,
        4u, 5u, 6u, 255u,
    };
    const std::uint8_t rightPixels[] {
        monoPixels ? 1u : 7u,
        monoPixels ? 2u : 8u,
        monoPixels ? 3u : 9u,
        255u,
        monoPixels ? 4u : 10u,
        monoPixels ? 5u : 11u,
        monoPixels ? 6u : 12u,
        255u,
    };
    std::memcpy(view + leftOffset, leftPixels, sizeof(leftPixels));
    std::memcpy(view + rightOffset, rightPixels, sizeof(rightPixels));
    InterlockedExchange64(
        &header->publicationGeneration,
        long64FromBits(publicationGeneration));
    MemoryBarrier();
    InterlockedExchange(&header->writing, 0);
}

bool runProbe(
    const char* probePath,
    int timeoutMilliseconds,
    bool reportFailure = true)
{
    if (!probePath || probePath[0] == '\0')
        return false;
    std::string command = "\"";
    command += probePath;
    command += "\" --require-ui-then-engine-center-world --transition-timeout-ms ";
    command += std::to_string(timeoutMilliseconds);
    std::vector<char> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back('\0');

    STARTUPINFOA startup {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process {};
    if (!CreateProcessA(
            nullptr,
            mutableCommand.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &startup,
            &process))
    {
        return false;
    }
    const DWORD wait = WaitForSingleObject(process.hProcess, 10000u);
    DWORD exitCode = EXIT_FAILURE;
    const bool complete = wait == WAIT_OBJECT_0
        && GetExitCodeProcess(process.hProcess, &exitCode) != FALSE;
    if (!complete)
    {
        TerminateProcess(process.hProcess, EXIT_FAILURE);
        WaitForSingleObject(process.hProcess, 1000u);
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (!complete || exitCode != EXIT_SUCCESS)
    {
        if (reportFailure)
        {
            std::cerr << "transition probe failed exit=" << exitCode
                      << " timeoutMs=" << timeoutMilliseconds << "\n";
        }
        return false;
    }
    return true;
}

bool publishThenRunProbe(
    const char* probePath,
    std::uint8_t* view,
    std::uint32_t worldProducerMode,
    std::uint64_t worldTransactionId,
    std::uint64_t worldSourceFrame,
    std::uint64_t worldPublicationGeneration,
    bool reportFailure = true,
    std::uint64_t worldRendererProducerEpoch = 5u)
{
    std::thread publisher([
        view,
        worldProducerMode,
        worldTransactionId,
        worldSourceFrame,
        worldPublicationGeneration,
        worldRendererProducerEpoch]() {
        Sleep(300u);
        writePresentation(
            view,
            worldProducerMode,
            false,
            true,
            true,
            false,
            worldTransactionId,
            worldSourceFrame,
            worldSourceFrame,
            worldPublicationGeneration,
            4,
            worldRendererProducerEpoch);
    });
    const bool passed = runProbe(probePath, 1200, reportFailure);
    publisher.join();
    return passed;
}
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: fnvxr_shared_state_probe_ui_then_world_test <probe-exe>\n";
        return EXIT_FAILURE;
    }

    Mapping mapping;
    if (!mapping.create())
    {
        std::cerr << "could not create isolated stereo mapping\n";
        return EXIT_FAILURE;
    }

    writePresentation(
        mapping.view(),
        fnvxr::shared::StereoProducerMonoUiQuad,
        true,
        false,
        false,
        true,
        10u,
        10u,
        10u,
        100u,
        2);
    if (!publishThenRunProbe(
            argv[1],
            mapping.view(),
            fnvxr::shared::StereoProducerEngineCenter,
            11u,
            11u,
            101u))
    {
        std::cerr << "probe rejected an exact UI-to-engine-center-world transition\n";
        return EXIT_FAILURE;
    }

    writePresentation(
        mapping.view(),
        fnvxr::shared::StereoProducerMonoUiQuad,
        true,
        false,
        false,
        true,
        20u,
        20u,
        20u,
        200u,
        2);
    if (publishThenRunProbe(
            argv[1],
            mapping.view(),
            fnvxr::shared::StereoProducerEngineCenter,
            20u,
            21u,
            201u,
            false))
    {
        std::cerr << "probe accepted a world transaction that did not follow the menu\n";
        return EXIT_FAILURE;
    }

    writePresentation(
        mapping.view(),
        fnvxr::shared::StereoProducerMonoUiQuad,
        true,
        false,
        false,
        false,
        30u,
        30u,
        30u,
        300u,
        2);
    if (publishThenRunProbe(
            argv[1],
            mapping.view(),
            fnvxr::shared::StereoProducerEngineCenter,
            31u,
            31u,
            301u,
            false))
    {
        std::cerr << "probe accepted a non-mono menu frame as the transition boundary\n";
        return EXIT_FAILURE;
    }

    writePresentation(
        mapping.view(),
        fnvxr::shared::StereoProducerMonoUiQuad,
        true,
        false,
        false,
        true,
        40u,
        40u,
        40u,
        400u,
        2,
        5u);
    if (publishThenRunProbe(
            argv[1],
            mapping.view(),
            fnvxr::shared::StereoProducerEngineCenter,
            41u,
            41u,
            401u,
            false,
            6u))
    {
        std::cerr << "probe accepted a world pair from a different renderer epoch\n";
        return EXIT_FAILURE;
    }

    std::cout << "UI-to-engine-center-world probe mapping test passed\n";
    return EXIT_SUCCESS;
}
