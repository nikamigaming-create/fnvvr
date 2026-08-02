# FNVVR Phase 0 mutation-site inventory

Audit baseline: `6faed4620d029803ceb129487ebacb64237111ef` on 2026-08-01.

This inventory records all currently known FNVVR mutation seams and their
restoration boundary. An entry without an exact retail-manifest key is blocked
from full-product use; it is not a license to infer an address from a nearby
function. The exact byte hashes below are owned by
`runtime/fnvxr_retail_engine_manifest.h`.

| Capability area | Source seam | Exact validation key | Current authorization | Required restoration |
|---|---|---|---|---|
| Camera | `plugin/fnvxr_nvse_plugin.cpp::installCameraHook` | `PlayerCharacter::UpdateCamera` | Full-product gate is false; desktop-assist is separately narrowed. | Restore camera basis/pose on every rejected frame, UI/load/cell/device transition, and shutdown; remove trampoline when the owning lease ends. |
| World stereo | `renderhook/fnvxr_d3d9_proxy.cpp` retail accumulation bridge | `RenderWorldSceneGraph`, `AccumulateScene`, `BSCullingProcess::ProcessAlt`, `NiAccumulator::*` | CPU-v8 visual trial only; legacy replay remains blocked. | Restore camera, culler, visible array, accumulators, targets, depth/stencil, viewport, shaders, and hook bytes before publication/teardown. |
| First-person render | retail `RenderFirstPerson` observation/transaction seam | `RenderFirstPerson` | No product first-person authorization. | Restore every eye-local first-person transform and target before the other eye, UI, load, or shutdown. |
| Visual rig | `plugin/fnvxr_nvse_plugin.cpp::installRetailRigHook` | `PlayerAnimationApplyCallSiteAddress` has no full-product manifest key | Visual-only tracked-prop assist can be requested; full product is false. | Save/apply/render/restore node transforms; invalidate nodes/calibration on equip, root/animation, mode, load, cell, tracking, and recenter changes. |
| Weapon/projectile observation | `installProjectileNodeConsumeHook` and rig diagnostics | No firing seam has an authorized exact key | Read-only observation only. | No gameplay mutation is permitted; discard transient diagnostics on invalid lineage. |
| Input | xNVSE `processMainGameLoop` controller consumer | Current runtime/compatibility/mapping evidence | Selected future product owner; authorization remains false. | Release all held actions on transition or producer staleness, then require a fresh press. |
| DirectInput/XInput proxies | `renderhook/fnvxr_dinput8_proxy.cpp`, `fnvxr_xinput_proxy.cpp` | `fnvxr_input_proxy_safety.h` source fuses | Transparent product paths; never a second controller owner. | No injected state survives because the proxies forward unchanged while fused. |
| UI Present lease | `renderhook/fnvxr_d3d9_proxy.cpp::initializeRetailVrPresentBootstrap` | Native Present slot is an audited lease, not a retail-manifest function body | Bounded UI/visual-trial setup only. | Uninstall the one leased slot and release mappings/resources on device reset and shutdown. |
| GPU publication | ABI v5 producer/consumer components | Color ABI v5 identity, fence, release, and adapter checks | Component-level only; not release-authorized. | Do not reuse a resource set until the producer observes the matching consumer-release fence; invalidate on device/process/epoch changes. |

## Exact retail function-hash keys

| Manifest entry | Preferred address | SHA-256 |
|---|---:|---|
| `RenderWorldSceneGraph` | `0x00873200` | `D2355FF1593FD9D843C0C61FE95205C1B2C4F1FB6D560499B6FA4EE9C312AEAE` |
| `RenderFirstPerson` | `0x00875110` | `7F734D69C1C74C2099BE684FB4FE682BF84B3F75A108F109CCF1DF74EF9D55F2` |
| `PlayerCharacter::UpdateCamera` | `0x0094AE40` | `6BB45EDC72162B703610CBF425DB949BE060C516F2187A84D8420B2224FB35B5` |
| `NiAccumulator::AddVisibleArray` | `0x00A9B790` | `A929F2C8289B45EC15A0DBFA4F6D2650471A17775AD3950032419C381A3FB20F` |
| `FinalizeAccumulator` | `0x00B6B930` | `C3CA665ECDDCAC09D42561F945B34450340B1354CC2054BF45599426581BB792` |
| `RenderAccumulatorWithoutFinalize` | `0x00B6BA20` | `D493A62CA76EBEF84C25CDFA7B1BA4C10966D1EEED2D0FA0EA4E2D583880C9E4` |
| `AccumulateScene` | `0x00B6BEE0` | `45BF64E849FF26829D43BB52866BF51C412DF95F12F3177159A2BEC2D5838A5A` |
| `RenderAndFinalizeAccumulator` | `0x00B6C0D0` | `1BB66CF7419B4FBF086CA3D2F36BE79F17EEA3D04516C7E4A0A3E4EEAA99119B` |
| `BSCullingProcess::ProcessAlt` | `0x00C4F070` | `213054747E94294B95DABD125B34D430DD0D12137396C97EAE269FCA7A0E301F` |
| `NiAccumulator::SetCamera` | `0x00D47A40` | `B35CAAC991EB0D06657C3FBBA49C85DE5F83E8CF4DE0B07B70FA4B90CB5770FD` |

The loaded image, all required function bytes, object ancestry, compatibility
inventory, producer epoch, pose/runtime lineage, and mode must be checked at
the mutation decision. A retained inventory row or hash does not itself grant
a lease.
