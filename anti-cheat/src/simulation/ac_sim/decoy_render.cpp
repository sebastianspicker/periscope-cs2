#include "ac_sim/decoy_render.hpp"

#include "ac_sim/xorshift.hpp"

#include <cmath>
#include <cstdio>

namespace sim {
namespace {

// Legitimate OSD corner templates (RTSS / Afterburner / Steam-like).
struct CornerSlot {
    float xFrac, yFrac, w, h;
};

constexpr CornerSlot kSlots[] = {
    {0.01f, 0.01f, 120.f, 18.f},  // top-left FPS
    {0.01f, 0.92f, 180.f, 48.f},  // bottom-left chat
    {0.75f, 0.02f, 200.f, 80.f},  // top-right menu
    {0.40f, 0.88f, 220.f, 14.f},  // bottom health
    {0.85f, 0.85f, 64.f, 64.f},   // minimap-ish
};

const char* kChatLines[] = {
    "gg", "nice", "wp", "nt", "glhf", "1", "?", "lol", "ez", "afk",
};

} // namespace

void DecoyRenderEngine::initialize(std::uint64_t seed) noexcept {
    m_seed = seed ? seed : 0xDEC0u;
}

void DecoyRenderEngine::set_params(int decoysPerFrame, float alphaJitter,
                                    float posJitter) noexcept {
    m_params.decoysPerFrame = decoysPerFrame < 0 ? 0 : decoysPerFrame;
    if (m_params.decoysPerFrame > 32) m_params.decoysPerFrame = 32;
    m_params.alphaJitter = alphaJitter;
    m_params.posJitter = posJitter;
}

void DecoyRenderEngine::set_params(const DecoyRenderParams& p) noexcept {
    m_params = p;
    if (m_params.decoysPerFrame < 0) m_params.decoysPerFrame = 0;
    if (m_params.decoysPerFrame > 32) m_params.decoysPerFrame = 32;
}

std::uint64_t DecoyRenderEngine::next_u64() noexcept {
    if (XorShiftPool::seeded()) {
        return XorShiftPool::instance(XorShiftInstance::Render).next();
    }
    m_seed = m_seed * 6364136223846793005ULL + 1442695040888963407ULL;
    return m_seed;
}

float DecoyRenderEngine::next_float() noexcept {
    return static_cast<float>((next_u64() >> 40) & 0xFFFFFFu) / 16777216.0f;
}

float DecoyRenderEngine::jitter_float(float base, float range) noexcept {
    return base + (next_float() - 0.5f) * 2.0f * range;
}

std::uint32_t DecoyRenderEngine::plausible_color() noexcept {
    // Muted HUD palette — greens/cyans/grays, not neon ESP colors.
    static const std::uint32_t kPalette[] = {
        0xC0A0A0A0u, // gray
        0xC020C020u, // green
        0xC020A0C0u, // cyan
        0xC0C0C020u, // yellow
        0xC0E0E0E0u, // white-ish
        0xB0808080u, // dim gray
    };
    return kPalette[next_u64() % (sizeof(kPalette) / sizeof(kPalette[0]))];
}

DecoyOp DecoyRenderEngine::make_legit_element(int displayW, int displayH,
                                              int kind) noexcept {
    DecoyOp op{};
    const CornerSlot& slot = kSlots[static_cast<std::size_t>(kind) %
                                    (sizeof(kSlots) / sizeof(kSlots[0]))];

    op.x = jitter_float(slot.xFrac * static_cast<float>(displayW), m_params.posJitter);
    op.y = jitter_float(slot.yFrac * static_cast<float>(displayH), m_params.posJitter);
    op.w = jitter_float(slot.w, m_params.posJitter * 0.5f);
    op.h = jitter_float(slot.h, m_params.posJitter * 0.3f);
    op.color = plausible_color();
    op.alpha = jitter_float(0.75f, m_params.alphaJitter);
    if (op.alpha < 0.15f) op.alpha = 0.15f;
    if (op.alpha > 1.0f) op.alpha = 1.0f;

    switch (kind % 5) {
    case 0:
        op.type = DecoyOp::FpsCounter;
        {
            const int fps = 60 + static_cast<int>(next_u64() % 80); // 60–139
            char buf[32];
            std::snprintf(buf, sizeof(buf), "FPS: %d", fps);
            op.text = buf;
        }
        break;
    case 1:
        op.type = DecoyOp::HealthBar;
        op.w = jitter_float(200.f, 20.f);
        op.h = jitter_float(12.f, 3.f);
        break;
    case 2:
        op.type = DecoyOp::ChatBox;
        op.text = kChatLines[next_u64() % (sizeof(kChatLines) / sizeof(kChatLines[0]))];
        break;
    case 3:
        op.type = DecoyOp::MenuPanel;
        op.w = jitter_float(180.f, 30.f);
        op.h = jitter_float(90.f, 20.f);
        break;
    default:
        op.type = DecoyOp::Text;
        {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%u%%",
                          static_cast<unsigned>(next_u64() % 100));
            op.text = buf;
        }
        break;
    }

    // Clamp into display.
    if (op.x < 0) op.x = 0;
    if (op.y < 0) op.y = 0;
    if (op.x + op.w > static_cast<float>(displayW))
        op.x = static_cast<float>(displayW) - op.w;
    if (op.y + op.h > static_cast<float>(displayH))
        op.y = static_cast<float>(displayH) - op.h;

    (void)displayH;
    return op;
}

std::vector<DecoyOp> DecoyRenderEngine::generate(int displayW, int displayH,
                                                 const ac::Vec3& localOrigin) noexcept {
    (void)localOrigin;
    std::vector<DecoyOp> ops;
    if (displayW <= 0 || displayH <= 0 || m_params.decoysPerFrame <= 0) {
        return ops;
    }

    ops.reserve(static_cast<std::size_t>(m_params.decoysPerFrame));

    for (int i = 0; i < m_params.decoysPerFrame; ++i) {
        if (m_params.preferLegitCorners) {
            ops.push_back(make_legit_element(displayW, displayH, i + static_cast<int>(next_u64() & 7)));
            continue;
        }

        DecoyOp op{};
        const auto roll = next_u64() % 5;
        switch (roll) {
        case 0: op.type = DecoyOp::FilledRect; break;
        case 1: op.type = DecoyOp::Text; break;
        case 2: op.type = DecoyOp::Circle; break;
        case 3: op.type = DecoyOp::Line; break;
        default: op.type = DecoyOp::Gradient; break;
        }
        op.x = next_float() * static_cast<float>(displayW);
        op.y = next_float() * static_cast<float>(displayH);
        op.w = jitter_float(40.f, 10.f);
        op.h = jitter_float(20.f, 5.f);
        op.color = plausible_color();
        op.alpha = jitter_float(0.7f, m_params.alphaJitter);
        if (op.alpha < 0.1f) op.alpha = 0.1f;
        if (op.alpha > 1.0f) op.alpha = 1.0f;
        if (op.type == DecoyOp::Text) {
            op.text = std::to_string(next_u64() % 100);
        }
        ops.push_back(op);
    }
    return ops;
}

} // namespace sim
