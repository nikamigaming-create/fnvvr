# Prop Layer Plan

The playable target is not a new placeholder renderer. It is the smallest glue layer that lets the existing OpenMWVR FNV hand, Pip-Boy, pointer, and GUI systems operate beside retail `FalloutNV.exe`.

The current bridge already proves:

- retail FNV can load the xNVSE DLL;
- the OpenXR host can send headset/controller packets;
- FNV can answer with `GameFrame` packets.

The next step is reuse, not reinvention.

## Product Path

- FNV keeps running normally and remains authoritative for game state.
- The xNVSE DLL receives pose/action frames and injects normal FNV input/mouse/activate events.
- The OpenXR side reuses the OpenMWVR hand/Pip-Boy/pointer implementation documented in `openmwvr-reuse-map.md`.
- The D3D9/depth/stereo hook is added only after hands/Pip-Boy/pointer behave correctly.

## Visual Target

First acceptable headset behavior:

- left and right hands use the existing FNV hand meshes, bones, and finger curl mapping;
- trigger moves the index finger;
- squeeze/grip curls the other fingers;
- thumb/touch values use the existing OpenMWVR thumb source choices;
- Pip-Boy uses the existing wrist/socket alignment;
- pointer uses the existing ray/focus/click model.

The old colored quads and colored cubes are diagnostics only. They are not the playable prop layer.

## UI Target

The first UI target is mouse-equivalent behavior:

- if the pointer is over a VR GUI layer, the existing `VRGUIManager::injectMouseClick` pattern is the model;
- if the pointer is over the retail game, the xNVSE side should inject a normal mouse/activate action;
- the normal FNV Pip-Boy can open first, while the wrist Pip-Boy mesh proves hand attachment and spatial alignment.

Later, the D3D9 hook can capture or mirror the FNV Pip-Boy texture onto the wrist surface.

## Implementation Notes

Use `openmwvr-reuse-map.md` as the source-of-truth file list. In particular:

- OpenXR input comes from `mwvr/openxrinput.*`;
- hands/finger curl/Pip-Boy attachment come from `mwvr/vranimation.*`;
- pointer/click behavior comes from `mwvr/vrpointer.*` and `mwvr/vrgui.*`;
- FNV-specific hand/Pip-Boy calibration comes from the local FNV VR run profile.
