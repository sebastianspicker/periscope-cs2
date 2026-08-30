// dns.cpp — DNS tunneling (base32 labels), wire-format query/response,
// UDP resolver, and DNS-over-HTTPS.

#include "real/net/net_client.hpp"
#include "real/net/net_internal.hpp"
#include "real/platform.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#endif

namespace real::net {

// ── Tunnel encode / decode ─────────────────────────────────────────

Result<std::vector<std::string>> encode_dns_tunnel(
    const std::uint8_t* data, std::size_t size, const std::string& domain) {
  if (domain.empty())
    return Result<std::vector<std::string>>({}, "Empty tunnel domain");

  // Base32 is DNS-label safe (A-Z2-7). Each query is: <b32label>.<domain>
  // with at most 63 chars per label and total name <= 253.
  const std::string encoded = (data && size) ? to_base32(data, size) : std::string{};
  std::vector<std::string> chunks;
  if (encoded.empty()) {
    chunks.push_back(std::string("0.") + domain);
    return chunks;
  }

  const std::size_t max_label = 63;
  const std::size_t max_name = 253;
  const std::size_t domain_budget = domain.size() + 1;  // leading dot
  std::size_t max_data_in_name =
      (max_name > domain_budget) ? (max_name - domain_budget) : max_label;
  // One data label per qname keeps decode trivial (first label = payload chunk).
  const std::size_t chunk_len = (std::min)(max_label, max_data_in_name);

  for (std::size_t pos = 0; pos < encoded.size(); pos += chunk_len) {
    const std::size_t take = (std::min)(chunk_len, encoded.size() - pos);
    chunks.push_back(encoded.substr(pos, take) + "." + domain);
  }
  return chunks;
}

Result<std::vector<std::uint8_t>> decode_dns_tunnel(
    const std::vector<std::string>& txt_records) {
  std::string encoded;
  for (const auto& record : txt_records) {
    // Wire form from encode: <base32_or_hex_label>.<domain...>
    // Also accept raw base32/hex strings and multi-label data without domain.
    if (record.empty()) continue;

    // If the record is a full qname, the payload is always the first label
    // (encode_dns_tunnel contract). Remaining labels are the tunnel domain.
    const auto dot = record.find('.');
    std::string part = (dot == std::string::npos) ? record : record.substr(0, dot);

    // Skip empty / marker-only
    if (part == "0" && dot != std::string::npos) continue;

    // Reject obvious non-alphabet junk
    bool ok = !part.empty();
    for (char c : part) {
      const unsigned char u = static_cast<unsigned char>(c);
      const bool alnum =
          (u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') ||
          (u >= '0' && u <= '9');
      if (!alnum) { ok = false; break; }
    }
    if (ok) encoded += part;
  }

  if (encoded.empty())
    return std::vector<std::uint8_t>{};

  // Prefer base32; if fails, try hex (legacy encoder).
  auto b32 = from_base32(encoded);
  if (b32) return b32;
  return from_hex(encoded);
}

// ── Wire format ────────────────────────────────────────────────────

static void dns_write_name(std::vector<std::uint8_t>& out, const std::string& name) {
  std::size_t i = 0;
  while (i < name.size()) {
    if (name[i] == '.') { ++i; continue; }
    const auto dot = name.find('.', i);
    const std::size_t end = (dot == std::string::npos) ? name.size() : dot;
    std::size_t lab_len = end - i;
    if (lab_len > 63) lab_len = 63;
    out.push_back(static_cast<std::uint8_t>(lab_len));
    out.insert(out.end(), name.begin() + static_cast<std::ptrdiff_t>(i),
               name.begin() + static_cast<std::ptrdiff_t>(i + lab_len));
    i = end + (dot == std::string::npos ? 0 : 1);
    if (dot == std::string::npos) break;
  }
  out.push_back(0);
}

static Result<std::string> dns_read_name(const std::uint8_t* data, std::size_t len,
                                          std::size_t& offset, int depth = 0) {
  if (depth > 10) return Result<std::string>({}, "DNS name pointer loop");
  std::string name;
  while (offset < len) {
    const std::uint8_t lab = data[offset];
    if (lab == 0) {
      ++offset;
      break;
    }
    // Compression pointer
    if ((lab & 0xC0) == 0xC0) {
      if (offset + 1 >= len) return Result<std::string>({}, "Truncated DNS pointer");
      const std::size_t ptr =
          static_cast<std::size_t>(((lab & 0x3F) << 8) | data[offset + 1]);
      offset += 2;
      std::size_t tmp = ptr;
      auto rest = dns_read_name(data, len, tmp, depth + 1);
      if (!rest) return rest;
      if (!name.empty()) name.push_back('.');
      name += *rest;
      return name;
    }
    ++offset;
    if (offset + lab > len) return Result<std::string>({}, "Truncated DNS label");
    if (!name.empty()) name.push_back('.');
    name.append(reinterpret_cast<const char*>(data + offset), lab);
    offset += lab;
  }
  return name;
}

Result<std::vector<std::uint8_t>> dns_build_query(
    const std::string& qname, DnsType type, std::uint16_t id) {
  if (qname.empty()) return Result<std::vector<std::uint8_t>>({}, "Empty qname");
  if (id == 0) {
    auto r = random_bytes(2);
    if (r && r->size() == 2) id = read_u16_le(r->data());
    if (id == 0) id = 0xAC12;
  }
  std::vector<std::uint8_t> out;
  out.resize(12);
  write_u16_be(out.data(), id);
  write_u16_be(out.data() + 2, 0x0100);  // RD
  write_u16_be(out.data() + 4, 1);       // QDCOUNT
  write_u16_be(out.data() + 6, 0);
  write_u16_be(out.data() + 8, 0);
  write_u16_be(out.data() + 10, 0);
  dns_write_name(out, qname);
  std::uint8_t tail[4];
  write_u16_be(tail, static_cast<std::uint16_t>(type));
  write_u16_be(tail + 2, 1);  // IN
  out.insert(out.end(), tail, tail + 4);
  return out;
}

Result<std::vector<DnsRecord>> dns_parse_response(
    const std::uint8_t* data, std::size_t len) {
  if (!data || len < 12)
    return Result<std::vector<DnsRecord>>({}, "DNS response too short");

  const std::uint16_t flags = read_u16_be(data + 2);
  if ((flags & 0x000F) != 0) {
    return Result<std::vector<DnsRecord>>(
        {}, "DNS RCODE " + std::to_string(flags & 0xF));
  }
  const std::uint16_t qd = read_u16_be(data + 4);
  const std::uint16_t an = read_u16_be(data + 6);

  std::size_t offset = 12;
  // Skip questions
  for (std::uint16_t i = 0; i < qd; ++i) {
    auto name = dns_read_name(data, len, offset);
    if (!name) return Result<std::vector<DnsRecord>>({}, name.error_msg);
    if (offset + 4 > len)
      return Result<std::vector<DnsRecord>>({}, "Truncated question");
    offset += 4;
  }

  std::vector<DnsRecord> records;
  for (std::uint16_t i = 0; i < an; ++i) {
    auto name = dns_read_name(data, len, offset);
    if (!name) return Result<std::vector<DnsRecord>>({}, name.error_msg);
    if (offset + 10 > len)
      return Result<std::vector<DnsRecord>>({}, "Truncated RR header");
    DnsRecord rec;
    rec.name = *name;
    rec.type = static_cast<DnsType>(read_u16_be(data + offset));
    // class at offset+2
    rec.ttl = read_u32_be(data + offset + 4);
    const std::uint16_t rdlen = read_u16_be(data + offset + 8);
    offset += 10;
    if (offset + rdlen > len)
      return Result<std::vector<DnsRecord>>({}, "Truncated RDATA");

    if (rec.type == DnsType::A && rdlen == 4) {
      char ip[32];
      std::snprintf(ip, sizeof(ip), "%u.%u.%u.%u", data[offset], data[offset + 1],
                    data[offset + 2], data[offset + 3]);
      rec.data = ip;
    } else if (rec.type == DnsType::AAAA && rdlen == 16) {
      // Compact hex form
      rec.data = to_hex(data + offset, 16);
    } else if (rec.type == DnsType::TXT) {
      std::size_t p = 0;
      while (p < rdlen) {
        const std::uint8_t n = data[offset + p];
        ++p;
        if (p + n > rdlen) break;
        rec.data.append(reinterpret_cast<const char*>(data + offset + p), n);
        p += n;
      }
    } else if (rec.type == DnsType::CNAME || rec.type == DnsType::NS) {
      std::size_t tmp = offset;
      auto n = dns_read_name(data, len, tmp);
      if (n) rec.data = *n;
    } else {
      rec.data = to_hex(data + offset, rdlen);
    }
    offset += rdlen;
    records.push_back(std::move(rec));
  }
  return records;
}

Result<std::vector<DnsRecord>> dns_query(
    const std::string& name, DnsType type,
    const std::string& resolver, std::uint16_t port, int timeout_ms) {
#if LR_PLATFORM_WINDOWS
  auto ws = ensure_winsock();
  if (!ws) return Result<std::vector<DnsRecord>>({}, ws.error_msg);
#endif

  auto packet = dns_build_query(name, type, 0);
  if (!packet) return Result<std::vector<DnsRecord>>({}, packet.error_msg);

  // UDP socket
  const int fd = static_cast<int>(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
  if (fd < 0) return os_error("dns socket");
  Socket sock{fd};
  set_timeout(sock, timeout_ms);

  auto sent = sendto(sock, packet->data(), static_cast<int>(packet->size()),
                     resolver, port);
  if (!sent) {
    close(sock);
    return Result<std::vector<DnsRecord>>({}, sent.error_msg);
  }

  std::uint8_t buf[4096];
  auto got = recvfrom(sock, buf, sizeof(buf));
  close(sock);
  if (!got) return Result<std::vector<DnsRecord>>({}, got.error_msg);
  return dns_parse_response(buf, static_cast<std::size_t>(*got));
}

Result<std::vector<DnsRecord>> dns_query_doh(
    const std::string& name, DnsType type,
    const std::string& doh_url, int timeout_ms) {
  auto packet = dns_build_query(name, type, 0);
  if (!packet) return Result<std::vector<DnsRecord>>({}, packet.error_msg);

  // RFC 8484: POST application/dns-message
  HttpRequestOptions opts;
  opts.timeout_ms = timeout_ms;
  opts.user_agent = "EducationalLab-DoH/1.0";
  opts.headers["Accept"] = "application/dns-message";

  auto resp = https_post_ex(doh_url, *packet, opts, "application/dns-message");
  if (!resp) return Result<std::vector<DnsRecord>>({}, resp.error_msg);
  if (resp->status_code != 200)
    return Result<std::vector<DnsRecord>>(
        {}, "DoH status " + std::to_string(resp->status_code));
  return dns_parse_response(resp->body.data(), resp->body.size());
}

}  // namespace real::net
