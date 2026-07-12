param(
    [Parameter(Mandatory=$true)]
    [string]$LeftPpm,
    [Parameter(Mandatory=$true)]
    [string]$RightPpm,
    [string]$SceneName = "openmw-harvest-0",
    [string]$OutputRoot = "",
    [int]$OutputWidth = 2048,
    [int]$OutputHeight = 1280
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
if (-not $OutputRoot) {
    $OutputRoot = Join-Path $Root "local\scene-cache"
}
$SceneDir = Join-Path $OutputRoot $SceneName
New-Item -ItemType Directory -Force -Path $SceneDir | Out-Null

if (-not ("FnvxrPpmSceneImporter" -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Security.Cryptography;

public static class FnvxrPpmSceneImporter {
    struct PpmImage {
        public int Width;
        public int Height;
        public byte[] Rgb;
    }

    static string ReadToken(byte[] bytes, ref int offset) {
        while (offset < bytes.Length) {
            byte c = bytes[offset];
            if (c == '#') {
                while (offset < bytes.Length && bytes[offset] != '\n') offset++;
                continue;
            }
            if (!char.IsWhiteSpace((char)c)) break;
            offset++;
        }
        int start = offset;
        while (offset < bytes.Length && !char.IsWhiteSpace((char)bytes[offset])) offset++;
        return System.Text.Encoding.ASCII.GetString(bytes, start, offset - start);
    }

    static PpmImage ReadPpm(string path) {
        byte[] bytes = File.ReadAllBytes(path);
        int offset = 0;
        if (ReadToken(bytes, ref offset) != "P6")
            throw new InvalidDataException("Only binary P6 PPM is supported: " + path);
        int width = int.Parse(ReadToken(bytes, ref offset));
        int height = int.Parse(ReadToken(bytes, ref offset));
        int max = int.Parse(ReadToken(bytes, ref offset));
        if (max != 255)
            throw new InvalidDataException("Only maxval 255 PPM is supported: " + path);
        while (offset < bytes.Length && char.IsWhiteSpace((char)bytes[offset])) offset++;
        int required = width * height * 3;
        if (bytes.Length - offset < required)
            throw new InvalidDataException("PPM pixel payload is short: " + path);
        byte[] rgb = new byte[required];
        Buffer.BlockCopy(bytes, offset, rgb, 0, required);
        return new PpmImage { Width = width, Height = height, Rgb = rgb };
    }

    static byte[] ResampleToBgra(PpmImage image, int outputWidth, int outputHeight) {
        byte[] bgra = new byte[outputWidth * outputHeight * 4];
        for (int y = 0; y < outputHeight; ++y) {
            int srcY = y * image.Height / outputHeight;
            for (int x = 0; x < outputWidth; ++x) {
                int srcX = x * image.Width / outputWidth;
                int src = (srcY * image.Width + srcX) * 3;
                int dst = (y * outputWidth + x) * 4;
                bgra[dst + 0] = image.Rgb[src + 2];
                bgra[dst + 1] = image.Rgb[src + 1];
                bgra[dst + 2] = image.Rgb[src + 0];
                bgra[dst + 3] = 255;
            }
        }
        return bgra;
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

    static void WriteRawCache(string path, int width, int height, byte[] leftPixels, byte[] rightPixels, byte[] leftHash, byte[] rightHash) {
        const uint HeaderBytes = 160;
        ulong leftOffset = HeaderBytes;
        ulong leftBytes = (ulong)leftPixels.LongLength;
        ulong rightOffset = leftOffset + leftBytes;
        ulong rightBytes = (ulong)rightPixels.LongLength;
        uint flags = 1u | 2u | 4u;

        string tempPath = path + ".tmp";
        using (var stream = new FileStream(tempPath, FileMode.Create, FileAccess.Write, FileShare.None))
        using (var writer = new BinaryWriter(stream)) {
            writer.Write(new byte[] { (byte)'F', (byte)'N', (byte)'V', (byte)'X', (byte)'S', (byte)'C', (byte)'N', 0 });
            writer.Write((UInt32)1);
            writer.Write(HeaderBytes);
            writer.Write(flags);
            writer.Write((UInt32)width);
            writer.Write((UInt32)height);
            writer.Write((UInt32)22);
            writer.Write((UInt32)(width * 4));
            writer.Write((UInt32)2);
            writer.Write((UInt32)0);
            writer.Write((UInt32)0);
            writer.Write((UInt64)0);
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

    public static string Import(string leftPath, string rightPath, string outputDir, string sceneName, int outputWidth, int outputHeight) {
        Directory.CreateDirectory(outputDir);
        string completePath = Path.Combine(outputDir, ".complete");
        if (File.Exists(completePath))
            File.Delete(completePath);

        PpmImage leftImage = ReadPpm(leftPath);
        PpmImage rightImage = ReadPpm(rightPath);
        byte[] leftPixels = ResampleToBgra(leftImage, outputWidth, outputHeight);
        byte[] rightPixels = ResampleToBgra(rightImage, outputWidth, outputHeight);

        byte[] leftHash;
        byte[] rightHash;
        using (SHA256 sha = SHA256.Create()) {
            leftHash = sha.ComputeHash(leftPixels);
            rightHash = sha.ComputeHash(rightPixels);
        }

        string artifactPath = Path.Combine(outputDir, "scene.fnvxscn");
        WriteRawCache(artifactPath, outputWidth, outputHeight, leftPixels, rightPixels, leftHash, rightHash);
        string manifest =
            "{\n" +
            "  \"version\": 1,\n" +
            "  \"artifact\": \"scene.fnvxscn\",\n" +
            "  \"sceneName\": \"" + sceneName.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\",\n" +
            "  \"source\": \"openmw-ppm-stereo-import\",\n" +
            "  \"sourceLeft\": \"" + leftPath.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\",\n" +
            "  \"sourceRight\": \"" + rightPath.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\",\n" +
            "  \"width\": " + outputWidth + ",\n" +
            "  \"height\": " + outputHeight + ",\n" +
            "  \"format\": \"BGRA8_UNORM_SRGB\",\n" +
            "  \"rowPitch\": " + (outputWidth * 4) + ",\n" +
            "  \"eyeCount\": 2,\n" +
            "  \"separated\": true,\n" +
            "  \"worldCandidate\": true,\n" +
            "  \"uiActive\": false,\n" +
            "  \"leftSha256\": \"" + Hex(leftHash) + "\",\n" +
            "  \"rightSha256\": \"" + Hex(rightHash) + "\"\n" +
            "}\n";
        File.WriteAllText(Path.Combine(outputDir, "scene-cache.json"), manifest);
        File.WriteAllText(completePath, DateTime.UtcNow.ToString("o") + "\n");
        return manifest;
    }
}
'@
}

$manifest = [FnvxrPpmSceneImporter]::Import(
    (Resolve-Path -LiteralPath $LeftPpm).Path,
    (Resolve-Path -LiteralPath $RightPpm).Path,
    $SceneDir,
    $SceneName,
    $OutputWidth,
    $OutputHeight)

Write-Host "imported OpenMW stereo scene cache: $(Join-Path $SceneDir 'scene-cache.json')"
Write-Host $manifest
