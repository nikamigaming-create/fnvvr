# Headset-Free Assist Mode

`fnvxr_assist` lets us exercise the first VR acceptance gate without opening
OpenXR or wearing a headset: synthetic head motion must change the camera while
the retail player/body remains the locomotion frame.

The harness publishes a deterministic pose trace into the normal pose mapping.
By default it owns no controller state; its explicit `--tracked-prop` fixture
adds only a synthetic right grip/aim pose for the visual-rig trial below. It
starts no renderer, launches no process, installs no hook, sends no input, and
changes no save. A separately approved game process is the only thing that can
elect to consume that trace.

## What the trace covers

The `head-body` scenario begins and ends neutral, then runs:

- `+20` and `-20` degree yaw;
- pitch and roll;
- `+/-0.16 m` lateral lean and `-0.16 m` forward lean.
- when `--tracked-prop` is requested, isolated right-controller position and
  aim-yaw probes while the synthetic head remains neutral.

Every sample keeps the midpoint of the synthetic left/right eyes exactly at the
synthetic HMD position. The harness checks that invariant itself, so it cannot
mistake a malformed eye pair for a real head pose.

## Desktop-assist boundary

The `desktop-assist` profile is an intentionally tiny experiment, not VR
enablement. It is allowed only after the plugin proves the exact, same-process
retail compatibility contract at the camera-hook decision point. It then
permits only a first-person, local-camera yaw/pitch/roll overlay.

By default it expressly denies local translation, world-transform writes, the
unverified Ni transform updater, input injection, controller/weapon/rig
mutation, world stereo, legacy D3D replay, and OpenXR presentation. Selecting
`desktop-assist` without its explicit camera-only opt-in starts neither the
desktop bridge nor the old full bridge.

The supervisor has one separately opt-in unattended acceptance exception:
`-AutomateAcceptance` may submit only the literal
`load FNVXR_HostExitRecovery` command after the real Start Menu is observed,
then make exactly two `Escape` key taps after proving that the owned Fallout
window is foreground. The plugin rejects every other command, accepts that
load only once per game process, and never receives a general input bridge.
This bounded setup is for the repeatable desktop test only; it does not enable
OpenXR, controller input, mouse input, translation, or world stereo.

There is one separately opted-in D3D exception:
`FNVXR_DESKTOP_ASSIST_UI_CAPTURE=1` can lease only the native `Present` slot,
and only to CPU-copy a *confirmed retail menu frame* into
`FNVXR_Desktop_Assist_Ui_Quad_v1`. It does not initialize eye targets, the
world bridge, replay, input, or OpenXR. A non-UI Present invalidates the record
instead of leaving a stale menu image behind.

The plugin publishes a separate `FNVXR_Desktop_Assist_State_v2` record after
the hook runs. It contains the engine's explicit non-first-person player
**body-root** world transform and the camera **local** transform, plus the exact
pose-producer epoch and pose sequence that caused the camera update. The harness
accepts that record only when both the epoch and the sequence are inside the
currently published synthetic phase. This rejects an old pose from the same
producer, not just a different producer. Treating the ordinary camera
world-transform record as proof would create a false failure when world
propagation is intentionally not requested.

That record proves only post-hook local-transform separation. It does not prove
that a renderer consumed the transform, translation works, or the game is true
binocular stereo.

The UI mapping proves a narrower second fact: a non-black CPU menu image was
copied during native Present and committed with the exact runtime frame, menu
state, pose frame, pose sequence, and pose-producer epoch. Its reader recomputes
the pixel hash and non-black count before accepting it, and accepts the capture
only from the currently published synthetic phase. This is evidence of a desktop
menu source capture; it is not evidence that a headset compositor received a
flat quad.

That same narrow Present bootstrap records the live device's D3D9Ex query and
creation parameters in `fnvxr_desktop_assist_ui.log`. It is read-only capability
evidence for the later transport decision, not an attempt to create interop
resources and not authorization for GPU transport, world stereo, or OpenXR.

## Tracked-prop visual trial

