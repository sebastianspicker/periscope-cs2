#include "cs2/diagnostic_sensors.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace cs2::sensors {
namespace {

constexpr int kPollingCallThreshold = 50;
constexpr double kConsistentIntervalStdDev = 2.0;
constexpr int kUnsolicitedMessageThreshold = 10;
constexpr int kMessageRateThreshold = 10;

}  // namespace

CallGraphSensor::CallGraphSensor(sim::World& world) : world_(world) {}

CallGraphReport CallGraphSensor::analyze() {
  CallGraphReport report{};
  std::unordered_map<std::string, std::size_t> edge_indices;
  std::unordered_set<std::string> callers;
  std::unordered_set<std::string> callees;

  for (const auto& call : world_.diagnostic_state.call_records) {
    ++report.total_calls;
    callers.insert(call.from_function);
    callees.insert(call.to_function);
    const std::string key = call.from_function + '\n' + call.to_function;
    const auto [it, inserted] = edge_indices.emplace(key, report.edges.size());
    if (inserted) {
      report.edges.push_back({call.from_function, call.to_function, call.tick, 1});
    } else {
      ++report.edges[it->second].call_count;
    }
  }
  report.unique_callers = static_cast<int>(callers.size());
  report.unique_callees = static_cast<int>(callees.size());

  const auto& calls = world_.diagnostic_state.call_records;
  for (std::size_t i = 2; i < calls.size(); ++i) {
    if (calls[i - 2].to_function == "GETENTITY" &&
        calls[i - 1].to_function == "GETPLAYER" &&
        calls[i].to_function == "GETBONE") {
      report.detected_patterns.push_back("radar read sequence");
      report.anomalous_sequence = true;
      break;
    }
  }

  const auto& instrumentation = world_.diagnostic_state.instrumentation;
  if (instrumentation.active &&
      (instrumentation.tracking_ptr1 != 0 || instrumentation.tracking_ptr2 != 0 ||
       !instrumentation.nodes.empty())) {
    report.detected_patterns.push_back("active diagnostic instrumentation");
  }

  report.detail = "calls=" + std::to_string(report.total_calls) +
                  " edges=" + std::to_string(report.edges.size());
  world_.note(report.detail);
  return report;
}

PatternFrequencySensor::PatternFrequencySensor(sim::World& world) : world_(world) {}

FrequencyReport PatternFrequencySensor::analyze() {
  FrequencyReport report{};
  std::unordered_map<std::string, std::vector<std::uint64_t>> ticks_by_function;
  std::unordered_map<std::string, std::uint64_t> last_tick;

  for (const auto& call : world_.diagnostic_state.call_records) {
    const auto found = last_tick.find(call.to_function);
    const std::uint64_t delta = found == last_tick.end() || call.tick < found->second
        ? 0
        : call.tick - found->second;
    report.calls.push_back({call.to_function, call.tick, delta});
    last_tick[call.to_function] = call.tick;
    ticks_by_function[call.to_function].push_back(call.tick);
  }

  for (const auto& [name, ticks] : ticks_by_function) {
    FrequencyReport::FunctionStats stats{};
    stats.name = name;
    stats.call_count = static_cast<int>(ticks.size());
    std::vector<double> deltas;
    for (std::size_t i = 1; i < ticks.size(); ++i) {
      if (ticks[i] >= ticks[i - 1]) deltas.push_back(static_cast<double>(ticks[i] - ticks[i - 1]));
    }
    if (!deltas.empty()) {
      for (double delta : deltas) stats.avg_delta += delta;
      stats.avg_delta /= deltas.size();
      for (double delta : deltas) {
        const double difference = delta - stats.avg_delta;
        stats.std_dev_delta += difference * difference;
      }
      stats.std_dev_delta = std::sqrt(stats.std_dev_delta / deltas.size());
    }
    stats.consistent_interval = deltas.size() >= 2 && stats.std_dev_delta < kConsistentIntervalStdDev;
    stats.flagged = stats.consistent_interval && stats.call_count > kPollingCallThreshold;
    report.flagged_functions += stats.flagged;
    report.stats.push_back(std::move(stats));
  }

  report.detail = "functions=" + std::to_string(report.stats.size()) +
                  " flagged=" + std::to_string(report.flagged_functions);
  world_.note(report.detail);
  return report;
}

MessageFrequencySensor::MessageFrequencySensor(sim::World& world) : world_(world) {}

