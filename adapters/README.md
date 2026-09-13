# Rig adapters

The shared C++ kernel owns pose composition and geometric rig targets. `rig_c_api`
exposes those same implementations to C# without engine pointers or allocations.
It does not contain Fallout hooks, OpenMW code, Godot objects or owned assets.

The boundary uses meters, right-handed +Y up / -Z forward and XYZW quaternions.
Pass all poses relative to one XR reference space and the exact source eye-frame
identity. A retained eye pair retains its original rig poses; current input may
hit its submitted UI surface, but must not relabel its world or weapon pixels.

| Engine | Adapter | Native responsibilities |
| --- | --- | --- |
| Retail FNV | `host/fnvxr_openxr_spatial_adapter.*` | Exact x86 render/animation publication, source hand sockets, live native inventory and UI pixels |
| OpenMW | `openmw/rig_adapter.hpp` | `Stereo::Pose` / OSG axes and unit conversion, native animation and MyGUI layer/input ownership |
| OpenNV / Godot | `godot/RigAdapter.cs` | `Transform3D` in XR meters, source `Skeleton3D` bones/twist helpers, equipped device geometry, native viewport/inventory state |

These adapters do not replace either engine's gameplay or skinning. Grip-to-hand,
hand-to-device and weapon sockets must come from that game's source rig. Controller
aim remains separate from grip position. OpenNV's contact/reach constraint runs
before the shared solve; the solve preserves the supplied physical wrist endpoint.
Menus own input while their independently rendered surfaces appear inside the
stereo world. A screen crop and its pointer coordinates must use the same mapping.

The OpenMW adapter was compiled and executed against the actual `Stereo::Pose`
and OSG headers/libraries in `nikami-openmw-lab`. The Godot adapter was compiled
and executed with OpenNV's GodotSharp 4.7.2 assembly, including a native DLL call,
rotated wrist attachment, ABI layout and arm solve. The other games have not yet
been switched to this library or passed integrated gameplay/headset acceptance.

Build `fnvxr_rig` in the main CMake project. Its native API test runs with CTest.
The optional engine-type checks live in `tests/adapters/openmw` and
`tests/adapters/godot`; supply the local engine headers/libraries or
`GodotSharpPath`, plus the built rig library. No engine installation is required
for ordinary kernel builds.
