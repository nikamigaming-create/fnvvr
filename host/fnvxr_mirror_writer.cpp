#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include "fnvxr_mirror_writer.h"

#include <condition_variable>
#include <deque>
#include <limits>
#include <mutex>
#include <thread>

namespace fnvxr::host::mirror
{
using Microsoft::WRL::ComPtr;
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
HRESULT writePng(IWICImagingFactory* factory, Image& image)
{
    if (!factory || !valid(image)) return E_INVALIDARG;
    if (image.rgba)
        for (std::uint32_t y = 0; y != image.height; ++y)
            for (std::uint32_t x = 0; x != image.width; ++x)
                std::swap(image.pixels[std::size_t(y)*image.rowPitch + x*4],
                    image.pixels[std::size_t(y)*image.rowPitch + x*4 + 2]);
    const auto temporary = image.path + L".tmp";
    // Files are unique to this explicitly requested capture run. Publishing
    // only after encoder commit keeps interrupted output distinguishable.
    DeleteFileW(temporary.c_str());
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapEncoder> encoder;
    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> properties;
    HRESULT result = factory->CreateStream(&stream);
    if (SUCCEEDED(result)) result = stream->InitializeFromFilename(temporary.c_str(), GENERIC_WRITE);
    if (SUCCEEDED(result)) result = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    if (SUCCEEDED(result)) result = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (SUCCEEDED(result)) result = encoder->CreateNewFrame(&frame, &properties);
    if (SUCCEEDED(result)) result = frame->Initialize(properties.Get());
    if (SUCCEEDED(result)) result = frame->SetSize(image.width, image.height);
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    if (SUCCEEDED(result)) result = frame->SetPixelFormat(&format);
    if (SUCCEEDED(result) && !IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA))
        result = WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT;
    if (SUCCEEDED(result)) result = frame->WritePixels(image.height, image.rowPitch,
        static_cast<UINT>(image.pixels.size()), image.pixels.data());
    if (SUCCEEDED(result)) result = frame->Commit();
    if (SUCCEEDED(result)) result = encoder->Commit();
    frame.Reset();
    properties.Reset();
    encoder.Reset();
    stream.Reset();
    if (SUCCEEDED(result) && !MoveFileExW(temporary.c_str(), image.path.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        result = HRESULT_FROM_WIN32(GetLastError());
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
        const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ComPtr<IWICImagingFactory> factory;
        const HRESULT created = SUCCEEDED(com)
            ? CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&factory)) : com;
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
                try { result.status[eye] = FAILED(created) ? created : writePng(factory.Get(), pair.eyes[eye]); }
                catch (...) { result.status[eye] = E_OUTOFMEMORY; }
            }
            std::lock_guard<std::mutex> lock(mutex);
            completed.push_back(result);
        }
        factory.Reset();
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
