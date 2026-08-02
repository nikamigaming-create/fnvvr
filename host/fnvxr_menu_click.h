#pragma once

namespace fnvxr::host::menu_input
{
enum class PointerHand
{
    Right,
    Left,
    Both,
    Head,
};

struct ClickEdges
{
    bool rightTrigger = false;
    bool leftTrigger = false;
    bool buttonA = false;
    bool buttonX = false;
};

constexpr bool selectClick(
    PointerHand pointerHand,
    const ClickEdges& edges,
    bool interactiveUiPointerAuthorized,
    bool pointerActive) noexcept
{
    if (!interactiveUiPointerAuthorized || !pointerActive)
        return false;

    switch (pointerHand)
    {
        case PointerHand::Right:
            return edges.rightTrigger || edges.buttonA;
        case PointerHand::Left:
            return edges.leftTrigger || edges.buttonX;
        case PointerHand::Both:
        case PointerHand::Head:
            return edges.rightTrigger
                || edges.leftTrigger
                || edges.buttonA
                || edges.buttonX;
    }
    return false;
}
}
