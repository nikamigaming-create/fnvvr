using System;
using System.Runtime.InteropServices;
using Godot;

namespace Fnvxr.Adapters;

// Pass poses relative to the same XROrigin3D/reference space as the source
// eye frame. World-space scale and skeleton-local transforms stay in OpenNV.
public static class RigAdapter
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Pose
    {
        public float X, Y, Z, Qx, Qy, Qz, Qw;
        public static Pose FromGodot(Transform3D pose)
        {
            var scale = pose.Basis.Scale;
            if (!scale.IsEqualApprox(Vector3.One))
                throw new ArgumentException("Convert asset units/scale to XR meters before the rig boundary.");
            var q = pose.Basis.GetRotationQuaternion();
            return new() { X = pose.Origin.X, Y = pose.Origin.Y, Z = pose.Origin.Z,
                Qx = q.X, Qy = q.Y, Qz = q.Z, Qw = q.W };
        }
        public readonly Transform3D ToGodot() => new(
            new Basis(new Quaternion(Qx, Qy, Qz, Qw)), new Vector3(X, Y, Z));
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct Arm
    {
        public Pose Shoulder, Elbow, Wrist;
        public uint ReachClamped;
    }
    [DllImport("fnvxr_rig", CallingConvention = CallingConvention.Cdecl)]
    public static extern uint fnvxr_rig_abi_version();
    [DllImport("fnvxr_rig", CallingConvention = CallingConvention.Cdecl)]
    public static extern int fnvxr_rig_compose_v1(in Pose parent, in Pose local, out Pose output);
    [DllImport("fnvxr_rig", CallingConvention = CallingConvention.Cdecl)]
    public static extern int fnvxr_rig_solve_arm_v1(uint left, float upperArmMeters,
        float forearmMeters, in Pose shoulder, in Pose wrist, out Arm output);
}
