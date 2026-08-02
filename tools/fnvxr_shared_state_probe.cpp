#include "fnvxr_shared_state.h"
#include "../runtime/fnvxr_desktop_assist_ui_evidence.h"

#include <windows.h>

#include <array>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    constexpr std::size_t SharedVideoMappingBytes = sizeof(fnvxr::shared::SharedD3D9FrameHeader)
        + static_cast<std::size_t>(fnvxr::shared::D3D9SharedFrameMaxWidth)
            * static_cast<std::size_t>(fnvxr::shared::D3D9SharedFrameMaxHeight) * 4u;
    constexpr std::size_t SharedStereoMappingBytes = sizeof(fnvxr::shared::SharedD3D9StereoFrameHeader)
        + static_cast<std::size_t>(fnvxr::shared::D3D9SharedFrameMaxWidth)
            * static_cast<std::size_t>(fnvxr::shared::D3D9SharedFrameMaxHeight) * 4u * 2u
            * fnvxr::shared::D3D9StereoFrameSlotCount;
    constexpr std::size_t SharedDesktopAssistUiQuadMappingBytes =
        fnvxr::engine::desktopAssistUiQuadMappingBytes();

    struct MappingView
    {
        HANDLE handle = nullptr;
        void* view = nullptr;
        std::string error;

        ~MappingView()
        {
            if (view)
                UnmapViewOfFile(view);
            if (handle)
                CloseHandle(handle);
        }

        bool open(const char* name, std::size_t bytes)
        {
            handle = OpenFileMappingA(FILE_MAP_READ, FALSE, name);
            if (!handle)
            {
                error = "missing";
                return false;
            }

            view = MapViewOfFile(handle, FILE_MAP_READ, 0, 0, bytes);
            if (!view)
            {
                error = "map_failed";
                CloseHandle(handle);
                handle = nullptr;
                return false;
            }

            return true;
        }
    };

    struct Options
    {
        bool requirePlayer = false;
        bool requireRuntime = false;
        bool requireVideo = false;
        bool requireStereo = false;
        bool requireWorldStereo = false;
        bool requireEngineCenterWorldStereo = false;
        // Observe one actual shared-protocol transition rather than accepting
        // a world frame in isolation. This is the headset-free evidence that
        // a confirmed menu record was retired before a later engine-center
        // world transaction became eligible.
        bool requireUiThenEngineCenterWorld = false;
        bool requirePose = false;
        bool requireCamera = false;
        bool requireDesktopAssistReady = false;
        bool requireDesktopAssist = false;
        bool requireDesktopAssistUiQuad = false;
        bool requireAdvancing = false;
        int sampleDelayMs = 250;
        int transitionTimeoutMs = 5000;
    };

    struct PlayerStatus
    {
        bool present = false;
        bool usable = false;
        fnvxr::shared::SharedPlayerState state {};
    };

    struct StereoStatus
    {
        bool usable = false;
        bool world = false;
        bool engineCenterWorld = false;
    };

    struct StereoTransitionStatus
    {
        bool uiObserved = false;
        bool uiPixelsVerified = false;
        bool worldObservedAfterUi = false;
        bool worldPixelsVerified = false;
        bool complete = false;
        std::uint64_t samples = 0u;
        std::uint64_t uiTransactionId = 0u;
        std::uint64_t uiSourceFrame = 0u;
        std::uint64_t uiRuntimeStateSample = 0u;
        std::uint64_t uiPublicationGeneration = 0u;
        std::uint64_t uiRendererProducerEpoch = 0u;
        std::uint64_t worldTransactionId = 0u;
        std::uint64_t worldSourceFrame = 0u;
        std::uint64_t worldRuntimeStateSample = 0u;
        std::uint64_t worldPublicationGeneration = 0u;
        std::uint64_t worldRendererProducerEpoch = 0u;
    };

    struct StereoPayloadEvidence
    {
        bool layoutUsable = false;
        bool stable = false;
        std::uint64_t leftNonBlackPixels = 0u;
        std::uint64_t rightNonBlackPixels = 0u;
        std::uint64_t differentPixels = 0u;

        bool complete() const noexcept
        {
            return layoutUsable
                && stable
                && leftNonBlackPixels != 0u
                && rightNonBlackPixels != 0u
                && differentPixels != 0u;
        }
    };

    enum class CounterDomain : std::uint8_t
    {
        StrictlyIncreasing,
        NonzeroModulo32,
        NonzeroModulo64,
    };

    struct CounterStatus
    {
        bool present = false;
        bool usable = false;
        CounterDomain domain = CounterDomain::StrictlyIncreasing;
        std::uint64_t value = 0;
    };

    struct DesktopAssistStatus
    {
        bool ready = false;
        bool poseApplied = false;
    };

    struct DesktopAssistUiQuadStatus
    {
        bool present = false;
        bool stable = false;
        bool headerComplete = false;
        bool pixelsHashMatch = false;
        bool nonBlackCountMatch = false;
        bool usable = false;
        fnvxr::shared::SharedDesktopAssistUiQuadHeader header {};
        LONG sequenceBefore = 0;
        LONG sequenceAfter = 0;
    };

    bool hasArg(const std::vector<std::string>& args, const char* name)
    {
        for (const std::string& arg : args)
        {
            if (arg == name)
                return true;
        }
        return false;
    }

    int argInt(const std::vector<std::string>& args, const char* name, int defaultValue)
    {
        for (std::size_t i = 0; i + 1 < args.size(); ++i)
        {
            if (args[i] != name)
                continue;

            char* end = nullptr;
            const long parsed = std::strtol(args[i + 1].c_str(), &end, 10);
            return end != args[i + 1].c_str() ? static_cast<int>(parsed) : defaultValue;
        }
        return defaultValue;
    }

    void printUsage()
    {
        std::cout
            << "usage: fnvxr_shared_state_probe [--require-player] [--require-runtime]\n"
            << "                                [--require-video]\n"
            << "                                [--require-stereo] [--require-world-stereo]\n"
            << "                                [--require-engine-center-world-stereo]\n"
            << "                                [--require-ui-then-engine-center-world]\n"
            << "                                [--require-pose] [--require-camera]\n"
            << "                                [--require-desktop-assist-ready]\n"
            << "                                [--require-desktop-assist]\n"
            << "                                [--require-desktop-assist-ui-quad]\n"
            << "                                [--require-advancing] [--sample-delay-ms <ms>]\n"
            << "                                [--transition-timeout-ms <ms>]\n";
    }

    bool finiteArray(const float* values, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            if (!(values[i] == values[i]) || values[i] > 3.4e38f || values[i] < -3.4e38f)
                return false;
        }
        return true;
    }

    bool finite3(const float values[3])
    {
        return finiteArray(values, 3);
    }

    bool finite4(const float values[4])
    {
        return finiteArray(values, 4);
    }

    bool normalizedQuat(const float values[4])
    {
        if (!finite4(values))
            return false;
        const double normSquared = static_cast<double>(values[0]) * values[0]
            + static_cast<double>(values[1]) * values[1]
            + static_cast<double>(values[2]) * values[2]
            + static_cast<double>(values[3]) * values[3];
        return normSquared >= 0.98 * 0.98 && normSquared <= 1.02 * 1.02;
    }

    bool poseAimFieldsUsable(const fnvxr::shared::SharedVrPoseState& state)
    {
        const bool leftAimCurrent =
            (state.trackingFlags & fnvxr::shared::VrPoseTrackingLeftAimCurrent) != 0;
        const bool rightAimCurrent =
            (state.trackingFlags & fnvxr::shared::VrPoseTrackingRightAimCurrent) != 0;
        return (!leftAimCurrent || (normalizedQuat(state.leftAimRot) && finite3(state.leftAimPos)))
            && (!rightAimCurrent || (normalizedQuat(state.rightAimRot) && finite3(state.rightAimPos)));
    }

    template <class T>
    bool copyTornChecked(const void* source, T& out, LONG& sequenceBefore, LONG& sequenceAfter)
    {
        const auto* typed = static_cast<const T*>(source);
        sequenceBefore = typed->sequence;
        std::memcpy(&out, typed, sizeof(T));
        sequenceAfter = typed->sequence;
        return (sequenceBefore & 1) == 0 && sequenceBefore == sequenceAfter;
    }

    template <class T>
    bool copyFrameHeaderChecked(const void* source, T& out, LONG& sequenceBefore, LONG& sequenceAfter)
    {
        const auto* typed = static_cast<const T*>(source);
        if (typed->writing != 0)
            return false;
        sequenceBefore = typed->sequence;
        MemoryBarrier();
        std::memcpy(&out, typed, sizeof(T));
        MemoryBarrier();
        sequenceAfter = typed->sequence;
        return typed->writing == 0
            && out.writing == 0
            && sequenceBefore == sequenceAfter;
    }

    bool copyDesktopAssistUiQuadHeaderChecked(
        const void* source,
        fnvxr::shared::SharedDesktopAssistUiQuadHeader& out,
        LONG& sequenceBefore,
        LONG& sequenceAfter)
    {
        const auto* header = static_cast<const fnvxr::shared::SharedDesktopAssistUiQuadHeader*>(source);
        if (header->writing != 0)
            return false;
        sequenceBefore = header->sequence;
        MemoryBarrier();
        std::memcpy(&out, header, sizeof(out));
        MemoryBarrier();
        sequenceAfter = header->sequence;
        return header->writing == 0
            && out.writing == 0
            && sequenceBefore == sequenceAfter
            && out.sequence == sequenceBefore;
    }

    void jsonBool(const char* name, bool value, bool trailing = true)
    {
        std::cout << "\"" << name << "\":" << (value ? "true" : "false");
        if (trailing)
            std::cout << ",";
        std::cout << "\n";
    }

    void jsonNumber(const char* name, std::uint64_t value, bool trailing = true)
    {
        std::cout << "\"" << name << "\":" << value;
        if (trailing)
            std::cout << ",";
        std::cout << "\n";
    }

    void jsonSigned(const char* name, std::int64_t value, bool trailing = true)
    {
        std::cout << "\"" << name << "\":" << value;
        if (trailing)
            std::cout << ",";
        std::cout << "\n";
    }

    void jsonFloatArray(const char* name, const float* values, int count, bool trailing = true)
    {
        std::cout << "\"" << name << "\":[";
        for (int i = 0; i < count; ++i)
        {
            if (i != 0)
                std::cout << ",";
            std::cout << values[i];
        }
        std::cout << "]";
        if (trailing)
            std::cout << ",";
        std::cout << "\n";
    }

    void jsonFloatArray3(const char* name, const float values[3], bool trailing = true)
    {
        jsonFloatArray(name, values, 3, trailing);
    }

    PlayerStatus printPlayer(
        const char* jsonName,
        const char* mappingName,
        std::uint32_t expectedMagic,
        std::uint32_t expectedVersion)
    {
        PlayerStatus status;
        MappingView mapping;
        std::cout << "\"" << jsonName << "\":{\n";
        if (!mapping.open(mappingName, sizeof(fnvxr::shared::SharedPlayerState)))
        {
            jsonBool("present", false, false);
            std::cout << "},\n";
            return status;
        }

        fnvxr::shared::SharedPlayerState state {};
        LONG sequenceBefore = 0;
        LONG sequenceAfter = 0;
        const bool stable = copyTornChecked(mapping.view, state, sequenceBefore, sequenceAfter);
        const bool magicOk = state.magic == expectedMagic;
        const bool versionOk = state.version == expectedVersion;
        const bool playerValid = (state.flags & fnvxr::shared::PlayerSharedFlagPlayerNodeValid) != 0;
        const bool cellKnown = (state.flags & fnvxr::shared::PlayerSharedFlagCellKnown) != 0;
        const bool usable = stable && magicOk && versionOk && playerValid && cellKnown && state.currentCellFormId != 0
            && finite3(state.playerWorldPos);
        status.present = true;
        status.usable = usable;
        status.state = state;

        jsonBool("present", true);
        jsonBool("stable", stable);
        jsonBool("magicOk", magicOk);
        jsonBool("versionOk", versionOk);
        jsonSigned("sequenceBefore", sequenceBefore);
        jsonSigned("sequenceAfter", sequenceAfter);
        jsonNumber("frame", state.frame);
        jsonNumber("flags", state.flags);
        jsonBool("playerNodeValid", playerValid);
        jsonBool("cameraValid", (state.flags & fnvxr::shared::PlayerSharedFlagCameraValid) != 0);
        jsonBool("cellKnown", cellKnown);
        jsonBool("gameplay", (state.flags & fnvxr::shared::PlayerSharedFlagGameplay) != 0);
        jsonBool("weaponOut", (state.flags & fnvxr::shared::PlayerSharedFlagWeaponOut) != 0);
        jsonBool("weaponClassKnown", (state.flags & fnvxr::shared::PlayerSharedFlagWeaponClassKnown) != 0);
        jsonNumber("weaponClass", state.reserved[fnvxr::shared::PlayerSharedWeaponClassReservedIndex]);
        jsonNumber("equippedWeaponFormId", state.reserved[fnvxr::shared::PlayerSharedEquippedWeaponFormIdReservedIndex]);
        jsonNumber("equippedFavoriteSlot", state.reserved[fnvxr::shared::PlayerSharedEquippedFavoriteSlotReservedIndex]);
        jsonNumber("currentCellFormId", state.currentCellFormId);
        jsonNumber("playerAddress", state.playerAddress);
        jsonNumber("playerNodeAddress", state.playerNodeAddress);
        jsonNumber("cameraNodeAddress", state.cameraNodeAddress);
        jsonFloatArray3("playerWorldPos", state.playerWorldPos);
        jsonFloatArray("playerWorldRot", state.playerWorldRot, 9);
        jsonFloatArray3("cameraWorldPos", state.cameraWorldPos);
        jsonBool("usable", usable, false);
        std::cout << "},\n";
        return status;
    }

    bool counterAdvanced(const CounterStatus& before, const CounterStatus& after)
    {
        if (!before.usable || !after.usable || before.domain != after.domain)
            return false;
        if (before.domain == CounterDomain::StrictlyIncreasing)
            return after.value > before.value;
        if (before.domain == CounterDomain::NonzeroModulo32)
        {
            const std::uint32_t older = static_cast<std::uint32_t>(before.value);
            const std::uint32_t newer = static_cast<std::uint32_t>(after.value);
            const std::uint32_t delta = newer - older;
            return newer != 0u && delta != 0u && delta < 0x80000000u;
        }

        // publicationGeneration is a nonzero modulo-2^64 ABA guard. It is
        // deliberately independent of the wrapped 32-bit display sequence:
        // a fresh stereo publication must move this generation forward even
        // when its signed LONG sequence looks plausible after a wrap.
        const std::uint64_t older = before.value;
        const std::uint64_t newer = after.value;
        const std::uint64_t delta = newer - older;
        return newer != 0u
            && delta != 0u
            && delta < (std::uint64_t{1} << 63u);
    }

    bool copyStereoHeaderChecked(
        const void* source,
        fnvxr::shared::SharedD3D9StereoFrameHeader& out,
        LONG& sequenceBefore,
        LONG& sequenceAfter,
        std::uint64_t& publicationGeneration)
    {
        publicationGeneration = 0u;
        const auto* header = static_cast<
            const fnvxr::shared::SharedD3D9StereoFrameHeader*>(source);
        if (!header)
            return false;

        const bool headerStable = copyFrameHeaderChecked(
            source,
            out,
            sequenceBefore,
            sequenceAfter);
        // This tool maps the evidence record read-only.  The producer already
        // brackets both the 64-bit generation and the header metadata with
        // `writing` plus the frame sequence, so a successful seqlock-style
        // copy is the read-only snapshot boundary.  Do not use an interlocked
        // read here: Windows interlocked operations require write access even
        // when their value does not change.
        publicationGeneration = static_cast<std::uint64_t>(
            out.publicationGeneration);
        return headerStable && publicationGeneration != 0u;
    }

    bool stereoPayloadLayoutUsable(const fnvxr::shared::SharedD3D9StereoFrameHeader& header)
    {
        if (header.publishedSlot < 0
            || header.publishedSlot >= static_cast<LONG>(fnvxr::shared::D3D9StereoFrameSlotCount)
            || header.width <= 0
            || header.height <= 0
            || header.pitchBytes != header.width * 4)
        {
            return false;
        }
        constexpr std::uint64_t slotBytes =
            static_cast<std::uint64_t>(fnvxr::shared::D3D9SharedFrameMaxWidth)
            * fnvxr::shared::D3D9SharedFrameMaxHeight * 4u * 2u;
        const std::uint64_t planeBytes =
            static_cast<std::uint64_t>(header.pitchBytes) * header.height;
        const std::uint64_t expectedLeft = sizeof(header)
            + static_cast<std::uint64_t>(header.publishedSlot) * slotBytes;
        const std::uint64_t expectedRight = expectedLeft + planeBytes;
        return header.leftPayloadOffset == expectedLeft
            && header.rightPayloadOffset == expectedRight
            && expectedRight + planeBytes <= expectedLeft + slotBytes
            && expectedRight + planeBytes <= header.totalMappingBytes;
    }

    bool stereoProducerCarriesSameTransactionEyes(LONG producerMode)
    {
        return producerMode >= 0
            && fnvxr::shared::stereoProducerCarriesSameTransactionEyes(
                static_cast<std::uint32_t>(producerMode));
    }

    StereoPayloadEvidence inspectStereoPayload(
        const void* mappingView,
        const fnvxr::shared::SharedD3D9StereoFrameHeader& header,
        LONG sequence) noexcept
    {
        StereoPayloadEvidence evidence;
        if (!mappingView
            || header.sequence != sequence
            || !stereoPayloadLayoutUsable(header))
        {
            return evidence;
        }

        const std::size_t planeBytes = static_cast<std::size_t>(
            header.pitchBytes) * static_cast<std::size_t>(header.height);
        const auto* bytes = static_cast<const std::uint8_t*>(mappingView);
        const auto* left = bytes + header.leftPayloadOffset;
        const auto* right = bytes + header.rightPayloadOffset;
        evidence.layoutUsable = true;
        for (std::size_t offset = 0u; offset < planeBytes; offset += 4u)
        {
            const bool leftNonBlack = left[offset] != 0u
                || left[offset + 1u] != 0u
                || left[offset + 2u] != 0u;
            const bool rightNonBlack = right[offset] != 0u
                || right[offset + 1u] != 0u
                || right[offset + 2u] != 0u;
            if (leftNonBlack)
                ++evidence.leftNonBlackPixels;
            if (rightNonBlack)
                ++evidence.rightNonBlackPixels;
            if (left[offset] != right[offset]
                || left[offset + 1u] != right[offset + 1u]
                || left[offset + 2u] != right[offset + 2u])
            {
                ++evidence.differentPixels;
            }
        }

        MemoryBarrier();
        const auto* live = static_cast<
            const fnvxr::shared::SharedD3D9StereoFrameHeader*>(mappingView);
        evidence.stable = live->writing == 0
            && live->sequence == sequence
            && live->publishedSlot == header.publishedSlot;
        return evidence;
    }

    struct StereoFrameObservation
    {
        bool present = false;
        bool monoUiQuad = false;
        bool engineCenterWorld = false;
        std::uint64_t transactionId = 0u;
        std::uint64_t sourceFrame = 0u;
        std::uint64_t runtimeStateSample = 0u;
        std::uint64_t publicationGeneration = 0u;
        std::uint64_t rendererProducerEpoch = 0u;
    };

    bool stereoTransactionTokenMatches(
        const fnvxr::shared::SharedD3D9StereoFrameHeader& header) noexcept
    {
        std::uint32_t expected = static_cast<std::uint32_t>(
            header.transactionId);
        if (expected == 0u)
            expected = 1u;
        return fnvxr::shared::sequencedValueBits(header.renderPairSequence)
            == expected;
    }

    bool nonzeroGenerationFollows(
        std::uint64_t newer,
        std::uint64_t older) noexcept
    {
        const std::uint64_t delta = newer - older;
        return newer != 0u
            && delta != 0u
            && delta < (std::uint64_t{1} << 63u);
    }

    bool readStereoPresentationObservation(
        StereoFrameObservation& observation)
    {
        observation = {};
        MappingView mapping;
        if (!mapping.open(
                fnvxr::shared::D3D9StereoFrameSharedMappingName,
                SharedStereoMappingBytes))
        {
            return false;
        }
        observation.present = true;

        fnvxr::shared::SharedD3D9StereoFrameHeader header {};
        LONG sequenceBefore = 0;
        LONG sequenceAfter = 0;
        std::uint64_t publicationGeneration = 0u;
        const bool stable = copyStereoHeaderChecked(
            mapping.view,
            header,
            sequenceBefore,
            sequenceAfter,
            publicationGeneration);
        const bool dimensionsOk = header.width > 0
            && header.height > 0
            && header.width <= static_cast<LONG>(
                fnvxr::shared::D3D9SharedFrameMaxWidth)
            && header.height <= static_cast<LONG>(
                fnvxr::shared::D3D9SharedFrameMaxHeight)
            && header.pitchBytes == header.width * 4;
        const bool common = stable
            && header.magic == fnvxr::shared::D3D9StereoFrameSharedMagic
            && header.version == fnvxr::shared::D3D9StereoFrameSharedVersion
            && header.headerBytes == sizeof(header)
            && header.totalMappingBytes == SharedStereoMappingBytes
            && dimensionsOk
            && header.rendererProducerEpoch != 0u
            && header.producerProcessId != 0u
            && publicationGeneration != 0u
            && header.transactionId != 0u
            && header.sourceFrame != 0u
            && header.runtimeStateSample != 0u
            && stereoTransactionTokenMatches(header)
            && header.poseValid != 0
            && fnvxr::shared::sequencedValueIsPublished(
                header.poseSequence)
            && header.renderedDisplayTime > 0
            && header.referenceSpaceGeneration != 0u
            && header.producerEpoch != 0u
            && stereoPayloadLayoutUsable(header);
        const StereoPayloadEvidence payload = common
            ? inspectStereoPayload(mapping.view, header, sequenceAfter)
            : StereoPayloadEvidence {};

        observation.transactionId = header.transactionId;
        observation.sourceFrame = header.sourceFrame;
        observation.runtimeStateSample = header.runtimeStateSample;
        observation.publicationGeneration = publicationGeneration;
        observation.rendererProducerEpoch = header.rendererProducerEpoch;
        observation.monoUiQuad = common
            && header.producerMode == static_cast<LONG>(
                fnvxr::shared::StereoProducerMonoUiQuad)
            && header.separated == 0
            && header.worldCandidate == 0
            && header.uiActive != 0
            && payload.layoutUsable
            && payload.stable
            && payload.leftNonBlackPixels != 0u
            && payload.rightNonBlackPixels != 0u
            && payload.differentPixels == 0u;
        observation.engineCenterWorld = common
            && header.producerMode == static_cast<LONG>(
                fnvxr::shared::StereoProducerEngineCenter)
            && fnvxr::shared::stereoProducerCarriesSameTransactionEyes(
                fnvxr::shared::StereoProducerEngineCenter)
            && header.separated != 0
            && header.worldCandidate != 0
            && header.uiActive == 0
            && payload.complete();
        return true;
    }

    StereoTransitionStatus observeUiThenEngineCenterWorld(
        int requestedTimeoutMs)
    {
        StereoTransitionStatus status {};
        const int timeoutMs = requestedTimeoutMs < 0
            ? 0
            : (requestedTimeoutMs > 60000 ? 60000 : requestedTimeoutMs);
        const ULONGLONG startedAt = GetTickCount64();
        for (;;)
        {
            StereoFrameObservation observation {};
            if (readStereoPresentationObservation(observation))
            {
                ++status.samples;
                if (observation.monoUiQuad)
                {
                    status.uiObserved = true;
                    status.uiPixelsVerified = true;
                    status.uiTransactionId = observation.transactionId;
                    status.uiSourceFrame = observation.sourceFrame;
                    status.uiRuntimeStateSample =
                        observation.runtimeStateSample;
                    status.uiPublicationGeneration =
                        observation.publicationGeneration;
                    status.uiRendererProducerEpoch =
                        observation.rendererProducerEpoch;
                }
                else if (status.uiObserved
                    && observation.engineCenterWorld)
                {
                    status.worldObservedAfterUi = true;
                    status.worldPixelsVerified = true;
                    status.worldTransactionId = observation.transactionId;
                    status.worldSourceFrame = observation.sourceFrame;
                    status.worldRuntimeStateSample =
                        observation.runtimeStateSample;
                    status.worldPublicationGeneration =
                        observation.publicationGeneration;
                    status.worldRendererProducerEpoch =
                        observation.rendererProducerEpoch;
                    status.complete = observation.transactionId
                            > status.uiTransactionId
                        && observation.sourceFrame > status.uiSourceFrame
                        && observation.rendererProducerEpoch
                            == status.uiRendererProducerEpoch
                        && nonzeroGenerationFollows(
                            observation.publicationGeneration,
                            status.uiPublicationGeneration);
                    if (status.complete)
                        return status;
                }
            }

            if (GetTickCount64() - startedAt
                >= static_cast<ULONGLONG>(timeoutMs))
            {
                return status;
            }
            Sleep(25u);
        }
    }

    StereoTransitionStatus printUiThenEngineCenterWorldTransition(
        const Options& options)
    {
        const StereoTransitionStatus status =
            options.requireUiThenEngineCenterWorld
                ? observeUiThenEngineCenterWorld(options.transitionTimeoutMs)
                : StereoTransitionStatus {};
        std::cout << "\"uiThenEngineCenterWorld\":{\n";
        jsonBool("required", options.requireUiThenEngineCenterWorld);
        jsonNumber("timeoutMs", static_cast<std::uint64_t>(
            options.transitionTimeoutMs < 0
                ? 0
                : (options.transitionTimeoutMs > 60000
                    ? 60000
                    : options.transitionTimeoutMs)));
        jsonNumber("samples", status.samples);
        jsonBool("uiObserved", status.uiObserved);
        jsonBool("uiPixelsVerified", status.uiPixelsVerified);
        jsonNumber("uiTransactionId", status.uiTransactionId);
        jsonNumber("uiSourceFrame", status.uiSourceFrame);
        jsonNumber("uiRuntimeStateSample", status.uiRuntimeStateSample);
        jsonNumber("uiPublicationGeneration", status.uiPublicationGeneration);
        jsonNumber("uiRendererProducerEpoch", status.uiRendererProducerEpoch);
        jsonBool("worldObservedAfterUi", status.worldObservedAfterUi);
        jsonBool("worldPixelsVerified", status.worldPixelsVerified);
        jsonNumber("worldTransactionId", status.worldTransactionId);
        jsonNumber("worldSourceFrame", status.worldSourceFrame);
        jsonNumber("worldRuntimeStateSample", status.worldRuntimeStateSample);
        jsonNumber("worldPublicationGeneration", status.worldPublicationGeneration);
        jsonNumber("worldRendererProducerEpoch", status.worldRendererProducerEpoch);
        jsonBool("complete", status.complete, false);
        std::cout << "},\n";
        return status;
    }

    CounterStatus readPlayerCounter(const char* mappingName, std::uint32_t expectedMagic, std::uint32_t expectedVersion)
    {
        CounterStatus status;
        MappingView mapping;
        if (!mapping.open(mappingName, sizeof(fnvxr::shared::SharedPlayerState)))
            return status;

        fnvxr::shared::SharedPlayerState state {};
        LONG sequenceBefore = 0;
        LONG sequenceAfter = 0;
        const bool stable = copyTornChecked(mapping.view, state, sequenceBefore, sequenceAfter);
        const bool magicOk = state.magic == expectedMagic;
        const bool versionOk = state.version == expectedVersion;
        const bool playerValid = (state.flags & fnvxr::shared::PlayerSharedFlagPlayerNodeValid) != 0;
        const bool cellKnown = (state.flags & fnvxr::shared::PlayerSharedFlagCellKnown) != 0;
        status.present = true;
        status.usable = stable && magicOk && versionOk && playerValid && cellKnown && state.currentCellFormId != 0
            && finite3(state.playerWorldPos);
        status.value = state.frame;
        return status;
    }

    CounterStatus readRuntimeCounter()
    {
        CounterStatus status;
        MappingView mapping;
        if (!mapping.open("Local\\FNVXR_Runtime_State", sizeof(fnvxr::shared::SharedRuntimeState)))
            return status;

        fnvxr::shared::SharedRuntimeState state {};
        LONG sequenceBefore = 0;
        LONG sequenceAfter = 0;
        const bool stable = copyTornChecked(mapping.view, state, sequenceBefore, sequenceAfter);
        status.present = true;
        status.usable = stable && state.magic == fnvxr::shared::RuntimeSharedMagic
            && state.version == fnvxr::shared::RuntimeSharedVersion;
        status.value = state.frame;
        return status;
    }

    CounterStatus readCameraCounter()
    {
        CounterStatus status;
        MappingView mapping;
        if (!mapping.open("Local\\FNVXR_Camera_State", sizeof(fnvxr::shared::SharedCameraState)))
            return status;

        fnvxr::shared::SharedCameraState state {};
        LONG sequenceBefore = 0;
        LONG sequenceAfter = 0;
        const bool stable = copyTornChecked(mapping.view, state, sequenceBefore, sequenceAfter);
        status.present = true;
        status.usable = stable && state.magic == fnvxr::shared::CameraSharedMagic
            && state.version == fnvxr::shared::CameraSharedVersion && state.active != 0 && finite3(state.worldPos);
        status.value = state.frame;
        return status;
    }

    CounterStatus readDesktopAssistCounter(bool requireCameraPoseApplied = true)
    {
        CounterStatus status;
        MappingView mapping;
        if (!mapping.open(
                fnvxr::shared::DesktopAssistSharedMappingName,
                sizeof(fnvxr::shared::SharedDesktopAssistState)))
        {
            return status;
        }

        fnvxr::shared::SharedDesktopAssistState state {};
        LONG sequenceBefore = 0;
        LONG sequenceAfter = 0;
        const bool stable = copyTornChecked(mapping.view, state, sequenceBefore, sequenceAfter);
        constexpr std::uint32_t readyFlags =
            fnvxr::shared::DesktopAssistFlagLeaseCurrent
            | fnvxr::shared::DesktopAssistFlagCameraHookInstalled
            | fnvxr::shared::DesktopAssistFlagFirstPerson
            | fnvxr::shared::DesktopAssistFlagPlayerTransformValid
            | fnvxr::shared::DesktopAssistFlagCameraLocalTransformValid
            | fnvxr::shared::DesktopAssistFlagBodyRootTransformValid;
        const bool ready = stable
            && state.magic == fnvxr::shared::DesktopAssistSharedMagic
            && state.version == fnvxr::shared::DesktopAssistSharedVersion
            && (state.flags & readyFlags) == readyFlags
            && finite3(state.playerWorldPos)
            && finiteArray(state.playerWorldRot, 9)
            && finite3(state.cameraLocalPos)
            && finiteArray(state.cameraLocalRot, 9)
            && state.bodyRootAddress != 0u
            && finite3(state.bodyRootWorldPos)
            && finiteArray(state.bodyRootWorldRot, 9);
        status.present = true;
        status.usable = ready
            && (!requireCameraPoseApplied
                || ((state.flags & fnvxr::shared::DesktopAssistFlagCameraPoseApplied) != 0u
                    && state.poseSequence != 0u
                    && state.poseProducerEpoch != 0u));
        status.value = state.frame;
        return status;
    }

    DesktopAssistUiQuadStatus readDesktopAssistUiQuadStatus()
    {
        DesktopAssistUiQuadStatus status;
        MappingView mapping;
        if (!mapping.open(
                fnvxr::shared::DesktopAssistUiQuadSharedMappingName,
                SharedDesktopAssistUiQuadMappingBytes))
        {
            return status;
        }

        status.present = true;
        const auto* source = static_cast<const fnvxr::shared::SharedDesktopAssistUiQuadHeader*>(
            mapping.view);
        status.stable = copyDesktopAssistUiQuadHeaderChecked(
            mapping.view,
            status.header,
            status.sequenceBefore,
            status.sequenceAfter);
        status.headerComplete = status.stable
            && fnvxr::engine::desktopAssistUiQuadHeaderIsComplete(status.header);
        if (!status.headerComplete)
            return status;

        std::size_t payloadBytes = 0u;
        if (!fnvxr::engine::desktopAssistUiQuadPayloadLayoutIsValid(
                status.header,
                &payloadBytes))
        {
            return status;
        }

        const auto* pixels = reinterpret_cast<const std::uint8_t*>(mapping.view)
            + status.header.headerBytes;
        std::uint32_t nonBlack = 0u;
        const std::uint32_t hash = fnvxr::engine::desktopAssistUiQuadPixelHash(
            pixels,
            payloadBytes,
            &nonBlack);
        MemoryBarrier();
        const LONG finalSequence = source->sequence;
        const LONG finalWriting = source->writing;
        status.stable = status.stable
            && finalWriting == 0
            && finalSequence == status.sequenceAfter;
        status.pixelsHashMatch = status.stable && hash == status.header.pixelHash;
        status.nonBlackCountMatch = status.stable
            && nonBlack == status.header.nonBlackSampleCount;
        status.usable = status.headerComplete
            && status.pixelsHashMatch
            && status.nonBlackCountMatch;
        return status;
    }

    CounterStatus readDesktopAssistUiQuadCounter()
    {
        const DesktopAssistUiQuadStatus uiQuad = readDesktopAssistUiQuadStatus();
        CounterStatus status;
        status.present = uiQuad.present;
        status.usable = uiQuad.usable;
        status.value = uiQuad.header.captureOrdinal;
        return status;
    }

    CounterStatus readVideoCounter()
    {
        CounterStatus status;
        MappingView mapping;
        if (!mapping.open("Local\\FNVXR_D3D9_Frame_v1", SharedVideoMappingBytes))
            return status;

        fnvxr::shared::SharedD3D9FrameHeader header {};
        LONG sequenceBefore = 0;
        LONG sequenceAfter = 0;
        const bool stable = copyFrameHeaderChecked(
            mapping.view,
            header,
            sequenceBefore,
            sequenceAfter);
        const bool dimensionsOk = header.width > 0 && header.height > 0
            && header.width <= static_cast<LONG>(fnvxr::shared::D3D9SharedFrameMaxWidth)
            && header.height <= static_cast<LONG>(fnvxr::shared::D3D9SharedFrameMaxHeight)
            && header.pitchBytes >= header.width * 4;
        status.present = true;
        status.usable = stable && header.magic == fnvxr::shared::D3D9FrameSharedMagic && dimensionsOk;
        status.value = static_cast<std::uint64_t>(header.sequence);
        return status;
    }

    CounterStatus readStereoCounter(
        bool requireWorld,
        bool requireEngineCenterWorld = false)
    {
        CounterStatus status;
        MappingView mapping;
        if (!mapping.open(fnvxr::shared::D3D9StereoFrameSharedMappingName, SharedStereoMappingBytes))
            return status;

        fnvxr::shared::SharedD3D9StereoFrameHeader header {};
        LONG sequenceBefore = 0;
        LONG sequenceAfter = 0;
        std::uint64_t publicationGeneration = 0u;
        const bool stable = copyStereoHeaderChecked(
            mapping.view,
            header,
            sequenceBefore,
            sequenceAfter,
            publicationGeneration);
        const bool dimensionsOk = header.width > 0 && header.height > 0
            && header.width <= static_cast<LONG>(fnvxr::shared::D3D9SharedFrameMaxWidth)
            && header.height <= static_cast<LONG>(fnvxr::shared::D3D9SharedFrameMaxHeight)
            && header.pitchBytes >= header.width * 4;
        const bool protocolOk = header.magic == fnvxr::shared::D3D9StereoFrameSharedMagic
            && header.version == fnvxr::shared::D3D9StereoFrameSharedVersion
            && header.headerBytes == sizeof(header)
            && header.totalMappingBytes == SharedStereoMappingBytes
            && header.rendererProducerEpoch != 0
            && header.producerProcessId != 0
            && publicationGeneration != 0u
            && header.transactionId != 0u
            && header.sourceFrame != 0u
            && header.runtimeStateSample != 0u
            && stereoPayloadLayoutUsable(header);
        const bool usable = stable && protocolOk
            && dimensionsOk
            && header.poseValid != 0
            && fnvxr::shared::sequencedValueIsPublished(header.poseSequence)
            && header.renderedDisplayTime > 0
            && header.referenceSpaceGeneration != 0
            && header.producerEpoch != 0
            && header.uiActive == 0;
        const bool coherentSameTickProducer =
            stereoProducerCarriesSameTransactionEyes(header.producerMode)
            && fnvxr::shared::sequencedValueBits(header.renderPairSequence) != 0u;
        const bool worldMetadata = usable
            && coherentSameTickProducer
            && header.worldCandidate != 0
            && header.separated != 0;
        const StereoPayloadEvidence payloadEvidence = worldMetadata && requireWorld
            ? inspectStereoPayload(mapping.view, header, sequenceAfter)
            : StereoPayloadEvidence {};
        const bool world = worldMetadata
            && (!requireWorld || payloadEvidence.complete());
        const bool engineCenterWorld = world
            && header.producerMode == static_cast<LONG>(
                fnvxr::shared::StereoProducerEngineCenter);
        status.present = true;
        status.usable = requireEngineCenterWorld
            ? engineCenterWorld
            : (requireWorld ? world : usable);
        status.domain = CounterDomain::NonzeroModulo64;
        status.value = publicationGeneration;
        return status;
    }

    CounterStatus readPoseCounter()
    {
        CounterStatus status;
        MappingView mapping;
        if (!mapping.open(fnvxr::shared::VrPoseSharedMappingName, sizeof(fnvxr::shared::SharedVrPoseState)))
            return status;

        fnvxr::shared::SharedVrPoseState state {};
        LONG sequenceBefore = 0;
        LONG sequenceAfter = 0;
        const bool stable = copyTornChecked(mapping.view, state, sequenceBefore, sequenceAfter);
        status.present = true;
        status.usable = stable && state.magic == fnvxr::shared::VrPoseSharedMagic
            && state.version == fnvxr::shared::VrPoseSharedVersion
            && state.referenceSpaceGeneration != 0
            && state.producerEpoch != 0
            && (state.trackingFlags & fnvxr::shared::VrPoseTrackingHmd) != 0
            && normalizedQuat(state.hmdRot)
            && finite3(state.hmdPos)
            && normalizedQuat(state.leftEyeRot)
            && normalizedQuat(state.rightEyeRot)
            && finite3(state.leftEyePos)
            && finite3(state.rightEyePos)
            && poseAimFieldsUsable(state);
        status.value = state.frame;
        return status;
    }

    void jsonFreshCounter(const char* name, const CounterStatus& before, const CounterStatus& after, bool trailing = true)
    {
        std::cout << "\"" << name << "\":{\n";
        jsonBool("presentBefore", before.present);
        jsonBool("presentAfter", after.present);
        jsonBool("usableBefore", before.usable);
        jsonBool("usableAfter", after.usable);
        jsonNumber("valueBefore", before.value);
        jsonNumber("valueAfter", after.value);
        jsonBool("advanced", counterAdvanced(before, after), false);
        std::cout << "}";
        if (trailing)
            std::cout << ",";
        std::cout << "\n";
    }

    bool printFreshness(const Options& options)
    {
        const CounterStatus playerBefore = readPlayerCounter(
            "Local\\FNVXR_Player_State", fnvxr::shared::PlayerSharedMagic, fnvxr::shared::PlayerSharedVersion);
        const CounterStatus runtimeBefore = readRuntimeCounter();
        const CounterStatus cameraBefore = readCameraCounter();
        const CounterStatus desktopAssistBefore = readDesktopAssistCounter(
            options.requireDesktopAssist);
        const CounterStatus desktopAssistUiQuadBefore = readDesktopAssistUiQuadCounter();
        const CounterStatus videoBefore = readVideoCounter();
        const CounterStatus stereoBefore = readStereoCounter(
            options.requireWorldStereo
                || options.requireEngineCenterWorldStereo
                || options.requireUiThenEngineCenterWorld,
            options.requireEngineCenterWorldStereo
                || options.requireUiThenEngineCenterWorld);
        const CounterStatus poseBefore = readPoseCounter();

        const int sampleDelayMs = options.sampleDelayMs < 0 ? 0 : options.sampleDelayMs;
        Sleep(static_cast<DWORD>(sampleDelayMs));

        const CounterStatus playerAfter = readPlayerCounter(
            "Local\\FNVXR_Player_State", fnvxr::shared::PlayerSharedMagic, fnvxr::shared::PlayerSharedVersion);
        const CounterStatus runtimeAfter = readRuntimeCounter();
        const CounterStatus cameraAfter = readCameraCounter();
        const CounterStatus desktopAssistAfter = readDesktopAssistCounter(
            options.requireDesktopAssist);
        const CounterStatus desktopAssistUiQuadAfter = readDesktopAssistUiQuadCounter();
        const CounterStatus videoAfter = readVideoCounter();
        const CounterStatus stereoAfter = readStereoCounter(
            options.requireWorldStereo
                || options.requireEngineCenterWorldStereo
                || options.requireUiThenEngineCenterWorld,
            options.requireEngineCenterWorldStereo
                || options.requireUiThenEngineCenterWorld);
        const CounterStatus poseAfter = readPoseCounter();

        const bool anySpecificRequirement = options.requirePlayer || options.requireRuntime
            || options.requireCamera || options.requireDesktopAssistReady
            || options.requireDesktopAssist || options.requireDesktopAssistUiQuad
            || options.requireVideo || options.requireStereo
            || options.requireWorldStereo || options.requireEngineCenterWorldStereo
            || options.requireUiThenEngineCenterWorld
            || options.requirePose;
        bool allRequiredAdvanced = true;
        if (!anySpecificRequirement || options.requirePlayer)
            allRequiredAdvanced = allRequiredAdvanced && counterAdvanced(playerBefore, playerAfter);
        if (!anySpecificRequirement || options.requireRuntime)
            allRequiredAdvanced = allRequiredAdvanced && counterAdvanced(runtimeBefore, runtimeAfter);
        if (!anySpecificRequirement || options.requireCamera)
            allRequiredAdvanced = allRequiredAdvanced && counterAdvanced(cameraBefore, cameraAfter);
        if (!anySpecificRequirement || options.requireDesktopAssistReady || options.requireDesktopAssist)
            allRequiredAdvanced = allRequiredAdvanced && counterAdvanced(desktopAssistBefore, desktopAssistAfter);
        if (!anySpecificRequirement || options.requireDesktopAssistUiQuad)
            allRequiredAdvanced = allRequiredAdvanced
                && counterAdvanced(desktopAssistUiQuadBefore, desktopAssistUiQuadAfter);
        if (!anySpecificRequirement || options.requireVideo)
            allRequiredAdvanced = allRequiredAdvanced && counterAdvanced(videoBefore, videoAfter);
        if (!anySpecificRequirement || options.requireStereo || options.requireWorldStereo
            || options.requireEngineCenterWorldStereo
            || options.requireUiThenEngineCenterWorld)
            allRequiredAdvanced = allRequiredAdvanced && counterAdvanced(stereoBefore, stereoAfter);
        if (!anySpecificRequirement || options.requirePose)
            allRequiredAdvanced = allRequiredAdvanced && counterAdvanced(poseBefore, poseAfter);

        std::cout << "\"freshness\":{\n";
        jsonSigned("sampleDelayMs", sampleDelayMs);
        jsonFreshCounter("player", playerBefore, playerAfter);
        jsonFreshCounter("runtime", runtimeBefore, runtimeAfter);
        jsonFreshCounter("camera", cameraBefore, cameraAfter);
        jsonFreshCounter("desktopAssist", desktopAssistBefore, desktopAssistAfter);
        jsonFreshCounter("desktopAssistUiQuad", desktopAssistUiQuadBefore, desktopAssistUiQuadAfter);
        jsonFreshCounter("video", videoBefore, videoAfter);
        jsonFreshCounter("stereo", stereoBefore, stereoAfter);
        jsonFreshCounter("pose", poseBefore, poseAfter);
        jsonBool("allRequiredAdvanced", allRequiredAdvanced, false);
        std::cout << "},\n";
        return allRequiredAdvanced;
    }

    bool printRuntime()
    {
        MappingView mapping;
        std::cout << "\"runtime\":{\n";
        if (!mapping.open("Local\\FNVXR_Runtime_State", sizeof(fnvxr::shared::SharedRuntimeState)))
        {
            jsonBool("present", false, false);
            std::cout << "},\n";
            return false;
        }

        fnvxr::shared::SharedRuntimeState state {};
        LONG sequenceBefore = 0;
        LONG sequenceAfter = 0;
        const bool stable = copyTornChecked(mapping.view, state, sequenceBefore, sequenceAfter);
        const bool magicOk = state.magic == fnvxr::shared::RuntimeSharedMagic;
        const bool versionOk = state.version == fnvxr::shared::RuntimeSharedVersion;
        const bool usable = stable && magicOk && versionOk;

        jsonBool("present", true);
        jsonBool("stable", stable);
        jsonBool("magicOk", magicOk);
        jsonBool("versionOk", versionOk);
        jsonSigned("sequenceBefore", sequenceBefore);
        jsonSigned("sequenceAfter", sequenceAfter);
        jsonNumber("frame", state.frame);
        jsonNumber("menuBits", state.menuBits);
        jsonNumber("phase", state.phase);
        jsonBool("uiInputAllowed", state.uiInputAllowed != 0);
        jsonBool("cameraActive", state.cameraActive != 0);
        jsonBool("showroomActive", state.showroomActive != 0);
        jsonBool("usable", usable, false);
        std::cout << "},\n";
        return usable;
    }

    bool printCamera()
    {
        MappingView mapping;
        std::cout << "\"camera\":{\n";
        if (!mapping.open("Local\\FNVXR_Camera_State", sizeof(fnvxr::shared::SharedCameraState)))
        {
            jsonBool("present", false, false);
            std::cout << "},\n";
            return false;
        }

        fnvxr::shared::SharedCameraState state {};
        LONG sequenceBefore = 0;
        LONG sequenceAfter = 0;
        const bool stable = copyTornChecked(mapping.view, state, sequenceBefore, sequenceAfter);
        const bool magicOk = state.magic == fnvxr::shared::CameraSharedMagic;
        const bool versionOk = state.version == fnvxr::shared::CameraSharedVersion;
        const bool usable = stable && magicOk && versionOk && state.active != 0 && finite3(state.worldPos);

        jsonBool("present", true);
        jsonBool("stable", stable);
        jsonBool("magicOk", magicOk);
        jsonBool("versionOk", versionOk);
        jsonSigned("sequenceBefore", sequenceBefore);
        jsonSigned("sequenceAfter", sequenceAfter);
        jsonNumber("frame", state.frame);
        jsonBool("active", state.active != 0);
        jsonBool("thirdPerson", state.thirdPerson != 0);
        jsonFloatArray3("worldPos", state.worldPos);
        jsonBool("usable", usable, false);
        std::cout << "},\n";
        return usable;
    }

    DesktopAssistStatus printDesktopAssist()
    {
        MappingView mapping;
        std::cout << "\"desktopAssist\":{\n";
        if (!mapping.open(
                fnvxr::shared::DesktopAssistSharedMappingName,
                sizeof(fnvxr::shared::SharedDesktopAssistState)))
        {
            jsonBool("present", false, false);
            std::cout << "},\n";
            return {};
        }

        fnvxr::shared::SharedDesktopAssistState state {};
        LONG sequenceBefore = 0;
        LONG sequenceAfter = 0;
        const bool stable = copyTornChecked(mapping.view, state, sequenceBefore, sequenceAfter);
        const bool magicOk = state.magic == fnvxr::shared::DesktopAssistSharedMagic;
        const bool versionOk = state.version == fnvxr::shared::DesktopAssistSharedVersion;
        constexpr std::uint32_t readyFlags =
            fnvxr::shared::DesktopAssistFlagLeaseCurrent
            | fnvxr::shared::DesktopAssistFlagCameraHookInstalled
            | fnvxr::shared::DesktopAssistFlagFirstPerson
            | fnvxr::shared::DesktopAssistFlagPlayerTransformValid
            | fnvxr::shared::DesktopAssistFlagCameraLocalTransformValid
            | fnvxr::shared::DesktopAssistFlagBodyRootTransformValid;
        const bool ready = stable && magicOk && versionOk
            && (state.flags & readyFlags) == readyFlags
            && finite3(state.playerWorldPos)
            && finiteArray(state.playerWorldRot, 9)
            && finite3(state.cameraLocalPos)
            && finiteArray(state.cameraLocalRot, 9)
            && state.bodyRootAddress != 0u
            && finite3(state.bodyRootWorldPos)
            && finiteArray(state.bodyRootWorldRot, 9);
        const bool poseApplied = ready
            && (state.flags & fnvxr::shared::DesktopAssistFlagCameraPoseApplied) != 0u
            && state.poseSequence != 0u
            && state.poseProducerEpoch != 0u;

        jsonBool("present", true);
        jsonBool("stable", stable);
        jsonBool("magicOk", magicOk);
        jsonBool("versionOk", versionOk);
        jsonSigned("sequenceBefore", sequenceBefore);
        jsonSigned("sequenceAfter", sequenceAfter);
        jsonNumber("frame", state.frame);
        jsonNumber("flags", state.flags);
        jsonNumber("poseSequence", state.poseSequence);
        jsonNumber("poseProducerEpoch", state.poseProducerEpoch);
        jsonBool("leaseCurrent", (state.flags & fnvxr::shared::DesktopAssistFlagLeaseCurrent) != 0u);
        jsonBool("cameraHookInstalled", (state.flags & fnvxr::shared::DesktopAssistFlagCameraHookInstalled) != 0u);
        jsonBool("cameraPoseApplied", (state.flags & fnvxr::shared::DesktopAssistFlagCameraPoseApplied) != 0u);
        jsonBool("firstPerson", (state.flags & fnvxr::shared::DesktopAssistFlagFirstPerson) != 0u);
        jsonBool("playerTransformValid", (state.flags & fnvxr::shared::DesktopAssistFlagPlayerTransformValid) != 0u);
        jsonBool("cameraLocalTransformValid", (state.flags & fnvxr::shared::DesktopAssistFlagCameraLocalTransformValid) != 0u);
        jsonBool("bodyRootTransformValid", (state.flags & fnvxr::shared::DesktopAssistFlagBodyRootTransformValid) != 0u);
        jsonNumber("bodyRootAddress", state.bodyRootAddress);
        jsonFloatArray3("playerWorldPos", state.playerWorldPos);
        jsonFloatArray3("cameraLocalPos", state.cameraLocalPos);
        jsonFloatArray3("bodyRootWorldPos", state.bodyRootWorldPos);
        jsonBool("ready", ready);
        jsonBool("poseApplied", poseApplied);
        jsonBool("usable", poseApplied, false);
        std::cout << "},\n";
        return { ready, poseApplied };
    }

    bool printDesktopAssistUiQuad()
    {
        const DesktopAssistUiQuadStatus status = readDesktopAssistUiQuadStatus();
        std::cout << "\"desktopAssistUiQuad\":{\n";
        if (!status.present)
        {
            jsonBool("present", false, false);
            std::cout << "},\n";
            return false;
        }

        const auto& header = status.header;
        const bool magicOk = header.magic == fnvxr::shared::DesktopAssistUiQuadSharedMagic;
        const bool versionOk = header.version == fnvxr::shared::DesktopAssistUiQuadSharedVersion;
        const bool headerBytesOk = header.headerBytes == sizeof(header);
        const bool requiredFlags = (header.flags & fnvxr::engine::DesktopAssistUiQuadRequiredFlags)
            == fnvxr::engine::DesktopAssistUiQuadRequiredFlags;
        const bool payloadLayoutOk = fnvxr::engine::desktopAssistUiQuadPayloadLayoutIsValid(header);
        const bool runtimeUiConfirmed = fnvxr::engine::desktopAssistUiQuadRuntimeConfirmedUi(header);

        jsonBool("present", true);
        jsonBool("stable", status.stable);
        jsonBool("magicOk", magicOk);
        jsonBool("versionOk", versionOk);
        jsonBool("headerBytesOk", headerBytesOk);
        jsonBool("requiredFlags", requiredFlags);
        jsonBool("payloadLayoutOk", payloadLayoutOk);
        jsonBool("runtimeUiConfirmed", runtimeUiConfirmed);
        jsonSigned("sequenceBefore", status.sequenceBefore);
        jsonSigned("sequenceAfter", status.sequenceAfter);
        jsonSigned("sequence", header.sequence);
        jsonNumber("flags", header.flags);
        jsonSigned("width", header.width);
        jsonSigned("height", header.height);
        jsonSigned("pitchBytes", header.pitchBytes);
        jsonSigned("format", header.format);
        jsonNumber("runtimeStateSample", header.runtimeStateSample);
        jsonNumber("poseFrame", header.poseFrame);
        jsonSigned("poseSequence", header.poseSequence);
        jsonNumber("runtimePhase", header.runtimePhase);
        jsonNumber("runtimeMenuBits", header.runtimeMenuBits);
        jsonNumber("pixelHash", header.pixelHash);
        jsonNumber("captureFailure", header.captureFailure);
        jsonNumber("nonBlackSampleCount", header.nonBlackSampleCount);
        jsonNumber("poseProducerEpoch", header.poseProducerEpoch);
        jsonNumber("captureOrdinal", header.captureOrdinal);
        jsonBool("headerComplete", status.headerComplete);
        jsonBool("pixelsHashMatch", status.pixelsHashMatch);
        jsonBool("nonBlackCountMatch", status.nonBlackCountMatch);
        jsonBool("usable", status.usable, false);
        std::cout << "},\n";
        return status.usable;
    }

    bool printVideo()
    {
        MappingView mapping;
        std::cout << "\"video\":{\n";
        if (!mapping.open("Local\\FNVXR_D3D9_Frame_v1", SharedVideoMappingBytes))
        {
            jsonBool("present", false, false);
            std::cout << "},\n";
            return false;
        }

        fnvxr::shared::SharedD3D9FrameHeader header {};
        LONG sequenceBefore = 0;
        LONG sequenceAfter = 0;
        const bool stable = copyFrameHeaderChecked(mapping.view, header, sequenceBefore, sequenceAfter);
        const bool magicOk = header.magic == fnvxr::shared::D3D9FrameSharedMagic;
        const bool dimensionsOk = header.width > 0 && header.height > 0
            && header.width <= static_cast<LONG>(fnvxr::shared::D3D9SharedFrameMaxWidth)
            && header.height <= static_cast<LONG>(fnvxr::shared::D3D9SharedFrameMaxHeight)
            && header.pitchBytes >= header.width * 4;
        const bool usable = magicOk && stable && dimensionsOk;

        jsonBool("present", true);
        jsonBool("stable", stable);
        jsonBool("magicOk", magicOk);
        jsonSigned("sequence", header.sequence);
        jsonSigned("width", header.width);
        jsonSigned("height", header.height);
        jsonSigned("pitchBytes", header.pitchBytes);
        jsonSigned("format", header.format);
        jsonBool("dimensionsOk", dimensionsOk);
        jsonBool("usable", usable, false);
        std::cout << "},\n";
        return usable;
    }

    StereoStatus printStereo()
    {
        MappingView mapping;
        std::cout << "\"stereo\":{\n";
        if (!mapping.open(fnvxr::shared::D3D9StereoFrameSharedMappingName, SharedStereoMappingBytes))
        {
            jsonBool("present", false, false);
            std::cout << "},\n";
            return {};
        }

        fnvxr::shared::SharedD3D9StereoFrameHeader header {};
        LONG sequenceBefore = 0;
        LONG sequenceAfter = 0;
        std::uint64_t publicationGeneration = 0u;
        const bool stable = copyStereoHeaderChecked(
            mapping.view,
            header,
            sequenceBefore,
            sequenceAfter,
            publicationGeneration);
        const bool magicOk = header.magic == fnvxr::shared::D3D9StereoFrameSharedMagic;
        const bool protocolOk = magicOk
            && header.version == fnvxr::shared::D3D9StereoFrameSharedVersion
            && header.headerBytes == sizeof(header)
            && header.totalMappingBytes == SharedStereoMappingBytes
            && header.rendererProducerEpoch != 0
            && header.producerProcessId != 0
            && publicationGeneration != 0u
            && header.transactionId != 0u
            && header.sourceFrame != 0u
            && header.runtimeStateSample != 0u
            && stereoPayloadLayoutUsable(header);
        const bool dimensionsOk = header.width > 0 && header.height > 0
            && header.width <= static_cast<LONG>(fnvxr::shared::D3D9SharedFrameMaxWidth)
            && header.height <= static_cast<LONG>(fnvxr::shared::D3D9SharedFrameMaxHeight)
            && header.pitchBytes >= header.width * 4;
        const bool usable = protocolOk && stable && dimensionsOk
            && header.poseValid != 0
            && fnvxr::shared::sequencedValueIsPublished(header.poseSequence)
            && header.renderedDisplayTime > 0
            && header.referenceSpaceGeneration != 0
            && header.producerEpoch != 0
            && header.uiActive == 0;
        const bool nativeSameFrame = header.producerMode
            == static_cast<LONG>(fnvxr::shared::StereoProducerNativeSameFrame);
        const bool singleTraversal = header.producerMode
            == static_cast<LONG>(fnvxr::shared::StereoProducerSingleTraversal);
        const bool engineCenter = header.producerMode
            == static_cast<LONG>(fnvxr::shared::StereoProducerEngineCenter);
        const bool coherentSameTickProducer =
            stereoProducerCarriesSameTransactionEyes(header.producerMode)
            && fnvxr::shared::sequencedValueBits(header.renderPairSequence) != 0u;
        const bool worldStereo = usable
            && coherentSameTickProducer
            && header.worldCandidate != 0
            && header.separated != 0;
        const StereoPayloadEvidence payloadEvidence = worldStereo
            ? inspectStereoPayload(mapping.view, header, sequenceAfter)
            : StereoPayloadEvidence {};
        const bool payloadEvidenceComplete = payloadEvidence.complete();
        const bool usableWorldStereo = worldStereo && payloadEvidenceComplete;
        const bool engineCenterWorldStereo = usableWorldStereo && engineCenter;

        jsonBool("present", true);
        jsonBool("stable", stable);
        jsonBool("magicOk", magicOk);
        jsonBool("protocolOk", protocolOk);
        jsonSigned("sequence", header.sequence);
        jsonNumber("publicationGeneration", publicationGeneration);
        jsonNumber("transactionId", header.transactionId);
        jsonNumber("sourceFrame", header.sourceFrame);
        jsonNumber("runtimeStateSample", header.runtimeStateSample);
        jsonSigned("width", header.width);
        jsonSigned("height", header.height);
        jsonSigned("pitchBytes", header.pitchBytes);
        jsonSigned("format", header.format);
        jsonBool("dimensionsOk", dimensionsOk);
        jsonBool("separated", header.separated != 0);
        jsonBool("worldCandidate", header.worldCandidate != 0);
        jsonBool("uiActive", header.uiActive != 0);
        jsonBool("poseValid", header.poseValid != 0);
        jsonSigned("poseSequence", header.poseSequence);
        jsonSigned("producerMode", header.producerMode);
        jsonSigned("renderPairSequence", header.renderPairSequence);
        jsonBool("nativeSameFrame", nativeSameFrame);
        jsonBool("singleTraversal", singleTraversal);
        jsonBool("engineCenter", engineCenter);
        jsonBool("coherentSameTickProducer", coherentSameTickProducer);
        jsonBool("payloadLayoutUsable", payloadEvidence.layoutUsable);
        jsonBool("payloadStable", payloadEvidence.stable);
        jsonNumber("leftNonBlackPixels", payloadEvidence.leftNonBlackPixels);
        jsonNumber("rightNonBlackPixels", payloadEvidence.rightNonBlackPixels);
        jsonNumber("differentPixels", payloadEvidence.differentPixels);
        jsonBool("payloadEvidenceComplete", payloadEvidenceComplete);
        jsonBool("usableForHostStereo", usable);
        jsonBool("usableWorldStereo", usableWorldStereo);
        jsonBool("usableEngineCenterWorldStereo", engineCenterWorldStereo, false);
        std::cout << "},\n";
        return { usable, usableWorldStereo, engineCenterWorldStereo };
    }

    bool printPose()
    {
        MappingView mapping;
        std::cout << "\"pose\":{\n";
        if (!mapping.open(fnvxr::shared::VrPoseSharedMappingName, sizeof(fnvxr::shared::SharedVrPoseState)))
        {
            jsonBool("present", false, false);
            std::cout << "}\n";
            return false;
        }

        fnvxr::shared::SharedVrPoseState state {};
        LONG sequenceBefore = 0;
        LONG sequenceAfter = 0;
        const bool stable = copyTornChecked(mapping.view, state, sequenceBefore, sequenceAfter);
        const bool magicOk = state.magic == fnvxr::shared::VrPoseSharedMagic;
        const bool versionOk = state.version == fnvxr::shared::VrPoseSharedVersion;
        const bool aimFieldsUsable = poseAimFieldsUsable(state);
        const bool hmdTracked =
            (state.trackingFlags & fnvxr::shared::VrPoseTrackingHmd) != 0;
        const bool usable = stable && magicOk && versionOk
            && state.referenceSpaceGeneration != 0
            && state.producerEpoch != 0
            && hmdTracked
            && normalizedQuat(state.hmdRot)
            && finite3(state.hmdPos)
            && normalizedQuat(state.leftEyeRot)
            && normalizedQuat(state.rightEyeRot)
            && finite3(state.leftEyePos)
            && finite3(state.rightEyePos)
            && aimFieldsUsable;
        const bool leftAimActive =
            (state.trackingFlags & fnvxr::shared::VrPoseTrackingLeftAimActive) != 0;
        const bool rightAimActive =
            (state.trackingFlags & fnvxr::shared::VrPoseTrackingRightAimActive) != 0;
        const bool leftAimCurrent =
            (state.trackingFlags & fnvxr::shared::VrPoseTrackingLeftAimCurrent) != 0;
        const bool rightAimCurrent =
            (state.trackingFlags & fnvxr::shared::VrPoseTrackingRightAimCurrent) != 0;

        jsonBool("present", true);
        jsonBool("stable", stable);
        jsonBool("magicOk", magicOk);
        jsonBool("versionOk", versionOk);
        jsonSigned("sequenceBefore", sequenceBefore);
        jsonSigned("sequenceAfter", sequenceAfter);
        jsonNumber("frame", state.frame);
        jsonNumber("referenceSpaceGeneration", state.referenceSpaceGeneration);
        jsonNumber("producerEpoch", state.producerEpoch);
        jsonNumber("trackingFlags", state.trackingFlags);
        jsonBool("hmdTracked", hmdTracked);
        jsonBool("leftAimActive", leftAimActive);
        jsonBool("rightAimActive", rightAimActive);
        jsonBool("leftAimCurrent", leftAimCurrent);
        jsonBool("rightAimCurrent", rightAimCurrent);
        jsonBool("aimFieldsUsable", aimFieldsUsable);
        jsonFloatArray("hmdRot", state.hmdRot, 4);
        jsonFloatArray3("hmdPos", state.hmdPos);
        jsonFloatArray("leftGripRot", state.leftRot, 4);
        jsonFloatArray3("leftPos", state.leftPos);
        jsonFloatArray("rightGripRot", state.rightRot, 4);
        jsonFloatArray3("rightPos", state.rightPos);
        jsonFloatArray("leftAimRot", state.leftAimRot, 4);
        jsonFloatArray3("leftAimPos", state.leftAimPos);
        jsonFloatArray("rightAimRot", state.rightAimRot, 4);
        jsonFloatArray3("rightAimPos", state.rightAimPos);
        jsonBool("usable", usable, false);
        std::cout << "}\n";
        return usable;
    }
}

