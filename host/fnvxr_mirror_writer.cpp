#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include "fnvxr_mirror_writer.h"

#include <condition_variable>
#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <limits>
#include <mutex>
#include <thread>

namespace fnvxr::host::mirror
{
namespace
{
bool valid(const Image& image)
{
    return !image.path.empty() && image.width != 0 && image.height != 0
        && image.width <= 16384 && image.height <= 16384
        && image.rowPitch >= std::uint64_t(image.width) * 4
        && std::uint64_t(image.rowPitch) * image.height == image.pixels.size()
        && image.pixels.size() <= (std::numeric_limits<UINT>::max)();
}
void appendBigEndian(std::vector<std::uint8_t>& bytes, std::uint32_t value)
{
    for (int shift = 24; shift >= 0; shift -= 8)
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}
void appendChunk(std::vector<std::uint8_t>& png, const char* type,
    const std::vector<std::uint8_t>& data)
{
    static const auto table = [] {
        std::array<std::uint32_t, 256> values {};
        for (std::uint32_t n = 0; n != 256; ++n)
        {
            auto c = n;
            for (int bit = 0; bit != 8; ++bit)
                c = (c >> 1) ^ ((c & 1) ? 0xedb88320u : 0u);
            values[n] = c;
        }
        return values;
    }();
    appendBigEndian(png, static_cast<std::uint32_t>(data.size()));
    const auto start = png.size();
    png.insert(png.end(), type, type + 4);
    png.insert(png.end(), data.begin(), data.end());
    std::uint32_t crc = 0xffffffffu;
    for (std::size_t i = start; i != png.size(); ++i)
        crc = table[(crc ^ png[i]) & 255u] ^ (crc >> 8);
    appendBigEndian(png, crc ^ 0xffffffffu);
}
HRESULT writePng(Image& image)
{
    if (!valid(image)) return E_INVALIDARG;
    // Recording is opt-in and bounded. Stored DEFLATE blocks trade temporary
    // disk space for deterministic lossless capture without a compression stall.
    const auto stride = std::size_t(image.width) * 4 + 1;
    std::vector<std::uint8_t> filtered(stride * image.height);
    for (std::uint32_t y = 0; y != image.height; ++y)
    {
        auto* row = filtered.data() + std::size_t(y) * stride;
        const auto* source = image.pixels.data() + std::size_t(y) * image.rowPitch;
        row[0] = 0;
        std::memcpy(row + 1, source, stride - 1);
        if (!image.rgba)
            for (std::uint32_t x = 0; x != image.width; ++x)
                std::swap(row[1 + x*4], row[3 + x*4]);
    }
    std::uint32_t adlerA = 1, adlerB = 0;
    for (std::size_t start = 0; start != filtered.size();)
    {
        const auto end = (std::min)(start + 5552, filtered.size());
        for (; start != end; ++start)
        {
            adlerA += filtered[start];
            adlerB += adlerA;
        }
        adlerA %= 65521u;
        adlerB %= 65521u;
    }
    std::vector<std::uint8_t> zlib { 0x78, 0x01 };
    zlib.reserve(filtered.size() + filtered.size()/65535*5 + 16);
    for (std::size_t offset = 0; offset != filtered.size();)
    {
        const auto size = static_cast<std::uint16_t>(
            (std::min)(std::size_t(65535), filtered.size() - offset));
        const auto inverse = static_cast<std::uint16_t>(~size);
        zlib.push_back(offset + size == filtered.size() ? 1 : 0);
        zlib.push_back(static_cast<std::uint8_t>(size));
        zlib.push_back(static_cast<std::uint8_t>(size >> 8));
        zlib.push_back(static_cast<std::uint8_t>(inverse));
        zlib.push_back(static_cast<std::uint8_t>(inverse >> 8));
        zlib.insert(zlib.end(), filtered.begin() + offset, filtered.begin() + offset + size);
        offset += size;
    }
    appendBigEndian(zlib, (adlerB << 16) | adlerA);
    std::vector<std::uint8_t> png { 137, 80, 78, 71, 13, 10, 26, 10 };
    png.reserve(zlib.size() + 70);
    std::vector<std::uint8_t> header;
    appendBigEndian(header, image.width);
    appendBigEndian(header, image.height);
    header.insert(header.end(), { 8, 6, 0, 0, 0 });
    appendChunk(png, "IHDR", header);
    appendChunk(png, "IDAT", zlib);
    appendChunk(png, "IEND", {});
    const auto temporary = image.path + L".tmp";
    // Files are unique to this explicitly requested capture run. Publishing
    // only after encoder commit keeps interrupted output distinguishable.
    DeleteFileW(temporary.c_str());
    const HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return HRESULT_FROM_WIN32(GetLastError());
    DWORD written = 0;
    const BOOL saved = WriteFile(file, png.data(), static_cast<DWORD>(png.size()), &written, nullptr);
    HRESULT result = saved && written == png.size() ? S_OK : E_FAIL;
    CloseHandle(file);
    if (SUCCEEDED(result) && !MoveFileExW(temporary.c_str(), image.path.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        result = HRESULT_FROM_WIN32(GetLastError());
    if (FAILED(result)) DeleteFileW(temporary.c_str());
    return result;
}

HRESULT writeJpeg(Image& image)
{
    if (!valid(image)) return E_INVALIDARG;
    using Microsoft::WRL::ComPtr;
    ComPtr<IWICImagingFactory> factory;
    auto result = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (FAILED(result)) return result;
    const auto temporary = image.path + L".tmp";
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapEncoder> encoder;
    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> options;
    if (SUCCEEDED(result)) result = factory->CreateStream(&stream);
    if (SUCCEEDED(result)) result = stream->InitializeFromFilename(temporary.c_str(), GENERIC_WRITE);
    if (SUCCEEDED(result)) result = factory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &encoder);
    if (SUCCEEDED(result)) result = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (SUCCEEDED(result)) result = encoder->CreateNewFrame(&frame, &options);
    PROPBAG2 property {}; property.pstrName = const_cast<wchar_t*>(L"ImageQuality");
    VARIANT quality {}; quality.vt = VT_R4; quality.fltVal = 0.9f;
    if (SUCCEEDED(result)) result = options->Write(1, &property, &quality);
    if (SUCCEEDED(result)) result = frame->Initialize(options.Get());
    if (SUCCEEDED(result)) result = frame->SetSize(image.width, image.height);
    WICPixelFormatGUID format = GUID_WICPixelFormat24bppBGR;
    if (SUCCEEDED(result)) result = frame->SetPixelFormat(&format);
    if (SUCCEEDED(result) && format != GUID_WICPixelFormat24bppBGR) result = E_FAIL;
    if (SUCCEEDED(result))
    {
        std::vector<BYTE> bgr(std::size_t(image.width) * image.height * 3);
        for (UINT y = 0; y < image.height; ++y)
            for (UINT x = 0; x < image.width; ++x)
            {
                const auto* source = image.pixels.data() + std::size_t(y)*image.rowPitch + x*4;
                auto* dest = bgr.data() + (std::size_t(y)*image.width + x)*3;
                dest[0] = source[image.rgba ? 2 : 0]; dest[1] = source[1]; dest[2] = source[image.rgba ? 0 : 2];
            }
        result = frame->WritePixels(image.height, image.width*3, static_cast<UINT>(bgr.size()), bgr.data());
    }
    if (SUCCEEDED(result)) result = frame->Commit();
    if (SUCCEEDED(result)) result = encoder->Commit();
    frame.Reset(); encoder.Reset(); stream.Reset();
    if (SUCCEEDED(result) && !MoveFileExW(temporary.c_str(), image.path.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) result = HRESULT_FROM_WIN32(GetLastError());
    if (FAILED(result)) DeleteFileW(temporary.c_str());
    return result;
}
}
struct Writer::State
{
    std::mutex mutex;
    std::condition_variable available;
    std::deque<Pair> pending;
    std::vector<Result> completed;
    bool stopping = false;
    std::thread worker;

    State() : worker([this] { run(); }) {}
    void run()
    {
        const auto com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        for (;;)
        {
            Pair pair;
            {
                std::unique_lock<std::mutex> lock(mutex);
                available.wait(lock, [this] { return stopping || !pending.empty(); });
                if (pending.empty()) break;
                pair = std::move(pending.front());
                pending.pop_front();
            }
            Result result { pair.frame, pair.ordinal, pair.eyes[0].width,
                pair.eyes[0].height, pair.eyes[0].format, {}, pair.hashes };
            for (std::size_t eye = 0; eye != 2; ++eye)
            {
                try { result.status[eye] = pair.eyes[eye].jpeg
                    ? writeJpeg(pair.eyes[eye]) : writePng(pair.eyes[eye]); }
                catch (...) { result.status[eye] = E_OUTOFMEMORY; }
            }
            std::lock_guard<std::mutex> lock(mutex);
            completed.push_back(result);
        }
        if (SUCCEEDED(com)) CoUninitialize();
    }
};
Writer::Writer() : state_(std::make_unique<State>()) {}
Writer::~Writer() { finish(); }
bool Writer::submit(Pair&& pair)
{
    if (!pair.frame || !pair.ordinal || !valid(pair.eyes[0]) || !valid(pair.eyes[1])
        || pair.eyes[0].width != pair.eyes[1].width || pair.eyes[0].height != pair.eyes[1].height
        || pair.eyes[0].path == pair.eyes[1].path) return false;
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (state_->stopping || state_->pending.size() >= 2) return false;
    state_->pending.push_back(std::move(pair));
    state_->available.notify_one();
    return true;
}
std::vector<Result> Writer::takeCompleted()
{
    std::lock_guard<std::mutex> lock(state_->mutex);
    std::vector<Result> result;
    result.swap(state_->completed);
    return result;
}
void Writer::finish()
{
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        state_->stopping = true;
    }
    state_->available.notify_one();
    if (state_->worker.joinable()) state_->worker.join();
}
}
