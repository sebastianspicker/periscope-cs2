#pragma once

// Decoy render engine — synthesizes plausible UI primitives (menus, health
// bars, chat boxes, FPS counters) to dilute VACnet-style ML overlay features.
//
// CC-ledger CC.4

#include "ac/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace sim {

struct DecoyOp {
    enum Type : std::uint8_t {
        FilledRect = 0,
        Text,
        Circle,
        Line,
        Gradient,
        HealthBar,
        ChatBox,
        MenuPanel,
        FpsCounter
    };
    Type type{FilledRect};
    float x{0}, y{0}, w{0}, h{0};
    std::uint32_t color{0};
    std::string text;
    float alpha{1.0f};
};

struct DecoyRenderParams {
    int decoysPerFrame{4};
    float alphaJitter{0.08f};   // max |delta| on alpha (0–18/255 ≈ 0.07)
    float posJitter{5.0f};      // pixel scatter
    bool preferLegitCorners{true};
};

class DecoyRenderEngine {
public:
    void initialize(std::uint64_t seed) noexcept;
    void set_params(int decoysPerFrame, float alphaJitter, float posJitter) noexcept;
    void set_params(const DecoyRenderParams& p) noexcept;

    /// Generate decoy draw list for one frame. localOrigin biases placement
    /// toward on-screen HUD regions relative to the local player.
    std::vector<DecoyOp> generate(int displayW, int displayH,
                                  const ac::Vec3& localOrigin) noexcept;

    std::uint64_t seed() const noexcept { return m_seed; }
    int decoys_per_frame() const noexcept { return m_params.decoysPerFrame; }

private:
    std::uint64_t m_seed{1};
    DecoyRenderParams m_params{};

    std::uint64_t next_u64() noexcept;
    float next_float() noexcept;
    std::uint32_t plausible_color() noexcept;
    float jitter_float(float base, float range) noexcept;
    DecoyOp make_legit_element(int displayW, int displayH, int kind) noexcept;
};

} // namespace sim
