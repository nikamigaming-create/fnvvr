# Retail Hands and Weapon Plan

The playable prop path uses the standalone OpenXR host for desired tracked
poses and the retail FNV scene graph for authoritative hands, weapon,
animation, muzzle, and gameplay results.

## Current Foundation

- The host samples grip and aim poses plus trigger, squeeze, touch/button, and
  thumbstick actions.
- The xNVSE plugin receives those poses in-process.
- Retail rig discovery searches for and logs first-person arm chains, hands,
  weapon, projectile, and muzzle-flash nodes; retained runs have not proved a
  complete non-null weapon/projectile/muzzle chain.
- A FABRIK solver and tests provide the arm-chain math foundation.
- The normal retail UI frame and pointer remain available as a flat headset
  surface.

These pieces are diagnostic until a live transform and firing proof passes.

## Transform Ownership

- HMD local motion must drive the eye cameras without continuous player/body
  rotation.
- Grip poses drive hand/wrist targets.
- The right aim pose is intended to drive weapon/barrel direction through one
  calibrated transform. Currently it supplies only right-hand IK orientation.
- Retail FNV remains responsible for animations, recoil, spread, ammunition,
  projectile creation, hit tests, and damage.
- The plugin validates scene/runtime state before touching any retail node and
  restores or declines writes during UI, loading, third-person, invalid
  tracking, or rig rebuilds.

## Weapon Proof Order

1. Log controller, camera, player/body, hand, weapon, and muzzle transforms in
   one coordinate space without applying changes.
2. Establish a configurable controller-grip-to-weapon offset and validate axes,
   handedness, scale, and roll.
3. Move the visual weapon and arm target while firing remains unmodified.
4. Compare the controller/weapon ray with the retail muzzle and hit ray.
5. Route firing direction through the smallest guarded in-process hook that
   preserves retail recoil, spread, projectile, and damage logic.
6. Verify alignment after recoil, reload, weapon swaps, crouch, movement, turn,
   recenter, and tracking loss.

## Flat UI Rule

No spatial Pip-Boy or native stereo menu is required for this milestone.
Startup, pause, inventory, Pip-Boy, dialogue, VATS, loading, and any unknown UI
state use the retail mono surface. The right-hand ray may point and click that
surface, but the host does not recreate menu behavior.

## Acceptance

The prop layer is ready when:

- head motion is independent of the player/body frame;
- hands remain stable in the recentered tracking space;
- the visible barrel follows the right aim pose with a documented calibration;
- muzzle/projectile or hit-ray direction and impact agree with the controller
  ray at several distances;
- UI transitions hide/suspend world props as intended and always restore the
  flat retail surface;
- tracking loss and rig rebuilds fail closed without corrupting retail state.
