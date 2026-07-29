#include "../protocol/fnvxr_shared_state.h"

#include <windows.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
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

bool runProbe(
    const char* probePath,
    const char* requirement,
    bool reportFailure = true)
{
    if (!probePath || probePath[0] == '\0' || !requirement || requirement[0] == '\0')
        return false;
    std::string command = "\"";
    command += probePath;
    command += "\" ";
    command += requirement;
    std::vector<char> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back('\0');

    STARTUPINFOA startup {};
    startup.cb = sizeof(startup);
    SECURITY_ATTRIBUTES outputAttributes {};
    outputAttributes.nLength = sizeof(outputAttributes);
    outputAttributes.bInheritHandle = TRUE;
    HANDLE outputRead = nullptr;
    HANDLE outputWrite = nullptr;
    if (!CreatePipe(&outputRead, &outputWrite, &outputAttributes, 65536u)
        || !SetHandleInformation(outputRead, HANDLE_FLAG_INHERIT, 0u))
    {
        if (outputRead)
            CloseHandle(outputRead);
        if (outputWrite)
            CloseHandle(outputWrite);
        return false;
    }
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = outputWrite;
    startup.hStdError = outputWrite;
    PROCESS_INFORMATION process {};
    if (!CreateProcessA(
            nullptr,
            mutableCommand.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &startup,
            &process))
    {
        CloseHandle(outputRead);
        CloseHandle(outputWrite);
        return false;
    }
    CloseHandle(outputWrite);
    const DWORD wait = WaitForSingleObject(process.hProcess, 10000u);
    DWORD exitCode = EXIT_FAILURE;
    const bool complete = wait == WAIT_OBJECT_0
        && GetExitCodeProcess(process.hProcess, &exitCode) != FALSE;
    std::string output;
    char buffer[1024] {};
    DWORD outputBytes = 0u;
    while (ReadFile(outputRead, buffer, sizeof(buffer), &outputBytes, nullptr)
        && outputBytes != 0u)
    {
        output.append(buffer, outputBytes);
    }
    CloseHandle(outputRead);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (!complete || exitCode != EXIT_SUCCESS)
    {
        if (reportFailure)
        {
            std::cerr << "probe failed requirement=" << requirement
                      << " exit=" << exitCode << "\n";
            if (!output.empty())
                std::cerr << output;
        }
        return false;
    }
    return true;
}

