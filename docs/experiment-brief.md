# Experiment Brief

The experiment targets retail Windows Fallout: New Vegas directly instead of making OpenMW run FNV.

## Proposed Runtime Split

```text
SteamVR / OpenXR headset
        |
        v
FNVXRHost.exe
OpenXR app: hands, controllers, haptics, VR UI, pose timing
        |
        | shared memory / shared textures
        v
FalloutNV.exe
xNVSE plugin + optional D3D9 render hook
camera pose, weapon aim, activation, firing, UI hooks, stereo capture
```

## Why This Lab Exists

The OpenMW VR code can donate systems and ideas:

- OpenXR action setup.
- Hand pose and gesture handling.
- VR GUI, pointer, radial menu, and haptic patterns.
- Stereo and multiview math references.

It should not become the authoritative FNV runtime. Retail FNV should remain in charge of world state, quests, NPCs, collision, weapons, UI state, and save behavior.

## Live Bridge Nucleus

The live bridge uses fixed-size same-machine shared-memory mappings and shared D3D9 frame surfaces. Each stream has a single producer, magic/version validation, and sequence numbers for torn-write rejection.

The pose/input state contains:

- HMD rotation and position.
- Left/right controller rotation and position.
- Trigger and grip values.
- Button bitfield.
- Predicted display timestamp.

The game/runtime state contains:

- Player world position and body rotation.
- Equipped weapon form ID and ammo count.
- Muzzle/raycast diagnostics.
- Menu, Pip-Boy, dialogue, VATS, and loading flags.

## MVP Definition

MVP 0 is successful when:

- The OpenXR/OpenMW side publishes pose, pointer, click, and controller state through the shared mappings.
- Retail FNV publishes runtime phase, camera/player state, mono menu frames, and separated world eye frames.
- Menu clicks are direct Gamebryo menu/tile activations driven by the shared pointer state.
- Handoff to world rendering happens only after retail runtime state proves gameplay with no blocking menu bits.
