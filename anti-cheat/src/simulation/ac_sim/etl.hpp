#pragma once

// Execution Timeline Logger (ETL) — structured ring buffer of subsystem events.
// Every radar subsystem records Init/Frame/MemoryRead/... events for post-run
// verification. Stack-only messages; no heap in the hot path.
//
// CC-ledger CC.1

#include <cstddef>
#include <cstdint>
#include <string>

namespace sim {

/// Fixed message buffer — no heap, no exceptions.
struct EtlMessage {
    char buf[256]{};

    EtlMessage() = default;
    explicit EtlMessage(const char* s) noexcept { copy_from(s); }

    EtlMessage(const EtlMessage& other) noexcept { copy_from(other.buf); }
    EtlMessage& operator=(const EtlMessage& other) noexcept {
        if (this != &other) copy_from(other.buf);
        return *this;
    }
    EtlMessage(EtlMessage&& other) noexcept {
        copy_from(other.buf);
        other.buf[0] = 0;
    }
    EtlMessage& operator=(EtlMessage&& other) noexcept {
        if (this != &other) {
            copy_from(other.buf);
            other.buf[0] = 0;
        }
        return *this;
    }

    const char* c_str() const noexcept { return buf; }
    bool empty() const noexcept { return buf[0] == 0; }

private:
    void copy_from(const char* src) noexcept {
        if (!src) {
            buf[0] = 0;
            return;
        }
        std::size_t i = 0;
        while (i < sizeof(buf) - 1 && src[i]) {
            buf[i] = src[i];
            ++i;
        }
        buf[i] = 0;
    }
};

enum class EtlEventType : std::uint8_t {
    Init = 0,
    Frame,
    MemoryRead,
    PatternScan,
    EntityCollect,
    HudSnapshot,
    CvarRead,
    RadarRender,
    EvasionTick,
    HealAction,
    OverlayPresent,
    Error,
    Shutdown,
    Count
};

struct EtlEvent {
    EtlEventType type{EtlEventType::Init};
    std::uint64_t timestampNs{0};
    std::uint32_t frameIndex{0};
    std::uint32_t pid{0};
    std::uint64_t address{0};
    std::size_t size{0};
    std::int64_t value{0};
    EtlMessage message;
};

class ExecutionTimeline {
public:
    static constexpr std::size_t kMaxEvents = 2048;

    void record(EtlEvent event) noexcept;

    /// Convenience: fill timestamp + common fields.
    void record(EtlEventType type, const char* msg,
                std::uint32_t frame = 0, std::uint32_t pid = 0,
                std::uint64_t address = 0, std::size_t size = 0,
                std::int64_t value = 0) noexcept;

    void record(EtlEventType type, const std::string& msg,
                std::uint32_t frame = 0, std::uint32_t pid = 0,
                std::uint64_t address = 0, std::size_t size = 0,
                std::int64_t value = 0) noexcept {
        record(type, msg.c_str(), frame, pid, address, size, value);
    }

    void flush_to_file(const char* path) noexcept;

    bool verify_min_frames(int count) const noexcept;
    bool verify_no_errors() const noexcept;
    bool verify_has_type(EtlEventType type) const noexcept;
    int count_type(EtlEventType type) const noexcept;

    std::size_t size() const noexcept { return m_count; }
    std::size_t capacity() const noexcept { return kMaxEvents; }

    /// Chronological access (oldest first). Returns nullptr if idx out of range.
    const EtlEvent* at(std::size_t idx) const noexcept;

    static ExecutionTimeline& instance() noexcept;

    void clear() noexcept;
    void shutdown() noexcept;

    /// Replayable logical nanoseconds, independent of the host clock.
    std::uint64_t now_ns() noexcept;

private:
    EtlEvent m_events[kMaxEvents]{};
    std::size_t m_count = 0;
    std::size_t m_head = 0; // next write index
    std::uint64_t m_logicalNs = 0;
};

// In release builds, ETL_RECORD compiles to nothing (no I/O surface).
#ifndef NDEBUG
#define ETL_RECORD(type, msg)                                                  \
    do {                                                                       \
        sim::ExecutionTimeline::instance().record((type), (msg));              \
    } while (false)
#define ETL_RECORD_FULL(type, msg, frame, pid, addr, sz, val)                  \
    do {                                                                       \
        sim::ExecutionTimeline::instance().record(                             \
            (type), (msg), (frame), (pid), (addr), (sz), (val));               \
    } while (false)
#else
#define ETL_RECORD(type, msg) ((void)0)
#define ETL_RECORD_FULL(type, msg, frame, pid, addr, sz, val) ((void)0)
#endif

} // namespace sim
