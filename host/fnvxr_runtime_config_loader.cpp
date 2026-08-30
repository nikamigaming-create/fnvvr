#include "fnvxr_runtime_config_loader.h"

#include <cerrno>
#include <charconv>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

namespace fnvxr::host
{
namespace
{
struct ReadError
{
    RuntimeConfigTextError error = RuntimeConfigTextError::None;
    const char* variable = nullptr;

    explicit operator bool() const noexcept
    {
        return error != RuntimeConfigTextError::None;
    }
};

ReadError readText(
    const RuntimeConfigSource& source,
    const char* variable,
    std::string& destination,
    bool& present)
{
    destination.clear();
    present = source.read(variable, destination);
    if (!present)
        return {};
    if (destination.empty())
        return { RuntimeConfigTextError::EmptyValue, variable };
    return {};
}

ReadError readUnsigned(
    const RuntimeConfigSource& source,
    const char* variable,
    std::uint32_t& destination)
{
    std::string text;
    bool present = false;
    if (const ReadError failure = readText(
            source, variable, text, present))
        return failure;
    if (!present)
        return {};

    std::uint64_t parsed = 0;
    const char* value = text.c_str();
    const char* end = value + std::strlen(value);
    const std::from_chars_result result = std::from_chars(value, end, parsed);
    if (result.ec != std::errc {} || result.ptr != end)
        return { RuntimeConfigTextError::InvalidUnsignedInteger, variable };
    if (parsed > std::numeric_limits<std::uint32_t>::max())
        return { RuntimeConfigTextError::IntegerOutOfRange, variable };
    destination = static_cast<std::uint32_t>(parsed);
    return {};
}

ReadError readSize(
    const RuntimeConfigSource& source,
    const char* variable,
    std::size_t& destination)
{
    std::string text;
    bool present = false;
    if (const ReadError failure = readText(
            source, variable, text, present))
        return failure;
    if (!present)
        return {};

    std::uint64_t parsed = 0;
    const char* value = text.c_str();
    const char* end = value + std::strlen(value);
    const std::from_chars_result result = std::from_chars(value, end, parsed);
    if (result.ec != std::errc {} || result.ptr != end)
        return { RuntimeConfigTextError::InvalidUnsignedInteger, variable };
    if (parsed > std::numeric_limits<std::size_t>::max())
        return { RuntimeConfigTextError::IntegerOutOfRange, variable };
    destination = static_cast<std::size_t>(parsed);
    return {};
}

ReadError readFloat(
    const RuntimeConfigSource& source,
    const char* variable,
    float& destination)
{
    std::string text;
    bool present = false;
    if (const ReadError failure = readText(
            source, variable, text, present))
        return failure;
    if (!present)
        return {};

    errno = 0;
    char* end = nullptr;
    const char* value = text.c_str();
    const float parsed = std::strtof(value, &end);
    if (end == value || end == nullptr || end[0] != '\0' || errno == ERANGE)
        return { RuntimeConfigTextError::InvalidFloat, variable };
    destination = parsed;
    return {};
}

ReadError readBoolean(
    const RuntimeConfigSource& source,
    const char* variable,
    bool& destination)
{
    std::string text;
    bool present = false;
    if (const ReadError failure = readText(
            source, variable, text, present))
        return failure;
    if (!present)
        return {};
    const char* value = text.c_str();
    if (std::strcmp(value, "1") == 0 || std::strcmp(value, "true") == 0)
    {
        destination = true;
        return {};
    }
    if (std::strcmp(value, "0") == 0 || std::strcmp(value, "false") == 0)
    {
        destination = false;
        return {};
    }
    return { RuntimeConfigTextError::InvalidBoolean, variable };
}

RuntimeConfigLoadResult textFailure(const ReadError& failure) noexcept
{
    RuntimeConfigLoadResult result {};
    result.textError = failure.error;
    result.variable = failure.variable;
    return result;
}
}

RuntimeConfigLoadResult::operator bool() const noexcept
{
    return textError == RuntimeConfigTextError::None
        && validationError == kernel::ConfigError::None
        && config.has_value();
}

RuntimeConfigLoadResult loadRuntimeConfig(
    const RuntimeConfigSource& source)
{
    kernel::RuntimeConfig candidate {};
    ReadError failure {};

#define FNVXR_READ(reader, variable, destination) \
    failure = reader(source, variable, destination); \
    if (failure) return textFailure(failure)

    FNVXR_READ(readUnsigned, "FNVXR_TARGET_REFRESH_HZ", candidate.performance.targetRefreshHz);
    FNVXR_READ(readUnsigned, "FNVXR_MAX_FRAMES_IN_FLIGHT", candidate.performance.maximumFramesInFlight);
    FNVXR_READ(readSize, "FNVXR_POSE_HISTORY_CAPACITY", candidate.performance.poseHistoryCapacity);
    FNVXR_READ(readFloat, "FNVXR_STEREO_MAX_SOURCE_POSE_AGE_MS", candidate.performance.maximumPoseAgeMilliseconds);
    FNVXR_READ(readBoolean, "FNVXR_REQUIRE_GPU_EYE_TRANSPORT", candidate.performance.requireGpuEyeTransport);

    FNVXR_READ(readFloat, "FNVXR_RENDER_SCALE", candidate.presentation.renderScale);
    FNVXR_READ(readFloat, "FNVXR_NEAR_CLIP_METERS", candidate.presentation.nearClipMeters);
    FNVXR_READ(readFloat, "FNVXR_FAR_CLIP_METERS", candidate.presentation.farClipMeters);
    FNVXR_READ(readFloat, "FNVXR_MENU_WIDTH_METERS", candidate.presentation.menuWidthMeters);
    FNVXR_READ(readFloat, "FNVXR_MENU_DISTANCE_METERS", candidate.presentation.menuDistanceMeters);

    FNVXR_READ(readFloat, "FNVXR_BODY_STANDING_HEIGHT", candidate.bodyRig.standingHeightMeters);
    FNVXR_READ(readFloat, "FNVXR_BODY_SHOULDER_WIDTH", candidate.bodyRig.shoulderWidthMeters);
    FNVXR_READ(readFloat, "FNVXR_ARM_UPPER_LENGTH", candidate.bodyRig.upperArmLengthMeters);
    FNVXR_READ(readFloat, "FNVXR_ARM_LOWER_LENGTH", candidate.bodyRig.forearmLengthMeters);
    FNVXR_READ(readFloat, "FNVXR_HAND_LENGTH", candidate.bodyRig.handLengthMeters);

    FNVXR_READ(readFloat, "FNVXR_PIPBOY_WRIST_UI_WIDTH", candidate.wristUi.widthMeters);
    FNVXR_READ(readFloat, "FNVXR_PIPBOY_WRIST_UI_HEIGHT", candidate.wristUi.heightMeters);
    FNVXR_READ(readFloat, "FNVXR_WRIST_UI_ACTIVATION_DISTANCE", candidate.wristUi.activationDistanceMeters);
    FNVXR_READ(readFloat, "FNVXR_WRIST_UI_DEACTIVATION_DISTANCE", candidate.wristUi.deactivationDistanceMeters);
    FNVXR_READ(readFloat, "FNVXR_WRIST_UI_ACTIVATION_ANGLE", candidate.wristUi.activationAngleDegrees);

#undef FNVXR_READ

    kernel::ConfigValidationResult validated = kernel::validateRuntimeConfig(candidate);
    RuntimeConfigLoadResult result {};
    result.validationError = validated.error;
    if (validated)
        result.config = validated.config;
    return result;
}

const char* runtimeConfigTextErrorName(RuntimeConfigTextError error) noexcept
{
    switch (error)
    {
    case RuntimeConfigTextError::None: return "none";
    case RuntimeConfigTextError::EmptyValue: return "empty-value";
    case RuntimeConfigTextError::InvalidUnsignedInteger:
        return "invalid-unsigned-integer";
    case RuntimeConfigTextError::IntegerOutOfRange: return "integer-out-of-range";
    case RuntimeConfigTextError::InvalidFloat: return "invalid-float";
    case RuntimeConfigTextError::InvalidBoolean: return "invalid-boolean";
    }
    return "unknown";
}
}
