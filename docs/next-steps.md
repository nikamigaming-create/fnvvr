# Next Steps

For today's work, `docs/today-rock-solid-direction.md` is the active direction. The north star is performance, accuracy, UI/UX, and retail parity.

Pinned checkpoint: `docs/playable-big-screen-vr-baseline.md` is the current first playable baseline. Treat the big-screen OpenXR retail sidecar as working for its scoped contract before changing interaction or stereo-world code.

Use `docs/openmw-swg-fnvxr-convergence.md` as the current reference contract:

- OpenMWVR is the primary behavior reference for action binding, tracking gates, hands, pointer, and Xbox-style controller semantics.
- OG SWG is the secondary bridge reference for D3D11/OpenXR ownership, runtime staging, and proof logging.
- Retail Fallout New Vegas remains authoritative for gameplay, UI, favorites, weapons, projectiles, inventory, perks, and mod behavior.
- FNVXR outputs only vanilla-supported input surfaces: Xbox/XInput, mouse, keyboard, and DirectInput.

The first menu experience must already be a stereo 3D VR scene with a pointer and clickable surfaced FNV menu quad. The menu quad remains the normal retail FNV menu capture. The surrounding scene is a separate VR render layer and must not be created by `coc`, `cow`, player movement, or any other mutation of the live retail game state. Then live FNV D3D stereo takes over when a real world camera is proven. Do not mix loose environment flags by hand; use an explicit sidecar run profile.

## Phase 0: Shared-Memory Nucleus

- Keep every live stream fixed-size, versioned, and sequence-guarded.
- Use shared memory or shared textures for same-machine state.
- Validate magic/version/size before consuming state.

## Phase 1: OpenXR Host

- Publish OpenXR HMD/controller actions into the shared pose/input mappings.
- Keep menu pointer coordinates tied to the rendered quad hit.
- Add haptic output as an explicit retail-to-host shared stream when needed.

## Phase 2: xNVSE Receiver

- Keep the xNVSE plugin as the retail-side owner of menu state and direct UI clicks.
- Publish runtime phase, menu bits, camera state, and player state from the main loop.
- Log pose/input frames through xNVSE/FNV logging first.

## Phase 3: Input Mapping

- Map buttons to retail movement, activate, fire, menu, favorites/hotkeys, and recenter test actions.
- Keep the hand ray as the mouse pointer for surfaced FNV UI quads.
- Use left grip as the practical modifier that turns left stick into virtual Xbox D-pad for favorites/hotkeys outside menus.
- Keep gameplay authority inside FNV.
- Publish menu/loading/dialogue state from FNV back to the host through runtime shared state.

## Phase 4: Camera and Aim

- Inject HMD rotation into first-person camera orientation.
- Add local HMD translation after recenter/origin handling is solid.
- Add right-hand aim ray tests and compare against FNV raycasts.

## Phase 5: Rendering

- Prototype a D3D9 hook only after pose, input, menus, and recentering are predictable.
- Start with depth or frame capture diagnostics.
- Move toward per-eye camera passes only when the flat game remains stable.
