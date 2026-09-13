#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include "../host/fnvxr_mirror_writer.h"
#include <array>
#include <cstdio>

using Microsoft::WRL::ComPtr;
int main()
{
    wchar_t root[MAX_PATH] {}, directory[MAX_PATH] {};
    if (!GetTempPathW(MAX_PATH, root)) return 1;
    swprintf_s(directory, L"%sfnvxr-mirror-test-%lu", root, GetCurrentProcessId());
    if (!CreateDirectoryW(directory, nullptr)) return 2;
    fnvxr::host::mirror::Pair pair;
    pair.frame = 47;
    pair.ordinal = 9;
    for (std::size_t eye = 0; eye != 2; ++eye)
    {
        auto& image = pair.eyes[eye];
        image.path = std::wstring(directory) + (eye ? L"\\right.png" : L"\\left.png");
        image.width = 256;
        image.height = 96;
        image.rowPitch = 1040; // Padded rows and multiple DEFLATE blocks.
        image.rgba = true;
        image.pixels.resize(image.rowPitch * image.height, 0xcd);
        for (std::uint32_t y = 0; y != image.height; ++y)
            for (std::uint32_t x = 0; x != image.width; ++x)
            {
                auto* pixel = image.pixels.data() + y*image.rowPitch + x*4;
                pixel[0] = static_cast<unsigned char>(eye ? 197 : 31);
                pixel[1] = static_cast<unsigned char>(x);
                pixel[2] = static_cast<unsigned char>(y);
                pixel[3] = 231; // Presentation tags in alpha must be lossless.
            }
    }
    const auto paths = std::array<std::wstring, 2> { pair.eyes[0].path, pair.eyes[1].path };
    auto compact = pair;
    compact.frame = 48; compact.ordinal = 10;
    for (auto& eye : compact.eyes) { eye.path += L".jpg"; eye.jpeg = true; }
    fnvxr::host::mirror::Writer writer;
    if (!writer.submit(std::move(pair))) return 3;
    if (!writer.submit(std::move(compact))) return 12;
    writer.finish();
    const auto results = writer.takeCompleted();
    if (results.size() != 2 || results[0].frame != 47 || results[0].ordinal != 9
        || results[0].status[0] < 0 || results[0].status[1] < 0) return 4;
    if (results[1].frame != 48 || results[1].status[0] < 0 || results[1].status[1] < 0) return 13;
    for (const auto& result : results)
        for (std::size_t eye = 0; eye != 2; ++eye)
            if (!result.fileTimes[eye] || !result.fileBytes[eye]) return 18;
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) return 5;
    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) return 6;
    for (std::size_t eye = 0; eye != 2; ++eye)
    {
        ComPtr<IWICBitmapDecoder> decoder;
        ComPtr<IWICBitmapFrameDecode> frame;
        ComPtr<IWICFormatConverter> converter;
        if (FAILED(factory->CreateDecoderFromFilename(paths[eye].c_str(), nullptr,
            GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder))
            || FAILED(decoder->GetFrame(0, &frame))
            || FAILED(factory->CreateFormatConverter(&converter))
            || FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) return 7;
        std::array<unsigned char, 256*96*4> decoded {};
        if (FAILED(converter->CopyPixels(nullptr, 256*4, static_cast<UINT>(decoded.size()), decoded.data()))) return 8;
        for (unsigned y = 0; y != 96; ++y)
            for (unsigned x = 0; x != 256; ++x)
            {
                const auto* pixel = decoded.data() + (y*256+x)*4;
                if (pixel[0] != (eye ? 197 : 31) || pixel[1] != x
                    || pixel[2] != y || pixel[3] != 231) return 9;
            }
        converter.Reset(); frame.Reset(); decoder.Reset();
        if (!DeleteFileW(paths[eye].c_str())) return 10;
        const auto compactPath = paths[eye] + L".jpg";
        if (FAILED(factory->CreateDecoderFromFilename(compactPath.c_str(), nullptr,
            GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder))
            || FAILED(decoder->GetFrame(0, &frame)) || FAILED(factory->CreateFormatConverter(&converter))
            || FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) return 14;
        if (FAILED(converter->CopyPixels(nullptr, 256*4, static_cast<UINT>(decoded.size()), decoded.data()))) return 15;
        for (unsigned y = 4; y < 92; y += 8)
            for (unsigned x = 4; x < 252; x += 8)
            {
                const auto* pixel = decoded.data() + (y*256+x)*4;
                if (abs(int(pixel[0]) - (eye ? 197 : 31)) > 8 || abs(int(pixel[1])-int(x)) > 8
                    || abs(int(pixel[2])-int(y)) > 8 || pixel[3] != 255) return 16;
            }
        converter.Reset(); frame.Reset(); decoder.Reset();
        if (!DeleteFileW(compactPath.c_str())) return 17;
    }
    factory.Reset();
    CoUninitialize();
    if (!RemoveDirectoryW(directory)) return 11;
    std::puts("Async stereo PNG encoding preserved pixels, padded rows, alpha and frame identity");
    return 0;
}
