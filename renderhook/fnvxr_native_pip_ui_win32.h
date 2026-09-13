#pragma once
#include <d3d9.h>
#include <array>
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace fnvxr::d3d9
{
// Retail 1.4.0.525 caches BSTM_RT_INTERFACE_RENDEREDMENU in the NiPointer at
// 0x11DEC64 (allocation/assignment at 0x871B02). It is retained outside the
// texture manager's borrowed/unused lists. Track native writes and sampling
// of that exact texture, including its authored viewport.
class NativePipUiCapture
{
    struct Slot
    {
        IDirect3DTexture9* texture = nullptr;
        IDirect3DSurface9* surface = nullptr;
        D3DSURFACE_DESC description {};
        RECT viewport {};
        bool written = false;
    };
    std::array<Slot, 4> slots_ {};
    std::size_t count_ = 0;
    int sampled_ = -1;

    static std::uintptr_t pointer(std::uintptr_t address) noexcept
    { return address ? *reinterpret_cast<const std::uint32_t*>(address) : 0; }

    void refresh() noexcept
    {
        __try
        {
            const auto* allocation = reinterpret_cast<const unsigned char*>(0x00871B02);
            std::uint64_t hash = 14695981039346656037ull;
            for (unsigned i = 0; i != 37; ++i) hash = (hash ^ allocation[i]) * 1099511628211ull;
            if (hash != 0xb166a7dc957fa168ull) return;
            const auto rendered = pointer(0x011DEC64);
            if (!rendered) return;
            const auto niTexture = pointer(rendered + 0x30u);
            const auto rendererData = niTexture ? pointer(niTexture + 0x24u) : 0;
            auto* texture = reinterpret_cast<IDirect3DTexture9*>(
                rendererData ? pointer(rendererData + 0x64u) : 0);
            if (!texture) return;
            for (std::size_t i = 0; i != count_; ++i)
                if (slots_[i].texture == texture) return;
            if (count_ == slots_.size()) reset();
            auto& slot = slots_[count_];
            if (FAILED(texture->GetLevelDesc(0, &slot.description))
                || !(slot.description.Usage & D3DUSAGE_RENDERTARGET)
                || FAILED(texture->GetSurfaceLevel(0, &slot.surface))) return;
            texture->AddRef();
            slot.texture = texture;
            ++count_;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { reset(); }
    }
public:
    ~NativePipUiCapture() { reset(); }
    void reset() noexcept
    {
        for (auto& slot : slots_)
        {
            if (slot.surface) slot.surface->Release();
            if (slot.texture) slot.texture->Release();
            slot = {};
        }
        count_ = 0; sampled_ = -1;
    }
    std::size_t targets() const noexcept { return count_; }
    bool sampled() const noexcept { return sampled_ >= 0; }
    RECT sourceRegion() const noexcept
    {
        if (sampled_ < 0) return {};
        const auto& slot = slots_[sampled_];
        // Authored screen UVs address the original, padded texture extent.
        return { 0, 0, static_cast<LONG>(slot.description.Width), static_cast<LONG>(slot.description.Height) };
    }
    void observe(IDirect3DDevice9* device) noexcept
    {
        // The native cached menu target can be allocated after the first draw
        // and is not necessarily redrawn every frame. Discover it when it
        // becomes available and retain its write history until menu/device reset.
        refresh();
        if (!count_) return;
        IDirect3DSurface9* target = nullptr;
        if (SUCCEEDED(device->GetRenderTarget(0, &target)) && target)
        {
            D3DVIEWPORT9 viewport {};
            if (SUCCEEDED(device->GetViewport(&viewport)))
                for (std::size_t i = 0; i != count_; ++i)
                {
                    auto& slot = slots_[i];
                    if (slot.surface != target || !viewport.Width || !viewport.Height
                        || viewport.X + viewport.Width > slot.description.Width
                        || viewport.Y + viewport.Height > slot.description.Height) continue;
                    slot.viewport = { static_cast<LONG>(viewport.X), static_cast<LONG>(viewport.Y),
                        static_cast<LONG>(viewport.X + viewport.Width), static_cast<LONG>(viewport.Y + viewport.Height) };
                    slot.written = true;
                }
            target->Release();
        }
        IDirect3DBaseTexture9* sampled = nullptr;
        if (SUCCEEDED(device->GetTexture(0, &sampled)) && sampled)
        {
            for (std::size_t i = 0; i != count_; ++i)
                if (slots_[i].texture == sampled && slots_[i].written) sampled_ = static_cast<int>(i);
            sampled->Release();
        }
    }
    bool copyTo(IDirect3DDevice9* device, IDirect3DSurface9* left, IDirect3DSurface9* right) noexcept
    {
        bool copied = false;
        if (sampled_ >= 0 && left && right)
        {
            const auto& slot = slots_[sampled_];
            const auto region = sourceRegion();
            copied = SUCCEEDED(device->StretchRect(slot.surface, &region, left, nullptr, D3DTEXF_LINEAR))
                && SUCCEEDED(device->StretchRect(slot.surface, &region, right, nullptr, D3DTEXF_LINEAR));
        }
        // A later frame must observe the game sampling this target again, but
        // unchanged native UI pixels remain valid without a redundant RT write.
        sampled_ = -1;
        return copied;
    }
};
}
