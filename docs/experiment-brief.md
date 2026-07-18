# Experiment Brief

FNVVR targets the retail Windows Fallout: New Vegas executable directly.

## Runtime Split

```text
OpenXR headset and controllers
              |
              v
standalone FNVXR OpenXR host
tracking, input intent, frame timing, flat UI surface, VR composition
              |
              | fixed shared state / GPU handles, fences, transaction metadata
              v
retail FalloutNV.exe
xNVSE plugin + D3D9/DirectInput/XInput proxies
gameplay, UI, camera, weapon, hit tests, animation, retail rendering
```

There is one authoritative game runtime. Shared memory is a same-machine ABI,
not a simulation boundary or a network protocol.

## Authority Boundary

The host owns:

- predicted HMD and eye poses;
- grip and aim controller poses;
- OpenXR actions and composition;
- the stable flat retail UI surface;
- non-authoritative debug rays, hands, and calibration aids.

Retail FNV owns:

- player/body state and movement;
- camera and scene graph;
- equipped weapon, animation, muzzle, spread, recoil, projectile, and hit tests;
- menus, dialogue, Pip-Boy, VATS, loading, and save state;
- world rendering and final gameplay results.

## Live Bridge Nucleus

Shared mappings use fixed-size, versioned layouts. Any mapping read
concurrently must define a stable-snapshot protocol, such as a sequence guard
or frame-header writing flag, and every mutable field must have one declared
producer. Consumers reject invalid headers, unstable snapshots, and
incompatible runtime phases. This is a bridge acceptance requirement, not a
blanket claim that every current mapping already satisfies it.

The XInput v2 and DirectInput v9 frame fields now use odd/even sequence guards.
Their producers finish all buttons, triggers, sticks, pointer, and gameplay
fields before publishing an even sequence; the plugin and retail proxies use
stable local snapshots or reject that poll. The three XInput reserved bytes
are explicitly separate consumer/plugin-owned acknowledgement and movement
status lanes rather than producer-frame fields.

DirectInput v9 publishes head/hand look as cumulative signed angle counters.
The retail proxy differences each producer frame once, so polling the same
frame cannot repeat a delta and skipped producer frames do not discard the
intervening tracked rotation.

Host-to-retail state includes HMD/eye/controller poses, tracking validity,
buttons, triggers, grips, thumbsticks, pointer position, and input intent.

Retail-to-host state includes runtime/UI phase, player/body and camera
transforms, weapon classification and rig diagnostics, plus versioned mono-UI
or stereo GPU resource metadata. Eye pixels do not travel through a CPU ring.
Stereo GPU metadata is published only through the fully ordered odd/even
producer helper, and the host must observe the declared shared fence itself.

## Presentation Contract

The normal retail mono frame is the UI source. Confirmed startup/menu, pause,
Pip-Boy/inventory, dialogue, VATS, and loading states are shown flat in the
headset. Unknown/stale runtime fails blank and cannot manufacture an allowed
quad. UI input remains ordinary retail mouse/keyboard/controller input.

All non-blocking gameplay requires native stereo with no persistent gameplay
HUD. A world transition requires a fresh, complete, pose-matched same-frame
color/depth pair whose retail source frame strictly postdates the last displayed
UI frame. That UI freshness watermark survives transition-hold expiry. A UI
transition enters the flat retail surface; an invalid or stale gameplay
transaction is rejected and is not reported as mono VR.

## Current Milestone

The next milestone is not another renderer shell. It is a live retail proof
that:

1. HMD motion changes the eye camera independently of the player/body frame;
2. the right controller, retail weapon/barrel, muzzle/projectile direction, and
   hit result agree;
3. all UI paths remain usable on the flat retail surface;
4. the complete retail world eye pair can then be presented safely in OpenXR.
