#pragma once
#include <d3d9.h>
#include <array>
#include <cstdint>

namespace fnvxr::d3d9
{
// Mirrors only draws inside the verified native InterfaceManager render call.
// The game still draws its original interface once and owns all UI state.
class NativeUiCapture
{
public:
    using SetTarget = HRESULT (WINAPI*)(IDirect3DDevice9*, DWORD, IDirect3DSurface9*);
    using SetDepth = HRESULT (WINAPI*)(IDirect3DDevice9*, IDirect3DSurface9*);
    using Clear = HRESULT (WINAPI*)(IDirect3DDevice9*, DWORD, const D3DRECT*, DWORD,
        D3DCOLOR, float, DWORD);
    ~NativeUiCapture() { reset(); }
    void reset() noexcept
    {
        if (surface_) surface_->Release();
        if (backbuffer_) backbuffer_->Release();
        if (depth_) depth_->Release();
        surface_ = backbuffer_ = nullptr;
        depth_ = nullptr;
        active_ = complete_ = false;
        draws_ = 0;
        observedDraws_ = 0;
    }
    void begin(IDirect3DDevice9* device, std::uint32_t menuBits,
        SetTarget setTarget, SetDepth setDepth, Clear clear) noexcept
    {
        active_ = complete_ = false;
        draws_ = 0;
        observedDraws_ = 0;
        firstTargetWidth_ = firstTargetHeight_ = 0;
        failed_ = false;
        beginStatus_ = E_PENDING;
        menuBits_ = menuBits;
        if (!device || !menuBits || !setTarget || !setDepth || !clear) return;
        IDirect3DSurface9* current = nullptr;
        beginStatus_ = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &current);
        if (FAILED(beginStatus_)) return;
        if (backbuffer_ != current) reset();
        if (backbuffer_) current->Release(); else backbuffer_ = current;
        if (!surface_)
        {
            D3DSURFACE_DESC description {};
            beginStatus_ = backbuffer_->GetDesc(&description);
            if (FAILED(beginStatus_)) return;
            beginStatus_ = device->CreateRenderTarget(description.Width, description.Height,
                D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &surface_, nullptr);
            if (FAILED(beginStatus_)) return;
        }
        if (!depth_)
        {
            D3DSURFACE_DESC description {};
            beginStatus_ = surface_->GetDesc(&description);
            if (FAILED(beginStatus_)) return;
            beginStatus_ = device->CreateDepthStencilSurface(description.Width,
                description.Height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE,
                &depth_, nullptr);
            if (FAILED(beginStatus_)) return;
        }
        // UI clip masks use native depth/stencil draws, including draws that
        // intentionally write no color. Give the replay its own cleared buffer;
        // never duplicate stencil writes into the game's original buffer.
        IDirect3DSurface9* originalTarget = nullptr;
        IDirect3DSurface9* originalDepth = nullptr;
        D3DVIEWPORT9 viewport {};
        RECT scissorRect {};
        DWORD scissor = FALSE;
        const bool ready = SUCCEEDED(device->GetRenderTarget(0, &originalTarget))
            && SUCCEEDED(device->GetViewport(&viewport))
            && SUCCEEDED(device->GetScissorRect(&scissorRect))
            && SUCCEEDED(device->GetRenderState(D3DRS_SCISSORTESTENABLE, &scissor));
        device->GetDepthStencilSurface(&originalDepth);
        if (ready)
        {
            D3DSURFACE_DESC description {};
            surface_->GetDesc(&description);
            const D3DVIEWPORT9 full { 0, 0, description.Width, description.Height, 0.0F, 1.0F };
            const bool bound = SUCCEEDED(setDepth(device, nullptr))
                && SUCCEEDED(setTarget(device, 0, surface_))
                && SUCCEEDED(setDepth(device, depth_))
                && SUCCEEDED(device->SetViewport(&full))
                && SUCCEEDED(device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE));
            beginStatus_ = bound ? clear(device, 0, nullptr,
                D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0F, 0) : E_FAIL;
            bool restored = SUCCEEDED(setDepth(device, nullptr));
            restored = SUCCEEDED(setTarget(device, 0, originalTarget)) && restored;
            restored = SUCCEEDED(setDepth(device, originalDepth)) && restored;
            restored = SUCCEEDED(device->SetViewport(&viewport)) && restored;
            restored = SUCCEEDED(device->SetScissorRect(&scissorRect)) && restored;
            restored = SUCCEEDED(device->SetRenderState(D3DRS_SCISSORTESTENABLE, scissor)) && restored;
            active_ = SUCCEEDED(beginStatus_) && restored;
        }
        if (originalDepth) originalDepth->Release();
        if (originalTarget) originalTarget->Release();
    }
    void end() noexcept { complete_ = active_ && !failed_ && draws_ != 0; active_ = false; }
    std::uint32_t draws() const noexcept { return draws_; }
    std::uint32_t observedDraws() const noexcept { return observedDraws_; }
    std::uint32_t firstTargetWidth() const noexcept { return firstTargetWidth_; }
    std::uint32_t firstTargetHeight() const noexcept { return firstTargetHeight_; }
    bool complete() const noexcept { return complete_; }
    HRESULT beginStatus() const noexcept { return beginStatus_; }
    bool copyTo(IDirect3DDevice9* device, IDirect3DSurface9* left,
        IDirect3DSurface9* right, std::uint32_t menuBits) noexcept
    {
        const bool eligible = complete_ && menuBits == menuBits_;
        complete_ = false; // A new UI source requires a new native render call.
        return eligible && left && right
            && SUCCEEDED(device->StretchRect(surface_, nullptr, left, nullptr, D3DTEXF_NONE))
            && SUCCEEDED(device->StretchRect(surface_, nullptr, right, nullptr, D3DTEXF_NONE));
    }
    template<class Draw, class... Args>
    void mirror(IDirect3DDevice9* device, SetTarget setTarget, SetDepth setDepth,
        Draw draw, Args... args) noexcept
    {
        if (!active_ || !setTarget || !setDepth) return;
        IDirect3DSurface9* target = nullptr;
        if (FAILED(device->GetRenderTarget(0, &target))) { failed_ = true; return; }
        ++observedDraws_;
        if (!firstTargetWidth_)
        {
            D3DSURFACE_DESC description {};
            if (SUCCEEDED(target->GetDesc(&description)))
            { firstTargetWidth_ = description.Width; firstTargetHeight_ = description.Height; }
        }
        if (target != backbuffer_) { target->Release(); return; }
        IDirect3DSurface9* depth = nullptr;
        device->GetDepthStencilSurface(&depth);
        D3DVIEWPORT9 viewport {};
        RECT scissorRect {};
        constexpr std::array<D3DRENDERSTATETYPE, 11> states {
            D3DRS_ZENABLE, D3DRS_ZWRITEENABLE, D3DRS_ALPHABLENDENABLE,
            D3DRS_SRCBLEND, D3DRS_DESTBLEND, D3DRS_BLENDOP,
            D3DRS_SEPARATEALPHABLENDENABLE, D3DRS_SRCBLENDALPHA,
            D3DRS_DESTBLENDALPHA, D3DRS_BLENDOPALPHA, D3DRS_COLORWRITEENABLE };
        std::array<DWORD, states.size()> values {};
        bool ready = SUCCEEDED(device->GetViewport(&viewport))
            && SUCCEEDED(device->GetScissorRect(&scissorRect));
        for (std::size_t i = 0; i != states.size(); ++i)
            ready = SUCCEEDED(device->GetRenderState(states[i], &values[i])) && ready;
        // Native UI uses one color output. Never mirror into an unrelated MRT.
        IDirect3DSurface9* extra = nullptr;
        device->GetRenderTarget(1, &extra);
        if (extra) { extra->Release(); ready = false; }
        if (ready)
        {
            ready = SUCCEEDED(setDepth(device, nullptr))
                && SUCCEEDED(setTarget(device, 0, surface_))
                && SUCCEEDED(setDepth(device, depth_))
                && SUCCEEDED(device->SetViewport(&viewport))
                && SUCCEEDED(device->SetScissorRect(&scissorRect));
            const std::array<DWORD, states.size()> captured {
                values[0], values[1], TRUE,
                values[2] ? values[3] : D3DBLEND_ONE,
                values[2] ? values[4] : D3DBLEND_ZERO,
                values[2] ? values[5] : D3DBLENDOP_ADD,
                TRUE, D3DBLEND_ONE, D3DBLEND_INVSRCALPHA, D3DBLENDOP_ADD,
                (values[10] & 7u) ? (values[10] | D3DCOLORWRITEENABLE_ALPHA) : values[10] };
            for (std::size_t i = 0; i != states.size(); ++i)
                ready = SUCCEEDED(device->SetRenderState(states[i], captured[i])) && ready;
            if (ready && SUCCEEDED(draw(device, args...))) ++draws_; else failed_ = true;
            for (std::size_t i = 0; i != states.size(); ++i)
                if (FAILED(device->SetRenderState(states[i], values[i]))) failed_ = true;
            if (FAILED(setDepth(device, nullptr))) failed_ = true;
            if (FAILED(setTarget(device, 0, target))) failed_ = true;
            if (FAILED(setDepth(device, depth))) failed_ = true;
            if (FAILED(device->SetViewport(&viewport))) failed_ = true;
            if (FAILED(device->SetScissorRect(&scissorRect))) failed_ = true;
        }
        else failed_ = true;
        if (depth) depth->Release();
        target->Release();
    }
private:
    IDirect3DSurface9* surface_ = nullptr;
    IDirect3DSurface9* backbuffer_ = nullptr;
    IDirect3DSurface9* depth_ = nullptr;
    std::uint32_t menuBits_ = 0, draws_ = 0;
    std::uint32_t observedDraws_ = 0, firstTargetWidth_ = 0, firstTargetHeight_ = 0;
    bool active_ = false, complete_ = false, failed_ = false;
    HRESULT beginStatus_ = E_PENDING;
};
}
