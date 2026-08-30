// Split from periscope_overlay.cpp — see MONOLITH_REFACTOR_LEDGER.
#include "real/gpu/periscope_overlay.hpp"
#include "real/win/peb_util.hpp"
#include "real/win/timing.hpp"
#include "real/win/xorstr.hpp"

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#include "real/win/api_table.hpp"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#if LR_COMPILER_MSVC
#include <intrin.h>
#endif
#pragma comment(lib, "d3dcompiler.lib")
#endif

#include <cstring>
#include <cstdio>
#include <cstdint>
#include <random>
#include <vector>

namespace real::gpu::periscope {

#if LR_PLATFORM_WINDOWS
// ====================================================================
// D3D11 XOR surface scramble — PIMPL state (D3D11 types stay in .cpp)
// ====================================================================

namespace {

struct ScrambleState {
    ID3D11Device* device{};
    ID3D11DeviceContext* context{};
    IDXGISwapChain* swap_chain{};
    ID3D11RenderTargetView* rtv{};
    ID3D11VertexShader* vs{};
    ID3D11PixelShader* ps{};
    ID3D11InputLayout* layout{};
    ID3D11Buffer* vb{};
    ID3D11Buffer* ib{};
    ID3D11Texture2D* noise_tex{};
    ID3D11ShaderResourceView* noise_srv{};
    ID3D11SamplerState* sampler{};
    ID3D11BlendState* blend{};
    ID3D11RasterizerState* raster{};
    int width{};
    int height{};
};

constexpr uint32_t kScrambleNoiseDim = 64;
constexpr uint64_t kScrambleKeyMul = 0x9E3779B97F4A7C15ULL;
constexpr uint64_t kScrambleKeyAdd = 0x14057B7EF767814FULL;

struct ScrambleVertex {
    float x, y, u, v;
};

constexpr ScrambleVertex kScrambleQuad[4] = {
    {-1.0f, -1.0f, 0.0f, 1.0f},
    { 1.0f, -1.0f, 1.0f, 1.0f},
    {-1.0f,  1.0f, 0.0f, 0.0f},
    { 1.0f,  1.0f, 1.0f, 0.0f},
};

constexpr uint16_t kScrambleIndices[6] = {0, 1, 2, 2, 1, 3};

// CPU-generate a 64x64 RGBA noise block from the rolling XOR key. The
// pattern is deterministic per key and changes every frame as the key
// advances, so each captured frame shows a unique scramble.
void scramble_fill_noise(uint8_t* out, uint64_t key) noexcept {
    for (uint32_t y = 0; y < kScrambleNoiseDim; ++y) {
        for (uint32_t x = 0; x < kScrambleNoiseDim; ++x) {
            uint64_t seed = key ^ (static_cast<uint64_t>(x) * kScrambleKeyMul +
                                   static_cast<uint64_t>(y) * kScrambleKeyAdd);
            seed ^= seed >> 33;
            seed *= 0xFF51AFD7ED558CCDULL;
            seed ^= seed >> 29;
            const uint32_t v = static_cast<uint32_t>(seed);
            uint8_t* p = out + (y * kScrambleNoiseDim + x) * 4;
            p[0] = static_cast<uint8_t>(v);
            p[1] = static_cast<uint8_t>(v >> 8);
            p[2] = static_cast<uint8_t>(v >> 16);
            p[3] = 0xFF;  // Opaque — the scramble must cover the surface
        }
    }
}

// Release every COM pointer held by the scramble state.
void scramble_release_state(ScrambleState* state) noexcept {
    if (!state) return;
    if (state->context) {
        state->context->ClearState();
        state->context->Release();
        state->context = nullptr;
    }
    if (state->sampler) { state->sampler->Release(); state->sampler = nullptr; }
    if (state->noise_srv) { state->noise_srv->Release(); state->noise_srv = nullptr; }
    if (state->noise_tex) { state->noise_tex->Release(); state->noise_tex = nullptr; }
    if (state->blend) { state->blend->Release(); state->blend = nullptr; }
    if (state->raster) { state->raster->Release(); state->raster = nullptr; }
    if (state->ib) { state->ib->Release(); state->ib = nullptr; }
    if (state->vb) { state->vb->Release(); state->vb = nullptr; }
    if (state->layout) { state->layout->Release(); state->layout = nullptr; }
    if (state->ps) { state->ps->Release(); state->ps = nullptr; }
    if (state->vs) { state->vs->Release(); state->vs = nullptr; }
    if (state->rtv) { state->rtv->Release(); state->rtv = nullptr; }
    if (state->swap_chain) { state->swap_chain->Release(); state->swap_chain = nullptr; }
    if (state->device) { state->device->Release(); state->device = nullptr; }
}

// Shaders, full-screen quad buffers, noise texture and fixed pipeline
// state. Mirrors render_pipeline.cpp's resource-creation pattern.
bool scramble_create_pipeline(ScrambleState* state) noexcept {
    if (!state || !state->device) return false;

    static constexpr const char* scramble_vs_src = R"(
      struct VSIn { float2 pos : POSITION; float2 uv : TEXCOORD; };
      struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD; };
      VSOut main(VSIn input) {
        VSOut output;
        output.pos = float4(input.pos, 0.0f, 1.0f);
        output.uv = input.uv;
        return output;
      }
    )";
    static constexpr const char* scramble_ps_src = R"(
      Texture2D scramble_tex : register(t0);
      SamplerState scramble_sampler : register(s0);
      struct PSIn { float4 pos : SV_Position; float2 uv : TEXCOORD; };
      float4 main(PSIn input) : SV_Target {
        return scramble_tex.Sample(scramble_sampler, input.uv);
      }
    )";

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* errors = nullptr;
    HRESULT hr = D3DCompile(scramble_vs_src, std::strlen(scramble_vs_src),
                            nullptr, nullptr, nullptr, "main", "vs_4_0",
                            0, 0, &vs_blob, &errors);
    if (FAILED(hr)) {
        if (errors) errors->Release();
        return false;
    }
    hr = D3DCompile(scramble_ps_src, std::strlen(scramble_ps_src),
                    nullptr, nullptr, nullptr, "main", "ps_4_0",
                    0, 0, &ps_blob, &errors);
    if (FAILED(hr)) {
        if (errors) errors->Release();
        vs_blob->Release();
        return false;
    }

    hr = state->device->CreateVertexShader(
        vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, &state->vs);
    if (SUCCEEDED(hr)) hr = state->device->CreatePixelShader(
        ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, &state->ps);

    D3D11_INPUT_ELEMENT_DESC input_layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    if (SUCCEEDED(hr)) hr = state->device->CreateInputLayout(
        input_layout, 2, vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), &state->layout);
    vs_blob->Release();
    ps_blob->Release();
    if (FAILED(hr)) return false;

    D3D11_BUFFER_DESC vb_desc = {};
    vb_desc.Usage = D3D11_USAGE_IMMUTABLE;
    vb_desc.ByteWidth = sizeof(ScrambleVertex) * 4;
    vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vb_init = {};
    vb_init.pSysMem = kScrambleQuad;
    if (FAILED(state->device->CreateBuffer(&vb_desc, &vb_init, &state->vb))) return false;

    D3D11_BUFFER_DESC ib_desc = {};
    ib_desc.Usage = D3D11_USAGE_IMMUTABLE;
    ib_desc.ByteWidth = sizeof(uint16_t) * 6;
    ib_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ib_init = {};
    ib_init.pSysMem = kScrambleIndices;
    if (FAILED(state->device->CreateBuffer(&ib_desc, &ib_init, &state->ib))) return false;

    // The GPU texture is repopulated every frame via UpdateSubresource.
    uint8_t noise[kScrambleNoiseDim * kScrambleNoiseDim * 4]{};
    scramble_fill_noise(noise, 1);
    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.Width = kScrambleNoiseDim;
    tex_desc.Height = kScrambleNoiseDim;
    tex_desc.MipLevels = 1;
    tex_desc.ArraySize = 1;
    tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.Usage = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA tex_init = {};
    tex_init.pSysMem = noise;
    tex_init.SysMemPitch = kScrambleNoiseDim * 4;
    if (FAILED(state->device->CreateTexture2D(&tex_desc, &tex_init, &state->noise_tex))) return false;
    if (FAILED(state->device->CreateShaderResourceView(
            state->noise_tex, nullptr, &state->noise_srv))) return false;

    D3D11_SAMPLER_DESC sampler_desc = {};
    sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(state->device->CreateSamplerState(&sampler_desc, &state->sampler))) return false;

    D3D11_BLEND_DESC blend_desc = {};
    blend_desc.RenderTarget[0].BlendEnable = TRUE;
    blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(state->device->CreateBlendState(&blend_desc, &state->blend))) return false;

    D3D11_RASTERIZER_DESC raster_desc = {};
    raster_desc.FillMode = D3D11_FILL_SOLID;
    raster_desc.CullMode = D3D11_CULL_NONE;
    raster_desc.DepthClipEnable = TRUE;
    if (FAILED(state->device->CreateRasterizerState(&raster_desc, &state->raster))) return false;

    return true;
}

} // namespace
#endif // LR_PLATFORM_WINDOWS

