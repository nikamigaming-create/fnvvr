using System;
using System.Runtime.InteropServices;
using Fnvxr.Adapters;
using Godot;

NativeLibrary.SetDllImportResolver(typeof(RigAdapter).Assembly,
    (name, assembly, path) => name == "fnvxr_rig" ? NativeLibrary.Load(args[0]) : IntPtr.Zero);
if (RigAdapter.fnvxr_rig_abi_version() != 1 || Marshal.SizeOf<RigAdapter.Pose>() != 28
    || Marshal.SizeOf<RigAdapter.Arm>() != 88) throw new Exception("Rig ABI layout mismatch");
var grip = new Transform3D(new Basis(Vector3.Up, .7f), new Vector3(-.2f, 1.4f, -.4f));
var mount = new Transform3D(new Basis(Vector3.Right, -.4f), new Vector3(-.065f, -.021f, .09f));
var a = RigAdapter.Pose.FromGodot(grip);
var b = RigAdapter.Pose.FromGodot(mount);
if (RigAdapter.fnvxr_rig_compose_v1(in a, in b, out var output) != 1
    || !output.ToGodot().IsEqualApprox(grip * mount)) throw new Exception("Godot attachment composition differs");
var shoulder = RigAdapter.Pose.FromGodot(new Transform3D(Basis.Identity, new Vector3(-.19f, 1.46f, 0)));
if (RigAdapter.fnvxr_rig_solve_arm_v1(1, .31f, .27f, in shoulder, in a, out var arm) != 1
    || !arm.Wrist.ToGodot().Origin.IsEqualApprox(grip.Origin)) throw new Exception("Managed/native arm solve differs");
Console.WriteLine("Godot 4.7.2: native ABI, rigid pose conversion, wrist attachment and arm solve passed.");
