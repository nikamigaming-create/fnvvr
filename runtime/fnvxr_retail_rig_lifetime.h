#pragma once

#include <cstddef>

namespace fnvxr::engine
{
// FNV 1.4.0.525, 0x00950A60: ECX is the player, one bool is on the
// stack, and RET 4 belongs to the callee. Omitting the bool corrupts ESP.
#if defined(_MSC_VER) && defined(_M_IX86)
using RetailGetActorAnimData = void* (__thiscall*)(void*, bool);
#else
using RetailGetActorAnimData = void* (*)(void*, bool);
#endif

inline void* readRetailPlayerAnimation(
    RetailGetActorAnimData lookup, void* player, bool firstPerson)
{
    return lookup && player ? lookup(player, firstPerson) : nullptr;
}

// A detached node can retain a stale parent pointer. The parent must still
// own that exact child at every edge before any cached rig node is writable.
template<class Node, class ReadParent, class OwnsChild>
bool retailOwnedDescendant(
    Node object, Node ancestor, ReadParent readParent, OwnsChild ownsChild)
{
    if (!object || !ancestor) return false;
    for (std::size_t depth = 0; object && depth < 64; ++depth)
    {
        if (object == ancestor) return true;
        const Node parent = readParent(object);
        if (!parent || !ownsChild(parent, object)) return false;
        object = parent;
    }
    return false;
}
}
