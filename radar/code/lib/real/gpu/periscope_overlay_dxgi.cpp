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

// ====================================================================
// DxgiComposite implementation (PIMPL)
// ====================================================================

struct DxgiComposite::DxgiState {
#if LR_PLATFORM_WINDOWS
    ID3D11Device* device{};
    ID3D11DeviceContext* context{};
    IDXGIOutputDuplication* dupe{};
#endif
};

bool DxgiComposite::initialize() noexcept {
#if LR_PLATFORM_WINDOWS
    m_state = new DxgiState{};
    if (!create_device()) {
        shutdown();
        return false;
    }
    if (!create_duplication()) {
        // DXGI dupe may fail on remote desktops — that's OK
        m_initialized = true;  // Allow graceful fallback
        return true;
    }
    m_initialized = true;
    return true;
#else
    return false;
#endif
}

bool DxgiComposite::create_device() noexcept {
#if LR_PLATFORM_WINDOWS
    if (!m_state) return false;

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &m_state->device,
        nullptr,
        &m_state->context
    );

    return SUCCEEDED(hr);
#else
    return false;
#endif
}

bool DxgiComposite::create_duplication() noexcept {
#if LR_PLATFORM_WINDOWS
    if (!m_state || !m_state->device) return false;

    IDXGIDevice* dxgiDevice = nullptr;
    HRESULT hr = m_state->device->QueryInterface(
        __uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
    if (FAILED(hr)) return false;

    IDXGIAdapter* adapter = nullptr;
    hr = dxgiDevice->GetParent(__uuidof(IDXGIAdapter),
                                reinterpret_cast<void**>(&adapter));
    dxgiDevice->Release();
    if (FAILED(hr)) return false;

    IDXGIOutput* output = nullptr;
    hr = adapter->EnumOutputs(0, &output);
    adapter->Release();
    if (FAILED(hr)) return false;

    IDXGIOutput1* output1 = nullptr;
    hr = output->QueryInterface(__uuidof(IDXGIOutput1),
                                 reinterpret_cast<void**>(&output1));
    output->Release();
    if (FAILED(hr)) return false;

    hr = output1->DuplicateOutput(m_state->device, &m_state->dupe);
    output1->Release();

    return SUCCEEDED(hr);
#else
    return false;
#endif
}

void DxgiComposite::shutdown() noexcept {
#if LR_PLATFORM_WINDOWS
    if (m_state) {
        if (m_state->dupe) {
            m_state->dupe->Release();
            m_state->dupe = nullptr;
        }
        if (m_state->context) {
            m_state->context->Release();
            m_state->context = nullptr;
        }
        if (m_state->device) {
            m_state->device->Release();
            m_state->device = nullptr;
        }
        delete m_state;
        m_state = nullptr;
    }
#endif
    m_initialized = false;
}

#if LR_PLATFORM_WINDOWS
// XorShift PRNG — avoids detectable crypto RNG calls
static uint64_t xor_shift(uint64_t& state) {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
}

static inline uint64_t read_timestamp() {
#if LR_COMPILER_MSVC
    unsigned int aux;
    return __rdtscp(&aux);
#else
    unsigned int aux;
    return __builtin_ia32_rdtscp(&aux);
#endif
}

static uint64_t tsc_frequency() {
    // Hardcoded ~2.5 GHz estimate. Absolute accuracy isn't required for timing
    // ratios, and this avoids an observable Sleep(10) IAT entry and 10ms delay.
    return 2500000000ULL;
}
#endif

std::optional<CompositeFrameMetrics> DxgiComposite::acquire_and_composite(
    int timeoutMs) noexcept
{
#if LR_PLATFORM_WINDOWS
    if (!m_state || !m_state->dupe) return std::nullopt;

    CompositeFrameMetrics metrics{};
    DXGI_OUTDUPL_FRAME_INFO info{};
    IDXGIResource* desktopRes = nullptr;

    uint64_t tsc_start = read_timestamp();
    uint64_t tsc_freq = tsc_frequency();

    HRESULT hr = m_state->dupe->AcquireNextFrame(
        static_cast<UINT>(timeoutMs), &info, &desktopRes);

    uint64_t tsc_after_acquire = read_timestamp();

    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_ACCESS_LOST) {
            create_duplication();  // Re-create
        }
        return std::nullopt;
    }

    metrics.acquireNs = (tsc_after_acquire - tsc_start) * 1000000000ULL / tsc_freq;
    metrics.realDxgiPath = true;

    // Prefer ID3D11Texture2D desc for exact width/height, then staging Map for
    // a real BGRA byte count. Fall back to IDXGISurface::Map when QI fails.
    ID3D11Texture2D* desktop_tex = nullptr;
    hr = desktopRes->QueryInterface(__uuidof(ID3D11Texture2D),
                                    reinterpret_cast<void**>(&desktop_tex));
    if (SUCCEEDED(hr) && desktop_tex && m_state->device && m_state->context) {
        D3D11_TEXTURE2D_DESC desc{};
        desktop_tex->GetDesc(&desc);
        if (desc.Width > 0 && desc.Height > 0) {
            D3D11_TEXTURE2D_DESC staging_desc = desc;
            staging_desc.BindFlags = 0;
            staging_desc.MiscFlags = 0;
            staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            staging_desc.Usage = D3D11_USAGE_STAGING;
            staging_desc.ArraySize = 1;
            staging_desc.MipLevels = 1;
            staging_desc.SampleDesc.Count = 1;
            staging_desc.SampleDesc.Quality = 0;

            ID3D11Texture2D* staging = nullptr;
            hr = m_state->device->CreateTexture2D(&staging_desc, nullptr, &staging);
            if (SUCCEEDED(hr) && staging) {
                m_state->context->CopyResource(staging, desktop_tex);
                D3D11_MAPPED_SUBRESOURCE mapped{};
                hr = m_state->context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped);
                if (SUCCEEDED(hr) && mapped.pData) {
                    // Touch first and last scanline so the map is not optimized away.
                    volatile const uint8_t* row0 =
                        static_cast<const uint8_t*>(mapped.pData);
                    volatile const uint8_t* rowN =
                        row0 + static_cast<size_t>(desc.Height - 1) * mapped.RowPitch;
                    (void)row0[0];
                    (void)rowN[0];
                    metrics.bytesCopied =
                        static_cast<uint64_t>(desc.Width) *
                        static_cast<uint64_t>(desc.Height) * 4ull;
                    m_pixelDataCaptured = true;
                    m_state->context->Unmap(staging, 0);
                }
                staging->Release();
            }
        }
        desktop_tex->Release();
    } else {
        IDXGISurface* surface = nullptr;
        hr = desktopRes->QueryInterface(__uuidof(IDXGISurface),
                                         reinterpret_cast<void**>(&surface));
        if (SUCCEEDED(hr) && surface) {
            DXGI_MAPPED_RECT mapped{};
            if (SUCCEEDED(surface->Map(&mapped, DXGI_MAP_READ))) {
                DXGI_SURFACE_DESC sdesc{};
                if (SUCCEEDED(surface->GetDesc(&sdesc)) && sdesc.Height > 0) {
                    metrics.bytesCopied =
                        static_cast<uint64_t>(mapped.Pitch) *
                        static_cast<uint64_t>(sdesc.Height);
                } else {
                    metrics.bytesCopied = static_cast<uint64_t>(mapped.Pitch);
                }
                surface->Unmap();
                m_pixelDataCaptured = true;
            }
            surface->Release();
        }
    }

    m_state->dupe->ReleaseFrame();
    desktopRes->Release();

    uint64_t tsc_copy_end = read_timestamp();
    uint64_t copy_delta = (tsc_copy_end > tsc_after_acquire) ? (tsc_copy_end - tsc_after_acquire) : 0;
    metrics.copyNs = copy_delta * 1000000000ULL / tsc_freq;

    // Fuzzed delay between frame iterations (1-5ms) to prevent predictable timing patterns
    {
        static uint64_t rng_state = read_timestamp();
        uint32_t delay_ms = 1 + static_cast<uint32_t>(xor_shift(rng_state) % 5);
        real::win::fuzzed_sleep(static_cast<int32_t>(delay_ms));
    }

    return metrics;
#else
    (void)timeoutMs;
    return std::nullopt;
#endif
}


} // namespace real::gpu::periscope