`tracked-prop-assist` is a separate, explicit profile for answering the next
question: can a first-person hand/weapon visual follow the right controller
without following head motion? It is not an extension of `desktop-assist` and
is not a general VR or firing path.

Before either the camera hook or animation hook can install, the plugin repeats
the exact same-process retail compatibility proof and requires all of the
following together: local camera yaw/pitch/roll, the visual-only profile flag,
the post-animation rig hook, rig-transform writes, weapon-transform writes,
and a current right grip **and** aim pose. The rig latches the first-person rig
root in the player body's frame, then derives controller targets from that body
anchor. It deliberately does not reuse the camera/HMD origin for the weapon.

The authorization rejects local camera translation, world-transform writes,
the unverified transform updater, input injection, projectile-node hooks,
projectile or hit mutation, world stereo, D3D replay, UI capture, and OpenXR
presentation. In particular, this trial can move a rendered gun model but it
cannot make bullets, raycasts, recoil, or damage originate from that model.

Use the temporary supervisor only with the named switch. It leaves the normal
desktop-assist defaults unchanged and restores the staged files when its owned
game process exits. Once first-person gameplay is ready, run the fixture from
another terminal while that temporary session remains open:

```powershell
.\scripts\start-desktop-assist.ps1 `
  -GameRoot 'D:\SteamLibrary\steamapps\common\Fallout New Vegas' `
  -ApproveStageAndLaunch `
  -TrackedPropVisualTrial

.\build-product-x64\Release\fnvxr_assist.exe `
  --scenario head-body --tracked-prop --step-ms 1200 --period-ms 20 --cycles 2 --no-observe
```

The plugin logs `fnvxrRigIndependence` records with
`originSource="tracked-prop-assist-body"` and
`anchorSource="first-person-rig-root-at-latch"`. A controller-only phase is
the useful check: its `controllerMoved` should be true while `headMoved` is
false, and the gun/hand visual should follow that controller change. Treat that
as a visual transform result only; it does not validate a physical headset,
stereo presentation, collision, or combat behavior.

## Safe checks available now

Validate the trace and its deliberately coupled negative case without opening
any shared mapping:

```powershell
.\build-product-x64\Release\fnvxr_assist.exe --validate-scenario
.\build-product-x64\Release\fnvxr_assist.exe --self-test
```

Run the synthetic pose producer by itself, with no headset and no game:

```powershell
.\build-product-x64\Release\fnvxr_assist.exe `
  --scenario head-body --step-ms 250 --period-ms 5 --cycles 1 --no-observe
```

The automated named-mapping contract test goes one step further: it starts the
real `fnvxr_assist` executable against isolated synthetic desktop-assist,
runtime, and menu-pixel mappings. It verifies body-root stability, including an
adversarial case where head lean is deliberately leaked into the body root and
must be rejected; distinct camera-local yaw/pitch/roll responses;
gameplay-to-menu-to-gameplay ordering; pixel-hash validation; and explicit
post-menu invalidation. It never starts Fallout, OpenXR, or a headset runtime,
so it proves the cross-process evidence contract only—not a physical headset
result.

```powershell
ctest --test-dir .\build -C Release `
  -R "^fnvxr_assist_mapping_integration_test$" --output-on-failure
```

Review the narrow desktop profile before any future stage or launch. This is
read-only: it does not build, copy, start, stop, or set environment variables.

```powershell
.\scripts\validate-desktop-assist.ps1 `
  -GameRoot 'D:\SteamLibrary\steamapps\common\Fallout New Vegas'
```

The current preflight reports that the local Win32 plugin artifact and the
installed game plugin have different hashes. That is expected until someone
explicitly approves a temporary stage; the preflight does not change either
file.

## Approved temporary desktop supervisor

When stage-and-launch approval is explicitly given, use the dedicated
supervisor rather than manually copying DLLs. It stages only the Win32 D3D9
proxy and NVSE plugin, verifies the exact loaded module hashes, starts no
OpenXR process, and automatically restores the two prior game files after its
owned Fallout process stops or its bounded desktop session expires.

Before that live supervisor performs any preflight or game-tree action, it
rebuilds the exact artifacts it will use: the Win32 proxy and NVSE plugin, plus
the x64 assist, shared-state, command, and read-only runtime-evidence tools. A
build failure stops the run before anything in the game installation changes.
This keeps a successful acceptance trace tied to the source revision being
tested, rather than to a stale DLL left in a product build directory.

```powershell
.\scripts\start-desktop-assist.ps1 `
  -GameRoot 'D:\SteamLibrary\steamapps\common\Fallout New Vegas' `
  -ApproveStageAndLaunch
