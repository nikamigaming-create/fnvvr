# FNVXR VR capability assessment - 2026-08-09

## Bottom line

The current build proves retail Fallout stereo, tracked head/controller motion,
locomotion, firing, and reload. It now also proves that the authentic retail
left forearm/hand, right-hand weapon, and Pip-Boy model can be included in the
first-person stereo roots. It does **not** yet prove the product interaction the
next milestone requires: opening a live wrist-mounted Pip-Boy with a controller,
selecting a ranged or melee inventory item, and observing that exact selection
replace the equipped model in the hand.

Do not present the current showcase as proof of that milestone.

## Newly verified exact-node run

Run: `20260809-211305-358-09f147c46f1e`

- Fresh attested build: 238 tests passed (120 Win32 and 118 x64).
- First-person root mask `31` requested weapon, upper body, left hand, right
  hand, and Pip-Boy.
- The final stereo mirror visibly contains the retail left forearm/hand and
  equipped pistol. These are game meshes, not the host's debug cubes.
- The run reached sustained binocular output and retained 300 stereo pairs.
- The bounded combat harness recorded 15 attacks, 2 reload events, 113 rig
  events, and 424.117 world units of player movement.
- A per-run headless controller chord was acknowledged by the simulator, but
  the retail runtime remained in gameplay with `runtimeMenuBits=0`. Therefore
  it is **not** Pip-Boy-open proof.
- A follow-up bounded `HeadsetDemoFixture` run
  (`20260809-211700-083-a6be4e7ff199`) also failed closed: no proven binocular
  engine-stereo frame reached OpenXR before its supervised time limit, so it
  produced no acceptable Pip-Boy footage.
- The manifest correctly remains `accepted=false` and
  `fullProductAccepted=false`; this is a simulator visual trial.

## Exact acceptance gate for hands, fingers, Pip-Boy, and inventory

A proof video is acceptable only when one continuous, auditable retail run
shows all of the following:

1. Distinct left/right OpenXR eye output from the retail renderer.
2. The authentic left and right hand meshes plus the retail finger skeleton;
   synthetic cube hands/fingers are forbidden evidence.
3. The retail Pip-Boy model attached to the left forearm with its live menu
   texture spatially presented at that wrist; a detached flat fallback quad is
   insufficient.
4. Controller input opens the Pip-Boy and navigates the actual inventory menu.
5. Accepting a ranged item changes the retail equipped form ID/class and the
   visible model in the hand.
6. Accepting a melee item produces a second, independently observed form/class
   and visible-model transition.
7. Telemetry ties controller action, menu state, selected item, equipped form,
   first-person node, stereo source frame, and captured output into the same
   run lineage.
8. The video has captured game audio, narration/overlays, and no claim broader
   than the retained evidence.

## Current engineering gap

The plugin already discovers and publishes real `LeftHand`, `RightHand`, and
`PipBoy` nodes, including named thumb and finger phalanges. It also has direct
TileMenu navigation/click plumbing and publishes the equipped weapon form and
class. Those pieces are not yet joined into one accepted wrist-inventory swap
trial. Current menu presentation switches to a mono UI plane, so the run cannot
simultaneously prove the live wrist display and the real stereo hand roots.

## Showcase provenance

The existing showcase is a truthful combat-capability reel only. It uses
retained simulator stereo frames, retained combat audio, narration, and
overlays. Its composition is file-only and does not control the desktop,
windows, keyboard, mouse, registry, or simulator UI.
