#include "fnvxr_retail_rig_lifetime.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
struct Player { int first = 1; int third = 2; unsigned calls = 0; };
#if defined(_MSC_VER) && defined(_M_IX86)
// The unused EDX parameter makes this fixture compatible with a native
// thiscall callee: player in ECX, bool on the stack, callee executes RET 4.
__declspec(noinline) void* __fastcall animation(void* owner, void*, bool first)
#else
void* animation(void* owner, bool first)
#endif
{
    auto& player = *static_cast<Player*>(owner);
    ++player.calls;
    return first ? &player.first : &player.third;
}
struct Node { Node* parent = nullptr; std::vector<Node*> children; };
bool owned(Node* child, Node* root)
{
    return fnvxr::engine::retailOwnedDescendant(child, root,
        [](Node* node) { return node->parent; },
        [](Node* parent, Node* object) {
            for (auto* candidate : parent->children)
                if (candidate == object) return true;
            return false;
        });
}
}

int main() try
{
    using namespace fnvxr::engine;
    Player player;
    RetailGetActorAnimData volatile lookup = reinterpret_cast<RetailGetActorAnimData>(&animation);
#if defined(_MSC_VER) && defined(_M_IX86)
    std::uintptr_t before = 0, after = 0;
    __asm mov before, esp
#endif
    for (unsigned i = 0; i < 4096; ++i)
    {
        require(readRetailPlayerAnimation(lookup, &player, true) == &player.first,
            "first-person selector did not reach the native callee");
        require(readRetailPlayerAnimation(lookup, &player, false) == &player.third,
            "third-person selector did not reach the native callee");
    }
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm mov after, esp
    require(before == after, "native animation lookup changed caller ESP");
#endif
    require(player.calls == 8192, "animation calls were lost");
    require(!readRetailPlayerAnimation(lookup, nullptr, true), "null player accepted");
    require(!readRetailPlayerAnimation(nullptr, &player, true), "null lookup accepted");

    Node root, arm, hand, replacement;
    root.children = {&arm}; arm.parent = &root;
    arm.children = {&hand}; hand.parent = &arm;
    require(owned(&hand, &root), "live hand was rejected");
    // An in-place rebuild leaves the old node's parent bytes readable while
    // removing it from the live parent's child array. It must never be written.
    arm.children = {&replacement}; replacement.parent = &arm;
    require(!owned(&hand, &root), "detached hand with stale parent was accepted");
    require(owned(&replacement, &root), "replacement hand was rejected");
    root.children.clear();
    require(!owned(&replacement, &root), "detached whole arm was accepted");
    arm.parent = &replacement; replacement.children = {&arm};
    require(!owned(&replacement, &root), "parent cycle was accepted");
    require(!owned(nullptr, &root), "null cached node was accepted");
    return 0;
}
catch (const std::exception& error)
{
    std::cerr << error.what() << '\n';
    return 1;
}