```

`-ValidateOnly` reports that same build plan but does not build it, create a
run directory, change the game tree, launch a process, or set `FNVXR_*`
environment variables. The normal live command still does not inject input or
enter a save; use normal desktop controls to reach first-person gameplay and
to open and close a menu during the later acceptance commands below.

For the repeatable headset-free acceptance run, add `-RunAcceptanceTrial`. The
supervisor waits for an explicit first-person body-root observation before it
starts the synthetic trace, then stores both the harness JSON report and the
supervisor manifest in its run directory. It rejects a stale same-epoch pose,
an unchanged rotation phase, a moved body root, an unpaired menu capture, or a
menu image left valid after returning to gameplay. A passing trial immediately
closes its owned desktop game session and restores the two staged files. In its
default form it sends no input, so the visible desktop game must be taken to
first-person gameplay and a normal menu must be opened and closed during the
trace.

```powershell
.\scripts\start-desktop-assist.ps1 `
  -GameRoot 'D:\SteamLibrary\steamapps\common\Fallout New Vegas' `
  -ApproveStageAndLaunch `
  -RunAcceptanceTrial
```

For the fully supervised, headset-free variant, add
`-AutomateAcceptance`. Preflight must find the already existing
`FNVXR_HostExitRecovery.fos`; the process then accepts one exact recovery load
at the real Start Menu and the supervisor makes one menu open/close round-trip
with two foreground-verified `Escape` taps. It records the command result,
both key records, runtime samples, the UI pixels, and the acceptance report.
Any wrong window, missing save, rejected command, stale UI record, or failed
body-root check fails the run and triggers normal cleanup.

```powershell
.\scripts\start-desktop-assist.ps1 `
  -GameRoot 'D:\SteamLibrary\steamapps\common\Fallout New Vegas' `
  -ApproveStageAndLaunch `
  -RunAcceptanceTrial `
  -AutomateAcceptance
```

To make the same short, headset-free session useful for the later world-stereo
work, add `-CollectEngineEvidence`. After the exact body-root readiness gate,
the supervisor runs `fnvxr_retail_runtime_probe.exe` against its owned Fallout
process with read-only process access and stores the output as
`retail-runtime-evidence.log`. A nonzero probe exit is recorded as incomplete
evidence, not turned into a fake pass; the collector neither enables stereo
nor changes the game process.

```powershell
.\scripts\start-desktop-assist.ps1 `
  -GameRoot 'D:\SteamLibrary\steamapps\common\Fallout New Vegas' `
  -ApproveStageAndLaunch `
  -RunAcceptanceTrial `
  -CollectEngineEvidence
```

## A later desktop-only acceptance run

After an explicitly approved, temporary desktop stage and manually launched
first-person Fallout session, first verify the narrow bridge is alive:

```powershell
.\build-product-x64\Release\fnvxr_shared_state_probe.exe `
  --require-desktop-assist --require-advancing --sample-delay-ms 250
```

While a normal desktop menu is visibly open, verify the actual menu-source
record. This requires no headset, but it intentionally fails outside a
confirmed menu because stale images are invalidated.

```powershell
.\build-product-x64\Release\fnvxr_shared_state_probe.exe `
  --require-desktop-assist-ui-quad --require-advancing --sample-delay-ms 250
```

Then run the scripted head/body and UI-source transition check. During the run,
use ordinary desktop controls to enter a menu and return to gameplay at least
once; the harness sends no input.

