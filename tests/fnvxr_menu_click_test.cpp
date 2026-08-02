#include "fnvxr_menu_click.h"

#include <cstdlib>
#include <iostream>

namespace
{
int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}
}

int main()
{
    using fnvxr::host::menu_input::ClickEdges;
    using fnvxr::host::menu_input::PointerHand;
    using fnvxr::host::menu_input::selectClick;

    if (!selectClick(PointerHand::Right, { true, false, false, false }, true, true))
        return fail("right trigger must select for the right-hand pointer");
    if (!selectClick(PointerHand::Right, { false, false, true, false }, true, true))
        return fail("A must select for the right-hand pointer");
    if (!selectClick(PointerHand::Left, { false, true, false, false }, true, true))
        return fail("left trigger must select for the left-hand pointer");
    if (!selectClick(PointerHand::Left, { false, false, false, true }, true, true))
        return fail("X must select for the left-hand pointer");

    if (selectClick(PointerHand::Right, { false, true, false, true }, true, true))
        return fail("the opposite hand must not click a right-hand pointer");
    if (selectClick(PointerHand::Left, { true, false, true, false }, true, true))
        return fail("the opposite hand must not click a left-hand pointer");
    if (!selectClick(PointerHand::Both, { false, true, false, false }, true, true)
        || !selectClick(PointerHand::Head, { false, false, true, false }, true, true))
    {
        return fail("both/head pointer modes must accept either controller select control");
    }

    const ClickEdges everyEdge { true, true, true, true };
    if (selectClick(PointerHand::Right, everyEdge, false, true))
        return fail("a non-interactive or unaccepted UI must never receive a click");
    if (selectClick(PointerHand::Right, everyEdge, true, false))
        return fail("an inactive pointer must never receive a click");
    if (selectClick(PointerHand::Right, {}, true, true))
        return fail("held controls without a rising edge must not repeat-click");

    std::cout << "menu click policy PASS\n";
    return EXIT_SUCCESS;
}
