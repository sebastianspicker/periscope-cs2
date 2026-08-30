#pragma once

// Ops graph fusion: account + HWID + VPN + C2 + payment → seller/cluster risk.
// One correlator over ≥3 signal families — not six lone flags.

#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace depth {

// Lab type `SellerSignalSnapshot` used by this educational unit.
struct SellerSignalSnapshot {
  int linked_account_pairs = 0;   // shared hwid or payment
  int shared_hwid_clusters = 0;
  int shared_payment_clusters = 0;
  int vpn_proxy_accounts = 0;
  int offset_c2_flows = 0;
  int radar_saas_flows = 0;
  int high_report_accounts = 0;
  int unique_build_ids = 0;
};

enum class ClusterAction : std::uint8_t {
  None = 0,
  Watchlist,
  SoftRestrictCluster,
  BanClusterCandidate,
};

// Aggregate outcome fields for `SellerFusionResult` (lab narrative / tests).
struct SellerFusionResult {
  SellerSignalSnapshot snap{};
  int families_hit = 0;  // how many of the 5 families fired
  double risk = 0;
  ClusterAction action = ClusterAction::None;
  bool cluster_detected = false;
  std::vector<std::string> reasons;
  std::string detail;
};

// Lab type `SellerFusionCorrelator` used by this educational unit.
class SellerFusionCorrelator {
 public:
  void set_watch_threshold(double t) { watch_th_ = t; }
  void set_restrict_threshold(double t) { restrict_th_ = t; }
  void set_ban_threshold(double t) { ban_th_ = t; }

  SellerSignalSnapshot snapshot(const sim::World& w) const;
  SellerFusionResult evaluate(const sim::World& w) const;

  /// Plant multi-family seller cluster residual on World (lab red ops path).
  static void plant_seller_cluster(sim::World& w);

 private:
  double watch_th_ = 3.0;
  double restrict_th_ = 6.0;
  double ban_th_ = 10.0;
};

}  // namespace depth