```powershell
.\build-product-x64\Release\fnvxr_assist.exe `
  --scenario head-body --step-ms 1200 --period-ms 20 --cycles 2 `
  --require-desktop-assist --require-ui-quad-transition `
  --report .\local\assist-head-body.json
```

That command exits nonzero unless every rotational and lean phase has a valid
current-phase post-hook desktop-assist sample, the explicit body root stays
within 3 degrees and 0.25 Gamebryo world units of baseline, and each
camera-local rotation response is both large enough and distinct from the other
requested rotation phases. Its UI requirement also rejects a stale image, a
wrong pose producer, an earlier pose from the same producer, or a menu capture
whose recorded runtime frame does not match the surrounding UI observation. It
recomputes the captured pixel hash once during the UI visit, then requires that
record to be explicitly invalidated after the return to gameplay. It needs no
headset.

`--require-6dof` is supposed to fail in this first transaction because
translation is deliberately disabled. `--expect-ui-transition` observes only a
desktop runtime `gameplay -> UI -> gameplay` transition while the trace runs;
it is not evidence of pixels. `--require-ui-quad-transition` is the stronger
desktop source-capture check described above. Neither command proves a rendered
headset quad; the manual command expects normal desktop controls, while the
approved supervisor can make its narrow two-key transition using
`-AutomateAcceptance`.

## Future headset-free world-stereo evidence

The world-rendering gate has its own desktop probe; it is deliberately separate
from assist mode. Its source and shared-protocol path are compiled and tested
headlessly, but live evidence remains unavailable until a separately authorized
desktop session produces real retail frames. That command requires the
engine-center renderer specifically, a current same-transaction world pair,
non-black left and right payloads, and at least one real RGB pixel difference
between those payloads. It uses shared-memory readback only, so it does not
require wearing a headset:

```powershell
.\build-product-x64\Release\fnvxr_shared_state_probe.exe `
  --require-engine-center-world-stereo --require-advancing --sample-delay-ms 250
```

This is intentionally stronger than the generic `--require-world-stereo`
check: it rejects the old replay producer and every non-engine-center producer.
It is evidence that two rendered images exist; it is not yet a substitute for
the later in-headset comfort, scale, and UI-composition test.

For the specific menu-return failure that previously let an old world image
come back after UI, use the transition verifier during a controlled desktop
session:

```powershell
.\build-product-x64\Release\fnvxr_shared_state_probe.exe `
  --require-ui-then-engine-center-world --transition-timeout-ms 5000
```

It observes one visible, exact-mono `StereoProducerMonoUiQuad` record, then
accepts a world only when the later record is an `EngineCenter` pair with
non-black, meaningfully different eye pixels and strictly newer transaction,
source-frame, and publication-generation identities from the same renderer
epoch. It rejects a non-mono or mislabeled menu, a stale pair, and a world that
was published before the menu.
Like the other shared-memory checks, it opens neither OpenXR nor a headset.

When `--require-advancing` is used, the probe tracks the stereo header's
nonzero 64-bit `publicationGeneration`, not its wrapped 32-bit display
sequence. A new-looking sequence with a retained or regressed generation is
rejected; the valid `UINT64_MAX -> 1` nonzero-generation wrap remains valid.
That closes an ABA/stale-publication hole in the headset-free evidence check;
it still does not turn CPU pixels or a synthetic mapping into a headset result.

The later engine-center evidence chain carries that same generation as a
decimal string from the CPU pair publisher into the host submit record. Its
verifier requires an exact source/host generation match and rejects a
regressed generation even if the older 32-bit render-pair token still appears
to advance. It accepts the valid nonzero `UINT64_MAX -> 1` wrap. This is a
lineage and freshness contract, not permission to enable the OpenXR renderer.

## What remains unproven

The next gates remain separate:

- safe local translation and recenter calibration;
- a real headset UI-quad submission and return to world;
- a renderer/pixel proof that the local camera transform is consumed;
- true same-transaction binocular world rendering and OpenXR presentation.

The old D3D draw replay is not an answer to any of those gates. It remains a
diagnostic-only path and cannot be reclassified as true 3D.