MessageFrequencyReport MessageFrequencySensor::analyze() {
  MessageFrequencyReport report{};
  std::uint64_t first_tick = 0;
  std::uint64_t last_tick = 0;
  for (const auto& message : world_.diagnostic_state.message_records) {
    report.messages.push_back({message.message_id, message.tick, message.server_requested});
    ++report.unsolicited_count;
    if (message.server_requested) --report.unsolicited_count;
    if (report.messages.size() == 1) first_tick = message.tick;
    if (message.tick > last_tick) last_tick = message.tick;
  }
  int recorded_message_rate = 0;
  if (!report.messages.empty()) {
    const std::uint64_t span = last_tick >= first_tick ? last_tick - first_tick + 1 : 1;
    recorded_message_rate = static_cast<int>(report.messages.size() / span);
  }
  const int diagnostic_responses = std::max(0, world_.diagnostic_response_count);
  report.message_rate = std::max(recorded_message_rate, diagnostic_responses);
  report.unsolicited_anomaly = report.unsolicited_count > kUnsolicitedMessageThreshold;
  report.rate_anomalous = report.message_rate > kMessageRateThreshold;
  report.expected_pattern_matched = !report.unsolicited_anomaly && !report.rate_anomalous;
  report.detail = "messages=" + std::to_string(report.messages.size()) +
                  " unsolicited=" + std::to_string(report.unsolicited_count) +
                  " responses=" + std::to_string(diagnostic_responses) +
                  " rate=" + std::to_string(report.message_rate);
  world_.note(report.detail);
  return report;
}

EventListenerSensor::EventListenerSensor(sim::World& world) : world_(world) {}

EventListenerReport EventListenerSensor::analyze() {
  EventListenerReport report{};
  for (const auto& listener : world_.diagnostic_state.event_listeners) {
    report.listeners.push_back({listener.event_name, listener.owning_module,
                                listener.is_game_module, listener.is_cheat_module});
    ++report.total_listeners;
    report.non_game_listeners += !listener.is_game_module;
    report.cheat_listener_detected = report.cheat_listener_detected || listener.is_cheat_module;
  }
  report.cheat_listener_detected = report.cheat_listener_detected || report.non_game_listeners > 0;
  report.detail = "listeners=" + std::to_string(report.total_listeners) +
                  " non-game=" + std::to_string(report.non_game_listeners);
  world_.note(report.detail);
  return report;
}

SubtickAnalysisSensor::SubtickAnalysisSensor(sim::World& world) : world_(world) {}

SubtickAnalysisReport SubtickAnalysisSensor::analyze() {
  SubtickAnalysisReport report{};
  const auto& events = world_.diagnostic_state.counter_strafe_events;
  for (std::size_t i = 0; i < events.size(); ++i) {
    const int delta = i == 0 || events[i].tick < events[i - 1].tick
        ? 0
        : static_cast<int>(events[i].tick - events[i - 1].tick);
    report.deltas.push_back({events[i].action, delta, events[i].keys_pressed});
  }
  report.total_events = static_cast<int>(report.deltas.size());
  if (report.total_events > 1) {
    const auto first_delta = report.deltas.begin() + 1;
    const int delta_count = report.total_events - 1;
    for (auto it = first_delta; it != report.deltas.end(); ++it) report.mean_delta += it->tick_delta;
    report.mean_delta /= delta_count;
    for (auto it = first_delta; it != report.deltas.end(); ++it) {
      const double difference = it->tick_delta - report.mean_delta;
      report.variance_delta += difference * difference;
      report.perfect_strafes += it->tick_delta == 1;
    }
    report.variance_delta /= delta_count;
    report.std_dev_delta = std::sqrt(report.variance_delta);
    report.perfect_ratio = static_cast<double>(report.perfect_strafes) / delta_count;
    report.macro_suspected = report.perfect_ratio > 0.5;
    report.automation_suspected = report.std_dev_delta < 0.1;
    report.human_likelihood = 1.0 - report.perfect_ratio;
  } else {
    report.human_likelihood = 1.0;
  }
  std::ostringstream detail;
  detail << "events=" << report.total_events << " perfect=" << report.perfect_strafes
         << " stddev=" << report.std_dev_delta;
  report.detail = detail.str();
  world_.note(report.detail);
  return report;
}

}  // namespace cs2::sensors
