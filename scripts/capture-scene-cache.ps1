param(
    [string]$OutputRoot = "",
    [string]$SceneName = "scene-0",
    [int]$TimeoutSeconds = 45,
    [switch]$AllowMono,
    [switch]$AllowNonWorldCandidate
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
if (-not $OutputRoot) {
    $OutputRoot = Join-Path $Root "local\scene-cache"
}
$SceneDir = Join-Path $OutputRoot $SceneName
New-Item -ItemType Directory -Force -Path $SceneDir | Out-Null

if (-not ("FnvxrStereoFrameReader" -as [type])) {
    Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Collections.Generic;
using System.Threading;

public static class FnvxrStereoFrameReader {
    const UInt32 SharedStereoMagic = 0x53585646;
    const UInt32 SharedStereoVersion = 3;
    // Keep this synchronized with sizeof(SharedD3D9StereoFrameHeader).
    const int HeaderSize = 184;
    const int MaxWidth = 4096;
    const int MaxHeight = 2560;
    const int MappingBytes = HeaderSize + MaxWidth * MaxHeight * 4 * 2;

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

    const int FILE_MAP_READ = 0x0004;

    static int ReadInt32(IntPtr ptr, int offset) {
        return Marshal.ReadInt32(ptr, offset);
    }

    static long ReadInt64(IntPtr ptr, int offset) {
        return Marshal.ReadInt64(ptr, offset);
    }

    static void SavePlane(byte[] pixels, int width, int height, string path) {
        int rowBytes = width * 4;
        using (var bitmap = new Bitmap(width, height, PixelFormat.Format32bppArgb)) {
            var rect = new Rectangle(0, 0, width, height);
            var data = bitmap.LockBits(rect, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            try {
                for (int y = 0; y < height; ++y)
                    Marshal.Copy(pixels, y * rowBytes, IntPtr.Add(data.Scan0, y * data.Stride), rowBytes);
            } finally {
                bitmap.UnlockBits(data);
            }
            bitmap.Save(path, ImageFormat.Png);
        }
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

    static string Hex(byte[] bytes) {
        char[] chars = new char[bytes.Length * 2];
        const string hex = "0123456789abcdef";
        for (int i = 0; i < bytes.Length; ++i) {
            chars[i * 2] = hex[bytes[i] >> 4];
            chars[i * 2 + 1] = hex[bytes[i] & 15];
        }
        return new string(chars);
    }

    sealed class PixelMetrics {
        public int Samples;
        public int NonBlack;
        public int MeaningfulDifferent;
        public double LeftActiveFraction;
        public double RightActiveFraction;
    }

    static PixelMetrics Analyze(byte[] left, byte[] right) {
        var result = new PixelMetrics();
        var leftBuckets = new Dictionary<int,int>();
        var rightBuckets = new Dictionary<int,int>();
        int leftDominant = 0;
        int rightDominant = 0;
        int pixelCount = Math.Min(left.Length, right.Length) / 4;
        for (int pixel = 0; pixel < pixelCount; pixel += 16) {
            int offset = pixel * 4;
            int lb = left[offset], lg = left[offset + 1], lr = left[offset + 2];
            int rb = right[offset], rg = right[offset + 1], rr = right[offset + 2];
            result.Samples++;
            if (Math.Max(Math.Max(lr, lg), lb) > 8 || Math.Max(Math.Max(rr, rg), rb) > 8)
                result.NonBlack++;
            if (Math.Max(Math.Max(Math.Abs(lr - rr), Math.Abs(lg - rg)), Math.Abs(lb - rb)) >= 4)
                result.MeaningfulDifferent++;
            int leftBucket = ((lr >> 4) << 8) | ((lg >> 4) << 4) | (lb >> 4);
            int rightBucket = ((rr >> 4) << 8) | ((rg >> 4) << 4) | (rb >> 4);
            int leftCount = leftBuckets.ContainsKey(leftBucket) ? leftBuckets[leftBucket] + 1 : 1;
            int rightCount = rightBuckets.ContainsKey(rightBucket) ? rightBuckets[rightBucket] + 1 : 1;
            leftBuckets[leftBucket] = leftCount;
            rightBuckets[rightBucket] = rightCount;
            leftDominant = Math.Max(leftDominant, leftCount);
            rightDominant = Math.Max(rightDominant, rightCount);
        }
        if (result.Samples > 0) {
            result.LeftActiveFraction = 1.0 - (double)leftDominant / result.Samples;
            result.RightActiveFraction = 1.0 - (double)rightDominant / result.Samples;
        }
        return result;
    }

    static void WriteRawCache(string path, int width, int height, int format, int separated, int worldCandidate, int sequence, byte[] leftPixels, byte[] rightPixels, byte[] leftHash, byte[] rightHash) {
        const uint HeaderBytes = 160;
        ulong leftOffset = HeaderBytes;
        ulong leftBytes = (ulong)leftPixels.LongLength;
        ulong rightOffset = leftOffset + leftBytes;
        ulong rightBytes = (ulong)rightPixels.LongLength;
        uint flags = 0;
        if (separated != 0) flags |= 1u;
        if (worldCandidate != 0) flags |= 2u;
        flags |= 4u;

        string tempPath = path + ".tmp";
        using (var stream = new FileStream(tempPath, FileMode.Create, FileAccess.Write, FileShare.None))
        using (var writer = new BinaryWriter(stream)) {
            writer.Write(new byte[] { (byte)'F', (byte)'N', (byte)'V', (byte)'X', (byte)'S', (byte)'C', (byte)'N', 0 });
            writer.Write((UInt32)1);
            writer.Write(HeaderBytes);
            writer.Write(flags);
            writer.Write((UInt32)width);
            writer.Write((UInt32)height);
            writer.Write((UInt32)format);
            writer.Write((UInt32)(width * 4));
            writer.Write((UInt32)2);
            writer.Write((UInt32)0);
            writer.Write((UInt32)0);
            writer.Write((UInt64)(UInt32)sequence);
            writer.Write((UInt64)leftOffset);
            writer.Write((UInt64)leftBytes);
            writer.Write((UInt64)rightOffset);
            writer.Write((UInt64)rightBytes);
            writer.Write(leftHash);
            writer.Write(rightHash);
            while (stream.Position < HeaderBytes)
                writer.Write((byte)0);
            writer.Write(leftPixels);
            writer.Write(rightPixels);
        }
        if (File.Exists(path))
            File.Delete(path);
        File.Move(tempPath, path);
    }

    public static string Capture(string outputDir, string sceneName, int timeoutSeconds, bool allowMono, bool allowNonWorldCandidate) {
        var deadline = DateTime.UtcNow.AddSeconds(timeoutSeconds);
        IntPtr mapping = IntPtr.Zero;
        IntPtr view = IntPtr.Zero;
        int lastSequence = 0;
        int firstQualifiedSequence = 0;
        int firstQualifiedPair = 0;
        try {
            while (DateTime.UtcNow < deadline) {
                if (mapping == IntPtr.Zero) {
                    mapping = OpenFileMapping(FILE_MAP_READ, false, "Local\\FNVXR_D3D9_StereoFrame_v3");
                    if (mapping == IntPtr.Zero) {
                        Thread.Sleep(250);
                        continue;
                    }
                    view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, (UIntPtr)MappingBytes);
                    if (view == IntPtr.Zero) {
                        CloseHandle(mapping);
                        mapping = IntPtr.Zero;
                        Thread.Sleep(250);
                        continue;
                    }
                }

                UInt32 magic = unchecked((UInt32)ReadInt32(view, 0));
                UInt32 version = unchecked((UInt32)ReadInt32(view, 4));
                int headerBytes = ReadInt32(view, 8);
                int writing = ReadInt32(view, 12);
                int sequence = ReadInt32(view, 16);
                int width = ReadInt32(view, 20);
                int height = ReadInt32(view, 24);
                int pitchBytes = ReadInt32(view, 28);
                int format = ReadInt32(view, 32);
                int separated = ReadInt32(view, 36);
                int worldCandidate = ReadInt32(view, 40);
                int uiActive = ReadInt32(view, 44);
                int poseValid = ReadInt32(view, 48);
                int poseSequence = ReadInt32(view, 52);
                long renderedDisplayTime = ReadInt64(view, 56);
                int producerMode = ReadInt32(view, 152);
                int renderPairSequence = ReadInt32(view, 156);
                int leftPayloadOffset = ReadInt32(view, 160);
                int rightPayloadOffset = ReadInt32(view, 164);
                int totalMappingBytes = ReadInt32(view, 168);
                int referenceSpaceGeneration = ReadInt32(view, 172);
                int producerEpoch = ReadInt32(view, 176);

                bool sane = magic == SharedStereoMagic
                    && version == SharedStereoVersion
                    && headerBytes == HeaderSize
                    && writing == 0
                    && sequence != 0
                    && sequence != lastSequence
                    && width > 0 && width <= MaxWidth
                    && height > 0 && height <= MaxHeight
                    && pitchBytes == width * 4
                    && (format == 21 || format == 22)
                    && leftPayloadOffset == HeaderSize
                    && rightPayloadOffset == HeaderSize + pitchBytes * height
                    && totalMappingBytes == MappingBytes
                    && rightPayloadOffset + pitchBytes * height <= totalMappingBytes
                    && poseValid != 0
                    && poseSequence > 0
                    && renderedDisplayTime > 0
                    && producerMode == 3
                    && renderPairSequence > 0
                    && referenceSpaceGeneration > 0
                    && producerEpoch > 0;
                if (!sane) {
                    Thread.Sleep(100);
                    continue;
                }
                if (!allowMono && separated == 0) {
                    Thread.Sleep(100);
                    continue;
                }
                if (!allowNonWorldCandidate && worldCandidate == 0) {
                    Thread.Sleep(100);
                    continue;
                }
                if (uiActive != 0) {
                    Thread.Sleep(100);
                    continue;
                }

                Directory.CreateDirectory(outputDir);
                string completePath = Path.Combine(outputDir, ".complete");
                if (File.Exists(completePath))
                    File.Delete(completePath);
                string leftPath = Path.Combine(outputDir, sceneName + "-left.png");
                string rightPath = Path.Combine(outputDir, sceneName + "-right.png");
                string artifactPath = Path.Combine(outputDir, "scene.fnvxscn");
                IntPtr left = IntPtr.Add(view, leftPayloadOffset);
                IntPtr right = IntPtr.Add(view, rightPayloadOffset);
                byte[] leftPixels = CopyPlane(left, width, height, pitchBytes);
                byte[] rightPixels = CopyPlane(right, width, height, pitchBytes);
                Thread.MemoryBarrier();
                int writingAfterCopy = ReadInt32(view, 12);
                int sequenceAfterCopy = ReadInt32(view, 16);
                if (writingAfterCopy != 0 || sequenceAfterCopy != sequence) {
                    Thread.Sleep(25);
                    continue;
                }
                lastSequence = sequence;
                byte[] leftHash;
                byte[] rightHash;
                using (SHA256 sha = SHA256.Create()) {
                    leftHash = sha.ComputeHash(leftPixels);
                    rightHash = sha.ComputeHash(rightPixels);
                }
                PixelMetrics metrics = Analyze(leftPixels, rightPixels);
                int minimumNonBlack = Math.Max(64, metrics.Samples / 1000);
                int minimumMeaningfulDifferent = Math.Max(64, metrics.Samples / 1000);
                bool pixelProof = metrics.NonBlack >= minimumNonBlack
                    && metrics.MeaningfulDifferent >= minimumMeaningfulDifferent
                    && metrics.LeftActiveFraction >= 0.50
                    && metrics.RightActiveFraction >= 0.50;
                if (!pixelProof) {
                    Thread.Sleep(25);
                    continue;
                }
                if (firstQualifiedSequence == 0) {
                    firstQualifiedSequence = sequence;
                    firstQualifiedPair = renderPairSequence;
                    Thread.Sleep(25);
                    continue;
                }
                if (sequence <= firstQualifiedSequence || renderPairSequence <= firstQualifiedPair) {
                    Thread.Sleep(25);
                    continue;
                }
                WriteRawCache(artifactPath, width, height, format, separated, worldCandidate, sequence, leftPixels, rightPixels, leftHash, rightHash);
                SavePlane(leftPixels, width, height, leftPath);
                SavePlane(rightPixels, width, height, rightPath);

                string manifest =
                    "{\n" +
                    "  \"version\": 1,\n" +
                    "  \"artifact\": \"scene.fnvxscn\",\n" +
                    "  \"sceneName\": \"" + sceneName.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\",\n" +
                    "  \"firstQualifiedSequence\": " + firstQualifiedSequence + ",\n" +
                    "  \"sequence\": " + sequence + ",\n" +
                    "  \"firstQualifiedRenderPairSequence\": " + firstQualifiedPair + ",\n" +
                    "  \"renderPairSequence\": " + renderPairSequence + ",\n" +
                    "  \"poseSequence\": " + poseSequence + ",\n" +
                    "  \"renderedDisplayTime\": " + renderedDisplayTime + ",\n" +
                    "  \"referenceSpaceGeneration\": " + referenceSpaceGeneration + ",\n" +
                    "  \"producerEpoch\": " + producerEpoch + ",\n" +
                    "  \"producerMode\": " + producerMode + ",\n" +
                    "  \"width\": " + width + ",\n" +
                    "  \"height\": " + height + ",\n" +
                    "  \"format\": \"BGRA8_UNORM_SRGB\",\n" +
                    "  \"sourceFormat\": " + format + ",\n" +
                    "  \"rowPitch\": " + (width * 4) + ",\n" +
                    "  \"eyeCount\": 2,\n" +
                    "  \"separated\": " + (separated != 0 ? "true" : "false") + ",\n" +
                    "  \"worldCandidate\": " + (worldCandidate != 0 ? "true" : "false") + ",\n" +
                    "  \"uiActive\": false,\n" +
                    "  \"leftSha256\": \"" + Hex(leftHash) + "\",\n" +
                    "  \"rightSha256\": \"" + Hex(rightHash) + "\",\n" +
                    "  \"independentPixelMetrics\": {\"samples\": " + metrics.Samples +
                    ", \"nonBlack\": " + metrics.NonBlack +
                    ", \"meaningfulDifferent\": " + metrics.MeaningfulDifferent +
                    ", \"leftActiveFraction\": " + metrics.LeftActiveFraction.ToString("R", System.Globalization.CultureInfo.InvariantCulture) +
                    ", \"rightActiveFraction\": " + metrics.RightActiveFraction.ToString("R", System.Globalization.CultureInfo.InvariantCulture) + "},\n" +
                    "  \"previewLeft\": \"" + Path.GetFileName(leftPath) + "\",\n" +
                    "  \"previewRight\": \"" + Path.GetFileName(rightPath) + "\"\n" +
                    "}\n";
                string manifestPath = Path.Combine(outputDir, "scene-cache.json");
                string manifestTempPath = manifestPath + ".tmp";
                File.WriteAllText(manifestTempPath, manifest);
                if (File.Exists(manifestPath))
                    File.Delete(manifestPath);
                File.Move(manifestTempPath, manifestPath);
                File.WriteAllText(completePath, DateTime.UtcNow.ToString("o") + "\n");
                return manifest;
            }
            throw new TimeoutException("Timed out waiting for a stereo scene frame that satisfied separated/worldCandidate requirements.");
        } finally {
            if (view != IntPtr.Zero) UnmapViewOfFile(view);
            if (mapping != IntPtr.Zero) CloseHandle(mapping);
        }
    }
}
'@
}

$manifest = [FnvxrStereoFrameReader]::Capture(
    $SceneDir,
    $SceneName,
    $TimeoutSeconds,
    [bool]$AllowMono,
    [bool]$AllowNonWorldCandidate)

$manifestPath = Join-Path $SceneDir "scene-cache.json"
Write-Host "scene cache captured: $manifestPath"
Write-Host $manifest
