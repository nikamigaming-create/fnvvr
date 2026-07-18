param(
    [string]$OutputRoot = "",
    [string]$SceneName = "scene-0",
    [int]$TimeoutSeconds = 45,
    [int]$ExpectedProducerProcessId = 0,
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
$staleCompleteMarker = Join-Path $SceneDir ".complete"
if (Test-Path -LiteralPath $staleCompleteMarker) {
    Remove-Item -LiteralPath $staleCompleteMarker -Force
}

if (-not ("FnvxrStereoFrameReaderV7" -as [type])) {
    $compilerParameters = New-Object System.CodeDom.Compiler.CompilerParameters
    $compilerParameters.CompilerOptions = "/unsafe"
    [void]$compilerParameters.ReferencedAssemblies.Add("System.Drawing.dll")
    Add-Type -CompilerParameters $compilerParameters -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Collections.Generic;
using System.Threading;

public static class FnvxrStereoFrameReaderV7 {
    const UInt32 SharedStereoMagic = 0x53585646;
    const UInt32 SharedStereoVersion = 7;
    // Keep this synchronized with sizeof(SharedD3D9StereoFrameHeader).
    const int HeaderSize = 216;
    const int MaxWidth = 4096;
    const int MaxHeight = 2560;
    const int SlotCount = 4;
    const int EyeCapacityBytes = MaxWidth * MaxHeight * 4;
    const int SlotBytes = EyeCapacityBytes * 2;
    const int MappingBytes = HeaderSize + SlotBytes * SlotCount;
    const int PublishedSlotOffset = 196;
    // Lane zero belongs to the OpenXR host; evidence capture owns lane one.
    const int ReaderSlotOffset = 204;

    [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Ansi)]
    static extern IntPtr OpenFileMapping(int desiredAccess, bool inheritHandle, string name);
    [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Ansi)]
    static extern IntPtr CreateMutex(IntPtr attributes, bool initialOwner, string name);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern UInt32 WaitForSingleObject(IntPtr handle, UInt32 milliseconds);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern bool ReleaseMutex(IntPtr handle);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern IntPtr MapViewOfFile(IntPtr mapping, int desiredAccess, uint offsetHigh, uint offsetLow, UIntPtr bytesToMap);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern bool UnmapViewOfFile(IntPtr address);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern bool CloseHandle(IntPtr handle);
    [DllImport("kernel32.dll", EntryPoint="RtlMoveMemory", SetLastError=false)]
    static extern void CopyMemory(IntPtr destination, IntPtr source, UIntPtr length);
    static unsafe int NativeInterlockedCompareExchange(IntPtr destination, int exchange, int comparand) {
        return Interlocked.CompareExchange(ref *((int*)destination.ToPointer()), exchange, comparand);
    }
    static unsafe int NativeInterlockedExchange(IntPtr destination, int value) {
        return Interlocked.Exchange(ref *((int*)destination.ToPointer()), value);
    }
    static unsafe long NativeInterlockedReadInt64(IntPtr source) {
        return Interlocked.Read(ref *((long*)source.ToPointer()));
    }

    const int FILE_MAP_READ = 0x0004;
    const int FILE_MAP_WRITE = 0x0002;
    const UInt32 WAIT_OBJECT_0 = 0;
    const UInt32 WAIT_ABANDONED = 0x80;

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

    static string FileSha256Hex(string path) {
        using (var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read))
        using (SHA256 sha = SHA256.Create())
            return Hex(sha.ComputeHash(stream));
    }

    sealed class PixelMetrics {
        public int Samples;
        public int NonBlack;
        public int MeaningfulDifferent;
        public double LeftActiveFraction;
        public double RightActiveFraction;
        public int LeftActiveTiles;
        public int RightActiveTiles;
        public int DifferentTiles;
    }

    static int BitCount(int value) {
        int count = 0;
        value &= 0xffff;
        while (value != 0) { value &= value - 1; count++; }
        return count;
    }

    static PixelMetrics Analyze(byte[] left, byte[] right, int width, int height) {
        var result = new PixelMetrics();
        var leftBuckets = new Dictionary<int,int>();
        var rightBuckets = new Dictionary<int,int>();
        int leftDominant = 0;
        int rightDominant = 0;
        int leftDominantBucket = 0;
        int rightDominantBucket = 0;
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
            if (leftCount > leftDominant) { leftDominant = leftCount; leftDominantBucket = leftBucket; }
            if (rightCount > rightDominant) { rightDominant = rightCount; rightDominantBucket = rightBucket; }
        }
        if (result.Samples > 0) {
            result.LeftActiveFraction = 1.0 - (double)leftDominant / result.Samples;
            result.RightActiveFraction = 1.0 - (double)rightDominant / result.Samples;
        }
        int leftTileMask = 0, rightTileMask = 0, differentTileMask = 0;
        for (int pixel = 0; pixel < pixelCount; pixel += 16) {
            int offset = pixel * 4;
            int lb = left[offset], lg = left[offset + 1], lr = left[offset + 2];
            int rb = right[offset], rg = right[offset + 1], rr = right[offset + 2];
            int x = pixel % width;
            int y = pixel / width;
            int tileX = Math.Min(3, x * 4 / width);
            int tileY = Math.Min(3, y * 4 / height);
            int tileBit = 1 << (tileY * 4 + tileX);
            int leftBucket = ((lr >> 4) << 8) | ((lg >> 4) << 4) | (lb >> 4);
            int rightBucket = ((rr >> 4) << 8) | ((rg >> 4) << 4) | (rb >> 4);
            if (leftBucket != leftDominantBucket) leftTileMask |= tileBit;
            if (rightBucket != rightDominantBucket) rightTileMask |= tileBit;
            if (Math.Max(Math.Max(Math.Abs(lr - rr), Math.Abs(lg - rg)), Math.Abs(lb - rb)) >= 4)
                differentTileMask |= tileBit;
        }
        result.LeftActiveTiles = BitCount(leftTileMask);
        result.RightActiveTiles = BitCount(rightTileMask);
        result.DifferentTiles = BitCount(differentTileMask);
        return result;
    }

    static void WriteRawCache(string path, int width, int height, int format, int separated, int worldCandidate, UInt32 sequence, byte[] leftPixels, byte[] rightPixels, byte[] leftHash, byte[] rightHash) {
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
            writer.Write((UInt64)sequence);
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

    static bool Advanced(UInt32 newer, UInt32 older) {
        UInt32 delta = unchecked(newer - older);
        return newer != 0 && delta != 0 && delta < 0x80000000u;
    }

    public static string Capture(string outputDir, string sceneName, int timeoutSeconds, int expectedProducerProcessId, bool allowMono, bool allowNonWorldCandidate) {
        var deadline = DateTime.UtcNow.AddSeconds(timeoutSeconds);
        IntPtr mapping = IntPtr.Zero;
        IntPtr view = IntPtr.Zero;
        IntPtr readerLease = IntPtr.Zero;
        bool ownsReaderLease = false;
        bool resetReaderClaim = false;
        UInt32 lastSequence = 0;
        UInt32 firstQualifiedSequence = 0;
        UInt32 firstQualifiedPair = 0;
        UInt32 firstQualifiedPoseSequence = 0;
        long firstRenderedDisplayTime = 0;
        UInt64 firstProducerEpoch = 0;
        UInt64 firstRendererProducerEpoch = 0;
        UInt64 firstPublicationGeneration = 0;
        int firstReferenceSpaceGeneration = 0;
        int firstProducerProcessId = 0;
        int firstWidth = 0;
        int firstHeight = 0;
        int firstFormat = 0;
        int firstSeparated = 0;
        int firstWorldCandidate = 0;
        byte[] firstLeftHash = null;
        byte[] firstRightHash = null;
        PixelMetrics firstMetrics = null;
        string firstArtifactSha256 = null;
        try {
            readerLease = CreateMutex(IntPtr.Zero, false, "Local\\FNVXR_D3D9_Stereo_CaptureReader_v7");
            if (readerLease == IntPtr.Zero)
                throw new InvalidOperationException("Could not create the stereo capture reader lease.");
            UInt32 waitResult = WaitForSingleObject(readerLease, 0);
            if (waitResult != WAIT_OBJECT_0 && waitResult != WAIT_ABANDONED)
                throw new InvalidOperationException("Another live stereo evidence capture owns the capture reader lane.");
            ownsReaderLease = true;
            resetReaderClaim = true;
            while (DateTime.UtcNow < deadline) {
                if (mapping == IntPtr.Zero) {
                    mapping = OpenFileMapping(FILE_MAP_READ | FILE_MAP_WRITE, false, "Local\\FNVXR_D3D9_StereoFrame_v7");
                    if (mapping == IntPtr.Zero) {
                        Thread.Sleep(250);
                        continue;
                    }
                    view = MapViewOfFile(mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, (UIntPtr)MappingBytes);
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
                UInt32 sequence = unchecked((UInt32)ReadInt32(view, 16));
                int width = ReadInt32(view, 20);
                int height = ReadInt32(view, 24);
                int pitchBytes = ReadInt32(view, 28);
                int format = ReadInt32(view, 32);
                int separated = ReadInt32(view, 36);
                int worldCandidate = ReadInt32(view, 40);
                int uiActive = ReadInt32(view, 44);
                int poseValid = ReadInt32(view, 48);
                UInt32 poseSequence = unchecked((UInt32)ReadInt32(view, 52));
                long renderedDisplayTime = ReadInt64(view, 56);
                int producerMode = ReadInt32(view, 152);
                UInt32 renderPairSequence = unchecked((UInt32)ReadInt32(view, 156));
                int leftPayloadOffset = ReadInt32(view, 160);
                int rightPayloadOffset = ReadInt32(view, 164);
                int totalMappingBytes = ReadInt32(view, 168);
                int referenceSpaceGeneration = ReadInt32(view, 172);
                UInt64 producerEpoch = unchecked((UInt64)ReadInt64(view, 176));
                UInt64 rendererProducerEpoch = unchecked((UInt64)ReadInt64(view, 184));
                int producerProcessId = ReadInt32(view, 192);
                int publishedSlot = ReadInt32(view, PublishedSlotOffset);
                UInt64 publicationGeneration = unchecked((UInt64)NativeInterlockedReadInt64(IntPtr.Add(view, 208)));
                int expectedLeftPayloadOffset = publishedSlot >= 0 && publishedSlot < SlotCount
                    ? HeaderSize + publishedSlot * SlotBytes
                    : -1;
                int payloadBytes = pitchBytes > 0 && height > 0
                    ? pitchBytes * height
                    : -1;

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
                    && publishedSlot >= 0 && publishedSlot < SlotCount
                    && leftPayloadOffset == expectedLeftPayloadOffset
                    && rightPayloadOffset == leftPayloadOffset + payloadBytes
                    && totalMappingBytes == MappingBytes
                    && payloadBytes > 0 && payloadBytes <= EyeCapacityBytes
                    && rightPayloadOffset + payloadBytes <= leftPayloadOffset + SlotBytes
                    && poseValid != 0
                    && poseSequence != 0
                    && (poseSequence & 1u) == 0u
                    && renderedDisplayTime > 0
                    && producerMode == 3
                    && renderPairSequence != 0
                    && referenceSpaceGeneration > 0
                    && producerEpoch > 0
                    && rendererProducerEpoch > 0
                    && publicationGeneration > 0
                    && producerProcessId > 0
                    && (expectedProducerProcessId <= 0 || producerProcessId == expectedProducerProcessId);
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

                IntPtr readerSlotAddress = IntPtr.Add(view, ReaderSlotOffset);
                if (resetReaderClaim) {
                    // The role mutex proves that a previous capture holding
                    // this fixed lane terminated or released it. A live or
                    // suspended reader cannot have its protection revoked.
                    NativeInterlockedExchange(readerSlotAddress, -1);
                    resetReaderClaim = false;
                }
                if (NativeInterlockedCompareExchange(readerSlotAddress, publishedSlot, -1) != -1) {
                    Thread.Sleep(25);
                    continue;
                }
                byte[] leftPixels;
                byte[] rightPixels;
                bool stableCopy = false;
                try {
                    Thread.MemoryBarrier();
                    if (ReadInt32(view, 12) != 0
                        || unchecked((UInt32)ReadInt32(view, 16)) != sequence
                        || ReadInt32(view, PublishedSlotOffset) != publishedSlot
                        || unchecked((UInt64)NativeInterlockedReadInt64(IntPtr.Add(view, 208))) != publicationGeneration) {
                        Thread.Sleep(1);
                        continue;
                    }
                    IntPtr left = IntPtr.Add(view, leftPayloadOffset);
                    IntPtr right = IntPtr.Add(view, rightPayloadOffset);
                    leftPixels = CopyPlane(left, width, height, pitchBytes);
                    rightPixels = CopyPlane(right, width, height, pitchBytes);
                    Thread.MemoryBarrier();
                    // The producer excludes this lane's claimed slot. Header
                    // publication may advance during a large copy without
                    // invalidating the immutable slot snapshot.
                    stableCopy = true;
                } finally {
                    NativeInterlockedCompareExchange(readerSlotAddress, -1, publishedSlot);
                }
                if (!stableCopy) {
                    Thread.Sleep(25);
                    continue;
                }
                lastSequence = sequence;
                Directory.CreateDirectory(outputDir);
                string completePath = Path.Combine(outputDir, ".complete");
                if (File.Exists(completePath))
                    File.Delete(completePath);
                string leftPath = Path.Combine(outputDir, sceneName + "-left.png");
                string rightPath = Path.Combine(outputDir, sceneName + "-right.png");
                string artifactPath = Path.Combine(outputDir, "scene.fnvxscn");
                string firstLeftPath = Path.Combine(outputDir, sceneName + "-first-left.png");
                string firstRightPath = Path.Combine(outputDir, sceneName + "-first-right.png");
                string firstArtifactPath = Path.Combine(outputDir, "scene-first.fnvxscn");
                byte[] leftHash;
                byte[] rightHash;
                using (SHA256 sha = SHA256.Create()) {
                    leftHash = sha.ComputeHash(leftPixels);
                    rightHash = sha.ComputeHash(rightPixels);
                }
                PixelMetrics metrics = Analyze(leftPixels, rightPixels, width, height);
                int minimumNonBlack = Math.Max(64, metrics.Samples / 1000);
                int minimumMeaningfulDifferent = Math.Max(64, metrics.Samples / 1000);
                bool pixelProof = metrics.NonBlack >= minimumNonBlack
                    && metrics.MeaningfulDifferent >= minimumMeaningfulDifferent
                    && metrics.LeftActiveFraction >= 0.50
                    && metrics.RightActiveFraction >= 0.50
                    && metrics.LeftActiveTiles >= 12
                    && metrics.RightActiveTiles >= 12
                    && metrics.DifferentTiles >= 8;
                if (!pixelProof) {
                    Thread.Sleep(25);
                    continue;
                }
                bool sameProofStream = firstQualifiedSequence != 0
                    && firstProducerEpoch == producerEpoch
                    && firstRendererProducerEpoch == rendererProducerEpoch
                    && firstReferenceSpaceGeneration == referenceSpaceGeneration
                    && firstProducerProcessId == producerProcessId
                    && firstWidth == width
                    && firstHeight == height
                    && firstFormat == format
                    && firstSeparated == separated
                    && firstWorldCandidate == worldCandidate;
                bool advancesFirst = sameProofStream
                    && Advanced(sequence, firstQualifiedSequence)
                    && Advanced(renderPairSequence, firstQualifiedPair)
                    && Advanced(poseSequence, firstQualifiedPoseSequence)
                    && publicationGeneration > firstPublicationGeneration
                    && renderedDisplayTime > firstRenderedDisplayTime;
                if (!advancesFirst) {
                    firstQualifiedSequence = sequence;
                    firstQualifiedPair = renderPairSequence;
                    firstQualifiedPoseSequence = poseSequence;
                    firstRenderedDisplayTime = renderedDisplayTime;
                    firstProducerEpoch = producerEpoch;
                    firstRendererProducerEpoch = rendererProducerEpoch;
                    firstPublicationGeneration = publicationGeneration;
                    firstReferenceSpaceGeneration = referenceSpaceGeneration;
                    firstProducerProcessId = producerProcessId;
                    firstWidth = width;
                    firstHeight = height;
                    firstFormat = format;
                    firstSeparated = separated;
                    firstWorldCandidate = worldCandidate;
                    firstLeftHash = leftHash;
                    firstRightHash = rightHash;
                    firstMetrics = metrics;
                    WriteRawCache(firstArtifactPath, width, height, format, separated, worldCandidate, sequence, leftPixels, rightPixels, leftHash, rightHash);
                    SavePlane(leftPixels, width, height, firstLeftPath);
                    SavePlane(rightPixels, width, height, firstRightPath);
                    firstArtifactSha256 = FileSha256Hex(firstArtifactPath);
                    Thread.Sleep(25);
                    continue;
                }
                WriteRawCache(artifactPath, width, height, format, separated, worldCandidate, sequence, leftPixels, rightPixels, leftHash, rightHash);
                SavePlane(leftPixels, width, height, leftPath);
                SavePlane(rightPixels, width, height, rightPath);
                string artifactSha256 = FileSha256Hex(artifactPath);

                string manifest =
                    "{\n" +
                    "  \"version\": 1,\n" +
                    "  \"firstArtifact\": \"scene-first.fnvxscn\",\n" +
                    "  \"artifact\": \"scene.fnvxscn\",\n" +
                    "  \"sceneName\": \"" + sceneName.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\",\n" +
                    "  \"firstQualifiedSequence\": " + firstQualifiedSequence + ",\n" +
                    "  \"sequence\": " + sequence + ",\n" +
                    "  \"firstQualifiedRenderPairSequence\": " + firstQualifiedPair + ",\n" +
                    "  \"renderPairSequence\": " + renderPairSequence + ",\n" +
                    "  \"firstQualifiedPoseSequence\": " + firstQualifiedPoseSequence + ",\n" +
                    "  \"poseSequence\": " + poseSequence + ",\n" +
                    "  \"firstRenderedDisplayTime\": " + firstRenderedDisplayTime + ",\n" +
                    "  \"renderedDisplayTime\": " + renderedDisplayTime + ",\n" +
                    "  \"firstReferenceSpaceGeneration\": " + firstReferenceSpaceGeneration + ",\n" +
                    "  \"referenceSpaceGeneration\": " + referenceSpaceGeneration + ",\n" +
                    "  \"firstProducerEpoch\": \"" + firstProducerEpoch + "\",\n" +
                    "  \"producerEpoch\": \"" + producerEpoch + "\",\n" +
                    "  \"firstRendererProducerEpoch\": \"" + firstRendererProducerEpoch + "\",\n" +
                    "  \"rendererProducerEpoch\": \"" + rendererProducerEpoch + "\",\n" +
                    "  \"firstPublicationGeneration\": \"" + firstPublicationGeneration + "\",\n" +
                    "  \"publicationGeneration\": \"" + publicationGeneration + "\",\n" +
                    "  \"firstProducerProcessId\": " + firstProducerProcessId + ",\n" +
                    "  \"producerProcessId\": " + producerProcessId + ",\n" +
                    "  \"producerMode\": " + producerMode + ",\n" +
                    "  \"firstWidth\": " + firstWidth + ",\n" +
                    "  \"width\": " + width + ",\n" +
                    "  \"firstHeight\": " + firstHeight + ",\n" +
                    "  \"height\": " + height + ",\n" +
                    "  \"format\": \"BGRA8_UNORM_SRGB\",\n" +
                    "  \"sourceFormat\": " + format + ",\n" +
                    "  \"firstSourceFormat\": " + firstFormat + ",\n" +
                    "  \"rowPitch\": " + (width * 4) + ",\n" +
                    "  \"eyeCount\": 2,\n" +
                    "  \"separated\": " + (separated != 0 ? "true" : "false") + ",\n" +
                    "  \"firstSeparated\": " + (firstSeparated != 0 ? "true" : "false") + ",\n" +
                    "  \"worldCandidate\": " + (worldCandidate != 0 ? "true" : "false") + ",\n" +
                    "  \"firstWorldCandidate\": " + (firstWorldCandidate != 0 ? "true" : "false") + ",\n" +
                    "  \"uiActive\": false,\n" +
                    "  \"leftSha256\": \"" + Hex(leftHash) + "\",\n" +
                    "  \"rightSha256\": \"" + Hex(rightHash) + "\",\n" +
                    "  \"artifactSha256\": \"" + artifactSha256 + "\",\n" +
                    "  \"firstLeftSha256\": \"" + Hex(firstLeftHash) + "\",\n" +
                    "  \"firstRightSha256\": \"" + Hex(firstRightHash) + "\",\n" +
                    "  \"firstArtifactSha256\": \"" + firstArtifactSha256 + "\",\n" +
                    "  \"firstIndependentPixelMetrics\": {\"samples\": " + firstMetrics.Samples +
                    ", \"nonBlack\": " + firstMetrics.NonBlack +
                    ", \"meaningfulDifferent\": " + firstMetrics.MeaningfulDifferent +
                    ", \"leftActiveFraction\": " + firstMetrics.LeftActiveFraction.ToString("R", System.Globalization.CultureInfo.InvariantCulture) +
                    ", \"rightActiveFraction\": " + firstMetrics.RightActiveFraction.ToString("R", System.Globalization.CultureInfo.InvariantCulture) +
                    ", \"leftActiveTiles\": " + firstMetrics.LeftActiveTiles +
                    ", \"rightActiveTiles\": " + firstMetrics.RightActiveTiles +
                    ", \"differentTiles\": " + firstMetrics.DifferentTiles + "},\n" +
                    "  \"independentPixelMetrics\": {\"samples\": " + metrics.Samples +
                    ", \"nonBlack\": " + metrics.NonBlack +
                    ", \"meaningfulDifferent\": " + metrics.MeaningfulDifferent +
                    ", \"leftActiveFraction\": " + metrics.LeftActiveFraction.ToString("R", System.Globalization.CultureInfo.InvariantCulture) +
                    ", \"rightActiveFraction\": " + metrics.RightActiveFraction.ToString("R", System.Globalization.CultureInfo.InvariantCulture) +
                    ", \"leftActiveTiles\": " + metrics.LeftActiveTiles +
                    ", \"rightActiveTiles\": " + metrics.RightActiveTiles +
                    ", \"differentTiles\": " + metrics.DifferentTiles + "},\n" +
                    "  \"firstPreviewLeft\": \"" + Path.GetFileName(firstLeftPath) + "\",\n" +
                    "  \"firstPreviewRight\": \"" + Path.GetFileName(firstRightPath) + "\",\n" +
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
            if (view != IntPtr.Zero && ownsReaderLease)
                NativeInterlockedExchange(IntPtr.Add(view, ReaderSlotOffset), -1);
            if (view != IntPtr.Zero) UnmapViewOfFile(view);
            if (mapping != IntPtr.Zero) CloseHandle(mapping);
            if (readerLease != IntPtr.Zero) {
                if (ownsReaderLease) ReleaseMutex(readerLease);
                CloseHandle(readerLease);
            }
        }
    }
}
'@
}

$manifest = [FnvxrStereoFrameReaderV7]::Capture(
    $SceneDir,
    $SceneName,
    $TimeoutSeconds,
    $ExpectedProducerProcessId,
    [bool]$AllowMono,
    [bool]$AllowNonWorldCandidate)

$manifestPath = Join-Path $SceneDir "scene-cache.json"
Write-Host "scene cache captured: $manifestPath"
Write-Host $manifest
