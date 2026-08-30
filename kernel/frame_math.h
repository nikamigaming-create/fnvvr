#pragma once

namespace fnvxr::kernel
{
struct Vec3 final
{
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct Quaternion final
{
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float w = 1.0F;
};

struct Pose final
{
    Vec3 position {};
    Quaternion orientation {};
};

struct FieldOfView final
{
    float leftRadians = 0.0F;
    float rightRadians = 0.0F;
    float upRadians = 0.0F;
    float downRadians = 0.0F;
};

struct View final
{
    Pose pose {};
    FieldOfView fieldOfView {};
};
}
