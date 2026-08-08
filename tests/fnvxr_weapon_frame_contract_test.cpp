#include "fnvxr_weapon_frame_contract.h"

#include <cstdint>
#include <limits>

namespace
{
int fail()
{
    return 1;
}
}

int main()
{
    using fnvxr::weapon_frame::Failure;
    constexpr std::uint32_t committed = 1u;
    constexpr std::uint32_t required = 0x0fu;
    if (fnvxr::weapon_frame::validateIdentity(
            committed, committed, required, required,
            42u, 900u, 42u, 900u, 0x1000u, 0x2000u) != Failure::None)
        return fail();
    if (fnvxr::weapon_frame::validateIdentity(
            committed, committed, required, required,
            42u, 900u, 44u, 900u, 0x1000u, 0x2000u) != Failure::PoseMismatch)
        return fail();
    if (fnvxr::weapon_frame::validateIdentity(
            committed, committed, required & ~4u, required,
            42u, 900u, 42u, 900u, 0x1000u, 0x2000u) != Failure::Incomplete)
        return fail();
    if (fnvxr::weapon_frame::validateIdentity(
            committed, committed, required, required,
            42u, 900u, 42u, 900u, 0u, 0x2000u) != Failure::MissingNodes)
        return fail();

    const float committedTransform[3] { 1.0f, 2.0f, 3.0f };
    const float stableTransform[3] { 1.01f, 1.99f, 3.0f };
    const float overwrittenTransform[3] { 1.0f, 2.0f, 3.03f };
    const float invalidTransform[3] {
        1.0f, 2.0f, std::numeric_limits<float>::quiet_NaN() };
    if (!fnvxr::weapon_frame::transformMatches(
            stableTransform, committedTransform, 3, 0.02f))
        return fail();
    if (fnvxr::weapon_frame::transformMatches(
            overwrittenTransform, committedTransform, 3, 0.02f))
        return fail();
    if (fnvxr::weapon_frame::transformMatches(
            invalidTransform, committedTransform, 3, 0.02f))
        return fail();
    return 0;
}
