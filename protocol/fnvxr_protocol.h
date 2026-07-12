#pragma once

#include <cmath>
#include <cstdint>

namespace fnvxr
{
constexpr std::uint32_t ProtocolMagic = 0x5856564e; // NVVX, little-endian marker.
constexpr std::uint16_t ProtocolVersion = 2;

struct Vec3
{
    float x;
    float y;
    float z;
};

struct Quat
{
    float x;
    float y;
    float z;
    float w;
};

enum ButtonBits : std::uint64_t
{
    ButtonA = 1ull << 0,
    ButtonB = 1ull << 1,
    ButtonX = 1ull << 2,
    ButtonY = 1ull << 3,
    LeftMenu = 1ull << 4,
    RightMenu = 1ull << 5,
    LeftThumbstick = 1ull << 6,
    RightThumbstick = 1ull << 7,
};

#pragma pack(push, 1)
struct PoseFrame
{
    std::uint32_t magic = ProtocolMagic;
    std::uint16_t version = ProtocolVersion;
    std::uint16_t byteSize = sizeof(PoseFrame);

    std::uint64_t frame = 0;
    double predictedDisplayTime = 0.0;

    Quat hmdRot { 0.0f, 0.0f, 0.0f, 1.0f };
    Vec3 hmdPos { 0.0f, 0.0f, 0.0f };

    Quat leftRot { 0.0f, 0.0f, 0.0f, 1.0f };
    Vec3 leftPos { -0.25f, -0.2f, -0.45f };

    Quat rightRot { 0.0f, 0.0f, 0.0f, 1.0f };
    Vec3 rightPos { 0.25f, -0.2f, -0.45f };

    float leftTrigger = 0.0f;
    float rightTrigger = 0.0f;
    float leftGrip = 0.0f;
    float rightGrip = 0.0f;

    std::uint64_t buttons = 0;

    float menuPointerX = -1.0f;
    float menuPointerY = -1.0f;
    float leftThumbstickX = 0.0f;
    float leftThumbstickY = 0.0f;
    float rightThumbstickX = 0.0f;
    float rightThumbstickY = 0.0f;
    std::uint8_t menuPointerActive = 0;
    std::uint8_t poseReserved[7] {};
};

struct GameFrame
{
    std::uint32_t magic = ProtocolMagic;
    std::uint16_t version = ProtocolVersion;
    std::uint16_t byteSize = sizeof(GameFrame);

    std::uint64_t frame = 0;

    Vec3 playerWorldPos { 0.0f, 0.0f, 0.0f };
    Quat playerBodyRot { 0.0f, 0.0f, 0.0f, 1.0f };

    std::uint32_t equippedWeaponFormId = 0;
    std::int32_t currentAmmo = 0;

    std::uint32_t rayHitRefId = 0;
    Vec3 weaponMuzzleWorld { 0.0f, 0.0f, 0.0f };
    Vec3 rayHitWorld { 0.0f, 0.0f, 0.0f };

    std::uint8_t pipboyOpen = 0;
    std::uint8_t menuOpen = 0;
    std::uint8_t inDialogue = 0;
    std::uint8_t inVATS = 0;
    std::uint8_t loading = 0;
    std::uint8_t showroomActive = 0;
    std::uint8_t showroomSceneIndex = 0;
    std::uint8_t showroomTransition = 0;
    std::uint8_t showroomLocked = 0;
    std::uint8_t showroomReserved[3] {};

    std::uint32_t showroomCellFormId = 0;
};
#pragma pack(pop)

static_assert(sizeof(PoseFrame) == 164, "PoseFrame layout changed; update protocol docs.");
static_assert(sizeof(GameFrame) == 96, "GameFrame layout changed; update protocol docs.");

inline bool isFiniteVec3(const Vec3& value)
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

inline bool isFiniteQuat(const Quat& value)
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z)
        && std::isfinite(value.w);
}

inline bool isValidPoseFrame(const PoseFrame& frame)
{
    return frame.magic == ProtocolMagic
        && frame.version == ProtocolVersion
        && frame.byteSize == sizeof(PoseFrame)
        && std::isfinite(frame.predictedDisplayTime)
        && isFiniteQuat(frame.hmdRot)
        && isFiniteVec3(frame.hmdPos)
        && isFiniteQuat(frame.leftRot)
        && isFiniteVec3(frame.leftPos)
        && isFiniteQuat(frame.rightRot)
        && isFiniteVec3(frame.rightPos)
        && std::isfinite(frame.leftTrigger)
        && std::isfinite(frame.rightTrigger)
        && std::isfinite(frame.leftGrip)
        && std::isfinite(frame.rightGrip)
        && std::isfinite(frame.menuPointerX)
        && std::isfinite(frame.menuPointerY)
        && std::isfinite(frame.leftThumbstickX)
        && std::isfinite(frame.leftThumbstickY)
        && std::isfinite(frame.rightThumbstickX)
        && std::isfinite(frame.rightThumbstickY);
}

inline bool isValidGameFrame(const GameFrame& frame)
{
    return frame.magic == ProtocolMagic
        && frame.version == ProtocolVersion
        && frame.byteSize == sizeof(GameFrame)
        && isFiniteVec3(frame.playerWorldPos)
        && isFiniteQuat(frame.playerBodyRot)
        && isFiniteVec3(frame.weaponMuzzleWorld)
        && isFiniteVec3(frame.rayHitWorld);
}
}
