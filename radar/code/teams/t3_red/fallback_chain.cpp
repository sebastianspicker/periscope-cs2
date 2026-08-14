// fallback_chain.cpp — multi-step T3→T2→T0 backend fallback attach.
// Key methods: add, attach_first_available, attach_with_report, active_tier.

#include "t3_red/fallback_chain.hpp"

#include <sstream>

namespace t3_red {

namespace {

const char* status_label(ac::Status st) {
  switch (st) {
    case ac::Status::Ok:
      return "ok";
    case ac::Status::NotImplemented:
      return "not_implemented";
    case ac::Status::Denied:
      return "denied";
    case ac::Status::Unavailable:
      return "unavailable";
    case ac::Status::InvalidArgument:
      return "invalid";
    case ac::Status::LabOnly:
      return "lab_only";
    default:
      return "error";
  }
}

}  // namespace

// FallbackChain::add: Register a tier fallback step in the chain.
void FallbackChain::add(std::unique_ptr<ac::IMemoryBackend> backend) {
  backends_.push_back(std::move(backend));
}

// FallbackChain::attach_with_report: Ordered multi-step try until one succeeds.
FallbackAttachReport FallbackChain::attach_with_report(std::uint32_t target_id) {
  last_ = {};
  active_ = nullptr;
  attempts_ = 0;
  failed_steps_ = 0;

  for (auto& b : backends_) {
    ++attempts_;
    last_.attempts = attempts_;
    const auto st = b->attach(target_id);
    std::ostringstream step;
    step << "step=" << attempts_ << " backend=" << b->name()
         << " tier=" << static_cast<int>(b->tier())
         << " status=" << status_label(st);
    last_.attempt_log.push_back(step.str());

    if (st == ac::Status::Ok) {
      active_ = b.get();
      last_.ok = true;
      last_.active_tier = b->tier();
      last_.active_name = std::string(b->name());
      last_.failed_steps = failed_steps_;
      std::ostringstream detail;
      detail << "fallback_ok active=" << last_.active_name
             << " tier=" << static_cast<int>(last_.active_tier)
             << " attempts=" << attempts_
             << " failed=" << failed_steps_;
      last_.detail = detail.str();
      return last_;
    }

    ++failed_steps_;
    last_.failed_steps = failed_steps_;
  }

  last_.ok = false;
  last_.active_tier = ac::Tier::T0_UsermodeRpm;
  last_.detail = "fallback_exhausted attempts=" + std::to_string(attempts_) +
                 " failed=" + std::to_string(failed_steps_);
  return last_;
}

// FallbackChain::attach_first_available: Try chain steps until one attach succeeds.
ac::Status FallbackChain::attach_first_available(std::uint32_t target_id) {
  auto rep = attach_with_report(target_id);
  return rep.ok ? ac::Status::Ok : ac::Status::Unavailable;
}

// FallbackChain::active_tier: Which delivery tier is currently active after attach.
ac::Tier FallbackChain::active_tier() const {
  return active_ ? active_->tier() : ac::Tier::T0_UsermodeRpm;
}

}  // namespace t3_red
