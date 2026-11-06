#include "ac_sim/etl.hpp"

#include <cstdio>

namespace sim {
namespace {

const char* type_name(EtlEventType t) noexcept {
    switch (t) {
    case EtlEventType::Init:           return "Init";
    case EtlEventType::Frame:          return "Frame";
    case EtlEventType::MemoryRead:     return "MemoryRead";
    case EtlEventType::PatternScan:    return "PatternScan";
    case EtlEventType::EntityCollect:  return "EntityCollect";
    case EtlEventType::HudSnapshot:    return "HudSnapshot";
    case EtlEventType::CvarRead:       return "CvarRead";
    case EtlEventType::RadarRender:    return "RadarRender";
    case EtlEventType::EvasionTick:    return "EvasionTick";
    case EtlEventType::HealAction:     return "HealAction";
    case EtlEventType::OverlayPresent: return "OverlayPresent";
    case EtlEventType::Error:          return "Error";
    case EtlEventType::Shutdown:       return "Shutdown";
    default:                           return "Unknown";
    }
}

std::size_t chronological_index(std::size_t logical,
                                std::size_t count,
                                std::size_t head) noexcept {
    // Ring: when not full, events occupy [0, count). When full, head is the
    // oldest overwrite position, so oldest = head.
    if (count < ExecutionTimeline::kMaxEvents) {
        return logical;
    }
    return (head + logical) % ExecutionTimeline::kMaxEvents;
}

} // namespace

std::uint64_t ExecutionTimeline::now_ns() noexcept {
    // Logical time is replayable.  Host clocks belong at real composition
    // boundaries and are deliberately unavailable to the simulation graph.
    m_logicalNs += 1'000'000;
    return m_logicalNs;
}

void ExecutionTimeline::record(EtlEvent event) noexcept {
    if (event.timestampNs == 0) {
        event.timestampNs = now_ns();
    }
    m_events[m_head] = event;
    m_head = (m_head + 1) % kMaxEvents;
    if (m_count < kMaxEvents) {
        ++m_count;
    }
}

void ExecutionTimeline::record(EtlEventType type, const char* msg,
                               std::uint32_t frame, std::uint32_t pid,
                               std::uint64_t address, std::size_t size,
                               std::int64_t value) noexcept {
    EtlEvent e{};
    e.type = type;
    e.timestampNs = now_ns();
    e.frameIndex = frame;
    e.pid = pid;
    e.address = address;
    e.size = size;
    e.value = value;
    e.message = EtlMessage(msg ? msg : "");
    record(e);
}

void ExecutionTimeline::flush_to_file(const char* path) noexcept {
#ifndef NDEBUG
    if (!path || !path[0]) return;
    FILE* f = std::fopen(path, "w");
    if (!f) return;
    std::fprintf(f, "type,type_name,timestamp_ns,frame,pid,address,size,value,message\n");
    for (std::size_t i = 0; i < m_count; ++i) {
        const EtlEvent* e = at(i);
        if (!e) continue;
        std::fprintf(f, "%d,%s,%llu,%u,%u,0x%llx,%zu,%lld,%s\n",
                     static_cast<int>(e->type),
                     type_name(e->type),
                     static_cast<unsigned long long>(e->timestampNs),
                     e->frameIndex,
                     e->pid,
                     static_cast<unsigned long long>(e->address),
                     e->size,
                     static_cast<long long>(e->value),
                     e->message.c_str());
    }
    std::fclose(f);
#else
    (void)path;
#endif
}

bool ExecutionTimeline::verify_min_frames(int count) const noexcept {
    return count_type(EtlEventType::Frame) >= count;
}

bool ExecutionTimeline::verify_no_errors() const noexcept {
    return count_type(EtlEventType::Error) == 0;
}

bool ExecutionTimeline::verify_has_type(EtlEventType type) const noexcept {
    return count_type(type) > 0;
}

int ExecutionTimeline::count_type(EtlEventType type) const noexcept {
    int n = 0;
    for (std::size_t i = 0; i < m_count; ++i) {
        const std::size_t idx = chronological_index(i, m_count, m_head);
        if (m_events[idx].type == type) ++n;
    }
    return n;
}

const EtlEvent* ExecutionTimeline::at(std::size_t idx) const noexcept {
    if (idx >= m_count) return nullptr;
    return &m_events[chronological_index(idx, m_count, m_head)];
}

ExecutionTimeline& ExecutionTimeline::instance() noexcept {
    static ExecutionTimeline timeline;
    return timeline;
}

void ExecutionTimeline::clear() noexcept {
    m_count = 0;
    m_head = 0;
    m_logicalNs = 0;
}

void ExecutionTimeline::shutdown() noexcept {
    record(EtlEventType::Shutdown, "timeline shutdown");
    // Keep events for post-run analysis; only reset counters if caller wants wipe via clear().
}

} // namespace sim