LONG64 long64FromBits(std::uint64_t bits)
{
    LONG64 result = 0;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

void writeWorldStereo(
    std::uint8_t* view,
    LONG producerMode,
    LONG sequence,
    std::uint64_t publicationGeneration,
    LONG renderPairSequence,
    LONG slot,
    bool distinctPixels)
{
    if (!view
        || slot < 0
        || slot >= static_cast<LONG>(fnvxr::shared::D3D9StereoFrameSlotCount))
    {
        return;
    }

    auto* header = reinterpret_cast<fnvxr::shared::SharedD3D9StereoFrameHeader*>(view);
    InterlockedExchange(&header->writing, 1);
    MemoryBarrier();
    header->magic = fnvxr::shared::D3D9StereoFrameSharedMagic;
    header->version = fnvxr::shared::D3D9StereoFrameSharedVersion;
    header->headerBytes = sizeof(*header);
    header->sequence = sequence;
    header->width = 2;
    header->height = 1;
    header->pitchBytes = 8;
    header->format = 22;
    header->separated = 1;
    header->worldCandidate = 1;
    header->uiActive = 0;
    header->poseValid = 1;
    header->poseSequence = 2;
    header->renderedDisplayTime = 123;
    header->producerMode = producerMode;
    header->renderPairSequence = renderPairSequence;
    const std::size_t leftPayloadOffset = sizeof(*header)
        + static_cast<std::size_t>(slot) * SharedStereoSlotBytes;
    const std::size_t rightPayloadOffset = leftPayloadOffset + 8u;
    header->leftPayloadOffset = static_cast<std::uint32_t>(leftPayloadOffset);
    header->rightPayloadOffset = static_cast<std::uint32_t>(rightPayloadOffset);
    header->totalMappingBytes = static_cast<std::uint32_t>(SharedStereoMappingBytes);
    header->referenceSpaceGeneration = 3u;
    header->producerEpoch = 4u;
    header->rendererProducerEpoch = 5u;
    header->producerProcessId = GetCurrentProcessId();
    header->transactionId =
        fnvxr::shared::sequencedValueBits(renderPairSequence);
    header->sourceFrame =
        static_cast<std::uint64_t>(
            fnvxr::shared::sequencedValueBits(sequence));
    header->runtimeStateSample = header->sourceFrame;
    header->publishedSlot = slot;
    for (std::uint32_t lane = 0u;
         lane < fnvxr::shared::D3D9StereoFrameReaderLaneCount;
         ++lane)
    {
        header->readerSlots[lane] = -1;
    }
    const std::uint8_t leftPixels[] {
        1u, 2u, 3u, 255u, 4u, 5u, 6u, 255u,
    };
    const std::uint8_t rightPixels[] {
        distinctPixels ? 7u : 1u,
        distinctPixels ? 8u : 2u,
        distinctPixels ? 9u : 3u,
        255u,
        distinctPixels ? 10u : 4u,
        distinctPixels ? 11u : 5u,
        distinctPixels ? 12u : 6u,
        255u,
    };
    std::memcpy(view + leftPayloadOffset, leftPixels, sizeof(leftPixels));
    std::memcpy(view + rightPayloadOffset, rightPixels, sizeof(rightPixels));
    InterlockedExchange64(
        &header->publicationGeneration,
        long64FromBits(publicationGeneration));
    MemoryBarrier();
    InterlockedExchange(&header->writing, 0);
}

bool runAdvancingProbe(
    const char* probePath,
    std::uint8_t* view,
    LONG producerMode,
    LONG sequence,
    std::uint64_t publicationGeneration,
    LONG renderPairSequence,
    LONG slot,
    bool distinctPixels,
    bool reportFailure = true)
{
    // The real probe samples before and after its delay. Publish the second
    // record between those reads so this exercises its actual freshness path,
    // rather than merely validating one static header.
    std::thread publisher([
        view,
        producerMode,
        sequence,
        publicationGeneration,
        renderPairSequence,
        slot,
        distinctPixels]() {
        Sleep(500u);
        writeWorldStereo(
            view,
            producerMode,
            sequence,
            publicationGeneration,
            renderPairSequence,
            slot,
            distinctPixels);
    });
    const bool passed = runProbe(
        probePath,
        "--require-engine-center-world-stereo --require-advancing --sample-delay-ms 1000",
        reportFailure);
    publisher.join();
    return passed;
}
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: fnvxr_shared_state_probe_stereo_test <probe-exe>\n";
        return EXIT_FAILURE;
    }

    Mapping mapping;
    if (!mapping.create())
    {
        std::cerr << "could not create isolated stereo mapping\n";
        return EXIT_FAILURE;
    }

    writeWorldStereo(
        mapping.view(),
        static_cast<LONG>(fnvxr::shared::StereoProducerEngineCenter),
        2,
        1u,
        1,
        0,
        true);
    if (!runProbe(argv[1], "--require-engine-center-world-stereo"))
    {
        std::cerr << "probe rejected an engine-center world-stereo mapping\n";
        return EXIT_FAILURE;
    }

    writeWorldStereo(
        mapping.view(),
        static_cast<LONG>(fnvxr::shared::StereoProducerEngineCenter),
        4,
        2u,
        2,
        0,
        false);
    if (runProbe(argv[1], "--require-engine-center-world-stereo", false))
    {
        std::cerr << "probe accepted identical engine-center eye payloads\n";
        return EXIT_FAILURE;
    }

    writeWorldStereo(
        mapping.view(),
        static_cast<LONG>(fnvxr::shared::StereoProducerNativeSameFrame),
        6,
        3u,
        3,
        0,
        true);
    if (runProbe(argv[1], "--require-engine-center-world-stereo", false))
    {
        std::cerr << "probe accepted a non-engine-center world-stereo mapping\n";
        return EXIT_FAILURE;
    }

    writeWorldStereo(
        mapping.view(),
        static_cast<LONG>(fnvxr::shared::StereoProducerDrawReplay),
        8,
        4u,
        4,
        0,
        true);
    if (runProbe(argv[1], "--require-world-stereo", false))
    {
        std::cerr << "probe accepted a draw-replay world-stereo mapping\n";
        return EXIT_FAILURE;
    }

    writeWorldStereo(
        mapping.view(),
        static_cast<LONG>(fnvxr::shared::StereoProducerEngineCenter),
        10,
        100u,
        10,
        0,
        true);
    if (!runAdvancingProbe(
            argv[1],
            mapping.view(),
            static_cast<LONG>(fnvxr::shared::StereoProducerEngineCenter),
            12,
            101u,
            11,
            1,
            true))
    {
        std::cerr << "probe rejected an advancing engine-center publication generation\n";
        return EXIT_FAILURE;
    }

    // A changed 32-bit header sequence alone is not freshness proof. The
    // generation is deliberately held stale here; the old probe accepted it.
    writeWorldStereo(
        mapping.view(),
        static_cast<LONG>(fnvxr::shared::StereoProducerEngineCenter),
        20,
        200u,
        20,
        0,
        true);
    if (runAdvancingProbe(
            argv[1],
            mapping.view(),
            static_cast<LONG>(fnvxr::shared::StereoProducerEngineCenter),
            22,
            200u,
            21,
            1,
            true,
            false))
    {
        std::cerr << "probe accepted a changed sequence with a stale stereo generation\n";
        return EXIT_FAILURE;
    }

    writeWorldStereo(
        mapping.view(),
        static_cast<LONG>(fnvxr::shared::StereoProducerEngineCenter),
        30,
        300u,
        30,
        0,
        true);
    if (runAdvancingProbe(
            argv[1],
            mapping.view(),
            static_cast<LONG>(fnvxr::shared::StereoProducerEngineCenter),
            32,
            299u,
            31,
            1,
            true,
            false))
    {
        std::cerr << "probe accepted a regressed stereo publication generation\n";
        return EXIT_FAILURE;
    }

    // Generation zero is skipped by the producer. Verify the probe accepts
    // the valid UINT64_MAX -> 1 wrap rather than falling back to signed math.
    writeWorldStereo(
        mapping.view(),
        static_cast<LONG>(fnvxr::shared::StereoProducerEngineCenter),
        40,
        (std::numeric_limits<std::uint64_t>::max)(),
        40,
        0,
        true);
    if (!runAdvancingProbe(
            argv[1],
            mapping.view(),
            static_cast<LONG>(fnvxr::shared::StereoProducerEngineCenter),
            42,
            1u,
            41,
            1,
            true))
    {
        std::cerr << "probe rejected a wrapped nonzero stereo publication generation\n";
        return EXIT_FAILURE;
    }

    std::cout << "engine-center stereo probe mapping test passed\n";
    return EXIT_SUCCESS;
}
