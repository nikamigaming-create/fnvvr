#include "fnvxr_native_menu_geometry.h"
#include "fnvxr_native_menu_stack.h"
#include <iostream>
#include <limits>

int main()
{
    int failures = 0;
    const auto expect = [&](bool pass, const char* name) {
        if (!pass) { std::cerr << name << '\n'; ++failures; }
    };
    // Observed loaded retail pause row: this lies outside the old 1280-wide
    // hit-test viewport, even though it is visible inside the 1280x720 image.
    const fnvxr::ui::MenuViewport viewport{1706.6666f, 960.0f, 0.0f, 0.0f, 1.0f};
    const auto row = fnvxr::ui::renderedMenuRect(viewport, 448.3333f, 144.0f,
        1.0f, true, 0.0f, 0.0f, 370.0f, 48.0f);
    float x = 1120.0f, y = 270.0f;
    expect(fnvxr::ui::nativeMenuPointer(viewport,1280.0f,720.0f,x,y) && row.contains(x,y),
        "The visible native Continue row receives its corresponding quad click");
    const auto child = fnvxr::ui::renderedMenuRect(viewport, 448.3333f, 144.0f,
        1.0f, false, 350.0f, 10.0f, 54.0f, 42.0f);
    expect(std::fabs(child.x - row.x - 350.0f) < 0.001f && child.y == row.y + 10.0f,
        "A non-locus tile applies its own geometry offset exactly once");
    const auto clipped = fnvxr::ui::clipMenuRect(child, {row.x,row.y,375.0f,48.0f});
    expect(clipped.width == 25.0f && clipped.height == 38.0f,
        "A scrolling container clips child hit areas to visible pixels");
    x=1280.0f; y=270.0f;
    expect(!fnvxr::ui::nativeMenuPointer(viewport,1280.0f,720.0f,x,y),
        "A ray outside the source cannot click a menu edge");
    expect(!fnvxr::ui::renderedMenuRect(viewport,
        std::numeric_limits<float>::quiet_NaN(),0,1,true,0,0,10,10).valid(),
        "An unavailable rendered transform cannot invent a clickable tile");
    expect(fnvxr::ui::nativeMenuStackTop({1008,1016,0}) == 1016,
        "Quantity popup owns input above its container parent");
    expect(fnvxr::ui::nativeMenuStackTop({1,1016,1001,0}) == 1001,
        "A nested message owns input even with a smaller numeric menu ID");
    expect(fnvxr::ui::nativeMenuStackTop({1008,0,1016}) == 1008,
        "Stale entries beyond the native terminator do not own input");
    expect(fnvxr::ui::nativeMenuStackTop({1,0}) == 1,
        "The native Pip-Boy sentinel is preserved");
    expect(fnvxr::ui::nativeMenuStackTop({1008,0xffffffff,0}) == 0,
        "Invalid stack entries cannot select a native callback");
    return failures ? 1 : 0;
}