bool StealthOverlay::scramble_surface() noexcept {
#if LR_PLATFORM_WINDOWS
    if (!m_hwnd || !m_scramble_armed) return false;

    if (!scramble_ensure_initialized()) return false;

    auto* state = static_cast<ScrambleState*>(m_scramble);
    if (!state || !state->device || !state->context || !state->swap_chain ||
        !state->rtv || !state->noise_tex || !state->noise_srv) {
        return false;
    }

    // Recreate the render target if the overlay window was resized.
    RECT rc{};
    if (GetClientRect(reinterpret_cast<HWND>(m_hwnd), &rc)) {
        const int w = rc.right - rc.left;
        const int h = rc.bottom - rc.top;
        if (w > 0 && h > 0 && (w != state->width || h != state->height)) {
            if (state->rtv) { state->rtv->Release(); state->rtv = nullptr; }
            state->context->OMSetRenderTargets(0, nullptr, nullptr);
            HRESULT hr = state->swap_chain->ResizeBuffers(
                0, static_cast<UINT>(w), static_cast<UINT>(h),
                DXGI_FORMAT_UNKNOWN, 0);
            if (FAILED(hr)) return false;
            state->width = w;
            state->height = h;
            ID3D11Texture2D* backbuffer = nullptr;
            hr = state->swap_chain->GetBuffer(0, IID_PPV_ARGS(&backbuffer));
            if (SUCCEEDED(hr)) hr = state->device->CreateRenderTargetView(
                backbuffer, nullptr, &state->rtv);
            if (backbuffer) backbuffer->Release();
            if (FAILED(hr) || !state->rtv) return false;
        }
    }

    // Advance the rolling XOR key so the pattern changes every frame.
    if (m_scramble_key == 0) {
        unsigned int aux;
        m_scramble_key = static_cast<uint64_t>(__rdtscp(&aux)) ^ build::kXorKeySeed;
    }
    m_scramble_key = m_scramble_key * kScrambleKeyMul + kScrambleKeyAdd;

    // CPU-generate the per-frame noise and push it into the GPU texture.
    uint8_t noise[kScrambleNoiseDim * kScrambleNoiseDim * 4]{};
    scramble_fill_noise(noise, m_scramble_key);
    state->context->UpdateSubresource(state->noise_tex, 0, nullptr, noise,
                                      kScrambleNoiseDim * 4, 0);

    // Draw the full-screen quad textured with the scrambled noise.
    UINT stride = sizeof(ScrambleVertex);
    UINT offset = 0;
    state->context->OMSetRenderTargets(1, &state->rtv, nullptr);
    const D3D11_VIEWPORT viewport = {0.0f, 0.0f,
                                     static_cast<float>(state->width),
                                     static_cast<float>(state->height),
                                     0.0f, 1.0f};
    state->context->RSSetViewports(1, &viewport);
    state->context->IASetVertexBuffers(0, 1, &state->vb, &stride, &offset);
    state->context->IASetIndexBuffer(state->ib, DXGI_FORMAT_R16_UINT, 0);
    state->context->IASetInputLayout(state->layout);
    state->context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    state->context->VSSetShader(state->vs, nullptr, 0);
    state->context->PSSetShader(state->ps, nullptr, 0);
    state->context->PSSetShaderResources(0, 1, &state->noise_srv);
    state->context->PSSetSamplers(0, 1, &state->sampler);
    if (state->blend) state->context->OMSetBlendState(state->blend, nullptr, 0xFFFFFFFF);
    if (state->raster) state->context->RSSetState(state->raster);
    state->context->DrawIndexed(6, 0, 0);

    const HRESULT hr = state->swap_chain->Present(0, 0);
    if (FAILED(hr)) {
        // Present can fail on device-lost; drop the pipeline and retry lazily.
        scramble_release();
        return false;
    }

    m_scramble_active = true;
    return true;
#else
    return false;
#endif
}