int main(int argc, char** argv)
{
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i)
        args.emplace_back(argv[i]);

    if (hasArg(args, "--help") || hasArg(args, "-h"))
    {
        printUsage();
        return 0;
    }

    Options options;
    options.requirePlayer = hasArg(args, "--require-player");
    options.requireRuntime = hasArg(args, "--require-runtime");
    options.requireVideo = hasArg(args, "--require-video");
    options.requireStereo = hasArg(args, "--require-stereo");
    options.requireWorldStereo = hasArg(args, "--require-world-stereo");
    options.requireEngineCenterWorldStereo = hasArg(
        args,
        "--require-engine-center-world-stereo");
    options.requireUiThenEngineCenterWorld = hasArg(
        args,
        "--require-ui-then-engine-center-world");
    options.requirePose = hasArg(args, "--require-pose");
    options.requireCamera = hasArg(args, "--require-camera");
    options.requireDesktopAssistReady = hasArg(args, "--require-desktop-assist-ready");
    options.requireDesktopAssist = hasArg(args, "--require-desktop-assist");
    options.requireDesktopAssistUiQuad = hasArg(args, "--require-desktop-assist-ui-quad");
    options.requireAdvancing = hasArg(args, "--require-advancing");
    options.sampleDelayMs = argInt(args, "--sample-delay-ms", options.sampleDelayMs);
    options.transitionTimeoutMs = argInt(
        args,
        "--transition-timeout-ms",
        options.transitionTimeoutMs);

    std::cout << "{\n";
    const PlayerStatus player = printPlayer(
        "player",
        "Local\\FNVXR_Player_State",
        fnvxr::shared::PlayerSharedMagic,
        fnvxr::shared::PlayerSharedVersion);
    const bool runtime = printRuntime();
    const bool camera = printCamera();
    const DesktopAssistStatus desktopAssist = printDesktopAssist();
    const bool desktopAssistUiQuad = printDesktopAssistUiQuad();
    const bool video = printVideo();
    const StereoStatus stereo = printStereo();
    const StereoTransitionStatus uiThenEngineCenterWorld =
        printUiThenEngineCenterWorldTransition(options);
    bool advancing = true;
    if (options.requireAdvancing)
        advancing = printFreshness(options);
    const bool pose = printPose();
    std::cout << "}\n";

    bool failed = false;
    failed = failed || (options.requirePlayer && !player.usable);
    failed = failed || (options.requireRuntime && !runtime);
    failed = failed || (options.requireCamera && !camera);
    failed = failed || (options.requireDesktopAssistReady && !desktopAssist.ready);
    failed = failed || (options.requireDesktopAssist && !desktopAssist.poseApplied);
    failed = failed || (options.requireDesktopAssistUiQuad && !desktopAssistUiQuad);
    failed = failed || (options.requireVideo && !video);
    failed = failed || (options.requireStereo && !stereo.usable);
    failed = failed || (options.requireWorldStereo && !stereo.world);
    failed = failed || (options.requireEngineCenterWorldStereo
        && !stereo.engineCenterWorld);
    failed = failed || (options.requireUiThenEngineCenterWorld
        && !uiThenEngineCenterWorld.complete);
    failed = failed || (options.requirePose && !pose);
    failed = failed || (options.requireAdvancing && !advancing);
    return failed ? 2 : 0;
}
