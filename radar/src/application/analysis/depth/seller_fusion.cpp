// seller_fusion.cpp — depth analytics: multi-sample / fusion scoring for blue.
// Educational scorers over World residuals.

#include "depth/seller_fusion.hpp"

#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace depth {

// SellerFusionCorrelator::snapshot: Snapshot current fusion cluster state.
SellerSignalSnapshot SellerFusionCorrelator::snapshot(
    const sim::World& w) const {
  SellerSignalSnapshot s;

  // Account / HWID / payment linkage
  std::unordered_map<std::string, int> hwid_count;
  std::unordered_map<std::string, int> pay_count;
  for (const auto& a : w.accounts) {
    if (!a.hwid.empty()) {
      ++hwid_count[a.hwid];
    }
    if (!a.payment_fp.empty()) {
      ++pay_count[a.payment_fp];
    }
    if (a.reports >= 3) {
      ++s.high_report_accounts;
    }
  }
  for (const auto& [_, c] : hwid_count) {
    if (c >= 2) {
      ++s.shared_hwid_clusters;
    }
  }
  for (const auto& [_, c] : pay_count) {
    if (c >= 2) {
      ++s.shared_payment_clusters;
    }
  }
  for (std::size_t i = 0; i < w.accounts.size(); ++i) {
    for (std::size_t j = i + 1; j < w.accounts.size(); ++j) {
      if (w.accounts[i].hwid == w.accounts[j].hwid ||
          w.accounts[i].payment_fp == w.accounts[j].payment_fp) {
        ++s.linked_account_pairs;
      }
    }
  }

  // VPN family
  if (w.vpn_proxy_active) {
    s.vpn_proxy_accounts = static_cast<int>(std::max<std::size_t>(
        1, w.accounts.empty() ? 1 : w.accounts.size() / 2));
  }

  // C2 / SaaS network family
  for (const auto& f : w.net) {
    if (f.looks_like_offset_c2) {
      ++s.offset_c2_flows;
    }
    if (f.looks_like_radar_saas) {
      ++s.radar_saas_flows;
    }
  }

  // Unique build family (polymorphic seller builds)
  std::unordered_set<std::string> builds;
  if (!w.binary_build_id.empty() && w.binary_build_id != "shared") {
    builds.insert(w.binary_build_id);
  }
  if (!w.build_watermark.empty()) {
    builds.insert(w.build_watermark);
  }
  s.unique_build_ids = static_cast<int>(builds.size());

  return s;
}

// SellerFusionCorrelator::evaluate: Syscall-aware monitor: correlate handles with syscall/soft signals.
SellerFusionResult SellerFusionCorrelator::evaluate(const sim::World& w) const {
  SellerFusionResult r;
  r.snap = snapshot(w);

  // Family 1: account/HWID/payment graph
  if (r.snap.linked_account_pairs > 0 || r.snap.shared_hwid_clusters > 0 ||
      r.snap.shared_payment_clusters > 0) {
    ++r.families_hit;
    r.risk += 2.0 * r.snap.linked_account_pairs +
              2.5 * r.snap.shared_hwid_clusters +
              2.5 * r.snap.shared_payment_clusters;
    r.reasons.push_back("account_graph");
  }

  // Family 2: VPN/proxy
  if (r.snap.vpn_proxy_accounts > 0) {
    ++r.families_hit;
    r.risk += 1.5 * r.snap.vpn_proxy_accounts;
    r.reasons.push_back("vpn_proxy");
  }

  // Family 3: C2 / radar SaaS
  if (r.snap.offset_c2_flows > 0 || r.snap.radar_saas_flows > 0) {
    ++r.families_hit;
    r.risk += 2.0 * r.snap.offset_c2_flows + 1.5 * r.snap.radar_saas_flows;
    r.reasons.push_back("c2_or_saas");
  }

  // Family 4: report velocity
  if (r.snap.high_report_accounts > 0) {
    ++r.families_hit;
    r.risk += 1.0 * r.snap.high_report_accounts;
    r.reasons.push_back("report_velocity");
  }

  // Family 5: polymorphic / watermark builds
  if (r.snap.unique_build_ids > 0) {
    ++r.families_hit;
    r.risk += 1.0 * r.snap.unique_build_ids;
    r.reasons.push_back("unique_build");
  }

  // Fusion requires ≥3 families for cluster_detected (plan acceptance).
  r.cluster_detected = r.families_hit >= 3;

  if (r.risk >= ban_th_ && r.cluster_detected) {
    r.action = ClusterAction::BanClusterCandidate;
  } else if (r.risk >= restrict_th_ && r.families_hit >= 2) {
    r.action = ClusterAction::SoftRestrictCluster;
  } else if (r.risk >= watch_th_ || r.families_hit >= 2) {
    r.action = ClusterAction::Watchlist;
  } else {
    r.action = ClusterAction::None;
  }

  std::ostringstream oss;
  oss << "families=" << r.families_hit << " risk=" << r.risk
      << " linked_pairs=" << r.snap.linked_account_pairs
      << " hwid_c=" << r.snap.shared_hwid_clusters
      << " pay_c=" << r.snap.shared_payment_clusters
      << " vpn=" << r.snap.vpn_proxy_accounts
      << " c2=" << r.snap.offset_c2_flows
      << " saas=" << r.snap.radar_saas_flows
      << " reports=" << r.snap.high_report_accounts
      << " builds=" << r.snap.unique_build_ids
      << " action=" << static_cast<int>(r.action)
      << " cluster=" << (r.cluster_detected ? 1 : 0);
  r.detail = oss.str();
  return r;
}

// SellerFusionCorrelator::plant_seller_cluster: Plant multi-account seller cluster for fusion demos.
void SellerFusionCorrelator::plant_seller_cluster(sim::World& w) {
  // Multi-family seller residual (≥3 required for cluster_detected):
  //  1) account/HWID/payment graph — shared HWID + shared payment across buyers
  //  2) VPN/proxy fabric on the cluster
  //  3) offset-C2 + radar SaaS network flows
  //  4) high report velocity on buyer accounts
  //  5) polymorphic build IDs / watermarks (unique per drop)
  w.add_account(sim::Account{"acct-a", "HWID-SELLER-1", "pay-stripe-99",
                             "net-10", 5});
  w.add_account(sim::Account{"acct-b", "HWID-SELLER-1", "pay-stripe-99",
                             "net-11", 4});
  w.add_account(sim::Account{"acct-c", "HWID-OTHER", "pay-stripe-99", "net-12",
                             3});
  w.vpn_proxy_active = true;
  w.add_net(sim::NetFlow{0, "offsets.seller.example:443", true, false});
  w.add_net(sim::NetFlow{0, "radar-saas.example:443", false, true});
  w.binary_build_id = "buyer_fusion_42";
  w.build_watermark = "wm-seller-cluster";
  w.note("plant_seller_cluster families>=3");
}

}  // namespace depth
