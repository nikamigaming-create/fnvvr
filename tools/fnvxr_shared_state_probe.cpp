#include "fnvxr_shared_state.h"

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
            * static_cast<std::size_t>(fnvxr::shared::D3D9SharedFrameMaxHeight) * 4u * 2u;

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
        bool requireOpenMwPlayer = false;
        bool requirePlayerMatch = false;
        bool requireRuntime = false;
        bool requireVideo = false;
        bool requireStereo = false;
        bool requireWorldStereo = false;
        bool requirePose = false;
        bool requireCamera = false;
        bool requireAdvancing = false;
        int sampleDelayMs = 250;
        float playerMatchTolerance = 1.0f;
        float playerRotationTolerance = 0.01f;
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
    };

    struct CounterStatus
    {
        bool present = false;
        bool usable = false;
        std::uint64_t value = 0;
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

    float argFloat(const std::vector<std::string>& args, const char* name, float defaultValue)
    {
        for (std::size_t i = 0; i + 1 < args.size(); ++i)
        {
            if (args[i] != name)
                continue;

            char* end = nullptr;
            const float parsed = std::strtof(args[i + 1].c_str(), &end);
            return end != args[i + 1].c_str() ? parsed : defaultValue;
        }
        return defaultValue;
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
            << "usage: fnvxr_shared_state_probe [--require-player] [--require-openmw-player]\n"
            << "                                [--require-player-match]\n"
            << "                                [--require-runtime] [--require-video]\n"
            << "                                [--require-stereo] [--require-world-stereo]\n"
            << "                                [--require-pose] [--require-camera]\n"
            << "                                [--match-position-tolerance <world-units>]\n"
            << "                                [--rotation-tolerance <matrix-units>]\n"
            << "                                [--require-advancing] [--sample-delay-ms <ms>]\n";
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

    bool finite9(const float values[9])
    {
        return finiteArray(values, 9);
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

    void jsonFloat(const char* name, float value, bool trailing = true)
    {
        std::cout << "\"" << name << "\":" << value;
        if (trailing)
            std::cout << ",";
        std::cout << "\n";
    }

    float distanceSquaredN(const float* left, const float* right, int count)
    {
        float result = 0.f;
        for (int i = 0; i < count; ++i)
        {
            const float delta = left[i] - right[i];
            result += delta * delta;
        }
        return result;
    }

    float distanceSquared(const float left[3], const float right[3])
    {
        return distanceSquaredN(left, right, 3);
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
        jsonBool("usableForOpenMwCellPositionSync", usable, false);
        std::cout << "},\n";
        return status;
    }

    bool printPlayerMatch(const PlayerStatus& retail, const PlayerStatus& openMw, float positionTolerance,
        float rotationTolerance)
    {
        const bool bothUsable = retail.usable && openMw.usable;
        const bool sameCell = bothUsable && retail.state.currentCellFormId == openMw.state.currentCellFormId;
        const float posDeltaSq = bothUsable ? distanceSquared(retail.state.playerWorldPos, openMw.state.playerWorldPos) : 0.f;
        const bool rotationFinite = bothUsable && finite9(retail.state.playerWorldRot) && finite9(openMw.state.playerWorldRot);
        const float rotDeltaSq = rotationFinite ? distanceSquaredN(retail.state.playerWorldRot, openMw.state.playerWorldRot, 9) : 0.f;
        const float clampedPositionTolerance = positionTolerance < 0.f ? 0.f : positionTolerance;
        const float clampedRotationTolerance = rotationTolerance < 0.f ? 0.f : rotationTolerance;
        const bool positionWithinTolerance = bothUsable && posDeltaSq <= clampedPositionTolerance * clampedPositionTolerance;
        const bool rotationWithinTolerance = rotationFinite
            && rotDeltaSq <= clampedRotationTolerance * clampedRotationTolerance;
        const bool cellPositionMatched = sameCell && positionWithinTolerance;
        const bool matched = cellPositionMatched && rotationWithinTolerance;

        std::cout << "\"playerMatch\":{\n";
        jsonBool("retailUsable", retail.usable);
        jsonBool("openMwUsable", openMw.usable);
        jsonBool("sameCellFormId", sameCell);
        jsonNumber("retailCellFormId", retail.state.currentCellFormId);
        jsonNumber("openMwCellFormId", openMw.state.currentCellFormId);
        jsonFloatArray3("retailPlayerWorldPos", retail.state.playerWorldPos);
        jsonFloatArray3("openMwPlayerWorldPos", openMw.state.playerWorldPos);
        jsonFloat("positionDeltaSquared", posDeltaSq);
        jsonFloat("positionTolerance", clampedPositionTolerance);
        jsonBool("positionWithinTolerance", positionWithinTolerance);
        jsonFloatArray("retailPlayerWorldRot", retail.state.playerWorldRot, 9);
        jsonFloatArray("openMwPlayerWorldRot", openMw.state.playerWorldRot, 9);
        jsonBool("rotationFinite", rotationFinite);
        jsonFloat("rotationDeltaSquared", rotDeltaSq);
        jsonFloat("rotationTolerance", clampedRotationTolerance);
        jsonBool("rotationWithinTolerance", rotationWithinTolerance);
        jsonBool("cellPositionMatched", cellPositionMatched);
        jsonBool("matched", matched, false);
        std::cout << "},\n";
        return matched;
    }

    bool counterAdvanced(const CounterStatus& before, const CounterStatus& after)
    {
        return before.usable && after.usable && after.value > before.value;
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

    CounterStatus readVideoCounter()
    {
        CounterStatus status;
        MappingView mapping;
        if (!mapping.open("Local\\FNVXR_D3D9_Frame_v1", SharedVideoMappingBytes))
            return status;

        auto* header = static_cast<const fnvxr::shared::SharedD3D9FrameHeader*>(mapping.view);
        const bool dimensionsOk = header->width > 0 && header->height > 0
            && header->width <= static_cast<LONG>(fnvxr::shared::D3D9SharedFrameMaxWidth)
            && header->height <= static_cast<LONG>(fnvxr::shared::D3D9SharedFrameMaxHeight)
            && header->pitchBytes >= header->width * 4;
        status.present = true;
        status.usable = header->magic == fnvxr::shared::D3D9FrameSharedMagic && header->writing == 0 && dimensionsOk;
        status.value = static_cast<std::uint64_t>(header->sequence);
        return status;
    }

    CounterStatus readStereoCounter(bool requireWorld)
    {
        CounterStatus status;
        MappingView mapping;
        if (!mapping.open("Local\\FNVXR_D3D9_StereoFrame_v1", SharedStereoMappingBytes))
            return status;

        auto* header = static_cast<const fnvxr::shared::SharedD3D9StereoFrameHeader*>(mapping.view);
        const bool dimensionsOk = header->width > 0 && header->height > 0
            && header->width <= static_cast<LONG>(fnvxr::shared::D3D9SharedFrameMaxWidth)
            && header->height <= static_cast<LONG>(fnvxr::shared::D3D9SharedFrameMaxHeight)
            && header->pitchBytes >= header->width * 4;
        const bool usable = header->magic == fnvxr::shared::D3D9StereoFrameSharedMagic && header->writing == 0
            && dimensionsOk && header->poseValid != 0 && header->uiActive == 0;
        const bool nativeSameFrame =
            header->producerMode == static_cast<LONG>(fnvxr::shared::StereoProducerNativeSameFrame)
            && header->renderPairSequence > 0;
        const bool world = usable && nativeSameFrame && header->worldCandidate != 0 && header->separated != 0;
        status.present = true;
        status.usable = requireWorld ? world : usable;
        status.value = static_cast<std::uint64_t>(header->sequence);
        return status;
    }

    CounterStatus readPoseCounter()
    {
        CounterStatus status;
        MappingView mapping;
        if (!mapping.open("Local\\FNVXR_VR_Pose_State", sizeof(fnvxr::shared::SharedVrPoseState)))
            return status;

        fnvxr::shared::SharedVrPoseState state {};
        LONG sequenceBefore = 0;
        LONG sequenceAfter = 0;
        const bool stable = copyTornChecked(mapping.view, state, sequenceBefore, sequenceAfter);
        status.present = true;
        status.usable = stable && state.magic == fnvxr::shared::VrPoseSharedMagic
            && state.version == fnvxr::shared::VrPoseSharedVersion && finite3(state.hmdPos);
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
        const CounterStatus openMwPlayerBefore = readPlayerCounter("Local\\FNVXR_OpenMW_Player_State",
            fnvxr::shared::OpenMwPlayerSharedMagic, fnvxr::shared::OpenMwPlayerSharedVersion);
        const CounterStatus runtimeBefore = readRuntimeCounter();
        const CounterStatus cameraBefore = readCameraCounter();
        const CounterStatus videoBefore = readVideoCounter();
        const CounterStatus stereoBefore = readStereoCounter(options.requireWorldStereo);
        const CounterStatus poseBefore = readPoseCounter();

        const int sampleDelayMs = options.sampleDelayMs < 0 ? 0 : options.sampleDelayMs;
        Sleep(static_cast<DWORD>(sampleDelayMs));

        const CounterStatus playerAfter = readPlayerCounter(
            "Local\\FNVXR_Player_State", fnvxr::shared::PlayerSharedMagic, fnvxr::shared::PlayerSharedVersion);
        const CounterStatus openMwPlayerAfter = readPlayerCounter("Local\\FNVXR_OpenMW_Player_State",
            fnvxr::shared::OpenMwPlayerSharedMagic, fnvxr::shared::OpenMwPlayerSharedVersion);
        const CounterStatus runtimeAfter = readRuntimeCounter();
        const CounterStatus cameraAfter = readCameraCounter();
        const CounterStatus videoAfter = readVideoCounter();
        const CounterStatus stereoAfter = readStereoCounter(options.requireWorldStereo);
        const CounterStatus poseAfter = readPoseCounter();

        const bool anySpecificRequirement = options.requirePlayer || options.requireOpenMwPlayer
            || options.requireRuntime || options.requireCamera || options.requireVideo || options.requireStereo
            || options.requireWorldStereo || options.requirePose;
        bool allRequiredAdvanced = true;
        if (!anySpecificRequirement || options.requirePlayer)
            allRequiredAdvanced = allRequiredAdvanced && counterAdvanced(playerBefore, playerAfter);
        if (options.requireOpenMwPlayer)
            allRequiredAdvanced = allRequiredAdvanced && counterAdvanced(openMwPlayerBefore, openMwPlayerAfter);
        if (!anySpecificRequirement || options.requireRuntime)
            allRequiredAdvanced = allRequiredAdvanced && counterAdvanced(runtimeBefore, runtimeAfter);
        if (!anySpecificRequirement || options.requireCamera)
            allRequiredAdvanced = allRequiredAdvanced && counterAdvanced(cameraBefore, cameraAfter);
        if (!anySpecificRequirement || options.requireVideo)
            allRequiredAdvanced = allRequiredAdvanced && counterAdvanced(videoBefore, videoAfter);
        if (!anySpecificRequirement || options.requireStereo || options.requireWorldStereo)
            allRequiredAdvanced = allRequiredAdvanced && counterAdvanced(stereoBefore, stereoAfter);
        if (!anySpecificRequirement || options.requirePose)
            allRequiredAdvanced = allRequiredAdvanced && counterAdvanced(poseBefore, poseAfter);

        std::cout << "\"freshness\":{\n";
        jsonSigned("sampleDelayMs", sampleDelayMs);
        jsonFreshCounter("player", playerBefore, playerAfter);
        jsonFreshCounter("openMwPlayer", openMwPlayerBefore, openMwPlayerAfter);
        jsonFreshCounter("runtime", runtimeBefore, runtimeAfter);
        jsonFreshCounter("camera", cameraBefore, cameraAfter);
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

        auto* header = static_cast<const fnvxr::shared::SharedD3D9FrameHeader*>(mapping.view);
        const bool magicOk = header->magic == fnvxr::shared::D3D9FrameSharedMagic;
        const bool stable = header->writing == 0;
        const bool dimensionsOk = header->width > 0 && header->height > 0
            && header->width <= static_cast<LONG>(fnvxr::shared::D3D9SharedFrameMaxWidth)
            && header->height <= static_cast<LONG>(fnvxr::shared::D3D9SharedFrameMaxHeight)
            && header->pitchBytes >= header->width * 4;
        const bool usable = magicOk && stable && dimensionsOk;

        jsonBool("present", true);
        jsonBool("stable", stable);
        jsonBool("magicOk", magicOk);
        jsonSigned("sequence", header->sequence);
        jsonSigned("width", header->width);
        jsonSigned("height", header->height);
        jsonSigned("pitchBytes", header->pitchBytes);
        jsonSigned("format", header->format);
        jsonBool("dimensionsOk", dimensionsOk);
        jsonBool("usable", usable, false);
        std::cout << "},\n";
        return usable;
    }

    StereoStatus printStereo()
    {
        MappingView mapping;
        std::cout << "\"stereo\":{\n";
        if (!mapping.open("Local\\FNVXR_D3D9_StereoFrame_v1", SharedStereoMappingBytes))
        {
            jsonBool("present", false, false);
            std::cout << "},\n";
            return {};
        }

        auto* header = static_cast<const fnvxr::shared::SharedD3D9StereoFrameHeader*>(mapping.view);
        const bool magicOk = header->magic == fnvxr::shared::D3D9StereoFrameSharedMagic;
        const bool stable = header->writing == 0;
        const bool dimensionsOk = header->width > 0 && header->height > 0
            && header->width <= static_cast<LONG>(fnvxr::shared::D3D9SharedFrameMaxWidth)
            && header->height <= static_cast<LONG>(fnvxr::shared::D3D9SharedFrameMaxHeight)
            && header->pitchBytes >= header->width * 4;
        const bool usable = magicOk && stable && dimensionsOk && header->poseValid != 0 && header->uiActive == 0;
        const bool nativeSameFrame =
            header->producerMode == static_cast<LONG>(fnvxr::shared::StereoProducerNativeSameFrame)
            && header->renderPairSequence > 0;
        const bool worldStereo = usable && nativeSameFrame && header->worldCandidate != 0 && header->separated != 0;

        jsonBool("present", true);
        jsonBool("stable", stable);
        jsonBool("magicOk", magicOk);
        jsonSigned("sequence", header->sequence);
        jsonSigned("width", header->width);
        jsonSigned("height", header->height);
        jsonSigned("pitchBytes", header->pitchBytes);
        jsonSigned("format", header->format);
        jsonBool("dimensionsOk", dimensionsOk);
        jsonBool("separated", header->separated != 0);
        jsonBool("worldCandidate", header->worldCandidate != 0);
        jsonBool("uiActive", header->uiActive != 0);
        jsonBool("poseValid", header->poseValid != 0);
        jsonSigned("poseSequence", header->poseSequence);
        jsonSigned("producerMode", header->producerMode);
        jsonSigned("renderPairSequence", header->renderPairSequence);
        jsonBool("nativeSameFrame", nativeSameFrame);
        jsonBool("usableForHostStereo", usable);
        jsonBool("usableWorldStereo", worldStereo, false);
        std::cout << "},\n";
        return { usable, worldStereo };
    }

    bool printPose()
    {
        MappingView mapping;
        std::cout << "\"pose\":{\n";
        if (!mapping.open("Local\\FNVXR_VR_Pose_State", sizeof(fnvxr::shared::SharedVrPoseState)))
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
        const bool usable = stable && magicOk && versionOk && finite3(state.hmdPos);

        jsonBool("present", true);
        jsonBool("stable", stable);
        jsonBool("magicOk", magicOk);
        jsonBool("versionOk", versionOk);
        jsonSigned("sequenceBefore", sequenceBefore);
        jsonSigned("sequenceAfter", sequenceAfter);
        jsonNumber("frame", state.frame);
        jsonNumber("trackingFlags", state.trackingFlags);
        jsonFloatArray3("hmdPos", state.hmdPos);
        jsonFloatArray3("leftPos", state.leftPos);
        jsonFloatArray3("rightPos", state.rightPos);
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
    options.requireOpenMwPlayer = hasArg(args, "--require-openmw-player");
    options.requirePlayerMatch = hasArg(args, "--require-player-match");
    options.requireRuntime = hasArg(args, "--require-runtime");
    options.requireVideo = hasArg(args, "--require-video");
    options.requireStereo = hasArg(args, "--require-stereo");
    options.requireWorldStereo = hasArg(args, "--require-world-stereo");
    options.requirePose = hasArg(args, "--require-pose");
    options.requireCamera = hasArg(args, "--require-camera");
    options.requireAdvancing = hasArg(args, "--require-advancing");
    options.playerMatchTolerance = argFloat(args, "--match-position-tolerance", options.playerMatchTolerance);
    options.playerRotationTolerance = argFloat(args, "--rotation-tolerance", options.playerRotationTolerance);
    options.sampleDelayMs = argInt(args, "--sample-delay-ms", options.sampleDelayMs);

    std::cout << "{\n";
    const PlayerStatus player = printPlayer(
        "player",
        "Local\\FNVXR_Player_State",
        fnvxr::shared::PlayerSharedMagic,
        fnvxr::shared::PlayerSharedVersion);
    const PlayerStatus openMwPlayer = printPlayer(
        "openMwPlayer",
        "Local\\FNVXR_OpenMW_Player_State",
        fnvxr::shared::OpenMwPlayerSharedMagic,
        fnvxr::shared::OpenMwPlayerSharedVersion);
    const bool playerMatch = printPlayerMatch(
        player, openMwPlayer, options.playerMatchTolerance, options.playerRotationTolerance);
    const bool runtime = printRuntime();
    const bool camera = printCamera();
    const bool video = printVideo();
    const StereoStatus stereo = printStereo();
    bool advancing = true;
    if (options.requireAdvancing)
        advancing = printFreshness(options);
    const bool pose = printPose();
    std::cout << "}\n";

    bool failed = false;
    failed = failed || (options.requirePlayer && !player.usable);
    failed = failed || (options.requireOpenMwPlayer && !openMwPlayer.usable);
    failed = failed || (options.requirePlayerMatch && !playerMatch);
    failed = failed || (options.requireRuntime && !runtime);
    failed = failed || (options.requireCamera && !camera);
    failed = failed || (options.requireVideo && !video);
    failed = failed || (options.requireStereo && !stereo.usable);
    failed = failed || (options.requireWorldStereo && !stereo.world);
    failed = failed || (options.requirePose && !pose);
    failed = failed || (options.requireAdvancing && !advancing);
    return failed ? 2 : 0;
}
