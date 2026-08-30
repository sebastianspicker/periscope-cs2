// error.hpp — Real-backend error reporting.
// Stack-only fixed-size error messages — no CRT heap dependency in the
// hot path. std::string is accepted at construction for convenience and
// truncated into the fixed buffer.

#pragma once

#include <cstddef>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>

namespace real {

// Fixed-size error message buffer — no heap allocation, no exception risk.
struct FixedError {
    char buf[128]{};
    bool is_empty = true;

    FixedError() = default;
    explicit FixedError(const char* msg) noexcept { copy_from(msg); }
    explicit FixedError(const std::string& msg) noexcept { copy_from(msg.c_str()); }

    FixedError(const FixedError& other) noexcept { copy_from(other.buf); is_empty = other.is_empty; }
    FixedError& operator=(const FixedError& other) noexcept {
        if (this != &other) {
            copy_from(other.buf);
            is_empty = other.is_empty;
        }
        return *this;
    }
    FixedError(FixedError&& other) noexcept {
        copy_from(other.buf);
        is_empty = other.is_empty;
        other.is_empty = true;
        other.buf[0] = 0;
    }
    FixedError& operator=(FixedError&& other) noexcept {
        if (this != &other) {
            copy_from(other.buf);
            is_empty = other.is_empty;
            other.is_empty = true;
            other.buf[0] = 0;
        }
        return *this;
    }

    const char* c_str() const noexcept { return buf; }
    bool empty() const noexcept { return is_empty; }

    // MSVC-friendly comparisons / string concatenation with legacy call sites.
    bool operator==(const char* s) const noexcept {
        if (!s) return is_empty;
        return std::strcmp(buf, s) == 0;
    }
    bool operator!=(const char* s) const noexcept { return !(*this == s); }
    bool operator==(const std::string& s) const noexcept { return s == buf; }
    bool operator!=(const std::string& s) const noexcept { return s != buf; }
    friend std::string operator+(const std::string& a, const FixedError& b) {
        return a + b.c_str();
    }
    friend std::string operator+(const char* a, const FixedError& b) {
        return std::string(a ? a : "") + b.c_str();
    }
    friend std::string operator+(const FixedError& a, const std::string& b) {
        return std::string(a.c_str()) + b;
    }
    friend std::string operator+(const FixedError& a, const char* b) {
        return std::string(a.c_str()) + (b ? b : "");
    }

private:
    void copy_from(const char* src) noexcept {
        if (!src || !*src) { is_empty = true; buf[0] = 0; return; }
        std::size_t len = 0;
        while (len < sizeof(buf) - 1 && src[len]) { ++len; }
        std::memcpy(buf, src, len);
        buf[len] = 0;
        is_empty = false;
    }
};

// Forward declaration so Result<T> can convert from failed Result<void>.
template <typename T>
struct Result;

/// Specialization for void results.
template <>
struct Result<void> {
  FixedError error_msg;
  bool ok = true;

  Result() = default;
  explicit Result(const char* msg) : error_msg(msg), ok(false) {}
  explicit Result(const FixedError& err) : error_msg(err), ok(false) {}
  explicit Result(FixedError&& err) : error_msg(std::move(err)), ok(false) {}
  explicit Result(const std::string& msg) : error_msg(msg), ok(false) {}

  operator bool() const { return ok; }
};

/// Simple result type for real-backend operations.
template <typename T>
struct Result {
  T value{};
  FixedError error_msg;
  bool ok = true;

  Result() = default;
  Result(T v) : value(std::move(v)), ok(true) {}
  Result(T v, const char* msg)
      : value(std::move(v)), error_msg(msg), ok(false) {}
  Result(T v, const FixedError& msg)
      : value(std::move(v)), error_msg(msg), ok(false) {}
  Result(T v, FixedError&& msg)
      : value(std::move(v)), error_msg(std::move(msg)), ok(false) {}
  Result(T v, const std::string& msg)
      : value(std::move(v)), error_msg(msg), ok(false) {}

  // Allow `return os_error("...")` from functions that return Result<T>.
  Result(Result<void> err)
      : value{}, error_msg(std::move(err.error_msg)), ok(err.ok) {}

  operator bool() const { return ok; }
  const T& operator*() const { return value; }
  T& operator*() { return value; }
  const T* operator->() const { return &value; }
  T* operator->() { return &value; }
};

/// Wrap a platform GetLastError() / errno into a Result.
Result<void> os_error(const char* context);

/// Format a platform error code into a human-readable string.
std::string format_os_error(unsigned long code);

}  // namespace real
