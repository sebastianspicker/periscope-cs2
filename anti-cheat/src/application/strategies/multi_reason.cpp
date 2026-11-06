#include "strategies/multi_reason.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace strategies {

MultiReasonDetector::MultiReasonDetector(const char* strategy_id)
    : id_(strategy_id ? strategy_id : "pair") {}

MultiReasonDetector& MultiReasonDetector::set_detect_min_signals(int n) {
  detect_min_signals_ = n;
  return *this;
}

MultiReasonDetector& MultiReasonDetector::set_mitigate_min_signals(int n) {
  mitigate_min_signals_ = n;
  return *this;
}

MultiReasonDetector& MultiReasonDetector::set_detect_min_risk(double r) {
  detect_min_risk_ = r;
  return *this;
}

MultiReasonDetector& MultiReasonDetector::set_mitigate_min_risk(double r) {
  mitigate_min_risk_ = r;
  return *this;
}

MultiReasonDetector& MultiReasonDetector::set_require_specific_scar(bool v) {
  require_specific_ = v;
  return *this;
}

MultiReasonDetector& MultiReasonDetector::signal(bool present, std::string reason,
                                                 double weight) {
  if (!present) return *this;
  SignalHit hit;
  hit.present = true;
  hit.reason = std::move(reason);
  hit.weight = weight;
  hits_.push_back(std::move(hit));
  return *this;
}

MultiReasonDetector& MultiReasonDetector::note(std::string reason, double weight) {
  SignalHit hit;
  hit.present = true;
  hit.reason = std::move(reason);
  hit.weight = weight;
  hits_.push_back(std::move(hit));
  return *this;
}

MultiReasonDetector& MultiReasonDetector::specific_scar(bool present,
                                                        std::string reason,
                                                        double weight) {
  if (present) {
    specific_scar_ = true;
    if (!reason.empty()) {
      signal(true, std::move(reason), weight);
    }
  }
  return *this;
}

MultiReasonDetector& MultiReasonDetector::deny_ranked_on_mitigate(bool v) {
  deny_ranked_ = v;
  return *this;
}

int MultiReasonDetector::signal_count() const {
  int n = 0;
  for (const auto& h : hits_) {
    if (h.present) ++n;
  }
  return n;
}

double MultiReasonDetector::risk() const {
  double r = 0.0;
  for (const auto& h : hits_) {
    if (h.present) r += h.weight;
  }
  return std::min(1.0, r);
}

BlueOutcome MultiReasonDetector::finish(sim::World* w) const {
  BlueOutcome out;
  out.signals = signal_count();
  out.risk = risk();
  out.reasons.reserve(hits_.size());
  for (const auto& h : hits_) {
    if (h.present) out.reasons.push_back(h.reason);
  }

  const bool scar_ok = !require_specific_ || specific_scar_;
  out.detected = scar_ok && out.signals >= detect_min_signals_ &&
                 out.risk >= detect_min_risk_;
  out.mitigated = out.detected && (out.signals >= mitigate_min_signals_ ||
                                   out.risk >= mitigate_min_risk_);

  if (out.mitigated && deny_ranked_ && w != nullptr) {
    w->ranked_access_denied = true;
  }

  out.detail = std::string(id_) + " signals=" + std::to_string(out.signals) +
               " risk=" + std::to_string(out.risk) +
               " detected=" + (out.detected ? "1" : "0") +
               " mitigated=" + (out.mitigated ? "1" : "0");

  std::printf("[multi_reason:%s] signals=%d risk=%.2f detected=%s mitigated=%s\n",
              id_, out.signals, out.risk, out.detected ? "true" : "false",
              out.mitigated ? "true" : "false");
  for (const auto& r : out.reasons) {
    std::printf("[multi_reason:%s]   %s\n", id_, r.c_str());
  }
  return out;
}

double risk_for_signals(int signals, double base, double step) {
  if (signals <= 0) return 0.0;
  return std::min(1.0, base + static_cast<double>(signals) * step);
}

void add_signal(int& signals, std::vector<std::string>& reasons, bool present,
                const char* reason) {
  if (!present || reason == nullptr) return;
  ++signals;
  reasons.emplace_back(reason);
}

void add_weighted_signal(BlueOutcome& out, bool present, std::string reason,
                         double weight) {
  if (!present) return;
  ++out.signals;
  out.risk = std::min(1.0, out.risk + weight);
  out.reasons.push_back(std::move(reason));
}

}  // namespace strategies
