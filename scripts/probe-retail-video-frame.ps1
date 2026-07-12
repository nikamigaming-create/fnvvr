param(
    [string]$OutputRoot = "",
    [int]$TimeoutSeconds = 10,
    [switch]$NoPreview
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
if (-not $OutputRoot) {
    $OutputRoot = Join-Path $Root "local\retail-video-probes"
}
$Stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$RunDir = Join-Path $OutputRoot $Stamp
New-Item -ItemType Directory -Force -Path $RunDir | Out-Null

if (-not ("FnvxrRetailVideoFrameProbe" -as [type])) {
    Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Threading;

public static class FnvxrRetailVideoFrameProbe {
    const UInt32 SharedVideoMagic = 0x46585646;
    const int HeaderSize = 28;
    const int MaxWidth = 4096;
    const int MaxHeight = 2560;
    const int MappingBytes = HeaderSize + MaxWidth * MaxHeight * 4;
    const int FILE_MAP_READ = 0x0004;

    [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Ansi)]
    static extern IntPtr OpenFileMapping(int desiredAccess, bool inheritHandle, string name);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern IntPtr MapViewOfFile(IntPtr mapping, int desiredAccess, uint offsetHigh, uint offsetLow, UIntPtr bytesToMap);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern bool UnmapViewOfFile(IntPtr address);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern bool CloseHandle(IntPtr handle);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern void CopyMemory(IntPtr destination, IntPtr source, UIntPtr length);

    static int ReadInt32(IntPtr ptr, int offset) {
        return Marshal.ReadInt32(ptr, offset);
    }

    static byte[] CopyPlane(IntPtr source, int width, int height, int pitchBytes) {
        int rowBytes = width * 4;
        byte[] pixels = new byte[rowBytes * height];
        GCHandle handle = GCHandle.Alloc(pixels, GCHandleType.Pinned);
        try {
            IntPtr destination = handle.AddrOfPinnedObject();
            for (int y = 0; y < height; ++y) {
                IntPtr srcRow = IntPtr.Add(source, y * pitchBytes);
                IntPtr dstRow = IntPtr.Add(destination, y * rowBytes);
                CopyMemory(dstRow, srcRow, (UIntPtr)rowBytes);
            }
        } finally {
            handle.Free();
        }
        return pixels;
    }

    static void SavePreview(byte[] pixels, int width, int height, string path) {
        GCHandle handle = GCHandle.Alloc(pixels, GCHandleType.Pinned);
        try {
            using (var bitmap = new Bitmap(width, height, width * 4, PixelFormat.Format32bppArgb, handle.AddrOfPinnedObject())) {
                bitmap.Save(path, ImageFormat.Png);
            }
        } finally {
            handle.Free();
        }
    }

    static string Hex(byte[] bytes) {
        char[] chars = new char[bytes.Length * 2];
        const string hex = "0123456789abcdef";
        for (int i = 0; i < bytes.Length; ++i) {
            chars[i * 2] = hex[bytes[i] >> 4];
            chars[i * 2 + 1] = hex[bytes[i] & 15];
        }
        return new string(chars);
    }

    public static string Probe(string runDir, int timeoutSeconds, bool savePreview) {
        var deadline = DateTime.UtcNow.AddSeconds(timeoutSeconds);
        IntPtr mapping = IntPtr.Zero;
        IntPtr view = IntPtr.Zero;
        int lastSequence = 0;
        try {
            while (DateTime.UtcNow < deadline) {
                if (mapping == IntPtr.Zero) {
                    mapping = OpenFileMapping(FILE_MAP_READ, false, "Local\\FNVXR_D3D9_Frame_v1");
                    if (mapping == IntPtr.Zero) {
                        Thread.Sleep(100);
                        continue;
                    }
                    view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, (UIntPtr)MappingBytes);
                    if (view == IntPtr.Zero) {
                        CloseHandle(mapping);
                        mapping = IntPtr.Zero;
                        Thread.Sleep(100);
                        continue;
                    }
                }

                UInt32 magic = unchecked((UInt32)ReadInt32(view, 0));
                int writing = ReadInt32(view, 4);
                int sequence = ReadInt32(view, 8);
                int width = ReadInt32(view, 12);
                int height = ReadInt32(view, 16);
                int pitchBytes = ReadInt32(view, 20);
                int format = ReadInt32(view, 24);

                bool sane = magic == SharedVideoMagic
                    && writing == 0
                    && sequence != 0
                    && sequence != lastSequence
                    && width > 0 && width <= MaxWidth
                    && height > 0 && height <= MaxHeight
                    && pitchBytes >= width * 4
                    && pitchBytes <= MaxWidth * 4;
                if (!sane) {
                    Thread.Sleep(50);
                    continue;
                }

                IntPtr pixelsStart = IntPtr.Add(view, HeaderSize);
                byte[] pixels = CopyPlane(pixelsStart, width, height, pitchBytes);
                if (ReadInt32(view, 4) != 0 || ReadInt32(view, 8) != sequence) {
                    lastSequence = sequence;
                    Thread.Sleep(25);
                    continue;
                }

                byte[] hash;
                using (SHA256 sha = SHA256.Create())
                    hash = sha.ComputeHash(pixels);

                string preview = "";
                if (savePreview) {
                    preview = Path.Combine(runDir, "retail-frame.png");
                    SavePreview(pixels, width, height, preview);
                }

                string raw = Path.Combine(runDir, "retail-frame.bgra");
                File.WriteAllBytes(raw, pixels);

                string json =
                    "{\n" +
                    "  \"mapping\": \"Local\\\\FNVXR_D3D9_Frame_v1\",\n" +
                    "  \"magic\": \"0x" + magic.ToString("x8") + "\",\n" +
                    "  \"sequence\": " + sequence + ",\n" +
                    "  \"width\": " + width + ",\n" +
                    "  \"height\": " + height + ",\n" +
                    "  \"pitchBytes\": " + pitchBytes + ",\n" +
                    "  \"format\": " + format + ",\n" +
                    "  \"sha256\": \"" + Hex(hash) + "\",\n" +
                    "  \"raw\": \"" + Path.GetFileName(raw) + "\",\n" +
                    "  \"preview\": \"" + (savePreview ? Path.GetFileName(preview) : "") + "\"\n" +
                    "}\n";
                File.WriteAllText(Path.Combine(runDir, "retail-video-probe.json"), json);
                return json;
            }
            throw new TimeoutException("Timed out waiting for Local\\FNVXR_D3D9_Frame_v1.");
        } finally {
            if (view != IntPtr.Zero) UnmapViewOfFile(view);
            if (mapping != IntPtr.Zero) CloseHandle(mapping);
        }
    }
}
'@
}

[FnvxrRetailVideoFrameProbe]::Probe($RunDir, $TimeoutSeconds, -not $NoPreview)