bool StealthOverlay::scramble_ensure_initialized() noexcept {
#if LR_PLATFORM_WINDOWS
    if (m_scramble) return true;

    HWND hwnd = reinterpret_cast<HWND>(m_hwnd);
    if (!hwnd) return false;

    RECT rc{};
    if (!GetClientRect(hwnd, &rc)) return false;
    const int width = rc.right - rc.left;
    const int height = rc.bottom - rc.top;
    if (width <= 0 || height <= 0) return false;

    auto* state = new ScrambleState{};
    state->width = width;
    state->height = height;

    DXGI_SWAP_CHAIN_DESC swap_desc = {};
    swap_desc.BufferCount = 1;
    swap_desc.BufferDesc.Width = static_cast<UINT>(width);
    swap_desc.BufferDesc.Height = static_cast<UINT>(height);
    swap_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_desc.BufferDesc.RefreshRate.Numerator = 60;
    swap_desc.BufferDesc.RefreshRate.Denominator = 1;
    swap_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_desc.OutputWindow = hwnd;
    swap_desc.SampleDesc.Count = 1;
    swap_desc.Windowed = TRUE;
    swap_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    // Direct D3D11CreateDeviceAndSwapChain with the correct 12-arg ABI
    // (device, feature-level out, context). Avoids the ApiTable packed typedef.
    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
    D3D_FEATURE_LEVEL out_level = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = ::D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, &feature_level, 1,
        D3D11_SDK_VERSION, &swap_desc,
        &state->swap_chain, &state->device, &out_level, &state->context);
    if (FAILED(hr) || !state->device || !state->context || !state->swap_chain) {
        delete state;
        return false;
    }

    ID3D11Texture2D* backbuffer = nullptr;
    hr = state->swap_chain->GetBuffer(0, IID_PPV_ARGS(&backbuffer));
    if (SUCCEEDED(hr)) hr = state->device->CreateRenderTargetView(
        backbuffer, nullptr, &state->rtv);
    if (backbuffer) backbuffer->Release();
    if (FAILED(hr) || !state->rtv) {
        scramble_release_state(state);
        delete state;
        return false;
    }

    if (!scramble_create_pipeline(state)) {
        scramble_release_state(state);
        delete state;
        return false;
    }

    m_scramble = state;
    return true;
#else
    return false;
#endif
}

void StealthOverlay::scramble_release() noexcept {
#if LR_PLATFORM_WINDOWS
    if (m_scramble) {
        auto* state = static_cast<ScrambleState*>(m_scramble);
        scramble_release_state(state);
        delete state;
        m_scramble = nullptr;
    }
#endif
    m_scramble_armed = false;
    m_scramble_active = false;
    m_scramble_key = 0;
}

} // namespace real::gpu::periscope

