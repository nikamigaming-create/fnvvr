# OpenMWVR Reuse Map

This experiment should reuse the working OpenMWVR/FNV VR systems, not invent a new hand/Pip-Boy stack.

## Goal

Retail `FalloutNV.exe` remains the game. The glue layer moves pose, input, clicks, and state between:

- OpenXR device tracking and actions
- the existing OpenMWVR hand, Pip-Boy, pointer, and GUI behavior
- the xNVSE bridge inside retail FNV
- a future D3D9/depth/stereo hook when the flat game view needs to become true in-world stereo

## Existing Systems To Reuse

### OpenXR Input

Authoritative donor implementation in
[`nikami-openmw-lab`](https://github.com/nikamigaming-create/nikami-openmw-lab):

- `apps/openmw/mwvr/openxrinput.hpp`
- `apps/openmw/mwvr/openxrinput.cpp`
- `files/openxrinteractionprofiles.xml`

Important anchors:

- `OpenXRInput::createActionSets`
- `OpenXRInput::createPoseActions`
- `OpenXRInput::attachActionSets`
- `OpenXRInput::createActionSpace`
- `OpenXRInput::LeftHandAim`
- `OpenXRInput::LeftHandGrip`
- `OpenXRInput::RightHandAim`
- `OpenXRInput::RightHandGrip`

Reuse rule:

- Do not hand-code one Oculus-only action table as the final path.
- Use the OpenMWVR interaction-profile enumeration and action path model.
- For retail FNV, serialize the resolved action values into `PoseFrame`.

### Hand Meshes And Finger Curl

Authoritative donor implementation in `nikami-openmw-lab`:

- `apps/openmw/mwvr/vranimation.cpp`
- `apps/openmw/mwvr/vranimation.hpp`

Important anchors:

- `FingerCurlSource`
- `getFingerSensorIds`
- `getCurlActionIds`
- `getFingerCurlAnglesDegrees`
- `FingerController::getCurl`
- `FingerController::operator()`
- `HandController::operator()`
- `BindFalloutVrHandFingerControllersVisitor`
- log proof: `VR static hand finger deform attached`
- log proof: `VRHandsOnly direct hand wrapper attached`
- log proof: `VRHandsOnly attached surfaces count`

Known FNV hand assets from previous proofs:

- `meshes/characters/_male/lefthand.nif`
- `meshes/characters/_male/righthand.nif`
- left hand/Pip-Boy glove surfaces such as `LeftHandPipBoyGlove`
- bones: `Bip01 L Hand`, `Bip01 R Hand`, `bip01 l finger1`, `bip01 r finger1`, and the other Fallout finger chains

Reuse rule:

- Do not replace the hand system with cubes or a new synthetic rig.
- Port the existing finger curl source mapping: thumb from thumbrest/proximity, index from trigger, remaining fingers from squeeze/grip with trigger fallback.
- Keep the existing direct-pose/staticized-hand proof path as the first visual target.

### Pip-Boy Wrist Attachment

Authoritative donor implementation in `nikami-openmw-lab`:

- `apps/openmw/mwvr/vranimation.cpp`
- launcher calibration from the local FNV VR run profile

Important anchors:

- `getPipBoyEnvFloat`
- `OPENMW_FNV_PIPBOY_*`
- `OPENMW_FNV_RIGHT_PIPBOY_*`
- log proof: `PipBoy wrist offset applied`

Known calibration from the local FNV VR run profile:

```bat
OPENMW_FNV_PIPBOY_ROT_X=0
OPENMW_FNV_PIPBOY_ROT_Y=0
OPENMW_FNV_PIPBOY_ROT_Z=90
OPENMW_FNV_PIPBOY_OFFSET_X=-3
OPENMW_FNV_PIPBOY_OFFSET_Y=-13
OPENMW_FNV_PIPBOY_OFFSET_Z=-6.5
OPENMW_FNV_PIPBOY_SOCKET_MODEL_X=17.0616
OPENMW_FNV_RIGHT_PIPBOY_CALIBRATION=1
OPENMW_FNV_RIGHT_PIPBOY_SOCKET_MIRROR_SCALE_X=1
OPENMW_FNV_RIGHT_PIPBOY_SOCKET_MIRROR_SCALE_Y=-1
OPENMW_FNV_RIGHT_PIPBOY_SOCKET_MIRROR_SCALE_Z=1
OPENMW_FNV_RIGHT_PIPBOY_VISUAL_OFFSET_X=3.0
OPENMW_FNV_RIGHT_PIPBOY_VISUAL_OFFSET_Y=0
OPENMW_FNV_RIGHT_PIPBOY_VISUAL_OFFSET_Z=0.25
```

Reuse rule:

- Use the wrist/socket/bounds attachment algorithm already in `vranimation.cpp`.
- Use the launcher values as the default calibration for the FNVXR bridge.
- The first retail-FNV build can trigger the normal Pip-Boy menu, but the wrist prop should use the existing 3D Pip-Boy attachment path.

### Pointer And Mouse-Like Clicks

Authoritative donor implementation in `nikami-openmw-lab`:

- `apps/openmw/mwvr/vrpointer.cpp`
- `apps/openmw/mwvr/vrpointer.hpp`
- `apps/openmw/mwvr/vrgui.cpp`
- `apps/openmw/mwvr/vrgui.hpp`

Important anchors:

- `UserPointer::setSource`
- `UserPointer::update`
- `UserPointer::activate`
- `Util::getPoseTarget`
- `VRGUIManager::updateFocus`
- `VRGUIManager::computeGuiCursor`
- `VRGUIManager::injectMouseClick`
- log proof: `VR pointer activate hit target`
- log proof: `inventory panel GUI click`

Reuse rule:

- Keep one pointer source, normally dominant-hand aim pose.
- If the pointer hits a VR GUI layer, inject a mouse-style click into that GUI.
- If the pointer hits the game world, retail FNV/xNVSE must own the actual activation or click effect.

## Minimal Glue Contract

### Host To Retail FNV

`PoseFrame` remains the bridge packet, but its fields should be sourced from the OpenMWVR action model:

- HMD pose
- left/right aim pose
- left/right grip pose or wrist pose when available
- trigger curl/value
- squeeze/grip curl/value
- thumb/touch/button bits
- click intent for pointer/menu/game actions

### Retail FNV To Host

`GameFrame` should stay small and authoritative:

- player/body/world anchor
- menu/Pip-Boy/dialogue/loading state
- equipped weapon/ammo
- raycast result or reference id when FNV owns the raycast
- haptic/event hints later

### Rendering

The immediate visual target is not a new empty OpenXR scene. It is:

1. use the existing OpenMWVR hand/Pip-Boy/pointer implementation as the prop renderer;
2. feed it OpenXR poses/actions through the same action IDs it already expects;
3. let the xNVSE plugin trigger normal FNV input/mouse behavior;
4. add D3D9/depth/stereo only when props need occlusion or the FNV world needs true stereo.

## First Port Slice

1. Extend the host packet fill so trigger/squeeze/thumb/button values match `getCurlActionIds`.
2. Extract or mirror the OpenMWVR hand/Pip-Boy calibration constants into the FNVXR host config.
3. Replace the diagnostic colored prop renderer with the OpenMWVR hand/Pip-Boy asset path.
4. Route pointer click intent to xNVSE so it can inject normal FNV mouse/activate input.
5. Keep D3D9 hook work separate until the prop layer is visibly correct.
