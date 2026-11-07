// scatter_batch.cpp: R10 scatter-read discipline. See scatter_batch.hpp.

#include "radar/scatter_batch.hpp"

namespace radar_domain {

RevalidationOutcome classify_revalidation(
    const PointerCacheEntry& cached, const PointerRevalidation& fresh) {
  if (!fresh.read_ok) {
    return RevalidationOutcome::ReadFailed;
  }
  if (cached.valid && cached.generation != fresh.generation) {
    return RevalidationOutcome::GenerationGap;
  }
  if (!cached.valid || cached.pointer != fresh.pointer) {
    return RevalidationOutcome::Moved;
  }
  return RevalidationOutcome::Unchanged;
}

std::vector<std::vector<PointerCacheEntry>> plan_scatter_batches(
    const std::vector<PointerCacheEntry>& entries, std::size_t max_in_flight) {
  std::vector<std::vector<PointerCacheEntry>> batches;
  if (max_in_flight == 0 || entries.empty()) {
    return batches;
  }
  batches.reserve(entries.size() / max_in_flight + 1);
  for (std::size_t begin = 0; begin < entries.size(); begin += max_in_flight) {
    const std::size_t end =
        begin + max_in_flight < entries.size() ? begin + max_in_flight
: entries.size();
    batches.emplace_back(entries.begin() + static_cast<std::ptrdiff_t>(begin),
                         entries.begin() + static_cast<std::ptrdiff_t>(end));
  }
  return batches;
}

double DeviceLatencyModel::batch_latency_ms(std::size_t reads) const {
  double latency = base_latency_ms + static_cast<double>(reads) * per_read_ms;
  if (reads > in_flight_capacity) {
    latency += stall_penalty_ms;
  }
  return latency;
}

}  // namespace radar_domain
